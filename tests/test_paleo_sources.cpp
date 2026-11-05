#include "core/BasemapDsm.h"
#include "core/GeoInfoBoreholeService.h"
#include "core/HgisPaleoEngine.h"
#include "core/KigamPaleoService.h"
#include "core/LayoutService.h"
#include "core/PaleoAnalysisRunner.h"
#include "core/PaleoDemClassifier.h"
#include "core/PaleoLandformService.h"
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest/QtTest>
#include <qgsapplication.h>
#include <qgscolorrampshader.h>
#include <qgsfeature.h>
#include <qgsfield.h>
#include <qgsfields.h>
#include <qgsgeometry.h>
#include <qgslayertreelayer.h>
#include <qgslayertreemodellegendnode.h>
#include <qgslayoutitemlegend.h>
#include <qgsmaplayerlegend.h>
#include <qgsprintlayout.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgssinglebandpseudocolorrenderer.h>
#include <qgsvectorlayer.h>

class TestPaleoSources : public QObject {
  Q_OBJECT
private slots:
  void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

  void parseGeoInfoSichudan_extractsAlluvialAndWeatheredDepths() {
    const QByteArray payload = QStringLiteral(
        "{\"jsonView\":\"{\\\"PCODE\\\":\\\"B1600\\\",\\\"ALTITUDE\\\":\\\"32.13\\\","
        "\\\"DEPTH\\\":\\\"35.79\\\",\\\"TM\\\":{\\\"X\\\":\\\"197733.7247\\\",\\\"Y\\\":\\\"552242.2081\\\"},"
        "\\\"PNAME\\\":\\\"서대문-세종로간 지반조사\\\",\\\"WATER\\\":\\\"-3.7\\\",\\\"PCOM\\\":\\\"서울 종로구\\\","
        "\\\"HCODE\\\":\\\"B1600BH005\\\",\\\"LAYER\\\":[{\\\"from\\\":0,\\\"to\\\":2.6,\\\"uscs\\\":\\\"SP\\\","
        "\\\"name\\\":\\\"매립토층\\\",\\\"color\\\":\\\"갈색\\\",\\\"desc\\\":\\\"모래\\\"},"
        "{\\\"from\\\":2.6,\\\"to\\\":5.7,\\\"uscs\\\":\\\"CL\\\",\\\"name\\\":\\\"퇴적토층\\\","
        "\\\"color\\\":\\\"암회색\\\",\\\"desc\\\":\\\"점토 및 실트\\\"},{\\\"from\\\":5.7,\\\"to\\\":9.6,"
        "\\\"uscs\\\":\\\"SM\\\",\\\"name\\\":\\\"풍화토층\\\",\\\"color\\\":\\\"갈색\\\",\\\"desc\\\":\\\"풍화토\\\"}]}\"}").toUtf8();
    GeoInfoBoreholeService::BoreholeRecord rec;
    QString err;
    QVERIFY2(GeoInfoBoreholeService::parseSichudanJson(payload, &rec, &err), qPrintable(err));
    QCOMPARE(rec.holeCode, QStringLiteral("B1600BH005"));
    QCOMPARE(rec.projectCode, QStringLiteral("B1600"));
    QVERIFY(std::abs(rec.tmX - 197733.7247) < 0.01 && std::abs(rec.tmY - 552242.2081) < 0.01);
    QVERIFY(std::abs(rec.fillDepthM - 2.6) < 0.01 && std::abs(rec.alluvialThicknessM - 3.1) < 0.01);
    QVERIFY(std::abs(rec.weatheredDepthM - 5.7) < 0.01 && rec.hasWaterLevel);
    QCOMPARE(rec.dominantUscs, QStringLiteral("CL"));
    QVERIFY(rec.depositEnv.contains(QStringLiteral("점토")) && rec.paleoHint.contains(QStringLiteral("생토·풍화대 5.7m")));

    const QByteArray emptyLayerPayload = QStringLiteral("{\"HCODE\":\"B1791BH062\",\"TM\":{\"X\":\"219100\",\"Y\":\"360100\"},\"LAYER\":[]}").toUtf8();
    QVERIFY(!GeoInfoBoreholeService::parseSichudanJson(emptyLayerPayload, &rec, &err));

    const QByteArray realNamesPayload = QStringLiteral(
        "{\"HCODE\":\"C1901BH001\",\"TM\":{\"X\":\"219100\",\"Y\":\"360100\"},\"LAYER\":["
        "{\"from\":0,\"to\":1.5,\"uscs\":\"SM\",\"name\":\"매립층\",\"desc\":\"성토\"},"
        "{\"from\":1.5,\"to\":4.0,\"uscs\":\"CL\",\"name\":\"실트질점토\",\"desc\":\"연약 점토\"},"
        "{\"from\":4.0,\"to\":6.5,\"uscs\":\"GP\",\"name\":\"모래섞인자갈\",\"desc\":\"모래 자갈\"},"
        "{\"from\":6.5,\"to\":10.0,\"uscs\":\"SW\",\"name\":\"풍화토\",\"desc\":\"기반암 풍화대\"}]}").toUtf8();
    QVERIFY2(GeoInfoBoreholeService::parseSichudanJson(realNamesPayload, &rec, &err), qPrintable(err));
    QVERIFY(std::abs(rec.fillDepthM - 1.5) < 0.01 && std::abs(rec.alluvialThicknessM - 5.0) < 0.01 && std::abs(rec.weatheredDepthM - 6.5) < 0.01);
  }

