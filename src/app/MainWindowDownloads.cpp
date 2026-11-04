#include "KaCrashGuard.h"
#include "core/KaLogExcept.h"
#include "MainWindow.h"
#include "KaReferenceDownloadJob.h"
#include "core/BasemapDsm.h"
#include "core/DemDownloadService.h"
#include "core/LayerOps.h"
#include "core/PaleoAnalysisRunner.h"
#include "core/ReferenceStorage.h"
#include <QDir>
#include <QFileInfo>
#include <QProgressDialog>
#include <QStandardPaths>
#include <QStatusBar>
#include <QToolButton>
#include <qgsapplication.h>
#include <qgscoordinatetransform.h>
#include <qgsexception.h>
#include <qgslayertreeview.h>
#include <qgsmapcanvas.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>

QProgressDialog* MainWindow::createDownloadProgress(const QString& title) {
  auto* dialog = new QProgressDialog(
      title + QStringLiteral(" 자료를 내려받는 중입니다. 지도 작업을 계속할 수 있습니다."),
      QStringLiteral("취소"), 0, 0, this);
  dialog->setObjectName(QStringLiteral("mapDownloadProgress"));
  dialog->setWindowTitle(title + QStringLiteral(" 내려받기"));
  dialog->setWindowModality(Qt::NonModal);
  dialog->setMinimumDuration(0);
  dialog->setAutoClose(false);
  dialog->setAutoReset(false);
  return dialog;
}

void MainWindow::startFileDownload(const QString& title,
    std::function<PreparedReferenceMap(QgsFeedback*, const std::function<bool()>&)> prepare,
    std::function<void(const PreparedReferenceMap&)> apply) {
  if (m_isOpeningSurvey || m_closingWindow) return;
  if (m_referenceDownload) {
    statusBar()->showMessage(QStringLiteral("지도 자료를 받고 있습니다. 완료를 기다리거나 진행 창에서 취소하세요."), 5000);
    return;
  }
  const QPointer<QProgressDialog> dialog = createDownloadProgress(title);
  const QPointer<MainWindow> window(this);
  const quint64 generation = m_surveyGeneration;
  auto* job = new KaReferenceDownloadJob(title, std::move(prepare),
      [window, dialog, title, generation, apply = std::move(apply)](const PreparedReferenceMap& result) {
    if (dialog) { dialog->hide(); dialog->deleteLater(); }
    if (!window) return;
    window->m_referenceDownload = nullptr;
    if (window->m_closingWindow || generation != window->m_surveyGeneration) return;
    window->syncThematicButtons();
    if (result.status == PreparedReferenceMap::Status::Cancelled) {
      window->statusBar()->showMessage(title + QStringLiteral(" 내려받기를 취소했습니다. 기존 자료는 그대로입니다."), 6000);
      return;
    }
    if (!result.isReady()) {
      window->notify(Notice::Warning, title + QStringLiteral(" 내려받기 실패"), result.error.isEmpty()
          ? QStringLiteral("자료를 받지 못했습니다. 인터넷 연결과 저장 공간을 확인하고 다시 시도하세요.") : result.error);
      return;
    }
    // Keep the completed download available for recovery even if GUI insertion
    // fails after a layer has started using the file.
    result.retainFiles();
    try {
      const auto before = ReferenceStorage::referencedFolderNames(QgsProject::instance());
      apply(result);
      // Remember which earlier download folder this registration replaced; it is
      // removed only at the next save point (see pruneRetiredReferenceFolders).
      auto& ledger = ReferenceStorage::sessionLedger();
      const auto after = ReferenceStorage::referencedFolderNames(QgsProject::instance());
      if (window && result.storage && after.contains(QFileInfo(result.storage->path()).fileName()))
        ledger.adopt(result.storage->path(), window->m_surveyPath);
      ledger.supersede(before, after);
      if (window) window->syncThematicButtons();
    } catch (...) {
      KA_LOG_EXCEPT();
      if (window) window->notify(Notice::Warning, title + QStringLiteral(" 표시 실패"),
          QStringLiteral("자료를 받았지만 지도에 표시하지 못했습니다. 현재 작업을 저장한 뒤 파일함에서 다시 열어 주세요.\n%1").arg(result.rasterUri));
    }
  });
  m_referenceDownload = job;
  connect(dialog, &QProgressDialog::canceled, job, &KaReferenceDownloadJob::cancel);
  connect(job, &QgsTask::progressChanged, dialog, [dialog](double progress) {
    if (!dialog || dialog->wasCanceled()) return;
    dialog->setRange(0, 100);
    dialog->setValue(qBound(0, qRound(progress), 100));
  });
  dialog->show();
  QgsApplication::taskManager()->addTask(job);
}

