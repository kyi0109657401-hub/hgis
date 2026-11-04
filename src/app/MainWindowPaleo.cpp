#include "KaPaleoMenu.h"
#include "KaDrawingStudio.h"
#include "KaIcons.h"
#include "KaPaleoStudio.h"
#include "KaTheme.h"
#include "KaUserError.h"
#include "MainWindow.h"
#include "core/GeoInfoBoreholeService.h"
#include "core/KaLogExcept.h"
#include "core/KigamPaleoService.h"
#include "core/LayerOps.h"
#include "core/PaleoAnalysisRunner.h"

#include <QAction>
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEngineDownloadRequest>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineView>

#if KA_HGIS_HAS_QGIS
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransform.h>
#include <qgsexception.h>
#include <qgsmapcanvas.h>
#include <qgsproject.h>
#endif

namespace {

void openPaleoPortalDialog(QWidget* parent, const QString& title, const QUrl& startUrl,
                           const QString& surveyPath, QStatusBar* bar) {
  auto* dlg = new QDialog(parent);
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  dlg->setWindowModality(Qt::NonModal);
  dlg->setWindowTitle(title);
  dlg->resize(1180, 780);
  auto* layout = new QVBoxLayout(dlg);
  layout->setContentsMargins(6, 6, 6, 6);
  layout->setSpacing(4);

  const QString saveDir =
      surveyPath.isEmpty()
          ? QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)
          : QFileInfo(surveyPath).absoluteDir().filePath(QStringLiteral("지형분석"));
  QDir().mkpath(saveDir);

  auto* info = new QLabel(
      QStringLiteral("로그인 상태가 앱 안에 유지됩니다. 내려받는 파일은 조사 폴더의 「지형분석」(%1)에 저장됩니다.")
          .arg(QDir::toNativeSeparators(saveDir)),
      dlg);
  info->setWordWrap(true);
  layout->addWidget(info);

  static QWebEngineProfile* s_profile = nullptr;
  if (!s_profile) {
    s_profile = new QWebEngineProfile(QStringLiteral("ka-hgis-paleo-portal"));
    const QString storagePath =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
        QStringLiteral("/paleo-portal-profile");
    QDir().mkpath(storagePath);
    s_profile->setPersistentStoragePath(storagePath);
    s_profile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);
  }
  QObject::connect(
      s_profile, &QWebEngineProfile::downloadRequested, dlg,
      [saveDir, bar](QWebEngineDownloadRequest* req) {
        if (!req) return;
        req->setDownloadDirectory(saveDir);
        req->accept();
        if (bar) {
          bar->showMessage(
              QStringLiteral("지형·지반 자료 내려받기 시작: %1").arg(req->downloadFileName()),
              6000);
        }
      });

  auto* view = new QWebEngineView(dlg);
  auto* page = new QWebEnginePage(s_profile, view);
  view->setPage(page);
  view->setUrl(startUrl);
  layout->addWidget(view, 1);
  dlg->show();
}

}  // namespace

namespace KaPaleoMenu {

void install(QToolButton* button, QWidget* host, const std::function<void()>& trigger) {
  if (!button || !host || button->menu()) return;
  button->setPopupMode(QToolButton::MenuButtonPopup);
  button->setContextMenuPolicy(Qt::CustomContextMenu);
  auto* menu = new QMenu(button);

  auto addItem = [&](const QString& objName, const QString& text) {
    auto* act = menu->addAction(KaIcons::icon(QStringLiteral("paleo")), text);
    act->setObjectName(objName);
    QObject::connect(act, &QAction::triggered, host, [button, objName, trigger] {
      if (button) button->setProperty("paleoAction", objName);
      if (trigger) trigger();
    });
  };
  addItem(QStringLiteral("actionPaleoRun"),
          QStringLiteral("「고지형분석」 전용 탭 열기 및 반경 10km 종합 분석 실행"));
  menu->addSeparator();
  addItem(QStringLiteral("actionPaleoGeoInfo"),
          QStringLiteral("국토지반정보 시추주상도 내려받기 (반경 10km)"));
  addItem(QStringLiteral("actionPaleoKigam"),
          QStringLiteral("KIGAM 단층·지하수위·단구연대 내려받기 (반경 10km)"));
  menu->addSeparator();
  addItem(QStringLiteral("actionPaleoPortalGeoInfo"),
          QStringLiteral("국토지반정보 포털 열기 (geoinfo.or.kr)…"));
  addItem(QStringLiteral("actionPaleoPortalKigam"),
          QStringLiteral("KIGAM 지오빅데이터 포털 열기 (data.kigam.re.kr)…"));

  button->setMenu(menu);
  QObject::connect(button, &QToolButton::customContextMenuRequested, host,
                   [button, menu](const QPoint& pos) { menu->exec(button->mapToGlobal(pos)); });
}

}  // namespace KaPaleoMenu

