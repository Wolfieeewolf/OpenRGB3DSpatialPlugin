#-----------------------------------------------------------------------------------------------#
# OpenRGB 3D Spatial Plugin QMake Project                                                      #
#-----------------------------------------------------------------------------------------------#

#-----------------------------------------------------------------------------------------------#
# Qt Configuration                                                                              #
#-----------------------------------------------------------------------------------------------#
QT +=                                                                                           \
    core                                                                                        \
    gui                                                                                         \
    widgets                                                                                     \
    opengl

# Qt 6+ requires openglwidgets module
greaterThan(QT_MAJOR_VERSION, 5): QT += openglwidgets

DEFINES += OPENRGB3DSPATIALPLUGIN_LIBRARY
TEMPLATE = lib

message("OpenRGB3DSpatialPlugin: Qt $$QT_VERSION (QT_MAJOR_VERSION=$$QT_MAJOR_VERSION)")

#-----------------------------------------------------------------------------------------------#
# Build Configuration                                                                           #
#-----------------------------------------------------------------------------------------------#
CONFIG +=                                                                                       \
    plugin                                                                                      \
    silent

# MSVC defaults to a legacy code page for .cpp unless told otherwise; UTF-8 in strings (— … × →)
# would compile to wrong bytes and show garbage in the UI.
msvc {
    QMAKE_CXXFLAGS += /utf-8
    # Qt 6.11+ headers still use Q{Gui,Core}Application::compressEvent (deprecated; removed in Qt 7),
    # which floods the build with C4996 from qguiapplication.h / qapplication.h — not application code.
    greaterThan(QT_MAJOR_VERSION, 5): QMAKE_CXXFLAGS += /wd4996
}

#-----------------------------------------------------------------------------------------------#
# Application Configuration                                                                     #
#-----------------------------------------------------------------------------------------------#
# Date-based versioning: YY.MM.DD.V (e.g., 26.01.20.1)
# Tags should be in format: v26.01.20.1
# Prefer PROJECT_VERSION file in repo root when present; else git describe; else today's date
SHORTHASH   = $$system("git rev-parse --short=7 HEAD")

# 1. Optional PROJECT_VERSION file (one line: YY.MM.DD.V, e.g. 26.02.03.5)
exists(PROJECT_VERSION) {
    win32: VERSION_FROM_FILE = $$system(powershell -NoProfile -Command "(Get-Content PROJECT_VERSION -TotalCount 1).Trim()")
    else: VERSION_FROM_FILE = $$system(head -n1 PROJECT_VERSION 2>/dev/null | tr -d '\\n')
    contains(VERSION_FROM_FILE, "\\."): VERSION_NUM = $$VERSION_FROM_FILE
}

# 2. Try to get version from git describe if no PROJECT_VERSION file
isEmpty(VERSION_NUM) {
# Format: v26.01.20.1 or v26.01.20.1-5-gabc1234
GIT_DESCRIBE = $$system("git describe --tags --always 2>nul || git describe --tags --always 2>/dev/null || echo ''")
!isEmpty(GIT_DESCRIBE) {
    # Remove 'v' prefix and any commit suffix (e.g., "-5-gabc1234")
    VERSION_FROM_TAG = $$replace(GIT_DESCRIBE, "v", "")
    # Remove everything after first dash (commit count and hash)
    VERSION_FROM_TAG = $$section(VERSION_FROM_TAG, "-", 0, 0)
    # Use it if it looks like a version (contains dots)
    contains(VERSION_FROM_TAG, "\\.") {
        VERSION_NUM = $$VERSION_FROM_TAG
    }
}

# If no valid version from tag, use today's date with version 1 (2-digit year)
isEmpty(VERSION_NUM) {
    win32:DATE_YEAR_FULL = $$system(powershell -Command "(Get-Date).ToString('yyyy')")
    win32:DATE_YEAR = $$system(powershell -Command "(Get-Date).ToString('yy')")
    win32:DATE_MONTH = $$system(powershell -Command "(Get-Date).ToString('MM')")
    win32:DATE_DAY = $$system(powershell -Command "(Get-Date).ToString('dd')")
    unix:DATE_YEAR_FULL = $$system(date +%Y)
    unix:DATE_YEAR = $$system(date +%y)
    unix:DATE_MONTH = $$system(date +%m)
    unix:DATE_DAY = $$system(date +%d)
    VERSION_NUM = $$DATE_YEAR"."$$DATE_MONTH"."$$DATE_DAY".1"
}
}

