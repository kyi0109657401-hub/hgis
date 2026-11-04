#include "core/GeoInfoBoreholeService.h"
#include "core/KigamPaleoService.h"
#include "core/PaleoDemClassifier.h"

#include <QStandardPaths>
#include <QtTest/QtTest>

class TestPaleoSources : public QObject {
  Q_OBJECT
private slots:
  void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

  void parseGeoInfoSichudan_extractsAlluvialAndWeatheredDepths() {
    const QByteArray payload =
        QStringLiteral(
            "{\"jsonView\":\"{\\\"PCODE\\\":\\\"B1600\\\",\\\"ALTITUDE\\\":\\\"32.13\\\","
            "\\\"DEPTH\\\":\\\"35.79\\\",\\\"TM\\\":{\\\"X\\\":\\\"197733.7247\\\","
            "\\\"Y\\\":\\\"552242.2081\\\"},\\\"PNAME\\\":\\\"서대문-세종로간 지반조사\\\","
            "\\\"WATER\\\":\\\"-3.7\\\",\\\"PCOM\\\":\\\"서울 종로구\\\","
            "\\\"HCODE\\\":\\\"B1600BH005\\\",\\\"LAYER\\\":[{\\\"from\\\":0,\\\"to\\\":2.6,"
            "\\\"uscs\\\":\\\"SP\\\",\\\"name\\\":\\\"매립토층\\\",\\\"color\\\":\\\"갈색\\\","
            "\\\"desc\\\":\\\"모래\\\"},{\\\"from\\\":2.6,\\\"to\\\":5.7,\\\"uscs\\\":\\\"CL\\\","
            "\\\"name\\\":\\\"퇴적토층\\\",\\\"color\\\":\\\"암회색\\\",\\\"desc\\\":\\\"점토 및 실트\\\"},"
            "{\\\"from\\\":5.7,\\\"to\\\":9.6,\\\"uscs\\\":\\\"SM\\\",\\\"name\\\":\\\"풍화토층\\\","
            "\\\"color\\\":\\\"갈색\\\",\\\"desc\\\":\\\"풍화토\\\"}]}\"}")
            .toUtf8();
    GeoInfoBoreholeService::BoreholeRecord rec;
    QString err;
    QVERIFY2(GeoInfoBoreholeService::parseSichudanJson(payload, &rec, &err), qPrintable(err));
    QCOMPARE(rec.holeCode, QStringLiteral("B1600BH005"));
    QCOMPARE(rec.projectCode, QStringLiteral("B1600"));
    QVERIFY(std::abs(rec.tmX - 197733.7247) < 0.01);
    QVERIFY(std::abs(rec.tmY - 552242.2081) < 0.01);
    QVERIFY(std::abs(rec.fillDepthM - 2.6) < 0.01);
    QVERIFY(std::abs(rec.alluvialFromM - 2.6) < 0.01);
    QVERIFY(std::abs(rec.alluvialToM - 5.7) < 0.01);
    QVERIFY(std::abs(rec.alluvialThicknessM - 3.1) < 0.01);
    QVERIFY(std::abs(rec.weatheredDepthM - 5.7) < 0.01);
    QVERIFY(rec.hasWaterLevel);
    QVERIFY(std::abs(rec.waterLevelM - (-3.7)) < 0.01);
    QCOMPARE(rec.dominantUscs, QStringLiteral("CL"));
    QVERIFY(rec.depositEnv.contains(QStringLiteral("점토")));
    QVERIFY(rec.paleoHint.contains(QStringLiteral("생토·풍화대 5.7m")));
  }

