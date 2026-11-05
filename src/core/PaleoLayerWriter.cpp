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
#include <QList>
#include <QPair>
#include <QRegularExpression>
#include <QTextStream>
#include <algorithm>
#include <array>
#include <cmath>
#include <cpl_conv.h>
#include <gdal_priv.h>
#include <gdal_utils.h>
#include <qgsbilinearrasterresampler.h>
#include <qgscolorrampshader.h>
#include <qgscoordinatereferencesystem.h>
#include <qgslayertreelayer.h>
#include <qgslayertreemodellegendnode.h>
#include <qgsmapcanvas.h>
#include <qgsmaplayerlegend.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgsrasterresamplefilter.h>
#include <qgsrastershader.h>
#include <qgssinglebandpseudocolorrenderer.h>

namespace {
struct Stop { double t; uint8_t r, g, b; };
constexpr std::array<Stop, 7> kImhofStops = {{
    {0.00, 118, 198, 174}, {0.15, 156, 216, 146}, {0.32, 208, 226, 132},
    {0.50, 242, 214, 128}, {0.68, 234, 174, 110}, {0.85, 206, 138, 96},
    {1.00, 244, 239, 233},
}};

void imhofColor(double normT, uint8_t& r, uint8_t& g, uint8_t& b) noexcept {
  const double u = std::clamp(normT, 0.0, 1.0);
  for (size_t i = 1; i < kImhofStops.size(); ++i) {
    if (u <= kImhofStops[i].t) {
      const double f = (u - kImhofStops[i - 1].t) / std::max(1e-6, kImhofStops[i].t - kImhofStops[i - 1].t);
      r = static_cast<uint8_t>(std::lround(kImhofStops[i - 1].r + f * (kImhofStops[i].r - kImhofStops[i - 1].r)));
      g = static_cast<uint8_t>(std::lround(kImhofStops[i - 1].g + f * (kImhofStops[i].g - kImhofStops[i - 1].g)));
      b = static_cast<uint8_t>(std::lround(kImhofStops[i - 1].b + f * (kImhofStops[i].b - kImhofStops[i - 1].b)));
      return;
    }
  }
  r = kImhofStops.back().r; g = kImhofStops.back().g; b = kImhofStops.back().b;
}

class SwatchListLegend final : public QgsMapLayerLegend {
public:
  SwatchListLegend(QList<QPair<QColor, QString>> items, QObject* parent = nullptr)
      : QgsMapLayerLegend(parent), m_items(std::move(items)) {}
  QList<QgsLayerTreeModelLegendNode*> createLayerTreeModelLegendNodes(QgsLayerTreeLayer* node) override {
    QList<QgsLayerTreeModelLegendNode*> out;
    if (!node) return out;
    for (const auto& it : m_items) out.append(new QgsRasterSymbolLegendNode(node, it.first, it.second));
    return out;
  }
private:
  QList<QPair<QColor, QString>> m_items;
};

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
  if (auto* rf = rl->resampleFilter()) rf->setZoomedInResampler(new QgsBilinearRasterResampler());
  LayerOps::markReferenceLayer(rl); LayerOps::applyLegendCrsLabel(rl);
  if (!project->addMapLayer(rl, true)) { delete rl; return nullptr; }
  LayerOps::placeInLegendGroup(project, rl, QStringLiteral("참조 지도"));
  return rl;
}
}  // namespace

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
  std::vector<double> z(rawZ.size()), sortedZ;
  sortedZ.reserve(rawZ.size());
  double validZ = 20.0;
  for (size_t i = 0; i < rawZ.size(); ++i) {
    if (std::isfinite(rawZ[i]) && rawZ[i] > -500.0f) validZ = rawZ[i];
    z[i] = validZ;
    sortedZ.push_back(validZ);
  }
  std::sort(sortedZ.begin(), sortedZ.end());
  const double zLow = sortedZ.empty() ? 0.0 : sortedZ[sortedZ.size() * 2 / 100];
  const double zHigh = std::max(zLow + 20.0, sortedZ.empty() ? 200.0 : sortedZ[sortedZ.size() * 98 / 100]);
  std::vector<double> slopeDeg, hillshade, twi;
  hgis::paleo::HgisPaleoEngine::compute_terrain_derivatives(tf, z, slopeDeg, hillshade, twi);
  const std::vector<double> tpi50 = hgis::paleo::HgisPaleoEngine::compute_tpi_integral_image(tf, z, 50.0);
  std::vector<uint8_t> wideRgba(z.size() * 4, 255);
  for (size_t i = 0; i < z.size(); ++i) {
    uint8_t r = 0, g = 0, b = 0;
    imhofColor((z[i] - zLow) / (zHigh - zLow), r, g, b);
    const double hs = (hillshade[i] - 128.0) / 127.0;
    const double ao = std::clamp(tpi50[i] / 5.0, -0.18, 0.15) * 26.0;
    const double slp = std::clamp(slopeDeg[i] / 42.0, 0.0, 1.0) * 16.0;
    const double dr = (hs >= 0.0 ? 28.0 * hs : 38.0 * hs) + ao - slp;
    const double dg = (hs >= 0.0 ? 22.0 * hs : 34.0 * hs) + ao - slp;
    const double db = (hs >= 0.0 ? 8.0 * hs : 20.0 * hs) + ao * 0.7 - slp * 0.6;
    wideRgba[i * 4 + 0] = static_cast<uint8_t>(std::clamp(std::lround(r + dr), 0L, 255L));
    wideRgba[i * 4 + 1] = static_cast<uint8_t>(std::clamp(std::lround(g + dg), 0L, 255L));
    wideRgba[i * 4 + 2] = static_cast<uint8_t>(std::clamp(std::lround(b + db), 0L, 255L));
    wideRgba[i * 4 + 3] = 236;
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
  if (auto* slpLayer = addOrReplaceNamedRaster(project, targetSlope, QStringLiteral("[DEM] 경사도"))) {
    auto* fcn = new QgsColorRampShader(0.0, 45.0);
    fcn->setColorRampType(Qgis::ShaderInterpolationMethod::Discrete);
    fcn->setColorRampItemList({
        QgsColorRampShader::ColorRampItem(2.0, QColor(46, 184, 114), QStringLiteral("0~2° 평탄지 (충적·단구면)")),
        QgsColorRampShader::ColorRampItem(5.0, QColor(148, 216, 88), QStringLiteral("2~5° 완경사 (유적 입지 유망)")),
        QgsColorRampShader::ColorRampItem(12.0, QColor(245, 212, 66), QStringLiteral("5~12° 사면 (구릉·선상지)")),
        QgsColorRampShader::ColorRampItem(20.0, QColor(242, 132, 44), QStringLiteral("12~20° 급사면 (성곽·고분)")),
        QgsColorRampShader::ColorRampItem(90.0, QColor(192, 57, 43), QStringLiteral("20°+ 산지 급경사")),
    });
    auto* sh = new QgsRasterShader();
    sh->setRasterShaderFunction(fcn);
    slpLayer->setRenderer(new QgsSingleBandPseudoColorRenderer(slpLayer->dataProvider(), 1, sh));
    slpLayer->setOpacity(0.84);
  }
  if (auto* wideLayer = addOrReplaceNamedRaster(project, targetWide, QStringLiteral("[DEM] 광역지형도(채색+음영기복)"))) {
    const auto zAt = [&](double f) { return qRound(zLow + (zHigh - zLow) * f); };
    wideLayer->setLegend(new SwatchListLegend({
        {QColor(118, 198, 174), QStringLiteral("저지·충적면 (%1~%2m)").arg(zAt(0.0)).arg(zAt(0.2))},
        {QColor(156, 216, 146), QStringLiteral("완경사·저위단구 (%1~%2m)").arg(zAt(0.2)).arg(zAt(0.4))},
        {QColor(242, 214, 128), QStringLiteral("구릉·중위단구 (%1~%2m)").arg(zAt(0.4)).arg(zAt(0.65))},
        {QColor(234, 174, 110), QStringLiteral("산록·사면 (%1~%2m)").arg(zAt(0.65)).arg(zAt(0.85))},
        {QColor(206, 138, 96), QStringLiteral("산지·능선 (%1~%2m+)").arg(zAt(0.85)).arg(zAt(1.0))},
    }, wideLayer));
  }
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
