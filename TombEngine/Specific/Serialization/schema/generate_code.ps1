# Paths to the .fbs and corresponding generated .h files.
$fbsFiles = @(
    @{ fbs = "$PSScriptRoot/ten_common.fbs"; h = "$PSScriptRoot/../flatbuffers/ten_common_generated.h"; out = "$PSScriptRoot/../flatbuffers" },
    @{ fbs = "$PSScriptRoot/ten_itemdata.fbs"; h = "$PSScriptRoot/../flatbuffers/ten_itemdata_generated.h"; out = "$PSScriptRoot/../flatbuffers" },
    @{ fbs = "$PSScriptRoot/ten_savegame.fbs"; h = "$PSScriptRoot/../flatbuffers/ten_savegame_generated.h"; out = "$PSScriptRoot/../flatbuffers" },
    @{ fbs = "$PSScriptRoot/ten_configuration.fbs"; h = "$PSScriptRoot/../flatbuffers/ten_configuration_generated.h"; out = "$PSScriptRoot/../flatbuffers" })

# Function to check if the .fbs file is newer than the .h file.
function IsFbsNewerThanH ($fbs, $h) 
{
    if (Test-Path $h) 
	{
        $fbsLastWrite = (Get-Item $fbs).LastWriteTime
        $hLastWrite = (Get-Item $h).LastWriteTime
        return $fbsLastWrite -gt $hLastWrite
    }
    else 
	{
        # If the .h file does not exist, treat the .fbs as newer.
        return $true
    }
}

# Check if any .fbs file is newer than its corresponding .h file.
$shouldGenerate = $false

foreach ($file in $fbsFiles) 
{
    if (IsFbsNewerThanH $file.fbs $file.h) 
	{
        $shouldGenerate = $true
        break
    }
}

# If any .fbs file is newer, run the generation process.
if ($shouldGenerate) 
{
    Write-Host "Generating FlatBuffers code from schema..."
    
    foreach ($file in $fbsFiles)
    {
        & "$PSScriptRoot\flatc.exe" --cpp --strict-json --unknown-json --gen-object-api --force-empty --force-empty-vectors --cpp-std c++17 --scoped-enums -o $file.out $file.fbs

        if ($LASTEXITCODE -ne 0)
        {
            Write-Host "Error occurred during FlatBuffers code generation."
            exit $LASTEXITCODE
        }
    }
    
    # Check for errors and output result.
    if ($LASTEXITCODE -eq 0) 
	{
		Write-Host "FlatBuffers code generation completed successfully."
    }
    else 
	{
		Write-Host "Error occurred during FlatBuffers code generation."
        exit $LASTEXITCODE
    }
}
else 
{
	Write-Host "FlatBuffers schema files are unchanged. Skipping code generation."
}