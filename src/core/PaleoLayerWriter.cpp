#include "PaleoAnalysisRunner.h"
#include "DemPresentation.h"
#include "KaLogExcept.h"
#include "LayerOps.h"
#include "ReferenceKind.h"
#include "TopographicArchive.h"
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QRegularExpression>
#include <QTextStream>
#include <algorithm>
#include <array>
#include <cmath>
#include <cpl_conv.h>
#include <gdal_priv.h>
#include <gdal_utils.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsmapcanvas.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>

namespace {
struct Stop { double z; uint8_t r, g, b; };
constexpr std::array<Stop, 10> kHypsoStops = {{{0.0, 114, 198, 181}, {25.0, 145, 211, 150},
    {50.0, 165, 214, 128}, {100.0, 208, 220, 120}, {200.0, 235, 210, 122}, {400.0, 232, 166, 102},
    {600.0, 217, 139, 90}, {900.0, 185, 108, 82}, {1300.0, 182, 152, 134}, {2000.0, 244, 243, 239}}};

void hypsoColor(double z, uint8_t& r, uint8_t& g, uint8_t& b) noexcept {
  if (z <= kHypsoStops.front().z) { r = kHypsoStops.front().r; g = kHypsoStops.front().g; b = kHypsoStops.front().b; return; }
  for (size_t i = 1; i < kHypsoStops.size(); ++i) {
    if (z <= kHypsoStops[i].z) {
      const double t = (z - kHypsoStops[i - 1].z) / std::max(1.0, kHypsoStops[i].z - kHypsoStops[i - 1].z);
      r = static_cast<uint8_t>(std::lround(kHypsoStops[i - 1].r + t * (kHypsoStops[i].r - kHypsoStops[i - 1].r)));
      g = static_cast<uint8_t>(std::lround(kHypsoStops[i - 1].g + t * (kHypsoStops[i].g - kHypsoStops[i - 1].g)));
      b = static_cast<uint8_t>(std::lround(kHypsoStops[i - 1].b + t * (kHypsoStops[i].b - kHypsoStops[i - 1].b)));
      return;
    }
  }
  r = kHypsoStops.back().r; g = kHypsoStops.back().g; b = kHypsoStops.back().b;
}

QString safeOut(const QString& p) {
  if (!QFileInfo::exists(p) || QFile::remove(p)) return p;
  const QFileInfo fi(p);
  for (int i = 1; i <= 50; ++i) {
    const QString c = fi.dir().filePath(QStringLiteral("%1_%2.%3").arg(fi.completeBaseName()).arg(i).arg(fi.suffix()));
    if (!QFileInfo::exists(c) || QFile::remove(c)) return c;
  }
  return p;
}

QgsRasterLayer* addOrReplaceNamedRaster(QgsProject* project, const QString& path, const QString& title) {
  if (!project || !QFileInfo::exists(path)) return nullptr;
  for (QgsMapLayer* l : project->mapLayersByName(title)) if (l) project->removeMapLayer(l->id());
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  auto* rl = new QgsRasterLayer(path, title, QStringLiteral("gdal"));
  if (!rl->isValid()) { delete rl; return nullptr; }
  LayerOps::markReferenceLayer(rl); LayerOps::applyLegendCrsLabel(rl);
  if (!project->addMapLayer(rl, true)) { delete rl; return nullptr; }
  LayerOps::placeInLegendGroup(project, rl, QStringLiteral("참조 지도"));
  return rl;
}
}  // namespace

bool PaleoAnalysisRunner::writeRgbaGeoTiff(const QString& outPath, const hgis::paleo::GridTransform& tf,
                                           const QString& crsWkt, const std::vector<uint8_t>& rgba, QString* errorOut) {
  if (tf.width <= 0 || tf.height <= 0 || rgba.size() < static_cast<size_t>(tf.width) * tf.height * 4) {
    if (errorOut) *errorOut = QStringLiteral("RGBA 격자 크기가 올바르지 않습니다.");
    return false;
  }
  GDALAllRegister(); CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "YES");
  QDir().mkpath(QFileInfo(outPath).absolutePath());
  GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("GTiff");
  if (!drv) return false;
  const char* opts[] = {"COMPRESS=DEFLATE", "TILED=YES", nullptr};
  GDALDataset* ds = drv->Create(outPath.toUtf8().constData(), tf.width, tf.height, 4, GDT_Byte, const_cast<char**>(opts));
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
    band->RasterIO(GF_Write, 0, 0, tf.width, tf.height, bandBuf.data(), tf.width, tf.height, GDT_Byte, 0, 0);
  }
  GDALClose(ds);
  return true;
}

