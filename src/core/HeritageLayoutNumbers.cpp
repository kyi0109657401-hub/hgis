#include "HeritageLayoutNumbers.h"
#include "LayoutBadgePlacer.h"
#include "LayoutService.h"
#include "HeritageImport.h"
#include "HeritageStyle.h"
#include "LayerOps.h"
#include "PdfExportSettings.h"

#include <cmath>

#include <qgscategorizedsymbolrenderer.h>
#include <qgsexception.h>
#include <qgscallout.h>
#include <qgsfillsymbollayer.h>
#include <qgslabelpointsettings.h>
#include <qgslabelplacementsettings.h>
#include <qgslabelthinningsettings.h>
#include <qgslabelingresults.h>
#include <qgslinesymbol.h>
#include <qgscoordinatetransform.h>
#include <qgsexpression.h>
#include <qgsexpressioncontext.h>
#include <qgsexpressioncontextutils.h>
#include <qgsfeaturerequest.h>
#include <qgsfeatureiterator.h>
#include <qgsgeometry.h>
#include <qgsgeometryengine.h>
#include <qgspointxy.h>
#include <qgslayout.h>
#include <qgslayoutitemlegend.h>
#include <qgslayoutitemmap.h>
#include <qgsmaplayer.h>
#include <qgsnullsymbolrenderer.h>
#include <qgslayoutitempage.h>
#include <qgslayoutpagecollection.h>
#include <qgslayoutexporter.h>
#include <qgslayertree.h>
#include <qgslayertreelayer.h>
#include <qgslayertreemodel.h>
#include <qgslegendrenderer.h>
#include <qgsmaplayerlegend.h>
#include <qgsmaplayerstyle.h>
#include <qgsmarkersymbol.h>
#include <qgsmarkersymbollayer.h>
#include <qgslayertreemodellegendnode.h>
#include <qgspallabeling.h>
#include <qgsproject.h>
#include <qgsrendercontext.h>
#include <qgssinglesymbolrenderer.h>
#include <qgssymbollayer.h>
#include <qgstextbackgroundsettings.h>
#include <qgstextformat.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayerlabeling.h>

#include <QFont>
#include <QFontMetricsF>
#include <QGraphicsItem>
#include <QMetaType>
#include <QSet>
#include <QDomDocument>
#include <QCryptographicHash>
#include <QDataStream>
#include <QTransform>
#include <algorithm>
#include <cmath>
#include <memory>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QPainterPath>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTimer>

