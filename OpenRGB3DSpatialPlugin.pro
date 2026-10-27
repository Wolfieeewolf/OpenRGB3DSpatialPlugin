QT += core gui widgets opengl network
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
    VERSION_NUM = 0.0.0
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

PROJECT_DESC = "Organize and control RGB devices in a 3D grid with spatial effects"
PROJECT_NAME = "OpenRGB 3D Spatial LED Control"
PROJECT_URL  = "https://gitlab.com/wolfieeewolf1/OpenRGB3DSpatialPlugin"

DEFINES += \
    VERSION_STRING=\\"\"\"$$VERSION_STR\\"\"\" \
    BUILDDATE_STRING=\\"\"\"$$BUILDDATE\\"\"\" \
    GIT_COMMIT_ID=\\"\"\"$$GIT_COMMIT_ID\\"\"\" \
    GIT_COMMIT_DATE=\\"\"\"$$GIT_COMMIT_DATE\\"\"\" \
    GIT_BRANCH=\\"\"\"$$GIT_BRANCH\\"\"\"

#-----------------------------------------------------------------------------------------------#
# Generate OpenRGB plugin metadata JSON (required by PluginManager API v5)                      #
#-----------------------------------------------------------------------------------------------#
JSON_FILE_IN           = $$PWD/OpenRGB3DSpatialPlugin.json.in
JSON_FILE_OUT          = $$PWD/OpenRGB3DSpatialPlugin.json

prebuild_json.target   = prebuild_json_target
prebuild_json.depends  = FORCE
prebuild_json.commands = $$QMAKE_STREAM_EDITOR -e \"s|VERSION_NUM|$$VERSION_NUM|g\" \
                                               -e \"s|VERSION_STR|$$VERSION_STR|g\" \
                                               -e \"s|GIT_COMMIT_ID|$$GIT_COMMIT_ID|g\" \
                                               -e \"s|PROJECT_DESC|$$PROJECT_DESC|g\" \
                                               -e \"s|PROJECT_NAME|$$PROJECT_NAME|g\" \
                                               -e \"s|PROJECT_URL|$$PROJECT_URL|g\" \
                                               $$JSON_FILE_IN > $$JSON_FILE_OUT

QMAKE_EXTRA_TARGETS   += prebuild_json
PRE_TARGETDEPS        += prebuild_json_target

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
    OpenRGB/ResourceManagerCallback.h \
    OpenRGB/RGBController/RGBControllerInterface.h \
    OpenRGB/RGBController/RGBController.h \
    OpenRGB/LogManager.h \
    PluginLog.h

RESOURCES += \
    resources/plugin_ui.qrc

INCLUDEPATH += \
    ui \
    ui/widgets \
    Effects3D \
    engines/movie-sync \
    Effects3D/Actions \
    engines/triggers \
    Effects3D/FolderVolume \
    engines/game \
    SpatialSamplers \
    SpatialRoom \
    SpatialLighting \
    Shaders \
    Audio \
    engines/reactive \
    engines/screen-mirror \
    engines/shader-field \
    engines/volume-field \
    engines/particle-field \
    engines/ribbon-field

