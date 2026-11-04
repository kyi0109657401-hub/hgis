#include "MainWindow.h"
#include "core/KaLogExcept.h"
#include "KaHgisVersion.h"
#include <QDateTime>
#include "KaStartupSplash.h"
#include "KaLayerInformation.h"
#include "KaLayerListChrome.h"  // [P6] 레이어 찾기 · 편집 중 ✎
#include "KaLayerRefreshBatch.h"
#include "KaLayerTreeUndo.h"
#include "KaWindowGeometry.h"
#include "core/DemPresentation.h"
#include "KaTheme.h"
#include "KaUserError.h"
#include "KaMapPagePlate.h"
#include "KaClosePrompt.h"
#include "KaControlCsvBox.h"
#include "KaTabClose.h"
#include "core/EditBufferSummary.h"
#include "KaCadImport.h"
#include "KaIcons.h"
#include "KaCaptureMapTool.h"
#include "KaAttributeMapTool.h"
#include "KaAlignMapTool.h"
#include "KaImageView.h"
#include "KaDrawingStudio.h"
#include "KaTerrain3dStudio.h"
#include "KaTerrain3dLayoutStudio.h"
#include "KaStartPage.h"
#include "KaMeasureMapTool.h"
#include "KaEditErrors.h"
#include "core/BasemapDsm.h"
#include "core/DemAnalyzer.h"
#include "core/PaleoAnalysisRunner.h"
#include "core/TilePackService.h"
#include "core/TrenchGridGenerator.h"
#include "KaAboveLabelsOverlay.h"
#include "KaCanvasGridOverlay.h"
#include "KaTrenchMoveTool.h"
#include "KaFeatureSelectTool.h"
#include "KaParcelCopyKeys.h"
#include "KaShellFocus.h"
#include "KaShellGridControls.h"
#include "KaGridOriginButton.h"
#include "KaShellUi.h"
#include "KaStatusBar.h"
#include "KaBeginnerRibbon.h"
#include "KaSnapSettingsWidget.h"
#include "KaFeatureFormDialog.h"
#include "KaFileBrowserPanel.h"
#include "KaLayerOpacityRail.h"
#include "KaCrashGuard.h"
#include "core/KaSessionLog.h"
#include "KaReferenceDownloadJob.h"
#include <QProgressDialog>
#include <QElapsedTimer>
#include <QScopeGuard>
#include <exception>
#include "KaTrenchDialog.h"
#include "core/TrenchLayerEdit.h"  // [pkg B2] F003/F156 grid placement
#include "core/TrenchPlanCache.h"  // [pkg B2] F097 cached 10%/2% plans
#include "core/TrenchTerrain.h"    // 시굴 사면 방향 from the 수치지형도 contours without a DEM
#include "core/BasemapDsm.h"       // [int W1] DEM lookup by kind, not by title
#include "KaDemClassDialog.h"
#include "KaTopographicBrowser.h"
#include "KaTopographicImportDialog.h"
#include "KaSurveyAreaDialog.h"
#include "core/RecentSurveys.h"
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
#include <QEventLoop>
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
#include <QGridLayout>

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

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle(QStringLiteral("Strata"));
  setWindowIcon(KaIcons::appIcon());
  resize(1280, 900);
  m_status = new KaStatusBar(this);
  setStatusBar(m_status);
  m_checklist = new ChecklistEngine(this);
  m_checklist->loadRules(rulesPath());
  m_locator = new LocationSearch(this);
  connect(m_locator, &LocationSearch::finished, this, [this](const QVector<LocationHit>& hits) {
    onLocationResults(hits);
  });
  connect(m_locator, &LocationSearch::failed, this, [this](const QString& msg) {
    onLocationFailed(msg);
  });
  buildMenus();
  buildUi();
  updateNextActionStatus();
  auto* delAct = new QAction(QStringLiteral("선택 도형 삭제"), this);
  delAct->setShortcut(QKeySequence::Delete);
  delAct->setShortcutContext(Qt::WindowShortcut);
  connect(delAct, &QAction::triggered, this, [this]() {
    if (routeEditKeyToActiveStudio(true)) return;
    QWidget* focus = QApplication::focusWidget();
    if (m_alignPointList && focus && (focus == m_alignPointList || m_alignPointList->isAncestorOf(focus)))
      deleteSelectedAlignPoint();
    else if (m_layerTree && focus && (focus == m_layerTree || m_layerTree->isAncestorOf(focus)))
      removeSelectedLayers();
    else
      deleteFeaturesOrSelectedReferenceLayers();
  });
  addAction(delAct);
  auto* fullAct = new QAction(QStringLiteral("전체 화면"), this);
  fullAct->setShortcut(Qt::Key_F11);
  fullAct->setShortcutContext(Qt::WindowShortcut);
  connect(fullAct, &QAction::triggered, this, [this]() {
    if (isFullScreen())
      showMaximized();
    else
      showFullScreen();
  });
  addAction(fullAct);
  {
    QSettings st = RecentSurveys::userSettings();
    const QByteArray geo = st.value(QStringLiteral("MainWindow/geometry")).toByteArray();
    if (!geo.isEmpty()) restoreGeometry(geo); else setWindowState(Qt::WindowMaximized);  // first start: full window
    const QByteArray dockState = st.value(QStringLiteral("MainWindow/state")).toByteArray();
    if (!dockState.isEmpty())
      restoreState(dockState);
    // restoreState brings back whether the drawing-tools row was showing when the window was
    // last closed. That row belongs to an active tool, so it must not reappear as an empty strip.
    if (m_subToolbar) m_subToolbar->hide();
    const QByteArray split = st.value(QStringLiteral("MainWindow/mainSplit")).toByteArray();
    // A layout saved with another pane count (2 before the inspector) is left alone: the ratio default sizes all three.
    const int savedPanes = st.value(QStringLiteral("MainWindow/mainSplitPanes"), 2).toInt();
    if (m_mainSplit && !split.isEmpty() && savedPanes == m_mainSplit->count()) {
      m_mainSplit->restoreState(split);
      KaMapPagePlate::applyHandle(m_mainSplit);  // the saved state held the width of an older build
      if (m_shellFocus) m_shellFocus->markUserWidthRestored();
      const int totalW = m_mainSplit->width();
      if (totalW > 300) {
        const QList<int> sz = m_mainSplit->sizes();
        if (!sz.isEmpty() && sz.at(0) > totalW * 0.35) {
          const int leftW = qBound(160, int(totalW * 0.22), 360);
          m_mainSplit->setSizes(KaShellFocus::sizesWithLeft(m_mainSplit, leftW));
        }
      }
    }
    const QByteArray leftState = st.value(QStringLiteral("MainWindow/leftSplit")).toByteArray();
    if (m_leftSplit && !leftState.isEmpty())
      m_leftSplit->restoreState(leftState);
    // 처음 켜면 파일함이 보인다. 그다음부터는 사용자가 파일함 단추로 고른 상태를 따른다.
    if (auto* filesToggle = findChild<QToolButton*>(QStringLiteral("sidebarFilesToggle")))
      filesToggle->setChecked(st.value(QStringLiteral("MainWindow/filesOpen"), true).toBool());
    if (m_leftSplit) {
      QTimer::singleShot(0, this, [this]() {
        KaLayerInformationView::protectSidebarList(
            m_leftSplit, m_layerTree, findChild<QToolButton*>(QStringLiteral("sidebarFilesToggle")),
            findChild<QWidget*>(QStringLiteral("sidebarFilesScroll")),
            findChild<KaLayerInformationPanel*>(QStringLiteral("layerInformationPanel")));
      });
    }
  }
}

MainWindow::~MainWindow() {
  delete m_topographicBrowser.data();
  delete m_topographicImport.data();
  if (m_referenceDownload) m_referenceDownload->cancel();
  // QgsMapToolIdentify must release its canvas-dependent state before the
  // central widget destroys the map canvas.
  delete m_attributeTool;
  m_attributeTool = nullptr;
}

void MainWindow::closeEvent(QCloseEvent* event) {
  if (m_isOpeningSurvey) {
    statusBar()->showMessage(QStringLiteral("조사를 열거나 저장하는 중입니다. 작업이 끝난 뒤 닫아 주세요."), 5000);
    event->ignore();
    return;
  }
  // 예전에는 여기서 말없이 저장했다. 이제 저장은 사용자가 시키는 것이므로,
  // 저장 안 한 작업이 있으면 묻고 답에 따른다. 묻지 않고 버리면 조용히 잃는다.
  if (surveyHasUnsavedChanges()) {
    const auto answer = KaClosePrompt::ask(this, m_surveyPath, EditBufferSummary::summarize(QgsProject::instance()).features);
    if (answer == QMessageBox::Cancel) {
      event->ignore();
      return;
    }
    if (answer == QMessageBox::Save && !persistSurveyWork()) {
      event->ignore();
      return;
    }
  }
  m_closingWindow = true;
  // Cancel catalog jobs before app.exec() can wait on their event-loop locks.
  delete m_topographicImport.data();
  delete m_topographicBrowser.data();
  if (m_referenceDownload) m_referenceDownload->cancel();
  m_locator->cancel();
  if (m_searchProgress) { m_searchProgress->hide(); m_searchProgress->deleteLater(); m_searchProgress = nullptr; }
  m_locationSearchBusy = false;
  // 지도 넓게 보기·접은 왼쪽 패널은 이번 세션에만 쓴다. 다음 실행에 리본이 숨은 채 뜨지 않게
  // 창 상태를 저장하기 전에 되돌린다.
  if (m_shellFocus) m_shellFocus->restoreAll();
  QSettings st = RecentSurveys::userSettings();
  RecentSurveys::setSkipAutoRestore(st, false);
  st.setValue(QStringLiteral("MainWindow/geometry"), saveGeometry());
  st.setValue(QStringLiteral("MainWindow/state"), saveState());
  if (m_mainSplit) {
    st.setValue(QStringLiteral("MainWindow/mainSplit"), m_mainSplit->saveState());
    st.setValue(QStringLiteral("MainWindow/mainSplitPanes"), m_mainSplit->count());  // restored only into this shape
  }
  if (m_leftSplit)
    st.setValue(QStringLiteral("MainWindow/leftSplit"), m_leftSplit->saveState());
  QMainWindow::closeEvent(event);
}

// 작업공간을 읽은 뒤 화면·범례·창 제목을 한 번에 맞춘다. 내장(.gpkg)과 동반(.qgz)
// 두 경로가 같은 마무리를 쓰도록 한곳에 모았다.

QStringList MainWindow::mapOnlyRibbonGroups() {
  return {QStringLiteral("record"), QStringLiteral("fetch"), QStringLiteral("basemap"),
          QStringLiteral("align")};
}

// 도면 같은 작업 탭에서 지도 전용 단추를 눌렀을 때만 지도 탭으로 넘긴다.
// 홈에서는 넘기지 않는다: 조사를 열기 전에 지도 화면을 여는 것은 사용자가 고른다.
void MainWindow::showMapTabFromStudio() {
  if (!m_viewTabs || !m_mapPage) return;
  QWidget* page = m_viewTabs->currentWidget();
  if (page == m_mapPage || page == m_startPage) return;
  m_viewTabs->setCurrentWidget(m_mapPage);
}

void MainWindow::updateNextActionStatus() {
  QString msg;
  if (m_surveyPath.isEmpty()) {
    msg = QStringLiteral("먼저 「새 조사」로 오늘 현장을 만드세요.");
  } else {
#if KA_HGIS_HAS_QGIS
    const bool hasBg = LayerOps::hasVisibleReferenceLayer(QgsProject::instance());
    const bool hasDraw = domainLayerCount() > 0;
#else
    const bool hasBg = false;
    const bool hasDraw = (m_stubSurveyArea + m_stubFeatures) > 0;
#endif
    if (!hasBg)
      msg = QStringLiteral("위성·지적이 없습니다. 더보기 → API 키 입력에서 VWorld 키를 확인하세요.");
    else if (!hasDraw)
      msg = QStringLiteral("「그리기」로 구역을 그리세요.");
    else
      msg = QStringLiteral("다 그렸으면 리본의 「도면」으로 종이에 옮기세요.");
  }
  statusBar()->showMessage(msg);
}

void MainWindow::notify(Notice level, const QString& title, const QString& text,
                        const QString& details) {
#if KA_HGIS_HAS_QGIS
  if (m_messageBar) {
    Qgis::MessageLevel lv = Qgis::MessageLevel::Info;
    switch (level) {
      case Notice::Success:
        lv = Qgis::MessageLevel::Success;
        break;
      case Notice::Warning:
        lv = Qgis::MessageLevel::Warning;
        break;
      case Notice::Critical:
        lv = Qgis::MessageLevel::Critical;
        break;
      case Notice::Info:
        break;
    }
    if (details.isEmpty())
      m_messageBar->pushMessage(title, text, lv);
    else
      m_messageBar->pushMessage(title, text, details, lv);
    return;
  }
#endif
  const QString body = details.isEmpty() ? text : text + QLatin1Char('\n') + details;
  if (level == Notice::Warning || level == Notice::Critical)
    QMessageBox::warning(this, title, body);
  else
    QMessageBox::information(this, title, body);
}

#if KA_HGIS_HAS_QGIS
// 위성·배경 타일이 일부 실패해 반만 그려진 채 캐시에 굳는 문제의 자동 복구.
// 실패가 감지된 타일 레이어만 1.8초 뒤 다시 그린다(같은 화면에서 레이어당 최대 3회).
void MainWindow::healTileLayer(QgsRasterLayer* layer) {
  if (!layer || layer->providerType() != QLatin1String("wms")) return;
  const QString id = layer->id();
  if (m_tileHealCount.value(id) >= 3 || m_tileHealPending.contains(id)) return;
  const int attempt = ++m_tileHealCount[id];
  m_tileHealPending.insert(id);
  QPointer<QgsRasterLayer> guard(layer);
  QTimer::singleShot(1800, this, [this, guard, id, attempt]() {
    m_tileHealPending.remove(id);
    if (!guard) return;
    if (m_canvas && m_canvas->isDrawing()) return;
    KaCrashGuard::logLine(QStringLiteral("[render] '%1' 빠진 타일 자동 재시도 %2/3")
                              .arg(guard->name())
                              .arg(attempt));
    guard->triggerRepaint();
  });
}

void MainWindow::notifyBasemapFailure(bool timedOut, const QString& reason) {
  if (m_closingWindow) return;
  // 서버가 답을 준 정상 응답(범례 미지원·국토 밖 타일)은 실패가 아니다.
  if (LayerOps::mapServerMessageIsBenign(reason)) return;
  // 원인은 매번 로그에 남기고, 화면 안내는 세션에 한 번만 띄운다. URL에는 인증키가
  // 들어 있으므로 괄호 앞까지만 남긴다.
  KaCrashGuard::logLine(QStringLiteral("[basemap] 배경지도 실패 — %1")
                            .arg(reason.section(QLatin1Char('('), 0, 0).trimmed()));
  if (m_basemapNoticeShown) return;
  m_basemapNoticeShown = true;
  // 렌더러와 네트워크 콜백 안에서는 UI를 바꾸지 않는다.
  QTimer::singleShot(0, this, [this, timedOut] {
    if (m_closingWindow) return;
    notify(Notice::Warning, QStringLiteral("배경지도를 불러오지 못했습니다"),
           (timedOut ? QStringLiteral("지도 서버의 응답이 늦어 요청을 중단했습니다. ")
                     : QStringLiteral("지도 서버에 연결하지 못했거나 서버가 요청을 거절했습니다. ")) +
               QStringLiteral("인터넷 연결과 배경지도 설정을 확인한 뒤 다시 켜 주세요. "
                              "조사 도형을 그리거나 저장하는 작업은 계속할 수 있습니다. "
                              "같은 안내는 다시 띄우지 않습니다."));
  });
}
#endif

