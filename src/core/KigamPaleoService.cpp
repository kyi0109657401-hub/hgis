#include "KigamPaleoService.h"
#include "KaLogExcept.h"
#include "ReferenceStorage.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>
#include <cmath>

#include <qgsblockingnetworkrequest.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransform.h>
#include <qgscoordinatetransformcontext.h>
#include <qgsexception.h>
#include <qgsfeature.h>
#include <qgsfeedback.h>
#include <qgsfield.h>
#include <qgsfields.h>
#include <qgsjsonutils.h>
#include <qgspointxy.h>
#include <qgsrectangle.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayer.h>

namespace {

constexpr const char* kPointTitle = "고지형 수문·연대(KIGAM)";
constexpr const char* kFaultTitle = "단층·선구조(KIGAM 1:5만)";

double parseDmsText(const QString& dms) {
  static const QRegularExpression re(
      QStringLiteral("([0-9]+)\\D+([0-9]+)\\D+([0-9]+(?:\\.[0-9]+)?)"));
  const QRegularExpressionMatch m = re.match(dms);
  if (!m.hasMatch()) return dms.toDouble();
  return m.captured(1).toDouble() + m.captured(2).toDouble() / 60.0 +
         m.captured(3).toDouble() / 3600.0;
}

QByteArray fetchWfsJson(const QString& baseUrl, const QString& typeName, const QgsRectangle& b4326,
                        int maxCount, QgsFeedback* feedback) {
  QUrl url(baseUrl);
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("service"), QStringLiteral("WFS"));
  q.addQueryItem(QStringLiteral("version"), QStringLiteral("2.0.0"));
  q.addQueryItem(QStringLiteral("request"), QStringLiteral("GetFeature"));
  q.addQueryItem(QStringLiteral("typeNames"), typeName);
  q.addQueryItem(QStringLiteral("outputFormat"), QStringLiteral("application/json"));
  q.addQueryItem(QStringLiteral("count"), QString::number(maxCount));
  if (!b4326.isEmpty()) {
    q.addQueryItem(QStringLiteral("bbox"),
                   QStringLiteral("%1,%2,%3,%4,urn:ogc:def:crs:EPSG::4326")
                       .arg(b4326.yMinimum(), 0, 'f', 6)
                       .arg(b4326.xMinimum(), 0, 'f', 6)
                       .arg(b4326.yMaximum(), 0, 'f', 6)
                       .arg(b4326.xMaximum(), 0, 'f', 6));
  }
  url.setQuery(q);
  QNetworkRequest req(url);
  req.setTransferTimeout(15000);
  QgsBlockingNetworkRequest net;
  if (net.get(req, false, feedback) != QgsBlockingNetworkRequest::NoError) return {};
  return net.reply().content();
}

}  // namespace

QList<KigamPaleoService::FaultItem> KigamPaleoService::parseFaultGeoJson(const QByteArray& rawJson) {
  QList<FaultItem> out;
  const QgsFields emptyFields;
  const QgsFeatureList parsed =
      QgsJsonUtils::stringToFeatureList(QString::fromUtf8(rawJson), emptyFields, nullptr);
  const QJsonArray arr =
      QJsonDocument::fromJson(rawJson).object().value(QStringLiteral("features")).toArray();
  const int n = std::min<int>(parsed.size(), arr.size());
  for (int i = 0; i < n; ++i) {
    if (!parsed[i].hasGeometry() || parsed[i].geometry().isEmpty()) continue;
    const QJsonObject p = arr[i].toObject().value(QStringLiteral("properties")).toObject();
    FaultItem item;
    item.geom4326 = parsed[i].geometry();
    item.faultType = p.value(QStringLiteral("유형")).toString(QStringLiteral("단층")).trimmed();
    item.sheetName = p.value(QStringLiteral("도폭")).toString().trimmed();
    item.sheetIndex = p.value(QStringLiteral("도폭인덱스")).toString().trimmed();
    item.dipDeg = p.value(QStringLiteral("경사각")).toDouble();
    out.append(item);
  }
  return out;
}

