#pragma once
// DataSourcesStyles.h — full QSS for the Data Sources screen.
// Substituted with .arg() at runtime so colour tokens stay live.

#include <QString>

namespace fincept::screens::datasources {

// Shared stylesheet for all tab views. Call with .arg(col::BG_BASE,
// col::BG_SURFACE, ...) — the placeholder indices match the original
// inline definition (see DataSourcesScreen::apply_screen_styles).
inline const QString& screen_stylesheet() {
    static const QString kScreenStylesheet = R"(
    * { font-family: 'Consolas','Courier New',monospace; }

    /* ── Root ── */
    #dsScreen { background: %1; }

    /* ── Screen header ── */
    #dsScreenHeader {
        background: %1;
        border-bottom: 1px solid %4;
    }
    #dsScreenTitle {
        color: %5; font-size: 13px; font-weight: 700;
        letter-spacing: 1px; background: transparent;
    }
    #dsScreenSubtitle {
        color: %8; font-size: 11px; background: transparent;
    }
    #dsClock {
        color: %5; font-size: 11px; font-weight: 700;
        background: transparent; letter-spacing: 0.5px;
    }
    #dsSearch {
        background: %2; border: 1px solid %4; color: %10;
        padding: 0 10px; font-size: 12px;
    }
    #dsSearch:focus { border-color: %7; }
    #dsSearch::selection { background: %5; color: %1; }

    /* ── Tab bar ── */
    #dsTabBar { background: %2; border-bottom: 1px solid %4; }
    #dsTab {
        background: transparent; color: %6; border: none;
        border-bottom: 2px solid transparent;
        padding: 0 18px; font-size: 12px; font-weight: 700;
        letter-spacing: 0.5px;
    }
    #dsTab:hover { color: %10; background: %9; }
    #dsTabActive {
        background: transparent; color: %5; border: none;
        border-bottom: 2px solid %5;
        padding: 0 18px; font-size: 12px; font-weight: 700;
        letter-spacing: 0.5px;
    }

    /* ── Stat boxes ── */
    #dsStatRow { background: %3; border-bottom: 1px solid %4; }
    #dsStatBox {
        background: %2; border: 1px solid %4;
        border-right: none;
    }
    #dsStatBox:last-child { border-right: 1px solid %4; }
    #dsStatValue {
        color: %10; font-size: 22px; font-weight: 700;
        background: transparent;
    }
    #dsStatLabel {
        color: %8; font-size: 10px; font-weight: 700;
        letter-spacing: 0.5px; background: transparent;
    }

    /* ── Left sidebar ── */
    #dsSidebar { background: %2; border-right: 1px solid %4; }
    #dsSidebarHeader {
        background: %3; border-bottom: 1px solid %4;
    }
    #dsSidebarTitle {
        color: %5; font-size: 10px; font-weight: 700;
        letter-spacing: 1px; background: transparent;
    }
    #dsCategoryList { background: transparent; border: none; outline: none; }
    #dsCategoryList::item {
        padding: 0 14px; height: 30px; color: %6;
        font-size: 12px; border-bottom: 1px solid %4;
    }
    #dsCategoryList::item:selected {
        background: rgba(217,119,6,0.1); color: %5;
        border-left: 2px solid %5;
    }
    #dsCategoryList::item:hover:!selected { background: %9; color: %10; }

    /* ── Provider ladder ── */
    #dsProviderHeader { background: %3; border-bottom: 1px solid %4; border-top: 1px solid %4; }
    #dsProviderLadder { background: transparent; border: none; outline: none; }
    #dsProviderLadder::item {
        padding: 0 14px; height: 26px; color: %6;
        font-size: 11px; border-bottom: 1px solid %4;
    }
    #dsProviderLadder::item:selected { background: %9; color: %10; }
    #dsProviderLadder::item:hover:!selected { background: %9; }

    /* ── Connector table ── */
    #dsConnPanel { background: %1; border: none; }
    #dsConnHeader { background: %3; border-bottom: 1px solid %4; }
    #dsConnPanelTitle {
        color: %5; font-size: 10px; font-weight: 700;
        letter-spacing: 1px; background: transparent;
    }
    #dsConnCount {
        color: %8; font-size: 11px; background: transparent;
    }
    #dsConnectorTable {
        background: %1; border: none; color: %10;
        font-size: 12px; gridline-color: transparent;
        selection-background-color: rgba(217,119,6,0.1);
        selection-color: %10;
    }
    #dsConnectorTable::item {
        padding: 0 8px; border-bottom: 1px solid %4; height: 26px;
    }
    #dsConnectorTable::item:hover:!selected { background: %9; }
    #dsConnectorTable QHeaderView::section {
        background: %2; color: %8; font-size: 10px; font-weight: 700;
        letter-spacing: 0.5px; border: none;
        border-bottom: 1px solid %4; border-right: 1px solid %4;
        padding: 0 8px; height: 26px;
    }
    #dsConnectorTable QHeaderView::section:last { border-right: none; }

    /* ── Detail panel ── */
    #dsDetail { background: %2; border-left: 1px solid %4; }
    #dsDetailHeader { background: %3; border-bottom: 1px solid %4; }
    #dsDetailTitle {
        color: %5; font-size: 10px; font-weight: 700;
        letter-spacing: 1px; background: transparent;
    }
    #dsDetailSymbol {
        min-width: 38px; max-width: 38px;
        min-height: 38px; max-height: 38px;
        font-size: 13px; font-weight: 700;
        color: %5; background: %1; border: 1px solid %5;
    }
    #dsDetailName {
        color: %10; font-size: 13px; font-weight: 700;
        background: transparent;
    }
    #dsDetailDesc {
        color: %6; font-size: 11px; background: transparent;
    }
    #dsDetailEmpty {
        color: %8; font-size: 12px; background: transparent;
    }
    #dsInfoRow { background: transparent; border-bottom: 1px solid %4; }
    #dsInfoLabel {
        color: %8; font-size: 11px; font-weight: 700;
        letter-spacing: 0.5px; background: transparent;
    }
    #dsInfoValue {
        color: %10; font-size: 11px; font-weight: 700;
        background: transparent;
    }
    #dsSectionSep {
        color: %5; font-size: 10px; font-weight: 700;
        letter-spacing: 1px; background: transparent;
    }

    /* ── Field table ── */
    #dsFieldTable {
        background: %2; border: none; color: %10;
        font-size: 11px; gridline-color: transparent;
        selection-background-color: %9;
    }
    #dsFieldTable::item {
        padding: 0 6px; border-bottom: 1px solid %4; height: 24px;
    }
    #dsFieldTable QHeaderView::section {
        background: %2; color: %8; font-size: 10px; font-weight: 700;
        border: none; border-bottom: 1px solid %4;
        border-right: 1px solid %4; padding: 0 6px; height: 24px;
    }

    /* ── Detail connection list ── */
    #dsDetailConnList { background: transparent; border: none; outline: none; }
    #dsDetailConnList::item {
        padding: 0 10px; height: 26px; color: %6;
        font-size: 11px; border-bottom: 1px solid %4;
    }
    #dsDetailConnList::item:selected { background: %9; color: %10; }
    #dsDetailConnList::item:hover:!selected { background: %9; }

    /* ── Connections panel ── */
    #dsConnsPanel { background: %1; border: none; }
    #dsConnsHeader { background: %3; border-bottom: 1px solid %4; }
    #dsConnsToolbar { background: %2; border-bottom: 1px solid %4; }
    #dsConnectionsTable {
        background: %1; border: none; color: %10;
        font-size: 12px; gridline-color: transparent;
        selection-background-color: rgba(217,119,6,0.1);
        selection-color: %10;
    }
    #dsConnectionsTable::item {
        padding: 0 8px; border-bottom: 1px solid %4; height: 26px;
    }
    #dsConnectionsTable::item:hover:!selected { background: %9; }
    #dsConnectionsTable QHeaderView::section {
        background: %2; color: %8; font-size: 10px; font-weight: 700;
        letter-spacing: 0.5px; border: none;
        border-bottom: 1px solid %4; border-right: 1px solid %4;
        padding: 0 8px; height: 26px;
    }

    /* ── Buttons ── */
    #dsBtn {
        background: %2; color: %6; border: 1px solid %4;
        padding: 0 10px; font-size: 11px; font-weight: 700;
        letter-spacing: 0.5px;
    }
    #dsBtn:hover:enabled { background: %9; color: %10; border-color: %7; }
    #dsBtn:disabled { color: %8; opacity: 0.4; }
    #dsBtnAccent {
        background: rgba(217,119,6,0.08); color: %5;
        border: 1px solid %12; padding: 0 10px;
        font-size: 11px; font-weight: 700; letter-spacing: 0.5px;
    }
    #dsBtnAccent:hover:enabled { background: %5; color: %1; }
    #dsBtnAccent:disabled { opacity: 0.4; }
    #dsBtnDanger {
        background: rgba(220,38,38,0.08); color: %14;
        border: 1px solid %15; padding: 0 10px;
        font-size: 11px; font-weight: 700; letter-spacing: 0.5px;
    }
    #dsBtnDanger:hover:enabled { background: rgba(220,38,38,0.18); border-color: %14; }
    #dsBtnDanger:disabled { opacity: 0.4; }
    #dsBtnGreen {
        background: rgba(22,163,74,0.08); color: %13;
        border: 1px solid %16; padding: 0 10px;
        font-size: 11px; font-weight: 700; letter-spacing: 0.5px;
    }
    #dsBtnGreen:hover:enabled { background: rgba(22,163,74,0.18); border-color: %13; }
    #dsBtnGreen:disabled { opacity: 0.4; }
    #dsSearchConn {
        background: %2; border: 1px solid %4; color: %10;
        padding: 0 8px; font-size: 11px;
    }
    #dsSearchConn:focus { border-color: %7; }
    #dsEnableToggle { color: %10; background: transparent; }
    #dsEnableToggle::indicator {
        width: 13px; height: 13px;
        border: 1px solid %7; background: %2;
    }
    #dsEnableToggle::indicator:checked { background: %13; border-color: %13; }
    #dsStatusDot { font-size: 11px; font-weight: 700; background: transparent; }

    /* ── Splitters ── */
    #dsBrowseSplitter::handle, #dsConnsSplitter::handle {
        background: %4; width: 1px; height: 1px;
    }

    /* ── Scrollbars ── */
    QScrollBar:vertical { background: transparent; width: 5px; margin: 0; }
    QScrollBar::handle:vertical { background: %7; min-height: 20px; }
    QScrollBar::handle:vertical:hover { background: %11; }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    QScrollBar:horizontal { background: transparent; height: 5px; margin: 0; }
    QScrollBar::handle:horizontal { background: %7; min-width: 20px; }
    QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
)";
    return kScreenStylesheet;
}

} // namespace fincept::screens::datasources
