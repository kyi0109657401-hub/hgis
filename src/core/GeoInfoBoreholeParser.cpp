#include "GeoInfoBoreholeService.h"
#include "HgisPaleoEngine.h"
#include "KaLogExcept.h"
#include "LayerOps.h"
#include "PaleoAnalysisRunner.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QStringList>
#include <algorithm>
#include <cmath>

#include <qgsbilinearrasterresampler.h>
#include <qgscolorrampshader.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransform.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsgeometry.h>
#include <qgsmapcanvas.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgsrasterresamplefilter.h>
#include <qgsrastershader.h>
#include <qgssinglebandpseudocolorrenderer.h>
#include <qgsvectorlayer.h>

namespace {

QString classifyUscsEnv(const QString& uscs, const QString& desc, double alluvialM) {
  if (alluvialM <= 0.01) return QStringLiteral("풍화대·기반암 우세");
  const QString u = uscs.trimmed().toUpper();
  if (u.startsWith(QLatin1Char('G')) || desc.contains(QStringLiteral("자갈")) ||
      desc.contains(QStringLiteral("호박돌")))
    return QStringLiteral("자갈·붕적(선상지·하상)");
  if (u.startsWith(QLatin1Char('C')) || u.startsWith(QLatin1Char('M')) ||
      u.startsWith(QLatin1Char('O')) || u.startsWith(QLatin1String("PT")) ||
      desc.contains(QStringLiteral("점토")) || desc.contains(QStringLiteral("실트")))
    return QStringLiteral("점토·실트 충적(구하도·저습지)");
  return QStringLiteral("사질 충적(자연제방·미고지)");
}

}  // namespace

bool GeoInfoBoreholeService::parseSichudanJson(const QByteArray& rawBytes, BoreholeRecord* out,
                                               QString* errorOut) {
  if (!out) return false;
  *out = BoreholeRecord{};
  QJsonObject obj = QJsonDocument::fromJson(rawBytes).object();
  if (obj.contains(QStringLiteral("jsonView"))) {
    const QByteArray inner = obj.value(QStringLiteral("jsonView")).toString().toUtf8();
    obj = QJsonDocument::fromJson(inner).object();
  }
  out->holeCode = obj.value(QStringLiteral("HCODE")).toString().trimmed();
  if (out->holeCode.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("시추공 코드(HCODE)가 없습니다.");
    return false;
  }
  out->projectCode = obj.value(QStringLiteral("PCODE")).toString().trimmed();
  out->projectName = obj.value(QStringLiteral("PNAME")).toString().trimmed();
  out->address = obj.value(QStringLiteral("PCOM")).toString().trimmed();
  out->altitudeM = obj.value(QStringLiteral("ALTITUDE")).toVariant().toDouble();
  out->totalDepthM = obj.value(QStringLiteral("DEPTH")).toVariant().toDouble();
  const QString waterStr = obj.value(QStringLiteral("WATER")).toVariant().toString().trimmed();
  bool waterOk = false;
  const double wVal = waterStr.toDouble(&waterOk);
  if (waterOk) {
    out->waterLevelM = wVal;
    out->hasWaterLevel = true;
  }
  const QJsonObject tm = obj.value(QStringLiteral("TM")).toObject();
  out->tmX = tm.value(QStringLiteral("X")).toVariant().toDouble();
  out->tmY = tm.value(QStringLiteral("Y")).toVariant().toDouble();

  QStringList parts;
  double bestThick = -1.0;
  QString bestDesc;
  const QJsonArray layerArr = obj.value(QStringLiteral("LAYER")).toArray();
  if (layerArr.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("시추공 지층(LAYER) 정보가 비어 있습니다.");
    return false;
  }
  for (const QJsonValue& lv : layerArr) {
    const QJsonObject lo = lv.toObject();
    StratumLayer s;
    s.fromM = lo.value(QStringLiteral("from")).toDouble();
    s.toM = lo.value(QStringLiteral("to")).toDouble();
    s.name = lo.value(QStringLiteral("name")).toString().trimmed();
    s.uscs = lo.value(QStringLiteral("uscs")).toString().trimmed();
    s.color = lo.value(QStringLiteral("color")).toString().trimmed();
    s.desc = lo.value(QStringLiteral("desc")).toString().trimmed();
    out->layers.append(s);
    const double thick = std::max(0.0, s.toM - s.fromM);
    parts.append(QStringLiteral("%1~%2m %3(%4)")
                     .arg(s.fromM, 0, 'f', 1)
                     .arg(s.toM, 0, 'f', 1)
                     .arg(s.name, s.uscs));
    const bool isFill =
        s.name.contains(QStringLiteral("매립")) || s.name.contains(QStringLiteral("성토"));
    const bool isRockOrWeathered =
        s.name.contains(QStringLiteral("풍화")) || s.name.contains(QStringLiteral("연암")) ||
        s.name.contains(QStringLiteral("경암")) || s.name.contains(QStringLiteral("기반암")) ||
        s.name.contains(QStringLiteral("보통암")) || s.name.contains(QStringLiteral("암반"));
    const QString u = s.uscs.trimmed().toUpper();
    const bool isNaturalSoil =
        s.name.contains(QStringLiteral("퇴적")) || s.name.contains(QStringLiteral("충적")) ||
        s.name.contains(QStringLiteral("붕적")) || s.name.contains(QStringLiteral("하성")) ||
        s.name.contains(QStringLiteral("점토")) || s.name.contains(QStringLiteral("실트")) ||
        s.name.contains(QStringLiteral("모래")) || s.name.contains(QStringLiteral("자갈")) ||
        s.name.contains(QStringLiteral("사질")) || s.name.contains(QStringLiteral("점성")) ||
        u.startsWith(QLatin1Char('C')) || u.startsWith(QLatin1Char('M')) ||
        u.startsWith(QLatin1Char('S')) || u.startsWith(QLatin1Char('G')) ||
        u.startsWith(QLatin1Char('O')) || u.startsWith(QLatin1String("PT"));
    if (isFill) {
      out->fillDepthM = std::max(out->fillDepthM, s.toM);
    } else if (isRockOrWeathered) {
      if (out->weatheredDepthM < 0.0) out->weatheredDepthM = s.fromM;
    } else if (out->weatheredDepthM < 0.0 && isNaturalSoil) {
      if (out->alluvialThicknessM <= 0.0) out->alluvialFromM = s.fromM;
      out->alluvialToM = std::max(out->alluvialToM, s.toM);
      out->alluvialThicknessM += thick;
      if (thick > bestThick) {
        bestThick = thick;
        out->dominantUscs = s.uscs;
        bestDesc = s.desc.isEmpty() ? s.name : s.desc;
      }
    }
  }
  out->layerSummary = parts.join(QStringLiteral(" / "));
  out->depositEnv = classifyUscsEnv(out->dominantUscs, bestDesc, out->alluvialThicknessM);
  QStringList hints;
  if (out->fillDepthM > 0.0)
    hints.append(QStringLiteral("매립·성토 %1m").arg(out->fillDepthM, 0, 'f', 1));
  if (out->alluvialThicknessM > 0.0)
    hints.append(QStringLiteral("충적·퇴적 %1~%2m(두께 %3m, %4) → %5")
                     .arg(out->alluvialFromM, 0, 'f', 1)
                     .arg(out->alluvialToM, 0, 'f', 1)
                     .arg(out->alluvialThicknessM, 0, 'f', 1)
                     .arg(out->dominantUscs.isEmpty() ? QStringLiteral("-") : out->dominantUscs,
                          out->depositEnv));
  if (out->weatheredDepthM >= 0.0)
    hints.append(QStringLiteral("생토·풍화대 %1m").arg(out->weatheredDepthM, 0, 'f', 1));
  if (out->hasWaterLevel)
    hints.append(QStringLiteral("지하수위 %1m").arg(out->waterLevelM, 0, 'f', 1));
  out->paleoHint = hints.join(QStringLiteral(" · "));
  return true;
}

