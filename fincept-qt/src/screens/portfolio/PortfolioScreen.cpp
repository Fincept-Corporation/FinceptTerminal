// src/screens/portfolio/PortfolioScreen.cpp
//
// Core lifecycle: ctor, show/hide/resize events, refresh_theme, find_holding,
// save_state/restore_state, current_symbol, on_group_symbol_changed.
// Other concerns:
//   - PortfolioScreen_Layout.cpp   — build_ui + build_* + update_main_view_data
//   - PortfolioScreen_Handlers.cpp — service-event slots + user-action slots
#include "screens/portfolio/PortfolioScreen.h"

#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "core/symbol/SymbolRef.h"
#include "screens/common/CommandRows.h"
#include "screens/portfolio/PortfolioBlotter.h"
#include "screens/portfolio/PortfolioCommandBar.h"
#include "screens/portfolio/PortfolioDetailWrapper.h"
#include "screens/portfolio/PortfolioDialogs.h"
#include "screens/portfolio/PortfolioFFNView.h"
#include "screens/portfolio/PortfolioHeatmap.h"
#include "screens/portfolio/PortfolioInsightsPanel.h"
#include "screens/portfolio/PortfolioOrderPanel.h"
#include "ui/widgets/PanelHeader.h"
#include "screens/portfolio/PortfolioPerfChart.h"
#include "screens/portfolio/PortfolioSectorPanel.h"
#include "screens/portfolio/PortfolioStatsRibbon.h"
#include "screens/portfolio/PortfolioStatusBar.h"
#include "screens/portfolio/PortfolioTxnPanel.h"
#include "services/cloud/CloudSyncEngine.h"
#include "services/file_manager/FileManagerService.h"
#include "services/portfolio/PortfolioService.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/theme/Theme.h"

#include <QCoreApplication>
#include <QEvent>
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
#include <QSplitter>
#include <QVBoxLayout>

#include <memory>

