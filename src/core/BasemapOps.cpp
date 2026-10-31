#include "KaSessionLog.h"
#include "KaLogExcept.h"
#include "LayerOps.h"
#include "LayerOpsInternal.h"
#include "ReferenceKind.h"
#include "BasemapPolicy.h"
#include "BasemapDsm.h"
#include "BasemapExtentCache.h"
#include "LayerLabelControls.h"
#include "DemPresentation.h"
#include "DemColorRampLegend.h"
#include "GeorefService.h"
#include "SoilMapService.h"
#include "VworldSettings.h"
#include "KaPortableRuntime.h"

#include <QSignalBlocker>
#include <QDateTime>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTextStream>
#include <QStringConverter>
#include <QRegularExpression>
#include <cmath>
#include <memory>
#include <limits>
#include <algorithm>
#include <QPainter>
#include <QScreen>
#include <QSize>
#include <QUrl>
#include <QWindow>
#include <QColor>
#include <QFont>
#include <QDir>
#include <QDomDocument>
#include <QUrlQuery>
#include <QSet>
#include <QTemporaryFile>
#include <QPointer>
#include <QScopedValueRollback>
#include <QTimer>
#include <QHash>
#include <functional>
#include <QNetworkRequest>

#include <qgis.h>
#include <QUndoStack>
#include <qgsproject.h>
#include <qgssnappingconfig.h>
#include <qgsvectorlayer.h>
#include <qgsrasterlayer.h>
#include <qgsbrightnesscontrastfilter.h>
#include <qgsmapcanvas.h>
#include <qgsvectorfilewriter.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransformcontext.h>
#include <qgscoordinatetransform.h>
#include <qgsexception.h>
#include <qgsfield.h>
#include <qgsfields.h>
#include <qgsfeature.h>
#include <qgsfeaturerequest.h>
#include <qgsfeatureiterator.h>
#include <qgsgeometry.h>
#include <qgspoint.h>
#include <qgspointxy.h>
#include <qgslinestring.h>
#include <qgscategorizedsymbolrenderer.h>
#include <qgssinglesymbolrenderer.h>
#include <qgsinvertedpolygonrenderer.h>
#include <qgssymbol.h>
#include <qgssymbollayer.h>
#include <qgsfillsymbol.h>
#include <qgsfillsymbollayer.h>
#include <qgslinesymbol.h>
#include <qgslinesymbollayer.h>
#include <qgsmarkersymbol.h>
#include <qgsrenderer.h>
#include <qgsrectangle.h>
#include <qgslayertree.h>
#include <qgslayertreenode.h>
#include <qgslayertreelayer.h>
#include <qgsbilinearrasterresampler.h>
#include <qgsrasterresamplefilter.h>
#include <qgsrasterdataprovider.h>
#include <qgsvectordataprovider.h>
#include <qgsrasterrenderer.h>
#include <qgsrastertransparency.h>
#include <qgssinglebandpseudocolorrenderer.h>
#include <qgsrastershader.h>
#include <qgscolorrampshader.h>
#include <qgscolorramplegendnodesettings.h>
#include <qgshillshaderenderer.h>
#include <qgsrasterbandstats.h>
#include <cpl_conv.h>
#include <cpl_error.h>
#include <gdal.h>
#include <gdal_utils.h>
#include <ogr_api.h>
#include <qgsnetworkaccessmanager.h>
#include <qgslayertreegroup.h>
#include <qgsdataprovider.h>
#include <qgsprojectviewsettings.h>
#include <qgspallabeling.h>
#include <qgsvectorlayerlabeling.h>
#include <qgstextformat.h>
#include <qgslabelobstaclesettings.h>
#include <qgsreferencedgeometry.h>

static void applyKaNetworkHeaders(QNetworkRequest* req) {
  if (!req) return;
  req->setHeader(QNetworkRequest::UserAgentHeader,
                 QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) ka-hgis/0.3 QGIS"));
  // VWorld authenticates keys by Referer; no other provider is sent one (F059).
  const QString referer = BasemapPolicy::refererForUrl(req->url().toString());
  if (!referer.isEmpty())
    req->setRawHeader("Referer", referer.toLatin1());
  const QUrl fixed = SoilMapService::rewriteArcGisCacheUrl(req->url());
  if (fixed != req->url())
    req->setUrl(fixed);
}

void LayerOps::ensureTileNetworkIdentity() {
  static bool once = false;
  if (once) return;
  once = true;
  QgsNetworkAccessManager::instance()->setCacheDisabled(false);
  QgsNetworkAccessManager::setRequestPreprocessor(&applyKaNetworkHeaders);
}

static bool uriLooksLikeXyz(const QString& source) {
  return source.contains(QLatin1String("type=xyz"), Qt::CaseInsensitive);
}

// QGIS leaves a block blank when it needs more than 256 tiles. With maxOversampling 2 a
// 2000 px PDF piece asked for 4000 px, so a 300 DPI 1919 조선지형도 sheet lost its first piece.
static void limitTileOversampling(QgsRasterLayer* rl) {
  if (!rl || rl->providerType() != QLatin1String("wms") || !rl->resampleFilter()) return;
  const QString src = rl->source();
  if (uriLooksLikeXyz(src) || src.contains(QLatin1String("tileMatrixSet="), Qt::CaseInsensitive))
    rl->resampleFilter()->setMaxOversampling(1.0);
}

static void tuneBasemapLayer(QgsRasterLayer* rl, bool crispText = false) {
  if (!rl || !rl->isValid()) return;
  rl->setBlendMode(QPainter::CompositionMode_SourceOver);
  const QString src = rl->source();
  if (src.contains(QLatin1String("crs=EPSG:4326"), Qt::CaseInsensitive)) {
    rl->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4326")));
  } else if (uriLooksLikeXyz(src) || src.contains(QLatin1String("vworld-cadastral"), Qt::CaseInsensitive) ||
             src.contains(QLatin1String("crs=EPSG:3857"), Qt::CaseInsensitive) ||
             src.contains(QLatin1String("crs=EPSG:900913"), Qt::CaseInsensitive)) {
    rl->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:3857")));
  } else if (!rl->crs().isValid()) {
    rl->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:3857")));
  }
  if (QgsRasterResampleFilter* rf = rl->resampleFilter()) {
    if (crispText) {
      rf->setZoomedInResampler(nullptr);
      rf->setZoomedOutResampler(nullptr);
    } else {
      rf->setZoomedInResampler(new QgsBilinearRasterResampler());
      rf->setZoomedOutResampler(new QgsBilinearRasterResampler());
    }
  }
  limitTileOversampling(rl);
  if (crispText) {
    if (QgsRasterDataProvider* dp = rl->dataProvider()) {
      dp->setDpi(192);
      dp->setZoomedInResamplingMethod(Qgis::RasterResamplingMethod::Nearest);
      dp->setZoomedOutResamplingMethod(Qgis::RasterResamplingMethod::Nearest);
    }
  }
}

static void zoomCanvasToWorkingScale(QgsMapCanvas* canvas, const QString& crsAuthId,
                                     double targetScale = 50000.0) {
  if (!canvas) return;
  const QString auth = crsAuthId.trimmed().isEmpty() ? QStringLiteral("EPSG:5186") : crsAuthId;
  const QgsRectangle cur = canvas->extent();
  const bool hasLocalView = !cur.isEmpty() && cur.isFinite() && cur.width() > 0 && cur.height() > 0 &&
                            canvas->scale() > 50.0 && canvas->scale() < 500000.0;
  if (!hasLocalView)
    LayerOps::zoomToKorea(canvas, auth);
  if (targetScale > 0.0)
    canvas->zoomScale(targetScale, true);
  LayerOps::clampCanvasToKorea(canvas);
}

void LayerOps::knockOutRasterPaper(QgsRasterLayer* layer) {
  if (!layer || !layer->isValid()) return;
  if (isBasemapLayer(layer)) return;
  // 내장 GT가 있는 GeoTIFF를 맞추기 대기로 숨기면 지도에는 보여도 조판에서 빠진다.
  if (isAlignPending(layer) && !GeorefService::looksUnreferencedRaster(layer))
    setAlignPending(layer, false);
  if (layer->bandCount() < 3) return;
  QgsRasterRenderer* rend = layer->renderer();
  if (!rend) return;
  if (const QgsRasterTransparency* old = rend->rasterTransparency()) {
    const auto list = old->transparentThreeValuePixelList();
    for (const auto& px : list) {
      if (std::abs(px.red - 255.0) < 1e-6 && px.opacity < 0.01)
        return;
    }
  }
  auto* trans = new QgsRasterTransparency();
  QVector<QgsRasterTransparency::TransparentThreeValuePixel> whites;
  whites.append(QgsRasterTransparency::TransparentThreeValuePixel(255, 255, 255, 0.0, 16, 16, 16));
  trans->setTransparentThreeValuePixelList(whites);
  rend->setRasterTransparency(trans);
  layer->triggerRepaint();
}

void LayerOps::limitTileOversampling(QgsProject* project) {
  if (!project) return;
  for (QgsMapLayer* ml : project->mapLayers())
    ::limitTileOversampling(qobject_cast<QgsRasterLayer*>(ml));
}

void LayerOps::knockOutProjectRasterPaper(QgsProject* project) {
  if (!project) return;
  for (QgsMapLayer* ml : project->mapLayers())
    knockOutRasterPaper(qobject_cast<QgsRasterLayer*>(ml));
}

// Our own satellite background (F035), by the same identity rule every lookup uses:
// a layer tagged satellite, or — in a project saved before tagging — an untagged
// reference/background layer that ReferenceKind accepts by its old title ("위성",
// "VWorld 위성", "Google 위성"; a folded "…위성…" only for live tiles). A user raster such as
// "위성사진_판독" is neither, so it is never pruned, replaced or re-stacked with ours.
static bool isSatelliteBackground(const QgsMapLayer* l) {
  if (!l) return false;
  const QString kind = ReferenceKind::of(l);
  if (!kind.isEmpty()) return kind == QLatin1String(ReferenceKind::kSatellite);
  if (!LayerOps::isReferenceOrBasemapLayer(l)) return false;
  // An offline pack saved before tagging ("VWorld 위성 (오프라인)") is a copy, not a duplicate.
  if (l->source().contains(QLatin1String(".mbtiles"), Qt::CaseInsensitive)) return false;
  return ReferenceKind::matchesLegacy(l, QString::fromLatin1(ReferenceKind::kSatellite));
}

// Satellite background or its offline copy: both stay at the bottom and stay shown in the
// surface-survey view. A user layer that merely has 위성 in its title is neither.
static bool isSatelliteImagery(const QgsMapLayer* l) {
  if (isSatelliteBackground(l)) return true;
  if (!l || !l->name().contains(QStringLiteral("위성"))) return false;
  const QString kind = ReferenceKind::of(l);
  if (!kind.isEmpty()) return kind == QLatin1String(ReferenceKind::kTilePack);
  return LayerOps::isReferenceOrBasemapLayer(l) &&
         l->source().contains(QLatin1String(".mbtiles"), Qt::CaseInsensitive);
}

void LayerOps::pruneDuplicateSatelliteLayers(QgsProject* project) {
  if (!project) return;
  static bool inPrune = false;
  if (inPrune) return;
  struct Guard {
    bool& flag;
    explicit Guard(bool& f) : flag(f) { flag = true; }
    ~Guard() { flag = false; }
  } guard(inPrune);

  // 1. 프로젝트 맵 레이어 중 "위성" 배경지도 목록 수집.
  //    이름만 보고 지우면 사용자가 들여온 "위성사진_판독" 같은 조사 레이어까지
  //    같이 지워졌다. 여기서 지워도 되는 것은 우리가 올린 참조/배경 레이어뿐이다.
  //    An offline tile pack (tile_pack) is a separate copy and is never pruned. Only what
  //    isSatelliteBackground identifies as ours is removed; a layer that merely has 위성
  //    in its title (the user's data, F035) stays.
  QList<QgsMapLayer*> satLayers;
  for (QgsMapLayer* l : project->mapLayers()) {
    if (isSatelliteBackground(l)) satLayers.append(l);
  }

  // 2. 위성 레이어가 2개 이상이면 유효한 1개(keep)만 남기고 나머지 project에서 안전하게 제거.
  //    Keep the one shown in the legend (a title lookup reported every satellite as shown).
  QgsLayerTree* treeRoot = project->layerTreeRoot();
  const auto shown = [treeRoot](const QgsMapLayer* l) {
    const QgsLayerTreeLayer* node = treeRoot ? treeRoot->findLayer(l->id()) : nullptr;
    return node && node->isVisible();
  };
  QgsMapLayer* keep = nullptr;
  if (!satLayers.isEmpty()) {
    for (QgsMapLayer* l : satLayers) {
      if (l && l->isValid()) {
        if (!keep || shown(l)) {
          keep = l;
        }
      }
    }
    if (!keep)
      keep = satLayers.first();

    for (QgsMapLayer* l : satLayers) {
      if (l && l != keep) {
        project->removeMapLayer(l->id());
      }
    }
  }

  // 3. 같은 위성 레이어를 두 번 가리키는 범례 노드만 지운다.
  //    예전 코드는 "이름에 위성이 들어간 첫 노드"를 기준으로 삼아, 그 뒤에 오는
  //    keep 자신의 노드를 지워 버릴 수 있었다. 레이어는 프로젝트에 남고 범례에서만
  //    빠지므로 화면에서 사라졌다가 다시 열면 (restoreMissingLayerTreeNodes 덕에)
  //    되살아나는, 원인 찾기 어려운 증상이 됐다. 이제는 레이어 id로만 판단한다.
  if (QgsLayerTree* root = project->layerTreeRoot()) {
    QList<QgsLayerTreeNode*> toRemove;
    QSet<QString> seenLayerIds;
    std::function<void(QgsLayerTreeGroup*)> cleanGroup = [&](QgsLayerTreeGroup* grp) {
      if (!grp) return;
      for (QgsLayerTreeNode* child : grp->children()) {
        if (auto* lnode = qobject_cast<QgsLayerTreeLayer*>(child)) {
          // QgsProject reads the tree before resolving its layer references.
          // A null layer here can be a saved survey layer still being loaded.
          const QString id = lnode->layerId();
          if (id.isEmpty() || !lnode->layer()) continue;
          if (seenLayerIds.contains(id))
            toRemove.append(child);
          else
            seenLayerIds.insert(id);
        } else if (auto* subGrp = qobject_cast<QgsLayerTreeGroup*>(child)) {
          cleanGroup(subGrp);
        }
      }
    };
    cleanGroup(root);
    for (auto* node : toRemove) {
      if (node && node->parent()) {
        if (auto* pgrp = QgsLayerTree::toGroup(node->parent()))
          pgrp->removeChildNode(node);
      }
    }
  }
}

namespace {
bool legendOrderChanging = false; // Layer tree mutations run on the GUI thread.
}

bool LayerOps::moveLegendLayer(QgsLayerTreeLayer* node, int destinationIndex) {
  if (!node || !node->layer()) return false;
  QPointer<QgsLayerTreeGroup> group = qobject_cast<QgsLayerTreeGroup*>(node->parent());
  if (!group) return false;
  const int oldIndex = group->children().indexOf(node);
  if (oldIndex < 0 || destinationIndex < 0 || destinationIndex >= group->children().size())
    return false;
  if (oldIndex == destinationIndex) return true;

  QScopedValueRollback<bool> moving(legendOrderChanging, true);
  QPointer<QgsLayerTreeLayer> oldNode(node);
  QPointer<QgsLayerTreeLayer> replacement(node->clone());
  // Keep a tree reference throughout the move. Removing first queues deletion
  // of the datasource in QgsLayerTreeRegistryBridge, even if reinserted later.
  group->insertChildNode(destinationIndex > oldIndex ? destinationIndex + 1 : destinationIndex,
                         replacement);
  if (!group || !replacement || !oldNode) return false;
  group->removeChildNode(oldNode);
  return group && replacement && replacement->layer();
}

