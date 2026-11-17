#include "screens/news/NewsScreen.h"

#include "core/events/EventBus.h"
#include "core/instrument/VenueSymbols.h"
#include "core/keys/KeyConfigManager.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "core/symbol/SymbolDragSource.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/news/NewsAnalyticsPanel.h"
#include "screens/news/NewsCommandBar.h"
#include "screens/news/NewsDetailPanel.h"
#include "screens/news/NewsFeedPanel.h"
#include "screens/news/NewsSidePanel.h"
#include "screens/news/NewsTickerStrip.h"
#include "screens/news/dialogs/RssFeedManagerDialog.h"
#include "services/cloud/CloudSyncEngine.h"
#include "services/markets/MarketDataService.h"
#include "services/news/NewsCorrelationService.h"
#include "services/news/NewsFeedParse.h"
#include "services/news/NewsNlpService.h"
#include "services/news/NewsSymbolDirectory.h"
#include "services/notifications/NotificationService.h"
#include "services/portfolio/PortfolioService.h"
#include "storage/repositories/NewsArticleRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/formatting/DateFormat.h"
#include "ui/tables/DataTable.h"
#include "ui/theme/StyleSheets.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QSpinBox>
#include <QSplitter>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
#include <QtConcurrent>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace fincept::screens {

namespace na = services::news_analytics;

/// Everything one filter pass produces (worker thread -> GUI thread).
struct NewsFilterResult {
    int generation = 0;
    QVector<services::NewsArticle> filtered; ///< every filter applied, duplicates included
    QVector<services::NewsArticle> wire;     ///< filtered minus collapsed duplicates (WIRE view)
    QHash<QString, int> duplicate_counts;    ///< wire lead id -> other sources with the same headline
    QVector<services::NewsCluster> clusters;
    QMap<QString, int> category_counts;
    int bullish = 0;
    int bearish = 0;
    int neutral = 0;
    int not_scored = 0;
    double mean_tone = std::numeric_limits<double>::quiet_NaN();
    QVector<NewsTickerMention> trending;
    QVector<NewsTickerMention> holding_mentions;
    NewsAnalyticsSnapshot analytics;
    QStringList top_symbols; ///< most mentioned tickers (market:info subscriptions)
    bool input_empty = true;
    /// A followed (link-group) symbol matched nothing: the list is the whole feed.
    bool symbol_fallback = false;
};

// Desktop notifications are only raised for articles this fresh — otherwise every
// FLASH / BREAKING / monitor hit already sitting in the feed fires at launch.
static constexpr int64_t kNewsNotifyMaxAgeSec = 1800;

// Default view: most stories one source shows per hour of publication (see the
// WIRE pass in apply_filters_async).
static constexpr int kNewsSourceBurstPerHour = 4;

// Tickers shown in the intel drawer's mention lists.
static constexpr int kNewsTrendingMax = 10;
static constexpr int kNewsTrendingMinMentions = 2; // a single mention is noise, not a trend

/// Category pill code (NewsCommandBar) -> article category. The INTEL drawer
/// emits full category names, so the filter accepts either form.
static const QHash<QString, QString>& news_category_codes() {
    static const QHash<QString, QString> codes = {
        {"MKT", "MARKETS"}, {"EARN", "EARNINGS"}, {"ECO", "ECONOMIC"},    {"REG", "REGULATORY"}, {"TECH", "TECH"},
        {"NRG", "ENERGY"},  {"CRPT", "CRYPTO"},   {"GEO", "GEOPOLITICS"}, {"DEF", "DEFENSE"},
    };
    return codes;
}

/// Region filter value -> the feed regions it covers.
static bool news_region_matches(const QString& filter, const QString& region) {
    if (filter.isEmpty() || filter == QLatin1String("ALL"))
        return true;
    QString r = region.toUpper();
    if (r == QLatin1String("ME")) // older user feeds
        r = QStringLiteral("MENA");
    if (filter == QLatin1String("EUROPE"))
        return r == QLatin1String("EU") || r == QLatin1String("UK");
    if (filter == QLatin1String("ASIA-PAC"))
        return r == QLatin1String("ASIA") || r == QLatin1String("CHINA") || r == QLatin1String("JAPAN") ||
               r == QLatin1String("KOREA") || r == QLatin1String("INDIA") || r == QLatin1String("SEA") ||
               r == QLatin1String("ANZ");
    return r == filter;
}

/// The market-data key of a linked / dropped symbol, in the Yahoo form the tagger
/// writes on articles: the publisher's `yahoo` when set, else the shared venue
/// table (RELIANCE on NSE -> RELIANCE.NS, BTC/USDT on BINANCE -> BTC-USD), else
/// the symbol as given (unknown venue). Table lookups only: never blocks.
static QString news_ref_key(const SymbolRef& ref) {
    if (!ref.yahoo.trimmed().isEmpty())
        return ref.yahoo.trimmed().toUpper();
    const QString sym = ref.symbol.trimmed();
    if (!sym.isEmpty() && !ref.exchange.trimmed().isEmpty()) {
        const QString yahoo = core::instrument::to_yahoo(sym, ref.exchange.trimmed());
        if (!yahoo.isEmpty())
            return yahoo.toUpper();
    }
    return sym.toUpper();
}

/// Ticker as headlines name it: data-vendor suffixes stripped ("RELIANCE.NS" ->
/// RELIANCE, "BTC-USD" -> BTC, "^NSEI" -> NSEI).
static QString news_symbol_base(const QString& symbol) {
    QString up = symbol.trimmed().toUpper();
    static const QRegularExpression suffix_re(
        QStringLiteral("^([A-Z0-9&^-]+?)(?:\\.[A-Z]{1,3}|-(?:USD|USDT|USDC|EUR|GBP))$"));
    const auto m = suffix_re.match(up);
    if (m.hasMatch())
        up = m.captured(1);
    up.remove(QLatin1Char('^'));
    return up;
}

/// Whole-word ticker matcher (an optional leading "$" allowed). Tickers of three
/// letters or fewer match case-sensitively, or "V" / "ON" / "IT" would hit almost
/// every article.
static QRegularExpression news_symbol_regex(const QString& base) {
    const QString pattern = QStringLiteral("(?<![\\w$.-])\\$?%1(?![\\w-])").arg(QRegularExpression::escape(base));
    return QRegularExpression(pattern, base.size() <= 3 ? QRegularExpression::NoPatternOption
                                                        : QRegularExpression::CaseInsensitiveOption);
}

/// A set of symbols matched against articles in one go: validated tags first,
/// then a single case-sensitive alternation over the headline / summary for
/// bases of three or more characters (the MY filter can hold hundreds of symbols).
struct NewsSymbolMatcher {
    QSet<QString> keys; // full symbols and bases, upper case
    QRegularExpression re;
    bool has_re = false;

    explicit NewsSymbolMatcher(const QStringList& symbols) {
        QStringList alts;
        for (const QString& s : symbols) {
            const QString full = s.trimmed().toUpper();
            if (full.isEmpty())
                continue;
            const QString base = news_symbol_base(full);
            keys.insert(full);
            keys.insert(base);
            if (base.size() >= 3)
                alts.append(QRegularExpression::escape(base));
        }
        if (!alts.isEmpty()) {
            re = QRegularExpression(QStringLiteral("(?<![\\w$.-])\\$?(?:%1)(?![\\w-])").arg(alts.join(QLatin1Char('|'))));
            has_re = re.isValid();
        }
    }
    bool matches(const services::NewsArticle& a) const {
        for (const auto& t : a.tickers) {
            const QString up = t.toUpper();
            if (keys.contains(up) || keys.contains(news_symbol_base(up)))
                return true;
        }
        return has_re && (re.match(a.headline).hasMatch() || re.match(a.summary).hasMatch());
    }
    bool empty() const { return keys.isEmpty(); }
};

/// RELEVANCE ordering: event priority, impact, source tier and threat level, with a
/// recency decay (half-life ~4 h). Priority-then-time alone kept a day-old FLASH item
/// above every fresher story, and ranked a blog the same as a wire service.
static double news_relevance(const services::NewsArticle& a, int64_t now_s) {
    using services::Impact;
    using services::Priority;
    using services::ThreatLevel;
    double s = 0.0;
    switch (a.priority) {
        case Priority::FLASH:
            s += 4.0;
            break;
        case Priority::URGENT:
            s += 3.0;
            break;
        case Priority::BREAKING:
            s += 2.0;
            break;
        case Priority::ROUTINE:
            break;
    }
    s += a.impact == Impact::HIGH ? 1.5 : (a.impact == Impact::MEDIUM ? 0.75 : 0.0);
    s += std::max(0, 4 - a.tier) * 0.5; // tier 1 wire +1.5 … tier 4 blog +0
    switch (a.threat.level) {
        case ThreatLevel::CRITICAL:
            s += 1.5;
            break;
        case ThreatLevel::HIGH:
            s += 1.0;
            break;
        case ThreatLevel::MEDIUM:
            s += 0.5;
            break;
        default:
            break;
    }
    if (!a.tickers.isEmpty())
        s += 0.25;
    if (a.undated)
        return s; // no publication time: no recency bonus (its time is only when we first saw it)
    const double age_h = static_cast<double>(std::max<int64_t>(0, now_s - a.sort_ts)) / 3600.0;
    s += 3.0 * std::exp(-age_h / 6.0);
    return s;
}
// The notified-id sets only exist to dedupe; cap them so a long session can't
// grow them forever (clearing at worst re-notifies something still in the feed).
static constexpr int kNewsNotifiedSetCap = 20000;

// State rules layered over ui::styles::news_screen_styles(): per-priority stripes,
// signal / odds colours, live badge, credit footer, splitter handles, feed
// health. Widgets select them by object name / dynamic property.
static QString news_screen_local_rules() {
    using services::Priority;
    using services::priority_color;
    return QString("QPushButton#newsTopStoryBtn[prio=\"flash\"] { border-left: 3px solid %1; }"
                   "QPushButton#newsTopStoryBtn[prio=\"urgent\"] { border-left: 3px solid %2; }"
                   "QPushButton#newsTopStoryBtn[prio=\"breaking\"] { border-left: 3px solid %3; }"
                   "QPushButton#newsTopStoryBtn[prio=\"routine\"] { border-left: 3px solid %4; }"
                   "QLabel#newsSignalCritical { color: %12; background: transparent; }"
                   "QLabel#newsPredLow { color: %5; background: transparent; }"
                   "QLabel#newsSignalWarn, QLabel#newsPredMid { color: %6; background: transparent; }"
                   "QLabel#newsPredHigh { color: %7; background: transparent; }"
                   "QLabel#newsPredSource { color: %8; background: transparent; font-size: " + fincept::ui::fonts::px(-5) + "px; }"
                   "QPushButton#newsLiveBadge { color: %8; background: transparent; border: 1px solid %8;"
                   " padding: 0 6px; font-weight: 700; font-size: " + fincept::ui::fonts::px(-4) + "px; }"
                   "QPushButton#newsLiveBadge[live=\"true\"] { color: %13; border-color: %13; }"
                   "QLabel#newsDetailAiCredits { color: %8; font-size: " + fincept::ui::fonts::px(-4) + "px; }"
                   "QLabel#newsDetailAiCredits[low=\"true\"] { color: %6; }"
                   "QPushButton#newsTickerBtn { background: transparent; font-size: " + fincept::ui::fonts::px(-3) + "px; text-align: left;"
                   " border: none; border-bottom: 1px solid %9; padding: 2px 4px; font-family: %10; color: %11; }"
                   "QPushButton#newsTickerBtn:hover { background: %9; }"
                   "QPushButton#newsTickerBtn[tone=\"bull\"] { color: %7; }"
                   "QPushButton#newsTickerBtn[tone=\"bear\"] { color: %5; }"
                   "#newsSentimentUnscored { background: %14; }"
                   "QPushButton#newsFeedHealthBtn { color: %11; background: transparent; border: none; padding: 0 4px;"
                   " font-weight: 700; font-size: " + fincept::ui::fonts::px(-4) + "px; }"
                   "QPushButton#newsFeedHealthBtn[degraded=\"true\"] { color: %6; }"
                   "QPushButton#newsFeedHealthBtn:hover { background: %9; }"
                   "#newsAnalyticsPanel { background: %15; border-bottom: 1px solid %9; }"
                   "QSplitter#newsContentSplitter::handle, QSplitter#newsCenterSplitter::handle { background: %9; }"
                   "QSplitter#newsContentSplitter::handle:hover, QSplitter#newsCenterSplitter::handle:hover { background: %14; }")
        .arg(ui::colors::legible(QColor(priority_color(Priority::FLASH))),
             ui::colors::legible(QColor(priority_color(Priority::URGENT))),
             ui::colors::legible(QColor(priority_color(Priority::BREAKING))),
             ui::colors::legible(QColor(priority_color(Priority::ROUTINE))), ui::colors::NEGATIVE(),
             ui::colors::WARNING(), ui::colors::POSITIVE(), ui::colors::TEXT_TERTIARY(), ui::colors::BORDER_DIM())
        .arg(ui::fonts::DATA_FAMILY, ui::colors::TEXT_SECONDARY(), ui::colors::DANGER(), ui::colors::SUCCESS(),
             ui::colors::BORDER_MED(), ui::colors::BG_BASE());
}

NewsScreen::NewsScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("newsScreen");
    apply_news_styles();
    // The sheet bakes theme colours in; rebuild it when the theme changes.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { apply_news_styles(); });

    // Restore persistent preferences
    QSettings settings;
    settings.beginGroup("news");
    active_category_ = settings.value("category", "ALL").toString();
    time_range_ = settings.value("time_range", "24H").toString();
    // Default order is publication time, newest first. Builds before this one
    // defaulted to RELEVANCE (priority / tier / recency score), which put an
    // older "breaking" or wire story above fresher news — the list did not read
    // in the order things were published. Every install switches to NEWEST once;
    // REL stays one click away.
    sort_mode_ = settings.value("sort_mode", "NEWEST").toString();
    if (!settings.value("sort_newest_v2", false).toBool()) {
        settings.setValue("sort_newest_v2", true);
        if (sort_mode_ != QLatin1String("NEWEST")) {
            sort_mode_ = QStringLiteral("NEWEST");
            settings.setValue("sort_mode", sort_mode_);
        }
        sort_migrated_ = true;
    }
    view_mode_ = settings.value("view_mode", "WIRE").toString();
    // The variant and language filter were written to QSettings on change but
    // never read back, so a restart silently reset them.
    active_variant_ = settings.value("variant", "FULL").toString();
    active_lang_ = settings.value("language", "ALL").toString();
    active_region_ = settings.value("region", "ALL").toString();
    my_filter_ = settings.value("my_filter", "ALL").toString();
    unread_only_ = settings.value("unread_only", false).toBool();
    charts_open_ = settings.value("charts_open", false).toBool();
    follow_link_ = settings.value("follow_link", false).toBool();
    settings.endGroup();

    build_ui();
    connect_signals();

    // Push the persisted preferences into the command bar so the pill
    // highlights (category / time / REL-NEW sort / WIRE-CLST view) reflect the
    // restored state on first paint instead of their hardcoded defaults.
    command_bar_->set_active_category(active_category_);
    command_bar_->set_active_time_range(time_range_);
    command_bar_->set_active_sort(sort_mode_);
    command_bar_->set_active_view(view_mode_);
    command_bar_->set_active_variant(active_variant_);
    command_bar_->set_active_language(active_lang_);
    command_bar_->set_active_region(active_region_);
    command_bar_->set_active_my(my_filter_);
    command_bar_->set_unread_only(unread_only_);
    command_bar_->set_charts_checked(charts_open_);
    feed_panel_->set_view_mode_hint(view_mode_);
    analytics_panel_->setVisible(charts_open_);

    // Drop a symbol anywhere on the News screen to narrow the feed to it (an
    // explicit request: the context chip, removable).
    symbol_dnd::installDropFilter(this, [this](const SymbolRef& ref, SymbolGroup) {
        if (ref.is_valid())
            set_symbol_context(news_ref_key(ref), /*from_link=*/false);
    });

    // A link-group broadcast reaches on_group_symbol_changed() through the dock
    // router in the same SymbolContext emit; mark it so that call is read as the
    // group's (an offer, or followed when FOLLOW LINK is on), not as a direct
    // "show news for X" hand-off. The mark lives for this event-loop pass only.
    connect(&SymbolContext::instance(), &SymbolContext::group_symbol_changed, this,
            [this](SymbolGroup g, const SymbolRef& ref, QObject* source) {
                if (g == SymbolGroup::None || g != link_group_ || source == this)
                    return;
                if (!ref.is_valid()) {
                    // The group's symbol was cleared: nothing to offer or follow.
                    linked_symbol_.clear();
                    if (symbol_from_link_)
                        clear_symbol_context();
                    else
                        update_context_bar();
                    return;
                }
                const QString sym = news_ref_key(ref); // same key as on_group_symbol_changed()
                link_broadcast_marker_ = sym;
                QMetaObject::invokeMethod(
                    this,
                    [this, sym]() {
                        if (link_broadcast_marker_ == sym)
                            link_broadcast_marker_.clear();
                    },
                    Qt::QueuedConnection);
            });

    LOG_DEBUG("NewsScreen", "News screen constructed (no data fetch in constructor)");
}

