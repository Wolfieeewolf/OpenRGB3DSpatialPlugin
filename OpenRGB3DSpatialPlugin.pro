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
MAJOR       = 1
MINOR       = 0
SUFFIX      = git

SHORTHASH   = $$system("git rev-parse --short=7 HEAD")
LASTTAG     = "v"$$MAJOR"."$$MINOR".0"
COMMAND     = "git rev-list --count "$$LASTTAG"..HEAD"
COMMITS     = $$system($$COMMAND)

VERSION_NUM = $$MAJOR"."$$MINOR"."$$COMMITS
VERSION_STR = $$MAJOR"."$$MINOR

VERSION_DEB = $$VERSION_NUM
VERSION_WIX = $$VERSION_NUM
VERSION_AUR = $$VERSION_NUM
VERSION_RPM = $$VERSION_NUM

equals(SUFFIX, "git") {
VERSION_STR = $$VERSION_STR"+ ("$$SUFFIX$$COMMITS")"
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

SOURCES +=                                                                                      \
    OpenRGB/RGBController/RGBController.cpp                                                     \
    OpenRGB/LogManager.cpp                                                                      \
    OpenRGB/qt/hsv.cpp

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
    SpatialEffectTypes.h                                                                        \
    SpatialEffect3D.h                                                                           \
    EffectListManager3D.h                                                                       \
    EffectRegisterer3D.h                                                                        \
    VirtualController3D.h                                                                       \
    VirtualReferencePoint3D.h                                                                   \
    Zone3D.h                                                                                    \
    ZoneManager3D.h                                                                             \
    ui/OpenRGB3DSpatialTab.h                                                                    \
    ui/LEDViewport3D.h                                                                          \
    ui/CustomControllerDialog.h                                                                 \
    ui/Gizmo3D.h                                                                                \
    Effects3D/Wave3D/Wave3D.h                                                                   \
    Effects3D/Wipe3D/Wipe3D.h                                                                   \
    Effects3D/Plasma3D/Plasma3D.h                                                               \
    Effects3D/Spiral3D/Spiral3D.h                                                               \
    Effects3D/Spin3D/Spin3D.h                                                                   \
    Effects3D/Explosion3D/Explosion3D.h                                                         \
    Effects3D/BreathingSphere3D/BreathingSphere3D.h                                             \
    Effects3D/DNAHelix3D/DNAHelix3D.h                                                           \

SOURCES +=                                                                                      \
    OpenRGB3DSpatialPlugin.cpp                                                                  \
    ControllerLayout3D.cpp                                                                      \
    SpatialEffect3D.cpp                                                                         \
    VirtualController3D.cpp                                                                     \
    VirtualReferencePoint3D.cpp                                                                 \
    Zone3D.cpp                                                                                  \
    ZoneManager3D.cpp                                                                           \
    ui/OpenRGB3DSpatialTab.cpp                                                                  \
    ui/OpenRGB3DSpatialTab_RefPoints.cpp                                                        \
    ui/OpenRGB3DSpatialTab_Zones.cpp                                                            \
    ui/LEDViewport3D.cpp                                                                        \
    ui/CustomControllerDialog.cpp                                                               \
    ui/Gizmo3D.cpp                                                                              \
    Effects3D/Wave3D/Wave3D.cpp                                                                 \
    Effects3D/Wipe3D/Wipe3D.cpp                                                                 \
    Effects3D/Plasma3D/Plasma3D.cpp                                                             \
    Effects3D/Spiral3D/Spiral3D.cpp                                                             \
    Effects3D/Spin3D/Spin3D.cpp                                                                 \
    Effects3D/Explosion3D/Explosion3D.cpp                                                       \
    Effects3D/BreathingSphere3D/BreathingSphere3D.cpp                                           \
    Effects3D/DNAHelix3D/DNAHelix3D.cpp                                                         \

#-----------------------------------------------------------------------------------------------#
# Windows-specific Configuration                                                                #
#-----------------------------------------------------------------------------------------------#
win32:CONFIG += QTPLUGIN c++17

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