void LayerOps::ensureSatelliteAtBottom(QgsProject* project) {
  if (!project || legendOrderChanging) return;
  QScopedValueRollback<bool> sorting(legendOrderChanging, true);
  // The guard includes pruning: project signals must not reenter while a row
  // temporarily has both its old node and its replacement.
  pruneDuplicateSatelliteLayers(project);
  QgsLayerTree* root = project->layerTreeRoot();
  if (!root) return;
  // Satellite background and its offline pack sink to the bottom; a user layer does not.
  auto moveSatellites = [](QgsLayerTreeGroup* group) {
    QStringList backgroundIds;
    for (QgsLayerTreeNode* child : group->children()) {
      auto* node = qobject_cast<QgsLayerTreeLayer*>(child);
      if (node && LayerOps::isBasemapLayer(node->layer())) backgroundIds.append(node->layerId());
    }
    // 배경 타일은 유산 등 참조 벡터 그룹 아래, 위성은 그중에서도 맨 아래.
    for (const QString& id : backgroundIds) {
      auto* node = group->findLayer(id);
      if (node && node->parent() == group)
        LayerOps::moveLegendLayer(node, group->children().size() - 1);
    }
    QStringList ids;
    for (QgsLayerTreeNode* child : group->children()) {
      auto* node = qobject_cast<QgsLayerTreeLayer*>(child);
      if (node && isSatelliteImagery(node->layer()))
        ids.append(node->layerId());
    }
    for (const QString& id : ids) {
      // Resolve afresh after every move; moved nodes have been deleted.
      auto* node = group->findLayer(id);
      if (node && node->parent() == group)
        LayerOps::moveLegendLayer(node, group->children().size() - 1);
    }
  };
  moveSatellites(root);
  for (QgsLayerTreeNode* child : root->children()) {
    auto* group = qobject_cast<QgsLayerTreeGroup*>(child);
    if (group && group->name().contains(QStringLiteral("참조"))) moveSatellites(group);
  }
}

QString LayerOps::withVworldApiKey(const QString& source, const QString& currentKey) {
  const QString key = currentKey.trimmed();
  if (key.isEmpty() || source.isEmpty()) return source;
  if (!source.contains(QLatin1String("vworld.kr"), Qt::CaseInsensitive)) return source;

  // 키가 들어가는 자리는 두 곳뿐이다.
  //   WMTS/XYZ : .../req/wmts/1.0.0/<키>/Satellite/{z}/{y}/{x}.jpeg
  //   WMS      : ...?KEY=<키>&DOMAIN=...   (url= 안에서 퍼센트 인코딩되면 KEY%3D<키>)
  // 키 모양(8-4-4-4-12)만 바꾼다. 주소의 다른 부분은 건드리지 않는다.
  static const QString guid =
      QStringLiteral("[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}");
  static const QRegularExpression wmts(QStringLiteral("(req/wmts/1\\.0\\.0/)") + guid);
  static const QRegularExpression wms(QStringLiteral("(KEY(?:=|%3D))") + guid,
                                      QRegularExpression::CaseInsensitiveOption);
  QString out = source;
  out.replace(wmts, QStringLiteral("\\1") + key);
  out.replace(wms, QStringLiteral("\\1") + key);
  return out;
}

// GDAL sources can be a saved XML filename, not a URL. Reconnect with inline XML
// so a saved survey's original file is never overwritten just by opening it.
static QString refreshedVworldGdalSource(const QString& source, const QString& currentKey) {
  QString xml = source;
  if (!xml.trimmed().startsWith(QLatin1String("<GDAL_WMS"))) {
    const QFileInfo info(source);
    if (!info.isFile() || info.size() > 256 * 1024) return source;
    QFile file(source);
    if (!file.open(QIODevice::ReadOnly)) return source;
    xml = QString::fromUtf8(file.readAll());
  }
  QDomDocument doc;
  if (!doc.setContent(xml)) return source;
  QDomElement root = doc.documentElement();
  if (root.tagName() != QLatin1String("GDAL_WMS")) return source;
  QDomElement service = root.firstChildElement(QStringLiteral("Service"));
  QDomElement server = service.firstChildElement(QStringLiteral("ServerUrl"));
  QUrl url(server.text());
  if (service.attribute(QStringLiteral("name")) != QLatin1String("WMS") ||
      url.host().compare(QLatin1String("api.vworld.kr"), Qt::CaseInsensitive) != 0 ||
      url.path() != QLatin1String("/req/wms")) return source;
  const QStringList layers = service.firstChildElement(QStringLiteral("Layers")).text().split(',');
  if (layers.isEmpty() || !std::all_of(layers.cbegin(), layers.cend(), [](const QString& layer) {
        return layer.trimmed() == QLatin1String("lp_pa_cbnd_bonbun") ||
               layer.trimmed() == QLatin1String("lp_pa_cbnd_bubun");
      })) return source;

  bool changed = false;
  QUrlQuery query(url);
  const auto items = query.queryItems();
  for (const auto& item : items) {
    if (item.first.compare(QLatin1String("key"), Qt::CaseInsensitive) == 0 &&
        item.second != currentKey.trimmed()) {
      query.removeAllQueryItems(item.first);
      query.addQueryItem(item.first, currentKey.trimmed());
      changed = true;
    }
  }
  if (changed) {
    url.setQuery(query);
    while (!server.firstChild().isNull()) server.removeChild(server.firstChild());
    server.appendChild(doc.createTextNode(url.toString(QUrl::FullyEncoded)));
  }
  // The old 16,384-pixel Korea-wide image requested ~79 m/pixel: even a close
  // canvas zoom only magnified a blank, small-scale WMS tile. Use square 0.5 m
  // pixels for this app's known Korea window; unrelated GDAL windows stay intact.
  QDomElement window = root.firstChildElement(QStringLiteral("DataWindow"));
  if (root.firstChildElement(QStringLiteral("Projection")).text() == QLatin1String("EPSG:3857") &&
      window.firstChildElement(QStringLiteral("UpperLeftX")).text().toDouble() == 13500000.0 &&
      window.firstChildElement(QStringLiteral("UpperLeftY")).text().toDouble() == 4800000.0 &&
      window.firstChildElement(QStringLiteral("LowerRightX")).text().toDouble() == 14800000.0 &&
      window.firstChildElement(QStringLiteral("LowerRightY")).text().toDouble() == 3800000.0) {
    for (const auto& size : {qMakePair(QStringLiteral("SizeX"), QStringLiteral("2600000")),
                             qMakePair(QStringLiteral("SizeY"), QStringLiteral("2000000"))}) {
      QDomElement element = window.firstChildElement(size.first);
      if (element.text() == size.second) continue;
      if (element.isNull()) {
        element = doc.createElement(size.first);
        window.appendChild(element);
      }
      while (!element.firstChild().isNull()) element.removeChild(element.firstChild());
      element.appendChild(doc.createTextNode(size.second));
      changed = true;
    }
  }
  if (!changed) return source;
  QString result;
  QTextStream stream(&result);
  root.save(stream, 0);
  return result;
}

int LayerOps::refreshVworldApiKeyInLayers(QgsProject* project, const QString& currentKey,
                                          QStringList* changed) {
  if (!project || currentKey.trimmed().isEmpty()) return 0;
  int n = 0;
  for (QgsMapLayer* l : project->mapLayers()) {
    if (!l) continue;
    const QString src = l->source();
    const QString fixed = l->providerType() == QLatin1String("gdal")
                              ? refreshedVworldGdalSource(src, currentKey)
                              : withVworldApiKey(src, currentKey);
    if (fixed == src) continue;
    l->setDataSource(fixed, l->name(), l->providerType());
    l->triggerRepaint();
    if (changed) *changed << l->name();
    ++n;
  }
  return n;
}

void LayerOps::applyCanvasScreenDpi(QgsMapCanvas* canvas) {
  if (!canvas) return;
  // QgsMapCanvas uses logical viewport pixels for outputSize and mapToPixel.
  // QGIS applies DPR when allocating the render image. Multiplying size here
  // applies scaling twice and separates rendered features from canvas overlays
  // and mouse coordinates (e.g. an 80% offset copy at Windows 125%).
  const qreal dpr = canvas->devicePixelRatioF();
  const qreal pixelDpr = dpr > 0.05 ? dpr : 1.0;
  if (!qFuzzyCompare(canvas->mapSettings().devicePixelRatio(), static_cast<float>(pixelDpr)))
    canvas->mapSettings().setDevicePixelRatio(static_cast<float>(pixelDpr));
  const QSize want = canvas->viewport()->size();
  if (want.width() >= 2 && want.height() >= 2) {
    if (canvas->mapSettings().outputSize() != want)
      canvas->mapSettings().setOutputSize(want);
  }
  QWindow* wh = canvas->windowHandle();
  if (!wh && canvas->window())
    wh = canvas->window()->windowHandle();
  // Match QGIS logical DPI; devicePixelRatio already controls physical pixels.
  double dpi = canvas->logicalDpiX();
  if (wh && wh->screen()) {
    const double logical = wh->screen()->logicalDotsPerInch();
    if (logical > 10.0)
      dpi = logical;
  }
  if (!qFuzzyCompare(canvas->mapSettings().outputDpi(), dpi))
    canvas->mapSettings().setOutputDpi(dpi);
}

bool LayerOps::canvasDisplayEventNeedsTileRefresh(int eventType) {
  const auto t = static_cast<QEvent::Type>(eventType);
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
  if (t == QEvent::DevicePixelRatioChange)
    return true;
#endif
  Q_UNUSED(t);
  return false;
}

static bool extentUsable(const QgsRectangle& ext) {
  return !ext.isNull() && ext.isFinite();
}

static QgsRectangle vectorFeatureExtent(QgsVectorLayer* vl) {
  if (!vl) return {};
  vl->updateExtents();
  QgsRectangle ext = CadDrawingLayers::viewExtent(vl);  // 도면은 멀리 떨어진 몇 개를 뺀 범위
  if (extentUsable(ext)) return ext;
  QgsRectangle acc;
  bool any = false;
  QgsFeatureIterator it = vl->getFeatures(QgsFeatureRequest().setNoAttributes());
  QgsFeature f;
  while (it.nextFeature(f)) {
    if (!f.hasGeometry() || f.geometry().isEmpty()) continue;
    const QgsRectangle b = f.geometry().boundingBox();
    if (!extentUsable(b)) continue;
    if (!any) {
      acc = b;
      any = true;
    } else {
      acc.combineExtentWith(b);
    }
  }
  return any ? acc : QgsRectangle();
}

bool LayerOps::zoomToLayerMax(QgsMapCanvas* canvas, QgsMapLayer* layer) {
  if (!canvas || !layer || !layer->isValid()) return false;

  QgsRectangle ext;
  if (auto* vl = qobject_cast<QgsVectorLayer*>(layer))
    ext = vectorFeatureExtent(vl);
  else
    ext = layer->extent();

  const QgsCoordinateReferenceSystem layerCrs = layer->crs();
  const QgsCoordinateReferenceSystem mapCrs = canvas->mapSettings().destinationCrs().isValid()
                                                  ? canvas->mapSettings().destinationCrs()
                                                  : QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186"));
  const QString mapAuth = mapCrs.isValid() ? mapCrs.authid() : QStringLiteral("EPSG:5186");
  const QgsRectangle kr = koreaExtentForCrs(mapAuth);

  if (layerCrs.isValid() && mapCrs.isValid() && extentUsable(ext) &&
      layerCrs.authid() != mapCrs.authid()) {
    try {
      QgsCoordinateTransform xf(layerCrs, mapCrs, QgsProject::instance()
                                                      ? QgsProject::instance()->transformContext()
                                                      : QgsCoordinateTransformContext());
      xf.setBallparkTransformsAreAppropriate(true);
      ext = xf.transformBoundingBox(ext);
    } catch (...) {
      KA_LOG_EXCEPT();
      if (qobject_cast<QgsRasterLayer*>(layer)) {
        zoomCanvasToWorkingScale(canvas, mapAuth, 50000.0);
        refreshCanvasIfIdle(canvas);
        return true;
      }
      return false;
    }
  }

  const bool worldLike = !extentUsable(ext) ||
                         (!kr.isNull() && kr.isFinite() &&
                          (ext.width() > kr.width() * 1.2 || ext.height() > kr.height() * 1.2));
  if (qobject_cast<QgsRasterLayer*>(layer) && worldLike) {
    zoomCanvasToWorkingScale(canvas, mapAuth, 50000.0);
    refreshCanvasIfIdle(canvas);
    return true;
  }

  if (!extentUsable(ext))
    return false;

  if (!kr.isEmpty() && kr.isFinite() && (ext.width() > kr.width() * 1.2 || ext.height() > kr.height() * 1.2)) {
    zoomCanvasToWorkingScale(canvas, mapAuth, 50000.0);
    refreshCanvasIfIdle(canvas);
    return true;
  }

  const double minW = mapCrs.isGeographic() ? 0.004 : 80.0;
  if (ext.width() < minW || ext.height() < minW) {
    const QgsPointXY c = ext.center();
    // A narrow site/section can still be kilometers long. Pad only the short
    // dimension; replacing both with a minimum square crops the actual layer.
    const double halfW = std::max(ext.width(), minW) * 0.5;
    const double halfH = std::max(ext.height(), minW) * 0.5;
    ext = QgsRectangle(c.x() - halfW, c.y() - halfH, c.x() + halfW, c.y() + halfH);
  }
  ext.scale(1.15);
  canvas->setExtent(ext);
  canvas->zoomToFeatureExtent(ext);
  const QgsRectangle after = canvas->extent();
  if (!kr.isEmpty() && kr.isFinite() &&
      (after.width() > kr.width() * 1.12 || after.height() > kr.height() * 1.12))
    clampCanvasToKorea(canvas);
  refreshCanvasIfIdle(canvas);
  return true;
}

bool LayerOps::zoomToProjectDataLayers(QgsMapCanvas* canvas, QgsProject* project) {
  if (!canvas || !project) return false;
  QgsRectangle totalExt;
  bool found = false;
  const QgsCoordinateReferenceSystem mapCrs = canvas->mapSettings().destinationCrs().isValid()
                                                  ? canvas->mapSettings().destinationCrs()
                                                  : project->crs();
  const QString mapAuth = mapCrs.isValid() ? mapCrs.authid() : QStringLiteral("EPSG:5186");
  const QgsRectangle kr = koreaExtentForCrs(mapAuth);

  for (QgsMapLayer* l : project->mapLayers()) {
    if (layerKeyOf(l) != QLatin1String("survey_area")) continue;
    auto* survey = qobject_cast<QgsVectorLayer*>(l);
    if (survey && survey->featureCount() > 0 && zoomToLayerMax(canvas, survey))
      return true;
  }

  for (QgsMapLayer* l : project->mapLayers()) {
    if (!l || !l->isValid() || isBasemapLayer(l)) continue;
    const QString n = l->name();
    if (n.contains(QStringLiteral("위성")) || n.contains(QStringLiteral("지적"))) continue;
    auto* vl = qobject_cast<QgsVectorLayer*>(l);
    if (!vl || vl->featureCount() == 0) continue;
    QgsRectangle ext = vectorFeatureExtent(vl);
    if (!extentUsable(ext)) continue;
    // Skip unreasonable world/whole-korea bounds for a single survey vector layer
    if (!kr.isEmpty() && kr.isFinite() && (ext.width() > kr.width() * 0.95 || ext.height() > kr.height() * 0.95))
      continue;
    if (vl->crs().isValid() && mapCrs.isValid() && vl->crs() != mapCrs) {
      try {
        QgsCoordinateTransform xf(vl->crs(), mapCrs, project->transformContext());
        xf.setBallparkTransformsAreAppropriate(true);
        ext = xf.transformBoundingBox(ext);
      } catch (...) {
        KA_LOG_EXCEPT();
        continue;
      }
    }
    if (extentUsable(ext)) {
      if (!found) {
        totalExt = ext;
        found = true;
      } else {
        totalExt.combineExtentWith(ext);
      }
    }
  }

  if (found && extentUsable(totalExt)) {
    const double minW = mapCrs.isGeographic() ? 0.004 : 100.0;
    if (totalExt.width() < minW || totalExt.height() < minW) {
      const QgsPointXY c = totalExt.center();
      const double pad = minW * 0.5;
      totalExt = QgsRectangle(c.x() - pad, c.y() - pad, c.x() + pad, c.y() + pad);
    }
    totalExt.scale(1.2);
    canvas->setExtent(totalExt);
    canvas->zoomToFeatureExtent(totalExt);
    clampCanvasToKorea(canvas);
    if (canvas->scale() > 80000.0)
      canvas->zoomScale(5000.0, true);
    refreshCanvasIfIdle(canvas);
    return true;
  }
  return false;
}