namespace {
struct DatasetKey {
  HeritageDataset dataset;
  const char* key;
};
// ASCII keys stored in the project file. Never reuse a key for another dataset.
constexpr DatasetKey kDatasetKeys[] = {
    {HeritageDataset::DesignatedHeritage, "designated_heritage"},
    {HeritageDataset::AlterationStandard, "alteration_standard"},
    {HeritageDataset::BuriedHeritageArea, "buried_heritage_area"},
    {HeritageDataset::HeritageDistributionMap, "heritage_distribution_map"},
    {HeritageDataset::SurfaceSurveyArea, "surface_survey_area"},
    {HeritageDataset::ExcavationSurveyArea, "excavation_survey_area"},
};

std::optional<HeritageDataset> datasetFor(QgsVectorLayer* layer) {
  // The logical tag wins; group and layer titles are only a fallback for
  // layers loaded before the tag existed.
  if (auto tagged = HeritageLayoutNumbers::taggedDataset(layer)) return tagged;
  if (layer->project()) {
    for (QgsLayerTreeNode* node = layer->project()->layerTreeRoot()->findLayer(layer->id());
         node; node = node->parent()) {
      if (auto dataset = HeritageStyle::fromLayerName(node->name())) return dataset;
    }
  }
  return HeritageStyle::fromLayerName(layer->name());
}

QColor inkFor(const QColor& color) {
  return (color.redF() * .299 + color.greenF() * .587 + color.blueF() * .114) > .62
             ? QColor(Qt::black) : QColor(Qt::white);
}

double circleSize(int number) {
  return qMax(2.3, 1.0 + QString::number(number).size() * 0.725) * 1.3;
}

// Site names (1:5,000 and larger, when the sheet asks for names): black text
// with a white halo, no box. The box is the text plus the halo, in mm.
constexpr double kNameSizeMm = 2.2;
constexpr double kNameHaloMm = 0.5;

QSizeF nameBoxMm(const QString& name) {
  QFont font(QStringLiteral("Malgun Gothic"));
  font.setPixelSize(1000);
  const QFontMetricsF metrics(font);
  return QSizeF(metrics.horizontalAdvance(name) / 1000. * kNameSizeMm + kNameHaloMm,
                kNameSizeMm * 1.15 + kNameHaloMm);
}

void prepareLayoutSymbol(QgsSymbol* symbol, HeritageDataset dataset)
{
  if (!symbol)
    return;
  if (!symbol->hasDataDefinedProperties()) {
    for (int i = 0; i < symbol->symbolLayerCount(); ++i) {
      auto* part = symbol->symbolLayer(i);
      if (!part || (part->layerType() != QLatin1String("SimpleFill") &&
                    part->layerType() != QLatin1String("SimpleLine") &&
                    part->layerType() != QLatin1String("SimpleMarker")))
        continue;
      const QColor color = part->color();
      const QColor stroke = part->strokeColor();
      const QColor mappedColor = HeritageStyle::layoutColor(dataset, color);
      const QColor mappedStroke = HeritageStyle::layoutColor(dataset, stroke);
      if (mappedColor != color)
        part->setColor(mappedColor);
      if (mappedStroke != stroke)
        part->setStrokeColor(mappedStroke);
    }
  }
  if (symbol->type() != Qgis::SymbolType::Fill)
    return;
  for (int i = 0; i < symbol->symbolLayerCount(); ++i) {
    if (symbol->symbolLayer(i) && symbol->symbolLayer(i)->layerType() == QLatin1String("CentroidFill"))
      return;
  }
  auto* centroid = new QgsCentroidFillSymbolLayer();
  centroid->setPointOnSurface(true);
  centroid->setClipPoints(false);
  auto* marker = new QgsMarkerSymbol();
  marker->setSize(0.01);
  marker->setSizeUnit(Qgis::RenderUnit::Millimeters);
  marker->setColor(QColor(0, 0, 0, 0));
  centroid->setSubSymbol(marker);
  symbol->appendSymbolLayer(centroid);
}

int categoryIndexForClassValue(const QgsCategoryList& categories, const QVariant& value)
{
  for (int i = 0; i < categories.size(); ++i) {
    if (!categories.at(i).renderState())
      continue;
    const QVariant categoryValue = categories.at(i).value();
    if (categoryValue == value)
      return i;
    if (categoryValue.userType() == QMetaType::QVariantList && categoryValue.toList().contains(value))
      return i;
  }
  return -1;
}

QVariant classAttributeValue(QgsVectorLayer* layer, const QString& attribute, const QgsFeature& feature,
                             QgsExpressionContext& context)
{
  const int field = layer->fields().lookupField(attribute);
  if (field >= 0)
    return feature.attribute(field);
  QgsExpression expression(attribute);
  context.setFeature(feature);
  return expression.evaluate(&context);
}

// A one-digit badge is about 3 mm and a three-digit badge about 4.2 mm.
// Badge centres stay the sheet's largest badge plus this gap apart on paper
// (circles may nearly touch, never cover); LayoutBadgePlacer moves a badge to
// a free ring slot inside the visible frame and off the legend card.
constexpr double kNumberGapMm = 0.3;

// Only sites this close to the survey area get a number and a legend row;
// farther sites stay drawn in colour (2026-10-19 사용자 요청).
constexpr double kNumberedNearSurveyM = 500.;

// The survey area grown by kNumberedNearSurveyM, in the map CRS. Empty without
// a drawn survey area: then every site on the paper is numbered as before.
QgsGeometry surveyNeighbourhood(QgsProject* project, const QgsCoordinateReferenceSystem& mapCrs) {
  QgsVectorLayer* survey = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
  if (!survey || !survey->isValid()) return {};
  QVector<QgsGeometry> parts;
  QgsFeature feature;
  auto features = survey->getFeatures(QgsFeatureRequest().setNoAttributes());
  while (features.nextFeature(feature))
    if (feature.hasGeometry()) parts.append(feature.geometry());
  if (parts.isEmpty()) return {};
  // Survey layers use the projected work CRS (metres), so buffer there.
  QgsGeometry near = QgsGeometry::unaryUnion(parts).buffer(kNumberedNearSurveyM, 8);
  try {
    near.transform(QgsCoordinateTransform(survey->crs(), mapCrs, project->transformContext()));
  } catch (const QgsCsException&) {
    return {};
  }
  return near;
}

void applyHeritageNumberCallout(QgsPalLayerSettings& labels) {
  labels.geometryGeneratorEnabled = true;
  labels.geometryGenerator = QStringLiteral("point_on_surface($geometry)");
  labels.geometryGeneratorType = Qgis::GeometryType::Point;
  // C++ already chose PositionX/Y. OrderedPositionsAroundPoint orbited the
  // pin and stacked numbers. Callout draws only when the badge is displaced.
  // https://docs.qgis.org/3.44/en/docs/user_manual/style_library/label_settings.html
  labels.placement = Qgis::LabelPlacement::OverPoint;
  labels.centroidInside = true;
  labels.dist = 0;
  labels.zIndex = 10000;
  labels.placementSettings().setOverlapHandling(Qgis::LabelOverlapHandling::AllowOverlapAtNoCost);
  labels.placementSettings().setAllowDegradedPlacement(true);
  auto* callout = new QgsSimpleLineCallout();
  callout->setEnabled(true);
  callout->setMinimumLength(1.2);
  callout->setMinimumLengthUnit(Qgis::RenderUnit::Millimeters);
  auto line = QgsLineSymbol::createSimple({
      {QStringLiteral("line_color"), QStringLiteral("#1f2937")},
      {QStringLiteral("line_width"), QStringLiteral("0.10")},
      {QStringLiteral("line_width_unit"), QStringLiteral("MM")},
  });
  if (line) callout->setLineSymbol(line.release());
  labels.setCallout(callout);
}

struct Site {
  QString name;
  QStringList ids;
  QVariantList values;
  QString labelId;
  double labelX = 0.;
  double labelY = 0.;
  double labelWeight = -1.;
  double originX = 0.;
  double originY = 0.;
  int index = 0;
  int originalIndex = -1;
  QVector<QgsGeometry> pieces;  // On-paper polygon parts, layer CRS.
  QgsGeometry area;              // Their union in map CRS: the badge may sit anywhere on it.
  QColor color;
  std::shared_ptr<QgsSymbol> symbol;
};

QPainterPath visibleMapOnPaper(QgsLayoutItemMap* map, bool exporting) {
  QPainterPath paper;
  auto* pages = map->layout()->pageCollection();
  for (int i = 0; i < pages->pageCount(); ++i) {
    if (exporting && !pages->shouldExportPage(i)) continue;
    QPainterPath page;
    page.addPolygon(pages->page(i)->mapToScene(pages->page(i)->rect()));
    paper = paper.united(page);
  }
  QPainterPath frame;
  frame.addPolygon(map->mapToScene(map->rect()));
  return paper.intersected(frame);
}

// Geographic footprint of the map that actually sits on the exported page.
// Scale changes this polygon; label collision does not.
QgsGeometry mapFootprintOnPaper(QgsLayoutItemMap* map, bool exporting) {
  const QgsGeometry extent = QgsGeometry::fromQPolygonF(map->visibleExtentPolygon());
  const QPainterPath onPaper = visibleMapOnPaper(map, exporting);
  if (onPaper.isEmpty()) return extent;
  const QgsGeometry clipped = QgsGeometry::fromQPolygonF(
      map->layoutToMapCoordsTransform().map(onPaper.toFillPolygon()));
  return clipped.isEmpty() ? extent : clipped;
}

QgsLayoutItemMap* layoutMapById(QgsLayout* layout, const QString& id) {
  if (!layout) return nullptr;
  for (QGraphicsItem* item : layout->items()) {
    auto* map = dynamic_cast<QgsLayoutItemMap*>(item);
    if (map && map->id() == id) return map;
  }
  return nullptr;
}

QList<QgsMapLayer*> numberedSourceLayers(QgsLayoutItemMap* map) {
  QList<QgsMapLayer*> layers;
  QSet<QString> seen;
  auto add = [&](QgsMapLayer* layer) {
    if (!layer || seen.contains(layer->id())) return;
    seen.insert(layer->id());
    layers.append(layer);
  };
  if (!map) return layers;
  for (auto* layer : map->layers()) add(layer);
  auto* layout = map->layout();
  if (!layout) return layers;
  if (auto* overlay = layoutMapById(layout, QStringLiteral("ka_map_above"))) {
    for (auto* layer : overlay->layers()) add(layer);
  }
  return layers;
}

void bindLegendFilterMaps(QgsLayoutItemLegend* legend) {
  if (!legend || !legend->layout()) return;
  QList<QgsLayoutItemMap*> maps;
  if (QgsLayoutItemMap* linked = legend->linkedMap())
    maps.append(linked);
  for (const QString& id : {QStringLiteral("ka_map_above"), QStringLiteral("ka_map_numbers")}) {
    if (QgsLayoutItemMap* map = layoutMapById(legend->layout(), id)) {
      if (!maps.contains(map)) maps.append(map);
    }
  }
  if (maps.size() <= 1 || legend->filterByMapItems() == maps) return;
  // 본지도에서 뺀 유적 도형은 덧그림에만 있다. 범례 필터가 본지도만 보면
  // 번호·유적명이 빠진다. 여기서 검사를 다시 돌리면 hitTestCompleted가
  // applyLegend를 반복해 조판이 멈춘다.
  // https://qgis.org/pyqgis/master/core/QgsLayoutItemLegend.html
  legend->setFilterByMapItems(maps);
}

QMap<QString, QString> geometryOnlyOverrides(const QMap<QString, QString>& overrides) {
  QMap<QString, QString> stripped;
  for (auto it = overrides.cbegin(); it != overrides.cend(); ++it) {
    QDomDocument document;
    if (!document.setContent(it.value())) {
      stripped.insert(it.key(), it.value());
      continue;
    }
    document.documentElement().setAttribute(QStringLiteral("labelsEnabled"), QStringLiteral("0"));
    stripped.insert(it.key(), document.toString());
  }
  return stripped;
}

const QgsLabelingResults* numberPreviewResults(QgsLayoutItemMap* base) {
  if (auto* numbers = layoutMapById(base ? base->layout() : nullptr, QStringLiteral("ka_map_numbers"))) {
    if (auto* results = numbers->previewLabelingResults()) return results;
  }
  return base ? base->previewLabelingResults() : nullptr;
}

const QgsLabelingResults* numberExportResults(QgsLayoutItemMap* base, QgsLayoutExporter& exporter) {
  if (auto* numbers = layoutMapById(base ? base->layout() : nullptr, QStringLiteral("ka_map_numbers"))) {
    if (auto* results = exporter.labelingResults().value(numbers->uuid())) return results;
  }
  return base ? exporter.labelingResults().value(base->uuid()) : nullptr;
}
}  // namespace

HeritageLayoutNumbers::~HeritageLayoutNumbers() {
  delete m_numberLayer;
}

QString HeritageLayoutNumbers::entryKey(const QString& layerId, int number) {
  return layerId + QLatin1Char(':') + QString::number(number);
}

QString HeritageLayoutNumbers::datasetPropertyKey() {
  return QStringLiteral("ka_hgis/heritage_dataset");
}

QString HeritageLayoutNumbers::namesPropertyKey() {
  return QStringLiteral("ka_hgis/heritage_names_large_scale");
}

void HeritageLayoutNumbers::tagDataset(QgsMapLayer* layer, HeritageDataset dataset) {
  if (!layer) return;
  for (const auto& entry : kDatasetKeys) {
    if (entry.dataset == dataset) {
      layer->setCustomProperty(datasetPropertyKey(), QString::fromLatin1(entry.key));
      return;
    }
  }
}

std::optional<HeritageDataset> HeritageLayoutNumbers::taggedDataset(const QgsMapLayer* layer) {
  if (!layer) return std::nullopt;
  const QString key = layer->customProperty(datasetPropertyKey()).toString();
  if (key.isEmpty()) return std::nullopt;
  for (const auto& entry : kDatasetKeys)
    if (key == QLatin1String(entry.key)) return entry.dataset;
  return std::nullopt;
}

QSet<QString> HeritageLayoutNumbers::legendKeys() const {
  if (m_tracking) return m_visibleKeys;
  QSet<QString> keys;
  for (const auto& entry : m_entries) {
    if (entry.number > 0) keys.insert(entryKey(entry.layerId, entry.number));
  }
  return keys;
}

HeritageLayoutNumbers* HeritageLayoutNumbers::forMap(QgsLayoutItemMap* map) {
  return map ? qobject_cast<HeritageLayoutNumbers*>(map->property("ka_number_owner").value<QObject*>()) : nullptr;
}

