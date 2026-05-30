// src/screens/portfolio/PortfolioCommandBar.cpp
#include "screens/portfolio/PortfolioCommandBar.h"

#include "ui/theme/Theme.h"

#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QMenu>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

namespace fincept::screens {

struct DetailBtnDef {
    const char* label;
    portfolio::DetailView view;
};

// Neutral tab pills — color is reserved for data, not navigation.
// QT_TRANSLATE_NOOP marks the literal for lupdate under the
// PortfolioCommandBar context; runtime tr() in build_detail_tabs() resolves it.
static const DetailBtnDef kDetailButtons[] = {
    {QT_TRANSLATE_NOOP("fincept::screens::PortfolioCommandBar", "SECTORS"), portfolio::DetailView::AnalyticsSectors},
    {QT_TRANSLATE_NOOP("fincept::screens::PortfolioCommandBar", "PERF/RISK"), portfolio::DetailView::PerfRisk},
    {QT_TRANSLATE_NOOP("fincept::screens::PortfolioCommandBar", "OPTIMIZE"), portfolio::DetailView::Optimization},
    {QT_TRANSLATE_NOOP("fincept::screens::PortfolioCommandBar", "QUANTSTATS"), portfolio::DetailView::QuantStats},
    {QT_TRANSLATE_NOOP("fincept::screens::PortfolioCommandBar", "REPORTS"), portfolio::DetailView::ReportsPme},
    {QT_TRANSLATE_NOOP("fincept::screens::PortfolioCommandBar", "INDICES"), portfolio::DetailView::Indices},
    {QT_TRANSLATE_NOOP("fincept::screens::PortfolioCommandBar", "RISK"), portfolio::DetailView::RiskMgmt},
    {QT_TRANSLATE_NOOP("fincept::screens::PortfolioCommandBar", "PLANNING"), portfolio::DetailView::Planning},
    {QT_TRANSLATE_NOOP("fincept::screens::PortfolioCommandBar", "ECONOMICS"), portfolio::DetailView::Economics},
};

PortfolioCommandBar::PortfolioCommandBar(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void PortfolioCommandBar::build_ui() {
    setObjectName("portfolioCommandBar");
    setStyleSheet(QString("#portfolioCommandBar { background:%1; border-bottom:2px solid %2; }")
                      .arg(ui::colors::BG_SURFACE(), ui::colors::AMBER()));

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // ── Row 1: identity + stats + refresh cluster ────────────────────────────
    row1_ = new QWidget(this);
    row1_->setFixedHeight(32);
    row1_->setObjectName("portfolioCmdRow1");
    row1_->setStyleSheet("#portfolioCmdRow1 { background:transparent; }");
    auto* row1_layout = new QHBoxLayout(row1_);
    row1_layout->setContentsMargins(8, 0, 8, 0);
    row1_layout->setSpacing(10);
    build_row1(row1_layout);
    outer->addWidget(row1_);

    // Thin divider between rows
    auto* divider = new QWidget(this);
    divider->setFixedHeight(1);
    divider->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM()));
    outer->addWidget(divider);

    // ── Row 2: trade actions + detail tabs + AI/Agent ────────────────────────
    row2_ = new QWidget(this);
    row2_->setFixedHeight(32);
    row2_->setObjectName("portfolioCmdRow2");
    row2_->setStyleSheet("#portfolioCmdRow2 { background:transparent; }");
    auto* row2_layout = new QHBoxLayout(row2_);
    row2_layout->setContentsMargins(8, 0, 8, 0);
    row2_layout->setSpacing(8);
    build_row2(row2_layout);
    outer->addWidget(row2_);

    // Initially hide secondary clusters until portfolios load
    trade_cluster_->hide();
    tabs_container_->hide();
    tools_cluster_->hide();
}

// ── Row 1 ────────────────────────────────────────────────────────────────────

