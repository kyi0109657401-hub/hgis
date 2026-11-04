#pragma once
#include <QColor>
#include <QMainWindow>
#include <QJsonObject>
#include <QPair>
#include <QVector>
#include <QHash>
#include <QSet>
#include <QPointer>
#include <vector>
#include <memory>
#include "core/LocationSearch.h"
#include "core/TrenchGridGenerator.h"
#include "core/HeritageImport.h"
#include "core/AccountStatus.h"  // [P6 wiring] openAccountSetup(AccountStatus::Source)
class QListWidget;
class QListWidgetItem;
class QAction;
class QMenu;
class QLabel;
class QToolBar;
class QToolButton;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QEvent;
class QBoxLayout;  // [int W1] F044 setupFeatureCard
class QUndoCommand;
class KaAboveLabelsOverlay;
class KaLayerOpacityRail;
class KaReferenceDownloadJob;
class KaTopographicBrowser;
class KaTopographicImportDialog;
class QgsFeedback;
struct PreparedReferenceMap;
class QProgressDialog;
struct KaRemovedLayers;
class QgsRectangle;
class QShowEvent;
class QCloseEvent;
class QTabWidget;
class QFrame;
class ChecklistEngine;
struct CheckResult;  // [pkg A] core/ChecklistEngine.h
class KaStatusBar;
class KaBeginnerRibbon;
namespace PolygonErase {
struct Plan;
}
#if KA_HGIS_HAS_QGIS
#include <qgsfeature.h>
class QgsMapCanvas;
class QgsLayerTreeView;
enum class HeritageDataset;  // core/HeritageStyle.h
class QgsMapLayer;
class QgsVectorLayer;
class QgsRasterLayer;
class QgsMapTool;
class QgsMapToolEmitPoint;
class QgsPointXY;
class KaCaptureMapTool;
class KaAttributeMapTool;
class KaAlignMapTool;
class KaAlignPickTool;
class KaImageView;
class KaAlignLinkOverlay;
class KaDrawingStudio;
class KaMapCornerBar;
class KaTerrain3dStudio;
class KaTerrain3dLayoutStudio;
class KaStartPage;
class KaAppBar;
class KaMeasureMapTool;
class KaFeatureSelectTool;
class KaVertexEditTool;
class EditHistory;  // [pkg B1] core/EditHistory.h: edits of all layers on one time line
class KaLayerRefreshBatch;   // [pkg E1] F131 app/KaLayerRefreshBatch.h
class KaLayerTreeUndo;       // [pkg E1] F186 app/KaLayerTreeUndo.h
struct KaLayerTreeSnapshot;  // [pkg E1] F186
class KaFeatureCard;         // [pkg K] F044 app/KaFeatureCard.h
class QSplitter;
class QListWidget;
class QTimer;
class QgsVertexMarker;
class QgsMapToolPan;
class QgsGeometry;
class QgsFeature;
class QgsLayerTreeMapCanvasBridge;
class QgsMessageBar;
#endif
// [P6 wiring] Strata shell widgets attached in MainWindowChrome.cpp.
class KaInspectorPanel;
class KaSurveyBadge;
class KaDrawGuideBand;
class KaBasemapQuickCard;
class QTimer;

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  // [v3 int] F073: tests wait for the post-open satellite/cadastral pair before taking a layer baseline.
  bool basemapBootPending() const { return m_basemapBootPending; }
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;
  bool eventFilter(QObject* watched, QEvent* event) override;
  void showEvent(QShowEvent* event) override;
  void changeEvent(QEvent* event) override;
  void closeEvent(QCloseEvent* event) override;
  enum class OpenSurveyMode { PreferWorkspace, LayersOnly };
  bool openSurveyGpkg(const QString& gpkgPath);
  bool openSurveyGpkg(const QString& gpkgPath, OpenSurveyMode mode);
  int domainLayerCount() const;
  QString workCrsAuthId() const { return m_workCrs; }
  void runChecklistPublic() { runChecklist(); }
  int seedDemoFieldData();
  int lastChecklistErrorCount() const;
  // 부팅 때 미뤄 둔 배경지도 로딩을 지금 끝낸다(자동 QA·스모크가 결정적으로 돌게).
  void loadBootBasemaps();
  // P3-1: 배경 켜/끄기·조판 들락날락·저장·줌을 반복한다. 0이면 성공.
  int runUiStressLoop(int iterations);
  bool addVectorFromPath(const QString& path, const QString& cadAuthId = QString());  // 도면은 KaCadImport
  bool addRasterFromPath(const QString& path);
  bool tryAddDroppedUrls(const QList<QUrl>& urls);
  void showLayerTreeContextMenu(QgsLayerTreeView* treeView, const QPoint& pos);
  void updateLayerOpacityControl();
  // "2000" 과 "1:2000" 을 모두 분모 2000 으로 읽는다. 0 이면 숫자가 아니다.
  static double scaleDenominatorFromUi(const QString& raw);
  void editCurrentLayerStyle(QgsMapLayer* layer = nullptr);
  void editCurrentLayerAttributes(QgsMapLayer* layer = nullptr);
  void removeLayersFromTree(QgsLayerTreeView* tree);
  void undoMapAction();
  void redoMapAction();

