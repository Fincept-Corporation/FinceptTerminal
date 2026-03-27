// src/screens/algo_trading/StrategyListPanel.cpp
#include "screens/algo_trading/StrategyListPanel.h"

#include "core/logging/Logger.h"
#include "services/algo_trading/AlgoTradingService.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

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

    // ── Deploy form (hidden until DEPLOY clicked) ────────────────────────────
    auto* deploy_form = new QWidget(card);
    deploy_form->setVisible(false);
    deploy_form->setStyleSheet(QString("background: %1; border-top: 1px solid %2;")
                                   .arg(fincept::ui::colors::BG_BASE, fincept::ui::colors::BORDER_DIM));
    auto* form_hl = new QHBoxLayout(deploy_form);
    form_hl->setContentsMargins(0, 6, 0, 0);
    form_hl->setSpacing(8);

    auto make_field = [&](const QString& label_text, QWidget* input_widget) {
        auto* col = new QWidget(deploy_form);
        auto* cvl = new QVBoxLayout(col);
        cvl->setContentsMargins(0, 0, 0, 0);
        cvl->setSpacing(2);
        auto* lbl = new QLabel(label_text, col);
        lbl->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: 700; %3"
                                   " background: transparent; border: none;")
                               .arg(fincept::ui::colors::TEXT_TERTIARY)
                               .arg(fincept::ui::fonts::TINY)
                               .arg(kMonoFont()));
        cvl->addWidget(lbl);
        input_widget->setParent(col);
        cvl->addWidget(input_widget);
        form_hl->addWidget(col);
    };

    auto* sym_input = new QLineEdit(deploy_form);
    sym_input->setPlaceholderText("RELIANCE");
    sym_input->setText("RELIANCE");
    sym_input->setFixedHeight(26);
    sym_input->setStyleSheet(kInputStyle());
    make_field("SYMBOL", sym_input);

    auto* mode_combo = new QComboBox(deploy_form);
    mode_combo->addItems({"paper", "live"});
    mode_combo->setFixedHeight(26);
    mode_combo->setStyleSheet(
        QString("QComboBox { background: %1; color: %2; border: 1px solid %3; padding: 2px 6px;"
                " font-size: %4px; %5 }"
                "QComboBox::drop-down { border: none; }"
                "QComboBox QAbstractItemView { background: %1; color: %2; border: 1px solid %3;"
                " selection-background-color: %6; %5 }")
            .arg(fincept::ui::colors::BG_SURFACE, fincept::ui::colors::TEXT_PRIMARY,
                 fincept::ui::colors::BORDER_DIM)
            .arg(fincept::ui::fonts::SMALL)
            .arg(kMonoFont())
            .arg(fincept::ui::colors::BG_HOVER));
    make_field("MODE", mode_combo);

    auto* tf_combo = new QComboBox(deploy_form);
    tf_combo->addItems(algo_timeframes());
    tf_combo->setCurrentIndex(algo_timeframes().indexOf("1d"));
    tf_combo->setFixedHeight(26);
    tf_combo->setStyleSheet(mode_combo->styleSheet());
    make_field("TIMEFRAME", tf_combo);

    auto* qty_spin = new QDoubleSpinBox(deploy_form);
    qty_spin->setRange(0.01, 1e7);
    qty_spin->setDecimals(2);
    qty_spin->setValue(1.0);
    qty_spin->setFixedHeight(26);
    qty_spin->setStyleSheet(
        QString("QDoubleSpinBox { background: %1; color: %2; border: 1px solid %3; padding: 2px 6px;"
                " font-size: %4px; %5 }"
                "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { width: 14px; }")
            .arg(fincept::ui::colors::BG_SURFACE, fincept::ui::colors::TEXT_PRIMARY,
                 fincept::ui::colors::BORDER_DIM)
            .arg(fincept::ui::fonts::SMALL)
            .arg(kMonoFont()));
    make_field("QTY", qty_spin);

    auto* confirm_btn = new QPushButton("GO", deploy_form);
    confirm_btn->setCursor(Qt::PointingHandCursor);
    confirm_btn->setFixedSize(48, 26);
    confirm_btn->setStyleSheet(
        QString("QPushButton { background: rgba(22,163,74,0.1); color: %1; border: 1px solid %1;"
                " font-size: %2px; font-weight: 700; %3 }"
                "QPushButton:hover { background: %1; color: %4; }")
            .arg(fincept::ui::colors::POSITIVE)
            .arg(fincept::ui::fonts::TINY)
            .arg(kMonoFont())
            .arg(fincept::ui::colors::BG_BASE));
    connect(confirm_btn, &QPushButton::clicked, card,
            [id = s.id, sym_input, mode_combo, tf_combo, qty_spin, deploy_form]() {
                QString symbol = sym_input->text().trimmed().toUpper();
                if (symbol.isEmpty())
                    symbol = "RELIANCE";
                AlgoTradingService::instance().deploy_strategy(id, symbol, mode_combo->currentText(),
                                                               tf_combo->currentText(), qty_spin->value());
                deploy_form->setVisible(false);
                LOG_INFO("AlgoTrading",
                         QString("Deploy: strategy=%1 symbol=%2 mode=%3").arg(id, symbol, mode_combo->currentText()));
            });
    auto* cancel_btn = new QPushButton("✕", deploy_form);
    cancel_btn->setCursor(Qt::PointingHandCursor);
    cancel_btn->setFixedSize(26, 26);
    cancel_btn->setStyleSheet(
        QString("QPushButton { background: transparent; color: %1; border: 1px solid %2;"
                " font-size: %3px; %4 }"
                "QPushButton:hover { color: %5; border-color: %5; }")
            .arg(fincept::ui::colors::TEXT_TERTIARY, fincept::ui::colors::BORDER_DIM)
            .arg(fincept::ui::fonts::TINY)
            .arg(kMonoFont())
            .arg(fincept::ui::colors::NEGATIVE));
    connect(cancel_btn, &QPushButton::clicked, deploy_form, [deploy_form]() { deploy_form->setVisible(false); });

    form_hl->addWidget(confirm_btn, 0, Qt::AlignBottom);
    form_hl->addWidget(cancel_btn, 0, Qt::AlignBottom);
    form_hl->addStretch();

    vl->addWidget(deploy_form);

    // ── Action buttons ───────────────────────────────────────────────────────
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
    connect(deploy_btn, &QPushButton::clicked, card, [deploy_form]() {
        deploy_form->setVisible(!deploy_form->isVisible());
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