bool LayerOps::applyInvertedPaperMask(QgsVectorLayer* layer) {
  if (!layer || !layer->isValid()) return false;
  auto fill = QgsFillSymbol::createSimple({
      {QStringLiteral("color"), QStringLiteral("255,255,255,255")},
      {QStringLiteral("style"), QStringLiteral("solid")},
      {QStringLiteral("outline_style"), QStringLiteral("no")},
  });
  if (!fill) return false;
  auto* embedded = new QgsSingleSymbolRenderer(fill.release());
  auto* inv = new QgsInvertedPolygonRenderer(embedded);
  inv->setPreprocessingEnabled(true);
  layer->setRenderer(inv);
  layer->triggerRepaint();
  return true;
}

QgsVectorLayer* LayerOps::upsertAdminEmdMask(QgsProject* project, const QgsGeometry& geom,
                                             const QgsCoordinateReferenceSystem& srcCrs,
                                             const QString& workCrsAuthId, const QString& titleKo) {
  if (!project || geom.isEmpty()) return nullptr;
  const QString destAuth =
      workCrsAuthId.isEmpty() ? QStringLiteral("EPSG:5186") : workCrsAuthId;
  QgsGeometry local = geom;
  const QgsCoordinateReferenceSystem dest(destAuth);
  if (srcCrs.isValid() && dest.isValid() && srcCrs != dest) {
    QgsCoordinateTransform xf(srcCrs, dest, project->transformContext());
    xf.setBallparkTransformsAreAppropriate(true);
    if (local.transform(xf) != Qgis::GeometryOperationResult::Success)
      return nullptr;
  }

  QgsVectorLayer* layer = nullptr;
  for (QgsMapLayer* l : project->mapLayers()) {
    if (isAdminEmdLayer(l)) {
      layer = qobject_cast<QgsVectorLayer*>(l);
      break;
    }
  }
  if (!layer) {
    const QString title =
        titleKo.isEmpty() ? QStringLiteral("읍면동 마스크") : titleKo;
    layer = new QgsVectorLayer(QStringLiteral("Polygon?crs=%1").arg(destAuth), title,
                               QStringLiteral("memory"));
    if (!layer->isValid()) {
      delete layer;
      return nullptr;
    }
    markReferenceLayer(layer);
    ReferenceKind::tag(layer, QString::fromLatin1(ReferenceKind::kAdminMask));
    layer->setCustomProperty(QStringLiteral("ka_hgis/admin_emd"), true);
    project->addMapLayer(layer, true);
  } else if (!titleKo.isEmpty()) {
    layer->setName(titleKo);
  }
  if (layer && layerKeyOf(layer) == QLatin1String(kAdminEmdKey))
    layer->removeCustomProperty(QString::fromUtf8(kPropLayerKey));

  if (!layer->isValid()) return nullptr;
  if (layer->isEditable())
    layer->rollBack();
  if (!layer->startEditing()) return nullptr;
  QgsFeatureIds ids;
  QgsFeatureIterator it = layer->getFeatures();
  QgsFeature existing;
  while (it.nextFeature(existing))
    ids.insert(existing.id());
  if (!ids.isEmpty())
    layer->deleteFeatures(ids);
  QgsFeature f(layer->fields());
  f.setGeometry(local);
  if (!layer->addFeature(f)) {
    layer->rollBack();
    return nullptr;
  }
  if (!layer->commitChanges()) {
    layer->rollBack();
    return nullptr;
  }
  applyInvertedPaperMask(layer);
  applyLegendCrsLabel(layer);
  return layer;
}

bool LayerOps::isolateSurfaceSurveyView(QgsProject* project, QgsMapCanvas* canvas,
                                        QgsMapLayer* siteLayer) {
  if (!project) return false;
  QgsLayerTree* root = project->layerTreeRoot();
  if (!root) return false;
  // Our satellite (and its offline copy) by identity, not every title with 위성 (F035).
  for (QgsMapLayer* l : project->mapLayers()) {
    if (!l) continue;
    QgsLayerTreeLayer* n = root->findLayer(l->id());
    if (!n) continue;
    const bool show = isSatelliteImagery(l) || isAdminEmdLayer(l) || l == siteLayer ||
                      (siteLayer == nullptr && isImportedSiteLayer(l));
    n->setItemVisibilityChecked(show);
  }
  if (canvas)
    syncMapCanvas(project, canvas, false);
  return true;
}

void LayerOps::zoomToFullMax(QgsMapCanvas* canvas) {
  if (!canvas) return;
  const QString auth = canvas->mapSettings().destinationCrs().isValid()
                           ? canvas->mapSettings().destinationCrs().authid()
                           : QStringLiteral("EPSG:5186");
  LayerOps::zoomToKorea(canvas, auth);
  LayerOps::clampCanvasToKorea(canvas);
}

static void syncCanvasToProject(QgsProject* project, QgsMapCanvas* canvas) {
  LayerOps::syncMapCanvas(project, canvas, false);
}

static bool isFatalVworldAuthError(const QString& raw) {
  return raw.contains(QStringLiteral("INVALID_KEY"), Qt::CaseInsensitive) ||
         raw.contains(QStringLiteral("등록되지 않은")) ||
         raw.contains(QStringLiteral("인증키")) ||
         raw.contains(QStringLiteral("인증URL 불일치"));
}

static QString friendlyBasemapError(const QString& raw) {
  const QString r = raw;
  if (isFatalVworldAuthError(r) ||
      r.contains(QStringLiteral("InvalidParameterValue"), Qt::CaseInsensitive)) {
    return QStringLiteral(
        "등록되지 않은 VWorld 키입니다. 리본의 더보기 → API 키 입력에서 키를 확인하세요.");
  }
  return r;
}

static QgsRasterLayer* tryCreateXyzLayer(const QString& url, const QString& name, QString* errDetail) {
  auto* rl = new QgsRasterLayer(url, name, QStringLiteral("wms"));
  if (rl->isValid()) return rl;
  if (errDetail) *errDetail = friendlyBasemapError(rl->error().message());
  delete rl;
  return nullptr;
}

static bool addXyzBasemap(QgsProject* project, QgsMapCanvas* canvas, const QString& url,
                          const QString& name, QString* errorOut, bool crispText = false) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("No project");
    return false;
  }
  LayerOps::ensureTileNetworkIdentity();
  const bool cadastralPicture = name.contains(QStringLiteral("지적"));
  // Logical identity of the new layer (F035). 본번/부번 pictures share a kind but stay
  // separate layers, so for them the title still has to match.
  const QString kind = cadastralPicture ? QString::fromLatin1(ReferenceKind::kCadastralPicture)
                                        : ReferenceKind::forTitle(name);

  {
    // Replace only our own previous copy: the same kind (even if the user renamed it), or
    // an untagged tile/VWorld layer of the same title from an older project. A user layer
    // with a similar name ("위성사진_판독", downloaded 지적도) is left alone.
    QStringList removeIds;
    for (QgsMapLayer* old : project->mapLayers()) {
      if (!old) continue;
      const QString oldKind = ReferenceKind::of(old);
      const bool sameTitle = legendTitlesMatch(old->name(), name);
      bool ours = false;
      if (!oldKind.isEmpty())
        ours = !kind.isEmpty() && oldKind == kind && (!cadastralPicture || sameTitle);
      else
        ours = sameTitle && LayerOps::layerKeyOf(old).isEmpty() &&
               !old->source().contains(QLatin1String(".mbtiles"), Qt::CaseInsensitive) &&
               (LayerOps::isBasemapLayer(old) ||  // live tiles, or our GDAL_WMS cadastral XML
                old->source().contains(QLatin1String("vworld-cadastral"), Qt::CaseInsensitive));
      if (ours)
        removeIds.append(old->id());
    }
    for (const QString& id : removeIds)
      project->removeMapLayer(id);
  }

  QString detail;
  QgsRasterLayer* rl = tryCreateXyzLayer(url, name, &detail);
  if (!rl) {
    if (errorOut) {
      *errorOut = QStringLiteral("Basemap 실패 (%1): %2").arg(name, detail.isEmpty()
                                                                   ? QStringLiteral("invalid wms/xyz layer")
                                                                   : detail);
    }
    return false;
  }
  tuneBasemapLayer(rl, crispText);
  if (cadastralPicture)
    LayerOps::markCadastralLayer(rl);
  else
    LayerOps::markReferenceLayer(rl);
  ReferenceKind::tag(rl, kind);
  QgsMapLayer* added = project->addMapLayer(rl, false);
  if (!added) {
    if (errorOut)
      *errorOut = QStringLiteral("Basemap 실패 (%1): addMapLayer rejected").arg(name);
    delete rl;
    return false;
  }
  LayerOps::applyLegendCrsLabel(added);
  if (QgsLayerTree* root = project->layerTreeRoot()) {
    QgsLayerTreeLayer* node = root->addLayer(added);
    if (node) {
      node->setItemVisibilityChecked(true);
    }
  }
  if (cadastralPicture)
    LayerOps::placeCadastralLayer(project, added);
  LayerOps::ensureSatelliteAtBottom(project);
  LayerOps::pruneEmptyLegendGroups(project);
  if (project->mapLayer(added->id()) == nullptr) {
    if (errorOut)
      *errorOut = QStringLiteral("Basemap 실패 (%1): layer removed after add").arg(name);
    return false;
  }
  // 프로젝트에 있다고 화면에 보이는 게 아니다. 배경지도는 addMapLayer(.., false) 로
  // 넣고 범례 노드를 직접 단다. 그 뒤 정렬·가지치기가 노드를 건드리므로, 여기서
  // 노드가 실제로 살아 있는지 확인한다. 예전에는 프로젝트만 보고 "성공"이라 답해서,
  // 레이어 패널에 아무것도 없는데 앱은 올렸다고 여겼다.
  if (QgsLayerTree* root = project->layerTreeRoot()) {
    QgsLayerTreeLayer* node = root->findLayer(added->id());
    if (!node) {
      node = root->addLayer(added);
      if (node) {
        node->setItemVisibilityChecked(true);
        LayerOps::ensureSatelliteAtBottom(project);
      }
    }
    if (!root->findLayer(added->id())) {
      if (errorOut)
        *errorOut = QStringLiteral("Basemap 실패 (%1): 레이어 목록에 넣지 못했습니다").arg(name);
      return false;
    }
  }
  if (canvas) {
    const QString workAuth = project && project->crs().isValid()
                                 ? project->crs().authid()
                                 : QStringLiteral("EPSG:5186");
    LayerOps::ensureOtfEnabled(project, canvas, workAuth);
    const QgsRectangle before = canvas->extent();
    const QgsPointXY centerBefore = before.center();
    const bool keepCenter = !before.isEmpty() && before.isFinite() && before.width() > 0 &&
                            canvas->scale() > 50.0 && canvas->scale() < 500000.0;
    const bool needScaleOnly = canvas->extent().isEmpty() || !canvas->extent().isFinite() ||
                               canvas->scale() > 400000.0 || canvas->scale() < 100.0;
    syncCanvasToProject(project, canvas);
    if (keepCenter) {
      canvas->setCenter(centerBefore);
      if (canvas->scale() > 80000.0)
        canvas->zoomScale(10000.0, true);
    } else if (needScaleOnly) {
      zoomCanvasToWorkingScale(canvas, workAuth, 50000.0);
    }
    LayerOps::refreshXyzBasemapTiles(canvas);
  }
  return project->mapLayer(added->id()) != nullptr;
}

bool LayerOps::addOsmBasemap(QgsProject* project, QgsMapCanvas* canvas, QString* errorOut) {
  LayerOps::ensureTileNetworkIdentity();
  const QStringList uris = {
      QStringLiteral(
          "type=xyz&url=https://tile.openstreetmap.org/%7Bz%7D/%7Bx%7D/%7By%7D.png&zmax=19&zmin=0&crs=EPSG:3857&tilePixelRatio=1"),
      QStringLiteral(
          "type=xyz&url=https://tile.openstreetmap.org/%7Bz%7D/%7Bx%7D/%7By%7D.png&zmax=19&zmin=0&tilePixelRatio=1"),
      QStringLiteral(
          "type=xyz&url=https://basemaps.cartocdn.com/light_all/%7Bz%7D/%7Bx%7D/%7By%7D.png&zmax=20&zmin=0&crs=EPSG:3857&tilePixelRatio=1"),
      QStringLiteral(
          "type=xyz&url=https://a.basemaps.cartocdn.com/light_all/%7Bz%7D/%7Bx%7D/%7By%7D.png&zmax=20&zmin=0&tilePixelRatio=1"),
  };
  QString lastErr;
  for (int i = 0; i < uris.size(); ++i) {
    const QString layerName = (i >= 2) ? QStringLiteral("Carto Light") : QStringLiteral("OSM");
    QString err;
    if (addXyzBasemap(project, canvas, uris.at(i), layerName, &err)) {
      if (canvas) {
        syncCanvasToProject(project, canvas);
        LayerOps::zoomToKorea(canvas, project && project->crs().isValid()
                                          ? project->crs().authid()
                                          : QStringLiteral("EPSG:5186"));
      }
      if (errorOut && i >= 2)
        *errorOut = QStringLiteral("OSM 대체: Carto Light 사용");
      return true;
    }
    lastErr = err;
  }
  if (errorOut) *errorOut = lastErr.isEmpty() ? QStringLiteral("OSM/Carto 타일 레이어 생성 실패") : lastErr;
  return false;
}

static bool requireVworldKey(const QString& apiKey, QString* errorOut) {
  if (!apiKey.trimmed().isEmpty()) return true;
  if (errorOut) {
    *errorOut = QStringLiteral(
        "VWorld API 키가 없습니다. 리본의 더보기 → API 키 입력에서 키를 넣으세요.");
  }
  return false;
}

// Moves the layer just added under `title` into 참조 지도, by its reference kind so a user
// layer with a similar title stays where it is (F035).
static void placeKindInReferenceGroup(QgsProject* project, const QString& title) {
  if (!project) return;
  const QString kind = ReferenceKind::forTitle(title);
  const auto layers = project->mapLayers();
  for (QgsMapLayer* l : layers) {
    const bool ours = kind.isEmpty() ? (l && legendTitlesMatchDirect(l->name(), title))
                                     : ReferenceKind::is(l, kind);
    if (ours) LayerOps::placeInLegendGroup(project, l, QStringLiteral("참조 지도"));
  }
}

static bool addBasemapWithFallbacks(QgsProject* project, QgsMapCanvas* canvas,
                                    const QStringList& uris, const QString& name,
                                    QString* errorOut) {
  QString last;
  for (const QString& uri : uris) {
    QString err;
    if (addXyzBasemap(project, canvas, uri, name, &err))
      return true;
    last = err;
  }
  if (errorOut) *errorOut = last;
  return false;
}