VERSION_STR = $$VERSION_NUM
SUFFIX = git

VERSION_DEB = $$VERSION_NUM
VERSION_WIX = $$VERSION_NUM
VERSION_AUR = $$VERSION_NUM
VERSION_RPM = $$VERSION_NUM

equals(SUFFIX, "git") {
VERSION_STR = $$VERSION_STR"+ ("$$SUFFIX$$SHORTHASH")"
VERSION_DEB = $$VERSION_DEB"~git"$$SHORTHASH
VERSION_AUR = $$VERSION_AUR".g"$$SHORTHASH
VERSION_RPM = $$VERSION_RPM"^git"$$SHORTHASH
} else {
    !isEmpty(SUFFIX) {
VERSION_STR = $$VERSION_STR"+ ("$$SUFFIX")"
VERSION_DEB = $$VERSION_DEB"~"$$SUFFIX
VERSION_AUR = $$VERSION_AUR"."$$SUFFIX
VERSION_RPM = $$VERSION_RPM"^"$$SUFFIX
    }
}

message("VERSION_NUM: "$$VERSION_NUM)
message("VERSION_STR: "$$VERSION_STR)
message("VERSION_DEB: "$$VERSION_DEB)
message("VERSION_WIX: "$$VERSION_WIX)
message("VERSION_AUR: "$$VERSION_AUR)
message("VERSION_RPM: "$$VERSION_RPM)

#-----------------------------------------------------------------------------------------------#
# Automatically generated build information                                                     #
#-----------------------------------------------------------------------------------------------#
win32:BUILDDATE = $$system(date /t)
unix:BUILDDATE  = $$system(date -R -d "@${SOURCE_DATE_EPOCH:-$(date +%s)}")
GIT_COMMIT_ID   = $$system(git --git-dir $$_PRO_FILE_PWD_/.git --work-tree $$_PRO_FILE_PWD_ rev-parse HEAD)
GIT_COMMIT_DATE = $$system(git --git-dir $$_PRO_FILE_PWD_/.git --work-tree $$_PRO_FILE_PWD_ show -s --format=%ci HEAD)
GIT_BRANCH      = $$system(git --git-dir $$_PRO_FILE_PWD_/.git --work-tree $$_PRO_FILE_PWD_ rev-parse --abbrev-ref HEAD)

#-----------------------------------------------------------------------------------------------#
# Download links                                                                                #
#-----------------------------------------------------------------------------------------------#
win32:LATEST_BUILD_URL="https://gitlab.com/OpenRGBDevelopers/OpenRGB3DSpatialPlugin/-/jobs/artifacts/master/download?job=Windows 64"
unix:!macx:LATEST_BUILD_URL="https://gitlab.com/OpenRGBDevelopers/OpenRGB3DSpatialPlugin/-/jobs/artifacts/master/download?job=Linux 64"

#-----------------------------------------------------------------------------------------------#
# Inject vars in defines                                                                        #
#-----------------------------------------------------------------------------------------------#
DEFINES +=                                                                                      \
    VERSION_STRING=\\"\"\"$$VERSION_STR\\"\"\"                                                  \
    BUILDDATE_STRING=\\"\"\"$$BUILDDATE\\"\"\"                                                  \
    GIT_COMMIT_ID=\\"\"\"$$GIT_COMMIT_ID\\"\"\"                                                 \
    GIT_COMMIT_DATE=\\"\"\"$$GIT_COMMIT_DATE\\"\"\"                                             \
    GIT_BRANCH=\\"\"\"$$GIT_BRANCH\\"\"\"                                                       \
    LATEST_BUILD_URL=\\"\"\"$$LATEST_BUILD_URL\\"\"\"                                           \

