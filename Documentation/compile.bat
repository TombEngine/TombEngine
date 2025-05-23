@echo off
setlocal
set DOC_DIR=.\doc
set LDOC_DIR=.\compiler\ldoc
set LUA_PATH=.\compiler\?.lua
set LUA_CPATH=.\compiler\?.dll
rmdir /s /q %DOC_DIR%
mkdir %DOC_DIR%
.\compiler\lua.exe %LDOC_DIR%\\ldoc.lua %*
del output.xml
exit /b %ERRORLEVEL%
