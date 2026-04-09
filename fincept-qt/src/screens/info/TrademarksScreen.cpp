#include "screens/info/TrademarksScreen.h"

#include "ui/theme/Theme.h"

#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;

static const char* MF = "font-family:'Consolas','Courier New',monospace;";

static QLabel* heading(const QString& num, const QString& title) {
    auto* lbl = new QLabel(QString("%1. %2").arg(num, title));
    lbl->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: 700; "
                               "background: transparent; margin-top: 8px; %2")
                           .arg(colors::AMBER, MF));
    return lbl;
}

static QLabel* body(const QString& text) {
    auto* lbl = new QLabel(text);
    lbl->setWordWrap(true);
    lbl->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent; %2")
                           .arg(colors::TEXT_PRIMARY, MF));
    return lbl;
}

static QLabel* bullet(const QString& text) {
    auto* lbl = new QLabel(QString("  > %1").arg(text));
    lbl->setWordWrap(true);
    lbl->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent; %2")
                           .arg(colors::TEXT_SECONDARY, MF));
    return lbl;
}

TrademarksScreen::TrademarksScreen(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("QWidget#TmRoot { background: %1; }").arg(colors::BG_BASE));
    setObjectName("TmRoot");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    auto* page = new QWidget;
    page->setStyleSheet(QString("background: %1;").arg(colors::BG_BASE));
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(6);

    // Back
    auto* back_btn = new QPushButton("< BACK");
    back_btn->setCursor(Qt::PointingHandCursor);
    back_btn->setStyleSheet(QString("QPushButton { color: %1; background: transparent; border: none; "
                                    "font-size: 12px; %2 } QPushButton:hover { color: %3; }")
                                .arg(colors::TEXT_SECONDARY, MF, colors::TEXT_PRIMARY));
    connect(back_btn, &QPushButton::clicked, this, &TrademarksScreen::navigate_back);
    vl->addWidget(back_btn, 0, Qt::AlignLeft);

    auto* title = new QLabel("TRADEMARKS");
    title->setStyleSheet(QString("color: %1; font-size: 20px; font-weight: 700; letter-spacing: 1px; "
                                 "background: transparent; %2")
                             .arg(colors::AMBER, MF));
    vl->addWidget(title);

    auto* updated = new QLabel("Last updated: January 1, 2026");
    updated->setStyleSheet(
        QString("color: %1; font-size: 11px; background: transparent; %2").arg(colors::TEXT_TERTIARY, MF));
    vl->addWidget(updated);
    vl->addSpacing(12);

    // Panel
    auto* panel = new QWidget;
    panel->setStyleSheet(QString("background: %1; border: 1px solid %2; border-radius: 2px;")
                             .arg(colors::BG_SURFACE, colors::BORDER_DIM));
    auto* pvl = new QVBoxLayout(panel);
    pvl->setContentsMargins(20, 16, 20, 16);
    pvl->setSpacing(6);

    pvl->addWidget(heading("1", "FINCEPT TRADEMARKS"));
    pvl->addWidget(bullet("Fincept (TM)"));
    pvl->addWidget(bullet("Fincept Terminal (TM)"));
    pvl->addWidget(bullet("Fincept Corporation (TM)"));
    pvl->addWidget(bullet("Fincept Logo and associated visual identities"));

    pvl->addWidget(heading("2", "THIRD-PARTY TRADEMARKS"));
    pvl->addWidget(body("The following are trademarks of their respective owners:"));
    pvl->addWidget(bullet("Reuters — Thomson Reuters Corporation"));
    pvl->addWidget(bullet("S&P 500 — S&P Dow Jones Indices LLC"));
    pvl->addWidget(bullet("NASDAQ — Nasdaq, Inc."));
    pvl->addWidget(bullet("NYSE — Intercontinental Exchange, Inc."));
    pvl->addWidget(bullet("FTSE — FTSE International Limited"));

    pvl->addWidget(heading("3", "TRADEMARK GUIDELINES"));
    pvl->addWidget(body("Permitted Uses:"));
    pvl->addWidget(bullet("Referring to Fincept products in editorial or descriptive contexts"));
    pvl->addWidget(bullet("Linking to official Fincept resources"));
    pvl->addWidget(bullet("Academic or research references"));

    pvl->addSpacing(4);
    pvl->addWidget(body("Prohibited Uses:"));
    pvl->addWidget(bullet("Using Fincept marks to imply endorsement or affiliation"));
    pvl->addWidget(bullet("Modifying or altering any Fincept trademark"));
    pvl->addWidget(bullet("Using Fincept marks in domain names or product names"));
    pvl->addWidget(bullet("Creating confusingly similar marks"));

    pvl->addWidget(heading("4", "COPYRIGHT NOTICE"));
    pvl->addWidget(body("Copyright 2024-2026 Fincept Corporation. All rights reserved."));
    pvl->addWidget(body("This software is licensed under AGPL-3.0-or-later for open source use, "
                        "with a separate commercial license available for enterprise deployment."));

    pvl->addWidget(heading("5", "DATA PROVIDER ACKNOWLEDGMENTS"));
    pvl->addWidget(bullet("Market data provided by various exchanges and data vendors"));
    pvl->addWidget(bullet("Economic data from DBnomics, FRED, and government sources"));
    pvl->addWidget(bullet("News data from licensed news aggregation services"));
    pvl->addWidget(bullet("Geopolitical data from HDX and public sources"));
    pvl->addWidget(bullet("Crypto data from exchange APIs (Kraken, HyperLiquid, etc.)"));

    pvl->addWidget(heading("6", "REPORTING INFRINGEMENT"));
    pvl->addWidget(body("To report trademark infringement, contact: support@fincept.in"));

    pvl->addWidget(heading("7", "LEGAL DEPARTMENT"));
    pvl->addWidget(body("Fincept Corporation — Legal Department"));
    pvl->addWidget(body("Email: support@fincept.in"));

    vl->addWidget(panel);
    vl->addStretch();

    scroll->setWidget(page);
    root->addWidget(scroll, 1);
}

} // namespace fincept::screens
