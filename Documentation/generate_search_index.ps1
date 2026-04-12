param(
    [string]$DocRoot = ".\doc",
    [string]$OutputFile
)

Add-Type -AssemblyName System.Web

function Normalize-Whitespace {
    param(
        [string]$Text
    )

    if ($null -eq $Text) {
        $Text = ""
    }

    return ([regex]::Replace($Text, "\s+", " ")).Trim()
}

function Get-RegexGroupValue {
    param(
        [string]$InputText,
        [string]$Pattern
    )

    $match = [regex]::Match(
        $InputText,
        $Pattern,
        [System.Text.RegularExpressions.RegexOptions]::IgnoreCase -bor [System.Text.RegularExpressions.RegexOptions]::Singleline)

    if (-not $match.Success) {
        return ""
    }

    $decodedValue = [System.Web.HttpUtility]::HtmlDecode($match.Groups[1].Value)
    $withoutTags = [regex]::Replace($decodedValue, "<[^>]+>", " ")
    return Normalize-Whitespace $withoutTags
}

function Convert-HtmlToText {
    param(
        [string]$Html
    )

    $text = $Html
    $text = [regex]::Replace($text, "(?is)<!--.*?-->", " ")
    $text = [regex]::Replace($text, "(?is)<script\b.*?</script>", " ")
    $text = [regex]::Replace($text, "(?is)<style\b.*?</style>", " ")
    $text = [regex]::Replace($text, "(?i)<br\s*/?>", " ")
    $text = [regex]::Replace($text, "(?i)</(p|div|h1|h2|h3|h4|h5|h6|li|tr|table|ul|ol|pre|blockquote)>", " ")
    $text = [regex]::Replace($text, "<[^>]+>", " ")
    $text = [System.Web.HttpUtility]::HtmlDecode($text)
    return Normalize-Whitespace $text
}

function Get-DocumentContentHtml {
    param(
        [string]$Html
    )

    $match = [regex]::Match(
        $Html,
        '<div id="content">(.*?)</div>\s*<!-- id="content" -->',
        [System.Text.RegularExpressions.RegexOptions]::IgnoreCase -bor [System.Text.RegularExpressions.RegexOptions]::Singleline)

    if ($match.Success) {
        return $match.Groups[1].Value
    }

    return $Html
}

function Get-NearestSectionName {
    param(
        [System.Collections.ArrayList]$Sections,
        [int]$Position
    )

    $closestSection = ""

    foreach ($section in $Sections) {
        if ($section.Position -gt $Position) {
            break
        }

        $closestSection = $section.Name
    }

    return $closestSection
}

$resolvedDocRoot = (Resolve-Path $DocRoot).Path

if (-not $OutputFile) {
    $OutputFile = Join-Path $resolvedDocRoot "search-index.js"
}

$regexOptions = [System.Text.RegularExpressions.RegexOptions]::IgnoreCase -bor [System.Text.RegularExpressions.RegexOptions]::Singleline

