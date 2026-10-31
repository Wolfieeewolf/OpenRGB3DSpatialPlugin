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
    shared/utils/PluginLog.h

RESOURCES += \
    resources/plugin_ui.qrc

INCLUDEPATH += \
    ui \
    ui/widgets \
    ui/actions \
    ui/effects \
    ui/viewport \
    ui/controllers \
    ui/screen \
    ui/audio \
    ui/shell \
    ui/custom-controller-grid \
    shared \
    shared/effects \
    shared/zones \
    shared/controllers \
    shared/scene \
    shared/utils \
    engines/movie-sync \
    engines/actions \
    engines/triggers \
    engines/folder-volume \
    engines/game \
    shared/SpatialSamplers \
    shared/SpatialRoom \
    shared/SpatialLighting \
    shared/Shaders \
    engines/audio \
    engines/reactive \
    engines/screen-mirror \
    engines/shader-field \
    engines/volume-field \
    engines/particle-field \
    engines/ribbon-field

HEADERS += \
    OpenRGB3DSpatialPlugin.h \
    shared/zones/LEDPosition3D.h \
    shared/controllers/ControllerLayout3D.h \
    shared/controllers/MatrixWiringOrder.h \
    shared/controllers/DeviceTypeUtils.h \
    shared/zones/GridSpaceUtils.h \
    shared/zones/ZoneGrid3D.h \
    shared/effects/SpatialEffectTypes.h \
    shared/effects/SpatialEffect3D.h \
    shared/effects/EffectVolume3D.h \
    shared/effects/EffectListManager3D.h \
    shared/effects/EffectRegisterer3D.h \
    shared/effects/EffectInstance3D.h \
    shared/effects/StackPreset3D.h \
    shared/controllers/VirtualController3D.h \
    shared/scene/VirtualReferencePoint3D.h \
    shared/zones/Zone3D.h \
    shared/zones/ZoneManager3D.h \
    shared/scene/DisplayPlane3D.h \
    shared/scene/SceneProp3D.h \
    shared/scene/DisplayPlaneManager.h \
    shared/scene/ScenePropManager.h \
    engines/screen-mirror/ScreenCaptureManager.h \
    engines/screen-mirror/ScreenCaptureDownscale.h \
    engines/screen-mirror/DisplayPlaneCaptureLabels.h \
    shared/zones/Geometry3DUtils.h \
    shared/utils/TransformJson.h \
    shared/utils/MediaTextureEffectUtils.h \
    shared/StripPatternSurface.h \
    shared/utils/QtCompat.h \
    ui/widgets/GameTelemetryStatusPanel.h \
    engines/game/Game/GameTelemetryBridge.h \
    engines/game/Game/RoomSampleFrameProtocol.h \
    engines/game/Game/RoomSampleShmPaths.h \
    engines/game/Game/RoomSampleFrameShmReader.h \
    engines/game/Game/RoomSampleConfigPublisher.h \
    shared/SpatialSamplers/SpatialBasisUtils.h \
    shared/SpatialSamplers/SpatialLayerCore.h \
    shared/SpatialSamplers/RoomSampleMapping.h \
    engines/game/Minecraft/MinecraftGame.h \
    engines/game/Minecraft/MinecraftGameSettings.h \
    engines/game/Minecraft/MinecraftSubEffect3D.h \
    engines/game/Minecraft/MinecraftHealth/MinecraftHealthEffect3D.h \
    engines/game/Minecraft/MinecraftHunger/MinecraftHungerEffect3D.h \
    engines/game/Minecraft/MinecraftAir/MinecraftAirEffect3D.h \
    engines/game/Minecraft/MinecraftDurability/MinecraftDurabilityEffect3D.h \
    engines/game/Minecraft/MinecraftDamage/MinecraftDamageEffect3D.h \
    engines/game/Minecraft/MinecraftRoomAmbilight/MinecraftRoomAmbilightEffect3D.h \
    engines/actions/Action.h \
    engines/actions/ActionMedia.h \
    engines/actions/ActionPlayer.h \
    engines/actions/ActionApplier.h \
    engines/actions/ActionApplierDetail.h \
    engines/actions/ActionClipEval.h \
    engines/actions/ActionDetail.h \
    engines/actions/ActionScript.h \
    engines/volume-field/ActionVolumeField.h \
    engines/particle-field/ActionParticleField.h \
    engines/ribbon-field/ActionRibbonField.h \
    engines/actions/ActionBuiltinGpu.h \
    engines/actions/ActionLibrary.h \
    engines/triggers/TriggerBinding.h \
    engines/triggers/TriggerSource.h \
    engines/triggers/ManualTriggerSource.h \
    engines/triggers/WindowsTriggerSource.h \
    engines/triggers/LinuxTriggerSource.h \
    engines/triggers/MacTriggerSource.h \
    engines/triggers/TriggerSourceRegistry.h \
    engines/triggers/TriggerRuntime.h \
    shared/SpatialRoom/SpatialRoomTypes.h \
    shared/SpatialRoom/SpatialRoomFrame.h \
    shared/SpatialLighting/BlockerGridOccluder.h \
    shared/SpatialLighting/OccluderSpatialIndex.h \
    shared/SpatialLighting/SpatialLightingEngine.h \
    shared/SpatialLighting/EmitterRelayMirror.h \
    shared/SpatialLighting/EmitterLocalSampling.h \
    shared/SpatialLighting/SpatialLightingSceneProvider.h \
    shared/SpatialLighting/RoomSpatialLightingUi.h \
    ui/shell/PluginSettingsPaths.h \
    ui/shell/GridSettingsPanel.h \
    ui/viewport/SceneTransformPanel.h \
    ui/viewport/SceneObjectSpacingPanel.h \
    ui/viewport/SceneObjectEditHostPanel.h \
    ui/viewport/PositionAxisDragController.h \
    ui/shell/ObjectCreatorTabPanel.h \
    ui/controllers/ControllerListPanel.h \
    ui/effects/EffectLibraryPanel.h \
    ui/actions/ActionsPanel.h \
    ui/actions/ActionEditorDialog.h \
    ui/actions/ActionTimelineWidget.h \
    ui/actions/ActionGradientBar.h \
    ui/actions/ActionCurveBar.h \
    ui/actions/ActionToolBar.h \
    ui/actions/ActionCatalog.h \
    ui/actions/ActionUserCurves.h \
    ui/actions/ActionUserGradients.h \
    ui/actions/TriggersPanel.h \
    ui/effects/EffectStackPanel.h \
    ui/shell/ZonesPanel.h \
    ui/effects/EffectGlobalSettingsPanel.h \
    ui/audio/AudioInputPanel.h \
    ui/audio/AudioAnalyzerSettings.h \
    ui/effects/EffectControlsHostPanel.h \
    ui/shell/OpenRGB3DSpatialTab.h \
    ui/shell/SpatialTabLedHelpers.h \
    ui/controllers/ControllerDisplayUtils.h \
    ui/shell/TooltipProxy.h \
    ui/viewport/LEDViewport3D.h \
    ui/viewport/LEDViewport3D_Internal.h \
    ui/controllers/ZoneControllerPickerDialog.h \
    ui/controllers/CustomControllerTypes.h \
    ui/controllers/CustomControllerMappingUtils.h \
    ui/controllers/CustomControllerGridKeys.h \
    ui/controllers/CustomControllerClipboard.h \
    ui/controllers/CustomControllerHistory.h \
    ui/controllers/CustomControllerDialog.h \
    ui/controllers/CustomControllerDialog_Internal.h \
    ui/shell/ReferencePointDialog.h \
    ui/screen/DisplayPlaneDialog.h \
    ui/viewport/ScenePropDialog.h \
    ui/viewport/ScenePropPreviewWidget.h \
    ui/screen/DisplayPlaneCaptureCombo.h \
    ui/screen/DisplayPlaneCaptureUi.h \
    ui/custom-controller-grid/CustomControllerGridCell.h \
    ui/custom-controller-grid/CustomControllerGridLayoutMath.h \
    ui/custom-controller-grid/CustomControllerGridItem.h \
    ui/custom-controller-grid/CustomControllerGridScene.h \
    ui/custom-controller-grid/CustomControllerLayoutGrid.h \
    ui/controllers/CustomControllerSourceRef.h \
    ui/controllers/CustomControllerDeviceList.h \
    ui/controllers/SpatialControllerListBacking.h \
    ui/controllers/SpatialControllerEntryKey.h \
    ui/controllers/SpatialControllerCardWidget.h \
    ui/controllers/SpatialControllerCardList.h \
    ui/controllers/CustomControllerDeviceWidget.h \
    ui/controllers/CustomControllerPreviewDialog.h \
    ui/shell/OpenRGBPluginsFont.h \
    ui/shell/PluginClickableLabel.h \
    ui/viewport/Gizmo3D.h \
    ui/viewport/ViewportMath.h \
    ui/viewport/ViewportGLFormat.h \
    ui/viewport/GlProgram.h \
    ui/viewport/MeshBatch.h \
    ui/viewport/MeshGeometry.h \
    ui/viewport/MeshImport.h \
    ui/viewport/ScenePropMeshCache.h \
    ui/viewport/ScenePropMeshPaths.h \
    ui/viewport/ViewportShaders.h \
    ui/screen/CaptureZonesWidget.h \
    ui/shell/PluginUiUtils.h \
    shared/EffectStratumBlend.h \
    shared/EffectColorUtils.h \
    shared/EffectUiSync.h \
    engines/audio/AudioReactiveCommon.h \
    shared/SpatialKernelColormap.h \
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
    ui/widgets/EffectDoubleSliderRow.h \
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
    shared/PlayerEngines.h \
    engines/movie-sync/MovieSync.h \
    engines/movie-sync/MovieSyncCatalog.h \
    engines/movie-sync/MovieSyncListener.h \
    engines/movie-sync/MovieSyncRuntime.h \
    shared/effects/EffectListManager3D.h \
    engines/shader-field/SpatialShaderEngine.h \
    engines/shader-field/SpatialShaderUniforms.h \
    engines/shader-field/SpatialShaderCatalog.h \
    shared/Shaders/GlslUniformArray.h \
    shared/Shaders/SpatialFieldAssistBase.h \
    engines/volume-field/SpatialVolumeFieldEngine.h \
    shared/Shaders/SpatialOffscreenGlPool.h \
    shared/Shaders/FieldFullscreenQuad410.h \
    engines/volume-field/SpatialVolumeFieldAssist.h \
    shared/Shaders/SpatialStripFieldEngine.h \
    shared/Shaders/SpatialStripFieldAssist.h \
    engines/audio/AudioReactiveUi.h \
    engines/audio/AudioInputManager.h \
    engines/reactive/ReactiveInputTypes.h \
    engines/reactive/ReactiveKeyMap.h \
    engines/reactive/ReactiveInputManager.h \
    engines/reactive/Reactive.h \
    engines/folder-volume/FolderVolumeEffect.h \
    engines/screen-mirror/ScreenMirror.h \
    engines/screen-mirror/ScreenMirror_Internal.h \
    engines/screen-mirror/ScreenMirrorWaveMath.h \
    engines/screen-mirror/ScreenMirrorCalibrationPattern.h \
    engines/screen-mirror/ScreenMirrorMonitorPanel.h \
    shared/SpatialPatternKernels/SpatialPatternKernels.h \
    shared/SpatialPatternKernels/SpatialPatternPalettes.h \

