// src/screens/equity_research/EquityFinancialsCharts.h
//
// Painted charts of the Financials tab that the shared chart set (src/ui/charts) does not have:
//   FinComboChart     — period bars (grouped and / or stacked) with line series on a primary or
//                       secondary axis and an optional reference (hurdle) line;
//   FinWaterfallChart — a horizontal bridge (income waterfall, cash bridge): start / subtotal /
//                       end bars and signed steps between them;
//   FinStackChart     — 100 % stacked columns, two per period (assets | liabilities + equity);
//   FinDupontView     — the DuPont tree: ROE = net margin × asset turnover × equity multiplier,
//                       each node with its value and a small trend line;
//   FinBulletChart    — rows of "this company vs the peer median" bullets with the peer range.
// Candidates for src/ui/charts once they have settled (they depend on nothing tab-specific).
//
// All derive from ui::PaintedChart (cached static layer + hover overlay, Loading / Empty / Error
// state, theme-change invalidation) and read every colour and font from the live theme at paint
// time: ui::colors tokens, ui::colors::chart_color(slot) for categorical series,
// chartkit::up() / down() (the user's direction palette) for signed steps, chart_style /
// chartkit fonts — never a literal. Hover shows a tooltip with the exact values; the period
// charts emit category_clicked(index) so the tab can select that period everywhere.
// Accessible name / description are set by the caller (set_summary()).
#pragma once

#include "ui/charts/ChartKit.h"
#include "ui/theme/ChartStyle.h"

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <limits>

namespace fincept::screens::fin_charts {

inline constexpr double kChartNaN = std::numeric_limits<double>::quiet_NaN();

// ── Base: period categories along x ─────────────────────────────────────────────────────────

/// Categories (periods) along the x axis, a selected category drawn as a faint accent band with
/// an accent label, hover band + tooltip, click → category_clicked.
class FinChartBase : public ui::PaintedChart {
    Q_OBJECT
  public:
    explicit FinChartBase(QWidget* parent = nullptr);

    /// Period labels ("FY2024") and their tooltips ("Fiscal year ended 30 Sep 2024").
    void set_categories(const QStringList& labels, const QStringList& tooltips = {});
    const QStringList& categories() const { return labels_; }
    /// Highlighted period; -1 = none.
    void set_selected(int index);
    int selected() const { return selected_; }
    /// Accessible description (a one-sentence reading of the chart for screen readers).
    void set_summary(const QString& text);

  signals:
    void category_clicked(int index);

  protected:
    /// The category under `pos` (widget coordinates) for the last painted plot, -1 when none.
    int category_at(const QPointF& pos) const;
    /// x centre and band width of category `i` inside `plot`.
    qreal band_center(const QRectF& plot, int i) const;
    qreal band_width(const QRectF& plot) const;
    /// Draw the category labels under `plot` (thinned when crowded, the selected one in the
    /// accent colour, bold) and the selection band inside `plot`.
    void draw_categories(QPainter& p, const QRectF& plot) const;
    void draw_selection(QPainter& p, const QRectF& plot) const;
    /// Height the category labels need.
    qreal category_axis_height() const;

    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void on_hover_left() override;
    void paint_overlay(QPainter& p, const QRectF& r) override;
    /// Rows of the hover tooltip for category `i` (title = the category label).
    virtual QVector<ui::chartkit::TooltipRow> tooltip_rows(int i) const;

    QStringList labels_;
    QStringList tips_;
    int selected_ = -1;
    int hover_ = -1;
    QRectF plot_; ///< plot rectangle of the last paint (for hit-testing)
};

// ── Combo: bars + lines ──────────────────────────────────────────────────────────────────────

struct ComboBars {
    QString name;
    QVector<double> values; ///< one per category; NaN = no bar
    int color_slot = 0;     ///< ui::colors::chart_color(slot)
    /// Series with the same non-empty key stack on each other (positives up, negatives down);
    /// each distinct key (or each series without one) takes its own slot in the category group.
    QString stack;
};

struct ComboLine {
    QString name;
    QVector<double> values; ///< one per category; NaN = gap
    int color_slot = 0;
    bool secondary = false; ///< right-hand axis
    bool dashed = false;
    bool markers = true;    ///< a dot on each point
};

class FinComboChart : public FinChartBase {
    Q_OBJECT
  public:
    explicit FinComboChart(QWidget* parent = nullptr);