// The feed's loading page (NewsFeedPanel::build_skeleton): titled progress over
// placeholder rows, visible on the first frame in either theme.
static QString news_loading_rules() {
    return QString("#newsLoadingHead { background: %1; border-bottom: 1px solid %2; }"
                   "#newsLoadingTitle { color: %3; background: transparent; font-weight: 700; letter-spacing: 0.5px;"
                   " font-size: " + fincept::ui::fonts::px(-1) + "px; }"
                   "#newsLoadingHint { color: %4; background: transparent; font-size: " + fincept::ui::fonts::px(-3) +
                   "px; }"
                   "#newsSkeletonRow { background: %5; border-bottom: 1px solid %2; }")
        .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_SECONDARY(),
             ui::colors::TEXT_TERTIARY(), ui::colors::BG_RAISED());
}

// The symbol context bar (NewsFeedPanel::set_context_bar): an accent-tinted strip,
// the symbol as a filled accent chip, FOLLOW LINK as a toggle pill.
static QString news_context_rules() {
    return QString("#newsContextBar { background: %1; border-bottom: 1px solid %2; }"
                   "QLabel#newsContextLead { color: %3; background: transparent; font-weight: 700;"
                   " font-size: " + fincept::ui::fonts::px(-3) + "px; }"
                   "QLabel#newsContextSymbol { color: %4; background: %5; font-weight: 700; padding: 1px 6px;"
                   " font-family: %8; font-size: " + fincept::ui::fonts::px(-2) + "px; }"
                   "QLabel#newsContextNote { color: %6; background: transparent;"
                   " font-size: " + fincept::ui::fonts::px(-3) + "px; }"
                   "QPushButton#newsContextFollow { color: %3; background: transparent; border: 1px solid %7;"
                   " padding: 1px 8px; font-weight: 700; font-size: " + fincept::ui::fonts::px(-4) + "px; }"
                   "QPushButton#newsContextFollow:hover { border-color: %5; }"
                   "QPushButton#newsContextFollow:checked { color: %4; background: %5; border-color: %5; }")
        .arg(ui::colors::tint(QColor(ui::colors::AMBER()), 0.10), ui::colors::BORDER_DIM(),
             ui::colors::TEXT_SECONDARY(), ui::colors::TEXT_ON_ACCENT(), ui::colors::AMBER(),
             ui::colors::TEXT_TERTIARY(), ui::colors::BORDER_MED(), ui::fonts::DATA_FAMILY);
}

void NewsScreen::apply_news_styles() {
    // Only when the text differs: setStyleSheet() re-styles this whole screen even for the
    // sheet already in place, and after a theme or font change the theme manager has
    // already rewritten it (ThemeRestyle.cpp) and styles the screen once itself.
    const QString css = ui::styles::news_screen_styles() + news_screen_local_rules() + news_loading_rules() +
                        news_context_rules();
    if (styleSheet() != css)
        setStyleSheet(css);
}

void NewsScreen::build_ui() {
    // Construction cost per part (one log line) — the screen is built on the GUI thread.
    QElapsedTimer build_clock;
    build_clock.start();
    QStringList build_costs;
    auto cost = [&build_clock, &build_costs](const char* part) {
        build_costs << QStringLiteral("%1 %2").arg(QLatin1String(part)).arg(build_clock.restart());
    };
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    command_bar_ = new NewsCommandBar(this);
    root->addWidget(command_bar_);
    cost("command_bar");

    // Content: [drawer] [analytics over feed] [detail] in a splitter — every
    // pane is resizable (the drawer and the detail panel used to be fixed at
    // 280 / 420 px) and the sizes persist.
    auto* content_widget = new QWidget(this);
    content_widget->setObjectName("newsContentArea");
    auto* content_layout = new QHBoxLayout(content_widget);
    content_layout->setContentsMargins(0, 0, 0, 0);
    content_layout->setSpacing(0);

    content_splitter_ = new QSplitter(Qt::Horizontal, content_widget);
    content_splitter_->setObjectName("newsContentSplitter");
    content_splitter_->setChildrenCollapsible(false);
    content_splitter_->setHandleWidth(3);

    side_panel_ = new NewsSidePanel(content_splitter_);
    cost("drawer");
    center_splitter_ = new QSplitter(Qt::Vertical, content_splitter_);
    center_splitter_->setObjectName("newsCenterSplitter");
    center_splitter_->setChildrenCollapsible(false);
    center_splitter_->setHandleWidth(3);
    analytics_panel_ = new NewsAnalyticsPanel(center_splitter_);
    analytics_panel_->setMinimumHeight(180);
    cost("analytics");
    feed_panel_ = new NewsFeedPanel(center_splitter_);
    cost("feed");
    center_splitter_->addWidget(analytics_panel_);
    center_splitter_->addWidget(feed_panel_);
    center_splitter_->setStretchFactor(0, 2);
    center_splitter_->setStretchFactor(1, 3);
    detail_panel_ = new NewsDetailPanel(content_splitter_);
    cost("detail");
    content_splitter_->addWidget(side_panel_);
    content_splitter_->addWidget(center_splitter_);
    content_splitter_->addWidget(detail_panel_);
    content_splitter_->setStretchFactor(0, 0);
    content_splitter_->setStretchFactor(1, 1);
    content_splitter_->setStretchFactor(2, 0);
    content_layout->addWidget(content_splitter_);

    {
        QSettings s;
        s.beginGroup("news");
        const QByteArray h = s.value("splitter_h").toByteArray();
        const QByteArray v = s.value("splitter_v").toByteArray();
        s.endGroup();
        if (!h.isEmpty())
            content_splitter_->restoreState(h);
        if (!v.isEmpty())
            center_splitter_->restoreState(v);
    }
    auto save_splitters = [this]() {
        QSettings s;
        s.beginGroup("news");
        s.setValue("splitter_h", content_splitter_->saveState());
        s.setValue("splitter_v", center_splitter_->saveState());
        s.endGroup();
    };
    connect(content_splitter_, &QSplitter::splitterMoved, this, save_splitters);
    connect(center_splitter_, &QSplitter::splitterMoved, this, save_splitters);

    root->addWidget(content_widget, 1);
    cost("splitters");

    ticker_strip_ = new NewsTickerStrip(this);
    root->addWidget(ticker_strip_);
    cost("ticker");
    LOG_DEBUG("NewsScreen", QStringLiteral("build_ui ms: %1").arg(build_costs.join(QStringLiteral(", "))));
}

