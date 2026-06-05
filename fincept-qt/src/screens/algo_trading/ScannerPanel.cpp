// src/screens/algo_trading/ScannerPanel.cpp
#include "screens/algo_trading/ScannerPanel.h"

#include "algo_engine/AlgoScanner.h"
#include "algo_engine/CandleDataFetcher.h"
#include "core/logging/Logger.h"
#include "services/algo_trading/AlgoTradingService.h"
#include "trading/AccountManager.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <functional>

// ── Shared style constants ──────────────────────────────────────────────────

namespace {

inline QString kMonoFont() {
    return QString("font-family: %1;").arg(fincept::ui::fonts::DATA_FAMILY);
}

inline QString kLabelStyle() {
    return QString("color: %1; font-size: %2px; font-weight: 700; letter-spacing: 0.5px; %3"
                   " background: transparent; border: none;")
        .arg(fincept::ui::colors::TEXT_SECONDARY())
        .arg(fincept::ui::fonts::TINY)
        .arg(kMonoFont());
}

inline QString kSectionLabel() {
    return QString("color: %1; font-size: %2px; font-weight: 700; letter-spacing: 0.5px; %3"
                   " background: transparent; border: none;")
        .arg(fincept::ui::colors::AMBER())
        .arg(fincept::ui::fonts::TINY)
        .arg(kMonoFont());
}

inline QString kComboStyle() {
    return QString("QComboBox { background: %1; color: %2; border: 1px solid %3; padding: 4px 8px;"
                   " font-size: %4px; %5 }"
                   "QComboBox::drop-down { border: none; }"
                   "QComboBox QAbstractItemView { background: %1; color: %2; border: 1px solid %3;"
                   " selection-background-color: %6; %5 }")
        .arg(fincept::ui::colors::BG_SURFACE(), fincept::ui::colors::TEXT_PRIMARY(), fincept::ui::colors::BORDER_DIM())
        .arg(fincept::ui::fonts::SMALL)
        .arg(kMonoFont())
        .arg(fincept::ui::colors::BG_HOVER());
}

static QVector<QPair<QString, int>> range_options_for(const QString& tf) {
    if (tf == QStringLiteral("live")) return {};
    const int cap = fincept::services::algo::algo_default_lookback_days(tf);
    const QVector<QPair<QString, int>> ladder = {
        {QStringLiteral("Today"), 1}, {QStringLiteral("5D"), 5}, {QStringLiteral("1M"), 30},
        {QStringLiteral("6M"), 180},  {QStringLiteral("1Y"), 365}};
    QVector<QPair<QString, int>> out;
    for (const auto& o : ladder)
        if (o.second < cap) out.append(o);
    out.append({QStringLiteral("Max"), cap});
    return out;
}

} // namespace

namespace fincept::screens {

using namespace fincept::services::algo;

// ── Constructor ─────────────────────────────────────────────────────────────

ScannerPanel::ScannerPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
    connect_service();
    LOG_INFO("AlgoTrading", "ScannerPanel constructed");
}

// ── Service connections ─────────────────────────────────────────────────────

void ScannerPanel::connect_service() {
    auto& scanner = fincept::algo::AlgoScanner::instance();
    connect(&scanner, &fincept::algo::AlgoScanner::scan_complete, this, &ScannerPanel::on_scan_result);
    connect(&scanner, &fincept::algo::AlgoScanner::scan_error, this, [this](const QString& err) {
        on_error(QStringLiteral("scan"), err);
    });
}

// ── Apply preset ────────────────────────────────────────────────────────────

void ScannerPanel::apply_preset(int index) {
    const auto presets = scanner_presets();
    if (index <= 0 || index > presets.size()) {
        section_->clear_all();
        return; // index 0 = "Custom"
    }
    const auto& preset = presets[index - 1];
    section_->set_conditions(preset.conditions, QStringLiteral("AND"));
    LOG_INFO("AlgoTrading", QString("Applied scanner preset: %1").arg(preset.name));
}

// ── Range helper ─────────────────────────────────────────────────────────────

