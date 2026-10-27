flatc.exe --cpp --strict-json --unknown-json --gen-object-api --force-empty --force-empty-vectors --cpp-std c++17 --scoped-enums -o ..\flatbuffers ten_common.fbs
flatc.exe --cpp --strict-json --unknown-json --gen-object-api --force-empty --force-empty-vectors --cpp-std c++17 --scoped-enums -o ..\flatbuffers ten_itemdata.fbs
flatc.exe --cpp --strict-json --unknown-json --gen-object-api --force-empty --force-empty-vectors --cpp-std c++17 --scoped-enums -o ..\flatbuffers ten_savegame.fbs
flatc.exe --cpp --strict-json --unknown-json --gen-object-api --force-empty --force-empty-vectors --cpp-std c++17 --scoped-enums -o ..\flatbuffers ten_configuration.fbs
echo %errorlevel%