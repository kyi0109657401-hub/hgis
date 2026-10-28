#include "TrenchPlanCache.h"

#include <QCryptographicHash>
#include <QList>
#include <QMutex>
#include <QMutexLocker>

#include <atomic>

namespace TrenchPlanCache {
namespace {

// A few areas x two targets x a handful of azimuths is all the dialog cycles through.
constexpr int kMaxEntries = 16;

struct Key {
  QByteArray areaHash;
  double targetPct = 0.0;
  double width = 0.0;
  double azimuthDeg = 0.0;
  double armLength = 0.0;  // ㄱ·ㄴ자 plans are not the straight plan for the same area
  int armBend = 0;
  double trenchLength = 20.0;  // straight trenches: one length (20, 15 or 10 m)
  bool onGrid = false;  // 「좌표격자에 맞추기」: the coordinate grid below, not azimuthDeg
  TrenchGridSnap::Grid grid;
  bool operator==(const Key& other) const {
    return areaHash == other.areaHash && targetPct == other.targetPct && width == other.width &&
           azimuthDeg == other.azimuthDeg && armLength == other.armLength && armBend == other.armBend &&
           trenchLength == other.trenchLength &&
           onGrid == other.onGrid && grid.originX == other.grid.originX && grid.originY == other.grid.originY &&
           grid.rotationDeg == other.grid.rotationDeg && grid.step == other.grid.step;
  }
};

struct Entry {
  Key key;
  TrenchGridGenerator::RatioFill plan;
};

QMutex& cacheMutex() {
  static QMutex mutex;
  return mutex;
}

// Newest first.
QList<Entry>& entries() {
  static QList<Entry> list;
  return list;
}

std::atomic<int> g_computeCount{0};

Key makeKey(const QByteArray& areaWkb, double targetPct, double width, double azimuthDeg, double armLength,
            int armBend, const TrenchGridSnap::Grid* grid, double trenchLength) {
  return Key{QCryptographicHash::hash(areaWkb, QCryptographicHash::Sha1), targetPct, width,
             grid ? 0.0 : azimuthDeg, armLength, armBend, trenchLength, grid != nullptr,
             grid ? *grid : TrenchGridSnap::Grid{}};
}

TrenchGridGenerator::RatioFill compute(const QByteArray& areaWkb, double targetPct, double width, double azimuthDeg,
                                       double armLength, int armBend, const TrenchGridSnap::Grid* grid,
                                       double trenchLength) {
  if (grid) return TrenchGridSnap::buildForTargetRatio(areaWkb, targetPct, width, armLength, armBend, *grid);
  return TrenchGridGenerator::buildForTargetRatio(areaWkb, targetPct, width, azimuthDeg, armLength, armBend,
                                                  trenchLength);
}

bool find(const Key& key, TrenchGridGenerator::RatioFill* out) {
  QMutexLocker lock(&cacheMutex());
  QList<Entry>& list = entries();
  for (qsizetype i = 0; i < list.size(); ++i) {
    if (!(list.at(i).key == key)) continue;
    if (out) *out = list.at(i).plan;
    if (i > 0) list.move(i, 0);
    return true;
  }
  return false;
}

}  // namespace

bool lookup(const QByteArray& areaWkb, double targetPct, double width, double azimuthDeg,
            TrenchGridGenerator::RatioFill* out, double armLength, int armBend, const TrenchGridSnap::Grid* grid,
            double trenchLength) {
  if (areaWkb.isEmpty()) return false;
  return find(makeKey(areaWkb, targetPct, width, azimuthDeg, armLength, armBend, grid, trenchLength), out);
}

TrenchGridGenerator::RatioFill ratioPlan(const QByteArray& areaWkb, double targetPct, double width,
                                         double azimuthDeg, double armLength, int armBend,
                                         const TrenchGridSnap::Grid* grid, double trenchLength) {
  if (areaWkb.isEmpty())
    return compute(areaWkb, targetPct, width, azimuthDeg, armLength, armBend, grid, trenchLength);
  const Key key = makeKey(areaWkb, targetPct, width, azimuthDeg, armLength, armBend, grid, trenchLength);
  TrenchGridGenerator::RatioFill plan;
  if (find(key, &plan)) return plan;
  // Computed outside the lock: two threads asking for the same new plan both search once.
  plan = compute(areaWkb, targetPct, width, azimuthDeg, armLength, armBend, grid, trenchLength);
  ++g_computeCount;
  QMutexLocker lock(&cacheMutex());
  QList<Entry>& list = entries();
  list.removeIf([&key](const Entry& entry) { return entry.key == key; });
  list.prepend(Entry{key, plan});
  while (list.size() > kMaxEntries) list.removeLast();
  return plan;
}

int computeCount() { return g_computeCount.load(); }

void clear() {
  QMutexLocker lock(&cacheMutex());
  entries().clear();
  g_computeCount = 0;
}

}  // namespace TrenchPlanCache
