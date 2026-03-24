// src/screens/algo_trading/StrategyBuilderPanel.cpp
#include "screens/algo_trading/StrategyBuilderPanel.h"

#include "core/logging/Logger.h"
#include "services/algo_trading/AlgoTradingService.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QScrollArea>
#include <QUuid>

// ── Shared style constants ──────────────────────────────────────────────────

namespace {

inline QString kMonoFont() {
    return QString("font-family: %1;").arg(fincept::ui::fonts::DATA_FAMILY);
}

inline QString kLabelStyle() {
    return QString("color: %1; font-size: %2px; font-weight: 700; letter-spacing: 0.5px; %3"
                   "background: transparent; border: none;")
        .arg(fincept::ui::colors::TEXT_SECONDARY)
        .arg(fincept::ui::fonts::TINY)
        .arg(kMonoFont());
}

inline QString kSectionLabel() {
    return QString("color: %1; font-size: %2px; font-weight: 700; letter-spacing: 0.5px; %3"
                   "background: transparent; border: none;")
        .arg(fincept::ui::colors::AMBER)
        .arg(fincept::ui::fonts::TINY)
        .arg(kMonoFont());
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

inline QString kComboStyle() {
    return QString("QComboBox { background: %1; color: %2; border: 1px solid %3; padding: 4px 8px;"
                   " font-size: %4px; %5 }"
                   "QComboBox::drop-down { border: none; }"
                   "QComboBox QAbstractItemView { background: %1; color: %2; border: 1px solid %3;"
                   " selection-background-color: %6; %5 }")
        .arg(fincept::ui::colors::BG_SURFACE, fincept::ui::colors::TEXT_PRIMARY, fincept::ui::colors::BORDER_DIM)
        .arg(fincept::ui::fonts::SMALL)
        .arg(kMonoFont())
        .arg(fincept::ui::colors::BG_HOVER);
}

inline QString kSpinStyle() {
    return QString("QDoubleSpinBox { background: %1; color: %2; border: 1px solid %3; padding: 4px 8px;"
                   " font-size: %4px; %5 }"
                   "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { width: 14px; }")
        .arg(fincept::ui::colors::BG_SURFACE, fincept::ui::colors::TEXT_PRIMARY, fincept::ui::colors::BORDER_DIM)
        .arg(fincept::ui::fonts::SMALL)
        .arg(kMonoFont());
}

inline QString kBtnStyle() {
    return QString("QPushButton { background: %1; color: %2; border: 1px solid %3; padding: 4px 12px;"
                   " font-size: %4px; font-weight: 700; %5 }"
                   "QPushButton:hover { background: %6; color: %7; }")
        .arg(fincept::ui::colors::BG_RAISED, fincept::ui::colors::TEXT_SECONDARY, fincept::ui::colors::BORDER_DIM)
        .arg(fincept::ui::fonts::TINY)
        .arg(kMonoFont())
        .arg(fincept::ui::colors::BG_HOVER, fincept::ui::colors::TEXT_PRIMARY);
}

} // namespace

namespace fincept::screens {

using namespace fincept::services::algo;

// ── Constructor ─────────────────────────────────────────────────────────────

StrategyBuilderPanel::StrategyBuilderPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
    connect_service();
    LOG_INFO("AlgoTrading", "StrategyBuilderPanel constructed");
}

// ── Service connections ─────────────────────────────────────────────────────

void StrategyBuilderPanel::connect_service() {
    auto& svc = AlgoTradingService::instance();
    connect(&svc, &AlgoTradingService::strategy_saved, this, [this](const QString& id) {
        if (status_label_)
            status_label_->setText(QString("Strategy saved: %1").arg(id));
        status_label_->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                                         .arg(fincept::ui::colors::POSITIVE)
                                         .arg(fincept::ui::fonts::SMALL)
                                         .arg(kMonoFont()));
    });
    connect(&svc, &AlgoTradingService::backtest_result, this, &StrategyBuilderPanel::on_backtest_result);
    connect(&svc, &AlgoTradingService::error_occurred, this, &StrategyBuilderPanel::on_error);
}