SOURCES += \
    OpenRGB3DSpatialPlugin.cpp \
    shared/controllers/ControllerLayout3D.cpp \
    shared/controllers/MatrixWiringOrder.cpp \
    shared/zones/GridSpaceUtils.cpp \
    shared/zones/ZoneGrid3D.cpp \
    shared/effects/SpatialEffect3D.cpp \
    shared/effects/SpatialEffect3D_Eval.cpp \
    shared/effects/SpatialEffect3D_Settings.cpp \
    shared/effects/EffectVolume3D.cpp \
    shared/effects/EffectInstance3D.cpp \
    shared/effects/StackPreset3D.cpp \
    shared/controllers/VirtualController3D.cpp \
    shared/scene/VirtualReferencePoint3D.cpp \
    shared/zones/Zone3D.cpp \
    shared/zones/ZoneManager3D.cpp \
    shared/scene/DisplayPlane3D.cpp \
    shared/scene/SceneProp3D.cpp \
    engines/screen-mirror/ScreenCaptureManager.cpp \
    ui/widgets/GameTelemetryStatusPanel.cpp \
    engines/game/Game/GameTelemetryBridge.cpp \
    engines/game/Game/RoomSampleFrameShmReader.cpp \
    engines/game/Game/RoomSampleConfigPublisher.cpp \
    shared/SpatialSamplers/SpatialLayerCore.cpp \
    shared/SpatialSamplers/RoomSampleMapping.cpp \
    engines/game/Minecraft/MinecraftGame.cpp \
    engines/game/Minecraft/MinecraftGameSettings.cpp \
    engines/game/Minecraft/MinecraftSubEffect3D.cpp \
    engines/game/Minecraft/MinecraftHealth/MinecraftHealthEffect3D.cpp \
    engines/game/Minecraft/MinecraftHunger/MinecraftHungerEffect3D.cpp \
    engines/game/Minecraft/MinecraftAir/MinecraftAirEffect3D.cpp \
    engines/game/Minecraft/MinecraftDurability/MinecraftDurabilityEffect3D.cpp \
    engines/game/Minecraft/MinecraftDamage/MinecraftDamageEffect3D.cpp \
    engines/game/Minecraft/MinecraftRoomAmbilight/MinecraftRoomAmbilightEffect3D.cpp \
    engines/actions/Action.cpp \
    engines/actions/ActionMedia.cpp \
    engines/actions/ActionSerialize.cpp \
    engines/actions/ActionClipEvalAxis.cpp \
    engines/actions/ActionSpatial.cpp \
    engines/actions/ActionScript.cpp \
    engines/volume-field/ActionVolumeField.cpp \
    engines/particle-field/ActionParticleField.cpp \
    engines/ribbon-field/ActionRibbonField.cpp \
    engines/actions/ActionBuiltinGpu.cpp \
    engines/actions/ActionApplier.cpp \
    engines/actions/ActionApplierMatch.cpp \
    engines/actions/ActionApplierSpatial.cpp \
    engines/actions/ActionLibrary.cpp \
    engines/triggers/TriggerBinding.cpp \
    engines/triggers/WindowsTriggerSource.cpp \
    engines/triggers/LinuxTriggerSource.cpp \
    engines/triggers/MacTriggerSource.cpp \
    engines/triggers/TriggerSourceRegistry.cpp \
    engines/triggers/TriggerRuntime.cpp \
    shared/SpatialRoom/SpatialRoomFrame.cpp \
    shared/SpatialLighting/BlockerGridOccluder.cpp \
    shared/SpatialLighting/OccluderSpatialIndex.cpp \
    shared/SpatialLighting/SpatialLightingEngine.cpp \
    shared/SpatialLighting/EmitterRelayMirror.cpp \
    shared/SpatialLighting/EmitterLocalSampling.cpp \
    shared/SpatialLighting/SpatialLightingSceneProvider.cpp \
    shared/SpatialLighting/RoomSpatialLightingUi.cpp \
    ui/shell/PluginSettingsPaths.cpp \
    ui/shell/GridSettingsPanel.cpp \
    ui/viewport/SceneTransformPanel.cpp \
    ui/viewport/SceneObjectSpacingPanel.cpp \
    ui/viewport/SceneObjectEditHostPanel.cpp \
    ui/viewport/PositionAxisDragController.cpp \
    ui/shell/ObjectCreatorTabPanel.cpp \
    ui/controllers/ControllerListPanel.cpp \
    ui/effects/EffectLibraryPanel.cpp \
    ui/actions/ActionsPanel.cpp \
    ui/actions/TriggersPanel.cpp \
    ui/actions/ActionEditorDialog.cpp \
    ui/actions/ActionEditorProps.cpp \
    ui/actions/ActionEditorTimelineBridge.cpp \
    ui/actions/ActionTimelineWidget.cpp \
    ui/actions/ActionTimelinePaint.cpp \
    ui/actions/ActionTimelineInteract.cpp \
    ui/actions/ActionGradientBar.cpp \
    ui/actions/ActionCurveBar.cpp \
    ui/actions/ActionUserGradients.cpp \
    ui/actions/ActionEffectFiles.cpp \
    ui/actions/ActionUserCurves.cpp \
    ui/actions/ActionToolBar.cpp \
    ui/effects/EffectStackPanel.cpp \
    ui/shell/ZonesPanel.cpp \
    ui/effects/EffectGlobalSettingsPanel.cpp \
    ui/audio/AudioInputPanel.cpp \
    ui/audio/AudioAnalyzerSettings.cpp \
    ui/effects/EffectControlsHostPanel.cpp \
    ui/shell/OpenRGB3DSpatialTab.cpp \
    ui/shell/OpenRGB3DSpatialTab_Audio.cpp \
    ui/shell/OpenRGB3DSpatialTab_Setup.cpp \
    ui/shell/SpatialTabLedHelpers.cpp \
    ui/shell/OpenRGB3DSpatialTab_SetupDisplayPlanes.cpp \
    ui/shell/OpenRGB3DSpatialTab_SetupSceneProps.cpp \
    ui/shell/OpenRGB3DSpatialTab_Layout.cpp \
    ui/shell/OpenRGB3DSpatialTab_LayoutCustomControllers.cpp \
    ui/shell/OpenRGB3DSpatialTab_Scene.cpp \
    ui/controllers/ZoneControllerPickerDialog.cpp \
    ui/shell/OpenRGB3DSpatialTab_Effects.cpp \
    ui/shell/OpenRGB3DSpatialTab_EffectsRender.cpp \
    ui/viewport/LEDViewport3D.cpp \
    ui/viewport/LEDViewport3D_Input.cpp \
    ui/viewport/LEDViewport3D_Camera.cpp \
    ui/viewport/LEDViewport3D_Draw.cpp \
    ui/viewport/LEDViewport3D_Controllers.cpp \
    ui/viewport/LEDViewport3D_SceneObjects.cpp \
    ui/viewport/LEDViewport3D_DisplayPlanes.cpp \
    ui/viewport/LEDViewport3D_SceneProps.cpp \
    ui/controllers/CustomControllerDialog.cpp \
    ui/controllers/CustomControllerDialog_Grid.cpp \
    ui/controllers/CustomControllerDialog_Sources.cpp \
    ui/controllers/CustomControllerDialog_Transform.cpp \
    ui/controllers/CustomControllerMappingUtils.cpp \
    ui/shell/ReferencePointDialog.cpp \
    ui/screen/DisplayPlaneDialog.cpp \
    ui/viewport/ScenePropDialog.cpp \
    ui/viewport/ScenePropPreviewWidget.cpp \
    ui/screen/DisplayPlaneCaptureCombo.cpp \
    ui/custom-controller-grid/CustomControllerGridItem.cpp \
    ui/custom-controller-grid/CustomControllerGridScene.cpp \
    ui/custom-controller-grid/CustomControllerLayoutGrid.cpp \
    ui/controllers/ControllerCards.cpp \
    ui/controllers/ControllerDisplayUtils.cpp \
    ui/controllers/CustomControllerWidgets.cpp \
    ui/controllers/CustomControllerPreviewDialog.cpp \
    ui/shell/OpenRGBPluginsFont.cpp \
    ui/shell/PluginClickableLabel.cpp \
    ui/viewport/Gizmo3D.cpp \
    ui/viewport/Gizmo3D_Mesh.cpp \
    ui/viewport/ViewportMath.cpp \
    ui/viewport/ViewportGLFormat.cpp \
    ui/viewport/GlProgram.cpp \
    ui/viewport/MeshBatch.cpp \
    ui/viewport/MeshGeometry.cpp \
    ui/viewport/MeshImport.cpp \
    ui/viewport/ViewportShaders.cpp \
    ui/screen/CaptureZonesWidget.cpp \
    ui/widgets/StratumBandPanel.cpp \
    ui/widgets/StripKernelColormapPanel.cpp \
    ui/widgets/EffectCommonPanels.cpp \
    ui/widgets/EffectRowWidgets.cpp \
    ui/widgets/RoomSpatialLightSettingsPanel.cpp \
    ui/widgets/EffectRoomOutputPanel.cpp \
    ui/widgets/RoomOutputDeviceCard.cpp \
    ui/widgets/EffectStackBlendRow.cpp \
    ui/widgets/EffectTransportRow.cpp \
    ui/widgets/MediaTextureAmbienceBlock.cpp \
    ui/widgets/AudioEqBandColumn.cpp \
    engines/folder-volume/FolderVolumeEffect.cpp \
    engines/folder-volume/FolderVolumeEffect_UI.cpp \
    engines/folder-volume/FolderVolumeSpec.cpp \
    shared/PlayerEngines.cpp \
    shared/effects/EffectListManager3D.cpp \
    engines/movie-sync/MovieSync.cpp \
    engines/movie-sync/MovieSyncCatalog.cpp \
    engines/movie-sync/MovieSyncListener.cpp \
    engines/movie-sync/MovieSyncRuntime.cpp \
    engines/shader-field/ShaderField.cpp \
    engines/shader-field/SpatialShaderEngine.cpp \
    engines/shader-field/SpatialShaderCatalog.cpp \
    engines/volume-field/SpatialVolumeFieldEngine.cpp \
    shared/Shaders/SpatialOffscreenGlPool.cpp \
    shared/Shaders/FieldFullscreenQuad410.cpp \
    engines/volume-field/SpatialVolumeFieldAssist.cpp \
    shared/Shaders/SpatialStripFieldEngine.cpp \
    shared/Shaders/SpatialStripFieldAssist.cpp \
    engines/reactive/Reactive.cpp \
    engines/reactive/Reactive_UI.cpp \
    engines/reactive/ReactiveKeyMap.cpp \
    engines/reactive/ReactiveInputManager.cpp \
    engines/screen-mirror/ScreenMirror.cpp \
    engines/screen-mirror/ScreenMirror_Render.cpp \
    engines/screen-mirror/ScreenMirror_Settings.cpp \
    engines/screen-mirror/ScreenMirrorMonitorPanel.cpp \
    shared/SpatialPatternKernels/SpatialPatternKernels.cpp \
    shared/SpatialPatternKernels/SpatialPatternPalettes.cpp \
    engines/audio/AudioInputManager.cpp \
    engines/audio/AudioInputManager_Wasapi.cpp

