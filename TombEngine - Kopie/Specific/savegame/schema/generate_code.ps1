# Paths to the .fbs and corresponding generated .h files.
$fbsFiles = @(
    @{ fbs = "$PSScriptRoot/ten_itemdata.fbs"; h = "$PSScriptRoot/../flatbuffers/ten_itemdata_generated.h" },
    @{ fbs = "$PSScriptRoot/ten_savegame.fbs"; h = "$PSScriptRoot/../flatbuffers/ten_savegame_generated.h" })

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
    Write-Host "Generating savegame code from flatbuffer schema..."
    
    # Run flatc commands.
    & "$PSScriptRoot\flatc.exe" --cpp --strict-json --unknown-json --gen-object-api --force-empty --force-empty-vectors --cpp-std c++17 --scoped-enums $fbsFiles[0].fbs
    & "$PSScriptRoot\flatc.exe" --cpp --strict-json --unknown-json --gen-object-api --force-empty --force-empty-vectors --cpp-std c++17 --scoped-enums $fbsFiles[1].fbs
    
    # Move generated header files.
    Move-Item -Force "ten_itemdata_generated.h" $fbsFiles[0].h
    Move-Item -Force "ten_savegame_generated.h" $fbsFiles[1].h
    
    # Check for errors and output result.
    if ($LASTEXITCODE -eq 0) 
	{
        Write-Host "Savegame code generation completed successfully."
    }
    else 
	{
        Write-Host "Error occurred during savegame code generation."
        exit $LASTEXITCODE
    }
}
else 
{
    Write-Host "Savegame schema files are unchanged. Skipping code generation."
}