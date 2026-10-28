#include "TrenchLShape.h"
#include "TrenchTerrain.h"

#include <cmath>
#include <limits>
#include <memory>
#include <type_traits>

#include <ogr_api.h>
#include <ogr_geometry.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace TrenchLShape {
namespace {

using TrenchGridGenerator::Cell;
using TrenchGridGenerator::RatioFill;
using TrenchGridGenerator::Spec;
using Point = std::pair<double, double>;

constexpr double kMaxWidth = 2.0;
constexpr double kMaxArm = 20.0;  // each arm keeps the 20 m trench limit
constexpr size_t kMaxCells = 200000;
constexpr double kTolerance = 1e-7;
constexpr double kMinGap = 1.0, kMaxGap = 120.0;  // gaps between L squares tried for the 10%/2% plan (m)

struct GeometryDeleter {
  void operator()(OGRGeometry* geometry) const { OGRGeometryFactory::destroyGeometry(geometry); }
};
using Geometry = std::unique_ptr<OGRGeometry, GeometryDeleter>;
struct PreparedDeleter {
  void operator()(OGRPreparedGeometryH prepared) const { OGRDestroyPreparedGeometry(prepared); }
};
using Prepared = std::unique_ptr<std::remove_pointer_t<OGRPreparedGeometryH>, PreparedDeleter>;

bool validShape(double width, double arm) {
  return std::isfinite(width) && width > 0.0 && width <= kMaxWidth && std::isfinite(arm) && arm > width &&
         arm <= kMaxArm;
}

bool validSpec(const Spec& spec) {
  return validShape(spec.trenchWidth, spec.armLength) && spec.armBend >= 0 && spec.armBend <= 3 &&
         std::isfinite(spec.balkWidth) && spec.balkWidth >= 0.0 && std::isfinite(spec.azimuthDeg) &&
         std::isfinite(spec.originX) && std::isfinite(spec.originY);
}

// The grid turned clockwise by the azimuth: local u = east, v = north at 0°.
struct Frame {
  double c = 1.0, s = 0.0, ox = 0.0, oy = 0.0;
  Frame(double azimuthDeg, double originX, double originY)
      : c(std::cos(azimuthDeg * M_PI / 180.0)), s(std::sin(azimuthDeg * M_PI / 180.0)), ox(originX), oy(originY) {}
  Point world(double u, double v) const { return {u * c + v * s + ox, -u * s + v * c + oy}; }
  Point local(double x, double y) const {
    const double dx = x - ox, dy = y - oy;
    return {dx * c - dy * s, dx * s + dy * c};
  }
  Point ex() const { return {c, -s}; }
  Point ey() const { return {s, c}; }
};

// Outer corner at `corner`, one arm along ex and the other along ey.
Cell lCell(const QString& name, Point corner, Point ex, Point ey, double arm, double width) {
  const auto at = [&](double u, double v) {
    return Point{corner.first + u * ex.first + v * ey.first, corner.second + u * ex.second + v * ey.second};
  };
  Cell cell;
  cell.name = name;
  cell.width = width;
  cell.arm = arm;
  cell.length = 2.0 * arm - width;
  cell.ring = {at(0, 0), at(arm, 0), at(arm, width), at(width, width), at(width, arm), at(0, arm), at(0, 0)};
  return cell;
}

// The L in the arm × arm square from (u0, v0), its outer corner in the square's corner the bend
// names: ㄴ south-west, ㄱ north-east, ㄴ mirrored south-east, ㄱ mirrored north-west.
Cell squareL(const QString& name, const Frame& frame, double u0, double v0, double arm, double width, int bend) {
  const bool east = bend == 1 || bend == 2;   // the across arm runs back west
  const bool north = bend == 1 || bend == 3;  // the along arm runs back south
  const Point ex = frame.ex(), ey = frame.ey();
  return lCell(name, frame.world(east ? u0 + arm : u0, north ? v0 + arm : v0),
               east ? Point{-ex.first, -ex.second} : ex, north ? Point{-ey.first, -ey.second} : ey, arm, width);
}

OGRPolygon polygonOf(const Cell& cell) {
  OGRLinearRing ring;
  for (const Point& point : cell.ring) ring.addPoint(point.first, point.second);
  OGRPolygon polygon;
  polygon.addRing(&ring);
  return polygon;
}

// The area, prepared once, and its grid frame anchored like the straight fill: envelope centre,
// first square at the low corner of the rotated envelope.
struct AreaGrid {
  Geometry area;
  Prepared prepared;
  Frame frame{0.0, 0.0, 0.0};
  double uMin = 0.0, uMax = 0.0, vMin = 0.0, vMax = 0.0;
};

AreaGrid areaGrid(OGRGeometry* area, double azimuthDeg) {
  AreaGrid grid;
  grid.area.reset(area);
  grid.prepared.reset(OGRCreatePreparedGeometry(OGRGeometry::ToHandle(area)));
  OGREnvelope env;
  area->getEnvelope(&env);
  grid.frame = Frame(azimuthDeg, (env.MinX + env.MaxX) * 0.5, (env.MinY + env.MaxY) * 0.5);
  bool first = true;
  for (double x : {env.MinX, env.MaxX}) {
    for (double y : {env.MinY, env.MaxY}) {
      const Point l = grid.frame.local(x, y);
      grid.uMin = first ? l.first : std::min(grid.uMin, l.first);
      grid.uMax = first ? l.first : std::max(grid.uMax, l.first);
      grid.vMin = first ? l.second : std::min(grid.vMin, l.second);
      grid.vMax = first ? l.second : std::max(grid.vMax, l.second);
      first = false;
    }
  }
  return grid;
}

// Whole Ls inside the area with these gaps; empty with *tooMany when the grid is too fine.
std::vector<Cell> fill(const AreaGrid& grid, double width, double arm, int bend, double gapU, double gapV,
                       const QString& prefix, bool* tooMany) {
  std::vector<Cell> out;
  *tooMany = false;
  const double stepU = arm + gapU, stepV = arm + gapV;
  const double countU = (grid.uMax - grid.uMin) / stepU + 1.0;
  const double countV = (grid.vMax - grid.vMin) / stepV + 1.0;
  if (!std::isfinite(countU) || !std::isfinite(countV) || countU * countV > double(kMaxCells)) {
    *tooMany = true;
    return out;
  }
  for (long long j = 0; j < static_cast<long long>(countV); ++j) {
    for (long long i = 0; i < static_cast<long long>(countU); ++i) {
      const double u0 = grid.uMin + i * stepU, v0 = grid.vMin + j * stepV;
      if (u0 + arm > grid.uMax || v0 + arm > grid.vMax) continue;  // outside the envelope
      Cell cell = squareL(prefix + QString::number(out.size() + 1), grid.frame, u0, v0, arm, width, bend);
      OGRPolygon polygon = polygonOf(cell);
      if (!OGRPreparedGeometryContains(grid.prepared.get(), OGRGeometry::ToHandle(&polygon))) continue;
      if (validCell(cell)) out.push_back(std::move(cell));
    }
  }
  return out;
}

}  // namespace