private:
  enum class ReferenceMapKind { Soil, PaleoSoil, Geology, River, GeoInfoBorehole, KigamPaleo };
  QProgressDialog* createDownloadProgress(const QString& title);
  void startFileDownload(const QString& title,
      std::function<PreparedReferenceMap(QgsFeedback*, const std::function<bool()>&)> prepare,
      std::function<void(const PreparedReferenceMap&)> apply);
  void startDemDownload();
  // [pkg G1] F121: downloads start only with an open survey folder (empty = refused
  // after telling the user); download folders replaced this session go at a save point.
  QString referenceDownloadFolder(const QString& title);
  void pruneRetiredReferenceFolders();
  void watchUndoFeatureIds(QgsVectorLayer* layer);
  void remapUndoFeatureIdsAfterSave();
  void populateMapContextMenu(QMenu* menu, const QPoint& pos);
  void showLayerAreaSummary(QgsVectorLayer* layer, bool showRatio);

private slots:
  void exportMapGeoTiff();
  void newSurvey();
  void openVectorLayer();
  void importSoilShapefile();
  void downloadSoilTerrain();
  void downloadGeologyMap();
  void downloadRiverMap();
  void startReferenceDownload(ReferenceMapKind kind, const QgsRectangle& extent,
                              const QString& targetPath, const QString& apiKey = {});
  void saveProject();
  void saveProjectAs();
  // 작업공간을 조사 파일에 쓴다. 20초 타이머로 자동 호출하던 것을 없앴으므로,
  // 이제 「저장」과 닫기 확인에서만 불린다. 테스트도 이 이름으로 직접 부른다.
  bool persistSurveyWork();
  void extractEmbeddedReferenceVectors();
  // [pkg D2] F114/F133 via D1 API: user-clicked cleanup of stale app staging next to the survey.
  void cleanSurveyStaging();
  void openProject();
  void startEditSurveyArea();
  void startEditFeaturePoly();
  void startEditFeatureLine();
  void startEditSectionLine();
  void startEditArtifact();
  void startEasyDraw();
  void mergeFeaturePolygons();
  void clipOverlappingLayers();
  // 위에 그린 면 모양대로 아래 면에서 그 자리만 지운다(겹친 곳 지우기).
  void eraseOverlapWithShape();
  void startSplitPolygonTool();
  void startAttributeEditTool();
  void addUserLayer();
  void addControlPoint();
  void importControlCsv();
  void runChecklist();
  void exportPdf();
  void exportShpPackage();
  void setWorkCrs5186();
  void setWorkCrs5187();
  void convertSelectedTo5179();
  void startSelectTool();
  void startMeasureTool();
  void toggleTerrainMap();
  void openTopographicDownload();
  void importTopographicFolder();
  void showTopographicFiles(const QString& folder);
  void toggleDemMap();
  void importDemElevationRaster();
  void runDemHillshade();
  void editDemElevationClasses();
  void createSurveyContours();
  void startPaleoLandform();
  void startTrenchGrid();
  void placeTrenchGridAt(const QgsPointXY& origin);
  // Atomically replaces the previous grid and reports the excavation ratio
  // against the survey area when known. areaM2 <= 0 skips the ratio line.
  bool applyTrenchCells(const std::vector<TrenchGridGenerator::Cell>& cells, double areaM2,
                       double targetPct = 0.0, const QString& sourceCrs = {});
  void applyTrenchByRatio(double targetPct);
  // 속성 창의 「적용」: 자동 채움이면 구역 재배치, 아니면 기존 격자 중심을
  // 유지한 채 회전·간격만 바꿔 재배치. 격자가 없으면 원점 클릭으로 넘어간다.
  bool applyTrenchFromDialog();
  void beginTrenchOriginPick();
  void startTrenchGridMove();   // 전체 이동 모드
  void startTrenchGridEdit();   // 개별 편집 모드(그래픽식 선택·이동·삭제)
  void activateTrenchTool(bool single);
  // [pkg I] F087: copies the corner-bar grid settings (explicit 미터/경위도 kind, debounced
  // values) onto the canvas grid. announce = on/off or kind change → one status line.
  void applyMapGrid(bool announce);
  void addDaedongyeojidoMap();
  void addHistoryGisMap1919();
  void downloadCadastral();
  bool configureCadastralAccount();  // true when a new account was saved
  void configureCadastralStyle();
  void addBasemapOsm();
  void addBasemapGoogle();
  void removeSelectedLayers();
  void clearDrawnFeaturesOfCurrentLayer();
  // 지금 화면 범위의 배경 타일을 MBTiles로 받아 두고 그 파일로 바꿔 쓴다.
  // 그 뒤로는 네트워크를 타지 않아 팬·줌이 디스크 속도로 돈다.
  void saveOfflineTilePack();
  // [pkg G1] F060: read-only 「자료 준비 상태」 table, opened only by the user.
  void showReferenceStatus();
  void georefAssistant();
  void showSubToolsAlign();
  void showAbout();

  void configureVworldKey();
  void configureTopographicAccount();
  bool configureHeritageAccount();
  void updateTopographicDirectory(const QString& surveyPath);
  void exportReportLayout();
  static QString resolvedDesktopPath();
  void onMapContextMenu(const QPoint& pos);
  void onLayerTreeContextMenu(const QPoint& pos);
  void renameSelectedLayer(QgsMapLayer* layer = nullptr);
  void onLayerTreeDoubleClicked(const QModelIndex& index);
  void zoomMapToFullMax();
  void zoomSelectedLayerMax();
  void onCanvasScaleChanged(double scale);
  void applyMapScaleFromUi();
  void refreshMapCanvasNow();
  void showSubToolsDraw();
  void showSubToolsBuffer();
  // 조사구역이 속한 시/군의 국가유산 자료를 인트라넷에서 받아 참조 레이어로 올린다.
  // 묻는 것은 시/군 판정 확인 하나뿐이다.
  void fetchNearbyHeritage();
  void ensureHeritageBrowser();
  void openHeritageBrowserFor(const struct HeritageRegion& region, const QString& reason = {});
  HeritageImport::Result importHeritageDataset(HeritageDataset dataset, const QStringList& files);
  void saveHeritageAgreementReceipt(const QDateTime& when, const QString& terms);
  void showSubToolsBasemap();
  void hideSubTools();
  void runSiteBuffer500();
  void runSiteBuffer1000();
  void exportNearbyHeritageExcel();
  void applySnapConfig();
  void openLayoutDesigner();
  // 어느 화면에서든 인쇄(Ctrl+P). 도면이 없으면 도면 화면부터 연다.
  void printDrawing();
  void placeTerrain3dOnSheet();
  void openTerrain3dStudio();
  void openTerrain3dLayout();
  void applyTerrain3dSheetScale(int denominator);
  void refreshTerrain3dDrapeAndSheet();
  QString terrain3dSheetPngPath() const;
  void onViewTabCloseRequested(int index);
  void openRecentSurvey(const QString& path);
  void showMapWorkspace();
  void rememberSurvey(const QString& path, const QString& name);
  void undoLastAction();
  void redoLastAction();
  void deleteSelectedFeatures();
  void deleteFeaturesOrSelectedReferenceLayers();
  void updateUndoRedoActions();

