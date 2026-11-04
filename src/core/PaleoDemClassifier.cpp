#include "PaleoDemClassifier.h"
#include "BasemapDsm.h"
#include "DemAnalyzer.h"
#include "KaLogExcept.h"

#include <QStringList>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransform.h>
#include <qgsexception.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsgeometry.h>
#include <qgsmaplayer.h>
#include <qgspointxy.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgsrectangle.h>
#include <qgsvectorlayer.h>

namespace {

QgsVectorLayer* findRiverLayer(QgsProject* project) {
  if (!project) return nullptr;
  for (QgsMapLayer* l : project->mapLayers()) {
    auto* v = qobject_cast<QgsVectorLayer*>(l);
    if (v && v->isValid() &&
        (v->name().contains(QStringLiteral("수계")) || v->fields().indexOf(QStringLiteral("riv_nm")) >= 0))
      return v;
  }
  return nullptr;
}

}  // namespace

PaleoDemClassifier::Classification PaleoDemClassifier::classify(const QString& baseKind,
                                                                const Metrics& m) {
  Classification out;
  out.kind = baseKind;
  QStringList notes;

  if (baseKind == QStringLiteral("하안단구")) {
    QString step = QStringLiteral("단구면");
    if (m.hasDem) {
      if (m.localReliefM >= 35.0)
        step = QStringLiteral("고위단구(T3, 비고 %1m)").arg(m.localReliefM, 0, 'f', 1);
      else if (m.localReliefM >= 15.0)
        step = QStringLiteral("중위단구(T2, 비고 %1m)").arg(m.localReliefM, 0, 'f', 1);
      else
        step = QStringLiteral("저위단구(T1, 비고 %1m)").arg(m.localReliefM, 0, 'f', 1);
    }
    notes.append(step);
  } else if (baseKind == QStringLiteral("자연제방") || baseKind == QStringLiteral("구하도") ||
             baseKind == QStringLiteral("미저지") || baseKind == QStringLiteral("미고지")) {
    if (m.hasBorehole) {
      if (m.depositEnv.contains(QStringLiteral("사질")) &&
          (baseKind == QStringLiteral("미저지") || baseKind == QStringLiteral("자연제방"))) {
        out.kind = QStringLiteral("자연제방");
      } else if (m.depositEnv.contains(QStringLiteral("점토")) &&
                 (baseKind == QStringLiteral("미저지") || baseKind == QStringLiteral("구하도"))) {
        out.kind = QStringLiteral("구하도");
      }
    } else if (m.hasDem && baseKind == QStringLiteral("미저지")) {
      if (m.localReliefM >= 2.5 && m.meanSlopeDeg <= 5.0)
        out.kind = QStringLiteral("미고지");
    }
    notes.append(out.kind);
  } else {
    notes.append(out.kind);
  }

  if (m.hasDem) {
    notes.append(QStringLiteral("표고 %1m(비고 +%2m)·경사 %3°")
                     .arg(m.meanElevM, 0, 'f', 1)
                     .arg(m.localReliefM, 0, 'f', 1)
                     .arg(m.meanSlopeDeg, 0, 'f', 1));
  }
  if (m.riverDistM >= 0.0) {
    notes.append(QStringLiteral("하천거리 %1m").arg(m.riverDistM, 0, 'f', 0));
  }
  if (m.hasBorehole) {
    QString bh = QStringLiteral("시추 %1 충적 %2m(%3)")
                     .arg(m.boreholeCode)
                     .arg(m.alluvialThicknessM, 0, 'f', 1)
                     .arg(m.dominantUscs.isEmpty() ? m.depositEnv : m.dominantUscs);
    if (m.weatheredDepthM >= 0.0)
      bh += QStringLiteral("·생토 %1m").arg(m.weatheredDepthM, 0, 'f', 1);
    notes.append(bh);
  }
  out.note = QStringLiteral("자동: ") + notes.join(QStringLiteral(" · "));
  return out;
}

