@echo off
setlocal
for /f "usebackq delims=" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "ULW_VS=%%I"
if not defined ULW_VS (
    echo An existing MSVC x86 toolchain is required on the build machine.
    exit /b 1
)
call "%ULW_VS%\VC\Auxiliary\Build\vcvars32.bat" >nul
if errorlevel 1 exit /b 1
set "ULW_SOURCE=%~dp0"
set "ULW_BUILD=%ULW_SOURCE%..\bin\ulw"
set "ULW_FLAGS=/nologo /std:c++20 /permissive- /W4 /WX /EHsc /O2 /MT /utf-8"
if not exist "%ULW_BUILD%\work" mkdir "%ULW_BUILD%\work"
if not exist "%ULW_BUILD%\dist" mkdir "%ULW_BUILD%\dist"
pushd "%ULW_BUILD%\work"
if errorlevel 1 exit /b 1
cl %ULW_FLAGS% /LD "%ULW_SOURCE%IntelligenceCore.cpp" "%ULW_SOURCE%StreetSystems.cpp" "%ULW_SOURCE%PoliceInteractions.cpp" "%ULW_SOURCE%CityLife.cpp" "%ULW_SOURCE%MissionState.cpp" "%ULW_SOURCE%HoodlumGame.cpp" /Fe:UltimateLivingWorld.asi
if errorlevel 1 goto fail
cl %ULW_FLAGS% "%ULW_SOURCE%IntelligenceCore.cpp" "%ULW_SOURCE%StreetSystems.cpp" "%ULW_SOURCE%PoliceInteractions.cpp" "%ULW_SOURCE%CityLife.cpp" "%ULW_SOURCE%MissionState.cpp" "%ULW_SOURCE%IntelligenceCoreTests.cpp" /Fe:UlwTests.exe
if errorlevel 1 goto fail
UlwTests.exe "%CD%\UltimateLivingWorld.asi"
if errorlevel 1 goto fail
copy /y UltimateLivingWorld.asi "..\dist\UltimateLivingWorld.asi" >nul
if errorlevel 1 goto fail
for %%I in (UltimateLivingWorld.ini Verify-Game.ps1 README.md) do (
    copy /y "%ULW_SOURCE%%%I" "..\dist\%%I" >nul
    if errorlevel 1 goto fail
)
popd
exit /b 0

:fail
popd
exit /b 1
