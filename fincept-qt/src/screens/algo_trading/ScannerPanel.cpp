// src/screens/algo_trading/ScannerPanel.cpp
#include "screens/algo_trading/ScannerPanel.h"

#include "core/logging/Logger.h"
#include "services/algo_trading/AlgoTradingService.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>

// ── Shared style constants ──────────────────────────────────────────────────

namespace {

inline QString kMonoFont() {
    return QString("font-family: %1;").arg(fincept::ui::fonts::DATA_FAMILY);
}

inline QString kLabelStyle() {
    return QString("color: %1; font-size: %2px; font-weight: 700; letter-spacing: 0.5px; %3"
                   " background: transparent; border: none;")
        .arg(fincept::ui::colors::TEXT_SECONDARY)
        .arg(fincept::ui::fonts::TINY)
        .arg(kMonoFont());
}

inline QString kSectionLabel() {
    return QString("color: %1; font-size: %2px; font-weight: 700; letter-spacing: 0.5px; %3"
                   " background: transparent; border: none;")
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
                   "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { width: 14px; }"
                   "QSpinBox { background: %1; color: %2; border: 1px solid %3; padding: 4px 8px;"
                   " font-size: %4px; %5 }"
                   "QSpinBox::up-button, QSpinBox::down-button { width: 14px; }")
        .arg(fincept::ui::colors::BG_SURFACE, fincept::ui::colors::TEXT_PRIMARY, fincept::ui::colors::BORDER_DIM)
        .arg(fincept::ui::fonts::SMALL)
        .arg(kMonoFont());
}

inline QString kTextEditStyle() {
    return QString("QTextEdit { background: %1; border: 1px solid %2; color: %3; padding: 6px;"
                   " font-size: %4px; %5 }"
                   "QTextEdit:focus { border-color: %6; }")
        .arg(fincept::ui::colors::BG_SURFACE, fincept::ui::colors::BORDER_DIM, fincept::ui::colors::TEXT_PRIMARY)
        .arg(fincept::ui::fonts::SMALL)
        .arg(kMonoFont())
        .arg(fincept::ui::colors::BORDER_BRIGHT);
}

} // namespace

namespace fincept::screens {

using namespace fincept::services::algo;

// ── Helper: build a condition row ───────────────────────────────────────────

static QWidget* build_condition_row(QVBoxLayout* owner_layout, QWidget* parent) {
    auto* row = new QWidget(parent);
    row->setStyleSheet(QString("background: %1; border: 1px solid %2;")
                           .arg(fincept::ui::colors::BG_SURFACE, fincept::ui::colors::BORDER_DIM));
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(6, 4, 6, 4);
    hl->setSpacing(4);

    // Indicator
    auto* ind_combo = new QComboBox(row);
    ind_combo->setStyleSheet(kComboStyle());
    ind_combo->setFixedHeight(28);
    ind_combo->setMinimumWidth(120);
    const auto indicators = algo_indicators();
    for (const auto& ind : indicators)
        ind_combo->addItem(ind.label, ind.id);

    // Field
    auto* field_combo = new QComboBox(row);
    field_combo->setStyleSheet(kComboStyle());
    field_combo->setFixedHeight(28);
    field_combo->setMinimumWidth(90);
    QObject::connect(ind_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), row,
                     [field_combo, indicators](int idx) {
                         field_combo->clear();
                         if (idx >= 0 && idx < indicators.size())
                             field_combo->addItems(indicators[idx].fields);
                     });

    // Operator
    auto* op_combo = new QComboBox(row);
    op_combo->setStyleSheet(kComboStyle());
    op_combo->setFixedHeight(28);
    op_combo->setMinimumWidth(100);
    op_combo->addItems(algo_operators());

    // Value
    auto* val_spin = new QDoubleSpinBox(row);
    val_spin->setStyleSheet(kSpinStyle());
    val_spin->setFixedHeight(28);
    val_spin->setMinimumWidth(90);
    val_spin->setRange(-1e9, 1e9);
    val_spin->setDecimals(4);
    val_spin->setValue(0);

    // Remove
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
    QObject::connect(rm_btn, &QPushButton::clicked, row, [row]() { row->deleteLater(); });

    hl->addWidget(ind_combo);
    hl->addWidget(field_combo);
    hl->addWidget(op_combo);
    hl->addWidget(val_spin);
    hl->addWidget(rm_btn);

