// src/ui/widgets/WorldMapWidget.cpp
//
// A self-contained slippy map. Coordinates: "world" = normalised Web-Mercator
// (0..1 on both axes, see map/GeoBaseMap.h); the world repeats horizontally so
// the view can pan across the antimeridian. Layers, bottom to top:
//   offline base tiles (Natural Earth, themed)  ─ always
//   NASA GIBS imagery + coast/border lines       ─ SATELLITE / NIGHT LIGHTS
//   country / sea labels, reference layers (ports, chokepoints)
//   drawn shapes, voyage track, pins / clusters, chrome (zoom, scale, attribution, status)
#include "ui/widgets/WorldMapWidget.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/map/GeoBaseMap.h"
#include "ui/widgets/map/ImageryTiles.h"

#include <QEasingCurve>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSet>
#include <QToolTip>
#include <QVariantAnimation>
#include <QWheelEvent>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <utility>

namespace fincept::ui {

// Helpers carry a wmw_ prefix: unity builds merge this file with other widget
// sources and an anonymous namespace does not isolate names there.
namespace {

// Deepest zoom (about 300 m per pixel). The bundled Natural Earth data and the
// NASA imagery (native to level 8) carry no detail beyond this: at 11 the map
// was one flat colour with no coast, border or label in sight, which read as a
// map that had not loaded — and a saved view could reopen there.
constexpr double kWmwMaxZoom = 9.0;
constexpr int kWmwClusterThreshold = 200; // interactive pins before clustering starts
constexpr double kWmwClusterCellPx = 44.0;

struct WmwBasemap {
    const char* label;
    int imagery; // index into map::imagery_sources(), -1 = offline base map only
};
constexpr WmwBasemap kWmwBasemaps[] = {{"MAP", -1}, {"SATELLITE", 0}, {"NIGHT LIGHTS", 1}};
constexpr int kWmwBasemapCount = static_cast<int>(sizeof(kWmwBasemaps) / sizeof(kWmwBasemaps[0]));

enum WmwLabelStyle {
    kWmwCountry = 0,
    kWmwSea,
    kWmwCountryOnImagery,
    kWmwSeaOnImagery,
    kWmwPort,
    kWmwChokepoint,
};

double wmw_haversine_km(double lat1, double lng1, double lat2, double lng2) {
    constexpr double kEarthR = 6371.0088;
    const double dlat = qDegreesToRadians(lat2 - lat1);
    const double dlng = qDegreesToRadians(lng2 - lng1);
    const double a = std::sin(dlat / 2) * std::sin(dlat / 2) + std::cos(qDegreesToRadians(lat1)) *
                                                                   std::cos(qDegreesToRadians(lat2)) *
                                                                   std::sin(dlng / 2) * std::sin(dlng / 2);
    return 2 * kEarthR * std::asin(std::min(1.0, std::sqrt(a)));
}

double wmw_wrap01(double x) {
    x = std::fmod(x, 1.0);
    return x < 0 ? x + 1.0 : x;
}

QColor wmw_alpha(QColor c, int a) {
    c.setAlpha(a);
    return c;
}

quint32 wmw_palette_key(std::initializer_list<QColor> colors, quint32 salt) {
    quint32 h = 2166136261u ^ salt;
    for (const QColor& c : colors) {
        h ^= c.rgba();
        h *= 16777619u;
    }
    return (h ^ (h >> 16)) & 0xffff;
}

/// "Nice" scale-bar length (1, 2, 5 x 10^n metres) not longer than `max_m`.
double wmw_nice_length(double max_m) {
    const double p = std::pow(10.0, std::floor(std::log10(std::max(max_m, 1.0))));
    for (double m : {5.0, 2.0, 1.0})
        if (m * p <= max_m)
            return m * p;
    return p;
}

} // namespace

// ── MapShape ─────────────────────────────────────────────────────────────────
bool MapShape::contains(double lat, double lng) const {
    if (type == Type::Rectangle)
        return lat >= min_lat && lat <= max_lat && lng >= min_lng && lng <= max_lng;
    return wmw_haversine_km(centre_lat, centre_lng, lat, lng) <= radius_km;
}

// ── Construction ─────────────────────────────────────────────────────────────

WorldMapWidget::WorldMapWidget(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(200, 150);
    setAccessibleName(tr("World map"));
    setAccessibleDescription(
        tr("Drag to pan, scroll or press + / - to zoom, arrow keys to move. Click a marker for its details; click a "
           "numbered cluster to zoom into it."));

    refresh_palette();

    settle_timer_.setSingleShot(true);
    settle_timer_.setInterval(180);
    connect(&settle_timer_, &QTimer::timeout, this, &WorldMapWidget::view_settled);
    repaint_timer_.setSingleShot(true);
    repaint_timer_.setInterval(30); // coalesce a burst of finished tiles into one repaint
    connect(&repaint_timer_, &QTimer::timeout, this, qOverload<>(&QWidget::update));

    auto& base = map::GeoBaseMap::instance();
    connect(&base, &map::GeoBaseMap::tile_ready, this, &WorldMapWidget::schedule_repaint);
    connect(&base, &map::GeoBaseMap::data_ready, this, &WorldMapWidget::schedule_repaint);
    base.data(); // starts loading the bundled data off the GUI thread

    auto& imagery = map::ImageryTiles::instance();
    connect(&imagery, &map::ImageryTiles::tile_ready, this, [this]() {
        if (imagery_source() >= 0)
            schedule_repaint();
    });
    connect(&imagery, &map::ImageryTiles::state_changed, this, [this]() {
        if (imagery_source() >= 0)
            schedule_repaint();
    });

    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this, [this](const ThemeTokens&) {
        refresh_palette();
        update();
    });
}

WorldMapWidget::~WorldMapWidget() {
    map::ImageryTiles::instance().release(this);
}

void WorldMapWidget::schedule_repaint() {
    if (isVisible() && !repaint_timer_.isActive())
        repaint_timer_.start();
}

void WorldMapWidget::refresh_palette() {
    const bool dark = colors::is_dark();
    // Sea: the base background washed with the info hue; land: the raised surface.
    pal_.water = colors::tint_color(QColor(colors::INFO()), dark ? 0.08 : 0.16);
    pal_.land = dark ? QColor(colors::BG_HOVER()) : QColor(colors::BG_SURFACE());
    pal_.coast = QColor(colors::BORDER_BRIGHT());
    pal_.border = dark ? QColor(colors::TEXT_DIM()) : QColor(colors::BORDER_BRIGHT());
    pal_.label = QColor(colors::TEXT_SECONDARY());
    pal_.label_sea = QColor(colors::TEXT_DIM());
    pal_.halo_land = wmw_alpha(pal_.land, 215);
    pal_.halo_sea = wmw_alpha(pal_.water, 215);
    pal_.accent = QColor(colors::AMBER());
    pal_.ring = QColor(colors::TEXT_PRIMARY());
    pal_.chrome_bg = wmw_alpha(QColor(colors::BG_RAISED()), 235);
    pal_.chrome_border = QColor(colors::BORDER_MED());
    pal_.chrome_text = QColor(colors::TEXT_PRIMARY());
    pal_.chrome_dim = QColor(colors::TEXT_TERTIARY());
    pal_.port = QColor(colors::CYAN());
    pal_.chokepoint = QColor(colors::WARNING());
    pal_.pin_outline = wmw_alpha(QColor(colors::BG_BASE()), dark ? 210 : 235);
    // Over satellite imagery (always dark) the dark preset's own text colours read best.
    pal_.over_imagery_line = wmw_alpha(QColor(THEME_OBSIDIAN.text_primary), 120);
    pal_.over_imagery_text = QColor(THEME_OBSIDIAN.text_primary);
    pal_.over_imagery_halo = wmw_alpha(QColor(THEME_OBSIDIAN.bg_base), 200);
    pal_.base_key = wmw_palette_key({pal_.water, pal_.land, pal_.coast, pal_.border}, 1);
    pal_.lines_key = wmw_palette_key({pal_.over_imagery_line}, 2) | 0x1; // never equal to a base key by chance
    if (pal_.lines_key == pal_.base_key)
        pal_.lines_key ^= 0x8000;
    pin_cache_.clear();
    label_cache_.clear();
}

