// src/screens/algo_trading/AlgoTabKit.cpp
#include "screens/algo_trading/AlgoTabKit.h"

#include "ui/charts/ChartExport.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/FlowLayout.h"
#include "ui/widgets/PanelHeader.h"
#include "ui/widgets/ScreenCommandBar.h"
#include "ui/widgets/ScreenStatusBar.h"

#include <QAbstractItemModel>
#include <QAbstractScrollArea>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>
#include <QShortcut>
#include <QSplitter>
#include <QStyle>
#include <QTableView>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace fincept::screens::algo_kit {

namespace {

QString atk_tr(const char* text, const char* disambiguation = nullptr, int n = -1) {
    return QCoreApplication::translate("fincept::screens::algo_kit", text, disambiguation, n);
}

const char* const kAtkTabKeys[TabCount] = {"builder", "strategies", "scanner", "alerts", "dashboard", "universe"};

struct AtkTabText {
    QString text;
    QString tooltip;
};

AtkTabText atk_tab_text(int tab) {
    switch (tab) {
        case TabBuilder:
            return {atk_tr("BUILDER"), atk_tr("Define entry / exit rules, risk and costs; backtest them (Alt+1)")};
        case TabStrategies:
            return {atk_tr("STRATEGIES"), atk_tr("Your saved strategies with their last backtest (Alt+2)")};
        case TabScanner:
            return {atk_tr("SCANNER"), atk_tr("Find the symbols that match a rule set now (Alt+3)")};
        case TabAlerts:
            return {atk_tr("ALERTS"), atk_tr("Background watches and the alerts they fired (Alt+4)")};
        case TabDashboard:
            return {atk_tr("DASHBOARD"), atk_tr("Paper and live deployments: state, P&L, orders, log (Alt+5)")};
        case TabUniverse:
            return {atk_tr("UNIVERSE"), atk_tr("Run a saved strategy live across an exchange or a list (Alt+6)")};
        default:
            break;
    }
    return {};
}

int atk_row_height(const QTableView* view) {
    const QFontMetrics fm(view->font());
    return std::max(18, fm.height() + 6);
}

void atk_apply_column_chars(QTableView* view) {
    const QVariantList chars = view->property("algoColumnChars").toList();
    if (chars.isEmpty())
        return;
    const QFontMetrics fm(view->font());
    const int avg = std::max(5, fm.averageCharWidth());
    for (int i = 0; i < chars.size(); ++i) {
        const int c = chars.at(i).toInt();
        if (c > 0)
            view->setColumnWidth(i, c * avg + 14);
    }
}

/// Wheel over an unfocused spin box / combo scrolls the nearest scroll area instead.
class AtkWheelGuard : public QObject {
  public:
    using QObject::QObject;

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() != QEvent::Wheel)
            return false;
        auto* w = qobject_cast<QWidget*>(watched);
        if (!w || w->hasFocus())
            return false;
        for (QWidget* p = w->parentWidget(); p; p = p->parentWidget()) {
            if (auto* area = qobject_cast<QAbstractScrollArea*>(p)) {
                if (area->verticalScrollBar())
                    QCoreApplication::sendEvent(area->verticalScrollBar(), event);
                return true;
            }
        }
        event->ignore();
        return true;
    }
};

AtkWheelGuard* atk_wheel_guard() {
    static AtkWheelGuard* guard = [] {
        auto* g = new AtkWheelGuard(QCoreApplication::instance());
        return g;
    }();
    return guard;
}

} // namespace

// ── Sub-tabs ────────────────────────────────────────────────────────────────

QString tab_key(int tab) {
    return tab >= 0 && tab < TabCount ? QString::fromLatin1(kAtkTabKeys[tab]) : QString();
}

int tab_from_key(const QString& key) {
    for (int i = 0; i < TabCount; ++i)
        if (key == QLatin1String(kAtkTabKeys[i]))
            return i;
    return -1;
}

