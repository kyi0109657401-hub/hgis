#include "PaleoAnalysisRunner.h"
#include "DemPresentation.h"
#include "KaLogExcept.h"
#include "LayerOps.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <algorithm>
#include <array>
#include <cmath>

#include <cpl_conv.h>
#include <gdal_priv.h>
#include <gdal_utils.h>
#include <qgscoordinatereferencesystem.h>
#include <qgslayoutitemlabel.h>
#include <qgslayoutmanager.h>
#include <qgslayoutpoint.h>
#include <qgslayoutsize.h>
#include <qgsmapcanvas.h>
#include <qgsprintlayout.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgstextformat.h>

namespace {

struct Stop {
  double z;
  uint8_t r, g, b;
};
constexpr std::array<Stop, 10> kHypsoStops = {{{0.0, 114, 198, 181},
                                               {25.0, 145, 211, 150},
                                               {50.0, 165, 214, 128},
                                               {100.0, 208, 220, 120},
                                               {200.0, 235, 210, 122},
                                               {400.0, 232, 166, 102},
                                               {600.0, 217, 139, 90},
                                               {900.0, 185, 108, 82},
                                               {1300.0, 182, 152, 134},
                                               {2000.0, 244, 243, 239}}};

void hypsoColor(double z, uint8_t& r, uint8_t& g, uint8_t& b) noexcept {
  if (z <= kHypsoStops.front().z) {
    r = kHypsoStops.front().r;
    g = kHypsoStops.front().g;
    b = kHypsoStops.front().b;
    return;
  }
  for (size_t i = 1; i < kHypsoStops.size(); ++i) {
    if (z <= kHypsoStops[i].z) {
      const double t =
          (z - kHypsoStops[i - 1].z) / std::max(1.0, kHypsoStops[i].z - kHypsoStops[i - 1].z);
      r = static_cast<uint8_t>(
          std::lround(kHypsoStops[i - 1].r + t * (kHypsoStops[i].r - kHypsoStops[i - 1].r)));
      g = static_cast<uint8_t>(
          std::lround(kHypsoStops[i - 1].g + t * (kHypsoStops[i].g - kHypsoStops[i - 1].g)));
      b = static_cast<uint8_t>(
          std::lround(kHypsoStops[i - 1].b + t * (kHypsoStops[i].b - kHypsoStops[i - 1].b)));
      return;
    }
  }
  r = kHypsoStops.back().r;
  g = kHypsoStops.back().g;
  b = kHypsoStops.back().b;
}

QgsRasterLayer* addOrReplaceNamedRaster(QgsProject* project, const QString& path,
                                        const QString& title) {
  if (!project || !QFileInfo::exists(path)) return nullptr;
  for (QgsMapLayer* l : project->mapLayersByName(title)) {
    if (l) project->removeMapLayer(l->id());
  }
  auto* rl = new QgsRasterLayer(path, title, QStringLiteral("gdal"));
  if (!rl->isValid()) {
    delete rl;
    return nullptr;
  }
  LayerOps::markReferenceLayer(rl);
  LayerOps::applyLegendCrsLabel(rl);
  if (!project->addMapLayer(rl, true)) {
    delete rl;
    return nullptr;
  }
  LayerOps::placeInLegendGroup(project, rl, QStringLiteral("참조 지도"));
  return rl;
}

}  // namespace

bool PaleoAnalysisRunner::writeRgbaGeoTiff(const QString& outPath,
                                           const hgis::paleo::GridTransform& tf,
                                           const QString& crsWkt,
                                           const std::vector<uint8_t>& rgba, QString* errorOut) {
  if (tf.width <= 0 || tf.height <= 0 ||
      rgba.size() < static_cast<size_t>(tf.width) * tf.height * 4) {
    if (errorOut) *errorOut = QStringLiteral("RGBA 격자 크기가 올바르지 않습니다.");
    return false;
  }
  GDALAllRegister();
  CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "YES");
  QDir().mkpath(QFileInfo(outPath).absolutePath());
  GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("GTiff");
  if (!drv) return false;
  const char* opts[] = {"COMPRESS=DEFLATE", "TILED=YES", nullptr};
  GDALDataset* ds = drv->Create(outPath.toUtf8().constData(), tf.width, tf.height, 4, GDT_Byte,
                                const_cast<char**>(opts));
  if (!ds) return false;
  double gt[6] = {tf.origin_x, tf.cell_size, 0.0, tf.origin_y, 0.0, -tf.cell_size};
  ds->SetGeoTransform(gt);
  if (!crsWkt.isEmpty()) ds->SetProjection(crsWkt.toUtf8().constData());

  const size_t n = static_cast<size_t>(tf.width) * tf.height;
  std::vector<uint8_t> bandBuf(n);
  const GDALColorInterp interps[4] = {GCI_RedBand, GCI_GreenBand, GCI_BlueBand, GCI_AlphaBand};
  for (int b = 0; b < 4; ++b) {
    for (size_t i = 0; i < n; ++i) bandBuf[i] = rgba[i * 4 + b];
    GDALRasterBand* band = ds->GetRasterBand(b + 1);
    band->SetColorInterpretation(interps[b]);
    band->RasterIO(GF_Write, 0, 0, tf.width, tf.height, bandBuf.data(), tf.width, tf.height,
                   GDT_Byte, 0, 0);
  }
  GDALClose(ds);
  return true;
}

