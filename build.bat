@echo off
setlocal enabledelayedexpansion

echo [*] Initializing Visual Studio Build Tools...

set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat"
if not exist "%VCVARS%" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
)
if not exist "%VCVARS%" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat"
)
if not exist "%VCVARS%" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
)

if not exist "%VCVARS%" (
    echo [-] Error: vcvarsall.bat not found. Please verify Visual Studio installation.
    exit /b 1
)

call "%VCVARS%" x64 >nul 2>&1

echo [*] Compiling Dual-Mode (Internal ^& External) KtnaGUI Engine...

set CXXFLAGS=/std:c++20 /D_SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS /O2 /W3 /EHsc /MD /nologo /I"include"
set LIBS=user32.lib gdi32.lib d3d11.lib dxgi.lib d3dcompiler.lib dwmapi.lib WindowsApp.lib winhttp.lib

set CORE_SOURCES=src\void_font.cpp src\void_draw.cpp src\void_layout.cpp src\void_widgets.cpp src\void_popup.cpp src\void_gui.cpp src\void_gui_d3d11.cpp src\void_overlay.cpp src\void_hook.cpp src\void_spotify.cpp src\void_playerbar.cpp

cl %CXXFLAGS% %CORE_SOURCES% demo\main.cpp /Fe:ktna_demo.exe /link %LIBS% /SUBSYSTEM:WINDOWS

if %ERRORLEVEL% EQU 0 (
    echo [+] Build successful: ktna_demo.exe
    del *.obj 2>nul
) else (
    echo [-] Build failed with exit code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)
