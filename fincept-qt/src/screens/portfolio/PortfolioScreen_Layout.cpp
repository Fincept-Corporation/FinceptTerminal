// src/screens/portfolio/PortfolioScreen_Layout.cpp
//
// UI construction: build_ui, make_cta_card helper, build_empty_state,
// build_loading_state, update_content_state, build_main_view,
// update_main_view_data.
//
// Part of the partial-class split of PortfolioScreen.cpp.

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "core/market/InstrumentCurrency.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "core/symbol/SymbolRef.h"
#include "screens/portfolio/PortfolioBlotter.h"
#include "screens/portfolio/PortfolioCsv.h"
#include "screens/portfolio/PortfolioCommandBar.h"
#include "screens/portfolio/PortfolioDetailWrapper.h"
#include "screens/portfolio/PortfolioDialogs.h"
#include "screens/portfolio/PortfolioFFNView.h"
#include "screens/portfolio/PortfolioFormat.h"
#include "screens/portfolio/PortfolioHeatmap.h"
#include "screens/portfolio/PortfolioInsightsPanel.h"
#include "screens/portfolio/PortfolioLedgerDialogs.h"
#include "screens/portfolio/PortfolioOrderPanel.h"
#include "ui/widgets/PanelHeader.h"
#include "screens/portfolio/PortfolioPerfChart.h"
#include "screens/portfolio/PortfolioScreen.h"
#include "screens/portfolio/PortfolioSectorPanel.h"
#include "screens/portfolio/PortfolioStatsRibbon.h"
#include "screens/portfolio/PortfolioStatusBar.h"
#include "screens/portfolio/PortfolioTxnPanel.h"
#include "services/backtesting/BacktestingService.h"
#include "services/file_manager/FileManagerService.h"
#include "services/portfolio/PortfolioService.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/ProAnalytics.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QDate>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
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

#include <algorithm>
#include <cmath>
#include <memory>

