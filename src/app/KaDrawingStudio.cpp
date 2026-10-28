#include "KaDrawingStudio.h"
#include "KaWindowSize.h"
#include "KaCanvasGridOverlay.h"
#include "core/KaLogExcept.h"
#include "KaLayerInformation.h"
#include <QDomDocument>
#include <qgsmaplayerstyle.h>
#include "KaTheme.h"
#include "KaIcons.h"
#include "KaCrashGuard.h"
#include "KaBeginnerRibbon.h"
#include "KaDrawingCoordCallout.h"
#include "KaDrawingLayerSets.h"
#include "KaDrawingNorthArrow.h"
#include "KaDrawingPdfProgress.h"
#include "KaDrawingSheetSet.h"
#include "KaDrawingTools.h"
#include "KaPaperDialog.h"
#include "KaMapPagePlate.h"
#include "KaUserError.h"
#include "KaTitleBlock.h"
#include "core/LayoutService.h"
#include "core/LayerOps.h"
#include "core/BasemapPolicy.h"
#include "core/PdfExportSettings.h"
#include "core/StandardScales.h"
#include "core/GeorefService.h"
#include "KaFileBrowserPanel.h"
#include "KaLayerOpacityRail.h"
#include "KaPrintDialog.h"
#include "MainWindow.h"

#include <QInputDialog>
#include <QScopedValueRollback>
#include <QTemporaryDir>
#include <algorithm>
#include <cmath>

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QAction>
#include <QBrush>
#include <QButtonGroup>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QIODevice>
#include <QDataStream>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QEvent>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QKeySequence>
#include <QUndoStack>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QScrollBar>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QShowEvent>
#include <QSizePolicy>
#include <QToolButton>
#include <QGraphicsRectItem>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QTransform>
#include <QWheelEvent>
#include <QToolButton>
#include <QVBoxLayout>

#include <qgis.h>
#include <qgsapplication.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransform.h>
#include <qgspointxy.h>
#include <qgscoordinatetransform.h>
#include <qgsgui.h>
#include <qgslayout.h>
#include <qgslayoutexporter.h>
#include <qgslayoutguiutils.h>
#include <qgslayoutitem.h>
#include <qgslayoutpoint.h>
#include <qgslayoutsize.h>
#include <qgslayoutitempage.h>
#include <qgslayoutitemlabel.h>
#include <qgslayoutitemlegend.h>
#include <qgslayoutitemmap.h>
#include <qgsmapsettings.h>
#include <qgslayoutitemmapgrid.h>
#include <qgstextformat.h>
#include <qgslayoutitempage.h>
#include <qgslayoutitempicture.h>
#include <qgslayoutitemscalebar.h>
#include <qgslayoutitempolyline.h>
#include <qgslayoutitempolygon.h>
#include <qgslinesymbol.h>
#include <qgsfillsymbol.h>
#include <qgslayoutmanager.h>
#include <qgslayoutmeasurementconverter.h>
#include <qgslayoutpagecollection.h>
#include <qgslayoutrendercontext.h>
#include <qgslayoutview.h>
#include <qgslayoutviewmouseevent.h>
#include <qgslayoutviewtool.h>
#include <qgslayoutviewtoolselect.h>
#include <qgslayertree.h>
#include <qgslayertreenode.h>
#include <qgslayertreelayer.h>
#include <qgslayertreemodel.h>
#include <qgslayertreeview.h>
#include <qgsmapcanvas.h>
#include <qgsmaplayer.h>
#include <qgsmasterlayoutinterface.h>
#include <qgsprintlayout.h>
#include <qgslayoutundostack.h>
#include <qgsproject.h>
#include <qgsrectangle.h>
#include <qgsvectorlayer.h>
#include <qgsfeature.h>
#include <qgsfeaturerequest.h>
#include <qgsgeometry.h>
#include <qgswkbtypes.h>

namespace {
constexpr const char* kSheetName = "user_sheet";
constexpr const char* kIdMap = "ka_map";
constexpr const char* kIdLegend = "ka_legend";
// 라벨 위에 위 레이어를 한 번 더 그리는 덧지도. 본 지도 바로 위에 놓는다.
constexpr const char* kIdMapAbove = "ka_map_above";
constexpr const char* kIdMapNumbers = "ka_map_numbers";
constexpr const char* kIdNorth = "ka_north";
constexpr const char* kIdScaleBar = "ka_scalebar";
constexpr const char* kIdScale = "ka_scale";
constexpr const char* kIdCrs = "ka_crs";
constexpr const char* kIdTerrain3d = "ka_terrain3d";
constexpr const char* kGridAnnPrefix = "ka_grid_ann_";
// 화면 미리보기 해상도. 인쇄용 300 DPI로 미리보기를 그리면 A4 지도 칸이
// 2300×3000픽셀이 되어 QGIS가 래스터를 2000픽셀 조각으로 나누고 위성 타일도
// 수백 장 필요해진다. 조각 하나가 비면 위성이 반만 나온 것처럼 보인다.
constexpr double kPreviewDpi = 96.0;

bool isLiveBasemapLayer(QgsMapLayer* layer) {
  if (!layer) return false;
  const QString p = layer->providerType().toLower();
  return p == QLatin1String("wms") || p == QLatin1String("xyz");
}

QgsCoordinateReferenceSystem studioMapCrs(QgsMapCanvas* canvas, QgsProject* project) {
  if (canvas && canvas->mapSettings().destinationCrs().isValid())
    return canvas->mapSettings().destinationCrs();
  if (project && project->crs().isValid())
    return project->crs();
  return QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186"));
}

QgsRectangle studioMapExtent(QgsMapCanvas* canvas, const QgsCoordinateReferenceSystem& crs) {
  QgsRectangle ext;
  if (canvas)
    ext = canvas->extent();
  if (ext.isNull() || !ext.isFinite() || ext.width() <= 0.0 || ext.height() <= 0.0)
    ext = LayerOps::koreaExtentForCrs(crs.isValid() ? crs.authid() : QStringLiteral("EPSG:5186"));
  return ext;
}

QString koreanCrsLabel(const QgsCoordinateReferenceSystem& crs) {
  const QString auth = crs.isValid() ? crs.authid() : QString();
  if (auth.isEmpty())
    return QStringLiteral("좌표계 미지정");
  const QString name = LayoutService::koreanCrsName(auth);
  return name.isEmpty() ? QStringLiteral("좌표계 %1").arg(auth)
                        : QStringLiteral("%1 · %2").arg(name, auth);
}

QgsLayoutItem* findItemById(QgsLayout* ly, const char* id) {
  if (!ly) return nullptr;
  for (QGraphicsItem* gi : ly->items()) {
    if (auto* it = dynamic_cast<QgsLayoutItem*>(gi)) {
      if (it->id() == QLatin1String(id))
        return it;
    }
  }
  return nullptr;
}

QRectF itemPaperRect(QgsLayoutItem* item) {
  if (!item)
    return {};
  const QRectF box(item->pos(), item->rect().size());
  if (box.width() >= 8.0 && box.height() >= 8.0)
    return box;
  return item->sceneBoundingRect();
}

bool isSheetDecoration(const QgsLayoutItem* item) {
  if (!item) return false;
  const QString id = item->id();
  return id == QLatin1String(kIdScaleBar) || id == QLatin1String(kIdScale)
         || id == QLatin1String(kIdCrs) || id == QLatin1String(kIdNorth);
}

double decorationSizePercent(const QgsLayoutItem* item) {
  return item->customProperty(QStringLiteral("ka_hgis/chrome_size_percent"), 100.).toDouble();
}

void compactSheetScaleBar(QgsLayoutItemScaleBar* sb) {
  if (!sb)
    return;
  const double factor = decorationSizePercent(sb) / 100.;
  sb->setLabelVerticalPlacement(Qgis::ScaleBarDistanceLabelVerticalPlacement::AboveSegment);
  sb->setHeight(2.4 * factor);
  sb->setLabelBarSpace(1.0 * factor);
  sb->setBoxContentSpace(0.6 * factor);
  QgsTextFormat fmt = sb->textFormat();
  QFont f(QStringLiteral("Malgun Gothic"), 7);
  fmt.setFont(f);
  fmt.setSize(7.0 * factor);
  fmt.setSizeUnit(Qgis::RenderUnit::Points);
  sb->setTextFormat(fmt);
  LayoutService::applySheetScaleBarInk(sb);
}

QIcon scaleBarPreviewIcon(const char* style) {
  QPixmap pm(72, 40);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(QPen(KaTheme::tokens().border, 1));
  p.setBrush(KaTheme::tokens().bevelLight);
  p.drawRoundedRect(QRectF(1, 4, 70, 32), 5, 5);
  const QString s = QString::fromUtf8(style);
  if (s == QLatin1String("Line Ticks Up")) {
    p.setPen(QPen(KaTheme::tokens().ink, 1.6));
    p.drawLine(QPointF(10, 26), QPointF(62, 26));
    for (int i = 0; i < 5; ++i) {
      const double x = 10.0 + i * 13.0;
      p.drawLine(QPointF(x, 26), QPointF(x, 14));
    }
  } else if (s == QLatin1String("Single Box")) {
    p.setPen(QPen(KaTheme::tokens().ink, 1));
    for (int i = 0; i < 4; ++i) {
      p.setBrush(i % 2 == 0 ? KaTheme::tokens().ink : KaTheme::tokens().bevelLight);
      p.drawRect(QRectF(10 + i * 13, 16, 13, 10));
    }
  } else {
    p.setPen(QPen(KaTheme::tokens().ink, 1));
    for (int row = 0; row < 2; ++row) {
      for (int i = 0; i < 4; ++i) {
        const bool dark = ((i + row) % 2) == 0;
        p.setBrush(dark ? KaTheme::tokens().ink : KaTheme::tokens().bevelLight);
        p.drawRect(QRectF(10 + i * 13, 12 + row * 8, 13, 8));
      }
    }
  }
  return QIcon(pm);
}

QToolButton* makeRailTile(QWidget* parent, const QIcon& icon, const QString& text, const QSize& iconSize) {
  auto* b = new QToolButton(parent);
  b->setIcon(icon);
  b->setIconSize(iconSize);
  const QString displayText = (text.size() <= 5 && !text.contains(QLatin1Char('\n')))
                                  ? text
                                  : KaBeginnerRibbon::twoLine(text);
  b->setText(displayText);
  b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  b->setAutoRaise(true);
  b->setCursor(Qt::PointingHandCursor);
  b->setToolTip(text);
  b->setProperty("class", QStringLiteral("sampleTile"));
  b->ensurePolished();
  b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  const auto& metrics = KaTheme::buttonMetrics();
  // Qt 6: once minimumWidth is set, layout ignores minimumSizeHint and can
  // shrink below sizeHint (https://doc.qt.io/qt-6/qwidget.html).
  b->setMinimumWidth(std::max(metrics.scaleButtonMinWidth, b->sizeHint().width()));
  const int contentHeight = iconSize.height() + 2 * b->fontMetrics().lineSpacing()
                            + 2 * metrics.buttonPadding + 6;
  b->setFixedHeight(std::max({metrics.layoutButtonHeight, contentHeight, b->sizeHint().height()}));
  return b;
}
}

class KaLayoutMenuProvider : public QgsLayoutViewMenuProvider {
public:
  explicit KaLayoutMenuProvider(KaDrawingStudio* studio) : m_studio(studio) {}

  QMenu* createContextMenu(QWidget* parent, QgsLayout* layout, QPointF layoutPoint) const override {
    if (!m_studio || !layout) return nullptr;
    QgsLayoutItem* hit = layout->layoutItemAt(layoutPoint, true);
    auto* map = dynamic_cast<QgsLayoutItemMap*>(hit);
    if (!map) {
      for (QGraphicsItem* gi : layout->items(layoutPoint)) {
        map = dynamic_cast<QgsLayoutItemMap*>(gi);
        if (map) break;
      }
    }
    if (!map) return nullptr;
    if (m_studio->isPlacingCoordPoint()) {
      auto* menu = new QMenu(parent);
      menu->addAction(QStringLiteral("마지막 점 지우기"), m_studio,
                      &KaDrawingStudio::undoLastCoordCallout);
      menu->addAction(QStringLiteral("점찍기 끝"), m_studio, &KaDrawingStudio::endPlaceCoordPoint);
      return menu;
    }
    auto* menu = new QMenu(parent);
    menu->addAction(KaIcons::icon(QStringLiteral("layout_activate")),
                    QStringLiteral("지도 조정"),
                    m_studio, &KaDrawingStudio::beginActivateMap);
    menu->addAction(KaIcons::icon(QStringLiteral("layout_center")),
                    QStringLiteral("그린 것 가운데로"),
                    m_studio, &KaDrawingStudio::centerSurveyInMap);
    if (m_studio->isMapAdjusting()) {
      menu->addSeparator();
      menu->addAction(KaIcons::icon(QStringLiteral("layout_activate_done")),
                      QStringLiteral("지도조정끝"),
                      m_studio, &KaDrawingStudio::endActivateMap);
    }
    return menu;
  }

private:
  KaDrawingStudio* m_studio = nullptr;
};

void KaDrawingStudio::ensureLayoutGuiRegistered(QgsMapCanvas* mapCanvas) {
  static bool once = false;
  if (once) return;
  once = true;
  if (mapCanvas)
    QgsLayoutGuiUtils::registerGuiForKnownItemTypes(mapCanvas);
}

bool KaDrawingStudio::promptPaper(QWidget* parent, double* widthMm, double* heightMm) {
  return KaPaperDialog::prompt(parent, widthMm, heightMm, kFieldPaperWidthMm, kFieldPaperHeightMm);  // 보완 L5
}

KaDrawingStudio::KaDrawingStudio(QgsProject* project, QgsMapCanvas* mapCanvas,
                                 double paperWidthMm, double paperHeightMm, QWidget* parent)
    : QMainWindow(parent), m_project(project ? project : QgsProject::instance()), m_mapCanvas(mapCanvas),
      m_paperW(paperWidthMm), m_paperH(paperHeightMm) {
  setAttribute(Qt::WA_DeleteOnClose, false);
  setWindowTitle(QStringLiteral("도면"));
  resize(1280, 860);
  ensureLayoutGuiRegistered(mapCanvas);
  const auto* savedLayout = layout();
  if (!savedLayout) ensureBlankLayout();
  else LayoutService::ensureLayoutPage(layout(), m_paperW, m_paperH);
  buildUi();
  if (savedLayout) {
    connectMapSignals(mapItem());
    if (isLegacyA4LandscapeMm(m_paperW, m_paperH))
      applyFieldPageGrow();
    else {
      zoomPaperVisible();
      ensureStandardDecorations();
    }
  } else {
    autoPlaceDefaultSheet();
  }
  connect(&m_heritageNumbers, &HeritageLayoutNumbers::visibleEntriesChanged, this, [this]() {
    if (property("ka_interacting").toBool()) {
      setProperty("ka_visible_legend_pending", true);
      return;
    }
    if (auto* legend = dynamic_cast<QgsLayoutItemLegend*>(findItemById(layout(), kIdLegend))) {
      bool missing = !legend->model() || !legend->model()->rootGroup();
      QSet<QString> present;
      if (!missing) {
        for (auto* node : legend->model()->rootGroup()->findLayers()) {
          if (node->layer()) present.insert(node->layer()->id());
        }
      }
      for (const auto& entry : m_heritageNumbers.entries()) {
        if (entry.number > 0 && !present.contains(entry.layerId)) {
          missing = true;
          break;
        }
      }
      if (missing) LayoutService::tuneSheetLegend(legend);
      m_heritageNumbers.applyLegend(legend);
    }
  });
  // The embedded studio shares MainWindow's shortcut router. Registering a
  // second WindowShortcut makes Ctrl+Z ambiguous and neither action runs.
  if (!parent) {
    auto* undoAct = new QAction(QStringLiteral("되돌리기"), this);
    undoAct->setShortcut(QKeySequence::Undo);
    connect(undoAct, &QAction::triggered, this, &KaDrawingStudio::undoLastChange);
    addAction(undoAct);
  }
}

bool KaDrawingStudio::isLegacyA4LandscapeMm(double widthMm, double heightMm) {
  return qAbs(widthMm - kA4LandscapeWidthMm) < 0.51 &&
         qAbs(heightMm - kA4LandscapeHeightMm) < 0.51;
}

void KaDrawingStudio::applyFieldPageGrow() {
  if (!isLegacyA4LandscapeMm(m_paperW, m_paperH))
    return;
  resetPaper(kFieldPaperWidthMm, kFieldPaperHeightMm, true);
}

void KaDrawingStudio::applyFieldEdge() {
  applyStandardChromePositions();
}

void KaDrawingStudio::openPaperSettingsDialog() {
  double w = m_paperW;
  double h = m_paperH;
  if (!promptPaper(this, &w, &h))
    return;
  resetPaper(w, h, true);
  markUserComposed();
}

void KaDrawingStudio::resetPaper(double widthMm, double heightMm, bool preserveExisting) {
  auto* ly = layout();
  QgsLayoutItemMap* map = ly ? mapItem() : nullptr;

  if (!preserveExisting || !ly || !map) {
    m_paperW = widthMm;
    m_paperH = heightMm;
    endActivateMap();
    ensureBlankLayout();
    m_paperFitPending = true;
    zoomPaperVisible();
    autoPlaceDefaultSheet();
    if (m_status)
      showStatus(QStringLiteral("용지에 지도를 올려 두었습니다. 축척을 맞추거나 그린곳 가운데를 누르세요."));
    return;
  }

  // ── 기존 도면 요소 및 지도 범위 보존하면서 용지 크기/방향 전환 ──
  endActivateMap();

  const double oldW = m_paperW;
  const double oldH = m_paperH;
  m_paperW = widthMm;
  m_paperH = heightMm;

  const double dw = m_paperW - oldW;
  const double dh = m_paperH - oldH;

  // 1. QGIS Layout 페이지 크기 변경
  if (ly->pageCollection() && ly->pageCollection()->pageCount() > 0) {
    ly->pageCollection()->page(0)->setPageSize(
        QgsLayoutSize(m_paperW, m_paperH, Qgis::LayoutUnit::Millimeters));
  }

  // 2. 지도 중심 좌표, 축척, CRS 보존
  const QgsRectangle curExt = map->extent();
  const QgsPointXY mapCenter = curExt.center();
  const double mapScale = map->scale();
  const QgsCoordinateReferenceSystem mapCrs = map->crs();

  // 3. 지도 프레임 위치 및 크기를 새 용지 마진에 맞춰 재계산
  const QRectF newMapRect = defaultMapRect();
  map->attemptMove(QgsLayoutPoint(newMapRect.x(), newMapRect.y(), Qgis::LayoutUnit::Millimeters));
  map->attemptResize(QgsLayoutSize(newMapRect.width(), newMapRect.height(),
                                   Qgis::LayoutUnit::Millimeters));

  // 지도 중심과 축척 복원
  if (mapCrs.isValid()) map->setCrs(mapCrs);
  const QgsRectangle nextExt(mapCenter.x() - curExt.width() * 0.5,
                             mapCenter.y() - curExt.height() * 0.5,
                             mapCenter.x() + curExt.width() * 0.5,
                             mapCenter.y() + curExt.height() * 0.5);
  map->zoomToExtent(nextExt);
  if (mapScale > 1.0)
    map->setScale(mapScale, true);
  map->invalidateCache();
  map->refresh();

  // 4. 범례·표제란 등 나머지 항목은 상대 이동 뒤 용지 안으로. 용지 자체는 건드리지 않고,
  //    아래 띠 장식 넷(축척자·축척 글·좌표계·방위)은 6에서 새 용지의 정한 자리에 다시 놓는다
  //    (2026-10-28 현장: 용지를 바꾸면 엉뚱한 곳으로 가던 것. 「일률적으로 자리를 잡게」).
  for (QGraphicsItem* gi : ly->items()) {
    auto* item = dynamic_cast<QgsLayoutItem*>(gi);
    if (!item || item == map || dynamic_cast<QgsLayoutItemPage*>(item) || isSheetDecoration(item)) continue;

    const QRectF itemBox = itemPaperRect(item);
    const QPointF pos = itemBox.topLeft();
    const QSizeF sz = itemBox.size();

    double newX = pos.x();
    double newY = pos.y();

    // 하단 영역(oldH의 55% 이상 위치)에 있던 아이템은 Y 오프셋(dh)만큼 아래로 이동
    if (pos.y() > oldH * 0.55) {
      newY += dh;
    }
    // 우측 영역(oldW의 50% 이상 위치)에 있던 아이템은 X 오프셋(dw)만큼 우측으로 이동
    if (pos.x() > oldW * 0.5) {
      newX += dw;
    }

    // 용지 영역 벗어남 방지 (클램핑)
    const double maxX = std::max(4.0, m_paperW - sz.width() - 4.0);
    const double maxY = std::max(4.0, m_paperH - sz.height() - 4.0);
    newX = std::clamp(newX, 4.0, maxX);
    newY = std::clamp(newY, 4.0, maxY);

    item->attemptSetSceneRect(QRectF(QPointF(newX, newY), sz));
  }

  // 5. 좌표점 콜아웃 재계산 (있을 경우)
  if (!m_coordMapPts.isEmpty()) {
    relayoutCoordCallouts();
  }

  // 6. 장식을 새 용지의 띠에 놓고 뷰 갱신
  m_paperFitPending = true;
  zoomPaperVisible();
  relinkDecorations();
  replaceSheetChrome();
  ly->refresh();

  if (m_status) {
    showStatus(QStringLiteral("용지 전환 완료: %1×%2 mm — 기존 지도 범위와 배치가 유지되었습니다.")
                          .arg(qRound(m_paperW))
                          .arg(qRound(m_paperH)));
  }
}

void KaDrawingStudio::ensureBlankLayout() {
  if (!m_project) return;
  detachLayoutFromView();
  LayoutService::createBlankSheet(m_project, m_paperW, m_paperH, QString::fromUtf8(kSheetName));
  m_coordMapPts.clear();
  m_coordFrameMap.clear();
  attachLayoutToView();
}

void KaDrawingStudio::detachLayoutFromView() {
  // The working layout is about to be replaced and deleted. Let go of it first.
  if (!m_view) return;
  if (QgsLayoutViewTool* t = m_view->tool())
    m_view->unsetTool(t);
  static_cast<QGraphicsView*>(m_view)->setScene(nullptr);
}