void NewsScreen::connect_signals() {
    // Command bar
    connect(command_bar_, &NewsCommandBar::category_changed, this, &NewsScreen::on_category_changed);
    connect(command_bar_, &NewsCommandBar::time_range_changed, this, &NewsScreen::on_time_range_changed);
    connect(command_bar_, &NewsCommandBar::sort_changed, this, &NewsScreen::on_sort_changed);
    connect(command_bar_, &NewsCommandBar::view_mode_changed, this, &NewsScreen::on_view_mode_changed);
    connect(command_bar_, &NewsCommandBar::search_changed, this, &NewsScreen::on_search_changed);
    connect(command_bar_, &NewsCommandBar::refresh_clicked, this, &NewsScreen::on_refresh);
    connect(command_bar_, &NewsCommandBar::drawer_toggle_requested, this, &NewsScreen::on_drawer_toggle);
    connect(command_bar_, &NewsCommandBar::manage_sources_clicked, this, &NewsScreen::on_manage_sources);

    // Live-feed badge: state mirrored from NewsService, click toggles
    // connect/disconnect. With no live-feed server (NewsService::
    // live_feed_available()) it is hidden — a permanently disabled "OFFLINE"
    // read like a fault, and headlines update from RSS on the refresh interval.
    if (!services::NewsService::live_feed_available()) {
        if (auto* badge = command_bar_->findChild<QPushButton*>(QStringLiteral("newsLiveBadge"))) {
            badge->setEnabled(false);
            badge->setVisible(false);
        }
    }
    connect(&services::NewsService::instance(), &services::NewsService::live_state_changed, this,
            [this](bool connected) {
                if (command_bar_)
                    command_bar_->set_live_state(connected);
            });
    connect(command_bar_, &NewsCommandBar::live_toggle_clicked, this, []() {
        auto& svc = services::NewsService::instance();
        if (svc.is_live_connected()) {
            svc.disconnect_live_feed();
        } else {
            svc.connect_live_feed();
        }
    });

    // Auto-refresh cadence — 0 = manual, otherwise minutes.
    connect(command_bar_, &NewsCommandBar::refresh_interval_changed, this, [this](int minutes) {
        auto& svc = services::NewsService::instance();
        if (minutes <= 0) {
            svc.stop_auto_refresh();
        } else {
            svc.set_refresh_interval(minutes);
            if (visible_)
                svc.start_auto_refresh();
        }
        QSettings s;
        s.beginGroup("news");
        s.setValue("refresh_interval_minutes", minutes);
        s.endGroup();
    });

    // New filters: region, MY (holdings / watchlists), unread only.
    auto persist = [](const char* key, const QVariant& value) {
        QSettings s;
        s.beginGroup("news");
        s.setValue(QLatin1String(key), value);
        s.endGroup();
    };
    connect(command_bar_, &NewsCommandBar::region_filter_changed, this, [this, persist](const QString& region) {
        active_region_ = region.isEmpty() ? QStringLiteral("ALL") : region;
        visible_article_count_ = PAGE_SIZE;
        persist("region", active_region_);
        ScreenStateManager::instance().notify_changed(this);
        apply_filters_async();
    });
    connect(command_bar_, &NewsCommandBar::my_filter_changed, this, [this, persist](const QString& value) {
        my_filter_ = value.isEmpty() ? QStringLiteral("ALL") : value;
        visible_article_count_ = PAGE_SIZE;
        persist("my_filter", my_filter_);
        ScreenStateManager::instance().notify_changed(this);
        apply_filters_async();
    });
    connect(command_bar_, &NewsCommandBar::unread_only_toggled, this, [this, persist](bool on) {
        unread_only_ = on;
        visible_article_count_ = PAGE_SIZE;
        persist("unread_only", on);
        apply_filters_async();
    });
    connect(command_bar_, &NewsCommandBar::mark_all_read_clicked, this, &NewsScreen::mark_all_read);
    connect(command_bar_, &NewsCommandBar::charts_toggled, this, &NewsScreen::set_charts_open);
    connect(command_bar_, &NewsCommandBar::saved_searches_clicked, this, &NewsScreen::show_saved_menu);
    connect(command_bar_, &NewsCommandBar::feed_health_clicked, this, &NewsScreen::show_feed_health);
    connect(&services::NewsService::instance(), &services::NewsService::feed_health_changed, this,
            &NewsScreen::update_feed_health_badge);

    // Watchlists (for the MY filter) come from the symbol directory.
    connect(&services::NewsSymbolDirectory::instance(), &services::NewsSymbolDirectory::dictionary_changed, this,
            [this]() {
                command_bar_->set_watchlists(services::NewsSymbolDirectory::instance().watchlists());
                if (my_filter_ != QLatin1String("ALL") && my_filter_ != QLatin1String("HOLDINGS") && visible_)
                    apply_filters_async();
            });

    // Analytics panel → filters.
    connect(analytics_panel_, &NewsAnalyticsPanel::window_selected, this, [this](qint64 from, qint64 to) {
        window_from_ = from;
        window_to_ = to;
        visible_article_count_ = PAGE_SIZE;
        apply_filters_async();
    });
    connect(analytics_panel_, &NewsAnalyticsPanel::window_cleared, this, [this]() {
        window_from_ = window_to_ = 0;
        visible_article_count_ = PAGE_SIZE;
        apply_filters_async();
    });
    connect(analytics_panel_, &NewsAnalyticsPanel::ticker_clicked, this,
            [this](const QString& symbol) { set_symbol_context(symbol, /*from_link=*/false); });
    connect(analytics_panel_, &NewsAnalyticsPanel::category_clicked, this, &NewsScreen::on_sidebar_category_clicked);
    connect(analytics_panel_, &NewsAnalyticsPanel::region_clicked, this, [this](const QString& region) {
        // A heatmap cell is a source region; the filter accepts it directly.
        active_region_ = region;
        command_bar_->set_active_region(region);
        visible_article_count_ = PAGE_SIZE;
        apply_filters_async();
    });

    // Feed panel
    connect(feed_panel_, &NewsFeedPanel::article_clicked, this, &NewsScreen::on_article_clicked);
    connect(feed_panel_, &NewsFeedPanel::cluster_clicked, this, &NewsScreen::on_cluster_clicked);
    connect(feed_panel_, &NewsFeedPanel::near_bottom, this, &NewsScreen::on_near_bottom);
    connect(feed_panel_, &NewsFeedPanel::clear_filters_requested, this, &NewsScreen::clear_all_filters);
    connect(feed_panel_, &NewsFeedPanel::retry_requested, this, &NewsScreen::on_refresh);
    connect(feed_panel_, &NewsFeedPanel::manage_sources_requested, this, &NewsScreen::on_manage_sources);
    // Symbol context bar / symbol empty state.
    connect(feed_panel_, &NewsFeedPanel::context_show_requested, this, [this]() {
        // The user asked for the offered symbol's news: an explicit filter.
        if (!linked_symbol_.isEmpty())
            set_symbol_context(linked_symbol_, /*from_link=*/false);
    });
    connect(feed_panel_, &NewsFeedPanel::context_clear_requested, this, [this]() {
        // SHOW ALL NEWS on an active filter, or dismiss the offer; the same
        // linked symbol is not offered (or followed) again until it changes.
        if (symbol_context_.isEmpty() || symbol_from_link_)
            link_offer_dismissed_ = true;
        if (symbol_context_.isEmpty())
            update_context_bar();
        else
            clear_symbol_context();
    });
    connect(feed_panel_, &NewsFeedPanel::follow_link_toggled, this, &NewsScreen::set_follow_link);
    connect(feed_panel_, &NewsFeedPanel::symbol_text_search_requested, this, [this](const QString& symbol) {
        // Search the headlines for the bare ticker ("RELIANCE.NS" -> "RELIANCE") as typed text.
        const QString text = news_symbol_base(symbol);
        if (text.isEmpty())
            return;
        command_bar_->set_search_text(text);
        on_search_changed(text);
    });

    // Side panel (drawer)
    connect(side_panel_, &NewsSidePanel::category_clicked, this, &NewsScreen::on_sidebar_category_clicked);
    connect(side_panel_, &NewsSidePanel::article_clicked, this, &NewsScreen::on_sidebar_article_clicked);
    connect(side_panel_, &NewsSidePanel::monitor_added, this, &NewsScreen::on_monitor_added);
    connect(side_panel_, &NewsSidePanel::monitor_toggled, this, &NewsScreen::on_monitor_toggled);
    connect(side_panel_, &NewsSidePanel::monitor_deleted, this, &NewsScreen::on_monitor_deleted);
    connect(side_panel_, &NewsSidePanel::close_requested, this, [this]() { set_drawer_open(false); });
    // Trending ticker / holding row → filter the feed by that symbol (ticker-aware).
    connect(side_panel_, &NewsSidePanel::ticker_clicked, this,
            [this](const QString& symbol) { set_symbol_context(symbol, /*from_link=*/false); });

    // Holdings changed in the Portfolio tab → re-read them for the drawer's
    // "holdings in the news" list (only while this screen is on display).
    auto& psvc = services::PortfolioService::instance();
    auto on_holdings_changed = [this](const QString&) {
        if (visible_)
            refresh_holdings();
    };
    connect(&psvc, &services::PortfolioService::asset_added, this, on_holdings_changed);
    connect(&psvc, &services::PortfolioService::asset_sold, this, on_holdings_changed);
    connect(&psvc, &services::PortfolioService::ledger_changed, this, on_holdings_changed);
    connect(&psvc, &services::PortfolioService::portfolio_deleted, this, on_holdings_changed);

    // Detail panel (overlay)
    connect(detail_panel_, &NewsDetailPanel::analyze_requested, this, &NewsScreen::on_analyze_requested);
    connect(detail_panel_, &NewsDetailPanel::panel_closed, this, &NewsScreen::on_detail_closed);
    connect(detail_panel_, &NewsDetailPanel::bookmark_requested, this, [this](const services::NewsArticle& article) {
        // SQLite on a worker (P1). The article is upserted first so the toggle
        // can't fail with "Article not found" for a row that was never stored
        // (live pushes, or anything the 30-day prune already removed). On any
        // failure the button is re-synced from the DB instead of staying toggled.
        QPointer<NewsScreen> self = this;
        (void)QtConcurrent::run([self, article]() {
            auto& repo = fincept::NewsArticleRepository::instance();
            (void)repo.upsert_batch({article});
            auto r = repo.toggle_saved(article.id);
            QVector<services::NewsArticle> saved;
            if (r.is_ok()) {
                if (auto saved_r = repo.load_saved(); saved_r.is_ok())
                    saved = saved_r.value();
            }
            const bool ok = r.is_ok();
            const QString err = ok ? QString() : QString::fromStdString(r.error());
            if (!self)
                return;
            QMetaObject::invokeMethod(
                self,
                [self, ok, err, saved = std::move(saved)]() {
                    if (!self)
                        return;
                    if (ok)
                        self->side_panel_->update_saved(saved);
                    else
                        LOG_WARN("NewsScreen", "Bookmark toggle failed: " + err);
                    self->detail_panel_->refresh_bookmark_state();
                },
                Qt::QueuedConnection);
        });
    });

    // RTL toggle
    connect(command_bar_, &NewsCommandBar::rtl_toggled, this, []() { ui::set_rtl(!ui::is_rtl()); });

    // Variant selector
    connect(command_bar_, &NewsCommandBar::variant_changed, this, [this](const QString& variant) {
        active_variant_ = variant;
        QSettings s;
        s.beginGroup("news");
        s.setValue("variant", variant);
        s.endGroup();
        on_refresh();
    });

    // Language filter (now real: articles carry a detected language).
    connect(command_bar_, &NewsCommandBar::language_filter_changed, this, [this](const QString& lang) {
        active_lang_ = lang.isEmpty() ? QStringLiteral("ALL") : lang;
        visible_article_count_ = PAGE_SIZE;
        QSettings s;
        s.beginGroup("news");
        s.setValue("language", active_lang_);
        s.endGroup();
        ScreenStateManager::instance().notify_changed(this);
        apply_filters_async();
    });

    // Feed context menu → "Filter feed by $TICKER": the context chip.
    connect(feed_panel_, &NewsFeedPanel::ticker_filter_requested, this,
            [this](const QString& ticker) { set_symbol_context(ticker, /*from_link=*/false); });

    // Pulse animation timer (500ms cycle for new item glow)
    pulse_timer_ = new QTimer(this);
    pulse_timer_->setInterval(500);
    connect(pulse_timer_, &QTimer::timeout, this, [this]() {
        if (visible_)
            feed_panel_->model()->advance_pulse();
    });

    // Restore persisted auto-refresh cadence (default 10 min).
    {
        QSettings s;
        s.beginGroup("news");
        const int minutes = s.value("refresh_interval_minutes", 10).toInt();
        s.endGroup();
        command_bar_->set_refresh_interval_minutes(minutes);
        if (minutes > 0)
            services::NewsService::instance().set_refresh_interval(minutes);
    }

    // Enrichment debounce — coalesce rapid filter changes (search typing,
    // category clicks) into a single round of NER + geo + correlation +
    // prediction calls.
    enrichment_timer_ = new QTimer(this);
    enrichment_timer_->setInterval(350);
    enrichment_timer_->setSingleShot(true);
    connect(enrichment_timer_, &QTimer::timeout, this, &NewsScreen::run_enrichment_now);

    // Debounced DB seen-write timer
    seen_flush_timer_ = new QTimer(this);
    seen_flush_timer_->setInterval(1000);
    seen_flush_timer_->setSingleShot(true);
    connect(seen_flush_timer_, &QTimer::timeout, this, [this]() {
        if (pending_seen_ids_.isEmpty())
            return;
        const QSet<QString> ids = std::move(pending_seen_ids_);
        pending_seen_ids_.clear();
        const int flushed = static_cast<int>(ids.size());
        QPointer<NewsScreen> self = this;
        (void)QtConcurrent::run([ids, self, flushed]() {
            for (const auto& id : ids)
                fincept::NewsArticleRepository::instance().mark_seen(id);
            if (self) {
                QMetaObject::invokeMethod(
                    self,
                    [self, flushed]() {
                        if (self)
                            LOG_DEBUG("NewsScreen", QString("Flushed %1 seen IDs to DB").arg(flushed));
                    },
                    Qt::QueuedConnection);
            }
        });
    });

    // Scroll-based seen tracking. Only the rows between the first and last one
    // inside the viewport are looked at (indexAt on the viewport edges).
    connect(feed_panel_->list_view()->verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        auto* lv = feed_panel_->list_view();
        auto* model = feed_panel_->model();
        const int row_count = model->rowCount();
        if (row_count == 0)
            return;
        const QModelIndex first = lv->indexAt(QPoint(4, 2));
        if (!first.isValid())
            return;
        const QModelIndex last = lv->indexAt(QPoint(4, lv->viewport()->height() - 2));
        const int first_row = first.row();
        const int last_row = last.isValid() ? last.row() : row_count - 1;
        for (int i = first_row; i <= last_row && i < row_count; ++i) {
            const QString id = model->article_at(i).id;
            if (id.isEmpty())
                continue;
            model->mark_seen(id);
            pending_seen_ids_.insert(id);
        }
        if (!pending_seen_ids_.isEmpty())
            seen_flush_timer_->start();
        command_bar_->set_unseen_count(model->unseen_count());
    });

    // Summarize button
    connect(command_bar_, &NewsCommandBar::summarize_clicked, this, [this]() {
        if (filtered_articles_.isEmpty())
            return;
        command_bar_->set_summarizing(true);
        // Offline brief of the current filter's top stories (NewsService picks
        // them locally — no network, no credits), shown as clickable rows.
        const auto items = services::NewsService::brief_articles(filtered_articles_, 8);
        command_bar_->set_summarizing(false);
        if (items.isEmpty())
            command_bar_->show_summary(tr("No headlines to brief for the current filter."));
        else
            command_bar_->show_brief(items);
    });
    connect(command_bar_, &NewsCommandBar::brief_article_clicked, this, &NewsScreen::on_article_clicked);
    connect(detail_panel_, &NewsDetailPanel::related_article_clicked, this, &NewsScreen::on_related_clicked);
    connect(detail_panel_, &NewsDetailPanel::ticker_filter_requested, this,
            [this](const QString& ticker) { set_symbol_context(ticker, /*from_link=*/false); });

    // Ticker strip
    connect(ticker_strip_, &NewsTickerStrip::article_clicked, this, &NewsScreen::on_article_clicked);

    // Service auto-refresh
    connect(&services::NewsService::instance(), &services::NewsService::articles_updated, this,
            [this](QVector<services::NewsArticle> articles) {
                if (!visible_ || loading_ || articles.isEmpty())
                    return; // a refresh of ours is running: its own callback delivers the list
                if (articles.size() == all_articles_.size() && articles.first().id == all_articles_.first().id &&
                    articles.last().id == all_articles_.last().id)
                    return; // the list our own refresh callback just applied
                fold_into_stored();
                all_articles_ = std::move(articles);
                apply_filters_async();
            });

    // Feeds arriving during a fetch re-filter at most every kFilterCoalesceMs:
    // one filter pass (analytics, clustering) per feed used to queue dozens of
    // passes on the shared thread pool, ahead of the feed parsing itself.
    filter_coalesce_timer_ = new QTimer(this);
    filter_coalesce_timer_->setSingleShot(true);
    filter_coalesce_timer_->setInterval(kFilterCoalesceMs);
    connect(filter_coalesce_timer_, &QTimer::timeout, this, &NewsScreen::apply_filters_async);

    // Keyboard shortcuts via KeyConfigManager
    auto& km = KeyConfigManager::instance();

    auto* act_next = km.action(KeyAction::NewsNext);
    act_next->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(act_next);
    connect(act_next, &QAction::triggered, this, [this]() { feed_panel_->select_next(); });

    auto* act_prev = km.action(KeyAction::NewsPrev);
    act_prev->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(act_prev);
    connect(act_prev, &QAction::triggered, this, [this]() { feed_panel_->select_previous(); });

    auto* act_open = km.action(KeyAction::NewsOpen);
    act_open->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(act_open);
    connect(act_open, &QAction::triggered, this, [this]() {
        auto idx = feed_panel_->list_view()->currentIndex();
        if (idx.isValid()) {
            auto article = feed_panel_->model()->article_at(idx.row());
            // Feed links are untrusted — web URLs only.
            const QUrl url(article.link);
            if (url.isValid() && (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https")))
                QDesktopServices::openUrl(url);
        }
    });

    auto* act_close = km.action(KeyAction::NewsClose);
    act_close->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(act_close);
    connect(act_close, &QAction::triggered, this, [this]() {
        if (detail_panel_->is_panel_open())
            detail_panel_->close_panel();
        else if (side_panel_->is_drawer_open())
            set_drawer_open(false);
    });
}

// ── Lifecycle ───────────────────────────────────────────────────────────────

void NewsScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    visible_ = true;
    ticker_strip_->resume();
    pulse_timer_->start();
    // Rate-gated pull of cloud news monitors + feeds on screen entry (no-op when sync is off).
    fincept::services::cloud::CloudSyncEngine::instance().request_pull(QStringLiteral("news_monitor"));
    fincept::services::cloud::CloudSyncEngine::instance().request_pull(QStringLiteral("news_feed"));
    {
        QSettings s;
        s.beginGroup("news");
        const int minutes = s.value("refresh_interval_minutes", 10).toInt();
        s.endGroup();
        if (minutes > 0)
            services::NewsService::instance().start_auto_refresh();
    }
    if (services::NewsService::live_feed_available())
        services::NewsService::instance().connect_live_feed();
    subscribe_mcp_events();
    refresh_holdings();
    services::NewsSymbolDirectory::instance().request_reload();
    update_feed_health_badge();

    if (!first_show_) {
        // Returning to the screen: articles_updated is ignored while hidden, so
        // the list could be hours old with the next auto-refresh a full interval
        // away. force=false serves the 10-minute cache instantly when it is
        // still fresh and only hits the network when it has expired.
        refresh_data(/*force=*/false);
    }

    if (first_show_) {
        first_show_ = false;
        load_saved_searches();
        refresh_volume_baseline(/*force=*/true);

        // Seen / saved columns + restore run on a worker (Database hands out a
        // per-thread SQLite connection); results applied back here (§P1/§P8).
        const int64_t since_ts = QDateTime::currentSecsSinceEpoch() - (30LL * 86400);
        QPointer<NewsScreen> self = this;
        (void)QtConcurrent::run([self, since_ts]() {
            auto& repo = fincept::NewsArticleRepository::instance();
            repo.ensure_seen_column();
            repo.ensure_saved_column();

            QSet<QString> seen_ids;
            if (auto seen_result = repo.load_seen_ids(since_ts); seen_result.is_ok())
                seen_ids = seen_result.value();

            QVector<services::NewsArticle> saved;
            if (auto saved_result = repo.load_saved(); saved_result.is_ok())
                saved = saved_result.value();

            if (!self)
                return;
            QMetaObject::invokeMethod(
                self,
                [self, seen_ids = std::move(seen_ids), saved = std::move(saved)]() {
                    if (!self)
                        return;
                    for (const auto& id : seen_ids)
                        self->feed_panel_->model()->mark_seen(id);
                    self->side_panel_->update_saved(saved);
                    self->command_bar_->set_unseen_count(self->feed_panel_->model()->unseen_count());
                    LOG_DEBUG("NewsScreen", QString("Restored %1 seen IDs and %2 bookmarks from DB")
                                               .arg(seen_ids.size())
                                               .arg(saved.size()));
                },
                Qt::QueuedConnection);
        });

        // What the database already holds (earlier sessions' fetches) is shown at
        // once — read on a worker — while the feeds are fetched. It used to wait
        // for the network: a black list until the first fresh feed answered.
        services::NewsService::instance().load_recent_from_db(
            QDateTime::currentSecsSinceEpoch() - kStoredWindowSec, 4000,
            [self](bool ok, QVector<services::NewsArticle> rows) {
                if (!self || !ok || rows.isEmpty())
                    return;
                self->stored_articles_ =
                    services::news_feed::merge_sorted_unique(self->stored_articles_, rows);
                LOG_DEBUG("NewsScreen", QString("Showing %1 stored articles while feeds load").arg(rows.size()));
                self->apply_filters_async();
            });

        // Independent of the restore above — kick the feed fetch immediately so
        // the screen starts filling while the worker runs.
        on_refresh();
    }
}

void NewsScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    visible_ = false;
    ticker_strip_->pause();
    pulse_timer_->stop();
    enrichment_timer_->stop(); // no point spawning Python for a hidden screen
    services::NewsService::instance().stop_auto_refresh();
    services::NewsService::instance().disconnect_live_feed();
    filter_generation_.fetch_add(1, std::memory_order_relaxed);
    unsubscribe_mcp_events();
    datahub::DataHub::instance().unsubscribe(this); // market:info subscriptions
    info_subscribed_.clear();
}

// ── MCP-driven UI sync ──────────────────────────────────────────────────────
// MCP tools (called from AI Chat / Finagent) publish events when the LLM
// mutates news state. We subscribe while visible so the UI reflects the
// change without the user having to manually refresh.

void NewsScreen::subscribe_mcp_events() {
    if (!mcp_event_subs_.isEmpty())
        return; // idempotent

    QPointer<NewsScreen> self = this;
    auto on_monitors_changed = [self](const QVariantMap&) {
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self]() {
                if (!self || !self->visible_)
                    return;
                self->update_monitors();
            },
            Qt::QueuedConnection);
    };
    auto on_refresh_requested = [self](const QVariantMap&) {
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self]() {
                if (!self || !self->visible_)
                    return;
                self->refresh_data(/*force=*/true);
            },
            Qt::QueuedConnection);
    };

    auto& bus = EventBus::instance();
    mcp_event_subs_.append(bus.subscribe(this, "news.monitor_added", on_monitors_changed));
    mcp_event_subs_.append(bus.subscribe(this, "news.monitor_toggled", on_monitors_changed));
    mcp_event_subs_.append(bus.subscribe(this, "news.monitor_deleted", on_monitors_changed));
    mcp_event_subs_.append(bus.subscribe(this, "news.refresh_requested", on_refresh_requested));
}

void NewsScreen::unsubscribe_mcp_events() {
    auto& bus = EventBus::instance();
    for (auto id : mcp_event_subs_)
        bus.unsubscribe(id);
    mcp_event_subs_.clear();
}

// ── Slots ───────────────────────────────────────────────────────────────────

void NewsScreen::on_category_changed(const QString& category) {
    active_category_ = category;
    visible_article_count_ = PAGE_SIZE;
    QSettings s;
    s.beginGroup("news");
    s.setValue("category", category);
    s.endGroup();
    ScreenStateManager::instance().notify_changed(this);
    apply_filters_async();
}

void NewsScreen::on_time_range_changed(const QString& range) {
    time_range_ = range;
    visible_article_count_ = PAGE_SIZE;
    // A window picked on the timeline belongs to the previous range.
    window_from_ = window_to_ = 0;
    analytics_panel_->set_window(0, 0);
    QSettings s;
    s.beginGroup("news");
    s.setValue("time_range", range);
    s.endGroup();
    ScreenStateManager::instance().notify_changed(this);
    apply_filters_async();
}

void NewsScreen::on_sort_changed(const QString& sort) {
    sort_mode_ = sort;
    QSettings s;
    s.beginGroup("news");
    s.setValue("sort_mode", sort);
    s.endGroup();
    ScreenStateManager::instance().notify_changed(this);
    apply_filters_async();
}

void NewsScreen::on_view_mode_changed(const QString& mode) {
    view_mode_ = mode;
    QSettings s;
    s.beginGroup("news");
    s.setValue("view_mode", mode);
    s.endGroup();
    ScreenStateManager::instance().notify_changed(this);
    feed_panel_->set_view_mode_hint(mode); // uniform row heights only for WIRE
    feed_panel_->model()->set_view_mode(mode);
    command_bar_->set_unseen_count(feed_panel_->model()->unseen_count());
}

