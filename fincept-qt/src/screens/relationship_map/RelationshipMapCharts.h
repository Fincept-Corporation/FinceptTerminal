// src/screens/relationship_map/RelationshipMapCharts.h
// Small painted charts of the Relationship Map's linked views (the tabs under the graph):
//
//   RelInsiderActivityChart  open-market insider buying (bars up) and selling (bars down) per
//                            calendar month over the last 12 months, by value, with the
//                            cumulative net as a line; hover read-out per month.
//   RelPeerBulletChart       one bullet row per valuation / quality metric: the peers' range as
//                            a band, their median as a tick, every peer as a dot and the company
//                            as an accent marker; hover read-out with the company's rank.
//
// Both derive from ui::PaintedChart: colours and fonts are read from the live theme at paint
// time (the base drops its cached rendering on a theme change), numbers go through
// fincept::ui::formatting, and an empty data set shows the base's designed Empty overlay.
#pragma once

#include "screens/relationship_map/RelationshipMapData.h"
#include "ui/theme/ChartStyle.h"

#include <QDate>
#include <QSize>
#include <QString>
#include <QVector>

#include <limits>

class QMouseEvent;

namespace fincept::relmap {

/// Monthly open-market insider buys and sells (Form 4 "buy" / "sell" trades only — grants,
/// gifts and option exercises are not counted).
class RelInsiderActivityChart : public ui::PaintedChart {
    Q_OBJECT
  public:
    explicit RelInsiderActivityChart(QWidget* parent = nullptr);

    /// Counts the "buy" / "sell" trades dated in the 12 calendar months that end with the month
    /// of `end`, by value (the trade's value, else shares x price) in the currency most of them
    /// are reported in ("" = no currency stated: amounts print without a symbol). Trades in
    /// another currency are left out and counted by excluded_count().
    void set_trades(const QVector<RelInsiderTrade>& trades, const QDate& end);

    QString currency() const { return currency_; }
    /// Trades drawn (buys + sells in the window and the currency).
    int counted_count() const { return counted_; }
    /// Buys / sells in the window left out because their currency differs.
    int excluded_count() const { return excluded_; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  protected:
    void paint_chart(QPainter& p, const QRectF& r) override;
    void paint_overlay(QPainter& p, const QRectF& r) override;
    bool has_data() const override { return counted_ > 0; }
    void on_hover_left() override;
    void mouseMoveEvent(QMouseEvent* e) override;

  private:
    struct Month {
        QDate first; ///< first day of the month
        double buy_value = 0;
        double sell_value = 0;
        double buy_shares = 0;
        double sell_shares = 0;
        int buys = 0;
        int sells = 0;
        double cumulative = 0; ///< net (buys - sells) from the first month through this one
    };
    struct Frame {
        QRectF plot;
        QRectF legend;
        QVector<double> ticks;
        double lo = 0;
        double hi = 1;
    };
    Frame frame(const QRectF& r) const;
    QString axis_text(double v) const;
    int month_at(const QPointF& pos) const;
    QString tooltip_for(int month) const;

    QVector<Month> months_;
    QString currency_;
    int counted_ = 0;
    int excluded_ = 0;
    int hover_ = -1;
};

/// One metric of the peer bullet strip.
struct RelBulletMetric {
    QString label;          ///< "P/E", "Net margin"
    bool fraction = false;  ///< a fraction printed as a percentage; else a multiple ("12.3x")
    double company = std::numeric_limits<double>::quiet_NaN();
    QVector<double> peers;  ///< one value per peer; non-finite entries are ignored
};

/// The company against its peers, one bullet row per metric (two columns when wide enough).
class RelPeerBulletChart : public ui::PaintedChart {
    Q_OBJECT
  public:
    explicit RelPeerBulletChart(QWidget* parent = nullptr);

    /// `company_label` names the company's marker in the read-out (its ticker).
    void set_metrics(const QString& company_label, const QVector<RelBulletMetric>& metrics);
    int metric_count() const { return static_cast<int>(rows_.size()); }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  protected:
    void paint_chart(QPainter& p, const QRectF& r) override;
    void paint_overlay(QPainter& p, const QRectF& r) override;
    bool has_data() const override;
    void on_hover_left() override;
    void mouseMoveEvent(QMouseEvent* e) override;

  private:
    struct Row {
        RelBulletMetric metric;
        QVector<double> sorted; ///< finite peer values, ascending
        double median = std::numeric_limits<double>::quiet_NaN();
    };
    struct Geometry {
        int columns = 1;
        int rows_per_column = 0;
        qreal row_h = 0;
        qreal label_w = 0;
        qreal company_w = 0;
        qreal median_w = 0;
        QRectF area;
    };
    Geometry geometry(const QRectF& r) const;
    QRectF row_rect(const Geometry& g, int index) const;
    QString value_text(const Row& row, double v) const;
    QString median_text(const Row& row) const;
    int row_at(const QPointF& pos) const;
    QString tooltip_for(int index) const;
    static qreal nominal_row_height();

    QString company_;
    QVector<Row> rows_;
    int hover_ = -1;
};

} // namespace fincept::relmap
