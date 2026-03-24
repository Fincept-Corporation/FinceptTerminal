// src/screens/equity_research/EquityResearchScreen.cpp
#include "screens/equity_research/EquityResearchScreen.h"

#include "screens/equity_research/EquityAnalysisTab.h"
#include "screens/equity_research/EquityFinancialsTab.h"
#include "screens/equity_research/EquityNewsTab.h"
#include "screens/equity_research/EquityOverviewTab.h"
#include "screens/equity_research/EquityPeersTab.h"
#include "screens/equity_research/EquityTalippTab.h"
#include "screens/equity_research/EquityTechnicalsTab.h"
#include "services/equity/EquityResearchService.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QVBoxLayout>

namespace fincept::screens {

EquityResearchScreen::EquityResearchScreen(QWidget* parent) : QWidget(parent) {
    build_ui();

    // Refresh timer — only started in showEvent
    refresh_timer_ = new QTimer(this);
    refresh_timer_->setInterval(30 * 1000); // 30s quote refresh
    connect(refresh_timer_, &QTimer::timeout, this, [this]() {
        if (!current_symbol_.isEmpty())
            services::equity::EquityResearchService::instance().load_symbol(current_symbol_);
    });

    // Wire service signals
    auto& svc = services::equity::EquityResearchService::instance();
    connect(&svc, &services::equity::EquityResearchService::search_results_loaded, this,
            &EquityResearchScreen::on_search_results_loaded);
    connect(&svc, &services::equity::EquityResearchService::quote_loaded, this, &EquityResearchScreen::on_quote_loaded);

    // Default symbol
    load_symbol("AAPL");
}

void EquityResearchScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    refresh_timer_->start();
}

void EquityResearchScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    refresh_timer_->stop();
}

// ── UI construction ───────────────────────────────────────────────────────────
void EquityResearchScreen::build_ui() {
    setStyleSheet(QString("background:%1; color:%2;").arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY));

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_search_bar());
    vl->addWidget(build_quote_bar());

    // ── Tabs ─────────────────────────────────────────────────────────────────
    tab_widget_ = new QTabWidget;
    tab_widget_->setDocumentMode(true);
    tab_widget_->setStyleSheet(QString(R"(
        QTabWidget::pane { border:0; background:%1; }
        QTabBar::tab {
            background:%2; color:%3; padding:8px 18px;
            border:0; border-bottom:2px solid transparent;
            font-size:12px; text-transform:uppercase; letter-spacing:1px;
        }
        QTabBar::tab:selected { color:%4; border-bottom:2px solid %4; }
        QTabBar::tab:hover { color:%5; }
    )")
                                   .arg(ui::colors::BG_BASE, ui::colors::BG_SURFACE, ui::colors::TEXT_SECONDARY,
                                        ui::colors::AMBER, ui::colors::TEXT_PRIMARY));

    overview_tab_ = new EquityOverviewTab;
    financials_tab_ = new EquityFinancialsTab;
    analysis_tab_ = new EquityAnalysisTab;
    technicals_tab_ = new EquityTechnicalsTab;
    talipp_tab_ = new EquityTalippTab;
    peers_tab_ = new EquityPeersTab;
    news_tab_ = new EquityNewsTab;

    tab_widget_->addTab(overview_tab_, "Overview");
    tab_widget_->addTab(financials_tab_, "Financials");
    tab_widget_->addTab(analysis_tab_, "Analysis");
    tab_widget_->addTab(technicals_tab_, "Technicals");
    tab_widget_->addTab(talipp_tab_, "TALIpp");
    tab_widget_->addTab(peers_tab_, "Peers");
    tab_widget_->addTab(news_tab_, "News");

    connect(tab_widget_, &QTabWidget::currentChanged, this, &EquityResearchScreen::on_tab_changed);

    vl->addWidget(tab_widget_, 1);
}

QWidget* EquityResearchScreen::build_search_bar() {
    auto* container = new QWidget;
    container->setFixedHeight(52);
    container->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));

    auto* hl = new QHBoxLayout(container);
    hl->setContentsMargins(16, 8, 16, 8);
    hl->setSpacing(8);

    // Screen title
    auto* title = new QLabel("EQUITY RESEARCH");
    title->setStyleSheet(
        QString("color:%1; font-size:13px; font-weight:700; letter-spacing:2px;").arg(ui::colors::AMBER));
    hl->addWidget(title);
    hl->addStretch();

    // Search area (input + dropdown)
    search_container_ = new QWidget;
    search_container_->setFixedWidth(340);
    auto* svl = new QVBoxLayout(search_container_);
    svl->setContentsMargins(0, 0, 0, 0);
    svl->setSpacing(0);

    search_edit_ = new QLineEdit;
    search_edit_->setPlaceholderText("Search symbol or company…");
    search_edit_->setStyleSheet(
        QString(R"(
        QLineEdit {
            background:%1; color:%2; border:1px solid %3;
            border-radius:4px; padding:6px 12px; font-size:13px;
        }
        QLineEdit:focus { border-color:%4; }
    )")
            .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM, ui::colors::AMBER));

    suggest_list_ = new QListWidget;
    suggest_list_->setStyleSheet(QString(R"(
        QListWidget {
            background:%1; border:1px solid %2;
            border-top:0; font-size:13px; color:%3;
        }
        QListWidget::item { padding:6px 12px; }
        QListWidget::item:hover { background:%4; }
        QListWidget::item:selected { background:%5; color:%6; }
    )")
                                     .arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM, ui::colors::TEXT_PRIMARY,
                                          ui::colors::BG_HOVER, ui::colors::AMBER, ui::colors::BG_BASE));
    suggest_list_->hide();
    suggest_list_->setMaximumHeight(240);

    svl->addWidget(search_edit_);
    svl->addWidget(suggest_list_);

    hl->addWidget(search_container_);

    connect(search_edit_, &QLineEdit::textChanged, this, &EquityResearchScreen::on_search_text_changed);
    connect(search_edit_, &QLineEdit::returnPressed, this, &EquityResearchScreen::on_search_committed);
    connect(suggest_list_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        QString sym = item->data(Qt::UserRole).toString();
        if (sym.isEmpty())
            sym = item->text().split(' ').first();
        search_edit_->setText(sym);
        suggest_list_->hide();
        load_symbol(sym);
    });

    return container;
}