// ── Camera ───────────────────────────────────────────────────────────────────

double WorldMapWidget::world_px() const {
    return map::kTilePx * std::pow(2.0, zoom_);
}

QPointF WorldMapWidget::world_to_screen(const QPointF& w) const {
    const double W = world_px();
    return {(w.x() - center_.x()) * W + width() / 2.0, (w.y() - center_.y()) * W + height() / 2.0};
}

QPointF WorldMapWidget::screen_to_world(const QPointF& s) const {
    const double W = world_px();
    return {center_.x() + (s.x() - width() / 2.0) / W, center_.y() + (s.y() - height() / 2.0) / W};
}

void WorldMapWidget::visible_copies(int* first, int* last) const {
    const double W = world_px();
    const double left = center_.x() - (width() / 2.0 + 40) / W;
    const double right = center_.x() + (width() / 2.0 + 40) / W;
    *first = static_cast<int>(std::floor(left)) - 1;
    *last = static_cast<int>(std::floor(right)) + 1;
}

double WorldMapWidget::min_zoom() const {
    // The world always fills the height, and is never much narrower than the view.
    const double by_height = std::log2(std::max(1, height()) / double(map::kTilePx));
    const double by_width = std::log2(std::max(1, width()) / double(map::kTilePx)) - 0.6;
    return std::clamp(std::max(by_height, by_width), 0.0, kWmwMaxZoom);
}

void WorldMapWidget::clamp_camera() {
    zoom_ = std::clamp(zoom_, min_zoom(), kWmwMaxZoom);
    const double half_h = (height() / 2.0) / world_px();
    double cx = wmw_wrap01(center_.x());
    double cy = half_h >= 0.5 ? 0.5 : std::clamp(center_.y(), half_h, 1.0 - half_h);
    center_ = {cx, cy};
}

void WorldMapWidget::camera_moved(bool interactive) {
    clamp_camera();
    if (interactive)
        interacting_ = true;
    clear_hover();
    settle_timer_.start();
    update();
}

void WorldMapWidget::view_settled() {
    if (anim_ && anim_->state() == QAbstractAnimation::Running)
        return;
    interacting_ = false;
    request_imagery();
    update();
    emit view_changed();
}

void WorldMapWidget::zoom_at(const QPointF& anchor, double new_zoom, bool animate) {
    new_zoom = std::clamp(new_zoom, min_zoom(), kWmwMaxZoom);
    const QPointF w = screen_to_world(anchor);
    const double W1 = map::kTilePx * std::pow(2.0, new_zoom);
    const QPointF c(w.x() - (anchor.x() - width() / 2.0) / W1, w.y() - (anchor.y() - height() / 2.0) / W1);
    if (animate) {
        animate_to(c, new_zoom);
        return;
    }
    center_ = c;
    zoom_ = new_zoom;
    camera_moved(true);
}

void WorldMapWidget::animate_to(const QPointF& center, double zoom) {
    if (!isVisible() || width() <= 0) {
        center_ = center;
        zoom_ = zoom;
        clamp_camera();
        settle_timer_.start();
        update();
        return;
    }
    if (!anim_) {
        anim_ = new QVariantAnimation(this);
        anim_->setEasingCurve(QEasingCurve::OutCubic);
        connect(anim_, &QVariantAnimation::finished, this, &WorldMapWidget::view_settled);
    }
    anim_->stop();
    QObject::disconnect(anim_conn_);
    const QPointF c0 = center_;
    const double z0 = zoom_;
    double dx = center.x() - c0.x();
    dx -= std::round(dx); // shortest way round the world
    const QPointF d(dx, center.y() - c0.y());
    const double z1 = std::clamp(zoom, min_zoom(), kWmwMaxZoom);
    anim_->setDuration(std::clamp(static_cast<int>(250 + 90 * std::abs(z1 - z0)), 250, 650));
    anim_->setStartValue(0.0);
    anim_->setEndValue(1.0);
    anim_conn_ = connect(anim_, &QVariantAnimation::valueChanged, this, [this, c0, d, z0, z1](const QVariant& v) {
        const double t = v.toDouble();
        center_ = c0 + d * t;
        zoom_ = z0 + (z1 - z0) * t;
        clamp_camera();
        interacting_ = true;
        update();
    });
    clear_hover();
    anim_->start();
}

void WorldMapWidget::fit_world_rect(const QRectF& r, double max_zoom, bool animate) {
    const double w = std::max(1, width());
    const double h = std::max(1, height());
    const double zx = std::log2(w * 0.92 / (std::max(r.width(), 1e-9) * map::kTilePx));
    const double zy = std::log2(h * 0.92 / (std::max(r.height(), 1e-9) * map::kTilePx));
    const double z = std::clamp(std::min({zx, zy, max_zoom}), min_zoom(), kWmwMaxZoom);
    if (animate)
        animate_to(r.center(), z);
    else {
        center_ = r.center();
        zoom_ = z;
        camera_moved(false);
    }
}

void WorldMapWidget::apply_initial_view() {
    // The whole world across the width, centred on 18°N / 10°E (the latitudes a
    // wide, short map can show then hold nearly everything that is populated).
    center_ = map::geo_to_world(18.0, 10.0);
    zoom_ = std::log2(std::max(1, width()) / double(map::kTilePx));
    camera_moved(false);
}

bool WorldMapWidget::pixel_to_geo(const QPointF& px, double* lat, double* lng) const {
    const QPointF w = screen_to_world(px);
    map::world_to_geo(QPointF(wmw_wrap01(w.x()), std::clamp(w.y(), 0.0, 1.0)), lat, lng);
    return true;
}

void WorldMapWidget::fly_to(double lat, double lng, int zoom) {
    const QPointF w = map::geo_to_world(lat, lng);
    const double z = std::max<double>(zoom, std::min(zoom_, 9.0));
    if (!camera_initialised_) {
        center_ = w;
        zoom_ = z;
        view_preset_ = true;
        return;
    }
    animate_to(w, z);
}

void WorldMapWidget::fit_to_pins() {
    if (std::exchange(suppress_next_fit_, false))
        return;
    if (pins_.isEmpty())
        return;
    if (!camera_initialised_) {
        fit_pending_ = true;
        return;
    }
    do_fit_pins();
}

void WorldMapWidget::do_fit_pins() {
    double min_lat = 90, max_lat = -90, min_lng = 180, max_lng = -180;
    double min_lng_e = 360, max_lng_e = -360; // longitudes shifted to 0..360 (Pacific-centred)
    for (const auto& pin : pins_) {
        min_lat = std::min(min_lat, pin.latitude);
        max_lat = std::max(max_lat, pin.latitude);
        min_lng = std::min(min_lng, pin.longitude);
        max_lng = std::max(max_lng, pin.longitude);
        const double e = pin.longitude < 0 ? pin.longitude + 360.0 : pin.longitude;
        min_lng_e = std::min(min_lng_e, e);
        max_lng_e = std::max(max_lng_e, e);
    }
    // A fleet on both sides of the antimeridian spans less when measured 0..360.
    if (max_lng_e - min_lng_e < max_lng - min_lng) {
        min_lng = min_lng_e - 360.0;
        max_lng = max_lng_e - 360.0;
    }
    const double lat_span = max_lat - min_lat;
    const double lng_span = max_lng - min_lng;
    if (lng_span > 200.0 || lat_span > 110.0) {
        apply_initial_view();
        settle_timer_.start();
        return;
    }
    const double lat_pad = std::max(lat_span * 0.1, 1.5);
    const double lng_pad = std::max(lng_span * 0.1, 2.0);
    const QPointF tl = map::geo_to_world(std::min(85.0, max_lat + lat_pad), min_lng - lng_pad);
    const QPointF br = map::geo_to_world(std::max(-85.0, min_lat - lat_pad), max_lng + lng_pad);
    // geo_to_world does not wrap, so a box west of -180 keeps its true width.
    fit_world_rect(QRectF(tl, br).normalized(), 8.0, isVisible());
}