void NewsScreen::on_search_changed(const QString& query) {
    search_query_ = query; // typed by the user
    // A typed search always wins over a symbol filter (explicit or followed).
    if (!query.trimmed().isEmpty() && !symbol_context_.isEmpty()) {
        symbol_context_.clear();
        symbol_from_link_ = false;
        symbol_fallback_ = false;
        update_context_bar();
    }
    visible_article_count_ = PAGE_SIZE;
    ScreenStateManager::instance().notify_changed(this);
    apply_filters_async();
}

void NewsScreen::set_symbol_context(const QString& symbol, bool from_link) {
    const QString sym = symbol.trimmed().toUpper();
    if (sym.isEmpty()) {
        clear_symbol_context();
        return;
    }
    if (!from_link && !search_query_.isEmpty()) {
        // An explicit symbol replaces the typed search (one narrowing at a time).
        search_query_.clear();
        command_bar_->set_search_text(QString());
    }
    if (sym == symbol_context_ && from_link == symbol_from_link_) {
        update_context_bar();
        return;
    }
    symbol_context_ = sym;
    symbol_from_link_ = from_link;
    symbol_fallback_ = false;
    visible_article_count_ = PAGE_SIZE;
    update_context_bar();
    ScreenStateManager::instance().notify_changed(this);
    apply_filters_async();
}

void NewsScreen::clear_symbol_context() {
    if (symbol_context_.isEmpty()) {
        update_context_bar();
        return;
    }
    symbol_context_.clear();
    symbol_from_link_ = false;
    symbol_fallback_ = false;
    visible_article_count_ = PAGE_SIZE;
    update_context_bar();
    ScreenStateManager::instance().notify_changed(this);
    apply_filters_async();
}

void NewsScreen::on_link_symbol(const QString& symbol) {
    if (symbol != linked_symbol_)
        link_offer_dismissed_ = false;
    linked_symbol_ = symbol;
    // Followed only on the user's opt-in, never over a typed search or a symbol
    // the user picked here; otherwise the context bar just offers it.
    if (follow_link_ && !link_offer_dismissed_ && search_query_.trimmed().isEmpty() &&
        (symbol_context_.isEmpty() || symbol_from_link_))
        set_symbol_context(symbol, /*from_link=*/true);
    else
        update_context_bar();
}

void NewsScreen::set_follow_link(bool on) {
    follow_link_ = on;
    QSettings s;
    s.beginGroup("news");
    s.setValue("follow_link", on);
    s.endGroup();
    if (on) {
        link_offer_dismissed_ = false;
        if (!linked_symbol_.isEmpty() && symbol_context_.isEmpty() && search_query_.trimmed().isEmpty())
            set_symbol_context(linked_symbol_, /*from_link=*/true);
        else
            update_context_bar();
    } else if (symbol_from_link_) {
        clear_symbol_context(); // stop following: the whole feed again, the symbol offered
    } else {
        update_context_bar();
    }
}

void NewsScreen::update_context_bar() {
    if (!feed_panel_)
        return;
    NewsFeedPanel::ContextBar bar;
    if (link_group_ != SymbolGroup::None)
        bar.group = symbol_group_letter(link_group_);
    bar.follow = follow_link_;
    if (!symbol_context_.isEmpty()) {
        bar.mode = NewsFeedPanel::ContextBar::Mode::Active;
        bar.symbol = symbol_context_;
        bar.from_link = symbol_from_link_;
        bar.fallback = symbol_from_link_ && symbol_fallback_;
    } else if (!linked_symbol_.isEmpty() && link_group_ != SymbolGroup::None && !link_offer_dismissed_) {
        bar.mode = NewsFeedPanel::ContextBar::Mode::Offer;
        bar.symbol = linked_symbol_;
    }
    feed_panel_->set_context_bar(bar);
}

void NewsScreen::set_group(SymbolGroup g) {
    if (g == link_group_)
        return;
    link_group_ = g;
    // The old group's symbol no longer applies. (When the user links the screen
    // from its tab badge, the router then hands over the group's symbol
    // directly — an explicit choice made here, so it narrows the feed, with the
    // chip and FOLLOW LINK on the context bar.)
    linked_symbol_.clear();
    link_offer_dismissed_ = false;
    if (symbol_from_link_)
        clear_symbol_context();
    else
        update_context_bar();
}

void NewsScreen::refresh_holdings() {
    QPointer<NewsScreen> self = this;
    (void)QtConcurrent::run([self]() {
        QStringList syms = services::PortfolioService::held_symbols(); // local SQLite, per-thread connection
        syms.sort();
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, syms]() {
                if (!self || syms == self->holding_symbols_)
                    return;
                self->holding_symbols_ = syms;
                if (self->has_any_articles())
                    self->apply_filters_async();
            },
            Qt::QueuedConnection);
    });
}

void NewsScreen::on_group_symbol_changed(const SymbolRef& ref) {
    if (!ref.is_valid())
        return;
    // In the tagger's Yahoo form (RELIANCE on NSE -> RELIANCE.NS), so a broker-
    // spelled publisher still narrows to the articles tagged for it.
    const QString sym = news_ref_key(ref);
    if (sym.isEmpty())
        return;
    // Two kinds of caller share this entry point:
    //  * the link group: another panel (Equity Research, Watchlist …) changed
    //    its symbol — possibly while this screen is hidden. That used to become
    //    a text search that emptied the whole feed; now it is an offer, followed
    //    only with FOLLOW LINK on (on_link_symbol);
    //  * a direct hand-off — "Open in News", ui_open — which narrows the feed.
    // The broadcast is marked by our own SymbolContext connection in the same
    // emit (before or after the dock router delivers it), hence the deferral.
    if (link_broadcast_marker_ == sym) {
        on_link_symbol(sym);
        return;
    }
    pending_group_symbol_ = sym;
    QMetaObject::invokeMethod(
        this,
        [this]() {
            const QString s = std::exchange(pending_group_symbol_, QString());
            if (s.isEmpty())
                return;
            if (link_broadcast_marker_ == s)
                on_link_symbol(s);
            else
                set_symbol_context(s, /*from_link=*/false);
        },
        Qt::QueuedConnection);
}

void NewsScreen::on_refresh() {
    refresh_data(true);
}

void NewsScreen::on_manage_sources() {
    RssFeedManagerDialog dlg(this);
    dlg.exec();
    if (dlg.changed()) {
        LOG_INFO("NewsScreen", "RSS feed sources changed; reloading");
        refresh_data(true);
    }
}

void NewsScreen::on_article_clicked(const services::NewsArticle& article) {
    detail_panel_->set_earnings_days(earnings_days_);
    detail_panel_->show_article(article);
    feed_panel_->set_selected(article.id);
    if (!feed_panel_->model()->is_seen(article.id)) {
        feed_panel_->model()->mark_seen(article.id);
        pending_seen_ids_.insert(article.id);
        seen_flush_timer_->start();
        command_bar_->set_unseen_count(feed_panel_->model()->unseen_count());
    }

    // Re-show a previously-run AI analysis for this article, if one was saved.
    // Only a fresh ANALYZE click re-fetches/overwrites it.
    if (auto cached = services::NewsService::instance().cached_analysis(article.link))
        detail_panel_->show_analysis(*cached);

    // Find related articles from the same cluster
    for (const auto& cluster : clusters_) {
        if (cluster.lead_article.id == article.id ||
            std::any_of(cluster.articles.begin(), cluster.articles.end(),
                        [&](const services::NewsArticle& a) { return a.id == article.id; })) {
            QVector<services::NewsArticle> related;
            for (const auto& a : cluster.articles) {
                if (a.id != article.id)
                    related.append(a);
            }
            detail_panel_->show_related(related);
            break;
        }
    }

    // Check monitor matches for this article
    auto monitors = services::NewsMonitorService::instance().get_monitors();
    QVector<QPair<services::NewsMonitor, QStringList>> article_matches;
    for (const auto& monitor : monitors) {
        if (!monitor.enabled)
            continue;
        QStringList matched_kws;
        for (const auto& k : monitor.keywords) {
            if (article.headline.contains(k, Qt::CaseInsensitive) || article.summary.contains(k, Qt::CaseInsensitive))
                matched_kws.append(k);
        }
        if (!matched_kws.isEmpty())
            article_matches.append({monitor, matched_kws});
    }
    detail_panel_->show_monitor_matches(article_matches);

    // NER entities for this article (per-article cache filled by enrichment).
    if (auto entities = services::NewsNlpService::instance().cached_entities(article.id))
        detail_panel_->show_entities(*entities);
    else
        detail_panel_->show_entities({});

    // If geolocated, fetch nearby infrastructure. The reply is pinned to this
    // article so a slow Overpass answer can't land on the next one opened.
    if (auto geo = services::NewsNlpService::instance().cached_geo(article.id)) {
        QPointer<NewsScreen> geo_self = this;
        const QString link = article.link;
        services::NewsNlpService::instance().nearby_infrastructure(
            geo->primary_lat, geo->primary_lon, 50,
            [geo_self, link](bool ok, QVector<services::InfrastructureItem> items) {
                if (geo_self && ok && geo_self->detail_panel_->current_article_link() == link)
                    geo_self->detail_panel_->show_infrastructure(items);
            });
    }
}

void NewsScreen::on_cluster_clicked(const services::NewsCluster& cluster) {
    // The same click already emitted article_clicked for the lead article, which
    // filled the panel. Showing the article again here would reset all of that.
    QVector<services::NewsArticle> related;
    for (const auto& a : cluster.articles) {
        if (a.id != cluster.lead_article.id)
            related.append(a);
    }
    detail_panel_->show_related(related);
}

void NewsScreen::on_near_bottom() {
    if (visible_article_count_ >= wire_articles_.size())
        return;
    visible_article_count_ += PAGE_SIZE;
    auto visible = wire_articles_.mid(0, visible_article_count_);
    feed_panel_->model()->set_wire_articles(visible);
    LOG_DEBUG("NewsScreen", QString("Lazy-loaded to %1 articles").arg(visible.size()));
}

void NewsScreen::on_sidebar_category_clicked(const QString& category) {
    // The drawer sends the article category ("MARKETS"); the pills and the
    // persisted filter use the pill code ("MKT"). A category without a pill
    // stays a full name — the filter accepts it and no pill is highlighted.
    const QString code = news_category_codes().key(category, category);
    command_bar_->set_active_category(code);
    on_category_changed(code); // persists + re-filters like a pill click
}

void NewsScreen::on_sidebar_article_clicked(const services::NewsArticle& article) {
    on_article_clicked(article);
}

void NewsScreen::on_monitor_added(const QString& label, const QStringList& keywords) {
    services::NewsMonitorService::instance().add_monitor(label, keywords);
    update_monitors();
}

void NewsScreen::on_monitor_toggled(const QString& id) {
    services::NewsMonitorService::instance().toggle_monitor(id);
    update_monitors();
}

void NewsScreen::on_monitor_deleted(const QString& id) {
    services::NewsMonitorService::instance().delete_monitor(id);
    update_monitors();
}

void NewsScreen::on_analyze_requested(const QString& url) {
    QPointer<NewsScreen> self = this;
    // The feed item's headline and summary go along: when the article page cannot be
    // read, newer servers analyse those (and label the result) instead of refusing.
    const services::NewsArticle article = detail_panel_->current_article();
    const bool same = article.link == url;
    services::NewsService::instance().analyze_article(
        url,
        [self, url](bool ok, services::NewsAnalysis analysis) {
            if (!self)
                return;
            // The request can take many seconds; the user may have opened another
            // article since. Don't paint this article's analysis over a different one.
            if (self->detail_panel_->current_article_link() != url)
                return;
            if (ok)
                self->detail_panel_->show_analysis(analysis);
            else
                self->detail_panel_->show_analysis_failed(analysis.error); // e.g. HTTP_402 -> plans link
        },
        same ? article.headline : QString(), same ? article.summary : QString());
}

void NewsScreen::on_related_clicked(const services::NewsArticle& article) {
    on_article_clicked(article);
}

void NewsScreen::on_drawer_toggle() {
    set_drawer_open(!side_panel_->is_drawer_open());
}

void NewsScreen::set_drawer_open(bool open) {
    side_panel_->set_drawer_open(open);
    command_bar_->set_drawer_checked(open);
    if (open)
        maybe_fetch_predictions();
}

void NewsScreen::on_detail_closed() {
    // Feed gets the width back when the detail panel closes
    feed_panel_->model()->set_selected_id("");
}

void NewsScreen::clear_all_filters() {
    // Everything that can hide articles, the time range included (a 1H / 6H
    // range on a quiet morning is the usual reason for an empty list).
    time_range_ = QStringLiteral("24H");
    command_bar_->set_active_time_range(time_range_);
    active_category_ = QStringLiteral("ALL");
    active_lang_ = QStringLiteral("ALL");
    active_region_ = QStringLiteral("ALL");
    my_filter_ = QStringLiteral("ALL");
    active_variant_ = QStringLiteral("FULL");
    unread_only_ = false;
    window_from_ = window_to_ = 0;
    search_query_.clear();
    if (symbol_from_link_)
        link_offer_dismissed_ = true; // a followed symbol is not re-applied until it changes
    symbol_context_.clear();
    symbol_from_link_ = false;
    symbol_fallback_ = false;
    update_context_bar();
    command_bar_->set_active_category(active_category_);
    command_bar_->set_active_language(active_lang_);
    command_bar_->set_active_region(active_region_);
    command_bar_->set_active_my(my_filter_);
    command_bar_->set_active_variant(active_variant_);
    command_bar_->set_unread_only(false);
    command_bar_->set_search_text(QString());
    analytics_panel_->set_window(0, 0);
    QSettings s;
    s.beginGroup("news");
    s.setValue("time_range", time_range_);
    s.setValue("category", active_category_);
    s.setValue("language", active_lang_);
    s.setValue("region", active_region_);
    s.setValue("my_filter", my_filter_);
    s.setValue("variant", active_variant_);
    s.setValue("unread_only", false);
    s.endGroup();
    visible_article_count_ = PAGE_SIZE;
    ScreenStateManager::instance().notify_changed(this);
    apply_filters_async();
}

void NewsScreen::mark_all_read() {
    auto* model = feed_panel_->model();
    QStringList ids = model->mark_all_seen();
    // Everything in the current list, not only the rows loaded so far.
    for (const auto& a : std::as_const(filtered_articles_)) {
        if (!model->is_seen(a.id)) {
            model->mark_seen(a.id);
            ids.append(a.id);
        }
    }
    for (const auto& id : std::as_const(ids))
        pending_seen_ids_.insert(id);
    if (!pending_seen_ids_.isEmpty())
        seen_flush_timer_->start();
    command_bar_->set_unseen_count(model->unseen_count());
    if (unread_only_)
        apply_filters_async();
}

void NewsScreen::set_charts_open(bool open) {
    charts_open_ = open;
    analytics_panel_->setVisible(open);
    command_bar_->set_charts_checked(open);
    QSettings s;
    s.beginGroup("news");
    s.setValue("charts_open", open);
    s.endGroup();
}

void NewsScreen::update_feed_health_badge() {
    const auto health = services::NewsService::instance().feed_health();
    // The list of the last fetch — no database read on the GUI thread (this
    // runs on every show and after every refresh).
    const auto feeds = services::NewsService::instance().effective_feeds_snapshot();
    QSet<QString> enabled;
    QHash<QString, QString> names;
    for (const auto& f : feeds) {
        enabled.insert(f.id);
        names.insert(f.id, f.name);
    }
    int known = 0;
    int ok = 0;
    QStringList failing;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    for (const auto& h : health) {
        if (!enabled.contains(h.feed_id))
            continue;
        ++known;
        if (h.consecutive_failures == 0)
            ++ok;
        else
            failing << tr("%1 — %2").arg(names.value(h.feed_id, h.feed_id), h.last_error);
    }
    if (known == 0) {
        command_bar_->update_feed_health(0, 0, 0, {});
        return;
    }
    QString tip = tr("%1 of %2 sources answered on the last refresh.").arg(ok).arg(known);
    if (!failing.isEmpty())
        tip += QLatin1Char('\n') + failing.mid(0, 12).join(QLatin1Char('\n')) +
               (failing.size() > 12 ? tr("\n… and %1 more").arg(failing.size() - 12) : QString());
    tip += tr("\nClick for details.");
    int backing = 0;
    for (const auto& h : health)
        if (enabled.contains(h.feed_id) && h.retry_after > now)
            ++backing;
    command_bar_->update_feed_health(ok, known, backing, tip);
}