void MainWindow::startPaleoLandform() {
#if KA_HGIS_HAS_QGIS
  if (m_isOpeningSurvey || m_closingWindow) return;
  QString actionName = m_btnPaleo ? m_btnPaleo->property("paleoAction").toString() : QString();
  if (m_btnPaleo) m_btnPaleo->setProperty("paleoAction", QString());
  if (actionName.isEmpty() && sender()) actionName = sender()->objectName();

  if (actionName == QLatin1String("actionPaleoPortalGeoInfo")) {
    openPaleoPortalDialog(this, QStringLiteral("국토지반정보 포털시스템 (시추·주상도 검색·유통)"),
                          QUrl(QStringLiteral("https://www.geoinfo.or.kr/index.do?cntyn=0")),
                          m_surveyPath, statusBar());
    return;
  }
  if (actionName == QLatin1String("actionPaleoPortalKigam")) {
    openPaleoPortalDialog(this, QStringLiteral("KIGAM 지오빅데이터 오픈플랫폼 (지질·고지형·원도)"),
                          QUrl(QStringLiteral("https://data.kigam.re.kr/?lang=ko")), m_surveyPath,
                          statusBar());
    return;
  }

  if (actionName == QLatin1String("actionPaleoGeoInfo") ||
      actionName == QLatin1String("actionPaleoKigam")) {
    if (m_surveyPath.isEmpty()) {
      QMessageBox::information(this, QStringLiteral("고지형분석"),
                               QStringLiteral("먼저 「새 조사」로 저장 위치를 만드세요."));
      return;
    }
    QgsProject* proj = QgsProject::instance();
    const QgsRectangle boxWork =
        PaleoAnalysisRunner::surveyExtentRadius(proj, m_canvas, PaleoAnalysisRunner::kRadiusMeters);
    QgsRectangle ext5186 = boxWork;
    const QgsCoordinateReferenceSystem crs5186(QStringLiteral("EPSG:5186"));
    if (proj->crs().isValid() && proj->crs() != crs5186) {
      try {
        const QgsCoordinateTransform tr(proj->crs(), crs5186, proj->transformContext());
        ext5186 = tr.transformBoundingBox(boxWork);
      } catch (...) {
        KA_LOG_EXCEPT();
      }
    }
    const QString surveyDir = QFileInfo(m_surveyPath).absolutePath();
    if (actionName == QLatin1String("actionPaleoGeoInfo")) {
      startReferenceDownload(ReferenceMapKind::GeoInfoBorehole, ext5186,
                             QDir(surveyDir).filePath(QStringLiteral("시추주상도_국토지반정보.gpkg")));
    } else {
      startReferenceDownload(ReferenceMapKind::KigamPaleo, ext5186,
                             QDir(surveyDir).filePath(QStringLiteral("고지형_KIGAM.gpkg")));
    }
    return;
  }

  if (!m_viewTabs) return;
  auto* studio = findChild<KaPaleoStudio*>(QStringLiteral("paleoStudio"));
  if (!studio) {
    studio = new KaPaleoStudio(QgsProject::instance(), m_canvas, this);
    studio->setAttribute(Qt::WA_DeleteOnClose, false);
    connect(studio, &KaPaleoStudio::requestApplyToDrawingSheet, this, [this, studio] {
      openLayoutDesigner();
      PaleoAnalysisRunner::applyEvidenceToSheet(QgsProject::instance(), studio->lastResult());
      if (m_drawingStudio) m_drawingStudio->refreshMapFromProject();
      statusBar()->showMessage(
          QStringLiteral("도면(조판)에 조사구역 반경 10km 고지형 판독 근거 및 기본층서 해설 상자를 추가했습니다."),
          8000);
    });
  }
  studio->setSurveyPath(m_surveyPath);
  studio->setParent(m_viewTabs, Qt::Widget);
  if (m_viewTabs->indexOf(studio) < 0) {
    m_viewTabs->addTab(studio, KaIcons::icon(QStringLiteral("paleo"), KaTheme::tokens().inkMuted),
                       QStringLiteral("고지형분석"));
  }
  m_viewTabs->setCurrentWidget(studio);
  hideSubTools();
  if (studio->lastResult().aoi_summary_text.empty()) {
    QTimer::singleShot(0, studio, &KaPaleoStudio::runAnalysis);
  }
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("고지형 시뮬레이션"));
#endif
}