namespace fincept::screens {

PortfolioScreen::PortfolioScreen(QWidget* parent) : QWidget(parent) {
    build_ui();
    refresh_theme(); // Apply theme-aware font sizes and colors on first build

    // Reload the portfolio list from the local cache when a cloud pull lands.
    connect(&fincept::services::cloud::CloudSyncEngine::instance(),
            &fincept::services::cloud::CloudSyncEngine::cloud_data_changed, this, [](const QString& entity) {
                if (entity == QLatin1String("portfolio"))
                    services::PortfolioService::instance().load_portfolios();
            });

    // Connect to PortfolioService signals
    auto& svc = services::PortfolioService::instance();
    connect(&svc, &services::PortfolioService::portfolios_loaded, this, &PortfolioScreen::on_portfolios_loaded);
    connect(&svc, &services::PortfolioService::summary_loaded, this, &PortfolioScreen::on_summary_loaded);
    connect(&svc, &services::PortfolioService::summary_error, this, &PortfolioScreen::on_summary_error);
    connect(&svc, &services::PortfolioService::metrics_computed, this, &PortfolioScreen::on_metrics_computed);
    connect(&svc, &services::PortfolioService::portfolio_created, this, &PortfolioScreen::on_portfolio_created);
    connect(&svc, &services::PortfolioService::portfolio_deleted, this, &PortfolioScreen::on_portfolio_deleted);
    connect(&svc, &services::PortfolioService::asset_added, this, &PortfolioScreen::on_asset_changed);
    connect(&svc, &services::PortfolioService::asset_sold, this, &PortfolioScreen::on_asset_changed);
    connect(&svc, &services::PortfolioService::snapshots_loaded, this, &PortfolioScreen::on_snapshots_loaded);
    // The signal carries a capped page of rows; the panel shows the whole ledger,
    // read locally for the selected portfolio.
    connect(&svc, &services::PortfolioService::transactions_loaded, this,
            [this](QVector<portfolio::Transaction>) { reload_transactions(); });
    // Market info (sector, country, 52-week range, yield) landed for some holdings.
    connect(&svc, &services::PortfolioService::allocation_updated, this, [this](QString portfolio_id) {
        if (portfolio_id == selected_id_)
            refresh_allocation();
    });
    // Benchmark / risk-free / cash / lot settings changed: everything is revalued.
    connect(&svc, &services::PortfolioService::portfolio_settings_changed, this, [this](QString portfolio_id) {
        if (portfolio_id == selected_id_)
            services::PortfolioService::instance().refresh_summary(portfolio_id);
    });
    // One analytics run per load delivers correlation, benchmark history,
    // risk-free rate and ex-ante risk together (was four Python spawns).
    connect(&svc, &services::PortfolioService::market_analytics_loaded, this,
            [this](QString portfolio_id, bool ok, QString error) {
                if (portfolio_id != selected_id_)
                    return; // a late result for a portfolio the user has left
                on_market_analytics(ok, error);
            });
    connect(&svc, &services::PortfolioService::ledger_changed, this, &PortfolioScreen::on_asset_changed);
    // Refused / failed writes (over-sell, DB error) used to be log-only, so the
    // action simply looked ignored.
    // A database failure carries its raw reason in last_failure_details(): shown under
    // "Show Details…", never as the message itself.
    connect(&svc, &services::PortfolioService::operation_failed, this,
            [this](QString portfolio_id, QString message) {
                if (!isVisible() || (!portfolio_id.isEmpty() && portfolio_id != selected_id_))
                    return;
                show_failure(tr("Portfolio"), message, services::PortfolioService::instance().last_failure_details());
            });
    connect(&svc, &services::PortfolioService::portfolios_load_failed, this,
            [this](QString message, QString details) { set_storage_failure(message, details); });
    // After yfinance backfill lands, refresh snapshots and metrics so Beta/MDD
    // populate without requiring a manual refresh.
    connect(&svc, &services::PortfolioService::history_backfilled, this, [this](QString portfolio_id, int point_count) {
        if (point_count <= 0 || !summary_loaded_ || portfolio_id != selected_id_)
            return;
        services::PortfolioService::instance().load_snapshots(portfolio_id, kSnapshotHistoryDays);
        request_metrics();
    });

    // Restore persisted refresh interval (P17)
    {
        auto r = SettingsRepository::instance().get("portfolio.refresh_interval_ms");
        if (r.is_ok() && !r.value().isEmpty()) {
            bool ok = false;
            const int saved = r.value().toInt(&ok);
            if (ok && saved >= 10000) // sanity: minimum 10 s
                refresh_interval_ms_ = saved;
        }
    }

    // Refresh timer (P3: only set interval, don't start)
    refresh_timer_ = new QTimer(this);
    refresh_timer_->setInterval(refresh_interval_ms_);
    connect(refresh_timer_, &QTimer::timeout, this, &PortfolioScreen::request_refresh);
    command_bar_->set_refresh_interval(refresh_interval_ms_);

    // Load portfolios
    svc.load_portfolios();

    // Theme / font change: refresh all child component styles (and font-sized fixed sizes)
    // once the change has settled — never inside theme_changed. Sheets are set only when
    // their text changed.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() { refresh_theme(); });
}

void PortfolioScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    update_loading_anim();
    refresh_timer_->start();
    status_bar_->start_clock();
    if (blotter_ && !current_summary_.portfolio.broker_account_id.isEmpty())
        blotter_->hub_resubscribe_broker_quotes(current_summary_.portfolio.broker_account_id);
    // Rate-gated pull of cloud portfolios on screen entry (no-op when sync is off).
    fincept::services::cloud::CloudSyncEngine::instance().request_pull(QStringLiteral("portfolio"));
}

void PortfolioScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (loading_anim_)
        loading_anim_->stop();
    refresh_timer_->stop();
    status_bar_->stop_clock();
}

// ── Slots ────────────────────────────────────────────────────────────────────

void PortfolioScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void PortfolioScreen::retranslateUi() {
    // PortfolioScreen owns the empty/loading state labels and the POSITIONS
    // header. Child widgets (command bar, stats ribbon, etc.) handle their
    // own retranslate via QEvent::LanguageChange delivered to each top-level
    // widget by Qt.

    if (empty_title_label_)
        empty_title_label_->setText(tr("PORTFOLIO WORKSPACE"));
    if (empty_sub_label_)
        empty_sub_label_->setText(tr("Create, import, or explore a sample portfolio to get started."));

    if (empty_create_card_.title)
        empty_create_card_.title->setText(tr("CREATE NEW"));
    if (empty_create_card_.subtitle)
        empty_create_card_.subtitle->setText(tr("Start a fresh portfolio. Name it, pick a currency, "
                                                "and add holdings one at a time."));
    if (empty_import_card_.title)
        empty_import_card_.title->setText(tr("IMPORT JSON"));
    if (empty_import_card_.subtitle)
        empty_import_card_.subtitle->setText(tr("Load an existing portfolio from an exported JSON file. "
                                                "Merge into an existing portfolio or create a new one."));
    if (empty_demo_card_.title)
        empty_demo_card_.title->setText(tr("LOAD DEMO"));
    if (empty_demo_card_.subtitle)
        empty_demo_card_.subtitle->setText(tr("Preview the workspace with a sample diversified portfolio "
                                              "of 12 major equities."));

    if (loading_label_)
        loading_label_->setText(tr("Loading portfolio data…"));
    if (positions_title_label_)
        positions_title_label_->setText(tr("POSITIONS"));
    if (positions_filter_edit_)
        positions_filter_edit_->setPlaceholderText(tr("Filter positions…"));
    if (sector_filter_btn_)
        sector_filter_btn_->setToolTip(tr("Showing one sector only - click to show all positions"));
    if (!storage_error_message_.isEmpty())
        set_storage_failure(storage_error_message_, storage_error_details_); // keeps its title / card state
}

