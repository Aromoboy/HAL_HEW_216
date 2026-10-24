@echo off
setlocal
chcp 65001 >nul
rem ミカ：実装本体をWARPで描画し、ウィンドウなしでPNGと画素比較を出力する
pushd "%~dp0..\.."
if errorlevel 1 exit /b 1
set "backgroundRegressionResult=1"
set "backgroundRegressionOutput=%CD%\build\mika_october\background"
if not exist "%backgroundRegressionOutput%" mkdir "%backgroundRegressionOutput%"
if errorlevel 1 goto finish
call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 goto finish
cl /nologo /std:c++20 /EHsc /MT /O2 /utf-8 /W4 /DNOMINMAX ^
 /IHEWGame\Engine /IHEWGame\Engine\Core /IHEWGame\Engine\Render /IHEWGame\Game\UI /IHEWGame\DirectXTex ^
 /Fo:build\mika_october\background\ ^
 /Fe:build\mika_october\background\background_regression.exe ^
 tests\mika_october\background_regression.cpp ^
 HEWGame\Game\UI\BackgroundRenderer.cpp ^
 HEWGame\Engine\Render\Shader.cpp ^
 HEWGame\Engine\Render\Texture.cpp ^
 HEWGame\Engine\Render\MeshBuffer.cpp ^
 HEWGame\Engine\Render\Sprite.cpp ^
 HEWGame\Engine\Core\VertexBuffer.cpp ^
 HEWGame\Engine\Core\IndexBuffer.cpp ^
 HEWGame\DirectXTex\x64\Release\DirectXTex.lib ole32.lib windowscodecs.lib /link /LIBPATH:HEWGame ^
 >"%backgroundRegressionOutput%\compile.log" 2>&1
set "backgroundRegressionResult=%errorlevel%"
if not "%backgroundRegressionResult%"=="0" goto finish
pushd HEWGame
"%backgroundRegressionOutput%\background_regression.exe" "%backgroundRegressionOutput%" >"%backgroundRegressionOutput%\run.log" 2>&1
set "backgroundRegressionResult=%errorlevel%"
popd
type "%backgroundRegressionOutput%\run.log"
:finish
popd
exit /b %backgroundRegressionResult%
