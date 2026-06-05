// AppearanceSection.cpp — typography / density / interface toggles.

#include "screens/settings/AppearanceSection.h"

#include "core/logging/Logger.h"
#include "screens/settings/SettingsRowHelpers.h"
#include "screens/settings/SettingsStyles.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QApplication>
#include <QFontDatabase>
#include <QLabel>
#include <QMetaObject>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QString>
#include <QVBoxLayout>

namespace fincept::screens {

namespace {
constexpr const char* kDefaultFontSize   = "14px";
constexpr const char* kDefaultFontFamily = "Consolas";
constexpr const char* kDefaultDensity    = "Default";

// make_row() (shared helper in SettingsRowHelpers.h) builds the label — and the
// optional description — QLabel internally and only returns the row widget. To
// re-translate those at runtime we grab them back from the row's direct child
// QLabels. Construction order in make_row is deterministic: the title label is
// added first, the description label (when present) second, so findChildren
// returns them in that order.
//
// Outputs are written only when found; pass nullptr for desc_out when the row
// has no description.
void capture_row_labels(QWidget* row, QLabel** label_out, QLabel** desc_out = nullptr) {
    const auto labels = row->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly);
    if (!labels.isEmpty() && label_out)
        *label_out = labels.at(0);
    if (labels.size() > 1 && desc_out)
        *desc_out = labels.at(1);
}
} // namespace

AppearanceSection::AppearanceSection(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void AppearanceSection::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    reload();
}

