#include "MainWindow.h"
#include "KaHgisVersion.h"
#include <QDateTime>
#include "KaStartupSplash.h"
#include "KaLayerInformation.h"
#include "KaWindowGeometry.h"
#include "core/DemPresentation.h"
#include "KaTheme.h"
#include "KaThemeOptions.h"
#include "KaUserError.h"
#include "KaIcons.h"
#include "KaMenuCaption.h"
#include "KaCaptureMapTool.h"
#include "KaAttributeMapTool.h"
#include "KaAlignMapTool.h"
#include "KaImageView.h"
#include "KaDrawingStudio.h"
#include "KaTerrain3dStudio.h"
#include "KaTerrain3dLayoutStudio.h"
#include "KaStartPage.h"
#include "KaMeasureMapTool.h"
#include "core/DemAnalyzer.h"
#include "core/TilePackService.h"
#include "core/TrenchGridGenerator.h"
#include "KaAboveLabelsOverlay.h"
#include "KaCanvasGridOverlay.h"
#include "KaTrenchMoveTool.h"
#include "KaFeatureSelectTool.h"
#include "KaShellFocus.h"
#include "KaShellUi.h"
#include "KaStatusBar.h"
#include "KaBeginnerRibbon.h"
#include "KaToolbarFit.h"
#include "KaSnapSettingsWidget.h"
#include "KaFeatureFormDialog.h"
#include "KaFileBrowserPanel.h"
#include "KaLayerOpacityRail.h"
#include "KaCrashGuard.h"
#include "KaReferenceDownloadJob.h"
#include <QProgressDialog>
#include <QElapsedTimer>
#include <QScopeGuard>
#include <exception>
#include "KaTrenchDialog.h"
#include "KaDemClassDialog.h"
#include "KaTopographicBrowser.h"
#include "KaTopographicImportDialog.h"
#include "KaSurveyAreaDialog.h"
#include "core/RecentSurveys.h"
#include "core/NearbyHeritageExport.h"
#include "core/KaSafeQgis.h"
#include "core/SurveyStorage.h"
#include "core/SurveySession.h"
#include "core/GeorefService.h"
#include "core/BufferAnalysis.h"
#include "core/ChecklistEngine.h"
#include "core/SurveyProjectFactory.h"
#include "core/Terrain3dLayoutService.h"
#include "core/LayerOps.h"
#include "KaHeritageBrowser.h"
#include "KaHeritageSetupDialogs.h"
#include "core/HeritageImport.h"
#include "core/HeritageIntranetSettings.h"
#include "core/HeritageRegionResolver.h"
#include "core/HeritageStyle.h"
#include "core/SoilMapService.h"
#include "core/PaleoLandformService.h"
#include "core/GeologyMapService.h"
#include "core/RiverMapService.h"
#include "core/VworldSettings.h"
#include "core/LocationSearch.h"
#include "core/CadastralImport.h"
#include "core/KoreaRegionCatalog.h"
#include "core/AdminBoundaryService.h"
#include "KaRegionLocator.h"
#include "KaAppBar.h"
#include "KaMapControls.h"
#include "core/WorkflowGuide.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <QApplication>
#include <QScreen>
#include <QShowEvent>
#include <QWindow>
#include <QTimer>
#include <QPointer>
#include <QCursor>
#include <QAction>
#include <QDockWidget>
#include <QScrollArea>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QTabBar>
#include <QTabWidget>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QToolBar>

#include <QVBoxLayout>
#include <QWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QTextStream>
#include <QFile>
#include <QShortcut>
#include <QKeySequence>
#include <QKeySequence>
#include <QAbstractItemView>
#include <QMenu>
#include <QPixmap>
#include <QImage>
#include <QAction>
#include <QSize>
#include <QListWidgetItem>
#include <QToolBar>
#include <QAction>
#include <QToolButton>
#include <QKeyEvent>
#include <QEvent>
#include <QMouseEvent>
#include <QModelIndex>
#include <QItemSelectionModel>
#include <QCompleter>
#include <QStringListModel>
#include <QInputDialog>
#include <QVector>
#include <QPair>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QCloseEvent>
#include <QSettings>
#include <QScopedValueRollback>
#include <QListWidget>
#include <QListWidgetItem>
#include <QSplitter>
#include <QFrame>
#include <QStandardPaths>
#include <QColor>
#include <QPalette>
#include <QUrl>
#include <QDesktopServices>
#include <QSizePolicy>
#include <QMimeData>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QStorageInfo>
#include <QHash>
#include <QMetaType>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QLocale>
#include <QLayout>

#if KA_HGIS_HAS_QGIS
#include <qgsmapcanvas.h>
#include <qgsmapmouseevent.h>
#include <qgsmessagebar.h>
#include <qgsproject.h>
#include <qgsmaplayerstyle.h>
#include <qgsmaplayer.h>
#include <qgsvectorlayer.h>
#include <qgsrasterlayer.h>
#include <qgssinglebandpseudocolorrenderer.h>
#include <qgsrasterrenderer.h>
#include <qgsrastertransparency.h>
#include <qgslayertree.h>
#include <qgslayertreegroup.h>
#include <qgslayertreenode.h>
#include <qgslayertreelayer.h>
#include <qgslayertreeview.h>
#include <qgslayertreemodel.h>
#include <qgslayertreemapcanvasbridge.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsmaptool.h>
#include <qgsmaptoolemitpoint.h>
#include <qgspointxy.h>
#include <qgsmaptoolpan.h>
#include <qgsmaptoolselect.h>
#include <qgssnappingconfig.h>
#include <qgssnappingutils.h>
#include <qgsrubberband.h>
#include <qgsapplication.h>
#include <qgsmessagelog.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgsvectorfilewriter.h>
#include <qgscoordinatetransform.h>
#include <qgscoordinatetransformcontext.h>
#include <qgsrectangle.h>
#include <qgsvertexmarker.h>
#include <qgsmaptopixel.h>
#include <qgspoint.h>
#include <qgspointlocator.h>
#include <qgsexception.h>
#include <qgsproviderregistry.h>
#include <qgsnetworkaccessmanager.h>
#include <qgsvectordataprovider.h>
#include <qgsprovidersublayerdetails.h>
#endif

