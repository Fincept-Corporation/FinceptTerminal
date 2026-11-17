#pragma once
// MonthlyReturnsHeatmap.h — years × months return table with a compounded
// year-total column, coloured on the theme's diverging scale with a robust
// (percentile) clamp so one outlier month does not wash out the rest. Accepts
// monthly returns directly or aggregates an equity curve / daily returns.
// All returns are in percent units (1.5 = +1.5 %).

#include "ui/theme/ChartStyle.h"

#include <QDate>
#include <QMap>
#include <QPair>
#include <QVector>

namespace fincept::ui {

class MonthlyReturnsHeatmap : public PaintedChart {
    Q_OBJECT
  public:
    /// year → month (1..12) → return in percent.
    using MonthlyReturns = QMap<int, QMap<int, double>>;

    explicit MonthlyReturnsHeatmap(QWidget* parent = nullptr);

    void set_monthly_returns(const MonthlyReturns& returns);
    /// Equity / NAV series (any frequency, any order). A month's return is its
    /// last value over the previous month's last value; the first month is
    /// measured from the first value. Non-positive values are ignored.
    void set_equity_curve(const QVector<QPair<QDate, double>>& equity);
    /// Periodic (e.g. daily) returns in percent, compounded within each month.
    void set_daily_returns(const QVector<QPair<QDate, double>>& returns_pct);
    void clear();
    const MonthlyReturns& monthly_returns() const { return data_; }

    /// The colour clamp is this percentile (0.5..1) of |monthly return|;
    /// year totals get their own percentile clamp. Default 0.9.
    void set_clamp_percentile(double p);
    /// A fixed clamp in percent for the month cells (> 0); 0 returns to the
    /// percentile clamp.
    void set_fixed_clamp(double clamp_pct);
    /// The compounded "Year" column. Default true.
    void set_show_year_total(bool show);

    /// The aggregations used by the setters, for callers that need the numbers.
    static MonthlyReturns monthly_from_equity(const QVector<QPair<QDate, double>>& equity);
    static MonthlyReturns monthly_from_daily_returns(const QVector<QPair<QDate, double>>& returns_pct);
    /// Compounded total of one year's months, in percent.
    static double compound_pct(const QMap<int, double>& months);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  signals:
    /// `month` is 1..12, or 0 for the year-total cell.
    void cell_clicked(int year, int month);

  protected:
    void paint_chart(QPainter& p, const QRectF& r) override;
    void paint_overlay(QPainter& p, const QRectF& r) override;
    QString default_format(double v) const override;
    bool has_data() const override { return !data_.isEmpty(); }
    void on_hover_left() override;

    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;

  private:
    struct Geometry {
        QRectF header;  // month names row
        qreal year_w = 0;
        qreal cell_w = 0;
        qreal cell_h = 0;
        qreal total_gap = 0;
        qreal left = 0;
        qreal top = 0; // first data row
    };
    Geometry geometry(const QRectF& r) const;
    QRectF cell_rect(const Geometry& g, int row, int month) const; // month 0 = total
    bool hit(const QPointF& pos, int* row, int* month) const;
    void recompute();

    MonthlyReturns data_;
    QMap<int, double> totals_;
    QVector<int> years_;
    double percentile_ = 0.9;
    double fixed_clamp_ = 0;
    double clamp_ = 1;
    double total_clamp_ = 1;
    bool show_total_ = true;
    int hover_row_ = -1;
    int hover_month_ = -1;
};

} // namespace fincept::ui
