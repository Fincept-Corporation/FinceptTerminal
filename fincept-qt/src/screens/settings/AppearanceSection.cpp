// AppearanceSection.cpp — theme / typography / density / interface toggles.

#include "screens/settings/AppearanceSection.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "screens/settings/SettingsRowHelpers.h"
#include "screens/settings/SettingsStyles.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/LoadingSkeleton.h"

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
constexpr const char* kDefaultTheme = "Dark";
constexpr const char* kDefaultFontSize = "14px";
constexpr const char* kDefaultFontFamily = ui::ThemeManager::kDefaultFontFamily;
constexpr const char* kDefaultDensity = "Default";

/// Dynamic-property guard set by reload() when a settings *read* failed. While
/// it is set, Save is disabled and its handler refuses to write — the widgets
/// hold defaults, not the user's values, and set() is an INSERT OR REPLACE.
constexpr const char* kAppearanceReadFailedProp = "fincept_appearance_read_failed";

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

    // ── THEME ─────────────────────────────────────────────────────────────────
    theme_title_ = new QLabel(tr("THEME"));
    theme_title_->setStyleSheet(section_title_ss());
    vl->addWidget(theme_title_);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    // Item data is the stored value ("Dark" / "Light" / "System"); the text
    // is what the user reads and is re-translated in retranslateUi().
    app_theme_ = new QComboBox;
    app_theme_->addItem(tr("Dark"), QStringLiteral("Dark"));
    app_theme_->addItem(tr("Light"), QStringLiteral("Light"));
    app_theme_->addItem(tr("System"), QStringLiteral("System"));
    app_theme_->setStyleSheet(combo_ss());
    auto* theme_row = make_row(tr("Colour Theme"), app_theme_,
                               tr("Dark, Light, or System to follow your computer's light / dark setting."));
    capture_row_labels(theme_row, &theme_label_, &theme_desc_);
    vl->addWidget(theme_row);

    app_density_ = new QComboBox;
    app_density_->addItems(ui::ThemeManager::available_densities());
    app_density_->setCurrentText("Default");
    app_density_->setStyleSheet(combo_ss());
    auto* density_row =
        make_row(tr("Content Density"), app_density_, tr("Controls padding and spacing throughout the UI."));
    capture_row_labels(density_row, &density_label_, &density_desc_);
    vl->addWidget(density_row);

    vl->addSpacing(8);

    // ── TYPOGRAPHY ────────────────────────────────────────────────────────────
    typography_title_ = new QLabel(tr("TYPOGRAPHY"));
    typography_title_->setStyleSheet(sub_title_ss());
    vl->addWidget(typography_title_);
    vl->addSpacing(4);

    app_font_size_ = new QComboBox;
    for (int px = ui::ThemeManager::kMinFontPx; px <= ui::ThemeManager::kMaxFontPx; ++px)
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
        // Fixed-width families first: the terminal is laid out for them
        // (aligned columns of figures), so they are the recommended choices.
        QStringList mono, other;
        for (const QString& f : QFontDatabase::families()) {
            if (f.startsWith('.') || f.contains("emoji", Qt::CaseInsensitive))
                continue;
            (QFontDatabase::isFixedPitch(f) ? mono : other) << f;
        }
        app_font_family_->addItems(mono);
        if (!mono.isEmpty() && !other.isEmpty())
            app_font_family_->insertSeparator(app_font_family_->count());
        app_font_family_->addItems(other);
    }
    // setCurrentText() is a no-op when the default isn't installed (Consolas is
    // Windows-only), which previously left the combo on item 0 — and with the
    // '.'-prefixed fonts sorted to the top, item 0 was the emoji font, so a save
    // persisted it. Fall back to the first available monospace instead.
    if (app_font_family_->findText(kDefaultFontFamily) >= 0) {
        app_font_family_->setCurrentText(kDefaultFontFamily);
    } else {
        for (const char* mono : {"Menlo", "SF Mono", "Monaco", "Cascadia Mono", "DejaVu Sans Mono", "Courier New"}) {
            int idx = app_font_family_->findText(QString::fromLatin1(mono));
            if (idx >= 0) {
                app_font_family_->setCurrentIndex(idx);
                break;
            }
        }
    }
    app_font_family_->setStyleSheet(combo_ss());
    auto* font_family_row =
        make_row(tr("Font Family"), app_font_family_,
                 tr("Fixed-width fonts are listed first — they keep columns of figures aligned."));
    capture_row_labels(font_family_row, &font_family_label_, &font_family_desc_);
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
    connect(appearance_debounce_, &QTimer::timeout, this, [this]() { apply_from_controls(); });

    auto restart_debounce = [this]() { appearance_debounce_->start(); };
    connect(app_font_size_, &QComboBox::currentTextChanged, this, restart_debounce);
    connect(app_font_family_, &QComboBox::currentTextChanged, this, restart_debounce);
    connect(app_density_, &QComboBox::currentTextChanged, this, restart_debounce);
    connect(app_theme_, &QComboBox::currentIndexChanged, this, restart_debounce);

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
    auto* ticker_bar_row =
        make_row(tr("Ticker Bar"), ticker_bar_toggle_, tr("Scrolling price ticker on the Dashboard. Applies the next time the Dashboard is shown."));
    capture_row_labels(ticker_bar_row, &ticker_bar_label_, &ticker_bar_desc_);
    vl->addWidget(ticker_bar_row);

    animations_toggle_ = new QCheckBox(tr("Enable Animations"));
    animations_toggle_->setChecked(true);
    animations_toggle_->setStyleSheet(check_ss());
    auto* animations_row =
        make_row(tr("Animations"), animations_toggle_, tr("Scrolls the Dashboard ticker and animates loading placeholders; when off the ticker shows its "
               "symbols without moving and placeholders stay still."));
    capture_row_labels(animations_row, &animations_label_, &animations_desc_);
    vl->addWidget(animations_row);

    vl->addSpacing(16);

    // ── SAVE ──────────────────────────────────────────────────────────────────
    // Accessibility: every control here is a bare combo/checkbox next to a
    // separate QLabel, which screen readers do not associate automatically.
    app_theme_->setAccessibleName(tr("Colour theme"));
    app_font_size_->setAccessibleName(tr("Font size"));
    app_font_family_->setAccessibleName(tr("Font family"));
    app_density_->setAccessibleName(tr("Content density"));
    chat_bubble_toggle_->setAccessibleName(tr("Show AI chat bubble"));
    ticker_bar_toggle_->setAccessibleName(tr("Show ticker bar"));
    animations_toggle_->setAccessibleName(tr("Enable animations"));

    save_btn_ = new QPushButton(tr("Save Settings"));
    save_btn_->setMinimumWidth(160);
    save_btn_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    save_btn_->setAccessibleName(tr("Save appearance settings"));
    save_btn_->setStyleSheet(btn_primary_ss());
    connect(save_btn_, &QPushButton::clicked, this, [this]() {
        if (property(kAppearanceReadFailedProp).toBool()) {
            LOG_WARN("Settings", logf("Appearance settings not saved: the stored values could not be read, so the "
                                      "widgets show defaults",
                                      {{"section", "appearance"}, {"error", "read failed at load (logged then)"}}));
            return;
        }

        auto& repo = SettingsRepository::instance();

        repo.set("appearance.theme", app_theme_->currentData().toString(), "appearance");
        repo.set("appearance.font_size", app_font_size_->currentText(), "appearance");
        repo.set("appearance.font_family", app_font_family_->currentText(), "appearance");
        repo.set("appearance.density", app_density_->currentText(), "appearance");
        const QString bubble = chat_bubble_toggle_->isChecked() ? "true" : "false";
        const auto prev_bubble = repo.get("appearance.show_chat_bubble");
        const bool bubble_changed = !prev_bubble.is_ok() || prev_bubble.value() != bubble;
        repo.set("appearance.show_chat_bubble", bubble, "appearance");
        // Every WindowFrame re-applies its chat bubble on this event, so the toggle
        // takes effect now instead of on the next screen change.
        if (bubble_changed)
            EventBus::instance().publish("settings.changed",
                                         {{"key", QStringLiteral("appearance.show_chat_bubble")}, {"value", bubble}});
        repo.set("appearance.show_ticker_bar", ticker_bar_toggle_->isChecked() ? "true" : "false", "appearance");
        repo.set("appearance.animations", animations_toggle_->isChecked() ? "true" : "false", "appearance");
        // Loading placeholders (shimmer) follow the same switch, app-wide and at once.
        fincept::ui::set_motion_enabled(animations_toggle_->isChecked());

        // Flush any pending debounce immediately on save. Same coalesced +
        // queued path as the live-preview lambda above so a Save click that
        // happens while a combo box just changed cannot trigger the
        // back-to-back-restyle Wayland crash.
        if (appearance_debounce_->isActive()) {
            appearance_debounce_->stop();
            apply_from_controls();
        }

        LOG_INFO("Settings", "Appearance saved and applied");
    });
    vl->addWidget(save_btn_);
    vl->addStretch();

    scroll->setWidget(page);
    root->addWidget(scroll);
}

