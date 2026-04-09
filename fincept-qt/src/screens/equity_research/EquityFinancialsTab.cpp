// src/screens/equity_research/EquityFinancialsTab.cpp
#include "screens/equity_research/EquityFinancialsTab.h"

#include "services/equity/EquityResearchService.h"
#include "services/file_manager/FileManagerService.h"
#include "ui/theme/Theme.h"

#include <QBarCategoryAxis>
#include <QBarSeries>
#include <QBarSet>
#include <QChart>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLineSeries>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSizePolicy>
#include <QTextStream>
#include <QVBoxLayout>
#include <QValueAxis>

namespace fincept::screens {

// ── Static helpers ────────────────────────────────────────────────────────────

namespace {

static const QString kAmber = "#f59e0b";
static const QString kCyan = "#22d3ee";
static const QString kGreen = ui::colors::POSITIVE;
static const QString kRed = ui::colors::NEGATIVE;
static const QString kBlue = "#3b82f6";
static const QString kPurple = "#a855f7";
static const QString kOrange = "#f97316";
static const QString kYellow = "#eab308";

QFrame* section_frame(const QString& title, const QString& color) {
    auto* f = new QFrame;
    f->setStyleSheet(QString("QFrame { background:%1; border:1px solid %2; border-radius:4px; }")
                         .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
    auto* vl = new QVBoxLayout(f);
    vl->setContentsMargins(10, 8, 10, 10);
    vl->setSpacing(8);

    auto* hdr = new QHBoxLayout;
    hdr->setSpacing(6);
    auto* title_lbl = new QLabel(title);
    title_lbl->setStyleSheet(QString("color:%1; font-size:11px; font-weight:700; "
                                     "letter-spacing:1px; background:transparent; border:0;")
                                 .arg(color));
    hdr->addWidget(title_lbl);
    hdr->addStretch();
    vl->addLayout(hdr);

    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(
        QString("border:0; border-top:1px solid %1; background:transparent;").arg(ui::colors::BORDER_DIM));
    vl->addWidget(sep);
    return f;
}

// Large metric card: label on top, big value, optional subtitle
QWidget* metric_card(const QString& label, QLabel*& val_out, QLabel*& sub_out, const QString& val_color,
                     const QString& initial_val = "—", const QString& initial_sub = {}) {
    auto* f = new QFrame;
    f->setStyleSheet(QString("QFrame { background:%1; border:1px solid %2; border-radius:4px; }")
                         .arg(ui::colors::BG_BASE, ui::colors::BORDER_DIM));
    auto* vl = new QVBoxLayout(f);
    vl->setContentsMargins(8, 6, 8, 6);
    vl->setSpacing(2);

    auto* lbl = new QLabel(label);
    lbl->setStyleSheet(QString("color:%1; font-size:9px; font-weight:600; letter-spacing:1px; "
                               "background:transparent; border:0;")
                           .arg(ui::colors::TEXT_SECONDARY));
    vl->addWidget(lbl);

    val_out = new QLabel(initial_val);
    val_out->setStyleSheet(QString("color:%1; font-size:14px; font-weight:700; "
                                   "background:transparent; border:0;")
                               .arg(val_color));
    vl->addWidget(val_out);

    if (!initial_sub.isNull()) {
        sub_out = new QLabel(initial_sub.isEmpty() ? "" : initial_sub);
        sub_out->setStyleSheet(
            QString("color:%1; font-size:9px; background:transparent; border:0;").arg(ui::colors::TEXT_TERTIARY));
        vl->addWidget(sub_out);
    } else {
        sub_out = nullptr;
    }
    return f;
}

// Small ratio row
QLabel* ratio_row(QWidget* parent_vl_owner, const QString& label, const QString& color) {
    auto* hl = new QHBoxLayout;
    hl->setSpacing(4);
    hl->setContentsMargins(0, 0, 0, 0);
    auto* k = new QLabel(label);
    k->setStyleSheet(
        QString("color:%1; font-size:10px; background:transparent; border:0;").arg(ui::colors::TEXT_SECONDARY));
    auto* v = new QLabel("—");
    v->setStyleSheet(
        QString("color:%1; font-size:10px; font-weight:600; background:transparent; border:0;").arg(color));
    v->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    hl->addWidget(k);
    hl->addStretch();
    hl->addWidget(v);
    static_cast<QVBoxLayout*>(parent_vl_owner->layout())->addLayout(hl);
    return v;
}

QTableWidget* make_table() {
    auto* t = new QTableWidget;
    t->setAlternatingRowColors(true);
    t->setStyleSheet(QString(R"(
        QTableWidget {
            background:%1; alternate-background-color:%2;
            gridline-color:%3; color:%4; border:0; font-size:10px;
        }
        QHeaderView::section {
            background:%5; color:%6; font-size:9px; font-weight:700;
            padding:4px; border:0; border-bottom:1px solid %3;
        }
        QTableWidget::item { padding:2px 6px; }
    )")
                         .arg(ui::colors::BG_SURFACE, ui::colors::BG_BASE, ui::colors::BORDER_DIM,
                              ui::colors::TEXT_PRIMARY, ui::colors::BG_RAISED, ui::colors::TEXT_SECONDARY));
    t->horizontalHeader()->setStretchLastSection(false);
    t->verticalHeader()->setDefaultSectionSize(24);
    t->verticalHeader()->hide();
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    return t;
}

QChartView* make_chart_view(int fixed_height = 0) {
    auto* cv = new QChartView;
    cv->setRenderHint(QPainter::Antialiasing, false);
    cv->setStyleSheet("background:transparent; border:0;");
    if (fixed_height > 0)
        cv->setFixedHeight(fixed_height);
    return cv;
}

} // anonymous namespace

// ── Constructor ───────────────────────────────────────────────────────────────

EquityFinancialsTab::EquityFinancialsTab(QWidget* parent) : QWidget(parent) {
    build_ui();
    auto& svc = services::equity::EquityResearchService::instance();
    connect(&svc, &services::equity::EquityResearchService::financials_loaded, this,
            &EquityFinancialsTab::on_financials_loaded);
}

void EquityFinancialsTab::set_symbol(const QString& symbol) {
    if (symbol == current_symbol_)
        return;
    current_symbol_ = symbol;
    loaded_ = false;
    loading_overlay_->show_loading("LOADING FINANCIALS…");
    services::equity::EquityResearchService::instance().fetch_financials(symbol);
}

// ── Build UI ──────────────────────────────────────────────────────────────────

void EquityFinancialsTab::build_ui() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    loading_overlay_ = new ui::LoadingOverlay(this);
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Statement selector ────────────────────────────────────────────────────
    auto* btn_bar = new QWidget(this);
    btn_bar->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
    auto* btn_hl = new QHBoxLayout(btn_bar);
    btn_hl->setContentsMargins(10, 6, 10, 6);
    btn_hl->setSpacing(6);