void PortfolioScreen::refresh_theme() {
    // build_ui() already set this exact stylesheet before the children existed. Applying the
    // same text again (the constructor calls refresh_theme() once the tree is built)
    // re-styled the whole screen; only a changed theme needs it.
    const QString root_css = QString("background:%1;").arg(ui::colors::BG_BASE());
    if (styleSheet() != root_css)
        setStyleSheet(root_css);
    apply_font_sizes();

    if (command_bar_)
        command_bar_->refresh_theme();
    if (stats_ribbon_)
        stats_ribbon_->refresh_theme();
    if (perf_chart_)
        perf_chart_->refresh_theme();
    if (heatmap_)
        heatmap_->refresh_theme();
    if (blotter_)
        blotter_->refresh_theme();
    if (txn_panel_)
        txn_panel_->refresh_theme();
}

void PortfolioScreen::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    reposition_order_panel();

    // Keep the insights dock (and its scrim) glued to the right edge when
    // the window is resized.
    if (insights_panel_ && command_bar_) {
        const int top = command_bar_->height();
        const int bottom_reserve = status_bar_ ? status_bar_->height() : 0;
        const int h = qMax(200, height() - top - bottom_reserve);
        if (insights_scrim_ && insights_scrim_->isVisible())
            insights_scrim_->setGeometry(0, top, width(), h);
        if (insights_panel_->isVisible()) {
            insights_panel_->setFixedHeight(h);
            insights_panel_->move(width() - insights_panel_->width(), top);
        }
    }
}

void PortfolioScreen::show_failure(const QString& title, const QString& message, const QString& details) {
    QMessageBox box(QMessageBox::Warning, title, message, QMessageBox::Ok, this);
    if (!details.trimmed().isEmpty())
        box.setDetailedText(tr("Technical reason (for support):\n%1").arg(details));
    box.exec();
}

void PortfolioScreen::set_storage_failure(const QString& message, const QString& details) {
    storage_error_message_ = message;
    storage_error_details_ = details;
    const bool failed = !message.isEmpty();
    if (storage_error_label_) {
        storage_error_label_->setText(message);
        storage_error_label_->setToolTip(details.isEmpty() ? QString()
                                                           : tr("Technical reason (for support):\n%1").arg(details));
        storage_error_label_->setVisible(failed);
    }
    if (empty_sub_label_)
        empty_sub_label_->setVisible(!failed);
    if (empty_title_label_)
        empty_title_label_->setText(failed ? tr("PORTFOLIO DATA UNAVAILABLE") : tr("PORTFOLIO WORKSPACE"));
    // Every card writes to the tables that cannot be read: they could only fail.
    for (auto* card : findChildren<QPushButton*>(QStringLiteral("pfCtaCard"))) {
        card->setEnabled(!failed);
        card->setToolTip(failed ? tr("Unavailable: this profile's portfolio data cannot be read by this build.")
                                : QString());
    }
    if (failed) {
        portfolios_.clear();
        selected_id_.clear();
        summary_loaded_ = false;
        command_bar_->set_portfolios({});
        command_bar_->set_has_portfolios(false);
        update_content_state();
    }
}

const portfolio::HoldingWithQuote* PortfolioScreen::find_holding(const QString& symbol) const {
    for (const auto& h : current_summary_.holdings) {
        if (h.symbol == symbol)
            return &h;
    }
    return nullptr;
}