    if (ind_combo->count() > 0)
        emit ind_combo->currentIndexChanged(0);

    row->setProperty("ind_combo", QVariant::fromValue(static_cast<QObject*>(ind_combo)));
    row->setProperty("field_combo", QVariant::fromValue(static_cast<QObject*>(field_combo)));
    row->setProperty("op_combo", QVariant::fromValue(static_cast<QObject*>(op_combo)));
    row->setProperty("val_spin", QVariant::fromValue(static_cast<QObject*>(val_spin)));

    return row;
}

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
        cond["params"] = QJsonObject{};
        arr.append(cond);
    }
    return arr;
}

// ── Constructor ─────────────────────────────────────────────────────────────

ScannerPanel::ScannerPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
    connect_service();
    LOG_INFO("AlgoTrading", "ScannerPanel constructed");
}

// ── Service connections ─────────────────────────────────────────────────────

void ScannerPanel::connect_service() {
    auto& svc = AlgoTradingService::instance();
    connect(&svc, &AlgoTradingService::scan_result, this, &ScannerPanel::on_scan_result);
    connect(&svc, &AlgoTradingService::error_occurred, this, &ScannerPanel::on_error);
}

// ── Apply preset ────────────────────────────────────────────────────────────

void ScannerPanel::apply_preset(int index) {
    // Clear existing conditions
    while (conditions_layout_->count() > 0) {
        auto* item = conditions_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    const auto presets = scanner_presets();
    if (index <= 0 || index > presets.size())
        return; // index 0 = "Custom"

    const auto& preset = presets[index - 1];
    const auto indicators = algo_indicators();

    for (const auto& cval : preset.conditions) {
        QJsonObject cond = cval.toObject();
        auto* row = build_condition_row(conditions_layout_, nullptr);

        auto* ind_combo = qobject_cast<QComboBox*>(row->property("ind_combo").value<QObject*>());
        auto* op_combo = qobject_cast<QComboBox*>(row->property("op_combo").value<QObject*>());
        auto* val_spin = qobject_cast<QDoubleSpinBox*>(row->property("val_spin").value<QObject*>());

        if (ind_combo) {
            QString ind_id = cond["indicator"].toString();
            for (int i = 0; i < ind_combo->count(); ++i) {
                if (ind_combo->itemData(i).toString() == ind_id) {
                    ind_combo->setCurrentIndex(i);
                    break;
                }
            }
        }
        if (op_combo) {
            int op_idx = op_combo->findText(cond["operator"].toString());
            if (op_idx >= 0)
                op_combo->setCurrentIndex(op_idx);
        }
        if (val_spin)
            val_spin->setValue(cond["value"].toDouble());

        conditions_layout_->addWidget(row);
    }

    LOG_INFO("AlgoTrading", QString("Applied scanner preset: %1").arg(preset.name));
}

// ── Build UI ────────────────────────────────────────────────────────────────

void ScannerPanel::build_ui() {
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
    auto* main_vl = new QVBoxLayout(content);
    main_vl->setContentsMargins(16, 12, 16, 12);
    main_vl->setSpacing(12);

    // Two-column layout
    auto* columns = new QHBoxLayout;
    columns->setSpacing(16);

    // ── LEFT: Conditions ────────────────────────────────────────────────────
    auto* left_col = new QWidget(content);
    auto* left_vl = new QVBoxLayout(left_col);
    left_vl->setContentsMargins(0, 0, 0, 0);
    left_vl->setSpacing(8);

    auto* cond_title = new QLabel("SCAN CONDITIONS", left_col);
    cond_title->setStyleSheet(kSectionLabel());
    left_vl->addWidget(cond_title);

    // Preset selector
    auto* preset_lbl = new QLabel("PRESET", left_col);
    preset_lbl->setStyleSheet(kLabelStyle());
    left_vl->addWidget(preset_lbl);

    preset_combo_ = new QComboBox(left_col);
    preset_combo_->setStyleSheet(kComboStyle());
    preset_combo_->setFixedHeight(30);
    preset_combo_->addItem("Custom");
    const auto presets = scanner_presets();
    for (const auto& p : presets)
        preset_combo_->addItem(p.name);
    connect(preset_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ScannerPanel::apply_preset);
    left_vl->addWidget(preset_combo_);

    // Logic combo
    auto* logic_row = new QHBoxLayout;
    logic_row->setSpacing(8);
    auto* logic_lbl = new QLabel("LOGIC:", left_col);
    logic_lbl->setStyleSheet(kLabelStyle());
    logic_row->addWidget(logic_lbl);

    auto* logic_combo = new QComboBox(left_col);
    logic_combo->addItems({"AND", "OR"});
    logic_combo->setStyleSheet(kComboStyle());
    logic_combo->setFixedHeight(26);
    logic_combo->setFixedWidth(80);
    logic_row->addWidget(logic_combo);
    logic_row->addStretch();
    left_vl->addLayout(logic_row);

    // Conditions container
    auto* cond_container = new QWidget(left_col);
    conditions_layout_ = new QVBoxLayout(cond_container);
    conditions_layout_->setContentsMargins(0, 0, 0, 0);
    conditions_layout_->setSpacing(4);
    left_vl->addWidget(cond_container);

    // Add condition button
    auto* add_cond_btn = new QPushButton("+ ADD CONDITION", left_col);
    add_cond_btn->setCursor(Qt::PointingHandCursor);
    add_cond_btn->setFixedHeight(28);
    add_cond_btn->setStyleSheet(QString("QPushButton { background: transparent; color: %1; border: 1px dashed %2;"
                                        " font-size: %3px; font-weight: 700; %4 }"
                                        "QPushButton:hover { color: %5; border-color: %5; }")
                                    .arg(fincept::ui::colors::TEXT_TERTIARY, fincept::ui::colors::BORDER_DIM)
                                    .arg(fincept::ui::fonts::TINY)
                                    .arg(kMonoFont())
                                    .arg(fincept::ui::colors::AMBER));
    connect(add_cond_btn, &QPushButton::clicked, this, [this, cond_container]() {
        conditions_layout_->addWidget(build_condition_row(conditions_layout_, cond_container));
    });
    left_vl->addWidget(add_cond_btn);
    left_vl->addStretch();

    columns->addWidget(left_col, 1);

    // ── RIGHT: Symbols + Params ─────────────────────────────────────────────
    auto* right_col = new QWidget(content);
    auto* right_vl = new QVBoxLayout(right_col);
    right_vl->setContentsMargins(0, 0, 0, 0);
    right_vl->setSpacing(8);

    auto* sym_title = new QLabel("SYMBOLS & PARAMETERS", right_col);
    sym_title->setStyleSheet(kSectionLabel());
    right_vl->addWidget(sym_title);

    // Symbols text area
    auto* sym_lbl = new QLabel("SYMBOLS (comma or newline separated)", right_col);
    sym_lbl->setStyleSheet(kLabelStyle());
    right_vl->addWidget(sym_lbl);

    symbols_edit_ = new QTextEdit(right_col);
    symbols_edit_->setStyleSheet(kTextEditStyle());
    symbols_edit_->setFixedHeight(120);
    symbols_edit_->setPlaceholderText("RELIANCE\nTCS\nINFY\n...");
    right_vl->addWidget(symbols_edit_);

    // Quick-add buttons
    auto* quick_row = new QHBoxLayout;
    quick_row->setSpacing(8);

    auto* nifty_btn = new QPushButton("NIFTY 50", right_col);
    nifty_btn->setCursor(Qt::PointingHandCursor);
    nifty_btn->setFixedHeight(26);
    nifty_btn->setStyleSheet(
        QString("QPushButton { background: %1; color: %2; border: 1px solid %3;"
                " font-size: %4px; font-weight: 700; %5 padding: 2px 10px; }"
                "QPushButton:hover { background: %6; color: %7; }")
            .arg(fincept::ui::colors::BG_RAISED, fincept::ui::colors::TEXT_SECONDARY, fincept::ui::colors::BORDER_DIM)
            .arg(fincept::ui::fonts::TINY)
            .arg(kMonoFont())
            .arg(fincept::ui::colors::BG_HOVER, fincept::ui::colors::TEXT_PRIMARY));
    connect(nifty_btn, &QPushButton::clicked, this,
            [this]() { symbols_edit_->setPlainText(nifty50_symbols().join("\n")); });
    quick_row->addWidget(nifty_btn);

    auto* banknifty_btn = new QPushButton("BANK NIFTY", right_col);
    banknifty_btn->setCursor(Qt::PointingHandCursor);
    banknifty_btn->setFixedHeight(26);
    banknifty_btn->setStyleSheet(nifty_btn->styleSheet());
    connect(banknifty_btn, &QPushButton::clicked, this,
            [this]() { symbols_edit_->setPlainText(bank_nifty_symbols().join("\n")); });
    quick_row->addWidget(banknifty_btn);
    quick_row->addStretch();

    right_vl->addLayout(quick_row);

    // Timeframe
    auto* tf_lbl = new QLabel("TIMEFRAME", right_col);
    tf_lbl->setStyleSheet(kLabelStyle());
    right_vl->addWidget(tf_lbl);

    timeframe_combo_ = new QComboBox(right_col);
    timeframe_combo_->addItems(algo_timeframes());
    timeframe_combo_->setStyleSheet(kComboStyle());
    timeframe_combo_->setFixedHeight(30);
    timeframe_combo_->setCurrentIndex(algo_timeframes().indexOf("1d"));
    right_vl->addWidget(timeframe_combo_);

    // Lookback
    auto* lb_lbl = new QLabel("LOOKBACK (DAYS)", right_col);
    lb_lbl->setStyleSheet(kLabelStyle());
    right_vl->addWidget(lb_lbl);

    lookback_spin_ = new QSpinBox(right_col);
    lookback_spin_->setStyleSheet(kSpinStyle());
    lookback_spin_->setFixedHeight(30);
    lookback_spin_->setRange(1, 3650);
    lookback_spin_->setValue(365);
    right_vl->addWidget(lookback_spin_);

    right_vl->addStretch();

    columns->addWidget(right_col, 1);

    main_vl->addLayout(columns);

    // ── SCAN button ─────────────────────────────────────────────────────────
    auto* scan_btn = new QPushButton("SCAN MARKET", content);
    scan_btn->setCursor(Qt::PointingHandCursor);
    scan_btn->setFixedHeight(38);
    scan_btn->setStyleSheet(QString("QPushButton { background: rgba(255,196,0,0.1); color: #FFC400;"
                                    " border: 1px solid #FFC400; font-size: %1px; font-weight: 700; %2"
                                    " padding: 6px 24px; }"
                                    "QPushButton:hover { background: #FFC400; color: %3; }")
                                .arg(fincept::ui::fonts::DATA)
                                .arg(kMonoFont())
                                .arg(fincept::ui::colors::BG_BASE));
    connect(scan_btn, &QPushButton::clicked, this, &ScannerPanel::on_scan);
    main_vl->addWidget(scan_btn);

    // Status
    status_label_ = new QLabel("", content);
    status_label_->setWordWrap(true);
    status_label_->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                                     .arg(fincept::ui::colors::TEXT_TERTIARY)
                                     .arg(fincept::ui::fonts::SMALL)
                                     .arg(kMonoFont()));
    main_vl->addWidget(status_label_);

    // ── Results area ────────────────────────────────────────────────────────
    auto* results_title = new QLabel("SCAN RESULTS", content);
    results_title->setStyleSheet(kSectionLabel());
    main_vl->addWidget(results_title);

    auto* results_container = new QWidget(content);
    results_layout_ = new QVBoxLayout(results_container);
    results_layout_->setContentsMargins(0, 0, 0, 0);
    results_layout_->setSpacing(4);
    main_vl->addWidget(results_container);

    main_vl->addStretch();

    scroll->setWidget(content);
    root->addWidget(scroll);
}