QString MainWindow::referenceDownloadFolder(const QString& title) {
  const QString folder = ReferenceStorage::surveyFolder(m_surveyPath);
  if (folder.isEmpty()) {
    // Without a survey the files would land in a temporary folder that the saved
    // survey never carries along. Ask for the survey first instead of guessing.
    notify(Notice::Info, title,
           QStringLiteral("먼저 「새 조사」를 만들거나 조사를 여세요. 받은 자료는 조사 폴더에 저장해 "
                          "다음에도 인터넷 없이 열 수 있게 합니다."));
  }
  return folder;
}

void MainWindow::pruneRetiredReferenceFolders() {
  // Called when the saved survey equals the current project: a folder replaced
  // by a newer download in this session is no longer used by the saved survey.
  auto& ledger = ReferenceStorage::sessionLedger();
  const auto referenced = ReferenceStorage::referencedFolderNames(QgsProject::instance());
  for (const QString& folder : ledger.removable(referenced, m_surveyPath)) {
    if (ReferenceStorage::removeGeneratedFolder(folder)) {
      ledger.forget(folder);
      KaCrashGuard::logLine(QStringLiteral("[reference] 바뀐 다운로드 폴더 정리 — %1").arg(folder));
    }
  }
}

void MainWindow::startDemDownload() {
  if (!m_canvas) return;
  if (!QStandardPaths::isTestModeEnabled()) {
    openTopographicDownload();
  }
  const QString dir = referenceDownloadFolder(QStringLiteral("DEM"));
  if (dir.isEmpty()) return;
  QgsRectangle extent = PaleoAnalysisRunner::surveyExtentRadius(QgsProject::instance(), m_canvas, 5000.0);
  const QgsCoordinateReferenceSystem workCrs =
      QgsProject::instance()->crs().isValid() ? QgsProject::instance()->crs()
                                              : m_canvas->mapSettings().destinationCrs();
  const QgsCoordinateReferenceSystem wgs(QStringLiteral("EPSG:4326"));
  try {
    const QgsCoordinateTransform transform(workCrs, wgs, QgsProject::instance());
    extent = transform.transformBoundingBox(extent);
  } catch (const QgsException&) {
    notify(Notice::Warning, QStringLiteral("DEM"), QStringLiteral("화면의 좌표 범위를 계산하지 못했습니다. 조사 좌표계를 확인하고 다시 실행하세요."));
    return;
  }
  const QString target = QDir(dir).filePath(QStringLiteral("DEM.tif"));
  const QPointer<MainWindow> window(this);
  startFileDownload(QStringLiteral("DEM"), [extent, target](QgsFeedback* feedback, const std::function<bool()>& cancelled) {
    return DemDownloadService::prepare(extent, target, feedback, cancelled);
  }, [window](const PreparedReferenceMap& result) {
    if (!window) return;
    QString error;
    if (!LayerOps::addDemElevationRaster(QgsProject::instance(), window->m_canvas, result.rasterUri, &error)) {
      window->notify(Notice::Warning, QStringLiteral("DEM 표시 실패"), error);
      return;
    }
    result.retainFiles();
    PaleoAnalysisRunner::registerDemWideLayers(QgsProject::instance(), window->m_canvas,
                                               result.rasterUri, window->m_surveyPath, nullptr);
    for (QgsMapLayer* layer : QgsProject::instance()->mapLayers()) {
      // By identity (ka_hgis/reference_kind = dem), not by the title "DEM".
      if (!BasemapDsm::isDemLayer(layer) || layer->source() != result.rasterUri) continue;
      // This button downloads Copernicus GLO-30, a surface model: the legend says so (F062).
      BasemapDsm::labelCopernicus(layer);
      const QgsRectangle coverage = layer->extent(); // downloaded raster stays in WGS84
      layer->setCustomProperty(QStringLiteral("ka_hgis/dem_cover_wgs84"), QStringLiteral("%1,%2,%3,%4")
          .arg(coverage.xMinimum(), 0, 'f', 9).arg(coverage.yMinimum(), 0, 'f', 9)
          .arg(coverage.xMaximum(), 0, 'f', 9).arg(coverage.yMaximum(), 0, 'f', 9));
      if (window->m_layerTree) window->m_layerTree->setCurrentLayer(layer);
      const QString reliefError = layer->customProperty(QStringLiteral("ka_hgis/dem_relief_error")).toString();
      if (!reliefError.isEmpty()) window->notify(Notice::Warning, QStringLiteral("DEM 음영 표시 실패"), reliefError);
    }
    QgsProject::instance()->setDirty(true);
    window->statusBar()->showMessage(QStringLiteral("DEM을 추가했습니다. DEM 버튼의 「DEM 표현」에서 색과 음영을 조절하세요."), 8000);
    if (!result.warnings.isEmpty()) window->notify(Notice::Warning, QStringLiteral("DEM 자료 확인"), result.warnings.join(QLatin1Char('\n')));
  });
}