QList<GeoInfoBoreholeService::HoleCandidate>
GeoInfoBoreholeService::selectSpatiallyBalancedCandidates(
    const QList<HoleCandidate>& rawCandidates, const QgsPointXY& center5186, int maxCount,
    double cellMeters) {
  if (rawCandidates.isEmpty() || maxCount <= 0) return {};
  QList<HoleCandidate> sorted = rawCandidates;
  std::sort(sorted.begin(), sorted.end(), [&center5186](const HoleCandidate& a, const HoleCandidate& b) {
    return a.pt5186.sqrDist(center5186) < b.pt5186.sqrDist(center5186);
  });
  const double cell = std::max(25.0, cellMeters);
  QList<QString> bucketOrder;
  QMap<QString, QList<HoleCandidate>> buckets;
  for (const HoleCandidate& c : sorted) {
    const QString projPrefix = c.code.section(QLatin1String("BH"), 0, 0).trimmed();
    const long long gx = static_cast<long long>(std::floor(c.pt5186.x() / cell));
    const long long gy = static_cast<long long>(std::floor(c.pt5186.y() / cell));
    const QString key = c.hasPt5186 ? QStringLiteral("%1:%2:%3").arg(projPrefix).arg(gx).arg(gy) : projPrefix;
    if (!buckets.contains(key)) bucketOrder.append(key);
    buckets[key].append(c);
  }
  QList<HoleCandidate> result;
  for (int pass = 0; result.size() < maxCount; ++pass) {
    bool addedAny = false;
    for (const QString& key : bucketOrder) {
      const auto& list = buckets[key];
      if (pass < list.size()) {
        result.append(list[pass]);
        addedAny = true;
        if (result.size() >= maxCount) break;
      }
    }
    if (!addedAny) break;
  }
  return result;
}