void WorldMapWidget::set_selected_pin(int id, bool reveal) {
    selected_id_ = id;
    if (reveal && id >= 0 && camera_initialised_) {
        for (int i = 0; i < pins_.size(); ++i) {
            if (pins_[i].id != id)
                continue;
            QPointF w = pin_world_[i];
            w.setX(w.x() + std::round(center_.x() - w.x())); // nearest copy
            const QPointF s = world_to_screen(w);
            if (!QRectF(rect()).adjusted(40, 40, -40, -40).contains(s))
                animate_to(w, std::max(zoom_, 3.0));
            break;
        }
    }
    update();
}

void WorldMapWidget::set_overlay_message(const QString& text) {
    if (overlay_message_ == text)
        return;
    overlay_message_ = text;
    update();
}

QVariantMap WorldMapWidget::view_state() const {
    QVariantMap s;
    s.insert(QStringLiteral("basemap"), basemap_index_);
    s.insert(QStringLiteral("layers"), layers_);
    if (camera_initialised_) {
        double lat = 0, lng = 0;
        map::world_to_geo(center_, &lat, &lng);
        s.insert(QStringLiteral("lat"), lat);
        s.insert(QStringLiteral("lng"), lng);
        s.insert(QStringLiteral("zoom"), zoom_);
    }
    return s;
}

void WorldMapWidget::restore_view_state(const QVariantMap& s) {
    if (s.contains(QStringLiteral("basemap")))
        set_basemap(s.value(QStringLiteral("basemap")).toInt());
    if (s.contains(QStringLiteral("layers")))
        layers_ = s.value(QStringLiteral("layers")).toInt() & 0x3;
    if (s.contains(QStringLiteral("lat")) && s.contains(QStringLiteral("zoom"))) {
        center_ = map::geo_to_world(s.value(QStringLiteral("lat")).toDouble(), s.value(QStringLiteral("lng")).toDouble());
        zoom_ = std::clamp(s.value(QStringLiteral("zoom")).toDouble(), 0.0, kWmwMaxZoom);
        if (camera_initialised_)
            clamp_camera();
        else
            view_preset_ = true; // kept (and clamped) when the widget first gets a size
        fit_pending_ = false;
        suppress_next_fit_ = true; // the first data load keeps the restored view
    }
    update();
}

// ── Content ──────────────────────────────────────────────────────────────────

QStringList WorldMapWidget::basemap_labels() {
    QStringList out;
    for (const auto& b : kWmwBasemaps)
        out << QString::fromLatin1(b.label);
    return out;
}

int WorldMapWidget::imagery_source() const {
    return basemap_index_ >= 0 && basemap_index_ < kWmwBasemapCount ? kWmwBasemaps[basemap_index_].imagery : -1;
}

void WorldMapWidget::set_basemap(int index) {
    if (index < 0 || index >= kWmwBasemapCount || index == basemap_index_)
        return;
    basemap_index_ = index;
    label_cache_.clear();
    if (imagery_source() < 0)
        map::ImageryTiles::instance().release(this);
    else
        request_imagery();
    update();
    emit view_changed();
}

void WorldMapWidget::set_reference_layer(MapLayer layer, bool on) {
    const int bit = static_cast<int>(layer);
    const int next = on ? (layers_ | bit) : (layers_ & ~bit);
    if (next == layers_)
        return;
    layers_ = next;
    update();
    emit view_changed();
}

void WorldMapWidget::rebuild_pin_world() {
    pin_world_.resize(pins_.size());
    clusterable_count_ = 0;
    for (int i = 0; i < pins_.size(); ++i) {
        pin_world_[i] = map::geo_to_world(pins_[i].latitude, pins_[i].longitude);
        if (pins_[i].id >= 0)
            ++clusterable_count_;
    }
    clusters_.clear();
    clear_hover();
}

void WorldMapWidget::set_pins(const QVector<MapPin>& pins) {
    pins_ = pins;
    rebuild_pin_world();
    // A fresh pin set implies a new context — drop any stale track polyline.
    // Callers that want a path (e.g. voyage history) call set_track_path after.
    track_world_.clear();
    update();
}

void WorldMapWidget::add_pin(const MapPin& pin) {
    pins_.append(pin);
    rebuild_pin_world();
    update();
}

void WorldMapWidget::clear_pins() {
    pins_.clear();
    rebuild_pin_world();
    update();
}

void WorldMapWidget::set_track_path(const QVector<QPointF>& latlng_points) {
    track_world_.clear();
    if (latlng_points.size() >= 2) {
        track_world_.reserve(latlng_points.size());
        double prev = latlng_points.first().y();
        double offset = 0;
        for (const QPointF& p : latlng_points) {
            // Unwrap across the antimeridian so the line does not cross the whole map.
            const double d = p.y() - prev;
            if (d > 180.0)
                offset -= 360.0;
            else if (d < -180.0)
                offset += 360.0;
            prev = p.y();
            const QPointF w = map::geo_to_world(p.x(), 0.0);
            track_world_.append(QPointF((p.y() + offset + 180.0) / 360.0, w.y()));
        }
    }
    update();
}

void WorldMapWidget::clear_track_path() {
    if (track_world_.isEmpty())
        return;
    track_world_.clear();
    update();
}

const QVector<WorldMapWidget::Cluster>& WorldMapWidget::clusters_for_zoom(int zoom) {
    auto it = clusters_.constFind(zoom);
    if (it != clusters_.constEnd())
        return it.value();
    const double cell = kWmwClusterCellPx / (map::kTilePx * std::ldexp(1.0, zoom));
    QHash<qint64, int> by_cell;
    QVector<Cluster> out;
    QVector<QPointF> sums;
    QVector<QHash<QRgb, int>> colour_votes;
    for (int i = 0; i < pins_.size(); ++i) {
        if (pins_[i].id < 0)
            continue;
        const QPointF& w = pin_world_[i];
        const qint64 cx = static_cast<qint64>(std::floor(w.x() / cell));
        const qint64 cy = static_cast<qint64>(std::floor(w.y() / cell));
        const qint64 key = (cx << 32) ^ (cy & 0xffffffff);
        auto c = by_cell.find(key);
        int idx;
        if (c == by_cell.end()) {
            idx = out.size();
            by_cell.insert(key, idx);
            out.append(Cluster{});
            sums.append(QPointF());
            colour_votes.append(QHash<QRgb, int>());
        } else {
            idx = c.value();
        }
        out[idx].members.append(i);
        sums[idx] += w;
        colour_votes[idx][pins_[i].color.rgba()] += 1;
        out[idx].radius = std::max(out[idx].radius, pins_[i].radius);
    }
    for (int k = 0; k < out.size(); ++k) {
        out[k].world = sums[k] / out[k].members.size();
        int best = -1;
        for (auto v = colour_votes[k].constBegin(); v != colour_votes[k].constEnd(); ++v) {
            if (v.value() > best) {
                best = v.value();
                out[k].color = QColor::fromRgba(v.key());
            }
        }
    }
    return clusters_.insert(zoom, out).value();
}

// ── Painting ─────────────────────────────────────────────────────────────────

void WorldMapWidget::paintEvent(QPaintEvent*) {
    if (!camera_initialised_ && width() > 0 && height() > 0) {
        // Normally resizeEvent got here first; this covers a paint without one.
        camera_initialised_ = true;
        if (std::exchange(view_preset_, false))
            clamp_camera();
        else
            apply_initial_view();
    }
    QPainter p(this);
    p.fillRect(rect(), pal_.water);
    hits_.clear();
    draw_base(p, false);
    if (imagery_source() >= 0) {
        draw_imagery(p);
        draw_base(p, true); // coastlines + borders over the imagery
    }
    draw_labels(p);
    draw_reference_layers(p);
    draw_shapes(p);
    draw_track(p);
    draw_pins(p);
    draw_chrome(p);
}

