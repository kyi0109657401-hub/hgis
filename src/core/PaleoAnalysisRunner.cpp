#include "PaleoAnalysisRunner.h"
#include "BasemapDsm.h"
#include "GeoInfoBoreholeService.h"
#include "GeologyMapService.h"
#include "KaLogExcept.h"
#include "KigamPaleoService.h"
#include "LayerOps.h"
#include "PaleoLandformService.h"
#include "RiverMapService.h"
#include "SoilMapService.h"
#include "VworldSettings.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <cmath>

#include <qgscategorizedsymbolrenderer.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransform.h>
#include <qgsexception.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgsfeedback.h>
#include <qgsfield.h>
#include <qgsfields.h>
#include <qgsgeometry.h>
#include <qgsmapcanvas.h>
#include <qgsmarkersymbol.h>
#include <qgspallabeling.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgstextformat.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayerlabeling.h>

namespace {

void addMarkerVectorLayer(QgsProject* project, const QString& gpkgPath,
                          const std::vector<hgis::paleo::ParsedBoreholeSummary>& boreholes,
                          const QgsCoordinateReferenceSystem& workCrs) {
  if (!project || boreholes.empty()) return;
  const QString title = QStringLiteral("[고지형] 시추주상도 마커");
  for (QgsMapLayer* l : project->mapLayersByName(title))
    if (l) project->removeMapLayer(l->id());
  const QString crsAuth = workCrs.isValid() ? workCrs.authid() : QStringLiteral("EPSG:5187");
  QgsVectorLayer mem(QStringLiteral("Point?crs=%1").arg(crsAuth), title, QStringLiteral("memory"));
  mem.dataProvider()->addAttributes({
      QgsField(QStringLiteral("hole_id"), QMetaType::Type::QString),
      QgsField(QStringLiteral("sed_type"), QMetaType::Type::QString),
      QgsField(QStringLiteral("map_label"), QMetaType::Type::QString),
  });
  mem.updateFields();

  QgsFeatureList feats;
  for (const auto& bh : boreholes) {
    const QgsPointXY pt(bh.x, bh.y);
    QgsFeature f(mem.fields());
    f.setGeometry(QgsGeometry::fromPointXY(pt));
    f.setAttribute(0, QString::fromUtf8(bh.hole_id.c_str()));
    f.setAttribute(1, QString::fromUtf8(bh.sed_type.c_str()));
    f.setAttribute(2, QString::fromUtf8(bh.map_label.c_str()));
    feats.append(f);
  }
  mem.dataProvider()->addFeatures(feats);
  QgsVectorFileWriter::SaveVectorOptions opts;
  opts.driverName = QStringLiteral("GPKG");
  opts.layerName = QStringLiteral("paleo_boreholes");
  opts.fileEncoding = QStringLiteral("UTF-8");
  opts.actionOnExistingFile = QgsVectorFileWriter::CreateOrOverwriteFile;
  QString err, fn, ln;
  if (QgsVectorFileWriter::writeAsVectorFormatV3(&mem, gpkgPath, project->transformContext(), opts,
                                                 &err, &fn, &ln) != QgsVectorFileWriter::NoError)
    return;
  auto* vl = new QgsVectorLayer(QStringLiteral("%1|layername=paleo_boreholes").arg(gpkgPath), title, QStringLiteral("ogr"));
  if (!vl->isValid()) { delete vl; return; }
  auto mkSym = [](const QString& hex) {
    return QgsMarkerSymbol::createSimple({{QStringLiteral("name"), QStringLiteral("circle")},
                                          {QStringLiteral("color"), hex},
                                          {QStringLiteral("outline_color"), QStringLiteral("#1f2937")},
                                          {QStringLiteral("size"), QStringLiteral("3.6")}}).release();
  };
  vl->setRenderer(new QgsCategorizedSymbolRenderer(
      QStringLiteral("sed_type"),
      {QgsRendererCategory(QVariant(QStringLiteral("Coarse")), mkSym(QStringLiteral("#FF9800")),
                           QStringLiteral("사질·자갈 우세 (자연제방·선상지형)")),
       QgsRendererCategory(QVariant(QStringLiteral("Fine")), mkSym(QStringLiteral("#00ACC1")),
                           QStringLiteral("점토·실트 우세 (구하도·배후습지형)"))}));
  QgsPalLayerSettings pal;
  pal.fieldName = QStringLiteral("map_label");
  QgsTextFormat tf;
  tf.setSize(8.0);
  tf.buffer().setEnabled(true);
  tf.buffer().setSize(0.8);
  tf.buffer().setColor(Qt::white);
  pal.setFormat(tf);
  vl->setLabeling(new QgsVectorLayerSimpleLabeling(pal));
  vl->setLabelsEnabled(true);
  LayerOps::markReferenceLayer(vl);
  LayerOps::applyLegendCrsLabel(vl);
  if (project->addMapLayer(vl, true))
    LayerOps::placeInLegendGroup(project, vl, QStringLiteral("참조 지도"));
  else
    delete vl;
}

}  // namespace

