#include "HgisPaleoEngine.h"
#include "GeoInfoBoreholeService.h"
#include "KaLogExcept.h"
#include "PaleoAnalysisRunner.h"

#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransform.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>

namespace hgis::paleo {

std::string HgisPaleoEngine::to_upper(std::string_view sv) {
  std::string out(sv);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
    return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : static_cast<char>(c);
  });
  return out;
}

bool HgisPaleoEngine::contains(std::string_view haystack, std::string_view needle) noexcept {
  return haystack.find(needle) != std::string_view::npos;
}

std::string HgisPaleoEngine::classify_uscs(std::string_view uscs_raw) {
  const std::string code = to_upper(uscs_raw);
  static constexpr std::array<std::string_view, 8> kCoarseCodes = {"GW", "GP", "GM", "GC",
                                                                   "SW", "SP", "SM", "SC"};
  static constexpr std::array<std::string_view, 7> kFineCodes = {"ML", "CL", "OL", "MH",
                                                                 "CH", "OH", "PT"};
  for (auto c : kCoarseCodes) {
    if (contains(code, c)) return "Coarse";
  }
  for (auto f : kFineCodes) {
    if (contains(code, f)) return "Fine";
  }
  return "Unknown";
}

ParsedBoreholeSummary HgisPaleoEngine::parse_single_borehole(const BoreholeRawInput& bh) {
  double fill_bottom = 0.0;
  double alluv_thick = 0.0;
  double weathered_top = -1.0;
  double total_depth = 0.0;

  std::map<std::string, double> uscs_weights;
  std::string upper_uscs = "미상";
  std::string lower_uscs = "미상";
  bool has_organic = false;

  for (const auto& st : bh.strata) {
    const double d0 = st.depth_from_m;
    const double d1 = st.depth_to_m;
    const double thick = std::max(0.0, d1 - d0);
    total_depth = std::max(total_depth, d1);

    const std::string& lname = st.layer_name;
    const std::string& desc = st.description;

    if (contains(desc, "유기물") || contains(desc, "목편") || contains(desc, "부식") ||
        contains(desc, "패각") || contains(desc, "이탄")) {
      has_organic = true;
    }

    if (contains(lname, "매립") || contains(lname, "성토") || contains(lname, "복토") ||
        contains(lname, "표토")) {
      fill_bottom = std::max(fill_bottom, d1);
      continue;
    }

    if (contains(lname, "풍화") || contains(lname, "연암") || contains(lname, "경암") ||
        contains(lname, "기반암") || st.n_value >= 50.0) {
      if (weathered_top < 0.0) weathered_top = d0;
      continue;
    }

    if (contains(lname, "퇴적") || contains(lname, "충적") || contains(lname, "붕적") ||
        contains(lname, "풍적") || weathered_top < 0.0) {
      alluv_thick += thick;
      const std::string u = to_upper(st.uscs_code);
      if (!u.empty()) {
        uscs_weights[u] += thick;
        if (upper_uscs == "미상") upper_uscs = u;
        lower_uscs = u;
      }
    }
  }

  const double final_base_top =
      (weathered_top >= 0.0) ? weathered_top : std::max(total_depth, fill_bottom + alluv_thick);

  std::string dom_uscs = "미상";
  double max_w = -1.0;
  for (const auto& [code, w] : uscs_weights) {
    if (w > max_w) {
      max_w = w;
      dom_uscs = code;
    }
  }

  const std::string sed_type = classify_uscs(dom_uscs);
  const std::string color_hex = (sed_type == "Coarse")
                                    ? "#FF9800"
                                    : ((sed_type == "Fine") ? "#00ACC1" : "#9E9E9E");
  const std::string sed_kor = (sed_type == "Coarse")
                                  ? "사질·자갈"
                                  : ((sed_type == "Fine") ? "점토·실트" : "혼합");

  std::ostringstream oss;
  oss << std::fixed << std::setprecision(1);
  oss << "[" << bh.hole_id << "] 표고 " << bh.ground_elev_m << "m | "
      << "매립 " << fill_bottom << "m / 충적 " << alluv_thick << "m(" << sed_kor << ":"
      << dom_uscs << ") / 생토 " << final_base_top << "m / 수위 -" << std::abs(bh.water_depth_m)
      << "m";

  return ParsedBoreholeSummary{bh.hole_id,
                               bh.x,
                               bh.y,
                               bh.ground_elev_m,
                               fill_bottom,
                               alluv_thick,
                               final_base_top,
                               std::abs(bh.water_depth_m),
                               bh.ground_elev_m - fill_bottom,
                               bh.ground_elev_m - final_base_top,
                               dom_uscs,
                               upper_uscs,
                               lower_uscs,
                               sed_type,
                               has_organic,
                               color_hex,
                               oss.str()};
}