void MainWindow::buildMenus() {
  menuBar()->setNativeMenuBar(false);
  menuBar()->hide();

  auto* mainTb = addToolBar(QStringLiteral("주요"));
  mainTb->setObjectName(QStringLiteral("mainToolbar"));
  mainTb->setIconSize(QSize(20, 20));
  mainTb->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  mainTb->setMovable(false);
  mainTb->setFloatable(false);
  mainTb->setAttribute(Qt::WA_InputMethodEnabled, true);

  // Place search joins the ribbon row at its right end (added after the ribbon below).
  // As a bar of its own it cost a whole row and ended up sharing the drawing tools' row.
  m_appBar = new KaAppBar(mainTb);
  connect(m_appBar, &KaAppBar::searchRequested, this,
          [this](const QString& q, bool lot) { searchLocation(q, lot); });
  auto* actFind = new QAction(QStringLiteral("주소·지번 찾기"), this);
  actFind->setShortcut(QKeySequence::Find);
  actFind->setShortcutContext(Qt::WindowShortcut);
  connect(actFind, &QAction::triggered, m_appBar, &KaAppBar::focusSearch);
  addAction(actFind);

  auto* ribbon = new KaBeginnerRibbon(mainTb);
  m_ribbon = ribbon;
  ribbon->addGroup(QStringLiteral("survey"), QStringLiteral("조사"));
  ribbon->addGroup(QStringLiteral("record"), QStringLiteral("기록"));
  ribbon->addGroup(QStringLiteral("fetch"), QStringLiteral("자료 받기"));
  ribbon->addGroup(QStringLiteral("basemap"), QStringLiteral("배경 지도"));
  ribbon->addGroup(QStringLiteral("align"), QStringLiteral("정합"));
  ribbon->addGroup(QStringLiteral("out"), QStringLiteral("내보내기"));
  ribbon->addGroup(QStringLiteral("more"), QStringLiteral("기타"));

  auto addIcon = [this, ribbon](const QString& group, const QString& iconId, const QString& text,
                                const QString& tip, auto slot) -> QPair<QAction*, QToolButton*> {
    auto* a = new QAction(KaIcons::icon(iconId), text, this);
    a->setToolTip(tip);
    connect(a, &QAction::triggered, this, slot);
    addAction(a);
    QToolButton* b = ribbon->addAction(group, a);
    return {a, b};
  };
  // 자주 누르는 단추(저장·도면·인쇄)만 진한 타일로 두드러지게 한다. 나머지는 옅은 타일이다.
  auto paintPrimary = [](QAction* a, const QString& iconId) {
    if (a) a->setIcon(KaIcons::strongIcon(iconId));
  };
  // 홈 화면·안내문이 모두 「새 조사」라고 부르므로 리본도 같은 이름을 쓴다.
  auto [actNew, btnNew] = addIcon(QStringLiteral("survey"), QStringLiteral("new"),
                                  QStringLiteral("새 조사"),
                                  QStringLiteral("현장 조사 프로젝트를 새로 만듭니다"),
                                  &MainWindow::newSurvey);
  auto [actOpen, btnOpen] = addIcon(QStringLiteral("survey"), QStringLiteral("open"),
                                    QStringLiteral("열기"),
                                    QStringLiteral("저장한 조사를 엽니다"), &MainWindow::openProject);
  auto [actSave, btnSave] = addIcon(QStringLiteral("survey"), QStringLiteral("save"),
                                    QStringLiteral("저장"),
                                    QStringLiteral("현재 조사를 저장합니다 (Ctrl+S)"), &MainWindow::saveProject);
  actNew->setShortcut(QKeySequence::New);
  actNew->setToolTip(QStringLiteral("새 조사 — 현장 조사 프로젝트를 새로 만듭니다 (Ctrl+N)"));
  actOpen->setShortcut(QKeySequence::Open);
  actOpen->setToolTip(QStringLiteral("저장한 조사를 엽니다 (Ctrl+O)"));
  actSave->setShortcut(QKeySequence::Save);
  auto [actSaveAs, btnSaveAs] = addIcon(QStringLiteral("survey"), QStringLiteral("save_as"),
                                        QStringLiteral("다른 이름"),
                                        QStringLiteral("작업 중인 모든 레이어를 다른 이름으로 저장합니다 (Ctrl+Shift+S)"),
                                        &MainWindow::saveProjectAs);
  actSaveAs->setShortcut(QKeySequence::SaveAs);
  Q_UNUSED(actNew);
  Q_UNUSED(actOpen);
  m_actSave = actSave;  // [P6] syncShellChips swaps this icon for save_unsaved (warn dot)
  Q_UNUSED(actSaveAs);
  paintPrimary(actSave, QStringLiteral("save"));
  btnNew->setObjectName(QStringLiteral("ribbonNew"));
  btnOpen->setObjectName(QStringLiteral("ribbonOpen"));
  btnSave->setObjectName(QStringLiteral("ribbonSave"));
  btnSaveAs->setObjectName(QStringLiteral("ribbonSaveAs"));

  auto [actSelect, btnSelect] = addIcon(
      QStringLiteral("record"), QStringLiteral("select"), QStringLiteral("선택"),
      QStringLiteral("그린 도형을 선택합니다. 다시 누르면 이동으로 돌아갑니다"),
      &MainWindow::startSelectTool);
  m_actSelect = actSelect;
  Q_UNUSED(btnSelect);
  m_actSelect->setCheckable(true);
  m_actSelect->setShortcut(QKeySequence(QStringLiteral("Ctrl+1")));
  // A = 도형선택, and the tooltip naming both keys. 맞추기 keeps the keyboard while it is open.
  KaFeatureSelectTool::installKeyShortcut(this, m_actSelect,
                                          [this]() { return m_subToolsMode == QLatin1String("align"); });
  auto* actUndo = new QAction(KaIcons::icon(QStringLiteral("undo")), QStringLiteral("되돌리기"), this);
  actUndo->setToolTip(QStringLiteral("마지막 그리기·정점·삭제를 되돌립니다 (Ctrl+Z)"));
  actUndo->setShortcut(QKeySequence::Undo);
  actUndo->setShortcutContext(Qt::WindowShortcut);
  connect(actUndo, &QAction::triggered, this, [this]() {
    if (routeEditKeyToActiveStudio(false)) return;
    undoLastAction();
  });
  addAction(actUndo);
  m_actUndo = actUndo;
  auto* actRedo = new QAction(KaIcons::icon(QStringLiteral("redo")), QStringLiteral("다시 실행"), this);
  actRedo->setToolTip(QStringLiteral("되돌린 편집을 다시 적용합니다 (Ctrl+Y)"));
  actRedo->setShortcut(QKeySequence::Redo);
  actRedo->setShortcutContext(Qt::WindowShortcut);
  connect(actRedo, &QAction::triggered, this, [this]() {
    redoLastAction();
  });
  addAction(actRedo);
  m_actRedo = actRedo;
  auto [actMeasure, btnMeasure] = addIcon(
      QStringLiteral("record"), QStringLiteral("measure"), QStringLiteral("측거"),
      QStringLiteral("지도에서 거리와 면적을 측정합니다. 다시 누르면 종료합니다"),
      &MainWindow::startMeasureTool);
  m_actMeasure = actMeasure;
  Q_UNUSED(btnMeasure);
  if (m_actMeasure)
    m_actMeasure->setCheckable(true);

  m_btnDraw = new QToolButton(ribbon);
  m_btnDraw->setObjectName(QStringLiteral("btnDraw"));
  m_btnDraw->setIcon(KaIcons::icon(QStringLiteral("draw_poly")));
  m_btnDraw->setText(QStringLiteral("그리기"));
  m_btnDraw->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  m_btnDraw->setCheckable(true);
  m_btnDraw->setToolTip(QStringLiteral("조사구역·유구 면과 선을 그립니다. 다시 누르면 도구를 닫고 이동합니다"));
  connect(m_btnDraw, &QToolButton::clicked, this, [this](bool on) {
    if (on) showSubToolsDraw();
    else hideSubTools();
    if (m_btnDraw)
      m_btnDraw->setChecked(m_subToolbar && m_subToolbar->isVisible() &&
                            m_subToolsMode == QLatin1String("draw"));
  });
  ribbon->addWidget(QStringLiteral("record"), m_btnDraw);
  auto* actDraw = new QAction(QStringLiteral("그리기"), this);
  actDraw->setShortcut(QKeySequence(QStringLiteral("Ctrl+D")));
  actDraw->setShortcutContext(Qt::WindowShortcut);
  actDraw->setToolTip(QStringLiteral("조사구역·유구 면과 선을 그립니다 (Ctrl+D)"));
  connect(actDraw, &QAction::triggered, this, [this]() {
    if (m_btnDraw) m_btnDraw->click();
  });
  addAction(actDraw);

  auto trenchAdded = addIcon(QStringLiteral("record"), QStringLiteral("trench_grid"), QStringLiteral("시굴격자"),
          QStringLiteral("조사구역이 있으면 바로 시굴격자를 놓고, 없으면 지도에서 놓을 자리를 누릅니다. 놓은 뒤에는 끌어 옮깁니다"),
          &MainWindow::startTrenchGrid);
  trenchAdded.second->setObjectName(QStringLiteral("btnTrenchGrid"));

  // 지형·DEM·토양·고지형·지질·수계·옛 지도는 리본에 그대로 둔다.
  m_btnTerrain = new QToolButton(ribbon);
  m_btnTerrain->setObjectName(QStringLiteral("btnTerrain"));
  m_btnTerrain->setIcon(KaIcons::icon(QStringLiteral("contour")));
  m_btnTerrain->setText(QStringLiteral("지형"));
  m_btnTerrain->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  m_btnTerrain->setCheckable(true);
  m_btnTerrain->setToolTip(
      QStringLiteral("등고·음영 지형도를 켭니다. 다시 누르면 숨깁니다"));
  connect(m_btnTerrain, &QToolButton::clicked, this, &MainWindow::toggleTerrainMap);
  connect(m_btnTerrain, &QToolButton::clicked, this, [this]() {
    QTimer::singleShot(0, this, [this]() { syncThematicButtons(); });
  });
  ribbon->addWidget(QStringLiteral("basemap"), m_btnTerrain);
  auto* topographic = new QToolButton(ribbon);
  topographic->setObjectName(QStringLiteral("btnTopographic"));
  topographic->setIcon(KaIcons::icon(QStringLiteral("topo_download")));
  topographic->setText(QStringLiteral("수치지형"));
  topographic->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  topographic->setToolTip(QStringLiteral(
      "HGIS 안에서 국토지리정보원에 로그인하고 수치지형도를 받습니다. 우클릭: 받아 둔 폴더 불러오기"));
  topographic->setPopupMode(QToolButton::MenuButtonPopup);
  auto* topographicMenu = new QMenu(topographic);
  topographicMenu->addAction(QStringLiteral("국토지리정보원에서 내려받기…"), this, &MainWindow::openTopographicDownload);
  topographicMenu->addAction(QStringLiteral("받아 둔 수치지형도 폴더…"), this, &MainWindow::importTopographicFolder);
  topographic->setMenu(topographicMenu);
  // The ribbon theme hides menu arrows, so the sub-menu opens on right-click like DEM and 토양.
  topographic->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(topographic, &QToolButton::customContextMenuRequested, this,
          [topographic, topographicMenu](const QPoint& pos) { topographicMenu->exec(topographic->mapToGlobal(pos)); });
  connect(topographic, &QToolButton::clicked, this, &MainWindow::openTopographicDownload);
  m_btnDem = new QToolButton(ribbon);
  m_btnDem->setObjectName(QStringLiteral("btnDem"));
  m_btnDem->setIcon(KaIcons::icon(QStringLiteral("dem")));
  m_btnDem->setText(QStringLiteral("DEM"));
  m_btnDem->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  m_btnDem->setCheckable(true);
  // F062: the one-click download is Copernicus GLO-30, a 30 m surface model (DSM). The chip
  // label stays the short 「DEM」; the tip says what the heights really are.
  m_btnDem->setToolTip(QStringLiteral(
      "고도를 색으로 보여 줍니다. 받는 자료는 지표모델(DSM) 30m · Copernicus라 나무·건물 높이가 섞여 있습니다.\n"
      "미세지형 판독에는 우클릭 → 「국토지리원 DEM 불러오기」를 쓰세요. 다시 누르면 숨깁니다. 우클릭: 상세 메뉴"));
  auto* demMenu = new QMenu(m_btnDem);
  demMenu->addAction(KaIcons::icon(QStringLiteral("dem")),
                      QStringLiteral("국토지리원 DEM 불러오기(.img)…"), this,
                      &MainWindow::importDemElevationRaster);
  demMenu->addAction(KaIcons::icon(QStringLiteral("dem")), QStringLiteral("DEM 파일로 음영 만들기…"),
                     this, &MainWindow::runDemHillshade);
  demMenu->addAction(KaIcons::icon(QStringLiteral("dem")), QStringLiteral("DEM 표현…"), this,
                     &MainWindow::editDemElevationClasses);
  demMenu->addAction(KaIcons::icon(QStringLiteral("survey_contour")), QStringLiteral("측량점으로 등고선…"),
                     this, &MainWindow::createSurveyContours);
  m_btnDem->setMenu(demMenu);
  m_btnDem->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_btnDem, &QToolButton::customContextMenuRequested, this,
          [this, demMenu](const QPoint& pos) { demMenu->exec(m_btnDem->mapToGlobal(pos)); });
  connect(m_btnDem, &QToolButton::clicked, this, &MainWindow::toggleDemMap);
  connect(m_btnDem, &QToolButton::clicked, this, [this]() {
    QTimer::singleShot(0, this, [this]() { syncThematicButtons(); });
  });
  ribbon->addWidget(QStringLiteral("fetch"), m_btnDem);
  auto* contour = new QToolButton(ribbon);
  contour->setObjectName(QStringLiteral("btnSurveyContour"));
  contour->setIcon(KaIcons::icon(QStringLiteral("survey_contour")));
  contour->setText(QStringLiteral("등고선"));
  contour->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  contour->setToolTip(QStringLiteral("측량 DXF·엑셀 좌표로 등고선과 해발 색 구간을 만듭니다"));
  connect(contour, &QToolButton::clicked, this, &MainWindow::createSurveyContours);
  ribbon->addWidget(QStringLiteral("basemap"), contour);
  m_btnSoil = new QToolButton(ribbon);
  m_btnSoil->setObjectName(QStringLiteral("btnSoil"));
  m_btnSoil->setIcon(KaIcons::icon(QStringLiteral("soil")));
  m_btnSoil->setText(QStringLiteral("토양"));
  m_btnSoil->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  m_btnSoil->setCheckable(true);
  m_btnSoil->setPopupMode(QToolButton::MenuButtonPopup);
  m_btnSoil->setToolTip(
      QStringLiteral("흙토람 토양도를 겹칩니다. 다시 누르면 숨깁니다. 우클릭: 상세 메뉴"));
  auto* soilMenu = new QMenu(m_btnSoil);
  soilMenu->addAction(KaIcons::icon(QStringLiteral("layer")),
                      QStringLiteral("분포지형 내려받기 — 현재 화면 범위"), this,
                      &MainWindow::downloadSoilTerrain);
  soilMenu->addAction(KaIcons::icon(QStringLiteral("open")),
                      QStringLiteral("토양도 파일 불러오기(SHP·GPKG)"), this,
                      &MainWindow::importSoilShapefile);
  m_btnSoil->setMenu(soilMenu);
  m_btnSoil->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_btnSoil, &QToolButton::customContextMenuRequested, this,
          [this, soilMenu](const QPoint& pos) { soilMenu->exec(m_btnSoil->mapToGlobal(pos)); });
  connect(m_btnSoil, &QToolButton::clicked, this, &MainWindow::downloadSoilTerrain);
  connect(m_btnSoil, &QToolButton::clicked, this, [this]() {
    QTimer::singleShot(0, this, [this]() { syncThematicButtons(); });
  });
  ribbon->addWidget(QStringLiteral("basemap"), m_btnSoil);
  m_btnPaleo = new QToolButton(ribbon);
  m_btnPaleo->setObjectName(QStringLiteral("btnPaleo"));
  m_btnPaleo->setIcon(KaIcons::icon(QStringLiteral("paleo")));
  m_btnPaleo->setText(QStringLiteral("고지형"));
  m_btnPaleo->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  m_btnPaleo->setToolTip(
      QStringLiteral("흙토람 입지 후보를 강조하고 구하도·자연제방 등 가설을 그립니다. 확정이 아닙니다"));
  connect(m_btnPaleo, &QToolButton::clicked, this, &MainWindow::startPaleoLandform);
  ribbon->addWidget(QStringLiteral("basemap"), m_btnPaleo);
  auto [actCadastral, btnCadastral] = addIcon(
      QStringLiteral("fetch"), QStringLiteral("cadastral"), QStringLiteral("지적"),
      QStringLiteral("조사구역 주변 5km 지적도를 받아 경계선과 지번을 표시합니다. 우클릭: 계정·선 색 설정"),
      &MainWindow::downloadCadastral);
  actCadastral->setObjectName(QStringLiteral("actionCadastralDownload"));
  auto* cadastralMenu = new QMenu(btnCadastral);
  cadastralMenu->addAction(QStringLiteral("VWorld 계정 설정"), this, &MainWindow::configureCadastralAccount);
  cadastralMenu->addAction(QStringLiteral("선 색·지번 표시"), this, &MainWindow::configureCadastralStyle);
  btnCadastral->setMenu(cadastralMenu);
  btnCadastral->setPopupMode(QToolButton::MenuButtonPopup);
  btnCadastral->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(btnCadastral, &QToolButton::customContextMenuRequested, this,
          [btnCadastral, cadastralMenu](const QPoint& pos) { cadastralMenu->exec(btnCadastral->mapToGlobal(pos)); });
  ribbon->addWidget(QStringLiteral("fetch"), topographic);
  auto* oldMaps = new QToolButton(ribbon);
  oldMaps->setObjectName(QStringLiteral("btnOldMaps"));
  oldMaps->setIcon(KaIcons::icon(QStringLiteral("old_map")));
  oldMaps->setText(QStringLiteral("옛 지도"));
  oldMaps->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  oldMaps->setPopupMode(QToolButton::InstantPopup);
  oldMaps->setToolTip(QStringLiteral("대동여지도와 1919년 조선지형도를 참조 지도로 올립니다"));
  auto* oldMenu = new QMenu(oldMaps);
  auto* daedong = oldMenu->addAction(KaIcons::icon(QStringLiteral("old_map")), QStringLiteral("대동여지도"),
                                    this, &MainWindow::addDaedongyeojidoMap);
  daedong->setObjectName(QStringLiteral("actionDaedongyeojido"));
  auto* map1919 = oldMenu->addAction(KaIcons::icon(QStringLiteral("old_topo")),
                                    QStringLiteral("1919 조선지형도 1:5만"),
                                    this, &MainWindow::addHistoryGisMap1919);
  map1919->setObjectName(QStringLiteral("actionMap1919"));
  oldMaps->setMenu(oldMenu);
  auto [actGeology, btnGeology] = addIcon(
      QStringLiteral("basemap"), QStringLiteral("geology"), QStringLiteral("지질"),
      QStringLiteral("KIGAM 1:5만 지질 색 위에 지형 음영을 겹칩니다. 다시 누르면 숨깁니다"),
      &MainWindow::downloadGeologyMap);
  m_actGeology = actGeology;
  Q_UNUSED(btnGeology);
  if (m_actGeology) m_actGeology->setCheckable(true);
  auto [actRiver, btnRiver] = addIcon(
      QStringLiteral("basemap"), QStringLiteral("river"), QStringLiteral("수계"),
      QStringLiteral("하천망을 겹칩니다. 다시 누르면 숨깁니다"),
      &MainWindow::downloadRiverMap);
  m_actRiver = actRiver;
  Q_UNUSED(btnRiver);
  if (m_actRiver) m_actRiver->setCheckable(true);
  for (QAction* thematic : {m_actGeology, m_actRiver}) {
    if (!thematic) continue;
    connect(thematic, &QAction::triggered, this, [this]() {
      QTimer::singleShot(0, this, [this]() { syncThematicButtons(); });
    });
  }
  ribbon->addWidget(QStringLiteral("basemap"), oldMaps);
  addIcon(QStringLiteral("align"), QStringLiteral("georef"), QStringLiteral("정합"),
          QStringLiteral("좌표없는 사진·CAD를 도면에 합치기"), &MainWindow::georefAssistant);

  auto* btnBuffer = new QToolButton(ribbon);
  btnBuffer->setObjectName(QStringLiteral("btnBuffer"));
  btnBuffer->setIcon(KaIcons::icon(QStringLiteral("buffer")));
  btnBuffer->setText(QStringLiteral("버퍼"));
  btnBuffer->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  btnBuffer->setCheckable(true);
  btnBuffer->setToolTip(QStringLiteral("주변유적 500M·1000M 표시하기 — 선택한 면 둘레에 거리 경계를 그립니다"));
  connect(btnBuffer, &QToolButton::clicked, this, [this, btnBuffer](bool on) {
    if (on) showSubToolsBuffer();
    else hideSubTools();
    btnBuffer->setChecked(m_subToolbar && m_subToolbar->isVisible()
                          && m_subToolsMode == QLatin1String("buffer"));
  });
  ribbon->addWidget(QStringLiteral("record"), btnBuffer);

  // 위 버튼은 이미 올라온 레이어에 버퍼를 그린다. 이 버튼은 자료를 받아 온다. 역할이 다르다.
  auto* btnHeritage = new QToolButton(ribbon);
  btnHeritage->setObjectName(QStringLiteral("btnHeritageFetch"));
  btnHeritage->setIcon(KaIcons::icon(QStringLiteral("heritage")));
  btnHeritage->setText(QStringLiteral("유산"));
  btnHeritage->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  btnHeritage->setToolTip(QStringLiteral(
      "조사구역이 속한 시·군의 국가유산 자료를 인트라넷에서 받아 참조 지도로 올립니다.\n"
      "받은 자료는 조사폴더 안에만 두며 포터블·제출물에 실리지 않습니다"));
  connect(btnHeritage, &QToolButton::clicked, this, &MainWindow::fetchNearbyHeritage);
  ribbon->addWidget(QStringLiteral("fetch"), btnHeritage);

  auto [actLayout, btnLayout] = addIcon(
      QStringLiteral("out"), QStringLiteral("pdf"), QStringLiteral("도면"),
      QStringLiteral("도면 만들기 — 종이에 지도를 올려 도면을 만듭니다 (Ctrl+L)"), &MainWindow::openLayoutDesigner);
  actLayout->setShortcut(QKeySequence(QStringLiteral("Ctrl+L")));
  Q_UNUSED(btnLayout);
  paintPrimary(actLayout, QStringLiteral("pdf"));
  // 인쇄는 사람들이 먼저 찾는 곳(첫 화면 리본)에도 둔다. 도면 화면 안의 「인쇄」와 같은 창을 연다.
  auto [actPrint, btnPrint] = addIcon(
      QStringLiteral("out"), QStringLiteral("print"), QStringLiteral("인쇄"),
      QStringLiteral("도면을 프린터로 찍거나, 큰 도면을 A3·A4 여러 장으로 나눠 찍습니다 (Ctrl+P)"),
      &MainWindow::printDrawing);
  actPrint->setShortcut(QKeySequence::Print);
  btnPrint->setObjectName(QStringLiteral("btnRibbonPrint"));
  paintPrimary(actPrint, QStringLiteral("print"));
  auto [actMapGeoTiff, btnMapGeoTiff] = addIcon(
      QStringLiteral("out"), QStringLiteral("geotiff"), QStringLiteral("GeoTIFF"),
      QStringLiteral("현재 지도에 보이는 범위와 레이어를 지도 좌표계 그대로 GeoTIFF로 저장합니다"),
      &MainWindow::exportMapGeoTiff);
  m_actMapGeoTiff = actMapGeoTiff;
  m_actMapGeoTiff->setObjectName(QStringLiteral("actionMapGeoTiff"));
  KaShellUi::setEnabledWithReason(m_actMapGeoTiff, false, KaShellUi::mapTabOnlyReason());
  btnMapGeoTiff->setObjectName(QStringLiteral("btnMapGeoTiff"));
  auto [actExport, btnExport] = addIcon(
      QStringLiteral("out"), QStringLiteral("export_convert"), QStringLiteral("검수·제출"),
      QStringLiteral("도면을 검수하고 제출 꾸러미(EPSG:5179 SHP·조사도면.pdf·MANIFEST)를 만듭니다. 오류는 고칠 곳을 보여 주고, 남은 채로 만들면 한 번 묻고 README에 적습니다. 레이어 하나만 바꾸는 「레이어 5179 변환」도 여기 있습니다 (Ctrl+E)."),
      &MainWindow::openSubmitReview);
  actExport->setShortcut(QKeySequence(QStringLiteral("Ctrl+E")));
  Q_UNUSED(btnExport);
  ribbon->applyTabOrder();

  // 기록·자료 받기·배경 지도·정합은 지도에서 일한다. 도면 탭에서 누르면 먼저
  // 지도 탭으로 넘어간다. 예전에는 보이지 않는 지도에 도구만 걸려 아무 일도 없어 보였다.
  for (const QString& id : mapOnlyRibbonGroups()) {
    QFrame* group = ribbon->group(id);
    if (!group) continue;
    for (QToolButton* button : group->findChildren<QToolButton*>()) {
      connect(button, &QAbstractButton::pressed, this, [this]() { showMapTabFromStudio(); });
      if (QAction* action = button->defaultAction())
        connect(action, &QAction::triggered, this, [this]() { showMapTabFromStudio(); });
    }
  }

  auto* region = new KaRegionLocator(m_appBar);
  region->setObjectName(QStringLiteral("regionLocator"));
  connect(region, &KaRegionLocator::searchRequested, this,
          [this](const QString& q) { searchLocation(q); });
  connect(region, &KaRegionLocator::parcelSearchRequested, this,
          [this](const QString& q) { searchLocation(q, true); });
  connect(region, &KaRegionLocator::regionSelected, this, [this](const QString& sido) {
    const auto bounds = KoreaRegionCatalog::overviewBounds(sido);
    if (!bounds || m_isOpeningSurvey) return;
    // Province chips navigate directly. An older address reply must not move
    // the map back after this newer user selection.
    if (m_locator) m_locator->cancel();
    m_locationSearchBusy = false;
    if (m_searchProgress) {
      m_searchProgress->hide();
      m_searchProgress->deleteLater();
      m_searchProgress = nullptr;
    }
    LocationHit hit;
    hit.title = sido;
    hit.west = bounds->west; hit.south = bounds->south;
    hit.east = bounds->east; hit.north = bounds->north;
    hit.lon = (hit.west + hit.east) * 0.5;
    hit.lat = (hit.south + hit.north) * 0.5;
    hit.hasBbox = true;
    zoomToLocation(hit);
  });
  m_appBar->setRegionWidget(region);

  auto* more = new QToolButton(ribbon);
  more->setIcon(KaIcons::icon(QStringLiteral("more")));
  more->setText(QStringLiteral("더보기"));
  more->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  more->setToolTip(QStringLiteral("가끔 쓰는 기능"));
  auto* moreMenu = new QMenu(more);
  QAction* loadGroup = moreMenu->addAction(KaIcons::icon(QStringLiteral("open")), QStringLiteral("벡터·도면 불러오기"),
                                          this, &MainWindow::openVectorLayer);
  moreMenu->addAction(KaIcons::icon(QStringLiteral("layer")),
                      QStringLiteral("참조 벡터를 조사 파일 밖으로…"), this,
                      &MainWindow::extractEmbeddedReferenceVectors);
  // [pkg D1] F114/F133: explicit, user-clicked cleanup of app staging left beside the survey.
  moreMenu->addAction(KaIcons::icon(QStringLiteral("layer")),
                      QStringLiteral("조사 폴더 임시 자료 정리…"), this,
                      &MainWindow::cleanSurveyStaging);
  moreMenu->addAction(KaIcons::icon(QStringLiteral("layer")),
                      QStringLiteral("토양도 SHP 불러오기"), this,
                      &MainWindow::importSoilShapefile);
  moreMenu->addAction(KaIcons::icon(QStringLiteral("import")), QStringLiteral("CSV 기준점"),
                      this, &MainWindow::importControlCsv);
  moreMenu->addAction(KaIcons::icon(QStringLiteral("georef")), QStringLiteral("맞추기"),
                      this, &MainWindow::georefAssistant);
  moreMenu->addAction(KaIcons::icon(QStringLiteral("map")), QStringLiteral("OSM 배경"),
                      this, &MainWindow::addBasemapOsm);
  moreMenu->addAction(KaIcons::icon(QStringLiteral("satellite")), QStringLiteral("Google 위성"),
                      this, &MainWindow::addBasemapGoogle);
  moreMenu->addSeparator();
  QAction* viewGroup = moreMenu->addAction(QStringLiteral("파일함 보이기/숨기기"), this, [this]() {
    if (auto* toggle = findChild<QToolButton*>(QStringLiteral("sidebarFilesToggle")))
      toggle->click();
  });
  // 작은 화면에서 지도를 넓히는 선택 기능. 창 단축키라 리본이 숨어 있어도 듣는다.
  auto* actMapFocus = new QAction(QStringLiteral("지도 넓게 보기"), this);
  actMapFocus->setObjectName(QStringLiteral("actionMapFocus"));
  actMapFocus->setShortcut(QKeySequence(KaShellFocus::mapFocusKey()));
  actMapFocus->setShortcutContext(Qt::WindowShortcut);
  actMapFocus->setToolTip(QStringLiteral("리본과 왼쪽 패널을 잠시 숨겨 지도를 넓게 봅니다. 다시 누르면 돌아옵니다 (%1)")
                              .arg(KaShellFocus::mapFocusKey()));
  connect(actMapFocus, &QAction::triggered, this, [this]() {
    if (m_shellFocus) m_shellFocus->toggleMapFocus();
  });
  addAction(actMapFocus);
  moreMenu->addAction(actMapFocus);
  auto* actLeftPanel = new QAction(QStringLiteral("왼쪽 패널 접기/펴기"), this);
  actLeftPanel->setObjectName(QStringLiteral("actionToggleLeftPanel"));
  actLeftPanel->setShortcut(QKeySequence(KaShellFocus::leftPanelKey()));
  actLeftPanel->setShortcutContext(Qt::WindowShortcut);
  actLeftPanel->setToolTip(QStringLiteral("레이어·파일함 패널을 접거나 다시 폅니다 (%1)")
                               .arg(KaShellFocus::leftPanelKey()));
  connect(actLeftPanel, &QAction::triggered, this, [this]() {
    if (m_shellFocus) m_shellFocus->toggleLeftPanel();
  });
  addAction(actLeftPanel);
  moreMenu->addAction(actLeftPanel);
  QAction* keyGroup = moreMenu->addAction(QStringLiteral("API 키 입력"), this, &MainWindow::configureVworldKey);
  moreMenu->addAction(QStringLiteral("VWorld 지적도 아이디·비밀번호"), this, &MainWindow::configureCadastralAccount);
  moreMenu->addAction(QStringLiteral("수치지형도 아이디·비밀번호"), this, &MainWindow::configureTopographicAccount);
  moreMenu->addAction(QStringLiteral("국가유산 인트라넷 아이디·비밀번호"), this, &MainWindow::configureHeritageAccount);
  // [pkg G1] F060: read-only reference/offline readiness table, opened only on click.
  moreMenu->addAction(KaIcons::icon(QStringLiteral("layer")), QStringLiteral("자료 준비 상태"), this,
                      &MainWindow::showReferenceStatus)->setObjectName(QStringLiteral("actionReferenceStatus"));
  // [pkg H] F081/F153/F154: opt-in 「화면 보기」 (고대비·큰 글씨); the ribbon itself does not change.
  moreMenu->addMenu(KaTheme::createDisplayOptionsMenu(moreMenu));
  moreMenu->addAction(QStringLiteral("정보"), this, &MainWindow::showAbout);
  auto* webMenu = moreMenu->addMenu(KaIcons::icon(QStringLiteral("web")), QStringLiteral("웹 자료"));
  webMenu->menuAction()->setObjectName(QStringLiteral("actionWebSources"));
  auto addWeb = [this, webMenu](const QString& iconId, const QString& text, const QString& url) {
    QAction* action = webMenu->addAction(KaIcons::icon(iconId), text);
    QObject::connect(action, &QAction::triggered, this, [url]() {
      QDesktopServices::openUrl(QUrl(url));
    });
  };
  addWeb(QStringLiteral("upload"), QStringLiteral("문화재 GIS 인트라넷"),
         QStringLiteral("https://intranet.gis-heritage.go.kr/"));
  addWeb(QStringLiteral("layer"), QStringLiteral("농진청 토양도"),
         QStringLiteral("http://soil.rda.go.kr/geoweb/soilmain.do"));
  addWeb(QStringLiteral("cadastral"), QStringLiteral("VWorld 지적도 자료"),
         QStringLiteral("https://www.vworld.kr/dtmk/dtmk_ntads_s002.do?dsId=30563"));
  addWeb(QStringLiteral("contour"), QStringLiteral("국토정보맵 지형도"),
         QStringLiteral("https://map.ngii.go.kr/ms/map/NlipMap.do"));
  webMenu->addSeparator();
  webMenu->addAction(KaIcons::icon(QStringLiteral("layer")),
                     QStringLiteral("토양도 SHP 불러오기(흙토람 다운로드)…"), this,
                     &MainWindow::importSoilShapefile);
  KaMenuCaption::install(moreMenu, {{QStringLiteral("불러오기·배경"), loadGroup}, {QStringLiteral("화면"), viewGroup},  // 보드 판 5
      {QStringLiteral("계정과 키"), keyGroup}, {QStringLiteral("상태와 정보"), moreMenu->findChild<QAction*>(QStringLiteral("actionReferenceStatus"))}});
  more->setMenu(moreMenu);
  more->setPopupMode(QToolButton::InstantPopup);
  ribbon->addWidget(QStringLiteral("more"), more);
  syncRecordTools();
  mainTb->addWidget(ribbon);
  // The ribbon expands and keeps its buttons packed left, so place search ends up on the right.
  mainTb->addWidget(m_appBar);
  updateHistoricalMapButtons();

  m_subToolbar = addToolBar(QStringLiteral("세부도구"));
  m_subToolbar->setObjectName(QStringLiteral("subToolbar"));
  m_subToolbar->setIconSize(QSize(20, 20));
  m_subToolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  m_subToolbar->setMovable(false);
  m_subToolbar->setVisible(false);
  // Flat chrome: the QSS draws a 1px top/bottom rule for this row instead of a drop shadow.
  insertToolBarBreak(m_subToolbar);
  KaToolbarFit::install(m_subToolbar);  // no » fold: labels hide first, then the icons shrink
}

