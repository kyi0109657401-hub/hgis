#include "PaleoAnalysisRunner.h"
#include "DemAnalyzer.h"
#include "DemDownloadService.h"
#include "DemPresentation.h"
#include "GeoInfoBoreholeService.h"
#include "GeologyMapService.h"
#include "KaLogExcept.h"
#include "LayerOps.h"
#include "PaleoDemClassifier.h"
#include "PaleoLandformService.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
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
#include <qgslayoutitemlabel.h>
#include <qgslayoutmanager.h>
#include <qgslayoutpoint.h>
#include <qgslayoutsize.h>
#include <qgsmapcanvas.h>
#include <qgsprintlayout.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgstextformat.h>
#include <qgsvectorlayer.h>

namespace {

QgsRasterLayer* registerNamedRaster(QgsProject* project, const QString& path, const QString& title) {
  if (!project || !QFileInfo::exists(path)) return nullptr;
  for (QgsMapLayer* l : project->mapLayersByName(title))
    if (l) project->removeMapLayer(l->id());
  auto* rl = new QgsRasterLayer(path, title, QStringLiteral("gdal"));
  if (!rl->isValid()) { delete rl; return nullptr; }
  LayerOps::markReferenceLayer(rl);
  LayerOps::applyLegendCrsLabel(rl);
  if (!project->addMapLayer(rl, true)) { delete rl; return nullptr; }
  LayerOps::placeInLegendGroup(project, rl, QStringLiteral("참조 지도"));
  return rl;
}

QgsRectangle toEpsg5186Box(const QgsRectangle& workBox, const QgsCoordinateReferenceSystem& workCrs,
                           QgsProject* project) {
  const QgsCoordinateReferenceSystem crs5186(QStringLiteral("EPSG:5186"));
  if (!workCrs.isValid() || workCrs == crs5186) return workBox;
  try {
    return QgsCoordinateTransform(workCrs, crs5186, project->transformContext())
        .transformBoundingBox(workBox);
  } catch (...) {
    KA_LOG_EXCEPT();
    return workBox;
  }
}

hgis::paleo::SoilLandformCode mapSoilCode(const QString& raw) {
  const QString c = raw.trimmed();
  if (c == QLatin1String("04") || c == QLatin1String("4")) return hgis::paleo::SoilLandformCode::ValleyFan;
  if (c == QLatin1String("05") || c == QLatin1String("5")) return hgis::paleo::SoilLandformCode::MarinePlain;
  if (c == QLatin1String("06") || c == QLatin1String("6")) return hgis::paleo::SoilLandformCode::FluvialPlain;
  if (c == QLatin1String("08") || c == QLatin1String("8")) return hgis::paleo::SoilLandformCode::DiluvialTerrace;
  if (c == QLatin1String("01") || c == QLatin1String("1")) return hgis::paleo::SoilLandformCode::Mountain;
  return hgis::paleo::SoilLandformCode::Hillslope;
}

hgis::paleo::GeologyCode mapGeolSymbol(const QString& sym, const QString& era) {
  const QString s = sym.trimmed().toUpper();
  if (s.startsWith(QLatin1String("QR")) || s.startsWith(QLatin1String("QT")))
    return hgis::paleo::GeologyCode::QrTerrace;
  if (s.startsWith(QLatin1String("QB"))) return hgis::paleo::GeologyCode::QbaBasalt;
  if (s.startsWith(QLatin1Char('Q')) || era.contains(QStringLiteral("제4기")))
    return hgis::paleo::GeologyCode::QaAlluvium;
  return hgis::paleo::GeologyCode::Bedrock;
}

void rasterizePolyGrid(QgsVectorLayer* vl, const QgsCoordinateReferenceSystem& workCrs,
                       QgsProject* project, const QgsRectangle& gridBox,
                       const hgis::paleo::GridTransform& tf,
                       const std::function<void(const QgsFeature&, size_t)>& applyCell) {
  if (!vl || !vl->isValid()) return;
  const QgsCoordinateTransform toWork(vl->crs(), workCrs, project->transformContext());
  const QgsCoordinateTransform toLayer(workCrs, vl->crs(), project->transformContext());
  const bool needTr = vl->crs().isValid() && vl->crs() != workCrs;
  QgsRectangle qBox = gridBox;
  if (needTr) {
    try { qBox = toLayer.transformBoundingBox(gridBox); } catch (...) { KA_LOG_EXCEPT(); }
  }
  QgsFeature f;
  QgsFeatureIterator it = vl->getFeatures(QgsFeatureRequest(qBox));
  while (it.nextFeature(f)) {
    if (!f.hasGeometry()) continue;
    QgsGeometry g = f.geometry();
    if (needTr) {
      try { g.transform(toWork); } catch (...) { KA_LOG_EXCEPT(); continue; }
    }
    const QgsRectangle b = g.boundingBox();
    const int c0 = std::clamp(static_cast<int>((b.xMinimum() - tf.origin_x) / tf.cell_size), 0, tf.width - 1);
    const int c1 = std::clamp(static_cast<int>((b.xMaximum() - tf.origin_x) / tf.cell_size), 0, tf.width - 1);
    const int r0 = std::clamp(static_cast<int>((tf.origin_y - b.yMaximum()) / tf.cell_size), 0, tf.height - 1);
    const int r1 = std::clamp(static_cast<int>((tf.origin_y - b.yMinimum()) / tf.cell_size), 0, tf.height - 1);
    for (int r = r0; r <= r1; ++r) {
      for (int c = c0; c <= c1; ++c) {
        double wx = 0.0, wy = 0.0;
        tf.pixel_to_world(r, c, wx, wy);
        const QgsPointXY pt(wx, wy);
        if (g.contains(&pt)) applyCell(f, static_cast<size_t>(r) * tf.width + c);
      }
    }
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
  const QgsRectangle ext10k = surveyExtentRadius(project, canvas, kRadiusMeters, &aoiGeom, &centerPt);
  const QgsRectangle ext5k = surveyExtentRadius(project, canvas, 5000.0, nullptr, nullptr);
  const QString paleoDir =
      surveyPath.isEmpty() ? QDir::temp().filePath(QStringLiteral("ka-hgis-paleo"))
                           : QFileInfo(surveyPath).absoluteDir().filePath(QStringLiteral("고지형분석"));
  QDir().mkpath(paleoDir);
  if (feedback) {
    ensureReferenceLayers(project, canvas, toEpsg5186Box(ext5k, workCrs, project),
                          toEpsg5186Box(ext10k, workCrs, project), paleoDir, feedback);
  }

  buildNgiiDemFromTopographic(project, canvas, surveyPath, nullptr);
  QgsRasterLayer* demLayer = findPreferredDem(project);
  if (demLayer && QFileInfo::exists(demLayer->source())) {
    registerDemWideLayers(project, canvas, demLayer->source(), surveyPath, nullptr);
    demLayer = findPreferredDem(project);
  }

  constexpr int kGridN = 160;
  const QgsRectangle gridBox = surveyExtentRadius(project, canvas, 1500.0, nullptr, nullptr);
  const double cellSz = std::max(5.0, gridBox.width() / kGridN);
  hgis::paleo::GridTransform tf{gridBox.xMinimum(), gridBox.yMaximum(), cellSz, kGridN, kGridN};
  const size_t totalCells = static_cast<size_t>(kGridN) * kGridN;

  std::vector<double> zCurr(totalCells, 0.0);
  if (demLayer && demLayer->isValid() && QFileInfo::exists(demLayer->source())) {
    try {
      QgsRectangle dBox = gridBox;
      if (demLayer->crs().isValid() && demLayer->crs() != workCrs)
        dBox = QgsCoordinateTransform(workCrs, demLayer->crs(), project->transformContext()).transformBoundingBox(gridBox);
      std::vector<float> zBuf;
      DemAnalyzer::RasterInfo rInfo;
      if (DemAnalyzer::readFloatWindow(demLayer->source(), dBox.xMinimum(), dBox.yMinimum(), dBox.xMaximum(), dBox.yMaximum(), kGridN, &zBuf, &rInfo, nullptr) &&
          rInfo.width > 0 && rInfo.height > 0) {
        double lastValid = 0.0;
        for (float v : zBuf) {
          if (std::isfinite(v) && (!rInfo.hasNoData || v != rInfo.noData)) { lastValid = v; break; }
        }
        for (int r = 0; r < kGridN; ++r) {
          const int sr = std::clamp(r * rInfo.height / kGridN, 0, rInfo.height - 1);
          for (int c = 0; c < kGridN; ++c) {
            const int sc = std::clamp(c * rInfo.width / kGridN, 0, rInfo.width - 1);
            const float v = zBuf[static_cast<size_t>(sr) * rInfo.width + sc];
            if (std::isfinite(v) && (!rInfo.hasNoData || v != rInfo.noData)) lastValid = v;
            zCurr[static_cast<size_t>(r) * kGridN + c] = lastValid;
          }
        }
      }
    } catch (...) { KA_LOG_EXCEPT(); }
  }

  std::vector<hgis::paleo::SoilLandformCode> soilGrid(totalCells, hgis::paleo::SoilLandformCode::FluvialPlain);
  std::vector<hgis::paleo::GeologyCode> geolGrid(totalCells, hgis::paleo::GeologyCode::QaAlluvium);
  std::vector<uint8_t> riverMask(totalCells, 0), aoiMask(totalCells, 0);
  for (int r = 0; r < kGridN; ++r) {
    for (int c = 0; c < kGridN; ++c) {
      double wx = 0.0, wy = 0.0;
      tf.pixel_to_world(r, c, wx, wy);
      const QgsPointXY pt(wx, wy);
      if (!aoiGeom.isEmpty() && aoiGeom.contains(&pt)) aoiMask[static_cast<size_t>(r) * kGridN + c] = 1;
    }
  }
  if (QgsVectorLayer* soilVl = PaleoLandformService::findSoilTerrainLayer(project)) {
    const int iGeo = soilVl->fields().indexOf(QStringLiteral("soil_type_geo"));
    if (iGeo >= 0)
      rasterizePolyGrid(soilVl, workCrs, project, gridBox, tf, [&](const QgsFeature& f, size_t idx) {
        soilGrid[idx] = mapSoilCode(f.attribute(iGeo).toString());
      });
  }
  if (auto* geolVl = qobject_cast<QgsVectorLayer*>(GeologyMapService::existingGeologyLayer(project))) {
    const int iSym = geolVl->fields().indexOf(QStringLiteral("기호"));
    const int iEra = geolVl->fields().indexOf(QStringLiteral("era_class"));
    if (iSym >= 0)
      rasterizePolyGrid(geolVl, workCrs, project, gridBox, tf, [&](const QgsFeature& f, size_t idx) {
        geolGrid[idx] = mapGeolSymbol(f.attribute(iSym).toString(), iEra >= 0 ? f.attribute(iEra).toString() : QString());
      });
  }

  QString riverName = QStringLiteral("인접 하천");
  for (QgsMapLayer* l : project->mapLayers()) {
    auto* rv = qobject_cast<QgsVectorLayer*>(l);
    if (!rv || !rv->isValid() || (!rv->name().contains(QStringLiteral("수계")) && rv->fields().indexOf(QStringLiteral("riv_nm")) < 0))
      continue;
    const int iNm = rv->fields().indexOf(QStringLiteral("riv_nm"));
    double bestDist = 1e18;
    const QgsCoordinateTransform toWork(rv->crs(), workCrs, project->transformContext());
    const bool needTr = rv->crs().isValid() && rv->crs() != workCrs;
    QgsFeature rf;
    QgsFeatureIterator it = rv->getFeatures();
    while (it.nextFeature(rf)) {
      if (!rf.hasGeometry()) continue;
      QgsGeometry g = rf.geometry();
      if (needTr) { try { g.transform(toWork); } catch (...) { KA_LOG_EXCEPT(); continue; } }
      const double d = g.distance(QgsGeometry::fromPointXY(centerPt));
      if (iNm >= 0 && d < bestDist && !rf.attribute(iNm).toString().trimmed().isEmpty()) {
        bestDist = d;
        riverName = rf.attribute(iNm).toString().trimmed();
      }
    }
    rasterizePolyGrid(rv, workCrs, project, gridBox, tf, [&](const QgsFeature&, size_t idx) {
      riverMask[idx] = 1;
    });
    break;
  }

  const auto rawBoreholes = collectBoreholes(project, workCrs, centerPt);
  const QByteArray rUtf8 = riverName.toUtf8();
  res = hgis::paleo::HgisPaleoEngine::run_pipeline(
      zCurr, tf, rawBoreholes, soilGrid, geolGrid, riverMask, aoiMask,
      {centerPt.x(), centerPt.y()}, rUtf8.constData(), "06 하성평탄지·08 홍적대지", "Qa 제4기 충적층·Qr 하안단구층");

  const QString wkt = workCrs.toWkt();
  const QString classTif = QDir(paleoDir).filePath(QStringLiteral("고지형_7대분류도.tif"));
  const QString remTif = QDir(paleoDir).filePath(QStringLiteral("고지형_하상비고_REM.tif"));
  for (const QString& title : {QStringLiteral("[고지형] 하상비고(REM)"), QStringLiteral("[고지형] 7대 고지형 분류도")})
    for (QgsMapLayer* l : project->mapLayersByName(title))
      if (l) project->removeMapLayer(l->id());
  QFile::remove(classTif);
  QFile::remove(remTif);
  writeRgbaGeoTiff(classTif, tf, wkt, res.paleo_rgba_buffer, nullptr);
  writeFloatGeoTiff(remTif, tf, wkt, res.rem, nullptr);
  if (auto* remLayer = registerNamedRaster(project, remTif, QStringLiteral("[고지형] 하상비고(REM)")))
    DemPresentation::apply(remLayer, QStringLiteral("viewport"), remLayer->extent());
  registerNamedRaster(project, classTif, QStringLiteral("[고지형] 7대 고지형 분류도"));
  registerBoreholeMarkers(project, QDir(paleoDir).filePath(QStringLiteral("고지형_시추마커.gpkg")), res.parsed_boreholes);
  if (QgsVectorLayer* soil = PaleoLandformService::findSoilTerrainLayer(project)) {
    QString err;
    if (QgsVectorLayer* paleoVl = PaleoLandformService::ensureInterpretationLayer(project, surveyPath, &err)) {
      PaleoLandformService::seedInterpretationFromSoil(soil, paleoVl, &err);
      PaleoDemClassifier::enrichInterpretationLayer(project, paleoVl);
      PaleoLandformService::applyInterpretationStyle(paleoVl);
    }
  }
  applyEvidenceToSheet(project, res);
  if (canvas) LayerOps::refreshCanvasIfIdle(canvas);
  return res;
}

bool PaleoAnalysisRunner::applyEvidenceToSheet(QgsProject* project,
                                               const hgis::paleo::PaleoAnalysisResult& res) {
  if (!project || !project->layoutManager()) return false;
  auto* ly = dynamic_cast<QgsPrintLayout*>(project->layoutManager()->layoutByName(QStringLiteral("user_sheet")));
  if (!ly) return false;
  auto* label = dynamic_cast<QgsLayoutItemLabel*>(ly->itemById(QStringLiteral("ka_paleo_evidence_box")));
  if (!label) {
    label = new QgsLayoutItemLabel(ly);
    label->setId(QStringLiteral("ka_paleo_evidence_box"));
    ly->addLayoutItem(label);
    label->attemptMove(QgsLayoutPoint(12.0, 210.0, Qgis::LayoutUnit::Millimeters));
    label->attemptResize(QgsLayoutSize(185.0, 48.0, Qgis::LayoutUnit::Millimeters));
  }
  label->setText(QString::fromUtf8(res.aoi_summary_text.c_str()));
  label->setBackgroundEnabled(true);
  label->setBackgroundColor(QColor(255, 255, 255, 235));
  label->setFrameEnabled(true);
  QgsTextFormat tf;
  tf.setFont(QFont(QStringLiteral("Malgun Gothic"), 7)); tf.setSize(7.2); tf.setColor(QColor(20, 24, 31));
  label->setTextFormat(tf); label->setMarginX(2.0); label->setMarginY(2.0);
  return true;
}
