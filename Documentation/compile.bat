@echo off
setlocal
set DOC_DIR=.\doc
set LDOC_DIR=.\compiler\ldoc
set LUA_PATH=.\compiler\?.lua
set LUA_CPATH=.\compiler\?.dll

echo Building LDoc documentation...
rmdir /s /q %DOC_DIR%
mkdir %DOC_DIR%

echo Running LDoc compiler...
.\compiler\lua.exe %LDOC_DIR%\\ldoc.lua %*

if %ERRORLEVEL% neq 0 (
    echo LDoc compilation failed with error code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo Post-processing Settings XML structure...
powershell.exe -File "settings_class_post_process.ps1"

if %ERRORLEVEL% neq 0 (
    echo XML post-processing failed with error code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo Documentation build completed successfully!
exit /b 0
