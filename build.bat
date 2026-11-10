@echo off
rem Builds menu.exe (Win32 + DirectX 11). Needs the Visual Studio C++ tools.
setlocal
cd /d "%~dp0"
if not exist imgui\imgui.h git submodule update --init || exit /b 1
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath`) do set "VS=%%i"
if not defined VS echo Visual Studio not found & exit /b 1
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 || exit /b 1
if not exist build mkdir build
cl /nologo /O2 /MT /std:c++17 /EHsc /W3 /DNDEBUG /D_CRT_SECURE_NO_WARNINGS /I imgui /I imgui\backends ^
  main.cpp menu.cpp imgui\imgui.cpp imgui\imgui_draw.cpp imgui\imgui_tables.cpp imgui\imgui_widgets.cpp ^
  imgui\backends\imgui_impl_win32.cpp imgui\backends\imgui_impl_dx11.cpp ^
  /Fo:build\ /Fe:menu.exe /link d3d11.lib d3dcompiler.lib