void MainWindow::buildUi() {
  auto* central = new QWidget(this);
  central->setObjectName(QStringLiteral("centralRoot"));
  central->setAttribute(Qt::WA_StyledBackground, true);
  auto* root = new QHBoxLayout(central);
  root->setContentsMargins(8, 8, 8, 8);
  root->setSpacing(8);

#if KA_HGIS_HAS_QGIS
  m_canvas = new QgsMapCanvas(central);
  connect(m_canvas, &QgsMapCanvas::messageEmitted, this,
          [this](const QString& title, const QString& message, Qgis::MessageLevel) {
    statusBar()->showMessage(title + QStringLiteral(": ") + message, 8000);
  });
  // 도형선택에서 Ctrl+C · Ctrl+V 로 지적 필지를 같은 자리에 복사한다. 결과는 상태 줄에 알린다.
  auto* parcelKeys = new KaParcelCopyKeys(m_canvas);
  connect(parcelKeys, &KaParcelCopyKeys::statusMessage, this,
          [this](const QString& text) { statusBar()->showMessage(text, 6000); });
  m_canvas->setObjectName(QStringLiteral("mapCanvas"));
  LayerOps::applyWheelZoomFactor(m_canvas);
  KaTheme::excludeMapSurface(m_canvas);
  m_canvas->setCanvasColor(KaTheme::tokens().canvasNeutral);
  m_canvas->enableAntiAliasing(true);
  m_canvas->setCachingEnabled(true);
  // 병렬 렌더는 꺼 둔다. ParallelJob이 provider_wms의 중첩 이벤트 루프와 겹치면
  // deleteLater가 ACCESS_VIOLATION을 낸다(현장 덤프 2026-08-31). KaApplication의
  // qgis/parallel_rendering=false와 같은 값이어야 서로 싸우지 않는다.
  m_canvas->setParallelRenderingEnabled(false);
  // provider_wms may enter another event loop from partial tile output while
  // the previous reply is still active. Keep the completed image until the next
  // render completes, avoiding that reentrant response-lifetime path.
  m_canvas->setMapSettingsFlags(m_canvas->mapSettings().flags() &
                               ~Qgis::MapSettingsFlags(Qgis::MapSettingsFlag::RenderPartialOutput));
  // 미리보기 작업은 켠다 — 이게 꺼져 있으면 화면을 끄는 동안 캔버스가 비어
  // 흰 화면이 보인다. 스레드 풀이 2개 이상이라야 실제로 겹쳐서 돈다.
  m_canvas->setPreviewJobsEnabled(true);
  // Completed renders and the existing cache still refresh normally.
  m_canvas->setMapUpdateInterval(80);
  m_canvas->setAcceptDrops(true);
  m_canvas->setSegmentationTolerance(2.0);
  const QgsCoordinateReferenceSystem crs(m_workCrs);
  m_canvas->setDestinationCrs(crs);
  QgsProject::instance()->setCrs(crs);
  m_panTool = new QgsMapToolPan(m_canvas);
  m_canvas->setMapTool(m_panTool);
  m_mapGrid = new KaCanvasGridOverlay(m_canvas);
  m_mapGrid->setEnabled(false);
  // 지도를 다 그린 뒤 위 레이어를 라벨 위에 한 번 더 그린다.
  m_aboveLabels = new KaAboveLabelsOverlay(m_canvas);
  // 팬·줌에서 지도가 하얗게 비는 원인을 좁히기 위한 계측.
  // KA_HGIS_TRACE_RENDER=1 로 켠다. 렌더가 몇 번 시작·취소되는지, 레이어 목록이
  // 팬 도중에 갈리는지가 로그에 남는다.
  if (!qgetenv("KA_HGIS_TRACE_RENDER").isEmpty()) {
    auto* seq = new int(0);
    connect(m_canvas, &QgsMapCanvas::extentsChanged, this, [this, seq]() {
      const QgsRectangle e = m_canvas->extent();
      KaCrashGuard::logLine(
          QStringLiteral("[trace %1] extentsChanged scale=%2 draw=%3 size=%4x%5 dpr=%6 dpi=%7")
              .arg(++*seq)
              .arg(m_canvas->scale(), 0, 'f', 0)
              .arg(m_canvas->isDrawing() ? 1 : 0)
              .arg(m_canvas->width())
              .arg(m_canvas->height())
              .arg(m_canvas->mapSettings().devicePixelRatio(), 0, 'f', 2)
              .arg(m_canvas->mapSettings().outputDpi(), 0, 'f', 1));
    });
    connect(m_canvas, &QgsMapCanvas::renderStarting, this, [this, seq]() {
      KaCrashGuard::logLine(QStringLiteral("[trace %1] renderStarting").arg(++*seq));
    });
    connect(m_canvas, &QgsMapCanvas::mapCanvasRefreshed, this, [this, seq]() {
      KaCrashGuard::logLine(QStringLiteral("[trace %1] refreshed").arg(++*seq));
    });
    connect(m_canvas, &QgsMapCanvas::layersChanged, this, [this, seq]() {
      KaCrashGuard::logLine(QStringLiteral("[trace %1] layersChanged  n=%2")
                                .arg(++*seq)
                                .arg(m_canvas->layers().size()));
    });
  }
  // 타일 일부 실패(요청 제한·순간 네트워크 오류)로 위성지도가 반만 보이면
  // 해당 레이어만 자동으로 다시 그린다. 화면을 움직이면 재시도 카운터 초기화.
  connect(m_canvas, &QgsMapCanvas::renderErrorOccurred, this,
          [this](const QString& err, QgsMapLayer* layer) {
            auto* raster = qobject_cast<QgsRasterLayer*>(layer);
            if (raster && raster->providerType() == QLatin1String("wms")) {
              notifyBasemapFailure(err.contains(QLatin1String("timeout"), Qt::CaseInsensitive), err);
              healTileLayer(raster);
            } else {
              const QString name = layer ? layer->name() : QStringLiteral("선택한 지도");
              QTimer::singleShot(0, this, [this, name] {
                if (!m_closingWindow)
                  notify(Notice::Warning, QStringLiteral("지도를 표시하지 못했습니다"),
                         QStringLiteral("%1의 원본 파일과 좌표계를 확인한 뒤 다시 켜 주세요.").arg(name));
              });
            }
          });
  connect(QgsApplication::messageLog(),
          &QgsMessageLog::messageReceivedWithFormat,
          this, [this](const QString& message, const QString& tag, Qgis::MessageLevel level,
                       Qgis::StringFormat) {
            if (level != Qgis::MessageLevel::Warning && level != Qgis::MessageLevel::Critical)
              return;
            const bool tileIssue = tag.contains(QLatin1String("WMS"), Qt::CaseInsensitive) ||
                                   message.contains(QLatin1String("tile"), Qt::CaseInsensitive) ||
                                   message.contains(QStringLiteral("타일"));
            if (!tileIssue || !m_canvas) return;
            notifyBasemapFailure(message.contains(QLatin1String("timeout"), Qt::CaseInsensitive),
                                 message);
          });
  connect(QgsNetworkAccessManager::instance(),
          qOverload<QgsNetworkRequestParameters>(&QgsNetworkAccessManager::requestTimedOut),
          this, [this](const QgsNetworkRequestParameters& request) {
            const QString host = request.request().url().host();
            if (host.endsWith(QLatin1String("vworld.kr"), Qt::CaseInsensitive) ||
                host.endsWith(QLatin1String("kigam.re.kr"), Qt::CaseInsensitive))
              notifyBasemapFailure(true, QStringLiteral("요청 시간 초과 · %1").arg(host));
          });
  connect(m_canvas, &QgsMapCanvas::extentsChanged, this, [this]() {
    m_tileHealCount.clear();
    LayerOps::applyCanvasScreenDpi(m_canvas);
    if (m_mapGrid && m_mapGrid->isEnabled()) {
      m_mapGrid->updatePosition();
      m_mapGrid->update();
    }
  });
  connect(m_canvas, &QgsMapCanvas::scaleChanged, this, [this](double) {
    if (m_mapGrid && m_mapGrid->isEnabled()) {
      m_mapGrid->updatePosition();
      m_mapGrid->update();
    }
  });
  connect(m_canvas, &QgsMapCanvas::mapToolSet, this, [this](QgsMapTool* tool, QgsMapTool*) {
    if (m_actMeasure)
      m_actMeasure->setChecked(m_measureTool && tool == m_measureTool);
  });

  auto* layerTreeRoot = QgsProject::instance()->layerTreeRoot();
  auto* model = new KaLayerInformationModel(QgsProject::instance(), false, this);
  model->setFlag(QgsLayerTreeModel::AllowNodeReorder, true);
  model->setFlag(QgsLayerTreeModel::AllowNodeChangeVisibility, true);
  model->setFlag(QgsLayerTreeModel::AllowNodeRename, true);
  // 범례 소분류(지층·분류 항목)는 만들 때 접어 둔다. 누르면 펼친다.
  model->setAutoCollapseLegendNodes(1);
  m_layerTree = new KaLayerInformationView(central);
  m_layerTree->setObjectName(QStringLiteral("layerTree"));
  m_layerTree->setModel(model);
  KaLayerInformationModel::configureView(m_layerTree);
  // [pkg E1] F186: Ctrl+Z also puts back the user's drag order and check marks.
  m_layerTreeUndo = std::make_unique<KaLayerTreeUndo>(QgsProject::instance(), m_layerTree,
      [this](std::shared_ptr<KaLayerTreeSnapshot> before) {
        if (m_isOpeningSurvey || m_closingWindow) return;
        KaUndoAction action;
        action.type = KaUndoAction::LayerTreeChanged;
        action.treeBefore = std::move(before);
        action.description = QStringLiteral("레이어 순서·표시");
        pushUndoAction(action);
        updateUndoRedoActions();
      });
  model->setScale(m_canvas->scale());
  connect(m_canvas, &QgsMapCanvas::scaleChanged, model, &KaLayerInformationModel::setScale);
  connect(model, &KaLayerInformationModel::labelsEdited, this, [this] {
    applyLabelStackOrder();
    LayerOps::refreshCanvasIfIdle(m_canvas);
    if (m_drawingStudio) m_drawingStudio->refreshMapFromProject();
  });
  m_layerTree->setFocusPolicy(Qt::StrongFocus);
  m_layerTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_layerTree->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
  m_layerTree->setDragEnabled(true);
  m_layerTree->setAcceptDrops(true);
  m_layerTree->setDropIndicatorShown(true);
  m_layerTree->setDefaultDropAction(Qt::MoveAction);
  m_layerTree->setDragDropMode(QAbstractItemView::DragDrop);
  connect(model, &QAbstractItemModel::rowsMoved, this, [this](const QModelIndex&, int, int, const QModelIndex&, int) {
    onLayerTreeRowsMoved();
  });
  m_layerTree->installEventFilter(this);
  if (m_layerTree->viewport())
    m_layerTree->viewport()->installEventFilter(this);
  m_layerTree->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_layerTree, &QWidget::customContextMenuRequested, this, &MainWindow::onLayerTreeContextMenu);
  connect(m_layerTree, &QTreeView::doubleClicked, this, [this](const QModelIndex& index) {
    if (index.column() == 0) onLayerTreeDoubleClicked(index);
  });
  // 레이어 창 ↔ 지도 연결. 덧그림이 그리는 레이어는 연결이 지도 목록에 다시 넣을 때마다 뺀다(R83).
  m_bridge = KaAboveLabelsOverlay::makeLayerTreeBridge(layerTreeRoot, m_canvas, this);

  m_canvas->setContextMenuPolicy(Qt::DefaultContextMenu);
  m_canvas->installEventFilter(this);
  if (m_canvas->viewport())
    m_canvas->viewport()->installEventFilter(this);
  connect(m_canvas, &QWidget::customContextMenuRequested, this, &MainWindow::onMapContextMenu);
  connect(m_canvas, &QgsMapCanvas::contextMenuAboutToShow, this,
          [this](QMenu* menu, QgsMapMouseEvent* event) {
            if (event) populateMapContextMenu(menu, event->pos());
          });
  connect(m_canvas, &QgsMapCanvas::zoomLastStatusChanged, this,
          [this](bool available) { m_canZoomPrevious = available; });
  connect(m_canvas, &QgsMapCanvas::zoomNextStatusChanged, this,
          [this](bool available) { m_canZoomNext = available; });
  connect(m_canvas, &QgsMapCanvas::scaleChanged, this, &MainWindow::onCanvasScaleChanged);
  connect(m_canvas, &QgsMapCanvas::xyCoordinates, this, [this](const QgsPointXY& p) {
    if (m_status) m_status->setCoordinate(p.x(), p.y());
  });
  m_status->setWorkCrs(m_workCrs);
  // 새 조사와 작업공간 복원도 실제 지도 좌표계 변경을 통해 표시를 갱신한다.
  connect(m_canvas, &QgsMapCanvas::destinationCrsChanged, this, [this]() {
    const auto crs = m_canvas->mapSettings().destinationCrs();
    if (crs.isValid()) m_status->setWorkCrs(crs.authid());
  });
  connect(m_status, &KaStatusBar::crsClicked, this, [this]() {
    QMenu menu(this);
    QAction* a86 = menu.addAction(QStringLiteral("중부원점 (EPSG:5186)"));
    QAction* a87 = menu.addAction(QStringLiteral("동부원점 (EPSG:5187)"));
    a86->setCheckable(true);
    a87->setCheckable(true);
    a86->setChecked(m_workCrs == QLatin1String("EPSG:5186"));
    a87->setChecked(m_workCrs == QLatin1String("EPSG:5187"));
    menu.addSeparator();
    QAction* note = menu.addAction(QStringLiteral("제출용 파일은 항상 EPSG:5179로 나갑니다"));
    note->setEnabled(false);
    const QAction* picked = menu.exec(QCursor::pos());
    if (picked == a86)
      setWorkCrs5186();
    else if (picked == a87)
      setWorkCrs5187();
  });
  connect(m_status, &KaStatusBar::renderingToggled, this, [this](bool on) {
    if (!m_canvas) return;
    m_canvas->setRenderFlag(on);
    if (on) m_canvas->refresh();
  });
  connect(m_canvas, &QgsMapCanvas::mapToolSet, this, [this](QgsMapTool* newTool, QgsMapTool*) {
    if (m_actSelect)
      m_actSelect->setChecked(newTool && m_featureSelectTool && newTool == m_featureSelectTool);
    if (m_actMeasure)
      m_actMeasure->setChecked(newTool && m_measureTool && newTool == m_measureTool);
    if (m_btnDraw) {
      const bool drawing = newTool && m_captureTool && newTool == m_captureTool;
      const bool subOpen = m_subToolbar && m_subToolbar->isVisible() &&
                           m_subToolsMode == QLatin1String("draw");
      m_btnDraw->setChecked(drawing || subOpen);
    }
  });
  connect(m_canvas, &QgsMapCanvas::extentsChanged, this, [this]() {
    if (m_subToolsMode == QLatin1String("align"))
      updateAlignOverlay();
    if (m_extentClampGuard || !m_canvas) return;
    // 예전에는 그리는 중이면 여기서 그냥 돌아갔다. 그런데 줌아웃은 렌더를 띄우므로
    // extentsChanged 가 올 때 캔버스는 대개 그리는 중이고, 그래서 클램프가 거의
    // 매번 건너뛰어졌다. 한국 밖(1:800만 등)까지 줌아웃되면 VWorld 타일이 없어
    // 위성이 통째로 사라진다. clampCanvasToKorea 가 알아서 끝난 뒤로 미룬다.
    m_extentClampGuard = true;
    LayerOps::clampCanvasToKorea(m_canvas);
    m_extentClampGuard = false;
  });
  connect(m_canvas, &QgsMapCanvas::scaleChanged, this, [this](double) {
    if (m_subToolsMode == QLatin1String("align"))
      updateAlignOverlay();
  });
  // 도구를 바꾸면 파란 밑줄도 그 자리로 옮겨간다.
  connect(m_canvas, &QgsMapCanvas::mapToolSet, this,
          [this](QgsMapTool*, QgsMapTool*) { updateSubToolbarChecks(); });
  connect(m_canvas, &QgsMapCanvas::renderComplete, this, [this](QPainter*) {
    if (m_subToolsMode == QLatin1String("align"))
      updateAlignOverlay();
    logCanvasPaintState();
  });
  // [pkg E1] F131: many layers added/removed in a row cost one map sync.
  m_layerRefreshBatch = std::make_unique<KaLayerRefreshBatch>(this, [this]() {
    refreshMapCanvasNow();
    syncThematicButtons();
  });
  connect(QgsProject::instance(), &QgsProject::layersAdded, this, [this](const QList<QgsMapLayer*>& layers) {
    for (auto* layer : layers) {
      if (auto* vector = qobject_cast<QgsVectorLayer*>(layer)) watchUndoFeatureIds(vector);
    }
    if (m_isOpeningSurvey) return;
    // AdvancedConfiguration 개별 설정은 추가 당시 레이어만 가진다. 새 조사 레이어를 다시 넣는다.
    applySnapConfig();
    LayerOps::restoreThematicOverlayVisibility(QgsProject::instance());
    LayerOps::ensureSatelliteAtBottom(QgsProject::instance());
    m_layerRefreshBatch->request();
  });
  connect(QgsProject::instance(), &QgsProject::layersRemoved, this, [this](const QStringList&) {
    if (m_isOpeningSurvey) return;
    LayerOps::ensureSatelliteAtBottom(QgsProject::instance());
    m_layerRefreshBatch->request();
  });
  // 레이어의 체크를 끄고 켜는 것도 아이콘에 그대로 따라와야 한다.
  if (QgsLayerTree* legendRoot = QgsProject::instance()->layerTreeRoot()) {
    connect(legendRoot, &QgsLayerTreeNode::visibilityChanged, this,
            [this](QgsLayerTreeNode*) { syncThematicButtons(); });
  }
  LayerOps::applyKoreaMapLimits(QgsProject::instance(), m_canvas);

  // 1. 레이어 패널 (m_layersCard) - 상단 배치
  auto* layersCard = new QFrame(central);
  layersCard->setObjectName(QStringLiteral("layersCard"));
  m_layersCard = layersCard;
  // Flat Strata chrome: cards are set apart by their 1px QSS border and the layout spacing.
  // A drop-shadow effect would also force the subtree through an offscreen pixmap.
  auto* layersLay = new QVBoxLayout(layersCard);
  layersLay->setContentsMargins(6, 6, 6, 6);
  layersLay->setSpacing(6);

  auto* capLayers = new QLabel(QStringLiteral("레이어"), layersCard);
  capLayers->setObjectName(QStringLiteral("cardCaption"));

  auto* layersInner = new QFrame(layersCard);
  layersInner->setObjectName(QStringLiteral("layersInner"));
  auto* layersInnerLay = new QVBoxLayout(layersInner);
  layersInnerLay->setContentsMargins(4, 4, 4, 4);
  layersInnerLay->addWidget(m_layerTree, 1);
  layersInnerLay->insertWidget(0, new KaLayerListChrome(m_layerTree, layersInner));  // [P6] 레이어 찾기 · 편집 중 ✎
  layersInnerLay->addWidget(new KaLayerInformationPanel(model, m_layerTree, layersInner));
  m_layerEmpty = new QLabel(
      QStringLiteral("레이어가 없습니다.\n파일함에서 SHP·DXF·DWG를 끌어 넣거나\n위성·지적 배경을 올리세요."),
      layersInner);
  m_layerEmpty->setObjectName(QStringLiteral("emptyState"));
  m_layerEmpty->setAlignment(Qt::AlignCenter);
  m_layerEmpty->setWordWrap(true);
  layersInnerLay->addWidget(m_layerEmpty, 1);
#if KA_HGIS_HAS_QGIS
  connect(QgsProject::instance(), &QgsProject::layersAdded, this, [this](const QList<QgsMapLayer*>&) {
    refreshLayerEmptyState();
  });
  connect(QgsProject::instance(), &QgsProject::layersRemoved, this, [this](const QStringList&) {
    refreshLayerEmptyState();
  });
#endif
  refreshLayerEmptyState();

  // 도면/글자 체크와 선택 도면 설정은 목록에 둔다. 가져오기/순서/삭제는
  // 기존 파일함·드래그·Delete 흐름을 유지하며 전체 도면 체크는 제목 옆에 둔다.
  auto* capRow = new QHBoxLayout();
  capRow->setContentsMargins(0, 0, 0, 0);
  capRow->setSpacing(6);
  capRow->addWidget(capLayers);
  capRow->addStretch(1);
  auto* filesToggle = new QToolButton(layersCard);
  filesToggle->setObjectName(QStringLiteral("sidebarFilesToggle"));
  filesToggle->setText(QStringLiteral("파일함"));
  filesToggle->setCheckable(true);
  filesToggle->setChecked(true);
  capRow->addWidget(filesToggle);
  m_layerCheckAllBtn = new QToolButton(layersCard);
  m_layerCheckAllBtn->setObjectName(QStringLiteral("layerCheckAllBtn"));
  m_layerCheckAllBtn->setFocusPolicy(Qt::NoFocus);
  m_layerCheckAllBtn->setToolTip(QStringLiteral(
      "레이어 체크를 한 번에 모두 끄거나 켭니다. 하나라도 켜져 있으면 전부 끕니다."));
  connect(m_layerCheckAllBtn, &QToolButton::clicked, this, &MainWindow::toggleAllLayersChecked);
  capRow->addWidget(m_layerCheckAllBtn);
  for (QToolButton* word : {filesToggle, m_layerCheckAllBtn}) KaTheme::followPaperLook(word);  // 새 모양: 글자 단추
  layersLay->addLayout(capRow);
#if KA_HGIS_HAS_QGIS
  // 체크를 하나씩 손으로 바꿔도 단추 글씨가 따라가야 한다.
  // visibilityChanged 는 트리 안 어느 노드가 바뀌어도 뿌리까지 올라온다.
  if (QgsLayerTree* visibilityRoot = QgsProject::instance()->layerTreeRoot()) {
    connect(visibilityRoot, &QgsLayerTreeNode::visibilityChanged, this,
            [this](QgsLayerTreeNode* node) {
              LayerOps::revealCheckedLegendNode(node);
              refreshLayerCheckAllButton();
              // 체크 하나마다 전 레이어 setLabeling 을 하면 유적 글자가
              // 보였다가 사라진다. 한 틱에 한 번만 다시 쌓는다.
              if (m_labelOrderQueued) return;
              m_labelOrderQueued = true;
              QTimer::singleShot(0, this, [this]() {
                m_labelOrderQueued = false;
                applyLabelStackOrder();
              });
            });
  }
  connect(QgsProject::instance(), &QgsProject::layersAdded, this,
          [this](const QList<QgsMapLayer*>&) { refreshLayerCheckAllButton(); });
  connect(QgsProject::instance(), &QgsProject::layersRemoved, this,
          [this](const QStringList&) { refreshLayerCheckAllButton(); });
  // 레이어가 밑에 있으면 글자도 밑으로. 순서가 바뀔 때마다 다시 건다.
  // 끌어서 순서를 바꾸면 트리에서 노드가 빠졌다 들어오므로 두 신호를 다 듣는다.
  if (QgsLayerTree* orderRoot = QgsProject::instance()->layerTreeRoot()) {
    connect(orderRoot, &QgsLayerTreeNode::addedChildren, this,
            [this](QgsLayerTreeNode*, int, int) { applyLabelStackOrder(); });
    connect(orderRoot, &QgsLayerTreeNode::removedChildren, this,
            [this](QgsLayerTreeNode*, int, int) { applyLabelStackOrder(); });
  }
  connect(QgsProject::instance(), &QgsProject::layersAdded, this,
          [this](const QList<QgsMapLayer*>&) { applyLabelStackOrder(); });
#endif
  refreshLayerCheckAllButton();
  layersLay->addWidget(layersInner, 1);

#if KA_HGIS_HAS_QGIS
  connect(m_layerTree, &QgsLayerTreeView::currentLayerChanged, this, &MainWindow::updateLayerOpacityControl);
  if (m_layerTree->selectionModel()) {
    connect(m_layerTree->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this](const QItemSelection&, const QItemSelection&) {
              updateLayerOpacityControl();
            });
  }
  connect(QgsProject::instance(), &QgsProject::layersAdded, this, [this](const QList<QgsMapLayer*>&) {
    updateLayerOpacityControl();
  });
  connect(QgsProject::instance(), &QgsProject::layersRemoved, this, [this](const QStringList&) {
    updateLayerOpacityControl();
  });