win32:CONFIG += QTPLUGIN
win32:LIBS += \
    -lOle32 -lOleAut32 -lAvrt -lMmdevapi -lPropsys -luuid \
    -lgdi32 -luser32 -ld3d11 -ldxgi -ld3dcompiler \
    -lws2_32 -lopengl32 -lWtsapi32

# Assimp (scene-prop mesh import). Default: build into $$OUT_PWD/_deps on first link
# via scripts/build-assimp.* (PRE_TARGETDEPS). Override ASSIMP_DIR to use a system prefix
# (skips auto-build). Scripts stabilize Windows link name as assimp.lib.
isEmpty(ASSIMP_DIR) {
    ASSIMP_DIR = $$OUT_PWD/_deps/assimp-install
    ASSIMP_AUTO_BUILD = 1
}
ASSIMP_BUILD_DIR = $$OUT_PWD/_deps/assimp-build
ASSIMP_LIBDIR = $$ASSIMP_DIR/lib
INCLUDEPATH += $$ASSIMP_DIR/include
DEFINES += ASSIMP_STATIC

win32 {
    LIBS += -L$$ASSIMP_LIBDIR -lassimp
    equals(ASSIMP_AUTO_BUILD, 1) {
        LIBS += -lzlibstatic
    } else:exists($$ASSIMP_LIBDIR/zlibstatic.lib) {
        LIBS += -lzlibstatic
    }
}
unix:!macx {
    LIBS += -L$$ASSIMP_LIBDIR -lassimp -lz
}
macx {
    LIBS += -L$$ASSIMP_LIBDIR -lassimp -lz
}

