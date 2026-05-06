// QuantModulePanel_HFT.cpp — HFT (high-frequency trading) panel builder.
// Method definition split from QuantModulePanel_Misc.cpp.

#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"

#include "core/logging/Logger.h"
#include "services/ai_quant_lab/AIQuantLabService.h"
#include "services/file_manager/FileManagerService.h"
#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QColor>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDateTime>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

using namespace fincept::services::quant;
using namespace fincept::screens::quant_styles;
using namespace fincept::screens::quant_common;
using namespace fincept::screens::quant_gs_helpers;


// ═══════════════════════════════════════════════════════════════════════════════
// HFT PANEL
// ═══════════════════════════════════════════════════════════════════════════════
QWidget* QuantModulePanel::build_hft_panel() {
    const QString accent = module_.color.name();

    auto* w  = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Shared exchange/symbol bar ────────────────────────────────────────────
    auto* top_bar = new QWidget(w);
    top_bar->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;")
                               .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* top_hl = new QHBoxLayout(top_bar);
    top_hl->setContentsMargins(12, 8, 12, 8);
    top_hl->setSpacing(8);

    auto* exch_lbl = new QLabel("EXCHANGE", top_bar);
    exch_lbl->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; letter-spacing:0.5px;")
                                .arg(ui::colors::TEXT_TERTIARY()));
    auto* hft_exchange = new QComboBox(top_bar);
    hft_exchange->addItems({"binance", "kraken", "coinbase", "bybit", "okx", "hyperliquid"});
    hft_exchange->setStyleSheet(combo_ss());
    hft_exchange->setFixedWidth(110);
    combo_inputs_["hft_exchange"] = hft_exchange;

    auto* sym_lbl = new QLabel("SYMBOL", top_bar);
    sym_lbl->setStyleSheet(exch_lbl->styleSheet());
    auto* hft_symbol = new QLineEdit(top_bar);
    hft_symbol->setText("BTC/USDT");
    hft_symbol->setFixedWidth(100);
    hft_symbol->setStyleSheet(input_ss());
    hft_symbol->setToolTip("e.g. BTC/USDT, ETH/USDT");
    text_inputs_["hft_symbol"] = hft_symbol;

    auto* depth_lbl = new QLabel("DEPTH", top_bar);
    depth_lbl->setStyleSheet(exch_lbl->styleSheet());
    auto* hft_depth = new QComboBox(top_bar);
    hft_depth->addItems({"10", "20", "50", "100"});
    hft_depth->setCurrentIndex(1);
    hft_depth->setFixedWidth(60);
    hft_depth->setStyleSheet(combo_ss());
    combo_inputs_["hft_depth"] = hft_depth;

    top_hl->addWidget(exch_lbl);
    top_hl->addWidget(hft_exchange);
    top_hl->addWidget(sym_lbl);
    top_hl->addWidget(hft_symbol);
    top_hl->addWidget(depth_lbl);
    top_hl->addWidget(hft_depth);
    top_hl->addStretch();

    // Live latency badge
    auto* latency_lbl = new QLabel("LATENCY —", top_bar);
    latency_lbl->setObjectName("hftLatency");
    latency_lbl->setStyleSheet(QString("color:%1; font-size:9px; font-family:'Courier New'; background:transparent;")
                                   .arg(ui::colors::TEXT_TERTIARY()));
    top_hl->addWidget(latency_lbl);
    vl->addWidget(top_bar);

    // ── Tab widget ────────────────────────────────────────────────────────────
    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(tab_ss(accent));

    // ════════════════════════════════════════════════════════
    // TAB 1 — LIVE ORDER BOOK
    // ════════════════════════════════════════════════════════
    auto* ob_tab  = new QWidget(this);
    auto* ob_root = new QVBoxLayout(ob_tab);
    ob_root->setContentsMargins(12, 10, 12, 10);
    ob_root->setSpacing(8);

    // Controls row
    auto* ob_ctrl = new QHBoxLayout;
    ob_ctrl->setSpacing(8);
    auto* ob_fetch = make_run_button("FETCH LIVE ORDER BOOK", ob_tab);
    connect(ob_fetch, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Fetching live order book...");
        QJsonObject p;
        p["exchange"] = combo_inputs_["hft_exchange"]->currentText();
        p["symbol"]   = text_inputs_["hft_symbol"]->text().trimmed();
        p["depth"]    = combo_inputs_["hft_depth"]->currentText().toInt();
        AIQuantLabService::instance().hft_create_orderbook(p);
    });
    ob_ctrl->addWidget(ob_fetch);
    ob_ctrl->addStretch();
    ob_root->addLayout(ob_ctrl);

    // Metrics row — 5 cards in a grid
    auto* metrics_row = new QHBoxLayout;
    metrics_row->setSpacing(6);

    auto make_card = [&](const QString& label, QWidget* parent) -> QPair<QWidget*, QLabel*> {
        auto* card = new QWidget(parent);
        card->setStyleSheet(QString("background:%1; border:1px solid %2; border-radius:3px;")
                                .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
        auto* cvl = new QVBoxLayout(card);
        cvl->setContentsMargins(10, 6, 10, 6);
        cvl->setSpacing(2);
        auto* l = new QLabel(label, card);
        l->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; letter-spacing:0.5px; background:transparent;")
                             .arg(ui::colors::TEXT_TERTIARY()));
        auto* v = new QLabel("—", card);
        v->setObjectName("hftCardVal");
        v->setStyleSheet(QString("color:%1; font-size:13px; font-weight:700; font-family:'Courier New'; background:transparent;")
                             .arg(ui::colors::TEXT_PRIMARY()));
        cvl->addWidget(l);
        cvl->addWidget(v);
        return {card, v};
    };

    auto [mid_card, mid_val]         = make_card("MID PRICE", ob_tab);
    auto [spread_card, spread_val]   = make_card("SPREAD BPS", ob_tab);
    auto [obi_card, obi_val]         = make_card("ORDER BOOK IMBALANCE", ob_tab);
    auto [pressure_card, pres_val]   = make_card("PRESSURE", ob_tab);
    auto [wmid_card, wmid_val]       = make_card("WEIGHTED MID", ob_tab);

    // Store for result update
    mid_val->setObjectName("hft_mid_val");
    spread_val->setObjectName("hft_spread_val");
    obi_val->setObjectName("hft_obi_val");
    pres_val->setObjectName("hft_pressure_val");
    wmid_val->setObjectName("hft_wmid_val");

    metrics_row->addWidget(mid_card, 1);
    metrics_row->addWidget(spread_card, 1);
    metrics_row->addWidget(obi_card, 1);
    metrics_row->addWidget(pressure_card, 1);
    metrics_row->addWidget(wmid_card, 1);
    ob_root->addLayout(metrics_row);

    // Bid / Ask tables side by side
    auto* books_row = new QHBoxLayout;
    books_row->setSpacing(8);

    // Bids
    auto* bids_frame = new QWidget(ob_tab);
    bids_frame->setStyleSheet(QString("background:%1; border:1px solid %2; border-radius:3px;")
                                  .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* bids_vl = new QVBoxLayout(bids_frame);
    bids_vl->setContentsMargins(0, 0, 0, 0);
    bids_vl->setSpacing(0);
    auto* bids_hdr = new QLabel("  BIDS", bids_frame);
    bids_hdr->setFixedHeight(24);
    bids_hdr->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; letter-spacing:1px;"
                                    "background:%2; border-bottom:1px solid %3;")
                                .arg(ui::colors::POSITIVE(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* bid_table = new QTableWidget(10, 3, bids_frame);
    bid_table->setObjectName("hft_bid_table");
    bid_table->setHorizontalHeaderLabels({"Price", "Size", "Cumulative"});
    bid_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    bid_table->verticalHeader()->setVisible(false);
    bid_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    bid_table->setSelectionMode(QAbstractItemView::NoSelection);
    bid_table->setShowGrid(false);
    bid_table->setStyleSheet(
        QString("QTableWidget { background:%1; border:none; }"
                "QHeaderView::section { background:%2; color:%3; font-size:9px; font-weight:700;"
                "  padding:3px 6px; border:none; border-bottom:1px solid %4; }"
                "QTableWidget::item { padding:2px 6px; font-family:'Courier New'; font-size:10px; border:none; }")
            .arg(ui::colors::BG_RAISED(), ui::colors::BG_SURFACE(),
                 ui::colors::TEXT_TERTIARY(), ui::colors::BORDER_DIM()));
    bids_vl->addWidget(bids_hdr);
    bids_vl->addWidget(bid_table, 1);

    // Asks
    auto* asks_frame = new QWidget(ob_tab);
    asks_frame->setStyleSheet(bids_frame->styleSheet());
    auto* asks_vl = new QVBoxLayout(asks_frame);
    asks_vl->setContentsMargins(0, 0, 0, 0);
    asks_vl->setSpacing(0);
    auto* asks_hdr = new QLabel("  ASKS", asks_frame);
    asks_hdr->setFixedHeight(24);
    asks_hdr->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; letter-spacing:1px;"
                                    "background:%2; border-bottom:1px solid %3;")
                                .arg(ui::colors::NEGATIVE(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* ask_table = new QTableWidget(10, 3, asks_frame);
    ask_table->setObjectName("hft_ask_table");
    ask_table->setHorizontalHeaderLabels({"Price", "Size", "Cumulative"});
    ask_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ask_table->verticalHeader()->setVisible(false);
    ask_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ask_table->setSelectionMode(QAbstractItemView::NoSelection);
    ask_table->setShowGrid(false);
    ask_table->setStyleSheet(bid_table->styleSheet());
    asks_vl->addWidget(asks_hdr);
    asks_vl->addWidget(ask_table, 1);

    books_row->addWidget(bids_frame, 1);
    books_row->addWidget(asks_frame, 1);
    ob_root->addLayout(books_row, 1);
    tabs->addTab(ob_tab, "Live Order Book");

    // ════════════════════════════════════════════════════════
    // TAB 2 — MICROSTRUCTURE (Market Making + Toxic Flow)
    // ════════════════════════════════════════════════════════
    auto* micro_tab  = new QWidget(this);
    auto* micro_root = new QVBoxLayout(micro_tab);
    micro_root->setContentsMargins(12, 10, 12, 10);
    micro_root->setSpacing(10);

    // ─ Market Making section ─
    auto* mm_section = new QWidget(micro_tab);
    mm_section->setStyleSheet(QString("background:%1; border:1px solid %2; border-radius:3px;")
                                  .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* mm_vl = new QVBoxLayout(mm_section);
    mm_vl->setContentsMargins(12, 10, 12, 10);
    mm_vl->setSpacing(8);

    auto* mm_title = new QLabel("MARKET MAKING  —  Avellaneda-Stoikov Model", mm_section);
    mm_title->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; letter-spacing:0.5px; background:transparent;")
                                .arg(accent));
    mm_vl->addWidget(mm_title);

    auto* mm_params = new QHBoxLayout;
    mm_params->setSpacing(8);
    auto* inv_spin = make_double_spin(-1000, 1000, 0.0, 4, " units", mm_section);
    int_inputs_["hft_inventory"] = nullptr;
    double_inputs_["hft_inventory_d"] = inv_spin;
    mm_params->addWidget(build_input_row("Inventory", inv_spin, mm_section));
    auto* spread_spin = make_double_spin(1.0, 10.0, 1.5, 2, "×", mm_section);
    double_inputs_["hft_spread_mult"] = spread_spin;
    mm_params->addWidget(build_input_row("Spread Mult", spread_spin, mm_section));
    auto* risk_spin = make_double_spin(0.001, 0.1, 0.01, 3, "", mm_section);
    double_inputs_["hft_risk_aversion"] = risk_spin;
    mm_params->addWidget(build_input_row("Risk Aversion", risk_spin, mm_section));
    mm_vl->addLayout(mm_params);

    auto* mm_run = make_run_button("CALCULATE OPTIMAL QUOTES", mm_section);
    connect(mm_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Fetching live data + computing quotes...");
        QJsonObject p;
        p["exchange"]         = combo_inputs_["hft_exchange"]->currentText();
        p["symbol"]           = text_inputs_["hft_symbol"]->text().trimmed();
        p["inventory"]        = double_inputs_["hft_inventory_d"]->value();
        p["spread_multiplier"] = double_inputs_["hft_spread_mult"]->value();
        p["risk_aversion"]    = double_inputs_["hft_risk_aversion"]->value();
        AIQuantLabService::instance().hft_market_making_quotes(p);
    });
    mm_vl->addWidget(mm_run);

    // MM result cards
    auto* mm_results = new QHBoxLayout;
    mm_results->setSpacing(6);
    auto [bid_card, bid_val2] = make_card("BID QUOTE", mm_section);
    auto [ask_card, ask_val2] = make_card("ASK QUOTE", mm_section);
    auto [qs_card, qs_val]    = make_card("QUOTED SPREAD BPS", mm_section);
    auto [edge_card, edge_val] = make_card("EDGE/SIDE BPS", mm_section);
    auto [rec_card, rec_val]  = make_card("RECOMMENDATION", mm_section);
    bid_val2->setObjectName("hft_mm_bid");
    ask_val2->setObjectName("hft_mm_ask");
    qs_val->setObjectName("hft_mm_qspread");
    edge_val->setObjectName("hft_mm_edge");
    rec_val->setObjectName("hft_mm_rec");
    mm_results->addWidget(bid_card, 1);
    mm_results->addWidget(ask_card, 1);
    mm_results->addWidget(qs_card, 1);
    mm_results->addWidget(edge_card, 1);
    mm_results->addWidget(rec_card, 1);
    mm_vl->addLayout(mm_results);
    micro_root->addWidget(mm_section);

    // ─ Toxic Flow section ─
    auto* tox_section = new QWidget(micro_tab);
    tox_section->setStyleSheet(mm_section->styleSheet());
    auto* tox_vl = new QVBoxLayout(tox_section);
    tox_vl->setContentsMargins(12, 10, 12, 10);
    tox_vl->setSpacing(8);

    auto* tox_title = new QLabel("TOXIC FLOW DETECTION  —  PIN Score Model", tox_section);
    tox_title->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; letter-spacing:0.5px; background:transparent;")
                                 .arg(accent));
    tox_vl->addWidget(tox_title);

    auto* tox_params = new QHBoxLayout;
    tox_params->setSpacing(8);
    auto* tox_limit = new QComboBox(tox_section);
    tox_limit->addItems({"50", "100", "200", "500"});
    tox_limit->setCurrentIndex(2);
    tox_limit->setStyleSheet(combo_ss());
    combo_inputs_["hft_tox_limit"] = tox_limit;
    tox_params->addWidget(build_input_row("Trade History", tox_limit, tox_section));
    tox_params->addStretch();
    tox_vl->addLayout(tox_params);

    auto* tox_run = make_run_button("DETECT TOXIC FLOW", tox_section);
    connect(tox_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Fetching trades + analyzing flow...");
        QJsonObject p;
        p["exchange"] = combo_inputs_["hft_exchange"]->currentText();
        p["symbol"]   = text_inputs_["hft_symbol"]->text().trimmed();
        p["limit"]    = combo_inputs_["hft_tox_limit"]->currentText().toInt();
        AIQuantLabService::instance().hft_detect_toxic(p);
    });
    tox_vl->addWidget(tox_run);

    auto* tox_results = new QHBoxLayout;
    tox_results->setSpacing(6);
    auto [pin_card, pin_val]      = make_card("PIN SCORE (0-100)", tox_section);
    auto [vol_card, vol_val]      = make_card("VOLUME IMBALANCE", tox_section);
    auto [impact_card, impact_val] = make_card("PRICE IMPACT BPS", tox_section);
    auto [class_card, class_val]  = make_card("CLASSIFICATION", tox_section);
    auto [action_card, action_val] = make_card("RECOMMENDED ACTION", tox_section);
    pin_val->setObjectName("hft_tox_pin");
    vol_val->setObjectName("hft_tox_vol");
    impact_val->setObjectName("hft_tox_impact");
    class_val->setObjectName("hft_tox_class");
    action_val->setObjectName("hft_tox_action");
    tox_results->addWidget(pin_card, 1);
    tox_results->addWidget(vol_card, 1);
    tox_results->addWidget(impact_card, 1);
    tox_results->addWidget(class_card, 1);
    tox_results->addWidget(action_card, 1);
    tox_vl->addLayout(tox_results);
    micro_root->addWidget(tox_section);
    micro_root->addStretch();
    tabs->addTab(micro_tab, "Microstructure");

    // ════════════════════════════════════════════════════════
    // TAB 3 — SLIPPAGE ESTIMATOR
    // ════════════════════════════════════════════════════════
    auto* slip_tab  = new QWidget(this);
    auto* slip_root = new QVBoxLayout(slip_tab);
    slip_root->setContentsMargins(12, 10, 12, 10);
    slip_root->setSpacing(10);

    auto* slip_section = new QWidget(slip_tab);
    slip_section->setStyleSheet(QString("background:%1; border:1px solid %2; border-radius:3px;")
                                    .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* slip_vl = new QVBoxLayout(slip_section);
    slip_vl->setContentsMargins(12, 10, 12, 10);
    slip_vl->setSpacing(8);

    auto* slip_title = new QLabel("SLIPPAGE ESTIMATOR  —  Real Order Book Walk", slip_section);
    slip_title->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; letter-spacing:0.5px; background:transparent;")
                                  .arg(accent));
    slip_vl->addWidget(slip_title);

    auto* slip_desc = new QLabel(
        "Walks the live order book level-by-level to compute actual fill price and slippage for a given order size.", slip_section);
    slip_desc->setWordWrap(true);
    slip_desc->setStyleSheet(QString("color:%1; font-size:10px; background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
    slip_vl->addWidget(slip_desc);

    auto* slip_params = new QHBoxLayout;
    slip_params->setSpacing(8);
    auto* slip_side = new QComboBox(slip_section);
    slip_side->addItems({"buy", "sell"});
    slip_side->setStyleSheet(combo_ss());
    combo_inputs_["hft_slip_side"] = slip_side;
    slip_params->addWidget(build_input_row("Side", slip_side, slip_section));
    auto* slip_qty = make_double_spin(0.0001, 1'000'000, 1.0, 6, "", slip_section);
    double_inputs_["hft_slip_qty"] = slip_qty;
    slip_params->addWidget(build_input_row("Quantity", slip_qty, slip_section));
    slip_vl->addLayout(slip_params);

    auto* slip_run = make_run_button("ESTIMATE SLIPPAGE", slip_section);
    connect(slip_run, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Walking order book...");
        QJsonObject p;
        p["exchange"] = combo_inputs_["hft_exchange"]->currentText();
        p["symbol"]   = text_inputs_["hft_symbol"]->text().trimmed();
        p["side"]     = combo_inputs_["hft_slip_side"]->currentText();
        p["quantity"] = double_inputs_["hft_slip_qty"]->value();
        AIQuantLabService::instance().hft_execute_order(p);
    });
    slip_vl->addWidget(slip_run);

    // Slippage result cards
    auto* slip_results = new QHBoxLayout;
    slip_results->setSpacing(6);
    auto [avgp_card, avgp_val]     = make_card("AVG FILL PRICE", slip_section);
    auto [slbps_card, slbps_val]   = make_card("SLIPPAGE BPS", slip_section);
    auto [cost_card, cost_val]     = make_card("TOTAL COST", slip_section);
    auto [fills_card, fills_val]   = make_card("FILL LEVELS", slip_section);
    auto [viable_card, viable_val] = make_card("VIABILITY", slip_section);
    avgp_val->setObjectName("hft_slip_avgp");
    slbps_val->setObjectName("hft_slip_bps");
    cost_val->setObjectName("hft_slip_cost");
    fills_val->setObjectName("hft_slip_fills");
    viable_val->setObjectName("hft_slip_viable");
    slip_results->addWidget(avgp_card, 1);
    slip_results->addWidget(slbps_card, 1);
    slip_results->addWidget(cost_card, 1);
    slip_results->addWidget(fills_card, 1);
    slip_results->addWidget(viable_card, 1);
    slip_vl->addLayout(slip_results);

    // Fills table
    auto* fills_table = new QTableWidget(0, 3, slip_section);
    fills_table->setObjectName("hft_slip_table");
    fills_table->setHorizontalHeaderLabels({"Fill Price", "Quantity", "Cost"});
    fills_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    fills_table->verticalHeader()->setVisible(false);
    fills_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    fills_table->setSelectionMode(QAbstractItemView::NoSelection);
    fills_table->setShowGrid(false);
    fills_table->setMaximumHeight(160);
    fills_table->setStyleSheet(
        QString("QTableWidget { background:%1; border:1px solid %2; }"
                "QHeaderView::section { background:%3; color:%4; font-size:9px; font-weight:700;"
                "  padding:3px 6px; border:none; border-bottom:1px solid %2; }"
                "QTableWidget::item { padding:2px 6px; font-family:'Courier New'; font-size:10px; }")
            .arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM(),
                 ui::colors::BG_SURFACE(), ui::colors::TEXT_TERTIARY()));
    slip_vl->addWidget(fills_table);

    slip_root->addWidget(slip_section);
    slip_root->addStretch();
    tabs->addTab(slip_tab, "Slippage Estimator");

    // ── Full Analyze button (bottom bar) ─────────────────────────────────────
    auto* bottom_bar = new QWidget(w);
    bottom_bar->setStyleSheet(QString("background:%1; border-top:1px solid %2;")
                                  .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* bot_hl = new QHBoxLayout(bottom_bar);
    bot_hl->setContentsMargins(12, 6, 12, 6);
    auto* analyze_btn = make_run_button("⚡ FULL ANALYSIS — FETCH ALL & COMPUTE", bottom_bar);
    analyze_btn->setToolTip("Fetches live order book + trades, computes book metrics, market making quotes, toxic flow, and slippage in one call");
    connect(analyze_btn, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Running full microstructure analysis...");
        QJsonObject p;
        p["exchange"]          = combo_inputs_["hft_exchange"]->currentText();
        p["symbol"]            = text_inputs_["hft_symbol"]->text().trimmed();
        p["depth"]             = combo_inputs_["hft_depth"]->currentText().toInt();
        p["limit"]             = combo_inputs_["hft_tox_limit"]->currentText().toInt();
        p["inventory"]         = double_inputs_["hft_inventory_d"]->value();
        p["spread_multiplier"] = double_inputs_["hft_spread_mult"]->value();
        p["quantity"]          = double_inputs_["hft_slip_qty"]->value();
        AIQuantLabService::instance().hft_snapshot(p);
    });
    bot_hl->addWidget(analyze_btn, 1);

    vl->addWidget(tabs, 1);
    vl->addWidget(bottom_bar);

    results_layout_ = new QVBoxLayout;
    vl->addLayout(results_layout_);
    return w;
}

} // namespace fincept::screens