void WorldMapWidget::draw_base(QPainter& p, bool lines_only) {
    auto& base = map::GeoBaseMap::instance();
    if (!base.data())
        return; // still loading: the sea colour stands in, the status pill says so
    const double W = world_px();
    const int tz = std::clamp(static_cast<int>(std::floor(zoom_ + 0.35)), 0, static_cast<int>(kWmwMaxZoom));
    const int n = 1 << tz;
    const double tile_px = W / n;
    const double left = center_.x() * W - width() / 2.0;
    const double top = center_.y() * W - height() / 2.0;
    const int tx0 = static_cast<int>(std::floor(left / tile_px));
    const int tx1 = static_cast<int>(std::floor((left + width()) / tile_px));
    const int ty0 = std::max(0, static_cast<int>(std::floor(top / tile_px)));
    const int ty1 = std::min(n - 1, static_cast<int>(std::floor((top + height()) / tile_px)));
    const qreal dpr = devicePixelRatioF();

    map::BaseMapPalette bp;
    if (lines_only) {
        bp.water = Qt::transparent;
        bp.land = Qt::transparent;
        bp.coast = pal_.over_imagery_line;
        bp.border = pal_.over_imagery_line;
        bp.lines_only = true;
        bp.key = pal_.lines_key;
    } else {
        bp.water = pal_.water;
        bp.land = pal_.land;
        bp.coast = pal_.coast;
        bp.border = pal_.border;
        bp.key = pal_.base_key;
    }

    struct TileSlot {
        int tx, ty;
        double dist;
    };
    QVector<TileSlot> todo;
    const double cx = (left + width() / 2.0) / tile_px - 0.5;
    const double cy = (top + height() / 2.0) / tile_px - 0.5;
    for (int ty = ty0; ty <= ty1; ++ty)
        for (int tx = tx0; tx <= tx1; ++tx)
            todo.append({tx, ty, std::hypot(tx - cx, ty - cy)});
    // Renders run newest-request-first: ask for the far tiles first so the
    // centre is drawn first.
    std::sort(todo.begin(), todo.end(), [](const TileSlot& a, const TileSlot& b) { return a.dist > b.dist; });

    p.setRenderHint(QPainter::SmoothPixmapTransform, !interacting_);
    for (const TileSlot& s : todo) {
        const int wx = ((s.tx % n) + n) % n;
        // Integer-aligned target so neighbouring tiles meet without seams.
        const int x0 = static_cast<int>(std::floor(s.tx * tile_px - left));
        const int x1 = static_cast<int>(std::floor((s.tx + 1) * tile_px - left));
        const int y0 = static_cast<int>(std::floor(s.ty * tile_px - top));
        const int y1 = static_cast<int>(std::floor((s.ty + 1) * tile_px - top));
        const QRect target(x0, y0, x1 - x0, y1 - y0);
        if (const QImage* img = base.tile(tz, wx, s.ty, bp, dpr, true)) {
            p.drawImage(target, *img);
            continue;
        }
        // Not rendered yet: stretch the nearest cached ancestor over the gap.
        for (int dz = 1; dz <= tz; ++dz) {
            const QImage* parent = base.tile(tz - dz, wx >> dz, s.ty >> dz, bp, dpr, false);
            if (!parent)
                continue;
            const double sub = parent->width() / double(1 << dz);
            const QRectF src((wx - ((wx >> dz) << dz)) * sub, (s.ty - ((s.ty >> dz) << dz)) * sub, sub, sub);
            p.drawImage(QRectF(target), *parent, src);
            break;
        }
    }
}

void WorldMapWidget::draw_imagery(QPainter& p) {
    const int source = imagery_source();
    const auto& src = map::imagery_sources()[source];
    auto& tiles = map::ImageryTiles::instance();
    const double W = world_px();
    const int iz = std::clamp(static_cast<int>(std::floor(zoom_ + 0.35)), 0, src.max_zoom);
    const int n = 1 << iz;
    const double tile_px = W / n;
    const double left = center_.x() * W - width() / 2.0;
    const double top = center_.y() * W - height() / 2.0;
    const int tx0 = static_cast<int>(std::floor(left / tile_px));
    const int tx1 = static_cast<int>(std::floor((left + width()) / tile_px));
    const int ty0 = std::max(0, static_cast<int>(std::floor(top / tile_px)));
    const int ty1 = std::min(n - 1, static_cast<int>(std::floor((top + height()) / tile_px)));
    p.setRenderHint(QPainter::SmoothPixmapTransform, !interacting_);
    for (int ty = ty0; ty <= ty1; ++ty) {
        for (int tx = tx0; tx <= tx1; ++tx) {
            const int wx = ((tx % n) + n) % n;
            const int x0 = static_cast<int>(std::floor(tx * tile_px - left));
            const int x1 = static_cast<int>(std::floor((tx + 1) * tile_px - left));
            const int y0 = static_cast<int>(std::floor(ty * tile_px - top));
            const int y1 = static_cast<int>(std::floor((ty + 1) * tile_px - top));
            const QRect target(x0, y0, x1 - x0, y1 - y0);
            if (const QImage* img = tiles.cached(source, iz, wx, ty)) {
                p.drawImage(target, *img);
                continue;
            }
            for (int dz = 1; dz <= iz; ++dz) {
                const QImage* parent = tiles.cached(source, iz - dz, wx >> dz, ty >> dz);
                if (!parent)
                    continue;
                const double sub = parent->width() / double(1 << dz);
                const QRectF srcr((wx - ((wx >> dz) << dz)) * sub, (ty - ((ty >> dz) << dz)) * sub, sub, sub);
                p.drawImage(QRectF(target), *parent, srcr);
                break;
            }
            // Nothing cached: the offline base map underneath stays visible.
        }
    }
}

void WorldMapWidget::request_imagery() {
    const int source = imagery_source();
    auto& tiles = map::ImageryTiles::instance();
    if (source < 0 || !isVisible() || width() <= 0) {
        tiles.release(this);
        return;
    }
    const int max_zoom = map::imagery_sources()[source].max_zoom;
    const double W = world_px();
    const int iz = std::clamp(static_cast<int>(std::floor(zoom_ + 0.35)), 0, max_zoom);
    const int n = 1 << iz;
    const double tile_px = W / n;
    const double left = center_.x() * W - width() / 2.0;
    const double top = center_.y() * W - height() / 2.0;
    const int tx0 = static_cast<int>(std::floor(left / tile_px));
    const int tx1 = static_cast<int>(std::floor((left + width()) / tile_px));
    const int ty0 = std::max(0, static_cast<int>(std::floor(top / tile_px)));
    const int ty1 = std::min(n - 1, static_cast<int>(std::floor((top + height()) / tile_px)));
    const double cx = (left + width() / 2.0) / tile_px - 0.5;
    const double cy = (top + height() / 2.0) / tile_px - 0.5;
    struct TileSlot {
        map::ImageryTiles::TileId id;
        double dist;
    };
    QVector<TileSlot> todo;
    QSet<qint64> seen;
    for (int ty = ty0; ty <= ty1; ++ty) {
        for (int tx = tx0; tx <= tx1; ++tx) {
            const int wx = ((tx % n) + n) % n;
            if (seen.contains((qint64(wx) << 32) | ty))
                continue;
            seen.insert((qint64(wx) << 32) | ty);
            todo.append({{iz, wx, ty}, std::hypot(tx - cx, ty - cy)});
        }
    }
    std::sort(todo.begin(), todo.end(), [](const TileSlot& a, const TileSlot& b) { return a.dist < b.dist; });
    QVector<map::ImageryTiles::TileId> want;
    want.reserve(todo.size() + 1);
    // The whole world at zoom 0 as a cheap stand-in behind everything else.
    if (iz > 0)
        want.append({0, 0, 0});
    for (const TileSlot& s : todo)
        want.append(s.id);
    tiles.want(this, source, want);
}