    QString btn_style =
        QString(R"(
        QPushButton {
            background:transparent; color:%1; border:1px solid %2;
            border-radius:3px; padding:4px 14px; font-size:11px; font-weight:700;
        }
        QPushButton:checked { background:%3; color:%4; border-color:%3; }
        QPushButton:hover:!checked { border-color:%3; background:%5; }
    )")
            .arg(ui::colors::TEXT_SECONDARY, ui::colors::BORDER_DIM, kAmber, ui::colors::BG_BASE, ui::colors::BG_HOVER);

    auto make_btn = [&](const QString& label, QPushButton*& out) {
        out = new QPushButton(label);
        out->setCheckable(true);
        out->setStyleSheet(btn_style);
        btn_hl->addWidget(out);
    };
    make_btn("Income Statement", btn_income_);
    make_btn("Balance Sheet", btn_balance_);
    make_btn("Cash Flow", btn_cashflow_);
    btn_income_->setChecked(true);
    btn_hl->addStretch();

    auto* export_btn = new QPushButton("EXPORT CSV");
    export_btn->setStyleSheet(QString(R"(
        QPushButton {
            background:transparent; color:%1; border:1px solid %1;
            border-radius:3px; padding:4px 14px; font-size:11px; font-weight:700;
        }
        QPushButton:hover { background:%1; color:#000; }
    )")
                                  .arg(kAmber));
    btn_hl->addWidget(export_btn);

    connect(export_btn, &QPushButton::clicked, this, [this]() {
        int idx = stack_ ? stack_->currentIndex() : 0;
        QTableWidget* tbl = nullptr;
        QString label;
        if (idx == 0) {
            tbl = inc_table_;
            label = "income";
        } else if (idx == 1) {
            tbl = bal_table_;
            label = "balance";
        } else {
            tbl = cf_table_;
            label = "cashflow";
        }
        if (!tbl)
            return;

        // Build sanitised ticker prefix from object name or use generic
        QString ticker = objectName().isEmpty() ? "equity" : objectName();
        ticker.remove(QRegularExpression(R"([^A-Za-z0-9_\-])"));

        QString filename =
            QString("%1_%2_%3.csv").arg(ticker, label, QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
        QString dest = services::FileManagerService::instance().storage_dir() + "/" + filename;

        QFile f(dest);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
            return;
        QTextStream out(&f);

        // Header row
        QStringList headers;
        for (int c = 0; c < tbl->columnCount(); ++c) {
            auto* item = tbl->horizontalHeaderItem(c);
            headers << (item ? item->text() : QString("Col%1").arg(c));
        }
        out << headers.join(",") << "\n";

        // Data rows
        for (int r = 0; r < tbl->rowCount(); ++r) {
            QStringList row;
            for (int c = 0; c < tbl->columnCount(); ++c) {
                auto* item = tbl->item(r, c);
                QString cell = item ? item->text() : "";
                if (cell.contains(',') || cell.contains('"') || cell.contains('\n'))
                    cell = "\"" + cell.replace("\"", "\"\"") + "\"";
                row << cell;
            }
            out << row.join(",") << "\n";
        }
        f.close();

        services::FileManagerService::instance().register_file(dest, QFileInfo(dest).fileName(), QFileInfo(dest).size(),
                                                               "text/csv", "equity_research");
    });

    vl->addWidget(btn_bar);

    auto switch_to = [this](int idx) {
        btn_income_->setChecked(idx == 0);
        btn_balance_->setChecked(idx == 1);
        btn_cashflow_->setChecked(idx == 2);
        stack_->setCurrentIndex(idx);
    };
    connect(btn_income_, &QPushButton::clicked, this, [switch_to]() { switch_to(0); });
    connect(btn_balance_, &QPushButton::clicked, this, [switch_to]() { switch_to(1); });
    connect(btn_cashflow_, &QPushButton::clicked, this, [switch_to]() { switch_to(2); });

    // ── Stacked views ─────────────────────────────────────────────────────────
    stack_ = new QStackedWidget;
    stack_->addWidget(build_income_view());
    stack_->addWidget(build_balance_view());
    stack_->addWidget(build_cashflow_view());
    vl->addWidget(stack_, 1);
}

// ── Income View ───────────────────────────────────────────────────────────────

QWidget* EquityFinancialsTab::build_income_view() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("background:transparent; border:0;");

    auto* content = new QWidget(this);
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(10, 10, 10, 10);
    vl->setSpacing(10);

    // ── Key Metrics ───────────────────────────────────────────────────────────
    {
        auto* sec = section_frame("KEY FINANCIAL METRICS", kOrange);
        auto* svl = static_cast<QVBoxLayout*>(sec->layout());

        // Row 1: 4 large cards
        auto* r1 = new QHBoxLayout;
        r1->setSpacing(8);
        r1->addWidget(metric_card("TOTAL REVENUE", inc_revenue_val_, inc_revenue_sub_, kGreen));
        r1->addWidget(metric_card("GROSS PROFIT", inc_gross_val_, inc_gross_sub_, kCyan));
        r1->addWidget(metric_card("OPERATING INCOME", inc_opincome_val_, inc_opincome_sub_, kBlue));
        r1->addWidget(metric_card("NET INCOME", inc_netincome_val_, inc_netincome_sub_, kGreen));
        svl->addLayout(r1);

        // Row 2: EBITDA + margins
        auto* r2 = new QHBoxLayout;
        r2->setSpacing(8);
        r2->addWidget(metric_card("EBITDA", inc_ebitda_val_, inc_ebitda_sub_, kPurple));

        // Margin cards (no subtitle needed)
        QLabel* dummy = nullptr;
        auto* gm = metric_card("GROSS MARGIN", inc_gross_margin_, dummy, kGreen, "—", QString());
        auto* om = metric_card("OPERATING MARGIN", inc_op_margin_, dummy, kCyan, "—", QString());
        auto* nm = metric_card("NET MARGIN", inc_net_margin_, dummy, kOrange, "—", QString());
        auto* em = metric_card("EBITDA MARGIN", inc_ebitda_margin_, dummy, kPurple, "—", QString());
        r2->addWidget(gm);
        r2->addWidget(om);
        r2->addWidget(nm);
        r2->addWidget(em);
        svl->addLayout(r2);

        vl->addWidget(sec);
    }

    // ── Revenue & Earnings Chart ───────────────────────────────────────────────
    {
        auto* sec = section_frame("REVENUE & EARNINGS TREND", kBlue);
        inc_revenue_chart_ = make_chart_view(200);
        static_cast<QVBoxLayout*>(sec->layout())->addWidget(inc_revenue_chart_);
        vl->addWidget(sec);
    }

    // ── Margin Trend Chart ────────────────────────────────────────────────────
    {
        auto* sec = section_frame("MARGIN TRENDS (%)", kGreen);
        inc_margin_chart_ = make_chart_view(180);
        static_cast<QVBoxLayout*>(sec->layout())->addWidget(inc_margin_chart_);
        vl->addWidget(sec);
    }

    // ── DuPont & Return Metrics ───────────────────────────────────────────────
    {
        auto* sec = section_frame("RETURN METRICS & DUPONT ANALYSIS", kCyan);
        auto* svl = static_cast<QVBoxLayout*>(sec->layout());
        auto* hl = new QHBoxLayout;
        hl->setSpacing(10);

        // Return metrics column
        auto* ret_frame = new QFrame;
        ret_frame->setStyleSheet("QFrame { background:transparent; border:0; }");
        auto* rvl = new QVBoxLayout(ret_frame);
        rvl->setContentsMargins(0, 0, 0, 0);
        rvl->setSpacing(6);
        auto* ret_title = new QLabel("RETURN METRICS");
        ret_title->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; "
                                         "background:transparent; border:0;")
                                     .arg(kCyan));
        rvl->addWidget(ret_title);
        ret_roe_val_ = ratio_row(ret_frame, "Return on Equity (ROE)", kCyan);
        ret_roa_val_ = ratio_row(ret_frame, "Return on Assets (ROA)", kCyan);
        ret_roic_val_ = ratio_row(ret_frame, "Return on Inv. Capital", kBlue);
        ret_roce_val_ = ratio_row(ret_frame, "Return on Cap. Employed", kBlue);
        rvl->addStretch();
        hl->addWidget(ret_frame, 1);

