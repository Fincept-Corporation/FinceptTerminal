// src/screens/algo_trading/StrategyBuilderPanel.cpp
#include "screens/algo_trading/StrategyBuilderPanel.h"

#include "core/logging/Logger.h"
#include "services/algo_trading/AlgoTradingService.h"
#include "services/file_manager/FileManagerService.h"
#include "ui/theme/Theme.h"

#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSplitter>
#include <QUuid>

// ── Shared style helpers ────────────────────────────────────────────────────

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
        .arg(fincept::ui::colors::BG_SURFACE, fincept::ui::colors::BORDER_DIM,
             fincept::ui::colors::TEXT_PRIMARY)
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
        .arg(fincept::ui::colors::BG_SURFACE, fincept::ui::colors::TEXT_PRIMARY,
             fincept::ui::colors::BORDER_DIM)
        .arg(fincept::ui::fonts::SMALL)
        .arg(kMonoFont())
        .arg(fincept::ui::colors::BG_HOVER);
}
inline QString kSpinStyle() {
    return QString("QDoubleSpinBox { background: %1; color: %2; border: 1px solid %3; padding: 4px 8px;"
                   " font-size: %4px; %5 }"
                   "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { width: 14px; }")
        .arg(fincept::ui::colors::BG_SURFACE, fincept::ui::colors::TEXT_PRIMARY,
             fincept::ui::colors::BORDER_DIM)
        .arg(fincept::ui::fonts::SMALL)
        .arg(kMonoFont());
}

} // namespace

namespace fincept::screens {

using namespace fincept::services::algo;

// ── gather_from_layout ──────────────────────────────────────────────────────

static QJsonArray gather_from_layout(QVBoxLayout* layout) {
    QJsonArray arr;
    for (int i = 0; i < layout->count(); ++i) {
        auto* item = layout->itemAt(i);
        auto* row  = item ? item->widget() : nullptr;
        if (!row) continue;
        auto* ind_combo   = qobject_cast<QComboBox*>(row->property("ind_combo").value<QObject*>());
        auto* field_combo = qobject_cast<QComboBox*>(row->property("field_combo").value<QObject*>());
        auto* op_combo    = qobject_cast<QComboBox*>(row->property("op_combo").value<QObject*>());
        auto* val_spin    = qobject_cast<QDoubleSpinBox*>(row->property("val_spin").value<QObject*>());
        if (!ind_combo || !field_combo || !op_combo || !val_spin) continue;
        QJsonObject cond;
        cond["indicator"] = ind_combo->currentData().toString();
        cond["field"]     = field_combo->currentText();
        cond["operator"]  = op_combo->currentText();
        cond["value"]     = val_spin->value();
        cond["params"]    = QJsonObject{};
        arr.append(cond);
    }
    return arr;
}

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
        status_label_->setStyleSheet(
            QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                .arg(fincept::ui::colors::POSITIVE)
                .arg(fincept::ui::fonts::SMALL)
                .arg(kMonoFont()));
    });
    connect(&svc, &AlgoTradingService::backtest_result, this,
            &StrategyBuilderPanel::on_backtest_result);
    connect(&svc, &AlgoTradingService::error_occurred, this,
            &StrategyBuilderPanel::on_error);
}

// ── Condition row ───────────────────────────────────────────────────────────