void NewsScreen::show_feed_health() {
    QDialog dlg(this);
    dlg.setWindowTitle(tr("News source health"));
    dlg.resize(760, 480);
    auto* layout = new QVBoxLayout(&dlg);
    auto* note = new QLabel(tr("A source that fails three refreshes in a row is paused and retried later (15 min, "
                               "doubling up to 6 h) so a dead feed does not cost a request every refresh."),
                            &dlg);
    note->setWordWrap(true);
    layout->addWidget(note);
    auto* table = new ui::DataTable(&dlg);
    table->set_headers({tr("Source"), tr("Region"), tr("Lang"), tr("Status"), tr("Articles"), tr("Last OK"),
                        tr("Last error")});
    const auto feeds = services::NewsService::instance().list_effective_feeds();
    QHash<QString, services::FeedHealth> health;
    for (const auto& h : services::NewsService::instance().feed_health())
        health.insert(h.feed_id, h);
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    namespace fmt = ui::formatting;
    for (const auto& f : feeds) {
        const auto h = health.value(f.id);
        QString status;
        if (!health.contains(f.id))
            status = tr("not fetched yet");
        else if (h.retry_after > now)
            status = tr("paused until %1").arg(fmt::time(h.retry_after));
        else if (h.consecutive_failures > 0)
            status = tr("failed %n time(s)", "", h.consecutive_failures);
        else
            status = tr("OK");
        table->add_row({f.name, f.region, f.lang.toUpper(), status,
                        h.last_ok_at > 0 ? QString::number(h.last_count) : QStringLiteral("—"),
                        h.last_ok_at > 0 ? fmt::relative(h.last_ok_at) : QStringLiteral("—"), h.last_error});
        const int row = table->rowCount() - 1;
        if (health.contains(f.id))
            table->set_cell_color(row, 3, h.consecutive_failures == 0 ? ui::colors::SUCCESS() : ui::colors::DANGER());
    }
    table->resizeColumnsToContents();
    table->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(table, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    auto* retry = buttons->addButton(tr("Retry paused sources"), QDialogButtonBox::ActionRole);
    connect(retry, &QPushButton::clicked, &dlg, [this, &dlg, health]() {
        for (auto it = health.cbegin(); it != health.cend(); ++it)
            if (it->consecutive_failures > 0)
                services::NewsService::instance().reset_feed_health(it.key());
        dlg.accept();
        refresh_data(true);
    });
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(buttons);
    dlg.exec();
}

// ── Core data pipeline ──────────────────────────────────────────────────────

void NewsScreen::fold_into_stored() {
    if (all_articles_.isEmpty())
        return;
    namespace nf = services::news_feed;
    stored_articles_ = nf::merge_sorted_unique(all_articles_, stored_articles_);
    const qint64 cutoff = QDateTime::currentSecsSinceEpoch() - kStoredWindowSec;
    // Canonical order is newest first: everything after the first too-old row goes.
    const auto first_old = std::find_if(stored_articles_.cbegin(), stored_articles_.cend(),
                                        [cutoff](const services::NewsArticle& a) { return a.sort_ts < cutoff; });
    stored_articles_.resize(std::distance(stored_articles_.cbegin(), first_old));
    all_articles_.clear();
}

void NewsScreen::schedule_filters() {
    if (filter_coalesce_timer_ && !filter_coalesce_timer_->isActive())
        filter_coalesce_timer_->start();
}

void NewsScreen::refresh_data(bool force) {
    // Never blank the screen while waiting (P11): what is on screen now — the
    // previous fetch and the stored articles — stays listed, and each feed's
    // articles join it as they arrive. With nothing to show yet the feed panel
    // shows its loading page (titled progress), never an empty black list.
    fold_into_stored();
    loading_ = true;
    fetch_done_ = 0;
    fetch_total_ = 0;
    command_bar_->set_loading(true);
    if (!has_any_articles()) {
        feed_panel_->set_loading(true);
        feed_panel_->set_loading_progress(0, 0, 0);
    }

    auto* svc = &services::NewsService::instance();

    QPointer<NewsScreen> self = this;
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(svc, &services::NewsService::articles_partial, this,
                    [self, conn](QVector<services::NewsArticle> articles, int done, int total) {
                        if (!self)
                            return;
                        self->fetch_done_ = done;
                        self->fetch_total_ = total;
                        self->command_bar_->set_loading_progress(done, total);
                        self->feed_panel_->set_loading_progress(done, total, static_cast<int>(articles.size()));
                        if (!articles.isEmpty()) {
                            self->all_articles_ = std::move(articles);
                            self->schedule_filters();
                        }
                        if (done == total)
                            QObject::disconnect(*conn);
                    });

    svc->fetch_all_news_progressive(force, [self, conn, svc](bool ok, QVector<services::NewsArticle> articles) {
        if (!self)
            return;
        QObject::disconnect(*conn);
        self->loading_ = false;
        self->command_bar_->set_loading(false);
        self->command_bar_->set_loading_progress(0, 0);
        self->update_feed_health_badge();
        // Every feed failing yields an empty result with feeds configured: the
        // error state (unless earlier articles are still on screen, which stay).
        self->last_fetch_failed_ = !ok || (articles.isEmpty() && svc->feed_count() > 0);
        if (!ok)
            LOG_ERROR("NewsScreen", logf("Failed to fetch news", {{"feeds", svc->feed_count()},
                                                                  {"error", "every feed failed (see the feed health list)"}}));
        else if (articles.isEmpty() && self->has_any_articles() && svc->feed_count() > 0)
            LOG_WARN("NewsScreen", "Refresh returned no articles — keeping the previous list");
        if (ok && !articles.isEmpty())
            self->all_articles_ = std::move(articles);
        // Always one final pass: it settles the list / empty / error page.
        if (self->filter_coalesce_timer_)
            self->filter_coalesce_timer_->stop();
        self->apply_filters_async();
        if (ok)
            self->refresh_volume_baseline(); // the fetch just recorded new volume history
    });
}

void NewsScreen::apply_filters_async() {
    const int gen = filter_generation_.fetch_add(1, std::memory_order_relaxed) + 1;

    QPointer<NewsScreen> self = this;
    // Implicitly shared — the merge of the two runs on the worker.
    auto live_copy = all_articles_;
    auto stored_copy = stored_articles_;
    const QString category = active_category_;
    const QString time_range = time_range_;
    const QString search_lower = search_query_.toLower();
    const QString sort = sort_mode_;
    const QString variant = active_variant_;
    const QString lang_filter = active_lang_.toLower();
    const QString region_filter = active_region_.toUpper();
    const bool unread_only = unread_only_;
    const QSet<QString> seen = unread_only ? feed_panel_->model()->seen_ids() : QSet<QString>();
    const qint64 window_from = window_from_;
    const qint64 window_to = window_to_;

    // Symbol context: stories TAGGED with the ticker (validated tags, data-vendor
    // suffix ignored), or naming it as written ("AAPL", "$AAPL") — never a
    // substring or a case-folded word ("reliance on imports" is not RELIANCE).
    const QString symbol_full = symbol_context_;
    const QString symbol_base = symbol_full.isEmpty() ? QString() : news_symbol_base(symbol_full);
    const bool symbol_mode = !symbol_base.isEmpty();
    // A followed (link-group) symbol never empties the list: nothing tagged ->
    // the whole feed, and the context bar says so.
    const bool symbol_soft = symbol_mode && symbol_from_link_;
    // The canonical (Yahoo) form for the price chart: "RELIANCE" -> "RELIANCE.NS"
    // when the symbol directory knows it.
    QString chart_symbol;
    if (symbol_mode) {
        chart_symbol = symbol_full.contains(QLatin1Char('.')) || symbol_full.contains(QLatin1Char('-'))
                           ? symbol_full
                           : services::NewsSymbolDirectory::current()->resolve(symbol_full);
        if (chart_symbol.isEmpty())
            chart_symbol = symbol_full;
    }
    const QStringList holdings = holding_symbols_;

    // MY filter: holdings and / or watchlist symbols.
    QStringList my_symbols;
    bool my_active = my_filter_ != QLatin1String("ALL");
    if (my_active) {
        auto& dir = services::NewsSymbolDirectory::instance();
        const auto wl = dir.watchlist_symbols();
        if (my_filter_ == QLatin1String("HOLDINGS") || my_filter_ == QLatin1String("MY"))
            my_symbols << holdings;
        if (my_filter_ == QLatin1String("WATCHLISTS") || my_filter_ == QLatin1String("MY"))
            for (auto it = wl.cbegin(); it != wl.cend(); ++it)
                my_symbols << it.value();
        if (my_filter_.startsWith(QLatin1String("WL:")))
            my_symbols << wl.value(my_filter_.mid(3));
        my_symbols.removeDuplicates();
    }

    (void)QtConcurrent::run([self, gen, live_copy = std::move(live_copy), stored_copy = std::move(stored_copy),
                             category, time_range, search_lower, sort, variant, lang_filter, region_filter,
                             unread_only, seen, window_from, window_to, symbol_mode, symbol_soft, symbol_full,
                             symbol_base, chart_symbol, holdings, my_active, my_symbols]() mutable {
        namespace nf = services::news_feed;
        // The network list plus what was known before (database, earlier
        // fetches); the network copy of a story wins. Both arrive newest first.
        if (!std::is_sorted(live_copy.cbegin(), live_copy.cend(), nf::newer_first<services::NewsArticle>))
            nf::sort_newest_first(live_copy);
        QVector<services::NewsArticle> articles_copy =
            stored_copy.isEmpty() ? std::move(live_copy) : nf::merge_sorted_unique(live_copy, stored_copy);
        auto out = std::make_shared<NewsFilterResult>();
        out->generation = gen;
        out->input_empty = articles_copy.isEmpty();
        // The ticker as written: case-sensitive whole word, two characters or more
        // ("V" / "F" / "T" only through their tags).
        const bool has_symbol_re = symbol_mode && symbol_base.size() >= 2;
        const QRegularExpression symbol_re =
            has_symbol_re ? QRegularExpression(QStringLiteral("(?<![\\w$.-])\\$?%1(?![\\w-])")
                                                   .arg(QRegularExpression::escape(symbol_base)))
                          : QRegularExpression();
        const QString fts_query = symbol_mode ? symbol_base.toLower() : search_lower;
        // 7D/30D history merge — off the UI thread (Database hands out per-thread
        // connections). Rows from the DB carry their persisted tone (or "unknown").
        if (time_range == "7D" || time_range == "30D") {
            const int64_t hist_window = (time_range == "30D") ? (30LL * 86400) : (7LL * 86400);
            const int64_t cutoff = QDateTime::currentSecsSinceEpoch() - hist_window;
            QSet<QString> live_ids;
            live_ids.reserve(articles_copy.size());
            for (const auto& a : std::as_const(articles_copy))
                live_ids.insert(a.id);
            auto merge_into = [&](const QVector<services::NewsArticle>& extras) {
                for (const auto& a : extras) {
                    if (live_ids.contains(a.id))
                        continue;
                    articles_copy.append(a);
                    // A row labelled by an older rule set: re-judge it (only rows
                    // flagged at all — the over-classified ones — to keep this cheap).
                    auto& row = articles_copy.last();
                    if (row.priority != services::Priority::ROUTINE || row.impact != services::Impact::LOW)
                        services::NewsService::classify_article(row);
                }
            };
            if (!fts_query.isEmpty()) {
                auto fts_result = fincept::NewsArticleRepository::instance().search_fts(fts_query, cutoff, 1000);
                if (fts_result.is_ok())
                    merge_into(fts_result.value());
            } else {
                auto db_result = fincept::NewsArticleRepository::instance().load_recent(cutoff, {}, 5000);
                if (db_result.is_ok())
                    merge_into(db_result.value());
            }
        }
        int64_t window_sec = 86400;
        if (time_range == "1H")
            window_sec = 3600;
        else if (time_range == "6H")
            window_sec = 21600;
        else if (time_range == "48H")
            window_sec = 172800;
        else if (time_range == "7D")
            window_sec = 604800;
        else if (time_range == "30D")
            window_sec = 30LL * 86400;
        const int64_t now_s = QDateTime::currentSecsSinceEpoch();
        const int64_t cutoff = now_s - window_sec;

        const NewsSymbolMatcher my_matcher(my_symbols);

        // Pass 1: every filter except the timeline window (the analytics show
        // the whole range, with the window highlighted by the zoom).
        QVector<services::NewsArticle> in_range;
        in_range.reserve(articles_copy.size());
        QVector<services::NewsArticle> symbol_rejected; // passed every filter but a followed symbol
        int rejected_time = 0, rejected_variant = 0, rejected_category = 0, rejected_search = 0;
        int rejected_lang = 0, rejected_region = 0, rejected_my = 0, rejected_unread = 0;

        for (const auto& a : articles_copy) {
            // Rows stored by older builds can carry a time in the future (feeds
            // that mislabel their zone); they never sort above real news.
            if (a.sort_ts < cutoff || a.sort_ts > now_s + nf::kMaxFutureSkewSec) {
                ++rejected_time;
                continue;
            }
            // Language filter — articles carry a detected language now; an
            // undetermined one does not pass a specific language filter.
            if (lang_filter != "all" && a.lang.toLower() != lang_filter) {
                ++rejected_lang;
                continue;
            }
            if (!news_region_matches(region_filter, a.region)) {
                ++rejected_region;
                continue;
            }
            // Variant filter
            if (variant == "FINANCE" && a.category != "MARKETS" && a.category != "EARNINGS" &&
                a.category != "ECONOMIC" && a.category != "REGULATORY") {
                ++rejected_variant;
                continue;
            }
            if (variant == "CRYPTO" && a.category != "CRYPTO" && a.category != "TECH") {
                ++rejected_variant;
                continue;
            }
            if (variant == "MACRO" && a.category != "ECONOMIC" && a.category != "REGULATORY" &&
                a.category != "GEOPOLITICS" && a.category != "ENERGY") {
                ++rejected_variant;
                continue;
            }
            // Category filter
            if (category != "ALL") {
                const auto& cat_map = news_category_codes();
                auto it = cat_map.find(category);
                // A pill code maps to its category; anything else (the INTEL
                // drawer's full names, e.g. "REGULATORY") is the category itself.
                const QString& wanted = it != cat_map.end() ? it.value() : category;
                if (a.category != wanted) {
                    ++rejected_category;
                    continue;
                }
            }
            // Symbol context / typed search. The symbol test is applied last (below)
            // so a followed symbol can fall back to everything else that passed.
            bool symbol_ok = true;
            if (symbol_mode) {
                symbol_ok = false;
                for (const auto& t : std::as_const(a.tickers)) {
                    if (t.compare(symbol_base, Qt::CaseInsensitive) == 0 ||
                        t.compare(symbol_full, Qt::CaseInsensitive) == 0 ||
                        news_symbol_base(t).compare(symbol_base, Qt::CaseInsensitive) == 0) {
                        symbol_ok = true;
                        break;
                    }
                }
                if (!symbol_ok && has_symbol_re)
                    symbol_ok = symbol_re.match(a.headline).hasMatch() || symbol_re.match(a.summary).hasMatch();
            } else if (!search_lower.isEmpty()) {
                const QString hl = a.headline.toLower();
                const QString sum = a.summary.toLower();
                bool match = hl.contains(search_lower) || sum.contains(search_lower) ||
                             a.source.toLower().contains(search_lower);
                if (!match) {
                    for (const auto& t : std::as_const(a.tickers)) {
                        if (t.toLower().contains(search_lower)) {
                            match = true;
                            break;
                        }
                    }
                }
                if (!match) {
                    ++rejected_search;
                    continue;
                }
            }
            if (my_active && (my_matcher.empty() || !my_matcher.matches(a))) {
                ++rejected_my;
                continue;
            }
            if (unread_only && seen.contains(a.id)) {
                ++rejected_unread;
                continue;
            }
            if (!symbol_ok) {
                ++rejected_search;
                if (symbol_soft)
                    symbol_rejected.append(a);
                continue;
            }
            in_range.append(a);
        }
        // Nothing tagged with a followed symbol: the whole (otherwise filtered)
        // feed, never an empty list the user did not ask for.
        if (symbol_soft && in_range.isEmpty() && !symbol_rejected.isEmpty()) {
            in_range = std::move(symbol_rejected);
            out->symbol_fallback = true;
        }
        symbol_rejected = {};

        // ── Analytics over the whole range ─────────────────────────────────
        {
            auto& snap = out->analytics;
            snap.from = cutoff;
            snap.to = now_s;
            snap.bucket = na::bucket_seconds_for_span(window_sec);
            QVector<na::Sample> samples;
            samples.reserve(in_range.size());
            QVector<na::TrendInput> ticker_in;
            QVector<na::TrendInput> cat_in;
            QVector<na::TrendInput> region_in;
            for (const auto& a : std::as_const(in_range)) {
                const int tone = a.sentiment == services::Sentiment::BULLISH   ? 1
                                 : a.sentiment == services::Sentiment::BEARISH ? -1
                                                                               : 0;
                samples.append({a.sort_ts, a.sentiment_score, a.sentiment_scored, a.sentiment_scored ? tone : 0});
                if (!a.tickers.isEmpty())
                    ticker_in.append({a.sort_ts, a.tickers, a.sentiment_score, a.sentiment_scored});
                cat_in.append({a.sort_ts, {a.category}, a.sentiment_score, a.sentiment_scored});
                region_in.append({a.sort_ts, {a.region}, a.sentiment_score, a.sentiment_scored});
                ++snap.total;
                if (a.sentiment_scored)
                    ++snap.scored;
            }
            snap.series = na::bucketize(samples, cutoff, now_s, snap.bucket);
            // Tickers: the second half of the range against the first half.
            snap.tickers = na::trending(ticker_in, now_s, window_sec / 2, 15, 2);
            // Categories / regions: the whole range (current = count over the range).
            snap.categories = na::trending(cat_in, now_s, window_sec, 30, 1);
            snap.regions = na::trending(region_in, now_s, window_sec, 30, 1);
            if (symbol_mode && !out->symbol_fallback) {
                snap.symbol = chart_symbol;
                for (const auto& a : std::as_const(in_range))
                    snap.symbol_news.append({a.sort_ts, a.headline,
                                             a.sentiment_scored ? a.sentiment_score
                                                                : std::numeric_limits<double>::quiet_NaN()});
            }
            for (const auto& r : std::as_const(snap.tickers)) {
                out->top_symbols.append(r.key);
                if (out->top_symbols.size() >= 20)
                    break;
            }
        }

        // Pass 2: the timeline window.
        QVector<services::NewsArticle>& filtered = out->filtered;
        if (window_from > 0 && window_to > window_from) {
            filtered.reserve(in_range.size());
            for (const auto& a : std::as_const(in_range))
                if (a.sort_ts >= window_from && a.sort_ts <= window_to)
                    filtered.append(a);
        } else {
            filtered = std::move(in_range);
        }

        // Counts and tone of the list (tone over SCORED articles only).
        double tone_sum = 0;
        int tone_n = 0;
        for (const auto& a : std::as_const(filtered)) {
            out->category_counts[a.category]++;
            if (!a.sentiment_scored) {
                ++out->not_scored;
                continue;
            }
            tone_sum += a.sentiment_score;
            ++tone_n;
            if (a.sentiment == services::Sentiment::BULLISH)
                out->bullish++;
            else if (a.sentiment == services::Sentiment::BEARISH)
                out->bearish++;
            else
                out->neutral++;
        }
        if (tone_n > 0)
            out->mean_tone = tone_sum / tone_n;

        // Sort. NEWEST (the default) is the canonical order every news view
        // shares: publication time (UTC) newest first, ties by id — identical on
        // every refresh whatever order the feeds answered in.
        if (sort != QLatin1String("RELEVANCE")) {
            nf::sort_newest_first(filtered);
        } else {
            // Score once per article, not once per comparison.
            QVector<QPair<double, int>> order;
            order.reserve(filtered.size());
            for (int i = 0; i < filtered.size(); ++i)
                order.append({news_relevance(filtered[i], now_s), i});
            std::stable_sort(order.begin(), order.end(), [&filtered](const auto& x, const auto& y) {
                if (x.first != y.first)
                    return x.first > y.first;
                return nf::newer_first(filtered[x.second], filtered[y.second]);
            });
            QVector<services::NewsArticle> ranked;
            ranked.reserve(filtered.size());
            for (const auto& o : order)
                ranked.append(filtered[o.second]);
            filtered = std::move(ranked);
        }

        // WIRE view: the same headline from several sources is one row ("+N").
        // In the default view (no search / ticker / MY filter) one source also
        // shows at most kNewsSourceBurstPerHour stories per hour of publication,
        // so a burst (a wire's batch of press releases, a paper's daily dump)
        // cannot fill the screen; the order of what is shown is unchanged, and a
        // search still lists every match.
        {
            QHash<QString, int> lead_of; // duplicate key -> index into wire
            QVector<services::NewsArticle> wire;
            wire.reserve(filtered.size());
            for (const auto& a : std::as_const(filtered)) {
                const QString key = na::duplicate_key(a.headline);
                const auto it = lead_of.constFind(key);
                if (key.size() >= 12 && it != lead_of.constEnd()) {
                    out->duplicate_counts[wire[it.value()].id]++;
                    continue;
                }
                lead_of.insert(key, static_cast<int>(wire.size()));
                wire.append(a);
            }
            const bool default_view = (!symbol_mode || out->symbol_fallback) && search_lower.isEmpty() && !my_active;
            out->wire = default_view ? nf::cap_per_source_hour(wire, kNewsSourceBurstPerHour) : std::move(wire);
        }

        // ── Ticker mentions (drawer): validated tags only, tone over scored ──
        auto add_mention = [](NewsTickerMention& m, const services::NewsArticle& a) {
            ++m.count;
            if (a.sentiment_scored) {
                m.net_sentiment += a.sentiment_score; // summed here, averaged below
                ++m.scored;
            }
            if (a.sentiment == services::Sentiment::BULLISH)
                ++m.bullish;
            else if (a.sentiment == services::Sentiment::BEARISH)
                ++m.bearish;
            m.latest_ts = std::max<int64_t>(m.latest_ts, a.sort_ts);
        };
        auto finish = [](QHash<QString, NewsTickerMention>& by_sym, int min_count, int max_rows) {
            QVector<NewsTickerMention> res;
            for (auto it = by_sym.begin(); it != by_sym.end(); ++it) {
                if (it->count < min_count)
                    continue;
                if (it->scored > 0)
                    it->net_sentiment /= it->scored;
                res.append(*it);
            }
            std::sort(res.begin(), res.end(), [](const auto& x, const auto& y) {
                if (x.count != y.count)
                    return x.count > y.count;
                return x.latest_ts > y.latest_ts;
            });
            if (res.size() > max_rows)
                res.resize(max_rows);
            return res;
        };

        QHash<QString, NewsTickerMention> trending_map;
        const NewsSymbolMatcher holdings_matcher(holdings);
        struct HoldingMatcher {
            QString symbol;
            QString base;
            QRegularExpression re;
        };
        QVector<HoldingMatcher> holding_matchers;
        holding_matchers.reserve(holdings.size());
        for (const auto& h : holdings) {
            const QString base = news_symbol_base(h);
            if (!base.isEmpty())
                holding_matchers.append({h, base, news_symbol_regex(base)});
        }
        QHash<QString, NewsTickerMention> holdings_map;

        for (const auto& a : std::as_const(filtered)) {
            QSet<QString> seen_syms;
            for (const auto& t : std::as_const(a.tickers)) {
                const QString up = t.toUpper();
                if (up.isEmpty() || seen_syms.contains(up))
                    continue;
                seen_syms.insert(up);
                auto& m = trending_map[up];
                m.symbol = up;
                add_mention(m, a);
            }
            if (holding_matchers.isEmpty() || !holdings_matcher.matches(a))
                continue;
            for (const auto& hm : std::as_const(holding_matchers)) {
                bool hit = seen_syms.contains(hm.base) || seen_syms.contains(hm.symbol.toUpper());
                if (!hit)
                    hit = hm.re.match(a.headline).hasMatch() || hm.re.match(a.summary).hasMatch();
                if (!hit)
                    continue;
                auto& m = holdings_map[hm.symbol];
                m.symbol = hm.symbol;
                add_mention(m, a);
            }
        }
        out->trending = finish(trending_map, kNewsTrendingMinMentions, kNewsTrendingMax);
        out->holding_mentions = finish(holdings_map, 1, 25);
        // Change vs the previous half of the range, from the analytics pass.
        QHash<QString, int> previous;
        for (const auto& r : std::as_const(out->analytics.tickers))
            previous.insert(r.key, r.previous);
        for (auto& m : out->trending)
            m.previous = previous.value(m.symbol, -1);

        out->clusters = services::cluster_articles(filtered);

        if (!articles_copy.isEmpty() && filtered.isEmpty()) {
            LOG_DEBUG("NewsScreen", QString("Filter rejected ALL %1 articles (time=%2 lang=%3 region=%4 variant=%5 "
                                           "category=%6 search=%7 my=%8 unread=%9)")
                                       .arg(articles_copy.size())
                                       .arg(rejected_time)
                                       .arg(rejected_lang)
                                       .arg(rejected_region)
                                       .arg(rejected_variant)
                                       .arg(rejected_category)
                                       .arg(rejected_search)
                                       .arg(rejected_my)
                                       .arg(rejected_unread));
        }

        QMetaObject::invokeMethod(
            self,
            [self, gen, out = std::shared_ptr<const NewsFilterResult>(out)]() {
                if (!self)
                    return;
                if (gen < self->filter_generation_.load(std::memory_order_relaxed))
                    return; // a newer filter pass is under way
                self->update_ui_from_filtered(out);
            },
            Qt::QueuedConnection);
    });
}

void NewsScreen::update_ui_from_filtered(const std::shared_ptr<const NewsFilterResult>& result) {
    const auto& filtered = result->filtered;
    filtered_articles_ = filtered;
    wire_articles_ = result->wire;
    clusters_ = result->clusters;

    // Feed model
    auto* model = feed_panel_->model();
    model->set_wire_articles(wire_articles_.mid(0, visible_article_count_));
    model->set_duplicate_counts(result->duplicate_counts);
    model->set_clusters(clusters_);
    model->set_view_mode(view_mode_);
    model->set_earnings_days(earnings_days_);

    // A followed symbol with nothing tagged: the bar says the whole feed is shown.
    if (result->symbol_fallback != symbol_fallback_) {
        symbol_fallback_ = result->symbol_fallback;
        update_context_bar();
    }

    // Which page the feed shows. Never an empty list: rows when anything passes
    // the filters; while a fetch is still running, the loading page — which says
    // so when articles have arrived but the filters hide all of them (a 1H
    // range, a language / region / category, a restored saved search) and
    // offers CLEAR FILTERS; once it is done, an empty state that says WHY.
    if (!filtered.isEmpty()) {
        feed_panel_->show_list();
    } else if (loading_) {
        feed_panel_->set_loading(true);
        feed_panel_->set_loading_progress(fetch_done_, fetch_total_, static_cast<int>(all_articles_.size()));
        feed_panel_->set_loading_filtered_out(!result->input_empty);
    } else {
        if (has_any_articles() && !symbol_context_.isEmpty() && !symbol_from_link_) {
            // Narrowed to a symbol nothing is tagged with: say so, with SHOW ALL
            // NEWS (the context bar above offers it too).
            feed_panel_->set_empty_state(true, NewsFeedPanel::EmptyKind::Symbol, symbol_context_);
        } else if (has_any_articles()) {
            feed_panel_->set_empty_state(true, NewsFeedPanel::EmptyKind::Filtered);
        } else if (last_fetch_failed_) {
            QStringList failing;
            for (const auto& h : services::NewsService::instance().feed_health())
                if (h.consecutive_failures > 0)
                    failing << QStringLiteral("%1: %2").arg(h.name.isEmpty() ? h.feed_id : h.name, h.last_error);
            failing.sort();
            QString detail = failing.mid(0, 8).join(QStringLiteral(" · "));
            if (failing.size() > 8)
                detail += tr(" · and %1 more").arg(failing.size() - 8);
            feed_panel_->set_empty_state(true, NewsFeedPanel::EmptyKind::Error, detail);
        } else {
            feed_panel_->set_empty_state(true, NewsFeedPanel::EmptyKind::NoArticles);
        }
    }

    // Command bar counts
    command_bar_->set_article_count(filtered.size());
    const auto breaking = services::get_breaking_clusters(clusters_);
    command_bar_->set_alert_count(static_cast<int>(breaking.size()));
    command_bar_->set_unseen_count(model->unseen_count());

    // Intel strip stats + tone
    command_bar_->update_stats(services::NewsService::instance().feed_count(), filtered.size(), clusters_.size(),
                               services::NewsService::instance().active_sources().size());
    command_bar_->update_sentiment(result->bullish, result->bearish, result->neutral, result->not_scored,
                                   result->mean_tone);

    // Drawer
    QVector<services::NewsArticle> top5;
    for (int i = 0; i < std::min(5, static_cast<int>(filtered.size())); i++)
        top5.append(filtered[i]);
    side_panel_->update_top_stories(top5);
    side_panel_->update_categories(result->category_counts);
    auto with_earnings = [this](QVector<NewsTickerMention> rows) {
        for (auto& r : rows) {
            const auto it = earnings_days_.constFind(r.symbol.toUpper());
            if (it != earnings_days_.constEnd())
                r.earnings = it.value() == 0 ? tr("ER today") : tr("ER %1d").arg(it.value());
        }
        return rows;
    };
    side_panel_->update_trending(with_earnings(result->trending));
    side_panel_->update_holdings_mentions(with_earnings(result->holding_mentions));

    // Analytics
    analytics_panel_->set_snapshot(result->analytics);
    analytics_panel_->set_sectors(sectors_);
    analytics_panel_->set_window(window_from_, window_to_);
    subscribe_ticker_info(result->top_symbols);

    // Breaking banner (several stories, newest first)
    if (!breaking.isEmpty()) {
        feed_panel_->show_breaking(breaking);

        auto& repo = fincept::SettingsRepository::instance();
        auto get_bool = [&](const QString& key, bool def) -> bool {
            auto r = repo.get(key);
            return r.is_ok() && !r.value().isEmpty() ? (r.value() == "1") : def;
        };
        if (get_bool("notifications.news_breaking", true)) {
            using namespace fincept::notifications;
            const int64_t notify_now = QDateTime::currentSecsSinceEpoch();
            for (const auto& cluster : breaking) {
                const QString& lead_id = cluster.lead_article.id;
                if (cluster.lead_article.undated || notify_now - cluster.lead_article.sort_ts > kNewsNotifyMaxAgeSec)
                    continue; // stale (or of unknown age): already in the feed when we started
                if (notified_breaking_.contains(lead_id))
                    continue;
                if (notified_breaking_.size() > kNewsNotifiedSetCap)
                    notified_breaking_.clear();
                notified_breaking_.insert(lead_id);

                NotificationRequest req;
                req.title = tr("BREAKING: %1").arg(cluster.lead_article.headline.left(80));
                req.message = cluster.lead_article.summary.isEmpty() ? cluster.lead_article.source
                                                                     : cluster.lead_article.summary.left(160);
                req.level = cluster.lead_article.priority == services::Priority::FLASH ? NotifLevel::Critical
                                                                                       : NotifLevel::Alert;
                req.trigger = NotifTrigger::NewsAlert;
                NotificationService::instance().send(req);
            }
        }
    }

    // Ticker strip
    QVector<services::NewsArticle> ticker_articles;
    for (const auto& a : filtered) {
        if (a.priority == services::Priority::FLASH || a.priority == services::Priority::URGENT ||
            a.priority == services::Priority::BREAKING)
            ticker_articles.append(a);
    }
    ticker_strip_->set_articles(ticker_articles);

    update_monitors();
    compute_deviations();
    request_enrichment();
}

void NewsScreen::request_enrichment() {
    if (!enrichment_timer_)
        return;
    enrichment_timer_->start();
}

void NewsScreen::run_enrichment_now() {
    if (filtered_articles_.isEmpty())
        return;

    // Cap input size — the drawer renders top-N anyway and the top of the
    // filtered list is what the user is looking at.
    constexpr int kMaxEnrichArticles = 200;
    const auto subset = filtered_articles_.mid(0, kMaxEnrichArticles);

    // Skip a pass over an unchanged set: NER/places come from the per-article
    // cache and the velocity signals only move with the clock.
    QStringList ids;
    ids.reserve(subset.size());
    for (const auto& a : subset)
        ids.append(a.id);
    const size_t signature = qHash(ids);
    const int64_t now = QDateTime::currentSecsSinceEpoch();
    if (signature == last_enrich_signature_ && now - last_enrich_at_ < kEnrichMinIntervalSec) {
        maybe_fetch_predictions();
        return;
    }
    last_enrich_signature_ = signature;
    last_enrich_at_ = now;

    const int gen = enrichment_generation_.fetch_add(1, std::memory_order_relaxed) + 1;
    QPointer<NewsScreen> self = this;
    QHash<QString, qint64> ts_by_id;
    for (const auto& a : subset)
        ts_by_id.insert(a.id, a.sort_ts);
    QHash<QString, QPair<double, bool>> tone_by_id;
    for (const auto& a : subset)
        tone_by_id.insert(a.id, {a.sentiment_score, a.sentiment_scored});
    const qint64 span = std::max<qint64>(3600, now - (subset.isEmpty() ? now : [&subset]() {
                                                     qint64 lo = subset.first().sort_ts;
                                                     for (const auto& a : subset)
                                                         lo = std::min<qint64>(lo, a.sort_ts);
                                                     return lo;
                                                 }()));

    // NER + places -> entities / locations sections, feed-row geo marks, the
    // per-article cache the detail panel reads, trending entities, and the
    // topics prediction markets are matched to.
    services::NewsNlpService::instance().enrich_articles(
        subset, [self, gen, ts_by_id, tone_by_id, now, span](bool ok, services::NewsEnrichment result) {
            if (!self || gen != self->enrichment_generation_.load(std::memory_order_relaxed))
                return; // superseded by a newer filter
            if (!ok) {
                self->last_enrich_signature_ = 0; // let the next pass retry
                return;
            }
            self->side_panel_->update_entities(result.ner);
            self->side_panel_->update_locations(result.geo);
            QSet<QString> geo_ids;
            geo_ids.reserve(result.geo.size());
            for (const auto& g : std::as_const(result.geo))
                geo_ids.insert(g.id);
            self->feed_panel_->model()->set_geo_articles(geo_ids);

            // Trending entities: second half of the enriched span vs the first half.
            QVector<na::TrendInput> inputs;
            for (const auto& e : std::as_const(result.ner.per_article)) {
                QStringList keys = e.organizations + e.people;
                for (const auto& c : e.countries)
                    keys << c.first;
                if (keys.isEmpty())
                    continue;
                const auto tone = tone_by_id.value(e.id);
                inputs.append({ts_by_id.value(e.id), keys, tone.first, tone.second});
            }
            self->analytics_panel_->set_entities(na::trending(inputs, now, std::max<qint64>(1800, span / 2), 15, 2));

            // Prediction topics: the most mentioned organisations / people.
            QStringList terms;
            for (const auto& o : std::as_const(result.ner.top_organizations))
                if (terms.size() < 2)
                    terms << o.name;
            for (const auto& p : std::as_const(result.ner.top_people))
                if (terms.size() < 3)
                    terms << p.name;
            self->prediction_terms_ = terms;
            self->maybe_fetch_predictions();
        });

    // Correlation signals — computed in-process (synchronous).
    services::NewsCorrelationService::instance().detect_signals(
        subset, [self](bool ok, QVector<services::CorrelationSignal> sigs) {
            if (self && ok)
                self->side_panel_->update_signals(sigs);
        });
}

void NewsScreen::maybe_fetch_predictions() {
    if (!visible_ || !side_panel_->is_drawer_open())
        return;
    // Markets about the feed's topics only (the drawer used to list the 20
    // most recently bet play-money markets, unrelated to the headlines).
    QStringList terms = prediction_terms_;
    if (terms.isEmpty()) {
        // Before the NER pass: the most mentioned tickers' company names are not
        // known here, so fall back to nothing rather than to unrelated markets.
        side_panel_->update_predictions({});
        return;
    }
    QPointer<NewsScreen> self = this;
    services::NewsCorrelationService::instance().fetch_predictions_for(
        terms, [self](bool ok, QVector<services::PredictionMarket> preds) {
            if (self && ok)
                self->side_panel_->update_predictions(preds);
        });
}

void NewsScreen::update_monitors() {
    auto monitors = services::NewsMonitorService::instance().get_monitors();
    auto matches = services::NewsMonitorService::instance().scan_monitors(monitors, filtered_articles_);
    side_panel_->update_monitors(monitors, matches);
    feed_panel_->model()->set_monitor_matches(matches, monitors);

    int total_alerts = 0;
    for (auto it = matches.begin(); it != matches.end(); ++it)
        total_alerts += it.value().size();
    command_bar_->update_monitor_summary(monitors.size(), total_alerts);

    // Notifications
    auto& repo = fincept::SettingsRepository::instance();
    auto get_bool = [&](const QString& key, bool def) -> bool {
        auto r = repo.get(key);
        return r.is_ok() && !r.value().isEmpty() ? (r.value() == "1") : def;
    };
    if (get_bool("notifications.news_monitors", true)) {
        using namespace fincept::notifications;
        const int64_t notify_now = QDateTime::currentSecsSinceEpoch();
        for (const auto& monitor : monitors) {
            if (!monitor.enabled)
                continue;
            const auto& articles = matches.value(monitor.id);
            for (const auto& article : articles) {
                if (article.undated || notify_now - article.sort_ts > kNewsNotifyMaxAgeSec)
                    continue; // stale (or of unknown age): already in the feed when we started
                const QString dedup_key = monitor.id + ":" + article.id;
                if (notified_monitors_.contains(dedup_key))
                    continue;
                if (notified_monitors_.size() > kNewsNotifiedSetCap)
                    notified_monitors_.clear();
                notified_monitors_.insert(dedup_key);

                NotificationRequest req;
                req.title = tr("WATCH \"%1\": %2").arg(monitor.label, article.headline.left(70));
                req.message = article.summary.isEmpty() ? article.source : article.summary.left(160);
                req.level = NotifLevel::Warning;
                req.trigger = NotifTrigger::NewsAlert;
                NotificationService::instance().send(req);
            }
        }
    }
}

void NewsScreen::refresh_volume_baseline(bool force) {
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (baseline_loading_ || (!force && now - baseline_loaded_at_ < kBaselineReloadSec))
        return;
    baseline_loading_ = true;
    QPointer<NewsScreen> self = this;
    (void)QtConcurrent::run([self, now]() {
        // Observed hours of the last 7 days (an "all" row exists for every hour a
        // fetch saw), excluding the hour being judged. A category without a row
        // in an observed hour had no articles then: zero.
        auto& repo = fincept::NewsAnalyticsRepository::instance();
        const qint64 hour_now = now - (now % 3600);
        const qint64 since = hour_now - 7LL * 86400;
        QSet<qint64> observed;
        if (auto all = repo.load_volume(since, QStringLiteral("all")); all.is_ok())
            for (const auto& r : all.value())
                if (r.hour_ts < hour_now - 3600)
                    observed.insert(r.hour_ts);
        QHash<QString, QHash<qint64, int>> per_cat;
        if (auto cat = repo.load_volume(since, QStringLiteral("cat")); cat.is_ok())
            for (const auto& r : cat.value())
                per_cat[r.key][r.hour_ts] = r.count;
        QHash<QString, na::Baseline> baselines;
        for (auto it = per_cat.cbegin(); it != per_cat.cend(); ++it) {
            QVector<int> counts;
            counts.reserve(observed.size());
            for (const qint64 h : std::as_const(observed))
                counts.append(it.value().value(h, 0));
            baselines.insert(it.key(), na::baseline_of(counts));
        }
        const int n = static_cast<int>(observed.size());
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, baselines, n, now]() {
                if (!self)
                    return;
                self->baseline_loading_ = false;
                self->baselines_ = baselines;
                self->baseline_observed_hours_ = n;
                self->baseline_loaded_at_ = now;
                self->compute_deviations();
            },
            Qt::QueuedConnection);
    });
}

