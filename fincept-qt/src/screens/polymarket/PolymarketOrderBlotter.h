#pragma once

#include "screens/polymarket/ExchangePresentation.h"
#include "services/prediction/PredictionTypes.h"

#include <QEvent>
#include <QHash>
#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QSet>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVariant>
#include <QVector>
#include <QWidget>

namespace fincept::screens::polymarket {

/// Open-orders blotter with inline controls (amend price / refresh / cancel).
///
/// Consumes unified prediction::OpenOrder so Polymarket and Kalshi render
/// through the same table. Kalshi-only controls (amend, batch cancel) are
/// surfaced via the capabilities advertised through ExchangePresentation and
/// the companion signals; when active adapter is Polymarket the amend
/// column is hidden to avoid offering an unsupported action.
///
/// Where the adapter supports it (Kalshi), a HISTORY tab sits beside OPEN
/// ORDERS with three account lists — settlements, fills, orders — each loaded
/// on first view and paged by cursor ("LOAD MORE"). The blotter only asks for
/// pages (history_requested); the screen fetches them and hands back rows.
class PolymarketOrderBlotter : public QWidget {
    Q_OBJECT
  public:
    explicit PolymarketOrderBlotter(QWidget* parent = nullptr);

    void set_orders(const QVector<fincept::services::prediction::OpenOrder>& orders);
    /// Replace a single row in place by order_id. If the order_id is not
    /// present, appends as a new row. Used by the single-order-refresh flow.
    void update_order(const fincept::services::prediction::OpenOrder& order);
    /// Empties the open orders and every loaded history list.
    void clear();

    // ── Per-row cancel in flight ─────────────────────────────────────────
    /// A cancel was sent for `order_id`: its row says "cancelling…" and its ✕ is
    /// disabled until the exchange answers (set_cancel_done / clear_cancels_pending).
    void set_cancel_pending(const QString& order_id);
    /// The exchange answered the cancel of `order_id`: `cancelled` removes the row,
    /// otherwise the row is restored as it was.
    void set_cancel_done(const QString& order_id, bool cancelled);
    /// Restores every row still waiting for a cancel answer (a failure that names
    /// no order; the caller re-fetches the open orders to reconcile).
    void clear_cancels_pending();

    // ── Account history ──────────────────────────────────────────────────
    // `kind` is "settlements" | "fills" | "orders" — the same strings
    // KalshiAdapter::history_*() return. Rows are the adapter's flat maps.

    /// Show / hide the HISTORY tab. Hiding it while it is open returns to OPEN ORDERS.
    void set_history_supported(bool supported);
    /// Drop every loaded history list (new account or exchange). When the
    /// HISTORY tab is showing, its current list reloads from the first page.
    void reset_history();
    /// One page for `kind`. An empty `request_cursor` replaces the list; otherwise
    /// the rows are appended, provided it is the page last asked for.
    void set_history_page(const QString& kind, const QVariantList& rows, const QString& request_cursor,
                          const QString& next_cursor);
    void set_history_error(const QString& kind, const QString& message);

    /// Exchange presentation drives column-visibility (amend hidden on
    /// Polymarket), price formatting, and accent colour.
    void set_presentation(const ExchangePresentation& p);
    /// `supports_amend` / `supports_batch_cancel` are pulled from the active
    /// adapter's ExchangeCapabilities so the UI reflects reality. Polymarket
    /// today answers false; Kalshi answers true.
    void set_capabilities(bool supports_amend, bool supports_batch_cancel);

  signals:
    /// User clicked the refresh button on a single row — refetch that order.
    void refresh_order(const QString& order_id);
    /// User committed a new limit price on a row (double-click the price
    /// cell, enter a new value). `price` is the raw 0.0–1.0 probability
    /// (Kalshi cents derived from this × 100).
    void amend_order(const QString& order_id, const QString& side, double price);
    /// User clicked the cancel button on a single row.
    void cancel_order(const QString& order_id);
    /// User clicked "CANCEL ALL" — emits every currently-displayed order_id.
    void cancel_all(const QStringList& order_ids);
    /// A history page is wanted: `cursor` empty = first page, else the next page.
    void history_requested(const QString& kind, const QString& cursor);

  protected:
    void changeEvent(QEvent* event) override;

  private slots:
    void on_cell_double_clicked(int row, int column);
    void on_cancel_all_clicked();

  private:
    struct HistoryList {
        QVariantList rows;
        QString next_cursor;    // empty = no further page
        QString pending_cursor; // cursor of the page in flight
        QString error;
        bool loaded = false;
        bool loading = false;
    };

    void rebuild_rows();
    void install_row_controls(int row, const fincept::services::prediction::OpenOrder& order);
    /// Compose the table stylesheet, including object-name rules for the
    /// per-row action buttons so those never need their own setStyleSheet.
    void apply_table_style();
    /// Header-bar stylesheet: OPEN ORDERS / HISTORY tabs and the history list pills.
    void apply_header_style();
    void retranslateUi();

    void show_history(bool on);
    void select_history_kind(const QString& kind);
    void request_history(const QString& kind, const QString& cursor);
    void rebuild_history();
    void set_history_message(const QString& text, bool is_error);
    void sync_header();

    QLabel* title_label_ = nullptr;
    QPushButton* cancel_all_btn_ = nullptr;
    QTableWidget* table_ = nullptr;
    QVector<fincept::services::prediction::OpenOrder> orders_;
    QSet<QString> cancelling_; // order ids with a cancel sent and no answer yet
    ExchangePresentation presentation_ = ExchangePresentation::for_polymarket();
    bool supports_amend_ = false;
    bool supports_batch_cancel_ = false;

    // Account history.
    QWidget* header_bar_ = nullptr;
    QPushButton* orders_tab_btn_ = nullptr;
    QPushButton* history_tab_btn_ = nullptr;
    QWidget* history_kinds_ = nullptr;     // the SETTLEMENTS / FILLS / ORDERS pills
    QList<QPushButton*> history_kind_btns_; // property "kind" = list key
    QLabel* history_count_lbl_ = nullptr;
    QPushButton* load_more_btn_ = nullptr;
    QPushButton* history_refresh_btn_ = nullptr;
    QStackedWidget* stack_ = nullptr; // 0 = open orders, 1 = history
    QTableWidget* history_table_ = nullptr;
    QHash<QString, HistoryList> history_;
    QString history_kind_;
    bool history_supported_ = false;
    bool history_shown_ = false;
};

} // namespace fincept::screens::polymarket