QWidget* StrategyBuilderPanel::build_condition_row(QWidget* parent) {
    auto* row = new QWidget(parent);
    row->setStyleSheet(QString("background: %1; border: 1px solid %2;")
                           .arg(fincept::ui::colors::BG_SURFACE, fincept::ui::colors::BORDER_DIM));
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(6, 4, 6, 4);
    hl->setSpacing(4);

    auto* ind_combo = new QComboBox(row);
    ind_combo->setStyleSheet(kComboStyle());
    ind_combo->setFixedHeight(28);
    ind_combo->setMinimumWidth(120);
    const auto indicators = algo_indicators();
    for (const auto& ind : indicators)
        ind_combo->addItem(ind.label, ind.id);

    auto* field_combo = new QComboBox(row);
    field_combo->setStyleSheet(kComboStyle());
    field_combo->setFixedHeight(28);
    field_combo->setMinimumWidth(90);

    connect(ind_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), row,
            [field_combo, indicators](int idx) {
                field_combo->clear();
                if (idx >= 0 && idx < indicators.size())
                    field_combo->addItems(indicators[idx].fields);
            });

    auto* op_combo = new QComboBox(row);
    op_combo->setStyleSheet(kComboStyle());
    op_combo->setFixedHeight(28);
    op_combo->setMinimumWidth(100);
    op_combo->addItems(algo_operators());

    auto* val_spin = new QDoubleSpinBox(row);
    val_spin->setStyleSheet(kSpinStyle());
    val_spin->setFixedHeight(28);
    val_spin->setMinimumWidth(90);
    val_spin->setRange(-1e9, 1e9);
    val_spin->setDecimals(4);
    val_spin->setValue(0);

    auto* rm_btn = new QPushButton("X", row);
    rm_btn->setFixedSize(28, 28);
    rm_btn->setCursor(Qt::PointingHandCursor);
    rm_btn->setStyleSheet(
        QString("QPushButton { background: transparent; color: %1; border: 1px solid %2;"
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

    if (ind_combo->count() > 0)
        emit ind_combo->currentIndexChanged(0);

    row->setProperty("ind_combo",   QVariant::fromValue(static_cast<QObject*>(ind_combo)));
    row->setProperty("field_combo", QVariant::fromValue(static_cast<QObject*>(field_combo)));
    row->setProperty("op_combo",    QVariant::fromValue(static_cast<QObject*>(op_combo)));
    row->setProperty("val_spin",    QVariant::fromValue(static_cast<QObject*>(val_spin)));

    return row;
}

// ── Left pane: strategy editor ──────────────────────────────────────────────

QWidget* StrategyBuilderPanel::build_left_pane() {
    auto* pane = new QWidget(this);
    pane->setStyleSheet(
        QString("background: %1; border-right: 1px solid %2;")
            .arg(fincept::ui::colors::BG_BASE, fincept::ui::colors::BORDER_DIM));

    auto* root_layout = new QVBoxLayout(pane);
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(0);

    auto* scroll = new QScrollArea(pane);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(
        QString("QScrollArea { background: %1; border: none; }"
                "QScrollBar:vertical { background: %1; width: 6px; }"
                "QScrollBar::handle:vertical { background: %2; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
            .arg(fincept::ui::colors::BG_BASE, fincept::ui::colors::BORDER_MED));

    auto* content = new QWidget;
    content->setStyleSheet(QString("background: %1;").arg(fincept::ui::colors::BG_BASE));
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(16, 12, 16, 12);
    vl->setSpacing(8);

    // ── Strategy Definition ─────────────────────────────────────────────────
    auto* id_sec = new QLabel("STRATEGY DEFINITION", content);
    id_sec->setStyleSheet(kSectionLabel());
    vl->addWidget(id_sec);

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

    auto* entry_container = new QWidget(content);
    entry_conditions_layout_ = new QVBoxLayout(entry_container);
    entry_conditions_layout_->setContentsMargins(0, 0, 0, 0);
    entry_conditions_layout_->setSpacing(4);
    vl->addWidget(entry_container);

    auto* add_entry_btn = new QPushButton("+ ADD ENTRY CONDITION", content);
    add_entry_btn->setCursor(Qt::PointingHandCursor);
    add_entry_btn->setFixedHeight(28);
    add_entry_btn->setStyleSheet(
        QString("QPushButton { background: transparent; color: %1; border: 1px dashed %2;"
                " font-size: %3px; font-weight: 700; %4 }"
                "QPushButton:hover { color: %5; border-color: %5; }")
            .arg(fincept::ui::colors::TEXT_TERTIARY, fincept::ui::colors::BORDER_DIM)
            .arg(fincept::ui::fonts::TINY)
            .arg(kMonoFont())
            .arg(fincept::ui::colors::AMBER));
    connect(add_entry_btn, &QPushButton::clicked, this, [this, entry_container]() {
        entry_conditions_layout_->addWidget(build_condition_row(entry_container));
    });
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

    auto* exit_container = new QWidget(content);
    exit_conditions_layout_ = new QVBoxLayout(exit_container);
    exit_conditions_layout_->setContentsMargins(0, 0, 0, 0);
    exit_conditions_layout_->setSpacing(4);
    vl->addWidget(exit_container);

    auto* add_exit_btn = new QPushButton("+ ADD EXIT CONDITION", content);
    add_exit_btn->setCursor(Qt::PointingHandCursor);
    add_exit_btn->setFixedHeight(28);
    add_exit_btn->setStyleSheet(add_entry_btn->styleSheet());
    connect(add_exit_btn, &QPushButton::clicked, this, [this, exit_container]() {
        exit_conditions_layout_->addWidget(build_condition_row(exit_container));
    });
    vl->addWidget(add_exit_btn);

    // ── Risk Management ─────────────────────────────────────────────────────
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

    stop_loss_spin_     = make_risk_field("STOP LOSS %",     0.0);
    take_profit_spin_   = make_risk_field("TAKE PROFIT %",   0.0);
    trailing_stop_spin_ = make_risk_field("TRAILING STOP %", 0.0);
    vl->addWidget(risk_grid);

    vl->addStretch();

    // ── Save button ─────────────────────────────────────────────────────────
    auto* save_btn = new QPushButton("SAVE STRATEGY", content);
    save_btn->setCursor(Qt::PointingHandCursor);
    save_btn->setFixedHeight(36);
    save_btn->setStyleSheet(
        QString("QPushButton { background: rgba(217,119,6,0.1); color: %1; border: 1px solid %1;"
                " font-size: %2px; font-weight: 700; %3 padding: 6px 24px; }"
                "QPushButton:hover { background: %1; color: %4; }")
            .arg(fincept::ui::colors::AMBER)
            .arg(fincept::ui::fonts::DATA)
            .arg(kMonoFont())
            .arg(fincept::ui::colors::BG_BASE));
    connect(save_btn, &QPushButton::clicked, this, &StrategyBuilderPanel::on_save);
    vl->addWidget(save_btn);

    scroll->setWidget(content);
    root_layout->addWidget(scroll);
    return pane;
}

// ── Right pane: backtest workbench ──────────────────────────────────────────

QWidget* StrategyBuilderPanel::build_right_pane() {
    auto* pane = new QWidget(this);
    pane->setStyleSheet(QString("background: %1;").arg(fincept::ui::colors::BG_BASE));

    auto* root_layout = new QVBoxLayout(pane);
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(0);

    auto* scroll = new QScrollArea(pane);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(
        QString("QScrollArea { background: %1; border: none; }"
                "QScrollBar:vertical { background: %1; width: 6px; }"
                "QScrollBar::handle:vertical { background: %2; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
            .arg(fincept::ui::colors::BG_BASE, fincept::ui::colors::BORDER_MED));

    auto* content = new QWidget;
    content->setStyleSheet(QString("background: %1;").arg(fincept::ui::colors::BG_BASE));
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(16, 12, 16, 12);
    vl->setSpacing(8);

    // ── Backtest Parameters ─────────────────────────────────────────────────
    auto* bt_sec = new QLabel("BACKTEST PARAMETERS", content);
    bt_sec->setStyleSheet(kSectionLabel());
    vl->addWidget(bt_sec);

    // 2×2 grid of params
    auto* params_grid = new QWidget(content);
    auto* params_gl   = new QGridLayout(params_grid);
    params_gl->setContentsMargins(0, 0, 0, 0);
    params_gl->setSpacing(8);

    auto make_param_col = [&](const QString& label_text, QWidget* input) -> QWidget* {
        auto* col = new QWidget(params_grid);
        auto* cvl = new QVBoxLayout(col);
        cvl->setContentsMargins(0, 0, 0, 0);
        cvl->setSpacing(2);
        auto* lbl = new QLabel(label_text, col);
        lbl->setStyleSheet(kLabelStyle());
        cvl->addWidget(lbl);
        input->setParent(col);
        cvl->addWidget(input);
        return col;
    };

    bt_symbol_ = new QLineEdit;
    bt_symbol_->setPlaceholderText("RELIANCE");
    bt_symbol_->setStyleSheet(kInputStyle());
    bt_symbol_->setFixedHeight(30);
    params_gl->addWidget(make_param_col("SYMBOL", bt_symbol_), 0, 0);

    bt_capital_ = new QDoubleSpinBox;
    bt_capital_->setStyleSheet(kSpinStyle());
    bt_capital_->setFixedHeight(30);
    bt_capital_->setRange(100, 1e9);
    bt_capital_->setDecimals(0);
    bt_capital_->setPrefix("$ ");
    bt_capital_->setValue(100000);
    params_gl->addWidget(make_param_col("CAPITAL ($)", bt_capital_), 0, 1);

    bt_start_date_ = new QLineEdit;
    bt_start_date_->setPlaceholderText("YYYY-MM-DD");
    bt_start_date_->setText("2024-01-01");
    bt_start_date_->setStyleSheet(kInputStyle());
    bt_start_date_->setFixedHeight(30);
    params_gl->addWidget(make_param_col("START DATE", bt_start_date_), 1, 0);

    bt_end_date_ = new QLineEdit;
    bt_end_date_->setPlaceholderText("YYYY-MM-DD");
    bt_end_date_->setText("2025-01-01");
    bt_end_date_->setStyleSheet(kInputStyle());
    bt_end_date_->setFixedHeight(30);
    params_gl->addWidget(make_param_col("END DATE", bt_end_date_), 1, 1);

    vl->addWidget(params_grid);

    // RUN BACKTEST button
    auto* bt_btn = new QPushButton("RUN BACKTEST", content);
    bt_btn->setCursor(Qt::PointingHandCursor);
    bt_btn->setFixedHeight(36);
    bt_btn->setStyleSheet(
        QString("QPushButton { background: rgba(8,145,178,0.1); color: %1; border: 1px solid %1;"
                " font-size: %2px; font-weight: 700; %3 padding: 6px 24px; }"
                "QPushButton:hover { background: %1; color: %4; }")
            .arg(fincept::ui::colors::CYAN)
            .arg(fincept::ui::fonts::DATA)
            .arg(kMonoFont())
            .arg(fincept::ui::colors::BG_BASE));
    connect(bt_btn, &QPushButton::clicked, this, &StrategyBuilderPanel::on_backtest);
    vl->addWidget(bt_btn);

    // Status label
    status_label_ = new QLabel("", content);
    status_label_->setWordWrap(true);
    status_label_->setStyleSheet(
        QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
            .arg(fincept::ui::colors::TEXT_TERTIARY)
            .arg(fincept::ui::fonts::SMALL)
            .arg(kMonoFont()));
    vl->addWidget(status_label_);

    // ── Results area ────────────────────────────────────────────────────────
    auto* results_container = new QWidget(content);
    results_layout_ = new QVBoxLayout(results_container);
    results_layout_->setContentsMargins(0, 4, 0, 0);
    results_layout_->setSpacing(8);

    // Empty state
    bt_empty_label_ = new QLabel("Run a backtest to see results", results_container);
    bt_empty_label_->setAlignment(Qt::AlignCenter);
    bt_empty_label_->setStyleSheet(
        QString("color: %1; font-size: %2px; %3 background: transparent; border: none; padding: 24px;")
            .arg(fincept::ui::colors::TEXT_TERTIARY)
            .arg(fincept::ui::fonts::SMALL)
            .arg(kMonoFont()));
    results_layout_->addWidget(bt_empty_label_);

    // KPI grid — hidden until first result
    kpi_grid_widget_ = new QWidget(results_container);
    kpi_grid_widget_->setVisible(false);

    auto* kpi_gl = new QGridLayout(kpi_grid_widget_);
    kpi_gl->setContentsMargins(0, 0, 0, 0);
    kpi_gl->setSpacing(8);

    // Helper to build one KPI card and wire up its labels
    struct KpiDef {
        QString  label;
        QLabel** val_out;
        QLabel** sub_out;
    };

    QList<KpiDef> kpi_defs = {
        {"TOTAL RETURN",  &kpi_total_return_val_,  &kpi_total_return_sub_},
        {"SHARPE RATIO",  &kpi_sharpe_val_,        &kpi_sharpe_sub_},
        {"MAX DRAWDOWN",  &kpi_max_dd_val_,        &kpi_max_dd_sub_},
        {"WIN RATE",      &kpi_win_rate_val_,      &kpi_win_rate_sub_},
        {"TOTAL TRADES",  &kpi_trades_val_,        &kpi_trades_sub_},
        {"PROFIT FACTOR", &kpi_profit_factor_val_, &kpi_profit_factor_sub_},
    };

    for (int i = 0; i < kpi_defs.size(); ++i) {
        const auto& def = kpi_defs[i];

        auto* card = new QWidget(kpi_grid_widget_);
        card->setObjectName("btKpiCard");
        card->setStyleSheet(
            QString("#btKpiCard { background:%1; border:1px solid %2; border-radius:4px; }")
                .arg(fincept::ui::colors::BG_SURFACE, fincept::ui::colors::BORDER_DIM));

        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(12, 10, 12, 10);
        cl->setSpacing(2);

        auto* lbl = new QLabel(def.label, card);
        lbl->setStyleSheet(
            QString("color:%1; font-size:%2px; font-family:%3; font-weight:600; letter-spacing:1px;"
                    " background:transparent; border:none;")
                .arg(fincept::ui::colors::TEXT_TERTIARY)
                .arg(fincept::ui::fonts::TINY)
                .arg(fincept::ui::fonts::DATA_FAMILY));
        cl->addWidget(lbl);

        auto* val = new QLabel("—", card);
        val->setStyleSheet(
            QString("color:%1; font-size:%2px; font-family:%3; font-weight:800;"
                    " background:transparent; border:none;")
                .arg(fincept::ui::colors::TEXT_PRIMARY)
                .arg(fincept::ui::fonts::HEADER + 2)
                .arg(fincept::ui::fonts::DATA_FAMILY));
        cl->addWidget(val);
        *def.val_out = val;

        auto* sub = new QLabel("", card);
        sub->setStyleSheet(
            QString("color:%1; font-size:%2px; font-family:%3;"
                    " background:transparent; border:none;")
                .arg(fincept::ui::colors::TEXT_TERTIARY)
                .arg(fincept::ui::fonts::TINY)
                .arg(fincept::ui::fonts::DATA_FAMILY));
        cl->addWidget(sub);
        *def.sub_out = sub;

        kpi_gl->addWidget(card, i / 3, i % 3);
    }

    results_layout_->addWidget(kpi_grid_widget_);
    vl->addWidget(results_container);
    vl->addStretch();

    scroll->setWidget(content);
    root_layout->addWidget(scroll);
    return pane;
}

// ── build_ui: QSplitter shell ────────────────────────────────────────────────

void StrategyBuilderPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(4);
    splitter->setStyleSheet(
        QString("QSplitter::handle { background: %1; }")
            .arg(fincept::ui::colors::BORDER_DIM));
    splitter->addWidget(build_left_pane());
    splitter->addWidget(build_right_pane());
    splitter->setStretchFactor(0, 2); // left ~40%
    splitter->setStretchFactor(1, 3); // right ~60%

    root->addWidget(splitter, 1);
}

// ── clear_results ────────────────────────────────────────────────────────────

void StrategyBuilderPanel::clear_results() {
    bt_empty_label_->setVisible(true);
    kpi_grid_widget_->setVisible(false);
}

// ── display_backtest_result — Quant Lab KPI card style ───────────────────────

void StrategyBuilderPanel::display_backtest_result(const QJsonObject& data) {
    bt_empty_label_->setVisible(false);
    kpi_grid_widget_->setVisible(true);

    double total_return  = data.value("total_return").toDouble();
    double sharpe        = data.value("sharpe_ratio").toDouble();
    double max_dd        = data.value("max_drawdown").toDouble();
    int    total_trades  = data.value("total_trades").toInt();
    double win_rate      = data.value("win_rate").toDouble();
    double profit_factor = data.value("profit_factor").toDouble();
    double final_val     = data.value("final_value").toDouble();

    auto set_kpi = [](QLabel* lbl, const QString& text, const QString& color) {
        lbl->setText(text);
        lbl->setStyleSheet(
            QString("color:%1; font-size:%2px; font-family:%3; font-weight:800;"
                    " background:transparent; border:none;")
                .arg(color)
                .arg(fincept::ui::fonts::HEADER + 2)
                .arg(fincept::ui::fonts::DATA_FAMILY));
    };

    // TOTAL RETURN
    set_kpi(kpi_total_return_val_,
            QString("%1%2%").arg(total_return >= 0 ? "+" : "").arg(total_return, 0, 'f', 2),
            total_return >= 0 ? fincept::ui::colors::POSITIVE : fincept::ui::colors::NEGATIVE);
    kpi_total_return_sub_->setText(
        QString("Final: $%1").arg(final_val, 0, 'f', 0));

    // SHARPE
    set_kpi(kpi_sharpe_val_, QString::number(sharpe, 'f', 3),
            sharpe >= 0.5 ? fincept::ui::colors::POSITIVE : fincept::ui::colors::NEGATIVE);
    kpi_sharpe_sub_->setText(sharpe >= 1.0 ? "Excellent" : sharpe >= 0.5 ? "Good" : "Weak");

    // MAX DRAWDOWN (always red)
    set_kpi(kpi_max_dd_val_,
            QString("-%1%").arg(qAbs(max_dd), 0, 'f', 2),
            fincept::ui::colors::NEGATIVE);
    kpi_max_dd_sub_->setText("Max Drawdown");

    // WIN RATE
    set_kpi(kpi_win_rate_val_,
            QString("%1%").arg(win_rate, 0, 'f', 1),
            win_rate >= 50.0 ? fincept::ui::colors::POSITIVE : fincept::ui::colors::NEGATIVE);
    kpi_win_rate_sub_->setText(win_rate >= 50.0 ? "Above average" : "Below average");

    // TOTAL TRADES
    set_kpi(kpi_trades_val_, QString::number(total_trades), fincept::ui::colors::TEXT_PRIMARY);
    kpi_trades_sub_->setText("Total trades");

    // PROFIT FACTOR
    set_kpi(kpi_profit_factor_val_, QString::number(profit_factor, 'f', 2),
            profit_factor >= 1.0 ? fincept::ui::colors::POSITIVE : fincept::ui::colors::NEGATIVE);
    kpi_profit_factor_sub_->setText(profit_factor >= 1.5 ? "Strong" : profit_factor >= 1.0 ? "Profitable" : "Losing");
}

// ── on_backtest_result ───────────────────────────────────────────────────────

void StrategyBuilderPanel::on_backtest_result(const QJsonObject& data) {
    status_label_->setText("Backtest complete.");
    status_label_->setStyleSheet(
        QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
            .arg(fincept::ui::colors::POSITIVE)
            .arg(fincept::ui::fonts::SMALL)
            .arg(kMonoFont()));
    display_backtest_result(data);
    LOG_INFO("AlgoTrading", "Backtest result displayed");
}

// ── on_save ──────────────────────────────────────────────────────────────────

void StrategyBuilderPanel::on_save() {
    if (name_edit_->text().trimmed().isEmpty()) {
        status_label_->setText("Strategy name is required.");
        status_label_->setStyleSheet(
            QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                .arg(fincept::ui::colors::NEGATIVE)
                .arg(fincept::ui::fonts::SMALL)
                .arg(kMonoFont()));
        return;
    }

    AlgoStrategy strategy;
    strategy.id               = QUuid::createUuid().toString(QUuid::WithoutBraces);
    strategy.name             = name_edit_->text().trimmed();
    strategy.description      = desc_edit_->text().trimmed();
    strategy.timeframe        = timeframe_combo_->currentText();
    strategy.entry_conditions = gather_from_layout(entry_conditions_layout_);
    strategy.exit_conditions  = gather_from_layout(exit_conditions_layout_);
    strategy.entry_logic      = entry_logic_combo_->currentText();
    strategy.exit_logic       = exit_logic_combo_->currentText();
    strategy.stop_loss        = stop_loss_spin_->value();
    strategy.take_profit      = take_profit_spin_->value();
    strategy.trailing_stop    = trailing_stop_spin_->value();

    status_label_->setText("Saving...");
    status_label_->setStyleSheet(
        QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
            .arg(fincept::ui::colors::TEXT_SECONDARY)
            .arg(fincept::ui::fonts::SMALL)
            .arg(kMonoFont()));

    AlgoTradingService::instance().save_strategy(strategy);
    LOG_INFO("AlgoTrading", QString("Saving strategy: %1").arg(strategy.name));

    {
        QJsonObject json;
        json["id"]            = strategy.id;
        json["name"]          = strategy.name;
        json["description"]   = strategy.description;
        json["timeframe"]     = strategy.timeframe;
        json["entry_logic"]   = strategy.entry_logic;
        json["exit_logic"]    = strategy.exit_logic;
        json["stop_loss"]     = strategy.stop_loss;
        json["take_profit"]   = strategy.take_profit;
        json["trailing_stop"] = strategy.trailing_stop;

        QJsonArray entry_arr;
        for (const auto& c : strategy.entry_conditions) entry_arr.append(c);
        json["entry_conditions"] = entry_arr;

        QJsonArray exit_arr;
        for (const auto& c : strategy.exit_conditions) exit_arr.append(c);
        json["exit_conditions"] = exit_arr;

        QString safe_name = strategy.name;
        safe_name.replace(QRegularExpression("[^a-zA-Z0-9_\\-]"), "_");
        QString dest = services::FileManagerService::instance().storage_dir() + "/" +
                       safe_name + "_" + strategy.id.left(8) + ".json";

        QFile f(dest);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
            f.close();
            services::FileManagerService::instance().register_file(
                safe_name + "_" + strategy.id.left(8) + ".json",
                strategy.name + ".json", QFileInfo(dest).size(),
                "application/json", "algo_trading");
        }
    }
}

// ── on_backtest ──────────────────────────────────────────────────────────────

void StrategyBuilderPanel::on_backtest() {
    QString symbol = bt_symbol_->text().trimmed();
    if (symbol.isEmpty()) {
        status_label_->setText("Enter a symbol for backtesting.");
        status_label_->setStyleSheet(
            QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                .arg(fincept::ui::colors::NEGATIVE)
                .arg(fincept::ui::fonts::SMALL)
                .arg(kMonoFont()));
        return;
    }

    status_label_->setText("Running backtest...");
    status_label_->setStyleSheet(
        QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
            .arg(fincept::ui::colors::CYAN)
            .arg(fincept::ui::fonts::SMALL)
            .arg(kMonoFont()));

    QString start_date = bt_start_date_->text().trimmed();
    QString end_date   = bt_end_date_->text().trimmed();
    if (start_date.isEmpty()) start_date = "2024-01-01";
    if (end_date.isEmpty())   end_date   = "2025-01-01";

    QString strat_id = name_edit_->text().trimmed();
    double  capital  = bt_capital_->value();

    AlgoTradingService::instance().run_backtest(strat_id, symbol, start_date, end_date, capital);
    LOG_INFO("AlgoTrading", QString("Backtest requested: %1 on %2").arg(strat_id, symbol));
}

// ── on_error ─────────────────────────────────────────────────────────────────

void StrategyBuilderPanel::on_error(const QString& context, const QString& msg) {
    if (status_label_) {
        status_label_->setText(QString("Error [%1]: %2").arg(context, msg));
        status_label_->setStyleSheet(
            QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                .arg(fincept::ui::colors::NEGATIVE)
                .arg(fincept::ui::fonts::SMALL)
                .arg(kMonoFont()));
    }
}

} // namespace fincept::screens
