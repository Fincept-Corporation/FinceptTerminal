#pragma once
// NotebookStyle — the Code tab's appearance in one place (header-only, no moc):
//
//   * nb_screen_sheet(): ONE selector stylesheet set on the screen root before
//     any child exists. Every widget of the tab is styled by object name and
//     dynamic properties ("tone", "sel", "active"), so state changes only
//     re-polish one widget and no child carries a sheet of its own. The central
//     re-style pass (ThemeRestyle.cpp) keeps it in step with theme and font.
//   * nb_set_sheet(): the only way a sheet is set here — skips unchanged text.
//   * nb_icon(): line icons for the notebook commands, painted at paint time
//     from the live theme (so they need no theme handler).
//   * nb_syntax_palette(), nb_html_palette(), nb_chart_style(): colours for the
//     code highlighter, rendered markdown / HTML export and matplotlib figures,
//     derived from theme tokens each time they are asked for (never cached).
//
// Colour rules: AMBER marks interaction and identity (selected cell, active
// tab, primary action); SUCCESS / WARNING / DANGER are run states; POSITIVE /
// NEGATIVE are never used here (no price direction in a notebook).

#include "screens/code_editor/NotebookDocument.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QIconEngine>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QString>
#include <QStyle>
#include <QWidget>

#include <algorithm>

