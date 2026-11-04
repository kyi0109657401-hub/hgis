#include "KaPaleoMenu.h"
#include "KaIcons.h"
#include "KaUserError.h"
#include "MainWindow.h"
#include "core/GeoInfoBoreholeService.h"
#include "core/KaLogExcept.h"
#include "core/KigamPaleoService.h"
#include "core/LayerOps.h"
#include "core/PaleoDemClassifier.h"
#include "core/PaleoLandformService.h"
#include "core/SoilMapService.h"

#include <QAction>
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QStatusBar>
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
#include <qgslayertree.h>
#include <qgslayertreelayer.h>
#include <qgslayertreeview.h>
#include <qgsmapcanvas.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
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

  auto* topRow = new QHBoxLayout();
  auto* info = new QLabel(
      QStringLiteral("로그인 상태가 앱 안에 유지됩니다. 내려받는 파일은 조사 폴더의 「지형분석」(%1)에 저장됩니다.")
          .arg(QDir::toNativeSeparators(saveDir)),
      dlg);
  info->setWordWrap(true);
  topRow->addWidget(info, 1);
  layout->addLayout(topRow);

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
          QStringLiteral("고지형 가설 판독 (토양 + 공개 DEM·수계·시추 결합)"));
  menu->addSeparator();
  addItem(QStringLiteral("actionPaleoGeoInfo"),
          QStringLiteral("국토지반정보 시추주상도 내려받기 — 현재 화면 범위"));
  addItem(QStringLiteral("actionPaleoKigam"),
          QStringLiteral("KIGAM 단층·지하수위·단구연대 내려받기 — 현재 화면 범위"));
  menu->addSeparator();
  addItem(QStringLiteral("actionPaleoPortalGeoInfo"),
          QStringLiteral("국토지반정보 포털 열기 (geoinfo.or.kr 로그인·검색유통)…"));
  addItem(QStringLiteral("actionPaleoPortalKigam"),
          QStringLiteral("KIGAM 지오빅데이터 포털 열기 (data.kigam.re.kr 원도·도폭)…"));

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

  if (m_surveyPath.isEmpty()) {
    QMessageBox::information(this, QStringLiteral("고지형"),
                             QStringLiteral("먼저 「새 조사」로 저장 위치를 만드세요.\n"
                                            "그다음 조사구역으로 확대한 뒤 다시 「고지형」을 누르면 "
                                            "흙토람 분포지형과 시추·지질 자료가 깔립니다."));
    return;
  }
  if (!m_canvas) return;
  QgsProject* proj = QgsProject::instance();
  LayerOps::clampCanvasToThematicScale(m_canvas);

  QgsRectangle ext5186 = m_canvas->extent();
  const QgsCoordinateReferenceSystem crs5186(QStringLiteral("EPSG:5186"));
  const QgsCoordinateReferenceSystem canvasCrs = m_canvas->mapSettings().destinationCrs();
  if (canvasCrs.isValid() && canvasCrs != crs5186) {
    try {
      const QgsCoordinateTransform tr(canvasCrs, crs5186, QgsProject::instance());
      ext5186 = tr.transformBoundingBox(ext5186);
    } catch (const QgsException&) {
      KA_LOG_EXCEPT();
      KaUserError::warn(this, {QStringLiteral("고지형"), QStringLiteral("고지형을 올리지 못했습니다"),
                               QStringLiteral("화면 범위를 좌표 변환하지 못했습니다."),
                               QStringLiteral("작업 좌표계를 확인한 뒤 다시 누르세요.")});
      return;
    }
  }

  const QString surveyDir = QFileInfo(m_surveyPath).absolutePath();
  if (actionName == QLatin1String("actionPaleoGeoInfo")) {
    const QString outGpkg = QDir(surveyDir).filePath(QStringLiteral("시추주상도_국토지반정보.gpkg"));
    startReferenceDownload(ReferenceMapKind::GeoInfoBorehole, ext5186, outGpkg);
    return;
  }
  if (actionName == QLatin1String("actionPaleoKigam")) {
    const QString outGpkg = QDir(surveyDir).filePath(QStringLiteral("고지형_KIGAM.gpkg"));
    startReferenceDownload(ReferenceMapKind::KigamPaleo, ext5186, outGpkg);
    return;
  }

  QgsVectorLayer* soil = PaleoLandformService::findSoilTerrainLayer(proj);
  if (!soil) {
    if (ext5186.width() > SoilMapService::maxSpanMeters() ||
        ext5186.height() > SoilMapService::maxSpanMeters()) {
      QMessageBox::information(
          this, QStringLiteral("고지형"),
          QStringLiteral("지금 화면이 너무 넓습니다. 조사구역(한 변 %1km 이하)으로 확대한 뒤 "
                         "다시 「고지형」을 누르세요.\n"
                         "전국·시도 화면에는 분포지형을 깔지 않습니다.")
              .arg(SoilMapService::maxSpanMeters() / 1000.0, 0, 'f', 0));
      return;
    }
    const QString outGpkg = QDir(surveyDir).filePath(QStringLiteral("토양도_흙토람.gpkg"));
    startReferenceDownload(ReferenceMapKind::PaleoSoil, ext5186, outGpkg);
    return;
  }
  PaleoLandformService::applyCandidateEmphasis(soil);
  if (QgsLayerTreeLayer* n = proj->layerTreeRoot()->findLayer(soil->id()))
    n->setItemVisibilityChecked(true);
  if (m_layerTree) m_layerTree->setCurrentLayer(soil);

  QString err;
  QgsVectorLayer* layer = PaleoLandformService::ensureInterpretationLayer(proj, m_surveyPath, &err);
  if (!layer) {
    notify(Notice::Warning, QStringLiteral("고지형"),
           err.isEmpty() ? QStringLiteral("판독 레이어를 만들지 못했습니다.") : err);
    return;
  }
  const PaleoLandformService::SeedResult seeded =
      PaleoLandformService::seedInterpretationFromSoil(soil, layer, &err);
  const int enriched = PaleoDemClassifier::enrichInterpretationLayer(proj, layer);
  PaleoLandformService::applyInterpretationStyle(layer);
  beginEdit(layer);
  if (seeded.added > 0) {
    const QString extra =
        enriched > 0
            ? QStringLiteral(" (공개 DEM·하천·시추주상도 지표 %1건 반영)").arg(enriched)
            : QString();
    notify(Notice::Success, QStringLiteral("고지형"),
           QStringLiteral("흙토람 분포지형에서 가설 %1개를 자동으로 깔았습니다%2. 확정이 아닙니다.")
               .arg(seeded.added)
               .arg(extra));
    QMessageBox::information(
        this, QStringLiteral("고지형"),
        QStringLiteral("흙토람 분포지형에서 가설 면을 자동으로 깔았습니다%1.\n\n"
                       "선상지 · 해성평탄 · 하안단구는 토양 구분을 옮긴 것입니다.\n"
                       "하성평탄은 안쪽을 구하도, 가장자리를 자연제방 가설로 나누며,\n"
                       "켜 둔 공개 DEM·수계도·국토지반정보 시추주상도가 있으면 표고·비고·경사·충적층 두께를 판독 근거에 함께 적습니다.\n\n"
                       "옛 지형이 자동으로 복원된 것은 아닙니다. "
                       "틀린 면은 지우고 고치세요. 확정이 아닙니다.")
            .arg(extra));
    statusBar()->showMessage(
        QStringLiteral("고지형 가설 %1개%2 — 확정 아님. 좌클릭으로 추가, 우클릭으로 완료")
            .arg(seeded.added)
            .arg(extra),
        0);
  } else {
    notify(Notice::Success, QStringLiteral("고지형"),
           QStringLiteral("분포지형을 올렸습니다. 진한 색이 입지 후보"
                          "(곡간·선상·해성·하성평탄·홍적대지)입니다."));
    QMessageBox::information(
        this, QStringLiteral("고지형"),
        QStringLiteral("지도에 색 면이 생겼으면 그게 고지형입니다.\n\n"
                       "진한 색 = 유적 입지 후보 (곡간·선상·해성평탄·하성평탄·홍적대지)\n"
                       "연한 색 = 산지·구릉 등\n\n"
                       "옛 지형이 자동으로 복원된 것은 아닙니다. "
                       "이제 그 위에 구하도·자연제방 가설을 그리세요. 확정이 아닙니다."));
    statusBar()->showMessage(
        QStringLiteral("고지형 가설 — 진한 색이 입지 후보. 좌클릭으로 그리고 우클릭으로 완료"), 0);
  }
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("고지형 시뮬레이션"));
#endif
}
