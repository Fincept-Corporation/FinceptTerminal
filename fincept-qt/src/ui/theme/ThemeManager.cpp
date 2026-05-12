#include "ui/theme/ThemeManager.h"

#include "core/logging/Logger.h"

#include <QApplication>
#include <QFontInfo>
#include <QWidget>

namespace fincept::ui {

const ThemeTokens THEME_OBSIDIAN = {
    .name = "Obsidian",
    .bg_base = "#080808",
    .bg_surface = "#0a0a0a",
    .bg_raised = "#111111",
    .bg_hover = "#161616",
    .border_dim = "#1a1a1a",
    .border_med = "#222222",
    .border_bright = "#333333",
    .text_primary = "#e5e5e5",
    .text_secondary = "#808080",
    .text_tertiary = "#525252",
    .text_dim = "#404040",
    .accent = "#d97706",
    .accent_dim = "#78350f",
    .text_on_accent = "#ffffff",
    .icon_dim = "#808080",
    .icon_hover = "#e5e5e5",
    .positive = "#16a34a",
    .positive_dim = "#14532d",
    .negative = "#dc2626",
    .negative_dim = "#7f1d1d",
    .warning = "#ca8a04",
    .info = "#2563eb",
    .cyan = "#0891b2",
    .accent_bg = "#1a1000",
    .positive_bg = "#0a1a0f",
    .negative_bg = "#1a0a0a",
    .row_alt = "#0c0c0c",
    .font_family = "'Consolas','Courier New',monospace",
    .font_size_base = 14,
    .chart_colors = {"#d97706", "#0891b2", "#16a34a", "#dc2626", "#2563eb", "#ca8a04"},
};

ThemeManager::ThemeManager() : QObject(nullptr), current_(THEME_OBSIDIAN) {}

ThemeManager& ThemeManager::instance() {
    static ThemeManager inst;
    return inst;
}

QString ThemeManager::qss_font_family_list() const {
    QStringList parts;
    parts.reserve(font_families_.size());
    for (const QString& f : font_families_) {
        // CSS spec: generic families (monospace, serif, sans-serif) are
        // unquoted; named families get double-quoted to survive families
        // with spaces ("SF Mono", "Cascadia Mono").
        if (f.compare("monospace", Qt::CaseInsensitive) == 0 ||
            f.compare("serif", Qt::CaseInsensitive) == 0 ||
            f.compare("sans-serif", Qt::CaseInsensitive) == 0) {
            parts.append(f.toLower());
        } else {
            parts.append(QStringLiteral("\"%1\"").arg(f));
        }
    }
    return parts.join(QStringLiteral(", "));
}

QStringList ThemeManager::build_font_chain(const QString& preferred) {
    // Cross-platform monospace fallback cascade. Qt's font matcher walks
    // this list and picks the first family installed on the running
    // system. The macOS entries (SF Mono / Menlo / Monaco) come first
    // because that's where the original "Consolas only" default broke —
    // macOS doesn't ship Consolas, the silent fallback was a proportional
    // system font, and QLineEdit/QTextEdit rendered with mismatched
    // glyph metrics (the visible "fragmented" text symptom).
    static const QStringList kDefaults = {
        QStringLiteral("SF Mono"),           // macOS Mojave+ (system monospace)
        QStringLiteral("Menlo"),             // macOS standard
        QStringLiteral("Monaco"),            // macOS classic
        QStringLiteral("Cascadia Mono"),     // Windows 11 + Terminal
        QStringLiteral("Cascadia Code"),     // Windows 11
        QStringLiteral("Consolas"),          // Windows
        QStringLiteral("DejaVu Sans Mono"),  // Linux (Debian/Ubuntu)
        QStringLiteral("Liberation Mono"),   // Linux (Fedora/RHEL)
        QStringLiteral("Source Code Pro"),   // Adobe — common 3rd-party
        QStringLiteral("Courier New"),       // universal but ugly last resort
        QStringLiteral("monospace"),         // Qt generic family
    };
    QStringList chain;
    const QString trimmed = preferred.trimmed();
    if (!trimmed.isEmpty()) {
        // Allow the caller to pass a CSS-style list like
        // "'Consolas','Courier New',monospace" without breaking the
        // single-string font matcher — split, strip quotes, dedup.
        const auto parts = trimmed.split(',', Qt::SkipEmptyParts);
        for (const QString& raw : parts) {
            QString name = raw.trimmed();
            if (name.startsWith('\'') || name.startsWith('"'))
                name = name.mid(1);
            if (name.endsWith('\'') || name.endsWith('"'))
                name.chop(1);
            name = name.trimmed();
            if (!name.isEmpty() && !chain.contains(name, Qt::CaseInsensitive))
                chain.append(name);
        }
    }
    for (const QString& f : kDefaults) {
        if (!chain.contains(f, Qt::CaseInsensitive))
            chain.append(f);
    }
    return chain;
}

void ThemeManager::apply_theme(const QString& /*name*/) {
    current_ = THEME_OBSIDIAN;
    rebuild_and_apply();
}

void ThemeManager::apply_font(const QString& family, int size_px) {
    font_families_ = build_font_chain(family);
    font_size_px_ = (size_px > 0) ? size_px : 14;
    rebuild_and_apply();
}

void ThemeManager::apply_density(const QString& density) {
    if (density == "Compact")
        density_pad_ = 2;
    else if (density == "Comfortable")
        density_pad_ = 8;
    else
        density_pad_ = 4;

    rebuild_and_apply();
}

void ThemeManager::apply_typography_and_density(const QString& family, int size_px,
                                                const QString& density) {
    if (!family.isEmpty())
        font_families_ = build_font_chain(family);
    font_size_px_ = (size_px > 0) ? size_px : 14;
    if (density == "Compact")
        density_pad_ = 2;
    else if (density == "Comfortable")
        density_pad_ = 8;
    else
        density_pad_ = 4;
    rebuild_and_apply();
}

QString ThemeManager::current_theme_name() const {
    return QString(current_.name);
}

QStringList ThemeManager::available_densities() {
    return {"Compact", "Default", "Comfortable"};
}

QFont ThemeManager::current_font() const {
    QFont f;
    // setFamilies (Qt 5.13+) accepts an explicit fallback list — Qt walks
    // it natively and picks the first available family. This is the right
    // primitive on macOS where the legacy single-string ctor with a
    // missing primary (e.g. "Consolas") silently resolved to a proportional
    // system font.
    f.setFamilies(font_families_);
    f.setStyleHint(QFont::Monospace);
    f.setStyleStrategy(QFont::PreferAntialias);
    f.setPixelSize(font_size_px_);
    return f;
}

void ThemeManager::rebuild_and_apply() {
    LOG_INFO("ThemeManager", QString("rebuild_and_apply: families=[%1] size=%2px density_pad=%3")
        .arg(font_families_.join(", ")).arg(font_size_px_).arg(density_pad_));

    QFont f;
    f.setFamilies(font_families_);
    f.setStyleHint(QFont::Monospace);
    // PreferAntialias forces grayscale AA over subpixel, which on macOS
    // dark themes is noticeably cleaner — subpixel AA on dark backgrounds
    // produces coloured fringes around glyphs that look "fragmented" on
    // small text sizes. Cross-platform safe (Windows/Linux ignore where
    // the system chooses the AA strategy itself).
    f.setStyleStrategy(QFont::PreferAntialias);
    f.setPixelSize(font_size_px_);
    if (qApp->font() != f) {
        LOG_INFO("ThemeManager", QString("Applying new QFont to qApp (resolved family: %1)")
                                     .arg(QFontInfo(f).family()));
        qApp->setFont(f);
    }

    QString qss = build_global_qss();
    LOG_INFO("ThemeManager", QString("Global QSS length: %1 chars").arg(qss.length()));
    if (qss != cached_qss_) {
        LOG_INFO("ThemeManager", "QSS changed — calling qApp->setStyleSheet()");
        cached_qss_ = qss;
        qApp->setStyleSheet(qss);
        LOG_INFO("ThemeManager", "qApp->setStyleSheet() complete");
    } else {
        LOG_INFO("ThemeManager", "QSS unchanged — skipping setStyleSheet");
    }
    emit theme_changed(current_);
    LOG_INFO("ThemeManager", "theme_changed emitted");
}

QString ThemeManager::build_global_qss() const {
    const auto& t = current_;
    const int p = density_pad_;
    const int p2 = density_pad_ * 2;

    return QString(R"(
        * {
            font-family: %15;
            font-size: %16px;
        }
        QWidget {
            background-color: %1;
            color: %2;
        }
        QScrollBar:vertical {
            background: transparent; width: 5px; margin: 0;
        }
        QScrollBar::handle:vertical { background: %3; min-height: 20px; }
        QScrollBar::handle:vertical:hover { background: %4; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar:horizontal {
            background: transparent; height: 5px; margin: 0;
        }
        QScrollBar::handle:horizontal { background: %3; min-width: 20px; }
        QScrollBar::handle:horizontal:hover { background: %4; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
        QLineEdit, QTextEdit, QPlainTextEdit {
            background: %5; color: %2; border: 1px solid %6;
            padding: %7px %8px;
            selection-background-color: %9; selection-color: %1;
        }
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {
            border: 1px solid %4;
        }
        QPushButton {
            background: %5; color: %10; border: 1px solid %6;
            padding: %7px %8px;
        }
        QPushButton:hover { background: %11; color: %2; }
        QPushButton:pressed { background: %12; }
        QPushButton:disabled { color: %13; }
        QComboBox {
            background: %5; color: %2; border: 1px solid %6;
            padding: %7px %8px;
        }
        QComboBox::drop-down { border: none; width: 16px; }
        QComboBox QAbstractItemView {
            background: %5; color: %2; border: 1px solid %6;
            selection-background-color: %11;
        }
        QHeaderView::section {
            background: %5; color: %13; border: none;
            border-bottom: 1px solid %6; padding: %7px %8px;
            font-weight: 600;
        }
        QTableView, QTreeView, QListView {
            background: %1; alternate-background-color: %14;
            gridline-color: %11; border: none;
        }
        QTableView::item, QTreeView::item, QListView::item {
            padding: 1px %7px; border-bottom: 1px solid %11;
        }
        QTableView::item:selected, QTreeView::item:selected, QListView::item:selected {
            background: %12; color: %2;
        }
        QSplitter::handle { background: %6; }
        QSplitter::handle:hover { background: %4; }
        QTabBar::tab {
            background: %5; color: %13; border: 1px solid %6;
            padding: %7px %8px;
        }
        QTabBar::tab:selected {
            background: %11; color: %2; border-bottom: 1px solid %9;
        }
        QToolTip {
            background: %5; color: %2; border: 1px solid %6;
            padding: %7px %8px;
        }
        QCheckBox { color: %10; }
        QCheckBox::indicator {
            width: 12px; height: 12px; border: 1px solid %4; background: %5;
        }
        QCheckBox::indicator:checked { background: %9; border-color: %9; }
        QProgressBar {
            background: %5; border: 1px solid %6; color: %13;
        }
        QProgressBar::chunk { background: %9; }
        QMenu { background: %5; color: %2; border: 1px solid %6; }
        QMenu::item:selected { background: %12; }
        QMenu::separator { background: %6; height: 1px; }

        QScrollArea { border: none; background: %1; }
        /* App-level QFrame transparent rule would erase ADS tab/title styling,
           so re-assert ADS widget backgrounds below — later, more-specific rules win. */
        QFrame { background: transparent; border: none; }
        ads--CDockContainerWidget { background: %1; border: none; }
        ads--CDockAreaWidget { background: %1; border: none; }
        ads--CDockWidget { background: %1; border: none; }
        ads--CDockAreaTitleBar {
            background: %12;
            border-bottom: 1px solid %6;
            padding: 0px;
            min-height: 24px;
            max-height: 24px;
        }
        ads--CDockAreaWidget[focused="true"] ads--CDockAreaTitleBar {
            background: %12;
            border-bottom: 1px solid %9;
        }
        ads--CDockWidgetTab {
            background: %5;
            border: none;
            border-right: 1px solid %6;
            padding: 2px 8px;
            min-height: 22px;
            max-height: 22px;
        }
        ads--CDockWidgetTab[activeTab="true"] {
            background: %12;
            border-bottom: 2px solid %9;
            border-right: 1px solid %6;
        }
        ads--CDockWidgetTab[focused="true"] {
            background: %12;
            border-bottom: 2px solid %9;
            border-right: 1px solid %6;
        }
        ads--CDockWidgetTab QLabel {
            color: %10;
            font-size: 11px;
            font-weight: 600;
            background: transparent;
        }
        ads--CDockWidgetTab[activeTab="true"] QLabel { color: %2; }
        ads--CDockWidgetTab[focused="true"] QLabel { color: %2; }
        ads--CDockSplitter::handle { background-color: %6; }
        ads--CTitleBarButton {
            padding: 2px; max-width: 16px; max-height: 16px;
            min-width: 16px; min-height: 16px;
            background: transparent; border: none;
        }
        ads--CTitleBarButton:hover { background: %11; }
        ads--CAutoHideSideBar { background: %1; border: none; }
        QScrollArea#dockWidgetScrollArea { padding: 0px; border: none; }

        QTabWidget::pane { border: none; background: %1; }

        QSpinBox, QDoubleSpinBox {
            background: %5; color: %2; border: 1px solid %6;
            padding: %7px %8px;
        }
        QSpinBox:focus, QDoubleSpinBox:focus { border: 1px solid %4; }
        QSpinBox::up-button, QSpinBox::down-button,
        QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
            border: none; background: %11; width: 16px;
        }

        QTextBrowser {
            background: %1; color: %2; border: none; padding: 12px;
        }

        QToolButton {
            background: transparent; color: %10; border: none;
            padding: %7px %8px;
        }
        QToolButton:hover { background: %11; color: %2; }
        QToolButton:pressed { background: %12; }
        QToolButton:checked { background: %12; color: %2; border-bottom: 2px solid %9; }

        QRadioButton { color: %10; }
        QRadioButton::indicator {
            width: 12px; height: 12px; border: 1px solid %4;
            border-radius: 6px; background: %5;
        }
        QRadioButton::indicator:checked {
            background: %9; border-color: %9;
        }

        QGroupBox {
            border: 1px solid %6; margin-top: 12px;
            padding: %8px %7px; color: %10;
        }
        QGroupBox::title {
            subcontrol-origin: margin; left: 8px;
            padding: 0 4px; color: %9;
        }

        QSlider::groove:horizontal {
            height: 4px; background: %6; border-radius: 2px;
        }
        QSlider::handle:horizontal {
            width: 12px; height: 12px; margin: -4px 0;
            background: %9; border-radius: 6px;
        }
        QSlider::handle:horizontal:hover { background: %2; }

        QStatusBar { border: none; background: %1; padding: 0; }
        QToolBar { border: none; background: %1; spacing: 0; padding: 0; }

        /* ── ADS icon buttons ───────────────────────────────────────────── */
        #tabCloseButton {
            background: none; border: none; padding: 2px;
            width: 16px; height: 16px; min-width: 16px; min-height: 16px;
            max-width: 16px; max-height: 16px;
            image: url("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'><line x1='4' y1='4' x2='12' y2='12' stroke='%2380808080' stroke-width='1.5' stroke-linecap='round'/><line x1='12' y1='4' x2='4' y2='12' stroke='%2380808080' stroke-width='1.5' stroke-linecap='round'/></svg>");
        }
        #tabCloseButton:hover {
            background: %11;
            image: url("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'><line x1='4' y1='4' x2='12' y2='12' stroke='%23e5e5e5' stroke-width='1.5' stroke-linecap='round'/><line x1='12' y1='4' x2='4' y2='12' stroke='%23e5e5e5' stroke-width='1.5' stroke-linecap='round'/></svg>");
        }
        #tabCloseButton:pressed { background: %12; }
        ads--CDockWidgetTab[focused="true"] > #tabCloseButton:hover { background: %12; }
        #tabsMenuButton {
            background: none; border: none; padding: 2px;
            width: 16px; height: 16px; min-width: 16px; min-height: 16px;
            max-width: 16px; max-height: 16px;
            image: url("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'><line x1='3' y1='5' x2='13' y2='5' stroke='%2380808080' stroke-width='1.5' stroke-linecap='round'/><line x1='3' y1='8' x2='13' y2='8' stroke='%2380808080' stroke-width='1.5' stroke-linecap='round'/><line x1='3' y1='11' x2='13' y2='11' stroke='%2380808080' stroke-width='1.5' stroke-linecap='round'/></svg>");
        }
        #tabsMenuButton::menu-indicator { image: none; }
        #tabsMenuButton:hover {
            background: %11;
            image: url("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'><line x1='3' y1='5' x2='13' y2='5' stroke='%23e5e5e5' stroke-width='1.5' stroke-linecap='round'/><line x1='3' y1='8' x2='13' y2='8' stroke='%23e5e5e5' stroke-width='1.5' stroke-linecap='round'/><line x1='3' y1='11' x2='13' y2='11' stroke='%23e5e5e5' stroke-width='1.5' stroke-linecap='round'/></svg>");
        }
        #dockAreaCloseButton {
            background: none; border: none; padding: 2px;
            width: 16px; height: 16px; min-width: 16px; min-height: 16px;
            max-width: 16px; max-height: 16px;
            image: url("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'><line x1='4' y1='4' x2='12' y2='12' stroke='%2380808080' stroke-width='1.5' stroke-linecap='round'/><line x1='12' y1='4' x2='4' y2='12' stroke='%2380808080' stroke-width='1.5' stroke-linecap='round'/></svg>");
        }
        #dockAreaCloseButton:hover {
            background: %11;
            image: url("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'><line x1='4' y1='4' x2='12' y2='12' stroke='%23e5e5e5' stroke-width='1.5' stroke-linecap='round'/><line x1='12' y1='4' x2='4' y2='12' stroke='%23e5e5e5' stroke-width='1.5' stroke-linecap='round'/></svg>");
        }
        #detachGroupButton {
            background: none; border: none; padding: 2px;
            width: 16px; height: 16px; min-width: 16px; min-height: 16px;
            max-width: 16px; max-height: 16px;
            image: url("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'><rect x='3' y='7' width='6' height='6' fill='none' stroke='%2380808080' stroke-width='1.5' stroke-linejoin='round'/><polyline points='9,3 13,3 13,7' fill='none' stroke='%2380808080' stroke-width='1.5' stroke-linecap='round' stroke-linejoin='round'/><line x1='8' y1='8' x2='13' y2='3' stroke='%2380808080' stroke-width='1.5' stroke-linecap='round'/></svg>");
        }
        #detachGroupButton:hover {
            background: %11;
            image: url("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'><rect x='3' y='7' width='6' height='6' fill='none' stroke='%23e5e5e5' stroke-width='1.5' stroke-linejoin='round'/><polyline points='9,3 13,3 13,7' fill='none' stroke='%23e5e5e5' stroke-width='1.5' stroke-linecap='round' stroke-linejoin='round'/><line x1='8' y1='8' x2='13' y2='3' stroke='%23e5e5e5' stroke-width='1.5' stroke-linecap='round'/></svg>");
        }
        #dockAreaAutoHideButton, #dockAreaMinimizeButton {
            background: none; border: none; padding: 2px;
            width: 16px; height: 16px; min-width: 16px; min-height: 16px;
            max-width: 16px; max-height: 16px;
            image: url("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'><line x1='3' y1='12' x2='13' y2='12' stroke='%2380808080' stroke-width='1.5' stroke-linecap='round'/></svg>");
        }
        #dockAreaAutoHideButton:hover, #dockAreaMinimizeButton:hover {
            background: %11;
            image: url("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'><line x1='3' y1='12' x2='13' y2='12' stroke='%23e5e5e5' stroke-width='1.5' stroke-linecap='round'/></svg>");
        }
        /* ── Reusable object-name selectors ────────────────────────────── */
        #sectionTitle {
            color: %9; font-size: 13px; font-weight: 700;
            background: transparent; border: none; padding: 0;
        }
        #sectionHeader {
            color: %10; font-size: 11px; font-weight: 600;
            text-transform: uppercase; letter-spacing: 1px;
            background: transparent; border: none;
            border-bottom: 1px solid %6; padding-bottom: 4px;
        }
        #cardFrame {
            background: %5; border: 1px solid %6; border-radius: 6px;
            padding: %8px;
        }
        #commandBar {
            background: %5; border-bottom: 1px solid %6;
            padding: %7px %8px;
        }
        #accentButton {
            background: %9; color: %17; border: none;
            padding: %7px %8px; font-weight: 600;
        }
        #accentButton:hover { background: %4; }
        #accentButton:pressed { background: %12; }
        #mutedButton {
            background: %12; color: %10; border: 1px solid %6;
            padding: %7px %8px;
        }
        #mutedButton:hover { background: %11; color: %2; }
        #statusLabel {
            color: %10; font-size: 11px; background: transparent;
            border: none; padding: 0;
        }
        #dimLabel {
            color: %13; font-size: 11px; background: transparent;
            border: none; padding: 0;
        }
        #dataLabel {
            color: %10; font-size: 13px; background: transparent;
            border: none; padding: 0;
        }
        #dataValue {
            color: %2; font-size: 13px; font-weight: 600;
            background: transparent; border: none; padding: 0;
        }
        #positiveText { color: %20; }
        #negativeText { color: %21; }
        #warningText  { color: %22; }
    )")
        .arg(t.bg_base)         // %1
        .arg(t.text_primary)    // %2
        .arg(t.border_med)      // %3
        .arg(t.border_bright)   // %4
        .arg(t.bg_surface)      // %5
        .arg(t.border_dim)      // %6
        .arg(p)                 // %7  padding
        .arg(p2)                // %8  double padding
        .arg(t.accent)          // %9
        .arg(t.text_secondary)  // %10
        .arg(t.bg_hover)        // %11
        .arg(t.bg_raised)       // %12
        .arg(t.text_dim)        // %13
        .arg(t.row_alt)         // %14
        .arg(qss_font_family_list())  // %15
        .arg(font_size_px_)     // %16
        .arg(t.text_on_accent)  // %17
        .arg(t.icon_dim)        // %18
        .arg(t.icon_hover)      // %19
        .arg(t.positive)        // %20
        .arg(t.negative)        // %21
        .arg(t.warning)         // %22
        .arg(t.positive_dim)    // %23
        .arg(t.negative_dim)    // %24
        .arg(t.accent_bg)       // %25
        .arg(t.positive_bg)     // %26
        .arg(t.negative_bg);    // %27
}

