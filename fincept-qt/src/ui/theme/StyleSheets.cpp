#include "ui/theme/StyleSheets.h"

#include "ui/theme/Theme.h"

namespace fincept::ui::styles {

QString card_frame() {
    return QString("QFrame { background: %1; border: 1px solid %2; border-radius: 2px; }")
        .arg(colors::CARD_BG(), colors::BORDER());
}

QString card_title() {
    return QString("color: %1; font-size: 13px; font-weight: bold; background: transparent;").arg(colors::GRAY());
}

QString card_close_button() {
    return QString("QPushButton { color: %1; background: transparent; border: none; font-size: 12px; }"
                   "QPushButton:hover { color: %2; }")
        .arg(colors::MUTED(), colors::GRAY());
}

QString section_header() {
    return QString("color: %1; font-size: 12px; font-weight: bold; "
                   "background: transparent; padding: 2px 0; "
                   "border-bottom: 1px solid %2;")
        .arg(colors::MUTED(), colors::BORDER());
}

QString data_label() {
    return QString("color: %1; font-size: 13px; background: transparent;").arg(colors::GRAY());
}

QString data_value() {
    return QString("color: %1; font-size: 13px; background: transparent;").arg(colors::WHITE());
}

QString accent_button() {
    return QString("QPushButton { background: %1; color: %2; border: 1px solid %3; "
                   "padding: 4px 12px; font-size: 12px; font-weight: bold; }"
                   "QPushButton:hover { background: %4; color: %2; }")
        .arg(colors::PANEL(), colors::ORANGE(), colors::BORDER(), colors::BG_RAISED());
}

QString muted_button() {
    return QString("QPushButton { background: %1; color: %2; border: 1px solid %3; "
                   "padding: 4px 12px; font-size: 12px; }"
                   "QPushButton:hover { background: %4; color: %5; }")
        .arg(colors::PANEL(), colors::MUTED(), colors::BORDER(), colors::BG_RAISED(), colors::GRAY());
}

QString news_screen_styles() {
    return QString(
               /* ── NewsScreen root ── */
               "#newsScreen { background: %1; }"

               /* ── Command bar (two-row header) ── */
               "#newsCommandBar { background: %2; border-bottom: 1px solid %3; }"
               "#newsCommandRow { background: %2; border-bottom: 1px solid %3; }"
               "#newsCommandBarSearch { background: %1; color: %4; border: 1px solid %3; "
               "  padding: 2px 8px; font-size: 12px; }"
               "#newsCommandBarSearch:focus { border-color: %5; }"
               "#newsCommandBarPill { background: transparent; color: %6; border: 1px solid %3; "
               "  padding: 0 5px; font-size: 10px; font-weight: 600; letter-spacing: 0.3px; }"
               "#newsCommandBarPill:hover { color: %7; background: %8; }"
               "#newsCommandBarPill[active=\"true\"] { background: %9; color: %4; border-color: %10; }"
               "#newsCommandBarSep { color: %3; font-size: 12px; background: transparent; }"
               "#newsCommandBarCount { color: %7; font-size: 12px; background: transparent; padding: 0 4px; }"
               "#newsCommandBarAlert { color: %11; font-size: 11px; font-weight: 700; "
               "  background: transparent; padding: 0 6px; }"
               "#newsCommandBarUnseen { color: %12; font-size: 11px; font-weight: 700; "
               "  background: transparent; padding: 0 6px; }"
               "#newsCommandBarProgress { color: %6; font-size: 11px; background: transparent; padding: 0 4px; }"
               "#newsCommandBarRefresh { background: %8; color: %7; border: 1px solid %3; "
               "  padding: 0 10px; font-size: 11px; font-weight: 700; }"
               "#newsCommandBarRefresh:hover { color: %4; background: %13; }"
               "#newsCommandBarRefresh:disabled { color: %6; }"
               "#newsCommandBarCombo { background: %1; color: %7; border: 1px solid %3; "
               "  padding: 1px 4px; font-size: 11px; }"

               "#newsCommandRowScroll { background: transparent; border: none; }"

               /* ── Drawer toggle button ── */
               "#newsDrawerBtn { background: rgba(217,119,6,0.08); color: %12; "
               "  border: 1px solid %10; padding: 0 10px; font-size: 11px; "
               "  font-weight: 700; letter-spacing: 0.5px; }"
               "#newsDrawerBtn:hover { background: rgba(217,119,6,0.15); }"
               "#newsDrawerBtn:checked { background: %12; color: %1; }"

               /* ── Intel strip (row 2 of header) ── */
               "#newsIntelStrip { background: %8; border-bottom: 1px solid %3; }"
               "#newsIntelLabel { color: %6; font-size: 10px; font-weight: 700; "
               "  letter-spacing: 0.5px; background: transparent; }"
               "#newsIntelValue { color: %14; font-size: 11px; font-weight: 700; background: transparent; }"
               "#newsIntelSep { color: %3; font-size: 10px; background: transparent; }"
               "#newsIntelScore { color: %7; font-size: 11px; font-weight: 700; background: transparent; }"
               "#newsIntelMonitors { color: %7; font-size: 11px; background: transparent; }"
               "#newsIntelDeviations { color: %11; font-size: 11px; font-weight: 700; background: transparent; }"
               "#newsSentimentBull { background: %15; }"
               "#newsSentimentNeut { background: %16; }"
               "#newsSentimentBear { background: %11; }"

               /* ── Content area ── */
               "#newsContentArea { background: %1; }"

               /* ── Feed list ── */
               "#newsFeedList { background: %1; border: none; outline: none; }"
               "#newsFeedList::item { border: none; padding: 0; }"
               "#newsFeedList::item:selected { background: transparent; }"
               "#newsFeedList::item:hover { background: transparent; }"

               /* ── Breaking banner ── */
               "#newsBreakingBanner { background: %17; border-bottom: 1px solid %11; }"
               "#newsBreakingTag { color: %4; background: %11; font-size: 10px; font-weight: 700; "
               "  letter-spacing: 1px; padding: 1px 4px; }"
               "#newsBreakingHeadline { color: %4; font-size: 12px; font-weight: 700; background: transparent; }"
               "#newsBreakingSource { color: %14; font-size: 11px; background: transparent; }"
               "#newsBreakingDismiss { color: %6; background: transparent; border: none; font-size: 12px; }"
               "#newsBreakingDismiss:hover { color: %4; }"

               /* ── Intel drawer (left overlay) ── */
               "#newsDrawerPanel { background: %2; border-right: 1px solid %5; }"
               "#newsDrawerHeader { background: %8; border-bottom: 1px solid %3; }"
               "#newsDrawerTitle { color: %12; font-size: 12px; font-weight: 700; "
               "  letter-spacing: 1px; background: transparent; }"
               "#newsDrawerCloseBtn { color: %6; background: transparent; border: 1px solid %3; "
               "  font-size: 12px; font-weight: 700; }"
               "#newsDrawerCloseBtn:hover { color: %4; background: %8; }"
               "#newsDrawerScroll { background: transparent; }"
               "#newsDrawerContent { background: transparent; }"
               "#newsDrawerSectionTitle { color: %12; font-size: 11px; font-weight: 700; "
               "  letter-spacing: 0.5px; background: transparent; padding: 4px 0 2px; "
               "  border-bottom: 1px solid %3; }"

               /* ── Top stories ── */
               "#newsTopStoryBtn { background: transparent; color: %7; font-size: 11px; "
               "  text-align: left; border: none; border-bottom: 1px solid %3; padding: 3px 4px; }"
               "#newsTopStoryBtn:hover { color: %4; background: %8; }"

               /* ── Categories ── */
               "#newsCategoryBtn { background: transparent; color: %7; font-size: 11px; "
               "  text-align: left; border: none; border-bottom: 1px solid %3; padding: 2px 4px; }"
               "#newsCategoryBtn:hover { color: %4; background: %8; }"

               /* ── Monitors ── */
               "#newsMonitorRow { background: transparent; }"
               "#newsMonitorLabel { color: %7; font-size: 11px; background: transparent; }"
               "#newsMonitorInput { background: %1; color: %4; border: 1px solid %3; "
               "  padding: 2px 6px; font-size: 11px; }"
               "#newsMonitorInput:focus { border-color: %5; }"
               "#newsMonitorAddBtn { background: %8; color: %12; border: 1px solid %3; "
               "  font-size: 12px; font-weight: 700; }"
               "#newsMonitorAddBtn:hover { color: %4; }"
               "#newsMonitorToggleOn { background: transparent; color: %15; border: 1px solid %3; "
               "  font-size: 9px; font-weight: 700; }"
               "#newsMonitorToggleOff { background: transparent; color: %6; border: 1px solid %3; "
               "  font-size: 9px; }"
               "#newsMonitorDeleteBtn { background: transparent; color: %6; border: 1px solid %3; font-size: 10px; }"
               "#newsMonitorDeleteBtn:hover { color: %11; }"

               /* ── Deviations ── */
               "#newsDeviationCategory { color: %7; font-size: 11px; background: transparent; }"
               "#newsDeviationScore { color: %11; font-size: 11px; font-weight: 700; background: transparent; }"

               /* ── Detail overlay (right panel) ── */
               "#newsDetailOverlay { background: %2; border-left: 1px solid %5; }"
               "#newsDetailHeader { background: %8; border-bottom: 1px solid %3; }"
               "#newsDetailHeaderTitle { color: %12; font-size: 12px; font-weight: 700; "
               "  letter-spacing: 1px; background: transparent; }"
               "#newsDetailCloseBtn { color: %6; background: transparent; border: 1px solid %3; "
               "  font-size: 12px; font-weight: 700; }"
               "#newsDetailCloseBtn:hover { color: %4; background: %8; }"
               "#newsDetailContent { background: transparent; }"
               "#newsDetailEmpty { color: %6; font-size: 13px; background: transparent; }"
               "#newsDetailHeadline { color: %4; font-size: 15px; font-weight: 700; background: transparent; }"
               "#newsDetailPriorityBadge { font-size: 10px; font-weight: 700; padding: 1px 6px; }"
               "#newsDetailSentimentBadge { font-size: 11px; background: transparent; }"
               "#newsDetailTierBadge { color: %12; font-size: 10px; font-weight: 700; background: transparent; }"
               "#newsDetailCategory { color: %7; font-size: 11px; background: transparent; }"
               "#newsDetailSource { color: %14; font-size: 12px; font-weight: 700; background: transparent; }"
               "#newsDetailTime { color: %6; font-size: 11px; background: transparent; }"
               "#newsDetailSummary { color: %7; font-size: 12px; line-height: 1.4; background: transparent; }"
               "#newsDetailImpact { color: %16; font-size: 11px; background: transparent; }"
               "#newsDetailTickers { color: %16; font-size: 11px; font-weight: 700; background: transparent; }"
               "#newsDetailSep { background: %3; }"
               "#newsDetailSectionTitle { color: %12; font-size: 11px; font-weight: 700; "
               "  letter-spacing: 0.5px; background: transparent; padding: 2px 0; "
               "  border-bottom: 1px solid %3; }"
               "#newsDetailSubTitle { color: %6; font-size: 10px; font-weight: 700; background: transparent; }"
               "#newsDetailOpenBtn, #newsDetailCopyBtn, #newsDetailSaveBtn { background: %8; color: %7; "
               "  border: 1px solid %3; font-size: 11px; font-weight: 700; padding: 0 8px; }"
               "#newsDetailOpenBtn:hover, #newsDetailCopyBtn:hover, #newsDetailSaveBtn:hover { color: %4; background: %13; }"
               "#newsDetailAnalyzeBtn { background: rgba(217,119,6,0.1); color: %12; "
               "  border: 1px solid %10; font-size: 11px; font-weight: 700; padding: 0 10px; }"
               "#newsDetailAnalyzeBtn:hover { background: %12; color: %1; }"
               "#newsDetailAnalyzeBtn:disabled { color: %6; background: %8; border-color: %3; }"
               "#newsDetailAiSummary { color: %7; font-size: 12px; background: transparent; }"
               "#newsDetailAiSentiment { font-size: 12px; font-weight: 700; background: transparent; }"
               "#newsDetailAiUrgency { color: %16; font-size: 12px; background: transparent; }"
               "#newsDetailKeyPoint { color: %7; font-size: 11px; background: transparent; }"
               "#newsDetailRisk { color: %11; font-size: 11px; background: transparent; }"
               "#newsDetailTopicBadge { color: %14; font-size: 10px; background: %8; "
               "  border: 1px solid %3; padding: 1px 6px; }"
               "#newsRelatedBtn { background: transparent; color: %7; font-size: 11px; "
               "  text-align: left; border: none; border-bottom: 1px solid %3; padding: 3px 4px; }"
               "#newsRelatedBtn:hover { color: %4; background: %8; }"
               "#newsMonitorMatchLabel { color: %7; font-size: 11px; background: transparent; }"

               /* ── Ticker strip ── */
               "#newsTickerStrip { background: %1; border-top: 1px solid %3; }"

               /* ── Skeleton ── */
               "#newsSkeletonOverlay { background: %1; }"

               /* ── Empty state ── */
               "#newsEmptyState { background: %1; }"
               "#newsEmptyStateTitle { color: %12; font-size: 13px; font-weight: 700; "
               "  letter-spacing: 0.5px; background: transparent; }"
               "#newsEmptyStateHint { color: %6; font-size: 11px; background: transparent; }")
        .arg(colors::BG_BASE())        // %1
        .arg(colors::BG_SURFACE())     // %2
        .arg(colors::BORDER_DIM())     // %3
        .arg(colors::TEXT_PRIMARY())   // %4
        .arg(colors::BORDER_BRIGHT())  // %5
        .arg(colors::TEXT_TERTIARY())  // %6
        .arg(colors::TEXT_SECONDARY()) // %7
        .arg(colors::BG_RAISED())      // %8
        .arg(colors::ORANGE())         // %9
        .arg(colors::AMBER_DIM())      // %10
        .arg(colors::NEGATIVE())       // %11
        .arg(colors::AMBER())          // %12
        .arg(colors::BG_HOVER())       // %13
        .arg(colors::CYAN())           // %14
        .arg(colors::POSITIVE())       // %15
        .arg(colors::WARNING())        // %16
        .arg(colors::NEGATIVE_BG())    // %17
        ;
}

QString crypto_trading_styles() {
    return QString(
               /* ── Root ── */
               "#cryptoScreen { background: %1; font-family: 'Consolas','Courier New',monospace; }"

               /* ── Command bar ── */
               "#cryptoCommandBar { background: %8; border-bottom: 1px solid %3; }"
               "#cryptoCommandBarTitle { color: %12; font-weight: 700; font-size: 12px; "
               "  letter-spacing: 1px; background: transparent; border: none; }"
               "#cryptoCommandBarSep { color: %3; font-size: 14px; background: transparent; border: none; }"

               "#cryptoExchangeBtn { background: %16; color: %12; "
               "  border: 1px solid %10; padding: 2px 10px; font-size: 11px; "
               "  font-weight: 700; letter-spacing: 0.5px; }"
               "#cryptoExchangeBtn:hover { background: %12; color: %1; }"
               "#cryptoExchangeBtn::menu-indicator { width: 0; height: 0; }"

               "#cryptoSymbolInput { background: %2; color: %12; border: 1px solid %3; "
               "  padding: 2px 8px; font-size: 13px; font-weight: 700; }"
               "#cryptoSymbolInput:focus { border-color: %12; }"

               /* ── Price ribbon ── */
               "#cryptoPriceRibbon { background: transparent; border: none; }"
               "#cryptoHeroPrice { color: %4; font-size: 18px; font-weight: 700; "
               "  background: transparent; border: none; }"
               "#cryptoChange { font-size: 12px; font-weight: 700; background: transparent; border: none; }"
               "#cryptoBid { color: %15; font-size: 11px; font-weight: 600; background: transparent; border: none; }"
               "#cryptoAsk { color: %11; font-size: 11px; font-weight: 600; background: transparent; border: none; }"
               "#cryptoSpreadInline { color: %6; font-size: 11px; background: transparent; border: none; }"
               "#cryptoStatLabel { color: %6; font-size: 11px; background: transparent; border: none; }"

               "#cryptoModeBtn { padding: 2px 10px; font-weight: 700; font-size: 11px; letter-spacing: 0.5px; }"
               "#cryptoModeBtn[mode=\"paper\"] { background: %17; color: %15; border: 1px solid "
               "%19; }"
               "#cryptoModeBtn[mode=\"live\"] { background: %18; color: %11; border: 1px solid "
               "%20; }"
               "#cryptoApiBtn { background: %8; color: %7; border: 1px solid %3; "
               "  padding: 2px 10px; font-size: 11px; font-weight: 700; }"
               "#cryptoApiBtn:hover { color: %4; background: %13; }"

               /* ── WS status pill ── */
               "#cryptoWsStatus { font-size: 10px; font-weight: 700; letter-spacing: 0.5px; "
               "  background: transparent; border: 1px solid %3; border-radius: 9px; "
               "  padding: 2px 8px; }"
               "#cryptoWsStatus[ws=\"live\"] { color: %15; border-color: %19; "
               "  background: %17; }"
               "#cryptoWsStatus[ws=\"connecting\"] { color: %12; border-color: %10; "
               "  background: %8; }"
               "#cryptoWsStatus[ws=\"offline\"] { color: %11; border-color: %20; "
               "  background: %18; }"
               "#cryptoWsTransport { color: %9; font-size: 9px; font-weight: 600; "
               "  letter-spacing: 0.5px; background: transparent; border: none; "
               "  margin-left: 4px; }"
               "#cryptoClock { color: %6; font-size: 11px; font-weight: 600; "
               "  background: transparent; border: none; }"

               /* ── Splitter ── */
               "#cryptoMainSplitter::handle { background: %3; width: 1px; }"
               "#cryptoCenterSplitter::handle { background: %3; height: 1px; }"
               "#cryptoRightSplitter::handle { background: %3; height: 1px; }"

               /* ── Watchlist ── */
               "#cryptoWatchlist { background: %2; border-right: 1px solid %3; }"
               "#cryptoWatchlistHeader { background: %8; border-bottom: 1px solid %3; }"
               "#cryptoWatchlistTitle { color: %12; font-weight: 700; font-size: 11px; "
               "  letter-spacing: 0.5px; background: transparent; border: none; }"
               "#cryptoWatchlistCount { color: %6; font-size: 11px; background: transparent; border: none; }"
               "#cryptoWatchlistSearch { background: %1; border: none; border-bottom: 1px solid %3; "
               "  color: %4; padding: 3px 10px; font-size: 12px; }"
               "#cryptoWatchlistSearch:focus { border-bottom-color: %5; }"
               "#cryptoWatchlistTable { background: %1; border: none; color: %4; font-size: 12px; }"
               "#cryptoWatchlistTable::item { padding: 0 4px; border-bottom: 1px solid %3; }"
               "#cryptoWatchlistTable::item:selected { background: %13; }"
               "QHeaderView::section { background: %2; color: %9; font-size: 11px; font-weight: 700; "
               "  border: none; border-bottom: 1px solid %3; padding: 0 4px; }"

               /* ── Chart ── */
               "#cryptoChart { background: %2; border: 1px solid %3; }"
               "#cryptoChartHeader { background: %8; border-bottom: 1px solid %3; }"
               "#cryptoChartTitle { color: %12; font-weight: 700; font-size: 11px; "
               "  letter-spacing: 0.5px; background: transparent; border: none; }"
               "#cryptoTfBtn { background: transparent; color: %6; border: none; "
               "  border-bottom: 2px solid transparent; padding: 2px 8px; "
               "  font-size: 11px; font-weight: 700; }"
               "#cryptoTfBtn:hover { color: %7; }"
               "#cryptoTfBtn[active=\"true\"] { color: %12; border-bottom-color: %12; }"

               /* ── Order book ── */
               "#cryptoOrderBook { background: %2; border: 1px solid %3; }"
               "#cryptoObHeader { background: %8; border-bottom: 1px solid %3; }"
               "#cryptoObTitle { color: %12; font-weight: 700; font-size: 11px; "
               "  letter-spacing: 0.5px; background: transparent; border: none; }"
               "#cryptoObModeBtn { background: transparent; color: %6; border: none; "
               "  border-bottom: 2px solid transparent; padding: 1px 6px; "
               "  font-size: 10px; font-weight: 700; }"
               "#cryptoObModeBtn:hover { color: %7; }"
               "#cryptoObModeBtn[active=\"true\"] { color: %4; border-bottom-color: %12; }"
               "#cryptoObSpread { background: %8; color: %7; font-size: 11px; font-weight: 700; "
               "  border-top: 1px solid %3; border-bottom: 1px solid %3; }"

               /* ── Order entry ── */
               "#cryptoOrderEntry { background: %2; border: 1px solid %3; }"
               "#cryptoOeHeader { background: %8; border-bottom: 1px solid %3; }"
               "#cryptoOeTitle { color: %12; font-weight: 700; font-size: 11px; "
               "  letter-spacing: 0.5px; background: transparent; border: none; }"
               "#cryptoOeMode { font-weight: 700; font-size: 11px; letter-spacing: 0.5px; "
               "  background: transparent; border: none; }"
               "#cryptoOeMode[mode=\"paper\"] { color: %15; }"
               "#cryptoOeMode[mode=\"live\"] { color: %11; }"

               "#cryptoBuyTab { background: transparent; color: %6; border: none; "
               "  border-bottom: 2px solid transparent; padding: 4px 0; "
               "  font-size: 12px; font-weight: 700; }"
               "#cryptoBuyTab:hover { color: %15; }"
               "#cryptoBuyTab[active=\"true\"] { color: %15; border-bottom-color: %15; }"
               "#cryptoSellTab { background: transparent; color: %6; border: none; "
               "  border-bottom: 2px solid transparent; padding: 4px 0; "
               "  font-size: 12px; font-weight: 700; }"
               "#cryptoSellTab:hover { color: %11; }"
               "#cryptoSellTab[active=\"true\"] { color: %11; border-bottom-color: %11; }"

               "#cryptoOeTypeBtn { background: transparent; color: %6; border: none; "
               "  border-bottom: 2px solid transparent; padding: 1px 6px; "
               "  font-size: 10px; font-weight: 700; }"
               "#cryptoOeTypeBtn:hover { color: %7; }"
               "#cryptoOeTypeBtn[active=\"true\"] { color: %4; border-bottom-color: %12; }"

               "#cryptoOeLabel { color: %7; font-size: 11px; font-weight: 700; "
               "  letter-spacing: 0.5px; background: transparent; border: none; }"
               "#cryptoOeInput { background: %1; border: 1px solid %3; color: %4; "
               "  padding: 3px 6px; font-size: 12px; }"
               "#cryptoOeInput:focus { border-color: %5; }"
               "#cryptoOeInput:disabled { color: %9; }"
               "#cryptoOePctBtn { background: %8; color: %7; border: 1px solid %3; "
               "  font-size: 10px; font-weight: 700; padding: 0 4px; }"
               "#cryptoOePctBtn:hover { color: %4; background: %13; }"

               "#cryptoOeBalance { color: %14; font-size: 12px; font-weight: 700; "
               "  background: transparent; border: none; }"
               "#cryptoOeMarketPrice { color: %6; font-size: 11px; background: transparent; border: none; }"
               "#cryptoOeCost { color: %6; font-size: 11px; background: transparent; border: none; }"
               "#cryptoOeStatus { font-size: 11px; background: transparent; border: none; }"
               "#cryptoOeStatus[error=\"true\"] { color: %11; }"

               "#cryptoBuySubmit { background: %17; color: %15; "
               "  border: 1px solid %19; padding: 8px; font-weight: 700; font-size: 13px; }"
               "#cryptoBuySubmit:hover { background: %15; color: %1; }"
               "#cryptoSellSubmit { background: %18; color: %11; "
               "  border: 1px solid %20; padding: 8px; font-weight: 700; font-size: 13px; }"
               "#cryptoSellSubmit:hover { background: %11; color: %4; }"

               "#cryptoAdvToggle { background: transparent; color: %6; border: none; "
               "  font-size: 10px; font-weight: 700; letter-spacing: 0.5px; padding: 2px 0; }"
               "#cryptoAdvToggle:hover { color: %7; }"

               /* ── Bottom panel ── */
               "#cryptoBottomPanel { background: %2; border: 1px solid %3; }"
               "#cryptoBottomTabs::pane { border: none; background: %2; }"
               "#cryptoBottomTabs QTabBar::tab { background: transparent; color: %6; "
               "  padding: 3px 10px; border: none; border-bottom: 2px solid transparent; "
               "  font-size: 11px; font-weight: 700; letter-spacing: 0.5px; }"
               "#cryptoBottomTabs QTabBar::tab:selected { color: %12; border-bottom-color: %12; }"
               "#cryptoBottomTabs QTabBar::tab:hover:!selected { color: %7; }"

               "#cryptoBottomTable { background: %1; border: none; color: %4; font-size: 12px; }"
               "#cryptoBottomTable::item { padding: 0 4px; border-bottom: 1px solid %3; }"
               "#cryptoBottomTable QHeaderView::section { background: %2; color: %9; "
               "  font-size: 11px; font-weight: 700; border: none; border-bottom: 1px solid %3; padding: 0 4px; }"

               "#cryptoCancelBtn { background: %18; color: %11; "
               "  border: 1px solid %20; padding: 1px 6px; font-size: 10px; font-weight: 700; }"
               "#cryptoCancelBtn:hover { background: %11; color: %4; }"

               "#cryptoInfoRow { border-bottom: 1px solid %3; }"
               "#cryptoInfoLabel { color: %7; font-size: 11px; font-weight: 700; "
               "  letter-spacing: 0.5px; background: transparent; border: none; }"
               "#cryptoInfoValue { color: %4; font-size: 12px; font-weight: 700; "
               "  background: transparent; border: none; }"

               "#cryptoStatRow { border-bottom: 1px solid %3; }"
               "#cryptoStatLabel { color: %7; font-size: 11px; font-weight: 700; "
               "  letter-spacing: 0.5px; background: transparent; border: none; }"
               "#cryptoStatValue { color: %4; font-size: 13px; font-weight: 700; "
               "  background: transparent; border: none; }"
               "#cryptoStatValue[pnl=\"positive\"] { color: %15; }"
               "#cryptoStatValue[pnl=\"negative\"] { color: %11; }"

               /* ── Time & Sales ── */
               "#cryptoTimeSales { background: %1; }"

               /* ── Depth Chart ── */
               "#cryptoDepthChart { background: %1; }")
        .arg(colors::BG_BASE())        // %1
        .arg(colors::BG_SURFACE())     // %2
        .arg(colors::BORDER_DIM())     // %3
        .arg(colors::TEXT_PRIMARY())   // %4
        .arg(colors::BORDER_BRIGHT())  // %5
        .arg(colors::TEXT_TERTIARY())  // %6
        .arg(colors::TEXT_SECONDARY()) // %7
        .arg(colors::BG_RAISED())      // %8
        .arg(colors::TEXT_DIM())       // %9
        .arg(colors::AMBER_DIM())      // %10
        .arg(colors::NEGATIVE())       // %11
        .arg(colors::AMBER())          // %12
        .arg(colors::BG_HOVER())       // %13
        .arg(colors::CYAN())           // %14
        .arg(colors::POSITIVE())       // %15
        .arg(colors::ACCENT_BG())      // %16
        .arg(colors::POSITIVE_BG())    // %17
        .arg(colors::NEGATIVE_BG())    // %18
        .arg(colors::POSITIVE_DIM())   // %19
        .arg(colors::NEGATIVE_DIM())   // %20
        ;
}

QString equity_trading_styles() {
    // Reuse same Obsidian color palette as crypto — identical visual language
    return QString(
               /* ── Root ── */
               "#eqScreen { background: %1; font-family: 'Consolas','Courier New',monospace; }"

               /* ── Command bar ── */
               "#eqCommandBar { background: %8; border-bottom: 1px solid %3; }"
               "#eqCommandBarSep { color: %3; font-size: 14px; background: transparent; border: none; }"

               "#eqBrokerBtn { background: %16; color: %12; "
               "  border: 1px solid %10; padding: 2px 10px; font-size: 11px; "
               "  font-weight: 700; letter-spacing: 0.5px; }"
               "#eqBrokerBtn:hover { background: %12; color: %1; }"
               "#eqBrokerBtn::menu-indicator { width: 0; height: 0; }"

               "#eqExchangeLabel { color: %14; font-size: 11px; font-weight: 700; "
               "  letter-spacing: 0.5px; background: transparent; border: none; }"

               "#eqSymbolInput { background: %2; color: %12; border: 1px solid %3; "
               "  padding: 2px 8px; font-size: 13px; font-weight: 700; }"
               "#eqSymbolInput:focus { border-color: %12; }"

               /* ── Price ribbon ── */
               "#eqPriceRibbon { background: transparent; border: none; }"
               "#eqCommandBarTitle { color: %12; font-weight: 700; font-size: 12px; "
               "  letter-spacing: 1px; background: transparent; border: none; }"
               "#eqHeroPrice { color: %4; font-size: 18px; font-weight: 700; "
               "  background: transparent; border: none; }"
               "#eqChange { font-size: 12px; font-weight: 700; background: transparent; border: none; }"
               "#eqBid { color: %15; font-size: 11px; font-weight: 600; background: transparent; border: none; }"
               "#eqAsk { color: %11; font-size: 11px; font-weight: 600; background: transparent; border: none; }"
               "#eqSpreadInline { color: %6; font-size: 11px; background: transparent; border: none; }"
               "#eqStatLabel { color: %6; font-size: 11px; background: transparent; border: none; }"

               "#eqModeBtn { padding: 2px 10px; font-weight: 700; font-size: 11px; letter-spacing: 0.5px; }"
               "#eqModeBtn[mode=\"paper\"] { background: %17; color: %15; border: 1px solid %19; }"
               "#eqModeBtn[mode=\"live\"] { background: %18; color: %11; border: 1px solid %20; }"
               "#eqApiBtn { background: %8; color: %7; border: 1px solid %3; "
               "  padding: 2px 10px; font-size: 11px; font-weight: 700; }"
               "#eqApiBtn:hover { color: %4; background: %13; }"

               "#eqClock { color: %6; font-size: 11px; font-weight: 600; "
               "  background: transparent; border: none; }"

               /* ── Splitters ── */
               "#eqMainSplitter::handle { background: %3; width: 1px; }"
               "#eqCenterSplitter::handle { background: %3; height: 1px; }"
               "#eqRightSplitter::handle { background: %3; height: 1px; }"

               /* ── Watchlist ── */
               "#eqWatchlist { background: %2; border-right: 1px solid %3; }"
               "#eqWatchlistHeader { background: %8; border-bottom: 1px solid %3; }"
               "#eqWatchlistTitle { color: %12; font-weight: 700; font-size: 11px; "
               "  letter-spacing: 0.5px; background: transparent; border: none; }"
               "#eqWatchlistCount { color: %6; font-size: 11px; background: transparent; border: none; }"
               "#eqWatchlistSearch { background: %1; border: none; border-bottom: 1px solid %3; "
               "  color: %4; padding: 3px 10px; font-size: 12px; }"
               "#eqWatchlistSearch:focus { border-bottom-color: %5; }"
               "#eqWatchlistTable { background: %1; border: none; color: %4; font-size: 12px; }"
               "#eqWatchlistTable::item { padding: 0 4px; border-bottom: 1px solid %3; }"
               "#eqWatchlistTable::item:selected { background: %13; }"
               "#eqWatchlistTable QHeaderView::section { background: %2; color: %9; font-size: 11px; "
               "  font-weight: 700; border: none; border-bottom: 1px solid %3; padding: 0 4px; }"

               /* ── Chart ── */
               "#eqChart { background: %2; border: 1px solid %3; }"
               "#eqChartHeader { background: %8; border-bottom: 1px solid %3; }"
               "#eqChartTitle { color: %12; font-weight: 700; font-size: 11px; "
               "  letter-spacing: 0.5px; background: transparent; border: none; }"
               "#eqTfBtn { background: transparent; color: %6; border: none; "
               "  border-bottom: 2px solid transparent; padding: 2px 8px; "
               "  font-size: 11px; font-weight: 700; }"
               "#eqTfBtn:hover { color: %7; }"
               "#eqTfBtn[active=\"true\"] { color: %12; border-bottom-color: %12; }"

               /* ── Order book ── */
               "#eqOrderBook { background: %2; border: 1px solid %3; }"
               "#eqObHeader { background: %8; border-bottom: 1px solid %3; }"
               "#eqObTitle { color: %12; font-weight: 700; font-size: 11px; "
               "  letter-spacing: 0.5px; background: transparent; border: none; }"
               "#eqObSpread { background: %8; color: %7; font-size: 11px; font-weight: 700; "
               "  border-top: 1px solid %3; border-bottom: 1px solid %3; }"

               /* ── Order entry ── */
               "#eqOrderEntry { background: %2; border: 1px solid %3; }"
               "#eqOeHeader { background: %8; border-bottom: 1px solid %3; }"
               "#eqOeTitle { color: %12; font-weight: 700; font-size: 11px; "
               "  letter-spacing: 0.5px; background: transparent; border: none; }"
               "#eqOeMode { font-weight: 700; font-size: 11px; letter-spacing: 0.5px; "
               "  background: transparent; border: none; }"

               "#eqBuyTab { background: transparent; color: %6; border: none; "
               "  border-bottom: 2px solid transparent; padding: 4px 0; "
               "  font-size: 12px; font-weight: 700; }"
               "#eqBuyTab:hover { color: %15; }"
               "#eqBuyTab[active=\"true\"] { color: %15; border-bottom-color: %15; }"
               "#eqSellTab { background: transparent; color: %6; border: none; "
               "  border-bottom: 2px solid transparent; padding: 4px 0; "
               "  font-size: 12px; font-weight: 700; }"
               "#eqSellTab:hover { color: %11; }"
               "#eqSellTab[active=\"true\"] { color: %11; border-bottom-color: %11; }"

               "#eqOeTypeBtn { background: transparent; color: %6; border: none; "
               "  border-bottom: 2px solid transparent; padding: 1px 6px; "
               "  font-size: 10px; font-weight: 700; }"
               "#eqOeTypeBtn:hover { color: %7; }"
               "#eqOeTypeBtn[active=\"true\"] { color: %4; border-bottom-color: %12; }"

               "#eqOeLabel { color: %7; font-size: 11px; font-weight: 700; "
               "  letter-spacing: 0.5px; background: transparent; border: none; }"
               "#eqOeInput { background: %1; border: 1px solid %3; color: %4; "
               "  padding: 3px 6px; font-size: 12px; }"
               "#eqOeInput:focus { border-color: %5; }"
               "#eqOeInput:disabled { color: %9; }"
               "#eqOeCombo { background: %1; border: 1px solid %3; color: %4; "
               "  padding: 3px 6px; font-size: 12px; }"
               "#eqOeCombo:focus { border-color: %5; }"
               "#eqOeCombo QAbstractItemView { background: %2; color: %4; "
               "  selection-background-color: %13; border: 1px solid %3; }"

               "#eqOeBalance { color: %14; font-size: 12px; font-weight: 700; "
               "  background: transparent; border: none; }"
               "#eqOeMarketPrice { color: %6; font-size: 11px; background: transparent; border: none; }"
               "#eqOeCost { color: %6; font-size: 11px; background: transparent; border: none; }"
               "#eqOeStatus { font-size: 11px; background: transparent; border: none; }"

               "#eqBuySubmit { background: %17; color: %15; "
               "  border: 1px solid %19; padding: 8px; font-weight: 700; font-size: 13px; }"
               "#eqBuySubmit:hover { background: %15; color: %1; }"
               "#eqSellSubmit { background: %18; color: %11; "
               "  border: 1px solid %20; padding: 8px; font-weight: 700; font-size: 13px; }"
               "#eqSellSubmit:hover { background: %11; color: %4; }"

               "#eqAdvToggle { background: transparent; color: %6; border: none; "
               "  font-size: 10px; font-weight: 700; letter-spacing: 0.5px; padding: 2px 0; }"
               "#eqAdvToggle:hover { color: %7; }"

               /* ── Bottom panel ── */
               "#eqBottomPanel { background: %2; border: 1px solid %3; }"
               "#eqBottomTabs::pane { border: none; background: %2; }"
               "#eqBottomTabs QTabBar::tab { background: transparent; color: %6; "
               "  padding: 3px 10px; border: none; border-bottom: 2px solid transparent; "
               "  font-size: 11px; font-weight: 700; letter-spacing: 0.5px; }"
               "#eqBottomTabs QTabBar::tab:selected { color: %12; border-bottom-color: %12; }"
               "#eqBottomTabs QTabBar::tab:hover:!selected { color: %7; }"

               "#eqTable { background: %1; border: none; color: %4; font-size: 12px; }"
               "#eqTable::item { padding: 0 4px; border-bottom: 1px solid %3; }"
               "#eqTable QHeaderView::section { background: %2; color: %9; "
               "  font-size: 11px; font-weight: 700; border: none; border-bottom: 1px solid %3; padding: 0 4px; }")
        .arg(colors::BG_BASE())        // %1
        .arg(colors::BG_SURFACE())     // %2
        .arg(colors::BORDER_DIM())     // %3
        .arg(colors::TEXT_PRIMARY())   // %4
        .arg(colors::BORDER_BRIGHT())  // %5
        .arg(colors::TEXT_TERTIARY())  // %6
        .arg(colors::TEXT_SECONDARY()) // %7
        .arg(colors::BG_RAISED())      // %8
        .arg(colors::TEXT_DIM())       // %9
        .arg(colors::AMBER_DIM())      // %10
        .arg(colors::NEGATIVE())       // %11
        .arg(colors::AMBER())          // %12
        .arg(colors::BG_HOVER())       // %13
        .arg(colors::CYAN())           // %14
        .arg(colors::POSITIVE())       // %15
        .arg(colors::ACCENT_BG())      // %16
        .arg(colors::POSITIVE_BG())    // %17
        .arg(colors::NEGATIVE_BG())    // %18
        .arg(colors::POSITIVE_DIM())   // %19
        .arg(colors::NEGATIVE_DIM())   // %20
        ;
}

} // namespace fincept::ui::styles