HEADERS += \
    OpenRGB3DSpatialPlugin.h \
    LEDPosition3D.h \
    ControllerLayout3D.h \
    MatrixWiringOrder.h \
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
    SceneProp3D.h \
    DisplayPlaneManager.h \
    ScenePropManager.h \
    engines/screen-mirror/ScreenCaptureManager.h \
    engines/screen-mirror/ScreenCaptureDownscale.h \
    engines/screen-mirror/DisplayPlaneCaptureLabels.h \
    Geometry3DUtils.h \
    TransformJson.h \
    MediaTextureEffectUtils.h \
    engines/game/Game/StripPatternSurface.h \
    QtCompat.h \
    ui/widgets/GameTelemetryStatusPanel.h \
    engines/game/Game/GameTelemetryBridge.h \
    engines/game/Game/RoomSampleFrameProtocol.h \
    engines/game/Game/RoomSampleShmPaths.h \
    engines/game/Game/RoomSampleFrameShmReader.h \
    engines/game/Game/RoomSampleConfigPublisher.h \
    SpatialSamplers/SpatialBasisUtils.h \
    SpatialSamplers/SpatialLayerCore.h \
    SpatialSamplers/RoomSampleMapping.h \
    engines/game/Minecraft/MinecraftGame.h \
    engines/game/Minecraft/MinecraftGameSettings.h \
    engines/game/Minecraft/MinecraftSubEffect3D.h \
    engines/game/Minecraft/MinecraftHealth/MinecraftHealthEffect3D.h \
    engines/game/Minecraft/MinecraftHunger/MinecraftHungerEffect3D.h \
    engines/game/Minecraft/MinecraftAir/MinecraftAirEffect3D.h \
    engines/game/Minecraft/MinecraftDurability/MinecraftDurabilityEffect3D.h \
    engines/game/Minecraft/MinecraftDamage/MinecraftDamageEffect3D.h \
    engines/game/Minecraft/MinecraftRoomAmbilight/MinecraftRoomAmbilightEffect3D.h \
    Effects3D/Actions/Action.h \
    Effects3D/Actions/ActionMedia.h \
    Effects3D/Actions/ActionPlayer.h \
    Effects3D/Actions/ActionApplier.h \
    Effects3D/Actions/ActionApplierDetail.h \
    engines/volume-field/ActionVolumeField.h \
    engines/particle-field/ActionParticleField.h \
    engines/ribbon-field/ActionRibbonField.h \
    Effects3D/Actions/ActionBuiltinGpu.h \
    Effects3D/Actions/ActionLibrary.h \
    engines/triggers/TriggerBinding.h \
    engines/triggers/TriggerSource.h \
    engines/triggers/ManualTriggerSource.h \
    engines/triggers/WindowsTriggerSource.h \
    engines/triggers/LinuxTriggerSource.h \
    engines/triggers/MacTriggerSource.h \
    engines/triggers/TriggerSourceRegistry.h \
    engines/triggers/TriggerRuntime.h \
    SpatialRoom/SpatialRoomTypes.h \
    SpatialRoom/SpatialRoomDefaults.h \
    SpatialRoom/SpatialRoomFrame.h \
    SpatialLighting/BlockerGridOccluder.h \
    SpatialLighting/OccluderSpatialIndex.h \
    SpatialLighting/SpatialLightingEngine.h \
    SpatialLighting/EmitterRelayMirror.h \
    SpatialLighting/EmitterLocalSampling.h \
    SpatialLighting/SpatialLightingSceneProvider.h \
    Effects3D/SpatialLighting/RoomSpatialLightingUi.h \
    ui/PluginSettingsPaths.h \
    ui/GridSettingsPanel.h \
    ui/SceneTransformPanel.h \
    ui/SceneObjectSpacingPanel.h \
    ui/SceneObjectEditHostPanel.h \
    ui/PositionAxisDragController.h \
    ui/ObjectCreatorTabPanel.h \
    ui/ControllerListPanel.h \
    ui/EffectLibraryPanel.h \
    ui/ActionsPanel.h \
    ui/ActionEditorDialog.h \
    ui/ActionTimelineWidget.h \
    ui/ActionGradientBar.h \
    ui/ActionCurveBar.h \
    ui/ActionToolBar.h \
    ui/TriggersPanel.h \
    ui/EffectStackPanel.h \
    ui/ZonesPanel.h \
    ui/EffectGlobalSettingsPanel.h \
    ui/AudioInputPanel.h \
    ui/AudioAnalyzerSettings.h \
    ui/EffectControlsHostPanel.h \
    ui/OpenRGB3DSpatialTab.h \
    ui/SpatialTabLedHelpers.h \
    ui/ControllerDisplayUtils.h \
    ui/TooltipProxy.h \
    ui/LEDViewport3D.h \
    ui/LEDViewport3D_Internal.h \
    ui/ZoneControllerPickerDialog.h \
    ui/CustomControllerTypes.h \
    ui/CustomControllerMappingUtils.h \
    ui/CustomControllerGridKeys.h \
    ui/CustomControllerClipboard.h \
    ui/CustomControllerHistory.h \
    ui/CustomControllerDialog.h \
    ui/CustomControllerDialog_Internal.h \
    ui/ReferencePointDialog.h \
    ui/DisplayPlaneDialog.h \
    ui/ScenePropDialog.h \
    ui/ScenePropPreviewWidget.h \
    ui/DisplayPlaneCaptureCombo.h \
    ui/custom-controller-grid/CustomControllerGridCell.h \
    ui/custom-controller-grid/CustomControllerGridLayoutMath.h \
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
    ui/viewport/ViewportGLFormat.h \
    ui/viewport/GlProgram.h \
    ui/viewport/MeshBatch.h \
    ui/viewport/MeshGeometry.h \
    ui/viewport/MeshImport.h \
    ui/viewport/ScenePropMeshCache.h \
    ui/viewport/ScenePropMeshPaths.h \
    ui/viewport/ViewportShaders.h \
    ui/CaptureZonesWidget.h \
    ui/PluginUiUtils.h \
    Effects3D/EffectStratumBlend.h \
    Effects3D/EffectColorUtils.h \
    Effects3D/EffectUiSync.h \
    Effects3D/AudioReactiveCommon.h \
    Effects3D/SpatialKernelColormap.h \
    ui/widgets/StripKernelColormapPanel.h \
    ui/widgets/StratumBandPanel.h \
    ui/widgets/EffectMotionPanel.h \
    ui/widgets/EffectOutputPanel.h \
    ui/widgets/EffectGeometryPanel.h \
    ui/widgets/EffectSurfacesPanel.h \
    ui/widgets/EffectLayerBanner.h \
    ui/widgets/EffectStackBlendRow.h \
    ui/widgets/EffectColorPanel.h \
    ui/widgets/EffectCustomHost.h \
    ui/widgets/EffectTransportRow.h \
    ui/widgets/EffectSliderRow.h \
    ui/widgets/EffectLabeledComboRow.h \
    ui/widgets/EffectLabeledSpinRow.h \
    ui/widgets/EffectCheckRow.h \
    ui/widgets/EffectInfoLabel.h \
    ui/widgets/EffectSectionHeading.h \
    ui/widgets/EffectCollapsibleSection.h \
    ui/widgets/RoomSpatialLightSettingsPanel.h \
    ui/widgets/EffectRoomOutputPanel.h \
    ui/widgets/RoomOutputDeviceCard.h \
    ui/widgets/EffectUiRows.h \
    ui/widgets/MediaTextureAmbienceBlock.h \
    ui/widgets/AudioEqBandColumn.h \
    ui/widgets/EffectControlsRoot.h \
    engines/shader-field/ShaderField.h \
    Effects3D/PlayerEngines.h \
    engines/movie-sync/MovieSync.h \
    engines/movie-sync/MovieSyncCatalog.h \
    engines/movie-sync/MovieSyncListener.h \
    engines/movie-sync/MovieSyncRuntime.h \
    EffectListManager3D.h \
    engines/shader-field/SpatialShaderEngine.h \
    engines/shader-field/SpatialShaderUniforms.h \
    engines/shader-field/SpatialShaderCatalog.h \
    Shaders/GlslUniformArray.h \
    Shaders/SpatialFieldAssistBase.h \
    engines/volume-field/SpatialVolumeFieldEngine.h \
    Shaders/SpatialOffscreenGlPool.h \
    Shaders/FieldFullscreenQuad410.h \
    engines/volume-field/SpatialVolumeFieldAssist.h \
    Shaders/SpatialStripFieldEngine.h \
    Shaders/SpatialStripFieldAssist.h \
    Effects3D/AudioReactiveUi.h \
    Audio/AudioInputManager.h \
    engines/reactive/ReactiveInputTypes.h \
    engines/reactive/ReactiveKeyMap.h \
    engines/reactive/ReactiveInputManager.h \
    engines/reactive/Reactive.h \
    Effects3D/FolderVolume/FolderVolumeEffect.h \
    engines/screen-mirror/ScreenMirror.h \
    engines/screen-mirror/ScreenMirror_Internal.h \
    engines/screen-mirror/ScreenMirrorWaveMath.h \
    engines/screen-mirror/ScreenMirrorCalibrationPattern.h \
    engines/screen-mirror/ScreenMirrorMonitorPanel.h \
    Effects3D/SpatialPatternKernels/SpatialPatternKernels.h \
    Effects3D/SpatialPatternKernels/SpatialPatternPalettes.h \

