// 「조용한 도판」 시굴격자 속성 (2차 보드 판 5 새 표시): the 시굴 비율 on a 0 … 2×목표 bar with the 목표 marked, and a dial
// beside 회전(방위) whose needle shows the turn from north. 새 모양 only; the stock window keeps its sentence.
#include <QtTest>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>

#include <qgsapplication.h>
#include <qgsgeometry.h>
#include <qgsrectangle.h>

#include "app/KaAzimuthDial.h"
#include "app/KaTargetMeter.h"
#include "app/KaTrenchDialog.h"

namespace {
QByteArray squareWkb(double size) { return QgsGeometry::fromRect(QgsRectangle(0, 0, size, size)).asWkb(); }
bool ink(const QColor& c) { return c.lightness() < 70; }
}  // namespace

class TestTrenchPlate : public QObject {
  Q_OBJECT
private slots:
  void cleanup() { qApp->setProperty("kaPaperLook", QVariant()); }

  // 10.2 % against 목표 10 %: the fill reaches just past the middle, where the 목표 mark stands.
  void meterFillsToTheRatioAndMarksTheTarget() {
    KaTargetMeter meter;
    meter.resize(512, meter.sizeHint().height());
    meter.setTarget(10.0);
    meter.setValue(10.2);
    const QImage image = meter.grab().toImage();
    QVERIFY(ink(image.pixelColor(128, 2)));    // filled
    QVERIFY(!ink(image.pixelColor(384, 2)));   // the rest of the track
    QVERIFY(ink(image.pixelColor(256, 8)));    // the 목표 mark below the track
  }

  // The needle points along the turn: up at 0°, right at 90°.
  void dialNeedleFollowsTheTurn() {
    KaAzimuthDial dial;
    dial.resize(40, 40);
    dial.setAzimuth(0.0);
    QImage image = dial.grab().toImage();
    QVERIFY(ink(image.pixelColor(20, 13)));
    QVERIFY(!ink(image.pixelColor(27, 20)));
    dial.setAzimuth(90.0);
    image = dial.grab().toImage();
    QVERIFY(ink(image.pixelColor(27, 20)));
    QVERIFY(!ink(image.pixelColor(20, 13)));
    QCOMPARE(KaAzimuthDial::note(0.0), QStringLiteral("위가 북쪽입니다"));
    QCOMPARE(KaAzimuthDial::note(90.0), QStringLiteral("북에서 시계 방향 90°"));
  }

  // 사면 방향 안내 (사용자 요청 2026-10-05): no terrain data and flat ground are told apart; both lay north-south.
  void terrainNoteSaysWhyTheGridRunsNorthSouth() {
    KaTrenchDialog dialog;
    const auto note = [&dialog] {
      QString all;
      for (auto* label : dialog.findChildren<QLabel*>(QStringLiteral("dialogHint"))) all += label->text();
      return all;
    };
    dialog.setTerrainAspect({});  // no DEM and no contour with a 표고
    QVERIFY2(note().contains(QStringLiteral("지형 자료가 없어 남북으로")), qPrintable(note()));
    TrenchGridGenerator::SlopeAspect flat;
    flat.samples = 40;
    flat.slopePct = 0.2;
    dialog.setTerrainAspect(flat);
    QVERIFY2(note().contains(QStringLiteral("평지")) && note().contains(QStringLiteral("남북으로")), qPrintable(note()));
  }

  // ㄱ·ㄴ자 시굴트렌치 (사용자 요청 2026-10-19): the 모양 choice lays whole 15 m + 15 m Ls just over the 10%
  // area, bent the way the chosen 모양 says.
  void lShapeChoiceLaysTheTenPercentPlan() {
    KaTrenchDialog dialog;
    dialog.setArea(squareWkb(200), 40000.0);
    auto* shape = dialog.findChild<QComboBox*>(QStringLiteral("trenchShape"));
    QVERIFY(shape);
    QCOMPARE(shape->count(), 5);  // 곧은 트렌치, then ㄴ, ㄱ and their mirrors
    shape->setCurrentIndex(2);
    QVERIFY(shape->currentText().startsWith(QStringLiteral("ㄱ자")));
    QCOMPARE(dialog.spec().armLength, 15.0);
    QCOMPARE(dialog.spec().armBend, 1);
    QTRY_VERIFY_WITH_TIMEOUT(!dialog.planPending(), 20000);
    const QString summary = dialog.findChild<QLabel*>(QStringLiteral("trenchSummary"))->text();
    QVERIFY2(summary.contains(QStringLiteral("ㄱ·ㄴ자")) && summary.contains(QStringLiteral("팔 15 m + 15 m")) &&
                 summary.contains(QStringLiteral("(목표 10%)")),
             qPrintable(summary));
  }