void PortfolioCommandBar::build_row1(QHBoxLayout* layout) {
    // Portfolio selector (left-aligned). Inline NAV/P&L/DAY/POS labels were
    // dropped — they duplicated the StatsRibbon below this bar. The bar is now
    // strictly identity + actions, never KPI display.
    build_portfolio_selector();
    layout->addWidget(selector_btn_);

    layout->addStretch(1);

    // Refresh + interval + overflow (right-aligned)
    refresh_btn_ = new QPushButton("\u21BB");
    refresh_btn_->setFixedSize(24, 22);
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    refresh_btn_->setToolTip(tr("Refresh portfolio data"));
    refresh_btn_->setObjectName("pfIconBtn");
    connect(refresh_btn_, &QPushButton::clicked, this, &PortfolioCommandBar::refresh_requested);
    layout->addWidget(refresh_btn_);

    interval_cb_ = new QComboBox;
    interval_cb_->setFixedHeight(22);
    interval_cb_->setToolTip(tr("Auto-refresh interval"));
    interval_cb_->addItem("1m", 60000);
    interval_cb_->addItem("5m", 300000);
    interval_cb_->addItem("10m", 600000);
    interval_cb_->addItem("30m", 1800000);
    interval_cb_->addItem("1h", 3600000);
    interval_cb_->addItem("3h", 10800000);
    interval_cb_->addItem("1d", 86400000);
    connect(interval_cb_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) { emit refresh_interval_changed(interval_cb_->itemData(idx).toInt()); });
    layout->addWidget(interval_cb_);

    // Overflow "⋯" menu: CSV / JSON / Import / FFN
    build_overflow_menu();
    layout->addWidget(overflow_btn_);

    apply_row1_styles();
}

void PortfolioCommandBar::build_portfolio_selector() {
    selector_btn_ = new QPushButton(tr("SELECT PORTFOLIO \u25BE"));
    selector_btn_->setFixedHeight(24);
    selector_btn_->setMinimumWidth(180);
    selector_btn_->setMaximumWidth(260);
    selector_btn_->setCursor(Qt::PointingHandCursor);
    connect(selector_btn_, &QPushButton::clicked, this, &PortfolioCommandBar::toggle_dropdown);

    // Dropdown panel
    dropdown_ = new QWidget(this);
    dropdown_->setWindowFlags(Qt::Popup);
    dropdown_->setFixedWidth(280);
    dropdown_->setStyleSheet(
        QString("background:%1; border:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_MED()));

    auto* dd_layout = new QVBoxLayout(dropdown_);
    dd_layout->setContentsMargins(4, 4, 4, 4);
    dd_layout->setSpacing(2);

    search_edit_ = new QLineEdit;
    search_edit_->setPlaceholderText(tr("Search portfolios..."));
    search_edit_->setFixedHeight(26);
    search_edit_->setStyleSheet(QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                                        "  padding:0 8px; font-size:11px; }"
                                        "QLineEdit:focus { border-color:%4; }")
                                    .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(),
                                         ui::colors::AMBER()));
    dd_layout->addWidget(search_edit_);

    portfolio_list_ = new QListWidget;
    portfolio_list_->setFixedHeight(180);
    portfolio_list_->setStyleSheet(QString("QListWidget { background:%1; color:%2; border:none; font-size:11px; }"
                                           "QListWidget::item { padding:5px 8px; }"
                                           "QListWidget::item:selected { background:%3; color:%4; }"
                                           "QListWidget::item:hover { background:%5; }")
                                       .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::AMBER_DIM(),
                                            ui::colors::AMBER(), ui::colors::BG_HOVER()));
    dd_layout->addWidget(portfolio_list_);

    connect(portfolio_list_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        QString id = item->data(Qt::UserRole).toString();
        if (!id.isEmpty()) {
            selected_id_ = id;
            update_selector_label();
            dropdown_->hide();
            dropdown_visible_ = false;
            emit portfolio_selected(id);
        }
    });

    connect(search_edit_, &QLineEdit::textChanged, this, [this](const QString& text) {
        for (int i = 0; i < portfolio_list_->count(); ++i) {
            auto* item = portfolio_list_->item(i);
            bool match = text.isEmpty() || item->text().contains(text, Qt::CaseInsensitive);
            item->setHidden(!match);
        }
    });

    auto* btn_row = new QHBoxLayout;
    btn_row->setSpacing(4);

    create_btn_ = new QPushButton(tr("+ CREATE NEW"));
    create_btn_->setFixedHeight(24);
    create_btn_->setCursor(Qt::PointingHandCursor);
    create_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%3; border:none;"
                                       "  font-size:10px; font-weight:700; letter-spacing:0.5px; }"
                                       "QPushButton:hover { background:%2; }")
                                   .arg(ui::colors::AMBER(), ui::colors::WARNING(), ui::colors::BG_BASE()));
    connect(create_btn_, &QPushButton::clicked, this, [this]() {
        dropdown_->hide();
        dropdown_visible_ = false;
        emit create_requested();
    });
    btn_row->addWidget(create_btn_);

    delete_btn_ = new QPushButton(tr("DELETE"));
    delete_btn_->setFixedHeight(24);
    delete_btn_->setCursor(Qt::PointingHandCursor);
    delete_btn_->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %1;"
                                       "  font-size:10px; font-weight:700; letter-spacing:0.5px; }"
                                       "QPushButton:hover { background:%1; color:%2; }")
                                   .arg(ui::colors::NEGATIVE(), ui::colors::BG_BASE()));
    connect(delete_btn_, &QPushButton::clicked, this, [this]() {
        if (!selected_id_.isEmpty()) {
            dropdown_->hide();
            dropdown_visible_ = false;
            emit delete_requested(selected_id_);
        }
    });
    btn_row->addWidget(delete_btn_);

    dd_layout->addLayout(btn_row);
    dropdown_->hide();
}