private:
  void buildUi();
  void buildMenus();
  void updateNextActionStatus();
  void clearSubToolbar();
  // 지금 켜져 있는 도구에 파란 밑줄이 오게 체크 상태를 맞춘다.
  void updateSubToolbarChecks();
  // 큰 항공사진처럼 Qt 로 못 여는 원판은 GDAL 축소본으로 왼쪽 칸을 채운다.
  bool loadAlignPreviewFromRaster();
  // 레이어 체크를 한 번에 모두 끄고 켜는 단추. 라벨은 다음에 할 일을 보여 준다.
  // 레이어 순서를 글자(라벨)에도 그대로 먹인다.
  // 창 단축키(Delete·Ctrl+Z)를 열려 있는 조판 화면으로 넘긴다.
  bool routeEditKeyToActiveStudio(bool isDelete);
  void applyLabelStackOrder();
  void refreshAboveLabelsOverlay();
  void toggleAllLayersChecked();
  void refreshLayerCheckAllButton();
  bool tryAddDroppedPaths(const QStringList& paths);
  QStringList selectedBrowserFiles() const;
  void loadSurveyLayers(const QString& gpkgOrStub);
  // 내장(.gpkg) / 동반(.qgz) 두 복원 경로가 공유하는 마무리.
  void finishOpenedProject(const QString& gpkgPath, const QString& sourceLabel,
                           qint64 elapsedMs);
  void ensureDefaultBasemaps();
  // 주제도 아이콘의 눌림 상태를 범례에서 되읽는다. 레이어를 지우거나 체크를
  // 끄면 아이콘도 꺼져야 한다(범례가 진실).
  void syncThematicButtons();
  void updateHistoricalMapButtons();
  void syncRecordTools();
  // 조사구역 안 DEM 표고로 오르막 방위를 낸다(트렌치 장축 = 등고선 직교).
  void syncTrenchCoordinateGrid(const QString& areaCrs);
  void pickTrenchFromSelection();  // MainWindowTrench.cpp
  TrenchGridGenerator::SlopeAspect terrainAspectForArea(const QByteArray& areaWkb,
                                                       const QString& areaCrs);
  void applyStartupMap();
  void ensureStartupViewReady();
  void scheduleMapDisplayRefresh();
  void bindMapDisplayScreen();
  void setWorkCrs(const QString& authId);
  void searchLocation(const QString& query, bool parcel = false);
  void onLocationResults(const QVector<LocationHit>& hits);
  void onLocationFailed(const QString& message);
  void zoomToLocation(const LocationHit& hit);
  QJsonObject buildProjectState() const;
  QString rulesPath() const;