bool validCell(const Cell& cell) {
  const double w = cell.width, a = cell.arm;
  if (cell.ring.size() != 7 || !validShape(w, a) || std::abs(cell.length - (2.0 * a - w)) > kTolerance) return false;
  for (const Point& point : cell.ring)
    if (!std::isfinite(point.first) || !std::isfinite(point.second)) return false;
  const auto& p = cell.ring;
  if (std::hypot(p[6].first - p[0].first, p[6].second - p[0].second) > kTolerance) return false;
  // Edges outer arm, end, inner arm, inner arm, end, outer arm; all corners square, and only
  // the inner corner (between edges 2 and 3) turns the other way.
  const double expected[6] = {a, w, a - w, a - w, w, a};
  Point unit[6];
  for (int i = 0; i < 6; ++i) {
    const double dx = p[i + 1].first - p[i].first, dy = p[i + 1].second - p[i].second;
    const double length = std::hypot(dx, dy);
    if (std::abs(length - expected[i]) > kTolerance) return false;
    unit[i] = {dx / length, dy / length};
  }
  // Dot (square corner) or cross (which way it turns) of edge i and the next one.
  const auto corner = [&](int i, bool dot) {
    const Point& d = unit[i];
    const Point& e = unit[(i + 1) % 6];
    return dot ? d.first * e.first + d.second * e.second : d.first * e.second - d.second * e.first;
  };
  const double turn = corner(0, false) > 0.0 ? 1.0 : -1.0;
  for (int i = 0; i < 6; ++i)
    if (std::abs(corner(i, true)) > kTolerance || std::abs(corner(i, false) - (i == 2 ? -turn : turn)) > kTolerance)
      return false;
  return true;
}