// ── Build condition row ─────────────────────────────────────────────────────

QWidget* StrategyBuilderPanel::build_condition_row(QWidget* parent) {
    auto* row = new QWidget(parent);
    row->setStyleSheet(QString("background: %1; border: 1px solid %2;")
                           .arg(fincept::ui::colors::BG_SURFACE, fincept::ui::colors::BORDER_DIM));
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(6, 4, 6, 4);
    hl->setSpacing(4);

    // Indicator combo
    auto* ind_combo = new QComboBox(row);
    ind_combo->setStyleSheet(kComboStyle());
    ind_combo->setFixedHeight(28);
    ind_combo->setMinimumWidth(120);
    const auto indicators = algo_indicators();
    for (const auto& ind : indicators)
        ind_combo->addItem(ind.label, ind.id);

    // Field combo
    auto* field_combo = new QComboBox(row);
    field_combo->setStyleSheet(kComboStyle());
    field_combo->setFixedHeight(28);
    field_combo->setMinimumWidth(90);

    // Populate fields when indicator changes
    connect(ind_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), row, [field_combo, indicators](int idx) {
        field_combo->clear();
        if (idx >= 0 && idx < indicators.size())
            field_combo->addItems(indicators[idx].fields);
    });

    // Operator combo
    auto* op_combo = new QComboBox(row);
    op_combo->setStyleSheet(kComboStyle());
    op_combo->setFixedHeight(28);
    op_combo->setMinimumWidth(100);
    op_combo->addItems(algo_operators());

    // Value spin
    auto* val_spin = new QDoubleSpinBox(row);
    val_spin->setStyleSheet(kSpinStyle());
    val_spin->setFixedHeight(28);
    val_spin->setMinimumWidth(90);
    val_spin->setRange(-1e9, 1e9);
    val_spin->setDecimals(4);
    val_spin->setValue(0);

    // Remove button
    auto* rm_btn = new QPushButton("X", row);
    rm_btn->setFixedSize(28, 28);
    rm_btn->setCursor(Qt::PointingHandCursor);
    rm_btn->setStyleSheet(QString("QPushButton { background: transparent; color: %1; border: 1px solid %2;"
                                  " font-size: %3px; font-weight: 700; %4 }"
                                  "QPushButton:hover { color: %5; border-color: %5; }")
                              .arg(fincept::ui::colors::TEXT_TERTIARY, fincept::ui::colors::BORDER_DIM)
                              .arg(fincept::ui::fonts::TINY)
                              .arg(kMonoFont())
                              .arg(fincept::ui::colors::NEGATIVE));
    connect(rm_btn, &QPushButton::clicked, row, [row]() { row->deleteLater(); });

    hl->addWidget(ind_combo);
    hl->addWidget(field_combo);
    hl->addWidget(op_combo);
    hl->addWidget(val_spin);
    hl->addWidget(rm_btn);

    // Initialize fields for first indicator
    if (ind_combo->count() > 0)
        emit ind_combo->currentIndexChanged(0);

    // Store combos/spin as properties for later retrieval
    row->setProperty("ind_combo", QVariant::fromValue(static_cast<QObject*>(ind_combo)));
    row->setProperty("field_combo", QVariant::fromValue(static_cast<QObject*>(field_combo)));
    row->setProperty("op_combo", QVariant::fromValue(static_cast<QObject*>(op_combo)));
    row->setProperty("val_spin", QVariant::fromValue(static_cast<QObject*>(val_spin)));

    return row;
}

// ── Gather conditions from a layout ─────────────────────────────────────────

static QJsonArray gather_from_layout(QVBoxLayout* layout) {
    QJsonArray arr;
    for (int i = 0; i < layout->count(); ++i) {
        auto* item = layout->itemAt(i);
        auto* row = item ? item->widget() : nullptr;
        if (!row)
            continue;

        auto* ind_combo = qobject_cast<QComboBox*>(row->property("ind_combo").value<QObject*>());
        auto* field_combo = qobject_cast<QComboBox*>(row->property("field_combo").value<QObject*>());
        auto* op_combo = qobject_cast<QComboBox*>(row->property("op_combo").value<QObject*>());
        auto* val_spin = qobject_cast<QDoubleSpinBox*>(row->property("val_spin").value<QObject*>());
        if (!ind_combo || !field_combo || !op_combo || !val_spin)
            continue;

        QJsonObject cond;
        cond["indicator"] = ind_combo->currentData().toString();
        cond["field"] = field_combo->currentText();
        cond["operator"] = op_combo->currentText();
        cond["value"] = val_spin->value();
        cond["params"] = QJsonObject{}; // default params
        arr.append(cond);
    }
    return arr;
}