        // DuPont column
        auto* dp_frame = new QFrame;
        dp_frame->setStyleSheet(QString("QFrame { background:%1; border:1px solid %2; border-radius:4px; }")
                                    .arg(ui::colors::BG_BASE, ui::colors::BORDER_DIM));
        auto* dvl = new QVBoxLayout(dp_frame);
        dvl->setContentsMargins(10, 8, 10, 8);
        dvl->setSpacing(6);
        auto* dp_title = new QLabel("DUPONT ANALYSIS");
        dp_title->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; "
                                        "background:transparent; border:0;")
                                    .arg(kPurple));
        dvl->addWidget(dp_title);

        // Net Margin × Asset Turnover × Equity Multiplier = ROE
        auto* formula_hl = new QHBoxLayout;
        formula_hl->setSpacing(4);
        auto make_dp_val = [&](QLabel*& out, const QString& label_text) {
            auto* col = new QVBoxLayout;
            out = new QLabel("—");
            out->setStyleSheet(QString("color:%1; font-size:14px; font-weight:700; "
                                       "background:transparent; border:0;")
                                   .arg(kGreen));
            out->setAlignment(Qt::AlignCenter);
            auto* lbl = new QLabel(label_text);
            lbl->setStyleSheet(
                QString("color:%1; font-size:9px; background:transparent; border:0;").arg(ui::colors::TEXT_TERTIARY));
            lbl->setAlignment(Qt::AlignCenter);
            col->addWidget(out);
            col->addWidget(lbl);
            formula_hl->addLayout(col);
        };
        auto make_op = [&](const QString& sym) {
            auto* op = new QLabel(sym);
            op->setStyleSheet(
                QString("color:%1; font-size:16px; background:transparent; border:0;").arg(ui::colors::TEXT_TERTIARY));
            op->setAlignment(Qt::AlignCenter);
            formula_hl->addWidget(op);
        };
        make_dp_val(dupont_net_margin_, "Net Margin");
        make_op("×");
        make_dp_val(dupont_asset_turn_, "Asset Turn");
        make_op("×");
        make_dp_val(dupont_eq_mult_, "Eq. Mult");
        make_op("=");
        make_dp_val(dupont_roe_result_, "ROE");
        dupont_roe_result_->setStyleSheet(QString("color:%1; font-size:16px; font-weight:700; "
                                                  "background:transparent; border:0;")
                                              .arg(kPurple));
        dvl->addLayout(formula_hl);
        hl->addWidget(dp_frame, 1);

        // Return trend chart
        ret_chart_ = make_chart_view(160);
        hl->addWidget(ret_chart_, 1);

        svl->addLayout(hl);
        vl->addWidget(sec);
    }

    // ── Raw table ─────────────────────────────────────────────────────────────
    {
        auto* sec = section_frame("INCOME STATEMENT", kAmber);
        inc_table_ = make_table();
        inc_table_->setFixedHeight(300);
        static_cast<QVBoxLayout*>(sec->layout())->addWidget(inc_table_);
        vl->addWidget(sec);
    }

    vl->addStretch();
    scroll->setWidget(content);
    return scroll;
}

// ── Balance View ──────────────────────────────────────────────────────────────