void NewsScreen::compute_deviations() {
    const int64_t now = QDateTime::currentSecsSinceEpoch();
    const int64_t hour_ago = now - 3600;

    // Last-hour volume per category across the WHOLE feed (not the UI filter, so
    // picking a category does not change the "normal" level).
    QMap<QString, int> current_counts;
    for (const auto& a : std::as_const(all_articles_)) {
        if (a.sort_ts >= hour_ago && !a.undated) // an undated item's time is only when we first saw it
            current_counts[a.category]++;
    }

    // Judged against the persisted hourly history (news_volume_hourly), which
    // survives restarts — the in-memory hourly samples it replaces needed 24
    // hours of uninterrupted runtime and were lost on every restart.
    QVector<QPair<QString, double>> deviations;
    if (!loading_) { // a half-loaded list is not comparable to the baseline
        for (auto it = current_counts.cbegin(); it != current_counts.cend(); ++it) {
            const double z = na::zscore(it.value(), baselines_.value(it.key()), kBaselineMinHours, 0.5);
            if (!std::isnan(z) && z >= 3.0)
                deviations.append({it.key(), z});
        }
    }
    std::sort(deviations.begin(), deviations.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    last_deviations_ = deviations;

    side_panel_->update_deviations(deviations);
    side_panel_->update_baseline_status(baseline_observed_hours_, kBaselineMinHours);
    command_bar_->update_deviations(deviations); // intel strip

    auto& repo = fincept::SettingsRepository::instance();
    auto get_bool = [&](const QString& key, bool def) -> bool {
        auto r = repo.get(key);
        return r.is_ok() && !r.value().isEmpty() ? (r.value() == "1") : def;
    };

    // Re-arm categories that have calmed down, so a later spike notifies again.
    for (auto it = notified_deviations_.begin(); it != notified_deviations_.end();) {
        const QString cat = *it;
        const bool still_spiking = std::any_of(deviations.cbegin(), deviations.cend(),
                                               [&cat](const auto& d) { return d.first == cat; });
        it = still_spiking ? std::next(it) : notified_deviations_.erase(it);
    }

    if (get_bool("notifications.news_deviations", true)) {
        using namespace fincept::notifications;
        for (const auto& [category, z_score] : deviations) {
            if (notified_deviations_.contains(category))
                continue;
            notified_deviations_.insert(category);

            NotificationRequest req;
            req.title = tr("DEVIATION: %1 news spike").arg(category);
            req.message = tr("Unusual volume detected (z-score: %1). More %2 articles than normal.")
                              .arg(z_score, 0, 'f', 1)
                              .arg(category);
            req.level = z_score >= 5.0 ? NotifLevel::Critical : NotifLevel::Warning;
            req.trigger = NotifTrigger::NewsAlert;
            NotificationService::instance().send(req);
        }
    }

    // FLASH articles
    if (get_bool("notifications.news_flash", true)) {
        using namespace fincept::notifications;
        for (const auto& article : filtered_articles_) {
            if (article.priority != services::Priority::FLASH)
                continue;
            if (article.impact != services::Impact::HIGH)
                continue;
            // Only fresh items: every FLASH item of the last day used to fire a
            // Critical notification at launch.
            if (article.undated || now - article.sort_ts > kNewsNotifyMaxAgeSec)
                continue;
            if (notified_flash_.contains(article.id))
                continue;
            if (notified_flash_.size() > kNewsNotifiedSetCap)
                notified_flash_.clear();
            notified_flash_.insert(article.id);

            NotificationRequest req;
            req.title = tr("FLASH: %1").arg(article.headline.left(80));
            req.message = article.summary.isEmpty() ? article.source : article.summary.left(160);
            req.level = NotifLevel::Critical;
            req.trigger = NotifTrigger::NewsAlert;
            NotificationService::instance().send(req);
        }
    }

    check_saved_alerts(deviations);
    refresh_volume_baseline(); // no-op unless the cached baselines are stale
}

void NewsScreen::subscribe_ticker_info(const QStringList& symbols) {
    if (!visible_)
        return;
    auto& hub = datahub::DataHub::instance();
    QStringList topics;
    for (const QString& sym : symbols) {
        if (info_subscribed_.contains(sym) || info_subscribed_.size() >= 40)
            continue;
        info_subscribed_.insert(sym);
        const QString topic = services::MarketDataService::info_topic(sym);
        topics.append(topic);
        hub.subscribe(this, topic, [this, sym](const QVariant& v) {
            if (!v.canConvert<services::InfoData>())
                return;
            const auto info = v.value<services::InfoData>();
            bool changed = false;
            if (!info.sector.isEmpty() && sectors_.value(sym) != info.sector) {
                sectors_.insert(sym, info.sector);
                changed = true;
            }
            int days = -1;
            if (info.earnings_date.isValid())
                days = static_cast<int>(QDate::currentDate().daysTo(info.earnings_date));
            if (days >= 0 && days <= kEarningsHorizonDays) {
                if (earnings_days_.value(sym, -1) != days) {
                    earnings_days_.insert(sym, days);
                    changed = true;
                }
            } else if (earnings_days_.remove(sym) > 0) {
                changed = true;
            }
            if (!changed)
                return;
            feed_panel_->model()->set_earnings_days(earnings_days_);
            detail_panel_->set_earnings_days(earnings_days_);
            analytics_panel_->set_sectors(sectors_);
            // Key-events line: who reports today, how many this week.
            QStringList today;
            QStringList week;
            for (auto it = earnings_days_.cbegin(); it != earnings_days_.cend(); ++it) {
                if (it.value() == 0)
                    today << it.key();
                else if (it.value() <= 7)
                    week << QStringLiteral("%1 (%2d)").arg(it.key()).arg(it.value());
            }
            today.sort();
            week.sort();
            QString text;
            if (!today.isEmpty())
                text = tr("EARNINGS TODAY: %1").arg(today.mid(0, 4).join(QStringLiteral(", ")));
            if (!week.isEmpty())
                text += (text.isEmpty() ? QString() : QStringLiteral("  ")) + tr("%n REPORT(S) THIS WEEK", "", week.size());
            command_bar_->update_events(text, tr("Earnings dates of companies in the current headlines\nToday: %1\n"
                                                 "Next 7 days: %2")
                                                  .arg(today.isEmpty() ? tr("none") : today.join(QStringLiteral(", ")),
                                                       week.isEmpty() ? tr("none") : week.join(QStringLiteral(", "))));
        });
    }
    if (!topics.isEmpty())
        hub.request(topics);
}

// ── Saved searches & alerts ─────────────────────────────────────────────────

QVariantMap NewsScreen::current_filter_state() const {
    return {
        {"category", active_category_},  {"time_range", time_range_}, {"sort_mode", sort_mode_},
        {"view_mode", view_mode_},       {"variant", active_variant_}, {"language", active_lang_},
        {"region", active_region_},      {"my_filter", my_filter_},   {"unread_only", unread_only_},
        // One narrowing at a time: the symbol filter, else the typed search
        // (the saved format predates the context chip: search + symbol_mode).
        {"search", symbol_context_.isEmpty() ? search_query_ : symbol_context_},
        {"symbol_mode", !symbol_context_.isEmpty()},
    };
}

void NewsScreen::apply_filter_state(const QVariantMap& state) {
    active_category_ = state.value("category", "ALL").toString();
    time_range_ = state.value("time_range", "24H").toString();
    sort_mode_ = state.value("sort_mode", "NEWEST").toString();
    view_mode_ = state.value("view_mode", view_mode_).toString();
    active_variant_ = state.value("variant", "FULL").toString();
    active_lang_ = state.value("language", "ALL").toString();
    active_region_ = state.value("region", "ALL").toString();
    my_filter_ = state.value("my_filter", "ALL").toString();
    unread_only_ = state.value("unread_only", false).toBool();
    // A saved search is an explicit choice: its symbol is the context chip
    // (removable), its text the search box.
    const QString saved_query = state.value("search").toString().trimmed();
    const bool saved_symbol = state.value("symbol_mode", false).toBool() && !saved_query.isEmpty();
    search_query_ = saved_symbol ? QString() : saved_query;
    symbol_context_ = saved_symbol ? saved_query.toUpper() : QString();
    symbol_from_link_ = false;
    symbol_fallback_ = false;
    update_context_bar();
    window_from_ = window_to_ = 0;
    command_bar_->set_active_category(active_category_);
    command_bar_->set_active_time_range(time_range_);
    command_bar_->set_active_sort(sort_mode_);
    command_bar_->set_active_view(view_mode_);
    command_bar_->set_active_variant(active_variant_);
    command_bar_->set_active_language(active_lang_);
    command_bar_->set_active_region(active_region_);
    command_bar_->set_active_my(my_filter_);
    command_bar_->set_unread_only(unread_only_);
    command_bar_->set_search_text(search_query_);
    analytics_panel_->set_window(0, 0);
    feed_panel_->set_view_mode_hint(view_mode_);
    feed_panel_->model()->set_view_mode(view_mode_);
    visible_article_count_ = PAGE_SIZE;
    ScreenStateManager::instance().notify_changed(this);
    apply_filters_async();
}

void NewsScreen::load_saved_searches() {
    QPointer<NewsScreen> self = this;
    (void)QtConcurrent::run([self]() {
        auto r = fincept::NewsAnalyticsRepository::instance().list_saved_searches();
        QVector<fincept::NewsSavedSearchRow> rows;
        if (r.is_ok())
            rows = r.value();
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, rows]() {
                if (self)
                    self->saved_searches_ = rows;
            },
            Qt::QueuedConnection);
    });
}

