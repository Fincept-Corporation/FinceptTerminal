#pragma once
// AltPrivateMarketsPanel — the Alternative Investments PRIVATE MARKETS workbench (package A3).
//
// Left: the fund book (ADD / EDIT / DELETE, and ADD EXAMPLE FUND on an empty book) and the selected
// fund's ledger (ui::DataGridEditor: date, kind call / distribution / NAV / fee, amount, note; paste
// and CSV import), saved to the local database as it is edited (services::alt::AltFundStore).
// Right, four views over the same ledger, each a ui::AutoLoadPane:
//   PERFORMANCE  IRR / TVPI / DPI / RVPI / paid-in / unfunded, the J-curve and year-end figures
//                (local; runs on first show, again after the ledger is saved)
//   PME          the fund against a public index picked with the symbol box (Yahoo prices: never
//                automatic — the last result with its "as of", else a preview with RUN)
//   PACING       Takahashi-Alexander commitment model (local; inputs remembered)
//   BOOK         every fund pooled, by strategy and vintage (local; refreshed when the book changes)
// Python through services::alt::AltInvestmentsService (private_markets.py); a reply is drawn only
// under the view and fund that asked for it (the panes' request ids); leaving a view cancels its run.

#include "services/alt_investments/AltFundStore.h"
#include "screens/alt_investments/AltWorkbenchKit.h"

#include <QHash>
#include <QJsonObject>
#include <QPointer>
#include <QWidget>

class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTabWidget;
class QTableView;
class QTimer;

namespace fincept::ui {
class AutoLoadPane;
class BarChart;
class DataGridEditor;
class DonutChart;
class EmptyState;
class InputMemory;
class KpiStrip;
class SymbolSearchBox;
class TimeSeriesChart;
} // namespace fincept::ui

namespace fincept::screens::alt {

class AltPrivateMarketsPanel : public QWidget {
    Q_OBJECT
  public:
    explicit AltPrivateMarketsPanel(QWidget* parent = nullptr);
    ~AltPrivateMarketsPanel() override;

  protected:
    void showEvent(QShowEvent* e) override;

  private:
    // ── Build ───────────────────────────────────────────────────────────────
    void build_ui();
    QWidget* build_book_side(QWidget* parent);
    QWidget* build_performance_page(QWidget* parent);
    QWidget* build_pme_page(QWidget* parent);
    QWidget* build_pacing_page(QWidget* parent);
    QWidget* build_book_page(QWidget* parent);
    /// A pane whose content is a scroll area; returns the scroll body.
    QWidget* make_pane_body(fincept::ui::AutoLoadPane* pane);

    // ── Fund book ───────────────────────────────────────────────────────────
    void reload_funds(const QString& select_id = QString());
    void show_funds();
    void select_fund(const QString& id);
    const fincept::services::alt::AltFund* current_fund() const;
    void add_fund();
    void add_example_fund();
    void edit_fund();
    void delete_fund();
    /// Fund dialog; false when cancelled.
    bool edit_fund_dialog(fincept::services::alt::AltFund* fund, const QString& title);
    void on_ledger_changed();
    void flush_ledger_save();
    void set_ledger_status(const QString& text, bool error = false);

    // ── Views ───────────────────────────────────────────────────────────────
    QJsonObject ledger_input() const;
    QString ledger_signature() const;
    void update_views();
    void start_performance(quint64 id);
    void render_performance(const QJsonValue& payload);
    void start_pme(quint64 id);
    void render_pme(const QJsonValue& payload);
    QString pme_symbol() const;
    void configure_pme_preview();
    void start_pacing(quint64 id);
    void render_pacing(const QJsonValue& payload);
    QJsonObject pacing_input(QString* problem) const;
    void start_book(quint64 id);
    void render_book(const QJsonValue& payload);
    void cancel_service(quint64 pane_request);

    QString currency() const;