bool LayerOps::addVworldBaseMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut) {
  if (!requireVworldKey(apiKey, errorOut)) return false;
  const QString key = apiKey.trimmed();
  const QStringList uris = {
      QStringLiteral(
          "type=xyz&url=https://api.vworld.kr/req/wmts/1.0.0/%1/Base/%7Bz%7D/%7By%7D/%7Bx%7D.png"
          "&zmax=19&zmin=6&crs=EPSG:3857&tilePixelRatio=1")
          .arg(key),
      QStringLiteral(
          "type=xyz&url=https://xdworld.vworld.kr/2d/Base/service/%7Bz%7D/%7Bx%7D/%7By%7D.png"
          "&zmax=19&zmin=6&crs=EPSG:3857&tilePixelRatio=1"),
  };
  const bool ok =
      addBasemapWithFallbacks(project, canvas, uris, QStringLiteral("VWorld 배경"), errorOut);
  if (ok && canvas) {
    const QString workAuth = project && project->crs().isValid()
                                 ? project->crs().authid()
                                 : QStringLiteral("EPSG:5186");
    if (canvas->scale() > 200000.0)
      zoomCanvasToWorkingScale(canvas, workAuth, 50000.0);
  }
  return ok;
}

// ── 캔버스가 그리는 중일 때: 버리지 말고 뒤로 미룬다 ─────────────────────────
// 예전에는 canvas->isDrawing() 이면 새로고침·클램프를 그냥 버렸다. 진행 중인 WMS
// 작업을 끊으면 TileDownloadManager 가 뒤늦게 deleteLater 를 불러 ACCESS_VIOLATION
// 이 나기 때문이다. 그런데 버리기만 하니 「이 레이어로 이동」처럼 옮긴 직후 다시
// 그려야 하는 자리에서 위성 타일이 새 범위로 갱신되지 않았고, 사용자가 줌인·줌아웃
// 하거나 점을 찍고 지울 때까지 배경이 빈 종이로 남았다.
// 이제는 그리기가 끝난 뒤로 미룬다. 진행 중인 작업을 끊지 않으므로 크래시 회피는
// 그대로고, 미뤄 둔 일은 반드시 실행된다.
namespace {
constexpr int kDeferredRetryMax = 40;   // 40 × 120ms ≈ 4.8초까지 기다린다
constexpr int kDeferredRetryMs = 120;
constexpr const char* kPropRefreshPending = "kaRefreshPending";
constexpr const char* kPropClampPending = "kaKoreaClampPending";

void runWhenCanvasIdle(QgsMapCanvas* canvas, const char* pendingProp, int retriesLeft,
                       const std::function<void(QgsMapCanvas*)>& job) {
  if (!canvas) return;
  if (!canvas->isDrawing()) {
    canvas->setProperty(pendingProp, false);
    job(canvas);
    return;
  }
  if (retriesLeft <= 0) {
    canvas->setProperty(pendingProp, false);
    return;
  }
  // 같은 일이 이미 예약돼 있으면 타이머를 겹쳐 쌓지 않는다.
  if (retriesLeft == kDeferredRetryMax && canvas->property(pendingProp).toBool())
    return;
  canvas->setProperty(pendingProp, true);
  QPointer<QgsMapCanvas> guard(canvas);
  QTimer::singleShot(kDeferredRetryMs, canvas, [guard, pendingProp, retriesLeft, job]() {
    if (guard)
      runWhenCanvasIdle(guard.data(), pendingProp, retriesLeft - 1, job);
  });
}

void refreshCanvasNowOrLater(QgsMapCanvas* canvas) {
  runWhenCanvasIdle(canvas, kPropRefreshPending, kDeferredRetryMax,
                    [](QgsMapCanvas* c) { c->refresh(); });
}
}  // namespace

void LayerOps::refreshXyzBasemapTiles(QgsMapCanvas* canvas) {
  if (!canvas) return;
  applyCanvasScreenDpi(canvas);
  // 병렬 렌더만 끈다(WMS 중첩 이벤트 루프 AV 방지). 미리보기 작업은 끄지 않는다.
  // 예전에는 여기서 setPreviewJobsEnabled(false)를 불렀는데, 이 함수가 11곳에서
  // 불리는 탓에 화면이 뜨거나 배경지도를 올릴 때마다 미리보기가 다시 꺼졌다.
  // 그러면 화면을 끄는 동안 캔버스에 보여줄 그림이 없어 지도가 꺼졌다 켜진다.
  canvas->setParallelRenderingEnabled(false);
  // Aborting an in-flight WMS job drops TileDownloadManager objects that
  // still finish and call deleteLater — ACCESS_VIOLATION on Windows.
  // 그래서 끊지 않는다. 다만 버리지도 않고, 그리기가 끝나면 새 범위로 다시 그린다.
  refreshCanvasNowOrLater(canvas);
}

// api.vworld.kr은 등록된 키로도 국토 밖 타일에 FileNotFound "서비스 제공영역이
// 아닙니다"를, WMS 범례 요청에는 INVALID_RANGE(GetLegendGraphic 미지원)를 준다.
// 둘 다 HTTP 200이라 provider_wms는 "Tile request error (Status: 200 …)"로 기록한다.
bool LayerOps::mapServerMessageIsBenign(const QString& message) {
  if (message.isEmpty()) return false;
  static constexpr const char* kBenign[] = {"legend", "Status: 200", "canceled", "cancelled",
                                            "aborted"};
  for (const char* needle : kBenign)
    if (message.contains(QLatin1String(needle), Qt::CaseInsensitive)) return true;
  return message.contains(QStringLiteral("범례")) || message.contains(QStringLiteral("제공영역"));
}

bool LayerOps::addVworldSatelliteMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut) {
  if (!project) return false;
  pruneDuplicateSatelliteLayers(project);
  for (QgsMapLayer* l : project->mapLayers()) {
    // Only our satellite counts as "already there"; a user raster named 위성… does not.
    if (l && l->isValid() && isSatelliteBackground(l)) {
      ensureSatelliteAtBottom(project);
      if (canvas) {
        const QString workAuth = project->crs().isValid()
                                     ? project->crs().authid()
                                     : QStringLiteral("EPSG:5186");
        LayerOps::ensureOtfEnabled(project, canvas, workAuth);
        refreshXyzBasemapTiles(canvas);
      }
      return true;
    }
  }
  const QString key = apiKey.trimmed();
  QStringList uris;
  if (!key.isEmpty()) {
    uris << QStringLiteral(
                "type=xyz&url=https://api.vworld.kr/req/wmts/1.0.0/%1/Satellite/%7Bz%7D/%7By%7D/%7Bx%7D.jpeg"
                "&zmax=19&zmin=6&crs=EPSG:3857&tilePixelRatio=1&http-header:referer=https://localhost")
                .arg(key);
    uris << QStringLiteral(
                "type=xyz&url=https://api.vworld.kr/req/wmts/1.0.0/%1/Satellite/%7Bz%7D/%7By%7D/%7Bx%7D.jpeg"
                "&zmax=19&zmin=6&crs=EPSG:3857&tilePixelRatio=1")
                .arg(key);
  }
  uris << QStringLiteral(
      "type=xyz&url=https://xdworld.vworld.kr/2d/Satellite/service/%7Bz%7D/%7Bx%7D/%7By%7D.jpeg"
      "&zmax=19&zmin=6&crs=EPSG:3857&tilePixelRatio=1&http-header:referer=https://localhost");
  uris << QStringLiteral(
      "type=xyz&url=https://xdworld.vworld.kr/2d/Satellite/service/%7Bz%7D/%7Bx%7D/%7By%7D.jpeg"
      "&zmax=19&zmin=6&crs=EPSG:3857&tilePixelRatio=1");
  const bool ok =
      addBasemapWithFallbacks(project, canvas, uris, QStringLiteral("VWorld 위성"), errorOut);
  if (ok && canvas) {
    const QString workAuth = project && project->crs().isValid()
                                 ? project->crs().authid()
                                 : QStringLiteral("EPSG:5186");
    LayerOps::ensureOtfEnabled(project, canvas, workAuth);
    zoomCanvasToWorkingScale(canvas, workAuth, 50000.0);
    refreshXyzBasemapTiles(canvas);
  }
  return ok;
}

static QString makeVworldWmsUri(const QString& apiKey, const QString& layers, const QString& styles,
                                const QString& crsAuthId) {
  const QString key = apiKey.trimmed();
  const QString crs = crsAuthId.trimmed().isEmpty() ? QStringLiteral("EPSG:3857") : crsAuthId.trimmed();
  // GitHub baseline (eac6c9c): KEY/DOMAIN + tiled WMS (tilePixelRatio=2). That path drew parcels.
  //
  // DOMAIN 은 뺐다. VWorld 는 DOMAIN=localhost 가 붙으면 같은 키라도 INCORRECT_KEY 로
  // 거절한다. 인증은 Referer 헤더가 하므로 http-header:referer 를 반드시 같이 보낸다
  // (XYZ 배경지도가 쓰는 것과 같은 값). 2026-09-06 실측:
  //   referer O + DOMAIN=localhost → INCORRECT_KEY
  //   referer O + DOMAIN 없음      → 정상 PNG
  //   referer X                    → DOMAIN 과 무관하게 INCORRECT_KEY
  const QString baseUrl = QStringLiteral("https://api.vworld.kr/req/wms?KEY=%1").arg(key);
  const QString encUrl = QString::fromLatin1(QUrl::toPercentEncoding(baseUrl));
  const QString stylePart = styles.isEmpty() ? QStringLiteral("styles")
                                             : QStringLiteral("styles=%1").arg(styles);
  return QStringLiteral(
             "IgnoreGetMapUrl=1&IgnoreGetFeatureInfoUrl=1&contextualWMSLegend=0"
             "&crs=%1&dpiMode=7&format=image/png&transparent=true&featureCount=10"
             "&tilePixelRatio=2&stepWidth=512&stepHeight=512"
             "&http-header:referer=https://localhost"
             "&layers=%2&%3&url=%4")
      .arg(crs, layers, stylePart, encUrl);
}

// 지적 GDAL_WMS 설정 파일이 사는 곳. 포터블은 자기 config, 개발은 앱 전용 폴더.
static QString kaVworldCadastralXmlPath() {
  const KaPortablePaths bundled = KaPortableRuntime::discover(KaPortableRuntime::resolvedExeDir());
  if (bundled.looksBundled()) {
    const QString dir = KaPortableRuntime::userConfigDir();
    QDir().mkpath(dir);
    return QDir(dir).filePath(QStringLiteral("vworld-cadastral.xml"));
  }
  const QByteArray localAppData = qgetenv("LOCALAPPDATA");
  const QString base = localAppData.isEmpty() ? QDir::tempPath()
                                              : QString::fromLocal8Bit(localAppData);
  const QString dir = base + QStringLiteral("/ka-hgis");
  QDir().mkpath(dir);
  return QDir(dir).filePath(QStringLiteral("vworld-cadastral.xml"));
}

static bool addGdalVworldCadastral(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey,
                                   const QString& layers, QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("No project");
    return false;
  }
  const QString xml = QStringLiteral(
                          "<GDAL_WMS>"
                          "<Service name=\"WMS\">"
                          "<Version>1.3.0</Version>"
                          // domain= 은 넣지 않는다. VWorld 는 domain=localhost 가 붙으면
                          // 같은 키·같은 Referer 라도 INCORRECT_KEY 로 거절한다(실측).
                          //   referer 있음 + domain=localhost → INCORRECT_KEY
                          //   referer 있음 + domain 없음      → 정상 PNG
                          //   referer 없음                    → domain 과 무관하게 INCORRECT_KEY
                          // 즉 인증은 아래 <Referer> 가 하고, domain 은 해가 되기만 한다.
                          "<ServerUrl>https://api.vworld.kr/req/wms?key=%1&amp;</ServerUrl>"
                          "<Layers>%2</Layers>"
                          "<Styles>lp_pa_cbnd_bonbun,lp_pa_cbnd_bubun</Styles>"
                          "<CRS>EPSG:3857</CRS>"
                          "<ImageFormat>image/png</ImageFormat>"
                          "<Transparent>TRUE</Transparent>"
                          "<BBoxOrder>xyXY</BBoxOrder>"
                          "</Service>"
                          "<DataWindow>"
                          "<UpperLeftX>13500000</UpperLeftX>"
                          "<UpperLeftY>4800000</UpperLeftY>"
                          "<LowerRightX>14800000</LowerRightX>"
                          "<LowerRightY>3800000</LowerRightY>"
                          "<SizeX>2600000</SizeX>"
                          "<SizeY>2000000</SizeY>"
                          "</DataWindow>"
                          "<Projection>EPSG:3857</Projection>"
                          "<BandsCount>4</BandsCount>"
                          "<BlockSizeX>512</BlockSizeX>"
                          "<BlockSizeY>512</BlockSizeY>"
                          "<UserAgent>Mozilla/5.0 ka-hgis/0.3</UserAgent>"
                          "<Referer>https://localhost</Referer>"
                          "</GDAL_WMS>")
                          .arg(apiKey.trimmed(), layers);
  // 예전에는 이 파일을 윈도우 임시폴더에 뒀다. 프로젝트에는 상대경로
  // "../../AppData/Local/Temp/ka-hgis-vworld-cadastral.xml" 로 적혀서,
  // 임시폴더가 비워지거나 조사 파일을 다른 폴더로 옮기면 지적 레이어가 깨졌다.
  // 앱 전용 폴더에 두어 세션이 바뀌어도 남게 한다.
  const QString xmlPath = kaVworldCadastralXmlPath();
  {
    QFile f(xmlPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
      if (errorOut) *errorOut = QStringLiteral("지적 설정 파일을 쓰지 못했습니다.");
      return false;
    }
    f.write(xml.toUtf8());
  }

  const QString name = QStringLiteral("VWorld 지적(본번·부번)");
  QStringList removeIds;
  for (QgsMapLayer* old : project->mapLayers()) {
    if (!old) continue;
    const QString n = old->name();
    if (n == name || n.startsWith(name + QLatin1String(" [")))
      removeIds.append(old->id());
  }
  for (const QString& id : removeIds)
    project->removeMapLayer(id);

  auto* rl = new QgsRasterLayer(xmlPath, name, QStringLiteral("gdal"));
  if (!rl->isValid()) {
    if (errorOut)
      *errorOut = friendlyBasemapError(rl->error().message());
    delete rl;
    return false;
  }
  // OTF only works if the layer CRS is the server CRS (3857), not the work CRS.
  rl->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:3857")));
  LayerOps::markCadastralLayer(rl);
  ReferenceKind::tag(rl, QString::fromLatin1(ReferenceKind::kCadastralPicture));
  rl->setOpacity(1.0);
  QgsMapLayer* added = project->addMapLayer(rl, false);
  if (!added) {
    delete rl;
    if (errorOut) *errorOut = QStringLiteral("지적 레이어를 프로젝트에 넣지 못했습니다.");
    return false;
  }
  LayerOps::applyLegendCrsLabel(added);
  LayerOps::placeCadastralLayer(project, added);
  if (canvas) {
    const QString workAuth = project->crs().isValid() ? project->crs().authid()
                                                      : QStringLiteral("EPSG:5186");
    LayerOps::ensureOtfEnabled(project, canvas, workAuth);
    syncCanvasToProject(project, canvas);
    if (canvas->scale() > 80000.0 || canvas->scale() < 200.0)
      zoomCanvasToWorkingScale(canvas, workAuth, 25000.0);
    LayerOps::refreshXyzBasemapTiles(canvas);
  }
  return true;
}

QStringList LayerOps::cadastralWmsCrsCandidates(const QString& workCrsAuthId) {
  Q_UNUSED(workCrsAuthId);
  // Do not put 5186/5187/5179 here. Caps only list 4326 bbox; QGIS then
  // reports "Cannot calculate extent" and the layer never draws.
  return {
      QStringLiteral("EPSG:4326"),
      QStringLiteral("EPSG:3857"),
      QStringLiteral("EPSG:900913"),
  };
}

