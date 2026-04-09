// src/screens/portfolio/PortfolioCommandBar.cpp
#include "screens/portfolio/PortfolioCommandBar.h"

#include "ui/theme/Theme.h"

#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens {

// Detail view button definitions
struct DetailBtnDef {
    const char* label;
    portfolio::DetailView view;
    const char* color;
};

static const DetailBtnDef kDetailButtons[] = {
    {"SECTORS", portfolio::DetailView::AnalyticsSectors, "#0891b2"},
    {"PERF/RISK", portfolio::DetailView::PerfRisk, "#2563eb"},
    {"OPTIMIZE", portfolio::DetailView::Optimization, "#d97706"},
    {"QUANTSTATS", portfolio::DetailView::QuantStats, "#16a34a"},
    {"REPORTS", portfolio::DetailView::ReportsPme, "#ca8a04"},
    {"INDICES", portfolio::DetailView::Indices, "#9333ea"},
    {"RISK", portfolio::DetailView::RiskMgmt, "#dc2626"},
    {"PLANNING", portfolio::DetailView::Planning, "#0d9488"},
    {"ECONOMICS", portfolio::DetailView::Economics, "#6366f1"},
};

PortfolioCommandBar::PortfolioCommandBar(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void PortfolioCommandBar::build_ui() {
    setFixedHeight(36);
    setObjectName("portfolioCommandBar");
    setStyleSheet(QString("#portfolioCommandBar { background:%1; border-bottom:2px solid %2; }")
                      .arg(ui::colors::BG_SURFACE, ui::colors::AMBER));

    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // Wrap entire bar in scroll area to prevent horizontal overflow
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background:transparent; border:none; }");

    auto* inner = new QWidget(this);
    inner->setStyleSheet("background:transparent;");
    auto* layout = new QHBoxLayout(inner);
    layout->setContentsMargins(6, 0, 6, 0);
    layout->setSpacing(3);

    build_portfolio_selector(layout);

    // Inline stats (wrapped in container for show/hide)
    stats_container_ = new QWidget(this);
    stats_container_->setStyleSheet("background:transparent;");
    auto* stats_layout = new QHBoxLayout(stats_container_);
    stats_layout->setContentsMargins(0, 0, 0, 0);
    stats_layout->setSpacing(3);
    stats_layout->addWidget(nav_label_);
    stats_layout->addWidget(pnl_label_);
    stats_layout->addWidget(day_label_);
    stats_layout->addWidget(pos_label_);
    layout->addWidget(stats_container_);

    // Action buttons (wrapped for show/hide)
    actions_container_ = new QWidget(this);
    actions_container_->setStyleSheet("background:transparent;");
    auto* actions_layout = new QHBoxLayout(actions_container_);
    actions_layout->setContentsMargins(0, 0, 0, 0);
    actions_layout->setSpacing(3);
    build_action_buttons(actions_layout);
    layout->addWidget(actions_container_);

    // Separator
    auto* sep = new QWidget(this);
    sep->setFixedSize(1, 18);
    sep->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_MED));
    layout->addWidget(sep);

    // Detail buttons + AI/Agent (wrapped for show/hide)
    details_container_ = new QWidget(this);
    details_container_->setStyleSheet("background:transparent;");
    auto* details_layout = new QHBoxLayout(details_container_);
    details_layout->setContentsMargins(0, 0, 0, 0);
    details_layout->setSpacing(2);
    build_detail_buttons(details_layout);

    // AI + Agent buttons inside details container
    auto make_tool_btn = [&](const QString& text, const char* color, auto signal) {
        auto* btn = new QPushButton(text);
        btn->setFixedHeight(20);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %1;"
                                   "  padding:0 6px; font-size:9px; font-weight:700; }"
                                   "QPushButton:hover { background:%1; color:%2; }")
                               .arg(color, ui::colors::BG_BASE));
        connect(btn, &QPushButton::clicked, this, signal);
        details_layout->addWidget(btn);
        return btn;
    };

    make_tool_btn("AI", "#9D4EDD", &PortfolioCommandBar::ai_analyze_requested);
    make_tool_btn("AGENT", "#00D4AA", &PortfolioCommandBar::agent_run_requested);

    layout->addWidget(details_container_);

    scroll->setWidget(inner);
    outer->addWidget(scroll);

    // Initially hide everything until portfolios load
    stats_container_->hide();
    actions_container_->hide();
    details_container_->hide();
}

