#include "MainWindow.h"
#include "KaHgisVersion.h"
#include <QDateTime>
#include "KaStartupSplash.h"
#include "KaLayerInformation.h"
#include "KaWindowGeometry.h"
#include "core/DemPresentation.h"
#include "KaTheme.h"
#include "KaUserError.h"
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
#include "core/DemAnalyzer.h"
#include "core/TilePackService.h"
#include "core/TrenchGridGenerator.h"
#include "KaAboveLabelsOverlay.h"
#include "KaCanvasGridOverlay.h"
#include "KaTrenchMoveTool.h"
#include "KaFeatureSelectTool.h"
#include "KaStatusBar.h"
#include "KaBeginnerRibbon.h"
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
#include "core/KaSafeQgis.h"
#include "core/SurveyStorage.h"
#include "core/SurveySession.h"
#include "core/GeorefService.h"
#include "core/GeoInfoBoreholeService.h"
#include "core/KigamPaleoService.h"
#include "core/PaleoDemClassifier.h"
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
#include "core/BasemapDsm.h"
#include "core/ReferenceKind.h"
#include "core/ReferenceStorage.h"
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
namespace {
// QGIS 범례 체크 / ArcGIS 레이어 on-off: 이미 있으면 보이기↔숨기기만 한다.
bool toggleExistingOverlay(QgsProject* project, QgsMapCanvas* canvas, const QString& title,
                           QAction* act, QStatusBar* bar) {
  if (!project) return false;
  if (LayerOps::isLayerVisible(project, title)) {
    LayerOps::toggleLayerVisibility(project, canvas, title, false);
    if (act) act->setChecked(false);
    if (bar) bar->showMessage(title + QStringLiteral("를 껐습니다."), 4000);
    return true;
  }
  if (LayerOps::toggleLayerVisibility(project, canvas, title, true)) {
    if (act) act->setChecked(true);
    if (bar) bar->showMessage(title + QStringLiteral("를 다시 켰습니다."), 4000);
    return true;
  }
  return false;
}
}  // namespace

void MainWindow::toggleTerrainMap() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas)
    return;
  if (toggleExistingOverlay(QgsProject::instance(), m_canvas, QStringLiteral("지형맵"), nullptr,
                            statusBar())) {
    if (m_btnTerrain)
      m_btnTerrain->setChecked(
          LayerOps::isLayerVisible(QgsProject::instance(), QStringLiteral("지형맵")));
    return;
  }
  QString err;
  if (!LayerOps::addElevationHillshadeMap(QgsProject::instance(), m_canvas,
                                          VworldSettings::loadApiKey(), &err)) {
    notify(Notice::Warning, QStringLiteral("지형맵"),
           err.isEmpty() ? QStringLiteral("지형맵을 올리지 못했습니다.") : err);
    if (m_btnTerrain)
      m_btnTerrain->setChecked(false);
    return;
  }
  if (!m_canvas->isDrawing())
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  if (m_btnTerrain)
    m_btnTerrain->setChecked(true);
  statusBar()->showMessage(QStringLiteral("지형맵을 올렸습니다. 다시 누르면 숨깁니다."), 6000);
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("지형맵 시뮬레이션"));
#endif
}

void MainWindow::editDemElevationClasses() {
#if KA_HGIS_HAS_QGIS
  QgsRasterLayer* dem = nullptr;
  if (QgsProject* proj = QgsProject::instance()) {
    // By identity, not title: the Copernicus layer is 「지표모델(DSM) …」 and may be renamed.
    const QList<QgsMapLayer*> found =
        ReferenceKind::find(proj, QString::fromLatin1(ReferenceKind::kDem));
    for (QgsMapLayer* l : found) {
      auto* rl = qobject_cast<QgsRasterLayer*>(l);
      if (rl && rl->isValid() &&
          dynamic_cast<QgsSingleBandPseudoColorRenderer*>(rl->renderer())) {
        dem = rl;
        break;
      }
    }
  }
  if (!dem) {
    QMessageBox::information(
        this, QStringLiteral("DEM 표현"),
        QStringLiteral("먼저 DEM을 켜 주세요. 표고 자료를 불러오면 색 표현과 음영을 조절할 수 있습니다."));
    return;
  }
  auto* dlg = new KaDemClassDialog(dem, this, m_canvas);
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  dlg->show();
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("DEM 높이 구간"));
#endif
}

