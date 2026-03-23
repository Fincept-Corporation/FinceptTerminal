#pragma once
#include <QColor>
#include <QString>

namespace fincept::ui {

/// OBSIDIAN — Deep blacks, amber data, ultra-minimal hairline borders.
/// Dense monospace grid, no rounded corners, razor-sharp.
namespace colors {
// Backgrounds — pure black layers
constexpr const char* BG_BASE = "#080808";
constexpr const char* BG_SURFACE = "#0a0a0a";
constexpr const char* BG_RAISED = "#111111";
constexpr const char* BG_HOVER = "#161616";

// Borders — hairline neutral
constexpr const char* BORDER_DIM = "#1a1a1a";
constexpr const char* BORDER_MED = "#222222";
constexpr const char* BORDER_BRIGHT = "#333333";

// Text hierarchy
constexpr const char* TEXT_PRIMARY = "#e5e5e5";
constexpr const char* TEXT_SECONDARY = "#808080";
constexpr const char* TEXT_TERTIARY = "#525252";
constexpr const char* TEXT_DIM = "#404040";

// Accent — deep amber
constexpr const char* AMBER = "#d97706";
constexpr const char* AMBER_DIM = "#78350f";
constexpr const char* ORANGE = "#b45309";

// Functional
constexpr const char* POSITIVE = "#16a34a";
constexpr const char* NEGATIVE = "#dc2626";
constexpr const char* INFO = "#2563eb";
constexpr const char* WARNING = "#ca8a04";
constexpr const char* CYAN = "#0891b2";

// Legacy aliases
constexpr const char* GREEN = POSITIVE;
constexpr const char* RED = NEGATIVE;
constexpr const char* WHITE = TEXT_PRIMARY;
constexpr const char* GRAY = TEXT_SECONDARY;
constexpr const char* MUTED = TEXT_TERTIARY;
constexpr const char* DARK = BG_BASE;
constexpr const char* PANEL = BG_SURFACE;
constexpr const char* HEADER = BG_RAISED;
constexpr const char* BORDER = BORDER_DIM;
constexpr const char* CARD_BG = BG_SURFACE;
constexpr const char* ROW_ALT = "#0c0c0c";
} // namespace colors

/// Fonts — monospace, readable at all screen sizes.
namespace fonts {
constexpr const char* DATA_FAMILY = "'Consolas','Courier New',monospace";
constexpr const char* UI_FAMILY = "'Consolas','Courier New',monospace";
constexpr int TITLE = 20;
constexpr int HEADER = 16;
constexpr int BODY = 16;
constexpr int DATA = 14;
constexpr int SMALL = 13;
constexpr int TINY = 12;
} // namespace fonts

QString change_color(double value);
void apply_global_stylesheet();

/// Returns true if current layout is RTL (for Arabic/Hebrew support).
bool is_rtl();
/// Set RTL layout direction.
void set_rtl(bool rtl);

} // namespace fincept::ui