void AppearanceSection::build_ui() {
    using namespace settings_styles;
    using namespace settings_helpers;

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QString("QScrollArea { border: none; background: transparent; }"
                                  "QScrollBar:vertical { background: %1; width: 6px; }"
                                  "QScrollBar::handle:vertical { background: %2; }"
                                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
                              .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_MED()));

    auto* page = new QWidget(this);
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(8);

    // ── TYPOGRAPHY ────────────────────────────────────────────────────────────
    typography_title_ = new QLabel(tr("TYPOGRAPHY"));
    typography_title_->setStyleSheet(section_title_ss());
    vl->addWidget(typography_title_);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    app_font_size_ = new QComboBox;
    for (int px = 8; px <= 24; ++px)
        app_font_size_->addItem(QString("%1px").arg(px));
    app_font_size_->setCurrentText(kDefaultFontSize);
    app_font_size_->setStyleSheet(combo_ss());
    auto* font_size_row = make_row(tr("Font Size"), app_font_size_);
    capture_row_labels(font_size_row, &font_size_label_);
    vl->addWidget(font_size_row);

    app_font_family_ = new QComboBox;
    // QFontDatabase::families() on macOS includes hidden system families
    // prefixed with '.' (".Apple Color Emoji UI", ".AppleSystemUIFont", …).
    // Offering them is a trap: the emoji font carries ASCII digit glyphs
    // (keycap-emoji bases) that render grossly letter-spaced, so picking it
    // mangles every number in the UI. Exclude private and emoji families.
    {
        QStringList fams;
        for (const QString& f : QFontDatabase::families())
            if (!f.startsWith('.') && !f.contains("emoji", Qt::CaseInsensitive))
                fams << f;
        app_font_family_->addItems(fams);
    }
    // setCurrentText() is a no-op when the default isn't installed (Consolas is
    // Windows-only), which previously left the combo on item 0 — and with the
    // '.'-prefixed fonts sorted to the top, item 0 was the emoji font, so a save
    // persisted it. Fall back to the first available monospace instead.
    if (app_font_family_->findText(kDefaultFontFamily) >= 0) {
        app_font_family_->setCurrentText(kDefaultFontFamily);
    } else {
        for (const char* mono : {"Menlo", "SF Mono", "Monaco", "Cascadia Mono",
                                  "DejaVu Sans Mono", "Courier New"}) {
            int idx = app_font_family_->findText(QString::fromLatin1(mono));
            if (idx >= 0) { app_font_family_->setCurrentIndex(idx); break; }
        }
    }
    app_font_family_->setStyleSheet(combo_ss());
    auto* font_family_row = make_row(tr("Font Family"), app_font_family_);
    capture_row_labels(font_family_row, &font_family_label_);
    vl->addWidget(font_family_row);

    // Debounced live preview — coalesce rapid changes into one apply after 300ms idle.
    //
    // Two safety properties beyond the debounce window matter for KDE Plasma 6 +
    // Qt6 + Wayland (issue #247: app segfaulted on Font Size / Font Family change):
    //
    //   1. apply_typography_and_density() coalesces font + density into ONE
    //      qApp->setStyleSheet() call. Two back-to-back global restyles during
    //      a QComboBox event chain are a documented crash trigger on Wayland —
    //      the second restyle mutates surface state while the popup is still
    //      tearing down.
    //
    //   2. The apply is dispatched via Qt::QueuedConnection (singleShot(0))
    //      so the QComboBox::currentTextChanged → debounce::timeout call stack
    //      fully unwinds before we re-style every widget in the app, including
    //      the combo box that started the chain.
    appearance_debounce_ = new QTimer(this);
    appearance_debounce_->setSingleShot(true);
    appearance_debounce_->setInterval(300);
    connect(appearance_debounce_, &QTimer::timeout, this, [this]() {
        if (!app_font_size_ || !app_font_family_ || !app_density_)
            return;
        const QString family  = app_font_family_->currentText();
        const QString density = app_density_->currentText();
        const int px = QString(app_font_size_->currentText()).replace("px", "").toInt();
        // Skip families that aren't actually installed — Qt would otherwise
        // hand QFont an invalid family which on some Wayland font backends
        // turns into a setPointSizeF(0) / null-glyph crash.
        if (!family.isEmpty() && !QFontDatabase::families().contains(family))
            return;

        QPointer<AppearanceSection> guard(this);
        QMetaObject::invokeMethod(qApp, [guard, family, px, density]() {
            if (!guard)
                return;
            ui::ThemeManager::instance().apply_typography_and_density(family, px, density);
        }, Qt::QueuedConnection);
    });

    auto restart_debounce = [this]() { appearance_debounce_->start(); };
    connect(app_font_size_,   &QComboBox::currentTextChanged, this, restart_debounce);
    connect(app_font_family_, &QComboBox::currentTextChanged, this, restart_debounce);

    vl->addSpacing(8);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    // ── THEME ─────────────────────────────────────────────────────────────────
    theme_title_ = new QLabel(tr("THEME"));
    theme_title_->setStyleSheet(sub_title_ss());
    vl->addWidget(theme_title_);
    vl->addSpacing(4);

    app_density_ = new QComboBox;
    app_density_->addItems(ui::ThemeManager::available_densities());
    app_density_->setCurrentText("Default");
    app_density_->setStyleSheet(combo_ss());
    auto* density_row = make_row(tr("Content Density"), app_density_, tr("Controls padding and spacing throughout the UI."));
    capture_row_labels(density_row, &density_label_, &density_desc_);
    vl->addWidget(density_row);

    connect(app_density_, &QComboBox::currentTextChanged, this, restart_debounce);

    vl->addSpacing(8);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    // ── INTERFACE ─────────────────────────────────────────────────────────────
    interface_title_ = new QLabel(tr("INTERFACE"));
    interface_title_->setStyleSheet(sub_title_ss());
    vl->addWidget(interface_title_);
    vl->addSpacing(4);

    chat_bubble_toggle_ = new QCheckBox(tr("Show AI Chat Bubble"));
    chat_bubble_toggle_->setChecked(true);
    chat_bubble_toggle_->setStyleSheet(check_ss());
    auto* chat_bubble_row =
        make_row(tr("AI Chat Bubble"), chat_bubble_toggle_, tr("Floating chat assistant in the bottom-right corner."));
    capture_row_labels(chat_bubble_row, &chat_bubble_label_, &chat_bubble_desc_);
    vl->addWidget(chat_bubble_row);

    ticker_bar_toggle_ = new QCheckBox(tr("Show Ticker Bar"));
    ticker_bar_toggle_->setChecked(true);
    ticker_bar_toggle_->setStyleSheet(check_ss());
    auto* ticker_bar_row = make_row(tr("Ticker Bar"), ticker_bar_toggle_, tr("Live price ticker at the bottom of the screen."));
    capture_row_labels(ticker_bar_row, &ticker_bar_label_, &ticker_bar_desc_);
    vl->addWidget(ticker_bar_row);

    animations_toggle_ = new QCheckBox(tr("Enable Animations"));
    animations_toggle_->setChecked(true);
    animations_toggle_->setStyleSheet(check_ss());
    auto* animations_row = make_row(tr("Animations"), animations_toggle_, tr("Fade and transition effects throughout the UI."));
    capture_row_labels(animations_row, &animations_label_, &animations_desc_);
    vl->addWidget(animations_row);

    vl->addSpacing(16);

    // ── SAVE ──────────────────────────────────────────────────────────────────
    save_btn_ = new QPushButton(tr("Save Settings"));
    save_btn_->setFixedWidth(160);
    save_btn_->setStyleSheet(btn_primary_ss());
    connect(save_btn_, &QPushButton::clicked, this, [this]() {
        auto& repo = SettingsRepository::instance();

        repo.set("appearance.font_size",         app_font_size_->currentText(),                            "appearance");
        repo.set("appearance.font_family",       app_font_family_->currentText(),                          "appearance");
        repo.set("appearance.density",           app_density_->currentText(),                              "appearance");
        repo.set("appearance.show_chat_bubble",  chat_bubble_toggle_->isChecked() ? "true" : "false",      "appearance");
        repo.set("appearance.show_ticker_bar",   ticker_bar_toggle_->isChecked()  ? "true" : "false",      "appearance");
        repo.set("appearance.animations",        animations_toggle_->isChecked()  ? "true" : "false",      "appearance");

        // Flush any pending debounce immediately on save. Same coalesced +
        // queued path as the live-preview lambda above so a Save click that
        // happens while a combo box just changed cannot trigger the
        // back-to-back-restyle Wayland crash.
        if (appearance_debounce_->isActive()) {
            appearance_debounce_->stop();
            const QString family  = app_font_family_->currentText();
            const QString density = app_density_->currentText();
            int px = QString(app_font_size_->currentText()).replace("px", "").toInt();
            if (px <= 0) px = 14;
            QPointer<AppearanceSection> guard(this);
            QMetaObject::invokeMethod(qApp, [guard, family, px, density]() {
                if (!guard)
                    return;
                ui::ThemeManager::instance().apply_typography_and_density(family, px, density);
            }, Qt::QueuedConnection);
        }

        LOG_INFO("Settings", "Appearance saved and applied");
    });
    vl->addWidget(save_btn_);
    vl->addStretch();

    scroll->setWidget(page);
    root->addWidget(scroll);
}

