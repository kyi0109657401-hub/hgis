#pragma once

#include "GeoInfoBoreholeService.h"

#include <QList>
#include <QString>

class QgsGeometry;
class QgsProject;
class QgsRasterLayer;
class QgsVectorLayer;

// 5대 고지형·지질고고학 문헌(국립문화유산연구원 현장 안내서; 장우영 외 2010;
// 이희진·박동주 2023; 이동영 / 이민부·이광률 2016; 박지훈 2015)의 지형 인자:
//  - 공개 DEM 표고(m), 경사도(°), 하상·주변 저지대 대비 상대고도(하상비고 REM, m)
//  - 용수하천 최단거리(m)
//  - 국토지반정보 시추주상도(충적층 두께·토질 USCS·생토 심도·지하수위)
//  - KIGAM 지질·단층·지하수위·단구 OSL 연대
// 를 결합해 고지형 판독 레이어(paleo_landform)의 분류와 근거(note)를 보강한다.
class PaleoDemClassifier {
public:
  struct Metrics {
    bool hasDem = false;
    double minElevM = 0.0;
    double maxElevM = 0.0;
    double meanElevM = 0.0;
    double localReliefM = 0.0; // 하상·주변 최소 표고 대비 비고(m)
    double meanSlopeDeg = 0.0;
    double riverDistM = -1.0;  // 수계도 하천까지 최단거리(m), 없으면 -1

    bool hasBorehole = false;
    QString boreholeCode;
    double alluvialThicknessM = 0.0;
    double weatheredDepthM = -1.0;
    double waterLevelM = 0.0;
    QString dominantUscs;
    QString depositEnv;
  };

  struct Classification {
    QString kind; // 자연제방, 구하도, 하안단구, 선상지, 해성평탄, 미고지, 미저지
    QString note; // 자동: ... (표고·비고·경사·하천거리·시추 층서 근거)
  };

  static Classification classify(const QString& baseKind, const Metrics& m);

  static Metrics samplePolygonMetrics(const QgsGeometry& geomWorkCrs, QgsProject* project,
                                      QgsRasterLayer* demLayer, double baseMinElevM);

  // 고지형 판독 레이어(paleo_landform)의 자동 가설 도형들에 공개 DEM·수계·시추주상도
  // 지표를 결합해 분류(kind)와 판독 근거(note)를 갱신한다. 갱신된 피처 수를 반환한다.
  static int enrichInterpretationLayer(QgsProject* project, QgsVectorLayer* paleoLayer);
};