QJsonArray StrategyBuilderPanel::gather_conditions() {
    return gather_from_layout(entry_conditions_layout_);
}

// ── Build UI ────────────────────────────────────────────────────────────────

void StrategyBuilderPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Scroll area
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QString("QScrollArea { background: %1; border: none; }"
                                  "QScrollBar:vertical { background: %1; width: 6px; }"
                                  "QScrollBar::handle:vertical { background: %2; }"
                                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
                              .arg(fincept::ui::colors::BG_BASE, fincept::ui::colors::BORDER_MED));

    auto* content = new QWidget;
    content->setStyleSheet(QString("background: %1;").arg(fincept::ui::colors::BG_BASE));
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(16, 12, 16, 12);
    vl->setSpacing(12);

    // ── Strategy Identity ───────────────────────────────────────────────────
    auto* id_section = new QLabel("STRATEGY DEFINITION", content);
    id_section->setStyleSheet(kSectionLabel());
    vl->addWidget(id_section);

    auto* name_lbl = new QLabel("NAME", content);
    name_lbl->setStyleSheet(kLabelStyle());
    vl->addWidget(name_lbl);
    name_edit_ = new QLineEdit(content);
    name_edit_->setPlaceholderText("My Strategy");
    name_edit_->setStyleSheet(kInputStyle());
    name_edit_->setFixedHeight(30);
    vl->addWidget(name_edit_);

    auto* desc_lbl = new QLabel("DESCRIPTION", content);
    desc_lbl->setStyleSheet(kLabelStyle());
    vl->addWidget(desc_lbl);
    desc_edit_ = new QLineEdit(content);
    desc_edit_->setPlaceholderText("Strategy description...");
    desc_edit_->setStyleSheet(kInputStyle());
    desc_edit_->setFixedHeight(30);
    vl->addWidget(desc_edit_);

    auto* tf_lbl = new QLabel("TIMEFRAME", content);
    tf_lbl->setStyleSheet(kLabelStyle());
    vl->addWidget(tf_lbl);
    timeframe_combo_ = new QComboBox(content);
    timeframe_combo_->addItems(algo_timeframes());
    timeframe_combo_->setStyleSheet(kComboStyle());
    timeframe_combo_->setFixedHeight(30);
    vl->addWidget(timeframe_combo_);

    // ── Entry Conditions ────────────────────────────────────────────────────
    auto* entry_hdr = new QWidget(content);
    auto* entry_hdr_hl = new QHBoxLayout(entry_hdr);
    entry_hdr_hl->setContentsMargins(0, 8, 0, 0);
    entry_hdr_hl->setSpacing(8);

    auto* entry_lbl = new QLabel("ENTRY CONDITIONS", entry_hdr);
    entry_lbl->setStyleSheet(kSectionLabel());
    entry_hdr_hl->addWidget(entry_lbl);

    auto* entry_logic_lbl = new QLabel("Logic:", entry_hdr);
    entry_logic_lbl->setStyleSheet(kLabelStyle());
    entry_hdr_hl->addWidget(entry_logic_lbl);

    entry_logic_combo_ = new QComboBox(entry_hdr);
    entry_logic_combo_->addItems({"AND", "OR"});
    entry_logic_combo_->setStyleSheet(kComboStyle());
    entry_logic_combo_->setFixedHeight(26);
    entry_logic_combo_->setFixedWidth(80);
    entry_hdr_hl->addWidget(entry_logic_combo_);

    entry_hdr_hl->addStretch();
    vl->addWidget(entry_hdr);

    // Entry conditions container
    auto* entry_container = new QWidget(content);
    entry_conditions_layout_ = new QVBoxLayout(entry_container);
    entry_conditions_layout_->setContentsMargins(0, 0, 0, 0);
    entry_conditions_layout_->setSpacing(4);
    vl->addWidget(entry_container);

    auto* add_entry_btn = new QPushButton("+ ADD ENTRY CONDITION", content);
    add_entry_btn->setCursor(Qt::PointingHandCursor);
    add_entry_btn->setFixedHeight(28);
    add_entry_btn->setStyleSheet(QString("QPushButton { background: transparent; color: %1; border: 1px dashed %2;"
                                         " font-size: %3px; font-weight: 700; %4 }"
                                         "QPushButton:hover { color: %5; border-color: %5; }")
                                     .arg(fincept::ui::colors::TEXT_TERTIARY, fincept::ui::colors::BORDER_DIM)
                                     .arg(fincept::ui::fonts::TINY)
                                     .arg(kMonoFont())
                                     .arg(fincept::ui::colors::AMBER));
    connect(add_entry_btn, &QPushButton::clicked, this,
            [this, entry_container]() { entry_conditions_layout_->addWidget(build_condition_row(entry_container)); });
    vl->addWidget(add_entry_btn);

    // ── Exit Conditions ─────────────────────────────────────────────────────
    auto* exit_hdr = new QWidget(content);
    auto* exit_hdr_hl = new QHBoxLayout(exit_hdr);
    exit_hdr_hl->setContentsMargins(0, 8, 0, 0);
    exit_hdr_hl->setSpacing(8);

    auto* exit_lbl = new QLabel("EXIT CONDITIONS", exit_hdr);
    exit_lbl->setStyleSheet(kSectionLabel());
    exit_hdr_hl->addWidget(exit_lbl);

    auto* exit_logic_lbl = new QLabel("Logic:", exit_hdr);
    exit_logic_lbl->setStyleSheet(kLabelStyle());
    exit_hdr_hl->addWidget(exit_logic_lbl);

    exit_logic_combo_ = new QComboBox(exit_hdr);
    exit_logic_combo_->addItems({"AND", "OR"});
    exit_logic_combo_->setStyleSheet(kComboStyle());
    exit_logic_combo_->setFixedHeight(26);
    exit_logic_combo_->setFixedWidth(80);
    exit_hdr_hl->addWidget(exit_logic_combo_);

    exit_hdr_hl->addStretch();
    vl->addWidget(exit_hdr);

    // Exit conditions container
    auto* exit_container = new QWidget(content);
    exit_conditions_layout_ = new QVBoxLayout(exit_container);
    exit_conditions_layout_->setContentsMargins(0, 0, 0, 0);
    exit_conditions_layout_->setSpacing(4);
    vl->addWidget(exit_container);

    auto* add_exit_btn = new QPushButton("+ ADD EXIT CONDITION", content);
    add_exit_btn->setCursor(Qt::PointingHandCursor);
    add_exit_btn->setFixedHeight(28);
    add_exit_btn->setStyleSheet(QString("QPushButton { background: transparent; color: %1; border: 1px dashed %2;"
                                        " font-size: %3px; font-weight: 700; %4 }"
                                        "QPushButton:hover { color: %5; border-color: %5; }")
                                    .arg(fincept::ui::colors::TEXT_TERTIARY, fincept::ui::colors::BORDER_DIM)
                                    .arg(fincept::ui::fonts::TINY)
                                    .arg(kMonoFont())
                                    .arg(fincept::ui::colors::AMBER));
    connect(add_exit_btn, &QPushButton::clicked, this,
            [this, exit_container]() { exit_conditions_layout_->addWidget(build_condition_row(exit_container)); });
    vl->addWidget(add_exit_btn);

    // ── Risk Parameters ─────────────────────────────────────────────────────
    auto* risk_lbl = new QLabel("RISK MANAGEMENT", content);
    risk_lbl->setStyleSheet(kSectionLabel());
    vl->addWidget(risk_lbl);

    auto* risk_grid = new QWidget(content);
    auto* rgl = new QHBoxLayout(risk_grid);
    rgl->setContentsMargins(0, 0, 0, 0);
    rgl->setSpacing(12);

    auto make_risk_field = [&](const QString& label, double def_val) -> QDoubleSpinBox* {
        auto* col = new QWidget(risk_grid);
        auto* cvl = new QVBoxLayout(col);
        cvl->setContentsMargins(0, 0, 0, 0);
        cvl->setSpacing(2);
        auto* lbl = new QLabel(label, col);
        lbl->setStyleSheet(kLabelStyle());
        cvl->addWidget(lbl);
        auto* spin = new QDoubleSpinBox(col);
        spin->setStyleSheet(kSpinStyle());
        spin->setFixedHeight(30);
        spin->setRange(0.0, 100.0);
        spin->setDecimals(2);
        spin->setSuffix(" %");
        spin->setValue(def_val);
        spin->setSpecialValueText("DISABLED");
        cvl->addWidget(spin);
        rgl->addWidget(col);
        return spin;
    };

    stop_loss_spin_ = make_risk_field("STOP LOSS %", 0.0);
    take_profit_spin_ = make_risk_field("TAKE PROFIT %", 0.0);
    trailing_stop_spin_ = make_risk_field("TRAILING STOP %", 0.0);

    vl->addWidget(risk_grid);

    // ── Backtest Section ────────────────────────────────────────────────────
    auto* bt_lbl = new QLabel("BACKTEST", content);
    bt_lbl->setStyleSheet(kSectionLabel());
    vl->addWidget(bt_lbl);

    auto* bt_row = new QWidget(content);
    auto* bt_hl = new QHBoxLayout(bt_row);
    bt_hl->setContentsMargins(0, 0, 0, 0);
    bt_hl->setSpacing(8);

    // Symbol
    auto* bt_sym_col = new QWidget(bt_row);
    auto* bt_sym_vl = new QVBoxLayout(bt_sym_col);
    bt_sym_vl->setContentsMargins(0, 0, 0, 0);
    bt_sym_vl->setSpacing(2);
    auto* bt_sym_lbl = new QLabel("SYMBOL", bt_sym_col);
    bt_sym_lbl->setStyleSheet(kLabelStyle());
    bt_sym_vl->addWidget(bt_sym_lbl);
    bt_symbol_ = new QLineEdit(bt_sym_col);
    bt_symbol_->setPlaceholderText("RELIANCE");
    bt_symbol_->setStyleSheet(kInputStyle());
    bt_symbol_->setFixedHeight(30);
    bt_sym_vl->addWidget(bt_symbol_);
    bt_hl->addWidget(bt_sym_col);

    // Capital
    auto* bt_cap_col = new QWidget(bt_row);
    auto* bt_cap_vl = new QVBoxLayout(bt_cap_col);
    bt_cap_vl->setContentsMargins(0, 0, 0, 0);
    bt_cap_vl->setSpacing(2);
    auto* bt_cap_lbl = new QLabel("CAPITAL ($)", bt_cap_col);
    bt_cap_lbl->setStyleSheet(kLabelStyle());
    bt_cap_vl->addWidget(bt_cap_lbl);
    bt_capital_ = new QDoubleSpinBox(bt_cap_col);
    bt_capital_->setStyleSheet(kSpinStyle());
    bt_capital_->setFixedHeight(30);
    bt_capital_->setRange(100, 1e9);
    bt_capital_->setDecimals(0);
    bt_capital_->setPrefix("$ ");
    bt_capital_->setValue(100000);
    bt_cap_vl->addWidget(bt_capital_);
    bt_hl->addWidget(bt_cap_col);

    // Backtest button
    auto* bt_btn = new QPushButton("BACKTEST", bt_row);
    bt_btn->setCursor(Qt::PointingHandCursor);
    bt_btn->setFixedHeight(30);
    bt_btn->setMinimumWidth(100);
    bt_btn->setStyleSheet(QString("QPushButton { background: rgba(8,145,178,0.1); color: %1; border: 1px solid %1;"
                                  " font-size: %2px; font-weight: 700; %3 padding: 4px 16px; }"
                                  "QPushButton:hover { background: %1; color: %4; }")
                              .arg(fincept::ui::colors::CYAN)
                              .arg(fincept::ui::fonts::TINY)
                              .arg(kMonoFont())
                              .arg(fincept::ui::colors::BG_BASE));
    connect(bt_btn, &QPushButton::clicked, this, &StrategyBuilderPanel::on_backtest);
    bt_hl->addWidget(bt_btn, 0, Qt::AlignBottom);

    vl->addWidget(bt_row);

    // ── Save Button ─────────────────────────────────────────────────────────
    auto* save_btn = new QPushButton("SAVE STRATEGY", content);
    save_btn->setCursor(Qt::PointingHandCursor);
    save_btn->setFixedHeight(36);
    save_btn->setStyleSheet(QString("QPushButton { background: rgba(217,119,6,0.1); color: %1; border: 1px solid %1;"
                                    " font-size: %2px; font-weight: 700; %3 padding: 6px 24px; }"
                                    "QPushButton:hover { background: %1; color: %4; }")
                                .arg(fincept::ui::colors::AMBER)
                                .arg(fincept::ui::fonts::DATA)
                                .arg(kMonoFont())
                                .arg(fincept::ui::colors::BG_BASE));
    connect(save_btn, &QPushButton::clicked, this, &StrategyBuilderPanel::on_save);
    vl->addWidget(save_btn);

    // ── Status label ────────────────────────────────────────────────────────
    status_label_ = new QLabel("", content);
    status_label_->setWordWrap(true);
    status_label_->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                                     .arg(fincept::ui::colors::TEXT_TERTIARY)
                                     .arg(fincept::ui::fonts::SMALL)
                                     .arg(kMonoFont()));
    vl->addWidget(status_label_);

    // ── Results area ────────────────────────────────────────────────────────
    auto* results_section = new QLabel("BACKTEST RESULTS", content);
    results_section->setStyleSheet(kSectionLabel());
    vl->addWidget(results_section);

    auto* results_container = new QWidget(content);
    results_layout_ = new QVBoxLayout(results_container);
    results_layout_->setContentsMargins(0, 0, 0, 0);
    results_layout_->setSpacing(4);
    vl->addWidget(results_container);

    vl->addStretch();

    scroll->setWidget(content);
    root->addWidget(scroll);
}