void MainWindow::toggleDemMap() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas)
    return;
  QgsProject* proj = QgsProject::instance();
  // Tagged dem, or an untagged 「DEM」 from an older project; the title may have changed.
  QgsRasterLayer* dem = BasemapDsm::findDem(proj);

  if (dem) {
    const bool wasVisible = LayerOps::isLayerVisible(proj, QStringLiteral("DEM"));
    const bool makeVisible = !wasVisible;
    // 켜는 참인데 지금 화면이 기존 DEM 밖으로 나가 있으면, 화면 전체를 덮도록 새로 받는다.
    if (makeVisible && !LayerOps::demCoversCanvas(proj, m_canvas)) {
      startDemDownload();
      return;
    }
    LayerOps::toggleLayerVisibility(proj, m_canvas, QStringLiteral("DEM"), makeVisible);
    if (m_btnDem)
      m_btnDem->setChecked(makeVisible);

    if (makeVisible) {
      LayerOps::ensureDemRelief(proj, dem);
      DemPresentation::followCanvas(dem, m_canvas);
    }
    if (!makeVisible && !LayerOps::isLayerVisible(proj, QStringLiteral("지질도"))) {
      LayerOps::toggleLayerVisibility(proj, m_canvas, QStringLiteral("지형 음영"), false);
    }
    if (!m_canvas->isDrawing()) {
      LayerOps::syncMapCanvas(proj, m_canvas, false);
      m_canvas->refresh();
    }
    statusBar()->showMessage(
        makeVisible ? QStringLiteral("DEM을 켰습니다. 「DEM 표현」에서 색과 음영을 조절하세요.")
                    : QStringLiteral("DEM 지형을 숨겼습니다."),
        4000);
    return;
  }

  startDemDownload();
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("DEM 시뮬레이션"));
#endif
}

// 흙토람 공개 지도서버(국립농업과학원 GeoServer)에서 현재 화면 범위의
// 정밀토양도를 내려받아 분포지형 공식 색으로 겹친다. 신청·키가 필요 없다.
void MainWindow::downloadSoilTerrain() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  // 본체 클릭만 토글. 화살표 메뉴의 「내려받기」는 다시 받는다.
  if (!qobject_cast<QAction*>(sender())) {
    if (toggleExistingOverlay(QgsProject::instance(), m_canvas, QStringLiteral("토양도(흙토람)"),
                              nullptr, statusBar())) {
      if (m_btnSoil)
        m_btnSoil->setChecked(
            LayerOps::isLayerVisible(QgsProject::instance(), QStringLiteral("토양도(흙토람)")));
      return;
    }
  }
  // 조사 GPKG 옆에 저장해 다음에도(오프라인 포함) 다시 쓸 수 있게 한다.
  // 조사 폴더가 없으면 임시 폴더에 받지 않고 조사부터 만들도록 안내한다.
  const QString dir = referenceDownloadFolder(QStringLiteral("토양도"));
  if (dir.isEmpty()) return;
  if (LayerOps::clampCanvasToThematicScale(m_canvas))
    statusBar()->showMessage(QStringLiteral("축척을 1:100000으로 맞춘 뒤 토양도를 받습니다."), 4000);

  QgsRectangle ext = m_canvas->extent();
  const QgsCoordinateReferenceSystem crs5186(QStringLiteral("EPSG:5186"));
  const QgsCoordinateReferenceSystem canvasCrs = m_canvas->mapSettings().destinationCrs();
  if (canvasCrs.isValid() && canvasCrs != crs5186) {
    try {
      const QgsCoordinateTransform tr(canvasCrs, crs5186, QgsProject::instance());
      ext = tr.transformBoundingBox(ext);
    } catch (const QgsException&) {
      notify(Notice::Critical, QStringLiteral("토양도"),
             QStringLiteral("화면 범위를 좌표 변환하지 못했습니다."));
      return;
    }
  }
  if (ext.width() > SoilMapService::maxSpanMeters() ||
      ext.height() > SoilMapService::maxSpanMeters()) {
    notify(Notice::Warning, QStringLiteral("토양도"),
           QStringLiteral("범위가 너무 넓습니다. 지도를 조사구역(한 변 %1km 이하)으로 "
                          "확대한 뒤 다시 내려받으세요.")
               .arg(SoilMapService::maxSpanMeters() / 1000.0, 0, 'f', 0));
    return;
  }

  const QString outGpkg = QDir(dir).filePath(QStringLiteral("토양도_흙토람.gpkg"));

  startReferenceDownload(ReferenceMapKind::Soil, ext, outGpkg);
