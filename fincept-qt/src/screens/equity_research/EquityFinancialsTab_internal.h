#pragma once
// Shared helpers for the EquityFinancialsTab partial-class split.
//
// Both `EquityFinancialsTab_Views.cpp` and `EquityFinancialsTab_Populate.cpp`
// need the same color constants, frame builders, table factory, and chart
// view factory. Defining them at file scope inside each TU works under
// non-Unity builds but causes ODR violations under Unity concatenation.
//
// Placing them here as `inline` (C++17) gives them a single canonical
// definition across translation units while keeping them inline-able.

#include "ui/theme/Theme.h"

#include <QChartView>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QString>
#include <QTableWidget>
#include <QVBoxLayout>

namespace fincept::screens::financials_internal {

inline const QString kAmber  = QStringLiteral("#f59e0b");
inline const QString kCyan   = QStringLiteral("#22d3ee");
inline const QString kGreen  = ui::colors::POSITIVE;
inline const QString kRed    = ui::colors::NEGATIVE;
inline const QString kBlue   = QStringLiteral("#3b82f6");
inline const QString kPurple = QStringLiteral("#a855f7");
inline const QString kOrange = QStringLiteral("#f97316");
inline const QString kYellow = QStringLiteral("#eab308");

inline QFrame* section_frame(const QString& title, const QString& color) {
    auto* f = new QFrame;
    f->setStyleSheet(QString("QFrame { background:%1; border:1px solid %2; border-radius:4px; }")
                         .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
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
        QString("border:0; border-top:1px solid %1; background:transparent;").arg(ui::colors::BORDER_DIM()));
    vl->addWidget(sep);
    return f;
}

// Large metric card: label on top, big value, optional subtitle
inline QWidget* metric_card(const QString& label, QLabel*& val_out, QLabel*& sub_out,
                            const QString& val_color, const QString& initial_val = QStringLiteral("—"),
                            const QString& initial_sub = {}) {
    auto* f = new QFrame;
    f->setStyleSheet(QString("QFrame { background:%1; border:1px solid %2; border-radius:4px; }")
                         .arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));
    auto* vl = new QVBoxLayout(f);
    vl->setContentsMargins(8, 6, 8, 6);
    vl->setSpacing(2);

    auto* lbl = new QLabel(label);
    lbl->setStyleSheet(QString("color:%1; font-size:9px; font-weight:600; letter-spacing:1px; "
                               "background:transparent; border:0;")
                           .arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(lbl);

    val_out = new QLabel(initial_val);
    val_out->setStyleSheet(QString("color:%1; font-size:14px; font-weight:700; "
                                   "background:transparent; border:0;")
                               .arg(val_color));
    vl->addWidget(val_out);

    if (!initial_sub.isNull()) {
        sub_out = new QLabel(initial_sub.isEmpty() ? QString() : initial_sub);
        sub_out->setStyleSheet(
            QString("color:%1; font-size:9px; background:transparent; border:0;").arg(ui::colors::TEXT_TERTIARY()));
        vl->addWidget(sub_out);
    } else {
        sub_out = nullptr;
    }
    return f;
}

// Small ratio row appended to an existing QVBoxLayout owner.
inline QLabel* ratio_row(QWidget* parent_vl_owner, const QString& label, const QString& color) {
    auto* hl = new QHBoxLayout;
    hl->setSpacing(4);
    hl->setContentsMargins(0, 0, 0, 0);
    auto* k = new QLabel(label);
    k->setStyleSheet(
        QString("color:%1; font-size:10px; background:transparent; border:0;").arg(ui::colors::TEXT_SECONDARY()));
    auto* v = new QLabel(QStringLiteral("—"));
    v->setStyleSheet(
        QString("color:%1; font-size:10px; font-weight:600; background:transparent; border:0;").arg(color));
    v->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    hl->addWidget(k);
    hl->addStretch();
    hl->addWidget(v);
    static_cast<QVBoxLayout*>(parent_vl_owner->layout())->addLayout(hl);
    return v;
}

inline QTableWidget* make_table() {
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
                         .arg(ui::colors::BG_SURFACE(), ui::colors::BG_BASE(), ui::colors::BORDER_DIM(),
                              ui::colors::TEXT_PRIMARY(), ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY()));
    t->horizontalHeader()->setStretchLastSection(false);
    t->verticalHeader()->setDefaultSectionSize(24);
    t->verticalHeader()->hide();
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    return t;
}

inline QChartView* make_chart_view(int fixed_height = 0) {
    auto* cv = new QChartView;
    cv->setRenderHint(QPainter::Antialiasing, false);
    cv->setStyleSheet(QStringLiteral("background:transparent; border:0;"));
    if (fixed_height > 0)
        cv->setFixedHeight(fixed_height);
    return cv;
}

} // namespace fincept::screens::financials_internal