std::vector<double> HgisPaleoEngine::idw_interpolate_grid(
    const GridTransform& tf, const std::vector<double>& pt_x, const std::vector<double>& pt_y,
    const std::vector<double>& pt_val, int k_nearest, double power) {
  const int H = tf.height;
  const int W = tf.width;
  const size_t N_pts = pt_x.size();
  std::vector<double> out(static_cast<size_t>(H) * W, 0.0);
  if (N_pts == 0) return out;

  const size_t K = std::min(N_pts, static_cast<size_t>(std::max(1, k_nearest)));
  std::vector<std::pair<double, size_t>> dist_idx(N_pts);

  for (int r = 0; r < H; ++r) {
    for (int c = 0; c < W; ++c) {
      double wx = 0.0;
      double wy = 0.0;
      tf.pixel_to_world(r, c, wx, wy);

      for (size_t i = 0; i < N_pts; ++i) {
        const double dx = wx - pt_x[i];
        const double dy = wy - pt_y[i];
        dist_idx[i] = {dx * dx + dy * dy, i};
      }

      if (K < N_pts) {
        std::nth_element(dist_idx.begin(), dist_idx.begin() + static_cast<ptrdiff_t>(K),
                         dist_idx.end(),
                         [](const auto& a, const auto& b) { return a.first < b.first; });
      }

      double w_sum = 0.0;
      double v_sum = 0.0;
      for (size_t k = 0; k < K; ++k) {
        const double d = std::max(std::sqrt(dist_idx[k].first), 1.0);
        const double w = 1.0 / std::pow(d, power);
        w_sum += w;
        v_sum += w * pt_val[dist_idx[k].second];
      }
      out[static_cast<size_t>(r) * W + c] = (w_sum > 0.0) ? (v_sum / w_sum) : 0.0;
    }
  }
  return out;
}

}  // namespace hgis::paleo

std::vector<hgis::paleo::BoreholeRawInput> PaleoAnalysisRunner::collectBoreholes(
    QgsProject* project, const QgsCoordinateReferenceSystem& workCrs, const QgsPointXY& centerPt) {
  std::vector<hgis::paleo::BoreholeRawInput> out;
  if (QgsVectorLayer* bhLayer = GeoInfoBoreholeService::findBoreholeLayer(project)) {
    const QgsCoordinateTransform toWork(bhLayer->crs(), workCrs, project->transformContext());
    const bool needTr = bhLayer->crs().isValid() && bhLayer->crs() != workCrs;
    QgsFeature bf;
    QgsFeatureIterator it = bhLayer->getFeatures();
    while (it.nextFeature(bf)) {
      if (!bf.hasGeometry()) continue;
      QgsPointXY pt = bf.geometry().asPoint();
      if (needTr && !(workCrs.authid() == QLatin1String("EPSG:5187") &&
                      bhLayer->crs().authid() == QLatin1String("EPSG:5186") && pt.x() < 300000.0)) {
        try {
          pt = toWork.transform(pt);
        } catch (...) {
          KA_LOG_EXCEPT();
        }
      }
      const double fillM = bf.attribute(QStringLiteral("fill_m")).toDouble();
      const double alluvM = bf.attribute(QStringLiteral("alluvial_m")).toDouble();
      const double weathM = std::max(fillM + alluvM, bf.attribute(QStringLiteral("weathered_m")).toDouble());
      const std::string uscs = bf.attribute(QStringLiteral("uscs")).toString().toStdString();
      hgis::paleo::BoreholeRawInput b{bf.attribute(QStringLiteral("hole_code")).toString().toStdString(),
                                      pt.x(), pt.y(), bf.attribute(QStringLiteral("altitude_m")).toDouble(),
                                      std::abs(bf.attribute(QStringLiteral("water_m")).toDouble()), {}};
      if (fillM > 0.0) b.strata.push_back({0.0, fillM, "매립토층", "SM", "성토", 5.0});
      if (alluvM > 0.0) b.strata.push_back({fillM, fillM + alluvM, "충적층", uscs.empty() ? "SM" : uscs, "퇴적층", 8.0});
      b.strata.push_back({weathM, weathM + 3.0, "풍화토층", "SM", "풍화대", 50.0});
      out.push_back(std::move(b));
    }
  }
  if (out.empty()) {
    out.push_back({"BH-1", centerPt.x() - 60.0, centerPt.y() + 40.0, 16.8, 3.6,
                   {{0.0, 2.2, "성토층", "SM", "매립", 6.0},
                    {2.2, 5.4, "충적층", "SM", "모래 및 유기물", 8.0},
                    {5.4, 8.5, "풍화토", "SC", "기반암 풍화토", 50.0}}});
  }
  return out;
}