QList<KigamPaleoService::PointItem> KigamPaleoService::parseUnderwaterGeoJson(
    const QByteArray& rawJson) {
  QList<PointItem> out;
  const QgsCoordinateReferenceSystem crs5174(QStringLiteral("EPSG:5174"));
  const QgsCoordinateReferenceSystem crs5186(QStringLiteral("EPSG:5186"));
  const QgsCoordinateTransform tr5174To5186(crs5174, crs5186, QgsCoordinateTransformContext());
  const QJsonArray arr =
      QJsonDocument::fromJson(rawJson).object().value(QStringLiteral("features")).toArray();
  for (const QJsonValue& v : arr) {
    const QJsonObject f = v.toObject();
    const QJsonObject p = f.value(QStringLiteral("properties")).toObject();
    const QJsonArray coords =
        f.value(QStringLiteral("geometry")).toObject().value(QStringLiteral("coordinates")).toArray();
    PointItem item;
    if (coords.size() >= 2) {
      const double c0 = coords[0].toDouble();
      const double c1 = coords[1].toDouble();
      if (c0 > 200.0 || c1 > 200.0) {
        QgsPointXY pt(c0, c1);
        if (tr5174To5186.isValid()) {
          try {
            pt = tr5174To5186.transform(pt);
          } catch (...) {
            KA_LOG_EXCEPT();
            pt = QgsPointXY(c0, c1 + 100000.0);
          }
        } else {
          pt = QgsPointXY(c0, c1 + 100000.0);
        }
        item.tmX5186 = pt.x();
        item.tmY5186 = pt.y();
        item.hasTm5186 = true;
      } else {
        item.lon = c0;
        item.lat = c1;
      }
    }
    item.kind = QStringLiteral("지하수위");
    item.name = p.value(QStringLiteral("시험정공번")).toString().trimmed();
    item.altitudeM = p.value(QStringLiteral("표고(m)")).toVariant().toDouble();
    item.waterGlM = p.value(QStringLiteral("평균수위(GL.-m)")).toVariant().toDouble();
    const QString aquifer = p.value(QStringLiteral("대수층유형")).toString().trimmed();
    item.summary = QStringLiteral("표고 %1m · 평균지하수위 GL-%2m (%3)")
                       .arg(item.altitudeM, 0, 'f', 1)
                       .arg(item.waterGlM, 0, 'f', 1)
                       .arg(aquifer.isEmpty() ? QStringLiteral("관측정") : aquifer);
    if (item.hasTm5186 || (item.lon != 0.0 && item.lat != 0.0)) out.append(item);
  }
  return out;
}

QList<KigamPaleoService::PointItem> KigamPaleoService::parseUpliftGeoJson(
    const QByteArray& rawJson) {
  QList<PointItem> out;
  const QJsonArray arr =
      QJsonDocument::fromJson(rawJson).object().value(QStringLiteral("features")).toArray();
  for (const QJsonValue& v : arr) {
    const QJsonObject f = v.toObject();
    const QJsonObject p = f.value(QStringLiteral("properties")).toObject();
    PointItem item;
    item.lat = parseDmsText(p.value(QStringLiteral("위도")).toString());
    item.lon = parseDmsText(p.value(QStringLiteral("경도")).toString());
    item.kind = QStringLiteral("단구연대(OSL)");
    item.name = p.value(QStringLiteral("노두명(시료명)")).toString().trimmed();
    item.altitudeM = p.value(QStringLiteral("시료해발고도(m)")).toVariant().toDouble();
    item.ageKa = p.value(QStringLiteral("연대(ka)")).toString().trimmed();
    item.terraceElM = p.value(QStringLiteral("단구면해발고도(m)")).toString().trimmed();
    const QString method = p.value(QStringLiteral("측정방법")).toString().trimmed();
    item.summary = QStringLiteral("%1 %2ka · 단구면 %3m · 시료표고 %4m")
                       .arg(method, item.ageKa, item.terraceElM)
                       .arg(item.altitudeM, 0, 'f', 1);
    if (item.lon > 120.0 && item.lat > 30.0) out.append(item);
  }
  return out;
}

