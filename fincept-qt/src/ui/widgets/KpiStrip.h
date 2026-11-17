#pragma once
// KpiStrip — a responsive row of StatTiles that wraps into a balanced grid.
//
//   auto* strip = new ui::KpiStrip(this);
//   strip->add_tile("ret", tr("RETURN"))->set_value("12.4%");
//   strip->add_tile("vol", tr("VOLATILITY"))->set_value("18.1%");
//   strip->tile("ret")->set_delta(+1.2);
//
// Columns = how many tiles of at least `min_tile_width` fit, capped by
// `max_columns`, then balanced (7 tiles that fit 5 per row lay out 4 + 3, not
// 5 + 2). All tiles in the strip share one height. The strip reports
// height-for-width, so it grows in a layout as it wraps.

#include <QPointer>
#include <QString>
#include <QVector>
#include <QWidget>

namespace fincept::ui {

class StatTile;

class KpiStrip : public QWidget {
    Q_OBJECT
  public:
    explicit KpiStrip(QWidget* parent = nullptr);

    /// Create a tile, append it and return it.
    StatTile* add_tile(const QString& label);
    /// Same, registered under `key` for tile(key).
    StatTile* add_tile(const QString& key, const QString& label);
    /// Append an existing tile (re-parented to the strip).
    void add_tile(StatTile* tile, const QString& key = {});

    StatTile* tile(int index) const;
    StatTile* tile(const QString& key) const;
    int count() const;

    /// Remove and delete one tile / all tiles.
    void remove_tile(StatTile* tile);
    void clear();

    /// Narrowest a tile may get before the strip wraps (0 = derived from the
    /// font, about 20 characters).
    void set_min_tile_width(int px);
    /// Upper bound on tiles per row (0 = no limit).
    void set_max_columns(int columns);
    /// Gap between tiles in px.
    void set_spacing(int px);

    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  protected:
    bool event(QEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

  private:
    QVector<StatTile*> live_tiles() const;
    int min_width() const;
    int columns_for(int width, int n) const;
    int row_height(const QVector<StatTile*>& tiles) const;
    void relayout();

    QVector<QPointer<StatTile>> tiles_;
    int min_tile_width_ = 0;
    int max_columns_ = 0;
    int min_tile_font_px_ = 0; // base font size when set_min_tile_width() was called
    int spacing_ = 6;
};

} // namespace fincept::ui
