#pragma once
// DerivBookPage — BOOK: the saved position book of the Derivatives tab (D7).
//
//   * Positions (single options, stock, futures, swaps, named strategies) with legs,
//     entry prices, quantity and notes, stored in SQLite (migration v065) through
//     services/options/DerivBookStore — every statement off the GUI thread.
//   * Live marks from the free DELAYED chains (FreeOptionChainService's cache: memory,
//     then the saved copy, then a fetch), spots for stock-only underlyings
//     (MarketDataService), futures months (CurveDataService) and the Treasury zero
//     curve for swaps; each row shows where its mark came from and "as of" when.
//     No network storm: marks refresh on a timer ONLY while the page is visible, for
//     the underlyings of the rows in view, at most kMaxPerTick per tick, one fetch at
//     a time; REFRESH MARKS refreshes the whole book, still one at a time.
//   * Valuation (services/options/BookMath: P&L since entry, delta / gamma / vega /
//     theta / rho and dollar Greeks per position, totals, exposure by underlying,
//     expiry ladder) runs on a worker with a request id; stale results are dropped.
//   * Hand-offs: open a position in STRATEGY or SCENARIOS, price a single option in the
//     PRICER; positions arrive from the chain, the STRATEGY page and CURVES through
//     DerivContext::book_add_requested. CSV / PNG / Add to Report through the result bar.

#include "screens/derivatives/DerivCommon.h"
#include "screens/derivatives/DerivContext.h"
#include "services/options/BookMath.h"

#include <QAbstractTableModel>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QWidget>

#include <atomic>
#include <memory>

class QComboBox;
class QLabel;
class QLineEdit;
class QMenu;
class QSpinBox;
class QPushButton;
class QSortFilterProxyModel;
class QStackedWidget;
class QStandardItemModel;
class QTableView;
class QTimer;

namespace fincept::ui {
class BarChart;
class EmptyState;
class InputMemory;
class KpiStrip;
class LoadingSkeleton;
} // namespace fincept::ui

namespace fincept::screens::deriv {

/// The positions table: one row per position, its marks and Greeks once valued.
/// Qt::DisplayRole = formatted text, Qt::UserRole = the number (sorting, exports).
class DerivBookModel : public QAbstractTableModel {
    Q_OBJECT
  public:
    enum Col {
        kName, kKind, kUnderlying, kDetail, kEntry, kMark, kValue, kPnl, kDelta, kDollarDelta, kGamma, kDollarGamma,
        kVega, kTheta, kRho, kExpiry, kMarked, kNotes, kCols
    };
    explicit DerivBookModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    void set_positions(const QVector<services::options::book::BookPosition>& positions);
    /// Results in the order of the positions they were computed for (matched by id).
    void set_results(const QVector<services::options::book::PositionResult>& results);
    const QVector<services::options::book::BookPosition>& positions() const { return rows_; }
    const services::options::book::BookPosition* position(int row) const;
    const services::options::book::PositionResult* result(int row) const;
    int row_of(const QString& id) const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    /// Plain-language name of a mark source ("chain mid", "model at chain IV" …).
    static QString source_text(services::options::book::MarkSource s);
    static QString kind_text(const QString& kind);
    static QString detail_text(const services::options::book::BookPosition& p);

  private:
    QVector<services::options::book::BookPosition> rows_;
    QHash<QString, services::options::book::PositionResult> results_;
};

class DerivBookPage : public QWidget {
    Q_OBJECT
  public:
    explicit DerivBookPage(DerivContext* ctx, QWidget* parent = nullptr);
    ~DerivBookPage() override;

    /// REFRESH MARKS / F5: re-mark the whole book (one fetch at a time).
    void refresh();
    /// Open the add dialog on a draft (from the chain, the STRATEGY page or CURVES).
    void add_draft(const services::options::book::BookPosition& draft);

    static constexpr int kMaxPerTick = 3;      ///< underlyings fetched per timer tick
    static constexpr int kTickMs = 60 * 1000;  ///< timer period while visible
    static constexpr int kFreshSeconds = 300;  ///< an underlying fetched more recently is not fetched again by the timer

  protected:
    void changeEvent(QEvent* e) override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    // Build
    void build();
    QWidget* build_toolbar();
    QWidget* build_table(QWidget* parent);
    QWidget* build_detail(QWidget* parent);
    QWidget* ensure_detail_view(int index);
    void set_detail_view(int index);
    void retranslate();
    /// Column widths and row heights from the current font (again after every appearance change).
    void refit();