#endif

  // 2. 파일함 패널 (m_filesPanel) - 하단 배치
  auto* filesPanel = new KaFileBrowserPanel(central);
  m_filesPanel = filesPanel;
  m_filesCard = filesPanel;
  m_fileBrowser = filesPanel->listView();
  connect(filesPanel, &KaFileBrowserPanel::fileActivated, this, [this](const QString& path) {
    if (GeorefService::isCadPath(path)) { addVectorFromPath(path); return; }  // 도면은 실패를 그 흐름이 알린다
    const bool raster = GeorefService::isImagePath(path);
    if (raster ? !addRasterFromPath(path) : !addVectorFromPath(path)) {
      KaUserError::warn(this, {
          QStringLiteral("파일"),
          QStringLiteral("선택한 파일을 지도 레이어로 열지 못했습니다."),
          QStringLiteral("SHP/DXF/DWG/GPKG/GeoTIFF/JPG만 지도에 올릴 수 있습니다.\n%1")
              .arg(QDir::toNativeSeparators(path)),
          QStringLiteral("지원 형식인지 확인한 뒤 다시 열어 주세요. DWG는 DXF로 저장해 보세요."),
      });
    }
  });
  connect(filesPanel, &KaFileBrowserPanel::statusMessage, this, [this](const QString& msg) {
    statusBar()->showMessage(msg, 6000);
  });

  // 3. 좌측 패널 수직 분할: 레이어(위) + 파일함(아래) 동시 노출
  auto* leftSplit = new QSplitter(Qt::Vertical, central);
  leftSplit->setObjectName(QStringLiteral("leftSplit"));
  m_leftSplit = leftSplit;
  KaMapPagePlate::followHandle(leftSplit, 8);  // 새 모양: 7 px with one hairline
  leftSplit->setChildrenCollapsible(false);
  leftSplit->setCollapsible(1, true);
  leftSplit->installEventFilter(this);
  auto* filesScroll = new QScrollArea(leftSplit);
  filesScroll->setObjectName(QStringLiteral("sidebarFilesScroll"));
  filesScroll->setWidgetResizable(true);
  filesScroll->setFrameShape(QFrame::NoFrame);
  filesScroll->setMinimumHeight(0);
  filesPanel->setMinimumHeight(0);
  filesScroll->setWidget(filesPanel);
  m_filesCard = filesScroll;
  connect(filesToggle, &QToolButton::toggled, filesScroll, [leftSplit, filesScroll](bool on) {
    if (on) KaLayerInformationView::openSidebarFiles(leftSplit, filesScroll);
    else filesScroll->hide();
  });
  // Only a press is the user's choice; a short window shuts the 파일함 without asking.
  connect(filesToggle, &QToolButton::clicked, this, [](bool on) {
    RecentSurveys::userSettings().setValue(QStringLiteral("MainWindow/filesOpen"), on);
  });
  leftSplit->addWidget(layersCard);
  leftSplit->addWidget(filesScroll);
  leftSplit->setStretchFactor(0, 3);
  leftSplit->setStretchFactor(1, 2);
  leftSplit->setSizes({380, 260});
  leftSplit->setMinimumWidth(160);
  leftSplit->setMaximumWidth(720);
  leftSplit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

  auto* mapCard = new QFrame(central);
  mapCard->setObjectName(QStringLiteral("mapCard"));
  auto* mapLay = new QVBoxLayout(mapCard);
  mapLay->setContentsMargins(4, 4, 4, 4);
  mapLay->setSpacing(4);
  m_messageBar = new QgsMessageBar(mapCard);
  m_messageBar->setObjectName(QStringLiteral("mapMessageBar"));
  mapLay->addWidget(m_messageBar, 0);
  mapLay->addWidget(m_canvas, 1);
  {
    QWidget* railHost = m_canvas->viewport() ? m_canvas->viewport() : static_cast<QWidget*>(m_canvas);
    m_layerOpacityRail = new KaLayerOpacityRail(railHost, QStringLiteral("map"));
    // Zoom buttons and a scale bar float on the map like the opacity card.
    auto* controls = new KaMapControls(m_canvas, railHost);
    controls->setFitHandler([this]() {
      for (QgsVectorLayer* area : LayerOps::findAllByLayerKey(QgsProject::instance(), QStringLiteral("survey_area")))
        if (area && area->isValid() && area->featureCount() > 0 && LayerOps::zoomToLayerMax(m_canvas, area))
          return;
      if (m_layerTree && m_layerTree->currentLayer())
        LayerOps::zoomToLayerMax(m_canvas, m_layerTree->currentLayer());
    });
    new KaMapScaleBar(m_canvas, railHost);
    // 좌표격자는 지도 오른쪽 아래에 떠 있는 작은 막대다. 지도 아래 따로 떨어진 띠에
    // 체크 상자 하나만 두면 한 줄을 통째로 차지했다. 세부 칸은 켰을 때만 위로 두 줄 펼쳐
    // 막대가 좁게 남는다. 그래야 왼쪽 아래 축척 막대를 덮지 않는다.
    m_mapGridBar = new KaMapCornerBar(railHost);
  }
  connect(m_layerOpacityRail, &KaLayerOpacityRail::brightnessChanged, this, [this](int value) {
    if (!m_layerTree) return;
    QgsMapLayer* cur = m_layerTree->currentLayer();
    if (!LayerOps::canAdjustBrightness(cur)) return;
    LayerOps::setMapLayerBrightness(cur, value, m_canvas);
    if (m_drawingStudio) {
      m_drawingStudio->updateLayerOpacityControl();
      m_drawingStudio->repaintMapLayers();
    }
  });
  connect(m_layerOpacityRail, &KaLayerOpacityRail::percentChanged, this, [this](int value) {
    if (!m_layerTree) return;
    QgsMapLayer* cur = m_layerTree->currentLayer();
    // [pkg E1] F018: every valid layer can be see-through (display only).
    if (!LayerOps::canAdjustOpacity(cur)) return;
    LayerOps::applyLayerOpacity(cur, value / 100.0, m_canvas);
    // A see-through survey layer leaves the above-labels pass: resync the map list.
    if (!LayerOps::isReferenceOrBasemapLayer(cur)) refreshMapCanvasNow();
    if (m_drawingStudio) {
      m_drawingStudio->updateLayerOpacityControl();
      m_drawingStudio->repaintMapLayers();
    }
  });
  updateLayerOpacityControl();

  m_scaleEdit = m_status->scaleEdit();
  m_scaleCombo = m_status->scaleCombo();
  connect(m_scaleEdit, &QLineEdit::returnPressed, this, &MainWindow::applyMapScaleFromUi);
  // 프리셋 선택과 직접 입력이 완전히 같은 경로를 타야 결과가 같다.
  connect(m_scaleCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int idx) {
    if (!m_scaleCombo || !m_scaleEdit) return;
    const int s = m_scaleCombo->itemData(idx).toInt();
    if (s > 0) {
      m_scaleEdit->setText(QStringLiteral("1:%1").arg(s));
      applyMapScaleFromUi();
    }
  });
  // 좌표격자 칸은 지도 오른쪽 아래 막대 안에 있다. 격자 종류(미터·경위도)는 콤보로
  // 명시해 고르고 Shift 상태를 읽지 않는다. 값을 바꾸면 모아서 한 번만 다시 그린다.
  auto* cornerLay = new QHBoxLayout(m_mapGridBar);
  cornerLay->setContentsMargins(10, 4, 10, 4);
  cornerLay->setSpacing(6);
  m_gridControls = new KaShellGridControls(m_mapGridBar);
  cornerLay->addWidget(m_gridControls);
  connect(m_gridControls, &KaShellGridControls::settingsChanged, this, &MainWindow::applyMapGrid);
  // [pkg F2] F070: excavation-grid origin; the button edits the overlay config directly.
  if (auto* detail = m_gridControls->findChild<QWidget*>(QStringLiteral("gridDetail")))
    if (auto* detailGrid = qobject_cast<QGridLayout*>(detail->layout()))
      detailGrid->addWidget(new KaGridOriginButton(m_canvas, [this] { return m_mapGrid; }, detail), 0, 3);

  // 왼쪽 패널 ↔ 지도 사이를 끌어서 나눌 수 있게 한다. 나눈 폭은 창 상태와
  // 같이 저장돼 다음에 열 때 그대로 온다(MainWindow/mainSplit).
  m_mainSplit = new QSplitter(Qt::Horizontal, central);
  m_mainSplit->setObjectName(QStringLiteral("mainSplit"));
  m_mainSplit->setHandleWidth(10);
  m_mainSplit->setChildrenCollapsible(false);
  m_mainSplit->addWidget(leftSplit);
  m_mainSplit->addWidget(mapCard);
  m_mainSplit->setStretchFactor(0, 0);
  m_mainSplit->setStretchFactor(1, 1);
  m_mainSplit->setSizes({348, 932});
  root->addWidget(m_mainSplit, 1);
  // 작은 노트북에서 지도를 넓히는 방법은 사용자가 켤 때만 쓴다(더보기, Ctrl+F11 · F9).
  // 리본 칸 크기·한 줄 배치는 그대로이고, 지도 넓게 보기는 리본을 잠시 숨길 뿐이다.
  m_shellFocus = new KaShellFocus(m_mainSplit, this);
  if (auto* ribbonBar = findChild<QToolBar*>(QStringLiteral("mainToolbar")))
    m_shellFocus->setChrome({ribbonBar});
  connect(m_shellFocus, &KaShellFocus::changed, this,
          [this](const QString& message) { statusBar()->showMessage(message, 6000); });
#else
  root->addWidget(new QLabel(QStringLiteral("QGIS SDK 스텁 모드"), central), 1);
  setCentralWidget(central);
#endif
#if KA_HGIS_HAS_QGIS
  m_viewTabs = new QTabWidget(this);
  m_viewTabs->setObjectName(QStringLiteral("viewTabs"));
  m_viewTabs->setDocumentMode(true);
  m_viewTabs->setTabsClosable(true);
  m_viewTabs->setMovable(false);
  m_startPage = new KaStartPage(m_viewTabs);
  connect(m_startPage, &KaStartPage::newSurveyRequested, this, &MainWindow::newSurvey);
  connect(m_startPage, &KaStartPage::openRequested, this, &MainWindow::openProject);
  connect(m_startPage, &KaStartPage::recentOpened, this, &MainWindow::openRecentSurvey);
  connect(m_startPage, &KaStartPage::forgetRequested, this, [this](const QString& path) {
    QSettings st = RecentSurveys::userSettings();
    RecentSurveys::forget(st, path);
    if (m_startPage) m_startPage->reload();
  });
  m_mapPage = central;
  // 탭 아이콘은 타일 없는 한 색 그림이다. 16px 타일은 뭉개져 무엇인지 안 보였다.
  const QColor tabInk = KaTheme::tokens().inkMuted;
  const int homeIdx = m_viewTabs->addTab(m_startPage, KaIcons::icon(QStringLiteral("home"), tabInk),
                                         QStringLiteral("홈"));
  const int mapIdx = m_viewTabs->addTab(central, KaIcons::icon(QStringLiteral("map"), tabInk),
                                        QStringLiteral("지도"));
  if (QTabBar* bar = m_viewTabs->tabBar()) {
    bar->setTabButton(homeIdx, QTabBar::RightSide, nullptr);
    bar->setTabButton(mapIdx, QTabBar::RightSide, nullptr);
  }
  setupStrataShell();  // [P6] 인스펙터(유구 카드 포함)·배지·안내 띠·배경 카드·홈 「설정」 (MainWindowChrome.cpp)
  connect(m_viewTabs, &QTabWidget::tabCloseRequested, this, &MainWindow::onViewTabCloseRequested);
  KaTabClose::follow(m_viewTabs);  // 새 모양 「도면 ✕」: a clear muted cross on closable tabs (2차 보드 판 4)
  connect(m_viewTabs, &QTabWidget::currentChanged, this, [this](int i) {
    if (!m_viewTabs) return;
    QWidget* page = m_viewTabs->widget(i);
    KaShellUi::setEnabledWithReason(m_actMapGeoTiff, page == m_mapPage, KaShellUi::mapTabOnlyReason());
    if (m_startPage && page == m_startPage)
      m_startPage->reload();
    if (m_status) {
      // 지도 탭만 커서 좌표와 지도갱신을 쓴다. 도면 탭은 아래 축척칸으로 용지 축척을
      // 고치므로 축척만 남긴다. 좌표계 알약은 조사가 있는 탭에서만 보인다.
      const bool onMap = page == m_mapPage;
      const bool onDrawing = m_drawingStudio && page == m_drawingStudio;
      m_status->setInstrumentsVisible(onMap, onMap || onDrawing);
      m_status->setCrsChipsVisible(page != m_startPage);
    }
    if (m_ribbon) {
      // 작업 탭에서는 지도 전용 묶음 제목을 옅게 해 「지금 탭의 일이 아님」을 보인다.
      const bool studio = page != m_mapPage && page != m_startPage;
      for (const QString& id : mapOnlyRibbonGroups()) {
        QFrame* group = m_ribbon->group(id);
        if (!group || group->property("offContext").toBool() == studio) continue;
        group->setProperty("offContext", studio);
        for (QWidget* w : group->findChildren<QLabel*>(QStringLiteral("ribbonGroupCaption"))) {
          w->style()->unpolish(w);
          w->style()->polish(w);
        }
      }
    }
    if (m_mapPage && page == m_mapPage) {
      resumeParkedSketch();  // [pkg B1] F088: show a kept sketch's drawing row again
      QTimer::singleShot(0, this, [this]() { ensureStartupViewReady(); });
    } else if (!parkSketchForOtherTab()) {  // [pkg B1] F088: keep unfinished sketch points
      hideSubTools();
    }
    if (m_drawingStudio && page == m_drawingStudio)
      QTimer::singleShot(0, this, [this]() {
        if (!m_drawingStudio) return;
        m_drawingStudio->refreshMapFromProject();
      });
    if (m_canvas) onCanvasScaleChanged(m_canvas->scale());
  });
  m_viewTabs->setCurrentWidget(m_startPage);
  if (m_status) {
    m_status->setMapInstrumentsVisible(false);
    m_status->setCrsChipsVisible(false);
  }
  setCentralWidget(m_viewTabs);
  // 자동 저장 없음. 저장은 사용자가 「저장」(Ctrl+S)을 누를 때만 일어난다.
  // 저장 안 한 작업은 창 제목의 * 로 보이고, 닫을 때 한 번 물어본다.
  if (QgsProject* proj = QgsProject::instance()) {
    connect(proj, &QgsProject::isDirtyChanged, this, [this](bool) { refreshWindowTitle(); });
  }
  // 레이어가 세션 도중 사라지는 일을 잡기 위한 감시. 레이어 상태를 세어 직전과 다르면
  // 무엇이 어떻게 달라졌는지 세션 로그에 남긴다. 사용자가 "사라졌다"고 말한 시각과
  // 로그를 맞춰 볼 수 있는 유일한 근거다. 파일에 쓰지 않으므로 저장과 무관하다.
  m_layerWatchTimer = new QTimer(this);
  m_layerWatchTimer->setObjectName(QStringLiteral("layerWatchTimer"));
  m_layerWatchTimer->setInterval(30000);
  connect(m_layerWatchTimer, &QTimer::timeout, this, &MainWindow::auditLayerHealth);
  m_layerWatchTimer->start();
  // 원본 조사 파일에는 쓰지 않는다. 간격은 60초보다 길게 두어, 없앤 20초 자동 저장과
  // 같은 타이머로 잡히지 않게 한다.
  m_recoverySnapshotTimer = new QTimer(this);
  m_recoverySnapshotTimer->setObjectName(QStringLiteral("recoverySnapshotTimer"));
  m_recoverySnapshotTimer->setInterval(120000);
  connect(m_recoverySnapshotTimer, &QTimer::timeout, this, &MainWindow::captureRecoverySnapshot);
  m_recoverySnapshotTimer->start();
  // 시작은 홈 화면만 연다. 마지막 조사·배경지도·작업공간을 스스로 복원하지 않는다.
#endif

}

void MainWindow::openTerrain3dStudio() {
#if KA_HGIS_HAS_QGIS
  if (!m_viewTabs)
    return;
  if (m_terrain3dStudio && m_viewTabs->indexOf(m_terrain3dStudio) >= 0) {
    m_viewTabs->setCurrentWidget(m_terrain3dStudio);
    hideSubTools();
    return;
  }
  if (!m_terrain3dStudio) {
    m_terrain3dStudio = new KaTerrain3dStudio(QgsProject::instance(), m_canvas, this);
    m_terrain3dStudio->setAttribute(Qt::WA_DeleteOnClose, false);
    connect(m_terrain3dStudio, &KaTerrain3dStudio::requestDrawingStudio, this,
            &MainWindow::placeTerrain3dOnSheet);
  }
  m_terrain3dStudio->setParent(m_viewTabs, Qt::Widget);
  if (m_viewTabs->indexOf(m_terrain3dStudio) < 0)
    m_viewTabs->addTab(m_terrain3dStudio, KaIcons::icon(QStringLiteral("terrain_3d"), KaTheme::tokens().inkMuted),
                       QStringLiteral("입체지형"));
  m_viewTabs->setCurrentWidget(m_terrain3dStudio);
  hideSubTools();
  statusBar()->showMessage(QStringLiteral("지금 지도 화면을 고해상 입체로 만듭니다."), 6000);
#endif
}

// 레이어가 수십 개가 되면 하나씩 체크를 푸는 게 일이다. 한 번에 끄고 켠다.
// 라벨은 「지금 상태」가 아니라 「누르면 일어날 일」을 보여 준다.
// 라벨은 QGIS 가 맨 마지막에 한꺼번에 얹는다. 그대로 두면 아래 레이어의 지번이
// 위 레이어의 선 위로 올라온다. 레이어 순서를 글자에도 그대로 먹인다.
// Delete·Ctrl+Z 는 이 창의 단축키(WindowShortcut)라 같은 창 안 탭인 조판 화면보다
// 먼저 잡힌다. 그래서 조판에서는 레이어도 범례도 지워지지 않았다.
// 조판이 열려 있으면 그 화면에 넘긴다. 글자 칸에 커서가 있으면 아무것도 하지 않는다.
bool MainWindow::routeEditKeyToActiveStudio(bool isDelete) {
#if KA_HGIS_HAS_QGIS
  if (m_topographicBrowser && (QApplication::activeWindow() == m_topographicBrowser.data() ||
      m_topographicBrowser->isAncestorOf(QApplication::focusWidget())))
    return true;
  QWidget* focus = QApplication::focusWidget();
  if (qobject_cast<QLineEdit*>(focus) || qobject_cast<QAbstractSpinBox*>(focus))
    return true;  // 글자를 고치는 중이다. 도형·레이어를 지우면 안 된다.
  if (m_viewTabs && m_terrain3dLayoutStudio && m_viewTabs->currentWidget() == m_terrain3dLayoutStudio) {
    if (isDelete) m_terrain3dLayoutStudio->deleteSelectedItems();
    else m_terrain3dLayoutStudio->undoLastChange();
    return true;
  }
  if (m_viewTabs && m_viewTabs->currentWidget() &&
      m_viewTabs->currentWidget()->objectName() == QLatin1String("paleoStudio")) {
    if (isDelete) {
      if (auto* tree = m_viewTabs->currentWidget()->findChild<QgsLayerTreeView*>())
        removeLayersFromTree(tree);
    }
    return true;
  }
  if (!m_viewTabs || !m_drawingStudio) return false;
  if (m_viewTabs->currentWidget() != m_drawingStudio) return false;
  if (isDelete)
    m_drawingStudio->handleDeleteKey();
  else
    m_drawingStudio->handleUndoKey();
  return true;
#else
  Q_UNUSED(isDelete);
  return false;
#endif
}

