#include "GeoInfoBoreholeService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <algorithm>
#include <cmath>

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
