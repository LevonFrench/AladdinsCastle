@echo off
rem Builds the patched hotd2-vr Flycast fork for Windows PC VR with the MSVC compiler.
rem   build.cmd <source folder> <vcvars64.bat> <cmake.exe> <ninja.exe> [MSYS2 prefix to ignore]
rem The CMake and Ninja are called by full path and never put on PATH. With an MSYS2 CMake, pass
rem its prefix (its ucrt64 folder, with forward slashes) as the fifth argument:
rem   CMAKE_IGNORE_PREFIX_PATH      keeps MSYS2's own zlib, jsoncpp and headers out of an MSVC build
rem   PkgConfig disabled            for the same reason
rem CMAKE_NINJA_CMCLDEPS_RC=OFF is always set: MSYS2's CMake has no cmcldeps for resource builds,
rem and with Visual Studio's own CMake it only drops resource-file dependency scanning.
setlocal
if "%~4"=="" (
  echo usage: build.cmd ^<source folder^> ^<vcvars64.bat^> ^<cmake.exe^> ^<ninja.exe^> [MSYS2 prefix]
  exit /b 2
)
call "%~2" >nul || exit /b 1
cd /d "%~1" || exit /b 1
set "EXTRA="
if not "%~5"=="" set EXTRA="-DCMAKE_IGNORE_PREFIX_PATH=%~5" -DCMAKE_DISABLE_FIND_PACKAGE_PkgConfig=ON
"%~3" -S . -B build-win -G Ninja -DCMAKE_MAKE_PROGRAM="%~4" -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DCMAKE_NINJA_CMCLDEPS_RC=OFF %EXTRA% -DCMAKE_BUILD_TYPE=RelWithDebInfo -DUSE_DX9=OFF -DUSE_OPENXR=ON || exit /b 1
"%~3" --build build-win --parallel 8 || exit /b 1
echo BUILD-OK