QWidget* EquityFinancialsTab::build_balance_view() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("background:transparent; border:0;");

    auto* content = new QWidget(this);
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(10, 10, 10, 10);
    vl->setSpacing(10);

    // ── Key Balance Sheet Metrics ─────────────────────────────────────────────
    {
        auto* sec = section_frame("BALANCE SHEET OVERVIEW", kBlue);
        auto* svl = static_cast<QVBoxLayout*>(sec->layout());
        auto* r1 = new QHBoxLayout;
        r1->setSpacing(8);
        QLabel* dummy = nullptr;
        r1->addWidget(metric_card("TOTAL ASSETS", bal_assets_val_, dummy, kBlue, "—", QString()));
        r1->addWidget(metric_card("TOTAL LIABILITIES", bal_liabilities_val_, dummy, kRed, "—", QString()));
        r1->addWidget(metric_card("STOCKHOLDERS EQUITY", bal_equity_val_, dummy, kGreen, "—", QString()));
        r1->addWidget(metric_card("TOTAL DEBT", bal_debt_val_, dummy, kOrange, "—", QString()));
        r1->addWidget(metric_card("CASH & EQUIV.", bal_cash_val_, dummy, kCyan, "—", QString()));
        svl->addLayout(r1);
        vl->addWidget(sec);
    }

    // ── Balance Trend Chart ───────────────────────────────────────────────────
    {
        auto* sec = section_frame("ASSETS, LIABILITIES & EQUITY TREND", kBlue);
        bal_chart_ = make_chart_view(200);
        static_cast<QVBoxLayout*>(sec->layout())->addWidget(bal_chart_);
        vl->addWidget(sec);
    }

    // ── Liquidity & Leverage Ratios ───────────────────────────────────────────
    {
        auto* sec = section_frame("LIQUIDITY & LEVERAGE RATIOS", kYellow);
        auto* svl = static_cast<QVBoxLayout*>(sec->layout());
        auto* hl = new QHBoxLayout;
        hl->setSpacing(20);

        // Liquidity
        auto* liq = new QFrame;
        liq->setStyleSheet("QFrame { background:transparent; border:0; }");
        auto* lvl = new QVBoxLayout(liq);
        lvl->setContentsMargins(0, 0, 0, 0);
        lvl->setSpacing(4);
        auto* lt = new QLabel("LIQUIDITY");
        lt->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; "
                                  "background:transparent; border:0;")
                              .arg(kCyan));
        lvl->addWidget(lt);
        bal_current_ratio_ = ratio_row(liq, "Current Ratio", kCyan);
        bal_quick_ratio_ = ratio_row(liq, "Quick Ratio", kCyan);
        bal_working_cap_ = ratio_row(liq, "Working Capital", kGreen);
        lvl->addStretch();
        hl->addWidget(liq, 1);

        // Leverage
        auto* lev = new QFrame;
        lev->setStyleSheet("QFrame { background:transparent; border:0; }");
        auto* evl = new QVBoxLayout(lev);
        evl->setContentsMargins(0, 0, 0, 0);
        evl->setSpacing(4);
        auto* et = new QLabel("LEVERAGE / SOLVENCY");
        et->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; "
                                  "background:transparent; border:0;")
                              .arg(kOrange));
        evl->addWidget(et);
        bal_debt_equity_ = ratio_row(lev, "Debt / Equity", kOrange);
        bal_debt_assets_ = ratio_row(lev, "Debt / Assets", kOrange);
        bal_int_coverage_ = ratio_row(lev, "Interest Coverage", kYellow);
        evl->addStretch();
        hl->addWidget(lev, 1);

        svl->addLayout(hl);
        vl->addWidget(sec);
    }

    // ── Raw table ─────────────────────────────────────────────────────────────
    {
        auto* sec = section_frame("BALANCE SHEET", kAmber);
        bal_table_ = make_table();
        bal_table_->setFixedHeight(300);
        static_cast<QVBoxLayout*>(sec->layout())->addWidget(bal_table_);
        vl->addWidget(sec);
    }

    vl->addStretch();
    scroll->setWidget(content);
    return scroll;
}

// ── Cash Flow View ────────────────────────────────────────────────────────────

QWidget* EquityFinancialsTab::build_cashflow_view() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("background:transparent; border:0;");

    auto* content = new QWidget(this);
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(10, 10, 10, 10);
    vl->setSpacing(10);

    // ── Key CF Metrics ────────────────────────────────────────────────────────
    {
        auto* sec = section_frame("CASH FLOW OVERVIEW", kGreen);
        auto* svl = static_cast<QVBoxLayout*>(sec->layout());
        auto* r1 = new QHBoxLayout;
        r1->setSpacing(8);
        QLabel* dummy1 = nullptr;
        r1->addWidget(metric_card("OPERATING CF", cf_operating_val_, dummy1, kGreen));
        r1->addWidget(metric_card("INVESTING CF", cf_investing_val_, dummy1, kOrange));
        r1->addWidget(metric_card("FINANCING CF", cf_financing_val_, dummy1, kPurple));
        r1->addWidget(metric_card("FREE CASH FLOW", cf_fcf_val_, cf_fcf_sub_, kCyan));
        svl->addLayout(r1);

        auto* r2 = new QHBoxLayout;
        r2->setSpacing(8);
        QLabel* dummy = nullptr;
        r2->addWidget(metric_card("CAPEX", cf_capex_val_, dummy, kRed, "—", QString()));
        r2->addWidget(metric_card("DIVIDENDS PAID", cf_dividends_val_, dummy, kYellow, "—", QString()));
        r2->addWidget(metric_card("STOCK BUYBACKS", cf_buybacks_val_, dummy, kBlue, "—", QString()));
        r2->addWidget(metric_card("FCF MARGIN", cf_fcf_margin_, dummy, kCyan, "—", QString()));
        r2->addWidget(metric_card("CAPEX/REVENUE", cf_capex_rev_, dummy, kOrange, "—", QString()));
        svl->addLayout(r2);
        vl->addWidget(sec);
    }

    // ── Cash Flow Trend Chart ─────────────────────────────────────────────────
    {
        auto* sec = section_frame("CASH FLOW TREND (BILLIONS $)", kGreen);
        cf_chart_ = make_chart_view(200);
        static_cast<QVBoxLayout*>(sec->layout())->addWidget(cf_chart_);
        vl->addWidget(sec);
    }

    // ── Raw table ─────────────────────────────────────────────────────────────
    {
        auto* sec = section_frame("CASH FLOW STATEMENT", kAmber);
        cf_table_ = make_table();
        cf_table_->setFixedHeight(300);
        static_cast<QVBoxLayout*>(sec->layout())->addWidget(cf_table_);
        vl->addWidget(sec);
    }

    vl->addStretch();
    scroll->setWidget(content);
    return scroll;
}