  void parseKigamGeoJson_extractsFaultsGroundwaterAndOslTerraces() {
    const QByteArray uwJson = QStringLiteral(
        "{\"type\":\"FeatureCollection\",\"features\":[{\"type\":\"Feature\","
        "\"geometry\":{\"type\":\"Point\",\"coordinates\":[398229.0,259521.0]},"
        "\"properties\":{\"시험정공번\":\"GNW00022\",\"표고(m)\":49,\"평균수위(GL.-m)\":\"2.6\",\"대수층유형\":\"충적층\"}}]}").toUtf8();
    const auto uw = KigamPaleoService::parseUnderwaterGeoJson(uwJson);
    QCOMPARE(uw.size(), 1);
    QCOMPARE(uw[0].name, QStringLiteral("GNW00022"));
    QVERIFY(std::abs(uw[0].altitudeM - 49.0) < 0.01 && std::abs(uw[0].waterGlM - 2.6) < 0.01 && uw[0].tmY5186 > 350000.0);

    const QByteArray oslJson = QStringLiteral(
        "{\"type\":\"FeatureCollection\",\"features\":[{\"type\":\"Feature\","
        "\"properties\":{\"노두명(시료명)\":\"1104UM-1\",\"위도\":\"36°05'48.442\\\"\",\"경도\":\"129°25'42.017\\\"\","
        "\"연대(ka)\":\"79.6±2.6\",\"측정방법\":\"OSL\",\"시료해발고도(m)\":\"7.6\",\"단구면해발고도(m)\":\"10-25\"}}]}").toUtf8();
    const auto osl = KigamPaleoService::parseUpliftGeoJson(oslJson);
    QCOMPARE(osl.size(), 1);
    QCOMPARE(osl[0].name, QStringLiteral("1104UM-1"));
    QCOMPARE(osl[0].ageKa, QStringLiteral("79.6±2.6"));
    QVERIFY(osl[0].summary.contains(QStringLiteral("OSL")));
    QVERIFY(KigamPaleoService::reportPdfUrl(QStringLiteral("HD05")).contains(QStringLiteral("HD05.pdf")));
    QVERIFY(KigamPaleoService::originalMapPdfUrl(QStringLiteral("HD05")).contains(QStringLiteral("HD05_org_map.pdf")));
  }