#else
  QMessageBox::information(this, QStringLiteral("스텁"),
                           QStringLiteral("토양도 내려받기 시뮬레이션"));
#endif
}

// KIGAM 공개 지도서버에서 현재 화면 범위의 1:5만 지질도(암상)를 내려받아
// 지질시대별 ICS 표준색 + 암상 기호 라벨로 겹친다. 신청·키가 필요 없다.
void MainWindow::downloadGeologyMap() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  if (toggleExistingOverlay(QgsProject::instance(), m_canvas,
                            QStringLiteral("지질도(KIGAM 1:5만)"), m_actGeology, statusBar())) {
    QgsProject* proj = QgsProject::instance();
    const bool on = LayerOps::isLayerVisible(proj, QStringLiteral("지질도(KIGAM 1:5만)"));
    if (on) {
      if (QgsMapLayer* geo = GeologyMapService::existingGeologyLayer(proj))
        GeologyMapService::ensureReliefUnderlay(proj, m_canvas, geo, nullptr);
      if (m_canvas && !m_canvas->isDrawing())
        LayerOps::syncMapCanvas(proj, m_canvas, false);
    }
    LayerOps::toggleLayerVisibility(proj, m_canvas,
                                    GeologyMapService::reliefLayerTitle(), on);
    return;
  }
  const QString dir = referenceDownloadFolder(QStringLiteral("지질도"));
  if (dir.isEmpty()) {
    if (m_actGeology) m_actGeology->setChecked(false);
    return;
  }
  if (LayerOps::clampCanvasToThematicScale(m_canvas))
    statusBar()->showMessage(QStringLiteral("축척을 1:100000으로 맞춘 뒤 지질도를 받습니다."), 4000);

  QgsRectangle ext = m_canvas->extent();
  const QgsCoordinateReferenceSystem crs5186(QStringLiteral("EPSG:5186"));
  const QgsCoordinateReferenceSystem canvasCrs = m_canvas->mapSettings().destinationCrs();
  if (canvasCrs.isValid() && canvasCrs != crs5186) {
    try {
      const QgsCoordinateTransform tr(canvasCrs, crs5186, QgsProject::instance());
      ext = tr.transformBoundingBox(ext);
    } catch (const QgsException&) {
      notify(Notice::Critical, QStringLiteral("지질도"),
             QStringLiteral("화면 범위를 좌표 변환하지 못했습니다."));
      return;
    }
  }
  if (ext.width() > GeologyMapService::maxSpanMeters() ||
      ext.height() > GeologyMapService::maxSpanMeters()) {
    notify(Notice::Warning, QStringLiteral("지질도"),
           QStringLiteral("범위가 너무 넓습니다. 지도를 조사구역(한 변 %1km 이하)으로 "
                          "확대한 뒤 다시 내려받으세요.")
               .arg(GeologyMapService::maxSpanMeters() / 1000.0, 0, 'f', 0));
    return;
  }

  const QString outGpkg = QDir(dir).filePath(QStringLiteral("지질도_KIGAM.gpkg"));

  startReferenceDownload(ReferenceMapKind::Geology, ext, outGpkg);