void MainWindow::clearSubToolbar() {
#if KA_HGIS_HAS_QGIS
  if (!m_subToolbar) return;
  m_subToolbar->clear();
#endif
}

// 어떤 도구가 켜져 있는지는 실제 지도 도구와 편집 대상 레이어가 말해 준다.
// 버튼마다 따로 플래그를 두면 어긋나므로 여기서 한 곳에서만 판단한다.
// QSS 의 QToolBar#subToolbar QToolButton:checked 가 파란 밑줄을 그린다.
void MainWindow::updateSubToolbarChecks() {
#if KA_HGIS_HAS_QGIS
  if (!m_subToolbar || !m_canvas) return;
  QString active;
  if (m_featureSelectTool && m_canvas->mapTool() == m_featureSelectTool) {
    active = QStringLiteral("select");
  } else if (m_captureTool && m_canvas->mapTool() == m_captureTool && m_editLayer) {
    const QString key = LayerOps::layerKeyOf(m_editLayer);
    if (m_editLayer->name() == QLatin1String("쉽게그리기"))
      active = QStringLiteral("easy");
    else if (key == QLatin1String("survey_area"))
      active = QStringLiteral("area");
    else if (key == QLatin1String("feature_poly"))
      active = QStringLiteral("poly");
    else if (key == QLatin1String("feature_line"))
      active = QStringLiteral("line");
    else if (key.startsWith(QLatin1String("artifact")))
      active = QStringLiteral("artifact");
  }
  for (QAction* a : m_subToolbar->actions()) {
    if (!a) continue;
    const QString id = a->property("kaSubTool").toString();
    if (id.isEmpty()) continue;
    a->setCheckable(true);
    const QSignalBlocker block(a);
    a->setChecked(id == active);
  }
#endif
}