const QPixmap& WorldMapWidget::label_pixmap(const QString& text, int style) {
    const qreal dpr = devicePixelRatioF();
    const QString key = QStringLiteral("%1|%2|%3").arg(style).arg(dpr).arg(text);
    auto it = label_cache_.constFind(key);
    if (it != label_cache_.constEnd())
        return it.value();
    if (label_cache_.size() > 3000)
        label_cache_.clear();

    QFont font = fonts::font(-3, QFont::Medium);
    QColor fg = pal_.label;
    QColor halo = pal_.halo_land;
    switch (style) {
        case kWmwSea:
            font.setItalic(true);
            font.setWeight(QFont::Normal);
            fg = pal_.label_sea;
            halo = pal_.halo_sea;
            break;
        case kWmwCountryOnImagery:
            fg = pal_.over_imagery_text;
            halo = pal_.over_imagery_halo;
            break;
        case kWmwSeaOnImagery:
            font.setItalic(true);
            fg = wmw_alpha(pal_.over_imagery_text, 190);
            halo = pal_.over_imagery_halo;
            break;
        case kWmwPort:
            font = fonts::font(-4);
            fg = imagery_source() >= 0 ? pal_.over_imagery_text : pal_.label;
            halo = imagery_source() >= 0 ? pal_.over_imagery_halo : pal_.halo_land;
            break;
        case kWmwChokepoint:
            font = fonts::font(-4, QFont::Bold);
            fg = imagery_source() >= 0 ? QColor(THEME_OBSIDIAN.warning) : colors::legible_color(pal_.chokepoint);
            halo = imagery_source() >= 0 ? pal_.over_imagery_halo : pal_.halo_sea;
            break;
        default:
            break;
    }
    font.setLetterSpacing(QFont::AbsoluteSpacing, style == kWmwSea || style == kWmwSeaOnImagery ? 1.2 : 0.3);
    const QFontMetricsF fm(font);
    const QString shown = (style == kWmwSea || style == kWmwSeaOnImagery) ? text.toUpper() : text;
    const QSizeF size(fm.horizontalAdvance(shown) + 6, fm.height() + 4);
    QPixmap pm((size * dpr).toSize());
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter lp(&pm);
    lp.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    lp.setFont(font);
    // Halo: the text drawn around itself in the halo colour, then once on top.
    // Stroking the glyph outlines instead cost 2–6 ms per label, and a zoom
    // step or basemap switch builds dozens of new labels in one paint, which
    // froze the map for 150–450 ms; glyph draws come from the font cache.
    const QPointF origin(3, 2 + fm.ascent());
    static constexpr int kHaloOffsets[][2] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0},  {-1, 1},
                                              {0, 1},   {1, 1},  {-2, 0}, {2, 0},  {0, -2}, {0, 2}};
    lp.setPen(halo);
    for (const auto& o : kHaloOffsets)
        lp.drawText(origin + QPointF(o[0] * 0.75, o[1] * 0.75), shown);
    lp.setPen(fg);
    lp.drawText(origin, shown);
    lp.end();
    return label_cache_.insert(key, pm).value();
}

void WorldMapWidget::draw_labels(QPainter& p) {
    const auto data = map::GeoBaseMap::instance().data();
    if (!data)
        return;
    const bool on_imagery = imagery_source() >= 0;
    int k0 = 0, k1 = 0;
    visible_copies(&k0, &k1);
    QVector<QRectF> placed;
    const QRectF view = QRectF(rect()).adjusted(-50, -20, 50, 20);
    // Chokepoint names are drawn later, beside their markers: keep that space
    // free so a sea or country name is not printed underneath one.
    if (reference_layer(MapLayer::Chokepoints)) {
        for (const auto& c : map::maritime_chokepoints()) {
            if (c.min_zoom > zoom_ + 0.5)
                continue;
            const QSizeF sz = label_pixmap(c.name, kWmwChokepoint).deviceIndependentSize();
            for (int k = k0; k <= k1; ++k) {
                const QPointF s = world_to_screen(c.world + QPointF(k, 0));
                if (view.contains(s))
                    placed.append(QRectF(s.x() - 8, s.y() - sz.height() / 2, sz.width() + 18, sz.height()));
            }
        }
    }
    auto place = [&](const map::MapLabel& l, int style) {
        const QPixmap& pm = label_pixmap(l.name, style);
        const QSizeF sz = pm.deviceIndependentSize();
        for (int k = k0; k <= k1; ++k) {
            const QPointF s = world_to_screen(l.world + QPointF(k, 0));
            if (!view.contains(s))
                continue;
            const QRectF r(s.x() - sz.width() / 2, s.y() - sz.height() / 2, sz.width(), sz.height());
            bool clash = false;
            for (const QRectF& o : placed) {
                if (o.intersects(r)) {
                    clash = true;
                    break;
                }
            }
            if (clash)
                continue;
            placed.append(r.adjusted(-4, -2, 4, 2));
            p.drawPixmap(r.topLeft(), pm);
        }
    };
    for (const auto& l : data->seas)
        if (l.min_zoom <= zoom_ + 0.4)
            place(l, on_imagery ? kWmwSeaOnImagery : kWmwSea);
    for (const auto& l : data->countries)
        if (l.min_zoom <= zoom_ + 0.6)
            place(l, on_imagery ? kWmwCountryOnImagery : kWmwCountry);
}

void WorldMapWidget::draw_reference_layers(QPainter& p) {
    if (layers_ == 0)
        return;
    int k0 = 0, k1 = 0;
    visible_copies(&k0, &k1);
    const QRectF view = QRectF(rect()).adjusted(-10, -10, 10, 10);
    const bool on_imagery = imagery_source() >= 0;
    p.setRenderHint(QPainter::Antialiasing, true);

    if (reference_layer(MapLayer::Ports)) {
        if (const auto data = map::GeoBaseMap::instance().data()) {
            const QColor fill = on_imagery ? QColor(THEME_OBSIDIAN.cyan) : colors::legible_color(pal_.port);
            const double size = zoom_ >= 5 ? 6.0 : 4.5;
            QVector<QRectF> placed;
            for (const auto& port : data->ports) {
                if (port.min_zoom > zoom_ + 0.5)
                    continue;
                for (int k = k0; k <= k1; ++k) {
                    const QPointF s = world_to_screen(port.world + QPointF(k, 0));
                    if (!view.contains(s))
                        continue;
                    p.setPen(QPen(pal_.pin_outline, 1.0));
                    p.setBrush(fill);
                    p.drawRect(QRectF(s.x() - size / 2, s.y() - size / 2, size, size));
                    hits_.append({s, size / 2 + 2, -1, -1, tr("Port: %1").arg(port.name)});
                    if (zoom_ >= 5.0) {
                        const QPixmap& pm = label_pixmap(port.name, kWmwPort);
                        const QRectF r(s.x() + size, s.y() - pm.deviceIndependentSize().height() / 2,
                                       pm.deviceIndependentSize().width(), pm.deviceIndependentSize().height());
                        bool clash = false;
                        for (const QRectF& o : placed)
                            if ((clash = o.intersects(r)))
                                break;
                        if (!clash) {
                            placed.append(r);
                            p.drawPixmap(r.topLeft(), pm);
                        }
                    }
                }
            }
        }
    }

    if (reference_layer(MapLayer::Chokepoints)) {
        const QColor fill = on_imagery ? QColor(THEME_OBSIDIAN.warning) : colors::legible_color(pal_.chokepoint);
        for (const auto& c : map::maritime_chokepoints()) {
            if (c.min_zoom > zoom_ + 0.5)
                continue;
            for (int k = k0; k <= k1; ++k) {
                const QPointF s = world_to_screen(c.world + QPointF(k, 0));
                if (!view.contains(s))
                    continue;
                QPainterPath diamond;
                const double r = 6.0;
                diamond.moveTo(s.x(), s.y() - r);
                diamond.lineTo(s.x() + r, s.y());
                diamond.lineTo(s.x(), s.y() + r);
                diamond.lineTo(s.x() - r, s.y());
                diamond.closeSubpath();
                p.setPen(QPen(pal_.pin_outline, 1.2));
                p.setBrush(wmw_alpha(fill, 230));
                p.drawPath(diamond);
                const QPixmap& pm = label_pixmap(c.name, kWmwChokepoint);
                p.drawPixmap(QPointF(s.x() + r + 2, s.y() - pm.deviceIndependentSize().height() / 2), pm);
                hits_.append({s, r + 2, -1, -1, tr("Chokepoint: %1").arg(c.name)});
            }
        }
    }
}