  // 「좌표격자에 맞추기」 (사용자 요청 2026-10-20): with a metre coordinate grid on the map and a ㄱ·ㄴ자 shape, the
  // check lays the Ls on the grid's nodes and turn; without a grid, or for straight trenches, it is off.
  void gridLockLaysTheLsOnTheCoordinateGrid() {
    KaTrenchDialog dialog;
    dialog.setArea(squareWkb(200), 40000.0);
    auto* lock = dialog.findChild<QCheckBox*>(QStringLiteral("trenchGridLock"));
    auto* shape = dialog.findChild<QComboBox*>(QStringLiteral("trenchShape"));
    QVERIFY(lock && shape);
    shape->setCurrentIndex(1);
    QVERIFY(!lock->isEnabled());  // no coordinate grid on the map yet
    dialog.setCoordinateGrid(true, TrenchGridSnap::Grid{0.0, 0.0, 20.0, 5.0});
    QVERIFY(lock->isEnabled());
    lock->setChecked(true);
    QVERIFY(dialog.gridLock() && dialog.gridLock()->step == 5.0);
    QTRY_VERIFY_WITH_TIMEOUT(!dialog.planPending(), 20000);
    const QString summary = dialog.findChild<QLabel*>(QStringLiteral("trenchSummary"))->text();
    QVERIFY2(summary.contains(QStringLiteral("좌표격자 5 m")) && summary.contains(QStringLiteral("방위 20°")),
             qPrintable(summary));
    shape->setCurrentIndex(0);
    QVERIFY(!lock->isEnabled() && !dialog.gridLock());
  }

  // 표준 규격 (사용자 요청 2026-10-27): the 10% plan says how many 20 m and 10 m trenches it lays, and no length
  // of its own; 1000 ㎡ of a 100 m square is 23 × 20 m + 4 × 10 m.
  void ratioSummaryCountsTwentyAndTenMetreTrenches() {
    KaTrenchDialog dialog;
    dialog.setArea(squareWkb(100), 10000.0);
    QTRY_VERIFY_WITH_TIMEOUT(!dialog.planPending(), 20000);
    const QString summary = dialog.findChild<QLabel*>(QStringLiteral("trenchSummary"))->text();
    QVERIFY2(summary.contains(QStringLiteral("20 m 23개 · 10 m 4개")) && summary.contains(QStringLiteral("비율 10.0%")),
             qPrintable(summary));
  }

  void paperWindowCarriesTheMeterAndTheDial() {
    qApp->setProperty("kaPaperLook", true);
    KaTrenchDialog dialog;
    dialog.setArea(squareWkb(100), 10000.0);
    auto* dial = dialog.findChild<KaAzimuthDial*>();
    QVERIFY(dialog.findChild<KaTargetMeter*>() && dial);
    QDoubleSpinBox* azimuth = nullptr;
    for (auto* spin : dialog.findChildren<QDoubleSpinBox*>())
      if (spin->suffix().contains(QStringLiteral("°"))) azimuth = spin;
    azimuth->setValue(30.0);
    QCOMPARE(dial->azimuth(), 30.0);
    QTRY_VERIFY_WITH_TIMEOUT(!dialog.planPending(), 20000);
    qApp->setProperty("kaPaperLook", QVariant());
    KaTrenchDialog stock;
    QVERIFY(!stock.findChild<KaTargetMeter*>() && !stock.findChild<KaAzimuthDial*>());
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable(
      "QGIS_PREFIX_PATH", QFile::exists(QStringLiteral("A:/OSGeo4W/apps/qgis-dev"))
                              ? QStringLiteral("A:/OSGeo4W/apps/qgis-dev")
                              : QStringLiteral("C:/OSGeo4W/apps/qgis-dev"));
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
  QgsApplication::initQgis();
  TestTrenchPlate tc;
  const int rc = QTest::qExec(&tc, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}

#include "test_trench_plate.moc"
