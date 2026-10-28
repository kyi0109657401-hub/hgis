#pragma once
#include "core/HeritageLayoutNumbers.h"
#include "KaDrawingStudioPlate.h"
#include <QMainWindow>
#include <QPoint>
#include <QPointer>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QVector>
#include <qgscoordinatereferencesystem.h>
#include <qgspointxy.h>
#include <qgsrectangle.h>

class QCheckBox;
class QDoubleSpinBox;
class QEvent;
class QFrame;
class QKeyEvent;
class QLabel;
class QLineEdit;
class QToolButton;
class QShowEvent;
class QSpinBox;
class QTimer;
class QSplitter;
class QWidget;
class KaLayerOpacityRail;
class KaDrawingPdfProgress;
class QgsProject;
class QgsMapCanvas;
class QgsLayoutView;
class QgsLayoutViewToolSelect;
class QgsPrintLayout;
class QgsLayoutItem;
class QgsLayoutItemMap;
class QgsLayoutItemScaleBar;
class QgsLayerTreeView;
class QgsLayerTreeModel;
class QgsMapLayer;
class QgsVectorLayer;
class QgsCoordinateReferenceSystem;
class QgsRectangle;
class QgsPointXY;
class QgsGeometry;
class KaLayoutMapAdjustTool;
class KaLayoutCoordPointTool;

class KaDrawingStudio : public QMainWindow {
  Q_OBJECT
public:
  enum class PlaceKind { MapFrame, Legend, North, ScaleBar, ScaleLabel, CrsLabel, TitleBlock };

