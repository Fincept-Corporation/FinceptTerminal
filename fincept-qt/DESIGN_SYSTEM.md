# Fincept Terminal — Obsidian Design System

> Reference for UI/UX developers. Every screen, widget, and component in the terminal
> MUST follow this specification exactly. No exceptions.

---

## 1. Design Philosophy

**Name:** Obsidian
**Inspiration:** Bloomberg Terminal, Refinitiv Eikon, institutional trading desks
**Principles:**
- Information density over whitespace
- Monospace data alignment
- Functional color only — no decoration
- Hairline borders, zero rounded corners
- Every pixel earns its place

---

## 2. Color Palette

### Backgrounds (layered depth — darkest to lightest)
| Token | Hex | Usage |
|-------|-----|-------|
| `BG_BASE` | `#080808` | App background, deepest layer |
| `BG_SURFACE` | `#0a0a0a` | Panels, cards, sidebar |
| `BG_RAISED` | `#111111` | Panel headers, elevated bars |
| `BG_HOVER` | `#161616` | Hover states, active rows |

### Borders (hairline only)
| Token | Hex | Usage |
|-------|-----|-------|
| `BORDER_DIM` | `#1a1a1a` | Default panel/cell borders |
| `BORDER_MED` | `#222222` | Emphasized separators |
| `BORDER_BRIGHT` | `#333333` | Focus rings, active borders |

### Text Hierarchy
| Token | Hex | Usage |
|-------|-----|-------|
| `TEXT_PRIMARY` | `#e5e5e5` | Data values, prices, primary content |
| `TEXT_SECONDARY` | `#808080` | Labels, headers, column names |
| `TEXT_TERTIARY` | `#525252` | Inactive tabs, muted info |
| `TEXT_DIM` | `#404040` | Disabled, timestamps, hints |

### Accent & Brand
| Token | Hex | Usage |
|-------|-----|-------|
| `AMBER` | `#d97706` | Primary accent — section titles, brand, active highlights |
| `AMBER_DIM` | `#78350f` | Muted amber — borders on amber elements |
| `ORANGE` | `#b45309` | Active tab background |

### Functional (data-driven — never decorative)
| Token | Hex | Usage |
|-------|-----|-------|
| `POSITIVE` | `#16a34a` | Gain, up, verified, connected, LIVE |
| `NEGATIVE` | `#dc2626` | Loss, down, error, logout |
| `INFO` | `#2563eb` | Informational highlights |
| `WARNING` | `#ca8a04` | Warnings, credits used |
| `CYAN` | `#0891b2` | Credit balance, stat box values |

### Alternating Row
| Token | Hex | Usage |
|-------|-----|-------|
| `ROW_ALT` | `#0c0c0c` | Alternating table row background |

---

## 3. Typography

### Font Families
| Context | Font Stack |
|---------|-----------|
| **Everything** | `'Consolas', 'Courier New', monospace` |

The entire terminal uses a single monospace font. No sans-serif, no serif, no variable fonts.
This ensures perfect data alignment and the authentic terminal feel.

### Font Sizes
| Token | Size | Usage |
|-------|------|-------|
| `TITLE` | 20px | Screen titles (rare) |
| `HEADER` | 16px | Major section headers |
| `BODY` | 16px | Default body text, inputs |
| `DATA` | 14px | Table data, panel content |
| `SMALL` | 13px | Tab labels, button text, navigation |
| `TINY` | 12px | Status bar, timestamps, badges |

### Font Weights
| Weight | Usage |
|--------|-------|
| `400` (normal) | Body text, data values |
| `600` (semi-bold) | Active tabs, button labels |
| `700` (bold) | Section titles, brand text, stat values |

### Letter Spacing
| Context | Spacing |
|---------|---------|
| Section titles (amber) | `0.5px – 1px` |
| Tab labels | `0.5px – 1px` |
| Status indicators (LIVE, READY) | `0.5px – 1px` |
| Body text | `0` (none) |

---

## 4. Layout Structure