// ── Slots ───────────────────────────────────────────────────────────────────

void ScannerPanel::on_scan() {
    QJsonArray conditions = gather_from_layout(conditions_layout_);
    if (conditions.isEmpty()) {
        status_label_->setText("Add at least one scan condition.");
        status_label_->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                                         .arg(fincept::ui::colors::NEGATIVE)
                                         .arg(fincept::ui::fonts::SMALL)
                                         .arg(kMonoFont()));
        return;
    }

    // Parse symbols
    QString raw = symbols_edit_->toPlainText().trimmed();
    if (raw.isEmpty()) {
        status_label_->setText("Enter symbols to scan.");
        status_label_->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                                         .arg(fincept::ui::colors::NEGATIVE)
                                         .arg(fincept::ui::fonts::SMALL)
                                         .arg(kMonoFont()));
        return;
    }

    QStringList symbols;
    for (const auto& line : raw.split(QRegularExpression("[,\\n\\r]+"), Qt::SkipEmptyParts)) {
        QString s = line.trimmed().toUpper();
        if (!s.isEmpty())
            symbols.append(s);
    }

    status_label_->setText(QString("Scanning %1 symbols...").arg(symbols.size()));
    status_label_->setStyleSheet(QString("color: #FFC400; font-size: %1px; %2 background: transparent; border: none;")
                                     .arg(fincept::ui::fonts::SMALL)
                                     .arg(kMonoFont()));

    AlgoTradingService::instance().run_scan(conditions, symbols, timeframe_combo_->currentText(),
                                            lookback_spin_->value());

    LOG_INFO("AlgoTrading",
             QString("Scan started: %1 conditions, %2 symbols").arg(conditions.size()).arg(symbols.size()));
}