void KaDrawingStudio::ensureSheetPage() {
  if (auto* ly = layout())
    LayoutService::ensureLayoutPage(ly, m_paperW, m_paperH);
}

bool KaDrawingStudio::sheetPageInView() const {
  auto* ly = layout();
  if (!m_view || !m_view->viewport() || !ly || !ly->pageCollection())
    return false;
  if (m_view->viewport()->width() < 32 || m_view->viewport()->height() < 32)
    return false;
  QgsLayoutItemPage* page = ly->pageCollection()->page(0);
  if (!page)
    return false;
  const QRectF paper = page->mapRectToScene(page->rect());
  if (paper.width() < 8.0 || paper.height() < 8.0)
    return false;
  const QRectF visible = m_view->mapToScene(m_view->viewport()->rect()).boundingRect();
  return visible.intersects(paper);
}

void KaDrawingStudio::showSheetPage() {
  ensureSheetPage();
  if (!sheetPageInView())
    zoomPaperVisible();
}

void KaDrawingStudio::attachLayoutToView() {
  auto* ly = layout();
  if (!m_view || !ly) return;
  ensureSheetPage();
  // 위성 배경이 조각 단위로 빈 채 남는 것을 막는다(다시 열린 조판까지 포함).
  LayoutService::applySingleRasterPassRendering(ly);
  // 화면 미리보기는 화면 해상도로 그린다. 300 DPI로 미리보기를 그리면 A4가
  // 2300×3000픽셀이 되어 위성 타일을 수백 장 받아야 하고, 다 못 받으면 배경이
  // 사각형 단위로 빈다. 인쇄·PDF는 내보낼 때만 300 DPI로 올린다(savePdf).
  ly->renderContext().setDpi(kPreviewDpi);
  if (m_view->currentLayout() != ly)
    m_view->setCurrentLayout(ly);
  if (m_toolSelect) {
    m_toolSelect->setLayout(ly);
    // 도구를 하나도 걸지 않으면 용지 위 항목이 선택되지 않고, 선택이 없으면
    // Delete 도 지울 것을 못 찾는다. 범례가 안 지워지던 원인이 이것이다.
    // ensureBlankLayout 이 unsetTool 을 하므로 붙일 때마다 다시 건다.
    if (m_view && !m_view->tool())
      m_view->setTool(m_toolSelect);
  }
  // Scene backgrounds also reach exports with transparent pages; preserve that color.
  ly->setBackgroundBrush(QBrush(QColor(229, 231, 235)));
  m_view->setBackgroundBrush(QBrush(KaTheme::tokens().desk));
  connect(ly, &QgsLayout::selectedItemChanged, this, &KaDrawingStudio::onLayoutSelectionChanged,
          Qt::UniqueConnection);
  loadGridSettings();
  if (m_heritageNames) {
    const QSignalBlocker block(m_heritageNames);
    m_heritageNames->setChecked(ly->customProperty(HeritageLayoutNumbers::namesPropertyKey(), false).toBool());
  }
  updateInspector(nullptr);
}

QgsPrintLayout* KaDrawingStudio::layout() const {
  if (!m_project) return nullptr;
  return dynamic_cast<QgsPrintLayout*>(
      m_project->layoutManager()->layoutByName(QString::fromUtf8(kSheetName)));
}

QgsLayoutItemMap* KaDrawingStudio::mapItem() const {
  return dynamic_cast<QgsLayoutItemMap*>(findItemById(layout(), kIdMap));
}

void KaDrawingStudio::buildUi() {
  auto* root = new QWidget(this);
  auto* rootLay = new QHBoxLayout(root);
  rootLay->setContentsMargins(0, 0, 0, 0);
  rootLay->setSpacing(0);

  auto* leftCol = new QFrame(root);
  leftCol->setObjectName(QStringLiteral("studioLeftCol"));
  leftCol->setMinimumWidth(160);
  leftCol->setMaximumWidth(720);
  auto* leftLay = new QVBoxLayout(leftCol);
  leftLay->setContentsMargins(6, 6, 6, 6);
  leftLay->setSpacing(4);

  auto* leftSplit = new QSplitter(Qt::Vertical, leftCol);
  leftSplit->setObjectName(QStringLiteral("studioLeftSplit"));
  KaMapPagePlate::followHandle(leftSplit, 8);  // 새 모양: 7 px with one hairline
  leftSplit->setChildrenCollapsible(false);
  leftSplit->setCollapsible(1, true);
  leftSplit->installEventFilter(this);

  auto* layerBox = new QFrame(leftSplit);
  layerBox->setObjectName(QStringLiteral("layersCard"));
  auto* layerLay = new QVBoxLayout(layerBox);
  layerLay->setContentsMargins(8, 8, 8, 8);
  layerLay->setSpacing(4);
  auto* leftCap = new QLabel(QStringLiteral("레이어"), layerBox);
  leftCap->setObjectName(QStringLiteral("cardCaption"));
  auto* capRow = new QHBoxLayout();
  capRow->setContentsMargins(0, 0, 0, 0);
  capRow->setSpacing(6);
  capRow->addWidget(leftCap);
  capRow->addStretch(1);
  auto* filesToggle = new QToolButton(layerBox);
  filesToggle->setObjectName(QStringLiteral("sidebarFilesToggle"));
  filesToggle->setText(QStringLiteral("파일함"));
  filesToggle->setCheckable(true);
  filesToggle->setChecked(true);
  capRow->addWidget(KaDrawingStudioPlate::quiet(filesToggle));  // 새 모양: the map screen's quiet header words
  m_layerCheckAllBtn = new QToolButton(layerBox);
  m_layerCheckAllBtn->setObjectName(QStringLiteral("layoutLayerCheckAllBtn"));
  m_layerCheckAllBtn->setFocusPolicy(Qt::NoFocus);
  m_layerCheckAllBtn->setToolTip(QStringLiteral(
      "레이어 체크를 한 번에 모두 끄거나 켭니다. 하나라도 켜져 있으면 전부 끕니다."));
  connect(m_layerCheckAllBtn, &QToolButton::clicked, this, &KaDrawingStudio::toggleAllLayersChecked);
  capRow->addWidget(KaDrawingStudioPlate::quiet(m_layerCheckAllBtn));
  // Named layer sets (map themes). Applied only when the user picks one here.
  auto* layerSetBtn = new QToolButton(layerBox);
  layerSetBtn->setObjectName(QStringLiteral("layoutLayerSetBtn"));
  KaDrawingStudioPlate::setWord(layerSetBtn);  // 「세트」, 「세트 ▾」 in 새 모양 (2차 보드 판 4)
  layerSetBtn->setFocusPolicy(Qt::NoFocus);
  layerSetBtn->setPopupMode(QToolButton::InstantPopup);
  layerSetBtn->setToolTip(QStringLiteral(
      "켜 둔 레이어 조합을 이름 붙여 저장하고, 고를 때만 다시 적용합니다 (예: 유구 배치, 지형·지적 중첩)."));
  auto* layerSetMenu = new QMenu(layerSetBtn);
  layerSetBtn->setMenu(layerSetMenu);
  connect(layerSetMenu, &QMenu::aboutToShow, this, [this, layerSetMenu]() {
    KaDrawingLayerSets::fillMenu(layerSetMenu, m_project, m_layerModel, this,
                                 [this](const QString& text) { showStatus(text); });
  });
  capRow->addWidget(KaDrawingStudioPlate::quiet(layerSetBtn));
  auto* informationModel = new KaLayerInformationModel(m_project, true, this);
  m_layerModel = informationModel;
  m_layerModel->setFlag(QgsLayerTreeModel::AllowNodeChangeVisibility, true);
  m_layerModel->setFlag(QgsLayerTreeModel::AllowNodeReorder, true);
  m_layerModel->setFlag(QgsLayerTreeModel::AllowNodeRename, false);
  m_layerModel->setAutoCollapseLegendNodes(1);
  m_layerTree = new KaLayerInformationView(layerBox);
  m_layerTree->setObjectName(QStringLiteral("layoutLayerTree"));
  m_layerTree->setModel(m_layerModel);
  KaLayerInformationModel::configureView(m_layerTree);
  connect(informationModel, &KaLayerInformationModel::labelsEdited, this, &KaDrawingStudio::syncMapFromLayers);
  connect(this, &KaDrawingStudio::drawingScaleChanged, informationModel, &KaLayerInformationModel::setScale);
  if (auto* map = mapItem()) informationModel->setScale(map->scale());
  m_layerTree->setMinimumWidth(300);
  m_layerTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_layerTree->setDragDropMode(QAbstractItemView::InternalMove);
  m_layerTree->setDefaultDropAction(Qt::MoveAction);
  m_layerTree->setDragEnabled(true);
  m_layerTree->setAcceptDrops(true);
  m_layerTree->setDropIndicatorShown(true);
  m_layerTree->installEventFilter(this);
  if (m_layerTree->viewport())
    m_layerTree->viewport()->installEventFilter(this);
  m_layerTree->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_layerTree, &QWidget::customContextMenuRequested, this, [this](const QPoint& pt) {
    auto* mainWin = qobject_cast<MainWindow*>(window());
    if (mainWin) {
      mainWin->showLayerTreeContextMenu(m_layerTree, pt);
    } else {
      QMenu menu(this);
      menu.addAction(QStringLiteral("레이어 삭제"), this, &KaDrawingStudio::removeSelectedLayers);
      menu.exec(m_layerTree->viewport()->mapToGlobal(pt));
    }
  });
  connect(m_layerTree, &QTreeView::doubleClicked, this, [this](const QModelIndex& index) {
    if (index.column() != 0) return;
    auto* mainWin = qobject_cast<MainWindow*>(window());
    if (mainWin) {
      mainWin->editCurrentLayerStyle(m_layerTree->currentLayer());
    }
  });
  connect(m_layerTree, &QgsLayerTreeView::currentLayerChanged, this,
          &KaDrawingStudio::updateLayerOpacityControl);

  // Selecting a row does not change the drawing's layer set or styles.
  connect(m_layerModel, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex& first, const QModelIndex&) {
    if (first.column() == 0) syncMapFromLayers();
  });
  connect(m_layerModel, &QAbstractItemModel::rowsMoved, this, &KaDrawingStudio::syncMapFromLayers);
  connect(m_layerModel, &QAbstractItemModel::modelReset, this, &KaDrawingStudio::syncMapFromLayers);
  connect(m_layerModel, &QAbstractItemModel::layoutChanged, this, &KaDrawingStudio::syncMapFromLayers);
  if (QgsLayerTree* tree = m_project->layerTreeRoot()) {
    connect(tree, &QgsLayerTreeNode::visibilityChanged, this, [this](QgsLayerTreeNode* node) {
      LayerOps::revealCheckedLegendNode(node);
      refreshLayerCheckAllButton();
      syncMapFromLayers();
    });
  }
  layerLay->addLayout(capRow);
  layerLay->addWidget(m_layerTree, 1);
  layerLay->addWidget(new KaLayerInformationPanel(informationModel, m_layerTree, layerBox));
  refreshLayerCheckAllButton();
  auto* layerEmpty = new QLabel(
      QStringLiteral("이 도면에는 레이어가 없습니다.\n필요한 레이어를 켜고\n용지 위에 올려 보세요."),
      layerBox);
  layerEmpty->setObjectName(QStringLiteral("emptyState"));
  layerEmpty->setAlignment(Qt::AlignCenter);
  layerEmpty->setWordWrap(true);
  layerEmpty->setVisible(m_project->mapLayers().isEmpty());
  layerLay->addWidget(layerEmpty);
  auto syncEmpty = [this, layerEmpty]() {
    const bool empty = !m_project || m_project->mapLayers().isEmpty();
    if (layerEmpty) layerEmpty->setVisible(empty);
    if (m_layerTree) m_layerTree->setVisible(!empty);
  };
  connect(m_project, &QgsProject::layersAdded, this,
          [syncEmpty](const QList<QgsMapLayer*>&) { syncEmpty(); });
  connect(m_project, &QgsProject::layersRemoved, this,
          [syncEmpty](const QStringList&) { syncEmpty(); });
  syncEmpty();
  leftSplit->addWidget(layerBox);

  auto* mainWin = qobject_cast<MainWindow*>(parentWidget());
  if (!mainWin) mainWin = qobject_cast<MainWindow*>(parent());

  auto* filesScroll = new QScrollArea(leftSplit);
  filesScroll->setObjectName(QStringLiteral("sidebarFilesScroll"));
  filesScroll->setWidgetResizable(true);
  filesScroll->setFrameShape(QFrame::NoFrame);
  filesScroll->setMinimumHeight(0);
  m_filesPanel = new KaFileBrowserPanel(filesScroll);
  m_filesPanel->setObjectName(QStringLiteral("studioFilesPanel"));
  m_filesPanel->setMinimumHeight(0);
  filesScroll->setWidget(m_filesPanel);
  connect(filesToggle, &QToolButton::toggled, filesScroll, &QWidget::setVisible);
  connect(m_filesPanel, &KaFileBrowserPanel::fileActivated, this, [this, mainWin](const QString& path) {
    if (mainWin) {
      const bool raster = GeorefService::isImagePath(path);
      if (raster ? mainWin->addRasterFromPath(path) : mainWin->addVectorFromPath(path)) {
        syncMapFromLayers();
      }
    }
  });
  connect(m_filesPanel, &KaFileBrowserPanel::statusMessage, this, [this](const QString& msg) {
    if (m_status) showStatus(msg);
  });
  leftSplit->addWidget(filesScroll);

  leftSplit->setStretchFactor(0, 3);
  leftSplit->setStretchFactor(1, 2);
  leftSplit->setSizes({340, 240});
  leftLay->addWidget(leftSplit, 1);

  auto* right = new QWidget(root);
  auto* rightLay = new QVBoxLayout(right);
  rightLay->setContentsMargins(0, 0, 0, 0);
  rightLay->setSpacing(0);

  m_adjustBar = new QFrame(right);
  m_adjustBar->setObjectName(QStringLiteral("mapAdjustBar"));
  auto* barLay = new QHBoxLayout(m_adjustBar);
  barLay->setContentsMargins(12, 8, 12, 8);
  auto* barTxt = new QLabel(
      QStringLiteral("지도 조정 중 — 드래그로 이동, 휠로 확대. 끝나면 「조정끝」."),
      m_adjustBar);
  auto* btnCenter = new QPushButton(QStringLiteral("레이어를 가운데"), m_adjustBar);
  auto* btnDone = new QPushButton(QStringLiteral("조정끝"), m_adjustBar);
  btnDone->setObjectName(QStringLiteral("btnAdjustDone"));
  connect(btnCenter, &QPushButton::clicked, this, &KaDrawingStudio::centerSurveyInMap);
  connect(btnDone, &QPushButton::clicked, this, &KaDrawingStudio::endActivateMap);
  barLay->addWidget(barTxt, 1);
  barLay->addWidget(btnCenter);
  barLay->addWidget(btnDone);
  m_adjustBar->hide();

  m_view = new QgsLayoutView(right);
  m_view->setObjectName(QStringLiteral("layoutView"));
  KaTheme::excludeMapSurface(m_view);
  m_view->setFrameShape(QFrame::NoFrame);
  m_view->setBackgroundBrush(QBrush(KaTheme::tokens().desk));
  m_view->setFocusPolicy(Qt::StrongFocus);
  m_view->setAlignment(Qt::AlignCenter);
  m_view->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
  m_view->setResizeAnchor(QGraphicsView::AnchorViewCenter);
  m_view->setDragMode(QGraphicsView::NoDrag);
  m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_view->setMenuProvider(new KaLayoutMenuProvider(this));
  m_view->setAcceptDrops(true);
  m_view->installEventFilter(this);
  if (m_view->viewport()) {
    m_view->viewport()->setAcceptDrops(true);
    m_view->viewport()->installEventFilter(this);
  }

  m_toolSelect = new QgsLayoutViewToolSelect(m_view);
  m_toolMoveContent = new KaLayoutMapAdjustTool(m_view);
  m_toolCoordPoint = new KaLayoutCoordPointTool(m_view);
  connect(m_toolCoordPoint, &KaLayoutCoordPointTool::pointClicked, this,
          &KaDrawingStudio::placeCoordCallout);
  connect(m_toolCoordPoint, &KaLayoutCoordPointTool::undoRequested, this,
          &KaDrawingStudio::undoLastCoordCallout);
  connect(m_toolMoveContent, &KaLayoutMapAdjustTool::mapViewChanged, this,
          &KaDrawingStudio::syncScaleDecorations);
  attachLayoutToView();

  m_scaleSyncTimer = new QTimer(this);
  m_scaleSyncTimer->setSingleShot(true);
  m_scaleSyncTimer->setInterval(280);
  connect(m_scaleSyncTimer, &QTimer::timeout, this, &KaDrawingStudio::flushHeavyScaleSync);
  m_layerSyncTimer = new QTimer(this);
  m_layerSyncTimer->setSingleShot(true);
  m_layerSyncTimer->setInterval(80);
  connect(m_layerSyncTimer, &QTimer::timeout, this, &KaDrawingStudio::flushLayerSync);

  auto* side = new QFrame(root);
  side->setObjectName(QStringLiteral("studioLeftCol"));
  side->setMinimumWidth(260);
  side->setMaximumWidth(300);
  auto* sideLay = new QVBoxLayout(side);
  sideLay->setContentsMargins(12, 8, 12, 8);
  sideLay->setSpacing(8);
  sideLay->setSizeConstraint(QLayout::SetMinimumSize);

  m_cardLegend = new QFrame(side);
  m_cardLegend->setObjectName(QStringLiteral("itemInspector"));
  // Keep each card's complete contents visible. Theme minimums alone may be
  // smaller than a button's text/icon size after Qt repolishes the widgets.
  m_cardLegend->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
  auto* legendLay = new QVBoxLayout(m_cardLegend);
  legendLay->setContentsMargins(10, 10, 10, 10);
  legendLay->setSpacing(6);
  auto* legendCap = new QLabel(QStringLiteral("도면 항목"), m_cardLegend);
  legendCap->setObjectName(QStringLiteral("cardCaption"));
  legendLay->addWidget(legendCap);
  // 이 카드는 용지에 올릴 범례와, 그 옆의 PDF·인쇄를 둔다.
  auto* legendRow = new QHBoxLayout;
  legendRow->setSpacing(8);
  auto* legendBtn = makeRailTile(m_cardLegend, KaIcons::icon(QStringLiteral("layout_legend")),
                                 QStringLiteral("범례"), QSize(KaTheme::buttonMetrics().layoutIconSize, KaTheme::buttonMetrics().layoutIconSize));
  legendBtn->setObjectName(QStringLiteral("legendTile"));
  m_legendTile = legendBtn;
  legendBtn->setToolTip(QStringLiteral("범례를 용지에 넣습니다. 넣은 뒤 끌어 옮깁니다"));
  legendBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  legendBtn->setFixedSize(legendBtn->sizeHint());
  connect(legendBtn, &QToolButton::clicked, this, [this]() {
    beginPlaceLegend();
    if (m_cardLegend) m_cardLegend->setFocus();
  });
  auto makeOutput = [this](const QString& iconId, const QString& text, const QColor& ink) {
    auto* b = new QToolButton(m_cardLegend);
    b->setIcon(KaIcons::icon(iconId, ink));
    b->setIconSize(QSize(20, 20));
    b->setText(text);
    b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    b->setCursor(Qt::PointingHandCursor);
    return b;
  };
  auto* pdfBtn = makeOutput(QStringLiteral("pdf"), QStringLiteral("PDF 내보내기"), KaTheme::tokens().surface);
  pdfBtn->setObjectName(QStringLiteral("btnPrimary"));
  pdfBtn->setToolTip(QStringLiteral("지금 용지를 PDF 파일로 저장합니다"));
  connect(pdfBtn, &QToolButton::clicked, this, &KaDrawingStudio::savePdf);
  auto* printBtn = makeOutput(QStringLiteral("print"), QStringLiteral("인쇄"), KaTheme::tokens().ink);
  printBtn->setObjectName(QStringLiteral("btnPrint"));
  printBtn->setToolTip(QStringLiteral("프린터로 찍거나, 큰 도면을 작은 용지 여러 장으로 나눠 찍습니다 (Ctrl+P)"));
  // Ctrl+P 는 창 전체의 「인쇄」 동작이 받는다. 여기에 또 두면 같은 키가 둘이라 둘 다 안 먹는다.
  connect(printBtn, &QToolButton::clicked, this, &KaDrawingStudio::printDrawing);
  // Optional title block. The default sheet stays map frame only.
  auto* titleBtn = makeRailTile(m_cardLegend, KaIcons::icon(QStringLiteral("new")),
                                QStringLiteral("표제란"), QSize(KaTheme::buttonMetrics().layoutIconSize, KaTheme::buttonMetrics().layoutIconSize));
  titleBtn->setObjectName(QStringLiteral("btnTitleBlock"));
  m_titleTile = titleBtn;
  titleBtn->setToolTip(QStringLiteral("도면명·조사명·유적명·축척·작성일 상자를 넣습니다. 조사구역의 조사명·유적명을 씁니다. 필요할 때만 넣습니다"));
  titleBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  titleBtn->setFixedSize(titleBtn->sizeHint());
  connect(titleBtn, &QToolButton::clicked, this, &KaDrawingStudio::placeTitleBlock);
  legendRow->addWidget(legendBtn);
  legendRow->addWidget(titleBtn);
  legendRow->addWidget(pdfBtn, 1);
  legendRow->addWidget(printBtn, 1);
  legendLay->addLayout(legendRow);
  m_legendTitle = new QLineEdit(m_cardLegend);
  m_legendTitle->setPlaceholderText(QStringLiteral("제목을 입력하세요"));
  m_legendTitle->setText(QStringLiteral("범례"));
  connect(m_legendTitle, &QLineEdit::textChanged, this, &KaDrawingStudio::applyLegendSettings);
  legendLay->addWidget(new QLabel(QStringLiteral("범례 제목"), m_cardLegend));
  legendLay->addWidget(m_legendTitle);
  auto* fontRow = new QHBoxLayout;
  m_legendFont = new QSpinBox(m_cardLegend);
  m_legendFont->setRange(5, 24);
  m_legendFont->setValue(10);
  m_legendFont->setSuffix(QStringLiteral(" pt"));
  connect(m_legendFont, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &KaDrawingStudio::applyLegendSettings);
  m_legendBold = new QCheckBox(QStringLiteral("B"), m_cardLegend);
  m_legendBold->setChecked(true);
  m_legendItalic = new QCheckBox(QStringLiteral("I"), m_cardLegend);
  connect(m_legendBold, &QCheckBox::toggled, this, &KaDrawingStudio::applyLegendSettings);
  connect(m_legendItalic, &QCheckBox::toggled, this, &KaDrawingStudio::applyLegendSettings);
  fontRow->addWidget(new QLabel(QStringLiteral("글자"), m_cardLegend));
  fontRow->addWidget(m_legendFont, 1);
  fontRow->addWidget(m_legendBold);
  fontRow->addWidget(m_legendItalic);
  legendLay->addLayout(fontRow);
  m_heritageNames = new QCheckBox(QStringLiteral("1:5,000 이하에서 유적 이름으로 표시"), m_cardLegend);
  m_heritageNames->setObjectName(QStringLiteral("heritageNamesCheck"));
  m_heritageNames->setToolTip(QStringLiteral(
      "켜면 1:5,000과 더 크게 본 축척에서 주변유적 번호 대신 유적 이름을 쓰고, 범례에는 유적 종류만 남깁니다"));
  if (auto* ly = layout())
    m_heritageNames->setChecked(ly->customProperty(HeritageLayoutNumbers::namesPropertyKey(), false).toBool());
  connect(m_heritageNames, &QCheckBox::toggled, this, [this](bool on) {
    auto* ly = layout();
    if (!ly || ly->customProperty(HeritageLayoutNumbers::namesPropertyKey(), false).toBool() == on) return;
    ly->setCustomProperty(HeritageLayoutNumbers::namesPropertyKey(), on);
    markUserComposed();
    if (m_project) m_project->setDirty(true);
    syncHeritageNumbers(true);
  });
  legendLay->addWidget(m_heritageNames);
  sideLay->addWidget(m_cardLegend);

  m_cardNorth = new QFrame(side);
  m_cardNorth->setObjectName(QStringLiteral("itemInspector"));
  m_cardNorth->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
  auto* northLay = new QVBoxLayout(m_cardNorth);
  northLay->setContentsMargins(10, 10, 10, 10);
  northLay->setSpacing(6);
  auto* northCap = new QLabel(QStringLiteral("방위 (도북)"), m_cardNorth);
  northCap->setObjectName(QStringLiteral("cardCaption"));
  northCap->setToolTip(KaDrawingNorth::gridNorthNote());
  northLay->addWidget(northCap);
  auto* northRow = new QHBoxLayout;
  northRow->setSpacing(6);
  struct NorthSample { const char* rel; const char* tip; int art; };
  const NorthSample norths[] = {
      {"", "북 글자", 0},
      {"arrows/NorthArrow_02.svg", "북 화살", 1},
      {"arrows/NorthArrow_04.svg", "나침반", 2},
      {"wind_roses/WindRose_01.svg", "바람장미", 3},
  };
  for (const auto& ns : norths) {
    auto* b = makeRailTile(m_cardNorth, KaDrawingNorth::previewIcon(ns.art), QString::fromUtf8(ns.tip),
                           QSize(KaTheme::buttonMetrics().layoutIconSize, KaTheme::buttonMetrics().layoutIconSize));
    b->setObjectName(QStringLiteral("northSwatch"));
    b->setToolTip(QString::fromUtf8(ns.tip) + QStringLiteral(" — ") + KaDrawingNorth::gridNorthNote());
    const QString rel = QString::fromUtf8(ns.rel);
    b->setProperty("northRel", rel);
    connect(b, &QToolButton::clicked, this, [this, rel]() { beginPlaceNorth(rel); });
    northRow->addWidget(b);
  }
  northLay->addLayout(northRow);
  auto* northSizeRow = new QHBoxLayout;
  northSizeRow->addWidget(new QLabel(QStringLiteral("크기"), m_cardNorth));
  m_northSize = new QDoubleSpinBox(m_cardNorth);
  m_northSize->setRange(3.0, 80.0);
  m_northSize->setDecimals(0);
  m_northSize->setSuffix(QStringLiteral(" mm"));
  m_northSize->setValue(28.0);
  connect(m_northSize, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double mm) {
    auto* it = findItemById(layout(), kIdNorth);
    if (!it) return;
    it->beginCommand(QStringLiteral("방위표 크기"));
    QRectF r(it->pos(), it->rect().size());
    r.setWidth(mm);
    r.setHeight(mm);
    it->attemptSetSceneRect(r);
    it->endCommand();
    it->update();
  });
  northSizeRow->addWidget(m_northSize, 1);
  northLay->addLayout(northSizeRow);
  sideLay->addWidget(m_cardNorth);

  m_scaleBar = new QFrame(side);
  m_scaleBar->setObjectName(QStringLiteral("itemInspector"));
  m_scaleBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
  const auto& buttonMetrics = KaTheme::buttonMetrics();
  auto* scaleLay = new QVBoxLayout(m_scaleBar);
  scaleLay->setContentsMargins(buttonMetrics.panelMargin, buttonMetrics.panelMargin,
                              buttonMetrics.panelMargin, buttonMetrics.panelMargin);
  scaleLay->setSpacing(0);
  // Distribute the available panel height between rows instead of leaving it
  // below the card. MinimumExpanding keeps the minimum gap on short screens.
  const auto addScaleRowGap = [scaleLay, &buttonMetrics]() {
    scaleLay->addSpacerItem(new QSpacerItem(0, buttonMetrics.panelMargin,
        QSizePolicy::Minimum, QSizePolicy::MinimumExpanding));
  };
  auto* scaleCap = new QLabel(QStringLiteral("도면 정보"), m_scaleBar);
  scaleCap->setObjectName(QStringLiteral("cardCaption"));
  scaleLay->addWidget(scaleCap);
  m_scaleSpin = new QSpinBox(m_scaleBar);
  m_scaleSpin->setRange(10, 5000000);
  m_scaleSpin->setSingleStep(10);
  m_scaleSpin->setValue(1000);
  m_scaleSpin->setKeyboardTracking(false);
  m_scaleSpin->setGroupSeparatorShown(false);
  auto* applySc = new QPushButton(QStringLiteral("적용"), m_scaleBar);
  applySc->setObjectName(QStringLiteral("scaleApply"));
  applySc->ensurePolished();
  applySc->setFixedHeight(buttonMetrics.scaleButtonHeight);
  m_scaleSpin->setObjectName(QStringLiteral("drawingScale"));
  m_scaleSpin->ensurePolished();
  m_scaleSpin->setFixedHeight(buttonMetrics.scaleButtonHeight);
  connect(applySc, &QPushButton::clicked, this, &KaDrawingStudio::applyOnScreenScale);
  connect(m_scaleSpin, &QSpinBox::editingFinished, this, &KaDrawingStudio::applyOnScreenScale);
  auto* scaleTop = new QHBoxLayout;
  scaleTop->setSpacing(buttonMetrics.buttonSpacing);
  scaleTop->addWidget(new QLabel(QStringLiteral("1 :"), m_scaleBar));
  scaleTop->addWidget(m_scaleSpin, 1);
  scaleTop->addWidget(applySc);
  addScaleRowGap();
  scaleLay->addLayout(scaleTop);
  m_scaleProps = m_scaleBar;
  auto addChipRow = [&](const int* vals, int count) {
    auto* row = new QHBoxLayout;
    row->setSpacing(KaDrawingStudioPlate::chipGap(buttonMetrics.buttonSpacing));  // 새 모양: one joined table
    for (int i = 0; i < count; ++i) {
      const int n = vals[i];
      auto* chip = new QToolButton(m_scaleBar);
      chip->setObjectName(QStringLiteral("scaleChip"));
      chip->setText(QString::number(n));
      chip->setToolButtonStyle(Qt::ToolButtonTextOnly);
      chip->setCheckable(true);
      chip->setCursor(Qt::PointingHandCursor);
      chip->ensurePolished();
      chip->setMinimumWidth(buttonMetrics.scaleButtonMinWidth);
      chip->setFixedHeight(buttonMetrics.scaleButtonHeight);
      chip->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
      chip->setProperty("denom", n);
      connect(chip, &QToolButton::clicked, this, [this, n]() {
        if (m_scaleSpin) m_scaleSpin->setValue(n);
        applyOnScreenScale();
      });
      row->addWidget(chip, 1);
    }
    // Keep a partial last row aligned with the three preset columns above it.
    for (int i = count; i < KaDrawingStudioPlate::chipsPerRow(); ++i) {
      auto* spacer = new QWidget(m_scaleBar);
      spacer->setMinimumWidth(buttonMetrics.scaleButtonMinWidth);
      spacer->setFixedHeight(buttonMetrics.scaleButtonHeight);
      spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
      row->addWidget(spacer, 1);
    }
    addScaleRowGap();
    scaleLay->addLayout(row);
  };
  // One shared scale table (StandardScales): excavation 1:20·1:30·1:50 first,
  // then the user's presets up to 1:25,000, three per row.
  const QList<int> chipScales = StandardScales::denominators(StandardScales::StudioChip);
  for (qsizetype first = 0; first < chipScales.size(); first += KaDrawingStudioPlate::chipsPerRow()) {  // 새 모양: 7
    const int count = static_cast<int>(std::min<qsizetype>(KaDrawingStudioPlate::chipsPerRow(), chipScales.size() - first));
    addChipRow(chipScales.constData() + first, count);
  }
  auto* barRow = new QHBoxLayout;
  barRow->setSpacing(buttonMetrics.buttonSpacing);
  struct BarSample { const char* style; const char* tip; };
  const BarSample bars[] = {
      {"Double Box", "쌍칸"},
      {"Single Box", "외칸"},
      {"Line Ticks Up", "눈금"},
  };
  for (const auto& bs : bars) {
    auto* b = makeRailTile(m_scaleBar, scaleBarPreviewIcon(bs.style), QString::fromUtf8(bs.tip),
                           QSize(buttonMetrics.ribbonIconSize, buttonMetrics.layoutIconSize));
    const QString style = QString::fromUtf8(bs.style);
    connect(b, &QToolButton::clicked, this, [this, style]() { beginPlaceScaleBar(style); });
    barRow->addWidget(b);
  }
  addScaleRowGap();
  scaleLay->addLayout(barRow);
  auto* sizeRow = new QHBoxLayout;
  sizeRow->addWidget(new QLabel(QStringLiteral("선택 항목 크기"), m_scaleBar));
  m_decorationSize = new QDoubleSpinBox(m_scaleBar);
  m_decorationSize->setObjectName(QStringLiteral("decorationSize"));
  m_decorationSize->setRange(25., 200.);
  m_decorationSize->setDecimals(0);
  m_decorationSize->setSingleStep(10.);
  m_decorationSize->setSuffix(QStringLiteral(" %"));
  m_decorationSize->setValue(100.);
  m_decorationSize->setKeyboardTracking(false);
  m_decorationSize->setEnabled(false);
  m_decorationSize->setToolTip(QStringLiteral("용지의 축척자·축척·좌표계·방위표를 선택해 크기를 조절합니다. 위치는 끌어서 옮깁니다."));
  connect(m_decorationSize, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &KaDrawingStudio::resizeSelectedDecoration);
  sizeRow->addWidget(m_decorationSize, 1);
  addScaleRowGap();
  scaleLay->addLayout(sizeRow);
  auto* extraRow = new QHBoxLayout;
  extraRow->setSpacing(buttonMetrics.buttonSpacing);
  auto* scaleLblBtn = makeRailTile(m_scaleBar, KaIcons::icon(QStringLiteral("layout_scale")),
                                   QStringLiteral("축척 글자"), QSize(buttonMetrics.layoutIconSize, buttonMetrics.layoutIconSize));
  connect(scaleLblBtn, &QToolButton::clicked, this, &KaDrawingStudio::beginPlaceScaleLabel);
  extraRow->addWidget(scaleLblBtn);
  auto* crsBtn = makeRailTile(m_scaleBar, KaIcons::icon(QStringLiteral("crs")),
                              QStringLiteral("좌표계"), QSize(buttonMetrics.layoutIconSize, buttonMetrics.layoutIconSize));
  connect(crsBtn, &QToolButton::clicked, this, &KaDrawingStudio::beginPlaceCrsLabel);
  extraRow->addWidget(crsBtn);
  m_gridBtn = makeRailTile(m_scaleBar, KaIcons::icon(QStringLiteral("map_grid")),
                           QStringLiteral("좌표 격자"), QSize(buttonMetrics.layoutIconSize, buttonMetrics.layoutIconSize));
  m_gridBtn->setObjectName(QStringLiteral("btnGridSettings"));
  m_gridBtn->setCheckable(true);
  connect(m_gridBtn, &QToolButton::clicked, this, [this]() {
    focusGridSettings();
    syncGridButton();
  });
  extraRow->addWidget(m_gridBtn);
  syncGridButton();
  extraRow->addStretch(1);
  addScaleRowGap();
  scaleLay->addLayout(extraRow);
  sideLay->addWidget(m_scaleBar, 1);

  auto* desk = new QWidget(right);
  auto* deskGrid = new QGridLayout(desk);
  deskGrid->setContentsMargins(0, 0, 0, 0);
  deskGrid->setSpacing(0);
  deskGrid->addWidget(m_view, 0, 0);
  deskGrid->addWidget(m_adjustBar, 0, 0, Qt::AlignTop);
  m_adjustBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  {
    // 투명도 막대는 캔버스 위에 붙박이로 떠 있어야 한다.
    // QgsLayoutView 는 QGraphicsView 라서 페이지를 끌면 scrollContentsBy 가
    // viewport()->scroll() 을 부르고, 그러면 viewport 의 자식 위젯까지 같이 밀린다.
    // 지도(QgsMapCanvas)는 스크롤 없이 범위를 다시 그려서 이 문제가 없었다.
    // 스크롤하지 않는 desk 에 붙여 페이지를 움직여도 제자리에 남게 한다.
    m_opacityRail = new KaLayerOpacityRail(desk, QStringLiteral("drawing"));
  }
  connect(m_opacityRail, &KaLayerOpacityRail::brightnessChanged, this, [this](int value) {
    // 막대를 켤 때 잡아 둔 레이어를 쓴다. 조판에서는 용지를 클릭하는 사이에
    // 트리의 현재 항목이 달라질 수 있어, 그때마다 조용히 아무 일도 안 했다.
    QgsMapLayer* cur = m_railLayer.data();
    if (!LayerOps::canAdjustBrightness(cur) && m_layerTree)
      cur = m_layerTree->currentLayer();
    if (!LayerOps::canAdjustBrightness(cur)) {
      if (m_status)
        showStatus(QStringLiteral("밝기를 바꿀 그림 레이어를 왼쪽에서 먼저 고르세요."));
      return;
    }
    LayerOps::setMapLayerBrightness(cur, value, m_mapCanvas);
    // 조판 그림은 캐시로 그린다. 다시 그리라고 확실히 시켜야 바뀐 게 보인다.
    if (auto* map = mapItem()) {
      map->invalidateCache();
      map->refresh();
    }
    syncMapFromLayers();
    if (m_view && m_view->viewport())
      m_view->viewport()->update();
    if (m_status)
      showStatus(QStringLiteral("「%1」 밝기 %2").arg(cur->name()).arg(value));
  });
  connect(m_opacityRail, &KaLayerOpacityRail::percentChanged, this, [this](int value) {
    QgsMapLayer* cur = m_railLayer.data();
    if (!LayerOps::canAdjustOpacity(cur) && m_layerTree)  // [pkg E1] F018
      cur = m_layerTree->currentLayer();
    if (!LayerOps::canAdjustOpacity(cur)) return;
    LayerOps::applyLayerOpacity(cur, value / 100.0, m_mapCanvas);
    syncMapFromLayers();
    if (auto* mainWin = qobject_cast<MainWindow*>(window()))
      mainWin->updateLayerOpacityControl();
  });
  updateLayerOpacityControl();
  rightLay->addWidget(desk, 1);

  deskGrid->addWidget(KaDrawingStudioPlate::bottomStrip(buildBottomTools(desk)), 0, 0, Qt::AlignHCenter | Qt::AlignBottom);
  if (m_opacityRail)
    m_opacityRail->raise();
  side->setMinimumWidth(260);
  side->setMaximumWidth(QWIDGETSIZE_MAX);
  auto* inspectorScroll = new QScrollArea(root);
  inspectorScroll->setObjectName(QStringLiteral("drawingInspectorScroll"));
  inspectorScroll->setFrameShape(QFrame::NoFrame);
  inspectorScroll->setWidgetResizable(true);
  inspectorScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  inspectorScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  inspectorScroll->setWidget(side);
  side->ensurePolished();
  if (auto* lay = side->layout())
    lay->activate();
  // Reserve the scrollbar width as well, so the north-arrow and PDF row are
  // never clipped at the narrowest splitter position.
  const int inspectorMinWidth = std::max(side->sizeHint().width(), side->minimumSizeHint().width()) +
      inspectorScroll->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
  inspectorScroll->setMinimumWidth(std::max(260, inspectorMinWidth));
  inspectorScroll->setMaximumWidth(std::max(420, inspectorMinWidth));

  m_studioSplit = new QSplitter(Qt::Horizontal, root);
  m_studioSplit->setObjectName(QStringLiteral("studioMainSplit"));
  KaMapPagePlate::followHandle(m_studioSplit, 10);
  m_studioSplit->setChildrenCollapsible(false);
  m_studioSplit->addWidget(leftCol);
  m_studioSplit->addWidget(right);
  m_studioSplit->addWidget(KaDrawingStudioPlate::inspectorColumn(inspectorScroll));  // 새 모양: PDF·인쇄 at the foot
  m_studioSplit->setStretchFactor(0, 0);
  m_studioSplit->setStretchFactor(1, 1);
  m_studioSplit->setStretchFactor(2, 0);
  m_studioSplit->setSizes({348, 820, 320});
  rootLay->addWidget(m_studioSplit, 1);
  setCentralWidget(root);

  // 안내는 메인 창 상태줄 한 줄로 보낸다. 탭 안에 상태줄을 또 두면 두 줄이 겹쳐 보였다.
  // m_status 는 마지막 안내를 기억하는 보이지 않는 칸이다.
  m_status = new QLabel(this);
  m_status->hide();
  showStatus(QStringLiteral(
      "도면 만드는 중 — 항목을 끌어 옮기고, 끝나면 「PDF 내보내기」. 작업 좌표계 → 제출 5179."));
  m_paperFitPending = true;
}

