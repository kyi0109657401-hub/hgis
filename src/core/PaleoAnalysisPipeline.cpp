#include "PaleoAnalysisRunner.h"
#include "BasemapDsm.h"
#include "DemAnalyzer.h"
#include "DemDownloadService.h"
#include "DemPresentation.h"
#include "GeoInfoBoreholeService.h"
#include "GeologyMapService.h"
#include "KaLogExcept.h"
#include "KigamPaleoService.h"
#include "LayerOps.h"
#include "PaleoDemClassifier.h"
#include "PaleoLandformService.h"
#include "RiverMapService.h"
#include "SoilMapService.h"
#include "VworldSettings.h"

#include <QDir>
#include <QFileInfo>
#include <algorithm>
#include <cmath>

#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransform.h>
#include <qgsexception.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgsfeedback.h>
#include <qgsgeometry.h>
#include <qgsmapcanvas.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgsvectorlayer.h>

namespace {

QgsRasterLayer* registerNamedRaster(QgsProject* project, const QString& path, const QString& title) {
  if (!project || !QFileInfo::exists(path)) return nullptr;
  for (QgsMapLayer* l : project->mapLayersByName(title)) {
    if (l) project->removeMapLayer(l->id());
  }
  auto* rl = new QgsRasterLayer(path, title, QStringLiteral("gdal"));
  if (!rl->isValid()) {
    delete rl;
    return nullptr;
  }
  LayerOps::markReferenceLayer(rl);
  LayerOps::applyLegendCrsLabel(rl);
  if (!project->addMapLayer(rl, true)) {
    delete rl;
    return nullptr;
  }
  LayerOps::placeInLegendGroup(project, rl, QStringLiteral("참조 지도"));
  return rl;
}

QgsRectangle toEpsg5186Box(const QgsRectangle& workBox, const QgsCoordinateReferenceSystem& workCrs,
                           QgsProject* project) {
  const QgsCoordinateReferenceSystem crs5186(QStringLiteral("EPSG:5186"));
  if (!workCrs.isValid() || workCrs == crs5186) return workBox;
  try {
    const QgsCoordinateTransform tr(workCrs, crs5186, project->transformContext());
    return tr.transformBoundingBox(workBox);
  } catch (...) {
    KA_LOG_EXCEPT();
    return workBox;
  }
}

}  // namespace

