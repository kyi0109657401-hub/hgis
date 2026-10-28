// 시굴격자 한 가지 길이 (사용자 요청 2026-10-28): the trench length is chosen (20 m, 15 m or 10 m, 2 m wide) and the grid
// is made of that length only: no mixing, no edge pieces, no length averaged down to the target (the old 9.84 m
// trenches). The 10%/2% plan is the count of whole trenches nearest the target, within one trench of it either way.
#include <cmath>
#include <memory>

#include <QtTest>
#include <QCoreApplication>

#include "core/TrenchGridGenerator.h"
#include "core/TrenchPlanCache.h"

#include <ogr_api.h>
#include <ogr_geometry.h>

namespace {
struct GeometryDeleter {
  void operator()(OGRGeometry* geometry) const { OGRGeometryFactory::destroyGeometry(geometry); }
};
using Geometry = std::unique_ptr<OGRGeometry, GeometryDeleter>;

QByteArray wktWkb(const QString& wkt) {
  OGRGeometry* geometry = nullptr;
  OGRGeometryFactory::createFromWkt(wkt.toLatin1().constData(), nullptr, &geometry);
  Geometry owner(geometry);
  if (!owner) return {};
  QByteArray result(int(owner->WkbSize()), '\0');
  owner->exportToWkb(wkbNDR, reinterpret_cast<unsigned char*>(result.data()));
  return result;
}

QByteArray squareWkb(double x0, double y0, double x1, double y1) {
  return wktWkb(QStringLiteral("POLYGON((%1 %2,%3 %2,%3 %4,%1 %4,%1 %2))")
                    .arg(x0, 0, 'f', 6).arg(y0, 0, 'f', 6).arg(x1, 0, 'f', 6).arg(y1, 0, 'f', 6));
}

OGRPolygon cellPolygon(const TrenchGridGenerator::Cell& cell) {
  OGRLinearRing ring;
  for (const auto& point : cell.ring) ring.addPoint(point.first, point.second);
  OGRPolygon polygon;
  polygon.addRing(&ring);
  return polygon;
}
}  // namespace

class TestTrenchStandard : public QObject {
  Q_OBJECT
private slots:
  void ratioPlanUsesOnlyTheChosenLength_data();
  void ratioPlanUsesOnlyTheChosenLength();
  void buildInAreaKeepsOnlyWholeTrenches();
  void targetCheckAllowsLessThanOneTrenchEitherWay();
  void planCacheKeepsLengthsApart();
};

void TestTrenchStandard::ratioPlanUsesOnlyTheChosenLength_data() {
  QTest::addColumn<QByteArray>("areaWkb");
  QTest::addColumn<double>("target");
  QTest::addColumn<double>("azimuth");
  QTest::addColumn<double>("length");
  QTest::newRow("100 m square 20 m") << squareWkb(0, 0, 100, 100) << 10.0 << 0.0 << 20.0;
  QTest::newRow("5187 trial 15 m") << squareWkb(200000, 450000, 200200, 450200) << 10.0 << 0.0 << 15.0;
  QTest::newRow("sample 2% 10 m") << squareWkb(200000, 450000, 200200, 450200) << 2.0 << 0.0 << 10.0;
  QTest::newRow("slope 45° 20 m") << squareWkb(200000, 450000, 200100, 450100) << 10.0 << 45.0 << 20.0;
  QTest::newRow("concave 15 m") << wktWkb(QStringLiteral("POLYGON((0 0,100 0,100 30,30 30,30 100,0 100,0 0))")) << 10.0
                                << 0.0 << 15.0;
  QTest::newRow("hole 10 m") << wktWkb(QStringLiteral("POLYGON((0 0,100 0,100 100,0 100,0 0),(30 30,30 70,70 70,70 30,30 30))"))
                             << 10.0 << 0.0 << 10.0;
  // A long road corridor like the 경주 sewer survey: 100 m wide, 900 m long, with a narrow island.
  QTest::newRow("corridor 20 m") << wktWkb(QStringLiteral(
      "POLYGON((219700 360500,219800 360500,219800 361400,219700 361400,219700 360500),"
      "(219715 360720,219724 360720,219724 361030,219715 361030,219715 360720))")) << 10.0 << 0.0 << 20.0;
}

