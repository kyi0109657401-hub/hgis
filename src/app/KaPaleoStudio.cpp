#include "KaPaleoStudio.h"
#include "KaDrawingStudioPlate.h"
#include "KaLayerInformation.h"
#include "KaMapPagePlate.h"
#include "MainWindow.h"
#include "core/GeoInfoBoreholeService.h"
#include "core/GeologyMapService.h"
#include "core/LayerOps.h"
#include "core/PaleoAnalysisRunner.h"
#include "core/PaleoLandformService.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>

#include <qgslayertree.h>
#include <qgsmapcanvas.h>
#include <qgsproject.h>

KaPaleoStudio::KaPaleoStudio(QgsProject* project, QgsMapCanvas* mainCanvas, QWidget* parent)
    : QWidget(parent), m_project(project), m_mainCanvas(mainCanvas) {
  setObjectName(QStringLiteral("paleoStudio"));
  buildUi();
}

void KaPaleoStudio::buildUi() {
  auto* rootLay = new QHBoxLayout(this);
  rootLay->setContentsMargins(0, 0, 0, 0);
  rootLay->setSpacing(0);

  auto* mainSplit = new QSplitter(Qt::Horizontal, this);
  mainSplit->setObjectName(QStringLiteral("paleoMainSplit"));
  KaMapPagePlate::followHandle(mainSplit, 8);
  mainSplit->setChildrenCollapsible(false);

  auto* layerBox = new QFrame(mainSplit);
  layerBox->setObjectName(QStringLiteral("layersCard"));
  layerBox->setMinimumWidth(260);
  layerBox->setMaximumWidth(400);
  auto* layerLay = new QVBoxLayout(layerBox);
  layerLay->setContentsMargins(8, 8, 8, 8);
  layerLay->setSpacing(6);

  auto* capRow = new QHBoxLayout();
  auto* leftCap = new QLabel(QStringLiteral("레이어"), layerBox);
  leftCap->setObjectName(QStringLiteral("cardCaption"));
  capRow->addWidget(leftCap);
  capRow->addStretch(1);
  auto* delBtn = new QToolButton(layerBox);
  delBtn->setText(QStringLiteral("선택 삭제(Del)"));
  connect(delBtn, &QToolButton::clicked, this, &KaPaleoStudio::handleDeleteKey);
  capRow->addWidget(KaDrawingStudioPlate::quiet(delBtn));
  layerLay->addLayout(capRow);

  if (m_project) {
    m_layerModel = new KaLayerInformationModel(m_project, true, this);
    m_layerModel->setFlag(QgsLayerTreeModel::AllowNodeChangeVisibility, true);
    m_layerModel->setFlag(QgsLayerTreeModel::AllowNodeReorder, true);
    m_layerModel->setFlag(QgsLayerTreeModel::AllowNodeRename, false);
    m_layerTree = new KaLayerInformationView(layerBox);
    m_layerTree->setObjectName(QStringLiteral("paleoLayerTree"));
    m_layerTree->setModel(m_layerModel);
    KaLayerInformationModel::configureView(m_layerTree);
    m_layerTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_layerTree->setDragDropMode(QAbstractItemView::InternalMove);
    m_layerTree->installEventFilter(this);
    if (m_layerTree->viewport()) m_layerTree->viewport()->installEventFilter(this);
    m_layerTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_layerTree, &QWidget::customContextMenuRequested, this, [this](const QPoint& pt) {
      if (auto* mw = qobject_cast<MainWindow*>(window())) {
        mw->showLayerTreeContextMenu(m_layerTree, pt);
      } else {
        QMenu menu(this);
        menu.addAction(QStringLiteral("레이어 삭제 (Delete)"), this,
                       &KaPaleoStudio::handleDeleteKey);
        menu.exec(m_layerTree->viewport()->mapToGlobal(pt));
      }
    });
    layerLay->addWidget(m_layerTree, 1);
    layerLay->addWidget(new KaLayerInformationPanel(m_layerModel, m_layerTree, layerBox));
    connect(m_project, &QgsProject::layersAdded, this, &KaPaleoStudio::refreshSourceStatus);
    connect(m_project, &QgsProject::layersRemoved, this, &KaPaleoStudio::refreshSourceStatus);
  }
  mainSplit->addWidget(layerBox);

  auto* centerSplit = new QSplitter(Qt::Vertical, mainSplit);
  centerSplit->setChildrenCollapsible(false);
  auto* mapFrame = new QFrame(centerSplit);
  mapFrame->setObjectName(QStringLiteral("mapCard"));
  auto* mapLay = new QVBoxLayout(mapFrame);
  mapLay->setContentsMargins(8, 8, 8, 8);
  m_mapPreview = new QLabel(
      QStringLiteral("우측 「준비된 자료로 고지형 분석 실행」을 누르면\n"
                     "국토지리원 DEM·음영기복 위에 7대 고지형 분류도와 시추공 마커가 표시됩니다."),
      mapFrame);
  m_mapPreview->setAlignment(Qt::AlignCenter);
  m_mapPreview->setWordWrap(true);
  m_mapPreview->setMinimumSize(420, 320);
  mapLay->addWidget(m_mapPreview, 1);
  centerSplit->addWidget(mapFrame);

  m_crossSection = new KaPaleoCrossSectionWidget(centerSplit);
  centerSplit->addWidget(m_crossSection);
  centerSplit->setStretchFactor(0, 3);
  centerSplit->setStretchFactor(1, 1);
  mainSplit->addWidget(centerSplit);

  auto* rightScroll = new QScrollArea(mainSplit);
  rightScroll->setWidgetResizable(true);
  rightScroll->setFrameShape(QFrame::NoFrame);
  rightScroll->setMinimumWidth(330);
  rightScroll->setMaximumWidth(440);
  auto* rightHost = new QFrame(rightScroll);
  rightHost->setObjectName(QStringLiteral("itemInspector"));
  auto* rightLay = new QVBoxLayout(rightHost);
  rightLay->setContentsMargins(12, 12, 12, 12);
  rightLay->setSpacing(8);

  auto* rCap = new QLabel(QStringLiteral("고지형 · 기본층서 종합 분석 (반경 10km)"), rightHost);
  rCap->setObjectName(QStringLiteral("cardCaption"));
  rightLay->addWidget(rCap);

  m_sourcesStatusLabel = new QLabel(rightHost);
  m_sourcesStatusLabel->setObjectName(QStringLiteral("paleoSourcesStatus"));
  m_sourcesStatusLabel->setWordWrap(true);
  rightLay->addWidget(m_sourcesStatusLabel);

  auto* dlRow = new QHBoxLayout();
  auto* btnDlSources = new QPushButton(QStringLiteral("시추·KIGAM 내려받기…"), rightHost);
  connect(btnDlSources, &QPushButton::clicked, this, &KaPaleoStudio::requestOpenSourceDialog);
  dlRow->addWidget(btnDlSources);
  auto* btnDlDem = new QPushButton(QStringLiteral("국토지리원 DEM 받기…"), rightHost);
  connect(btnDlDem, &QPushButton::clicked, this, &KaPaleoStudio::requestDemDownload);
  dlRow->addWidget(btnDlDem);
  rightLay->addLayout(dlRow);

  auto* btnRun =
      new QPushButton(QStringLiteral("준비된 자료로 고지형 분석 실행"), rightHost);
  btnRun->setObjectName(QStringLiteral("btnPrimary"));
  connect(btnRun, &QPushButton::clicked, this, &KaPaleoStudio::runAnalysis);
  rightLay->addWidget(btnRun);

  auto* btnSheet = new QPushButton(QStringLiteral("도면(조판)에 고지형 판독 근거 넣기"), rightHost);
  connect(btnSheet, &QPushButton::clicked, this, [this] {
    if (m_project) PaleoAnalysisRunner::applyEvidenceToSheet(m_project, m_lastResult);
    emit requestApplyToDrawingSheet();
  });
  rightLay->addWidget(btnSheet);

  m_statusLabel = new QLabel(
      QStringLiteral("6대 입력 자료 구비 현황을 확인한 뒤 분석을 실행하세요."),
      rightHost);
  m_statusLabel->setWordWrap(true);
  rightLay->addWidget(m_statusLabel);

  auto* legCap = new QLabel(QStringLiteral("7대 고지형 분류 체계 및 유적 잠재력"), rightHost);
  legCap->setObjectName(QStringLiteral("cardCaption"));
  rightLay->addWidget(legCap);
  for (size_t i = 1; i < hgis::paleo::kPaleoClassTable.size(); ++i) {
    const auto& meta = hgis::paleo::kPaleoClassTable[i];
    auto* row = new QLabel(
        QStringLiteral("<span style='color:%1;'>■</span> <b>%2</b> — %3")
            .arg(QString::fromLatin1(meta.color_hex), QString::fromUtf8(meta.name_kor),
                 QString::fromUtf8(meta.arch_potential)),
        rightHost);
    row->setWordWrap(true);
    rightLay->addWidget(row);
  }

  auto* sumCap = new QLabel(QStringLiteral("조사구역 고지형·기본층서 판독 요약 및 근거"), rightHost);
  sumCap->setObjectName(QStringLiteral("cardCaption"));
  rightLay->addWidget(sumCap);
  m_summaryBox = new QPlainTextEdit(rightHost);
  m_summaryBox->setReadOnly(true);
  m_summaryBox->setMinimumHeight(210);
  rightLay->addWidget(m_summaryBox, 1);

  auto* btnCopy = new QPushButton(QStringLiteral("판독 요약·근거 문장 복사"), rightHost);
  connect(btnCopy, &QPushButton::clicked, this, [this] {
    if (m_summaryBox && QApplication::clipboard())
      QApplication::clipboard()->setText(m_summaryBox->toPlainText());
    if (m_statusLabel)
      m_statusLabel->setText(QStringLiteral("고지형·기본층서 판독 요약문을 클립보드에 복사했습니다."));
  });
  rightLay->addWidget(btnCopy);

  rightScroll->setWidget(rightHost);
  mainSplit->addWidget(rightScroll);
  mainSplit->setSizes({300, 760, 380});
  rootLay->addWidget(mainSplit, 1);
}

