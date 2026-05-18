// src/screens/portfolio/PortfolioScreen_Handlers.cpp
//
// Service-event slots (on_portfolios_loaded / on_summary_loaded / on_metrics_computed
// / on_snapshots_loaded / on_portfolio_created/deleted / on_asset_changed),
// user-action slots (create / delete / detail_view / refresh interval /
// request_refresh / symbol_selected / buy / sell / order_panel close / animate /
// reposition), and load_demo_portfolio.
//
// Part of the partial-class split of PortfolioScreen.cpp.

#include "screens/portfolio/PortfolioScreen.h"

#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "core/symbol/SymbolRef.h"
#include "screens/portfolio/PortfolioBlotter.h"
#include "screens/portfolio/PortfolioCommandBar.h"
#include "screens/portfolio/PortfolioDetailWrapper.h"
#include "screens/portfolio/PortfolioDialogs.h"
#include "screens/portfolio/PortfolioFFNView.h"
#include "screens/portfolio/PortfolioHeatmap.h"
#include "screens/portfolio/PortfolioInsightsPanel.h"
#include "screens/portfolio/PortfolioOrderPanel.h"
#include "screens/portfolio/PortfolioPanelHeader.h"
#include "screens/portfolio/PortfolioPerfChart.h"
#include "screens/portfolio/PortfolioSectorPanel.h"
#include "screens/portfolio/PortfolioStatsRibbon.h"
#include "screens/portfolio/PortfolioStatusBar.h"
#include "screens/portfolio/PortfolioTxnPanel.h"
#include "services/file_manager/FileManagerService.h"
#include "services/portfolio/PortfolioService.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/theme/Theme.h"

#include <QFileDialog>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSplitter>
#include <QVBoxLayout>

#include <memory>

