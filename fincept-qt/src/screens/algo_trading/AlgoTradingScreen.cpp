// src/screens/algo_trading/AlgoTradingScreen.cpp
#include "screens/algo_trading/AlgoTradingScreen.h"

#include "core/logging/Logger.h"
#include "screens/algo_trading/DeploymentDashboard.h"
#include "screens/algo_trading/ScannerPanel.h"
#include "screens/algo_trading/StrategyBuilderPanel.h"
#include "screens/algo_trading/StrategyListPanel.h"
#include "services/algo_trading/AlgoTradingService.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::services::algo;

AlgoTradingScreen::AlgoTradingScreen(QWidget* parent) : QWidget(parent) {
    build_ui();

    poll_timer_ = new QTimer(this);
    poll_timer_->setInterval(5000); // 5s deployment polling
    connect(poll_timer_, &QTimer::timeout, this, [this]() {
        if (active_tab_ == 3) // Dashboard tab
            AlgoTradingService::instance().list_deployments();
    });

    // Keep deploy count badge in sync whenever deployments are loaded
    connect(&AlgoTradingService::instance(), &AlgoTradingService::deployments_loaded, this,
            [this](const QVector<AlgoDeployment>& deps) {
                int active = 0;
                for (const auto& d : deps) {
                    if (d.status == "running" || d.status == "starting")
                        ++active;
                }
                deploy_count_label_->setText(QString("%1 LIVE").arg(active));
            });

    LOG_INFO("AlgoTrading", "Screen constructed");
}

void AlgoTradingScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    poll_timer_->start();
    if (first_show_) {
        first_show_ = false;
        on_tab_changed(0);
    }
}

void AlgoTradingScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    poll_timer_->stop();
}

void AlgoTradingScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_top_bar());

    content_stack_ = new QStackedWidget(this);
    builder_ = new StrategyBuilderPanel(this);
    strategies_ = new StrategyListPanel(this);
    scanner_ = new ScannerPanel(this);
    dashboard_ = new DeploymentDashboard(this);

    content_stack_->addWidget(builder_);
    content_stack_->addWidget(strategies_);
    content_stack_->addWidget(scanner_);
    content_stack_->addWidget(dashboard_);
    root->addWidget(content_stack_, 1);

    root->addWidget(build_status_bar());
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
}

QWidget* AlgoTradingScreen::build_top_bar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(44);
    bar->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(12);

    auto* brand = new QLabel("ALGO TRADING", bar);
    brand->setStyleSheet(QString("color:#FF6B35; font-size:%1px; font-weight:700; font-family:%2;"
                                 "padding:4px 12px; background:rgba(255,107,53,0.08);"
                                 "border:1px solid rgba(255,107,53,0.25); border-radius:2px;")
                             .arg(ui::fonts::TINY)
                             .arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(brand);

    auto* div = new QWidget(bar);
    div->setFixedSize(1, 20);
    div->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM));
    hl->addWidget(div);

    // Tab buttons
    QStringList tabs = {"BUILDER", "MY STRATEGIES", "SCANNER", "DASHBOARD"};
    QStringList colors = {"#FF6B35", "#00E5FF", "#FFC400", "#00D66F"};

    for (int i = 0; i < tabs.size(); ++i) {
        auto* btn = new QPushButton(tabs[i], bar);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString("QPushButton { color:%1; font-size:9px; font-family:%2;"
                                   "padding:4px 12px; border:1px solid transparent; border-radius:2px;"
                                   "background:transparent; font-weight:400; }"
                                   "QPushButton:hover { background:rgba(%3,0.1); color:%4; }")
                               .arg(ui::colors::TEXT_TERTIARY)
                               .arg(ui::fonts::DATA_FAMILY)
                               .arg(colors[i].mid(1))
                               .arg(colors[i]));
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_tab_changed(i); });
        hl->addWidget(btn);
        tab_buttons_.append(btn);
    }

    hl->addStretch(1);

    // Deployment count badge
    deploy_count_label_ = new QLabel("0 LIVE", bar);
    deploy_count_label_->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; font-family:%2;"
                                               "padding:3px 8px; background:rgba(22,163,74,0.08);"
                                               "border:1px solid rgba(22,163,74,0.25); border-radius:2px;")
                                           .arg(ui::colors::POSITIVE)
                                           .arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(deploy_count_label_);

    return bar;
}

QWidget* AlgoTradingScreen::build_status_bar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(24);
    bar->setStyleSheet(
        QString("background:%1; border-top:1px solid %2;").arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(16);
    auto s =
        QString("color:%1; font-size:8px; font-family:%2;").arg(ui::colors::TEXT_TERTIARY).arg(ui::fonts::DATA_FAMILY);
    auto* l1 = new QLabel("ENGINE:", bar);
    l1->setStyleSheet(s);
    auto* v1 = new QLabel("ALGO v1.0", bar);
    v1->setStyleSheet(QString("color:%1; font-size:8px; font-weight:700; font-family:%2;")
                          .arg(ui::colors::TEXT_PRIMARY)
                          .arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(l1);
    hl->addWidget(v1);
    hl->addStretch();
    status_label_ = new QLabel("IDLE", bar);
    status_label_->setStyleSheet(QString("color:%1; font-size:8px; font-weight:700; font-family:%2;")
                                     .arg(ui::colors::POSITIVE)
                                     .arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(status_label_);
    return bar;
}

void AlgoTradingScreen::on_tab_changed(int index) {
    if (index < 0 || index >= tab_buttons_.size())
        return;
    active_tab_ = index;
    content_stack_->setCurrentIndex(index);
    update_tab_buttons();

    // Refresh data when switching tabs
    if (index == 1)
        AlgoTradingService::instance().list_strategies();
    if (index == 3)
        AlgoTradingService::instance().list_deployments();
}

void AlgoTradingScreen::update_tab_buttons() {
    QStringList colors = {"#FF6B35", "#00E5FF", "#FFC400", "#00D66F"};
    for (int i = 0; i < tab_buttons_.size(); ++i) {
        bool active = (i == active_tab_);
        tab_buttons_[i]->setStyleSheet(QString("QPushButton { color:%1; font-size:9px; font-family:%2;"
                                               "padding:4px 12px; border:1px solid %3; border-radius:2px;"
                                               "background:%4; font-weight:%5; }")
                                           .arg(active ? colors[i] : ui::colors::TEXT_TERTIARY)
                                           .arg(ui::fonts::DATA_FAMILY)
                                           .arg(active ? QString("rgba(%1,0.3)").arg(colors[i].mid(1)) : "transparent")
                                           .arg(active ? QString("rgba(%1,0.12)").arg(colors[i].mid(1)) : "transparent")
                                           .arg(active ? "700" : "400"));
    }
}

} // namespace fincept::screens
