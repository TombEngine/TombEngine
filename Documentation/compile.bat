@echo off
setlocal
set DOC_DIR=.\doc
set LDOC_DIR=.\compiler\ldoc
set LUA_PATH=.\compiler\?.lua
set LUA_CPATH=.\compiler\?.dll

REM Check if argument is provided, if not, set to "."
if "%~1"=="" (
    set ARG=.
) else (
    set ARG=%*
)

echo Building LDoc documentation...
rmdir /s /q %DOC_DIR%
mkdir %DOC_DIR%

echo Running LDoc compiler...
.\compiler\lua.exe %LDOC_DIR%\ldoc.lua %ARG%

if %ERRORLEVEL% neq 0 (
    echo LDoc compilation failed with error code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo Post-processing Settings XML structure...
powershell.exe -ExecutionPolicy Bypass -File "settings_class_post_process.ps1"

if %ERRORLEVEL% neq 0 (
    echo XML post-processing failed with error code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo Copying TEN logo asset...
copy /Y "..\TEN logo.png" "%DOC_DIR%\TEN logo.png" >nul

if %ERRORLEVEL% neq 0 (
    echo TEN logo copy failed with error code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo Copying documentation search script...
copy /Y ".\docs-search.js" "%DOC_DIR%\docs-search.js" >nul

if %ERRORLEVEL% neq 0 (
    echo Documentation search script copy failed with error code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo Generating documentation search index...
powershell.exe -ExecutionPolicy Bypass -File "generate_search_index.ps1" -DocRoot "%DOC_DIR%"

if %ERRORLEVEL% neq 0 (
    echo Documentation search index generation failed with error code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo Documentation build completed successfully!
exit /b 0
