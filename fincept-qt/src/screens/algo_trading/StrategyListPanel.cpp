// src/screens/algo_trading/StrategyListPanel.cpp
#include "screens/algo_trading/StrategyListPanel.h"

#include "core/logging/Logger.h"
#include "services/algo_trading/AlgoTradingService.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>

// ── Shared style constants ──────────────────────────────────────────────────

namespace {

inline QString kMonoFont() {
    return QString("font-family: %1;").arg(fincept::ui::fonts::DATA_FAMILY);
}

inline QString kInputStyle() {
    return QString("QLineEdit { background: %1; border: 1px solid %2; color: %3; padding: 4px 8px;"
                   " font-size: %4px; %5 }"
                   "QLineEdit:focus { border-color: %6; }")
        .arg(fincept::ui::colors::BG_SURFACE, fincept::ui::colors::BORDER_DIM, fincept::ui::colors::TEXT_PRIMARY)
        .arg(fincept::ui::fonts::SMALL)
        .arg(kMonoFont())
        .arg(fincept::ui::colors::BORDER_BRIGHT);
}

} // namespace

namespace fincept::screens {

using namespace fincept::services::algo;

// ── Constructor ─────────────────────────────────────────────────────────────

StrategyListPanel::StrategyListPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
    connect_service();
    LOG_INFO("AlgoTrading", "StrategyListPanel constructed");
}

// ── Service connections ─────────────────────────────────────────────────────

void StrategyListPanel::connect_service() {
    auto& svc = AlgoTradingService::instance();
    connect(&svc, &AlgoTradingService::strategies_loaded, this, &StrategyListPanel::on_strategies_loaded);
    connect(&svc, &AlgoTradingService::strategy_deleted, this, [this](const QString& id) {
        Q_UNUSED(id)
        AlgoTradingService::instance().list_strategies();
    });
    connect(&svc, &AlgoTradingService::deployment_started, this,
            [](const QString& id) { LOG_INFO("AlgoTrading", QString("Deployment started: %1").arg(id)); });
    connect(&svc, &AlgoTradingService::error_occurred, this, &StrategyListPanel::on_error);
}

// ── Build strategy card ─────────────────────────────────────────────────────

QWidget* StrategyListPanel::build_strategy_card(const AlgoStrategy& s, QWidget* parent) {
    auto* card = new QWidget(parent);
    card->setStyleSheet(QString("background: %1; border: 1px solid %2;")
                            .arg(fincept::ui::colors::BG_SURFACE, fincept::ui::colors::BORDER_DIM));

    auto* vl = new QVBoxLayout(card);
    vl->setContentsMargins(12, 10, 12, 10);
    vl->setSpacing(6);

    // Top row: name + timeframe
    auto* top = new QHBoxLayout;
    top->setSpacing(8);

    auto* name_lbl = new QLabel(s.name, card);
    name_lbl->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: 700; %3"
                                    " background: transparent; border: none;")
                                .arg(fincept::ui::colors::AMBER)
                                .arg(fincept::ui::fonts::DATA)
                                .arg(kMonoFont()));
    top->addWidget(name_lbl);

    auto* tf_badge = new QLabel(s.timeframe.toUpper(), card);
    tf_badge->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: 700; %3"
                                    " padding: 2px 6px; background: rgba(8,145,178,0.08);"
                                    " border: 1px solid rgba(8,145,178,0.25);")
                                .arg(fincept::ui::colors::CYAN)
                                .arg(fincept::ui::fonts::TINY)
                                .arg(kMonoFont()));
    top->addWidget(tf_badge);
    top->addStretch();

    vl->addLayout(top);

    // Description
    if (!s.description.isEmpty()) {
        auto* desc = new QLabel(s.description, card);
        desc->setWordWrap(true);
        desc->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                                .arg(fincept::ui::colors::TEXT_SECONDARY)
                                .arg(fincept::ui::fonts::SMALL)
                                .arg(kMonoFont()));
        vl->addWidget(desc);
    }

    // Metrics row
    auto* metrics = new QHBoxLayout;
    metrics->setSpacing(16);

    auto add_metric = [&](const QString& label, const QString& value) {
        auto* w = new QWidget(card);
        auto* ml = new QVBoxLayout(w);
        ml->setContentsMargins(0, 0, 0, 0);
        ml->setSpacing(0);
        auto* lbl = new QLabel(label, w);
        lbl->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                               .arg(fincept::ui::colors::TEXT_TERTIARY)
                               .arg(fincept::ui::fonts::TINY)
                               .arg(kMonoFont()));
        auto* val = new QLabel(value, w);
        val->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: 700; %3"
                                   " background: transparent; border: none;")
                               .arg(fincept::ui::colors::TEXT_PRIMARY)
                               .arg(fincept::ui::fonts::SMALL)
                               .arg(kMonoFont()));
        ml->addWidget(lbl);
        ml->addWidget(val);
        metrics->addWidget(w);
    };

    add_metric("ENTRY", QString::number(s.entry_conditions.size()));
    add_metric("EXIT", QString::number(s.exit_conditions.size()));
    add_metric("SL", s.stop_loss > 0 ? QString("%1%").arg(s.stop_loss, 0, 'f', 1) : "OFF");
    add_metric("TP", s.take_profit > 0 ? QString("%1%").arg(s.take_profit, 0, 'f', 1) : "OFF");

    metrics->addStretch();

    vl->addLayout(metrics);

    // Action buttons
    auto* actions = new QHBoxLayout;
    actions->setSpacing(8);
    actions->addStretch();

    auto* deploy_btn = new QPushButton("DEPLOY", card);
    deploy_btn->setCursor(Qt::PointingHandCursor);
    deploy_btn->setFixedHeight(28);
    deploy_btn->setStyleSheet(QString("QPushButton { background: rgba(22,163,74,0.1); color: %1; border: 1px solid %1;"
                                      " font-size: %2px; font-weight: 700; %3 padding: 4px 16px; }"
                                      "QPushButton:hover { background: %1; color: %4; }")
                                  .arg(fincept::ui::colors::POSITIVE)
                                  .arg(fincept::ui::fonts::TINY)
                                  .arg(kMonoFont())
                                  .arg(fincept::ui::colors::BG_BASE));
    connect(deploy_btn, &QPushButton::clicked, card, [id = s.id]() {
        AlgoTradingService::instance().deploy_strategy(id, "RELIANCE", "paper", "1d", 1.0);
        LOG_INFO("AlgoTrading", QString("Deploy requested for strategy: %1").arg(id));
    });
    actions->addWidget(deploy_btn);

    auto* delete_btn = new QPushButton("DELETE", card);
    delete_btn->setCursor(Qt::PointingHandCursor);
    delete_btn->setFixedHeight(28);
    delete_btn->setStyleSheet(QString("QPushButton { background: transparent; color: %1; border: 1px solid %1;"
                                      " font-size: %2px; font-weight: 700; %3 padding: 4px 16px; }"
                                      "QPushButton:hover { background: rgba(220,38,38,0.1); }")
                                  .arg(fincept::ui::colors::NEGATIVE)
                                  .arg(fincept::ui::fonts::TINY)
                                  .arg(kMonoFont()));
    connect(delete_btn, &QPushButton::clicked, card, [id = s.id]() {
        AlgoTradingService::instance().delete_strategy(id);
        LOG_INFO("AlgoTrading", QString("Delete requested for strategy: %1").arg(id));
    });
    actions->addWidget(delete_btn);

    vl->addLayout(actions);

    return card;
}