  void paleoDemClassifier_distinguishesTerracesAndAlluvialEnvironments() {
    PaleoDemClassifier::Metrics tm;
    tm.hasDem = true; tm.meanElevM = 48.0; tm.localReliefM = 22.5; tm.meanSlopeDeg = 3.2; tm.riverDistM = 180.0;
    const auto t2 = PaleoDemClassifier::classify(QStringLiteral("하안단구"), tm);
    QCOMPARE(t2.kind, QStringLiteral("하안단구"));
    QVERIFY(t2.note.contains(QStringLiteral("중위단구(T2")) && t2.note.contains(QStringLiteral("하천거리 180m")));

    PaleoDemClassifier::Metrics lm;
    lm.hasDem = true; lm.meanElevM = 14.2; lm.localReliefM = 2.8; lm.meanSlopeDeg = 1.1; lm.hasBorehole = true;
    lm.boreholeCode = QStringLiteral("BH-1"); lm.alluvialThicknessM = 4.5; lm.weatheredDepthM = 6.2;
    lm.dominantUscs = QStringLiteral("SM"); lm.depositEnv = QStringLiteral("사질 충적(자연제방·미고지)");
    const auto levee = PaleoDemClassifier::classify(QStringLiteral("미저지"), lm);
    QCOMPARE(levee.kind, QStringLiteral("자연제방"));
    QVERIFY(levee.note.contains(QStringLiteral("시추 BH-1 충적 4.5m(SM)·생토 6.2m")));
  }

  void seedInterpretationFromSoil_splitsMultiPolygonsIntoGpkg() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QgsProject* proj = QgsProject::instance();
    proj->clear();
    QgsVectorLayer soil(QStringLiteral("MultiPolygon?crs=EPSG:5186"), QStringLiteral("토양도"), QStringLiteral("memory"));
    soil.dataProvider()->addAttributes({QgsField(QStringLiteral("soil_type_geo"), QMetaType::Type::QString)});
    soil.updateFields();
    QgsFeature f(soil.fields());
    f.setGeometry(QgsGeometry::fromWkt(QStringLiteral(
        "MULTIPOLYGON(((200000 500000, 200200 500000, 200200 500200, 200000 500200, 200000 500000)),"
        "((200400 500000, 200500 500000, 200500 500100, 200400 500100, 200400 500000)))")));
    f.setAttribute(QStringLiteral("soil_type_geo"), QStringLiteral("08"));
    QgsFeatureList fl{f};
    QVERIFY(soil.dataProvider()->addFeatures(fl));
    QString err;
    QgsVectorLayer* paleo = PaleoLandformService::ensureInterpretationLayer(proj, dir.filePath(QStringLiteral("paleo.gpkg")), &err);
    QVERIFY2(paleo != nullptr, qPrintable(err));
    const auto res = PaleoLandformService::seedInterpretationFromSoil(&soil, paleo, &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QVERIFY(res.added >= 1 && paleo->featureCount() >= 2);
    proj->clear();
  }