ui::ScreenCommandBar* make_tab_bar(QWidget* panel, Tab active) {
    auto* bar = new ui::ScreenCommandBar(panel);
    bar->setObjectName(QStringLiteral("algoTabBar"));
    bar->set_title(atk_tr("ALGO"));
    QVector<ui::ScreenCommandBar::Segment> segs;
    for (int i = 0; i < TabCount; ++i) {
        const AtkTabText t = atk_tab_text(i);
        segs.append({tab_key(i), t.text, QString::number(i + 1), t.tooltip});
    }
    bar->add_segmented(QStringLiteral("tab"), segs, 95);
    bar->set_segment(QStringLiteral("tab"), tab_key(active));
    bar->set_overflow_text(atk_tr("More"));
    auto* badge = new QLabel(bar->right_area());
    badge->setObjectName(QStringLiteral("algoDeployBadge"));
    badge->setAccessibleName(atk_tr("Running deployments"));
    badge->setVisible(false); // AlgoTradingScreen shows it with the count
    if (auto* l = bar->right_area()->layout())
        l->addWidget(badge);
    return bar;
}

void retranslate_tab_bar(ui::ScreenCommandBar* bar) {
    if (!bar)
        return;
    for (int i = 0; i < TabCount; ++i) {
        const AtkTabText t = atk_tab_text(i);
        bar->set_segment_text(QStringLiteral("tab"), tab_key(i), t.text, t.tooltip);
    }
    bar->set_title(atk_tr("ALGO"));
    bar->set_overflow_text(atk_tr("More"));
}

ui::ScreenStatusBar* make_status_bar(QWidget* panel) {
    using SB = ui::ScreenStatusBar;
    auto* sb = new SB(panel);
    sb->setObjectName(QStringLiteral("algoStatusBar"));
    sb->add_item(QStringLiteral("engine"), SB::Segment::Left, atk_tr("ENGINE · NATIVE C++"), SB::Tone::Accent, 100);
    sb->set_tooltip(QStringLiteral("engine"),
                    atk_tr("Rules are evaluated by the terminal's own engine (no Python): the same code backtests, "
                           "paper-trades and trades live."));
    sb->add_item(QStringLiteral("data"), SB::Segment::Left, QString(), SB::Tone::Normal, 80);
    sb->add_item(QStringLiteral("info"), SB::Segment::Centre, QString(), SB::Tone::Dim, 50);
    sb->add_item(QStringLiteral("asof"), SB::Segment::Right, QString(), SB::Tone::Dim, 70);
    sb->add_item(QStringLiteral("state"), SB::Segment::Right, atk_tr("READY"), SB::Tone::Ok, 90);
    sb->set_chip(QStringLiteral("state"), true);
    sb->add_item(QStringLiteral("deploy"), SB::Segment::Right, QString(), SB::Tone::Normal, 60);
    sb->set_chip(QStringLiteral("deploy"), true);
    sb->set_item_visible(QStringLiteral("deploy"), false);
    sb->set_clock(true, false);
    return sb;
}

void set_state(ui::ScreenStatusBar* bar, RunState state, const QString& text) {
    if (!bar)
        return;
    using SB = ui::ScreenStatusBar;
    QString t = text;
    SB::Tone tone = SB::Tone::Normal;
    switch (state) {
        case RunState::Ready:
            tone = SB::Tone::Ok;
            if (t.isEmpty())
                t = atk_tr("READY");
            break;
        case RunState::Running:
            tone = SB::Tone::Accent;
            if (t.isEmpty())
                t = atk_tr("RUNNING");
            break;
        case RunState::Queued:
            tone = SB::Tone::Info;
            if (t.isEmpty())
                t = atk_tr("QUEUED");
            break;
        case RunState::Failed:
            tone = SB::Tone::Danger;
            if (t.isEmpty())
                t = atk_tr("FAILED");
            break;
        case RunState::Cancelled:
            tone = SB::Tone::Warn;
            if (t.isEmpty())
                t = atk_tr("CANCELLED");
            break;
        case RunState::Idle:
            tone = SB::Tone::Dim;
            if (t.isEmpty())
                t = atk_tr("IDLE");
            break;
    }
    bar->set_item(QStringLiteral("state"), t, tone);
}

