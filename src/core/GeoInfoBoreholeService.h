#pragma once

#include "PreparedReferenceMap.h"

#include <QByteArray>
#include <QList>
#include <QString>

class QgsCoordinateTransformContext;
class QgsFeedback;
class QgsMapCanvas;
class QgsProject;
class QgsRectangle;
class QgsVectorLayer;

// 국토지반정보 포털시스템(https://www.geoinfo.or.kr) 시추공·주상도 수집 및
// 고환경·고지형 층서 분석 서비스.
//  - 시추공 공간조회: /proxyo2map.jsp (TBL_HEADER_HOLE) 및 /map/searchHoleByExtent.do
//  - 시추주상도 조회: /map/getSichudanData.do?code={HOLE_CODE} (jsonView)
//  - 층서 분석: 매립토 두께, 충적·퇴적층 구간·두께·토질(USCS 사질/점토·실트질/자갈질),
//    생토·기반암(풍화토·풍화암·연암·경암) 출현 심도, 지하수위(GL.-m), 고지형 판독 힌트 산출.
class GeoInfoBoreholeService {
public:
  struct StratumLayer {
    double fromM = 0.0;
    double toM = 0.0;
    QString name;
    QString uscs;
    QString color;
    QString desc;
  };

  struct BoreholeRecord {
    QString holeCode;
    QString projectCode;
    QString projectName;
    QString address;
    double tmX = 0.0;       // EPSG:5186 Easting (X)
    double tmY = 0.0;       // EPSG:5186 Northing (Y)
    double altitudeM = 0.0; // 표고 (EL.m)
    double totalDepthM = 0.0;
    double waterLevelM = 0.0; // 지하수위 (GL.-m)
    bool hasWaterLevel = false;

    double fillDepthM = 0.0;         // 매립·성토층 하한 심도(m)
    double alluvialFromM = 0.0;      // 충적·퇴적·붕적층 시작 심도(m)
    double alluvialToM = 0.0;        // 충적·퇴적·붕적층 종료 심도(m)
    double alluvialThicknessM = 0.0; // 충적·퇴적·붕적층 총 두께(m)
    double weatheredDepthM = -1.0;   // 풍화토·풍화암·기반암(생토) 출현 심도(m), 미출현 시 -1
    QString dominantUscs;            // 대표 USCS (SM, CL, GP 등)
    QString depositEnv;              // 사질 충적(자연제방·미고지) / 점토·실트 충적(구하도·저습지) 등
    QString paleoHint;               // 고지형·문화층 부존 심도 판독 요약
    QString layerSummary;            // 전체 층서 요약 문자열
    QList<StratumLayer> layers;
  };

  // getSichudanData.do 응답(외부 래퍼 {"jsonView":"..."} 또는 내부 JSON 직접)을 파싱해
  // 고지형·기본층서 지표를 계산한다.
  static bool parseSichudanJson(const QByteArray& rawBytes, BoreholeRecord* out,
                                QString* errorOut = nullptr);

  // Worker-only preparation; no project, canvas or layer objects escape.
  static PreparedReferenceMap prepare(const QgsRectangle& extent5186,
                                      const QString& requestedBasePath,
                                      const QgsCoordinateTransformContext& transformContext,
                                      QgsFeedback* feedback = nullptr);

  // GUI-only registration; adds the prepared GPKG into 「참조 지도」.
  static QgsVectorLayer* addPrepared(QgsProject* project, QgsMapCanvas* canvas,
                                     const PreparedReferenceMap& prepared,
                                     QString* errorOut = nullptr);

  static bool applyBoreholeStyle(QgsVectorLayer* layer);
  static QgsVectorLayer* findBoreholeLayer(QgsProject* project);
  static double maxSpanMeters() { return 20000.0; }
};