QWidget* KaDrawingStudio::buildBottomTools(QWidget* desk) {
  auto* bottomTools = new QWidget(desk);
  bottomTools->setObjectName(QStringLiteral("layoutBottomTools"));
  bottomTools->setAttribute(Qt::WA_StyledBackground, true);
  auto* btLay = new QHBoxLayout(bottomTools);
  btLay->setContentsMargins(14, 6, 14, 6);
  btLay->setSpacing(22);
  auto addBottom = [this, bottomTools](const QString& iconId, const QString& text,
                                       const QString& tip, auto slot) {
    auto* b = new QToolButton(bottomTools);
    b->setIcon(KaIcons::icon(iconId));
    b->setText(text);
    b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    b->setIconSize(QSize(22, 22));
    b->setToolTip(tip);
    b->setAutoRaise(true);
    connect(b, &QToolButton::clicked, this, slot);
    return b;
  };
  btLay->addWidget(addBottom(QStringLiteral("layout_coord_point"), QStringLiteral("좌표점"),
                             QStringLiteral("용지에서 꼭짓점을 찍으면 화살표와 X·Y가 붙습니다"),
                             &KaDrawingStudio::beginPlaceCoordPoint));
  btLay->addWidget(addBottom(QStringLiteral("layout_center"), QStringLiteral("레이어를 가운데"),
                             QStringLiteral("축척은 두고, 고른 레이어를 용지 한가운데로 옮깁니다"),
                             &KaDrawingStudio::centerSurveyInMap));
  // Same actions as the map frame's right-click menu, now visible without it.
  m_adjustBtn = addBottom(QStringLiteral("layout_activate"), QStringLiteral("지도 조정"),
                          QStringLiteral("지도 칸 안의 지도를 끌어 옮기고 휠로 확대합니다. 다시 누르면 끝냅니다"),
                          &KaDrawingStudio::toggleMapAdjust);
  m_adjustBtn->setObjectName(QStringLiteral("btnMapAdjust"));
  btLay->addWidget(m_adjustBtn);
  btLay->addWidget(addBottom(QStringLiteral("map"), QStringLiteral("화면 맞춤"),
                             QStringLiteral("지도 화면과 같은 범위·축척으로 지도 칸을 다시 맞춥니다"),
                             &KaDrawingStudio::fitToMapCanvas));
  btLay->addWidget(addBottom(QStringLiteral("layout_select"),
                             KaBeginnerRibbon::twoLine(QStringLiteral("항목 이동")),
                             QStringLiteral("좌표 상자를 끌어 옮깁니다"),
                             &KaDrawingStudio::useSelectTool));
  btLay->addWidget(addBottom(QStringLiteral("layout_map_frame"), QStringLiteral("용지/방향"),
                             QStringLiteral("용지 크기(A4/A3) 및 방향(가로/세로)을 전환합니다"),
                             &KaDrawingStudio::openPaperSettingsDialog));
  // Drawing set: store the sheet under a name and reopen it later.
  auto* sheets = new QToolButton(bottomTools);
  sheets->setObjectName(QStringLiteral("btnSheetSet"));
  sheets->setIcon(KaIcons::icon(QStringLiteral("open")));
  sheets->setText(QStringLiteral("도면 목록"));
  sheets->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  sheets->setIconSize(QSize(22, 22));
  sheets->setAutoRaise(true);
  sheets->setPopupMode(QToolButton::InstantPopup);
  sheets->setToolTip(QStringLiteral("지금 도면을 이름 붙여 보관하고, 보관한 도면을 다시 엽니다. 제출 PDF는 지금 열린 도면입니다"));
  auto* sheetMenu = new QMenu(sheets);
  sheets->setMenu(sheetMenu);
  connect(sheetMenu, &QMenu::aboutToShow, this, [this, sheetMenu]() {
    KaDrawingSheetSet::MenuActions actions;
    actions.store = [this]() { storeCurrentSheet(); };
    actions.restore = [this](const QString& title) { restoreStoredSheet(title); };
    actions.remove = [this](const QString& title) { removeStoredSheet(title); };
    KaDrawingSheetSet::fillMenu(sheetMenu, m_project, actions);
  });
  btLay->addWidget(sheets);
  bottomTools->raise();
  return bottomTools;
}