QString root_stylesheet() {
    using namespace fincept::ui;
    const QString base = colors::BG_BASE, surface = colors::BG_SURFACE, raised = colors::BG_RAISED,
                  hover = colors::BG_HOVER, bdim = colors::BORDER_DIM, bmed = colors::BORDER_MED,
                  bbright = colors::BORDER_BRIGHT, tp = colors::TEXT_PRIMARY, ts = colors::TEXT_SECONDARY,
                  tt = colors::TEXT_TERTIARY, td = colors::TEXT_DIM, accent = colors::AMBER,
                  on_accent = colors::TEXT_ON_ACCENT, pos = colors::POSITIVE, neg = colors::NEGATIVE,
                  warn = colors::WARNING, ok = colors::SUCCESS, danger = colors::DANGER, info = colors::INFO,
                  abg = colors::ACCENT_BG, alt = colors::ROW_ALT;
    const QString family = QString(fonts::DATA_FAMILY);
    QString css;
    css += QStringLiteral("#algoScreen{background:%1;}").arg(base);
    css += QStringLiteral("#algoScreen QWidget{font-size:%1px;}").arg(fonts::px(0));
    css += QStringLiteral("QFrame#algoPanel{background:%1;border:1px solid %2;}").arg(surface, bdim);
    css += QStringLiteral("#algoPanelBody{background:transparent;}");
    css += QStringLiteral("#algoStrip{background:%1;border:none;border-bottom:1px solid %2;}").arg(surface, bdim);
    css += QStringLiteral("#algoRibbon{background:%1;border:none;border-bottom:1px solid %2;}").arg(base, bdim);
    css += QStringLiteral("QLabel#algoFieldLabel{color:%1;font-weight:700;font-size:%2px;letter-spacing:0.5px;"
                          "background:transparent;}")
               .arg(tt)
               .arg(fonts::px(-3));
    css += QStringLiteral("QLabel#algoFieldLabel[error=\"true\"]{color:%1;}").arg(danger);
    css += QStringLiteral("QLabel#algoNote{color:%1;font-size:%2px;background:transparent;}").arg(ts).arg(fonts::px(-3));
    css += QStringLiteral("QLabel#algoSentence{color:%1;font-size:%2px;background:transparent;padding:2px 0px;}")
               .arg(ts)
               .arg(fonts::px(-2));
    css += QStringLiteral("QLabel#algoValue{color:%1;font-weight:600;font-family:%2;font-size:%3px;"
                          "background:transparent;}")
               .arg(tp, family)
               .arg(fonts::px(-1));
    css += QStringLiteral("QLabel#algoChip,QLabel#algoDeployBadge{border:1px solid %1;color:%2;padding:1px 6px;"
                          "font-weight:700;font-family:%3;font-size:%4px;background:transparent;}")
               .arg(bmed, ts, family)
               .arg(fonts::px(-3));
    const QPair<const char*, QString> tones[] = {{"pos", pos},       {"neg", neg}, {"warn", warn},     {"dim", td},
                                                 {"accent", accent}, {"ok", ok},   {"danger", danger}, {"info", info}};
    for (const auto& t : tones) {
        // The id-qualified forms are needed: "QLabel#algoChip{color}" (an id) outranks a bare
        // "QLabel[tone]" (an attribute), so the tone must carry the id too to win.
        css += QStringLiteral("QLabel[tone=\"%1\"],QPushButton[tone=\"%1\"],QLabel#algoChip[tone=\"%1\"],"
                              "QLabel#algoDeployBadge[tone=\"%1\"],QLabel#algoNote[tone=\"%1\"],"
                              "QLabel#algoValue[tone=\"%1\"],QLabel#algoHero[tone=\"%1\"],QLabel#algoKey[tone=\"%1\"],"
                              "QLabel#algoSentence[tone=\"%1\"],QPushButton#algoToolButton[tone=\"%1\"],"
                              "QPushButton#algoLink[tone=\"%1\"]{color:%2;}")
                   .arg(QLatin1String(t.first), t.second);
        css += QStringLiteral("QLabel#algoChip[tone=\"%1\"],QLabel#algoDeployBadge[tone=\"%1\"]{border-color:%2;}")
                   .arg(QLatin1String(t.first), t.second);
    }
    css += QStringLiteral("QPushButton#algoToolButton{background:%1;border:1px solid %2;color:%3;padding:2px 8px;"
                          "font-weight:700;font-size:%4px;}")
               .arg(raised, bmed, tp)
               .arg(fonts::px(-3));
    css += QStringLiteral("QPushButton#algoToolButton:hover{border-color:%1;background:%2;}").arg(bbright, hover);
    css += QStringLiteral("QPushButton#algoToolButton:focus{border-color:%1;}").arg(accent);
    css += QStringLiteral("QPushButton#algoToolButton:disabled{color:%1;border-color:%2;}").arg(td, bdim);
    css += QStringLiteral("QPushButton#algoToolButton[active=\"true\"],QPushButton#algoToolButton:checked{color:%1;"
                          "border-color:%1;}")
               .arg(accent);
    css += QStringLiteral("QPushButton#algoSeg{background:%1;border:1px solid %2;color:%3;padding:1px 8px;"
                          "font-weight:700;font-size:%4px;}")
               .arg(raised, bmed, ts)
               .arg(fonts::px(-3));
    css += QStringLiteral("QPushButton#algoSeg:checked{background:%1;color:%2;border-color:%1;}").arg(accent, on_accent);
    css += QStringLiteral("QPushButton#algoSeg:focus{border-color:%1;}").arg(accent);
    css += QStringLiteral("QPushButton#algoSeg:disabled{color:%1;}").arg(td);
    css += QStringLiteral("QPushButton#algoPrimary{background:%1;color:%2;border:1px solid %1;padding:3px 12px;"
                          "font-weight:700;font-size:%3px;}")
               .arg(accent, on_accent)
               .arg(fonts::px(-2));
    css += QStringLiteral("QPushButton#algoPrimary:disabled{background:%1;color:%2;border-color:%3;}").arg(raised, td, bdim);
    css += QStringLiteral("QPushButton#algoPrimary[tone=\"danger\"]{background:transparent;color:%1;border-color:%1;}")
               .arg(danger);
    css += QStringLiteral("QPushButton#algoTemplate{background:transparent;border:1px solid %1;color:%2;"
                          "padding:1px 8px;font-weight:600;font-size:%3px;}")
               .arg(bmed, ts)
               .arg(fonts::px(-3));
    css += QStringLiteral("QPushButton#algoTemplate:hover,QPushButton#algoTemplate:focus{border-color:%1;color:%2;}")
               .arg(accent, tp);
    css += QStringLiteral("QPushButton#algoTemplate[active=\"true\"]{border-color:%1;color:%1;}").arg(accent);
    css += QStringLiteral("QPushButton#algoLink{background:transparent;border:none;color:%1;padding:0px 2px;"
                          "font-weight:700;font-size:%2px;text-align:left;}")
               .arg(accent)
               .arg(fonts::px(-3));
    css += QStringLiteral("QPushButton#algoLink:hover{color:%1;}").arg(tp);
    css += QStringLiteral("QPushButton#algoLink:disabled{color:%1;}").arg(td);
    css += QStringLiteral("QLineEdit#algoField,QComboBox#algoField,QAbstractSpinBox#algoField{background:%1;"
                          "border:1px solid %2;color:%3;padding:1px 4px;font-size:%4px;}")
               .arg(base, bmed, tp)
               .arg(fonts::px(-2));
    css += QStringLiteral("QLineEdit#algoField:focus,QComboBox#algoField:focus,QAbstractSpinBox#algoField:focus{"
                          "border-color:%1;}")
               .arg(accent);
    css += QStringLiteral("QLineEdit#algoField:disabled,QComboBox#algoField:disabled,QAbstractSpinBox#algoField:disabled{"
                          "color:%1;border-color:%2;background:%3;}")
               .arg(td, bdim, surface);
    css += QStringLiteral("QLineEdit#algoField[error=\"true\"],QComboBox#algoField[error=\"true\"],"
                          "QAbstractSpinBox#algoField[error=\"true\"]{border-color:%1;}")
               .arg(danger);
    css += QStringLiteral("QTableView#algoTable{background:%1;alternate-background-color:%2;border:none;color:%3;"
                          "selection-background-color:%4;selection-color:%3;gridline-color:%5;font-size:%6px;}")
               .arg(base, alt, tp, abg, bdim)
               .arg(fonts::px(-2));
    css += QStringLiteral("QTableView#algoTable QHeaderView::section{background:%1;color:%2;border:none;"
                          "border-bottom:1px solid %3;padding:2px 6px;font-weight:700;font-size:%4px;}")
               .arg(surface, tt, bdim)
               .arg(fonts::px(-4));
    css += QStringLiteral("QScrollArea#algoScroll{background:transparent;border:none;}");
    css += QStringLiteral("QScrollArea#algoScroll > QWidget > QWidget{background:transparent;}");
    css += QStringLiteral("QTabBar#algoTabs{background:%1;border-bottom:1px solid %2;}").arg(surface, bdim);
    css += QStringLiteral("QTabBar#algoTabs::tab{background:%1;color:%2;padding:4px 12px;border:none;"
                          "border-bottom:2px solid transparent;font-weight:700;font-size:%3px;}")
               .arg(surface, ts)
               .arg(fonts::px(-3));
    css += QStringLiteral("QTabBar#algoTabs::tab:selected{color:%1;border-bottom-color:%1;background:%2;}").arg(accent, base);
    css += QStringLiteral("QTabBar#algoTabs::tab:hover{color:%1;}").arg(tp);
    css += QStringLiteral("QSplitter#algoSplitter::handle{background:%1;}").arg(bdim);
    css += QStringLiteral("QSplitter#algoSplitter::handle:horizontal{width:1px;}");
    css += QStringLiteral("QSplitter#algoSplitter::handle:vertical{height:1px;}");
    css += QStringLiteral("QFrame#algoRuleRow{background:transparent;border:none;border-bottom:1px solid %1;}").arg(bdim);
    css += QStringLiteral("QFrame#algoRuleRow[invalid=\"true\"]{background:%1;}").arg(colors::tint(QColor(danger), 0.10));
    css += QStringLiteral("QFrame#algoRuleGroup{background:transparent;border:none;border-left:1px solid %1;}").arg(bmed);
    css += QStringLiteral("QProgressBar#algoProgress{background:%1;border:none;}").arg(surface);
    css += QStringLiteral("QProgressBar#algoProgress::chunk{background:%1;}").arg(accent);
    css += QStringLiteral("QLabel#algoHero{color:%1;font-weight:700;font-family:%2;font-size:%3px;background:transparent;}")
               .arg(tp, family)
               .arg(fonts::px(6));
    css += QStringLiteral("QLabel#algoKey{color:%1;font-size:%2px;background:transparent;}").arg(tt).arg(fonts::px(-2));
    css += QStringLiteral("QPlainTextEdit#algoLog,QTextEdit#algoLog,QTextBrowser#algoLog{background:%1;color:%2;"
                          "border:none;font-family:%3;font-size:%4px;selection-background-color:%5;}")
               .arg(base, ts, family)
               .arg(fonts::px(-2))
               .arg(abg);
    css += QStringLiteral("QListView#algoList,QListWidget#algoList{background:%1;color:%2;border:none;"
                          "font-size:%3px;selection-background-color:%4;selection-color:%2;}")
               .arg(base, tp)
               .arg(fonts::px(-2))
               .arg(abg);
    css += QStringLiteral("QCheckBox#algoCheck{color:%1;font-size:%2px;spacing:4px;background:transparent;}")
               .arg(ts)
               .arg(fonts::px(-2));
    css += QStringLiteral("QCheckBox#algoCheck:disabled{color:%1;}").arg(td);
    css += QStringLiteral("QFrame#algoHairline{background:%1;border:none;max-height:1px;min-height:1px;}").arg(bdim);
    css += QStringLiteral("QWidget#algoDetail{background:%1;}").arg(surface);
    return css;
}