void PortfolioCommandBar::build_overflow_menu() {
    overflow_btn_ = new QToolButton(this);
    overflow_btn_->setText("\u22EF"); // horizontal ellipsis
    overflow_btn_->setFixedSize(24, 22);
    overflow_btn_->setCursor(Qt::PointingHandCursor);
    overflow_btn_->setToolTip(tr("More actions"));
    overflow_btn_->setPopupMode(QToolButton::InstantPopup);
    overflow_btn_->setObjectName("pfOverflowBtn");

    overflow_menu_ = new QMenu(this);
    overflow_menu_->setStyleSheet(QString("QMenu { background:%1; color:%2; border:1px solid %3;"
                                          "  padding:4px; font-size:11px; }"
                                          "QMenu::item { padding:6px 16px; }"
                                          "QMenu::item:selected { background:%4; color:%5; }"
                                          "QMenu::separator { height:1px; background:%3; margin:4px 8px; }")
                                      .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(),
                                           ui::colors::AMBER_DIM(), ui::colors::AMBER()));

    export_csv_action_  = overflow_menu_->addAction(tr("Export CSV"));
    export_json_action_ = overflow_menu_->addAction(tr("Export JSON"));
    import_action_      = overflow_menu_->addAction(tr("Import JSON…"));
    overflow_menu_->addSeparator();
    ffn_action_ = overflow_menu_->addAction(tr("FFN Analysis"));
    ffn_action_->setCheckable(true);

    connect(export_csv_action_,  &QAction::triggered, this, &PortfolioCommandBar::export_csv_requested);
    connect(export_json_action_, &QAction::triggered, this, &PortfolioCommandBar::export_json_requested);
    connect(import_action_,      &QAction::triggered, this, &PortfolioCommandBar::import_requested);
    connect(ffn_action_,         &QAction::triggered, this, &PortfolioCommandBar::ffn_toggled);

    overflow_menu_->addSeparator();
    backtest_action_ = overflow_menu_->addAction(tr("Backtest Portfolio"));
    connect(backtest_action_, &QAction::triggered, this, &PortfolioCommandBar::backtest_requested);

    overflow_btn_->setMenu(overflow_menu_);
}

// ── Row 2 ────────────────────────────────────────────────────────────────────

void PortfolioCommandBar::build_row2(QHBoxLayout* layout) {
    // Left: trade cluster
    trade_cluster_ = new QWidget(this);
    trade_cluster_->setStyleSheet("background:transparent;");
    auto* trade_layout = new QHBoxLayout(trade_cluster_);
    trade_layout->setContentsMargins(0, 0, 0, 0);
    trade_layout->setSpacing(4);
    build_trade_cluster(trade_layout);
    layout->addWidget(trade_cluster_);

    // Vertical separator
    auto* sep = new QWidget(this);
    sep->setFixedSize(1, 18);
    sep->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_MED()));
    layout->addWidget(sep);

    // Middle: detail tabs (pill-style)
    tabs_container_ = new QWidget(this);
    tabs_container_->setStyleSheet("background:transparent;");
    auto* tabs_layout = new QHBoxLayout(tabs_container_);
    tabs_layout->setContentsMargins(0, 0, 0, 0);
    tabs_layout->setSpacing(2);
    build_detail_tabs(tabs_layout);
    layout->addWidget(tabs_container_);

    layout->addStretch(1);

    // Right: AI / Agent
    tools_cluster_ = new QWidget(this);
    tools_cluster_->setStyleSheet("background:transparent;");
    auto* tools_layout = new QHBoxLayout(tools_cluster_);
    tools_layout->setContentsMargins(0, 0, 0, 0);
    tools_layout->setSpacing(4);
    build_tools_cluster(tools_layout);
    layout->addWidget(tools_cluster_);

    apply_row2_styles();
}

