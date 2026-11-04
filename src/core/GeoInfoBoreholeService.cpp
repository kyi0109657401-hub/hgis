#include "GeoInfoBoreholeService.h"
#include "KaLogExcept.h"
#include "LayerOps.h"
#include "ReferenceStorage.h"

#include <QColor>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QSet>
#include <QUrl>
#include <QUrlQuery>
#include <algorithm>

#include <qgsblockingnetworkrequest.h>
#include <qgscategorizedsymbolrenderer.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransform.h>
#include <qgscoordinatetransformcontext.h>
#include <qgsexception.h>
#include <qgsfeature.h>
#include <qgsfeedback.h>
#include <qgsfield.h>
#include <qgsfields.h>
#include <qgsgeometry.h>
#include <qgsmapcanvas.h>
#include <qgsmarkersymbol.h>
#include <qgspallabeling.h>
#include <qgspointxy.h>
#include <qgsproject.h>
#include <qgsrectangle.h>
#include <qgstextformat.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayerlabeling.h>

namespace {

constexpr const char* kLayerTitle = "시추주상도(국토지반정보)";
constexpr const char* kCustomTag = "ka_hgis/geoinfo_borehole";

QStringList fetchHoleCodesInExtent(const QgsRectangle& extent5186,
                                   const QgsCoordinateTransformContext& transformContext,
                                   QgsFeedback* feedback) {
  QStringList codes;
  QSet<QString> seen;
  try {
    const QgsCoordinateReferenceSystem crs5186(QStringLiteral("EPSG:5186"));
    const QgsCoordinateReferenceSystem crs3857(QStringLiteral("EPSG:3857"));
    const QgsCoordinateTransform tr(crs5186, crs3857, transformContext);
    const QgsRectangle b = tr.transformBoundingBox(extent5186);
    QUrl url(QStringLiteral("https://www.geoinfo.or.kr/proxyo2map.jsp"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("SERVICE"), QStringLiteral("WMS"));
    q.addQueryItem(QStringLiteral("VERSION"), QStringLiteral("1.3.0"));
    q.addQueryItem(QStringLiteral("REQUEST"), QStringLiteral("GetFeatureInfo"));
    q.addQueryItem(QStringLiteral("LAYERS"), QStringLiteral("TBL_HEADER_HOLE"));
    q.addQueryItem(QStringLiteral("QUERY_LAYERS"), QStringLiteral("TBL_HEADER_HOLE"));
    q.addQueryItem(QStringLiteral("STYLES"), QString());
    q.addQueryItem(QStringLiteral("CRS"), QStringLiteral("EPSG:3857"));
    q.addQueryItem(QStringLiteral("BBOX"),
                   QStringLiteral("%1,%2,%3,%4")
                       .arg(b.xMinimum(), 0, 'f', 2)
                       .arg(b.yMinimum(), 0, 'f', 2)
                       .arg(b.xMaximum(), 0, 'f', 2)
                       .arg(b.yMaximum(), 0, 'f', 2));
    q.addQueryItem(QStringLiteral("WIDTH"), QStringLiteral("2"));
    q.addQueryItem(QStringLiteral("HEIGHT"), QStringLiteral("2"));
    q.addQueryItem(QStringLiteral("I"), QStringLiteral("1"));
    q.addQueryItem(QStringLiteral("J"), QStringLiteral("1"));
    q.addQueryItem(QStringLiteral("INFO_FORMAT"), QStringLiteral("application/json"));
    q.addQueryItem(QStringLiteral("FEATURE_COUNT"), QStringLiteral("40"));
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setTransferTimeout(15000);
    QgsBlockingNetworkRequest net;
    if (net.get(req, false, feedback) == QgsBlockingNetworkRequest::NoError) {
      const QJsonObject root = QJsonDocument::fromJson(net.reply().content()).object();
      for (const QJsonValue& v : root.value(QStringLiteral("features")).toArray()) {
        const QJsonObject props = v.toObject().value(QStringLiteral("properties")).toObject();
        const QString code = props.value(QStringLiteral("HOLE_CODE")).toString().trimmed();
        const double x = props.value(QStringLiteral("HOLE_LOCATION_TM_X")).toDouble();
        const double y = props.value(QStringLiteral("HOLE_LOCATION_TM_Y")).toDouble();
        if (!code.isEmpty() && !seen.contains(code) &&
            (x <= 0.0 || extent5186.contains(QgsPointXY(x, y)))) {
          seen.insert(code);
          codes.append(code);
        }
      }
    }
  } catch (...) {
    KA_LOG_EXCEPT();
  }
  return codes;
}

}  // namespace

PreparedReferenceMap GeoInfoBoreholeService::prepare(
    const QgsRectangle& extent5186, const QString& requestedBasePath,
    const QgsCoordinateTransformContext& transformContext, QgsFeedback* feedback) {
  PreparedReferenceMap result;
  if (!ReferenceMapPreparation::initializeStorage(result, requestedBasePath)) return result;
  const QString outPath = result.gpkgPath;
  const QStringList codes = fetchHoleCodesInExtent(extent5186, transformContext, feedback);
  if (codes.isEmpty()) {
    result.error = QStringLiteral("현재 화면 범위 안에 등록된 국토지반정보 시추공이 없습니다.");
    return result;
  }

  QgsVectorLayer mem(QStringLiteral("Point?crs=EPSG:5186"), QString::fromUtf8(kLayerTitle),
                     QStringLiteral("memory"));
  QgsFields fields;
  fields.append(QgsField(QStringLiteral("hole_code"), QMetaType::Type::QString));
  fields.append(QgsField(QStringLiteral("project_name"), QMetaType::Type::QString));
  fields.append(QgsField(QStringLiteral("altitude_m"), QMetaType::Type::Double));
  fields.append(QgsField(QStringLiteral("fill_m"), QMetaType::Type::Double));
  fields.append(QgsField(QStringLiteral("alluvial_m"), QMetaType::Type::Double));
  fields.append(QgsField(QStringLiteral("weathered_m"), QMetaType::Type::Double));
  fields.append(QgsField(QStringLiteral("water_m"), QMetaType::Type::Double));
  fields.append(QgsField(QStringLiteral("uscs"), QMetaType::Type::QString));
  fields.append(QgsField(QStringLiteral("deposit_env"), QMetaType::Type::QString));
  fields.append(QgsField(QStringLiteral("paleo_hint"), QMetaType::Type::QString));
  fields.append(QgsField(QStringLiteral("layer_summary"), QMetaType::Type::QString));
  mem.dataProvider()->addAttributes(fields.toList());
  mem.updateFields();

  QgsFeatureList feats;
  const int limit = std::min<int>(codes.size(), 25);
  for (int i = 0; i < limit; ++i) {
    if (feedback && feedback->isCanceled()) {
      result.status = PreparedReferenceMap::Status::Cancelled;
      return result;
    }
    if (feedback) feedback->setProgress((i * 90.0) / std::max(1, limit));
    QUrl u(QStringLiteral("https://www.geoinfo.or.kr/map/getSichudanData.do"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("code"), codes[i]);
    u.setQuery(q);
    QNetworkRequest req(u);
    req.setTransferTimeout(10000);
    QgsBlockingNetworkRequest net;
    if (net.get(req, false, feedback) != QgsBlockingNetworkRequest::NoError) continue;
    BoreholeRecord rec;
    if (!parseSichudanJson(net.reply().content(), &rec) || rec.tmX <= 0.0 || rec.tmY <= 0.0)
      continue;
    QgsFeature f(mem.fields());
    f.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(rec.tmX, rec.tmY)));
    f.setAttribute(QStringLiteral("hole_code"), rec.holeCode);
    f.setAttribute(QStringLiteral("project_name"), rec.projectName);
    f.setAttribute(QStringLiteral("altitude_m"), rec.altitudeM);
    f.setAttribute(QStringLiteral("fill_m"), rec.fillDepthM);
    f.setAttribute(QStringLiteral("alluvial_m"), rec.alluvialThicknessM);
    f.setAttribute(QStringLiteral("weathered_m"), rec.weatheredDepthM);
    f.setAttribute(QStringLiteral("water_m"), rec.waterLevelM);
    f.setAttribute(QStringLiteral("uscs"), rec.dominantUscs);
    f.setAttribute(QStringLiteral("deposit_env"), rec.depositEnv);
    f.setAttribute(QStringLiteral("paleo_hint"), rec.paleoHint);
    f.setAttribute(QStringLiteral("layer_summary"), rec.layerSummary);
    feats.append(f);
  }
  if (feats.isEmpty()) {
    result.error = QStringLiteral("시추공 주상도 상세 데이터를 읽지 못했습니다.");
    return result;
  }
  mem.dataProvider()->addFeatures(feats);
  mem.updateExtents();

