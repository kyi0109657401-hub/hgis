#pragma once

#include "PreparedReferenceMap.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <qgsgeometry.h>

class QgsCoordinateTransformContext;
class QgsFeedback;
class QgsMapCanvas;
class QgsProject;
class QgsRectangle;
class QgsVectorLayer;

// KIGAM 지오빅데이터 오픈플랫폼(https://data.kigam.re.kr)의 고지형·지질환경 WFS 및
// 1:5만 도폭 원도·설명서 PDF 연동 서비스.
//  - 단층·구조선: geoOpen:l_50k_geology_fault_latest (1:5만 단층·추정단층)
//  - 지하수위·수리지질: GeoEnv:underwater_2019_avg, GeoEnv:Hydrogeology
//  - 단구 OSL/IRSL 연대·융기율: GeoEnv:nation_upliftrate_2023
class KigamPaleoService {
public:
  struct FaultItem {
    QgsGeometry geom4326;
    QString faultType;
    QString sheetName;
    QString sheetIndex;
    double dipDeg = 0.0;
  };

  struct PointItem {
    double lon = 0.0;
    double lat = 0.0;
    double tmX5186 = 0.0;
    double tmY5186 = 0.0;
    bool hasTm5186 = false;
    QString kind;        // 지하수위 / 수리지질 / 단구연대(OSL)
    QString name;        // 공번 또는 시료명
    double altitudeM = 0.0;
    double waterGlM = 0.0;
    QString ageKa;
    QString terraceElM;
    QString summary;
  };

  static QList<FaultItem> parseFaultGeoJson(const QByteArray& rawJson);
  static QList<PointItem> parseUnderwaterGeoJson(const QByteArray& rawJson);
  static QList<PointItem> parseUpliftGeoJson(const QByteArray& rawJson);

  static QString reportPdfUrl(const QString& sheetIndex);
  static QString originalMapPdfUrl(const QString& sheetIndex);

  static PreparedReferenceMap prepare(const QgsRectangle& extent5186,
                                      const QString& requestedBasePath,
                                      const QgsCoordinateTransformContext& transformContext,
                                      QgsFeedback* feedback = nullptr);

  static QgsVectorLayer* addPrepared(QgsProject* project, QgsMapCanvas* canvas,
                                     const PreparedReferenceMap& prepared,
                                     QString* errorOut = nullptr);

  static bool applyKigamPaleoStyle(QgsVectorLayer* pointLayer, QgsVectorLayer* faultLayer = nullptr);
  static double maxSpanMeters() { return 80000.0; }
};
