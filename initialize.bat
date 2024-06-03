@echo off
cd %~dp0
if not exist build/ (mkdir build)
cd build
if not exist allegro5/ (mkdir allegro5)
cd allegro5
if not exist deps/ (mkdir deps)
cd deps
echo Checking for nuget package 'AllegroDeps':
nuget install AllegroDeps -Version 1.14.0 -OutputDirectory . -ExcludeVersion
xcopy "./AllegroDeps/build/native/include" "./include" /E /I /Q /Y
xcopy "./AllegroDeps/build/native/v143/x64/deps/lib" "./lib" /E /I /Q /Y
cd %~dp0/build
echo Running CMake:
cmake .. -DWIN32=1