void MainWindow::hideSubTools() {
#if KA_HGIS_HAS_QGIS
  if (m_subToolsMode == QLatin1String("align"))
    stopAlignSession();
  if (m_subToolsMode == QLatin1String("draw"))
    stopCaptureTool();
  clearSubToolbar();
  if (m_subToolbar) m_subToolbar->setVisible(false);
  m_subToolsMode.clear();
  if (auto* b = findChild<QToolButton*>(QStringLiteral("btnDraw"))) b->setChecked(false);
  if (auto* b = findChild<QToolButton*>(QStringLiteral("btnBasemap"))) b->setChecked(false);
  if (auto* b = findChild<QToolButton*>(QStringLiteral("btnSubmit"))) b->setChecked(false);
  if (auto* b = findChild<QToolButton*>(QStringLiteral("btnBuffer"))) b->setChecked(false);
#endif
}

void MainWindow::showSubToolsBuffer() {
#if KA_HGIS_HAS_QGIS
  if (!m_subToolbar) return;
  if (m_subToolsMode == QLatin1String("align")) stopAlignSession();
  if (m_subToolsMode == QLatin1String("buffer") && m_subToolbar->isVisible()) {
    hideSubTools();
    return;
  }
  clearSubToolbar();
  m_subToolsMode = QStringLiteral("buffer");
  auto* lab = new QLabel(QStringLiteral("  주변유적경계 › "));
  lab->setObjectName(QStringLiteral("subToolbarCaption"));
  m_subToolbar->addWidget(lab);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("buffer")), QStringLiteral("500m"),
                          this, &MainWindow::runSiteBuffer500);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("buffer")), QStringLiteral("1000m"),
                          this, &MainWindow::runSiteBuffer1000);
  auto* excelAct = m_subToolbar->addAction(KaIcons::icon(QStringLiteral("export")),
                                           QStringLiteral("엑셀 내보내기"),
                                           this, &MainWindow::exportNearbyHeritageExcel);
  excelAct->setObjectName(QStringLiteral("actBufferExcel"));
  excelAct->setToolTip(QStringLiteral(
      "버퍼(500m·1000m) 안에 포함되는 주변유적의 유적명과 조사구역 기준 방향·거리(m)를 엑셀(.xlsx)로 저장합니다"));
  auto* closeAct = m_subToolbar->addAction(QStringLiteral("닫기"));
  connect(closeAct, &QAction::triggered, this, &MainWindow::hideSubTools);
  m_subToolbar->setVisible(true);
  statusBar()->showMessage(
      QStringLiteral("500m·1000m 경계를 그리거나 「엑셀 내보내기」로 범위 안 주변유적(유적명·방향·거리)을 저장하세요."),
      8000);