QString ThemeManager::build_ads_qss() const {
    const auto& t = current_;
    LOG_INFO("ThemeManager", QString("build_ads_qss: bg_base=%1 bg_raised=%2 bg_surface=%3 accent=%4 border_dim=%5")
        .arg(t.bg_base).arg(t.bg_raised).arg(t.bg_surface).arg(t.accent).arg(t.border_dim));
    // Applied to CDockManager to override ADS's default.css/focus_highlighting.css palette() colours.
    QString qss = QString(R"(
        ads--CDockContainerWidget { background: %1; }
        ads--CDockAreaWidget { background: %1; }
        ads--CDockAreaTitleBar {
            background: %2;
            border-bottom: 1px solid %3;
            padding: 0px;
            min-height: 24px;
            max-height: 24px;
        }
        ads--CDockAreaWidget[focused="true"] ads--CDockAreaTitleBar {
            background: %2;
            border-bottom: 1px solid %4;
        }
        ads--CDockWidgetTab {
            background: %5;
            border: none;
            border-right: 1px solid %3;
            padding: 2px 8px;
            min-height: 22px;
            max-height: 22px;
        }
        ads--CDockWidgetTab[activeTab="true"] {
            background: %2;
            border-bottom: 2px solid %4;
            border-right: 1px solid %3;
        }
        ads--CDockWidgetTab[focused="true"] {
            background: %2;
            border-bottom: 2px solid %4;
            border-right: 1px solid %3;
        }
        ads--CDockWidgetTab QLabel {
            color: %6;
            font-size: 11px;
            font-weight: 600;
            background: transparent;
        }
        ads--CDockWidgetTab[activeTab="true"] QLabel {
            color: %7;
        }
        ads--CDockWidgetTab[focused="true"] QLabel {
            color: %7;
        }
        ads--CDockWidget { background: %1; border: none; }
        ads--CDockSplitter::handle { background-color: %3; }
        ads--CTitleBarButton {
            padding: 2px; max-width: 16px; max-height: 16px;
            min-width: 16px; min-height: 16px;
            background: transparent; border: none;
        }
        ads--CTitleBarButton:hover { background: %8; }
        ads--CAutoHideSideBar { background: %1; border: none; }
        QScrollArea#dockWidgetScrollArea { padding: 0px; border: none; }

        /* Drop overlay + splitter hover both use accent for visual consistency across drag/resize. */
        ads--CDockOverlay { background: transparent; }
        ads--CDockOverlayCross { background: transparent; }
        ads--CDockSplitter::handle:hover { background-color: %4; }
    )")
        .arg(t.bg_base)        // %1
        .arg(t.bg_raised)      // %2
        .arg(t.border_dim)     // %3
        .arg(t.accent)         // %4
        .arg(t.bg_surface)     // %5
        .arg(t.text_secondary) // %6
        .arg(t.text_primary)   // %7
        .arg(t.bg_hover);      // %8

    LOG_INFO("ThemeManager", QString("build_ads_qss: produced %1 chars").arg(qss.length()));
    return qss;
}

} // namespace fincept::ui