// ── Text / tones ────────────────────────────────────────────────────────────

QString dash() {
    return QStringLiteral("—");
}

const char* tone_key(Tone tone) {
    switch (tone) {
        case Tone::Pos:
            return "pos";
        case Tone::Neg:
            return "neg";
        case Tone::Warn:
            return "warn";
        case Tone::Dim:
            return "dim";
        case Tone::Accent:
            return "accent";
        case Tone::Ok:
            return "ok";
        case Tone::Danger:
            return "danger";
        case Tone::Info:
            return "info";
        case Tone::None:
            break;
    }
    return "";
}

Tone sign_tone(double v) {
    if (!std::isfinite(v) || v == 0)
        return Tone::None;
    return v > 0 ? Tone::Pos : Tone::Neg;
}

void set_tone(QWidget* w, Tone tone) {
    if (!w)
        return;
    const QString key = QString::fromLatin1(tone_key(tone));
    if (w->property("tone").toString() == key)
        return;
    w->setProperty("tone", key);
    w->style()->unpolish(w);
    w->style()->polish(w);
    w->update();
}

QColor tone_color(Tone tone) {
    using namespace fincept::ui;
    switch (tone) {
        case Tone::Pos:
            return QColor(colors::POSITIVE());
        case Tone::Neg:
            return QColor(colors::NEGATIVE());
        case Tone::Warn:
            return QColor(colors::WARNING());
        case Tone::Dim:
            return QColor(colors::TEXT_TERTIARY());
        case Tone::Accent:
            return QColor(colors::AMBER());
        case Tone::Ok:
            return QColor(colors::SUCCESS());
        case Tone::Danger:
            return QColor(colors::DANGER());
        case Tone::Info:
            return QColor(colors::INFO());
        case Tone::None:
            break;
    }
    return QColor(colors::TEXT_PRIMARY());
}