void AppearanceSection::reload() {
    if (!app_font_size_) return;
    auto& repo = SettingsRepository::instance();

    // Block signals during load so live-preview slots don't fire for every
    // setCurrentIndex() call — prevents 4+ ThemeManager apply_* calls and
    // the associated theme_changed signal re-rendering every widget on screen.
    const QSignalBlocker b1(app_font_size_);
    const QSignalBlocker b2(app_font_family_);
    const QSignalBlocker b4(app_density_);

    auto load_combo = [&](QComboBox* cb, const QString& key, const QString& def) {
        auto r = repo.get(key);
        QString val = r.is_ok() ? r.value() : def;
        int idx = cb->findText(val);
        if (idx >= 0) cb->setCurrentIndex(idx);
    };

    auto load_check = [&](QCheckBox* cb, const QString& key, bool def) {
        if (!cb) return;
        auto r = repo.get(key);
        cb->setChecked(!r.is_ok() ? def : r.value() != "false");
    };

    load_combo(app_font_size_,   "appearance.font_size",   kDefaultFontSize);
    load_combo(app_font_family_, "appearance.font_family", kDefaultFontFamily);
    load_combo(app_density_,     "appearance.density",     kDefaultDensity);

    load_check(chat_bubble_toggle_, "appearance.show_chat_bubble", true);
    load_check(ticker_bar_toggle_,  "appearance.show_ticker_bar",  true);
    load_check(animations_toggle_,  "appearance.animations",       true);
}

void AppearanceSection::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void AppearanceSection::retranslateUi() {
    // Section titles.
    if (typography_title_) typography_title_->setText(tr("TYPOGRAPHY"));
    if (theme_title_)      theme_title_->setText(tr("THEME"));
    if (interface_title_)  interface_title_->setText(tr("INTERFACE"));

    // Typography rows.
    if (font_size_label_)   font_size_label_->setText(tr("Font Size"));
    if (font_family_label_) font_family_label_->setText(tr("Font Family"));

    // Theme row + description.
    if (density_label_) density_label_->setText(tr("Content Density"));
    if (density_desc_)  density_desc_->setText(tr("Controls padding and spacing throughout the UI."));

    // Interface rows: row labels, checkbox texts, and descriptions.
    if (chat_bubble_label_)  chat_bubble_label_->setText(tr("AI Chat Bubble"));
    if (chat_bubble_toggle_) chat_bubble_toggle_->setText(tr("Show AI Chat Bubble"));
    if (chat_bubble_desc_)   chat_bubble_desc_->setText(tr("Floating chat assistant in the bottom-right corner."));

    if (ticker_bar_label_)  ticker_bar_label_->setText(tr("Ticker Bar"));
    if (ticker_bar_toggle_) ticker_bar_toggle_->setText(tr("Show Ticker Bar"));
    if (ticker_bar_desc_)   ticker_bar_desc_->setText(tr("Live price ticker at the bottom of the screen."));

    if (animations_label_)  animations_label_->setText(tr("Animations"));
    if (animations_toggle_) animations_toggle_->setText(tr("Enable Animations"));
    if (animations_desc_)   animations_desc_->setText(tr("Fade and transition effects throughout the UI."));

    // Save button.
    if (save_btn_) save_btn_->setText(tr("Save Settings"));
}

} // namespace fincept::screens