### Vertical Stack (top to bottom)
```
┌─────────────────────────────────────────────────────────┐
│ TOOLBAR (32px)                                          │
│ File | Navigate | View | Help  ···  FINCEPT TERMINAL    │
│ ● LIVE          clock          user | credits | LOGOUT  │
├─────────────────────────────────────────────────────────┤
│ TAB BAR (32px)                                          │
│ DASHBOARD | MARKETS | CRYPTO | PORTFOLIO | NEWS | ...   │
├─────────────────────────────────────────────────────────┤
│                                                         │
│ CONTENT AREA (fills remaining space)                    │
│                                                         │
│                                                         │
├─────────────────────────────────────────────────────────┤
│ STATUS BAR (26px)                                       │
│ v4.0.0 | EQ FX CM FI CR               ● READY          │
└─────────────────────────────────────────────────────────┘
```

### Component Heights
| Component | Height | Background |
|-----------|--------|------------|
| Toolbar | 32px | `#0a0a0a` |
| Tab Bar | 32px | `#0e0e0e` |
| Status Bar | 26px | `#0a0a0a` |
| Panel Header | 34px | `#111111` |
| Data Row | 26px | transparent |
| Table Row | 26px | alternating `#080808` / `#0c0c0c` |
| Table Header | 26px | `#0a0a0a` or `#111111` |
| Button (standard) | 22-26px | `#111111` |
| Input Field | 28px | `#0a0a0a` |

### Spacing
| Context | Value |
|---------|-------|
| Page padding | `14px` |
| Panel gap | `10px` |
| Grid cell spacing | `4-8px` |
| Data row padding | `6px 12px` |
| Button padding | `0 10-12px` |

---

## 5. Component Specifications

### 5.1 Panel
```
┌─ SECTION TITLE ──────────────────────────── status ─┐  ← #111111, 34px
│ LABEL                                        VALUE  │  ← data row
│ LABEL                                        VALUE  │
│ LABEL                                        VALUE  │
└─────────────────────────────────────────────────────┘
```
- Background: `#0a0a0a`
- Border: `1px solid #1a1a1a`
- Title: `#d97706`, 12px, bold, letter-spacing 0.5px
- No rounded corners
- No shadows
- No padding on the panel itself — content fills edge to edge

### 5.2 Data Row
```
LABEL ·································· VALUE
```
- Label: `#808080`, 12px, bold, letter-spacing 0.5px
- Value: `#e5e5e5`, 13px, bold
- Bottom border: `1px solid #1a1a1a`
- Padding: `6px 12px`
- Functional color on value when semantic (green for YES, red for NO)

### 5.3 Stat Box
```
┌─────────────────┐
│      1,234      │  ← value: 28px, bold, colored
│   STAT LABEL    │  ← label: 10px, #808080, bold
└─────────────────┘
```
- Background: `#111111`
- Border: `1px solid #1a1a1a`
- Value centered, label centered below
- Padding: `12px 12px 14px`

### 5.4 Table
- Header: `#0a0a0a` or `#111111`, text `#404040`, 13px, bold
- Rows: alternating `#080808` / `#0c0c0c`
- Row height: 26px
- Cell padding: `0 6px`
- Row border: `1px solid #111111`
- Symbol column: left-aligned, `#e5e5e5`
- Numeric columns: right-aligned
- Change values: `#16a34a` (positive), `#dc2626` (negative)
- No grid lines, no focus ring, no selection highlight

### 5.5 Button Styles

**Standard Button:**
```css
background: #111111;
color: #808080;
border: 1px solid #1a1a1a;
font-size: 11-13px;
font-weight: 700;
```
Hover: `color: #e5e5e5; background: #161616`

**Accent Button (amber):**
```css
background: rgba(217,119,6,0.1);
color: #d97706;
border: 1px solid #78350f;
```
Hover: `background: #d97706; color: #080808`

**Danger Button (red):**
```css
background: rgba(220,38,38,0.1);
color: #dc2626;
border: 1px solid #7f1d1d;
```
Hover: `background: #dc2626; color: #e5e5e5`

**Active Tab:**
```css
background: #b45309;
color: #e5e5e5;
border: 1px solid #4d3300;
```

**Inactive Tab:**
```css
background: transparent;
color: #525252;
```
Hover: `color: #808080; background: #111111`

