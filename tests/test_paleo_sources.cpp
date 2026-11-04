#include "core/GeoInfoBoreholeService.h"
#include "core/HgisPaleoEngine.h"
#include "core/KigamPaleoService.h"
#include "core/PaleoAnalysisRunner.h"
#include "core/PaleoDemClassifier.h"
#include "core/PaleoLandformService.h"

#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest/QtTest>
#include <qgsapplication.h>
#include <qgsfeature.h>
#include <qgsfield.h>
#include <qgsfields.h>
#include <qgsgeometry.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgsvectorlayer.h>

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

  void seedInterpretationFromSoil_splitsMultiPolygonsIntoGpkg() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString gpkg = dir.filePath(QStringLiteral("paleo_multipoly.gpkg"));
    QgsProject* proj = QgsProject::instance();
    proj->clear();

    QgsVectorLayer soil(QStringLiteral("MultiPolygon?crs=EPSG:5186"), QStringLiteral("토양도"),
                        QStringLiteral("memory"));
    soil.dataProvider()->addAttributes(
        {QgsField(QStringLiteral("soil_type_geo"), QMetaType::Type::QString)});
    soil.updateFields();
    QgsFeature f(soil.fields());
    f.setGeometry(QgsGeometry::fromWkt(QStringLiteral(
        "MULTIPOLYGON(((200000 500000, 200200 500000, 200200 500200, 200000 500200, 200000 500000)),"
        "((200400 500000, 200500 500000, 200500 500100, 200400 500100, 200400 500000)))")));
    f.setAttribute(QStringLiteral("soil_type_geo"), QStringLiteral("08"));
    QgsFeatureList list{f};
    QVERIFY(soil.dataProvider()->addFeatures(list));

    QString err;
    QgsVectorLayer* paleo = PaleoLandformService::ensureInterpretationLayer(proj, gpkg, &err);
    QVERIFY2(paleo != nullptr, qPrintable(err));
    const auto res = PaleoLandformService::seedInterpretationFromSoil(&soil, paleo, &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QVERIFY(res.added >= 1);
    QVERIFY(paleo->featureCount() >= 2);
    proj->clear();
  }

  void hgisPaleoEngine_runsFullPipelineAndRegistersDemLayers() {
    using namespace hgis::paleo;
    GridTransform tf{200000.0, 500000.0, 10.0, 32, 32};
    const size_t n = 32 * 32;
    std::vector<double> zCurr(n, 16.0);
    std::vector<SoilLandformCode> soilGrid(n, SoilLandformCode::FluvialPlain);
    std::vector<GeologyCode> geolGrid(n, GeologyCode::QaAlluvium);
    std::vector<uint8_t> riverMask(n, 0);
    std::vector<uint8_t> aoiMask(n, 1);
    riverMask[0] = 1;

    std::vector<BoreholeRawInput> bhs = {
        {"BH-1",
         200150.0,
         499850.0,
         16.8,
         3.6,
         {{0.0, 2.2, "성토층", "SM", "매립", 6.0},
          {2.2, 5.4, "충적층", "SM", "모래 및 유기물", 9.0},
          {5.4, 8.5, "풍화토", "SC", "풍화대", 50.0}}},
    };

    const PaleoAnalysisResult out =
        HgisPaleoEngine::run_pipeline(zCurr, tf, bhs, soilGrid, geolGrid, riverMask, aoiMask,
                                      {200160.0, 499840.0}, "형산강", "06 하성평탄지", "Qa 충적층");
    QCOMPARE(out.parsed_boreholes.size(), size_t(1));
    QVERIFY(out.parsed_boreholes[0].has_organic_inclusion);
    QCOMPARE(out.paleo_rgba_buffer.size(), n * 4);
    QCOMPARE(out.center_cross_section.dist_m.size(), size_t(32));
    QVERIFY(QString::fromUtf8(out.aoi_summary_text.c_str()).contains(QStringLiteral("형산강")));
    QVERIFY(QString::fromUtf8(out.aoi_summary_text.c_str()).contains(QStringLiteral("반경 10km")));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString demTif = dir.filePath(QStringLiteral("sample_dem.tif"));
    QVERIFY(PaleoAnalysisRunner::writeFloatGeoTiff(demTif, tf, QString(), zCurr));
    QgsProject* proj = QgsProject::instance();
    proj->clear();
    QVERIFY(PaleoAnalysisRunner::registerDemWideLayers(
        proj, nullptr, demTif, dir.filePath(QStringLiteral("survey.gpkg"))));
    QVERIFY(!proj->mapLayersByName(QStringLiteral("[DEM] 국토지리원_DEM.tif")).isEmpty());
    QVERIFY(!proj->mapLayersByName(QStringLiteral("[DEM] 광역지형도(채색+음영기복)")).isEmpty());
    QVERIFY(!proj->mapLayersByName(QStringLiteral("[DEM] 경사도")).isEmpty());
    // 재호출 시에도 열린 레이어 잠금(Permission denied) 없이 정상 갱신되어야 한다.
    QVERIFY(PaleoAnalysisRunner::registerDemWideLayers(
        proj, nullptr, demTif, dir.filePath(QStringLiteral("survey.gpkg"))));
    QVERIFY(PaleoAnalysisRunner::findPreferredDem(proj) != nullptr);
    proj->clear();
  }

  void buildNgiiDemFromTopographic_interpolatesBareEarthDemAndFeedsPaleoPipeline() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString surveyGpkg = dir.filePath(QStringLiteral("survey.gpkg"));
    QgsProject* proj = QgsProject::instance();
    proj->clear();
    proj->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));

    // 3D 등고선(elevation 속성 포함)이 들어있는 수치지형도 레이어를 구성한다.
    auto* topo = new QgsVectorLayer(QStringLiteral("MultiLineStringZ?crs=EPSG:5187"),
                                    QStringLiteral("수치지형도"), QStringLiteral("memory"));
    topo->dataProvider()->addAttributes({
        QgsField(QStringLiteral("Layer"), QMetaType::Type::QString),
        QgsField(QStringLiteral("elevation"), QMetaType::Type::Double),
        QgsField(QStringLiteral("z_valid"), QMetaType::Type::Int),
    });
    topo->updateFields();
    QgsFeatureList cFeats;
    for (int i = 0; i < 6; ++i) {
      const double y = 360000.0 + i * 100.0;
      const double elev = 20.0 + i * 5.0;
      QgsFeature f(topo->fields());
      f.setGeometry(QgsGeometry::fromWkt(
          QStringLiteral("MULTILINESTRING Z ((219000 %1 %2, 219300 %1 %2, 219600 %1 %2))")
              .arg(y, 0, 'f', 1)
              .arg(elev, 0, 'f', 1)));
      f.setAttribute(QStringLiteral("Layer"), QStringLiteral("F0017111"));
      f.setAttribute(QStringLiteral("elevation"), elev);
      f.setAttribute(QStringLiteral("z_valid"), 1);
      cFeats.append(f);
    }
    QVERIFY(topo->dataProvider()->addFeatures(cFeats));
    topo->updateExtents();
    QVERIFY(proj->addMapLayer(topo));

    QString err;
    const QString builtDem =
        PaleoAnalysisRunner::buildNgiiDemFromTopographic(proj, nullptr, surveyGpkg, &err);
    QVERIFY2(!builtDem.isEmpty(), qPrintable(err));
    QVERIFY(QFileInfo::exists(builtDem));
    QgsRasterLayer* ngiiDem = PaleoAnalysisRunner::findPreferredDem(proj);
    QVERIFY(ngiiDem != nullptr);
    QCOMPARE(ngiiDem->name(), QStringLiteral("[DEM] 국토지리원_DEM.tif"));

    // 토양도·지질도·수계도·시추공 레이어를 올리고 runFullPipeline이 실제 공간 속성을 읽는지 검증한다.
    auto* soil = new QgsVectorLayer(QStringLiteral("MultiPolygon?crs=EPSG:5187"),
                                    QStringLiteral("토양도(흙토람)"), QStringLiteral("memory"));
    soil->dataProvider()->addAttributes(
        {QgsField(QStringLiteral("soil_type_geo"), QMetaType::Type::QString)});
    soil->updateFields();
    {
      QgsFeature sf(soil->fields());
      sf.setGeometry(QgsGeometry::fromWkt(QStringLiteral(
          "MULTIPOLYGON(((219000 360000, 219600 360000, 219600 360500, 219000 360500, 219000 360000)))")));
      sf.setAttribute(QStringLiteral("soil_type_geo"), QStringLiteral("08"));
      QgsFeatureList sfList{sf};
      QVERIFY(soil->dataProvider()->addFeatures(sfList));
    }
    QVERIFY(proj->addMapLayer(soil));

    auto* river = new QgsVectorLayer(QStringLiteral("MultiPolygon?crs=EPSG:5187"),
                                     QStringLiteral("수계도(하천망)"), QStringLiteral("memory"));
    river->dataProvider()->addAttributes(
        {QgsField(QStringLiteral("riv_nm"), QMetaType::Type::QString)});
    river->updateFields();
    {
      QgsFeature rf(river->fields());
      rf.setGeometry(QgsGeometry::fromWkt(QStringLiteral(
          "MULTIPOLYGON(((219000 360000, 219600 360000, 219600 360040, 219000 360040, 219000 360000)))")));
      rf.setAttribute(QStringLiteral("riv_nm"), QStringLiteral("북천"));
      QgsFeatureList rfList{rf};
      QVERIFY(river->dataProvider()->addFeatures(rfList));
    }
    QVERIFY(proj->addMapLayer(river));

    const auto res = PaleoAnalysisRunner::runFullPipeline(proj, nullptr, surveyGpkg, nullptr);
    QVERIFY(QString::fromUtf8(res.aoi_summary_text.c_str()).contains(QStringLiteral("북천")));
    proj->clear();
  }
};

QTEST_MAIN(TestPaleoSources)
#include "test_paleo_sources.moc"