// 덧그림에 올릴 레이어를 다시 고른다. 레이어 순서·표시가 바뀔 때마다 부른다.
void MainWindow::refreshAboveLabelsOverlay() {
#if KA_HGIS_HAS_QGIS
  if (!m_aboveLabels) return;
  const QList<QgsMapLayer*> above = LayerOps::layersDrawnAboveLabels(QgsProject::instance());
  m_aboveLabels->setLayers(above);
  // 몇 개가 뽑혔는지 세션 로그에 남긴다. 0이면 규칙이 대상을 못 고른 것이다.
  if (m_aboveLabelsCount != above.size()) {
    m_aboveLabelsCount = above.size();
    QStringList names;
    for (QgsMapLayer* l : above) {
      if (l) names << l->name();
    }
    // [pkg E1] F184: developer diagnostics go to the session log, not the status bar.
    KaSessionLog::line(
        above.isEmpty()
            ? QStringLiteral("[labels] 글자 위로 올릴 레이어 없음")
            : QStringLiteral("[labels] 글자 위로 올릴 레이어 %1개: %2")
                  .arg(above.size())
                  .arg(names.join(QStringLiteral(", "))));
  }
#endif
}

void MainWindow::applyLabelStackOrder() {
#if KA_HGIS_HAS_QGIS
  LayerOps::applyLayerOrderToLabels(QgsProject::instance(), m_canvas);
  refreshAboveLabelsOverlay();
  if (m_drawingStudio)
    m_drawingStudio->repaintMapLayers();
#endif
}

void MainWindow::toggleAllLayersChecked() {
#if KA_HGIS_HAS_QGIS
  QgsProject* proj = QgsProject::instance();
  QgsLayerTree* root = proj ? proj->layerTreeRoot() : nullptr;
  if (!root) return;
  if (m_layerTreeUndo) m_layerTreeUndo->noteUserGesture();  // [pkg E1] F186: 전체 켜기/끄기 is the user's
  const QList<QgsLayerTreeLayer*> layers = root->findLayers();
  if (layers.isEmpty()) return;
  bool anyOn = false;
  for (QgsLayerTreeLayer* n : layers) {
    if (n && n->isVisible()) {
      anyOn = true;
      break;
    }
  }
  // 하나라도 켜져 있으면 전부 끈다. 전부 꺼져 있을 때만 전부 켠다.
  const bool turnOn = !anyOn;
  const auto children = root->children();
  for (QgsLayerTreeNode* n : children) {
    if (n) n->setItemVisibilityCheckedRecursive(turnOn);
  }
  refreshLayerCheckAllButton();
  LayerOps::refreshCanvasIfIdle(m_canvas);
  statusBar()->showMessage(turnOn ? QStringLiteral("레이어 %1개를 모두 켰습니다").arg(layers.size())
                                  : QStringLiteral("레이어 %1개를 모두 껐습니다").arg(layers.size()),
                           4000);
#endif
}

void MainWindow::refreshLayerCheckAllButton() {
#if KA_HGIS_HAS_QGIS
  if (!m_layerCheckAllBtn) return;
  QgsProject* proj = QgsProject::instance();
  QgsLayerTree* root = proj ? proj->layerTreeRoot() : nullptr;
  const QList<QgsLayerTreeLayer*> layers = root ? root->findLayers() : QList<QgsLayerTreeLayer*>();
  m_layerCheckAllBtn->setEnabled(!layers.isEmpty());
  bool anyOn = false;
  for (QgsLayerTreeLayer* n : layers) {
    if (n && n->isVisible()) {
      anyOn = true;
      break;
    }
  }
  m_layerCheckAllBtn->setText(anyOn ? QStringLiteral("전체 끄기") : QStringLiteral("전체 켜기"));
#endif
}

void MainWindow::updateLayerOpacityControl() {
#if KA_HGIS_HAS_QGIS
  if (!m_layerOpacityRail || !m_layerTree) return;
  QgsMapLayer* cur = m_layerTree->currentLayer();
  if (LayerOps::canAdjustOpacity(cur)) {  // [pkg E1] F018
    const int val = qBound(0, qRound(LayerOps::mapLayerOpacity(cur) * 100.0), 100);
    m_layerOpacityRail->setPercent(val, true);
  } else {
    m_layerOpacityRail->setPercent(100, false);
  }
  // 밝기는 그림(래스터)에만 있다. 항공사진·위성·지형맵이 대상이다.
  const bool bright = LayerOps::canAdjustBrightness(cur);
  m_layerOpacityRail->setBrightness(bright ? LayerOps::mapLayerBrightness(cur) : 0, bright);
  m_layerOpacityRail->setTarget(cur ? cur->name() : QString(),
                cur && (bright || LayerOps::canAdjustOpacity(cur)));
  if (m_drawingStudio)
    m_drawingStudio->updateLayerOpacityControl();
#endif
}

void MainWindow::ensureDefaultBasemaps() {
#if KA_HGIS_HAS_QGIS
  QgsProject* proj = QgsProject::instance();
  if (!proj) return;
  const bool dirtyBefore = proj->isDirty();
  // 저장된 조사의 위성·지적 주소에는 그때 쓰던 인증키가 박혀 있다. 키를 새로 받아도
  // 예전 조사를 열면 만료된 키로 타일을 받아 배경지도가 소리 없이 백지가 됐다.
  {
    QStringList swapped;
    const int n = LayerOps::refreshVworldApiKeyInLayers(proj, VworldSettings::loadApiKey(), &swapped);
    if (n > 0)
      KaCrashGuard::logLine(QStringLiteral("[basemap] 저장된 VWorld 인증키를 현재 키로 교체 %1개 — %2")
                                .arg(n)
                                .arg(swapped.join(QStringLiteral(", "))));
  }
  LayerOps::pruneDuplicateSatelliteLayers(proj);
  bool hasSat = false;
  for (QgsMapLayer* l : proj->mapLayers()) {
    if (!l) continue;
    // 이름만 보고 "이미 있다"고 판단하면, 원본이 깨진 배경지도가 이름만 남아 영영
    // 다시 만들어지지 않는다(지적 설정 파일이 지워진 경우가 그랬다). 살아 있는
    // 레이어만 있다고 친다.
    if (!l->isValid()) continue;
    if (l->name().contains(QStringLiteral("위성")))
      hasSat = true;
  }
  bool hasCad = false;
  for (QgsMapLayer* l : proj->mapLayers()) {
    if (l && l->isValid() && LayerOps::isVworldCadastralPicture(l))
      hasCad = true;
  }
  const QString key = VworldSettings::loadApiKey();
  QString satErr;
  QString cadErr;
  // Add without canvas so LayerOps does not rewrite the current extent/scale.
  if (!hasSat)
    hasSat = LayerOps::addVworldSatelliteMap(proj, nullptr, key, &satErr);
  if (!hasCad && !key.isEmpty() && !LayerOps::userRemovedCadastral(proj))
    hasCad = LayerOps::addVworldCadastralMap(proj, nullptr, key, &cadErr);
  LayerOps::ensureSatelliteAtBottom(proj);
  // 예전에는 실패해도 사라지는 상태바 메시지뿐이라 현장 로그에 아무 흔적이 없었다.
  // 무엇이 왜 안 올라왔는지 반드시 남긴다. 인증키 값 자체는 절대 남기지 않는다.
  KaCrashGuard::logLine(
      QStringLiteral("[basemap] 위성 %1 · 지적 %2 · 키 %3%4%5")
          .arg(hasSat ? QStringLiteral("있음") : QStringLiteral("없음"),
               hasCad ? QStringLiteral("있음") : QStringLiteral("없음"),
               key.isEmpty() ? QStringLiteral("없음") : QStringLiteral("있음"),
               satErr.isEmpty() ? QString()
                                : QStringLiteral(" · 위성오류=%1").arg(satErr.left(200)),
               cadErr.isEmpty() ? QString()
                                : QStringLiteral(" · 지적오류=%1").arg(cadErr.left(200))));
  if (hasSat && hasCad)
    statusBar()->showMessage(QStringLiteral("위성과 지적도를 올려 두었습니다."), 5000);
  else if (hasSat && !hasCad)
    statusBar()->showMessage(
        key.isEmpty()
            ? QStringLiteral("위성은 올렸습니다. 지적도는 VWorld API 키가 필요합니다.")
            : (cadErr.isEmpty() ? QStringLiteral("지적도를 올리지 못했습니다.") : cadErr),
        8000);
  else if (!hasSat)
    statusBar()->showMessage(satErr.isEmpty() ? QStringLiteral("위성을 올리지 못했습니다.") : satErr,
                             8000);
  if (!dirtyBefore)
    proj->setDirty(false);
  updateHistoricalMapButtons();
#endif
}

void MainWindow::applyStartupMap() {
#if KA_HGIS_HAS_QGIS
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, m_workCrs);
  LayerOps::setWorkCrs(QgsProject::instance(), m_canvas, m_workCrs, nullptr, false);
  if (m_canvas) m_canvas->freeze(true);
  LayerOps::applyKoreaMapLimits(QgsProject::instance(), m_canvas);
  updateNextActionStatus();
  m_startupViewApplied = false;
  // 지적 WMS는 GetCapabilities 왕복을 동기로 기다린다(현장 측정 약 2.9초).
  // 창이 뜨기 전에 그 왕복을 붙들고 있으면 시작이 그만큼 늦다. 이벤트 루프로
  // 미뤄 창을 먼저 띄우고 배경지도는 뒤따라 올린다.
  if (!m_basemapBootPending) {
    m_basemapBootPending = true;
    QTimer::singleShot(0, this, &MainWindow::loadBootBasemaps);
  }
  LayerOps::pruneEmptyLegendGroups(QgsProject::instance());
  ensureStartupViewReady();
#endif
}

void MainWindow::syncThematicButtons() {
#if KA_HGIS_HAS_QGIS
  QgsProject* proj = QgsProject::instance();
  if (!proj) return;
  // 범례가 진실이다. 레이어를 지우거나 레이어의 체크를 끄면 아이콘도 꺼진다.
  // isLayerVisible은 레이어가 없으면 false라 삭제·체크해제를 한 번에 덮는다.
  const auto sync = [proj](QAction* act, QToolButton* btn, const QString& title) {
    const bool on = LayerOps::isLayerVisible(proj, title);
    if (act && act->isCheckable() && act->isChecked() != on) {
      const QSignalBlocker block(act);
      act->setChecked(on);
    }
    if (btn && btn->isCheckable() && btn->isChecked() != on) {
      const QSignalBlocker block(btn);
      btn->setChecked(on);
    }
  };
  sync(nullptr, m_btnTerrain, QStringLiteral("지형맵"));
  sync(nullptr, m_btnDem, QStringLiteral("DEM"));
  sync(nullptr, m_btnSoil, QStringLiteral("토양도(흙토람)"));
  sync(m_actGeology, nullptr, QStringLiteral("지질도(KIGAM 1:5만)"));
  sync(m_actRiver, nullptr, QStringLiteral("수계도(하천망)"));
  updateHistoricalMapButtons();
  syncBasemapCard();  // [P6] 「배경 지도」 카드도 범례를 따른다
#endif
}

void MainWindow::updateHistoricalMapButtons() {
  const bool hasHistoryKey =
      LayerOps::historyGisApiKeyUsable(VworldSettings::loadHistoryGisApiKey());
  if (auto* map1919 = findChild<QAction*>(QStringLiteral("actionMap1919"))) {
    map1919->setEnabled(hasHistoryKey);
    map1919->setToolTip(
        hasHistoryKey
            ? QStringLiteral("1919년 조선지형도 1:5만 (국사편찬위원회 WMTS, EPSG:5179)")
            : QStringLiteral(
                  "더보기 → API 키 입력에 역사지리정보DB 키를 저장하면 켤 수 있습니다"));
  }
}

void MainWindow::syncRecordTools() {
  const bool ready = m_surveySessionReady;
  // A grey chip says why in its tooltip; the chip keeps its size and short label.
  const QString why = KaShellUi::needSurveyReason();
  KaShellUi::setEnabledWithReason(m_actSelect, ready, why);
  KaShellUi::setEnabledWithReason(m_actMeasure, ready, why);
  KaShellUi::setEnabledWithReason(m_btnDraw, ready, why);
  if (auto* trench = findChild<QToolButton*>(QStringLiteral("btnTrenchGrid"))) {
    // The chip mirrors its action's tooltip, so the reason goes on the action.
    if (QAction* action = trench->defaultAction())
      KaShellUi::setEnabledWithReason(action, ready, why);
    else
      KaShellUi::setEnabledWithReason(trench, ready, why);
  }
  if (auto* buffer = findChild<QToolButton*>(QStringLiteral("btnBuffer")))
    KaShellUi::setEnabledWithReason(buffer, ready, why);
}

void MainWindow::loadBootBasemaps() {
#if KA_HGIS_HAS_QGIS
  if (!m_basemapBootPending) return;
  if (m_isOpeningSurvey) {
    // 조사를 여는 중이면 지금 올리지 않는다. 그런데 예전에는 여기서 대기 표시를
    // 꺼 버리고 끝냈다. 시작하자마자 작업공간을 복원하는 포터블에서는 이 경합에
    // 늘 져서 위성·지적이 영영 올라오지 않았다(현장 로그에 [boot] 배경지도 줄이
    // 아예 없다). 표시를 유지하고 열기가 끝난 뒤 다시 시도한다.
    if (m_basemapBootRetries < kBasemapBootRetryMax) {
      ++m_basemapBootRetries;
      QTimer::singleShot(300, this, &MainWindow::loadBootBasemaps);
    } else {
      m_basemapBootPending = false;
      KaCrashGuard::logLine(
          QStringLiteral("[boot] 배경지도 건너뜀 — 조사 열기가 %1초 넘게 끝나지 않았다")
              .arg(kBasemapBootRetryMax * 0.3, 0, 'f', 1));
    }
    return;
  }
  m_basemapBootPending = false;
  m_basemapBootRetries = 0;
  m_isLoadingBasemaps = true;
  QElapsedTimer bm;
  bm.start();
  ensureDefaultBasemaps();
  LayerOps::pruneEmptyLegendGroups(QgsProject::instance());
  ensureStartupViewReady();
  m_isLoadingBasemaps = false;
  KaCrashGuard::logLine(
      QStringLiteral("[boot] 배경지도 %1 ms · 미리보기 = %2 · 병렬렌더 = %3")
          .arg(bm.elapsed())
          .arg(m_canvas && m_canvas->previewJobsEnabled() ? QStringLiteral("켬")
                                                          : QStringLiteral("끔"))
          .arg(QStringLiteral("끔")));
#endif
}

void MainWindow::ensureStartupViewReady() {
#if KA_HGIS_HAS_QGIS
  if (m_isOpeningSurvey || !m_canvas || m_startupViewApplied) return;
  if (m_canvas->width() < 40 || m_canvas->height() < 40) return;
  m_canvas->freeze(true);
  LayerOps::applyCanvasScreenDpi(m_canvas);
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, m_workCrs);
  LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  m_canvas->freeze(true);
  LayerOps::zoomToKorea(m_canvas, m_workCrs, false);
  LayerOps::clampCanvasToKorea(m_canvas);
  m_canvas->freeze(false);
  LayerOps::refreshXyzBasemapTiles(m_canvas);
  m_startupViewApplied = true;
#endif
}

void MainWindow::scheduleMapDisplayRefresh() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  if (!m_displayRefresh) {
    m_displayRefresh = new QTimer(this);
    m_displayRefresh->setObjectName(QStringLiteral("mapDisplayRefresh"));
    m_displayRefresh->setSingleShot(true);
    connect(m_displayRefresh, &QTimer::timeout, this, [this]() {
      if (!m_canvas) return;
      if (m_canvas->isDrawing()) {
        if (m_displayRefreshWaits < 30) {
          ++m_displayRefreshWaits;
          m_displayRefresh->start(200);
        }
        return;
      }
      m_displayRefreshWaits = 0;
      const QSize before = m_canvas->mapSettings().outputSize();
      const float dprBefore = m_canvas->mapSettings().devicePixelRatio();
      const double dpiBefore = m_canvas->mapSettings().outputDpi();
      LayerOps::applyCanvasScreenDpi(m_canvas);
      const bool changed = m_canvas->mapSettings().outputSize() != before ||
                           !qFuzzyCompare(m_canvas->mapSettings().devicePixelRatio(), dprBefore) ||
                           !qFuzzyCompare(m_canvas->mapSettings().outputDpi(), dpiBefore);
      if (changed)
        LayerOps::refreshXyzBasemapTiles(m_canvas);
    });
  }
  // QGIS resizeEvent는 500ms 뒤에 refresh 한다. 그 전에 격자를 바꾸지 않는다.
  m_displayRefreshWaits = 0;
  m_displayRefresh->start(700);
#endif
}

void MainWindow::changeEvent(QEvent* event) {
  QMainWindow::changeEvent(event);
#if KA_HGIS_HAS_QGIS
  if (event && event->type() == QEvent::WindowStateChange)
    scheduleMapDisplayRefresh();
#endif
}

void MainWindow::bindMapDisplayScreen() {
#if KA_HGIS_HAS_QGIS
  QWindow* wh = windowHandle();
  if (!wh || m_mapScreenBound) return;
  m_mapScreenBound = true;
  connect(wh, &QWindow::screenChanged, this, [this](QScreen*) {
    KaWindowGeometry::fit(this);
    if (m_canvas) LayerOps::applyCanvasScreenDpi(m_canvas);
    scheduleMapDisplayRefresh();
    QTimer::singleShot(100, this, [this]() {
      if (m_mainSplit) {
        const int tw = m_mainSplit->width();
        if (tw > 300) {
          const QList<int> sz = m_mainSplit->sizes();
          if (!sz.isEmpty() && sz.at(0) > tw * 0.35) {
            const int lw = qBound(200, int(tw * 0.22), 360);
            m_mainSplit->setSizes(KaShellFocus::sizesWithLeft(m_mainSplit, lw));
          }
        }
      }
      updateGeometry();
      if (centralWidget()) centralWidget()->updateGeometry();
    });
  });
#endif
}

void MainWindow::showEvent(QShowEvent* event) {
  QMainWindow::showEvent(event);
  KaWindowGeometry::fit(this);
#if KA_HGIS_HAS_QGIS
  bindMapDisplayScreen();
  if (m_canvas) {
    LayerOps::applyCanvasScreenDpi(m_canvas);
    scheduleMapDisplayRefresh();
  }
  // No saved layout yet: size the left panel by window width (capped at the old 348 px).
  if (m_shellFocus) m_shellFocus->applyDefaultWidthOnce();
  if (m_mainSplit) {
    const int tw = m_mainSplit->width();
    if (tw > 300) {
      const QList<int> sz = m_mainSplit->sizes();
      if (!sz.isEmpty() && sz.at(0) > tw * 0.35) {
        const int lw = qBound(200, int(tw * 0.22), 360);
        m_mainSplit->setSizes(KaShellFocus::sizesWithLeft(m_mainSplit, lw));
      }
    }
  }
  if (!m_recoveryOfferDone)
    QTimer::singleShot(0, this, &MainWindow::offerRecoverySnapshot);
#endif
}

void MainWindow::zoomSelectedLayerMax() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  QgsMapLayer* layer = nullptr;
  if (m_layerTree) {
    layer = m_layerTree->currentLayer();
    if (!layer) {
      const QList<QgsMapLayer*> sel = m_layerTree->selectedLayers();
      if (!sel.isEmpty())
        layer = sel.first();
    }
  }
  if (!layer) {
    zoomMapToFullMax();
    return;
  }
  if (!LayerOps::zoomToLayerMax(m_canvas, layer)) {
    statusBar()->showMessage(
        QStringLiteral("이 레이어에 도형이 없습니다. 먼저 그린 뒤 다시 누르세요: %1").arg(layer->name()),
        8000);
    return;
  }
  // 옮긴 범위로 위성·지적 타일을 다시 받아 온다. 예전에는 이동 직후 배경이 비어
  // 있다가 사용자가 줌인·줌아웃하거나 점을 찍고 지워야 채워졌다.
  LayerOps::refreshXyzBasemapTiles(m_canvas);
  statusBar()->showMessage(QStringLiteral("이 레이어로 이동: %1").arg(layer->name()), 5000);