namespace fincept::screens {

namespace {

// Fixed-size chrome sized for its text: the design size at the default font
// size, growing with the user's font size.
QSize cta_card_size() {
    const double scale = std::max(1.0, ui::fonts::font_px(0) / 14.0);
    return {static_cast<int>(220 * scale), static_cast<int>(140 * scale)};
}
int badge_height() {
    return std::max(16, ui::fonts::font_px(-4) + 6);
}
int filter_height() {
    return std::max(22, ui::fonts::font_px(-3) + 11);
}

} // namespace

void PortfolioScreen::apply_font_sizes() {
    for (auto* card : findChildren<QPushButton*>(QStringLiteral("pfCtaCard")))
        card->setFixedSize(cta_card_size());
    if (positions_count_label_)
        positions_count_label_->setFixedHeight(badge_height());
    if (sector_filter_btn_)
        sector_filter_btn_->setFixedHeight(badge_height());
    if (auto* wrap = findChild<QWidget*>(QStringLiteral("pfFilterWrap")))
        wrap->setFixedHeight(filter_height());
}

void PortfolioScreen::build_ui() {
    // Construction runs on the GUI thread when the tab is first opened: time its parts
    // so a slow one shows up in the log (only reported when the whole build is slow).
    QElapsedTimer build_clock;
    build_clock.start();
    QStringList build_marks;
    qint64 last_mark = 0;
    const auto mark = [&build_clock, &build_marks, &last_mark](const char* what) {
        const qint64 now = build_clock.elapsed();
        build_marks << QStringLiteral("%1 %2 ms").arg(QLatin1String(what)).arg(now - last_mark);
        last_mark = now;
    };

    setObjectName("portfolioScreen");
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Command bar
    command_bar_ = new PortfolioCommandBar(this);
    layout->addWidget(command_bar_);
    // Pro analytics: the server's risk report for the holdings on screen, in its own
    // window (opens on click only; hidden while the server does not offer Pro analytics).
    // Weights are the summary's own base-currency weights; a holding that could not be
    // valued (no price or no FX rate: weight is NaN) is left out, never guessed.
    command_bar_->add_tool(ui::pro::make_button(ui::pro::Kind::PortfolioRisk, [this]() {
        ui::pro::Request r;
        QVector<portfolio::HoldingWithQuote> valued;
        for (const auto& h : current_summary_.holdings)
            if (std::isfinite(h.weight) && h.weight > 0 && !h.symbol.trimmed().isEmpty())
                valued.append(h);
        std::sort(valued.begin(), valued.end(),
                  [](const portfolio::HoldingWithQuote& a, const portfolio::HoldingWithQuote& b) {
                      return a.weight > b.weight;
                  });
        for (const auto& h : valued) {
            if (r.symbols.size() >= 15) // the report covers the 15 largest holdings
                break;
            if (r.symbols.contains(h.symbol.trimmed().toUpper()))
                continue;
            r.symbols << h.symbol.trimmed().toUpper();
            r.weights << h.weight;
        }
        return r;
    }, command_bar_));

    connect(command_bar_, &PortfolioCommandBar::portfolio_selected, this, &PortfolioScreen::on_portfolio_selected);
    connect(command_bar_, &PortfolioCommandBar::create_requested, this, &PortfolioScreen::on_create_requested);
    connect(command_bar_, &PortfolioCommandBar::delete_requested, this, &PortfolioScreen::on_delete_requested);
    connect(command_bar_, &PortfolioCommandBar::refresh_requested, this, &PortfolioScreen::request_refresh);
    connect(command_bar_, &PortfolioCommandBar::refresh_interval_changed, this,
            &PortfolioScreen::on_refresh_interval_changed);
    connect(command_bar_, &PortfolioCommandBar::detail_view_selected, this, &PortfolioScreen::on_detail_view_selected);
    connect(command_bar_, &PortfolioCommandBar::buy_requested, this, &PortfolioScreen::on_buy_requested);
    connect(command_bar_, &PortfolioCommandBar::sell_requested, this, &PortfolioScreen::on_sell_requested);
    // Dividend on the ex-date entitlement (ledger), with withholding tax / currency / FX.
    connect(command_bar_, &PortfolioCommandBar::dividend_requested, this, [this]() { open_dividend_dialog(); });
    connect(command_bar_, &PortfolioCommandBar::cash_requested, this, [this]() { open_cash_dialog(); });
    connect(command_bar_, &PortfolioCommandBar::split_requested, this, [this]() { open_split_dialog(); });
    connect(command_bar_, &PortfolioCommandBar::settings_requested, this, [this]() { open_settings_dialog(); });
    connect(command_bar_, &PortfolioCommandBar::ffn_toggled, this, [this]() {
        show_ffn_ = !show_ffn_;
        if (show_ffn_) {
            active_detail_ = std::nullopt;
            command_bar_->set_detail_view(std::nullopt);
            ensure_ffn_view()->set_data(current_summary_, current_summary_.portfolio.currency);
        }
        update_content_state();
    });
    connect(command_bar_, &PortfolioCommandBar::backtest_requested, this, [this]() {
        if (current_summary_.holdings.isEmpty())
            return;
        QJsonArray symbols, weights;
        for (const auto& h : current_summary_.holdings) {
            symbols.append(h.symbol);
            weights.append(h.weight / 100.0);
        }
        QJsonObject config;
        config["symbols"] = symbols;
        config["weights"] = weights;
        config["initialCapital"] = current_summary_.total_market_value;
        services::backtest::BacktestingService::instance().set_pending_portfolio_config(config);
        fincept::EventBus::instance().publish("nav.switch_screen", {{"screen_id", QString("backtesting")}});
    });

    mark("command bar");
    // Stats ribbon — its period selector asks for that window's metrics.
    stats_ribbon_ = new PortfolioStatsRibbon(this);
    layout->addWidget(stats_ribbon_);
    connect(stats_ribbon_, &PortfolioStatsRibbon::window_changed, this, [this](portfolio::PerfWindow w) {
        if (summary_loaded_)
            services::PortfolioService::instance().compute_metrics(current_summary_, w);
    });

    // Content stack
    content_stack_ = new QStackedWidget(this);
    layout->addWidget(content_stack_, 1);

    mark("ribbon");
    empty_state_ = build_empty_state();
    loading_state_ = build_loading_state();
    mark("empty+loading");

    // Main view: heatmap (left) + center (chart/sector/blotter) + order panel (right)
    main_view_ = build_main_view();
    mark("main view");

    // Detail view wrapper (lazily constructed views inside)
    detail_wrapper_ = new PortfolioDetailWrapper(content_stack_); // built where it stays
    mark("detail wrapper");
    connect(detail_wrapper_, &PortfolioDetailWrapper::back_requested, this, [this]() {
        active_detail_ = std::nullopt;
        command_bar_->set_detail_view(std::nullopt);
        update_content_state();
    });
    // AnalyticsSectorsView → filter the main blotter by the clicked sector.
    // Routed through the main sector panel so its donut highlights the same
    // sector, then the detail view is left so the user lands on the filtered
    // POSITIONS table (it used to filter the hidden blotter silently).
    connect(detail_wrapper_, &PortfolioDetailWrapper::sector_selected, this, [this](const QString& sector) {
        if (sector_panel_)
            sector_panel_->select_sector(sector);
        else
            apply_sector_filter(sector);
        active_detail_ = std::nullopt;
        command_bar_->set_detail_view(std::nullopt);
        update_content_state();
    });

    // PlanningView "Optimize for this return" → jump to the Optimization sub-tab,
    // pre-set to the Target Return method at the return the plan needs. The target
    // used to be dropped here, so the optimizer ran its default method instead.
    connect(detail_wrapper_, &PortfolioDetailWrapper::optimize_requested, this, [this](double target) {
        active_detail_ = portfolio::DetailView::Optimization;
        detail_wrapper_->show_view(portfolio::DetailView::Optimization, current_summary_,
                                   current_summary_.portfolio.currency);
        detail_wrapper_->set_optimization_target(target);
        command_bar_->set_detail_view(active_detail_);
        update_content_state();
    });
    // An empty-book state's BUY (OPTIMIZE, QUANTSTATS, RISK, ECONOMICS) opens Add Asset.
    connect(detail_wrapper_, &PortfolioDetailWrapper::buy_requested, this, &PortfolioScreen::on_buy_requested);
    // ECONOMICS › MACRO SHOCKS "OPEN RISK › STRESS" → the RISK view on its STRESS tab.
    connect(detail_wrapper_, &PortfolioDetailWrapper::risk_stress_requested, this, [this]() {
        active_detail_ = portfolio::DetailView::RiskMgmt;
        detail_wrapper_->show_view(portfolio::DetailView::RiskMgmt, current_summary_,
                                   current_summary_.portfolio.currency);
        detail_wrapper_->show_risk_stress();
        command_bar_->set_detail_view(active_detail_);
        update_content_state();
    });

    // FFN view: built on first use (ensure_ffn_view) — it was a third of the screen's
    // construction time on the GUI thread, for a view most sessions never open.
    mark("wrapper wiring");

    content_stack_->addWidget(empty_state_);    // index 0
    content_stack_->addWidget(loading_state_);  // index 1
    content_stack_->addWidget(main_view_);      // index 2
    content_stack_->addWidget(detail_wrapper_); // index 3
                                                // index 4: FFN view, added by ensure_ffn_view()

    content_stack_->setCurrentIndex(0);
    mark("stack");

    // Status bar
    status_bar_ = new PortfolioStatusBar(this);
    layout->addWidget(status_bar_);

    // ── Insights dock (unified AI + Agent right-hand panel) ─────────────────
    // Sits above all other widgets as a child overlay, positioned in
    // resizeEvent so it tracks window size. A scrim behind it dims the rest
    // of the screen so the user knows focus has moved.
    insights_scrim_ = new QWidget(this);
    insights_scrim_->setObjectName("PortfolioInsightsScrim");
    insights_scrim_->setAttribute(Qt::WA_StyledBackground, true); // a plain QWidget paints no QSS background without it
    insights_scrim_->setStyleSheet(QString("#PortfolioInsightsScrim { background:%1; }").arg(ui::colors::scrim(0.45)));
    insights_scrim_->hide();

    mark("status bar");
    insights_panel_ = new PortfolioInsightsPanel(this);
    mark("insights panel");
    connect(insights_panel_, &PortfolioInsightsPanel::close_requested, this, [this]() { insights_scrim_->hide(); });

    // Wire export/import/AI/Agent signals from CommandBar
    connect(command_bar_, &PortfolioCommandBar::export_csv_requested, this, [this]() {
        if (selected_id_.isEmpty())
            return;
        QString path = QFileDialog::getSaveFileName(this, tr("Export CSV"), "portfolio.csv", tr("CSV Files (*.csv)"));
        if (!path.isEmpty()) {
            services::PortfolioService::instance().export_csv(selected_id_, path);
            if (QFileInfo::exists(path)) // a failed export reports via export_failed
                services::FileManagerService::instance().import_file(path, "portfolio");
        }
    });
    connect(command_bar_, &PortfolioCommandBar::export_json_requested, this, [this]() {
        if (selected_id_.isEmpty())
            return;
        QString path =
            QFileDialog::getSaveFileName(this, tr("Export JSON"), "portfolio.json", tr("JSON Files (*.json)"));
        if (!path.isEmpty()) {
            services::PortfolioService::instance().export_json(selected_id_, path);
            if (QFileInfo::exists(path)) // a failed export reports via export_failed
                services::FileManagerService::instance().import_file(path, "portfolio");
        }
    });
    connect(command_bar_, &PortfolioCommandBar::import_requested, this, [this]() {
        ImportPortfolioDialog dlg(portfolios_, this);
        if (dlg.exec() == QDialog::Accepted) {
            services::PortfolioService::instance().import_json(dlg.file_path(), dlg.mode(), dlg.merge_target_id());
        }
    });
    auto open_insights = [this](PortfolioInsightsPanel::Tab tab) {
        if (!summary_loaded_)
            return;
        insights_panel_->set_summary(current_summary_);
        const int top = command_bar_->height();
        const int bottom_reserve = status_bar_ ? status_bar_->height() : 0;
        const int h = qMax(200, height() - top - bottom_reserve);
        insights_scrim_->setGeometry(0, top, width(), h);
        insights_scrim_->show();
        insights_scrim_->raise();
        insights_panel_->setFixedHeight(h);
        insights_panel_->move(width() - insights_panel_->width(), top);
        insights_panel_->raise();
        insights_panel_->open_tab(tab);
    };
    connect(command_bar_, &PortfolioCommandBar::ai_analyze_requested, this,
            [open_insights]() { open_insights(PortfolioInsightsPanel::Tab::AI); });
    connect(command_bar_, &PortfolioCommandBar::agent_run_requested, this,
            [open_insights]() { open_insights(PortfolioInsightsPanel::Tab::Agent); });

    // Export failures used to be log-only: the save dialog closed and nothing
    // happened. Tell the user (only when this screen is the one on display -
    // the signal is service-wide).
    connect(&services::PortfolioService::instance(), &services::PortfolioService::export_failed, this,
            [this](const QString& file_path, const QString& error) {
                if (!isVisible())
                    return;
                QMessageBox::warning(this, tr("Export Failed"),
                                     tr("Could not export the portfolio to:\n%1\n\n%2").arg(file_path, error));
            });

    // Wire import completion
    connect(&services::PortfolioService::instance(), &services::PortfolioService::import_complete, this,
            [this](portfolio::ImportResult result) {
                if (result.portfolio_id.isEmpty() && !result.details.isEmpty()) {
                    // The database refused the import: the file format is not the problem.
                    show_failure(tr("Portfolio Import Failed"), result.errors.join("\n"), result.details);
                    return;
                }
                if (result.portfolio_id.isEmpty()) {
                    QString detail =
                        result.errors.isEmpty() ? tr("Import failed with no details.") : result.errors.join("\n");
                    QMessageBox::warning(
                        this, tr("Portfolio Import Failed"),
                        tr("Could not import the portfolio.\n\n") + detail +
                            tr("\n\nAccepted formats (either one):\n"
                               "1) Terminal export — a ledger of trades:\n"
                               "{ \"portfolio_name\": \"My Portfolio\", \"currency\": \"USD\",\n"
                               "  \"transactions\": [ {\"date\": \"YYYY-MM-DD\", \"symbol\": \"AAPL\",\n"
                               "     \"type\": \"BUY\" | \"SELL\" | \"DIVIDEND\", \"quantity\": 10, \"price\": 150.0} ] }\n\n"
                               "2) Holdings snapshot — each holding becomes a BUY:\n"
                               "{ \"name\": \"My Portfolio\", \"currency\": \"USD\",\n"
                               "  \"holdings\": [ {\"symbol\": \"AAPL\", \"quantity\": 10, \"avg_cost\": 150.0,\n"
                               "     \"date\": \"YYYY-MM-DD\" (optional), \"sector\": \"Technology\" (optional)} ] }"));
                    return;
                }
                on_portfolio_selected(result.portfolio_id);

                // A partial import (e.g. a SELL with no matching position) used to be
                // dropped on the floor: the new portfolio simply opened with fewer
                // holdings than the file implied and no hint why.
                if (!result.errors.isEmpty()) {
                    QStringList shown = result.errors.mid(0, 10);
                    if (result.errors.size() > 10)
                        shown << tr("… and %1 more").arg(result.errors.size() - 10);
                    show_failure(tr("Import Completed With Warnings"),
                                 tr("Imported %1 transaction(s), but %2 could not be applied:\n\n%3")
                                     .arg(result.transactions_replayed)
                                     .arg(result.errors.size())
                                     .arg(shown.join(QLatin1Char('\n'))),
                                 result.details);
                }
            });
    mark("wiring");
    if (build_clock.elapsed() >= 150)
        LOG_DEBUG("PortfolioScreen", QStringLiteral("built in %1 ms (%2)")
                                        .arg(build_clock.elapsed())
                                        .arg(build_marks.join(QStringLiteral(", "))));
}

PortfolioFFNView* PortfolioScreen::ensure_ffn_view() {
    if (ffn_view_)
        return ffn_view_;
    ffn_view_ = new PortfolioFFNView(content_stack_); // built where it stays (no re-style on addWidget)
    connect(ffn_view_, &PortfolioFFNView::back_requested, this, [this]() {
        show_ffn_ = false;
        update_content_state();
    });
    // Its empty-book state's BUY opens Add Asset.
    connect(ffn_view_, &PortfolioFFNView::add_position_requested, this, &PortfolioScreen::on_buy_requested);
    content_stack_->addWidget(ffn_view_);
    // It missed the analytics runs before it existed: hand it the last benchmark now.
    auto& svc = services::PortfolioService::instance();
    const auto a = svc.last_market_analytics(selected_id_);
    if (!a.portfolio_id.isEmpty()) {
        const QString cut = QDate::currentDate().addYears(-1).toString(Qt::ISODate);
        QVector<double> bench_1y;
        for (qsizetype i = 0; i < a.benchmark_dates.size() && i < a.benchmark_closes.size(); ++i)
            if (a.benchmark_dates.at(i) >= cut)
                bench_1y.append(a.benchmark_closes.at(i));
        ffn_view_->set_benchmark(a.benchmark, bench_1y, svc.risk_free_rate());
    }
    return ffn_view_;
}

// Build one CTA card used in the empty state. Each card is a clickable,
// equal-weight tile: icon, title, subtitle, accent bar at the bottom.
//
// out_title / out_subtitle let the caller keep handles to the inner QLabels so
// they can be retranslated on QEvent::LanguageChange without reconstructing the
// card.
static QPushButton* make_cta_card(const QString& glyph, const QString& title, const QString& subtitle,
                                  const QString& accent_hex, QWidget* parent, QLabel** out_title = nullptr,
                                  QLabel** out_subtitle = nullptr) {
    auto* btn = new QPushButton(parent);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedSize(cta_card_size());
    btn->setObjectName("pfCtaCard");
    btn->setProperty("accent", accent_hex);
    // Square corners — DESIGN_SYSTEM rule 9.1 forbids border-radius.
    btn->setStyleSheet(QString("QPushButton#pfCtaCard { background:%1; color:%2; border:1px solid %3;"
                               "  text-align:left; padding:16px; }"
                               "QPushButton#pfCtaCard:hover { border-color:%4; background:%5; }")
                           .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(),
                                accent_hex, ui::colors::BG_HOVER()));