#endif
}
void MainWindow::runSiteBuffer500() {
#if KA_HGIS_HAS_QGIS
  auto* src = NearbyHeritageExport::resolveSourceLayer(
      QgsProject::instance(), m_layerTree ? qobject_cast<QgsVectorLayer*>(m_layerTree->currentLayer()) : nullptr);
  QString err;
  if (!BufferAnalysis::addDistanceRing(QgsProject::instance(), m_canvas, src, 500.0, &err))
    return notify(Notice::Warning, QStringLiteral("주변유적경계"), QStringLiteral("500m 경계를 그리지 못했습니다."), err);
  const auto near = NearbyHeritageExport::collect(QgsProject::instance(), src, 500.0);
  statusBar()->showMessage(near.ok() ? QStringLiteral("주변 500m 경계를 그렸습니다 — 포함 주변유적 %1곳 (「엑셀 내보내기」로 저장)").arg(near.items.size())
                                     : QStringLiteral("주변 500m 경계를 그렸습니다"), 7000);
#endif
}
void MainWindow::runSiteBuffer1000() {
#if KA_HGIS_HAS_QGIS
  auto* src = NearbyHeritageExport::resolveSourceLayer(
      QgsProject::instance(), m_layerTree ? qobject_cast<QgsVectorLayer*>(m_layerTree->currentLayer()) : nullptr);
  QString err;
  if (!BufferAnalysis::addDistanceRing(QgsProject::instance(), m_canvas, src, 1000.0, &err))
    return notify(Notice::Warning, QStringLiteral("주변유적경계"), QStringLiteral("1000m 경계를 그리지 못했습니다."), err);
  const auto near = NearbyHeritageExport::collect(QgsProject::instance(), src, 1000.0);
  statusBar()->showMessage(near.ok() ? QStringLiteral("주변 1000m 경계를 그렸습니다 — 포함 주변유적 %1곳 (「엑셀 내보내기」로 저장)").arg(near.items.size())
                                     : QStringLiteral("주변 1000m 경계를 그렸습니다"), 7000);
#endif
}

