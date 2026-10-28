#pragma once

#include <QByteArray>
#include <QDialog>

#include "core/TrenchGridGenerator.h"
#include "core/TrenchGridSnap.h"

class KaAzimuthDial;
class KaTargetMeter;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QThreadPool;
class QTimer;

// 시굴격자 속성 창(모덜리스). 규격·둑 간격·행/열·회전(방위)·이름 접두를 세밀하게
// 조정하고, 조사구역 자동 채움일 때 시굴 비율(기준: 시굴 10% · 표본 2%)을 미리 보여준다.
// 「구역에 깔기」는 기존 격자를 대체한다(Ctrl+Z 한 단계). 개별 트렌치 삭제는 이동 도구에서 우클릭.
// The preview search runs after input settles (debounced), off the UI thread, through
// TrenchPlanCache; the plan it shows is the exact plan 「구역에 깔기」 applies.
class KaTrenchDialog : public QDialog {
  Q_OBJECT
public:
  explicit KaTrenchDialog(QWidget* parent = nullptr);
  ~KaTrenchDialog() override;

  // 선택한(없으면 마지막) survey_area WKB와 면적(㎡). 비어 있으면 수동 배치만.
  void setArea(const QByteArray& wkb, double areaM2);
  TrenchGridGenerator::Spec spec() const;
  bool autoFill() const;
  // 격자를 깐 뒤에만 「개별 편집」「전체 이동」을 보인다.
  void setGridPlaced(bool placed);

  // 조사 종류. 시굴 10% · 표본 2%는 매장유산 조사의 기준 비율이다.
  enum class SurveyKind { Trial, Sample, Manual };
  SurveyKind surveyKind() const;
  double targetPct() const;  // Manual이면 0
  // Trial/Sample: the program fits lengths and balks to targetPct(). Manual: the user's spec.
  bool ratioMode() const { return surveyKind() != SurveyKind::Manual; }

  // 지형(DEM) 사면 방위. valid하면 방위 칸을 그 값으로 잠근다.
  void setTerrainAspect(const TrenchGridGenerator::SlopeAspect& aspect);
  bool useTerrainAzimuth() const;

  // 「좌표격자에 맞추기」 (사용자 요청 2026-10-20): the map's metre coordinate grid, or none.
  void setCoordinateGrid(bool available, const TrenchGridSnap::Grid& grid);
  // The grid the ㄱ·ㄴ자 10%/2% plan is laid on; null when the check is off or not usable.
  const TrenchGridSnap::Grid* gridLock() const;

  QByteArray areaWkb() const { return m_areaWkb; }
  double areaM2() const { return m_areaM2; }

  // A preview is waiting for input to settle or being computed in the background.
  bool planPending() const { return m_planPending; }

  // Asks before 「구역에 깔기」 replaces trenches moved, deleted or renamed by hand.
  // Returns true when the user chose to replace; the old grid stays one Ctrl+Z away.
  static bool confirmReplaceAdjusted(QWidget* parent, qint64 trenchCount);

signals:
  void applyRequested();        // 현재 설정으로 재배치(기존 격자 대체)
  void manualPlaceRequested();  // 맵에서 원점 클릭 배치
  void editSingleRequested();   // 개별 편집(그래픽처럼 선택·이동·삭제)
  void moveRequested();         // 전체 이동 도구 켜기

private:
  void schedulePlan();  // update visible rows now, compute the preview once input settles
  void refreshRows();
  void addShapeRows(class QFormLayout* form);
  void refreshPlan();
  void showRatioPlan(const TrenchGridGenerator::RatioFill& plan, double azimuth);
  void showFillPlan(const std::vector<TrenchGridGenerator::Cell>& cells, const QString& reason);
  double effectiveAzimuth() const;
  void setSummaryTone(const char* tone);  // ok · warn · plain → QSS 결과 칸 색

  QCheckBox* m_auto = nullptr;
  QComboBox* m_kind = nullptr;
  QCheckBox* m_terrain = nullptr;
  QLabel* m_terrainInfo = nullptr;
  TrenchGridGenerator::SlopeAspect m_aspect;
  QComboBox* m_shape = nullptr;  // 곧은 트렌치 (data 0) or a ㄱ·ㄴ자 bend (data 1 + Spec::armBend)
  QCheckBox* m_gridSnap = nullptr;
  bool m_gridAvailable = false;
  TrenchGridSnap::Grid m_grid;
  // 트렌치 길이 체크란 20 / 15 / 10 m (사용자 요청 2026-10-28): the id is the length in metres.
  class QButtonGroup* m_length = nullptr;
  QWidget* m_lengthRow = nullptr;
  QDoubleSpinBox* m_balk = nullptr;
  QSpinBox* m_rows = nullptr;
  QSpinBox* m_cols = nullptr;
  QDoubleSpinBox* m_az = nullptr;
  QLineEdit* m_prefix = nullptr;
  QLabel* m_ratio = nullptr;
  // 새 모양 (2차 보드 판 5): the 시굴 비율 bar under the summary and the dial beside 회전(방위); null in stock.
  KaTargetMeter* m_meter = nullptr;
  KaAzimuthDial* m_dial = nullptr;
  QLabel* m_dialNote = nullptr;
  void showMeter(double percent, double target);  // a negative percent hides the bar
  QWidget* m_afterPlace = nullptr;
  QByteArray m_areaWkb;
  double m_areaM2 = 0.0;
  QTimer* m_planTimer = nullptr;
  QThreadPool* m_planPool = nullptr;
  quint64 m_planSeq = 0;  // results of older requests are dropped
  bool m_planPending = false;
};
