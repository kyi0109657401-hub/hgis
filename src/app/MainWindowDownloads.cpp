#include "KaCrashGuard.h"
#include "core/KaLogExcept.h"
#include "MainWindow.h"
#include "KaReferenceDownloadJob.h"
#include "KaTopographicBrowser.h"
#include "core/BasemapDsm.h"
#include "core/LayerOps.h"
#include "core/PaleoAnalysisRunner.h"
#include "core/ReferenceStorage.h"
#include "core/TopographicSettings.h"
#include "core/TopographicSheets.h"
#include <QDir>
#include <QFileInfo>
#include <QLabel>
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
  auto* project = QgsProject::instance();
  for (QgsMapLayer* old : project->mapLayersByName(BasemapDsm::copernicusTitle())) {
    if (old) project->removeMapLayer(old->id());
  }
  if (m_surveyPath.isEmpty() || !QFileInfo::exists(m_surveyPath)) {
    notify(Notice::Info, QStringLiteral("국토지리정보원 DEM"),
           QStringLiteral("먼저 새 조사를 만들거나 기존 조사를 여세요. 조사 폴더 안의 DEM 폴더에 저장합니다."));
    return;
  }
  const QString demDir = QFileInfo(m_surveyPath).absoluteDir().filePath(QStringLiteral("DEM"));
  const QString rawDir = QDir(demDir).filePath(QStringLiteral("원본"));
  if (!QDir().mkpath(rawDir)) {
    notify(Notice::Warning, QStringLiteral("국토지리정보원 DEM"),
           QStringLiteral("조사 폴더 안에 DEM 폴더를 만들지 못했습니다."));
    return;
  }
  if (!m_demBrowser) {
    m_demBrowser = new KaTopographicBrowser(this, rawDir);
    m_demBrowser->setProperty("kaDemMode", true);
    m_demBrowser->setObjectName(QStringLiteral("demBrowser"));
    m_demBrowser->setWindowTitle(QStringLiteral("국토지리정보원 DEM 내려받기"));
    if (auto* title = m_demBrowser->findChild<QLabel*>(QStringLiteral("downloadTitle")))
      title->setText(QStringLiteral("국토지리정보원 DEM 다운로드"));
    if (auto* sub = m_demBrowser->findChild<QLabel*>(QStringLiteral("downloadSubtitle")))
      sub->setText(QStringLiteral("국토정보플랫폼 · 조사 주변 도엽의 공개DEM 자료를 받습니다."));
    auto importDem = [this](const QString& path) {
      QString err;
      if (PaleoAnalysisRunner::importNgiiDemDownload(QgsProject::instance(), m_canvas, path, m_surveyPath, &err)) {
        if (m_demBrowser) m_demBrowser->setActivityStatus(QStringLiteral("국토지리정보원 DEM·광역지형도·경사도를 지도에 올렸습니다."));
        QgsProject::instance()->setDirty(true);
        syncThematicButtons();
        statusBar()->showMessage(QStringLiteral("국토지리정보원 DEM·광역지형도·경사도를 적재했습니다."), 8000);
      } else {
        const QString msg = err.isEmpty() ? QStringLiteral("받은 DEM 파일을 열지 못했습니다.") : err;
        if (m_demBrowser) m_demBrowser->setActivityAttention(msg);
        notify(Notice::Warning, QStringLiteral("국토지리정보원 DEM 적재 실패"), msg);
      }
    };
    connect(m_demBrowser, &KaTopographicBrowser::sheetDownloaded, this,
            [importDem](const QString& path, const QJsonObject&) { importDem(path); });
    connect(m_demBrowser, &KaTopographicBrowser::fileDownloaded, this, importDem);
    connect(m_demBrowser, &KaTopographicBrowser::importFolderRequested, this, &MainWindow::importDemElevationRaster);
    connect(m_demBrowser, &KaTopographicBrowser::automationNeedsInput, this,
            [this](const QString& message) { statusBar()->showMessage(message, 8000); });
  } else {
    m_demBrowser->setDownloadRoot(rawDir);
  }
  const auto account = TopographicSettings::credentials();
  m_demBrowser->setCredentials(account.username, account.password);
  QgsPointXY centerWork;
  PaleoAnalysisRunner::surveyExtentRadius(project, m_canvas, 5000.0, nullptr, &centerWork);
  const QgsCoordinateReferenceSystem workCrs =
      project->crs().isValid() ? project->crs() : m_canvas->mapSettings().destinationCrs();
  try {
    const auto selected = TopographicSheets::select(centerWork, workCrs, 5.0, project->transformContext());
    QStringList wanted;
    for (const auto& s : selected.sheets) wanted.append(s.number);
    if (wanted.isEmpty()) wanted.append(QStringLiteral("359011"));
    QgsCoordinateTransform toPortal(workCrs, QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")), project);
    toPortal.setAllowFallbackTransforms(false);
    toPortal.setBallparkTransformsAreAppropriate(false);
    const QgsPointXY portal = toPortal.transform(centerWork);
    m_demBrowser->prepareSheets(portal.x(), portal.y(), 5000.0, wanted);
  } catch (const QgsCsException&) {
    KA_LOG_EXCEPT();
    m_demBrowser->setActivityAttention(QStringLiteral("현재 위치를 국토지리정보원 검색 좌표로 변환하지 못했습니다."));
  }
  m_demBrowser->setCompactMode(true);
  m_demBrowser->show();
  m_demBrowser->raise();
  m_demBrowser->activateWindow();
  hideSubTools();
}