#if KA_HGIS_HAS_QGIS
  QgsVectorLayer* layerByKey(const QString& layerKey) const;
  QgsVectorLayer* ensureDomainLayerForEdit(const QString& layerKey, const QString& titleKo);
  void onLayerTreeRowsMoved();
  void startAlignSession(QgsMapLayer* layer);
  void stopAlignSession();
  void ensureAlignSplit();
  void showAlignSplit();
  void hideAlignSplit();
  void refreshAlignUi();
  void updateAlignOverlay();
  void trackAlignPointer(const QPoint& globalPos);
  void deleteSelectedAlignPoint();
  void applyAlignMove();
  void beginEdit(QgsVectorLayer* layer);
  bool refuseReadOnlyLayer(const QString& title, const QgsVectorLayer* layer);
  void onGeometryCaptured(const QgsGeometry& geom);
  void stopCaptureTool();
  void releaseRemovedLayersForSave();
  // [pkg B1] drawing helpers (KaDrawSketchTools.cpp). 완료·되돌리기·취소 and
  // 연속 그리기 on the draw sub-toolbar; the first three work only while a sketch has points.
  void addDrawSketchButtons();
  void syncDrawSketchButtons();
  // [pkg B1] Current tool chip in the status bar (도구: 이동 / 그리기 · 유구 면 · 점 3 …).
  void updateToolChip();
  QString currentToolLabel() const;
  // [pkg B1] Leaving the map tab keeps an unfinished sketch; returning shows it again.
  // parkSketchForOtherTab returns true when it kept the sketch (the caller then must not
  // call hideSubTools, which would throw the points away).
  bool parkSketchForOtherTab();
  void resumeParkedSketch();
  void ensureAttributeTool();
  void editFeatureAttributes(QgsVectorLayer* layer, const QgsFeature& feature);
  static QString attributeFieldLabelKo(const QString& fieldName);