void WorldMapWidget::draw_shapes(QPainter& p) {
    if (shapes_.isEmpty() && !has_preview_)
        return;
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor fill = wmw_alpha(pal_.accent, 48);
    QPen pen(wmw_alpha(pal_.accent, 220));
    pen.setWidthF(1.6);
    p.setPen(pen);
    p.setBrush(fill);
    auto draw_one = [&](const MapShape& s) {
        QPointF tl = map::geo_to_world(s.max_lat, s.min_lng);
        QPointF br = map::geo_to_world(s.min_lat, s.max_lng);
        const double shift = std::round(center_.x() - (tl.x() + br.x()) / 2); // nearest copy
        tl.rx() += shift;
        br.rx() += shift;
        const QRectF r(world_to_screen(tl), world_to_screen(br));
        if (s.type == MapShape::Type::Circle && s.radius_km > 0)
            p.drawEllipse(r.normalized());
        else
            p.drawRect(r.normalized());
    };
    for (const auto& s : shapes_)
        draw_one(s);
    if (has_preview_)
        draw_one(preview_shape_);
}

void WorldMapWidget::draw_track(QPainter& p) {
    if (track_world_.size() < 2)
        return;
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(wmw_alpha(imagery_source() >= 0 ? QColor(THEME_OBSIDIAN.accent) : pal_.accent, 215));
    pen.setWidthF(2.0);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    int k0 = 0, k1 = 0;
    visible_copies(&k0, &k1);
    for (int k = k0; k <= k1; ++k) {
        QPolygonF poly;
        poly.reserve(track_world_.size());
        for (const QPointF& w : track_world_)
            poly.append(world_to_screen(w + QPointF(k, 0)));
        if (poly.boundingRect().intersects(QRectF(rect())))
            p.drawPolyline(poly);
    }
}

const QPixmap& WorldMapWidget::pin_pixmap(const QColor& color, double radius) {
    const bool on_imagery = imagery_source() >= 0;
    const qreal dpr = devicePixelRatioF();
    const QString key = QStringLiteral("%1|%2|%3|%4").arg(color.rgba()).arg(qRound(radius * 10)).arg(dpr).arg(on_imagery);
    auto it = pin_cache_.constFind(key);
    if (it != pin_cache_.constEnd())
        return it.value();
    // Content colour: legible on the themed map, as is on (dark) imagery.
    QColor fill = on_imagery ? color : colors::legible_color(color);
    fill.setAlpha(color.alpha());
    const double r = std::max(2.5, radius);
    const int side = static_cast<int>(std::ceil(2 * r + 6));
    QPixmap pm(QSize(side, side) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter pp(&pm);
    pp.setRenderHint(QPainter::Antialiasing);
    const QPointF c(side / 2.0, side / 2.0);
    pp.setPen(Qt::NoPen);
    pp.setBrush(wmw_alpha(fill, std::min(70, fill.alpha() / 3)));
    pp.drawEllipse(c, r + 2.5, r + 2.5); // soft halo
    pp.setPen(QPen(pal_.pin_outline, 1.2));
    pp.setBrush(fill);
    pp.drawEllipse(c, r, r);
    pp.end();
    return pin_cache_.insert(key, pm).value();
}

void WorldMapWidget::draw_pins(QPainter& p) {
    if (pins_.isEmpty())
        return;
    p.setRenderHint(QPainter::Antialiasing, true);
    int k0 = 0, k1 = 0;
    visible_copies(&k0, &k1);
    const QRectF view = QRectF(rect()).adjusted(-24, -24, 24, 24);
    const bool clustering = clusterable_count_ > kWmwClusterThreshold;
    hit_zoom_ = std::clamp(static_cast<int>(std::floor(zoom_)), 0, 24);

    auto draw_pin = [&](int i) {
        const MapPin& pin = pins_[i];
        const QPixmap& pm = pin_pixmap(pin.color, pin.radius);
        const QSizeF sz = pm.deviceIndependentSize();
        for (int k = k0; k <= k1; ++k) {
            const QPointF s = world_to_screen(pin_world_[i] + QPointF(k, 0));
            if (!view.contains(s))
                continue;
            p.drawPixmap(QPointF(s.x() - sz.width() / 2, s.y() - sz.height() / 2), pm);
            if (pin.id >= 0)
                hits_.append({s, pin.radius + 3, -1, i, {}});
        }
    };

    if (clustering) {
        const auto& clusters = clusters_for_zoom(hit_zoom_);
        QFont f = fonts::font(-4, QFont::Bold);
        p.setFont(f);
        for (int c = 0; c < clusters.size(); ++c) {
            const Cluster& cl = clusters[c];
            if (cl.members.size() == 1) {
                draw_pin(cl.members.first());
                continue;
            }
            const double r = std::min(26.0, 9.0 + 3.2 * std::log2(double(cl.members.size())));
            QColor fill = imagery_source() >= 0 ? cl.color : colors::legible_color(cl.color);
            for (int k = k0; k <= k1; ++k) {
                const QPointF s = world_to_screen(cl.world + QPointF(k, 0));
                if (!view.contains(s))
                    continue;
                p.setPen(QPen(pal_.pin_outline, 1.5));
                p.setBrush(wmw_alpha(fill, 70));
                p.drawEllipse(s, r + 3, r + 3);
                p.setBrush(wmw_alpha(fill, 225));
                p.drawEllipse(s, r, r);
                p.setPen(colors::contrast_text_on(fill));
                const QString txt = cl.members.size() >= 1000 ? QStringLiteral("%1k").arg(cl.members.size() / 1000)
                                                              : QString::number(cl.members.size());
                p.drawText(QRectF(s.x() - r, s.y() - r, 2 * r, 2 * r), Qt::AlignCenter, txt);
                hits_.append({s, r + 2, c, -1, {}});
            }
        }
        for (int i = 0; i < pins_.size(); ++i)
            if (pins_[i].id < 0)
                draw_pin(i);
    } else {
        for (int i = 0; i < pins_.size(); ++i)
            draw_pin(i);
    }

    // Selection and hover rings on top.
    auto ring = [&](const QPointF& w, double r, const QColor& color, double width) {
        for (int k = k0; k <= k1; ++k) {
            const QPointF s = world_to_screen(w + QPointF(k, 0));
            if (!view.contains(s))
                continue;
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(pal_.pin_outline, width + 2));
            p.drawEllipse(s, r, r);
            p.setPen(QPen(color, width));
            p.drawEllipse(s, r, r);
        }
    };
    if (selected_id_ >= 0) {
        for (int i = 0; i < pins_.size(); ++i) {
            if (pins_[i].id == selected_id_) {
                draw_pin(i); // raised above its neighbours
                ring(pin_world_[i], pins_[i].radius + 5, imagery_source() >= 0 ? QColor(THEME_OBSIDIAN.accent) : pal_.accent, 2.2);
                break;
            }
        }
    }
    if (hover_pin_ >= 0 && hover_pin_ < pins_.size())
        ring(pin_world_[hover_pin_], pins_[hover_pin_].radius + 3.5, imagery_source() >= 0 ? pal_.over_imagery_text : pal_.ring, 1.4);
}

