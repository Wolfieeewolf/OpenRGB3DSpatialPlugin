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

#-----------------------------------------------------------------------------------------------#
# Build Configuration                                                                           #
#-----------------------------------------------------------------------------------------------#
CONFIG +=                                                                                       \
    plugin                                                                                      \
    silent

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
    OpenRGB/NetworkServer.h                                                                     \
    OpenRGB/StringUtils.h                                                                       \

SOURCES +=                                                                                      \
    OpenRGB/RGBController/RGBController.cpp                                                     \
    OpenRGB/LogManager.cpp                                                                      \
    OpenRGB/qt/hsv.cpp                                                                          \
    OpenRGB/StringUtils.cpp

#-------------------------------------------------------------------#
# Includes                                                          #
#-------------------------------------------------------------------#
INCLUDEPATH +=                                                                                  \
    ui                                                                                          \
    Effects3D                                                                                   \

HEADERS +=                                                                                      \
    OpenRGB3DSpatialPlugin.h                                                                    \
    LEDPosition3D.h                                                                             \
    ControllerLayout3D.h                                                                        \
    GridSpaceUtils.h                                                                            \
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
    QtCompat.h                                                                                  \
    FrequencyRangeEffect3D.h                                                                    \
    ui/OpenRGB3DSpatialTab.h                                                                    \
    ui/LEDViewport3D.h                                                                          \
    ui/CustomControllerDialog.h                                                                 \
    ui/Gizmo3D.h                                                                                \
    Effects3D/Wave3D/Wave3D.h                                                                   \
    Effects3D/Wipe3D/Wipe3D.h                                                                   \
    Effects3D/SpectrumBars3D/SpectrumBars3D.h                                                   \
    Effects3D/BeatPulse3D/BeatPulse3D.h                                                         \
    Effects3D/BandScan3D/BandScan3D.h                                                           \
    Effects3D/AudioLevel3D/AudioLevel3D.h                                                       \
    Effects3D/AudioContainer3D/AudioContainer3D.h                                               \
    Effects3D/AudioPulse3D/AudioPulse3D.h                                                       \
    Effects3D/FreqFill3D/FreqFill3D.h                                                           \
    Effects3D/FreqRipple3D/FreqRipple3D.h                                                       \
    Effects3D/DiscoFlash3D/DiscoFlash3D.h                                                       \
    Effects3D/CubeLayer3D/CubeLayer3D.h                                                       \
    Audio/AudioInputManager.h                                                                   \
    Effects3D/Plasma3D/Plasma3D.h                                                               \
    Effects3D/Spiral3D/Spiral3D.h                                                               \
    Effects3D/Spin3D/Spin3D.h                                                                   \
    Effects3D/Explosion3D/Explosion3D.h                                                         \
    Effects3D/TravelingLight3D/TravelingLight3D.h                                                 \
    Effects3D/BreathingSphere3D/BreathingSphere3D.h                                             \
    Effects3D/DNAHelix3D/DNAHelix3D.h                                                           \
    Effects3D/Tornado3D/Tornado3D.h                                                             \
    Effects3D/Lightning3D/Lightning3D.h                                                         \
    Effects3D/Matrix3D/Matrix3D.h                                                               \
    Effects3D/BouncingBall3D/BouncingBall3D.h                                                   \
    Effects3D/WireframeCube3D/WireframeCube3D.h                                                 \
    Effects3D/WaveSurface3D/WaveSurface3D.h                                                      \
    Effects3D/PulseRing3D/PulseRing3D.h                                                         \
    Effects3D/SurfaceAmbient3D/SurfaceAmbient3D.h                                               \
    Effects3D/Starfield3D/Starfield3D.h                                                         \
    Effects3D/Fireworks3D/Fireworks3D.h                                                          \
    Effects3D/Bubbles3D/Bubbles3D.h                                                              \
    Effects3D/ColorWheel3D/ColorWheel3D.h                                                       \
    Effects3D/Beam3D/Beam3D.h                                                                    \
    Effects3D/MovingPanes3D/MovingPanes3D.h                                                     \
    Effects3D/Sunrise3D/Sunrise3D.h                                                              \
    Effects3D/SkyLightning3D/SkyLightning3D.h                                                     \
    Effects3D/ScreenMirror3D/ScreenMirror3D.h                                                   \