void ScannerPanel::rebuild_range_options() {
    const QString tf = timeframe_combo_->currentText();
    const auto opts = range_options_for(tf);
    const bool live = opts.isEmpty();
    range_lbl_->setVisible(!live);
    range_combo_->setVisible(!live);
    if (live) return;
    const QString prev = range_combo_->currentText();
    range_combo_->clear();
    for (const auto& o : opts)
        range_combo_->addItem(o.first, o.second);
    const int idx = range_combo_->findText(prev);
    range_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
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
                              .arg(fincept::ui::colors::BG_BASE(), fincept::ui::colors::BORDER_MED()));

    auto* content = new QWidget(this);
    content->setStyleSheet(QString("background: %1;").arg(fincept::ui::colors::BG_BASE()));
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

    cond_title_ = new QLabel(tr("SCAN CONDITIONS"), left_col);
    cond_title_->setStyleSheet(kSectionLabel());
    left_vl->addWidget(cond_title_);

    // Preset selector
    preset_lbl_ = new QLabel(tr("PRESET"), left_col);
    preset_lbl_->setStyleSheet(kLabelStyle());
    left_vl->addWidget(preset_lbl_);

    preset_combo_ = new QComboBox(left_col);
    preset_combo_->setStyleSheet(kComboStyle());
    preset_combo_->setFixedHeight(30);
    preset_combo_->addItem(tr("Custom"));
    const auto presets = scanner_presets();
    for (const auto& p : presets)
        preset_combo_->addItem(p.name);
    connect(preset_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ScannerPanel::apply_preset);
    left_vl->addWidget(preset_combo_);

    // Rich condition builder (shared with Strategy Builder): params, offsets,
    // indicator-vs-indicator RHS, nested AND/OR. Replaces the old flat rows.
    section_ = new fincept::ui::algo::ConditionSection(
        fincept::ui::algo::ConditionSection::Type::Entry, left_col);
    left_vl->addWidget(section_);
    left_vl->addStretch();

    columns->addWidget(left_col, 1);

    // ── RIGHT: Symbols + Params ─────────────────────────────────────────────
    auto* right_col = new QWidget(content);
    auto* right_vl = new QVBoxLayout(right_col);
    right_vl->setContentsMargins(0, 0, 0, 0);
    right_vl->setSpacing(8);

    sym_title_ = new QLabel(tr("SYMBOLS & PARAMETERS"), right_col);
    sym_title_->setStyleSheet(kSectionLabel());
    right_vl->addWidget(sym_title_);

    // Symbols text area
    sym_lbl_ = new QLabel(tr("SYMBOLS (comma or newline separated)"), right_col);
    sym_lbl_->setStyleSheet(kLabelStyle());
    right_vl->addWidget(sym_lbl_);

    symbols_input_ = new fincept::ui::algo::SymbolChipInput(right_col);
    symbols_input_->setMinimumHeight(54);
    right_vl->addWidget(symbols_input_);
    connect(symbols_input_, &fincept::ui::algo::SymbolChipInput::price_resolved,
            this, [this](const QString& s, double px) { prefill_close_from_price(s, px); });

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
            .arg(fincept::ui::colors::BG_RAISED(), fincept::ui::colors::TEXT_SECONDARY(), fincept::ui::colors::BORDER_DIM())
            .arg(fincept::ui::fonts::TINY)
            .arg(kMonoFont())
            .arg(fincept::ui::colors::BG_HOVER(), fincept::ui::colors::TEXT_PRIMARY()));
    connect(nifty_btn, &QPushButton::clicked, this,
            [this]() { symbols_input_->set_symbols(nifty50_symbols()); });
    quick_row->addWidget(nifty_btn);

    auto* banknifty_btn = new QPushButton("BANK NIFTY", right_col);
    banknifty_btn->setCursor(Qt::PointingHandCursor);
    banknifty_btn->setFixedHeight(26);
    banknifty_btn->setStyleSheet(nifty_btn->styleSheet());
    connect(banknifty_btn, &QPushButton::clicked, this,
            [this]() { symbols_input_->set_symbols(bank_nifty_symbols()); });
    quick_row->addWidget(banknifty_btn);
    quick_row->addStretch();

    right_vl->addLayout(quick_row);

    // Timeframe
    tf_lbl_ = new QLabel(tr("TIMEFRAME"), right_col);
    tf_lbl_->setStyleSheet(kLabelStyle());
    right_vl->addWidget(tf_lbl_);

    timeframe_combo_ = new QComboBox(right_col);
    timeframe_combo_->addItems(algo_timeframes());
    timeframe_combo_->setStyleSheet(kComboStyle());
    timeframe_combo_->setFixedHeight(30);
    timeframe_combo_->setCurrentIndex(algo_timeframes().indexOf("1d"));
    right_vl->addWidget(timeframe_combo_);

    // Range (replaces lookback spinbox — adapts to timeframe)
    range_lbl_ = new QLabel(tr("RANGE"), right_col);
    range_lbl_->setStyleSheet(kLabelStyle());
    right_vl->addWidget(range_lbl_);

    range_combo_ = new QComboBox(right_col);
    range_combo_->setStyleSheet(kComboStyle());
    range_combo_->setFixedHeight(30);
    right_vl->addWidget(range_combo_);

    connect(timeframe_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { rebuild_range_options(); });
    rebuild_range_options();

    // Data source
    ds_lbl_ = new QLabel(tr("DATA SOURCE"), right_col);
    ds_lbl_->setStyleSheet(kLabelStyle());
    right_vl->addWidget(ds_lbl_);

    data_source_combo_ = new QComboBox(right_col);
    // Visible labels translatable; the userData keys ("Auto"/"Broker"/"YFinance") drive logic.
    data_source_combo_->addItem(tr("Auto (Broker → YFinance)"), "Auto");
    data_source_combo_->addItem(tr("Broker Only"), "Broker");
    data_source_combo_->addItem(tr("YFinance Only"), "YFinance");
    data_source_combo_->setStyleSheet(kComboStyle());
    data_source_combo_->setFixedHeight(30);
    right_vl->addWidget(data_source_combo_);

    // Broker account selector (visible when Broker or Auto is selected)
    acct_lbl_ = new QLabel(tr("BROKER ACCOUNT"), right_col);
    acct_lbl_->setStyleSheet(kLabelStyle());
    right_vl->addWidget(acct_lbl_);

    account_combo_ = new QComboBox(right_col);
    account_combo_->setStyleSheet(kComboStyle());
    account_combo_->setFixedHeight(30);
    account_combo_->addItem(tr("None (use YFinance fallback)"), "");
    auto accounts = fincept::trading::AccountManager::instance().list_accounts();
    for (const auto& acct : accounts)
        account_combo_->addItem(
            QString("%1 (%2)").arg(acct.display_name, acct.broker_id), acct.account_id);
    right_vl->addWidget(account_combo_);

    connect(data_source_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            right_col, [this](int idx) {
                bool show = (idx != 2); // hide account for "YFinance Only"
                account_combo_->setVisible(show);
                if (acct_lbl_) acct_lbl_->setVisible(show);
            });

    right_vl->addStretch();

    columns->addWidget(right_col, 1);

    main_vl->addLayout(columns);

    // ── SCAN button ─────────────────────────────────────────────────────────
    scan_btn_ = new QPushButton(tr("SCAN MARKET"), content);
    scan_btn_->setCursor(Qt::PointingHandCursor);
    scan_btn_->setFixedHeight(38);
    scan_btn_->setStyleSheet(QString("QPushButton { background: rgba(255,196,0,0.1); color: #FFC400;"
                                     " border: 1px solid #FFC400; font-size: %1px; font-weight: 700; %2"
                                     " padding: 6px 24px; }"
                                     "QPushButton:hover { background: #FFC400; color: %3; }")
                                 .arg(fincept::ui::fonts::DATA)
                                 .arg(kMonoFont())
                                 .arg(fincept::ui::colors::BG_BASE()));
    connect(scan_btn_, &QPushButton::clicked, this, &ScannerPanel::on_scan);
    main_vl->addWidget(scan_btn_);

    // + CREATE ALERT button
    auto* create_alert_btn = new QPushButton(tr("+ CREATE ALERT"), content);
    create_alert_btn->setCursor(Qt::PointingHandCursor);
    create_alert_btn->setFixedHeight(38);
    create_alert_btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1;"
        " border:1px solid %2; font-size:%3px; font-weight:700; %4 padding:6px 24px; }"
        "QPushButton:hover { color:%5; border-color:%5; }")
        .arg(fincept::ui::colors::TEXT_SECONDARY(), fincept::ui::colors::BORDER_DIM())
        .arg(fincept::ui::fonts::DATA).arg(QString("font-family:%1;").arg(fincept::ui::fonts::DATA_FAMILY))
        .arg(fincept::ui::colors::AMBER()));
    connect(create_alert_btn, &QPushButton::clicked, this, [this]() {
        const QJsonArray conds = section_->conditions();
        if (conds.isEmpty()) { status_label_->setText(tr("Add a condition first.")); return; }
        QStringList syms = symbols_input_->symbols();
        emit create_alert_requested(conds, section_->combined_logic(), syms,
                                    timeframe_combo_->currentText(),
                                    data_source_combo_->currentData().toString(),
                                    account_combo_->currentData().toString());
    });
    main_vl->addWidget(create_alert_btn);

    // Status
    status_label_ = new QLabel("", content);
    status_label_->setWordWrap(true);
    status_label_->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                                     .arg(fincept::ui::colors::TEXT_TERTIARY())
                                     .arg(fincept::ui::fonts::SMALL)
                                     .arg(kMonoFont()));
    main_vl->addWidget(status_label_);

    // ── Results area ────────────────────────────────────────────────────────
    results_title_ = new QLabel(tr("SCAN RESULTS"), content);
    results_title_->setStyleSheet(kSectionLabel());
    main_vl->addWidget(results_title_);

    results_table_ = new QTableWidget(0, 5, content);
    results_table_->setHorizontalHeaderLabels(
        {tr("SYMBOL"), tr("SIGNAL"), tr("MATCH"), tr("TIMEFRAME"), tr("DETAILS")});
    results_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    results_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    results_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    results_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    results_table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    results_table_->setColumnWidth(0, 100);
    results_table_->setColumnWidth(1, 90);
    results_table_->setColumnWidth(2, 80);
    results_table_->setColumnWidth(3, 90);
    results_table_->setSortingEnabled(true);
    results_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    results_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    results_table_->setAlternatingRowColors(true);
    results_table_->verticalHeader()->setVisible(false);
    results_table_->setMinimumHeight(160);
    results_table_->setStyleSheet(
        QString("QTableWidget { background: %1; border: 1px solid %2; gridline-color: %2;"
                " font-size: %3px; font-family: %4; color: %5;"
                " alternate-background-color: %6; }"
                "QTableWidget::item { padding: 4px 8px; border: none; }"
                "QHeaderView::section { background: %7; color: %8; font-size: %3px;"
                " font-weight: 700; font-family: %4; padding: 4px 8px;"
                " border: none; border-bottom: 1px solid %2; }"
                "QTableWidget::item:selected { background: %9; color: %5; }")
            .arg(fincept::ui::colors::BG_BASE(), fincept::ui::colors::BORDER_DIM())
            .arg(fincept::ui::fonts::SMALL)
            .arg(fincept::ui::fonts::DATA_FAMILY)
            .arg(fincept::ui::colors::TEXT_PRIMARY())
            .arg(fincept::ui::colors::BG_SURFACE())
            .arg(fincept::ui::colors::BG_RAISED())
            .arg(fincept::ui::colors::TEXT_TERTIARY())
            .arg(fincept::ui::colors::BG_HOVER()));
    main_vl->addWidget(results_table_);

    main_vl->addStretch();

    scroll->setWidget(content);
    root->addWidget(scroll);
}