QVariantMap PortfolioScreen::save_state() const {
    QVariantMap state{{"portfolio_id", selected_id_}, {"symbol", selected_symbol_}};
    if (positions_filter_edit_)
        state["filter"] = positions_filter_edit_->text();
    return state;
}

void PortfolioScreen::restore_state(const QVariantMap& state) {
    const QString id = state.value("portfolio_id").toString();
    const QString sym = state.value("symbol").toString();
    if (!id.isEmpty())
        on_portfolio_selected(id);
    if (!sym.isEmpty())
        selected_symbol_ = sym;
    if (positions_filter_edit_ && state.contains("filter"))
        positions_filter_edit_->setText(state.value("filter").toString());
}

// ── IGroupLinked ─────────────────────────────────────────────────────────────

SymbolRef PortfolioScreen::current_symbol() const {
    if (selected_symbol_.isEmpty())
        return {};
    return SymbolRef::equity(selected_symbol_);
}

void PortfolioScreen::on_group_symbol_changed(const SymbolRef& ref) {
    if (!ref.is_valid())
        return;
    // Only react if the symbol is actually held — otherwise the group is
    // pointing at a ticker the user can't act on here, and silently
    // selecting a phantom would be misleading.
    if (!find_holding(ref.symbol))
        return;
    if (selected_symbol_ == ref.symbol)
        return;
    selected_symbol_ = ref.symbol;
    if (heatmap_)
        heatmap_->set_selected_symbol(ref.symbol);
    if (blotter_)
        blotter_->set_selected_symbol(ref.symbol);
    if (order_panel_)
        order_panel_->set_holding(find_holding(ref.symbol));
}

// ── ISectionLinked ───────────────────────────────────────────────────────────

namespace {

struct PfSectionView {
    const char* id;
    portfolio::DetailView view;
};

const PfSectionView kPfSectionViews[] = {
    {"sectors", portfolio::DetailView::AnalyticsSectors}, {"perf_risk", portfolio::DetailView::PerfRisk},
    {"optimization", portfolio::DetailView::Optimization}, {"quantstats", portfolio::DetailView::QuantStats},
    {"reports", portfolio::DetailView::ReportsPme},        {"indices", portfolio::DetailView::Indices},
    {"risk", portfolio::DetailView::RiskMgmt},             {"tax_lots", portfolio::DetailView::TaxLots},
    {"income", portfolio::DetailView::Income},             {"planning", portfolio::DetailView::Planning},
    {"economics", portfolio::DetailView::Economics},
};

} // namespace

bool PortfolioScreen::open_section(const QString& section, const QString& focus) {
    const QString key = section.trimmed();
    const QString lower = key.toLower();
    // focus = a held symbol: selected as a click on its row would (exact, then any case).
    auto apply_focus = [this, &focus]() {
        const QString sym = focus.trimmed();
        if (sym.isEmpty() || !summary_loaded_)
            return;
        const portfolio::HoldingWithQuote* h = find_holding(sym);
        for (const auto& candidate : current_summary_.holdings) {
            if (h)
                break;
            if (candidate.symbol.compare(sym, Qt::CaseInsensitive) == 0)
                h = &candidate;
        }
        if (h && h->symbol != selected_symbol_)
            on_symbol_selected(h->symbol);
    };
    if (key.isEmpty()) {
        apply_focus();
        return true;
    }
    for (const PfSectionView& v : kPfSectionViews) {
        if (lower != QLatin1String(v.id))
            continue;
        const bool left_ffn = show_ffn_;
        show_ffn_ = false; // as the FFN view's BACK: the detail view would sit under it
        if (!active_detail_.has_value() || *active_detail_ != v.view)
            on_detail_view_selected(v.view); // the detail tab's click
        else if (left_ffn)
            update_content_state();
        apply_focus();
        return true;
    }
    if (lower == QLatin1String("positions")) {
        const bool left_ffn = show_ffn_;
        show_ffn_ = false;
        if (active_detail_.has_value())
            on_detail_view_selected(*active_detail_); // a click on the open tab closes it
        else if (left_ffn)
            update_content_state();
        apply_focus();
        return true;
    }
    if (lower == QLatin1String("new")) {
        // CREATE NEW (start page card / list button): a modal dialog, so after the link has landed.
        // The start page disables it while the portfolio data cannot be read.
        if (storage_error_message_.isEmpty())
            QMetaObject::invokeMethod(this, [this]() { on_create_requested(); }, Qt::QueuedConnection);
        return true;
    }
    QString rest;
    if (command_rows::split_kind(key, "portfolio", &rest)) {
        if (rest.isEmpty())
            return false;
        QString id;
        for (const Qt::CaseSensitivity cs : {Qt::CaseSensitive, Qt::CaseInsensitive}) {
            for (const auto& p : portfolios_) {
                if (p.id.compare(rest, cs) == 0) {
                    id = p.id;
                    break;
                }
            }
            if (!id.isEmpty())
                break;
        }
        // The list is still loading: select it as restore_state does (checked when the list lands).
        if (id.isEmpty() && portfolios_.isEmpty() && storage_error_message_.isEmpty())
            id = rest;
        if (id.isEmpty())
            return false;
        on_portfolio_selected(id); // the portfolio list's click
        apply_focus();
        return true;
    }
    return false;
}

