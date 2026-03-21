#include "screens/about/AboutScreen.h"
#include "ui/theme/Theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QFrame>
#include <QDesktopServices>
#include <QUrl>

namespace fincept::screens {

// ── Style constants ───────────────────────────────────────────────────────────

static const char* SECTION_LABEL =
    "color: #808080; font-size: 11px; font-weight: bold; letter-spacing: 0.5px; "
    "text-transform: uppercase; background: transparent; "
    "font-family: 'Consolas','Courier New',monospace;";

static const char* BODY =
    "color: #e5e5e5; font-size: 13px; background: transparent; "
    "font-family: 'Consolas','Courier New',monospace;";

static const char* MUTED =
    "color: #525252; font-size: 12px; background: transparent; "
    "font-family: 'Consolas','Courier New',monospace;";

static const char* LINK_STYLE =
    "color: #0891b2; font-size: 13px; background: transparent; "
    "font-family: 'Consolas','Courier New',monospace;";

static const char* PANEL =
    "background: #0a0a0a; border: 1px solid #1a1a1a; border-radius: 2px;";

static const char* PANEL_HEADER =
    "background: #111111; border-bottom: 1px solid #1a1a1a;";

static const char* LINK_BTN =
    "QPushButton { background: #111111; color: #0891b2; border: 1px solid #1a1a1a; "
    "border-radius: 2px; padding: 8px 12px; font-size: 12px; text-align: left; "
    "font-family: 'Consolas','Courier New',monospace; }"
    "QPushButton:hover { background: #161616; color: #38bdf8; }";

// ── Helpers ───────────────────────────────────────────────────────────────────

static QWidget* makePanel() {
    auto* w = new QWidget;
    w->setStyleSheet(PANEL);
    return w;
}

static QLabel* makePanelHeader(const QString& icon, const QString& title,
                                const QString& iconColor) {
    auto* lbl = new QLabel(QString("%1  %2").arg(icon, title));
    lbl->setStyleSheet(QString(
        "color: %1; font-size: 11px; font-weight: bold; letter-spacing: 0.5px; "
        "background: #111111; padding: 10px 14px; border-bottom: 1px solid #1a1a1a; "
        "font-family: 'Consolas','Courier New',monospace;").arg(iconColor));
    return lbl;
}

static QLabel* makeBullet(const QString& text) {
    auto* lbl = new QLabel(QString("■  %1").arg(text));
    lbl->setStyleSheet(
        "color: #808080; font-size: 12px; background: transparent; "
        "font-family: 'Consolas','Courier New',monospace;");
    lbl->setWordWrap(true);
    return lbl;
}

static QWidget* makeSeparator() {
    auto* line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("color: #1a1a1a;");
    return line;
}

// ── Constructor ───────────────────────────────────────────────────────────────

AboutScreen::AboutScreen(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("QWidget#AboutRoot { background: %1; }").arg(ui::colors::BG_BASE));
    setObjectName("AboutRoot");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);


    // ── Scrollable content ────────────────────────────────────────────────────
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    auto* page = new QWidget;
    page->setStyleSheet(QString("background: %1;").arg(ui::colors::BG_BASE));
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(12);

    // ── Version Information ───────────────────────────────────────────────────
    {
        auto* panel = makePanel();
        auto* pvl = new QVBoxLayout(panel);
        pvl->setContentsMargins(0, 0, 0, 0);
        pvl->setSpacing(0);

        pvl->addWidget(makePanelHeader("ℹ", "VERSION INFORMATION", ui::colors::AMBER));

        auto* body = new QWidget;
        body->setStyleSheet("background: transparent;");
        auto* bhl = new QHBoxLayout(body);
        bhl->setContentsMargins(14, 12, 14, 12);

        auto* left = new QVBoxLayout;
        left->setSpacing(4);
        auto* name = new QLabel("Fincept Terminal");
        name->setStyleSheet(
            "color: #e5e5e5; font-size: 15px; font-weight: bold; background: transparent; "
            "font-family: 'Consolas','Courier New',monospace;");
        left->addWidget(name);
        auto* sub = new QLabel("NATIVE DESKTOP FINANCIAL INTELLIGENCE TERMINAL");
        sub->setStyleSheet(MUTED);
        left->addWidget(sub);
        bhl->addLayout(left);
        bhl->addStretch();

        auto* right = new QVBoxLayout;
        right->setSpacing(4);
        right->setAlignment(Qt::AlignRight);
        auto* ver = new QLabel("v4.0.0");
        ver->setStyleSheet(
            "color: #d97706; font-size: 18px; font-weight: bold; background: transparent; "
            "font-family: 'Consolas','Courier New',monospace;");
        ver->setAlignment(Qt::AlignRight);
        right->addWidget(ver);
        auto* bdate = new QLabel("Build: 2026-03-20");
        bdate->setStyleSheet(MUTED);
        bdate->setAlignment(Qt::AlignRight);
        right->addWidget(bdate);
        bhl->addLayout(right);

        pvl->addWidget(body);

        // Footer bar
        auto* foot = new QLabel("© 2024-2026 Fincept Corporation. All rights reserved.");
        foot->setStyleSheet(
            "color: #525252; font-size: 11px; background: #111111; "
            "padding: 6px 14px; border-top: 1px solid #1a1a1a; "
            "font-family: 'Consolas','Courier New',monospace;");
        pvl->addWidget(foot);

        vl->addWidget(panel);
    }

    // ── License — two columns ─────────────────────────────────────────────────
    {
        auto* row = new QWidget;
        row->setStyleSheet("background: transparent;");
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(12);

        // Open Source
        {
            auto* panel = makePanel();
            auto* pvl = new QVBoxLayout(panel);
            pvl->setContentsMargins(0, 0, 0, 0);
            pvl->setSpacing(0);
            pvl->addWidget(makePanelHeader("📄", "OPEN SOURCE LICENSE", ui::colors::POSITIVE));

            auto* body = new QWidget;
            body->setStyleSheet("background: transparent;");
            auto* bvl = new QVBoxLayout(body);
            bvl->setContentsMargins(14, 10, 14, 10);
            bvl->setSpacing(6);
            bvl->addWidget(makeBullet("AGPL-3.0-or-later"));
            bvl->addWidget(makeBullet("Free for personal & educational use"));
            bvl->addWidget(makeBullet("Share modifications under same license"));
            bvl->addWidget(makeBullet("Network use counts as distribution"));
            pvl->addWidget(body);

            // Footer link
            auto* foot = new QLabel("gnu.org/licenses/agpl-3.0");
            foot->setStyleSheet(
                "color: #0891b2; font-size: 11px; background: transparent; "
                "padding: 6px 14px; border-top: 1px solid #1a1a1a; "
                "font-family: 'Consolas','Courier New',monospace;");
            pvl->addWidget(foot);

            rl->addWidget(panel, 1);
        }

        // Commercial
        {
            auto* panel = makePanel();
            auto* pvl = new QVBoxLayout(panel);
            pvl->setContentsMargins(0, 0, 0, 0);
            pvl->setSpacing(0);
            pvl->addWidget(makePanelHeader("★", "COMMERCIAL LICENSE", ui::colors::AMBER));

            auto* body = new QWidget;
            body->setStyleSheet("background: transparent;");
            auto* bvl = new QVBoxLayout(body);
            bvl->setContentsMargins(14, 10, 14, 10);
            bvl->setSpacing(6);
            bvl->addWidget(makeBullet("Required for commercial deployment"));
            bvl->addWidget(makeBullet("No source sharing required"));
            bvl->addWidget(makeBullet("Priority support included"));
            bvl->addWidget(makeBullet("Custom integration options available"));
            pvl->addWidget(body);

            // Footer link
            auto* foot = new QLabel("support@fincept.in");
            foot->setStyleSheet(
                "color: #0891b2; font-size: 11px; background: transparent; "
                "padding: 6px 14px; border-top: 1px solid #1a1a1a; "
                "font-family: 'Consolas','Courier New',monospace;");
            pvl->addWidget(foot);

            rl->addWidget(panel, 1);
        }

        vl->addWidget(row);
    }

    // ── Trademarks ────────────────────────────────────────────────────────────
    {
        auto* panel = makePanel();
        auto* pvl = new QVBoxLayout(panel);
        pvl->setContentsMargins(0, 0, 0, 0);
        pvl->setSpacing(0);
        pvl->addWidget(makePanelHeader("🛡", "TRADEMARKS", ui::colors::AMBER));

        auto* body = new QWidget;
        body->setStyleSheet("background: transparent;");
        auto* bvl = new QVBoxLayout(body);
        bvl->setContentsMargins(14, 10, 14, 12);
        bvl->setSpacing(6);

        auto* desc = new QLabel(
            "\"Fincept\", \"Fincept Terminal\", and associated logos are trademarks of "
            "Fincept Corporation. Use of these marks requires explicit written permission.");
        desc->setStyleSheet(BODY);
        desc->setWordWrap(true);
        bvl->addWidget(desc);

        auto* perm = new QLabel(
            "Permission is not granted to use Fincept trademarks in a way that suggests "
            "affiliation with or endorsement by Fincept Corporation without prior written consent.");
        perm->setStyleSheet(MUTED);
        perm->setWordWrap(true);
        bvl->addWidget(perm);

        pvl->addWidget(body);
        vl->addWidget(panel);
    }

    // ── Resources — 3×2 grid ─────────────────────────────────────────────────
    {
        auto* panel = makePanel();
        auto* pvl = new QVBoxLayout(panel);
        pvl->setContentsMargins(0, 0, 0, 0);
        pvl->setSpacing(0);
        pvl->addWidget(makePanelHeader("🌐", "RESOURCES", ui::colors::AMBER));

        auto* body = new QWidget;
        body->setStyleSheet("background: transparent;");
        auto* grid = new QGridLayout(body);
        grid->setContentsMargins(14, 10, 14, 12);
        grid->setSpacing(8);

        struct Link { QString label; QString url; };
        const Link links[] = {
            { "GitHub Repository",   "https://github.com/Fincept-Corporation/FinceptTerminal" },
            { "License (AGPL-3.0)", "https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE" },
            { "Commercial License",  "https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md" },
            { "Trademark Policy",    "https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/TRADEMARK.md" },
            { "Contributor CLA",     "https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/CLA.md" },
            { "Official Website",    "https://fincept.in" },
        };

        for (int i = 0; i < 6; ++i) {
            auto* btn = new QPushButton(links[i].label);
            btn->setStyleSheet(LINK_BTN);
            btn->setFixedHeight(36);
            const QString url = links[i].url;
            connect(btn, &QPushButton::clicked, this, [url]() {
                QDesktopServices::openUrl(QUrl(url));
            });
            grid->addWidget(btn, i / 3, i % 3);
        }

        pvl->addWidget(body);
        vl->addWidget(panel);
    }

    // ── Contact ───────────────────────────────────────────────────────────────
    {
        auto* panel = makePanel();
        auto* pvl = new QVBoxLayout(panel);
        pvl->setContentsMargins(0, 0, 0, 0);
        pvl->setSpacing(0);
        pvl->addWidget(makePanelHeader("✉", "CONTACT", ui::colors::AMBER));

        auto* body = new QWidget;
        body->setStyleSheet("background: transparent;");
        auto* grid = new QGridLayout(body);
        grid->setContentsMargins(14, 10, 14, 12);
        grid->setSpacing(12);

        struct Contact { QString label; QString email; };
        const Contact contacts[] = {
            { "GENERAL",    "support@fincept.in" },
            { "COMMERCIAL", "support@fincept.in" },
            { "SECURITY",   "support@fincept.in" },
            { "LEGAL",      "support@fincept.in" },
        };

        for (int i = 0; i < 4; ++i) {
            auto* col = new QWidget;
            col->setStyleSheet("background: transparent;");
            auto* cvl = new QVBoxLayout(col);
            cvl->setContentsMargins(0, 0, 0, 0);
            cvl->setSpacing(2);

            auto* lbl = new QLabel(contacts[i].label);
            lbl->setStyleSheet(SECTION_LABEL);
            cvl->addWidget(lbl);

            auto* email = new QLabel(contacts[i].email);
            email->setStyleSheet(LINK_STYLE);
            cvl->addWidget(email);

            grid->addWidget(col, 0, i);
        }

        pvl->addWidget(body);
        vl->addWidget(panel);
    }

    vl->addStretch();
    scroll->setWidget(page);
    root->addWidget(scroll, 1);

}

} // namespace fincept::screens