void PortfolioCommandBar::build_portfolio_selector(QHBoxLayout* layout) {
    // Selector button (dropdown trigger)
    selector_btn_ = new QPushButton("SELECT PORTFOLIO \u25BE");
    selector_btn_->setFixedHeight(22);
    selector_btn_->setMaximumWidth(240);
    selector_btn_->setCursor(Qt::PointingHandCursor);
    selector_btn_->setStyleSheet(
        QString("QPushButton { background:%1; color:%2; border:1px solid %3;"
                "  padding:0 12px; font-size:10px; font-weight:700; letter-spacing:0.5px; }"
                "QPushButton:hover { border-color:%4; color:%4; }")
            .arg(ui::colors::BG_RAISED, ui::colors::TEXT_SECONDARY, ui::colors::BORDER_MED, ui::colors::AMBER));
    connect(selector_btn_, &QPushButton::clicked, this, &PortfolioCommandBar::toggle_dropdown);
    layout->addWidget(selector_btn_);

    // Create stat labels (added to stats_container_ in build_ui, not here)
    auto make_stat = [](QLabel*& lbl) {
        lbl = new QLabel;
        lbl->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700;").arg(ui::colors::TEXT_SECONDARY));
    };
    make_stat(nav_label_);
    make_stat(pnl_label_);
    make_stat(day_label_);
    make_stat(pos_label_);

    // Dropdown panel (initially hidden, positioned absolutely)
    dropdown_ = new QWidget(this);
    dropdown_->setWindowFlags(Qt::Popup);
    dropdown_->setFixedWidth(280);
    dropdown_->setStyleSheet(
        QString("background:%1; border:1px solid %2;").arg(ui::colors::BG_SURFACE, ui::colors::BORDER_MED));

    auto* dd_layout = new QVBoxLayout(dropdown_);
    dd_layout->setContentsMargins(4, 4, 4, 4);
    dd_layout->setSpacing(2);

    // Search
    search_edit_ = new QLineEdit;
    search_edit_->setPlaceholderText("Search portfolios...");
    search_edit_->setFixedHeight(24);
    search_edit_->setStyleSheet(
        QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                "  padding:0 8px; font-size:10px; }"
                "QLineEdit:focus { border-color:%4; }")
            .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM, ui::colors::AMBER));
    dd_layout->addWidget(search_edit_);

    // List
    portfolio_list_ = new QListWidget;
    portfolio_list_->setFixedHeight(160);
    portfolio_list_->setStyleSheet(QString("QListWidget { background:%1; color:%2; border:none; font-size:10px; }"
                                           "QListWidget::item { padding:4px 8px; }"
                                           "QListWidget::item:selected { background:%3; color:%4; }"
                                           "QListWidget::item:hover { background:%5; }")
                                       .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY, ui::colors::AMBER_DIM,
                                            ui::colors::AMBER, ui::colors::BG_HOVER));
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

    // Create + Delete buttons row
    auto* btn_row = new QHBoxLayout;
    btn_row->setSpacing(4);

    auto* create_btn = new QPushButton("+ CREATE NEW");
    create_btn->setFixedHeight(22);
    create_btn->setCursor(Qt::PointingHandCursor);
    create_btn->setStyleSheet(QString("QPushButton { background:%1; color:%3; border:none;"
                                      "  font-size:9px; font-weight:700; }"
                                      "QPushButton:hover { background:%2; }")
                                  .arg(ui::colors::AMBER, ui::colors::WARNING, ui::colors::BG_BASE));
    connect(create_btn, &QPushButton::clicked, this, [this]() {
        dropdown_->hide();
        dropdown_visible_ = false;
        emit create_requested();
    });
    btn_row->addWidget(create_btn);

    auto* delete_btn = new QPushButton("DELETE");
    delete_btn->setFixedHeight(22);
    delete_btn->setCursor(Qt::PointingHandCursor);
    delete_btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %1;"
                                      "  font-size:9px; font-weight:700; }"
                                      "QPushButton:hover { background:%1; color:%2; }")
                                  .arg(ui::colors::NEGATIVE, ui::colors::BG_BASE));
    connect(delete_btn, &QPushButton::clicked, this, [this]() {
        if (!selected_id_.isEmpty()) {
            dropdown_->hide();
            dropdown_visible_ = false;
            emit delete_requested(selected_id_);
        }
    });
    btn_row->addWidget(delete_btn);

    dd_layout->addLayout(btn_row);
    dropdown_->hide();
}

