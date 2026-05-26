// src/screens/equity_research/EquityFinancialsTab_Views.cpp
//
// View construction: build_ui, build_income_view, build_balance_view,
// build_cashflow_view.
//
// Part of the partial-class split of EquityFinancialsTab.cpp.

#include "screens/equity_research/EquityFinancialsTab.h"

#include "screens/equity_research/EquityFinancialsTab_internal.h"
#include "services/equity/EquityResearchService.h"
#include "services/file_manager/FileManagerService.h"
#include "ui/theme/Theme.h"

#include <QBarCategoryAxis>
#include <QBarSeries>
#include <QBarSet>
#include <QChart>
#include <QChartView>
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
#include <QStackedWidget>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextStream>
#include <QVBoxLayout>
#include <QValueAxis>

namespace fincept::screens {

using namespace financials_internal;


void EquityFinancialsTab::build_ui() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    loading_overlay_ = new ui::LoadingOverlay(this);
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Statement selector ────────────────────────────────────────────────────
    auto* btn_bar = new QWidget(this);
    btn_bar->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
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
            .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(), kAmber, ui::colors::BG_BASE(), ui::colors::BG_HOVER());

    auto make_btn = [&](const QString& label, QPushButton*& out) {
        out = new QPushButton(label);
        out->setCheckable(true);
        out->setStyleSheet(btn_style);
        btn_hl->addWidget(out);
    };
    make_btn(tr("Income Statement"), btn_income_);
    make_btn(tr("Balance Sheet"),    btn_balance_);
    make_btn(tr("Cash Flow"),        btn_cashflow_);
    btn_income_->setChecked(true);
    btn_hl->addStretch();

    btn_export_csv_ = new QPushButton(tr("EXPORT CSV"));
    btn_export_csv_->setStyleSheet(QString(R"(
        QPushButton {
            background:transparent; color:%1; border:1px solid %1;
            border-radius:3px; padding:4px 14px; font-size:11px; font-weight:700;
        }
        QPushButton:hover { background:%1; color:#000; }
    )")
                                  .arg(kAmber));
    btn_hl->addWidget(btn_export_csv_);

    connect(btn_export_csv_, &QPushButton::clicked, this, [this]() {
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
        auto* sec = section_frame(tr("KEY FINANCIAL METRICS"), kOrange);
        auto* svl = static_cast<QVBoxLayout*>(sec->layout());

        // Row 1: 4 large cards
        auto* r1 = new QHBoxLayout;
        r1->setSpacing(8);
        r1->addWidget(metric_card(tr("TOTAL REVENUE"),     inc_revenue_val_,   inc_revenue_sub_,   kGreen));
        r1->addWidget(metric_card(tr("GROSS PROFIT"),      inc_gross_val_,     inc_gross_sub_,     kCyan));
        r1->addWidget(metric_card(tr("OPERATING INCOME"),  inc_opincome_val_,  inc_opincome_sub_,  kBlue));
        r1->addWidget(metric_card(tr("NET INCOME"),        inc_netincome_val_, inc_netincome_sub_, kGreen));
        svl->addLayout(r1);

        // Row 2: EBITDA + margins
        auto* r2 = new QHBoxLayout;
        r2->setSpacing(8);
        r2->addWidget(metric_card(tr("EBITDA"), inc_ebitda_val_, inc_ebitda_sub_, kPurple));

        // Margin cards (no subtitle needed)
        QLabel* dummy = nullptr;
        auto* gm = metric_card(tr("GROSS MARGIN"),     inc_gross_margin_,  dummy, kGreen,  QStringLiteral("—"), QString());
        auto* om = metric_card(tr("OPERATING MARGIN"), inc_op_margin_,     dummy, kCyan,   QStringLiteral("—"), QString());
        auto* nm = metric_card(tr("NET MARGIN"),       inc_net_margin_,    dummy, kOrange, QStringLiteral("—"), QString());
        auto* em = metric_card(tr("EBITDA MARGIN"),    inc_ebitda_margin_, dummy, kPurple, QStringLiteral("—"), QString());
        r2->addWidget(gm);
        r2->addWidget(om);
        r2->addWidget(nm);
        r2->addWidget(em);
        svl->addLayout(r2);

        vl->addWidget(sec);
    }

    // ── Revenue & Earnings Chart ───────────────────────────────────────────────
    {
        auto* sec = section_frame(tr("REVENUE & EARNINGS TREND"), kBlue);
        inc_revenue_chart_ = make_chart_view(200);
        static_cast<QVBoxLayout*>(sec->layout())->addWidget(inc_revenue_chart_);
        vl->addWidget(sec);
    }

    // ── Margin Trend Chart ────────────────────────────────────────────────────
    {
        auto* sec = section_frame(tr("MARGIN TRENDS (%)"), kGreen);
        inc_margin_chart_ = make_chart_view(180);
        static_cast<QVBoxLayout*>(sec->layout())->addWidget(inc_margin_chart_);
        vl->addWidget(sec);
    }

    // ── DuPont & Return Metrics ───────────────────────────────────────────────
    {
        auto* sec = section_frame(tr("RETURN METRICS & DUPONT ANALYSIS"), kCyan);
        auto* svl = static_cast<QVBoxLayout*>(sec->layout());
        auto* hl = new QHBoxLayout;
        hl->setSpacing(10);

        // Return metrics column
        auto* ret_frame = new QFrame;
        ret_frame->setStyleSheet("QFrame { background:transparent; border:0; }");
        auto* rvl = new QVBoxLayout(ret_frame);
        rvl->setContentsMargins(0, 0, 0, 0);
        rvl->setSpacing(6);
        auto* ret_title = new QLabel(tr("RETURN METRICS"));
        ret_title->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; "
                                         "background:transparent; border:0;")
                                     .arg(kCyan));
        rvl->addWidget(ret_title);
        ret_roe_val_  = ratio_row(ret_frame, tr("Return on Equity (ROE)"), kCyan);
        ret_roa_val_  = ratio_row(ret_frame, tr("Return on Assets (ROA)"), kCyan);
        ret_roic_val_ = ratio_row(ret_frame, tr("Return on Inv. Capital"), kBlue);
        ret_roce_val_ = ratio_row(ret_frame, tr("Return on Cap. Employed"), kBlue);
        rvl->addStretch();
        hl->addWidget(ret_frame, 1);

        // DuPont column
        auto* dp_frame = new QFrame;
        dp_frame->setStyleSheet(QString("QFrame { background:%1; border:1px solid %2; border-radius:4px; }")
                                    .arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));
        auto* dvl = new QVBoxLayout(dp_frame);
        dvl->setContentsMargins(10, 8, 10, 8);
        dvl->setSpacing(6);
        auto* dp_title = new QLabel(tr("DUPONT ANALYSIS"));
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
                QString("color:%1; font-size:9px; background:transparent; border:0;").arg(ui::colors::TEXT_TERTIARY()));
            lbl->setAlignment(Qt::AlignCenter);
            col->addWidget(out);
            col->addWidget(lbl);
            formula_hl->addLayout(col);
        };
        auto make_op = [&](const QString& sym) {
            auto* op = new QLabel(sym);
            op->setStyleSheet(
                QString("color:%1; font-size:16px; background:transparent; border:0;").arg(ui::colors::TEXT_TERTIARY()));
            op->setAlignment(Qt::AlignCenter);
            formula_hl->addWidget(op);
        };
        make_dp_val(dupont_net_margin_, tr("Net Margin"));
        make_op("×");
        make_dp_val(dupont_asset_turn_, tr("Asset Turn"));
        make_op("×");
        make_dp_val(dupont_eq_mult_,    tr("Eq. Mult"));
        make_op("=");
        make_dp_val(dupont_roe_result_, tr("ROE"));
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
        auto* sec = section_frame(tr("INCOME STATEMENT"), kAmber);
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
        auto* sec = section_frame(tr("BALANCE SHEET OVERVIEW"), kBlue);
        auto* svl = static_cast<QVBoxLayout*>(sec->layout());
        auto* r1 = new QHBoxLayout;
        r1->setSpacing(8);
        QLabel* dummy = nullptr;
        r1->addWidget(metric_card(tr("TOTAL ASSETS"),        bal_assets_val_,      dummy, kBlue,   QStringLiteral("—"), QString()));
        r1->addWidget(metric_card(tr("TOTAL LIABILITIES"),   bal_liabilities_val_, dummy, kRed,    QStringLiteral("—"), QString()));
        r1->addWidget(metric_card(tr("STOCKHOLDERS EQUITY"), bal_equity_val_,      dummy, kGreen,  QStringLiteral("—"), QString()));
        r1->addWidget(metric_card(tr("TOTAL DEBT"),          bal_debt_val_,        dummy, kOrange, QStringLiteral("—"), QString()));
        r1->addWidget(metric_card(tr("CASH & EQUIV."),       bal_cash_val_,        dummy, kCyan,   QStringLiteral("—"), QString()));
        svl->addLayout(r1);
        vl->addWidget(sec);
    }

    // ── Balance Trend Chart ───────────────────────────────────────────────────
    {
        auto* sec = section_frame(tr("ASSETS, LIABILITIES & EQUITY TREND"), kBlue);
        bal_chart_ = make_chart_view(200);
        static_cast<QVBoxLayout*>(sec->layout())->addWidget(bal_chart_);
        vl->addWidget(sec);
    }

    // ── Liquidity & Leverage Ratios ───────────────────────────────────────────
    {
        auto* sec = section_frame(tr("LIQUIDITY & LEVERAGE RATIOS"), kYellow);
        auto* svl = static_cast<QVBoxLayout*>(sec->layout());
        auto* hl = new QHBoxLayout;
        hl->setSpacing(20);

        // Liquidity
        auto* liq = new QFrame;
        liq->setStyleSheet("QFrame { background:transparent; border:0; }");
        auto* lvl = new QVBoxLayout(liq);
        lvl->setContentsMargins(0, 0, 0, 0);
        lvl->setSpacing(4);
        auto* lt = new QLabel(tr("LIQUIDITY"));
        lt->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; "
                                  "background:transparent; border:0;")
                              .arg(kCyan));
        lvl->addWidget(lt);
        bal_current_ratio_ = ratio_row(liq, tr("Current Ratio"),   kCyan);
        bal_quick_ratio_   = ratio_row(liq, tr("Quick Ratio"),     kCyan);
        bal_working_cap_   = ratio_row(liq, tr("Working Capital"), kGreen);
        lvl->addStretch();
        hl->addWidget(liq, 1);

        // Leverage
        auto* lev = new QFrame;
        lev->setStyleSheet("QFrame { background:transparent; border:0; }");
        auto* evl = new QVBoxLayout(lev);
        evl->setContentsMargins(0, 0, 0, 0);
        evl->setSpacing(4);
        auto* et = new QLabel(tr("LEVERAGE / SOLVENCY"));
        et->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; "
                                  "background:transparent; border:0;")
                              .arg(kOrange));
        evl->addWidget(et);
        bal_debt_equity_  = ratio_row(lev, tr("Debt / Equity"),     kOrange);
        bal_debt_assets_  = ratio_row(lev, tr("Debt / Assets"),     kOrange);
        bal_int_coverage_ = ratio_row(lev, tr("Interest Coverage"), kYellow);
        evl->addStretch();
        hl->addWidget(lev, 1);

        svl->addLayout(hl);
        vl->addWidget(sec);
    }

    // ── Raw table ─────────────────────────────────────────────────────────────
    {
        auto* sec = section_frame(tr("BALANCE SHEET"), kAmber);
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
        auto* sec = section_frame(tr("CASH FLOW OVERVIEW"), kGreen);
        auto* svl = static_cast<QVBoxLayout*>(sec->layout());
        auto* r1 = new QHBoxLayout;
        r1->setSpacing(8);
        QLabel* dummy1 = nullptr;
        r1->addWidget(metric_card(tr("OPERATING CF"),   cf_operating_val_, dummy1,     kGreen));
        r1->addWidget(metric_card(tr("INVESTING CF"),   cf_investing_val_, dummy1,     kOrange));
        r1->addWidget(metric_card(tr("FINANCING CF"),   cf_financing_val_, dummy1,     kPurple));
        r1->addWidget(metric_card(tr("FREE CASH FLOW"), cf_fcf_val_,       cf_fcf_sub_, kCyan));
        svl->addLayout(r1);

        auto* r2 = new QHBoxLayout;
        r2->setSpacing(8);
        QLabel* dummy = nullptr;
        r2->addWidget(metric_card(tr("CAPEX"),          cf_capex_val_,     dummy, kRed,    QStringLiteral("—"), QString()));
        r2->addWidget(metric_card(tr("DIVIDENDS PAID"), cf_dividends_val_, dummy, kYellow, QStringLiteral("—"), QString()));
        r2->addWidget(metric_card(tr("STOCK BUYBACKS"), cf_buybacks_val_,  dummy, kBlue,   QStringLiteral("—"), QString()));
        r2->addWidget(metric_card(tr("FCF MARGIN"),     cf_fcf_margin_,    dummy, kCyan,   QStringLiteral("—"), QString()));
        r2->addWidget(metric_card(tr("CAPEX/REVENUE"),  cf_capex_rev_,     dummy, kOrange, QStringLiteral("—"), QString()));
        svl->addLayout(r2);
        vl->addWidget(sec);
    }

    // ── Cash Flow Trend Chart ─────────────────────────────────────────────────
    {
        auto* sec = section_frame(tr("CASH FLOW TREND (BILLIONS $)"), kGreen);
        cf_chart_ = make_chart_view(200);
        static_cast<QVBoxLayout*>(sec->layout())->addWidget(cf_chart_);
        vl->addWidget(sec);
    }

    // ── Raw table ─────────────────────────────────────────────────────────────
    {
        auto* sec = section_frame(tr("CASH FLOW STATEMENT"), kAmber);
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
} // namespace fincept::screens
