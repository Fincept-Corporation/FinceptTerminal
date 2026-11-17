#pragma once
// DerivChainPage — CHAIN & VOL: the free DELAYED listed-option chain of a US stock,
// ETF or index (Cboe delayed quotes, Yahoo Finance as fallback), with
//   * the chain table (calls | strike | puts per expiry, or every contract flat and
//     sortable — virtualised, 30k contracts scroll freely),
//   * SMILE per expiry with its raw-SVI fit, fit quality and no-arbitrage flags,
//   * TERM structure (ATM IV, forward vols) and the expected move per expiry,
//   * SURFACE heatmap (moneyness × expiry),
//   * OPEN INTEREST / VOLUME by strike, put/call ratios and max pain.
// Fetching and all analysis run off the GUI thread (FreeOptionChainService); the
// page shows the last chain at once with its "as of" time and refreshes it.
// Contracts go to the PRICER or the STRATEGY page from the table.

#include "screens/derivatives/DerivCommon.h"
#include "screens/derivatives/DerivContext.h"

#include <QHash>
#include <QPointer>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QStandardItemModel;
class QTableView;

namespace fincept::ui {
class AutoLoadPane;
class BarChart;
class InputMemory;
class KpiStrip;
class MatrixHeatmap;
class SymbolSearchBox;
class TimeSeriesChart;
} // namespace fincept::ui

namespace fincept::screens::deriv {

class DerivChainModel;

class DerivChainPage : public QWidget {
    Q_OBJECT
  public:
    explicit DerivChainPage(DerivContext* ctx, QWidget* parent = nullptr);
    ~DerivChainPage() override;

    /// Reload the chain now (REFRESH / F5).
    void refresh();
    /// Show `symbol`'s chain (from another page).
    void open_symbol(const QString& symbol);

  protected:
    void changeEvent(QEvent* e) override;

  private:
    void build();
    QWidget* build_toolbar();
    QWidget* build_content(QWidget* parent);
    QWidget* build_smile(QWidget* parent);
    QWidget* build_term(QWidget* parent);
    QWidget* build_surface(QWidget* parent);
    QWidget* build_oi(QWidget* parent);
    void retranslate();
    void set_symbol(const QString& symbol, bool run);
    void show_chain(const DerivContext::ChainPtr& chain);
    void on_expiry_changed(int row);
    void set_analysis(int index);
    void update_smile();
    void update_term();
    void update_surface();
    void update_oi();
    void update_header();
    void update_notice();
    void table_menu(const QPoint& pos);
    void table_activated(const QModelIndex& index);
    ContractRef contract_ref(int quote) const;
    LegView leg_view(int quote, double quantity) const;
    ui::ChartExport::Table export_table() const;
    QPixmap export_image() const;
    report::ReportPayload report_payload() const;

    DerivContext* ctx_ = nullptr;
    TrBook book_;
    ui::InputMemory* memory_ = nullptr;
    QString symbol_ = QStringLiteral("SPY");
    DerivContext::ChainPtr chain_;
    QHash<quint64, quint64> service_ids_; ///< pane request id → service request id
    bool showing_saved_ = false;
    QString failure_text_;

    // Toolbar
    ui::SymbolSearchBox* sym_box_ = nullptr;
    QLineEdit* symbol_memory_ = nullptr; ///< hidden: remembers the symbol through InputMemory
    QLineEdit* expiry_memory_ = nullptr; ///< hidden: remembers the expiry date
    QSpinBox* tab_memory_ = nullptr;     ///< hidden: remembers the analysis tab
    QComboBox* source_ = nullptr;
    QPushButton* refresh_ = nullptr;
    QLabel* spot_label_ = nullptr;
    QLabel* source_chip_ = nullptr;
    QLabel* status_ = nullptr;
    DerivResultBar* result_bar_ = nullptr;

    // Content
    ui::AutoLoadPane* pane_ = nullptr;
    QListWidget* expiries_ = nullptr;
    QSpinBox* window_ = nullptr;
    QComboBox* table_mode_ = nullptr;
    QTableView* table_ = nullptr;
    DerivChainModel* model_ = nullptr;
    QList<QPushButton*> tab_btns_;
    QStackedWidget* analysis_ = nullptr;
    int expiry_ = 0;

    // Smile
    ui::KpiStrip* smile_kpis_ = nullptr;
    QComboBox* smile_x_ = nullptr;
    ui::TimeSeriesChart* smile_chart_ = nullptr;
    QLabel* smile_note_ = nullptr;
    // Term
    ui::KpiStrip* term_kpis_ = nullptr;
    ui::TimeSeriesChart* term_chart_ = nullptr;
    QTableView* move_table_ = nullptr;
    QStandardItemModel* move_model_ = nullptr;
    // Surface
    ui::MatrixHeatmap* surface_ = nullptr;
    QLabel* surface_note_ = nullptr;
    // OI
    ui::KpiStrip* oi_kpis_ = nullptr;
    QComboBox* oi_metric_ = nullptr;
    ui::BarChart* oi_chart_ = nullptr;
    QLabel* footer_ = nullptr;
};

} // namespace fincept::screens::deriv
