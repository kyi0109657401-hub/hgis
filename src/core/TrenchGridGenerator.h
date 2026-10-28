#pragma once

#include <QByteArray>
#include <QString>
#include <QtGlobal>
#include <vector>

// Rotated excavation trench rectangles in work CRS (5186/5187). GPKG via OGR.
//
// 시굴조사 도메인: 트렌치는 조사구역 폴리곤 전체를 일정 간격으로 덮고,
// 총 굴착 면적은 목표 비율(시굴 10%, 표본 2%)이며 개별 폭은 최대 2 m,
// 길이는 최대 20 m다. 경계에서는 더 짧아질 수 있다.
namespace TrenchGridGenerator {

struct Spec {
  double originX = 0.0;
  double originY = 0.0;
  double trenchWidth = 2.0;
  double trenchLength = 20.0;
  double balkWidth = 1.0;
  int rows = 1;
  int cols = 1;
  double azimuthDeg = 0.0;  // clockwise from north
  QString namePrefix = QStringLiteral("Tr-");
  // ㄱ·ㄴ자 트렌치: outer length of each of the two arms (m); 0 = straight trench.
  // trenchLength is not used then; balkWidth is the gap between the L squares.
  double armLength = 0.0;
  // ㄱ·ㄴ자 꺾는 방향: 0 ㄴ, 1 ㄱ, 2 ㄴ 좌우 반대, 3 ㄱ 좌우 반대 (seen with the grid's north up).
  int armBend = 0;
};

struct Cell {
  QString name;
  double width = 2.0;
  // Straight: the long side. ㄱ·ㄴ자: the centreline (arm + arm - width), so width × length
  // is the trench area either way.
  double length = 20.0;
  double arm = 0.0;  // ㄱ·ㄴ자: outer length of each arm; 0 = straight trench
  // Closed ring, work CRS metres: 5 points (straight) or 7 (ㄱ·ㄴ자, ring[0] = outer corner,
  // ring[1] and ring[5] = the arm ends).
  std::vector<std::pair<double, double>> ring;
};

std::vector<Cell> build(const Spec& spec);

// One survey_area feature (work CRS WKB). fid is QGIS/OGR id; newer draws
// usually have a higher fid.
struct SurveyPoly {
  QByteArray wkb;
  qint64 fid = -1;
};

struct PickedArea {
  QByteArray wkb;
  double areaM2 = 0.0;
  int usedCount = 0;
  int totalCount = 0;
  bool usedSelection = false;
};

// Auto-fill must not union leftover survey polygons. Selected fids (if any
// match) are combined; otherwise only the highest fid (last drawn) is used.
//
// 조사구역을 여러 조각으로 그린 경우에는 사용자가 전체를 쓰겠다고 밝힐 수 있다.
// 그때만 useAll 로 전부 합친다. 기본값은 예전대로 「마지막 것만」이라, 지난
// 조사의 구역이 남아 있어도 격자가 수백 칸으로 불어나지 않는다.
PickedArea pickAutoFillArea(const std::vector<SurveyPoly>& features,
                            const std::vector<qint64>& selectedFids,
                            bool useAll = false);

// Fills the survey-area polygon (WKB) with trenches: a rotated regular grid
// anchored at the polygon envelope centre. Only complete rectangles inside
// the polygon are retained; boundary cells may be shortened.
// rows/cols/origin of the spec are ignored.
// An empty result sets reasonOut to a user-facing reason (no area, not a polygon,
// self-intersecting boundary, bad spec, too many cells, nothing fits).
std::vector<Cell> buildInArea(const Spec& spec, const QByteArray& areaWkb,
                              QString* reasonOut = nullptr);

// Why the survey-area polygon (WKB) cannot hold trenches; empty when it can.
QString areaProblem(const QByteArray& areaWkb);

// Every cell is a finite rectangle within the 2 m x 20 m limit matching its attributes, or a
// ㄱ·ㄴ자 whose arms are within it (TrenchLShape::validCell).
bool cellsValid(const std::vector<Cell>& cells, QString* errorOut = nullptr);

// 표고 표본 하나(작업 CRS 미터 + 표고 m).
struct ElevSample {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

// 시굴 트렌치는 등고선에 직교, 즉 사면(오르막) 방향으로 넣어야 층서가 잘린다.
// 등고선과 나란히 넣으면 같은 층만 따라가 층위를 못 읽는다.
// 표본에 최소제곱 평면을 맞춰 오르막 방위를 낸다.
struct SlopeAspect {
  double azimuthDeg = 0.0;  // 오르막 방위(북 기준 시계 방향) = 트렌치 장축 방위
  double slopePct = 0.0;    // 평균 경사(%)
  bool valid = false;       // 표본 부족·평지면 false
  int samples = 0;          // 표고 표본 수: 3 미만이면 지형 자료가 없는 것
};
// 경사가 minSlopePct보다 완만하면 방향에 의미가 없어 valid=false.
SlopeAspect upslopeAspect(const std::vector<ElevSample>& samples, double minSlopePct = 0.5);

// Width defaults to 2 m (maximum 2). Search lengths <= 20 m and balks, then
// shorten each cell about its centre to match targetPct (시굴 10, 표본 2).
// Cells stay inside the picked polygon and do not overlap.
struct RatioFill {
  std::vector<Cell> cells;
  double length = 0.0;  // longest actual cell, including ratio adjustment
  double balk = 10.0;
  double ratioPct = 0.0;
  double areaM2 = 0.0;
  double azimuthDeg = 0.0;
  double arm = 0.0;  // ㄱ·ㄴ자: longest actual arm; 0 for straight trenches
  QString error;  // nonempty when no valid plan can be made
};
// Straight trenches are all trenchLength (20, 15 or 10 m; 사용자 요청 2026-10-28): the count of whole
// trenches nearest the target, never shortened. armLength > 0 lays whole ㄱ·ㄴ자 trenches of that arm,
// bent by armBend, instead: the fewest reaching the target, so the total is just over it.
RatioFill buildForTargetRatio(const QByteArray& areaWkb, double targetPct, double width = 2.0,
                              double azimuthDeg = 0.0, double armLength = 0.0, int armBend = 0,
                              double trenchLength = 20.0);

// The cells cover targetPct of areaM2 as a plan of whole trenches must: within less than the smallest
// trench of the target, over or under. targetPct 0 = no target.
bool meetsTarget(const std::vector<Cell>& cells, double areaM2, double targetPct);

// Sum of trench areas (w × len per cell, square metres).
double totalArea(const std::vector<Cell>& cells);

// Validate cells, then replace the layer contents in one dataset transaction.
// Empty/invalid input and write failures preserve the existing layer contents.
bool writeGpkg(const QString& gpkgPath, const QString& layerName, const std::vector<Cell>& cells,
               const QString& authid, QString* errorOut);

// Deletes every feature of the layer (no-op when file/layer is missing). A file that
// exists but cannot be opened or written is a failure.
// "새로 만들기" must replace the previous grid instead of stacking on top of it.
bool clearLayer(const QString& gpkgPath, const QString& layerName, QString* errorOut);

}  // namespace TrenchGridGenerator