QWidget* EquityResearchScreen::build_quote_bar() {
    auto* bar = new QFrame;
    bar->setFixedHeight(40);
    bar->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);
    hl->setSpacing(24);

    auto make_label = [&](const QString& txt, const QString& color = "") -> QLabel* {
        auto* l = new QLabel(txt);
        QString style = "font-size:12px; font-weight:600;";
        if (!color.isEmpty())
            style += "color:" + color + ";";
        else
            style += "color:" + QString(ui::colors::TEXT_SECONDARY) + ";";
        l->setStyleSheet(style);
        hl->addWidget(l);
        return l;
    };

    sym_label_ = make_label("—", ui::colors::AMBER);
    price_label_ = make_label("—", ui::colors::TEXT_PRIMARY);
    change_label_ = make_label("—");
    vol_label_ = make_label("VOL: —");
    hl_label_ = make_label("H/L: —");
    mktcap_label_ = make_label("MKT CAP: —");
    rec_label_ = make_label("—");
    hl->addStretch();

    return bar;
}

// ── Slots ─────────────────────────────────────────────────────────────────────
void EquityResearchScreen::on_search_text_changed(const QString& text) {
    if (text.trimmed().length() < 2) {
        suggest_list_->hide();
        return;
    }
    services::equity::EquityResearchService::instance().schedule_search(text.trimmed());
}

void EquityResearchScreen::on_search_committed() {
    QString sym = search_edit_->text().trimmed().toUpper();
    if (sym.isEmpty())
        return;
    suggest_list_->hide();
    load_symbol(sym);
}

void EquityResearchScreen::on_search_results_loaded(QVector<services::equity::SearchResult> results) {
    last_search_results_ = results;
    suggest_list_->clear();
    for (const auto& r : results) {
        auto* item = new QListWidgetItem(QString("%1  %2").arg(r.symbol, r.name));
        item->setData(Qt::UserRole, r.symbol);
        suggest_list_->addItem(item);
    }
    suggest_list_->setVisible(!results.isEmpty());
    suggest_list_->setFixedHeight(qMin(results.size() * 34, 240));
}

void EquityResearchScreen::on_quote_loaded(services::equity::QuoteData q) {
    update_quote_bar(q);
}

void EquityResearchScreen::on_tab_changed(int index) {
    if (current_symbol_.isEmpty())
        return;
    // Lazy-load data for tab when first visited
    switch (index) {
        case 1:
            financials_tab_->set_symbol(current_symbol_);
            break;
        case 2:
            analysis_tab_->set_symbol(current_symbol_);
            break;
        case 3:
            technicals_tab_->set_symbol(current_symbol_);
            break;
        case 4:
            talipp_tab_->set_symbol(current_symbol_);
            break;
        case 5:
            peers_tab_->set_symbol(current_symbol_);
            break;
        case 6:
            news_tab_->set_symbol(current_symbol_);
            break;
        default:
            break;
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────
void EquityResearchScreen::load_symbol(const QString& symbol) {
    if (symbol.isEmpty() || symbol == current_symbol_)
        return;
    current_symbol_ = symbol;
    search_edit_->setText(symbol);
    suggest_list_->hide();

    // Reset quote bar
    sym_label_->setText(symbol);
    price_label_->setText("Loading…");

    // Overview always loads (tab 0 is default)
    overview_tab_->set_symbol(symbol);

    // Load data for currently active tab
    on_tab_changed(tab_widget_->currentIndex());

    // Trigger quote + info + historical
    services::equity::EquityResearchService::instance().load_symbol(symbol);
}

void EquityResearchScreen::update_quote_bar(const services::equity::QuoteData& q) {
    if (q.symbol != current_symbol_)
        return;

    sym_label_->setText(q.symbol);
    price_label_->setText(QString("$%1").arg(q.price, 0, 'f', 2));

    bool up = q.change_pct >= 0;
    QString arrow = up ? "▲" : "▼";
    QString chg_color = up ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
    change_label_->setText(QString("%1%2  %3%4%")
                               .arg(up ? "+" : "")
                               .arg(q.change, 0, 'f', 2)
                               .arg(arrow)
                               .arg(qAbs(q.change_pct), 0, 'f', 2));
    change_label_->setStyleSheet(QString("font-size:12px; font-weight:600; color:%1;").arg(chg_color));

    auto fmt_vol = [](double v) -> QString {
        if (v >= 1e9)
            return QString("%1B").arg(v / 1e9, 0, 'f', 1);
        if (v >= 1e6)
            return QString("%1M").arg(v / 1e6, 0, 'f', 1);
        if (v >= 1e3)
            return QString("%1K").arg(v / 1e3, 0, 'f', 0);
        return QString::number(static_cast<qint64>(v));
    };

    vol_label_->setText("VOL: " + fmt_vol(q.volume));
    hl_label_->setText(QString("H:%1  L:%2").arg(q.high, 0, 'f', 2).arg(q.low, 0, 'f', 2));
}

} // namespace fincept::screens