namespace fincept::screens {

/// Selected rows / cells: the accent at this alpha.
inline constexpr double kNbSelectionAlpha = 0.14;
/// Wash behind the line a cell failed on.
inline constexpr double kNbErrorLineAlpha = 0.18;
/// Wash behind find-in-notebook matches.
inline constexpr double kNbMatchAlpha = 0.30;
/// Current-line wash of the focused editor.
inline constexpr double kNbCurrentLineAlpha = 0.04;

inline void nb_set_sheet(QWidget* w, const QString& sheet) {
    if (w && w->styleSheet() != sheet)
        w->setStyleSheet(sheet);
}

/// Set a dynamic property used by the screen sheet and re-polish only `w`.
inline void nb_set_prop(QWidget* w, const char* name, const QString& value) {
    if (!w || w->property(name).toString() == value)
        return;
    w->setProperty(name, value);
    w->style()->unpolish(w);
    w->style()->polish(w);
}

inline QString nb_menu_sheet() {
    namespace c = ui::colors;
    return QStringLiteral("QMenu{background:%1;border:1px solid %2;color:%3;font-family:%4;font-size:%5px;}"
                          "QMenu::item{padding:4px 22px 4px 14px;}"
                          "QMenu::item:selected{background:%6;}"
                          "QMenu::item:disabled{color:%7;}"
                          "QMenu::separator{height:1px;background:%8;margin:4px 8px;}"
                          "QMenu::section{color:%7;padding:4px 14px 2px 14px;font-weight:700;}")
        .arg(c::BG_RAISED(), c::BORDER_MED(), c::TEXT_PRIMARY(), QString(ui::fonts::DATA_FAMILY))
        .arg(ui::fonts::px(-2))
        .arg(c::BG_HOVER(), c::TEXT_TERTIARY(), c::BORDER_DIM());
}

/// The one stylesheet of the Code tab (see the header comment).
inline QString nb_screen_sheet() {
    namespace c = ui::colors;
    const QString fam = QString(ui::fonts::DATA_FAMILY);
    const QString sel = c::alpha(QColor(c::AMBER()), kNbSelectionAlpha);
    QString s;
    // Canvas, bars, panel headers.
    s += QStringLiteral(
             "#nbScreen{background:%1;font-family:%9;}"
             "#nbCmdBar{background:%2;border-bottom:1px solid %3;}"
             "#nbStatusBar{background:%1;border-top:1px solid %3;}"
             "#nbRail,#nbInspector{background:%1;}"
             "#nbRail{border-right:1px solid %3;}"
             "#nbInspector{border-left:1px solid %3;}"
             "#nbPanelHead{background:%2;border-bottom:1px solid %3;}"
             "#nbTick{background:%4;}"
             "QLabel#nbPanelTitle{color:%5;font-size:%10px;font-weight:700;letter-spacing:1px;background:transparent;}"
             "QLabel#nbDim{color:%6;background:transparent;font-size:%10px;}"
             "QLabel#nbText{color:%5;background:transparent;}"
             "QLabel#nbBadge{color:%6;background:%7;border:1px solid %3;padding:0 6px;font-size:%11px;}"
             "QLabel#nbName{color:%5;background:transparent;font-weight:700;}"
             "#nbVSep{background:%3;}"
             "QSplitter::handle{background:%3;}"
             "QToolTip{background:%8;color:%5;border:1px solid %3;}")
             .arg(c::BG_BASE(), c::BG_SURFACE(), c::BORDER_DIM(), c::AMBER(), c::TEXT_PRIMARY(), c::TEXT_TERTIARY(),
                  c::BG_RAISED(), c::BG_RAISED(), fam)
             .arg(ui::fonts::px(-3))
             .arg(ui::fonts::px(-4));
    // Buttons: command-bar / panel tool buttons, the primary run, tabs.
    s += QStringLiteral(
             "QToolButton#nbBtn{background:transparent;color:%1;border:1px solid transparent;padding:2px 7px;"
             "font-size:%8px;font-weight:600;}"
             "QToolButton#nbBtn:hover{color:%2;border-color:%3;background:%4;}"
             "QToolButton#nbBtn:pressed,QToolButton#nbBtn:checked{color:%5;border-color:%5;background:%6;}"
             "QToolButton#nbBtn:disabled{color:%7;}"
             "QToolButton#nbBtn::menu-indicator{image:none;width:0;}"
             "QToolButton#nbBtn[tone=\"danger\"]{color:%9;border-color:%10;}"
             "QToolButton#nbRun{background:%5;color:%11;border:1px solid %5;padding:2px 9px;font-size:%8px;"
             "font-weight:700;}"
             "QToolButton#nbRun:hover{background:%12;border-color:%12;}"
             "QToolButton#nbRun:disabled{background:transparent;color:%7;border-color:%3;}"
             "QToolButton#nbRun::menu-indicator,QToolButton#nbKernel::menu-indicator{image:none;width:0;}")
             .arg(c::TEXT_SECONDARY(), c::TEXT_PRIMARY(), c::BORDER_MED(), c::BG_HOVER(), c::AMBER(), c::ACCENT_BG(),
                  c::TEXT_TERTIARY())
             .arg(ui::fonts::px(-3))
             .arg(c::DANGER(), c::alpha(QColor(c::DANGER()), 0.45), c::TEXT_ON_ACCENT(), c::AMBER_DIM());
    s += QStringLiteral(
             "QPushButton#nbTab{background:transparent;color:%1;border:none;border-bottom:2px solid transparent;"
             "padding:3px 8px;font-size:%6px;font-weight:700;letter-spacing:0.5px;}"
             "QPushButton#nbTab:hover{color:%2;}"
             "QPushButton#nbTab[active=\"true\"]{color:%3;border-bottom-color:%3;}"
             "QPushButton#nbPrimary{background:%3;color:%4;border:1px solid %3;padding:4px 14px;font-size:%6px;"
             "font-weight:700;}"
             "QPushButton#nbPrimary:hover{background:%5;border-color:%5;}"
             "QPushButton#nbSecondary{background:transparent;color:%2;border:1px solid %7;padding:4px 14px;"
             "font-size:%6px;font-weight:700;}"
             "QPushButton#nbSecondary:hover{border-color:%3;color:%3;}"
             "QPushButton#nbLink{background:transparent;color:%3;border:none;padding:0 2px;font-size:%8px;"
             "font-weight:700;text-align:left;}"
             "QPushButton#nbLink:hover{color:%2;}")
             .arg(c::TEXT_SECONDARY(), c::TEXT_PRIMARY(), c::AMBER(), c::TEXT_ON_ACCENT(), c::AMBER_DIM())
             .arg(ui::fonts::px(-3))
             .arg(c::BORDER_MED())
             .arg(ui::fonts::px(-3));
    // Kernel badge and status labels: state through a "tone" property.
    s += QStringLiteral(
             "QToolButton#nbKernel{background:transparent;color:%1;border:1px solid %2;padding:2px 8px;"
             "font-size:%9px;font-weight:700;}"
             "QToolButton#nbKernel:hover{background:%3;}"
             "QToolButton#nbKernel[tone=\"ok\"]{color:%4;border-color:%10;}"
             "QToolButton#nbKernel[tone=\"warn\"]{color:%5;border-color:%11;}"
             "QToolButton#nbKernel[tone=\"danger\"]{color:%6;border-color:%12;}"
             "QToolButton#nbKernel[tone=\"dim\"]{color:%7;}"
             "#nbStatusBar QLabel{background:transparent;color:%1;font-size:%13px;}"
             "QLabel[tone=\"ok\"]{color:%4;} QLabel[tone=\"warn\"]{color:%5;} QLabel[tone=\"danger\"]{color:%6;}"
             "QLabel[tone=\"dim\"]{color:%7;} QLabel[tone=\"accent\"]{color:%8;}"
             "QLabel#nbSep{color:%2;}")
             .arg(c::TEXT_SECONDARY(), c::BORDER_MED(), c::BG_HOVER(), c::SUCCESS(), c::WARNING(), c::DANGER(),
                  c::TEXT_TERTIARY(), c::AMBER())
             .arg(ui::fonts::px(-3))
             .arg(c::alpha(QColor(c::SUCCESS()), 0.45), c::alpha(QColor(c::WARNING()), 0.45),
                  c::alpha(QColor(c::DANGER()), 0.45))
             .arg(ui::fonts::px(-4));
    // Inputs, lists, trees, tables, scroll bars.
    s += QStringLiteral(
             "QLineEdit#nbSearch{background:%1;color:%2;border:1px solid %3;padding:2px 6px;font-size:%10px;}"
             "QLineEdit#nbSearch:focus{border-color:%4;}"
             "QTreeWidget#nbTree,QTreeView#nbTree,QListWidget#nbList{background:%1;color:%2;border:none;outline:none;"
             "font-size:%10px;}"
             "QTreeWidget#nbTree::item,QTreeView#nbTree::item,QListWidget#nbList::item{padding:2px 2px;}"
             "QTreeWidget#nbTree::item:hover,QTreeView#nbTree::item:hover,QListWidget#nbList::item:hover{background:%5;}"
             "QTreeWidget#nbTree::item:selected,QTreeView#nbTree::item:selected,QListWidget#nbList::item:selected"
             "{background:%6;color:%2;}"
             "QTableView#nbTable{background:%1;alternate-background-color:%7;color:%2;gridline-color:%3;border:1px solid %3;"
             "selection-background-color:%6;selection-color:%2;font-size:%10px;}"
             "QHeaderView::section{background:%8;color:%9;border:none;border-right:1px solid %3;"
             "border-bottom:1px solid %3;padding:2px 6px;font-weight:700;font-size:%11px;}"
             "QTableCornerButton::section{background:%8;border:none;border-bottom:1px solid %3;}")
             .arg(c::BG_BASE(), c::TEXT_PRIMARY(), c::BORDER_DIM(), c::AMBER(), c::BG_HOVER(), sel, c::ROW_ALT(),
                  c::BG_SURFACE(), c::TEXT_SECONDARY())
             .arg(ui::fonts::px(-2))
             .arg(ui::fonts::px(-3));
    s += QStringLiteral(
             "QScrollBar:vertical{width:8px;background:transparent;margin:0;border:none;}"
             "QScrollBar::handle:vertical{background:%1;min-height:24px;}"
             "QScrollBar::handle:vertical:hover{background:%2;}"
             "QScrollBar:horizontal{height:8px;background:transparent;margin:0;border:none;}"
             "QScrollBar::handle:horizontal{background:%1;min-width:24px;}"
             "QScrollBar::handle:horizontal:hover{background:%2;}"
             "QScrollBar::add-line,QScrollBar::sub-line{width:0;height:0;border:none;}"
             "QScrollBar::add-page,QScrollBar::sub-page{background:transparent;}"
             "QScrollArea{border:none;background:transparent;}"
             "#nbCanvas,#nbStartBody{background:%3;}")
             .arg(c::BORDER_MED(), c::BORDER_BRIGHT(), c::BG_BASE());
    // Cells: frame, gutter, editor, outputs.
    s += QStringLiteral(
             "#nbCell{background:transparent;border-left:3px solid transparent;}"
             "#nbCell[sel=\"cmd\"]{border-left-color:%1;background:%2;}"
             "#nbCell[sel=\"edit\"]{border-left-color:%1;}"
             "QLabel#nbCount{color:%3;background:transparent;font-size:%12px;font-weight:700;}"
             "#nbCell[sel=\"cmd\"] QLabel#nbCount,#nbCell[sel=\"edit\"] QLabel#nbCount{color:%1;}"
             "QLabel#nbRunState{background:transparent;font-size:%13px;font-weight:600;color:%3;}"
             "QPlainTextEdit#nbCode{background:%4;color:%5;border:1px solid %6;font-size:%14px;"
             "selection-background-color:%7;selection-color:%8;}"
             "#nbCell[sel=\"edit\"] QPlainTextEdit#nbCode{border-color:%9;}"
             "QTextBrowser#nbMarkdown{background:transparent;color:%5;border:1px solid transparent;font-size:%14px;}"
             "#nbCell[sel=\"edit\"] QTextBrowser#nbMarkdown,#nbCell[sel=\"cmd\"] QTextBrowser#nbMarkdown"
             "{border-color:%6;}"
             "QPushButton#nbFolded{background:%4;color:%3;border:1px dashed %6;padding:3px 8px;text-align:left;"
             "font-size:%12px;}"
             "QPushButton#nbFolded:hover{color:%5;border-color:%10;}"
             "#nbCellBar{background:%11;border:1px solid %6;}")
             .arg(c::AMBER(), c::alpha(QColor(c::AMBER()), 0.04), c::TEXT_TERTIARY(), c::BG_SURFACE(),
                  c::TEXT_PRIMARY(), c::BORDER_DIM(), c::AMBER_DIM(), c::TEXT_ON_ACCENT(), c::AMBER_DIM())
             .arg(c::BORDER_BRIGHT(), c::BG_RAISED())
             .arg(ui::fonts::px(-3))
             .arg(ui::fonts::px(-4))
             .arg(ui::fonts::px(0));
    s += QStringLiteral(
             "QPlainTextEdit#nbOutText{background:transparent;color:%1;border:none;font-size:%8px;}"
             "QPlainTextEdit#nbOutText[stream=\"stderr\"]{color:%2;}"
             "QLabel#nbOutTag{color:%3;background:transparent;font-size:%9px;font-weight:700;letter-spacing:0.5px;}"
             "QLabel#nbOutTag[tone=\"warn\"]{color:%4;}"
             "#nbOutErr{background:%5;border-left:3px solid %6;}"
             "QLabel#nbErrTitle{color:%6;background:transparent;font-size:%8px;font-weight:700;}"
             "QLabel#nbErrWhere{color:%2;background:transparent;font-size:%9px;}"
             "QTextBrowser#nbTraceback{background:transparent;color:%2;border:none;font-size:%10px;}"
             "#nbOutTableHead,#nbOutImageHead{background:transparent;}"
             "QLabel#nbImage{background:transparent;}"
             "#nbOutStderr{border-left:2px solid %7;}"
             "#nbOutputs{background:transparent;}"
             "#nbCellFooter{background:transparent;}")
             .arg(c::TEXT_PRIMARY(), c::TEXT_SECONDARY(), c::TEXT_TERTIARY(), c::WARNING(),
                  c::tint(QColor(c::DANGER()), 0.10), c::DANGER(), c::alpha(QColor(c::WARNING()), 0.55))
             .arg(ui::fonts::px(-1))
             .arg(ui::fonts::px(-4))
             .arg(ui::fonts::px(-2));
    // Start page.
    s += QStringLiteral(
             "QLabel#nbHero{color:%1;background:transparent;font-size:%7px;font-weight:700;letter-spacing:1px;}"
             "QLabel#nbHeroSub{color:%2;background:transparent;font-size:%8px;}"
             "QFrame#nbCard{background:%3;border:1px solid %4;}"
             "QFrame#nbCard:hover{border-color:%5;}"
             "QLabel#nbCardTitle{color:%1;background:transparent;font-size:%9px;font-weight:700;}"
             "QLabel#nbCardText{color:%2;background:transparent;font-size:%10px;}"
             "QLabel#nbChip{color:%2;background:transparent;border:1px solid %4;padding:0 5px;font-size:%11px;"
             "font-weight:700;}"
             "QLabel#nbChip[tone=\"accent\"]{color:%6;border-color:%6;}")
             .arg(c::TEXT_PRIMARY(), c::TEXT_SECONDARY(), c::BG_SURFACE(), c::BORDER_DIM(), c::BORDER_BRIGHT(),
                  c::AMBER())
             .arg(ui::fonts::px(4))
             .arg(ui::fonts::px(-2))
             .arg(ui::fonts::px(-1))
             .arg(ui::fonts::px(-3))
             .arg(ui::fonts::px(-4));
    return s;
}

/// Panel header of the house style (Portfolio's make_panel_header contract,
/// styled by the screen sheet): BG_SURFACE strip, 1 px BORDER_DIM rule, 3×14
/// accent tick, small bold letter-spaced title, a controls row on the right.
struct NbPanelHeader {
    QWidget* header = nullptr;
    QLabel* title = nullptr;
    QHBoxLayout* controls = nullptr;
};

inline NbPanelHeader nb_panel_header(const QString& title, QWidget* parent) {
    NbPanelHeader r;
    r.header = new QWidget(parent);
    r.header->setObjectName(QStringLiteral("nbPanelHead"));
    r.header->setAttribute(Qt::WA_StyledBackground, true);
    r.header->setMinimumHeight(30);
    r.header->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* h = new QHBoxLayout(r.header);
    h->setContentsMargins(10, 0, 6, 0);
    h->setSpacing(6);
    auto* tick = new QWidget(r.header);
    tick->setObjectName(QStringLiteral("nbTick"));
    tick->setAttribute(Qt::WA_StyledBackground, true);
    tick->setFixedSize(3, 14);
    h->addWidget(tick);
    r.title = new QLabel(title, r.header);
    r.title->setObjectName(QStringLiteral("nbPanelTitle"));
    h->addWidget(r.title);
    h->addStretch(1);
    r.controls = new QHBoxLayout;
    r.controls->setSpacing(2);
    h->addLayout(r.controls);
    return r;
}

// ── Icons ────────────────────────────────────────────────────────────────────

enum class NbGlyph {
    Play, PlayAll, PlayDown, Stop, Restart, Save, Open, NewDoc, Export, Plus, RailLeft, RailRight,
    Up, Down, Trash, Dots, Copy, Expand, Code, Text, Search, Close, Folder, Check, ChevronRight, ChevronDown,
};

enum class NbTone { Normal, Accent, OnAccent, Danger, Success };

class NbIconEngine : public QIconEngine {
  public:
    NbIconEngine(NbGlyph glyph, NbTone tone) : glyph_(glyph), tone_(tone) {}
    QIconEngine* clone() const override { return new NbIconEngine(glyph_, tone_); }
    QString key() const override { return QStringLiteral("fincept.nb_icon"); }

    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override {
        QPixmap pm(size);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        paint(&p, QRect(QPoint(0, 0), size), mode, state);
        return pm;
    }

