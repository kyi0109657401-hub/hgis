#pragma once

#include "HgisPaleoEngine.h"

#include <QString>
#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgsrectangle.h>

class QgsCoordinateReferenceSystem;
class QgsFeedback;
class QgsMapCanvas;
class QgsProject;
class QgsRasterLayer;
class QgsVectorLayer;

class PaleoAnalysisRunner {
public:
  static constexpr double kRadiusMeters = 10000.0; // 조사구역 반경 10km (10,000m)

  // 조사구역(survey_area) 폴리곤들을 작업 좌표계로 모아 합치고, 외곽선 기준 반경 radiusM 상자를 반환한다.
  static QgsRectangle surveyExtentRadius(QgsProject* project, QgsMapCanvas* canvas,
                                         double radiusM = kRadiusMeters,
                                         QgsGeometry* outAoiWorkCrs = nullptr,
                                         QgsPointXY* outCenterWorkCrs = nullptr);

  // [기능 1] 병합된 국토지리원 DEM에서
  //  1) [DEM] 국토지리원_DEM.tif
  //  2) [DEM] 광역지형도(채색+음영기복) (고도별 색상표 + 4방향 다중광원 음영기복 + 경사도 블렌딩)
  //  3) [DEM] 경사도
  // 3종 GeoTIFF를 생성하고 레이어창에 개별 항목으로 즉시 등록한다.
  static bool registerDemWideLayers(QgsProject* project, QgsMapCanvas* canvas,
                                    const QString& sourceDemPath, const QString& surveyPath,
                                    QString* errorOut = nullptr);

  // 국토지리정보원 자동 다운로드 창에서 받은 DEM 압축(.zip) 또는 래스터(.img/.tif/.dem/.txt/.asc) 파일을
  // 조사 폴더의 DEM 폴더에 풀고 국토지리원 DEM·광역지형도·경사도로 즉시 적재한다.
  static bool importNgiiDemDownload(QgsProject* project, QgsMapCanvas* canvas,
                                    const QString& downloadedPath, const QString& surveyPath,
                                    QString* errorOut = nullptr);

  // 국토지리정보원 수치지형도(3D 등고선 F001*·표고점 F002*)로부터 나무·건물이 제거된
  // 순수 지형(Bare-earth) 정밀 국토지리원 DEM을 보간·합성하고 광역지형도·경사도와 함께 등록한다.
  static QString buildNgiiDemFromTopographic(QgsProject* project, QgsMapCanvas* canvas,
                                             const QString& surveyPath,
                                             QString* errorOut = nullptr);

  // 프로젝트에 등록된 DEM 중 국토지리원 정밀 DEM([DEM] 국토지리원_DEM.tif)을 최우선으로 찾고,
  // 없을 때만 일반 DEM(Copernicus DSM 등)으로 폴백한다.
  static QgsRasterLayer* findPreferredDem(const QgsProject* project);

  // [기능 2] 조사구역 반경 10km 기준 5대 원본 자료 확인·등록 및 HgisPaleoEngine 4단계 파이프라인 실행,
  // 파생 레이어([고지형] 7대 고지형 분류도, [고지형] 하상비고(REM), [고지형] 시추주상도 마커) 자동 등록.
  static hgis::paleo::PaleoAnalysisResult runFullPipeline(QgsProject* project,
                                                          QgsMapCanvas* canvas,
                                                          const QString& surveyPath,
                                                          QgsFeedback* feedback = nullptr);

  // 조판(도면 탭 user_sheet)에 고지형 10km 분석 근거와 기본층서 해설 상자를 추가·갱신한다.
  static bool applyEvidenceToSheet(QgsProject* project,
                                   const hgis::paleo::PaleoAnalysisResult& res);

  // RGBA8888 또는 Float32 격자를 GeoTIFF로 저장한다.
  static bool writeRgbaGeoTiff(const QString& outPath, const hgis::paleo::GridTransform& tf,
                               const QString& crsWkt, const std::vector<uint8_t>& rgba,
                               QString* errorOut = nullptr);
  static bool writeFloatGeoTiff(const QString& outPath, const hgis::paleo::GridTransform& tf,
                                const QString& crsWkt, const std::vector<double>& values,
                                QString* errorOut = nullptr);
  static void registerBoreholeMarkers(
      QgsProject* project, const QString& gpkgPath,
      const std::vector<hgis::paleo::ParsedBoreholeSummary>& boreholes);

  static void ensureReferenceLayers(QgsProject* project, QgsMapCanvas* canvas,
                                    const QgsRectangle& ext5186, const QgsRectangle& ext10k5186,
                                    const QString& paleoDir, QgsFeedback* feedback);
  static std::vector<hgis::paleo::BoreholeRawInput> collectBoreholes(
      QgsProject* project, const QgsCoordinateReferenceSystem& workCrs,
      const QgsPointXY& centerPt);
};