void KaDrawingStudio::showStatus(const QString& text) {
  if (m_status)
    m_status->setText(text);
  emit statusMessage(text);
}

QRectF KaDrawingStudio::defaultMapRect() const {
  // 위·좌·우는 1cm만 남긴다. 아래만 축척막대·좌표계·방위표 자리로 넓게 둔다.
  const double edge = kFieldEdgeMm;
  const double bottom = kFieldBottomChromeMm;
  return QRectF(edge, edge, std::max(40.0, m_paperW - edge * 2.0),
                std::max(40.0, m_paperH - edge - bottom));
}

void KaDrawingStudio::autoPlaceDefaultSheet() {
  createOrResizeMap(defaultMapRect());
  zoomPaperVisible();
}

void KaDrawingStudio::refreshMapFromProject() {
  syncMapFromLayers();
}

void KaDrawingStudio::placeTerrain3dPicture(const QString& pngPath, const QgsRectangle& groundExtent,
                                            const QgsCoordinateReferenceSystem& crs) {
  auto* ly = layout();
  if (!ly || pngPath.isEmpty() || !QFile::exists(pngPath))
    return;
  m_holdTerrainExtent = true;
  m_terrainGroundExtent = groundExtent;
  m_terrainCrs = crs;
  QgsLayoutItemMap* map = mapItem();
  QRectF mapR(12.0, 12.0, m_paperW - 24.0, m_paperH - 36.0);
  if (map) {
    const QRectF b = itemPaperRect(map);
    if (b.width() > 8.0 && b.height() > 8.0)
      mapR = b;
    if (crs.isValid())
      map->setCrs(crs);
    if (!groundExtent.isEmpty() && groundExtent.isFinite())
      map->zoomToExtent(groundExtent);
  }
  if (auto* old = findItemById(ly, kIdTerrain3d))
    ly->removeLayoutItem(old);
  auto* pic = new QgsLayoutItemPicture(ly);
  pic->setId(QString::fromUtf8(kIdTerrain3d));
  pic->setPicturePath(pngPath, Qgis::PictureFormat::Raster);
  pic->setMode(Qgis::PictureFormat::Raster);
  pic->setResizeMode(QgsLayoutItemPicture::Zoom);
  pic->setFrameEnabled(false);
  pic->attemptSetSceneRect(mapR);
  pic->refreshPicture();
  ly->addLayoutItem(pic);
  if (map)
    pic->setZValue(map->zValue() + 1.0);
  if (auto* legend = findItemById(ly, kIdLegend))
    legend->setZValue(pic->zValue() + 1.0);
  ensureStandardDecorations();
  applyCrsLabelNow();
}

void KaDrawingStudio::importMapCoordCallouts(const QVector<QgsPointXY>& pts,
                                             const QVector<QString>& letters,
                                             const QVector<QString>& texts,
                                             const QgsGeometry& frame) {
  Q_UNUSED(frame);
  m_importCoordPts = pts;
  m_importCoordLetters = letters;
  m_importCoordTexts = texts;
  QTimer::singleShot(0, this, [this]() { applyImportedCoordCallouts(); });
  QTimer::singleShot(80, this, [this]() { applyImportedCoordCallouts(); });
}

void KaDrawingStudio::applyImportedCoordCallouts() {
  auto* ly = layout();
  auto* map = mapItem();
  const QVector<QgsPointXY>& pts = m_importCoordPts;
  const QVector<QString>& letters = m_importCoordLetters;
  const QVector<QString>& texts = m_importCoordTexts;
  if (!ly || !map || pts.isEmpty()) return;
  KaDrawingCoordCallout::removeAll(ly);
  m_coordMapPts.clear();
  m_coordFrameMap.clear();
  const QRectF ir = map->rect();
  const QgsRectangle ext = map->extent();
  if (ir.width() < 1.0 || ext.isEmpty() || !ext.isFinite()) return;
  QVector<QRectF> usedBoxes;
  for (int i = 0; i < pts.size(); ++i) {
    const QgsPointXY mapPt = pts.at(i);
    const QPointF tip = map->mapToScene(LayoutService::layoutMapItemFromXy(ext, ir, mapPt.x(), mapPt.y()));
    const QString tag = i < letters.size() ? letters.at(i) : KaDrawingCoordCallout::tagFor(i);
    const QString text = i < texts.size() ? texts.at(i)
                                          : KaDrawingCoordCallout::xyText(mapPt.x(), mapPt.y());
    const QRectF boxR = KaDrawingCoordCallout::freeBoxRect(tip, usedBoxes);
    usedBoxes.append(boxR);
    KaDrawingCoordCallout::add(ly, tip, tag, text, boxR);
    m_coordMapPts.append(QPointF(mapPt.x(), mapPt.y()));
  }
  if (m_status)
    showStatus(QStringLiteral("맵에서 찍은 좌표점 %1개를 도면에 올렸습니다.").arg(pts.size()));
}

namespace {
// The map's metre coordinate grid when it is on and north-up (a turned grid cannot go on a sheet grid).
const KaCanvasGridOverlay* northUpMapGrid(QgsMapCanvas* canvas) {
  if (!canvas) return nullptr;
  for (QGraphicsItem* item : canvas->items()) {
    const auto* grid = dynamic_cast<const KaCanvasGridOverlay*>(item);
    if (grid && grid->isEnabled() && grid->config().type == KaCanvasGridOverlay::Type::ProjectedMeters &&
        std::abs(std::remainder(grid->config().rotationDeg, 360.0)) < 1e-9)
      return grid;
  }
  return nullptr;
}
}  // namespace

void KaDrawingStudio::focusGridSettings() {
  QDialog dlg(this);
  dlg.setWindowTitle(QStringLiteral("격자 설정"));
  dlg.setMinimumWidth(280);
  auto* root = new QVBoxLayout(&dlg);
  root->setContentsMargins(14, 12, 14, 12);
  root->setSpacing(8);
  auto* on = new QCheckBox(QStringLiteral("좌표 격자"), &dlg);
  on->setChecked(m_gridEnabled);
  on->setToolTip(QStringLiteral("끄면 도면 안 +와 바깥 테두리 좌표 자가 사라집니다."));
  auto* nums = new QCheckBox(QStringLiteral("칸 밖에 좌표 숫자"), &dlg);
  nums->setChecked(m_gridShowNums);
  auto* step = new QDoubleSpinBox(&dlg);
  step->setRange(0.0, 100000.0);
  step->setDecimals(0);
  step->setSingleStep(10.0);
  step->setSuffix(QStringLiteral(" m"));
  step->setSpecialValueText(QStringLiteral("자동(축척에 맞춤)"));
  step->setValue(m_gridIntervalM);
  step->setKeyboardTracking(false);
  // 지도 좌표격자와 같게 (사용자 요청 2026-10-20): the grid the 시굴격자 is laid on, on the sheet too.
  const KaCanvasGridOverlay* mapGrid = northUpMapGrid(m_mapCanvas);
  auto* follow = new QCheckBox(mapGrid ? QStringLiteral("지도 좌표격자와 같게 (%1 m)").arg(mapGrid->stepMeters(), 0, 'g', 6)
                                       : QStringLiteral("지도 좌표격자와 같게"), &dlg);
  follow->setObjectName(QStringLiteral("gridFollowMap"));
  follow->setEnabled(mapGrid != nullptr);
  follow->setChecked(m_gridFollowMap && mapGrid != nullptr);
  follow->setToolTip(mapGrid ? QStringLiteral("지도 화면의 좌표격자와 같은 간격·같은 선으로 도면에 격자선을 긋습니다.")
                             : QStringLiteral("지도에 미터 좌표격자를 켜면 쓸 수 있습니다(회전한 격자는 안 됩니다)."));
  step->setEnabled(!follow->isChecked());
  auto apply = [this, on, nums, step, follow]() {
    step->setEnabled(!follow->isChecked());
    setGridOptions(on->isChecked(), step->value(), nums->isChecked(), follow->isChecked());
    if (m_status)
      showStatus(!m_gridEnabled ? QStringLiteral("좌표 격자를 껐습니다.")
                 : m_gridIntervalM > 0.0
                     ? QStringLiteral("좌표 격자 간격 %1 m").arg(m_gridIntervalM, 0, 'f', 0)
                     : QStringLiteral("좌표 격자 간격: 자동(축척에 맞춤)"));
  };
  connect(on, &QCheckBox::toggled, &dlg, [apply](bool) { apply(); });
  connect(nums, &QCheckBox::toggled, &dlg, [apply](bool) { apply(); });
  connect(follow, &QCheckBox::toggled, &dlg, [apply](bool) { apply(); });
  connect(step, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg,
          [apply](double) { apply(); });
  connect(step, &QDoubleSpinBox::editingFinished, &dlg, [apply]() { apply(); });
  root->addWidget(on);
  auto* stepRow = new QHBoxLayout;
  stepRow->addWidget(new QLabel(QStringLiteral("간격"), &dlg));
  stepRow->addWidget(step, 1);
  root->addLayout(stepRow);
  root->addWidget(follow);
  root->addWidget(nums);
  auto* box = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
  box->button(QDialogButtonBox::Close)->setText(QStringLiteral("닫기"));
  connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  root->addWidget(box);
  dlg.exec();
  syncGridButton();
}

namespace {
const QString kGridEnabledProperty = QStringLiteral("ka_hgis/grid_enabled");
const QString kGridIntervalProperty = QStringLiteral("ka_hgis/grid_interval_m");
const QString kGridNumbersProperty = QStringLiteral("ka_hgis/grid_numbers");
const QString kGridFollowMapProperty = QStringLiteral("ka_hgis/grid_follow_map");
}  // namespace

void KaDrawingStudio::setGridOptions(bool enabled, double intervalM, bool showNumbers, bool followMap) {
  const bool changed = enabled != m_gridEnabled || showNumbers != m_gridShowNums || followMap != m_gridFollowMap ||
                       std::abs(intervalM - m_gridIntervalM) > 1e-9;
  m_gridEnabled = enabled;
  m_gridFollowMap = followMap;
  m_gridShowNums = showNumbers;
  m_gridIntervalM = std::max(0.0, intervalM);
  // Remember the choice on the sheet so reopening or exporting keeps it.
  if (auto* ly = layout()) {
    ly->setCustomProperty(kGridEnabledProperty, m_gridEnabled);
    ly->setCustomProperty(kGridIntervalProperty, m_gridIntervalM);
    ly->setCustomProperty(kGridNumbersProperty, m_gridShowNums);
    ly->setCustomProperty(kGridFollowMapProperty, m_gridFollowMap);
  }
  if (auto* map = mapItem())
    applyCrsGrid(map);
  syncGridButton();
  if (changed) {
    markUserComposed();
    if (m_project) m_project->setDirty(true);
  }
}

void KaDrawingStudio::loadGridSettings() {
  // Default stays off; a sheet only shows the grid after the user turned it on.
  auto* ly = layout();
  m_gridEnabled = ly && ly->customProperty(kGridEnabledProperty, false).toBool();
  m_gridShowNums = ly && ly->customProperty(kGridNumbersProperty, false).toBool();
  m_gridIntervalM = ly ? std::max(0.0, ly->customProperty(kGridIntervalProperty, 0.0).toDouble()) : 0.0;
  m_gridFollowMap = ly && ly->customProperty(kGridFollowMapProperty, false).toBool();
  syncGridButton();
}

void KaDrawingStudio::syncGridButton() {
  if (!m_gridBtn) return;
  const QSignalBlocker block(m_gridBtn);
  m_gridBtn->setChecked(m_gridEnabled);
  m_gridBtn->setToolTip(
      m_gridEnabled
          ? QStringLiteral("좌표 격자 켜짐 (%1). 눌러서 끄거나 간격을 바꿉니다.")
                .arg(m_gridIntervalM > 0.0 ? QStringLiteral("%1 m").arg(m_gridIntervalM, 0, 'f', 0)
                                           : QStringLiteral("간격 자동"))
          : QStringLiteral("지도 안 +와 테두리 좌표 자를 켭니다. 기본은 꺼져 있습니다."));
}

void KaDrawingStudio::syncSheetTiles() {
  // 새 모양에서만: 용지에 있는 범례·표제란은 귀리색, 지금 방위는 귀리색에 먹 테두리.
  const bool paper = KaTheme::displayOptions().paperLook;
  auto* ly = layout();
  auto mark = [](QToolButton* b, bool on) {
    if (!b) return;
    const bool blocked = b->blockSignals(true);
    b->setCheckable(on || b->isChecked());
    b->setChecked(on);
    if (!on) b->setCheckable(false);
    b->blockSignals(blocked);
  };
  if (!paper) {
    mark(m_legendTile, false);
    mark(m_titleTile, false);
  } else {
    mark(m_legendTile, ly && findItemById(ly, kIdLegend));
    mark(m_titleTile, ly && ly->itemById(KaTitleBlock::itemId()));
  }
  QString chosenRel;
  QgsLayoutItem* north = paper && ly ? findItemById(ly, kIdNorth) : nullptr;
  if (north) {
    const int kind = KaDrawingNorth::generatedKind(north);
    if (kind >= 0)
      chosenRel = KaDrawingNorth::relForKind(kind);
    else if (!dynamic_cast<QgsLayoutItemLabel*>(north))
      chosenRel = m_pendingNorthSvg;
  }
  if (!m_cardNorth) return;
  const auto buttons = m_cardNorth->findChildren<QToolButton*>(QStringLiteral("northSwatch"));
  for (QToolButton* b : buttons)
    mark(b, north && b->property("northRel").toString() == chosenRel);
}

void KaDrawingStudio::beginPlaceLegend() {
  m_legendRemoved = false;  // 사용자가 다시 넣겠다는 뜻
  endActivateMap();
  m_placeKind = PlaceKind::Legend;
  applyLegendSettings();
  m_placeUndo.append(QString::fromUtf8(kIdLegend));
  markUserComposed();
  finishPlace();
  if (m_status)
    showStatus(QStringLiteral("범례를 넣었습니다. 끌어 옮기세요."));
}

void KaDrawingStudio::beginPlaceNorth(const QString& svgRel) {
  endActivateMap();
  m_pendingNorthSvg = svgRel;
  m_placeKind = PlaceKind::North;
  applyNorthNow();
  m_placeUndo.append(QString::fromUtf8(kIdNorth));
  markUserComposed();
  if (m_status)
    showStatus(QStringLiteral("방위표를 넣었습니다. 끌어 옮기세요."));
}

void KaDrawingStudio::beginPlaceScaleBar(const QString& style) {
  endActivateMap();
  m_pendingScaleBarStyle = style;
  m_placeKind = PlaceKind::ScaleBar;
  applyScaleBarNow();
  m_placeUndo.append(QString::fromUtf8(kIdScaleBar));
  markUserComposed();
  if (m_status)
    showStatus(QStringLiteral("축척자를 넣었습니다. 끌어 옮기세요."));
}

void KaDrawingStudio::beginPlaceScaleLabel() {
  endActivateMap();
  m_placeKind = PlaceKind::ScaleLabel;
  auto* item = findItemById(layout(), kIdScale);
  placeScaleLabel(item ? QRectF(item->pos(), item->rect().size()) : defaultItemRect(kIdScale));
  m_placeUndo.append(QString::fromUtf8(kIdScale));
  markUserComposed();
  if (m_status)
    showStatus(QStringLiteral("축척 글자를 넣었습니다. 끌어 옮기세요."));
}

void KaDrawingStudio::beginPlaceCrsLabel() {
  endActivateMap();
  m_placeKind = PlaceKind::CrsLabel;
  applyCrsLabelNow();
  m_placeUndo.append(QString::fromUtf8(kIdCrs));
  markUserComposed();
  finishPlace();
  if (m_status)
    showStatus(QStringLiteral("좌표계를 넣었습니다. 끌어 옮기세요."));
}

void KaDrawingStudio::beginPlaceCoordPoint() {
  endActivateMap();
  if (!m_view || !m_toolCoordPoint || !mapItem()) {
    if (m_status)
      showStatus(QStringLiteral("먼저 지도 칸을 그리세요."));
    return;
  }
  // The callout magnet below searches the sheet's own layers. The project's
  // snapping setup belongs to map editing and is left as the user set it.
  m_view->setTool(m_toolCoordPoint);
  if (m_status)
    showStatus(QStringLiteral(
        "지도 칸에서 찍으세요. 선·점에 자석이 붙습니다. 우클릭·Delete·Ctrl+Z는 마지막 점 지우기."));
}

bool KaDrawingStudio::isPlacingCoordPoint() const {
  return m_view && m_toolCoordPoint && m_view->tool() == m_toolCoordPoint;
}

void KaDrawingStudio::endPlaceCoordPoint() {
  useSelectTool();
  if (m_status)
    showStatus(QStringLiteral("좌표점 찍기를 끝냈습니다."));
}

void KaDrawingStudio::placeCoordCallout(const QPointF& layoutPt) {
  auto* ly = layout();
  auto* map = mapItem();
  if (!ly || !map) return;
  const QPointF itemPt = map->mapFromScene(layoutPt);
  const QRectF ir = map->rect();
  if (ir.width() < 1.0 || ir.height() < 1.0) return;
  if (!LayoutService::layoutMapItemContains(itemPt, ir, 0.8)) {
    if (m_status)
      showStatus(QStringLiteral("지도 칸 안에서 찍으세요. 흰 용지에는 점이 안 생깁니다."));
    return;
  }
  const QgsRectangle ext = map->extent();
  if (ext.isEmpty() || !ext.isFinite()) return;

  const QgsPointXY click = LayoutService::layoutMapXyFromItem(ext, ir, itemPt);
  QgsPointXY mapPt = click;
  const double viewPx = m_view ? std::max(1e-9, m_view->transform().m11()) : 1.0;
  const double snapMm = 18.0 / viewPx;
  const double tol = std::max(ext.width() * snapMm / std::max(1.0, ir.width()),
                              ext.height() * snapMm / std::max(1.0, ir.height()));
  bool snapped = false;
  const QgsCoordinateReferenceSystem mapCrs = map->crs();
  QgsProject* proj = m_project ? m_project.data() : QgsProject::instance();
  double bestD = tol;
  // Snap only to what the sheet shows: layers on the map item and its
  // above-labels overlay that are checked in the layer list.
  QList<QgsMapLayer*> search = map->layers();
  if (auto* above = dynamic_cast<QgsLayoutItemMap*>(findItemById(ly, kIdMapAbove))) {
    for (QgsMapLayer* l : above->layers()) {
      if (l && !search.contains(l))
        search.append(l);
    }
  }
  QgsLayerTree* tree = proj ? proj->layerTreeRoot() : nullptr;
  for (QgsMapLayer* raw : search) {
    auto* vl = qobject_cast<QgsVectorLayer*>(raw);
    if (!vl || !vl->isValid() || vl->geometryType() == Qgis::GeometryType::Null)
      continue;
    if (LayerOps::isReferenceLayer(vl) || LayerOps::isCadastralLayer(vl))
      continue;
    QgsLayerTreeLayer* node = tree ? tree->findLayer(vl->id()) : nullptr;
    if (!node || !node->isVisible())
      continue;
    QgsRectangle req(click.x() - tol, click.y() - tol, click.x() + tol, click.y() + tol);
    if (vl->crs().isValid() && mapCrs.isValid() && vl->crs() != mapCrs && proj) {
      try {
        const QgsCoordinateTransform xf(mapCrs, vl->crs(), proj->transformContext());
        req = xf.transformBoundingBox(req);
      } catch (...) {
        KA_LOG_EXCEPT();
        continue;
      }
    }
    QgsFeatureIterator it =
        vl->getFeatures(QgsFeatureRequest().setFilterRect(req).setNoAttributes());
    QgsFeature f;
    while (it.nextFeature(f)) {
      QgsGeometry g = f.geometry();
      if (g.isEmpty()) continue;
      if (vl->crs().isValid() && mapCrs.isValid() && vl->crs() != mapCrs && proj) {
        try {
          const QgsCoordinateTransform xf(vl->crs(), mapCrs, proj->transformContext());
          g.transform(xf);
        } catch (...) {
          KA_LOG_EXCEPT();
          continue;
        }
      }
      int atV = 0, before = 0, after = 0;
      double vSqr = -1.0;
      const QgsPointXY v = g.closestVertex(click, atV, before, after, vSqr);
      if (vSqr >= 0.0) {
        const double d = std::sqrt(vSqr);
        if (d < bestD) {
          bestD = d;
          mapPt = v;
          snapped = true;
        }
      }
      QgsPointXY onSeg;
      int afterSeg = -1;
      const double sSqr = g.closestSegmentWithContext(click, onSeg, afterSeg);
      if (sSqr >= 0.0) {
        const double d = std::sqrt(sSqr);
        if (d < bestD) {
          bestD = d;
          mapPt = onSeg;
          snapped = true;
        }
      }
    }
  }

  const QPointF itemSnap = LayoutService::layoutMapItemFromXy(ext, ir, mapPt.x(), mapPt.y());
  const QPointF tip = map->mapToScene(itemSnap);

  int n = 0;
  QList<QgsLayoutItemLabel*> labs;
  ly->layoutItems(labs);
  for (QgsLayoutItemLabel* l : labs) {
    if (l && l->id().startsWith(QLatin1String("ka_coord_box_")))
      ++n;
  }
  const QString tag = KaDrawingCoordCallout::tagFor(n);
  KaDrawingCoordCallout::add(ly, tip, tag, KaDrawingCoordCallout::xyText(mapPt.x(), mapPt.y()),
                             KaDrawingCoordCallout::clickBoxRect(tip, m_paperW));
  markUserComposed();
  m_placeUndo.append(QStringLiteral("ka_coord_%1").arg(tag));
  m_coordMapPts.append(QPointF(mapPt.x(), mapPt.y()));
  updateCoordFrame();
  if (m_status)
    showStatus(QStringLiteral("%1점  X=%2  Y=%3%4")
                          .arg(tag)
                          .arg(mapPt.y(), 0, 'f', 3)
                          .arg(mapPt.x(), 0, 'f', 3)
                          .arg(snapped ? QStringLiteral("  (자석)") : QString()));
}

void KaDrawingStudio::undoLastCoordCallout() {
  if (m_coordMapPts.isEmpty()) {
    if (m_status)
      showStatus(QStringLiteral("지울 좌표점이 없습니다."));
    return;
  }
  m_coordMapPts.removeLast();
  while (!m_placeUndo.isEmpty() && m_placeUndo.last().startsWith(QLatin1String("ka_coord_")))
    m_placeUndo.removeLast();
  relayoutCoordCallouts();
  if (m_status)
    showStatus(m_coordMapPts.isEmpty()
                          ? QStringLiteral("좌표점을 모두 지웠습니다.")
                          : QStringLiteral("마지막 좌표점을 지웠습니다. 남은 점 %1개.")
                                .arg(m_coordMapPts.size()));
}