void MainWindow::showSubToolsDraw() {
#if KA_HGIS_HAS_QGIS
  if (!m_subToolbar) return;
  if (m_subToolsMode == QLatin1String("align")) stopAlignSession();
  if (m_subToolsMode == QLatin1String("draw") && m_subToolbar->isVisible()) {
    hideSubTools();
    return;
  }
  if (m_viewTabs && m_mapPage && m_viewTabs->currentWidget() != m_mapPage)
    showMapWorkspace();
  clearSubToolbar();
  m_subToolsMode = QStringLiteral("draw");
  auto* lab = new QLabel(QStringLiteral("  그리기 › "));
  lab->setObjectName(QStringLiteral("subToolbarCaption"));
  m_subToolbar->addWidget(lab);
  auto* selAct = m_subToolbar->addAction(KaIcons::icon(QStringLiteral("select")),
                                        QStringLiteral("도형선택"),
                                        this, &MainWindow::startSelectTool);
  selAct->setProperty("kaSubTool", QStringLiteral("select"));
  selAct->setToolTip(QStringLiteral(
      "도형을 클릭하면 수정점이 나옵니다.\n"
      "점을 끌면 그 점만 옮겨집니다.\n"
      "점 위에서 우클릭하면 그 점을 지우고, 선 위에서 우클릭하면 점을 넣습니다.\n"
      "Ctrl을 누른 채 끌면 다른 도형의 점·선에 붙습니다. 점을 클릭해 고른 뒤 Delete로 지웁니다.\n"
      "겹친 도형은 같은 자리를 다시 누르면 다음 도형이 골라집니다. 단축키 A. 면 안쪽 도형은 클릭 후 Delete."));
  auto* snap = new KaSnapSettingsWidget(m_subToolbar);
  snap->syncFromProject();
  connect(snap, &KaSnapSettingsWidget::settingsChanged, this, [this, snap]() {
    m_snapEnabled = LayerOps::readSnapSettings(QgsProject::instance()).enabled;
    applySnapConfig();
    snap->syncFromProject();
    statusBar()->showMessage(m_snapEnabled
                                 ? QStringLiteral("자석 켜짐 — 조사 도형·지적 선에 붙습니다. 위성 그림은 제외")
                                 : QStringLiteral("자석 꺼짐"),
                             4000);
  });
  m_subToolbar->addWidget(snap);
  auto* easyAct = m_subToolbar->addAction(KaIcons::icon(QStringLiteral("easy_draw")),
                                          QStringLiteral("쉽게그리기"),
                                          this, &MainWindow::startEasyDraw);
  easyAct->setProperty("kaSubTool", QStringLiteral("easy"));
  easyAct->setToolTip(QStringLiteral(
      "쉽게그리기 — 이미 찍은 점 가까이에 마우스를 가져가면 그 점에 딱 붙어 자동으로 찍힙니다.\n"
      "잘못 찍었으면 Ctrl+Z를 누르세요. 바로 앞 점으로 되돌아갑니다."));
  auto* areaAct = m_subToolbar->addAction(KaIcons::icon(QStringLiteral("draw_area")),
                                          QStringLiteral("조사구역 그리기"),
                                          this, &MainWindow::startEditSurveyArea);
  areaAct->setProperty("kaSubTool", QStringLiteral("area"));
  areaAct->setToolTip(QStringLiteral(
      "조사구역(발굴 범위) 면을 그립니다. 점을 차례로 찍고 Enter로 닫습니다.\n"
      "Ctrl+Z는 마지막 점을 되돌립니다."));
  auto* polyAct = m_subToolbar->addAction(KaIcons::icon(QStringLiteral("draw_poly")),
                                          QStringLiteral("유구 그리기"),
                                          this, &MainWindow::startEditFeaturePoly);
  polyAct->setProperty("kaSubTool", QStringLiteral("poly"));
  polyAct->setToolTip(QStringLiteral(
      "유구의 면(수혈·주거지 등)을 그립니다. 점을 차례로 찍고 Enter로 닫습니다.\n"
      "Ctrl+Z는 마지막 점을 되돌립니다."));
  auto* lineAct = m_subToolbar->addAction(KaIcons::icon(QStringLiteral("draw_line")),
                                          QStringLiteral("유구 선"),
                                          this, &MainWindow::startEditFeatureLine);
  lineAct->setProperty("kaSubTool", QStringLiteral("line"));
  lineAct->setToolTip(QStringLiteral(
      "유구의 선(석렬·구 등)을 그립니다. 점을 차례로 찍고 Enter로 끝냅니다."));
  auto* artiAct = m_subToolbar->addAction(KaIcons::icon(QStringLiteral("artifact")),
                                          QStringLiteral("유물위치표시"),
                                          this, &MainWindow::startEditArtifact);
  artiAct->setProperty("kaSubTool", QStringLiteral("artifact"));
  artiAct->setToolTip(QStringLiteral(
      "유물이 나온 자리를 점으로 찍어 표시합니다. 한 번 클릭에 한 점입니다."));
  addDrawSketchButtons();  // [pkg B1] F085/F150: 완료·되돌리기·취소 (only while sketching) + 연속 그리기

  // 폴리곤 묶기·나누기는 「그리는 도구」가 아니라 「이미 그린 면을 고치는 도구」다.
  // 예전에는 늘어나는 빈칸으로 오른쪽 끝까지 밀어 두었는데, 넓은 화면에서는 그리기
  // 버튼과 너무 멀어져 한눈에 들어오지 않았다. 빈칸을 빼고 구분선만 두어
  // 그리기 버튼 바로 옆(가운데 쪽)에 묶기·나누기·구간 분리·겹친 곳 지우기·닫기를 같이 보이게 한다.
  m_subToolbar->addSeparator();
  auto* mergeAct = m_subToolbar->addAction(QStringLiteral("폴리곤 묶기"),
                                           this, &MainWindow::mergeFeaturePolygons);
  mergeAct->setToolTip(QStringLiteral(
      "여러 면을 하나로 묶습니다.\n"
      "쓰는 법: [도형선택]을 누르고 Shift를 누른 채 지도에서 면을 두 개 이상 고른 뒤\n"
      "[폴리곤 묶기]를 누르면 고른 면들이 하나로 합쳐집니다."));
  auto* splitAct = m_subToolbar->addAction(QStringLiteral("폴리곤 나누기"),
                                           this, &MainWindow::startSplitPolygonTool);
  splitAct->setToolTip(QStringLiteral(
      "[폴리곤 묶기]로 하나가 된 면을 다시 나눕니다.\n"
      "쓰는 법: 면을 가로지르는 선을 클릭해 그리고 우클릭하면 그 선을 따라 갈라집니다.\n"
      "(Shift로 도형 두 개를 고른 뒤 눌러도 겹친 구간에서 나뉩니다.)"));
  auto* clipAct = m_subToolbar->addAction(QStringLiteral("구간 분리"),
                                          this, &MainWindow::clipOverlappingLayers);
  clipAct->setToolTip(QStringLiteral(
      "겹쳐 있는 두 레이어에서 겹치는 구간만 새 레이어로 떼어 냅니다. 원본은 그대로 남습니다."));
  auto* eraseAct = m_subToolbar->addAction(QStringLiteral("겹친 곳 지우기"),
                                           this, &MainWindow::eraseOverlapWithShape);
  eraseAct->setObjectName(QStringLiteral("actEraseOverlap"));
  eraseAct->setToolTip(QStringLiteral(
      "위에 그린 도형 모양대로 아래 도형에서 그 자리만 지웁니다(클립).\n"
      "쓰는 법: 같은 레이어의 도형 위에 겹쳐 그리고 우클릭으로 마치면 선택창에서 고를 수 있습니다.\n"
      "이미 그린 도형은 [도형선택](A)으로 고른 뒤 이 단추를 누릅니다.\n"
      "아래 도형에 구멍이 나고 위에 그린 도형은 없어집니다. Ctrl+Z로 되돌립니다."));
  auto* closeAct = m_subToolbar->addAction(QStringLiteral("닫기"));
  closeAct->setToolTip(QStringLiteral("그리기 도구 모음을 닫고 지도 이동으로 돌아갑니다"));
  connect(closeAct, &QAction::triggered, this, &MainWindow::hideSubTools);
  m_subToolbar->setVisible(true);
  updateSubToolbarChecks();
  statusBar()->showMessage(
      QStringLiteral("그릴 도구를 고르세요 — 조사구역·유구·유구 선·유물 가운데 하나를 누르면 그리기가 시작됩니다."), 8000);
#endif
}

void MainWindow::showSubToolsBasemap() {
#if KA_HGIS_HAS_QGIS
  hideSubTools();
  ensureDefaultBasemaps();
  showMapWorkspace();
  statusBar()->showMessage(QStringLiteral("위성과 지적도는 시작할 때 자동으로 올라옵니다."), 6000);
#endif
}

