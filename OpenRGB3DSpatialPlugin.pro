QT += core gui widgets opengl
greaterThan(QT_MAJOR_VERSION, 5): QT += openglwidgets

DEFINES += OPENRGB3DSPATIALPLUGIN_LIBRARY QT_NO_CONNECT_SLOTS_BY_NAME
TEMPLATE = lib

CONFIG += plugin silent c++17
CONFIG -= debug_and_release debug_and_release_target

msvc {
    QMAKE_CXXFLAGS += /utf-8 /bigobj
    greaterThan(QT_MAJOR_VERSION, 5): QMAKE_CXXFLAGS += /wd4996 /Zm300
}

win32:DEFINES += NOMINMAX WIN32_LEAN_AND_MEAN

exists(PROJECT_VERSION) {
    win32: VERSION_FROM_FILE = $$system(powershell -NoProfile -Command "(Get-Content PROJECT_VERSION -TotalCount 1).Trim()")
    else: VERSION_FROM_FILE = $$system(head -n1 PROJECT_VERSION 2>/dev/null | tr -d '\\r\\n' | xargs)
    !isEmpty(VERSION_FROM_FILE): VERSION_NUM = $$VERSION_FROM_FILE
}

isEmpty(VERSION_NUM) {
    GIT_DESCRIBE = $$system("git describe --tags --always 2>nul || git describe --tags --always 2>/dev/null || echo ''")
    !isEmpty(GIT_DESCRIBE) {
        VERSION_FROM_TAG = $$replace(GIT_DESCRIBE, "v", "")
        VERSION_FROM_TAG = $$section(VERSION_FROM_TAG, "-", 0, 0)
        contains(VERSION_FROM_TAG, "\\.") {
            VERSION_NUM = $$VERSION_FROM_TAG
        }
    }
}

isEmpty(VERSION_NUM) {
    win32:DATE_YEAR = $$system(powershell -Command "(Get-Date).ToString('yy')")
    win32:DATE_MONTH = $$system(powershell -Command "(Get-Date).ToString('MM')")
    win32:DATE_DAY = $$system(powershell -Command "(Get-Date).ToString('dd')")
    unix:DATE_YEAR = $$system(date +%y)
    unix:DATE_MONTH = $$system(date +%m)
    unix:DATE_DAY = $$system(date +%d)
    VERSION_NUM = $$DATE_YEAR"."$$DATE_MONTH"."$$DATE_DAY".1"
}

VERSION_STR = $$VERSION_NUM
SUFFIX = git
VERSION_DEB = $$VERSION_NUM
VERSION_WIX = $$VERSION_NUM
VERSION_AUR = $$VERSION_NUM
VERSION_RPM = $$VERSION_NUM

equals(SUFFIX, "git") {
    SHORTHASH = $$system("git rev-parse --short=7 HEAD")
    VERSION_STR = $$VERSION_STR"+ ("$$SUFFIX$$SHORTHASH")"
    VERSION_DEB = $$VERSION_DEB"~git"$$SHORTHASH
    VERSION_AUR = $$VERSION_AUR".g"$$SHORTHASH
    VERSION_RPM = $$VERSION_RPM"^git"$$SHORTHASH
} else:!isEmpty(SUFFIX) {
    VERSION_STR = $$VERSION_STR"+ ("$$SUFFIX")"
    VERSION_DEB = $$VERSION_DEB"~"$$SUFFIX
    VERSION_AUR = $$VERSION_AUR"."$$SUFFIX
    VERSION_RPM = $$VERSION_RPM"^"$$SUFFIX
}

win32:BUILDDATE = $$system(date /t)
unix:BUILDDATE  = $$system(date -R -d "@${SOURCE_DATE_EPOCH:-$(date +%s)}")
GIT_COMMIT_ID   = $$system(git --git-dir $$_PRO_FILE_PWD_/.git --work-tree $$_PRO_FILE_PWD_ rev-parse HEAD)
GIT_COMMIT_DATE = $$system(git --git-dir $$_PRO_FILE_PWD_/.git --work-tree $$_PRO_FILE_PWD_ show -s --format=%ci HEAD)
GIT_BRANCH      = $$system(git --git-dir $$_PRO_FILE_PWD_/.git --work-tree $$_PRO_FILE_PWD_ rev-parse --abbrev-ref HEAD)