// ── Slots ───────────────────────────────────────────────────────────────────

void ScannerPanel::on_scan() {
    QJsonArray conditions = section_->conditions();
    if (conditions.isEmpty()) {
        status_label_->setText(tr("Add at least one scan condition."));
        status_label_->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                                         .arg(fincept::ui::colors::NEGATIVE())
                                         .arg(fincept::ui::fonts::SMALL)
                                         .arg(kMonoFont()));
        return;
    }

    // Parse symbols
    QStringList symbols = symbols_input_->symbols();
    if (symbols.isEmpty()) {
        status_label_->setText(tr("Enter symbols to scan."));
        status_label_->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                                         .arg(fincept::ui::colors::NEGATIVE())
                                         .arg(fincept::ui::fonts::SMALL)
                                         .arg(kMonoFont()));
        return;
    }

    status_label_->setText(tr("Scanning %1 symbols...").arg(symbols.size()));
    status_label_->setStyleSheet(QString("color: #FFC400; font-size: %1px; %2 background: transparent; border: none;")
                                     .arg(fincept::ui::fonts::SMALL)
                                     .arg(kMonoFont()));

    auto source = fincept::algo::data_source_from_string(
        data_source_combo_->currentData().toString());

    QString broker_id, account_id;
    if (source != fincept::algo::DataSource::YFinance) {
        account_id = account_combo_->currentData().toString();
        if (!account_id.isEmpty()) {
            auto acct = fincept::trading::AccountManager::instance().get_account(account_id);
            broker_id = acct.broker_id;
        }
    }

    const int lookback_days = range_combo_->isVisible()
        ? range_combo_->currentData().toInt()
        : algo_default_lookback_days(timeframe_combo_->currentText());

    fincept::algo::AlgoScanner::instance().scan(
        conditions, symbols, timeframe_combo_->currentText(),
        lookback_days, section_->combined_logic(),
        source, broker_id, account_id);

    LOG_INFO("AlgoTrading",
             QString("Scan started: %1 conditions, %2 symbols").arg(conditions.size()).arg(symbols.size()));
}

