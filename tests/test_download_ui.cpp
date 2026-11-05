#include <QtTest>
#include <QApplication>
#include <QDir>
#include <QFrame>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QWebEngineProfile>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlRequestInfo>
#include <QWebEngineView>

#include "app/KaDownloadUi.h"
#include "app/KaHeritageBrowser.h"
#include "app/KaTopographicBrowser.h"
#include "core/KaPortableRuntime.h"

namespace {
class OfflineRequests final : public QWebEngineUrlRequestInterceptor {
public:
  explicit OfflineRequests(QObject* parent) : QWebEngineUrlRequestInterceptor(parent) {}
  void interceptRequest(QWebEngineUrlRequestInfo& info) override {
    const auto scheme = info.requestUrl().scheme();
    if (scheme != QLatin1String("about") && scheme != QLatin1String("data") &&
        scheme != QLatin1String("qrc")) info.block(true);
  }
};

void restrictRequests(QWidget& dialog) {
  for (auto* profile : dialog.findChildren<QWebEngineProfile*>())
    profile->setUrlRequestInterceptor(new OfflineRequests(profile));
}

QPushButton* buttonWithText(QWidget& parent, const QString& text) {
  for (auto* button : parent.findChildren<QPushButton*>())
    if (button->text() == text) return button;
  return nullptr;
}

bool capture(QWidget& dialog, const QString& name) {
  const auto directory = qEnvironmentVariable("KA_DOWNLOAD_UI_QA");
  if (directory.isEmpty()) return true;
  if (!QDir().mkpath(directory)) return false;
  const auto scale = qEnvironmentVariable("QT_SCALE_FACTOR", QStringLiteral("1"));
  return dialog.grab().save(QDir(directory).filePath(name + QLatin1Char('-') + scale + QStringLiteral(".png")));
}

// Check that the same user-facing status hierarchy fits each actual dialog;
// this catches a shared style being installed without the controls being visible.
void verifyVisibleStatus(QDialog& dialog) {
  QVERIFY(dialog.property("kaDownloadWindow").toBool());
  auto* header = dialog.findChild<QFrame*>(QStringLiteral("downloadHeader"));
  auto* title = dialog.findChild<QLabel*>(QStringLiteral("downloadTitle"));
  auto* card = dialog.findChild<QFrame*>(QStringLiteral("downloadStatusCard"));
  QVERIFY(header); QVERIFY(title); QVERIFY(card);
  QVERIFY(header->isVisible()); QVERIFY(card->isVisible()); QVERIFY(!title->text().isEmpty());
  QVERIFY(dialog.rect().contains(header->geometry()));
  QVERIFY(dialog.rect().contains(card->geometry()));
  const auto progressBars = card->findChildren<QProgressBar*>();
  QCOMPARE(progressBars.size(), 1);
  QVERIFY(progressBars.front()->property("downloadProgress").toBool());
  QVERIFY(progressBars.front()->isVisible());
  QVERIFY(progressBars.front()->isTextVisible());
}
}

class DownloadUiTest final : public QObject {
  Q_OBJECT
private slots:
  void cadastralProgressKeepsCompletionVisibleAndCancelsOnce_data() {
    QTest::addColumn<bool>("closeWindow");
    QTest::newRow("cancel-button") << false;
    QTest::newRow("window-close") << true;
  }

