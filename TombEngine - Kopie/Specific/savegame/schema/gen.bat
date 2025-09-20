flatc.exe --cpp --strict-json --unknown-json --gen-object-api --force-empty --force-empty-vectors --cpp-std c++17 --scoped-enums ten_itemdata.fbs
flatc.exe --cpp --strict-json --unknown-json --gen-object-api --force-empty --force-empty-vectors --cpp-std c++17 --scoped-enums ten_savegame.fbs
move /y ten_itemdata_generated.h ..\flatbuffers\ten_itemdata_generated.h
move /y ten_savegame_generated.h ..\flatbuffers\ten_savegame_generated.h
echo %errorlevel%