std::vector<Cell> build(const Spec& spec) {
  std::vector<Cell> out;
  if (!validSpec(spec) || spec.rows <= 0 || spec.cols <= 0 ||
      static_cast<size_t>(spec.rows) > kMaxCells / static_cast<size_t>(spec.cols))
    return out;
  const Frame frame(spec.azimuthDeg, spec.originX, spec.originY);
  const double step = spec.armLength + spec.balkWidth;
  out.reserve(static_cast<size_t>(spec.rows) * spec.cols);
  for (int r = 0; r < spec.rows; ++r) {
    for (int col = 0; col < spec.cols; ++col) {
      Cell cell = squareL(spec.namePrefix + QString::number(out.size() + 1), frame, col * step, r * step,
                          spec.armLength, spec.trenchWidth, spec.armBend);
      if (!validCell(cell)) return {};
      out.push_back(std::move(cell));
    }
  }
  return out;
}

std::vector<Cell> buildInArea(const Spec& spec, const QByteArray& areaWkb, QString* reasonOut) {
  if (reasonOut) reasonOut->clear();
  const auto fail = [reasonOut](const QString& why) {
    if (reasonOut) *reasonOut = why;
    return std::vector<Cell>{};
  };
  if (const QString problem = TrenchGridGenerator::areaProblem(areaWkb); !problem.isEmpty()) return fail(problem);
  if (!validSpec(spec))
    return fail(QStringLiteral("ㄱ·ㄴ자 트렌치 규격이 맞지 않습니다. 폭은 2 m 이하, 팔 길이는 폭보다 길고 20 m 이하, "
                               "둑은 0 m 이상이어야 합니다."));
  OGRGeometry* area = nullptr;
  OGRGeometryFactory::createFromWkb(areaWkb.constData(), nullptr, &area, static_cast<size_t>(areaWkb.size()));
  const AreaGrid grid = areaGrid(area, spec.azimuthDeg);
  bool tooMany = false;
  std::vector<Cell> out = fill(grid, spec.trenchWidth, spec.armLength, spec.armBend, spec.balkWidth, spec.balkWidth,
                               spec.namePrefix, &tooMany);
  if (tooMany)
    return fail(QStringLiteral("구역에 비해 트렌치 간격이 좁아 칸 수가 한도를 넘습니다. 둑(간격)을 키우세요."));
  if (out.empty())
    return fail(QStringLiteral("현재 팔 길이와 방향으로는 구역 안에 온전히 들어가는 ㄱ·ㄴ자 트렌치가 없습니다. 구역이 "
                               "좁으면 회전이나 둑을 바꿔 보세요."));
  TrenchTerrain::numberFromHighSide(out, spec.azimuthDeg, spec.namePrefix);
  return out;
}