#endif
}

void MainWindow::renameSelectedLayer(QgsMapLayer* targetLayer) {
#if KA_HGIS_HAS_QGIS
  QString currentName;
  QgsMapLayer* mapLayer = targetLayer;
  if (!mapLayer && m_layerTree) {
    mapLayer = m_layerTree->currentLayer();
  }
  QgsLayerTreeNode* node = m_layerTree ? m_layerTree->currentNode() : nullptr;

  if (mapLayer) {
    currentName = mapLayer->name();
  } else if (node && QgsLayerTree::isGroup(node)) {
    currentName = node->name();
  } else {
    QMessageBox::information(this, QStringLiteral("이름 바꾸기"),
                             QStringLiteral("이름을 바꿀 레이어 또는 그룹을 선택하세요."));
    return;
  }

  bool ok = false;
  const QString name = QInputDialog::getText(
      this, QStringLiteral("이름 바꾸기"), QStringLiteral("새 이름:"),
      QLineEdit::Normal, currentName, &ok);
  if (!ok) return;
  const QString trimmed = name.trimmed();
  if (trimmed.isEmpty()) {
    KaUserError::warn(this, {QStringLiteral("이름 바꾸기"), QStringLiteral("이름을 바꾸지 못했습니다"), QStringLiteral("이름은 비울 수 없습니다."), QStringLiteral("한 글자 이상 넣으세요.")});
    return;
  }
  if (trimmed == currentName) return;

  if (mapLayer)
    mapLayer->setName(trimmed);
  else if (node)
    node->setName(trimmed);

  if (m_canvas) m_canvas->refresh();
  if (m_drawingStudio) m_drawingStudio->refreshMapFromProject();
  statusBar()->showMessage(QStringLiteral("이름 변경: %1 → %2").arg(currentName, trimmed), 5000);
#endif
}

void MainWindow::onLayerTreeDoubleClicked(const QModelIndex& index) {
#if KA_HGIS_HAS_QGIS
  if (!m_layerTree || !index.isValid()) return;
  m_layerTree->setCurrentIndex(index);
  m_layerTree->edit(index);
#else
  Q_UNUSED(index);
#endif
}

void MainWindow::importDemElevationRaster() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("국토지리원 DEM"), QString(),
      QStringLiteral("국토지리원 DEM (*.img *.IMG *.tif *.tiff *.TIF *.TIFF)"));
  if (path.isEmpty()) return;
  QString err;
  if (!LayerOps::addDemElevationRaster(QgsProject::instance(), m_canvas, path, &err)) {
    notify(Notice::Warning, QStringLiteral("DEM"),
           err.isEmpty() ? QStringLiteral("DEM 파일을 열지 못했습니다.") : err);
    if (m_btnDem) m_btnDem->setChecked(false);
    return;
  }
  PaleoAnalysisRunner::registerDemWideLayers(QgsProject::instance(), m_canvas, path, m_surveyPath,
                                             nullptr);
  if (m_btnDem) m_btnDem->setChecked(true);
  statusBar()->showMessage(QStringLiteral("국토지리원 DEM을 올렸습니다. 범례에 높이(m)가 표시됩니다."), 6000);
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("DEM 파일 불러오기"));
#endif
}

void MainWindow::runDemHillshade() {
#if KA_HGIS_HAS_QGIS
  QString dem;
  if (auto* loadedDem = BasemapDsm::findDem(QgsProject::instance())) {
    if (QFileInfo::exists(loadedDem->source())) dem = loadedDem->source();
  }
  if (dem.isEmpty()) {
    dem = QFileDialog::getOpenFileName(
        this, QStringLiteral("DEM GeoTIFF"), QString(),
        QStringLiteral("GeoTIFF (*.tif *.tiff *.TIF *.TIFF)"));
  }
  if (dem.isEmpty())
    return;
  QDialog dlg(this);
  dlg.setWindowTitle(QStringLiteral("지형분석"));
  auto* form = new QFormLayout(&dlg);
  auto* mode = new QComboBox(&dlg);
  mode->addItem(QStringLiteral("다중광원 음영 (유구·분묘)"), static_cast<int>(DemAnalyzer::HillshadeMode::Multi));
  mode->addItem(QStringLiteral("단방향 음영"), static_cast<int>(DemAnalyzer::HillshadeMode::Single));
  auto* az = new QDoubleSpinBox(&dlg);
  az->setRange(0, 360);
  az->setValue(315);
  az->setSuffix(QStringLiteral(" °"));
  auto* alt = new QDoubleSpinBox(&dlg);
  alt->setRange(1, 90);
  alt->setValue(45);
  alt->setSuffix(QStringLiteral(" °"));
  auto* zf = new QDoubleSpinBox(&dlg);
  zf->setRange(0.01, 50);
  zf->setValue(1.0);
  form->addRow(QStringLiteral("방식"), mode);
  form->addRow(QStringLiteral("방위각"), az);
  form->addRow(QStringLiteral("고도각"), alt);
  form->addRow(QStringLiteral("Z계수"), zf);
  auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  form->addRow(box);
  connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted)
    return;
  DemAnalyzer::Options opt;
  opt.hillshade = static_cast<DemAnalyzer::HillshadeMode>(mode->currentData().toInt());
  opt.azimuthDeg = az->value();
  opt.altitudeDeg = alt->value();
  opt.zFactor = zf->value();
  // [pkg F2] F069: a new file in the survey's 지형분석 folder; with no survey open the user confirms where.
  QString out = DemAnalyzer::hillshadeOutputPath(dem, m_surveyPath);
  if (m_surveyPath.isEmpty()) {
    out = QFileDialog::getSaveFileName(this, QStringLiteral("음영기복 저장"), out,
                                       QStringLiteral("GeoTIFF (*.tif)"));
    if (out.isEmpty())
      return;
  }
  QString err;
  if (!DemAnalyzer::runHillshadeFile(dem, out, opt, &err)) {
    notify(Notice::Warning, QStringLiteral("지형분석"),
           QStringLiteral("음영기복을 만들지 못했습니다."), err);
    return;
  }
  if (!addRasterFromPath(out)) {
    notify(Notice::Warning, QStringLiteral("지형분석"),
           QStringLiteral("음영 지도를 지도 화면에 올리지 못했습니다."), out);
    return;
  }
  if (auto* rl = qobject_cast<QgsRasterLayer*>(QgsProject::instance()->mapLayersByName(
          QFileInfo(out).completeBaseName()).value(0, nullptr))) {
    LayerOps::markReferenceLayer(rl);
    rl->setName(QStringLiteral("음영기복"));
    LayerOps::placeInLegendGroup(QgsProject::instance(), rl, QStringLiteral("참조 지도"));
  }
  LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  statusBar()->showMessage(QStringLiteral("음영기복을 참조 지도에 올렸습니다."), 6000);
#endif
}

namespace {
TrenchGridGenerator::Spec g_pendingTrench;

#if KA_HGIS_HAS_QGIS
TrenchGridGenerator::PickedArea trenchFillFromSurveyLayer(QgsVectorLayer* areaVl, bool useAll = false) {
  TrenchGridGenerator::PickedArea empty;
  if (!areaVl)
    return empty;
  std::vector<TrenchGridGenerator::SurveyPoly> feats;
  QgsFeature f;
  QgsFeatureIterator it = areaVl->getFeatures();
  while (it.nextFeature(f)) {
    if (!f.hasGeometry() || f.geometry().isEmpty())
      continue;
    feats.push_back({f.geometry().asWkb(), f.id()});
  }
  std::vector<qint64> selected;
  const QgsFeatureIds ids = areaVl->selectedFeatureIds();
  selected.reserve(static_cast<size_t>(ids.size()));
  for (QgsFeatureId id : ids)
    selected.push_back(id);
  return TrenchGridGenerator::pickAutoFillArea(feats, selected, useAll);
}

QString leftoverSurveyAreaHint(const TrenchGridGenerator::PickedArea& pick) {
  if (pick.totalCount <= pick.usedCount || pick.usedCount <= 0)
    return {};
  if (pick.usedSelection) {
    return QStringLiteral("조사구역 %1곳 중 선택한 %2곳에만 시굴격자를 놓습니다.")
        .arg(pick.totalCount)
        .arg(pick.usedCount);
  }
  return QStringLiteral(
             "조사구역 %1곳이 남아 있어 마지막에 그린 구역에만 시굴격자를 놓습니다. "
             "예전 구역에 놓으려면 그 구역을 선택한 뒤 다시 누르세요.")
      .arg(pick.totalCount);
}
#endif
}

void MainWindow::startTrenchGrid() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas)
    return;
  if (m_surveyPath.isEmpty()) {
    notify(Notice::Info, QStringLiteral("시굴격자"),
           QStringLiteral("먼저 「새 조사」를 만들거나 「열기」로 조사를 여세요."));
    return;
  }
  // 시굴조사 도메인: 선택한(없으면 마지막) 조사구역만 규칙 배치로 덮고,
  // 총 굴착 면적이 그 구역 면적의 규정 비율(시굴 10%, 표본 2%)인지 확인한다.
  // 남은 옛 조사구역을 union 하면 격자가 그 큰 구역에 깔린다.
  QByteArray areaWkb;
  double areaM2 = 0.0;
  QString areaCrs;
  if (auto* areaVl = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"))) {
    const TrenchGridGenerator::PickedArea pick = trenchFillFromSurveyLayer(areaVl);
    areaWkb = pick.wkb;
    areaM2 = pick.areaM2;
    areaCrs = areaVl->crs().authid();
    const QString leftover = leftoverSurveyAreaHint(pick);
    if (!leftover.isEmpty())
      statusBar()->showMessage(leftover, 8000);
  }

  // 세밀 설정(회전 포함)은 하나의 속성 창에서: 모덜리스라 맵을 보면서 조정한다.
  if (!m_trenchDlg) {
    m_trenchDlg = new KaTrenchDialog(this);
    connect(m_trenchDlg, &KaTrenchDialog::applyRequested, this,
            &MainWindow::applyTrenchFromDialog);
    connect(m_trenchDlg, &KaTrenchDialog::manualPlaceRequested, this,
            &MainWindow::beginTrenchOriginPick);
    connect(m_trenchDlg, &KaTrenchDialog::editSingleRequested, this,
            &MainWindow::startTrenchGridEdit);
    connect(m_trenchDlg, &KaTrenchDialog::moveRequested, this,
            &MainWindow::startTrenchGridMove);
  }
  m_trenchDlg->setArea(areaWkb, areaM2);
  m_trenchDlg->setTerrainAspect(terrainAspectForArea(areaWkb, areaCrs));
  syncTrenchCoordinateGrid(areaCrs);
  {
    auto* trenchVl = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("trial_trench"));
    m_trenchDlg->setGridPlaced(trenchVl && trenchVl->featureCount() > 0);
  }
  m_trenchDlg->show();
  m_trenchDlg->raise();
  if (m_trenchDlg->autoFill()) {
    applyTrenchFromDialog();
  } else {
    beginTrenchOriginPick();
  }
#endif
}

// 조사구역 안에서 DEM 표고를 격자로 뽑아 오르막 방위를 낸다. DEM이 없으면 수치지형도 등고선 표고로 낸다.
// 지형 자료가 없거나 평지면 valid=false — 그때는 방위 칸 값(기본 남북)을 그대로 쓴다.
TrenchGridGenerator::SlopeAspect MainWindow::terrainAspectForArea(const QByteArray& areaWkb,
                                                                 const QString& areaCrs) {
#if KA_HGIS_HAS_QGIS
  TrenchGridGenerator::SlopeAspect none;
  if (areaWkb.isEmpty() || !m_canvas) return none;
  QgsProject* proj = QgsProject::instance();
  if (!proj) return none;

  // [int W1] By kind (ka_hgis/reference_kind = dem), so the renamed Copernicus DSM and an
  // older project's untagged "DEM" layer are both found.
  QgsRasterLayer* dem = BasemapDsm::findDem(proj);
  if (!dem || !dem->dataProvider()) return TrenchTerrain::contourAspect(proj, areaWkb, areaCrs);

  QgsGeometry area;
  area.fromWkb(areaWkb);
  if (area.isNull() || area.isEmpty()) return none;
  const QgsRectangle env = area.boundingBox();
  if (env.isEmpty()) return none;

  const QgsCoordinateReferenceSystem workCrs(areaCrs.isEmpty() ? m_workCrs : areaCrs);
  QgsCoordinateTransform toDem(workCrs, dem->crs(), proj);
  std::vector<TrenchGridGenerator::ElevSample> samples;
  const int kSteps = 14;  // 14×14 표본이면 사면 방향은 충분히 안정적이다.
  for (int i = 0; i <= kSteps; ++i) {
    for (int j = 0; j <= kSteps; ++j) {
      const double x = env.xMinimum() + env.width() * i / kSteps;
      const double y = env.yMinimum() + env.height() * j / kSteps;
      if (!area.contains(x, y)) continue;  // 구역 밖 표고는 안 쓴다
      QgsPointXY inDem(x, y);
      try {
        inDem = toDem.transform(QgsPointXY(x, y));
      } catch (const QgsException&) {
        return none;
      }
      bool ok = false;
      const double z = dem->dataProvider()->sample(inDem, 1, &ok);
      if (!ok || std::isnan(z)) continue;
      samples.push_back({x, y, z});
    }
  }
  return TrenchGridGenerator::upslopeAspect(samples);
#else
  Q_UNUSED(areaWkb);
  Q_UNUSED(areaCrs);
  return {};
#endif
}

bool MainWindow::applyTrenchFromDialog() {
#if KA_HGIS_HAS_QGIS
  if (!m_trenchDlg)
    return false;
  const TrenchGridGenerator::Spec sp = m_trenchDlg->spec();
  if (m_trenchDlg->autoFill()) {
    QString areaCrs;
    if (auto* areaVl = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"))) {
      const TrenchGridGenerator::PickedArea pick = trenchFillFromSurveyLayer(areaVl);
      m_trenchDlg->setArea(pick.wkb, pick.areaM2);
      areaCrs = areaVl->crs().authid();
      syncTrenchCoordinateGrid(areaCrs);
      const QString leftover = leftoverSurveyAreaHint(pick);
      if (!leftover.isEmpty())
        statusBar()->showMessage(leftover, 8000);
    }
    // 시굴 10% · 표본 2%는 길이·둑을 프로그램이 맞춘다. 「직접 지정」만 사용자 규격.
    const double target = m_trenchDlg->targetPct();
    std::vector<TrenchGridGenerator::Cell> cells;
    QString noGridReason;  // [pkg B2] F047: why nothing fits (e.g. a self-crossing boundary)
    if (m_trenchDlg->ratioMode()) {
      // [pkg B2] F097: the plan the dialog preview already searched (cached), not a second search.
      const auto plan =
          TrenchPlanCache::ratioPlan(m_trenchDlg->areaWkb(), target, 2.0, sp.azimuthDeg, sp.armLength, sp.armBend,
                                     m_trenchDlg->gridLock(), sp.trenchLength);
      if (plan.cells.empty()) {
        notify(Notice::Warning, QStringLiteral("시굴격자"), plan.error);
        return false;
      }
      cells = plan.cells;
    } else {
      cells = TrenchGridGenerator::buildInArea(sp, m_trenchDlg->areaWkb(), &noGridReason);
    }
    if (cells.empty()) {
      notify(Notice::Warning, QStringLiteral("시굴격자"),
             noGridReason.isEmpty()
                 ? QStringLiteral("현재 규격과 방향으로 구역 안에 격자를 배치하지 못했습니다. 회전이나 규격을 바꿔 다시 적용하세요. 기존 격자는 유지됩니다.")
                 : noGridReason + QStringLiteral(" 기존 격자는 유지됩니다."));
      return false;
    }
    if (!applyTrenchCells(cells, m_trenchDlg->areaM2(), target, areaCrs))
      return false;
    // 깔자마자 마우스로 하나씩 옮길 수 있어야 한다(회전·재배치 뒤도 같다).
    activateTrenchTool(true);
    return true;
  }
  // 수동 모드: 격자가 이미 있으면 중심을 고정한 채 회전·간격만 바꿔 재배치한다.
  auto* vl = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("trial_trench"));
  if (vl && vl->featureCount() > 0) {
    TrenchGridGenerator::Spec centered = sp;
    centered.originX = 0.0;
    centered.originY = 0.0;
    auto cells = TrenchGridGenerator::build(centered);
    if (cells.empty()) {
      notify(Notice::Warning, QStringLiteral("시굴격자"),
             QStringLiteral("격자를 계산하지 못했습니다. 간격과 행·열을 확인하세요."));
      return false;
    }
    double minx = 1e300, miny = 1e300, maxx = -1e300, maxy = -1e300;
    for (const auto& c : cells) {
      for (const auto& pt : c.ring) {
        minx = std::min(minx, pt.first);
        maxx = std::max(maxx, pt.first);
        miny = std::min(miny, pt.second);
        maxy = std::max(maxy, pt.second);
      }
    }
    const QgsPointXY keep = vl->extent().center();
    const double dx = keep.x() - (minx + maxx) * 0.5;
    const double dy = keep.y() - (miny + maxy) * 0.5;
    for (auto& c : cells) {
      for (auto& pt : c.ring) {
        pt.first += dx;
        pt.second += dy;
      }
    }
    return applyTrenchCells(cells, m_trenchDlg->areaM2(), 0.0, vl->crs().authid());
  }
  beginTrenchOriginPick();
  return false;
#else
  return false;
#endif
}

void MainWindow::beginTrenchOriginPick() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas || !m_trenchDlg)
    return;
  g_pendingTrench = m_trenchDlg->spec();
  if (!m_trenchOriginTool) {
    m_trenchOriginTool = new QgsMapToolEmitPoint(m_canvas);
    m_trenchOriginTool->setParent(this);
    connect(m_trenchOriginTool, &QgsMapToolEmitPoint::canvasClicked, this,
            [this](const QgsPointXY& pt, Qt::MouseButton btn) {
              if (btn != Qt::LeftButton)
                return;
              placeTrenchGridAt(pt);
            });
  }
  m_canvas->setMapTool(m_trenchOriginTool);
  statusBar()->showMessage(QStringLiteral("시굴격자: 놓을 자리(원점)를 지도에서 누르세요."), 0);
#endif
}

void MainWindow::placeTrenchGridAt(const QgsPointXY& origin) {
#if KA_HGIS_HAS_QGIS
  g_pendingTrench.originX = origin.x();
  g_pendingTrench.originY = origin.y();
  const auto cells = TrenchGridGenerator::build(g_pendingTrench);
  if (cells.empty()) {
    notify(Notice::Warning, QStringLiteral("시굴격자"),
           QStringLiteral("격자를 계산하지 못했습니다. 간격과 범위를 확인하세요."));
    return;
  }
  applyTrenchCells(cells, m_trenchDlg ? m_trenchDlg->areaM2() : 0.0);
#endif
}