#-----------------------------------------------------------------------------------------------#
# OpenRGB Plugin SDK                                                                            #
#-----------------------------------------------------------------------------------------------#
INCLUDEPATH +=                                                                                  \
    OpenRGB/                                                                                    \
    OpenRGB/SPDAccessor                                                                         \
    OpenRGB/hidapi_wrapper                                                                      \
    OpenRGB/dependencies/hidapi-win/include                                                     \
    OpenRGB/i2c_smbus                                                                           \
    OpenRGB/RGBController                                                                       \
    OpenRGB/net_port                                                                            \
    OpenRGB/dependencies/json                                                                   \
    OpenRGB/qt                                                                                  \

HEADERS +=                                                                                      \
    OpenRGB/Colors.h                                                                            \
    OpenRGB/OpenRGBPluginInterface.h                                                            \
    OpenRGB/ResourceManagerInterface.h                                                          \
    OpenRGB/RGBController/RGBController.h                                                       \
    OpenRGB/LogManager.h                                                                        \

SOURCES +=                                                                                      \
    OpenRGB/RGBController/RGBController.cpp                                                     \
    OpenRGB/LogManager.cpp                                                                      \
    OpenRGB/qt/hsv.cpp

#-------------------------------------------------------------------#
# Includes                                                          #
#-------------------------------------------------------------------#
INCLUDEPATH +=                                                                                  \
    ui                                                                                          \
    ui/widgets                                                                                  \
    Effects3D                                                                                   \
    Game                                                                                        \
    SpatialSamplers                                                                             \

