@echo off
setlocal
chcp 65001 >nul
rem ミカ：実際のMedia FoundationとMP4で再生時刻を検証する
pushd "%~dp0..\.."
if errorlevel 1 exit /b 1
set "videoRegressionResult=1"
if not exist "build\mika_october\video_focused" mkdir "build\mika_october\video_focused"
if errorlevel 1 goto finish
call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 goto finish
set "INCLUDE=build\mika_october\video_focused;HEWGame;HEWGame\Engine;HEWGame\Engine\Core;HEWGame\Engine\Render;HEWGame\Engine\Render\Legacy;HEWGame\Game\UI;%INCLUDE%"
rem ミカ：デコーダー部分を毎回本体から抽出しテスト専用コピーの乖離を防ぐ
powershell -NoProfile -Command "$source=[IO.File]::ReadAllText('HEWGame/Game/UI/VideoBackgroundRenderer.cpp'); $marker=$source.IndexOf('struct VideoBackgroundRenderer::DecoderState'); if($marker -lt 0){exit 1}; [IO.File]::WriteAllText('build/mika_october/video_focused/video_decoder_under_test.inc',$source.Substring(0,$marker),[Text.UTF8Encoding]::new($false))"
if errorlevel 1 goto finish
cl /nologo /std:c++20 /EHsc /MT /O2 /Gy /utf-8 /DNOMINMAX /DWIN32 /D_WINDOWS /c ^
 /Fo:build\mika_october\video_focused\VideoBackgroundRenderer.obj ^
 HEWGame\Game\UI\VideoBackgroundRenderer.cpp ^
 >build\mika_october\video_focused\production_compile.log 2>&1
set "videoRegressionResult=%errorlevel%"
if not "%videoRegressionResult%"=="0" goto finish
cl /nologo /std:c++20 /EHsc /MT /O2 /Gy /utf-8 /DNOMINMAX /DWIN32 /D_WINDOWS ^
 /Fo:build\mika_october\video_focused\video_regression.obj ^
 /Fe:build\mika_october\video_focused\video_regression.exe ^
 tests\mika_october\video_regression.cpp ^
 /link /SUBSYSTEM:CONSOLE /OPT:REF /INCREMENTAL:NO ole32.lib ^
 >build\mika_october\video_focused\compile.log 2>&1
set "videoRegressionResult=%errorlevel%"
if not "%videoRegressionResult%"=="0" goto finish
build\mika_october\video_focused\video_regression.exe >build\mika_october\video_focused\run.log 2>&1
set "videoRegressionResult=%errorlevel%"
type build\mika_october\video_focused\run.log
:finish
popd
exit /b %videoRegressionResult%
