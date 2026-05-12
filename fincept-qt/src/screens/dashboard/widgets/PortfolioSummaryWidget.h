#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "screens/portfolio/PortfolioTypes.h"
#include "services/markets/MarketDataService.h"

#include <QGridLayout>
#include <QHash>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens::widgets {

/// Portfolio Summary Widget — reads from the v006 multi-portfolio store
/// (`portfolios` + `portfolio_assets`) shared with the Portfolio screen, so
/// holdings added/sold in the Portfolio tab show up here automatically. A
/// gear icon in the title bar lets the user pick which portfolio this tile
/// displays; the choice is persisted via the dashboard canvas's config
/// round-trip. Live quotes still come through DataHub `market:quote:<sym>`
/// subscriptions, one per holding, so P&L updates in real time.
class PortfolioSummaryWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit PortfolioSummaryWidget(QWidget* parent = nullptr);

    struct Holding {
        QString symbol;
        double shares = 0;
        double avg_cost = 0;
    };

    // ── BaseWidget config hooks ─────────────────────────────────────────────
    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    QDialog* make_config_dialog(QWidget* parent) override;

  private:
    void apply_styles();

    /// Resolve which portfolio to display, then load its assets. Picks the
    /// first available portfolio when `selected_portfolio_id_` is empty or
    /// no longer exists. Renders the empty-state when zero portfolios.
    void load_holdings();

    /// Re-subscribe to `market:quote:<sym>` for every holding. Drops old
    /// subscriptions first — holdings set may have changed since last call.
    void hub_resubscribe(const QVector<Holding>& holdings);
    void hub_unsubscribe_all();

    /// Rebuild quotes vector from `row_cache_` in `last_holdings_` order
    /// and re-render.
    void rebuild_from_cache();

    void render(const QVector<Holding>& holdings, const QVector<services::QuoteData>& quotes);

    /// Render the "no portfolios / no holdings" placeholder + zero out the
    /// summary metric cards. Idempotent.
    void render_empty(const QString& message);

    /// Pull the current list of portfolios from `PortfolioRepository` and
    /// cache it for the picker dialog. Cheap (a single SELECT).
    void refresh_portfolio_cache();

    // ── Selected portfolio ─────────────────────────────────────────────────
    QString selected_portfolio_id_;
    QString selected_portfolio_name_;
    QVector<portfolio::Portfolio> portfolio_cache_;

    // ── Summary labels ──────────────────────────────────────────────────────
    QLabel* total_value_lbl_ = nullptr;
    QLabel* day_pnl_lbl_ = nullptr;
    QLabel* total_pnl_lbl_ = nullptr;
    QLabel* num_holdings_lbl_ = nullptr;

    // ── Holdings list layout ────────────────────────────────────────────────
    QVBoxLayout* list_layout_ = nullptr;

    // ── Theme-tracked widgets ──────────────────────────────────────────────
    QWidget* summary_card_ = nullptr;
    QWidget* header_row_ = nullptr;
    QScrollArea* scroll_area_ = nullptr;
    QLabel* portfolio_name_lbl_ = nullptr;
    QVector<QLabel*> metric_labels_;
    QVector<QLabel*> metric_values_;
    QVector<QLabel*> header_labels_;

    // ── Cached for theme-change re-render ──────────────────────────────────
    QVector<Holding> last_holdings_;
    QVector<services::QuoteData> last_quotes_;

    QHash<QString, services::QuoteData> row_cache_;
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