namespace fincept::screens {

void PortfolioScreen::on_portfolios_loaded(QVector<portfolio::Portfolio> portfolios) {
    portfolios_ = portfolios;
    command_bar_->set_portfolios(portfolios);

    if (portfolios.isEmpty()) {
        command_bar_->set_has_portfolios(false);
        selected_id_.clear();
        summary_loaded_ = false;
    } else {
        command_bar_->set_has_portfolios(true);
        // Auto-select first portfolio if none selected
        if (selected_id_.isEmpty()) {
            on_portfolio_selected(portfolios.first().id);
        }
    }
    update_content_state();
}

void PortfolioScreen::on_portfolio_selected(const QString& id) {
    if (id == selected_id_ && summary_loaded_)
        return;

    selected_id_ = id;
    ScreenStateManager::instance().notify_changed(this);
    summary_loaded_ = false;
    active_detail_ = std::nullopt;
    command_bar_->set_detail_view(std::nullopt);
    if (txn_panel_)
        txn_panel_->clear();
    if (blotter_)
        blotter_->set_sector_filter({});

    // Find portfolio and update UI
    for (const auto& p : portfolios_) {
        if (p.id == id) {
            command_bar_->set_selected_portfolio(p);
            status_bar_->set_portfolio_name(p.name);
            break;
        }
    }

    update_content_state();
    services::PortfolioService::instance().load_summary(id);
}

void PortfolioScreen::on_summary_loaded(portfolio::PortfolioSummary summary) {
    if (summary.portfolio.id != selected_id_)
        return; // Stale response

    current_summary_ = summary;
    summary_loaded_ = true;

    command_bar_->set_summary(summary);
    command_bar_->set_refreshing(false);
    stats_ribbon_->set_summary(summary);
    status_bar_->set_summary(summary);

    update_main_view_data();
    update_content_state();

    // Auto-select highest weighted holding if none selected
    if (selected_symbol_.isEmpty() && !summary.holdings.isEmpty()) {
        double max_w = -1;
        QString top;
        for (const auto& h : summary.holdings) {
            if (h.weight > max_w) {
                max_w = h.weight;
                top = h.symbol;
            }
        }
        on_symbol_selected(top);
    }

    // Trigger metrics computation
    services::PortfolioService::instance().compute_metrics(summary);

    // Load performance history for the chart
    services::PortfolioService::instance().load_snapshots(summary.portfolio.id);

    // Load recent transactions for the history panel
    if (txn_panel_)
        services::PortfolioService::instance().load_transactions(summary.portfolio.id, 50);

    // Fetch real 30-day correlation for the sector panel
    if (!summary.holdings.isEmpty()) {
        QStringList syms;
        for (const auto& h : summary.holdings)
            syms.append(h.symbol);
        services::PortfolioService::instance().fetch_correlation(syms);
    }

    // Fetch benchmark history for perf chart overlay. Use the portfolio's
    // currency to pick a sensible default index (TSX for CAD, SPY for USD,
    // FTSE for GBP, …). We also always fetch SPY itself because Beta in
    // compute_metrics() regresses against SPY regardless of currency.
    {
        auto& svc = services::PortfolioService::instance();
        const QString bench = services::PortfolioService::default_benchmark_for_currency(
            summary.portfolio.currency);
        svc.fetch_benchmark_history(bench, "1y");
        if (bench != QStringLiteral("SPY"))
            svc.fetch_benchmark_history("SPY", "1y");
    }

    // Fetch live risk-free rate (DGS10) for Sharpe computation — cached 24h
    services::PortfolioService::instance().fetch_risk_free_rate();
}

void PortfolioScreen::on_summary_error(QString portfolio_id, QString /*error*/) {
    if (portfolio_id != selected_id_)
        return;
    // Show empty state with error — for now just revert to empty
    summary_loaded_ = false;
    update_content_state();
}

void PortfolioScreen::on_metrics_computed(portfolio::ComputedMetrics metrics) {
    current_metrics_ = metrics;
    stats_ribbon_->set_metrics(metrics);
    if (heatmap_)
        heatmap_->set_metrics(metrics);
    if (detail_wrapper_)
        detail_wrapper_->update_metrics(metrics);
}

void PortfolioScreen::on_snapshots_loaded(QString portfolio_id, QVector<portfolio::PortfolioSnapshot> snapshots) {
    if (portfolio_id != selected_id_)
        return;
    if (perf_chart_)
        perf_chart_->set_snapshots(snapshots);
    if (detail_wrapper_)
        detail_wrapper_->update_snapshots(snapshots);
}

void PortfolioScreen::on_portfolio_created(portfolio::Portfolio portfolio) {
    // portfolios_loaded fires asynchronously after creation, so the new portfolio
    // may not be in portfolios_ yet when on_portfolio_selected looks it up.
    // Append it immediately so the selector label and status bar show the correct
    // name without waiting for the reload round-trip.
    bool already_present = false;
    for (const auto& p : portfolios_) {
        if (p.id == portfolio.id) {
            already_present = true;
            break;
        }
    }
    if (!already_present)
        portfolios_.append(portfolio);

    on_portfolio_selected(portfolio.id);
}

void PortfolioScreen::on_portfolio_deleted(QString id) {
    if (id == selected_id_) {
        selected_id_.clear();
        summary_loaded_ = false;
        update_content_state();
    }
}

void PortfolioScreen::on_asset_changed(QString portfolio_id) {
    if (portfolio_id == selected_id_) {
        services::PortfolioService::instance().refresh_summary(portfolio_id);
    }
}

void PortfolioScreen::on_create_requested() {
    CreatePortfolioDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        services::PortfolioService::instance().create_portfolio(dlg.name(), dlg.owner(), dlg.currency());
    }
}

void PortfolioScreen::on_delete_requested(const QString& id) {
    QString name;
    for (const auto& p : portfolios_) {
        if (p.id == id) {
            name = p.name;
            break;
        }
    }

    ConfirmDeleteDialog dlg(name, this);
    if (dlg.exec() == QDialog::Accepted) {
        services::PortfolioService::instance().delete_portfolio(id);
    }
}

void PortfolioScreen::on_detail_view_selected(portfolio::DetailView view) {
    if (active_detail_.has_value() && *active_detail_ == view) {
        // Toggle off — go back to main view
        active_detail_ = std::nullopt;
    } else {
        active_detail_ = view;
        // Show the detail view with current data
        detail_wrapper_->show_view(view, current_summary_, current_summary_.portfolio.currency);
    }
    command_bar_->set_detail_view(active_detail_);
    update_content_state();
}

void PortfolioScreen::on_refresh_interval_changed(int ms) {
    refresh_interval_ms_ = ms;
    refresh_timer_->setInterval(ms);
    SettingsRepository::instance().set("portfolio.refresh_interval_ms", QString::number(ms), "portfolio");
}

void PortfolioScreen::request_refresh() {
    if (!selected_id_.isEmpty()) {
        command_bar_->set_refreshing(true);
        services::PortfolioService::instance().refresh_summary(selected_id_);
    }
}

// ── Phase 3: Main view construction ──────────────────────────────────────────