    // The button's stylesheet padding does not reach a layout's children: the text
    // block gets its own inset, the accent bar stays edge to edge at the bottom.
    auto* outer = new QVBoxLayout(btn);
    outer->setContentsMargins(1, 1, 1, 1); // inside the 1 px border
    outer->setSpacing(0);
    auto* v = new QVBoxLayout;
    v->setContentsMargins(14, 12, 14, 8);
    v->setSpacing(8);
    v->setAlignment(Qt::AlignTop);
    outer->addLayout(v, 1);

    // Icon beside the title: stacked, the wrapped subtitle (4 lines at the
    // default font) no longer fit the fixed card height and was clipped.
    auto* head = new QHBoxLayout;
    head->setSpacing(8);
    auto* icon = new QLabel(glyph);
    icon->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(4) + "px; background:transparent; border:none;").arg(accent_hex));
    head->addWidget(icon);

    auto* h = new QLabel(title);
    h->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-1) + "px; font-weight:700; letter-spacing:0.5px;"
                             " background:transparent; border:none;")
                         .arg(ui::colors::TEXT_PRIMARY()));
    head->addWidget(h, 1);
    v->addLayout(head);
    if (out_title)
        *out_title = h;

    auto* p = new QLabel(subtitle);
    p->setWordWrap(true);
    p->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; line-height:1.4; background:transparent; border:none;")
                         .arg(ui::colors::TEXT_SECONDARY()));
    v->addWidget(p);
    if (out_subtitle)
        *out_subtitle = p;

    v->addStretch(1);

    // Accent bar at bottom
    auto* bar = new QWidget;
    bar->setFixedHeight(2);
    bar->setStyleSheet(QString("background:%1; border:none;").arg(accent_hex));
    outer->addWidget(bar);

    return btn;
}

