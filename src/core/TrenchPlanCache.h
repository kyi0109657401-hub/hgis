#pragma once

#include <QByteArray>

#include "TrenchGridGenerator.h"
#include "TrenchGridSnap.h"

// Remembers the last few 10%/2% trench plans. buildForTargetRatio searches up to
// 300 fills with GEOS per cell; the trench dialog and the apply path ask for the
// same (area, target, width, azimuth) several times in a row. The result is the
// exact same RatioFill the generator returns, so the exact-area contract holds.
// Thread-safe: the dialog computes on a worker thread.
namespace TrenchPlanCache {

// Cached or freshly computed TrenchGridGenerator::buildForTargetRatio result.
TrenchGridGenerator::RatioFill ratioPlan(const QByteArray& areaWkb, double targetPct,
                                         double width = 2.0, double azimuthDeg = 0.0,
                                         double armLength = 0.0, int armBend = 0,
                                         const TrenchGridSnap::Grid* grid = nullptr, double trenchLength = 20.0);

// True and *out filled when the plan is already known; never computes.
bool lookup(const QByteArray& areaWkb, double targetPct, double width, double azimuthDeg,
            TrenchGridGenerator::RatioFill* out, double armLength = 0.0, int armBend = 0,
            const TrenchGridSnap::Grid* grid = nullptr, double trenchLength = 20.0);

// Number of real searches run so far (not cache hits). For tests and logs.
int computeCount();

void clear();

}  // namespace TrenchPlanCache