void PaleoAnalysisRunner::ensureReferenceLayers(QgsProject* project, QgsMapCanvas* canvas,
                                                const QgsRectangle& ext5186,
                                                const QgsRectangle& ext10k5186,
                                                const QString& paleoDir, QgsFeedback* feedback) {
  const auto ctx = project->transformContext();
  if (!PaleoLandformService::findSoilTerrainLayer(project)) {
    const auto p = SoilMapService::prepare(ext5186, QDir(paleoDir).filePath(QStringLiteral("토양도_흙토람.gpkg")), ctx, feedback);
    if (p.isReady()) SoilMapService::addPrepared(project, canvas, p, nullptr);
  }
  if (!GeologyMapService::existingGeologyLayer(project)) {
    const auto p = GeologyMapService::prepare(ext10k5186, QDir(paleoDir).filePath(QStringLiteral("지질도_KIGAM.gpkg")), ctx, feedback);
    if (p.isReady()) GeologyMapService::addPrepared(project, canvas, p, nullptr);
  }
  if (project->mapLayersByName(QStringLiteral("고지형 수문·연대(KIGAM)")).isEmpty()) {
    const auto p = KigamPaleoService::prepare(ext10k5186, QDir(paleoDir).filePath(QStringLiteral("고지형_KIGAM.gpkg")), ctx, feedback);
    if (p.isReady()) KigamPaleoService::addPrepared(project, canvas, p, nullptr);
  }
  if (!GeoInfoBoreholeService::findBoreholeLayer(project)) {
    const auto p = GeoInfoBoreholeService::prepare(ext5186, QDir(paleoDir).filePath(QStringLiteral("시추주상도_국토지반정보.gpkg")), ctx, feedback);
    if (p.isReady()) GeoInfoBoreholeService::addPrepared(project, canvas, p, nullptr);
  }
  const QString vwKey = VworldSettings::loadApiKey();
  if (!vwKey.isEmpty() && project->mapLayersByName(QStringLiteral("수계도(하천망)")).isEmpty()) {
    const auto p = RiverMapService::prepare(ext10k5186, vwKey, QDir(paleoDir).filePath(QStringLiteral("수계도_VWorld.gpkg")), ctx, feedback);
    if (p.isReady()) RiverMapService::addPrepared(project, canvas, p, nullptr);
  }
}

QgsRasterLayer* PaleoAnalysisRunner::findPreferredDem(const QgsProject* project) {
  if (!project) return nullptr;
  for (QgsMapLayer* l : project->mapLayersByName(QStringLiteral("[DEM] 국토지리원_DEM.tif"))) {
    auto* rl = qobject_cast<QgsRasterLayer*>(l);
    if (rl && rl->isValid() && QFileInfo::exists(rl->source())) return rl;
  }
  return BasemapDsm::findDem(project);
}

