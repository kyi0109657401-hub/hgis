// [P6 wiring] Strata shell: attaches the P3-P5 widgets to the window and feeds them values.
// The widgets decide nothing. The inspector is the third pane of m_mainSplit and hosts the
// existing feature card; the badge sits in the tab corner; the guide band floats over the
// map; the basemap card asks the window; the home 「설정」 buttons open the owning dialogs.
// MainWindow.cpp itself only calls setupStrataShell() and syncBasemapCard().
#include "MainWindow.h"

#include "KaBasemapQuickCard.h"
#include "KaDrawGuideBand.h"
#include "KaDrawingStudio.h"
#include "KaFeatureCard.h"  // KaInspectorPanel.h's inline featureCard() needs the complete type
#include "KaIcons.h"
#include "KaInspectorPanel.h"
#include "KaInspectorStyle.h"
#include "KaLayerOpacityRail.h"
#include "KaMapPagePlate.h"
#include "KaPaleoMenu.h"
#include "KaShellFocus.h"
#include "KaStartPage.h"
#include "KaStatusBar.h"
#include "KaSurveyBadge.h"
#include "KaThemeOptions.h"
#include "core/EditBufferSummary.h"
#include "core/LayerOps.h"
#include "core/LayerStyleDefaults.h"
#include "core/RecentSurveys.h"
#include "core/SurveyFacts.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QSettings>
#include <QShortcut>
#include <QSplitter>
#include <QStyle>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#if KA_HGIS_HAS_QGIS
#include <qgslayertree.h>
#include <qgslayertreeview.h>
#include <qgsmapcanvas.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#endif

namespace {
constexpr int kShellSyncMs = 300;
#if KA_HGIS_HAS_QGIS
// The satellite basemap's legend node (live tiles named 「위성」), or null when none is loaded.
QgsLayerTreeLayer* satelliteNode(QgsProject* project) {
  QgsLayerTree* root = project ? project->layerTreeRoot() : nullptr;
  if (!root) return nullptr;
  for (QgsMapLayer* layer : project->mapLayers()) {
    if (!layer || !layer->isValid() || !LayerOps::isBasemapLayer(layer)) continue;
    if (!layer->name().contains(QStringLiteral("위성"))) continue;
    if (QgsLayerTreeLayer* node = root->findLayer(layer->id())) return node;
  }
  return nullptr;
}

// Sum of featureCount() over the domain layers with this key (no geometry is read).
int featureTotal(QgsProject* project, const QString& layerKey) {
  int total = 0;
  for (QgsVectorLayer* layer : LayerOps::findAllByLayerKey(project, layerKey))
    if (layer && layer->isValid()) total += int(qMax(0LL, layer->featureCount()));
  return total;
}
#endif

QString hintForTool(const QString& iconId) {
  if (iconId == QLatin1String("draw_poly") || iconId == QLatin1String("draw_line"))
    return QStringLiteral("클릭으로 점을 찍고 우클릭으로 마칩니다 (Esc 취소)");
  if (iconId == QLatin1String("gps") || iconId == QLatin1String("artifact"))
    return QStringLiteral("클릭한 자리에 점을 놓습니다");
  if (iconId == QLatin1String("measure")) return QStringLiteral("두 점을 클릭하면 거리가, 셋 이상이면 면적이 나옵니다");
  if (iconId == QLatin1String("select")) return QStringLiteral("도형을 클릭하면 수정점이 나옵니다. 점을 끌어 옮깁니다");
  if (iconId == QLatin1String("georef")) return QStringLiteral("사진과 지도에서 같은 자리를 짝지어 클릭합니다");
  if (iconId == QLatin1String("trench_grid")) return QStringLiteral("격자를 놓을 원점을 클릭합니다");
  if (iconId == QLatin1String("note")) return QStringLiteral("도형을 클릭하면 조사카드가 열립니다");
  return {};
}
}  // namespace

