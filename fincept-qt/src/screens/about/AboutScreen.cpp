#include "screens/about/AboutScreen.h"

#include "core/config/AppPaths.h"
#include "services/updater/UpdateService.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>

namespace fincept::screens {

// ── Style constants ───────────────────────────────────────────────────────────

static QString SECTION_LABEL() {
    return QString("color: %1; font-size: 11px; font-weight: bold; letter-spacing: 0.5px; "
                   "text-transform: uppercase; background: transparent; "
                   "font-family: 'Consolas','Courier New',monospace;")
        .arg(ui::colors::TEXT_SECONDARY());
}

static QString BODY() {
    return QString("color: %1; font-size: 13px; background: transparent; "
                   "font-family: 'Consolas','Courier New',monospace;")
        .arg(ui::colors::TEXT_PRIMARY());
}

static QString MUTED() {
    return QString("color: %1; font-size: 12px; background: transparent; "
                   "font-family: 'Consolas','Courier New',monospace;")
        .arg(ui::colors::TEXT_TERTIARY());
}

static QString LINK_STYLE() {
    return QString("color: %1; font-size: 13px; background: transparent; "
                   "font-family: 'Consolas','Courier New',monospace;")
        .arg(ui::colors::CYAN());
}

static QString PANEL() {
    return QString("background: %1; border: 1px solid %2; border-radius: 2px;")
        .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM());
}

static QString LINK_BTN() {
    return QString("QPushButton { background: %1; color: %2; border: 1px solid %3; "
                   "border-radius: 2px; padding: 8px 12px; font-size: 12px; text-align: left; "
                   "font-family: 'Consolas','Courier New',monospace; }"
                   "QPushButton:hover { background: %4; color: #38bdf8; }")
        .arg(ui::colors::BG_RAISED(), ui::colors::CYAN(), ui::colors::BORDER_DIM(), ui::colors::BG_HOVER());
}

// ── Helpers ───────────────────────────────────────────────────────────────────

static QWidget* makePanel() {
    auto* w = new QWidget(nullptr);
    w->setStyleSheet(PANEL());
    return w;
}

static QLabel* makePanelHeader(const QString& icon, const QString& title, const QString& iconColor) {
    auto* lbl = new QLabel(icon.isEmpty() ? title : QString("%1  %2").arg(icon, title));
    lbl->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; letter-spacing: 0.5px; "
                               "background: %2; padding: 10px 14px; border-bottom: 1px solid %3; "
                               "font-family: 'Consolas','Courier New',monospace;")
                           .arg(iconColor, ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    return lbl;
}

static QLabel* makeBullet(const QString& text) {
    auto* lbl = new QLabel(QString("■  %1").arg(text));
    lbl->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent; "
                               "font-family: 'Consolas','Courier New',monospace;")
                           .arg(ui::colors::TEXT_SECONDARY()));
    lbl->setWordWrap(true);
    return lbl;
}

// Re-apply icon + title to a panel header (mirrors makePanelHeader's text format).
static void setPanelHeaderText(QLabel* lbl, const QString& icon, const QString& title) {
    if (lbl) lbl->setText(icon.isEmpty() ? title : QString("%1  %2").arg(icon, title));
}

// Re-apply bullet text (mirrors makeBullet's "■  %1" format).
static void setBulletText(QLabel* lbl, const QString& text) {
    if (lbl) lbl->setText(QString("■  %1").arg(text));
}

// ── Constructor ───────────────────────────────────────────────────────────────