SOURCES += \
    OpenRGB3DSpatialPlugin.cpp \
    ControllerLayout3D.cpp \
    MatrixWiringOrder.cpp \
    GridSpaceUtils.cpp \
    ZoneGrid3D.cpp \
    SpatialEffect3D.cpp \
    SpatialEffect3D_Eval.cpp \
    SpatialEffect3D_Settings.cpp \
    EffectInstance3D.cpp \
    StackPreset3D.cpp \
    VirtualController3D.cpp \
    VirtualReferencePoint3D.cpp \
    Zone3D.cpp \
    ZoneManager3D.cpp \
    DisplayPlane3D.cpp \
    SceneProp3D.cpp \
    engines/screen-mirror/ScreenCaptureManager.cpp \
    ui/widgets/GameTelemetryStatusPanel.cpp \
    engines/game/Game/GameTelemetryBridge.cpp \
    engines/game/Game/RoomSampleFrameShmReader.cpp \
    engines/game/Game/RoomSampleConfigPublisher.cpp \
    SpatialSamplers/SpatialLayerCore.cpp \
    SpatialSamplers/RoomSampleMapping.cpp \
    engines/game/Minecraft/MinecraftGame.cpp \
    engines/game/Minecraft/MinecraftGameSettings.cpp \
    engines/game/Minecraft/MinecraftSubEffect3D.cpp \
    engines/game/Minecraft/MinecraftHealth/MinecraftHealthEffect3D.cpp \
    engines/game/Minecraft/MinecraftHunger/MinecraftHungerEffect3D.cpp \
    engines/game/Minecraft/MinecraftAir/MinecraftAirEffect3D.cpp \
    engines/game/Minecraft/MinecraftDurability/MinecraftDurabilityEffect3D.cpp \
    engines/game/Minecraft/MinecraftDamage/MinecraftDamageEffect3D.cpp \
    engines/game/Minecraft/MinecraftRoomAmbilight/MinecraftRoomAmbilightEffect3D.cpp \
    Effects3D/Actions/Action.cpp \
    Effects3D/Actions/ActionMedia.cpp \
    Effects3D/Actions/ActionSerialize.cpp \
    Effects3D/Actions/ActionClipEvalAxis.cpp \
    Effects3D/Actions/ActionSpatial.cpp \
    Effects3D/Actions/ActionScript.cpp \
    engines/volume-field/ActionVolumeField.cpp \
    engines/particle-field/ActionParticleField.cpp \
    engines/ribbon-field/ActionRibbonField.cpp \
    Effects3D/Actions/ActionBuiltinGpu.cpp \
    Effects3D/Actions/ActionApplier.cpp \
    Effects3D/Actions/ActionApplierMatch.cpp \
    Effects3D/Actions/ActionApplierSpatial.cpp \
    Effects3D/Actions/ActionLibrary.cpp \
    engines/triggers/TriggerBinding.cpp \
    engines/triggers/WindowsTriggerSource.cpp \
    engines/triggers/LinuxTriggerSource.cpp \
    engines/triggers/MacTriggerSource.cpp \
    engines/triggers/TriggerSourceRegistry.cpp \
    engines/triggers/TriggerRuntime.cpp \
    SpatialRoom/SpatialRoomDefaults.cpp \
    SpatialRoom/SpatialRoomFrame.cpp \
    SpatialLighting/BlockerGridOccluder.cpp \
    SpatialLighting/OccluderSpatialIndex.cpp \
    SpatialLighting/SpatialLightingEngine.cpp \
    SpatialLighting/EmitterRelayMirror.cpp \
    SpatialLighting/EmitterLocalSampling.cpp \
    SpatialLighting/SpatialLightingSceneProvider.cpp \
    Effects3D/SpatialLighting/RoomSpatialLightingUi.cpp \
    ui/PluginSettingsPaths.cpp \
    ui/GridSettingsPanel.cpp \
    ui/SceneTransformPanel.cpp \
    ui/SceneObjectSpacingPanel.cpp \
    ui/SceneObjectEditHostPanel.cpp \
    ui/OpenRGB3DSpatialTab_SetupSceneObjectEdit.cpp \
    ui/PositionAxisDragController.cpp \
    ui/ObjectCreatorTabPanel.cpp \
    ui/ControllerListPanel.cpp \
    ui/EffectLibraryPanel.cpp \
    ui/ActionsPanel.cpp \
    ui/TriggersPanel.cpp \
    ui/ActionEditorDialog.cpp \
    ui/ActionEditorProps.cpp \
    ui/ActionEditorTimelineBridge.cpp \
    ui/ActionTimelineWidget.cpp \
    ui/ActionTimelinePaint.cpp \
    ui/ActionTimelineInteract.cpp \
    ui/ActionGradientBar.cpp \
    ui/ActionCurveBar.cpp \
    ui/ActionUserGradients.cpp \
    ui/ActionEffectFiles.cpp \
    ui/ActionUserCurves.cpp \
    ui/ActionToolBar.cpp \
    ui/EffectStackPanel.cpp \
    ui/ZonesPanel.cpp \
    ui/EffectGlobalSettingsPanel.cpp \
    ui/AudioInputPanel.cpp \
    ui/AudioAnalyzerSettings.cpp \
    ui/EffectControlsHostPanel.cpp \
    ui/OpenRGB3DSpatialTab.cpp \
    ui/OpenRGB3DSpatialTab_Audio.cpp \
    ui/OpenRGB3DSpatialTab_Setup.cpp \
    ui/SpatialTabLedHelpers.cpp \
    ui/OpenRGB3DSpatialTab_SetupDisplayPlanes.cpp \
    ui/OpenRGB3DSpatialTab_SetupSceneProps.cpp \
    ui/OpenRGB3DSpatialTab_Layout.cpp \
    ui/OpenRGB3DSpatialTab_LayoutCustomControllers.cpp \
    ui/OpenRGB3DSpatialTab_Settings.cpp \
    ui/OpenRGB3DSpatialTab_Scene.cpp \
    ui/ZoneControllerPickerDialog.cpp \
    ui/OpenRGB3DSpatialTab_Effects.cpp \
    ui/OpenRGB3DSpatialTab_EffectsRender.cpp \
    ui/OpenRGB3DSpatialTab_EffectsProfiles.cpp \
    ui/LEDViewport3D.cpp \
    ui/LEDViewport3D_Input.cpp \
    ui/LEDViewport3D_Camera.cpp \
    ui/LEDViewport3D_Draw.cpp \
    ui/LEDViewport3D_Controllers.cpp \
    ui/LEDViewport3D_SceneObjects.cpp \
    ui/LEDViewport3D_DisplayPlanes.cpp \
    ui/LEDViewport3D_SceneProps.cpp \
    ui/CustomControllerDialog.cpp \
    ui/CustomControllerDialog_Grid.cpp \
    ui/CustomControllerDialog_Sources.cpp \
    ui/CustomControllerDialog_Transform.cpp \
    ui/CustomControllerMappingUtils.cpp \
    ui/ReferencePointDialog.cpp \
    ui/DisplayPlaneDialog.cpp \
    ui/ScenePropDialog.cpp \
    ui/ScenePropPreviewWidget.cpp \
    ui/DisplayPlaneCaptureCombo.cpp \
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
    ui/viewport/ViewportGLFormat.cpp \
    ui/viewport/GlProgram.cpp \
    ui/viewport/MeshBatch.cpp \
    ui/viewport/MeshGeometry.cpp \
    ui/viewport/MeshImport.cpp \
    ui/viewport/ViewportShaders.cpp \
    ui/CaptureZonesWidget.cpp \
    ui/widgets/StratumBandPanel.cpp \
    ui/widgets/StripKernelColormapPanel.cpp \
    ui/widgets/EffectCommonPanels.cpp \
    ui/widgets/EffectRowWidgets.cpp \
    ui/widgets/RoomSpatialLightSettingsPanel.cpp \
    ui/widgets/EffectRoomOutputPanel.cpp \
    ui/widgets/RoomOutputDeviceCard.cpp \
    ui/widgets/EffectStackBlendRow.cpp \
    ui/widgets/EffectCustomHost.cpp \
    ui/widgets/EffectTransportRow.cpp \
    ui/widgets/MediaTextureAmbienceBlock.cpp \
    ui/widgets/AudioEqBandColumn.cpp \
    Effects3D/FolderVolume/FolderVolumeEffect.cpp \
    Effects3D/PlayerEngines.cpp \
    EffectListManager3D.cpp \
    engines/movie-sync/MovieSync.cpp \
    engines/movie-sync/MovieSyncCatalog.cpp \
    engines/movie-sync/MovieSyncListener.cpp \
    engines/movie-sync/MovieSyncRuntime.cpp \
    engines/shader-field/ShaderField.cpp \
    engines/shader-field/SpatialShaderEngine.cpp \
    engines/shader-field/SpatialShaderCatalog.cpp \
    engines/volume-field/SpatialVolumeFieldEngine.cpp \
    Shaders/SpatialOffscreenGlPool.cpp \
    Shaders/FieldFullscreenQuad410.cpp \
    engines/volume-field/SpatialVolumeFieldAssist.cpp \
    Shaders/SpatialStripFieldEngine.cpp \
    Shaders/SpatialStripFieldAssist.cpp \
    engines/reactive/Reactive.cpp \
    engines/reactive/ReactiveKeyMap.cpp \
    engines/reactive/ReactiveInputManager.cpp \
    engines/screen-mirror/ScreenMirror.cpp \
    engines/screen-mirror/ScreenMirror_Render.cpp \
    engines/screen-mirror/ScreenMirror_Settings.cpp \
    engines/screen-mirror/ScreenMirrorMonitorPanel.cpp \
    Effects3D/SpatialPatternKernels/SpatialPatternKernels.cpp \
    Effects3D/SpatialPatternKernels/SpatialPatternPalettes.cpp \
    Audio/AudioInputManager.cpp