#else
  QMessageBox::information(this, QStringLiteral("스텁"),
                           QStringLiteral("지질도 내려받기 시뮬레이션"));
#endif
}

// VWorld 공개 WFS에서 현재 화면 범위의 하천망(국가·지방하천)을 내려받아
// 등급별 물색 + 하천명 라벨로 겹친다. 배경지도와 같은 VWorld 키를 쓴다.
void MainWindow::downloadRiverMap() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  if (toggleExistingOverlay(QgsProject::instance(), m_canvas, QStringLiteral("수계도(하천망)"),
                            m_actRiver, statusBar()))
    return;
  const QString dir = referenceDownloadFolder(QStringLiteral("수계도"));
  if (dir.isEmpty()) {
    if (m_actRiver) m_actRiver->setChecked(false);
    return;
  }
  if (LayerOps::clampCanvasToThematicScale(m_canvas))
    statusBar()->showMessage(QStringLiteral("축척을 1:100000으로 맞춘 뒤 수계도를 받습니다."), 4000);

  const QString key = VworldSettings::loadApiKey();
  if (key.trimmed().isEmpty()) {
    notify(Notice::Warning, QStringLiteral("수계도"),
           QStringLiteral("VWorld 인증키가 없습니다. 배경지도 설정에서 키를 먼저 "
                          "등록하세요."));
    return;
  }

  QgsRectangle ext = m_canvas->extent();
  const QgsCoordinateReferenceSystem crs5186(QStringLiteral("EPSG:5186"));
  const QgsCoordinateReferenceSystem canvasCrs = m_canvas->mapSettings().destinationCrs();
  if (canvasCrs.isValid() && canvasCrs != crs5186) {
    try {
      const QgsCoordinateTransform tr(canvasCrs, crs5186, QgsProject::instance());
      ext = tr.transformBoundingBox(ext);
    } catch (const QgsException&) {
      notify(Notice::Critical, QStringLiteral("수계도"),
             QStringLiteral("화면 범위를 좌표 변환하지 못했습니다."));
      return;
    }
  }
  if (ext.width() > RiverMapService::maxSpanMeters() ||
      ext.height() > RiverMapService::maxSpanMeters()) {
    notify(Notice::Warning, QStringLiteral("수계도"),
           QStringLiteral("범위가 너무 넓습니다. 지도를 조사구역(한 변 %1km 이하)으로 "
                          "확대한 뒤 다시 내려받으세요.")
               .arg(RiverMapService::maxSpanMeters() / 1000.0, 0, 'f', 0));
    return;
  }

  const QString outGpkg = QDir(dir).filePath(QStringLiteral("수계도_VWorld.gpkg"));

  startReferenceDownload(ReferenceMapKind::River, ext, outGpkg, key);
#else
  QMessageBox::information(this, QStringLiteral("스텁"),
                           QStringLiteral("수계도 내려받기 시뮬레이션"));
#endif
}

