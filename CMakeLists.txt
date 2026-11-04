cmake_minimum_required(VERSION 3.21)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" KA_HGIS_VERSION_FILE LIMIT_COUNT 1)
string(STRIP "${KA_HGIS_VERSION_FILE}" KA_HGIS_VERSION)
if(NOT KA_HGIS_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
  message(FATAL_ERROR "VERSION must be major.minor.patch, got '${KA_HGIS_VERSION}'")
endif()
project(ka-hgis VERSION ${KA_HGIS_VERSION} LANGUAGES CXX)
find_package(Git QUIET)
set(KA_HGIS_GIT_HASH "unknown")
if(GIT_FOUND)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-parse --short=12 HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE KA_HGIS_GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
endif()
if(NOT KA_HGIS_GIT_HASH MATCHES "^[0-9a-fA-F]+$")
  set(KA_HGIS_GIT_HASH "unknown")
endif()
# One generated file carries the hash (src/core/KaGitHash.h): configure_file rewrites it only when
# the hash changes, so a commit recompiles this file instead of every file of ka_core and the tests.
set(KA_HGIS_GIT_HASH_CPP "${CMAKE_BINARY_DIR}/generated/KaGitHash.cpp")
configure_file("${CMAKE_SOURCE_DIR}/cmake/KaGitHash.cpp.in" "${KA_HGIS_GIT_HASH_CPP}" @ONLY)
# F138: the hash above is read at configure time. Re-run configure when HEAD or the
# checked-out branch moves, so a rebuilt EXE never carries an older commit's hash.
if(GIT_FOUND)
  set(_ka_git_watch HEAD packed-refs)
  execute_process(COMMAND "${GIT_EXECUTABLE}" symbolic-ref -q HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" OUTPUT_VARIABLE _ka_git_branch
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(_ka_git_branch)
    list(APPEND _ka_git_watch "${_ka_git_branch}")
  endif()
  foreach(_ka_git_ref IN LISTS _ka_git_watch)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --git-path "${_ka_git_ref}"
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" OUTPUT_VARIABLE _ka_git_file
      OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(_ka_git_file)
      get_filename_component(_ka_git_file "${_ka_git_file}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
      if(EXISTS "${_ka_git_file}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_ka_git_file}")
      endif()
    endif()
  endforeach()
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if (MSVC)
  add_compile_options(/MP)
  # /Zc:__cplusplus : 없으면 CMAKE_CXX_STANDARD 20 을 줘도 __cplusplus 가 199711(C++98)로 보고되어
  #                   라이브러리들의 C++20 기능 검출이 조용히 실패한다. (이 머신에서 실측 확인)
  # /utf-8          : 소스/실행 문자셋 UTF-8 고정 (기존에 타깃마다 흩어져 있던 것을 여기서 일괄 적용)
  add_compile_options(/Zc:__cplusplus /utf-8 /permissive-)
  # 경고 수준 상향. 자체 코드는 따옴표 include, Qt/QGIS 는 꺾쇠 include 이므로
  # /external:anglebrackets 로 외부 헤더 경고만 차단하고 우리 코드에는 /W4 를 적용한다.
  add_compile_options(/W4 /external:anglebrackets /external:W0)
endif()

# LTO(링크 타임 최적화): 번역 단위 경계를 넘어 인라이닝한다. Release 에서만.
# check_ipo_supported() 는 build/CMakeFiles 에 임시 테스트 프로젝트를 남겨
# VS 제너레이터의 ALL_BUILD 를 깨뜨리므로(MSB3202) 쓰지 않는다.
# MSVC: LTO 는 끈다. /GL + /LTCG 빌드에서 c2.dll 내부 오류(ICE)가 났다. 다시 켜려면 링크 시간·EXE 크기를 재고, 먼저 /Gw·/OPT:ICF·PCH 효과부터 본다(F208).
if(MSVC)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE OFF)
  message(STATUS "LTO: MSVC c2.dll 내부 오류 회피를 위해 OFF")
else()
  include(CheckIPOSupported)
  check_ipo_supported(RESULT KA_IPO_OK OUTPUT KA_IPO_MSG)
  if(KA_IPO_OK)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
  else()
    message(STATUS "LTO 미지원: ${KA_IPO_MSG}")
  endif()
endif()

option(KA_HGIS_BUILD_TESTS "Build tests" ON)
if (EXISTS "C:/OSGeo4W")
  set(OSGEO4W_ROOT_DEFAULT "C:/OSGeo4W")
elseif (EXISTS "D:/OSGeo4W")
  set(OSGEO4W_ROOT_DEFAULT "D:/OSGeo4W")
elseif (EXISTS "A:/OSGeo4W")
  set(OSGEO4W_ROOT_DEFAULT "A:/OSGeo4W")
endif()
set(OSGEO4W_ROOT "${OSGEO4W_ROOT_DEFAULT}" CACHE PATH "OSGeo4W root")
# Keep the prefix under the selected root. A stale cache still pointed tests
# at another PC's qgis-dev, so the WMS provider never loaded.
set(QGIS_PREFIX_PATH "${OSGEO4W_ROOT}/apps/qgis-dev" CACHE PATH "QGIS prefix" FORCE)
list(PREPEND CMAKE_PREFIX_PATH "${OSGEO4W_ROOT}/apps/Qt6" "${OSGEO4W_ROOT}/apps/Qt6/lib/cmake")
# Pdf: 인쇄는 저장한 도면 PDF를 잘라 찍는다(TilePrint). Qt6Pdf.dll 은 OSGeo4W Qt 에 이미 있다.
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Xml PrintSupport Pdf Network Svg Test Multimedia MultimediaWidgets)
find_package(Qt6 REQUIRED COMPONENTS WebEngineWidgets)

set(KA_INCLUDES
  ${CMAKE_SOURCE_DIR}/src
  ${QGIS_PREFIX_PATH}/include
  ${OSGEO4W_ROOT}/apps/gdal-dev/include
  ${OSGEO4W_ROOT}/include
)
set(KA_LIBDIRS
  ${QGIS_PREFIX_PATH}/lib
  ${OSGEO4W_ROOT}/apps/gdal-dev/lib
  ${OSGEO4W_ROOT}/lib
)

add_library(ka_core STATIC
  ${KA_HGIS_GIT_HASH_CPP}
  src/core/KaGitHash.h
  src/core/LayerLabelControls.cpp
  src/core/CadastralImport.cpp
  src/core/ParcelClipboard.cpp
  src/core/CadastralPortal.cpp
  data/topographic/topographic.qrc
  src/core/TopographicArchive.cpp
  src/core/TopographicSheets.cpp
  src/core/TopographicSourceCrs.cpp
  src/core/TopographicCatalog.cpp
  src/core/TopographicShapefileCache.cpp
  src/core/TopographicSettings.cpp
  src/core/ChecklistEngine.cpp
  src/core/SurveyProjectFactory.cpp
  src/core/FileCleanup.cpp
  src/core/SurveyHandles.cpp
  src/core/SurveySaveAs.cpp
  src/core/SurveyStorage.cpp
  src/core/SurveyBundle.cpp
  src/core/SurveySession.cpp
  src/core/SurveyDurability.cpp
  src/core/SurveyFileFingerprint.cpp
  src/core/SurveyFileHygiene.cpp
  src/core/SurveyRecovery.cpp
  src/core/PreparedReferenceMap.cpp
  src/core/ReferenceTiledFetch.cpp
  src/core/ReferenceStorage.cpp
  src/core/ReferenceInventory.cpp
  src/core/ReferenceSheetSnapshot.cpp
  src/core/DemDownloadService.cpp
  src/core/ExportService.cpp
  src/core/MapGeoTiffExport.cpp
  src/core/ProjectStateBuilder.cpp
  src/core/ChecklistStateGeometry.cpp
  src/core/ChecklistStateFields.cpp
  src/core/ChecklistStateSheets.cpp
  src/core/SubmitShp.cpp
  src/core/SubmitReadme.cpp
  src/core/LayoutService.cpp
  src/core/LayerOps.cpp
  src/core/FeaturePick.cpp
  src/core/PolygonErase.cpp
  src/core/BasemapOps.cpp
  src/core/LabelOps.cpp
  src/core/LayerTreeLabelStack.cpp
  src/core/LayerRole.cpp
  src/core/LayerStyleDefaults.cpp
  src/core/LayerStyleKinds.cpp
  src/core/LayerStyleOpacity.cpp
  src/core/LayerTreeRecovery.cpp
  src/core/LayerTreePolicy.cpp
  src/core/GeometryEditMerge.cpp
  src/core/PolygonPieces.cpp
  src/core/GeometryEditVertex.cpp
  src/core/ControlPointCsv.cpp
  src/core/SurveyPointSchema.cpp
  src/core/SurveyPointReader.cpp
  src/core/SurveyContourBuilder.cpp
  src/core/SurveyContourWrite.cpp
  src/core/SurveyContourClip.cpp
  src/core/SurveyContourStyle.cpp
  src/core/SurveyPointArrange.cpp
  src/core/SurveyContourTin.cpp
  src/core/KaPortableRuntime.cpp
  src/core/DemColorRampLegend.cpp
  src/core/DemPresentation.cpp
  src/core/LocationSearch.cpp
  src/core/AdminBoundaryService.cpp
  src/core/KoreaRegionCatalog.cpp
  src/core/HeritageStyle.cpp
  src/core/HeritageSiteLegend.cpp
  src/core/HeritageLayoutNumbers.cpp
  src/core/HeritageRegionResolver.cpp
  src/core/HeritageIntranetSettings.cpp
  src/core/KaSecretStore.cpp
  src/core/KaSessionLog.cpp
  src/core/HeritageIntranetFlow.cpp
  src/core/HeritageImport.cpp
  src/core/SurveyScopeClip.cpp
  src/core/HeritageFormParser.cpp
  src/core/HeritageHttpClient.cpp
  src/core/HeritageFetchPlan.cpp
  src/core/HeritageNearbyRegions.cpp
  src/core/HeritageRecentDownloads.cpp
  src/core/HeritagePledge.cpp
  src/core/AccountStatus.cpp
  src/core/CadastralEncoding.cpp
  src/core/WorkflowGuide.cpp
  src/core/VworldSettings.cpp
  src/core/GeorefService.cpp
  src/core/GeorefBackup.cpp
  src/core/GeorefPixelWindow.cpp
  src/core/GeorefQuality.cpp
  src/core/FeaturePresets.cpp
  src/core/FeatureRecordStyle.cpp
  src/core/FeatureNumbering.cpp
  src/core/FeatureRecord.cpp
  src/core/SurveySchema.cpp
  src/core/BufferAnalysis.cpp
  src/core/NearbyHeritageExport.cpp
  src/core/RecentSurveys.cpp
  src/core/KaSafeQgis.cpp
  src/core/MeasureOps.cpp
  src/core/EditGeometryRepair.cpp
  src/core/EditHistory.cpp
  src/core/DemAnalyzer.cpp
  src/core/DemHillshadeFile.cpp
  src/core/TrenchGridGenerator.cpp
  src/core/TrenchLShape.cpp
  src/core/TrenchGridSnap.cpp
  src/core/TrenchTerrain.cpp
  src/core/TrenchLayerEdit.cpp
  src/core/TrenchPlanCache.cpp
  src/core/TilePackService.cpp
  src/core/SoilMapService.cpp
  src/core/PaleoLandformService.cpp
  src/core/GeoInfoBoreholeService.cpp
  src/core/GeoInfoBoreholeParser.cpp
  src/core/KigamPaleoService.cpp
  src/core/KigamPaleoLayer.cpp
  src/core/PaleoDemClassifier.cpp
  src/core/GeologyMapService.cpp
  src/core/RiverMapService.cpp
  src/core/Terrain3dService.cpp
  src/core/Terrain3dLayoutService.cpp
  src/core/TilePrint.cpp
  src/core/PdfExportSettings.cpp
  src/core/StandardScales.cpp
  src/core/LayoutBadgePlacer.cpp
  src/core/CadDwgConverter.cpp
  src/core/CadDrawingReader.cpp
  src/core/CadCrsGuess.cpp
  src/core/CadCrsHints.cpp
  src/core/CadPendingCopies.cpp
  src/core/LineMapStyle.cpp
  src/core/CadDrawingStore.cpp
  src/core/CadDrawingLayers.cpp
  src/core/SnapTraceOps.cpp
  src/core/ChecklistEngine.h
  src/core/SurveyProjectFactory.h
  src/core/FileCleanup.h
  src/core/SurveyHandles.h
  src/core/SurveySaveAs.h
  src/core/SurveyStorage.h
  src/core/SurveySession.h
  src/core/SurveyDurability.h
  src/core/SurveyFileFingerprint.h
  src/core/SurveyFileHygiene.h
  src/core/SurveyRecovery.h
  src/core/ExportService.h
  src/core/ProjectStateBuilder.h
  src/core/ChecklistStateChecks.h
  src/core/SubmitShp.h
  src/core/SubmitReadme.h
  src/core/LayoutService.h
  src/core/LayerFeatures.h
  src/core/LayerOps.h
  src/core/LayerRole.h
  src/core/LayerStyleDefaults.h
  src/core/LayerStyleKinds.h
  src/core/LayerTreeRecovery.h
  src/core/LayerTreePolicy.h
  src/core/GeometryEditOps.h
  src/core/FeaturePick.h
  src/core/PolygonErase.h
  src/core/KaPortableRuntime.h
  src/core/KaSessionLog.h
  src/core/LocationSearch.h
  src/core/AdminBoundaryService.h
  src/core/KoreaRegionCatalog.h
  src/core/HeritageStyle.h
  src/core/HeritageSiteLegend.h
  src/core/HeritageLayoutNumbers.h
  src/core/HeritageRegionResolver.h
  src/core/HeritageIntranetSettings.h
  src/core/HeritageIntranetFlow.h
  src/core/HeritageImport.h
  src/core/SurveyScopeClip.h
  src/core/HeritageFormParser.h
  src/core/HeritageHttpClient.h
  src/core/HeritageFetchPlan.h
  src/core/HeritageNearbyRegions.h
  src/core/HeritageRecentDownloads.h
  src/core/HeritagePledge.h
  src/core/AccountStatus.h
  src/core/CadastralEncoding.h
  src/core/WorkflowGuide.h
  src/core/VworldSettings.h
  src/core/GeorefService.h
  src/core/GeorefBackup.h
  src/core/GeorefPixelWindow.h
  src/core/GeorefQuality.h
  src/core/FeaturePresets.h
  src/core/FeatureNumbering.h
  src/core/FeatureRecord.h
  src/core/SurveySchema.h
  src/core/ControlPointCsv.h
  src/core/BufferAnalysis.h
  src/core/NearbyHeritageExport.h
  src/core/RecentSurveys.h
  src/core/KaSafeQgis.h
  src/core/MeasureOps.h
  src/core/EditGeometryRepair.h
  src/core/EditHistory.h
  src/core/DemAnalyzer.h
  src/core/TrenchGridGenerator.h
  src/core/TrenchLShape.h
  src/core/TrenchGridSnap.h
  src/core/TrenchLayerEdit.h
  src/core/TrenchPlanCache.h
  src/core/TilePackService.h
  src/core/SoilMapService.h
  src/core/PaleoLandformService.h
  src/core/GeologyMapService.h
  src/core/RiverMapService.h
  src/core/Terrain3dService.h
  src/core/Terrain3dLayoutService.h
  src/core/TilePrint.h
  src/core/CadDwgConverter.h
  src/core/CadDrawingReader.h
  src/core/CadCrsGuess.h
  src/core/CadCrsHints.h
  src/core/CadPendingCopies.h
  src/core/LineMapStyle.h
  src/core/CadDrawingStore.h
  src/core/CadDrawingLayers.h
  src/core/SnapTraceOps.h
)
# clangd (C++ code tool): the Visual Studio generator writes no compile_commands.json, so leave a
# CMake file-API codemodel query (answered by every later cmake run) and convert the reply whenever
# ka_core builds. docs/intent/2026-10-03-dev-setup-gaps.md
file(WRITE "${CMAKE_BINARY_DIR}/.cmake/api/v1/query/codemodel-v2" "")
find_program(KA_NODE_EXECUTABLE node)
if(KA_NODE_EXECUTABLE)
  add_custom_target(ka_compile_commands
    COMMAND "${KA_NODE_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/compile-commands.mjs" "${CMAKE_BINARY_DIR}" "${CMAKE_CXX_COMPILER}"
    VERBATIM)
  add_dependencies(ka_core ka_compile_commands)
endif()
target_include_directories(ka_core PUBLIC ${KA_INCLUDES})
target_link_directories(ka_core PUBLIC ${KA_LIBDIRS})
target_link_libraries(ka_core PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Xml Qt6::PrintSupport Qt6::Pdf Qt6::Network Qt6::Svg qgis_core qgis_gui gdal_i)
if (WIN32)
  target_link_libraries(ka_core PUBLIC crypt32)
endif()
if (MSVC)
  target_compile_options(ka_core PRIVATE /openmp /utf-8)
endif()
target_compile_definitions(ka_core PUBLIC
  KA_HGIS_HAS_QGIS=1
  "CORE_EXPORT=__declspec(dllimport)"
  "GUI_EXPORT=__declspec(dllimport)"
  "KA_HGIS_VERSION=\"${PROJECT_VERSION}\"")

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/data/theme/ka-hgis.qss")
file(READ "${CMAKE_SOURCE_DIR}/data/theme/ka-hgis.qss" KA_QSS_CONTENT)
# CMake string lengths are UTF-8 byte counts. Bound each MSVC raw literal
# to 8 KiB and split only after a newline, preserving Korean byte sequences.
set(_ka_qss_remaining "${KA_QSS_CONTENT}")
set(KA_QSS_LITERALS "")
while(TRUE)
  string(LENGTH "${_ka_qss_remaining}" _ka_qss_length)
  if(_ka_qss_length GREATER 8192)
    string(SUBSTRING "${_ka_qss_remaining}" 0 8192 _ka_qss_prefix)
    string(FIND "${_ka_qss_prefix}" "\n" _ka_qss_split REVERSE)
    if(_ka_qss_split LESS 0)
      message(FATAL_ERROR "ka-hgis.qss has a line longer than 8192 UTF-8 bytes; wrap that line before embedding it.")
    endif()
    math(EXPR _ka_qss_chunk_length "${_ka_qss_split} + 1")
  else()
    set(_ka_qss_chunk_length ${_ka_qss_length})
  endif()
  string(SUBSTRING "${_ka_qss_remaining}" 0 ${_ka_qss_chunk_length} _ka_qss_chunk)
  string(FIND "${_ka_qss_chunk}" ")KAQSS\"" _ka_qss_delimiter)
  if(NOT _ka_qss_delimiter LESS 0)
    message(FATAL_ERROR "ka-hgis.qss contains the reserved raw-string delimiter )KAQSS\".")
  endif()
  string(APPEND KA_QSS_LITERALS "R\"KAQSS(${_ka_qss_chunk})KAQSS\"\n")
  if(_ka_qss_length LESS_EQUAL 8192)
    break()
  endif()
  string(SUBSTRING "${_ka_qss_remaining}" ${_ka_qss_chunk_length} -1 _ka_qss_remaining)
endwhile()
configure_file(
  "${CMAKE_SOURCE_DIR}/src/app/ka-hgis.qss.inc.in"
  "${CMAKE_BINARY_DIR}/generated/ka-hgis.qss.inc"
  @ONLY
  NEWLINE_STYLE UNIX
)

add_executable(ka-hgis
  data/theme/icons.qrc
  data/providers/ngii.qrc
  src/app/KaTopographicScopePanel.cpp
  src/app/KaTopographicScopePanel.h
  src/app/app.rc
  src/app/main.cpp
  src/app/MainWindow.cpp
    src/app/MainWindowContextMenus.cpp
    src/app/MainWindowUndo.cpp
    src/app/MainWindowRibbon.cpp
    src/app/MainWindowOffline.cpp
    src/app/MainWindowTrench.cpp
    src/app/MainWindowAlign.cpp
    src/app/MainWindowOverlay.cpp
    src/app/MainWindowPaleo.cpp
    src/app/KaPaleoMenu.h
    src/app/MainWindowFiles.cpp
    src/app/MainWindowDownloads.cpp
    src/app/MainWindowExport.cpp
    src/app/MainWindowEditing.cpp
    src/app/MainWindowErase.cpp
    src/app/MainWindowSession.cpp
    src/app/MainWindowTopographic.cpp
    src/app/MainWindowCadastral.cpp
    src/app/MainWindowFeatureCard.cpp
  src/app/KaLayerInformation.cpp
  src/app/KaLayerInformation.h
  src/app/KaLayerInformationPanel.cpp
  src/app/KaLayerRefreshBatch.cpp
  src/app/KaLayerRefreshBatch.h
  src/app/KaLayerTreeUndo.cpp
  src/app/KaLayerTreeUndo.h
  src/app/KaTopographicBrowser.cpp
  src/app/KaTopographicBrowser.h
  src/app/KaHeritageBrowser.cpp
  src/app/KaHeritageBrowser.h
  src/app/KaHeritageBrowserPlan.cpp
  src/app/KaHeritageSetupDialogs.cpp
  src/app/KaHeritageSetupDialogs.h
  src/app/KaTopographicAccountDialog.cpp
  src/app/KaTopographicAccountDialog.h
  src/app/KaTopographicImportDialog.cpp
  src/app/KaTopographicImportDialog.h
  src/app/KaBeginnerRibbon.cpp
  src/app/KaToolbarFit.cpp
  src/app/KaApplication.cpp
  src/app/KaStartupSplash.cpp
  src/app/KaStartupSplash.h
  src/app/KaSplashStrata.cpp
  src/app/KaSplashStrata.h
  src/app/KaSplashPalette.h
  src/app/KaSplashCredits.cpp
  src/app/KaSplashCredits.h
  src/app/KaSplashArt.cpp
  src/app/KaSplashArt.h
  src/app/KaDownloadUi.cpp
  src/app/KaDownloadUi.h
  src/app/KaReferenceDownloadJob.cpp
  src/app/KaReferenceStatusDialog.cpp
  src/app/KaReferenceStatusDialog.h
  src/app/KaTheme.cpp
  src/app/KaThemeSheet.cpp
  src/app/KaThemeStyle.cpp
  src/app/KaThemeOptions.cpp
  src/app/KaThemeOptions.h
  src/app/KaIconMetrics.h
  src/app/KaUserError.cpp
  src/app/KaSubmitDialog.cpp
  src/app/KaSubmitDialogPlate.cpp
  src/app/KaSubmitDrawingStrip.cpp
  src/app/KaSubmitDialog.h
  src/app/KaSubmitDrawingStrip.h
  src/app/KaSubmitFlow.cpp
  src/app/KaIcons.cpp
  src/app/KaCaptureMapTool.cpp
  src/app/KaSketchMeasure.cpp
  src/app/KaAttributeMapTool.cpp
  src/app/KaDrawingStudio.cpp
  src/app/KaPaperDialog.cpp
  src/app/KaPaperDialog.h
  src/app/KaMapPagePlate.cpp
  src/app/KaMapPagePlate.h
  src/app/KaDrawingStudioSheets.cpp
  src/app/KaDrawingTools.cpp
  src/app/KaDrawingNorthArrow.cpp
  src/app/KaDrawingCoordCallout.cpp
  src/app/KaDrawingLayerSets.cpp
  src/app/KaDrawingSheetSet.cpp
  src/app/KaDrawingPdfProgress.cpp
  src/app/KaTitleBlock.cpp
  src/app/KaTerrain3dStudio.cpp
  src/app/KaTerrain3dLayoutStudio.cpp
  src/app/KaTerrain3dView.cpp
  src/app/KaAlignMapTool.cpp
  src/app/KaAlignMapToolEdit.cpp
  src/app/KaAlignMapToolSave.cpp
  src/app/KaImageView.cpp
  src/app/KaAlignImageDetail.cpp
  src/app/KaStartPage.cpp
  src/app/KaHomeTitleBlock.cpp
  src/app/KaStartHero.cpp
  src/app/KaHomeRecentCard.cpp
  src/app/KaHomeStatus.cpp
  src/app/KaHomeText.cpp
  src/app/KaClosePrompt.cpp
  src/app/KaNewSurveyDialog.cpp
  src/app/KaLongitudeRuler.cpp
  src/app/KaRecoverySnapshots.cpp
  src/app/KaAppBar.cpp
  src/app/KaMapControls.cpp
  src/app/KaMeasureMapTool.cpp
  src/app/KaEditErrors.cpp
  src/app/KaEditUndoOrder.cpp
  src/app/KaDrawSketchTools.cpp
  src/app/KaDrawRowRules.cpp
  src/app/KaDrawRowRules.h
  src/app/KaAboveLabelsOverlay.cpp
  src/app/KaCanvasGridOverlay.cpp
  src/app/KaGridOriginButton.cpp
  src/app/KaTrenchMoveTool.cpp
  src/app/KaTrenchMoveToolKeys.cpp
  src/app/KaVertexEditTool.cpp
  src/app/KaFeatureSelectTool.cpp
  src/app/KaFeatureSelectPick.cpp
  src/app/KaParcelCopyKeys.cpp
  src/app/KaTrenchDialog.cpp
  src/app/KaTrenchDialogPlan.cpp
  src/app/KaDemClassDialog.cpp
  src/app/KaSurveyContourDialog.cpp
  src/app/KaCadImport.cpp
  src/app/KaLineColorDialog.cpp
  src/app/KaCadImportNotice.cpp
  src/app/KaBlockingTask.cpp
  src/app/MainWindowSurveyContour.cpp
  src/app/KaSurveyAreaDialog.cpp
  src/app/KaStatusBar.cpp
  src/app/KaCrashGuard.cpp
  src/app/KaShellUi.cpp
  src/app/KaShellStartup.cpp
  src/app/KaShellGridControls.cpp
  src/app/KaShellFocus.cpp
  src/app/KaGdalErrorLog.cpp
  src/app/KaRegionLocator.cpp
  src/app/KaFileBrowserPanel.cpp
  src/app/KaLayerOpacityRail.cpp
  src/app/KaSnapSettingsWidget.cpp
  src/app/KaSnapSettingsWidget.h
  src/app/KaFeatureFormDialog.cpp
  src/app/KaFeatureFormDialog.h
  src/app/KaFeatureCard.cpp
  src/app/KaFeatureCard.h
  src/app/KaFeatureCardEdit.cpp
  src/app/KaApiKeyDialog.cpp
  src/app/KaMenuCaption.cpp
  src/app/KaFormPlate.cpp
  src/app/KaRecordPlate.cpp
  src/app/KaElevationFigure.cpp
  src/app/KaControlPointDialog.cpp
  src/app/KaControlCsvBox.cpp
  src/app/KaProgressPlate.cpp
  src/app/KaDrawingStudioPlate.cpp
  src/app/KaTabClose.cpp
  src/app/KaFileRowDelegate.cpp
  src/app/KaStageRail.cpp
  src/app/KaTargetMeter.cpp
  src/app/KaAzimuthDial.cpp
  src/app/KaPrintDialog.cpp
  src/app/KaPrintDialogPlate.cpp
  src/app/KaTilePreview.cpp
  src/app/KaApiKeyDialog.h
  src/app/KaMenuCaption.h
  src/app/KaFormPlate.h
  src/app/KaRecordPlate.h
  src/app/KaElevationFigure.h
  src/app/KaControlPointDialog.h
  src/app/KaControlCsvBox.h
  src/app/KaProgressPlate.h
  src/app/KaDrawingStudioPlate.h
  src/app/KaTabClose.h
  src/app/KaFileRowDelegate.h
  src/app/KaStageRail.h
  src/app/KaTargetMeter.h
  src/app/KaAzimuthDial.h
  src/app/KaPrintDialog.h
  src/app/KaTilePreview.h
  src/app/MainWindow.h
  src/app/KaSurveyAreaDialog.h
  src/app/KaFileBrowserPanel.h
  src/app/KaLayerOpacityRail.h
  src/app/KaBeginnerRibbon.h
  src/app/KaToolbarFit.h
  src/app/KaApplication.h
  src/app/KaTheme.h
  src/app/KaUserError.h
  src/app/KaIcons.h
  src/app/KaCaptureMapTool.h
  src/app/KaSketchMeasure.h
  src/app/KaDrawCursor.h
  src/app/KaAttributeMapTool.h
  src/app/KaDrawingStudio.h
  src/app/KaDrawingTools.h
  src/app/KaDrawingNorthArrow.h
  src/app/KaDrawingCoordCallout.h
  src/app/KaDrawingLayerSets.h
  src/app/KaDrawingSheetSet.h
  src/app/KaDrawingPdfProgress.h
  src/app/KaTitleBlock.h
  src/app/KaTerrain3dStudio.h
  src/app/KaTerrain3dLayoutStudio.h
  src/app/KaTerrain3dView.h
  src/app/KaAlignMapTool.h
  src/app/KaImageView.h
  src/app/KaStartPage.h
  src/app/KaHomeTitleBlock.h
  src/app/KaStartHero.h
  src/app/KaHomeRecentCard.h
  src/app/KaHomeStatus.h
  src/app/KaHomeText.h
  src/app/KaClosePrompt.h
  src/app/KaNewSurveyDialog.h
  src/app/KaLongitudeRuler.h
  src/app/KaWindowSize.h
  src/app/KaRecoverySnapshots.h
  src/app/KaAppBar.h
  src/app/KaMapControls.h
  src/app/KaMeasureMapTool.h
  src/app/KaEditErrors.h
  src/app/KaEditTolerance.h
  src/app/KaAboveLabelsOverlay.h
  src/app/KaCanvasGridOverlay.h
  src/app/KaGridOriginButton.h
  src/app/KaTrenchMoveTool.h
  src/app/KaVertexEditTool.h
  src/app/KaFeatureSelectTool.h
  src/app/KaTrenchDialog.h
  src/app/KaDemClassDialog.h
  src/app/KaSurveyContourDialog.h
  src/app/KaCadImport.h
  src/app/KaLineColorDialog.h
  src/app/KaCadImportNotice.h
  src/app/KaBlockingTask.h
  src/app/KaUndoGroup.h
  src/app/KaStatusBar.h
  src/app/KaRegionLocator.h
  src/app/KaCrashGuard.h
  src/app/KaShellUi.h
  src/app/KaShellStartup.h
  src/app/KaShellGridControls.h
  src/app/KaShellFocus.h
  src/app/KaGdalErrorLog.h
)
target_include_directories(ka-hgis PRIVATE ${KA_INCLUDES} ${CMAKE_BINARY_DIR}/generated)
target_link_directories(ka-hgis PRIVATE ${KA_LIBDIRS})
target_link_libraries(ka-hgis PRIVATE ka_core Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Xml Qt6::PrintSupport Qt6::Network Qt6::Svg Qt6::WebEngineWidgets Qt6::Multimedia Qt6::MultimediaWidgets qgis_core qgis_gui)
target_compile_definitions(ka-hgis PRIVATE
  KA_HGIS_HAS_QGIS=1
  "CORE_EXPORT=__declspec(dllimport)"
  "GUI_EXPORT=__declspec(dllimport)"
  "KA_HGIS_VERSION=\"${PROJECT_VERSION}\"")
# Desktop GUI. QGIS/Qt debug text must not open a console behind the window.
set_target_properties(ka-hgis PROPERTIES WIN32_EXECUTABLE TRUE)

# 릴리스에도 PDB를 만들어 크래시 로그(KaCrashGuard)가 함수명·줄번호까지 심볼화하게 한다.
# /FS: 병렬 컴파일에서 같은 PDB에 안전하게 쓰도록(C1041 방지).
# KA_HGIS_EMBEDDED_DEBUG_INFO: GitHub 검사의 캐시 실험용. 컴파일러 캐시는 /Zi를 담지 못해 /Z7(디버그
# 정보를 .obj 안에)로 바꾼다. 링크의 /DEBUG가 그대로 ka-hgis.pdb를 만든다.
option(KA_HGIS_EMBEDDED_DEBUG_INFO "Release debug info as /Z7 so a compiler cache can keep it" OFF)
set(KA_HGIS_RELEASE_DEBUG_INFO /Zi)
if(KA_HGIS_EMBEDDED_DEBUG_INFO)
  set(KA_HGIS_RELEASE_DEBUG_INFO /Z7)
endif()
if(MSVC)
  target_compile_options(ka_core PRIVATE "$<$<CONFIG:Release>:${KA_HGIS_RELEASE_DEBUG_INFO}>" "$<$<CONFIG:Release>:/FS>")
  target_compile_options(ka-hgis PRIVATE /utf-8 "$<$<CONFIG:Release>:${KA_HGIS_RELEASE_DEBUG_INFO}>" "$<$<CONFIG:Release>:/FS>")
  target_link_options(ka-hgis PRIVATE
    "$<$<CONFIG:Release>:/DEBUG>" "$<$<CONFIG:Release>:/OPT:REF>" "$<$<CONFIG:Release>:/OPT:ICF>")
endif()

# 가장 많이 쓰는 Qt·표준·QGIS 헤더를 미리 컴파일한다(docs/intent/2026-10-03-dev-speed-and-flaky.md).
set(KA_HGIS_PCH_CORE <QDir> <QFile> <QFileInfo> <QSet> <QString> <QVariant> <algorithm> <cmath> <memory>
    <qgscoordinatereferencesystem.h> <qgscoordinatetransform.h> <qgsfeature.h> <qgsgeometry.h> <qgsproject.h>
    <qgsvectorlayer.h>)
target_precompile_headers(ka_core PRIVATE ${KA_HGIS_PCH_CORE})
target_precompile_headers(ka-hgis PRIVATE ${KA_HGIS_PCH_CORE} <QAction> <QHBoxLayout> <QLabel> <QLineEdit>
    <QPushButton> <QTimer> <QToolButton> <QVBoxLayout> <qgsmapcanvas.h>)


add_custom_command(TARGET ka-hgis POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ka-hgis>/data/rules"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/data/rules/drawing_checklist.v1.json"
    "$<TARGET_FILE_DIR:ka-hgis>/data/rules/drawing_checklist.v1.json"
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ka-hgis>/data/styles"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/data/styles/feature_presets.json"
    "$<TARGET_FILE_DIR:ka-hgis>/data/styles/feature_presets.json"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/data/korea_dongs.json"
    "$<TARGET_FILE_DIR:ka-hgis>/data/korea_dongs.json"
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ka-hgis>/data/theme"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/data/theme/ka-hgis.qss"
    "$<TARGET_FILE_DIR:ka-hgis>/data/theme/ka-hgis.qss"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/data/theme/ka-hgis.ico"
    "$<TARGET_FILE_DIR:ka-hgis>/data/theme/ka-hgis.ico"
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ka-hgis>/data/fonts"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/ka-hgis-실행.bat"
    "$<TARGET_FILE_DIR:ka-hgis>/ka-hgis-실행.bat"
  # DWG 변환기 LibreDWG(별도 프로그램): CadDwgConverter::bundledTool() 이 앱 폴더의 tools/libredwg 에서 찾는다.
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ka-hgis>/tools/libredwg"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/third_party/libredwg/0.14/dwg2dxf.exe"
    "${CMAKE_SOURCE_DIR}/third_party/libredwg/0.14/libredwg-0.dll"
    "${CMAKE_SOURCE_DIR}/third_party/libredwg/0.14/libiconv-2.dll"
    "${CMAKE_SOURCE_DIR}/third_party/libredwg/0.14/COPYING"
    "${CMAKE_SOURCE_DIR}/third_party/libredwg/0.14/SOURCE.txt"
    "$<TARGET_FILE_DIR:ka-hgis>/tools/libredwg"
)

if(KA_HGIS_BUILD_TESTS)
  enable_testing()
  # QtTest 결과는 이 Qt 빌드에서 표준출력으로 나오지 않는다. 래퍼로 실행해
  # 실패했을 때만 원인을 ctest 로그에 남긴다. 자세한 사정은 cmake/run_qtest.cmake 참고.
  function(ka_add_qtest test_name target_name)
    ka_add_qtest_filter(${test_name} ${target_name})
  endfunction()
  function(ka_add_qtest_filter test_name target_name)
    string(JOIN " " _ka_filter ${ARGN})
    add_test(NAME ${test_name} COMMAND ${CMAKE_COMMAND}
      -DKA_TEST_EXE=$<TARGET_FILE:${target_name}>
      -DKA_TEST_LOG=${CMAKE_BINARY_DIR}/test-logs/${test_name}.txt
      "-DKA_TEST_FILTER=${_ka_filter}"
      -P ${CMAKE_SOURCE_DIR}/cmake/run_qtest.cmake
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  endfunction()
  set(KA_TEST_DEFS KA_HGIS_HAS_QGIS=1 "CORE_EXPORT=__declspec(dllimport)" "GUI_EXPORT=__declspec(dllimport)")
  set(KA_TEST_LIBS ka_core Qt6::Test Qt6::Core Qt6::Xml Qt6::Gui Qt6::Widgets Qt6::PrintSupport qgis_core qgis_gui)

  add_executable(ka_hgis_tests tests/test_checklist.cpp)
  target_link_libraries(ka_hgis_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_hgis_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_hgis_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_hgis_tests PRIVATE ${KA_TEST_DEFS})
  ka_add_qtest(checklist_engine ka_hgis_tests)

  add_executable(ka_workflow_tests tests/test_workflow.cpp
    src/app/KaCaptureMapTool.cpp src/app/KaCaptureMapTool.h src/app/KaSketchMeasure.cpp src/app/KaSketchMeasure.h
    src/app/KaCrashGuard.cpp src/app/KaCrashGuard.h)
  target_link_libraries(ka_workflow_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_workflow_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_workflow_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_workflow_tests PRIVATE ${KA_TEST_DEFS})
  if (MSVC)
    target_compile_options(ka_workflow_tests PRIVATE /utf-8)
  endif()
  ka_add_qtest(workflow_engine ka_workflow_tests)
  # 이 PC 약 90초. GitHub 무료 기계는 GPKG 열기만 6~11초 걸려 300초를 넘겼다(2026-10-02 CI).
  set_tests_properties(workflow_engine PROPERTIES TIMEOUT 900)

  add_executable(ka_editing_tools_tests tests/test_editing_tools.cpp
    src/app/KaCaptureMapTool.cpp src/app/KaCaptureMapTool.h src/app/KaSketchMeasure.cpp src/app/KaSketchMeasure.h
    src/app/KaFeatureSelectTool.cpp src/app/KaFeatureSelectPick.cpp src/app/KaFeatureSelectTool.h
    src/app/KaVertexEditTool.cpp src/app/KaVertexEditTool.h
    src/app/KaAttributeMapTool.cpp src/app/KaAttributeMapTool.h
    src/app/KaEditTolerance.h
    src/app/KaCrashGuard.cpp src/app/KaCrashGuard.h)
  target_link_libraries(ka_editing_tools_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_editing_tools_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_editing_tools_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_editing_tools_tests PRIVATE ${KA_TEST_DEFS})
  ka_add_qtest(editing_tools ka_editing_tools_tests)
  add_executable(ka_parcel_copy_tests tests/test_parcel_copy.cpp tests/parcel_copy_fixture.h)
  target_link_libraries(ka_parcel_copy_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_parcel_copy_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_parcel_copy_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_parcel_copy_tests PRIVATE ${KA_TEST_DEFS})
  ka_add_qtest(parcel_copy ka_parcel_copy_tests)
  add_executable(ka_parcel_copy_keys_tests tests/test_parcel_copy_keys.cpp tests/parcel_copy_fixture.h
    src/app/KaCaptureMapTool.cpp src/app/KaCaptureMapTool.h src/app/KaSketchMeasure.cpp src/app/KaSketchMeasure.h
    src/app/KaFeatureSelectTool.cpp src/app/KaFeatureSelectPick.cpp src/app/KaFeatureSelectTool.h
    src/app/KaVertexEditTool.cpp src/app/KaVertexEditTool.h
    src/app/KaAttributeMapTool.cpp src/app/KaAttributeMapTool.h
    src/app/KaEditTolerance.h
    src/app/KaCrashGuard.cpp src/app/KaCrashGuard.h
    src/app/KaParcelCopyKeys.cpp src/app/KaParcelCopyKeys.h)
  target_link_libraries(ka_parcel_copy_keys_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_parcel_copy_keys_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_parcel_copy_keys_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_parcel_copy_keys_tests PRIVATE ${KA_TEST_DEFS})
  ka_add_qtest(parcel_copy_keys ka_parcel_copy_keys_tests)

  # 도형선택 cases that used to pick nothing (unsaved shape, lifted layer, inner piece) and key A.
  add_executable(ka_inner_piece_tests tests/test_inner_piece.cpp
    src/app/KaCaptureMapTool.cpp src/app/KaCaptureMapTool.h src/app/KaSketchMeasure.cpp src/app/KaSketchMeasure.h
    src/app/KaFeatureSelectTool.cpp src/app/KaFeatureSelectPick.cpp src/app/KaFeatureSelectTool.h
    src/app/KaVertexEditTool.cpp src/app/KaVertexEditTool.h
    src/app/KaAttributeMapTool.cpp src/app/KaAttributeMapTool.h
    src/app/KaEditTolerance.h src/app/KaPickLayers.h
    src/app/KaCrashGuard.cpp src/app/KaCrashGuard.h)
  target_link_libraries(ka_inner_piece_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_inner_piece_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_inner_piece_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_inner_piece_tests PRIVATE ${KA_TEST_DEFS})
  ka_add_qtest(inner_piece ka_inner_piece_tests)

  add_executable(ka_polygon_pieces_tests tests/test_polygon_pieces.cpp)
  target_link_libraries(ka_polygon_pieces_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_polygon_pieces_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_polygon_pieces_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_polygon_pieces_tests PRIVATE ${KA_TEST_DEFS})
  ka_add_qtest(polygon_pieces ka_polygon_pieces_tests)

  add_executable(ka_georef_tests tests/test_georef.cpp)
  target_link_libraries(ka_georef_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_georef_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_georef_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_georef_tests PRIVATE ${KA_TEST_DEFS})
  ka_add_qtest(georef_engine ka_georef_tests)

  # DWG를 DXF로 바꾸는 CadDwgConverter. 앱 폴더 대신 third_party 의 LibreDWG dwg2dxf 를 바로 돌린다.
  add_executable(ka_cad_dwg_tests tests/test_cad_dwg.cpp)
  target_link_libraries(ka_cad_dwg_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_cad_dwg_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_cad_dwg_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_cad_dwg_tests PRIVATE ${KA_TEST_DEFS}
    "KA_TEST_LIBREDWG=\"${CMAKE_SOURCE_DIR}/third_party/libredwg/0.14/dwg2dxf.exe\""
    "KA_TEST_CAD_DATA=\"${CMAKE_SOURCE_DIR}/tests/data/cad\"")
  set_target_properties(ka_cad_dwg_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(cad_dwg ka_cad_dwg_tests)

  # DXF 한 장을 모형 공간 도형만 선·면·점·글자로 읽는 CadDrawingReader(GDAL DXF 드라이버).
  add_executable(ka_cad_reader_tests tests/test_cad_reader.cpp)
  target_link_libraries(ka_cad_reader_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_cad_reader_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_cad_reader_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_cad_reader_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_cad_reader_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(cad_reader ka_cad_reader_tests)

  # 좌표계 없는 CAD 도면 숫자로 한국 좌표계 후보를 찾는 CadCrsGuess.
  add_executable(ka_cad_crs_tests tests/test_cad_crs.cpp)
  target_link_libraries(ka_cad_crs_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_cad_crs_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_cad_crs_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_cad_crs_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_cad_crs_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(cad_crs ka_cad_crs_tests)
  # 도면 좌표계 판단의 단서: 도면이 밝힌 좌표계(.prj·파일 이름·글자), 지명이 가리키는 시·도, 파일마다의 기억.
  add_executable(ka_cad_hints_tests tests/test_cad_hints.cpp)
  target_link_libraries(ka_cad_hints_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_cad_hints_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_cad_hints_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_cad_hints_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_cad_hints_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(cad_hints ka_cad_hints_tests)
  # 한 번도 저장된 적 없는 도면 변환본만 지우기(CadPendingCopies).
  add_executable(ka_cad_pending_copies_tests tests/test_cad_pending_copies.cpp)
  target_link_libraries(ka_cad_pending_copies_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_cad_pending_copies_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_cad_pending_copies_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_cad_pending_copies_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_cad_pending_copies_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(cad_pending_copies ka_cad_pending_copies_tests)
  # 선으로 된 지도(도면 선·받은 지적도·등고선)의 선 색 바꾸기와 원래 색으로 되돌리기(LineMapStyle).
  add_executable(ka_line_map_style_tests tests/test_line_map_style.cpp)
  target_link_libraries(ka_line_map_style_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_line_map_style_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_line_map_style_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_line_map_style_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_line_map_style_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(line_map_style ka_line_map_style_tests)

  # 도면 한 장을 작업 좌표계 GPKG 변환본으로 쓰고 정합을 모든 표에 적용하는 CadDrawingStore.
  add_executable(ka_cad_store_tests tests/test_cad_store.cpp)
  target_link_libraries(ka_cad_store_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_cad_store_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_cad_store_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_cad_store_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_cad_store_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(cad_store ka_cad_store_tests)

  # 도면 변환본을 참조 지도 아래 「<이름> (도면)」 묶음으로 올리는 CadDrawingLayers.
  add_executable(ka_cad_layers_tests tests/test_cad_layers.cpp)
  target_link_libraries(ka_cad_layers_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_cad_layers_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_cad_layers_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_cad_layers_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_cad_layers_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(cad_layers ka_cad_layers_tests)

  add_executable(ka_preset_tests tests/test_presets.cpp)
  target_link_libraries(ka_preset_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_preset_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_preset_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_preset_tests PRIVATE ${KA_TEST_DEFS})
  ka_add_qtest(feature_presets ka_preset_tests)
  add_executable(ka_feature_record_tests tests/test_feature_records.cpp)
  target_link_libraries(ka_feature_record_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_feature_record_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_feature_record_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_feature_record_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_feature_record_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(feature_records ka_feature_record_tests)

  add_executable(ka_schema_version_tests tests/test_schema_version.cpp)
  target_link_libraries(ka_schema_version_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_schema_version_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_schema_version_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_schema_version_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_schema_version_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(schema_version ka_schema_version_tests)

  add_executable(ka_control_point_csv_tests tests/test_control_point_csv.cpp)
  target_link_libraries(ka_control_point_csv_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_control_point_csv_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_control_point_csv_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_control_point_csv_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_control_point_csv_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(control_point_csv ka_control_point_csv_tests)

  add_executable(ka_feature_card_tests tests/test_feature_card.cpp
    src/app/KaFeatureCard.cpp src/app/KaFeatureCard.h src/app/KaFeatureCardEdit.cpp
    src/app/KaFeatureFormDialog.cpp src/app/KaFeatureFormDialog.h)
  target_link_libraries(ka_feature_card_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_feature_card_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_feature_card_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_feature_card_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_feature_card_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(feature_card ka_feature_card_tests)

  add_executable(ka_buffer_tests tests/test_buffer.cpp)
  target_link_libraries(ka_buffer_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_buffer_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_buffer_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_buffer_tests PRIVATE ${KA_TEST_DEFS})
  ka_add_qtest(buffer_ring ka_buffer_tests)
  add_executable(ka_georef_quality_tests tests/test_georef_quality.cpp)
  target_link_libraries(ka_georef_quality_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_georef_quality_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_georef_quality_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_georef_quality_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_georef_quality_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(georef_quality ka_georef_quality_tests)

  add_executable(ka_georef_backup_tests tests/test_georef_backup.cpp)
  target_link_libraries(ka_georef_backup_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_georef_backup_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_georef_backup_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_georef_backup_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_georef_backup_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(georef_backup ka_georef_backup_tests)

  add_executable(ka_align_tool_tests tests/test_align_tool.cpp
    src/app/KaAlignMapTool.cpp src/app/KaAlignMapTool.h src/app/KaAlignMapToolEdit.cpp src/app/KaAlignMapToolSave.cpp)
  target_link_libraries(ka_align_tool_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_align_tool_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_align_tool_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_align_tool_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_align_tool_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(align_tool ka_align_tool_tests)
  # 도면(CAD 변환본) 정합: 도면 전체가 같이 움직이고, 다시 시작·다시 불러오기·좌표계 바뀜에도 안전하다.
  add_executable(ka_cad_align_tests tests/test_cad_align.cpp
    src/app/KaAlignMapTool.cpp src/app/KaAlignMapTool.h src/app/KaAlignMapToolEdit.cpp src/app/KaAlignMapToolSave.cpp)
  target_link_libraries(ka_cad_align_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_cad_align_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_cad_align_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_cad_align_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_cad_align_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(cad_align ka_cad_align_tests)

  add_executable(ka_align_image_view_tests
    tests/test_align_image_view.cpp
    src/app/KaImageView.cpp src/app/KaImageView.h
    src/app/KaAlignImageDetail.cpp
    src/app/KaTheme.cpp
    src/app/KaThemeSheet.cpp
    src/app/KaThemeStyle.cpp)
  set_target_properties(ka_align_image_view_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_align_image_view_tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets)
  target_include_directories(ka_align_image_view_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  ka_add_qtest(align_image_view ka_align_image_view_tests)

  add_executable(ka_measure_tests tests/test_measure.cpp)
  target_link_libraries(ka_measure_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_measure_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_measure_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_measure_tests PRIVATE ${KA_TEST_DEFS})
  ka_add_qtest(measure_tape ka_measure_tests)
  add_executable(ka_editing_history_tests tests/test_editing_history.cpp)
  target_link_libraries(ka_editing_history_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_editing_history_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_editing_history_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_editing_history_tests PRIVATE ${KA_TEST_DEFS})
  ka_add_qtest(editing_history ka_editing_history_tests)

  add_executable(ka_dem_trench_tests tests/test_dem_trench.cpp)
  target_link_libraries(ka_dem_trench_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_dem_trench_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_dem_trench_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_dem_trench_tests PRIVATE ${KA_TEST_DEFS})
  if (MSVC)
    target_compile_options(ka_dem_trench_tests PRIVATE /utf-8)
  endif()
  ka_add_qtest(dem_trench_engine ka_dem_trench_tests)
  add_executable(ka_trench_plan_tests tests/test_trench_plan.cpp)
  target_link_libraries(ka_trench_plan_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_trench_plan_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_trench_plan_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_trench_plan_tests PRIVATE ${KA_TEST_DEFS})
  if (MSVC)
    target_compile_options(ka_trench_plan_tests PRIVATE /utf-8)
  endif()
  ka_add_qtest(trench_plan ka_trench_plan_tests)
  set_tests_properties(trench_plan PROPERTIES TIMEOUT 120)
  # ㄱ·ㄴ자 시굴트렌치 15 m + 15 m at the 10% area (user request 2026-10-19).
  add_executable(ka_trench_lshape_tests tests/test_trench_lshape.cpp)
  target_link_libraries(ka_trench_lshape_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_trench_lshape_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_trench_lshape_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_trench_lshape_tests PRIVATE ${KA_TEST_DEFS})
  if (MSVC)
    target_compile_options(ka_trench_lshape_tests PRIVATE /utf-8)
  endif()
  ka_add_qtest(trench_lshape ka_trench_lshape_tests)
  set_tests_properties(trench_lshape PROPERTIES TIMEOUT 120)
  # 「좌표격자에 맞추기」: ㄱ·ㄴ자 corners on the coordinate grid's nodes (user request 2026-10-20).
  add_executable(ka_trench_grid_snap_tests tests/test_trench_grid_snap.cpp)
  target_link_libraries(ka_trench_grid_snap_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_trench_grid_snap_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_trench_grid_snap_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_trench_grid_snap_tests PRIVATE ${KA_TEST_DEFS})
  if (MSVC)
    target_compile_options(ka_trench_grid_snap_tests PRIVATE /utf-8)
  endif()
  ka_add_qtest(trench_grid_snap ka_trench_grid_snap_tests)
  set_tests_properties(trench_grid_snap PROPERTIES TIMEOUT 120)
  # 시굴격자 표준 규격: 20 m · 10 m trenches only, no averaged lengths (user request 2026-10-27).
  add_executable(ka_trench_standard_tests tests/test_trench_standard.cpp)
  target_link_libraries(ka_trench_standard_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_trench_standard_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_trench_standard_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_trench_standard_tests PRIVATE ${KA_TEST_DEFS})
  if (MSVC)
    target_compile_options(ka_trench_standard_tests PRIVATE /utf-8)
  endif()
  ka_add_qtest(trench_standard ka_trench_standard_tests)
  set_tests_properties(trench_standard PROPERTIES TIMEOUT 120)
  # 트렌치 쉽게 옮기기: grid-node drop and arrow keys (user request 2026-10-20).
  add_executable(ka_trench_handle_tests tests/test_trench_handle.cpp
    src/app/KaTrenchMoveTool.cpp src/app/KaTrenchMoveToolKeys.cpp src/app/KaTrenchMoveTool.h
    src/app/KaCanvasGridOverlay.cpp src/app/KaCanvasGridOverlay.h
    src/app/KaCrashGuard.cpp src/app/KaCrashGuard.h)
  target_link_libraries(ka_trench_handle_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_trench_handle_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_trench_handle_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_trench_handle_tests PRIVATE ${KA_TEST_DEFS})
  if (MSVC)
    target_compile_options(ka_trench_handle_tests PRIVATE /utf-8)
  endif()
  ka_add_qtest(trench_handle ka_trench_handle_tests)
  set_tests_properties(trench_handle PROPERTIES TIMEOUT 120)
  # 시굴격자 사면 방향 from the 수치지형도 contours when there is no DEM (user request 2026-10-05).
  add_executable(ka_trench_terrain_tests tests/test_trench_terrain.cpp)
  target_link_libraries(ka_trench_terrain_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_trench_terrain_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_trench_terrain_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_trench_terrain_tests PRIVATE ${KA_TEST_DEFS})
  if (MSVC)
    target_compile_options(ka_trench_terrain_tests PRIVATE /utf-8)
  endif()
  ka_add_qtest(trench_terrain ka_trench_terrain_tests)

  add_executable(ka_trench_edit_tests tests/test_trench_edit.cpp
    src/app/KaTrenchMoveTool.cpp src/app/KaTrenchMoveToolKeys.cpp src/app/KaTrenchMoveTool.h
    src/app/KaTrenchDialog.cpp src/app/KaTrenchDialogPlan.cpp src/app/KaTrenchDialog.h
    src/app/KaTargetMeter.cpp src/app/KaTargetMeter.h src/app/KaAzimuthDial.cpp src/app/KaAzimuthDial.h
    src/app/KaCanvasGridOverlay.cpp src/app/KaCanvasGridOverlay.h
    src/app/KaCrashGuard.cpp src/app/KaCrashGuard.h)
  target_link_libraries(ka_trench_edit_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_trench_edit_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_trench_edit_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_trench_edit_tests PRIVATE ${KA_TEST_DEFS})
  if (MSVC)
    target_compile_options(ka_trench_edit_tests PRIVATE /utf-8)
  endif()
  ka_add_qtest(trench_edit ka_trench_edit_tests)
  set_tests_properties(trench_edit PROPERTIES TIMEOUT 120)
  # 「조용한 도판」 시굴격자 속성 meter and dial (board 5); tests/test_trench_edit.cpp is at its line limit.
  add_executable(ka_trench_plate_tests tests/test_trench_plate.cpp
    src/app/KaTargetMeter.cpp src/app/KaTargetMeter.h src/app/KaAzimuthDial.cpp src/app/KaAzimuthDial.h
    src/app/KaTrenchDialog.cpp src/app/KaTrenchDialogPlan.cpp src/app/KaTrenchDialog.h
    src/app/KaCrashGuard.cpp src/app/KaCrashGuard.h)
  target_link_libraries(ka_trench_plate_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_trench_plate_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_trench_plate_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_trench_plate_tests PRIVATE ${KA_TEST_DEFS})
  if (MSVC)
    target_compile_options(ka_trench_plate_tests PRIVATE /utf-8)
  endif()
  ka_add_qtest(trench_plate ka_trench_plate_tests)
  set_tests_properties(trench_plate PROPERTIES TIMEOUT 120)

  add_executable(ka_survey_contour_tests tests/test_survey_contour.cpp tests/test_survey_contour_clip.cpp)
  target_link_libraries(ka_survey_contour_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_survey_contour_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_survey_contour_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_survey_contour_tests PRIVATE ${KA_TEST_DEFS})
  if (MSVC)
    target_compile_options(ka_survey_contour_tests PRIVATE /utf-8)
  endif()
  ka_add_qtest(survey_contour ka_survey_contour_tests)
  set_tests_properties(survey_contour PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE)
  add_executable(ka_survey_point_arrange_tests tests/test_survey_point_arrange.cpp)
  target_link_libraries(ka_survey_point_arrange_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_survey_point_arrange_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  if (MSVC)
    target_compile_options(ka_survey_point_arrange_tests PRIVATE /utf-8)
  endif()
  ka_add_qtest(survey_point_arrange ka_survey_point_arrange_tests)

  add_executable(ka_contour_safety_tests tests/test_contour_safety.cpp)
  target_link_libraries(ka_contour_safety_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_contour_safety_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  if (MSVC)
    target_compile_options(ka_contour_safety_tests PRIVATE /utf-8)
  endif()
  ka_add_qtest(contour_safety ka_contour_safety_tests)
  set_tests_properties(contour_safety PROPERTIES TIMEOUT 180)

  add_executable(ka_grid_math_tests tests/test_grid_math.cpp)
  target_link_libraries(ka_grid_math_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_grid_math_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(grid_math ka_grid_math_tests)

  add_executable(ka_dem_hillshade_tests tests/test_dem_hillshade.cpp)
  target_link_libraries(ka_dem_hillshade_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_dem_hillshade_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  if (MSVC)
    target_compile_options(ka_dem_hillshade_tests PRIVATE /utf-8)
  endif()
  ka_add_qtest(dem_hillshade ka_dem_hillshade_tests)

  add_executable(ka_survey_point_dialog_tests tests/test_survey_point_dialogs.cpp
    src/app/KaSurveyContourDialog.cpp src/app/KaSurveyContourDialog.h
    src/app/KaSurveyAreaDialog.cpp src/app/KaSurveyAreaDialog.h
    src/app/KaDemClassDialog.cpp src/app/KaDemClassDialog.h
    src/app/KaGridOriginButton.cpp src/app/KaGridOriginButton.h
    src/app/KaCanvasGridOverlay.cpp src/app/KaCanvasGridOverlay.h
    src/app/KaCrashGuard.cpp src/app/KaCrashGuard.h
    src/app/KaTheme.cpp src/app/KaThemeSheet.cpp src/app/KaThemeStyle.cpp)
  target_link_libraries(ka_survey_point_dialog_tests PRIVATE ka_core Qt6::Test)
  target_include_directories(ka_survey_point_dialog_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  set_target_properties(ka_survey_point_dialog_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  if (MSVC)
    target_compile_options(ka_survey_point_dialog_tests PRIVATE /utf-8)
  endif()
  ka_add_qtest(survey_point_dialogs ka_survey_point_dialog_tests)
  set_tests_properties(survey_point_dialogs PROPERTIES TIMEOUT 90)
  # 새 모양 조사구역 그리기 설정 (2차 보드 판 5); tests/test_survey_point_dialogs.cpp is at its line limit.
  add_executable(ka_survey_area_plate_tests tests/test_survey_area_plate.cpp
    src/app/KaSurveyAreaDialog.cpp src/app/KaSurveyAreaDialog.h
    src/app/KaTheme.cpp src/app/KaThemeSheet.cpp src/app/KaThemeStyle.cpp)
  target_link_libraries(ka_survey_area_plate_tests PRIVATE ka_core Qt6::Test)
  target_include_directories(ka_survey_area_plate_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  set_target_properties(ka_survey_area_plate_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  if (MSVC)
    target_compile_options(ka_survey_area_plate_tests PRIVATE /utf-8)
  endif()
  ka_add_qtest(survey_area_plate ka_survey_area_plate_tests)
  set_tests_properties(survey_area_plate PROPERTIES ENVIRONMENT
    "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 90)

  add_executable(ka_terrain_3d_tests tests/test_terrain_3d.cpp)
  target_link_libraries(ka_terrain_3d_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_terrain_3d_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_terrain_3d_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_terrain_3d_tests PRIVATE ${KA_TEST_DEFS})
  if (MSVC)
    target_compile_options(ka_terrain_3d_tests PRIVATE /utf-8)
  endif()
  ka_add_qtest(terrain_3d_engine ka_terrain_3d_tests)

  # Console subsystem, otherwise QtTest PASS/FAIL lines never reach ctest.
  set_target_properties(ka_hgis_tests ka_workflow_tests ka_georef_tests ka_preset_tests
                        ka_buffer_tests ka_measure_tests ka_dem_trench_tests
                        ka_terrain_3d_tests
                        PROPERTIES WIN32_EXECUTABLE FALSE)


  add_executable(ka_theme_tests
    data/theme/icons.qrc
    tests/test_theme.cpp
    src/app/KaTheme.cpp
    src/app/KaThemeSheet.cpp
    src/app/KaThemeStyle.cpp
    src/app/KaBeginnerRibbon.cpp
    src/app/KaIcons.cpp
    src/app/KaAppBar.cpp
    src/app/KaSplashArt.cpp
  )
  set_target_properties(ka_theme_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_theme_tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets)
  target_include_directories(ka_theme_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
  )
  ka_add_qtest(theme_qss ka_theme_tests)
  add_executable(ka_theme_options_tests
    tests/test_theme_options.cpp
    src/app/KaTheme.cpp
    src/app/KaThemeSheet.cpp
    src/app/KaThemeStyle.cpp
    src/app/KaThemeOptions.cpp
  )
  add_executable(ka_theme_render_tests
    tests/test_theme_render.cpp
    src/app/KaTheme.cpp
    src/app/KaThemeSheet.cpp
    src/app/KaThemeStyle.cpp
    src/app/KaBeginnerRibbon.cpp
    src/app/KaDownloadUi.cpp
  )
  add_executable(ka_ribbon_overflow_tests
    tests/test_ribbon_overflow.cpp
    src/app/KaIcons.cpp
    src/app/KaTheme.cpp
    src/app/KaThemeSheet.cpp
    src/app/KaThemeStyle.cpp
    src/app/KaBeginnerRibbon.cpp
    src/app/KaAppBar.cpp
  )
  add_executable(ka_ribbon_pixels_tests
    tests/test_ribbon_pixels.cpp
    src/app/KaIcons.cpp
    src/app/KaTheme.cpp
    src/app/KaThemeSheet.cpp
    src/app/KaThemeStyle.cpp
    src/app/KaBeginnerRibbon.cpp
  )
  add_executable(ka_toolbar_fit_tests
    tests/test_toolbar_fit.cpp
    src/app/KaToolbarFit.cpp
    src/app/KaTheme.cpp
    src/app/KaThemeSheet.cpp
    src/app/KaThemeStyle.cpp
  )
  add_executable(ka_icon_glyph_tests
    tests/test_icons.cpp
    src/app/KaIcons.cpp
    src/app/KaTheme.cpp
    src/app/KaThemeSheet.cpp
    src/app/KaThemeStyle.cpp
  )
  add_executable(ka_icon_mockup_tests
    data/theme/icons.qrc
    tests/test_icons_mockup.cpp
    src/app/KaIcons.cpp
    src/app/KaTheme.cpp
    src/app/KaThemeSheet.cpp
    src/app/KaThemeStyle.cpp
  )
  foreach(_ka_theme_target IN ITEMS ka_theme_options_tests ka_theme_render_tests ka_ribbon_overflow_tests ka_ribbon_pixels_tests ka_toolbar_fit_tests ka_icon_glyph_tests ka_icon_mockup_tests)
    set_target_properties(${_ka_theme_target} PROPERTIES WIN32_EXECUTABLE FALSE)
    target_link_libraries(${_ka_theme_target} PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets)
    target_include_directories(${_ka_theme_target} PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  endforeach()
  ka_add_qtest(theme_options ka_theme_options_tests)
  # 화면 보기 기본값: first start in 새 모양 with 고대비 on (user request 2026-10-05).
  add_executable(ka_display_defaults_tests tests/test_display_defaults.cpp
    src/app/KaTheme.cpp src/app/KaThemeSheet.cpp src/app/KaThemeStyle.cpp src/app/KaThemeOptions.cpp)
  set_target_properties(ka_display_defaults_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_display_defaults_tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets)
  target_include_directories(ka_display_defaults_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  ka_add_qtest(display_defaults ka_display_defaults_tests)
  ka_add_qtest(theme_render ka_theme_render_tests)
  ka_add_qtest(ribbon_overflow ka_ribbon_overflow_tests)
  ka_add_qtest(ribbon_pixels ka_ribbon_pixels_tests)
  ka_add_qtest(toolbar_fit ka_toolbar_fit_tests)
  ka_add_qtest(icon_glyphs ka_icon_glyph_tests)
  ka_add_qtest(icon_mockup ka_icon_mockup_tests)

  add_executable(ka_user_error_tests
    tests/test_user_error.cpp
    src/app/KaUserError.cpp
  )
  set_target_properties(ka_user_error_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_user_error_tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets)
  target_include_directories(ka_user_error_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
  ka_add_qtest(user_error ka_user_error_tests)

  add_executable(ka_region_locator_tests tests/test_region_locator.cpp
    src/app/KaRegionLocator.cpp src/app/KaBeginnerRibbon.cpp src/app/KaTheme.cpp src/app/KaThemeSheet.cpp src/app/KaThemeStyle.cpp
    src/core/KoreaRegionCatalog.cpp)
  set_target_properties(ka_region_locator_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_region_locator_tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets)
  target_include_directories(ka_region_locator_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  ka_add_qtest(region_locator_popup ka_region_locator_tests)

  add_executable(ka_heritage_import_tests tests/test_heritage_import.cpp)
  add_executable(ka_gdal_log_tests tests/test_gdal_error_log.cpp
    src/app/KaGdalErrorLog.cpp src/app/KaGdalErrorLog.h src/app/KaCrashGuard.cpp src/app/KaCrashGuard.h)
  target_include_directories(ka_gdal_log_tests PRIVATE src ${KA_INCLUDES})
  target_link_directories(ka_gdal_log_tests PRIVATE ${KA_LIBDIRS})
  target_link_libraries(ka_gdal_log_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_gdal_log_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(gdal_error_log ka_gdal_log_tests)
  # 도면을 좌표계를 알아내 참조 지도 제자리에 올리는 흐름(KaCadImport). 앱 창 대신 창·지도·알림 막대를 따로 만든다.
  add_executable(ka_cad_import_tests tests/test_cad_import.cpp
    src/app/KaCadImport.cpp src/app/KaCadImport.h src/app/KaCadImportNotice.cpp src/app/KaCadImportNotice.h
    src/app/KaBlockingTask.cpp src/app/KaBlockingTask.h
    src/app/KaUserError.cpp src/app/KaUserError.h)
  target_include_directories(ka_cad_import_tests PRIVATE src ${KA_INCLUDES})
  target_link_directories(ka_cad_import_tests PRIVATE ${KA_LIBDIRS})
  target_link_libraries(ka_cad_import_tests PRIVATE ka_core Qt6::Test)
  target_compile_definitions(ka_cad_import_tests PRIVATE "KA_TEST_CAD_DATA=\"${CMAKE_SOURCE_DIR}/tests/data/cad\"")
  set_target_properties(ka_cad_import_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(cad_import ka_cad_import_tests)
  # 도면 좌표계는 묻지 않고 단서로 정한다. 로컬 좌표 도면만 정합으로 간다(2026-10-03).
  add_executable(ka_cad_import_auto_tests tests/test_cad_import_auto.cpp
    src/app/KaCadImport.cpp src/app/KaCadImport.h src/app/KaCadImportNotice.cpp src/app/KaCadImportNotice.h
    src/app/KaBlockingTask.cpp src/app/KaBlockingTask.h
    src/app/KaUserError.cpp src/app/KaUserError.h)
  target_include_directories(ka_cad_import_auto_tests PRIVATE src ${KA_INCLUDES})
  target_link_directories(ka_cad_import_auto_tests PRIVATE ${KA_LIBDIRS})
  target_link_libraries(ka_cad_import_auto_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_cad_import_auto_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(cad_import_auto ka_cad_import_auto_tests)
  # 무거운 일을 돌리는 동안의 진행 창(KaBlockingTask): 취소해도 일이 실제로 멈출 때까지 창을 둔다.
  add_executable(ka_blocking_task_tests tests/test_blocking_task.cpp src/app/KaBlockingTask.cpp src/app/KaBlockingTask.h)
  target_include_directories(ka_blocking_task_tests PRIVATE src ${KA_INCLUDES})
  target_link_directories(ka_blocking_task_tests PRIVATE ${KA_LIBDIRS})
  target_link_libraries(ka_blocking_task_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_blocking_task_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(blocking_task ka_blocking_task_tests)
  # 도형을 그리고 이름·번호를 넣으면 Ctrl+Z 한 번에 둘 다 사라지게 묶는 KaUndoGroup(헤더만).
  add_executable(ka_undo_group_tests tests/test_undo_group.cpp src/app/KaUndoGroup.h)
  target_include_directories(ka_undo_group_tests PRIVATE src ${KA_INCLUDES})
  target_link_directories(ka_undo_group_tests PRIVATE ${KA_LIBDIRS})
  target_link_libraries(ka_undo_group_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_undo_group_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(undo_group ka_undo_group_tests)
  # 「다른 이름으로 저장」이 원본 조사 파일을 바꾸지 않고 저장하지 않은 편집을 새 파일에만 쓴다(SurveySaveAs).
  add_executable(ka_survey_save_as_tests tests/test_survey_save_as.cpp)
  target_include_directories(ka_survey_save_as_tests PRIVATE src ${KA_INCLUDES})
  target_link_directories(ka_survey_save_as_tests PRIVATE ${KA_LIBDIRS})
  target_link_libraries(ka_survey_save_as_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_survey_save_as_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(survey_save_as ka_survey_save_as_tests)
  add_executable(ka_shell_tests tests/test_shell.cpp
    src/app/KaShellUi.cpp src/app/KaShellUi.h
    src/app/KaShellStartup.cpp src/app/KaShellStartup.h
    src/app/KaShellGridControls.cpp src/app/KaShellGridControls.h
    src/app/KaShellFocus.cpp src/app/KaShellFocus.h
    src/app/KaTheme.cpp src/app/KaTheme.h src/app/KaThemeSheet.cpp src/app/KaThemeStyle.cpp
    src/app/KaUserError.cpp src/app/KaUserError.h
    src/app/KaCrashGuard.cpp src/app/KaCrashGuard.h)
  target_include_directories(ka_shell_tests PRIVATE src ${KA_INCLUDES} ${CMAKE_BINARY_DIR}/generated)
  target_link_directories(ka_shell_tests PRIVATE ${KA_LIBDIRS})
  target_link_libraries(ka_shell_tests PRIVATE ka_core Qt6::Test Qt6::Widgets)
  set_target_properties(ka_shell_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(shell_chrome ka_shell_tests)
  add_executable(ka_catch_log_tests tests/test_catch_log.cpp src/core/KaSessionLog.cpp ${KA_HGIS_GIT_HASH_CPP})
  set_target_properties(ka_catch_log_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_include_directories(ka_catch_log_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
  target_link_libraries(ka_catch_log_tests PRIVATE Qt6::Test Qt6::Core)
  ka_add_qtest(catch_log ka_catch_log_tests)
  add_executable(ka_log_session_tests tests/test_log_session.cpp src/core/KaSessionLog.cpp ${KA_HGIS_GIT_HASH_CPP})
  set_target_properties(ka_log_session_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_include_directories(ka_log_session_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
  target_link_libraries(ka_log_session_tests PRIVATE Qt6::Test Qt6::Core)
  ka_add_qtest(log_session ka_log_session_tests)
  add_executable(ka_export_survey_areas_tests tests/test_export_survey_areas.cpp)
  target_link_libraries(ka_export_survey_areas_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_export_survey_areas_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(export_survey_areas ka_export_survey_areas_tests)
  get_target_property(KA_PERF_SOURCES ka-hgis SOURCES)
  list(REMOVE_ITEM KA_PERF_SOURCES src/app/main.cpp src/app/KaApplication.cpp src/app/app.rc)
  add_executable(ka_perf_tests tests/test_perf.cpp ${KA_PERF_SOURCES})
  set_target_properties(ka_perf_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_perf_tests PRIVATE ka_core Qt6::Test Qt6::Svg Qt6::WebEngineWidgets)
  target_include_directories(ka_perf_tests PRIVATE ${KA_INCLUDES} ${CMAKE_BINARY_DIR}/generated)
  target_compile_options(ka_perf_tests PRIVATE /utf-8 /FS)
  ka_add_qtest(perf_engine ka_perf_tests)
  # 시간을 재는 시험이라 다른 시험과 함께 돌지 않는다.
  set_tests_properties(perf_engine PROPERTIES TIMEOUT 300 RUN_SERIAL TRUE)
  add_executable(ka_map_geotiff_export_tests tests/test_map_geotiff_export.cpp)
  target_link_libraries(ka_map_geotiff_export_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_map_geotiff_export_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(map_geotiff_export ka_map_geotiff_export_tests)
  add_executable(ka_checklist_rules_tests tests/test_checklist_rules.cpp)
  target_link_libraries(ka_checklist_rules_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_checklist_rules_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(checklist_rules ka_checklist_rules_tests)
  add_executable(ka_checklist_state_tests tests/test_checklist_state.cpp)
  target_link_libraries(ka_checklist_state_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_checklist_state_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(checklist_state ka_checklist_state_tests)
  add_executable(ka_submit_package_tests tests/test_submit_package.cpp)
  target_link_libraries(ka_submit_package_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_submit_package_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(submit_package ka_submit_package_tests)
  add_executable(ka_submit_dialog_tests tests/test_submit_dialog.cpp src/app/KaSubmitDialog.cpp src/app/KaSubmitDialog.h
    src/app/KaSubmitDialogPlate.cpp src/app/KaSubmitDrawingStrip.cpp src/app/KaSubmitDrawingStrip.h)
  target_link_libraries(ka_submit_dialog_tests PRIVATE ka_core Qt6::Test Qt6::Widgets)
  target_include_directories(ka_submit_dialog_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
  set_target_properties(ka_submit_dialog_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(submit_dialog ka_submit_dialog_tests)
  target_link_libraries(ka_heritage_import_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_heritage_import_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(heritage_import ka_heritage_import_tests)
  add_executable(ka_survey_scope_clip_tests tests/test_survey_scope_clip.cpp)
  target_link_libraries(ka_survey_scope_clip_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_survey_scope_clip_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(survey_scope_clip ka_survey_scope_clip_tests)
  # 세부 디자인 보완(창 항목 D3): 실패 알림이 한 틀이다(소스 글을 읽는 시험).
  add_executable(ka_alert_polish_tests tests/test_alert_polish.cpp)
  target_link_libraries(ka_alert_polish_tests PRIVATE Qt6::Test)
  set_target_properties(ka_alert_polish_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(alert_polish ka_alert_polish_tests)
  add_executable(ka_polygon_erase_tests tests/test_polygon_erase.cpp)
  target_link_libraries(ka_polygon_erase_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_polygon_erase_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(polygon_erase ka_polygon_erase_tests)
  # 혼자 약 40초: 4개씩 함께 돌면 60초를 넘겼다(2026-10-03 전체 실행).
  set_tests_properties(polygon_erase PROPERTIES TIMEOUT 180)
  add_executable(ka_heritage_download_retry_tests tests/test_heritage_download_retry.cpp
    src/app/KaHeritageBrowser.cpp src/app/KaHeritageBrowser.h)
  target_link_libraries(ka_heritage_download_retry_tests PRIVATE ka_core Qt6::Test Qt6::WebEngineWidgets)
  set_target_properties(ka_heritage_download_retry_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(heritage_download_retry ka_heritage_download_retry_tests)
  add_executable(ka_heritage_form_tests tests/test_heritage_form.cpp)
  target_link_libraries(ka_heritage_form_tests PRIVATE ka_core Qt6::Test Qt6::Network)
  set_target_properties(ka_heritage_form_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(heritage_form ka_heritage_form_tests)
  add_executable(ka_heritage_style_tests tests/test_heritage_style.cpp)
  target_link_libraries(ka_heritage_style_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_heritage_style_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(heritage_style ka_heritage_style_tests)

  add_executable(ka_layout_badge_placer_tests tests/test_layout_badge_placer.cpp)
  target_link_libraries(ka_layout_badge_placer_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_layout_badge_placer_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(layout_badge_placer ka_layout_badge_placer_tests)
  add_executable(ka_layout_sheet_rules_tests tests/test_layout_sheet_rules.cpp)
  target_link_libraries(ka_layout_sheet_rules_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_layout_sheet_rules_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(layout_sheet_rules ka_layout_sheet_rules_tests)
  set_tests_properties(layout_sheet_rules PROPERTIES TIMEOUT 120)
  # 지도 좌표격자를 도면에 (user request 2026-10-20).
  add_executable(ka_layout_map_grid_tests tests/test_layout_map_grid.cpp)
  target_link_libraries(ka_layout_map_grid_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_layout_map_grid_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(layout_map_grid ka_layout_map_grid_tests)
  set_tests_properties(layout_map_grid PROPERTIES TIMEOUT 120)
  add_executable(ka_heritage_flow_tests tests/test_heritage_flow.cpp)
  target_link_libraries(ka_heritage_flow_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_heritage_flow_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(heritage_flow ka_heritage_flow_tests)
  add_executable(ka_webengine_smoke_tests tests/test_webengine_smoke.cpp)
  set_target_properties(ka_webengine_smoke_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_webengine_smoke_tests PRIVATE ka_core Qt6::Test Qt6::Widgets Qt6::WebEngineWidgets)
  ka_add_qtest(webengine_smoke ka_webengine_smoke_tests)
  add_executable(ka_heritage_region_tests tests/test_heritage_region.cpp)
  target_link_libraries(ka_heritage_region_tests PRIVATE ka_core Qt6::Test Qt6::Network)
  set_target_properties(ka_heritage_region_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(heritage_region ka_heritage_region_tests)
  add_executable(ka_heritage_fetch_plan_tests tests/test_heritage_fetch_plan.cpp)
  target_link_libraries(ka_heritage_fetch_plan_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_heritage_fetch_plan_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(heritage_fetch_plan ka_heritage_fetch_plan_tests)
  add_executable(ka_account_status_tests tests/test_account_status.cpp)
  target_link_libraries(ka_account_status_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_account_status_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(account_status ka_account_status_tests)
  add_executable(ka_layer_state_tests tests/test_layer_state.cpp)
  add_executable(ka_layer_label_tests tests/test_layer_labels.cpp)
  target_link_libraries(ka_layer_label_tests PRIVATE ka_core Qt6::Test)
  ka_add_qtest(layer_labels ka_layer_label_tests)
  add_executable(ka_layer_information_tests tests/test_layer_information.cpp
    src/app/KaLayerInformation.cpp src/app/KaLayerInformation.h src/app/KaLayerInformationPanel.cpp)
  target_link_libraries(ka_layer_information_tests PRIVATE ka_core Qt6::Test)
  ka_add_qtest(layer_information ka_layer_information_tests)
  set_tests_properties(layer_labels PROPERTIES TIMEOUT 90)
  # 22 steps at about 2.5 s each when TEMP is under build/ (A:): 90 s ran out under -j3.
  set_tests_properties(layer_information PROPERTIES TIMEOUT 150)
  set_target_properties(ka_layer_label_tests ka_layer_information_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  add_executable(ka_layer_roles_tests tests/test_layer_roles.cpp)
  target_link_libraries(ka_layer_roles_tests PRIVATE ka_core Qt6::Test)
  ka_add_qtest(layer_roles ka_layer_roles_tests)
  add_executable(ka_layer_styles_tests tests/test_layer_styles.cpp)
  target_link_libraries(ka_layer_styles_tests PRIVATE ka_core Qt6::Test)
  ka_add_qtest(layer_styles ka_layer_styles_tests)
  add_executable(ka_layer_edits_tests tests/test_layer_edits.cpp)
  target_link_libraries(ka_layer_edits_tests PRIVATE ka_core Qt6::Test)
  ka_add_qtest(layer_edits ka_layer_edits_tests)
  add_executable(ka_layer_panel_tests tests/test_layer_panel.cpp
    src/app/KaLayerInformation.cpp src/app/KaLayerInformation.h src/app/KaLayerInformationPanel.cpp)
  target_link_libraries(ka_layer_panel_tests PRIVATE ka_core Qt6::Test)
  ka_add_qtest(layer_panel ka_layer_panel_tests)
  add_executable(ka_layer_tree_undo_tests tests/test_layer_tree_undo.cpp
    src/app/KaLayerTreeUndo.cpp src/app/KaLayerTreeUndo.h src/app/KaLayerRefreshBatch.cpp src/app/KaLayerRefreshBatch.h)
  target_link_libraries(ka_layer_tree_undo_tests PRIVATE ka_core Qt6::Test)
  ka_add_qtest(layer_tree_undo ka_layer_tree_undo_tests)
  set_target_properties(ka_layer_roles_tests ka_layer_styles_tests ka_layer_edits_tests ka_layer_panel_tests ka_layer_tree_undo_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  set_tests_properties(layer_roles layer_styles layer_edits layer_panel layer_tree_undo PROPERTIES TIMEOUT 90)
  add_executable(ka_home_tests tests/test_home.cpp
    src/app/KaStartPage.cpp src/app/KaStartPage.h src/app/KaHomeTitleBlock.cpp src/app/KaHomeTitleBlock.h
    src/app/KaStartHero.cpp src/app/KaStartHero.h
    src/app/KaSplashArt.cpp
    src/app/KaHomeRecentCard.cpp src/app/KaHomeRecentCard.h
    src/app/KaHomeStatus.cpp src/app/KaHomeStatus.h
    src/app/KaHomeText.cpp
    src/app/KaNewSurveyDialog.cpp
    src/app/KaLongitudeRuler.cpp
    src/app/KaRecoverySnapshots.cpp
    src/app/KaTheme.cpp src/app/KaThemeSheet.cpp src/app/KaThemeStyle.cpp)
  target_link_libraries(ka_home_tests PRIVATE ${KA_TEST_LIBS} Qt6::Network)
  target_include_directories(ka_home_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  target_compile_definitions(ka_home_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_home_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(home_start ka_home_tests)
  set_tests_properties(home_start PROPERTIES TIMEOUT 90)
  add_executable(ka_dem_legend_tests tests/test_dem_legend.cpp)
  add_executable(ka_topographic_catalog_tests tests/test_topographic_catalog.cpp)
  add_executable(ka_topographic_archive_tests tests/test_topographic_archive.cpp)
  add_executable(ka_topographic_source_crs_tests tests/test_topographic_source_crs.cpp)
  add_executable(ka_topographic_shapefile_tests tests/test_topographic_shapefile.cpp)
  target_link_libraries(ka_topographic_shapefile_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_topographic_shapefile_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(topographic_shapefile ka_topographic_shapefile_tests)
  add_executable(ka_topographic_settings_tests tests/test_topographic_settings.cpp)
  target_link_libraries(ka_topographic_settings_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_topographic_settings_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(topographic_settings ka_topographic_settings_tests)
  add_executable(ka_topographic_account_dialog_tests tests/test_topographic_account_dialog.cpp
    src/app/KaTopographicAccountDialog.cpp src/app/KaTopographicAccountDialog.h)
  target_link_libraries(ka_topographic_account_dialog_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_topographic_account_dialog_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(topographic_account_dialog ka_topographic_account_dialog_tests)
  add_executable(ka_heritage_setup_dialogs_tests tests/test_heritage_setup_dialogs.cpp
    src/app/KaHeritageSetupDialogs.cpp src/app/KaHeritageSetupDialogs.h)
  target_link_libraries(ka_heritage_setup_dialogs_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_heritage_setup_dialogs_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(heritage_setup_dialogs ka_heritage_setup_dialogs_tests)
  target_link_libraries(ka_topographic_source_crs_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_topographic_source_crs_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(topographic_source_crs ka_topographic_source_crs_tests)
  add_executable(ka_topographic_sheets_tests tests/test_topographic_sheets.cpp)
  target_link_libraries(ka_topographic_sheets_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_topographic_sheets_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(topographic_sheets ka_topographic_sheets_tests)
  target_link_libraries(ka_topographic_archive_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_topographic_archive_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(topographic_archive ka_topographic_archive_tests)
  target_link_libraries(ka_topographic_catalog_tests PRIVATE ka_core Qt6::Test)
  ka_add_qtest(topographic_catalog ka_topographic_catalog_tests)
  add_executable(ka_topographic_browser_tests tests/test_topographic_browser.cpp
    data/providers/ngii.qrc
    src/app/KaTopographicBrowser.cpp src/app/KaTopographicBrowser.h src/app/KaStageRail.cpp src/app/KaStageRail.h)
  target_include_directories(ka_topographic_browser_tests PRIVATE src)
  set_target_properties(ka_topographic_browser_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_topographic_browser_tests PRIVATE ka_core Qt6::Test Qt6::WebEngineWidgets Qt6::Network)
  if(MSVC)
    target_compile_options(ka_topographic_browser_tests PRIVATE /utf-8)
  endif()
  ka_add_qtest(topographic_browser ka_topographic_browser_tests)
  add_executable(ka_topographic_import_tests tests/test_topographic_import.cpp
    src/app/KaTopographicImportDialog.cpp src/app/KaTopographicImportDialog.h
    src/app/KaCrashGuard.cpp src/app/KaCrashGuard.h)
  target_link_libraries(ka_topographic_import_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_topographic_import_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(topographic_import ka_topographic_import_tests)
  add_executable(ka_topographic_merge_tests tests/test_topographic_merge.cpp
    src/app/KaTopographicImportDialog.cpp src/app/KaTopographicImportDialog.h
    src/app/KaCrashGuard.cpp src/app/KaCrashGuard.h)
  target_link_libraries(ka_topographic_merge_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_topographic_merge_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(topographic_merge ka_topographic_merge_tests)
  set_tests_properties(topographic_merge PROPERTIES TIMEOUT 180)
  add_executable(ka_topographic_scope_tests tests/test_topographic_scope.cpp
    data/providers/ngii.qrc
    src/app/KaTopographicScopePanel.cpp src/app/KaTopographicScopePanel.h
    src/app/KaTopographicBrowser.cpp src/app/KaTopographicBrowser.h src/app/KaStageRail.cpp src/app/KaStageRail.h
    src/app/KaTopographicImportDialog.cpp src/app/KaTopographicImportDialog.h
    src/app/KaCrashGuard.cpp src/app/KaCrashGuard.h)
  target_link_libraries(ka_topographic_scope_tests PRIVATE ka_core Qt6::Test Qt6::WebEngineWidgets)
  set_target_properties(ka_topographic_scope_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(topographic_scope ka_topographic_scope_tests)
  foreach(_ka_download_target IN ITEMS ka_heritage_download_retry_tests ka_topographic_browser_tests ka_topographic_scope_tests)
    target_sources(${_ka_download_target} PRIVATE src/app/KaDownloadUi.cpp src/app/KaDownloadUi.h)
  endforeach()

  add_executable(ka_startup_splash_tests tests/test_startup_splash.cpp
    data/theme/icons.qrc
    src/app/KaStartupSplash.cpp src/app/KaStartupSplash.h
    src/app/KaSplashPalette.h src/app/KaSplashCredits.cpp src/app/KaSplashCredits.h src/app/KaSplashArt.cpp src/app/KaSplashArt.h
    src/app/KaSplashStrata.cpp src/app/KaSplashStrata.h)
  target_include_directories(ka_startup_splash_tests PRIVATE src)
  target_link_libraries(ka_startup_splash_tests PRIVATE Qt6::Test Qt6::Widgets)
  set_target_properties(ka_startup_splash_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(startup_splash ka_startup_splash_tests)
  set_tests_properties(startup_splash PROPERTIES TIMEOUT 45)
  # 새 모양 지층 시작 화면 (tests/test_startup_splash.cpp stays under 300 lines).
  add_executable(ka_startup_strata_tests tests/test_startup_strata.cpp
    data/theme/icons.qrc
    src/app/KaStartupSplash.cpp src/app/KaStartupSplash.h
    src/app/KaSplashPalette.h src/app/KaSplashCredits.cpp src/app/KaSplashCredits.h src/app/KaSplashArt.cpp src/app/KaSplashArt.h
    src/app/KaSplashStrata.cpp src/app/KaSplashStrata.h)
  target_include_directories(ka_startup_strata_tests PRIVATE src)
  target_link_libraries(ka_startup_strata_tests PRIVATE Qt6::Test Qt6::Widgets)
  set_target_properties(ka_startup_strata_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(startup_strata ka_startup_strata_tests)
  set_tests_properties(startup_strata PROPERTIES TIMEOUT 45)

  add_executable(ka_download_ui_tests tests/test_download_ui.cpp
    src/app/KaDownloadUi.cpp src/app/KaDownloadUi.h
    src/app/KaTopographicBrowser.cpp src/app/KaTopographicBrowser.h src/app/KaStageRail.cpp src/app/KaStageRail.h
    src/app/KaHeritageBrowser.cpp src/app/KaHeritageBrowser.h)
  target_link_libraries(ka_download_ui_tests PRIVATE ka_core Qt6::Test Qt6::WebEngineWidgets)
  set_target_properties(ka_download_ui_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(download_ui ka_download_ui_tests)
  target_sources(ka_heritage_download_retry_tests PRIVATE src/app/KaHeritageBrowserPlan.cpp)
  target_sources(ka_download_ui_tests PRIVATE src/app/KaHeritageBrowserPlan.cpp)
  set_tests_properties(download_ui PROPERTIES TIMEOUT 90)
  add_executable(ka_tile_print_tests tests/test_tile_print.cpp
    src/app/KaPrintDialog.cpp src/app/KaPrintDialogPlate.cpp src/app/KaTilePreview.cpp src/app/KaPrintDialog.h
    src/app/KaTilePreview.h src/app/KaUserError.cpp src/app/KaUserError.h)
  target_include_directories(ka_tile_print_tests PRIVATE src)
  target_link_libraries(ka_tile_print_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_tile_print_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(tile_print ka_tile_print_tests)
  set_tests_properties(tile_print PROPERTIES TIMEOUT 90)
  # 「조용한 도판」 도면 인쇄: two columns, the dotted ledger, 「A4 | A3」 words (tests/test_tile_print.cpp is full).
  add_executable(ka_print_plate_tests tests/test_print_plate.cpp src/app/KaProgressPlate.cpp src/app/KaProgressPlate.h
    src/app/KaPrintDialog.cpp src/app/KaPrintDialogPlate.cpp src/app/KaTilePreview.cpp src/app/KaPrintDialog.h
    src/app/KaTilePreview.h src/app/KaUserError.cpp src/app/KaUserError.h)
  target_include_directories(ka_print_plate_tests PRIVATE src)
  target_link_libraries(ka_print_plate_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_print_plate_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(print_plate ka_print_plate_tests)
  # 「조용한 도판」 도면 (board 4): the output buttons at the foot of the right panel (no QGIS).
  add_executable(ka_drawing_plate_tests tests/test_drawing_plate.cpp
    src/app/KaDrawingStudioPlate.cpp src/app/KaDrawingStudioPlate.h src/app/KaFileRowDelegate.cpp src/app/KaFileRowDelegate.h)
  target_include_directories(ka_drawing_plate_tests PRIVATE src)
  target_link_libraries(ka_drawing_plate_tests PRIVATE Qt6::Test Qt6::Widgets)
  set_target_properties(ka_drawing_plate_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(drawing_plate ka_drawing_plate_tests)
  # 「조용한 도판」 받는 순서 rail of the download windows (board 5, no QGIS).
  add_executable(ka_stage_rail_tests tests/test_stage_rail.cpp src/app/KaStageRail.cpp src/app/KaStageRail.h)
  target_include_directories(ka_stage_rail_tests PRIVATE src)
  target_link_libraries(ka_stage_rail_tests PRIVATE Qt6::Test Qt6::Widgets)
  set_target_properties(ka_stage_rail_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(stage_rail ka_stage_rail_tests)
  set_tests_properties(stage_rail PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
  set_tests_properties(drawing_plate PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
  add_executable(ka_drawing_studio_tests tests/test_drawing_studio.cpp
    src/app/KaDrawingNorthArrow.cpp src/app/KaDrawingNorthArrow.h
    src/app/KaDrawingCoordCallout.cpp src/app/KaDrawingCoordCallout.h
    src/app/KaDrawingLayerSets.cpp src/app/KaDrawingLayerSets.h
    src/app/KaDrawingSheetSet.cpp src/app/KaDrawingSheetSet.h
    src/app/KaDrawingPdfProgress.cpp src/app/KaDrawingPdfProgress.h src/app/KaProgressPlate.cpp src/app/KaProgressPlate.h
    src/app/KaTitleBlock.cpp src/app/KaTitleBlock.h)
  target_include_directories(ka_drawing_studio_tests PRIVATE src)
  target_link_libraries(ka_drawing_studio_tests PRIVATE ka_core Qt6::Test Qt6::Svg)
  set_target_properties(ka_drawing_studio_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  if (MSVC)
    target_compile_options(ka_drawing_studio_tests PRIVATE /utf-8)
  endif()
  ka_add_qtest(drawing_studio ka_drawing_studio_tests)
  set_tests_properties(drawing_studio PROPERTIES TIMEOUT 90)
  add_executable(ka_dem_rendering_tests tests/test_dem_rendering.cpp)
  add_executable(ka_dem_presentation_tests tests/test_dem_presentation.cpp src/app/KaDemClassDialog.cpp src/app/KaDemClassDialog.h)
  target_link_libraries(ka_dem_presentation_tests PRIVATE ka_core Qt6::Test)
  ka_add_qtest(dem_presentation ka_dem_presentation_tests)
  target_link_libraries(ka_dem_rendering_tests PRIVATE ka_core Qt6::Test)
  ka_add_qtest(dem_rendering ka_dem_rendering_tests)
  target_link_libraries(ka_dem_legend_tests PRIVATE ka_core Qt6::Test)
  ka_add_qtest(dem_legend ka_dem_legend_tests)
  set_target_properties(ka_layer_state_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_layer_state_tests PRIVATE ka_core Qt6::Test)
  ka_add_qtest(layer_state_regressions ka_layer_state_tests)

  add_executable(ka_map_render_tests tests/test_map_render.cpp)
  set_target_properties(ka_map_render_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_map_render_tests PRIVATE ka_core Qt6::Test Qt6::Network)
  ka_add_qtest(map_render ka_map_render_tests)
  add_executable(ka_vworld_settings_tests tests/test_vworld_settings.cpp)
  set_target_properties(ka_vworld_settings_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_vworld_settings_tests PRIVATE ka_core Qt6::Test)
  ka_add_qtest(vworld_settings ka_vworld_settings_tests)
  add_executable(ka_basemap_reference_tests tests/test_basemap_reference.cpp)
  set_target_properties(ka_basemap_reference_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_basemap_reference_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_basemap_reference_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_basemap_reference_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_basemap_reference_tests PRIVATE ${KA_TEST_DEFS})
  ka_add_qtest(basemap_reference ka_basemap_reference_tests)
  set_tests_properties(vworld_settings log_session PROPERTIES TIMEOUT 60)
  set_tests_properties(basemap_reference PROPERTIES TIMEOUT 120)
  add_executable(ka_parallel_render_tests tests/test_parallel_render.cpp)
  set_target_properties(ka_parallel_render_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_parallel_render_tests PRIVATE ka_core Qt6::Test Qt6::Network)
  ka_add_qtest(parallel_render ka_parallel_render_tests)
  set_tests_properties(region_locator_popup layer_state_regressions map_render PROPERTIES TIMEOUT 60)
  set_tests_properties(parallel_render PROPERTIES TIMEOUT 90)

  add_executable(ka_hgis_above_labels_tests tests/test_above_labels.cpp src/app/KaAboveLabelsOverlay.cpp)
  set_target_properties(ka_hgis_above_labels_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_hgis_above_labels_tests PRIVATE ka_core Qt6::Test)
  ka_add_qtest(above_labels ka_hgis_above_labels_tests)
  ka_add_qtest(above_labels_125 ka_hgis_above_labels_tests)
  ka_add_qtest(above_labels_150 ka_hgis_above_labels_tests)
  ka_add_qtest(above_labels_200 ka_hgis_above_labels_tests)
  set_tests_properties(above_labels above_labels_125 above_labels_150 above_labels_200 PROPERTIES TIMEOUT 60)
  add_executable(ka_hgis_above_labels_names_tests tests/test_above_labels_names.cpp src/app/KaAboveLabelsOverlay.cpp)
  set_target_properties(ka_hgis_above_labels_names_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_hgis_above_labels_names_tests PRIVATE ka_core Qt6::Test)
  ka_add_qtest(above_labels_names ka_hgis_above_labels_names_tests)
  set_tests_properties(above_labels_names PROPERTIES TIMEOUT 60)

  add_executable(ka_recent_tests
    tests/test_recent.cpp
    src/core/RecentSurveys.cpp
  )
  set_target_properties(ka_recent_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_recent_tests PRIVATE Qt6::Test Qt6::Core)
  target_include_directories(ka_recent_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
  ka_add_qtest(recent_surveys ka_recent_tests)

  add_executable(ka_network_tests tests/test_network_services.cpp)
  target_link_libraries(ka_network_tests PRIVATE ka_core Qt6::Test Qt6::Network)
  ka_add_qtest(network_services ka_network_tests)
  set_tests_properties(network_services PROPERTIES TIMEOUT 30)

  add_executable(ka_storage_safety_tests tests/test_storage_safety.cpp)
  target_link_libraries(ka_storage_safety_tests PRIVATE ka_core Qt6::Test)
  ka_add_qtest(storage_safety ka_storage_safety_tests)
  # 이 PC 약 50초, GitHub 무료 기계에서는 120초를 넘겼다(2026-10-02 CI).
  set_tests_properties(storage_safety PROPERTIES TIMEOUT 300)
  add_executable(ka_storage_generation_tests tests/test_storage_generation.cpp)
  target_link_libraries(ka_storage_generation_tests PRIVATE ka_core Qt6::Test)
  ka_add_qtest(storage_generation ka_storage_generation_tests)
  # 앱 안에 남은 손잡이 때문에 저장이 원본을 바꾸지 못하고 「-저장.gpkg」로 빠지던 일(R67).
  add_executable(ka_save_handles_tests tests/test_save_handles.cpp)
  target_include_directories(ka_save_handles_tests PRIVATE src ${KA_INCLUDES})
  target_link_directories(ka_save_handles_tests PRIVATE ${KA_LIBDIRS})
  target_link_libraries(ka_save_handles_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_save_handles_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(save_handles ka_save_handles_tests)
  # 아직 열린 파일은 잠시 뒤 다시 지운다: 같은 도면을 다시 불러오면 옛 변환본이 남던 일(R89).
  add_executable(ka_file_cleanup_tests tests/test_file_cleanup.cpp)
  target_include_directories(ka_file_cleanup_tests PRIVATE src ${KA_INCLUDES})
  target_link_directories(ka_file_cleanup_tests PRIVATE ${KA_LIBDIRS})
  target_link_libraries(ka_file_cleanup_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_file_cleanup_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(file_cleanup ka_file_cleanup_tests)
  # 화면 글자의 용어 결정: 「조판」은 「도면」(R87). 소스 문자열과 점검 규칙을 읽어 본다.
  add_executable(ka_ui_terms_tests tests/test_ui_terms.cpp)
  target_link_libraries(ka_ui_terms_tests PRIVATE Qt6::Core Qt6::Test)
  target_compile_definitions(ka_ui_terms_tests PRIVATE "KA_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"")
  set_target_properties(ka_ui_terms_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(ui_terms ka_ui_terms_tests)
  # Six write-through generation saves take ~108 s alone on the field PC (13-23 s each);
  # 120 s left no room for a parallel ctest run.
  set_tests_properties(storage_generation PROPERTIES TIMEOUT 300)
  set_tests_properties(save_handles PROPERTIES TIMEOUT 300)

  add_executable(ka_survey_durability_tests tests/test_survey_durability.cpp)
  target_link_libraries(ka_survey_durability_tests PRIVATE ka_core Qt6::Test)
  ka_add_qtest(survey_durability ka_survey_durability_tests)
  # Two survey copies plus the product's 10 s replace retry window: about 40 s alone.
  set_tests_properties(survey_durability PROPERTIES TIMEOUT 180)

  add_executable(ka_storage_hygiene_tests tests/test_storage_hygiene.cpp)
  target_link_libraries(ka_storage_hygiene_tests PRIVATE ka_core Qt6::Test)
  ka_add_qtest(storage_hygiene ka_storage_hygiene_tests)
  set_tests_properties(storage_hygiene PROPERTIES TIMEOUT 60)

  add_executable(ka_reference_download_tests tests/test_reference_download.cpp
    src/app/KaReferenceDownloadJob.cpp
    src/app/KaCrashGuard.cpp src/app/KaCrashGuard.h)
  target_link_libraries(ka_reference_download_tests PRIVATE ka_core Qt6::Test Qt6::Network)
  ka_add_qtest(reference_download ka_reference_download_tests)
  set_tests_properties(reference_download PROPERTIES TIMEOUT 60)
  add_executable(ka_reference_status_tests tests/test_reference_status.cpp
    src/app/KaReferenceStatusDialog.cpp src/app/KaReferenceStatusDialog.h)
  target_link_libraries(ka_reference_status_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_reference_status_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_reference_status_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_reference_status_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_reference_status_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(reference_status ka_reference_status_tests)
  # 새 모양 자료 준비 상태 (2차 보드 판 5); tests/test_reference_status.cpp is near 300 lines.
  add_executable(ka_reference_status_plate_tests tests/test_reference_status_plate.cpp
    src/app/KaReferenceStatusDialog.cpp src/app/KaReferenceStatusDialog.h)
  target_link_libraries(ka_reference_status_plate_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_reference_status_plate_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_reference_status_plate_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_reference_status_plate_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_reference_status_plate_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(reference_status_plate ka_reference_status_plate_tests)
  set_tests_properties(reference_status_plate PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 60)

  add_executable(ka_cadastral_tests tests/test_cadastral.cpp)
  target_link_libraries(ka_cadastral_tests PRIVATE ka_core Qt6::Test Qt6::Network)
  ka_add_qtest(cadastral ka_cadastral_tests)
  set_tests_properties(cadastral PROPERTIES TIMEOUT 90)

  # Exercise the actual window's save/open slots, including project signal callbacks.
  get_target_property(KA_SAVE_OPEN_SOURCES ka-hgis SOURCES)
  list(REMOVE_ITEM KA_SAVE_OPEN_SOURCES src/app/main.cpp src/app/KaApplication.cpp src/app/app.rc)
  add_executable(ka_save_open_tests tests/test_save_open.cpp ${KA_SAVE_OPEN_SOURCES})
  set_target_properties(ka_save_open_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_save_open_tests PRIVATE ka_core Qt6::Test Qt6::Svg Qt6::WebEngineWidgets)
  target_include_directories(ka_save_open_tests PRIVATE ${KA_INCLUDES} ${CMAKE_BINARY_DIR}/generated)
  target_compile_definitions(ka_save_open_tests PRIVATE "KA_HGIS_VERSION=\"${PROJECT_VERSION}\"")
  if(MSVC)
    target_compile_options(ka_save_open_tests PRIVATE /utf-8 /FS)
  endif()
  # One exe, several CTest names. QtTest function filter:
  # https://doc.qt.io/qt-6/qtest-overview.html
  ka_add_qtest_filter(save_open_window ka_save_open_tests
    compactLayerListKeepsHierarchyAndMapLabelSize
    layerInformationControlsIntegrateWithMapAndDrawingStudio
    geoTiffButtonOnlyEnabledOnMapTab
    windowGeometryFitsLogicalMonitors
    mainWindowFitsAvailableScreenOnShow
    everyTabFitsA1920ScreenInPaper
    homePageShowsStrataHeroWithoutOpeningASurvey
    mapControlsZoomTheCanvasAndShowAScaleBar
    drawingToolsGetTheirOwnRowBelowTheRibbon
    firstStartIsMaximizedAndAChosenSizeIsKept
    filesBoxStartsOpenAndKeepsTheChosenState
    narrowWindowKeepsSearchOnTheRibbonRow
    ribbonButtonsAllHaveDifferentIcons
    ribbonGroupsMatchTheirPurpose
    narrowWindowKeepsDrawingAndPrintVisible
    subToolbarFitsAt1904WhileSketching
    unsavedSaveChipLabelTurnsBoldAndBack
    recordToolsWaitForASurvey
    provinceChipMovesMapWithoutLoadingLayers)
  ka_add_qtest_filter(save_open_topo ka_save_open_tests
    topographicActualWindowRemainsStableAfterCompletion
    topographicDownloadKeepsMapVisibleAndUsesSurveyFolder)
  ka_add_qtest_filter(save_open_drawing ka_save_open_tests
    drawingStudio_decorationsMoveAndShrink
    drawingStudio_legendStaysWhereTheUserPutsItOutsideThePaper
    drawingStudio_northArrowRendersOnOpen
    drawingStudio_fieldPageGrowsA4LandscapeByOneCentimetre
    drawingStudio_paperChangePutsChromeBackInTheStandardStrip
    drawingStudio_heritageRefreshRequestsCoalesceAndReuseOnRevisit
    drawingStudioAboveLabelsMapDrawsGeometryWithoutDuplicateNumberLabels
    drawingStudio_centerLayerMovesAboveLabelsMapToo
    drawingStudio_largeScaleChipsApplyToMapAndInput
    drawingInspectorUsesHeightWithoutOverlapping
    drawingStudioPrintSplitsTheDrawingIntoSheets
    ribbonPrintOpensTheDrawingThenThePrintWindow
    scaleControlFollowsActiveDrawingWithoutChangingMapCanvas
    scaleControl_isSingleWidgetAcceptingBothForms)
  # save_open_edit 은 한 묶음 70초(이 PC)라 GitHub 실행기(약 2.4배 느림)에서 180초 한도에 걸렸다(CI 38051687016).
  # 세 묶음으로 나눈다. 시험 함수는 그대로다.
  ka_add_qtest_filter(save_open_edit ka_save_open_tests
    layerContextMenu_matchesLayerKind
    changingLabelFontKeepsEveryLayerAndItsVisibility
    trenchPresetRespectsAreaAndDimensions)
  ka_add_qtest_filter(save_open_edit_delete ka_save_open_tests
    layerDeleteKeyPreservesSourceAndUndoRestoresPendingEdits
    layerDeleteKeyRemovesReferenceFromMap
    layerDeleteKeyRemovesCadastralAndKeepsItGone
    cadastralGroupDeleteKeyRemovesChildren
    cadastralGroupContextMenuCanRemove
    referenceGroupMenuRemovesEveryRowAndCtrlZRestores
    emptyGroupDeleteKeyRemovesRowAndCtrlZRestores)
  ka_add_qtest_filter(save_open_edit_draw ka_save_open_tests
    surveyAreaDrawingContinuesTheExistingAreaWithoutAWindow
    drawnShapeShowsItsRecordInTheInspector
    cardKeysLeaveTheSketchAlone
    ctrlZRestoresVertexEditsAndGroupedFeatureDeletion
    clearDrawnFeaturesLeavesOtherLayersSelectionAlone
    layerContextMenu_usesClickedRowAndLeavesSourceIntact
    layerContextMenu_moveToBottomPreservesRegisteredLayer
    mapContextMenu_onlyOffersRecordEditingForSurveyFeatures)
  ka_add_qtest_filter(save_open_erase ka_save_open_tests
    eraseOverlap_cutsTheShapeUnderTheSelectedOneAndCtrlZRestores
    eraseOverlap_acrossLayersUndoesAndRedoesTogether
    eraseOverlap_severalShapesAskBeforeRemovingThem
    eraseOverlap_secondPressUsesTheShapeJustDrawn
    drawnShapeOnTopOffersEraseWhenFinished
    selectTool_picksTheShapeDrawnOnTopAndKeyAStartsIt
    selectTool_rightClickErasesAndKeyAEndsSplitMode
    polygonCommands_leaveReferenceAndCadastralShapesAlone)
  ka_add_qtest_filter(save_open_roundtrip ka_save_open_tests
    saveAndReopen_keepsTreeGeometryAttributesAndStyle
    saveReopenSubmit_preservesWorkAndPackage
    save_keepsDrawnSurveyArea
    save_fieldPackageKeepsSurveyAreaReadable)
  ka_add_qtest_filter(save_open_open ka_save_open_tests
    open_repairsRegistryOnlyLayers
    startupHome_ignoresRememberedSurveyUntilUserOpensIt
    open_blocksNestedOpenAndAutosave
    emptyGpkgWithoutWorkspace_opensWithoutDomainLayers)
  ka_add_qtest_filter(save_open_open_invalid ka_save_open_tests
    invalidSurvey_doesNotReplaceCurrentWork
    openWhileDrawing_resetsCaptureBeforeReplacingLayers
    open_unsavedWorkRespectsAnswer)
  ka_add_qtest_filter(save_open_commit ka_save_open_tests
    partialCommit_preservesRemainingBufferAndCreatesIndependentRecovery
    failedCommit_doesNotOverwriteSavedWorkspace)
  # save_open_saveas 도 42초(이 PC)로 GitHub 에서 한도에 두 번 걸렸다. 두 묶음으로 나눈다.
  ka_add_qtest_filter(save_open_saveas ka_save_open_tests
    newSurvey_selectedCrsSurvivesSaveAndOpen
    embeddedOpenRestoresDemInActualTree
    saveAs_preservesActualTablesAndReopensCopy
    newSurvey_existingNamePreservesUnsavedCurrentSurvey
    closeSave_preservesMemoryReferenceVectorOnReopen)
  ka_add_qtest_filter(save_open_saveas_fail ka_save_open_tests
    saveAs_embeddedFailureRestoresSourcesAndRemainsUnsaved
    saveAs_existingDestinationIsNotOverwritten
    save_qgisExceptionStaysInsideSlot
    save_companionFailureKeepsEmbeddedWorkspaceAndWarns
    fieldCopy_restoresAllValidLayersAndSavesTree)
  ka_add_qtest_filter(save_open_portable ka_save_open_tests
    surveyReopensAfterUsbDriveOrFolderChange
    savedWorkspaceNeverPointsIntoTheSaveGenerationFolder
    newPortableFolderInheritsRecentSurveys
    saveAgainAfterAddingSurveyAreaKeepsEveryFeature
    oldVersionSurveysStillOpen
    samplesOpenAsSurveys)
  # 사용자 모니터(2293×912)와 1080p 작업 영역에서 홈·지도·도면이 잘리지 않는다(2026-10-10). 2293 폭이 들어가는
  # 가상 화면 cmake/offscreen-2560.json 에서 돈다(아래 ENVIRONMENT).
  ka_add_qtest_filter(save_open_monitor ka_save_open_tests
    monitorFitHomeMapDrawing
    monitorFitDialogs)
  set(KA_SAVE_OPEN_TESTS
    save_open_window save_open_topo save_open_drawing
    save_open_edit save_open_edit_delete save_open_edit_draw save_open_erase save_open_roundtrip save_open_open
    save_open_open_invalid save_open_commit save_open_saveas save_open_saveas_fail save_open_portable save_open_monitor)
  set_tests_properties(${KA_SAVE_OPEN_TESTS} PROPERTIES TIMEOUT 180 RESOURCE_LOCK ka_save_open)

  add_executable(ka_e2e_tests tests/test_e2e_opaque.cpp)
  set_target_properties(ka_e2e_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_e2e_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_e2e_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_e2e_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_e2e_tests PRIVATE ${KA_TEST_DEFS})
  if (MSVC)
    target_compile_options(ka_e2e_tests PRIVATE /utf-8 /W4)
  endif()
  ka_add_qtest(e2e_opaque_suite ka_e2e_tests)

  # Windows: ctest without OSGeo4W PATH → 0xc0000135 (DLL not found).
  # ENVIRONMENT is a CMake list, so unescaped ';' would truncate PATH to the
  # first bin dir. Escape path separators; keep ';' only between VAR=value.
  if(WIN32 AND EXISTS "${OSGEO4W_ROOT}/apps/qgis-dev/bin")
    set(_ka_path
      "${OSGEO4W_ROOT}/apps/qgis-dev/bin"
      "${OSGEO4W_ROOT}/apps/Qt6/bin"
      "${OSGEO4W_ROOT}/apps/gdal-dev/bin"
      "${OSGEO4W_ROOT}/apps/pdal-dev/bin"
      "${OSGEO4W_ROOT}/bin"
    )
    string(JOIN "\\;" _ka_path_esc ${_ka_path})
    set_tests_properties(layer_labels layer_information checklist_engine workflow_engine georef_engine feature_presets buffer_ring measure_tape dem_trench_engine theme_qss user_error recent_surveys terrain_3d_engine ${KA_SAVE_OPEN_TESTS} e2e_opaque_suite network_services storage_safety reference_download region_locator_popup layer_state_regressions map_render above_labels above_labels_125 above_labels_150 above_labels_200 PROPERTIES
      ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen")
    # A relative configfile: Qt splits the platform string at ':', so C:\... would break it (docs/ERROR_REGRESSION.md).
    set_tests_properties(save_open_monitor PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen:configfile=cmake/offscreen-2560.json")
    set_tests_properties(layer_roles layer_styles layer_edits layer_panel layer_tree_undo undo_group survey_save_as above_labels_names PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen")
    set_tests_properties(georef_quality georef_backup align_tool align_image_view PROPERTIES
      ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen")
    set_tests_properties(cad_dwg cad_reader cad_crs cad_hints cad_pending_copies cad_store cad_layers cad_import cad_import_auto cad_align blocking_task PROPERTIES
      ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 120)
    set_tests_properties(dem_legend PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
    set_tests_properties(topographic_catalog PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
    set_tests_properties(topographic_archive PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
    set_tests_properties(topographic_sheets PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
    set_tests_properties(topographic_source_crs PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
    set_tests_properties(topographic_shapefile topographic_settings topographic_account_dialog heritage_setup_dialogs PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
    set_tests_properties(topographic_scope PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen;QTWEBENGINEPROCESS_PATH=${OSGEO4W_ROOT}/apps/Qt6/bin/QtWebEngineProcess.exe;QTWEBENGINE_RESOURCES_PATH=${OSGEO4W_ROOT}/apps/Qt6/resources;QTWEBENGINE_LOCALES_PATH=${OSGEO4W_ROOT}/apps/Qt6/translations/qtwebengine_locales" TIMEOUT 90)
    set_tests_properties(topographic_import PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 300)
    set_tests_properties(reference_status PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
    set_tests_properties(heritage_flow PROPERTIES ENVIRONMENT "PATH=${_ka_path_esc};QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
    set_tests_properties(trench_plan trench_edit trench_plate trench_lshape trench_grid_snap trench_handle trench_standard PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen")
    set_tests_properties(heritage_style PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 180)
    set_tests_properties(heritage_form PROPERTIES ENVIRONMENT "PATH=${_ka_path_esc};QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
    set_tests_properties(theme_options theme_render ribbon_overflow ribbon_pixels toolbar_fit icon_glyphs icon_mockup PROPERTIES ENVIRONMENT "PATH=${_ka_path_esc};QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
    set_tests_properties(heritage_download_retry PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen;QTWEBENGINEPROCESS_PATH=${OSGEO4W_ROOT}/apps/Qt6/bin/QtWebEngineProcess.exe;QTWEBENGINE_RESOURCES_PATH=${OSGEO4W_ROOT}/apps/Qt6/resources;QTWEBENGINE_LOCALES_PATH=${OSGEO4W_ROOT}/apps/Qt6/translations/qtwebengine_locales" TIMEOUT 90)
    set_tests_properties(gdal_error_log PROPERTIES ENVIRONMENT "PATH=${_ka_path_esc};QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
    set_tests_properties(catch_log PROPERTIES ENVIRONMENT "PATH=${_ka_path_esc};QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
    set_tests_properties(cadastral PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PROJ_LIB=${OSGEO4W_ROOT}/share/proj;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
    set_tests_properties(export_survey_areas PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PROJ_LIB=${OSGEO4W_ROOT}/share/proj;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 120)
    set_tests_properties(perf_engine PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PROJ_LIB=${OSGEO4W_ROOT}/share/proj;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 300)
    set_tests_properties(parallel_render PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
    set_tests_properties(heritage_import map_geotiff_export PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PROJ_LIB=${OSGEO4W_ROOT}/share/proj;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 180)
    set_tests_properties(heritage_region PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PROJ_LIB=${OSGEO4W_ROOT}/share/proj;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
    set_tests_properties(heritage_fetch_plan account_status PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PROJ_LIB=${OSGEO4W_ROOT}/share/proj;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
    set_tests_properties(basemap_reference vworld_settings PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen")
    set_tests_properties(log_session PROPERTIES ENVIRONMENT "PATH=${_ka_path_esc};QT_QPA_PLATFORM=offscreen")
    set_tests_properties(webengine_smoke PROPERTIES ENVIRONMENT "PATH=${_ka_path_esc};QT_QPA_PLATFORM=offscreen;QTWEBENGINEPROCESS_PATH=${OSGEO4W_ROOT}/apps/Qt6/bin/QtWebEngineProcess.exe;QTWEBENGINE_RESOURCES_PATH=${OSGEO4W_ROOT}/apps/Qt6/resources;QTWEBENGINE_LOCALES_PATH=${OSGEO4W_ROOT}/apps/Qt6/translations/qtwebengine_locales" TIMEOUT 120)
    set_tests_properties(topographic_browser PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QT_QPA_PLATFORM=offscreen;QTWEBENGINEPROCESS_PATH=${OSGEO4W_ROOT}/apps/Qt6/bin/QtWebEngineProcess.exe;QTWEBENGINE_RESOURCES_PATH=${OSGEO4W_ROOT}/apps/Qt6/resources;QTWEBENGINE_LOCALES_PATH=${OSGEO4W_ROOT}/apps/Qt6/translations/qtwebengine_locales" TIMEOUT 300)
    # 기본 검사는 로컬 HTTP fixture에서 전송·취소·로그인 세션을 검증한다.
    # 실제 사이트 3개 검사는 KA_HGIS_LIVE_NGII_* opt-in 없이는 skip한다.
    set_tests_properties(dem_rendering PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 180)
    set_tests_properties(dem_presentation PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 120)
    set_property(TEST above_labels APPEND PROPERTY ENVIRONMENT "QT_SCALE_FACTOR=1")
    set_property(TEST above_labels_125 APPEND PROPERTY ENVIRONMENT "QT_SCALE_FACTOR=1.25")
    set_property(TEST above_labels_150 APPEND PROPERTY ENVIRONMENT "QT_SCALE_FACTOR=1.5")
    set_property(TEST above_labels_200 APPEND PROPERTY ENVIRONMENT "QT_SCALE_FACTOR=2")
    # This qgis-dev uses a fixed qgis-project-XXXXXX.zip inside TEMP for project
    # archives. Separate test processes must never share that staging file.
    file(TO_CMAKE_PATH "$ENV{TEMP}" _ka_test_temp_root)
    if(NOT IS_DIRECTORY "${_ka_test_temp_root}")
      set(_ka_test_temp_root "${CMAKE_BINARY_DIR}/test-tmp")
    endif()
    string(SHA256 _ka_test_temp_id "${CMAKE_BINARY_DIR}")
    string(SUBSTRING "${_ka_test_temp_id}" 0 12 _ka_test_temp_id)
    foreach(_ka_test IN ITEMS checklist_engine workflow_engine georef_engine feature_presets
        buffer_ring measure_tape dem_trench_engine theme_qss recent_surveys
        terrain_3d_engine ${KA_SAVE_OPEN_TESTS} e2e_opaque_suite network_services
        storage_safety reference_download region_locator_popup layer_state_regressions map_render
        above_labels above_labels_125 above_labels_150 above_labels_200 dem_presentation
        export_survey_areas perf_engine parallel_render catch_log cadastral)
      set(_ka_test_temp "${_ka_test_temp_root}/ka-hgis-tests-${_ka_test_temp_id}/${_ka_test}")
      file(MAKE_DIRECTORY "${_ka_test_temp}")
      set_property(TEST ${_ka_test} APPEND PROPERTY ENVIRONMENT
        "TEMP=${_ka_test_temp}" "TMP=${_ka_test_temp}")
    endforeach()
  endif()
endif()

# ---- [P1 design-system] KaChip + KaThemeFonts (append-only block; design section 3, P1) ----
set(_ka_p1_chip_sources ${CMAKE_SOURCE_DIR}/src/app/KaChip.cpp ${CMAKE_SOURCE_DIR}/src/app/KaChip.h)
# KaThemePaper (새 모양 tokens) and KaThemeColors (화면 테마) ride along: every target that compiles KaTheme.cpp needs it too.
set(_ka_p1_font_sources ${CMAKE_SOURCE_DIR}/src/app/KaThemeFonts.cpp ${CMAKE_SOURCE_DIR}/src/app/KaThemeFonts.h
  ${CMAKE_SOURCE_DIR}/src/app/KaThemePaper.cpp ${CMAKE_SOURCE_DIR}/src/app/KaThemePaper.h
  ${CMAKE_SOURCE_DIR}/src/app/KaThemeColors.cpp ${CMAKE_SOURCE_DIR}/src/app/KaThemeColors.h)
target_sources(ka-hgis PRIVATE ${_ka_p1_chip_sources} ${_ka_p1_font_sources})
# Bundled IBM Plex (OFL) travels next to the executable like the QSS copy; only files that exist.
file(GLOB _ka_p1_font_files "${CMAKE_SOURCE_DIR}/data/fonts/*.ttf" "${CMAKE_SOURCE_DIR}/data/fonts/LICENSE-*.txt")
foreach(_ka_p1_font IN LISTS _ka_p1_font_files)
  get_filename_component(_ka_p1_font_name "${_ka_p1_font}" NAME)
  add_custom_command(TARGET ka-hgis POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${_ka_p1_font}" "$<TARGET_FILE_DIR:ka-hgis>/data/fonts/${_ka_p1_font_name}")
endforeach()
if(KA_HGIS_BUILD_TESTS)
  # ka_feature_card_tests had no theme sources; KaChip needs KaTheme and KaIcons.
  target_sources(ka_feature_card_tests PRIVATE ${_ka_p1_chip_sources}
    ${CMAKE_SOURCE_DIR}/src/app/KaIcons.cpp ${CMAKE_SOURCE_DIR}/src/app/KaTheme.cpp
    ${CMAKE_SOURCE_DIR}/src/app/KaThemeSheet.cpp ${CMAKE_SOURCE_DIR}/src/app/KaThemeStyle.cpp)
  target_include_directories(ka_feature_card_tests PRIVATE ${CMAKE_BINARY_DIR}/generated)
  foreach(_ka_p1_target IN ITEMS ka_theme_tests ka_home_tests ka_shell_tests)
    target_sources(${_ka_p1_target} PRIVATE ${_ka_p1_chip_sources})
  endforeach()
  target_sources(ka_home_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app/KaIcons.cpp)
  target_sources(ka_shell_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app/KaIcons.cpp)
  # KaTheme.cpp / KaThemeSheet.cpp resolve the font tokens through KaThemeFonts.cpp:
  # every target that compiles them gets the file without editing each block above.
  get_property(_ka_p1_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
  foreach(_ka_p1_target IN LISTS _ka_p1_targets)
    get_target_property(_ka_p1_type ${_ka_p1_target} TYPE)
    if(NOT _ka_p1_type MATCHES "EXECUTABLE|LIBRARY")
      continue()
    endif()
    get_target_property(_ka_p1_sources ${_ka_p1_target} SOURCES)
    if(_ka_p1_sources MATCHES "KaTheme(Sheet)?\.cpp" AND NOT _ka_p1_sources MATCHES "KaThemeFonts\.cpp")
      target_sources(${_ka_p1_target} PRIVATE ${_ka_p1_font_sources})
    endif()
  endforeach()
  add_executable(ka_chip_theme_tests tests/test_chip_theme.cpp
    ${_ka_p1_chip_sources} ${_ka_p1_font_sources}
    src/app/KaIcons.cpp src/app/KaTheme.cpp src/app/KaThemeSheet.cpp src/app/KaThemeStyle.cpp)
  set_target_properties(ka_chip_theme_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_chip_theme_tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets)
  target_include_directories(ka_chip_theme_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  ka_add_qtest(chip_theme ka_chip_theme_tests)
  set_tests_properties(chip_theme PROPERTIES TIMEOUT 90)
  add_executable(ka_chip_plate_tests tests/test_chip_plate.cpp
    ${_ka_p1_chip_sources} ${_ka_p1_font_sources}
    src/app/KaIcons.cpp src/app/KaTheme.cpp src/app/KaThemeSheet.cpp src/app/KaThemeStyle.cpp)
  set_target_properties(ka_chip_plate_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_chip_plate_tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets)
  target_include_directories(ka_chip_plate_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  ka_add_qtest(chip_plate ka_chip_plate_tests)
  set_tests_properties(chip_plate PROPERTIES TIMEOUT 90)
  add_executable(ka_plate_sheet_tests tests/test_plate_sheet.cpp
    ${_ka_p1_font_sources} src/app/KaIcons.cpp src/app/KaTheme.cpp src/app/KaThemeSheet.cpp src/app/KaThemeStyle.cpp src/app/KaTabClose.cpp src/app/KaTabClose.h)
  set_target_properties(ka_plate_sheet_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_plate_sheet_tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets)
  target_include_directories(ka_plate_sheet_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  ka_add_qtest(plate_sheet ka_plate_sheet_tests)
  set_tests_properties(plate_sheet PROPERTIES TIMEOUT 90)
  # 새 모양 pieces drawn with the app sheet (board 4 review): tab ✕, PDF foot, chosen file row.
  add_executable(ka_plate_render_tests tests/test_plate_render.cpp
    ${_ka_p1_font_sources} src/app/KaIcons.cpp src/app/KaTheme.cpp src/app/KaThemeSheet.cpp src/app/KaThemeStyle.cpp
    src/app/KaTabClose.cpp src/app/KaTabClose.h src/app/KaDrawingStudioPlate.cpp src/app/KaDrawingStudioPlate.h
    src/app/KaFileRowDelegate.cpp src/app/KaFileRowDelegate.h)
  set_target_properties(ka_plate_render_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_plate_render_tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets)
  target_include_directories(ka_plate_render_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  ka_add_qtest(plate_render ka_plate_render_tests)
  set_tests_properties(plate_render PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
  # 「조용한 도판」 map screen panels (board 1) as the sheet draws them; split from plate_sheet at its 300-line limit.
  add_executable(ka_panel_sheet_tests tests/test_panel_sheet.cpp
    ${_ka_p1_font_sources} src/app/KaIcons.cpp src/app/KaTheme.cpp src/app/KaThemeSheet.cpp src/app/KaThemeStyle.cpp)
  set_target_properties(ka_panel_sheet_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_panel_sheet_tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets)
  target_include_directories(ka_panel_sheet_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  ka_add_qtest(panel_sheet ka_panel_sheet_tests)
  set_tests_properties(panel_sheet PROPERTIES TIMEOUT 90)
  add_executable(ka_sketch_measure_tests tests/test_sketch_measure.cpp src/app/KaSketchMeasure.cpp src/app/KaSketchMeasure.h
    src/app/KaDrawGuideBand.cpp src/app/KaDrawGuideBand.h ${_ka_p1_font_sources} src/app/KaIcons.cpp src/app/KaTheme.cpp
    src/app/KaThemeSheet.cpp src/app/KaThemeStyle.cpp)
  set_target_properties(ka_sketch_measure_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_sketch_measure_tests PRIVATE ka_core Qt6::Test Qt6::Widgets Qt6::Svg)
  target_include_directories(ka_sketch_measure_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  ka_add_qtest(sketch_measure ka_sketch_measure_tests)
  add_executable(ka_longitude_ruler_tests tests/test_longitude_ruler.cpp src/app/KaLongitudeRuler.cpp src/app/KaLongitudeRuler.h
    src/app/KaNewSurveyDialog.cpp src/app/KaNewSurveyDialog.h ${_ka_p1_font_sources} src/app/KaIcons.cpp src/app/KaTheme.cpp
    src/app/KaThemeSheet.cpp src/app/KaThemeStyle.cpp)
  set_target_properties(ka_longitude_ruler_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_longitude_ruler_tests PRIVATE ka_core Qt6::Test Qt6::Widgets Qt6::Svg)
  target_include_directories(ka_longitude_ruler_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  ka_add_qtest(longitude_ruler ka_longitude_ruler_tests)
endif()

# ---- [P2 icons] outline glyph set: travels with KaIcons.cpp wherever that file is compiled ----
set(_ka_p2_outline_sources
  ${CMAKE_SOURCE_DIR}/src/app/KaIconsOutline.h
  ${CMAKE_SOURCE_DIR}/src/app/KaIconsOutlineA.cpp
  ${CMAKE_SOURCE_DIR}/src/app/KaIconsOutlineB.cpp
  ${CMAKE_SOURCE_DIR}/src/app/KaIconsOutlineUi.cpp
  ${CMAKE_SOURCE_DIR}/src/app/KaIconsOutlineEngine.cpp)
get_property(_ka_p2_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(_ka_p2_target IN LISTS _ka_p2_targets)
  get_target_property(_ka_p2_type ${_ka_p2_target} TYPE)
  if(NOT _ka_p2_type MATCHES "EXECUTABLE|LIBRARY")
    continue()
  endif()
  get_target_property(_ka_p2_sources ${_ka_p2_target} SOURCES)
  if(_ka_p2_sources MATCHES "KaIcons\.cpp" AND NOT _ka_p2_sources MATCHES "KaIconsOutlineEngine")
    target_sources(${_ka_p2_target} PRIVATE ${_ka_p2_outline_sources})
  endif()
endforeach()
if(KA_HGIS_BUILD_TESTS)
  add_executable(ka_icon_outline_tests tests/test_icons_outline.cpp
    src/app/KaIcons.cpp ${_ka_p2_outline_sources} ${_ka_p1_font_sources}
    src/app/KaTheme.cpp src/app/KaThemeSheet.cpp src/app/KaThemeStyle.cpp)
  set_target_properties(ka_icon_outline_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_icon_outline_tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets)
  target_include_directories(ka_icon_outline_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  ka_add_qtest(icon_outline ka_icon_outline_tests)
  set_tests_properties(icon_outline PROPERTIES ENVIRONMENT "PATH=${_ka_path_esc};QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
endif()

# ---- [P3 home] home cards, row delegate and SurveyFacts (append-only block; design section 3, P3) ----
set(_ka_p3_app_sources
  ${CMAKE_SOURCE_DIR}/src/app/KaHomeRowDelegate.cpp ${CMAKE_SOURCE_DIR}/src/app/KaHomeRowDelegate.h
  ${CMAKE_SOURCE_DIR}/src/app/KaHomeGuideCard.cpp ${CMAKE_SOURCE_DIR}/src/app/KaHomeGuideCard.h
  ${CMAKE_SOURCE_DIR}/src/app/KaHomeConnectionCard.cpp ${CMAKE_SOURCE_DIR}/src/app/KaHomeConnectionCard.h)
target_sources(ka_core PRIVATE ${CMAKE_SOURCE_DIR}/src/core/SurveyFacts.cpp ${CMAKE_SOURCE_DIR}/src/core/SurveyFacts.h)
target_sources(ka-hgis PRIVATE ${_ka_p3_app_sources})
if(KA_HGIS_BUILD_TESTS)
  target_sources(ka_home_tests PRIVATE ${_ka_p3_app_sources})
  # The layer list shows a survey layer's feature count in 새 모양, so its test reads the display options.
  foreach(_ka_layer_target IN ITEMS ka_layer_information_tests ka_layer_panel_tests)
    target_sources(${_ka_layer_target} PRIVATE src/app/KaTheme.cpp src/app/KaThemeSheet.cpp src/app/KaThemeStyle.cpp
      src/app/KaIcons.cpp ${_ka_p2_outline_sources} ${_ka_p1_font_sources})
    target_include_directories(${_ka_layer_target} PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  endforeach()
  # Home cards, rows, badges and the compact hero (tests/test_home.cpp stays under 300 lines).
  add_executable(ka_home_cards_tests tests/test_home_cards.cpp
    src/app/KaStartPage.cpp src/app/KaStartHero.cpp src/app/KaSplashArt.cpp src/app/KaHomeTitleBlock.cpp
    src/app/KaHomeRecentCard.cpp src/app/KaHomeStatus.cpp src/app/KaHomeText.cpp
    src/app/KaRecoverySnapshots.cpp
    src/app/KaTheme.cpp src/app/KaThemeSheet.cpp src/app/KaThemeStyle.cpp src/app/KaIcons.cpp
    ${_ka_p3_app_sources} ${_ka_p1_chip_sources} ${_ka_p1_font_sources} ${_ka_p2_outline_sources})
  target_link_libraries(ka_home_cards_tests PRIVATE ${KA_TEST_LIBS} Qt6::Network)
  target_include_directories(ka_home_cards_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  target_compile_definitions(ka_home_cards_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_home_cards_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(home_cards ka_home_cards_tests)
  set_tests_properties(home_cards PROPERTIES TIMEOUT 90)
  # 「조용한 도판」 home: title block, quiet 새 조사, no hero band (test_home_cards.cpp is at 300 lines).
  add_executable(ka_home_plate_tests tests/test_home_plate.cpp
    src/app/KaStartPage.cpp src/app/KaStartHero.cpp src/app/KaSplashArt.cpp src/app/KaHomeTitleBlock.cpp
    src/app/KaHomeRecentCard.cpp src/app/KaHomeStatus.cpp src/app/KaHomeText.cpp
    src/app/KaRecoverySnapshots.cpp
    src/app/KaTheme.cpp src/app/KaThemeSheet.cpp src/app/KaThemeStyle.cpp src/app/KaIcons.cpp
    ${_ka_p3_app_sources} ${_ka_p1_chip_sources} ${_ka_p1_font_sources} ${_ka_p2_outline_sources})
  target_link_libraries(ka_home_plate_tests PRIVATE ${KA_TEST_LIBS} Qt6::Network)
  target_include_directories(ka_home_plate_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  target_compile_definitions(ka_home_plate_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_home_plate_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(home_plate ka_home_plate_tests)
  set_tests_properties(home_plate PROPERTIES TIMEOUT 90)
  # 세부 디자인 보완(홈 항목들): test_home_plate.cpp is at the 300-line limit, so these live beside it.
  add_executable(ka_home_polish_tests tests/test_home_polish.cpp
    src/app/KaStartPage.cpp src/app/KaStartHero.cpp src/app/KaSplashArt.cpp src/app/KaHomeTitleBlock.cpp
    src/app/KaHomeRecentCard.cpp src/app/KaHomeStatus.cpp src/app/KaHomeText.cpp
    src/app/KaRecoverySnapshots.cpp
    src/app/KaTheme.cpp src/app/KaThemeSheet.cpp src/app/KaThemeStyle.cpp src/app/KaIcons.cpp
    ${_ka_p3_app_sources} ${_ka_p1_chip_sources} ${_ka_p1_font_sources} ${_ka_p2_outline_sources})
  target_link_libraries(ka_home_polish_tests PRIVATE ${KA_TEST_LIBS} Qt6::Network)
  target_include_directories(ka_home_polish_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  target_compile_definitions(ka_home_polish_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_home_polish_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(home_polish ka_home_polish_tests)
  set_tests_properties(home_polish PROPERTIES TIMEOUT 90)
  # SurveyFacts records and the GDAL probe (no QGIS).
  add_executable(ka_survey_facts_tests tests/test_survey_facts.cpp)
  target_link_libraries(ka_survey_facts_tests PRIVATE ka_core Qt6::Test)
  target_include_directories(ka_survey_facts_tests PRIVATE ${KA_INCLUDES})
  set_target_properties(ka_survey_facts_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(survey_facts ka_survey_facts_tests)
  set_tests_properties(survey_facts PROPERTIES ENVIRONMENT "PATH=${_ka_path_esc};QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
endif()

# ---- [P4 map-panels] layer list chrome, section bands, inspector panel, basemap card (append-only block; design section 3, P4) ----
set(_ka_p4_list_sources
  ${CMAKE_SOURCE_DIR}/src/app/KaLayerInformationView.cpp
  ${CMAKE_SOURCE_DIR}/src/app/KaLayerSectionDelegate.cpp ${CMAKE_SOURCE_DIR}/src/app/KaLayerSectionDelegate.h)
set(_ka_p4_card_sources ${CMAKE_SOURCE_DIR}/src/app/KaFeatureCardEditors.cpp ${CMAKE_SOURCE_DIR}/src/app/KaFeatureCardPlate.cpp
  ${CMAKE_SOURCE_DIR}/src/app/KaSymbolSwatch.cpp ${CMAKE_SOURCE_DIR}/src/app/KaSymbolSwatch.h)
set(_ka_p4_panel_sources
  ${CMAKE_SOURCE_DIR}/src/app/KaLayerListChrome.cpp ${CMAKE_SOURCE_DIR}/src/app/KaLayerListChrome.h
  ${CMAKE_SOURCE_DIR}/src/app/KaInspectorPanel.cpp ${CMAKE_SOURCE_DIR}/src/app/KaInspectorPanel.h
  ${CMAKE_SOURCE_DIR}/src/app/KaInspectorStyle.cpp ${CMAKE_SOURCE_DIR}/src/app/KaInspectorStyle.h
  ${CMAKE_SOURCE_DIR}/src/app/KaStylePalette.h
  ${CMAKE_SOURCE_DIR}/src/app/KaBasemapQuickCard.cpp ${CMAKE_SOURCE_DIR}/src/app/KaBasemapQuickCard.h)
# The view methods and configureView moved out of KaLayerInformation.cpp, buildEditors out of
# KaFeatureCard.cpp: every target that compiles the original file gets the split-off files.
get_property(_ka_p4_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(_ka_p4_target IN LISTS _ka_p4_targets)
  get_target_property(_ka_p4_type ${_ka_p4_target} TYPE)
  if(NOT _ka_p4_type MATCHES "EXECUTABLE|LIBRARY")
    continue()
  endif()
  get_target_property(_ka_p4_sources ${_ka_p4_target} SOURCES)
  if(_ka_p4_sources MATCHES "KaLayerInformation\.cpp" AND NOT _ka_p4_sources MATCHES "KaLayerInformationView")
    target_sources(${_ka_p4_target} PRIVATE ${_ka_p4_list_sources})
  endif()
  if(_ka_p4_sources MATCHES "KaFeatureCard\.cpp" AND NOT _ka_p4_sources MATCHES "KaFeatureCardEditors")
    target_sources(${_ka_p4_target} PRIVATE ${_ka_p4_card_sources})
  endif()
endforeach()
target_sources(ka-hgis PRIVATE ${_ka_p4_panel_sources})
if(KA_HGIS_BUILD_TESTS)
  set(_ka_p4_theme_sources
    src/app/KaIcons.cpp ${_ka_p2_outline_sources} src/app/KaTheme.cpp src/app/KaThemeSheet.cpp
    src/app/KaThemeStyle.cpp ${_ka_p1_font_sources})
  # Section bands, base row height, the find field and the edit pencil.
  add_executable(ka_layer_list_chrome_tests tests/test_layer_list_chrome.cpp tests/layer_list_fixture.h
    src/app/KaLayerInformation.cpp src/app/KaLayerInformationPanel.cpp ${_ka_p4_list_sources}
    src/app/KaLayerListChrome.cpp src/app/KaLayerListChrome.h ${_ka_p4_theme_sources})
  target_link_libraries(ka_layer_list_chrome_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_layer_list_chrome_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  target_compile_definitions(ka_layer_list_chrome_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_layer_list_chrome_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(layer_list_chrome ka_layer_list_chrome_tests)
  # 4 s standalone, but 43 s alone / 65 s in a parallel run when ctest is the parent (theme
  # applied; cause not found): 180 like the save/open tests so -j16 load cannot time it out.
  set_tests_properties(layer_list_chrome PROPERTIES
    ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 180)
  # The right panel with the hosted feature card and the basemap card.
  add_executable(ka_inspector_panel_tests tests/test_inspector_panel.cpp
    src/app/KaInspectorPanel.cpp src/app/KaInspectorPanel.h
    src/app/KaInspectorStyle.cpp src/app/KaInspectorStyle.h
    src/app/KaBasemapQuickCard.cpp src/app/KaBasemapQuickCard.h
    src/app/KaFeatureCard.cpp src/app/KaFeatureCard.h src/app/KaFeatureCardEdit.cpp ${_ka_p4_card_sources}
    src/app/KaFeatureFormDialog.cpp src/app/KaFeatureFormDialog.h
    ${_ka_p1_chip_sources} ${_ka_p4_theme_sources})
  target_link_libraries(ka_inspector_panel_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_inspector_panel_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  target_compile_definitions(ka_inspector_panel_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_inspector_panel_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(inspector_panel ka_inspector_panel_tests)
  # 조사 상태·조사자·조사일 on the card and Enter in one box (test_feature_card.cpp is at 300 lines).
  add_executable(ka_feature_card_fields_tests tests/test_feature_card_fields.cpp
    src/app/KaFeatureCard.cpp src/app/KaFeatureCard.h src/app/KaFeatureCardEdit.cpp ${_ka_p4_card_sources}
    src/app/KaFeatureFormDialog.cpp src/app/KaFeatureFormDialog.h
    ${_ka_p1_chip_sources} ${_ka_p4_theme_sources})
  target_link_libraries(ka_feature_card_fields_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_feature_card_fields_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  target_compile_definitions(ka_feature_card_fields_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_feature_card_fields_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(feature_card_fields ka_feature_card_fields_tests)
  set_tests_properties(feature_card_fields PROPERTIES
    ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
  # 새 모양 기록 카드 (2차 보드 판 3); tests/test_feature_card_fields.cpp stays under 300 lines.
  add_executable(ka_feature_card_plate_tests tests/test_feature_card_plate.cpp
    src/app/KaFeatureCard.cpp src/app/KaFeatureCard.h src/app/KaFeatureCardEdit.cpp ${_ka_p4_card_sources}
    src/app/KaFeatureFormDialog.cpp src/app/KaFeatureFormDialog.h
    ${_ka_p1_chip_sources} ${_ka_p4_theme_sources})
  target_link_libraries(ka_feature_card_plate_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_feature_card_plate_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  target_compile_definitions(ka_feature_card_plate_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_feature_card_plate_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(feature_card_plate ka_feature_card_plate_tests)
  set_tests_properties(feature_card_plate PROPERTIES
    ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
  # The 스타일 tab alone: both check boxes, point layers, a change made elsewhere.
  add_executable(ka_inspector_style_tests tests/test_inspector_style.cpp
    src/app/KaInspectorStyle.cpp src/app/KaInspectorStyle.h src/app/KaSymbolSwatch.cpp src/app/KaSymbolSwatch.h ${_ka_p4_theme_sources})
  target_link_libraries(ka_inspector_style_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_inspector_style_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  target_compile_definitions(ka_inspector_style_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_inspector_style_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(inspector_style ka_inspector_style_tests)
  set_tests_properties(inspector_style PROPERTIES
    ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
  set_tests_properties(inspector_panel PROPERTIES
    ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
endif()

# ---- [P5 shell-chrome] survey badge, drawing guide band, edit-buffer summary (append-only block; design section 3, P5) ----
set(_ka_p5_app_sources
  ${CMAKE_SOURCE_DIR}/src/app/KaSurveyBadge.cpp ${CMAKE_SOURCE_DIR}/src/app/KaSurveyBadge.h
  ${CMAKE_SOURCE_DIR}/src/app/KaDrawGuideBand.cpp ${CMAKE_SOURCE_DIR}/src/app/KaDrawGuideBand.h)
target_sources(ka_core PRIVATE ${CMAKE_SOURCE_DIR}/src/core/EditBufferSummary.cpp ${CMAKE_SOURCE_DIR}/src/core/EditBufferSummary.h)
target_sources(ka-hgis PRIVATE ${_ka_p5_app_sources})
if(KA_HGIS_BUILD_TESTS)
  set(_ka_p5_theme_sources
    src/app/KaIcons.cpp ${_ka_p2_outline_sources} src/app/KaTheme.cpp src/app/KaThemeSheet.cpp
    src/app/KaThemeStyle.cpp ${_ka_p1_font_sources})
  # Status-bar chips, the tab badge and the guide band (no QGIS). The ctest name shell_chrome
  # already belongs to ka_shell_tests (tests/test_shell.cpp), so this one is shell_widgets.
  add_executable(ka_shell_chrome_tests tests/test_shell_chrome.cpp
    src/app/KaStatusBar.cpp src/app/KaStatusBar.h
    src/app/KaAppBar.cpp src/app/KaAppBar.h
    src/app/KaLayerOpacityRail.cpp src/app/KaLayerOpacityRail.h
    ${_ka_p5_app_sources} ${_ka_p1_chip_sources} ${_ka_p5_theme_sources})
  set_target_properties(ka_shell_chrome_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_shell_chrome_tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets)
  target_include_directories(ka_shell_chrome_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  ka_add_qtest(shell_widgets ka_shell_chrome_tests)
  set_tests_properties(shell_widgets PROPERTIES ENVIRONMENT "PATH=${_ka_path_esc};QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
  # 「조용한 도판」 map screen (board 1): survey mark, work CRS, address search, scale bar, basemap buttons.
  add_executable(ka_map_plate_tests tests/test_map_plate.cpp
    src/app/KaStatusBar.cpp src/app/KaStatusBar.h
    src/app/KaAppBar.cpp src/app/KaAppBar.h
    src/app/KaMapControls.cpp src/app/KaMapControls.h
    src/app/KaBasemapQuickCard.cpp src/app/KaBasemapQuickCard.h
    src/app/KaGridOriginButton.cpp src/app/KaGridOriginButton.h src/app/KaCanvasGridOverlay.cpp src/app/KaCanvasGridOverlay.h
    src/app/KaCrashGuard.cpp src/app/KaCrashGuard.h
    ${_ka_p5_app_sources} ${_ka_p1_chip_sources} ${_ka_p5_theme_sources})
  set_target_properties(ka_map_plate_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_map_plate_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_map_plate_tests PRIVATE ${KA_INCLUDES} ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  target_link_directories(ka_map_plate_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_map_plate_tests PRIVATE ${KA_TEST_DEFS})
  ka_add_qtest(map_plate ka_map_plate_tests)
  # 세부 디자인 보완(지도 화면 항목들): the plate test files are at the 300-line limit, so these live together.
  add_executable(ka_map_polish_tests tests/test_map_polish.cpp
    src/app/KaInspectorPanel.cpp src/app/KaInspectorPanel.h
    src/app/KaInspectorStyle.cpp src/app/KaInspectorStyle.h
    src/app/KaBasemapQuickCard.cpp src/app/KaBasemapQuickCard.h
    src/app/KaFeatureCard.cpp src/app/KaFeatureCard.h src/app/KaFeatureCardEdit.cpp ${_ka_p4_card_sources}
    src/app/KaFeatureFormDialog.cpp src/app/KaFeatureFormDialog.h
    src/app/KaStatusBar.cpp src/app/KaStatusBar.h
    src/app/KaDrawRowRules.cpp src/app/KaDrawRowRules.h
    ${_ka_p5_app_sources}
    ${_ka_p1_chip_sources} ${_ka_p4_theme_sources})
  target_link_libraries(ka_map_polish_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_map_polish_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  target_compile_definitions(ka_map_polish_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_map_polish_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(map_polish ka_map_polish_tests)
  set_tests_properties(map_polish PROPERTIES
    ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
  # 세부 디자인 보완(도면 화면 항목들): test_map_polish.cpp is at the 300-line limit.
  add_executable(ka_drawing_polish_tests tests/test_drawing_polish.cpp
    src/app/KaDrawingStudioPlate.cpp src/app/KaDrawingStudioPlate.h
    src/app/KaPaperDialog.cpp src/app/KaPaperDialog.h
    ${_ka_p1_chip_sources} ${_ka_p4_theme_sources})
  target_link_libraries(ka_drawing_polish_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_drawing_polish_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  target_compile_definitions(ka_drawing_polish_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_drawing_polish_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(drawing_polish ka_drawing_polish_tests)
  set_tests_properties(drawing_polish PROPERTIES
    ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
  # 세부 디자인 보완(지도 화면 판 틀: M1 · H1).
  add_executable(ka_page_polish_tests tests/test_page_polish.cpp
    src/app/KaMapPagePlate.cpp src/app/KaMapPagePlate.h
    ${_ka_p1_chip_sources} ${_ka_p4_theme_sources})
  target_link_libraries(ka_page_polish_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_page_polish_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  target_compile_definitions(ka_page_polish_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_page_polish_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(page_polish ka_page_polish_tests)
  set_tests_properties(page_polish PROPERTIES
    ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
  # 세부 디자인 보완(창 항목: D1 단추·입력칸 높이).
  add_executable(ka_window_polish_tests tests/test_window_polish.cpp
    src/app/KaSubmitDialog.cpp src/app/KaSubmitDialog.h src/app/KaSubmitDialogPlate.cpp
    src/app/KaSubmitDrawingStrip.cpp src/app/KaSubmitDrawingStrip.h
    ${_ka_p1_chip_sources} ${_ka_p4_theme_sources})
  target_link_libraries(ka_window_polish_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_window_polish_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  target_compile_definitions(ka_window_polish_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_window_polish_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(window_polish ka_window_polish_tests)
  set_tests_properties(window_polish PROPERTIES
    ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
  # 세부 디자인 보완(튼튼함 R1: 낮은 창의 오른쪽 판).
  add_executable(ka_inspector_polish_tests tests/test_inspector_polish.cpp
    src/app/KaInspectorPanel.cpp src/app/KaInspectorPanel.h
    src/app/KaInspectorStyle.cpp src/app/KaInspectorStyle.h
    src/app/KaBasemapQuickCard.cpp src/app/KaBasemapQuickCard.h
    src/app/KaFeatureCard.cpp src/app/KaFeatureCard.h src/app/KaFeatureCardEdit.cpp ${_ka_p4_card_sources}
    src/app/KaFeatureFormDialog.cpp src/app/KaFeatureFormDialog.h
    ${_ka_p1_chip_sources} ${_ka_p4_theme_sources})
  target_link_libraries(ka_inspector_polish_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_inspector_polish_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  target_compile_definitions(ka_inspector_polish_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_inspector_polish_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(inspector_polish ka_inspector_polish_tests)
  set_tests_properties(inspector_polish PROPERTIES
    ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
  # 세부 디자인 보완(파일함 M8: 바로가기 단추).
  add_executable(ka_browser_polish_tests tests/test_browser_polish.cpp
    src/app/KaFileBrowserPanel.cpp src/app/KaFileBrowserPanel.h src/app/KaFileRowDelegate.cpp src/app/KaFileRowDelegate.h
    ${_ka_p1_chip_sources} ${_ka_p4_theme_sources})
  target_link_libraries(ka_browser_polish_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_browser_polish_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  target_compile_definitions(ka_browser_polish_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_browser_polish_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(browser_polish ka_browser_polish_tests)
  set_tests_properties(browser_polish PROPERTIES
    ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
  set_tests_properties(map_plate PROPERTIES ENVIRONMENT
    "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
  # 「조용한 도판」 windows (board 5): message boxes with a serif question and no icon disc (no QGIS).
  add_executable(ka_window_plate_tests tests/test_window_plate.cpp src/app/KaApiKeyDialog.cpp src/app/KaApiKeyDialog.h
    src/app/KaMenuCaption.cpp src/app/KaMenuCaption.h src/app/KaFormPlate.cpp src/app/KaFormPlate.h
    src/app/KaControlPointDialog.cpp src/app/KaControlPointDialog.h src/app/KaControlCsvBox.cpp src/app/KaControlCsvBox.h
    src/app/KaClosePrompt.cpp src/app/KaClosePrompt.h src/app/KaHomeText.cpp src/app/KaHomeText.h
    src/app/KaUserError.cpp src/app/KaUserError.h ${_ka_p5_theme_sources})
  set_target_properties(ka_window_plate_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_window_plate_tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets)
  target_include_directories(ka_window_plate_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  ka_add_qtest(window_plate ka_window_plate_tests)
  set_tests_properties(window_plate PROPERTIES ENVIRONMENT "PATH=${_ka_path_esc};QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
  # 「도형 속성」 (조사카드) in 새 모양 (board 5): leader sections, two to a row, the 표고 section picture (no QGIS).
  add_executable(ka_record_plate_tests tests/test_record_plate.cpp src/app/KaRecordPlate.cpp src/app/KaRecordPlate.h
    src/app/KaElevationFigure.cpp src/app/KaElevationFigure.h ${_ka_p5_theme_sources})
  set_target_properties(ka_record_plate_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_record_plate_tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets)
  target_include_directories(ka_record_plate_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  ka_add_qtest(record_plate ka_record_plate_tests)
  set_tests_properties(record_plate PROPERTIES ENVIRONMENT "PATH=${_ka_path_esc};QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
  # Three-pane splitter, F10 fold and the rail inset (tests/test_shell.cpp is at its 300-line limit).
  add_executable(ka_shell_focus_tests tests/test_shell_focus.cpp
    src/app/KaShellFocus.cpp src/app/KaShellFocus.h
    src/app/KaLayerOpacityRail.cpp src/app/KaLayerOpacityRail.h)
  set_target_properties(ka_shell_focus_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_shell_focus_tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets)
  target_include_directories(ka_shell_focus_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
  ka_add_qtest(shell_focus ka_shell_focus_tests)
  set_tests_properties(shell_focus PROPERTIES ENVIRONMENT "PATH=${_ka_path_esc};QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
  # 「저장 안 됨 n건」: unique feature ids over the edit buffers (QGIS memory layers).
  add_executable(ka_edit_buffer_summary_tests tests/test_edit_buffer_summary.cpp)
  target_link_libraries(ka_edit_buffer_summary_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_edit_buffer_summary_tests PRIVATE ${KA_INCLUDES} ${CMAKE_SOURCE_DIR}/src)
  target_link_directories(ka_edit_buffer_summary_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_edit_buffer_summary_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_edit_buffer_summary_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(edit_buffer_summary ka_edit_buffer_summary_tests)
  set_tests_properties(edit_buffer_summary PROPERTIES
    ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
  add_executable(ka_paleo_sources_tests tests/test_paleo_sources.cpp)
  target_link_libraries(ka_paleo_sources_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_paleo_sources_tests PRIVATE ${KA_INCLUDES} ${CMAKE_SOURCE_DIR}/src)
  target_link_directories(ka_paleo_sources_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_paleo_sources_tests PRIVATE ${KA_TEST_DEFS})
  set_target_properties(ka_paleo_sources_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(paleo_sources ka_paleo_sources_tests)
  set_tests_properties(paleo_sources PROPERTIES
    ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
endif()

# ---- [P6 wiring] MainWindowChrome.cpp; ka_save_open_tests catches up with the P1-P5 sources (append-only block; design section 3, P6) ----
target_sources(ka-hgis PRIVATE ${CMAKE_SOURCE_DIR}/src/app/MainWindowChrome.cpp)
if(KA_HGIS_BUILD_TESTS)
  # ka_save_open_tests copied the ka-hgis source list before the P1-P5 blocks appended KaChip,
  # the home cards, the panels, the badge/band and now MainWindowChrome.cpp (which MainWindow*.cpp
  # call). Give it every ka-hgis source it does not compile yet; main.cpp/KaApplication.cpp/app.rc
  # stay out as before.
  get_target_property(_ka_p6_app_sources ka-hgis SOURCES)
  get_target_property(_ka_p6_test_sources ka_save_open_tests SOURCES)
  set(_ka_p6_have "")
  foreach(_ka_p6_src IN LISTS _ka_p6_test_sources)
    get_filename_component(_ka_p6_abs "${_ka_p6_src}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
    list(APPEND _ka_p6_have "${_ka_p6_abs}")
  endforeach()
  set(_ka_p6_missing "")
  foreach(_ka_p6_src IN LISTS _ka_p6_app_sources)
    get_filename_component(_ka_p6_abs "${_ka_p6_src}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
    if(_ka_p6_abs MATCHES "/src/app/(main\.cpp|KaApplication\.cpp|app\.rc)$")
      continue()
    endif()
    list(FIND _ka_p6_have "${_ka_p6_abs}" _ka_p6_index)
    if(_ka_p6_index LESS 0)
      list(APPEND _ka_p6_missing "${_ka_p6_abs}")
    endif()
  endforeach()
  if(_ka_p6_missing)
    target_sources(ka_save_open_tests PRIVATE ${_ka_p6_missing})
  endif()
  target_link_libraries(ka_save_open_tests PRIVATE Qt6::Network)
endif()

# ---- [fix] ka_perf_tests catches up with the P1-P6 sources (append-only block; review fix 2026-09-30) ----
# KA_PERF_SOURCES copied the ka-hgis source list at line ~973, before the P1-P6 blocks appended
# KaChip, the home cards, the panels, the badge/band and MainWindowChrome.cpp; the perf target
# then failed to link (19 unresolved symbols). Same catch-up as the P6 block for ka_save_open_tests.
if(KA_HGIS_BUILD_TESTS)
  get_target_property(_ka_fix_app_sources ka-hgis SOURCES)
  get_target_property(_ka_fix_perf_sources ka_perf_tests SOURCES)
  set(_ka_fix_have "")
  foreach(_ka_fix_src IN LISTS _ka_fix_perf_sources)
    get_filename_component(_ka_fix_abs "${_ka_fix_src}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
    list(APPEND _ka_fix_have "${_ka_fix_abs}")
  endforeach()
  set(_ka_fix_missing "")
  foreach(_ka_fix_src IN LISTS _ka_fix_app_sources)
    get_filename_component(_ka_fix_abs "${_ka_fix_src}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
    if(_ka_fix_abs MATCHES "/src/app/(main\.cpp|KaApplication\.cpp|app\.rc)$")
      continue()
    endif()
    list(FIND _ka_fix_have "${_ka_fix_abs}" _ka_fix_index)
    if(_ka_fix_index LESS 0)
      list(APPEND _ka_fix_missing "${_ka_fix_abs}")
    endif()
  endforeach()
  if(_ka_fix_missing)
    target_sources(ka_perf_tests PRIVATE ${_ka_fix_missing})
  endif()
  target_link_libraries(ka_perf_tests PRIVATE Qt6::Network)
endif()

# ---- [mockup icons] grey tile + Lucide SVG glyph style (design 2026-10-01-mockup-match) ----
# Every target that compiles KaIcons.cpp (the app, the icon tests, anything that renders a ribbon or a
# chip) also gets the Mockup engine, the id -> SVG table, the SVG resources and Qt6::Svg. Placed last so
# targets added by the earlier blocks (and the source copies made for the save/open and perf tests) are
# all visible here.
set(_ka_mockup_sources
  ${CMAKE_SOURCE_DIR}/src/app/KaIconsMockup.h
  ${CMAKE_SOURCE_DIR}/src/app/KaIconsMockupEngine.cpp
  ${CMAKE_SOURCE_DIR}/src/app/KaIconsMockupMap.cpp
  ${CMAKE_SOURCE_DIR}/data/icons/icons-mockup.qrc)
get_property(_ka_mockup_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(_ka_mockup_target IN LISTS _ka_mockup_targets)
  get_target_property(_ka_mockup_type ${_ka_mockup_target} TYPE)
  if(NOT _ka_mockup_type MATCHES "EXECUTABLE|LIBRARY")
    continue()
  endif()
  get_target_property(_ka_mockup_target_sources ${_ka_mockup_target} SOURCES)
  if(_ka_mockup_target_sources MATCHES "KaIcons\\.cpp" AND NOT _ka_mockup_target_sources MATCHES "KaIconsMockupEngine")
    target_sources(${_ka_mockup_target} PRIVATE ${_ka_mockup_sources})
    target_link_libraries(${_ka_mockup_target} PRIVATE Qt6::Svg)
  endif()
endforeach()