void MainWindow::applyTrenchByRatio(double targetPct) {
#if KA_HGIS_HAS_QGIS
  if (m_surveyPath.isEmpty()) {
    notify(Notice::Info, QStringLiteral("시굴격자"),
           QStringLiteral("먼저 「새 조사」를 만들거나 「열기」로 조사를 여세요."));
    return;
  }
  QgsVectorLayer* areaVl = LayerOps::findByLayerKey(QgsProject::instance(),
                                                    QStringLiteral("survey_area"));
  if (m_layerTree) {
    if (auto* cur = qobject_cast<QgsVectorLayer*>(m_layerTree->currentLayer())) {
      if (LayerOps::layerKeyOf(cur) == QLatin1String("survey_area"))
        areaVl = cur;
    }
  }
  if (!areaVl || areaVl->featureCount() <= 0) {
    notify(Notice::Info, QStringLiteral("시굴격자"),
           QStringLiteral("먼저 조사구역을 그린 뒤 그 레이어에서 우클릭하세요."));
    return;
  }
  TrenchGridGenerator::PickedArea pick = trenchFillFromSurveyLayer(areaVl);
  // 조사구역을 여러 조각으로 그렸으면, 예전에는 마지막 조각에만 깔렸다. 그래서
  // 사용자가 시굴격자를 쓰려고 굳이 「폴리곤 묶기」를 먼저 해야 했다. 이제 묻는다.
  // 기본값을 전체로 바꾸지는 않는다 — 지난 조사의 구역이 남아 있으면 격자가
  // 수백 칸으로 불어나기 때문이다(tests/test_dem_trench.cpp 에 그 이유가 있다).
  if (!pick.usedSelection && pick.totalCount > 1) {
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("시굴격자"));
    box.setText(QStringLiteral("조사구역이 %1곳입니다. 시굴격자를 어디에 놓을까요?").arg(pick.totalCount));
    box.setInformativeText(
        QStringLiteral("「전체」를 고르면 %1곳을 합친 면적으로 비율을 계산해 모든 구역에 시굴격자를 놓습니다.\n"
                       "예전 조사의 구역이 남아 있다면 「마지막 구역만」을 고르세요.")
            .arg(pick.totalCount));
    QPushButton* all = box.addButton(QStringLiteral("전체 %1곳").arg(pick.totalCount),
                                     QMessageBox::AcceptRole);
    QPushButton* last = box.addButton(QStringLiteral("마지막 구역만"), QMessageBox::RejectRole);
    box.addButton(QStringLiteral("취소"), QMessageBox::DestructiveRole);
    box.setDefaultButton(all);
    box.exec();
    if (box.clickedButton() == all)
      pick = trenchFillFromSurveyLayer(areaVl, true);
    else if (box.clickedButton() != last)
      return;  // 취소
  }
  const QString leftover = leftoverSurveyAreaHint(pick);
  if (!leftover.isEmpty())
    statusBar()->showMessage(leftover, 8000);
  if (pick.wkb.isEmpty() || pick.areaM2 <= 0.0) {
    notify(Notice::Warning, QStringLiteral("시굴격자"),
           QStringLiteral("조사구역 면을 찾지 못했습니다."));
    return;
  }
  // [pkg B2] F022/F097: same terrain direction as the dialog (trenches across the contours;
  // no DEM or flat ground = 0°), through the shared plan cache.
  const TrenchGridGenerator::SlopeAspect aspect = terrainAspectForArea(pick.wkb, areaVl->crs().authid());
  const TrenchGridGenerator::RatioFill plan =
      TrenchPlanCache::ratioPlan(pick.wkb, targetPct, 2.0, aspect.valid ? aspect.azimuthDeg : 0.0);
  if (plan.cells.empty()) {
    notify(Notice::Warning, QStringLiteral("시굴격자"), plan.error);
    return;
  }
  applyTrenchCells(plan.cells, pick.areaM2, targetPct, areaVl->crs().authid());
#else
  Q_UNUSED(targetPct);
#endif
}

bool MainWindow::applyTrenchCells(const std::vector<TrenchGridGenerator::Cell>& cells,
                                  double areaM2, double targetPct, const QString& sourceCrs) {
#if KA_HGIS_HAS_QGIS
  if (!TrenchGridGenerator::meetsTarget(cells, areaM2, targetPct)) {
    notify(Notice::Warning, QStringLiteral("시굴격자"),
           QStringLiteral("목표 면적에 맞는 격자를 계산하지 못했습니다. 방향과 규격을 확인해 다시 적용하세요. 기존 격자는 유지됩니다."));
    return false;
  }
  // Auto-fill cells are in the survey layer CRS, even if the canvas has since
  // changed work CRS. Preserve that identity; QGIS transforms them for display.
  const QString auth = !sourceCrs.isEmpty() ? sourceCrs
                       : QgsProject::instance() && QgsProject::instance()->crs().isValid()
                           ? QgsProject::instance()->crs().authid()
                           : QStringLiteral("EPSG:5186");
  // [pkg B2] F003/F156: a grid already on the map is replaced as one Ctrl+Z step (written on
  // 저장); unsaved hand edits still block it and a hand-adjusted grid is replaced only after
  // asking. The first grid of a survey is written and loaded as before.
  const TrenchLayerEdit::PlaceResult placed = TrenchLayerEdit::placeGrid(
      QgsProject::instance(), m_surveyPath, cells, auth,
      [this] { return ensureDomainLayerForEdit(QStringLiteral("trial_trench"), QStringLiteral("시굴격자")); },
      [this](qint64 count) { return KaTrenchDialog::confirmReplaceAdjusted(this, count); });
  if (placed.outcome == TrenchLayerEdit::PlaceOutcome::Kept) {
    statusBar()->showMessage(placed.message, 6000);
    return false;
  }
  if (placed.outcome != TrenchLayerEdit::PlaceOutcome::Placed || !placed.layer) {
    notify(Notice::Warning, QStringLiteral("시굴격자"), placed.message, placed.detail);
    return false;
  }
  QgsProject::instance()->setDirty(true);
  updateUndoRedoActions();
  LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  const double t = TrenchGridGenerator::totalArea(cells);
  QString msg = QStringLiteral("시굴격자 %1개 · 총 %2㎡")
                    .arg(cells.size())
                    .arg(QLocale().toString(t, 'f', 2));
  if (areaM2 > 0.0) {
    const double pct = t / areaM2 * 100.0;
    const QString kind = (targetPct > 0.0 && targetPct < 5.0)
                             ? QStringLiteral("표본 기준 2%")
                             : QStringLiteral("시굴 기준 10%");
    msg += QStringLiteral(" · 조사구역의 %1% (%2)")
               .arg(QLocale().toString(pct, 'f', 1), kind);
  }
  msg += QStringLiteral(" — 격자를 끌어 옮기세요. 우클릭 = 개별 삭제");
  statusBar()->showMessage(msg, 0);
  notify(Notice::Success, QStringLiteral("시굴격자"), msg);
  if (m_trenchDlg) m_trenchDlg->setGridPlaced(true);
  startTrenchGridMove();
  return true;
#else
  Q_UNUSED(cells);
  Q_UNUSED(areaM2);
  Q_UNUSED(targetPct);
  Q_UNUSED(sourceCrs);
  return false;
#endif
}

void MainWindow::startTrenchGridMove() {
  activateTrenchTool(false);
}

void MainWindow::startTrenchGridEdit() {
  activateTrenchTool(true);
}

void MainWindow::activateTrenchTool(bool single) {
#if KA_HGIS_HAS_QGIS
  auto* vl = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("trial_trench"));
  if (!vl || vl->featureCount() <= 0) {
    notify(Notice::Info, QStringLiteral("시굴격자"), QStringLiteral("먼저 시굴격자를 만드세요."));
    return;
  }
  if (!m_trenchMoveTool) {
    m_trenchMoveTool = new KaTrenchMoveTool(m_canvas);
    m_trenchMoveTool->setParent(this);
    connect(m_trenchMoveTool, &KaTrenchMoveTool::statusMessage, this, [this](const QString& t) {
      statusBar()->showMessage(t, 6000);
    });
    // [pkg B2] F003: moves/deletes are unsaved, undoable edits; the title ' *' and Ctrl+Z follow them.
    connect(m_trenchMoveTool, &KaTrenchMoveTool::trenchesEdited, this, [this]() {
      QgsProject::instance()->setDirty(true);
      updateUndoRedoActions();
    });
  }
  m_trenchMoveTool->setLayer(vl);
  double snapM = 0.0;
  if (m_mapGrid && m_mapGrid->isEnabled())
    snapM = m_mapGrid->stepMeters();
  else if (m_gridControls)
    snapM = m_gridControls->stepMeters();
  m_trenchMoveTool->setSnapMeters(snapM);
  m_trenchMoveTool->setGridOverlay(m_mapGrid);
  m_trenchMoveTool->setMode(single ? KaTrenchMoveTool::Mode::Single
                                   : KaTrenchMoveTool::Mode::Whole);
  m_canvas->setMapTool(m_trenchMoveTool);
  m_canvas->setFocus(Qt::OtherFocusReason);
#else
  Q_UNUSED(single);
#endif
}

void MainWindow::applyMapGrid(bool announce) {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas || !m_gridControls)
    return;
  if (!m_mapGrid)
    m_mapGrid = new KaCanvasGridOverlay(m_canvas);
  const KaShellGridSettings s = m_gridControls->settings();
  KaCanvasGridOverlay::Config cfg = m_mapGrid->config();
  cfg.enabled = s.enabled;
  cfg.type = s.geographic ? KaCanvasGridOverlay::Type::GeographicDms
                          : KaCanvasGridOverlay::Type::ProjectedMeters;
  cfg.stepMeters = s.stepMeters;
  cfg.rotationDeg = s.rotationDeg;
  cfg.lineWidth = s.lineWidth;
  cfg.penStyle = s.penStyle;
  if (s.color.isValid())
    cfg.color = s.color;
  // The grid is a canvas item: repainting it does not need a full map render.
  m_mapGrid->setConfig(cfg);
  if (!announce)
    return;
  statusBar()->showMessage(
      cfg.enabled
          ? (cfg.type == KaCanvasGridOverlay::Type::GeographicDms
                 ? QStringLiteral("경위도 격자를 켰습니다.")
                 : QStringLiteral("미터 좌표 격자를 켰습니다."))
          : QStringLiteral("좌표 격자를 껐습니다."),
      4000);
#else
  Q_UNUSED(announce);
#endif
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
#if KA_HGIS_HAS_QGIS
  if (!event) return QMainWindow::eventFilter(watched, event);
  // The first size reaches the sidebar before the 지도 tab is visible; Show has the real one.
  if (watched == m_leftSplit && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
    auto* filesToggle = findChild<QToolButton*>(QStringLiteral("sidebarFilesToggle"));
    auto* filesScroll = findChild<QWidget*>(QStringLiteral("sidebarFilesScroll"));
    // A 파일함 saved shut opens when the tab shows; a short window shuts it again below.
    if (event->type() == QEvent::Show && filesToggle && filesToggle->isChecked())
      KaLayerInformationView::openSidebarFiles(m_leftSplit, filesScroll);
    KaLayerInformationView::protectSidebarList(
        m_leftSplit, m_layerTree, filesToggle, filesScroll,
        findChild<KaLayerInformationPanel*>(QStringLiteral("layerInformationPanel")));
  }
  if (m_mapSplitter && watched == m_mapSplitter && event->type() == QEvent::Resize)
    refreshAlignUi();
  if (m_subToolsMode == QLatin1String("align") &&
      (event->type() == QEvent::Wheel || event->type() == QEvent::Resize)) {
    const bool onAlignView =
        (m_alignImage && (watched == m_alignImage || watched == m_alignImage->viewport())) ||
        (m_alignLeftCanvas &&
         (watched == m_alignLeftCanvas || watched == m_alignLeftCanvas->viewport())) ||
        (m_canvas && (watched == m_canvas || watched == m_canvas->viewport()));
    if (onAlignView)
      QTimer::singleShot(0, this, [this]() { updateAlignOverlay(); });
  }
  if (m_subToolsMode == QLatin1String("align") && event->type() == QEvent::MouseMove) {
    if (auto* me = static_cast<QMouseEvent*>(event)) {
      QWidget* w = qobject_cast<QWidget*>(watched);
      if (w) trackAlignPointer(w->mapToGlobal(me->pos()));
    }
  }

  const bool onCanvas = m_canvas &&
      (watched == m_canvas || watched == m_canvas->viewport());
  if (onCanvas) {
    const QEvent::Type t = event->type();
    if (t == QEvent::Resize || t == QEvent::Show
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
        || t == QEvent::DevicePixelRatioChange
#endif
    ) {
      // 그리는 중에 outputSize·DPR을 바꾸면, 끝난 그림이 바뀐 격자에 안 맞아
      // 전체 화면에서 지도가 빈다. QGIS가 리사이즈 500ms 뒤에 스스로 다시 그린다.
      // 여기서는 그 그림이 끝난 뒤에, 크기가 아직 어긋날 때만 한 번 더 맞춘다.
      scheduleMapDisplayRefresh();
      if (m_subToolsMode == QLatin1String("align"))
        QTimer::singleShot(0, this, [this]() { updateAlignOverlay(); });
    }
    if (t == QEvent::Resize && !m_startupViewApplied)
      QTimer::singleShot(0, this, [this]() { ensureStartupViewReady(); });
  }
  if (onCanvas && event->type() == QEvent::KeyPress) {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (ke && (ke->matches(QKeySequence::Undo) ||
               ((ke->modifiers() & Qt::ControlModifier) && ke->key() == Qt::Key_Z))) {
      undoLastAction();
      return true;
    }
    if (ke && (ke->matches(QKeySequence::Redo) ||
               ((ke->modifiers() & Qt::ControlModifier) && ke->key() == Qt::Key_Y))) {
      redoLastAction();
      return true;
    }
    // A = 도형선택 has one implementation, the window action of KaFeatureSelectTool::installKeyShortcut.
    // With the Korean input method on, the A key can arrive without Qt::Key_A and the shortcut does
    // not match; the physical key (VK_A) is handed to that same action.
    if (ke && ke->modifiers() == Qt::NoModifier && !ke->isAutoRepeat() && ke->key() != Qt::Key_A &&
        ke->nativeVirtualKey() == 0x41) {
      if (auto* key = findChild<QAction*>(QStringLiteral("actSelectShapeKey"))) {
        key->trigger();
        return true;
      }
    }
  }
  if (onCanvas && event->type() == QEvent::MouseButtonDblClick) {
    const bool capturing = m_captureTool && m_canvas->mapTool() == m_captureTool;
    const bool measuring = m_measureTool && m_canvas->mapTool() == m_measureTool;
    if (!capturing && !measuring) {
      auto* me = static_cast<QMouseEvent*>(event);
      if (me && me->button() == Qt::LeftButton) {
        ensureAttributeTool();
        if (m_attributeTool) {
          QgsVectorLayer* layer = nullptr;
          QgsFeature feat;
          if (m_attributeTool->pickAtScreen(me->pos(), &layer, &feat) && layer &&
              !LayerOps::isReferenceLayer(layer) && !LayerOps::isCadastralLayer(layer)) {
            if (m_layerTree) m_layerTree->setCurrentLayer(layer);
            editCurrentLayerStyle();
            return true;
          }
        }
      }
    }
  }

  const bool fromLayerTree = m_layerTree &&
      (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove ||
       event->type() == QEvent::Drop) &&
      (static_cast<QDropEvent*>(event)->source() == m_layerTree ||
       static_cast<QDropEvent*>(event)->source() == m_layerTree->viewport());
  const bool onLayerDrop = !fromLayerTree &&
      ((m_layerTree &&
        (watched == m_layerTree || watched == m_layerTree->viewport())) ||
       (m_canvas && (watched == m_canvas || watched == m_canvas->viewport())));
  if (onLayerDrop) {
    if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
      auto* de = static_cast<QDragEnterEvent*>(event);
      const bool fromBrowser = de->source() == m_fileBrowser ||
                               (m_fileBrowser && de->source() == m_fileBrowser->viewport());
      if ((de->mimeData() && de->mimeData()->hasUrls()) || fromBrowser) {
        de->acceptProposedAction();
        return true;
      }
    }
    if (event->type() == QEvent::Drop) {
      auto* de = static_cast<QDropEvent*>(event);
      bool ok = false;
      if (de->mimeData() && de->mimeData()->hasUrls())
        ok = tryAddDroppedUrls(de->mimeData()->urls());
      if (!ok)
        ok = tryAddDroppedPaths(selectedBrowserFiles());
      if (ok) {
        de->acceptProposedAction();
        return true;
      }
    }
    if (event->type() == QEvent::KeyPress) {
      auto* ke = static_cast<QKeyEvent*>(event);
      const bool measuring = m_measureTool && m_canvas && m_canvas->mapTool() == m_measureTool;
      if (!measuring && ke->key() == Qt::Key_Delete) {
        if (onCanvas) {
          if (m_captureTool && m_canvas->mapTool() == m_captureTool) return false;
          deleteFeaturesOrSelectedReferenceLayers();
        } else removeSelectedLayers();
        return true;
      }
      if (!onCanvas && ke->key() == Qt::Key_F2) {
        renameSelectedLayer();
        return true;
      }
    }
  }
#else
  Q_UNUSED(watched);
  Q_UNUSED(event);
#endif
  return QMainWindow::eventFilter(watched, event);
}


void MainWindow::onLayerTreeRowsMoved() {
  if (m_isOpeningSurvey || !m_canvas) return;
  LayerOps::ensureSatelliteAtBottom(QgsProject::instance());
  LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  LayerOps::refreshCanvasIfIdle(m_canvas);
}

namespace {

void kaPaintColorButton(QPushButton* b, const QColor& c, const QString& suffix) {
  if (!b) return;
  b->setStyleSheet(KaTheme::colorSwatchStyle(c));
  b->setText(c.name(QColor::HexRgb).toUpper() + QStringLiteral("  ·  ") + suffix);
  b->setProperty("kaColor", c);
  b->setCursor(Qt::PointingHandCursor);
}

QPushButton* kaMakeColorButton(QWidget* parent, const QColor& c, const QString& suffix,
                               const QString& pickerTitle, const std::function<void()>& onChanged = {}) {
  auto* b = new QPushButton(parent);
  kaPaintColorButton(b, c.isValid() && c.alpha() > 0 ? c : QColor(22, 163, 74, 160), suffix);
  QObject::connect(b, &QPushButton::clicked, b, [b, suffix, pickerTitle, onChanged]() {
    QColorDialog picker(b->property("kaColor").value<QColor>(), b->window());
    picker.setOption(QColorDialog::DontUseNativeDialog, true);
    picker.setOption(QColorDialog::ShowAlphaChannel, true);
    picker.setWindowTitle(pickerTitle);
    if (picker.exec() != QDialog::Accepted) return;
    const QColor picked = picker.selectedColor();
    if (!picked.isValid()) return;
    kaPaintColorButton(b, picked, suffix);
    if (onChanged) onChanged();
  });
  return b;
}

QWidget* kaWrapLabeled(QWidget* parent, const QString& caption, QWidget* inner) {
  auto* box = new QWidget(parent);
  auto* v = new QVBoxLayout(box);
  v->setContentsMargins(0, 0, 0, 0);
  v->setSpacing(4);
  auto* lab = new QLabel(caption, box);
  v->addWidget(lab);
  v->addWidget(inner);
  return box;
}

}  // namespace

