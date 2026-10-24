@echo off
setlocal
chcp 65001 >nul
rem ミカ：実装本体と既存のBox2Dライブラリで接触イベントと接着角度を検証する
pushd "%~dp0..\.."
if errorlevel 1 exit /b 1
set "physicsRegressionResult=1"
if not exist "build\mika_october\physics_focused" mkdir "build\mika_october\physics_focused"
if errorlevel 1 goto finish
call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 goto finish
set "INCLUDE=HEWGame;HEWGame\Engine;HEWGame\Engine\Core;HEWGame\Engine\Core\Diagnostics;HEWGame\Engine\Camera;HEWGame\Engine\Render;HEWGame\Engine\Render\Legacy;HEWGame\Engine\Render\VideoTexture;HEWGame\Engine\Config;HEWGame\Engine\Systems\Physics;HEWGame\Engine\Systems\Physics\Collision;HEWGame\Engine\Systems\Physics\Components;HEWGame\Engine\Systems\Draw;HEWGame\Game;HEWGame\Game\Scene;HEWGame\Game\SceneGame;HEWGame\Game\Entities;HEWGame\Game\Sweets;HEWGame\Game\Stage;HEWGame\Game\UI;HEWGame\Game\GameFlow;HEWGame\Game\Rules;HEWGame\Game\Guide;HEWGame\Game\PowerUp;HEWGame\ThirdParty\Assimp\include;HEWGame\DirectXTex;HEWGame\box2d\include;HEWGame\ThirdParty\Effekseer;%INCLUDE%"
cl /nologo /std:c++20 /EHsc /MT /O2 /Gy /utf-8 /c /DNOMINMAX /DWIN32 /D_WINDOWS ^
 /Fo:build\mika_october\physics_focused\ ^
 tests\mika_october\physics_regression.cpp ^
 HEWGame\Engine\Systems\Physics\Box2DContactListener.cpp ^
 HEWGame\Engine\Systems\Physics\Box2DPhysicsSystem.cpp ^
 HEWGame\Engine\Systems\Physics\PhysicsMachine.cpp ^
 HEWGame\Engine\Core\Entities\PhysicalEntity.cpp ^
 HEWGame\Engine\Core\Entities\Entity.cpp ^
 HEWGame\Engine\Core\UpdateNode.cpp ^
 HEWGame\Engine\Core\RegistrationHandle.cpp ^
 HEWGame\Engine\Core\UpdateScheduler.cpp ^
 HEWGame\Engine\Config\ColliderRegistry.cpp ^
 HEWGame\Engine\Core\Debug.cpp >build\mika_october\physics_focused\compile.log 2>&1
set "physicsRegressionResult=%errorlevel%"
if not "%physicsRegressionResult%"=="0" goto finish
link /nologo /SUBSYSTEM:CONSOLE /OPT:REF /INCREMENTAL:NO ^
 /OUT:build\mika_october\physics_focused\physics_regression.exe ^
 build\mika_october\physics_focused\physics_regression.obj ^
 build\mika_october\physics_focused\Box2DContactListener.obj ^
 build\mika_october\physics_focused\Box2DPhysicsSystem.obj ^
 build\mika_october\physics_focused\PhysicsMachine.obj ^
 build\mika_october\physics_focused\PhysicalEntity.obj ^
 build\mika_october\physics_focused\Entity.obj ^
 build\mika_october\physics_focused\UpdateNode.obj ^
 build\mika_october\physics_focused\RegistrationHandle.obj ^
 build\mika_october\physics_focused\UpdateScheduler.obj ^
 build\mika_october\physics_focused\ColliderRegistry.obj ^
 build\mika_october\physics_focused\Debug.obj ^
 HEWGame\box2d\build\src\Release\box2d.lib user32.lib >build\mika_october\physics_focused\link.log 2>&1
set "physicsRegressionResult=%errorlevel%"
if not "%physicsRegressionResult%"=="0" goto finish
build\mika_october\physics_focused\physics_regression.exe
set "physicsRegressionResult=%errorlevel%"
:finish
popd
exit /b %physicsRegressionResult%