void ScannerPanel::on_scan_result(const QJsonObject& data) {
    // Clear previous results
    while (results_layout_->count() > 0) {
        auto* item = results_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    QJsonArray matches = data.value("matches").toArray();
    int total_scanned = data.value("total_scanned").toInt();

    status_label_->setText(
        QString("Scan complete: %1 matches out of %2 symbols").arg(matches.size()).arg(total_scanned));
    status_label_->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                                     .arg(fincept::ui::colors::POSITIVE)
                                     .arg(fincept::ui::fonts::SMALL)
                                     .arg(kMonoFont()));

    if (matches.isEmpty()) {
        auto* empty_lbl = new QLabel("No symbols matched the scan conditions.");
        empty_lbl->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: transparent; border: none;"
                                         " padding: 12px;")
                                     .arg(fincept::ui::colors::TEXT_TERTIARY)
                                     .arg(fincept::ui::fonts::SMALL)
                                     .arg(kMonoFont()));
        results_layout_->addWidget(empty_lbl);
        return;
    }

    for (const auto& mval : matches) {
        QJsonObject match = mval.toObject();
        QString symbol = match.value("symbol").toString();
        QJsonArray matched_conds = match.value("conditions").toArray();

        auto* card = new QWidget;
        card->setStyleSheet(QString("background: %1; border: 1px solid %2;")
                                .arg(fincept::ui::colors::BG_SURFACE, fincept::ui::colors::BORDER_DIM));
        auto* card_hl = new QHBoxLayout(card);
        card_hl->setContentsMargins(10, 8, 10, 8);
        card_hl->setSpacing(12);

        // Symbol
        auto* sym_lbl = new QLabel(symbol, card);
        sym_lbl->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: 700; %3"
                                       " background: transparent; border: none;")
                                   .arg(fincept::ui::colors::AMBER)
                                   .arg(fincept::ui::fonts::DATA)
                                   .arg(kMonoFont()));
        sym_lbl->setFixedWidth(100);
        card_hl->addWidget(sym_lbl);

        // Condition details
        QStringList cond_strs;
        for (const auto& cv : matched_conds) {
            QJsonObject c = cv.toObject();
            cond_strs.append(QString("%1 %2 %3 %4")
                                 .arg(c["indicator"].toString(), c["field"].toString(), c["operator"].toString(),
                                      QString::number(c["value"].toDouble(), 'f', 2)));
        }
        auto* detail_lbl = new QLabel(cond_strs.join(" | "), card);
        detail_lbl->setWordWrap(true);
        detail_lbl->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                                      .arg(fincept::ui::colors::TEXT_SECONDARY)
                                      .arg(fincept::ui::fonts::SMALL)
                                      .arg(kMonoFont()));
        card_hl->addWidget(detail_lbl, 1);

        // Match count badge
        auto* cnt_badge =
            new QLabel(QString("%1/%2").arg(matched_conds.size()).arg(data.value("condition_count").toInt()), card);
        cnt_badge->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: 700; %3"
                                         " padding: 2px 6px; background: rgba(22,163,74,0.08);"
                                         " border: 1px solid rgba(22,163,74,0.25);")
                                     .arg(fincept::ui::colors::POSITIVE)
                                     .arg(fincept::ui::fonts::TINY)
                                     .arg(kMonoFont()));
        card_hl->addWidget(cnt_badge);

        results_layout_->addWidget(card);
    }

    LOG_INFO("AlgoTrading", QString("Scan results: %1 matches").arg(matches.size()));
}

void ScannerPanel::on_error(const QString& context, const QString& msg) {
    if (status_label_) {
        status_label_->setText(QString("Error [%1]: %2").arg(context, msg));
        status_label_->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                                         .arg(fincept::ui::colors::NEGATIVE)
                                         .arg(fincept::ui::fonts::SMALL)
                                         .arg(kMonoFont()));
    }
}

} // namespace fincept::screens