  QgsVectorFileWriter::SaveVectorOptions opts;
  opts.driverName = QStringLiteral("GPKG");
  opts.layerName = QStringLiteral("geoinfo_boreholes");
  opts.fileEncoding = QStringLiteral("UTF-8");
  QString errMsg, newFn, newLayer;
  if (QgsVectorFileWriter::writeAsVectorFormatV3(&mem, outPath, transformContext, opts, &errMsg,
                                                 &newFn, &newLayer) != QgsVectorFileWriter::NoError) {
    result.error = errMsg;
    return result;
  }
  result.tableName = QStringLiteral("geoinfo_boreholes");
  result.status = PreparedReferenceMap::Status::Ready;
  return result;
}

bool GeoInfoBoreholeService::applyBoreholeStyle(QgsVectorLayer* layer) {
  if (!layer || !layer->isValid()) return false;
  struct Cat {
    const char* env;
    const char* color;
  };
  constexpr Cat kCats[] = {
      {"사질 충적(자연제방·미고지)", "#d97706"},
      {"점토·실트 충적(구하도·저습지)", "#2563eb"},
      {"자갈·붕적(선상지·하상)", "#059669"},
      {"풍화대·기반암 우세", "#6b7280"},
  };
  QgsCategoryList cats;
  for (const Cat& c : kCats) {
    auto sym = QgsMarkerSymbol::createSimple({
        {QStringLiteral("name"), QStringLiteral("circle")},
        {QStringLiteral("color"), QString::fromLatin1(c.color)},
        {QStringLiteral("outline_color"), QStringLiteral("#1f2937")},
        {QStringLiteral("outline_width"), QStringLiteral("0.4")},
        {QStringLiteral("size"), QStringLiteral("3.4")},
    });
    const QString label = QString::fromUtf8(c.env);
    cats.append(QgsRendererCategory(QVariant(label), sym.release(), label));
  }
  layer->setRenderer(new QgsCategorizedSymbolRenderer(QStringLiteral("deposit_env"), cats));
  QgsPalLayerSettings pal;
  pal.fieldName = QStringLiteral("concat(\"hole_code\", ' (충적 ', round(\"alluvial_m\", 1), 'm)')");
  pal.isExpression = true;
  QgsTextFormat tf;
  tf.setSize(8.5);
  tf.setColor(QColor(30, 35, 42));
  tf.buffer().setEnabled(true);
  tf.buffer().setSize(0.8);
  tf.buffer().setColor(Qt::white);
  pal.setFormat(tf);
  layer->setLabeling(new QgsVectorLayerSimpleLabeling(pal));
  layer->setLabelsEnabled(true);
  layer->triggerRepaint();
  return true;
}