bool LayerOps::addVworldCadastralMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut) {
  if (!requireVworldKey(apiKey, errorOut)) return false;
  const QString workCrs = (project && project->crs().isValid())
                              ? project->crs().authid()
                              : QStringLiteral("EPSG:5186");
  // Restore GitHub baseline path that drew parcels: EPSG:3857 tiled WMS, empty styles.
  const QString uriBoth = makeVworldWmsUri(
      apiKey, QStringLiteral("lp_pa_cbnd_bonbun,lp_pa_cbnd_bubun"), QString(), QStringLiteral("EPSG:3857"));
  const QString uriBon =
      makeVworldWmsUri(apiKey, QStringLiteral("lp_pa_cbnd_bonbun"), QString(), QStringLiteral("EPSG:3857"));
  const QString uriBu =
      makeVworldWmsUri(apiKey, QStringLiteral("lp_pa_cbnd_bubun"), QString(), QStringLiteral("EPSG:3857"));

  QString err;
  bool ok = addXyzBasemap(project, canvas, uriBoth, QStringLiteral("VWorld 지적(본번·부번)"), &err, true);
  if (!ok) {
    const bool okBon = addXyzBasemap(project, canvas, uriBon, QStringLiteral("VWorld 지적 본번"), &err, true);
    const bool okBu = addXyzBasemap(project, canvas, uriBu, QStringLiteral("VWorld 지적 부번"), &err, true);
    ok = okBon || okBu;
  }
  if (!ok)
    ok = addGdalVworldCadastral(project, canvas, apiKey,
                                QStringLiteral("lp_pa_cbnd_bonbun,lp_pa_cbnd_bubun"), &err);
  if (!ok) {
    if (errorOut)
      *errorOut = err.isEmpty() ? QStringLiteral("지적도 WMS 추가 실패") : err;
    return false;
  }
  if (canvas) {
    LayerOps::ensureOtfEnabled(project, canvas, workCrs);
    for (QgsMapLayer* l : project->mapLayers()) {
      if (!l) continue;
      const QString n = l->name();
      // VWorld pictures by identity (F035): a downloaded 지적도 titled "지적" keeps its own CRS.
      // (The former QLatin1String("지적…") title checks compared UTF-8 bytes as Latin-1 and
      // never matched.) Untagged pictures from older projects still carry "VWorld" in the title.
      const QString kind = ReferenceKind::of(l);
      const bool vworldCad = kind == QLatin1String(ReferenceKind::kCadastralPicture) ||
                             (kind.isEmpty() && n.contains(QStringLiteral("VWorld")) &&
                              n.contains(QStringLiteral("지적")));
      if (vworldCad)
        l->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:3857")));
    }
    LayerOps::syncMapCanvas(project, canvas, false);
    const double s = canvas->scale();
    if (s > 80000.0 || s < 200.0)
      canvas->zoomScale(25000.0, true);
    LayerOps::refreshXyzBasemapTiles(canvas);
  }
  return true;
}

double LayerOps::suggestCadastralScale(double currentScale, double target, double maxOk) {
  if (currentScale <= 0.0) return target;
  if (currentScale > maxOk) return target;
  return currentScale;
}

LayerOps::FieldBasemapPackResult LayerOps::prepareFieldBasemapPack(
    QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey,
    const QString& workCrsAuthId, QString* errorOut) {
  FieldBasemapPackResult r;
  if (!requireVworldKey(apiKey, errorOut)) return r;

  const QString work = workCrsAuthId.trimmed().isEmpty() ? QStringLiteral("EPSG:5186")
                                                         : workCrsAuthId.trimmed();
  ensureOtfEnabled(project, canvas, work);

  QgsPointXY keepCenter;
  bool hadLocal = false;
  double keepScale = 10000.0;
  if (canvas) {
    const QgsRectangle e = canvas->extent();
    if (!e.isEmpty() && e.isFinite() && e.width() > 0 && canvas->scale() > 50.0 &&
        canvas->scale() < 500000.0) {
      keepCenter = e.center();
      hadLocal = true;
      keepScale = canvas->scale();
    }
  }

  QString satErr;
  r.satelliteOk = addVworldSatelliteMap(project, canvas, apiKey, &satErr);
  QString cadErr;
  r.cadastralOk = addVworldCadastralMap(project, canvas, apiKey, &cadErr);

  if (!r.satelliteOk && !r.cadastralOk) {
    if (errorOut) {
      *errorOut = satErr.isEmpty() ? cadErr : satErr;
      if (errorOut->isEmpty())
        *errorOut = QStringLiteral("현장 배경(위성·지적) 추가 실패");
    }
    return r;
  }

  if (canvas) {
    ensureOtfEnabled(project, canvas, work);
    syncMapCanvas(project, canvas, false);
    if (hadLocal) {
      canvas->setCenter(keepCenter);
      const double next = suggestCadastralScale(keepScale, 5000.0, 15000.0);
      canvas->zoomScale(next, true);
    } else {
      const double next = suggestCadastralScale(canvas->scale(), 4000.0, 5000.0);
      if (qAbs(next - canvas->scale()) > 1.0)
        canvas->zoomScale(next, true);
    }
    clampCanvasToKorea(canvas);
    refreshCanvasIfIdle(canvas);
  }
  if (errorOut && (!r.satelliteOk || !r.cadastralOk)) {
    QStringList parts;
    if (!r.satelliteOk && !satErr.isEmpty()) parts << satErr;
    if (!r.cadastralOk && !cadErr.isEmpty()) parts << cadErr;
    *errorOut = parts.join(QStringLiteral(" / "));
  }
  return r;
}

bool LayerOps::addVworldHybridMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut) {
  if (!requireVworldKey(apiKey, errorOut)) return false;
  const QString key = apiKey.trimmed();
  const QStringList uris = {
      QStringLiteral(
          "type=xyz&url=https://api.vworld.kr/req/wmts/1.0.0/%1/Hybrid/%7Bz%7D/%7By%7D/%7Bx%7D.png"
          "&zmax=19&zmin=0&crs=EPSG:3857&tilePixelRatio=1")
          .arg(key),
      QStringLiteral(
          "type=xyz&url=https://xdworld.vworld.kr/2d/Hybrid/service/%7Bz%7D/%7Bx%7D/%7By%7D.png"
          "&zmax=19&zmin=0&crs=EPSG:3857&tilePixelRatio=1"),
  };
  return addBasemapWithFallbacks(project, canvas, uris, QStringLiteral("VWorld 하이브리드"), errorOut);
}

bool LayerOps::addVworldContourMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut) {
  if (!requireVworldKey(apiKey, errorOut)) return false;
  const QStringList uris = {
      makeVworldWmsUri(apiKey, QStringLiteral("lt_c_upisuq"), QStringLiteral("lt_c_upisuq"),
                       QStringLiteral("EPSG:3857")),
      makeVworldWmsUri(apiKey, QStringLiteral("lt_c_upisuq"), QString(), QStringLiteral("EPSG:3857")),
  };
  return addBasemapWithFallbacks(project, canvas, uris, QStringLiteral("VWorld 등고선"), errorOut);
}

bool LayerOps::historyGisApiKeyUsable(const QString& apiKey) {
  return !apiKey.trimmed().isEmpty();
}

QString LayerOps::historyGisWmtsUri(const QString& apiKey, const QString& layerId) {
  const QString key = apiKey.trimmed();
  const QString layer = layerId.trimmed().isEmpty() ? QStringLiteral("history:map1919")
                                                    : layerId.trimmed();
  // Official KVP GetCapabilities: https://hgis.history.go.kr/api/intro.do
  // QGIS 3.44 WMTS KVP: append SERVICE=WMTS&REQUEST=GetCapabilities
  // https://docs.qgis.org/3.44/en/docs/user_manual/working_with_ogc/ogc_client_support.html
  QUrl caps(QStringLiteral("https://hgis.history.go.kr/openapi/get.do"));
  QUrlQuery query;
  query.addQueryItem(QStringLiteral("Service"), QStringLiteral("WMTS"));
  query.addQueryItem(QStringLiteral("Request"), QStringLiteral("GetCapabilities"));
  query.addQueryItem(QStringLiteral("apiKey"), key);
  caps.setQuery(query);
  const QString encUrl = QString::fromLatin1(QUrl::toPercentEncoding(caps.toString(QUrl::FullyEncoded)));
  return QStringLiteral(
             "contextualWMSLegend=0&crs=EPSG:5179&dpiMode=7&format=image/png"
             "&layers=%1&styles&tileMatrixSet=EPSG:5179&url=%2")
      .arg(layer, encUrl);
}

// The server only has history:map1919 (bare map1919 is not in its capabilities), but it answers
// GetCapabilities with HTTP 400 「GWC configuration out of synch」 about half the time (checked 2026-10-04,
// 3 of 6 requests): the same request is tried four times.
QStringList LayerOps::historyGisMap1919Uris(const QString& apiKey) {
  return QStringList(4, historyGisWmtsUri(apiKey, QStringLiteral("history:map1919")));
}

QString LayerOps::historyGisFailureText(const QString& providerError, const QString& apiKey) {
  QString detail = providerError;
  const QString key = apiKey.trimmed();
  if (!key.isEmpty()) {
    detail.replace(key, QStringLiteral("[가림]"));
    detail.replace(QString::fromLatin1(QUrl::toPercentEncoding(key)), QStringLiteral("[가림]"));
  }
  return QStringLiteral("국사편찬위원회 역사지리정보DB 서버가 지도 목록을 보내 주지 않았습니다(4번 시도). "
                        "서버 쪽이 잠시 불안정할 때 생기는 일이니 잠시 뒤 다시 눌러 보세요.\n\n") +
         detail;
}

bool LayerOps::addHistoryGisMap1919(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey,
                                    QString* errorOut) {
  if (!historyGisApiKeyUsable(apiKey)) {
    if (errorOut) {
      *errorOut = QStringLiteral(
          "1919 조선지형도는 국사편찬위원회 역사지리정보DB API 키가 필요합니다. "
          "더보기 → API 키 입력에서 키를 저장하세요. "
          "https://hgis.history.go.kr/api/intro.do");
    }
    return false;
  }
  const QString key = apiKey.trimmed();
  const QString name = QStringLiteral("1919 조선지형도 1:5만");
  QString failure;
  if (!addBasemapWithFallbacks(project, canvas, historyGisMap1919Uris(key), name, &failure)) {
    if (errorOut) *errorOut = historyGisFailureText(failure, key);
    return false;
  }
  placeKindInReferenceGroup(project, name);
  return true;
}

QString LayerOps::daedongyeojidoWmsUri(const QString& layerId) {
  const QString layer = layerId.trimmed().isEmpty()
                            ? QStringLiteral("korea_oldmap_ddymap_kyu")
                            : layerId.trimmed();
  // Official NGII 국토정보플랫폼 역사지도 WMS. No API key.
  // Viewer: https://map.ngii.go.kr/ms/map/NlipMap.do?tabGb=daedong
  // GetCapabilities 1.3.0/1.1.1 and GetMap PNG confirmed 200 without key (2026-09-20).
  // QGIS 3.44 WMS URI: crs&format&layers&styles&url
  // https://docs.qgis.org/3.44/en/docs/pyqgis_developer_cookbook/loadlayer.html
  // WMS 1.1.1: yesterday's 1.3.0 XY GetMap was empty; 1.1.1 returned a real PNG.
  QUrl caps(QStringLiteral("https://map.ngii.go.kr/spcemapserver/korea_old_map/ows"));
  const QString encUrl = QString::fromLatin1(QUrl::toPercentEncoding(caps.toString(QUrl::FullyEncoded)));
  return QStringLiteral(
             "contextualWMSLegend=0&crs=EPSG:5179&dpiMode=7&format=image/png"
             "&transparent=true&version=1.1.1&layers=%1&styles&url=%2")
      .arg(layer, encUrl);
}

bool LayerOps::addDaedongyeojidoMap(QgsProject* project, QgsMapCanvas* canvas, QString* errorOut) {
  const QString name = QStringLiteral("대동여지도");
  const QStringList uris = {
      daedongyeojidoWmsUri(QStringLiteral("korea_oldmap_ddymap_kyu")),
      daedongyeojidoWmsUri(QStringLiteral("korea_oldmap_addAlphaChannel")),
  };
  if (!addBasemapWithFallbacks(project, canvas, uris, name, errorOut))
    return false;
  placeKindInReferenceGroup(project, name);
  return true;
}

bool LayerOps::addElevationHillshadeMap(QgsProject* project, QgsMapCanvas* canvas,
                                        const QString& apiKey, QString* errorOut) {
  Q_UNUSED(apiKey);
  const QString name = QStringLiteral("지형맵");
  // XYZ only. VWorld WMS GetMap + OTF 5186 + pan was crash-20260901-102801
  // (provider_wms deleteLater / QgsRasterProjector).
  const QStringList uris = {
      QStringLiteral(
          "type=xyz&url=https://tile.opentopomap.org/%7Bz%7D/%7Bx%7D/%7By%7D.png"
          "&zmax=17&zmin=5&crs=EPSG:3857&tilePixelRatio=1"),
      QStringLiteral(
          "type=xyz&url=https://a.tile.opentopomap.org/%7Bz%7D/%7Bx%7D/%7By%7D.png"
          "&zmax=17&zmin=5&crs=EPSG:3857&tilePixelRatio=1"),
  };
  if (!addBasemapWithFallbacks(project, canvas, uris, name, errorOut))
    return false;
  placeKindInReferenceGroup(project, name);
  return true;
}

// An earlier copy of the same offline pack: the same file, or the same title compared
// directly, so the online satellite ("위성") is never taken for the pack (F035).
static void removeTilePackCopies(QgsProject* project, const QString& path, const QString& name) {
  if (!project) return;
  const QString file = QFileInfo(path).absoluteFilePath();
  QStringList ids;
  for (QgsMapLayer* old : project->mapLayers()) {
    if (!old) continue;
    const bool sameFile = old->providerType() == QLatin1String("gdal") &&
                          QFileInfo(old->source()).absoluteFilePath().compare(file, Qt::CaseInsensitive) == 0;
    if (sameFile || legendTitlesMatchDirect(old->name(), name))
      ids.append(old->id());
  }
  for (const QString& id : ids)
    project->removeMapLayer(id);
}

// Our previous layer of `kind`, even when the user renamed it; an untagged layer of the
// kind's old title for projects saved before tagging.
static void removeReferenceKind(QgsProject* project, const char* kind) {
  if (!project) return;
  const QString wanted = QString::fromLatin1(kind);
  QStringList ids;
  for (QgsMapLayer* old : project->mapLayers()) {
    if (old && ReferenceKind::is(old, wanted))
      ids.append(old->id());
  }
  for (const QString& id : ids)
    project->removeMapLayer(id);
}

static QString copernicusCogVsicurl(int latFloor, int lonFloor) {
  const QString ns = latFloor >= 0
                         ? QStringLiteral("N%1").arg(latFloor, 2, 10, QChar('0'))
                         : QStringLiteral("S%1").arg(-latFloor, 2, 10, QChar('0'));
  const QString ew = lonFloor >= 0
                         ? QStringLiteral("E%1").arg(lonFloor, 3, 10, QChar('0'))
                         : QStringLiteral("W%1").arg(-lonFloor, 3, 10, QChar('0'));
  const QString tile = QStringLiteral("Copernicus_DSM_COG_10_%1_00_%2_00_DEM").arg(ns, ew);
  return QStringLiteral("/vsicurl/https://copernicus-dem-30m.s3.amazonaws.com/%1/%1.tif").arg(tile);
}

QString LayerOps::copernicusCogUriForWgs84(double latDeg, double lonDeg) {
  const int latFloor = static_cast<int>(std::floor(latDeg));
  const int lonFloor = static_cast<int>(std::floor(lonDeg));
  return copernicusCogVsicurl(latFloor, lonFloor);
}

// 화면에 보이는 범위를 덮는 Copernicus 1°x1° 타일을 모두 모아 VRT 한 장으로 붙인다.
// 예전에는 화면 한가운데 칸 하나만 올려서, 조금만 옮기면 DEM이 사라졌다.
// 넓게 보고 있을 때 수십 장을 원격으로 여는 것은 느리므로 상한을 두고,
// 화면 한가운데에서 가까운 칸부터 채운다.
static constexpr int kDemMosaicMaxTiles = 24;

// 이 DEM이 덮고 있는 경위도 범위. 다시 만들지 판단할 때 쓴다.
static const char* kDemCoverProp = "ka_hgis/dem_cover_wgs84";

