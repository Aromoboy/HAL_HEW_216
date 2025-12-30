@echo off
setlocal enabledelayedexpansion

rem Usage: copy_box2d_artifacts.bat [build_root] [platform] [config]
rem build_root: CMake build directory that contains src/<Config>/box2d*.lib
rem platform: Win32 or x64
rem config: Debug or Release

set "SCRIPT_DIR=%~dp0"
set "BUILD_ROOT=%~1"
if "%BUILD_ROOT%"=="" set "BUILD_ROOT=%SCRIPT_DIR%..\..\box2d\build"

set "PLATFORM=%~2"
if "%PLATFORM%"=="" set "PLATFORM=Win32"

set "CONFIG=%~3"
if "%CONFIG%"=="" set "CONFIG=Debug"

if /I "%CONFIG%"=="Debug" (
  set "LIB_NAME=box2dd.lib"
) else (
  set "LIB_NAME=box2d.lib"
)

set "SOURCE_LIB=%BUILD_ROOT%\src\%CONFIG%\%LIB_NAME%"
set "DEST_INCLUDE=%SCRIPT_DIR%include"
set "DEST_LIB=%SCRIPT_DIR%lib\%PLATFORM%\%CONFIG%"

if not exist "%SOURCE_LIB%" (
  echo [ERROR] Library not found: %SOURCE_LIB%
  exit /b 1
)

if not exist "%DEST_INCLUDE%" mkdir "%DEST_INCLUDE%"
if not exist "%DEST_LIB%" mkdir "%DEST_LIB%"

robocopy "%SCRIPT_DIR%..\..\box2d\include" "%DEST_INCLUDE%" /MIR >nul
if %ERRORLEVEL% GEQ 8 (
  echo [ERROR] Failed to copy include directory.
  exit /b 1
)

copy /Y "%SOURCE_LIB%" "%DEST_LIB%\" >nul
if %ERRORLEVEL% NEQ 0 (
  echo [ERROR] Failed to copy library.
  exit /b 1
)

echo [OK] Copied include and %LIB_NAME% to ThirdParty/Box2D.
exit /b 0