    void set_bars(const QVector<ComboBars>& bars);
    void set_lines(const QVector<ComboLine>& lines);
    /// Formatters of the left (bars and primary lines) and right (secondary lines) axes; also
    /// used in the tooltip. Defaults: compact numbers.
    void set_primary_formatter(Formatter f);
    void set_secondary_formatter(Formatter f);
    /// A dashed horizontal reference across the plot (a hurdle rate), labelled at its right end.
    /// NaN removes it.
    void set_reference_line(double value, const QString& label, bool secondary = false);
    /// Value labels on top of bars when they fit (default off).
    void set_show_values(bool on);
    /// The left axis always includes 0 (bars); a lines-only chart may set this false to zoom.
    void set_zero_based(bool on);
    void clear();

    QSize sizeHint() const override { return {480, 260}; }
    QSize minimumSizeHint() const override { return {200, 140}; }

  protected:
    void paint_chart(QPainter& p, const QRectF& r) override;
    bool has_data() const override;
    QVector<ui::chartkit::TooltipRow> tooltip_rows(int i) const override;

  private:
    QVector<ComboBars> bars_;
    QVector<ComboLine> lines_;
    Formatter primary_fmt_;
    Formatter secondary_fmt_;
    double ref_value_ = kChartNaN;
    QString ref_label_;
    bool ref_secondary_ = false;
    bool show_values_ = false;
    bool zero_based_ = true;
};

// ── Waterfall (horizontal bridge) ────────────────────────────────────────────────────────────

class FinWaterfallChart : public ui::PaintedChart {
    Q_OBJECT
  public:
    enum class Kind { Start, Delta, Subtotal, End };
    struct Step {
        QString label;
        double value = kChartNaN; ///< Start / Subtotal / End: the level; Delta: the signed change
        Kind kind = Kind::Delta;
        QString tip;              ///< extra tooltip line
    };

    explicit FinWaterfallChart(QWidget* parent = nullptr);
    /// Steps top to bottom. Totals are drawn from 0 in the neutral total colour; deltas float
    /// from the running level, POSITIVE (up) / NEGATIVE (down) with a signed value label and a
    /// thin connector to the next bar. A value axis runs along the bottom.
    void set_steps(const QVector<Step>& steps);
    /// Caption drawn dim above the bars ("FY2026"), empty = none.
    void set_caption(const QString& caption);
    void set_summary(const QString& text);
    void clear();

    QSize sizeHint() const override { return {420, 300}; }
    QSize minimumSizeHint() const override { return {220, 160}; }

  protected:
    void paint_chart(QPainter& p, const QRectF& r) override;
    void paint_overlay(QPainter& p, const QRectF& r) override;
    bool has_data() const override { return !steps_.isEmpty(); }
    void mouseMoveEvent(QMouseEvent* e) override;
    void on_hover_left() override;

  private:
    int step_at(const QPointF& pos) const;
    QVector<Step> steps_;
    QString caption_;
    QVector<QRectF> rows_; ///< hit rows of the last paint (full width)
    int hover_ = -1;
};

// ── 100 % stacks: two columns per period ─────────────────────────────────────────────────────

class FinStackChart : public FinChartBase {
    Q_OBJECT
  public:
    struct Segment {
        QString name;
        QVector<double> values; ///< amounts per category (>= 0; NaN / negative = 0)
        int color_slot = 0;
    };