HEADERS +=                                                                                      \
    OpenRGB3DSpatialPlugin.h                                                                    \
    LEDPosition3D.h                                                                             \
    ControllerLayout3D.h                                                                        \
    GridSpaceUtils.h                                                                            \
    ZoneGrid3D.h                                                                                \
    PluginLogOnce.h                                                                             \
    SpatialEffectTypes.h                                                                        \
    SpatialEffect3D.h                                                                           \
    EffectListManager3D.h                                                                       \
    EffectRegisterer3D.h                                                                        \
    EffectInstance3D.h                                                                          \
    StackPreset3D.h                                                                             \
    VirtualController3D.h                                                                       \
    VirtualReferencePoint3D.h                                                                   \
    Zone3D.h                                                                                    \
    ZoneManager3D.h                                                                             \
    DisplayPlane3D.h                                                                            \
    DisplayPlaneManager.h                                                                       \
    ScreenCaptureManager.h                                                                      \
    Geometry3DUtils.h                                                                           \
    MediaTextureEffectUtils.h                                                                   \
    QtCompat.h                                                                                  \
    Game/GameTelemetryStatusPanel.h                                                             \
    Game/GameTelemetryBridge.h                                                                  \
    Game/LedLayoutCoordinateMap.h                                                               \
    SpatialSamplers/SpatialBasisUtils.h                                                         \
    SpatialSamplers/SpatialLayerCore.h                                                          \
    SpatialSamplers/VoxelRoomCore.h                                                             \
    SpatialSamplers/VoxelMapping.h                                                              \
    Effects3D/Games/Minecraft/MinecraftGameEffect3D.h                                             \
    Effects3D/Games/Minecraft/MinecraftGame.h                                                   \
    Effects3D/Games/Minecraft/MinecraftGameSettings.h                                           \
    Effects3D/Games/Minecraft/MinecraftSubEffect3D.h                                            \
    Effects3D/Games/Minecraft/MinecraftEffectLibrary.h                                          \
    FrequencyRangeEffect3D.h                                                                    \
    ui/OpenRGB3DSpatialTab.h                                                                    \
    ui/LEDViewport3D.h                                                                          \
    ui/CustomControllerDialog.h                                                                 \
    ui/Gizmo3D.h                                                                                \
    ui/CaptureZonesWidget.h                                                                     \
    ui/PluginUiUtils.h                                                                         \
    Effects3D/EffectStratumBlend.h                                                             \
    Effects3D/SpatialKernelColormap.h                                                          \
    ui/widgets/StripKernelColormapPanel.h                                                       \
    ui/widgets/StratumBandPanel.h                                                               \
    Effects3D/SpectrumBars/SpectrumBars.h                                                        \
    Effects3D/BeatPulse/BeatPulse.h                                                             \
    Effects3D/BeatKernelSnap/BeatKernelSnap.h                                                 \
    Effects3D/BandScan/BandScan.h                                                               \
    Effects3D/DualKernelBlend/DualKernelBlend.h                                                \
    Effects3D/KernelFogWash/KernelFogWash.h                                                    \
    Effects3D/KernelHueRipple/KernelHueRipple.h                                                \
    Effects3D/LightningKernelFlash/LightningKernelFlash.h                                      \
    Effects3D/ParticleKernelTrail/ParticleKernelTrail.h                                        \
    Effects3D/RoomScannerKernel/RoomScannerKernel.h                                             \
    Effects3D/SpiralStaircaseKernel/SpiralStaircaseKernel.h                                   \
    Effects3D/TravelingUnfoldKernel/TravelingUnfoldKernel.h                                   \
    Effects3D/AudioLevel/AudioLevel.h                                                            \
    Effects3D/AudioContainer/AudioContainer.h                                                    \
    Effects3D/AudioContainer/AudioEffectLibrary.h                                                \
    Effects3D/AudioPulse/AudioPulse.h                                                           \
    Effects3D/FreqFill/FreqFill.h                                                               \
    Effects3D/FreqRipple/FreqRipple.h                                                          \
    Effects3D/DiscoFlash/DiscoFlash.h                                                          \
    Effects3D/CubeLayer/CubeLayer.h                                                             \
    Audio/AudioInputManager.h                                                                   \
    Effects3D/Plasma/Plasma.h                                                                   \
    Effects3D/Spiral/Spiral.h                                                                   \
    Effects3D/Explosion/Explosion.h                                                             \
    Effects3D/TravelingLight/TravelingLight.h                                                    \
    Effects3D/Wave/Wave.h                                                                        \
    Effects3D/BreathingSphere/BreathingSphere.h                                                 \
    Effects3D/DNAHelix/DNAHelix.h                                                               \
    Effects3D/Tornado/Tornado.h                                                                  \
    Effects3D/Lightning/Lightning.h                                                              \
    Effects3D/Matrix/Matrix.h                                                                   \
    Effects3D/BouncingBall/BouncingBall.h                                                       \
    Effects3D/WireframeCube/WireframeCube.h                                                     \
    Effects3D/PulseRing/PulseRing.h                                                             \
    Effects3D/SurfaceAmbient/SurfaceAmbient.h                                                   \
    Effects3D/Starfield/Starfield.h                                                             \
    Effects3D/Fireworks/Fireworks.h                                                             \
    Effects3D/Bubbles/Bubbles.h                                                                 \
    Effects3D/ColorWheel/ColorWheel.h                                                            \
    Effects3D/Sunrise/Sunrise.h                                                                  \
    Effects3D/ScreenMirror/ScreenMirror.h                                                       \
    Effects3D/ScreenMirror/ScreenMirrorCalibrationPattern.h                                     \
    Effects3D/TextureProjection/TextureProjection.h                                             \
    Effects3D/OmniShapeTexture/OmniShapeTexture.h                                                \
    Effects3D/StripShellPattern/StripShellPattern.h                                              \
    Effects3D/StripShellPattern/StripKernelPatternPalettes.h                                    \
    Effects3D/RotatingConeSpotlights/RotatingConeSpotlights3D.h                                  \