void MainWindow::setupStrataShell() {
#if KA_HGIS_HAS_QGIS
  if (m_inspector || !m_mainSplit || !m_canvas) return;
  // 도형을 선택했을 때 내부가 불투명 노란색으로 막혀 뒤 배경이 가려지지 않게 반투명 선택 색을 기본으로 둔다.
  m_canvas->setSelectionColor(QColor(255, 235, 59, 45));
  QgsProject::instance()->setSelectionColor(QColor(255, 235, 59, 45));
  // 1. Inspector: third pane; the feature card is built into its 속성 tab (host API of P4).
  m_inspector = new KaInspectorPanel(m_mainSplit);
  m_mainSplit->addWidget(m_inspector);
  m_mainSplit->setStretchFactor(2, 0);
  m_mainSplit->setSizes({348, 932 - KaShellFocus::kDefaultRightWidth, KaShellFocus::kDefaultRightWidth});
  KaMapPagePlate::follow(m_mapPage, m_mainSplit,
                         findChild<QFrame*>(QStringLiteral("mapCard")));  // 새 모양: the panels are one sheet (보완 M1)
  setupFeatureCard(m_inspector->attributeHost(), m_inspector->attributeLayout());
  m_inspector->setFeatureCard(m_featureCard);
  // 스타일 tab: the most recent choice wins, a record on the card or a layer picked in the list (user 2026-10-03).
  const auto styleListLayer = [this] {
    m_inspector->styleEditor()->setLayer(
        qobject_cast<QgsVectorLayer*>(m_layerTree ? m_layerTree->currentLayer() : nullptr));
  };
  connect(m_featureCard, &KaFeatureCard::featureChanged, this, [this, styleListLayer] {
    if (m_featureCard->hasFeature())
      m_inspector->styleEditor()->setLayer(m_featureCard->layer());
    else
      styleListLayer();
  });
  if (m_layerTree) connect(m_layerTree, &QgsLayerTreeView::currentLayerChanged, this, styleListLayer);
  connect(m_inspector->styleEditor(), &KaInspectorStyle::styleApplied, this, [this](QgsVectorLayer*) {
    QgsProject::instance()->setDirty(true);  // the title gets ' *'; 저장 keeps the colour
    LayerOps::applyLayerOrderToLabels(QgsProject::instance(), m_canvas);
    LayerOps::refreshCanvasIfIdle(m_canvas);
    if (m_drawingStudio) m_drawingStudio->refreshMapFromProject();
  });
  styleListLayer();
  connect(m_inspector, &KaInspectorPanel::collapseRequested, this, [this] {
    if (m_shellFocus) m_shellFocus->setRightPanelCollapsed(true);
  });
  auto* foldRight = new QShortcut(QKeySequence(KaShellFocus::rightPanelKey()), this);
  foldRight->setContext(Qt::WindowShortcut);
  connect(foldRight, &QShortcut::activated, this, [this] {
    if (m_shellFocus) m_shellFocus->toggleRightPanel();
  });
  // 2. 「배경 지도」 card: the ribbon's buttons again; the legend stays the only truth.
  m_basemapCard = new KaBasemapQuickCard(m_inspector);
  m_inspector->footerLayout()->addWidget(m_basemapCard);
  connect(m_basemapCard, &KaBasemapQuickCard::basemapRequested, this, [this](const QString& id) {
    if (id == QLatin1String("terrain")) {
      if (m_btnTerrain) m_btnTerrain->click();
    } else if (id == QLatin1String("old_map")) {
      if (auto* oldMaps = findChild<QToolButton*>(QStringLiteral("btnOldMaps"))) oldMaps->showMenu();
    } else if (id == QLatin1String("satellite")) {
      QgsLayerTreeLayer* node = satelliteNode(QgsProject::instance());
      if (node)
        node->setItemVisibilityChecked(!node->isVisible());
      else
        ensureDefaultBasemaps();
    }
    syncBasemapCard();
  });
  // 3. 「조사 열림 · 이름」 badge and the small 「테마 ▾」 at the right end of the tab bar; a badge click goes to the map.
  if (m_viewTabs) {
    auto* corner = new QWidget(m_viewTabs);
    corner->setObjectName(QStringLiteral("viewTabsCorner"));
    auto* cornerRow = new QHBoxLayout(corner);
    cornerRow->setContentsMargins(0, 0, 0, 0);
    cornerRow->setSpacing(6);
    m_surveyBadge = new KaSurveyBadge(corner);
    cornerRow->addWidget(m_surveyBadge);
    cornerRow->addWidget(KaTheme::createColorThemeButton(corner));
    m_viewTabs->setCornerWidget(corner, Qt::TopRightCorner);
    connect(m_surveyBadge, &KaSurveyBadge::clicked, this, &MainWindow::showMapWorkspace);
  }
  // 「→ 제출 5179」 in the status bar looked clickable and did nothing (2026-10-10): it opens 검수·제출 like Ctrl+E.
  if (m_status) connect(m_status, &KaStatusBar::submitClicked, this, &MainWindow::openSubmitReview);
  // 4. Guide band over the canvas; the opacity card moves down while the pill shows.
  QWidget* host = m_canvas->viewport() ? m_canvas->viewport() : static_cast<QWidget*>(m_canvas);
  m_drawGuide = new KaDrawGuideBand(host);
  m_drawGuide->setActions(m_actUndo, m_actRedo);
  if (m_layerOpacityRail)
    connect(m_drawGuide, &KaDrawGuideBand::topInsetChanged, m_layerOpacityRail, &KaLayerOpacityRail::setTopInset);
  // 5. Home 「설정」 → the owning dialog.
  if (m_startPage) connect(m_startPage, &KaStartPage::configureRequested, this, &MainWindow::openAccountSetup);
  // 6. Badge, unsaved chip and the 「저장」 dot follow the title refresh and the dirty flag (300 ms merge).
  m_shellSyncTimer = new QTimer(this);
  m_shellSyncTimer->setObjectName(QStringLiteral("shellSyncTimer"));  // a UI merge, not an autosave (test_save_open)
  m_shellSyncTimer->setSingleShot(true);
  m_shellSyncTimer->setInterval(kShellSyncMs);
  connect(m_shellSyncTimer, &QTimer::timeout, this, [this] {
    const bool hasSurvey = !m_surveyPath.isEmpty();
    const bool unsaved = hasSurvey && surveyHasUnsavedChanges();
    if (m_surveyBadge)
      m_surveyBadge->setSurvey(hasSurvey ? QFileInfo(m_surveyPath).completeBaseName() : QString(), unsaved);
    if (m_status)
      m_status->setUnsavedCount(unsaved ? EditBufferSummary::summarize(QgsProject::instance())
                                        : EditBufferSummary::Summary{});
    if (m_actSave) {
      m_actSave->setIcon(KaIcons::strongIcon(unsaved ? QStringLiteral("save_unsaved") : QStringLiteral("save")));
      // The 「저장」 label turns bold while unsaved: the style sheet keys on this property
      // (QWidget#beginnerRibbon QToolButton[unsaved="true"]), and a changed property needs a repolish.
      for (QObject* object : m_actSave->associatedObjects()) {
        auto* chip = qobject_cast<QToolButton*>(object);
        if (!chip || chip->property("unsaved").toBool() == unsaved) continue;
        chip->setProperty("unsaved", unsaved);
        chip->style()->unpolish(chip);
        chip->style()->polish(chip);
      }
    }
  });
  connect(QgsProject::instance(), &QgsProject::isDirtyChanged, this, [this](bool) { syncShellChips(); });
  // A buffered edit does not flip the dirty flag once it is set: the layers report too.
  const auto watch = [this](const QList<QgsMapLayer*>& layers) {
    if (m_canvas) m_canvas->setSelectionColor(QColor(255, 235, 59, 45));
    QgsProject::instance()->setSelectionColor(QColor(255, 235, 59, 45));
    for (QgsMapLayer* layer : layers) {
      auto* vector = qobject_cast<QgsVectorLayer*>(layer);
      if (!vector) continue;
      if (!LayerOps::layerKeyOf(vector).isEmpty()) {
        bool noFill = false;
        LayerOps::readSimpleVectorStyle(vector, nullptr, nullptr, nullptr, nullptr, &noFill);
        LayerStyleDefaults::applySelectionStyle(vector, noFill);
      }
      connect(vector, &QgsVectorLayer::layerModified, this, &MainWindow::syncShellChips, Qt::UniqueConnection);
      connect(vector, &QgsVectorLayer::editingStopped, this, &MainWindow::syncShellChips, Qt::UniqueConnection);
    }
  };
  connect(QgsProject::instance(), &QgsProject::layersAdded, this, watch);
  watch(QgsProject::instance()->mapLayers().values());
  KaPaleoMenu::install(m_btnPaleo, this, [this] { startPaleoLandform(); });
  syncShellChips();
  syncBasemapCard();
#endif
}