// ── Slots ───────────────────────────────────────────────────────────────────

void StrategyBuilderPanel::on_save() {
    if (name_edit_->text().trimmed().isEmpty()) {
        status_label_->setText("Strategy name is required.");
        status_label_->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                                         .arg(fincept::ui::colors::NEGATIVE)
                                         .arg(fincept::ui::fonts::SMALL)
                                         .arg(kMonoFont()));
        return;
    }

    AlgoStrategy strategy;
    strategy.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    strategy.name = name_edit_->text().trimmed();
    strategy.description = desc_edit_->text().trimmed();
    strategy.timeframe = timeframe_combo_->currentText();
    strategy.entry_conditions = gather_from_layout(entry_conditions_layout_);
    strategy.exit_conditions = gather_from_layout(exit_conditions_layout_);
    strategy.entry_logic = entry_logic_combo_->currentText();
    strategy.exit_logic = exit_logic_combo_->currentText();
    strategy.stop_loss = stop_loss_spin_->value();
    strategy.take_profit = take_profit_spin_->value();
    strategy.trailing_stop = trailing_stop_spin_->value();

    status_label_->setText("Saving...");
    status_label_->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                                     .arg(fincept::ui::colors::TEXT_SECONDARY)
                                     .arg(fincept::ui::fonts::SMALL)
                                     .arg(kMonoFont()));

    AlgoTradingService::instance().save_strategy(strategy);
    LOG_INFO("AlgoTrading", QString("Saving strategy: %1").arg(strategy.name));
}