    void paint(QPainter* p, const QRect& rect, QIcon::Mode mode, QIcon::State) override {
        namespace c = ui::colors;
        QColor color;
        if (mode == QIcon::Disabled) {
            color = QColor(c::TEXT_TERTIARY());
        } else {
            switch (tone_) {
                case NbTone::Accent:
                    color = QColor(c::AMBER());
                    break;
                case NbTone::OnAccent:
                    color = QColor(c::TEXT_ON_ACCENT());
                    break;
                case NbTone::Danger:
                    color = QColor(c::DANGER());
                    break;
                case NbTone::Success:
                    color = QColor(c::SUCCESS());
                    break;
                case NbTone::Normal:
                    color = QColor(mode == QIcon::Active || mode == QIcon::Selected ? c::ICON_HOVER() : c::ICON_DIM());
                    break;
            }
        }
        const qreal side = std::min(rect.width(), rect.height());
        if (side <= 0)
            return;
        p->save();
        p->setRenderHint(QPainter::Antialiasing, true);
        p->translate(rect.x() + (rect.width() - side) / 2.0, rect.y() + (rect.height() - side) / 2.0);
        p->scale(side / 16.0, side / 16.0);
        QPen pen(color, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);
        auto tri = [&](qreal x, qreal y, qreal w, qreal h) {
            QPainterPath path;
            path.moveTo(x, y);
            path.lineTo(x + w, y + h / 2);
            path.lineTo(x, y + h);
            path.closeSubpath();
            p->fillPath(path, color);
        };
        switch (glyph_) {
            case NbGlyph::Play:
                tri(4.5, 3, 8, 10);
                break;
            case NbGlyph::PlayAll:
                tri(2.5, 3.5, 6, 9);
                tri(8, 3.5, 6, 9);
                break;
            case NbGlyph::PlayDown:
                tri(3, 2.5, 6.5, 7.5);
                p->drawLine(QPointF(3, 13), QPointF(13, 13));
                p->drawLine(QPointF(11.5, 6), QPointF(11.5, 10.5));
                p->drawPolyline(QPolygonF({QPointF(9.8, 9), QPointF(11.5, 10.8), QPointF(13.2, 9)}));
                break;
            case NbGlyph::Stop:
                p->fillRect(QRectF(4, 4, 8, 8), color);
                break;
            case NbGlyph::Restart: {
                p->drawArc(QRectF(3, 3, 10, 10), 60 * 16, 290 * 16);
                QPainterPath head;
                head.moveTo(10.4, 2.6);
                head.lineTo(10.8, 5.6);
                head.lineTo(13.6, 4.4);
                p->drawPath(head);
                break;
            }
            case NbGlyph::Save:
                p->drawRect(QRectF(2.8, 2.8, 10.4, 10.4));
                p->drawRect(QRectF(5, 2.8, 6, 3.6));
                p->drawRect(QRectF(5, 9, 6, 4.2));
                break;
            case NbGlyph::Open:
            case NbGlyph::Folder:
                p->drawPolyline(QPolygonF({QPointF(2.5, 12.5), QPointF(2.5, 3.5), QPointF(6.5, 3.5), QPointF(8, 5),
                                           QPointF(13.5, 5), QPointF(13.5, 12.5), QPointF(2.5, 12.5)}));
                if (glyph_ == NbGlyph::Open)
                    p->drawLine(QPointF(2.5, 7.5), QPointF(13.5, 7.5));
                break;
            case NbGlyph::NewDoc:
                p->drawPolyline(QPolygonF({QPointF(9.5, 2.5), QPointF(3.5, 2.5), QPointF(3.5, 13.5), QPointF(12.5, 13.5),
                                           QPointF(12.5, 5.5), QPointF(9.5, 2.5), QPointF(9.5, 5.5), QPointF(12.5, 5.5)}));
                p->drawLine(QPointF(8, 7.5), QPointF(8, 11.5));
                p->drawLine(QPointF(6, 9.5), QPointF(10, 9.5));
                break;
            case NbGlyph::Export:
                p->drawPolyline(QPolygonF({QPointF(6, 5), QPointF(3, 5), QPointF(3, 13), QPointF(13, 13), QPointF(13, 10)}));
                p->drawLine(QPointF(7, 9), QPointF(13, 3));
                p->drawPolyline(QPolygonF({QPointF(9, 3), QPointF(13, 3), QPointF(13, 7)}));
                break;
            case NbGlyph::Plus:
                p->drawLine(QPointF(8, 3.5), QPointF(8, 12.5));
                p->drawLine(QPointF(3.5, 8), QPointF(12.5, 8));
                break;
            case NbGlyph::RailLeft:
            case NbGlyph::RailRight: {
                p->drawRect(QRectF(2.5, 3, 11, 10));
                const qreal x = glyph_ == NbGlyph::RailLeft ? 6 : 10;
                p->drawLine(QPointF(x, 3), QPointF(x, 13));
                break;
            }
            case NbGlyph::Up:
                p->drawPolyline(QPolygonF({QPointF(4, 10), QPointF(8, 6), QPointF(12, 10)}));
                break;
            case NbGlyph::Down:
            case NbGlyph::ChevronDown:
                p->drawPolyline(QPolygonF({QPointF(4, 6), QPointF(8, 10), QPointF(12, 6)}));
                break;
            case NbGlyph::ChevronRight:
                p->drawPolyline(QPolygonF({QPointF(6, 4), QPointF(10, 8), QPointF(6, 12)}));
                break;
            case NbGlyph::Trash:
                p->drawLine(QPointF(3, 4.5), QPointF(13, 4.5));
                p->drawPolyline(QPolygonF({QPointF(6, 4.5), QPointF(6.5, 2.5), QPointF(9.5, 2.5), QPointF(10, 4.5)}));
                p->drawPolyline(QPolygonF({QPointF(4.5, 4.5), QPointF(5.2, 13.5), QPointF(10.8, 13.5), QPointF(11.5, 4.5)}));
                break;
            case NbGlyph::Dots:
                p->setBrush(color);
                for (const qreal y : {3.5, 8.0, 12.5})
                    p->drawEllipse(QPointF(8, y), 0.9, 0.9);
                break;
            case NbGlyph::Copy:
                p->drawRect(QRectF(5.5, 5.5, 7.5, 7.5));
                p->drawPolyline(QPolygonF({QPointF(3, 10.5), QPointF(3, 3), QPointF(10.5, 3)}));
                break;
            case NbGlyph::Expand:
                p->drawPolyline(QPolygonF({QPointF(9.5, 2.5), QPointF(13.5, 2.5), QPointF(13.5, 6.5)}));
                p->drawPolyline(QPolygonF({QPointF(6.5, 13.5), QPointF(2.5, 13.5), QPointF(2.5, 9.5)}));
                p->drawLine(QPointF(13.5, 2.5), QPointF(9.5, 6.5));
                p->drawLine(QPointF(2.5, 13.5), QPointF(6.5, 9.5));
                break;
            case NbGlyph::Code:
                p->drawPolyline(QPolygonF({QPointF(5.5, 4), QPointF(2, 8), QPointF(5.5, 12)}));
                p->drawPolyline(QPolygonF({QPointF(10.5, 4), QPointF(14, 8), QPointF(10.5, 12)}));
                p->drawLine(QPointF(9, 3.5), QPointF(7, 12.5));
                break;
            case NbGlyph::Text:
                for (const qreal y : {4.0, 7.0, 10.0})
                    p->drawLine(QPointF(3, y), QPointF(13, y));
                p->drawLine(QPointF(3, 13), QPointF(9, 13));
                break;
            case NbGlyph::Search:
                p->drawEllipse(QPointF(7, 7), 3.8, 3.8);
                p->drawLine(QPointF(9.8, 9.8), QPointF(13, 13));
                break;
            case NbGlyph::Close:
                p->drawLine(QPointF(4, 4), QPointF(12, 12));
                p->drawLine(QPointF(12, 4), QPointF(4, 12));
                break;
            case NbGlyph::Check:
                p->drawPolyline(QPolygonF({QPointF(3.5, 8.5), QPointF(6.5, 11.5), QPointF(12.5, 4.5)}));
                break;
        }
        p->restore();
    }

