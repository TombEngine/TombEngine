# PowerShell script to generate Settings classes from C++ source.
# This script reads Settings.cpp directly to extract documentation and generates clean Settings classes from scratch,
# discarding the LDoc-generated Flow.Settings.

param(
    [string]$InputXmlFile = "API.xml",
    [string]$OutputXmlFile = "API.xml",
    [string]$SettingsCppFile = "..\TombEngine\Scripting\Internal\TEN\Flow\Settings\Settings.cpp"
)

# Constants
$FLOW_SETTINGS_CLASS = "Flow.Settings"
$TEN_CLASS_TYPE = "tenclass"

Write-Host "Starting Settings class generation from C++ source..."
Write-Host "Input XML: $InputXmlFile"
Write-Host "Output XML: $OutputXmlFile"
Write-Host "Settings C++ file: $SettingsCppFile"

# Helper function to create XML elements with text content
function New-XmlElementWithText {
    param(
        [xml]$XmlDoc,
        [string]$ElementName,
        [string]$TextContent
    )

    $element = $XmlDoc.CreateElement($ElementName)
    $element.InnerText = $TextContent
    return $element
}

# Helper function to extract section description from comment lines
function Get-SectionDescription {
    param(
        [string[]]$Lines,
        [int]$StartIndex
    )

    $description = @()

    for ($i = $StartIndex + 1; $i -lt $Lines.Length; $i++) {
        $line = $Lines[$i].Trim()

        if ($line.StartsWith('//') -and -not $line.StartsWith('// @')) {
            $description += $line.TrimStart('/').Trim()
        } else {
            break
        }
    }

    return $description -join " "
}

# Helper function to extract field documentation from preceding comment lines
function Get-FieldDocumentation {
    param(
        [string[]]$Lines,
        [int]$FieldIndex
    )

    $summary = ""
    $description = ""
    $fieldType = ""
    $commentLines = @()

    for ($i = $FieldIndex - 1; $i -ge 0; $i--) {
        $line = $Lines[$i].Trim()

        if ($line.StartsWith('///') -or $line.StartsWith('//')) {
            if ($line -match '//\s*@tfield\s+(\w+)\s+\w+\s+(.+)$') {
                # Extract type and description from @tfield line
                $fieldType = $matches[1]
                $description = $matches[2]
            } elseif ($line.StartsWith('///')) {
                # Three slashes indicate summary text
                $summary = $line.TrimStart('/').Trim()
            } elseif ($line.StartsWith('//') -and -not $line.StartsWith('// @')) {
                # Two slashes (non-@) indicate description
                $descText = $line.TrimStart('/').Trim()

                if ($descText) {
                    $commentLines = @($descText) + $commentLines
                }
            }
        } else {
            break
        }
    }

    # Build description from collected comment lines
    if ($commentLines.Count -gt 0) {
        $description = $commentLines -join " "
    }

    # Add type annotation to summary if we have both type and summary
    if ($fieldType -and $summary) {
        $summary = "($fieldType) $summary"
    } elseif ($fieldType -and $description) {
        # If no summary but we have description, add type to description
        $description = "($fieldType) $description"
    }

    return @{
        Summary = $summary
        Description = $description
    }
}

# Helper function to extract field type override from comments
function Get-FieldTypeOverride {
    param(
        [string[]]$Lines,
        [int]$FieldIndex
    )

    # Look backwards for /* @fieldtype ... */ comment
    for ($i = $FieldIndex - 1; $i -ge 0; $i--) {
        $line = $Lines[$i].Trim()

        # Check for /* @fieldtype ... */ pattern
        if ($line -match '/\*\s*@fieldtype\s+([^*]+)\s*\*/') {
            $fieldType = $matches[1].Trim()
            return $fieldType
        }

        # Stop if we encounter a non-comment line or different comment type
        if (-not ($line.StartsWith('///') -or $line.StartsWith('//') -or $line.StartsWith('/*') -or $line -eq "")) {
            break
        }
    }

    return $null
}

# Helper function to extract main Flow.Settings class documentation
function Get-MainClassDocumentation {
    param([string[]]$Lines)

    $summary = ""
    $description = ""
    $commentLines = @()

    for ($i = 0; $i -lt $Lines.Length; $i++) {
        $line = $Lines[$i].Trim()

        # Look for the @tenclass Flow.Settings marker
        if ($line -match '//\s*@tenclass\s+Flow\.Settings') {
            # Found the marker, now collect preceding documentation
            for ($j = $i - 1; $j -ge 0; $j--) {
                $prevLine = $Lines[$j].Trim()

                if ($prevLine.StartsWith('///') -or ($prevLine.StartsWith('//') -and -not $prevLine.StartsWith('// @'))) {
                    $commentText = $prevLine.TrimStart('/').Trim()

                    if ($commentText) {
                        $commentLines = @($commentText) + $commentLines
                    }
                } else {
                    break
                }
            }

            # Build summary and description from collected comments
            if ($commentLines.Count -gt 0) {
                $summary = $commentLines[0]

                if ($commentLines.Count -gt 1) {
                    $description = ($commentLines | Select-Object -Skip 1) -join " "
                }
            }

            break
        }
    }

    return @{
        Summary = $summary
        Description = $description
    }
}

