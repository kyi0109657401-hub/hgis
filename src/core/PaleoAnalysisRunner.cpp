#include "PaleoAnalysisRunner.h"
#include "BasemapDsm.h"
#include "DemAnalyzer.h"
#include "DemDownloadService.h"
#include "DemPresentation.h"
#include "GeoInfoBoreholeService.h"
#include "GeologyMapService.h"
#include "KaLogExcept.h"
#include "KigamPaleoService.h"
#include "LayerOps.h"
#include "PaleoLandformService.h"
#include "RiverMapService.h"
#include "SoilMapService.h"
#include "VworldSettings.h"

#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <algorithm>
#include <cmath>

#include <qgscategorizedsymbolrenderer.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransform.h>
#include <qgsexception.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgsfeedback.h>
#include <qgsfield.h>
#include <qgsfields.h>
#include <qgsgeometry.h>
#include <qgsmapcanvas.h>
#include <qgsmarkersymbol.h>
#include <qgspallabeling.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgstextformat.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayerlabeling.h>

namespace {

QgsRectangle toEpsg5186(const QgsRectangle& workBox, const QgsCoordinateReferenceSystem& workCrs,
                        QgsProject* project) {
  const QgsCoordinateReferenceSystem crs5186(QStringLiteral("EPSG:5186"));
  if (!workCrs.isValid() || workCrs == crs5186) return workBox;
  try {
    const QgsCoordinateTransform tr(workCrs, crs5186, project->transformContext());
    return tr.transformBoundingBox(workBox);
  } catch (...) {
    KA_LOG_EXCEPT();
    return workBox;
  }
}

void addMarkerVectorLayer(QgsProject* project, const QString& gpkgPath,
                          const std::vector<hgis::paleo::ParsedBoreholeSummary>& boreholes,
                          const QgsCoordinateReferenceSystem& workCrs) {
  if (!project || boreholes.empty()) return;
  const QString title = QStringLiteral("[고지형] 시추주상도 마커");
  for (QgsMapLayer* l : project->mapLayersByName(title)) {
    if (l) project->removeMapLayer(l->id());
  }
  const QString crsAuth = workCrs.isValid() ? workCrs.authid() : QStringLiteral("EPSG:5187");
  QgsVectorLayer mem(QStringLiteral("Point?crs=%1").arg(crsAuth), title, QStringLiteral("memory"));
  QgsFields fields;
  fields.append(QgsField(QStringLiteral("hole_id"), QMetaType::Type::QString));
  fields.append(QgsField(QStringLiteral("sed_type"), QMetaType::Type::QString));
  fields.append(QgsField(QStringLiteral("map_label"), QMetaType::Type::QString));
  mem.dataProvider()->addAttributes(fields.toList());
  mem.updateFields();

  const QgsCoordinateReferenceSystem crs5186(QStringLiteral("EPSG:5186"));
  const QgsCoordinateTransform toWork(crs5186, workCrs, project->transformContext());
  const bool needTr = workCrs.isValid() && workCrs != crs5186;
  QgsFeatureList feats;
  for (const auto& bh : boreholes) {
    QgsPointXY pt(bh.x, bh.y);
    if (needTr) {
      try {
        pt = toWork.transform(pt);
      } catch (...) {
        KA_LOG_EXCEPT();
      }
    }
    QgsFeature f(mem.fields());
    f.setGeometry(QgsGeometry::fromPointXY(pt));
    f.setAttribute(QStringLiteral("hole_id"), QString::fromUtf8(bh.hole_id.c_str()));
    f.setAttribute(QStringLiteral("sed_type"), QString::fromUtf8(bh.sed_type.c_str()));
    f.setAttribute(QStringLiteral("map_label"), QString::fromUtf8(bh.map_label.c_str()));
    feats.append(f);
  }
  mem.dataProvider()->addFeatures(feats);
  QgsVectorFileWriter::SaveVectorOptions opts;
  opts.driverName = QStringLiteral("GPKG");
  opts.layerName = QStringLiteral("paleo_boreholes");
  opts.fileEncoding = QStringLiteral("UTF-8");
  QString err, fn, ln;
  if (QgsVectorFileWriter::writeAsVectorFormatV3(&mem, gpkgPath, project->transformContext(), opts,
                                                 &err, &fn, &ln) != QgsVectorFileWriter::NoError)
    return;
  auto* vl = new QgsVectorLayer(QStringLiteral("%1|layername=paleo_boreholes").arg(gpkgPath), title,
                                QStringLiteral("ogr"));
  if (!vl->isValid()) {
    delete vl;
    return;
  }
  QgsCategoryList cats;
  auto coarseSym = QgsMarkerSymbol::createSimple({{QStringLiteral("name"), QStringLiteral("circle")},
                                                  {QStringLiteral("color"), QStringLiteral("#FF9800")},
                                                  {QStringLiteral("outline_color"), QStringLiteral("#1f2937")},
                                                  {QStringLiteral("size"), QStringLiteral("3.6")}});
  cats.append(QgsRendererCategory(QVariant(QStringLiteral("Coarse")), coarseSym.release(),
                                  QStringLiteral("사질·자갈 우세 (자연제방·선상지형)")));
  auto fineSym = QgsMarkerSymbol::createSimple({{QStringLiteral("name"), QStringLiteral("circle")},
                                                {QStringLiteral("color"), QStringLiteral("#00ACC1")},
                                                {QStringLiteral("outline_color"), QStringLiteral("#1f2937")},
                                                {QStringLiteral("size"), QStringLiteral("3.6")}});
  cats.append(QgsRendererCategory(QVariant(QStringLiteral("Fine")), fineSym.release(),
                                  QStringLiteral("점토·실트 우세 (구하도·배후습지형)")));
  vl->setRenderer(new QgsCategorizedSymbolRenderer(QStringLiteral("sed_type"), cats));
  QgsPalLayerSettings pal;
  pal.fieldName = QStringLiteral("map_label");
  QgsTextFormat tf;
  tf.setSize(8.0);
  tf.buffer().setEnabled(true);
  tf.buffer().setSize(0.8);
  tf.buffer().setColor(Qt::white);
  pal.setFormat(tf);
  vl->setLabeling(new QgsVectorLayerSimpleLabeling(pal));
  vl->setLabelsEnabled(true);
  LayerOps::markReferenceLayer(vl);
  LayerOps::applyLegendCrsLabel(vl);
  if (project->addMapLayer(vl, true))
    LayerOps::placeInLegendGroup(project, vl, QStringLiteral("참조 지도"));
  else
    delete vl;
}

}  // namespace