  void hgisPaleoEngine_runsFullPipelineAndRegistersDemLayers() {
    using namespace hgis::paleo;
    GridTransform tf{200000.0, 500000.0, 10.0, 32, 32};
    const size_t n = 32 * 32;
    std::vector<double> zCurr(n, 16.0);
    std::vector<SoilLandformCode> soilGrid(n, SoilLandformCode::FluvialPlain);
    std::vector<GeologyCode> geolGrid(n, GeologyCode::QaAlluvium);
    std::vector<uint8_t> riverMask(n, 0), aoiMask(n, 1);
    riverMask[0] = 1;
    std::vector<BoreholeRawInput> bhs = {{"BH-1", 200150.0, 499850.0, 16.8, 3.6,
        {{0.0, 2.2, "성토층", "SM", "매립", 6.0}, {2.2, 5.4, "충적층", "SM", "모래 및 유기물", 9.0}, {5.4, 8.5, "풍화토", "SC", "풍화대", 50.0}}}};
    const PaleoAnalysisResult out = HgisPaleoEngine::run_pipeline(
        zCurr, tf, bhs, soilGrid, geolGrid, riverMask, aoiMask, {200160.0, 499840.0}, "형산강", "06 하성평탄지", "Qa 충적층");
    QCOMPARE(out.parsed_boreholes.size(), size_t(1));
    QVERIFY(out.parsed_boreholes[0].has_organic_inclusion);
    QCOMPARE(out.paleo_rgba_buffer.size(), n * 4);
    QCOMPARE(out.center_cross_section.dist_m.size(), size_t(32));
    QVERIFY(QString::fromUtf8(out.aoi_summary_text.c_str()).contains(QStringLiteral("형산강")));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString surveyGpkg = dir.filePath(QStringLiteral("survey.gpkg"));
    const QString demDir = dir.filePath(QStringLiteral("DEM"));
    QDir().mkpath(demDir);
    const QString sheetWest = QDir(demDir).filePath(QStringLiteral("35901.tif"));
    const QString sheetEast = QDir(demDir).filePath(QStringLiteral("35902.tif"));
    const QString wkt5187 = QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")).toWkt();
    QVERIFY(PaleoAnalysisRunner::writeFloatGeoTiff(sheetWest, GridTransform{200000.0, 500000.0, 20.0, 32, 32}, wkt5187, std::vector<double>(n, 30.0)));
    QVERIFY(PaleoAnalysisRunner::writeFloatGeoTiff(sheetEast, GridTransform{200640.0, 500000.0, 20.0, 32, 32}, wkt5187, std::vector<double>(n, 45.0)));
    QgsProject* proj = QgsProject::instance();
    proj->clear();
    proj->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    QVERIFY(PaleoAnalysisRunner::collectBoreholes(proj, proj->crs(), QgsPointXY(200300, 499700)).empty());
    QVERIFY(PaleoAnalysisRunner::importNgiiDemDownload(proj, nullptr, sheetEast, surveyGpkg));
    QVERIFY(PaleoAnalysisRunner::importNgiiDemDownload(proj, nullptr, sheetWest, surveyGpkg));
    QgsRasterLayer* mergedDem = PaleoAnalysisRunner::findPreferredDem(proj);
    QVERIFY(mergedDem != nullptr && mergedDem->extent().width() >= 1200.0);
    proj->clear();
  }