void PortfolioCommandBar::build_action_buttons(QHBoxLayout* layout) {
    auto make_btn = [&](const QString& text, const char* bg, const char* fg) {
        auto* btn = new QPushButton(text);
        btn->setFixedHeight(20);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none; border-radius:2px;"
                                   "  padding:0 8px; font-size:9px; font-weight:700; letter-spacing:0.5px; }"
                                   "QPushButton:hover { opacity:0.85; }")
                               .arg(bg, fg));
        layout->addWidget(btn);
        return btn;
    };

    buy_btn_ = make_btn("BUY", ui::colors::POSITIVE, ui::colors::BG_BASE);
    sell_btn_ = make_btn("SELL", ui::colors::NEGATIVE, ui::colors::TEXT_PRIMARY);
    auto* div_btn = make_btn("DIV", ui::colors::CYAN, ui::colors::BG_BASE);

    connect(buy_btn_, &QPushButton::clicked, this, &PortfolioCommandBar::buy_requested);
    connect(sell_btn_, &QPushButton::clicked, this, &PortfolioCommandBar::sell_requested);
    connect(div_btn, &QPushButton::clicked, this, &PortfolioCommandBar::dividend_requested);

    // Refresh
    refresh_btn_ = new QPushButton("\u21BB");
    refresh_btn_->setFixedSize(20, 20);
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    refresh_btn_->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                                        "  font-size:11px; }"
                                        "QPushButton:hover { border-color:%1; }")
                                    .arg(ui::colors::AMBER, ui::colors::BORDER_MED));
    connect(refresh_btn_, &QPushButton::clicked, this, &PortfolioCommandBar::refresh_requested);
    layout->addWidget(refresh_btn_);

    // Refresh interval selector
    interval_cb_ = new QComboBox;
    interval_cb_->setFixedHeight(20);
    interval_cb_->addItem("1m", 60000);
    interval_cb_->addItem("5m", 300000);
    interval_cb_->addItem("10m", 600000);
    interval_cb_->addItem("30m", 1800000);
    interval_cb_->addItem("1h", 3600000);
    interval_cb_->addItem("3h", 10800000);
    interval_cb_->addItem("1d", 86400000);
    interval_cb_->setStyleSheet(
        QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                "  padding:0 6px; font-size:9px; font-weight:600; min-width:36px; }"
                "QComboBox::drop-down { border:none; }"
                "QComboBox QAbstractItemView { background:%1; color:%2; selection-background-color:%4; }")
            .arg(ui::colors::BG_RAISED, ui::colors::TEXT_SECONDARY, ui::colors::BORDER_DIM, ui::colors::AMBER_DIM));
    connect(interval_cb_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        int ms = interval_cb_->itemData(idx).toInt();
        emit refresh_interval_changed(ms);
    });
    layout->addWidget(interval_cb_);

    // Export buttons
    auto* export_csv = make_btn("CSV\u2193", ui::colors::BG_RAISED, ui::colors::CYAN);
    auto* export_json = make_btn("JSON\u2193", ui::colors::BG_RAISED, ui::colors::CYAN);
    auto* import_btn = make_btn("\u2191 IMPORT", ui::colors::BG_RAISED, ui::colors::CYAN);

    connect(export_csv, &QPushButton::clicked, this, &PortfolioCommandBar::export_csv_requested);
    connect(export_json, &QPushButton::clicked, this, &PortfolioCommandBar::export_json_requested);
    connect(import_btn, &QPushButton::clicked, this, &PortfolioCommandBar::import_requested);

    // FFN toggle
    ffn_btn_ = new QPushButton("FFN");
    ffn_btn_->setFixedHeight(20);
    ffn_btn_->setCheckable(true);
    ffn_btn_->setCursor(Qt::PointingHandCursor);
    ffn_btn_->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                "  padding:0 8px; font-size:9px; font-weight:700; }"
                "QPushButton:checked { background:%3; color:%4; border-color:%3; }"
                "QPushButton:hover { border-color:%3; }")
            .arg(ui::colors::TEXT_SECONDARY, ui::colors::BORDER_MED, ui::colors::AMBER, ui::colors::BG_BASE));
    connect(ffn_btn_, &QPushButton::clicked, this, &PortfolioCommandBar::ffn_toggled);
    layout->addWidget(ffn_btn_);
}