void PortfolioCommandBar::build_trade_cluster(QHBoxLayout* layout) {
    auto make_trade_btn = [&](QPushButton*& out, const QString& text, const char* accent, auto signal) {
        out = new QPushButton(text);
        out->setFixedHeight(22);
        out->setMinimumWidth(48);
        out->setCursor(Qt::PointingHandCursor);
        out->setProperty("accent", QString(accent));
        connect(out, &QPushButton::clicked, this, signal);
        layout->addWidget(out);
    };

    make_trade_btn(buy_btn_, tr("BUY"), ui::colors::POSITIVE(), &PortfolioCommandBar::buy_requested);
    make_trade_btn(sell_btn_, tr("SELL"), ui::colors::NEGATIVE(), &PortfolioCommandBar::sell_requested);
    make_trade_btn(div_btn_, tr("DIV"), ui::colors::CYAN(), &PortfolioCommandBar::dividend_requested);
}

void PortfolioCommandBar::build_detail_tabs(QHBoxLayout* layout) {
    for (const auto& def : kDetailButtons) {
        // Translate the English source label at the call site so lupdate sees
        // all 9 literals; the static array stores the source key for stable
        // wiring (property("detailView") + signal).
        const QString label = tr(def.label);
        auto* btn = new QPushButton(label);
        btn->setFixedHeight(22);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("detailView", static_cast<int>(def.view));
        btn->setObjectName("pfTab");
        connect(btn, &QPushButton::clicked, this, [this, view = def.view]() { emit detail_view_selected(view); });
        layout->addWidget(btn);
        detail_btns_.append(btn);
    }
}

void PortfolioCommandBar::build_tools_cluster(QHBoxLayout* layout) {
    auto make_tool_btn = [&](QPushButton*& out, const QString& text, const char* accent, auto signal) {
        out = new QPushButton(text);
        out->setFixedHeight(22);
        out->setMinimumWidth(52);
        out->setCursor(Qt::PointingHandCursor);
        out->setProperty("accent", QString(accent));
        out->setObjectName("pfToolBtn");
        connect(out, &QPushButton::clicked, this, signal);
        layout->addWidget(out);
    };

    // AI = AMBER (brand action). AGENT = CYAN (info-tier).
    // Custom hex colors (#9D4EDD purple, #00D4AA teal) violated DESIGN_SYSTEM
    // colour discipline — only AMBER/CYAN/POSITIVE/NEGATIVE/WARNING are allowed.
    make_tool_btn(ai_btn_, tr("AI"), ui::colors::AMBER(), &PortfolioCommandBar::ai_analyze_requested);
    make_tool_btn(agent_btn_, tr("AGENT"), ui::colors::CYAN(), &PortfolioCommandBar::agent_run_requested);
}

// ── Styling ──────────────────────────────────────────────────────────────────

void PortfolioCommandBar::apply_row1_styles() {
    selector_btn_->setStyleSheet(
        QString("QPushButton { background:%1; color:%2; border:1px solid %3;"
                "  padding:0 12px; font-size:11px; font-weight:700; letter-spacing:0.5px; text-align:left; }"
                "QPushButton:hover { border-color:%4; color:%4; }")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED(), ui::colors::AMBER()));

    refresh_btn_->setStyleSheet(QString("QPushButton#pfIconBtn { background:transparent; color:%1;"
                                        "  border:1px solid %2; font-size:13px; }"
                                        "QPushButton#pfIconBtn:hover { border-color:%1; color:%1; }"
                                        "QPushButton#pfIconBtn:disabled { color:%3; }")
                                    .arg(ui::colors::AMBER(), ui::colors::BORDER_MED(), ui::colors::TEXT_TERTIARY()));

    interval_cb_->setStyleSheet(
        QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                "  padding:0 6px; font-size:11px; font-weight:600; min-width:42px; }"
                "QComboBox::drop-down { border:none; width:14px; }"
                "QComboBox QAbstractItemView { background:%1; color:%2; selection-background-color:%4; }")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(), ui::colors::AMBER_DIM()));

    overflow_btn_->setStyleSheet(
        QString("QToolButton#pfOverflowBtn { background:transparent; color:%1; border:1px solid %2;"
                "  font-size:14px; font-weight:700; padding-bottom:4px; }"
                "QToolButton#pfOverflowBtn:hover { border-color:%3; color:%3; }"
                "QToolButton#pfOverflowBtn::menu-indicator { image:none; width:0; }")
            .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(), ui::colors::AMBER()));
}