// ── Panels and strips ───────────────────────────────────────────────────────

Panel make_panel(const QString& title, QWidget* parent, int body_margin) {
    Panel p;
    p.frame = new QFrame(parent);
    p.frame->setObjectName(QStringLiteral("algoPanel"));
    p.frame->setAttribute(Qt::WA_StyledBackground, true);
    auto* v = new QVBoxLayout(p.frame);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    p.header = new ui::PanelHeader(title, p.frame);
    v->addWidget(p.header);
    p.body_widget = new QWidget(p.frame);
    p.body_widget->setObjectName(QStringLiteral("algoPanelBody"));
    p.body = new QVBoxLayout(p.body_widget);
    p.body->setContentsMargins(body_margin, body_margin, body_margin, body_margin);
    p.body->setSpacing(6);
    v->addWidget(p.body_widget, 1);
    return p;
}

QWidget* make_strip(QWidget* parent, ui::FlowLayout** flow_out) {
    auto* strip = new QWidget(parent);
    strip->setObjectName(QStringLiteral("algoStrip"));
    strip->setAttribute(Qt::WA_StyledBackground, true);
    // A band never grows taller than its (wrapped) rows: no blank band under it.
    strip->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto* flow = new ui::FlowLayout(strip);
    flow->setContentsMargins(8, 4, 8, 4);
    if (flow_out)
        *flow_out = flow;
    return strip;
}