// ── Data slot ─────────────────────────────────────────────────────────────────

void EquityFinancialsTab::on_financials_loaded(services::equity::FinancialsData data) {
    if (data.symbol != current_symbol_)
        return;
    cached_data_ = data;
    loaded_ = true;
    loading_overlay_->hide_loading();

    populate_income_view(data);
    populate_balance_view(data);
    populate_cashflow_view(data);
    populate_table(inc_table_, data.income_statement);
    populate_table(bal_table_, data.balance_sheet);
    populate_table(cf_table_, data.cash_flow);
    rebuild_revenue_chart(data);
    rebuild_margin_chart(data);
    rebuild_balance_chart(data);
    rebuild_cashflow_chart(data);
    rebuild_return_chart(data);
}

// ── Populate helpers ──────────────────────────────────────────────────────────

double EquityFinancialsTab::get_val(const QJsonObject& o, const QStringList& keys) {
    for (const auto& k : keys) {
        if (o.contains(k) && !o[k].isNull()) {
            double v = o[k].toVariant().toDouble();
            if (v != 0.0)
                return v;
        }
    }
    return 0.0;
}

void EquityFinancialsTab::populate_income_view(const services::equity::FinancialsData& d) {
    if (d.income_statement.isEmpty())
        return;

    const auto& latest = d.income_statement[0].second;
    const auto& prev = d.income_statement.size() > 1 ? d.income_statement[1].second : QJsonObject();

    double revenue = get_val(latest, {"Total Revenue", "Revenue"});
    double prev_rev = get_val(prev, {"Total Revenue", "Revenue"});
    double gross = get_val(latest, {"Gross Profit"});
    double op_income = get_val(latest, {"Operating Income", "Operating Profit"});
    double net_income = get_val(latest, {"Net Income", "Net Income Common Stockholders"});
    double prev_net = get_val(prev, {"Net Income", "Net Income Common Stockholders"});
    double ebitda = get_val(latest, {"EBITDA", "Normalized EBITDA"});

    // Margins
    double gross_margin = revenue > 0 ? gross / revenue : 0.0;
    double op_margin = revenue > 0 ? op_income / revenue : 0.0;
    double net_margin = revenue > 0 ? net_income / revenue : 0.0;
    double ebitda_margin = revenue > 0 ? ebitda / revenue : 0.0;

    // Growth labels
    double rev_growth = prev_rev > 0 ? (revenue - prev_rev) / prev_rev : 0.0;
    double net_growth = prev_net > 0 ? (net_income - prev_net) / prev_net : 0.0;

    auto set = [](QLabel* l, const QString& t) {
        if (l)
            l->setText(t);
    };

    set(inc_revenue_val_, fmt_large(revenue));
    set(inc_revenue_sub_, rev_growth != 0.0 ? fmt_pct(rev_growth) + " YoY" : "");
    set(inc_gross_val_, fmt_large(gross));
    set(inc_gross_sub_, fmt_pct(gross_margin) + " margin");
    set(inc_opincome_val_, fmt_large(op_income));
    set(inc_opincome_sub_, fmt_pct(op_margin) + " margin");
    set(inc_netincome_val_, fmt_large(net_income));
    set(inc_netincome_sub_, net_growth != 0.0 ? fmt_pct(net_growth) + " YoY" : "");
    set(inc_ebitda_val_, fmt_large(ebitda));
    set(inc_ebitda_sub_, fmt_pct(ebitda_margin) + " margin");
    set(inc_gross_margin_, fmt_pct(gross_margin));
    set(inc_op_margin_, fmt_pct(op_margin));
    set(inc_net_margin_, fmt_pct(net_margin));
    set(inc_ebitda_margin_, fmt_pct(ebitda_margin));

    // DuPont (needs balance sheet too)
    if (!d.balance_sheet.isEmpty()) {
        const auto& bal = d.balance_sheet[0].second;
        double total_assets = get_val(bal, {"Total Assets"});
        double total_equity = get_val(bal, {"Stockholders Equity", "Total Equity", "Total Stockholder Equity"});
        double asset_turnover = total_assets > 0 ? revenue / total_assets : 0.0;
        double eq_mult = total_equity > 0 ? total_assets / total_equity : 0.0;
        double roe = total_equity > 0 ? net_income / total_equity : 0.0;
        double roa = total_assets > 0 ? net_income / total_assets : 0.0;

        set(dupont_net_margin_, fmt_pct(net_margin));
        set(dupont_asset_turn_, QString::number(asset_turnover, 'f', 2) + "x");
        set(dupont_eq_mult_, QString::number(eq_mult, 'f', 2) + "x");
        set(dupont_roe_result_, fmt_pct(roe));

        // Invested capital for ROIC
        double total_debt = get_val(bal, {"Total Debt", "Long Term Debt"});
        double cash = get_val(bal, {"Cash And Cash Equivalents", "Cash"});
        double inv_cap = total_equity + total_debt - cash;
        double roic = inv_cap > 0 ? (op_income * 0.75) / inv_cap : 0.0;
        double cur_liab = get_val(bal, {"Current Liabilities", "Total Current Liabilities"});
        double roce = (total_assets - cur_liab) > 0 ? op_income / (total_assets - cur_liab) : 0.0;

        set(ret_roe_val_, fmt_pct(roe));
        set(ret_roa_val_, fmt_pct(roa));
        set(ret_roic_val_, fmt_pct(roic));
        set(ret_roce_val_, fmt_pct(roce));
    }
}