QWidget* PortfolioScreen::build_empty_state() {
    // Built inside content_stack_ (its final parent): addWidget() then moves nothing.
    auto* w = new QWidget(content_stack_ ? static_cast<QWidget*>(content_stack_) : this);
    w->setObjectName("pfEmptyState");
    w->setStyleSheet(QString("#pfEmptyState { background:%1; }").arg(ui::colors::BG_BASE()));

    auto* outer = new QVBoxLayout(w);
    outer->setAlignment(Qt::AlignCenter);
    outer->setContentsMargins(24, 24, 24, 24);
    outer->setSpacing(0);

    // Inner content block (keeps width capped so it doesn't stretch)
    auto* content = new QWidget(w);
    content->setStyleSheet("background:transparent;");
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->setAlignment(Qt::AlignCenter);

    auto* accent_dot = new QLabel("\u25C6"); // diamond
    accent_dot->setAlignment(Qt::AlignCenter);
    accent_dot->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(0) + "px; letter-spacing:4px;").arg(ui::colors::AMBER()));
    layout->addWidget(accent_dot);

    empty_title_label_ = new QLabel(tr("PORTFOLIO WORKSPACE"));
    empty_title_label_->setAlignment(Qt::AlignCenter);
    empty_title_label_->setStyleSheet(
        QString("color:%1; font-size:" + fincept::ui::fonts::px(4) + "px; font-weight:700; letter-spacing:2px;").arg(ui::colors::TEXT_PRIMARY()));
    layout->addWidget(empty_title_label_);

    empty_sub_label_ = new QLabel(tr("Create, import, or explore a sample portfolio to get started."));
    empty_sub_label_->setAlignment(Qt::AlignCenter);
    empty_sub_label_->setStyleSheet(
        QString("color:%1; font-size:" + fincept::ui::fonts::px(-2) + "px; letter-spacing:0.2px;").arg(ui::colors::TEXT_SECONDARY()));
    layout->addWidget(empty_sub_label_);

    // Shown instead of the subtitle when the portfolio tables cannot be read
    // (set_storage_failure): translated text, the raw reason in the tooltip.
    storage_error_label_ = new QLabel(content);
    storage_error_label_->setAlignment(Qt::AlignCenter);
    storage_error_label_->setWordWrap(true);
    storage_error_label_->setMaximumWidth(700);
    storage_error_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    storage_error_label_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-2) +
                                                "px; background:%2; border:1px solid %3; padding:10px 14px;")
                                            .arg(ui::colors::DANGER(), ui::colors::alpha(ui::colors::DANGER, 0.08),
                                                 ui::colors::alpha(ui::colors::DANGER, 0.4)));
    storage_error_label_->setVisible(false);
    layout->addWidget(storage_error_label_, 0, Qt::AlignHCenter);

    layout->addSpacing(24);

    // CTA card row: Create / Import / Demo — equal weight
    auto* card_row = new QHBoxLayout;
    card_row->setAlignment(Qt::AlignCenter);
    card_row->setSpacing(14);

    auto* create_card =
        make_cta_card("\u25A2", tr("CREATE NEW"),
                      tr("Start a fresh portfolio. Name it, pick a currency, "
                         "and add holdings one at a time."),
                      ui::colors::AMBER(), content, &empty_create_card_.title, &empty_create_card_.subtitle);
    connect(create_card, &QPushButton::clicked, this, &PortfolioScreen::on_create_requested);

    auto* import_card =
        make_cta_card("\u2913", tr("IMPORT JSON"),
                      tr("Load an existing portfolio from an exported JSON file. "
                         "Merge into an existing portfolio or create a new one."),
                      ui::colors::CYAN(), content, &empty_import_card_.title, &empty_import_card_.subtitle);
    connect(import_card, &QPushButton::clicked, command_bar_, &PortfolioCommandBar::import_requested);

    auto* demo_card =
        make_cta_card("\u25B6", tr("LOAD DEMO"),
                      tr("Preview the workspace with a sample diversified portfolio "
                         "of 12 major equities."),
                      ui::colors::POSITIVE(), content, &empty_demo_card_.title, &empty_demo_card_.subtitle);
    connect(demo_card, &QPushButton::clicked, this, [this]() { load_demo_portfolio(); });

    card_row->addWidget(create_card);
    card_row->addWidget(import_card);
    card_row->addWidget(demo_card);
    layout->addLayout(card_row);

    outer->addWidget(content, 0, Qt::AlignCenter);
    return w;
}