bool PaleoAnalysisRunner::writeFloatGeoTiff(const QString& outPath, const hgis::paleo::GridTransform& tf,
                                            const QString& crsWkt, const std::vector<double>& values, QString* errorOut) {
  if (tf.width <= 0 || tf.height <= 0 || values.size() < static_cast<size_t>(tf.width) * tf.height) {
    if (errorOut) *errorOut = QStringLiteral("실수 격자 크기가 올바르지 않습니다.");
    return false;
  }
  GDALAllRegister(); CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "YES");
  QDir().mkpath(QFileInfo(outPath).absolutePath());
  GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("GTiff");
  if (!drv) return false;
  const char* opts[] = {"COMPRESS=DEFLATE", "TILED=YES", nullptr};
  GDALDataset* ds = drv->Create(outPath.toUtf8().constData(), tf.width, tf.height, 1, GDT_Float32, const_cast<char**>(opts));
  if (!ds) return false;
  double gt[6] = {tf.origin_x, tf.cell_size, 0.0, tf.origin_y, 0.0, -tf.cell_size};
  ds->SetGeoTransform(gt);
  if (!crsWkt.isEmpty()) ds->SetProjection(crsWkt.toUtf8().constData());
  const size_t n = static_cast<size_t>(tf.width) * tf.height;
  std::vector<float> buf(n);
  for (size_t i = 0; i < n; ++i) buf[i] = static_cast<float>(values[i]);
  GDALRasterBand* band = ds->GetRasterBand(1);
  band->SetNoDataValue(-9999.0);
  band->RasterIO(GF_Write, 0, 0, tf.width, tf.height, buf.data(), tf.width, tf.height, GDT_Float32, 0, 0);
  GDALClose(ds);
  return true;
}