QWidget* field_group(QWidget* parent, const QString& caption, std::initializer_list<QWidget*> widgets,
                     const QString& tooltip) {
    auto* g = new QWidget(parent);
    g->setObjectName(QStringLiteral("algoFieldGroup"));
    auto* h = new QHBoxLayout(g);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(4);
    if (!caption.isEmpty()) {
        auto* l = new QLabel(caption, g);
        l->setObjectName(QStringLiteral("algoFieldLabel"));
        if (!tooltip.isEmpty())
            l->setToolTip(tooltip);
        h->addWidget(l);
        // The caption names the first input for screen readers.
        if (widgets.size() > 0 && *widgets.begin())
            l->setBuddy(*widgets.begin());
    }
    for (QWidget* w : widgets) {
        if (!w)
            continue;
        if (w->parentWidget() != g)
            w->setParent(g);
        h->addWidget(w);
    }
    return g;
}

void mark_field(QWidget* input, const QString& accessible_name, const QString& tooltip) {
    if (!input)
        return;
    input->setObjectName(QStringLiteral("algoField"));
    input->setAccessibleName(accessible_name);
    if (!tooltip.isEmpty())
        input->setToolTip(tooltip);
}

QLabel* make_note(const QString& text, QWidget* parent, Tone tone) {
    auto* l = new QLabel(text, parent);
    l->setObjectName(QStringLiteral("algoNote"));
    l->setWordWrap(true);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    set_tone(l, tone);
    return l;
}

QLabel* make_chip(const QString& text, Tone tone, QWidget* parent) {
    auto* l = new QLabel(text, parent);
    l->setObjectName(QStringLiteral("algoChip"));
    set_tone(l, tone);
    return l;
}

QPushButton* make_tool_button(const QString& text, const QString& tooltip, QWidget* parent) {
    auto* b = new QPushButton(text, parent);
    b->setObjectName(QStringLiteral("algoToolButton"));
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::StrongFocus);
    if (!tooltip.isEmpty())
        b->setToolTip(tooltip);
    QString name = text;
    name.replace(QStringLiteral("&&"), QStringLiteral("&"));
    b->setAccessibleName(name);
    return b;
}

QPushButton* make_primary_button(const QString& text, const QString& tooltip, QWidget* parent) {
    QPushButton* b = make_tool_button(text, tooltip, parent);
    b->setObjectName(QStringLiteral("algoPrimary"));
    return b;
}