win32:LATEST_BUILD_URL="https://gitlab.com/OpenRGBDevelopers/OpenRGB3DSpatialPlugin/-/jobs/artifacts/main/download?job=Windows 64"
unix:!macx:LATEST_BUILD_URL="https://gitlab.com/OpenRGBDevelopers/OpenRGB3DSpatialPlugin/-/jobs/artifacts/main/download?job=Linux 64"

DEFINES += \
    VERSION_STRING=\\"\"\"$$VERSION_STR\\"\"\" \
    BUILDDATE_STRING=\\"\"\"$$BUILDDATE\\"\"\" \
    GIT_COMMIT_ID=\\"\"\"$$GIT_COMMIT_ID\\"\"\" \
    GIT_COMMIT_DATE=\\"\"\"$$GIT_COMMIT_DATE\\"\"\" \
    GIT_BRANCH=\\"\"\"$$GIT_BRANCH\\"\"\" \
    LATEST_BUILD_URL=\\"\"\"$$LATEST_BUILD_URL\\"\"\"

INCLUDEPATH += \
    OpenRGB/ \
    OpenRGB/SPDAccessor \
    OpenRGB/hidapi_wrapper \
    OpenRGB/dependencies/hidapi-win/include \
    OpenRGB/i2c_smbus \
    OpenRGB/RGBController \
    OpenRGB/net_port \
    OpenRGB/dependencies/json \
    OpenRGB/qt

HEADERS += \
    OpenRGB/Colors.h \
    OpenRGB/OpenRGBPluginInterface.h \
    OpenRGB/ResourceManagerInterface.h \
    OpenRGB/RGBController/RGBController.h \
    OpenRGB/LogManager.h

SOURCES += \
    OpenRGB/RGBController/RGBController.cpp \
    OpenRGB/LogManager.cpp \
    OpenRGB/qt/hsv.cpp

RESOURCES += \
    resources/spatial_shaders.qrc \
    resources/plugin_ui.qrc

INCLUDEPATH += \
    ui \
    ui/widgets \
    Effects3D \
    Game \
    SpatialSamplers \
    Shaders \
    Audio