// ── Build UI ────────────────────────────────────────────────────────────────

void StrategyListPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Top bar: search + count
    auto* top_bar = new QWidget(this);
    top_bar->setFixedHeight(44);
    top_bar->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;")
                               .arg(fincept::ui::colors::BG_RAISED, fincept::ui::colors::BORDER_DIM));
    auto* top_hl = new QHBoxLayout(top_bar);
    top_hl->setContentsMargins(12, 0, 12, 0);
    top_hl->setSpacing(12);

    auto* search_icon = new QLabel("SEARCH", top_bar);
    search_icon->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: 700; %3"
                                       " background: transparent; border: none;")
                                   .arg(fincept::ui::colors::TEXT_TERTIARY)
                                   .arg(fincept::ui::fonts::TINY)
                                   .arg(kMonoFont()));
    top_hl->addWidget(search_icon);

    search_edit_ = new QLineEdit(top_bar);
    search_edit_->setPlaceholderText("Filter strategies...");
    search_edit_->setStyleSheet(kInputStyle());
    search_edit_->setFixedHeight(28);
    top_hl->addWidget(search_edit_, 1);

    count_label_ = new QLabel("0 strategies", top_bar);
    count_label_->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: 700; %3"
                                        " background: transparent; border: none;")
                                    .arg(fincept::ui::colors::TEXT_SECONDARY)
                                    .arg(fincept::ui::fonts::TINY)
                                    .arg(kMonoFont()));
    top_hl->addWidget(count_label_);

    root->addWidget(top_bar);

    // Scroll area for cards
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QString("QScrollArea { background: %1; border: none; }"
                                  "QScrollBar:vertical { background: %1; width: 6px; }"
                                  "QScrollBar::handle:vertical { background: %2; }"
                                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
                              .arg(fincept::ui::colors::BG_BASE, fincept::ui::colors::BORDER_MED));

    auto* list_widget = new QWidget;
    list_widget->setStyleSheet(QString("background: %1;").arg(fincept::ui::colors::BG_BASE));
    list_layout_ = new QVBoxLayout(list_widget);
    list_layout_->setContentsMargins(16, 12, 16, 12);
    list_layout_->setSpacing(8);
    list_layout_->addStretch();

    scroll->setWidget(list_widget);
    root->addWidget(scroll, 1);

    // Filter strategies when search text changes
    connect(search_edit_, &QLineEdit::textChanged, this, [this](const QString& text) {
        QString filter = text.trimmed().toLower();
        for (int i = 0; i < list_layout_->count(); ++i) {
            auto* item = list_layout_->itemAt(i);
            auto* w = item ? item->widget() : nullptr;
            if (!w)
                continue;
            if (filter.isEmpty()) {
                w->setVisible(true);
            } else {
                QString card_name = w->property("strategy_name").toString().toLower();
                w->setVisible(card_name.contains(filter));
            }
        }
    });
}

// ── Slots ───────────────────────────────────────────────────────────────────

void StrategyListPanel::on_strategies_loaded(QVector<AlgoStrategy> strategies) {
    // Clear existing cards (keep the stretch at end)
    while (list_layout_->count() > 0) {
        auto* item = list_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    for (const auto& s : strategies) {
        auto* card = build_strategy_card(s, nullptr);
        card->setProperty("strategy_name", s.name);
        list_layout_->addWidget(card);
    }
    list_layout_->addStretch();

    count_label_->setText(
        QString("%1 %2").arg(strategies.size()).arg(strategies.size() == 1 ? "strategy" : "strategies"));

    LOG_INFO("AlgoTrading", QString("Loaded %1 strategies").arg(strategies.size()));
}

void StrategyListPanel::on_error(const QString& context, const QString& msg) {
    LOG_ERROR("AlgoTrading", QString("StrategyList error [%1]: %2").arg(context, msg));
}

} // namespace fincept::screens