void PortfolioScreen::on_symbol_selected(const QString& symbol) {
    selected_symbol_ = symbol;
    if (heatmap_)
        heatmap_->set_selected_symbol(symbol);
    if (blotter_)
        blotter_->set_selected_symbol(symbol);
    if (order_panel_)
        order_panel_->set_holding(find_holding(symbol));

    // Publish to the linked group so other panels (Equity Research, Watchlist
    // …) follow the selection. Only when actually linked.
    if (link_group_ != SymbolGroup::None && !symbol.isEmpty()) {
        SymbolContext::instance().set_group_symbol(
            link_group_, SymbolRef::equity(symbol), this);
    }
}

void PortfolioScreen::on_buy_requested() {
    order_panel_visible_ = true;
    order_panel_->set_side("BUY");
    animate_order_panel_in();
}

void PortfolioScreen::on_sell_requested() {
    order_panel_visible_ = true;
    order_panel_->set_side("SELL");
    animate_order_panel_in();
}

void PortfolioScreen::on_order_panel_close() {
    order_panel_visible_ = false;
    order_panel_->setVisible(false);
}

// ── Order panel overlay helpers ──────────────────────────────────────────────

void PortfolioScreen::reposition_order_panel() {
    if (!order_panel_ || !order_panel_visible_)
        return;

    const int panel_w = order_panel_->width();
    // Anchor below command bar + stats ribbon; stop above status bar.
    const int top = command_bar_->height() + (stats_ribbon_->isVisible() ? stats_ribbon_->height() : 0);
    const int bottom = status_bar_->isVisible() ? status_bar_->height() : 0;
    const int h = height() - top - bottom;
    order_panel_->setGeometry(width() - panel_w, top, panel_w, h);
    order_panel_->raise();
}

void PortfolioScreen::animate_order_panel_in() {
    if (!order_panel_)
        return;

    const int panel_w = order_panel_->width();
    const int top = command_bar_->height() + (stats_ribbon_->isVisible() ? stats_ribbon_->height() : 0);
    const int bottom = status_bar_->isVisible() ? status_bar_->height() : 0;
    const int h = height() - top - bottom;

    if (!order_panel_->isVisible()) {
        // Start fully off-screen to the right, then slide into place.
        order_panel_->setGeometry(width(), top, panel_w, h);
        order_panel_->setVisible(true);
    }
    order_panel_->raise();

    order_panel_anim_->stop();
    order_panel_anim_->setStartValue(order_panel_->geometry());
    order_panel_anim_->setEndValue(QRect(width() - panel_w, top, panel_w, h));
    order_panel_anim_->start();
}


void PortfolioScreen::load_demo_portfolio() {
    auto& svc = services::PortfolioService::instance();

    // Connect BEFORE create_portfolio() — create_portfolio() emits portfolio_created
    // synchronously, so the lambda must be connected first or it will never fire.
    QMetaObject::Connection* conn = new QMetaObject::Connection;
    *conn = connect(&svc, &services::PortfolioService::portfolio_created, this, [this, conn](portfolio::Portfolio p) {
        disconnect(*conn);
        delete conn;

        auto& svc = services::PortfolioService::instance();

        // Demo holdings: diversified mix of major stocks
        struct DemoHolding {
            const char* symbol;
            double qty;
            double price;
        };
        static const DemoHolding demo[] = {
            {"AAPL", 15, 178.50}, {"MSFT", 12, 375.20}, {"GOOGL", 8, 141.80}, {"NVDA", 10, 480.00},
            {"AMZN", 6, 178.25},  {"TSLA", 5, 245.00},  {"JPM", 20, 195.50},  {"JNJ", 15, 155.75},
            {"XOM", 25, 105.30},  {"V", 10, 280.00},    {"UNH", 4, 525.60},   {"PG", 12, 158.90},
        };

        for (const auto& h : demo) {
            svc.add_asset(p.id, h.symbol, h.qty, h.price);
        }

        // portfolios_ may not yet contain the new portfolio since load_portfolios()
        // is async. Add it directly so on_portfolio_selected() can find it.
        bool already_present = std::any_of(portfolios_.begin(), portfolios_.end(),
                                           [&p](const portfolio::Portfolio& x) { return x.id == p.id; });
        if (!already_present) {
            portfolios_.append(p);
        }

        on_portfolio_selected(p.id);
    });

    // Create the demo portfolio (emits portfolio_created synchronously)
    svc.create_portfolio("Demo Portfolio", "Fincept User", "USD", "Sample portfolio for demonstration");
}

} // namespace fincept::screens
