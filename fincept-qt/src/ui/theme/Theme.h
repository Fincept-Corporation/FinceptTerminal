#pragma once
#include "ui/theme/ThemeTokens.h"
#include "ui/theme/ThemeManager.h"

#include <QColor>
#include <QString>

namespace fincept::ui {

// ── Color shim ────────────────────────────────────────────────────────────────
// ColorToken is a single type for all color names.
// It holds a pointer-to-member of ThemeTokens and resolves the live value
// from ThemeManager at the point of use.
//
// Supports all usage patterns:
//   .arg(colors::BG_BASE)              — implicit const char* via operator const char*
//   .arg(colors::BG_BASE, colors::X)   — multi-arg via operator const char* (Qt6 variadic)
//   QColor(colors::BG_BASE)            — via operator QColor (Qt accepts QString ctor)
//   cond ? colors::A : colors::B       — same type, works natively
//   colors::BG_BASE()                  — explicit call via operator()
//   QString var = colors::X            — via operator const char* -> QString(const char*)

struct ColorToken {
    using Field = const char* ThemeTokens::*;
    Field field_;

    constexpr explicit ColorToken(Field f) noexcept : field_(f) {}

    const char* get() const noexcept { return ThemeManager::instance().tokens().*field_; }

    operator const char*()    const noexcept { return get(); }
    operator QColor()         const { return QColor(get()); }
    operator QString()        const { return QString::fromLatin1(get()); }
    const char* operator()()  const noexcept { return get(); }
};

namespace colors {

inline constexpr ColorToken BG_BASE       {&ThemeTokens::bg_base};
inline constexpr ColorToken BG_SURFACE    {&ThemeTokens::bg_surface};
inline constexpr ColorToken BG_RAISED     {&ThemeTokens::bg_raised};
inline constexpr ColorToken BG_HOVER      {&ThemeTokens::bg_hover};

inline constexpr ColorToken BORDER_DIM    {&ThemeTokens::border_dim};
inline constexpr ColorToken BORDER_MED    {&ThemeTokens::border_med};
inline constexpr ColorToken BORDER_BRIGHT {&ThemeTokens::border_bright};

inline constexpr ColorToken TEXT_PRIMARY  {&ThemeTokens::text_primary};
inline constexpr ColorToken TEXT_SECONDARY{&ThemeTokens::text_secondary};
inline constexpr ColorToken TEXT_TERTIARY {&ThemeTokens::text_tertiary};
inline constexpr ColorToken TEXT_DIM      {&ThemeTokens::text_dim};

inline constexpr ColorToken AMBER         {&ThemeTokens::accent};
inline constexpr ColorToken AMBER_DIM     {&ThemeTokens::accent_dim};
inline constexpr ColorToken ORANGE        {&ThemeTokens::accent};

inline constexpr ColorToken POSITIVE      {&ThemeTokens::positive};
inline constexpr ColorToken NEGATIVE      {&ThemeTokens::negative};
inline constexpr ColorToken WARNING       {&ThemeTokens::warning};
inline constexpr ColorToken INFO          {&ThemeTokens::info};
inline constexpr ColorToken CYAN          {&ThemeTokens::cyan};

// Legacy aliases
inline constexpr ColorToken GREEN         {&ThemeTokens::positive};
inline constexpr ColorToken RED           {&ThemeTokens::negative};
inline constexpr ColorToken WHITE         {&ThemeTokens::text_primary};
inline constexpr ColorToken GRAY          {&ThemeTokens::text_secondary};
inline constexpr ColorToken MUTED         {&ThemeTokens::text_tertiary};
inline constexpr ColorToken DARK          {&ThemeTokens::bg_base};
inline constexpr ColorToken PANEL         {&ThemeTokens::bg_surface};
inline constexpr ColorToken HEADER        {&ThemeTokens::bg_raised};
inline constexpr ColorToken BORDER        {&ThemeTokens::border_dim};
inline constexpr ColorToken CARD_BG       {&ThemeTokens::bg_surface};
inline constexpr ColorToken ROW_ALT       {&ThemeTokens::row_alt};

} // namespace colors

/// Fonts — read from active theme tokens.
namespace fonts {

struct FontFamilyToken {
    const char* get() const noexcept { return ThemeManager::instance().tokens().font_family; }
    operator QString()       const { return QString::fromLatin1(get()); }
    const char* operator()() const noexcept { return get(); }
};

inline constexpr FontFamilyToken DATA_FAMILY{};
inline constexpr FontFamilyToken UI_FAMILY{};

constexpr int TITLE  = 20;
constexpr int HEADER = 16;
constexpr int BODY   = 16;
constexpr int DATA   = 14;
constexpr int SMALL  = 13;
constexpr int TINY   = 12;

} // namespace fonts

/// Returns "#16a34a" for positive values, "#dc2626" for negative.
QString change_color(double value);

/// Applies the default theme at startup (delegates to ThemeManager).
void apply_global_stylesheet();

bool is_rtl();
void set_rtl(bool rtl);

} // namespace fincept::ui
