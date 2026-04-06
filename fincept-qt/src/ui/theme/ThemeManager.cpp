#include "ui/theme/ThemeManager.h"

#include <QApplication>

namespace fincept::ui {

// ---------------------------------------------------------------------------
// Built-in theme preset definitions
// ---------------------------------------------------------------------------

const ThemeTokens THEME_OBSIDIAN = {
    .name          = "Obsidian",
    .bg_base       = "#080808",
    .bg_surface    = "#0a0a0a",
    .bg_raised     = "#111111",
    .bg_hover      = "#161616",
    .border_dim    = "#1a1a1a",
    .border_med    = "#222222",
    .border_bright = "#333333",
    .text_primary  = "#e5e5e5",
    .text_secondary= "#808080",
    .text_tertiary = "#525252",
    .text_dim      = "#404040",
    .accent        = "#d97706",
    .accent_dim    = "#78350f",
    .positive      = "#16a34a",
    .negative      = "#dc2626",
    .warning       = "#ca8a04",
    .info          = "#2563eb",
    .cyan          = "#0891b2",
    .row_alt       = "#0c0c0c",
    .font_family   = "'Consolas','Courier New',monospace",
    .font_size_base= 14,
    .chart_colors  = {"#d97706","#0891b2","#16a34a","#dc2626","#2563eb","#ca8a04"},
};

const ThemeTokens THEME_BLOOMBERG = {
    .name          = "Bloomberg Dark",
    .bg_base       = "#0a0a0a",
    .bg_surface    = "#0f0f0f",
    .bg_raised     = "#141414",
    .bg_hover      = "#1a1a1a",
    .border_dim    = "#1f1f1f",
    .border_med    = "#282828",
    .border_bright = "#383838",
    .text_primary  = "#ffffff",
    .text_secondary= "#999999",
    .text_tertiary = "#555555",
    .text_dim      = "#3a3a3a",
    .accent        = "#ff6600",
    .accent_dim    = "#7c2d00",
    .positive      = "#16a34a",
    .negative      = "#ff3333",
    .warning       = "#ca8a04",
    .info          = "#2563eb",
    .cyan          = "#00b4d8",
    .row_alt       = "#0d0d0d",
    .font_family   = "'Consolas','Courier New',monospace",
    .font_size_base= 14,
    .chart_colors  = {"#ff6600","#00b4d8","#16a34a","#ff3333","#2563eb","#ca8a04"},
};

const ThemeTokens THEME_MATRIX = {
    .name          = "Matrix",
    .bg_base       = "#030a03",
    .bg_surface    = "#051005",
    .bg_raised     = "#0a1a0a",
    .bg_hover      = "#0f240f",
    .border_dim    = "#0d2b0d",
    .border_med    = "#143d14",
    .border_bright = "#1e5c1e",
    .text_primary  = "#00ff41",
    .text_secondary= "#00bb30",
    .text_tertiary = "#007a1f",
    .text_dim      = "#004d13",
    .accent        = "#00ff41",
    .accent_dim    = "#003d10",
    .positive      = "#00ff41",
    .negative      = "#ff0000",
    .warning       = "#ffff00",
    .info          = "#00ccff",
    .cyan          = "#00ccff",
    .row_alt       = "#040c04",
    .font_family   = "'Consolas','Courier New',monospace",
    .font_size_base= 14,
    .chart_colors  = {"#00ff41","#00ccff","#ffff00","#ff0000","#00bb30","#007a1f"},
};

const ThemeTokens THEME_MIDNIGHT = {
    .name          = "Midnight Blue",
    .bg_base       = "#050a14",
    .bg_surface    = "#080f1e",
    .bg_raised     = "#0d1628",
    .bg_hover      = "#121e33",
    .border_dim    = "#0f1f3a",
    .border_med    = "#162845",
    .border_bright = "#1e3a5f",
    .text_primary  = "#e2e8f0",
    .text_secondary= "#94a3b8",
    .text_tertiary = "#475569",
    .text_dim      = "#2d3f55",
    .accent        = "#2563eb",
    .accent_dim    = "#1e3a8a",
    .positive      = "#22c55e",
    .negative      = "#ef4444",
    .warning       = "#f59e0b",
    .info          = "#38bdf8",
    .cyan          = "#38bdf8",
    .row_alt       = "#060c18",
    .font_family   = "'Consolas','Courier New',monospace",
    .font_size_base= 14,
    .chart_colors  = {"#2563eb","#38bdf8","#22c55e","#ef4444","#f59e0b","#a78bfa"},
};

// ---------------------------------------------------------------------------
// ThemeManager implementation
// ---------------------------------------------------------------------------

ThemeManager::ThemeManager()
    : QObject(nullptr)
    , current_(THEME_OBSIDIAN)
{}

ThemeManager& ThemeManager::instance() {
    static ThemeManager inst;
    return inst;
}

void ThemeManager::apply_theme(const QString& name) {
    if (name == "Bloomberg Dark")   current_ = THEME_BLOOMBERG;
    else if (name == "Matrix")      current_ = THEME_MATRIX;
    else if (name == "Midnight Blue") current_ = THEME_MIDNIGHT;
    else                            current_ = THEME_OBSIDIAN;

    // Re-apply custom accent override if one was set
    if (!accent_override_.isEmpty())
        current_.accent = accent_override_.constData();

    rebuild_and_apply();
}

void ThemeManager::apply_accent(const QString& hex) {
    accent_override_ = hex.toUtf8();          // owned storage — no dangling pointer
    current_.accent  = accent_override_.constData();
    rebuild_and_apply();
}

void ThemeManager::apply_font(const QString& family, int size_px) {
    font_family_  = family;
    font_size_px_ = (size_px > 0) ? size_px : 14;

    QFont f(font_family_);
    f.setStyleHint(QFont::Monospace);
    f.setPixelSize(font_size_px_);
    qApp->setFont(f);
    // Also rebuild QSS so widgets with inline font-size rules get overridden by
    // the global * { font-size } rule — otherwise 178+ hardcoded rules win.
    rebuild_and_apply();
}

void ThemeManager::apply_density(const QString& density) {
    if (density == "Compact")          density_pad_ = 2;
    else if (density == "Comfortable") density_pad_ = 8;
    else                               density_pad_ = 4;

    rebuild_and_apply();
}

QString ThemeManager::current_theme_name() const {
    return QString(current_.name);
}

QStringList ThemeManager::available_themes() {
    return {"Obsidian", "Bloomberg Dark", "Matrix", "Midnight Blue"};
}

QStringList ThemeManager::available_densities() {
    return {"Compact", "Default", "Comfortable"};
}

QFont ThemeManager::current_font() const {
    QFont f(font_family_);
    f.setPixelSize(font_size_px_);
    return f;
}

void ThemeManager::rebuild_and_apply() {
    QString qss = build_global_qss();
    if (qss != cached_qss_) {
        cached_qss_ = qss;
        qApp->setStyleSheet(qss);
    }
    emit theme_changed(current_);
}

QString ThemeManager::build_global_qss() const {
    const auto& t  = current_;
    const int   p  = density_pad_;       // padding px
    const int   p2 = density_pad_ * 2;  // double padding

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
    )")
    // %1  bg_base
    .arg(t.bg_base)
    // %2  text_primary
    .arg(t.text_primary)
    // %3  border_med
    .arg(t.border_med)
    // %4  border_bright
    .arg(t.border_bright)
    // %5  bg_surface
    .arg(t.bg_surface)
    // %6  border_dim
    .arg(t.border_dim)
    // %7  padding (density)
    .arg(p)
    // %8  double padding
    .arg(p2)
    // %9  accent
    .arg(t.accent)
    // %10 text_secondary
    .arg(t.text_secondary)
    // %11 bg_hover
    .arg(t.bg_hover)
    // %12 bg_raised
    .arg(t.bg_raised)
    // %13 text_dim
    .arg(t.text_dim)
    // %14 row_alt
    .arg(t.row_alt)
    // %15 font-family
    .arg(font_family_)
    // %16 font-size
    .arg(font_size_px_);
}

} // namespace fincept::ui