win32:CONFIG += QTPLUGIN
win32:LIBS += \
    -lOle32 -lOleAut32 -lAvrt -lMmdevapi -lPropsys -luuid \
    -lgdi32 -luser32 -ld3d11 -ldxgi -ld3dcompiler \
    -lws2_32 -lopengl32 -lWtsapi32

# Assimp (scene-prop mesh import). Default: dependencies/assimp-install from scripts/build-assimp.*.
# Override with ASSIMP_DIR (prefix containing include/ and lib/).
isEmpty(ASSIMP_DIR) {
    ASSIMP_DIR = $$PWD/dependencies/assimp-install
}
INCLUDEPATH += $$ASSIMP_DIR/include
win32 {
    DEFINES += ASSIMP_STATIC
    ASSIMP_LIBDIR = $$ASSIMP_DIR/lib
    exists($$ASSIMP_LIBDIR/assimp-vc143-mt.lib) {
        LIBS += -L$$ASSIMP_LIBDIR -lassimp-vc143-mt
    } else:exists($$ASSIMP_LIBDIR/assimp-vc142-mt.lib) {
        LIBS += -L$$ASSIMP_LIBDIR -lassimp-vc142-mt
    } else:exists($$ASSIMP_LIBDIR/assimp.lib) {
        LIBS += -L$$ASSIMP_LIBDIR -lassimp
    } else {
        warning("Assimp not found under $$ASSIMP_DIR — run scripts/build-assimp.ps1")
    }
    exists($$ASSIMP_LIBDIR/zlibstatic.lib) {
        LIBS += -L$$ASSIMP_LIBDIR -lzlibstatic
    }
}
unix:!macx {
    DEFINES += ASSIMP_STATIC
    # Fedora/CMake often installs to lib64; prefer lib then lib64.
    ASSIMP_LIBDIR = $$ASSIMP_DIR/lib
    !exists($$ASSIMP_LIBDIR/libassimp.a):exists($$ASSIMP_DIR/lib64/libassimp.a) {
        ASSIMP_LIBDIR = $$ASSIMP_DIR/lib64
    }
    LIBS += -L$$ASSIMP_LIBDIR -lassimp -lz
}
macx {
    DEFINES += ASSIMP_STATIC
    LIBS += -L$$ASSIMP_DIR/lib -lassimp -lz
}

