#include "PaleoLandformService.h"
#include "LayerOps.h"
#include "SoilMapService.h"

#include <QColor>
#include <QFileInfo>
#include <QSet>
#include <algorithm>
#include <cmath>

#include <qgsfeaturerequest.h>

#include <qgscategorizedsymbolrenderer.h>
#include <qgssinglesymbolrenderer.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransform.h>
#include <qgsexception.h>
#include <qgsfeature.h>
#include <qgsfeatureid.h>
#include <qgsfeatureiterator.h>
#include <qgsgeometry.h>
#include <qgis.h>
#include <qgsfield.h>
#include <qgsfields.h>
#include <qgsfillsymbol.h>
#include <qgsmaplayer.h>
#include <qgsproject.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayer.h>

namespace {

const char* kCandidates[] = {"04", "05", "06", "08"};

const char* kTerrainCodes[] = {"01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "99"};

struct KindStyle {
  const char* kind;
  int r, g, b, a;
};

constexpr KindStyle kKinds[] = {
    {"자연제방", 210, 140, 70, 150}, {"선상지", 230, 200, 110, 150},
    {"하안단구", 200, 110, 40, 150}, {"구하도", 40, 90, 160, 160},
    {"미고지", 190, 70, 70, 150},    {"미저지", 70, 130, 95, 150},
    {"해성평탄", 80, 155, 200, 150}, {"개변", 120, 120, 120, 110},
    {"미분류", 90, 90, 90, 90},
};

QgsSymbol* fillAt(int r, int g, int b, int a, double outlineMm) {
  QColor fill(r, g, b, a);
  QColor outline(40, 44, 52, std::min(220, a + 80));
  auto fs = QgsFillSymbol::createSimple({
      {QStringLiteral("color"), fill.name(QColor::HexArgb)},
      {QStringLiteral("outline_color"), outline.name(QColor::HexArgb)},
      {QStringLiteral("outline_width"), QString::number(outlineMm, 'f', 2)},
      {QStringLiteral("outline_width_unit"), QStringLiteral("MM")},
  });
  return fs.release();
}

}  // namespace

bool PaleoLandformService::isCandidateTerrainCode(const QString& code) {
  for (const char* c : kCandidates) {
    if (code == QLatin1String(c)) return true;
  }
  return false;
}

QStringList PaleoLandformService::candidateTerrainCodes() {
  return {QStringLiteral("04"), QStringLiteral("05"), QStringLiteral("06"), QStringLiteral("08")};
}

QStringList PaleoLandformService::interpretationKinds() {
  QStringList out;
  for (const KindStyle& k : kKinds)
    out.append(QString::fromUtf8(k.kind));
  return out;
}

bool PaleoLandformService::applyCandidateEmphasis(QgsVectorLayer* soilLayer) {
  if (!soilLayer || !soilLayer->isValid()) return false;
  if (soilLayer->fields().indexOf(QStringLiteral("soil_type_geo")) < 0) return false;

  QgsCategoryList cats;
  for (const char* code : kTerrainCodes) {
    const QString id = QString::fromLatin1(code);
    QColor c = SoilMapService::terrainColor(id);
    const bool cand = isCandidateTerrainCode(id);
    if (cand) {
      c.setAlpha(220);
    } else {
      c = QColor((c.red() + 440) / 3, (c.green() + 440) / 3, (c.blue() + 440) / 3, 95);
    }
    const QString legend = cand ? QStringLiteral("입지후보 · %1").arg(SoilMapService::terrainName(id))
                                : SoilMapService::terrainName(id);
    if (QgsSymbol* sym = fillAt(c.red(), c.green(), c.blue(), c.alpha(), cand ? 0.55 : 0.28))
      cats.append(QgsRendererCategory(QVariant(id), sym, legend));
  }
  if (QgsSymbol* rest = fillAt(200, 200, 200, 90, 0.2))
    cats.append(QgsRendererCategory(QVariant(), rest, QStringLiteral("미분류")));
  if (cats.isEmpty()) return false;
  soilLayer->setRenderer(new QgsCategorizedSymbolRenderer(QStringLiteral("soil_type_geo"), cats));
  SoilMapService::applyTerrainLabels(soilLayer, 50000.0, true);
  soilLayer->triggerRepaint();
  return true;
}

namespace {

QString normalizeTerrainCode(QString code) {
  code = code.trimmed();
  if (code.size() == 1 && code[0].isDigit())
    return QStringLiteral("0") + code;
  return code;
}

bool addPaleoFeature(QgsVectorLayer* paleo, const QgsGeometry& geom, const QString& kind,
                     const QString& note) {
  if (!paleo || geom.isEmpty() || geom.area() < 1.0) return false;
  const int iKind = paleo->fields().indexOf(QStringLiteral("kind"));
  const int iNote = paleo->fields().indexOf(QStringLiteral("note"));
  const int iStatus = paleo->fields().indexOf(QStringLiteral("status"));
  const QVector<QgsGeometry> parts =
      geom.isMultipart() ? geom.asGeometryCollection() : QVector<QgsGeometry>{geom};
  bool anyAdded = false;
  for (QgsGeometry part : parts) {
    part.convertToSingleType();
    if (part.isEmpty() || part.area() < 1.0) continue;
    QgsFeature f(paleo->fields());
    f.setGeometry(part);
    if (iKind >= 0) f.setAttribute(iKind, kind);
    if (iNote >= 0) f.setAttribute(iNote, note);
    if (iStatus >= 0) f.setAttribute(iStatus, QStringLiteral("가설"));
    if (paleo->addFeature(f)) anyAdded = true;
  }
  return anyAdded;
}

}  // namespace

QString PaleoLandformService::suggestKindFromTerrain(const QString& code) {
  const QString c = normalizeTerrainCode(code);
  if (c == QLatin1String("04")) return QStringLiteral("선상지");
  if (c == QLatin1String("05")) return QStringLiteral("해성평탄");
  if (c == QLatin1String("08")) return QStringLiteral("하안단구");
  return {};
}

PaleoLandformService::SeedResult PaleoLandformService::seedInterpretationFromSoil(
    QgsVectorLayer* soilLayer, QgsVectorLayer* paleoLayer, QString* errorOut) {
  SeedResult out;
  if (!soilLayer || !soilLayer->isValid() || !paleoLayer || !paleoLayer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("토양도 또는 고지형 판독 레이어가 없습니다.");
    return out;
  }
  if (soilLayer->fields().indexOf(QStringLiteral("soil_type_geo")) < 0 ||
      paleoLayer->fields().indexOf(QStringLiteral("kind")) < 0) {
    if (errorOut) *errorOut = QStringLiteral("필요한 필드가 없습니다.");
    return out;
  }

  const bool startedHere = !paleoLayer->isEditable();
  if (startedHere && !paleoLayer->startEditing()) {
    if (errorOut)
      *errorOut = QStringLiteral("고지형 판독을 편집할 수 없습니다.");
    return out;
  }

  QgsFeatureIterator pit = paleoLayer->getFeatures();
  QgsFeature existing;
  QgsFeatureIds drop;
  while (pit.nextFeature(existing)) {
    const QString note = existing.attribute(QStringLiteral("note")).toString();
    if (note.startsWith(QStringLiteral("자동:")))
      drop.insert(existing.id());
    else
      ++out.keptUser;
  }
  for (QgsFeatureId id : drop) {
    if (paleoLayer->deleteFeature(id))
      ++out.replaced;
  }

  QgsCoordinateTransform tr;
  bool needTr = false;
  if (soilLayer->crs().isValid() && paleoLayer->crs().isValid() &&
      soilLayer->crs() != paleoLayer->crs()) {
    tr = QgsCoordinateTransform(soilLayer->crs(), paleoLayer->crs(), QgsProject::instance());
    needTr = tr.isValid();
  }

  QgsFeature f;
  QgsFeatureIterator sit = soilLayer->getFeatures();
  while (sit.nextFeature(f)) {
    if (!f.hasGeometry() || f.geometry().isEmpty())
      continue;
    const QString code = normalizeTerrainCode(f.attribute(QStringLiteral("soil_type_geo")).toString());
    if (!isCandidateTerrainCode(code))
      continue;
    QgsGeometry g = f.geometry();
    if (needTr) {
      try {
        if (g.transform(tr) != Qgis::GeometryOperationResult::Success)
          continue;
      } catch (const QgsException&) {
        continue;
      }
    }
    if (code == QLatin1String("06")) {
      const double area = std::max(0.0, g.area());
      const QgsRectangle box = g.boundingBox();
      const double shortSide = std::min(box.width(), box.height());
      const double r = std::sqrt(area / 3.14159265358979323846);
      const double inset = std::clamp(r * 0.22, 8.0, 80.0);
      QgsGeometry core = g.buffer(-inset, 8);
      const bool wideEnough = shortSide >= inset * 3.0 && !core.isEmpty() &&
                              core.area() > area * 0.12;
      if (wideEnough) {
        QgsGeometry rim = g.difference(core);
        if (addPaleoFeature(paleoLayer, core, QStringLiteral("구하도"),
                            QStringLiteral("자동: 하성평탄 안쪽")))
          ++out.added;
        if (!rim.isEmpty() &&
            addPaleoFeature(paleoLayer, rim, QStringLiteral("자연제방"),
                            QStringLiteral("자동: 하성평탄 가장자리")))
          ++out.added;
      } else if (addPaleoFeature(paleoLayer, g, QStringLiteral("미저지"),
                                QStringLiteral("자동: 하성평탄 (폭이 좁아 분할 안 함)"))) {
        ++out.added;
      }
      continue;
    }
    const QString kind = suggestKindFromTerrain(code);
    if (kind.isEmpty())
      continue;
    if (addPaleoFeature(paleoLayer, g, kind,
                        QStringLiteral("자동: 흙토람 %1").arg(SoilMapService::terrainName(code))))
      ++out.added;
  }

  if (!paleoLayer->commitChanges(false)) {
    if (errorOut)
      *errorOut = paleoLayer->commitErrors().join(QLatin1Char('\n'));
    paleoLayer->rollBack();
    if (startedHere)
      paleoLayer->startEditing();
    out.added = 0;
    return out;
  }
  applyInterpretationStyle(paleoLayer);
  if (!paleoLayer->isEditable())
    paleoLayer->startEditing();
  return out;
}

QgsVectorLayer* PaleoLandformService::findSoilTerrainLayer(QgsProject* project) {
  if (!project) return nullptr;
  for (QgsMapLayer* l : project->mapLayers()) {
    auto* v = qobject_cast<QgsVectorLayer*>(l);
    if (v && v->isValid() && v->fields().indexOf(QStringLiteral("soil_type_geo")) >= 0)
      return v;
  }
  return nullptr;
}

bool PaleoLandformService::applyInterpretationStyle(QgsVectorLayer* layer) {
  if (!layer || !layer->isValid()) return false;
  if (layer->fields().indexOf(QStringLiteral("kind")) < 0) return false;
  if (layer->featureCount() <= 0) {
    if (QgsSymbol* outline = fillAt(255, 255, 255, 0, 0.45))
      layer->setRenderer(new QgsSingleSymbolRenderer(outline));
    layer->triggerRepaint();
    return true;
  }
  // Every entry is a hypothesis. Kinds seeded automatically from soil terrain
  // (note "자동:") say so in the legend, because the sheet legend copies these
  // labels and an automatic inner-buffer split is not a landform finding.
  QSet<QString> automaticKinds;
  const int kindIdx = layer->fields().indexOf(QStringLiteral("kind"));
  const int noteIdx = layer->fields().indexOf(QStringLiteral("note"));
  if (noteIdx >= 0) {
    QgsFeatureRequest request;
    request.setFlags(Qgis::FeatureRequestFlag::NoGeometry);
    request.setSubsetOfAttributes(QgsAttributeList{kindIdx, noteIdx});
    QgsFeatureIterator it = layer->getFeatures(request);
    QgsFeature feature;
    while (it.nextFeature(feature)) {
      if (feature.attribute(noteIdx).toString().startsWith(QStringLiteral("자동:")))
        automaticKinds.insert(feature.attribute(kindIdx).toString());
    }
  }
  QgsCategoryList cats;
  for (const KindStyle& k : kKinds) {
    const QString kind = QString::fromUtf8(k.kind);
    const QString label = automaticKinds.contains(kind) ? QStringLiteral("%1 · 자동 가설").arg(kind)
                                                        : QStringLiteral("%1 · 가설").arg(kind);
    if (QgsSymbol* sym = fillAt(k.r, k.g, k.b, k.a, 0.35))
      cats.append(QgsRendererCategory(QVariant(kind), sym, label));
  }
  layer->setRenderer(new QgsCategorizedSymbolRenderer(QStringLiteral("kind"), cats));
  layer->triggerRepaint();
  return true;
}

QgsVectorLayer* PaleoLandformService::ensureInterpretationLayer(QgsProject* project,
                                                               const QString& gpkgPath,
                                                               QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("프로젝트가 없습니다.");
    return nullptr;
  }
  if (auto* existing = LayerOps::findByLayerKey(project, QLatin1String(kLayerKey))) {
    applyInterpretationStyle(existing);
    return existing;
  }
  if (gpkgPath.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("먼저 「새 조사」로 저장 위치를 만드세요.");
    return nullptr;
  }

