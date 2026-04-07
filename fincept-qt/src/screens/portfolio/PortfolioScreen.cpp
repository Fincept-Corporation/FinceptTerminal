// src/screens/portfolio/PortfolioScreen.cpp
#include "screens/portfolio/PortfolioScreen.h"

#include "screens/portfolio/PortfolioAgentPanel.h"
#include "screens/portfolio/PortfolioAiPanel.h"
#include "screens/portfolio/PortfolioBlotter.h"
#include "screens/portfolio/PortfolioTxnPanel.h"
#include "screens/portfolio/PortfolioCommandBar.h"
#include "screens/portfolio/PortfolioDetailWrapper.h"
#include "screens/portfolio/PortfolioDialogs.h"
#include "screens/portfolio/PortfolioFFNView.h"
#include "screens/portfolio/PortfolioHeatmap.h"
#include "screens/portfolio/PortfolioOrderPanel.h"
#include "screens/portfolio/PortfolioPerfChart.h"
#include "screens/portfolio/PortfolioSectorPanel.h"
#include "screens/portfolio/PortfolioStatsRibbon.h"
#include "screens/portfolio/PortfolioStatusBar.h"
#include "services/file_manager/FileManagerService.h"
#include "services/portfolio/PortfolioService.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/theme/Theme.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

#include <memory>