  void parseKigamGeoJson_extractsFaultsGroundwaterAndOslTerraces() {
    const QByteArray uwJson =
        QStringLiteral(
            "{\"type\":\"FeatureCollection\",\"features\":[{\"type\":\"Feature\","
            "\"geometry\":{\"type\":\"Point\",\"coordinates\":[127.4285,35.9744]},"
            "\"properties\":{\"시험정공번\":\"GNW00022\",\"표고(m)\":49,"
            "\"평균수위(GL.-m)\":\"2.6\",\"대수층유형\":\"충적층\"}}]}")
            .toUtf8();
    const auto uw = KigamPaleoService::parseUnderwaterGeoJson(uwJson);
    QCOMPARE(uw.size(), 1);
    QCOMPARE(uw[0].name, QStringLiteral("GNW00022"));
    QVERIFY(std::abs(uw[0].altitudeM - 49.0) < 0.01);
    QVERIFY(std::abs(uw[0].waterGlM - 2.6) < 0.01);

    const QByteArray oslJson =
        QStringLiteral(
            "{\"type\":\"FeatureCollection\",\"features\":[{\"type\":\"Feature\","
            "\"properties\":{\"노두명(시료명)\":\"1104UM-1\","
            "\"위도\":\"36°05'48.442\\\"\",\"경도\":\"129°25'42.017\\\"\","
            "\"연대(ka)\":\"79.6±2.6\",\"측정방법\":\"OSL\","
            "\"시료해발고도(m)\":\"7.6\",\"단구면해발고도(m)\":\"10-25\"}}]}")
            .toUtf8();
    const auto osl = KigamPaleoService::parseUpliftGeoJson(oslJson);
    QCOMPARE(osl.size(), 1);
    QCOMPARE(osl[0].name, QStringLiteral("1104UM-1"));
    QCOMPARE(osl[0].ageKa, QStringLiteral("79.6±2.6"));
    QVERIFY(osl[0].summary.contains(QStringLiteral("OSL")));
    QVERIFY(KigamPaleoService::reportPdfUrl(QStringLiteral("HD05")).contains(QStringLiteral("HD05.pdf")));
    QVERIFY(KigamPaleoService::originalMapPdfUrl(QStringLiteral("HD05"))
                .contains(QStringLiteral("HD05_org_map.pdf")));
  }

  void paleoDemClassifier_distinguishesTerracesAndAlluvialEnvironments() {
    PaleoDemClassifier::Metrics terraceMetrics;
    terraceMetrics.hasDem = true;
    terraceMetrics.meanElevM = 48.0;
    terraceMetrics.localReliefM = 22.5;
    terraceMetrics.meanSlopeDeg = 3.2;
    terraceMetrics.riverDistM = 180.0;
    const auto t2 = PaleoDemClassifier::classify(QStringLiteral("하안단구"), terraceMetrics);
    QCOMPARE(t2.kind, QStringLiteral("하안단구"));
    QVERIFY(t2.note.contains(QStringLiteral("중위단구(T2")));
    QVERIFY(t2.note.contains(QStringLiteral("하천거리 180m")));

    PaleoDemClassifier::Metrics leveeMetrics;
    leveeMetrics.hasDem = true;
    leveeMetrics.meanElevM = 14.2;
    leveeMetrics.localReliefM = 2.8;
    leveeMetrics.meanSlopeDeg = 1.1;
    leveeMetrics.hasBorehole = true;
    leveeMetrics.boreholeCode = QStringLiteral("BH-1");
    leveeMetrics.alluvialThicknessM = 4.5;
    leveeMetrics.weatheredDepthM = 6.2;
    leveeMetrics.dominantUscs = QStringLiteral("SM");
    leveeMetrics.depositEnv = QStringLiteral("사질 충적(자연제방·미고지)");
    const auto levee = PaleoDemClassifier::classify(QStringLiteral("미저지"), leveeMetrics);
    QCOMPARE(levee.kind, QStringLiteral("자연제방"));
    QVERIFY(levee.note.contains(QStringLiteral("시추 BH-1 충적 4.5m(SM)·생토 6.2m")));
  }
};

QTEST_GUILESS_MAIN(TestPaleoSources)
#include "test_paleo_sources.moc"