  private:
    NbGlyph glyph_;
    NbTone tone_;
};

inline QIcon nb_icon(NbGlyph glyph, NbTone tone = NbTone::Normal) {
    return QIcon(new NbIconEngine(glyph, tone));
}

// ── Derived palettes (asked for each time; never cached across a theme change) ─

struct NbSyntaxPalette {
    QColor keyword, builtin, string, number, comment, definition, decorator, self_, heading, code_span, link;
};

/// Python highlighting colours from the theme: categorical chart colours (none
/// is a price-direction hue) for the token classes, text tokens for the rest.
inline NbSyntaxPalette nb_syntax_palette() {
    namespace c = ui::colors;
    NbSyntaxPalette p;
    p.keyword = c::legible_color(c::chart_color(7));
    p.builtin = c::legible_color(c::chart_color(1));
    p.string = c::legible_color(c::chart_color(5));
    p.number = c::legible_color(c::chart_color(6));
    p.comment = QColor(c::TEXT_TERTIARY());
    p.definition = c::legible_color(c::chart_color(4));
    p.decorator = c::legible_color(c::chart_color(3));
    p.self_ = QColor(c::TEXT_SECONDARY());
    p.heading = QColor(c::TEXT_PRIMARY());
    p.code_span = p.string;
    p.link = c::legible_color(c::chart_color(1));
    return p;
}

inline notebook::HtmlPalette nb_html_palette() {
    namespace c = ui::colors;
    notebook::HtmlPalette p;
    p.bg = c::BG_BASE();
    p.surface = c::BG_SURFACE();
    p.text = c::TEXT_PRIMARY();
    p.dim = c::TEXT_SECONDARY();
    p.border = c::BORDER_DIM();
    p.accent = c::AMBER();
    p.link = c::legible(c::chart_color(1));
    p.code_bg = c::BG_RAISED();
    p.error = c::DANGER();
    p.font_family = QString(ui::fonts::DATA_FAMILY);
    p.font_px = ui::fonts::font_px(0);
    return p;
}

/// matplotlib colours for the kernel (NotebookKernel::set_chart_style).
inline QJsonObject nb_chart_style() {
    namespace c = ui::colors;
    QJsonArray cycle;
    for (int i = 0; i < 8; ++i)
        cycle.append(c::chart_color(i).name());
    return QJsonObject{{QStringLiteral("bg"), QString(c::BG_BASE())},
                       {QStringLiteral("fg"), QString(c::TEXT_PRIMARY())},
                       {QStringLiteral("dim"), QString(c::TEXT_SECONDARY())},
                       {QStringLiteral("grid"), QString(c::BORDER_DIM())},
                       {QStringLiteral("edge"), QString(c::BORDER_MED())},
                       {QStringLiteral("cycle"), cycle}};
}

} // namespace fincept::screens