void KaDrawingStudio::relayoutCoordCallouts() {
  if (m_coordMapPts.isEmpty()) {
    KaDrawingCoordCallout::removeAll(layout());
    return;
  }
  m_importCoordPts.clear();
  m_importCoordLetters.clear();
  m_importCoordTexts.clear();
  for (int i = 0; i < m_coordMapPts.size(); ++i) {
    const QPointF p = m_coordMapPts.at(i);
    m_importCoordPts.append(QgsPointXY(p.x(), p.y()));
    m_importCoordLetters.append(KaDrawingCoordCallout::tagFor(i));
    m_importCoordTexts.append(KaDrawingCoordCallout::xyText(p.x(), p.y()));
  }
  applyImportedCoordCallouts();
  updateCoordFrame();
}

void KaDrawingStudio::updateCoordFrame() {
  auto* ly = layout();
  auto* map = mapItem();
  if (!ly || !map) return;
  const QgsRectangle ext = map->extent();
  const QRectF ir = map->rect();
  if (ext.isEmpty() || ir.width() < 1.0 || ir.height() < 1.0) return;

  QVector<QPointF> src = m_coordFrameMap;
  if (src.size() < 3 && m_coordMapPts.size() >= 2)
    src = m_coordMapPts;
  if (src.size() < 2) return;

  auto toLayout = [&](double mx, double my) {
    return map->mapToScene(LayoutService::layoutMapItemFromXy(ext, ir, mx, my));
  };
  QPolygonF ring;
  for (const QPointF& p : src)
    ring << toLayout(p.x(), p.y());
  if (ring.size() >= 2 && ring.first() == ring.last())
    ring.removeLast();
  if (ring.size() < 2) return;

  QgsLayoutItemPolygon* frame = nullptr;
  QList<QgsLayoutItemPolygon*> polys;
  ly->layoutItems(polys);
  for (QgsLayoutItemPolygon* p : polys) {
    if (p && p->id() == QLatin1String("ka_coord_frame")) {
      frame = p;
      break;
    }
  }
  if (!frame) {
    frame = new QgsLayoutItemPolygon(ring, ly);
    frame->setId(QStringLiteral("ka_coord_frame"));
    if (auto fs = QgsFillSymbol::createSimple(
            {{QStringLiteral("color"), QStringLiteral("255,255,255,0")},
             {QStringLiteral("style"), QStringLiteral("no")},
             {QStringLiteral("outline_color"), QStringLiteral("#C2410C")},
             {QStringLiteral("outline_width"), QStringLiteral("0.45")},
             {QStringLiteral("outline_width_unit"), QStringLiteral("MM")}})) {
      frame->setSymbol(fs.get());
    }
    ly->addLayoutItem(frame);
    m_placeUndo.append(frame->id());
  } else {
    frame->setNodes(ring);
  }
}

void KaDrawingStudio::useSelectTool() {
  if (m_adjustingMap) return;
  if (m_view && m_toolSelect)
    m_view->setTool(m_toolSelect);
  if (m_status)
    showStatus(QStringLiteral("항목을 눌러 옮기거나, 모서리를 끌어 가로·세로를 바꾸세요."));
}

void KaDrawingStudio::recenterPaper() {
  auto* ly = layout();
  if (!m_view || !ly) return;
  ensureSheetPage();
  if (!m_view->viewport() || m_view->viewport()->width() < 32 || m_view->viewport()->height() < 32) {
    m_paperFitPending = true;
    return;
  }
  QgsLayoutItemPage* page = ly->pageCollection() ? ly->pageCollection()->page(0) : nullptr;
  QRectF pr = page ? page->rect() : QRectF();
  if (page)
    pr = page->mapRectToScene(page->rect());
  if (pr.width() < 8.0 || pr.height() < 8.0)
    pr = QRectF(0, 0, m_paperW, m_paperH);
  m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_view->setAlignment(Qt::AlignCenter);
  m_view->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
  m_view->setResizeAnchor(QGraphicsView::AnchorViewCenter);
  const QRectF desk(pr.left() - pr.width() * 1.5, pr.top() - pr.height() * 1.5,
                    pr.width() * 4.0, pr.height() * 4.0);
  m_view->setSceneRect(desk);
  const qreal pad = std::max(pr.width(), pr.height()) * 0.10;
  m_view->fitInView(pr.adjusted(-pad, -pad, pad, pad), Qt::KeepAspectRatio);
  m_view->centerOn(pr.center());
  if (m_view->viewport())
    m_lastFitViewport = m_view->viewport()->size();
}

void KaDrawingStudio::zoomPaperVisible() {
  m_keepPaperCentered = true;
  recenterPaper();
  QTimer::singleShot(0, this, [this]() {
    if (m_keepPaperCentered) recenterPaper();
  });
  QTimer::singleShot(80, this, [this]() {
    if (m_keepPaperCentered) recenterPaper();
  });
}

void KaDrawingStudio::panLayoutView(const QPoint& from, const QPoint& to) {
  if (!m_view) return;
  const QPoint d = to - from;
  bool moved = false;
  if (auto* h = m_view->horizontalScrollBar()) {
    const int before = h->value();
    h->setValue(before - d.x());
    moved = moved || (h->value() != before);
  }
  if (auto* v = m_view->verticalScrollBar()) {
    const int before = v->value();
    v->setValue(before - d.y());
    moved = moved || (v->value() != before);
  }
  if (moved) return;
  const QTransform t = m_view->transform();
  const qreal sx = t.m11();
  const qreal sy = t.m22();
  if (qAbs(sx) < 1e-12 || qAbs(sy) < 1e-12) return;
  m_view->setTransformationAnchor(QGraphicsView::NoAnchor);
  m_view->translate(static_cast<qreal>(d.x()) / sx, static_cast<qreal>(d.y()) / sy);
}

void KaDrawingStudio::clearGridCoordinateLabels() {
  auto* ly = layout();
  if (!ly) return;
  QList<QgsLayoutItemLabel*> labels;
  ly->layoutItems(labels);
  for (QgsLayoutItemLabel* lbl : labels) {
    if (lbl && lbl->id().startsWith(QLatin1String(kGridAnnPrefix)))
      ly->removeLayoutItem(lbl);
  }
}

// (구버전 수제 좌표 라벨은 폐기 — 도곽 주기는 LayoutService::applySurveyFrameGrid의
//  QGIS 내장 주기가 그린다. clearGridCoordinateLabels는 옛 도면 정리용으로 유지.)

void KaDrawingStudio::applyCrsGrid(QgsLayoutItemMap* map) {
  if (!map || !map->grids()) return;
  // 구버전 수제 좌표 라벨 정리 — 도곽 주기는 QGIS 내장(정수 TM, 세로쓰기)을 쓴다.
  clearGridCoordinateLabels();
  if (!m_gridEnabled) {
    QStringList ids;
    const QList<QgsLayoutItemMapGrid*> olds = map->grids()->asList();
    for (QgsLayoutItemMapGrid* old : olds) {
      if (!old) continue;
      old->setEnabled(false);
      if (!old->id().isEmpty()) ids.append(old->id());
      if (!old->name().isEmpty()) ids.append(old->name());
    }
    ids.removeDuplicates();
    for (const QString& id : ids)
      map->grids()->removeGrid(id);
    map->updateBoundingRect();
    map->invalidateCache();
    map->refresh();
    map->update();
    if (auto* ly = layout()) ly->refresh();
    return;
  }
  const QgsCoordinateReferenceSystem crs = map->crs().isValid()
      ? map->crs()
      : studioMapCrs(m_mapCanvas, m_project);
  if (crs.isValid() && !map->crs().isValid())
    map->setCrs(crs);
  // 간격 0 = 자동: 축척·지도폭에 맞는 1-2-5 계열(종이에서 25~70mm).
  double interval = m_gridIntervalM;
  if (!(interval > 0.0)) {
    double mapWmm = 160.0;
    if (auto* ly = layout())
      mapWmm =
          ly->convertFromLayoutUnits(map->rect().width(), Qgis::LayoutUnit::Millimeters).length();
    const double denom = map->scale() > 0.0
                             ? map->scale()
                             : (m_scaleSpin ? static_cast<double>(m_scaleSpin->value()) : 1000.0);
    interval = LayoutService::niceGridIntervalMeters(denom, mapWmm);
  }
  const KaCanvasGridOverlay* mapGrid = m_gridFollowMap ? northUpMapGrid(m_mapCanvas) : nullptr;
  if (mapGrid)
    LayoutService::applyMapCoordinateGrid(map, mapGrid->stepMeters(), mapGrid->config().originX,
                                          mapGrid->config().originY, m_gridShowNums);
  else
    LayoutService::applySurveyFrameGrid(map, interval, true, m_gridShowNums);
  map->invalidateCache();
  map->refresh();
  map->update();
  if (auto* ly = layout())
    ly->refresh();
}

void KaDrawingStudio::showEvent(QShowEvent* event) {
  QMainWindow::showEvent(event);
  for (auto* button : findChildren<QToolButton*>()) {
    if (button->property("class").toString() != QLatin1String("sampleTile")) continue;
    button->ensurePolished();
    button->setMinimumWidth(std::max(button->minimumWidth(), button->sizeHint().width()));
    if (button->height() < button->sizeHint().height())
      button->setFixedHeight(button->sizeHint().height());
  }
  if (auto* scroll = findChild<QScrollArea*>(QStringLiteral("drawingInspectorScroll"))) {
    if (auto* side = scroll->widget()) {
      if (auto* lay = side->layout()) lay->activate();
      const int need = std::max(side->sizeHint().width(), side->minimumSizeHint().width())
          + scroll->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
      scroll->setMinimumWidth(std::max({260, scroll->minimumWidth(), need}));
    }
  }
  syncMapFromLayers();
  rebuildNorthIfNeeded();
  ensureSheetPage();
  const bool viewReady = m_view && m_view->viewport() && m_view->viewport()->width() >= 32
      && m_view->viewport()->height() >= 32;
  if (m_paperFitPending || !sheetPageInView()) {
    if (viewReady) {
      m_paperFitPending = false;
      QTimer::singleShot(0, this, [this]() { zoomPaperVisible(); });
    } else {
      m_paperFitPending = true;
    }
  }
}

void KaDrawingStudio::finishPlace() {
  QTimer::singleShot(0, this, [this]() {
    if (!m_adjustingMap)
      useSelectTool();
    selectPlacedItem();
  });
}

void KaDrawingStudio::selectPlacedItem() {
  auto* ly = layout();
  if (!ly) return;
  const char* id = kIdMap;
  switch (m_placeKind) {
    case PlaceKind::MapFrame:
      id = kIdMap;
      break;
    case PlaceKind::Legend:
      id = kIdLegend;
      break;
    case PlaceKind::North:
      id = kIdNorth;
      break;
    case PlaceKind::ScaleBar:
      id = kIdScaleBar;
      break;
    case PlaceKind::ScaleLabel:
      id = kIdScale;
      break;
    case PlaceKind::CrsLabel:
      id = kIdCrs;
      break;
    case PlaceKind::TitleBlock:
      if (auto* block = ly->itemById(KaTitleBlock::itemId()))
        ly->setSelectedItem(block);
      return;
  }
  if (auto* it = findItemById(ly, id))
    ly->setSelectedItem(it);
}

QgsVectorLayer* KaDrawingStudio::blankMapLayer() {
  if (!m_blankMapLayer) {
    m_blankMapLayer = new QgsVectorLayer(
        QStringLiteral("Polygon?crs=EPSG:5186"),
        QStringLiteral("layout_blank"),
        QStringLiteral("memory"));
    if (m_blankMapLayer)
      m_blankMapLayer->setParent(this);
  }
  return m_blankMapLayer;
}

void KaDrawingStudio::applyLayersToMap(QgsLayoutItemMap* map, bool includeLiveBasemap, bool refitExtent) {
  if (!map || !m_project) return;
  // 같은 화면을 다시 맞출 때마다 서명을 지우면 덧지도를 다시 만들고,
  // 그 과정에서 이미 찍힌 범례 노드가 사라진다.
  LayerOps::knockOutProjectRasterPaper(m_project);
  // 조판도 지도와 같은 규칙을 따라야 한다. 밑에 있는 레이어의 글자는 밑으로.
  // 도면을 그릴 때마다 다시 계산해, 지도에서만 먹고 조판에서는 안 먹는 일이 없게 한다.
  LayerOps::applyLayerOrderToLabels(m_project, nullptr);
  QList<QgsMapLayer*> layers = LayerOps::sheetBasePaintLayers(m_project);
  // Heritage polygons are painted again above labels, so the base list omits them.
  // The sheet map still has to list a checked heritage layer.
  if (auto* root = m_project->layerTreeRoot()) {
    for (auto* node : root->findLayers()) {
      if (!node || !node->isVisible() || !node->layer() || layers.contains(node->layer())) continue;
      auto* parent = node->parent();
      if (parent && HeritageStyle::fromLayerName(parent->name()))
        layers.append(node->layer());
    }
  }
  if (!includeLiveBasemap) {
    QList<QgsMapLayer*> safe;
    for (QgsMapLayer* ml : layers) {
      if (ml && !isLiveBasemapLayer(ml))
        safe.append(ml);
    }
    layers = safe;
  }
  if (layers.isEmpty()) {
    if (auto* blank = blankMapLayer())
      layers.append(blank);
  }
  map->setKeepLayerSet(true);
  map->setFollowVisibilityPreset(false);
  if (map->layers() != layers) map->setLayers(layers);
  const QgsCoordinateReferenceSystem crs = studioMapCrs(m_mapCanvas, m_project);
  if (crs.isValid() && crs != map->crs())
    map->setCrs(crs);
  // Layer sync alone does not make this a composed sheet (see markUserComposed).
  if (refitExtent) {
    const QgsRectangle ext = studioMapExtent(m_mapCanvas, crs);
    if (!ext.isNull() && ext.isFinite() && ext.width() > 0.0)
      map->zoomToExtent(ext);
  }
  // 본지도에서 덧그림을 뺀 뒤 같은 호출에서 덧지도를 맞춘다.
  // 번호 갱신을 기다리면 조사·지적·유적이 조판에서 빠진다.
  syncAboveLabelsMap(map);
}

// 조판·PDF 용 덧지도.
//
// QGIS 는 한 번의 렌더에서 도형을 모두 그린 뒤 라벨을 맨 위에 얹는다. 그래서
// 위 레이어의 경계선이 아래 지적도의 지번 글자에 가린다. 레이어의
// rendering/renderAboveLabels 는 병렬 렌더러가 이미지를 합칠 때만 쓰이고,
// 조판은 QgsMapRendererCustomPainterJob 경로라 무시된다.
//
// 그래서 본 지도와 똑같은 범위·크기의 지도 항목을 하나 더 올리고, 거기에는
// 그 레이어들만 담아 배경 없이 그린다. 미리보기와 PDF 가 같은 경로를 타므로
// 두 결과가 항상 같다.
void KaDrawingStudio::syncAboveLabelsMap(QgsLayoutItemMap* base) {
  auto* ly = layout();
  if (!ly || !base || !m_project) return;
  const QList<QgsMapLayer*> above = LayerOps::layersDrawnAboveLabels(m_project);
  auto* top = dynamic_cast<QgsLayoutItemMap*>(findItemById(ly, kIdMapAbove));

  if (above.isEmpty()) {
    m_aboveMapDigest.clear();
    if (top) ly->removeLayoutItem(top);
    return;
  }

  QByteArray digest;
  QDataStream digestStream(&digest, QIODevice::WriteOnly);
  const QgsRectangle ext = base->extent();
  // Exact doubles drift between two refreshes of the same sheet. Rewriting the
  // overlay then makes QGIS replace the legend nodes.
  auto whole = [](double value) {
    return std::isfinite(value) ? std::llround(value) : 0LL;
  };
  const QRectF paper = base->rect();
  const QPointF origin = base->pos();
  digestStream << whole(ext.xMinimum()) << whole(ext.yMinimum())
               << whole(ext.xMaximum()) << whole(ext.yMaximum())
               << whole(base->scale()) << base->crs().authid() << whole(base->mapRotation())
               << whole(paper.x()) << whole(paper.y()) << whole(paper.width()) << whole(paper.height())
               << whole(origin.x()) << whole(origin.y());
  const auto styles = base->layerStyleOverrides();
  for (auto* layer : above) {
    digestStream << (layer ? layer->id() : QString());
    if (layer) digestStream << styles.value(layer->id());
  }
  if (top && digest == m_aboveMapDigest)
    return;
  m_aboveMapDigest = digest;

  const bool created = (top == nullptr);
  if (created) {
    top = new QgsLayoutItemMap(ly);
    top->setId(QString::fromUtf8(kIdMapAbove));
    ly->addLayoutItem(top);
  }
  top->setFrameEnabled(false);
  top->setBackgroundEnabled(false);
  top->setKeepLayerSet(true);
  top->setFollowVisibilityPreset(false);
  top->setLayers(above);
  top->setKeepLayerStyles(true);
  auto overlayStyles = base->layerStyleOverrides();
  for (auto* layer : above) {
    if (!qobject_cast<QgsVectorLayer*>(layer)) continue;
    QgsMapLayerStyle style;
    if (overlayStyles.contains(layer->id())) style = QgsMapLayerStyle(overlayStyles.value(layer->id()));
    else style.readFromLayer(layer);
    QDomDocument document;
    if (!document.setContent(style.xmlData())) continue;
    // The overlay draws geometry only. Number labels live on ka_map_numbers
    // so PAL runs once, not on the base map or this overlay.
    document.documentElement().setAttribute(QStringLiteral("labelsEnabled"), QStringLiteral("0"));
    overlayStyles.insert(layer->id(), document.toString());
  }
  top->setLayerStyleOverrides(overlayStyles);
  top->setCrs(base->crs());
  top->setMapRotation(base->mapRotation());
  top->attemptSetSceneRect(base->rect().translated(base->pos()));
  top->zoomToExtent(base->extent());
  top->setExtent(base->extent());
  if (created) {
    // 본 지도 바로 위. 범례·축척자 같은 장식은 계속 그 위에 남아야 한다.
    ly->moveItemToBottom(top, true);
    ly->raiseItem(top, true);
    ly->updateZValues(false);
  }
  top->invalidateCache();
  top->refresh();
}

void KaDrawingStudio::createOrResizeMap(const QRectF& layoutRect) {
  auto* ly = layout();
  if (!ly || !m_view || m_view->currentLayout() != ly) return;

  const QgsCoordinateReferenceSystem crs = studioMapCrs(m_mapCanvas, m_project);
  const QgsRectangle ext = studioMapExtent(m_mapCanvas, crs);
  if (ext.isNull() || !ext.isFinite() || ext.width() <= 0.0)
    return;

  QgsLayoutItemMap* map = mapItem();
  const bool created = !map;
  const QRectF page(0.0, 0.0, m_paperW, m_paperH);
  const auto chrome = LayoutService::standardSheetChrome(page, layoutRect);
  if (created)
    m_placeUndo.append(QString::fromUtf8(kIdMap));
  if (!map) {
    map = new QgsLayoutItemMap(ly);
    map->setId(QString::fromUtf8(kIdMap));
    map->setFrameEnabled(true);
    map->attemptSetSceneRect(chrome.map);
    map->setCrs(crs);
    map->setKeepLayerSet(true);
    applyLayersToMap(map, false, true);
    map->zoomToExtent(ext);
    map->setCacheMode(QGraphicsItem::ItemCoordinateCache);
    if (map->scene() != ly)
      ly->addLayoutItem(map);
  } else {
    map->attemptSetSceneRect(chrome.map);
    map->setCrs(crs);
    map->zoomToExtent(ext);
    map->setCacheMode(QGraphicsItem::ItemCoordinateCache);
  }

  QPointer<QgsLayoutItemMap> held(map);
  QTimer::singleShot(0, this, [this, held]() {
    if (!held) return;
    applyLayersToMap(held, true, false);
    applyCrsGrid(held);
    if (m_holdTerrainExtent && !m_terrainGroundExtent.isEmpty() &&
        m_terrainGroundExtent.isFinite()) {
      if (m_terrainCrs.isValid())
        held->setCrs(m_terrainCrs);
      held->zoomToExtent(m_terrainGroundExtent);
    } else if (m_mapCanvas)
      centerOnMapCanvas();
    else
      snapMapScaleToNice();
    connectMapSignals(held);
    ensureStandardDecorations();
    refreshScaleWidgets(true);
    useSelectTool();
    if (auto* ly = layout())
      ly->setSelectedItem(held);
    if (m_status)
      showStatus(QStringLiteral(
          "지도 칸을 만들었습니다. 나침반·좌표계·축척자·축척이 들어갔습니다."));
  });
}

void KaDrawingStudio::connectMapSignals(QgsLayoutItemMap* map) {
  if (!map) return;
  connect(map, &QgsLayoutItemMap::extentChanged, this, &KaDrawingStudio::syncScaleDecorations,
          Qt::UniqueConnection);
  connect(map, &QgsLayoutItemMap::mapRotationChanged, this, &KaDrawingStudio::syncScaleDecorations,
          Qt::UniqueConnection);
}

QRectF KaDrawingStudio::defaultItemRect(const char* id) const {
  QRectF mapR(12.0, 12.0, m_paperW - 24.0, m_paperH - 24.0);
  if (auto* map = mapItem()) {
    const QRectF b = itemPaperRect(map);
    if (b.width() > 8.0 && b.height() > 8.0)
      mapR = b;
  }
  if (qstrcmp(id, kIdLegend) == 0)
    return QRectF(mapR.right() - 52.0, mapR.top() + 6.0, 48.0, 74.0);
  const auto chrome =
      LayoutService::standardSheetChrome(QRectF(0.0, 0.0, m_paperW, m_paperH), mapR);
  if (qstrcmp(id, kIdNorth) == 0)
    return chrome.north;
  if (qstrcmp(id, kIdScaleBar) == 0)
    return chrome.scaleBar;
  if (qstrcmp(id, kIdScale) == 0)
    return chrome.scaleLabel;
  if (qstrcmp(id, kIdCrs) == 0)
    return chrome.crs;
  return QRectF(20.0, 20.0, 40.0, 30.0);
}