  void cadastralProgressKeepsCompletionVisibleAndCancelsOnce() {
    QFETCH(bool, closeWindow);
    KaDownloadProgressDialog dialog(QStringLiteral("지적도 다운로드"),
        QStringLiteral("VWorld · 조사 주변 5 km의 지적도 자료를 받습니다."));
    QSignalSpy canceled(&dialog, &KaDownloadProgressDialog::canceled);
    dialog.show();
    QTest::qWait(30);
    verifyVisibleStatus(dialog);
    auto* progress = dialog.findChild<QProgressBar*>(QStringLiteral("downloadProgress"));
    QVERIFY(progress);
    QCOMPARE(progress->minimum(), 0);
    QCOMPARE(progress->maximum(), 0);
    dialog.setLabelText(QStringLiteral("지적도 자료를 받고 있습니다. 42 / 100"));
    dialog.setRange(0, 100);
    dialog.setValue(42);
    QCOMPARE(progress->value(), 42);
    auto* detail = dialog.findChild<QLabel*>(QStringLiteral("downloadDetail"));
    QVERIFY(detail);
    QVERIFY(detail->text().contains(QStringLiteral("42 / 100")));
    QVERIFY(capture(dialog, QStringLiteral("cadastral")));
    dialog.setValue(100);
    QCoreApplication::processEvents();
    QCOMPARE(progress->value(), 100);
    QVERIFY(dialog.isVisible()); // Data transfer completion must not hide map preparation.
    QCOMPARE(canceled.count(), 0);
    if (closeWindow) dialog.close();
    else {
      auto* cancel = dialog.findChild<QPushButton*>(QStringLiteral("downloadCancel"));
      QVERIFY(cancel);
      QTest::mouseClick(cancel, Qt::LeftButton);
    }
    QCOMPARE(canceled.count(), 1);
    QVERIFY(!dialog.isVisible());
    dialog.reject();
    QCOMPARE(canceled.count(), 1);
  }

  void heritageDetailsPreserveWaitingAndIdleCancelIsHarmless() {
    KaHeritageBrowser dialog;
    restrictRequests(dialog);
    QSignalSpy finished(&dialog, &KaHeritageBrowser::allFinished);
    QSignalSpy failed(&dialog, &KaHeritageBrowser::failed);
    QSignalSpy stage(&dialog, &KaHeritageBrowser::stageChanged);
    dialog.showWaiting(QStringLiteral("조사 주변 자료 범위를 확인하고 있습니다."));
    dialog.show();
    QTest::qWait(30);
    verifyVisibleStatus(dialog);
    auto* details = dialog.findChild<QPushButton*>(QStringLiteral("heritageDetails"));
    auto* tabs = dialog.findChild<QTabWidget*>();
    auto* outline = dialog.findChild<QPlainTextEdit*>();
    auto* cancel = buttonWithText(dialog, QStringLiteral("취소"));
    QVERIFY(details); QVERIFY(tabs); QVERIFY(outline); QVERIFY(cancel);
    QVERIFY(!tabs->isVisible()); QVERIFY(!outline->isVisible());
    QVERIFY(capture(dialog, QStringLiteral("heritage")));
    QTest::mouseClick(details, Qt::LeftButton);
    QVERIFY(tabs->isVisible()); QVERIFY(outline->isVisible());
    verifyVisibleStatus(dialog);
    QTest::mouseClick(details, Qt::LeftButton);
    QVERIFY(!tabs->isVisible()); QVERIFY(!outline->isVisible());
    QTest::mouseClick(cancel, Qt::LeftButton);
    dialog.stop();
    QCOMPARE(dialog.stage(), HeritageStage::Idle);
    QCOMPARE(finished.count(), 0); QCOMPARE(failed.count(), 0); QCOMPARE(stage.count(), 0);
    // Active-transfer cancellation is covered by HeritageDownloadRetryTest;
    // this fixture deliberately never starts login or accesses a real server.
  }

