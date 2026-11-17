#pragma once
// ScenarioGrid — named scenarios as rows, each with editable shock inputs, and the
// metrics the host computes for them as further columns (cross-tab package X3).
// Derivatives stress tests (spot / vol / rate / days shocks → P&L, Greeks), an
// allocation stress, a case comparison. For a two-axis numeric grid (WACC × growth,
// spot × vol) use ui::SensitivityTable instead.
//
//   auto* g = new ui::ScenarioGrid(parent);
//   g->set_columns({{tr("Spot %"), 1}, {tr("Vol pts"), 1}},                       // shock inputs
//                  {{tr("P&L"), money_fmt, true}, {tr("Delta"), num_fmt}});       // computed metrics
//   g->set_rows({{tr("Base"), {0, 0}, {}, true}, {tr("Crash"), {-20, 25}}});
//   connect(g, &ui::ScenarioGrid::scenarios_edited, this, [=] { recompute(g->scenarios()); });
//   g->set_metric_values(row, {pnl, delta});                                       // after computing
//
// Names and shocks edit in place (F2 / double-click / typing); ADD, DUPLICATE and DELETE
// manage rows (the base row stays). Signed-money metrics read in the theme's positive /
// negative colours, every other number in the primary text colour. headers() / rows() /
// to_tsv() give the formatted table for Copy, CSV and Send to Report. Theme tokens only;
// row height follows the font (re-fitted on ThemeManager::appearance_settled).

#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <functional>

class QPushButton;
class QTableView;

namespace fincept::ui {

class ScenarioGridModel;

struct ScenarioShockColumn {
    QString title;
    int decimals = 2;
    QString tooltip;
};

struct ScenarioMetricColumn {
    QString title;
    std::function<QString(double)> format; ///< empty = locale number, 2 decimals
    bool signed_money = false;             ///< colour by sign (P&L); else neutral text
    QString tooltip;
};

struct ScenarioRow {
    QString name;
    QVector<double> shocks;  ///< one per shock column
    QVector<double> metrics; ///< one per metric column; NaN = not computed
    bool base = false;       ///< the reference row (bold, cannot be deleted)
};

class ScenarioGrid : public QWidget {
    Q_OBJECT
  public:
    explicit ScenarioGrid(QWidget* parent = nullptr);
    ~ScenarioGrid() override;

    void set_columns(const QVector<ScenarioShockColumn>& shocks, const QVector<ScenarioMetricColumn>& metrics);
    void set_rows(const QVector<ScenarioRow>& rows);
    /// Current rows (names and shocks as edited; metrics as last set).
    QVector<ScenarioRow> scenarios() const;
    void set_metric_values(int row, const QVector<double>& values);
    /// Names / shocks editable and the ADD / DUPLICATE / DELETE buttons shown. Default true.
    void set_editable(bool on);

    QStringList headers() const;
    QVector<QStringList> rows() const;
    QString to_tsv() const;

  signals:
    /// A name or shock was edited, or rows were added / removed: recompute the metrics.
    void scenarios_edited();
    void row_activated(int row);

  protected:
    void changeEvent(QEvent* e) override;

  private:
    void retranslate();
    void refit();

    ScenarioGridModel* model_ = nullptr;
    QTableView* table_ = nullptr;
    QWidget* buttons_ = nullptr;
    QPushButton* add_ = nullptr;
    QPushButton* dup_ = nullptr;
    QPushButton* del_ = nullptr;
};

} // namespace fincept::ui
