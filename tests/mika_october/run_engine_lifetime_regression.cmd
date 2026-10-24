@echo off
setlocal
chcp 65001 >nul
rem ミカ：ゲームを起動せずWARPデバイスで描画資源の寿命を検証する
pushd "%~dp0..\.."
if errorlevel 1 exit /b 1
set "engineLifetimeResult=1"
if not exist "build\mika_october\engine_lifetime" mkdir "build\mika_october\engine_lifetime"
if errorlevel 1 goto finish
call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 goto finish
cl /nologo /std:c++20 /EHsc /MT /O2 /utf-8 /W4 /we4150 /we4154 /DNOMINMAX ^
 /IHEWGame\Engine /IHEWGame\Engine\Core /IHEWGame\Engine\Render ^
 /Fo:build\mika_october\engine_lifetime\ ^
 /Fe:build\mika_october\engine_lifetime\engine_lifetime_regression.exe ^
 tests\mika_october\engine_lifetime_regression.cpp ^
 HEWGame\Engine\Render\MeshBuffer.cpp ^
 HEWGame\Engine\Render\Geometory.cpp ^
 HEWGame\Engine\Core\VertexBuffer.cpp ^
 HEWGame\Engine\Core\IndexBuffer.cpp ^
 >build\mika_october\engine_lifetime\compile.log 2>&1
set "engineLifetimeResult=%errorlevel%"
if not "%engineLifetimeResult%"=="0" goto finish
build\mika_october\engine_lifetime\engine_lifetime_regression.exe
set "engineLifetimeResult=%errorlevel%"
:finish
popd
exit /b %engineLifetimeResult%
