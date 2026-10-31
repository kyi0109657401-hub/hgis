#include "KaAboveLabelsOverlay.h"

#include "core/LayerOps.h"

#include <QObject>
#include <QPainter>
#include <QTimer>

#include <qgslayertreemapcanvasbridge.h>
#include <qgsmapcanvas.h>
#include <qgsmaplayer.h>
#include <qgsmaprenderersequentialjob.h>
#include <qgsmapsettings.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

namespace {

// 레이어 창 연결이 덧그림 레이어를 다시 넣은 렌더가 시작되기 전에 뺀다. 그리는 중이면 목록을 건드리지 않고
// 끝난 뒤에 뺀다(타일을 받는 동안 다시 그리기 금지 규칙). 연결이 넣은 목록에서 덧그림 레이어만 뺀다: 목록을
// 새로 만들면 저장 중 잠깐 끊긴 레이어가 빠진 채 남는다(LayerOps::syncMapCanvas 참고).
void keepOverlayLayersOff(const QPointer<QgsMapCanvas>& canvas) {
  if (!canvas) return;
  if (canvas->isDrawing()) {
    QTimer::singleShot(80, canvas.data(), [canvas] { keepOverlayLayersOff(canvas); });
    return;
  }
  QList<QgsMapLayer*> base = canvas->layers();
  for (QgsMapLayer* layer : LayerOps::layersDrawnAboveLabels(QgsProject::instance()))
    base.removeAll(layer);
  if (base != canvas->layers()) canvas->setLayers(base);
}

}  // namespace

KaAboveLabelsOverlay::KaAboveLabelsOverlay(QgsMapCanvas* canvas)
    : QgsMapCanvasItem(canvas),
      m_canvasContext(std::make_unique<QObject>()),
      m_layerContext(std::make_unique<QObject>()),
      m_jobContext(std::make_unique<QObject>()) {
  // 좌표격자(1000)보다 아래, 지도보다는 위. 격자와 눈금은 계속 맨 위에 남는다.
  setZValue(900);
  setVisible(false);
  // 지도가 다시 그려지면 대상 목록만 다시 읽는다(조회만 하는 함수). 그림은 목록이나
  // 레이어가 실제로 바뀌었을 때만 다시 만든다.
  if (canvas)
    QObject::connect(canvas, &QgsMapCanvas::renderComplete, m_canvasContext.get(), [this](QPainter*) {
      // 본 지도가 방금 다 그린 레이어 목록. 그림이 바뀌는 이때에 맞춰 이름을 누가 쓸지도 바꾼다(startRender).
      QStringList drawn;
      for (const QgsMapLayer* layer : mMapCanvas->layers())
        drawn << layer->id();
      if (drawn != m_canvasDrawnIds) {
        m_canvasDrawnIds = drawn;
        markDirty();
      }
      syncWithProject();
    });
}

KaAboveLabelsOverlay::~KaAboveLabelsOverlay() {
  // 그리는 중인 레이어가 이 항목보다 먼저 사라지지 않게 여기서 렌더를 끝낸다.
  if (m_job) {
    QObject::disconnect(m_job.get(), nullptr, m_jobContext.get(), nullptr);
    m_job->cancel();
  }
}

QgsLayerTreeMapCanvasBridge* KaAboveLabelsOverlay::makeLayerTreeBridge(QgsLayerTree* root, QgsMapCanvas* canvas,
                                                                       QObject* parent) {
  auto* bridge = new QgsLayerTreeMapCanvasBridge(root, canvas, parent);
  bridge->setAutoSetupOnFirstLayer(false);
  QObject::connect(bridge, &QgsLayerTreeMapCanvasBridge::canvasLayersChanged, canvas,
                   [target = QPointer<QgsMapCanvas>(canvas)] { keepOverlayLayersOff(target); });
  return bridge;
}

void KaAboveLabelsOverlay::syncWithProject() {
  const QList<QgsMapLayer*> above = LayerOps::layersDrawnAboveLabels(QgsProject::instance());
  bool same = above.size() == m_layers.size();
  for (int i = 0; same && i < above.size(); ++i)
    same = m_layers.at(i).data() == above.at(i);
  if (same) {
    if (!above.isEmpty() && !isVisible()) setVisible(true);
    return;
  }
  // Layers handed in directly (not in the project) are the caller's to replace.
  bool fromProject = false;
  for (const QPointer<QgsMapLayer>& layer : std::as_const(m_layers))
    fromProject = fromProject || (layer && QgsProject::instance()->mapLayer(layer->id()) == layer.data());
  if (above.isEmpty() && !fromProject && !m_layers.isEmpty()) return;
  setLayers(above);
}

QList<QgsMapLayer*> KaAboveLabelsOverlay::currentLayers() const {
  QList<QgsMapLayer*> chosen;
  for (const QPointer<QgsMapLayer>& layer : m_layers) {
    if (layer) chosen.append(layer.data());
  }
  return chosen;
}