HEADERS += \
    OpenRGB3DSpatialPlugin.h \
    LEDPosition3D.h \
    ControllerLayout3D.h \
    GridSpaceUtils.h \
    ZoneGrid3D.h \
    SpatialEffectTypes.h \
    SpatialEffect3D.h \
    EffectListManager3D.h \
    EffectRegisterer3D.h \
    EffectInstance3D.h \
    StackPreset3D.h \
    VirtualController3D.h \
    VirtualReferencePoint3D.h \
    Zone3D.h \
    ZoneManager3D.h \
    DisplayPlane3D.h \
    DisplayPlaneManager.h \
    ScreenCaptureManager.h \
    Geometry3DUtils.h \
    MediaTextureEffectUtils.h \
    QtCompat.h \
    ui/widgets/GameTelemetryStatusPanel.h \
    Game/GameTelemetryBridge.h \
    Game/LedLayoutCoordinateMap.h \
    SpatialSamplers/SpatialBasisUtils.h \
    SpatialSamplers/SpatialLayerCore.h \
    SpatialSamplers/VoxelRoomCore.h \
    SpatialSamplers/VoxelMapping.h \
    Effects3D/Games/Minecraft/MinecraftGameEffect3D.h \
    Effects3D/Games/Minecraft/MinecraftGame.h \
    Effects3D/Games/Minecraft/MinecraftGameSettings.h \
    Effects3D/Games/Minecraft/MinecraftSubEffect3D.h \
    Effects3D/Games/Minecraft/MinecraftEffectLibrary.h \
    ui/PluginSettingsPaths.h \
    ui/ProfilesTabPanel.h \
    ui/GridSettingsPanel.h \
    ui/SceneTransformPanel.h \
    ui/SceneObjectSpacingPanel.h \
    ui/SceneObjectEditHostPanel.h \
    ui/PositionAxisDragController.h \
    ui/ObjectCreatorTabPanel.h \
    ui/ControllerListPanel.h \
    ui/EffectLibraryPanel.h \
    ui/EffectStackPanel.h \
    ui/ZonesPanel.h \
    ui/EffectGlobalSettingsPanel.h \
    ui/MinecraftLibraryPanel.h \
    ui/AudioInputPanel.h \
    ui/AudioAdvancedSettingsDialog.h \
    ui/EffectControlsHostPanel.h \
    ui/OpenRGB3DSpatialTab.h \
    ui/SpatialTabLedHelpers.h \
    ui/ControllerDisplayUtils.h \
    ui/LEDViewport3D.h \
    ui/ZoneControllerPickerDialog.h \
    ui/CustomControllerDialog.h \
    ui/ReferencePointDialog.h \
    ui/DisplayPlaneDialog.h \
    ui/custom-controller-grid/CustomControllerGridCell.h \
    ui/custom-controller-grid/CustomControllerGridItem.h \
    ui/custom-controller-grid/CustomControllerGridScene.h \
    ui/custom-controller-grid/CustomControllerLayoutGrid.h \
    ui/CustomControllerSourceRef.h \
    ui/CustomControllerDeviceList.h \
    ui/SpatialControllerListBacking.h \
    ui/SpatialControllerEntryKey.h \
    ui/SpatialControllerCardWidget.h \
    ui/SpatialControllerCardList.h \
    ui/CustomControllerDeviceWidget.h \
    ui/CustomControllerPreviewDialog.h \
    ui/OpenRGBPluginsFont.h \
    ui/PluginClickableLabel.h \
    ui/Gizmo3D.h \
    ui/viewport/ViewportMath.h \
    ui/viewport/ViewportCamera.h \
    ui/viewport/ViewportGLFormat.h \
    ui/viewport/ViewportLegacyGL.h \
    ui/viewport/ViewportRenderer.h \
    ui/viewport/ViewportPrograms.h \
    ui/viewport/ViewportGpuMesh.h \
    ui/CaptureZonesWidget.h \
    ui/PluginUiUtils.h \
    Effects3D/EffectStratumBlend.h \
    Effects3D/SpatialKernelColormap.h \
    ui/widgets/StripKernelColormapPanel.h \
    ui/widgets/StratumBandPanel.h \
    ui/widgets/EffectMotionPanel.h \
    ui/widgets/EffectOutputPanel.h \
    ui/widgets/EffectGeometryPanel.h \
    ui/widgets/EffectSurfacesPanel.h \
    ui/widgets/EffectLayerBanner.h \
    ui/widgets/EffectStackBlendRow.h \
    ui/widgets/EffectSamplerPanel.h \
    ui/widgets/EffectColorPanel.h \
    ui/widgets/EffectCustomHost.h \
    ui/widgets/EffectTransportRow.h \
    ui/widgets/EffectSliderRow.h \
    ui/widgets/EffectLabeledComboRow.h \
    ui/widgets/EffectLabeledSpinRow.h \
    ui/widgets/EffectCheckRow.h \
    ui/widgets/EffectInfoLabel.h \
    ui/widgets/EffectSectionHeading.h \
    ui/widgets/EffectUiRows.h \
    ui/widgets/MediaTextureAmbienceBlock.h \
    ui/widgets/AudioEqBandColumn.h \
    ui/widgets/EffectControlsRoot.h \
    Effects3D/SpectrumBars/SpectrumBars.h \
    Effects3D/AudioStripVisualizer/AudioStripVisualizer.h \
    Effects3D/ShaderField/ShaderField.h \
    Shaders/SpatialShaderEngine.h \
    Shaders/SpatialShaderUniforms.h \
    Shaders/SpatialShaderCatalog.h \
    Effects3D/BandScan/BandScan.h \
    Effects3D/AudioReactiveUi.h \
    Effects3D/AudioLevel/AudioLevel.h \
    Effects3D/AudioPulse/AudioPulse.h \
    Effects3D/FreqFill/FreqFill.h \
    Effects3D/DiscoFlash/DiscoFlash.h \
    Effects3D/CubeLayer/CubeLayer.h \
    Audio/AudioInputManager.h \
    Effects3D/Plasma/Plasma.h \
    Effects3D/Spiral/Spiral.h \
    Effects3D/Explosion/Explosion.h \
    Effects3D/TravelingLight/TravelingLight.h \
    Effects3D/Wave/Wave.h \
    Effects3D/BreathingSphere/BreathingSphere.h \
    Effects3D/DNAHelix/DNAHelix.h \
    Effects3D/Tornado/Tornado.h \
    Effects3D/Lightning/Lightning.h \
    Effects3D/Matrix/Matrix.h \
    Effects3D/BouncingBall/BouncingBall.h \
    Effects3D/WireframeCube/WireframeCube.h \
    Effects3D/PulseRing/PulseRing.h \
    Effects3D/SurfaceAmbient/SurfaceAmbient.h \
    Effects3D/Starfield/Starfield.h \
    Effects3D/Fireworks/Fireworks.h \
    Effects3D/Bubbles/Bubbles.h \
    Effects3D/ColorWheel/ColorWheel.h \
    Effects3D/Sunrise/Sunrise.h \
    Effects3D/ScreenMirror/ScreenMirror.h \
    Effects3D/ScreenMirror/ScreenMirrorCalibrationPattern.h \
    Effects3D/ScreenMirror/ScreenMirrorMonitorPanel.h \
    Effects3D/TextureProjection/TextureProjection.h \
    Effects3D/OmniShapeTexture/OmniShapeTexture.h \
    Effects3D/ShellPattern/ShellPattern.h \
    Effects3D/SpatialPatternKernels/SpatialPatternKernels.h \
    Effects3D/SpatialPatternKernels/SpatialPatternPalettes.h \
    Effects3D/RotatingConeSpotlights/RotatingConeSpotlights.h \
    Effects3D/HarmonicPulse/HarmonicPulse.h \
    Effects3D/Bouncer/Bouncer.h \
    Effects3D/HexLattice/HexLattice.h \
    Effects3D/DepthTone/DepthTone.h \
    Effects3D/SharpPulse/SharpPulse.h \
    Effects3D/XorField/XorField.h