QgsLayoutItemMap* HeritageLayoutNumbers::numbersMapOf(QgsLayoutItemMap* base) {
  return layoutMapById(base ? base->layout() : nullptr, QStringLiteral("ka_map_numbers"));
}

void HeritageLayoutNumbers::applyBaseStyleOverrides(QgsLayoutItemMap* map) {
  if (!map) return;
  map->setKeepLayerStyles(true);
  map->setLayerStyleOverrides(geometryOnlyOverrides(m_overrides));
}

void HeritageLayoutNumbers::connectNumberPreview(QgsLayoutItemMap* map) {
  auto* numbers = numbersMapOf(map);
  if (!numbers || !m_tracking || m_followedMap != map) return;
  if (numbers->property("ka_number_follow").toBool()) return;
  numbers->setProperty("ka_number_follow", true);
  m_renderConnections.append(connect(numbers, &QgsLayoutItemMap::previewRefreshed, this, [this, map]() {
    if (!map || map->property("ka_interacting").toBool() || m_exporting) return;
    acceptRenderedLabels(map, numberPreviewResults(map));
  }));
}

QByteArray HeritageLayoutNumbers::renderSignature(QgsLayoutItemMap* map, bool includeStyles) const {
  QByteArray bytes;
  QDataStream stream(&bytes, QIODevice::WriteOnly);
  stream << m_revision << map->visibleExtentPolygon() << map->crs().toWkt()
         << map->scale() << map->rect() << map->sceneTransform() << map->layout()->renderContext().dpi()
         << mapFootprintOnPaper(map, m_exporting).asWkt();
  if (includeStyles) stream << map->layerStyleOverrides();
  for (auto* layer : map->layers())
    stream << layer->id() << m_layerRevisions.value(layer->id())
           << layer->customProperty(QStringLiteral("ka_hgis/layout_numbers_visible"), true).toBool();
  for (auto* page : map->layout()->pageCollection()->pages()) stream << page->rect() << page->sceneTransform();
  return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
}

QSet<QString> HeritageLayoutNumbers::placedKeys(QgsLayoutItemMap* map, const QgsLabelingResults* results) const {
  QSet<QString> keys;
  if (!map || !map->layout() || !results || !map->isVisible()) return keys;
  const QPainterPath onPaper = visibleMapOnPaper(map, m_exporting);
  bool invertible = false;
  const QTransform toPaper = map->layoutToMapCoordsTransform().inverted(&invertible);
  if (!invertible) return keys;
  QHash<QString, QHash<qint64, int>> featureNumbers;
  for (const auto& entry : m_entries)
    for (qint64 id : entry.featureIds) featureNumbers[entry.layerId].insert(id, entry.number);
  for (const auto& label : results->allLabels()) {
    if (label.isUnplaced || label.isDiagram) continue;
    const auto source = featureNumbers.constFind(label.layerID);
    if (source == featureNumbers.cend()) continue;
    const int number = source->value(label.featureId, 0);
    if (!number || label.labelText != QString::number(number)) continue;
    QPolygonF polygon;
    for (const auto& point : label.cornerPoints)
      polygon.append(toPaper.map(QPointF(point.x(), point.y())));
    QPainterPath text;
    text.addPolygon(polygon);
    if (!text.isEmpty() && onPaper.intersects(text)) keys.insert(entryKey(label.layerID, number));
  }
  return keys;
}

bool HeritageLayoutNumbers::acceptRenderedLabels(QgsLayoutItemMap* map, const QgsLabelingResults* results) {
  const auto keys = placedKeys(map, results);
  // Numbers come from the paper footprint in update(). PAL must not drop or
  // renumber them; update() already placed every badge clear of the others,
  // inside the frame and off the legend, so every on-page site stays visible.
  if (keys.isEmpty() && !m_exporting) return false;
  if (keys != m_visibleKeys) return false;
  if (!m_legendPending) return false;
  m_legendPending = false;
  return true;
}

void HeritageLayoutNumbers::followRenderedLabels(QgsLayoutItemMap* map) {
  if (m_tracking && m_followedMap == map) return;
  if (auto* numbers = numbersMapOf(m_followedMap))
    numbers->setProperty("ka_number_follow", false);
  if (m_followedMap && forMap(m_followedMap) == this)
    m_followedMap->setProperty("ka_number_owner", QVariant());
  for (const auto& connection : m_renderConnections) disconnect(connection);
  m_renderConnections.clear();
  m_followedMap = map;
  m_tracking = true;
  if (m_visibleKeys.isEmpty()) {
    for (const auto& entry : m_entries) {
      if (entry.number > 0) m_visibleKeys.insert(entryKey(entry.layerId, entry.number));
    }
  }
  ++m_placementRevision;
  m_previewBusy = false;
  m_previewDirty = false;
  if (!map) return;
  map->setProperty("ka_number_owner", QVariant::fromValue<QObject*>(this));
  m_renderConnections.append(connect(this, &QObject::destroyed, map, [map, owner = static_cast<QObject*>(this)]() {
    if (map->property("ka_number_owner").value<QObject*>() == owner)
      map->setProperty("ka_number_owner", QVariant());
  }));
  auto* timer = new QTimer(map);
  timer->setSingleShot(true);
  timer->setInterval(120);
  m_renderConnections.append(connect(map, &QgsLayoutItem::backgroundTaskCountChanged, this, [this, map](int count) {
    if (count <= 0) return;
    const bool interacting = map->property("ka_interacting").toBool();
    m_previewBusy = true;
    m_previewDirty = m_exporting || interacting;
    m_previewSignature = renderSignature(map);
  }));
  auto changed = [this]() { if (m_previewBusy) m_previewDirty = true; };
  m_renderConnections.append(connect(map, &QgsLayoutItemMap::extentChanged, this, changed));
  m_renderConnections.append(connect(map, &QgsLayoutItemMap::mapRotationChanged, this, changed));
  m_renderConnections.append(connect(map, &QgsLayoutItemMap::crsChanged, this, changed));
  m_renderConnections.append(connect(map, &QgsLayoutItemMap::layerStyleOverridesChanged, this, changed));
  // Moving the map or changing paper clips the footprint without changing
  // the geographic extent. Rebuild membership from that clip; legend movement
  // is unrelated.
  m_renderConnections.append(connect(timer, &QTimer::timeout, this, [this, map, timer]() {
    if (m_followedMap != map) return;
    if (map->property("ka_interacting").toBool() || m_exporting) { timer->start(); return; }
    map->invalidateCache();
    map->update();
    acceptRenderedLabels(map, numberPreviewResults(map));
  }));
  auto paperChanged = [this, map, changed, timer]() {
    changed();
    if (map->property("ka_interacting").toBool()) {
      timer->start();
      return;
    }
    update(map);
    timer->start();
  };
  m_renderConnections.append(connect(map, &QgsLayoutItem::sizePositionChanged, this, paperChanged));
  m_renderConnections.append(connect(map->layout()->pageCollection(), &QgsLayoutPageCollection::changed, this, paperChanged));
  m_renderConnections.append(connect(map, &QgsLayoutItemMap::previewRefreshed, this, [this, map, timer]() {
    const bool current = m_previewBusy && !m_previewDirty && !m_exporting
        && m_previewSignature == renderSignature(map);
    m_previewBusy = false;
    if (m_exporting) return;
    if (map->property("ka_interacting").toBool()) { timer->start(); return; }
    if (current) acceptRenderedLabels(map, numberPreviewResults(map));
    else if (m_previewDirty) {
      m_previewDirty = false;
      map->invalidateCache();
      map->update();
    }
  }));
  connectNumberPreview(map);
}