void EquityFinancialsTab::populate_balance_view(const services::equity::FinancialsData& d) {
    if (d.balance_sheet.isEmpty())
        return;

    const auto& b = d.balance_sheet[0].second;
    double total_assets = get_val(b, {"Total Assets"});
    double total_liab = get_val(b, {"Total Liabilities Net Minority Interest", "Total Liabilities"});
    double total_equity = get_val(b, {"Stockholders Equity", "Total Equity", "Total Stockholder Equity"});
    double total_debt = get_val(b, {"Total Debt"});
    double cash = get_val(b, {"Cash And Cash Equivalents", "Cash"});
    double cur_assets = get_val(b, {"Current Assets", "Total Current Assets"});
    double cur_liab = get_val(b, {"Current Liabilities", "Total Current Liabilities"});
    double inventory = get_val(b, {"Inventory"});
    double ebitda = 0.0;
    double interest = 0.0;
    if (!d.income_statement.isEmpty()) {
        const auto& inc = d.income_statement[0].second;
        ebitda = get_val(inc, {"EBITDA", "Normalized EBITDA"});
        interest = get_val(inc, {"Interest Expense", "Interest Expense Non Operating"});
        if (interest < 0)
            interest = -interest;
    }

    auto set = [](QLabel* l, const QString& t) {
        if (l)
            l->setText(t);
    };

    set(bal_assets_val_, fmt_large(total_assets));
    set(bal_liabilities_val_, fmt_large(total_liab));
    set(bal_equity_val_, fmt_large(total_equity));
    set(bal_debt_val_, fmt_large(total_debt));
    set(bal_cash_val_, fmt_large(cash));

    double cur_ratio = cur_liab > 0 ? cur_assets / cur_liab : 0.0;
    double quick_ratio = cur_liab > 0 ? (cur_assets - inventory) / cur_liab : 0.0;
    double work_cap = cur_assets - cur_liab;
    double d_e = total_equity > 0 ? total_debt / total_equity : 0.0;
    double d_a = total_assets > 0 ? total_debt / total_assets : 0.0;
    double int_cov = interest > 0 ? ebitda / interest : 0.0;

    set(bal_current_ratio_, fmt_ratio(cur_ratio));
    set(bal_quick_ratio_, fmt_ratio(quick_ratio));
    set(bal_working_cap_, fmt_large(work_cap));
    set(bal_debt_equity_, fmt_ratio(d_e));
    set(bal_debt_assets_, fmt_pct(d_a));
    set(bal_int_coverage_, int_cov > 0 ? fmt_ratio(int_cov) + "x" : "N/A");
}

void EquityFinancialsTab::populate_cashflow_view(const services::equity::FinancialsData& d) {
    if (d.cash_flow.isEmpty())
        return;

    const auto& cf = d.cash_flow[0].second;
    double op_cf = get_val(cf, {"Operating Cash Flow", "Total Cash From Operating Activities"});
    double inv_cf = get_val(cf, {"Investing Cash Flow", "Total Cash From Investing Activities"});
    double fin_cf = get_val(cf, {"Financing Cash Flow", "Total Cash From Financing Activities"});
    double capex = get_val(cf, {"Capital Expenditure", "Capital Expenditures"});
    if (capex > 0)
        capex = -capex;         // capex is usually negative
    double fcf = op_cf + capex; // capex is negative so this subtracts
    double dividends = get_val(cf, {"Cash Dividends Paid", "Payment Of Dividends"});
    if (dividends > 0)
        dividends = -dividends;
    double buybacks = get_val(
        cf, {"Repurchase Of Capital Stock", "Common Stock Repurchased", "Common Stock Payments", "Purchase Of Stock"});
    if (buybacks > 0)
        buybacks = -buybacks;

    double revenue = 0.0;
    if (!d.income_statement.isEmpty())
        revenue = get_val(d.income_statement[0].second, {"Total Revenue", "Revenue"});
    double fcf_margin = revenue > 0 ? fcf / revenue : 0.0;
    double capex_rev = revenue > 0 ? qAbs(capex) / revenue : 0.0;

    auto set = [](QLabel* l, const QString& t) {
        if (l)
            l->setText(t);
    };

    set(cf_operating_val_, fmt_large(op_cf));
    set(cf_investing_val_, fmt_large(inv_cf));
    set(cf_financing_val_, fmt_large(fin_cf));
    set(cf_fcf_val_, fmt_large(fcf));
    set(cf_fcf_sub_, fcf_margin != 0.0 ? fmt_pct(fcf_margin) + " margin" : "");
    set(cf_capex_val_, fmt_large(qAbs(capex)));
    set(cf_dividends_val_, fmt_large(qAbs(dividends)));
    set(cf_buybacks_val_, fmt_large(qAbs(buybacks)));
    set(cf_fcf_margin_, fmt_pct(fcf_margin));
    set(cf_capex_rev_, fmt_pct(capex_rev));
}

// ── Chart builders ────────────────────────────────────────────────────────────

static void style_chart(QChart* chart) {
    chart->setBackgroundBrush(QBrush(QColor(ui::colors::BG_SURFACE())));
    chart->setPlotAreaBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
    chart->setPlotAreaBackgroundVisible(true);
    chart->legend()->setLabelColor(QColor(ui::colors::TEXT_SECONDARY()));
    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->setMargins(QMargins(4, 4, 4, 8));
}

static QBarCategoryAxis* make_x_axis(const QStringList& cats) {
    auto* ax = new QBarCategoryAxis;
    ax->append(cats);
    ax->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    ax->setLabelsFont(QFont("monospace", 8));
    ax->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    return ax;
}

static QValueAxis* make_y_axis(const QString& fmt = "%.1fB") {
    auto* ay = new QValueAxis;
    ay->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    ay->setLabelsFont(QFont("monospace", 8));
    ay->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    ay->setLabelFormat(fmt);
    return ay;
}