void TestTrenchStandard::ratioPlanUsesOnlyTheChosenLength() {
  QFETCH(QByteArray, areaWkb);
  QFETCH(double, target);
  QFETCH(double, azimuth);
  QFETCH(double, length);
  const auto plan = TrenchGridGenerator::buildForTargetRatio(areaWkb, target, 2.0, azimuth, 0.0, 0, length);
  QVERIFY2(!plan.cells.empty(), qPrintable(plan.error));
  QVERIFY(TrenchGridGenerator::cellsValid(plan.cells));
  OGRGeometry* raw = nullptr;
  OGRGeometryFactory::createFromWkb(areaWkb.constData(), nullptr, &raw, size_t(areaWkb.size()));
  Geometry area(raw);
  Geometry united;
  double sum = 0.0;
  for (const auto& cell : plan.cells) {
    QCOMPARE(cell.width, 2.0);
    QVERIFY2(std::abs(cell.length - length) < 1e-9, qPrintable(QStringLiteral("%1 is %2 m").arg(cell.name).arg(cell.length)));
    OGRPolygon polygon = cellPolygon(cell);
    Geometry outside(polygon.Difference(area.get()));
    QVERIFY(OGR_G_Area(OGRGeometry::ToHandle(outside.get())) < 1e-7);
    sum += polygon.get_Area();
    united.reset(united ? united->Union(&polygon) : polygon.clone());
  }
  const double unionArea = OGR_G_Area(OGRGeometry::ToHandle(united.get()));
  const double targetArea = OGR_G_Area(OGRGeometry::ToHandle(area.get())) * target / 100.0;
  QVERIFY2(std::abs(sum - unionArea) < 1e-6, "trenches must not overlap");
  // The count nearest the target: within one trench of it, either way.
  QVERIFY2(std::abs(unionArea - targetArea) < 2.0 * length,
           qPrintable(QStringLiteral("actual %1, target %2").arg(unionArea).arg(targetArea)));
  QVERIFY(TrenchGridGenerator::meetsTarget(plan.cells, plan.areaM2, target));
  QCOMPARE(plan.length, length);
  QVERIFY(std::abs(plan.ratioPct - unionArea / plan.areaM2 * 100.0) < 1e-9);
}

// A cell that cannot hold the whole trench is left empty: no shorter piece at the edge.
void TestTrenchStandard::buildInAreaKeepsOnlyWholeTrenches() {
  TrenchGridGenerator::Spec spec;
  spec.trenchWidth = 2.0;
  spec.trenchLength = 20.0;
  spec.balkWidth = 10.0;  // rows at 0, 30, 60, 90: the last row has only 15 m of room
  const auto cells = TrenchGridGenerator::buildInArea(spec, squareWkb(0, 0, 100, 105));
  QCOMPARE(int(cells.size()), 27);
  for (const auto& cell : cells) QCOMPARE(cell.length, 20.0);
  spec.trenchLength = 15.0;  // 15 m fits in the last row
  const auto fifteen = TrenchGridGenerator::buildInArea(spec, squareWkb(0, 0, 100, 105));
  for (const auto& cell : fifteen) QCOMPARE(cell.length, 15.0);
  QVERIFY(!TrenchGridGenerator::buildInArea(spec, squareWkb(0, 0, 12, 14)).size());  // nothing whole fits
}

void TestTrenchStandard::targetCheckAllowsLessThanOneTrenchEitherWay() {
  TrenchGridGenerator::Spec spec;  // 2 × 20 m
  spec.cols = 2;
  const auto two = TrenchGridGenerator::build(spec);  // 80 ㎡
  QVERIFY(TrenchGridGenerator::meetsTarget(two, 800.0, 10.0));    // exactly 10%
  QVERIFY(TrenchGridGenerator::meetsTarget(two, 790.0, 10.0));    // 1 ㎡ over
  QVERIFY(TrenchGridGenerator::meetsTarget(two, 810.0, 10.0));    // 1 ㎡ under
  QVERIFY(!TrenchGridGenerator::meetsTarget(two, 300.0, 10.0));   // 50 ㎡ over: one trench too many
  QVERIFY(!TrenchGridGenerator::meetsTarget(two, 1300.0, 10.0));  // 50 ㎡ under: one trench too few
}

void TestTrenchStandard::planCacheKeepsLengthsApart() {
  TrenchPlanCache::clear();
  const QByteArray area = squareWkb(200000, 450000, 200200, 450200);
  const auto twenty = TrenchPlanCache::ratioPlan(area, 10.0, 2.0, 0.0, 0.0, 0, nullptr, 20.0);
  const auto ten = TrenchPlanCache::ratioPlan(area, 10.0, 2.0, 0.0, 0.0, 0, nullptr, 10.0);
  QCOMPARE(TrenchPlanCache::computeCount(), 2);
  QCOMPARE(twenty.cells.front().length, 20.0);
  QCOMPARE(ten.cells.front().length, 10.0);
  TrenchGridGenerator::RatioFill known;
  QVERIFY(TrenchPlanCache::lookup(area, 10.0, 2.0, 0.0, &known, 0.0, 0, nullptr, 10.0));
  QCOMPARE(known.cells.front().length, 10.0);
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  TestTrenchStandard tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "test_trench_standard.moc"