bool PaleoAnalysisRunner::registerDemWideLayers(QgsProject* project, QgsMapCanvas* canvas,
                                                const QString& sourceDemPath,
                                                const QString& surveyPath, QString* errorOut) {
  if (!project || !QFileInfo::exists(sourceDemPath)) return false;
  const QString outDir = surveyPath.isEmpty() ? QFileInfo(sourceDemPath).absolutePath()
                                              : QFileInfo(surveyPath).absoluteDir().filePath(QStringLiteral("지형분석"));
  QDir().mkpath(outDir);
  const QString demOut = QDir(outDir).filePath(QStringLiteral("국토지리원_DEM.tif"));
  const QString wideOut = QDir(outDir).filePath(QStringLiteral("광역지형도_채색음영.tif"));
  const QString slopeOut = QDir(outDir).filePath(QStringLiteral("경사도.tif"));
  const QgsCoordinateReferenceSystem workCrs =
      project->crs().isValid() ? project->crs() : QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187"));
  GDALAllRegister(); CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "YES");
  const bool sameDemPath =
      QFileInfo(sourceDemPath).absolutePath() == QFileInfo(demOut).absolutePath() &&
      QFileInfo(sourceDemPath).fileName().startsWith(QStringLiteral("국토지리원_DEM"));
  for (const QString& title : {QStringLiteral("[DEM] 국토지리원_DEM.tif"),
                               QStringLiteral("[DEM] 광역지형도(채색+음영기복)"),
                               QStringLiteral("[DEM] 경사도")}) {
    for (QgsMapLayer* l : project->mapLayersByName(title)) if (l) project->removeMapLayer(l->id());
  }
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QStringList srcPaths;
  auto addSrc = [&](const QString& p) {
    if (QFileInfo::exists(p) && !srcPaths.contains(QFileInfo(p).canonicalFilePath()))
      srcPaths.append(QFileInfo(p).canonicalFilePath());
  };
  if (!surveyPath.isEmpty()) {
    const QString demDir = QFileInfo(surveyPath).absoluteDir().filePath(QStringLiteral("DEM"));
    static const QRegularExpression rExt(QStringLiteral("^(img|tif|tiff|dem|asc|bil|flt)$"),
                                         QRegularExpression::CaseInsensitiveOption);
    QDirIterator it(demDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
      const QString f = it.next();
      if (rExt.match(QFileInfo(f).suffix()).hasMatch()) addSrc(f);
    }
  }
  if (srcPaths.isEmpty()) addSrc(sourceDemPath);
  QString activeDem = sourceDemPath;
  if (!sameDemPath || srcPaths.size() > 1) {
    const QString targetDem = safeOut(demOut);
    const QByteArray dstWkt = workCrs.toWkt().toUtf8();
    std::vector<GDALDatasetH> srcSets;
    const char* srcSrs = nullptr;
    for (const QString& sp : srcPaths) {
      if (GDALDatasetH ds = GDALOpen(sp.toUtf8().constData(), GA_ReadOnly)) {
        if (srcSets.empty() && (!GDALGetProjectionRef(ds) || !*GDALGetProjectionRef(ds))) {
          double srcGt[6] = {0}; GDALGetGeoTransform(ds, srcGt);
          srcSrs = (srcGt[0] >= 700000.0 && srcGt[3] >= 1300000.0) ? "EPSG:5179" : dstWkt.constData();
        }
        srcSets.push_back(ds);
      }
    }
    if (!srcSets.empty()) {
      const char* warpWithSrc[] = {"-s_srs", srcSrs, "-t_srs", dstWkt.constData(), "-r", "bilinear", "-overwrite", nullptr};
      const char* warpNoSrc[] = {"-t_srs", dstWkt.constData(), "-r", "bilinear", "-overwrite", nullptr};
      GDALWarpAppOptions* wOpts = GDALWarpAppOptionsNew(const_cast<char**>(srcSrs ? warpWithSrc : warpNoSrc), nullptr);
      int err = 0;
      if (GDALDatasetH warped = GDALWarp(targetDem.toUtf8().constData(), nullptr, static_cast<int>(srcSets.size()), srcSets.data(), wOpts, &err))
        GDALClose(warped);
      for (GDALDatasetH ds : srcSets) GDALClose(ds);
      GDALWarpAppOptionsFree(wOpts);
      if (QFileInfo::exists(targetDem)) activeDem = targetDem;
    }
  }
  const QString targetWide = safeOut(wideOut), targetSlope = safeOut(slopeOut);
  GDALDataset* ds = static_cast<GDALDataset*>(GDALOpen(activeDem.toUtf8().constData(), GA_ReadOnly));
  if (!ds) return false;
  const int w = std::clamp(ds->GetRasterXSize(), 4, 1024), h = std::clamp(ds->GetRasterYSize(), 4, 1024);
  double gt[6] = {0, 5, 0, 0, 0, -5}; ds->GetGeoTransform(gt);
  const double scaleX = static_cast<double>(ds->GetRasterXSize()) / w;
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
  std::vector<double> slopeDeg, hillshade, twi;
  hgis::paleo::HgisPaleoEngine::compute_terrain_derivatives(tf, z, slopeDeg, hillshade, twi);
  std::vector<uint8_t> wideRgba(z.size() * 4, 255);
  for (size_t i = 0; i < z.size(); ++i) {
    uint8_t r = 0, g = 0, b = 0; hypsoColor(z[i], r, g, b);
    const double factor = std::clamp(hillshade[i] / 255.0, 0.25, 1.0) * std::clamp(1.0 - (slopeDeg[i] / 90.0) * 0.35, 0.55, 1.0);
    wideRgba[i * 4 + 0] = static_cast<uint8_t>(std::clamp(std::lround(r * factor), 0L, 255L));
    wideRgba[i * 4 + 1] = static_cast<uint8_t>(std::clamp(std::lround(g * factor), 0L, 255L));
    wideRgba[i * 4 + 2] = static_cast<uint8_t>(std::clamp(std::lround(b * factor), 0L, 255L));
    wideRgba[i * 4 + 3] = 230;
  }
  writeRgbaGeoTiff(targetWide, tf, wkt, wideRgba, errorOut);
  writeFloatGeoTiff(targetSlope, tf, wkt, slopeDeg, errorOut);
  for (QgsMapLayer* old : ReferenceKind::find(project, QString::fromLatin1(ReferenceKind::kDem))) {
    if (old && old->name() != QStringLiteral("[DEM] 국토지리원_DEM.tif")) project->removeMapLayer(old->id());
  }
  if (auto* demLayer = addOrReplaceNamedRaster(project, activeDem, QStringLiteral("[DEM] 국토지리원_DEM.tif"))) {
    ReferenceKind::tag(demLayer, QString::fromLatin1(ReferenceKind::kDem));
    DemPresentation::apply(demLayer, QStringLiteral("viewport"), demLayer->extent());
  }
  addOrReplaceNamedRaster(project, targetSlope, QStringLiteral("[DEM] 경사도"));
  addOrReplaceNamedRaster(project, targetWide, QStringLiteral("[DEM] 광역지형도(채색+음영기복)"));
  if (canvas) LayerOps::refreshCanvasIfIdle(canvas);
  return true;
}