    // ── State ───────────────────────────────────────────────────────────────
    QVector<fincept::services::alt::AltFund> funds_;
    QString current_id_;
    QString loaded_ledger_id_; ///< whose flows are in the grid
    bool funds_loaded_ = false;
    bool loading_ledger_ = false;
    bool ledger_dirty_ = false;
    quint64 ledger_request_ = 0; ///< latest load_flows (a slower, older reply is dropped)
    QString store_error_;
    QHash<quint64, quint64> service_ids_; ///< pane request id -> AltInvestmentsService request id
    bool started_ = false;

    // ── Widgets: book side ─────────────────────────────────────────────────
    AltWbRowsModel* fund_model_ = nullptr;
    QTableView* fund_view_ = nullptr;
    QPushButton* add_btn_ = nullptr;
    QPushButton* edit_btn_ = nullptr;
    QPushButton* delete_btn_ = nullptr;
    QPushButton* example_btn_ = nullptr;
    QLabel* book_hint_ = nullptr;
    QLabel* ledger_title_ = nullptr;
    QLabel* ledger_status_ = nullptr;
    fincept::ui::DataGridEditor* ledger_ = nullptr;
    QTimer* save_timer_ = nullptr;

    QTabWidget* tabs_ = nullptr;

    // Performance
    AltWbActions perf_actions_;
    fincept::ui::AutoLoadPane* perf_pane_ = nullptr;
    QStackedWidget* perf_stack_ = nullptr;
    fincept::ui::EmptyState* perf_empty_ = nullptr;
    fincept::ui::KpiStrip* perf_kpis_ = nullptr;
    QLabel* perf_notes_ = nullptr;
    fincept::ui::TimeSeriesChart* jcurve_ = nullptr;
    AltWbRowsModel* perf_model_ = nullptr;
    QTableView* perf_table_ = nullptr;
    AltWbExport perf_export_;

    // PME
    AltWbActions pme_actions_;
    fincept::ui::SymbolSearchBox* pme_index_ = nullptr;
    QPushButton* pme_run_ = nullptr;
    fincept::ui::AutoLoadPane* pme_pane_ = nullptr;
    QStackedWidget* pme_stack_ = nullptr;
    fincept::ui::EmptyState* pme_empty_ = nullptr;
    fincept::ui::KpiStrip* pme_kpis_ = nullptr;
    QLabel* pme_notes_ = nullptr;
    fincept::ui::TimeSeriesChart* pme_chart_ = nullptr;
    AltWbRowsModel* pme_model_ = nullptr;
    QTableView* pme_table_ = nullptr;
    AltWbExport pme_export_;

    // Pacing
    AltWbActions pace_actions_;
    QDoubleSpinBox* pace_commitment_ = nullptr;
    QSpinBox* pace_life_ = nullptr;
    QLineEdit* pace_rates_ = nullptr;
    QDoubleSpinBox* pace_bow_ = nullptr;
    QDoubleSpinBox* pace_growth_ = nullptr;
    QDoubleSpinBox* pace_yield_ = nullptr;
    QSpinBox* pace_vintages_ = nullptr;
    QPushButton* pace_run_ = nullptr;
    fincept::ui::InputMemory* pace_memory_ = nullptr;
    fincept::ui::AutoLoadPane* pace_pane_ = nullptr;
    fincept::ui::KpiStrip* pace_kpis_ = nullptr;
    fincept::ui::BarChart* pace_chart_ = nullptr;
    AltWbRowsModel* pace_model_ = nullptr;
    QTableView* pace_table_ = nullptr;
    QLabel* pace_notes_ = nullptr;
    AltWbExport pace_export_;

    // Book
    AltWbActions book_actions_;
    fincept::ui::AutoLoadPane* book_pane_ = nullptr;
    QStackedWidget* book_stack_ = nullptr;
    fincept::ui::EmptyState* book_empty_ = nullptr;
    fincept::ui::KpiStrip* book_kpis_ = nullptr;
    fincept::ui::DonutChart* book_strategy_ = nullptr;
    fincept::ui::DonutChart* book_vintage_ = nullptr;
    AltWbRowsModel* book_model_ = nullptr;
    QTableView* book_table_ = nullptr;
    QLabel* book_notes_ = nullptr;
    AltWbExport book_export_;
};

} // namespace fincept::screens::alt