void KaDrawingStudio::applyLegendSettings() {
  auto* ly = layout();
  if (!ly) {
    syncSheetTiles();
    return;
  }
  auto* legend = dynamic_cast<QgsLayoutItemLegend*>(findItemById(ly, kIdLegend));
  const bool created = !legend;
  if (!legend) {
    // 사용자가 지운 범례는 글자 설정을 만졌다고 되살아나면 안 된다.
    if (m_legendRemoved) return;  // deleteSelectedItems already synced the 범례 tile
    legend = new QgsLayoutItemLegend(ly);
    legend->setId(QString::fromUtf8(kIdLegend));
    legend->setFrameEnabled(true);
    legend->setBackgroundEnabled(true);
    legend->setCacheMode(QGraphicsItem::DeviceCoordinateCache);
    legend->setResizeToContents(false);
    if (auto* map = mapItem())
      legend->setLinkedMap(map);
    legend->attemptSetSceneRect(defaultItemRect(kIdLegend));
    ly->addLayoutItem(legend);
  }
  legend->setResizeToContents(false);
  legend->setTitle(m_legendTitle ? m_legendTitle->text() : QStringLiteral("범례"));
  const int pt = m_legendFont ? m_legendFont->value() : 10;
  const int weight = (m_legendBold && m_legendBold->isChecked()) ? QFont::Bold : QFont::Normal;
  QFont titleFont(QStringLiteral("Malgun Gothic"), pt, weight);
  if (m_legendItalic) titleFont.setItalic(m_legendItalic->isChecked());
  QFont bodyFont(QStringLiteral("Malgun Gothic"), pt, QFont::Normal);
  if (m_legendItalic) bodyFont.setItalic(m_legendItalic->isChecked());
  legend->setStyleFont(Qgis::LegendComponent::Title, titleFont);
  legend->setStyleFont(Qgis::LegendComponent::Group, bodyFont);
  legend->setStyleFont(Qgis::LegendComponent::Subgroup, bodyFont);
  legend->setStyleFont(Qgis::LegendComponent::SymbolLabel,
                       QFont(QStringLiteral("Malgun Gothic"), qMax(5, pt - 1)));
  if (auto* map = mapItem()) {
    legend->setLinkedMap(map);
  }
  if (created) {
    LayoutService::tuneSheetLegend(legend);
    m_heritageNumbers.applyLegend(legend);
  } else {
    // Font/title edits only affect presentation, not map features or numbering.
    LayoutService::flowSheetLegend(legend);
  }
  legend->update();
  legend->setCacheMode(QGraphicsItem::DeviceCoordinateCache);
  syncSheetTiles();
}

void KaDrawingStudio::rebuildNorthIfNeeded() {
  auto* north = findItemById(layout(), kIdNorth);
  if (!north) return;
  // A missing file, or an older sheet's PNG arrow, is redrawn as SVG with the
  // same style, place, size and rotation.
  if (!KaDrawingNorth::isLegacyRaster(north) && !KaDrawingNorth::pictureNeedsRebuild(north)) return;
  const int kind = KaDrawingNorth::generatedKind(north);
  if (kind >= 0) m_pendingNorthSvg = KaDrawingNorth::relForKind(kind);
  placeNorth(QRectF(north->pos(), north->rect().size()), false);
}

void KaDrawingStudio::applyNorthNow() {
  auto* item = findItemById(layout(), kIdNorth);
  placeNorth(item ? QRectF(item->pos(), item->rect().size()) : defaultItemRect(kIdNorth));
}

void KaDrawingStudio::placeNorth(const QRectF& layoutRect, bool selectAfter) {
  auto* ly = layout();
  if (!ly) return;
  double percent = 100.;
  double rotation = 0.;
  bool placed = false;
  if (auto* old = findItemById(ly, kIdNorth)) {
    percent = decorationSizePercent(old);
    rotation = old->itemRotation();
    placed = old->customProperty(QStringLiteral("ka_hgis/chrome_placed"), false).toBool();
    ly->removeLayoutItem(old);
  }

  const int kind = KaDrawingNorth::kindFromRel(m_pendingNorthSvg);
  // SVG keeps the arrow vector in the PDF. The picture follows grid north (도북).
  const QString svgPath = KaDrawingNorth::writeSvg(kind);
  QRectF r = layoutRect;
  if (!r.isValid())
    r = defaultItemRect(kIdNorth);

  if (svgPath.isEmpty()) {
    auto* north = new QgsLayoutItemLabel(ly);
    north->setId(QString::fromUtf8(kIdNorth));
    north->setText(KaDrawingNorth::fallbackLabel());
    north->setHAlign(Qt::AlignHCenter);
    north->setFont(QFont(QStringLiteral("Malgun Gothic"), 12, QFont::Bold));
    north->attemptSetSceneRect(r);
    north->setFrameEnabled(false);
    ly->addLayoutItem(north);
  } else {
    auto* pic = new QgsLayoutItemPicture(ly);
    pic->setId(QString::fromUtf8(kIdNorth));
    pic->setPicturePath(svgPath, Qgis::PictureFormat::SVG);
    pic->setMode(Qgis::PictureFormat::SVG);
    pic->setResizeMode(QgsLayoutItemPicture::Zoom);
    pic->setNorthMode(QgsLayoutItemPicture::GridNorth);
    if (auto* map = mapItem())
      pic->setLinkedMap(map);
    pic->attemptSetSceneRect(r);
    ly->addLayoutItem(pic);
    pic->refreshPicture();
    if (pic->isMissingImage()) {
      const QString svg = KaDrawingNorth::qgisArrowSvg();
      if (!svg.isEmpty()) {
        pic->setPicturePath(svg, Qgis::PictureFormat::SVG);
        pic->setMode(Qgis::PictureFormat::SVG);
        pic->refreshPicture();
      }
    }
  }
  if (auto* item = findItemById(ly, kIdNorth)) {
    item->setItemRotation(rotation, false);
    item->setCustomProperty(QStringLiteral("ka_hgis/chrome_size_percent"), percent);
    item->setCustomProperty(QStringLiteral("ka_hgis/chrome_placed"), placed);
  }
  if (selectAfter)
    finishPlace();
  syncSheetTiles();
}

void KaDrawingStudio::applyScaleBarNow() {
  auto* ly = layout();
  if (!ly) return;
  auto* item = findItemById(ly, kIdScaleBar);
  placeScaleBar(item ? QRectF(item->pos(), item->rect().size()) : defaultItemRect(kIdScaleBar));
}

void KaDrawingStudio::placeScaleBar(const QRectF& layoutRect, bool selectAfter) {
  auto* ly = layout();
  if (!ly) return;
  auto* sb = dynamic_cast<QgsLayoutItemScaleBar*>(findItemById(ly, kIdScaleBar));
  if (!sb) {
    sb = new QgsLayoutItemScaleBar(ly);
    sb->setId(QString::fromUtf8(kIdScaleBar));
    ly->addLayoutItem(sb);
  }
  sb->applyDefaultSettings();
  compactSheetScaleBar(sb);
  if (auto* map = mapItem())
    sb->setLinkedMap(map);
  sb->setStyle(m_pendingScaleBarStyle.isEmpty() ? QStringLiteral("Line Ticks Up")
                                                 : m_pendingScaleBarStyle);
  sb->setUnits(Qgis::DistanceUnit::Meters);
  sb->setUnitLabel(QStringLiteral("m"));
  const QRectF keep = layoutRect.isValid()
                          ? layoutRect
                          : defaultItemRect(kIdScaleBar);
  sb->attemptSetSceneRect(keep);
  applyNiceScaleBar(sb);
  if (selectAfter)
    finishPlace();
}

void KaDrawingStudio::applyCrsLabelNow() {
  auto* ly = layout();
  if (!ly) return;
  auto* item = findItemById(ly, kIdCrs);
  placeCrsLabel(item ? QRectF(item->pos(), item->rect().size()) : defaultItemRect(kIdCrs));
}

void KaDrawingStudio::placeCrsLabel(const QRectF& layoutRect) {
  auto* ly = layout();
  if (!ly) return;
  auto* lbl = dynamic_cast<QgsLayoutItemLabel*>(findItemById(ly, kIdCrs));
  if (!lbl) {
    lbl = new QgsLayoutItemLabel(ly);
    lbl->setId(QString::fromUtf8(kIdCrs));
    lbl->setFont(QFont(QStringLiteral("Malgun Gothic"), 8, QFont::Bold));
    lbl->setBackgroundEnabled(false);
    lbl->setFrameEnabled(false);
    ly->addLayoutItem(lbl);
  }
  lbl->setBackgroundEnabled(false);
  lbl->setHAlign(Qt::AlignRight);
  lbl->setVAlign(Qt::AlignVCenter);
  const QgsCoordinateReferenceSystem crs =
      (mapItem() && mapItem()->crs().isValid()) ? mapItem()->crs()
                                                : studioMapCrs(m_mapCanvas, m_project);
  lbl->setText(koreanCrsLabel(crs));
  const QRectF keep = layoutRect.isValid()
                          ? layoutRect
                          : defaultItemRect(kIdCrs);
  lbl->attemptSetSceneRect(keep);
}

void KaDrawingStudio::placeScaleLabel(const QRectF& layoutRect, bool selectAfter) {
  auto* ly = layout();
  if (!ly) return;
  auto* lbl = dynamic_cast<QgsLayoutItemLabel*>(findItemById(ly, kIdScale));
  if (!lbl) {
    lbl = new QgsLayoutItemLabel(ly);
    lbl->setId(QString::fromUtf8(kIdScale));
    lbl->setFont(QFont(QStringLiteral("Malgun Gothic"), 9, QFont::Bold));
    ly->addLayoutItem(lbl);
  }
  const int sc = m_scaleSpin ? m_scaleSpin->value()
                             : (mapItem() ? displayScale(mapItem()->scale()) : 1000);
  lbl->setText(QStringLiteral("축척 1 : %1").arg(sc > 0 ? sc : 1000));
  lbl->setHAlign(Qt::AlignHCenter);
  lbl->setVAlign(Qt::AlignTop);
  lbl->setBackgroundEnabled(false);
  lbl->setFrameEnabled(false);
  lbl->setReferencePoint(QgsLayoutItem::UpperLeft);
  const QRectF keep = layoutRect.isValid()
                          ? layoutRect
                          : defaultItemRect(kIdScale);
  lbl->attemptSetSceneRect(keep);
  if (selectAfter) {
    finishPlace();
    if (m_status)
      showStatus(QStringLiteral("축척 글자를 넣었습니다."));
  }
}

void KaDrawingStudio::updateLayerOpacityControl() {
  if (!m_opacityRail || !m_layerTree) return;
  QgsMapLayer* cur = m_layerTree->currentLayer();
  m_railLayer = cur;
  if (LayerOps::canAdjustOpacity(cur)) {
    const int val = qBound(0, qRound(LayerOps::mapLayerOpacity(cur) * 100.0), 100);
    m_opacityRail->setPercent(val, true);
  } else {
    m_opacityRail->setPercent(100, false);
  }
  // 밝기는 그림(래스터)에만 있다. 어두운 옛 항공사진을 조판에서 바로 밝힌다.
  const bool bright = LayerOps::canAdjustBrightness(cur);
  m_opacityRail->setBrightness(bright ? LayerOps::mapLayerBrightness(cur) : 0, bright);
  m_opacityRail->setTarget(cur ? cur->name() : QString(),
                cur && (bright || LayerOps::canAdjustOpacity(cur)));
}

void KaDrawingStudio::repaintMapLayers() {
  syncMapFromLayers();
}

void KaDrawingStudio::toggleAllLayersChecked() {
  QgsProject* proj = m_project;
  QgsLayerTree* root = proj ? proj->layerTreeRoot() : nullptr;
  if (!root) return;
  const QList<QgsLayerTreeLayer*> layers = root->findLayers();
  if (layers.isEmpty()) return;
  bool anyOn = false;
  for (QgsLayerTreeLayer* n : layers) {
    if (n && n->isVisible()) {
      anyOn = true;
      break;
    }
  }
  const bool turnOn = !anyOn;
  const auto children = root->children();
  for (QgsLayerTreeNode* n : children) {
    if (n) n->setItemVisibilityCheckedRecursive(turnOn);
  }
  refreshLayerCheckAllButton();
  syncMapFromLayers();
}

void KaDrawingStudio::refreshLayerCheckAllButton() {
  if (!m_layerCheckAllBtn) return;
  QgsProject* proj = m_project;
  QgsLayerTree* root = proj ? proj->layerTreeRoot() : nullptr;
  const QList<QgsLayerTreeLayer*> layers = root ? root->findLayers() : QList<QgsLayerTreeLayer*>();
  m_layerCheckAllBtn->setEnabled(!layers.isEmpty());
  bool anyOn = false;
  for (QgsLayerTreeLayer* n : layers) {
    if (n && n->isVisible()) {
      anyOn = true;
      break;
    }
  }
  m_layerCheckAllBtn->setText(anyOn ? QStringLiteral("전체 끄기") : QStringLiteral("전체 켜기"));
}

void KaDrawingStudio::syncMapFromLayers() {
  if (m_syncingMapFromLayers || m_syncingHeritageNumbers) return;
  m_layerSyncPending = true;
  if (isVisible() && m_layerSyncTimer) m_layerSyncTimer->start();
}

void KaDrawingStudio::flushLayerSync() {
  if (!isVisible() || !m_layerSyncPending) return;
  if (property("ka_interacting").toBool() || m_pdfBusy) { m_layerSyncTimer->start(); return; }
  if (m_syncingMapFromLayers) return;
  m_layerSyncPending = false;
  m_syncingMapFromLayers = true;
  struct Guard {
    bool& flag;
    ~Guard() { flag = false; }
  } guard{m_syncingMapFromLayers};

  if (auto* map = mapItem()) {
    applyLayersToMap(map, true, false);
    syncHeritageNumbers();
    if (auto* legend = dynamic_cast<QgsLayoutItemLegend*>(findItemById(layout(), kIdLegend))) {
      if (legend->model() && legend->model()->rootGroup()) {
        const auto live = map->layers();
        QList<QgsLayerTreeLayer*> drop;
        QSet<QString> shown;
        for (auto* node : legend->model()->rootGroup()->findLayers()) {
          if (!node->layer()) continue;
          shown.insert(node->layer()->id());
          auto* treeNode = m_project ? m_project->layerTreeRoot()->findLayer(node->layer()->id()) : nullptr;
          if (treeNode && !treeNode->isVisible()) drop.append(node);
        }
        for (auto* node : drop) {
          shown.remove(node->layer()->id());
          if (auto* parent = qobject_cast<QgsLayerTreeGroup*>(node->parent()))
            parent->removeChildNode(node);
        }
        bool missing = false;
        for (auto* layer : live) {
          if (!layer || shown.contains(layer->id()) || !m_project) continue;
          auto* treeNode = m_project->layerTreeRoot()->findLayer(layer->id());
          auto* parent = treeNode ? treeNode->parent() : nullptr;
          if (parent && HeritageStyle::fromLayerName(parent->name())) missing = true;
        }
        if (missing) {
          LayoutService::tuneSheetLegend(legend);
          m_heritageNumbers.applyLegend(legend);
        }
      }
    }
  }
}

void KaDrawingStudio::relinkDecorations() {
  auto* map = mapItem();
  auto* ly = layout();
  if (!map || !ly) return;
  if (auto* legend = dynamic_cast<QgsLayoutItemLegend*>(findItemById(ly, kIdLegend))) {
    legend->setLinkedMap(map);
    legend->setResizeToContents(false);
    m_heritageNumbers.applyLegend(legend);
    const QRectF page(0.0, 0.0, m_paperW, m_paperH);
    const QRectF box = itemPaperRect(legend);
    if (!legend->customProperty(QStringLiteral("ka_hgis/legend_kept_on_page")).toBool()
        && page.isValid() && box.isValid() && !page.contains(box.center())) {
      legend->setReferencePoint(QgsLayoutItem::UpperLeft);
      legend->attemptSetSceneRect(defaultItemRect(kIdLegend));
      legend->setCustomProperty(QStringLiteral("ka_hgis/legend_kept_on_page"), true);
      LayoutService::flowSheetLegend(legend);
    }
  }
  if (auto* sb = dynamic_cast<QgsLayoutItemScaleBar*>(findItemById(ly, kIdScaleBar)))
    applyNiceScaleBar(sb);
  if (auto* pic = dynamic_cast<QgsLayoutItemPicture*>(findItemById(ly, kIdNorth)))
    pic->setLinkedMap(map);
  syncScaleDecorations();
}

void KaDrawingStudio::ensureStandardDecorations() {
  auto* ly = layout();
  if (!ly || !mapItem()) return;
  // 전문 도면 기본: 측량도면식 니들 방위표 + 교호식(Double Box) 축척자.
  if (m_pendingNorthSvg.isEmpty())
    m_pendingNorthSvg = QStringLiteral("arrows/NorthArrow_02.svg");
  if (m_pendingScaleBarStyle.isEmpty())
    m_pendingScaleBarStyle = QStringLiteral("Double Box");
  if (findItemById(ly, kIdNorth))
    rebuildNorthIfNeeded();
  else
    placeNorth(defaultItemRect(kIdNorth), false);
  if (!findItemById(ly, kIdScaleBar))
    placeScaleBar(defaultItemRect(kIdScaleBar), false);
  if (!findItemById(ly, kIdScale))
    placeScaleLabel(defaultItemRect(kIdScale), false);
  applyCrsLabelNow();
  if (auto* map = mapItem())
    applyCrsGrid(map);
  relinkDecorations();
  applyStandardChromePositions();
  syncSheetTiles();
}

// 용지를 바꿀 때: 축척자·축척 글·좌표계·방위를 어디에 있었든 지도 아래 띠의 정한 자리로 되돌린다.
void KaDrawingStudio::replaceSheetChrome() {
  auto* ly = layout();
  if (!ly) return;
  for (const char* id : {kIdScaleBar, kIdScale, kIdCrs, kIdNorth})
    if (auto* item = findItemById(ly, id))
      item->setCustomProperty(QStringLiteral("ka_hgis/chrome_placed"), false);
  applyStandardChromePositions();
}

void KaDrawingStudio::applyStandardChromePositions() {
  auto* map = mapItem();
  auto* ly = layout();
  if (!map || !ly) return;
  const QRectF page(0.0, 0.0, m_paperW, m_paperH);
  const QRectF current = itemPaperRect(map);
  const QRectF equalized = current.width() >= page.width() * 0.65
                               ? defaultMapRect()
                               : LayoutService::equalFullSheetMapRect(page, current);
  if (qAbs(equalized.left() - current.left()) > 0.5 ||
      qAbs(equalized.top() - current.top()) > 0.5 ||
      qAbs(equalized.width() - current.width()) > 0.5 ||
      qAbs(equalized.height() - current.height()) > 0.5) {
    const QgsRectangle ext = map->extent();
    const double scale = map->scale();
    map->attemptMove(QgsLayoutPoint(equalized.x(), equalized.y(), Qgis::LayoutUnit::Millimeters));
    map->attemptResize(QgsLayoutSize(equalized.width(), equalized.height(),
                                     Qgis::LayoutUnit::Millimeters));
    if (!ext.isEmpty() && ext.isFinite())
      map->zoomToExtent(ext);
    if (scale > 1.0)
      map->setScale(scale, true);
  }
  const auto chrome = LayoutService::standardSheetChrome(page, itemPaperRect(map));
  QRectF bar = chrome.scaleBar;
  if (auto* sb = dynamic_cast<QgsLayoutItemScaleBar*>(findItemById(ly, kIdScaleBar))) {
    const double denom = map->scale() > 0.0 ? map->scale()
                                            : (m_scaleSpin ? static_cast<double>(m_scaleSpin->value())
                                                           : 1000.0);
    const int segs = std::max(1, sb->numberOfSegments());
    const double segM = sb->unitsPerSegment();
    const double wantW = LayoutService::scaleBarWidthMm(segM, segs, denom);
    const double maxW = std::max(32.0, chrome.crs.left() - 4.0 - chrome.scaleBar.left());
    if (wantW >= 32.0)
      bar.setWidth(std::clamp(wantW, 32.0, maxW));
    if (!sb->customProperty(QStringLiteral("ka_hgis/chrome_placed"), false).toBool()) {
      compactSheetScaleBar(sb);
      sb->setReferencePoint(QgsLayoutItem::UpperLeft);
      sb->attemptSetSceneRect(bar);
      sb->refresh();
      sb->setCustomProperty(QStringLiteral("ka_hgis/chrome_placed"), true);
    }
    bar = QRectF(sb->pos(), sb->rect().size());
    if (bar.width() < 8.0 || bar.height() < 4.0)
      bar = chrome.scaleBar;
    if (bar.right() > chrome.crs.left() - 2.0)
      bar.setWidth(std::max(32.0, chrome.crs.left() - 4.0 - bar.left()));
    sb->setLocked(false);
  }
  QRectF label = chrome.scaleLabel;
  label.setLeft(bar.left());
  label.setWidth(std::max(36.0, bar.width()));
  label.setHeight(7.0);
  label.moveTop(bar.bottom() + 2.5);
  if (auto* sc = findItemById(ly, kIdScale)) {
    if (!sc->customProperty(QStringLiteral("ka_hgis/chrome_placed"), false).toBool()) {
      sc->setReferencePoint(QgsLayoutItem::UpperLeft);
      if (auto* scaleLbl = dynamic_cast<QgsLayoutItemLabel*>(sc)) {
        scaleLbl->setHAlign(Qt::AlignHCenter);
        scaleLbl->setVAlign(Qt::AlignTop);
      }
      sc->attemptSetSceneRect(label);
      sc->setCustomProperty(QStringLiteral("ka_hgis/chrome_placed"), true);
    }
    sc->setLocked(false);
  }
  if (auto* crs = findItemById(ly, kIdCrs)) {
    if (!crs->customProperty(QStringLiteral("ka_hgis/chrome_placed"), false).toBool()) {
      crs->attemptSetSceneRect(chrome.crs);
      crs->setCustomProperty(QStringLiteral("ka_hgis/chrome_placed"), true);
    }
    crs->setLocked(false);
  }
  if (auto* north = findItemById(ly, kIdNorth)) {
    if (!north->customProperty(QStringLiteral("ka_hgis/chrome_placed"), false).toBool()) {
      north->attemptSetSceneRect(chrome.north);
      north->setCustomProperty(QStringLiteral("ka_hgis/chrome_placed"), true);
    }
    north->setLocked(false);
  }
}

