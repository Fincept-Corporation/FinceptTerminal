// AiChatStyle.cpp — see AiChatStyle.h.

#include "screens/ai_chat/AiChatStyle.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/ScreenCommandBar.h"

#include <QFontMetrics>
#include <QHash>
#include <QWidget>

namespace fincept::screens::aichat::style {

namespace col = fincept::ui::colors;
namespace fnt = fincept::ui::fonts;

namespace {

QColor st_token(const QString& value) {
    return QColor(value);
}

} // namespace

// ── Metrics ──────────────────────────────────────────────────────────────────

int text_h(int offset) {
    return QFontMetrics(ui_font(offset)).height();
}

int avg_char(int offset) {
    return qMax(4, QFontMetrics(ui_font(offset)).averageCharWidth());
}

int strip_h() {
    return qMax(28, text_h(0) + 12);
}

int button_h() {
    return qMax(22, text_h(-3) + 8);
}

// ── Fonts ────────────────────────────────────────────────────────────────────

QFont ui_font(int offset, int weight) {
    return fnt::font(offset, weight);
}

QFont mono_font(int offset, int weight) {
    QFont f = fnt::font(offset, weight);
    f.setFeature(QFont::Tag("tnum"), 1); // tabular figures: columns of numbers line up
    return f;
}

bool motion() {
    return ui::motion_enabled();
}

// ── Live colours ─────────────────────────────────────────────────────────────

QColor bg_base() { return st_token(col::BG_BASE()); }
QColor bg_surface() { return st_token(col::BG_SURFACE()); }
QColor bg_raised() { return st_token(col::BG_RAISED()); }
QColor bg_hover() { return st_token(col::BG_HOVER()); }
QColor border_dim() { return st_token(col::BORDER_DIM()); }
QColor border_med() { return st_token(col::BORDER_MED()); }
QColor border_bright() { return st_token(col::BORDER_BRIGHT()); }
QColor text_primary() { return st_token(col::TEXT_PRIMARY()); }
QColor text_secondary() { return st_token(col::TEXT_SECONDARY()); }
QColor text_tertiary() { return st_token(col::TEXT_TERTIARY()); }
QColor text_dim() { return st_token(col::TEXT_DIM()); }
QColor accent() { return st_token(col::AMBER()); }
QColor accent_bg() { return st_token(col::ACCENT_BG()); }
QColor on_accent() { return st_token(col::TEXT_ON_ACCENT()); }
QColor ok() { return st_token(col::SUCCESS()); }
QColor warn() { return st_token(col::WARNING()); }
QColor danger() { return st_token(col::DANGER()); }
QColor info() { return st_token(col::INFO()); }
QColor positive() { return st_token(col::POSITIVE()); }
QColor negative() { return st_token(col::NEGATIVE()); }

// ── Sheets ───────────────────────────────────────────────────────────────────
// Built in chunks: each chunk's colour markers are filled by ONE multi-argument arg()
// call, then its font sizes by single .arg(ui::fonts::px(n)) calls in marker order.

QString sheet() {
    QString s;
    // Surfaces
    s += QString("QWidget#aicRoot,QWidget#aicStartPage{background:%1;}"
                 "QWidget#aicRailLeft{background:%2;border:none;border-right:1px solid %3;}"
                 "QWidget#aicRailRight{background:%2;border:none;border-left:1px solid %3;}"
                 "QWidget#aicDrawer{background:%2;border:1px solid %4;}"
                 "QFrame#aicHairline{background:%3;border:none;min-height:1px;max-height:1px;}")
             .arg(col::BG_BASE(), col::BG_SURFACE(), col::BORDER_DIM(), col::BORDER_MED());
    // Labels
    s += QString("QLabel#aicSectionTitle{color:%1;background:transparent;font-weight:700;letter-spacing:1px;"
                 "font-size:%4px;}"
                 "QLabel#aicSectionMeta{color:%2;background:transparent;font-size:%5px;}"
                 "QLabel#aicDim{color:%2;background:transparent;font-size:%6px;}"
                 "QLabel#aicBody{color:%3;background:transparent;font-size:%7px;}")
             .arg(col::TEXT_SECONDARY(), col::TEXT_TERTIARY(), col::TEXT_PRIMARY())
             .arg(fincept::ui::fonts::px(-3))
             .arg(fincept::ui::fonts::px(-3))
             .arg(fincept::ui::fonts::px(-2))
             .arg(fincept::ui::fonts::px(0));
    // Outlined text button
    s += QString("QPushButton#aicBtn{background:transparent;color:%1;border:1px solid %2;padding:0 8px;"
                 "font-weight:700;letter-spacing:0.5px;font-size:%8px;}"
                 "QPushButton#aicBtn:hover{color:%3;border-color:%4;background:%5;}"
                 "QPushButton#aicBtn:focus{border-color:%6;}"
                 "QPushButton#aicBtn:checked,QPushButton#aicBtn[active=\"true\"]{color:%6;border-color:%6;"
                 "background:%7;}"
                 "QPushButton#aicBtn:disabled{color:%2;border-color:%2;background:transparent;}")
             .arg(col::TEXT_SECONDARY(), col::BORDER_MED(), col::TEXT_PRIMARY(), col::BORDER_BRIGHT(),
                  col::BG_HOVER(), col::AMBER(), col::ACCENT_BG())
             .arg(fincept::ui::fonts::px(-3));
    // Primary (accent fill) and danger (outline)
    s += QString("QPushButton#aicBtnPrimary{background:%1;color:%2;border:1px solid %1;padding:0 12px;"
                 "font-weight:700;letter-spacing:0.5px;font-size:%9px;}"
                 "QPushButton#aicBtnPrimary:hover{background:%3;border-color:%3;}"
                 "QPushButton#aicBtnPrimary:focus{border-color:%4;}"
                 "QPushButton#aicBtnPrimary:disabled{background:%5;color:%6;border-color:%7;}"
                 "QPushButton#aicBtnDanger{background:transparent;color:%8;border:1px solid %8;padding:0 12px;"
                 "font-weight:700;letter-spacing:0.5px;font-size:%10px;}")
             .arg(col::AMBER(), col::TEXT_ON_ACCENT(), col::AMBER_DIM(), col::TEXT_PRIMARY(), col::BG_RAISED(),
                  col::TEXT_DIM(), col::BORDER_DIM(), col::DANGER())
             .arg(fincept::ui::fonts::px(-3))
             .arg(fincept::ui::fonts::px(-3));
    s += QString("QPushButton#aicBtnDanger:hover{background:%1;color:%2;}"
                 "QPushButton#aicBtnDanger:focus{border-color:%3;}"
                 "QPushButton#aicBtnDanger:disabled{color:%4;border-color:%5;background:transparent;}")
             .arg(col::alpha(col::DANGER, 0.14), col::DANGER(), col::TEXT_PRIMARY(), col::TEXT_DIM(),
                  col::BORDER_DIM());
    // Flat link-style action, icon buttons, chips
    s += QString("QPushButton#aicLinkBtn{background:transparent;color:%1;border:none;padding:0 4px;"
                 "font-weight:700;letter-spacing:0.5px;font-size:%7px;}"
                 "QPushButton#aicLinkBtn:hover{color:%2;}"
                 "QPushButton#aicLinkBtn:focus{color:%2;border-bottom:1px solid %2;}"
                 "QPushButton#aicLinkBtn:disabled{color:%3;}"
                 "QPushButton#aicIconBtn,QToolButton#aicIconBtn{background:transparent;color:%4;border:1px solid "
                 "transparent;padding:0;font-size:%8px;}"
                 "QPushButton#aicIconBtn:hover,QToolButton#aicIconBtn:hover{color:%5;border-color:%6;}"
                 "QPushButton#aicIconBtn:focus,QToolButton#aicIconBtn:focus{border-color:%2;}")
             .arg(col::TEXT_TERTIARY(), col::AMBER(), col::TEXT_DIM(), col::TEXT_SECONDARY(), col::TEXT_PRIMARY(),
                  col::BORDER_MED())
             .arg(fincept::ui::fonts::px(-3))
             .arg(fincept::ui::fonts::px(0));
    s += QString("QPushButton#aicChip,QLabel#aicChip{background:%1;color:%2;border:1px solid %3;padding:1px 6px;"
                 "font-size:%8px;}"
                 "QPushButton#aicChip:hover{border-color:%4;color:%5;}"
                 "QPushButton#aicChip:focus{border-color:%6;}"
                 "QPushButton#aicChip[tone=\"accent\"],QLabel#aicChip[tone=\"accent\"]{color:%6;border-color:%6;"
                 "background:%7;}")
             .arg(col::BG_RAISED(), col::TEXT_SECONDARY(), col::BORDER_MED(), col::BORDER_BRIGHT(),
                  col::TEXT_PRIMARY(), col::AMBER(), col::ACCENT_BG())
             .arg(fincept::ui::fonts::px(-3));
    s += QString("QPushButton#aicChip[tone=\"warn\"],QLabel#aicChip[tone=\"warn\"]{color:%1;border-color:%1;}"
                 "QPushButton#aicChip[tone=\"danger\"],QLabel#aicChip[tone=\"danger\"]{color:%2;border-color:%2;}"
                 "QPushButton#aicChip[tone=\"ok\"],QLabel#aicChip[tone=\"ok\"]{color:%3;border-color:%3;}")
             .arg(col::WARNING(), col::DANGER(), col::SUCCESS());
    // Inputs
    s += QString("QLineEdit#aicSearch{background:%1;color:%2;border:1px solid %3;padding:2px 6px;"
                 "selection-background-color:%4;font-size:%7px;}"
                 "QLineEdit#aicSearch:focus{border-color:%5;}"
                 "QLineEdit#aicSearch:disabled{color:%6;}")
             .arg(col::BG_BASE(), col::TEXT_PRIMARY(), col::BORDER_MED(), col::ACCENT_BG(), col::AMBER(),
                  col::TEXT_DIM())
             .arg(fincept::ui::fonts::px(-1));
    s += QString("QFrame#aicComposer{background:%1;border:none;border-top:1px solid %2;}"
                 "QFrame#aicComposer[focus=\"true\"]{border-top:1px solid %3;}"
                 "QPlainTextEdit#aicComposerEdit{background:%4;color:%5;border:1px solid %2;padding:4px 8px;"
                 "selection-background-color:%6;font-size:%8px;}"
                 "QPlainTextEdit#aicComposerEdit:focus{border-color:%3;}"
                 "QLabel#aicHint{color:%7;background:transparent;font-size:%9px;}")
             .arg(col::BG_SURFACE(), col::BORDER_DIM(), col::AMBER(), col::BG_BASE(), col::TEXT_PRIMARY(),
                  col::ACCENT_BG(), col::TEXT_TERTIARY())
             .arg(fincept::ui::fonts::px(0))
             .arg(fincept::ui::fonts::px(-3));
    // Lists in rails
    s += QString("QListView#aicList,QListWidget#aicList{background:transparent;border:none;outline:none;"
                 "color:%1;font-size:%6px;}"
                 "QListView#aicList::item:selected,QListWidget#aicList::item:selected{background:%2;color:%3;}"
                 "QListView#aicList::item:hover,QListWidget#aicList::item:hover{background:%4;}"
                 "QListView#aicList:focus,QListWidget#aicList:focus{border:none;}"
                 "QLabel#aicMath{color:%5;background:transparent;font-size:%7px;}")
             .arg(col::TEXT_SECONDARY(), col::ACCENT_BG(), col::TEXT_PRIMARY(), col::BG_HOVER(), col::TEXT_PRIMARY())
             .arg(fincept::ui::fonts::px(-1))
             .arg(fincept::ui::fonts::px(0));
    // Transcript
    s += QString("QAbstractScrollArea#aicTranscript{background:%1;border:none;}"
                 "QWidget#aicTurn{background:transparent;}"
                 "QWidget#aicTurn[current=\"true\"]{border-left:2px solid %2;}"
                 "QLabel#aicTurnRole{color:%3;background:transparent;font-weight:700;letter-spacing:1px;"
                 "font-size:%7px;}"
                 "QLabel#aicTurnRole[role=\"ai\"]{color:%4;}"
                 "QLabel#aicTurnRole[role=\"notice\"]{color:%5;}"
                 "QLabel#aicTurnMeta{color:%6;background:transparent;font-size:%8px;}")
             .arg(col::BG_BASE(), col::AMBER(), col::TEXT_SECONDARY(), col::TEXT_PRIMARY(), col::WARNING(),
                  col::TEXT_TERTIARY())
             .arg(fincept::ui::fonts::px(-3))
             .arg(fincept::ui::fonts::px(-3));
    s += QString("QLabel#aicText{color:%1;background:transparent;font-size:%5px;}"
                 "QLabel#aicUserText{color:%1;background:transparent;font-size:%6px;}"
                 "QLabel#aicHeading{color:%1;background:transparent;font-weight:700;font-size:%7px;}"
                 "QLabel#aicHeading[level=\"1\"]{font-size:%8px;}"
                 "QLabel#aicQuote{color:%2;background:transparent;border:none;border-left:2px solid %3;"
                 "padding-left:8px;font-style:italic;font-size:%9px;}"
                 "QLabel#aicText:focus,QLabel#aicUserText:focus{border:none;outline:none;}"
                 "QLabel#aicText[dim=\"true\"]{color:%4;}")
             .arg(col::TEXT_PRIMARY(), col::TEXT_SECONDARY(), col::BORDER_BRIGHT(), col::TEXT_TERTIARY())
             .arg(fincept::ui::fonts::px(0))
             .arg(fincept::ui::fonts::px(0))
             .arg(fincept::ui::fonts::px(1))
             .arg(fincept::ui::fonts::px(3))
             .arg(fincept::ui::fonts::px(0));
    s += QString("QFrame#aicCode{background:%1;border:1px solid %2;}"
                 "QWidget#aicCodeHead{background:%3;border:none;border-bottom:1px solid %2;}"
                 "QPlainTextEdit#aicCodeBody{background:%1;color:%4;border:none;padding:4px 8px;"
                 "selection-background-color:%5;font-size:%7px;}"
                 "QFrame#aicActivity{background:transparent;border:none;border-left:1px solid %2;}"
                 "QPushButton#aicActivityHead{background:transparent;color:%6;border:none;text-align:left;"
                 "padding:0 6px;font-weight:700;letter-spacing:0.5px;font-size:%8px;}")
             .arg(col::BG_SURFACE(), col::BORDER_DIM(), col::BG_RAISED(), col::TEXT_PRIMARY(), col::ACCENT_BG(),
                  col::TEXT_SECONDARY())
             .arg(fincept::ui::fonts::px(-1))
             .arg(fincept::ui::fonts::px(-3));
    s += QString("QPushButton#aicActivityHead:hover{color:%1;}"
                 "QPushButton#aicActivityHead:focus{color:%2;}"
                 "QFrame#aicError{background:%3;border:none;border-left:2px solid %4;}"
                 "QFrame#aicError[tone=\"warn\"]{background:%5;border-left-color:%6;}"
                 "QFrame#aicError[tone=\"accent\"]{background:%7;border-left-color:%2;}"
                 "QFrame#aicError[tone=\"info\"]{background:%8;border-left-color:%9;}")
             .arg(col::TEXT_PRIMARY(), col::AMBER(), col::alpha(col::DANGER, 0.08), col::DANGER(),
                  col::alpha(col::WARNING, 0.08), col::WARNING(), col::ACCENT_BG(), col::alpha(col::INFO, 0.08),
                  col::INFO());
    s += QString("QPushButton#aicPill{background:%1;color:%2;border:1px solid %1;padding:2px 10px;"
                 "font-weight:700;letter-spacing:0.5px;font-size:%7px;}"
                 "QPushButton#aicPill:hover{background:%3;}"
                 "QPushButton#aicCard{background:%4;border:1px solid %5;text-align:left;padding:0;}"
                 "QPushButton#aicCard:hover{background:%6;border-color:%1;}"
                 "QPushButton#aicCard:focus{border-color:%1;}")
             .arg(col::AMBER(), col::TEXT_ON_ACCENT(), col::AMBER_DIM(), col::BG_SURFACE(), col::BORDER_DIM(),
                  col::BG_HOVER())
             .arg(fincept::ui::fonts::px(-3));
    // Scroll bars inside the tree: thin, flat
    s += QString("QScrollBar:vertical{background:transparent;width:6px;margin:0;}"
                 "QScrollBar::handle:vertical{background:%1;min-height:24px;}"
                 "QScrollBar::handle:vertical:hover{background:%2;}"
                 "QScrollBar:horizontal{background:transparent;height:6px;margin:0;}"
                 "QScrollBar::handle:horizontal{background:%1;min-width:24px;}"
                 "QScrollBar::handle:horizontal:hover{background:%2;}"
                 "QScrollBar::add-line,QScrollBar::sub-line{width:0;height:0;}"
                 "QScrollBar::add-page,QScrollBar::sub-page{background:transparent;}"
                 "QToolTip{background:%3;color:%4;border:1px solid %5;}")
             .arg(col::BORDER_MED(), col::BORDER_BRIGHT(), col::BG_RAISED(), col::TEXT_PRIMARY(), col::BORDER_MED());
    s += menu_sheet();
    return s;
}

void apply_sheet(QWidget* root) {
    if (!root)
        return;
    const QString s = sheet();
    if (root->styleSheet() != s)
        root->setStyleSheet(s);
}

QString menu_sheet() {
    return ui::ScreenCommandBar::menu_css();
}

// ── Rich text ────────────────────────────────────────────────────────────────

namespace {

struct StInlineCache {
    QString key;
    QHash<QString, QString> map;
    quint64 generation = 0;
};

StInlineCache& st_inline_cache() {
    static StInlineCache c;
    return c;
}

// The theme- and font-dependent replacements, rebuilt only when the inputs change.
const StInlineCache& st_inline_current() {
    StInlineCache& c = st_inline_cache();
    const QString family = QString(fnt::DATA_FAMILY);
    const QString key = QStringList{col::AMBER(), col::ACCENT_BG(), col::INFO(), col::BG_RAISED(),
                                    col::TEXT_PRIMARY(), col::POSITIVE(), col::NEGATIVE(), col::TEXT_SECONDARY(),
                                    family, fnt::px(0)}
                            .join(QLatin1Char('|'));
    if (key == c.key)
        return c;
    c.key = key;
    ++c.generation;
    c.map.clear();
    c.map.insert(QStringLiteral("<code>"),
                 QString("<code style=\"font-family:%1;background-color:%2;color:%3;\">")
                     .arg(family, col::BG_RAISED(), col::TEXT_PRIMARY()));
    c.map.insert(QStringLiteral("<a class=\"ln\" "),
                 QString("<a style=\"color:%1;text-decoration:underline;\" ").arg(col::INFO()));
    c.map.insert(QStringLiteral("<a class=\"tk\" "),
                 QString("<a style=\"color:%1;background-color:%2;font-family:%3;font-weight:600;"
                         "text-decoration:none;\" ")
                     .arg(col::AMBER(), col::ACCENT_BG(), family));
    c.map.insert(QStringLiteral("<span class=\"n\">"), QString("<span style=\"font-family:%1;\">").arg(family));
    c.map.insert(QStringLiteral("<span class=\"up\">"),
                 QString("<span style=\"font-family:%1;color:%2;\">").arg(family, col::POSITIVE()));
    c.map.insert(QStringLiteral("<span class=\"dn\">"),
                 QString("<span style=\"font-family:%1;color:%2;\">").arg(family, col::NEGATIVE()));
    c.map.insert(QStringLiteral("<span class=\"m\">"),
                 QString("<span style=\"font-family:%1;font-style:italic;color:%2;\">")
                     .arg(family, col::TEXT_SECONDARY()));
    return c;
}

} // namespace

QString inline_html(const QString& neutral) {
    if (neutral.isEmpty())
        return neutral;
    const StInlineCache& c = st_inline_current();
    QString out = neutral;
    for (auto it = c.map.constBegin(); it != c.map.constEnd(); ++it)
        out.replace(it.key(), it.value());
    return out;
}

quint64 inline_generation() {
    return st_inline_current().generation;
}

} // namespace fincept::screens::aichat::style