bool HeritageLayoutNumbers::exportPdf(QgsLayoutItemMap* map, QgsLayoutItemLegend* legend,
                                     const QString& path, double dpi, QString* error) {
  auto fail = [error](const QString& message) { if (error) *error = message; return false; };
  if (!map || !map->layout()) return fail(QStringLiteral("출력할 도면이 없습니다."));
  followRenderedLabels(map);
  const QScopedValueRollback<bool> exporting(m_exporting, true);
  auto* layout = map->layout();
  KaPdfExport::prepareLayout(layout);
  const double previousDpi = layout->renderContext().dpi();
  struct RestoreDpi {
    QgsLayout* layout;
    QgsLayoutItemMap* map;
    double dpi;
    ~RestoreDpi() {
      layout->renderContext().setDpi(dpi);
      map->invalidateCache();
      map->update();
    }
  } restore{layout, map, previousDpi};
  layout->renderContext().setDpi(dpi);
  QTemporaryDir temporary;
  if (!temporary.isValid()) return fail(QStringLiteral("PDF 임시 폴더를 만들지 못했습니다."));
  const QString draft = temporary.filePath(QStringLiteral("drawing.pdf"));
  auto syncLegend = [&]() {
    if (!legend) return;
    LayoutService::tuneSheetLegend(legend);
    applyLegend(legend);
    m_legendPending = false;
  };
  const QRectF legendBefore = legend ? legend->sceneBoundingRect() : QRectF();
  syncLegend();
  // Badges keep off a legend card that overlaps the map. Taking its final rows
  // can resize the card, so place the badges once more against the final card.
  // Entries do not change here, so the legend keeps that size afterwards.
  if (legend && legend->sceneBoundingRect() != legendBefore &&
      legend->sceneBoundingRect().intersects(map->sceneBoundingRect())) {
    if (!update(map, true)) return fail(m_error);
    syncLegend();
  }
  LayoutService::settleSheetLegendsForExport(layout);
  const QByteArray signature = renderSignature(map);
  // One pass: numbers are pins placed in update(), not PAL labels that the
  // exporter may drop, so there is nothing to retry.
  QgsLayoutExporter exporter(layout);
  if (exporter.exportToPdf(draft, KaPdfExport::sheetSettings(dpi)) != QgsLayoutExporter::Success)
    return fail(QStringLiteral("PDF 임시 출력에 실패했습니다."));
  if (signature != renderSignature(map))
    return fail(QStringLiteral("출력 중 도면이 변경되었습니다. 다시 내보내세요."));
  const auto* results = numberExportResults(map, exporter);
  if (!results && !m_entries.isEmpty())
    return fail(QStringLiteral("PDF 번호 배치 결과를 확인하지 못했습니다."));
  acceptRenderedLabels(map, results);
  if (!m_error.isEmpty()) return fail(m_error);
  QFile source(draft);
  QSaveFile destination(path);
  if (!source.open(QIODevice::ReadOnly) || !destination.open(QIODevice::WriteOnly))
    return fail(QStringLiteral("PDF 저장 파일을 열지 못했습니다."));
  while (!source.atEnd()) {
    const QByteArray bytes = source.read(1024 * 1024);
    if (source.error() != QFileDevice::NoError || destination.write(bytes) != bytes.size())
      return fail(QStringLiteral("PDF 파일을 저장하지 못했습니다."));
  }
  if (!destination.commit()) return fail(QStringLiteral("PDF 파일을 확정하지 못했습니다."));
  return true;
}