void MainWindow::addUserLayer() {
#if KA_HGIS_HAS_QGIS
  QDialog dlg(this);
  dlg.setObjectName(QStringLiteral("kaStyleDlg"));
  dlg.setWindowTitle(QStringLiteral("레이어 추가"));
  dlg.setWindowFlag(Qt::MSWindowsFixedSizeDialogHint, true);
  auto* root = new QVBoxLayout(&dlg);
  root->setSpacing(8);
  root->setContentsMargins(16, 14, 16, 12);
  root->setSizeConstraint(QLayout::SetFixedSize);
  auto* nameEdit = new QLineEdit(&dlg);
  nameEdit->setPlaceholderText(QStringLiteral("예: 조사구역, 1호 주거지"));
  nameEdit->setMinimumHeight(36);
  root->addWidget(kaWrapLabeled(&dlg, QStringLiteral("이름"), nameEdit));
  auto* crsHost = new QWidget(&dlg);
  auto* crsRow = new QHBoxLayout(crsHost);
  crsRow->setContentsMargins(0, 0, 0, 0);
  crsRow->setSpacing(8);
  auto* btn5186 = new QPushButton(QStringLiteral("5186  중부"), crsHost);
  auto* btn5187 = new QPushButton(QStringLiteral("5187  동부"), crsHost);
  btn5186->setCheckable(true);
  btn5187->setCheckable(true);
  btn5186->setMinimumHeight(40);
  btn5187->setMinimumHeight(40);
  btn5186->setCursor(Qt::PointingHandCursor);
  btn5187->setCursor(Qt::PointingHandCursor);
  const bool use5187 = m_workCrs.contains(QLatin1String("5187"));
  btn5186->setChecked(!use5187);
  btn5187->setChecked(use5187);
  connect(btn5186, &QPushButton::clicked, &dlg, [btn5186, btn5187]() {
    btn5186->setChecked(true);
    btn5187->setChecked(false);
  });
  connect(btn5187, &QPushButton::clicked, &dlg, [btn5186, btn5187]() {
    btn5187->setChecked(true);
    btn5186->setChecked(false);
  });
  crsRow->addWidget(btn5186, 1);
  crsRow->addWidget(btn5187, 1);
  root->addWidget(kaWrapLabeled(&dlg, QStringLiteral("좌표계"), crsHost));
  auto* noFill = new QCheckBox(QStringLiteral("채우기 없음 (외곽선만)"), &dlg);
  root->addWidget(noFill);
  auto* fillBtn = kaMakeColorButton(&dlg, QColor(34, 197, 94, 140),
                                    QStringLiteral("클릭해서 색 고르기"), QStringLiteral("면 색"));
  auto* fillBox = kaWrapLabeled(&dlg, QStringLiteral("면 색"), fillBtn);
  root->addWidget(fillBox);
  auto* strokeBtn = kaMakeColorButton(&dlg, QColor(21, 128, 61),
                                      QStringLiteral("클릭해서 색 고르기"), QStringLiteral("외곽선 색"));
  root->addWidget(kaWrapLabeled(&dlg, QStringLiteral("외곽선 색"), strokeBtn));
  connect(noFill, &QCheckBox::toggled, &dlg, [&dlg, fillBox](bool on) {
    fillBox->setVisible(!on);
    dlg.adjustSize();
  });
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("추가"));
  buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("취소"));
  root->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted) return;
  const QString title = nameEdit->text().trimmed();
  if (title.isEmpty()) {
    QMessageBox::information(this, QStringLiteral("레이어 추가"), QStringLiteral("이름을 입력하세요."));
    return;
  }
  if (m_surveyPath.isEmpty()) {
    const auto ans = QMessageBox::question(this, QStringLiteral("레이어"),
                                           QStringLiteral("먼저 새 조사가 필요합니다. 지금 만들까요?"));
    if (ans != QMessageBox::Yes) return;
    newSurvey();
    if (m_surveyPath.isEmpty()) return;
  }
  QString err;
  const QString crsId = btn5187->isChecked() ? QStringLiteral("EPSG:5187")
                                             : QStringLiteral("EPSG:5186");
  auto* vl = LayerOps::createUserPolygonLayer(QgsProject::instance(), m_surveyPath, title,
                                              crsId, &err);
  if (!vl) {
    notify(Notice::Warning, QStringLiteral("레이어 추가"),
           QStringLiteral("레이어를 만들지 못했습니다."), err);
    return;
  }
  LayerOps::applySimpleVectorStyle(vl, fillBtn->property("kaColor").value<QColor>(),
                                   strokeBtn->property("kaColor").value<QColor>(),
                                   1.2, 3.5, noFill->isChecked(), false);
  LayerOps::applyAreaM2Labels(vl);
  if (m_layerTree) m_layerTree->setCurrentLayer(vl);
  if (m_canvas) {
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
    m_canvas->refresh();
  }
  statusBar()->showMessage(QStringLiteral("레이어 추가: %1").arg(title), 6000);
#else
  QMessageBox::information(this, QStringLiteral("레이어"), QStringLiteral("QGIS 빌드 필요"));
#endif
}

void MainWindow::importControlCsv() {
  const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("CSV 기준점"), QString(),
                                                    QStringLiteral("CSV (*.csv)"));
  if (path.isEmpty()) return;
#if KA_HGIS_HAS_QGIS
  auto* layer = ensureDomainLayerForEdit(QStringLiteral("control_points"), QStringLiteral("GPS기준점"));
  if (!layer) return;
  QString err;
  const LayerOps::ControlCsvPreview preview =
      LayerOps::previewControlPointsCsv(layer, path, QgsProject::instance(), &err);
  if (!preview.ok) {
    KaUserError::warn(this, {
        QStringLiteral("CSV"),
        QStringLiteral("기준점 CSV를 읽지 못했습니다."),
        err.isEmpty() ? QStringLiteral("파일 형식이나 좌표 칸을 확인하지 못했습니다.") : err,
        QStringLiteral("CSV에 X·Y(또는 경도·위도) 칸이 있는지 확인한 뒤 다시 가져오세요."),
    });
    return;
  }
  QMessageBox box(this);
  KaControlCsvBox::dress(box, preview.swapSuggested, preview.count, preview.summary);
  auto* keep = qobject_cast<QPushButton*>(box.addButton(QStringLiteral("이대로 가져오기"), QMessageBox::AcceptRole));
  auto* swap = qobject_cast<QPushButton*>(box.addButton(QStringLiteral("X·Y 바꿔서 가져오기"), QMessageBox::ActionRole));
  box.addButton(QStringLiteral("취소"), QMessageBox::RejectRole);
  box.setDefaultButton(preview.swapSuggested ? swap : keep);
  box.exec();
  if (box.clickedButton() != keep && box.clickedButton() != swap) return;
  const int n = LayerOps::importControlPointsCsv(layer, path, &err, box.clickedButton() == swap);
  if (n < 0) {
    KaUserError::warn(this, {QStringLiteral("CSV"), QStringLiteral("기준점 CSV를 불러오지 못했습니다"), err, {}});
    return;
  }
  m_stubGcp = int(layer->featureCount());
  m_stubHasMeta = true;
  if (m_canvas) m_canvas->refresh();
  statusBar()->showMessage(QStringLiteral("CSV 기준점 %1개 저장 (합 %2)").arg(n).arg(m_stubGcp), 6000);
#else
  Q_UNUSED(path);
  statusBar()->showMessage(QStringLiteral("스텁: CSV"), 3000);
#endif
}

#if KA_HGIS_HAS_QGIS
static bool layerSitsOnWorkMap(QgsMapLayer* layer, const QString& workCrs) {
  if (!layer) return false;
  const QgsRectangle e = layer->extent();
  if (e.isEmpty() || !e.isFinite()) return false;
  const QgsRectangle kr = LayerOps::koreaExtentForCrs(workCrs);
  if (kr.isEmpty()) return true;
  return kr.intersects(e);
}
#endif


void MainWindow::openVectorLayer() {
#if KA_HGIS_HAS_QGIS
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("SHP/벡터·도면 추가"), QString(),
      QStringLiteral("벡터·도면 (*.shp *.gpkg *.geojson *.dxf *.dwg)"));
  if (path.isEmpty()) return;
  if (GeorefService::isCadPath(path)) { addVectorFromPath(path); return; }
  LayerOps::prepareShapefileEncoding(path);
  const QString title = QFileInfo(path).completeBaseName();
  auto* layer = new QgsVectorLayer(path, title, QStringLiteral("ogr"));
  if (!layer->isValid()) {
    KaUserError::warn(this, {QStringLiteral("오류"), QStringLiteral("파일을 열지 못했습니다"), layer->error().message(), {}});
    delete layer;
    return;
  }
  for (const QgsField& f : layer->fields()) {
    if (f.name().contains(QChar(0xFFFD))) {
      LayerOps::setShapefileEncoding(layer, QStringLiteral("CP949"));
      break;
    }
  }
  LayerOps::markSurveyLayer(layer, QStringLiteral("user:%1").arg(title));
  LayerOps::applySimpleVectorStyle(layer, QColor(0, 0, 0, 0), QColor(0, 0, 0), 0.2, 3.5, true,
                                   false);
  const QString nameField = LayerOps::detectNameField(layer);
  if (!nameField.isEmpty()) {
    LayerOps::applyNameAttributeLabels(layer, nameField, LayerOps::kDefaultLabelSizePt, false);  // [pkg E1] F145
  }
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, m_workCrs);
  if (!layer->crs().isValid() && QgsProject::instance() &&
      QgsProject::instance()->crs().isValid()) {
    layer->setCrs(QgsProject::instance()->crs());
    statusBar()->showMessage(
        QStringLiteral("벡터에 좌표계가 없어 작업 좌표계(%1)를 붙였습니다.").arg(m_workCrs),
        8000);
  }
  LayerOps::applyLegendCrsLabel(layer);
  QgsProject::instance()->addMapLayer(layer, true);
  LayerOps::pruneEmptyLegendGroups(QgsProject::instance());
  if (m_canvas) {
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
    LayerOps::zoomToLayerMax(m_canvas, layer);
  }
  statusBar()->showMessage(QStringLiteral("지도를 올렸습니다: %1").arg(title), 6000);
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("벡터 로드 시뮬레이션"));
#endif
}

// 흙토람(soil.rda.go.kr) 토양도 신청으로 받은 SHP를 참조 지도로 불러온다.
// VWorld WMS 토양 레이어는 서버 오류가 잦고 분포지형이 없어 파일 방식을 쓴다.
void MainWindow::importSoilShapefile() {
#if KA_HGIS_HAS_QGIS
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("토양도 SHP 불러오기"), QString(),
      QStringLiteral("Shapefile (*.shp)"));
  if (path.isEmpty()) return;

  // 필드 목록과 파일에 기록된 좌표계를 미리 읽어 선택지를 만든다.
  QStringList fieldNames;
  QString fileCrsAuth;
  {
    QgsVectorLayer probe(path, QStringLiteral("probe"), QStringLiteral("ogr"));
    if (!probe.isValid()) {
      notify(Notice::Critical, QStringLiteral("토양도"),
             QStringLiteral("SHP를 열 수 없습니다: %1").arg(probe.error().message()));
      return;
    }
    const QgsFields flds = probe.fields();
    for (int i = 0; i < flds.count(); ++i) {
      const QgsField f = flds.at(i);
      const auto t = static_cast<QMetaType::Type>(f.type());
      if (t == QMetaType::QString || t == QMetaType::Int || t == QMetaType::LongLong)
        fieldNames.append(f.name());
    }
    if (probe.crs().isValid()) fileCrsAuth = probe.crs().authid();
  }

  QDialog dlg(this);
  dlg.setWindowTitle(QStringLiteral("토양도 불러오기 설정"));
  auto* form = new QFormLayout(&dlg);

  auto* crsBox = new QComboBox(&dlg);
  if (!fileCrsAuth.isEmpty())
    crsBox->addItem(QStringLiteral("파일에 기록된 좌표계 사용 — %1").arg(fileCrsAuth), QString());
  crsBox->addItem(QStringLiteral("EPSG:2097 — 중부원점(Bessel) · 흙토람 고시 좌표계"),
                  QStringLiteral("EPSG:2097"));
  crsBox->addItem(QStringLiteral("EPSG:5174 — 중부원점(Bessel, 10.405″ 보정)"),
                  QStringLiteral("EPSG:5174"));
  crsBox->addItem(QStringLiteral("EPSG:5186 — 중부원점(GRS80)"), QStringLiteral("EPSG:5186"));
  crsBox->addItem(QStringLiteral("EPSG:5187 — 동부원점(GRS80)"), QStringLiteral("EPSG:5187"));
  crsBox->addItem(QStringLiteral("EPSG:5179 — UTM-K"), QStringLiteral("EPSG:5179"));
  crsBox->addItem(QStringLiteral("EPSG:4326 — 경위도(WGS84)"), QStringLiteral("EPSG:4326"));
  crsBox->setCurrentIndex(0);
  form->addRow(QStringLiteral("좌표계"), crsBox);

  auto* fieldBox = new QComboBox(&dlg);
  fieldBox->addItem(QStringLiteral("(단색 — 구분 없음)"), QString());
  for (const QString& n : fieldNames)
    fieldBox->addItem(n, n);
  // 분포지형·토양부호 계열 필드가 있으면 미리 고른다.
  static const QRegularExpression kSoilFieldRx(
      QStringLiteral("(분포|지형|토양|tpgrp|topo|dist|soil|sltp|sym)"),
      QRegularExpression::CaseInsensitiveOption);
  for (int i = 1; i < fieldBox->count(); ++i) {
    if (fieldBox->itemText(i).contains(kSoilFieldRx)) {
      fieldBox->setCurrentIndex(i);
      break;
    }
  }
  form->addRow(QStringLiteral("색 구분 필드"), fieldBox);

  auto* note = new QLabel(
      QStringLiteral("흙토람 → 토양도 신청에서 무료로 받은 SHP를 그대로 불러옵니다.\n"
                     "지도가 엉뚱한 위치에 뜨면 좌표계를 EPSG:2097 ↔ 5174로 바꿔 다시 불러오세요.\n"
                     "불러온 뒤에는 작업 좌표계(%1)로 자동 재투영되어 지적·위성과 겹쳐 보입니다.")
          .arg(m_workCrs),
      &dlg);
  note->setWordWrap(true);
  form->addRow(note);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("불러오기"));
  buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("취소"));
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  form->addRow(buttons);
  if (dlg.exec() != QDialog::Accepted) return;

  QString err;
  QgsVectorLayer* layer = LayerOps::addSoilShapefile(
      QgsProject::instance(), m_canvas, path, crsBox->currentData().toString(),
      fieldBox->currentData().toString(), &err);
  if (!layer) {
    notify(Notice::Critical, QStringLiteral("토양도"),
           err.isEmpty() ? QStringLiteral("토양도를 불러오지 못했습니다.") : err);
    return;
  }
  if (m_layerTree) m_layerTree->setCurrentLayer(layer);
  const QString fieldTxt = fieldBox->currentData().toString().isEmpty()
                               ? QStringLiteral("단색")
                               : fieldBox->currentData().toString();
  const QString msg = QStringLiteral("%1 · %2 · 색 구분: %3 — 참조 지도 그룹")
                          .arg(layer->name(), layer->crs().authid(), fieldTxt);
  statusBar()->showMessage(QStringLiteral("토양도를 올렸습니다: %1").arg(msg), 8000);
  notify(Notice::Success, QStringLiteral("토양도"), msg);
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("토양도 로드 시뮬레이션"));
#endif
}

#if KA_HGIS_HAS_QGIS
#endif


// 레이어 점호. 상태가 직전과 다르면 무엇이 어떻게 달라졌는지 로그에 남기고,
// 원본이 잠깐 끊겨 무효가 된 것은 다시 연다.

// 제목 뒤 " *" 하나로 "아직 저장 안 됨"을 보여 준다. 제목을 세우는 곳이 아홉 군데라
// 별도 필드를 두지 않고 현재 제목에서 표식만 떼었다 붙인다.

// 캔버스가 "지금 그리는 중"이라고 잡고 있는 레이어와 축척을 남긴다. 헤드리스 렌더로는
// 재현이 안 되는(레이어·좌표계·확대한계 모두 정상인) 화면 전용 현상을 추적하기 위한 것.
// 목록이 직전과 같으면 아무것도 쓰지 않는다 — 팬·줌마다 로그가 폭주하지 않도록.

void MainWindow::searchLocation(const QString& query, bool parcel) {
  if (!m_locator) return;
  const QString q = query.trimmed();
  if (q.isEmpty()) {
    statusBar()->showMessage(QStringLiteral("주소·지번·지역·상호를 입력하세요"), 4000);
    return;
  }
  // 검색 칸이 없어졌으므로 중복 실행은 플래그로 막는다.
  if (m_locationSearchBusy) return;
  m_locationSearchBusy = true;
  statusBar()->showMessage(QStringLiteral("위치 검색 중… %1").arg(q), 0);
  if (m_searchProgress) { m_searchProgress->hide(); m_searchProgress->deleteLater(); }
  // 검색은 내려받기가 아니다. 무엇을 찾는지 말하는 전용 문구를 쓴다(「위치 자료 자료」 겹침 없음).
  m_searchProgress = KaShellUi::createSearchProgress(this, q);
  connect(m_searchProgress, &QProgressDialog::canceled, this, [this]() {
    m_locator->cancel();
    m_locationSearchBusy = false;
    if (m_searchProgress) { m_searchProgress->deleteLater(); m_searchProgress = nullptr; }
    statusBar()->showMessage(QStringLiteral("위치 검색을 취소했습니다."), 4000);
  });
  m_searchProgress->show();
  if (parcel) m_locator->searchParcel(q);
  else m_locator->search(q);
}

void MainWindow::onLocationFailed(const QString& message) {
  if (m_searchProgress) { m_searchProgress->hide(); m_searchProgress->deleteLater(); m_searchProgress = nullptr; }
  m_locationSearchBusy = false;
  statusBar()->showMessage(message, 8000);
  QMessageBox::information(this, QStringLiteral("위치 검색"), message);
}

void MainWindow::onLocationResults(const QVector<LocationHit>& hits) {
  if (m_searchProgress) { m_searchProgress->hide(); m_searchProgress->deleteLater(); m_searchProgress = nullptr; }
  m_locationSearchBusy = false;
  if (hits.isEmpty()) {
    onLocationFailed(QStringLiteral("검색 결과 없음"));
    return;
  }
  if (hits.size() == 1) {
    zoomToLocation(hits.first());
    return;
  }
  QStringList labels;
  for (const LocationHit& h : hits) {
    QString line = h.title;
    if (!h.detail.isEmpty()) line += QStringLiteral("  —  ") + h.detail;
    labels << line;
  }
  bool ok = false;
  const QString pick = QInputDialog::getItem(
      this, QStringLiteral("위치 선택"),
      QStringLiteral("검색 결과 %1건 — 이동할 위치를 선택하세요").arg(hits.size()),
      labels, 0, false, &ok);
  if (!ok) return;
  const int idx = labels.indexOf(pick);
  if (idx >= 0 && idx < hits.size())
    zoomToLocation(hits.at(idx));
}

void MainWindow::zoomToLocation(const LocationHit& hit) {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  double lon = hit.lon;
  double lat = hit.lat;
  if (qAbs(lon) <= 90.0 && qAbs(lat) > 90.0)
    std::swap(lon, lat);
  if (lon < 120.0 || lon > 135.0 || lat < 30.0 || lat > 45.0) {
    statusBar()->showMessage(
        QStringLiteral("위치 좌표가 한국 범위 밖입니다 (lon=%1 lat=%2)").arg(lon).arg(lat), 8000);
  }

  const QgsCoordinateReferenceSystem wgs(QStringLiteral("EPSG:4326"));
  const QgsCoordinateReferenceSystem dest =
      m_workCrs.isEmpty() ? QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186"))
                          : QgsCoordinateReferenceSystem(m_workCrs);
  try {
    QgsCoordinateTransform xf(wgs, dest, QgsProject::instance()
                                             ? QgsProject::instance()->transformContext()
                                             : QgsCoordinateTransformContext());
    xf.setBallparkTransformsAreAppropriate(true);
    LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, dest.authid());

    QgsPointXY p = xf.transform(QgsPointXY(lon, lat));
    m_startupViewApplied = true; // Explicit navigation wins over queued initial framing.
    showMapWorkspace();
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
    const bool localParcel = !hit.pnu.isEmpty()
        && CadastralImport::focusParcel(QgsProject::instance(), m_canvas, hit.pnu, &p);
    const bool useBounds = hit.hasBbox && hit.west != 0.0 && hit.east != 0.0;
    if (localParcel) {
      // focusParcel has already framed the actual cadastral geometry.
    } else if (useBounds) {
      QgsRectangle r(hit.west, hit.south, hit.east, hit.north);
      r = xf.transformBoundingBox(r);
      r.scale(1.2);
      m_canvas->setExtent(r);
    } else {
      const double pad = dest.authid().contains(QLatin1String("4326")) ? 0.004 : 400.0;
      m_canvas->setExtent(QgsRectangle(p.x() - pad, p.y() - pad, p.x() + pad, p.y() + pad));
    }
    if (!localParcel && !useBounds && m_canvas->scale() > 8000.0)
      m_canvas->zoomScale(3000.0, true);
    LayerOps::clampCanvasToKorea(m_canvas);
    LayerOps::refreshCanvasIfIdle(m_canvas);
    statusBar()->showMessage(
        QStringLiteral("이동: %1  (lon %2, lat %3 → %4)")
            .arg(hit.title)
            .arg(lon, 0, 'f', 5)
            .arg(lat, 0, 'f', 5)
            .arg(dest.authid()),
        10000);
  } catch (const QgsCsException& e) {
    // [pkg B1] F077: the exception text goes only under 자세히 (QgsException::what() is a QString).
    KaEditErrors::show(this, {QStringLiteral("위치"), QStringLiteral("찾은 위치로 지도를 옮기지 못했습니다."),
                              QStringLiteral("찾은 좌표를 지금 작업 좌표계로 바꾸지 못했습니다."),
                              QStringLiteral("작업 좌표계(5186/5187)를 확인한 뒤 다시 검색하세요.")},
                       e.what());
  } catch (...) {
    KA_LOG_EXCEPT();
    KaUserError::warn(this, {QStringLiteral("위치"), QStringLiteral("위치를 좌표로 바꾸지 못했습니다"), QStringLiteral("좌표 변환에 실패했습니다."), QStringLiteral("작업 좌표계(5186/5187)를 확인한 뒤 다시 검색하세요.")});
  }