void PortfolioCommandBar::apply_row2_styles() {
    auto trade_style = [](QPushButton* btn, const char* accent, const char* fg) {
        btn->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none;"
                                   "  padding:0 10px; font-size:11px; font-weight:700; letter-spacing:0.5px; }"
                                   "QPushButton:hover { background:%1; opacity:0.85; }")
                               .arg(accent, fg));
    };
    trade_style(buy_btn_, ui::colors::POSITIVE(), ui::colors::BG_BASE());
    trade_style(sell_btn_, ui::colors::NEGATIVE(), ui::colors::TEXT_PRIMARY());
    trade_style(div_btn_, ui::colors::CYAN(), ui::colors::BG_BASE());

    // Neutral square tabs — color reserved for data, not navigation.
    // DESIGN_SYSTEM rule: no rounded corners on any element.
    const QString tab_qss =
        QString("QPushButton#pfTab { background:transparent; color:%1; border:1px solid transparent;"
                "  padding:0 10px; font-size:11px; font-weight:600; letter-spacing:0.4px; }"
                "QPushButton#pfTab:hover { color:%2; background:%3; }"
                "QPushButton#pfTab[active=\"true\"] { color:%4; background:%5; border-color:%4; }")
            .arg(ui::colors::TEXT_SECONDARY(), ui::colors::TEXT_PRIMARY(), ui::colors::BG_HOVER(), ui::colors::AMBER(),
                 ui::colors::AMBER_DIM());
    for (auto* btn : detail_btns_) {
        btn->setStyleSheet(tab_qss);
    }

    auto tool_style = [](QPushButton* btn, const char* accent) {
        btn->setStyleSheet(QString("QPushButton#pfToolBtn { background:transparent; color:%1; border:1px solid %1;"
                                   "  padding:0 10px; font-size:11px; font-weight:700;"
                                   "  letter-spacing:0.5px; }"
                                   "QPushButton#pfToolBtn:hover { background:%1; color:#000; }")
                               .arg(accent));
    };
    tool_style(ai_btn_, ui::colors::AMBER());
    tool_style(agent_btn_, ui::colors::CYAN());
}

// ── Dropdown ─────────────────────────────────────────────────────────────────

void PortfolioCommandBar::toggle_dropdown() {
    dropdown_visible_ = !dropdown_visible_;
    if (dropdown_visible_) {
        QPoint pos = selector_btn_->mapToGlobal(QPoint(0, selector_btn_->height()));
        dropdown_->move(pos);
        dropdown_->show();
        search_edit_->setFocus();
        search_edit_->clear();
    } else {
        dropdown_->hide();
    }
}

void PortfolioCommandBar::update_selector_label() {
    for (const auto& p : portfolios_) {
        if (p.id == selected_id_) {
            selector_btn_->setText(QString("%1 (%2)  \u25BE").arg(p.name.toUpper(), p.currency));
            return;
        }
    }
    selector_btn_->setText(tr("SELECT PORTFOLIO  \u25BE"));
}

// ── Public setters ───────────────────────────────────────────────────────────

void PortfolioCommandBar::set_portfolios(const QVector<portfolio::Portfolio>& portfolios) {
    portfolios_ = portfolios;
    portfolio_list_->clear();
    for (const auto& p : portfolios) {
        auto* item = new QListWidgetItem(QString("%1  (%2)").arg(p.name, p.currency));
        item->setData(Qt::UserRole, p.id);
        portfolio_list_->addItem(item);
    }
    update_selector_label();
}

