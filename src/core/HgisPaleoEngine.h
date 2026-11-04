#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hgis::paleo {

enum class PaleoClass : uint8_t {
  Unclassified = 0,
  NaturalLevee = 1,       // 자연제방 (충적 미고지) - 주황색 (#FF9800)
  PaleochannelSwamp = 2,  // 구하도·배후습지 (충적 미저지) - 청록색 (#00ACC1)
  ValleyAlluvialFan = 3,  // 곡간충적지·선상지 - 연두색 (#8BC34A)
  TerraceT1 = 4,          // 하안단구 T1 (저위단구, 비고 5~15m) - 황갈색 (#FBC02D)
  TerraceT2T3 = 5,        // 하안단구 T2·T3 (중·고위단구, 비고 15m~) - 적갈색 (#8D6E63)
  HillslopeGentle = 6,    // 구릉 완사면 (사면 주거·분묘대, 경사 2~15°) - 연보라 (#AB47BC)
  SteepMountain = 7       // 급경사 산지 (유적 희박, 경사 15° 이상) - 회색 (#9E9E9E)
};

struct PaleoClassMeta {
  PaleoClass code;
  const char* name_kor;
  const char* color_hex;
  std::array<uint8_t, 4> rgba;
  const char* arch_potential;
};

inline constexpr std::array<PaleoClassMeta, 8> kPaleoClassTable = {{
    {PaleoClass::Unclassified, "미분류", "#000000", {0, 0, 0, 0}, "-"},
    {PaleoClass::NaturalLevee, "자연제방(충적 미고지)", "#FF9800", {255, 152, 0, 190},
     "취락·주거지·밭 유구 유망 (홍수 안전 미고지)"},
    {PaleoClass::PaleochannelSwamp, "구하도·배후습지(충적 미저지)", "#00ACC1", {0, 172, 193, 190},
     "고대 논(수전)·저습지·수침목재 유망 (지하수위 높음)"},
    {PaleoClass::ValleyAlluvialFan, "곡간충적지·선상지", "#8BC34A", {139, 195, 74, 190},
     "선상지 단부 주거지 / 곡간 논·밭 경작 유구 유망"},
    {PaleoClass::TerraceT1, "하안단구 T1(저위단구)", "#FBC02D", {251, 192, 45, 190},
     "청동기~역사시대 대규모 취락·분묘 유망"},
    {PaleoClass::TerraceT2T3, "하안단구 T2·T3(중·고위단구)", "#8D6E63", {141, 110, 99, 190},
     "구석기 유물포함층(고토양층) 및 구릉성 유적 유망"},
    {PaleoClass::HillslopeGentle, "구릉 완사면(사면 주거·분묘대)", "#AB47BC", {171, 71, 188, 175},
     "구릉성 주거지(2~7°) 및 고분·분묘(7~15°) 유망"},
    {PaleoClass::SteepMountain, "급경사 산지(유적 희박)", "#9E9E9E", {158, 158, 158, 120},
     "생활유적 입지 부적합 (산성·봉수·관방 유적만 예외 검토)"},
}};

enum class SoilLandformCode : uint8_t {
  ValleyFan = 4,        // 04 곡간 및 선상지
  MarinePlain = 5,      // 05 해성평탄지
  FluvialPlain = 6,     // 06 하성평탄지
  DiluvialTerrace = 8,  // 08 홍적대지(단구)
  Hillslope = 10,       // 산록경사지·구릉지
  Mountain = 12         // 급경사 산림지
};

enum class GeologyCode : uint8_t {
  QaAlluvium = 1,   // Qa: 제4기 충적층
  QrTerrace = 2,    // Qr: 제4기 하안단구층
  QbaBasalt = 3,    // Qba: 제4기 현무암(용암대지)
  Bedrock = 4,      // 중생대·선캄브리아기 기반암
  FaultLine = 5     // 단층 파쇄대 및 구조곡
};

struct StratumLayer {
  double depth_from_m = 0.0;
  double depth_to_m = 0.0;
  std::string layer_name;
  std::string uscs_code;
  std::string description;
  double n_value = 0.0;
};