void PortfolioCommandBar::build_detail_buttons(QHBoxLayout* layout) {
    for (const auto& def : kDetailButtons) {
        auto* btn = new QPushButton(def.label);
        btn->setFixedHeight(20);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("detailView", static_cast<int>(def.view));

        QString color = def.color;
        QString rgb_str = [&]() {
            QColor c(color);
            return QString("%1,%2,%3").arg(c.red()).arg(c.green()).arg(c.blue());
        }();
        btn->setStyleSheet(
            QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                    "  padding:0 6px; font-size:8px; font-weight:700; letter-spacing:0.3px; }"
                    "QPushButton:hover { background:rgba(%5,0.12); color:%3; border-color:%3; }"
                    "QPushButton[active=\"true\"] { background:rgba(%5,0.15); color:%3; border-color:%3; }")
                .arg(color, ui::colors::BORDER_DIM, color, ui::colors::BG_BASE, rgb_str));

        connect(btn, &QPushButton::clicked, this, [this, view = def.view]() { emit detail_view_selected(view); });

        layout->addWidget(btn);
        detail_btns_.append(btn);
    }
}

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
            selector_btn_->setText(QString("%1 (%2) \u25BE").arg(p.name.toUpper(), p.currency));
            return;
        }
    }
    selector_btn_->setText("SELECT PORTFOLIO \u25BE");
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

void PortfolioCommandBar::set_summary(const portfolio::PortfolioSummary& s) {
    auto fmt = [](double v) { return QString::number(v, 'f', 2); };
    auto color = [](double v) { return v >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE; };

    nav_label_->setText(QString("NAV %1").arg(fmt(s.total_market_value)));
    nav_label_->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700;").arg(ui::colors::WARNING));

    pnl_label_->setText(
        QString("P&L %1%2%").arg(s.total_unrealized_pnl >= 0 ? "+" : "").arg(fmt(s.total_unrealized_pnl_percent)));
    pnl_label_->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700;").arg(color(s.total_unrealized_pnl)));

    day_label_->setText(
        QString("DAY %1%2%").arg(s.total_day_change >= 0 ? "+" : "").arg(fmt(s.total_day_change_percent)));
    day_label_->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700;").arg(color(s.total_day_change)));

    pos_label_->setText(QString("POS %1 | W%2/L%3").arg(s.total_positions).arg(s.gainers).arg(s.losers));
    pos_label_->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700;").arg(ui::colors::TEXT_SECONDARY));
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
    stats_container_->setVisible(has);
    actions_container_->setVisible(has);
    details_container_->setVisible(has);
}

void PortfolioCommandBar::set_has_portfolios(bool has) {
    // When zero portfolios, hide everything except selector + create
    stats_container_->setVisible(false);
    actions_container_->setVisible(false);
    details_container_->setVisible(false);
    if (!has) {
        selector_btn_->setText("NO PORTFOLIOS — CREATE ONE \u25BE");
    }
}

void PortfolioCommandBar::set_refresh_interval(int ms) {
    // Sync the combo box to show the restored value without emitting a signal
    for (int i = 0; i < interval_cb_->count(); ++i) {
        if (interval_cb_->itemData(i).toInt() == ms) {
            QSignalBlocker blocker(interval_cb_);
            interval_cb_->setCurrentIndex(i);
            break;
        }
    }
}

void PortfolioCommandBar::refresh_theme() {
    setStyleSheet(QString("#portfolioCommandBar { background:%1; border-bottom:2px solid %2; }")
                      .arg(ui::colors::BG_SURFACE, ui::colors::AMBER));

    selector_btn_->setStyleSheet(
        QString("QPushButton { background:%1; color:%2; border:1px solid %3;"
                "  padding:0 12px; font-size:10px; font-weight:700; letter-spacing:0.5px; }"
                "QPushButton:hover { border-color:%4; color:%4; }")
            .arg(ui::colors::BG_RAISED, ui::colors::TEXT_SECONDARY, ui::colors::BORDER_MED, ui::colors::AMBER));

    dropdown_->setStyleSheet(
        QString("background:%1; border:1px solid %2;").arg(ui::colors::BG_SURFACE, ui::colors::BORDER_MED));

    // Detail buttons keep their per-view accent colors but refresh base tokens
    for (auto* btn : detail_btns_) {
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
    }
}

} // namespace fincept::screens