PaleoDemClassifier::Metrics PaleoDemClassifier::samplePolygonMetrics(
    const QgsGeometry& geomWorkCrs, QgsProject* project, QgsRasterLayer* demLayer,
    double baseMinElevM) {
  Metrics m;
  if (geomWorkCrs.isEmpty() || !project) return m;
  const QgsCoordinateReferenceSystem workCrs =
      project->crs().isValid() ? project->crs()
                               : QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186"));

  if (demLayer && demLayer->isValid() && !demLayer->source().startsWith(QLatin1String("type="))) {
    try {
      QgsRectangle box = geomWorkCrs.boundingBox();
      if (demLayer->crs().isValid() && demLayer->crs() != workCrs) {
        const QgsCoordinateTransform tr(workCrs, demLayer->crs(), project->transformContext());
        box = tr.transformBoundingBox(box);
      }
      std::vector<float> z;
      DemAnalyzer::RasterInfo info;
      if (DemAnalyzer::readFloatWindow(demLayer->source(), box.xMinimum(), box.yMinimum(),
                                       box.xMaximum(), box.yMaximum(), 64, &z, &info, nullptr) &&
          !z.empty()) {
        double sum = 0.0;
        double zMin = std::numeric_limits<double>::infinity();
        double zMax = -std::numeric_limits<double>::infinity();
        int count = 0;
        for (float v : z) {
          if (!std::isfinite(v) || (info.hasNoData && v == info.noData)) continue;
          sum += v;
          zMin = std::min<double>(zMin, v);
          zMax = std::max<double>(zMax, v);
          ++count;
        }
        if (count > 0) {
          m.hasDem = true;
          m.minElevM = zMin;
          m.maxElevM = zMax;
          m.meanElevM = sum / count;
          const double refMin = std::isfinite(baseMinElevM) ? std::min(baseMinElevM, zMin) : zMin;
          m.localReliefM = std::max(0.0, m.meanElevM - refMin);

          if (demLayer->crs().isGeographic()) {
            info.geotransform[1] *= 88800.0;
            info.geotransform[5] *= 111000.0;
          }
          std::vector<float> slopes;
          DemAnalyzer::slopeDegrees(z, info, 1.0, &slopes);
          double sSum = 0.0;
          int sCount = 0;
          for (float s : slopes) {
            if (std::isfinite(s) && s > 0.0f) {
              sSum += s;
              ++sCount;
            }
          }
          m.meanSlopeDeg = sCount > 0 ? (sSum / sCount) : 0.0;
        }
      }
    } catch (...) {
      KA_LOG_EXCEPT();
    }
  }

  if (QgsVectorLayer* river = findRiverLayer(project)) {
    try {
      QgsCoordinateTransform tr(river->crs(), workCrs, project->transformContext());
      const bool needTr = river->crs().isValid() && river->crs() != workCrs;
      double best = std::numeric_limits<double>::infinity();
      QgsFeature rf;
      QgsFeatureIterator it = river->getFeatures();
      while (it.nextFeature(rf)) {
        if (!rf.hasGeometry()) continue;
        QgsGeometry rg = rf.geometry();
        if (needTr && rg.transform(tr) != Qgis::GeometryOperationResult::Success) continue;
        const double d = geomWorkCrs.distance(rg);
        if (std::isfinite(d) && d >= 0.0) best = std::min(best, d);
      }
      if (std::isfinite(best)) m.riverDistM = best;
    } catch (...) {
      KA_LOG_EXCEPT();
    }
  }

  if (QgsVectorLayer* bh = GeoInfoBoreholeService::findBoreholeLayer(project)) {
    try {
      QgsCoordinateTransform tr(bh->crs(), workCrs, project->transformContext());
      const bool needTr = bh->crs().isValid() && bh->crs() != workCrs;
      double bestDist = 300.0; // 300 m 이내 가장 가까운 시추공 매칭
      QgsFeature bf;
      QgsFeatureIterator it = bh->getFeatures();
      while (it.nextFeature(bf)) {
        if (!bf.hasGeometry()) continue;
        QgsGeometry bg = bf.geometry();
        if (needTr && bg.transform(tr) != Qgis::GeometryOperationResult::Success) continue;
        const double d = geomWorkCrs.distance(bg);
        if (std::isfinite(d) && d <= bestDist) {
          bestDist = d;
          m.hasBorehole = true;
          m.boreholeCode = bf.attribute(QStringLiteral("hole_code")).toString();
          m.alluvialThicknessM = bf.attribute(QStringLiteral("alluvial_m")).toDouble();
          m.weatheredDepthM = bf.attribute(QStringLiteral("weathered_m")).toDouble();
          m.waterLevelM = bf.attribute(QStringLiteral("water_m")).toDouble();
          m.dominantUscs = bf.attribute(QStringLiteral("uscs")).toString();
          m.depositEnv = bf.attribute(QStringLiteral("deposit_env")).toString();
        }
      }
    } catch (...) {
      KA_LOG_EXCEPT();
    }
  }
  return m;
}