namespace fincept::screens {

PortfolioScreen::PortfolioScreen(QWidget* parent) : QWidget(parent) {
    build_ui();

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
    connect(&svc, &services::PortfolioService::transactions_loaded, this,
            [this](QVector<portfolio::Transaction> txns) {
                if (txn_panel_)
                    txn_panel_->set_transactions(txns);
            });
    connect(&svc, &services::PortfolioService::correlation_computed, this,
            [this](QHash<QString, double> matrix) {
                if (sector_panel_)
                    sector_panel_->set_correlation(matrix);
            });
    connect(&svc, &services::PortfolioService::spy_history_loaded, this,
            [this](QStringList dates, QVector<double> closes) {
                if (perf_chart_)
                    perf_chart_->set_spy_history(dates, closes);
                // Recompute metrics now that SPY data is available for OLS beta
                if (summary_loaded_)
                    services::PortfolioService::instance().compute_metrics(current_summary_);
            });
    connect(&svc, &services::PortfolioService::risk_free_rate_loaded, this,
            [this](double /*rate*/) {
                // Recompute metrics with updated risk-free rate for Sharpe
                if (summary_loaded_)
                    services::PortfolioService::instance().compute_metrics(current_summary_);
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
}

void PortfolioScreen::build_ui() {
    setObjectName("portfolioScreen");
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Command bar
    command_bar_ = new PortfolioCommandBar(this);
    layout->addWidget(command_bar_);

    connect(command_bar_, &PortfolioCommandBar::portfolio_selected, this, &PortfolioScreen::on_portfolio_selected);
    connect(command_bar_, &PortfolioCommandBar::create_requested, this, &PortfolioScreen::on_create_requested);
    connect(command_bar_, &PortfolioCommandBar::delete_requested, this, &PortfolioScreen::on_delete_requested);
    connect(command_bar_, &PortfolioCommandBar::refresh_requested, this, &PortfolioScreen::request_refresh);
    connect(command_bar_, &PortfolioCommandBar::refresh_interval_changed, this,
            &PortfolioScreen::on_refresh_interval_changed);
    connect(command_bar_, &PortfolioCommandBar::detail_view_selected, this, &PortfolioScreen::on_detail_view_selected);
    connect(command_bar_, &PortfolioCommandBar::buy_requested, this, &PortfolioScreen::on_buy_requested);
    connect(command_bar_, &PortfolioCommandBar::sell_requested, this, &PortfolioScreen::on_sell_requested);
    connect(command_bar_, &PortfolioCommandBar::dividend_requested, this, [this]() {
        if (selected_id_.isEmpty() || current_summary_.holdings.isEmpty())
            return;
        QStringList syms;
        for (const auto& h : current_summary_.holdings)
            syms.append(h.symbol);
        AddDividendDialog dlg(syms, this);
        if (dlg.exec() == QDialog::Accepted) {
            const double qty   = [&]() -> double {
                for (const auto& h : current_summary_.holdings)
                    if (h.symbol == dlg.symbol()) return h.quantity;
                return 0.0;
            }();
            const double total = qty * dlg.amount_per_share();
            services::PortfolioService::instance().record_dividend(
                selected_id_, dlg.symbol(), qty, dlg.amount_per_share(), total, dlg.date(), dlg.notes());
        }
    });
    connect(command_bar_, &PortfolioCommandBar::ffn_toggled, this, [this]() {
        show_ffn_ = !show_ffn_;
        if (show_ffn_) {
            active_detail_ = std::nullopt;
            command_bar_->set_detail_view(std::nullopt);
            ffn_view_->set_data(current_summary_, current_summary_.portfolio.currency);
        }
        update_content_state();
    });

    // Stats ribbon
    stats_ribbon_ = new PortfolioStatsRibbon(this);
    layout->addWidget(stats_ribbon_);

    // Content stack
    content_stack_ = new QStackedWidget(this);
    layout->addWidget(content_stack_, 1);

    empty_state_ = build_empty_state();
    loading_state_ = build_loading_state();

    // Main view: heatmap (left) + center (chart/sector/blotter) + order panel (right)
    main_view_ = build_main_view();

    // Detail view wrapper (lazily constructed views inside)
    detail_wrapper_ = new PortfolioDetailWrapper(this);
    connect(detail_wrapper_, &PortfolioDetailWrapper::back_requested, this, [this]() {
        active_detail_ = std::nullopt;
        command_bar_->set_detail_view(std::nullopt);
        update_content_state();
    });

    // FFN view
    ffn_view_ = new PortfolioFFNView(this);
    connect(ffn_view_, &PortfolioFFNView::back_requested, this, [this]() {
        show_ffn_ = false;
        update_content_state();
    });

    content_stack_->addWidget(empty_state_);    // index 0
    content_stack_->addWidget(loading_state_);  // index 1
    content_stack_->addWidget(main_view_);      // index 2
    content_stack_->addWidget(detail_wrapper_); // index 3
    content_stack_->addWidget(ffn_view_);       // index 4

    content_stack_->setCurrentIndex(0);

    // Status bar
    status_bar_ = new PortfolioStatusBar(this);
    layout->addWidget(status_bar_);

    // ── Floating panels (overlay, not in layout) ─────────────────────────────
    ai_panel_ = new PortfolioAiPanel(this);
    agent_panel_ = new PortfolioAgentPanel(this);

    // Wire export/import/AI/Agent signals from CommandBar
    connect(command_bar_, &PortfolioCommandBar::export_csv_requested, this, [this]() {
        if (selected_id_.isEmpty())
            return;
        QString path = QFileDialog::getSaveFileName(this, "Export CSV", "portfolio.csv", "CSV Files (*.csv)");
        if (!path.isEmpty()) {
            services::PortfolioService::instance().export_csv(selected_id_, path);
            services::FileManagerService::instance().import_file(path, "portfolio");
        }
    });
    connect(command_bar_, &PortfolioCommandBar::export_json_requested, this, [this]() {
        if (selected_id_.isEmpty())
            return;
        QString path = QFileDialog::getSaveFileName(this, "Export JSON", "portfolio.json", "JSON Files (*.json)");
        if (!path.isEmpty()) {
            services::PortfolioService::instance().export_json(selected_id_, path);
            services::FileManagerService::instance().import_file(path, "portfolio");
        }
    });
    connect(command_bar_, &PortfolioCommandBar::import_requested, this, [this]() {
        ImportPortfolioDialog dlg(portfolios_, this);
        if (dlg.exec() == QDialog::Accepted) {
            services::PortfolioService::instance().import_json(dlg.file_path(), dlg.mode(), dlg.merge_target_id());
        }
    });
    connect(command_bar_, &PortfolioCommandBar::ai_analyze_requested, this, [this]() {
        if (!summary_loaded_)
            return;
        ai_panel_->set_summary(current_summary_);
        ai_panel_->move(width() - 492, 42);
        ai_panel_->setFixedHeight(height() - 100);
        ai_panel_->show_panel();
    });
    connect(command_bar_, &PortfolioCommandBar::agent_run_requested, this, [this]() {
        if (!summary_loaded_)
            return;
        agent_panel_->set_summary(current_summary_);
        agent_panel_->move(width() - 984, 42);
        agent_panel_->setFixedHeight(height() - 100);
        agent_panel_->show_panel();
    });

    // Wire import completion
    connect(&services::PortfolioService::instance(), &services::PortfolioService::import_complete, this,
            [this](portfolio::ImportResult result) {
                if (!result.portfolio_id.isEmpty())
                    on_portfolio_selected(result.portfolio_id);
            });
}

QWidget* PortfolioScreen::build_empty_state() {
    auto* w = new QWidget;
    w->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    auto* layout = new QVBoxLayout(w);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(16);

    auto* icon = new QLabel("\U0001F4BC"); // briefcase emoji
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet(QString("font-size:36px; color:%1; opacity:0.2;").arg(ui::colors::TEXT_TERTIARY));
    layout->addWidget(icon);

    auto* title = new QLabel("NO PORTFOLIO SELECTED");
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::TEXT_SECONDARY));
    layout->addWidget(title);

    auto* sub = new QLabel("Select a portfolio or create a new one");
    sub->setAlignment(Qt::AlignCenter);
    sub->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::TEXT_TERTIARY));
    layout->addWidget(sub);

    // Buttons row
    auto* btn_row = new QHBoxLayout;
    btn_row->setAlignment(Qt::AlignCenter);
    btn_row->setSpacing(12);

    auto* create_btn = new QPushButton("CREATE NEW");
    create_btn->setFixedSize(120, 32);
    create_btn->setCursor(Qt::PointingHandCursor);
    create_btn->setStyleSheet(QString("QPushButton { background:%1; color:#000; border:none;"
                                      "  font-size:10px; font-weight:700; letter-spacing:0.5px; }"
                                      "QPushButton:hover { background:%2; }")
                                  .arg(ui::colors::AMBER, ui::colors::WARNING));
    connect(create_btn, &QPushButton::clicked, this, &PortfolioScreen::on_create_requested);
    btn_row->addWidget(create_btn);

    auto* import_btn = new QPushButton("IMPORT JSON");
    import_btn->setFixedSize(120, 32);
    import_btn->setCursor(Qt::PointingHandCursor);
    import_btn->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                "  font-size:10px; font-weight:700; letter-spacing:0.5px; }"
                "QPushButton:hover { background:%3; color:%4; }")
            .arg(ui::colors::TEXT_SECONDARY, ui::colors::BORDER_MED, ui::colors::BG_HOVER, ui::colors::TEXT_PRIMARY));
    connect(import_btn, &QPushButton::clicked, command_bar_, &PortfolioCommandBar::import_requested);
    btn_row->addWidget(import_btn);

    auto* demo_btn = new QPushButton("LOAD DEMO PORTFOLIO");
    demo_btn->setFixedSize(160, 32);
    demo_btn->setCursor(Qt::PointingHandCursor);
    demo_btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %1;"
                                    "  font-size:10px; font-weight:700; letter-spacing:0.5px; }"
                                    "QPushButton:hover { background:%1; color:#000; }")
                                .arg(ui::colors::CYAN));
    connect(demo_btn, &QPushButton::clicked, this, [this]() { load_demo_portfolio(); });
    btn_row->addWidget(demo_btn);

    layout->addLayout(btn_row);
    return w;
}