void KaDrawingStudio::snapMapScaleToNice() {
  auto* map = mapItem();
  auto* ly = layout();
  if (!map || !ly) return;
  const double widthMm =
      ly->convertFromLayoutUnits(map->rect().width(), Qgis::LayoutUnit::Millimeters).length();
  const double heightMm =
      ly->convertFromLayoutUnits(map->rect().height(), Qgis::LayoutUnit::Millimeters).length();
  QgsRectangle ext = map->extent();
  if (!(widthMm > 0.0) || ext.isNull() || !ext.isFinite() || ext.width() <= 0.0)
    return;
  const double rawW = ext.width() / (widthMm / 1000.0);
  const double rawH = (heightMm > 0.0 && ext.height() > 0.0)
                          ? ext.height() / (heightMm / 1000.0)
                          : rawW;
  const double raw = std::max(rawW, rawH);
  const int nice = LayoutService::niceScaleDenominator(raw > 0.0 ? raw : map->scale());
  if (nice <= 0) return;
  map->setScale(static_cast<double>(nice), true);
  if (m_scaleSpin) {
    const bool blocked = m_scaleSpin->blockSignals(true);
    m_scaleSpin->setValue(nice);
    m_scaleSpin->blockSignals(blocked);
  }
}

void KaDrawingStudio::applyNiceScaleBar(QgsLayoutItemScaleBar* sb) {
  if (!sb) return;
  auto* map = mapItem();
  auto* ly = layout();
  if (map)
    sb->setLinkedMap(map);
  const int segs = 4;
  sb->setNumberOfSegments(segs);
  sb->setUnits(Qgis::DistanceUnit::Meters);
  sb->setUnitLabel(QStringLiteral("m"));
  sb->setSegmentSizeMode(Qgis::ScaleBarSegmentSizeMode::Fixed);
  const double denom = map ? map->scale()
                           : (m_scaleSpin ? static_cast<double>(m_scaleSpin->value()) : 1000.0);
  double mapWmm = 160.0;
  if (map && ly)
    mapWmm = ly->convertFromLayoutUnits(map->rect().width(), Qgis::LayoutUnit::Millimeters).length();
  const double segM = LayoutService::niceScaleBarSegmentMeters(mapWmm, denom, segs);
  if (segM > 0.0)
    sb->setUnitsPerSegment(segM * decorationSizePercent(sb) / 100.);
  compactSheetScaleBar(sb);
  sb->resizeToMinimumWidth();
}

int KaDrawingStudio::displayScale(double raw) {
  if (!(raw > 0.0) || !std::isfinite(raw))
    return 1000;
  return std::max(10, static_cast<int>(std::lround(raw)));
}

void KaDrawingStudio::refreshScaleWidgets(bool readFromMap) {
  if (!m_scaleSpin) return;
  if (readFromMap) {
    if (auto* map = mapItem()) {
      const int sc = displayScale(map->scale());
      if (sc > 0) {
        const bool blocked = m_scaleSpin->blockSignals(true);
        m_scaleSpin->setValue(sc);
        m_scaleSpin->blockSignals(blocked);
      }
    }
  }
  if (auto* lbl = dynamic_cast<QgsLayoutItemLabel*>(findItemById(layout(), kIdScale)))
    lbl->setText(QStringLiteral("축척 1 : %1").arg(m_scaleSpin->value()));
  syncScaleChips();
}

void KaDrawingStudio::syncScaleDecorations() {
  auto* map = mapItem();
  auto* ly = layout();
  if (!map || !ly) return;
  const int sc = displayScale(map->scale());
  emit drawingScaleChanged(map->scale());
  if (m_scaleSpin && sc > 0) {
    const bool blocked = m_scaleSpin->blockSignals(true);
    m_scaleSpin->setValue(sc);
    m_scaleSpin->blockSignals(blocked);
  }
  bool scaleChanged = sc <= 0;
  if (auto* lbl = dynamic_cast<QgsLayoutItemLabel*>(findItemById(ly, kIdScale))) {
    const QString text = QStringLiteral("축척 1 : %1").arg(sc);
    if (sc > 0 && lbl->text() != text) {
      lbl->setText(text);
      scaleChanged = true;
    }
  } else if (sc > 0) {
    scaleChanged = true;
  }
  if (auto* crs = dynamic_cast<QgsLayoutItemLabel*>(findItemById(ly, kIdCrs))) {
    const QgsCoordinateReferenceSystem dest =
        map->crs().isValid() ? map->crs() : studioMapCrs(m_mapCanvas, m_project);
    const QString text = koreanCrsLabel(dest);
    if (crs->text() != text) {
      crs->setText(text);
      scaleChanged = true;
    }
  }
  syncScaleChips();
  // 축척은 그대로 두고 지도만 옮겨도(레이어를 가운데, 지도 조정) 주변유적 덧지도가
  // 바로 따라와야 한다. 예전에는 축척이 바뀔 때만 맞춰 유적이 옛 자리에 남았다.
  const QByteArray aboveBefore = m_aboveMapDigest;
  syncAboveLabelsMap(map);
  const bool moved = m_aboveMapDigest != aboveBefore;
  // 같은 축척·같은 자리로 다시 맞추면 번호·범례를 다시 만들지 않는다.
  if (m_scaleSyncTimer && (scaleChanged || moved))
    m_scaleSyncTimer->start();
}

void KaDrawingStudio::flushHeavyScaleSync() {
  auto* map = mapItem();
  auto* ly = layout();
  if (!map || !ly) return;
  if (property("ka_interacting").toBool() || m_pdfBusy) { m_scaleSyncTimer->start(); return; }
  if (m_layerSyncPending) flushLayerSync();
  if (auto* sb = dynamic_cast<QgsLayoutItemScaleBar*>(findItemById(ly, kIdScaleBar)))
    applyNiceScaleBar(sb);
  // 자동 간격이면 축척이 바뀔 때 도곽 간격도 다시 계산한다.
  if (m_gridEnabled)
    applyCrsGrid(map);
  if (!m_coordMapPts.isEmpty())
    relayoutCoordCallouts();
  syncHeritageNumbers();
}

bool KaDrawingStudio::syncHeritageNumbers(bool force) {
  if (m_syncingHeritageNumbers || (!isVisible() && !force)) return true;
  auto* map = mapItem();
  auto* ly = layout();
  if (!map || !ly) return false;
  m_syncingHeritageNumbers = true;
  struct Guard {
    bool& flag;
    ~Guard() { flag = false; }
  } guard{m_syncingHeritageNumbers};
  const quint64 previous = m_heritageNumbers.revision();
  m_heritageNumbers.followRenderedLabels(map);
  if (!m_heritageNumbers.update(map, force)) {
    if (m_status) showStatus(m_heritageNumbers.error());
    return false;
  }
  if (previous == m_heritageNumbers.revision()) return true;
  if (!m_numberChromePending) {
    m_numberChromePending = true;
    QTimer::singleShot(0, this, [this]() {
      m_numberChromePending = false;
      applyHeritageNumberChrome();
    });
  }
  return true;
}

void KaDrawingStudio::applyHeritageNumberChrome() {
  auto* map = mapItem();
  auto* ly = layout();
  if (!map || !ly) return;
  syncAboveLabelsMap(map);
  m_heritageNumbers.raiseAboveGeometries(map);
  if (auto* legend = dynamic_cast<QgsLayoutItemLegend*>(findItemById(ly, kIdLegend))) {
    bool missing = !legend->model() || !legend->model()->rootGroup();
    QSet<QString> present;
    if (!missing) {
      for (auto* node : legend->model()->rootGroup()->findLayers()) {
        if (node->layer()) present.insert(node->layer()->id());
      }
    }
    for (const auto& entry : m_heritageNumbers.entries()) {
      if (entry.number > 0 && !present.contains(entry.layerId)) {
        missing = true;
        break;
      }
    }
    if (missing) LayoutService::tuneSheetLegend(legend);
    m_heritageNumbers.applyLegend(legend);
  }
  // 번호만 바뀌면 번호 레이어만 다시 그린다. 수치지형도가 있는 본지도는 그대로 둔다.
  if (auto* numbers = dynamic_cast<QgsLayoutItemMap*>(findItemById(ly, kIdMapNumbers))) {
    if (!map->property("ka_interacting").toBool())
      numbers->refresh();
  }
}

void KaDrawingStudio::syncScaleChips() {
  if (!m_scaleProps || !m_scaleSpin) return;
  const int v = m_scaleSpin->value();
  const auto chips = m_scaleProps->findChildren<QToolButton*>(QStringLiteral("scaleChip"));
  for (QToolButton* b : chips) {
    const bool on = b->property("denom").toInt() == v;
    const bool blocked = b->blockSignals(true);
    b->setChecked(on);
    b->blockSignals(blocked);
  }
}

void KaDrawingStudio::onLayoutSelectionChanged(QgsLayoutItem* item) {
  updateInspector(item);
}

void KaDrawingStudio::resizeSelectedDecoration(double percent) {
  auto* ly = layout();
  if (!ly) return;
  const auto selected = ly->selectedLayoutItems(false);
  if (selected.size() != 1 || !isSheetDecoration(selected.first())) return;
  auto* item = selected.first();
  const double previous = decorationSizePercent(item);
  const double ratio = percent / previous;
  const QPointF position = item->pos();
  const QSizeF size = item->rect().size();
  item->beginCommand(QStringLiteral("도면 항목 크기"));
  item->setCustomProperty(QStringLiteral("ka_hgis/chrome_size_percent"), percent);
  if (auto* bar = dynamic_cast<QgsLayoutItemScaleBar*>(item)) {
    // Change map distance as well as ink size: scaling the graphic alone lies about distance.
    applyNiceScaleBar(bar);
    item->attemptResize(QgsLayoutSize(item->rect().width(), size.height() * ratio));
  } else {
    if (auto* label = dynamic_cast<QgsLayoutItemLabel*>(item)) {
      QFont font = label->font();
      font.setPointSizeF(font.pointSizeF() * ratio);
      label->setFont(font);
    }
    item->attemptResize(QgsLayoutSize(size * ratio));
  }
  item->attemptMove(QgsLayoutPoint(position));
  item->endCommand();
  item->update();
  updateInspector(item);
}

void KaDrawingStudio::updateInspector(QgsLayoutItem* item) {
  if (!item) {
    if (auto* ly = layout()) {
      const QList<QgsLayoutItem*> sel = ly->selectedLayoutItems(false);
      item = sel.isEmpty() ? nullptr : sel.first();
    }
  }
  const QString id = item ? item->id() : QString();
  const bool legend = id == QLatin1String(kIdLegend);
  const bool scaleItem = id == QLatin1String(kIdScale) || id == QLatin1String(kIdScaleBar)
                         || id == QLatin1String(kIdMap);
  const bool north = id == QLatin1String(kIdNorth);
  if (m_decorationSize) {
    const bool blocked = m_decorationSize->blockSignals(true);
    const bool editable = isSheetDecoration(item) && layout()->selectedLayoutItems(false).size() == 1;
    m_decorationSize->setEnabled(editable);
    m_decorationSize->setValue(editable ? decorationSizePercent(item) : 100.);
    m_decorationSize->blockSignals(blocked);
  }
  if (north && m_northSize) {
    const bool blocked = m_northSize->blockSignals(true);
    m_northSize->setValue(item->rect().width());
    m_northSize->blockSignals(blocked);
  }

  if (m_scaleBar) m_scaleBar->setVisible(true);
  if (m_cardLegend) m_cardLegend->setVisible(true);
  if (m_cardNorth) m_cardNorth->setVisible(true);

  if (legend) {
    if (auto* lg = dynamic_cast<QgsLayoutItemLegend*>(item)) {
      const bool b1 = m_legendTitle ? m_legendTitle->blockSignals(true) : false;
      const bool b2 = m_legendFont ? m_legendFont->blockSignals(true) : false;
      if (m_legendTitle) m_legendTitle->setText(lg->title());
      if (m_legendFont) {
        const QFont f = lg->styleFont(Qgis::LegendComponent::Title);
        if (f.pointSize() > 0) m_legendFont->setValue(f.pointSize());
      }
      if (m_legendTitle) m_legendTitle->blockSignals(b1);
      if (m_legendFont) m_legendFont->blockSignals(b2);
    }
  }
  if (scaleItem)
    syncScaleChips();
}

double KaDrawingStudio::drawingScale() const {
  const auto* map = mapItem();
  return map ? map->scale() : 0.;
}

void KaDrawingStudio::setDrawingScale(double denominator) {
  if (!m_scaleSpin || !mapItem() || !std::isfinite(denominator) || denominator <= 0.) return;
  const double bounded = std::clamp(denominator, static_cast<double>(m_scaleSpin->minimum()),
                                   static_cast<double>(m_scaleSpin->maximum()));
  m_scaleSpin->setValue(static_cast<int>(std::lround(bounded)));
  applyOnScreenScale();
}

void KaDrawingStudio::applyOnScreenScale() {
  auto* map = mapItem();
  auto* ly = layout();
  if (!map || !ly || !m_scaleSpin) {
    if (m_status)
      showStatus(QStringLiteral("먼저 지도 칸을 그리세요."));
    return;
  }
  const int wanted = std::max(10, m_scaleSpin->value());
  if (wanted <= 0) return;
  if (displayScale(map->scale()) != wanted)
    markUserComposed();
  map->setScale(static_cast<double>(wanted));
  syncScaleDecorations();
  if (m_status)
    showStatus(QStringLiteral("축척을 1 : %1 로 맞췄습니다.").arg(wanted));
}

QgsRectangle KaDrawingStudio::surveyExtentOnMap(QgsLayoutItemMap* map) const {
  QgsRectangle acc;
  bool have = false;
  if (!map || !m_project) return acc;
  const QgsCoordinateReferenceSystem dest = map->crs().isValid()
                                                ? map->crs()
                                                : studioMapCrs(m_mapCanvas, m_project);
  const QgsCoordinateTransformContext ctx = m_project->transformContext();
  const QSet<QString> drawnKeys = {QStringLiteral("survey_area"), QStringLiteral("feature_poly"),
                                   QStringLiteral("feature_line"), QStringLiteral("section_line"),
                                   QStringLiteral("control_points"),
                                   QStringLiteral("artifact_point"),
                                   QStringLiteral("trial_trench")};
  for (QgsMapLayer* ml : m_project->mapLayers()) {
    if (!ml || !ml->isValid()) continue;
    if (LayerOps::isReferenceLayer(ml) || LayerOps::isCadastralLayer(ml) ||
        isLiveBasemapLayer(ml))
      continue;
    const QString key = LayerOps::layerKeyOf(ml);
    if (!drawnKeys.contains(key)) continue;
    if (auto* root = m_project->layerTreeRoot()) {
      if (QgsLayerTreeLayer* n = root->findLayer(ml->id())) {
        if (!n->isVisible()) continue;
      }
    }
    QgsRectangle e = ml->extent();
    if (e.isNull() || !e.isFinite()) continue;
    if (ml->crs().isValid() && dest.isValid() && ml->crs() != dest) {
      try {
        QgsCoordinateTransform xf(ml->crs(), dest, ctx);
        xf.setBallparkTransformsAreAppropriate(true);
        e = xf.transformBoundingBox(e);
      } catch (...) {
        KA_LOG_EXCEPT();
        continue;
      }
    }
    if (!e.isFinite()) continue;
    if (!have) {
      acc = e;
      have = true;
    } else {
      acc.combineExtentWith(e);
    }
  }
  if (!have) return {};
  if (acc.width() <= 0.0 || acc.height() <= 0.0) {
    const double pad = 40.0;
    acc = QgsRectangle(acc.center().x() - pad, acc.center().y() - pad,
                       acc.center().x() + pad, acc.center().y() + pad);
  } else {
    acc.scale(1.2);
  }
  return acc;
}

void KaDrawingStudio::beginActivateMap() {
  auto* map = mapItem();
  auto* ly = layout();
  if (!map || !ly || !m_view || !m_toolMoveContent) {
    if (m_status)
      showStatus(QStringLiteral("먼저 지도 칸을 그리세요."));
    return;
  }
  m_adjustingMap = true;
  setFastLayoutPreview(true);
  if (m_adjustBar) m_adjustBar->show();
  syncAdjustButton();
  ly->setSelectedItem(map);
  m_view->setTool(m_toolMoveContent);
  if (m_status)
    showStatus(QStringLiteral("지도 조정: 드래그로 이동, 휠로 확대. 끝나면 「지도조정끝」."));
}

void KaDrawingStudio::deleteSelectedItems() {
  if (qobject_cast<QLineEdit*>(QApplication::focusWidget()) ||
      qobject_cast<QAbstractSpinBox*>(QApplication::focusWidget()))
    return;
  auto* ly = layout();
  if (!ly) return;
  const QList<QgsLayoutItem*> sel = ly->selectedLayoutItems(false);
  QSet<QString> coordTags;
  int n = 0;
  for (QgsLayoutItem* it : sel) {
    if (!it || dynamic_cast<QgsLayoutItemPage*>(it)) continue;
    const QString id = it->id();
    if (id.startsWith(QLatin1String("ka_coord_"))) {
      const int u = id.lastIndexOf(QLatin1Char('_'));
      if (u >= 0)
        coordTags.insert(id.mid(u + 1));
      continue;
    }
    // 지운 뒤에 제목·글자 칸을 건드리면 applyLegendSettings 가 범례를 다시 만들었다.
    // 사용자가 일부러 지웠으면 [범례] 를 다시 누르기 전까지는 만들지 않는다.
    if (id == QLatin1String(kIdLegend))
      m_legendRemoved = true;
    ly->removeLayoutItem(it);
    ++n;
  }
  if (!coordTags.isEmpty()) {
    QVector<QPointF> keep;
    for (int i = 0; i < m_coordMapPts.size(); ++i) {
      const QString tag(QChar(static_cast<char>('A' + (i % 26))));
      if (!coordTags.contains(tag))
        keep.append(m_coordMapPts.at(i));
    }
    m_coordMapPts = keep;
    relayoutCoordCallouts();
    n += coordTags.size();
  }
  updateInspector(nullptr);
  syncSheetTiles();
  if (m_status) {
    if (n > 0)
      showStatus(QStringLiteral("선택한 항목 %1개를 지웠습니다.").arg(n));
    else
      showStatus(QStringLiteral("지울 항목을 먼저 선택하세요."));
  }
}

