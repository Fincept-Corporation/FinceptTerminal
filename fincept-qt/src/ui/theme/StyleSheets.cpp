#include "ui/theme/StyleSheets.h"

#include "ui/theme/Theme.h"

namespace fincept::ui::styles {

QString card_frame() {
    return QString("QFrame { background: %1; border: 1px solid %2; border-radius: 2px; }")
        .arg(colors::BG_SURFACE(), colors::BORDER_MED());
}

QString card_title() {
    return QString("color: %1; font-size: " + fincept::ui::fonts::px(-1) + "px; font-weight: bold; background: transparent;").arg(colors::TEXT_SECONDARY());
}

QString card_close_button() {
    return QString("QPushButton { color: %1; background: transparent; border: none; font-size: " + fincept::ui::fonts::px(-2) + "px; }"
                   "QPushButton:hover { color: %2; }")
        .arg(colors::TEXT_SECONDARY(), colors::TEXT_PRIMARY());
}

QString section_header() {
    return QString("color: %1; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: bold; "
                   "background: transparent; padding: 2px 0; "
                   "border-bottom: 1px solid %2;")
        .arg(colors::TEXT_SECONDARY(), colors::BORDER_MED());
}

QString data_label() {
    return QString("color: %1; font-size: " + fincept::ui::fonts::px(-1) + "px; background: transparent;").arg(colors::TEXT_SECONDARY());
}

QString data_value() {
    return QString("color: %1; font-size: " + fincept::ui::fonts::px(-1) + "px; background: transparent;").arg(colors::TEXT_PRIMARY());
}

QString accent_button() {
    return QString("QPushButton { background: %1; color: %2; border: 1px solid %3; "
                   "padding: 4px 12px; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: bold; }"
                   "QPushButton:hover { background: %4; color: %2; }")
        .arg(colors::BG_SURFACE(), colors::AMBER(), colors::BORDER_MED(), colors::BG_HOVER());
}

QString muted_button() {
    return QString("QPushButton { background: %1; color: %2; border: 1px solid %3; "
                   "padding: 4px 12px; font-size: " + fincept::ui::fonts::px(-2) + "px; }"
                   "QPushButton:hover { background: %4; color: %5; }")
        .arg(colors::BG_SURFACE(), colors::TEXT_SECONDARY(), colors::BORDER_MED(), colors::BG_HOVER(),
             colors::TEXT_PRIMARY());
}

QString fno_dense_table() {
    return QString("QTableView { background:%1; color:%2; border:none; gridline-color:transparent; "
                   "  alternate-background-color:%3; font-size:" + fincept::ui::fonts::px(-3) + "px; "
                   "  selection-background-color:%4; selection-color:%2; }"
                   "QTableView::item { padding:0 6px; border:none; }"
                   "QTableView::item:hover { background:%4; }"
                   "QHeaderView::section { background:%5; color:%6; border:none; "
                   "  border-bottom:1px solid %7; padding:2px 6px; "
                   "  font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; letter-spacing:0.4px; }"
                   "QScrollBar:vertical { background:%1; width:6px; margin:0; }"
                   "QScrollBar::handle:vertical { background:%7; min-height:20px; border-radius:3px; }"
                   "QScrollBar::handle:vertical:hover { background:%6; }"
                   "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }")
        .arg(colors::BG_BASE(),        // %1
             colors::TEXT_PRIMARY(),   // %2
             colors::ROW_ALT(),        // %3
             colors::BG_HOVER(),       // %4
             colors::BG_RAISED(),      // %5
             colors::TEXT_SECONDARY(), // %6
             colors::BORDER_MED());    // %7  header rule + scrollbar handle
}

QString news_screen_styles() {
    return QString(
               /* ── NewsScreen root ── */
               "#newsScreen { background: %1; }"

               /* ── Command bar (two-row header) ── */
               "#newsCommandBar { background: %2; border-bottom: 1px solid %3; }"
               "#newsCommandRow { background: %2; border-bottom: 1px solid %3; }"
               "#newsCommandBarSearch { background: %1; color: %4; border: 1px solid %3; "
               "  padding: 2px 8px; font-size: " + fincept::ui::fonts::px(-2) + "px; }"
               "#newsCommandBarSearch:focus { border-color: %5; }"
               "#newsCommandBarPill { background: transparent; color: %6; border: 1px solid %3; "
               "  padding: 0 5px; font-size: " + fincept::ui::fonts::px(-4) + "px; font-weight: 600; letter-spacing: 0.3px; }"
               "#newsCommandBarPill:hover { color: %7; background: %8; }"
               "#newsCommandBarPill[active=\"true\"] { background: %9; color: %18; border-color: %10; }"
               "#newsCommandBarSep { color: %3; font-size: " + fincept::ui::fonts::px(-2) + "px; background: transparent; }"
               "#newsCommandBarCount { color: %7; font-size: " + fincept::ui::fonts::px(-2) + "px; background: transparent; padding: 0 4px; }"
               "#newsCommandBarAlert { color: %20; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; "
               "  background: transparent; padding: 0 6px; }"
               "#newsCommandBarUnseen { color: %12; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; "
               "  background: transparent; padding: 0 6px; }"
               "#newsCommandBarProgress { color: %6; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent; padding: 0 4px; }"
               "#newsCommandBarRefresh { background: %8; color: %7; border: 1px solid %3; "
               "  padding: 0 10px; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; }"
               "#newsCommandBarRefresh:hover { color: %4; background: %13; }"
               "#newsCommandBarRefresh:disabled { color: %6; }"
               "#newsCommandBarCombo { background: %1; color: %7; border: 1px solid %3; "
               "  padding: 1px 4px; font-size: " + fincept::ui::fonts::px(-3) + "px; }"

               "#newsCommandRowScroll { background: transparent; border: none; }"

               /* ── Drawer toggle button ── */
               "#newsDrawerBtn { background: %21; color: %12; "
               "  border: 1px solid %10; padding: 0 10px; font-size: " + fincept::ui::fonts::px(-3) + "px; "
               "  font-weight: 700; letter-spacing: 0.5px; }"
               "#newsDrawerBtn:hover { background: %22; }"
               "#newsDrawerBtn:checked { background: %12; color: %18; }"

               /* ── Intel strip (row 2 of header) ── */
               "#newsIntelStrip { background: %8; border-bottom: 1px solid %3; }"
               "#newsIntelLabel { color: %7; font-size: " + fincept::ui::fonts::px(-4) + "px; font-weight: 700; "
               "  letter-spacing: 0.5px; background: transparent; }"
               "#newsIntelValue { color: %14; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; background: transparent; }"
               "#newsIntelSep { color: %3; font-size: " + fincept::ui::fonts::px(-4) + "px; background: transparent; }"
               "#newsIntelScore { color: %7; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; background: transparent; }"
               "#newsIntelMonitors { color: %7; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent; }"
               "#newsIntelDeviations { color: %20; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; background: transparent; }"
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
               "#newsBreakingBanner { background: %17; border-bottom: 1px solid %20; }"
               "#newsBreakingTag { color: %18; background: %20; font-size: " + fincept::ui::fonts::px(-4) + "px; font-weight: 700; "
               "  letter-spacing: 1px; padding: 1px 4px; }"
               "#newsBreakingHeadline { color: %4; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700; background: transparent; }"
               "#newsBreakingSource { color: %14; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent; }"
               "#newsBreakingDismiss { color: %7; background: transparent; border: none; font-size: " + fincept::ui::fonts::px(-2) + "px; }"
               "#newsBreakingDismiss:hover { color: %4; }"

               /* ── Intel drawer (left overlay) ── */
               "#newsDrawerPanel { background: %2; border-right: 1px solid %5; }"
               "#newsDrawerHeader { background: %8; border-bottom: 1px solid %3; }"
               "#newsDrawerTitle { color: %12; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700; "
               "  letter-spacing: 1px; background: transparent; }"
               "#newsDrawerCloseBtn { color: %7; background: transparent; border: 1px solid %3; "
               "  font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700; }"
               "#newsDrawerCloseBtn:hover { color: %4; background: %8; }"
               "#newsDrawerScroll { background: transparent; }"
               "#newsDrawerContent { background: transparent; }"
               "#newsDrawerSectionTitle { color: %12; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; "
               "  letter-spacing: 0.5px; background: transparent; padding: 4px 0 2px; "
               "  border-bottom: 1px solid %19; }"

               /* ── Top stories ── */
               "#newsTopStoryBtn { background: transparent; color: %7; font-size: " + fincept::ui::fonts::px(-3) + "px; "
               "  text-align: left; border: none; border-bottom: 1px solid %19; padding: 3px 4px; }"
               "#newsTopStoryBtn:hover { color: %4; background: %8; }"

               /* ── Categories ── */
               "#newsCategoryBtn { background: transparent; color: %7; font-size: " + fincept::ui::fonts::px(-3) + "px; "
               "  text-align: left; border: none; border-bottom: 1px solid %19; padding: 2px 4px; }"
               "#newsCategoryBtn:hover { color: %4; background: %8; }"

               /* ── Monitors ── */
               "#newsMonitorRow { background: transparent; }"
               "#newsMonitorLabel { color: %7; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent; }"
               "#newsMonitorInput { background: %1; color: %4; border: 1px solid %3; "
               "  padding: 2px 6px; font-size: " + fincept::ui::fonts::px(-3) + "px; }"
               "#newsMonitorInput:focus { border-color: %5; }"
               "#newsMonitorAddBtn { background: %8; color: %12; border: 1px solid %3; "
               "  font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700; }"
               "#newsMonitorAddBtn:hover { color: %4; }"
               "#newsMonitorToggleOn { background: transparent; color: %24; border: 1px solid %3; "
               "  font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: 700; }"
               "#newsMonitorToggleOff { background: transparent; color: %7; border: 1px solid %3; "
               "  font-size: " + fincept::ui::fonts::px(-5) + "px; }"
               "#newsMonitorDeleteBtn { background: transparent; color: %7; border: 1px solid %3; font-size: " + fincept::ui::fonts::px(-4) + "px; }"
               "#newsMonitorDeleteBtn:hover { color: %20; }"

               /* ── Deviations ── */
               "#newsDeviationCategory { color: %7; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent; }"
               "#newsDeviationScore { color: %20; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; background: transparent; }"

               /* ── Detail overlay (right panel) ── */
               "#newsDetailOverlay { background: %2; border-left: 1px solid %5; }"
               "#newsDetailHeader { background: %8; border-bottom: 1px solid %3; }"
               "#newsDetailHeaderTitle { color: %12; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700; "
               "  letter-spacing: 1px; background: transparent; }"
               "#newsDetailCloseBtn { color: %7; background: transparent; border: 1px solid %3; "
               "  font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700; }"
               "#newsDetailCloseBtn:hover { color: %4; background: %8; }"
               "#newsDetailContent { background: transparent; }"
               "#newsDetailEmpty { color: %6; font-size: " + fincept::ui::fonts::px(-1) + "px; background: transparent; }"
               "#newsDetailHeadline { color: %4; font-size: " + fincept::ui::fonts::px(1) + "px; font-weight: 700; background: transparent; }"
               "#newsDetailPriorityBadge { font-size: " + fincept::ui::fonts::px(-4) + "px; font-weight: 700; padding: 1px 6px; }"
               "#newsDetailSentimentBadge { font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent; }"
               "#newsDetailTierBadge { color: %12; font-size: " + fincept::ui::fonts::px(-4) + "px; font-weight: 700; background: transparent; }"
               "#newsDetailCategory { color: %7; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent; }"
               "#newsDetailSource { color: %14; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700; background: transparent; }"
               "#newsDetailTime { color: %6; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent; }"
               "#newsDetailSummary { color: %7; font-size: " + fincept::ui::fonts::px(-2) + "px; line-height: 1.4; background: transparent; }"
               "#newsDetailImpact { color: %16; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent; }"
               "#newsDetailTickers { color: %16; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; background: transparent; }"
               "#newsDetailSep { background: %3; }"
               "#newsDetailSectionTitle { color: %12; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; "
               "  letter-spacing: 0.5px; background: transparent; padding: 2px 0; "
               "  border-bottom: 1px solid %19; }"
               "#newsDetailSubTitle { color: %6; font-size: " + fincept::ui::fonts::px(-4) + "px; font-weight: 700; background: transparent; }"
               "#newsDetailOpenBtn, #newsDetailCopyBtn, #newsDetailCopyTitleBtn, #newsDetailSaveBtn { background: %8; "
               "color: %7; "
               "  border: 1px solid %3; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; padding: 0 8px; }"
               "#newsDetailOpenBtn:hover, #newsDetailCopyBtn:hover, #newsDetailCopyTitleBtn:hover, "
               "#newsDetailSaveBtn:hover { color: %4; background: %13; }"
               "#newsDetailAnalyzeBtn { background: %23; color: %12; "
               "  border: 1px solid %10; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; padding: 0 10px; }"
               "#newsDetailAnalyzeBtn:hover { background: %12; color: %18; }"
               "#newsDetailAnalyzeBtn:disabled { color: %6; background: %8; border-color: %3; }"
               "#newsDetailAiFetchNote { color: %16; font-size: " + fincept::ui::fonts::px(-4) + "px; background: %21; "
               "  border-left: 2px solid %16; padding: 3px 6px; }"
               "#newsDetailAiSummary { color: %7; font-size: " + fincept::ui::fonts::px(-2) + "px; line-height: 1.4; background: transparent; }"
               "#newsDetailAiSentiment { font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700; background: transparent; }"
               "#newsDetailAiUrgency { color: %16; font-size: " + fincept::ui::fonts::px(-2) + "px; background: transparent; }"
               "#newsDetailAiConfidence { color: %7; font-size: " + fincept::ui::fonts::px(-2) + "px; background: transparent; }"
               "#newsDetailKeyPoint { color: %7; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent; }"
               "#newsDetailRisk { font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent; }"
               "#newsDetailRiskDetail { color: %6; font-size: " + fincept::ui::fonts::px(-4) + "px; background: transparent; padding-left: 8px; }"
               "#newsDetailEntity { color: %14; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent; }"
               "#newsDetailTopicBadge { color: %14; font-size: " + fincept::ui::fonts::px(-4) + "px; background: %8; "
               "  border: 1px solid %3; padding: 1px 6px; }"
               "#newsRelatedBtn { background: transparent; color: %7; font-size: " + fincept::ui::fonts::px(-3) + "px; "
               "  text-align: left; border: none; border-bottom: 1px solid %19; padding: 3px 4px; }"
               "#newsRelatedBtn:hover { color: %4; background: %8; }"
               "#newsMonitorMatchLabel { color: %7; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent; }"

               /* ── Ticker strip ── */
               "#newsTickerStrip { background: %1; border-top: 1px solid %3; }"

               /* ── Skeleton ── */
               "#newsSkeletonOverlay { background: %1; }"

               /* ── Empty state ── */
               "#newsEmptyState { background: %1; }"
               "#newsEmptyStateTitle { color: %12; font-size: " + fincept::ui::fonts::px(-1) + "px; font-weight: 700; "
               "  letter-spacing: 0.5px; background: transparent; }"
               "#newsEmptyStateHint { color: %6; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent; }")
        .arg(colors::BG_BASE())                // %1
        .arg(colors::BG_SURFACE())             // %2
        .arg(colors::BORDER_MED())             // %3  area / control borders
        .arg(colors::TEXT_PRIMARY())           // %4
        .arg(colors::BORDER_BRIGHT())          // %5
        .arg(colors::TEXT_TERTIARY())          // %6
        .arg(colors::TEXT_SECONDARY())         // %7
        .arg(colors::BG_RAISED())              // %8
        .arg(colors::ORANGE())                 // %9
        .arg(colors::AMBER_DIM())              // %10
        .arg(colors::NEGATIVE())               // %11 bearish sentiment
        .arg(colors::AMBER())                  // %12
        .arg(colors::BG_HOVER())               // %13
        .arg(colors::CYAN())                   // %14
        .arg(colors::POSITIVE())               // %15 bullish sentiment
        .arg(colors::WARNING())                // %16
        .arg(colors::tint(colors::DANGER, 0.12)) // %17 breaking banner
        .arg(colors::TEXT_ON_ACCENT())         // %18 text on accent / danger fills
        .arg(colors::BORDER_DIM())             // %19 row hairlines
        .arg(colors::DANGER())                 // %20 alerts, breaking, delete
        .arg(colors::alpha(colors::AMBER, 0.08)) // %21
        .arg(colors::alpha(colors::AMBER, 0.15)) // %22
        .arg(colors::alpha(colors::AMBER, 0.1))  // %23
        .arg(colors::SUCCESS())                // %24 monitor enabled
        ;
}

QString crypto_trading_styles() {
    return QString(
               /* ── Root ── */
               "#cryptoScreen { background: %1; }"

               /* ── Command bar ── */
               "#cryptoCommandBar { background: %8; border-bottom: 1px solid %3; }"
               "#cryptoCommandBarTitle { color: %12; font-weight: 700; font-size: " + fincept::ui::fonts::px(-2) + "px; "
               "  letter-spacing: 1px; background: transparent; border: none; }"
               "#cryptoCommandBarSep { color: %3; font-size: " + fincept::ui::fonts::px(0) + "px; background: transparent; border: none; }"

               "#cryptoExchangeBtn { background: %16; color: %12; "
               "  border: 1px solid %10; padding: 2px 10px; font-size: " + fincept::ui::fonts::px(-3) + "px; "
               "  font-weight: 700; letter-spacing: 0.5px; }"
               "#cryptoExchangeBtn:hover { background: %12; color: %21; }"
               "#cryptoExchangeBtn::menu-indicator { width: 0; height: 0; }"

               "#cryptoSymbolInput { background: %2; color: %12; border: 1px solid %3; "
               "  padding: 2px 8px; font-size: " + fincept::ui::fonts::px(-1) + "px; font-weight: 700; }"
               "#cryptoSymbolInput:focus { border-color: %12; }"

               /* ── Price ribbon ── */
               "#cryptoPriceRibbon { background: transparent; border: none; }"
               "#cryptoHeroPrice { color: %4; font-size: " + fincept::ui::fonts::px(4) + "px; font-weight: 700; "
               "  background: transparent; border: none; }"
               "#cryptoChange { font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700; background: transparent; border: none; }"
               "#cryptoBid { color: %15; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 600; background: transparent; border: none; }"
               "#cryptoAsk { color: %11; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 600; background: transparent; border: none; }"
               "#cryptoSpreadInline { color: %7; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent; border: none; }"
               "#cryptoStatLabel { color: %7; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent; border: none; }"

               "#cryptoModeBtn { padding: 2px 10px; font-weight: 700; font-size: " + fincept::ui::fonts::px(-3) + "px; letter-spacing: 0.5px; }"
               "#cryptoModeBtn[mode=\"paper\"] { background: %24; color: %23; border: 1px solid "
               "%19; }"
               "#cryptoModeBtn[mode=\"live\"] { background: %26; color: %25; border: 1px solid "
               "%20; }"
               "#cryptoApiBtn { background: %8; color: %7; border: 1px solid %3; "
               "  padding: 2px 10px; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; }"
               "#cryptoApiBtn:hover { color: %4; background: %13; }"

               /* ── WS status pill ── */
               "#cryptoWsStatus { font-size: " + fincept::ui::fonts::px(-4) + "px; font-weight: 700; letter-spacing: 0.5px; "
               "  background: transparent; border: 1px solid %3; border-radius: 9px; "
               "  padding: 2px 8px; }"
               "#cryptoWsStatus[ws=\"live\"] { color: %23; border-color: %19; "
               "  background: %24; }"
               "#cryptoWsStatus[ws=\"connecting\"] { color: %12; border-color: %10; "
               "  background: %8; }"
               "#cryptoWsStatus[ws=\"offline\"] { color: %25; border-color: %20; "
               "  background: %26; }"
               "#cryptoWsTransport { color: %7; font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: 600; "
               "  letter-spacing: 0.5px; background: transparent; border: none; "
               "  margin-left: 4px; }"
               "#cryptoClock { color: %7; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 600; "
               "  background: transparent; border: none; }"

               /* ── Splitter ── */
               "#cryptoMainSplitter::handle { background: %3; width: 1px; }"
               "#cryptoCenterSplitter::handle { background: %3; height: 1px; }"
               "#cryptoRightSplitter::handle { background: %3; height: 1px; }"

               /* ── Watchlist ── */
               "#cryptoWatchlist { background: %2; border-right: 1px solid %3; }"
               "#cryptoWatchlistHeader { background: %8; border-bottom: 1px solid %3; }"
               "#cryptoWatchlistTitle { color: %12; font-weight: 700; font-size: " + fincept::ui::fonts::px(-3) + "px; "
               "  letter-spacing: 0.5px; background: transparent; border: none; }"
               "#cryptoWatchlistCount { color: %7; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent; border: none; }"
               "#cryptoWatchlistSearch { background: %1; border: none; border-bottom: 1px solid %3; "
               "  color: %4; padding: 3px 10px; font-size: " + fincept::ui::fonts::px(-2) + "px; }"
               "#cryptoWatchlistSearch:focus { border-bottom-color: %5; }"
               "#cryptoWatchlistTable { background: %1; border: none; color: %4; font-size: " + fincept::ui::fonts::px(-2) + "px; }"
               "#cryptoWatchlistTable::item { padding: 0 4px; border-bottom: 1px solid %22; }"
               "#cryptoWatchlistTable::item:selected { background: %13; }"
               "QHeaderView::section { background: %2; color: %9; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; "
               "  border: none; border-bottom: 1px solid %3; padding: 0 4px; }"

               /* ── Chart ── */
               "#cryptoChart { background: %2; border: 1px solid %3; }"
               "#cryptoChartHeader { background: %8; border-bottom: 1px solid %3; }"
               "#cryptoChartTitle { color: %12; font-weight: 700; font-size: " + fincept::ui::fonts::px(-3) + "px; "
               "  letter-spacing: 0.8px; background: transparent; border: none; }"
               "#cryptoTfBtn { background: transparent; color: %7; border: none; "
               "  border-bottom: 2px solid transparent; padding: 4px 10px; "
               "  font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; letter-spacing: 0.5px; }"
               "#cryptoTfBtn:hover { color: %4; background: %13; }"
               "#cryptoTfBtn[active=\"true\"] { color: %12; border-bottom-color: %12; "
               "  background: %16; }"
               "#cryptoChartOhlc { background: %27; color: %4; "
               "  border: 1px solid %10; border-radius: 4px; "
               "  padding: 8px 12px; "
               "  min-width: 130px; }"

               /* ── Order book ── */
               "#cryptoOrderBook { background: %2; border: 1px solid %3; }"
               "#cryptoObHeader { background: %8; border-bottom: 1px solid %3; }"
               "#cryptoObTitle { color: %12; font-weight: 700; font-size: " + fincept::ui::fonts::px(-3) + "px; "
               "  letter-spacing: 0.5px; background: transparent; border: none; }"
               "#cryptoObModeBtn { background: transparent; color: %7; border: none; "
               "  border-bottom: 2px solid transparent; padding: 1px 6px; "
               "  font-size: " + fincept::ui::fonts::px(-4) + "px; font-weight: 700; }"
               "#cryptoObModeBtn:hover { color: %4; }"
               "#cryptoObModeBtn[active=\"true\"] { color: %4; border-bottom-color: %12; }"
               "#cryptoObSpread { background: %8; color: %7; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; "
               "  border-top: 1px solid %3; border-bottom: 1px solid %3; }"

               /* ── Order entry ── */
               "#cryptoOrderEntry { background: %2; border: 1px solid %3; }"
               "#cryptoOeHeader { background: %8; border-bottom: 1px solid %3; }"
               "#cryptoOeTitle { color: %12; font-weight: 700; font-size: " + fincept::ui::fonts::px(-3) + "px; "
               "  letter-spacing: 0.8px; background: transparent; border: none; }"
               "#cryptoOeMode { font-weight: 700; font-size: " + fincept::ui::fonts::px(-4) + "px; letter-spacing: 0.8px; "
               "  padding: 2px 8px; border-radius: 8px; }"
               "#cryptoOeMode[mode=\"paper\"] { color: %23; background: %24; border: 1px solid %19; }"
               "#cryptoOeMode[mode=\"live\"]  { color: %25; background: %26; border: 1px solid %20; }"

               /* Account / breakdown card */
               "#cryptoOeCard, #cryptoOeBreakdown { background: %1; border: 1px solid %3; "
               "  border-radius: 4px; }"
               "#cryptoOeKvLabel { color: %9; font-size: " + fincept::ui::fonts::px(-4) + "px; font-weight: 700; "
               "  letter-spacing: 0.7px; background: transparent; border: none; }"
               "#cryptoOeKvValue { color: %4; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700; "
               "  background: transparent; border: none; }"
               "#cryptoOeKvValueAccent { color: %14; font-size: " + fincept::ui::fonts::px(-1) + "px; font-weight: 700; "
               "  background: transparent; border: none; }"
               "#cryptoOeKvValueDim { color: %7; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 600; "
               "  background: transparent; border: none; }"

               /* BUY / SELL side tabs — full pill buttons rather than the old underline. */
               "#cryptoBuyTab { background: %2; color: %7; border: 1px solid %3; "
               "  padding: 8px 0; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700; letter-spacing: 1px; "
               "  border-top-left-radius: 4px; border-bottom-left-radius: 4px; "
               "  border-right: none; }"
               "#cryptoBuyTab:hover { color: %15; background: %17; }"
               "#cryptoBuyTab[active=\"true\"] { color: %21; background: %15; "
               "  border-color: %15; }"
               "#cryptoSellTab { background: %2; color: %7; border: 1px solid %3; "
               "  padding: 8px 0; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700; letter-spacing: 1px; "
               "  border-top-right-radius: 4px; border-bottom-right-radius: 4px; }"
               "#cryptoSellTab:hover { color: %11; background: %18; }"
               "#cryptoSellTab[active=\"true\"] { color: %21; background: %11; "
               "  border-color: %11; }"

               "#cryptoOeTypeBtn { background: %1; color: %7; border: 1px solid %3; "
               "  border-right: none; padding: 6px 0; "
               "  font-size: " + fincept::ui::fonts::px(-4) + "px; font-weight: 700; letter-spacing: 0.5px; }"
               "#cryptoOeTypeBtn:hover { color: %4; background: %13; }"
               "#cryptoOeTypeBtn[active=\"true\"] { color: %12; background: %16; "
               "  border-color: %10; }"

               "#cryptoOeLabel { color: %9; font-size: " + fincept::ui::fonts::px(-4) + "px; font-weight: 700; "
               "  letter-spacing: 0.8px; background: transparent; border: none; "
               "  padding-bottom: 2px; }"
               "#cryptoOeInput { background: %1; border: 1px solid %3; color: %4; "
               "  padding: 7px 10px; font-size: " + fincept::ui::fonts::px(-1) + "px; font-weight: 600; "
               "  selection-background-color: %13; }"
               "#cryptoOeInput:focus { border-color: %12; }"
               "#cryptoOeInput:disabled { color: %9; background: %2; }"
               "#cryptoOeInput[invalid=\"true\"] { border-color: %25; }"
               "#cryptoOeInputWrap { background: transparent; border: none; }"
               "#cryptoOeUnit { background: %2; color: %9; border: 1px solid %3; "
               "  border-left: none; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; "
               "  letter-spacing: 0.5px; padding: 0 6px; }"

               "#cryptoOePctBtn { background: %1; color: %7; border: 1px solid %3; "
               "  font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; padding: 5px 0; }"
               "#cryptoOePctBtn:hover { color: %12; background: %13; border-color: %10; }"
               "#cryptoOePctBtn:pressed { background: %16; color: %12; }"

               "#cryptoOeSpinBox, #cryptoOeCombo { background: %1; border: 1px solid %3; "
               "  color: %4; padding: 6px 10px; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700; "
               "  selection-background-color: %13; }"
               "#cryptoOeSpinBox:focus, #cryptoOeCombo:focus { border-color: %12; }"
               "#cryptoOeCombo::drop-down { border: none; width: 18px; }"
               "#cryptoOeCombo QAbstractItemView { background: %2; color: %4; "
               "  border: 1px solid %3; selection-background-color: %13; }"

               "#cryptoOeStatus { font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 600; "
               "  background: transparent; border: 1px solid %3; "
               "  border-radius: 3px; padding: 6px 8px; }"
               "#cryptoOeStatus[severity=\"error\"]   { color: %25; background: %26; "
               "  border-color: %20; }"
               "#cryptoOeStatus[severity=\"warning\"] { color: %12; background: %16; "
               "  border-color: %10; }"
               "#cryptoOeStatus[severity=\"info\"]    { color: %4; background: %13; }"
               /* Back-compat for old [error=true] callers */
               "#cryptoOeStatus[error=\"true\"] { color: %25; background: %26; border-color: %20; }"

               "#cryptoBuySubmit { background: %15; color: %21; "
               "  border: 1px solid %15; padding: 12px 8px; "
               "  font-weight: 800; font-size: " + fincept::ui::fonts::px(0) + "px; letter-spacing: 1px; "
               "  border-radius: 4px; }"
               "#cryptoBuySubmit:hover { background: %15; color: %21; border-color: %4; }"
               "#cryptoBuySubmit:pressed { background: %15; color: %21; }"
               "#cryptoSellSubmit { background: %11; color: %21; "
               "  border: 1px solid %11; padding: 12px 8px; "
               "  font-weight: 800; font-size: " + fincept::ui::fonts::px(0) + "px; letter-spacing: 1px; "
               "  border-radius: 4px; }"
               "#cryptoSellSubmit:hover { background: %11; color: %21; border-color: %4; }"
               "#cryptoSellSubmit:pressed { background: %11; color: %21; }"

               "#cryptoOeSubmitSubtitle { color: %9; font-size: " + fincept::ui::fonts::px(-4) + "px; font-weight: 600; "
               "  letter-spacing: 0.4px; background: transparent; border: none; "
               "  padding: 2px 4px 0 4px; }"

               "#cryptoAdvToggle { background: transparent; color: %7; border: none; "
               "  font-size: " + fincept::ui::fonts::px(-4) + "px; font-weight: 700; letter-spacing: 0.6px; "
               "  padding: 4px 0; text-align: left; }"
               "#cryptoAdvToggle:hover { color: %12; }"

               /* Legacy aliases — kept so any other code referencing the old
                  IDs continues to render.                                     */
               "#cryptoOeBalance { color: %14; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700; "
               "  background: transparent; border: none; }"
               "#cryptoOeMarketPrice { color: %6; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent; border: none; }"
               "#cryptoOeCost { color: %6; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent; border: none; }"

               /* ── Bottom panel ── */
               "#cryptoBottomPanel { background: %2; border: 1px solid %3; }"
               "#cryptoBottomTabs::pane { border: none; background: %2; }"
               "#cryptoBottomTabs QTabBar { background: %8; border-bottom: 1px solid %3; }"
               "#cryptoBottomTabs QTabBar::tab { background: transparent; color: %7; "
               "  padding: 7px 14px; border: none; border-bottom: 2px solid transparent; "
               "  font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; letter-spacing: 0.6px; "
               "  min-width: 60px; }"
               "#cryptoBottomTabs QTabBar::tab:selected { color: %12; "
               "  border-bottom-color: %12; background: %2; }"
               "#cryptoBottomTabs QTabBar::tab:hover:!selected { color: %4; background: %13; }"
               "#cryptoBottomStack { background: %1; border: none; }"

               "#cryptoBottomTable { background: %1; border: none; color: %4; "
               "  font-size: " + fincept::ui::fonts::px(-2) + "px; gridline-color: transparent; "
               "  selection-background-color: %13; selection-color: %4; }"
               "#cryptoBottomTable::item { padding: 0 8px; border: none; }"
               "#cryptoBottomTable::item:hover { background: %13; }"
               "#cryptoBottomTable::item:selected { background: %13; color: %4; }"
               "#cryptoBottomTable QHeaderView { background: %2; }"
               "#cryptoBottomTable QHeaderView::section { background: %2; color: %9; "
               "  font-size: " + fincept::ui::fonts::px(-4) + "px; font-weight: 700; letter-spacing: 0.5px; "
               "  border: none; border-bottom: 1px solid %3; padding: 6px 8px; }"
               "#cryptoBottomTable QHeaderView::section:first { padding-left: 12px; }"
               "#cryptoBottomTable QScrollBar:vertical { background: %2; width: 8px; margin: 0; }"
               "#cryptoBottomTable QScrollBar::handle:vertical { background: %3; min-height: 24px; "
               "  border-radius: 3px; }"
               "#cryptoBottomTable QScrollBar::handle:vertical:hover { background: %10; }"
               "#cryptoBottomTable QScrollBar::add-line:vertical, "
               "#cryptoBottomTable QScrollBar::sub-line:vertical { height: 0; }"
               "#cryptoBottomTable QScrollBar:horizontal { background: %2; height: 8px; margin: 0; }"
               "#cryptoBottomTable QScrollBar::handle:horizontal { background: %3; min-width: 24px; "
               "  border-radius: 3px; }"
               "#cryptoBottomTable QScrollBar::handle:horizontal:hover { background: %10; }"
               "#cryptoBottomTable QScrollBar::add-line:horizontal, "
               "#cryptoBottomTable QScrollBar::sub-line:horizontal { width: 0; }"

               "#cryptoCancelBtn { background: %26; color: %25; "
               "  border: 1px solid %20; border-radius: 3px; padding: 0; "
               "  min-width: 22px; min-height: 22px; max-width: 22px; max-height: 22px; "
               "  font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700; }"
               "#cryptoCancelBtn:hover { background: %25; color: %21; border-color: %25; }"
               "#cryptoCancelBtn:pressed { background: %20; color: %4; }"

               "#cryptoEmptyHost { background: %1; }"
               "#cryptoEmptyState { color: %9; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 600; "
               "  letter-spacing: 0.4px; background: transparent; border: none; "
               "  padding: 24px; }"

               /* Card-grid layout used by MARKET and STATS tabs. */
               "#cryptoCardHost { background: %1; }"
               "#cryptoCard { background: %2; border: 1px solid %3; border-radius: 4px; }"
               "#cryptoCard:hover { border-color: %10; }"
               "#cryptoCardLabel { color: %9; font-size: " + fincept::ui::fonts::px(-4) + "px; font-weight: 700; "
               "  letter-spacing: 0.8px; background: transparent; border: none; }"
               "#cryptoCardValue { color: %4; font-size: " + fincept::ui::fonts::px(2) + "px; font-weight: 700; "
               "  background: transparent; border: none; }"
               "#cryptoCardValue[pnl=\"positive\"] { color: %15; }"
               "#cryptoCardValue[pnl=\"negative\"] { color: %11; }"
               "#cryptoCardValue[pnl=\"neutral\"]  { color: %4; }"

               /* Legacy aliases — kept so older callers still match. */
               "#cryptoInfoLabel { color: %9; font-size: " + fincept::ui::fonts::px(-4) + "px; font-weight: 700; "
               "  letter-spacing: 0.8px; background: transparent; border: none; }"
               "#cryptoInfoValue { color: %4; font-size: " + fincept::ui::fonts::px(2) + "px; font-weight: 700; "
               "  background: transparent; border: none; }"
               "#cryptoStatValue { color: %4; font-size: " + fincept::ui::fonts::px(2) + "px; font-weight: 700; "
               "  background: transparent; border: none; }"
               "#cryptoStatValue[pnl=\"positive\"] { color: %15; }"
               "#cryptoStatValue[pnl=\"negative\"] { color: %11; }"

               /* ── Time & Sales ── */
               "#cryptoTimeSales { background: %1; }"

               /* ── Depth Chart ── */
               "#cryptoDepthChart { background: %1; }")
        .arg(colors::BG_BASE())                   // %1
        .arg(colors::BG_SURFACE())                // %2
        .arg(colors::BORDER_MED())                // %3  area / control borders
        .arg(colors::TEXT_PRIMARY())              // %4
        .arg(colors::BORDER_BRIGHT())             // %5
        .arg(colors::TEXT_TERTIARY())             // %6
        .arg(colors::TEXT_SECONDARY())            // %7
        .arg(colors::BG_RAISED())                 // %8
        .arg(colors::TEXT_DIM())                  // %9
        .arg(colors::AMBER_DIM())                 // %10
        .arg(colors::NEGATIVE())                  // %11 sell / ask / loss
        .arg(colors::AMBER())                     // %12
        .arg(colors::BG_HOVER())                  // %13
        .arg(colors::CYAN())                      // %14
        .arg(colors::POSITIVE())                  // %15 buy / bid / gain
        .arg(colors::ACCENT_BG())                 // %16
        .arg(colors::POSITIVE_BG())               // %17
        .arg(colors::NEGATIVE_BG())               // %18
        .arg(colors::tint(colors::SUCCESS, 0.45)) // %19 good-state border (paper, live feed)
        .arg(colors::tint(colors::DANGER, 0.45))  // %20 bad-state border (live mode, offline, error)
        .arg(colors::TEXT_ON_ACCENT())            // %21 text on filled buttons
        .arg(colors::BORDER_DIM())                // %22 row hairlines
        .arg(colors::SUCCESS())                   // %23
        .arg(colors::tint(colors::SUCCESS, 0.12)) // %24
        .arg(colors::DANGER())                    // %25
        .arg(colors::tint(colors::DANGER, 0.12))  // %26
        .arg(colors::alpha(colors::BG_SURFACE, 0.92)) // %27 chart OHLC overlay
        ;
}

QString equity_trading_styles() {
    // Same tokens as crypto — identical visual language
    return QString(
               /* ── Root ── */
               "#eqScreen { background: %1; }"

               /* ── Command bar ── */
               "#eqCommandBar { background: %8; border-bottom: 1px solid %3; }"
               "#eqCommandBarSep { color: %3; font-size: " + fincept::ui::fonts::px(0) + "px; background: transparent; border: none; }"

               "#eqBrokerBtn { background: %16; color: %12; "
               "  border: 1px solid %10; padding: 2px 10px; font-size: " + fincept::ui::fonts::px(-3) + "px; "
               "  font-weight: 700; letter-spacing: 0.5px; }"
               "#eqBrokerBtn:hover { background: %12; color: %21; }"
               "#eqBrokerBtn::menu-indicator { width: 0; height: 0; }"

               "#eqExchangeLabel { color: %14; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; "
               "  letter-spacing: 0.5px; background: transparent; border: none; }"

               "#eqSymbolInput { background: %2; color: %12; border: 1px solid %3; "
               "  padding: 2px 8px; font-size: " + fincept::ui::fonts::px(-1) + "px; font-weight: 700; }"
               "#eqSymbolInput:focus { border-color: %12; }"

               /* ── Price ribbon ── */
               "#eqPriceRibbon { background: transparent; border: none; }"
               "#eqCommandBarTitle { color: %12; font-weight: 700; font-size: " + fincept::ui::fonts::px(-2) + "px; "
               "  letter-spacing: 1px; background: transparent; border: none; }"
               "#eqHeroPrice { color: %4; font-size: " + fincept::ui::fonts::px(4) + "px; font-weight: 700; "
               "  background: transparent; border: none; }"
               "#eqChange { font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700; background: transparent; border: none; }"
               "#eqBid { color: %15; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 600; background: transparent; border: none; }"
               "#eqAsk { color: %11; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 600; background: transparent; border: none; }"
               "#eqSpreadInline { color: %7; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent; border: none; }"
               "#eqStatLabel { color: %7; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent; border: none; }"

               "#eqModeBtn { padding: 2px 10px; font-weight: 700; font-size: " + fincept::ui::fonts::px(-3) + "px; letter-spacing: 0.5px; }"
               "#eqModeBtn[mode=\"paper\"] { background: %24; color: %23; border: 1px solid %19; }"
               "#eqModeBtn[mode=\"live\"] { background: %26; color: %25; border: 1px solid %20; }"
               "#eqApiBtn { background: %8; color: %7; border: 1px solid %3; "
               "  padding: 2px 10px; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; }"
               "#eqApiBtn:hover { color: %4; background: %13; }"

               "#eqClock { color: %7; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 600; "
               "  background: transparent; border: none; }"

               /* ── Splitters ── */
               "#eqMainSplitter::handle { background: %3; width: 1px; }"
               "#eqCenterSplitter::handle { background: %3; height: 1px; }"
               "#eqRightSplitter::handle { background: %3; height: 1px; }"

               /* ── Watchlist ── */
               "#eqWatchlist { background: %2; border-right: 1px solid %3; }"
               "#eqWatchlistHeader { background: %8; border-bottom: 1px solid %3; }"
               "#eqWatchlistTitle { color: %12; font-weight: 700; font-size: " + fincept::ui::fonts::px(-3) + "px; "
               "  letter-spacing: 0.5px; background: transparent; border: none; }"
               "#eqWatchlistCount { color: %7; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent; border: none; }"
               "#eqWatchlistSearch { background: %1; border: none; border-bottom: 1px solid %3; "
               "  color: %4; padding: 3px 10px; font-size: " + fincept::ui::fonts::px(-2) + "px; }"
               "#eqWatchlistSearch:focus { border-bottom-color: %5; }"
               "#eqWatchlistTable { background: %1; border: none; color: %4; font-size: " + fincept::ui::fonts::px(-2) + "px; }"
               "#eqWatchlistTable::item { padding: 0 4px; border-bottom: 1px solid %22; }"
               "#eqWatchlistTable::item:selected { background: %13; }"
               "#eqWatchlistTable QHeaderView::section { background: %2; color: %9; font-size: " + fincept::ui::fonts::px(-3) + "px; "
               "  font-weight: 700; border: none; border-bottom: 1px solid %3; padding: 0 4px; }"

               /* ── Chart ── */
               "#eqChart { background: %2; border: 1px solid %3; }"
               "#eqChartHeader { background: %8; border-bottom: 1px solid %3; }"
               "#eqChartTitle { color: %12; font-weight: 700; font-size: " + fincept::ui::fonts::px(-3) + "px; "
               "  letter-spacing: 0.5px; background: transparent; border: none; }"
               "#eqTfBtn { background: transparent; color: %7; border: none; "
               "  border-bottom: 2px solid transparent; padding: 2px 8px; "
               "  font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; }"
               "#eqTfBtn:hover { color: %4; }"
               "#eqTfBtn[active=\"true\"] { color: %12; border-bottom-color: %12; }"

               /* ── Order book ── */
               "#eqOrderBook { background: %2; border: 1px solid %3; }"
               "#eqObHeader { background: %8; border-bottom: 1px solid %3; }"
               "#eqObTitle { color: %12; font-weight: 700; font-size: " + fincept::ui::fonts::px(-3) + "px; "
               "  letter-spacing: 0.5px; background: transparent; border: none; }"
               "#eqObLevels { color: %7; font-size: " + fincept::ui::fonts::px(-4) + "px; font-weight: 600; "
               "  letter-spacing: 0.3px; background: transparent; border: none; }"
               "#eqObCanvas { background: transparent; }"

               /* ── Order entry ── */
               "#eqOrderEntry { background: %2; border: 1px solid %3; }"
               "#eqOeHeader { background: %8; border-bottom: 1px solid %3; }"
               "#eqOeTitle { color: %12; font-weight: 700; font-size: " + fincept::ui::fonts::px(-3) + "px; "
               "  letter-spacing: 0.5px; background: transparent; border: none; }"
               "#eqOeMode { font-weight: 700; font-size: " + fincept::ui::fonts::px(-4) + "px; letter-spacing: 0.5px; "
               "  background: %1; border: 1px solid %3; border-radius: 3px; padding: 2px 8px; }"
               "#eqOeScroll { background: %2; border: none; }"
               "#eqOeContent { background: %2; }"
               "#eqOeSep { background: %3; border: none; }"

               /* BUY / SELL — filled segmented toggle */
               "#eqBuyTab { background: %1; color: %7; border: 1px solid %3; border-right: none; "
               "  border-top-left-radius: 4px; border-bottom-left-radius: 4px; "
               "  font-size: " + fincept::ui::fonts::px(-1) + "px; font-weight: 800; letter-spacing: 1px; }"
               "#eqBuyTab:hover { color: %15; background: %17; }"
               "#eqBuyTab[active=\"true\"] { background: %15; color: %21; border-color: %15; }"
               "#eqSellTab { background: %1; color: %7; border: 1px solid %3; "
               "  border-top-right-radius: 4px; border-bottom-right-radius: 4px; "
               "  font-size: " + fincept::ui::fonts::px(-1) + "px; font-weight: 800; letter-spacing: 1px; }"
               "#eqSellTab:hover { color: %11; background: %18; }"
               "#eqSellTab[active=\"true\"] { background: %11; color: %21; border-color: %11; }"

               /* Order type — filled chips */
               "#eqOeTypeBtn { background: %1; color: %7; border: 1px solid %3; border-radius: 3px; "
               "  padding: 2px 4px; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; }"
               "#eqOeTypeBtn:hover { color: %4; border-color: %5; }"
               "#eqOeTypeBtn[active=\"true\"] { background: %12; color: %21; border-color: %12; }"

               /* Symbol context line */
               "#eqOeSymbol { color: %4; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700; letter-spacing: 0.5px; "
               "  background: transparent; border: none; padding: 1px 0; }"

               /* Live info box — LTP + balance, bright */
               "#eqOeInfo { background: %1; border: 1px solid %3; border-radius: 4px; }"
               "#eqOeMarketPrice { color: %14; font-size: " + fincept::ui::fonts::px(-1) + "px; font-weight: 700; "
               "  background: transparent; border: none; }"
               "#eqOeBalance { color: %4; font-size: " + fincept::ui::fonts::px(-1) + "px; font-weight: 700; "
               "  background: transparent; border: none; }"

               "#eqOeLabel { color: %7; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; "
               "  letter-spacing: 0.5px; background: transparent; border: none; }"
               "#eqOeInput { background: %1; border: 1px solid %3; color: %4; "
               "  border-radius: 3px; padding: 4px 6px; font-size: " + fincept::ui::fonts::px(-1) + "px; }"
               "#eqOeInput:focus { border-color: %5; }"
               "#eqOeInput:disabled { color: %6; background: %2; }"
               "#eqOeCombo { background: %1; border: 1px solid %3; color: %4; "
               "  border-radius: 3px; padding: 4px 6px; font-size: " + fincept::ui::fonts::px(-1) + "px; }"
               "#eqOeCombo:focus { border-color: %5; }"
               "#eqOeCombo::drop-down { border: none; width: 18px; }"
               "#eqOeCombo QAbstractItemView { background: %2; color: %4; "
               "  selection-background-color: %13; border: 1px solid %3; }"

               "#eqOeBrokerage { color: %7; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent; border: none; }"

               /* Cost / margin summary */
               "#eqOeSummary { background: %1; border: 1px solid %3; border-radius: 4px; }"
               "#eqOeCost { color: %4; font-size: " + fincept::ui::fonts::px(-1) + "px; font-weight: 700; "
               "  background: transparent; border: none; }"
               "#eqOeMargin { color: %7; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent; border: none; }"
               "#eqOeStatus { font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 600; background: transparent; border: none; }"

               /* Submit — filled buy/sell */
               "#eqBuySubmit { background: %15; color: %21; "
               "  border: 1px solid %15; border-radius: 4px; padding: 10px; "
               "  font-weight: 800; font-size: " + fincept::ui::fonts::px(-1) + "px; letter-spacing: 0.5px; }"
               "#eqBuySubmit:hover { background: %15; border-color: %4; }"
               "#eqSellSubmit { background: %11; color: %21; "
               "  border: 1px solid %11; border-radius: 4px; padding: 10px; "
               "  font-weight: 800; font-size: " + fincept::ui::fonts::px(-1) + "px; letter-spacing: 0.5px; }"
               "#eqSellSubmit:hover { background: %11; border-color: %4; }"

               "#eqAdvToggle { background: transparent; color: %7; border: none; "
               "  font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; letter-spacing: 0.5px; padding: 3px 0; text-align: left; }"
               "#eqAdvToggle:hover { color: %12; }"

               /* ── Bottom panel ── */
               "#eqBottomPanel { background: %2; border: 1px solid %3; }"
               "#eqBottomTabs::pane { border: none; background: %2; }"
               "#eqBottomTabs QTabBar::tab { background: transparent; color: %7; "
               "  padding: 3px 10px; border: none; border-bottom: 2px solid transparent; "
               "  font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; letter-spacing: 0.5px; }"
               "#eqBottomTabs QTabBar::tab:selected { color: %12; border-bottom-color: %12; }"
               "#eqBottomTabs QTabBar::tab:hover:!selected { color: %4; }"

               "#eqTable { background: %1; border: none; color: %4; font-size: " + fincept::ui::fonts::px(-2) + "px; }"
               "#eqTable::item { padding: 0 4px; border-bottom: 1px solid %22; }"
               "#eqTable QHeaderView::section { background: %2; color: %9; "
               "  font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; border: none; border-bottom: 1px solid %3; padding: 0 4px; }")
        .arg(colors::BG_BASE())                   // %1
        .arg(colors::BG_SURFACE())                // %2
        .arg(colors::BORDER_MED())                // %3  area / control borders
        .arg(colors::TEXT_PRIMARY())              // %4
        .arg(colors::BORDER_BRIGHT())             // %5
        .arg(colors::TEXT_TERTIARY())             // %6
        .arg(colors::TEXT_SECONDARY())            // %7
        .arg(colors::BG_RAISED())                 // %8
        .arg(colors::TEXT_DIM())                  // %9
        .arg(colors::AMBER_DIM())                 // %10
        .arg(colors::NEGATIVE())                  // %11 sell / ask
        .arg(colors::AMBER())                     // %12
        .arg(colors::BG_HOVER())                  // %13
        .arg(colors::CYAN())                      // %14
        .arg(colors::POSITIVE())                  // %15 buy / bid
        .arg(colors::ACCENT_BG())                 // %16
        .arg(colors::POSITIVE_BG())               // %17
        .arg(colors::NEGATIVE_BG())               // %18
        .arg(colors::tint(colors::SUCCESS, 0.45)) // %19 paper-mode border
        .arg(colors::tint(colors::DANGER, 0.45))  // %20 live-mode border
        .arg(colors::TEXT_ON_ACCENT())            // %21 text on filled buttons
        .arg(colors::BORDER_DIM())                // %22 row hairlines
        .arg(colors::SUCCESS())                   // %23
        .arg(colors::tint(colors::SUCCESS, 0.12)) // %24
        .arg(colors::DANGER())                    // %25
        .arg(colors::tint(colors::DANGER, 0.12))  // %26
        ;
}

} // namespace fincept::ui::styles