RatioFill buildForTargetRatio(const QByteArray& areaWkb, double targetPct, double width, double azimuthDeg,
                              double armLength, int armBend) {
  RatioFill best;
  if (!validShape(width, armLength) || armBend < 0 || armBend > 3 || !std::isfinite(targetPct) || targetPct <= 0.0 ||
      targetPct > 100.0 || !std::isfinite(azimuthDeg)) {
    best.error = QStringLiteral("폭은 0 초과 2 m 이하, 팔 길이는 폭보다 길고 20 m 이하, 목표 비율은 0 초과 100% 이하의 "
                                "유효한 값이어야 합니다.");
    return best;
  }
  if (const QString problem = TrenchGridGenerator::areaProblem(areaWkb); !problem.isEmpty()) {
    best.error = problem;
    return best;
  }
  OGRGeometry* area = nullptr;
  OGRGeometryFactory::createFromWkb(areaWkb.constData(), nullptr, &area, static_cast<size_t>(areaWkb.size()));
  const AreaGrid grid = areaGrid(area, azimuthDeg);
  const double areaM2 = OGR_G_Area(OGRGeometry::ToHandle(grid.area.get()));
  const double targetArea = areaM2 * (targetPct / 100.0);
  if (!std::isfinite(areaM2) || !(areaM2 > 1e-6) || !(targetArea > 0.0)) {
    best.error = QStringLiteral("조사구역 면적과 목표 면적을 계산하지 못했습니다.");
    return best;
  }
  best.areaM2 = areaM2;
  const QString prefix = targetPct <= 3.5 ? QStringLiteral("Sp-") : QStringLiteral("Tr-");
  const double oneL = width * (2.0 * armLength - width);

  // Each direction takes its own gap, so the count is not stuck at squares (8×8, 9×9). Wider gaps
  // only remove Ls: once a pair falls short, wider ones are not tried. Of pairs giving the same
  // count the widest is kept, so the Ls spread over the area instead of crowding one side.
  double bestArea = std::numeric_limits<double>::infinity();
  double bestSpread = 0.0;
  for (double gapU = kMinGap; gapU <= kMaxGap; gapU += 1.0) {
    bool reached = false;
    for (double gapV = kMinGap; gapV <= kMaxGap; gapV += 1.0) {
      bool tooMany = false;
      std::vector<Cell> cells = fill(grid, width, armLength, armBend, gapU, gapV, prefix, &tooMany);
      if (tooMany) continue;
      const double candidateArea = double(cells.size()) * oneL;
      if (candidateArea < targetArea) break;
      reached = true;
      const bool fewer = candidateArea < bestArea - oneL * 0.5;
      if (fewer || (candidateArea < bestArea + oneL * 0.5 && gapU + gapV > bestSpread)) {
        bestArea = candidateArea;
        bestSpread = gapU + gapV;
        best.cells = std::move(cells);
        best.balk = std::min(gapU, gapV);
      }
    }
    if (!reached) break;  // even the narrowest gap the other way falls short
  }
  if (best.cells.empty()) {
    best.error = QStringLiteral("현재 구역과 방향에서는 팔 %1 m ㄱ·ㄴ자 트렌치로 목표 %2%를 채울 수 없습니다. "
                                "기존 격자는 유지됩니다.").arg(armLength).arg(targetPct);
    return best;
  }
  TrenchTerrain::numberFromHighSide(best.cells, azimuthDeg, prefix);
  best.arm = armLength;
  best.length = best.cells.front().length;
  best.azimuthDeg = azimuthDeg;
  best.ratioPct = TrenchGridGenerator::totalArea(best.cells) / areaM2 * 100.0;
  return best;
}

}  // namespace TrenchLShape

// Whole trenches: within less than the smallest of the target (TrenchGridGenerator.cpp is at its line limit).
bool TrenchGridGenerator::meetsTarget(const std::vector<Cell>& cells, double areaM2, double targetPct) {
  if (targetPct <= 0.0) return !cells.empty();
  if (cells.empty() || !(areaM2 > 0.0) || !std::isfinite(areaM2)) return false;
  const double over = totalArea(cells) - areaM2 * targetPct / 100.0;
  double smallest = std::numeric_limits<double>::infinity();
  for (const Cell& cell : cells) smallest = std::min(smallest, cell.width * cell.length);
  return std::abs(over) < smallest;
}