void NewsScreen::show_saved_menu() {
    QMenu menu(this);
    if (saved_searches_.isEmpty()) {
        menu.addAction(tr("No saved searches yet"))->setEnabled(false);
    } else {
        for (const auto& row : std::as_const(saved_searches_)) {
            const QJsonObject alert = QJsonDocument::fromJson(row.alert_json.toUtf8()).object();
            const bool alerting = alert.value("enabled").toBool();
            QMenu* sub = menu.addMenu(alerting ? row.name + QStringLiteral("  ") + tr("(alert)") : row.name);
            connect(sub->addAction(tr("Apply")), &QAction::triggered, this, [this, row]() { apply_saved_search(row); });
            connect(sub->addAction(tr("Delete")), &QAction::triggered, this,
                    [this, id = row.id]() { delete_saved_search(id); });
            sub->actions().constLast()->setProperty("fincept_agent_risk", "delete"); // agents: deletes the saved search
        }
    }
    menu.addSeparator();
    connect(menu.addAction(tr("Save current search…")), &QAction::triggered, this, &NewsScreen::save_current_search);
    auto* anchor = command_bar_->saved_button();
    menu.exec(anchor && anchor->isVisible() ? anchor->mapToGlobal(QPoint(0, anchor->height())) : QCursor::pos());
}