win32:CONFIG(debug, debug|release): DESTDIR = debug
win32:CONFIG(release, debug|release): DESTDIR = release
win32:OBJECTS_DIR = _intermediate_$$DESTDIR/.obj
win32:MOC_DIR     = _intermediate_$$DESTDIR/.moc
win32:RCC_DIR     = _intermediate_$$DESTDIR/.qrc
win32:UI_DIR      = _intermediate_$$DESTDIR/.ui
win32:QMAKE_DEL_FILE = cmd /c del /f /q 2^>nul

# Qt Creator warns "Discarding excessive amount of pending output" on Clean because
# qmake emits one enormous per-file `del` of every moc/obj. After each qmake, rewrite
# the Makefile `clean` target to scripts/qt-win-clean.cmd (short directory wipe).
win32 {
    REWRITE_CLEAN_CMD = $$shell_path($$PWD/scripts/rewrite-makefile-clean.cmd)
    QUIET_CLEAN_CMD   = $$shell_path($$PWD/scripts/qt-win-clean.cmd)
    rewrite_win_clean.target = rewrite_win_clean
    rewrite_win_clean.commands = cmd /c $$shell_quote($$REWRITE_CLEAN_CMD) $$shell_quote($$shell_path($$OUT_PWD))
    QMAKE_EXTRA_TARGETS += rewrite_win_clean
    qmake_all.depends += rewrite_win_clean
    quietclean.target = quietclean
    quietclean.commands = cmd /c $$shell_quote($$QUIET_CLEAN_CMD) $$shell_quote($$shell_path($$OUT_PWD))
    QMAKE_EXTRA_TARGETS += quietclean
}

