@echo off
setlocal
chcp 65001 >nul
rem ミカ：変更前の描画器と現行の描画器を同じD3D11デバイスで比較する
pushd "%~dp0..\.."
if errorlevel 1 exit /b 1
set "modelRenderResult=1"
set "modelRenderOutput=%CD%\build\mika_october\model_render"
set "modelRenderBaseline=%CD%\build\mika_october\optimization-before\HEWGame"
if not exist "%modelRenderOutput%" mkdir "%modelRenderOutput%"
if not exist "%modelRenderBaseline%\Engine\Render\ModelRender.cpp" goto finish
call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 goto finish
set "modelRenderIncludes=/IHEWGame\Engine /IHEWGame\Engine\Core /IHEWGame\Engine\Render /IHEWGame\Engine\Camera /IHEWGame\DirectXTex /IHEWGame\ThirdParty\Assimp\include"
copy /y "%modelRenderBaseline%\Engine\Render\ModelRender.cpp" "%modelRenderOutput%\baseline_model.cpp" >nul
if errorlevel 1 goto finish
cl /nologo /std:c++20 /EHsc /MT /O2 /utf-8 /W3 /DNOMINMAX %modelRenderIncludes% ^
 /DModelRender=BaselineModelRender ^
 /c "%modelRenderOutput%\baseline_model.cpp" /Fo:"%modelRenderOutput%\baseline.obj" ^
 >"%modelRenderOutput%\baseline-compile.log" 2>&1
if errorlevel 1 goto finish
cl /nologo /std:c++20 /EHsc /MT /O2 /utf-8 /W3 /DNOMINMAX %modelRenderIncludes% ^
 /Fo:build\mika_october\model_render\ /Fe:build\mika_october\model_render\model_render_regression.exe ^
 tests\mika_october\model_render_regression.cpp HEWGame\Engine\Render\ModelRender.cpp ^
 HEWGame\Engine\Render\ShaderList.cpp HEWGame\Engine\Render\Shader.cpp HEWGame\Engine\Render\Texture.cpp ^
 HEWGame\Engine\Render\ModelDX.cpp HEWGame\Engine\Render\MeshDX.cpp ^
 HEWGame\Engine\Render\MaterialDX.cpp HEWGame\Engine\Render\DrawMatrix.cpp ^
 HEWGame\Engine\Render\Animation.cpp HEWGame\Engine\Core\VertexBuffer.cpp HEWGame\Engine\Core\IndexBuffer.cpp ^
 "%modelRenderOutput%\baseline.obj" HEWGame\DirectXTex\x64\Release\DirectXTex.lib ^
 HEWGame\ThirdParty\lib\assimp-vc143-mt.lib ole32.lib windowscodecs.lib /link /LIBPATH:HEWGame ^
 >"%modelRenderOutput%\compile.log" 2>&1
if errorlevel 1 goto finish
set "PATH=%CD%\HEWGame\ThirdParty\lib;%PATH%"
pushd HEWGame
"%modelRenderOutput%\model_render_regression.exe" "%modelRenderOutput%" %1 >"%modelRenderOutput%\run.log" 2>&1
set "modelRenderResult=%errorlevel%"
popd
type "%modelRenderOutput%\run.log"
:finish
popd
exit /b %modelRenderResult%
