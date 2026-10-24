@echo off
setlocal
chcp 65001 >nul
rem ミカ：動画の実シェーダーで境界の色を比較する
pushd "%~dp0..\.."
if errorlevel 1 exit /b 1
set "videoSamplingResult=1"
if not exist "build\mika_october\video_sampling" mkdir "build\mika_october\video_sampling"
call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 goto finish
cl /nologo /std:c++20 /EHsc /MT /O2 /utf-8 /DNOMINMAX ^
 /Fo:build\mika_october\video_sampling\ /Fe:build\mika_october\video_sampling\video_sampling_regression.exe ^
 tests\mika_october\video_sampling_regression.cpp /link d3d11.lib d3dcompiler.lib ^
 >build\mika_october\video_sampling\compile.log 2>&1
if errorlevel 1 goto finish
build\mika_october\video_sampling\video_sampling_regression.exe >build\mika_october\video_sampling\run.log 2>&1
set "videoSamplingResult=%errorlevel%"
type build\mika_october\video_sampling\run.log
:finish
popd
exit /b %videoSamplingResult%