QString PaleoAnalysisRunner::buildNgiiDemFromTopographic(QgsProject* project, QgsMapCanvas* canvas,
                                                         const QString& surveyPath, QString* errorOut) {
  if (!project) return {};
  const QgsCoordinateReferenceSystem workCrs =
      project->crs().isValid() ? project->crs() : QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187"));
  const QgsRectangle box = surveyExtentRadius(project, canvas, 5000.0);
  std::vector<double> ptX, ptY, ptZ;
  ptX.reserve(12000); ptY.reserve(12000); ptZ.reserve(12000);
  for (QgsMapLayer* raw : project->mapLayers()) {
    auto* vl = qobject_cast<QgsVectorLayer*>(raw);
    if (!vl || !vl->isValid()) continue;
    const int iElev = vl->fields().indexOf(QStringLiteral("elevation"));
    if (iElev < 0) continue;
    const int iZValid = vl->fields().indexOf(QStringLiteral("z_valid"));
    const int iCad = vl->fields().indexOf(QStringLiteral("Layer"));
    const QgsCoordinateTransform toWork(vl->crs(), workCrs, project->transformContext());
    const QgsCoordinateTransform toLayer(workCrs, vl->crs(), project->transformContext());
    const bool needTr = vl->crs().isValid() && vl->crs() != workCrs;
    QgsRectangle qBox = box;
    if (needTr) { try { qBox = toLayer.transformBoundingBox(box); } catch (...) { KA_LOG_EXCEPT(); } }
    QgsFeature f;
    QgsFeatureIterator it = vl->getFeatures(QgsFeatureRequest(qBox));
    while (it.nextFeature(f)) {
      if (!f.hasGeometry() || (iZValid >= 0 && f.attribute(iZValid).toInt() == 0)) continue;
      const QString code = iCad >= 0 ? f.attribute(iCad).toString() : QString();
      if (!code.isEmpty() && !code.startsWith(QLatin1String("F001")) && !code.startsWith(QLatin1String("F002")))
        continue;
      bool ok = false;
      const double z = f.attribute(iElev).toDouble(&ok);
      if (!ok || !std::isfinite(z) || z <= -100.0 || z > 3000.0) continue;
      QgsGeometry g = f.geometry();
      if (needTr) { try { g.transform(toWork); } catch (...) { KA_LOG_EXCEPT(); continue; } }
      const QgsGeometry dense = g.densifyByDistance(40.0);
      int step = 0;
      for (auto v = dense.vertices_begin(); v != dense.vertices_end() && ptX.size() < 12000; ++v) {
        if ((step++ % 2) == 0 && box.contains(QgsPointXY((*v).x(), (*v).y()))) {
          ptX.push_back((*v).x()); ptY.push_back((*v).y()); ptZ.push_back(z);
        }
      }
      if (ptX.size() >= 12000) break;
    }
    if (ptX.size() >= 12000) break;
  }
  if (ptX.size() < 4) {
    if (errorOut) *errorOut = QStringLiteral("수치지형도 등고선 표고점이 부족합니다.");
    return {};
  }
  const double cellSz = std::max(5.0, std::max(box.width(), box.height()) / 240.0);
  const int w = std::clamp(static_cast<int>(std::ceil(box.width() / cellSz)), 16, 360);
  const int h = std::clamp(static_cast<int>(std::ceil(box.height() / cellSz)), 16, 360);
  const hgis::paleo::GridTransform tf{box.xMinimum(), box.yMaximum(), cellSz, w, h};
  const std::vector<double> zGrid = hgis::paleo::HgisPaleoEngine::idw_interpolate_grid(tf, ptX, ptY, ptZ, 10, 2.0);
  const QString outDir = surveyPath.isEmpty() ? QDir::temp().filePath(QStringLiteral("ka-hgis-paleo"))
                                              : QFileInfo(surveyPath).absoluteDir().filePath(QStringLiteral("지형분석"));
  QDir().mkpath(outDir);
  const QString demOut = QDir(outDir).filePath(QStringLiteral("국토지리원_DEM.tif"));
  for (QgsMapLayer* l : project->mapLayersByName(QStringLiteral("[DEM] 국토지리원_DEM.tif")))
    if (l) project->removeMapLayer(l->id());
  QFile::remove(demOut);
  if (!writeFloatGeoTiff(demOut, tf, workCrs.toWkt(), zGrid, errorOut)) return {};
  registerDemWideLayers(project, canvas, demOut, surveyPath, errorOut);
  return demOut;
}

void PaleoAnalysisRunner::registerBoreholeMarkers(
    QgsProject* project, const QString& gpkgPath,
    const std::vector<hgis::paleo::ParsedBoreholeSummary>& boreholes) {
  if (!project) return;
  addMarkerVectorLayer(project, gpkgPath, boreholes,
                       project->crs().isValid() ? project->crs() : QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
}

QgsRectangle PaleoAnalysisRunner::surveyExtentRadius(QgsProject* project, QgsMapCanvas* canvas,
                                                     double radiusM, QgsGeometry* outAoiWorkCrs,
                                                     QgsPointXY* outCenterWorkCrs) {
  const QgsCoordinateReferenceSystem workCrs =
      (project && project->crs().isValid()) ? project->crs() : QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187"));
  QVector<QgsGeometry> parts;
  if (project) {
    for (QgsVectorLayer* vl : LayerOps::findAllByLayerKey(project, QStringLiteral("survey_area"))) {
      if (!vl || !vl->isValid()) continue;
      const QgsCoordinateTransform tr(vl->crs(), workCrs, project->transformContext());
      const bool needTr = vl->crs().isValid() && vl->crs() != workCrs;
      QgsFeature f;
      QgsFeatureIterator it = vl->getFeatures(QgsFeatureRequest().setNoAttributes());
      while (it.nextFeature(f)) {
        if (!f.hasGeometry() || f.geometry().isEmpty()) continue;
        QgsGeometry g = f.geometry();
        if (needTr) { try { g.transform(tr); } catch (...) { KA_LOG_EXCEPT(); } }
        parts.append(g);
      }
    }
  }
  QgsGeometry aoi;
  QgsPointXY center(200000.0, 500000.0);
  QgsRectangle box(199800.0, 499800.0, 200200.0, 500200.0);
  if (!parts.isEmpty()) {
    aoi = QgsGeometry::unaryUnion(parts);
    box = aoi.boundingBox();
    center = box.center();
  } else {
    if (canvas && !canvas->extent().isEmpty() && canvas->extent().isFinite()) {
      center = canvas->extent().center();
    } else if (project) {
      for (QgsMapLayer* l : project->mapLayers()) {
        if (!l || !l->isValid() || l->extent().isEmpty() || !l->extent().isFinite() || l->crs().isGeographic()) continue;
        center = l->extent().center();
        break;
      }
    }
    box = QgsRectangle(center.x() - 250.0, center.y() - 250.0, center.x() + 250.0, center.y() + 250.0);
    aoi = QgsGeometry::fromRect(box);
  }
  if (outAoiWorkCrs) *outAoiWorkCrs = aoi;
  if (outCenterWorkCrs) *outCenterWorkCrs = center;
  box.grow(radiusM);
  return box;
}