SOURCES +=                                                                                      \
    OpenRGB3DSpatialPlugin.cpp                                                                  \
    ControllerLayout3D.cpp                                                                      \
    GridSpaceUtils.cpp                                                                          \
    ZoneGrid3D.cpp                                                                              \
    PluginLogOnce.cpp                                                                           \
    SpatialEffect3D.cpp                                                                         \
    EffectInstance3D.cpp                                                                        \
    StackPreset3D.cpp                                                                           \
    VirtualController3D.cpp                                                                     \
    VirtualReferencePoint3D.cpp                                                                 \
    Zone3D.cpp                                                                                  \
    ZoneManager3D.cpp                                                                           \
    DisplayPlane3D.cpp                                                                          \
    ScreenCaptureManager.cpp                                                                    \
    Game/GameTelemetryStatusPanel.cpp                                                           \
    Game/GameTelemetryBridge.cpp                                                                \
    SpatialSamplers/SpatialLayerCore.cpp                                                        \
    SpatialSamplers/VoxelRoomCore.cpp                                                           \
    SpatialSamplers/VoxelMapping.cpp                                                            \
    Effects3D/Games/Minecraft/MinecraftGameEffect3D.cpp                                           \
    Effects3D/Games/Minecraft/MinecraftGame.cpp                                                 \
    Effects3D/Games/Minecraft/MinecraftGameSettings.cpp                                         \
    Effects3D/Games/Minecraft/MinecraftSubEffect3D.cpp                                          \
    ui/OpenRGB3DSpatialTab.cpp                                                                  \
    ui/OpenRGB3DSpatialTab_Audio.cpp                                                            \
    ui/OpenRGB3DSpatialTab_Ambilight.cpp                                                        \
    ui/OpenRGB3DSpatialTab_ObjectCreator.cpp                                                    \
    ui/OpenRGB3DSpatialTab_Presets.cpp                                                          \
    ui/OpenRGB3DSpatialTab_Profiles.cpp                                                         \
    ui/OpenRGB3DSpatialTab_RefPoints.cpp                                                        \
    ui/OpenRGB3DSpatialTab_Zones.cpp                                                            \
    ui/OpenRGB3DSpatialTab_EffectStack.cpp                                                      \
    ui/OpenRGB3DSpatialTab_EffectStackRender.cpp                                                \
    ui/OpenRGB3DSpatialTab_EffectStackPersist.cpp                                               \
    ui/OpenRGB3DSpatialTab_EffectProfiles.cpp                                                   \
    ui/OpenRGB3DSpatialTab_StackPresets.cpp                                                     \
    ui/OpenRGB3DSpatialTab_FreqRanges.cpp                                                       \
    ui/LEDViewport3D.cpp                                                                        \
    ui/CustomControllerDialog.cpp                                                               \
    ui/Gizmo3D.cpp                                                                              \
    ui/CaptureZonesWidget.cpp                                                                   \
    ui/widgets/StratumBandPanel.cpp                                                             \
    ui/widgets/StripKernelColormapPanel.cpp                                                     \
    Effects3D/Plasma/Plasma.cpp                                                             \
    Effects3D/Spiral/Spiral.cpp                                                             \
    Effects3D/Explosion/Explosion.cpp                                                       \
    Effects3D/TravelingLight/TravelingLight.cpp                                              \
    Effects3D/Wave/Wave.cpp                                                                  \
    Effects3D/BreathingSphere/BreathingSphere.cpp                                           \
    Effects3D/DNAHelix/DNAHelix.cpp                                                         \
    Effects3D/Tornado/Tornado.cpp                                                           \
    Effects3D/Lightning/Lightning.cpp                                                       \
    Effects3D/Matrix/Matrix.cpp                                                             \
    Effects3D/BouncingBall/BouncingBall.cpp                                                 \
    Effects3D/SpectrumBars/SpectrumBars.cpp                                                 \
    Effects3D/BeatPulse/BeatPulse.cpp                                                       \
    Effects3D/BeatKernelSnap/BeatKernelSnap.cpp                                             \
    Effects3D/BandScan/BandScan.cpp                                                         \
    Effects3D/DualKernelBlend/DualKernelBlend.cpp                                           \
    Effects3D/KernelFogWash/KernelFogWash.cpp                                               \
    Effects3D/KernelHueRipple/KernelHueRipple.cpp                                           \
    Effects3D/LightningKernelFlash/LightningKernelFlash.cpp                                 \
    Effects3D/ParticleKernelTrail/ParticleKernelTrail.cpp                                   \
    Effects3D/RoomScannerKernel/RoomScannerKernel.cpp                                       \
    Effects3D/SpiralStaircaseKernel/SpiralStaircaseKernel.cpp                               \
    Effects3D/TravelingUnfoldKernel/TravelingUnfoldKernel.cpp                               \
    Effects3D/AudioLevel/AudioLevel.cpp                                                     \
    Effects3D/AudioContainer/AudioContainer.cpp                                             \
    Effects3D/AudioPulse/AudioPulse.cpp                                                     \
    Effects3D/FreqFill/FreqFill.cpp                                                         \
    Effects3D/FreqRipple/FreqRipple.cpp                                                     \
    Effects3D/DiscoFlash/DiscoFlash.cpp                                                     \
    Effects3D/CubeLayer/CubeLayer.cpp                                                      \
    Effects3D/WireframeCube/WireframeCube.cpp                                               \
    Effects3D/PulseRing/PulseRing.cpp                                                      \
    Effects3D/SurfaceAmbient/SurfaceAmbient.cpp                                             \
    Effects3D/Starfield/Starfield.cpp                                                      \
    Effects3D/Fireworks/Fireworks.cpp                                                      \
    Effects3D/Bubbles/Bubbles.cpp                                                          \
    Effects3D/ColorWheel/ColorWheel.cpp                                                     \
    Effects3D/Sunrise/Sunrise.cpp                                                         \
    Effects3D/ScreenMirror/ScreenMirror.cpp                                                \
    Effects3D/TextureProjection/TextureProjection.cpp                                      \
    Effects3D/OmniShapeTexture/OmniShapeTexture.cpp                                        \
    Effects3D/StripShellPattern/StripShellPattern.cpp                                      \
    Effects3D/StripShellPattern/StripShellPatternKernels.cpp                               \
    Effects3D/StripShellPattern/StripKernelPatternPalettes.cpp                               \
    Effects3D/RotatingConeSpotlights/RotatingConeSpotlights3D.cpp                            \
    Audio/AudioInputManager.cpp                                                                 \