void AppearanceSection::apply_from_controls() {
    if (!app_theme_ || !app_font_size_ || !app_font_family_ || !app_density_)
        return;
    const QString theme = app_theme_->currentData().toString();
    const QString family = app_font_family_->currentText();
    const QString density = app_density_->currentText();
    int px = QString(app_font_size_->currentText()).replace("px", "").toInt();
    if (px <= 0)
        px = ui::ThemeManager::kDefaultFontPx;
    // Skip families that aren't actually installed — Qt would otherwise
    // hand QFont an invalid family which on some Wayland font backends
    // turns into a setPointSizeF(0) / null-glyph crash.
    if (!family.isEmpty() && !QFontDatabase::families().contains(family))
        return;

    // Queued so the combo box's own event chain fully unwinds before every
    // widget in the app — including that combo box — is re-styled, and one
    // coalesced apply so there is a single global restyle (issue #247).
    QPointer<AppearanceSection> guard(this);
    QMetaObject::invokeMethod(
        qApp,
        [guard, theme, family, px, density]() {
            if (!guard)
                return;
            ui::ThemeManager::instance().apply_appearance(theme, family, px, density);
        },
        Qt::QueuedConnection);
}

void AppearanceSection::reload() {
    if (!app_font_size_)
        return;
    auto& repo = SettingsRepository::instance();

    // Block signals during load so live-preview slots don't fire for every
    // setCurrentIndex() call — prevents 4+ ThemeManager apply_* calls and
    // the associated theme_changed signal re-rendering every widget on screen.
    const QSignalBlocker b0(app_theme_);
    const QSignalBlocker b1(app_font_size_);
    const QSignalBlocker b2(app_font_family_);
    const QSignalBlocker b4(app_density_);

    // A read error is not "unset". The Save handler writes every widget on this
    // page straight back, so a failed read must disarm Save; the default is
    // still used in memory so the page renders something sensible.
    bool read_failed = false;
    auto log_read_error = [&read_failed](const QString& key, const std::string& err) {
        LOG_ERROR("Settings", QString("settings read failed for '%1' — showing the default, leaving the stored "
                                      "value untouched and disabling Save: %2")
                                  .arg(key, QString::fromStdString(err)));
        read_failed = true;
    };

    auto load_combo = [&](QComboBox* cb, const QString& key, const QString& def) {
        auto r = repo.get(key);
        if (r.is_err())
            log_read_error(key, r.error());
        const QString val = r.is_ok() ? r.value() : def;
        // An empty stored value must not match: the font list's separator row
        // has empty text, and selecting it leaves the box blank.
        const int idx = val.isEmpty() ? -1 : cb->findText(val);
        if (idx >= 0)
            cb->setCurrentIndex(idx);
    };

    auto load_check = [&](QCheckBox* cb, const QString& key, bool def) {
        if (!cb)
            return;
        auto r = repo.get(key);
        if (r.is_err()) {
            log_read_error(key, r.error());
            cb->setChecked(def);
            return;
        }
        cb->setChecked(r.value() != "false");
    };

    {
        auto r = repo.get("appearance.theme");
        if (r.is_err())
            log_read_error("appearance.theme", r.error());
        const QString val = r.is_ok() && !r.value().isEmpty() ? r.value() : QString::fromLatin1(kDefaultTheme);
        const int idx = app_theme_->findData(val);
        app_theme_->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    load_combo(app_font_size_, "appearance.font_size", kDefaultFontSize);
    load_combo(app_font_family_, "appearance.font_family", kDefaultFontFamily);
    load_combo(app_density_, "appearance.density", kDefaultDensity);

    // A previewed change that was not saved stays in effect when the page is
    // left, so the boxes show what is active rather than what is stored.
    // Otherwise the page says "Dark" over a light terminal, and choosing
    // "Dark" there does nothing because the box did not change.
    {
        const auto& tm = ui::ThemeManager::instance();
        auto show_active = [](QComboBox* cb, int idx) {
            if (idx >= 0)
                cb->setCurrentIndex(idx);
        };
        show_active(app_theme_, app_theme_->findData(tm.theme_choice()));
        show_active(app_font_size_, app_font_size_->findText(QString::number(tm.font_size_px()) + QStringLiteral("px")));
        show_active(app_font_family_, app_font_family_->findText(tm.font_family()));
        show_active(app_density_, app_density_->findText(tm.density()));
    }

    load_check(chat_bubble_toggle_, "appearance.show_chat_bubble", true);
    load_check(ticker_bar_toggle_, "appearance.show_ticker_bar", true);
    load_check(animations_toggle_, "appearance.animations", true);

    setProperty(kAppearanceReadFailedProp, read_failed);
    if (save_btn_) {
        save_btn_->setEnabled(!read_failed);
        save_btn_->setToolTip(read_failed ? tr("Your saved appearance settings could not be read — saving is "
                                               "disabled so the defaults shown cannot overwrite them. Reopen "
                                               "Settings to retry.")
                                          : tr("Theme, font and density changes preview immediately; Save keeps them — "
                                               "and the toggles — across restarts."));
    }
}

void AppearanceSection::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void AppearanceSection::retranslateUi() {
    // Section titles.
    if (typography_title_)
        typography_title_->setText(tr("TYPOGRAPHY"));
    if (theme_title_)
        theme_title_->setText(tr("THEME"));
    if (interface_title_)
        interface_title_->setText(tr("INTERFACE"));

    // Theme row: label, description and the three choices.
    if (theme_label_)
        theme_label_->setText(tr("Colour Theme"));
    if (theme_desc_)
        theme_desc_->setText(tr("Dark, Light, or System to follow your computer's light / dark setting."));
    if (app_theme_ && app_theme_->count() == 3) {
        app_theme_->setItemText(0, tr("Dark"));
        app_theme_->setItemText(1, tr("Light"));
        app_theme_->setItemText(2, tr("System"));
    }
    if (font_family_desc_)
        font_family_desc_->setText(tr("Fixed-width fonts are listed first — they keep columns of figures aligned."));

    // Typography rows.
    if (font_size_label_)
        font_size_label_->setText(tr("Font Size"));
    if (font_family_label_)
        font_family_label_->setText(tr("Font Family"));

    // Density row + description.
    if (density_label_)
        density_label_->setText(tr("Content Density"));
    if (density_desc_)
        density_desc_->setText(tr("Controls padding and spacing throughout the UI."));

    // Interface rows: row labels, checkbox texts, and descriptions.
    if (chat_bubble_label_)
        chat_bubble_label_->setText(tr("AI Chat Bubble"));
    if (chat_bubble_toggle_)
        chat_bubble_toggle_->setText(tr("Show AI Chat Bubble"));
    if (chat_bubble_desc_)
        chat_bubble_desc_->setText(tr("Floating chat assistant in the bottom-right corner."));

    if (ticker_bar_label_)
        ticker_bar_label_->setText(tr("Ticker Bar"));
    if (ticker_bar_toggle_)
        ticker_bar_toggle_->setText(tr("Show Ticker Bar"));
    if (ticker_bar_desc_)
        ticker_bar_desc_->setText(tr("Scrolling price ticker on the Dashboard. Applies the next time the Dashboard is shown."));

    if (animations_label_)
        animations_label_->setText(tr("Animations"));
    if (animations_toggle_)
        animations_toggle_->setText(tr("Enable Animations"));
    if (animations_desc_)
        animations_desc_->setText(tr("Scrolls the Dashboard ticker and animates loading placeholders; when off the ticker shows its "
               "symbols without moving and placeholders stay still."));

    // Save button.
    if (save_btn_)
        save_btn_->setText(tr("Save Settings"));
}

} // namespace fincept::screens