#endif
  bool commitSurveyEdits(int* committedCount = nullptr);
  bool confirmSaveBeforeOpeningSurvey();
  bool validateStandaloneProjectForOpen(const QString& path);
  // 저장하지 않은 작업이 있는가. 프로젝트 dirty 플래그 + 커밋 안 된 편집 버퍼.
  bool surveyHasUnsavedChanges() const;
  // 저장 직후·열기 직후 호출해 "깨끗한 상태"로 되돌린다.
  void markSurveySaved();
  // 새 조사·다른 이름으로 저장이 처음 여는 폴더. 바탕화면이 OneDrive 로 리디렉션된
  // PC에서 기본값이 그리로 향하지 않도록, 마지막에 쓴 조사 폴더를 먼저 쓴다.
  QString preferredSurveyDir() const;
  void rememberSurveyDir(const QString& path);
  void refreshWindowTitle();
  void captureRecoverySnapshot();
  void offerRecoverySnapshot();
  void clearRecoveryOffer(const QString& recoveryDirectory);
  // [pkg D2] F073/F176/F038 (MainWindowSession.cpp): satellite·cadastral after open/new run on
  // the event loop; the survey save freezes the canvas around the GPKG write (the silent
  // recovery copy waits for an idle canvas instead); a failed save stays visible outside the
  // map tab. [v3 D2] waitForCanvasIdle (nested event loop) removed.
  void scheduleDefaultBasemaps();
  void reportSaveFailure(const QString& title, const QString& reason,
                         const QString& recoveryNote = QString(),
                         const QString& details = QString());
  // 레이어 점호. 직전과 달라졌으면 무엇이 사라졌는지 세션 로그에 적고 되살린다.
  void auditLayerHealth();
  // 조사를 연 뒤에도 원본 파일을 못 찾은 레이어를 알림 줄로 알린다(다른 PC에서 옮겨 온 조사).
  void reportMissingLayerFiles();
  void logLayerCensus(const QString& tag);
  // 화면에 실제로 무엇이 그려졌는지. 확대했을 때 위성이 비는 현상을 잡기 위한 계측 —
  // 캔버스가 그리기로 잡고 있는 레이어 목록과 축척을, 목록이 바뀔 때만 기록한다.
  void logCanvasPaintState();

  // Non-blocking feedback on the canvas. Reserve QMessageBox for questions and
  // for failures the user must acknowledge before anything else happens.
  enum class Notice { Info, Success, Warning, Critical };
  void notify(Notice level, const QString& title, const QString& text,
              const QString& details = QString());

  void refreshLayerEmptyState();
  QLabel* m_layerEmpty = nullptr;
  QToolButton* m_layerCheckAllBtn = nullptr;
  QLabel* m_help = nullptr;
  QLabel* m_checkView = nullptr;
  KaStatusBar* m_status = nullptr;
  QSplitter* m_leftSplit = nullptr;
  QFrame* m_layersCard = nullptr;
  QFrame* m_filesCard = nullptr;
  class KaFileBrowserPanel* m_filesPanel = nullptr;
  QListWidget* m_fileBrowser = nullptr;
  ChecklistEngine* m_checklist = nullptr;
  LocationSearch* m_locator = nullptr;
  QString m_surveyPath;
  class HeritageRegionResolver* m_heritageResolver = nullptr;
  QPointer<class KaHeritageBrowser> m_heritageBrowser;
  QString m_workCrs = QStringLiteral("EPSG:5187");
#if KA_HGIS_HAS_QGIS
  void healTileLayer(QgsRasterLayer* layer);
  void notifyBasemapFailure(bool timedOut, const QString& reason);
  // 배경지도 실패 안내는 세션에 한 번만. 예전에는 30초마다 다시 떠서 현장에서 계속 가렸다.
  bool m_basemapNoticeShown = false;
#endif
  // 타일 자동 복구 상태: 레이어별 재시도 횟수(화면 이동 시 초기화)와 예약 중 표시.
  QHash<QString, int> m_tileHealCount;
  QSet<QString> m_tileHealPending;
  int m_stubSurveyArea = 0;
  int m_stubFeatures = 0;
  int m_stubGcp = 0;
  bool m_stubHasMeta = false;
  bool m_packageCreated = false;
  mutable int m_lastChecklistErrors = -1;