void EquityFinancialsTab::rebuild_revenue_chart(const services::equity::FinancialsData& d) {
    if (d.income_statement.isEmpty() || !inc_revenue_chart_)
        return;

    // Up to 5 periods, reversed (oldest first)
    int n = qMin(5, d.income_statement.size());
    QStringList cats;
    QVector<double> revenue_v, gross_v, net_v;

    for (int i = n - 1; i >= 0; --i) {
        cats << d.income_statement[i].first.left(4);
        revenue_v << get_val(d.income_statement[i].second, {"Total Revenue", "Revenue"}) / 1e9;
        gross_v << get_val(d.income_statement[i].second, {"Gross Profit"}) / 1e9;
        net_v << get_val(d.income_statement[i].second, {"Net Income", "Net Income Common Stockholders"}) / 1e9;
    }

    auto* rev_set = new QBarSet("Revenue");
    rev_set->setColor(QColor(kBlue));
    auto* gross_set = new QBarSet("Gross Profit");
    gross_set->setColor(QColor(kCyan));
    for (int i = 0; i < cats.size(); ++i) {
        *rev_set << revenue_v[i];
        *gross_set << gross_v[i];
    }

    auto* bar_series = new QBarSeries;
    bar_series->append(rev_set);
    bar_series->append(gross_set);

    auto* net_series = new QLineSeries;
    net_series->setName("Net Income");
    net_series->setColor(QColor(kGreen));
    net_series->setPen(QPen(QColor(kGreen), 2));
    for (int i = 0; i < cats.size(); ++i)
        net_series->append(i, net_v[i]);

    auto* chart = new QChart;
    style_chart(chart);
    chart->addSeries(bar_series);
    chart->addSeries(net_series);
    chart->legend()->show();

    auto* axX = make_x_axis(cats);
    auto* axY = make_y_axis("$%.0fB");
    chart->addAxis(axX, Qt::AlignBottom);
    chart->addAxis(axY, Qt::AlignLeft);
    bar_series->attachAxis(axX);
    bar_series->attachAxis(axY);
    net_series->attachAxis(axX);
    net_series->attachAxis(axY);

    inc_revenue_chart_->setChart(chart);
}

void EquityFinancialsTab::rebuild_margin_chart(const services::equity::FinancialsData& d) {
    if (d.income_statement.isEmpty() || !inc_margin_chart_)
        return;

    int n = qMin(5, d.income_statement.size());
    QStringList cats;
    QVector<double> gross_v, op_v, net_v, ebitda_v;

    for (int i = n - 1; i >= 0; --i) {
        const auto& stmt = d.income_statement[i].second;
        cats << d.income_statement[i].first.left(4);
        double rev = get_val(stmt, {"Total Revenue", "Revenue"});
        if (rev == 0.0)
            rev = 1.0;
        gross_v << get_val(stmt, {"Gross Profit"}) / rev * 100.0;
        op_v << get_val(stmt, {"Operating Income", "Operating Profit"}) / rev * 100.0;
        net_v << get_val(stmt, {"Net Income", "Net Income Common Stockholders"}) / rev * 100.0;
        ebitda_v << get_val(stmt, {"EBITDA", "Normalized EBITDA"}) / rev * 100.0;
    }

    auto make_line = [&](const QString& name, const QString& color, const QVector<double>& vals) {
        auto* s = new QLineSeries;
        s->setName(name);
        s->setColor(QColor(color));
        s->setPen(QPen(QColor(color), 2));
        for (int i = 0; i < cats.size(); ++i)
            s->append(i, vals[i]);
        return s;
    };

    auto* chart = new QChart;
    style_chart(chart);
    auto* s1 = make_line("Gross", kGreen, gross_v);
    auto* s2 = make_line("Operating", kCyan, op_v);
    auto* s3 = make_line("Net", kOrange, net_v);
    auto* s4 = make_line("EBITDA", kPurple, ebitda_v);
    chart->addSeries(s1);
    chart->addSeries(s2);
    chart->addSeries(s3);
    chart->addSeries(s4);
    chart->legend()->show();

    auto* axX = make_x_axis(cats);
    auto* axY = make_y_axis("%.0f%%");
    chart->addAxis(axX, Qt::AlignBottom);
    chart->addAxis(axY, Qt::AlignLeft);
    for (auto* s : {s1, s2, s3, s4}) {
        s->attachAxis(axX);
        s->attachAxis(axY);
    }

    inc_margin_chart_->setChart(chart);
}

void EquityFinancialsTab::rebuild_balance_chart(const services::equity::FinancialsData& d) {
    if (d.balance_sheet.isEmpty() || !bal_chart_)
        return;

    int n = qMin(5, d.balance_sheet.size());
    QStringList cats;

    auto* assets_set = new QBarSet("Assets");
    assets_set->setColor(QColor(kBlue));
    auto* liab_set = new QBarSet("Liabilities");
    liab_set->setColor(QColor(kRed));
    auto* equity_set = new QBarSet("Equity");
    equity_set->setColor(QColor(kGreen));

    for (int i = n - 1; i >= 0; --i) {
        cats << d.balance_sheet[i].first.left(4);
        const auto& b = d.balance_sheet[i].second;
        *assets_set << get_val(b, {"Total Assets"}) / 1e9;
        *liab_set << get_val(b, {"Total Liabilities Net Minority Interest", "Total Liabilities"}) / 1e9;
        *equity_set << get_val(b, {"Stockholders Equity", "Total Equity"}) / 1e9;
    }

    auto* series = new QBarSeries;
    series->append(assets_set);
    series->append(liab_set);
    series->append(equity_set);

    auto* chart = new QChart;
    style_chart(chart);
    chart->addSeries(series);
    chart->legend()->show();

    auto* axX = make_x_axis(cats);
    auto* axY = make_y_axis("$%.0fB");
    chart->addAxis(axX, Qt::AlignBottom);
    chart->addAxis(axY, Qt::AlignLeft);
    series->attachAxis(axX);
    series->attachAxis(axY);

    bal_chart_->setChart(chart);
}