#-----------------------------------------------------------------------------------------------#
# Windows-specific Configuration                                                                #
#-----------------------------------------------------------------------------------------------#
win32:CONFIG += QTPLUGIN c++17
win32:LIBS += \
    -lOle32 \
    -lOleAut32 \
    -lAvrt \
    -lMmdevapi \
    -lPropsys \
    -luuid \
    -lgdi32 \
    -luser32 \
    -ld3d11 \
    -ldxgi \
    -ld3dcompiler

win32:CONFIG(debug, debug|release) {
    win32:DESTDIR = debug
}

win32:CONFIG(release, debug|release) {
    win32:DESTDIR = release
}

win32:OBJECTS_DIR = _intermediate_$$DESTDIR/.obj
win32:MOC_DIR     = _intermediate_$$DESTDIR/.moc
win32:RCC_DIR     = _intermediate_$$DESTDIR/.qrc
win32:UI_DIR      = _intermediate_$$DESTDIR/.ui

win32:contains(QMAKE_TARGET.arch, x86_64) {
    LIBS +=                                                                                     \
        -lws2_32                                                                                \
        -lole32                                                                                 \
        -lopengl32                                                                              \
        -lglu32                                                                                 \
}

win32:contains(QMAKE_TARGET.arch, x86) {
    LIBS +=                                                                                     \
        -lws2_32                                                                                \
        -lole32                                                                                 \
        -lopengl32                                                                              \
        -lglu32                                                                                 \
}

win32:DEFINES +=                                                                                \
    _MBCS                                                                                       \
    WIN32                                                                                       \
    _CRT_SECURE_NO_WARNINGS                                                                     \
    _WINSOCK_DEPRECATED_NO_WARNINGS                                                             \
    WIN32_LEAN_AND_MEAN                                                                         \

#-----------------------------------------------------------------------------------------------#
# Linux-specific Configuration                                                                  #
#-----------------------------------------------------------------------------------------------#
unix:!macx {
    CONFIG += c++17
    QMAKE_CXXFLAGS += -Wno-psabi
    target.path=$$PREFIX/lib/openrgb/plugins/
    INSTALLS += target

    LIBS += -lGL -lGLU
}

#-----------------------------------------------------------------------------------------------#
# MacOS-specific Configuration                                                                  #
#-----------------------------------------------------------------------------------------------#
QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.15

macx: {
    CONFIG += c++17

    LIBS += -framework OpenGL
}