struct BoreholeRawInput {
  std::string hole_id;
  double x = 0.0;
  double y = 0.0;
  double ground_elev_m = 0.0;
  double water_depth_m = 0.0;
  std::vector<StratumLayer> strata;
};

struct ParsedBoreholeSummary {
  std::string hole_id;
  double x = 0.0;
  double y = 0.0;
  double ground_elev_m = 0.0;
  double fill_thickness_m = 0.0;
  double alluvial_thickness_m = 0.0;
  double weathered_top_depth_m = 0.0;
  double water_depth_m = 0.0;
  double paleo_surface_elev_m = 0.0;
  double weathered_top_elev_m = 0.0;
  std::string dominant_uscs;
  std::string upper_alluv_uscs;
  std::string lower_alluv_uscs;
  std::string sed_type;
  bool has_organic_inclusion = false;
  std::string marker_color_hex;
  std::string map_label;
};

struct GridTransform {
  double origin_x = 0.0;
  double origin_y = 0.0;
  double cell_size = 5.0;
  int width = 0;
  int height = 0;

  void pixel_to_world(int r, int c, double& wx, double& wy) const noexcept {
    wx = origin_x + (c + 0.5) * cell_size;
    wy = origin_y - (r + 0.5) * cell_size;
  }
};

struct CrossSectionProfile {
  std::vector<double> dist_m;
  std::vector<double> z_surface_curr_m;
  std::vector<double> z_paleo_top_m;
  std::vector<double> z_bedrock_base_m;
  std::vector<double> z_river_level_m;
};

struct PaleoAnalysisResult {
  GridTransform transform;
  std::vector<double> z_paleo;
  std::vector<double> rem;
  std::vector<double> slope_deg;
  std::vector<double> tpi_50m;
  std::vector<double> dist_to_river_m;
  std::vector<double> multidirectional_hillshade;
  std::vector<double> fill_depth_grid;
  std::vector<double> base_depth_grid;
  std::vector<double> coarse_prob_grid;
  std::vector<PaleoClass> paleo_class_grid;
  std::vector<uint8_t> paleo_rgba_buffer;
  std::vector<ParsedBoreholeSummary> parsed_boreholes;
  CrossSectionProfile center_cross_section;
  std::string aoi_summary_text;
};

class HgisPaleoEngine {
public:
  static std::string to_upper(std::string_view sv);
  static bool contains(std::string_view haystack, std::string_view needle) noexcept;
  static std::string classify_uscs(std::string_view uscs_raw);
  static ParsedBoreholeSummary parse_single_borehole(const BoreholeRawInput& bh);
  static std::vector<double> idw_interpolate_grid(
      const GridTransform& tf, const std::vector<double>& pt_x, const std::vector<double>& pt_y,
      const std::vector<double>& pt_val, int k_nearest = 8, double power = 2.0);
  static void edt_1d(const std::vector<double>& f, std::vector<double>& d, int n);
  static std::vector<double> compute_river_distance_edt(const GridTransform& tf,
                                                        const std::vector<uint8_t>& river_mask);
  static std::vector<double> compute_tpi_integral_image(const GridTransform& tf,
                                                        const std::vector<double>& z_grid,
                                                        double radius_m = 50.0);
  static void compute_terrain_derivatives(const GridTransform& tf,
                                          const std::vector<double>& z_paleo,
                                          std::vector<double>& out_slope_deg,
                                          std::vector<double>& out_hillshade,
                                          std::vector<double>& out_twi);
  static PaleoAnalysisResult run_pipeline(
      const std::vector<double>& z_curr, const GridTransform& tf,
      const std::vector<BoreholeRawInput>& raw_boreholes,
      const std::vector<SoilLandformCode>& soil_grid, const std::vector<GeologyCode>& geol_grid,
      const std::vector<uint8_t>& river_mask, const std::vector<uint8_t>& aoi_mask,
      const std::pair<double, double>& aoi_center_xy,
      std::string_view nearest_river_name = "인접 하천",
      std::string_view dominant_soil_label = "06 하성평탄지",
      std::string_view dominant_geol_label = "Qa 제4기 충적층");
};

}  // namespace hgis::paleo
