#include "MainWindow.h"
#include "KaTopographicBrowser.h"
#include "KaTopographicImportDialog.h"
#include "KaTopographicScopePanel.h"
#include "KaTopographicAccountDialog.h"
#include "KaUserError.h"
#include "core/PaleoAnalysisRunner.h"
#include "core/TopographicSettings.h"
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>
#include <QVBoxLayout>

void MainWindow::openTopographicDownload() {
  if (!m_viewTabs) return;
  if (m_surveyPath.isEmpty() || !QFileInfo::exists(m_surveyPath)) {
    QMessageBox::information(this,QStringLiteral("수치지형도"),
      QStringLiteral("먼저 새 조사를 만들거나 기존 조사를 여세요. 조사 폴더 안의 지형도 폴더에 저장합니다."));
    return;
  }
  const QString directory=QFileInfo(m_surveyPath).absoluteDir().filePath(QStringLiteral("지형도"));
  if (!QDir().mkpath(directory)) {
    KaUserError::warn(this, {
        QStringLiteral("수치지형도"),
        QStringLiteral("조사 폴더에 지형도 폴더를 만들지 못했습니다."),
        QStringLiteral("저장 위치의 쓰기 권한이나 공간이 부족할 수 있습니다."),
        QStringLiteral("저장 위치의 쓰기 권한과 공간을 확인한 뒤 다시 열어 주세요."),
    });
    return;
  }
  if (!m_topographicBrowser) {
    auto* browser = new KaTopographicBrowser(this,QDir(directory).filePath(QStringLiteral("원본")));
    m_topographicBrowser = browser;
    if (!m_topographicImport) m_topographicImport = new KaTopographicImportDialog(m_canvas, this);
    auto* scope = new KaTopographicScopePanel(m_canvas, browser, m_topographicImport, browser,directory);
    if (auto* layout = qobject_cast<QVBoxLayout*>(browser->layout())) layout->insertWidget(0, scope);
    scope->hide();
    connect(scope,&KaTopographicScopePanel::statusChanged,browser,&KaTopographicBrowser::setActivityStatus);
    connect(scope,&KaTopographicScopePanel::processingChanged,browser,&KaTopographicBrowser::setProcessing);
    connect(scope,&KaTopographicScopePanel::attentionRequired,browser,&KaTopographicBrowser::setActivityAttention);
    connect(scope,&KaTopographicScopePanel::preparationProgressChanged,browser,&KaTopographicBrowser::setPreparationProgress);
    connect(browser,&KaTopographicBrowser::cancelRequested,scope,&KaTopographicScopePanel::stop);
    connect(scope, &KaTopographicScopePanel::mapRequested, this, &MainWindow::showMapWorkspace);
    connect(browser, &KaTopographicBrowser::sheetDownloaded, scope, &KaTopographicScopePanel::acceptDownload);
    connect(browser, &KaTopographicBrowser::fileDownloaded, this, [this](const QString& path) {
      const QString ext = QFileInfo(path).suffix().toLower();
      if (ext == QLatin1String("img") || ext == QLatin1String("tif") || ext == QLatin1String("tiff")) {
        PaleoAnalysisRunner::registerDemWideLayers(QgsProject::instance(), m_canvas, path, m_surveyPath, nullptr);
      } else if (ext == QLatin1String("zip") || ext == QLatin1String("shp") || ext == QLatin1String("dxf") ||
          ext == QLatin1String("ngi") || ext == QLatin1String("gpkg")) {
        QDir received(QFileInfo(path).absolutePath());
        received.cdUp(); // Include earlier completed downloads and their sidecars.
        showTopographicFiles(received.absolutePath());
      } else {
        statusBar()->showMessage(QStringLiteral("파일을 받았습니다. 수치지형도·DEM 자료를 선택해 주세요."), 10000);
      }
    });
    connect(browser, &KaTopographicBrowser::importFolderRequested, this, &MainWindow::importTopographicFolder);
  }
  auto account=TopographicSettings::credentials();
  if ((account.username.isEmpty() || account.password.isEmpty()) && !QStandardPaths::isTestModeEnabled()) {
    configureTopographicAccount();
    account = TopographicSettings::credentials();
  }
  m_topographicBrowser->setCredentials(account.username,account.password);
  if (auto* scope = m_topographicBrowser->findChild<KaTopographicScopePanel*>()) {
    scope->setLibraryDirectory(directory); scope->activate();
  }
  showMapWorkspace();
  m_topographicBrowser->setWindowModality(Qt::NonModal);
  m_topographicBrowser->setCompactMode(true);
  m_topographicBrowser->show();
  hideSubTools();
}

void MainWindow::configureTopographicAccount() {
  const auto previous=TopographicSettings::credentials();
  KaTopographicAccountDialog dialog(this);
  if (dialog.exec()!=QDialog::Accepted) return;
  if (m_topographicBrowser) {
    const auto account=TopographicSettings::credentials();
    auto* scope=m_topographicBrowser->findChild<KaTopographicScopePanel*>();
    const bool changed=account.username!=previous.username || account.password!=previous.password;
    const bool resume=changed && scope && scope->isActive();
    if (changed && scope) scope->stop();
    m_topographicBrowser->setCredentials(account.username,account.password);
    if (resume) scope->activate();
  }
  statusBar()->showMessage(QStringLiteral("수치지형도 로그인 설정을 저장했습니다."),5000);
}

void MainWindow::updateTopographicDirectory(const QString& surveyPath) {
  if (surveyPath.isEmpty() || !QFileInfo::exists(surveyPath)) return;
  const QString directory=QFileInfo(surveyPath).absoluteDir().filePath(QStringLiteral("지형도"));
  if (!QDir().mkpath(directory)) {
    if (m_topographicBrowser)
      if (auto* scope=m_topographicBrowser->findChild<KaTopographicScopePanel*>()) scope->setLibraryDirectory({});
    statusBar()->showMessage(QStringLiteral("조사 폴더의 지형도 저장 위치를 만들지 못했습니다."),10000); return;
  }
  if (m_topographicBrowser)
    if (auto* scope=m_topographicBrowser->findChild<KaTopographicScopePanel*>()) scope->setLibraryDirectory(directory);
}

void MainWindow::importTopographicFolder() {
  QSettings settings;
  const QString initial = m_surveyPath.isEmpty()
      ? settings.value(QStringLiteral("topographic/folder"),QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString()
      : QFileInfo(m_surveyPath).absoluteDir().filePath(QStringLiteral("지형도"));
  const QString folder = QFileDialog::getExistingDirectory(this, QStringLiteral("수치지형도 폴더 선택 — 하위 폴더도 확인합니다"), initial);
  if (!folder.isEmpty()) showTopographicFiles(folder);
}

void MainWindow::showTopographicFiles(const QString& folder) {
  showMapWorkspace();
  QSettings().setValue(QStringLiteral("topographic/folder"), QDir(folder).absolutePath());
  if (!m_topographicImport) m_topographicImport = new KaTopographicImportDialog(m_canvas, this);
  // Manually added sheets merge into the survey's topographic folder as well.
  if (!m_surveyPath.isEmpty() && QFileInfo::exists(m_surveyPath))
    m_topographicImport->setMergeDirectory(
        QFileInfo(m_surveyPath).absoluteDir().filePath(QStringLiteral("지형도/합침")));
  m_topographicImport->scanFolder(folder);
}