bool HeritageLayoutNumbers::update(QgsLayoutItemMap* map, bool force) {
  if (!map || !map->layout() || !map->layout()->project()) return false;
  QgsProject* project = map->layout()->project();
  const auto layers = numberedSourceLayers(map);
  const QgsGeometry nearSurvey = surveyNeighbourhood(project, map->crs());
  // 표시 축척이 1:5,000이면 QGIS 축척이 5000.3처럼 조금 넘어도 이름을 쓴다.
  const bool names = map->layout()->customProperty(namesPropertyKey(), false).toBool() && map->scale() > 0. &&
                     map->scale() < kNamesMaxScale + 0.5;
  QByteArray signature;
  QDataStream stream(&signature, QIODevice::WriteOnly);
  // Round the view so a repeated refresh of the same screen does not miss the
  // cache and rebuild every legend row. QGIS scale and footprint text drift.
  const QgsRectangle extent = map->extent();
  const QgsRectangle paperBox = mapFootprintOnPaper(map, m_exporting).boundingBox();
  auto whole = [](double value) {
    return std::isfinite(value) ? std::llround(value) : 0LL;
  };
  stream << map->uuid() << whole(extent.xMinimum()) << whole(extent.yMinimum())
         << whole(extent.xMaximum()) << whole(extent.yMaximum())
         << map->crs().authid() << whole(map->scale())
         << whole(paperBox.xMinimum()) << whole(paperBox.yMinimum())
         << whole(paperBox.xMaximum()) << whole(paperBox.yMaximum()) << names;
  const QgsRectangle nearBox = nearSurvey.boundingBox();
  stream << nearSurvey.isEmpty() << whole(nearBox.xMinimum()) << whole(nearBox.yMinimum())
         << whole(nearBox.xMaximum()) << whole(nearBox.yMaximum());
  // Badges keep off a legend card that overlaps the map, so its place on the
  // map is part of the placement input (0.1 mm steps).
  const QVector<QPolygonF> legendBlocks = LayoutBadgePlacer::legendBlocks(map, m_exporting);
  if (!legendBlocks.isEmpty()) {
    auto tenth = [](double value) { return std::isfinite(value) ? std::llround(value * 10.) : 0LL; };
    const QRectF frame = map->sceneBoundingRect();
    stream << tenth(frame.left()) << tenth(frame.top()) << tenth(frame.width()) << tenth(frame.height());
    for (const QPolygonF& block : legendBlocks) {
      const QRectF r = block.boundingRect();
      stream << tenth(r.left()) << tenth(r.top()) << tenth(r.width()) << tenth(r.height());
    }
  }
  // Do not serialize thousands of symbols merely to discover a cache hit.
  // Track source changes with lifetime-bound connections, including in-place
  // renderer check state changes (repaintRequested) and uncommitted edits.
  for (auto* layer : layers) {
    const QString id = layer->id();
    if (!m_layerRevisions.contains(id)) {
      m_layerRevisions.insert(id, 0);
      auto invalidate = [this, id]() {
        if (!m_applying) ++m_layerRevisions[id];
      };
      connect(layer, &QgsMapLayer::styleChanged, this, invalidate);
      connect(layer, &QgsMapLayer::dataChanged, this, invalidate);
      connect(layer, &QgsMapLayer::dataSourceChanged, this, invalidate);
      connect(layer, &QgsMapLayer::crsChanged, this, invalidate);
      connect(layer, &QgsMapLayer::nameChanged, this, invalidate);
      connect(layer, &QgsMapLayer::repaintRequested, this, [this, id]() {
        // A page move invalidates the layout preview and can emit
        // repaintRequested on the source. Membership is rebuilt from the
        // paper footprint in update() via sizePositionChanged, not here.
        if (!m_applying) ++m_layerRevisions[id];
      });
      connect(layer, &QObject::destroyed, this, [this, id]() {
        m_layerRevisions.remove(id);
        m_drawingSources.remove(id);
        m_signature.clear();
      });
      if (auto* vector = qobject_cast<QgsVectorLayer*>(layer)) {
        connect(vector, &QgsVectorLayer::updatedFields, this, invalidate);
        connect(vector, &QgsVectorLayer::featureAdded, this, invalidate);
        connect(vector, &QgsVectorLayer::featureDeleted, this, invalidate);
        connect(vector, &QgsVectorLayer::geometryChanged, this, invalidate);
        connect(vector, &QgsVectorLayer::attributeValueChanged, this, invalidate);
        connect(vector, &QgsVectorLayer::afterRollBack, this, invalidate);
      }
    }
    stream << id << m_layerRevisions.value(id) << layer->name()
           << layer->customProperty(QStringLiteral("ka_hgis/layout_numbers_visible"), true).toBool()
           << layer->customProperty(datasetPropertyKey()).toString();
  }
  // Number assignment follows project tree order, not paint order alone.
  for (auto* node : project->layerTreeRoot()->findLayers()) {
    if (!layers.contains(node->layer())) continue;
    stream << node->layerId();
    for (auto* ancestor = node->parent(); ancestor; ancestor = ancestor->parent())
      stream << ancestor->name();
  }
  const QString digest = QString::fromLatin1(QCryptographicHash::hash(signature, QCryptographicHash::Sha256).toHex());
  if (!force && digest == m_signature) return m_error.isEmpty();
  m_error.clear();
  m_names = names;
  QVector<Entry> entries;
  QVector<NumberPin> pins;
  QVector<QgsGeometry> pinAreas;
  QMap<QString, QString> overrides = map->layerStyleOverrides();
  // Remove our previous styles when a heritage layer is hidden or removed.
  for (auto it = m_drawingSources.cbegin(); it != m_drawingSources.cend(); ++it)
    overrides.remove(it.key());
  for (auto* layer : layers) {
    if (auto* vector = qobject_cast<QgsVectorLayer*>(layer); vector && datasetFor(vector))
      overrides.remove(layer->id());
  }
  QMap<QString, int> counters;
  // Project tree order is stable and agrees with the user's layer panel.
  for (QgsLayerTreeLayer* treeLayer : project->layerTreeRoot()->findLayers()) {
    auto* layer = qobject_cast<QgsVectorLayer*>(treeLayer->layer());
    if (!layer || !layers.contains(layer) || !layer->renderer()) continue;
    const auto dataset = datasetFor(layer);
    if (!dataset) continue;
    const QString datasetName = HeritageStyle::layerName(*dataset);
    auto& source = m_drawingSources[layer->id()];
    const auto sourceRevision = m_layerRevisions.value(layer->id());
    if (force || !source.layer || source.revision != sourceRevision) {
      source.layer.reset(layer->clone());
      source.revision = sourceRevision;
    }
    const auto& drawing = source.layer;
    if (!layer->customProperty(QStringLiteral("ka_hgis/layout_numbers_visible"), true).toBool()) {
      // Omitting the numeric override would expose the source's name labels.
      // Restore the source geometry renderer and disable labels only in this
      // layout clone, without scanning features or changing the main map.
      if (layer->renderer())
        drawing->setRenderer(layer->renderer()->clone());
      drawing->setLabelsEnabled(false);
      QgsMapLayerStyle style;
      style.readFromLayer(drawing.get());
      overrides.insert(layer->id(), style.xmlData());
      continue;
    }
    auto* sourceRenderer = layer->renderer();
    auto* categorized = dynamic_cast<QgsCategorizedSymbolRenderer*>(sourceRenderer);
    auto* singleRenderer = dynamic_cast<QgsSingleSymbolRenderer*>(sourceRenderer);
    const bool single = singleRenderer != nullptr;
    if (!categorized && !single) {
      m_error = QStringLiteral("'%1'의 도면 번호는 단일 심볼 또는 분류 스타일에서 사용할 수 있습니다.").arg(layer->name());
      return false;
    }
    QgsGeometry footprint = mapFootprintOnPaper(map, m_exporting);
    // Number only the paper near the survey; far sites are never even read.
    if (!nearSurvey.isEmpty()) footprint = footprint.intersection(nearSurvey);
    try {
      footprint.transform(QgsCoordinateTransform(map->crs(), layer->crs(), project->transformContext()));
    } catch (const QgsCsException& e) {
      m_error = QStringLiteral("'%1'의 도면 범위를 변환하지 못했습니다: %2").arg(layer->name(), e.what());
      return false;
    }
    QMap<QString, Site> sites;
    // The field chosen in 「표시할 항목」 names the sites, as on the map.
    QString nameField = layer->customProperty(QStringLiteral("ka_hgis/label_field")).toString();
    if (layer->fields().lookupField(nameField) < 0) nameField = HeritageImport::chooseNameField(layer);
    QgsExpressionContext classContext;
    if (categorized && layer->fields().lookupField(categorized->classAttribute()) < 0)
      classContext.appendScopes(QgsExpressionContextUtils::globalProjectLayerScopes(layer));
    if (layer->isInScaleRange(map->scale()) && !footprint.isEmpty()) {
      QgsFeatureRequest request;
      request.setFilterRect(footprint.boundingBox());
      QgsAttributeList subset;
      bool canSubset = true;
      if (!nameField.isEmpty()) {
        const int nameIndex = layer->fields().lookupField(nameField);
        if (nameIndex >= 0)
          subset << nameIndex;
      }
      if (categorized) {
        const int classIndex = layer->fields().lookupField(categorized->classAttribute());
        if (classIndex >= 0)
          subset << classIndex;
        else
          canSubset = false;
      }
      if (canSubset && !subset.isEmpty())
        request.setSubsetOfAttributes(subset);
      // Heritage zones carry up to millions of vertices: test against a
      // prepared footprint and cut each zone to the footprint box before the
      // exact intersection (clipped() is fast but may return an invalid ring).
      const QgsRectangle footprintBox = footprint.boundingBox();
      std::unique_ptr<QgsGeometryEngine> footprintEngine(QgsGeometry::createGeometryEngine(footprint.constGet()));
      footprintEngine->prepareGeometry();
      auto features = layer->getFeatures(request);
      QgsFeature feature;
      while (features.nextFeature(feature)) {
        if (!feature.hasGeometry()) continue;
        const QgsGeometry geom = feature.geometry();
        if (!footprintEngine->intersects(geom.constGet())) continue;
        QgsGeometry clipped;
        if (!footprintBox.contains(geom.boundingBox()))
          clipped = QgsGeometry(geom).clipped(footprintBox).intersection(footprint);
        if (clipped.isEmpty()) clipped = geom.intersection(footprint);
        if (clipped.isEmpty()) {
          // Points often lose the intersection product even when they sit
          // on the page. Keep the site and pin the number to the feature.
          clipped = geom.type() == Qgis::GeometryType::Point ? geom : geom.pointOnSurface();
          if (clipped.isEmpty() ||
              !(footprint.intersects(clipped) || footprint.contains(clipped)))
            clipped = footprint.nearestPoint(geom);
        }
        if (clipped.isEmpty()) continue;
        QString key;
        QString name;
        int index = 0;
        const QgsSymbol* symbol = nullptr;
        if (categorized) {
          const QgsCategoryList& categories = categorized->categories();
          index = categoryIndexForClassValue(
              categories, classAttributeValue(layer, categorized->classAttribute(), feature, classContext));
          if (index < 0) continue;
          symbol = categories.at(index).symbol();
          if (!symbol) continue;
          key = QString::number(index).rightJustified(10, QLatin1Char('0'));
          name = categories.at(index).label();
        } else {
          name = nameField.isEmpty() ? QString() : feature.attribute(nameField).toString().trimmed();
          if (name.isEmpty()) name = HeritageStyle::unnamedLabel();
          key = name;
          symbol = singleRenderer->symbol();
          if (!symbol) continue;
        }
        auto& site = sites[key];
        if (site.name.isEmpty()) {
          site.name = name;
          site.index = index;
          site.color = symbol->color().isValid() ? symbol->color() : HeritageStyle::color(*dataset);
        }
        const QString id = QString::number(feature.id());
        site.ids.append(id);
        if (clipped.type() == Qgis::GeometryType::Polygon) site.pieces.append(clipped);
        site.values.append(QVariant::fromValue(feature.id()));
        const double weight = std::max({clipped.area(), clipped.length(), 1.});
        if (weight > site.labelWeight) {
          const QgsGeometry mark = clipped.pointOnSurface();
          if (mark.isEmpty()) continue;
          const QgsPointXY xy = mark.asPoint();
          site.labelWeight = weight;
          site.labelId = id;
          site.labelX = xy.x();
          site.labelY = xy.y();
        }
      }
    }
    {
      // Each site's own spot in map coordinates; the badges are placed together below.
      QgsCoordinateTransform toMap(layer->crs(), map->crs(), project->transformContext());
      for (auto it = sites.begin(); it != sites.end(); ++it) {
        it->originX = it->labelX;
        it->originY = it->labelY;
        try {
          const QgsPointXY origin = toMap.transform(QgsPointXY(it->labelX, it->labelY));
          it->originX = origin.x();
          it->originY = origin.y();
          if (!it->pieces.isEmpty()) {
            it->area = QgsGeometry::unaryUnion(it->pieces);
            it->area.transform(toMap);
          }
        } catch (const QgsCsException&) {
          it->area = QgsGeometry();
        }
      }
    }
    if (categorized) {
      const QgsCategoryList& all = categorized->categories();
      QgsCategoryList visibleCategories;
      visibleCategories.reserve(sites.size());
      for (auto it = sites.begin(); it != sites.end(); ++it) {
        if (it->index < 0 || it->index >= all.size())
          continue;
        it->originalIndex = it->index;
        QgsRendererCategory category = all.at(it->index);
        if (QgsSymbol* symbol = category.symbol()) {
          prepareLayoutSymbol(symbol, *dataset);
          if (symbol->color().isValid())
            it->color = symbol->color();
        }
        it->index = visibleCategories.size();
        visibleCategories.append(category);
      }
      if (!nearSurvey.isEmpty()) {
        // Unnumbered sites farther from the survey keep their colour; legend
        // rows follow the numbered indices only.
        QSet<int> numbered;
        for (const Site& site : std::as_const(sites)) numbered.insert(site.originalIndex);
        for (int i = 0; i < all.size(); ++i) {
          if (numbered.contains(i)) continue;
          QgsRendererCategory category = all.at(i);
          if (QgsSymbol* symbol = category.symbol()) prepareLayoutSymbol(symbol, *dataset);
          visibleCategories.append(category);
        }
      }
      drawing->setRenderer(new QgsCategorizedSymbolRenderer(categorized->classAttribute(), visibleCategories));
    } else if (singleRenderer) {
      std::unique_ptr<QgsSymbol> prepared(singleRenderer->symbol() ? singleRenderer->symbol()->clone() : nullptr);
      prepareLayoutSymbol(prepared.get(), *dataset);
      const QColor color = prepared && prepared->color().isValid()
                               ? prepared->color()
                               : HeritageStyle::layoutColor(*dataset, HeritageStyle::color(*dataset));
      for (auto it = sites.begin(); it != sites.end(); ++it) {
        it->color = color;
        if (prepared)
          it->symbol.reset(prepared->clone());
      }
      if (prepared)
        drawing->setRenderer(new QgsSingleSymbolRenderer(prepared.release()));
    }
    QgsCategoryList fallbackCategories;
    for (auto it = sites.cbegin(); it != sites.cend(); ++it) {
      const Site& site = it.value();
      const int number = ++counters[datasetName];
      const int index = single ? fallbackCategories.size() : site.index;
      QSet<qint64> featureIds;
      for (const auto& id : site.ids) featureIds.insert(id.toLongLong());
      entries.append({layer->id(), datasetName, site.name, number, index, site.color, featureIds});
      // 같은 이름은 번호 하나. 점 하나로 그리고, 번호 자리는 아래에서 한꺼번에 정한다.
      const QSizeF nameBox = names ? nameBoxMm(site.name) : QSizeF();
      pins.append(NumberPin{site.originX, site.originY, site.originX, site.originY,
                            names ? nameBox.height() : circleSize(number), nameBox.width(), number, site.labelId.toLongLong(), site.name, layer->id(),
                            site.color.name(QColor::HexArgb), inkFor(site.color).name()});
      pinAreas.append(site.area);
      if (single) fallbackCategories.append(QgsRendererCategory(site.values, site.symbol->clone(), site.name));
    }
    if (single && !sites.isEmpty() && sites.first().symbol) {
      // The drawing clone alone expands a single symbol into visible site rows.
      fallbackCategories.append(QgsRendererCategory(QVariant(), sites.first().symbol->clone(), HeritageStyle::unnamedLabel()));
      drawing->setRenderer(new QgsCategorizedSymbolRenderer(QStringLiteral("$id"), fallbackCategories));
    }
    if (drawing->labeling() && !sites.isEmpty()) {
      QgsPalLayerSettings labels = drawing->labeling()->settings();
      QgsTextFormat format = labels.format();
      QgsTextBackgroundSettings background = format.background();
      background.setFillColor(sites.first().color);
      format.setBackground(background);
      labels.setFormat(format);
      QString numberCase = QStringLiteral("CASE");
      for (const Entry& entry : entries) {
        if (entry.layerId != layer->id() || entry.number <= 0) continue;
        for (qint64 id : entry.featureIds)
          numberCase += QStringLiteral(" WHEN $id = %1 THEN %2").arg(id).arg(entry.number);
      }
      labels.fieldName = numberCase + QStringLiteral(" ELSE NULL END");
      labels.isExpression = true;
      labels.scaleVisibility = false;
      applyHeritageNumberCallout(labels);
      drawing->setLabeling(new QgsVectorLayerSimpleLabeling(labels));
      // 번호는 핀 레이어만 그린다. 도면 복제본의 글자를 켜면 같은 번호가 한 번 더 나온다.
      drawing->setLabelsEnabled(false);
    } else {
      drawing->setLabelsEnabled(false);
    }
    drawing->setCustomProperty(QStringLiteral("rendering/renderAboveLabels"), false);
    QgsMapLayerStyle style;
    style.readFromLayer(drawing.get());
    overrides.insert(layer->id(), style.xmlData());
  }
  // All badges of the sheet at once: each on its site, moved only to clear another.
  QVector<QgsPointXY> origins;
  QVector<double> sizes;
  QVector<QSizeF> boxes;
  double widestMm = 0.;
  for (const NumberPin& pin : pins) {
    origins.append(QgsPointXY(pin.originX, pin.originY));
    sizes.append(pin.size);
    boxes.append(QSizeF(pin.width + kNumberGapMm, pin.size + kNumberGapMm));
    // Name rows step by their height; badges by the widest circle.
    widestMm = std::max(widestMm, pin.size);
  }
  // One placer for the whole sheet: badges of different layers never cover
  // each other, and all stay on the visible map paper and off the legend card.
  bool invertible = false;
  const QTransform mapToScene = map->layoutToMapCoordsTransform().inverted(&invertible);
  LayoutBadgePlacer placer(
      invertible ? mapToScene : QTransform(),
      invertible ? LayoutBadgePlacer::withoutBlocks(visibleMapOnPaper(map, m_exporting), legendBlocks)
                 : QPainterPath(),
      ((widestMm + kNumberGapMm) / 1000.0) * (map->scale() > 0. ? map->scale() : 5000.));
  const QVector<QgsPointXY> placed =
      names ? placer.placeAllBoxes(origins, boxes, pinAreas) : placer.placeAll(origins, sizes, pinAreas);
  for (int i = 0; i < pins.size(); ++i) {
    pins[i].x = placed.at(i).x();
    pins[i].y = placed.at(i).y();
    // The site point kept with the badge: the badge itself when it sits on its
    // own area, else the nearest edge of the area.
    const QgsGeometry& area = pinAreas.at(i);
    if (area.isEmpty()) continue;
    const QgsGeometry at = QgsGeometry::fromPointXY(placed.at(i));
    const QgsPointXY anchor = area.intersects(at) ? placed.at(i) : area.nearestPoint(at).asPoint();
    pins[i].originX = anchor.x();
    pins[i].originY = anchor.y();
  }
  m_entries = std::move(entries);
  m_overrides = std::move(overrides);
  m_legendPending = false;
  m_signature = digest;
  ++m_revision;
  m_visibleKeys.clear();
  for (const auto& entry : m_entries) {
    if (entry.number > 0) m_visibleKeys.insert(entryKey(entry.layerId, entry.number));
  }
  ++m_placementRevision;
  if (m_tracking && m_previewBusy) m_previewDirty = true;
  emit visibleEntriesChanged();
  const QScopedValueRollback<bool> applying(m_applying, true);
  applyBaseStyleOverrides(map);
  publishNumberPins(map, pins);
  map->invalidateCache();
  raiseAboveGeometries(map);
  return true;
}

