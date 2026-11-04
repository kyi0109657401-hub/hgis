#pragma once

// Logical identity of reference and background layers (evaluation F035).
//
// Survey domain layers are identified by ka_hgis/layer_key. Reference layers the app
// creates (satellite, DEM, terrain map, geology, soil …) carry ka_hgis/reference_kind,
// written once at creation. Toggles and lookups use the kind, so renaming a layer in the
// legend does not break its ribbon button, and a user layer that merely has a similar
// name ("위성사진_판독") is never switched, replaced or pruned with ours.
// Layers saved before the tag existed are found by their old titles (legacy fallback).
// Header-only so any ka_core/app file can use it without a CMake change.

#include "LayerOps.h"
#include "LayerOpsInternal.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <qgsmaplayer.h>
#include <qgsproject.h>

namespace ReferenceKind {

inline constexpr const char* kProperty = "ka_hgis/reference_kind";

inline constexpr const char* kVworldBase = "vworld_base";
inline constexpr const char* kSatellite = "satellite";        // VWorld or Google photo background
inline constexpr const char* kHybrid = "vworld_hybrid";
inline constexpr const char* kVworldContour = "vworld_contour";
inline constexpr const char* kCadastralPicture = "cadastral_picture";  // VWorld WMS picture, no snap
inline constexpr const char* kOsm = "osm";
inline constexpr const char* kCarto = "carto";
inline constexpr const char* kGoogleRoad = "google_road";
inline constexpr const char* kTerrain = "terrain";            // 지형맵 (OpenTopoMap)
inline constexpr const char* kDem = "dem";                    // elevation raster (DSM or NGII DEM)
inline constexpr const char* kDemRelief = "dem_relief";       // 지형 음영
inline constexpr const char* kGeology = "geology";
inline constexpr const char* kSoil = "soil";
inline constexpr const char* kRiver = "river";                // 수계도(하천망)
inline constexpr const char* kHistory1919 = "history_1919";
inline constexpr const char* kDaedongyeojido = "daedongyeojido";
inline constexpr const char* kTilePack = "tile_pack";         // offline MBTiles copy
inline constexpr const char* kAdminMask = "admin_mask";

inline QString of(const QgsMapLayer* layer) {
  return layer ? layer->customProperty(QString::fromLatin1(kProperty)).toString() : QString();
}

inline void tag(QgsMapLayer* layer, const QString& kind) {
  if (!layer || kind.isEmpty()) return;
  if (of(layer) != kind) layer->setCustomProperty(QString::fromLatin1(kProperty), kind);
}

// Titles the app gave each kind before tagging; also the titles callers pass to
// LayerOps::toggleLayerVisibility / isLayerVisible / setLayerOpacity.
inline QStringList legacyTitles(const QString& kind) {
  if (kind == QLatin1String(kVworldBase)) return {QStringLiteral("VWorld 배경")};
  if (kind == QLatin1String(kSatellite))
    return {QStringLiteral("VWorld 위성"), QStringLiteral("Google 위성"), QStringLiteral("위성")};
  if (kind == QLatin1String(kHybrid)) return {QStringLiteral("VWorld 하이브리드")};
  if (kind == QLatin1String(kVworldContour)) return {QStringLiteral("VWorld 등고선")};
  if (kind == QLatin1String(kOsm)) return {QStringLiteral("OSM")};
  if (kind == QLatin1String(kCarto)) return {QStringLiteral("Carto Light")};
  if (kind == QLatin1String(kGoogleRoad)) return {QStringLiteral("Google 도로")};
  if (kind == QLatin1String(kTerrain)) return {QStringLiteral("지형맵")};
  if (kind == QLatin1String(kDem)) return {QStringLiteral("DEM")};
  if (kind == QLatin1String(kDemRelief)) return {QStringLiteral("지형 음영")};
  if (kind == QLatin1String(kGeology)) return {QStringLiteral("지질도(KIGAM 1:5만)")};
  if (kind == QLatin1String(kSoil)) return {QStringLiteral("토양도(흙토람)")};
  if (kind == QLatin1String(kRiver)) return {QStringLiteral("수계도(하천망)")};
  if (kind == QLatin1String(kHistory1919)) return {QStringLiteral("1919 조선지형도 1:5만")};
  if (kind == QLatin1String(kDaedongyeojido)) return {QStringLiteral("대동여지도")};
  return {};
}

// Kind a caller's title refers to, or empty for an ordinary layer name.
// Cadastral titles are deliberately absent: "지적" can mean the downloaded
// cadastral map, and 본번/부번 pictures stay separate layers.
inline QString forTitle(const QString& title) {
  const QString base = kaStripLegendCrsSuffix(title.trimmed());
  if (base == QStringLiteral("지질도")) return QString::fromLatin1(kGeology);
  if (base == QStringLiteral("지표모델(DSM) 30m · Copernicus") ||
      base == QStringLiteral("[DEM] 국토지리원_DEM.tif"))
    return QString::fromLatin1(kDem);
  for (const char* kind : {kVworldBase, kSatellite, kHybrid, kVworldContour, kOsm, kCarto,
                           kGoogleRoad, kTerrain, kDem, kDemRelief, kGeology, kSoil, kRiver,
                           kHistory1919, kDaedongyeojido}) {
    for (const QString& legacy : legacyTitles(QString::fromLatin1(kind))) {
      if (base == legacy) return QString::fromLatin1(kind);
    }
  }
  return {};
}

// An untagged layer from an older project that used this kind's title.
// Survey domain layers (layer_key) are never reference layers. The title must be the same
// (apart from the " [EPSG:…]" suffix); the folded match ("…위성…" == "위성") is accepted only
// for live tile/WMS layers, so a user raster such as "위성사진_판독" is never taken for ours.
inline bool matchesLegacy(const QgsMapLayer* layer, const QString& kind) {
  if (!layer || !of(layer).isEmpty() || !LayerOps::layerKeyOf(layer).isEmpty()) return false;
  const QString name = layer->name();
  const bool liveTiles = LayerOps::isBasemapLayer(layer);
  for (const QString& legacy : legacyTitles(kind)) {
    if (legendTitlesMatchDirect(name, legacy)) return true;
    if (liveTiles && legendTitlesMatch(name, legacy)) return true;
  }
  return false;
}

// Tagged with `kind`, or (older project) untagged and named like it.
inline bool is(const QgsMapLayer* layer, const QString& kind) {
  if (!layer || kind.isEmpty()) return false;
  const QString own = of(layer);
  return own.isEmpty() ? matchesLegacy(layer, kind) : own == kind;
}

inline QList<QgsMapLayer*> tagged(const QgsProject* project, const QString& kind) {
  QList<QgsMapLayer*> out;
  if (!project || kind.isEmpty()) return out;
  const auto layers = project->mapLayers();
  for (QgsMapLayer* l : layers) {
    if (l && of(l) == kind) out.append(l);
  }
  return out;
}

// Tagged layers of `kind`; only when none exist, untagged legacy layers.
inline QList<QgsMapLayer*> find(const QgsProject* project, const QString& kind) {
  QList<QgsMapLayer*> out = tagged(project, kind);
  if (!out.isEmpty() || !project) return out;
  const auto layers = project->mapLayers();
  for (QgsMapLayer* l : layers) {
    if (matchesLegacy(l, kind)) out.append(l);
  }
  return out;
}

inline QgsMapLayer* first(const QgsProject* project, const QString& kind) {
  const QList<QgsMapLayer*> found = find(project, kind);
  for (QgsMapLayer* l : found) {
    if (l && l->isValid()) return l;
  }
  return found.isEmpty() ? nullptr : found.first();
}

}  // namespace ReferenceKind