void KaAboveLabelsOverlay::watchLayers() {
  // A fresh context drops the previous layer connections at once.
  m_layerContext = std::make_unique<QObject>();
  QObject* context = m_layerContext.get();
  const auto dirty = [this] { markDirty(); };
  for (const QPointer<QgsMapLayer>& layer : std::as_const(m_layers)) {
    if (!layer) continue;
    QgsMapLayer* watched = layer.data();
    QObject::connect(watched, &QgsMapLayer::repaintRequested, context, dirty);
    QObject::connect(watched, &QgsMapLayer::rendererChanged, context, dirty);
    QObject::connect(watched, &QgsMapLayer::styleChanged, context, dirty);
    QObject::connect(watched, &QgsMapLayer::dataChanged, context, dirty);
    QObject::connect(watched, &QgsMapLayer::opacityChanged, context, dirty);
    QObject::connect(watched, &QgsMapLayer::blendModeChanged, context, dirty);
    QObject::connect(watched, &QgsMapLayer::crsChanged, context, dirty);
    QObject::connect(watched, &QObject::destroyed, context, dirty);
    if (auto* vector = qobject_cast<QgsVectorLayer*>(watched)) {
      // Edit buffer changes, undo/redo, commit, roll back, and feature selection changes.
      QObject::connect(vector, &QgsVectorLayer::layerModified, context, dirty);
      QObject::connect(vector, &QgsVectorLayer::afterRollBack, context, dirty);
      QObject::connect(vector, &QgsVectorLayer::afterCommitChanges, context, dirty);
      QObject::connect(vector, &QgsVectorLayer::selectionChanged, context, dirty);
    }
  }
}

void KaAboveLabelsOverlay::markDirty() {
  if (m_dirty) return;
  m_dirty = true;
  update();
}

void KaAboveLabelsOverlay::setLayers(const QList<QgsMapLayer*>& layers) {
  QList<QgsMapLayer*> wanted;
  for (QgsMapLayer* l : layers) {
    if (l) wanted.append(l);
  }
  if (wanted == currentLayers() && wanted.size() == m_layers.size()) {
    // Same list (the label stack is re-applied often): keep the picture.
    setVisible(!m_layers.isEmpty());
    return;
  }
  m_layers.clear();
  for (QgsMapLayer* l : layers) {
    if (l) m_layers.append(QPointer<QgsMapLayer>(l));
  }
  watchLayers();
  setVisible(!m_layers.isEmpty());
  updatePosition();
  m_dirty = true;
  update();
}

void KaAboveLabelsOverlay::updatePosition() {
  if (!mMapCanvas) return;
  setRect(mMapCanvas->extent());
}

QString KaAboveLabelsOverlay::cacheKey() const {
  const QgsMapSettings& ms = mMapCanvas->mapSettings();
  const QgsRectangle ext = mMapCanvas->extent();
  return QStringLiteral("%1|%2|%3|%4|%5|%6")
      .arg(ext.toString(9), QString::number(mMapCanvas->width()), QString::number(mMapCanvas->height()),
           QString::number(ms.devicePixelRatio(), 'f', 3), ms.destinationCrs().authid(),
           QString::number(ms.rotation(), 'f', 4));
}

void KaAboveLabelsOverlay::paint(QPainter* painter) {
  if (!painter || !mMapCanvas || m_layers.isEmpty()) return;
  if (currentLayers().isEmpty()) return;

  const QSize logical = mMapCanvas->size();
  if (logical.width() < 2 || logical.height() < 2) return;
  // The base map image uses the canvas map settings' DPR; the overlay matches it.
  float dpr = mMapCanvas->mapSettings().devicePixelRatio();
  if (!(dpr > 0.05f)) dpr = static_cast<float>(mMapCanvas->devicePixelRatioF());
  if (!(dpr > 0.05f)) dpr = 1.0f;

  // 입력이 그대로면 담아 둔 그림을 쓴다. 바뀌었으면 뒤에서 새로 그리고 그동안은 지난 그림을 쓴다.
  // 여기(주 스레드)서 그리면 유적이 많은 시·군(경주 꼭짓점 약 275만)에서 확대·이동마다 창이 몇 초씩 멎는다.
  const QString key = cacheKey();
  const bool current = m_cacheKey == key && m_cacheSize == logical;
  if (m_dirty || (!current && m_jobKey != key)) {
    m_dirty = false;
    startRender(dpr, logical, key);
  }
  if (m_cache.isNull()) return;
  if (current) {
    painter->drawImage(QPointF(0, 0), m_cache);
    return;
  }
  // 지난 그림을 지금 화면 위치에 맞춰 늘이거나 옮겨 보인다(본 지도의 미리보기와 같은 방식).
  // 좌표계나 회전이 달라졌으면 맞출 수 없으니 새 그림이 올 때까지 비워 둔다.
  const QgsMapSettings& ms = mMapCanvas->mapSettings();
  if (m_cacheFrame != frameOf(ms)) return;
  const QPointF topLeft =
      toCanvasCoordinates(QgsPointXY(m_cacheExtent.xMinimum(), m_cacheExtent.yMaximum())) - pos();
  const QPointF bottomRight =
      toCanvasCoordinates(QgsPointXY(m_cacheExtent.xMaximum(), m_cacheExtent.yMinimum())) - pos();
  painter->drawImage(QRectF(topLeft, bottomRight), m_cache);
}