QWidget* PortfolioScreen::build_loading_state() {
    auto* w = new QWidget;
    w->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    auto* layout = new QVBoxLayout(w);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(12);

    auto* spinner = new QLabel("\u23F3");
    spinner->setAlignment(Qt::AlignCenter);
    spinner->setStyleSheet(QString("font-size:16px; color:%1;").arg(ui::colors::AMBER));
    layout->addWidget(spinner);

    auto* text = new QLabel("Loading portfolio data...");
    text->setAlignment(Qt::AlignCenter);
    text->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:600; letter-spacing:0.5px;").arg(ui::colors::TEXT_SECONDARY));
    layout->addWidget(text);
    return w;
}

void PortfolioScreen::update_content_state() {
    bool has_portfolios = !portfolios_.isEmpty();
    bool has_selection = !selected_id_.isEmpty();

    if (!has_portfolios || !has_selection) {
        // No portfolios or nothing selected — clean empty state
        content_stack_->setCurrentIndex(0);
        stats_ribbon_->setVisible(false);
        status_bar_->setVisible(false);
        command_bar_->setVisible(!has_portfolios ? false : true);
        if (has_portfolios) {
            command_bar_->set_has_selection(false);
        }
    } else if (!summary_loaded_) {
        content_stack_->setCurrentIndex(1); // loading
        stats_ribbon_->setVisible(false);
        status_bar_->setVisible(true);
        command_bar_->setVisible(true);
        command_bar_->set_has_selection(false);
    } else if (show_ffn_) {
        content_stack_->setCurrentIndex(4); // FFN view
        stats_ribbon_->setVisible(false);
        status_bar_->setVisible(true);
        command_bar_->setVisible(true);
        command_bar_->set_has_selection(true);
    } else if (active_detail_.has_value()) {
        content_stack_->setCurrentIndex(3); // detail view
        stats_ribbon_->setVisible(false);
        status_bar_->setVisible(true);
        command_bar_->setVisible(true);
        command_bar_->set_has_selection(true);
    } else {
        content_stack_->setCurrentIndex(2); // main view
        stats_ribbon_->setVisible(true);
        status_bar_->setVisible(true);
        command_bar_->setVisible(true);
        command_bar_->set_has_selection(true);
    }
}