void EquityFinancialsTab::rebuild_cashflow_chart(const services::equity::FinancialsData& d) {
    if (d.cash_flow.isEmpty() || !cf_chart_)
        return;

    int n = qMin(5, d.cash_flow.size());
    QStringList cats;

    auto* op_set = new QBarSet("Operating");
    op_set->setColor(QColor(kGreen));
    auto* inv_set = new QBarSet("Investing");
    inv_set->setColor(QColor(kOrange));
    auto* fin_set = new QBarSet("Financing");
    fin_set->setColor(QColor(kPurple));

    auto* fcf_series = new QLineSeries;
    fcf_series->setName("Free CF");
    fcf_series->setColor(QColor(kCyan));
    fcf_series->setPen(QPen(QColor(kCyan), 2));

    for (int i = n - 1; i >= 0; --i) {
        cats << d.cash_flow[i].first.left(4);
        const auto& cf = d.cash_flow[i].second;
        double op = get_val(cf, {"Operating Cash Flow", "Total Cash From Operating Activities"});
        double inv = get_val(cf, {"Investing Cash Flow", "Total Cash From Investing Activities"});
        double fin = get_val(cf, {"Financing Cash Flow", "Total Cash From Financing Activities"});
        double capex = get_val(cf, {"Capital Expenditure", "Capital Expenditures"});
        if (capex > 0)
            capex = -capex;
        *op_set << op / 1e9;
        *inv_set << inv / 1e9;
        *fin_set << fin / 1e9;
        fcf_series->append(cats.size() - 1, (op + capex) / 1e9);
    }

    auto* bar_series = new QBarSeries;
    bar_series->append(op_set);
    bar_series->append(inv_set);
    bar_series->append(fin_set);

    auto* chart = new QChart;
    style_chart(chart);
    chart->addSeries(bar_series);
    chart->addSeries(fcf_series);
    chart->legend()->show();

    auto* axX = make_x_axis(cats);
    auto* axY = make_y_axis("$%.0fB");
    chart->addAxis(axX, Qt::AlignBottom);
    chart->addAxis(axY, Qt::AlignLeft);
    bar_series->attachAxis(axX);
    bar_series->attachAxis(axY);
    fcf_series->attachAxis(axX);
    fcf_series->attachAxis(axY);

    cf_chart_->setChart(chart);
}

void EquityFinancialsTab::rebuild_return_chart(const services::equity::FinancialsData& d) {
    if (d.income_statement.isEmpty() || d.balance_sheet.isEmpty() || !ret_chart_)
        return;

    int n = qMin(5, qMin(d.income_statement.size(), d.balance_sheet.size()));
    QStringList cats;

    auto* roe_series = new QLineSeries;
    roe_series->setName("ROE %");
    roe_series->setColor(QColor(kCyan));
    roe_series->setPen(QPen(QColor(kCyan), 2));

    auto* roa_series = new QLineSeries;
    roa_series->setName("ROA %");
    roa_series->setColor(QColor(kGreen));
    roa_series->setPen(QPen(QColor(kGreen), 2));

    for (int i = n - 1; i >= 0; --i) {
        cats << d.income_statement[i].first.left(4);
        double net = get_val(d.income_statement[i].second, {"Net Income", "Net Income Common Stockholders"});
        double assets = get_val(d.balance_sheet[i].second, {"Total Assets"});
        double equity = get_val(d.balance_sheet[i].second, {"Stockholders Equity", "Total Equity"});
        roe_series->append(cats.size() - 1, equity > 0 ? (net / equity) * 100.0 : 0.0);
        roa_series->append(cats.size() - 1, assets > 0 ? (net / assets) * 100.0 : 0.0);
    }

    auto* chart = new QChart;
    style_chart(chart);
    chart->addSeries(roe_series);
    chart->addSeries(roa_series);
    chart->legend()->show();

    auto* axX = make_x_axis(cats);
    auto* axY = make_y_axis("%.0f%%");
    chart->addAxis(axX, Qt::AlignBottom);
    chart->addAxis(axY, Qt::AlignLeft);
    roe_series->attachAxis(axX);
    roe_series->attachAxis(axY);
    roa_series->attachAxis(axX);
    roa_series->attachAxis(axY);

    ret_chart_->setChart(chart);
}

// ── Raw table ─────────────────────────────────────────────────────────────────

void EquityFinancialsTab::populate_table(QTableWidget* table, const QVector<QPair<QString, QJsonObject>>& stmt) {

    if (!table || stmt.isEmpty())
        return;
    table->clearContents();

    QStringList periods;
    QSet<QString> keys_set;
    for (const auto& p : stmt) {
        periods << p.first.left(10);
        for (auto it = p.second.constBegin(); it != p.second.constEnd(); ++it)
            keys_set.insert(it.key());
    }
    QStringList metrics = keys_set.values();
    metrics.sort();

    table->setColumnCount(periods.size() + 1);
    table->setRowCount(metrics.size());

    QStringList headers;
    headers << "Metric";
    headers.append(periods);
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    for (int r = 0; r < metrics.size(); ++r) {
        auto* mk = new QTableWidgetItem(metrics[r]);
        mk->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
        table->setItem(r, 0, mk);

        for (int c = 0; c < stmt.size(); ++c) {
            double val = stmt[c].second[metrics[r]].toVariant().toDouble();
            QString text = val == 0.0 ? "—" : fmt_large(val);
            auto* cell = new QTableWidgetItem(text);
            cell->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            if (val < 0)
                cell->setForeground(QColor(kRed));
            table->setItem(r, c + 1, cell);
        }
    }
    table->resizeColumnsToContents();
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
}

// ── Formatters ────────────────────────────────────────────────────────────────

QString EquityFinancialsTab::fmt_large(double v) {
    bool neg = v < 0;
    double av = qAbs(v);
    QString s;
    if (av >= 1e12)
        s = QString("%1T").arg(av / 1e12, 0, 'f', 2);
    else if (av >= 1e9)
        s = QString("%1B").arg(av / 1e9, 0, 'f', 2);
    else if (av >= 1e6)
        s = QString("%1M").arg(av / 1e6, 0, 'f', 1);
    else if (av >= 1e3)
        s = QString("%1K").arg(av / 1e3, 0, 'f', 1);
    else
        s = QString::number(static_cast<qint64>(av));
    return neg ? "-" + s : s;
}

QString EquityFinancialsTab::fmt_pct(double v) {
    return QString("%1%").arg(v * 100.0, 0, 'f', 2);
}

QString EquityFinancialsTab::fmt_ratio(double v) {
    return QString::number(v, 'f', 2);
}

} // namespace fincept::screens