  void buildNgiiDemFromTopographic_interpolatesBareEarthDemAndFeedsPaleoPipeline() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString surveyGpkg = dir.filePath(QStringLiteral("survey.gpkg"));
    QgsProject* proj = QgsProject::instance();
    proj->clear();
    proj->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));

    auto* topo = new QgsVectorLayer(QStringLiteral("MultiLineStringZ?crs=EPSG:5187"), QStringLiteral("수치지형도"), QStringLiteral("memory"));
    topo->dataProvider()->addAttributes({QgsField(QStringLiteral("Layer"), QMetaType::Type::QString),
                                         QgsField(QStringLiteral("elevation"), QMetaType::Type::Double),
                                         QgsField(QStringLiteral("z_valid"), QMetaType::Type::Int)});
    topo->updateFields();
    QgsFeatureList cFeats;
    for (int i = 0; i < 6; ++i) {
      const double y = 360000.0 + i * 100.0, elev = 20.0 + i * 5.0;
      QgsFeature f(topo->fields());
      f.setGeometry(QgsGeometry::fromWkt(QStringLiteral("MULTILINESTRING Z ((219000 %1 %2, 219300 %1 %2, 219600 %1 %2))").arg(y, 0, 'f', 1).arg(elev, 0, 'f', 1)));
      f.setAttribute(0, QStringLiteral("F0017111")); f.setAttribute(1, elev); f.setAttribute(2, 1);
      cFeats.append(f);
    }
    QVERIFY(topo->dataProvider()->addFeatures(cFeats));
    topo->updateExtents();
    QVERIFY(proj->addMapLayer(topo));

    QString err;
    const QString builtDem = PaleoAnalysisRunner::buildNgiiDemFromTopographic(proj, nullptr, surveyGpkg, &err);
    QVERIFY2(!builtDem.isEmpty() && QFileInfo::exists(builtDem), qPrintable(err));
    QgsRasterLayer* ngiiDem = PaleoAnalysisRunner::findPreferredDem(proj);
    QVERIFY(ngiiDem != nullptr && ngiiDem->name() == QStringLiteral("[DEM] 국토지리원_DEM.tif"));
    QCOMPARE(BasemapDsm::findDem(proj), ngiiDem);
    const QString txtDem = dir.filePath(QStringLiteral("35901.txt"));
    { QFile tf(txtDem); QVERIFY(tf.open(QIODevice::WriteOnly | QIODevice::Text)); tf.write("219000 360000 32.0\n219600 360000 36.0\n219000 360500 42.0\n219600 360500 48.0\n"); }
    QVERIFY2(PaleoAnalysisRunner::importNgiiDemDownload(proj, nullptr, txtDem, surveyGpkg, &err), qPrintable(err));

    auto* soil = new QgsVectorLayer(QStringLiteral("MultiPolygon?crs=EPSG:5187"), QStringLiteral("토양도(흙토람)"), QStringLiteral("memory"));
    soil->dataProvider()->addAttributes({QgsField(QStringLiteral("soil_type_geo"), QMetaType::Type::QString)});
    soil->updateFields();
    { QgsFeature sf(soil->fields()); sf.setGeometry(QgsGeometry::fromWkt(QStringLiteral("MULTIPOLYGON(((219000 360000, 219600 360000, 219600 360500, 219000 360500, 219000 360000)))"))); sf.setAttribute(0, QStringLiteral("08")); QgsFeatureList sfl{sf}; QVERIFY(soil->dataProvider()->addFeatures(sfl)); }
    QVERIFY(proj->addMapLayer(soil));

    auto* river = new QgsVectorLayer(QStringLiteral("MultiPolygon?crs=EPSG:5187"), QStringLiteral("수계도(하천망)"), QStringLiteral("memory"));
    river->dataProvider()->addAttributes({QgsField(QStringLiteral("riv_nm"), QMetaType::Type::QString)});
    river->updateFields();
    { QgsFeature rf(river->fields()); rf.setGeometry(QgsGeometry::fromWkt(QStringLiteral("MULTIPOLYGON(((219000 360000, 219600 360000, 219600 360040, 219000 360040, 219000 360000)))"))); rf.setAttribute(0, QStringLiteral("북천")); QgsFeatureList rfl{rf}; QVERIFY(river->dataProvider()->addFeatures(rfl)); }
    QVERIFY(proj->addMapLayer(river));

    const auto res = PaleoAnalysisRunner::runFullPipeline(proj, nullptr, surveyGpkg, nullptr);
    QVERIFY(QString::fromUtf8(res.aoi_summary_text.c_str()).contains(QStringLiteral("북천")));

    auto* slopeRl = qobject_cast<QgsRasterLayer*>(proj->mapLayersByName(QStringLiteral("[DEM] 경사도")).value(0));
    QVERIFY(slopeRl != nullptr);
    QVERIFY2(dynamic_cast<QgsSingleBandPseudoColorRenderer*>(slopeRl->renderer()) != nullptr,
             "[DEM] 경사도 must use a classified pseudo-color renderer instead of dark grayscale");

    auto* paleoRl = qobject_cast<QgsRasterLayer*>(proj->mapLayersByName(QStringLiteral("[고지형] 7대 고지형 분류도")).value(0));
    QVERIFY(paleoRl != nullptr && paleoRl->legend() != nullptr);
    QgsLayerTreeLayer treeNode(paleoRl);
    const auto paleoNodes = paleoRl->legend()->createLayerTreeModelLegendNodes(&treeNode);
    QVERIFY2(paleoNodes.size() >= 7, "[고지형] 7대 고지형 분류도 must expose 7 landform classes in the legend");
    qDeleteAll(paleoNodes);

    QList<GeoInfoBoreholeService::HoleCandidate> rawCands;
    for (int i = 1; i <= 12; ++i)
      rawCands.append({QStringLiteral("F7846BH%1").arg(i, 3, 10, QLatin1Char('0')), QgsPointXY(200000.0 + i * 2.0, 500000.0 + i * 1.5), true});
    rawCands.append({QStringLiteral("B2227BH001"), QgsPointXY(200450.0, 500300.0), true});
    rawCands.append({QStringLiteral("C1901BH001"), QgsPointXY(199200.0, 500800.0), true});
    rawCands.append({QStringLiteral("D3001BH001"), QgsPointXY(201500.0, 498500.0), true});
    const auto balanced = GeoInfoBoreholeService::selectSpatiallyBalancedCandidates(rawCands, QgsPointXY(200000.0, 500000.0), 6, 150.0);
    QCOMPARE(balanced.size(), 6);
    QStringList pickedCodes;
    for (const auto& c : balanced) pickedCodes << c.code;
    QVERIFY2(pickedCodes.contains(QStringLiteral("B2227BH001")) && pickedCodes.contains(QStringLiteral("C1901BH001")) &&
                 pickedCodes.contains(QStringLiteral("D3001BH001")),
             "Spatial declustering must pick representatives across all projects/cells first");

    auto* bhVl = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5187"), QStringLiteral("시추주상도(국토지반정보)"), QStringLiteral("memory"));
    bhVl->dataProvider()->addAttributes({QgsField(QStringLiteral("hole_code"), QMetaType::Type::QString),
                                         QgsField(QStringLiteral("alluvial_m"), QMetaType::Type::Double),
                                         QgsField(QStringLiteral("deposit_env"), QMetaType::Type::QString)});
    bhVl->updateFields();
    for (int i = 0; i < 4; ++i) {
      QgsFeature bf(bhVl->fields());
      bf.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(219100.0 + i * 120.0, 360100.0 + (i % 2) * 180.0)));
      bf.setAttribute(0, QStringLiteral("BH-%1").arg(i + 1));
      bf.setAttribute(1, 1.5 + i * 2.2);
      bf.setAttribute(2, QStringLiteral("사질 충적(자연제방·미고지)"));
      QgsFeatureList bfl{bf};
      QVERIFY(bhVl->dataProvider()->addFeatures(bfl));
    }
    QVERIFY(proj->addMapLayer(bhVl));
    QgsRasterLayer* isopach = GeoInfoBoreholeService::registerAlluvialIsopachLayer(proj, nullptr, bhVl, surveyGpkg);
    QVERIFY2(isopach != nullptr && dynamic_cast<QgsSingleBandPseudoColorRenderer*>(isopach->renderer()) != nullptr,
             "Standalone borehole workflow must generate a pseudo-color alluvial isopach raster");
    QFile uiFile(QStringLiteral("../src/app/MainWindowPaleo.cpp"));
    if (!uiFile.exists()) uiFile.setFileName(QStringLiteral("src/app/MainWindowPaleo.cpp"));
    QVERIFY(uiFile.open(QIODevice::ReadOnly));
    const QString uiSrc = QString::fromUtf8(uiFile.readAll());
    QVERIFY2(uiSrc.contains(QLatin1String("actionGeology5k")) &&
                 uiSrc.contains(QLatin1String("btnDownloadGeologyOnly")) &&
                 uiSrc.contains(QLatin1String("btnDownloadBoreholeOnly")) &&
                 uiSrc.contains(QLatin1String("ReferenceMapKind::Geology")),
             "Standalone UI must expose separate 5km digital geology and borehole download actions");
    QFile ovFile(QStringLiteral("../src/app/MainWindowOverlay.cpp"));
    if (!ovFile.exists()) ovFile.setFileName(QStringLiteral("src/app/MainWindowOverlay.cpp"));
    QVERIFY(ovFile.open(QIODevice::ReadOnly));
    const QString ovSrc = QString::fromUtf8(ovFile.readAll());
    QVERIFY2(ovSrc.contains(QLatin1String("surveyExtentRadius(proj, m_canvas, 5000.0)")) &&
                 ovSrc.contains(QStringLiteral("단층·선구조(KIGAM 1:5만)")),
             "Geology button must zoom/fetch 5km survey radius and chain fault line overlay");
    proj->clear();
  }
};

QTEST_MAIN(TestPaleoSources)
#include "test_paleo_sources.moc"