QRectF WorldMapWidget::zoom_button_rect(int which) const {
    const double s = std::max(26.0, QFontMetricsF(fonts::font(0)).height() + 8);
    return QRectF(10, 10 + which * (s + 2), s, s);
}

QString WorldMapWidget::status_text() const {
    auto& base = map::GeoBaseMap::instance();
    if (base.failed())
        return tr("Offline base map is missing from this build");
    if (!base.data())
        return tr("Loading map…");
    const int source = imagery_source();
    if (source >= 0 && map::ImageryTiles::instance().state(source) == map::ImageryTiles::State::Offline) {
        const QString why = map::ImageryTiles::instance().last_error(source);
        return why.isEmpty() ? tr("%1 imagery unreachable — showing the offline map, retrying").arg(
                                   QString::fromLatin1(kWmwBasemaps[basemap_index_].label))
                             : tr("%1 imagery unreachable (%2) — showing the offline map, retrying")
                                   .arg(QString::fromLatin1(kWmwBasemaps[basemap_index_].label), why);
    }
    return {};
}

void WorldMapWidget::draw_chrome(QPainter& p) {
    p.setRenderHint(QPainter::Antialiasing, true);
    // Zoom buttons.
    for (int i = 0; i < 2; ++i) {
        const QRectF r = zoom_button_rect(i);
        p.setPen(QPen(pal_.chrome_border, 1));
        p.setBrush(pal_.chrome_bg);
        p.drawRoundedRect(r, 3, 3);
        p.setPen(QPen(pal_.chrome_text, 1.6));
        const QPointF c = r.center();
        const double h = r.width() * 0.22;
        p.drawLine(QPointF(c.x() - h, c.y()), QPointF(c.x() + h, c.y()));
        if (i == 0)
            p.drawLine(QPointF(c.x(), c.y() - h), QPointF(c.x(), c.y() + h));
    }

    const QFont small = fonts::font(-4);
    const QFontMetricsF fm(small);
    p.setFont(small);

    // Scale bar (bottom-left), measured at the centre latitude.
    {
        double lat = 0, lng = 0;
        map::world_to_geo(center_, &lat, &lng);
        const double mpp = map::metres_per_pixel(lat, zoom_);
        const double metres = wmw_nice_length(mpp * 110);
        const double px = metres / mpp;
        const QString text = metres >= 1000 ? tr("%1 km").arg(QLocale().toString(metres / 1000, 'f', 0))
                                            : tr("%1 m").arg(QLocale().toString(metres, 'f', 0));
        const double y = height() - 12;
        const QRectF box(8, y - fm.height() - 8, std::max(px, fm.horizontalAdvance(text)) + 12, fm.height() + 14);
        p.setPen(Qt::NoPen);
        p.setBrush(pal_.chrome_bg);
        p.drawRoundedRect(box, 2, 2);
        p.setPen(QPen(pal_.chrome_text, 1.2));
        p.drawLine(QPointF(14, y), QPointF(14 + px, y));
        p.drawLine(QPointF(14, y - 4), QPointF(14, y));
        p.drawLine(QPointF(14 + px, y - 4), QPointF(14 + px, y));
        p.drawText(QPointF(14, y - 6), text);
    }

    // Attribution (bottom-right): what the sources ask for.
    {
        QString text = tr("Made with Natural Earth");
        const int source = imagery_source();
        if (source >= 0)
            text += QStringLiteral(" · ") + map::imagery_sources()[source].attribution;
        const double w = fm.horizontalAdvance(text) + 12;
        const QRectF box(width() - w - 4, height() - fm.height() - 8, w, fm.height() + 4);
        p.setPen(Qt::NoPen);
        p.setBrush(pal_.chrome_bg);
        p.drawRect(box);
        p.setPen(pal_.chrome_dim);
        p.drawText(box, Qt::AlignCenter, text);
    }

    // Status pills: the map's own (base loading / imagery offline) at the bottom
    // centre, the caller's message (no data, sign in …) at the top centre.
    auto pill = [&](const QString& text, double y, const QColor& edge) {
        const QFont f = fonts::font(-3, QFont::Medium);
        const QFontMetricsF pfm(f);
        const double maxw = width() - 120.0;
        const QString shown = pfm.elidedText(text, Qt::ElideRight, std::max(80.0, maxw - 24));
        const double w = pfm.horizontalAdvance(shown) + 24;
        const QRectF box((width() - w) / 2, y, w, pfm.height() + 12);
        p.setPen(QPen(edge, 1));
        p.setBrush(pal_.chrome_bg);
        p.drawRoundedRect(box, 3, 3);
        p.setFont(f);
        p.setPen(pal_.chrome_text);
        p.drawText(box, Qt::AlignCenter, shown);
        p.setFont(small);
    };
    const QString status = status_text();
    if (!status.isEmpty())
        pill(status, height() - fm.height() - 50, QColor(colors::WARNING()));
    if (!overlay_message_.isEmpty())
        pill(overlay_message_, 12, pal_.chrome_border);
}

// ── Hit testing / hover ──────────────────────────────────────────────────────

int WorldMapWidget::hit_test(const QPointF& pos) const {
    int best = -1;
    double best_d = 1e9;
    for (int i = hits_.size() - 1; i >= 0; --i) {
        const double d = std::hypot(hits_[i].pos.x() - pos.x(), hits_[i].pos.y() - pos.y());
        if (d <= hits_[i].radius + 3 && d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

QString WorldMapWidget::tooltip_for(const HitTarget& hit) const {
    if (!hit.ref.isEmpty())
        return hit.ref;
    if (hit.pin >= 0 && hit.pin < pins_.size())
        return pins_[hit.pin].label;
    auto it = clusters_.constFind(hit_zoom_);
    if (hit.cluster >= 0 && it != clusters_.constEnd() && hit.cluster < it->size()) {
        const Cluster& c = it->at(hit.cluster);
        QStringList lines;
        lines << tr("%n item(s) here — click to zoom in", "", static_cast<int>(c.members.size()));
        for (int k = 0; k < c.members.size() && k < 6; ++k)
            lines << QStringLiteral("• ") + pins_[c.members[k]].label;
        if (c.members.size() > 6)
            lines << tr("… and %n more", "", static_cast<int>(c.members.size() - 6));
        return lines.join(QLatin1Char('\n'));
    }
    return {};
}

void WorldMapWidget::clear_hover() {
    if (hover_pin_ < 0 && hover_cluster_ < 0 && hover_ref_.isEmpty())
        return;
    hover_pin_ = -1;
    hover_cluster_ = -1;
    hover_ref_.clear();
    QToolTip::hideText();
    if (draw_mode_ == MapDrawMode::None)
        unsetCursor();
}

void WorldMapWidget::update_hover(const QPointF& pos) {
    const int idx = hit_test(pos);
    const int pin = idx >= 0 ? hits_[idx].pin : -1;
    const int cluster = idx >= 0 ? hits_[idx].cluster : -1;
    const QString ref = idx >= 0 ? hits_[idx].ref : QString();
    if (pin == hover_pin_ && cluster == hover_cluster_ && ref == hover_ref_)
        return;
    hover_pin_ = pin;
    hover_cluster_ = cluster;
    hover_ref_ = ref;
    if (idx >= 0) {
        if (draw_mode_ == MapDrawMode::None)
            setCursor(ref.isEmpty() ? Qt::PointingHandCursor : Qt::ArrowCursor);
        const QString tip = tooltip_for(hits_[idx]);
        if (!tip.isEmpty()) {
            // Shown as rich text so a long line wraps: as plain text a 140-character
            // headline made the card over 1000 px wide and it covered the map.
            QStringList lines;
            for (const QString& line : tip.split(QLatin1Char('\n')))
                lines << line.toHtmlEscaped();
            QToolTip::showText(mapToGlobal(pos.toPoint()),
                               QStringLiteral("<qt>") + lines.join(QStringLiteral("<br>")) + QStringLiteral("</qt>"), this);
        }
    } else {
        QToolTip::hideText();
        if (draw_mode_ == MapDrawMode::None)
            unsetCursor();
    }
    update();
}

// ── Events ───────────────────────────────────────────────────────────────────

void WorldMapWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (width() <= 0 || height() <= 0)
        return;
    if (!camera_initialised_) {
        camera_initialised_ = true;
        if (std::exchange(view_preset_, false))
            clamp_camera();
        else
            apply_initial_view();
        if (std::exchange(fit_pending_, false))
            do_fit_pins();
    } else {
        clamp_camera();
    }
    settle_timer_.start();
}

void WorldMapWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    settle_timer_.start(); // asks for imagery for the current view
}

void WorldMapWidget::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    map::ImageryTiles::instance().release(this);
    clear_hover();
}

void WorldMapWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    const QPointF pos = event->position();
    for (int i = 0; i < 2; ++i) {
        if (zoom_button_rect(i).contains(pos)) {
            zoom_at(QPointF(width() / 2.0, height() / 2.0), std::round(zoom_) + (i == 0 ? 1 : -1), true);
            event->accept();
            return;
        }
    }
    if (draw_mode_ != MapDrawMode::None) {
        if (pixel_to_geo(pos, &drag_start_lat_, &drag_start_lng_)) {
            drawing_ = true;
            has_preview_ = false;
        }
        event->accept();
        return;
    }
    if (anim_)
        anim_->stop();
    pressed_ = true;
    dragged_ = false;
    press_pos_ = pos;
    press_center_ = center_;
    event->accept();
}

void WorldMapWidget::mouseMoveEvent(QMouseEvent* event) {
    const QPointF pos = event->position();
    if (draw_mode_ != MapDrawMode::None) {
        if (!drawing_)
            return;
        double cur_lat = 0, cur_lng = 0;
        if (!pixel_to_geo(pos, &cur_lat, &cur_lng))
            return;
        MapShape s;
        s.min_lat = std::min(drag_start_lat_, cur_lat);
        s.max_lat = std::max(drag_start_lat_, cur_lat);
        s.min_lng = std::min(drag_start_lng_, cur_lng);
        s.max_lng = std::max(drag_start_lng_, cur_lng);
        if (draw_mode_ == MapDrawMode::Circle) {
            s.type = MapShape::Type::Circle;
            s.centre_lat = drag_start_lat_;
            s.centre_lng = drag_start_lng_;
            s.radius_km = wmw_haversine_km(drag_start_lat_, drag_start_lng_, cur_lat, cur_lng);
            const double dlat = s.radius_km / 111.0;
            const double dlng = s.radius_km / (111.320 * std::max(0.05, std::cos(qDegreesToRadians(drag_start_lat_))));
            s.min_lat = drag_start_lat_ - dlat;
            s.max_lat = drag_start_lat_ + dlat;
            s.min_lng = drag_start_lng_ - dlng;
            s.max_lng = drag_start_lng_ + dlng;
        } else {
            s.type = MapShape::Type::Rectangle;
        }
        preview_shape_ = s;
        has_preview_ = true;
        update();
        return;
    }
    if (pressed_ && (event->buttons() & Qt::LeftButton)) {
        const QPointF d = pos - press_pos_;
        if (!dragged_ && std::abs(d.x()) + std::abs(d.y()) > 3) {
            dragged_ = true;
            clear_hover();
            setCursor(Qt::ClosedHandCursor);
        }
        if (dragged_) {
            center_ = press_center_ - d / world_px();
            camera_moved(true);
        }
        return;
    }
    update_hover(pos);
}

void WorldMapWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    if (draw_mode_ != MapDrawMode::None) {
        if (!drawing_)
            return;
        drawing_ = false;
        if (has_preview_) {
            // Reject degenerate (zero-area / accidental click) shapes.
            const bool valid = (preview_shape_.type == MapShape::Type::Circle)
                                   ? preview_shape_.radius_km > 1.0
                                   : (preview_shape_.max_lat - preview_shape_.min_lat) > 0.01 &&
                                         (preview_shape_.max_lng - preview_shape_.min_lng) > 0.01;
            has_preview_ = false;
            if (valid) {
                shapes_.append(preview_shape_);
                emit shapes_changed();
            }
            update();
        }
        return;
    }
    if (!pressed_)
        return;
    pressed_ = false;
    if (dragged_) {
        dragged_ = false;
        unsetCursor();
        settle_timer_.start();
        return;
    }
    // A click.
    const int idx = hit_test(event->position());
    if (idx < 0)
        return;
    const HitTarget hit = hits_[idx];
    if (hit.pin >= 0 && hit.pin < pins_.size()) {
        const int id = pins_[hit.pin].id;
        if (id >= 0) {
            selected_id_ = id;
            update();
            emit pin_clicked(id);
        }
        return;
    }
    auto it = clusters_.constFind(hit_zoom_);
    if (hit.cluster >= 0 && it != clusters_.constEnd() && hit.cluster < it->size()) {
        // Zoom into the cluster's members.
        const Cluster& c = it->at(hit.cluster);
        QRectF r;
        for (int m : c.members) {
            QPointF w = pin_world_[m];
            w.setX(w.x() + std::round(c.world.x() - w.x()));
            r = r.isNull() ? QRectF(w, QSizeF(1e-9, 1e-9)) : r.united(QRectF(w, QSizeF(1e-9, 1e-9)));
        }
        const QPointF shift(std::round(center_.x() - r.center().x()), 0);
        r.translate(shift);
        const double pad = 24.0 / world_px();
        fit_world_rect(r.adjusted(-pad, -pad, pad, pad), std::min(kWmwMaxZoom, std::floor(zoom_) + 4), true);
    }
}

void WorldMapWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (draw_mode_ != MapDrawMode::None || event->button() != Qt::LeftButton) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }
    zoom_at(event->position(), std::floor(zoom_ + 0.01) + 1, true);
}

void WorldMapWidget::wheelEvent(QWheelEvent* event) {
    double steps = event->angleDelta().y() / 120.0;
    if (steps == 0 && !event->pixelDelta().isNull())
        steps = event->pixelDelta().y() / 60.0;
    if (steps == 0) {
        event->ignore();
        return;
    }
    if (anim_)
        anim_->stop();
    zoom_at(event->position(), zoom_ + steps * 0.5, false);
    event->accept();
}

void WorldMapWidget::keyPressEvent(QKeyEvent* event) {
    const QPointF mid(width() / 2.0, height() / 2.0);
    const double step = 0.25 * std::min(width(), height()) / world_px();
    switch (event->key()) {
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            zoom_at(mid, std::round(zoom_) + 1, true);
            break;
        case Qt::Key_Minus:
            zoom_at(mid, std::round(zoom_) - 1, true);
            break;
        case Qt::Key_Left:
            animate_to(center_ - QPointF(step, 0), zoom_);
            break;
        case Qt::Key_Right:
            animate_to(center_ + QPointF(step, 0), zoom_);
            break;
        case Qt::Key_Up:
            animate_to(center_ - QPointF(0, step), zoom_);
            break;
        case Qt::Key_Down:
            animate_to(center_ + QPointF(0, step), zoom_);
            break;
        case Qt::Key_Home:
            if (pins_.isEmpty())
                apply_initial_view();
            else
                do_fit_pins();
            break;
        default:
            QWidget::keyPressEvent(event);
            return;
    }
    event->accept();
}

void WorldMapWidget::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
    if (!pressed_) {
        clear_hover();
        update();
    }
}

// ── Drawing tools ────────────────────────────────────────────────────────────

void WorldMapWidget::set_draw_mode(MapDrawMode mode) {
    draw_mode_ = mode;
    drawing_ = false;
    has_preview_ = false;
    if (mode == MapDrawMode::None)
        unsetCursor();
    else
        setCursor(Qt::CrossCursor);
    update();
}

void WorldMapWidget::clear_shapes() {
    shapes_.clear();
    has_preview_ = false;
    update();
    emit shapes_changed();
}

} // namespace fincept::ui