#if KA_HGIS_HAS_QGIS
  QgsMapCanvas* m_canvas = nullptr;
  QgsLayerTreeView* m_layerTree = nullptr;
  KaCaptureMapTool* m_captureTool = nullptr;
  KaMeasureMapTool* m_measureTool = nullptr;
  QAction* m_actMeasure = nullptr;
  QAction* m_actSelect = nullptr;
  QAction* m_actUndo = nullptr;
  QAction* m_actRedo = nullptr;
  QAction* m_actGeology = nullptr;
  QAction* m_actRiver = nullptr;
  QToolButton* m_btnDraw = nullptr;
  QToolButton* m_btnSoil = nullptr;
  QToolButton* m_btnTerrain = nullptr;
  QToolButton* m_btnDem = nullptr;
  QToolButton* m_btnPaleo = nullptr;
  QToolButton* m_btnDaedong = nullptr;
  QToolButton* m_btnMap1919 = nullptr;
  QgsMapToolEmitPoint* m_trenchOriginTool = nullptr;
  class KaTrenchMoveTool* m_trenchMoveTool = nullptr;
  class KaTrenchDialog* m_trenchDlg = nullptr;
  class KaCanvasGridOverlay* m_mapGrid = nullptr;
  // 라벨 위에 위 레이어를 한 번 더 그리는 덧그림(2차 패스).
  KaAboveLabelsOverlay* m_aboveLabels = nullptr;
  int m_aboveLabelsCount = -1;
  bool m_labelOrderQueued = false;
  KaMapCornerBar* m_mapGridBar = nullptr;
  // [pkg I] F087: grid controls of the corner bar (KaShellGridControls.h).
  class KaShellGridControls* m_gridControls = nullptr;
  // [pkg I] F149: opt-in 지도 넓게 보기 / left-panel fold (KaShellFocus.h).
  class KaShellFocus* m_shellFocus = nullptr;
  KaAttributeMapTool* m_attributeTool = nullptr;
  KaLayerOpacityRail* m_layerOpacityRail = nullptr;
  KaAlignMapTool* m_alignTool = nullptr;
  KaAlignPickTool* m_alignPickTool = nullptr;
  KaImageView* m_alignImage = nullptr;
  QgsMapCanvas* m_alignLeftCanvas = nullptr;
  QWidget* m_alignLeftPane = nullptr;
  QLabel* m_alignLeftLabel = nullptr;
  QListWidget* m_alignPointList = nullptr;
  KaAlignLinkOverlay* m_alignOverlay = nullptr;
  QVector<QgsVertexMarker*> m_alignLeftMarks;
  QSplitter* m_mapSplitter = nullptr;
  bool m_alignApplied = false;
  bool m_alignCursorValid = false;
  bool m_alignLiveScreenValid = false;
  double m_alignCursorX = 0;
  double m_alignCursorY = 0;
  QPoint m_alignLiveScreen;
  QTimer* m_alignCursorTimer = nullptr;
  QgsMapToolPan* m_panTool = nullptr;
  KaFeatureSelectTool* m_featureSelectTool = nullptr;
  QgsVectorLayer* m_editLayer = nullptr;
  bool m_isSplittingPolygon = false;
  QgsLayerTreeMapCanvasBridge* m_bridge = nullptr;
  QgsMessageBar* m_messageBar = nullptr;
  QLineEdit* m_scaleEdit = nullptr;
  QComboBox* m_scaleCombo = nullptr;
  bool m_scaleUiGuard = false;
  bool m_extentClampGuard = false;
  QSplitter* m_mainSplit = nullptr;
  bool m_locationSearchBusy = false;
  bool m_startupViewApplied = false;
  bool m_workspaceRestoreFailed = false;  // .qgz를 못 읽어 외부 레이어가 빠진 채 열렸다
  // 복원되지 않은 작업공간은 덮어쓰지 않는다. 수동 저장도 새 사본으로 안내한다.
  bool m_workspaceRestoreSuppressesAutosave = false;
  bool m_basemapBootPending = false;
  // 조사 열기가 끝나기를 기다리며 다시 시도한 횟수. 0.3초 × 40 = 12초까지.
  int m_basemapBootRetries = 0;
  static constexpr int kBasemapBootRetryMax = 40;
  bool m_isLoadingBasemaps = false;
  bool m_isOpeningSurvey = false;
  bool m_canvasSyncQueued = false;
  bool m_closingWindow = false;
  bool m_canZoomPrevious = false;
  bool m_canZoomNext = false;
  QPointer<KaReferenceDownloadJob> m_referenceDownload;
  QPointer<KaTopographicBrowser> m_topographicBrowser;
  QPointer<KaTopographicImportDialog> m_topographicImport;
  quint64 m_surveyGeneration = 0;
  bool m_surveySessionReady = false;
  bool m_mapScreenBound = false;
  int m_displayRefreshWaits = 0;
  QTimer* m_displayRefresh = nullptr;
  // 20초 자동 저장은 없앴다. 저장은 사용자가 「저장」을 누를 때만 일어나고,
  // 저장 안 된 작업은 창 제목 뒤 * 와 닫기 확인창으로 알린다.
  // 2분 타이머는 원본을 쓰지 않고 복구사본/에만 미저장 조사 도형을 남긴다. 창·상태줄은 띄우지 않는다.
  QTimer* m_recoverySnapshotTimer = nullptr;
  bool m_recoverySnapshotBusy = false;
  bool m_recoveryOfferDone = false;
  // [pkg D2] F072/F116/F176: copy only layers with unsaved edits, skip when those edits did not
  // change since the last copy, and retry shortly instead of aborting an in-flight render.
  QByteArray m_recoverySignature;
  int m_recoverySnapshotRetries = 0;
  bool m_recoverySnapshotRetryQueued = false;
  // 레이어 사라짐 추적. 직전 점호 결과와 다를 때만 로그를 남긴다.
  QTimer* m_layerWatchTimer = nullptr;
  QString m_lastLayerCensus;
  QString m_lastCanvasPaintState;
  QStringList m_lastLayerKeys;
  QToolBar* m_subToolbar = nullptr;
  QString m_subToolsMode;
  bool m_snapEnabled = true;
  KaDrawingStudio* m_drawingStudio = nullptr;
  KaTerrain3dStudio* m_terrain3dStudio = nullptr;
  KaTerrain3dLayoutStudio* m_terrain3dLayoutStudio = nullptr;
  QAction* m_actMapGeoTiff = nullptr;
  QTabWidget* m_viewTabs = nullptr;
  KaStartPage* m_startPage = nullptr;
  KaAppBar* m_appBar = nullptr;
  QWidget* m_mapPage = nullptr;
  KaBeginnerRibbon* m_ribbon = nullptr;
  static QStringList mapOnlyRibbonGroups();
  void showMapTabFromStudio();
  struct KaUndoAction {
    enum Type { FeatureAdded, FeatureDeleted, FeatureChanged, AttributesChanged, LayerAdded, LayersRemoved,
                LayerTreeChanged /* [pkg E1] F186 */ };
    Type type = FeatureAdded;
    QString layerId;
    qint64 featureId = -1;
    QgsFeature featureData;
    QVector<QPair<QString, QgsFeature>> deletedFeatures;
    std::shared_ptr<KaRemovedLayers> removedLayers;
    std::shared_ptr<KaLayerTreeSnapshot> treeBefore;  // [pkg E1] F186: list order/checks before the user's drag or click
    QString description;
    quint64 seq = 0;        // [pkg B1] time on EditHistory's line
    quint64 commandId = 0;  // [pkg B1] layer-stack command this mirrors; 0 = standalone
  };
  QVector<KaUndoAction> m_undoActions;
  QSet<QString> m_undoObservedLayers;
  // [pkg E1] F186/F131 (KaLayerTreeUndo / KaLayerRefreshBatch, wired in MainWindow.cpp)
  std::unique_ptr<KaLayerTreeUndo> m_layerTreeUndo;
  std::unique_ptr<KaLayerRefreshBatch> m_layerRefreshBatch;
  // [int W1] F044 「선택한 유구」 card in the layer panel (MainWindowFeatureCard.cpp)
  KaFeatureCard* m_featureCard = nullptr;
  void setupFeatureCard(QWidget* host, QBoxLayout* layout);
  void syncFeatureCard(QgsMapLayer* layer);
  // [pkg B1] Ctrl+Z in time order across layer stacks and m_undoActions (KaEditUndoOrder.cpp).
  // Anything added to m_undoActions must go through pushUndoAction so it gets a time.
  EditHistory* m_editHistory = nullptr;
  EditHistory* editHistory();
  // Stamps the action (mirrors a command of stackLayer's undo stack when given) and caps
  // the history, releasing the oldest removed layers first.
  void pushUndoAction(KaUndoAction action, QgsVectorLayer* stackLayer = nullptr);
  QgsVectorLayer* newestUndoLayer();
  QgsVectorLayer* newestRedoLayer();
  int newestFallbackUndoIndex();
  void pruneUndoActions();
  // [pkg B1] drawing state (KaDrawSketchTools.cpp, MainWindowEditing.cpp)
  QLabel* m_toolChip = nullptr;
  bool m_continuousDraw = false;  // 연속 그리기: skip the name/number form after each shape
  // 여러 레이어를 한 번에 고친 편집(겹친 곳 지우기). 되돌리기 기록은 레이어마다 따로라서,
  // 묶어 두지 않으면 Ctrl+Z 한 번에 반만 돌아온다.
  struct KaLinkedEdit {
    struct Step {
      QString layerId;
      int index = 0;                          // 그 편집 직후의 되돌리기 위치
      const QUndoCommand* command = nullptr;  // 그 편집이 아직 그 자리에 있는지 확인하는 데 쓴다
      QString text;
    };
    QVector<Step> steps;
  };
  QVector<KaLinkedEdit> m_linkedEdits;
  QVector<KaLinkedEdit> m_linkedRedos;
  // 계획대로 지우고 결과를 알린다. 지웠으면 true.
  bool runErasePlan(const PolygonErase::Plan& plan);
  // 같은 레이어의 도형 위에 겹쳐 그렸으면 그 자리에 선택창(겹친 곳 지우기 / 그대로 두기)을 띄운다.
  // 지웠으면 true.
  bool offerEraseWithDrawnShape(QgsVectorLayer* layer, QgsFeatureId fid, const std::function<void()>& beforeErase = {});
  void forgetStaleLinkedEdits();
  void rememberLinkedEdit(const QList<QPointer<QgsVectorLayer>>& layers);
  bool undoLinkedEdit(QgsVectorLayer* layer);
  bool redoLinkedEdit(QgsVectorLayer* layer);
  // 방금 그린 면. 「겹친 곳 지우기」를 아무것도 고르지 않고 누르면 이 면을 골라 보여 준다.
  QPointer<QgsVectorLayer> m_lastDrawnLayer;
  QgsFeatureId m_lastDrawnFid = FID_NULL;
  QPointer<QProgressDialog> m_searchProgress;