equals(ASSIMP_AUTO_BUILD, 1) {
    !exists($$PWD/dependencies/assimp/CMakeLists.txt) {
        error("Assimp submodule missing — run: git submodule update --init dependencies/assimp")
    }
    win32 {
        ASSIMP_LIB_FILE = $$ASSIMP_LIBDIR/assimp.lib
        ensure_assimp.target = $$ASSIMP_LIB_FILE
        ensure_assimp.commands = powershell -NoProfile -ExecutionPolicy Bypass -File $$shell_quote($$shell_path($$PWD/scripts/build-assimp.ps1)) -BuildDir $$shell_quote($$shell_path($$ASSIMP_BUILD_DIR)) -InstallDir $$shell_quote($$shell_path($$ASSIMP_DIR))
        ensure_assimp.depends = $$PWD/dependencies/assimp/CMakeLists.txt
        QMAKE_EXTRA_TARGETS += ensure_assimp
        PRE_TARGETDEPS += $$ASSIMP_LIB_FILE
    } else {
        ASSIMP_LIB_FILE = $$ASSIMP_LIBDIR/libassimp.a
        ensure_assimp.target = $$ASSIMP_LIB_FILE
        ensure_assimp.commands = \
            ASSIMP_BUILD_DIR=$$shell_quote($$ASSIMP_BUILD_DIR) \
            ASSIMP_INSTALL_DIR=$$shell_quote($$ASSIMP_DIR) \
            $$shell_quote($$PWD/scripts/build-assimp.sh)
        ensure_assimp.depends = $$PWD/dependencies/assimp/CMakeLists.txt
        QMAKE_EXTRA_TARGETS += ensure_assimp
        PRE_TARGETDEPS += $$ASSIMP_LIB_FILE
    }
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
    ui/forms/EffectDoubleSliderRow.ui \
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
