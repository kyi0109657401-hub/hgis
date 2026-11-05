#include "HgisPaleoEngine.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>

namespace hgis::paleo {

PaleoAnalysisResult HgisPaleoEngine::run_pipeline(
    const std::vector<double>& z_curr, const GridTransform& tf,
    const std::vector<BoreholeRawInput>& raw_boreholes,
    const std::vector<SoilLandformCode>& soil_grid, const std::vector<GeologyCode>& geol_grid,
    const std::vector<uint8_t>& river_mask, const std::vector<uint8_t>& aoi_mask,
    const std::pair<double, double>& aoi_center_xy, std::string_view nearest_river_name,
    std::string_view dominant_soil_label, std::string_view dominant_geol_label) {
  PaleoAnalysisResult res;
  res.transform = tf;
  const int H = tf.height, W = tf.width;
  const size_t N = static_cast<size_t>(H) * W;
  std::vector<double> bh_x, bh_y, bh_fill, bh_base, bh_coarse;

  for (const auto& bh : raw_boreholes) {
    ParsedBoreholeSummary parsed = parse_single_borehole(bh);
    bh_x.push_back(parsed.x);
    bh_y.push_back(parsed.y);
    bh_fill.push_back(parsed.fill_thickness_m);
    bh_base.push_back(parsed.weathered_top_depth_m);
    bh_coarse.push_back(parsed.sed_type == "Coarse" ? 1.0 : (parsed.sed_type == "Fine" ? 0.0 : 0.5));
    res.parsed_boreholes.push_back(std::move(parsed));
  }

  if (!bh_x.empty()) {
    res.fill_depth_grid = idw_interpolate_grid(tf, bh_x, bh_y, bh_fill, 8, 2.0);
    res.base_depth_grid = idw_interpolate_grid(tf, bh_x, bh_y, bh_base, 8, 2.0);
    res.coarse_prob_grid = idw_interpolate_grid(tf, bh_x, bh_y, bh_coarse, 8, 2.0);
  } else {
    res.fill_depth_grid.assign(N, 0.0);
    res.base_depth_grid.assign(N, 5.0);
    res.coarse_prob_grid.assign(N, 0.5);
  }

  double fallback_z = 20.0;
  for (double v : z_curr) {
    if (std::isfinite(v) && v > -500.0) {
      fallback_z = v;
      break;
    }
  }
  std::vector<double> z_raw(N);
  for (size_t i = 0; i < N; ++i) {
    const double zc = (std::isfinite(z_curr[i]) && z_curr[i] > -500.0) ? z_curr[i] : fallback_z;
    fallback_z = zc;
    z_raw[i] = zc - res.fill_depth_grid[i];
  }
  res.z_paleo.resize(N);
  for (int r = 0; r < H; ++r) {
    for (int c = 0; c < W; ++c) {
      double sum = 0.0, wsum = 0.0;
      for (int dr = -1; dr <= 1; ++dr) {
        const int rr = std::clamp(r + dr, 0, H - 1);
        for (int dc = -1; dc <= 1; ++dc) {
          const int cc = std::clamp(c + dc, 0, W - 1);
          const double w = (dr == 0 && dc == 0) ? 4.0 : ((dr == 0 || dc == 0) ? 2.0 : 1.0);
          sum += w * z_raw[static_cast<size_t>(rr) * W + cc];
          wsum += w;
        }
      }
      res.z_paleo[static_cast<size_t>(r) * W + c] = sum / wsum;
    }
  }

  std::vector<double> riv_x;
  std::vector<double> riv_y;
  std::vector<double> riv_z;
  for (int r = 0; r < H; ++r) {
    for (int c = 0; c < W; ++c) {
      const size_t idx = static_cast<size_t>(r) * W + c;
      if (river_mask[idx] != 0) {
        double wx = 0.0;
        double wy = 0.0;
        tf.pixel_to_world(r, c, wx, wy);
        riv_x.push_back(wx);
        riv_y.push_back(wy);
        riv_z.push_back(res.z_paleo[idx]);
      }
    }
  }

  std::vector<double> z_river_grid(N, 0.0);
  if (!riv_x.empty()) {
    z_river_grid = idw_interpolate_grid(tf, riv_x, riv_y, riv_z, 12, 2.0);
  } else if (!res.z_paleo.empty()) {
    const double min_z = *std::min_element(res.z_paleo.begin(), res.z_paleo.end());
    std::fill(z_river_grid.begin(), z_river_grid.end(), min_z);
  }

  res.rem.resize(N);
  for (size_t i = 0; i < N; ++i) {
    res.rem[i] = std::max(0.0, res.z_paleo[i] - z_river_grid[i]);
  }

  std::vector<double> twi_dummy;
  compute_terrain_derivatives(tf, res.z_paleo, res.slope_deg, res.multidirectional_hillshade,
                              twi_dummy);
  res.tpi_50m = compute_tpi_integral_image(tf, res.z_paleo, 50.0);
  res.dist_to_river_m = compute_river_distance_edt(tf, river_mask);

  res.paleo_class_grid.assign(N, PaleoClass::Unclassified);
  res.paleo_rgba_buffer.assign(N * 4, 0);

  for (size_t i = 0; i < N; ++i) {
    const double s = res.slope_deg[i];
    const double r_m = res.rem[i];
    const double tpi = res.tpi_50m[i];
    const SoilLandformCode sc = soil_grid[i];
    const GeologyCode gc = geol_grid[i];
    const double c_prob = res.coarse_prob_grid[i];

    PaleoClass cls = PaleoClass::Unclassified;
    if (s >= 15.0) {
      cls = PaleoClass::SteepMountain;
    } else if ((sc == SoilLandformCode::DiluvialTerrace || gc == GeologyCode::QrTerrace ||
                gc == GeologyCode::QbaBasalt) &&
               r_m >= 15.0 && s < 12.0) {
      cls = PaleoClass::TerraceT2T3;
    } else if ((sc == SoilLandformCode::DiluvialTerrace || gc == GeologyCode::QrTerrace ||
                r_m >= 5.0) &&
               r_m >= 5.0 && r_m < 15.0 && s < 8.0) {
      cls = PaleoClass::TerraceT1;
    } else if (sc == SoilLandformCode::ValleyFan && s >= 1.5 && s < 10.0) {
      cls = PaleoClass::ValleyAlluvialFan;
    } else if ((sc == SoilLandformCode::FluvialPlain || gc == GeologyCode::QaAlluvium) &&
               s < 3.0) {
      if ((r_m >= 1.2 && r_m < 5.0 && tpi >= 0.15) || (r_m >= 0.8 && c_prob >= 0.6)) {
        cls = PaleoClass::NaturalLevee;
      } else {
        cls = PaleoClass::PaleochannelSwamp;
      }
    } else if (s >= 2.0 && s < 15.0) {
      cls = PaleoClass::HillslopeGentle;
    } else {
      cls = (tpi >= 0.0) ? PaleoClass::NaturalLevee : PaleoClass::PaleochannelSwamp;
    }

    res.paleo_class_grid[i] = cls;
  }

  for (int pass = 0; pass < 2; ++pass) {
    std::vector<PaleoClass> filtered = res.paleo_class_grid;
    for (int r = 0; r < H; ++r) {
      for (int c = 0; c < W; ++c) {
        std::array<int, 8> counts{};
        for (int dr = -1; dr <= 1; ++dr) {
          const int rr = std::clamp(r + dr, 0, H - 1);
          for (int dc = -1; dc <= 1; ++dc) {
            const int cc = std::clamp(c + dc, 0, W - 1);
            const size_t code = static_cast<size_t>(res.paleo_class_grid[static_cast<size_t>(rr) * W + cc]);
            if (code < counts.size()) counts[code] += (dr == 0 && dc == 0) ? 2 : 1;
          }
        }
        size_t bestCode = static_cast<size_t>(res.paleo_class_grid[static_cast<size_t>(r) * W + c]);
        for (size_t code = 1; code < counts.size(); ++code) {
          if (counts[code] > counts[bestCode]) bestCode = code;
        }
        filtered[static_cast<size_t>(r) * W + c] = static_cast<PaleoClass>(bestCode);
      }
    }
    res.paleo_class_grid.swap(filtered);
  }

  for (size_t i = 0; i < N; ++i) {
    const auto& rgba = kPaleoClassTable[static_cast<size_t>(res.paleo_class_grid[i])].rgba;
    const double hs = (res.multidirectional_hillshade[i] - 128.0) / 127.0;
    const double ao = std::clamp(res.tpi_50m[i] / 5.0, -0.15, 0.15) * 20.0;
    const double slp = std::clamp(res.slope_deg[i] / 40.0, 0.0, 1.0) * 14.0;
    const double dr = (hs >= 0.0 ? 24.0 * hs : 32.0 * hs) + ao - slp;
    const double dg = (hs >= 0.0 ? 20.0 * hs : 28.0 * hs) + ao - slp;
    const double db = (hs >= 0.0 ? 10.0 * hs : 18.0 * hs) + ao * 0.7 - slp * 0.6;
    res.paleo_rgba_buffer[i * 4 + 0] = static_cast<uint8_t>(std::clamp(std::lround(rgba[0] + dr), 0L, 255L));
    res.paleo_rgba_buffer[i * 4 + 1] = static_cast<uint8_t>(std::clamp(std::lround(rgba[1] + dg), 0L, 255L));
    res.paleo_rgba_buffer[i * 4 + 2] = static_cast<uint8_t>(std::clamp(std::lround(rgba[2] + db), 0L, 255L));
    res.paleo_rgba_buffer[i * 4 + 3] = 216;
  }

  const int mid_r = H / 2;
  for (int c = 0; c < W; ++c) {
    const size_t idx = static_cast<size_t>(mid_r) * W + c;
    res.center_cross_section.dist_m.push_back(c * tf.cell_size);
    res.center_cross_section.z_surface_curr_m.push_back(z_curr[idx]);
    res.center_cross_section.z_paleo_top_m.push_back(res.z_paleo[idx]);
    res.center_cross_section.z_bedrock_base_m.push_back(z_curr[idx] - res.base_depth_grid[idx]);
    res.center_cross_section.z_river_level_m.push_back(z_river_grid[idx]);
  }

  double sum_zc = 0.0, sum_zp = 0.0, sum_rem = 0.0, sum_slp = 0.0, sum_dist = 0.0;
  size_t aoi_cnt = 0;
  std::array<size_t, 8> cls_counts = {0, 0, 0, 0, 0, 0, 0, 0};

  for (size_t i = 0; i < N; ++i) {
    if (aoi_mask[i] != 0) {
      sum_zc += z_curr[i];
      sum_zp += res.z_paleo[i];
      sum_rem += res.rem[i];
      sum_slp += res.slope_deg[i];
      sum_dist += res.dist_to_river_m[i];
      cls_counts[static_cast<size_t>(res.paleo_class_grid[i])]++;
      aoi_cnt++;
    }
  }
  if (aoi_cnt == 0 && N > 0) {
    aoi_cnt = 1;
    cls_counts[static_cast<size_t>(res.paleo_class_grid[N / 2])] = 1;
  }

  const double mean_zc = sum_zc / aoi_cnt;
  const double mean_zp = sum_zp / aoi_cnt;
  const double mean_rem = sum_rem / aoi_cnt;
  const double mean_slp = sum_slp / aoi_cnt;
  const double mean_dist = sum_dist / aoi_cnt;

  std::vector<std::pair<size_t, size_t>> sorted_cls;
  for (size_t code = 1; code <= 7; ++code) {
    if (cls_counts[code] > 0) sorted_cls.push_back({cls_counts[code], code});
  }
  std::sort(sorted_cls.rbegin(), sorted_cls.rend());

  std::ostringstream cls_str;
  for (size_t i = 0; i < std::min<size_t>(2, sorted_cls.size()); ++i) {
    if (i > 0) cls_str << " 및 ";
    const int pct =
        static_cast<int>(std::round(100.0 * sorted_cls[i].first / static_cast<double>(aoi_cnt)));
    cls_str << kPaleoClassTable[sorted_cls[i].second].name_kor << "(" << pct << "%)";
  }

  double est_fill = 0.0, est_alluv = 0.0, est_base = 0.0, est_water = 0.0;
  std::string upper_u = "SM", lower_u = "CL";
  if (!res.parsed_boreholes.empty()) {
    std::vector<std::pair<double, size_t>> bh_dists;
    for (size_t i = 0; i < res.parsed_boreholes.size(); ++i) {
      const double d = std::hypot(res.parsed_boreholes[i].x - aoi_center_xy.first,
                                  res.parsed_boreholes[i].y - aoi_center_xy.second);
      bh_dists.push_back({std::max(d, 1.0), i});
    }
    std::sort(bh_dists.begin(), bh_dists.end());
    const size_t k_bh = std::min<size_t>(4, bh_dists.size());
    double w_sum = 0.0;
    for (size_t k = 0; k < k_bh; ++k) {
      const double w = 1.0 / (bh_dists[k].first * bh_dists[k].first);
      const auto& b = res.parsed_boreholes[bh_dists[k].second];
      w_sum += w;
      est_fill += w * b.fill_thickness_m;
      est_alluv += w * b.alluvial_thickness_m;
      est_base += w * b.weathered_top_depth_m;
      est_water += w * b.water_depth_m;
    }
    est_fill /= w_sum;
    est_alluv /= w_sum;
    est_base /= w_sum;
    est_water /= w_sum;
    upper_u = res.parsed_boreholes[bh_dists[0].second].upper_alluv_uscs;
    lower_u = res.parsed_boreholes[bh_dists[0].second].lower_alluv_uscs;
  }

  std::ostringstream report;
  report << std::fixed << std::setprecision(1);
  report << "[조사구역(AOI) 고지형·기본층서 종합 판독 요약 (반경 10km 5대 자료 근거)]\n"
         << "• 지형 입지: 평균 해발 " << mean_zc << "m (매립 제거 원지형 보정 시 " << mean_zp
         << "m), 최인접 하천(" << nearest_river_name << ")으로부터 직선거리 "
         << static_cast<int>(std::round(mean_dist)) << "m, 하상비고 +" << mean_rem
         << "m, 평균 경사도 " << mean_slp << "°로 ‘" << cls_str.str() << "’에 해당합니다"
         << "(흙토람: " << dominant_soil_label << ", 지질: " << dominant_geol_label << ").\n"
         << "• 예상 지하 층서 및 시굴 심도 (인접 시추공 IDW 보간 기준):\n"
         << "  1. 현대 매립·성토층: 지표 아래 0.0 ~ " << est_fill << "m (기초 굴착 제거 구간)\n"
         << "  2. 문화층 부존 가능 충적층: 심도 " << est_fill << " ~ " << est_base << "m (두께 "
         << est_alluv << "m, 해발 " << (mean_zc - est_fill) << " ~ " << (mean_zc - est_base)
         << "m) — 상부(" << upper_u << ") 및 하부(" << lower_u << ") 분포\n"
         << "  3. 생토면(풍화토 상단): 심도 " << est_base << "m (해발 " << (mean_zc - est_base)
         << "m) 출현 (신석기 이후 유구 확인 하한선)\n"
         << "  4. 지하수위: GL -" << est_water << "m (해발 " << (mean_zc - est_water)
         << "m) — 심도 " << est_water << "m 이하 굴착 시 양수 작업 대비 및 수침 목기·유기물 출토 유의\n"
         << "• 고지형 분류 판독 근거(Rule Matrix): 국토지리원 DEM 원지형 복원(Z_paleo = Z_curr - D_fill), "
         << "하천 수면 IDW 보간 하상비고(REM), 반경 50m 적분영상 국지기복(TPI_50m), "
         << "Felzenszwalb O(N) 하천 유클리드 거리(EDT), 흙토람 토양지형, KIGAM 1:5만 지질, "
         << "국토지반정보 시추주상도(" << res.parsed_boreholes.size() << "공) 교차 판독.";

  res.aoi_summary_text = report.str();
  return res;
}

}  // namespace hgis::paleo