# Main function to parse Settings.cpp and extract class definitions
function Parse-SettingsFromCpp {
    param([string]$CppFilePath)

    Write-Host "Parsing Settings.cpp for complete class definitions..."

    $content = Get-Content $CppFilePath -Raw -Encoding UTF8
    $classes = @{}
    $sectionFieldTypes = @{}  # Store field type overrides per section
    $lines = $content -split '\r?\n'

    # Extract main class documentation
    $mainClassDoc = Get-MainClassDocumentation -Lines $lines
    Write-Host "Found main class documentation: $($mainClassDoc.Summary)"

    $currentSection = $null
    $currentSectionDescription = ""
    $currentClassName = $null

    $inRegisterMethod = $false
    $fieldBuffer = @()

    for ($i = 0; $i -lt $lines.Length; $i++) {
        $line = $lines[$i].Trim()

        # Process @section declarations
        if ($line -match '//\s*@section\s+(\w+)') {
            $currentSection = $matches[1]
            $currentSectionDescription = Get-SectionDescription -Lines $lines -StartIndex $i
            Write-Host "`tFound section: $currentSection - $currentSectionDescription"

            # Look for field type override for this section
            $fieldTypeOverride = Get-FieldTypeOverride -Lines $lines -FieldIndex $i

            if ($fieldTypeOverride) {
                $sectionFieldTypes[$currentSection] = $fieldTypeOverride
                Write-Host "`t`tFound field type override: $fieldTypeOverride"
            }

            continue
        }

        # Process class Register method start
        if ($line -match 'void\s+(\w+)::Register\(') {
            $currentClassName = $matches[1]
            $inRegisterMethod = $true
            $fieldBuffer = @()

            if ($currentSection) {
                $fullClassName = "Flow.$currentClassName"

                $classes[$fullClassName] = @{
                    Name = $fullClassName
                    Section = $currentSection
                    Description = if ($currentSectionDescription) { $currentSectionDescription } else { "$currentSection settings." }
                    Fields = @()
                }

                Write-Host "`t`tProcessing class: $currentClassName"
            }

            continue
        }

        # Process Register method end
        if ($inRegisterMethod -and $line -eq "}") {
            if ($currentClassName -and $classes.ContainsKey("Flow.$currentClassName")) {
                $classes["Flow.$currentClassName"].Fields = $fieldBuffer
                Write-Host "`t`t`tAdded $($fieldBuffer.Count) fields to $currentClassName"
            }

            # Reset state
            $currentSection = $null
            $currentSectionDescription = ""
            $currentClassName = $null
            $inRegisterMethod = $false
            $fieldBuffer = @()
            continue
        }

        # Process field definitions within Register method
        if ($inRegisterMethod -and $currentClassName -and $line -match '"([^"]+)",\s*&') {
            $fieldName = $matches[1]
            $fieldDoc = Get-FieldDocumentation -Lines $lines -FieldIndex $i

            $fieldInfo = @{
                Name = $fieldName
                Summary = $fieldDoc.Summary
                Description = $fieldDoc.Description
            }

            $fieldBuffer += $fieldInfo
            Write-Host "`t`t`t`tField: $fieldName - $($fieldDoc.Summary)"
        }
    }

    $totalFields = ($classes.Values | ForEach-Object { $_.Fields.Count } | Measure-Object -Sum).Sum
    Write-Host "Parsed $totalFields fields across $($classes.Count) classes"

    return @{
        Classes = $classes
        MainClassDoc = $mainClassDoc
        SectionFieldTypes = $sectionFieldTypes
    }
}

# Helper function to create XML field element
function New-XmlFieldElement {
    param(
        [xml]$XmlDoc,
        [hashtable]$FieldInfo
    )

    $fieldElement = $XmlDoc.CreateElement("field")
    $fieldElement.AppendChild((New-XmlElementWithText $XmlDoc "name" $FieldInfo.Name)) | Out-Null

    # Only add summary if it's not empty
    if ($FieldInfo.Summary -and $FieldInfo.Summary.Trim() -ne "") {
        $fieldElement.AppendChild((New-XmlElementWithText $XmlDoc "summary" $FieldInfo.Summary)) | Out-Null
    }

    # Only add description if it's not empty
    if ($FieldInfo.Description -and $FieldInfo.Description.Trim() -ne "") {
        $fieldElement.AppendChild((New-XmlElementWithText $XmlDoc "description" $FieldInfo.Description)) | Out-Null
    }

    return $fieldElement
}