int PaleoDemClassifier::enrichInterpretationLayer(QgsProject* project, QgsVectorLayer* paleoLayer) {
  if (!project || !paleoLayer || !paleoLayer->isValid()) return 0;
  QgsRasterLayer* dem = BasemapDsm::findDem(project);
  QgsVectorLayer* bh = GeoInfoBoreholeService::findBoreholeLayer(project);
  QgsVectorLayer* river = findRiverLayer(project);
  if (!dem && !bh && !river) return 0;

  const int iKind = paleoLayer->fields().indexOf(QStringLiteral("kind"));
  const int iNote = paleoLayer->fields().indexOf(QStringLiteral("note"));
  if (iKind < 0 || iNote < 0) return 0;

  double baseMinElev = std::numeric_limits<double>::infinity();
  if (dem && dem->isValid() && !dem->source().startsWith(QLatin1String("type="))) {
    try {
      QgsRectangle ext = paleoLayer->extent();
      if (dem->crs().isValid() && paleoLayer->crs().isValid() && dem->crs() != paleoLayer->crs()) {
        const QgsCoordinateTransform tr(paleoLayer->crs(), dem->crs(), project->transformContext());
        ext = tr.transformBoundingBox(ext);
      }
      double low = 0.0, high = 0.0;
      std::vector<float> z;
      DemAnalyzer::RasterInfo info;
      if (DemAnalyzer::readFloatWindow(dem->source(), ext.xMinimum(), ext.yMinimum(),
                                       ext.xMaximum(), ext.yMaximum(), 64, &z, &info, nullptr)) {
        for (float v : z) {
          if (std::isfinite(v) && (!info.hasNoData || v != info.noData))
            baseMinElev = std::min<double>(baseMinElev, v);
        }
      }
      Q_UNUSED(low);
      Q_UNUSED(high);
    } catch (...) {
      KA_LOG_EXCEPT();
    }
  }

  const bool wasEditable = paleoLayer->isEditable();
  if (!wasEditable && !paleoLayer->startEditing()) return 0;

  int updated = 0;
  QgsFeature f;
  QgsFeatureIterator it = paleoLayer->getFeatures();
  while (it.nextFeature(f)) {
    const QString note = f.attribute(iNote).toString();
    if (!note.startsWith(QStringLiteral("자동:"))) continue;
    const QString kind = f.attribute(iKind).toString();
    const Metrics m = samplePolygonMetrics(f.geometry(), project, dem, baseMinElev);
    if (!m.hasDem && !m.hasBorehole && m.riverDistM < 0.0) continue;
    const Classification c = classify(kind, m);
    paleoLayer->changeAttributeValue(f.id(), iKind, c.kind);
    paleoLayer->changeAttributeValue(f.id(), iNote, c.note);
    ++updated;
  }
  if (updated > 0) {
    paleoLayer->commitChanges(!wasEditable);
    if (wasEditable && !paleoLayer->isEditable()) paleoLayer->startEditing();
    paleoLayer->triggerRepaint();
  }
  return updated;
}
