// src/screens/portfolio/PortfolioScreen_Layout.cpp
//
// UI construction: build_ui, make_cta_card helper, build_empty_state,
// build_loading_state, update_content_state, build_main_view,
// update_main_view_data.
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

void PortfolioScreen::build_ui() {
    setObjectName("portfolioScreen");
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));

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
            const double qty = [&]() -> double {
                for (const auto& h : current_summary_.holdings)
                    if (h.symbol == dlg.symbol())
                        return h.quantity;
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
    // AnalyticsSectorsView → filter the main blotter by the clicked sector.
    // Mirrors the PortfolioSectorPanel::sector_selected wiring below so both
    // entry points behave identically.
    connect(detail_wrapper_, &PortfolioDetailWrapper::sector_selected, this, [this](const QString& sector) {
        if (!blotter_)
            return;
        if (sector.isEmpty()) {
            blotter_->set_sector_filter({});
            return;
        }
        QStringList matching;
        for (const auto& h : current_summary_.holdings) {
            QString h_sector = h.sector.isEmpty() ? QStringLiteral("Unclassified") : h.sector;
            if (h_sector == sector)
                matching.append(h.symbol);
        }
        blotter_->set_sector_filter(matching);
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

    // ── Insights dock (unified AI + Agent right-hand panel) ─────────────────
    // Sits above all other widgets as a child overlay, positioned in
    // resizeEvent so it tracks window size. A scrim behind it dims the rest
    // of the screen so the user knows focus has moved.
    insights_scrim_ = new QWidget(this);
    insights_scrim_->setObjectName("PortfolioInsightsScrim");
    insights_scrim_->setStyleSheet("#PortfolioInsightsScrim { background:rgba(0,0,0,0.45); }");
    insights_scrim_->hide();

    insights_panel_ = new PortfolioInsightsPanel(this);
    connect(insights_panel_, &PortfolioInsightsPanel::close_requested, this, [this]() {
        insights_scrim_->hide();
    });

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

    // Wire import completion
    connect(&services::PortfolioService::instance(), &services::PortfolioService::import_complete, this,
            [this](portfolio::ImportResult result) {
                if (result.portfolio_id.isEmpty()) {
                    QString detail = result.errors.isEmpty()
                                         ? QString("Import failed with no details.")
                                         : result.errors.join("\n");
                    QMessageBox::warning(this, "Portfolio Import Failed",
                                         "Could not import the portfolio.\n\n" + detail +
                                             "\n\nExpected format:\n"
                                             "{\n"
                                             "  \"portfolio_name\": \"My Portfolio\",\n"
                                             "  \"currency\": \"USD\",\n"
                                             "  \"owner\": \"...\",\n"
                                             "  \"transactions\": [\n"
                                             "    {\"date\": \"YYYY-MM-DD\", \"symbol\": \"AAPL\", \"type\": \"BUY\",\n"
                                             "     \"quantity\": 10, \"price\": 150.0}\n"
                                             "  ]\n"
                                             "}");
                    return;
                }
                on_portfolio_selected(result.portfolio_id);
            });
}

// Build one CTA card used in the empty state. Each card is a clickable,
// equal-weight tile: icon, title, subtitle, accent bar at the bottom.
static QPushButton* make_cta_card(const QString& glyph, const QString& title, const QString& subtitle,
                                  const QString& accent_hex, QWidget* parent) {
    auto* btn = new QPushButton(parent);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedSize(220, 140);
    btn->setObjectName("pfCtaCard");
    btn->setProperty("accent", accent_hex);
    // Square corners — DESIGN_SYSTEM rule 9.1 forbids border-radius.
    btn->setStyleSheet(
        QString("QPushButton#pfCtaCard { background:%1; color:%2; border:1px solid %3;"
                "  text-align:left; padding:16px; }"
                "QPushButton#pfCtaCard:hover { border-color:%4; background:%5; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), accent_hex,
                 ui::colors::BG_HOVER()));

    auto* v = new QVBoxLayout(btn);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(6);
    v->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    auto* icon = new QLabel(glyph);
    icon->setStyleSheet(QString("color:%1; font-size:22px; background:transparent; border:none;").arg(accent_hex));
    v->addWidget(icon);

    v->addSpacing(4);

    auto* h = new QLabel(title);
    h->setStyleSheet(QString("color:%1; font-size:13px; font-weight:700; letter-spacing:0.5px;"
                             " background:transparent; border:none;")
                         .arg(ui::colors::TEXT_PRIMARY()));
    v->addWidget(h);

    auto* p = new QLabel(subtitle);
    p->setWordWrap(true);
    p->setStyleSheet(QString("color:%1; font-size:11px; line-height:1.4; background:transparent; border:none;")
                         .arg(ui::colors::TEXT_TERTIARY()));
    v->addWidget(p);

    v->addStretch(1);

    // Accent bar at bottom
    auto* bar = new QWidget;
    bar->setFixedHeight(2);
    bar->setStyleSheet(QString("background:%1; border:none;").arg(accent_hex));
    v->addWidget(bar);

    return btn;
}

QWidget* PortfolioScreen::build_empty_state() {
    auto* w = new QWidget(this);
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
    accent_dot->setStyleSheet(QString("color:%1; font-size:14px; letter-spacing:4px;").arg(ui::colors::AMBER()));
    layout->addWidget(accent_dot);

    auto* title = new QLabel("PORTFOLIO WORKSPACE");
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(
        QString("color:%1; font-size:18px; font-weight:700; letter-spacing:2px;").arg(ui::colors::TEXT_PRIMARY()));
    layout->addWidget(title);

    auto* sub = new QLabel("Create, import, or explore a sample portfolio to get started.");
    sub->setAlignment(Qt::AlignCenter);
    sub->setStyleSheet(QString("color:%1; font-size:12px; letter-spacing:0.2px;").arg(ui::colors::TEXT_SECONDARY()));
    layout->addWidget(sub);

    layout->addSpacing(24);

    // CTA card row: Create / Import / Demo — equal weight
    auto* card_row = new QHBoxLayout;
    card_row->setAlignment(Qt::AlignCenter);
    card_row->setSpacing(14);

    auto* create_card = make_cta_card("\u25A2", "CREATE NEW",
                                      "Start a fresh portfolio. Name it, pick a currency, "
                                      "and add holdings one at a time.",
                                      ui::colors::AMBER(), content);
    connect(create_card, &QPushButton::clicked, this, &PortfolioScreen::on_create_requested);

    auto* import_card = make_cta_card("\u2913", "IMPORT JSON",
                                      "Load an existing portfolio from an exported JSON file. "
                                      "Merge into an existing portfolio or create a new one.",
                                      ui::colors::CYAN(), content);
    connect(import_card, &QPushButton::clicked, command_bar_, &PortfolioCommandBar::import_requested);

    auto* demo_card = make_cta_card("\u25B6", "LOAD DEMO",
                                    "Preview the workspace with a sample diversified portfolio "
                                    "of 12 major equities.",
                                    ui::colors::POSITIVE(), content);
    connect(demo_card, &QPushButton::clicked, this, [this]() { load_demo_portfolio(); });

    card_row->addWidget(create_card);
    card_row->addWidget(import_card);
    card_row->addWidget(demo_card);
    layout->addLayout(card_row);

    outer->addWidget(content, 0, Qt::AlignCenter);
    return w;
}

QWidget* PortfolioScreen::build_loading_state() {
    auto* w = new QWidget(this);
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
    auto* text = new QLabel("Loading portfolio data…");
    text->setAlignment(Qt::AlignCenter);
    text->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:600; letter-spacing:0.8px;").arg(ui::colors::AMBER()));
    outer->addWidget(text);

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
    // Ping-pong: swap start/end each loop
    connect(anim, &QPropertyAnimation::finished, anim, [anim]() {
        auto s = anim->startValue();
        anim->setStartValue(anim->endValue());
        anim->setEndValue(s);
    });
    anim->start();

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


QWidget* PortfolioScreen::build_main_view() {
    auto* w = new QWidget(this);
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
    auto* top_split = new QSplitter(Qt::Horizontal);
    top_split->setHandleWidth(1);
    top_split->setStyleSheet(QString("QSplitter::handle { background:%1; }").arg(ui::colors::BORDER_DIM()));

    heatmap_ = new PortfolioHeatmap;
    connect(heatmap_, &PortfolioHeatmap::symbol_selected, this, &PortfolioScreen::on_symbol_selected);

    perf_chart_ = new PortfolioPerfChart;
    // Trigger backfill when the user clicks a period that needs more history
    // than we have cached. PortfolioService re-emits history_backfilled when
    // done, which routes back through the chart via load_snapshots.
    connect(perf_chart_, &PortfolioPerfChart::backfill_period_requested, this,
            [this](const QString& period) {
                if (selected_id_.isEmpty())
                    return;
                services::PortfolioService::instance().backfill_history(selected_id_, period);
            });

    sector_panel_ = new PortfolioSectorPanel;
    connect(sector_panel_, &PortfolioSectorPanel::sector_selected, this, [this](const QString& sector) {
        if (sector.isEmpty()) {
            blotter_->set_sector_filter({});
            return;
        }
        QStringList matching;
        for (const auto& h : current_summary_.holdings) {
            QString h_sector = h.sector.isEmpty() ? QStringLiteral("Unclassified") : h.sector;
            if (h_sector == sector)
                matching.append(h.symbol);
        }
        blotter_->set_sector_filter(matching);
    });

    top_split->addWidget(heatmap_);
    top_split->addWidget(perf_chart_);
    top_split->addWidget(sector_panel_);
    top_split->setStretchFactor(0, 0); // heatmap fixed-ish (200-240 via setMin/MaxWidth)
    top_split->setStretchFactor(1, 7); // chart takes the lion's share
    top_split->setStretchFactor(2, 3); // sector ~30% of remaining
    sector_panel_->setMinimumWidth(280);
    // Initial pixel sizes — splitter respects min/max so heatmap clamps to 220
    // even if we ask for 500. Without setSizes the splitter sometimes gives
    // the chart 100% on first paint until a layout tick fires.
    top_split->setSizes({220, 1100, 320});

    root_layout->addWidget(top_split, 40); // 40% of vertical space

    // Separator
    auto* sep = new QWidget(this);
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM()));
    root_layout->addWidget(sep);

    // Positions section: unified ▌POSITIONS panel header. The previous ad-hoc
    // header_row pattern was the original template that drove make_panel_header;
    // now it consumes the helper like every other panel.
    auto* blotter_section = new QWidget(this);
    auto* blotter_layout = new QVBoxLayout(blotter_section);
    blotter_layout->setContentsMargins(0, 0, 0, 0);
    blotter_layout->setSpacing(0);

    auto pos_header = make_panel_header("POSITIONS", this);
    auto* header_hl = pos_header.controls_slot->layout();

    // Count badge — square hairline rectangle.
    positions_count_label_ = new QLabel("0");
    positions_count_label_->setAlignment(Qt::AlignCenter);
    positions_count_label_->setMinimumWidth(22);
    positions_count_label_->setFixedHeight(16);
    positions_count_label_->setStyleSheet(
        QString("color:%1; background:%2; border:1px solid %3;"
                "  font-size:10px; font-weight:700; padding:0 6px;")
            .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    header_hl->addWidget(positions_count_label_);

    // Filter field — right-aligned, framed so it reads as an input. Square (rule 9.1).
    auto* filter_wrap = new QWidget(this);
    filter_wrap->setFixedHeight(22);
    filter_wrap->setMinimumWidth(200);
    filter_wrap->setObjectName("pfFilterWrap");
    filter_wrap->setStyleSheet(
        QString("#pfFilterWrap { background:%1; border:1px solid %2; }"
                "#pfFilterWrap:focus-within { border-color:%3; }")
            .arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM(), ui::colors::AMBER()));
    auto* filter_hl = new QHBoxLayout(filter_wrap);
    filter_hl->setContentsMargins(8, 0, 8, 0);
    filter_hl->setSpacing(6);

    auto* filter_icon = new QLabel("\u2315"); // ⌕ search glyph
    filter_icon->setStyleSheet(
        QString("color:%1; font-size:12px; background:transparent; border:none;").arg(ui::colors::TEXT_TERTIARY()));
    filter_hl->addWidget(filter_icon);

    auto* filter_edit = new QLineEdit;
    filter_edit->setPlaceholderText("Filter positions…");
    filter_edit->setStyleSheet(QString("QLineEdit { background:transparent; color:%1; border:none;"
                                       "  font-size:11px; font-family:%2; }"
                                       "QLineEdit:focus { color:%3; }")
                                   .arg(ui::colors::TEXT_SECONDARY(), ui::fonts::DATA_FAMILY, ui::colors::TEXT_PRIMARY()));
    filter_hl->addWidget(filter_edit, 1);

    header_hl->addWidget(filter_wrap);

    blotter_layout->addWidget(pos_header.header);

    // Bottom: positions blotter
    blotter_ = new PortfolioBlotter;
    connect(blotter_, &PortfolioBlotter::symbol_selected, this, &PortfolioScreen::on_symbol_selected);
    connect(blotter_, &PortfolioBlotter::edit_transaction_requested, this, [this](const QString& symbol) {
        // Load transactions for this symbol, show edit dialog for the most recent one
        services::PortfolioService::instance().load_transactions(selected_id_, 100);
        QPointer<PortfolioScreen> self = this;
        auto conn = std::make_shared<QMetaObject::Connection>();
        *conn = connect(
            &services::PortfolioService::instance(), &services::PortfolioService::transactions_loaded, this,
            [this, self, symbol, conn](QVector<portfolio::Transaction> txns) {
                disconnect(*conn);
                if (!self)
                    return;
                // Find most recent transaction for this symbol
                portfolio::Transaction* match = nullptr;
                for (auto& t : txns) {
                    if (t.symbol == symbol) {
                        match = &t;
                        break;
                    }
                }
                if (!match)
                    return;
                EditTransactionDialog dlg(*match, this);
                if (dlg.exec() == QDialog::Accepted) {
                    services::PortfolioService::instance().update_transaction(match->id, dlg.quantity(), dlg.price(),
                                                                              dlg.date(), dlg.notes());
                }
            },
            Qt::SingleShotConnection);
    });
    connect(blotter_, &PortfolioBlotter::delete_position_requested, this, [this](const QString& symbol) {
        auto* h = find_holding(symbol);
        if (!h)
            return;
        ConfirmDeleteDialog dlg(QString("%1 (%2 shares)").arg(symbol).arg(h->quantity, 0, 'f', 2), this);
        if (dlg.exec() == QDialog::Accepted) {
            services::PortfolioService::instance().sell_asset(selected_id_, symbol, h->quantity, h->current_price);
        }
    });
    connect(filter_edit, &QLineEdit::textChanged, blotter_, &PortfolioBlotter::set_filter);

    blotter_layout->addWidget(blotter_, 1);

    root_layout->addWidget(blotter_section, 60); // 60% of vertical space

    // ── Footer: transaction history (collapsible) ────────────────────────────
    // Sits at the bottom of the main view, full-width. Defaults open at 140px;
    // user can collapse via the chevron in its header.
    txn_panel_ = new PortfolioTxnPanel;
    txn_panel_->setFixedHeight(140);
    root_layout->addWidget(txn_panel_);
    connect(txn_panel_, &PortfolioTxnPanel::collapse_toggled, this, [this](bool collapsed) {
        // Header is 30px, table content is the rest. When collapsed, shrink
        // to header height; when expanded, restore to 140px.
        txn_panel_->setFixedHeight(collapsed ? 30 : 140);
    });

    // Order panel is a floating overlay (parented to PortfolioScreen, not in layout)
    // so BUY/SELL slides in from the right edge without reflowing the main grid.
    order_panel_ = new PortfolioOrderPanel(this);
    order_panel_->setVisible(false);
    order_panel_->raise();
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

    order_panel_anim_ = new QPropertyAnimation(order_panel_, "geometry", this);
    order_panel_anim_->setDuration(180);
    order_panel_anim_->setEasingCurve(QEasingCurve::OutCubic);

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
    if (positions_count_label_)
        positions_count_label_->setText(QString::number(current_summary_.holdings.size()));

    order_panel_->set_currency(currency);

    // Re-select the current symbol
    if (!selected_symbol_.isEmpty()) {
        heatmap_->set_selected_symbol(selected_symbol_);
        blotter_->set_selected_symbol(selected_symbol_);
        order_panel_->set_holding(find_holding(selected_symbol_));
    }
}

} // namespace fincept::screens