QgsVectorLayer* GeoInfoBoreholeService::findBoreholeLayer(QgsProject* project) {
  if (!project) return nullptr;
  for (QgsMapLayer* l : project->mapLayers()) {
    auto* v = qobject_cast<QgsVectorLayer*>(l);
    if (v && v->isValid() &&
        (v->customProperty(QString::fromLatin1(kCustomTag)).toBool() ||
         v->fields().indexOf(QStringLiteral("deposit_env")) >= 0))
      return v;
  }
  return nullptr;
}

QgsVectorLayer* GeoInfoBoreholeService::addPrepared(QgsProject* project, QgsMapCanvas* canvas,
                                                    const PreparedReferenceMap& prepared,
                                                    QString* errorOut) {
  if (!project || !prepared.isReady()) return nullptr;
  const QString uri = QStringLiteral("%1|layername=%2").arg(prepared.gpkgPath, prepared.tableName);
  auto* vl = new QgsVectorLayer(uri, QString::fromUtf8(kLayerTitle), QStringLiteral("ogr"));
  if (!vl->isValid()) {
    if (errorOut) *errorOut = vl->error().message();
    delete vl;
    return nullptr;
  }
  if (QgsVectorLayer* old = findBoreholeLayer(project)) project->removeMapLayer(old->id());
  vl->setCustomProperty(QString::fromLatin1(kCustomTag), true);
  LayerOps::markReferenceLayer(vl);
  LayerOps::applyLegendCrsLabel(vl);
  applyBoreholeStyle(vl);
  if (!project->addMapLayer(vl, true)) {
    delete vl;
    return nullptr;
  }
  prepared.retainFiles();
  LayerOps::placeInLegendGroup(project, vl, QStringLiteral("참조 지도"));
  if (canvas) LayerOps::refreshCanvasIfIdle(canvas);
  return vl;
}