void HeritageLayoutNumbers::publishNumberPins(QgsLayoutItemMap* map, const QVector<NumberPin>& pins) {
  const QString auth = map && map->crs().isValid() && !map->crs().authid().isEmpty()
                           ? map->crs().authid()
                           : QStringLiteral("EPSG:5186");
  const bool created = !m_numberLayer || m_numberLayer->crs().authid() != auth;
  if (created) {
    delete m_numberLayer;
    m_numberLayer = new QgsVectorLayer(
        QStringLiteral("Point?crs=%1&field=num:integer&field=nm:string(80)&field=layer:string(64)"
                       "&field=ox:double&field=oy:double&field=size:double&field=w:double"
                       "&field=fill:string(16)&field=ink:string(16)&field=fid:string(32)")
            .arg(auth),
        QStringLiteral("도면번호"), QStringLiteral("memory"));
    m_numberLayer->setLabelsEnabled(false);
    m_numberLayer->setCustomProperty(QStringLiteral("ka_hgis/omit_sheet_legend"), true);
  }
  if (created || m_pinsShowNames != m_names) {
    m_pinsShowNames = m_names;
    auto marker = QgsMarkerSymbol::createSimple({
        {QStringLiteral("name"), QStringLiteral("circle")},
        {QStringLiteral("color"), QStringLiteral("#888888")},
        {QStringLiteral("outline_color"), QStringLiteral("#000000")},
        {QStringLiteral("outline_width"), QStringLiteral("0.15")},
        {QStringLiteral("outline_width_unit"), QStringLiteral("MM")},
        {QStringLiteral("size"), QStringLiteral("2.99")},
        {QStringLiteral("size_unit"), QStringLiteral("MM")},
    });
    if (auto* circle = marker->symbolLayer(0)) {
      circle->setDataDefinedProperty(QgsSymbolLayer::Property::FillColor, QgsProperty::fromField(QStringLiteral("fill")));
      circle->setDataDefinedProperty(QgsSymbolLayer::Property::Size, QgsProperty::fromField(QStringLiteral("size")));
    }
    auto* glyph = new QgsFontMarkerSymbolLayer(QStringLiteral("Malgun Gothic"), QStringLiteral("1"), 1.82, Qt::black);
    glyph->setFontStyle(QStringLiteral("Bold"));
    glyph->setSizeUnit(Qgis::RenderUnit::Millimeters);
    glyph->setDataDefinedProperty(QgsSymbolLayer::Property::Character, QgsProperty::fromField(QStringLiteral("num")));
    glyph->setDataDefinedProperty(QgsSymbolLayer::Property::FillColor, QgsProperty::fromField(QStringLiteral("ink")));
    // No leader lines: a badge sits on its site or on the nearest free spot
    // next to it (2026-10-19 현장 요청 「선을 긋지 말고 제위치에」).
    marker->appendSymbolLayer(glyph);
    if (m_names) {
      // 이름은 상자 없이 검은 글자에 흰 테두리: 흰 글자를 흰 선으로 굵게 먼저 그리고 그 위에 검은 글자.
      QgsSymbolLayerList text;
      for (const bool halo : {true, false}) {
        auto* glyphs = new QgsFontMarkerSymbolLayer(QStringLiteral("Malgun Gothic"), QStringLiteral("가"), kNameSizeMm,
                                                    halo ? Qt::white : Qt::black);
        glyphs->setSizeUnit(Qgis::RenderUnit::Millimeters);
        glyphs->setDataDefinedProperty(QgsSymbolLayer::Property::Character, QgsProperty::fromField(QStringLiteral("nm")));
        if (halo) {
          glyphs->setStrokeColor(Qt::white);
          glyphs->setStrokeWidth(kNameHaloMm);
          glyphs->setStrokeWidthUnit(Qgis::RenderUnit::Millimeters);
          glyphs->setPenJoinStyle(Qt::RoundJoin);
        }
        text << glyphs;
      }
      marker = std::make_unique<QgsMarkerSymbol>(text);
    }
    m_numberLayer->setRenderer(new QgsSingleSymbolRenderer(marker.release()));
  }
  if (!m_numberLayer->isValid()) return;
  m_numberLayer->startEditing();
  QgsFeatureIds gone;
  QgsFeature existing;
  auto have = m_numberLayer->getFeatures();
  while (have.nextFeature(existing)) gone.insert(existing.id());
  if (!gone.isEmpty()) m_numberLayer->deleteFeatures(gone);
  for (const NumberPin& pin : pins) {
    QgsFeature feature(m_numberLayer->fields());
    feature.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(pin.x, pin.y)));
    feature.setAttribute(QStringLiteral("num"), pin.number);
    feature.setAttribute(QStringLiteral("nm"), pin.name);
    feature.setAttribute(QStringLiteral("layer"), pin.layerId);
    feature.setAttribute(QStringLiteral("ox"), pin.originX);
    feature.setAttribute(QStringLiteral("oy"), pin.originY);
    feature.setAttribute(QStringLiteral("size"), pin.size);
    feature.setAttribute(QStringLiteral("w"), pin.width);
    feature.setAttribute(QStringLiteral("fill"), pin.fill);
    feature.setAttribute(QStringLiteral("ink"), pin.ink);
    feature.setAttribute(QStringLiteral("fid"), QString::number(pin.sourceId));
    m_numberLayer->addFeature(feature);
  }
  m_numberLayer->commitChanges();
  m_numberLayer->updateExtents();
}