void NewsScreen::save_current_search() {
    if (!fincept::NewsAnalyticsRepository::instance().available()) {
        QMessageBox::information(this, tr("Saved searches"),
                                 tr("Saved searches need the news analytics database update (migration v058), "
                                    "which has not been applied to this database yet."));
        return;
    }
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Save search"));
    auto* form = new QFormLayout(&dlg);
    auto* name = new QLineEdit(&dlg);
    const QString narrowing = symbol_context_.isEmpty() ? search_query_ : symbol_context_;
    name->setText(narrowing.isEmpty() ? tr("%1 · %2").arg(active_category_, time_range_) : narrowing);
    form->addRow(tr("Name"), name);

    auto* alert_on = new QCheckBox(tr("Notify me about new matching articles"), &dlg);
    form->addRow(QString(), alert_on);
    // The shared picker: the alert stores the picked instrument's market-data
    // symbol (the form the tagger writes on articles), never typed text.
    auto* symbol = new ui::SymbolSearchBox(&dlg);
    symbol->set_compact(true);
    symbol->set_clear_on_pick(false);
    symbol->set_recent_key(QStringLiteral("news_alert"));
    symbol->set_placeholder(tr("Ticker, company name or ISIN (optional)"));
    symbol->set_text(symbol_context_); // the current narrowing, already a tagged symbol
    form->addRow(tr("Ticker"), symbol);
    auto* symbol_note = new QLabel(&dlg);
    symbol_note->setWordWrap(true);
    symbol_note->hide();
    form->addRow(QString(), symbol_note);
    auto* keyword = new QLineEdit(&dlg);
    keyword->setPlaceholderText(tr("word or phrase in the headline (optional)"));
    keyword->setText(search_query_);
    form->addRow(tr("Keyword"), keyword);
    auto* tone_rule = new QComboBox(&dlg);
    tone_rule->addItem(tr("Any tone"), QString());
    tone_rule->addItem(tr("Tone at or below"), QStringLiteral("below"));
    tone_rule->addItem(tr("Tone at or above"), QStringLiteral("above"));
    auto* tone_value = new QDoubleSpinBox(&dlg);
    tone_value->setRange(-1.0, 1.0);
    tone_value->setSingleStep(0.05);
    tone_value->setValue(-0.5);
    auto* tone_row = new QWidget(&dlg);
    auto* tl = new QHBoxLayout(tone_row);
    tl->setContentsMargins(0, 0, 0, 0);
    tl->addWidget(tone_rule, 1);
    tl->addWidget(tone_value);
    form->addRow(tr("Tone"), tone_row);
    auto* volume_on = new QCheckBox(tr("Volume spike in the category, z-score at least"), &dlg);
    auto* volume_z = new QDoubleSpinBox(&dlg);
    volume_z->setRange(2.0, 10.0);
    volume_z->setSingleStep(0.5);
    volume_z->setValue(3.0);
    auto* vol_row = new QWidget(&dlg);
    auto* vl = new QHBoxLayout(vol_row);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->addWidget(volume_on, 1);
    vl->addWidget(volume_z);
    form->addRow(tr("Volume"), vol_row);
    auto* quiet_on = new QCheckBox(tr("Quiet hours (no notifications), local time"), &dlg);
    auto* quiet_from = new QSpinBox(&dlg);
    quiet_from->setRange(0, 23);
    quiet_from->setValue(22);
    quiet_from->setSuffix(QStringLiteral(":00"));
    auto* quiet_to = new QSpinBox(&dlg);
    quiet_to->setRange(0, 23);
    quiet_to->setValue(7);
    quiet_to->setSuffix(QStringLiteral(":00"));
    auto* q_row = new QWidget(&dlg);
    auto* ql = new QHBoxLayout(q_row);
    ql->setContentsMargins(0, 0, 0, 0);
    ql->addWidget(quiet_on, 1);
    ql->addWidget(quiet_from);
    ql->addWidget(new QLabel(tr("to"), q_row));
    ql->addWidget(quiet_to);
    form->addRow(tr("Quiet"), q_row);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
    QPushButton* save_btn = buttons->button(QDialogButtonBox::Save);
    // Save: a picked instrument (or the untouched current narrowing) saves at
    // once; typed text is first resolved asynchronously — an unambiguous hit is
    // saved in its canonical form, anything else asks for a pick from the list.
    connect(buttons, &QDialogButtonBox::accepted, &dlg,
            [&dlg, symbol, symbol_note, save_btn, prefilled = symbol_context_]() {
                const QString text = symbol->text().trimmed();
                if (text.isEmpty() || symbol->current_instrument().is_valid() ||
                    text.compare(prefilled, Qt::CaseInsensitive) == 0) {
                    dlg.accept();
                    return;
                }
                save_btn->setEnabled(false);
                symbol_note->setText(tr("Looking up %1…").arg(text));
                symbol_note->show();
                services::InstrumentResolver::instance().resolve(
                    text, symbol->options(), &dlg,
                    [&dlg, symbol, symbol_note, save_btn, text](const services::ResolveResult& r) {
                        if (r.exact && !r.matches.isEmpty() && services::is_quotable(r.matches.first())) {
                            symbol->set_instrument(r.matches.first());
                            dlg.accept();
                            return;
                        }
                        save_btn->setEnabled(true);
                        symbol_note->setText(
                            r.matches.isEmpty()
                                ? tr("No symbol matches \"%1\". Pick one from the list, or clear the ticker.").arg(text)
                                : tr("\"%1\" matches more than one listing. Pick one from the list.").arg(text));
                        symbol->focus_and_select();
                    });
            });
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(buttons);
    if (dlg.exec() != QDialog::Accepted || name->text().trimmed().isEmpty())
        return;

    QJsonObject alert;
    alert["enabled"] = alert_on->isChecked();
    const auto picked = symbol->current_instrument();
    alert["symbol"] = picked.is_valid() && !picked.symbol.isEmpty() ? picked.symbol
                                                                     : symbol->text().trimmed().toUpper();
    alert["keyword"] = keyword->text().trimmed();
    alert["category"] = active_category_ == QLatin1String("ALL")
                            ? QString()
                            : news_category_codes().value(active_category_, active_category_);
    if (!tone_rule->currentData().toString().isEmpty()) {
        alert["tone_rule"] = tone_rule->currentData().toString();
        alert["tone"] = tone_value->value();
    }
    if (volume_on->isChecked())
        alert["volume_z"] = volume_z->value();
    if (quiet_on->isChecked()) {
        alert["quiet_from"] = quiet_from->value();
        alert["quiet_to"] = quiet_to->value();
    }
    fincept::NewsSavedSearchRow row;
    row.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    row.name = name->text().trimmed();
    row.state_json = QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(current_filter_state())).toJson(
        QJsonDocument::Compact));
    row.alert_json = QString::fromUtf8(QJsonDocument(alert).toJson(QJsonDocument::Compact));
    saved_searches_.append(row);
    (void)QtConcurrent::run([row]() {
        auto r = fincept::NewsAnalyticsRepository::instance().save_saved_search(row);
        if (r.is_err())
            LOG_WARN("NewsScreen", "Saving a news search failed: " + QString::fromStdString(r.error()));
    });
}

void NewsScreen::apply_saved_search(const fincept::NewsSavedSearchRow& row) {
    const QJsonObject state = QJsonDocument::fromJson(row.state_json.toUtf8()).object();
    apply_filter_state(state.toVariantMap());
}

bool NewsScreen::open_section(const QString& section, const QString& focus) {
    // The WIRE / CLST pills' path (set_active_view + on_view_mode_changed) and the SAVED menu's
    // Apply (apply_saved_search). Validated before anything changes.
    const QString key = section.trimmed().toLower();
    QString mode;
    if (key == QLatin1String("wire"))
        mode = QStringLiteral("WIRE");
    else if (key == QLatin1String("clusters") || key == QLatin1String("stories"))
        mode = QStringLiteral("CLUSTERS");
    else if (!key.isEmpty())
        return false;
    QString want = focus.trimmed();
    if (want.startsWith(QLatin1String("search:"), Qt::CaseInsensitive))
        want = want.mid(7).trimmed();
    if (!want.isEmpty()) {
        for (const auto& row : std::as_const(saved_searches_))
            if (row.id == want || row.name.compare(want, Qt::CaseInsensitive) == 0) {
                apply_saved_search(row);
                break;
            }
    }
    if (!mode.isEmpty() && mode != view_mode_) {
        command_bar_->set_active_view(mode);
        on_view_mode_changed(mode);
    }
    return true;
}

void NewsScreen::delete_saved_search(const QString& id) {
    saved_searches_.erase(std::remove_if(saved_searches_.begin(), saved_searches_.end(),
                                         [&id](const auto& r) { return r.id == id; }),
                          saved_searches_.end());
    (void)QtConcurrent::run([id]() { (void)fincept::NewsAnalyticsRepository::instance().remove_saved_search(id); });
}

void NewsScreen::check_saved_alerts(const QVector<QPair<QString, double>>& deviations) {
    if (saved_searches_.isEmpty())
        return;
    using namespace fincept::notifications;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const int hour = QTime::currentTime().hour();
    for (const auto& row : std::as_const(saved_searches_)) {
        const QJsonObject alert = QJsonDocument::fromJson(row.alert_json.toUtf8()).object();
        if (!alert.value("enabled").toBool())
            continue;
        if (alert.contains("quiet_from")) {
            const int from = alert.value("quiet_from").toInt();
            const int to = alert.value("quiet_to").toInt();
            const bool quiet = from <= to ? (hour >= from && hour < to) : (hour >= from || hour < to);
            if (quiet)
                continue;
        }
        const QString symbol = alert.value("symbol").toString();
        const QString keyword = alert.value("keyword").toString();
        const QString category = alert.value("category").toString();
        const QString tone_rule = alert.value("tone_rule").toString();
        const double tone = alert.value("tone").toDouble();

        // Volume spike rule: the category (or any) spiking at the threshold.
        if (alert.contains("volume_z")) {
            const double zmin = alert.value("volume_z").toDouble(3.0);
            for (const auto& [cat, z] : deviations) {
                if (z < zmin || (!category.isEmpty() && cat != category))
                    continue;
                const QString key = row.id + QStringLiteral(":vol:") + cat + QLatin1Char(':') +
                                    QString::number(now / 3600);
                if (notified_alerts_.contains(key))
                    continue;
                notified_alerts_.insert(key);
                NotificationRequest req;
                req.title = tr("ALERT \"%1\": %2 news spike").arg(row.name, cat);
                req.message = tr("Last-hour volume is %1 standard deviations above the baseline.")
                                  .arg(z, 0, 'f', 1);
                req.level = NotifLevel::Warning;
                req.trigger = NotifTrigger::NewsAlert;
                NotificationService::instance().send(req);
            }
            if (symbol.isEmpty() && keyword.isEmpty() && tone_rule.isEmpty())
                continue; // a pure volume alert
        }

        const QString symbol_base = news_symbol_base(symbol);
        for (const auto& a : std::as_const(all_articles_)) {
            if (a.undated || now - a.sort_ts > kNewsNotifyMaxAgeSec)
                continue;
            if (!category.isEmpty() && a.category != category)
                continue;
            if (!symbol.isEmpty()) {
                bool hit = false;
                for (const auto& t : a.tickers)
                    hit = hit || t.compare(symbol, Qt::CaseInsensitive) == 0 ||
                          news_symbol_base(t).compare(symbol_base, Qt::CaseInsensitive) == 0;
                if (!hit)
                    continue;
            }
            if (!keyword.isEmpty() && !a.headline.contains(keyword, Qt::CaseInsensitive) &&
                !a.summary.contains(keyword, Qt::CaseInsensitive))
                continue;
            if (!tone_rule.isEmpty()) {
                if (!a.sentiment_scored)
                    continue; // never alert on a tone that was not measured
                if (tone_rule == QLatin1String("below") ? a.sentiment_score > tone : a.sentiment_score < tone)
                    continue;
            }
            const QString key = row.id + QLatin1Char(':') + a.id;
            if (notified_alerts_.contains(key))
                continue;
            if (notified_alerts_.size() > kNewsNotifiedSetCap)
                notified_alerts_.clear();
            notified_alerts_.insert(key);
            NotificationRequest req;
            req.title = tr("ALERT \"%1\": %2").arg(row.name, a.headline.left(70));
            req.message = a.sentiment_scored
                              ? tr("%1 · tone %2").arg(a.source, QString::number(a.sentiment_score, 'f', 2))
                              : a.source;
            req.level = NotifLevel::Warning;
            req.trigger = NotifTrigger::NewsAlert;
            NotificationService::instance().send(req);
        }
    }
}

// ── IStatefulScreen ─────────────────────────────────────────────────────────

QVariantMap NewsScreen::save_state() const {
    // search_query_ is intentionally NOT persisted — it's a transient typed-in
    // filter, and restoring it would silently filter the feed on next launch.
    return {
        {"category", active_category_}, {"time_range", time_range_},  {"sort_mode", sort_mode_},
        {"view_mode", view_mode_},      {"variant", active_variant_}, {"language", active_lang_},
        {"region", active_region_},     {"my_filter", my_filter_},    {"unread_only", unread_only_},
    };
}

void NewsScreen::restore_state(const QVariantMap& state) {
    active_category_ = state.value("category", "ALL").toString();
    time_range_ = state.value("time_range", "24H").toString();
    // A workspace saved by an older build carries its RELEVANCE default; the
    // one-time switch to NEWEST (constructor) wins over it.
    if (!sort_migrated_)
        sort_mode_ = state.value("sort_mode", "NEWEST").toString();
    view_mode_ = state.value("view_mode", "WIRE").toString();
    // Drop any legacy "search_query" stored by an older build — see save_state().
    // The symbol filter is never saved either (save_state), so a symbol pushed by
    // a linked panel cannot come back as a narrowed feed on the next start.
    search_query_.clear();
    active_variant_ = state.value("variant", active_variant_).toString();
    active_lang_ = state.value("language", active_lang_).toString();
    active_region_ = state.value("region", active_region_).toString();
    my_filter_ = state.value("my_filter", my_filter_).toString();
    unread_only_ = state.value("unread_only", unread_only_).toBool();

    if (command_bar_) {
        command_bar_->set_active_category(active_category_);
        command_bar_->set_active_time_range(time_range_);
        command_bar_->set_active_sort(sort_mode_);
        command_bar_->set_active_view(view_mode_);
        // The variant/language filters apply to the feed, so the combos must show
        // them — restoring only the member left a hidden filter behind.
        command_bar_->set_active_variant(active_variant_);
        command_bar_->set_active_language(active_lang_);
        command_bar_->set_active_region(active_region_);
        command_bar_->set_active_my(my_filter_);
        command_bar_->set_unread_only(unread_only_);
    }
    if (feed_panel_)
        feed_panel_->set_view_mode_hint(view_mode_);
}

} // namespace fincept::screens