  explicit KaDrawingStudio(QgsProject* project, QgsMapCanvas* mapCanvas,
                           double paperWidthMm, double paperHeightMm,
                           QWidget* parent = nullptr);
  static bool promptPaper(QWidget* parent, double* widthMm, double* heightMm);
  static constexpr double kA4PortraitWidthMm = 210.0;
  static constexpr double kA4PortraitHeightMm = 297.0;
  // A4 가로(297×210)에서 좌·우·위 각 10mm를 더 준 현장 조판.
  static constexpr double kA4LandscapeWidthMm = 297.0;
  static constexpr double kA4LandscapeHeightMm = 210.0;
  static constexpr double kFieldPaperExtraMm = 10.0;
  static constexpr double kFieldPaperWidthMm =
      kA4LandscapeWidthMm + kFieldPaperExtraMm * 2.0;
  static constexpr double kFieldPaperHeightMm =
      kA4LandscapeHeightMm + kFieldPaperExtraMm;
  static constexpr double kFieldEdgeMm = 10.0;
  static constexpr double kFieldBottomChromeMm = 38.0;
  static bool isLegacyA4LandscapeMm(double widthMm, double heightMm);
  void applyFieldPageGrow();
  void applyFieldEdge();
  void resetPaper(double widthMm, double heightMm, bool preserveExisting = true);
  void refreshMapFromProject();
  // 용지가 없거나 화면 밖에 있으면 페이지를 만들고 보이게 맞춘다.
  void showSheetPage();
  void updateLayerOpacityControl();
  void repaintMapLayers();
  double drawingScale() const;
  // 입체지형 3D 그림을 맵 칸에 올리고, 축척·방위는 DEM 범위에 맞춘다.
  void placeTerrain3dPicture(const QString& pngPath, const QgsRectangle& groundExtent,
                             const QgsCoordinateReferenceSystem& crs);
  void importMapCoordCallouts(const QVector<QgsPointXY>& pts, const QVector<QString>& letters,
                              const QVector<QString>& texts, const QgsGeometry& frame);
  void applyImportedCoordCallouts();
  bool isMapAdjusting() const { return m_adjustingMap; }
  // Coordinate grid (crosses and border ruler). Off by default; once turned on
  // it is stored on the sheet so reopening and PDF export keep it.
  // followMap: the map's coordinate grid (same step and nodes) instead of an interval of its own.
  void setGridOptions(bool enabled, double intervalM, bool showNumbers, bool followMap = false);
  bool eventFilter(QObject* watched, QEvent* event) override;

public slots:
  void setDrawingScale(double denominator);
  void beginActivateMap();
  void endActivateMap();
  // Bottom tool row 「지도 조정」: ends map adjusting when active, starts it otherwise.
  void toggleMapAdjust();
  void centerSurveyInMap();
  void centerOnMapCanvas();
  // Bottom tool row 「화면 맞춤」: copy the map canvas extent and scale again.
  void fitToMapCanvas();
  void openPaperSettingsDialog();
  void focusGridSettings();
  void placeTitleBlock();
  void beginPlaceCoordPoint();
  void undoLastCoordCallout();
  void endPlaceCoordPoint();
  bool isPlacingCoordPoint() const;
  // 창 단축키(Delete·Ctrl+Z)가 이 화면 대신 가로채므로, 바깥에서 넘겨받는다.
  void handleDeleteKey();
  void handleUndoKey();
  void handleRedoKey();
  void deleteSelectedItems();
  void removeSelectedLayers();
  void undoLastChange();
  void savePdf();
  // 「PDF 내보내기」와 같은 도면 PDF를 임시로 만든 뒤 인쇄 창(한 장 / 여러 장 나눠 찍기)을 연다.
  void printDrawing();

signals:
  void drawingScaleChanged(double denominator);
  // One-line guidance for the main window status bar.
  void statusMessage(const QString& text);

private slots:
  void useSelectTool();
  void zoomPaperVisible();
  void syncMapFromLayers();
  void flushLayerSync();
  void toggleAllLayersChecked();
  void refreshLayerCheckAllButton();
  void beginPlaceLegend();
  void beginPlaceNorth(const QString& svgRel);
  void beginPlaceScaleBar(const QString& style);
  void beginPlaceScaleLabel();
  void beginPlaceCrsLabel();
  void applyOnScreenScale();
  void applyLegendSettings();
  void onLayoutSelectionChanged(QgsLayoutItem* item);
  void syncScaleDecorations();
  void flushHeavyScaleSync();
  void placeCoordCallout(const QPointF& layoutPt);
  void relayoutCoordCallouts();
  void panLayoutMapTo(const QgsPointXY& center);
  void updateCoordFrame();
  void storeCurrentSheet();
  void restoreStoredSheet(const QString& title);
  void removeStoredSheet(const QString& title);

private:
  void buildUi();
  QWidget* buildBottomTools(QWidget* desk);
  void ensureBlankLayout();
  void attachLayoutToView();
  void detachLayoutFromView();
  void ensureSheetPage();
  bool sheetPageInView() const;
  QgsPrintLayout* layout() const;
  QgsLayoutItemMap* mapItem() const;
  void applyLayersToMap(QgsLayoutItemMap* map, bool includeLiveBasemap, bool refitExtent);
  // 라벨 위에 위 레이어를 한 번 더 그리는 덧지도를 본 지도에 맞춘다.
  void syncAboveLabelsMap(QgsLayoutItemMap* base);
  bool syncHeritageNumbers(bool force = false);
  // 저장과 인쇄가 같은 도면을 쓰도록 PDF 한 벌을 path 에 만든다(300 DPI, 유적 번호 확정 포함).
  // With progress, steps and a cancel button are shown; a cancel writes no file.
  bool exportDrawingPdf(const QString& path, QString* error,
                        KaDrawingPdfProgress* progress = nullptr);
  void applyHeritageNumberChrome();
  QgsVectorLayer* blankMapLayer();
  static void ensureLayoutGuiRegistered(QgsMapCanvas* mapCanvas);
  void createOrResizeMap(const QRectF& layoutRect);
  void connectMapSignals(QgsLayoutItemMap* map);
  void placeNorth(const QRectF& layoutRect, bool selectAfter = true);
  void placeScaleBar(const QRectF& layoutRect, bool selectAfter = true);
  void placeScaleLabel(const QRectF& layoutRect, bool selectAfter = true);
  void placeCrsLabel(const QRectF& layoutRect);
  void refreshScaleWidgets(bool readFromMap = false);
  void applyScaleBarNow();
  void applyNorthNow();
  void rebuildNorthIfNeeded();
  void applyCrsLabelNow();
  void relinkDecorations();
  void ensureStandardDecorations();
  void applyStandardChromePositions();
  void replaceSheetChrome();
  void snapMapScaleToNice();
  void applyNiceScaleBar(QgsLayoutItemScaleBar* sb);
  QRectF defaultItemRect(const char* id) const;
  QgsRectangle surveyExtentOnMap(QgsLayoutItemMap* map) const;
  void finishPlace();
  void selectPlacedItem();
  void updateInspector(QgsLayoutItem* item);
  void resizeSelectedDecoration(double percent);
  void syncScaleChips();
  void applyCrsGrid(QgsLayoutItemMap* map);
  void loadGridSettings();
  void syncGridButton();
  void syncSheetTiles();
  void syncAdjustButton();
  void clearGridCoordinateLabels();
  // Marks the sheet as composed only on user composing actions (placing or moving
  // items, scale, grid, PDF/print). Opening the tab or syncing layers does not.
  void markUserComposed();
  void autoPlaceDefaultSheet();
  QRectF defaultMapRect() const;
  void panLayoutView(const QPoint& from, const QPoint& to);
  void recenterPaper();
  void setFastLayoutPreview(bool on);
  void showEvent(QShowEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  static int displayScale(double raw);

  QPointer<QgsProject> m_project;
  QPointer<QgsMapCanvas> m_mapCanvas;
  double m_paperW = kA4PortraitWidthMm;
  double m_paperH = kA4PortraitHeightMm;
  QgsLayoutView* m_view = nullptr;
  QgsLayoutViewToolSelect* m_toolSelect = nullptr;
  KaLayoutMapAdjustTool* m_toolMoveContent = nullptr;
  KaLayoutCoordPointTool* m_toolCoordPoint = nullptr;
  QgsLayerTreeView* m_layerTree = nullptr;
  QgsLayerTreeModel* m_layerModel = nullptr;
  QToolButton* m_layerCheckAllBtn = nullptr;
  class KaFileBrowserPanel* m_filesPanel = nullptr;
  // 일부러 지운 범례는 글자 설정을 만져도 되살아나지 않는다.
  bool m_legendRemoved = false;
  KaLayerOpacityRail* m_opacityRail = nullptr;
  // 투명도·밝기 막대가 지금 어느 레이어를 보고 있는지. 막대를 만질 때
  // 트리의 현재 항목이 바뀌어 있을 수 있어, 켤 때 잡아 둔 레이어를 쓴다.
  QPointer<QgsMapLayer> m_railLayer;
  QSplitter* m_studioSplit = nullptr;
  // Grid crosses/border ruler are turned on from 「좌표 격자」 in the drawing info card.
  // Interval 0 = automatic from the scale (1-2-5 series).
  bool m_gridEnabled = false;
  bool m_gridShowNums = false;
  double m_gridIntervalM = 0.0;
  bool m_gridFollowMap = false;  // 지도 좌표격자와 같게 (사용자 요청 2026-10-20)
  QToolButton* m_gridBtn = nullptr;
  QToolButton* m_legendTile = nullptr;
  QToolButton* m_titleTile = nullptr;
  QToolButton* m_adjustBtn = nullptr;
  QLabel* m_status = nullptr;
  void showStatus(const QString& text);
  QFrame* m_adjustBar = nullptr;
  QFrame* m_scaleBar = nullptr;
  QLineEdit* m_legendTitle = nullptr;
  QSpinBox* m_legendFont = nullptr;
  QCheckBox* m_legendBold = nullptr;
  QCheckBox* m_legendItalic = nullptr;
  QCheckBox* m_heritageNames = nullptr;
  QDoubleSpinBox* m_northSize = nullptr;
  QDoubleSpinBox* m_decorationSize = nullptr;
  QSpinBox* m_scaleSpin = nullptr;
  QWidget* m_scaleProps = nullptr;
  QFrame* m_cardLegend = nullptr;
  QFrame* m_cardNorth = nullptr;
  PlaceKind m_placeKind = PlaceKind::MapFrame;
  QString m_pendingNorthSvg;
  QString m_pendingScaleBarStyle;
  QStringList m_placeUndo;
  bool m_adjustingMap = false;
  bool m_holdTerrainExtent = false;
  QgsRectangle m_terrainGroundExtent;
  QgsCoordinateReferenceSystem m_terrainCrs;
  bool m_paperFitPending = true;
  bool m_keepPaperCentered = true;
  bool m_mmbPanning = false;
  QPoint m_mmbLast;
  QSize m_lastFitViewport;
  bool m_syncingMapFromLayers = false;
  bool m_syncingHeritageNumbers = false;
  bool m_numberChromePending = false;
  // Keeps deferred syncs from changing the sheet while a PDF/print file is made.
  bool m_pdfBusy = false;
  // Undo index at mouse press; a different index at release means the user moved
  // or resized an item.
  int m_pressUndoIndex = -1;
  QByteArray m_aboveMapDigest;
  HeritageLayoutNumbers m_heritageNumbers;
  QTimer* m_layerSyncTimer = nullptr;
  bool m_layerSyncPending = false;
  QTimer* m_scaleSyncTimer = nullptr;
  double m_savedLayoutDpi = 0.0;
  QVector<QPointF> m_coordMapPts;
  QVector<QPointF> m_coordFrameMap;
  QVector<QgsPointXY> m_importCoordPts;
  QVector<QString> m_importCoordLetters;
  QVector<QString> m_importCoordTexts;
  QPointer<QgsVectorLayer> m_blankMapLayer;
};