SOURCES +=                                                                                      \
    OpenRGB3DSpatialPlugin.cpp                                                                  \
    ControllerLayout3D.cpp                                                                      \
    GridSpaceUtils.cpp                                                                          \
    SpatialEffect3D.cpp                                                                         \
    EffectInstance3D.cpp                                                                        \
    StackPreset3D.cpp                                                                           \
    VirtualController3D.cpp                                                                     \
    VirtualReferencePoint3D.cpp                                                                 \
    Zone3D.cpp                                                                                  \
    ZoneManager3D.cpp                                                                           \
    DisplayPlane3D.cpp                                                                          \
    ScreenCaptureManager.cpp                                                                    \
    ui/OpenRGB3DSpatialTab.cpp                                                                  \
    ui/OpenRGB3DSpatialTab_Audio.cpp                                                            \
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
    Effects3D/Wave3D/Wave3D.cpp                                                                 \
    Effects3D/Wipe3D/Wipe3D.cpp                                                                 \
    Effects3D/Plasma3D/Plasma3D.cpp                                                             \
    Effects3D/Spiral3D/Spiral3D.cpp                                                             \
    Effects3D/Spin3D/Spin3D.cpp                                                                 \
    Effects3D/Explosion3D/Explosion3D.cpp                                                       \
    Effects3D/TravelingLight3D/TravelingLight3D.cpp                                              \
    Effects3D/BreathingSphere3D/BreathingSphere3D.cpp                                           \
    Effects3D/DNAHelix3D/DNAHelix3D.cpp                                                         \
    Effects3D/Tornado3D/Tornado3D.cpp                                                           \
    Effects3D/Lightning3D/Lightning3D.cpp                                                       \
    Effects3D/Matrix3D/Matrix3D.cpp                                                             \
    Effects3D/BouncingBall3D/BouncingBall3D.cpp                                                 \
    Effects3D/SpectrumBars3D/SpectrumBars3D.cpp                                                 \
    Effects3D/BeatPulse3D/BeatPulse3D.cpp                                                       \
    Effects3D/BandScan3D/BandScan3D.cpp                                                         \
    Effects3D/AudioLevel3D/AudioLevel3D.cpp                                                     \
    Effects3D/AudioContainer3D/AudioContainer3D.cpp                                             \
    Effects3D/AudioPulse3D/AudioPulse3D.cpp                                                     \
    Effects3D/FreqFill3D/FreqFill3D.cpp                                                         \
    Effects3D/FreqRipple3D/FreqRipple3D.cpp                                                     \
    Effects3D/DiscoFlash3D/DiscoFlash3D.cpp                                                     \
    Effects3D/CubeLayer3D/CubeLayer3D.cpp                                                      \
    Effects3D/WireframeCube3D/WireframeCube3D.cpp                                               \
    Effects3D/WaveSurface3D/WaveSurface3D.cpp                                                  \
    Effects3D/PulseRing3D/PulseRing3D.cpp                                                      \
    Effects3D/SurfaceAmbient3D/SurfaceAmbient3D.cpp                                             \
    Effects3D/Starfield3D/Starfield3D.cpp                                                      \
    Effects3D/Fireworks3D/Fireworks3D.cpp                                                      \
    Effects3D/Bubbles3D/Bubbles3D.cpp                                                          \
    Effects3D/ColorWheel3D/ColorWheel3D.cpp                                                     \
    Effects3D/Beam3D/Beam3D.cpp                                                                 \
    Effects3D/MovingPanes3D/MovingPanes3D.cpp                                                  \
    Effects3D/Sunrise3D/Sunrise3D.cpp                                                         \
    Effects3D/SkyLightning3D/SkyLightning3D.cpp                                                 \
    Effects3D/ScreenMirror3D/ScreenMirror3D.cpp                                                \
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
    -ldxgi

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

# Suppress size_t to unsigned int conversion warnings from OpenRGB submodule
win32:QMAKE_CXXFLAGS += /wd4267

#-----------------------------------------------------------------------------------------------#
# Linux-specific Configuration                                                                  #
#-----------------------------------------------------------------------------------------------#
unix:!macx {
    QMAKE_CXXFLAGS += -std=c++17 -Wno-psabi
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
