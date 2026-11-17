#pragma once
// CandleChart — native OHLC candlestick chart with a volume pane (QPainter,
// no WebEngine). Timeframe-agnostic: bars are laid out by index, one slot
// each, so weekends / overnight gaps take no space whatever the bar size.
//
//   auto* c = new ui::CandleChart(this);
//   c->set_candles(history);                 // ui::CandleData, secs or ms auto-detected
//   const int ma = c->add_overlay_series(tr("MA 50"), sma50);   // aligned with the bars
//   c->add_trade_marker(fill_ms, 182.4, ui::CandleChart::TradeSide::Buy, tr("B"), tr("Bought 100"));
//   c->add_hline(stop, tr("Stop"), ui::ChartColorRole::Negative);
//   c->update_bar(live_bar);                 // replaces / appends; the view follows
//   c->add_band(entry_ms, exit_ms, ui::ChartColorRole::Accent, tr("Trade 12"));   // shaded selection
//   c->set_visible_range(entry_ms - pad_ms, exit_ms + pad_ms);                    // zoom to it
//
// TIME UNIT: Bar::time_ms is epoch MILLISECONDS (bar open time). set_candles()
// accepts ui::CandleData whose timestamps may be seconds or milliseconds
// (TimeUnit::Auto: values below 1e11 are taken as seconds).
//
// Interaction: crosshair with an O/H/L/C/V readout in the header, wheel zoom
// (about the cursor; pinned to the right edge while following live data),
// drag pan, double-click reset, hover/click on trade markers, right-click for
// copy image / PNG / CSV / log scale / volume / reset.
//
// Rendering: static layer cached at the device pixel ratio; the crosshair is
// drawn on top. When bars are narrower than ~1.5 px they are aggregated per
// pixel column (high-low range per column), so 10k+ bars stay smooth.
// Colours and fonts follow the active theme (rebuilt on theme_changed).

#include "ui/charts/ChartKit.h"

#include <QPixmap>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <functional>
#include <limits>

class QMenu;

namespace fincept::ui {

struct CandleData;

class CandleChart : public QWidget {
    Q_OBJECT
  public:
    struct Bar {
        double time_ms = 0.0; ///< bar open time, epoch milliseconds
        double open = 0.0;
        double high = 0.0;
        double low = 0.0;
        double close = 0.0;
        double volume = 0.0;
    };
    enum class TimeUnit { Auto, Seconds, Milliseconds };
    enum class TradeSide { Buy, Sell };
    using ColorRole = ChartColorRole;
    using State = ChartState;
    using ValueFormatter = std::function<QString(double)>;
    using TimeFormatter = std::function<QString(double)>; ///< receives epoch ms

    explicit CandleChart(QWidget* parent = nullptr);
    ~CandleChart() override;

    // ── Data ────────────────────────────────────────────────────────────────
    /// Replace all bars (sorted by time if needed; bars with a non-finite
    /// open/high/low/close are dropped).
    void set_bars(const QVector<Bar>& bars);
    /// Convenience for the existing ui::CandleData (src/ui/charts/CandleData.h).
    void set_candles(const QVector<CandleData>& candles, TimeUnit unit = TimeUnit::Auto);
    /// Live update: same time as the last bar → replace it; newer → append.
    /// Older bars are ignored.
    void update_bar(const Bar& bar);
    void clear();
    int bar_count() const;
    const QVector<Bar>& bars() const;

    // ── Overlays (moving averages, bands, VWAP …) ───────────────────────────
    /// `values` align index-for-index with the bars (NaN = no value). A
    /// shorter vector covers the first bars; set it again after set_bars().
    int add_overlay_series(const QString& name, const QVector<double>& values, ColorRole role = ColorRole::Auto,
                           qreal width = 1.25, bool dashed = false);
    void set_overlay_data(int id, const QVector<double>& values);
    void set_overlay_visible(int id, bool visible);
    void remove_overlay(int id);
    void clear_overlays();

    // ── Annotations (one id space) ──────────────────────────────────────────
    /// Arrow at `price` on the bar containing `time_ms` (the last bar opening
    /// at or before it). Hover shows `tooltip`; click emits trade_marker_clicked.
    int add_trade_marker(double time_ms, double price, TradeSide side, const QString& label = {},
                         const QString& tooltip = {});
    /// Horizontal price line with an axis tag.
    int add_hline(double price, const QString& label = {}, ColorRole role = ColorRole::Accent, bool dashed = true);
    /// Shaded vertical band over the bars containing `from_ms` .. `to_ms` (epoch ms, either order;
    /// each end on the bar containing it, as trade markers), behind the candles across the price
    /// and volume panes, with 1 px edges and an optional `label` tag at its top (opaque, contrast
    /// text, so it reads on both themes). Auto = Accent. A band ending before the first bar is not
    /// drawn. Returns its id (shared with the annotations: remove_annotation(id) removes it), or
    /// -1 when a time is not finite.
    int add_band(double from_ms, double to_ms, ColorRole role = ColorRole::Accent, const QString& label = {});
    /// Removes every band (markers and lines stay). clear() removes bands too.
    void clear_bands();
    int band_count() const;
    /// Removes the marker, line or band with this id.
    void remove_annotation(int id);
    /// Removes markers and horizontal lines (bands stay; see clear_bands()).
    void clear_annotations();