static bool tryAddCopernicusViewDem(QgsProject* project, QgsMapCanvas* canvas, QString* errorOut) {
  if (!project || !canvas) return false;
  try {
    QgsRectangle ext = canvas->extent();
    if (ext.isEmpty() || !ext.isFinite()) return false;
    QgsCoordinateReferenceSystem wgs(QStringLiteral("EPSG:4326"));
    const QgsCoordinateReferenceSystem canvasCrs = canvas->mapSettings().destinationCrs();
    QgsRectangle wgsExt = ext;
    if (canvasCrs.isValid() && canvasCrs != wgs) {
      try {
        const QgsCoordinateTransform tr(canvasCrs, wgs, QgsCoordinateTransformContext());
        wgsExt = tr.transformBoundingBox(ext);
      } catch (...) {
        KA_LOG_EXCEPT();
        return false;
      }
    }
    wgsExt = wgsExt.intersect(QgsRectangle(-180.0, -90.0, 180.0, 90.0));
    if (wgsExt.isEmpty() || !wgsExt.isFinite()) return false;

    const int lonFrom = static_cast<int>(std::floor(wgsExt.xMinimum()));
    const int lonTo = static_cast<int>(std::floor(wgsExt.xMaximum()));
    const int latFrom = static_cast<int>(std::floor(wgsExt.yMinimum()));
    const int latTo = static_cast<int>(std::floor(wgsExt.yMaximum()));
    if (latFrom < -90 || latTo > 89 || lonFrom < -180 || lonTo > 179)
      return false;

    const QgsPointXY c = wgsExt.center();
    struct DemCell {
      int lat;
      int lon;
      double dist2;
    };
    QVector<DemCell> cells;
    for (int la = latFrom; la <= latTo; ++la) {
      for (int lo = lonFrom; lo <= lonTo; ++lo) {
        const double dx = (lo + 0.5) - c.x();
        const double dy = (la + 0.5) - c.y();
        cells.append({la, lo, dx * dx + dy * dy});
      }
    }
    if (cells.isEmpty()) return false;
    std::sort(cells.begin(), cells.end(),
              [](const DemCell& a, const DemCell& b) { return a.dist2 < b.dist2; });
    const int askedTiles = cells.size();
    if (askedTiles > kDemMosaicMaxTiles)
      cells.resize(kDemMosaicMaxTiles);

    CPLSetConfigOption("GDAL_DISABLE_READDIR_ON_OPEN", "EMPTY_DIR");
    // 바다 칸은 Copernicus 버킷에 파일 자체가 없다. 열리는 것만 골라 담는다.
    CPLPushErrorHandler(CPLQuietErrorHandler);
    QVector<GDALDatasetH> sources;
    QgsRectangle covered;
    for (const DemCell& cell : cells) {
      const QString uri = copernicusCogVsicurl(cell.lat, cell.lon);
      GDALDatasetH ds = GDALOpenEx(uri.toUtf8().constData(),
                                   GDAL_OF_RASTER | GDAL_OF_READONLY, nullptr, nullptr, nullptr);
      if (!ds) continue;
      sources.append(ds);
      const QgsRectangle one(cell.lon, cell.lat, cell.lon + 1.0, cell.lat + 1.0);
      if (covered.isEmpty())
        covered = one;
      else
        covered.combineExtentWith(one);
    }
    CPLPopErrorHandler();
    if (sources.isEmpty()) {
      if (errorOut)
        *errorOut = QStringLiteral("이 범위에는 받아올 수 있는 DEM 자료가 없습니다(바다이거나 제공되지 않는 구역).");
      return false;
    }

    // 같은 파일에 덮어쓰면 이미 올라가 있는 레이어가 붙들고 있어 실패한다. 매번 새 이름.
    static int mosaicSerial = 0;
    const QString vrtPath =
        QDir(QDir::tempPath())
            .filePath(QStringLiteral("ka-hgis-dem-%1.vrt").arg(++mosaicSerial));
    char* argv[] = {const_cast<char*>("-resolution"), const_cast<char*>("highest"), nullptr};
    GDALBuildVRTOptions* vrtOpts = GDALBuildVRTOptionsNew(argv, nullptr);
    GDALDatasetH vrt = GDALBuildVRT(vrtPath.toUtf8().constData(), sources.size(), sources.data(),
                                    nullptr, vrtOpts, nullptr);
    if (vrtOpts) GDALBuildVRTOptionsFree(vrtOpts);
    if (vrt) GDALClose(vrt);
    for (GDALDatasetH ds : sources) GDALClose(ds);
    if (!vrt || !QFile::exists(vrtPath)) {
      if (errorOut)
        *errorOut = QStringLiteral("DEM 타일을 하나로 붙이지 못했습니다.");
      return false;
    }

    auto* rl = new QgsRasterLayer(vrtPath, QStringLiteral("DEM"), QStringLiteral("gdal"));
    if (!rl->isValid() || rl->bandCount() < 1) {
      if (errorOut) *errorOut = rl->error().message();
      delete rl;
      return false;
    }
    rl->setCustomProperty(QString::fromLatin1(kDemCoverProp),
                          QStringLiteral("%1,%2,%3,%4")
                              .arg(covered.xMinimum(), 0, 'f', 6)
                              .arg(covered.yMinimum(), 0, 'f', 6)
                              .arg(covered.xMaximum(), 0, 'f', 6)
                              .arg(covered.yMaximum(), 0, 'f', 6));
    if (errorOut && askedTiles > cells.size()) {
      // 조용히 잘라 내면 「전체가 덮였다」고 오해한다. 몇 칸만 채웠는지 알린다.
      *errorOut = QStringLiteral("화면이 넓어 가운데 %1칸만 DEM으로 채웠습니다(전체 %2칸). "
                                 "조금 확대한 뒤 DEM을 다시 누르면 그 범위가 채워집니다.")
                      .arg(cells.size())
                      .arg(askedTiles);
    }
    QgsRectangle statsExt;
    if (rl->crs().isValid() && canvasCrs.isValid() && rl->crs() != canvasCrs) {
      try {
        const QgsCoordinateTransform tr(canvasCrs, rl->crs(), QgsCoordinateTransformContext());
        statsExt = tr.transformBoundingBox(ext);
      } catch (...) {
        KA_LOG_EXCEPT();
      }
    } else if (rl->crs() == canvasCrs) {
      statsExt = ext;
    }
    LayerOps::applyDemElevationStyle(rl, statsExt);
    LayerOps::markReferenceLayer(rl);
    // Copernicus GLO-30 is a surface model: say so in the legend and tooltip (F062).
    BasemapDsm::labelCopernicus(rl);
    removeReferenceKind(project, ReferenceKind::kDem);
    if (!project->addMapLayer(rl, true)) {
      delete rl;
      return false;
    }
    LayerOps::placeInLegendGroup(project, rl, QStringLiteral("참조 지도"));
    LayerOps::ensureDemRelief(project, rl);
    return true;
  } catch (...) {
    KA_LOG_EXCEPT();
    if (errorOut) *errorOut = QStringLiteral("원격 DEM 접근 중 예외가 발생했습니다.");
    return false;
  }
}

bool LayerOps::demCoversCanvas(QgsProject* project, QgsMapCanvas* canvas) {
  if (!project || !canvas) return false;
  QgsRasterLayer* dem = nullptr;
  // By identity, not title: the DSM layer is titled 「지표모델(DSM) …」 and may be renamed.
  for (QgsMapLayer* ml : ReferenceKind::find(project, QString::fromLatin1(ReferenceKind::kDem))) {
    dem = qobject_cast<QgsRasterLayer*>(ml);
    if (dem) break;
  }
  if (!dem) return false;
  const QString cover = dem->customProperty(QString::fromLatin1(kDemCoverProp)).toString();
  const QStringList parts = cover.split(QLatin1Char(','), Qt::SkipEmptyParts);
  if (parts.size() != 4) return false;  // 옛 방식(한 칸짜리)으로 올라온 DEM은 다시 만든다.
  bool ok = true;
  double v[4] = {0, 0, 0, 0};
  for (int i = 0; i < 4; ++i) {
    bool one = false;
    v[i] = parts.at(i).toDouble(&one);
    ok = ok && one;
  }
  if (!ok) return false;
  const QgsRectangle covered(v[0], v[1], v[2], v[3]);

  QgsRectangle ext = canvas->extent();
  if (ext.isEmpty() || !ext.isFinite()) return true;
  const QgsCoordinateReferenceSystem wgs(QStringLiteral("EPSG:4326"));
  const QgsCoordinateReferenceSystem canvasCrs = canvas->mapSettings().destinationCrs();
  if (canvasCrs.isValid() && canvasCrs != wgs) {
    try {
      const QgsCoordinateTransform tr(canvasCrs, wgs, QgsCoordinateTransformContext());
      ext = tr.transformBoundingBox(ext);
    } catch (...) {
      KA_LOG_EXCEPT();
      return true;
    }
  }
  return covered.contains(ext);
}

double LayerOps::demElevationClassStep(double zMin, double zMax) {
  if (!std::isfinite(zMin) || !std::isfinite(zMax) || zMax <= zMin) return 20.0;
  // About 8 legend rows. 5 m steps on a 120 m site made a 24-line tree.
  const double raw = (zMax - zMin) / 8.0;
  const double nice[] = {1.0, 2.0, 5.0, 10.0, 15.0, 20.0, 25.0, 50.0, 100.0, 200.0, 250.0, 500.0};
  for (double s : nice) {
    if (s + 1e-9 >= raw) return s;
  }
  return 500.0;
}

namespace {

QColor demRampColor(double t) {
  const QColor cols[] = {QColor(48, 18, 59),  QColor(33, 102, 172), QColor(67, 170, 139),
                         QColor(201, 219, 87), QColor(253, 174, 97), QColor(165, 0, 38)};
  t = std::clamp(t, 0.0, 1.0);
  const double x = t * 5.0;
  const int i = std::min(4, static_cast<int>(std::floor(x)));
  const double u = x - double(i);
  const QColor& a = cols[i];
  const QColor& b = cols[i + 1];
  return QColor(int(a.red() + (b.red() - a.red()) * u),
                int(a.green() + (b.green() - a.green()) * u),
                int(a.blue() + (b.blue() - a.blue()) * u));
}

}  // namespace

QList<LayerOps::DemElevationClass> LayerOps::buildDemElevationClasses(double zMin, double zMax,
                                                                      int classCount,
                                                                      double stepMeters) {
  if (!std::isfinite(zMin) || !std::isfinite(zMax) || zMax <= zMin) {
    zMin = 0.0;
    zMax = 200.0;
  }
  if (zMax - zMin < 2.0) {
    zMin -= 1.0;
    zMax += 1.0;
  }
  double step = stepMeters;
  if (!(step > 0.0)) step = demElevationClassStep(zMin, zMax);
  const double z0 = std::floor(zMin / step) * step;
  double z1 = std::ceil(zMax / step) * step;
  if (z1 <= z0) z1 = z0 + step;
  int n = classCount;
  if (n < 2) n = static_cast<int>(std::lround((z1 - z0) / step));
  if (n < 2) n = 2;
  if (classCount < 2 && n > 8) n = 8;
  if (n > 12) n = 12;
  QList<DemElevationClass> out;
  out.reserve(n);
  for (int i = 1; i <= n; ++i) {
    DemElevationClass c;
    c.lo = z0 + step * double(i - 1);
    const double hi = z0 + step * double(i);
    const bool last = (i == n);
    c.hi = last ? std::numeric_limits<double>::infinity() : hi;
    const double t = n <= 1 ? 0.0 : double(i - 1) / double(n - 1);
    c.color = demRampColor(t);
    c.label = last ? QStringLiteral("%1 m 이상").arg(c.lo, 0, 'f', 0)
                   : QStringLiteral("%1–%2 m").arg(c.lo, 0, 'f', 0).arg(hi, 0, 'f', 0);
    out.append(c);
  }
  return out;
}

QList<LayerOps::DemElevationClass> LayerOps::readDemElevationClasses(const QgsRasterLayer* layer) {
  QList<DemElevationClass> out;
  if (!layer) return out;
  auto* rend = dynamic_cast<const QgsSingleBandPseudoColorRenderer*>(layer->renderer());
  if (!rend || !rend->shader()) return out;
  auto* fn = dynamic_cast<const QgsColorRampShader*>(rend->shader()->rasterShaderFunction());
  if (!fn) return out;
  const QList<QgsColorRampShader::ColorRampItem> items = fn->colorRampItemList();
  double prev = rend->classificationMin();
  if (!std::isfinite(prev)) prev = 0.0;
  for (int i = 0; i < items.size(); ++i) {
    DemElevationClass c;
    c.lo = prev;
    c.hi = items[i].value;
    c.color = items[i].color;
    c.label = items[i].label;
    out.append(c);
    if (std::isfinite(items[i].value)) prev = items[i].value;
  }
  return out;
}

bool LayerOps::applyDemElevationStyle(QgsRasterLayer* layer) {
  return applyDemElevationStyle(layer, QgsRectangle(), DemElevationStyle());
}

bool LayerOps::applyDemElevationStyle(QgsRasterLayer* layer, const QgsRectangle& statsExtent) {
  return applyDemElevationStyle(layer, statsExtent, DemElevationStyle());
}

bool LayerOps::applyDemElevationStyle(QgsRasterLayer* layer, const QgsRectangle& statsExtent,
                                      const DemElevationStyle& style) {
  // Legacy custom tables become continuous value/color stops; the public UI uses presets.
  if (style.classes.isEmpty()) return DemPresentation::apply(layer,
      statsExtent.isEmpty() ? QStringLiteral("national") : QStringLiteral("viewport"), statsExtent);
  if (!layer || !layer->isValid() || style.classes.size() < 2) return false;
  QList<QgsColorRampShader::ColorRampItem> items;
  double previous = -std::numeric_limits<double>::infinity();
  for (const auto& entry : style.classes) {
    if (!std::isfinite(entry.lo) || entry.lo <= previous || !entry.color.isValid()) return false;
    items.append({entry.lo, entry.color, entry.label});
    previous = entry.lo;
  }
  auto* ramp = new QgsColorRampShader(items.first().value, items.last().value);
  ramp->setColorRampType(Qgis::ShaderInterpolationMethod::Linear);
  ramp->setClip(false);
  ramp->setColorRampItemList(items);
  auto* shader = new QgsRasterShader(items.first().value, items.last().value);
  shader->setRasterShaderFunction(ramp);
  auto* renderer = new QgsSingleBandPseudoColorRenderer(layer->dataProvider(), 1, shader);
  renderer->setClassificationMin(items.first().value);
  renderer->setClassificationMax(items.last().value);
  layer->setRenderer(renderer);
  layer->setCustomProperty(QStringLiteral("ka_hgis/dem_display_version"), 1);
  layer->setCustomProperty(QStringLiteral("ka_hgis/dem_preset"), QStringLiteral("custom"));
  DemColorRampLegend::install(layer);
  layer->triggerRepaint();
  return true;
}

bool LayerOps::addTilePackBasemap(QgsProject* project, QgsMapCanvas* canvas, const QString& path,
                                  const QString& name, QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("프로젝트가 없습니다.");
    return false;
  }
  if (!QFileInfo::exists(path)) {
    if (errorOut) *errorOut = QStringLiteral("타일팩 파일이 없습니다: %1").arg(path);
    return false;
  }
  removeTilePackCopies(project, path, name);
  auto* rl = new QgsRasterLayer(path, name, QStringLiteral("gdal"));
  if (!rl->isValid()) {
    if (errorOut)
      *errorOut = QStringLiteral("타일팩을 열 수 없습니다: %1").arg(rl->error().message());
    delete rl;
    return false;
  }
  markReferenceLayer(rl);
  // An offline copy is its own layer, never a duplicate satellite to prune (F035).
  ReferenceKind::tag(rl, QString::fromLatin1(ReferenceKind::kTilePack));
  QgsMapLayer* added = project->addMapLayer(rl, true);
  if (!added) {
    if (errorOut) *errorOut = QStringLiteral("타일팩 레이어를 넣지 못했습니다.");
    delete rl;
    return false;
  }
  applyLegendCrsLabel(added);
  if (QgsLayerTree* root = project->layerTreeRoot()) {
    if (QgsLayerTreeLayer* node = root->findLayer(added->id()))
      node->setItemVisibilityChecked(true);
  }
  pruneEmptyLegendGroups(project);
  if (canvas)
    refreshCanvasIfIdle(canvas);
  return true;
}