void PaleoAnalysisRunner::registerBoreholeMarkers(
    QgsProject* project, const QString& gpkgPath,
    const std::vector<hgis::paleo::ParsedBoreholeSummary>& boreholes) {
  if (!project) return;
  const QgsCoordinateReferenceSystem workCrs =
      project->crs().isValid() ? project->crs()
                               : QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187"));
  addMarkerVectorLayer(project, gpkgPath, boreholes, workCrs);
}

QgsRectangle PaleoAnalysisRunner::surveyExtentRadius(QgsProject* project, QgsMapCanvas* canvas,
                                                     double radiusM, QgsGeometry* outAoiWorkCrs,
                                                     QgsPointXY* outCenterWorkCrs) {
  const QgsCoordinateReferenceSystem workCrs =
      (project && project->crs().isValid())
          ? project->crs()
          : QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187"));
  QVector<QgsGeometry> parts;
  if (project) {
    for (QgsVectorLayer* vl :
         LayerOps::findAllByLayerKey(project, QStringLiteral("survey_area"))) {
      if (!vl || !vl->isValid()) continue;
      const QgsCoordinateTransform tr(vl->crs(), workCrs, project->transformContext());
      const bool needTr = vl->crs().isValid() && vl->crs() != workCrs;
      QgsFeature f;
      QgsFeatureIterator it = vl->getFeatures(QgsFeatureRequest().setNoAttributes());
      while (it.nextFeature(f)) {
        if (!f.hasGeometry() || f.geometry().isEmpty()) continue;
        QgsGeometry g = f.geometry();
        if (needTr) {
          try {
            g.transform(tr);
          } catch (...) {
            KA_LOG_EXCEPT();
          }
        }
        parts.append(g);
      }
    }
  }
  QgsGeometry aoi;
  QgsPointXY center(200000.0, 500000.0);
  QgsRectangle box;
  if (!parts.isEmpty()) {
    aoi = QgsGeometry::unaryUnion(parts);
    box = aoi.boundingBox();
    center = box.center();
  } else if (canvas && !canvas->extent().isEmpty() && canvas->extent().isFinite()) {
    center = canvas->extent().center();
    box = QgsRectangle(center.x() - 250.0, center.y() - 250.0, center.x() + 250.0,
                       center.y() + 250.0);
    aoi = QgsGeometry::fromRect(box);
  } else {
    box = QgsRectangle(199800.0, 499800.0, 200200.0, 500200.0);
    aoi = QgsGeometry::fromRect(box);
  }
  if (outAoiWorkCrs) *outAoiWorkCrs = aoi;
  if (outCenterWorkCrs) *outCenterWorkCrs = center;
  box.grow(radiusM);
  return box;
}
