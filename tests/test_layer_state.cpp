#include <QtTest>
#include <QPointer>
#include <QTemporaryDir>
#include <qgsapplication.h>
#include <qgsproject.h>
#include <qgslayertree.h>
#include <qgslayertreeregistrybridge.h>
#include <qgsmapcanvas.h>
#include <qgsmaprenderersequentialjob.h>
#include <qgsvectorlayer.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorfilewriter.h>
#include <qgssinglesymbolrenderer.h>
#include <qgsfillsymbol.h>
#include <qgslinesymbol.h>
#include <qgscoordinatetransform.h>
#include <qgsvectorlayerlabeling.h>
#include <qgspallabeling.h>
#include <qgsexpression.h>
#include <qgsexpressioncontext.h>
#include "core/LayerOps.h"

class LayerStateTest : public QObject {
  Q_OBJECT
private slots:
  void hiddenAncestorExcludesLayersAndInvalidatesOverlayCache() {
    QgsProject project;
    auto* root = project.layerTreeRoot();
    auto* references = root->addGroup(QStringLiteral("참조 지도"));
    auto* subgroup = references->addGroup(QStringLiteral("주변유적"));
    auto* polygon = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187"),
                                       QStringLiteral("유적"), QStringLiteral("memory"));
    polygon->setRenderer(new QgsSingleSymbolRenderer(QgsFillSymbol::createSimple(
        {{QStringLiteral("color"), QStringLiteral("255,0,0,255")},
         {QStringLiteral("outline_color"), QStringLiteral("0,0,0,255")}}).release()));
    auto* label = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5187"),
                                     QStringLiteral("라벨"), QStringLiteral("memory"));
    QgsPalLayerSettings settings;
    settings.fieldName = QStringLiteral("'label'");
    settings.isExpression = true;
    label->setLabeling(new QgsVectorLayerSimpleLabeling(settings));
    label->setLabelsEnabled(true);
    project.addMapLayer(polygon, false);
    project.addMapLayer(label, false);
    auto* polygonNode = subgroup->addLayer(polygon);
    auto* labelNode = root->addLayer(label);
    QVERIFY(LayerOps::visibleLayersPaintOrder(&project).contains(polygon));
    QVERIFY(LayerOps::layersDrawnAboveLabels(&project).contains(polygon));
    references->setItemVisibilityChecked(false);
    QVERIFY(polygonNode->itemVisibilityChecked());
    QVERIFY(!polygonNode->isVisible());
    QVERIFY(!LayerOps::isLayerVisible(&project, polygon->name()));
    QVERIFY(!LayerOps::visibleLayersPaintOrder(&project).contains(polygon));
    QVERIFY(!LayerOps::layersDrawnAboveLabels(&project).contains(polygon));
    labelNode->setItemVisibilityChecked(false);
    QVERIFY(LayerOps::visibleLayersPaintOrder(&project).isEmpty());
    references->setItemVisibilityChecked(true);
    QCOMPARE(LayerOps::visibleLayersPaintOrder(&project), QList<QgsMapLayer*>{polygon});
    QVERIFY(!labelNode->itemVisibilityChecked());
    subgroup->setItemVisibilityChecked(false);
    QVERIFY(LayerOps::visibleLayersPaintOrder(&project).isEmpty());
    QVERIFY(LayerOps::toggleLayerVisibility(&project, nullptr, polygon->name(), true));
    QVERIFY(subgroup->isVisible());
    QVERIFY(LayerOps::isLayerVisible(&project, polygon->name()));
    QVERIFY(!labelNode->itemVisibilityChecked());
    labelNode->setItemVisibilityChecked(true);
    QVERIFY(LayerOps::layersDrawnAboveLabels(&project).contains(polygon));
  }

  void referenceMapsStayOutOfAboveLabelsPass() {
    QgsProject project;
    auto* labels = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5186"),
                                      QStringLiteral("지번"), QStringLiteral("memory"));
    QgsPalLayerSettings settings;
    settings.fieldName = QStringLiteral("'n'");
    settings.isExpression = true;
    labels->setLabeling(new QgsVectorLayerSimpleLabeling(settings));
    labels->setLabelsEnabled(true);
    auto* survey = new QgsVectorLayer(QStringLiteral("LineString?crs=EPSG:5186"),
                                      QStringLiteral("조사선"), QStringLiteral("memory"));
    survey->setRenderer(new QgsSingleSymbolRenderer(QgsLineSymbol::createSimple(
        {{QStringLiteral("color"), QStringLiteral("255,0,0,255")},
         {QStringLiteral("width"), QStringLiteral("0.6")}}).release()));
    auto* heritage = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                        QStringLiteral("주변유적"), QStringLiteral("memory"));
    heritage->setRenderer(new QgsSingleSymbolRenderer(QgsFillSymbol::createSimple(
        {{QStringLiteral("color"), QStringLiteral("255,0,0,255")},
         {QStringLiteral("outline_color"), QStringLiteral("0,0,0,255")}}).release()));
    LayerOps::markReferenceLayer(heritage);
    project.addMapLayer(labels);
    project.addMapLayer(survey);
    project.addMapLayer(heritage);
    auto* root = project.layerTreeRoot();
    root->removeAllChildren();
    root->addLayer(survey);
    root->addLayer(heritage);
    root->addLayer(labels);
    const QList<QgsMapLayer*> above = LayerOps::layersDrawnAboveLabels(&project);
    QVERIFY(above.contains(survey));
    QVERIFY(!above.contains(heritage));
  }

  void heritageDatasetStaysInAboveLabelsPass() {
    QgsProject project;
    auto* labels = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5186"),
                                      QStringLiteral("지번"), QStringLiteral("memory"));
    QgsPalLayerSettings settings;
    settings.fieldName = QStringLiteral("'n'");
    settings.isExpression = true;
    labels->setLabeling(new QgsVectorLayerSimpleLabeling(settings));
    labels->setLabelsEnabled(true);
    auto* survey = new QgsVectorLayer(QStringLiteral("LineString?crs=EPSG:5186"),
                                      QStringLiteral("조사선"), QStringLiteral("memory"));
    survey->setRenderer(new QgsSingleSymbolRenderer(QgsLineSymbol::createSimple(
        {{QStringLiteral("color"), QStringLiteral("255,0,0,255")},
         {QStringLiteral("width"), QStringLiteral("0.6")}}).release()));
    auto* heritage = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                        QStringLiteral("지정유산"), QStringLiteral("memory"));
    heritage->setRenderer(new QgsSingleSymbolRenderer(QgsFillSymbol::createSimple(
        {{QStringLiteral("style"), QStringLiteral("no")},
         {QStringLiteral("outline_color"), QStringLiteral("142,68,173,255")},
         {QStringLiteral("outline_width"), QStringLiteral("0.35")}}).release()));
    LayerOps::markReferenceLayer(heritage);
    project.addMapLayer(labels);
    project.addMapLayer(survey);
    project.addMapLayer(heritage);
    auto* root = project.layerTreeRoot();
    root->removeAllChildren();
    root->addLayer(survey);
    root->addLayer(heritage);
    root->addLayer(labels);
    const QList<QgsMapLayer*> above = LayerOps::layersDrawnAboveLabels(&project);
    QVERIFY(above.contains(survey));
    QVERIFY2(above.contains(heritage), "지정유산은 지적 덧그림 위에 다시 그려야 한다");
  }

  void heritageAloneStaysInAboveLabelsPass() {
    QgsProject project;
    auto* heritage = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                        QStringLiteral("지정유산"), QStringLiteral("memory"));
    heritage->setRenderer(new QgsSingleSymbolRenderer(QgsFillSymbol::createSimple(
        {{QStringLiteral("style"), QStringLiteral("no")},
         {QStringLiteral("outline_color"), QStringLiteral("142,68,173,255")},
         {QStringLiteral("outline_width"), QStringLiteral("0.35")}}).release()));
    LayerOps::markReferenceLayer(heritage);
    project.addMapLayer(heritage);
    auto* root = project.layerTreeRoot();
    root->removeAllChildren();
    root->addLayer(heritage);
    const QList<QgsMapLayer*> above = LayerOps::layersDrawnAboveLabels(&project);
    QVERIFY2(above.contains(heritage), "아래 글자가 없어도 지정유산은 덧그림에 모은다");
    QVERIFY(!LayerOps::sheetBasePaintLayers(&project).contains(heritage));
  }

  void newSurveyPolygonShowsAreaByDefault() {
    QgsVectorLayer layer(QStringLiteral("Polygon?crs=EPSG:5186&field=survey_name:string"),
                         QStringLiteral("조사구역"), QStringLiteral("memory"));
    LayerOps::markSurveyLayer(&layer, QStringLiteral("survey_area"));
    QVERIFY(LayerOps::applyDomainDrawStyle(&layer));
    QVERIFY(layer.labelsEnabled());
    QVERIFY(LayerOps::labelShowArea(&layer));
    QgsFeature feature(layer.fields());
    feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(190000, 450000, 190020, 450010)));
    QgsExpressionContext context;
    context.setFeature(feature);
    QgsExpression expression(layer.labeling()->settings().fieldName);
    const QString text = expression.evaluate(&context).toString();
    QVERIFY2(text.contains(QStringLiteral("200")) && text.contains(QStringLiteral("㎡")), qPrintable(text));
  }

  void drawingUpdatesPreserveLabelPreferences_data() {
    QTest::addColumn<bool>("areaOn");
    QTest::addColumn<bool>("visible");
    QTest::newRow("area-visible") << true << true;
    QTest::newRow("name-visible") << false << true;
    QTest::newRow("area-hidden") << true << false;
    QTest::newRow("name-hidden") << false << false;
  }
  void drawingUpdatesPreserveLabelPreferences() {
    QFETCH(bool, areaOn); QFETCH(bool, visible);
    QgsVectorLayer layer(QStringLiteral("Polygon?crs=EPSG:5187&field=survey_name:string"),
                         QStringLiteral("조사구역"), QStringLiteral("memory"));
    LayerOps::markSurveyLayer(&layer, QStringLiteral("survey_area"));
    QVERIFY(LayerOps::applyNameAttributeLabels(&layer, QStringLiteral("survey_name"), 12., areaOn));
    layer.setLabelsEnabled(visible);
    const auto before = layer.labeling()->settings();
    QVERIFY(LayerOps::setLabelFontSize(&layer, 14.));
    QCOMPARE(layer.labeling()->settings().fieldName, before.fieldName);
    QCOMPARE(layer.labeling()->settings().format().color(), before.format().color());
    QCOMPARE(layer.labelsEnabled(), visible);
    // These are the existing new-feature/attribute-save/vertex-move paths.
    QVERIFY(LayerOps::applyDomainDrawStyle(&layer));
    QVERIFY(LayerOps::applyAreaM2Labels(&layer));
    QCOMPARE(layer.labeling()->settings().fieldName, before.fieldName);
    QCOMPARE(LayerOps::labelFontSize(&layer), 14.);
    QCOMPARE(LayerOps::labelShowArea(&layer), areaOn);
    QCOMPARE(layer.labelsEnabled(), visible);
  }

  void legacyAreaExpressionMatchesItsCheckbox() {
    QgsVectorLayer layer(QStringLiteral("Polygon?crs=EPSG:5186"), QStringLiteral("구역"), QStringLiteral("memory"));
    QVERIFY(LayerOps::applyAreaM2Labels(&layer));
    // Older drawing code left this flag behind after enabling the area expression.
    layer.setCustomProperty(QStringLiteral("ka_hgis/label_show_area"), false);
    QVERIFY(LayerOps::labelShowArea(&layer));
  }

  void explicitFontSizeOverridesOnlySizeExpressions() {
    QgsVectorLayer layer(QStringLiteral("Polygon?crs=EPSG:5186"), QStringLiteral("구역"), QStringLiteral("memory"));
    QVERIFY(LayerOps::applyAreaM2Labels(&layer));
    auto settings = layer.labeling()->settings();
    auto& properties = settings.dataDefinedProperties();
    properties.setProperty(QgsPalLayerSettings::Property::Size, QgsProperty::fromExpression(QStringLiteral("5")));
    properties.setProperty(QgsPalLayerSettings::Property::FontSizeUnit, QgsProperty::fromValue(QStringLiteral("MapUnit")));
    properties.setProperty(QgsPalLayerSettings::Property::Color, QgsProperty::fromValue(QStringLiteral("red")));
    layer.setLabeling(new QgsVectorLayerSimpleLabeling(settings));
    QVERIFY(LayerOps::setLabelFontSize(&layer, 12.));
    const auto after = layer.labeling()->settings();
    QVERIFY(!after.dataDefinedProperties().isActive(QgsPalLayerSettings::Property::Size));
    QVERIFY(!after.dataDefinedProperties().isActive(QgsPalLayerSettings::Property::FontSizeUnit));
    QVERIFY(after.dataDefinedProperties().isActive(QgsPalLayerSettings::Property::Color));
    QCOMPARE(after.fieldName, settings.fieldName);
    QCOMPARE(after.format().size(), 12.);
    QCOMPARE(after.format().sizeUnit(), Qgis::RenderUnit::Points);
  }

  void polygonWithoutNameCanTurnAreaOff() {
    QgsVectorLayer layer(QStringLiteral("Polygon?crs=EPSG:5187"), QStringLiteral("구역"), QStringLiteral("memory"));
    QVERIFY(LayerOps::applyAreaM2Labels(&layer));
    QVERIFY(LayerOps::applyNameAttributeLabels(&layer, {}, 12., false));
    QgsFeature feature(layer.fields());
    feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(190000, 450000, 190020, 450010)));
    QgsExpressionContext context;
    context.setFeature(feature);
    QgsExpression expression(layer.labeling()->settings().fieldName);
    QCOMPARE(expression.evaluate(&context).toString(), QString());
    QVERIFY(!LayerOps::labelShowArea(&layer));
  }

  void zoomKeepsTheWholeLayer_data() {
    QTest::addColumn<QString>("crs");
    QTest::addColumn<QgsRectangle>("bounds");
    for (const QString& crs : {QStringLiteral("EPSG:5186"), QStringLiteral("EPSG:5187")}) {
      QTest::newRow(qPrintable(crs + "-wide")) << crs << QgsRectangle(190000, 450000, 194000, 450020);
      QTest::newRow(qPrintable(crs + "-tall")) << crs << QgsRectangle(190000, 450000, 190020, 454000);
      QTest::newRow(qPrintable(crs + "-point")) << crs << QgsRectangle(190000, 450000, 190000, 450000);
    }
  }
  void zoomKeepsTheWholeLayer() {
    QFETCH(QString, crs);
    QFETCH(QgsRectangle, bounds);
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(crs));
    const bool point = bounds.width() == 0;
    auto* layer = new QgsVectorLayer((point ? QStringLiteral("Point?crs=") : QStringLiteral("Polygon?crs=")) + crs,
                                     QStringLiteral("target"), QStringLiteral("memory"));
    QgsFeature feature(layer->fields());
    feature.setGeometry(point ? QgsGeometry::fromPointXY(bounds.center()) : QgsGeometry::fromRect(bounds));
    QgsFeatureList features{feature};
    QVERIFY(layer->dataProvider()->addFeatures(features));
    project.addMapLayer(layer);
    QgsMapCanvas canvas;
    canvas.resize(800, 600);
    canvas.setDestinationCrs(project.crs());
    canvas.setLayers({layer});
    canvas.setRenderFlag(false);
    QVERIFY(LayerOps::zoomToLayerMax(&canvas, layer));
    const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    if (!output.isEmpty()) {
      QgsMapRendererSequentialJob render(canvas.mapSettings());
      render.start(); render.waitForFinished();
      QVERIFY(render.renderedImage().save(QDir(output).filePath(QString::fromLatin1(QTest::currentDataTag()).replace(':', '_') + "-zoom.png")));
    }
    const QgsRectangle visible = canvas.extent();
    QVERIFY2(visible.contains(bounds), qPrintable(QStringLiteral("visible %1; layer %2").arg(visible.toString(), bounds.toString())));
    QVERIFY(visible.width() >= 80.);
    QVERIFY(visible.height() >= 80.);
    QVERIFY(visible.width() < 15000.);
    QVERIFY(visible.height() < 15000.);
  }

  void reorderPreservesLayers_data() {
    QTest::addColumn<bool>("inGroup");
    QTest::newRow("root") << false;
    QTest::newRow("reference-group") << true;
  }
  void reorderPreservesLayers() {
    QFETCH(bool, inGroup);
    QgsProject project;
    auto* root = project.layerTreeRoot();
    auto* surveyGroup = root->addGroup(QStringLiteral("조사 데이터"));
    auto* group = inGroup ? root->addGroup(QStringLiteral("참조 지도")) : root;
    QList<QPointer<QgsVectorLayer>> layers;
    for (const QString& title : {QStringLiteral("위성"), QStringLiteral("지적"), QStringLiteral("지형맵")}) {
      auto* layer = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187"), title, QStringLiteral("memory"));
      QVERIFY(layer->isValid());
      LayerOps::markReferenceLayer(layer);
      project.addMapLayer(layer, false);
      auto* node = group->addLayer(layer); // App XYZ path appends after the satellite.
      node->setExpanded(false);
      node->setCustomProperty(QStringLiteral("qa-row"), title);
      if (title == QStringLiteral("지적")) node->setItemVisibilityChecked(false);
      layers.append(layer);
    }
    const auto ids = project.mapLayers().keys();
    QSignalSpy removed(&project, &QgsProject::layersRemoved);
    // Exercise synchronous callbacks during clone/insert/remove, not just an
    // already-sorted tree. The registry bridge must never lose its layer.
    connect(root, &QgsLayerTreeNode::addedChildren, &project,
            [&project]() { LayerOps::ensureSatelliteAtBottom(&project); });
    connect(root, &QgsLayerTreeNode::removedChildren, &project,
            [&project]() { LayerOps::ensureSatelliteAtBottom(&project); });
    LayerOps::ensureSatelliteAtBottom(&project);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(project.mapLayers().keys(), ids);
    QCOMPARE(removed.count(), 0);
    QCOMPARE(root->children().first(), surveyGroup);
    QCOMPARE(group->children().last(), group->findLayer(layers.first()->id()));
    QCOMPARE(root->findLayers().size(), 3);
    for (const auto& layer : layers) {
      QVERIFY(layer);
      auto* node = root->findLayer(layer->id());
      QVERIFY(node);
      QCOMPARE(node->customProperty(QStringLiteral("qa-row")).toString(), layer->name());
      QVERIFY(!node->isExpanded());
      QCOMPARE(node->itemVisibilityChecked(), layer->name() != QStringLiteral("지적"));
    }
    QSignalSpy inserted(root, &QgsLayerTreeNode::addedChildren);
    QSignalSpy erased(root, &QgsLayerTreeNode::removedChildren);
    for (int i = 0; i < 20; ++i) LayerOps::ensureSatelliteAtBottom(&project);
    QCOMPARE(inserted.count(), 0);
    QCOMPARE(erased.count(), 0);
  }

  void moveAcrossGroupPreservesSelectedLayer() {
    QgsProject project;
    auto* root = project.layerTreeRoot();
    auto* a = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5186"), QStringLiteral("A"), QStringLiteral("memory"));
    auto* b = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5186"), QStringLiteral("B"), QStringLiteral("memory"));
    project.addMapLayer(a, false); project.addMapLayer(b, false);
    root->addLayer(a);
    auto* group = root->addGroup(QStringLiteral("참조 지도"));
    root->addLayer(b);
    for (int i = 0; i < 20; ++i) {
      QVERIFY(LayerOps::moveLegendLayer(root->findLayer(b), 1));
      QCOMPARE(root->children().at(1), root->findLayer(b));
      QCOMPARE(root->children().last(), group);
      QVERIFY(LayerOps::moveLegendLayer(root->findLayer(b), 2));
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(project.mapLayers().size(), 2);
    QCOMPARE(root->findLayers().size(), 2);
  }

  void thematicRemainsPaintedWhenZoomedOut_data() {
    QTest::addColumn<QString>("crs");
    QTest::addColumn<double>("scale");
    for (const auto& crs : {QStringLiteral("EPSG:5186"), QStringLiteral("EPSG:5187")})
      for (const double scale : {100000., 127158., 250000., 1000000.})
        QTest::newRow(qPrintable(crs + QString::number(scale))) << crs << scale;
  }
  void thematicRemainsPaintedWhenZoomedOut() {
    QFETCH(QString, crs); QFETCH(double, scale);
    QgsVectorLayer layer(QStringLiteral("Polygon?crs=") + crs, QStringLiteral("지질도"), QStringLiteral("memory"));
    QVERIFY(layer.isValid());
    QgsFeature feature(layer.fields());
    feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(170000, 420000, 210000, 460000)));
    QgsFeatureList features{feature};
    QVERIFY(layer.dataProvider()->addFeatures(features));
    layer.updateExtents();
    layer.setRenderer(new QgsSingleSymbolRenderer(QgsFillSymbol::createSimple(
        {{QStringLiteral("color"), QStringLiteral("20,90,160,255")},
         {QStringLiteral("outline_style"), QStringLiteral("no")}}).release()));
    LayerOps::applyThematicOverlayScaleRange(&layer);
    QgsMapSettings settings;
    settings.setDestinationCrs(layer.crs());
    settings.setOutputSize(QSize(320, 240));
    settings.setOutputDpi(96);
    const double halfWidth = scale * 320. * 0.0254 / 96. / 2.;
    settings.setExtent(QgsRectangle(190000 - halfWidth, 440000 - halfWidth * .75,
                                    190000 + halfWidth, 440000 + halfWidth * .75));
    settings.setLayers({&layer});
    settings.setBackgroundColor(Qt::white);
    QgsMapRendererSequentialJob job(settings);
    job.start(); job.waitForFinished();
    QVERIFY(job.errors().isEmpty());
    const QImage image = job.renderedImage();
    QVERIFY(!image.isNull());
    const QColor center = image.pixelColor(image.width() / 2, image.height() / 2);
    QVERIFY2(center.blue() > center.red() + 80, "The actual map must remain painted beyond 1:100000");
    const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    if (!output.isEmpty()) QVERIFY(image.save(QDir(output).filePath(crs.mid(5) + QLatin1Char('-') + QString::number(scale) + QStringLiteral("-map.png"))));
  }

  void savedThematicLimitIsUpgradedWithoutChangingImportedScale() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QgsProject project;
    QgsVectorLayer memory(QStringLiteral("Polygon?crs=EPSG:5187"), QStringLiteral("source"), QStringLiteral("memory"));
    QgsVectorFileWriter::SaveVectorOptions options;
    options.driverName = QStringLiteral("GPKG"); options.layerName = QStringLiteral("geology_map");
    const QString path = dir.filePath(QStringLiteral("reference.gpkg"));
    QCOMPARE(QgsVectorFileWriter::writeAsVectorFormatV3(&memory, path, project.transformContext(), options), QgsVectorFileWriter::NoError);
    auto* layer = new QgsVectorLayer(path + QStringLiteral("|layername=geology_map"), QStringLiteral("사용자가 바꾼 제목"), QStringLiteral("ogr"));
    QVERIFY(layer->isValid());
    LayerOps::markReferenceLayer(layer);
    layer->setScaleBasedVisibility(true); layer->setMinimumScale(100001.);
    project.addMapLayer(layer);
    auto* external = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187"), QStringLiteral("지질도"), QStringLiteral("memory"));
    external->setScaleBasedVisibility(true); external->setMinimumScale(100001.);
    project.addMapLayer(external);
    const QString qgz = dir.filePath(QStringLiteral("survey.qgz"));
    QVERIFY(project.write(qgz));
    project.clear();
    QVERIFY(project.read(qgz));
    LayerOps::restoreThematicOverlayVisibility(&project);
    auto* restored = project.mapLayersByName(QStringLiteral("사용자가 바꾼 제목")).first();
    QVERIFY(restored->isInScaleRange(250000.));
    QVERIFY(!project.mapLayersByName(QStringLiteral("지질도")).first()->isInScaleRange(250000.));
  }

  void checkingLayerAfterAllOffShowsOnMap() {
    QgsProject project;
    auto* root = project.layerTreeRoot();
    auto* group = root->addGroup(QStringLiteral("참조 지도"));
    auto* layer = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"), QStringLiteral("토양도"), QStringLiteral("memory"));
    auto* other = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"), QStringLiteral("지질도"), QStringLiteral("memory"));
    auto* sub = group->addGroup(QStringLiteral("지정유산"));
    auto* subLayer = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"), QStringLiteral("국가지정유산"), QStringLiteral("memory"));
    QVERIFY(layer->isValid());
    QVERIFY(other->isValid());
    QVERIFY(subLayer->isValid());
    project.addMapLayer(layer, false);
    project.addMapLayer(other, false);
    project.addMapLayer(subLayer, false);
    auto* node = group->addLayer(layer);
    auto* otherNode = group->addLayer(other);
    auto* subNode = sub->addLayer(subLayer);
    QVERIFY(node->isVisible());
    root->children().first()->setItemVisibilityCheckedRecursive(false);
    QVERIFY(!node->isVisible());
    node->setItemVisibilityChecked(true);
    QVERIFY(node->itemVisibilityChecked());
    QVERIFY2(!node->isVisible(), "부모 그룹이 꺼져 있으면 체크만 되고 지도에는 안 나온다");
    QVERIFY(!LayerOps::visibleLayersPaintOrder(&project).contains(layer));
    LayerOps::revealCheckedLegendNode(node);
    QVERIFY2(node->isVisible(), "개별 체크 후 부모를 열면 지도에 켜져야 한다");
    QVERIFY(group->itemVisibilityChecked());
    QVERIFY(LayerOps::visibleLayersPaintOrder(&project).contains(layer));
    QVERIFY(!otherNode->itemVisibilityChecked());
    QVERIFY(!otherNode->isVisible());
    QVERIFY(!sub->itemVisibilityChecked());
    QVERIFY(!subNode->isVisible());
    sub->setItemVisibilityChecked(true);
    LayerOps::revealCheckedLegendNode(sub);
    QVERIFY2(subNode->isVisible(), "전체 끄기 뒤 접힌 하위 묶음만 체크해도 자식 레이어가 켜져야 한다");
    QVERIFY(LayerOps::visibleLayersPaintOrder(&project).contains(subLayer));
  }

  void sheetBasePaintLayersOmitAboveGeometries() {
    QgsProject project;
    auto* labels = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5186"), QStringLiteral("지번"), QStringLiteral("memory"));
    QgsPalLayerSettings settings;
    settings.fieldName = QStringLiteral("'n'");
    settings.isExpression = true;
    labels->setLabeling(new QgsVectorLayerSimpleLabeling(settings));
    labels->setLabelsEnabled(true);
    auto* survey = new QgsVectorLayer(QStringLiteral("LineString?crs=EPSG:5186"), QStringLiteral("조사선"), QStringLiteral("memory"));
    survey->setRenderer(new QgsSingleSymbolRenderer(QgsLineSymbol::createSimple(
        {{QStringLiteral("color"), QStringLiteral("255,0,0,255")}, {QStringLiteral("width"), QStringLiteral("0.6")}}).release()));
    auto* heritage = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"), QStringLiteral("지정유산"), QStringLiteral("memory"));
    heritage->setRenderer(new QgsSingleSymbolRenderer(QgsFillSymbol::createSimple(
        {{QStringLiteral("style"), QStringLiteral("no")},
         {QStringLiteral("outline_color"), QStringLiteral("142,68,173,255")},
         {QStringLiteral("outline_width"), QStringLiteral("0.35")}}).release()));
    LayerOps::markReferenceLayer(heritage);
    auto* topo = new QgsVectorLayer(QStringLiteral("LineString?crs=EPSG:5186"), QStringLiteral("수치지형도"), QStringLiteral("memory"));
    topo->setRenderer(new QgsSingleSymbolRenderer(QgsLineSymbol::createSimple(
        {{QStringLiteral("color"), QStringLiteral("128,128,128,255")}, {QStringLiteral("width"), QStringLiteral("0.2")}}).release()));
    LayerOps::markReferenceLayer(topo);
    topo->setCustomProperty(QStringLiteral("ka_hgis/topographic_group"), QStringLiteral("topo"));
    project.addMapLayer(labels);
    project.addMapLayer(survey);
    project.addMapLayer(heritage);
    project.addMapLayer(topo);
    auto* root = project.layerTreeRoot();
    root->removeAllChildren();
    root->addLayer(topo);
    root->addLayer(survey);
    root->addLayer(heritage);
    root->addLayer(labels);
    const QList<QgsMapLayer*> above = LayerOps::layersDrawnAboveLabels(&project);
    QVERIFY(above.contains(survey));
    QVERIFY(above.contains(heritage));
    QVERIFY(!above.contains(topo));
    const QList<QgsMapLayer*> base = LayerOps::sheetBasePaintLayers(&project);
    QVERIFY(base.contains(topo));
    QVERIFY(base.contains(labels));
    QVERIFY(!base.contains(survey));
    QVERIFY2(!base.contains(heritage), "주변유적은 덧그림에서만 그린다");
    auto* cadastral = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"), QStringLiteral("지적도"), QStringLiteral("memory"));
    cadastral->setCustomProperty(QStringLiteral("ka_hgis/cadastral"), true);
    cadastral->setRenderer(new QgsSingleSymbolRenderer(QgsFillSymbol::createSimple(
        {{QStringLiteral("style"), QStringLiteral("no")},
         {QStringLiteral("outline_color"), QStringLiteral("0,0,0,255")},
         {QStringLiteral("outline_width"), QStringLiteral("0.2")}}).release()));
    QgsPalLayerSettings lotSettings;
    lotSettings.fieldName = QStringLiteral("'12'");
    lotSettings.isExpression = true;
    cadastral->setLabeling(new QgsVectorLayerSimpleLabeling(lotSettings));
    cadastral->setLabelsEnabled(true);
    project.addMapLayer(cadastral);
    QgsLayerTreeLayer* labelNode = root->findLayer(labels->id());
    const int labelIndex = root->children().indexOf(labelNode);
    QVERIFY(labelIndex >= 0);
    root->insertLayer(labelIndex, cadastral);
    const QList<QgsMapLayer*> withLots = LayerOps::layersDrawnAboveLabels(&project);
    QVERIFY2(!withLots.contains(cadastral), "지적 지번은 본 화면에 남긴다");
    QVERIFY(withLots.contains(survey));
    QVERIFY(LayerOps::sheetBasePaintLayers(&project).contains(cadastral));
    QgsMapCanvas canvas;
    canvas.resize(400, 300);
    LayerOps::syncMapCanvas(&project, &canvas, false);
    const QList<QgsMapLayer*> painted = canvas.layers();
    QVERIFY(painted.contains(labels));
    QVERIFY(painted.contains(cadastral));
    QVERIFY(painted.contains(topo));
    QVERIFY2(!painted.contains(survey), "조사선은 덧그림에서 한 번만 그린다");
    QVERIFY2(!painted.contains(heritage), "유적 도형은 덧그림에서 한 번만 그린다");
  }

  void officialCadastralCountsSoAutoVworldIsNotNeeded() {
    QgsProject project;
    auto* cad = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                   QStringLiteral("지적도 · 조사 주변 5km"), QStringLiteral("memory"));
    QVERIFY(cad->isValid());
    LayerOps::markCadastralLayer(cad);
    project.addMapLayer(cad, false);
    LayerOps::placeCadastralLayer(&project, cad);
    QVERIFY(LayerOps::isCadastralLayer(cad));
    QVERIFY(LayerOps::projectHasCadastralLayer(&project));
    QVERIFY(!LayerOps::userRemovedCadastral(&project));
    QVERIFY(!project.layerTreeRoot()->findGroup(QString::fromUtf8(LayerOps::kGroupCadastral)));
    auto* node = project.layerTreeRoot()->findLayer(cad->id());
    QVERIFY(node);
    QVERIFY(node->parent() == project.layerTreeRoot());
    QCOMPARE(LayerOps::removableCadastralLayersFromNode(node), QList<QgsMapLayer*>{cad});
  }

  void userRemovedCadastralStopsAutoAdd() {
    QgsProject project;
    QVERIFY(!LayerOps::userRemovedCadastral(&project));
    LayerOps::rememberUserRemovedCadastral(&project);
    QVERIFY(LayerOps::userRemovedCadastral(&project));
    LayerOps::clearUserRemovedCadastral(&project);
    QVERIFY(!LayerOps::userRemovedCadastral(&project));
  }

  void referenceGroupListsOnlyCadastralPictures() {
    QgsProject project;
    auto* refs = project.layerTreeRoot()->addGroup(QString::fromUtf8(LayerOps::kGroupReference));
    auto* picture = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                       QStringLiteral("VWorld 지적(본번·부번)"), QStringLiteral("memory"));
    auto* heritage = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5186"),
                                        QStringLiteral("지정유산"), QStringLiteral("memory"));
    QVERIFY(picture->isValid() && heritage->isValid());
    LayerOps::markReferenceLayer(heritage);
    project.addMapLayer(picture, false);
    project.addMapLayer(heritage, false);
    refs->addLayer(picture);
    refs->addLayer(heritage);
    QVERIFY(LayerOps::isVworldCadastralPicture(picture));
    QVERIFY(!LayerOps::isVworldCadastralPicture(heritage));
    QVERIFY(LayerOps::removableCadastralLayersFromNode(refs).isEmpty());
    QCOMPARE(LayerOps::removableCadastralLayersFromNode(refs->findLayer(picture->id())),
             QList<QgsMapLayer*>{picture});
  }

  void heritageKindGroupListsChildrenForDelete() {
    QgsProject project;
    auto* refs = project.layerTreeRoot()->addGroup(QString::fromUtf8(LayerOps::kGroupReference));
    auto* kind = refs->addGroup(QStringLiteral("지표조사구역"));
    auto* permit = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                      QStringLiteral("지표사업허가요역"), QStringLiteral("memory"));
    auto* points = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5186"),
                                      QStringLiteral("지표조사위치도"), QStringLiteral("memory"));
    auto* survey = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                      QStringLiteral("조사구역"), QStringLiteral("memory"));
    QVERIFY(permit->isValid() && points->isValid() && survey->isValid());
    LayerOps::markReferenceLayer(permit);
    LayerOps::markReferenceLayer(points);
    survey->setCustomProperty(QStringLiteral("ka_hgis/layer_key"), QStringLiteral("survey_area"));
    project.addMapLayer(permit, false);
    project.addMapLayer(points, false);
    project.addMapLayer(survey, false);
    kind->addLayer(permit);
    kind->addLayer(points);
    QVERIFY(LayerOps::removableReferenceLayersFromNode(kind).isEmpty());
    QCOMPARE(LayerOps::removableReferenceLayersFromNode(kind->findLayer(permit->id())),
             QList<QgsMapLayer*>{permit});
    QCOMPARE(LayerOps::removableReferenceLayersFromNode(kind->findLayer(points->id())),
             QList<QgsMapLayer*>{points});
    const QList<QgsMapLayer*> fromKind = LayerOps::removableLegendLayersFromNode(kind);
    QCOMPARE(fromKind.size(), 2);
    QVERIFY(fromKind.contains(permit) && fromKind.contains(points));
    const QList<QgsMapLayer*> fromRoot = LayerOps::removableLegendLayersFromNode(refs);
    QCOMPARE(fromRoot.size(), 2);
    QVERIFY(fromRoot.contains(permit) && fromRoot.contains(points));
    QVERIFY(!fromRoot.contains(survey));
    auto* surveyGroup = project.layerTreeRoot()->addGroup(QString::fromUtf8(LayerOps::kGroupSurveyData));
    surveyGroup->addLayer(survey);
    // 묶음 줄은 무엇이 들었든 통째로 지운다. 조사 레이어 한 줄은 레이어 창 선택으로 지운다.
    QCOMPARE(LayerOps::removableLegendLayersFromNode(surveyGroup), QList<QgsMapLayer*>{survey});
    QVERIFY(LayerOps::removableLegendLayersFromNode(surveyGroup->findLayer(survey->id())).isEmpty());
  }

  void referenceRootDeleteKeepsCadastralOut() {
    QgsProject project;
    auto* refs = project.layerTreeRoot()->addGroup(QString::fromUtf8(LayerOps::kGroupReference));
    auto* sat = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                  QStringLiteral("위성"), QStringLiteral("memory"));
    auto* heritage = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5186"),
                                        QStringLiteral("문화유적분포지도"), QStringLiteral("memory"));
    QVERIFY(sat->isValid() && heritage->isValid());
    LayerOps::markReferenceLayer(sat);
    LayerOps::markReferenceLayer(heritage);
    project.addMapLayer(sat, false);
    project.addMapLayer(heritage, false);
    refs->addLayer(sat);
    auto* kind = refs->addGroup(QStringLiteral("문화유적분포지도"));
    kind->addLayer(heritage);
    QVERIFY(LayerOps::removableReferenceLayersFromNode(refs).isEmpty());
    QCOMPARE(LayerOps::removableReferenceLayersFromNode(refs->findLayer(sat->id())),
             QList<QgsMapLayer*>{sat});
    QCOMPARE(LayerOps::removableReferenceLayersFromNode(kind->findLayer(heritage->id())),
             QList<QgsMapLayer*>{heritage});
  }

  void anyReferenceBundleListsUnmarkedChildrenForDelete() {
    QgsProject project;
    auto* refs = project.layerTreeRoot()->addGroup(QString::fromUtf8(LayerOps::kGroupReference));
    auto* topo = refs->addGroup(QStringLiteral("수치지형도"));
    auto* lines = new QgsVectorLayer(QStringLiteral("LineString?crs=EPSG:5186"),
                                     QStringLiteral("가져온선"), QStringLiteral("memory"));
    QVERIFY(lines->isValid());
    project.addMapLayer(lines, false);
    topo->addLayer(lines);
    QVERIFY(LayerOps::removableReferenceLayersFromNode(topo).isEmpty());
    QCOMPARE(LayerOps::removableReferenceLayersFromNode(topo->findLayer(lines->id())),
             QList<QgsMapLayer*>{lines});
    QCOMPARE(LayerOps::removableLegendLayersFromNode(topo), QList<QgsMapLayer*>{lines});
    QCOMPARE(LayerOps::removableLegendLayersFromNode(refs), QList<QgsMapLayer*>{lines});
    QVERIFY2(LayerOps::removableLegendLayersFromNode(project.layerTreeRoot()).isEmpty(),
             "보이지 않는 맨 위 뿌리를 지우면 모든 레이어가 사라진다");
  }

  void wheelZoomFactorIsFinerThanQgisDefault() {
    QCOMPARE(LayerOps::kWheelZoomFactor, 1.2);
    QVERIFY(LayerOps::kWheelZoomFactor > 1.0);
    QVERIFY(LayerOps::kWheelZoomFactor < 2.0);
    QgsMapCanvas canvas;
    LayerOps::applyWheelZoomFactor(&canvas);
    const double out = canvas.zoomOutFactor();
    const double in = canvas.zoomInFactor();
    QVERIFY2(qFuzzyCompare(out, LayerOps::kWheelZoomFactor) ||
                 qFuzzyCompare(in, LayerOps::kWheelZoomFactor),
             qPrintable(QStringLiteral("out=%1 in=%2").arg(out).arg(in)));
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  LayerStateTest test;
  const int rc = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
#include "test_layer_state.moc"
