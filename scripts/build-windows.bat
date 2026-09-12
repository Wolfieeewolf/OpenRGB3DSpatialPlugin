:: OpenRGB 3D Spatial Plugin Windows build (OpenRGBEffectsPlugin layout).
:: Usage: scripts\build-windows.bat QT_VER MSVC_VER BITS
:: Example: scripts\build-windows.bat 6.8.3 2022 64
@SET QT_VER=%1
@SET MSVC_VER=%2
@SET BITS=%3

@if %BITS% == 32 goto bits_32
@if %BITS% == 64 goto bits_64

:bits_32
@SET MSVC_ARCH=x86
@SET QT_PATH=
goto bits_done

:bits_64
@SET MSVC_ARCH=x64
@SET QT_PATH=_64
goto bits_done

:bits_done

@SET "PATH=%PATH%;C:\Qt\%QT_VER%\msvc%MSVC_VER%%QT_PATH%\bin"
@SET "PATH=%PATH%;C:\Qt\jom"

@call "C:\Program Files (x86)\Microsoft Visual Studio\%MSVC_VER%\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" %MSVC_ARCH%

qmake OpenRGB3DSpatialPlugin.pro CONFIG-=debug_and_release CONFIG+=release
jom
move "release" "OpenRGB3DSpatialPlugin Windows %BITS%-bit"
