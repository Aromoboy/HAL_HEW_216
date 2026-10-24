@echo off
setlocal
chcp 65001 >nul
rem ミカ：実データを変更せず専用ディレクトリで保存とリソース読込を検証する
pushd "%~dp0..\.."
if not exist "build\mika_october\resources" mkdir "build\mika_october\resources"
call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /EHsc /W4 /utf-8 /DNOMINMAX /MD /Zi /fsanitize=address /I HEWGame\Game\Save /I HEWGame\Game\PowerUp /I HEWGame\Game\Scene /I HEWGame\Engine\Core /I HEWGame\Engine\Render tests\mika_october\resource_regression.cpp HEWGame\Game\Save\PlayerData.cpp HEWGame\Engine\Core\SoundSystem.cpp HEWGame\Engine\Render\TextureDX.cpp /Fo:build\mika_october\resources\ /Fd:build\mika_october\resources\resource_regression.pdb /Fe:build\mika_october\resources\resource_regression.exe /link /INCREMENTAL:NO >build\mika_october\resources\compile.log 2>&1
if errorlevel 1 exit /b 1
build\mika_october\resources\resource_regression.exe build\mika_october\resources\resource-fixture-01
set "regressionResult=%errorlevel%"
popd
exit /b %regressionResult%