void StrategyBuilderPanel::on_backtest() {
    QString symbol = bt_symbol_->text().trimmed();
    if (symbol.isEmpty()) {
        status_label_->setText("Enter a symbol for backtesting.");
        status_label_->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                                         .arg(fincept::ui::colors::NEGATIVE)
                                         .arg(fincept::ui::fonts::SMALL)
                                         .arg(kMonoFont()));
        return;
    }

    status_label_->setText("Running backtest...");
    status_label_->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                                     .arg(fincept::ui::colors::CYAN)
                                     .arg(fincept::ui::fonts::SMALL)
                                     .arg(kMonoFont()));

    // Use strategy id if saved, otherwise use name as identifier
    QString strat_id = name_edit_->text().trimmed();
    double capital = bt_capital_->value();

    AlgoTradingService::instance().run_backtest(strat_id, symbol, "2024-01-01", "2025-01-01", capital);
    LOG_INFO("AlgoTrading", QString("Backtest requested: %1 on %2").arg(strat_id, symbol));
}

void StrategyBuilderPanel::on_backtest_result(const QJsonObject& data) {
    // Clear previous results
    while (results_layout_->count() > 0) {
        auto* item = results_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    status_label_->setText("Backtest complete.");
    status_label_->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                                     .arg(fincept::ui::colors::POSITIVE)
                                     .arg(fincept::ui::fonts::SMALL)
                                     .arg(kMonoFont()));

    // Build result cards
    auto add_metric = [this](const QString& label, const QString& value, const QString& color) {
        auto* row = new QWidget;
        row->setStyleSheet(QString("background: %1; border: 1px solid %2;")
                               .arg(fincept::ui::colors::BG_SURFACE, fincept::ui::colors::BORDER_DIM));
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(8, 4, 8, 4);
        hl->setSpacing(0);
        auto* lbl = new QLabel(label, row);
        lbl->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                               .arg(fincept::ui::colors::TEXT_SECONDARY)
                               .arg(fincept::ui::fonts::SMALL)
                               .arg(kMonoFont()));
        auto* val = new QLabel(value, row);
        val->setStyleSheet(
            QString("color: %1; font-size: %2px; font-weight: 700; %3 background: transparent; border: none;")
                .arg(color)
                .arg(fincept::ui::fonts::DATA)
                .arg(kMonoFont()));
        val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        hl->addWidget(lbl);
        hl->addStretch();
        hl->addWidget(val);
        results_layout_->addWidget(row);
    };

    double total_return = data.value("total_return").toDouble();
    double sharpe = data.value("sharpe_ratio").toDouble();
    double max_dd = data.value("max_drawdown").toDouble();
    int total_trades = data.value("total_trades").toInt();
    double win_rate = data.value("win_rate").toDouble();
    double profit_factor = data.value("profit_factor").toDouble();

    add_metric("TOTAL RETURN", QString("%1%2%").arg(total_return >= 0 ? "+" : "").arg(total_return, 0, 'f', 2),
               total_return >= 0 ? fincept::ui::colors::POSITIVE : fincept::ui::colors::NEGATIVE);
    add_metric("SHARPE RATIO", QString::number(sharpe, 'f', 2), fincept::ui::colors::TEXT_PRIMARY);
    add_metric("MAX DRAWDOWN", QString("-%1%").arg(qAbs(max_dd), 0, 'f', 2), fincept::ui::colors::NEGATIVE);
    add_metric("TOTAL TRADES", QString::number(total_trades), fincept::ui::colors::TEXT_PRIMARY);
    add_metric("WIN RATE", QString("%1%").arg(win_rate, 0, 'f', 1), fincept::ui::colors::TEXT_PRIMARY);
    add_metric("PROFIT FACTOR", QString::number(profit_factor, 'f', 2),
               profit_factor >= 1.0 ? fincept::ui::colors::POSITIVE : fincept::ui::colors::NEGATIVE);

    LOG_INFO("AlgoTrading", QString("Backtest results displayed: return=%1%").arg(total_return, 0, 'f', 2));
}

void StrategyBuilderPanel::on_error(const QString& context, const QString& msg) {
    if (status_label_) {
        status_label_->setText(QString("Error [%1]: %2").arg(context, msg));
        status_label_->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                                         .arg(fincept::ui::colors::NEGATIVE)
                                         .arg(fincept::ui::fonts::SMALL)
                                         .arg(kMonoFont()));
    }
}

} // namespace fincept::screens