void MainWindow::startReferenceDownload(ReferenceMapKind kind, const QgsRectangle& extent,
                                        const QString& targetPath, const QString& apiKey) {
  if (m_isOpeningSurvey || m_closingWindow) return;
  if (m_referenceDownload) {
    statusBar()->showMessage(QStringLiteral("지도 자료를 내려받고 있습니다. 완료를 기다리거나 취소하세요."), 5000);
    return;
  }
  const QString title = (kind == ReferenceMapKind::Soil || kind == ReferenceMapKind::PaleoSoil) ? QStringLiteral("토양도")
      : kind == ReferenceMapKind::Geology ? QStringLiteral("지질도")
      : kind == ReferenceMapKind::GeoInfoBorehole ? QStringLiteral("시추주상도")
      : kind == ReferenceMapKind::KigamPaleo ? QStringLiteral("KIGAM 고지형·수문") : QStringLiteral("수계도");
  auto* progress = createDownloadProgress(title);
  const auto context = QgsProject::instance()->transformContext();
  const quint64 generation = m_surveyGeneration;
  const QPointer<MainWindow> window(this);
  const QPointer<QProgressDialog> dialog(progress);
  auto prepare = [kind, extent, targetPath, apiKey, context](QgsFeedback* feedback) {
    switch (kind) {
      case ReferenceMapKind::Soil:
      case ReferenceMapKind::PaleoSoil:
        return SoilMapService::prepare(extent, targetPath, context, feedback);
      case ReferenceMapKind::Geology:
        return GeologyMapService::prepare(extent, targetPath, context, feedback);
      case ReferenceMapKind::River:
        return RiverMapService::prepare(extent, apiKey, targetPath, context, feedback);
      case ReferenceMapKind::GeoInfoBorehole:
        return GeoInfoBoreholeService::prepare(extent, targetPath, context, feedback);
      case ReferenceMapKind::KigamPaleo:
        return KigamPaleoService::prepare(extent, targetPath, context, feedback);
    }
    return PreparedReferenceMap{};
  };
  auto complete = [window, dialog, generation, kind, title, extent](const PreparedReferenceMap& result) {
    if (dialog) { dialog->hide(); dialog->deleteLater(); }
    if (!window) return;
    window->m_referenceDownload = nullptr;
    if (window->m_closingWindow || generation != window->m_surveyGeneration) return;
    if (result.status == PreparedReferenceMap::Status::Cancelled) {
      window->statusBar()->showMessage(title + QStringLiteral(" 내려받기를 취소했습니다. 기존 지도는 유지됩니다."), 6000);
      return;
    }
    if (!result.isReady()) {
      if (kind == ReferenceMapKind::KigamPaleo || kind == ReferenceMapKind::GeoInfoBorehole) {
        window->statusBar()->showMessage(result.error, 8000);
        if (kind == ReferenceMapKind::KigamPaleo &&
            !QgsProject::instance()->property("paleoGeoInfoAttempted").toBool()) {
          QgsProject::instance()->setProperty("paleoGeoInfoAttempted", true);
          const QString bhGpkg = QDir(QFileInfo(window->m_surveyPath).absolutePath())
                                     .filePath(QStringLiteral("시추주상도_국토지반정보.gpkg"));
          QTimer::singleShot(0, window, [window, extent, bhGpkg] {
            if (window && !window->m_closingWindow)
              window->startReferenceDownload(ReferenceMapKind::GeoInfoBorehole, extent, bhGpkg);
          });
        }
        return;
      }
      window->notify(Notice::Warning, title + QStringLiteral(" 내려받기 실패"), result.error.isEmpty()
          ? QStringLiteral("지도 자료를 받지 못했습니다. 인터넷 연결을 확인한 뒤 다시 시도하세요.") : result.error);
      return;
    }
    try {
      QString error;
      QgsMapLayer* layer = nullptr;
      const auto foldersBefore = ReferenceStorage::referencedFolderNames(QgsProject::instance());
      switch (kind) {
        case ReferenceMapKind::Soil:
        case ReferenceMapKind::PaleoSoil:
          layer = SoilMapService::addPrepared(QgsProject::instance(), window->m_canvas, result, &error); break;
        case ReferenceMapKind::Geology:
          layer = GeologyMapService::addPrepared(QgsProject::instance(), window->m_canvas, result, &error); break;
        case ReferenceMapKind::River:
          layer = RiverMapService::addPrepared(QgsProject::instance(), window->m_canvas, result, &error); break;
        case ReferenceMapKind::GeoInfoBorehole:
          layer = GeoInfoBoreholeService::addPrepared(QgsProject::instance(), window->m_canvas, result, &error); break;
        case ReferenceMapKind::KigamPaleo:
          layer = KigamPaleoService::addPrepared(QgsProject::instance(), window->m_canvas, result, &error); break;
      }
      if (!window || window->m_closingWindow || generation != window->m_surveyGeneration) return;
      if (!layer) {
        window->notify(Notice::Warning, title + QStringLiteral(" 표시 실패"), error.isEmpty()
            ? QStringLiteral("받은 지도를 열지 못했습니다. 기존 지도는 유지됩니다. 다시 내려받으세요.") : error);
        return;
      }
      // The replaced download folder is removed only at the next save point.
      auto& ledger = ReferenceStorage::sessionLedger();
      if (result.storage) ledger.adopt(result.storage->path(), window->m_surveyPath);
      ledger.supersede(foldersBefore, ReferenceStorage::referencedFolderNames(QgsProject::instance()));
      if (window->m_layerTree) window->m_layerTree->setCurrentLayer(layer);
      if ((kind == ReferenceMapKind::Soil || kind == ReferenceMapKind::PaleoSoil) && window->m_btnSoil)
        window->m_btnSoil->setChecked(true);
      if (kind == ReferenceMapKind::Geology && window->m_actGeology) window->m_actGeology->setChecked(true);
      if (kind == ReferenceMapKind::River && window->m_actRiver) window->m_actRiver->setChecked(true);
      QgsProject::instance()->setDirty(true);
      window->statusBar()->showMessage(title + QStringLiteral("를 추가했습니다. 조사 저장으로 지도 구성을 보관하세요."), 10000);
      if (!result.warnings.isEmpty())
        window->notify(Notice::Warning, title + QStringLiteral(" 확인 사항"), result.warnings.join(QLatin1Char('\n')));
      if (kind == ReferenceMapKind::PaleoSoil) {
        QTimer::singleShot(0, window, [window, generation] {
          if (window && !window->m_closingWindow && generation == window->m_surveyGeneration)
            window->startPaleoLandform();
        });
      } else if (kind == ReferenceMapKind::KigamPaleo &&
                 !QgsProject::instance()->property("paleoGeoInfoAttempted").toBool()) {
        QgsProject::instance()->setProperty("paleoGeoInfoAttempted", true);
        const QString bhGpkg = QDir(QFileInfo(window->m_surveyPath).absolutePath())
                                   .filePath(QStringLiteral("시추주상도_국토지반정보.gpkg"));
        QTimer::singleShot(0, window, [window, extent, bhGpkg] {
          if (window && !window->m_closingWindow)
            window->startReferenceDownload(ReferenceMapKind::GeoInfoBorehole, extent, bhGpkg);
        });
      } else if (kind == ReferenceMapKind::GeoInfoBorehole) {
        if (auto* paleo = LayerOps::findByLayerKey(QgsProject::instance(),
                                                   QLatin1String(PaleoLandformService::kLayerKey))) {
          PaleoDemClassifier::enrichInterpretationLayer(QgsProject::instance(), paleo);
          PaleoLandformService::applyInterpretationStyle(paleo);
        }
      }
    } catch (...) {
      QgsProject::instance()->setDirty(true);
      window->refreshWindowTitle();
      KaCrashGuard::logLine(QStringLiteral("[reference] 받은 지도 표시 중 예외 — 현재 작업 유지"));
      window->notify(Notice::Warning, title + QStringLiteral(" 표시 실패"),
                     QStringLiteral("받은 지도를 표시하는 중 오류가 발생했습니다. 현재 작업을 저장하고 "
                                    "레이어 목록을 확인한 뒤 다시 내려받으세요."));
    }
  };
  auto* job = new KaReferenceDownloadJob(title, std::move(prepare), std::move(complete));
  m_referenceDownload = job;
  connect(progress, &QProgressDialog::canceled, job, &KaReferenceDownloadJob::cancel);
  progress->show();
  QgsApplication::taskManager()->addTask(job);
}