void HeritageLayoutNumbers::raiseAboveGeometries(QgsLayoutItemMap* base) {
  if (!base || !base->layout()) return;
  auto* layout = base->layout();
  QgsLayoutItemMap* overlay = layoutMapById(layout, QStringLiteral("ka_map_above"));
  QgsLayoutItemMap* numbers = layoutMapById(layout, QStringLiteral("ka_map_numbers"));
  if (!m_numberLayer || m_numberLayer->featureCount() <= 0) {
    if (numbers) layout->removeLayoutItem(numbers);
    return;
  }
  const bool created = numbers == nullptr;
  if (created) {
    numbers = new QgsLayoutItemMap(layout);
    numbers->setId(QStringLiteral("ka_map_numbers"));
    layout->addLayoutItem(numbers);
  }
  numbers->setFrameEnabled(false);
  numbers->setBackgroundEnabled(false);
  numbers->setKeepLayerSet(true);
  numbers->setFollowVisibilityPreset(false);
  QList<QgsMapLayer*> labelLayers{m_numberLayer};
  QgsProject* project = layout->project();
  QMap<QString, QString> labelStyles;
  for (auto it = m_overrides.cbegin(); it != m_overrides.cend(); ++it) {
    auto* vector = qobject_cast<QgsVectorLayer*>(project ? project->mapLayer(it.key()) : nullptr);
    if (!vector) {
      labelStyles.insert(it.key(), it.value());
      continue;
    }
    // update() left this override on the cached drawing copy; reuse it
    // instead of opening the source again and parsing the style XML.
    const auto cached = m_drawingSources.constFind(it.key());
    const bool reuse = cached != m_drawingSources.cend() && cached->layer;
    std::shared_ptr<QgsVectorLayer> drawing = reuse ? cached->layer : std::shared_ptr<QgsVectorLayer>(vector->clone());
    if (!reuse) QgsMapLayerStyle(it.value()).writeToLayer(drawing.get());
    std::unique_ptr<QgsFeatureRenderer> kept(reuse && drawing->renderer() ? drawing->renderer()->clone() : nullptr);
    if (QgsFeatureRenderer* renderer = drawing->renderer()) {
      QgsRenderContext context;
      const auto symbols = renderer->symbols(context);
      for (QgsSymbol* symbol : symbols)
        if (symbol) symbol->setOpacity(0);
    }
    QgsMapLayerStyle style;
    style.readFromLayer(drawing.get());
    labelStyles.insert(it.key(), style.xmlData());
    if (kept) drawing->setRenderer(kept.release());
  }
  numbers->setLayers(labelLayers);
  numbers->setKeepLayerStyles(true);
  numbers->setLayerStyleOverrides(labelStyles);
  numbers->setCrs(base->crs());
  numbers->setMapRotation(base->mapRotation());
  numbers->attemptSetSceneRect(base->rect().translated(base->pos()));
  numbers->zoomToExtent(base->extent());
  numbers->setExtent(base->extent());
  if (overlay) {
    for (int i = 0; i < 20 && numbers->zValue() <= overlay->zValue(); ++i)
      layout->raiseItem(numbers, true);
  } else if (created) {
    layout->moveItemToBottom(numbers, true);
    layout->raiseItem(numbers, true);
  }
  layout->updateZValues(false);
  if (!base->property("ka_interacting").toBool() || created)
    numbers->invalidateCache();
  connectNumberPreview(base);
}