$entries = Get-ChildItem -Path $resolvedDocRoot -Filter *.html -Recurse |
    Sort-Object FullName |
    ForEach-Object {
        $rawHtml = Get-Content -LiteralPath $_.FullName -Raw
        $contentHtml = Get-DocumentContentHtml -Html $rawHtml
        $relativePath = $_.FullName.Substring($resolvedDocRoot.Length).TrimStart('\') -replace '\\', '/'
        $section = Split-Path $relativePath -Parent

        if ([string]::IsNullOrWhiteSpace($section)) {
            $section = "Overview"
        }

        $pageTitle = Get-RegexGroupValue -InputText $rawHtml -Pattern "<title[^>]*>(.*?)</title>"
        $heading = Get-RegexGroupValue -InputText $contentHtml -Pattern "<h1[^>]*>(.*?)</h1>"
        if ([string]::IsNullOrWhiteSpace($heading)) {
            $heading = Get-RegexGroupValue -InputText $contentHtml -Pattern "<h2[^>]*>(.*?)</h2>"
        }

        $documentText = Convert-HtmlToText -Html $contentHtml

        if ([string]::IsNullOrWhiteSpace($heading)) {
            $heading = [System.IO.Path]::GetFileNameWithoutExtension($_.Name)
        }

        $pageEntries = New-Object System.Collections.ArrayList
        $null = $pageEntries.Add([ordered]@{
            kind = "page"
            path = $relativePath
            section = $section
            title = $heading
            pageTitle = $pageTitle
            text = $documentText
        })

        $sectionMatches = [regex]::Matches(
            $contentHtml,
            '<h2[^>]*class="section-header[^"]*"[^>]*>\s*<a name="([^"]+)"></a>(.*?)</h2>',
            $regexOptions)

        $sections = New-Object System.Collections.ArrayList
        foreach ($sectionMatch in $sectionMatches) {
            $sectionName = Convert-HtmlToText -Html $sectionMatch.Groups[2].Value
            if (-not [string]::IsNullOrWhiteSpace($sectionName)) {
                $null = $sections.Add([ordered]@{
                    Position = $sectionMatch.Index
                    Name = $sectionName
                })
            }
        }

        $summaryMatches = [regex]::Matches(
            $contentHtml,
            '<h2><a href="#([^"]+)">(.*?)</a></h2>\s*<table class="function_list">(.*?)</table>',
            $regexOptions)

        $summaryByKey = @{}
        foreach ($summaryMatch in $summaryMatches) {
            $summarySection = Convert-HtmlToText -Html $summaryMatch.Groups[2].Value
            $rowMatches = [regex]::Matches(
                $summaryMatch.Groups[3].Value,
                '<tr>\s*<td class="name"[^>]*><a href="#([^"]+)">(.*?)</a></td>\s*<td class="summary">(.*?)</td>\s*</tr>',
                $regexOptions)

            foreach ($rowMatch in $rowMatches) {
                $rowAnchor = $rowMatch.Groups[1].Value
                $rowSummary = Convert-HtmlToText -Html $rowMatch.Groups[3].Value
                $summaryByKey["$summarySection|$rowAnchor"] = $rowSummary
            }
        }

        $detailMatches = [regex]::Matches(
            $contentHtml,
            '<dt>\s*<a name\s*=\s*"([^"]+)"></a>\s*<strong>(.*?)</strong>.*?</dt>\s*<dd>(.*?)</dd>',
            $regexOptions)

        $anchorCounts = @{}
        foreach ($detailMatch in $detailMatches) {
            $anchorName = $detailMatch.Groups[1].Value
            if ($anchorCounts.ContainsKey($anchorName)) {
                $anchorCounts[$anchorName]++
            } else {
                $anchorCounts[$anchorName] = 1
            }
        }

        foreach ($detailMatch in $detailMatches) {
            $anchorName = $detailMatch.Groups[1].Value
            if ($anchorCounts[$anchorName] -ne 1) {
                continue
            }

            $detailTitle = Convert-HtmlToText -Html $detailMatch.Groups[2].Value
            $detailBody = Convert-HtmlToText -Html $detailMatch.Groups[3].Value
            $detailSection = Get-NearestSectionName -Sections $sections -Position $detailMatch.Index
            $detailSummary = ""

            if (-not [string]::IsNullOrWhiteSpace($detailSection)) {
                $summaryKey = "$detailSection|$anchorName"
                if ($summaryByKey.ContainsKey($summaryKey)) {
                    $detailSummary = $summaryByKey[$summaryKey]
                }
            }

            $detailText = Normalize-Whitespace "$detailSummary $detailBody"
            if ([string]::IsNullOrWhiteSpace($detailText)) {
                $detailText = $detailSummary
            }

            if ([string]::IsNullOrWhiteSpace($detailTitle)) {
                continue
            }

            $detailSectionLabel = $section
            if (-not [string]::IsNullOrWhiteSpace($heading)) {
                $detailSectionLabel += " / $heading"
            }

            if (-not [string]::IsNullOrWhiteSpace($detailSection)) {
                $detailSectionLabel += " / $detailSection"
            }

            $null = $pageEntries.Add([ordered]@{
                kind = "anchor"
                path = "$relativePath#$anchorName"
                section = $detailSectionLabel
                title = $detailTitle
                pageTitle = $pageTitle
                text = $detailText
            })
        }

        $pageEntries
    }

$json = $entries | ConvertTo-Json -Depth 4 -Compress
$output = "window.TENSearchIndex = $json;"
Set-Content -LiteralPath $OutputFile -Value $output -Encoding UTF8

Write-Host "Search index saved to: $OutputFile"