// ── Search box rows (core/command/CommandRegistry.h; screens/common/CommandRows.h) ──
namespace {

QList<command::CommandItem> pf_command_rows() {
    namespace rows = command_rows;
    const QString where = PortfolioScreen::tr("Portfolio");
    const QString screen = QStringLiteral("portfolio");
    return {
        rows::section(screen, QStringLiteral("positions"), PortfolioScreen::tr("Positions"),
                      {QStringLiteral("holdings"), QStringLiteral("positions"), QStringLiteral("blotter"),
                       QStringLiteral("heatmap"), QStringLiteral("pnl")},
                      where),
        rows::section(screen, QStringLiteral("sectors"), PortfolioScreen::tr("Sector allocation"),
                      {QStringLiteral("sectors"), QStringLiteral("allocation"), QStringLiteral("diversification"),
                       QStringLiteral("correlation")},
                      where),
        rows::section(screen, QStringLiteral("perf_risk"), PortfolioScreen::tr("Performance & risk"),
                      {QStringLiteral("performance"), QStringLiteral("returns"), QStringLiteral("volatility"),
                       QStringLiteral("drawdown"), QStringLiteral("sharpe"), QStringLiteral("benchmark")},
                      where),
        rows::section(screen, QStringLiteral("optimization"), PortfolioScreen::tr("Portfolio optimization"),
                      {QStringLiteral("optimize"), QStringLiteral("optimise"), QStringLiteral("efficient frontier"),
                       QStringLiteral("rebalance"), QStringLiteral("weights")},
                      where),
        rows::section(screen, QStringLiteral("quantstats"), PortfolioScreen::tr("QuantStats"),
                      {QStringLiteral("tearsheet"), QStringLiteral("quant stats"), QStringLiteral("statistics")}, where),
        rows::section(screen, QStringLiteral("reports"), PortfolioScreen::tr("Portfolio reports"),
                      {QStringLiteral("reports"), QStringLiteral("pme"), QStringLiteral("public market equivalent")},
                      where),
        rows::section(screen, QStringLiteral("indices"), PortfolioScreen::tr("Custom indices"),
                      {QStringLiteral("indices"), QStringLiteral("index"), QStringLiteral("benchmark")}, where),
        rows::section(screen, QStringLiteral("risk"), PortfolioScreen::tr("Risk management"),
                      {QStringLiteral("risk"), QStringLiteral("var"), QStringLiteral("value at risk"),
                       QStringLiteral("stress test")},
                      where),
        rows::section(screen, QStringLiteral("tax_lots"), PortfolioScreen::tr("Tax lots"),
                      {QStringLiteral("tax"), QStringLiteral("lots"), QStringLiteral("realised gains"),
                       QStringLiteral("realized gains"), QStringLiteral("capital gains")},
                      where),
        rows::section(screen, QStringLiteral("income"), PortfolioScreen::tr("Dividend income"),
                      {QStringLiteral("income"), QStringLiteral("dividends"), QStringLiteral("interest"),
                       QStringLiteral("ex-dates")},
                      where),
        rows::section(screen, QStringLiteral("planning"), PortfolioScreen::tr("Financial planning"),
                      {QStringLiteral("planning"), QStringLiteral("goals"), QStringLiteral("retirement")}, where),
        rows::section(screen, QStringLiteral("economics"), PortfolioScreen::tr("Portfolio economics"),
                      {QStringLiteral("economics"), QStringLiteral("macro"), QStringLiteral("rates")}, where),
    };
}

void pf_install_commands() {
    command_rows::install(QStringLiteral("portfolio"), &pf_command_rows);
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(pf_install_commands)

} // namespace fincept::screens