void MainWindow::openAccountSetup(AccountStatus::Source source) {
  using Source = AccountStatus::Source;
  if (source == Source::VworldKey || source == Source::HistoryGisKey)
    configureVworldKey();
  else if (source == Source::CadastralAccount)
    configureCadastralAccount();
  else if (source == Source::TopographicAccount)
    configureTopographicAccount();
  else if (source == Source::HeritageAccount)
    configureHeritageAccount();
  if (!m_startPage) return;
  m_startPage->reload();
  m_startPage->refreshConnections();  // 「설정됨」 dot and text; deferred past the clicked row's own button
}

void MainWindow::syncShellChips() {
  // Before setupStrataShell (early title refreshes) there is nothing to feed yet.
  if (m_shellSyncTimer && !m_shellSyncTimer->isActive()) m_shellSyncTimer->start();
}

void MainWindow::syncBasemapCard() {
#if KA_HGIS_HAS_QGIS
  if (!m_basemapCard) return;
  QgsProject* project = QgsProject::instance();
  QgsLayerTreeLayer* satellite = satelliteNode(project);
  m_basemapCard->setChecked(QStringLiteral("satellite"), satellite && satellite->isVisible());
  m_basemapCard->setChecked(QStringLiteral("terrain"), m_btnTerrain && m_btnTerrain->isChecked());
  m_basemapCard->setChecked(QStringLiteral("old_map"),
                            LayerOps::isLayerVisible(project, QStringLiteral("대동여지도")) ||
                                LayerOps::isLayerVisible(project, QStringLiteral("1919 조선지형도 1:5만")));
#endif
}

void MainWindow::recordSurveyFacts() {
#if KA_HGIS_HAS_QGIS
  if (m_surveyPath.isEmpty()) return;
  QgsProject* project = QgsProject::instance();
  const int areas = featureTotal(project, QStringLiteral("survey_area"));
  const int features =
      featureTotal(project, QStringLiteral("feature_poly")) + featureTotal(project, QStringLiteral("feature_line"));
  QSettings st = RecentSurveys::userSettings();
  SurveyFacts::rememberCounts(st, m_surveyPath, areas, features, project->crs().authid());
#endif
}

// currentToolIconId() lives next to currentToolLabel() in KaDrawSketchTools.cpp (same tool
// dispatch, same tool headers); the sentence table is here so the wording stays in one file.
QString MainWindow::currentToolHint() const {
  return hintForTool(currentToolIconId());
}