# Helper function to create XML class element
function New-XmlClassElement {
    param(
        [xml]$XmlDoc,
        [hashtable]$ClassInfo
    )

    $classElement = $XmlDoc.CreateElement("class")
    $classElement.AppendChild((New-XmlElementWithText $XmlDoc "name" $ClassInfo.Name)) | Out-Null
    $classElement.AppendChild((New-XmlElementWithText $XmlDoc "type" $TEN_CLASS_TYPE)) | Out-Null
    $classElement.AppendChild((New-XmlElementWithText $XmlDoc "ctor" "false")) | Out-Null

    # Only add summary if it's not empty
    if ($ClassInfo.Summary -and $ClassInfo.Summary.Trim() -ne "") {
        $classElement.AppendChild((New-XmlElementWithText $XmlDoc "summary" $ClassInfo.Summary)) | Out-Null
    }

    # Only add description if it's not empty
    if ($ClassInfo.Description -and $ClassInfo.Description.Trim() -ne "") {
        $classElement.AppendChild((New-XmlElementWithText $XmlDoc "description" $ClassInfo.Description)) | Out-Null
    }

    # Add members container
    $membersElement = $XmlDoc.CreateElement("members")

    foreach ($fieldInfo in $ClassInfo.Fields) {
        $membersElement.AppendChild((New-XmlFieldElement $XmlDoc $fieldInfo)) | Out-Null
    }

    $classElement.AppendChild($membersElement) | Out-Null
    return $classElement
}

# Helper function to remove existing Flow.Settings class from XML
function Remove-ExistingFlowSettingsClass {
    param([xml]$XmlDoc)

    $allClasses = $XmlDoc.GetElementsByTagName("class")
    Write-Host "Found $($allClasses.Count) classes total in XML"

    foreach ($class in $allClasses) {
        if ($class.name -eq $FLOW_SETTINGS_CLASS) {
            Write-Host "Removing existing $FLOW_SETTINGS_CLASS class from XML"
            $class.ParentNode.RemoveChild($class) | Out-Null
            return
        }
    }

    Write-Host "No existing $FLOW_SETTINGS_CLASS class found in XML"
}

# Helper function to save XML with proper formatting
function Save-XmlWithFormatting {
    param(
        [xml]$XmlDoc,
        [string]$OutputPath
    )

    try {
        $xmlWriterSettings = New-Object System.Xml.XmlWriterSettings
        $xmlWriterSettings.Indent = $true
        $xmlWriterSettings.IndentChars = "`t"  # Use tab character for indentation
        $xmlWriterSettings.NewLineChars = "`r`n"
        $xmlWriterSettings.Encoding = [System.Text.Encoding]::UTF8

        $xmlWriter = [System.Xml.XmlWriter]::Create($OutputPath, $xmlWriterSettings)
        $XmlDoc.Save($xmlWriter)
        $xmlWriter.Close()

        Write-Host "File saved successfully to: $OutputPath (with tab indentation)"
    } catch {
        Write-Host "ERROR saving XML file: $($_.Exception.Message)"
        exit 1
    }
}

