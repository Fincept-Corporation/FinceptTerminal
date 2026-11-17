#pragma once
// StatTile — one KPI: label, value, optional signed delta, optional sub-label
// and embedded sparkline, a tone, and a data state (loading / error / stale).
//
//   auto* t = new ui::StatTile(tr("SHARPE"), this);
//   t->set_number(1.42, 2);
//   t->set_delta(+0.12, QStringLiteral("+0.12 vs 1Y"));
//   t->set_sub_label(tr("rolling 252d"));
//   t->set_sparkline(rolling_sharpe);
//   t->set_state(ui::StatTile::State::Stale, tr("last update 14:02"));
//
// Fully custom-painted: every colour and font is read from the active theme
// at paint time, so theme / font switches need no re-styling. Lay several out
// with ui::KpiStrip.

#include <QVector>
#include <QWidget>

#include <optional>

namespace fincept::ui {

class StatTile : public QWidget {
    Q_OBJECT
  public:
    /// Colour of the value text and the accent stripe on the left edge.
    /// Positive / Negative are price direction (they follow the user's
    /// direction palette). Danger / Success are *state* (failed / breached
    /// limit, passed / healthy): always the danger / success tokens, and they
    /// carry a shape cue too so they read without colour — a painted mark
    /// before the label (cross for Danger, tick for Success), and Danger's
    /// stripe is twice as wide. The accessible description says it as well.
    enum class Tone { Neutral, Positive, Negative, Warning, Danger, Success };
    /// Data state. Loading shows a placeholder, Error a dash and the reason,
    /// Stale keeps the content but dims it and shows a STALE badge. The
    /// reason given with set_state is appended to the tooltip.
    enum class State { Normal, Loading, Error, Stale };

    explicit StatTile(QWidget* parent = nullptr);
    explicit StatTile(const QString& label, QWidget* parent = nullptr);

    void set_label(const QString& label);
    QString label() const { return label_; }

    void set_value(const QString& text);
    QString value_text() const { return value_; }
    /// Locale-formatted number with `decimals` and an optional suffix.
    void set_number(double value, int decimals = 2, const QString& suffix = {});

    /// Signed change, coloured by the sign of `value` (zero = neutral). With
    /// an empty `text` the value is shown as percent points ("+1.23%").
    void set_delta(double value, const QString& text = {});
    void clear_delta();
    /// Lower is better (volatility, drawdown, cost): a negative delta is
    /// painted with the positive colour and vice versa.
    void set_delta_inverted(bool lower_is_better);

    void set_sub_label(const QString& text);

    void set_sparkline(const QVector<double>& values);
    void set_sparkline(const QVector<double>& values, double baseline);
    void clear_sparkline();

    /// Sparkline colour. Direction (default): the up / down price colours by the
    /// series' last value against its start (or baseline) — for P&L and prices.
    /// Neutral / Accent: one colour whatever the direction — for series that are
    /// not gains or losses (volume, exposure, counts, volatility).
    enum class SparkColor { Direction, Neutral, Accent };
    void set_sparkline_color(SparkColor color);
    SparkColor sparkline_color() const { return spark_color_; }

    void set_tone(Tone tone);
    Tone tone() const { return tone_; }

    void set_state(State state, const QString& reason = {});
    State state() const { return state_; }

    /// Explanation shown on hover (the state reason is appended to it).
    void set_tooltip_text(const QString& text);

    /// Clickable tiles show a pointing cursor, take Tab focus and emit clicked().
    void set_clickable(bool on);
    /// Compact tiles use a smaller value font.
    void set_compact(bool on);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  signals:
    void clicked();

  protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void enterEvent(QEnterEvent* e) override;
    void leaveEvent(QEvent* e) override;
    void changeEvent(QEvent* e) override;

  private:
    struct Metrics;
    Metrics metrics() const;
    /// Width of the left stripe for the current tone (Danger: double).
    int stripe_width(const Metrics& m) const;
    /// Width of the state mark before the label plus its gap (0 unless Danger / Success).
    int mark_width(const Metrics& m) const;
    void refresh_tooltip();
    void content_changed(bool geometry);

    QString label_;
    QString value_;
    QString sub_;
    QString tooltip_;
    QString reason_;
    std::optional<double> delta_;
    QString delta_text_;
    bool delta_inverted_ = false;
    QVector<double> spark_;
    std::optional<double> spark_baseline_;
    SparkColor spark_color_ = SparkColor::Direction;
    Tone tone_ = Tone::Neutral;
    State state_ = State::Normal;
    bool clickable_ = false;
    bool compact_ = false;
    bool hovered_ = false;
    bool pressed_ = false;
};

} // namespace fincept::ui