bool PaleoAnalysisRunner::writeFloatGeoTiff(const QString& outPath,
                                            const hgis::paleo::GridTransform& tf,
                                            const QString& crsWkt,
                                            const std::vector<double>& values, QString* errorOut) {
  if (tf.width <= 0 || tf.height <= 0 ||
      values.size() < static_cast<size_t>(tf.width) * tf.height) {
    if (errorOut) *errorOut = QStringLiteral("실수 격자 크기가 올바르지 않습니다.");
    return false;
  }
  GDALAllRegister();
  CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "YES");
  QDir().mkpath(QFileInfo(outPath).absolutePath());
  GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("GTiff");
  if (!drv) return false;
  const char* opts[] = {"COMPRESS=DEFLATE", "TILED=YES", nullptr};
  GDALDataset* ds = drv->Create(outPath.toUtf8().constData(), tf.width, tf.height, 1, GDT_Float32,
                                const_cast<char**>(opts));
  if (!ds) return false;
  double gt[6] = {tf.origin_x, tf.cell_size, 0.0, tf.origin_y, 0.0, -tf.cell_size};
  ds->SetGeoTransform(gt);
  if (!crsWkt.isEmpty()) ds->SetProjection(crsWkt.toUtf8().constData());

  const size_t n = static_cast<size_t>(tf.width) * tf.height;
  std::vector<float> buf(n);
  for (size_t i = 0; i < n; ++i) buf[i] = static_cast<float>(values[i]);
  GDALRasterBand* band = ds->GetRasterBand(1);
  band->SetNoDataValue(-9999.0);
  band->RasterIO(GF_Write, 0, 0, tf.width, tf.height, buf.data(), tf.width, tf.height, GDT_Float32,
                 0, 0);
  GDALClose(ds);
  return true;
}