void PortfolioCommandBar::set_selected_portfolio(const portfolio::Portfolio& p) {
    selected_id_ = p.id;
    update_selector_label();
}

void PortfolioCommandBar::set_refreshing(bool refreshing) {
    refresh_btn_->setEnabled(!refreshing);
    refresh_btn_->setText(refreshing ? "\u23F3" : "\u21BB");
}

void PortfolioCommandBar::set_detail_view(std::optional<portfolio::DetailView> view) {
    active_detail_ = view;
    for (auto* btn : detail_btns_) {
        auto btn_view = static_cast<portfolio::DetailView>(btn->property("detailView").toInt());
        btn->setProperty("active", view.has_value() && *view == btn_view);
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
    }
}

void PortfolioCommandBar::set_has_selection(bool has) {
    trade_cluster_->setVisible(has);
    tabs_container_->setVisible(has);
    tools_cluster_->setVisible(has);
}

void PortfolioCommandBar::set_has_portfolios(bool has) {
    has_portfolios_ = has;
    trade_cluster_->setVisible(false);
    tabs_container_->setVisible(false);
    tools_cluster_->setVisible(false);
    if (!has) {
        selector_btn_->setText(tr("NO PORTFOLIOS \u2014 CREATE ONE  \u25BE"));
    }
}

void PortfolioCommandBar::set_refresh_interval(int ms) {
    for (int i = 0; i < interval_cb_->count(); ++i) {
        if (interval_cb_->itemData(i).toInt() == ms) {
            QSignalBlocker blocker(interval_cb_);
            interval_cb_->setCurrentIndex(i);
            break;
        }
    }
}

void PortfolioCommandBar::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void PortfolioCommandBar::retranslateUi() {
    // Row 1 — selector label depends on portfolio state.
    if (has_portfolios_ && !selected_id_.isEmpty()) {
        update_selector_label(); // re-emits "%1 (%2)  ▾" with the current portfolio
    } else if (has_portfolios_) {
        selector_btn_->setText(tr("SELECT PORTFOLIO  ▾"));
    } else if (selector_btn_) {
        selector_btn_->setText(tr("NO PORTFOLIOS — CREATE ONE  ▾"));
    }

    if (search_edit_)   search_edit_->setPlaceholderText(tr("Search portfolios..."));
    if (create_btn_)    create_btn_->setText(tr("+ CREATE NEW"));
    if (delete_btn_)    delete_btn_->setText(tr("DELETE"));
    if (refresh_btn_)   refresh_btn_->setToolTip(tr("Refresh portfolio data"));
    if (interval_cb_)   interval_cb_->setToolTip(tr("Auto-refresh interval"));
    if (overflow_btn_)  overflow_btn_->setToolTip(tr("More actions"));

    if (export_csv_action_)  export_csv_action_->setText(tr("Export CSV"));
    if (export_json_action_) export_json_action_->setText(tr("Export JSON"));
    if (import_action_)      import_action_->setText(tr("Import JSON…"));
    if (ffn_action_)         ffn_action_->setText(tr("FFN Analysis"));

    // Row 2 trade cluster
    if (buy_btn_)   buy_btn_->setText(tr("BUY"));
    if (sell_btn_)  sell_btn_->setText(tr("SELL"));
    if (div_btn_)   div_btn_->setText(tr("DIV"));
    if (ai_btn_)    ai_btn_->setText(tr("AI"));
    if (agent_btn_) agent_btn_->setText(tr("AGENT"));

    // Row 2 detail tabs — populated in kDetailButtons order, so iterate by index.
    constexpr int kDetailCount = static_cast<int>(sizeof(kDetailButtons) / sizeof(kDetailButtons[0]));
    for (int i = 0; i < detail_btns_.size() && i < kDetailCount; ++i) {
        detail_btns_[i]->setText(tr(kDetailButtons[i].label));
    }
}

void PortfolioCommandBar::refresh_theme() {
    setStyleSheet(QString("#portfolioCommandBar { background:%1; border-bottom:2px solid %2; }")
                      .arg(ui::colors::BG_SURFACE(), ui::colors::AMBER()));

    apply_row1_styles();
    apply_row2_styles();

    dropdown_->setStyleSheet(
        QString("background:%1; border:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_MED()));
}

} // namespace fincept::screens