QWidget* PortfolioScreen::build_loading_state() {
    auto* w = new QWidget(content_stack_ ? static_cast<QWidget*>(content_stack_) : this); // see build_empty_state
    w->setObjectName("pfLoadingState");
    w->setStyleSheet(QString("#pfLoadingState { background:%1; }").arg(ui::colors::BG_BASE()));

    auto* outer = new QVBoxLayout(w);
    outer->setAlignment(Qt::AlignCenter);
    outer->setContentsMargins(24, 24, 24, 24);
    outer->setSpacing(14);

    // Animated bar skeleton — three stacked pills hinting at the ribbon/chart/table layout.
    auto make_bar = [](int h, int w_px, const QString& color, QWidget* parent) {
        auto* bar = new QFrame(parent);
        bar->setFixedSize(w_px, h);
        // Square — DESIGN_SYSTEM rule 9.1.
        bar->setStyleSheet(QString("background:%1;").arg(color));
        return bar;
    };

    auto* bar1 = make_bar(14, 360, ui::colors::BG_HOVER(), w);
    auto* bar2 = make_bar(86, 360, ui::colors::BG_SURFACE(), w);
    auto* bar3 = make_bar(14, 280, ui::colors::BG_HOVER(), w);
    auto* bar4 = make_bar(14, 220, ui::colors::BG_HOVER(), w);

    auto* row1 = new QHBoxLayout;
    row1->setAlignment(Qt::AlignCenter);
    row1->addWidget(bar1);
    outer->addLayout(row1);

    auto* row2 = new QHBoxLayout;
    row2->setAlignment(Qt::AlignCenter);
    row2->addWidget(bar2);
    outer->addLayout(row2);

    auto* row3 = new QHBoxLayout;
    row3->setAlignment(Qt::AlignCenter);
    row3->addWidget(bar3);
    outer->addLayout(row3);

    auto* row4 = new QHBoxLayout;
    row4->setAlignment(Qt::AlignCenter);
    row4->addWidget(bar4);
    outer->addLayout(row4);

    // Subtle label beneath skeleton
    loading_label_ = new QLabel(tr("Loading portfolio data…"));
    loading_label_->setAlignment(Qt::AlignCenter);
    loading_label_->setStyleSheet(
        QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:600; letter-spacing:0.8px;").arg(ui::colors::AMBER()));
    outer->addWidget(loading_label_);

    // Pulsing opacity animation for the skeleton group (subtle, ≤20 fps per P9)
    auto* effect = new QGraphicsOpacityEffect(w);
    effect->setOpacity(1.0);
    w->setGraphicsEffect(effect);
    auto* anim = new QPropertyAnimation(effect, "opacity", w);
    anim->setDuration(1200);
    anim->setStartValue(0.55);
    anim->setEndValue(1.0);
    anim->setLoopCount(-1);
    anim->setEasingCurve(QEasingCurve::InOutSine);
    // Ping-pong: swap start/end each loop. finished() never fires for an infinite
    // loop (loopCount -1), so this swap was dead code and the pulse restarted
    // abruptly each cycle; currentLoopChanged fires at every loop boundary.
    connect(anim, &QAbstractAnimation::currentLoopChanged, anim, [anim](int) {
        auto s = anim->startValue();
        anim->setStartValue(anim->endValue());
        anim->setEndValue(s);
    });
    // Not started here: an infinite animation ticks ~60x/s for the life of the
    // screen even while hidden. update_loading_anim() runs it only while the
    // skeleton page is actually showing (P3/P9).
    loading_anim_ = anim;

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
        content_stack_->setCurrentWidget(ensure_ffn_view()); // FFN view
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
    // The order panel belongs to the positions view: left open, it floated over the
    // detail views' tiles (and the empty / loading pages).
    if (order_panel_visible_ && content_stack_->currentIndex() != 2)
        on_order_panel_close();
    update_loading_anim();
}

void PortfolioScreen::update_loading_anim() {
    if (!loading_anim_ || !content_stack_)
        return;
    const bool want = isVisible() && content_stack_->currentIndex() == 1;
    if (want && loading_anim_->state() != QAbstractAnimation::Running)
        loading_anim_->start();
    else if (!want && loading_anim_->state() == QAbstractAnimation::Running)
        loading_anim_->stop();
}

// ── Lifecycle (P3) ───────────────────────────────────────────────────────────

QWidget* PortfolioScreen::build_main_view() {
    // Every part of the main view is created under its final parent. They used to be built
    // parentless or under the screen and then moved into place (top splitter, its three
    // panels, the positions section, the blotter, the lower splitter, the ledger panel, and
    // finally the whole view into content_stack_) — every move re-styled the moved subtree.
    auto* w = new QWidget(content_stack_ ? static_cast<QWidget*>(content_stack_) : this);
    w->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));

    // Root: vertical stack. Top row gets 40% (heatmap | chart | sector), the
    // POSITIONS panel takes 60% full-width, and the TXN history sits at the
    // bottom as a collapsible footer. This replaces the previous "heatmap
    // full-height left rail + center column" layout — POSITIONS now spans
    // the full width, which is where the user spends most of their time.
    auto* root_layout = new QVBoxLayout(w);
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(0);

    // ── Top row: heatmap | chart | sector — 40% vertical ────────────────────
    auto* top_split = new QSplitter(Qt::Horizontal, w);
    top_split->setHandleWidth(1);
    top_split->setStyleSheet(QString("QSplitter::handle { background:%1; }").arg(ui::colors::BORDER_DIM()));

    heatmap_ = new PortfolioHeatmap(top_split);
    connect(heatmap_, &PortfolioHeatmap::symbol_selected, this, &PortfolioScreen::on_symbol_selected);

    perf_chart_ = new PortfolioPerfChart(top_split);
    // Trigger backfill when the user clicks a period that needs more history
    // than we have cached. PortfolioService re-emits history_backfilled when
    // done, which routes back through the chart via load_snapshots.
    connect(perf_chart_, &PortfolioPerfChart::backfill_period_requested, this, [this](const QString& period) {
        if (selected_id_.isEmpty())
            return;
        services::PortfolioService::instance().backfill_history(selected_id_, period);
    });

    sector_panel_ = new PortfolioSectorPanel(top_split);
    connect(sector_panel_, &PortfolioSectorPanel::sector_selected, this,
            [this](const QString& sector) { apply_sector_filter(sector); });

    top_split->addWidget(heatmap_);
    top_split->addWidget(perf_chart_);
    top_split->addWidget(sector_panel_);
    top_split->setStretchFactor(0, 2); // holdings treemap
    top_split->setStretchFactor(1, 5); // chart takes the lion's share
    top_split->setStretchFactor(2, 3); // allocation donut + correlation
    sector_panel_->setMinimumWidth(ui::metrics::width(280));
    // Initial pixel sizes — without setSizes the splitter sometimes gives the
    // chart 100% on first paint until a layout tick fires.
    top_split->setSizes({340, 980, 340});

    root_layout->addWidget(top_split, 40); // 40% of vertical space

    // Separator
    auto* sep = new QWidget(w);
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM()));
    root_layout->addWidget(sep);

    // ── Positions | transaction history, side by side and user-resizable ──
    // The positions table is only as wide as its columns, so stacking the ledger
    // under it left the right half of the row empty. Side by side, both tables
    // get the full height and the row is filled.
    // Created before its two panes so they can be built inside it (see above).
    auto* lower_split = new QSplitter(Qt::Horizontal, w);
    lower_split->setObjectName("pfLowerSplit");
    lower_split->setHandleWidth(3);
    lower_split->setChildrenCollapsible(false);
    lower_split->setStyleSheet(QString("QSplitter::handle { background:%1; }").arg(ui::colors::BORDER_DIM()));

    // Positions section: unified ▌POSITIONS panel header. The previous ad-hoc
    // header_row pattern was the original template that drove make_panel_header;
    // now it consumes the helper like every other panel.
    auto* blotter_section = new QWidget(lower_split);
    auto* blotter_layout = new QVBoxLayout(blotter_section);
    blotter_layout->setContentsMargins(0, 0, 0, 0);
    blotter_layout->setSpacing(0);

    auto pos_header = ui::make_panel_header(tr("POSITIONS"), blotter_section);
    positions_title_label_ = pos_header.title_label;
    auto* header_hl = pos_header.controls_slot->layout();

    QWidget* const header_slot = pos_header.controls_slot;

    // Count badge — square hairline rectangle.
    positions_count_label_ = new QLabel("0", header_slot);
    positions_count_label_->setAlignment(Qt::AlignCenter);
    positions_count_label_->setMinimumWidth(22);
    positions_count_label_->setFixedHeight(badge_height());
    positions_count_label_->setStyleSheet(
        QString("color:%1; background:%2; border:1px solid %3;"
                "  font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; padding:0 6px;")
            .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    header_hl->addWidget(positions_count_label_);

    // Active sector filter chip (set from the SECTORS donut or the Analytics &
    // Sectors view). Hidden while no sector filter is active; click clears it.
    sector_filter_btn_ = new QPushButton(header_slot);
    sector_filter_btn_->setCursor(Qt::PointingHandCursor);
    sector_filter_btn_->setFixedHeight(badge_height());
    sector_filter_btn_->setToolTip(tr("Showing one sector only - click to show all positions"));
    sector_filter_btn_->setStyleSheet(QString("QPushButton { color:%1; background:transparent; border:1px solid %1;"
                                              "  font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; padding:0 6px; }"
                                              "QPushButton:hover { background:%1; color:%2; }")
                                          .arg(ui::colors::AMBER(), ui::colors::TEXT_ON_ACCENT()));
    sector_filter_btn_->setVisible(false);
    connect(sector_filter_btn_, &QPushButton::clicked, this, [this]() {
        if (sector_panel_)
            sector_panel_->select_sector({});
        else
            apply_sector_filter({});
    });
    header_hl->addWidget(sector_filter_btn_);

    // Filter field — right-aligned, framed so it reads as an input. Square (rule 9.1).
    auto* filter_wrap = new QWidget(header_slot);
    filter_wrap->setFixedHeight(filter_height());
    filter_wrap->setMinimumWidth(200);
    filter_wrap->setObjectName("pfFilterWrap");
    filter_wrap->setStyleSheet(QString("#pfFilterWrap { background:%1; border:1px solid %2; }"
                                       "#pfFilterWrap:focus-within { border-color:%3; }")
                                   .arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM(), ui::colors::AMBER()));
    auto* filter_hl = new QHBoxLayout(filter_wrap);
    filter_hl->setContentsMargins(8, 0, 8, 0);
    filter_hl->setSpacing(6);

    auto* filter_icon = new QLabel("\u2315"); // ⌕ search glyph
    filter_icon->setStyleSheet(
        QString("color:%1; font-size:" + fincept::ui::fonts::px(-2) + "px; background:transparent; border:none;").arg(ui::colors::TEXT_TERTIARY()));
    filter_hl->addWidget(filter_icon);

    positions_filter_edit_ = new QLineEdit(filter_wrap);
    positions_filter_edit_->setPlaceholderText(tr("Filter positions…"));
    positions_filter_edit_->setStyleSheet(
        QString("QLineEdit { background:transparent; color:%1; border:none;"
                "  font-size:" + fincept::ui::fonts::px(-3) + "px; font-family:%2; }"
                "QLineEdit:focus { color:%3; }")
            .arg(ui::colors::TEXT_SECONDARY(), ui::fonts::DATA_FAMILY, ui::colors::TEXT_PRIMARY()));
    filter_hl->addWidget(positions_filter_edit_, 1);

    header_hl->addWidget(filter_wrap);

    // Column chooser + export of the visible columns.
    auto make_header_btn = [header_slot](const QString& text, const QString& tip) {
        auto* b = new QPushButton(text, header_slot);
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(tip);
        b->setObjectName("pfPosHeaderBtn");
        b->setFixedHeight(filter_height());
        b->setStyleSheet(QString("QPushButton#pfPosHeaderBtn { background:transparent; color:%1; border:1px solid %2;"
                                 "  padding:0 8px; font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; }"
                                 "QPushButton#pfPosHeaderBtn:hover { color:%3; border-color:%3; }")
                             .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(), ui::colors::AMBER()));
        return b;
    };
    auto* columns_btn = make_header_btn(tr("COLUMNS ▾"), tr("Choose the columns of the positions table"));
    auto* export_pos_btn = make_header_btn(tr("EXPORT"), tr("Save the visible columns as CSV"));
    header_hl->addWidget(columns_btn);
    header_hl->addWidget(export_pos_btn);

    blotter_layout->addWidget(pos_header.header);

    // Bottom: positions blotter
    blotter_ = new PortfolioBlotter(blotter_section);
    connect(columns_btn, &QPushButton::clicked, this, [this, columns_btn]() {
        blotter_->show_column_menu(columns_btn->mapToGlobal(QPoint(0, columns_btn->height())));
    });
    connect(export_pos_btn, &QPushButton::clicked, blotter_, &PortfolioBlotter::export_csv);
    connect(blotter_, &PortfolioBlotter::buy_requested, this, [this](const QString& sym) { open_buy_dialog(sym); });
    connect(blotter_, &PortfolioBlotter::sell_requested, this, [this](const QString& sym) { open_sell_dialog(sym); });
    connect(blotter_, &PortfolioBlotter::split_requested, this, [this](const QString& sym) {
        on_symbol_selected(sym);
        open_split_dialog();
    });
    connect(blotter_, &PortfolioBlotter::symbol_selected, this, &PortfolioScreen::on_symbol_selected);
    // Double-click / context menu on a position -> Equity Research / News. The
    // router constructs the target if needed and hands it the symbol.
    connect(blotter_, &PortfolioBlotter::open_symbol_requested, this,
            [](const QString& screen_id, const QString& symbol) {
                if (symbol.isEmpty())
                    return;
                fincept::EventBus::instance().publish("nav.open_symbol",
                                                      {{"screen_id", screen_id}, {"symbol", symbol}});
            });
    connect(blotter_, &PortfolioBlotter::edit_transaction_requested, this, [this](const QString& symbol) {
        // Most recent ledger row for this symbol. The ledger read is local and
        // synchronous; the old version fired an async reload and caught the
        // next transactions_loaded with a one-shot connection.
        const auto txns = services::PortfolioService::instance().ledger(selected_id_);
        for (const auto& t : txns) {
            if (t.symbol == symbol) {
                edit_transaction_by_id(t.id);
                return;
            }
        }
        QMessageBox::information(this, tr("Edit Transaction"), tr("No ledger rows recorded for %1.").arg(symbol));
    });
    connect(blotter_, &PortfolioBlotter::delete_position_requested, this, [this](const QString& symbol) {
        const auto* h = find_holding(symbol);
        if (!h)
            return;
        // `h` points into current_summary_, which the refresh timer replaces while a
        // modal dialog is open - the old code read h->quantity AFTER exec(), i.e. from
        // freed memory whenever a refresh landed in between. Copy what is needed first.
        const double qty = h->quantity;
        const double price = h->current_price;
        const QString qccy = h->price_currency;
        if (!h->priced || !std::isfinite(price) || price <= 0) {
            // No live price: never close at 0 / cost — let the user enter the sale price.
            open_sell_dialog(symbol);
            return;
        }
        // Not ConfirmDeleteDialog: that one is worded for deleting a whole portfolio
        // ("DELETE PORTFOLIO ... remove all holdings and transactions"), which is
        // both wrong and alarming for closing a single position.
        const auto answer = QMessageBox::question(
            this, tr("Close Position"),
            tr("Close the entire %1 position (%2 units) at the last price of %3?\n\n"
               "A SELL transaction is recorded (no fees); the transaction history is kept. Use SELL to enter "
               "a different price, fees or taxes.")
                .arg(symbol, pfmt::qty(qty), pfmt::price(price, qccy, symbol)),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer == QMessageBox::Yes)
            services::PortfolioService::instance().sell_asset(selected_id_, symbol, qty, price);
    });
    connect(positions_filter_edit_, &QLineEdit::textChanged, blotter_, &PortfolioBlotter::set_filter);

    // A few rows always readable; the splitter below shares the rest with the ledger.
    blotter_->setMinimumHeight(ui::metrics::height(160));
    blotter_layout->addWidget(blotter_, 1);

    // Positions | transaction history (lower_split, created above).
    lower_split->addWidget(blotter_section);

    txn_panel_ = new PortfolioTxnPanel(lower_split);
    lower_split->addWidget(txn_panel_);
    lower_split->setStretchFactor(0, 3);
    lower_split->setStretchFactor(1, 2);
    // Initial split (scaled by the stretch factors to the real width): positions
    // get the larger share, the ledger enough for its date/type/symbol/amount columns.
    lower_split->setSizes({960, 640});
    root_layout->addWidget(lower_split, 60); // 60% of vertical space
    connect(txn_panel_, &PortfolioTxnPanel::symbol_activated, this, [](const QString& symbol) {
        fincept::EventBus::instance().publish("nav.open_symbol",
                                              {{"screen_id", QStringLiteral("equity_research")}, {"symbol", symbol}});
    });
    connect(txn_panel_, &PortfolioTxnPanel::open_symbol_requested, this,
            [](const QString& screen_id, const QString& symbol) {
                fincept::EventBus::instance().publish("nav.open_symbol",
                                                      {{"screen_id", screen_id}, {"symbol", symbol}});
            });
    connect(txn_panel_, &PortfolioTxnPanel::edit_transaction_requested, this,
            &PortfolioScreen::edit_transaction_by_id);
    connect(txn_panel_, &PortfolioTxnPanel::delete_transaction_requested, this,
            &PortfolioScreen::delete_transaction_by_id);
    connect(txn_panel_, &PortfolioTxnPanel::export_requested, this, &PortfolioScreen::export_ledger_csv);
    connect(txn_panel_, &PortfolioTxnPanel::collapse_toggled, this, [this, lower_split](bool collapsed) {
        // Collapsed: the panel narrows to its header strip (the table and filters
        // are hidden) and positions take the row. Expanded: back to two fifths of
        // the width; the user can drag the splitter from there.
        if (collapsed) {
            txn_panel_->setMaximumWidth(std::max(ui::metrics::width(150), txn_panel_->sizeHint().width()));
        } else {
            txn_panel_->setMaximumWidth(QWIDGETSIZE_MAX);
            const int total = lower_split->width();
            lower_split->setSizes({total * 3 / 5, total * 2 / 5});
        }
    });

    // Order panel is a floating overlay (parented to PortfolioScreen, not in layout)
    // so BUY/SELL slides in from the right edge without reflowing the main grid.
    order_panel_ = new PortfolioOrderPanel(this);
    order_panel_->setVisible(false);
    order_panel_->raise();
    connect(order_panel_, &PortfolioOrderPanel::close_requested, this, &PortfolioScreen::on_order_panel_close);
    // The order panel always opens against a selected position — carry that ticker
    // (and its live price) into the dialog. Fees, taxes, currency and trade FX are
    // recorded on the ledger row (record_transaction).
    connect(order_panel_, &PortfolioOrderPanel::buy_submitted, this, [this]() { open_buy_dialog(selected_symbol_); });
    connect(order_panel_, &PortfolioOrderPanel::sell_submitted, this,
            [this]() { open_sell_dialog(selected_symbol_); });

    order_panel_anim_ = new QPropertyAnimation(order_panel_, "geometry", this);
    order_panel_anim_->setDuration(180);
    order_panel_anim_->setEasingCurve(QEasingCurve::OutCubic);

    return w;
}