AboutScreen::AboutScreen(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("QWidget#AboutRoot { background: %1; }").arg(ui::colors::BG_BASE()));
    setObjectName("AboutRoot");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Scrollable content ────────────────────────────────────────────────────
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    auto* page = new QWidget(this);
    page->setStyleSheet(QString("background: %1;").arg(ui::colors::BG_BASE()));
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(12);

    // ── Version Information ───────────────────────────────────────────────────
    {
        auto* panel = makePanel();
        auto* pvl = new QVBoxLayout(panel);
        pvl->setContentsMargins(0, 0, 0, 0);
        pvl->setSpacing(0);

        version_header_ = makePanelHeader("ℹ", tr("VERSION INFORMATION"), ui::colors::AMBER);
        pvl->addWidget(version_header_);

        auto* body = new QWidget(this);
        body->setStyleSheet("background: transparent;");
        auto* bhl = new QHBoxLayout(body);
        bhl->setContentsMargins(14, 12, 14, 12);

        auto* left = new QVBoxLayout;
        left->setSpacing(4);
        app_name_ = new QLabel(QStringLiteral("Fincept Terminal"));
        app_name_->setStyleSheet(QString("color: %1; font-size: 15px; font-weight: bold; background: transparent; "
                                          "font-family: 'Consolas','Courier New',monospace;")
                                     .arg(ui::colors::TEXT_PRIMARY()));
        left->addWidget(app_name_);
        app_subtitle_ = new QLabel(tr("NATIVE DESKTOP FINANCIAL INTELLIGENCE TERMINAL"));
        app_subtitle_->setStyleSheet(MUTED());
        left->addWidget(app_subtitle_);
        bhl->addLayout(left);
        bhl->addStretch();

        auto* right = new QVBoxLayout;
        right->setSpacing(4);
        right->setAlignment(Qt::AlignRight);
        auto* ver = new QLabel(QStringLiteral("v%1").arg(QApplication::applicationVersion()));
        ver->setStyleSheet(QString("color: %1; font-size: 18px; font-weight: bold; background: transparent; "
                                   "font-family: 'Consolas','Courier New',monospace;")
                               .arg(ui::colors::AMBER()));
        ver->setAlignment(Qt::AlignRight);
        right->addWidget(ver);

        // Check-for-updates button — user-initiated, always shows a result dialog.
        check_btn_ = new QPushButton(tr("Check for Updates"));
        check_btn_->setStyleSheet(LINK_BTN());
        check_btn_->setCursor(Qt::PointingHandCursor);
        connect(check_btn_, &QPushButton::clicked, this, [this]() {
            check_in_progress_ = true;
            check_btn_->setEnabled(false);
            check_btn_->setText(tr("Checking…"));
            auto& svc = services::UpdateService::instance();
            svc.set_dialog_parent(window());
            // Re-enable the button when the check completes. Using a unique
            // connection is fine since the service is a long-lived singleton.
            connect(&svc, &services::UpdateService::check_finished, check_btn_,
                    [this](bool /*found*/) {
                        check_in_progress_ = false;
                        check_btn_->setEnabled(true);
                        check_btn_->setText(tr("Check for Updates"));
                    },
                    Qt::SingleShotConnection);
            svc.check_for_updates(/*silent=*/false);
        });
        right->addWidget(check_btn_);
        bhl->addLayout(right);

        pvl->addWidget(body);

        // Footer bar
        copyright_ = new QLabel(tr("© 2024-2026 Fincept Corporation. All rights reserved."));
        copyright_->setStyleSheet(QString("color: %1; font-size: 11px; background: %2; "
                                          "padding: 6px 14px; border-top: 1px solid %3; "
                                          "font-family: 'Consolas','Courier New',monospace;")
                                      .arg(ui::colors::TEXT_TERTIARY(), ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
        pvl->addWidget(copyright_);

        vl->addWidget(panel);
    }

    // ── License — two columns ─────────────────────────────────────────────────
    {
        auto* row = new QWidget(this);
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
            oss_header_ = makePanelHeader("", tr("OPEN SOURCE LICENSE"), ui::colors::POSITIVE);
            pvl->addWidget(oss_header_);

            auto* body = new QWidget(this);
            body->setStyleSheet("background: transparent;");
            auto* bvl = new QVBoxLayout(body);
            bvl->setContentsMargins(14, 10, 14, 10);
            bvl->setSpacing(6);
            // First bullet is the license SPDX identifier (a code value) — not translated.
            auto* oss_b0 = makeBullet(QStringLiteral("AGPL-3.0-or-later"));
            oss_bullets_ = {oss_b0,
                            makeBullet(tr("Free for personal & educational use")),
                            makeBullet(tr("Share modifications under same license")),
                            makeBullet(tr("Network use counts as distribution"))};
            for (auto* b : oss_bullets_) bvl->addWidget(b);
            pvl->addWidget(body);

            // Footer link
            auto* foot = new QLabel("gnu.org/licenses/agpl-3.0");
            foot->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent; "
                                        "padding: 6px 14px; border-top: 1px solid %2; "
                                        "font-family: 'Consolas','Courier New',monospace;")
                                    .arg(ui::colors::CYAN(), ui::colors::BORDER_DIM()));
            pvl->addWidget(foot);

            rl->addWidget(panel, 1);
        }

        // Commercial
        {
            auto* panel = makePanel();
            auto* pvl = new QVBoxLayout(panel);
            pvl->setContentsMargins(0, 0, 0, 0);
            pvl->setSpacing(0);
            commercial_header_ = makePanelHeader("★", tr("COMMERCIAL LICENSE"), ui::colors::AMBER);
            pvl->addWidget(commercial_header_);

            auto* body = new QWidget(this);
            body->setStyleSheet("background: transparent;");
            auto* bvl = new QVBoxLayout(body);
            bvl->setContentsMargins(14, 10, 14, 10);
            bvl->setSpacing(6);
            commercial_bullets_ = {makeBullet(tr("Required for commercial deployment")),
                                   makeBullet(tr("No source sharing required")),
                                   makeBullet(tr("Priority support included")),
                                   makeBullet(tr("Custom integration options available"))};
            for (auto* b : commercial_bullets_) bvl->addWidget(b);
            pvl->addWidget(body);

            // Footer link
            auto* foot = new QLabel("support@fincept.in");
            foot->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent; "
                                        "padding: 6px 14px; border-top: 1px solid %2; "
                                        "font-family: 'Consolas','Courier New',monospace;")
                                    .arg(ui::colors::CYAN(), ui::colors::BORDER_DIM()));
            pvl->addWidget(foot);

            rl->addWidget(panel, 1);
        }

        vl->addWidget(row);
    }

    // ── Diagnostics ───────────────────────────────────────────────────────────
    // Surfaces the crash-dump folder path so users filing bug reports can
    // attach minidumps without hunting through %LOCALAPPDATA% (see issue #215).
    {
        auto* panel = makePanel();
        auto* pvl = new QVBoxLayout(panel);
        pvl->setContentsMargins(0, 0, 0, 0);
        pvl->setSpacing(0);
        diag_header_ = makePanelHeader("⚙", tr("DIAGNOSTICS"), ui::colors::AMBER);
        pvl->addWidget(diag_header_);

        auto* body = new QWidget(this);
        body->setStyleSheet("background: transparent;");
        auto* bhl = new QHBoxLayout(body);
        bhl->setContentsMargins(14, 10, 14, 10);
        bhl->setSpacing(12);

        auto* left = new QVBoxLayout;
        left->setSpacing(2);
        crash_dumps_label_ = new QLabel(tr("CRASH DUMPS"));
        crash_dumps_label_->setStyleSheet(SECTION_LABEL());
        left->addWidget(crash_dumps_label_);
        const QString crash_dir = QDir::toNativeSeparators(AppPaths::crashdumps());
        auto* path = new QLabel(crash_dir);
        path->setStyleSheet(MUTED());
        path->setTextInteractionFlags(Qt::TextSelectableByMouse);
        path->setWordWrap(true);
        left->addWidget(path);
        bhl->addLayout(left, 1);

        open_folder_btn_ = new QPushButton(tr("Open Folder"));
        open_folder_btn_->setStyleSheet(LINK_BTN());
        open_folder_btn_->setCursor(Qt::PointingHandCursor);
        connect(open_folder_btn_, &QPushButton::clicked, this, [crash_dir]() {
            // mkpath is idempotent; ensures the folder exists before
            // QDesktopServices::openUrl on a freshly installed terminal
            // that hasn't crashed yet.
            QDir().mkpath(crash_dir);
            QDesktopServices::openUrl(QUrl::fromLocalFile(crash_dir));
        });
        bhl->addWidget(open_folder_btn_, 0, Qt::AlignTop);

        pvl->addWidget(body);
        vl->addWidget(panel);
    }

    // ── Trademarks ────────────────────────────────────────────────────────────
    {
        auto* panel = makePanel();
        auto* pvl = new QVBoxLayout(panel);
        pvl->setContentsMargins(0, 0, 0, 0);
        pvl->setSpacing(0);
        trademarks_header_ = makePanelHeader("", tr("TRADEMARKS"), ui::colors::AMBER);
        pvl->addWidget(trademarks_header_);

        auto* body = new QWidget(this);
        body->setStyleSheet("background: transparent;");
        auto* bvl = new QVBoxLayout(body);
        bvl->setContentsMargins(14, 10, 14, 12);
        bvl->setSpacing(6);

        trademarks_desc_ = new QLabel(tr("\"Fincept\", \"Fincept Terminal\", and associated logos are trademarks of "
                                         "Fincept Corporation. Use of these marks requires explicit written permission."));
        trademarks_desc_->setStyleSheet(BODY());
        trademarks_desc_->setWordWrap(true);
        bvl->addWidget(trademarks_desc_);

        trademarks_perm_ =
            new QLabel(tr("Permission is not granted to use Fincept trademarks in a way that suggests "
                          "affiliation with or endorsement by Fincept Corporation without prior written consent."));
        trademarks_perm_->setStyleSheet(MUTED());
        trademarks_perm_->setWordWrap(true);
        bvl->addWidget(trademarks_perm_);

        pvl->addWidget(body);
        vl->addWidget(panel);
    }

    // ── Resources — 3×2 grid ─────────────────────────────────────────────────
    {
        auto* panel = makePanel();
        auto* pvl = new QVBoxLayout(panel);
        pvl->setContentsMargins(0, 0, 0, 0);
        pvl->setSpacing(0);
        resources_header_ = makePanelHeader("", tr("RESOURCES"), ui::colors::AMBER);
        pvl->addWidget(resources_header_);

        auto* body = new QWidget(this);
        body->setStyleSheet("background: transparent;");
        auto* grid = new QGridLayout(body);
        grid->setContentsMargins(14, 10, 14, 12);
        grid->setSpacing(8);

        struct Link {
            QString label;
            QString url;
        };
        const Link links[] = {
            {tr("GitHub Repository"), "https://github.com/Fincept-Corporation/FinceptTerminal"},
            {tr("License (AGPL-3.0)"), "https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE"},
            {tr("Commercial License"),
             "https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md"},
            {tr("Trademark Policy"), "https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/TRADEMARK.md"},
            {tr("Contributor CLA"), "https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/CLA.md"},
            {tr("Official Website"), "https://fincept.in"},
        };

        for (int i = 0; i < 6; ++i) {
            auto* btn = new QPushButton(links[i].label);
            btn->setStyleSheet(LINK_BTN());
            btn->setFixedHeight(36);
            const QString url = links[i].url;
            connect(btn, &QPushButton::clicked, this, [url]() { QDesktopServices::openUrl(QUrl(url)); });
            grid->addWidget(btn, i / 3, i % 3);
            resource_btns_.append(btn);
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
        contact_header_ = makePanelHeader("✉", tr("CONTACT"), ui::colors::AMBER);
        pvl->addWidget(contact_header_);

        auto* body = new QWidget(this);
        body->setStyleSheet("background: transparent;");
        auto* grid = new QGridLayout(body);
        grid->setContentsMargins(14, 10, 14, 12);
        grid->setSpacing(12);

        struct Contact {
            QString label;
            QString email;
        };
        const Contact contacts[] = {
            {tr("GENERAL"), "support@fincept.in"},
            {tr("COMMERCIAL"), "support@fincept.in"},
            {tr("SECURITY"), "support@fincept.in"},
            {tr("LEGAL"), "support@fincept.in"},
        };

        for (int i = 0; i < 4; ++i) {
            auto* col = new QWidget(this);
            col->setStyleSheet("background: transparent;");
            auto* cvl = new QVBoxLayout(col);
            cvl->setContentsMargins(0, 0, 0, 0);
            cvl->setSpacing(2);

            auto* lbl = new QLabel(contacts[i].label);
            lbl->setStyleSheet(SECTION_LABEL());
            cvl->addWidget(lbl);
            contact_labels_.append(lbl);

            auto* email = new QLabel(contacts[i].email);
            email->setStyleSheet(LINK_STYLE());
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

void AboutScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void AboutScreen::retranslateUi() {
    // Version panel
    setPanelHeaderText(version_header_, "ℹ", tr("VERSION INFORMATION"));
    if (app_subtitle_) app_subtitle_->setText(tr("NATIVE DESKTOP FINANCIAL INTELLIGENCE TERMINAL"));
    if (check_btn_)
        check_btn_->setText(check_in_progress_ ? tr("Checking…") : tr("Check for Updates"));
    if (copyright_) copyright_->setText(tr("© 2024-2026 Fincept Corporation. All rights reserved."));

    // Open source license — bullet 0 is the SPDX identifier (untranslated).
    setPanelHeaderText(oss_header_, "", tr("OPEN SOURCE LICENSE"));
    if (oss_bullets_.size() == 4) {
        setBulletText(oss_bullets_[1], tr("Free for personal & educational use"));
        setBulletText(oss_bullets_[2], tr("Share modifications under same license"));
        setBulletText(oss_bullets_[3], tr("Network use counts as distribution"));
    }

    // Commercial license
    setPanelHeaderText(commercial_header_, "★", tr("COMMERCIAL LICENSE"));
    if (commercial_bullets_.size() == 4) {
        setBulletText(commercial_bullets_[0], tr("Required for commercial deployment"));
        setBulletText(commercial_bullets_[1], tr("No source sharing required"));
        setBulletText(commercial_bullets_[2], tr("Priority support included"));
        setBulletText(commercial_bullets_[3], tr("Custom integration options available"));
    }

    // Diagnostics
    setPanelHeaderText(diag_header_, "⚙", tr("DIAGNOSTICS"));
    if (crash_dumps_label_) crash_dumps_label_->setText(tr("CRASH DUMPS"));
    if (open_folder_btn_)   open_folder_btn_->setText(tr("Open Folder"));

    // Trademarks
    setPanelHeaderText(trademarks_header_, "", tr("TRADEMARKS"));
    if (trademarks_desc_)
        trademarks_desc_->setText(tr("\"Fincept\", \"Fincept Terminal\", and associated logos are trademarks of "
                                     "Fincept Corporation. Use of these marks requires explicit written permission."));
    if (trademarks_perm_)
        trademarks_perm_->setText(tr("Permission is not granted to use Fincept trademarks in a way that suggests "
                                     "affiliation with or endorsement by Fincept Corporation without prior written consent."));

    // Resources
    setPanelHeaderText(resources_header_, "", tr("RESOURCES"));
    if (resource_btns_.size() == 6) {
        resource_btns_[0]->setText(tr("GitHub Repository"));
        resource_btns_[1]->setText(tr("License (AGPL-3.0)"));
        resource_btns_[2]->setText(tr("Commercial License"));
        resource_btns_[3]->setText(tr("Trademark Policy"));
        resource_btns_[4]->setText(tr("Contributor CLA"));
        resource_btns_[5]->setText(tr("Official Website"));
    }

    // Contact
    setPanelHeaderText(contact_header_, "✉", tr("CONTACT"));
    if (contact_labels_.size() == 4) {
        contact_labels_[0]->setText(tr("GENERAL"));
        contact_labels_[1]->setText(tr("COMMERCIAL"));
        contact_labels_[2]->setText(tr("SECURITY"));
        contact_labels_[3]->setText(tr("LEGAL"));
    }
}

} // namespace fincept::screens
