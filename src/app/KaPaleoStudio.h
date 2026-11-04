#pragma once

#include "core/HgisPaleoEngine.h"

#include <QImage>
#include <QPointer>
#include <QWidget>

class KaLayerInformationModel;
class KaLayerInformationView;
class QLabel;
class QPlainTextEdit;
class QgsMapCanvas;
class QgsProject;

// 2D 가상 지층 단면도(Cross-Section) 및 고지형 컬러맵 프리뷰 위젯
class KaPaleoCrossSectionWidget : public QWidget {
  Q_OBJECT
public:
  explicit KaPaleoCrossSectionWidget(QWidget* parent = nullptr);
  void setResult(const hgis::paleo::PaleoAnalysisResult& res);

protected:
  void paintEvent(QPaintEvent* event) override;

private:
  hgis::paleo::PaleoAnalysisResult m_result;
  bool m_hasResult = false;
};

// 「고지형분석」 전용 탭창:
//  - 좌측: 상시 유지되는 레이어창 (KaLayerInformationView, Delete 키 즉시 삭제 지원)
//  - 중앙: 상단 고지형 컬러맵·마커 인터랙티브 뷰 + 하단 2D 가상 지층 단면도 뷰
//  - 우측: 반경 10km 원클릭 분석 실행, 7대 고지형 범례, 종합 판독 요약창 및 조판(도면) 근거 연동
class KaPaleoStudio : public QWidget {
  Q_OBJECT
public:
  KaPaleoStudio(QgsProject* project, QgsMapCanvas* mainCanvas, QWidget* parent = nullptr);

  void setSurveyPath(const QString& surveyPath) { m_surveyPath = surveyPath; }
  void runAnalysis();
  void handleDeleteKey();
  KaLayerInformationView* layerTree() const { return m_layerTree; }
  const hgis::paleo::PaleoAnalysisResult& lastResult() const { return m_lastResult; }

signals:
  void requestApplyToDrawingSheet();

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  void buildUi();
  void renderMapPreview();

  QPointer<QgsProject> m_project;
  QPointer<QgsMapCanvas> m_mainCanvas;
  QString m_surveyPath;
  KaLayerInformationModel* m_layerModel = nullptr;
  KaLayerInformationView* m_layerTree = nullptr;
  QLabel* m_mapPreview = nullptr;
  KaPaleoCrossSectionWidget* m_crossSection = nullptr;
  QPlainTextEdit* m_summaryBox = nullptr;
  QLabel* m_statusLabel = nullptr;
  hgis::paleo::PaleoAnalysisResult m_lastResult;
};