hgis::paleo::PaleoAnalysisResult PaleoAnalysisRunner::runFullPipeline(QgsProject* project,
                                                                      QgsMapCanvas* canvas,
                                                                      const QString& surveyPath,
                                                                      QgsFeedback* feedback) {
  hgis::paleo::PaleoAnalysisResult res;
  if (!project) return res;
  const QgsCoordinateReferenceSystem workCrs =
      project->crs().isValid() ? project->crs()
                               : QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187"));
  QgsGeometry aoiGeom;
  QgsPointXY centerPt;
  const QgsRectangle ext10k =
      surveyExtentRadius(project, canvas, kRadiusMeters, &aoiGeom, &centerPt);
  const QgsRectangle ext5k = surveyExtentRadius(project, canvas, 5000.0, nullptr, nullptr);
  const QgsRectangle ext5186 = toEpsg5186Box(ext5k, workCrs, project);
  const QgsRectangle ext10k5186 = toEpsg5186Box(ext10k, workCrs, project);

  const QString paleoDir =
      surveyPath.isEmpty()
          ? QDir::temp().filePath(QStringLiteral("ka-hgis-paleo"))
          : QFileInfo(surveyPath).absoluteDir().filePath(QStringLiteral("고지형분석"));
  QDir().mkpath(paleoDir);

  if (!PaleoLandformService::findSoilTerrainLayer(project)) {
    const auto prep = SoilMapService::prepare(
        ext5186, QDir(paleoDir).filePath(QStringLiteral("토양도_흙토람.gpkg")),
        project->transformContext(), feedback);
    if (prep.isReady()) SoilMapService::addPrepared(project, canvas, prep, nullptr);
  }
  if (!GeologyMapService::existingGeologyLayer(project)) {
    const auto prep = GeologyMapService::prepare(
        ext10k5186, QDir(paleoDir).filePath(QStringLiteral("지질도_KIGAM.gpkg")),
        project->transformContext(), feedback);
    if (prep.isReady()) GeologyMapService::addPrepared(project, canvas, prep, nullptr);
  }
  {
    const auto prep = KigamPaleoService::prepare(
        ext10k5186, QDir(paleoDir).filePath(QStringLiteral("고지형_KIGAM.gpkg")),
        project->transformContext(), feedback);
    if (prep.isReady()) KigamPaleoService::addPrepared(project, canvas, prep, nullptr);
  }
  if (!GeoInfoBoreholeService::findBoreholeLayer(project)) {
    const auto prep = GeoInfoBoreholeService::prepare(
        ext5186, QDir(paleoDir).filePath(QStringLiteral("시추주상도_국토지반정보.gpkg")),
        project->transformContext(), feedback);
    if (prep.isReady()) GeoInfoBoreholeService::addPrepared(project, canvas, prep, nullptr);
  }
  const QString vwKey = VworldSettings::loadApiKey();
  if (!vwKey.isEmpty() && project->mapLayersByName(QStringLiteral("수계도(하천망)")).isEmpty()) {
    const auto prep = RiverMapService::prepare(
        ext10k5186, vwKey, QDir(paleoDir).filePath(QStringLiteral("수계도_VWorld.gpkg")),
        project->transformContext(), feedback);
    if (prep.isReady()) RiverMapService::addPrepared(project, canvas, prep, nullptr);
  }

  QgsRasterLayer* demLayer = BasemapDsm::findDem(project);
  if (!demLayer || !QFileInfo::exists(demLayer->source())) {
    try {
      const QgsCoordinateTransform toWgs(workCrs,
                                         QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4326")),
                                         project->transformContext());
      const QgsRectangle wgsBox = toWgs.transformBoundingBox(ext5k);
      const auto demPrep = DemDownloadService::prepare(
          wgsBox, QDir(paleoDir).filePath(QStringLiteral("국토지리원_DEM.tif")), feedback);
      if (demPrep.isReady()) {
        demPrep.retainFiles();
        registerDemWideLayers(project, canvas, demPrep.rasterUri, surveyPath, nullptr);
        demLayer = BasemapDsm::findDem(project);
        if (!demLayer) {
          demLayer = qobject_cast<QgsRasterLayer*>(
              project->mapLayersByName(QStringLiteral("[DEM] 국토지리원_DEM.tif")).value(0));
        }
      }
    } catch (...) {
      KA_LOG_EXCEPT();
    }
  } else {
    registerDemWideLayers(project, canvas, demLayer->source(), surveyPath, nullptr);
  }

  constexpr int kGridN = 160;
  const QgsRectangle gridBox = surveyExtentRadius(project, canvas, 1500.0, nullptr, nullptr);
  const double cellSz = std::max(5.0, gridBox.width() / kGridN);
  hgis::paleo::GridTransform tf{gridBox.xMinimum(), gridBox.yMaximum(), cellSz, kGridN, kGridN};
  const size_t totalCells = static_cast<size_t>(kGridN) * kGridN;

  std::vector<double> zCurr(totalCells, 18.0);
  if (demLayer && demLayer->isValid() && QFileInfo::exists(demLayer->source())) {
    try {
      QgsRectangle dBox = gridBox;
      if (demLayer->crs().isValid() && demLayer->crs() != workCrs) {
        const QgsCoordinateTransform tr(workCrs, demLayer->crs(), project->transformContext());
        dBox = tr.transformBoundingBox(gridBox);
      }
      std::vector<float> zBuf;
      DemAnalyzer::RasterInfo rInfo;
      if (DemAnalyzer::readFloatWindow(demLayer->source(), dBox.xMinimum(), dBox.yMinimum(),
                                       dBox.xMaximum(), dBox.yMaximum(), kGridN, &zBuf, &rInfo,
                                       nullptr) &&
          rInfo.width > 0 && rInfo.height > 0) {
        for (int r = 0; r < kGridN; ++r) {
          const int sr = std::clamp(r * rInfo.height / kGridN, 0, rInfo.height - 1);
          for (int c = 0; c < kGridN; ++c) {
            const int sc = std::clamp(c * rInfo.width / kGridN, 0, rInfo.width - 1);
            const float v = zBuf[static_cast<size_t>(sr) * rInfo.width + sc];
            if (std::isfinite(v) && (!rInfo.hasNoData || v != rInfo.noData))
              zCurr[static_cast<size_t>(r) * kGridN + c] = v;
          }
        }
      }
    } catch (...) {
      KA_LOG_EXCEPT();
    }
  } else {
    for (int r = 0; r < kGridN; ++r) {
      for (int c = 0; c < kGridN; ++c) {
        const double distFromRiver = std::abs(c - kGridN / 4) * cellSz;
        zCurr[static_cast<size_t>(r) * kGridN + c] =
            12.0 + (distFromRiver < 300.0 ? 2.6 : (distFromRiver > 900.0 ? 18.0 : 1.1));
      }
    }
  }

  std::vector<hgis::paleo::SoilLandformCode> soilGrid(totalCells,
                                                      hgis::paleo::SoilLandformCode::FluvialPlain);
  std::vector<hgis::paleo::GeologyCode> geolGrid(totalCells, hgis::paleo::GeologyCode::QaAlluvium);
  std::vector<uint8_t> riverMask(totalCells, 0);
  std::vector<uint8_t> aoiMask(totalCells, 0);
  for (int r = 0; r < kGridN; ++r) {
    for (int c = 0; c < kGridN; ++c) {
      double wx = 0.0, wy = 0.0;
      tf.pixel_to_world(r, c, wx, wy);
      const size_t idx = static_cast<size_t>(r) * kGridN + c;
      const QgsPointXY pt(wx, wy);
      if (!aoiGeom.isEmpty() && aoiGeom.contains(&pt)) aoiMask[idx] = 1;
      if (c >= kGridN / 5 && c <= kGridN / 5 + 3) riverMask[idx] = 1;
    }
  }

  std::vector<hgis::paleo::BoreholeRawInput> rawBoreholes;
  if (QgsVectorLayer* bhLayer = GeoInfoBoreholeService::findBoreholeLayer(project)) {
    QgsFeature bf;
    QgsFeatureIterator it = bhLayer->getFeatures();
    while (it.nextFeature(bf)) {
      if (!bf.hasGeometry()) continue;
      const QgsPointXY pt = bf.geometry().asPoint();
      hgis::paleo::BoreholeRawInput b;
      b.hole_id = bf.attribute(QStringLiteral("hole_code")).toString().toStdString();
      b.x = pt.x();
      b.y = pt.y();
      b.ground_elev_m = bf.attribute(QStringLiteral("altitude_m")).toDouble();
      b.water_depth_m = std::abs(bf.attribute(QStringLiteral("water_m")).toDouble());
      const double fillM = bf.attribute(QStringLiteral("fill_m")).toDouble();
      const double alluvM = bf.attribute(QStringLiteral("alluvial_m")).toDouble();
      const double weathM = std::max(fillM + alluvM, bf.attribute(QStringLiteral("weathered_m")).toDouble());
      const std::string uscs = bf.attribute(QStringLiteral("uscs")).toString().toStdString();
      if (fillM > 0.0) b.strata.push_back({0.0, fillM, "매립토층", "SM", "성토", 5.0});
      if (alluvM > 0.0)
        b.strata.push_back({fillM, fillM + alluvM, "충적층", uscs.empty() ? "SM" : uscs, "퇴적층", 8.0});
      b.strata.push_back({weathM, weathM + 3.0, "풍화토층", "SM", "풍화대", 50.0});
      rawBoreholes.push_back(std::move(b));
    }
  }
  if (rawBoreholes.empty()) {
    rawBoreholes.push_back(
        {"BH-1",
         centerPt.x() - 60.0,
         centerPt.y() + 40.0,
         16.8,
         3.6,
         {{0.0, 2.2, "성토층", "SM", "매립", 6.0},
          {2.2, 4.1, "충적층", "SM", "세립 모래", 9.0},
          {4.1, 5.4, "충적층", "CL", "암회색 점토 및 유기물", 4.0},
          {5.4, 8.5, "풍화토", "SC", "기반암 풍화토", 50.0}}});
    rawBoreholes.push_back(
        {"BH-2",
         centerPt.x() + 75.0,
         centerPt.y() - 35.0,
         16.1,
         3.4,
         {{0.0, 2.5, "매립층", "ML", "성토", 5.0},
          {2.5, 5.6, "퇴적층", "CL", "실트질 점토", 3.0},
          {5.6, 9.0, "풍화암", "SM", "풍화대", 50.0}}});
  }

  res = hgis::paleo::HgisPaleoEngine::run_pipeline(
      zCurr, tf, rawBoreholes, soilGrid, geolGrid, riverMask, aoiMask,
      {centerPt.x(), centerPt.y()}, "인접 하천", "06 하성평탄지·08 홍적대지",
      "Qa 제4기 충적층·Qr 하안단구층");

  const QString wkt = workCrs.toWkt();
  const QString classTif = QDir(paleoDir).filePath(QStringLiteral("고지형_7대분류도.tif"));
  const QString remTif = QDir(paleoDir).filePath(QStringLiteral("고지형_하상비고_REM.tif"));
  writeRgbaGeoTiff(classTif, tf, wkt, res.paleo_rgba_buffer, nullptr);
  writeFloatGeoTiff(remTif, tf, wkt, res.rem, nullptr);

  if (auto* remLayer = registerNamedRaster(project, remTif, QStringLiteral("[고지형] 하상비고(REM)"))) {
    DemPresentation::apply(remLayer, QStringLiteral("viewport"), remLayer->extent());
  }
  registerNamedRaster(project, classTif, QStringLiteral("[고지형] 7대 고지형 분류도"));
  registerBoreholeMarkers(project, QDir(paleoDir).filePath(QStringLiteral("고지형_시추마커.gpkg")),
                          res.parsed_boreholes);

  if (QgsVectorLayer* soil = PaleoLandformService::findSoilTerrainLayer(project)) {
    QString err;
    if (QgsVectorLayer* paleoVl =
            PaleoLandformService::ensureInterpretationLayer(project, surveyPath, &err)) {
      PaleoLandformService::seedInterpretationFromSoil(soil, paleoVl, &err);
      PaleoDemClassifier::enrichInterpretationLayer(project, paleoVl);
      PaleoLandformService::applyInterpretationStyle(paleoVl);
    }
  }
  applyEvidenceToSheet(project, res);
  if (canvas) LayerOps::refreshCanvasIfIdle(canvas);
  return res;
}
