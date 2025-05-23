# PowerShell script to generate ObjectIDs.h from game_object_ids.h

$inputFile = "../TombEngine/Objects/game_object_ids.h"  # Adjust path if necessary.
$tempOutputFile = "../TombEngine/Scripting/Internal/TEN/Objects/temp.h"
$outputFile = "../TombEngine/Scripting/Internal/TEN/Objects/ObjectIDs.h"

# Read the input file
if (-Not (Test-Path $inputFile)) 
{
    Write-Host "Error: File '$inputFile' not found."
    exit 1
}

# Get the last modified times for both files.
$inputFileLastModified = (Get-Item $inputFile).LastWriteTime
if (Test-Path $outputFile) {
    $outputFileLastModified = (Get-Item $outputFile).LastWriteTime
} else {
    $outputFileLastModified = [datetime]::MinValue  # If output file doesn't exist, treat it as very old.
}

# Exit early if the output file is newer than the input file.
if ($outputFileLastModified -ge $inputFileLastModified) {
    Write-Host "$outputFile is up to date. No changes made."
    exit 0
}

$content = Get-Content $inputFile

# Extract enum values and categorize them.
$enumValues = @()
$pickupConstants = @()
$spriteConstants = @()

$inPickupSection = $false
$inSpriteSection = $false

foreach ($line in $content) 
{
    if ($line -match "^\s*ID_([A-Za-z0-9_]+)") 
	{
        $enumName = $matches[1]
		$enumValues += $enumName
        
        if ($enumName -match "_ITEM|EXAMINE") 
		{
            $pickupConstants += $enumName
        } 
		elseif ($enumName -match "_SPRITE|_GRAPHIC|TEXTURE") 
		{
			# These two object IDs are deprecated 3D objects, not sprites.
			if ($enumName -in @("BINOCULAR_GRAPHICS", "TARGET_GRAPHICS"))
			{
				continue
			}
		
            $spriteConstants += $enumName
        }
		
    }
}

if ($enumValues.Count -gt 0) { $enumValues = $enumValues[1..($enumValues.Count - 2)] }

# Generate ObjectIDs.h content.
$header = @"
#pragma once

// This file is generated automatically, do not edit it.
// Last generated on $(Get-Date -Format "dd/MM/yyyy").

#include <unordered_map>
#include <string>
#include "Objects/game_object_ids.h"

/***
Constants for object IDs.
@enum Objects.ObjID
@pragma nostrip
*/

/*** Objects.ObjID constants.

The following constants are inside ObjID.

"@

$body = $enumValues | ForEach-Object { "`t$_" }
$footer = @"
@table Members
*/
"@

# Pickup Constants Section.
$pickupHeader = @"

/*** Objects.ObjID pickup constants.

The following ObjID members refer to pickups.

"@

$pickupBody = $pickupConstants | ForEach-Object { "`t$_" }
$pickupFooter = @"
@table PickupConstants
*/
"@

# Sprite Constants Section.
$spriteHeader = @"

/*** Objects.ObjID sprite constants.

The following ObjID members refer to sprites.

"@

$spriteBody = $spriteConstants | ForEach-Object { "`t$_" }
$spriteFooter = @"
@table SpriteConstants
*/
"@

# Map definition.
$mapHeader = "static const std::unordered_map<std::string, GAME_OBJECT_ID> GAME_OBJECT_IDS {"
$mapBody = ($enumValues | ForEach-Object { "`t" + '{ "' + "$_" + '", ID_' + "$_" + ' }' }) -join ",`r`n"
$mapFooter = "};"

# Write to output file
$header | Set-Content $tempOutputFile
$body | Add-Content $tempOutputFile
$footer | Add-Content $tempOutputFile
$pickupHeader | Add-Content $tempOutputFile
$pickupBody | Add-Content $tempOutputFile
$pickupFooter | Add-Content $tempOutputFile
$spriteHeader | Add-Content $tempOutputFile
$spriteBody | Add-Content $tempOutputFile
$spriteFooter | Add-Content $tempOutputFile
$mapHeader | Add-Content $tempOutputFile
$mapBody | ForEach-Object { Add-Content $tempOutputFile $_ }
$mapFooter | Add-Content $tempOutputFile

# Rename the temporary file to the final name.
if (Test-Path $outputFile) {
    Remove-Item $outputFile -Force
}
Move-Item -Path $tempOutputFile -Destination $outputFile -Force

Write-Host "Generated $outputFile successfully."