  void topographicDetailsKeepPreparationAndCancelOnlyOnce() {
    QTemporaryDir downloads;
    QVERIFY(downloads.isValid());
    KaTopographicBrowser dialog(nullptr, downloads.path());
    restrictRequests(dialog);
    // Public navigation suppresses the constructor's queued official home URL.
    dialog.navigate(QUrl(QStringLiteral("about:blank")));
    dialog.setCompactMode(true);
    dialog.setProcessing(true);
    dialog.setPreparationProgress(QStringLiteral("지도에 올리기"), QStringLiteral("받은 도엽을 지도 자료로 준비하고 있습니다."));
    QSignalSpy canceled(&dialog, &KaTopographicBrowser::cancelRequested);
    dialog.show();
    QTest::qWait(60);
    verifyVisibleStatus(dialog);
    auto* expand = dialog.findChild<QPushButton*>(QStringLiteral("topographicExpandOfficial"));
    auto* details = dialog.findChild<QScrollArea*>(QStringLiteral("topographicDetailsScroll"));
    auto* phase = dialog.findChild<QLabel*>(QStringLiteral("topographicCurrentStage"));
    auto* cancel = dialog.findChild<QPushButton*>(QStringLiteral("topographicCompactCancel"));
    QVERIFY(expand); QVERIFY(details); QVERIFY(phase); QVERIFY(cancel);
    auto* trail = dialog.findChild<QLabel*>(QStringLiteral("topographicStageTrail"));
    auto* summary = dialog.findChild<QLabel*>(QStringLiteral("topographicTransferSummary"));
    QVERIFY(trail); QVERIFY(summary);
    QCOMPARE(expand->text(), QStringLiteral("상세 보기"));
    QCOMPARE(dialog.windowType(), Qt::Dialog);
    QVERIFY(!details->isVisible());
    QVERIFY(!trail->isVisible()); QVERIFY(!summary->isVisible());
    QVERIFY(phase->isVisible());
    QVERIFY(phase->text().contains(QStringLiteral("지도에 올리기")));
    QVERIFY(capture(dialog, QStringLiteral("topographic")));
    QTest::mouseClick(expand, Qt::LeftButton);
    QVERIFY(details->isVisible());
    verifyVisibleStatus(dialog);
    QVERIFY(phase->isVisible());
    QVERIFY(trail->isVisible()); QVERIFY(summary->isVisible());
    QCOMPARE(expand->text(), QStringLiteral("상세 접기"));
    QVERIFY(capture(dialog, QStringLiteral("topographic-details")));
    QCOMPARE(canceled.count(), 0);
    QTest::mouseClick(expand, Qt::LeftButton);
    QVERIFY(!details->isVisible());
    QVERIFY(!trail->isVisible()); QVERIFY(!summary->isVisible());
    QCOMPARE(expand->text(), QStringLiteral("상세 보기"));
    QVERIFY(phase->text().contains(QStringLiteral("지도에 올리기")));
    for (auto* view : dialog.findChildren<QWebEngineView*>())
      QVERIFY(view->url().isEmpty() || view->url() == QUrl(QStringLiteral("about:blank")));
    QTest::mouseClick(cancel, Qt::LeftButton);
    QCOMPARE(canceled.count(), 1);
    QVERIFY(!dialog.isVisible());
    dialog.reject();
    QCOMPARE(canceled.count(), 1);
  }

  void ngiiDemAutomationUsesDemEndpointsAndMapType4() {
    QFile scriptFile(QStringLiteral(":/providers/ngii-topographic.js"));
    if (!scriptFile.exists()) scriptFile.setFileName(QStringLiteral("data/providers/ngii-topographic.js"));
    QVERIFY(scriptFile.open(QIODevice::ReadOnly));
    const QString script = QString::fromUtf8(scriptFile.readAll());
    QVERIFY2(script.contains(QLatin1String("input.product==='dem'")) ||
                 script.contains(QLatin1String("input.product === 'dem'")),
             "ngii-topographic.js must branch on product==='dem' for NGII DEM downloads");
    QVERIFY2(script.contains(QLatin1String("/ms/map/demQuery.do")),
             "DEM mode must query /ms/map/demQuery.do instead of /ms/map/suchiQuery.do");
    QVERIFY2(script.contains(QLatin1String("section.mapList.dem")) &&
                 script.contains(QLatin1String("cb_dem")),
             "DEM mode must select checkboxes in section.mapList.dem (input[name=cb_dem])");
    QVERIFY2(script.contains(QLatin1String("onBasis")),
             "DEM mode must populate onBasis (상세용도) required for mapType 4");
    QVERIFY2(script.contains(QLatin1String("/pd/shbtManage/shbtInsertTritVidoDem.do")),
             "DEM mode must observe /pd/shbtManage/shbtInsertTritVidoDem.do order completion");
    QVERIFY2(script.contains(QLatin1String("toSel.options")) &&
                 script.contains(QLatin1String("o.value==='0'")),
             "DEM mode must ensure #toDemYear has option value='0' before selecting it");
  }
};

int main(int argc, char** argv) {
  QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
  KaPortableRuntime::applyWebEngineFlags();
  QApplication app(argc, argv);
  QCoreApplication::setOrganizationName(QStringLiteral("ka-hgis-offline-tests"));
  QCoreApplication::setApplicationName(QStringLiteral("download-ui"));
  QStandardPaths::setTestModeEnabled(true);
  DownloadUiTest test;
  return QTest::qExec(&test, argc, argv);
}

#include "test_download_ui.moc"