void ScannerPanel::on_scan_result(const QJsonObject& payload) {
    results_table_->setSortingEnabled(false);
    results_table_->setRowCount(0);

    QJsonArray matches         = payload.value("matches").toArray();
    int        total_scanned   = payload.value("total_scanned").toInt();
    int        condition_count = payload.value("condition_count").toInt();

    status_label_->setText(
        tr("Scan complete: %1 matches out of %2 symbols").arg(matches.size()).arg(total_scanned));
    status_label_->setStyleSheet(
        QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
            .arg(fincept::ui::colors::POSITIVE())
            .arg(fincept::ui::fonts::SMALL)
            .arg(kMonoFont()));

    for (const auto& mval : matches) {
        QJsonObject match         = mval.toObject();
        QString     symbol        = match.value("symbol").toString();
        QJsonArray  matched_conds = match.value("conditions").toArray();

        // Infer signal direction from operator types
        int bullish_count = 0, bearish_count = 0;
        for (const auto& cv : matched_conds) {
            QString op = cv.toObject()["operator"].toString();
            if (op == ">" || op == ">=" || op == "crosses above")
                ++bullish_count;
            else if (op == "<" || op == "<=" || op == "crosses below")
                ++bearish_count;
        }
        QString signal_text, signal_color;
        if (bullish_count > 0 && bearish_count == 0) {
            signal_text  = tr("BULLISH");
            signal_color = fincept::ui::colors::POSITIVE();
        } else if (bearish_count > 0 && bullish_count == 0) {
            signal_text  = tr("BEARISH");
            signal_color = fincept::ui::colors::NEGATIVE();
        } else {
            signal_text  = tr("NEUTRAL");
            signal_color = fincept::ui::colors::TEXT_SECONDARY();
        }

        // Build condition detail string
        QStringList cond_strs;
        for (const auto& cv : matched_conds) {
            QJsonObject c = cv.toObject();
            cond_strs.append(QString("%1 %2 %3 %4")
                .arg(c["indicator"].toString(), c["field"].toString(),
                     c["operator"].toString(),
                     QString::number(c["value"].toDouble(), 'f', 2)));
        }

        int row = results_table_->rowCount();
        results_table_->insertRow(row);

        // Col 0: Symbol
        auto* sym_item = new QTableWidgetItem(symbol);
        sym_item->setForeground(QColor(fincept::ui::colors::AMBER()));
        sym_item->setFont(QFont(fincept::ui::fonts::DATA_FAMILY(),
                                fincept::ui::fonts::SMALL, QFont::Bold));
        results_table_->setItem(row, 0, sym_item);

        // Col 1: Signal
        auto* sig_item = new QTableWidgetItem(signal_text);
        sig_item->setForeground(QColor(signal_color));
        sig_item->setFont(QFont(fincept::ui::fonts::DATA_FAMILY(),
                                fincept::ui::fonts::TINY, QFont::Bold));
        results_table_->setItem(row, 1, sig_item);

        // Col 2: Match count
        auto* match_item = new QTableWidgetItem(
            QString("%1/%2").arg(matched_conds.size()).arg(condition_count));
        match_item->setForeground(QColor(fincept::ui::colors::POSITIVE()));
        match_item->setTextAlignment(Qt::AlignCenter);
        results_table_->setItem(row, 2, match_item);

        // Col 3: Timeframe
        auto* tf_item = new QTableWidgetItem(timeframe_combo_->currentText().toUpper());
        tf_item->setForeground(QColor(fincept::ui::colors::CYAN()));
        tf_item->setTextAlignment(Qt::AlignCenter);
        results_table_->setItem(row, 3, tf_item);

        // Col 4: Details
        auto* detail_item = new QTableWidgetItem(cond_strs.join(" | "));
        detail_item->setForeground(QColor(fincept::ui::colors::TEXT_SECONDARY()));
        results_table_->setItem(row, 4, detail_item);
    }

    results_table_->setSortingEnabled(true);
    LOG_INFO("AlgoTrading", QString("Scan results: %1 matches").arg(matches.size()));
}

