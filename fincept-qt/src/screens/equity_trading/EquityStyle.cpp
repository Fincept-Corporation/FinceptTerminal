// EquityStyle.cpp — see header. Built in sections so every font size is a
// "font-size:%Npx" placeholder filled with .arg(ui::fonts::px(n)).
#include "screens/equity_trading/EquityStyle.h"

#include "ui/theme/Theme.h"

namespace fincept::screens::equity {

namespace {
namespace eqst_c = fincept::ui::colors;

QString eqst_root() {
    return QStringLiteral(
               "#eqScreen{background:%1;}"
               "#eqSymbolInput{background:%2;color:%3;border:1px solid %4;padding:2px 8px;font-size:%5px;"
               "font-weight:700;}"
               "#eqSymbolInput:focus{border-color:%3;}"
               "#eqCommandBarTitle{color:%3;font-weight:700;font-size:%6px;letter-spacing:1px;background:transparent;"
               "border:none;}"
               "#eqHeroPrice{color:%7;font-size:%8px;font-weight:700;background:transparent;border:none;}"
               "QMenu#eqAccountMenu::item:checked{color:%3;}")
        .arg(eqst_c::BG_BASE(), eqst_c::BG_BASE(), eqst_c::AMBER(), eqst_c::BORDER_MED())
        .arg(fincept::ui::fonts::px(-1))
        .arg(fincept::ui::fonts::px(-2))
        .arg(eqst_c::TEXT_PRIMARY())
        .arg(fincept::ui::fonts::px(1));
}

QString eqst_ribbon() {
    return QStringLiteral(
               "#eqChange{color:%1;font-size:%2px;font-weight:700;background:transparent;border:none;}"
               "#eqChange[dir=\"up\"]{color:%3;}"
               "#eqChange[dir=\"down\"]{color:%4;}"
               "#eqBid{color:%3;font-size:%5px;font-weight:600;background:transparent;border:none;}"
               "#eqAsk{color:%4;font-size:%5px;font-weight:600;background:transparent;border:none;}"
               "#eqSpreadInline,#eqStatLabel{color:%1;font-size:%5px;background:transparent;border:none;}"
               "#eqRibbonNotice{color:%6;border:1px solid %6;background:transparent;font-size:%5px;font-weight:700;"
               "padding:1px 6px;}")
        .arg(eqst_c::TEXT_SECONDARY())
        .arg(fincept::ui::fonts::px(-2))
        .arg(eqst_c::POSITIVE(), eqst_c::NEGATIVE())
        .arg(fincept::ui::fonts::px(-3))
        .arg(eqst_c::WARNING());
}

QString eqst_watchlist() {
    return QStringLiteral(
               "#eqWatchlist{background:%1;border:none;}"
               "#eqWatchlistCount{color:%2;font-size:%3px;background:transparent;border:none;}"
               "#eqWatchlistSearch{background:%4;border:none;border-bottom:1px solid %5;color:%6;padding:3px 10px;"
               "font-size:%7px;}"
               "#eqWatchlistSearch:focus{border-bottom-color:%8;}"
               "#eqWatchlistCombo{background:%4;color:%6;border:1px solid %5;padding:1px 6px;font-size:%3px;"
               "font-weight:700;}"
               "#eqWatchlistCombo:focus{border-color:%9;}"
               "#eqWatchlistMenuBtn{color:%2;background:transparent;border:1px solid %5;padding:0px 5px;}"
               "#eqWatchlistMenuBtn:hover{color:%9;border-color:%9;}")
        .arg(eqst_c::BG_SURFACE(), eqst_c::TEXT_SECONDARY())
        .arg(fincept::ui::fonts::px(-3))
        .arg(eqst_c::BG_BASE(), eqst_c::BORDER_DIM(), eqst_c::TEXT_PRIMARY())
        .arg(fincept::ui::fonts::px(-2))
        .arg(eqst_c::BORDER_BRIGHT(), eqst_c::AMBER());
}

QString eqst_chart_book() {
    return QStringLiteral(
               "#eqChart{background:%1;border:none;}"
               "#eqChartHeader{background:%1;border:none;border-bottom:1px solid %2;}"
               "#eqChartTitle{color:%3;font-weight:700;font-size:%4px;letter-spacing:0.5px;background:transparent;"
               "border:none;}"
               "#eqTfBtn{background:transparent;color:%5;border:none;border-bottom:2px solid transparent;"
               "padding:2px 8px;font-size:%4px;font-weight:700;}"
               "#eqTfBtn:hover{color:%6;}"
               "#eqTfBtn[active=\"true\"]{color:%3;border-bottom-color:%3;}"
               "#eqOrderBook{background:%1;border:none;}"
               "#eqObHeader{background:%1;border:none;border-bottom:1px solid %2;}"
               "#eqObTitle{color:%3;font-weight:700;font-size:%4px;letter-spacing:0.5px;background:transparent;"
               "border:none;}"
               "#eqObLevels{color:%5;font-size:%7px;font-weight:600;letter-spacing:0.3px;background:transparent;"
               "border:none;}"
               "#eqObCanvas{background:transparent;}")
        .arg(eqst_c::BG_SURFACE(), eqst_c::BORDER_DIM(), eqst_c::AMBER())
        .arg(fincept::ui::fonts::px(-3))
        .arg(eqst_c::TEXT_SECONDARY(), eqst_c::TEXT_PRIMARY())
        .arg(fincept::ui::fonts::px(-4));
}

// Order ticket (the ticket may override parts of this in its own sheet).
QString eqst_ticket_a() {
    return QStringLiteral(
               "#eqOrderEntry{background:%1;border:none;}"
               "#eqOeHeader{background:%1;border:none;border-bottom:1px solid %2;}"
               "#eqOeTitle{color:%3;font-weight:700;font-size:%4px;letter-spacing:0.5px;background:transparent;"
               "border:none;}"
               "#eqOeMode{font-weight:700;font-size:%5px;letter-spacing:0.5px;background:%6;border:1px solid %2;"
               "border-radius:3px;padding:2px 8px;}"
               "#eqOeScroll{background:%1;border:none;}"
               "#eqOeContent{background:%1;}"
               "#eqOeSep{background:%2;border:none;}"
               "#eqBuyTab{background:%6;color:%7;border:1px solid %8;border-right:none;border-top-left-radius:3px;"
               "border-bottom-left-radius:3px;font-size:%9px;font-weight:800;letter-spacing:1px;}"
               "#eqBuyTab:hover{color:%10;background:%11;}"
               "#eqBuyTab[active=\"true\"]{background:%10;color:%12;border-color:%10;}")
        .arg(eqst_c::BG_SURFACE(), eqst_c::BORDER_DIM(), eqst_c::AMBER())
        .arg(fincept::ui::fonts::px(-3))
        .arg(fincept::ui::fonts::px(-4))
        .arg(eqst_c::BG_BASE(), eqst_c::TEXT_SECONDARY(), eqst_c::BORDER_MED())
        .arg(fincept::ui::fonts::px(-1))
        .arg(eqst_c::POSITIVE(), eqst_c::POSITIVE_BG(), eqst_c::TEXT_ON_ACCENT());
}

QString eqst_ticket_b() {
    return QStringLiteral(
               "#eqSellTab{background:%1;color:%2;border:1px solid %3;border-top-right-radius:3px;"
               "border-bottom-right-radius:3px;font-size:%4px;font-weight:800;letter-spacing:1px;}"
               "#eqSellTab:hover{color:%5;background:%6;}"
               "#eqSellTab[active=\"true\"]{background:%5;color:%7;border-color:%5;}"
               "#eqOeTypeBtn{background:%1;color:%2;border:1px solid %3;border-radius:2px;padding:2px 4px;"
               "font-size:%8px;font-weight:700;}"
               "#eqOeTypeBtn:hover{color:%9;border-color:%10;}"
               "#eqOeTypeBtn[active=\"true\"]{background:%11;color:%7;border-color:%11;}"
               "#eqOeSymbol{color:%9;font-size:%12px;font-weight:700;letter-spacing:0.5px;background:transparent;"
               "border:none;padding:1px 0;}")
        .arg(eqst_c::BG_BASE(), eqst_c::TEXT_SECONDARY(), eqst_c::BORDER_MED())
        .arg(fincept::ui::fonts::px(-1))
        .arg(eqst_c::NEGATIVE(), eqst_c::NEGATIVE_BG(), eqst_c::TEXT_ON_ACCENT())
        .arg(fincept::ui::fonts::px(-3))
        .arg(eqst_c::TEXT_PRIMARY(), eqst_c::BORDER_BRIGHT(), eqst_c::AMBER())
        .arg(fincept::ui::fonts::px(-2));
}

QString eqst_ticket_c() {
    return QStringLiteral(
               "#eqOeInfo{background:%1;border:1px solid %2;border-radius:3px;}"
               "#eqOeMarketPrice{color:%3;font-size:%4px;font-weight:700;background:transparent;border:none;}"
               "#eqOeBalance{color:%5;font-size:%4px;font-weight:700;background:transparent;border:none;}"
               "#eqOeLabel{color:%6;font-size:%7px;font-weight:700;letter-spacing:0.5px;background:transparent;"
               "border:none;}"
               "#eqOeInput{background:%1;border:1px solid %8;color:%5;border-radius:2px;padding:4px 6px;"
               "font-size:%4px;}"
               "#eqOeInput:focus{border-color:%9;}"
               "#eqOeInput:disabled{color:%10;background:%11;}"
               "#eqOeCombo{background:%1;border:1px solid %8;color:%5;border-radius:2px;padding:4px 6px;"
               "font-size:%4px;}"
               "#eqOeCombo:focus{border-color:%9;}"
               "#eqOeCombo::drop-down{border:none;width:18px;}"
               "#eqOeCombo QAbstractItemView{background:%11;color:%5;selection-background-color:%12;border:1px solid "
               "%8;}")
        .arg(eqst_c::BG_BASE(), eqst_c::BORDER_DIM(), eqst_c::CYAN())
        .arg(fincept::ui::fonts::px(-1))
        .arg(eqst_c::TEXT_PRIMARY(), eqst_c::TEXT_SECONDARY())
        .arg(fincept::ui::fonts::px(-3))
        .arg(eqst_c::BORDER_MED(), eqst_c::BORDER_BRIGHT(), eqst_c::TEXT_TERTIARY(), eqst_c::BG_SURFACE(),
             eqst_c::BG_HOVER());
}

QString eqst_ticket_d() {
    return QStringLiteral(
               "#eqOeBrokerage{color:%1;font-size:%2px;background:transparent;border:none;}"
               "#eqOeSummary{background:%3;border:1px solid %4;border-radius:3px;}"
               "#eqOeCost{color:%5;font-size:%6px;font-weight:700;background:transparent;border:none;}"
               "#eqOeMargin{color:%1;font-size:%2px;background:transparent;border:none;}"
               "#eqOeStatus{font-size:%2px;font-weight:600;background:transparent;border:none;}"
               "#eqBuySubmit{background:%7;color:%8;border:1px solid %7;border-radius:3px;padding:9px;"
               "font-weight:800;font-size:%6px;letter-spacing:0.5px;}"
               "#eqBuySubmit:hover{background:%7;border-color:%5;}"
               "#eqSellSubmit{background:%9;color:%8;border:1px solid %9;border-radius:3px;padding:9px;"
               "font-weight:800;font-size:%6px;letter-spacing:0.5px;}"
               "#eqSellSubmit:hover{background:%9;border-color:%5;}"
               "#eqAdvToggle{background:transparent;color:%1;border:none;font-size:%2px;font-weight:700;"
               "letter-spacing:0.5px;padding:3px 0;text-align:left;}"
               "#eqAdvToggle:hover{color:%10;}")
        .arg(eqst_c::TEXT_SECONDARY())
        .arg(fincept::ui::fonts::px(-3))
        .arg(eqst_c::BG_BASE(), eqst_c::BORDER_DIM(), eqst_c::TEXT_PRIMARY())
        .arg(fincept::ui::fonts::px(-1))
        .arg(eqst_c::POSITIVE(), eqst_c::TEXT_ON_ACCENT(), eqst_c::NEGATIVE(), eqst_c::AMBER());
}

// PAPER / LIVE on the ticket: chip + the 2 px rule under its header, in the state tone.
QString eqst_ticket_mode() {
    return QStringLiteral(
               "#eqOeMode[mode=\"paper\"]{color:%1;border-color:%1;background:%2;}"
               "#eqOeMode[mode=\"live\"]{color:%3;border-color:%3;background:%4;}"
               "QFrame#eqOeModeRule{background:%1;border:none;}"
               "QFrame#eqOeModeRule[mode=\"live\"]{background:%3;}")
        .arg(eqst_c::SUCCESS(), eqst_c::tint(eqst_c::SUCCESS, 0.12), eqst_c::DANGER(), eqst_c::tint(eqst_c::DANGER, 0.18));
}

QString eqst_blotter() {
    return QStringLiteral(
               "#eqBottomPanel{background:%1;border:none;}"
               "#eqBottomTabs::pane{border:none;background:%1;}"
               "#eqBottomTabs QTabBar::tab{background:transparent;color:%2;padding:4px 10px;border:none;"
               "border-bottom:2px solid transparent;font-size:%3px;font-weight:700;letter-spacing:0.5px;}"
               "#eqBottomTabs QTabBar::tab:selected{color:%4;border-bottom-color:%4;}"
               "#eqBottomTabs QTabBar::tab:hover:!selected{color:%5;}"
               "#eqTable{background:%6;border:none;color:%5;font-size:%7px;}"
               "#eqTable::item{padding:0 4px;border-bottom:1px solid %8;}"
               "#eqTable QHeaderView::section{background:%1;color:%9;font-size:%3px;font-weight:700;border:none;"
               "border-bottom:1px solid %8;padding:0 4px;}")
        .arg(eqst_c::BG_SURFACE(), eqst_c::TEXT_SECONDARY())
        .arg(fincept::ui::fonts::px(-3))
        .arg(eqst_c::AMBER(), eqst_c::TEXT_PRIMARY(), eqst_c::BG_BASE())
        .arg(fincept::ui::fonts::px(-2))
        .arg(eqst_c::BORDER_DIM(), eqst_c::TEXT_DIM());
}

QString eqst_chrome() {
    return QStringLiteral(
               "QFrame#eqFirstRunNote{background:%1;border:none;border-bottom:1px solid %2;}"
               "QLabel#eqFirstRunText{color:%3;background:transparent;font-size:%4px;}"
               "QToolButton#eqFirstRunClose{color:%5;background:transparent;border:none;padding:0 4px;}"
               "QToolButton#eqFirstRunClose:hover{color:%6;}"
               "QPushButton#eqNoteBtn{background:transparent;color:%6;border:1px solid %6;padding:2px 10px;"
               "font-size:%7px;font-weight:700;}"
               "QPushButton#eqNoteBtn:hover{background:%8;}")
        .arg(eqst_c::ACCENT_BG(), eqst_c::BORDER_DIM(), eqst_c::TEXT_PRIMARY())
        .arg(fincept::ui::fonts::px(-3))
        .arg(eqst_c::TEXT_SECONDARY(), eqst_c::AMBER())
        .arg(fincept::ui::fonts::px(-4))
        .arg(eqst_c::BG_HOVER());
}

} // namespace

QString eq_screen_sheet() {
    return eqst_root() + eqst_ribbon() + eqst_watchlist() + eqst_chart_book() + eqst_ticket_a() + eqst_ticket_b() +
           eqst_ticket_c() + eqst_ticket_d() + eqst_ticket_mode() + eqst_blotter() + eqst_chrome();
}

} // namespace fincept::screens::equity