#else
  Q_UNUSED(hit);
#endif
}

bool MainWindow::addRasterFromPath(const QString& path) {
#if KA_HGIS_HAS_QGIS
  const QString title = QFileInfo(path).completeBaseName();
  auto* rl = new QgsRasterLayer(path, title, QStringLiteral("gdal"));
  if (!rl || !rl->isValid()) {
    delete rl;
    return false;
  }
  const bool unrefImage = GeorefService::isImagePath(path)
                          && GeorefService::looksUnreferencedRaster(rl);
  if (unrefImage)
    LayerOps::setAlignPending(rl, true);
  else if (!rl->crs().isValid() && QgsProject::instance() && QgsProject::instance()->crs().isValid()) {
    rl->setCrs(QgsProject::instance()->crs());
    statusBar()->showMessage(
        QStringLiteral("GeoTIFF에 좌표계가 없어 작업 좌표계(%1)로 올렸습니다.").arg(m_workCrs), 8000);
  }
  const bool geotiff = path.toLower().endsWith(QLatin1String(".tif"))
                       || path.toLower().endsWith(QLatin1String(".tiff"))
                       || path.toLower().endsWith(QLatin1String(".gtiff"));
  LayerOps::markReferenceLayer(rl);
  rl->setCustomProperty(QStringLiteral("ka_hgis/imported_reference"), true);
  LayerOps::applyLegendCrsLabel(rl);
  QgsProject::instance()->addMapLayer(rl, true);
  if (geotiff)
    LayerOps::knockOutRasterPaper(rl);
  if (m_layerTree) m_layerTree->setCurrentLayer(rl);
  if (unrefImage) {
    if (QgsLayerTreeLayer* n = QgsProject::instance()->layerTreeRoot()->findLayer(rl->id()))
      n->setItemVisibilityChecked(false);
  }
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, m_workCrs);
  LayerOps::pruneEmptyLegendGroups(QgsProject::instance());
  refreshLayerEmptyState();
  if (m_canvas)
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  if (unrefImage) {
    statusBar()->showMessage(
        QStringLiteral("좌표 없는 그림입니다. 더보기 → 맞추기로 지적 위에 올리세요."), 10000);
    return true;
  }
  if (m_canvas && geotiff && layerSitsOnWorkMap(rl, m_workCrs)) {
    if (!LayerOps::zoomToLayerMax(m_canvas, rl))
      statusBar()->showMessage(QStringLiteral("그림은 올렸지만 범위를 잡지 못했습니다."), 8000);
  }
  const QString crs = rl->crs().isValid() ? rl->crs().authid() : m_workCrs;
  statusBar()->showMessage(
      QStringLiteral("그림 추가: %1 · %2 · 화면 %3").arg(title, crs, m_workCrs), 10000);
  return true;
#else
  Q_UNUSED(path);
  return false;
#endif
}

bool MainWindow::addVectorFromPath(const QString& path, const QString& cadAuthId) {
#if KA_HGIS_HAS_QGIS
  // 도면(DXF·DWG)은 좌표계를 알아내 변환본으로 올린다. 맞추기·파일함·끌어놓기·벡터 불러오기가 모두 여기로 온다.
  if (GeorefService::isCadPath(path))
    return KaCadImport::run({this, m_canvas, m_messageBar, m_surveyPath, m_workCrs,
                             [this](QgsMapLayer* layer) { startAlignSession(layer); },
                             [this](const QString& p, const QString& a) { return addVectorFromPath(p, a); }},
                            path, cadAuthId);
  LayerOps::prepareShapefileEncoding(path);
  const QString baseTitle = QFileInfo(path).completeBaseName();
  QList<QgsVectorLayer*> added;
  const QList<QgsProviderSublayerDetails> subs =
      QgsProviderRegistry::instance()->querySublayers(path);
  auto takeLayer = [&](QgsVectorLayer* layer, const QString& title) {
    if (!layer || !layer->isValid()) {
      delete layer;
      return;
    }
    // 한글 필드명 깨짐(\uFFFD) 자동 감지 및 CP949 복구
    bool hasGarbled = false;
    for (const QgsField& f : layer->fields()) {
      if (f.name().contains(QChar(0xFFFD))) {
        hasGarbled = true;
        break;
      }
    }
    if (hasGarbled) {
      LayerOps::setShapefileEncoding(layer, QStringLiteral("CP949"));
    }
    layer->setName(title);
    LayerOps::markSurveyLayer(layer, QStringLiteral("user:%1").arg(title));
    LayerOps::applySimpleVectorStyle(layer, QColor(0, 0, 0, 0), QColor(0, 0, 0), 0.2, 3.5, true,
                                     false);
    // SHP 등 벡터 레이어 추가 시 명칭 속성 기본 크기 자동 라벨링
    const QString nameField = LayerOps::detectNameField(layer);
    if (!nameField.isEmpty()) {
      LayerOps::applyNameAttributeLabels(layer, nameField, LayerOps::kDefaultLabelSizePt, false);  // [pkg E1] F145
    }
    LayerOps::applyLegendCrsLabel(layer);
    QgsProject::instance()->addMapLayer(layer, true);
    added.append(layer);
  };
  if (path.endsWith(QLatin1String(".gpkg"), Qt::CaseInsensitive)) {
    bool isSurveyGpkg = false;
    for (const QgsProviderSublayerDetails& d : subs) {
      if (d.name() == QLatin1String("survey_area") || d.name() == QLatin1String("feature_poly") ||
          d.name() == QLatin1String("trial_trench") || d.name() == QLatin1String("control_points")) {
        isSurveyGpkg = true;
        break;
      }
    }
    if (isSurveyGpkg) {
      return openSurveyGpkg(path);
    }
  }
  if (!subs.isEmpty()) {
    for (const QgsProviderSublayerDetails& d : subs) {
      if (d.type() != Qgis::LayerType::Vector) continue;
      QgsProviderSublayerDetails::LayerOptions opt(QgsProject::instance()->transformContext());
      auto* ml = d.toLayer(opt);
      auto* vl = qobject_cast<QgsVectorLayer*>(ml);
      // GPKG 등 멀티 테이블 파일에서 피처가 0개인 빈 테이블은 레전드를 어지럽히지 않도록 추가하지 않는다.
      if (subs.size() > 1 && vl && vl->isValid() && vl->featureCount() == 0) {
        delete vl;
        continue;
      }
      takeLayer(vl, d.name().isEmpty() ? baseTitle : d.name());
    }
  } else {
    takeLayer(new QgsVectorLayer(path, baseTitle, QStringLiteral("ogr")), baseTitle);
  }
  if (added.isEmpty()) return false;
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, m_workCrs);
  LayerOps::pruneEmptyLegendGroups(QgsProject::instance());
  if (m_layerTree && !added.isEmpty()) m_layerTree->setCurrentLayer(added.first());
  if (m_canvas) {
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
    QgsVectorLayer* zoomLayer = added.first();
    for (QgsVectorLayer* vl : added) {
      if (vl && vl->featureCount() > 0) {
        zoomLayer = vl;
        break;
      }
    }
    if (!layerSitsOnWorkMap(zoomLayer, m_workCrs)) {
      statusBar()->showMessage(
          QStringLiteral("좌표가 없는 도면입니다. 더보기 → 맞추기로 지적 위에 올리세요."), 10000);
    } else if (!LayerOps::zoomToLayerMax(m_canvas, zoomLayer)) {
      statusBar()->showMessage(
          QStringLiteral("레이어는 추가됐지만 도형이 없습니다: %1").arg(zoomLayer->name()), 8000);
    }
  }
  qint64 feats = 0;
  for (QgsVectorLayer* vl : added)
    feats += vl ? vl->featureCount() : 0;
  statusBar()->showMessage(
      QStringLiteral("레이어 %1개 추가 · 도형 %2개 · 화면 %3")
          .arg(added.size())
          .arg(feats)
          .arg(m_workCrs),
      10000);
  return true;
#else
  Q_UNUSED(path);
  Q_UNUSED(cadAuthId);
  return false;
#endif
}

void MainWindow::showAbout() {
  QMessageBox::about(this, QStringLiteral("정보"),
      QStringLiteral("Strata · 필드고고학 GIS  v") + QLatin1String(KA_HGIS_VERSION) +
      QStringLiteral("\n") + KaSessionLog::buildLabel() +
      QStringLiteral("\n동국문화재연구원 · 만든이: 권영인 · 조유량 · 박종환\n\n"
                     "QGIS를 포크하지 않고 qgis_core / qgis_gui를 링크합니다.\n"
                     "작업 좌표계: EPSG:5186/5187 · 제출: EPSG:5179\n\n"
                     "저작권·라이선스\n") + KaStartupSplash::attributionText() +
      QStringLiteral("\n선택한 지도에 따라 OpenStreetMap·CARTO·OpenTopoMap·NASA GIBS·"
                     "Copernicus DEM·Google 자료를 사용합니다. 각 제공처의 표시·이용조건을 따릅니다.\n\n"
                     "DWG 변환: GNU LibreDWG 0.14 (GPLv3 이상, 별도 프로그램 tools/libredwg)\n"
                     "본 소프트웨어는 GNU GPL v2 이상으로 배포됩니다.\n"
                     "자세한 의존 고지는 앱 폴더의 THIRD_PARTY_NOTICES.md를 봅니다.\n\n") +
      KaCrashGuard::dumpHint());
}

bool MainWindow::configureHeritageAccount() {
  KaHeritageAccountDialog dialog(this);
  return dialog.exec() == QDialog::Accepted && HeritageIntranetSettings::hasCredentials();
}

// 조사구역이 속한 시/군의 국가유산 자료를 받아 온다.
// 버튼 한 번으로 끝나야 하고, 묻는 것은 시/군 판정 확인 하나뿐이다.
void MainWindow::fetchNearbyHeritage() {
  // 막히면 조용히 끝내지 않는다. 왜 못 하는지 창으로 말한다.
  auto stopWith = [this](const QString& why) {
    KaUserError::warn(this, {
        QStringLiteral("주변유적 받기"),
        QStringLiteral("주변유적을 받기 전에 막혔습니다."),
        why,
        QStringLiteral("안내를 확인한 뒤 조건이 되면 다시 「주변유적」을 누르세요."),
    });
  };

  auto* areaVl = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"));
  if (!areaVl) {
    stopWith(QStringLiteral("조사구역 레이어가 없습니다. 조사구역을 먼저 그리세요."));
    return;
  }
  const TrenchGridGenerator::PickedArea pick = trenchFillFromSurveyLayer(areaVl);
  QgsGeometry area;
  if (!pick.wkb.isEmpty()) area.fromWkb(pick.wkb);
  if (area.isNull() || area.isEmpty()) {
    stopWith(QStringLiteral("조사구역이 비어 있습니다. 조사구역을 먼저 그리세요.\n"
                            "그린 구역으로 어느 시·군인지 판정합니다."));
    return;
  }
  if (m_surveyPath.isEmpty()) {
    stopWith(QStringLiteral("조사를 먼저 열거나 저장하세요.\n"
                            "받은 자료는 그 조사폴더 안에만 둡니다."));
    return;
  }

  // 창을 먼저 띄운다. 판정이 늦어도 사용자가 아무것도 못 보는 일이 없게 한다.
  ensureHeritageBrowser();
  m_heritageBrowser->show();
  m_heritageBrowser->raise();
  m_heritageBrowser->activateWindow();
  m_heritageBrowser->showWaiting(QStringLiteral("조사구역이 속한 시·군을 찾는 중입니다…"));

  if (!m_heritageResolver) {
    m_heritageResolver = new HeritageRegionResolver(this);
    connect(m_heritageResolver, &HeritageRegionResolver::failed, this, [this](const QString& why) {
      // A previous lookup can still be pending (e.g. a repeated click). Its late
      // result must not restart the operation after the user selects a region.
      m_heritageResolver->cancel();
      if (m_heritageBrowser) m_heritageBrowser->showWaiting(why);
      openHeritageBrowserFor({}, why);
    });
    connect(m_heritageResolver, &HeritageRegionResolver::resolved, this,
            [this](const HeritageRegion& region) { openHeritageBrowserFor(region); });
  }
  m_heritageResolver->resolve(area, areaVl->crs(), QgsProject::instance());
}

// 창과 연결을 한 번만 만든다. 판정 전에도 창을 띄우기 위해 따로 뺐다.
void MainWindow::ensureHeritageBrowser() {
  if (m_heritageBrowser) return;
  m_heritageBrowser = new KaHeritageBrowser(this);
  connect(m_heritageBrowser, &KaHeritageBrowser::failed, this, [this](const QString& why) {
    notify(Notice::Warning, QStringLiteral("주변유적 받기"), why);
  });
  connect(m_heritageBrowser, &KaHeritageBrowser::stageChanged, this,
          [bar = QPointer<QStatusBar>(statusBar())](HeritageStage, const QString& message) {
            if (bar) bar->showMessage(message, 6000);
          });
  // 한 종류를 받을 때마다 바로 지도에 올린다. 여섯 종을 다 기다리게 하지 않는다.
  connect(m_heritageBrowser, &KaHeritageBrowser::datasetReady, this,
          [this](HeritageDataset dataset, const QStringList& files) {
            const auto result = importHeritageDataset(dataset, files);
            if (!result.ok())
              m_heritageBrowser->rejectDataset(result.error, result.retryableDownload);
          });
  connect(m_heritageBrowser, &KaHeritageBrowser::allFinished, this, [this]() {
    m_heritageBrowser->hide();
    statusBar()->showMessage(QStringLiteral("주변유적 자료 처리를 마쳤습니다. 받은 자료는 「참조 지도」에 있습니다."), 10000);
  });
  // 서약서 동의는 영수증으로 남긴다. 조용히 지나가지 않는다.
  connect(m_heritageBrowser, &KaHeritageBrowser::agreementAccepted, this,
          [this](const QDateTime& when, const QString& terms) {
            saveHeritageAgreementReceipt(when, terms);
          });
}

// 판정 결과를 확인받고 받기를 시작한다. 시·군 경계에 걸친 조사가 흔해서 이 한 번은 묻는다.
void MainWindow::openHeritageBrowserFor(const HeritageRegion& region, const QString& reason) {
  ensureHeritageBrowser();
  KaHeritageRegionDialog choice(region, reason, m_heritageBrowser);  // F120: 5 km neighbours listed
  if (choice.exec() != QDialog::Accepted) {
    m_heritageBrowser->showWaiting(QStringLiteral("취소했습니다."));
    return;
  }
  const QString sido = choice.sido();
  const QString city = choice.city();
  if (!HeritageIntranetSettings::hasCredentials() && !configureHeritageAccount()) {
    m_heritageBrowser->showWaiting(QStringLiteral(
        "받기를 시작하지 않았습니다. 국가유산 인트라넷 아이디·비밀번호를 저장한 뒤 다시 눌러 주세요."));
    return;
  }

  // **OneDrive 를 거치지 않는다.** 바탕 화면이 동기화 폴더라 내려받는 중에 가로채여
  // 0바이트로 보이는 일이 있었다(2026-09-12). 받는 자리는 로컬로 고정한다.
  // 조사폴더로 옮기는 것은 적재가 끝난 뒤 사용자가 정한다.
  const QString localBase =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  const QString root = QDir(localBase).filePath(
      QStringLiteral("주변유적/%1 %2/원본").arg(sido, city));
  if (!QDir().mkpath(root)) {
    KaUserError::warn(this, {
        QStringLiteral("주변유적 받기"),
        QStringLiteral("주변유적 자료를 둘 폴더를 만들지 못했습니다."),
        QStringLiteral("저장 위치의 쓰기 권한이나 공간이 부족할 수 있습니다."),
        QStringLiteral("디스크 공간과 폴더 권한을 확인한 뒤 다시 받아 주세요."),
    });
    return;
  }

  m_heritageBrowser->setDownloadRoot(root);
  m_heritageBrowser->setTarget(sido, city, HeritageStyle::allDatasets());
  m_heritageBrowser->setFetchPlan(choice.plan());  // F120 neighbours, F174 reuse
  m_heritageBrowser->show();
  m_heritageBrowser->raise();
  m_heritageBrowser->start();
}

// 받은 파일을 그 자리에서 지도에 올린다. 색·범례는 HeritageStyle 이 건다.
HeritageImport::Result MainWindow::importHeritageDataset(HeritageDataset dataset, const QStringList& files) {
  if (m_surveyPath.isEmpty()) {
    HeritageImport::Result result;
    result.error = QStringLiteral("열린 조사가 없어 자료를 지도에 올리지 못했습니다.");
    return result;
  }
  // 푸는 자리도 로컬이다. OneDrive 동기화 폴더에서 풀면 파일이 잠기거나 0바이트로 보인다.
  const QString archiveRoot =
      QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
          .filePath(QStringLiteral("주변유적/SHP"));
  QDir().mkpath(archiveRoot);

  const HeritageImport::Result result =
      HeritageImport::loadDataset(QgsProject::instance(), dataset, files, archiveRoot,
                                  m_heritageBrowser ? m_heritageBrowser->regionLabelForImport()
                                                    : QString());
  for (const QString& message : result.messages)
    statusBar()->showMessage(message, 8000);
  if (!result.ok()) {
    return result;
  }
  // F120: nothing of this 시·군 lies inside 5 km. The message above already says so.
  if (result.emptyInScope) return result;
  LayerOps::applyLayerOrderToLabels(QgsProject::instance(), m_canvas);
  if (m_canvas)
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  statusBar()->showMessage(QStringLiteral("%1 %2곳을 올렸습니다.")
                               .arg(HeritageStyle::layerName(dataset))
                               .arg(result.featureCount),
                           8000);
  return result;
}

// 서약서 동의 영수증. 언제 무엇에 동의했는지 남긴다.
void MainWindow::saveHeritageAgreementReceipt(const QDateTime& when, const QString& terms) {
  if (m_surveyPath.isEmpty()) return;
  const QString dir =
      QDir(QFileInfo(m_surveyPath).absolutePath()).filePath(QStringLiteral("주변유적/receipts"));
  if (!QDir().mkpath(dir)) return;
  const QString path = QDir(dir).filePath(
      QStringLiteral("서약서-%1.txt").arg(when.toString(QStringLiteral("yyyyMMdd-HHmmss"))));
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
  QTextStream out(&file);
  out.setEncoding(QStringConverter::Utf8);
  out << QStringLiteral("국가유산 공간정보 원본자료 사용 서약서 동의 기록") << Qt::endl;
  out << QStringLiteral("동의 시각: ") << when.toString(Qt::ISODate) << Qt::endl;
  out << QStringLiteral("기관 계정: ") << HeritageIntranetSettings::describeForLog() << Qt::endl;
  out << QStringLiteral("----") << Qt::endl;
  out << terms << Qt::endl;
}