bool KaPaleoStudio::eventFilter(QObject* watched, QEvent* event) {
  if (event->type() == QEvent::KeyPress) {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (ke->key() == Qt::Key_Delete) {
      handleDeleteKey();
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}

void KaPaleoStudio::handleDeleteKey() {
  if (!m_layerTree) return;
  if (auto* mw = qobject_cast<MainWindow*>(window())) {
    mw->removeLayersFromTree(m_layerTree);
  }
}

void KaPaleoStudio::refreshSourceStatus() {
  if (!m_sourcesStatusLabel || !m_project) return;
  const bool hasDem = PaleoAnalysisRunner::findPreferredDem(m_project) != nullptr;
  const bool hasSoil = PaleoLandformService::findSoilTerrainLayer(m_project) != nullptr;
  const bool hasGeol = GeologyMapService::existingGeologyLayer(m_project) != nullptr;
  bool hasRiver = false;
  for (QgsMapLayer* l : m_project->mapLayers())
    if (l && l->name().contains(QStringLiteral("수계"))) hasRiver = true;
  const bool hasKigam = !m_project->mapLayersByName(QStringLiteral("고지형 수문·연대(KIGAM)")).isEmpty();
  auto* bhLayer = GeoInfoBoreholeService::findBoreholeLayer(m_project);
  const long long bhCount = bhLayer ? bhLayer->featureCount() : 0;
  auto badge = [](bool ok, const QString& name) {
    return QStringLiteral("<span style='color:%1;'>%2 %3</span>")
        .arg(ok ? QStringLiteral("#15803d") : QStringLiteral("#b91c1c"),
             ok ? QStringLiteral("●") : QStringLiteral("○"), name);
  };
  m_sourcesStatusLabel->setText(
      QStringLiteral("<b>6대 입력 자료 현황</b>: %1 · %2 · %3<br>%4 · %5 · %6")
          .arg(badge(hasDem, QStringLiteral("국토지리원DEM")),
               badge(hasSoil, QStringLiteral("토양도")),
               badge(hasGeol, QStringLiteral("지질도")),
               badge(hasRiver, QStringLiteral("수계도")),
               badge(hasKigam, QStringLiteral("KIGAM수문")),
               badge(bhCount > 0, QStringLiteral("시추(%1공)").arg(bhCount))));
}

void KaPaleoStudio::runAnalysis() {
  if (!m_project) return;
  if (m_statusLabel)
    m_statusLabel->setText(QStringLiteral("고지형 4단계 파이프라인 연산 중…"));
  QApplication::processEvents();
  m_lastResult = PaleoAnalysisRunner::runFullPipeline(m_project, m_mainCanvas, m_surveyPath, nullptr);
  refreshSourceStatus();
  if (m_crossSection) m_crossSection->setResult(m_lastResult);
  if (m_summaryBox)
    m_summaryBox->setPlainText(QString::fromUtf8(m_lastResult.aoi_summary_text.c_str()));
  renderMapPreview();
  if (m_statusLabel)
    m_statusLabel->setText(
        QStringLiteral("고지형 분석 완료 — 파생 레이어가 좌측 레이어창에 등록되었습니다."));
}

void KaPaleoStudio::renderMapPreview() {
  const int w = m_lastResult.transform.width;
  const int h = m_lastResult.transform.height;
  if (!m_mapPreview || w <= 0 || h <= 0 ||
      m_lastResult.paleo_rgba_buffer.size() < static_cast<size_t>(w) * h * 4)
    return;
  QImage img(w, h, QImage::Format_ARGB32);
  for (int r = 0; r < h; ++r) {
    for (int c = 0; c < w; ++c) {
      const size_t i = static_cast<size_t>(r) * w + c;
      const double hs =
          (i < m_lastResult.multidirectional_hillshade.size())
              ? std::clamp(m_lastResult.multidirectional_hillshade[i] / 255.0, 0.35, 1.0)
              : 0.85;
      const int red = static_cast<int>(m_lastResult.paleo_rgba_buffer[i * 4 + 0] * hs);
      const int grn = static_cast<int>(m_lastResult.paleo_rgba_buffer[i * 4 + 1] * hs);
      const int blu = static_cast<int>(m_lastResult.paleo_rgba_buffer[i * 4 + 2] * hs);
      img.setPixelColor(c, r, QColor(red, grn, blu, 240));
    }
  }
  m_mapPreview->setPixmap(
      QPixmap::fromImage(img).scaled(m_mapPreview->size(), Qt::KeepAspectRatio,
                                     Qt::SmoothTransformation));
}