    // Data
    void reload_positions();
    void reload_books();
    void show_state();
    void edit_position(const services::options::book::BookPosition& p, bool is_new);
    void save_position(const services::options::book::BookPosition& p);
    void delete_position(const QString& id);
    void add_menu(QMenu* menu);

    // Marks
    QStringList keys_in_view() const;
    QStringList keys_all() const;
    void enqueue(const QStringList& keys, bool force);
    void pump();
    void fetch_option_key(const QString& key, bool force);
    void fetch_quote_keys(const QStringList& keys);
    void fetch_future_root(const QString& key, bool force);
    void ensure_curve();
    void on_tick();
    void cancel_marking();
    void schedule_revalue();
    void revalue();
    void apply_values(const QVector<services::options::book::PositionResult>& results,
                      const services::options::book::BookSummary& summary);
    void update_kpis();
    void update_detail();
    void update_status();

    // Hand-offs
    void row_menu(const QPoint& pos);
    void open_in_strategy(const services::options::book::BookPosition& p);
    void open_in_scenarios(const QVector<services::options::book::BookPosition>& positions, const QString& label);
    void price_in_pricer(const services::options::book::BookPosition& p);

    // Export
    ui::ChartExport::Table export_table() const;
    QPixmap export_image() const;
    report::ReportPayload report_payload() const;

    DerivContext* ctx_ = nullptr;
    TrBook book_;
    ui::InputMemory* memory_ = nullptr;

    // Toolbar
    QComboBox* book_combo_ = nullptr;
    QPushButton* add_btn_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QLabel* status_ = nullptr;
    DerivResultBar* result_bar_ = nullptr;
    QLineEdit* book_memory_ = nullptr; ///< hidden: remembers the book in view through InputMemory
    QSpinBox* detail_memory_ = nullptr; ///< hidden: remembers the detail view
    QSpinBox* exposure_metric_memory_ = nullptr; ///< hidden: the lazily built views' metric choices
    QSpinBox* ladder_metric_memory_ = nullptr;

    // Content
    QStackedWidget* states_ = nullptr; ///< 0 loading, 1 content, 2 empty / error
    ui::LoadingSkeleton* skeleton_ = nullptr;
    ui::EmptyState* empty_ = nullptr;
    bool empty_is_error_ = false;
    ui::KpiStrip* kpis_ = nullptr;
    QTableView* table_ = nullptr;
    DerivBookModel* model_ = nullptr;
    QSortFilterProxyModel* proxy_ = nullptr;
    QList<QPushButton*> detail_btns_;
    QStackedWidget* detail_ = nullptr;
    QVector<QWidget*> detail_views_;
    QStandardItemModel* exposure_model_ = nullptr;
    QStandardItemModel* ladder_model_ = nullptr;
    QStandardItemModel* legs_model_ = nullptr;
    QComboBox* exposure_metric_ = nullptr;
    QComboBox* ladder_metric_ = nullptr;
    ui::BarChart* exposure_chart_ = nullptr;
    ui::BarChart* ladder_chart_ = nullptr;
    QLabel* legs_caption_ = nullptr;
    QLabel* note_ = nullptr;

    // State
    QVector<services::options::book::BookPosition> positions_;
    QStringList books_;
    bool loaded_ = false;
    bool loading_ = false;
    QString load_error_;
    services::options::book::MarketView market_;
    services::options::book::BookSummary summary_;
    QHash<QString, QDateTime> fetched_at_;  ///< key → when its marks were last requested (UTC)
    QHash<QString, QString> failures_;      ///< key → last failure sentence
    QStringList queue_;
    QSet<QString> force_keys_;
    QString in_flight_;
    quint64 chain_req_ = 0;
    quint64 future_req_ = 0;
    quint64 curve_req_ = 0;
    int tick_offset_ = 0;
    QTimer* tick_ = nullptr;
    QTimer* revalue_timer_ = nullptr;
    quint64 value_id_ = 0;
    std::shared_ptr<std::atomic<bool>> value_cancel_;
    QDateTime valued_at_;
    bool pending_draft_ = false;
    services::options::book::BookPosition draft_;
};

} // namespace fincept::screens::deriv
