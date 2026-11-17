#pragma once
// DerivStrategyPage — STRATEGY: multi-leg option positions on the free delayed chain.
// Templates (verticals, straddles, strangles, butterflies, condors, calendars,
// diagonals, ratios, collars, covered calls, protective puts …) or legs sent from the
// chain; an editable legs table (buy / sell, call / put / stock, strike, expiry,
// quantity, price, IV); payoff at the first expiry and at a chosen date, breakevens,
// max profit / loss, probability of profit (lognormal, stated assumptions),
// aggregated Greeks, net debit / credit and a rule-based margin ESTIMATE.
// All maths is local (services/options/StrategyMath) and takes microseconds; live
// trading stays in the F&O tab.

#include "screens/derivatives/DerivCommon.h"
#include "screens/derivatives/DerivContext.h"

#include <QAbstractTableModel>
#include <QHash>
#include <QPointer>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpinBox;
class QTableView;

namespace fincept::ui {
class AutoLoadPane;
class InputMemory;
class KpiStrip;
class SymbolSearchBox;
class TimeSeriesChart;
} // namespace fincept::ui

namespace fincept::screens::deriv {

/// The legs as an editable table: quantity (signed), strike, entry price and IV edit in place.
class DerivLegsModel : public QAbstractTableModel {
    Q_OBJECT
  public:
    enum Col { kActive, kQty, kType, kStrike, kExpiry, kEntry, kIv, kMidNow, kDelta, kCols };
    explicit DerivLegsModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    void set_legs(const QVector<LegView>& legs);
    const QVector<LegView>& legs() const { return legs_; }
    void append(const QVector<LegView>& legs);
    void remove_row(int row);
    void set_chain(const DerivContext::ChainPtr& chain) { chain_ = chain; }
    void set_market(double spot, double r, double q) { spot_ = spot, r_ = r, q_ = q; }
    void refresh_view();

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

  signals:
    void legs_edited();

  private:
    QVector<LegView> legs_;
    DerivContext::ChainPtr chain_;
    double spot_ = 0, r_ = 0, q_ = 0;
};

class DerivStrategyPage : public QWidget {
    Q_OBJECT
  public:
    explicit DerivStrategyPage(DerivContext* ctx, QWidget* parent = nullptr);
    ~DerivStrategyPage() override;

    /// Legs from the chain page (appended; a different underlying starts a new position).
    void add_legs(const QVector<LegView>& legs);
    /// Replace the position with saved legs (from the BOOK or a saved strategy) on `symbol`;
    /// legs whose expiry is no longer listed are left out (and said so).
    void load_legs(const QString& symbol, const QString& name,
                   const QVector<services::options::book::BookLeg>& legs);
    void refresh();

  protected:
    void changeEvent(QEvent* e) override;

  private:
    void build();
    QWidget* build_controls(QWidget* parent);
    QWidget* build_analysis(QWidget* parent);
    void retranslate();
    void on_chain(const DerivContext::ChainPtr& chain);
    void fill_expiries();
    void apply_template();
    void add_leg(int kind);
    void reprice_entries();
    void recompute();
    void save_legs();
    void restore_legs();
    void save_as();
    void reload_saved();
    void fill_saved_menu();
    void add_to_book();
    QVector<services::options::book::BookLeg> book_legs() const;
    services::options::strategy::Spec spec() const;
    services::options::strategy::Market market() const;
    double horizon_vol() const;
    QString ccy() const;
    ui::ChartExport::Table export_table() const;
    report::ReportPayload report_payload() const;

    DerivContext* ctx_ = nullptr;
    TrBook book_;
    ui::InputMemory* memory_ = nullptr;
    DerivContext::ChainPtr chain_;
    QString symbol_ = QStringLiteral("SPY");
    QHash<quint64, quint64> service_ids_;
    bool restoring_ = false;

    ui::SymbolSearchBox* sym_box_ = nullptr;
    QLabel* chain_status_ = nullptr;
    QComboBox* template_ = nullptr;
    QComboBox* expiry_ = nullptr;
    QSpinBox* width_ = nullptr;
    QSpinBox* qty_ = nullptr;
    QPushButton* apply_ = nullptr;
    QTableView* legs_view_ = nullptr;
    DerivLegsModel* legs_ = nullptr;
    QLineEdit* legs_memory_ = nullptr;   ///< hidden: the legs as JSON, through InputMemory
    QLineEdit* symbol_memory_ = nullptr; ///< hidden: the underlying
    QLineEdit* template_memory_ = nullptr;
    QLineEdit* name_memory_ = nullptr;   ///< hidden: the name of a loaded / saved strategy
    QPushButton* saved_btn_ = nullptr;
    QVector<services::options::book::SavedStrategy> saved_;

    ui::AutoLoadPane* pane_ = nullptr;
    ui::KpiStrip* kpis_ = nullptr;
    ui::KpiStrip* greek_kpis_ = nullptr;
    QSlider* target_ = nullptr;
    QLabel* target_label_ = nullptr;
    ui::TimeSeriesChart* chart_ = nullptr;
    QLabel* note_ = nullptr;
    QLabel* status_ = nullptr;
    DerivResultBar* result_bar_ = nullptr;

    // Last analysis, for exports.
    QVector<QPair<QString, QString>> last_rows_;
};

} // namespace fincept::screens::deriv