void KaDrawingStudio::removeSelectedLayers() {
  if (!m_layerTree) return;
  if (auto* main = qobject_cast<MainWindow*>(window())) {
    main->removeLayersFromTree(m_layerTree);
    return;
  }
  QSet<QString> ids;
  for (QgsMapLayer* l : m_layerTree->selectedLayers()) {
    if (l) ids.insert(l->id());
  }
  if (ids.isEmpty() && m_layerTree->selectionModel()) {
    const auto idxs = m_layerTree->selectionModel()->selectedIndexes();
    auto* model = qobject_cast<QgsLayerTreeModel*>(m_layerTree->model());
    for (const auto& idx : idxs) {
      if (!idx.isValid() || !model) continue;
      auto* node = model->index2node(idx);
      if (node && QgsLayerTree::isLayer(node)) {
        if (auto* l = QgsLayerTree::toLayer(node)->layer())
          ids.insert(l->id());
      }
    }
  }
  if (ids.isEmpty()) {
    if (auto* l = m_layerTree->currentLayer())
      ids.insert(l->id());
  }
  if (ids.isEmpty()) {
    if (m_status)
      showStatus(QStringLiteral("삭제할 레이어를 먼저 선택하세요."));
    return;
  }

  auto* proj = m_project ? m_project.data() : QgsProject::instance();
  if (!proj) return;

  const auto ans = QMessageBox::question(
      this, QStringLiteral("레이어 삭제"),
      QStringLiteral("선택한 레이어 %1개를 도면/프로젝트에서 삭제할까요?").arg(ids.size()),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
  if (ans != QMessageBox::Yes) return;

  for (const QString& id : ids) {
    proj->removeMapLayer(id);
  }
  syncMapFromLayers();
  if (m_status)
    showStatus(QStringLiteral("레이어 %1개를 삭제했습니다.").arg(ids.size()));
}

bool KaDrawingStudio::eventFilter(QObject* watched, QEvent* event) {
  if (event && (event->type() == QEvent::Resize || event->type() == QEvent::Show) &&
      watched->objectName() == QLatin1String("studioLeftSplit")) {
    if (auto* splitter = qobject_cast<QSplitter*>(watched)) {
      KaLayerInformationView::protectSidebarList(
          splitter, m_layerTree, findChild<QToolButton*>(QStringLiteral("sidebarFilesToggle")),
          findChild<QWidget*>(QStringLiteral("sidebarFilesScroll")),
          findChild<KaLayerInformationPanel*>(QStringLiteral("layerInformationPanel")));
      KaDrawingStudioPlate::fitToHeight(findChild<QScrollArea*>(QStringLiteral("drawingInspectorScroll")), height());
    }
  }
  const bool onTree = m_layerTree && event && (watched == m_layerTree || watched == m_layerTree->viewport());
  const bool onDropTarget = event &&
      ((m_view && (watched == m_view || watched == m_view->viewport())) || onTree);
  if (onDropTarget &&
      (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove ||
       event->type() == QEvent::Drop)) {
    auto* de = static_cast<QDropEvent*>(event);
    const bool fromTree = m_layerTree &&
        (de->source() == m_layerTree || de->source() == m_layerTree->viewport());
    if (!fromTree && de->mimeData() && de->mimeData()->hasUrls()) {
      if (event->type() == QEvent::Drop) {
        auto* mainWin = qobject_cast<MainWindow*>(window());
        const bool ok = mainWin && mainWin->tryAddDroppedUrls(de->mimeData()->urls());
        if (ok) {
          syncMapFromLayers();
          de->acceptProposedAction();
          return true;
        }
      } else {
        de->acceptProposedAction();
        return true;
      }
    }
  }
  if (onTree && event->type() == QEvent::KeyPress) {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (ke->matches(QKeySequence::Undo) ||
        ((ke->modifiers() & Qt::ControlModifier) && ke->key() == Qt::Key_Z)) {
      if (auto* main = qobject_cast<MainWindow*>(window())) main->undoMapAction();
      else undoLastChange();
      return true;
    }
    if (ke->matches(QKeySequence::Redo) ||
        ((ke->modifiers() & Qt::ControlModifier) && ke->key() == Qt::Key_Y)) {
      if (auto* main = qobject_cast<MainWindow*>(window())) main->redoMapAction();
      else handleRedoKey();
      return true;
    }
    if (ke->key() == Qt::Key_Delete || ke->key() == Qt::Key_Backspace) {
      removeSelectedLayers();
      return true;
    }
  }
  const bool onView = m_view && event && (watched == m_view || watched == m_view->viewport());
  if (onView && event->type() == QEvent::MouseButtonPress) {
    auto* pressLayout = layout();
    m_pressUndoIndex = pressLayout && pressLayout->undoStack() && pressLayout->undoStack()->stack()
                           ? pressLayout->undoStack()->stack()->index()
                           : -1;
    setProperty("ka_interacting", true);
    if (auto* map = mapItem()) map->setProperty("ka_interacting", true);
    if (auto* legend = findItemById(layout(), kIdLegend))
      legend->setProperty("ka_interacting", true);
  }
  if (onView && (event->type() == QEvent::MouseButtonRelease ||
                 event->type() == QEvent::Hide || event->type() == QEvent::WindowDeactivate)) {
    setProperty("ka_interacting", false);
    if (auto* map = mapItem()) map->setProperty("ka_interacting", false);
    if (auto* legend = findItemById(layout(), kIdLegend))
      legend->setProperty("ka_interacting", false);
    // This filter runs before the view hands the release to the select tool, so
    // the move/resize undo command is not on the stack yet. Compare after QGIS
    // finished (below), not here.
    const int pressUndoIndex = event->type() == QEvent::MouseButtonRelease ? m_pressUndoIndex : -1;
    m_pressUndoIndex = -1;
    // Let QGIS finish its native move/resize first, then settle expensive work.
    QTimer::singleShot(0, this, [this, pressUndoIndex] {
      if (pressUndoIndex >= 0) {
        auto* ly = layout();
        if (ly && ly->undoStack() && ly->undoStack()->stack() &&
            ly->undoStack()->stack()->index() != pressUndoIndex) {
          markUserComposed();
          // 범례를 놓은 자리가 이긴다: 용지 밖이어도 도면을 다시 이을 때(relinkDecorations) 기본 자리로 되돌리지 않는다.
          if (auto* legend = findItemById(ly, kIdLegend))
            legend->setCustomProperty(QStringLiteral("ka_hgis/legend_kept_on_page"), true);
        }
      }
      if (property("ka_interacting").toBool()) return;
      if (property("ka_visible_legend_pending").toBool()) {
        setProperty("ka_visible_legend_pending", false);
        if (auto* legend = dynamic_cast<QgsLayoutItemLegend*>(findItemById(layout(), kIdLegend))) {
          LayoutService::tuneSheetLegend(legend);
          m_heritageNumbers.applyLegend(legend);
        }
      }
      if (auto* legend = findItemById(layout(), kIdLegend)) {
        if (auto* timer = legend->findChild<QTimer*>(QStringLiteral("ka_legend_flow_timer"));
            timer && timer->isActive()) timer->start(0);
      }
      syncScaleDecorations();
      if (m_layerSyncPending && m_layerSyncTimer) m_layerSyncTimer->start();
    });
  }
  if (onView && event->type() == QEvent::Resize && m_keepPaperCentered && !m_mmbPanning) {
    const QSize now = m_view->viewport() ? m_view->viewport()->size() : QSize();
    const QSize d = now - m_lastFitViewport;
    if (qAbs(d.width()) + qAbs(d.height()) > 8) {
      QTimer::singleShot(0, this, [this]() {
        if (m_keepPaperCentered && !m_mmbPanning)
          zoomPaperVisible();
      });
    }
  }
  if (onView && event->type() == QEvent::MouseButtonPress) {
    auto* me = static_cast<QMouseEvent*>(event);
    if (me->button() == Qt::MiddleButton) {
      m_mmbPanning = true;
      m_mmbLast = me->pos();
      if (m_view->viewport()) {
        m_view->viewport()->setCursor(Qt::ClosedHandCursor);
        m_view->viewport()->grabMouse();
      }
      return true;
    }
  }
  if (m_mmbPanning && event &&
      (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonRelease)) {
    auto* me = static_cast<QMouseEvent*>(event);
    if (event->type() == QEvent::MouseMove && (me->buttons() & Qt::MiddleButton)) {
      const QPoint now = me->pos();
      if (qAbs(now.x() - m_mmbLast.x()) + qAbs(now.y() - m_mmbLast.y()) > 2) {
        m_keepPaperCentered = false;
        panLayoutView(m_mmbLast, now);
        m_mmbLast = now;
      }
      return true;
    }
    if (event->type() == QEvent::MouseButtonRelease && me->button() == Qt::MiddleButton) {
      m_mmbPanning = false;
      if (m_view->viewport()) {
        m_view->viewport()->releaseMouse();
        m_view->viewport()->unsetCursor();
      }
      return true;
    }
  }
  if (onView && event->type() == QEvent::KeyPress) {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (ke->matches(QKeySequence::Undo) ||
        ((ke->modifiers() & Qt::ControlModifier) && ke->key() == Qt::Key_Z)) {
      undoLastChange();
      return true;
    }
    if (ke->key() == Qt::Key_Delete || ke->key() == Qt::Key_Backspace) {
      if (isPlacingCoordPoint())
        undoLastCoordCallout();
      else
        deleteSelectedItems();
      return true;
    }
  }
  return QMainWindow::eventFilter(watched, event);
}

void KaDrawingStudio::undoLastChange() {
  if (qobject_cast<QLineEdit*>(QApplication::focusWidget()) ||
      qobject_cast<QAbstractSpinBox*>(QApplication::focusWidget()))
    return;
  if (isPlacingCoordPoint() ||
      (!m_placeUndo.isEmpty() && m_placeUndo.last().startsWith(QLatin1String("ka_coord_")))) {
    undoLastCoordCallout();
    return;
  }
  auto* ly = layout();
  if (ly && ly->undoStack() && ly->undoStack()->stack() && ly->undoStack()->stack()->canUndo()) {
    ly->undoStack()->stack()->undo();
    updateInspector(nullptr);
    syncSheetTiles();
    if (m_status)
      showStatus(QStringLiteral("한 단계 되돌렸습니다."));
    return;
  }
  while (!m_placeUndo.isEmpty()) {
    const QString id = m_placeUndo.takeLast();
    QgsLayoutItem* it = ly ? findItemById(ly, id.toUtf8().constData()) : nullptr;
    if (it && !dynamic_cast<QgsLayoutItemPage*>(it)) {
      ly->removeLayoutItem(it);
      updateInspector(nullptr);
      syncSheetTiles();
      if (m_status)
        showStatus(QStringLiteral("방금 넣은 항목을 되돌렸습니다."));
      return;
    }
  }
  if (m_status)
    showStatus(QStringLiteral("되돌릴 것이 없습니다."));
}

// Delete 를 눌렀을 때 이 화면이 할 일. 창 단축키가 키를 먼저 가로채므로
// MainWindow 에서도 이 함수로 넘겨준다.
void KaDrawingStudio::handleDeleteKey() {
  if (m_layerTree &&
      (m_layerTree->hasFocus() || (m_layerTree->viewport() && m_layerTree->viewport()->hasFocus()))) {
    removeSelectedLayers();
    return;
  }
  if (isPlacingCoordPoint())
    undoLastCoordCallout();
  else
    deleteSelectedItems();
}

void KaDrawingStudio::handleUndoKey() {
  if (m_layerTree && (m_layerTree->hasFocus() || m_layerTree->viewport()->hasFocus())) {
    if (auto* main = qobject_cast<MainWindow*>(window())) {
      main->undoMapAction();
      return;
    }
  }
  undoLastChange();
}

void KaDrawingStudio::handleRedoKey() {
  auto* ly = layout();
  if (ly && ly->undoStack() && ly->undoStack()->stack() && ly->undoStack()->stack()->canRedo()) {
    ly->undoStack()->stack()->redo();
    updateInspector(nullptr);
    syncSheetTiles();
    if (m_status)
      showStatus(QStringLiteral("다시 실행했습니다."));
    return;
  }
  if (m_status)
    showStatus(QStringLiteral("다시 실행할 것이 없습니다."));
}

void KaDrawingStudio::keyPressEvent(QKeyEvent* event) {
  if (event && (event->matches(QKeySequence::Undo) ||
                ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_Z))) {
    handleUndoKey();
    event->accept();
    return;
  }
  if (event && (event->matches(QKeySequence::Redo) ||
                ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_Y))) {
    handleRedoKey();
    event->accept();
    return;
  }
  if (event && (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)) {
    handleDeleteKey();
    event->accept();
    return;
  }
  QMainWindow::keyPressEvent(event);
}

void KaDrawingStudio::setFastLayoutPreview(bool on) {
  auto* ly = layout();
  if (!ly) return;
  if (on) {
    if (m_savedLayoutDpi <= 1.0)
      m_savedLayoutDpi = ly->renderContext().dpi();
    if (m_savedLayoutDpi < 48.0)
      m_savedLayoutDpi = kPreviewDpi;
    ly->renderContext().setDpi(48.0);
    return;
  }
  if (m_savedLayoutDpi > 1.0) {
    ly->renderContext().setDpi(m_savedLayoutDpi);
    m_savedLayoutDpi = 0.0;
    if (auto* map = mapItem()) {
      map->invalidateCache();
      map->refresh();
    }
  }
}

void KaDrawingStudio::endActivateMap() {
  const bool was = m_adjustingMap;
  m_adjustingMap = false;
  setFastLayoutPreview(false);
  if (m_adjustBar) m_adjustBar->hide();
  syncAdjustButton();
  if (was) {
    snapMapScaleToNice();
    markUserComposed();
  }
  refreshScaleWidgets(true);
  if (auto* sb = dynamic_cast<QgsLayoutItemScaleBar*>(findItemById(layout(), kIdScaleBar)))
    applyNiceScaleBar(sb);
  if (was)
    useSelectTool();
  if (!m_coordMapPts.isEmpty())
    relayoutCoordCallouts();
}

void KaDrawingStudio::toggleMapAdjust() {
  if (m_adjustingMap)
    endActivateMap();
  else
    beginActivateMap();
}

void KaDrawingStudio::syncAdjustButton() {
  if (!m_adjustBtn) return;
  m_adjustBtn->setText(m_adjustingMap ? QStringLiteral("조정 끝") : QStringLiteral("지도 조정"));
  m_adjustBtn->setIcon(KaIcons::icon(m_adjustingMap ? QStringLiteral("layout_activate_done")
                                                    : QStringLiteral("layout_activate")));
}

void KaDrawingStudio::fitToMapCanvas() {
  if (!mapItem() || !m_mapCanvas) {
    if (m_status)
      showStatus(QStringLiteral("먼저 지도 칸을 그리세요."));
    return;
  }
  endActivateMap();
  centerOnMapCanvas();
  markUserComposed();
  if (m_status)
    showStatus(QStringLiteral("지도 화면과 같은 범위·축척으로 맞췄습니다. 축척 1 : %1")
                   .arg(displayScale(mapItem()->scale())));
}

void KaDrawingStudio::markUserComposed() {
  if (auto* ly = layout())
    LayoutService::markStudioSheetComposed(ly);
}

void KaDrawingStudio::panLayoutMapTo(const QgsPointXY& center) {
  auto* map = mapItem();
  if (!map) return;
  const QgsRectangle cur = map->extent();
  if (!cur.isFinite() || cur.width() <= 0.0 || cur.height() <= 0.0) return;
  const double keepScale = map->scale();
  const QgsRectangle next(center.x() - cur.width() * 0.5, center.y() - cur.height() * 0.5,
                          center.x() + cur.width() * 0.5, center.y() + cur.height() * 0.5);
  map->zoomToExtent(next);
  if (keepScale > 1.0)
    map->setScale(keepScale, true);
  map->invalidateCache();
  map->refresh();
  if (auto* ly = layout())
    ly->refresh();
  refreshScaleWidgets(true);
  if (auto* sb = dynamic_cast<QgsLayoutItemScaleBar*>(findItemById(layout(), kIdScaleBar)))
    applyNiceScaleBar(sb);
  if (!m_coordMapPts.isEmpty())
    relayoutCoordCallouts();
}

void KaDrawingStudio::centerOnMapCanvas() {
  auto* map = mapItem();
  if (!map || !m_mapCanvas) return;
  QgsRectangle ext = m_mapCanvas->extent();
  const QgsCoordinateReferenceSystem src = m_mapCanvas->mapSettings().destinationCrs();
  const QgsCoordinateReferenceSystem dst = map->crs().isValid()
                                               ? map->crs()
                                               : studioMapCrs(m_mapCanvas, m_project);
  if (m_project && src.isValid() && dst.isValid() && src != dst) {
    try {
      const QgsCoordinateTransform xf(src, dst, m_project->transformContext());
      ext = xf.transformBoundingBox(ext);
    } catch (...) {
      KA_LOG_EXCEPT();
    }
  }
  // Tab activation requests this more than once. Resetting an identical view
  // still makes QGIS rebuild its extent-filtered legend and render the map.
  const QgsRectangle current = map->extent();
  const double targetScale = m_mapCanvas->scale();
  const double tolerance = std::max(1.0, std::max(current.width(), current.height()) * 1e-6);
  const bool sameCenter = current.isFinite() && ext.isFinite() &&
                          std::abs(current.center().x() - ext.center().x()) <= tolerance &&
                          std::abs(current.center().y() - ext.center().y()) <= tolerance;
  const bool sameScale = !(targetScale > 1.0) ||
                         std::abs(map->scale() - targetScale) <= std::max(1.0, targetScale * 1e-4);
  if (sameCenter && sameScale)
    return;
  if (!LayoutService::applyCanvasViewToLayoutMap(map, ext, m_mapCanvas->scale()))
    return;
  map->invalidateCache();
  map->refresh();
  refreshScaleWidgets(true);
  if (auto* sb = dynamic_cast<QgsLayoutItemScaleBar*>(findItemById(layout(), kIdScaleBar)))
    applyNiceScaleBar(sb);
  if (!m_coordMapPts.isEmpty())
    relayoutCoordCallouts();
  // 아래 상태줄 축척칸도 도면 축척을 보여 준다. 예전에는 여기서 알리지 않아
  // 도면은 1:14172인데 아래 칸은 지도 탭의 1:25000으로 남았다.
  emit drawingScaleChanged(map->scale());
  if (m_status)
    showStatus(QStringLiteral("지도 화면과 같은 범위·축척으로 맞췄습니다. 표준 축척은 오른쪽 숫자 단추로 고르세요."));
}

void KaDrawingStudio::centerSurveyInMap() {
  auto* map = mapItem();
  if (!map) {
    if (m_status)
      showStatus(QStringLiteral("먼저 지도 칸을 그리세요."));
    return;
  }
  QgsPointXY c;
  bool have = false;
  QgsMapLayer* layer = m_layerTree ? m_layerTree->currentLayer() : nullptr;
  if (layer && layer->isValid()) {
    QgsRectangle target;
    if (auto* vl = qobject_cast<QgsVectorLayer*>(layer)) {
      vl->updateExtents();
      target = vl->extent();
    }
    if (target.isNull() || !target.isFinite())
      target = layer->extent();
    if (m_project && layer->crs().isValid() && map->crs().isValid() && layer->crs() != map->crs()) {
      try {
        const QgsCoordinateTransform xf(layer->crs(), map->crs(), m_project->transformContext());
        target = xf.transformBoundingBox(target);
      } catch (...) {
        KA_LOG_EXCEPT();
      }
    }
    if (target.isFinite() && target.width() > 0.0 && target.width() < 2000000.0) {
      c = target.center();
      have = true;
    }
  }
  if (!have) {
    const QgsRectangle survey = surveyExtentOnMap(map);
    if (survey.isFinite() && survey.width() > 0.0) {
      c = survey.center();
      have = true;
    }
  }
  if (!have) {
    if (m_status)
      showStatus(QStringLiteral("가운데로 둘 레이어를 왼쪽에서 고르세요."));
    return;
  }
  panLayoutMapTo(c);
  markUserComposed();
  if (m_status)
    showStatus(QStringLiteral("축척 1 : %1 을 유지한 채 용지 가운데로 옮겼습니다.")
                          .arg(displayScale(map->scale())));
}

bool KaDrawingStudio::exportDrawingPdf(const QString& path, QString* error,
                                       KaDrawingPdfProgress* progress) {
  auto* ly = layout();
  if (!ly) {
    if (error) *error = QStringLiteral("용지가 없습니다.");
    return false;
  }
  auto cancelled = [error]() {
    if (error) *error = KaDrawingPdfProgress::cancelledText();
    return false;
  };
  // Deferred syncs wait until the file is written; the PDF must match the sheet.
  const QScopedValueRollback<bool> busy(m_pdfBusy, true);
  if (progress && !progress->step(QStringLiteral("도면을 정리하는 중…"))) return cancelled();
  // Drain queued UI changes before the export's mandatory synchronous update.
  if (m_layerSyncTimer) m_layerSyncTimer->stop();
  if (m_layerSyncPending) {
    m_layerSyncPending = false;
    if (auto* map = mapItem()) applyLayersToMap(map, true, false);
  }
  if (!syncHeritageNumbers(true)) {
    if (error) *error = m_heritageNumbers.error();
    return false;
  }
  if (auto* map = mapItem())
    applyCrsGrid(map);
  if (progress &&
      !progress->step(KaDrawingPdfProgress::renderNote(m_paperW, m_paperH, KaPdfExport::kSheetDpi)))
    return cancelled();
  // With a progress window the PDF goes to a draft first: a 취소 clicked while
  // QGIS renders is seen right after, and then no file is written.
  QTemporaryDir draftDir;
  const QString target = progress && draftDir.isValid()
                             ? draftDir.filePath(QStringLiteral("drawing.pdf"))
                             : path;
  // 화면 미리보기는 낮은 해상도로 두고, 인쇄용 해상도는 내보내는 동안만 올린다.
  // Settings come from KaPdfExport (same recipe as the submission package).
  const double previewDpi = ly->renderContext().dpi();
  QString pdfError;
  const bool pdfOk = m_heritageNumbers.exportPdf(mapItem(),
      dynamic_cast<QgsLayoutItemLegend*>(findItemById(ly, kIdLegend)), target, KaPdfExport::kSheetDpi,
      &pdfError);
  ly->renderContext().setDpi(previewDpi);
  if (auto* map = mapItem()) {
    map->invalidateCache();
    map->refresh();
  }
  if (!pdfOk) {
    KaCrashGuard::logLine(QStringLiteral("[layout] PDF 내보내기 실패 %1 — %2")
                              .arg(pdfError)
                              .arg(path));
    if (error) *error = pdfError;
    return false;
  }
  if (target != path) {
    if (!progress->step(QStringLiteral("파일을 저장하는 중…"))) return cancelled();
    if (!KaDrawingPdfProgress::commit(target, path, error)) return false;
  }
  return true;
}

void KaDrawingStudio::savePdf() {
  if (!layout()) {
    QMessageBox::information(this, QStringLiteral("PDF"), QStringLiteral("용지가 없습니다."));
    return;
  }
  if (m_pdfBusy) return;
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("도면 PDF 저장"), QStringLiteral("도면.pdf"), QStringLiteral("PDF (*.pdf)"));
  if (path.isEmpty()) return;
  QString pdfError;
  bool ok = false;
  bool stopped = false;
  {
    KaDrawingPdfProgress progress(this, QStringLiteral("PDF 내보내기"));
    ok = exportDrawingPdf(path, &pdfError, &progress);
    stopped = progress.cancelled();
  }
  if (stopped) {
    showStatus(KaDrawingPdfProgress::cancelledText());
    return;
  }
  if (!ok) {
    KaUserError::warn(this, {QStringLiteral("PDF"), QStringLiteral("PDF를 저장하지 못했습니다"), pdfError, {}});
    return;
  }
  markUserComposed();
  // [int W6] E2 H11 (F059): Google imagery on the sheet gets one quiet note in the
  // status line (no dialog, no block); later saves show the plain status again.
  QString savedStatus = QStringLiteral("저장: %1").arg(path);
  const QString basemapNotice = BasemapPolicy::drawingNoticeForProject(m_project.data());
  if (!basemapNotice.isEmpty() && !property("kaBasemapNoticeShown").toBool()) {
    setProperty("kaBasemapNoticeShown", true);
    savedStatus += QStringLiteral(" — ") + basemapNotice;
  }
  if (m_status) showStatus(savedStatus);
  QMessageBox::information(this, QStringLiteral("PDF"), QStringLiteral("저장했습니다.\n%1").arg(path));
}

void KaDrawingStudio::printDrawing() {
  auto* ly = layout();
  if (!ly) {
    QMessageBox::information(this, QStringLiteral("인쇄"), QStringLiteral("용지가 없습니다."));
    return;
  }
  if (m_pdfBusy) return;
  QTemporaryDir temporary;
  if (!temporary.isValid()) {
    KaUserError::warn(this, {QStringLiteral("인쇄"), QStringLiteral("인쇄를 시작하지 못했습니다"), QStringLiteral("인쇄용 임시 폴더를 만들지 못했습니다."), {}});
    return;
  }
  const QString pdf = temporary.filePath(QStringLiteral("drawing-print.pdf"));
  // Steps and cancel go through a progress window; the print window opens after it closes.
  const QString previousStatus = m_status ? m_status->text() : QString();
  showStatus(QStringLiteral("인쇄할 도면을 만드는 중…"));
  QString error;
  bool ok = false;
  bool stopped = false;
  {
    KaDrawingPdfProgress progress(this, QStringLiteral("인쇄 준비"));
    ok = exportDrawingPdf(pdf, &error, &progress);
    stopped = progress.cancelled();
  }
  if (m_status) showStatus(previousStatus);
  if (stopped) {
    showStatus(QStringLiteral("인쇄를 취소했습니다."));
    return;
  }
  if (!ok) {
    KaUserError::warn(this, {QStringLiteral("인쇄"), QStringLiteral("인쇄하지 못했습니다"), error, {}});
    return;
  }
  markUserComposed();
  const QString name = ly->project() ? ly->project()->baseName() : QString();
  // 종이 위 축척을 알려 주려면 도면 지도의 축척이 필요하다.
  const double scale = mapItem() ? mapItem()->scale() : 0.0;
  KaPrintDialog dialog(pdf, name.isEmpty() ? QStringLiteral("도면") : name, scale, this);
  dialog.exec();
}