QWidget* make_segmented(const QStringList& labels, int checked, std::function<void(int)> on_change, QWidget* parent) {
    auto* box = new QWidget(parent);
    auto* h = new QHBoxLayout(box);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(0);
    auto* group = new QButtonGroup(box);
    group->setExclusive(true);
    for (int i = 0; i < labels.size(); ++i) {
        auto* b = new QPushButton(labels.at(i), box);
        b->setObjectName(QStringLiteral("algoSeg"));
        b->setCheckable(true);
        b->setChecked(i == checked);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::StrongFocus);
        QString name = labels.at(i);
        name.replace(QStringLiteral("&&"), QStringLiteral("&"));
        b->setAccessibleName(name);
        group->addButton(b, i);
        h->addWidget(b);
    }
    if (on_change)
        QObject::connect(group, &QButtonGroup::idClicked, box, [on_change](int id) { on_change(id); });
    return box;
}

void set_segment(QWidget* segmented, int index) {
    if (!segmented)
        return;
    // An exclusive group refuses to uncheck its checked button: index < 0 ("none", e.g. a
    // custom date range) lifts the exclusivity for the moment.
    auto* group = segmented->findChild<QButtonGroup*>(QString(), Qt::FindDirectChildrenOnly);
    if (group && index < 0)
        group->setExclusive(false);
    const auto buttons = segmented->findChildren<QPushButton*>(QString(), Qt::FindDirectChildrenOnly);
    for (int i = 0; i < buttons.size(); ++i)
        buttons.at(i)->setChecked(i == index);
    if (group && index < 0)
        group->setExclusive(true);
}

QSplitter* make_splitter(Qt::Orientation orientation, QWidget* parent) {
    auto* s = new QSplitter(orientation, parent);
    s->setObjectName(QStringLiteral("algoSplitter"));
    s->setChildrenCollapsible(false);
    s->setHandleWidth(1);
    return s;
}

// ── Tables ──────────────────────────────────────────────────────────────────

void setup_table(QTableView* view, const QString& export_name) {
    if (!view)
        return;
    view->setObjectName(QStringLiteral("algoTable"));
    view->verticalHeader()->hide();
    view->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    view->verticalHeader()->setDefaultSectionSize(atk_row_height(view));
    view->setAlternatingRowColors(true);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setSelectionMode(QAbstractItemView::SingleSelection);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->setShowGrid(false);
    view->setWordWrap(false);
    view->setTextElideMode(Qt::ElideRight);
    view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    view->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    view->horizontalHeader()->setStretchLastSection(true);
    view->horizontalHeader()->setHighlightSections(false);
    view->horizontalHeader()->setSortIndicatorShown(true);
    view->horizontalHeader()->setMinimumSectionSize(24);
    view->setSortingEnabled(true);
    ui::ChartExport::install_table_context_menu(view, export_name);

    auto* copy = new QShortcut(QKeySequence::Copy, view);
    copy->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(copy, &QShortcut::activated, view, [view]() {
        const QAbstractItemModel* m = view->model();
        if (!m || !view->selectionModel())
            return;
        QModelIndexList rows = view->selectionModel()->selectedRows();
        std::sort(rows.begin(), rows.end(), [](const QModelIndex& a, const QModelIndex& b) { return a.row() < b.row(); });
        if (rows.isEmpty())
            return;
        QStringList lines;
        QStringList head;
        for (int c = 0; c < m->columnCount(); ++c)
            head << m->headerData(c, Qt::Horizontal).toString();
        lines << head.join(QLatin1Char('\t'));
        for (const QModelIndex& r : rows) {
            QStringList cells;
            for (int c = 0; c < m->columnCount(); ++c)
                cells << m->index(r.row(), c).data(Qt::DisplayRole).toString();
            lines << cells.join(QLatin1Char('\t'));
        }
        QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
    });

    QObject::connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, view, [view]() {
        view->verticalHeader()->setDefaultSectionSize(atk_row_height(view));
        atk_apply_column_chars(view);
    });
}

void set_column_chars(QTableView* view, const QVector<int>& chars) {
    if (!view)
        return;
    QVariantList list;
    for (int c : chars)
        list << c;
    view->setProperty("algoColumnChars", list);
    atk_apply_column_chars(view);
}

void guard_wheel(QWidget* input) {
    if (!input)
        return;
    input->setFocusPolicy(Qt::StrongFocus);
    input->installEventFilter(atk_wheel_guard());
}

} // namespace fincept::screens::algo_kit