#endif
  // [pkg A] 검수·제출: review dialog, submit package flow and go-to (KaSubmitFlow.cpp).
  // The ribbon chip 「검수·제출」 (Ctrl+E) opens openSubmitReview.
private slots:
  void openSubmitReview();
private:
  bool ensureChecklistRules();
  QVector<CheckResult> evaluateChecklist();
  void makeSubmitPackage();
  void goToCheckTargets(const CheckResult& result);
  void runCheckAction(const QString& action);
  // " · 종류·시대 빈 유구 N" for the status bar after drawing; empty when none.
  QString missingAttributeCounterText() const;
  bool m_submitBusy = false;

  // [P6 wiring] Strata shell (MainWindowChrome.cpp): the 「선택한 유구」 inspector as the third
  // pane of m_mainSplit (F10 folds it), the 「조사 열림 · 이름」 badge in the tab corner, the
  // drawing guide band over the map, the 「배경 지도」 card and the home 「설정」 buttons. The
  // widgets only display; every value flows in from here.
private:
  void setupStrataShell();
  // Home 「설정」 → the dialog that already owns that key or account, then the home reloads.
  void openAccountSetup(AccountStatus::Source source);
  // Badge, 「저장 안 됨 n건」 chip and the 「저장」 icon dot; calls within 300 ms are merged.
  void syncShellChips();
  // The card's 위성·지형·옛 지도 buttons mirror the legend and the ribbon (display only).
  void syncBasemapCard();
  // After a successful save: survey_area / feature_poly + feature_line counts for the home dots.
  void recordSurveyFacts();
  // Guide-band words for the active map tool (empty: the band shows undo/redo only).
  QString currentToolHint() const;
  QString currentToolIconId() const;
  KaInspectorPanel* m_inspector = nullptr;
  KaSurveyBadge* m_surveyBadge = nullptr;
  KaDrawGuideBand* m_drawGuide = nullptr;
  KaBasemapQuickCard* m_basemapCard = nullptr;
  QAction* m_actSave = nullptr;
  QTimer* m_shellSyncTimer = nullptr;
};