  const QString title = QString::fromUtf8(kLayerTitle);
  auto* vl = new QgsVectorLayer(QStringLiteral("%1|layername=%2").arg(gpkgPath, QLatin1String(kLayerKey)),
                                title, QStringLiteral("ogr"));
  if (!vl->isValid()) {
    delete vl;
    vl = nullptr;
    const QgsCoordinateReferenceSystem crs = project->crs().isValid()
        ? project->crs()
        : QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186"));
    QgsVectorLayer mem(QStringLiteral("Polygon?crs=%1").arg(crs.authid()), title,
                       QStringLiteral("memory"));
    if (!mem.isValid()) {
      if (errorOut) *errorOut = QStringLiteral("고지형 판독 레이어를 만들지 못했습니다.");
      return nullptr;
    }
    QgsFields fields;
    fields.append(QgsField(QStringLiteral("kind"), QMetaType::Type::QString));
    fields.append(QgsField(QStringLiteral("note"), QMetaType::Type::QString));
    fields.append(QgsField(QStringLiteral("status"), QMetaType::Type::QString));
    mem.dataProvider()->addAttributes(fields.toList());
    mem.updateFields();
    mem.setCrs(crs);
    QgsVectorFileWriter::SaveVectorOptions opts;
    opts.driverName = QStringLiteral("GPKG");
    opts.layerName = QLatin1String(kLayerKey);
    opts.fileEncoding = QStringLiteral("UTF-8");
    opts.actionOnExistingFile = QFileInfo::exists(gpkgPath)
                                    ? QgsVectorFileWriter::CreateOrOverwriteLayer
                                    : QgsVectorFileWriter::CreateOrOverwriteFile;
    QString errMsg, newFn, newLayer;
    if (QgsVectorFileWriter::writeAsVectorFormatV3(&mem, gpkgPath, project->transformContext(), opts,
                                                   &errMsg, &newFn, &newLayer) !=
        QgsVectorFileWriter::NoError) {
      if (errorOut) *errorOut = errMsg;
      return nullptr;
    }
    vl = new QgsVectorLayer(QStringLiteral("%1|layername=%2").arg(gpkgPath, QLatin1String(kLayerKey)),
                            title, QStringLiteral("ogr"));
  }
  if (!vl || !vl->isValid()) {
    if (errorOut && errorOut->isEmpty())
      *errorOut = vl ? vl->error().message() : QStringLiteral("고지형 판독 레이어를 열 수 없습니다.");
    delete vl;
    return nullptr;
  }
  vl->setName(title);
  vl->setCustomProperty(QString::fromUtf8(LayerOps::kPropLayerKey), QLatin1String(kLayerKey));
  LayerOps::markReferenceLayer(vl);
  LayerOps::applyLegendCrsLabel(vl);
  applyInterpretationStyle(vl);
  LayerOps::applyThematicOverlayScaleRange(vl);
  if (!project->addMapLayer(vl, true)) {
    delete vl;
    if (errorOut) *errorOut = QStringLiteral("고지형 판독 레이어를 넣지 못했습니다.");
    return nullptr;
  }
  LayerOps::placeInLegendGroup(project, vl, QStringLiteral("참조 지도"));
  return vl;
}