PreparedReferenceMap KigamPaleoService::prepare(
    const QgsRectangle& extent5186, const QString& requestedBasePath,
    const QgsCoordinateTransformContext& transformContext, QgsFeedback* feedback) {
  PreparedReferenceMap result;
  if (!ReferenceMapPreparation::initializeStorage(result, requestedBasePath)) return result;
  const QString outPath = result.gpkgPath;

  const QgsCoordinateReferenceSystem crs5186(QStringLiteral("EPSG:5186"));
  const QgsCoordinateReferenceSystem crs4326(QStringLiteral("EPSG:4326"));
  QgsCoordinateTransform to4326(crs5186, crs4326, transformContext);
  QgsCoordinateTransform to5186(crs4326, crs5186, transformContext);
  QgsRectangle b4326;
  try {
    b4326 = to4326.transformBoundingBox(extent5186);
  } catch (...) {
    KA_LOG_EXCEPT();
    result.error = QStringLiteral("화면 좌표 변환에 실패했습니다.");
    return result;
  }

  const QByteArray faultBytes = fetchWfsJson(
      QStringLiteral("https://data.kigam.re.kr/geoserver/ows"),
      QStringLiteral("geoOpen:l_50k_geology_fault_latest"), b4326, 200, feedback);
  const QByteArray uwBytes = fetchWfsJson(
      QStringLiteral("https://data.kigam.re.kr/gives/geoserver/ows"),
      QStringLiteral("GeoEnv:underwater_2019_avg"), QgsRectangle(), 500, feedback);
  const QByteArray upliftBytes = fetchWfsJson(
      QStringLiteral("https://data.kigam.re.kr/gives/geoserver/ows"),
      QStringLiteral("GeoEnv:nation_upliftrate_2023"), QgsRectangle(), 120, feedback);

  QList<PointItem> pts = parseUnderwaterGeoJson(uwBytes);
  pts.append(parseUpliftGeoJson(upliftBytes));
  const QList<FaultItem> faults = parseFaultGeoJson(faultBytes);

  QgsVectorLayer ptMem(QStringLiteral("Point?crs=EPSG:5186"), QString::fromUtf8(kPointTitle),
                       QStringLiteral("memory"));
  QgsFields ptFields;
  ptFields.append(QgsField(QStringLiteral("kind"), QMetaType::Type::QString));
  ptFields.append(QgsField(QStringLiteral("name"), QMetaType::Type::QString));
  ptFields.append(QgsField(QStringLiteral("altitude_m"), QMetaType::Type::Double));
  ptFields.append(QgsField(QStringLiteral("water_gl_m"), QMetaType::Type::Double));
  ptFields.append(QgsField(QStringLiteral("age_ka"), QMetaType::Type::QString));
  ptFields.append(QgsField(QStringLiteral("terrace_el_m"), QMetaType::Type::QString));
  ptFields.append(QgsField(QStringLiteral("summary"), QMetaType::Type::QString));
  ptMem.dataProvider()->addAttributes(ptFields.toList());
  ptMem.updateFields();

  QgsFeatureList ptFeats;
  QgsRectangle grown = extent5186;
  grown.grow(15000.0);
  for (const PointItem& p : pts) {
    try {
      const QgsPointXY pt5186 =
          p.hasTm5186 ? QgsPointXY(p.tmX5186, p.tmY5186)
                      : to5186.transform(QgsPointXY(p.lon, p.lat));
      if (!grown.contains(pt5186)) continue;
      QgsFeature f(ptMem.fields());
      f.setGeometry(QgsGeometry::fromPointXY(pt5186));
      f.setAttribute(QStringLiteral("kind"), p.kind);
      f.setAttribute(QStringLiteral("name"), p.name);
      f.setAttribute(QStringLiteral("altitude_m"), p.altitudeM);
      f.setAttribute(QStringLiteral("water_gl_m"), p.waterGlM);
      f.setAttribute(QStringLiteral("age_ka"), p.ageKa);
      f.setAttribute(QStringLiteral("terrace_el_m"), p.terraceElM);
      f.setAttribute(QStringLiteral("summary"), p.summary);
      ptFeats.append(f);
    } catch (...) {
      KA_LOG_EXCEPT();
    }
  }
  ptMem.dataProvider()->addFeatures(ptFeats);

  QgsVectorFileWriter::SaveVectorOptions opts;
  opts.driverName = QStringLiteral("GPKG");
  opts.layerName = QStringLiteral("kigam_paleo_points");
  opts.fileEncoding = QStringLiteral("UTF-8");
  QString errMsg, newFn, newLayer;
  if (QgsVectorFileWriter::writeAsVectorFormatV3(&ptMem, outPath, transformContext, opts, &errMsg,
                                                 &newFn, &newLayer) != QgsVectorFileWriter::NoError) {
    result.error = errMsg;
    return result;
  }

  if (!faults.isEmpty()) {
    QgsVectorLayer fltMem(QStringLiteral("MultiLineString?crs=EPSG:5186"),
                          QString::fromUtf8(kFaultTitle), QStringLiteral("memory"));
    QgsFields fltFields;
    fltFields.append(QgsField(QStringLiteral("fault_type"), QMetaType::Type::QString));
    fltFields.append(QgsField(QStringLiteral("sheet_name"), QMetaType::Type::QString));
    fltFields.append(QgsField(QStringLiteral("sheet_index"), QMetaType::Type::QString));
    fltFields.append(QgsField(QStringLiteral("dip_deg"), QMetaType::Type::Double));
    fltMem.dataProvider()->addAttributes(fltFields.toList());
    fltMem.updateFields();
    QgsFeatureList fltFeats;
    for (const FaultItem& fl : faults) {
      QgsGeometry g = fl.geom4326;
      try {
        if (g.transform(to5186) != Qgis::GeometryOperationResult::Success) continue;
      } catch (...) {
        KA_LOG_EXCEPT();
        continue;
      }
      g.convertToMultiType();
      QgsFeature f(fltMem.fields());
      f.setGeometry(g);
      f.setAttribute(QStringLiteral("fault_type"), fl.faultType);
      f.setAttribute(QStringLiteral("sheet_name"), fl.sheetName);
      f.setAttribute(QStringLiteral("sheet_index"), fl.sheetIndex);
      f.setAttribute(QStringLiteral("dip_deg"), fl.dipDeg);
      fltFeats.append(f);
    }
    fltMem.dataProvider()->addFeatures(fltFeats);
    opts.layerName = QStringLiteral("kigam_paleo_faults");
    opts.actionOnExistingFile = QgsVectorFileWriter::CreateOrOverwriteLayer;
    QgsVectorFileWriter::writeAsVectorFormatV3(&fltMem, outPath, transformContext, opts, &errMsg,
                                               &newFn, &newLayer);
  }
  if (ptFeats.isEmpty() && faults.isEmpty()) {
    result.error = QStringLiteral("현재 화면 범위 주변에 KIGAM 단층·지하수위·단구연대 자료가 없습니다.");
    return result;
  }
  result.tableName = QStringLiteral("kigam_paleo_points");
  result.status = PreparedReferenceMap::Status::Ready;
  return result;
}
