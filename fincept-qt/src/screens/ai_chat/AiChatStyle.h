#pragma once
// AiChatStyle.h — the AI Chat look in one place (the tab, its rails, the composer, the
// transcript and the Quick Chat bubble).
//
// Rules this file enforces for every ai_chat widget:
//   * Colours only from ui::colors tokens, fonts only from ui::fonts; no literals.
//   * ONE selector stylesheet (sheet()) set on the root of a widget tree (the screen, the
//     Quick Chat panel) before its children exist; children select by object name (the
//     constants below) and by dynamic properties ([tone="…"], [role="…"], [active="true"]).
//     Re-applied after ThemeManager::appearance_settled only when its text changed — never
//     inside a theme_changed handler.
//   * Font sizes in stylesheets: "font-size:%Npx" filled with .arg(ui::fonts::px(N)).
//   * Custom-painted widgets read colours at paint time (colour helpers below).
//   * Rhythm: multiples of 4 px (u()); heights from font metrics so 9 / 14 / 24 px fonts fit.
//   * Motion only when ui::motion_enabled() (Settings > Appearance > Animations): new blocks
//     fade in over kFadeMs; nothing moves when it is off.

#include <QColor>
#include <QFont>
#include <QString>

class QWidget;

namespace fincept::screens::aichat::style {

// ── Object names (selectors in sheet()) ──────────────────────────────────────
inline constexpr const char* kRoot = "aicRoot";               // screen root / Quick Chat panel: BG_BASE
inline constexpr const char* kRailLeft = "aicRailLeft";       // sessions rail: BG_SURFACE, hairline right
inline constexpr const char* kRailRight = "aicRailRight";     // context rail: BG_SURFACE, hairline left
inline constexpr const char* kDrawer = "aicDrawer";           // sessions drawer (< 1100 px): BG_SURFACE, hairline frame
inline constexpr const char* kSectionTitle = "aicSectionTitle"; // QLabel: small upper-case section title
inline constexpr const char* kSectionMeta = "aicSectionMeta"; // QLabel: dim figure next to a section title
inline constexpr const char* kHairline = "aicHairline";       // QFrame 1 px rule (BORDER_DIM)
inline constexpr const char* kDim = "aicDim";                 // QLabel: TEXT_TERTIARY small
inline constexpr const char* kBody = "aicBody";               // QLabel: TEXT_PRIMARY base size
inline constexpr const char* kBtn = "aicBtn";                 // QPushButton: outlined text button (UPPER)
inline constexpr const char* kBtnPrimary = "aicBtnPrimary";   // QPushButton: accent fill (Send)
inline constexpr const char* kBtnDanger = "aicBtnDanger";     // QPushButton: danger outline (Stop, Delete)
inline constexpr const char* kLinkBtn = "aicLinkBtn";         // QPushButton: flat text action (turn actions)
inline constexpr const char* kIconBtn = "aicIconBtn";         // QToolButton / QPushButton: square glyph button
inline constexpr const char* kChip = "aicChip";               // QPushButton / QLabel: framed chip; [tone="accent|warn|danger|ok"]
inline constexpr const char* kSearch = "aicSearch";           // QLineEdit: search / name field
inline constexpr const char* kComposer = "aicComposer";       // QFrame: composer area
inline constexpr const char* kComposerEdit = "aicComposerEdit"; // QPlainTextEdit inside it
inline constexpr const char* kHint = "aicHint";               // QLabel: composer hint line
inline constexpr const char* kList = "aicList";               // QListView / QListWidget in a rail
inline constexpr const char* kTranscript = "aicTranscript";   // the transcript view + viewport
inline constexpr const char* kTurn = "aicTurn";               // a turn widget; [current="true"] accent tick
inline constexpr const char* kTurnRole = "aicTurnRole";       // QLabel: YOU / model name; [role="user|ai|notice"]
inline constexpr const char* kTurnMeta = "aicTurnMeta";       // QLabel: time · tokens · duration
inline constexpr const char* kText = "aicText";               // QLabel: a rich-text block of an answer
inline constexpr const char* kUserText = "aicUserText";       // QLabel: what the user typed
inline constexpr const char* kHeading = "aicHeading";         // QLabel: heading block; [level="1|2|3"]
inline constexpr const char* kQuote = "aicQuote";             // QLabel: block quote
inline constexpr const char* kCode = "aicCode";               // QFrame: code block
inline constexpr const char* kCodeHead = "aicCodeHead";       // QWidget: code block header strip
inline constexpr const char* kCodeBody = "aicCodeBody";       // QPlainTextEdit: code (monospace, read-only)
inline constexpr const char* kMath = "aicMath";               // QLabel: display math (monospace)
inline constexpr const char* kActivity = "aicActivity";       // QFrame: tools / reasoning section
inline constexpr const char* kActivityHead = "aicActivityHead"; // QPushButton: its toggle line
inline constexpr const char* kErrorBlock = "aicError";        // QFrame: inline error / notice; [tone="danger|warn|accent|info"]
inline constexpr const char* kPill = "aicPill";               // QPushButton: "↓ NEW" pill
inline constexpr const char* kCard = "aicCard";               // QPushButton: start-page example card
inline constexpr const char* kStartPage = "aicStartPage";     // start page root (BG_BASE)

// ── Rhythm / metrics ─────────────────────────────────────────────────────────
inline constexpr int kUnit = 4;
inline constexpr int u(int n) { return n * kUnit; }
/// Pixel height of the user's font at `offset`.
int text_h(int offset = 0);
/// Average character width of the user's font at `offset`.
int avg_char(int offset = 0);
/// Fixed height of a header strip / bar line (never stretches): max(28, text_h(0) + 12).
int strip_h();
/// Height of a compact button / chip line: max(22, text_h(-3) + 8).
int button_h();

// ── Fonts ────────────────────────────────────────────────────────────────────
QFont ui_font(int offset = 0, int weight = QFont::Normal);
/// The user's font with tabular figures (OpenType "tnum") — every figure in painted widgets.
QFont mono_font(int offset = 0, int weight = QFont::Normal);

// ── Motion ───────────────────────────────────────────────────────────────────
/// Settings > Appearance > Animations (ui::motion_enabled()).
bool motion();
inline constexpr int kFadeMs = 140;        ///< fade-in of a new block (≤ 150 ms)
inline constexpr int kStreamFlushMs = 50;  ///< streaming tail repaint period (≤ 20 / s)

// ── Live colours (custom paint) ──────────────────────────────────────────────
QColor bg_base();
QColor bg_surface();
QColor bg_raised();
QColor bg_hover();
QColor border_dim();
QColor border_med();
QColor border_bright();
QColor text_primary();
QColor text_secondary();
QColor text_tertiary();
QColor text_dim();
QColor accent();
QColor accent_bg();
QColor on_accent();
QColor ok();
QColor warn();
QColor danger();
QColor info();
QColor positive();
QColor negative();

// ── Sheets ───────────────────────────────────────────────────────────────────
/// The one selector stylesheet for an ai_chat widget tree (every object name above).
QString sheet();
/// Set sheet() on `root` when its text differs (cheap to call from appearance_settled).
void apply_sheet(QWidget* root);
/// QMenu sheet (the shared command bar's menu look) for menus popped by ai_chat widgets.
QString menu_sheet();

// ── Rich text ────────────────────────────────────────────────────────────────
/// The parser's neutral inline markers (AiChatTypes.h) → themed inline styles. GUI thread.
/// Input is already sanitised; this only rewrites the marker tags.
QString inline_html(const QString& neutral);
/// Changes whenever inline_html() would produce different output (theme / font switch):
/// widgets that cache themed HTML compare it to know when to re-render.
quint64 inline_generation();

} // namespace fincept::screens::aichat::style