SOURCES += \
    OpenRGB3DSpatialPlugin.cpp \
    ControllerLayout3D.cpp \
    GridSpaceUtils.cpp \
    ZoneGrid3D.cpp \
    SpatialEffect3D.cpp \
    EffectInstance3D.cpp \
    StackPreset3D.cpp \
    VirtualController3D.cpp \
    VirtualReferencePoint3D.cpp \
    Zone3D.cpp \
    ZoneManager3D.cpp \
    DisplayPlane3D.cpp \
    ScreenCaptureManager.cpp \
    ui/widgets/GameTelemetryStatusPanel.cpp \
    Game/GameTelemetryBridge.cpp \
    SpatialSamplers/SpatialLayerCore.cpp \
    SpatialSamplers/VoxelRoomCore.cpp \
    SpatialSamplers/VoxelMapping.cpp \
    Effects3D/Games/Minecraft/MinecraftGameEffect3D.cpp \
    Effects3D/Games/Minecraft/MinecraftGame.cpp \
    Effects3D/Games/Minecraft/MinecraftGameSettings.cpp \
    Effects3D/Games/Minecraft/MinecraftSubEffect3D.cpp \
    ui/PluginSettingsPaths.cpp \
    ui/ProfilesTabPanel.cpp \
    ui/GridSettingsPanel.cpp \
    ui/SceneTransformPanel.cpp \
    ui/SceneObjectSpacingPanel.cpp \
    ui/SceneObjectEditHostPanel.cpp \
    ui/OpenRGB3DSpatialTab_SetupSceneObjectEdit.cpp \
    ui/PositionAxisDragController.cpp \
    ui/ObjectCreatorTabPanel.cpp \
    ui/ControllerListPanel.cpp \
    ui/EffectLibraryPanel.cpp \
    ui/EffectStackPanel.cpp \
    ui/ZonesPanel.cpp \
    ui/EffectGlobalSettingsPanel.cpp \
    ui/MinecraftLibraryPanel.cpp \
    ui/AudioInputPanel.cpp \
    ui/AudioAdvancedSettingsDialog.cpp \
    ui/EffectControlsHostPanel.cpp \
    ui/OpenRGB3DSpatialTab.cpp \
    ui/OpenRGB3DSpatialTab_Audio.cpp \
    ui/OpenRGB3DSpatialTab_Setup.cpp \
    ui/OpenRGB3DSpatialTab_SetupDisplayPlanes.cpp \
    ui/OpenRGB3DSpatialTab_Layout.cpp \
    ui/OpenRGB3DSpatialTab_LayoutCustomControllers.cpp \
    ui/OpenRGB3DSpatialTab_Settings.cpp \
    ui/OpenRGB3DSpatialTab_Scene.cpp \
    ui/ZoneControllerPickerDialog.cpp \
    ui/OpenRGB3DSpatialTab_Effects.cpp \
    ui/OpenRGB3DSpatialTab_EffectsRender.cpp \
    ui/OpenRGB3DSpatialTab_EffectsProfiles.cpp \
    ui/LEDViewport3D.cpp \
    ui/LEDViewport3D_Gpu.cpp \
    ui/LEDViewport3D_SceneGpu.cpp \
    ui/CustomControllerDialog.cpp \
    ui/ReferencePointDialog.cpp \
    ui/DisplayPlaneDialog.cpp \
    ui/custom-controller-grid/CustomControllerGridItem.cpp \
    ui/custom-controller-grid/CustomControllerGridScene.cpp \
    ui/custom-controller-grid/CustomControllerLayoutGrid.cpp \
    ui/ControllerCards.cpp \
    ui/ControllerDisplayUtils.cpp \
    ui/CustomControllerWidgets.cpp \
    ui/CustomControllerPreviewDialog.cpp \
    ui/OpenRGBPluginsFont.cpp \
    ui/PluginClickableLabel.cpp \
    ui/Gizmo3D.cpp \
    ui/Gizmo3D_Mesh.cpp \
    ui/viewport/ViewportMath.cpp \
    ui/viewport/ViewportCamera.cpp \
    ui/viewport/ViewportGLFormat.cpp \
    ui/viewport/ViewportRenderer.cpp \
    ui/viewport/ViewportPrograms.cpp \
    ui/viewport/ViewportGpuMesh.cpp \
    ui/CaptureZonesWidget.cpp \
    ui/widgets/StratumBandPanel.cpp \
    ui/widgets/StripKernelColormapPanel.cpp \
    ui/widgets/EffectCommonPanels.cpp \
    ui/widgets/EffectRowWidgets.cpp \
    ui/widgets/EffectStackBlendRow.cpp \
    ui/widgets/EffectSamplerPanel.cpp \
    ui/widgets/EffectCustomHost.cpp \
    ui/widgets/EffectTransportRow.cpp \
    ui/widgets/MediaTextureAmbienceBlock.cpp \
    ui/widgets/AudioEqBandColumn.cpp \
    Effects3D/Plasma/Plasma.cpp \
    Effects3D/Spiral/Spiral.cpp \
    Effects3D/Explosion/Explosion.cpp \
    Effects3D/TravelingLight/TravelingLight.cpp \
    Effects3D/Wave/Wave.cpp \
    Effects3D/BreathingSphere/BreathingSphere.cpp \
    Effects3D/DNAHelix/DNAHelix.cpp \
    Effects3D/Tornado/Tornado.cpp \
    Effects3D/Lightning/Lightning.cpp \
    Effects3D/Matrix/Matrix.cpp \
    Effects3D/BouncingBall/BouncingBall.cpp \
    Effects3D/SpectrumBars/SpectrumBars.cpp \
    Effects3D/AudioStripVisualizer/AudioStripVisualizer.cpp \
    Effects3D/ShaderField/ShaderField.cpp \
    Shaders/SpatialShaderEngine.cpp \
    Shaders/SpatialShaderCatalog.cpp \
    Effects3D/BandScan/BandScan.cpp \
    Effects3D/AudioLevel/AudioLevel.cpp \
    Effects3D/AudioPulse/AudioPulse.cpp \
    Effects3D/FreqFill/FreqFill.cpp \
    Effects3D/DiscoFlash/DiscoFlash.cpp \
    Effects3D/CubeLayer/CubeLayer.cpp \
    Effects3D/WireframeCube/WireframeCube.cpp \
    Effects3D/PulseRing/PulseRing.cpp \
    Effects3D/SurfaceAmbient/SurfaceAmbient.cpp \
    Effects3D/Starfield/Starfield.cpp \
    Effects3D/Fireworks/Fireworks.cpp \
    Effects3D/Bubbles/Bubbles.cpp \
    Effects3D/ColorWheel/ColorWheel.cpp \
    Effects3D/Sunrise/Sunrise.cpp \
    Effects3D/ScreenMirror/ScreenMirror.cpp \
    Effects3D/ScreenMirror/ScreenMirrorMonitorPanel.cpp \
    Effects3D/TextureProjection/TextureProjection.cpp \
    Effects3D/OmniShapeTexture/OmniShapeTexture.cpp \
    Effects3D/ShellPattern/ShellPattern.cpp \
    Effects3D/SpatialPatternKernels/SpatialPatternKernels.cpp \
    Effects3D/SpatialPatternKernels/SpatialPatternPalettes.cpp \
    Effects3D/RotatingConeSpotlights/RotatingConeSpotlights.cpp \
    Effects3D/HarmonicPulse/HarmonicPulse.cpp \
    Effects3D/Bouncer/Bouncer.cpp \
    Effects3D/HexLattice/HexLattice.cpp \
    Effects3D/DepthTone/DepthTone.cpp \
    Effects3D/SharpPulse/SharpPulse.cpp \
    Effects3D/XorField/XorField.cpp \
    Audio/AudioInputManager.cpp