// ── Lifecycle (P3) ───────────────────────────────────────────────────────────

void PortfolioScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    refresh_timer_->start();
    status_bar_->start_clock();
}

void PortfolioScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    refresh_timer_->stop();
    status_bar_->stop_clock();
}

// ── Slots ────────────────────────────────────────────────────────────────────

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

    // Fetch SPY benchmark history for perf chart overlay
    services::PortfolioService::instance().fetch_spy_history("1y");

    // Fetch live risk-free rate (DGS10) for Sharpe computation — cached 24h
    services::PortfolioService::instance().fetch_risk_free_rate();
}

void PortfolioScreen::on_summary_error(QString portfolio_id, QString error) {
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

void PortfolioScreen::on_snapshots_loaded(QString portfolio_id,
                                          QVector<portfolio::PortfolioSnapshot> snapshots) {
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
    SettingsRepository::instance().set("portfolio.refresh_interval_ms",
                                       QString::number(ms), "portfolio");
}

void PortfolioScreen::request_refresh() {
    if (!selected_id_.isEmpty()) {
        command_bar_->set_refreshing(true);
        services::PortfolioService::instance().refresh_summary(selected_id_);
    }
}

// ── Phase 3: Main view construction ──────────────────────────────────────────

QWidget* PortfolioScreen::build_main_view() {
    auto* w = new QWidget;
    w->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));

    auto* h_layout = new QHBoxLayout(w);
    h_layout->setContentsMargins(0, 0, 0, 0);
    h_layout->setSpacing(0);

    // Left: Heatmap (220px)
    heatmap_ = new PortfolioHeatmap;
    connect(heatmap_, &PortfolioHeatmap::symbol_selected, this, &PortfolioScreen::on_symbol_selected);
    h_layout->addWidget(heatmap_);

    // Center: chart + sector (top), blotter (bottom)
    auto* center = new QWidget;
    auto* center_layout = new QVBoxLayout(center);
    center_layout->setContentsMargins(0, 0, 0, 0);
    center_layout->setSpacing(0);

    // Top: perf chart + sector panel side by side
    auto* top_split = new QSplitter(Qt::Horizontal);
    top_split->setHandleWidth(1);
    top_split->setStyleSheet(QString("QSplitter::handle { background:%1; }").arg(ui::colors::BORDER_DIM));

    perf_chart_ = new PortfolioPerfChart;
    sector_panel_ = new PortfolioSectorPanel;
    connect(sector_panel_, &PortfolioSectorPanel::sector_selected, this, [this](const QString& sector) {
        if (sector.isEmpty()) {
            blotter_->set_sector_filter({});
            return;
        }
        QStringList matching;
        for (const auto& h : current_summary_.holdings) {
            if (PortfolioSectorPanel::infer_sector(h.symbol) == sector)
                matching.append(h.symbol);
        }
        blotter_->set_sector_filter(matching);
    });

    top_split->addWidget(perf_chart_);
    top_split->addWidget(sector_panel_);
    top_split->setStretchFactor(0, 3); // 60%
    top_split->setStretchFactor(1, 2); // 40%

    center_layout->addWidget(top_split, 42); // 42% of vertical space

    // Separator
    auto* sep = new QWidget;
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM));
    center_layout->addWidget(sep);

    // Filter bar above blotter
    auto* blotter_section = new QWidget;
    auto* blotter_layout  = new QVBoxLayout(blotter_section);
    blotter_layout->setContentsMargins(0, 0, 0, 0);
    blotter_layout->setSpacing(0);

    auto* filter_row = new QWidget;
    filter_row->setFixedHeight(28);
    filter_row->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;")
                                  .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
    auto* filter_hl = new QHBoxLayout(filter_row);
    filter_hl->setContentsMargins(8, 2, 8, 2);
    filter_hl->setSpacing(6);

    auto* filter_icon = new QLabel("⌕");
    filter_icon->setStyleSheet(QString("color:%1; font-size:13px; background:transparent;")
                                   .arg(ui::colors::TEXT_TERTIARY));
    filter_hl->addWidget(filter_icon);

    auto* filter_edit = new QLineEdit;
    filter_edit->setPlaceholderText("Filter positions…");
    filter_edit->setStyleSheet(
        QString("QLineEdit { background:transparent; color:%1; border:none;"
                "  font-size:11px; font-family:%2; }"
                "QLineEdit:focus { color:%3; }")
            .arg(ui::colors::TEXT_SECONDARY, ui::fonts::DATA_FAMILY, ui::colors::TEXT_PRIMARY));
    filter_hl->addWidget(filter_edit, 1);

    blotter_layout->addWidget(filter_row);

    // Bottom: positions blotter
    blotter_ = new PortfolioBlotter;
    connect(blotter_, &PortfolioBlotter::symbol_selected, this, &PortfolioScreen::on_symbol_selected);
    connect(blotter_, &PortfolioBlotter::edit_transaction_requested, this, [this](const QString& symbol) {
        // Load transactions for this symbol, show edit dialog for the most recent one
        services::PortfolioService::instance().load_transactions(selected_id_, 100);
        QPointer<PortfolioScreen> self = this;
        auto conn = std::make_shared<QMetaObject::Connection>();
        *conn = connect(&services::PortfolioService::instance(),
                        &services::PortfolioService::transactions_loaded,
                        this, [this, self, symbol, conn](QVector<portfolio::Transaction> txns) {
            disconnect(*conn);
            if (!self) return;
            // Find most recent transaction for this symbol
            portfolio::Transaction* match = nullptr;
            for (auto& t : txns) {
                if (t.symbol == symbol) { match = &t; break; }
            }
            if (!match) return;
            EditTransactionDialog dlg(*match, this);
            if (dlg.exec() == QDialog::Accepted) {
                services::PortfolioService::instance().update_transaction(
                    match->id, dlg.quantity(), dlg.price(), dlg.date(), dlg.notes());
            }
        }, Qt::SingleShotConnection);
    });
    connect(blotter_, &PortfolioBlotter::delete_position_requested, this, [this](const QString& symbol) {
        auto* h = find_holding(symbol);
        if (!h) return;
        ConfirmDeleteDialog dlg(QString("%1 (%2 shares)").arg(symbol).arg(h->quantity, 0, 'f', 2), this);
        if (dlg.exec() == QDialog::Accepted) {
            services::PortfolioService::instance().sell_asset(
                selected_id_, symbol, h->quantity, h->current_price);
        }
    });
    connect(filter_edit, &QLineEdit::textChanged, blotter_, &PortfolioBlotter::set_filter);

    blotter_layout->addWidget(blotter_, 1);

    // Transaction history panel below blotter
    txn_panel_ = new PortfolioTxnPanel;
    blotter_layout->addWidget(txn_panel_);
    txn_panel_->setFixedHeight(130);

    center_layout->addWidget(blotter_section, 58); // 58% of vertical space

    h_layout->addWidget(center, 1); // center takes remaining space

    // Right: Order panel (hidden by default)
    order_panel_ = new PortfolioOrderPanel;
    order_panel_->setVisible(false);
    connect(order_panel_, &PortfolioOrderPanel::close_requested, this, &PortfolioScreen::on_order_panel_close);
    connect(order_panel_, &PortfolioOrderPanel::buy_submitted, this, [this]() {
        AddAssetDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            services::PortfolioService::instance().add_asset(selected_id_, dlg.symbol(), dlg.quantity(), dlg.price());
        }
    });
    connect(order_panel_, &PortfolioOrderPanel::sell_submitted, this, [this]() {
        auto* h = find_holding(selected_symbol_);
        if (!h)
            return;
        SellAssetDialog dlg(h->symbol, h->quantity, this);
        if (dlg.exec() == QDialog::Accepted) {
            services::PortfolioService::instance().sell_asset(selected_id_, h->symbol, dlg.quantity(), dlg.price());
        }
    });
    h_layout->addWidget(order_panel_);

    return w;
}