    explicit FinStackChart(QWidget* parent = nullptr);
    /// Two 100 % columns per category, `left` (e.g. "ASSETS") and `right` ("LIABILITIES +
    /// EQUITY"), segments bottom to top. Segment labels ("38 %") inside when they fit; the legend
    /// above lists both sides' segments; hover shows amount and share per segment.
    void set_sides(const QString& left_name, const QVector<Segment>& left, const QString& right_name,
                   const QVector<Segment>& right);
    /// Formats amounts in the tooltip.
    void set_amount_formatter(Formatter f);
    void clear();

    QSize sizeHint() const override { return {480, 280}; }
    QSize minimumSizeHint() const override { return {220, 160}; }

  protected:
    void paint_chart(QPainter& p, const QRectF& r) override;
    bool has_data() const override;
    QVector<ui::chartkit::TooltipRow> tooltip_rows(int i) const override;

  private:
    QString left_name_;
    QString right_name_;
    QVector<Segment> left_;
    QVector<Segment> right_;
    Formatter amount_fmt_;
};

// ── DuPont tree ──────────────────────────────────────────────────────────────────────────────

class FinDupontView : public ui::PaintedChart {
    Q_OBJECT
  public:
    struct Node {
        QString label;        ///< "NET MARGIN"
        QString value_text;   ///< "27.6%"
        QString change_text;  ///< "+0.7 pp YoY" (empty = none)
        int change_sign = 0;  ///< +1 / −1 / 0 → direction colour of change_text (+ arrow)
        QVector<double> trend; ///< a few periods, oldest first (NaN = gap)
        QString tip;          ///< formula
    };

    explicit FinDupontView(QWidget* parent = nullptr);
    /// root = ROE; drivers = net margin, asset turnover, equity multiplier (joined by "×").
    void set_tree(const Node& root, const QVector<Node>& drivers);
    void set_summary(const QString& text);
    void clear();

    QSize sizeHint() const override { return {460, 190}; }
    QSize minimumSizeHint() const override { return {240, 150}; }

  protected:
    void paint_chart(QPainter& p, const QRectF& r) override;
    void paint_overlay(QPainter& p, const QRectF& r) override;
    bool has_data() const override { return !root_.label.isEmpty(); }
    void mouseMoveEvent(QMouseEvent* e) override;
    void on_hover_left() override;

  private:
    Node root_;
    QVector<Node> drivers_;
    QVector<QRectF> boxes_; ///< root first, then drivers (last paint)
    int hover_ = -1;
};

// ── Bullets: company vs peers ────────────────────────────────────────────────────────────────

class FinBulletChart : public ui::PaintedChart {
    Q_OBJECT
  public:
    struct Row {
        QString label;          ///< "Operating margin"
        double value = kChartNaN;  ///< this company
        double median = kChartNaN; ///< peer median
        double lo = kChartNaN;     ///< peer minimum
        double hi = kChartNaN;     ///< peer maximum
        double q1 = kChartNaN;     ///< peer 25th percentile (NaN = not drawn)
        double q3 = kChartNaN;     ///< peer 75th percentile
        bool higher_is_better = true;
        QString value_text;     ///< formatted value
        QString median_text;    ///< formatted median
        QString tip;            ///< definition / peer count
    };

    explicit FinBulletChart(QWidget* parent = nullptr);
    /// One row per metric: label | a track with the peer range (faint), the inter-quartile range
    /// (stronger), a median tick, and this company's marker (a bar from the track start, POSITIVE
    /// when better than the median, NEGATIVE when worse — with "▲ / ▼ vs median" text, never
    /// colour alone) | value and median text. Each row has its own scale.
    void set_rows(const QVector<Row>& rows);
    void set_summary(const QString& text);
    void clear();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return {260, 120}; }

  protected:
    void paint_chart(QPainter& p, const QRectF& r) override;
    void paint_overlay(QPainter& p, const QRectF& r) override;
    bool has_data() const override { return !rows_.isEmpty(); }
    void mouseMoveEvent(QMouseEvent* e) override;
    void on_hover_left() override;

  private:
    QVector<Row> rows_;
    QVector<QRectF> hit_;
    int hover_ = -1;
};

} // namespace fincept::screens::fin_charts