win32:CONFIG += QTPLUGIN
win32:LIBS += \
    -lOle32 -lOleAut32 -lAvrt -lMmdevapi -lPropsys -luuid \
    -lgdi32 -luser32 -ld3d11 -ldxgi -ld3dcompiler \
    -lws2_32 -lopengl32 -lglu32

win32:CONFIG(debug, debug|release): DESTDIR = debug
win32:CONFIG(release, debug|release): DESTDIR = release
win32:OBJECTS_DIR = _intermediate_$$DESTDIR/.obj
win32:MOC_DIR     = _intermediate_$$DESTDIR/.moc
win32:RCC_DIR     = _intermediate_$$DESTDIR/.qrc
win32:UI_DIR      = _intermediate_$$DESTDIR/.ui
win32:QMAKE_DEL_FILE = cmd /c del /f /q 2^>nul

win32:DEFINES += \
    _MBCS \
    WIN32 \
    _CRT_SECURE_NO_WARNINGS \
    _WINSOCK_DEPRECATED_NO_WARNINGS

FORMS += \
    ui/forms/OpenRGB3DSpatialTab.ui \
    ui/forms/ProfilesTabPanel.ui \
    ui/forms/GridSettingsPanel.ui \
    ui/forms/SceneTransformPanel.ui \
    ui/forms/SceneObjectSpacingPanel.ui \
    ui/forms/SceneObjectEditHostPanel.ui \
    ui/forms/ObjectCreatorTabPanel.ui \
    ui/forms/ControllerListPanel.ui \
    ui/forms/MediaTextureAmbienceBlock.ui \
    ui/forms/EffectLibraryPanel.ui \
    ui/forms/EffectStackPanel.ui \
    ui/forms/ZonesPanel.ui \
    ui/forms/EffectGlobalSettingsPanel.ui \
    ui/forms/MinecraftLibraryPanel.ui \
    ui/forms/AudioInputPanel.ui \
    ui/forms/AudioAdvancedSettingsDialog.ui \
    ui/forms/EffectLayerBanner.ui \
    ui/forms/EffectMotionPanel.ui \
    ui/forms/EffectOutputPanel.ui \
    ui/forms/EffectSurfacesPanel.ui \
    ui/forms/EffectGeometryPanel.ui \
    ui/forms/EffectColorPanel.ui \
    ui/forms/ZoneControllerPickerDialog.ui \
    ui/forms/EffectSamplerPanel.ui \
    ui/forms/EffectStackBlendRow.ui \
    ui/forms/CustomControllerDialog.ui \
    ui/forms/ReferencePointDialog.ui \
    ui/forms/DisplayPlaneDialog.ui \
    ui/forms/StratumBandPanel.ui \
    ui/forms/StripKernelColormapPanel.ui \
    ui/forms/GameTelemetryStatusPanel.ui \
    ui/forms/EffectTransportRow.ui \
    ui/forms/AudioEqBandColumn.ui \
    ui/forms/CustomControllerPreviewDialog.ui \
    ui/forms/CaptureZonesWidget.ui \
    ui/forms/SpatialControllerCardWidget.ui \
    ui/forms/SpatialControllerCardList.ui \
    ui/forms/CustomControllerDeviceWidget.ui \
    ui/forms/CustomControllerDeviceList.ui \
    ui/forms/EffectSliderRow.ui \
    ui/forms/EffectLabeledComboRow.ui \
    ui/forms/EffectLabeledSpinRow.ui \
    ui/forms/EffectCheckRow.ui \
    ui/forms/EffectInfoLabel.ui \
    ui/forms/EffectSectionHeading.ui \
    ui/forms/ScreenMirrorCapturePanel.ui \
    ui/forms/ScreenMirrorMonitorSettings.ui \
    ui/forms/ScreenMirrorEffectShell.ui \
    ui/forms/MinecraftGameSettingsScroll.ui

unix:!macx {
    QMAKE_CXXFLAGS += -Wno-psabi
    target.path = $$PREFIX/lib/openrgb/plugins/
    INSTALLS += target
    LIBS += -lGL -lGLU
}

QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.15
macx:LIBS += -framework OpenGL