win32:DEFINES += \
    _MBCS \
    WIN32 \
    _CRT_SECURE_NO_WARNINGS \
    _WINSOCK_DEPRECATED_NO_WARNINGS

FORMS += \
    ui/forms/OpenRGB3DSpatialTab.ui \
    ui/forms/GridSettingsPanel.ui \
    ui/forms/SceneTransformPanel.ui \
    ui/forms/SceneObjectSpacingPanel.ui \
    ui/forms/SceneObjectEditHostPanel.ui \
    ui/forms/ObjectCreatorTabPanel.ui \
    ui/forms/ControllerListPanel.ui \
    ui/forms/MediaTextureAmbienceBlock.ui \
    ui/forms/EffectLibraryPanel.ui \
    ui/forms/ActionsPanel.ui \
    ui/forms/TriggersPanel.ui \
    ui/forms/EffectStackPanel.ui \
    ui/forms/ZonesPanel.ui \
    ui/forms/EffectGlobalSettingsPanel.ui \
    ui/forms/AudioInputPanel.ui \
    ui/forms/EffectLayerBanner.ui \
    ui/forms/EffectMotionPanel.ui \
    ui/forms/EffectOutputPanel.ui \
    ui/forms/EffectSurfacesPanel.ui \
    ui/forms/EffectGeometryPanel.ui \
    ui/forms/EffectColorPanel.ui \
    ui/forms/ZoneControllerPickerDialog.ui \
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
    ui/forms/EffectCollapsibleSection.ui \
    ui/forms/RoomSpatialLightSettingsPanel.ui \
    ui/forms/ScreenMirrorCapturePanel.ui \
    ui/forms/ScreenMirrorMonitorSettings.ui \
    ui/forms/ScreenMirrorEffectShell.ui \
    ui/forms/MinecraftGameSettingsScroll.ui

unix:!macx {
    QT += dbus
    # Q_OBJECT helper - only moc on Linux (header is #ifdef Q_OS_LINUX).
    HEADERS += engines/triggers/LinuxLoginWatcher.h
    # GCC 16 + Qt 6 / nlohmann json: false positives from system and bundled headers.
    QMAKE_CXXFLAGS += -Wno-psabi -Wno-array-bounds -Wno-sfinae-incomplete
    target.path = $$PREFIX/lib/openrgb/plugins/
    metainfo.path = $$PREFIX/share/metainfo/
    metainfo.files += flatpak/org.openrgb.OpenRGB.Plugin.Spatial.metainfo.xml
    INSTALLS += target metainfo
    LIBS += -lGL
}

QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.15
macx:LIBS += -framework OpenGL -framework CoreFoundation -framework IOKit

message("VERSION_NUM: "$$VERSION_NUM)
message("VERSION_STR: "$$VERSION_STR)
message("VERSION_DEB: "$$VERSION_DEB)
message("VERSION_RPM: "$$VERSION_RPM)
message("QT_VERSION:  "$$QT_VERSION)