void PortfolioScreen::edit_transaction_by_id(const QString& transaction_id) {
    const auto txns = services::PortfolioService::instance().ledger(selected_id_);
    const auto it = std::find_if(txns.begin(), txns.end(),
                                 [&transaction_id](const auto& t) { return t.id == transaction_id; });
    if (it == txns.end())
        return;
    const portfolio::Transaction txn = *it;
    EditTransactionDialog dlg(txn, this);
    dlg.set_base_currency(pfmt::base_ccy(current_summary_));
    if (dlg.exec() != QDialog::Accepted)
        return;
    auto& svc = services::PortfolioService::instance();
    // Re-derives the position from the ledger and emits ledger_changed → full
    // refresh. A SPLIT keeps price 0 and a cash row price 1 (their fields are hidden).
    const QString type = txn.transaction_type.toUpper();
    const double price = type == portfolio::txn::kSplit ? 0.0
                         : portfolio::txn::is_cash_only(type) ? (txn.price > 0 ? txn.price : 1.0)
                                                              : dlg.price();
    const bool core_changed = std::abs(dlg.quantity() - txn.quantity) > 1e-12 || std::abs(price - txn.price) > 1e-12 ||
                              dlg.date() != txn.transaction_date.left(10) || dlg.notes() != txn.notes;
    bool ok = true;
    if (core_changed)
        ok = svc.update_transaction(selected_id_, txn.id, dlg.quantity(), price, dlg.date(), dlg.notes());
    if (ok && dlg.costs_changed()) {
        auto* c = dlg.costs();
        svc.update_transaction_costs(selected_id_, txn.id, c->fees(), c->taxes(), c->currency(), c->fx_rate(),
                                     txn.lot_id);
    }
}