bool LayerOps::addDemElevationRaster(QgsProject* project, QgsMapCanvas* canvas, const QString& path,
                                     QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("프로젝트가 없습니다.");
    return false;
  }
  const QFileInfo fi(path);
  if (!fi.exists()) {
    if (errorOut) *errorOut = QStringLiteral("DEM 파일이 없습니다.");
    return false;
  }
  auto* rl = new QgsRasterLayer(path, QStringLiteral("DEM"), QStringLiteral("gdal"));
  if (!rl->isValid() || rl->bandCount() < 1) {
    if (errorOut)
      *errorOut = QStringLiteral("국토지리원 DEM(.img)을 열 수 없습니다: %1")
                      .arg(rl->error().message());
    delete rl;
    return false;
  }
  LayerOps::applyDemElevationStyle(rl);
  LayerOps::markReferenceLayer(rl);
  LayerOps::applyLegendCrsLabel(rl);
  ReferenceKind::tag(rl, QString::fromLatin1(ReferenceKind::kDem));
  removeReferenceKind(project, ReferenceKind::kDem);
  if (!project->addMapLayer(rl, true)) {
    delete rl;
    if (errorOut) *errorOut = QStringLiteral("DEM 레이어를 넣지 못했습니다.");
    return false;
  }
  LayerOps::placeInLegendGroup(project, rl, QStringLiteral("참조 지도"));
  LayerOps::ensureDemRelief(project, rl);
  DemPresentation::followCanvas(rl, canvas);
  if (canvas && !canvas->isDrawing()) {
    const QgsRectangle e = rl->extent();
    if (!rl->crs().isGeographic() && e.isFinite() && e.width() > 0 && e.width() < 20000.0 &&
        e.height() < 20000.0)
      LayerOps::zoomToLayerMax(canvas, rl);
    else
      LayerOps::syncMapCanvas(project, canvas, false);
  }
  return true;
}

QgsRasterLayer* LayerOps::ensureDemRelief(QgsProject* project, QgsRasterLayer* demLayer) {
  if (!project || !demLayer || !demLayer->isValid()) return nullptr;
  if (!DemPresentation::restore(demLayer)) {
    demLayer->setCustomProperty(QStringLiteral("ka_hgis/dem_relief_error"),
        QStringLiteral("이 지도는 표고값 DEM이 아닙니다. 단일 밴드 표고 자료를 불러오세요."));
    return nullptr;
  }
  const QString title = QStringLiteral("지형 음영");
  QgsRasterLayer* shade = nullptr;
  for (auto* candidate : ReferenceKind::find(project, QString::fromLatin1(ReferenceKind::kDemRelief))) {
    if (auto* raster = qobject_cast<QgsRasterLayer*>(candidate)) { shade = raster; break; }
  }
  const bool enabled = demLayer->customProperty(QStringLiteral("ka_hgis/dem_relief_enabled"), true).toBool();
  if (!enabled && !shade) return nullptr;
  QString error;
  const QString source = DemPresentation::reliefSource(demLayer, project->crs(), &error);
  if (source.isEmpty()) {
    demLayer->setCustomProperty(QStringLiteral("ka_hgis/dem_relief_error"), error);
    if (shade) {
      if (auto* node = project->layerTreeRoot()->findLayer(shade->id())) node->setItemVisibilityChecked(false);
    }
    return nullptr;
  }
  demLayer->removeCustomProperty(QStringLiteral("ka_hgis/dem_relief_error"));
  const bool created = !shade;
  if (!shade) shade = new QgsRasterLayer(source, title, QStringLiteral("gdal"));
  else if (!shade->isValid() || shade->source() != source || shade->providerType() != QLatin1String("gdal"))
    shade->setDataSource(source, title, QStringLiteral("gdal"));
  if (!shade->isValid()) {
    if (created) delete shade;
    demLayer->setCustomProperty(QStringLiteral("ka_hgis/dem_relief_error"), QStringLiteral("음영 자료를 열지 못했습니다. DEM을 다시 불러오세요."));
    return nullptr;
  }
  auto* renderer = new QgsHillshadeRenderer(shade->dataProvider(), 1, 315., 45.);
  double zFactor = demLayer->customProperty(QStringLiteral("ka_hgis/dem_z_factor"), 1.).toDouble();
  if (!std::isfinite(zFactor)) zFactor = 1.;
  renderer->setZFactor(std::clamp(zFactor, .1, 5.)); // Horizontal and vertical units are both metres.
  renderer->setMultiDirectional(true);
  shade->setRenderer(renderer);
  // Interpolate elevation samples before deriving slopes. Nearest-neighbour
  // enlargement creates false grid-shaped ridges in otherwise smooth terrain.
  shade->dataProvider()->setZoomedInResamplingMethod(Qgis::RasterResamplingMethod::Bilinear);
  shade->dataProvider()->setZoomedOutResamplingMethod(Qgis::RasterResamplingMethod::Bilinear);
  shade->setResamplingStage(Qgis::RasterResamplingStage::Provider);
  shade->setBlendMode(QPainter::CompositionMode_Multiply);
  double strength = demLayer->customProperty(QStringLiteral("ka_hgis/dem_relief_strength"), .30).toDouble();
  if (!std::isfinite(strength)) strength = .30;
  shade->setOpacity(std::clamp(strength, 0., .80));
  shade->setCustomProperty(QStringLiteral("ka_hgis/omit_sheet_legend"), true);
  markReferenceLayer(shade);
  ReferenceKind::tag(shade, QString::fromLatin1(ReferenceKind::kDemRelief));
  if (created && !project->addMapLayer(shade, false)) { delete shade; return nullptr; }
  auto* root = project->layerTreeRoot();
  auto* demNode = root->findLayer(demLayer->id());
  auto* parent = demNode ? qobject_cast<QgsLayerTreeGroup*>(demNode->parent()) : nullptr;
  if (!parent) parent = root;
  auto* shadeNode = root->findLayer(shade->id());
  if (!shadeNode) shadeNode = parent->insertLayer(demNode ? parent->children().indexOf(demNode) : 0, shade);
  else if (demNode && (shadeNode->parent() != parent || parent->children().indexOf(shadeNode) > parent->children().indexOf(demNode))) {
    auto* clone = shadeNode->clone();
    // Insert the clone before removal so the registry bridge retains the datasource.
    parent->insertChildNode(parent->children().indexOf(demNode), clone);
    qobject_cast<QgsLayerTreeGroup*>(shadeNode->parent())->removeChildNode(shadeNode);
    shadeNode = clone;
  }
  {
    const QSignalBlocker blocker(shadeNode);
    shadeNode->setItemVisibilityChecked(enabled && (!demNode || demNode->isVisible()));
  }
  if (shadeNode->property("demVisibilityOwner").toString() != demLayer->id()) {
    delete shadeNode->findChild<QObject*>(QStringLiteral("demVisibilityObserver"), Qt::FindDirectChildrenOnly);
    auto* observer = new QObject(shadeNode);
    observer->setObjectName(QStringLiteral("demVisibilityObserver"));
    shadeNode->setProperty("demVisibilityOwner", demLayer->id());
    const QPointer<QgsRasterLayer> guardedDem(demLayer);
    const QPointer<QgsLayerTreeLayer> guardedNode(demNode);
    QObject::connect(shadeNode, &QgsLayerTreeNode::visibilityChanged, observer,
        [guardedDem, guardedNode, shadeNode](QgsLayerTreeNode*) {
      if (guardedDem && (!guardedNode || guardedNode->isVisible()))
        guardedDem->setCustomProperty(QStringLiteral("ka_hgis/dem_relief_enabled"), shadeNode->itemVisibilityChecked());
    });
  }
  shade->triggerRepaint();
  return shade;
}

bool LayerOps::addDemColorReliefMap(QgsProject* project, QgsMapCanvas* canvas, QString* errorOut) {
  const QString name = QStringLiteral("DEM");
  // One-click: Copernicus GLO-30 /vsicurl/copernicus-dem + applyDemElevationStyle
  // (meter legend). GIBS RGB stays fallback when the COG cannot open.
  if (tryAddCopernicusViewDem(project, canvas, errorOut))
    return true;
  // NASA GIBS ASTER GDEM color + hillshade. XYZ/WMTS REST uses z/y/x.
  // Never VWorld WMS GetMap (crash-20260901-102801).
  const QStringList uris = {
      QStringLiteral(
          "type=xyz&url=https://gibs.earthdata.nasa.gov/wmts/epsg3857/best/"
          "ASTER_GDEM_Color_Shaded_Relief/default/GoogleMapsCompatible_Level12/"
          "%7Bz%7D/%7By%7D/%7Bx%7D.jpeg"
          "&zmax=12&zmin=2&crs=EPSG:3857&tilePixelRatio=1"),
      QStringLiteral(
          "type=xyz&url=https://gibs.earthdata.nasa.gov/wmts/epsg3857/best/"
          "ASTER_GDEM_Color_Shaded_Relief/default/2000-01-01/GoogleMapsCompatible_Level12/"
          "%7Bz%7D/%7By%7D/%7Bx%7D.jpeg"
          "&zmax=12&zmin=2&crs=EPSG:3857&tilePixelRatio=1"),
  };
  if (!addBasemapWithFallbacks(project, canvas, uris, name, errorOut))
    return false;
  placeKindInReferenceGroup(project, name);
  return true;
}

// 토양도 참조 스타일. categoryField 값별 반투명 채움 + 옅은 외곽선.
// 값이 수백 개면(토양부호 등) 범례가 무의미해지므로 단색으로 떨어진다.
static void applySoilCategoryStyle(QgsVectorLayer* layer, const QString& categoryField) {
  if (!layer || !layer->isValid()) return;
  const Qgis::GeometryType gt = layer->geometryType();

  auto makeSymbol = [gt](const QColor& c) -> QgsSymbol* {
    if (gt == Qgis::GeometryType::Polygon) {
      auto fs = QgsFillSymbol::createSimple({
          {QStringLiteral("color"), c.name(QColor::HexArgb)},
          {QStringLiteral("outline_color"), QColor(90, 96, 104, 130).name(QColor::HexArgb)},
          {QStringLiteral("outline_width"), QStringLiteral("0.12")},
          {QStringLiteral("outline_width_unit"), QStringLiteral("MM")},
      });
      return fs.release();
    }
    QgsSymbol* s = std::unique_ptr<QgsSymbol>(QgsSymbol::defaultSymbol(gt)).release();  // QGIS 판마다 반환형이 달라 감싼다
    if (s) s->setColor(c);
    return s;
  };

  const int fieldIdx = categoryField.isEmpty() ? -1 : layer->fields().indexOf(categoryField);
  if (fieldIdx >= 0) {
    constexpr int kMaxCategories = 200;
    const QSet<QVariant> uniq = layer->uniqueValues(fieldIdx, kMaxCategories + 1);
    if (!uniq.isEmpty() && uniq.size() <= kMaxCategories) {
      QStringList sorted;
      QHash<QString, QVariant> byText;
      for (const QVariant& v : uniq) {
        const QString t = v.toString().trimmed();
        if (t.isEmpty()) continue;
        if (!byText.contains(t)) {
          byText.insert(t, v);
          sorted.append(t);
        }
      }
      sorted.sort();
      QgsCategoryList cats;
      int i = 0;
      for (const QString& t : sorted) {
        // 황금각 색상환: 인접 폴리곤이 비슷한 색으로 붙지 않게 한다.
        const QColor c = QColor::fromHsv((i * 47) % 360, 140, 220, 150);
        if (QgsSymbol* sym = makeSymbol(c))
          cats.append(QgsRendererCategory(byText.value(t), sym, t));
        ++i;
      }
      // 빈 값·미분류는 회색 "기타"로 표시(없으면 해당 폴리곤이 아예 안 그려진다).
      if (QgsSymbol* rest = makeSymbol(QColor(150, 150, 150, 90)))
        cats.append(QgsRendererCategory(QVariant(), rest, QStringLiteral("기타")));
      if (!cats.isEmpty()) {
        layer->setRenderer(new QgsCategorizedSymbolRenderer(categoryField, cats));
        layer->triggerRepaint();
        return;
      }
    }
  }
  if (QgsSymbol* sym = makeSymbol(QColor(189, 183, 107, 110))) {
    layer->setRenderer(new QgsSingleSymbolRenderer(sym));
    layer->triggerRepaint();
  }
}

QgsVectorLayer* LayerOps::addSoilShapefile(QgsProject* project, QgsMapCanvas* canvas,
                                           const QString& path, const QString& crsOverrideAuthId,
                                           const QString& categoryField, QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("프로젝트가 없습니다.");
    return nullptr;
  }
  const QFileInfo fi(path);
  auto* layer = new QgsVectorLayer(path, fi.completeBaseName(), QStringLiteral("ogr"));
  if (!layer->isValid()) {
    if (errorOut)
      *errorOut = QStringLiteral("토양도 SHP를 열 수 없습니다: %1").arg(layer->error().message());
    delete layer;
    return nullptr;
  }

  // 정부 배포 SHP의 DBF는 대부분 CP949. .cpg가 없으면 한글 속성이 깨진다.
  const QString cpg = fi.dir().filePath(fi.completeBaseName() + QStringLiteral(".cpg"));
  if (fi.suffix().compare(QLatin1String("shp"), Qt::CaseInsensitive) == 0 && !QFile::exists(cpg))
    layer->setProviderEncoding(QStringLiteral("CP949"));

  // 흙토람 고시 좌표계 = EPSG:2097(중부원점/Bessel). 사용자가 고른 값이 우선,
  // 아니면 파일 좌표계 유지, 그것도 없으면 2097로 가정한다.
  const QString overrideAuth = crsOverrideAuthId.trimmed();
  if (!overrideAuth.isEmpty())
    layer->setCrs(QgsCoordinateReferenceSystem(overrideAuth));
  else if (!layer->crs().isValid())
    layer->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:2097")));

  applySoilCategoryStyle(layer, categoryField.trimmed());

  LayerOps::markReferenceLayer(layer);
  ReferenceKind::tag(layer, QString::fromLatin1(ReferenceKind::kSoil));
  LayerOps::applyLegendCrsLabel(layer);
  if (!project->addMapLayer(layer, true)) {
    delete layer;
    if (errorOut) *errorOut = QStringLiteral("토양도 레이어를 프로젝트에 넣지 못했습니다.");
    return nullptr;
  }
  LayerOps::placeInLegendGroup(project, layer, QStringLiteral("참조 지도"));
  LayerOps::applyThematicOverlayScaleRange(layer);
  if (canvas) {
    const QString workAuth = project->crs().isValid() ? project->crs().authid()
                                                      : QStringLiteral("EPSG:5186");
    LayerOps::ensureOtfEnabled(project, canvas, workAuth);
    LayerOps::syncMapCanvas(project, canvas, false);
    LayerOps::zoomToLayerMax(canvas, layer);
  }
  return layer;
}

// Layers a ribbon/menu title refers to (F035).
// - Titles of our reference layers ("DEM", "지형맵", "VWorld 위성", "지질도(KIGAM 1:5만)" …)
//   resolve to ka_hgis/reference_kind: a renamed layer still answers, and a user layer with
//   a similar title ("위성사진_판독") never does. Untagged layers from older projects are
//   found by the kind's old title (ReferenceKind::find).
// - Any other title is an exact legend title (apart from the " [EPSG:…]" suffix). Only when
//   no layer has it, the folded short title ("지적" for "VWorld 지적 본번") is tried, and
//   then only on reference/background layers, never on survey or imported user layers.
static QList<QgsMapLayer*> layersMatchingBaseName(QgsProject* project, const QString& name) {
  QList<QgsMapLayer*> out;
  if (!project) return out;
  const QString kind = ReferenceKind::forTitle(name);
  if (!kind.isEmpty()) return ReferenceKind::find(project, kind);
  const auto layers = project->mapLayers();
  for (QgsMapLayer* l : layers) {
    if (l && legendTitlesMatchDirect(l->name(), name)) out.append(l);
  }
  if (!out.isEmpty()) return out;
  for (QgsMapLayer* l : layers) {
    if (l && LayerOps::isReferenceOrBasemapLayer(l) && legendTitlesMatch(l->name(), name))
      out.append(l);
  }
  return out;
}