QString KaAboveLabelsOverlay::frameOf(const QgsMapSettings& ms) {
  return ms.destinationCrs().authid() + QLatin1Char('|') + QString::number(ms.rotation(), 'f', 4);
}

void KaAboveLabelsOverlay::dropJob() {
  if (!m_job) return;
  // 늦게 끝나는 지난 렌더는 결과를 버린다. 끝날 때 스스로 지워진다.
  QgsMapRendererSequentialJob* old = m_job.release();
  QObject::disconnect(old, nullptr, m_jobContext.get(), nullptr);
  QObject::connect(old, &QgsMapRendererJob::finished, old, &QObject::deleteLater);
  old->cancelWithoutBlocking();
  m_jobKey.clear();
}

void KaAboveLabelsOverlay::startRender(qreal dpr, const QSize& logical, const QString& key) {
  dropJob();
  if (!mMapCanvas || m_layers.isEmpty()) return;
  const QList<QgsMapLayer*> live = currentLayers();
  if (live.isEmpty()) return;
  ++m_rebuilds;

  QgsMapSettings ms = mMapCanvas->mapSettings();
  ms.setLayers(live);
  ms.setBackgroundColor(Qt::transparent);
  // 도형은 여기서 한 번만 그린다. 지번은 본 화면에 이미 있다.
  ms.setFlag(Qgis::MapSettingsFlag::DrawLabeling, false);
  ms.setOutputSize(logical);
  ms.setDevicePixelRatio(static_cast<float>(dpr));

  // 레이어 창 연결(bridge)이 이 레이어를 본 지도에 다시 넣었으면 본 지도가 이미 이름을 쓴다. 여기서 또 쓰면
  // 두 엔진이 다른 자리를 골라 「오외도지석묘1호호」처럼 겹친다(R83). 이름은 한 엔진에만 맡긴다.
  QList<QgsMapLayer*> labeled;
  for (QgsMapLayer* layer : live) {
    auto* vector = qobject_cast<QgsVectorLayer*>(layer);
    if (vector && vector->labelsEnabled() && vector->labeling() && !m_canvasDrawnIds.contains(layer->id()))
      labeled.append(vector);
  }
  m_labelSettings.reset();
  if (!labeled.isEmpty()) {
    QgsMapSettings labelSettings = ms;
    labelSettings.setLayers(labeled);
    labelSettings.setFlag(Qgis::MapSettingsFlag::DrawLabeling, true);
    labelSettings.setFlag(Qgis::MapSettingsFlag::SkipSymbolRendering, true);
    m_labelSettings = std::make_unique<QgsMapSettings>(labelSettings);
  }
  m_jobKey = key;
  m_jobSize = logical;
  m_jobExtent = mMapCanvas->extent();
  m_jobFrame = frameOf(ms);
  m_shapes = QImage();
  // The vector-only pass stays sequential like the base map (WMS crash class rule): one job, one thread.
  runJob(ms);
}

void KaAboveLabelsOverlay::runJob(const QgsMapSettings& settings) {
  m_job = std::make_unique<QgsMapRendererSequentialJob>(settings);
  QObject::connect(m_job.get(), &QgsMapRendererJob::finished, m_jobContext.get(), [this] { finishJob(); });
  m_job->start();
}

void KaAboveLabelsOverlay::finishJob() {
  if (!m_job) return;
  const QImage image = m_job->renderedImage();
  m_job.release()->deleteLater();
  if (m_shapes.isNull() && m_labelSettings) {
    // 도형 다음에 이름만 얹는다(심볼 없이). 두 번째 렌더도 뒤에서 돈다.
    m_shapes = image;
    const QgsMapSettings labels = *m_labelSettings;
    m_labelSettings.reset();
    runJob(labels);
    return;
  }
  QImage picture = image;
  if (!m_shapes.isNull()) {
    picture = m_shapes;
    QPainter p(&picture);
    p.drawImage(QPointF(0, 0), image);
    p.end();
    m_shapes = QImage();
  }
  m_cache = picture;
  m_cacheKey = m_jobKey;
  m_cacheSize = m_jobSize;
  m_cacheExtent = m_jobExtent;
  m_cacheFrame = m_jobFrame;
  m_jobKey.clear();
  update();
}