void HeritageLayoutNumbers::applyLegend(QgsLayoutItemLegend* legend) const {
  if (!legend || !legend->model() || !legend->model()->rootGroup() || m_applying) return;
  const QScopedValueRollback<bool> applying(m_applying, true);
  auto* model = legend->model();
  const QString stamp = QString::number(reinterpret_cast<quintptr>(this)) + QLatin1Char(':') + QString::number(m_revision)
      + QLatin1Char(':') + QString::number(m_placementRevision);
  const QString key = QStringLiteral("ka_hgis/layout_numbers_revision");
  bool current = legend->customProperty(key).toString() == stamp && model->layerStyleOverrides() == m_overrides;
  bool needsMapFilter = m_entries.isEmpty();
  for (auto* node : model->rootGroup()->findLayers()) {
    if (auto* layer = qobject_cast<QgsVectorLayer*>(node->layer())) {
      if (layer == m_numberLayer || layer->name() == QLatin1String("layout_blank")) continue;
      if (datasetFor(layer)) current = current && node->customProperty(key).toString() == stamp;
      else needsMapFilter = true;
    }
  }
  // Heritage rows are already limited to the on-paper map. Other vectors
  // (e.g. geology/soil) still need QGIS symbol hit testing.
  if (legend->legendFilterByMapEnabled() != needsMapFilter)
    legend->setLegendFilterByMapEnabled(needsMapFilter);
  if (needsMapFilter || layoutMapById(legend->layout(), QStringLiteral("ka_map_above")))
    bindLegendFilterMaps(legend);
  for (auto* node : model->rootGroup()->findLayers()) {
    auto* layer = qobject_cast<QgsVectorLayer*>(node->layer());
    if (!layer || !datasetFor(layer)) continue;
    int numbered = 0;
    for (const auto& entry : m_entries)
      if (entry.layerId == layer->id() && entry.number > 0) ++numbered;
    // Names on the map: one row per dataset instead of one per number.
    if (model->layerLegendNodes(node).size() != (m_names ? std::min(numbered, 1) : numbered)) current = false;
  }
  if (current) return;
  // Numbered rows already passed the paper-footprint intersection test.
  if (!m_entries.isEmpty()) {
    // This service applies the complete override snapshot below. Letting the
    // linked map also reload it rebuilds all rows once before our own update.
    if (auto* linked = legend->linkedMap())
      QObject::disconnect(linked, &QgsLayoutItemMap::layerStyleOverridesChanged, legend, nullptr);
  }
  bool rebuilt = false;
  QSet<QString> legendLayerIds;
  for (auto* node : model->rootGroup()->findLayers()) {
    if (node->layer()) legendLayerIds.insert(node->layer()->id());
  }
  for (auto* node : model->rootGroup()->findLayers()) {
    auto* layer = qobject_cast<QgsVectorLayer*>(node->layer());
    if (!layer || layer == m_numberLayer || !datasetFor(layer)) continue;
    const auto dataset = datasetFor(layer);
    const QString datasetName = HeritageStyle::layerName(*dataset);
    // Batch badge properties; otherwise every property refreshes the legend.
    QSignalBlocker nodeSignals(node);
    QList<int> order;
    QString series;
    for (const Entry& entry : m_entries) {
      if (entry.number <= 0) continue;
      const bool idMatch = entry.layerId == layer->id();
      const bool datasetMatch = !legendLayerIds.contains(entry.layerId) && entry.dataset == datasetName;
      if (!idMatch && !datasetMatch) continue;
      if (m_tracking && !m_visibleKeys.contains(entryKey(entry.layerId, entry.number))) continue;
      order.append(entry.legendIndex);
      series += QString::number(m_names ? 0 : entry.number) + QLatin1Char('\t')
          + (m_names ? datasetName : entry.name) + QLatin1Char('\t')
          + QString::number(entry.legendIndex) + QLatin1Char('\t')
          + entry.color.name(QColor::HexArgb) + QLatin1Char('\n');
      if (m_names) break;
    }
    const QString seriesKey = QStringLiteral("ka_hgis/legend_series");
    if (order.isEmpty()) {
      nodeSignals.unblock();
      if (!m_visibleKeys.isEmpty()) {
        if (auto* parent = qobject_cast<QgsLayerTreeGroup*>(node->parent())) parent->removeChildNode(node);
        rebuilt = true;
      }
      continue;
    }
    const auto existingNodes = model->layerLegendNodes(node);
    QString shown;
    for (auto* legendNode : existingNodes) {
      if (!legendNode) continue;
      shown += legendNode->data(Qt::DisplayRole).toString() + QLatin1Char('\n');
    }
    QString wanted;
    for (const QString& row : series.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
      const QStringList parts = row.split(QLatin1Char('\t'));
      if (parts.size() >= 2) wanted += parts.at(1) + QLatin1Char('\n');
    }
    int customBadges = 0;
    for (auto* legendNode : existingNodes) {
      auto* symbolNode = dynamic_cast<QgsSymbolLegendNode*>(legendNode);
      if (symbolNode && symbolNode->customSymbol()) ++customBadges;
    }
    // 같은 번호 배지면 속성을 다시 쓰지 않는다. 쓰면 범례 노드가 바뀐다.
    if (customBadges == order.size() &&
        (node->customProperty(seriesKey).toString() == series || shown == wanted))
      continue;
    auto stampBadges = [&]() {
      for (const Entry& entry : m_entries) {
        if (entry.number <= 0) continue;
        const bool idMatch = entry.layerId == layer->id();
        const bool datasetMatch = !legendLayerIds.contains(entry.layerId) && entry.dataset == datasetName;
        if (!idMatch && !datasetMatch) continue;
        if (m_tracking && !m_visibleKeys.contains(entryKey(entry.layerId, entry.number))) continue;
        if (m_names) {
          // 이름을 쓴 도면의 범례는 번호 없이 유적 종류와 색만.
          auto swatch = QgsMarkerSymbol::createSimple({
              {QStringLiteral("name"), QStringLiteral("square")},
              {QStringLiteral("color"), entry.color.name(QColor::HexArgb)},
              {QStringLiteral("outline_color"), QStringLiteral("#000000")},
              {QStringLiteral("outline_width"), QStringLiteral("0.15")},
              {QStringLiteral("outline_width_unit"), QStringLiteral("MM")},
              {QStringLiteral("size"), QStringLiteral("3")}});
          QgsMapLayerLegendUtils::setLegendNodeCustomSymbol(node, entry.legendIndex, swatch.get());
          QgsMapLayerLegendUtils::setLegendNodeUserLabel(node, entry.legendIndex, datasetName);
          QgsMapLayerLegendUtils::setLegendNodeSymbolSize(node, entry.legendIndex, QSizeF(3., 3.));
          break;
        }
        auto marker = QgsMarkerSymbol::createSimple({
            {QStringLiteral("name"), QStringLiteral("circle")},
            {QStringLiteral("color"), entry.color.name(QColor::HexArgb)},
            {QStringLiteral("outline_color"), QStringLiteral("#000000")},
            {QStringLiteral("outline_width"), QStringLiteral("0.15")},
            {QStringLiteral("outline_width_unit"), QStringLiteral("MM")},
            {QStringLiteral("size"), QString::number(circleSize(entry.number))}});
        auto* number = new QgsFontMarkerSymbolLayer(QStringLiteral("Malgun Gothic"),
            QString::number(entry.number), 1.82, inkFor(entry.color));
        number->setFontStyle(QStringLiteral("Bold"));
        number->setSizeUnit(Qgis::RenderUnit::Millimeters);
        marker->appendSymbolLayer(number);
        QgsMapLayerLegendUtils::setLegendNodeCustomSymbol(node, entry.legendIndex, marker.get());
        QgsMapLayerLegendUtils::setLegendNodeUserLabel(node, entry.legendIndex, entry.name);
        QgsMapLayerLegendUtils::setLegendNodeSymbolSize(node, entry.legendIndex, QSizeF(circleSize(entry.number), circleSize(entry.number)));
      }
      QgsMapLayerLegendUtils::setLegendNodeOrder(node, order);
    };
    if (model->layerStyleOverrides() != m_overrides) model->setLayerStyleOverrides(m_overrides);
    stampBadges();
    QgsLegendRenderer::setNodeLegendStyle(node, Qgis::LegendComponent::Hidden);
    model->refreshLayerLegend(node);
    // tuneSheetLegend / filter-by-map rebuilds from the single-symbol source
    // first. Re-stamp after the override categories exist so rows keep 1,2,3…
    // not the first site's "1". https://qgis.org/pyqgis/master/core/QgsMapLayerLegendUtils.html
    stampBadges();
    node->setCustomProperty(seriesKey, series);
    node->setCustomProperty(key, stamp);
    rebuilt = true;
  }
  if (rebuilt)
    legend->setCustomProperty(key, stamp);
  if (!legend->property("ka_hgis/number_legend_follow").toBool()) {
    legend->setProperty("ka_hgis/number_legend_follow", true);
    QObject::connect(model, &QgsLayerTreeModel::hitTestCompleted, legend, [this, legend]() {
      if (m_applying) return;
      applyLegend(legend);
    });
  }
  if (!rebuilt) return;
  // Number badges already know which sites are on the sheet. The spatial hit
  // test is only for other vectors (geology/soil) and only when the linked
  // map extent or that layer set actually changed.
  if (needsMapFilter) {
    QStringList ids;
    for (auto* node : model->rootGroup()->findLayers()) {
      auto* layer = qobject_cast<QgsVectorLayer*>(node->layer());
      if (!layer || layer == m_numberLayer || datasetFor(layer)) continue;
      ids.append(layer->id());
    }
    ids.sort();
    QString filterKey = ids.join(QLatin1Char('|'));
    if (auto* linked = legend->linkedMap()) {
      const QgsRectangle extent = linked->extent();
      filterKey += QStringLiteral("@%1,%2,%3,%4,%5")
                       .arg(extent.xMinimum(), 0, 'f', 2)
                       .arg(extent.yMinimum(), 0, 'f', 2)
                       .arg(extent.xMaximum(), 0, 'f', 2)
                       .arg(extent.yMaximum(), 0, 'f', 2)
                       .arg(linked->scale(), 0, 'f', 1);
    }
    const QString filterProp = QStringLiteral("ka_hgis/legend_map_filter_key");
    if (legend->customProperty(filterProp).toString() != filterKey) {
      legend->setCustomProperty(filterProp, filterKey);
      legend->updateFilterByMap(false);
    }
  }
  LayoutService::flowSheetLegend(legend);
  legend->update();
}
