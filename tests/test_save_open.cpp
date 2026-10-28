#include <QtTest>
#include <algorithm>
#include <cmath>
#include <QComboBox>
#include <QCheckBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QImage>
#include <QPainter>
#include <QFileDialog>
#include <QFileInfo>
#include <QDirIterator>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QSettings>
#include <QSpinBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QScopeGuard>
#include <QSplitter>
#include <QTemporaryDir>
#include <QTimer>
#include <QTabWidget>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QTableWidget>
#include <QMenu>
#include <QPushButton>
#include <QToolBar>
#include <QToolButton>
#include <QUrlQuery>
#include <QCryptographicHash>
#include <QSet>
#include <gdal.h>
#include <cpl_error.h>
#include <cpl_conv.h>
#include <memory>

#include "app/MainWindow.h"
#include "app/KaWindowGeometry.h"
#include "app/KaCaptureMapTool.h"
#include "app/KaFeatureCard.h"
#include "app/KaAttributeMapTool.h"
#include "app/KaFeatureSelectTool.h"
#include "app/KaVertexEditTool.h"
#include "app/KaDrawingStudio.h"
#include "app/KaNewSurveyDialog.h"
#include "app/KaSubmitDialog.h"
#include "app/KaTopographicAccountDialog.h"
#include "app/KaLayerInformation.h"
#include "app/KaPrintDialog.h"
#include "app/KaTheme.h"
#include "app/KaBeginnerRibbon.h"
#include "ribbon_label_ink.h"
#include "folder_dialog_fixture.h"
#include <QPdfDocument>
#include "app/KaRegionLocator.h"
#include "app/KaTopographicBrowser.h"
#include "app/KaTopographicImportDialog.h"
#include "app/KaTopographicScopePanel.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QElapsedTimer>
#include "core/LayerOps.h"
#include "core/RecentSurveys.h"
#include "core/KaPortableRuntime.h"
#include "core/SurveyBundle.h"
#include "core/SurveyProjectFactory.h"
#include "core/SurveyStorage.h"
#include "core/DemPresentation.h"
#include "core/DemColorRampLegend.h"
#include "core/HeritageStyle.h"
#include "core/HeritageLayoutNumbers.h"
#include "core/ExportService.h"
#include "core/ChecklistEngine.h"
#include "core/ProjectStateBuilder.h"
#include "core/LayoutService.h"
#include <qgsapplication.h>
#include <qgscategorizedsymbolrenderer.h>
#include <qgsfeature.h>
#include <qgsexpression.h>
#include <qgsexpressioncontext.h>
#include <qgsexception.h>
#include <qgsgeometry.h>
#include <qgscoordinatetransform.h>
#include <qgscolorramplegendnode.h>
#include <qgslayertree.h>
#include <qgslayertreemodel.h>
#include <qgslayertreemodellegendnode.h>
#include <qgslayertreeregistrybridge.h>
#include <qgslayertreeview.h>
#include <qgslayout.h>
#include <qgslayoutmanager.h>
#include <qgslayoutitemmap.h>
#include <qgsprintlayout.h>
#include <qgslabelingresults.h>
#include <qgslayoutitemlabel.h>
#include <qgslayoutitemlegend.h>
#include <qgslayoutitempicture.h>
#include <qgslayoutitemscalebar.h>
#include <qgslayoutitempage.h>
#include <qgslayoutpagecollection.h>
#include <qgslayoutexporter.h>
#include <QDoubleSpinBox>
#include <qgslayoutview.h>
#include <qgslayoutmousehandles.h>
#include <qgslayoutviewtoolselect.h>
#include <qgsmapcanvas.h>
#include <qgsrectangle.h>
#include <qgsmaplayerstyle.h>
#include <qgsmaptopixel.h>
#include <qgsmessagebar.h>
#include <qgsmessagebaritem.h>
#include <qgsmarkersymbollayer.h>
#include <qgsnetworkaccessmanager.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgsrastershader.h>
#include <qgssinglebandpseudocolorrenderer.h>
#include <qgssinglesymbolrenderer.h>
#include <qgsfillsymbol.h>
#include <qgsvectorlayer.h>
#include <qgsvectordataprovider.h>
#include <qgsogrproviderutils.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayerlabeling.h>
#include <qgspallabeling.h>
#include <qgstextbackgroundsettings.h>

static QString s_testSettingsPath;

class TestSaveOpen : public QObject {
  Q_OBJECT
private:
  QTemporaryDir m_files;
  QString makeSurvey(const QString& name, bool registryOnly = false) {
    QString error;
    const QString path = SurveyProjectFactory::createNewSurvey(
        m_files.path(), name, &error, QStringLiteral("EPSG:5187"));
    if (path.isEmpty()) return {};
    QgsProject project;
    project.setTitle(name);
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* layer = LayerOps::ensureDomainLayer(&project, path, QStringLiteral("survey_area"),
                                             name, &error);
    if (!layer || !layer->startEditing()) return {};
    QgsFeature feature(layer->fields());
    feature.setAttribute(QStringLiteral("survey_name"), name);
    feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(190000, 560000, 190100, 560100)));
    if (!layer->addFeature(feature) || !layer->commitChanges()) return {};
    layer->setRenderer(new QgsSingleSymbolRenderer(QgsFillSymbol::createSimple(
        {{QStringLiteral("color"), QStringLiteral("217,43,43,255")}}).release()));
    if (registryOnly) {
      // Reproduce field files whose data registry survived but whose legend was lost.
      project.layerTreeRegistryBridge()->setEnabled(false);
      project.layerTreeRoot()->removeLayer(layer);
      project.layerTreeRegistryBridge()->setEnabled(true);
    }
    const QString rasterPath = m_files.filePath(QStringLiteral("background.png"));
    QImage pixel(8, 8, QImage::Format_RGB32);
    pixel.fill(Qt::white);
    if (!pixel.save(rasterPath)) return {};
    QFile worldFile(m_files.filePath(QStringLiteral("background.pgw")));
    if (!worldFile.open(QIODevice::WriteOnly)) return {};
    worldFile.write("1\n0\n0\n-1\n190000.5\n560007.5\n");
    worldFile.close();
    for (const QString& title : {QStringLiteral("위성"), QStringLiteral("지적")}) {
      auto* raster = new QgsRasterLayer(rasterPath, title, QStringLiteral("gdal"));
      if (!raster->isValid()) { delete raster; return {}; }
      raster->setCrs(project.crs());
      project.addMapLayer(raster);
    }
    if (!SurveyStorage::writeEmbedded(&project, path, &error)) return {};
    return path;
  }
  // 조사 폴더를 따로 둔 새 조사(조사구역 1개). makeSurvey 는 모든 시험이 같은 폴더를 쓴다.
  static QString makeSurveyIn(const QString& dir, const QString& name) {
    QString error;
    const QString path = SurveyProjectFactory::createNewSurvey(dir, name, &error, QStringLiteral("EPSG:5187"));
    if (path.isEmpty()) return {};
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* layer = LayerOps::ensureDomainLayer(&project, path, QStringLiteral("survey_area"), name, &error);
    if (!layer || !layer->startEditing()) return {};
    QgsFeature feature(layer->fields());
    feature.setAttribute(QStringLiteral("survey_name"), name);
    feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(190000, 560000, 190100, 560100)));
    if (!layer->addFeature(feature) || !layer->commitChanges()) return {};
    if (!SurveyStorage::writeEmbedded(&project, path, &error)) return {};
    return path;
  }
  // count 개 도형이 든 GPKG 벡터 파일을 만든다.
  static bool writeVectorFile(const QString& path, const QString& table, int count) {
    QgsVectorLayer memory(QStringLiteral("Polygon?crs=EPSG:5187&field=nm:string(40)"), table,
                          QStringLiteral("memory"));
    if (!memory.isValid() || !memory.startEditing()) return false;
    for (int i = 0; i < count; ++i) {
      QgsFeature feature(memory.fields());
      feature.setAttribute(0, QStringLiteral("유적 %1").arg(i + 1));
      feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(190010. + i * 20, 560010., 190020. + i * 20, 560020.)));
      if (!memory.addFeature(feature)) return false;
    }
    if (!memory.commitChanges()) return false;
    QgsVectorFileWriter::SaveVectorOptions options;
    options.driverName = QStringLiteral("GPKG");
    options.layerName = table;
    QString error;
    return QgsVectorFileWriter::writeAsVectorFormatV3(&memory, path, QgsCoordinateTransformContext(), options,
                                                      &error) == QgsVectorFileWriter::NoError;
  }
  static bool copyDirectory(const QString& from, const QString& to) {
    if (!QDir().mkpath(to)) return false;
    const QDir source(from);
    for (const QFileInfo& entry : source.entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot)) {
      const QString target = QDir(to).filePath(entry.fileName());
      if (entry.isDir() ? !copyDirectory(entry.absoluteFilePath(), target)
                        : !QFile::copy(entry.absoluteFilePath(), target))
        return false;
    }
    return true;
  }
  static QgsMapLayer* layerNamed(const QString& name) {
    const QList<QgsMapLayer*> found = QgsProject::instance()->mapLayersByName(name);
    return found.isEmpty() ? nullptr : found.first();
  }
  static void disableRendering(MainWindow& window) {
    if (auto* canvas = window.findChild<QgsMapCanvas*>()) canvas->setRenderFlag(false);
  }
  // [v3] F073: satellite and cadastral join the project from the event loop right after an
  // explicit open (scheduleDefaultBasemaps -> loadBootBasemaps). Drain that turn before a test
  // takes a layer or row baseline, otherwise the pair lands inside the first nested loop.
  static bool openSettled(MainWindow& window, const QString& path) {
    if (!window.openSurveyGpkg(path)) return false;
    QElapsedTimer clock;
    clock.start();
    while (window.basemapBootPending() && clock.elapsed() < 5000) QTest::qWait(10);
    return !window.basemapBootPending();
  }
  static QByteArray contents(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
  }
  // 20초 자동 저장은 없앴다. 저장은 「저장」을 누를 때만 일어난다. 예전에는 이 타이머를
  // 찾아 timeout 을 쏘아 자동 저장을 흉내 냈지만, 이제 저장 경로를 직접 부른다.
  static bool saveNow(MainWindow& window) {
    bool ok = false;
    return QMetaObject::invokeMethod(&window, "persistSurveyWork", Qt::DirectConnection,
                                     Q_RETURN_ARG(bool, ok)) && ok;
  }
  static bool verticesWithinMm(const QgsGeometry& a, const QgsGeometry& b, double mm = 0.001) {
    if (a.isNull() || b.isNull() || a.isEmpty() || b.isEmpty()) return false;
    QgsVertexIterator ia = a.vertices();
    QgsVertexIterator ib = b.vertices();
    bool sequential = true;
    while (ia.hasNext() && ib.hasNext()) {
      const QgsPoint pa = ia.next();
      const QgsPoint pb = ib.next();
      if (QgsPointXY(pa).distance(QgsPointXY(pb)) > mm) sequential = false;
    }
    if (sequential && !ia.hasNext() && !ib.hasNext()) return true;
    // SHP 링 시작점이 바뀌어도 모양은 같아야 한다.
    const double hausdorff = a.hausdorffDistance(b);
    return std::isfinite(hausdorff) && hausdorff >= 0.0 && hausdorff <= mm;
  }
  static QgsGeometry to5179(const QgsGeometry& geometry, const QgsCoordinateReferenceSystem& src,
                            QgsProject* project) {
    QgsGeometry copy(geometry);
    if (!src.isValid() || src.authid() == QLatin1String("EPSG:5179")) return copy;
    QgsCoordinateTransform transform(src, QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")),
                                     project->transformContext());
    if (copy.transform(transform) != Qgis::GeometryOperationResult::Success) return QgsGeometry();
    return copy;
  }
  static QString fieldText(const QgsFeature& feature, const QStringList& names) {
    for (const QString& name : names) {
      const int index = feature.fields().indexOf(name);
      if (index >= 0) return feature.attribute(index).toString();
    }
    return {};
  }
  static QgsFeature featureByField(QgsVectorLayer* layer, const QStringList& names, const QString& value) {
    QgsFeature feature;
    if (!layer) return feature;
    QgsFeatureIterator it = layer->getFeatures();
    while (it.nextFeature(feature)) {
      if (fieldText(feature, names) == value) return feature;
    }
    return QgsFeature();
  }
  static bool addComposedUserSheet(QgsProject* project, QgsMapLayer* mapLayer, const QgsRectangle& extent) {
    QString error;
    if (LayoutService::createBlankSheet(project, 297.0, 210.0, QStringLiteral("user_sheet"), &error).isEmpty())
      return false;
    auto* layout = dynamic_cast<QgsPrintLayout*>(
        project->layoutManager()->layoutByName(QStringLiteral("user_sheet")));
    if (!layout || !mapLayer) return false;
    auto* map = new QgsLayoutItemMap(layout);
    map->setId(QStringLiteral("ka_map"));
    map->attemptSetSceneRect(QRectF(20.0, 20.0, 120.0, 80.0));
    map->setCrs(project->crs().isValid() ? project->crs() : mapLayer->crs());
    map->setKeepLayerSet(true);
    map->setLayers(QList<QgsMapLayer*>{mapLayer});
    map->zoomToExtent(extent);
    if (map->scene() != layout) layout->addLayoutItem(map);
    return LayoutService::isComposedStudioSheet(project);
  }
  QString makeRoundTripSurvey(const QString& name, const QString& authId) {
    QString error;
    const QString path = SurveyProjectFactory::createNewSurvey(m_files.path(), name, &error, authId);
    if (path.isEmpty()) return {};
    const bool east = authId.endsWith(QLatin1String("5187"));
    const double ox = east ? 190000.0 : 200000.0;
    const double oy = east ? 560000.0 : 450000.0;
    QgsProject project;
    project.setTitle(name);
    project.setCrs(QgsCoordinateReferenceSystem(authId));

    auto* surveyArea = LayerOps::ensureDomainLayer(&project, path, QStringLiteral("survey_area"),
                                                   name, &error);
    if (!surveyArea || !surveyArea->startEditing()) return {};
    QgsFeature surveyFeature(surveyArea->fields());
    surveyFeature.setAttribute(QStringLiteral("survey_name"), QStringLiteral("왕복조사"));
    surveyFeature.setAttribute(QStringLiteral("site_name"), QStringLiteral("테스트유적"));
    surveyFeature.setGeometry(QgsGeometry::fromRect(QgsRectangle(ox, oy, ox + 100.0, oy + 100.0)));
    if (!surveyArea->addFeature(surveyFeature) || !surveyArea->commitChanges()) return {};

    auto* featurePoly = LayerOps::ensureDomainLayer(&project, path, QStringLiteral("feature_poly"),
                                                    QStringLiteral("유구 면"), &error);
    if (!featurePoly || !featurePoly->startEditing()) return {};
    QgsFeature polyFeature(featurePoly->fields());
    polyFeature.setAttribute(QStringLiteral("kind"), QStringLiteral("수혈"));
    polyFeature.setAttribute(QStringLiteral("period"), QStringLiteral("청동기"));
    polyFeature.setAttribute(QStringLiteral("feature_no"), QStringLiteral("1"));
    polyFeature.setGeometry(QgsGeometry::fromRect(QgsRectangle(ox + 20.0, oy + 20.0, ox + 40.0, oy + 40.0)));
    if (!featurePoly->addFeature(polyFeature) || !featurePoly->commitChanges()) return {};

    auto* featureLine = LayerOps::ensureDomainLayer(&project, path, QStringLiteral("feature_line"),
                                                    QStringLiteral("유구 선"), &error);
    if (!featureLine || !featureLine->startEditing()) return {};
    QgsFeature lineFeature(featureLine->fields());
    lineFeature.setAttribute(QStringLiteral("kind"), QStringLiteral("경계"));
    lineFeature.setGeometry(QgsGeometry::fromPolylineXY(
        {QgsPointXY(ox + 10.0, oy + 50.0), QgsPointXY(ox + 90.0, oy + 50.0)}));
    if (!featureLine->addFeature(lineFeature) || !featureLine->commitChanges()) return {};
    if (auto* lineNode = project.layerTreeRoot()->findLayer(featureLine->id()))
      lineNode->setItemVisibilityChecked(false);

    auto* control = LayerOps::ensureDomainLayer(&project, path, QStringLiteral("control_points"),
                                                QStringLiteral("기준점"), &error);
    if (!control || !control->startEditing()) return {};
    for (int i = 0; i < 2; ++i) {
      QgsFeature point(control->fields());
      point.setAttribute(QStringLiteral("point_id"), QStringLiteral("G%1").arg(i + 1));
      point.setAttribute(QStringLiteral("x"), ox + 30.0 + i * 40.0);
      point.setAttribute(QStringLiteral("y"), oy + 30.0 + i * 50.0);
      point.setAttribute(QStringLiteral("datum"), QStringLiteral("세계측지계"));
      point.setAttribute(QStringLiteral("ellipsoid"), QStringLiteral("GRS80"));
      point.setAttribute(QStringLiteral("projection"), QStringLiteral("UTM-K"));
      point.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(ox + 30.0 + i * 40.0, oy + 30.0 + i * 50.0)));
      if (!control->addFeature(point)) return {};
    }
    if (!control->commitChanges()) return {};

    auto* artifact = LayerOps::ensureDomainLayer(&project, path, QStringLiteral("artifact_point"),
                                                 QStringLiteral("유물"), &error);
    if (!artifact || !artifact->startEditing()) return {};
    QgsFeature artifactFeature(artifact->fields());
    artifactFeature.setAttribute(QStringLiteral("kind"), QStringLiteral("토기"));
    artifactFeature.setAttribute(QStringLiteral("period"), QStringLiteral("청동기"));
    artifactFeature.setAttribute(QStringLiteral("artifact_no"), QStringLiteral("A-1"));
    artifactFeature.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(ox + 25.0, oy + 35.0)));
    if (!artifact->addFeature(artifactFeature) || !artifact->commitChanges()) return {};

    QgsVectorLayer source(
        QStringLiteral("Polygon?crs=%1&field=name:string").arg(authId),
        QStringLiteral("fixture"), QStringLiteral("memory"));
    QgsFeature referenceFeature(source.fields());
    referenceFeature.setAttribute(0, QStringLiteral("외부경계"));
    referenceFeature.setGeometry(QgsGeometry::fromRect(QgsRectangle(ox - 50.0, oy - 50.0, ox + 150.0, oy + 150.0)));
    if (!source.isValid() || !source.dataProvider()->addFeature(referenceFeature)) return {};
    const QString referencePath = m_files.filePath(name + QStringLiteral("-ref.shp"));
    QgsVectorFileWriter::SaveVectorOptions options;
    options.driverName = QStringLiteral("ESRI Shapefile");
    options.fileEncoding = QStringLiteral("UTF-8");
    if (QgsVectorFileWriter::writeAsVectorFormatV3(&source, referencePath, project.transformContext(),
                                                   options) != QgsVectorFileWriter::NoError)
      return {};
    auto* reference = new QgsVectorLayer(referencePath, QStringLiteral("외부경계"), QStringLiteral("ogr"));
    if (!reference->isValid()) {
      delete reference;
      return {};
    }
    LayerOps::markReferenceLayer(reference);
    project.addMapLayer(reference);

    const QgsRectangle sheetExtent(ox - 10.0, oy - 10.0, ox + 110.0, oy + 110.0);
    if (!addComposedUserSheet(&project, surveyArea, sheetExtent)) return {};
    if (!SurveyStorage::writeEmbedded(&project, path, &error)) return {};
    return path;
  }
  static QgsVectorLayer* findExternalReference(QgsProject* project, const QString& referencePath) {
    if (!project) return nullptr;
    const QString want = QFileInfo(referencePath).absoluteFilePath();
    for (auto* layer : project->mapLayers()) {
      auto* vector = qobject_cast<QgsVectorLayer*>(layer);
      if (!vector) continue;
      const QString source = QFileInfo(vector->source().section(QLatin1Char('|'), 0, 0)).absoluteFilePath();
      if (source.compare(want, Qt::CaseInsensitive) == 0) return vector;
      if (LayerOps::isReferenceLayer(vector) && vector->providerType() == QLatin1String("ogr") &&
          (vector->name().contains(QStringLiteral("외부")) || source.endsWith(QLatin1String("-ref.shp"), Qt::CaseInsensitive)))
        return vector;
    }
    return nullptr;
  }
  static QgsVectorLayer* ensureFileReference(QgsProject* project, const QString& referencePath) {
    if (auto* existing = findExternalReference(project, referencePath)) {
      LayerOps::markReferenceLayer(existing);
      return existing;
    }
    auto* reference = new QgsVectorLayer(referencePath, QStringLiteral("외부경계"), QStringLiteral("ogr"));
    if (!reference->isValid()) {
      delete reference;
      return nullptr;
    }
    LayerOps::markReferenceLayer(reference);
    project->addMapLayer(reference);
    return reference;
  }
  static bool hasNoAutosaveTimer(MainWindow& window) {
    // layerWatchTimer, shellSyncTimer (300 ms badge/chip merge, MainWindowChrome.cpp) and mapDisplayRefresh (700 ms redraw after a resize) write no file.
    for (auto* timer : window.findChildren<QTimer*>(QString(), Qt::FindDirectChildrenOnly))
      if (timer->isActive() && timer->objectName() != QLatin1String("layerWatchTimer") &&
          timer->objectName() != QLatin1String("shellSyncTimer") && timer->objectName() != QLatin1String("mapDisplayRefresh") && timer->interval() > 0 &&
          timer->interval() <= 60000)
        return false;
    return true;
  }
  struct MenuActionState {
    QString id;
    QString text;
    QString toolTip;
    bool enabled = false;
    bool separator = false;
  };
  struct LayerMenuState {
    bool seen = false;
    QString name;
    QList<MenuActionState> actions;
  };
  // 도형을 다 그린 뒤 무엇이 떴는지(선택창·이름번호 창).
  struct DrawAnswer {
    bool menuSeen = false;
    bool formSeen = false;
    QStringList menuItems;
  };
  static LayerMenuState inspectLayerMenu(MainWindow& window, QgsLayerTreeView* tree,
                                         const QPoint& viewportPosition,
                                         const QString& triggerId = {}) {
    LayerMenuState state;
    QTimer inspect;
    inspect.setSingleShot(true);
    connect(&inspect, &QTimer::timeout, &window, [&window, &state, triggerId] {
      auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
      if (!menu) return;
      state.seen = true;
      state.name = menu->objectName();
      for (const auto* action : menu->actions())
        state.actions.append({action->objectName(), action->text(), action->toolTip(),
                              action->isEnabled(), action->isSeparator()});
      const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
      const QString sample = window.property("qaMenuKind").toString();
      if (!output.isEmpty() && !sample.isEmpty())
        menu->grab().save(QDir(output).filePath(QStringLiteral("menu-%1.png").arg(sample)));
      if (!triggerId.isEmpty()) {
        for (auto* action : menu->actions()) {
          if (action->objectName() == triggerId && action->isEnabled()) {
            action->trigger();
            break;
          }
        }
      }
      menu->close();
    });
    inspect.start(0);
    window.showLayerTreeContextMenu(tree, viewportPosition);
    return state;
  }
  static LayerMenuState inspectMapMenu(MainWindow& window, const QPoint& point) {
    LayerMenuState state;
    QTimer inspect;
    inspect.setSingleShot(true);
    connect(&inspect, &QTimer::timeout, &window, [&state] {
      auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
      if (!menu) return;
      state.seen = true;
      state.name = menu->objectName();
      for (const auto* action : menu->actions())
        state.actions.append({action->objectName(), action->text(), action->toolTip(),
                              action->isEnabled(), action->isSeparator()});
      menu->close();
    });
    inspect.start(0);
    if (!QMetaObject::invokeMethod(&window, "onMapContextMenu", Qt::DirectConnection, Q_ARG(QPoint, point)))
      return {};
    return state;
  }
  QgsMapLayer* makeMenuReference(const QString& kind) {
    std::unique_ptr<QgsMapLayer> layer;
    if (kind == QLatin1String("satellite")) {
      layer = std::make_unique<QgsRasterLayer>(
          QStringLiteral("type=xyz&url=https://menu-fixture.invalid/{z}/{x}/{y}.png&zmin=0&zmax=18"),
          QStringLiteral("위성"), QStringLiteral("wms"));
    } else if (kind == QLatin1String("cadastral")) {
      QUrlQuery uri;
      uri.addQueryItem(QStringLiteral("url"), QUrl::fromLocalFile(
          QDir(s_testSettingsPath).filePath(QStringLiteral("cadastral-capabilities.xml"))).toString());
      uri.addQueryItem(QStringLiteral("layers"), QStringLiteral("lp_pa_cbnd_bonbun"));
      uri.addQueryItem(QStringLiteral("styles"), QStringLiteral("lp_pa_cbnd_bonbun"));
      uri.addQueryItem(QStringLiteral("format"), QStringLiteral("image/png"));
      uri.addQueryItem(QStringLiteral("crs"), QStringLiteral("EPSG:5187"));
      layer = std::make_unique<QgsRasterLayer>(uri.toString(QUrl::FullyEncoded),
                                               QStringLiteral("지적"), QStringLiteral("wms"));
    } else if (kind == QLatin1String("dem")) {
      const QString path = m_files.filePath(QStringLiteral("menu-elevation.tif"));
      GDALDriverH driver = GDALGetDriverByName("GTiff");
      if (!driver) return nullptr;
      GDALDatasetH dataset = GDALCreate(driver, path.toUtf8().constData(), 4, 4, 1, GDT_Float32, nullptr);
      if (!dataset) return nullptr;
      double transform[] = {190000.0, 10.0, 0.0, 560040.0, 0.0, -10.0};
      const bool written = GDALSetGeoTransform(dataset, transform) == CE_None &&
          GDALSetProjection(dataset, QgsProject::instance()->crs().toWkt().toUtf8().constData()) == CE_None &&
          GDALFillRaster(GDALGetRasterBand(dataset, 1), 100.0, 0.0) == CE_None;
      GDALClose(dataset);
      if (!written) return nullptr;
      layer = std::make_unique<QgsRasterLayer>(path, QStringLiteral("DEM"), QStringLiteral("gdal"));
    } else if (kind == QLatin1String("imported_raster")) {
      const QString path = m_files.filePath(QStringLiteral("menu-imported.png"));
      QImage image(8, 8, QImage::Format_RGB32);
      image.fill(Qt::white);
      if (!image.save(path)) return nullptr;
      QFile worldFile(m_files.filePath(QStringLiteral("menu-imported.pgw")));
      if (!worldFile.open(QIODevice::WriteOnly) ||
          worldFile.write("1\n0\n0\n-1\n190000.5\n560007.5\n") < 0) return nullptr;
      worldFile.close();
      layer = std::make_unique<QgsRasterLayer>(path, QStringLiteral("가져온 현장 도면"), QStringLiteral("gdal"));
      layer->setCrs(QgsProject::instance()->crs());
      layer->setCustomProperty(QStringLiteral("ka_hgis/imported_reference"), true);
    } else if (kind == QLatin1String("external_shp")) {
      QgsVectorLayer source(QStringLiteral("Polygon?crs=EPSG:5187&field=name:string"),
                            QStringLiteral("fixture"), QStringLiteral("memory"));
      QgsFeature feature(source.fields());
      feature.setAttribute(0, QStringLiteral("외부 경계"));
      feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(190000, 560000, 190100, 560100)));
      if (!source.isValid() || !source.dataProvider()->addFeature(feature)) return nullptr;
      const QString path = m_files.filePath(QStringLiteral("menu-external.shp"));
      QgsVectorFileWriter::SaveVectorOptions options;
      options.driverName = QStringLiteral("ESRI Shapefile");
      options.fileEncoding = QStringLiteral("UTF-8");
      if (QgsVectorFileWriter::writeAsVectorFormatV3(&source, path,
          QgsProject::instance()->transformContext(), options) != QgsVectorFileWriter::NoError)
        return nullptr;
      // A cadastral-looking display name must not turn an actual vector into WMS.
      layer = std::make_unique<QgsVectorLayer>(path, QStringLiteral("지적 외부 경계"), QStringLiteral("ogr"));
    }
    if (!layer || !layer->isValid()) return nullptr;
    if (kind != QLatin1String("external_shp")) LayerOps::markReferenceLayer(layer.get());
    return QgsProject::instance()->addMapLayer(layer.release());
  }
private slots:
  void compactLayerListKeepsHierarchyAndMapLabelSize() {
    QgsProject project;
    auto* group=project.layerTreeRoot()->addGroup(QStringLiteral("참조 지도"));
    auto* layersGroup=group->addGroup(QStringLiteral("수치지형도"));
    QgsVectorLayer* first=nullptr;
    for(int i=0;i<18;++i) {
      auto* layer=new QgsVectorLayer(QStringLiteral("LineString?crs=EPSG:5187&field=name:string"),
          QStringLiteral("%1 · %2").arg(i+1).arg(i%2?QStringLiteral("지형 등고선"):QStringLiteral("조사 주변 경계")),QStringLiteral("memory"));
      QVERIFY(layer->isValid());
      LayerOps::applyNameAttributeLabels(layer,QStringLiteral("name"),8.,false);
      project.addMapLayer(layer,false);
      auto* node=layersGroup->addLayer(layer);
      if(i==0) first=layer;
      if(i==2) node->setItemVisibilityChecked(false);
    }
    const auto labelSize=LayerOps::labelFontSize(first,8.);
    QgsLayerTreeModel baselineModel(project.layerTreeRoot());
    QgsLayerTreeView baseline;
    baseline.setModel(&baselineModel); baseline.resize(420,460); baseline.show(); baseline.expandAll();
    KaLayerInformationModel model(&project,false);
    model.setFlag(QgsLayerTreeModel::AllowNodeChangeVisibility);
    KaLayerInformationView tree;
    tree.setModel(&model); KaLayerInformationModel::configureView(&tree);
    tree.resize(420,460); tree.show(); tree.expandAll();
    QTest::qWait(50);
    const auto groupItem=tree.node2index(group);
    const auto layerItem=tree.node2index(project.layerTreeRoot()->findLayer(first->id()));
    const int oldHeight=baseline.visualRect(baseline.node2index(group)).height();
    const int newHeight=tree.visualRect(groupItem).height();
    qInfo()<<"LAYER_ROW_HEIGHT"<<oldHeight<<newHeight<<"FONT_PX"<<baseline.font().pixelSize()<<tree.font().pixelSize();
    QCOMPARE(tree.font().pixelSize(),10);
    QVERIFY(newHeight>0 && newHeight<=24 && newHeight<oldHeight);
    QVERIFY(tree.alternatingRowColors());
    const auto baseGroup=model.node2index(group);
    const auto baseLayer=model.node2index(project.layerTreeRoot()->findLayer(first->id()));
    QVERIFY(model.data(baseGroup,Qt::FontRole).value<QFont>().bold());
    QVERIFY(!model.data(baseLayer,Qt::FontRole).value<QFont>().bold());
    QVERIFY(model.data(baseGroup,Qt::ForegroundRole)!=model.data(baseLayer,Qt::ForegroundRole));
    const auto visibleColor=model.data(baseLayer,Qt::ForegroundRole);
    group->setItemVisibilityChecked(false);
    QCoreApplication::processEvents();
    QVERIFY(model.data(baseLayer,Qt::ForegroundRole)!=visibleColor);
    // 꺼진 레이어는 글자색으로만 구분한다. 한글 기울임꼴은 억지로 비튼 글자다.
    QVERIFY(!model.data(baseLayer,Qt::FontRole).value<QFont>().italic());
    group->setItemVisibilityChecked(true);
    QCoreApplication::processEvents();
    QCOMPARE(model.data(baseLayer,Qt::ForegroundRole),visibleColor);
    QCOMPARE(LayerOps::labelFontSize(first,8.),labelSize);
    // Click the compact row's native visibility control, preserving label settings.
    const QRect row=tree.visualRect(layerItem);
    QTest::mouseClick(tree.viewport(),Qt::LeftButton,Qt::NoModifier,QPoint(row.left()+10,row.center().y()));
    QTRY_VERIFY(!project.layerTreeRoot()->findLayer(first->id())->isVisible());
    QCOMPARE(LayerOps::labelFontSize(first,8.),labelSize);
    tree.clearSelection();
    const auto output=qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    if(!output.isEmpty()) {
      QVERIFY(QDir().mkpath(output));
      QVERIFY(tree.grab().save(QDir(output).filePath(QStringLiteral("compact-layer-list.png"))));
    }
  }

  void layerInformationControlsIntegrateWithMapAndDrawingStudio_data() {
    QTest::addColumn<QSize>("requestedSize");
    QTest::addColumn<bool>("maximized");
    if (qEnvironmentVariableIsEmpty("KA_HGIS_SMALL_SCREEN_QA")) {
      QTest::newRow("default") << QSize(1280, 900) << false;
      return;
    }
    const qreal factor = qEnvironmentVariable("QT_SCALE_FACTOR", "1").toDouble();
    QTest::newRow("1920-maximized") << QSize(qRound(1920 / factor), qRound(1080 / factor)) << true;
    QTest::newRow("1920-restored") << QSize(qRound(1600 / factor), qRound(900 / factor)) << false;
    if (qFuzzyCompare(factor, 1.0))
      QTest::newRow("1366-client") << QSize(1366, 768) << false;
  }

  void layerInformationControlsIntegrateWithMapAndDrawingStudio() {
    QFETCH(QSize, requestedSize);
    QFETCH(bool, maximized);
    const QString path = makeSurvey(QStringLiteral("글자 체크 합성 조사"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    if (qEnvironmentVariableIsEmpty("KA_HGIS_SMALL_SCREEN_QA")) window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setWindowState(Qt::WindowNoState); window.resize(requestedSize);
    if (maximized) window.showMaximized(); else window.show();
    auto* project = QgsProject::instance();
    auto* canvas = window.findChild<QgsMapCanvas*>(QStringLiteral("mapCanvas"));
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    auto* panel = window.findChild<KaLayerInformationPanel*>();
    auto* model = tree ? qobject_cast<KaLayerInformationModel*>(tree->layerTreeModel()) : nullptr;
    QVERIFY(canvas && tree && panel && model);
    QVERIFY(!model->layoutMode());
    QCOMPARE(model->columnCount(), 2);
    auto* layer = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187&field=nm:string(80)"),
                                      QStringLiteral("합성 주변유적"), QStringLiteral("memory"));
    QVERIFY(layer->isValid());
    QVERIFY(layer->startEditing());
    QgsFeature feature(layer->fields());
    feature.setAttribute(QStringLiteral("nm"), QStringLiteral("체크로 표시하는 유적"));
    feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(190020., 560020., 190060., 560060.)));
    QVERIFY(layer->addFeature(feature));
    QVERIFY(layer->commitChanges());
    QVERIFY(layer->getFeatures().nextFeature(feature));
    QVERIFY(HeritageStyle::apply(layer, HeritageDataset::SurfaceSurveyArea, QStringLiteral("nm")).ok);
    QgsPalLayerSettings labels;
    labels.fieldName = QStringLiteral("nm");
    layer->setLabeling(new QgsVectorLayerSimpleLabeling(labels));
    layer->setLabelsEnabled(true);
    LayerOps::markReferenceLayer(layer);
    project->addMapLayer(layer, false);
    auto* reference = project->layerTreeRoot()->findGroup(QStringLiteral("참조 지도"));
    if (!reference) reference = project->layerTreeRoot()->addGroup(QStringLiteral("참조 지도"));
    auto* node = reference->addGroup(HeritageStyle::layerName(HeritageDataset::SurfaceSurveyArea))->addLayer(layer);
    const auto originalGeometry = layer->getFeature(feature.id()).geometry().asWkb();
    tree->setFixedWidth(220);
    tree->expandAll();
    tree->setCurrentLayer(layer);
    const auto item = tree->node2index(node).siblingAtColumn(1);
    QVERIFY(item.isValid());
    tree->scrollTo(item);
    QCoreApplication::processEvents();
    QVERIFY(tree->isVisible() && panel->isVisible());
    QVERIFY(!tree->isHeaderHidden());
    QTRY_VERIFY(tree->header()->sectionViewportPosition(1) + tree->header()->sectionSize(1) <= tree->viewport()->width());
    QVERIFY(tree->header()->sectionSize(0) > 0);
    QVERIFY(tree->header()->sectionSize(1) >= 70);
    auto* checkbox = panel->findChild<QCheckBox*>(QStringLiteral("layerLabelVisible"));
    QVERIFY(checkbox);
    QTRY_VERIFY(checkbox->isChecked());
    const QRect cell = tree->visualRect(item);
    qInfo()<<"LAYER_INFORMATION_VIEWPORT"<<tree->viewport()->rect()<<"CELL"<<cell;
    if(!qEnvironmentVariableIsEmpty("KA_HGIS_QA_OUTPUT_DIR")) {
      const auto diagnosticDirectory=qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
      QDir().mkpath(diagnosticDirectory);
      window.grab().save(QDir(diagnosticDirectory).filePath(QStringLiteral("layer-list-integration-before-click.png")));
    }
    QVERIFY(tree->viewport()->rect().contains(cell.center()));
    QVERIFY2(tree->viewport()->height() >= 4 * 22, "four layer rows remain available");
    QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(cell.left() + 10, cell.center().y()));
    QTRY_VERIFY(!layer->labelsEnabled());
    QVERIFY(node->isVisible());
    QCOMPARE(layer->featureCount(), 1);
    QCOMPARE(layer->getFeature(feature.id()).geometry().asWkb(), originalGeometry);
    QTRY_VERIFY(!checkbox->isChecked());
    auto* settingsToggle = panel->findChild<QToolButton*>(QStringLiteral("layerInformationToggle"));
    QVERIFY(settingsToggle);
    QTest::mouseClick(settingsToggle, Qt::LeftButton);
    QTRY_VERIFY(checkbox->isVisible());
    auto* settingsScroll = panel->findChild<QScrollArea*>(QStringLiteral("layerInformationScroll"));
    QVERIFY(settingsScroll);
    settingsScroll->ensureWidgetVisible(checkbox);
    QCoreApplication::processEvents();
    QVERIFY(settingsScroll->viewport()->rect().contains(checkbox->mapTo(settingsScroll->viewport(), checkbox->rect().center())));
    QTest::mouseClick(checkbox, Qt::LeftButton, Qt::NoModifier, QPoint(8, checkbox->height() / 2));
    QTRY_VERIFY(layer->labelsEnabled());
    QCOMPARE(layer->labeling()->settings().fieldName, QStringLiteral("nm"));

    canvas->setDestinationCrs(project->crs());
    canvas->setLayers({layer});
    canvas->setExtent(QgsRectangle(189900., 559900., 190200., 560200.));
    const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    if (!output.isEmpty()) {
      QVERIFY(QDir().mkpath(output));
      QSignalSpy rendered(canvas, &QgsMapCanvas::mapCanvasRefreshed);
      canvas->setRenderFlag(true);
      canvas->refresh();
      QTRY_VERIFY_WITH_TIMEOUT(!rendered.isEmpty(), 5000);
      tree->setMaximumWidth(QWIDGETSIZE_MAX);
      tree->setMinimumWidth(300);
      QCoreApplication::processEvents();
      QVERIFY(window.grab().save(QDir(output).filePath(QStringLiteral("layer-information-map.png"))));
      QVERIFY(panel->parentWidget()->grab().save(QDir(output).filePath(QStringLiteral("layer-information-controls.png"))));
      canvas->setRenderFlag(false);
    }

    KaDrawingStudio studio(project, canvas, 297., 210.);
    if (qEnvironmentVariableIsEmpty("KA_HGIS_SMALL_SCREEN_QA")) studio.setAttribute(Qt::WA_DontShowOnScreen);
    studio.resize(requestedSize);
    if (maximized) studio.showMaximized(); else studio.show();
    studio.centerOnMapCanvas();
    auto* layoutTree = studio.findChild<QgsLayerTreeView*>(QStringLiteral("layoutLayerTree"));
    auto* layoutPanel = studio.findChild<KaLayerInformationPanel*>();
    auto* layoutModel = layoutTree ? qobject_cast<KaLayerInformationModel*>(layoutTree->layerTreeModel()) : nullptr;
    auto* view = studio.findChild<QgsLayoutView*>();
    QVERIFY(layoutTree && layoutPanel && layoutModel && view && view->currentLayout());
    QVERIFY(layoutModel->layoutMode());
    layoutTree->setFixedWidth(220);
    layoutTree->expandAll();
    layoutTree->setCurrentLayer(layer);
    const auto layoutItem = layoutTree->node2index(node).siblingAtColumn(1);
    QVERIFY(layoutItem.isValid());
    layoutTree->scrollTo(layoutItem);
    layoutTree->setCurrentIndex(layoutItem);
    layoutTree->setFocus();
    QTRY_VERIFY(layoutPanel->isVisible());
    QTRY_VERIFY(layoutTree->header()->sectionViewportPosition(1) + layoutTree->header()->sectionSize(1) <= layoutTree->viewport()->width());
    auto* map = dynamic_cast<QgsLayoutItemMap*>(view->currentLayout()->itemById(QStringLiteral("ka_map")));
    QVERIFY(map);
    QTRY_VERIFY_WITH_TIMEOUT(map->layerStyleOverrides().contains(layer->id()), 5000);
    auto* numbers = HeritageLayoutNumbers::forMap(map);
    QVERIFY(numbers);
    QTRY_COMPARE_WITH_TIMEOUT(numbers->entries().size(), 1, 5000);
    auto* layoutCheckbox = layoutPanel->findChild<QCheckBox*>(QStringLiteral("layerLabelVisible"));
    QVERIFY(layoutCheckbox);
    QTRY_VERIFY(layoutCheckbox->text().contains(QStringLiteral("번호")));
    QTest::keyClick(layoutTree, Qt::Key_Space);
    QTRY_VERIFY(!layer->customProperty(QStringLiteral("ka_hgis/layout_numbers_visible"), true).toBool());
    QTRY_VERIFY_WITH_TIMEOUT(numbers->entries().isEmpty(), 5000);
    QVERIFY(layer->labelsEnabled());
    QVERIFY(node->isVisible());
    QCOMPARE(layer->getFeature(feature.id()).geometry().asWkb(), originalGeometry);
    std::unique_ptr<QgsVectorLayer> drawing(layer->clone());
    QgsMapLayerStyle(map->layerStyleOverrides().value(layer->id())).writeToLayer(drawing.get());
    QVERIFY(!drawing->labelsEnabled());
    auto* layoutSettingsToggle = layoutPanel->findChild<QToolButton*>(QStringLiteral("layerInformationToggle"));
    QVERIFY(layoutSettingsToggle);
    QTest::mouseClick(layoutSettingsToggle, Qt::LeftButton);
    QTRY_VERIFY(layoutCheckbox->isVisible());
    auto* layoutSettingsScroll = layoutPanel->findChild<QScrollArea*>(QStringLiteral("layerInformationScroll"));
    QVERIFY(layoutSettingsScroll);
    layoutSettingsScroll->ensureWidgetVisible(layoutCheckbox);
    QCoreApplication::processEvents();
    QVERIFY(layoutSettingsScroll->viewport()->rect().contains(layoutCheckbox->mapTo(layoutSettingsScroll->viewport(), layoutCheckbox->rect().center())));
    QTest::mouseClick(layoutCheckbox, Qt::LeftButton, Qt::NoModifier, QPoint(8, layoutCheckbox->height() / 2));
    QTRY_VERIFY(layer->customProperty(QStringLiteral("ka_hgis/layout_numbers_visible"), true).toBool());
    QTRY_COMPARE_WITH_TIMEOUT(numbers->entries().size(), 1, 5000);
    QCOMPARE(numbers->entries().first().number, 1);
    if (!output.isEmpty()) {
      QSignalSpy rendered(map, &QgsLayoutItemMap::previewRefreshed);
      map->refresh();
      view->viewport()->update();
      QTRY_VERIFY_WITH_TIMEOUT(!rendered.isEmpty(), 5000);
      layoutTree->setMaximumWidth(QWIDGETSIZE_MAX);
      layoutTree->setMinimumWidth(300);
      QCoreApplication::processEvents();
      QVERIFY(studio.grab().save(QDir(output).filePath(QStringLiteral("layer-information-studio.png"))));
    }
    // 도면 탭의 「라벨·명칭 속성」도 지도 탭과 같은 줄을 보인다.
    const auto labelMenuRows = [&window](QgsLayerTreeView* view, const QModelIndex& row) {
      QMap<QString, QPair<bool, QString>> rows;
      QTimer::singleShot(0, &window, [&rows]() {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (!menu) return;
        auto* labels = menu->findChild<QAction*>(QStringLiteral("layer.labels"));
        if (labels && labels->menu())
          for (const auto* action : labels->menu()->actions())
            rows.insert(action->objectName(), {action->isEnabled(), action->toolTip()});
        menu->close();
      });
      window.showLayerTreeContextMenu(view, view->visualRect(row).center());
      return rows;
    };
    const auto mapRows = labelMenuRows(tree, tree->node2index(node));
    const auto layoutRows = labelMenuRows(layoutTree, layoutTree->node2index(node));
    QVERIFY(mapRows.contains(QStringLiteral("layer.labelField")));
    QCOMPARE(layoutRows.keys(), mapRows.keys());
    QVERIFY(layoutRows.value(QStringLiteral("layer.labelField")).first);
    // 도면 번호 동그라미는 크기가 정해져 있다. 누를 수 없게 두고 까닭을 보인다.
    QVERIFY(!layoutRows.value(QStringLiteral("layer.labelSize")).first);
    QVERIFY(!layoutRows.value(QStringLiteral("layer.labelSize")).second.isEmpty());
    project->setDirty(false);
  }

  void geoTiffButtonOnlyEnabledOnMapTab() {
    MainWindow window;
    auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("viewTabs"));
    auto* button = window.findChild<QToolButton*>(QStringLiteral("btnMapGeoTiff"));
    QVERIFY(tabs && button);
    QVERIFY(!button->isEnabled());
    int mapIndex = -1;
    for (int i = 0; i < tabs->count(); ++i)
      if (tabs->tabText(i) == QStringLiteral("지도")) mapIndex = i;
    QVERIFY(mapIndex >= 0);
    tabs->setCurrentIndex(mapIndex);
    QVERIFY(button->isEnabled());
    QVERIFY(button->defaultAction());
    QCOMPARE(button->defaultAction()->objectName(), QStringLiteral("actionMapGeoTiff"));
    tabs->setCurrentIndex(0);
    QVERIFY(!button->isEnabled());
  }
  void windowGeometryFitsLogicalMonitors_data() {
    QTest::addColumn<QRect>("available");
    QTest::newRow("1024x768") << QRect(0, 0, 1024, 728);
    QTest::newRow("1366x768") << QRect(0, 0, 1366, 728);
    QTest::newRow("1920x1080") << QRect(0, 0, 1920, 1040);
    QTest::newRow("150-percent") << QRect(0, 0, 910, 472);
    QTest::newRow("200-percent") << QRect(0, 0, 960, 500);
    QTest::newRow("negative-monitor") << QRect(-1920, -1080, 1920, 1040);
  }
  void windowGeometryFitsLogicalMonitors() {
    QFETCH(QRect, available);
    const QMargins frame(8, 32, 8, 8);
    for (const QRect& requested : {QRect(3000, 1600, 1280, 900), QRect(-3000, -1800, 980, 720)}) {
      const QRect fitted = KaWindowGeometry::fittedClientRect(requested, available, frame);
      QVERIFY(available.contains(fitted.marginsAdded(frame)));
      QCOMPARE(KaWindowGeometry::fittedClientRect(fitted, available, frame), fitted);
    }
  }
  void mainWindowFitsAvailableScreenOnShow() {
    MainWindow window;
    disableRendering(window);
    window.resize(1920, 1080);
    window.show();
    QCoreApplication::processEvents();
    const QRect available = window.screen()->availableGeometry();
    QVERIFY2(available.contains(window.frameGeometry()), qPrintable(
        QStringLiteral("screen %1x%2, window %3x%4")
            .arg(available.width()).arg(available.height())
            .arg(window.frameGeometry().width()).arg(window.frameGeometry().height())));
    QgsProject::instance()->setDirty(false);
  }
  // 사용자 신고 2026-10-05: in 새 모양 the window asked for 2415 px on a 1920 screen, so the 단면도 right panel sat
  // off screen. Every tab must fit a 1920 screen.
  void everyTabFitsA1920ScreenInPaper() {
    KaTheme::DisplayOptions paper;
    paper.paperLook = true;
    KaTheme::setDisplayOptions(qApp, paper);
    const auto restore = qScopeGuard([] { KaTheme::setDisplayOptions(qApp, KaTheme::DisplayOptions()); });
    MainWindow window;
    disableRendering(window);
    window.resize(1600, 1000);
    window.show();
    QCoreApplication::processEvents();
    auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("viewTabs"));
    QVERIFY(tabs);
    for (int i = 0; i < tabs->count(); ++i) {
      QWidget* page = tabs->widget(i);
      QWidget* widest = page;
      for (QWidget* child : page->findChildren<QWidget*>())
        if (child->isVisibleTo(page) && child->minimumSizeHint().width() > 1500 &&
            child->minimumSizeHint().width() <= widest->minimumSizeHint().width())
          widest = child;
      QVERIFY2(page->minimumSizeHint().width() <= 1880,
               qPrintable(QStringLiteral("%1: %2 px, narrowest wide part %3 (%4) %5 px")
                              .arg(tabs->tabText(i)).arg(page->minimumSizeHint().width())
                              .arg(widest->objectName(), QString::fromLatin1(widest->metaObject()->className()))
                              .arg(widest->minimumSizeHint().width())));
    }
    QVERIFY2(window.minimumSizeHint().width() <= 1880, qPrintable(QString::number(window.minimumSizeHint().width())));
    QgsProject::instance()->setDirty(false);
  }
  // 2026-10-10 사용자 모니터(3440×1440, 150 %)는 앱에 2293×912로 보인다(작업표시줄 제외). 세로가 낮아도 홈·지도·
  // 도면이 잘리지 않아야 한다(docs/intent/2026-10-10-monitor-3440x1440-fit.md). 1920×1040은 흔한 1080p 작업 영역.
  // ctest save_open_monitor 는 2560×1440 가상 화면(cmake/offscreen-2560.json)에서 돈다.
  void monitorFitHomeMapDrawing_data() {
    QTest::addColumn<QSize>("size");
    QTest::addColumn<bool>("largeText");
    QTest::newRow("2293x912-large") << QSize(2293, 912) << true;
    QTest::newRow("2293x912") << QSize(2293, 912) << false;
    QTest::newRow("1920x1040-large") << QSize(1920, 1040) << true;
    QTest::newRow("1920x1040") << QSize(1920, 1040) << false;
  }
  void monitorFitHomeMapDrawing() {
    QFETCH(QSize, size);
    QFETCH(bool, largeText);
    KaTheme::DisplayOptions look;
    look.paperLook = look.highContrast = true;
    look.largeText = largeText;
    KaTheme::setDisplayOptions(qApp, look);
    const auto restore = qScopeGuard([] { KaTheme::setDisplayOptions(qApp, KaTheme::DisplayOptions()); });
    const QString tag = QString::fromLatin1(QTest::currentDataTag());
    const QString path = makeSurvey(QStringLiteral("모니터맞춤-") + tag);
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    window.setWindowState(Qt::WindowNoState);
    window.resize(size);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCOMPARE(window.size(), size);
    QVERIFY(window.openSurveyGpkg(path));
    auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("viewTabs"));
    QVERIFY(tabs);
    const QString out = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    QStringList clipped;  // every cut part at once, so one run shows the whole screen
    const auto shot = [&](QWidget* widget, const QString& name) {
      if (!out.isEmpty()) QVERIFY(QDir().mkpath(out) && widget->grab().save(QDir(out).filePath(tag + name)));
    };
    // 홈: 「작업 순서」는 스크롤 없이 다 보이고, 「최근 조사」는 세 줄이 들어가고, 폴더 칸은 말줄임표로 줄인다.
    tabs->setCurrentIndex(0);
    QCoreApplication::processEvents();
    shot(&window, QStringLiteral("-home.png"));
    auto* steps = window.findChild<QScrollArea*>(QStringLiteral("startRightColumn"));
    auto* recent = window.findChild<QTableWidget*>(QStringLiteral("recentSurveyList"));
    auto* folder = window.findChild<QLabel*>(QStringLiteral("startCellFolder"));
    QVERIFY(steps && recent && folder);
    const int folderNeeds = folder->fontMetrics().horizontalAdvance(folder->text());
    qInfo() << tag << "home: steps scroll" << steps->verticalScrollBar()->maximum() << "recent viewport"
            << recent->viewport()->height() << "folder" << folderNeeds << "/" << folder->contentsRect().width();
    if (steps->verticalScrollBar()->maximum() > 0)
      clipped << QStringLiteral("작업 순서 %1 px 잘림").arg(steps->verticalScrollBar()->maximum());
    if (recent->viewport()->height() < 3 * 48)
      clipped << QStringLiteral("최근 조사 %1 px(세 줄 안 됨)").arg(recent->viewport()->height());
    if (folderNeeds > folder->contentsRect().width())
      clipped << QStringLiteral("폴더 칸 %1/%2 px").arg(folderNeeds).arg(folder->contentsRect().width());
    // 지도: 레이어 목록이 왼쪽 칸을 주로 쓰고 파일함은 ¼ 이하.
    QVERIFY(QMetaObject::invokeMethod(&window, "showMapWorkspace", Qt::DirectConnection));
    QCoreApplication::processEvents();
    shot(&window, QStringLiteral("-map.png"));
    auto* split = window.findChild<QSplitter*>(QStringLiteral("leftSplit"));
    auto* files = window.findChild<QWidget*>(QStringLiteral("sidebarFilesScroll"));
    QVERIFY(split && files);
    const int filesHeight = files->isVisible() ? files->height() : 0;
    qInfo() << tag << "map: left split" << split->height() << "files" << filesHeight;
    if (filesHeight * 4 > split->height())
      clipped << QStringLiteral("파일함 %1 px / 왼쪽 칸 %2 px").arg(filesHeight).arg(split->height());
    // 도면: 같은 높이의 도면 탭에서 오른쪽 「도면 항목」 칸이 스크롤 없이 다 보인다.
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    QgsMapCanvas canvas;
    canvas.setRenderFlag(false);
    canvas.setDestinationCrs(project.crs());
    canvas.setExtent(QgsRectangle(190000, 560000, 191000, 561000));
    QTabWidget studioTabs;
    KaDrawingStudio studio(&project, &canvas, 210., 297., &studioTabs);
    studio.setParent(&studioTabs, Qt::Widget);
    studioTabs.addTab(&studio, QStringLiteral("도면"));
    studioTabs.resize(tabs->size());
    studioTabs.show();
    QCoreApplication::processEvents();
    shot(&studioTabs, QStringLiteral("-drawing.png"));
    auto* inspector = studio.findChild<QScrollArea*>(QStringLiteral("drawingInspectorScroll"));
    QVERIFY(inspector);
    qInfo() << tag << "drawing: tab" << studioTabs.size() << "inspector scroll" << inspector->verticalScrollBar()->maximum();
    if (inspector->verticalScrollBar()->maximum() > 0)
      clipped << QStringLiteral("도면 항목 %1 px 잘림").arg(inspector->verticalScrollBar()->maximum());
    // 더보기: the whole menu fits the screen height (no scroll arrows).
    for (auto* menu : window.findChildren<QMenu*>())
      for (QAction* action : menu->actions())
        if (action->text() == QStringLiteral("벡터·도면 불러오기") && menu->sizeHint().height() > size.height())
          clipped << QStringLiteral("더보기 메뉴 %1 px").arg(menu->sizeHint().height());
    QgsProject::instance()->setDirty(false);
    QVERIFY2(clipped.isEmpty(), qPrintable(tag + QStringLiteral(": ") + clipped.join(QStringLiteral(" · "))));
  }
  // The same monitor rows for the dialogs opened from those: 검수·제출, 새 조사, 수치지형도 계정. Each fits under the
  // screen height with its buttons inside. (The 단면도 tab was removed on 2026-10-10.)
  void monitorFitDialogs_data() { monitorFitHomeMapDrawing_data(); }
  void monitorFitDialogs() {
    QFETCH(QSize, size);
    QFETCH(bool, largeText);
    KaTheme::DisplayOptions look;
    look.paperLook = look.highContrast = true;
    look.largeText = largeText;
    KaTheme::setDisplayOptions(qApp, look);
    const auto restore = qScopeGuard([] { KaTheme::setDisplayOptions(qApp, KaTheme::DisplayOptions()); });
    const QString tag = QString::fromLatin1(QTest::currentDataTag());
    const QString out = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    const int usable = size.height() - 40;  // the title bar
    QStringList clipped;
    const auto check = [&](QDialog* dialog, const QString& name) {
      dialog->show();
      QVERIFY(QTest::qWaitForWindowExposed(dialog));
      QCoreApplication::processEvents();
      if (!out.isEmpty()) QVERIFY(QDir().mkpath(out) && dialog->grab().save(QDir(out).filePath(tag + QStringLiteral("-") + name + QStringLiteral(".png"))));
      if (dialog->height() > usable) clipped << QStringLiteral("%1 %2 px").arg(name).arg(dialog->height());
      for (auto* button : dialog->findChildren<QPushButton*>())
        if (button->isVisible() && !dialog->rect().contains(QRect(button->mapTo(dialog, QPoint()), button->size())))
          clipped << name + QStringLiteral(" 단추 「%1」 밖").arg(button->text());
      dialog->hide();
    };
    KaSubmitDialog submit;
    check(&submit, QStringLiteral("submit"));
    KaNewSurveyDialog newSurvey(QStringLiteral("EPSG:5187"), 128.6);
    check(&newSurvey, QStringLiteral("new-survey"));
    KaTopographicAccountDialog account;
    check(&account, QStringLiteral("account"));
    QVERIFY2(clipped.isEmpty(), qPrintable(tag + QStringLiteral(": ") + clipped.join(QStringLiteral(" · "))));
  }
  void homePageShowsStrataHeroWithoutOpeningASurvey() {
    // Two remembered surveys give the continue card and the list something to show.
    const QString first = m_files.filePath(QStringLiteral("home-first/광령리.gpkg"));
    const QString second = m_files.filePath(QStringLiteral("home-second/병산동.gpkg"));
    for (const QString& path : {first, second}) {
      QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
      QFile file(path);
      QVERIFY(file.open(QIODevice::WriteOnly));
    }
    {
      QSettings st = RecentSurveys::userSettings();
      RecentSurveys::remember(st, second, QStringLiteral("병산동"));
      RecentSurveys::remember(st, first, QStringLiteral("광령리"));
    }
    MainWindow window;
    disableRendering(window);
    window.resize(1600, 900);
    window.show();
    QCoreApplication::processEvents();
    auto* home = window.findChild<QWidget*>(QStringLiteral("startPage"));
    QVERIFY(home && home->isVisible());
    auto* hero = home->findChild<QWidget*>(QStringLiteral("startHero"));
    QVERIFY(hero && hero->isVisible());
    auto* card = home->findChild<QWidget*>(QStringLiteral("startContinueCard"));
    QVERIFY(card && card->isVisible());
    auto* name = home->findChild<QLabel*>(QStringLiteral("startContinueName"));
    QVERIFY(name);
    QCOMPARE(name->text(), QStringLiteral("광령리"));
    auto* list = home->findChild<QTableWidget*>(QStringLiteral("recentSurveyList"));
    QVERIFY(list && list->rowCount() >= 2);
    // The home screen only offers surveys; nothing opens without a click.
    QCOMPARE(window.windowTitle(), QStringLiteral("Strata"));
    const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    if (!output.isEmpty()) {
      QVERIFY(QDir().mkpath(output));
      QVERIFY(window.grab().save(QDir(output).filePath(QStringLiteral("home-page.png"))));
    }
    QSettings st = RecentSurveys::userSettings();
    RecentSurveys::forget(st, first);
    RecentSurveys::forget(st, second);
    QgsProject::instance()->setDirty(false);
  }
  void drawingToolsGetTheirOwnRowBelowTheRibbon() {
    {
      MainWindow window;
      disableRendering(window);
      window.setWindowState(Qt::WindowNoState); window.resize(1600, 900);
      window.show();
      QCoreApplication::processEvents();
      auto* mainTb = window.findChild<QToolBar*>(QStringLiteral("mainToolbar"));
      auto* sub = window.findChild<QToolBar*>(QStringLiteral("subToolbar"));
      auto* appBar = window.findChild<QWidget*>(QStringLiteral("appBar"));
      auto* draw = window.findChild<QToolButton*>(QStringLiteral("btnDraw"));
      QVERIFY(mainTb && sub && appBar && draw);
      QCOMPARE(draw->text(), QStringLiteral("그리기"));
      const QString path = makeSurvey(QStringLiteral("그리기줄"));
      QVERIFY(!path.isEmpty());
      QVERIFY(window.openSurveyGpkg(path));
      QTRY_VERIFY(draw->isEnabled());
      draw->click();
      QTRY_VERIFY(sub->isVisible());
      // 그리기 도구는 리본 아래 한 줄을 쓰고, 주소 찾기는 리본 줄에 남아 서로 밀어내지 않는다.
      QTRY_VERIFY(sub->geometry().top() >= mainTb->geometry().bottom());
      QVERIFY(appBar->mapTo(&window, QPoint(0, 0)).y() < sub->geometry().top());
      // Closing the window now saves the tools row as showing.
      RecentSurveys::userSettings().setValue(QStringLiteral("MainWindow/state"), window.saveState());
    }
    MainWindow reopened;
    disableRendering(reopened);
    reopened.show();
    QCoreApplication::processEvents();
    RecentSurveys::userSettings().remove(QStringLiteral("MainWindow/state"));
    auto* sub = reopened.findChild<QToolBar*>(QStringLiteral("subToolbar"));
    QVERIFY(sub);
    QVERIFY2(!sub->isVisible(), "복원한 창 배치가 빈 그리기 도구 줄을 다시 띄웠습니다.");
  }
  // The app starts on 홈; the 지도 tab and its sidebar are sized only when it is first shown.
  static void showMapAfterHome(MainWindow& window, const QSize& size) {
    disableRendering(window);
    window.setWindowState(Qt::WindowNoState); window.resize(size);
    window.show();
    QCoreApplication::processEvents();
    if (auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("viewTabs")))
      for (int i = 0; i < tabs->count(); ++i)
        if (tabs->tabText(i) == QStringLiteral("지도")) tabs->setCurrentIndex(i);
    QCoreApplication::processEvents();
  }
  static QStringList windowLayoutKeys() {
    return {QStringLiteral("MainWindow/filesOpen"), QStringLiteral("MainWindow/leftSplit"),
            QStringLiteral("MainWindow/geometry"), QStringLiteral("MainWindow/state"),
            QStringLiteral("MainWindow/mainSplit"), QStringLiteral("MainWindow/mainSplitPanes")};
  }
  static void forgetWindowLayout() {
    QSettings st = RecentSurveys::userSettings();
    for (const QString& key : windowLayoutKeys()) st.remove(key);
  }
  // 처음 켜면(저장된 창 모양이 없으면) 전체 창으로 뜨고, 사용자가 줄인 창 모양은 그대로 기억한다.
  void firstStartIsMaximizedAndAChosenSizeIsKept() {
    forgetWindowLayout();
    const auto cleanup = qScopeGuard([] { forgetWindowLayout(); });
    {
      MainWindow window;
      QVERIFY(window.windowState().testFlag(Qt::WindowMaximized));
      window.showNormal();
      QVERIFY(window.close());  // closeEvent stores the geometry of the normal window
    }
    MainWindow again;
    QVERIFY(!again.windowState().testFlag(Qt::WindowMaximized));
  }
  // 처음 켜면 파일함이 보이고, 파일함 단추로 닫거나 연 상태가 다음 실행까지 남는다.
  void filesBoxStartsOpenAndKeepsTheChosenState() {
    forgetWindowLayout();
    const auto cleanup = qScopeGuard([] { forgetWindowLayout(); });
    {
      QSettings st = RecentSurveys::userSettings();
      // The 파일함 squeezed to 0 px, as earlier starts saved it.
      QSplitter shut(Qt::Vertical);
      shut.addWidget(new QWidget);
      shut.addWidget(new QWidget);
      shut.setSizes({792, 0});
      st.setValue(QStringLiteral("MainWindow/leftSplit"), shut.saveState());
    }
    const auto filesShown = [](MainWindow& window) {
      auto* files = window.findChild<QWidget*>(QStringLiteral("sidebarFilesScroll"));
      auto* split = window.findChild<QSplitter*>(QStringLiteral("leftSplit"));
      // Open, not a 0 px strip. A short column (< 900 px) gives it ¼, a taller one 2/5 (2026-10-10 monitor fit).
      return files && split && files->isVisible() && files->height() >= split->height() / 5;
    };
    const auto toggleOf = [](MainWindow& window) {
      return window.findChild<QToolButton*>(QStringLiteral("sidebarFilesToggle"));
    };
    {
      MainWindow window;
      showMapAfterHome(window, QSize(1600, 900));
      QVERIFY(toggleOf(window) && toggleOf(window)->isChecked());
      QTRY_VERIFY2(filesShown(window), "처음 켠 지도 탭에 파일함이 보이지 않습니다.");
      toggleOf(window)->click();
      QVERIFY(!toggleOf(window)->isChecked());
      QVERIFY(window.close());
    }
    {
      MainWindow window;
      showMapAfterHome(window, QSize(1600, 900));
      QVERIFY2(!toggleOf(window)->isChecked(), "닫고 끝낸 파일함이 다시 열렸습니다.");
      QVERIFY(!window.findChild<QWidget*>(QStringLiteral("sidebarFilesScroll"))->isVisible());
      toggleOf(window)->click();
      QTRY_VERIFY2(filesShown(window), "파일함 단추를 눌러도 파일함이 보이지 않습니다.");
      QVERIFY(window.close());
    }
    {
      MainWindow window;
      showMapAfterHome(window, QSize(1600, 900));
      QVERIFY(toggleOf(window)->isChecked());
      QTRY_VERIFY2(filesShown(window), "열어 두고 끝낸 파일함이 다음 실행에 보이지 않습니다.");
    }
  }
  void narrowWindowKeepsSearchOnTheRibbonRow() {
    for (const QSize size : {QSize(1024, 768), QSize(1280, 720), QSize(1904, 1000)}) {
      MainWindow window;
      disableRendering(window);
      window.setWindowState(Qt::WindowNoState); window.resize(size);
      window.show();
      QCoreApplication::processEvents();
      auto* mainTb = window.findChild<QToolBar*>(QStringLiteral("mainToolbar"));
      auto* appBar = window.findChild<QWidget*>(QStringLiteral("appBar"));
      auto* ribbon = window.findChild<KaBeginnerRibbon*>(QStringLiteral("beginnerRibbon"));
      QVERIFY(mainTb && appBar && ribbon);
      // The toolbar grows and shrinks with the ribbon's size, so no chip is cut off at the bottom.
      QTRY_VERIFY2(ribbon->height() >= ribbon->sizeHint().height() && mainTb->height() >= ribbon->height(),
                   qPrintable(QStringLiteral("%1 폭: 리본 높이 %2 필요 %3 줄 높이 %4")
                                  .arg(size.width()).arg(ribbon->height()).arg(ribbon->sizeHint().height()).arg(mainTb->height())));
      QVERIFY2(!window.findChild<QToolButton*>(QStringLiteral("ribbonOverflow")), "더 많은 작업 단추는 없다");
      QVERIFY(ribbon->parentWidget() == mainTb);
      QCOMPARE(appBar->parentWidget(), mainTb);
      const QRect ribbonBox = ribbon->geometry();
      const QRect searchBox = appBar->geometry();
      QVERIFY2(!ribbonBox.intersects(searchBox) && appBar->isVisible(),
               qPrintable(QStringLiteral("%1 폭(창 %2, 리본 %3, 찾기 %4)에서 찾기 칸이 리본과 겹치거나 숨는다")
                              .arg(size.width())
                              .arg(window.width())
                              .arg(ribbonBox.width())
                              .arg(appBar->isVisible())));
      QVERIFY(qAbs(ribbonBox.center().y() - searchBox.center().y()) < ribbonBox.height());
      // 접지 않는다: 어느 폭에서도 모든 칩이 리본 안에 보인다.
      for (QToolButton* chip : ribbon->tabButtons()) {
        const QRect box(chip->mapTo(ribbon, QPoint()), chip->size());
        QVERIFY2(chip->isVisible() && ribbon->rect().contains(box),
                 qPrintable(QStringLiteral("%1 폭(리본 %2)에서 「%3」가 리본에 다 보이지 않는다")
                                .arg(size.width()).arg(ribbon->width()).arg(chip->text())));
      }
      QgsProject::instance()->setDirty(false);
    }
  }
  void ribbonGroupsMatchTheirPurpose() {
    MainWindow window;
    disableRendering(window);
    auto* ribbon = window.findChild<KaBeginnerRibbon*>(QStringLiteral("beginnerRibbon"));
    QVERIFY(ribbon);
    const auto inGroup = [&](const QString& groupId, QWidget* widget) {
      auto* group = ribbon->group(groupId);
      return widget && group && group->isAncestorOf(widget);
    };
    QVERIFY(inGroup(QStringLiteral("record"), window.findChild<QToolButton*>(QStringLiteral("btnBuffer"))));
    QVERIFY(inGroup(QStringLiteral("fetch"), window.findChild<QToolButton*>(QStringLiteral("btnHeritageFetch"))));
    QVERIFY(inGroup(QStringLiteral("fetch"), window.findChild<QToolButton*>(QStringLiteral("btnTopographic"))));
    QVERIFY(inGroup(QStringLiteral("basemap"), window.findChild<QToolButton*>(QStringLiteral("btnSurveyContour"))));
    QVERIFY(!window.findChild<QToolButton*>(QStringLiteral("btnWeb")));
    QToolButton* more = nullptr;
    for (auto* button : window.findChildren<QToolButton*>()) {
      if (button->text() == QStringLiteral("더보기")) more = button;
    }
    QVERIFY(more && more->menu());
    QVERIFY(more->menu()->findChild<QAction*>(QStringLiteral("actionWebSources")));
    QToolButton* cadastral = nullptr;
    for (auto* button : window.findChildren<QToolButton*>()) {
      if (button->defaultAction() &&
          button->defaultAction()->objectName() == QLatin1String("actionCadastralDownload"))
        cadastral = button;
    }
    QVERIFY(inGroup(QStringLiteral("fetch"), cadastral));
    QVERIFY(inGroup(QStringLiteral("basemap"), window.findChild<QToolButton*>(QStringLiteral("btnOldMaps"))));
    QVERIFY(inGroup(QStringLiteral("out"), window.findChild<QToolButton*>(QStringLiteral("btnRibbonPrint"))));
  }
  void narrowWindowKeepsDrawingAndPrintVisible() {
    for (const int width : {1280, 1536, 1920}) {
      MainWindow window;
      disableRendering(window);
      window.setWindowState(Qt::WindowNoState); window.resize(width, 800);
      window.show();
      QCoreApplication::processEvents();
      auto* ribbon = window.findChild<KaBeginnerRibbon*>(QStringLiteral("beginnerRibbon"));
      auto* save = window.findChild<QToolButton*>(QStringLiteral("ribbonSave"));
      auto* draw = window.findChild<QToolButton*>(QStringLiteral("btnDraw"));
      auto* print = window.findChild<QToolButton*>(QStringLiteral("btnRibbonPrint"));
      QVERIFY(ribbon && save && draw && print);
      QVERIFY2(save->isVisible() && draw->isVisible() && print->isVisible(),
               qPrintable(QStringLiteral("%1 폭(창 %2, 리본 %3)에서 저장 %4 그리기 %5 인쇄 %6")
                              .arg(width)
                              .arg(window.width())
                              .arg(ribbon->width())
                              .arg(save->isVisible())
                              .arg(draw->isVisible())
                              .arg(print->isVisible())));
      auto* basemap = ribbon->group(QStringLiteral("basemap"));
      QVERIFY(basemap);
      if (width >= 1920) {
        auto* fetch = ribbon->group(QStringLiteral("fetch"));
        QVERIFY(fetch);
        QString placed;
        for (const QString& id : {QStringLiteral("survey"), QStringLiteral("out"), QStringLiteral("record"),
                                  QStringLiteral("basemap"), QStringLiteral("fetch"), QStringLiteral("align"),
                                  QStringLiteral("more")}) {
          auto* group = ribbon->group(id);
          placed += QStringLiteral("%1:%2/%3 ")
                        .arg(id)
                        .arg(group && group->parentWidget() == ribbon ? QStringLiteral("in") : QStringLiteral("out"))
                        .arg(group ? group->sizeHint().width() : -1);
        }
        QVERIFY2(basemap->parentWidget() == ribbon && fetch->parentWidget() == ribbon,
                 qPrintable(QStringLiteral("%1 ribbon %2 %3").arg(width).arg(ribbon->width()).arg(placed)));
        for (const char* name : {"btnTerrain", "btnDem", "btnSurveyContour", "btnSoil", "btnPaleo",
                                 "btnOldMaps", "btnHeritageFetch", "btnTopographic"}) {
          auto* button = window.findChild<QToolButton*>(QLatin1String(name));
          QVERIFY2(button && button->isVisible(),
                   qPrintable(QStringLiteral("%1 폭에서 %2가 리본에 없다").arg(width).arg(QLatin1String(name))));
        }
      }
      const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
      if (!output.isEmpty() && width == 1280) {
        QVERIFY(QDir().mkpath(output));
        QVERIFY(window.grab().save(QDir(output).filePath(QStringLiteral("ribbon-1280.png"))));
      }
      QgsProject::instance()->setDirty(false);
    }
    const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    if (!output.isEmpty()) {
      MainWindow maximized;
      disableRendering(maximized);
      maximized.showMaximized();
      QTest::qWait(150);  // the ribbon picks its size and the toolbar its height over a few layout rounds
      QVERIFY(QDir().mkpath(output));
      QVERIFY(maximized.grab().save(QDir(output).filePath(QStringLiteral("ribbon-maximized.png"))));
      QgsProject::instance()->setDirty(false);
    }
  }
  // 그리기 보조 줄(#subToolbar)도 리본처럼 접지 않는다: » 펼침 단추가 나오지 않는다. 1904 창에서 그리기를 시작하면
  // 단추 글자가 보이고, 줄이 모자라면 글자를 숨긴 뒤 아이콘을 20에서 16으로 줄인다. 한 점을 찍으면 완료·되돌리기·취소가
  // 줄에 더해져 길어지므로(1904 창에서는 글자가 안 들어간다) 줄은 20 px 아이콘만 남긴다. 이 모양은 받아들인 것이고(도구
  // 여섯 개의 아이콘과 자석 설정을 줄이는 일은 뒤 과제), 그때까지 20 px가 16 px로 줄거나 »가 돌아오면 안 된다.
  void subToolbarFitsAt1904WhileSketching() {
    const QString path = makeSurvey(QStringLiteral("그리기줄글자"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    auto* canvas = window.findChild<QgsMapCanvas*>();
    auto* sub = window.findChild<QToolBar*>(QStringLiteral("subToolbar"));
    auto* draw = window.findChild<QToolButton*>(QStringLiteral("btnDraw"));
    QVERIFY(canvas && sub && draw);
    window.resize(1904, 1000);
    window.show();
    QApplication::setActiveWindow(&window);
    QTRY_VERIFY(draw->isEnabled());
    draw->click();
    QTRY_VERIFY(sub->isVisible());
    QVERIFY(QMetaObject::invokeMethod(&window, "startEditFeaturePoly", Qt::DirectConnection));

    const auto extShown = [&] {
      auto* ext = sub->findChild<QToolButton*>(QStringLiteral("qt_toolbar_ext_button"));
      return ext && ext->isVisible();
    };
    QStringList hidden;
    const auto allShown = [&] {  // every action that is on the row has a visible button
      hidden.clear();
      for (QAction* action : sub->actions()) {
        QWidget* button = sub->widgetForAction(action);
        if (action->isVisible() && !action->isSeparator() && (!button || !button->isVisible())) hidden << action->text();
      }
      return hidden.isEmpty();
    };
    const auto where = [&] {
      return QStringLiteral("창 %1 · 줄 %2 · 필요 %3 · 글자 %4 · 아이콘 %5 · 펼침 %6 · 안 보임 [%7]")
          .arg(window.width()).arg(sub->width()).arg(sub->sizeHint().width())
          .arg(sub->toolButtonStyle()).arg(sub->iconSize().width()).arg(extShown())
          .arg(hidden.join(QLatin1Char(' ')));
    };
    // The tools are on the row, nothing is being drawn yet: labels beside the 20 px icons, nothing folded.
    QTRY_VERIFY2(allShown() && !extShown(), qPrintable(where()));
    QVERIFY2(sub->toolButtonStyle() != Qt::ToolButtonIconOnly && sub->iconSize() == QSize(20, 20), qPrintable(where()));
    qInfo().noquote() << "sub toolbar at 1904, nothing drawn yet:" << where();

    // One vertex placed: 완료·되돌리기·취소 join the row.
    canvas->setExtent(QgsRectangle(189950, 559950, 190150, 560150));
    QApplication::processEvents();
    const QgsPointXY pixel = canvas->getCoordinateTransform()->transform(QgsPointXY(190050, 560050));
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(qRound(pixel.x()), qRound(pixel.y())));
    auto* capture = window.findChild<KaCaptureMapTool*>();
    QVERIFY(capture);
    QTRY_VERIFY2(capture->hasSketch(), "fixture: the click placed a vertex");
    QAction* finish = nullptr;
    for (QAction* action : sub->actions())
      if (action->property("kaSketch").toString() == QLatin1String("finish")) finish = action;
    QVERIFY2(finish && finish->isVisible(), "fixture: 완료 shows while a shape is being drawn");
    QTRY_VERIFY2(allShown() && !extShown(), qPrintable(where()));
    QVERIFY2(sub->toolButtonStyle() == Qt::ToolButtonIconOnly && sub->iconSize() == QSize(20, 20), qPrintable(where()));
    qInfo().noquote() << "sub toolbar at 1904, one vertex placed:" << where();

    // What each stage needs on this row (the bar's own sizes, measured by trying them; the fit follows).
    const auto need = [&](Qt::ToolButtonStyle style, int icon) {
      sub->setToolButtonStyle(style);
      sub->setIconSize(QSize(icon, icon));
      sub->layout()->invalidate();
      return sub->sizeHint().width();
    };
    const int needIcons = need(Qt::ToolButtonIconOnly, 20);
    const int needSmall = need(Qt::ToolButtonIconOnly, 16);
    const int needText = need(Qt::ToolButtonTextBesideIcon, 20);
    QVERIFY2(needText > needIcons + 100 && needIcons > needSmall + 10, qPrintable(QStringLiteral("%1 %2 %3").arg(needText).arg(needIcons).arg(needSmall)));
    window.resize(needIcons + 25, 1000);  // too narrow for the labels, room for 20 px icons
    QTRY_VERIFY2(allShown() && !extShown(), qPrintable(where()));
    QVERIFY2(sub->toolButtonStyle() == Qt::ToolButtonIconOnly && sub->iconSize() == QSize(20, 20), qPrintable(where()));
    window.resize(needSmall + 5, 1000);  // not even room for those: the icons shrink
    QTRY_VERIFY2(allShown() && !extShown(), qPrintable(where()));
    QVERIFY2(sub->toolButtonStyle() == Qt::ToolButtonIconOnly && sub->iconSize() == QSize(16, 16), qPrintable(where()));
    qInfo().noquote() << "sub toolbar at the smallest stage:" << where();
    window.resize(1904, 1000);  // room again: back to what 1904 showed, icons only at 20 px
    QTRY_VERIFY2(allShown() && !extShown() && sub->iconSize() == QSize(20, 20), qPrintable(where()));
    QVERIFY2(sub->toolButtonStyle() == Qt::ToolButtonIconOnly, qPrintable(where()));

    // 1024 (the window itself stops at about 1082): the row has text-only tools and the snap settings, about
    // 1360 px even as bare icons, so the smallest stage is as far as it can go there and the » may show.
    window.resize(1024, 768);
    QTRY_VERIFY2(sub->toolButtonStyle() == Qt::ToolButtonIconOnly && sub->iconSize() == QSize(16, 16), qPrintable(where()));
    qInfo().noquote() << "sub toolbar at 1024:" << where();
    QgsProject::instance()->setDirty(false);
  }
  // 저장 안 됨(스펙 「아이콘 체계」 표): 「저장」 칩은 파란 타일·주황 점에 더해 라벨이 굵어지고,
  // 저장하면 보통 굵기로 돌아온다. 굵기는 창이 칩에 붙이는 `unsaved` 속성을 QSS 가 읽어 정한다.
  void unsavedSaveChipLabelTurnsBoldAndBack() {
    MainWindow window;
    disableRendering(window);
    window.resize(1904, 1000);  // the labels show from about 1500 px up; narrower windows keep only the icons
    window.show();
    const QString path = makeSurvey(QStringLiteral("굵은저장"));
    QVERIFY(!path.isEmpty());
    QVERIFY(openSettled(window, path));
    QgsProject::instance()->setDirty(false);
    QTest::qWait(450);  // the 300 ms chip merge (shellSyncTimer) settles on the clean state
    auto* save = window.findChild<QToolButton*>(QStringLiteral("ribbonSave"));
    QVERIFY(save && save->isVisible());
    QVERIFY(!save->property("unsaved").toBool());
    const RibbonLabelInk::Measure plain = RibbonLabelInk::measure(save);
    QVERIFY2(plain.pixels > 0, "the 「저장」 label has ink");

    QgsProject::instance()->setDirty(true);
    QTRY_VERIFY(save->property("unsaved").toBool());
    QApplication::processEvents();
    const RibbonLabelInk::Measure bold = RibbonLabelInk::measure(save);
    qInfo() << "저장 label ink" << plain.pixels << "->" << bold.pixels << "columns" << bold.left << ".." << bold.right
            << "in a" << save->width() << "px chip";
    QVERIFY2(bold.pixels > plain.pixels * 1.15,
             qPrintable(QStringLiteral("unsaved label ink %1 vs saved %2").arg(bold.pixels).arg(plain.pixels)));
    // The chip must not clip the bold label: ink stays inside its 2 px of border and padding.
    QVERIFY2(bold.left >= 2 && bold.right <= save->width() - 3,
             qPrintable(QStringLiteral("bold label spans %1..%2 in a %3 px chip").arg(bold.left).arg(bold.right).arg(save->width())));

    QVERIFY(saveNow(window));
    QTRY_VERIFY(!save->property("unsaved").toBool());
    QApplication::processEvents();
    QVERIFY2(RibbonLabelInk::measure(save).pixels <= plain.pixels * 1.05, "saved again: the label is regular");
    QgsProject::instance()->setDirty(false);
  }
  void recordToolsWaitForASurvey() {
    MainWindow window;
    disableRendering(window);
    window.show();
    QCoreApplication::processEvents();
    auto* draw = window.findChild<QToolButton*>(QStringLiteral("btnDraw"));
    auto* select = window.findChild<QToolButton*>(QStringLiteral("btnSelect"));
    QVERIFY(draw);
    if (!select) {
      for (auto* button : window.findChildren<QToolButton*>()) {
        if (button->defaultAction() && button->text() == QStringLiteral("선택")) {
          select = button;
          break;
        }
      }
    }
    QVERIFY(select);
    QVERIFY2(!draw->isEnabled() && !select->isEnabled(), "홈에서는 기록 단추가 흐려야 한다");
    const QString path = makeSurvey(QStringLiteral("기록단추"));
    QVERIFY(!path.isEmpty());
    QVERIFY(window.openSurveyGpkg(path));
    QTRY_VERIFY(draw->isEnabled());
    QVERIFY(select->isEnabled());
    QgsProject::instance()->setDirty(false);
  }

  // 리본 단추는 그림만 보고도 찾을 수 있게 모두 다른 그림이어야 한다.
  // 예전에는 대동여지·웹, 버퍼·유산, 지형·수치·1919지형, 저장·다른이름 그림이 같았다.
  void ribbonButtonsAllHaveDifferentIcons() {
    MainWindow window;
    disableRendering(window);
    auto* ribbon = window.findChild<KaBeginnerRibbon*>(QStringLiteral("beginnerRibbon"));
    QVERIFY(ribbon);
    QHash<QByteArray, QString> seen;
    int compared = 0;
    const auto buttons = ribbon->findChildren<QToolButton*>();
    for (QToolButton* button : buttons) {
      if (button->icon().isNull()) continue;  // chips keep their icon whether the labels show or not
      const QImage image =
          button->icon().pixmap(QSize(64, 64), 1.0).toImage().convertToFormat(QImage::Format_ARGB32);
      const QByteArray key = QCryptographicHash::hash(
          QByteArray(reinterpret_cast<const char*>(image.constBits()), int(image.sizeInBytes())),
          QCryptographicHash::Sha1);
      QVERIFY2(!seen.contains(key),
               qPrintable(QStringLiteral("「%1」과 「%2」 그림이 같습니다").arg(seen.value(key), button->text())));
      seen.insert(key, button->text());
      ++compared;
    }
    QVERIFY2(compared >= 25, qPrintable(QStringLiteral("리본 단추 %1개만 보았습니다").arg(compared)));
    const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    if (!output.isEmpty() && QDir(output).exists()) {
      window.setAttribute(Qt::WA_DontShowOnScreen);
      window.resize(2400, 900);
      window.show();
      QCoreApplication::processEvents();
      auto* toolbar = window.findChild<QToolBar*>(QStringLiteral("mainToolbar"));
      QVERIFY(toolbar);
      QVERIFY(toolbar->grab().save(QDir(output).filePath(QStringLiteral("main-ribbon-render.png"))));
    }
  }

  void mapControlsZoomTheCanvasAndShowAScaleBar() {
    MainWindow window;
    disableRendering(window);
    window.resize(1400, 900);
    window.show();
    QCoreApplication::processEvents();
    auto* canvas = window.findChild<QgsMapCanvas*>(QStringLiteral("mapCanvas"));
    auto* zoomIn = window.findChild<QToolButton*>(QStringLiteral("mapZoomIn"));
    auto* zoomOut = window.findChild<QToolButton*>(QStringLiteral("mapZoomOut"));
    QVERIFY(canvas && zoomIn && zoomOut);
    QVERIFY(window.findChild<QToolButton*>(QStringLiteral("mapZoomFit")));
    QVERIFY(window.findChild<QWidget*>(QStringLiteral("mapScaleBar")));
    // Place search sits at the right end of the ribbon row, and the maps are ribbon buttons.
    auto* appBar = window.findChild<QWidget*>(QStringLiteral("appBar"));
    QVERIFY(appBar && appBar->parentWidget() == window.findChild<QToolBar*>(QStringLiteral("mainToolbar")));
    QVERIFY(!window.findChild<QToolBar*>(QStringLiteral("appBarToolbar")));
    QVERIFY(window.findChild<QToolButton*>(QStringLiteral("btnTerrain")));
    // The canvas has no size while the home tab is showing: open the map tab.
    auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("viewTabs"));
    QVERIFY(tabs);
    for (int i = 0; i < tabs->count(); ++i)
      if (tabs->widget(i)->findChild<QgsMapCanvas*>(QStringLiteral("mapCanvas")) == canvas)
        tabs->setCurrentIndex(i);
    QTRY_VERIFY(canvas->isVisible() && canvas->width() > 100);
    canvas->setExtent(QgsRectangle(200000, 400000, 202000, 401500));
    QTRY_VERIFY(canvas->scale() > 0);
    const double before = canvas->scale();
    zoomIn->click();
    QTRY_VERIFY_WITH_TIMEOUT(canvas->scale() < before, 3000);
    const double closer = canvas->scale();
    zoomOut->click();
    QTRY_VERIFY_WITH_TIMEOUT(canvas->scale() > closer, 3000);
    // 「조용한 도판」: the scale bar ends with a 도북 arrow; the stock bar has none.
    auto* scaleBar = window.findChild<QWidget*>(QStringLiteral("mapScaleBar"));
    QVERIFY(!scaleBar->accessibleDescription().contains(QStringLiteral("도북")));
    KaTheme::DisplayOptions paper; paper.paperLook = true; KaTheme::setDisplayOptions(qApp, paper);
    QCoreApplication::processEvents();  // switching the look alone (no map move) brings the arrow
    const QString described = scaleBar->accessibleDescription();
    KaTheme::setDisplayOptions(qApp, KaTheme::DisplayOptions());
    QCoreApplication::processEvents();
    QVERIFY2(described.contains(QStringLiteral("도북")), qPrintable(described));
    QVERIFY(!scaleBar->accessibleDescription().contains(QStringLiteral("도북")));
    QgsProject::instance()->setDirty(false);
  }
  void topographicActualWindowRemainsStableAfterCompletion() {
    const QDir cache(qEnvironmentVariable("KA_HGIS_QA_TOPOGRAPHIC_SHP"));
    if(qEnvironmentVariableIsEmpty("KA_HGIS_QA_TOPOGRAPHIC_SHP")) QSKIP("Opt-in real SHP rendering/idle diagnostic");
    QList<TopographicCatalog::Record> records;
    for(const auto& index:cache.entryInfoList({QStringLiteral("*.json")},QDir::Files)) {
      QFile file(index.absoluteFilePath()); QVERIFY(file.open(QIODevice::ReadOnly));
      const auto json=QJsonDocument::fromJson(file.readAll()).object();
      const QDir data(cache.filePath(json.value("directory").toString()));
      for(const auto& entry:json.value("records").toArray()) {
        const auto object=entry.toObject(); const auto bounds=object.value("bounds").toArray();
        QCOMPARE(bounds.size(),4);
        TopographicCatalog::Record r;
        r.source=data.filePath(object.value("file").toString()); r.layerName=object.value("layer").toString();
        r.sourceSheet=object.value("sheet").toString(); r.displayName=object.value("name").toString();
        r.crsWkt=object.value("crs").toString(); r.category=static_cast<TopographicCatalog::Category>(object.value("category").toInt());
        r.extent=QgsRectangle(bounds[0].toDouble(),bounds[1].toDouble(),bounds[2].toDouble(),bounds[3].toDouble());
        r.hasExtent=true; r.signature=index.baseName(); records.append(r);
      }
    }
    QVERIFY(!records.isEmpty());
    MainWindow window;
    QString path=makeSurvey(QStringLiteral("topographic-stability")); QVERIFY(!path.isEmpty());
    const auto surveySource=qEnvironmentVariable("KA_HGIS_QA_TOPOGRAPHIC_SURVEY");
    QByteArray originalSurvey;
    if(!surveySource.isEmpty()) {
      originalSurvey=contents(surveySource); QVERIFY(!originalSurvey.isEmpty());
      path=m_files.filePath(QStringLiteral("field-survey-copy.gpkg"));
      QVERIFY(QFile::copy(surveySource,path));
    }
    QVERIFY(window.openSurveyGpkg(path,MainWindow::OpenSurveyMode::LayersOnly));
    window.resize(1500,950); window.show();
    auto* canvas=window.findChild<QgsMapCanvas*>(); QVERIFY(canvas);
    QTest::qWait(1000); // Let initial window geometry and project-open signals settle.
    connect(canvas,&QgsMapCanvas::extentsChanged,&window,[canvas] {
      qInfo()<<"extent"<<canvas->extent().toString()<<"crs"<<canvas->mapSettings().destinationCrs().authid()<<"scale"<<canvas->scale();
    });
    const auto crs=canvas->mapSettings().destinationCrs();
    QgsRectangle bounds;
    for(const auto& record:records) bounds.combineExtentWith(QgsCoordinateTransform(QgsCoordinateReferenceSystem(record.crsWkt),crs,QgsProject::instance()).transformBoundingBox(record.extent));
    qInfo()<<"requested bounds"<<bounds.toString()<<"records"<<records.size();
    if(surveySource.isEmpty()) canvas->setExtent(bounds);
    else {
      bool framed=false;
      for(auto* raw:QgsProject::instance()->mapLayers()) {
        if(raw->customProperty(QStringLiteral("ka_hgis/layer_key")).toString()!=QLatin1String("survey_area")) continue;
        auto* survey=qobject_cast<QgsVectorLayer*>(raw); QVERIFY(survey);
        QgsFeature feature;
        auto features=survey->getFeatures(); QVERIFY(features.nextFeature(feature));
        auto area=QgsCoordinateTransform(survey->crs(),crs,QgsProject::instance()).transformBoundingBox(feature.geometry().boundingBox());
        QVERIFY(!area.isEmpty()); area.scale(1.3); canvas->setExtent(area); framed=true; break;
      }
      QVERIFY(framed);
    }
    canvas->refresh();
    const auto initialExtent=canvas->extent();
    auto topographicCount=[] {
      int n=0;
      for (auto* layer:QgsProject::instance()->mapLayers())
        if (!layer->customProperty(QStringLiteral("ka_hgis/topographic_source")).toString().isEmpty()) ++n;
      return n;
    };
    QStringList preservedIds;
    for (auto* layer:QgsProject::instance()->mapLayers())
      if (layer->customProperty(QStringLiteral("ka_hgis/topographic_source")).toString().isEmpty())
        preservedIds.append(layer->id());
    QVERIFY(!preservedIds.isEmpty());
    const auto expected=TopographicCatalog::query(records,TopographicCatalog::coverageBounds(initialExtent),crs.toWkt(),QgsProject::instance()->transformContext(),
        nullptr,{},canvas->scale());
    QVERIFY(!expected.matches.isEmpty());
    KaTopographicImportDialog importer(canvas,&window);
    QTemporaryDir library;
    KaTopographicScopePanel scope(canvas,nullptr,&importer,&window,library.path()); scope.hide();
    QSignalSpy added(QgsProject::instance(),&QgsProject::layersAdded);
    QSignalSpy removed(QgsProject::instance(),&QgsProject::layersRemoved);
    QSignalSpy rendered(canvas,&QgsMapCanvas::mapCanvasRefreshed);
    QSignalSpy canceled(canvas,&QgsMapCanvas::mapRefreshCanceled);
    QElapsedTimer elapsed; elapsed.start();
    QString error; QVERIFY2(importer.importVerified(records,&error),qPrintable(error));
    QTRY_VERIFY_WITH_TIMEOUT(!importer.isAutomaticLoading(),120000);
    QTRY_VERIFY_WITH_TIMEOUT(!canvas->isDrawing() && !rendered.isEmpty(),120000);
    QSet<int> expectedGroups;
    for (const auto& record : expected.matches)
      expectedGroups.insert(static_cast<int>(record.category));
    QCOMPARE(topographicCount(),expectedGroups.size());
    QCOMPARE(QgsProject::instance()->mapLayers().size(),preservedIds.size()+expectedGroups.size());
    QCOMPARE(canvas->extent(),initialExtent);
    QVERIFY(added.size()<=1);
    qInfo()<<"initial load milliseconds"<<elapsed.elapsed()<<"adds"<<added.size()<<"renders"<<rendered.size()<<"cancels"<<canceled.size()
        <<"scale"<<canvas->scale()<<"topo"<<expected.matches.size()<<"preserved"<<preservedIds.size();
    const auto ids=QgsProject::instance()->mapLayers().keys();
    QMap<QString,bool> checks;
    for(auto* node:QgsProject::instance()->layerTreeRoot()->findLayers()) checks.insert(node->layerId(),node->itemVisibilityChecked());
    const int priorRenders=rendered.size(), priorAdds=added.size(), priorRemoves=removed.size();
    // Leave the real scope watcher and MainWindow's 30 s health timer active.
    QTest::qWait(35000);
    QCOMPARE(QgsProject::instance()->mapLayers().keys(),ids);
    for(auto* node:QgsProject::instance()->layerTreeRoot()->findLayers()) QCOMPARE(node->itemVisibilityChecked(),checks.value(node->layerId()));
    QCOMPARE(added.size(),priorAdds); QCOMPARE(removed.size(),priorRemoves);
    QVERIFY2(rendered.size()-priorRenders<=2,"Idle must not keep repainting/reloading");
    const auto output=qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    if(!output.isEmpty()) { QVERIFY(QDir().mkpath(output)); QVERIFY(window.grab().save(QDir(output).filePath(QStringLiteral("actual-window-idle.png")))); }
    qInfo()<<"idle renders"<<rendered.size()-priorRenders<<"layer count"<<ids.size()<<"scope active"<<scope.isActive();
    if(!surveySource.isEmpty()) {
      int visit=0;
      for(const auto& view:{bounds,initialExtent}) {
        const auto before=rendered.size();
        elapsed.restart(); canvas->setExtent(view); canvas->refresh();
        // Wait until the extent debounce has processed this view and the final
        // map render finishes; the watcher remains enabled throughout.
        QTest::qWait(600);
        QTRY_VERIFY_WITH_TIMEOUT(!importer.isAutomaticLoading() && !canvas->isDrawing() && rendered.size()>before,120000);
        const auto visible=TopographicCatalog::query(records,TopographicCatalog::coverageBounds(canvas->extent()),crs.toWkt(),QgsProject::instance()->transformContext(),
            nullptr,{},canvas->scale());
        QSet<int> visibleGroups;
        for (const auto& record : visible.matches)
          visibleGroups.insert(static_cast<int>(record.category));
        QCOMPARE(topographicCount(),visibleGroups.size());
        QCOMPARE(QgsProject::instance()->mapLayers().size(),preservedIds.size()+visibleGroups.size());
        for(const auto& id:preservedIds) {
          QVERIFY(QgsProject::instance()->mapLayer(id));
          auto* node=QgsProject::instance()->layerTreeRoot()->findLayer(id); QVERIFY(node);
          QCOMPARE(node->itemVisibilityChecked(),checks.value(id));
        }
        const auto picture=canvas->grab().toImage();
        int mapPixels=0;
        for(int y=100;y<picture.height()-40;++y) for(int x=100;x<picture.width()-40;++x) {
          const auto c=picture.pixelColor(x,y);
          if(c.red()>=120 && c.red()<200 && c.red()==c.green() && c.green()==c.blue()) ++mapPixels;
        }
        QVERIFY2(mapPixels>100,"Reference geometry must remain visible after navigation");
        qInfo()<<"navigation"<<visit<<"ms"<<elapsed.elapsed()<<"layers"<<visible.matches.size()<<"gray pixels"<<mapPixels;
        if(!output.isEmpty()) QVERIFY(window.grab().save(QDir(output).filePath(QStringLiteral("actual-window-navigation-%1.png").arg(visit))));
        ++visit;
      }
      QCOMPARE(contents(surveySource),originalSurvey);
    }
    QgsProject::instance()->setDirty(false);
  }
  void topographicDownloadKeepsMapVisibleAndUsesSurveyFolder() {
    MainWindow window;
    disableRendering(window);
    const QString path=makeSurvey(QStringLiteral("browser-key-isolation"));
    QVERIFY(!path.isEmpty());
    QVERIFY(window.openSurveyGpkg(path,MainWindow::OpenSurveyMode::LayersOnly));
    const QString directory=QFileInfo(path).absoluteDir().filePath(QStringLiteral("지형도"));
    QVERIFY(QFileInfo(directory).isDir());
    window.show();
    auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("viewTabs"));
    QVERIFY(tabs);
    QWidget* map=tabs->currentWidget();
    QVERIFY(!window.findChild<KaTopographicBrowser*>());
    auto* downloadButton=window.findChild<QToolButton*>(QStringLiteral("btnTopographic"));
    QVERIFY(downloadButton);
    downloadButton->click();
    auto* browser = window.findChild<KaTopographicBrowser*>();
    QVERIFY(browser);
    browser->stopAutomatic();
    browser->navigate(QUrl(QStringLiteral("about:blank")));
    QCOMPARE(browser->parentWidget(),&window);
    QCOMPARE(browser->windowModality(),Qt::NonModal);
    QCOMPARE(browser->windowType(),Qt::Dialog);
    QVERIFY(browser->property("kaDownloadWindow").toBool());
    auto* title=browser->findChild<QLabel*>(QStringLiteral("downloadTitle"));
    auto* stage=browser->findChild<QLabel*>(QStringLiteral("topographicCurrentStage"));
    auto* trail=browser->findChild<QLabel*>(QStringLiteral("topographicStageTrail"));
    QVERIFY(title && title->isVisible());
    QVERIFY(stage && stage->isVisible());
    QVERIFY(trail && !trail->isVisible());
    QCOMPARE(tabs->currentWidget(),map);
    QCOMPARE(tabs->indexOf(browser),-1);
    auto* details=browser->findChild<QWidget*>(QStringLiteral("topographicOfficialDetails"));
    QVERIFY(details && details->isHidden());
    const auto downloadQa=qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    if(!downloadQa.isEmpty()) {
      QVERIFY(QDir().mkpath(downloadQa));
      QVERIFY(browser->grab().save(QDir(downloadQa).filePath(QStringLiteral("topographic-from-ribbon.png"))));
    }
    browser->hide();
    QVERIFY(QMetaObject::invokeMethod(&window, "openTopographicDownload", Qt::DirectConnection));
    browser->stopAutomatic();
    browser->navigate(QUrl(QStringLiteral("about:blank")));
    QCOMPARE(window.findChild<KaTopographicBrowser*>(), browser);
    QCOMPARE(tabs->currentWidget(),map);
    QVERIFY(details->isHidden());
    // A main-window shortcut must not edit the survey while its download window is active.
    QString error;
    auto* survey=LayerOps::ensureDomainLayer(QgsProject::instance(),path,QStringLiteral("survey_area"),QStringLiteral("조사구역"),&error);
    QVERIFY(survey && survey->isValid());
    survey->selectAll();
    auto* canvas=window.findChild<QgsMapCanvas*>(); QVERIFY(canvas);
    canvas->setCurrentLayer(survey);
    const auto before=survey->featureCount();
    QVERIFY(before>0);
    QApplication::setActiveWindow(browser);
    QCOMPARE(QApplication::activeWindow(),browser);
    for(auto* action:window.actions())
      if(action->shortcut()==QKeySequence(QKeySequence::Delete)) action->trigger();
    QCOMPARE(survey->featureCount(),before);
    browser->hide();
    QgsProject::instance()->setDirty(false);
    const QString secondSource=makeSurvey(QStringLiteral("second-survey-directory"));
    QVERIFY(!secondSource.isEmpty());
    const QString secondFolder=m_files.filePath(QStringLiteral("another-folder"));
    QVERIFY(QDir().mkpath(secondFolder));
    const QString other=QDir(secondFolder).filePath(QStringLiteral("another.gpkg"));
    QVERIFY(QFile::copy(secondSource,other));
    QVERIFY(window.openSurveyGpkg(other,MainWindow::OpenSurveyMode::LayersOnly));
    QVERIFY(QFileInfo(QFileInfo(other).absoluteDir().filePath(QStringLiteral("지형도"))).isDir());
    QVERIFY(QFileInfo(directory).isDir());
    QgsProject::instance()->setDirty(false);
  }
  void cleanup() { QgsProject::instance()->clear(); }
  void provinceChipMovesMapWithoutLoadingLayers_data() {
    QTest::addColumn<QString>("crs");
    for (const QString& crs : {QStringLiteral("EPSG:5186"), QStringLiteral("EPSG:5187")})
      QTest::newRow(qPrintable(crs)) << crs;
  }
  void provinceChipMovesMapWithoutLoadingLayers() {
    QFETCH(QString, crs);
    MainWindow window;
    disableRendering(window);
    window.resize(1800, 1000);
    window.show();
    auto* canvas = window.findChild<QgsMapCanvas*>();
    auto* locator = window.findChild<KaRegionLocator*>();
    QVERIFY(canvas && locator);
    QVERIFY(QMetaObject::invokeMethod(&window, crs.endsWith('6') ? "setWorkCrs5186" : "setWorkCrs5187", Qt::DirectConnection));
    const auto layerIds = QgsProject::instance()->mapLayers().keys();
    const QgsCoordinateTransform transform(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4326")),
                                           QgsCoordinateReferenceSystem(crs), QgsProject::instance());
    struct Visit { const char* chip; double lon; double lat; };
    for (const Visit& visit : {Visit{"서울", 126.978, 37.5665}, Visit{"울산", 129.3114, 35.5396},
                              Visit{"제주", 126.531, 33.4996}}) {
      QToolButton* chip = nullptr;
      for (auto* button : locator->findChildren<QToolButton*>())
        if (button->text() == QString::fromUtf8(visit.chip)) chip = button;
      QVERIFY(chip);
      chip->click();
      QCoreApplication::processEvents();
      QCOMPARE(canvas->mapSettings().destinationCrs().authid(), crs);
      QCOMPARE(QgsProject::instance()->crs().authid(), crs);
      const QgsPointXY expected = transform.transform(QgsPointXY(visit.lon, visit.lat));
      QVERIFY2(canvas->extent().contains(expected), qPrintable(QStringLiteral("%1 is outside %2").arg(chip->text(), canvas->extent().toString())));
      QVERIFY(canvas->extent().width() > 10000.);
      QCOMPARE(QgsProject::instance()->mapLayers().keys(), layerIds);
      QVERIFY(!locator->findChild<QPushButton*>(QStringLiteral("regionFieldMap")));
    }
    QgsProject::instance()->setDirty(false);
  }
  // 범례는 용지 밖에도 놓을 수 있고 놓은 자리가 그대로 남는다 (현장 요청 2026-10-11, 보완 L4를 되돌림): 마우스로 끌어 용지 오른쪽
  // 밖으로 놓은 뒤에도, 도면을 다시 이어 붙이고 지도를 새로 고친 뒤에도 안쪽으로 밀려 오지 않는다.
  void drawingStudio_legendStaysWhereTheUserPutsItOutsideThePaper() {
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    QgsMapCanvas canvas;
    canvas.setRenderFlag(false);
    canvas.setDestinationCrs(project.crs());
    canvas.setExtent(QgsRectangle(190000, 560000, 191000, 561000));
    auto studioPtr = std::make_unique<KaDrawingStudio>(&project, &canvas, 420., 297.);
    KaDrawingStudio& studio = *studioPtr;
    studio.setAttribute(Qt::WA_DontShowOnScreen);
    studio.show();
    QCoreApplication::processEvents();
    auto* view = studio.findChild<QgsLayoutView*>();
    QVERIFY(view && view->currentLayout());
    auto* ly = view->currentLayout();
    QVERIFY(QMetaObject::invokeMethod(&studio, "beginPlaceLegend"));
    auto* legend = dynamic_cast<QgsLayoutItemLegend*>(ly->itemById(QStringLiteral("ka_legend")));
    QVERIFY(legend);
    QVERIFY(QMetaObject::invokeMethod(&studio, "useSelectTool"));
    auto* selectTool = studio.findChild<QgsLayoutViewToolSelect*>();
    QVERIFY(selectTool);
    // an A3 sheet (not the old A4 one, whose first opening lays everything out again): start at the right edge with the centre
    // still on the paper, then drag the legend 30 mm further out
    const QSizeF size = legend->rect().size();
    legend->attemptSetSceneRect(QRectF(420. - size.width() / 2. - 6., 60., size.width(), size.height()));
    view->zoomFull();
    view->centerOn(legend);
    ly->setSelectedItem(legend);
    QCoreApplication::processEvents();
    QTRY_VERIFY2(selectTool->mouseHandles() &&
                     QLineF(selectTool->mouseHandles()->sceneBoundingRect().center(), legend->sceneBoundingRect().center()).length() < 0.5,
                 "the selection handles did not settle on the legend");
    QTest::qWait(120);
    const QRectF before(legend->pos(), legend->rect().size());
    const QPoint from = view->mapFromScene(legend->mapToScene(legend->rect().center()));
    const double viewScale = std::max(1., view->transform().m11());
    const QPoint to = from + QPoint(static_cast<int>(std::ceil(30. * viewScale)), 0);
    QMouseEvent hover(QEvent::MouseMove, QPointF(from), view->viewport()->mapToGlobal(from), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &hover);
    QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, from);
    QMouseEvent drag(QEvent::MouseMove, QPointF(to), view->viewport()->mapToGlobal(to), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &drag);
    QApplication::sendEvent(view->viewport(), &drag);
    QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, to);
    QVERIFY2(legend->pos().x() > before.x() + 10., "the legend did not drag");
    QTest::qWait(150);  // the settle step that runs after the release
    const QRectF dropped(legend->pos(), legend->rect().size());
    QVERIFY2(dropped.right() > 420. + 10. && !QRectF(0., 0., 420., 297.).contains(dropped.center()),
             qPrintable(QStringLiteral("the legend was pushed back onto the paper: %1,%2").arg(dropped.x()).arg(dropped.y())));
    // refreshing the map leaves the place alone, and so does opening the drawing again (the saved sheet's decorations are reconnected)
    studio.refreshMapFromProject();
    QTest::qWait(450);
    QCOMPARE(QRectF(legend->pos(), legend->rect().size()), dropped);
    studioPtr.reset();
    KaDrawingStudio reopened(&project, &canvas, 420., 297.);
    reopened.setAttribute(Qt::WA_DontShowOnScreen);
    reopened.show();
    QCoreApplication::processEvents();
    QTest::qWait(450);
    auto* again = reopened.findChild<QgsLayoutView*>();
    QVERIFY(again && again->currentLayout());
    auto* legendAgain = dynamic_cast<QgsLayoutItemLegend*>(again->currentLayout()->itemById(QStringLiteral("ka_legend")));
    QVERIFY(legendAgain);
    QCOMPARE(QRectF(legendAgain->pos(), legendAgain->rect().size()), dropped);
  }

  void drawingStudio_decorationsMoveAndShrink() {
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    QgsMapCanvas canvas;
    canvas.setRenderFlag(false);
    canvas.setDestinationCrs(project.crs());
    canvas.setExtent(QgsRectangle(190000, 560000, 191000, 561000));
    KaDrawingStudio studio(&project, &canvas, 297., 210.);
    studio.setAttribute(Qt::WA_DontShowOnScreen);
    studio.show();
    QCoreApplication::processEvents();
    auto* view = studio.findChild<QgsLayoutView*>();
    QVERIFY(view && view->currentLayout());
    auto* ly = view->currentLayout();
    auto* map = dynamic_cast<QgsLayoutItemMap*>(ly->itemById(QStringLiteral("ka_map")));
    QVERIFY(map);
    if (auto* north = dynamic_cast<QgsLayoutItemPicture*>(ly->itemById(QStringLiteral("ka_north"))))
      QVERIFY2(!north->isMissingImage(), qPrintable(north->evaluatedPath()));
    auto* size = studio.findChild<QDoubleSpinBox*>(QStringLiteral("decorationSize"));
    QVERIFY(QMetaObject::invokeMethod(&studio, "useSelectTool"));
    auto* selectTool = studio.findChild<QgsLayoutViewToolSelect*>();
    QVERIFY(selectTool);
    QMap<QString, QPointF> positions;
    for (const QString& id : {QStringLiteral("ka_scalebar"), QStringLiteral("ka_scale"),
                              QStringLiteral("ka_crs"), QStringLiteral("ka_north")}) {
      auto* item = ly->itemById(id);
      QVERIFY2(item, qPrintable(id));
      QVERIFY2(!item->isLocked(), qPrintable(id + QStringLiteral(" is locked")));
      QVERIFY(size);
      view->centerOn(item);
      ly->setSelectedItem(item);
      QCoreApplication::processEvents();
      // 선택 핸들이 새 항목으로 옮겨오기 전에 누르면 이전 항목의 회전 핸들을 잡아,
      // 항목이 움직이는 대신 회전한다. 핸들이 자리를 잡을 때까지 기다린다.
      QTRY_VERIFY2(selectTool->mouseHandles() &&
                       QLineF(selectTool->mouseHandles()->sceneBoundingRect().center(),
                              item->sceneBoundingRect().center()).length() < 0.5,
                   qPrintable(id + QStringLiteral(" 선택 핸들이 자리를 잡지 못했다")));
      // Paper-fit timers (0 ms and 80 ms) reset the view. Wait them out, then zoom.
      // QGIS rotation zones are 3× the resize border and meet at the center of a 20 mm compass.
      QTest::qWait(120);
      const double side = std::min(item->rect().width(), item->rect().height());
      view->setZoomLevel(side < 30. ? 12. : 2.);
      view->centerOn(item);
      QVERIFY(size->isEnabled());
      const QRectF before(item->pos(), item->rect().size());
      const double rotationBefore = item->itemRotation();
      const QPoint from = view->mapFromScene(item->mapToScene(item->rect().center()));
      const double viewScale = std::max(1., view->transform().m11());
      const int dragPx = static_cast<int>(std::ceil(8. * viewScale));
      const QPoint to = from + QPoint(dragPx, -dragPx);
      QMouseEvent hover(QEvent::MouseMove, QPointF(from), view->viewport()->mapToGlobal(from),
                        Qt::NoButton, Qt::NoButton, Qt::NoModifier);
      QApplication::sendEvent(view->viewport(), &hover);
      QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, from);
      QMouseEvent drag(QEvent::MouseMove, QPointF(to), view->viewport()->mapToGlobal(to),
                       Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
      QApplication::sendEvent(view->viewport(), &drag);
      // QGIS mouse handles use lastScenePos(), so supply consecutive movement events
      // as a real pointer does before releasing the button.
      QApplication::sendEvent(view->viewport(), &drag);
      QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, to);
      const QPointF moved = item->pos();
      qInfo() << "decoration drag" << id << before << from << to << moved << item->rect().size();
      const QString dragOutput = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
      if (!dragOutput.isEmpty() && QLineF(before.topLeft(), moved).length() <= 3.)
        studio.grab().save(QDir(dragOutput).filePath(QStringLiteral("decoration-drag-failure.png")));
      QVERIFY2(QLineF(before.topLeft(), moved).length() > 3., qPrintable(id + QStringLiteral(" did not drag")));
      QVERIFY2(qAbs(item->itemRotation() - rotationBefore) < .001,
               qPrintable(id + QStringLiteral(" 가 회전했다")));
      positions.insert(id, moved);
      const auto* bar = dynamic_cast<QgsLayoutItemScaleBar*>(item);
      const double distance = bar ? bar->unitsPerSegment() : 0.;
      const auto* label = dynamic_cast<QgsLayoutItemLabel*>(item);
      const double fontSize = label ? label->font().pointSizeF() : 0.;
      size->setValue(50.);
      QVERIFY(item->rect().width() < before.width() * .8);
      QVERIFY(item->rect().height() < before.height());
      QVERIFY(QLineF(item->pos(), moved).length() < .01);
      if (bar) QVERIFY(qAbs(bar->unitsPerSegment() / distance - .5) < .001);
      if (label) QVERIFY(qAbs(label->font().pointSizeF() / fontSize - .5) < .001);
      view->setFocus();
      studio.undoLastChange();
      item = ly->itemById(id); // QGIS undo may replace the layout item.
      QVERIFY(item);
      QCOMPARE(size->value(), 100.);
      QVERIFY(qAbs(item->rect().width() - before.width()) < .01);
      size->setValue(50.);
      const QSizeF shrunk = item->rect().size();
      // A normal scale refresh used to reset and lock every decoration.
      QVERIFY(QMetaObject::invokeMethod(&studio, "flushHeavyScaleSync"));
      QVERIFY(QLineF(item->pos(), moved).length() < .01);
      QVERIFY(qAbs(item->rect().width() - shrunk.width()) < .01);
      QVERIFY(!item->isLocked());
    }
    auto* bar = dynamic_cast<QgsLayoutItemScaleBar*>(ly->itemById(QStringLiteral("ka_scalebar")));
    QVERIFY(bar);
    const QPointF barPosition = bar->pos();
    const double oldDistance = bar->unitsPerSegment();
    map->setScale(map->scale() * 2.);
    QVERIFY(QMetaObject::invokeMethod(&studio, "flushHeavyScaleSync"));
    QVERIFY(bar->unitsPerSegment() > oldDistance);
    QCOMPARE(bar->linkedMap(), map);
    QVERIFY(QLineF(bar->pos(), barPosition).length() < .01);
    studio.refreshMapFromProject();
    QVERIFY(QLineF(bar->pos(), barPosition).length() < .01);
    ly->setSelectedItem(bar);
    QCOMPARE(size->value(), 50.);
    for (auto it = positions.cbegin(); it != positions.cend(); ++it)
      QVERIFY(QLineF(ly->itemById(it.key())->pos(), it.value()).length() < .01);
    // The lower bound must shrink the rendered bar and distance again.
    ly->setSelectedItem(bar);
    const double halfDistance = bar->unitsPerSegment();
    const double halfWidth = bar->rect().width();
    size->setValue(25.);
    QVERIFY(qAbs(bar->unitsPerSegment() / halfDistance - .5) < .001);
    QVERIFY(bar->rect().width() < halfWidth * .8);
    size->setValue(50.);
    // Exercise style switches after shrinking: these used to recreate/reset items.
    QVERIFY(QMetaObject::invokeMethod(&studio, "beginPlaceNorth", Q_ARG(QString, QStringLiteral("arrows/NorthArrow_04.svg"))));
    QVERIFY(QMetaObject::invokeMethod(&studio, "beginPlaceScaleBar", Q_ARG(QString, QStringLiteral("Line Ticks Up"))));
    QVERIFY(QMetaObject::invokeMethod(&studio, "beginPlaceScaleLabel"));
    QCoreApplication::processEvents();
    studio.refreshMapFromProject();
    for (auto it = positions.cbegin(); it != positions.cend(); ++it)
      QVERIFY(QLineF(ly->itemById(it.key())->pos(), it.value()).length() < .01);
    QgsLayoutExporter exporter(ly);
    const QImage page = exporter.renderPageToImage(0, QSize(), 150.);
    QVERIFY(!page.isNull());
    const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    QTemporaryDir temp;
    const QString pdf = QDir(output.isEmpty() ? temp.path() : output).filePath(QStringLiteral("decoration-resized.pdf"));
    QgsLayoutExporter::PdfExportSettings settings;
    QCOMPARE(exporter.exportToPdf(pdf, settings), QgsLayoutExporter::Success);
    QVERIFY(QFileInfo(pdf).size() > 1000);
    if (!output.isEmpty()) {
      QVERIFY(QMetaObject::invokeMethod(&studio, "zoomPaperVisible"));
      QVERIFY(page.save(QDir(output).filePath(QStringLiteral("decoration-resized.png"))));
      QVERIFY(studio.grab().save(QDir(output).filePath(QStringLiteral("decoration-editor.png"))));
    }
  }
  void drawingStudio_northArrowRendersOnOpen() {
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    QgsMapCanvas canvas;
    canvas.setRenderFlag(false);
    canvas.setDestinationCrs(project.crs());
    canvas.setExtent(QgsRectangle(155000, 451000, 156000, 452000));
    QString northPath;
    {
      KaDrawingStudio studio(&project, &canvas, 297., 210.);
      studio.setAttribute(Qt::WA_DontShowOnScreen);
      studio.show();
      QCoreApplication::processEvents();
      auto* view = studio.findChild<QgsLayoutView*>();
      QVERIFY(view && view->currentLayout());
      QTRY_VERIFY(view->currentLayout()->itemById(QStringLiteral("ka_north")));
      auto* pic = dynamic_cast<QgsLayoutItemPicture*>(
          view->currentLayout()->itemById(QStringLiteral("ka_north")));
      QVERIFY(pic);
      QVERIFY2(!pic->isMissingImage(), qPrintable(pic->evaluatedPath()));
      northPath = pic->evaluatedPath();
      QVERIFY(!northPath.isEmpty());
      if (QFile::exists(northPath))
        QVERIFY(QFile::remove(northPath));
      pic->setPicturePath(QStringLiteral("C:/ka-hgis-missing-north.png"), Qgis::PictureFormat::Raster);
      pic->setMode(Qgis::PictureFormat::Raster);
      pic->refreshPicture();
      QVERIFY(pic->isMissingImage());
    }
    KaDrawingStudio reopen(&project, &canvas, 297., 210.);
    reopen.setAttribute(Qt::WA_DontShowOnScreen);
    reopen.show();
    QCoreApplication::processEvents();
    auto* view = reopen.findChild<QgsLayoutView*>();
    QVERIFY(view && view->currentLayout());
    auto* pic = dynamic_cast<QgsLayoutItemPicture*>(
        view->currentLayout()->itemById(QStringLiteral("ka_north")));
    QVERIFY(pic);
    QVERIFY2(!pic->isMissingImage(), qPrintable(pic->evaluatedPath()));
    QVERIFY(QFile::exists(pic->evaluatedPath()));
  }
  void drawingStudio_fieldPageGrowsA4LandscapeByOneCentimetre() {
    QCOMPARE(KaDrawingStudio::kFieldPaperWidthMm, 317.0);
    QCOMPARE(KaDrawingStudio::kFieldPaperHeightMm, 220.0);
    QVERIFY(KaDrawingStudio::isLegacyA4LandscapeMm(297.0, 210.0));
    QVERIFY(!KaDrawingStudio::isLegacyA4LandscapeMm(317.0, 220.0));
    QVERIFY(!KaDrawingStudio::isLegacyA4LandscapeMm(210.0, 297.0));

    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    QgsMapCanvas canvas;
    canvas.setRenderFlag(false);
    canvas.setDestinationCrs(project.crs());
    canvas.setExtent(QgsRectangle(155000, 451000, 156000, 452000));
    KaDrawingStudio studio(&project, &canvas, 297., 210.);
    studio.setAttribute(Qt::WA_DontShowOnScreen);
    studio.show();
    QCoreApplication::processEvents();
    auto* view = studio.findChild<QgsLayoutView*>();
    QVERIFY(view && view->currentLayout());
    auto* ly = view->currentLayout();
    auto* page = ly->pageCollection()->page(0);
    QVERIFY(page);
    const auto before = ly->renderContext().measurementConverter().convert(
        page->pageSize(), Qgis::LayoutUnit::Millimeters);
    QCOMPARE(before.width(), 297.0);
    QCOMPARE(before.height(), 210.0);

    auto* map = dynamic_cast<QgsLayoutItemMap*>(ly->itemById(QStringLiteral("ka_map")));
    QVERIFY(map);
    const QSizeF beforeMap = map->rect().size();
    studio.applyFieldPageGrow();
    const auto after = ly->renderContext().measurementConverter().convert(
        page->pageSize(), Qgis::LayoutUnit::Millimeters);
    QCOMPARE(after.width(), 317.0);
    QCOMPARE(after.height(), 220.0);
    qInfo() << "field page grow" << beforeMap << map->rect().size() << map->pos();
    QVERIFY2(qAbs(map->rect().width() - 297.0) < 1.0, "map width is page minus 10mm sides");
    QVERIFY2(qAbs(map->rect().height() - 172.0) < 1.0, "map height is page minus 10mm top and 38mm chrome");
    QVERIFY2(qAbs(map->pos().x() - 10.0) < 1.0, "left margin stays 10mm");

    studio.applyFieldPageGrow();
    const auto again = ly->renderContext().measurementConverter().convert(
        page->pageSize(), Qgis::LayoutUnit::Millimeters);
    QCOMPARE(again.width(), 317.0);
    QCOMPARE(again.height(), 220.0);

    QgsProject reopenProject;
    reopenProject.setCrs(project.crs());
    {
      KaDrawingStudio first(&reopenProject, &canvas, 297., 210.);
      first.setAttribute(Qt::WA_DontShowOnScreen);
    }
    KaDrawingStudio reopen(&reopenProject, &canvas, 297., 210.);
    reopen.setAttribute(Qt::WA_DontShowOnScreen);
    reopen.show();
    QCoreApplication::processEvents();
    auto* reopenView = reopen.findChild<QgsLayoutView*>();
    QVERIFY(reopenView && reopenView->currentLayout());
    auto* reopenPage = reopenView->currentLayout()->pageCollection()->page(0);
    QVERIFY(reopenPage);
    const auto grown = reopenView->currentLayout()->renderContext().measurementConverter().convert(
        reopenPage->pageSize(), Qgis::LayoutUnit::Millimeters);
    QCOMPARE(grown.width(), 317.0);
    QCOMPARE(grown.height(), 220.0);
  }
  // 2026-10-28 현장: 조판에서 용지를 A4·A3로 바꾸면 아래 축척자·축척 글·방위가 엉뚱한 곳으로 갔다.
  // 용지를 바꾸면 네 장식은 어디에 있었든 새 용지의 지도 아래 띠(standardSheetChrome) 자리로 돌아온다.
  void drawingStudio_paperChangePutsChromeBackInTheStandardStrip() {
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    QgsMapCanvas canvas;
    canvas.setRenderFlag(false);
    canvas.setDestinationCrs(project.crs());
    canvas.setExtent(QgsRectangle(190000, 560000, 191000, 561000));
    KaDrawingStudio studio(&project, &canvas, 210., 297.);
    studio.setAttribute(Qt::WA_DontShowOnScreen);
    studio.show();
    QCoreApplication::processEvents();
    QTest::qWait(200);
    auto* view = studio.findChild<QgsLayoutView*>();
    QVERIFY(view && view->currentLayout());
    auto* ly = view->currentLayout();
    auto* map = dynamic_cast<QgsLayoutItemMap*>(ly->itemById(QStringLiteral("ka_map")));
    QVERIFY(map);
    auto rectOf = [](QgsLayoutItem* item) { return QRectF(item->pos(), item->rect().size()); };
    auto pageMm = [ly]() {
      auto* page = ly->pageCollection()->page(0);
      const QgsLayoutSize size = ly->renderContext().measurementConverter().convert(
          page->pageSize(), Qgis::LayoutUnit::Millimeters);
      return QSizeF(size.width(), size.height());
    };
    // 네 장식이 지도 아래 띠의 정한 자리에 있는가. 축척자 너비는 축척에 따라 다르니 왼쪽·위만 본다.
    auto inStandardStrip = [&](const char* when) -> bool {
      const QSizeF paper = pageMm();
      const auto chrome =
          LayoutService::standardSheetChrome(QRectF(0.0, 0.0, paper.width(), paper.height()), rectOf(map));
      auto* bar = ly->itemById(QStringLiteral("ka_scalebar"));
      auto* label = ly->itemById(QStringLiteral("ka_scale"));
      auto* crs = ly->itemById(QStringLiteral("ka_crs"));
      auto* north = ly->itemById(QStringLiteral("ka_north"));
      if (!bar || !label || !crs || !north) {
        qWarning() << when << "장식이 없다" << (bar != nullptr) << (label != nullptr) << (crs != nullptr) << (north != nullptr);
        return false;
      }
      auto close = [](double a, double b) { return qAbs(a - b) < 0.6; };
      const QRectF b = rectOf(bar), l = rectOf(label), c = rectOf(crs), n = rectOf(north);
      bool ok = true;
      if (!close(b.left(), chrome.scaleBar.left()) || !close(b.top(), chrome.scaleBar.top())) {
        qWarning() << when << "축척자" << b << "기대" << chrome.scaleBar;
        ok = false;
      }
      if (!close(l.left(), b.left()) || !close(l.top(), b.bottom() + 2.5)) {
        qWarning() << when << "축척 글" << l << "축척자" << b;
        ok = false;
      }
      if (!close(c.left(), chrome.crs.left()) || !close(c.top(), chrome.crs.top())) {
        qWarning() << when << "좌표계" << c << "기대" << chrome.crs;
        ok = false;
      }
      if (!close(n.left(), chrome.north.left()) || !close(n.top(), chrome.north.top())) {
        qWarning() << when << "방위" << n << "기대" << chrome.north;
        ok = false;
      }
      if (b.right() > c.left() - 1.0) {
        qWarning() << when << "축척자가 좌표계 글과 겹친다" << b << c;
        ok = false;
      }
      return ok;
    };
    QCOMPARE(pageMm(), QSizeF(210.0, 297.0));
    QVERIFY(inStandardStrip("A4 세로 처음"));

    // 장식을 엉뚱한 곳에 둔 채 용지를 A3 가로로 바꾼다.
    ly->itemById(QStringLiteral("ka_north"))->attemptSetSceneRect(QRectF(30.0, 150.0, 20.0, 20.0));
    auto* barItem = ly->itemById(QStringLiteral("ka_scalebar"));
    barItem->attemptSetSceneRect(QRectF(80.0, 120.0, barItem->rect().width(), barItem->rect().height()));
    studio.resetPaper(420., 297.);
    QCoreApplication::processEvents();
    qInfo() << "A3 가로로 바꾼 뒤 지도" << map->pos() << map->rect().size() << "용지" << ly->pageCollection()->page(0)->pos();
    QCOMPARE(pageMm(), QSizeF(420.0, 297.0));
    QVERIFY2(QLineF(ly->pageCollection()->page(0)->pos(), QPointF(0.0, 0.0)).length() < 0.01,
             "용지 자체는 제자리(0,0)에 있어야 한다");
    QVERIFY2(qAbs(map->pos().x() - 10.0) < 0.6 && qAbs(map->pos().y() - 10.0) < 0.6, "지도 칸은 위·왼쪽 10 mm");
    QVERIFY2(qAbs(map->rect().width() - 400.0) < 0.6, "지도 칸 폭은 용지 너비에서 양쪽 10 mm를 뺀 것");
    QVERIFY(inStandardStrip("A3 가로로 바꾼 뒤"));
    // 축척 동기화 타이머가 돈 뒤에도 그대로다.
    QTest::qWait(600);
    QVERIFY(inStandardStrip("A3 가로, 타이머 뒤"));

    // 다시 A4 세로, 그리고 A3 세로: 어느 크기로 바꿔도 같은 자리다.
    studio.resetPaper(210., 297.);
    QCoreApplication::processEvents();
    QCOMPARE(pageMm(), QSizeF(210.0, 297.0));
    QVERIFY(inStandardStrip("A4 세로로 되돌린 뒤"));
    studio.resetPaper(297., 420.);
    QCoreApplication::processEvents();
    QCOMPARE(pageMm(), QSizeF(297.0, 420.0));
    QVERIFY2(QLineF(ly->pageCollection()->page(0)->pos(), QPointF(0.0, 0.0)).length() < 0.01,
             "용지 자체는 제자리(0,0)에 있어야 한다");
    QVERIFY(inStandardStrip("A3 세로로 바꾼 뒤"));
  }
  void drawingStudio_heritageRefreshRequestsCoalesceAndReuseOnRevisit() {
    constexpr int categoryCount = 200;
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* layer = new QgsVectorLayer(
        QStringLiteral("Polygon?crs=EPSG:5187&field=nm:string(80)"),
        QStringLiteral("합성 지표 자료"), QStringLiteral("memory"));
    QVERIFY(layer->isValid());
    QVERIFY(layer->startEditing());
    for (int i = 0; i < categoryCount; ++i) {
      const double x = 190050. + (i % 20) * 80.;
      const double y = 560050. + (i / 20) * 80.;
      QgsFeature feature(layer->fields());
      feature.setAttribute(QStringLiteral("nm"), QStringLiteral("합성 유적 %1").arg(i, 3, 10, QLatin1Char('0')));
      feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(x, y, x + 30., y + 30.)));
      QVERIFY(layer->addFeature(feature));
    }
    QVERIFY(layer->commitChanges());
    QVERIFY(HeritageStyle::apply(layer, HeritageDataset::SurfaceSurveyArea, QStringLiteral("nm")).ok);
    LayerOps::markReferenceLayer(layer);
    project.addMapLayer(layer, false);
    project.layerTreeRoot()->addGroup(QStringLiteral("참조 지도"))
        ->addGroup(HeritageStyle::layerName(HeritageDataset::SurfaceSurveyArea))->addLayer(layer);
    QgsMapCanvas canvas;
    canvas.setRenderFlag(false);
    canvas.resize(800, 600);
    canvas.setDestinationCrs(project.crs());
    canvas.setLayers({layer});
    canvas.setExtent(QgsRectangle(190000., 560000., 192000., 562000.));
    QElapsedTimer elapsed;
    elapsed.start();
    KaDrawingStudio studio(&project, &canvas, 297., 210.);
    studio.setAttribute(Qt::WA_DontShowOnScreen);
    studio.resize(1400, 900);
    studio.show();
    auto* view = studio.findChild<QgsLayoutView*>();
    QVERIFY(view && view->currentLayout());
    auto* layout = view->currentLayout();
    QTRY_VERIFY_WITH_TIMEOUT(layout->itemById(QStringLiteral("ka_map")), 5000);
    auto* map = dynamic_cast<QgsLayoutItemMap*>(layout->itemById(QStringLiteral("ka_map")));
    QVERIFY(map);
    // Match MainWindow's real first-entry centering before measuring revisits.
    studio.centerOnMapCanvas();
    QTRY_VERIFY_WITH_TIMEOUT(map->layerStyleOverrides().contains(layer->id()), 5000);
    const qint64 firstReadyMs = elapsed.elapsed();
    QVERIFY(QMetaObject::invokeMethod(&studio, "beginPlaceLegend"));
    auto* legend = dynamic_cast<QgsLayoutItemLegend*>(layout->itemById(QStringLiteral("ka_legend")));
    QVERIFY(legend);
    QSignalSpy overridesChanged(map, &QgsLayoutItemMap::layerStyleOverridesChanged);
    QSignalSpy previewRendered(map, &QgsLayoutItemMap::previewRefreshed);
    map->invalidateCache();
    map->refresh();
    view->viewport()->update();
    QTRY_VERIFY_WITH_TIMEOUT(previewRendered.count() > 0, 5000);
    // Let the 80 ms layer and 280 ms scale timers settle after first paint.
    QTest::qWait(450);
    QTRY_VERIFY_WITH_TIMEOUT(legend->model()->rootGroup()->findLayer(layer->id()), 5000);
    overridesChanged.clear();
    auto* legendLayer = legend->model()->rootGroup()->findLayer(layer->id());
    QVERIFY(legendLayer);
    const auto legendNodes = legend->model()->layerLegendNodes(legendLayer);
    QVERIFY(!legendNodes.isEmpty());
    QPointer<QgsLayerTreeModelLegendNode> retainedLegendNode(legendNodes.first());
    for (int i = 0; i < 10; ++i) {
      studio.centerOnMapCanvas();
      studio.refreshMapFromProject();
    }
    QTest::qWait(450);
    QCOMPARE(overridesChanged.count(), 0);
    QVERIFY(!retainedLegendNode.isNull());
    QCOMPARE(legend->model()->layerLegendNodes(legendLayer).first(), retainedLegendNode.data());

    studio.hide();
    for (int i = 0; i < 10; ++i) studio.refreshMapFromProject();
    QTest::qWait(450);
    QCOMPARE(overridesChanged.count(), 0);
    elapsed.restart();
    studio.show();
    studio.centerOnMapCanvas();
    studio.refreshMapFromProject();
    QCoreApplication::processEvents();
    const qint64 revisitShowMs = elapsed.elapsed();
    QTest::qWait(450);
    QCOMPARE(overridesChanged.count(), 0);
    QVERIFY(!retainedLegendNode.isNull());

    auto recolorSource = [&](const QColor& color) {
      if (auto* renderer = dynamic_cast<QgsCategorizedSymbolRenderer*>(layer->renderer()->clone())) {
        for (int i = 0; i < renderer->categories().size(); ++i) {
          auto* symbol = renderer->categories().at(i).symbol()->clone();
          symbol->setColor(color);
          renderer->updateCategorySymbol(i, symbol);
        }
        layer->setRenderer(renderer);
        return true;
      }
      if (auto* renderer = dynamic_cast<QgsSingleSymbolRenderer*>(layer->renderer()->clone())) {
        auto* symbol = renderer->symbol()->clone();
        symbol->setColor(color);
        renderer->setSymbol(symbol);
        layer->setRenderer(renderer);
        return true;
      }
      return false;
    };
    auto overrideFill = [&]() {
      std::unique_ptr<QgsVectorLayer> drawing(layer->clone());
      QgsMapLayerStyle(map->layerStyleOverrides().value(layer->id())).writeToLayer(drawing.get());
      return drawing->labeling() ? drawing->labeling()->settings().format().background().fillColor() : QColor();
    };
    const QColor visibleColor(QStringLiteral("#3178cc"));
    auto* numberOwner = HeritageLayoutNumbers::forMap(map);
    QVERIFY(numberOwner);
    quint64 candidateRevision = numberOwner->revision();
    QVERIFY(recolorSource(visibleColor));
    for (int i = 0; i < 10; ++i) studio.refreshMapFromProject();
    QTRY_COMPARE_WITH_TIMEOUT(numberOwner->revision(), candidateRevision + 1, 5000);
    QTest::qWait(450);
    QCOMPARE(numberOwner->revision(), candidateRevision + 1);
    QCOMPARE(overrideFill(), visibleColor);

    overridesChanged.clear();
    studio.hide();
    candidateRevision = numberOwner->revision();
    const QColor hiddenColor(QStringLiteral("#a43b92"));
    QVERIFY(recolorSource(hiddenColor));
    for (int i = 0; i < 10; ++i) studio.refreshMapFromProject();
    QTest::qWait(450);
    QCOMPARE(overridesChanged.count(), 0);
    studio.show();
    QTRY_COMPARE_WITH_TIMEOUT(numberOwner->revision(), candidateRevision + 1, 5000);
    QTest::qWait(450);
    QCOMPARE(numberOwner->revision(), candidateRevision + 1);
    QCOMPARE(overrideFill(), hiddenColor);

    // Editing only the legend's presentation must retain numbered nodes.
    QTRY_VERIFY_WITH_TIMEOUT(legend->model()->rootGroup()->findLayer(layer->id()), 5000);
    legendLayer = legend->model()->rootGroup()->findLayer(layer->id());
    QVERIFY(legendLayer);
    retainedLegendNode = legend->model()->layerLegendNodes(legendLayer).first();
    QLineEdit* legendTitle = nullptr;
    QSpinBox* legendFont = nullptr;
    for (auto* edit : studio.findChildren<QLineEdit*>())
      if (edit->placeholderText() == QStringLiteral("제목을 입력하세요")) legendTitle = edit;
    for (auto* spin : studio.findChildren<QSpinBox*>())
      if (spin->suffix() == QStringLiteral(" pt") && spin->maximum() == 24) legendFont = spin;
    QVERIFY(legendTitle && legendFont);
    overridesChanged.clear();
    legendTitle->setText(QStringLiteral("합성 범례 조절"));
    legendFont->setValue(11);
    QCoreApplication::processEvents();
    QCOMPARE(legend->title(), QStringLiteral("합성 범례 조절"));
    QVERIFY(!retainedLegendNode.isNull());
    QCOMPARE(legend->model()->layerLegendNodes(legendLayer).first(), retainedLegendNode.data());
    QCOMPARE(overridesChanged.count(), 0);
    QCOMPARE(legend->cacheMode(), QGraphicsItem::DeviceCoordinateCache);

    legend->attemptResize(QgsLayoutSize(70., legend->rect().height()));
    QTRY_COMPARE_WITH_TIMEOUT(legend->columnCount(), 1, 5000);
    QTest::qWait(450);
    overridesChanged.clear();
    candidateRevision = numberOwner->revision();
    QVERIFY(QMetaObject::invokeMethod(&studio, "useSelectTool"));
    const QPoint pressAt(3, 3);
    QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, pressAt);
    QVERIFY(studio.property("ka_interacting").toBool());
    QVERIFY(legend->property("ka_interacting").toBool());
    const double originalScale = studio.drawingScale();
    const double finalScale = std::round(originalScale * 1.4);
    for (int i = 1; i <= 4; ++i) {
      studio.setDrawingScale(originalScale * (1. + i * .1));
      studio.refreshMapFromProject();
    }
    legend->attemptResize(QgsLayoutSize(190., legend->rect().height()));
    // Longer than both the scale and legend debounce intervals: held input
    // must keep deferring feature/legend work instead of merely delaying it once.
    QTest::qWait(400);
    QVERIFY(studio.property("ka_interacting").toBool());
    QCOMPARE(overridesChanged.count(), 0);
    QCOMPARE(legend->columnCount(), 1);
    QVERIFY(qAbs(studio.drawingScale() - finalScale) < .5);
    QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, pressAt);
    QTRY_VERIFY_WITH_TIMEOUT(!studio.property("ka_interacting").toBool(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(numberOwner->revision(), candidateRevision + 1, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(legend->columnCount() > 1, 5000);
    QTest::qWait(450);
    QCOMPARE(numberOwner->revision(), candidateRevision + 1);
    QVERIFY(qAbs(legend->rect().width() - 190.) < .1);
    auto* paperScale = dynamic_cast<QgsLayoutItemLabel*>(layout->itemById(QStringLiteral("ka_scale")));
    QVERIFY(paperScale);
    QCOMPARE(paperScale->text(), QStringLiteral("축척 1 : %1").arg(finalScale, 0, 'f', 0));
    QTRY_VERIFY_WITH_TIMEOUT(legend->model()->rootGroup()->findLayer(layer->id()), 5000);
    legendLayer = legend->model()->rootGroup()->findLayer(layer->id());
    QVERIFY(legendLayer);
    auto placedNumbers = [&]() {
      QSet<QString> result;
      auto* numberMap = HeritageLayoutNumbers::numbersMapOf(map);
      if (!numberMap) return result;
      for (auto* candidate : numberMap->layers()) {
        auto* pins = qobject_cast<QgsVectorLayer*>(candidate);
        if (!pins || pins->fields().indexOf(QStringLiteral("num")) < 0) continue;
        QgsFeature feature;
        auto it = pins->getFeatures();
        while (it.nextFeature(feature)) {
          if (feature.attribute(QStringLiteral("layer")).toString() != layer->id()) continue;
          result.insert(feature.attribute(QStringLiteral("num")).toString());
        }
      }
      return result;
    };
    auto legendNumbers = [&]() {
      QSet<QString> result;
      if (auto* tree = legend->model()->rootGroup()->findLayer(layer->id()))
        for (auto* node : legend->model()->layerLegendNodes(tree)) {
          auto* symbol = dynamic_cast<QgsSymbolLegendNode*>(node);
          if (!symbol || !symbol->customSymbol()) continue;
          for (int i = 0; i < symbol->customSymbol()->symbolLayerCount(); ++i)
            if (auto* glyph = dynamic_cast<QgsFontMarkerSymbolLayer*>(symbol->customSymbol()->symbolLayer(i)))
              result.insert(glyph->character());
        }
      return result;
    };
    QTRY_VERIFY_WITH_TIMEOUT(!placedNumbers().isEmpty(), 10000);
    QTRY_COMPARE_WITH_TIMEOUT(legendNumbers(), placedNumbers(), 10000);
    legendLayer = legend->model()->rootGroup()->findLayer(layer->id());
    QVERIFY(legendLayer);
    const auto finalNodes = legend->model()->layerLegendNodes(legendLayer);
    QVERIFY(finalNodes.size() <= categoryCount);
    QMap<QString, int> displayedNumbers;
    int nextNumber = 0;
    for (auto* node : finalNodes) {
      auto* symbolNode = dynamic_cast<QgsSymbolLegendNode*>(node);
      QVERIFY(symbolNode && symbolNode->customSymbol());
      QString badge;
      for (int i = 0; i < symbolNode->customSymbol()->symbolLayerCount(); ++i)
        if (auto* font = dynamic_cast<QgsFontMarkerSymbolLayer*>(symbolNode->customSymbol()->symbolLayer(i)))
          badge = font->character();
      const QString name = node->data(Qt::DisplayRole).toString();
      QVERIFY(!name.isEmpty());
      QCOMPARE(badge, QString::number(++nextNumber));
      displayedNumbers.insert(name, nextNumber);
    }
    std::unique_ptr<QgsVectorLayer> drawing(layer->clone());
    QgsMapLayerStyle(map->layerStyleOverrides().value(layer->id())).writeToLayer(drawing.get());
    QVERIFY(drawing->labeling());
    QgsExpression labelExpression(drawing->labeling()->settings().fieldName);
    QVERIFY(!labelExpression.hasParserError());
    auto expressionContext = drawing->createExpressionContext();
    QgsFeature firstFeature;
    QVERIFY(drawing->getFeatures().nextFeature(firstFeature));
    expressionContext.setFeature(firstFeature);
    QCOMPARE(labelExpression.evaluate(&expressionContext).toInt(),
             displayedNumbers.value(firstFeature.attribute(QStringLiteral("nm")).toString()));
    QVERIFY(!labelExpression.hasEvalError());

    // Record repeated scene paints while moving the same cached legend. These
    // timings are diagnostic observations, not a machine-dependent pass limit.
    const QPointF legendPosition = legend->pos();
    QImage sceneImage(640, 480, QImage::Format_ARGB32_Premultiplied);
    elapsed.restart();
    for (int i = 0; i < 4; ++i) {
      legend->attemptMove(QgsLayoutPoint(legendPosition + QPointF(i, 0.)));
      sceneImage.fill(Qt::white);
      QPainter painter(&sceneImage);
      layout->render(&painter, QRectF(0., 0., 640., 480.), legend->sceneBoundingRect());
    }
    const qint64 legendMovePaintMs = elapsed.elapsed();
    legend->attemptMove(QgsLayoutPoint(legendPosition));
    qInfo() << "LAYOUT_INTERACTION held_ms=400 held_overrides=0 released_candidate_updates=1"
            << "legend_columns=" << legend->columnCount()
            << "legend_move_scene_paints=4 elapsed_ms=" << legendMovePaintMs;
    qInfo().noquote() << QStringLiteral(
        "LAYOUT_STUDIO_PERF categories=%1 first_ready_ms=%2 revisit_show_ms=%3 unchanged_refreshes=10 unchanged_overrides=0 visible_candidate_updates=1 hidden_candidate_updates=1")
        .arg(categoryCount).arg(firstReadyMs).arg(revisitShowMs);
    // A parent checkbox hides its descendants without erasing their own choices.
    auto* references = project.layerTreeRoot()->findGroup(QStringLiteral("참조 지도"));
    QVERIFY(references);
    auto* sourceNode = references->findLayer(layer->id());
    QVERIFY(sourceNode && sourceNode->itemVisibilityChecked());
    references->setItemVisibilityChecked(false);
    QTRY_VERIFY_WITH_TIMEOUT(!map->layers().contains(layer), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!map->layerStyleOverrides().contains(layer->id()), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!legend->model()->rootGroup()->findLayer(layer->id()), 5000);
    QVERIFY(sourceNode->itemVisibilityChecked());
    references->setItemVisibilityChecked(true);
    QTRY_VERIFY_WITH_TIMEOUT(map->layers().contains(layer), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(legend->model()->rootGroup()->findLayer(layer->id()), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!placedNumbers().isEmpty(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(legendNumbers(), placedNumbers(), 5000);
    // Moving the geographic map partly off paper changes the numbered subset
    // without changing its extent. Membership follows mapFootprintOnPaper, then
    // the full page set returns when the map is brought back onto the page.
    const QPointF beforeMove = map->pos();
    const int beforeMoveCount = legendNumbers().size();
    map->attemptMove(QgsLayoutPoint(-map->rect().width() * .5, beforeMove.y()));
    view->viewport()->update();
    auto clippedSubset = [&]() {
      return numberOwner->entries().size() < beforeMoveCount && !numberOwner->entries().isEmpty();
    };
    QTRY_VERIFY_WITH_TIMEOUT(clippedSubset(), 10000);
    QTRY_VERIFY_WITH_TIMEOUT(!legendNumbers().isEmpty() && legendNumbers().size() < beforeMoveCount, 10000);
    QTRY_COMPARE_WITH_TIMEOUT(legendNumbers(), placedNumbers(), 10000);
    const auto compacted = legendNumbers();
    for (int n = 1; n <= compacted.size(); ++n) QVERIFY(compacted.contains(QString::number(n)));
    map->attemptMove(QgsLayoutPoint(beforeMove));
    view->viewport()->update();
    QTRY_COMPARE_WITH_TIMEOUT(legendNumbers().size(), beforeMoveCount, 10000);
    QTRY_COMPARE_WITH_TIMEOUT(legendNumbers(), placedNumbers(), 10000);
  }
  void drawingStudioAboveLabelsMapDrawsGeometryWithoutDuplicateNumberLabels() {
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    QVector<QgsVectorLayer*> sources;
    const QList<HeritageDataset> datasets = {
        HeritageDataset::DesignatedHeritage, HeritageDataset::SurfaceSurveyArea};
    auto* references = project.layerTreeRoot()->addGroup(QStringLiteral("참조 지도"));
    for (int i = 0; i < datasets.size(); ++i) {
      auto* layer = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187&field=nm:string(80)"),
          QStringLiteral("합성 원본 %1").arg(i + 1), QStringLiteral("memory"));
      QVERIFY(layer->isValid());
      QVERIFY(layer->startEditing());
      QgsFeature feature(layer->fields());
      feature.setAttribute(QStringLiteral("nm"), QStringLiteral("원본 유적명 %1").arg(i + 1));
      const double offset = i * 30.;
      feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(
          190020. + offset, 560020. + offset, 190140. + offset, 560140. + offset)));
      QVERIFY(layer->addFeature(feature));
      QVERIFY(layer->commitChanges());
      QVERIFY(HeritageStyle::apply(layer, datasets[i], QStringLiteral("nm")).ok);
      QVERIFY(LayerOps::applyNameAttributeLabels(layer, QStringLiteral("nm"), 5., false));
      LayerOps::markReferenceLayer(layer);
      project.addMapLayer(layer, false);
      references->addGroup(HeritageStyle::layerName(datasets[i]))->addLayer(layer);
      sources.append(layer);
    }
    // Establish the existing map's label-order properties before capturing the
    // source baseline; the drawing must not change those styles or labels.
    LayerOps::applyLayerOrderToLabels(&project, nullptr);
    const QList<QgsMapLayer*> above = LayerOps::layersDrawnAboveLabels(&project);
    QVERIFY(above.contains(sources[0]));
    QVERIFY(above.contains(sources[1]));
    QMap<QString, QString> sourceStyles;
    QMap<QString, QString> sourceUris;
    for (auto* source : sources) {
      QgsMapLayerStyle style;
      style.readFromLayer(source);
      sourceStyles.insert(source->id(), style.xmlData());
      sourceUris.insert(source->id(), source->source());
      QVERIFY(source->labelsEnabled());
    }
    QgsMapCanvas canvas;
    canvas.setRenderFlag(false);
    canvas.setDestinationCrs(project.crs());
    canvas.setLayers({sources[0], sources[1]});
    canvas.setExtent(QgsRectangle(189950., 559950., 190250., 560250.));
    KaDrawingStudio studio(&project, &canvas, 297., 210.);
    studio.setAttribute(Qt::WA_DontShowOnScreen);
    studio.show();
    auto* view = studio.findChild<QgsLayoutView*>();
    QVERIFY(view && view->currentLayout());
    auto* layout = view->currentLayout();
    QTRY_VERIFY_WITH_TIMEOUT(layout->itemById(QStringLiteral("ka_map")), 5000);
    auto* base = dynamic_cast<QgsLayoutItemMap*>(layout->itemById(QStringLiteral("ka_map")));
    QVERIFY(base);
    QTRY_VERIFY_WITH_TIMEOUT(base->layerStyleOverrides().contains(sources[1]->id()), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(HeritageLayoutNumbers::numbersMapOf(base), 5000);
    auto* numbersMap = HeritageLayoutNumbers::numbersMapOf(base);
    QVERIFY(numbersMap);
    auto* overlay = dynamic_cast<QgsLayoutItemMap*>(layout->itemById(QStringLiteral("ka_map_above")));
    QVERIFY2(overlay, "주변유적은 본지도가 아니라 덧지도에 전부 그린다");
    QVERIFY(overlay->layers().contains(sources[0]));
    QVERIFY(overlay->layers().contains(sources[1]));
    QVERIFY(base->layers().contains(sources[0]));
    QVERIFY(base->layers().contains(sources[1]));
    QVERIFY(!numbersMap->layers().contains(sources[0]));
    QVERIFY(!numbersMap->layers().contains(sources[1]));
    bool numberPins = false;
    for (auto* candidate : numbersMap->layers()) {
      auto* pins = qobject_cast<QgsVectorLayer*>(candidate);
      numberPins = numberPins || (pins && pins->fields().indexOf(QStringLiteral("num")) >= 0);
    }
    QVERIFY2(numberPins, "번호는 원본 도형이 아니라 조판 번호 핀에 그린다");
    for (auto* source : sources) {
      std::unique_ptr<QgsVectorLayer> baseDrawing(source->clone());
      QgsMapLayerStyle(base->layerStyleOverrides().value(source->id())).writeToLayer(baseDrawing.get());
      QVERIFY(!baseDrawing->labelsEnabled());
      std::unique_ptr<QgsVectorLayer> drawing(source->clone());
      QgsMapLayerStyle(numbersMap->layerStyleOverrides().value(source->id())).writeToLayer(drawing.get());
      QVERIFY(!drawing->labelsEnabled());
      QgsVectorLayer* pins = nullptr;
      for (auto* candidate : numbersMap->layers()) {
        auto* vector = qobject_cast<QgsVectorLayer*>(candidate);
        if (vector && vector->fields().indexOf(QStringLiteral("num")) >= 0) pins = vector;
      }
      QVERIFY(pins);
      bool sawOne = false;
      QgsFeature pin;
      auto features = pins->getFeatures();
      while (features.nextFeature(pin)) {
        if (pin.attribute(QStringLiteral("layer")).toString() != source->id()) continue;
        QCOMPARE(pin.attribute(QStringLiteral("num")).toInt(), 1);
        sawOne = true;
      }
      QVERIFY2(sawOne, "자료마다 조판 번호는 1부터 시작한다");
      QgsMapLayerStyle unchanged;
      unchanged.readFromLayer(source);
      QCOMPARE(unchanged.xmlData(), sourceStyles.value(source->id()));
      QCOMPARE(source->source(), sourceUris.value(source->id()));
      QCOMPARE(source->featureCount(), 1LL);
      QVERIFY(source->labelsEnabled());
    }
  }

  // 「레이어를 가운데」는 축척을 그대로 둔 채 옮긴다. 주변유적 덧지도도 같이 옮겨야
  // 조사구역만 가운데로 오고 유적은 옛 자리에 남는 일이 없다.
  void drawingStudio_centerLayerMovesAboveLabelsMapToo() {
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* references = project.layerTreeRoot()->addGroup(QStringLiteral("참조 지도"));
    auto* heritage = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187&field=nm:string(80)"),
        QStringLiteral("합성 유적"), QStringLiteral("memory"));
    QVERIFY(heritage->isValid());
    QVERIFY(heritage->startEditing());
    QgsFeature site(heritage->fields());
    site.setAttribute(QStringLiteral("nm"), QStringLiteral("합성 유적명"));
    site.setGeometry(QgsGeometry::fromRect(QgsRectangle(190020., 560020., 190140., 560140.)));
    QVERIFY(heritage->addFeature(site));
    QVERIFY(heritage->commitChanges());
    QVERIFY(HeritageStyle::apply(heritage, HeritageDataset::DesignatedHeritage, QStringLiteral("nm")).ok);
    QVERIFY(LayerOps::applyNameAttributeLabels(heritage, QStringLiteral("nm"), 5., false));
    LayerOps::markReferenceLayer(heritage);
    project.addMapLayer(heritage, false);
    references->addGroup(HeritageStyle::layerName(HeritageDataset::DesignatedHeritage))->addLayer(heritage);
    // 조사구역은 지도 화면 한쪽 끝에 있어 「레이어를 가운데」가 지도를 옮기게 한다.
    auto* survey = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187"),
        QStringLiteral("조사구역"), QStringLiteral("memory"));
    QVERIFY(survey->isValid());
    QVERIFY(survey->startEditing());
    QgsFeature area(survey->fields());
    area.setGeometry(QgsGeometry::fromRect(QgsRectangle(190200., 560200., 190240., 560240.)));
    QVERIFY(survey->addFeature(area));
    QVERIFY(survey->commitChanges());
    survey->setCustomProperty(QStringLiteral("ka_hgis/layer_key"), QStringLiteral("survey_area"));
    project.addMapLayer(survey);
    LayerOps::applyLayerOrderToLabels(&project, nullptr);
    QVERIFY(LayerOps::layersDrawnAboveLabels(&project).contains(heritage));

    QgsMapCanvas canvas;
    canvas.setRenderFlag(false);
    canvas.setDestinationCrs(project.crs());
    canvas.setLayers({survey, heritage});
    canvas.setExtent(QgsRectangle(189950., 559950., 190250., 560250.));
    KaDrawingStudio studio(&project, &canvas, 297., 210.);
    studio.setAttribute(Qt::WA_DontShowOnScreen);
    studio.show();
    auto* view = studio.findChild<QgsLayoutView*>();
    QVERIFY(view && view->currentLayout());
    auto* layout = view->currentLayout();
    QTRY_VERIFY_WITH_TIMEOUT(layout->itemById(QStringLiteral("ka_map")), 5000);
    auto* base = dynamic_cast<QgsLayoutItemMap*>(layout->itemById(QStringLiteral("ka_map")));
    QVERIFY(base);
    // 축척 글자는 지도 칸을 다 차린 뒤에 붙는다. 사용자가 단추를 누를 때와 같은 상태.
    QTRY_VERIFY_WITH_TIMEOUT(layout->itemById(QStringLiteral("ka_scale")), 5000);
    auto* overlay = dynamic_cast<QgsLayoutItemMap*>(layout->itemById(QStringLiteral("ka_map_above")));
    QVERIFY(overlay);
    QTRY_VERIFY_WITH_TIMEOUT(overlay->extent().center().compare(base->extent().center(), 0.5), 5000);
    const QgsPointXY before = base->extent().center();
    const double scaleBefore = base->scale();

    studio.centerSurveyInMap();

    QVERIFY2(base->extent().center().distance(before) > 50., "본지도가 조사구역 쪽으로 옮겨져야 한다");
    QCOMPARE(qRound(base->scale()), qRound(scaleBefore));
    // 축척이 그대로여도 덧지도는 바로 따라와야 한다(자동 축척을 눌러야 맞던 버그).
    QVERIFY2(overlay->extent().center().compare(base->extent().center(), 0.5),
             qPrintable(QStringLiteral("덧지도 중심 %1 / 본지도 중심 %2")
                            .arg(overlay->extent().center().toString(1),
                                 base->extent().center().toString(1))));
    QCOMPARE(overlay->scale(), base->scale());
  }

  void drawingStudio_largeScaleChipsApplyToMapAndInput() {
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    QgsMapCanvas canvas;
    canvas.setRenderFlag(false);
    canvas.setDestinationCrs(project.crs());
    canvas.setExtent(QgsRectangle(190000, 560000, 191000, 561000));
    KaDrawingStudio studio(&project, &canvas, 297.0, 210.0);
    studio.setAttribute(Qt::WA_DontShowOnScreen);
    auto* view = studio.findChild<QgsLayoutView*>();
    QVERIFY(view && view->currentLayout());
    view->setUpdatesEnabled(false);
    studio.show();
    // Complete the studio's queued initial map placement before selecting a scale.
    QCoreApplication::processEvents();
    auto* map = dynamic_cast<QgsLayoutItemMap*>(view->currentLayout()->itemById(QStringLiteral("ka_map")));
    QVERIFY(map);
    QCOMPARE(map->crs().authid(), QStringLiteral("EPSG:5187"));
    auto* spin = studio.findChild<QSpinBox*>(QStringLiteral("drawingScale"));
    QVERIFY(spin);
    const QFontMetrics font(spin->font());
    QVERIFY(font.inFont(QChar(u'1')));
    QVERIFY(font.inFont(QChar(u'한')));
    auto* input = spin->findChild<QLineEdit*>();
    QVERIFY(input);
    const auto chips = studio.findChildren<QToolButton*>(QStringLiteral("scaleChip"));
    // Exercise both new choices and a return to an existing choice through the
    // actual clicked signal; setting the spinbox directly would miss bad wiring.
    for (const int denominator : {10000, 25000, 5000}) {
      QToolButton* target = nullptr;
      int matches = 0;
      for (QToolButton* chip : chips) {
        if (chip->property("denom").toInt() == denominator) {
          target = chip;
          ++matches;
        }
      }
      QCOMPARE(matches, 1);
      QVERIFY(target && target->isEnabled());
      target->click();
      QCOMPARE(spin->value(), denominator);
      QCOMPARE(input->text(), QString::number(denominator));
      for (QToolButton* chip : chips)
        QCOMPARE(chip->isChecked(), chip->property("denom").toInt() == denominator);
      QVERIFY2(qAbs(map->scale() - denominator) < 0.5,
               qPrintable(QStringLiteral("chip 1:%1 applied map scale %2")
                              .arg(denominator).arg(map->scale(), 0, 'f', 3)));
    }
    const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    if (!output.isEmpty() && QDir(output).exists()) {
      const QString path = QDir(output).filePath(QStringLiteral("drawing-scale-chips-widget-render.png"));
      QVERIFY2(spin->parentWidget()->grab().save(path), qPrintable(path));
      qInfo().noquote() << "Automatic Qt widget render; not a portable field screenshot:" << path;
    }
  }
  void drawingInspectorUsesHeightWithoutOverlapping_data() {
    QTest::addColumn<int>("height");
    QTest::newRow("normal") << 930;
    QTest::newRow("tall") << 1150;
    QTest::newRow("short") << 620;
  }
  void drawingInspectorUsesHeightWithoutOverlapping() {
    QFETCH(int, height);
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    QgsMapCanvas canvas;
    canvas.setRenderFlag(false);
    canvas.setDestinationCrs(project.crs());
    canvas.setExtent(QgsRectangle(190000, 560000, 191000, 561000));
    QTabWidget tabs;
    KaDrawingStudio studio(&project, &canvas, 210., 297., &tabs);
    studio.setParent(&tabs, Qt::Widget);
    tabs.addTab(&studio, QStringLiteral("레이아웃"));
    tabs.resize(1800, height);
    tabs.show();
    QCoreApplication::processEvents();
    auto* scale = studio.findChild<QSpinBox*>(QStringLiteral("drawingScale"));
    QVERIFY(scale);
    QWidget* card = scale->parentWidget();
    QWidget* panel = card->parentWidget();
    auto* scroll = studio.findChild<QScrollArea*>(QStringLiteral("drawingInspectorScroll"));
    const auto buttons = card->findChildren<QToolButton*>();
    QToolButton* crsButton = nullptr;
    for (auto* button : buttons) if (button->text() == QStringLiteral("좌표계")) crsButton = button;
    QVERIFY(crsButton);
    const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    const QString tag = QString::fromLatin1(QTest::currentDataTag());
    if (!output.isEmpty()) {
      QVERIFY(tabs.grab().save(QDir(output).filePath(tag + QStringLiteral("-studio.png"))));
      QVERIFY((scroll ? static_cast<QWidget*>(scroll) : panel)->grab().save(
          QDir(output).filePath(tag + QStringLiteral("-panel.png"))));
    }
    const int bottomGap = panel->height() - crsButton->mapTo(panel, QPoint(0, crsButton->height())).y();
    qInfo() << "inspector metrics: requested height" << height << "actual" << tabs.height()
            << "panel" << panel->size() << "bottom gap" << bottomGap;
    QVERIFY2(tabs.height() <= height, "Inspector must not force the app beyond its requested height");
    QVERIFY2(scroll, "Short windows need a scrollable inspector");
    QVERIFY2(bottomGap <= 24, qPrintable(QStringLiteral("Unused space below the last row: %1 px").arg(bottomGap)));
    for (auto* button : panel->findChildren<QToolButton*>()) {
      QVERIFY2(button->height() >= button->sizeHint().height(), qPrintable(
          QStringLiteral("%1 button clipped to %2 px; content needs %3 px")
              .arg(button->text()).arg(button->height()).arg(button->sizeHint().height())));
      QVERIFY(button->parentWidget()->rect().contains(button->geometry()));
    }
    QList<QToolButton*> firstColumn;
    for (int denominator : {100, 300, 1000, 10000}) {
      for (auto* button : buttons)
        if (button->property("denom").toInt() == denominator) firstColumn.append(button);
    }
    QCOMPARE(firstColumn.size(), 4);
    for (int i = 1; i < firstColumn.size(); ++i) {
      const auto* before = firstColumn[i - 1];
      const auto* after = firstColumn[i];
      const int gap = after->y() - before->geometry().bottom() - 1;
      qInfo() << "preset row gap" << gap;
      QVERIFY2(gap >= 8, qPrintable(QStringLiteral("Preset rows have only %1 px separation").arg(gap)));
    }
    for (int i = 0; i < buttons.size(); ++i)
      for (int j = i + 1; j < buttons.size(); ++j)
        QVERIFY(!buttons[i]->geometry().intersects(buttons[j]->geometry()));
    if (height < 700) {
      QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
      scroll->ensureWidgetVisible(crsButton);
      QCoreApplication::processEvents();
      const QRect lastRow(crsButton->mapTo(scroll->viewport(), QPoint()), crsButton->size());
      QVERIFY(scroll->viewport()->rect().contains(lastRow));
      if (!output.isEmpty()) QVERIFY(scroll->grab().save(QDir(output).filePath(tag + QStringLiteral("-scrolled.png"))));
      auto* splitter = studio.findChild<QSplitter*>(QStringLiteral("studioMainSplit"));
      QVERIFY(splitter);
      splitter->setSizes({268, 1200, 1});
      QCoreApplication::processEvents();
      QVERIFY(scroll->viewport()->width() >= panel->minimumSizeHint().width());
      for (auto* button : panel->findChildren<QToolButton*>()) {
        QVERIFY(button->parentWidget()->rect().contains(button->geometry()));
        QVERIFY2(button->width() >= button->sizeHint().width(), qPrintable(button->text()));
      }
      if (!output.isEmpty()) QVERIFY(scroll->grab().save(QDir(output).filePath(tag + QStringLiteral("-narrow.png"))));
    }
    scroll->ensureWidgetVisible(firstColumn.last());
    QTest::mouseClick(firstColumn.last(), Qt::LeftButton);
    QCOMPARE(scale->value(), 10000);
    auto* view = studio.findChild<QgsLayoutView*>();
    QVERIFY(view && view->currentLayout());
    auto* map = dynamic_cast<QgsLayoutItemMap*>(view->currentLayout()->itemById(QStringLiteral("ka_map")));
    QVERIFY(map && qAbs(map->scale() - 10000.) < .5);
    QCOMPARE(map->crs().authid(), QStringLiteral("EPSG:5187"));
  }

  void layerContextMenu_matchesLayerKind_data() {
    QTest::addColumn<QString>("kind");
    QTest::addColumn<QString>("firstAction");
    QTest::newRow("survey-area") << QStringLiteral("survey_area") << QStringLiteral("layer.import");
    QTest::newRow("feature-polygon") << QStringLiteral("feature_poly") << QStringLiteral("layer.import");
    QTest::newRow("control-points") << QStringLiteral("control_points") << QStringLiteral("layer.import");
    QTest::newRow("section-line") << QStringLiteral("section_line") << QStringLiteral("layer.import");
    QTest::newRow("trial-trench") << QStringLiteral("trial_trench") << QStringLiteral("layer.import");
    QTest::newRow("satellite-xyz") << QStringLiteral("satellite") << QStringLiteral("layer.import");
    QTest::newRow("cadastral-wms") << QStringLiteral("cadastral") << QStringLiteral("layer.import");
    QTest::newRow("dem-raster") << QStringLiteral("dem") << QStringLiteral("layer.import");
    QTest::newRow("external-shapefile") << QStringLiteral("external_shp") << QStringLiteral("layer.import");
    QTest::newRow("imported-reference-raster") << QStringLiteral("imported_raster") << QStringLiteral("layer.import");
  }
  void layerContextMenu_matchesLayerKind() {
    QFETCH(QString, kind);
    QFETCH(QString, firstAction);
    QString error;
    const QString path = SurveyProjectFactory::createNewSurvey(
        m_files.path(), QStringLiteral("menu_%1").arg(kind), &error, QStringLiteral("EPSG:5187"));
    QVERIFY2(!path.isEmpty(), qPrintable(error));
    MainWindow window;
    window.setProperty("qaMenuKind", kind);
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    const bool domain = LayerOps::domainLayerKeys().contains(kind);
    QgsMapLayer* layer = domain ? LayerOps::ensureDomainLayer(
        QgsProject::instance(), path, kind, kind, &error) : makeMenuReference(kind);
    QVERIFY2(layer && layer->isValid(), qPrintable(error));
    if (domain) {
      auto* vector = qobject_cast<QgsVectorLayer*>(layer);
      QVERIFY(vector && vector->startEditing());
      QgsFeature feature(vector->fields());
      if (vector->geometryType() == Qgis::GeometryType::Point)
        feature.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(190000, 560000)));
      else if (vector->geometryType() == Qgis::GeometryType::Line)
        feature.setGeometry(QgsGeometry::fromPolylineXY({QgsPointXY(190000, 560000), QgsPointXY(190100, 560100)}));
      else
        feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(190000, 560000, 190100, 560100)));
      QVERIFY(vector->addFeature(feature));
      QVERIFY(vector->commitChanges());
    }
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    QVERIFY(tree);
    window.show();
    tree->expandAll();
    tree->setCurrentLayer(layer);
    QApplication::processEvents();
    const QModelIndex index = tree->node2index(
        QgsProject::instance()->layerTreeRoot()->findLayer(layer));
    QVERIFY(index.isValid());
    tree->scrollTo(index);
    const LayerMenuState menu = inspectLayerMenu(window, tree, tree->visualRect(index).center());
    QVERIFY(menu.seen);
    QCOMPARE(menu.name, QStringLiteral("layerContextMenu"));
    QStringList ids;
    for (const auto& action : menu.actions) {
      if (action.separator) continue;
      QVERIFY2(!action.id.isEmpty(), qPrintable(action.text));
      ids.append(action.id);
      if (!action.enabled) {
        QVERIFY2(!action.toolTip.trimmed().isEmpty(), qPrintable(action.id));
        QVERIFY2(action.toolTip != action.text, qPrintable(action.id));
      }
    }
    // Common positions are intentional: the user requested the earlier shared menu.
    QCOMPARE(ids.first(), firstAction);
    QCOMPARE(ids.last(), QStringLiteral("layer.remove"));
    int destructiveStart = menu.actions.size() - 1;
    if (ids.contains(QStringLiteral("layer.clear"))) {
      QVERIFY(domain);
      QCOMPARE(menu.actions.at(destructiveStart - 1).id, QStringLiteral("layer.clear"));
      --destructiveStart;
    }
    QVERIFY(destructiveStart > 0);
    QVERIFY(menu.actions.at(destructiveStart - 1).separator);
    for (const QString& common : {QStringLiteral("layer.import"), QStringLiteral("layer.rename"),
        QStringLiteral("layer.style"), QStringLiteral("layer.attributes"), QStringLiteral("layer.labels"),
        QStringLiteral("layer.opacity"), QStringLiteral("layer.zoom"), QStringLiteral("layer.fullExtent")})
      QVERIFY2(ids.contains(common), qPrintable(common));
    if (!domain) QVERIFY(!ids.contains(QStringLiteral("layer.clear")));
    // [pkg E1] F019: the shape group is reachable again (no dead menu rows).
    if (domain) {
      QVERIFY(ids.contains(QStringLiteral("layer.draw")));
      QVERIFY(ids.contains(QStringLiteral("layer.export")));
      if (kind != QLatin1String("control_points")) QVERIFY(ids.contains(QStringLiteral("layer.vertices")));
      if (kind == QLatin1String("survey_area") || kind == QLatin1String("feature_poly") ||
          kind == QLatin1String("trial_trench"))
        QVERIFY(ids.contains(QStringLiteral("layer.area")));
    } else if (!qobject_cast<QgsVectorLayer*>(layer)) {
      QVERIFY(!ids.contains(QStringLiteral("layer.draw")) && !ids.contains(QStringLiteral("layer.area")));
    }
    if (auto* vector = qobject_cast<QgsVectorLayer*>(layer)) {
      // Removal keeps the live layer and edit buffer in undo history.
      QVERIFY(vector->startEditing());
      QgsFeature extra = vector->getFeature(*vector->allFeatureIds().constBegin());
      extra.setId(FID_NULL);
      QVERIFY(vector->addFeature(extra));
      QVERIFY(vector->isModified());
      const LayerMenuState dirtyMenu = inspectLayerMenu(window, tree, tree->visualRect(index).center());
      QVERIFY(dirtyMenu.seen);
      bool removeFound = false;
      for (const auto& action : dirtyMenu.actions) {
        if (action.id != QLatin1String("layer.remove")) continue;
        removeFound = true;
        QVERIFY(action.enabled);
        QVERIFY(!action.toolTip.trimmed().isEmpty());
        QVERIFY(action.toolTip != action.text);
      }
      QVERIFY(removeFound);
      QVERIFY(vector->rollBack());
    }
  }
  void changingLabelFontKeepsEveryLayerAndItsVisibility() {
    const QString path = makeSurvey(QStringLiteral("글자 크기 검증"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(openSettled(window, path));
    auto* project = QgsProject::instance();
    auto* layer = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    auto* canvas = window.findChild<QgsMapCanvas*>();
    QVERIFY(layer && tree && canvas);
    QVERIFY(LayerOps::applyAreaM2Labels(layer));
    const QString areaExpression = layer->labeling()->settings().fieldName;
    const auto ids = project->mapLayers().keys();
    QMap<QString, bool> visibility;
    for (auto* node : project->layerTreeRoot()->findLayers()) visibility[node->layerId()] = node->itemVisibilityChecked();
    window.resize(1800, 1000);
    window.show();
    tree->expandAll();
    tree->setCurrentLayer(layer);
    canvas->setExtent(QgsRectangle(189980, 559980, 190130, 560130));
    QSignalSpy rendered(canvas, &QgsMapCanvas::mapCanvasRefreshed);
    canvas->setRenderFlag(true);
    canvas->refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!rendered.isEmpty(), 15000);
    const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    if (!output.isEmpty()) QVERIFY(window.grab().save(QDir(output).filePath(QStringLiteral("map-before-font.png"))));
    bool changed = false;
    bool areaChecked = false;
    QTimer::singleShot(0, &window, [&]() {
      auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
      if (!menu) return;
      auto* areaAction = menu->findChild<QAction*>(QStringLiteral("layer.labelArea"));
      areaChecked = areaAction && areaAction->isChecked();
      auto* sizeAction = menu->findChild<QAction*>(QStringLiteral("layer.labelSize"));
      auto* sizes = sizeAction ? sizeAction->menu() : nullptr;
      if (sizes) {
        for (auto* action : sizes->actions()) if (action->text() == QLatin1String("12 pt")) {
          action->trigger(); changed = true; break;
        }
      }
      menu->close();
    });
    const QModelIndex index = tree->node2index(project->layerTreeRoot()->findLayer(layer));
    window.showLayerTreeContextMenu(tree, tree->visualRect(index).center());
    QVERIFY(changed);
    QVERIFY(areaChecked);
    QCOMPARE(LayerOps::labelFontSize(layer), 12.0);
    rendered.clear(); canvas->refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!rendered.isEmpty(), 15000);
    QCOMPARE(project->mapLayers().keys(), ids);
    for (auto it = visibility.cbegin(); it != visibility.cend(); ++it) {
      auto* node = project->layerTreeRoot()->findLayer(it.key());
      QVERIFY(node);
      QCOMPARE(node->itemVisibilityChecked(), it.value());
    }
    if (!output.isEmpty()) QVERIFY(window.grab().save(QDir(output).filePath(QStringLiteral("map-after-font.png"))));
    QCOMPARE(layer->labeling()->settings().fieldName, areaExpression);
    QVERIFY(LayerOps::labelShowArea(layer));
    canvas->setRenderFlag(false);
  }

  void trenchPresetRespectsAreaAndDimensions_data() {
    QTest::addColumn<bool>("changedCanvasCrs");
    QTest::newRow("work-5187") << false;
    QTest::newRow("canvas-changed-to-5186") << true;
  }
  void trenchPresetRespectsAreaAndDimensions() {
    QFETCH(bool, changedCanvasCrs);
    const QString path = makeSurvey(QStringLiteral("trench_limits_%1").arg(QString::fromLatin1(QTest::currentDataTag())));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    auto* project = QgsProject::instance();
    auto* area = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    auto* canvas = window.findChild<QgsMapCanvas*>();
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    QVERIFY(area && canvas && tree);
    QgsGeometry boundary = QgsGeometry::fromRect(QgsRectangle(190000, 560000, 190025, 560115));
    QVERIFY(area->startEditing());
    QVERIFY(area->changeGeometry(*area->allFeatureIds().constBegin(), boundary));
    QVERIFY(area->commitChanges());
    area->setRenderer(new QgsSingleSymbolRenderer(QgsFillSymbol::createSimple(
        {{QStringLiteral("color"), QStringLiteral("255,255,255,0")},
         {QStringLiteral("outline_color"), QStringLiteral("194,93,20,255")},
         {QStringLiteral("outline_width"), QStringLiteral("0.6")}}).release()));
    tree->setCurrentLayer(area);
    window.resize(1280, 860);
    window.show();
    QCoreApplication::processEvents();
    if (changedCanvasCrs)
      QVERIFY(QMetaObject::invokeMethod(&window, "setWorkCrs5186", Qt::DirectConnection));
    QVERIFY(QMetaObject::invokeMethod(&window, "applyTrenchByRatio", Qt::DirectConnection, Q_ARG(double, 10.)));
    auto* trenches = LayerOps::findByLayerKey(project, QStringLiteral("trial_trench"));
    QVERIFY(trenches && trenches->featureCount() > 0);
    const qint64 firstCount = trenches->featureCount();
    // Loading/generating schedules renders. Finish their cancellation before
    // measuring the frame requested below, not the old survey-opening frame.
    canvas->setRenderFlag(false);
    canvas->stopRendering();
    QCoreApplication::processEvents();
    QTRY_VERIFY_WITH_TIMEOUT(!canvas->isDrawing(), 15000);
    QgsCoordinateTransform toCanvas(area->crs(), canvas->mapSettings().destinationCrs(), project);
    const QgsRectangle viewBounds = toCanvas.transformBoundingBox(QgsRectangle(189935, 559990, 190090, 560125));
    canvas->setExtent(viewBounds);
    canvas->setLayers({trenches, area});
    QSignalSpy rendered(canvas, &QgsMapCanvas::mapCanvasRefreshed);
    canvas->setRenderFlag(true);
    canvas->refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!rendered.isEmpty(), 15000);
    QTRY_VERIFY_WITH_TIMEOUT(!canvas->isDrawing(), 15000);
    QVERIFY(canvas->extent().contains(toCanvas.transformBoundingBox(boundary.boundingBox())));
    QCOMPARE(canvas->mapSettings().destinationCrs().authid(), changedCanvasCrs ? QStringLiteral("EPSG:5186") : QStringLiteral("EPSG:5187"));
    QCOMPARE(trenches->crs().authid(), area->crs().authid());
    const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    if (!output.isEmpty()) QVERIFY(window.grab().save(QDir(output).filePath(changedCanvasCrs
        ? QStringLiteral("trench-preset-canvas5186.png") : QStringLiteral("trench-preset.png"))));
    canvas->setRenderFlag(false);
    double total = 0.;
    QgsFeature feature;
    auto iterator = trenches->getFeatures();
    while (iterator.nextFeature(feature)) {
      const QgsGeometry geometry = feature.geometry();
      total += geometry.area();
      QgsExpressionContext labelContext;
      labelContext.setFields(trenches->fields());
      labelContext.setFeature(feature);
      QgsExpression labelExpression(trenches->labeling()->settings().fieldName);
      const QString label = labelExpression.evaluate(&labelContext).toString();
      QVERIFY(label.contains(feature.attribute(QStringLiteral("name")).toString()));
      QVERIFY(geometry.difference(boundary).area() < 1e-6);
      const auto ring = geometry.asPolygon().constFirst();
      QVERIFY(ring.size() >= 5);
      QVERIFY(ring[0].distance(ring[1]) <= 2. + 1e-7);
      QVERIFY(ring[1].distance(ring[2]) <= 20. + 1e-7);
    }
    QVERIFY2(qAbs(total - boundary.area() * .1) < 1e-4, qPrintable(QString::number(total, 'f', 6)));
    tree->setCurrentLayer(area);
    QVERIFY(QMetaObject::invokeMethod(&window, "applyTrenchByRatio", Qt::DirectConnection, Q_ARG(double, 10.)));
    QCOMPARE(trenches->featureCount(), firstCount);
    QVERIFY(trenches->isEditable() || trenches->startEditing());
    const auto fid = *trenches->allFeatureIds().constBegin();
    QVERIFY(trenches->changeAttributeValue(fid, trenches->fields().indexOf(QStringLiteral("name")), QStringLiteral("작성 중")));
    tree->setCurrentLayer(area);
    QVERIFY(QMetaObject::invokeMethod(&window, "applyTrenchByRatio", Qt::DirectConnection, Q_ARG(double, 10.)));
    QVERIFY(trenches->isModified());
    QCOMPARE(trenches->getFeature(fid).attribute(QStringLiteral("name")).toString(), QStringLiteral("작성 중"));
    auto* messages = window.findChild<QgsMessageBar*>();
    QVERIFY(messages && messages->currentItem());
    QCOMPARE(messages->currentItem()->level(), Qgis::MessageLevel::Warning);
    QVERIFY(messages->currentItem()->text().contains(QStringLiteral("저장하지 않은 편집")));
    QVERIFY(trenches->rollBack());
  }

  void layerDeleteKeyPreservesSourceAndUndoRestoresPendingEdits() {
    const QString path = makeSurvey(QStringLiteral("delete_key_undo"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    auto* project = QgsProject::instance();
    auto* layer = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QVERIFY(tree && layer);
    const QString id = layer->id();
    QVERIFY(layer->startEditing());
    const auto ids = layer->allFeatureIds();
    const QgsFeatureId fid = *ids.constBegin();
    const int field = layer->fields().indexOf(QStringLiteral("survey_name"));
    QVERIFY(layer->changeAttributeValue(fid, field, QStringLiteral("저장 전 기록")));
    window.show();
    QApplication::setActiveWindow(&window);
    tree->setCurrentLayer(layer);
    tree->setFocus();
    QApplication::processEvents();
    const QByteArray original = contents(path);
    QTest::keyClick(tree, Qt::Key_Delete);
    QVERIFY(!project->mapLayer(id));
    QCOMPARE(contents(path), original);
    QTest::keyClick(tree, Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(project->mapLayer(id), layer);
    QVERIFY(project->layerTreeRoot()->findLayer(id));
    QVERIFY(layer->isModified());
    QCOMPARE(layer->getFeature(fid).attribute(field).toString(), QStringLiteral("저장 전 기록"));
    QCOMPARE(contents(path), original);
    QVERIFY(layer->rollBack());
  }

  void layerDeleteKeyRemovesReferenceFromMap() {
    const QString path = makeSurvey(QStringLiteral("delete_key_ref"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    auto* canvas = window.findChild<QgsMapCanvas*>();
    auto* project = QgsProject::instance();
    auto* reference = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5186&field=note:string"),
                                         QStringLiteral("현장참고점"), QStringLiteral("memory"));
    QVERIFY(tree && canvas && reference && reference->isValid());
    LayerOps::markReferenceLayer(reference);
    project->addMapLayer(reference);
    LayerOps::placeInLegendGroup(project, reference, QString::fromUtf8(LayerOps::kGroupReference));
    const QString id = reference->id();
    window.show();
    QApplication::setActiveWindow(&window);
    tree->setCurrentLayer(reference);
    canvas->setFocus();
    QApplication::processEvents();
    QTest::keyClick(canvas, Qt::Key_Delete);
    QVERIFY2(!project->mapLayer(id), "지도에서 Delete 를 눌러도 참조 지도가 남아 있습니다.");
    QVERIFY2(!project->layerTreeRoot()->findLayer(id), "삭제한 레이어 이름이 레이어 창에 남아 있습니다.");
    QTest::keyClick(&window, Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(project->mapLayer(id));
    QVERIFY(project->layerTreeRoot()->findLayer(id));
  }

  void layerDeleteKeyRemovesCadastralAndKeepsItGone() {
    const QString path = makeSurvey(QStringLiteral("delete_cad_layer"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    auto* project = QgsProject::instance();
    auto* cad = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                   QStringLiteral("지적도 · 조사 주변 5km"), QStringLiteral("memory"));
    QVERIFY(tree && cad && cad->isValid());
    LayerOps::markCadastralLayer(cad);
    project->addMapLayer(cad, false);
    LayerOps::placeCadastralLayer(project, cad);
    const QString id = cad->id();
    window.show();
    QApplication::setActiveWindow(&window);
    tree->setCurrentLayer(cad);
    tree->setFocus();
    QApplication::processEvents();
    QTest::keyClick(tree, Qt::Key_Delete);
    QVERIFY2(!project->mapLayer(id), "범례에서 Delete 를 눌러도 지적도가 남아 있습니다.");
    QVERIFY2(!LayerOps::userRemovedCadastral(project),
             "받은 지적도를 지워도 바탕 지적 그림까지 끈 것으로 기록하면 안 됩니다.");
  }

  void cadastralGroupDeleteKeyRemovesChildren() {
    const QString path = makeSurvey(QStringLiteral("delete_cad_group"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    auto* project = QgsProject::instance();
    auto* cad = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                   QStringLiteral("지적도 · 조사 주변 5km"), QStringLiteral("memory"));
    QVERIFY(tree && cad && cad->isValid());
    LayerOps::markCadastralLayer(cad);
    project->addMapLayer(cad, false);
    LayerOps::placeCadastralLayer(project, cad);
    const QString id = cad->id();
    auto* other = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                     QStringLiteral("VWorld 지적(본번·부번)"), QStringLiteral("memory"));
    QVERIFY(other->isValid());
    project->addMapLayer(other, false);
    project->layerTreeRoot()->insertLayer(0, other);
    const QString otherId = other->id();
    QVERIFY(!project->layerTreeRoot()->findGroup(QString::fromUtf8(LayerOps::kGroupCadastral)));
    window.show();
    QApplication::setActiveWindow(&window);
    tree->setCurrentLayer(cad);
    tree->setFocus();
    QApplication::processEvents();
    QTest::keyClick(tree, Qt::Key_Delete);
    QVERIFY2(!project->mapLayer(id), "선택한 지적도를 지우지 못했습니다.");
    QVERIFY2(project->mapLayer(otherId), "다른 지적 레이어가 같이 지워졌습니다.");
    QTest::keyClick(tree, Qt::Key_Z, Qt::ControlModifier);
    QVERIFY2(project->mapLayer(id), "Ctrl+Z 로 지운 지적도를 되살리지 못했습니다.");
    QVERIFY(project->mapLayer(otherId));
  }

  // A shape just drawn shows its record in the right panel at once (user 2026-10-03 「속성도 안나오고」).
  void drawnShapeShowsItsRecordInTheInspector() {
    const QString path = makeSurvey(QStringLiteral("drawn_record"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    QVERIFY(QMetaObject::invokeMethod(&window, "startEditFeaturePoly", Qt::DirectConnection));
    auto* capture = window.findChild<KaCaptureMapTool*>();
    auto* poly = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("feature_poly"));
    auto* card = window.findChild<KaFeatureCard*>();
    QVERIFY(capture && poly && card);
    captureAndDismissForm(capture, QgsGeometry::fromRect(QgsRectangle(190070, 560010, 190090, 560030)));
    QCOMPARE(poly->featureCount(), 1);
    QVERIFY2(card->hasFeature() && card->layer() == poly, "그린 도형의 기록이 오른쪽 속성에 나와야 합니다.");
    QVERIFY2(card->isEnabled(), "그리는 중에도 기록을 고칠 수 있어야 합니다(2026-10-03 「진행」).");
    // Shown, not selected: 겹친 곳 지우기·폴리곤 나누기·Delete must not pick it up (code review).
    QVERIFY(poly->selectedFeatureIds().isEmpty());
    QVERIFY(QMetaObject::invokeMethod(&window, "startSelectTool", Qt::DirectConnection));
    QVERIFY2(card->hasFeature(), "그리기를 내려놓아도 방금 그린 기록이 남아야 합니다.");
  }
  // Keys typed in a card box while drawing leave the sketch alone (user 2026-10-03 「진행」, code review).
  void cardKeysLeaveTheSketchAlone() {
    const QString path = makeSurvey(QStringLiteral("card_keys"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    QVERIFY(QMetaObject::invokeMethod(&window, "startEditFeaturePoly", Qt::DirectConnection));
    auto* capture = window.findChild<KaCaptureMapTool*>();
    auto* poly = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("feature_poly"));
    auto* card = window.findChild<KaFeatureCard*>();
    QVERIFY(capture && poly && card);
    captureAndDismissForm(capture, QgsGeometry::fromRect(QgsRectangle(190070, 560010, 190090, 560030)));
    auto* canvas = window.findChild<QgsMapCanvas*>();
    window.resize(1280, 900);
    window.show();
    canvas->setExtent(QgsRectangle(189950, 559950, 190150, 560150));
    QApplication::processEvents();
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(300, 300));
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(360, 300));
    const int sketched = capture->pointCount();
    QVERIFY(sketched >= 2);
    auto* box = card->findChild<QLineEdit*>(QStringLiteral("kaFeatureCardNumber"));
    QVERIFY(box && box->isEnabled());
    box->setFocus();
    for (Qt::Key key : {Qt::Key_Return, Qt::Key_Escape, Qt::Key_Backspace}) QTest::keyClick(box, key);
    QCOMPARE(capture->pointCount(), sketched);
    QCOMPARE(poly->featureCount(), 1);
  }
  // An area that is already there is continued without a window (user 2026-10-03 「추천진행」).
  void surveyAreaDrawingContinuesTheExistingAreaWithoutAWindow() {
    const QString path = makeSurvey(QStringLiteral("survey_area_default"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    const QList<QgsVectorLayer*> areas = LayerOps::surveyAreaLayers(QgsProject::instance());
    QVERIFY(!areas.isEmpty());
    // 다시 연 조사의 조사구역도 오른쪽 속성에 기록이 나온다(2026-10-10 「속성정보를 찍어도 나오지않는다」).
    auto* card = window.findChild<KaFeatureCard*>();
    QVERIFY(card && QMetaObject::invokeMethod(&window, "startSelectTool", Qt::DirectConnection));
    areas.first()->selectByIds({*areas.first()->allFeatureIds().constBegin()});
    QVERIFY2(card->hasFeature() && card->layer() == areas.first(), "도형선택으로 고른 조사구역의 기록이 나와야 합니다.");
    areas.first()->removeSelection();
    bool windowShown = false;
    QTimer::singleShot(300, &window, [&] {
      if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) windowShown = dialog->close();
    });
    QVERIFY(QMetaObject::invokeMethod(&window, "startEditSurveyArea", Qt::DirectConnection));
    QTest::qWait(400);
    QVERIFY2(!windowShown && areas.first()->isEditable(), "조사구역이 있으면 창 없이 이어 그려야 합니다.");
    QVERIFY2(card->hasFeature() && card->layer() == areas.first(), "이어 그리는 조사구역의 기록이 오른쪽 속성에 나와야 합니다.");
  }

  void cadastralGroupContextMenuCanRemove() {
    const QString path = makeSurvey(QStringLiteral("menu_cad_group"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    auto* project = QgsProject::instance();
    auto* cad = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                   QStringLiteral("지적도 · 조사 주변 5km"), QStringLiteral("memory"));
    QVERIFY(tree && cad && cad->isValid());
    LayerOps::markCadastralLayer(cad);
    project->addMapLayer(cad, false);
    LayerOps::placeCadastralLayer(project, cad);
    auto* node = project->layerTreeRoot()->findLayer(cad->id());
    QVERIFY(node);
    window.show();
    tree->expandAll();
    const QModelIndex index = tree->node2index(node);
    QVERIFY(index.isValid());
    tree->scrollTo(index);
    const LayerMenuState menu = inspectLayerMenu(window, tree, tree->visualRect(index).center());
    QVERIFY(menu.seen);
    QStringList ids;
    for (const auto& action : menu.actions) {
      if (!action.separator) ids.append(action.id);
    }
    QVERIFY2(ids.contains(QStringLiteral("layer.remove")), "지적도 묶음 우클릭에 삭제가 없습니다.");
  }

  void referenceGroupMenuRemovesEveryRowAndCtrlZRestores() {
    const QString path = makeSurvey(QStringLiteral("delete_ref_group"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    auto* project = QgsProject::instance();
    QVERIFY(tree);
    auto* root = project->layerTreeRoot();
    auto* refs = root->findGroup(QString::fromUtf8(LayerOps::kGroupReference));
    if (!refs) refs = root->addGroup(QString::fromUtf8(LayerOps::kGroupReference));
    auto* inner = refs->addGroup(QStringLiteral("하위 참고 묶음"));
    refs->addGroup(QStringLiteral("빈 하위 묶음"));
    auto* area = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                    QStringLiteral("참고 면"), QStringLiteral("memory"));
    auto* points = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5186"),
                                      QStringLiteral("참고 점"), QStringLiteral("memory"));
    auto* outside = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                       QStringLiteral("묶음 밖 참고"), QStringLiteral("memory"));
    QVERIFY(area->isValid() && points->isValid() && outside->isValid());
    for (auto* layer : {area, points, outside}) {
      LayerOps::markReferenceLayer(layer);
      project->addMapLayer(layer, false);
    }
    refs->addLayer(area);
    inner->addLayer(points);
    root->insertLayer(0, outside);
    const QString areaId = area->id();
    const QString pointsId = points->id();
    const QString outsideId = outside->id();
    const int refsIndex = root->children().indexOf(refs);
    window.show();
    QApplication::setActiveWindow(&window);
    tree->expandAll();
    // The panel shows the tree through a proxy model: use the view's own index for the row.
    const QModelIndex index = tree->node2index(refs);
    QVERIFY(index.isValid());
    tree->scrollTo(index);
    // A real right-click focuses the layer panel first; delete ignores keys typed into a text field.
    tree->setFocus();
    QApplication::processEvents();
    const LayerMenuState menu = inspectLayerMenu(window, tree, tree->visualRect(index).center(),
                                                 QStringLiteral("layer.remove"));
    QVERIFY(menu.seen);
    bool offered = false;
    bool groupMenu = false;
    for (const auto& action : menu.actions) {
      offered |= action.id == QLatin1String("layer.remove") && action.enabled &&
                 action.toolTip.contains(QStringLiteral("묶음"));
      groupMenu |= action.id == QLatin1String("layer.import");
    }
    QVERIFY2(groupMenu, "참조 지도 묶음 줄이 아닌 다른 줄의 메뉴가 열렸습니다.");
    QVERIFY2(offered, "참조 지도 묶음 우클릭에 레이어 삭제가 없습니다.");
    QVERIFY2(!project->mapLayer(areaId) && !project->mapLayer(pointsId),
             "묶음을 지워도 안의 레이어가 지도에 남아 있습니다.");
    QVERIFY2(!root->findGroup(QString::fromUtf8(LayerOps::kGroupReference)),
             "묶음을 지워도 참조 지도 줄이 레이어 창에 남아 있습니다.");
    QVERIFY2(project->mapLayer(outsideId), "묶음 밖의 레이어까지 지워졌습니다.");

    // The offscreen platform does not hand activation back after the popup closes; a desktop does.
    QApplication::setActiveWindow(&window);
    tree->setFocus();
    QApplication::processEvents();
    QTest::keyClick(tree, Qt::Key_Z, Qt::ControlModifier);
    auto* restored = root->findGroup(QString::fromUtf8(LayerOps::kGroupReference));
    QVERIFY2(restored, "Ctrl+Z 로 참조 지도 묶음이 돌아오지 않았습니다.");
    QCOMPARE(root->children().indexOf(restored), refsIndex);
    QVERIFY2(restored->findLayer(areaId), "Ctrl+Z 로 되살린 레이어가 참조 지도 묶음 안에 있지 않습니다.");
    auto* restoredInner = restored->findGroup(QStringLiteral("하위 참고 묶음"));
    QVERIFY2(restoredInner && restoredInner->findLayer(pointsId),
             "하위 묶음과 그 안의 레이어가 제자리로 돌아오지 않았습니다.");
    // A sub-group that held no layers at all drops out while QGIS detaches the group; empty
    // title rows are not kept elsewhere either, so it is not expected back.
    QCOMPARE(restored->children().size(), 2);
    QVERIFY(project->mapLayer(areaId) && project->mapLayer(pointsId) && project->mapLayer(outsideId));
  }

  void emptyGroupDeleteKeyRemovesRowAndCtrlZRestores() {
    const QString path = makeSurvey(QStringLiteral("delete_empty_group"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(openSettled(window, path));
    disableRendering(window);
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    QVERIFY(tree);
    auto* root = QgsProject::instance()->layerTreeRoot();
    const QString name = QStringLiteral("빈 묶음 시험");
    auto* empty = root->insertGroup(0, name);
    window.show();
    QApplication::setActiveWindow(&window);
    const QModelIndex index = tree->node2index(empty);
    QVERIFY(index.isValid());
    tree->setCurrentIndex(index);
    tree->setFocus();
    QApplication::processEvents();
    QTest::keyClick(tree, Qt::Key_Delete);
    QVERIFY2(!root->findGroup(name), "레이어가 없는 묶음 줄이 Delete 로 지워지지 않았습니다.");
    QTest::keyClick(tree, Qt::Key_Z, Qt::ControlModifier);
    auto* back = root->findGroup(name);
    QVERIFY2(back, "Ctrl+Z 로 빈 묶음이 돌아오지 않았습니다.");
    QCOMPARE(root->children().indexOf(back), 0);
  }

  void ctrlZRestoresVertexEditsAndGroupedFeatureDeletion() {
    const QString path = makeSurvey(QStringLiteral("vertex_undo"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(openSettled(window, path));
    disableRendering(window);
    auto* layer = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"));
    auto* canvas = window.findChild<QgsMapCanvas*>();
    QVERIFY(layer && canvas);
    const auto ids = layer->allFeatureIds();
    const QgsFeatureId fid = *ids.constBegin();
    const QgsGeometry original = layer->getFeature(fid).geometry();
    QVERIFY(QMetaObject::invokeMethod(&window, "startSelectTool", Qt::DirectConnection));
    auto* select = window.findChild<KaFeatureSelectTool*>();
    auto* vertex = select ? select->findChild<KaVertexEditTool*>() : nullptr;
    QVERIFY(vertex);
    window.show();
    QApplication::setActiveWindow(&window);
    canvas->setFocus();
    QApplication::processEvents();
    vertex->setTarget(layer, fid);
    QVERIFY(vertex->moveVertexTo(0, QgsPointXY(190020, 560020)));
    QVERIFY(!layer->getFeature(fid).geometry().equals(original));
    QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(layer->getFeature(fid).geometry().equals(original));
    vertex->setTarget(layer, fid);
    QVERIFY(vertex->insertVertexAt(1, QgsPointXY(190040, 560000)));
    QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(layer->getFeature(fid).geometry().equals(original));
    vertex->setTarget(layer, fid);
    QVERIFY(vertex->deleteVertexAt(1));
    QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(layer->getFeature(fid).geometry().equals(original));
    if (!layer->isEditable()) QVERIFY(layer->startEditing());
    QgsFeature second(layer->fields());
    second.setGeometry(QgsGeometry::fromRect(QgsRectangle(190200, 560000, 190250, 560050)));
    QVERIFY(layer->addFeature(second));
    QVERIFY(layer->commitChanges());
    layer->selectAll();
    canvas->setLayers({layer});
    QTest::keyClick(canvas, Qt::Key_Delete);
    QCOMPARE(layer->featureCount(), 0);
    QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(layer->featureCount(), 2);
    QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(layer->featureCount(), 2); // no duplicated fallback history
    // Restoring a deletion into a dirty edit buffer creates a temporary FID.
    // Saving must remap the earlier geometry command to the committed FID.
    const auto restoredIds = layer->allFeatureIds();
    const auto editedId = *restoredIds.constBegin();
    const auto previous = layer->getFeature(editedId).geometry();
    if (!layer->isEditable()) QVERIFY(layer->startEditing());
    const int nameField = layer->fields().indexOf(QStringLiteral("survey_name"));
    QVERIFY(layer->changeAttributeValue(editedId, nameField, QStringLiteral("미저장 기록")));
    vertex->setTarget(layer, editedId);
    QVERIFY(vertex->moveVertexTo(0, QgsPointXY(190025, 560025)));
    layer->selectByIds({editedId});
    QTest::keyClick(canvas, Qt::Key_Delete);
    QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(saveNow(window));
    QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
    bool previousFound = false;
    QgsFeature restored;
    auto iterator = layer->getFeatures();
    while (iterator.nextFeature(restored)) previousFound |= restored.geometry().equals(previous);
    QVERIFY(previousFound);
  }

  // 「그린 도형 모두 지우기…」는 확인 창에 적은 레이어만 비운다. 선택 도구를 내려놓아도
  // 선택은 남으므로, 다른 레이어에서 골라 둔 도형이 함께 지워지면 안 된다.
  void clearDrawnFeaturesLeavesOtherLayersSelectionAlone() {
    const QString path = makeSurvey(QStringLiteral("clear_drawn_scope"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    auto* project = QgsProject::instance();
    auto* area = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QString error;
    auto* poly = LayerOps::ensureDomainLayer(project, path, QStringLiteral("feature_poly"),
                                             QStringLiteral("유구 면"), &error);
    QVERIFY2(area && poly && area != poly, qPrintable(error));
    QVERIFY(poly->startEditing());
    QgsFeature drawn(poly->fields());
    drawn.setGeometry(QgsGeometry::fromRect(QgsRectangle(190020, 560020, 190040, 560040)));
    QVERIFY(poly->addFeature(drawn));
    QVERIFY(poly->commitChanges());
    QCOMPARE(area->featureCount(), 1);
    QCOMPARE(poly->featureCount(), 1);
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    auto* canvas = window.findChild<QgsMapCanvas*>();
    QVERIFY(tree && canvas);
    window.show();
    QApplication::setActiveWindow(&window);
    poly->selectAll();
    const QgsFeatureIds polySelection = poly->selectedFeatureIds();
    QCOMPARE(polySelection.size(), 1);
    tree->setCurrentLayer(area);
    QApplication::processEvents();
    QCOMPARE(tree->currentLayer(), area);
    QCOMPARE(poly->selectedFeatureIds(), polySelection);

    QString asked;
    QTimer answer;
    connect(&answer, &QTimer::timeout, [&] {
      if (auto* question = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
        asked = question->text();
        question->button(QMessageBox::Yes)->click();
      }
    });
    answer.start(20);
    QVERIFY(QMetaObject::invokeMethod(&window, "clearDrawnFeaturesOfCurrentLayer", Qt::DirectConnection));
    answer.stop();
    QVERIFY2(asked.contains(area->name()) && asked.contains(QStringLiteral("도형 1개")), qPrintable(asked));
    QCOMPARE(area->featureCount(), 0);
    QVERIFY2(poly->featureCount() == 1, "확인 창에 적지 않은 다른 레이어의 도형까지 지워졌습니다.");
    QCOMPARE(poly->selectedFeatureIds(), polySelection);

    QApplication::setActiveWindow(&window);
    canvas->setFocus();
    QApplication::processEvents();
    QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
    QVERIFY2(area->featureCount() == 1, "Ctrl+Z 한 번으로 비운 레이어가 돌아오지 않았습니다.");
    QCOMPARE(poly->featureCount(), 1);
  }

  // 「겹친 곳 지우기」: 위에 그린 도형을 고르고 누르면 아래 도형에서 그 자리만 지워지고 위 도형은
  // 없어진다. 그려 놓고 저장하지 않은 도형으로도 되어야 하고, Ctrl+Z 한 번에 둘 다 돌아와야 한다.
  void eraseOverlap_cutsTheShapeUnderTheSelectedOneAndCtrlZRestores() {
    const QString path = makeSurvey(QStringLiteral("erase_same_layer"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    auto* area = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"));
    auto* canvas = window.findChild<QgsMapCanvas*>();
    QVERIFY(area && canvas);
    const QgsFeatureId outer = *area->allFeatureIds().constBegin();
    QVERIFY(area->isEditable() || area->startEditing());
    QgsFeature inner(area->fields());
    inner.setGeometry(QgsGeometry::fromRect(QgsRectangle(190040, 560040, 190060, 560060)));
    QVERIFY(area->addFeature(inner));
    QVERIFY2(inner.id() < 0, "저장하지 않은 도형은 음수 번호를 가진다.");
    window.resize(1920, 1040);
    window.show();
    QApplication::setActiveWindow(&window);
    canvas->setFocus();
    QApplication::processEvents();

    // 그리기 도구 줄에 단추가 있고, 1920 폭에서 접혀 사라지지 않는다.
    auto* draw = window.findChild<QToolButton*>(QStringLiteral("btnDraw"));
    auto* sub = window.findChild<QToolBar*>(QStringLiteral("subToolbar"));
    QVERIFY(draw && sub);
    QTRY_VERIFY(draw->isEnabled());
    draw->click();
    QTRY_VERIFY(sub->isVisible());
    QAction* erase = nullptr;
    QAction* close = nullptr;
    for (QAction* action : sub->actions()) {
      if (action->objectName() == QLatin1String("actEraseOverlap")) erase = action;
      if (action->text() == QStringLiteral("닫기")) close = action;
    }
    QVERIFY2(erase, "그리기 도구 줄에 「겹친 곳 지우기」가 없습니다.");
    QCOMPARE(erase->text(), QStringLiteral("겹친 곳 지우기"));
    QVERIFY(close);
    QApplication::processEvents();
    QVERIFY2(sub->widgetForAction(erase) && sub->widgetForAction(erase)->isVisible() &&
                 sub->widgetForAction(close) && sub->widgetForAction(close)->isVisible(),
             qPrintable(QStringLiteral("1920 폭에서 그리기 도구 줄의 단추가 접혔습니다 (줄 %1, 필요 %2)")
                            .arg(sub->width())
                            .arg(sub->sizeHint().width())));

    // KA_HGIS_QA_OUTPUT_DIR 을 주면 지우기 전후 화면을 그림으로 남긴다(눈으로 확인할 때 쓴다).
    const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    const auto shot = [&](const QString& name) {
      if (output.isEmpty()) return true;
      if (!QDir().mkpath(output)) return false;
      canvas->setExtent(QgsRectangle(189940, 559940, 190160, 560160));
      // 다시 그리기는 타이머로 시작한다. 시작하기도 전에 찍으면 앞 장면이 찍히므로,
      // 그리기가 끝났다는 신호를 기다린 뒤 더 그릴 것이 없을 때까지 본다.
      QSignalSpy refreshed(canvas, &QgsMapCanvas::mapCanvasRefreshed);
      canvas->setRenderFlag(true);
      canvas->refresh();
      if (refreshed.isEmpty() && !refreshed.wait(20000)) return false;
      QElapsedTimer quiet;
      quiet.start();
      while (quiet.elapsed() < 500) {
        QTest::qWait(50);
        if (canvas->isDrawing()) quiet.restart();
      }
      const bool saved = window.grab().save(QDir(output).filePath(name));
      canvas->setRenderFlag(false);
      return saved;
    };

    const auto areaOf = [&](QgsFeatureId fid) { return area->getFeature(fid).geometry().area(); };
    area->selectByIds({inner.id()});
    QVERIFY(shot(QStringLiteral("erase-overlap-1-before.png")));
    erase->trigger();
    QVERIFY2(qAbs(areaOf(outer) - 9600.0) < 1e-3, "아래 도형에서 겹친 자리가 지워지지 않았습니다.");
    QVERIFY2(!area->getFeature(inner.id()).isValid(), "위에 그린 도형이 남아 있습니다.");
    QCOMPARE(area->featureCount(), 1);
    QVERIFY(shot(QStringLiteral("erase-overlap-2-after.png")));

    QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
    QVERIFY2(qAbs(areaOf(outer) - 10000.0) < 1e-3 && area->featureCount() == 2,
             "Ctrl+Z 한 번에 구멍이 메워지고 위 도형이 돌아오지 않았습니다.");
    QTest::keyClick(canvas, Qt::Key_Y, Qt::ControlModifier);
    QVERIFY(qAbs(areaOf(outer) - 9600.0) < 1e-3);
    QCOMPARE(area->featureCount(), 1);

    // 저장한 파일에도 구멍 난 도형 하나만 남는다.
    QVERIFY(saveNow(window));
    QgsVectorLayer saved(QStringLiteral("%1|layername=survey_area").arg(path), QStringLiteral("확인"),
                         QStringLiteral("ogr"));
    QVERIFY(saved.isValid());
    QCOMPARE(saved.featureCount(), 1);
    QgsFeature kept;
    QVERIFY(saved.getFeatures().nextFeature(kept));
    QVERIFY(kept.geometry().isGeosValid());
    QVERIFY2(qAbs(kept.geometry().area() - 9600.0) < 1e-3, "저장한 도형의 넓이가 다릅니다.");
  }

  // 다른 레이어에 그린 도형으로도 지운다. 레이어마다 되돌리기 기록이 따로라서, 묶지 않으면
  // Ctrl+Z 한 번에 반만 돌아온다(구멍은 메워졌는데 위 도형은 없는 식).
  void eraseOverlap_acrossLayersUndoesAndRedoesTogether() {
    const QString path = makeSurvey(QStringLiteral("erase_two_layers"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    auto* project = QgsProject::instance();
    auto* area = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QString error;
    auto* poly = LayerOps::ensureDomainLayer(project, path, QStringLiteral("feature_poly"),
                                             QStringLiteral("유구 면"), &error);
    QVERIFY2(area && poly && area != poly, qPrintable(error));
    const QgsFeatureId outer = *area->allFeatureIds().constBegin();
    QVERIFY(poly->startEditing());
    QgsFeature cutter(poly->fields());
    cutter.setGeometry(QgsGeometry::fromRect(QgsRectangle(190040, 560040, 190060, 560060)));
    QVERIFY(poly->addFeature(cutter));
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    auto* canvas = window.findChild<QgsMapCanvas*>();
    QVERIFY(tree && canvas);
    window.show();
    QApplication::setActiveWindow(&window);
    canvas->setFocus();
    tree->setCurrentLayer(poly);
    QApplication::processEvents();
    const auto cut = [&] { return qAbs(area->getFeature(outer).geometry().area() - 9600.0) < 1e-3; };
    const auto whole = [&] { return qAbs(area->getFeature(outer).geometry().area() - 10000.0) < 1e-3; };

    // 위 도형 하나만 골라도, 아래에 겹친 도형이 하나뿐이면 그 도형에서 지운다.
    poly->selectByIds({cutter.id()});
    QVERIFY(QMetaObject::invokeMethod(&window, "eraseOverlapWithShape", Qt::DirectConnection));
    QVERIFY2(cut(), "다른 레이어의 아래 도형에서 겹친 자리가 지워지지 않았습니다.");
    QCOMPARE(poly->featureCount(), 0);
    QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
    QVERIFY2(whole() && poly->featureCount() == 1, "Ctrl+Z 한 번에 두 레이어가 함께 돌아오지 않았습니다.");
    QTest::keyClick(canvas, Qt::Key_Y, Qt::ControlModifier);
    QVERIFY2(cut() && poly->featureCount() == 0, "다시 실행이 두 레이어에 함께 적용되지 않았습니다.");
    QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(whole());
    QCOMPARE(poly->featureCount(), 1);

    // 둘을 함께 고르면 큰 도형에서 작은 도형 자리를 지운다.
    area->selectByIds({outer});
    poly->selectByIds({cutter.id()});
    QVERIFY(QMetaObject::invokeMethod(&window, "eraseOverlapWithShape", Qt::DirectConnection));
    QVERIFY(cut());
    QCOMPARE(poly->featureCount(), 0);
    QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(whole());
    QCOMPARE(poly->featureCount(), 1);
  }

  // 도형 하나로 지우는 것은 바로 하지만, 여러 도형을 한꺼번에 없애는 것은 먼저 묻는다.
  // 끌어서 고르다 딸려 온 도형까지 사라질 수 있기 때문이다.
  void eraseOverlap_severalShapesAskBeforeRemovingThem() {
    const QString path = makeSurvey(QStringLiteral("erase_several"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    auto* area = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"));
    auto* canvas = window.findChild<QgsMapCanvas*>();
    QVERIFY(area && canvas);
    const QgsFeatureId outer = *area->allFeatureIds().constBegin();
    QVERIFY(area->isEditable() || area->startEditing());
    QgsFeature first(area->fields());
    first.setGeometry(QgsGeometry::fromRect(QgsRectangle(190010, 560010, 190030, 560030)));
    QgsFeature second(area->fields());
    second.setGeometry(QgsGeometry::fromRect(QgsRectangle(190060, 560060, 190080, 560080)));
    QVERIFY(area->addFeature(first) && area->addFeature(second));
    window.show();
    QApplication::setActiveWindow(&window);
    canvas->setFocus();
    QApplication::processEvents();
    const auto outerArea = [&] { return area->getFeature(outer).geometry().area(); };

    QString asked;
    bool agree = false;
    QTimer answer;
    connect(&answer, &QTimer::timeout, [&] {
      if (auto* question = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
        asked = question->text();
        question->button(agree ? QMessageBox::Yes : QMessageBox::No)->click();
      }
    });
    answer.start(20);
    area->selectByIds({outer, first.id(), second.id()});
    QVERIFY(QMetaObject::invokeMethod(&window, "eraseOverlapWithShape", Qt::DirectConnection));
    QVERIFY2(asked.contains(QStringLiteral("2개")), qPrintable(asked));
    QVERIFY2(area->featureCount() == 3 && qAbs(outerArea() - 10000.0) < 1e-3,
             "「아니요」를 눌렀는데 도형이 바뀌었습니다.");

    agree = true;
    asked.clear();
    QVERIFY(QMetaObject::invokeMethod(&window, "eraseOverlapWithShape", Qt::DirectConnection));
    answer.stop();
    QVERIFY(!asked.isEmpty());
    QCOMPARE(area->featureCount(), 1);
    QVERIFY(qAbs(outerArea() - 9200.0) < 1e-3);
    QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(area->featureCount(), 3);
    QVERIFY(qAbs(outerArea() - 10000.0) < 1e-3);
  }

  // 그린 직후에는 고르지 않고 눌러도 된다. 첫 번째 누름은 방금 그린 도형을 골라 보여 주기만 하고,
  // 두 번째 누름에 지운다.
  void eraseOverlap_secondPressUsesTheShapeJustDrawn() {
    const QString path = makeSurvey(QStringLiteral("erase_just_drawn"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    auto* project = QgsProject::instance();
    auto* area = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QVERIFY(area);
    // 아무것도 그리지도 고르지도 않았으면 알려 주기만 한다.
    QVERIFY(QMetaObject::invokeMethod(&window, "eraseOverlapWithShape", Qt::DirectConnection));
    QCOMPARE(area->featureCount(), 1);

    QVERIFY(QMetaObject::invokeMethod(&window, "startEditFeaturePoly", Qt::DirectConnection));
    auto* poly = LayerOps::findByLayerKey(project, QStringLiteral("feature_poly"));
    auto* capture = window.findChild<KaCaptureMapTool*>();
    QVERIFY(poly && capture);
    captureAndDismissForm(capture, QgsGeometry::fromRect(QgsRectangle(190000, 560000, 190100, 560100)));
    // 겹쳐 그리면 선택창이 뜬다. 여기서는 「그대로 두기」를 고르고 단추로 지우는 길을 본다.
    const DrawAnswer kept = captureAnswering(capture, QgsGeometry::fromRect(QgsRectangle(190040, 560040, 190060, 560060)),
                                             QStringLiteral("actKeepDrawnShape"));
    QVERIFY(kept.menuSeen);
    QCOMPARE(poly->featureCount(), 2);
    QVERIFY(poly->selectedFeatureIds().isEmpty());

    QVERIFY(QMetaObject::invokeMethod(&window, "eraseOverlapWithShape", Qt::DirectConnection));
    QCOMPARE(poly->selectedFeatureIds().size(), 1);
    QVERIFY2(poly->featureCount() == 2, "첫 번째 누름에서 바로 지웠습니다. 고른 도형을 보여 주기만 해야 합니다.");
    const QgsFeatureId small = *poly->selectedFeatureIds().constBegin();
    QVERIFY(qAbs(poly->getFeature(small).geometry().area() - 400.0) < 1e-3);

    QVERIFY(QMetaObject::invokeMethod(&window, "eraseOverlapWithShape", Qt::DirectConnection));
    QCOMPARE(poly->featureCount(), 1);
    QgsFeature left;
    QVERIFY(poly->getFeatures().nextFeature(left));
    QVERIFY2(qAbs(left.geometry().area() - 9600.0) < 1e-3, "방금 그린 도형 모양대로 지워지지 않았습니다.");
    // 같은 레이어에 겹친 도형이 있으면 다른 레이어(조사구역)는 건드리지 않는다.
    QgsFeature untouched;
    QVERIFY(area->getFeatures().nextFeature(untouched));
    QVERIFY(qAbs(untouched.geometry().area() - 10000.0) < 1e-3);
  }

  // 같은 레이어의 도형 위에 겹쳐 그리고 우클릭으로 마치면 그 자리에 선택창이 떠서 「겹친 곳 지우기」를
  // 고를 수 있다. 조사구역 안에 유구를 그리는 것처럼 다른 레이어의 도형 위는 늘 하는 그리기라 묻지 않는다.
  void drawnShapeOnTopOffersEraseWhenFinished() {
    const QString path = makeSurvey(QStringLiteral("erase_on_finish"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    auto* project = QgsProject::instance();
    auto* area = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QVERIFY(area);
    const QgsFeatureId surveyShape = *area->allFeatureIds().constBegin();
    QVERIFY(QMetaObject::invokeMethod(&window, "startEditFeaturePoly", Qt::DirectConnection));
    auto* poly = LayerOps::findByLayerKey(project, QStringLiteral("feature_poly"));
    auto* capture = window.findChild<KaCaptureMapTool*>();
    QVERIFY(poly && capture);
    const auto rect = [](double x1, double y1, double x2, double y2) {
      return QgsGeometry::fromRect(QgsRectangle(x1, y1, x2, y2));
    };

    // 조사구역(다른 레이어) 안에 유구를 그리면 묻지 않고 늘 하던 대로 이름·번호 창이 뜬다.
    const DrawAnswer pit = captureAnswering(capture, rect(190010, 560010, 190040, 560040),
                                            QStringLiteral("actEraseDrawnOverlap"));
    QVERIFY2(!pit.menuSeen, "다른 레이어의 도형 위에 그렸는데 선택창이 떴습니다.");
    QVERIFY(pit.formSeen);
    QCOMPARE(poly->featureCount(), 1);
    QVERIFY(qAbs(area->getFeature(surveyShape).geometry().area() - 10000.0) < 1e-3);

    // 같은 레이어의 도형 위에 겹쳐 그리면 선택창이 뜬다. 「그대로 두기」면 둘 다 남는다.
    const DrawAnswer kept = captureAnswering(capture, rect(190020, 560020, 190030, 560030),
                                             QStringLiteral("actKeepDrawnShape"));
    QVERIFY2(kept.menuSeen, "같은 레이어의 도형 위에 겹쳐 그렸는데 선택창이 뜨지 않았습니다.");
    QCOMPARE(kept.menuItems, QStringList({QStringLiteral("겹친 곳 지우기"), QStringLiteral("그대로 두기")}));
    QVERIFY(kept.formSeen);
    QCOMPARE(poly->featureCount(), 2);

    // 「겹친 곳 지우기」를 고르면 그린 도형 자리만큼 아래 유구가 지워지고, 그린 도형은 남지 않는다.
    QgsFeatureId big = FID_NULL;
    for (const QgsFeatureId fid : poly->allFeatureIds()) {
      if (qAbs(poly->getFeature(fid).geometry().area() - 900.0) < 1e-3) big = fid;
    }
    QVERIFY(!FID_IS_NULL(big));
    const DrawAnswer erased = captureAnswering(capture, rect(190032, 560032, 190038, 560038),
                                               QStringLiteral("actEraseDrawnOverlap"));
    QVERIFY(erased.menuSeen);
    QVERIFY2(!erased.formSeen, "지운 뒤에 없어진 도형의 이름·번호 창이 떴습니다.");
    QCOMPARE(poly->featureCount(), 2);
    QVERIFY2(qAbs(poly->getFeature(big).geometry().area() - 864.0) < 1e-3,
             "그린 도형 자리만큼 아래 유구가 지워지지 않았습니다.");
    QVERIFY(qAbs(area->getFeature(surveyShape).geometry().area() - 10000.0) < 1e-3);
    auto* canvas = window.findChild<QgsMapCanvas*>();
    QVERIFY(canvas);
    window.show();
    QApplication::setActiveWindow(&window);
    canvas->setFocus();
    QApplication::processEvents();
    QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(poly->featureCount(), 3);
    QVERIFY(qAbs(poly->getFeature(big).geometry().area() - 900.0) < 1e-3);
  }

  // 큰 도형 위에 그린 작은 도형은 찍으면 작은 도형이 잡혀야 한다. 저장하지 않은 도형도 마찬가지고,
  // 고르면 수정점이 나와야 한다. A 키는 도형선택을 켠다.
  void selectTool_picksTheShapeDrawnOnTopAndKeyAStartsIt() {
    const QString path = makeSurvey(QStringLiteral("select_top_shape"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    auto* area = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"));
    auto* canvas = window.findChild<QgsMapCanvas*>();
    QVERIFY(area && canvas);
    const QgsFeatureId outer = *area->allFeatureIds().constBegin();
    QVERIFY(area->isEditable() || area->startEditing());
    QgsFeature inner(area->fields());
    inner.setGeometry(QgsGeometry::fromRect(QgsRectangle(190040, 560040, 190060, 560060)));
    QVERIFY(area->addFeature(inner));
    window.resize(1280, 900);
    window.show();
    QApplication::setActiveWindow(&window);
    canvas->setFocus();
    canvas->setExtent(QgsRectangle(189950, 559950, 190150, 560150));
    QApplication::processEvents();
    const auto clickAt = [&](double x, double y) {
      const QgsPointXY pixel = canvas->getCoordinateTransform()->transform(QgsPointXY(x, y));
      QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier,
                        QPoint(qRound(pixel.x()), qRound(pixel.y())));
      QApplication::processEvents();
    };

    QTest::keyClick(canvas, Qt::Key_A);
    auto* select = window.findChild<KaFeatureSelectTool*>();
    QVERIFY2(select && canvas->mapTool() == select, "A 키로 도형선택이 켜지지 않았습니다.");
    QTest::keyClick(canvas, Qt::Key_A);
    QVERIFY2(canvas->mapTool() == select, "A 키를 다시 눌렀더니 도형선택이 꺼졌습니다.");

    clickAt(190050, 560050);
    QVERIFY2(area->selectedFeatureIds() == QgsFeatureIds{inner.id()},
             "큰 도형 위에 그린 작은 도형(저장 전)을 찍었는데 그 도형이 잡히지 않았습니다.");
    auto* vertex = select->findChild<KaVertexEditTool*>();
    QVERIFY2(vertex && vertex->hasTarget(), "저장하지 않은 도형을 골랐는데 수정점이 나오지 않았습니다.");
    clickAt(190010, 560010);
    QVERIFY2(area->selectedFeatureIds() == QgsFeatureIds{outer},
             "작은 도형 바깥을 찍었는데 큰 도형이 잡히지 않았습니다.");
  }

  // 「폴리곤 나누기」에서 A로 도형선택으로 빠져나온 뒤 그린 면은 나누기 선이 아니라 도형으로 들어간다.
  // 도형선택(A)으로 두 도형을 고르면 그리기 줄을 열지 않아도 우클릭에서 「겹친 곳 지우기」를 고를 수 있고,
  // 리본 「선택」 단추는 도형선택이 켜져 있는 동안 눌린 모양이다.
  void selectTool_rightClickErasesAndKeyAEndsSplitMode() {
    const QString path = makeSurvey(QStringLiteral("select_menu_erase"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    auto* project = QgsProject::instance();
    auto* area = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    auto* canvas = window.findChild<QgsMapCanvas*>();
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    QVERIFY(area && canvas && tree);
    const QgsFeatureId outer = *area->allFeatureIds().constBegin();
    window.resize(1280, 900);
    window.show();
    QApplication::setActiveWindow(&window);
    canvas->setFocus();
    canvas->setExtent(QgsRectangle(189950, 559950, 190150, 560150));
    QApplication::processEvents();

    tree->setCurrentLayer(area);
    QVERIFY(QMetaObject::invokeMethod(&window, "startSplitPolygonTool", Qt::DirectConnection));
    auto* capture = window.findChild<KaCaptureMapTool*>();
    QVERIFY2(capture && canvas->mapTool() == capture, "fixture: 폴리곤 나누기 is drawing its line");
    QTest::keyClick(canvas, Qt::Key_A);
    auto* select = window.findChild<KaFeatureSelectTool*>();
    QVERIFY(select && canvas->mapTool() == select);
    QAction* selectAct = nullptr;
    for (QAction* action : window.findChildren<QAction*>())
      if (action->shortcut() == QKeySequence(QStringLiteral("Ctrl+1"))) selectAct = action;
    QVERIFY2(selectAct && selectAct->isChecked(), "도형선택이 켜졌는데 「선택」 단추가 눌린 모양이 아닙니다.");
    QVERIFY(QMetaObject::invokeMethod(&window, "startEditFeaturePoly", Qt::DirectConnection));
    auto* poly = LayerOps::findByLayerKey(project, QStringLiteral("feature_poly"));
    QVERIFY(poly);
    captureAndDismissForm(capture, QgsGeometry::fromRect(QgsRectangle(190070, 560010, 190090, 560030)));
    QVERIFY2(poly->featureCount() == 1, "나누기에서 빠져나온 뒤 그린 면이 나누기 선으로 버려졌습니다.");

    QVERIFY(area->isEditable() || area->startEditing());
    QgsFeature inner(area->fields());
    inner.setGeometry(QgsGeometry::fromRect(QgsRectangle(190040, 560040, 190060, 560060)));
    QVERIFY(area->addFeature(inner));
    QVERIFY(QMetaObject::invokeMethod(&window, "startSelectTool", Qt::DirectConnection));
    QVERIFY(canvas->mapTool() == select);
    area->selectByIds({outer, inner.id()});
    bool offered = false;
    QTimer answer;
    connect(&answer, &QTimer::timeout, [&offered] {
      if (auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget())) {
        for (QAction* action : menu->actions()) {
          if (action->objectName() != QLatin1String("actSelectEraseOverlap") || !action->isEnabled()) continue;
          offered = true;
          action->trigger();
        }
        menu->close();
      }
    });
    answer.start(20);
    const QgsPointXY pixel = canvas->getCoordinateTransform()->transform(QgsPointXY(190020, 560080));
    QTest::mouseClick(canvas->viewport(), Qt::RightButton, Qt::NoModifier, QPoint(qRound(pixel.x()), qRound(pixel.y())));
    QApplication::processEvents();
    answer.stop();
    QVERIFY2(offered, "두 도형을 고르고 우클릭했는데 「겹친 곳 지우기」가 없습니다.");
    QCOMPARE(area->featureCount(), 1);
    QVERIFY(qAbs(area->getFeature(outer).geometry().area() - 9600.0) < 1e-3);
  }

  // 참고 자료(유적 경계 같은 참조 지도, 지적)의 도형은 도형선택으로 골라 볼 수는 있어도 고칠 수 없다.
  // 「폴리곤 나누기」·「폴리곤 묶기」가 그 파일에 바로 쓰면 Ctrl+Z로도 되돌릴 수 없었다.
  void polygonCommands_leaveReferenceAndCadastralShapesAlone() {
    const QString path = makeSurvey(QStringLiteral("reference_read_only"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    auto* project = QgsProject::instance();
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    QVERIFY(tree);
    const auto memoryLayer = [project](const QString& name, const QStringList& shapes) {
      auto* layer = new QgsVectorLayer(QStringLiteral("MultiPolygon?crs=EPSG:5187"), name, QStringLiteral("memory"));
      layer->startEditing();
      for (const QString& wkt : shapes) {
        QgsFeature feature(layer->fields());
        feature.setGeometry(QgsGeometry::fromWkt(wkt));
        layer->addFeature(feature);
      }
      layer->commitChanges();
      project->addMapLayer(layer);
      return layer;
    };
    auto* heritage = memoryLayer(QStringLiteral("유적 경계"),
        {QStringLiteral("MultiPolygon(((190050 560050,190200 560050,190200 560200,190050 560200,190050 560050)))"),
         QStringLiteral("MultiPolygon(((190150 560150,190300 560150,190300 560300,190150 560300,190150 560150)))")});
    auto* parcels = memoryLayer(QStringLiteral("지적"),
        {QStringLiteral("MultiPolygon(((190000 559800,190050 559800,190050 559850,190000 559850,190000 559800)),"
                        "((190100 559800,190150 559800,190150 559850,190100 559850,190100 559800)))")});
    LayerOps::markReferenceLayer(heritage);
    LayerOps::markCadastralLayer(parcels);
    QVERIFY(heritage->featureCount() == 2 && parcels->featureCount() == 1);
    int refusals = 0;
    QTimer dismiss;
    connect(&dismiss, &QTimer::timeout, [&refusals] {
      for (auto* widget : QApplication::topLevelWidgets())
        if (auto* message = qobject_cast<QMessageBox*>(widget); message && message->isVisible()) {
          ++refusals;
          message->accept();
        }
    });
    dismiss.start(20);

    heritage->selectByIds(heritage->allFeatureIds());
    QVERIFY(QMetaObject::invokeMethod(&window, "startSplitPolygonTool", Qt::DirectConnection));
    QVERIFY2(!heritage->isEditable() && heritage->featureCount() == 2,
             "참조 지도 도형 두 개를 고르고 「폴리곤 나누기」를 눌렀더니 참조 파일에 겹친 조각이 써졌습니다.");
    QVERIFY(QMetaObject::invokeMethod(&window, "mergeFeaturePolygons", Qt::DirectConnection));
    QVERIFY2(!heritage->isEditable() && heritage->featureCount() == 2,
             "참조 지도 도형을 「폴리곤 묶기」로 하나로 합쳤습니다(Ctrl+Z로 되돌릴 수 없음).");
    heritage->removeSelection();
    parcels->selectByIds(parcels->allFeatureIds());
    QVERIFY(QMetaObject::invokeMethod(&window, "startSplitPolygonTool", Qt::DirectConnection));
    QVERIFY2(!parcels->isEditable() && parcels->featureCount() == 1,
             "여러 조각인 지적 도형을 「폴리곤 나누기」가 지적 레이어 안에서 조각냈습니다.");
    parcels->removeSelection();
    tree->setCurrentLayer(heritage);
    QVERIFY(QMetaObject::invokeMethod(&window, "startSplitPolygonTool", Qt::DirectConnection));
    QVERIFY2(!heritage->isEditable(), "좌측 목록에서 참조 지도를 고르고 「폴리곤 나누기」를 누르면 편집이 열립니다.");
    dismiss.stop();
    QVERIFY2(refusals == 4, qPrintable(QStringLiteral("고칠 수 없다는 안내가 %1번 나왔습니다(4번이어야 함).").arg(refusals)));
  }

  void layerContextMenu_usesClickedRowAndLeavesSourceIntact() {
    const QString path = makeSurvey(QStringLiteral("menu_target"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(openSettled(window, path));
    disableRendering(window);
    auto* project = QgsProject::instance();
    auto* survey = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QVERIFY(survey);
    auto* external = makeMenuReference(QStringLiteral("external_shp"));
    QVERIFY(external && external->isValid());
    const QString externalId = external->id();
    const QString surveyId = survey->id();
    const QString externalPath = external->source().section(QLatin1Char('|'), 0, 0);
    const QByteArray originalSurvey = contents(path);
    const QByteArray originalShp = contents(externalPath);
    QVERIFY(!originalSurvey.isEmpty() && !originalShp.isEmpty());
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    QVERIFY(tree);
    window.resize(1280, 900);
    tree->setMinimumHeight(360);
    window.show();
    tree->expandAll();
    tree->setCurrentLayer(survey);
    QApplication::processEvents();
    tree->collapseAll();
    const QPoint blank(tree->viewport()->width() / 2, tree->viewport()->height() - 2);
    QVERIFY(!tree->indexAt(blank).isValid());
    const LayerMenuState blankMenu = inspectLayerMenu(window, tree, blank);
    QVERIFY(blankMenu.seen);
    for (const auto& action : blankMenu.actions) {
      QVERIFY(action.id != QLatin1String("layer.remove"));
      QVERIFY(action.id != QLatin1String("layer.clear"));
    }
    tree->expandAll();
    tree->setCurrentLayer(survey);
    const QModelIndex clicked = tree->node2index(project->layerTreeRoot()->findLayer(external));
    QVERIFY(clicked.isValid());
    tree->scrollTo(clicked);
    const LayerMenuState clickedMenu = inspectLayerMenu(window, tree,
        tree->visualRect(clicked).center(), QStringLiteral("layer.remove"));
    QVERIFY(clickedMenu.seen);
    QVERIFY(!project->mapLayer(externalId));
    QCOMPARE(project->mapLayer(surveyId), survey);
    QCOMPARE(survey->featureCount(), 1LL);
    QCOMPARE(contents(path), originalSurvey);
    QCOMPARE(contents(externalPath), originalShp);
    // Removing a survey legend entry must likewise preserve its saved features.
    tree->setCurrentLayer(survey);
    const QModelIndex surveyIndex = tree->node2index(project->layerTreeRoot()->findLayer(survey));
    tree->scrollTo(surveyIndex);
    const LayerMenuState surveyMenu = inspectLayerMenu(window, tree,
        tree->visualRect(surveyIndex).center(), QStringLiteral("layer.remove"));
    QVERIFY(surveyMenu.seen);
    QVERIFY(!project->mapLayer(surveyId));
    QCOMPARE(contents(path), originalSurvey);
    QgsVectorLayer stored(path + QStringLiteral("|layername=survey_area"),
                          QStringLiteral("stored"), QStringLiteral("ogr"));
    QVERIFY(stored.isValid());
    QCOMPARE(stored.featureCount(), 1LL);
  }
  void layerContextMenu_moveToBottomPreservesRegisteredLayer() {
    const QString path = makeSurvey(QStringLiteral("menu_reorder"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(openSettled(window, path));
    disableRendering(window);
    auto* project = QgsProject::instance();
    project->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* dem = makeMenuReference(QStringLiteral("dem"));
    QVERIFY(dem && dem->isValid());
    const QString source = dem->source();
    const QByteArray originalFile = contents(source);
    QVERIFY(!originalFile.isEmpty());
    LayerOps::placeInLegendGroup(project, dem, QStringLiteral("참조 지도"));
    auto* originalNode = project->layerTreeRoot()->findLayer(dem);
    QVERIFY(originalNode);
    auto* group = qobject_cast<QgsLayerTreeGroup*>(originalNode->parent());
    QVERIFY(group);
    auto* other = new QgsRasterLayer(source, QStringLiteral("다른 고도 자료"), QStringLiteral("gdal"));
    QVERIFY(other->isValid());
    LayerOps::markReferenceLayer(other);
    QVERIFY(project->addMapLayer(other, false));
    group->addLayer(other);
    QVERIFY(group->children().last() != originalNode);
    const QString id = dem->id();
    const QPointer<QgsMapLayer> guard(dem);
    const int layerCount = project->mapLayers().size();
    const int nodeCount = project->layerTreeRoot()->findLayers().size();
    const int siblingCount = group->children().size();
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    QVERIFY(tree);
    window.show();
    tree->expandAll();
    tree->setCurrentLayer(dem);
    QApplication::processEvents();
    const QModelIndex index = tree->node2index(originalNode);
    QVERIFY(index.isValid());
    tree->scrollTo(index);
    const LayerMenuState menu = inspectLayerMenu(window, tree, tree->visualRect(index).center(),
                                                QStringLiteral("layer.bottom"));
    QVERIFY(menu.seen);
    bool enabledBottom = false;
    for (const auto& action : menu.actions)
      if (action.id == QLatin1String("layer.bottom")) enabledBottom = action.enabled;
    QVERIFY(enabledBottom);
    // The registry bridge defers layer deletion after its last tree node disappears.
    // Drain that work before asserting preservation, rather than checking only the
    // synchronous clone/move result.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(guard);
    QCOMPARE(project->mapLayer(id), guard.data());
    QCOMPARE(project->mapLayers().size(), layerCount);
    QCOMPARE(project->layerTreeRoot()->findLayers().size(), nodeCount);
    auto* movedNode = project->layerTreeRoot()->findLayer(id);
    QVERIFY(movedNode);
    QCOMPARE(movedNode->parent(), group);
    QCOMPARE(group->children().size(), siblingCount);
    QCOMPARE(group->children().last(), movedNode);
    QCOMPARE(guard->source(), source);
    QCOMPARE(contents(source), originalFile);
  }
  void mapContextMenu_onlyOffersRecordEditingForSurveyFeatures() {
    const QString path = makeSurvey(QStringLiteral("map_menu_scope"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    auto* survey = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"));
    auto* external = qobject_cast<QgsVectorLayer*>(makeMenuReference(QStringLiteral("external_shp")));
    QVERIFY(survey && external && external->isValid());
    QVERIFY(LayerOps::layerKeyOf(external).isEmpty());
    QCOMPARE(external->providerType(), QStringLiteral("ogr"));
    auto* canvas = window.findChild<QgsMapCanvas*>(QStringLiteral("mapCanvas"));
    QVERIFY(canvas);
    window.show();
    QApplication::processEvents();
    canvas->setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    canvas->setExtent(QgsRectangle(189950, 559950, 190150, 560150));
    for (auto* target : {external, survey}) {
      canvas->setLayers({target});
      const QgsPointXY pixel = canvas->getCoordinateTransform()->transform(190050, 560050);
      const QPoint point(qRound(pixel.x()), qRound(pixel.y()));
      KaAttributeMapTool picker(canvas);
      QgsVectorLayer* picked = nullptr;
      QgsFeature feature;
      QVERIFY(picker.pickAtScreen(point, &picked, &feature));
      QCOMPARE(picked, target);
      QVERIFY(feature.isValid());
      const LayerMenuState menu = inspectMapMenu(window, point);
      QVERIFY(menu.seen);
      QCOMPARE(menu.name, QStringLiteral("mapContextMenu"));
      bool recordAction = false;
      int actionCount = 0;
      for (const auto& action : menu.actions) {
        if (action.separator) continue;
        ++actionCount;
        if (action.id == QLatin1String("map.attributes")) {
          recordAction = true;
          QVERIFY(action.enabled);
        }
        if (!action.enabled) {
          QVERIFY(!action.toolTip.trimmed().isEmpty());
          QVERIFY(action.toolTip != action.text);
        }
      }
      QCOMPARE(recordAction, target == survey);
      QVERIFY(actionCount <= 10);
    }
  }
  void newSurvey_selectedCrsSurvivesSaveAndOpen_data() {
    QTest::addColumn<QString>("authId");
    QTest::newRow("5186") << QStringLiteral("EPSG:5186");
    QTest::newRow("5187") << QStringLiteral("EPSG:5187");
  }
  static bool executeGpkgSql(const QString& path, const char* sql) {
    GDALDatasetH dataset = GDALOpenEx(path.toUtf8().constData(),
        GDAL_OF_VECTOR | GDAL_OF_UPDATE, nullptr, nullptr, nullptr);
    if (!dataset) return false;
    CPLErrorReset();
    OGRLayerH result = GDALDatasetExecuteSQL(dataset, sql, nullptr, nullptr);
    const bool ok = CPLGetLastErrorType() < CE_Failure;
    if (result) GDALDatasetReleaseResultSet(dataset, result);
    GDALClose(dataset);
    return ok;
  }
  static bool selectSaveAs(MainWindow& window, const QString& target) {
    QTimer choose;
    bool selected = false;
    connect(&choose, &QTimer::timeout, [&] {
      if (auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget())) {
        if (selected) return;
        dialog->setOption(QFileDialog::DontConfirmOverwrite);
        dialog->setDirectory(QFileInfo(target).absolutePath());
        dialog->selectFile(QFileInfo(target).fileName());
        if (auto* name = dialog->findChild<QLineEdit*>(QStringLiteral("fileNameEdit")))
          name->setText(QFileInfo(target).fileName());
        const QStringList files = dialog->selectedFiles();
        selected = files.size() == 1 && QFileInfo(files.first()).absoluteFilePath() == QFileInfo(target).absoluteFilePath();
        qInfo() << "Save As selected file:" << files << "matches target:" << selected;
        if (!selected) { choose.stop(); dialog->reject(); return; }
        QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
      } else if (auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
        message->accept();
      }
    });
    choose.start(20);
    QTimer::singleShot(5000, &choose, [&] {
      choose.stop();
      if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) dialog->reject();
    });
    return QMetaObject::invokeMethod(&window, "saveProjectAs", Qt::DirectConnection) && selected;
  }
  static bool openWithAnswer(MainWindow& window, const QString& path,
                             QMessageBox::StandardButton answer, bool* prompted = nullptr) {
    QTimer choose;
    connect(&choose, &QTimer::timeout, [&] {
      if (auto* question = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
        if (question->standardButtons().testFlag(answer)) {
          if (prompted) *prompted = true;
          question->button(answer)->click();
        } else question->reject();
      }
    });
    choose.start(20);
    return window.openSurveyGpkg(path);
  }
  static void captureAndDismissForm(KaCaptureMapTool* capture, const QgsGeometry& geometry) {
    QTimer dismiss;
    QObject::connect(&dismiss, &QTimer::timeout, [] {
      if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()))
        dialog->reject();
    });
    dismiss.start(20);
    capture->geometryCaptured(geometry);
  }
  // 도형을 다 그린 뒤 뜨는 선택창에 choice(동작 이름)로 답하고, 이름·번호 창은 닫는다.
  static DrawAnswer captureAnswering(KaCaptureMapTool* capture, const QgsGeometry& geometry, const QString& choice) {
    DrawAnswer seen;
    QTimer answer;
    QObject::connect(&answer, &QTimer::timeout, [&seen, choice] {
      if (auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget())) {
        seen.menuSeen = true;
        seen.menuItems.clear();
        QAction* picked = nullptr;
        for (QAction* action : menu->actions()) {
          seen.menuItems << action->text();
          if (action->objectName() == choice) picked = action;
        }
        const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
        if (!output.isEmpty() && QDir().mkpath(output))
          menu->grab().save(QDir(output).filePath(QStringLiteral("draw-choice-menu.png")));
        if (picked) picked->trigger();
        menu->close();
        return;
      }
      if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
        seen.formSeen = true;
        dialog->reject();
      }
    });
    answer.start(20);
    capture->geometryCaptured(geometry);
    return seen;
  }
  void newSurvey_selectedCrsSurvivesSaveAndOpen() {
    QFETCH(QString, authId);
    const QString name = QStringLiteral("crs_%1").arg(authId.mid(5));
    const QString path = m_files.filePath(name + QStringLiteral(".gpkg"));
    MainWindow window;
    disableRendering(window);
    window.show();
    QTimer choose;
    bool selected = false;
    bool folderChosen = false;
    connect(&choose, &QTimer::timeout, [&] {
      auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
      if (!dialog) return;
      if (auto* folder = qobject_cast<QFileDialog*>(dialog)) {
        choose.stop();
        FolderDialogFixture::focusNameEdit(folder);
        folderChosen = FolderDialogFixture::chooseFolder(folder, m_files.path());
      } else if (!selected && dialog->windowTitle() == QStringLiteral("새 조사")) {
        dialog->findChild<QLineEdit*>()->setText(name);
        for (auto* button : dialog->findChildren<QPushButton*>()) {
          if (button->text().startsWith(authId.mid(5))) {
            button->click();
            selected = button->isChecked();
          }
        }
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
      }
    });
    choose.start(20);
    QTimer::singleShot(15000, &choose, [&] {
      choose.stop();
      if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) dialog->reject();
    });
    QVERIFY(QMetaObject::invokeMethod(&window, "newSurvey", Qt::DirectConnection));
    choose.stop();
    QVERIFY(selected && folderChosen);
    QVERIFY(QFile::exists(path));
    auto* canvas = window.findChild<QgsMapCanvas*>(QStringLiteral("mapCanvas"));
    auto* chip = window.findChild<QToolButton*>(QStringLiteral("crsButton"));
    auto* upload = window.findChild<QLabel*>(QStringLiteral("uploadCrsChip"));
    QVERIFY(canvas && chip && upload);
    QCOMPARE(QgsProject::instance()->crs().authid(), authId);
    QCOMPARE(canvas->mapSettings().destinationCrs().authid(), authId);
    QCOMPARE(chip->text(), QStringLiteral("작업 %1").arg(authId.mid(5)));
    QCOMPARE(upload->text(), QStringLiteral("→ 제출 5179"));
    // 「→ 제출 5179」 글씨를 누르면 검수·제출 창이 열린다(2026-10-10 현장 보고: 눌러도 아무 일이 없었다).
    QString submitTitle;
    QTimer closeSubmit;
    connect(&closeSubmit, &QTimer::timeout, [&] {
      if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
        submitTitle = dialog->windowTitle();
        closeSubmit.stop();
        dialog->reject();
      }
    });
    closeSubmit.start(20);
    QCOMPARE(upload->cursor().shape(), Qt::PointingHandCursor);
    QTest::mouseClick(upload, Qt::LeftButton);
    closeSubmit.stop();
    QCOMPARE(submitTitle, QStringLiteral("검수·제출"));
    {
      QgsVectorLayer stored(path + QStringLiteral("|layername=survey_area"), name, QStringLiteral("ogr"));
      QVERIFY(stored.isValid());
      QCOMPARE(stored.crs().authid(), authId);
    }
    QVERIFY(saveNow(window));
    const QString other = authId.endsWith(QLatin1String("5186"))
        ? QStringLiteral("EPSG:5187") : QStringLiteral("EPSG:5186");
    QTimer selectCrs;
    connect(&selectCrs, &QTimer::timeout, [&] {
      if (auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget())) {
        for (auto* action : menu->actions()) {
          if (action->text().contains(other)) {
            menu->setActiveAction(action);
            selectCrs.stop();
            QTest::keyClick(menu, Qt::Key_Return);
            break;
          }
        }
      }
    });
    selectCrs.start(20);
    chip->click();
    selectCrs.stop();
    QCOMPARE(QgsProject::instance()->crs().authid(), other);
    QCOMPARE(canvas->mapSettings().destinationCrs().authid(), other);
    QCOMPARE(chip->text(), QStringLiteral("작업 %1").arg(other.mid(5)));
    QCOMPARE(upload->text(), QStringLiteral("→ 제출 5179"));
    QVERIFY(openWithAnswer(window, path, QMessageBox::Discard));
    QCOMPARE(QgsProject::instance()->crs().authid(), authId);
    QCOMPARE(canvas->mapSettings().destinationCrs().authid(), authId);
    QCOMPARE(chip->text(), QStringLiteral("작업 %1").arg(authId.mid(5)));
    QCOMPARE(upload->text(), QStringLiteral("→ 제출 5179"));
  }
  void embeddedOpenRestoresDemInActualTree_data() {
    QTest::addColumn<QString>("preset");
    QTest::addColumn<bool>("reliefEnabled");
    QTest::newRow("legacy-gray") << QStringLiteral("legacy") << false;
    QTest::newRow("lowland-relief-off") << QStringLiteral("lowland") << false;
    QTest::newRow("viewport-relief-off") << QStringLiteral("viewport") << false;
    QTest::newRow("national-relief-on") << QStringLiteral("national") << true;
  }
  void embeddedOpenRestoresDemInActualTree() {
    QFETCH(QString, preset);
    QFETCH(bool, reliefEnabled);
    const QString name = QStringLiteral("dem-embedded-") + preset;
    const QString rasterPath = m_files.filePath(name + QStringLiteral(".tif"));
    GDALAllRegister();
    const auto driver = GDALGetDriverByName("GTiff"); QVERIFY(driver);
    {
      std::unique_ptr<void, decltype(&GDALClose)> dataset(
          GDALCreate(driver, rasterPath.toUtf8().constData(), 16, 16, 1, GDT_Float32, nullptr), GDALClose);
      QVERIFY(dataset);
      double transform[] = {190000., 10., 0., 560000., 0., -10.};
      QCOMPARE(GDALSetGeoTransform(dataset.get(), transform), CE_None);
      const QgsCoordinateReferenceSystem crs(QStringLiteral("EPSG:5187"));
      QCOMPARE(GDALSetProjection(dataset.get(), crs.toWkt().toUtf8().constData()), CE_None);
      float elevations[256];
      for (int i = 0; i < 256; ++i) elevations[i] = 50.f + float(i % 16) * 50.f;
      QCOMPARE(GDALRasterIO(GDALGetRasterBand(dataset.get(), 1), GF_Write, 0, 0, 16, 16,
                            elevations, 16, 16, GDT_Float32, 0, 0), CE_None);
    }
    QString error;
    const QString path = SurveyProjectFactory::createNewSurvey(m_files.path(), name, &error,
                                                               QStringLiteral("EPSG:5187"));
    QVERIFY2(!path.isEmpty(), qPrintable(error));
    const auto colorAt = [](QgsRasterLayer* layer, double elevation) {
      auto* renderer = dynamic_cast<QgsSingleBandPseudoColorRenderer*>(layer->renderer());
      if (!renderer || !renderer->shader()) return QColor();
      int r = 0, g = 0, b = 0, a = 0;
      if (!renderer->shader()->shade(elevation, &r, &g, &b, &a)) return QColor();
      return QColor(r, g, b, a);
    };
    QColor savedColor;
    double savedMinimum = 0., savedMaximum = 2000.;
    {
      QgsProject saved;
      saved.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
      auto dem = std::make_unique<QgsRasterLayer>(rasterPath, QStringLiteral("DEM"), QStringLiteral("gdal"));
      QVERIFY(dem->isValid());
      if (preset != QLatin1String("legacy")) {
        QVERIFY(DemPresentation::apply(dem.get(), preset, dem->extent()));
        savedColor = colorAt(dem.get(), 100.); QVERIFY(savedColor.isValid());
        auto* renderer = dynamic_cast<QgsSingleBandPseudoColorRenderer*>(dem->renderer());
        savedMinimum = renderer->classificationMin(); savedMaximum = renderer->classificationMax();
      }
      LayerOps::markReferenceLayer(dem.get());
      auto* layer = dem.get(); QVERIFY(saved.addMapLayer(layer)); dem.release();
      if (preset != QLatin1String("legacy")) {
        auto* shade = LayerOps::ensureDemRelief(&saved, layer); QVERIFY(shade);
        auto* shadeNode = saved.layerTreeRoot()->findLayer(shade->id()); QVERIFY(shadeNode);
        shadeNode->setItemVisibilityChecked(reliefEnabled);
      }
      layer->setCustomProperty(QStringLiteral("ka_hgis/dem_relief_enabled"), reliefEnabled);
      QVERIFY2(SurveyStorage::writeEmbedded(&saved, path, &error), qPrintable(error));
    }
    QVERIFY(SurveyStorage::hasEmbeddedProject(path));
    MainWindow window;
    disableRendering(window);
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree")); QVERIFY(tree);
    auto* originalModel = tree->layerTreeModel();
    QVERIFY(window.openSurveyGpkg(path));
    auto* project = QgsProject::instance();
    const auto layers = project->mapLayersByName(QStringLiteral("DEM")); QCOMPARE(layers.size(), 1);
    auto* dem = qobject_cast<QgsRasterLayer*>(layers.first()); QVERIFY(dem && dem->isValid());
    auto* node = project->layerTreeRoot()->findLayer(dem->id()); QVERIFY(node);
    QCOMPARE(tree->layerTreeModel(), originalModel);
    const auto legendNodes = originalModel->layerLegendNodes(node);
    QCOMPARE(legendNodes.size(), 1);
    QVERIFY2(dynamic_cast<DemColorRampLegend*>(dem->legend()), "Actual MainWindow open must reinstall the DEM legend.");
    QVERIFY2(dynamic_cast<QgsColorRampLegendNode*>(legendNodes.first()), "The actual tree must show the continuous elevation ramp.");
    QVERIFY(!legendNodes.first()->data(Qt::DisplayRole).toString().contains(QStringLiteral("Gray")));
    QCOMPARE(dem->customProperty(QStringLiteral("ka_hgis/dem_preset")).toString(),
             preset == QLatin1String("legacy") ? QStringLiteral("national") : preset);
    auto* renderer = dynamic_cast<QgsSingleBandPseudoColorRenderer*>(dem->renderer()); QVERIFY(renderer);
    QCOMPARE(renderer->classificationMin(), savedMinimum);
    QCOMPARE(renderer->classificationMax(), savedMaximum);
    if (savedColor.isValid()) QCOMPARE(colorAt(dem, 100.), savedColor);
    QVERIFY(dem->findChild<QTimer*>(QStringLiteral("demViewportTimer")));
    QCOMPARE(dem->customProperty(QStringLiteral("ka_hgis/dem_relief_enabled")).toBool(), reliefEnabled);
    const auto shades = project->mapLayersByName(QStringLiteral("지형 음영"));
    QCOMPARE(shades.size(), preset == QLatin1String("legacy") ? 0 : 1);
    if (!shades.isEmpty()) {
      auto* shadeNode = project->layerTreeRoot()->findLayer(shades.first()->id()); QVERIFY(shadeNode);
      QCOMPARE(shadeNode->itemVisibilityChecked(), reliefEnabled);
    }
  }
  // 포터블을 USB 로 쓰면 꽂을 때마다 드라이브 글자가 바뀌고(E: → F:), 조사 폴더를 옮기기도
  // 한다. 앱이 이 PC의 AppData·임시 폴더에 만든 자료(주변유적 등)도 저장할 때 조사 폴더로
  // 모여야, 그 자료가 없는 곳에서 조사를 다시 열어도 모든 레이어가 살아 있다.
  void surveyReopensAfterUsbDriveOrFolderChange() {
    QTemporaryDir usb;
    QTemporaryDir appData;
    QVERIFY(usb.isValid() && appData.isValid());
    const QString surveyDir = usb.filePath(QStringLiteral("조사/안동"));
    QVERIFY(QDir().mkpath(surveyDir));
    const QString path = makeSurveyIn(surveyDir, QStringLiteral("안동"));
    QVERIFY(!path.isEmpty());
    const QString heritageFile = QDir(appData.path()).filePath(QStringLiteral("주변유적/heritage-T/heritage.gpkg"));
    QVERIFY(QDir().mkpath(QFileInfo(heritageFile).absolutePath()));
    QVERIFY(writeVectorFile(heritageFile, QStringLiteral("heritage"), 3));
    QVERIFY(SurveyBundle::isAppManagedPath(heritageFile));
    {
      MainWindow window;
      disableRendering(window);
      QVERIFY(window.openSurveyGpkg(path));
      auto* heritage = new QgsVectorLayer(heritageFile + QStringLiteral("|layername=heritage"),
                                          QStringLiteral("국가지정유산"), QStringLiteral("ogr"));
      QVERIFY(heritage->isValid());
      LayerOps::markReferenceLayer(heritage);
      QgsProject::instance()->addMapLayer(heritage);
      QVERIFY(saveNow(window));
      const QString source = QDir::fromNativeSeparators(SurveyBundle::sourceFile(heritage->source()));
      QVERIFY2(source.startsWith(QDir::fromNativeSeparators(surveyDir), Qt::CaseInsensitive), qPrintable(source));
      QVERIFY2(source.contains(SurveyBundle::collectedFolderName()), qPrintable(source));
      QCOMPARE(heritage->featureCount(), 3LL);
      QgsProject::instance()->setDirty(false);
    }
    // 앱 자료는 이 PC에만 있었다. 지우고, 조사 폴더는 다른 드라이브·폴더로 옮긴다.
    QVERIFY(appData.remove());
    QTemporaryDir otherUsb;
    QVERIFY(otherUsb.isValid());
    const QString movedDir = otherUsb.filePath(QStringLiteral("다른 폴더/안동"));
    QVERIFY(copyDirectory(surveyDir, movedDir));
    // 원래 자리는 이 PC에 없는 것처럼 지운다. 먼저 열린 파일을 모두 놓는다.
    QStringList held;
    for (QgsMapLayer* layer : QgsProject::instance()->mapLayers())
      held << SurveyBundle::sourceFile(layer->source());
    QgsProject::instance()->clear();
    for (const QString& file : held)
      if (!file.isEmpty()) QgsOgrProviderUtils::invalidateCachedDatasets(file);
    QTRY_VERIFY_WITH_TIMEOUT(QDir(usb.filePath(QStringLiteral("조사"))).removeRecursively(), 15000);
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(QDir(movedDir).filePath(QFileInfo(path).fileName())));
    QTRY_VERIFY(layerNamed(QStringLiteral("국가지정유산")) && layerNamed(QStringLiteral("국가지정유산"))->isValid());
    auto* heritage = qobject_cast<QgsVectorLayer*>(layerNamed(QStringLiteral("국가지정유산")));
    QVERIFY(heritage);
    QCOMPARE(heritage->featureCount(), 3LL);
    QVERIFY2(QDir::fromNativeSeparators(heritage->source()).startsWith(QDir::fromNativeSeparators(movedDir), Qt::CaseInsensitive),
             qPrintable(heritage->source()));
    auto* area = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"));
    QVERIFY(area && area->isValid());
    QCOMPARE(area->featureCount(), 1LL);
    QVERIFY2(SurveyBundle::missingFileLayers(QgsProject::instance()).isEmpty(),
             qPrintable(SurveyBundle::missingFileLayers(QgsProject::instance()).join(QStringLiteral(", "))));
    QgsProject::instance()->setDirty(false);
  }
  // 저장 때 조사 파일로 흡수한 레이어(주변 500m 버퍼 등)가 저장 뒤 지워지는 세대 폴더
  // (.ka-survey-gen-*)를 가리킨 채 기록되면, 열 때마다 끊기고 다음 저장이 「unable to open
  // database file」로 실패했다(안동시 조사, 2026-09-26 05:53).
  void savedWorkspaceNeverPointsIntoTheSaveGenerationFolder() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString path = makeSurveyIn(root.filePath(QStringLiteral("세대")), QStringLiteral("세대"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    auto* buffer = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187&field=nm:string(20)"),
                                      QStringLiteral("주변 500m"), QStringLiteral("memory"));
    QVERIFY(buffer->startEditing());
    QgsFeature ring(buffer->fields());
    ring.setGeometry(QgsGeometry::fromRect(QgsRectangle(189900, 559900, 190200, 560200)));
    QVERIFY(buffer->addFeature(ring));
    QVERIFY(buffer->commitChanges());
    QgsProject::instance()->addMapLayer(buffer);
    QVERIFY(saveNow(window));
    QgsProject stored;
    QVERIFY(stored.read(SurveyStorage::projectUri(path)));
    int checked = 0;
    for (QgsMapLayer* layer : stored.mapLayers()) {
      QVERIFY2(!layer->source().contains(QLatin1String(".ka-survey-gen-")),
               qPrintable(layer->name() + QStringLiteral(": ") + layer->source()));
      ++checked;
    }
    QVERIFY(checked >= 2);
    const QList<QgsMapLayer*> rings = stored.mapLayersByName(QStringLiteral("주변 500m"));
    QVERIFY(!rings.isEmpty());
    QVERIFY2(rings.first()->isValid(), "고치지 않고 바로 열려야 한다");
    QCOMPARE(qobject_cast<QgsVectorLayer*>(rings.first())->featureCount(), 1LL);
    QCOMPARE(SurveyBundle::savedSurveyDir(&stored),
             QDir::cleanPath(QDir::fromNativeSeparators(QFileInfo(path).absolutePath())));
    QgsProject::instance()->setDirty(false);
  }
  // 새 버전 포터블은 새 폴더다. 설정이 포터블 폴더 안에 있어 「이어서 열기」가 비어 예전 조사를
  // 못 찾는 것처럼 보였다. 옆에 있는 이전 포터블의 설정을 이어받되 계정·키 파일은 옮기지 않는다.
  void newPortableFolderInheritsRecentSurveys() {
    QTemporaryDir usb;
    QVERIFY(usb.isValid());
    const QString oldDir = usb.filePath(QStringLiteral("Strata-0925"));
    const QString newDir = usb.filePath(QStringLiteral("Strata-0926"));
    for (const QString& dir : {oldDir, newDir}) {
      QVERIFY(QDir().mkpath(dir));
      QFile exe(QDir(dir).filePath(QStringLiteral("ka-hgis.exe")));
      QVERIFY(exe.open(QIODevice::WriteOnly));
    }
    const QString oldIni = QDir(oldDir).filePath(QStringLiteral("config/ka-hgis/ka-hgis.ini"));
    const QStringList recent{QStringLiteral("E:/조사/안동/안동.gpkg\t안동\t1")};
    {
      QSettings old(oldIni, QSettings::IniFormat);
      old.setValue(QStringLiteral("RecentSurveys/items"), recent);
      old.sync();
    }
    QFile secret(QDir(oldDir).filePath(QStringLiteral("config/secrets.ini")));
    QVERIFY(secret.open(QIODevice::WriteOnly));
    secret.close();
    const QString newConfig = QDir(newDir).filePath(QStringLiteral("config"));
    QCOMPARE(QFileInfo(KaPortableRuntime::inheritSiblingSettings(newDir, newConfig)).absoluteFilePath(),
             QFileInfo(oldIni).absoluteFilePath());
    QSettings inherited(QDir(newConfig).filePath(QStringLiteral("ka-hgis/ka-hgis.ini")), QSettings::IniFormat);
    QCOMPARE(inherited.value(QStringLiteral("RecentSurveys/items")).toStringList(), recent);
    QVERIFY2(!QFileInfo::exists(QDir(newConfig).filePath(QStringLiteral("secrets.ini"))), "키 파일은 옮기지 않는다");
    // 이미 쓰던 설정은 덮어쓰지 않는다.
    QVERIFY(KaPortableRuntime::inheritSiblingSettings(newDir, newConfig).isEmpty());
  }

  // 9월 22일 판은 조사구역에 도형을 더해 두 번째로 저장하면 「UNIQUE constraint failed:
  // survey_area.fid」로 실패하고 편집이 복구 사본에만 남았다. 다른 PC에서 그 판으로 일한 뒤
  // 조사를 열면 마지막 작업이 없었다. 여러 번 고쳐 저장해도 모든 도형이 조사 파일에 남아야 한다.
  void saveAgainAfterAddingSurveyAreaKeepsEveryFeature() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString path = makeSurveyIn(root.filePath(QStringLiteral("다시저장")), QStringLiteral("다시저장"));
    QVERIFY(!path.isEmpty());
    {
      MainWindow window;
      disableRendering(window);
      QVERIFY(window.openSurveyGpkg(path));
      auto* area = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"));
      QVERIFY(area && area->isValid());
      QCOMPARE(area->featureCount(), 1LL);
      for (int round = 1; round <= 3; ++round) {
        QVERIFY(area->isEditable() || area->startEditing());
        QgsFeature feature(area->fields());
        feature.setAttribute(QStringLiteral("survey_name"), QStringLiteral("추가 %1").arg(round));
        feature.setGeometry(QgsGeometry::fromRect(
            QgsRectangle(190200. + round * 150, 560000., 190300. + round * 150, 560100.)));
        QVERIFY(area->addFeature(feature));
        QVERIFY2(saveNow(window), qPrintable(QStringLiteral("%1번째 저장이 실패했습니다").arg(round)));
        QgsVectorLayer stored(SurveyBundle::sourceFile(area->source()) + QStringLiteral("|layername=") +
                                  area->source().section(QLatin1String("layername="), 1, 1).section(QLatin1Char('|'), 0, 0),
                              QStringLiteral("저장본"), QStringLiteral("ogr"));
        QVERIFY(stored.isValid());
        QCOMPARE(stored.featureCount(), 1LL + round);
      }
      // 이미 저장된 도형을 고치고 하나는 지운 뒤 저장해도 그대로 남아야 한다.
      QVERIFY(area->isEditable() || area->startEditing());
      QgsFeature first;
      QVERIFY(area->getFeatures().nextFeature(first));
      const int nameField = area->fields().lookupField(QStringLiteral("survey_name"));
      QVERIFY(nameField >= 0);
      QVERIFY(area->changeAttributeValue(first.id(), nameField, QStringLiteral("고친 이름")));
      QgsGeometry movedShape = QgsGeometry::fromRect(QgsRectangle(189000, 559000, 189050, 559050));
      QVERIFY(area->changeGeometry(first.id(), movedShape));
      QgsFeature last;
      QgsFeatureIterator rows = area->getFeatures();
      QgsFeature row;
      while (rows.nextFeature(row)) last = row;
      QVERIFY(last.id() != first.id());
      QVERIFY(area->deleteFeature(last.id()));
      QVERIFY2(saveNow(window), "고치고 지운 뒤 저장이 실패했습니다");
      QgsProject::instance()->setDirty(false);
    }
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    auto* area = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"));
    QVERIFY(area && area->isValid());
    QCOMPARE(area->featureCount(), 3LL);
    bool renamed = false;
    QgsFeature row;
    QgsFeatureIterator rows = area->getFeatures();
    while (rows.nextFeature(row)) {
      if (row.attribute(QStringLiteral("survey_name")).toString() != QStringLiteral("고친 이름")) continue;
      renamed = true;
      QVERIFY(qAbs(row.geometry().boundingBox().xMinimum() - 189000.) < 0.01);
    }
    QVERIFY2(renamed, "고친 속성이 남아 있어야 한다");
    QgsProject::instance()->setDirty(false);
  }

  // Copies every file under `from` into `to` without overwriting files that are already there.
  // Entries whose name starts with one of `skip` are left out.
  static bool mergeDirectory(const QString& from, const QString& to, const QStringList& skip = {}) {
    if (!QDir().mkpath(to)) return false;
    for (const QFileInfo& entry : QDir(from).entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot)) {
      if (std::any_of(skip.cbegin(), skip.cend(),
                      [&entry](const QString& prefix) { return entry.fileName().startsWith(prefix); }))
        continue;
      const QString target = QDir(to).filePath(entry.fileName());
      if (entry.isDir()) {
        if (!mergeDirectory(entry.absoluteFilePath(), target, skip)) return false;
      } else if (!QFileInfo::exists(target) && !QFile::copy(entry.absoluteFilePath(), target)) {
        return false;
      }
    }
    return true;
  }

  // Rebuilds the 2026-09-22 compat survey when its binary (.gpkg) is not in the checkout.
  //
  // The original sample was saved by the 2026-09-22 app (3ddca7a) but its .gpkg/.qgz were
  // swallowed by .gitignore (*.gpkg, *.qgz) and could not be recovered, so a fresh clone,
  // CI or another PC failed on the first openSurveyGpkg. This writes the same survey the way
  // that version stored it (3ddca7a SurveyStorage.cpp: persistWorkspace, absorbExternalVectors,
  // writeEmbedded; LayerOps.cpp: placeCadastralLayer):
  //  - the workspace is embedded in the survey GPKG (qgis_projects, projectName=survey) with
  //    absolute paths into the folder of the PC that saved it, and no ka_hgis survey_dir entry;
  //  - absorbed memory layers (주변 500m, 지적도 본번) still point at the generation copy
  //    (.ka-survey-gen-*/survey.gpkg) that was deleted after the save;
  //  - cadastral layers sit in the old 「지적도」 group, heritage in 참조 지도 › 지정유산;
  //  - 국가지정유산 lived in that PC's AppData, so it does not travel with the survey folder.
  // The saving PC's folder is deleted at the end, so the survey opens like a moved folder.
  static bool writeLegacySurvey20260922(const QString& targetSurveyDir, const QString& surveyFile, QString* why) {
    const auto fail = [why](const QString& text) {
      if (why) *why = text;
      return false;
    };
    const QString name = QFileInfo(surveyFile).completeBaseName();
    if (name.isEmpty()) return fail(QStringLiteral("manifest.ini 에 survey= 가 없습니다"));
    QTemporaryDir otherPc;
    if (!otherPc.isValid()) return fail(QStringLiteral("저장한 PC 를 흉내 낼 임시 폴더를 만들지 못했습니다"));
    const QString oldSurveyDir = otherPc.filePath(QStringLiteral("Users/옛조사원/Documents/") + name);
    QString error;
    const QString gpkg =
        SurveyProjectFactory::createNewSurvey(oldSurveyDir, name, &error, QStringLiteral("EPSG:5187"));
    if (gpkg.isEmpty()) return fail(QStringLiteral("조사 파일을 만들지 못했습니다: ") + error);
    const QgsCoordinateReferenceSystem crs(QStringLiteral("EPSG:5187"));
    const QgsRectangle areaRect(190000, 560000, 190100, 560100);
    {
      QgsVectorLayer area(gpkg + QStringLiteral("|layername=survey_area"), name, QStringLiteral("ogr"));
      if (!area.isValid() || !area.startEditing()) return fail(QStringLiteral("survey_area 를 열지 못했습니다"));
      QgsFeature feature(area.fields());
      feature.setAttribute(QStringLiteral("survey_name"), name);
      feature.setGeometry(QgsGeometry::fromRect(areaRect));
      if (!area.addFeature(feature) || !area.commitChanges()) return fail(QStringLiteral("조사구역을 쓰지 못했습니다"));
    }
    // That version saved into a generation copy, absorbed memory layers there and then
    // replaced the survey file with it.
    const QString generationDir = QDir(oldSurveyDir).filePath(QStringLiteral(".ka-survey-gen-0922aB"));
    const QString generation = QDir(generationDir).filePath(QStringLiteral("survey.gpkg"));
    // A GeoPackage edited through QGIS may still keep committed pages in its WAL.
    const auto copySqlite = [](const QString& from, const QString& to) {
      for (const QString& suffix : {QString(), QStringLiteral("-wal")}) {
        if (!suffix.isEmpty() && !QFileInfo::exists(from + suffix)) continue;
        QFile::remove(to + suffix);
        if (!QFile::copy(from + suffix, to + suffix)) return false;
      }
      return true;
    };
    QgsOgrProviderUtils::invalidateCachedDatasets(QFileInfo(gpkg).absoluteFilePath());
    if (!QDir().mkpath(generationDir) || !copySqlite(gpkg, generation))
      return fail(QStringLiteral("세대 사본을 만들지 못했습니다"));
    const auto writeTable = [&](const QString& file, const QString& table, const QString& uri,
                                const QgsGeometry& shape) {
      QgsVectorLayer memory(uri, table, QStringLiteral("memory"));
      if (!memory.isValid() || !memory.startEditing()) return false;
      QgsFeature feature(memory.fields());
      feature.setAttribute(0, table);
      feature.setGeometry(shape);
      if (!memory.addFeature(feature) || !memory.commitChanges()) return false;
      QgsVectorFileWriter::SaveVectorOptions options;
      options.driverName = QStringLiteral("GPKG");
      options.layerName = table;
      options.fileEncoding = QStringLiteral("UTF-8");
      options.actionOnExistingFile = QFileInfo::exists(file) ? QgsVectorFileWriter::CreateOrOverwriteLayer
                                                             : QgsVectorFileWriter::CreateOrOverwriteFile;
      QString detail;
      return QgsVectorFileWriter::writeAsVectorFormatV3(&memory, file, QgsCoordinateTransformContext(), options,
                                                        &detail) == QgsVectorFileWriter::NoError;
    };
    const QString polygonUri = QStringLiteral("Polygon?crs=EPSG:5187&field=nm:string(40)");
    if (!writeTable(generation, QStringLiteral("주변_500m"), polygonUri,
                    QgsGeometry::fromRect(QgsRectangle(189500, 559500, 190600, 560600))) ||
        !writeTable(generation, QStringLiteral("지적도_본번"), polygonUri,
                    QgsGeometry::fromRect(QgsRectangle(189950, 559950, 190050, 560050))))
      return fail(QStringLiteral("흡수한 레이어를 세대 사본에 쓰지 못했습니다"));
    const QString heritageFile =
        otherPc.filePath(QStringLiteral("Users/옛조사원/AppData/Local/ka-hgis/ka-hgis/주변유적/heritage-0922.gpkg"));
    if (!QDir().mkpath(QFileInfo(heritageFile).absolutePath()) ||
        !writeTable(heritageFile, QStringLiteral("국가지정유산"), QStringLiteral("Point?crs=EPSG:5187&field=nm:string(40)"),
                    QgsGeometry::fromPointXY(QgsPointXY(190050, 560050))))
      return fail(QStringLiteral("조사 폴더 밖 유산 자료를 만들지 못했습니다"));
    // The relief raster is the one committed next to the manifest; recreate it if absent.
    const QString reliefDir = QDir(oldSurveyDir).filePath(QStringLiteral("ka-hgis-reference-T"));
    const QString shippedRelief = QDir(targetSurveyDir).filePath(QStringLiteral("ka-hgis-reference-T"));
    if (QFileInfo::exists(QDir(shippedRelief).filePath(QStringLiteral("relief.png")))) {
      if (!mergeDirectory(shippedRelief, reliefDir)) return fail(QStringLiteral("지형 음영 표본을 옮기지 못했습니다"));
    } else {
      QImage relief(8, 8, QImage::Format_RGB32);
      relief.fill(QColor(0, 128, 0));
      QFile world(QDir(reliefDir).filePath(QStringLiteral("relief.pgw")));
      if (!QDir().mkpath(reliefDir) || !relief.save(QDir(reliefDir).filePath(QStringLiteral("relief.png"))) ||
          !world.open(QIODevice::WriteOnly) || world.write("10\n0\n0\n-10\n190000.5\n560079.5\n") <= 0)
        return fail(QStringLiteral("지형 음영 래스터를 만들지 못했습니다"));
    }
    {
      QgsProject legacy;
      legacy.setCrs(crs);
      legacy.setTitle(name);
      QgsLayerTree* tree = legacy.layerTreeRoot();
      QgsLayerTreeGroup* surveyGroup = tree->addGroup(QStringLiteral("조사 데이터"));
      QgsLayerTreeGroup* cadastralGroup = tree->addGroup(QStringLiteral("지적도"));
      QgsLayerTreeGroup* referenceGroup = tree->addGroup(QStringLiteral("참조 지도"));
      QgsLayerTreeGroup* designatedGroup = referenceGroup->addGroup(QStringLiteral("지정유산"));
      QString broken;
      const auto place = [&](QgsMapLayer* layer, QgsLayerTreeGroup* group, const QString& role) -> QgsMapLayer* {
        if (!layer->isValid()) {
          broken += layer->name() + QLatin1Char(' ');
          delete layer;
          return nullptr;
        }
        if (!role.isEmpty()) layer->setCustomProperty(QStringLiteral("ka_hgis/layer_role"), role);
        legacy.addMapLayer(layer, false);
        group->addLayer(layer);
        return layer;
      };
      const auto web = [](const QString& host, const QString& title) {
        return new QgsRasterLayer(
            QStringLiteral("type=xyz&url=https://%1.invalid/{z}/{x}/{y}.png&zmin=0&zmax=19").arg(host), title,
            QStringLiteral("wms"));
      };
      auto* area = new QgsVectorLayer(gpkg + QStringLiteral("|layername=survey_area"), name, QStringLiteral("ogr"));
      area->setCustomProperty(QStringLiteral("ka_hgis/layer_key"), QStringLiteral("survey_area"));
      QgsMapLayer* areaLayer = place(area, surveyGroup, QString());
      place(new QgsVectorLayer(generation + QStringLiteral("|layername=주변_500m"), QStringLiteral("주변 500m"),
                               QStringLiteral("ogr")),
            surveyGroup, QString());
      auto* cadastral = new QgsVectorLayer(generation + QStringLiteral("|layername=지적도_본번"),
                                           QStringLiteral("지적도 본번"), QStringLiteral("ogr"));
      cadastral->setCustomProperty(QStringLiteral("ka_hgis/cadastral"), true);
      place(cadastral, cadastralGroup, QStringLiteral("cadastral"));
      place(web(QStringLiteral("compat-cadastral-main"), QStringLiteral("지적 본번")), cadastralGroup, QString());
      place(web(QStringLiteral("compat-cadastral-sub"), QStringLiteral("지적 부번")), cadastralGroup, QString());
      place(new QgsVectorLayer(heritageFile + QStringLiteral("|layername=국가지정유산"), QStringLiteral("국가지정유산"),
                               QStringLiteral("ogr")),
            designatedGroup, QStringLiteral("reference"));
      auto* relief = new QgsRasterLayer(QDir(reliefDir).filePath(QStringLiteral("relief.png")),
                                        QStringLiteral("지형 음영"), QStringLiteral("gdal"));
      relief->setCrs(crs);
      place(relief, referenceGroup, QStringLiteral("reference"));
      place(web(QStringLiteral("compat-satellite"), QStringLiteral("위성")), referenceGroup, QStringLiteral("reference"));
      if (!broken.isEmpty() || !areaLayer) return fail(QStringLiteral("표본 레이어를 열지 못했습니다: ") + broken);
      // One drawing, made like that version's LayoutService::createBlankSheet.
      auto* layout = new QgsPrintLayout(&legacy);
      layout->initializeDefaults();
      layout->setName(QStringLiteral("user_sheet"));
      layout->setUnits(Qgis::LayoutUnit::Millimeters);
      layout->setCustomProperty(QStringLiteral("ka_hgis/auto_template"), false);
      layout->setCustomProperty(QStringLiteral("ka_hgis/user_composed"), false);
      auto* map = new QgsLayoutItemMap(layout);
      map->setId(QStringLiteral("ka_map"));
      map->attemptSetSceneRect(QRectF(20.0, 20.0, 200.0, 150.0));
      map->setCrs(crs);
      map->setKeepLayerSet(true);
      map->setLayers(QList<QgsMapLayer*>{areaLayer});
      map->zoomToExtent(areaRect);
      layout->addLayoutItem(map);
      if (!legacy.layoutManager()->addLayout(layout)) return fail(QStringLiteral("도면을 넣지 못했습니다"));
      // 3ddca7a writeEmbedded: file name = generation copy, home = survey folder, absolute paths.
      legacy.setFileName(QFileInfo(generation).absoluteFilePath());
      legacy.setPresetHomePath(QFileInfo(oldSurveyDir).absoluteFilePath());
      legacy.setFilePathStorage(Qgis::FilePathType::Absolute);
      if (!legacy.write(QStringLiteral("geopackage:%1?projectName=survey").arg(QFileInfo(generation).absoluteFilePath())))
        return fail(QStringLiteral("작업공간을 조사 파일 안에 쓰지 못했습니다: ") + legacy.error());
      LayerOps::saveGpkgDefaultStyles(&legacy, QFileInfo(generation).absoluteFilePath());
      legacy.clear();
    }
    for (const QString& file : {gpkg, generation, heritageFile})
      QgsOgrProviderUtils::invalidateCachedDatasets(QFileInfo(file).absoluteFilePath());
    // That version then published the generation over the survey file, so the survey file that
    // travels holds the generation's bytes. Copy them (plus a WAL SQLite may still keep beside
    // them) rather than replacing a file another handle might hold open.
    if (QFileInfo(surveyFile).fileName() != QFileInfo(gpkg).fileName())
      return fail(QStringLiteral("조사 파일 이름이 manifest 와 다릅니다: ") + QFileInfo(gpkg).fileName());
    if (!QDir().mkpath(targetSurveyDir)) return fail(QStringLiteral("표본 조사 폴더를 만들지 못했습니다"));
    const QString targetGpkg = QDir(targetSurveyDir).filePath(QFileInfo(gpkg).fileName());
    if (!copySqlite(generation, targetGpkg)) return fail(QStringLiteral("세대 사본을 조사 파일로 옮기지 못했습니다"));
    // Everything else in the survey folder travels as is: the companion .qgz and the relief
    // raster. The pre-publish survey file, its journals and the generation folder do not.
    if (!mergeDirectory(oldSurveyDir, targetSurveyDir,
                        {QFileInfo(gpkg).fileName(), QStringLiteral(".ka-survey-gen-")}))
      return fail(QStringLiteral("만든 조사를 표본 폴더로 옮기지 못했습니다"));
    // The saving PC is gone: absolute paths into it must not resolve any more.
    if (!otherPc.remove())
      qWarning().noquote() << "compat: could not delete the simulated saving PC folder" << otherPc.path();
    return true;
  }

  // 예전 판 앱이 저장한 조사가 지금 판에서 열려야 한다. tests/data/compat/<판 날짜>/ 는 그 판의
  // 앱 코드로 저장한 합성 조사다(실제 유적 자료 없음). manifest.ini 에 그 판이 저장한 레이어·
  // 조사구역 도형 수·도면 수를 적어 두었다. 앱을 고쳐 이 시험이 깨지면, 포터블을 새 판으로
  // 바꿨을 때 예전 작업이 안 열리게 된 것이다. KA_COMPAT_DIR 로 다른 표본 폴더를 줄 수 있다.
  // 표본 조사 파일(.gpkg)이 저장소에 없으면(2026-09-22 표본은 .gitignore 에 걸려 잃었다) 그 판의
  // 저장 모양대로 다시 만든다(writeLegacySurvey20260922). 다시 만들 방법이 없는 판이면 원인을 적고 실패한다.
  void oldVersionSurveysStillOpen() {
    QString rootPath = qEnvironmentVariableIsEmpty("KA_COMPAT_DIR")
                           ? QStringLiteral("tests/data/compat")
                           : qEnvironmentVariable("KA_COMPAT_DIR");
    // ctest runs from the source root; a direct run from the build folder finds the data next to this file.
    if (qEnvironmentVariableIsEmpty("KA_COMPAT_DIR") && !QDir(rootPath).exists())
      rootPath = QFINDTESTDATA("data/compat");
    const QDir root(rootPath);
    QVERIFY2(root.exists(), qPrintable(QStringLiteral("호환 표본 폴더가 없습니다: ") + root.absolutePath()));
    const QStringList versions = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    QVERIFY(!versions.isEmpty());
    for (const QString& version : versions) {
      // 표본을 더럽히지 않게 사본에서 연다. 다시 저장까지 해 본다.
      QTemporaryDir work;
      QVERIFY(work.isValid());
      QVERIFY(copyDirectory(root.filePath(version), work.path()));
      QSettings manifest(QDir(work.path()).filePath(QStringLiteral("manifest.ini")), QSettings::IniFormat);
      const QString survey = QDir(work.path()).filePath(QStringLiteral("survey/") +
                                                        manifest.value(QStringLiteral("survey")).toString());
      if (!QFileInfo::exists(survey)) {
        QVERIFY2(version == QLatin1String("2026-09-22"),
                 qPrintable(version + QStringLiteral(": 표본 조사 파일이 저장소에 없습니다(") + survey +
                            QStringLiteral("). .gitignore 의 tests/data/compat 예외와 커밋을 확인하세요.")));
        QString why;
        QVERIFY2(writeLegacySurvey20260922(QDir(work.path()).filePath(QStringLiteral("survey")), survey, &why),
                 qPrintable(version + QStringLiteral(": 표본 조사 파일이 없고 그 판의 저장 모양으로 다시 만들지도 "
                                                     "못했습니다: ") + why));
        QVERIFY2(QFileInfo::exists(survey), qPrintable(version + QStringLiteral(": 다시 만든 조사 파일이 없습니다")));
        qInfo().noquote() << version << "compat survey regenerated in the 2026-09-22 storage layout";
      }
      const qint64 areaCount = manifest.value(QStringLiteral("survey_area")).toLongLong();
      const int layouts = manifest.value(QStringLiteral("layouts")).toInt();
      const QStringList layers = manifest.value(QStringLiteral("layers")).toStringList();
      // 표본을 만든 PC 의 조사 폴더 밖(AppData 등)에 있던 자료. 여기서는 없을 수 있다.
      const QStringList external = manifest.value(QStringLiteral("external")).toStringList();
      const auto check = [&](const QString& stage) {
        const QString where = version + QStringLiteral(" ") + stage + QStringLiteral(": ");
        auto* area = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"));
        QVERIFY2(area && area->isValid(), qPrintable(where + QStringLiteral("조사구역을 열지 못했습니다")));
        QCOMPARE(area->featureCount(), areaCount);
        QCOMPARE(QgsProject::instance()->layoutManager()->layouts().size(), layouts);
        const QStringList missing = SurveyBundle::missingFileLayers(QgsProject::instance());
        for (const QString& name : layers) {
          QgsMapLayer* layer = layerNamed(name);
          if (!layer) {
            QStringList present;
            for (QgsMapLayer* l : QgsProject::instance()->mapLayers()) present << l->name();
            present.sort();
            QVERIFY2(layer, qPrintable(where + name + QStringLiteral(" 레이어가 없어졌습니다. 지금 있는 것: ") +
                                       present.join(QStringLiteral(", "))));
          }
          if (external.contains(name) && !layer->isValid()) {
            QVERIFY2(missing.contains(name), qPrintable(where + name + QStringLiteral(" 가 없다는 알림이 없습니다")));
            continue;
          }
          QVERIFY2(layer->isValid(), qPrintable(where + name + QStringLiteral(" 레이어를 열지 못했습니다: ") +
                                                layer->source()));
        }
      };
      {
        MainWindow window;
        disableRendering(window);
        QVERIFY2(window.openSurveyGpkg(survey), qPrintable(version + QStringLiteral(": 조사를 열지 못했습니다")));
        QCoreApplication::processEvents();
        check(QStringLiteral("처음 열기"));
        if (QTest::currentTestFailed()) return;
        // 예전 판 조사를 새 판에서 열자마자 EPSG:5179 제출 꾸러미를 만들 수 있어야 한다
        // (2026-10-10 현장 보고: 예전 판에서 저장한 조사가 새 판에서 5179로 내보내지지 않는다).
        // 표본에는 기준점·유구가 없어 검수는 원래 막힌다(그 판에서도 같은 error 규칙). 여기서는 꾸러미 쓰기만 본다.
        {
          QTemporaryDir out;
          QVERIFY(out.isValid());
          QString submitError;
          const QString package = ExportService::exportSubmissionPackage(
              QgsProject::instance(), QDir(out.path()).filePath(QStringLiteral("제출")), QStringLiteral("UTF-8"),
              QString(), /*blockOnError=*/true, /*hasChecklistErrors=*/false, &submitError);
          QVERIFY2(!package.isEmpty(), qPrintable(version + QStringLiteral(": 제출 꾸러미를 만들지 못했습니다: ") + submitError));
          QgsVectorLayer shp(QDir(package).filePath(QStringLiteral("survey_area.shp")), QStringLiteral("submitted"),
                             QStringLiteral("ogr"));
          QVERIFY2(shp.isValid(), qPrintable(version + QStringLiteral(": survey_area.shp 가 없습니다")));
          QCOMPARE(shp.crs().authid(), QStringLiteral("EPSG:5179"));
          QCOMPARE(shp.featureCount(), areaCount);
        }
        // 새 판에서 이어서 작업하고 저장한 뒤 다시 열어도 그대로여야 한다.
        QVERIFY2(saveNow(window), qPrintable(version + QStringLiteral(": 새 판으로 저장하지 못했습니다")));
        QgsProject::instance()->setDirty(false);
      }
      MainWindow window;
      disableRendering(window);
      QVERIFY(window.openSurveyGpkg(survey));
      QCoreApplication::processEvents();
      check(QStringLiteral("새 판 저장 뒤 다시 열기"));
      if (QTest::currentTestFailed()) return;
      QgsProject::instance()->setDirty(false);
    }
  }

  // F141: samples/ holds practice surveys a new user opens by hand (never at startup). They
  // must open like any survey, in EPSG:5186, with the documented contents, and the bad sample
  // must really fail the submit checklist (samples/*/README.md).
  void samplesOpenAsSurveys() {
    QString samples = QStringLiteral("samples");
    if (!QDir(samples).exists()) samples = QFINDTESTDATA("../samples");
    QVERIFY2(QDir(samples).exists(), "samples 폴더가 없습니다");
    QString rules = QStringLiteral("data/rules/drawing_checklist.v1.json");
    if (!QFileInfo::exists(rules)) rules = QFINDTESTDATA("../data/rules/drawing_checklist.v1.json");
    struct Expect {
      QString file;
      QList<QPair<QString, qint64>> counts;
      QStringList mustPass;
      QStringList mustFail;
    };
    const QList<Expect> expects{
        {QStringLiteral("demo_survey/demo.gpkg"),
         {{QStringLiteral("survey_area"), 1}, {QStringLiteral("feature_poly"), 2}, {QStringLiteral("feature_line"), 1},
          {QStringLiteral("section_line"), 1}, {QStringLiteral("control_points"), 2},
          {QStringLiteral("artifact_point"), 1}, {QStringLiteral("trial_trench"), 1}},
         {QStringLiteral("GCP_MIN_TWO"), QStringLiteral("GCP_DATUM_META"), QStringLiteral("GCP_ELLIPSOID_META"),
          QStringLiteral("GCP_PROJECTION_META"), QStringLiteral("FEATURE_LEGEND_FIELDS"),
          QStringLiteral("GEOMETRY_VALID"), QStringLiteral("SURVEY_POLYGON_ONLY")},
         {}},
        {QStringLiteral("bad_survey/bad.gpkg"),
         {{QStringLiteral("survey_area"), 1}, {QStringLiteral("feature_poly"), 1}, {QStringLiteral("control_points"), 0}},
         {QStringLiteral("GEOMETRY_VALID")},
         {QStringLiteral("GCP_MIN_TWO"), QStringLiteral("FEATURE_LEGEND_FIELDS")}},
    };
    ChecklistEngine engine;
    QVERIFY2(engine.loadRules(rules), qPrintable(rules));
    for (const Expect& expect : expects) {
      // Work on a copy: opening and closing may touch the file, the sample must stay as shipped.
      QTemporaryDir work;
      QVERIFY(work.isValid());
      const QString copy = work.filePath(QFileInfo(expect.file).fileName());
      QVERIFY2(QFile::copy(QDir(samples).filePath(expect.file), copy), qPrintable(expect.file));
      QFile::setPermissions(copy, QFile::permissions(copy) | QFileDevice::WriteOwner);
      MainWindow window;
      disableRendering(window);
      QVERIFY2(window.openSurveyGpkg(copy), qPrintable(expect.file + QStringLiteral(": 조사로 열리지 않습니다")));
      QCoreApplication::processEvents();
      for (const auto& [key, count] : expect.counts) {
        auto* layer = LayerOps::findByLayerKey(QgsProject::instance(), key);
        if (count == 0) {
          QVERIFY2(!layer || layer->featureCount() == 0, qPrintable(expect.file + QStringLiteral(" ") + key));
          continue;
        }
        QVERIFY2(layer && layer->isValid(), qPrintable(expect.file + QStringLiteral(": ") + key + QStringLiteral(" 없음")));
        QCOMPARE(layer->featureCount(), count);
        QCOMPARE(layer->crs().authid(), QStringLiteral("EPSG:5186"));
      }
      const QVector<CheckResult> results = engine.evaluate(ProjectStateBuilder::fromProject(QgsProject::instance()));
      const auto passed = [&results](const QString& id) {
        for (const CheckResult& result : results)
          if (result.id == id) return result.passed;
        return false;
      };
      for (const QString& id : expect.mustPass)
        QVERIFY2(passed(id), qPrintable(expect.file + QStringLiteral(": ") + id + QStringLiteral(" 가 통과해야 합니다")));
      for (const QString& id : expect.mustFail)
        QVERIFY2(!passed(id), qPrintable(expect.file + QStringLiteral(": ") + id + QStringLiteral(" 가 막아야 합니다")));
      QgsProject::instance()->setDirty(false);
    }
  }

  void saveAndReopen_keepsTreeGeometryAttributesAndStyle() {
    const QString path = makeSurvey(QStringLiteral("왕복조사"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    QVERIFY(tree);
    auto* originalModel = tree->layerTreeModel();
    QVERIFY(window.openSurveyGpkg(path));
    auto* layer = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"));
    QVERIFY(layer && layer->isValid());
    QVERIFY(QgsProject::instance()->layerTreeRoot()->findLayer(layer->id()));
    QVERIFY(layer->startEditing());
    const auto id = *layer->allFeatureIds().constBegin();
    QVERIFY(layer->changeAttributeValue(id, layer->fields().indexOf(QStringLiteral("survey_name")),
                                        QStringLiteral("수정한 조사")));
    QVERIFY(QMetaObject::invokeMethod(&window, "saveProject", Qt::DirectConnection));
    QVERIFY(window.openSurveyGpkg(path));
    layer = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"));
    QVERIFY(layer && layer->isValid());
    QCOMPARE(layer->featureCount(), 1LL);
    QCOMPARE(layer->getFeature(id).attribute(QStringLiteral("survey_name")).toString(),
             QStringLiteral("수정한 조사"));
    QVERIFY(layer->getFeature(id).geometry().equals(
        QgsGeometry::fromRect(QgsRectangle(190000, 560000, 190100, 560100)))) ;
    auto* node = QgsProject::instance()->layerTreeRoot()->findLayer(layer->id());
    QVERIFY2(node, "Saved data must have a legend node after reopening");
    QVERIFY(node->isVisible());
    QCOMPARE(tree->layerTreeModel(), originalModel);
    auto* renderer = dynamic_cast<QgsSingleSymbolRenderer*>(layer->renderer());
    QVERIFY(renderer);
    QCOMPARE(renderer->symbol()->color(), QColor(217, 43, 43));
    QCOMPARE(QgsProject::instance()->crs().authid(), QStringLiteral("EPSG:5187"));
  }
  void saveReopenSubmit_preservesWorkAndPackage_data() {
    QTest::addColumn<QString>("authId");
    QTest::newRow("5186") << QStringLiteral("EPSG:5186");
    QTest::newRow("5187") << QStringLiteral("EPSG:5187");
  }
  void saveReopenSubmit_preservesWorkAndPackage() {
    QFETCH(QString, authId);
    const QString name = QStringLiteral("왕복제출%1").arg(authId.mid(5));
    const QString path = makeRoundTripSurvey(name, authId);
    QVERIFY2(!path.isEmpty(), "synthetic save-reopen-submit survey");
    const QString referencePath = m_files.filePath(name + QStringLiteral("-ref.shp"));
    QVERIFY(QFile::exists(referencePath));

    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    auto* project = QgsProject::instance();
    QCOMPARE(project->crs().authid(), authId);
    if (auto* canvas = window.findChild<QgsMapCanvas*>())
      QCOMPARE(canvas->mapSettings().destinationCrs().authid(), authId);

    auto* surveyArea = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    auto* featurePoly = LayerOps::findByLayerKey(project, QStringLiteral("feature_poly"));
    auto* featureLine = LayerOps::findByLayerKey(project, QStringLiteral("feature_line"));
    auto* control = LayerOps::findByLayerKey(project, QStringLiteral("control_points"));
    auto* artifact = LayerOps::findByLayerKey(project, QStringLiteral("artifact_point"));
    QVERIFY(surveyArea && featurePoly && featureLine && control && artifact);
    QCOMPARE(surveyArea->crs().authid(), authId);
    QCOMPARE(featurePoly->crs().authid(), authId);
    QCOMPARE(LayerOps::layerKeyOf(surveyArea), QStringLiteral("survey_area"));
    QVERIFY(!LayerOps::isReferenceLayer(surveyArea));
    QCOMPARE(surveyArea->featureCount(), 1LL);
    QCOMPARE(featurePoly->featureCount(), 1LL);
    QCOMPARE(featureLine->featureCount(), 1LL);
    QCOMPARE(control->featureCount(), 2LL);
    QCOMPARE(artifact->featureCount(), 1LL);

    auto* reference = ensureFileReference(project, referencePath);
    QVERIFY2(reference && reference->isValid(), "file-backed reference must stay in the open survey");
    LayerOps::markReferenceLayer(reference);
    QVERIFY(LayerOps::isReferenceLayer(reference));
    QCOMPARE(QFileInfo(reference->source().section(QLatin1Char('|'), 0, 0)).absoluteFilePath(),
             QFileInfo(referencePath).absoluteFilePath());
    QVERIFY(!reference->source().contains(path));
    if (auto* lineNode = project->layerTreeRoot()->findLayer(featureLine->id()))
      lineNode->setItemVisibilityChecked(false);
    if (!LayoutService::isComposedStudioSheet(project))
      QVERIFY(addComposedUserSheet(project, surveyArea, surveyArea->extent()));
    auto* lineNode = project->layerTreeRoot()->findLayer(featureLine->id());
    QVERIFY(lineNode);
    QVERIFY(!lineNode->itemVisibilityChecked());
    auto* areaNode = project->layerTreeRoot()->findLayer(surveyArea->id());
    QVERIFY(areaNode && areaNode->itemVisibilityChecked());
    QVERIFY(LayoutService::isComposedStudioSheet(project));

    const QgsFeature beforeArea = featureByField(surveyArea, {QStringLiteral("survey_name")},
                                                 QStringLiteral("왕복조사"));
    const QgsFeature beforePoly = featureByField(featurePoly, {QStringLiteral("feature_no")},
                                                 QStringLiteral("1"));
    const QgsFeature beforeLine = featureByField(featureLine, {QStringLiteral("kind")},
                                                 QStringLiteral("경계"));
    const QgsFeature beforeG1 = featureByField(control, {QStringLiteral("point_id")},
                                               QStringLiteral("G1"));
    const QgsFeature beforeArt = featureByField(artifact, {QStringLiteral("artifact_no")},
                                                QStringLiteral("A-1"));
    QVERIFY(beforeArea.isValid() && beforePoly.isValid() && beforeLine.isValid());
    QVERIFY(beforeG1.isValid() && beforeArt.isValid());
    QCOMPARE(beforeArea.attribute(QStringLiteral("site_name")).toString(), QStringLiteral("테스트유적"));
    QCOMPARE(beforePoly.attribute(QStringLiteral("kind")).toString(), QStringLiteral("수혈"));
    QCOMPARE(beforePoly.attribute(QStringLiteral("period")).toString(), QStringLiteral("청동기"));
    QCOMPARE(beforeArt.attribute(QStringLiteral("kind")).toString(), QStringLiteral("토기"));

    const QString pkgBefore = m_files.filePath(name + QStringLiteral("-pkg-a"));
    QString exportError;
    QVERIFY2(!ExportService::exportSubmissionPackage(project, pkgBefore, QStringLiteral("UTF-8"),
                                                    QStringLiteral("OK"), false, false, &exportError).isEmpty(),
             qPrintable(exportError));
    QVERIFY(QFile::exists(QDir(pkgBefore).filePath(QStringLiteral("조사도면.pdf"))));
    QVERIFY(QFileInfo(QDir(pkgBefore).filePath(QStringLiteral("조사도면.pdf"))).size() > 500);

    QVERIFY(saveNow(window));
    QVERIFY(window.openSurveyGpkg(path));
    project = QgsProject::instance();
    QCOMPARE(project->crs().authid(), authId);
    surveyArea = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    featurePoly = LayerOps::findByLayerKey(project, QStringLiteral("feature_poly"));
    featureLine = LayerOps::findByLayerKey(project, QStringLiteral("feature_line"));
    control = LayerOps::findByLayerKey(project, QStringLiteral("control_points"));
    artifact = LayerOps::findByLayerKey(project, QStringLiteral("artifact_point"));
    QVERIFY(surveyArea && featurePoly && featureLine && control && artifact);
    QCOMPARE(surveyArea->featureCount(), 1LL);
    QCOMPARE(featurePoly->featureCount(), 1LL);
    QCOMPARE(featureLine->featureCount(), 1LL);
    QCOMPARE(control->featureCount(), 2LL);
    QCOMPARE(artifact->featureCount(), 1LL);
    QCOMPARE(surveyArea->crs().authid(), authId);
    QCOMPARE(LayerOps::layerKeyOf(surveyArea), QStringLiteral("survey_area"));
    QVERIFY(!LayerOps::isReferenceLayer(surveyArea));
    lineNode = project->layerTreeRoot()->findLayer(featureLine->id());
    QVERIFY(lineNode);
    QVERIFY2(!lineNode->itemVisibilityChecked(), "hidden feature_line must stay hidden");
    QVERIFY(LayoutService::isComposedStudioSheet(project));

    reference = findExternalReference(project, referencePath);
    QVERIFY2(reference && reference->isValid() && LayerOps::isReferenceLayer(reference),
             "file-backed reference must survive save and reopen");
    QCOMPARE(QFileInfo(reference->source().section(QLatin1Char('|'), 0, 0)).absoluteFilePath(),
             QFileInfo(referencePath).absoluteFilePath());
    QVERIFY2(!reference->source().contains(path), "external SHP must not be absorbed into the survey GPKG");
    QCOMPARE(reference->featureCount(), 1LL);

    const QgsFeature afterArea = featureByField(surveyArea, {QStringLiteral("survey_name")},
                                                QStringLiteral("왕복조사"));
    const QgsFeature afterPoly = featureByField(featurePoly, {QStringLiteral("feature_no")},
                                                QStringLiteral("1"));
    const QgsFeature afterLine = featureByField(featureLine, {QStringLiteral("kind")},
                                                QStringLiteral("경계"));
    const QgsFeature afterG1 = featureByField(control, {QStringLiteral("point_id")},
                                              QStringLiteral("G1"));
    const QgsFeature afterArt = featureByField(artifact, {QStringLiteral("artifact_no")},
                                               QStringLiteral("A-1"));
    QVERIFY(afterArea.isValid() && afterPoly.isValid() && afterLine.isValid());
    QVERIFY(afterG1.isValid() && afterArt.isValid());
    QCOMPARE(afterArea.attribute(QStringLiteral("site_name")).toString(), QStringLiteral("테스트유적"));
    QCOMPARE(afterPoly.attribute(QStringLiteral("kind")).toString(), QStringLiteral("수혈"));
    QCOMPARE(afterPoly.attribute(QStringLiteral("period")).toString(), QStringLiteral("청동기"));
    QCOMPARE(afterArt.attribute(QStringLiteral("artifact_no")).toString(), QStringLiteral("A-1"));
    QVERIFY2(verticesWithinMm(afterArea.geometry(), beforeArea.geometry()), "survey_area moved");
    QVERIFY2(verticesWithinMm(afterPoly.geometry(), beforePoly.geometry()), "feature_poly moved");
    QVERIFY2(verticesWithinMm(afterLine.geometry(), beforeLine.geometry()), "feature_line moved");
    QVERIFY2(verticesWithinMm(afterG1.geometry(), beforeG1.geometry()), "control G1 moved");
    QVERIFY2(verticesWithinMm(afterArt.geometry(), beforeArt.geometry()), "artifact moved");

    const QString pkgAfter = m_files.filePath(name + QStringLiteral("-pkg-b"));
    QVERIFY2(!ExportService::exportSubmissionPackage(project, pkgAfter, QStringLiteral("UTF-8"),
                                                    QStringLiteral("OK"), false, false, &exportError).isEmpty(),
             qPrintable(exportError));
    const QString pdfBefore = QDir(pkgBefore).filePath(QStringLiteral("조사도면.pdf"));
    const QString pdfAfter = QDir(pkgAfter).filePath(QStringLiteral("조사도면.pdf"));
    QVERIFY(QFile::exists(pdfAfter));
    QVERIFY(QFileInfo(pdfAfter).size() > 500);
    const qint64 pdfSizeBefore = QFileInfo(pdfBefore).size();
    const qint64 pdfSizeAfter = QFileInfo(pdfAfter).size();
    QVERIFY2(qAbs(pdfSizeBefore - pdfSizeAfter) <= qMax(qint64(4096), pdfSizeBefore / 10),
             qPrintable(QStringLiteral("PDF size %1 vs %2").arg(pdfSizeBefore).arg(pdfSizeAfter)));
    QVERIFY(!QFile::exists(QDir(pkgAfter).filePath(QStringLiteral("외부경계.shp"))));

    const QStringList shpNames = {
      QStringLiteral("survey_area.shp"), QStringLiteral("feature_poly.shp"),
      QStringLiteral("feature_line.shp"), QStringLiteral("control_points.shp"),
      QStringLiteral("artifact_point.shp")
    };
    for (const QString& shpName : shpNames) {
      const QString beforePath = QDir(pkgBefore).filePath(shpName);
      const QString afterPath = QDir(pkgAfter).filePath(shpName);
      QVERIFY2(QFile::exists(beforePath) && QFile::exists(afterPath), qPrintable(shpName));
      QgsVectorLayer beforeLayer(beforePath, shpName + QStringLiteral("-a"), QStringLiteral("ogr"));
      QgsVectorLayer afterLayer(afterPath, shpName + QStringLiteral("-b"), QStringLiteral("ogr"));
      QVERIFY2(beforeLayer.isValid() && afterLayer.isValid(), qPrintable(shpName));
      QCOMPARE(beforeLayer.crs().authid(), QStringLiteral("EPSG:5179"));
      QCOMPARE(afterLayer.crs().authid(), QStringLiteral("EPSG:5179"));
      QCOMPARE(beforeLayer.featureCount(), afterLayer.featureCount());
    }

    QgsVectorLayer areaShp(QDir(pkgAfter).filePath(QStringLiteral("survey_area.shp")),
                           QStringLiteral("sa"), QStringLiteral("ogr"));
    QgsVectorLayer polyShp(QDir(pkgAfter).filePath(QStringLiteral("feature_poly.shp")),
                           QStringLiteral("fp"), QStringLiteral("ogr"));
    QgsVectorLayer artShp(QDir(pkgAfter).filePath(QStringLiteral("artifact_point.shp")),
                          QStringLiteral("ap"), QStringLiteral("ogr"));
    const QgsFeature shpArea = featureByField(&areaShp, {QStringLiteral("surv_name"), QStringLiteral("survey_name")},
                                              QStringLiteral("왕복조사"));
    const QgsFeature shpPoly = featureByField(&polyShp, {QStringLiteral("feature_no")}, QStringLiteral("1"));
    const QgsFeature shpArt = featureByField(&artShp, {QStringLiteral("artif_no"), QStringLiteral("artifact_no")},
                                             QStringLiteral("A-1"));
    QVERIFY(shpArea.isValid() && shpPoly.isValid() && shpArt.isValid());
    QCOMPARE(fieldText(shpArea, {QStringLiteral("surv_name"), QStringLiteral("survey_name")}),
             QStringLiteral("왕복조사"));
    QCOMPARE(shpPoly.attribute(QStringLiteral("kind")).toString(), QStringLiteral("수혈"));
    QCOMPARE(fieldText(shpArt, {QStringLiteral("artif_no"), QStringLiteral("artifact_no")}),
             QStringLiteral("A-1"));
    const QgsGeometry area5179 = to5179(afterArea.geometry(), surveyArea->crs(), project);
    const QgsGeometry poly5179 = to5179(afterPoly.geometry(), featurePoly->crs(), project);
    const QgsGeometry art5179 = to5179(afterArt.geometry(), artifact->crs(), project);
    QVERIFY2(verticesWithinMm(shpArea.geometry(), area5179),
             qPrintable(QStringLiteral("survey_area SHP hausdorff=%1")
                            .arg(shpArea.geometry().hausdorffDistance(area5179), 0, 'f', 6)));
    QVERIFY2(verticesWithinMm(shpPoly.geometry(), poly5179),
             qPrintable(QStringLiteral("feature_poly SHP hausdorff=%1")
                            .arg(shpPoly.geometry().hausdorffDistance(poly5179), 0, 'f', 6)));
    QVERIFY2(verticesWithinMm(shpArt.geometry(), art5179),
             qPrintable(QStringLiteral("artifact SHP hausdorff=%1")
                            .arg(shpArt.geometry().hausdorffDistance(art5179), 0, 'f', 6)));
  }
  void open_repairsRegistryOnlyLayers() {
    const QString path = makeSurvey(QStringLiteral("목록복구"), true);
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    auto* layer = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"));
    QVERIFY(layer && layer->isValid());
    QCOMPARE(layer->featureCount(), 1LL);
    QVERIFY2(QgsProject::instance()->layerTreeRoot()->findLayer(layer->id()),
             "Registry-only survey must be restored to the legend");
    QVERIFY(window.findChild<QgsMapCanvas*>()->layers().contains(layer));
  }
  void startupHome_ignoresRememberedSurveyUntilUserOpensIt() {
    const QString path = makeSurvey(QStringLiteral("수동열기"));
    QVERIFY(!path.isEmpty());
    QSettings st = RecentSurveys::userSettings();
    QVERIFY2(QFileInfo(st.fileName()).absoluteFilePath().startsWith(
                 QFileInfo(s_testSettingsPath).absoluteFilePath()),
             qPrintable(QStringLiteral("RecentSurveys settings escaped test dir: %1")
                            .arg(st.fileName())));
    RecentSurveys::remember(st, path, QStringLiteral("수동열기"));

    MainWindow window;
    window.show();
    QTest::qWait(250);

    auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("viewTabs"));
    QVERIFY(tabs);
    QCOMPARE(tabs->tabText(tabs->currentIndex()), QStringLiteral("홈"));
    auto* canvas = window.findChild<QgsMapCanvas*>(QStringLiteral("mapCanvas"));
    QVERIFY(canvas);
    QCOMPARE(QgsProject::instance()->mapLayers().size(), 0);
    QCOMPARE(canvas->layers().size(), 0);
    const QString screenshot = qEnvironmentVariable("KA_HGIS_STARTUP_SCREENSHOT");
    if (!screenshot.isEmpty())
      QVERIFY(window.grab().save(screenshot));

    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    QCOMPARE(tabs->tabText(tabs->currentIndex()), QStringLiteral("지도"));
    QVERIFY(LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area")));
  }
  // 도면 만들기의 「인쇄」는 PDF 내보내기와 같은 도면을 만들어 인쇄 창에 넘기고,
  // 인쇄 창은 큰 도면을 작은 용지 여러 장으로 나눠 PDF 쪽마다 한 장씩 쓴다.
  void drawingStudioPrintSplitsTheDrawingIntoSheets() {
    MainWindow window;
    disableRendering(window);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.show();
    auto* canvas = window.findChild<QgsMapCanvas*>(QStringLiteral("mapCanvas"));
    QVERIFY(canvas);
    canvas->setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    canvas->setExtent(QgsRectangle(190000., 560000., 191000., 561000.));
    QTimer acceptPaper;
    acceptPaper.setInterval(10);
    connect(&acceptPaper, &QTimer::timeout, &window, [&] {
      auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
      if (dialog && dialog->windowTitle() == QStringLiteral("용지 설정")) {
        acceptPaper.stop();
        dialog->accept();
      }
    });
    acceptPaper.start();
    QVERIFY(QMetaObject::invokeMethod(&window, "openLayoutDesigner"));
    acceptPaper.stop();
    QCoreApplication::processEvents();
    auto* studio = window.findChild<KaDrawingStudio*>();
    QVERIFY(studio);
    auto* printButton = studio->findChild<QToolButton*>(QStringLiteral("btnPrint"));
    QVERIFY2(printButton, "도면 만들기에 인쇄 단추가 없습니다.");

    QTemporaryDir out;
    QVERIFY(out.isValid());
    const QString tiles = out.filePath(QStringLiteral("tiles.pdf"));
    bool opened = false;
    int sheets = 0;
    int pages = 0;
    QString drawingLabel;
    QString error;
    QTimer handle;
    handle.setInterval(20);
    connect(&handle, &QTimer::timeout, &window, [&] {
      auto* dialog = qobject_cast<KaPrintDialog*>(QApplication::activeModalWidget());
      if (!dialog) return;
      handle.stop();
      opened = true;
      drawingLabel = dialog->outputLabels().value(0);
      dialog->setTiled(true);
      dialog->setSheet(KaPrintDialog::SheetA4);
      dialog->setOutput(KaPrintDialog::OutputA2);
      if (dialog->plan().ok && dialog->saveTilesPdf(tiles, &error)) {
        sheets = int(dialog->plan().sheets.size());
        pages = dialog->pageCount();
      }
      dialog->reject();
    });
    handle.start();
    printButton->click();
    handle.stop();
    QVERIFY2(opened, "인쇄 창이 열리지 않았습니다.");
    QVERIFY2(sheets > 1, qPrintable(error));
    // 도면 지도의 축척이 인쇄 창까지 넘어와 「도면 크기 그대로 · 1:…」로 보인다.
    QVERIFY2(drawingLabel.contains(QStringLiteral("1:")), qPrintable(drawingLabel));
    QVERIFY(pages >= sheets);
    QPdfDocument doc;
    QCOMPARE(doc.load(tiles), QPdfDocument::Error::None);
    QCOMPARE(doc.pageCount(), pages);
  }
  // 리본의 「인쇄」(Ctrl+P)는 도면 만들기를 열고 곧바로 인쇄 창을 띄운다.
  void ribbonPrintOpensTheDrawingThenThePrintWindow() {
    MainWindow window;
    disableRendering(window);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.show();
    auto* canvas = window.findChild<QgsMapCanvas*>(QStringLiteral("mapCanvas"));
    QVERIFY(canvas);
    canvas->setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    canvas->setExtent(QgsRectangle(190000., 560000., 191000., 561000.));
    auto* ribbonPrint = window.findChild<QToolButton*>(QStringLiteral("btnRibbonPrint"));
    QVERIFY2(ribbonPrint, "리본에 인쇄 단추가 없습니다.");
    QVERIFY(ribbonPrint->defaultAction());
    QCOMPARE(ribbonPrint->defaultAction()->shortcut(), QKeySequence(QKeySequence::Print));

    bool paperAsked = false;
    bool opened = false;
    QTimer handle;
    handle.setInterval(10);
    connect(&handle, &QTimer::timeout, &window, [&] {
      auto* modal = QApplication::activeModalWidget();
      if (auto* print = qobject_cast<KaPrintDialog*>(modal)) {
        handle.stop();
        opened = true;
        print->reject();
        return;
      }
      auto* dialog = qobject_cast<QDialog*>(modal);
      if (dialog && dialog->windowTitle() == QStringLiteral("용지 설정")) {
        paperAsked = true;
        dialog->accept();
      }
    });
    handle.start();
    ribbonPrint->click();
    handle.stop();
    QVERIFY2(paperAsked, "도면 만들기의 용지 설정을 거치지 않았습니다.");
    QVERIFY2(opened, "리본 인쇄로 인쇄 창이 열리지 않았습니다.");
    auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("viewTabs"));
    QVERIFY(tabs);
    QVERIFY2(qobject_cast<KaDrawingStudio*>(tabs->currentWidget()), "인쇄 뒤 도면 만들기 탭이 보이지 않습니다.");
  }
  // 축척 칸은 하나뿐이어야 한다. 예전에는 자유 입력 QLineEdit 과 프리셋 QComboBox 가
  // 따로 있어서 같은 축척인데도 어느 쪽으로 넣었느냐에 따라 화면이 달랐다.
  void scaleControlFollowsActiveDrawingWithoutChangingMapCanvas() {
    MainWindow window;
    disableRendering(window);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.show();
    auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("viewTabs"));
    auto* canvas = window.findChild<QgsMapCanvas*>(QStringLiteral("mapCanvas"));
    auto* combo = window.findChild<QComboBox*>(QStringLiteral("scaleCombo"));
    QVERIFY(tabs && canvas && combo && combo->lineEdit());
    QWidget* mapPage = nullptr;
    for (int i = 0; i < tabs->count(); ++i)
      if (tabs->tabText(i) == QStringLiteral("지도")) mapPage = tabs->widget(i);
    QVERIFY(mapPage);
    tabs->setCurrentWidget(mapPage);
    QCoreApplication::processEvents();
    canvas->setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    canvas->setExtent(QgsRectangle(190000., 560000., 191000., 561000.));
    canvas->zoomScale(25000.);

    QTimer acceptPaper;
    acceptPaper.setInterval(10);
    connect(&acceptPaper, &QTimer::timeout, &window, [&] {
      auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
      if (dialog && dialog->windowTitle() == QStringLiteral("용지 설정")) {
        acceptPaper.stop();
        dialog->accept();
      }
    });
    acceptPaper.start();
    QVERIFY(QMetaObject::invokeMethod(&window, "openLayoutDesigner"));
    acceptPaper.stop();
    QCoreApplication::processEvents();
    auto* studio = window.findChild<KaDrawingStudio*>();
    QVERIFY(studio && tabs->currentWidget() == studio);
    auto* view = studio->findChild<QgsLayoutView*>();
    QVERIFY(view && view->currentLayout());
    auto* map = dynamic_cast<QgsLayoutItemMap*>(view->currentLayout()->itemById(QStringLiteral("ka_map")));
    auto* paperScale = dynamic_cast<QgsLayoutItemLabel*>(view->currentLayout()->itemById(QStringLiteral("ka_scale")));
    auto* drawingScale = studio->findChild<QSpinBox*>(QStringLiteral("drawingScale"));
    auto* apply = studio->findChild<QPushButton*>(QStringLiteral("scaleApply"));
    QVERIFY(map && paperScale && drawingScale && apply);
    const QgsRectangle canvasExtent = canvas->extent();
    const double canvasScale = canvas->scale();
    const auto bottomScale = [&] { return MainWindow::scaleDenominatorFromUi(combo->lineEdit()->text()); };

    // The drawing's existing Apply control must update the shared bottom field.
    drawingScale->setValue(10000);
    apply->click();
    QVERIFY(qAbs(map->scale() - 10000.) < .5);
    QCOMPARE(bottomScale(), 10000.);
    QCOMPARE(canvas->extent(), canvasExtent);
    QCOMPARE(canvas->scale(), canvasScale);

    // Enter in that bottom field now targets the active drawing, not the hidden map.
    combo->lineEdit()->setText(QStringLiteral("1:5000"));
    QTest::keyClick(combo->lineEdit(), Qt::Key_Return);
    QVERIFY(qAbs(map->scale() - 5000.) < .5);
    QCOMPARE(drawingScale->value(), 5000);
    QCOMPARE(paperScale->text(), QStringLiteral("축척 1 : 5000"));
    QCOMPARE(bottomScale(), 5000.);
    QCOMPARE(canvas->extent(), canvasExtent);
    QCOMPARE(canvas->scale(), canvasScale);

    // A canvas notification while its tab is hidden must not overwrite drawing scale.
    QVERIFY(QMetaObject::invokeMethod(&window, "onCanvasScaleChanged", Q_ARG(double, canvasScale)));
    QCOMPARE(bottomScale(), 5000.);
    // Frame resizing reports the actual map scale to both labels.
    map->attemptResize(QgsLayoutSize(map->rect().width() * .8, map->rect().height()));
    QCoreApplication::processEvents();
    const double resizedScale = std::round(map->scale());
    QCOMPARE(bottomScale(), resizedScale);
    QCOMPARE(drawingScale->value(), static_cast<int>(resizedScale));
    QCOMPARE(paperScale->text(), QStringLiteral("축척 1 : %1").arg(resizedScale, 0, 'f', 0));

    tabs->setCurrentWidget(mapPage);
    QCoreApplication::processEvents();
    QCOMPARE(bottomScale(), std::round(canvas->scale()));
    tabs->setCurrentWidget(studio);
    QCoreApplication::processEvents();
    QCOMPARE(bottomScale(), resizedScale);
    QVERIFY(qAbs(studio->drawingScale() - resizedScale) < .5);
    QgsProject::instance()->setDirty(false);
  }

  void scaleControl_isSingleWidgetAcceptingBothForms() {
    MainWindow window;
    disableRendering(window);
    auto* combo = window.findChild<QComboBox*>(QStringLiteral("scaleCombo"));
    QVERIFY2(combo, "축척 콤보가 없다");
    QVERIFY2(combo->isEditable(), "축척 콤보는 직접 입력이 돼야 한다");
    QVERIFY2(combo->lineEdit(), "콤보에 입력줄이 있어야 한다");

    // 입력줄은 콤보의 것 하나뿐 — 따로 떠 있는 축척 칸이 있으면 안 된다.
    int standalone = 0;
    for (auto* e : window.findChildren<QLineEdit*>(QStringLiteral("scaleEdit")))
      if (e != combo->lineEdit()) ++standalone;
    QCOMPARE(standalone, 0);

    // 발굴 도면 축척이 프리셋에 있어야 한다.
    QVERIFY2(combo->findData(200) >= 0, "1:200 프리셋이 없다");
    QVERIFY2(combo->findData(500) >= 0, "1:500 프리셋이 없다");

    // "2000" 과 "1:2000" 이 같은 값으로 읽혀야 한 칸으로 합친 의미가 있다.
    QCOMPARE(MainWindow::scaleDenominatorFromUi(QStringLiteral("2000")), 2000.0);
    QCOMPARE(MainWindow::scaleDenominatorFromUi(QStringLiteral("1:2000")), 2000.0);
    QCOMPARE(MainWindow::scaleDenominatorFromUi(QStringLiteral(" 1 : 2,000 ")), 2000.0);
    QCOMPARE(MainWindow::scaleDenominatorFromUi(QStringLiteral("1:200")), 200.0);
    QCOMPARE(MainWindow::scaleDenominatorFromUi(QStringLiteral("메롱")), 0.0);
    QCOMPARE(MainWindow::scaleDenominatorFromUi(QString()), 0.0);
  }

  void open_blocksNestedOpenAndAutosave() {
    const QString first = makeSurvey(QStringLiteral("이전조사"));
    const QString second = makeSurvey(QStringLiteral("다음조사"));
    QVERIFY(!first.isEmpty() && !second.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(first));
    const QByteArray before = contents(first);
    QVERIFY(!before.isEmpty());
    bool invoked = false;
    bool nestedOpened = true;
    // 주기적으로 파일에 쓰는 타이머가 남아 있으면 안 된다.
    QVERIFY2(hasNoAutosaveTimer(window), "20초 자동 저장 타이머가 아직 살아 있다");
    bool saveDuringRead = true;
    const auto connection = connect(QgsProject::instance(), &QgsProject::readProject,
        &window, [&](const QDomDocument&) {
          if (invoked) return;
          invoked = true;
          saveDuringRead = saveNow(window);
          nestedOpened = window.openSurveyGpkg(first);
        });
    const bool opened = window.openSurveyGpkg(second);
    disconnect(connection);
    QVERIFY(invoked);
    QVERIFY2(!saveDuringRead, "프로젝트를 읽는 중에는 저장이 끼어들면 안 된다");
    QVERIFY(opened);
    QVERIFY2(!nestedOpened, "A nested open must not replace a project being read");
    QCOMPARE(contents(first), before);
    QgsProject savedFirst;
    QVERIFY(savedFirst.read(SurveyStorage::projectUri(first),
                            Qgis::ProjectReadFlag::DontResolveLayers | Qgis::ProjectReadFlag::DontLoadLayouts));
    QCOMPARE(savedFirst.title(), QStringLiteral("이전조사"));
    auto* layer = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"));
    QVERIFY(layer);
    QCOMPARE(layer->getFeature(*layer->allFeatureIds().constBegin())
             .attribute(QStringLiteral("survey_name")).toString(), QStringLiteral("다음조사"));
  }
  void partialCommit_preservesRemainingBufferAndCreatesIndependentRecovery() {
    const QString path = makeSurvey(QStringLiteral("뒤레이어커밋실패"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    auto* project = QgsProject::instance();
    QString error;
    auto* line = LayerOps::ensureDomainLayer(project, path, QStringLiteral("feature_line"),
                                             QStringLiteral("실패검사 선"), &error);
    QVERIFY2(line, qPrintable(error));
    QVERIFY(line->startEditing());
    QgsFeature feature(line->fields());
    feature.setAttribute(QStringLiteral("note"), QStringLiteral("기존 선"));
    feature.setGeometry(QgsGeometry::fromWkt(QStringLiteral("LineString (190000 560000, 190010 560010)")));
    QVERIFY(line->addFeature(feature));
    QVERIFY(saveNow(window));
    auto* first = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    auto* failed = LayerOps::findByLayerKey(project, QStringLiteral("feature_line"));
    QVERIFY(first && failed && first != failed);
    QMap<QString, QgsFeatureId> ids;
    int changeIndex = 0;
    for (auto* layer : {first, failed}) {
      if (!layer->isEditable()) QVERIFY(layer->startEditing());
      const QgsFeatureId id = *layer->allFeatureIds().constBegin();
      ids.insert(layer->id(), id);
      QVERIFY(layer->changeAttributeValue(id, layer->fields().indexOf(QStringLiteral("note")),
                                          QStringLiteral("미저장 변경 %1").arg(changeIndex++)));
    }
    failed->setAllowCommit(false);
    project->setTitle(QStringLiteral("전체 저장은 아직 실패"));
    const QString companion = QFileInfo(path).dir().filePath(
        QFileInfo(path).completeBaseName() + QStringLiteral(".qgz"));
    const QByteArray companionBefore = contents(companion);
    const auto gpkgFiles = [&]() {
      QSet<QString> files;
      QDirIterator iterator(m_files.path(), {QStringLiteral("*.gpkg")}, QDir::Files,
                            QDirIterator::Subdirectories);
      while (iterator.hasNext()) files.insert(iterator.next());
      return files;
    };
    const QSet<QString> beforeFiles = gpkgFiles();
    QVERIFY(!saveNow(window));
    QVERIFY(project->isDirty());
    QVERIFY(!first->isModified());
    QVERIFY(failed->isEditable() && failed->isModified());
    QCOMPARE(contents(companion), companionBefore);
    const QList<QgsVectorLayer*> order{first, failed};
    for (int i = 0; i < order.size(); ++i) {
      auto* layer = order.at(i);
      QCOMPARE(layer->getFeature(ids.value(layer->id())).attribute(QStringLiteral("note")).toString(),
               QStringLiteral("미저장 변경 %1").arg(i));
      QgsVectorLayer disk(layer->source(), QStringLiteral("디스크 확인"), QStringLiteral("ogr"));
      QVERIFY(disk.isValid());
      const QString saved = disk.getFeature(ids.value(layer->id())).attribute(QStringLiteral("note")).toString();
      if (i == 0) QCOMPARE(saved, QStringLiteral("미저장 변경 0"));
      else QVERIFY(saved != QStringLiteral("미저장 변경 1"));
    }
    const QSet<QString> created = gpkgFiles() - beforeFiles;
    QCOMPARE(created.size(), 1);
    const QString recovery = *created.constBegin();
    QVERIFY(!contents(recovery).isEmpty());
    QgsProject recovered;
    QVERIFY(recovered.read(SurveyStorage::projectUri(recovery)));
    for (int i = 0; i < order.size(); ++i) {
      auto* copied = LayerOps::findByLayerKey(&recovered, LayerOps::layerKeyOf(order.at(i)));
      QVERIFY(copied && copied->isValid());
      QCOMPARE(copied->featureCount(), 1LL);
      QgsFeature value;
      QVERIFY(copied->getFeatures().nextFeature(value));
      QCOMPARE(value.attribute(QStringLiteral("note")).toString(), QStringLiteral("미저장 변경 %1").arg(i));
    }
    recovered.clear();
    // 기준 바이트는 복구본을 읽은 뒤에 잡는다. 읽기만 해도 SQLite 가 WAL 을 정리해
    // 파일 헤더(저널 모드·변경 카운터)가 바뀐다. 읽기 전 값과 비교하면 항상 다르다.
    const QByteArray recoveryBefore = contents(recovery);
    QVERIFY(!recoveryBefore.isEmpty());
    auto* bar = window.findChild<QgsMessageBar*>();
    QVERIFY(bar);
    bool recoveryWarning = false;
    for (auto* item : bar->items())
      if (item->level() == Qgis::MessageLevel::Warning &&
          item->text().contains(QStringLiteral("복구"))) recoveryWarning = true;
    QVERIFY(recoveryWarning);
    QVERIFY(!saveNow(window)); // Persistent failure creates another snapshot, never overwrites.
    QCOMPARE((gpkgFiles() - beforeFiles).size(), 2);
    QCOMPARE(contents(recovery), recoveryBefore);
    QVERIFY(failed->isModified());
    failed->setAllowCommit(true);
    QVERIFY(saveNow(window));
    QVERIFY(window.openSurveyGpkg(path));
    const QStringList keys{QStringLiteral("survey_area"), QStringLiteral("feature_line")};
    for (int i = 0; i < keys.size(); ++i) {
      auto* reopened = LayerOps::findByLayerKey(project, keys.at(i));
      QVERIFY(reopened && reopened->isValid());
      QCOMPARE(reopened->featureCount(), 1LL);
      QgsFeature value;
      QVERIFY(reopened->getFeatures().nextFeature(value));
      QCOMPARE(value.attribute(QStringLiteral("note")).toString(), QStringLiteral("미저장 변경 %1").arg(i));
    }
  }


  void failedCommit_doesNotOverwriteSavedWorkspace() {
    const QString path = makeSurvey(QStringLiteral("커밋실패"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    auto* project = QgsProject::instance();
    auto* layer = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QVERIFY(layer);
    QVERIFY(layer->startEditing());
    QVERIFY(layer->changeAttributeValue(*layer->allFeatureIds().constBegin(),
        layer->fields().indexOf(QStringLiteral("survey_name")), QStringLiteral("저장실패한 편집")));
    layer->setAllowCommit(false);
    project->setTitle(QStringLiteral("커밋에 실패하면 이 작업공간도 쓰지 않는다"));
    const QByteArray before = contents(path);
    QVERIFY(!before.isEmpty());
    QTimer dismiss;
    connect(&dismiss, &QTimer::timeout, [] {
      for (auto* widget : QApplication::topLevelWidgets())
        if (auto* message = qobject_cast<QMessageBox*>(widget)) message->accept();
    });
    dismiss.start(20);
    QVERIFY(QMetaObject::invokeMethod(&window, "saveProject", Qt::DirectConnection));
    dismiss.stop();
    const QByteArray after = contents(path);
    const bool stillModified = layer->isModified();
    layer->setAllowCommit(true);
    layer->rollBack();
    QVERIFY2(stillModified, "A failed save must retain the edit buffer for retry");
    QgsProject saved;
    QVERIFY(saved.read(SurveyStorage::projectUri(path),
                       Qgis::ProjectReadFlag::DontResolveLayers | Qgis::ProjectReadFlag::DontLoadLayouts));
    QCOMPARE(saved.title(), QStringLiteral("커밋실패"));
    QCOMPARE(after, before);
  }
  void invalidSurvey_doesNotReplaceCurrentWork_data() {
    QTest::addColumn<QByteArray>("invalidContents");
    QTest::newRow("corrupt-file") << QByteArray("not a GeoPackage");
    QTest::newRow("other-vector-format") << QByteArray(
        R"({"type":"FeatureCollection","features":[{"type":"Feature","properties":{},"geometry":{"type":"Point","coordinates":[127,37]}}]})");
  }
  void emptyGpkgWithoutWorkspace_opensWithoutDomainLayers() {
    QString error;
    const QString path = SurveyProjectFactory::createNewSurvey(
        m_files.path(), QStringLiteral("빈조사"), &error, QStringLiteral("EPSG:5187"));
    QVERIFY2(!path.isEmpty(), qPrintable(error));
    // Exercise the on-disk schema alone, without the factory's companion layer tree.
    const QFileInfo file(path);
    QVERIFY(QFile::remove(file.dir().filePath(file.completeBaseName() + QStringLiteral(".qgz"))));
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    QCOMPARE(window.domainLayerCount(), 0);
  }
  void invalidSurvey_doesNotReplaceCurrentWork() {
    QFETCH(QByteArray, invalidContents);
    const QString path = makeSurvey(QStringLiteral("유지할조사_") + QString::fromLatin1(QTest::currentDataTag()));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    const QStringList layerIds = QgsProject::instance()->mapLayers().keys();
    QgsProject::instance()->setTitle(QStringLiteral("열기 실패 후 유지한 작업"));
    const QString invalid = m_files.filePath(QStringLiteral("잘못된.gpkg"));
    QFile file(invalid);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(invalidContents), invalidContents.size());
    file.close();
    QTimer dismiss;
    connect(&dismiss, &QTimer::timeout, [] {
      for (auto* widget : QApplication::topLevelWidgets())
        if (auto* message = qobject_cast<QMessageBox*>(widget)) message->accept();
    });
    dismiss.start(20);
    const bool opened = window.openSurveyGpkg(invalid);
    dismiss.stop();
    QVERIFY2(!opened, "Invalid survey must not be reported as opened");
    QCOMPARE(QgsProject::instance()->mapLayers().keys(), layerIds);
    QCOMPARE(QgsProject::instance()->title(), QStringLiteral("열기 실패 후 유지한 작업"));
    QVERIFY(saveNow(window));
    QgsProject saved;
    QVERIFY(saved.read(SurveyStorage::projectUri(path),
                        Qgis::ProjectReadFlag::DontResolveLayers | Qgis::ProjectReadFlag::DontLoadLayouts));
    QCOMPARE(saved.title(), QStringLiteral("열기 실패 후 유지한 작업"));
    QCOMPARE(contents(invalid), invalidContents);
    QgsProject::instance()->setTitle(QStringLiteral("열기 실패 후 수동 저장"));
    QVERIFY(QMetaObject::invokeMethod(&window, "saveProject", Qt::DirectConnection));
    QVERIFY(saved.read(SurveyStorage::projectUri(path),
                        Qgis::ProjectReadFlag::DontResolveLayers | Qgis::ProjectReadFlag::DontLoadLayouts));
    QCOMPARE(saved.title(), QStringLiteral("열기 실패 후 수동 저장"));
    QCOMPARE(contents(invalid), invalidContents);
  }
  void openWhileDrawing_resetsCaptureBeforeReplacingLayers() {
    const QString first = makeSurvey(QStringLiteral("그리던조사"));
    const QString second = makeSurvey(QStringLiteral("전환한조사"));
    QVERIFY(!first.isEmpty() && !second.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(first));
    auto* project = QgsProject::instance();
    QVERIFY(QMetaObject::invokeMethod(&window, "startEditFeaturePoly", Qt::DirectConnection));
    QPointer<QgsVectorLayer> oldLayer = LayerOps::findByLayerKey(project, QStringLiteral("feature_poly"));
    QVERIFY(oldLayer);
    auto* capture = window.findChild<KaCaptureMapTool*>();
    auto* canvas = window.findChild<QgsMapCanvas*>();
    QVERIFY(capture && canvas);
    QCOMPARE(canvas->mapTool(), capture);
    const QgsGeometry geometry = QgsGeometry::fromRect(QgsRectangle(190300, 560300, 190350, 560350));
    captureAndDismissForm(capture, geometry);
    QCOMPARE(oldLayer->featureCount(), 1LL);
    canvas->setRenderFlag(false);
    QVERIFY(openWithAnswer(window, second, QMessageBox::Discard));
    QVERIFY(oldLayer.isNull());
    QVERIFY2(canvas->mapTool() != capture, "Opening another survey must stop the previous drawing session");
    QCOMPARE(capture->pointCount(), 0);
    auto* layer = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QVERIFY(layer);
    captureAndDismissForm(capture, geometry); // A late capture cannot use the deleted edit layer.
    QCOMPARE(layer->featureCount(), 1LL);
    window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"))->setCurrentLayer(layer);
    QVERIFY(QMetaObject::invokeMethod(&window, "startEditFeaturePoly", Qt::DirectConnection));
    captureAndDismissForm(capture, geometry);
    auto* newDrawingLayer = LayerOps::findByLayerKey(project, QStringLiteral("feature_poly"));
    QVERIFY(newDrawingLayer);
    QCOMPARE(newDrawingLayer->featureCount(), 1LL);
    canvas->setRenderFlag(false);
  }
  void open_unsavedWorkRespectsAnswer_data() {
    QTest::addColumn<int>("answer");
    QTest::addColumn<bool>("vetoCommit");
    QTest::newRow("cancel") << int(QMessageBox::Cancel) << false;
    QTest::newRow("save-fails") << int(QMessageBox::Save) << true;
    QTest::newRow("discard") << int(QMessageBox::Discard) << false;
    QTest::newRow("save") << int(QMessageBox::Save) << false;
  }
  void open_unsavedWorkRespectsAnswer() {
    QFETCH(int, answer);
    QFETCH(bool, vetoCommit);
    const QString suffix = QString::number(answer) + QString::number(vetoCommit);
    const QString source = makeSurvey(QStringLiteral("열기전현재_") + suffix);
    const QString next = makeSurvey(QStringLiteral("열기전다음_") + suffix);
    QVERIFY(!source.isEmpty() && !next.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(source));
    auto* project = QgsProject::instance();
    QPointer<QgsVectorLayer> oldLayer = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QVERIFY(oldLayer && oldLayer->startEditing());
    const QgsFeatureId fid = *oldLayer->allFeatureIds().constBegin();
    const QgsGeometry original = oldLayer->getFeature(fid).geometry();
    QgsGeometry pending = QgsGeometry::fromRect(QgsRectangle(190010, 560011, 190121, 560122));
    QVERIFY(oldLayer->changeGeometry(fid, pending));
    const QStringList layerIds = project->mapLayers().keys();
    const QByteArray before = QCryptographicHash::hash(contents(source), QCryptographicHash::Sha256);
    if (vetoCommit) oldLayer->setAllowCommit(false);
    bool prompted = false;
    const bool opened = openWithAnswer(window, next, QMessageBox::StandardButton(answer), &prompted);
    if (oldLayer) oldLayer->setAllowCommit(true);
    QVERIFY(prompted);
    const bool shouldOpen = answer != int(QMessageBox::Cancel) && !vetoCommit;
    QCOMPARE(opened, shouldOpen);
    if (!shouldOpen) {
      QVERIFY(oldLayer && oldLayer->isEditable() && oldLayer->isModified());
      QCOMPARE(project->mapLayers().keys(), layerIds);
      QVERIFY(oldLayer->getFeature(fid).geometry().equals(pending));
      QCOMPARE(QCryptographicHash::hash(contents(source), QCryptographicHash::Sha256), before);
    } else {
      QVERIFY(oldLayer.isNull());
      auto* current = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
      QVERIFY(current && current->isValid());
      QCOMPARE(QFileInfo(current->source().section(QLatin1Char('|'), 0, 0)).absoluteFilePath(),
               QFileInfo(next).absoluteFilePath());
      QgsVectorLayer stored(source + QStringLiteral("|layername=survey_area"), QStringLiteral("저장본"), QStringLiteral("ogr"));
      QVERIFY(stored.isValid());
      QVERIFY(stored.getFeature(fid).geometry().equals(answer == int(QMessageBox::Save) ? pending : original));
    }
  }
  void saveAs_preservesActualTablesAndReopensCopy() {
    const QString path = makeSurvey(QStringLiteral("여러구역"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    auto* project = QgsProject::instance();
    QString error;
    auto* second = LayerOps::createSurveyAreaLayer(project, path, QStringLiteral("두번째 구역"),
                                                  Qt::black, QColor(25, 80, 170), 0.3, &error);
    QVERIFY2(second, qPrintable(error));
    QVERIFY(second->startEditing());
    QgsFeature feature(second->fields());
    QVERIFY(feature.setAttribute(QStringLiteral("name"), QStringLiteral("독립 구역")));
    feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(190200, 560200, 190250, 560250)));
    QVERIFY(second->addFeature(feature));
    QVERIFY(second->isModified());  // 저장하지 않은 편집은 새 파일에만 가고 원본은 그대로여야 한다(R19)
    const QString secondId = second->id();
    const QString tableOptions = second->source().section(QLatin1Char('|'), 1);
    const QString target = m_files.filePath(QStringLiteral("새 이름.gpkg"));
    QVERIFY(selectSaveAs(window, target));
    QVERIFY(QFile::exists(target) && QgsVectorLayer(path + QLatin1Char('|') + tableOptions, QStringLiteral("원본"), QStringLiteral("ogr")).featureCount() == 0);
    QCOMPARE(second->source().section(QLatin1Char('|'), 1), tableOptions);
    QCOMPARE(QFileInfo(second->source().section(QLatin1Char('|'), 0, 0)).absoluteFilePath(),
             QFileInfo(target).absoluteFilePath());
    QVERIFY(window.openSurveyGpkg(target));
    second = qobject_cast<QgsVectorLayer*>(project->mapLayer(secondId));
    QVERIFY(second && second->isValid());
    QCOMPARE(second->featureCount(), 1LL);
    QCOMPARE(second->getFeature(*second->allFeatureIds().constBegin())
                 .attribute(QStringLiteral("name")).toString(), QStringLiteral("독립 구역"));
  }
  void save_keepsDrawnSurveyArea() {
    const QString path = makeSurvey(QStringLiteral("저장후구역유지"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    auto* project = QgsProject::instance();
    QString error;
    auto* layer = LayerOps::createSurveyAreaLayer(project, path, QStringLiteral("그린 구역"),
                                                 Qt::black, QColor(180, 83, 9, 70), 1.6, &error);
    QVERIFY2(layer, qPrintable(error));
    QVERIFY(layer->startEditing());
    QgsFeature feature(layer->fields());
    feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(190400, 560400, 190480, 560480)));
    QVERIFY(layer->addFeature(feature));
    QVERIFY(layer->commitChanges(false));
    QCOMPARE(layer->featureCount(), 1LL);
    const QString table = layer->source().section(QLatin1String("layername="), 1, 1)
                              .section(QLatin1Char('|'), 0, 0);
    QVERIFY(saveNow(window));
    QCOMPARE(layer->featureCount(), 1LL);
    QgsFeature live;
    QVERIFY(layer->getFeatures().nextFeature(live));
    QVERIFY2(!live.geometry().isEmpty(), "Saved survey-area geometry must stay readable on the open layer");
    QgsVectorLayer stored(path + QStringLiteral("|layername=") + table, QStringLiteral("저장본"),
                          QStringLiteral("ogr"));
    QVERIFY(stored.isValid());
    QCOMPARE(stored.featureCount(), 1LL);
  }
  void save_fieldPackageKeepsSurveyAreaReadable() {
    const QString src = qEnvironmentVariable("KA_HGIS_REPRO_GPKG");
    if (src.isEmpty() || !QFile::exists(src))
      QSKIP("KA_HGIS_REPRO_GPKG is not set");
    const QString path = m_files.filePath(QFileInfo(src).fileName());
    QVERIFY(QFile::copy(src, path));
    MainWindow window;
    disableRendering(window);
    QVERIFY2(window.openSurveyGpkg(path), "field package must open");
    QgsVectorLayer* layer = nullptr;
    for (QgsVectorLayer* candidate : LayerOps::surveyAreaLayers(QgsProject::instance())) {
      if (candidate && candidate->featureCount() > 0) {
        layer = candidate;
        break;
      }
    }
    QVERIFY(layer);
    const long long before = layer->featureCount();
    QgsFeature seen;
    QVERIFY(layer->getFeatures().nextFeature(seen));
    QVERIFY(!seen.geometry().isEmpty());
    QVERIFY(saveNow(window));
    const QString providerError = layer->dataProvider() ? layer->dataProvider()->error().message() : QString();
    QgsFeature after;
    QVERIFY2(layer->isValid() && layer->getFeatures().nextFeature(after),
             qPrintable(QStringLiteral("source=%1 valid=%2 count=%3 err=%4")
                            .arg(layer->source())
                            .arg(layer->isValid())
                            .arg(layer->featureCount())
                            .arg(providerError)));
    QCOMPARE(layer->featureCount(), before);
    QVERIFY(!after.geometry().isEmpty());
  }
  void newSurvey_existingNamePreservesUnsavedCurrentSurvey() {
    const QString name = QStringLiteral("새조사실패보존");
    const QString path = makeSurvey(name);
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(openSettled(window, path));
    auto* project = QgsProject::instance();
    auto* layer = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QVERIFY(layer && layer->startEditing());
    const QgsFeatureId fid = *layer->allFeatureIds().constBegin();
    QgsGeometry changed = QgsGeometry::fromRect(QgsRectangle(190010, 560020, 190110, 560120));
    QVERIFY(layer->changeGeometry(fid, changed));
    const QStringList layerIds = project->mapLayers().keys();
    const QString projectTitle = project->title();
    const QString windowTitle = window.windowTitle();
    const QByteArray before = QCryptographicHash::hash(contents(path), QCryptographicHash::Sha256);
    const QString qgz = QFileInfo(path).dir().filePath(name + QStringLiteral(".qgz"));
    const QByteArray companionBefore = contents(qgz);
    QTimer choose;
    bool discarded = false;
    bool folderChosen = false;
    int nameSubmissions = 0;
    int folderSubmissions = 0;
    QStringList selectedFolders;
    QString submittedName;
    connect(&choose, &QTimer::timeout, [&] {
      auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
      if (!dialog) return;
      if (auto* question = qobject_cast<QMessageBox*>(dialog)) {
        if (question->standardButtons().testFlag(QMessageBox::Discard)) {
          discarded = true;
          question->button(QMessageBox::Discard)->click();
        } else question->accept();
      } else if (auto* folder = qobject_cast<QFileDialog*>(dialog)) {
        ++folderSubmissions;
        choose.stop();
        folderChosen = FolderDialogFixture::chooseFolder(folder, m_files.path(), &selectedFolders);
      } else if (dialog->windowTitle() == QStringLiteral("새 조사")) {
        ++nameSubmissions;
        auto* nameEdit = dialog->findChild<QLineEdit*>();
        nameEdit->setText(name);
        submittedName = nameEdit->text();
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
      }
    });
    choose.start(20);
    QTimer::singleShot(5000, &choose, [&] {
      choose.stop();
      if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) dialog->reject();
    });
    QVERIFY(QMetaObject::invokeMethod(&window, "newSurvey", Qt::DirectConnection));
    choose.stop();
    qInfo() << "New survey dialog state:" << discarded << nameSubmissions << folderSubmissions << submittedName;
    QVERIFY(discarded && folderChosen);
    QCOMPARE(nameSubmissions, 1);
    QCOMPARE(folderSubmissions, 1);
    QCOMPARE(submittedName, name);
    QCOMPARE(QFileInfo(selectedFolders.first()).canonicalFilePath(), QFileInfo(m_files.path()).canonicalFilePath());
    QCOMPARE(project->mapLayers().keys(), layerIds);
    QCOMPARE(project->title(), projectTitle);
    QCOMPARE(window.windowTitle(), windowTitle);
    QVERIFY(layer->isEditable() && layer->isModified());
    QVERIFY(layer->getFeature(fid).geometry().equals(changed));
    QCOMPARE(QCryptographicHash::hash(contents(path), QCryptographicHash::Sha256), before);
    QCOMPARE(contents(qgz), companionBefore);
  }
  void closeSave_preservesMemoryReferenceVectorOnReopen() {
    const QString path = makeSurvey(QStringLiteral("닫기저장참조벡터"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(openSettled(window, path));
    auto* project = QgsProject::instance();
    auto* reference = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5186&field=note:string"),
                                        QStringLiteral("현장참고점"), QStringLiteral("memory"));
    QVERIFY(reference->isValid());
    LayerOps::markReferenceLayer(reference);
    project->addMapLayer(reference);
    const QString referenceId = reference->id();
    QVERIFY(reference->startEditing());
    QgsFeature feature(reference->fields());
    feature.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(210000, 550000)));
    feature.setAttribute(QStringLiteral("note"), QStringLiteral("다음날 다시 볼 지점"));
    QVERIFY(reference->addFeature(feature));
    QTimer choose;
    bool prompted = false;
    connect(&choose, &QTimer::timeout, [&] {
      if (auto* question = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
        if (question->standardButtons().testFlag(QMessageBox::Save)) {
          prompted = true;
          question->button(QMessageBox::Save)->click();
        } else question->reject();
      }
    });
    choose.start(20);
    const bool closed = window.close();
    choose.stop();
    QVERIFY(prompted && closed);
    QVERIFY(openSettled(window, path));
    reference = qobject_cast<QgsVectorLayer*>(project->mapLayer(referenceId));
    QVERIFY(reference && reference->isValid());
    QVERIFY(LayerOps::isReferenceLayer(reference));
    QCOMPARE(reference->crs().authid(), QStringLiteral("EPSG:5186"));
    QCOMPARE(reference->featureCount(), 1LL);
    const QgsFeature restored = reference->getFeature(*reference->allFeatureIds().constBegin());
    QCOMPARE(restored.geometry().asPoint(), QgsPointXY(210000, 550000));
    QCOMPARE(restored.attribute(QStringLiteral("note")).toString(), QStringLiteral("다음날 다시 볼 지점"));
    QCOMPARE(QFileInfo(reference->source().section(QLatin1Char('|'), 0, 0)).absoluteFilePath(),
             QFileInfo(path).absoluteFilePath());
  }
  void saveAs_embeddedFailureRestoresSourcesAndRemainsUnsaved() {
    const QString path = makeSurvey(QStringLiteral("사본실패복귀"));
    QVERIFY(!path.isEmpty());
    // The copied database retains these triggers, rejecting only workspace writes.
    QVERIFY(executeGpkgSql(path, "CREATE TRIGGER deny_project_insert BEFORE INSERT ON qgis_projects BEGIN SELECT RAISE(ABORT, 'test workspace failure'); END"));
    QVERIFY(executeGpkgSql(path, "CREATE TRIGGER deny_project_update BEFORE UPDATE ON qgis_projects BEGIN SELECT RAISE(ABORT, 'test workspace failure'); END"));
    QVERIFY(executeGpkgSql(path, "CREATE TRIGGER deny_project_delete BEFORE DELETE ON qgis_projects BEGIN SELECT RAISE(ABORT, 'test workspace failure'); END"));
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    auto* project = QgsProject::instance();
    auto* layer = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QVERIFY(layer);
    const QString source = layer->source();
    const QString projectFile = project->fileName();
    const QString projectHome = project->presetHomePath();
    auto* reference = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5186&field=note:string"),
                                        QStringLiteral("실패후에도남을참고점"), QStringLiteral("memory"));
    QVERIFY(reference->isValid());
    LayerOps::markReferenceLayer(reference);
    project->addMapLayer(reference);
    const QString referenceId = reference->id();
    QVERIFY(reference->startEditing());
    QgsFeature point(reference->fields());
    point.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(210010, 550020)));
    point.setAttribute(QStringLiteral("note"), QStringLiteral("보존할 메모"));
    QVERIFY(reference->addFeature(point));
    project->setTitle(QStringLiteral("사본 실패 뒤에도 저장할 작업"));
    project->setDirty(true);
    const QString target = m_files.filePath(QStringLiteral("실패할사본.gpkg"));
    QVERIFY(selectSaveAs(window, target));
    QVERIFY(QFile::exists(target));
    QVERIFY(layer->isValid());
    QCOMPARE(layer->source(), source);
    QCOMPARE(project->fileName(), projectFile);
    QCOMPARE(project->presetHomePath(), projectHome);
    QVERIFY(project->isDirty());
    QCOMPARE(project->mapLayer(referenceId), reference);
    QVERIFY(reference->isValid() && reference->isEditable());
    QCOMPARE(reference->providerType(), QStringLiteral("memory"));
    QCOMPARE(reference->crs().authid(), QStringLiteral("EPSG:5186"));
    QCOMPARE(reference->featureCount(), 1LL);
    const QgsFeature restoredPoint = reference->getFeature(*reference->allFeatureIds().constBegin());
    QCOMPARE(restoredPoint.geometry().asPoint(), QgsPointXY(210010, 550020));
    QCOMPARE(restoredPoint.attribute(QStringLiteral("note")).toString(), QStringLiteral("보존할 메모"));
    QVERIFY(LayerOps::isReferenceLayer(reference));
    QVERIFY(executeGpkgSql(path, "DROP TRIGGER deny_project_insert"));
    QVERIFY(executeGpkgSql(path, "DROP TRIGGER deny_project_update"));
    QVERIFY(executeGpkgSql(path, "DROP TRIGGER deny_project_delete"));
    QVERIFY(saveNow(window));
    QgsProject saved;
    QVERIFY(saved.read(SurveyStorage::projectUri(path), Qgis::ProjectReadFlag::DontResolveLayers));
    QCOMPARE(saved.title(), QStringLiteral("사본 실패 뒤에도 저장할 작업"));
    QVERIFY(!project->isDirty());
  }
  void saveAs_existingDestinationIsNotOverwritten_data() {
    QTest::addColumn<bool>("companionOnly");
    QTest::newRow("existing-gpkg") << false;
    QTest::newRow("existing-qgz") << true;
  }
  void saveAs_existingDestinationIsNotOverwritten() {
    QFETCH(bool, companionOnly);
    const QString source = makeSurvey(QStringLiteral("덮어쓰기방지원본_%1").arg(companionOnly));
    QVERIFY(!source.isEmpty());
    const QString target = m_files.filePath(QStringLiteral("기존자료_%1.gpkg").arg(companionOnly));
    const QString existing = companionOnly
        ? QFileInfo(target).dir().filePath(QFileInfo(target).completeBaseName() + QStringLiteral(".qgz"))
        : target;
    const QString sourceQgz = QFileInfo(source).dir().filePath(QFileInfo(source).completeBaseName() + QStringLiteral(".qgz"));
    QVERIFY(QFile::copy(companionOnly ? sourceQgz : source, existing));
    const QByteArray before = QCryptographicHash::hash(contents(existing), QCryptographicHash::Sha256);
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(source));
    auto* project = QgsProject::instance();
    auto* layer = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QVERIFY(layer && layer->startEditing());
    const QString originalSource = layer->source();
    const QgsFeatureId fid = *layer->allFeatureIds().constBegin();
    QgsGeometry pending = QgsGeometry::fromRect(QgsRectangle(190001, 560002, 190111, 560112));
    QVERIFY(layer->changeGeometry(fid, pending));
    QVERIFY(selectSaveAs(window, target));
    QCOMPARE(QCryptographicHash::hash(contents(existing), QCryptographicHash::Sha256), before);
    QCOMPARE(layer->source(), originalSource);
    QVERIFY(layer->isModified());
    QVERIFY(layer->getFeature(fid).geometry().equals(pending));
    if (companionOnly) QVERIFY(!QFileInfo::exists(target));
    auto* bar = window.findChild<QgsMessageBar*>();
    QVERIFY(bar);
    bool warned = false;
    for (auto* item : bar->items())
      if (item->level() == Qgis::MessageLevel::Warning && item->text().contains(QStringLiteral("이름"))) warned = true;
    QVERIFY(warned);
  }
  void save_qgisExceptionStaysInsideSlot_data() {
    QTest::addColumn<bool>("saveAs");
    QTest::newRow("persist") << false;
    QTest::newRow("save-as") << true;
  }
  void save_qgisExceptionStaysInsideSlot() {
    QFETCH(bool, saveAs);
    const QString source = makeSurvey(QStringLiteral("예외보존_%1").arg(saveAs));
    QVERIFY(!source.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(source));
    auto* project = QgsProject::instance();
    auto* layer = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QVERIFY(layer);
    const QString originalSource = layer->source();
    const QString originalProjectFile = project->fileName();
    const QByteArray before = QCryptographicHash::hash(contents(source), QCryptographicHash::Sha256);
    project->setTitle(QStringLiteral("예외 뒤에도 남을 제목"));
    project->setDirty(true);
    bool injected = false;
    const auto connection = connect(project, &QgsProject::writeProject, &window,
        [&](QDomDocument&) {
          injected = true;
          throw QgsException(QStringLiteral("synthetic non-std write exception"));
        }, Qt::DirectConnection);
    bool escaped = false;
    bool result = false;
    try {
      result = saveAs ? selectSaveAs(window, m_files.filePath(QStringLiteral("예외사본.gpkg")))
                      : saveNow(window);
    } catch (...) {
      escaped = true;
    }
    disconnect(connection);
    QVERIFY2(!escaped, "QGIS exceptions must not escape the save slot into the GUI event loop");
    QVERIFY(injected);
    if (saveAs) QVERIFY(result);
    else QVERIFY(!result);
    QCOMPARE(layer->source(), originalSource);
    QCOMPARE(project->fileName(), originalProjectFile);
    QVERIFY(project->isDirty());
    QCOMPARE(project->title(), QStringLiteral("예외 뒤에도 남을 제목"));
    QCOMPARE(QCryptographicHash::hash(contents(source), QCryptographicHash::Sha256), before);
    auto* bar = window.findChild<QgsMessageBar*>();
    QVERIFY(bar);
    bool warned = false;
    for (auto* item : bar->items())
      if (item->level() == Qgis::MessageLevel::Warning && item->text().contains(QStringLiteral("다시 저장"))) warned = true;
    QVERIFY(warned);
    QVERIFY(saveNow(window));
  }
  void save_companionFailureKeepsEmbeddedWorkspaceAndWarns() {
    const QString name = QStringLiteral("보조사본실패");
    const QString path = makeSurvey(name);
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    const QString companion = QFileInfo(path).dir().filePath(name + QStringLiteral(".qgz"));
    QVERIFY(QFile::remove(companion));
    QVERIFY(QDir().mkdir(companion)); // A directory cannot be atomically replaced by a QGZ file.
    auto* project = QgsProject::instance();
    project->setTitle(QStringLiteral("내장 구성은 저장됨"));
    QVERIFY(saveNow(window));
    QVERIFY(QFileInfo(companion).isDir());
    QgsProject saved;
    QVERIFY(saved.read(SurveyStorage::projectUri(path), Qgis::ProjectReadFlag::DontResolveLayers));
    QCOMPARE(saved.title(), QStringLiteral("내장 구성은 저장됨"));
    auto* bar = window.findChild<QgsMessageBar*>();
    QVERIFY(bar);
    bool warned = false;
    for (auto* item : bar->items()) {
      if (item->level() == Qgis::MessageLevel::Warning && item->text().contains(QLatin1String("QGZ")) &&
          item->text().contains(QStringLiteral("다시 저장"))) warned = true;
    }
    QVERIFY2(warned, "A failed companion write must show an actionable warning even after GPKG success");
    QVERIFY(!project->isDirty());
  }
  void fieldCopy_restoresAllValidLayersAndSavesTree() {
    const QString input = qEnvironmentVariable("KA_HGIS_FIELD_GPKG");
    if (input.isEmpty()) QSKIP("Set KA_HGIS_FIELD_GPKG to a disposable field-file copy");
    const QString copy = m_files.filePath(QFileInfo(input).fileName());
    QVERIFY(QFile::copy(input, copy));
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(copy));
    auto* project = QgsProject::instance();
    QMap<QString, qint64> counts;
    for (auto* layer : project->mapLayers()) {
      if (!layer->isValid()) continue;
      QVERIFY2(project->layerTreeRoot()->findLayer(layer->id()), qPrintable(layer->name()));
      if (auto* vector = qobject_cast<QgsVectorLayer*>(layer)) {
        QCOMPARE(QFileInfo(vector->source().section(QLatin1Char('|'), 0, 0)).canonicalFilePath(),
                 QFileInfo(copy).canonicalFilePath());
        counts.insert(vector->id(), vector->featureCount());
      }
    }
    QVERIFY(!counts.isEmpty());
    QVERIFY(QMetaObject::invokeMethod(&window, "saveProject", Qt::DirectConnection));
    QVERIFY(window.openSurveyGpkg(copy));
    for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
      auto* vector = qobject_cast<QgsVectorLayer*>(project->mapLayer(it.key()));
      QVERIFY(vector && vector->isValid());
      QCOMPARE(vector->featureCount(), it.value());
      QVERIFY(project->layerTreeRoot()->findLayer(vector->id()));
    }
    qInfo() << "Field copy restored vector layers:" << counts.size()
            << "registry:" << project->mapLayers().size()
            << "legend:" << project->layerTreeRoot()->findLayers().size();
    const QString screenshot = qEnvironmentVariable("KA_HGIS_FIELD_SCREENSHOT");
    if (!screenshot.isEmpty()) {
      window.show();
      window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"))->expandAll();
      QApplication::processEvents();
      QVERIFY(window.grab().save(screenshot));
    }
  }
};

static QByteArray localCadastralCapabilities() {
  return QByteArray(R"(<WMS_Capabilities version="1.3.0" xmlns="http://www.opengis.net/wms" xmlns:xlink="http://www.w3.org/1999/xlink">
<Service><Name>WMS</Name><Title>Offline save/open fixture</Title></Service>
<Capability><Request>
<GetCapabilities><Format>text/xml</Format><DCPType><HTTP><Get><OnlineResource xlink:href="https://save-open.invalid/wms"/></Get></HTTP></DCPType></GetCapabilities>
<GetMap><Format>image/png</Format><DCPType><HTTP><Get><OnlineResource xlink:href="https://save-open.invalid/wms"/></Get></HTTP></DCPType></GetMap>
</Request><Layer><Title>Local cadastral fixture</Title>
<CRS>EPSG:3857</CRS><CRS>EPSG:5186</CRS><CRS>EPSG:5187</CRS><CRS>EPSG:4326</CRS>
<EX_GeographicBoundingBox><westBoundLongitude>124</westBoundLongitude><eastBoundLongitude>132</eastBoundLongitude><southBoundLatitude>32</southBoundLatitude><northBoundLatitude>40</northBoundLatitude></EX_GeographicBoundingBox>
<BoundingBox CRS="EPSG:3857" minx="13500000" miny="3500000" maxx="14900000" maxy="4900000"/>
<BoundingBox CRS="EPSG:5186" minx="-100000" miny="-100000" maxx="700000" maxy="1000000"/>
<BoundingBox CRS="EPSG:5187" minx="-100000" miny="-100000" maxx="700000" maxy="1000000"/>
<BoundingBox CRS="EPSG:4326" minx="32" miny="124" maxx="40" maxy="132"/>
<Layer queryable="0"><Name>lp_pa_cbnd_bonbun</Name><Title>Local parcels</Title><Style><Name>lp_pa_cbnd_bonbun</Name><Title>Local parcels</Title></Style></Layer>
<Layer queryable="0"><Name>lp_pa_cbnd_bubun</Name><Title>Local parcel labels</Title><Style><Name>lp_pa_cbnd_bubun</Name><Title>Local parcel labels</Title></Style></Layer>
</Layer></Capability></WMS_Capabilities>)");
}

int main(int argc, char** argv) {
  // Run directly (not through ctest), the app code would log into the user's own
  // session log. Keep test logs in a temporary folder instead.
  if (qEnvironmentVariableIsEmpty("KA_HGIS_LOG_DIR"))
    qputenv("KA_HGIS_LOG_DIR", QDir::temp().filePath(QStringLiteral("ka-hgis-test-logs")).toUtf8());
  QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
  QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
  // GDAL WMS uses libcurl outside QGIS's request preprocessor. Keep its remote
  // requests offline too; these options are scoped to this test process.
  CPLSetConfigOption("GDAL_HTTP_PROXY", "127.0.0.1:1");
  CPLSetConfigOption("GDAL_HTTP_TIMEOUT", "1");
  QgsApplication app(argc, argv, true);
#ifdef Q_OS_WIN
  if (QGuiApplication::platformName() == QLatin1String("offscreen")) {
    // Match the installed field font: offscreen Qt does not discover it itself.
    const QDir windows(qEnvironmentVariable("WINDIR"));
    for (const QString& file : {QStringLiteral("malgun.ttf"), QStringLiteral("malgunbd.ttf")}) {
      const QString path = windows.filePath(QStringLiteral("Fonts/") + file);
      if (QFontDatabase::addApplicationFont(path) < 0) {
        qCritical().noquote() << "Could not load the installed QA font:" << path;
        return 1;
      }
    }
  }
#endif
  QTemporaryDir settings;
  const QString capabilitiesPath = settings.filePath(QStringLiteral("cadastral-capabilities.xml"));
  const QString mapPath = settings.filePath(QStringLiteral("white-map.png"));
  QFile capabilities(capabilitiesPath);
  const QByteArray capabilitiesXml = localCadastralCapabilities();
  if (!capabilities.open(QIODevice::WriteOnly) || capabilities.write(capabilitiesXml) != capabilitiesXml.size())
    return 1;
  capabilities.close();
  QImage mapImage(512, 512, QImage::Format_RGB32);
  mapImage.fill(Qt::white);
  if (!mapImage.save(mapPath)) return 1;
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  // Save/open fixtures use local rasters. Incidental basemap refreshes must not
  // depend on external servers; network behavior has its own dedicated suites.
  const QUrl capabilitiesUrl = QUrl::fromLocalFile(capabilitiesPath);
  const QUrl mapUrl = QUrl::fromLocalFile(mapPath);
  const QString networkIsolation = QgsNetworkAccessManager::setRequestPreprocessor([capabilitiesUrl, mapUrl](QNetworkRequest* request) {
    if (!request) return;
    const QUrl url = request->url();
    const QString scheme = url.scheme().toLower();
    const QString host = url.host().toLower();
    if ((scheme == QLatin1String("http") || scheme == QLatin1String("https")) &&
        host != QLatin1String("localhost") && host != QLatin1String("127.0.0.1") &&
        host != QLatin1String("::1")) {
      bool capabilitiesRequest = false;
      for (const auto& item : QUrlQuery(url).queryItems())
        if (item.first.compare(QLatin1String("request"), Qt::CaseInsensitive) == 0 &&
            item.second.compare(QLatin1String("GetCapabilities"), Qt::CaseInsensitive) == 0)
          capabilitiesRequest = true;
      // Valid local capabilities avoid the GDAL failure fallback. The entire URL
      // is replaced so credentials from remote query/path stay private.
      request->setUrl(capabilitiesRequest ? capabilitiesUrl : mapUrl);
    }
  });
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
  s_testSettingsPath = settings.path();
  KaTheme::apply(&app);
  TestSaveOpen tests;
  const int result = QTest::qExec(&tests, argc, argv);
  QgsApplication::exitQgis();
  QgsNetworkAccessManager::removeRequestPreprocessor(networkIsolation);
  return result;
}

#include "test_save_open.moc"

