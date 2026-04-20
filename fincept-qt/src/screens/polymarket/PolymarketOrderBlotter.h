#pragma once

#include "screens/polymarket/ExchangePresentation.h"
#include "services/prediction/PredictionTypes.h"

#include <QTableWidget>
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
class PolymarketOrderBlotter : public QWidget {
    Q_OBJECT
  public:
    explicit PolymarketOrderBlotter(QWidget* parent = nullptr);

    void set_orders(const QVector<fincept::services::prediction::OpenOrder>& orders);
    /// Replace a single row in place by order_id. If the order_id is not
    /// present, appends as a new row. Used by the single-order-refresh flow.
    void update_order(const fincept::services::prediction::OpenOrder& order);
    void clear();

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

  private slots:
    void on_cell_double_clicked(int row, int column);
    void on_cancel_all_clicked();

  private:
    void rebuild_rows();
    void install_row_controls(int row, const fincept::services::prediction::OpenOrder& order);

    QTableWidget* table_ = nullptr;
    QVector<fincept::services::prediction::OpenOrder> orders_;
    ExchangePresentation presentation_ = ExchangePresentation::for_polymarket();
    bool supports_amend_ = false;
    bool supports_batch_cancel_ = false;
};

} // namespace fincept::screens::polymarket
