#include "KigamPaleoService.h"
#include "LayerOps.h"

#include <QColor>
#include <QFileInfo>

#include <qgscategorizedsymbolrenderer.h>
#include <qgslinesymbol.h>
#include <qgsmapcanvas.h>
#include <qgsmarkersymbol.h>
#include <qgspallabeling.h>
#include <qgsproject.h>
#include <qgssinglesymbolrenderer.h>
#include <qgstextformat.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayerlabeling.h>

namespace {

constexpr const char* kPointTitle = "고지형 수문·연대(KIGAM)";
constexpr const char* kFaultTitle = "단층·선구조(KIGAM 1:5만)";
constexpr const char* kPointTag = "ka_hgis/kigam_paleo_points";
constexpr const char* kFaultTag = "ka_hgis/kigam_paleo_faults";

void removeTaggedLayer(QgsProject* project, const char* tag) {
  if (!project) return;
  for (QgsMapLayer* l : project->mapLayers()) {
    if (l && l->customProperty(QString::fromLatin1(tag)).toBool())
      project->removeMapLayer(l->id());
  }
}

}  // namespace

QString KigamPaleoService::reportPdfUrl(const QString& sheetIndex) {
  const QString clean = sheetIndex.trimmed();
  if (clean.isEmpty()) return {};
  return QStringLiteral(
             "https://data.kigam.re.kr/mgeo/common/MAP/report/geology_50k/file_down.do?savFile=%1.pdf")
      .arg(clean);
}

QString KigamPaleoService::originalMapPdfUrl(const QString& sheetIndex) {
  const QString clean = sheetIndex.trimmed();
  if (clean.isEmpty()) return {};
  return QStringLiteral(
             "https://data.kigam.re.kr/mgeo/common/MAP/original_map/geology_50k/file_down.do?savFile=%1_org_map.pdf")
      .arg(clean);
}

bool KigamPaleoService::applyKigamPaleoStyle(QgsVectorLayer* pointLayer,
                                             QgsVectorLayer* faultLayer) {
  if (pointLayer && pointLayer->isValid()) {
    QgsCategoryList cats;
    auto gwSym = QgsMarkerSymbol::createSimple({
        {QStringLiteral("name"), QStringLiteral("diamond")},
        {QStringLiteral("color"), QStringLiteral("#0284c7")},
        {QStringLiteral("outline_color"), QStringLiteral("#0f172a")},
        {QStringLiteral("outline_width"), QStringLiteral("0.4")},
        {QStringLiteral("size"), QStringLiteral("3.4")},
    });
    cats.append(QgsRendererCategory(QVariant(QStringLiteral("지하수위")), gwSym.release(),
                                    QStringLiteral("지하수위 관측정(KIGAM)")));
    auto oslSym = QgsMarkerSymbol::createSimple({
        {QStringLiteral("name"), QStringLiteral("triangle")},
        {QStringLiteral("color"), QStringLiteral("#dc2626")},
        {QStringLiteral("outline_color"), QStringLiteral("#0f172a")},
        {QStringLiteral("outline_width"), QStringLiteral("0.4")},
        {QStringLiteral("size"), QStringLiteral("3.8")},
    });
    cats.append(QgsRendererCategory(QVariant(QStringLiteral("단구연대(OSL)")), oslSym.release(),
                                    QStringLiteral("단구 OSL·IRSL 연대(KIGAM)")));
    pointLayer->setRenderer(new QgsCategorizedSymbolRenderer(QStringLiteral("kind"), cats));
    QgsPalLayerSettings pal;
    pal.fieldName = QStringLiteral("concat(\"name\", ' · ', \"summary\")");
    pal.isExpression = true;
    QgsTextFormat tf;
    tf.setSize(8.0);
    tf.setColor(QColor(15, 23, 42));
    tf.buffer().setEnabled(true);
    tf.buffer().setSize(0.8);
    tf.buffer().setColor(Qt::white);
    pal.setFormat(tf);
    pointLayer->setLabeling(new QgsVectorLayerSimpleLabeling(pal));
    pointLayer->setLabelsEnabled(true);
    pointLayer->triggerRepaint();
  }
  if (faultLayer && faultLayer->isValid()) {
    auto lineSym = QgsLineSymbol::createSimple({
        {QStringLiteral("color"), QStringLiteral("#b91c1c")},
        {QStringLiteral("width"), QStringLiteral("0.55")},
        {QStringLiteral("line_style"), QStringLiteral("dash")},
    });
    faultLayer->setRenderer(new QgsSingleSymbolRenderer(lineSym.release()));
    faultLayer->triggerRepaint();
  }
  return true;
}

QgsVectorLayer* KigamPaleoService::addPrepared(QgsProject* project, QgsMapCanvas* canvas,
                                               const PreparedReferenceMap& prepared,
                                               QString* errorOut) {
  if (!project || !prepared.isReady()) return nullptr;
  const QString gpkgPath = prepared.gpkgPath;
  removeTaggedLayer(project, kPointTag);
  removeTaggedLayer(project, kFaultTag);

  auto* flt = new QgsVectorLayer(QStringLiteral("%1|layername=kigam_paleo_faults").arg(gpkgPath),
                                 QString::fromUtf8(kFaultTitle), QStringLiteral("ogr"));
  if (flt->isValid() && flt->featureCount() > 0) {
    flt->setCustomProperty(QString::fromLatin1(kFaultTag), true);
    LayerOps::markReferenceLayer(flt);
    LayerOps::applyLegendCrsLabel(flt);
    applyKigamPaleoStyle(nullptr, flt);
    if (project->addMapLayer(flt, true))
      LayerOps::placeInLegendGroup(project, flt, QStringLiteral("참조 지도"));
    else
      delete flt;
  } else {
    delete flt;
  }

  const QString ptUri = QStringLiteral("%1|layername=%2").arg(gpkgPath, prepared.tableName);
  auto* pts = new QgsVectorLayer(ptUri, QString::fromUtf8(kPointTitle), QStringLiteral("ogr"));
  if (!pts->isValid()) {
    if (errorOut) *errorOut = pts->error().message();
    delete pts;
    return nullptr;
  }
  pts->setCustomProperty(QString::fromLatin1(kPointTag), true);
  LayerOps::markReferenceLayer(pts);
  LayerOps::applyLegendCrsLabel(pts);
  applyKigamPaleoStyle(pts, nullptr);
  if (!project->addMapLayer(pts, true)) {
    delete pts;
    return nullptr;
  }
  prepared.retainFiles();
  LayerOps::placeInLegendGroup(project, pts, QStringLiteral("참조 지도"));
  if (canvas) LayerOps::refreshCanvasIfIdle(canvas);
  return pts;
}