void ScannerPanel::on_error(const QString& context, const QString& msg) {
    if (status_label_) {
        status_label_->setText(tr("Error [%1]: %2").arg(context, msg));
        status_label_->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                                         .arg(fincept::ui::colors::NEGATIVE())
                                         .arg(fincept::ui::fonts::SMALL)
                                         .arg(kMonoFont()));
    }
}

// ── Live language switch ──────────────────────────────────────────────────────

void ScannerPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void ScannerPanel::retranslateUi() {
    if (cond_title_)    cond_title_->setText(tr("SCAN CONDITIONS"));
    if (preset_lbl_)    preset_lbl_->setText(tr("PRESET"));
    if (sym_title_)     sym_title_->setText(tr("SYMBOLS & PARAMETERS"));
    if (sym_lbl_)       sym_lbl_->setText(tr("SYMBOLS (comma or newline separated)"));
    if (tf_lbl_)        tf_lbl_->setText(tr("TIMEFRAME"));
    if (range_lbl_)     range_lbl_->setText(tr("RANGE"));
    if (ds_lbl_)        ds_lbl_->setText(tr("DATA SOURCE"));
    if (acct_lbl_)      acct_lbl_->setText(tr("BROKER ACCOUNT"));
    if (scan_btn_)      scan_btn_->setText(tr("SCAN MARKET"));
    if (results_title_) results_title_->setText(tr("SCAN RESULTS"));

    // preset_combo_ item 0 is the fixed "Custom" entry; remaining items are
    // preset data names. Selection index drives apply_preset(), not the text.
    if (preset_combo_ && preset_combo_->count() > 0)
        preset_combo_->setItemText(0, tr("Custom"));

    // data_source_combo_ — visible labels only; userData keys are unchanged.
    if (data_source_combo_ && data_source_combo_->count() >= 3) {
        data_source_combo_->setItemText(0, tr("Auto (Broker → YFinance)"));
        data_source_combo_->setItemText(1, tr("Broker Only"));
        data_source_combo_->setItemText(2, tr("YFinance Only"));
    }

    // account_combo_ item 0 is the fixed "None" fallback; the rest are account data.
    if (account_combo_ && account_combo_->count() > 0)
        account_combo_->setItemText(0, tr("None (use YFinance fallback)"));

    if (results_table_) {
        results_table_->setHorizontalHeaderLabels(
            {tr("SYMBOL"), tr("SIGNAL"), tr("MATCH"), tr("TIMEFRAME"), tr("DETAILS")});
    }
    // Result-row signal labels (BULLISH/BEARISH/NEUTRAL) re-render on the next scan.
}

// ── CLOSE pre-fill helper ─────────────────────────────────────────────────────

void ScannerPanel::prefill_close_from_price(const QString& /*symbol*/, double price) {
    if (price <= 0) return;
    QJsonArray conds = section_->conditions();
    bool changed = false;
    std::function<void(QJsonArray&)> walk = [&](QJsonArray& arr) {
        for (int i = 0; i < arr.size(); ++i) {
            QJsonObject o = arr[i].toObject();
            if (o.contains("children")) { QJsonArray ch = o.value("children").toArray(); walk(ch); o["children"] = ch; }
            else if (o.value("indicator").toString() == "CLOSE"
                     && o.value("compare_mode").toString("value") == "value"
                     && qFuzzyIsNull(o.value("value").toDouble())) {
                o["value"] = price; changed = true;
            }
            arr[i] = o;
        }
    };
    walk(conds);
    if (changed) section_->set_conditions(conds, section_->combined_logic());
}

} // namespace fincept::screens