### 5.6 Input Fields
```css
background: #0a0a0a;
color: #e5e5e5;
border: 1px solid #1a1a1a;
padding: 3px 6px;
```
Focus: `border-color: #333333`
Selection: `background: #d97706; color: #080808`

### 5.7 Scrollbar
```css
width: 5-6px;
handle: #222222;
handle:hover: #333333;
track: transparent;
```

### 5.8 Menu Dropdown
```css
background: #0a0a0a;
border: 1px solid #1a1a1a;
item padding: 5px 24px 5px 12px;
item:selected: background #1a1a1a;
item:disabled: color #404040;
separator: #1a1a1a, 1px, margin 4px 8px;
```

---

## 6. Navigation Structure

### Toolbar Row (32px)
Left: `File | Navigate | View | Help` menus
Center-left: `FINCEPT` (amber, bold) + `TERMINAL` (dim) + `● LIVE` (green)
Center: Clock `yyyy-MM-dd  HH:mm:ss` (dim)
Right: `username | credits CR | PLAN | [LOGOUT]`

### Tab Bar (32px) — 14 tabs
`DASHBOARD | MARKETS | CRYPTO TRADING | PORTFOLIO | NEWS | AI CHAT | BACKTESTING | ALGO TRADING | NODE EDITOR | CODE EDITOR | AI QUANT LAB | QUANTLIB | SETTINGS | PROFILE`

Active tab: `#b45309` orange background
Inactive: `#525252` text, transparent background

### Navigate Dropdown — 30+ items in 5 groups
- MARKETS & DATA (7 items)
- TRADING & PORTFOLIO (5 items)
- RESEARCH & INTELLIGENCE (7 items)
- TOOLS (8 items)
- COMMUNITY (4 items)

Group headers: disabled items with `─` prefix
Scrollable with max-height: 480px

### Status Bar (26px)
Left: `v4.0.0 | EQ FX CM FI CR`
Right: `● READY` (green, bold)

---

## 7. Screen Patterns

### Market Data Screen
- Sub-header: `FINCEPT  MARKET TERMINAL  ● LIVE  ···  clock`
- Controls bar: `[REFRESH] [AUTO ON] [interval ▼]  ···  ● READY`
- Grid of market panels (3 columns)
- Section dividers: amber title + hairline

### Profile Screen
- Header: `PROFILE & ACCOUNT  username  ···  CR xxx  PLAN  [REFRESH]`
- Tab navigation: `OVERVIEW | USAGE | SECURITY | BILLING | SUPPORT`
- 2-column grid for overview (Account Info + Credits)
- Stat boxes for usage data
- Tables for history data

### Coming Soon Screen
- Centered layout
- ⚡ icon (48px)
- Tab name in amber (20px, bold, letter-spacing 2px)
- "COMING SOON" in dim (14px, letter-spacing 3px)
- Description in very dim (13px)

---

## 8. Rules

1. **Never use rounded corners** — everything is sharp rectangles
2. **Never use shadows** — depth comes from background layering only
3. **Never use gradients** — flat solid colors only
4. **Never use decorative icons** — text only, monospace
5. **Never use more than 2 font sizes** on any single row
6. **Green = good, Red = bad** — no other semantic mapping
7. **Amber = brand/accent** — section titles, active states, brand
8. **All text is monospace** — no sans-serif anywhere in the app
9. **Borders are always 1px solid** — never 2px, never dashed
10. **Hover states darken slightly** — never brighten or add glow

---

## 9. Anti-Patterns (DO NOT)

- ❌ Rounded corners on any element
- ❌ Box shadows or drop shadows
- ❌ Gradient backgrounds
- ❌ Sans-serif fonts
- ❌ Bright saturated colors (use muted tones)
- ❌ Large whitespace gaps between elements
- ❌ Animated transitions (instant state changes)
- ❌ Icons as primary navigation (use text labels)
- ❌ Colored backgrounds on data panels (always #0a0a0a)
- ❌ More than 3 levels of visual hierarchy on one screen

---

**Version:** 1.0
**Last Updated:** 2026-03-20
**Terminal Version:** 4.0.0
**Theme:** Obsidian