void PortfolioScreen::delete_transaction_by_id(const QString& transaction_id) {
    const auto txns = services::PortfolioService::instance().ledger(selected_id_);
    const auto it = std::find_if(txns.begin(), txns.end(),
                                 [&transaction_id](const auto& t) { return t.id == transaction_id; });
    if (it == txns.end())
        return;
    const bool cash_only = portfolio::txn::is_cash_only(it->transaction_type);
    const QString what =
        cash_only ? tr("%1 of %2").arg(it->transaction_type, pfmt::money(it->quantity, it->currency.isEmpty()
                                                                                      ? pfmt::base_ccy(current_summary_)
                                                                                      : it->currency))
                  : tr("%1 %2 %3 @ %4").arg(it->transaction_type, pfmt::qty(it->quantity), it->symbol,
                                            pfmt::price_sym(it->price,
                                                            it->currency.isEmpty()
                                                                ? core::market::currency_for_symbol(it->symbol)
                                                                : it->currency,
                                                            it->symbol));
    const auto answer = QMessageBox::question(
        this, tr("Delete Transaction"),
        tr("Delete %1 on %2?\n\nPositions, cash and history are re-derived from the remaining ledger rows. This "
           "cannot be undone.")
            .arg(what, pfmt::date(it->transaction_date)),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer == QMessageBox::Yes)
        services::PortfolioService::instance().delete_transaction(transaction_id, selected_id_);
}

