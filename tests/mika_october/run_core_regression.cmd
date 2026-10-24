@echo off
setlocal
chcp 65001 >nul
rem ミカ：入力の登録解除と休止後の時間リセットを実装のまま検証する
pushd "%~dp0..\.."
if not exist "build\mika_october\core" mkdir "build\mika_october\core"
call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /utf-8 /DNOMINMAX /MD /Zi /fsanitize=address /I HEWGame\Engine /I HEWGame\Engine\Core tests\mika_october\core_regression.cpp HEWGame\Engine\Core\Input.cpp HEWGame\Engine\Core\InputMap.cpp HEWGame\Engine\Core\Gamepad.cpp HEWGame\Engine\Core\UpdateNode.cpp HEWGame\Engine\Core\RegistrationHandle.cpp HEWGame\Engine\Core\UpdateScheduler.cpp /Fo:build\mika_october\core\ /Fd:build\mika_october\core\core_regression.pdb /Fe:build\mika_october\core\core_regression.exe /link user32.lib /INCREMENTAL:NO >build\mika_october\core\compile.log 2>&1
if errorlevel 1 exit /b 1
build\mika_october\core\core_regression.exe
set "regressionResult=%errorlevel%"
popd
exit /b %regressionResult%