bool PaleoAnalysisRunner::registerDemWideLayers(QgsProject* project, QgsMapCanvas* canvas,
                                                const QString& sourceDemPath,
                                                const QString& surveyPath, QString* errorOut) {
  if (!project || !QFileInfo::exists(sourceDemPath)) return false;
  const QString outDir =
      surveyPath.isEmpty()
          ? QFileInfo(sourceDemPath).absolutePath()
          : QFileInfo(surveyPath).absoluteDir().filePath(QStringLiteral("지형분석"));
  QDir().mkpath(outDir);
  const QString demOut = QDir(outDir).filePath(QStringLiteral("국토지리원_DEM.tif"));
  const QString wideOut = QDir(outDir).filePath(QStringLiteral("광역지형도_채색음영.tif"));
  const QString slopeOut = QDir(outDir).filePath(QStringLiteral("경사도.tif"));

  const QgsCoordinateReferenceSystem workCrs =
      project->crs().isValid() ? project->crs()
                               : QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187"));
  GDALAllRegister();
  CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "YES");

  const bool sameDemPath =
      QFileInfo::exists(demOut) &&
      QFileInfo(sourceDemPath).canonicalFilePath() == QFileInfo(demOut).canonicalFilePath();
  for (const QString& title :
       {QStringLiteral("[DEM] 광역지형도(채색+음영기복)"), QStringLiteral("[DEM] 경사도")}) {
    for (QgsMapLayer* l : project->mapLayersByName(title)) {
      if (l) project->removeMapLayer(l->id());
    }
  }
  if (!sameDemPath) {
    for (QgsMapLayer* l : project->mapLayersByName(QStringLiteral("[DEM] 국토지리원_DEM.tif"))) {
      if (l) project->removeMapLayer(l->id());
    }
    QFile::remove(demOut);
    const QByteArray dstWkt = workCrs.toWkt().toUtf8();
    const char* warpArgs[] = {"-t_srs", dstWkt.constData(), "-r", "bilinear",
                              "-overwrite", nullptr};
    GDALWarpAppOptions* wOpts = GDALWarpAppOptionsNew(const_cast<char**>(warpArgs), nullptr);
    GDALDatasetH srcDs = GDALOpen(sourceDemPath.toUtf8().constData(), GA_ReadOnly);
    if (srcDs) {
      int err = 0;
      GDALDatasetH warped =
          GDALWarp(demOut.toUtf8().constData(), nullptr, 1, &srcDs, wOpts, &err);
      if (warped) GDALClose(warped);
      GDALClose(srcDs);
    }
    GDALWarpAppOptionsFree(wOpts);
  }
  QFile::remove(wideOut);
  QFile::remove(slopeOut);
  const QString activeDem = QFileInfo::exists(demOut) ? demOut : sourceDemPath;
  GDALDataset* ds = static_cast<GDALDataset*>(GDALOpen(activeDem.toUtf8().constData(), GA_ReadOnly));
  if (!ds) return false;
  const int w = std::clamp(ds->GetRasterXSize(), 4, 1024);
  const int h = std::clamp(ds->GetRasterYSize(), 4, 1024);
  double gt[6] = {0, 5, 0, 0, 0, -5};
  ds->GetGeoTransform(gt);
  const double scaleX = static_cast<double>(ds->GetRasterXSize()) / w;
  const double scaleY = static_cast<double>(ds->GetRasterYSize()) / h;
  hgis::paleo::GridTransform tf{gt[0], gt[3], std::max(1.0, std::abs(gt[1]) * scaleX), w, h};
  const QString wkt = QString::fromUtf8(ds->GetProjectionRef() ? ds->GetProjectionRef() : "");
  std::vector<float> rawZ(static_cast<size_t>(w) * h, 0.0f);
  ds->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, ds->GetRasterXSize(), ds->GetRasterYSize(),
                                 rawZ.data(), w, h, GDT_Float32, 0, 0);
  GDALClose(ds);

  std::vector<double> z(rawZ.size());
  double validZ = 20.0;
  for (size_t i = 0; i < rawZ.size(); ++i) {
    if (std::isfinite(rawZ[i]) && rawZ[i] > -500.0f) validZ = rawZ[i];
    z[i] = validZ;
  }
  Q_UNUSED(scaleY);
  std::vector<double> slopeDeg, hillshade, twi;
  hgis::paleo::HgisPaleoEngine::compute_terrain_derivatives(tf, z, slopeDeg, hillshade, twi);
  std::vector<uint8_t> wideRgba(z.size() * 4, 255);
  for (size_t i = 0; i < z.size(); ++i) {
    uint8_t r = 0, g = 0, b = 0;
    hypsoColor(z[i], r, g, b);
    const double shade = std::clamp(hillshade[i] / 255.0, 0.25, 1.0);
    const double slpDark = std::clamp(1.0 - (slopeDeg[i] / 90.0) * 0.35, 0.55, 1.0);
    const double factor = shade * slpDark;
    wideRgba[i * 4 + 0] = static_cast<uint8_t>(std::clamp(std::lround(r * factor), 0L, 255L));
    wideRgba[i * 4 + 1] = static_cast<uint8_t>(std::clamp(std::lround(g * factor), 0L, 255L));
    wideRgba[i * 4 + 2] = static_cast<uint8_t>(std::clamp(std::lround(b * factor), 0L, 255L));
    wideRgba[i * 4 + 3] = 230;
  }
  writeRgbaGeoTiff(wideOut, tf, wkt, wideRgba, errorOut);
  writeFloatGeoTiff(slopeOut, tf, wkt, slopeDeg, errorOut);

  if (auto* demLayer = addOrReplaceNamedRaster(
          project, activeDem, QStringLiteral("[DEM] 국토지리원_DEM.tif"))) {
    DemPresentation::apply(demLayer, QStringLiteral("viewport"), demLayer->extent());
  }
  addOrReplaceNamedRaster(project, slopeOut, QStringLiteral("[DEM] 경사도"));
  addOrReplaceNamedRaster(project, wideOut, QStringLiteral("[DEM] 광역지형도(채색+음영기복)"));
  if (canvas) LayerOps::refreshCanvasIfIdle(canvas);
  return true;
}

bool PaleoAnalysisRunner::applyEvidenceToSheet(QgsProject* project,
                                               const hgis::paleo::PaleoAnalysisResult& res) {
  if (!project || !project->layoutManager()) return false;
  auto* ly = dynamic_cast<QgsPrintLayout*>(
      project->layoutManager()->layoutByName(QStringLiteral("user_sheet")));
  if (!ly) return false;

  auto* label =
      dynamic_cast<QgsLayoutItemLabel*>(ly->itemById(QStringLiteral("ka_paleo_evidence_box")));
  if (!label) {
    label = new QgsLayoutItemLabel(ly);
    label->setId(QStringLiteral("ka_paleo_evidence_box"));
    ly->addLayoutItem(label);
    label->attemptMove(QgsLayoutPoint(12.0, 210.0, Qgis::LayoutUnit::Millimeters));
    label->attemptResize(QgsLayoutSize(185.0, 48.0, Qgis::LayoutUnit::Millimeters));
  }
  label->setText(QString::fromUtf8(res.aoi_summary_text.c_str()));
  label->setBackgroundEnabled(true);
  label->setBackgroundColor(QColor(255, 255, 255, 235));
  label->setFrameEnabled(true);
  QgsTextFormat tf;
  tf.setFont(QFont(QStringLiteral("Malgun Gothic"), 7));
  tf.setSize(7.2);
  tf.setColor(QColor(20, 24, 31));
  label->setTextFormat(tf);
  label->setMarginX(2.0);
  label->setMarginY(2.0);
  return true;
}
