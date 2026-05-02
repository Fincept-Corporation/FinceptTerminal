#pragma once

#include "services/prediction/PredictionTypes.h"

#include <QString>
#include <QVector>
#include <QWidget>

class QHideEvent;
class QLabel;
class QPushButton;
class QShowEvent;
class QTableWidget;

namespace fincept::screens::panels {

/// MarketsListPanel — top section of the MARKETS tab (Phase 4 §4.2).
///
/// Lists the curated (or live, if `fincept.markets_endpoint` is configured)
/// prediction markets that settle in $FNCPT. Columns: MARKET, YES, NO,
/// 24h VOL, EXPIRES.
///
/// Wires to `FinceptInternalAdapter::markets_ready` and to the standard
/// error_occurred signal. The status pill on the panel head reads:
///   - "LIVE"  when the adapter has a configured endpoint
///   - "DEMO"  when running on the curated mock dataset
///   - "ERROR" when the most recent fetch surfaced a problem
///
/// Visibility-driven lifecycle (P3): on `showEvent` the panel asks the
/// adapter to `list_markets`; on `hideEvent` it disconnects and clears any
/// pending state. No `QTimer` for refresh — Phase 4's matching engine will
/// provide WS push when it ships.
class MarketsListPanel : public QWidget {
    Q_OBJECT
  public:
    explicit MarketsListPanel(QWidget* parent = nullptr);
    ~MarketsListPanel() override;

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void build_ui();
    void apply_theme();

    void on_markets_ready(
        const QVector<fincept::services::prediction::PredictionMarket>& markets);
    void on_error(const QString& context, const QString& message);
    void on_row_double_clicked(int row, int column);

    void set_status_demo(bool demo);
    void set_status_error(const QString& message);
    void rebuild_table(
        const QVector<fincept::services::prediction::PredictionMarket>& markets);

    QLabel* title_ = nullptr;
    QLabel* status_pill_ = nullptr;
    QLabel* footer_note_ = nullptr;
    QTableWidget* table_ = nullptr;

    bool subscribed_ = false;
};

} // namespace fincept::screens::panels