bool PaleoAnalysisRunner::importNgiiDemDownload(QgsProject* project, QgsMapCanvas* canvas,
                                                const QString& downloadedPath,
                                                const QString& surveyPath, QString* errorOut) {
  if (!project || !QFileInfo::exists(downloadedPath)) return false;
  const QString demDir = surveyPath.isEmpty() ? QFileInfo(downloadedPath).absolutePath()
                                              : QFileInfo(surveyPath).absoluteDir().filePath(QStringLiteral("DEM"));
  QDir().mkpath(demDir);
  QStringList candidates{downloadedPath};
  if (QFileInfo(downloadedPath).suffix().compare(QLatin1String("zip"), Qt::CaseInsensitive) == 0) {
    const auto extracted = TopographicArchive::prepare(downloadedPath, demDir);
    if (!extracted.error.isEmpty()) { if (errorOut) *errorOut = extracted.error; return false; }
    candidates = extracted.files;
  }
  static const QRegularExpression rasterExt(QStringLiteral("^(img|tif|tiff|dem|asc|bil|flt)$"),
                                            QRegularExpression::CaseInsensitiveOption);
  for (const QString& file : candidates)
    if (rasterExt.match(QFileInfo(file).suffix()).hasMatch())
      return registerDemWideLayers(project, canvas, file, surveyPath, errorOut);
  for (const QString& file : candidates) {
    if (QFileInfo(file).suffix().compare(QLatin1String("txt"), Qt::CaseInsensitive) != 0) continue;
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
    std::vector<double> xs, ys, zs;
    double minX = 1e18, maxX = -1e18, minY = 1e18, maxY = -1e18;
    QTextStream ts(&f);
    static const QRegularExpression sep(QStringLiteral("[\\s,;]+"));
    while (!ts.atEnd() && xs.size() < 50000) {
      const QStringList parts = ts.readLine().trimmed().split(sep, Qt::SkipEmptyParts);
      if (parts.size() < 3) continue;
      bool okX = false, okY = false, okZ = false;
      const double x = parts[0].toDouble(&okX), y = parts[1].toDouble(&okY), z = parts[2].toDouble(&okZ);
      if (!okX || !okY || !okZ || !std::isfinite(z) || z < -500.0 || z > 3000.0) continue;
      xs.push_back(x); ys.push_back(y); zs.push_back(z);
      minX = std::min(minX, x); maxX = std::max(maxX, x); minY = std::min(minY, y); maxY = std::max(maxY, y);
    }
    if (xs.size() >= 4 && maxX > minX && maxY > minY) {
      const double cell = std::max(5.0, std::max(maxX - minX, maxY - minY) / 240.0);
      const int w = std::clamp(static_cast<int>(std::ceil((maxX - minX) / cell)), 8, 360);
      const int h = std::clamp(static_cast<int>(std::ceil((maxY - minY) / cell)), 8, 360);
      const hgis::paleo::GridTransform tf{minX, maxY, cell, w, h};
      const auto grid = hgis::paleo::HgisPaleoEngine::idw_interpolate_grid(tf, xs, ys, zs, 8, 2.0);
      const QString tifPath = QDir(demDir).filePath(QFileInfo(file).completeBaseName() + QStringLiteral("_dem.tif"));
      const QgsCoordinateReferenceSystem crs(minX >= 700000.0 ? QStringLiteral("EPSG:5179")
          : (project->crs().isValid() ? project->crs().authid() : QStringLiteral("EPSG:5187")));
      if (writeFloatGeoTiff(tifPath, tf, crs.toWkt(), grid, errorOut))
        return registerDemWideLayers(project, canvas, tifPath, surveyPath, errorOut);
    }
  }
  if (errorOut) *errorOut = QStringLiteral("받은 압축 파일에서 DEM 래스터(.img/.tif/.dem/.txt)를 찾지 못했습니다.");
  return false;
}