# Main processing logic
try {
    # Parse the Settings.cpp file
    $parseResult = Parse-SettingsFromCpp -CppFilePath $SettingsCppFile
    $settingsClasses = $parseResult.Classes
    $mainClassDoc = $parseResult.MainClassDoc
    $sectionFieldTypes = $parseResult.SectionFieldTypes

    if ($settingsClasses.Count -eq 0) {
        Write-Host "ERROR: No settings classes found in Settings.cpp."
        exit 1
    }

    $mainDescription = if ($mainClassDoc.Summary) {
        if ($mainClassDoc.Description) {
            "$($mainClassDoc.Summary) $($mainClassDoc.Description)"
        } else {
            $mainClassDoc.Summary
        }
    } else {
        $($mainClassDoc.Description)
    }

    Write-Host "Using main class description: $mainDescription"

    # Load and process XML
    [xml]$xmlDoc = Get-Content $InputXmlFile -Encoding UTF8
    Remove-ExistingFlowSettingsClass -XmlDoc $xmlDoc

    # Find the classes container
    $classesContainer = $xmlDoc.SelectSingleNode("//classes")

    if (-not $classesContainer) {
        Write-Host "ERROR: Could not find <classes> container in XML."
        exit 1
    }

    Write-Host "Found classes container with $($classesContainer.ChildNodes.Count) child nodes"

    # Generate new settings classes from C++ source
    foreach ($className in $settingsClasses.Keys) {
        $classInfo = $settingsClasses[$className]
        Write-Host "Creating class: $className with $($classInfo.Fields.Count) fields"

        $newClassElement = New-XmlClassElement -XmlDoc $xmlDoc -ClassInfo $classInfo
        $classesContainer.AppendChild($newClassElement) | Out-Null
        Write-Host "`tAdded $className to classes container successfully"
    }

    # Generate the main Flow.Settings class with sub-class references
    Write-Host "Creating main $FLOW_SETTINGS_CLASS class with sub-class references"

    $mainClass = @{
        Name = $FLOW_SETTINGS_CLASS
        Description = $mainDescription
        Fields = @()
    }

    # Add sub-class references as fields
    foreach ($className in $settingsClasses.Keys | Sort-Object) {
        $classInfo = $settingsClasses[$className]

        # Check for field type override for this section
        $fieldType = if ($sectionFieldTypes.ContainsKey($classInfo.Section)) {
            $sectionFieldTypes[$classInfo.Section]
        } else {
            $className
        }

        $mainClass.Fields += @{
            Name = $classInfo.Section
            Type = $fieldType
            Summary = $classInfo.Description
            Description = $classInfo.Description
        }

        if ($fieldType -ne $className) {
            Write-Host "`tAdded field: $($classInfo.Section) -> $fieldType (overridden from $className)"
        } else {
            Write-Host "`tAdded field: $($classInfo.Section) -> $className"
        }
    }

    # Create main class element with custom field handling for type references
    $mainClassElement = $xmlDoc.CreateElement("class")
    $mainClassElement.AppendChild((New-XmlElementWithText $xmlDoc "name" $mainClass.Name)) | Out-Null
    $mainClassElement.AppendChild((New-XmlElementWithText $xmlDoc "type" $TEN_CLASS_TYPE)) | Out-Null

    # Only add summary if it's not empty
    if ($mainClass.Summary -and $mainClass.Summary.Trim() -ne "") {
        $mainClassElement.AppendChild((New-XmlElementWithText $xmlDoc "summary" $mainClass.Summary)) | Out-Null
    }

    # Only add description if it's not empty
    if ($mainClass.Description -and $mainClass.Description.Trim() -ne "") {
        $mainClassElement.AppendChild((New-XmlElementWithText $xmlDoc "description" $mainClass.Description)) | Out-Null
    }

    # Add members with type references
    $mainMembersElement = $xmlDoc.CreateElement("members")

    foreach ($fieldInfo in $mainClass.Fields) {
        $fieldElement = $xmlDoc.CreateElement("field")
        $fieldElement.AppendChild((New-XmlElementWithText $xmlDoc "name" $fieldInfo.Name)) | Out-Null
        $fieldElement.AppendChild((New-XmlElementWithText $xmlDoc "type" $fieldInfo.Type)) | Out-Null

        # Only add summary if it's not empty
        if ($fieldInfo.Summary -and $fieldInfo.Summary.Trim() -ne "") {
            $fieldElement.AppendChild((New-XmlElementWithText $xmlDoc "summary" $fieldInfo.Summary)) | Out-Null
        }

        # Only add description if it's not empty
        if ($fieldInfo.Description -and $fieldInfo.Description.Trim() -ne "") {
            $fieldElement.AppendChild((New-XmlElementWithText $xmlDoc "description" $fieldInfo.Description)) | Out-Null
        }

        $mainMembersElement.AppendChild($fieldElement) | Out-Null
    }

    $mainClassElement.AppendChild($mainMembersElement) | Out-Null

    $classesContainer.AppendChild($mainClassElement) | Out-Null
    Write-Host "`tAdded $FLOW_SETTINGS_CLASS class with $($settingsClasses.Keys.Count) sub-class references"

    # Save the processed XML
    Save-XmlWithFormatting -XmlDoc $xmlDoc -OutputPath $OutputXmlFile

    # Display results
    Write-Host ""
    Write-Host "Settings class generation completed successfully!"
    Write-Host "Generated main $FLOW_SETTINGS_CLASS class plus $($settingsClasses.Keys.Count) settings sub-classes:"
    Write-Host "`t$FLOW_SETTINGS_CLASS ($($settingsClasses.Keys.Count) sub-class references)"

    foreach ($className in $settingsClasses.Keys) {
        $fieldCount = $settingsClasses[$className].Fields.Count
        Write-Host "`t$className ($fieldCount fields)"
    }

    Write-Host ""
    Write-Host "Output saved to: $OutputXmlFile"

} catch {
    Write-Host "ERROR: $($_.Exception.Message)"
    exit 1
}