    // ── View ────────────────────────────────────────────────────────────────
    /// Bars shown after a reset (default 120).
    void set_default_visible_bars(int count);
    /// Show the latest bars and follow live updates again.
    void reset_view();
    /// Show the bars containing `from_ms` .. `to_ms` (epoch ms, either order) and stop following
    /// live updates (reset_view() / double-click follows again). Clamped to the data: a range
    /// before the first / after the last bar shows the first / last bars; a span shorter than the
    /// zoom floor (10 slots) is centred. Emits visible_range_changed when the shown span changes.
    /// Call after set_bars() / set_candles() (they reset the view). False (nothing changes) when
    /// there are no bars or a time is not finite.
    bool set_visible_range(double from_ms, double to_ms);
    void set_show_volume(bool on);
    /// Volume pane height as a fraction of the plot (default 0.2).
    void set_volume_ratio(double ratio);
    void set_log_scale(bool on);
    bool log_scale() const;
    void set_show_last_price(bool on);
    void set_title(const QString& title);
    void set_price_formatter(ValueFormatter fn);
    void set_volume_formatter(ValueFormatter fn);
    /// Readout / crosshair time label (default: date, plus time for intraday bars).
    void set_time_formatter(TimeFormatter fn);
    void set_utc(bool on);
    /// Loading draws a note over existing bars; Empty / Error replace the plot.
    void set_state(State state, const QString& message = {});
    State state() const;
    void set_export_name(const QString& name);
    void set_context_menu_extra(std::function<void(QMenu*)> fn);

    // ── Export ──────────────────────────────────────────────────────────────
    QPixmap render_image();
    /// Time, Open, High, Low, Close, Volume + one column per overlay.
    void csv_table(QStringList& headers, QVector<QStringList>& rows) const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  signals:
    /// Time span of the visible bars changed through zoom / pan / reset.
    void visible_range_changed(double from_ms, double to_ms);
    /// Bar under the crosshair; -1 when it goes away.
    void crosshair_moved(int bar_index);
    void trade_marker_clicked(int id);
    void log_scale_changed(bool on);

  protected:
    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void leaveEvent(QEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
    void changeEvent(QEvent* e) override;

  private:
    struct Overlay {
        int id = 0;
        QString name;
        QVector<double> values;
        ColorRole role = ColorRole::Auto;
        int palette_slot = 0;
        qreal width = 1.25;
        bool dashed = false;
        bool visible = true;
    };
    struct Annotation {
        int id = 0;
        bool is_marker = true;
        double time_ms = 0.0;
        double price = 0.0;
        TradeSide side = TradeSide::Buy;
        ColorRole role = ColorRole::Accent;
        bool dashed = true;
        QString label;
        QString tooltip;
    };
    struct Band {
        int id = 0;
        double from_ms = 0.0;
        double to_ms = 0.0;
        ColorRole role = ColorRole::Accent;
        QString label;
    };
    struct Scale {
        double lo = 0.0;
        double hi = 1.0;
        bool log = false;
        QVector<double> ticks;
        double step = 0.0;
    };
    struct MarkerHit {
        QRect rect;
        int id = -1;
    };
    struct Layout {
        bool valid = false;
        int header_h = 0;
        int x_axis_h = 0;
        int axis_w = 0;
        QRect price;  ///< price pane
        QRect volume; ///< volume pane (empty when hidden)
        QRect plot;   ///< union
        Scale pscale;
        Scale vscale;
        int i_first = 0; ///< first bar drawn
        int i_last = -1; ///< last bar drawn
        double slot_w = 1.0;
        QVector<QPair<int, QString>> x_labels; ///< bar index → label
        QVector<MarkerHit> marker_hits;
        double spacing_ms = 0.0;
    };

    void invalidate();
    double view_start() const;
    void set_view(double start, double count, bool follow, bool notify);
    void notify_range();
    double bar_x(int i) const;
    int index_at(int px) const;
    double py(const QRect& r, const Scale& s, double v) const;
    double value_at(const QRect& r, const Scale& s, double y) const;
    QString fmt_price(double v) const;
    QString fmt_volume(double v) const;
    QString fmt_time(double ms) const;
    int bar_at_time(double ms) const;

    void compute_layout();
    void rebuild_cache();
    void paint_candles(QPainter& p);
    void paint_volume(QPainter& p);
    void paint_overlays(QPainter& p);
    void paint_annotations(QPainter& p);
    void paint_bands(QPainter& p, bool labels);
    void paint_axes(QPainter& p);
    void paint_header(QPainter& p, int bar);
    void paint_overlay_layer(QPainter& p);
    void build_context_menu(QMenu* menu);

    QVector<Bar> bars_;
    QVector<Overlay> overlays_;
    QVector<Annotation> annotations_;
    QVector<Band> bands_;
    int next_overlay_id_ = 1;
    int next_annotation_id_ = 1;
    int next_palette_slot_ = 0;

    // View: fractional bar index at the left edge + number of slots across.
    double view_start_ = 0.0;
    double view_count_ = 120.0;
    bool follow_ = true;
    int default_count_ = 120;
    double last_from_ms_ = std::numeric_limits<double>::quiet_NaN();
    double last_to_ms_ = std::numeric_limits<double>::quiet_NaN();

    bool show_volume_ = true;
    double volume_ratio_ = 0.2;
    bool log_scale_ = false;
    bool show_last_price_ = true;
    bool utc_ = false;
    QString title_;
    ValueFormatter price_fmt_;
    ValueFormatter volume_fmt_;
    TimeFormatter time_fmt_;
    State state_ = State::Ready;
    QString state_message_;
    QString export_name_;
    std::function<void(QMenu*)> menu_extra_;

    QPixmap cache_;
    bool dirty_ = true;
    Layout layout_;

    QPoint mouse_;
    bool mouse_in_ = false;
    bool dragging_ = false;
    bool drag_moved_ = false;
    QPoint press_pos_;
    double press_start_ = 0.0;
    int cross_bar_ = -1;
    int hover_marker_ = -1;
};

} // namespace fincept::ui
