#include "ui/widgets/KpiStrip.h"

#include "ui/theme/Theme.h"
#include "ui/widgets/StatTile.h"

#include <QEvent>
#include <QFontMetrics>
#include <QResizeEvent>

#include <algorithm>

namespace fincept::ui {

namespace {
constexpr const char* kKpsKeyProperty = "kpi_strip_key";
} // namespace

KpiStrip::KpiStrip(QWidget* parent) : QWidget(parent) {
    QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Preferred);
    sp.setHeightForWidth(true);
    setSizePolicy(sp);
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this, [this]() {
        updateGeometry();
        relayout();
    });
}

StatTile* KpiStrip::add_tile(const QString& label) {
    return add_tile(QString(), label);
}

StatTile* KpiStrip::add_tile(const QString& key, const QString& label) {
    auto* t = new StatTile(label, this);
    add_tile(t, key);
    return t;
}

void KpiStrip::add_tile(StatTile* tile, const QString& key) {
    if (!tile)
        return;
    if (tile->parentWidget() != this)
        tile->setParent(this);
    if (!key.isEmpty())
        tile->setProperty(kKpsKeyProperty, key);
    tiles_.append(tile);
    tile->show();
    updateGeometry();
    // While the strip is not on screen (being built), placing every tile after every add made
    // a strip of n tiles measure all tiles n times. A hidden strip is laid out when it is
    // shown (QEvent::Show in event(), and the resize event of a first show).
    if (isVisible())
        relayout();
}

StatTile* KpiStrip::tile(int index) const {
    int i = 0;
    for (const auto& t : tiles_) {
        if (!t)
            continue;
        if (i == index)
            return t.data();
        ++i;
    }
    return nullptr;
}

StatTile* KpiStrip::tile(const QString& key) const {
    if (key.isEmpty())
        return nullptr;
    for (const auto& t : tiles_) {
        if (t && t->property(kKpsKeyProperty).toString() == key)
            return t.data();
    }
    return nullptr;
}

int KpiStrip::count() const {
    return static_cast<int>(std::count_if(tiles_.begin(), tiles_.end(), [](const QPointer<StatTile>& t) { return !t.isNull(); }));
}

void KpiStrip::remove_tile(StatTile* tile) {
    if (!tile)
        return;
    tiles_.removeIf([tile](const QPointer<StatTile>& t) { return t.isNull() || t.data() == tile; });
    if (tile->parentWidget() == this)
        delete tile;
    updateGeometry();
    relayout();
}

void KpiStrip::clear() {
    const auto copy = tiles_;
    tiles_.clear();
    for (const auto& t : copy) {
        if (t && t->parentWidget() == this)
            delete t.data();
    }
    updateGeometry();
}

void KpiStrip::set_min_tile_width(int px) {
    min_tile_width_ = std::max(0, px);
    min_tile_font_px_ = fonts::font_px();
    updateGeometry();
    relayout();
}

void KpiStrip::set_max_columns(int columns) {
    max_columns_ = std::max(0, columns);
    updateGeometry();
    relayout();
}

void KpiStrip::set_spacing(int px) {
    spacing_ = std::max(0, px);
    updateGeometry();
    relayout();
}

QVector<StatTile*> KpiStrip::live_tiles() const {
    QVector<StatTile*> out;
    out.reserve(tiles_.size());
    for (const auto& t : tiles_) {
        if (t && !t->isHidden())
            out.append(t.data());
    }
    return out;
}

int KpiStrip::min_width() const {
    int mw = min_tile_width_;
    if (mw <= 0) {
        mw = QFontMetrics(fonts::font(-3)).averageCharWidth() * 20;
    } else if (min_tile_font_px_ > 0) {
        // Callers pass a width chosen for the font in use when the strip was
        // built (metrics::width(132), a text width). It follows a later font
        // change the way metrics::width() does — in proportion above the
        // default size, the design value below it. Without this a strip built
        // at 14 px kept eight tiles in one row at 24 px, every value cut off.
        const int base = ThemeManager::kDefaultFontPx;
        mw = mw * std::max(fonts::font_px(), base) / std::max(min_tile_font_px_, base);
    }
    for (const auto& t : tiles_) {
        if (t && !t->isHidden())
            mw = std::max(mw, t->minimumSizeHint().width());
    }
    return std::max(1, mw);
}

int KpiStrip::columns_for(int width, int n) const {
    if (n <= 0)
        return 1;
    const int mw = min_width();
    int cols = std::max(1, (width + spacing_) / (mw + spacing_));
    if (max_columns_ > 0)
        cols = std::min(cols, max_columns_);
    cols = std::min(cols, n);
    // Balance: 7 tiles at 5 per row → 4 + 3, not 5 + 2.
    const int rows = (n + cols - 1) / cols;
    return (n + rows - 1) / rows;
}

int KpiStrip::row_height(const QVector<StatTile*>& tiles) const {
    int h = 0;
    for (auto* t : tiles)
        h = std::max(h, t->sizeHint().height());
    return h;
}

bool KpiStrip::hasHeightForWidth() const {
    return true;
}

int KpiStrip::heightForWidth(int width) const {
    const auto tiles = live_tiles();
    const int n = static_cast<int>(tiles.size());
    if (n == 0)
        return 0;
    const int cols = columns_for(width, n);
    const int rows = (n + cols - 1) / cols;
    return rows * row_height(tiles) + (rows - 1) * spacing_;
}

QSize KpiStrip::sizeHint() const {
    const auto tiles = live_tiles();
    const int n = static_cast<int>(tiles.size());
    if (n == 0)
        return {0, 0};
    const int per_row = max_columns_ > 0 ? std::min(n, max_columns_) : n;
    const int w = per_row * min_width() + (per_row - 1) * spacing_;
    return {w, heightForWidth(w)};
}

QSize KpiStrip::minimumSizeHint() const {
    const auto tiles = live_tiles();
    if (tiles.isEmpty())
        return {0, 0};
    return {min_width(), row_height(tiles)};
}

bool KpiStrip::event(QEvent* e) {
    switch (e->type()) {
        case QEvent::LayoutRequest: // a tile called updateGeometry()
        case QEvent::ChildRemoved:  // a tile was deleted or re-parented away
            updateGeometry();
            relayout();
            break;
        case QEvent::Show: // tiles added while hidden are placed now (add_tile)
            relayout();
            break;
        default:
            break;
    }
    return QWidget::event(e);
}

void KpiStrip::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    relayout();
}

void KpiStrip::relayout() {
    const auto tiles = live_tiles();
    const int n = static_cast<int>(tiles.size());
    if (n == 0)
        return;
    const int W = width();
    const int cols = columns_for(W, n);
    const int h = row_height(tiles);
    const int avail = std::max(0, W - (cols - 1) * spacing_);
    const int base_w = avail / cols;
    const int extra = avail % cols;
    for (int i = 0; i < n; ++i) {
        const int r = i / cols;
        const int c = i % cols;
        const int x = c * (base_w + spacing_) + std::min(c, extra);
        const int w = base_w + (c < extra ? 1 : 0);
        tiles[i]->setGeometry(x, r * (h + spacing_), w, h);
    }
}

} // namespace fincept::ui