void PortfolioScreen::export_ledger_csv() {
    const auto txns = services::PortfolioService::instance().ledger(selected_id_);
    if (txns.isEmpty()) {
        QMessageBox::information(this, tr("Export Ledger"), tr("No transactions recorded for this portfolio yet."));
        return;
    }
    QVector<QStringList> rows;
    rows.reserve(txns.size());
    const QString base = pfmt::base_ccy(current_summary_);
    for (auto it = txns.crbegin(); it != txns.crend(); ++it) { // oldest first
        // A row without a currency is in the instrument's quoted currency (cash rows: the
        // base): write the code it resolves to, so every amount names its currency.
        QString ccy = it->currency;
        if (ccy.isEmpty())
            ccy = portfolio::txn::is_cash_only(it->transaction_type) || it->symbol.isEmpty()
                      ? base
                      : core::market::currency_for_symbol(it->symbol);
        rows.append({it->transaction_date.left(10), it->symbol, it->transaction_type,
                     portfolio_csv::num(it->quantity, 8), portfolio_csv::num(it->price, 8),
                     portfolio_csv::num(it->total_value, 4), portfolio_csv::num(it->fees, 4),
                     portfolio_csv::num(it->taxes, 4), ccy,
                     it->fx_rate > 0 ? portfolio_csv::num(it->fx_rate, 8) : QString(), it->lot_id, it->notes, it->id});
    }
    const QString path = portfolio_csv::save(
        this, QStringLiteral("transactions_%1.csv").arg(QDate::currentDate().toString(Qt::ISODate)),
        {"date", "symbol", "type", "quantity", "price", "gross", "fees", "taxes", "currency",
         "fx_rate_to_" + pfmt::base_ccy(current_summary_), "lot_id", "notes", "transaction_id"},
        rows);
    if (!path.isEmpty())
        services::FileManagerService::instance().import_file(path, "portfolio");
}

void PortfolioScreen::apply_sector_filter(const QString& sector) {
    sector_filter_ = sector;
    const auto total = current_summary_.holdings.size();
    qsizetype shown = total;
    if (blotter_) {
        if (sector.isEmpty()) {
            blotter_->set_sector_filter({});
        } else {
            QStringList matching;
            for (const auto& h : current_summary_.holdings) {
                const QString h_sector = h.sector.isEmpty() ? tr("Unclassified") : h.sector;
                if (h_sector == sector)
                    matching.append(h.symbol);
            }
            blotter_->set_sector_filter(matching);
            shown = matching.size();
        }
    }
    // The badge used to keep showing the full count while the table showed one sector.
    if (positions_count_label_)
        positions_count_label_->setText(sector.isEmpty() ? QString::number(total)
                                                         : QStringLiteral("%1 / %2").arg(shown).arg(total));
    if (sector_filter_btn_) {
        sector_filter_btn_->setText(sector + QStringLiteral("  ✕"));
        sector_filter_btn_->setVisible(!sector.isEmpty());
    }
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

    blotter_->set_summary(current_summary_);
    refresh_allocation(); // sector / country / 52-week / yield columns (cached market info)
    // After set_holdings so the hub subscriptions already match the symbol set.
    // Previously only showEvent() did this, with whatever summary existed then -
    // nothing on first load, and the next holdings refresh dropped it again.
    blotter_->hub_resubscribe_broker_quotes(current_summary_.portfolio.broker_account_id);
    // Re-derive the sector filter's symbol list from the new holdings (a position bought
    // into the filtered sector shows up) and refresh the count badge ("shown / total").
    apply_sector_filter(sector_filter_);

    order_panel_->set_currency(currency);

    // Re-select the current symbol. find_holding() returns nullptr once the
    // position is gone (sold out, portfolio emptied) — always push the result
    // through so the order panel drops its stale copy instead of keeping a row
    // that no longer exists.
    if (!selected_symbol_.isEmpty()) {
        heatmap_->set_selected_symbol(selected_symbol_);
        blotter_->set_selected_symbol(selected_symbol_);
        order_panel_->set_holding(find_holding(selected_symbol_));
        if (!find_holding(selected_symbol_))
            selected_symbol_.clear();
    } else {
        order_panel_->set_holding(nullptr);
    }
}

} // namespace fincept::screens