void PortfolioScreen::update_main_view_data() {
    if (!heatmap_)
        return;

    QString currency = current_summary_.portfolio.currency;

    heatmap_->set_holdings(current_summary_.holdings);
    heatmap_->set_currency(currency);

    perf_chart_->set_summary(current_summary_);
    perf_chart_->set_currency(currency);

    sector_panel_->set_holdings(current_summary_.holdings);
    sector_panel_->set_currency(currency);

    blotter_->set_holdings(current_summary_.holdings);

    order_panel_->set_currency(currency);

    // Re-select the current symbol
    if (!selected_symbol_.isEmpty()) {
        heatmap_->set_selected_symbol(selected_symbol_);
        blotter_->set_selected_symbol(selected_symbol_);
        order_panel_->set_holding(find_holding(selected_symbol_));
    }
}

void PortfolioScreen::on_symbol_selected(const QString& symbol) {
    selected_symbol_ = symbol;
    if (heatmap_)
        heatmap_->set_selected_symbol(symbol);
    if (blotter_)
        blotter_->set_selected_symbol(symbol);
    if (order_panel_)
        order_panel_->set_holding(find_holding(symbol));
}

void PortfolioScreen::on_buy_requested() {
    order_panel_visible_ = true;
    order_panel_->setVisible(true);
    order_panel_->set_side("BUY");
}

void PortfolioScreen::on_sell_requested() {
    order_panel_visible_ = true;
    order_panel_->setVisible(true);
    order_panel_->set_side("SELL");
}

void PortfolioScreen::on_order_panel_close() {
    order_panel_visible_ = false;
    order_panel_->setVisible(false);
}

const portfolio::HoldingWithQuote* PortfolioScreen::find_holding(const QString& symbol) const {
    for (const auto& h : current_summary_.holdings) {
        if (h.symbol == symbol)
            return &h;
    }
    return nullptr;
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

QVariantMap PortfolioScreen::save_state() const {
    return {
        {"portfolio_id", selected_id_},
        {"symbol",       selected_symbol_}
    };
}

void PortfolioScreen::restore_state(const QVariantMap& state) {
    const QString id  = state.value("portfolio_id").toString();
    const QString sym = state.value("symbol").toString();
    if (!id.isEmpty())
        on_portfolio_selected(id);
    if (!sym.isEmpty())
        selected_symbol_ = sym;
}

} // namespace fincept::screens
