#include "HgisPaleoEngine.h"
#include "PaleoAnalysisRunner.h"

#include <QDir>
#include <QFileInfo>
#include <algorithm>
#include <cmath>
#include <cpl_conv.h>
#include <gdal_priv.h>
#include <limits>

namespace hgis::paleo {

void HgisPaleoEngine::edt_1d(const std::vector<double>& f, std::vector<double>& d, int n) {
  std::vector<int> v(n, 0);
  std::vector<double> z(n + 1, 0.0);
  int k = 0;
  v[0] = 0;
  z[0] = -std::numeric_limits<double>::infinity();
  z[1] = +std::numeric_limits<double>::infinity();

  for (int q = 1; q < n; ++q) {
    double s = 0.0;
    while (true) {
      const int vk = v[k];
      s = ((f[q] + static_cast<double>(q) * q) - (f[vk] + static_cast<double>(vk) * vk)) /
          (2.0 * (q - vk));
      if (s > z[k]) break;
      --k;
    }
    ++k;
    v[k] = q;
    z[k] = s;
    z[k + 1] = +std::numeric_limits<double>::infinity();
  }

  k = 0;
  for (int q = 0; q < n; ++q) {
    while (z[k + 1] < q) ++k;
    const double dx = static_cast<double>(q - v[k]);
    d[q] = dx * dx + f[v[k]];
  }
}

std::vector<double> HgisPaleoEngine::compute_river_distance_edt(
    const GridTransform& tf, const std::vector<uint8_t>& river_mask) {
  const int H = tf.height;
  const int W = tf.width;
  constexpr double INF = 1e18;

  std::vector<double> grid_sq(static_cast<size_t>(H) * W, INF);
  bool has_river = false;
  for (size_t i = 0; i < grid_sq.size(); ++i) {
    if (river_mask[i] != 0) {
      grid_sq[i] = 0.0;
      has_river = true;
    }
  }
  if (!has_river) {
    return std::vector<double>(static_cast<size_t>(H) * W, 0.0);
  }

  std::vector<double> f_col(H);
  std::vector<double> d_col(H);
  for (int c = 0; c < W; ++c) {
    for (int r = 0; r < H; ++r) f_col[r] = grid_sq[static_cast<size_t>(r) * W + c];
    edt_1d(f_col, d_col, H);
    for (int r = 0; r < H; ++r) grid_sq[static_cast<size_t>(r) * W + c] = d_col[r];
  }

  std::vector<double> f_row(W);
  std::vector<double> d_row(W);
  std::vector<double> dist_m(static_cast<size_t>(H) * W, 0.0);
  for (int r = 0; r < H; ++r) {
    for (int c = 0; c < W; ++c) f_row[c] = grid_sq[static_cast<size_t>(r) * W + c];
    edt_1d(f_row, d_row, W);
    for (int c = 0; c < W; ++c) {
      dist_m[static_cast<size_t>(r) * W + c] = std::sqrt(d_row[c]) * tf.cell_size;
    }
  }
  return dist_m;
}

std::vector<double> HgisPaleoEngine::compute_tpi_integral_image(
    const GridTransform& tf, const std::vector<double>& z_grid, double radius_m) {
  const int H = tf.height;
  const int W = tf.width;
  const int rad_px = std::max(1, static_cast<int>(std::round(radius_m / tf.cell_size)));

  std::vector<double> sat(static_cast<size_t>(H + 1) * (W + 1), 0.0);
  for (int r = 0; r < H; ++r) {
    double row_sum = 0.0;
    for (int c = 0; c < W; ++c) {
      row_sum += z_grid[static_cast<size_t>(r) * W + c];
      sat[static_cast<size_t>(r + 1) * (W + 1) + (c + 1)] =
          sat[static_cast<size_t>(r) * (W + 1) + (c + 1)] + row_sum;
    }
  }

  std::vector<double> tpi(static_cast<size_t>(H) * W, 0.0);
  for (int r = 0; r < H; ++r) {
    const int r0 = std::max(0, r - rad_px);
    const int r1 = std::min(H - 1, r + rad_px);
    for (int c = 0; c < W; ++c) {
      const int c0 = std::max(0, c - rad_px);
      const int c1 = std::min(W - 1, c + rad_px);

      const double count = static_cast<double>((r1 - r0 + 1) * (c1 - c0 + 1));
      const double box_sum = sat[static_cast<size_t>(r1 + 1) * (W + 1) + (c1 + 1)] -
                             sat[static_cast<size_t>(r0) * (W + 1) + (c1 + 1)] -
                             sat[static_cast<size_t>(r1 + 1) * (W + 1) + c0] +
                             sat[static_cast<size_t>(r0) * (W + 1) + c0];
      const double mean_z = box_sum / count;
      tpi[static_cast<size_t>(r) * W + c] = z_grid[static_cast<size_t>(r) * W + c] - mean_z;
    }
  }
  return tpi;
}

void HgisPaleoEngine::compute_terrain_derivatives(const GridTransform& tf,
                                                  const std::vector<double>& z_paleo,
                                                  std::vector<double>& out_slope_deg,
                                                  std::vector<double>& out_hillshade,
                                                  std::vector<double>& out_twi) {
  const int H = tf.height;
  const int W = tf.width;
  const double cs = tf.cell_size;
  constexpr double PI = 3.14159265358979323846;

  out_slope_deg.assign(static_cast<size_t>(H) * W, 0.0);
  out_hillshade.assign(static_cast<size_t>(H) * W, 0.0);
  out_twi.assign(static_cast<size_t>(H) * W, 0.0);

  constexpr double zenith_rad = (90.0 - 30.0) * PI / 180.0;
  const double cos_zen = std::cos(zenith_rad);
  const double sin_zen = std::sin(zenith_rad);
  constexpr std::array<double, 4> az_deg_list = {225.0, 270.0, 315.0, 360.0};

  auto get_z = [&](int r, int c) noexcept -> double {
    const int rr = std::clamp(r, 0, H - 1);
    const int cc = std::clamp(c, 0, W - 1);
    return z_paleo[static_cast<size_t>(rr) * W + cc];
  };

  for (int r = 0; r < H; ++r) {
    for (int c = 0; c < W; ++c) {
      const double dz_dx = ((get_z(r - 1, c + 1) + 2.0 * get_z(r, c + 1) + get_z(r + 1, c + 1)) -
                            (get_z(r - 1, c - 1) + 2.0 * get_z(r, c - 1) + get_z(r + 1, c - 1))) /
                           (8.0 * cs);
      const double dz_dy = ((get_z(r - 1, c - 1) + 2.0 * get_z(r - 1, c) + get_z(r - 1, c + 1)) -
                            (get_z(r + 1, c - 1) + 2.0 * get_z(r + 1, c) + get_z(r + 1, c + 1))) /
                           (8.0 * cs);

      const double slope_rad = std::atan(std::hypot(dz_dx, dz_dy));
      const double slope_d = slope_rad * 180.0 / PI;
      out_slope_deg[static_cast<size_t>(r) * W + c] = slope_d;

      double aspect_rad = std::atan2(dz_dy, -dz_dx);
      if (aspect_rad < 0.0) aspect_rad += 2.0 * PI;

      double hs_accum = 0.0;
      for (double az_d : az_deg_list) {
        double az_math = (360.0 - az_d + 90.0) * PI / 180.0;
        az_math = std::fmod(az_math, 2.0 * PI);
        const double hs = cos_zen * std::cos(slope_rad) +
                          sin_zen * std::sin(slope_rad) * std::cos(az_math - aspect_rad);
        hs_accum += std::clamp(hs, 0.0, 1.0);
      }
      out_hillshade[static_cast<size_t>(r) * W + c] = (hs_accum / 4.0) * 255.0;

      const double tan_b = std::max(std::tan(slope_rad), 0.001);
      const double upslope_area = cs * (1.0 + 10.0 / (1.0 + slope_d));
      out_twi[static_cast<size_t>(r) * W + c] = std::log(upslope_area / tan_b);
    }
  }
}

}  // namespace hgis::paleo

bool PaleoAnalysisRunner::writeRgbaGeoTiff(const QString& outPath, const hgis::paleo::GridTransform& tf,
                                           const QString& crsWkt, const std::vector<uint8_t>& rgba,
                                           QString* errorOut) {
  if (tf.width <= 0 || tf.height <= 0 || rgba.size() < static_cast<size_t>(tf.width) * tf.height * 4) {
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

bool PaleoAnalysisRunner::writeFloatGeoTiff(const QString& outPath, const hgis::paleo::GridTransform& tf,
                                            const QString& crsWkt, const std::vector<double>& values,
                                            QString* errorOut) {
  if (tf.width <= 0 || tf.height <= 0 || values.size() < static_cast<size_t>(tf.width) * tf.height) {
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