bool LayerOps::setLayerOpacity(QgsProject* project, QgsMapCanvas* canvas, const QString& name, double opacity) {
  if (!project) return false;
  const auto layers = layersMatchingBaseName(project, name);
  if (layers.isEmpty()) return false;
  const double op = qBound(0.0, opacity, 1.0);
  for (QgsMapLayer* l : layers) {
    if (l && isReferenceOrBasemapLayer(l)) {
      l->setOpacity(op);
      l->triggerRepaint();
    }
  }
  refreshCanvasIfIdle(canvas);
  return true;
}

double LayerOps::mapLayerOpacity(const QgsMapLayer* layer) {
  if (!layer) return 1.0;
  return layer->opacity();
}

// 오래된 항공사진은 원판이 어둡거나 흐려서 그대로는 지번·경계가 잘 안 보인다.
// 밝기는 QGIS 래스터의 밝기·대비 필터가 그대로 해 준다. 원본 파일은 건드리지 않는다.
bool LayerOps::canAdjustBrightness(const QgsMapLayer* layer) {
  const auto* rl = qobject_cast<const QgsRasterLayer*>(layer);
  return rl && rl->isValid() && rl->brightnessFilter() != nullptr;
}

bool LayerOps::setMapLayerBrightness(QgsMapLayer* layer, int brightness, QgsMapCanvas* canvas) {
  auto* rl = qobject_cast<QgsRasterLayer*>(layer);
  if (!rl || !rl->isValid()) return false;
  QgsBrightnessContrastFilter* f = rl->brightnessFilter();
  if (!f) return false;
  f->setBrightness(qBound(-255, brightness, 255));
  rl->triggerRepaint();
  if (canvas) refreshCanvasIfIdle(canvas);
  return true;
}

int LayerOps::mapLayerBrightness(const QgsMapLayer* layer) {
  const auto* rl = qobject_cast<const QgsRasterLayer*>(layer);
  return (rl && rl->brightnessFilter()) ? rl->brightnessFilter()->brightness() : 0;
}
bool LayerOps::toggleLayerVisibility(QgsProject* project, QgsMapCanvas* canvas, const QString& name, bool visible) {
  if (!project) return false;
  const auto layers = layersMatchingBaseName(project, name);
  if (layers.isEmpty()) return false;
  QgsLayerTree* root = project->layerTreeRoot();
  for (QgsMapLayer* l : layers) {
    if (QgsLayerTreeLayer* node = l ? root->findLayer(l->id()) : nullptr) {
      if (visible) node->setItemVisibilityCheckedParentRecursive(true);
      else node->setItemVisibilityChecked(false);
    }
  }
  refreshCanvasIfIdle(canvas);
  return true;
}

void LayerOps::revealCheckedLegendNode(QgsLayerTreeNode* node) {
  if (!node || !node->itemVisibilityChecked()) return;
  if (auto* group = qobject_cast<QgsLayerTreeGroup*>(node)) {
    const auto leaves = group->findLayers();
    if (std::none_of(leaves.begin(), leaves.end(), [](const QgsLayerTreeLayer* l) { return l && l->itemVisibilityChecked(); }))
      group->setItemVisibilityCheckedRecursive(true);
  }
  if (!node->isVisible()) node->setItemVisibilityCheckedParentRecursive(true);
}

bool LayerOps::isLayerVisible(QgsProject* project, const QString& name) {
  if (!project) return false;
  const auto layers = layersMatchingBaseName(project, name);
  if (layers.isEmpty()) return false;
  QgsLayerTree* root = project->layerTreeRoot();
  for (QgsMapLayer* l : layers) {
    if (!l) continue;
    if (QgsLayerTreeLayer* node = root->findLayer(l->id())) {
      if (node->isVisible()) return true;
    }
  }
  return false;
}

void LayerOps::refreshCanvasIfIdle(QgsMapCanvas* canvas) {
  if (!canvas) return;
  // 병렬 렌더만 끈다. 미리보기는 유지해야 팬·줌에서 지도가 안 꺼진다.
  canvas->setParallelRenderingEnabled(false);
  // 그리는 중이면 예전처럼 버리지 않고 끝난 뒤로 미룬다.
  refreshCanvasNowOrLater(canvas);
}

bool LayerOps::addKoreaBasemap(QgsProject* project, QgsMapCanvas* canvas, KoreaBasemap kind,
                               QString* errorOut) {
  const QString vworldKey = VworldSettings::loadApiKey();
  switch (kind) {
  case KoreaBasemap::VWorldBase:
    return addVworldBaseMap(project, canvas, vworldKey, errorOut);
  case KoreaBasemap::VWorldSatellite:
    return addVworldSatelliteMap(project, canvas, vworldKey, errorOut);
  case KoreaBasemap::VWorldHybrid:
    return addVworldHybridMap(project, canvas, vworldKey, errorOut);
  case KoreaBasemap::GoogleRoad: {
    const QString url = QStringLiteral(
        "type=xyz&url=https://mt1.google.com/vt/lyrs%3Dm%26x%3D%7Bx%7D%26y%3D%7By%7D%26z%3D%7Bz%7D"
        "&zmax=20&zmin=0&crs=EPSG:3857");
    return addXyzBasemap(project, canvas, url, QStringLiteral("Google 도로"), errorOut);
  }
  case KoreaBasemap::GoogleSatellite: {
    const QString url = QStringLiteral(
        "type=xyz&url=https://mt1.google.com/vt/lyrs%3Ds%26x%3D%7Bx%7D%26y%3D%7By%7D%26z%3D%7Bz%7D"
        "&zmax=20&zmin=0&crs=EPSG:3857");
    return addXyzBasemap(project, canvas, url, QStringLiteral("Google 위성"), errorOut);
  }
  case KoreaBasemap::Osm:
  default:
    return addOsmBasemap(project, canvas, errorOut);
  }
}

static QgsRectangle computeKoreaExtentForCrs(const QString& epsgAuthId) {
  const QgsCoordinateReferenceSystem wgs(QStringLiteral("EPSG:4326"));
  const QgsCoordinateReferenceSystem dest(epsgAuthId);
  const QgsRectangle krWgs(124.5, 33.0, 132.0, 39.5);
  if (!dest.isValid()) return krWgs;
  try {
    const QgsCoordinateTransform xf(wgs, dest, QgsCoordinateTransformContext());
    return xf.transformBoundingBox(krWgs);
  } catch (...) {
    KA_LOG_EXCEPT();
    return QgsRectangle();
  }
}

// Cached per destination CRS: clampCanvasToKorea asks on every pan and wheel zoom (F205).
QgsRectangle LayerOps::koreaExtentForCrs(const QString& epsgAuthId) {
  return BasemapExtentCache::get(BasemapExtentCache::Table::Korea, epsgAuthId,
                                 [&epsgAuthId] { return computeKoreaExtentForCrs(epsgAuthId); });
}

static QgsRectangle computeSatelliteFillExtentForCrs(const QString& epsgAuthId) {
  const QgsCoordinateReferenceSystem wgs(QStringLiteral("EPSG:4326"));
  const QgsCoordinateReferenceSystem merc(QStringLiteral("EPSG:3857"));
  const QgsCoordinateReferenceSystem dest(epsgAuthId);
  const QgsRectangle krWgs(124.5, 33.0, 132.0, 39.5);
  const QgsCoordinateTransformContext ctx;
  try {
    const QgsCoordinateTransform toMerc(wgs, merc, ctx);
    const QgsRectangle mercRect = toMerc.transformBoundingBox(krWgs);
    if (!dest.isValid() || dest.authid() == QLatin1String("EPSG:3857"))
      return mercRect;
    const QgsCoordinateTransform toDest(merc, dest, ctx);
    QgsPointXY sw(mercRect.xMinimum(), mercRect.yMinimum());
    QgsPointXY se(mercRect.xMaximum(), mercRect.yMinimum());
    QgsPointXY ne(mercRect.xMaximum(), mercRect.yMaximum());
    QgsPointXY nw(mercRect.xMinimum(), mercRect.yMaximum());
    sw = toDest.transform(sw);
    se = toDest.transform(se);
    ne = toDest.transform(ne);
    nw = toDest.transform(nw);
    const double xMin = std::max(sw.x(), nw.x());
    const double xMax = std::min(se.x(), ne.x());
    const double yMin = std::max(sw.y(), se.y());
    const double yMax = std::min(nw.y(), ne.y());
    if (!(xMax > xMin) || !(yMax > yMin))
      return toDest.transformBoundingBox(mercRect);
    return QgsRectangle(xMin, yMin, xMax, yMax);
  } catch (...) {
    KA_LOG_EXCEPT();
    return LayerOps::koreaExtentForCrs(epsgAuthId);
  }
}

QgsRectangle LayerOps::satelliteFillExtentForCrs(const QString& epsgAuthId) {
  return BasemapExtentCache::get(BasemapExtentCache::Table::SatelliteFill, epsgAuthId,
                                 [&epsgAuthId] { return computeSatelliteFillExtentForCrs(epsgAuthId); });
}

void LayerOps::applyKoreaMapLimits(QgsProject* project, QgsMapCanvas* canvas) {
  const QString auth = (project && project->crs().isValid())
                           ? project->crs().authid()
                           : (canvas && canvas->mapSettings().destinationCrs().isValid()
                                  ? canvas->mapSettings().destinationCrs().authid()
                                  : QStringLiteral("EPSG:5186"));
  QgsRectangle kr = koreaExtentForCrs(auth);
  if (kr.isEmpty() || !kr.isFinite()) return;
  const QgsCoordinateReferenceSystem crs(auth);
  if (project && crs.isValid() && project->viewSettings()) {
    const QgsReferencedRectangle ref(kr, crs);
    project->viewSettings()->setPresetFullExtent(ref);
    project->viewSettings()->setDefaultViewExtent(ref);
  }
  if (canvas && crs.isValid())
    canvas->setDestinationCrs(crs);
}

static QgsRectangle extentFittedInside(const QgsRectangle& kr, double viewAspect) {
  if (kr.isEmpty() || !kr.isFinite() || viewAspect <= 0.05)
    return kr;
  const double krAspect = kr.width() / kr.height();
  if (viewAspect > krAspect) {
    const double h = kr.width() / viewAspect;
    const double cy = kr.center().y();
    return QgsRectangle(kr.xMinimum(), cy - h * 0.5, kr.xMaximum(), cy + h * 0.5);
  }
  const double w = kr.height() * viewAspect;
  const double cx = kr.center().x();
  return QgsRectangle(cx - w * 0.5, kr.yMinimum(), cx + w * 0.5, kr.yMaximum());
}

static double canvasViewAspect(const QgsMapCanvas* canvas) {
  if (!canvas) return 1.0;
  // Fit to the same viewport aspect QGIS uses when expanding setExtent().
  // The outer widget includes the frame and can differ during resize or while
  // hidden; fitting to that aspect can expand the result past the target area.
  const QSize out = canvas->mapSettings().outputSize();
  if (out.width() >= 2 && out.height() >= 2)
    return double(out.width()) / double(out.height());
  const QSize view = canvas->viewport()->size();
  if (view.width() >= 2 && view.height() >= 2)
    return double(view.width()) / double(view.height());
  return 1.0;
}

bool LayerOps::clampCanvasToKorea(QgsMapCanvas* canvas) {
  if (!canvas) return false;
  if (canvas->isDrawing()) {
    // 그리는 중이라고 클램프를 버리면 한국 밖까지 줌아웃된 화면이 그대로 굳는다.
    // VWorld 위성·지적 타일은 한반도 범위에만 있어서 그 화면에는 타일이 한 장도
    // 오지 않는다(1:800만처럼 넓어지면 배경이 통째로 빈다). 끝난 뒤에 잡아 준다.
    runWhenCanvasIdle(canvas, kPropClampPending, kDeferredRetryMax, [](QgsMapCanvas* c) {
      if (LayerOps::clampCanvasToKorea(c))
        LayerOps::refreshCanvasIfIdle(c);
    });
    return false;
  }
  const QString auth = canvas->mapSettings().destinationCrs().isValid()
                           ? canvas->mapSettings().destinationCrs().authid()
                           : QStringLiteral("EPSG:5186");
  const QgsRectangle kr = satelliteFillExtentForCrs(auth);
  if (kr.isEmpty() || !kr.isFinite()) return false;

  const QgsRectangle fitted = extentFittedInside(kr, canvasViewAspect(canvas));
  const QgsRectangle cur = canvas->extent();
  if (cur.isEmpty() || !cur.isFinite()) {
    canvas->setExtent(fitted);
    return true;
  }

  const double eps = qMax(kr.width(), kr.height()) * 1e-9;
  // Bound zoom-out separately from navigation. Confining all four screen edges
  // left no horizontal pan range when a wide viewport reached Korea's width.
  // Keep the requested in-country center even at the largest allowed overview.
  const bool tooWide = cur.width() > kr.width() + eps || cur.height() > kr.height() + eps;
  // Preserve the aspect ratio QGIS is actually rendering (a hidden/resizing
  // widget can still have a different logical size). This makes a second clamp
  // a no-op instead of repeatedly asking QGIS to expand the same rectangle.
  const double factor = tooWide ? std::min(kr.width() / cur.width(), kr.height() / cur.height()) : 1.0;
  const double w = cur.width() * factor;
  const double h = cur.height() * factor;
  const double cx = std::clamp(cur.center().x(), kr.xMinimum(), kr.xMaximum());
  const double cy = std::clamp(cur.center().y(), kr.yMinimum(), kr.yMaximum());
  const QgsRectangle clamped(cx - w * 0.5, cy - h * 0.5, cx + w * 0.5, cy + h * 0.5);
  if (qAbs(clamped.xMinimum() - cur.xMinimum()) > eps ||
      qAbs(clamped.yMinimum() - cur.yMinimum()) > eps ||
      qAbs(clamped.xMaximum() - cur.xMaximum()) > eps ||
      qAbs(clamped.yMaximum() - cur.yMaximum()) > eps) {
    canvas->setExtent(clamped);
    return true;
  }
  return false;
}

void LayerOps::zoomToKorea(QgsMapCanvas* canvas, const QString& epsgAuthId, bool refresh) {
  if (!canvas) return;
  QgsRectangle ext = satelliteFillExtentForCrs(epsgAuthId);
  if (ext.isEmpty() || !ext.isFinite()) {
    ext = koreaExtentForCrs(epsgAuthId);
  }
  if (ext.isEmpty() || !ext.isFinite()) {
    ext = koreaExtentForCrs(QStringLiteral("EPSG:3857"));
    const QgsCoordinateReferenceSystem destCrs = canvas->mapSettings().destinationCrs();
    if (destCrs.isValid() && destCrs.authid() != QLatin1String("EPSG:3857")) {
      try {
        const QgsCoordinateTransform xf(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:3857")),
                                        destCrs, QgsCoordinateTransformContext());
        ext = xf.transformBoundingBox(ext);
      } catch (...) {
        KA_LOG_EXCEPT();
      }
    }
  }
  if (ext.isEmpty() || !ext.isFinite()) {
    ext = QgsRectangle(124.5, 33.0, 132.0, 39.5);
  }
  canvas->setExtent(extentFittedInside(ext, canvasViewAspect(canvas)));
  if (!canvas->renderFlag())
    canvas->setRenderFlag(true);
  if (!refresh) return;
  canvas->freeze(false);
  refreshCanvasIfIdle(canvas);
}