QgsRasterLayer* GeoInfoBoreholeService::registerAlluvialIsopachLayer(
    QgsProject* project, QgsMapCanvas* canvas, QgsVectorLayer* boreholeLayer,
    const QString& surveyPath) {
  if (!project || !boreholeLayer || !boreholeLayer->isValid()) return nullptr;
  const QgsCoordinateReferenceSystem workCrs =
      project->crs().isValid() ? project->crs() : QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187"));
  const QgsCoordinateTransform toWork(boreholeLayer->crs(), workCrs, project->transformContext());
  const bool needTr = boreholeLayer->crs().isValid() && boreholeLayer->crs() != workCrs;
  std::vector<double> xs, ys, vals;
  double minX = 1e18, maxX = -1e18, minY = 1e18, maxY = -1e18;
  QgsFeature f;
  QgsFeatureIterator it = boreholeLayer->getFeatures();
  while (it.nextFeature(f)) {
    if (!f.hasGeometry()) continue;
    QgsPointXY pt = f.geometry().asPoint();
    if (needTr) { try { pt = toWork.transform(pt); } catch (...) { KA_LOG_EXCEPT(); continue; } }
    xs.push_back(pt.x()); ys.push_back(pt.y());
    vals.push_back(std::max(0.0, f.attribute(QStringLiteral("alluvial_m")).toDouble()));
    minX = std::min(minX, pt.x()); maxX = std::max(maxX, pt.x());
    minY = std::min(minY, pt.y()); maxY = std::max(maxY, pt.y());
  }
  if (xs.size() < 2) return nullptr;
  const double pad = std::max(300.0, std::max(maxX - minX, maxY - minY) * 0.25);
  minX -= pad; maxX += pad; minY -= pad; maxY += pad;
  constexpr int kGrid = 180;
  const double cell = std::max(5.0, std::max(maxX - minX, maxY - minY) / kGrid);
  const hgis::paleo::GridTransform tf{minX, maxY, cell, kGrid, kGrid};
  const auto grid = hgis::paleo::HgisPaleoEngine::idw_interpolate_grid(tf, xs, ys, vals, 8, 2.0);
  const QString outDir = surveyPath.isEmpty() ? QDir::temp().filePath(QStringLiteral("ka-hgis-paleo"))
                                              : QFileInfo(surveyPath).absoluteDir().filePath(QStringLiteral("지형분석"));
  QDir().mkpath(outDir);
  const QString tifPath = QDir(outDir).filePath(QStringLiteral("시추_자연퇴적층_두께.tif"));
  const QString title = QStringLiteral("[시추·지반] 자연 퇴적층 두께(m)");
  for (QgsMapLayer* l : project->mapLayersByName(title)) if (l) project->removeMapLayer(l->id());
  QFile::remove(tifPath);
  if (!PaleoAnalysisRunner::writeFloatGeoTiff(tifPath, tf, workCrs.toWkt(), grid, nullptr)) return nullptr;
  auto* rl = new QgsRasterLayer(tifPath, title, QStringLiteral("gdal"));
  if (!rl->isValid()) { delete rl; return nullptr; }
  auto* fcn = new QgsColorRampShader(0.0, 8.0);
  fcn->setColorRampType(Qgis::ShaderInterpolationMethod::Linear);
  fcn->setColorRampItemList({
      QgsColorRampShader::ColorRampItem(0.5, QColor(254, 243, 199), QStringLiteral("0.5m 이하 (박층·기반암 직상)")),
      QgsColorRampShader::ColorRampItem(1.5, QColor(253, 224, 71), QStringLiteral("1.5m (완경사 퇴적)")),
      QgsColorRampShader::ColorRampItem(3.0, QColor(245, 158, 11), QStringLiteral("3.0m (중후층 충적·문화층 유망)")),
      QgsColorRampShader::ColorRampItem(5.0, QColor(217, 119, 6), QStringLiteral("5.0m (후층 충적)")),
      QgsColorRampShader::ColorRampItem(8.0, QColor(146, 64, 14), QStringLiteral("8.0m+ (심부 매몰곡·구하도)")),
  });
  auto* sh = new QgsRasterShader();
  sh->setRasterShaderFunction(fcn);
  rl->setRenderer(new QgsSingleBandPseudoColorRenderer(rl->dataProvider(), 1, sh));
  rl->setOpacity(0.72);
  if (auto* rf = rl->resampleFilter()) rf->setZoomedInResampler(new QgsBilinearRasterResampler());
  LayerOps::markReferenceLayer(rl);
  LayerOps::applyLegendCrsLabel(rl);
  if (!project->addMapLayer(rl, true)) { delete rl; return nullptr; }
  LayerOps::placeInLegendGroup(project, rl, QStringLiteral("참조 지도"));
  if (canvas) LayerOps::refreshCanvasIfIdle(canvas);
  return rl;
}
