#include "screens/dashboard/widgets/BaseWidget.h"

#include "core/agent/AgentRiskMarkers.h"
#include "core/events/EventBus.h"
#include "core/symbol/SymbolContext.h"
#include "core/symbol/SymbolGroupRegistry.h"
#include "datahub/DataHub.h"
#include "screens/dashboard/widgets/LoadingOverlay.h"
#include "services/backtesting/BacktestingService.h"
#include "storage/repositories/WatchlistRepository.h"
#include "trading/AccountManager.h"
#include "ui/widgets/ThemeIcon.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/GroupBadge.h"

#include <QAbstractButton>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QCursor>
#include <QFontMetrics>
#include <QJsonArray>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QSize>
#include <QStyle>
#include <QTableWidget>
#include <QToolTip>

#include <algorithm>
#include <iterator>

namespace fincept::screens::widgets {

namespace {

// Dynamic-property name used by link_symbol() to tag a clickable row/cell.
constexpr const char* kBaseWidgetSymbolProp = "fincept_link_symbol";
// ...and by link_screen() to tag a row that opens a whole screen.
constexpr const char* kBaseWidgetScreenProp = "fincept_link_screen";
// ...and by set_css_cached() to remember the last stylesheet it applied.
constexpr const char* kBaseWidgetCssProp = "fincept_cached_css";

// Index levels (^GSPC), FX/futures (EURUSD=X, GC=F) and Yahoo crypto pairs
// (BTC-USD) are not broker-tradable equities — same rule as the Watchlist menu.
bool base_widget_is_tradable_equity(const QString& symbol) {
    return !symbol.startsWith(QLatin1Char('^')) && !symbol.contains(QLatin1Char('=')) &&
           !symbol.endsWith(QLatin1String("-USD"));
}

// Short name for a hub topic in error text: "market:quote:AAPL" -> "AAPL",
// "market:history:AAPL:1y:1d" -> "AAPL". Other families keep their full name.
QString base_widget_topic_label(const QString& topic) {
    static const QString kMarketPrefix = QStringLiteral("market:");
    if (!topic.startsWith(kMarketPrefix))
        return topic;
    const int kind_end = topic.indexOf(QLatin1Char(':'), kMarketPrefix.size());
    if (kind_end < 0)
        return topic;
    const QString rest = topic.mid(kind_end + 1);
    const int sym_end = rest.indexOf(QLatin1Char(':'));
    return sym_end < 0 ? rest : rest.left(sym_end);
}

// Producer reasons are user-facing sentences; only bound their length so a
// stack-trace-sized message cannot swamp a tile.
QString base_widget_clip_reason(const QString& error) {
    constexpr int kMaxReasonChars = 280;
    const QString e = error.trimmed();
    return e.size() > kMaxReasonChars ? e.left(kMaxReasonChars) + QChar(0x2026) : e;
}

// One place for the three title-bar button stylesheets. The constructor used to
// style config/refresh/close inline and refresh_base_theme() only restyled the
// refresh button, so after a theme switch the gear and close buttons kept the
// previous theme's colours.
void apply_title_button_styles(QPushButton* config_btn, QPushButton* refresh_btn, QPushButton* close_btn,
                               const QString& accent) {
    const auto css = [](const QString& fg, const QString& hover) {
        return QString("QPushButton { color: %1; background: %2; border: 1px solid %3; border-radius: 2px; "
                       "padding: 0px; }"
                       "QPushButton:hover { color: %4; border-color: %4; background: %5; }")
            .arg(fg, ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), hover, ui::colors::BG_HOVER());
    };
    // Only on change: re-applied after every appearance change (refresh_base_theme).
    const auto put = [](QPushButton* b, const QString& sheet) {
        if (b && b->styleSheet() != sheet)
            b->setStyleSheet(sheet);
    };
    put(config_btn, css(accent, ui::colors::TEXT_PRIMARY()));
    put(refresh_btn, css(accent, ui::colors::TEXT_PRIMARY()));
    put(close_btn, css(ui::colors::TEXT_TERTIARY(), ui::colors::DANGER()));
}

// The theme token whose current value is `color`, so an accent handed over as
// a token value (ui::colors::CYAN() …) follows later theme changes. Null when
// `color` is not one of these tokens.
const char* ui::ThemeTokens::* base_widget_accent_field(const QString& color) {
    using Field = const char* ui::ThemeTokens::*;
    constexpr Field kFields[] = {
        &ui::ThemeTokens::accent,  &ui::ThemeTokens::cyan,           &ui::ThemeTokens::info,
        &ui::ThemeTokens::success, &ui::ThemeTokens::danger,         &ui::ThemeTokens::warning,
        &ui::ThemeTokens::positive, &ui::ThemeTokens::negative,      &ui::ThemeTokens::text_primary,
        &ui::ThemeTokens::text_secondary,
    };
    const auto& t = ui::ThemeManager::instance().tokens();
    for (Field f : kFields) {
        if (color.compare(QLatin1String(t.*f), Qt::CaseInsensitive) == 0)
            return f;
    }
    return nullptr;
}

// Host of a tile's designed empty state (BaseWidget::show_empty_state): covers
// the content area exactly — it filters its parent's resizes. It paints nothing
// itself: the EmptyState inside fills it and is opaque (EmptyState::set_opaque,
// the tile surface token read from the live theme), so nothing of the (empty)
// table or chart underneath shows through and no stylesheet is needed.
class BaseWidgetEmptyCover : public QWidget {
  public:
    explicit BaseWidgetEmptyCover(QWidget* target) : QWidget(target) {
        setObjectName(QStringLiteral("dashboardTileEmptyCover"));
        auto* vl = new QVBoxLayout(this);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(0);
        target->installEventFilter(this);
        setGeometry(target->rect());
    }

  protected:
    bool eventFilter(QObject* obj, QEvent* e) override {
        if (obj == parentWidget() && e->type() == QEvent::Resize)
            setGeometry(parentWidget()->rect());
        return QWidget::eventFilter(obj, e);
    }
};

} // namespace

BaseWidget::BaseWidget(const QString& title, QWidget* parent, const QString& accent_color)
    : QFrame(parent), accent_color_(accent_color.isEmpty() ? ui::colors::AMBER : accent_color) {
    accent_source_ = accent_color_;
    accent_field_ = base_widget_accent_field(accent_color_);
    if (!accent_field_)
        accent_color_ = ui::colors::legible(QColor(accent_source_));
    setObjectName("dashboardBaseWidget");
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(true);

    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(ui::colors::BG_SURFACE()));
    setPalette(pal);

    setStyleSheet(QString("#dashboardBaseWidget{background:%1;border:1px solid %2;border-radius:2px;}")
                      .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_BRIGHT()));

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Title bar ──
    title_bar_ = new QWidget(this);
    title_bar_->setFixedHeight(26);
    title_bar_->setStyleSheet(
        QString("background: %1; border-bottom: 1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));

    auto* hl = new QHBoxLayout(title_bar_);
    hl->setContentsMargins(8, 0, 4, 0);
    hl->setSpacing(6);

    // Accent bar
    accent_bar_ = new QLabel;
    accent_bar_->setFixedSize(2, 10);
    accent_bar_->setStyleSheet(QString("background: %1; border-radius: 1px;").arg(accent_color_));
    hl->addWidget(accent_bar_);

    title_label_ = new QLabel(title);
    title_label_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: bold; letter-spacing: 0.5px; "
                                        "background: transparent; text-transform: uppercase;")
                                    .arg(accent_color_));
    // The title is the one part of the header that gives way. A label is never
    // laid out narrower than its text unless it has a minimum width of its
    // own, so in a narrow tile at a large font the row was wider than the tile
    // and the title ran into the "1/11 FAILED" marker beside it. Now the end of
    // the title is cut instead (the tooltip has all of it) and the marker and
    // the buttons keep their place.
    title_label_->setMinimumWidth(1);
    title_label_->setToolTip(title);
    hl->addWidget(title_label_);

    // Loading indicator
    loading_label_ = new QLabel("...");
    loading_label_->setStyleSheet(
        QString("color: %1; font-size: " + fincept::ui::fonts::px(-5) + "px; background: transparent;").arg(ui::colors::AMBER()));
    loading_label_->setVisible(false);
    hl->addWidget(loading_label_);

    // Producer-error marker for tracked topics (see track_topics()): shown only
    // while the tile has data AND some tracked topic's last refresh failed.
    topic_status_label_ = new QLabel;
    topic_status_label_->setStyleSheet(
        QString("color: %1; font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: bold; background: transparent;").arg(ui::colors::WARNING()));
    topic_status_label_->setVisible(false);
    hl->addWidget(topic_status_label_);

    hl->addStretch();

    // Config (gear) button — hidden by default until subclass calls set_configurable(true)
    config_btn_ = new QPushButton;
    config_btn_->setFixedSize(20, 20);
    config_btn_->setText("");
    config_btn_->setIcon(ui::theme_icon(ui::ThemeGlyph::Settings));
    config_btn_->setIconSize(QSize(12, 12));
    config_btn_->setToolTip(tr("Configure widget"));
    config_btn_->setAccessibleName(tr("Configure widget"));
    config_btn_->setCursor(Qt::PointingHandCursor);
    config_btn_->setVisible(false);
    connect(config_btn_, &QPushButton::clicked, this, &BaseWidget::on_config_clicked);
    hl->addWidget(config_btn_);

    // Refresh button
    refresh_btn_ = new QPushButton;
    refresh_btn_->setFixedSize(20, 20);
    refresh_btn_->setText("");
    refresh_btn_->setIcon(ui::theme_icon(ui::ThemeGlyph::Refresh));
    refresh_btn_->setIconSize(QSize(12, 12));
    refresh_btn_->setToolTip(tr("Refresh widget data"));
    refresh_btn_->setAccessibleName(tr("Refresh widget data"));
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    connect(refresh_btn_, &QPushButton::clicked, this, &BaseWidget::refresh_requested);
    hl->addWidget(refresh_btn_);

    // Close button
    auto* close_btn = new QPushButton;
    close_btn_ = close_btn;
    close_btn->setFixedSize(20, 20);
    close_btn->setText("");
    close_btn->setIcon(ui::theme_icon(ui::ThemeGlyph::Close));
    close_btn->setIconSize(QSize(11, 11));
    close_btn->setToolTip(tr("Close widget"));
    close_btn->setAccessibleName(tr("Close widget"));
    close_btn->setCursor(Qt::PointingHandCursor);
    connect(close_btn, &QPushButton::clicked, this, &BaseWidget::close_requested);
    hl->addWidget(close_btn);

    apply_title_button_styles(config_btn_, refresh_btn_, close_btn_, accent_color_);

    vl->addWidget(title_bar_);

    // Auto-refresh styles on theme/font change — subclasses get this for free.
    // Once the appearance has settled, never inside theme_changed: a dashboard
    // holds dozens of tiles, and their restyles / re-renders / row-height fits
    // there stalled every theme switch. Stylesheets go through set_css_cached(),
    // which skips a sheet the theme manager has already translated.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        refresh_base_theme();
        on_theme_changed();
    });

    // ── Content ──
    content_ = new QWidget(this);
    content_->setStyleSheet("background: transparent;");
    content_layout_ = new QVBoxLayout(content_);
    content_layout_->setContentsMargins(0, 0, 0, 0);
    content_layout_->setSpacing(0);

    vl->addWidget(content_, 1);

    // ── Loading overlay ──
    // Lives as a child of the content area so it covers exactly the data
    // region (not the title bar). Sized via an event filter on `content_`.
    loading_overlay_ = new LoadingOverlay(content_);
    loading_overlay_->attach_to(content_);

    // ── Loading watchdog ──
    // Single-shot guard against producers that never publish. If the
    // overlay is still active 20 s after loading started with zero items
    // delivered, swap to a terminal "no data" overlay so the spinner can't
    // spin forever. Per-widget refresh still works to re-arm.
    //
    // 20 s (not 12 s) because dashboard restore fans out 20+ subscribe()
    // calls in one tick; producer-side rate limits then serialise the
    // cold-start fetches and tail widgets routinely take >12 s to see
    // their first payload even on healthy networks.
    loading_watchdog_ = new QTimer(this);
    loading_watchdog_->setSingleShot(true);
    loading_watchdog_->setInterval(20000);
    connect(loading_watchdog_, &QTimer::timeout, this, &BaseWidget::on_watchdog_fired);

    // Retry from a producer-error overlay: back to the spinner while the
    // subclass's refresh handler (connected later, so it runs after this)
    // re-requests. The recorded errors stay — the reason comes back with the
    // next error report, or at the latest when the watchdog fires (the hub may
    // rate-limit / back off the retry, so a fresh report is not guaranteed).
    connect(this, &BaseWidget::refresh_requested, this, [this]() {
        if (topic_error_shown_ && isVisible()) {
            set_loading(true);
            topic_retry_pending_ = true;
        }
    });
}

void BaseWidget::refresh_base_theme() {
    // The accent was resolved for the previous theme; re-read it.
    accent_color_ = accent_field_ ? QString::fromLatin1(ui::ThemeManager::instance().tokens().*accent_field_)
                                  : ui::colors::legible(QColor(accent_source_));

    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(ui::colors::BG_SURFACE()));
    setPalette(pal);

    // Only when the text differs (set_css_cached): setStyleSheet() re-styles the
    // whole tile even for an identical sheet, and after a theme or font change the
    // manager has already rewritten these sheets to the new values (a font step
    // does not touch them at all).
    set_css_cached(this, QString("#dashboardBaseWidget{background:%1;border:1px solid %2;border-radius:2px;}")
                             .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_BRIGHT()));
    set_css_cached(title_bar_, QString("background:%1;border-bottom:1px solid %2;")
                                   .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    set_css_cached(title_label_,
                   QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:bold;letter-spacing:0.5px;"
                           "background:transparent;text-transform:uppercase;")
                       .arg(accent_color_));
    set_css_cached(accent_bar_, QString("background:%1;border-radius:1px;").arg(accent_color_));
    set_css_cached(loading_label_,
                   QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;background:transparent;").arg(ui::colors::AMBER()));
    set_css_cached(topic_status_label_,
                   QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:bold;background:transparent;")
                       .arg(ui::colors::WARNING()));
    apply_title_button_styles(config_btn_, refresh_btn_, close_btn_, accent_color_);
}

void BaseWidget::set_loading(bool loading) {
    loading_label_->setVisible(loading);
    loading_label_->setText(loading ? tr("LOADING...") : QString());
    if (!loading_overlay_)
        return;
    error_shown_ = false;
    topic_error_shown_ = false;
    if (loading) {
        loading_overlay_->start_indeterminate();
        last_progress_loaded_ = 0;
        last_progress_expected_ = 0;
        arm_watchdog();
    } else {
        loading_overlay_->finish();
        disarm_watchdog();
    }
}

void BaseWidget::set_loading_progress(int loaded, int expected) {
    if (!loading_overlay_)
        return;
    const bool active = (expected > 0) && (loaded < expected);

    // Something was delivered and the overlay is already down (the watchdog
    // dismissed it, or the subclass called set_loading(false)): a partial
    // count must not raise it again over the delivered content. This was the
    // "table hidden two thirds of the time" loop — every delivery re-armed a
    // near-opaque overlay for one missing symbol until the watchdog fired.
    if (active && loaded > 0 && !loading_overlay_->is_active()) {
        last_progress_loaded_ = loaded;
        last_progress_expected_ = expected;
        return;
    }

    const bool progressed = loaded != last_progress_loaded_ || expected != last_progress_expected_;
    error_shown_ = false;
    topic_error_shown_ = false;
    loading_label_->setVisible(active);
    loading_label_->setText(active ? tr("LOADING...") : QString());
    loading_overlay_->set_progress(loaded, expected);
    last_progress_loaded_ = loaded;
    last_progress_expected_ = expected;
    // The watchdog measures time without progress: a repeat of the same count
    // (re-delivery of an already-counted item) must not restart it.
    if (active) {
        if (progressed || !loading_watchdog_ || !loading_watchdog_->isActive())
            arm_watchdog();
    } else {
        disarm_watchdog();
    }
}

void BaseWidget::set_error(const QString& error) {
    if (error.isEmpty())
        return;

    // Error overrides any in-flight progress. The overlay draws the message
    // over the (intact) content; the next set_loading(true) — e.g. the title
    // bar refresh — switches it back to the loading state.
    if (loading_label_) {
        loading_label_->setVisible(false);
        loading_label_->setText(QString());
    }
    disarm_watchdog();
    error_shown_ = true;
    topic_error_shown_ = false; // show_topic_failure() sets it back after this call
    if (loading_overlay_)
        loading_overlay_->set_error(error);
}

// ── Producer-error tracking ──────────────────────────────────────────────────

void BaseWidget::track_topics(const QStringList& topics, bool drive_loading_overlay) {
    auto& hub = datahub::DataHub::instance();

    QStringList next;
    next.reserve(topics.size());
    QSet<QString> next_set;
    for (const QString& t : topics) {
        if (!t.isEmpty() && !next_set.contains(t)) {
            next.append(t);
            next_set.insert(t);
        }
    }

    // Exactly one error slot per topic, however often this runs: drop ours for
    // every topic tracked so far and every topic about to be tracked first
    // (unsubscribe_errors() is a no-op for a missing entry). A subclass's own
    // DataHub::unsubscribe(this) in between has already removed them — fine.
    for (const QString& t : std::as_const(tracked_topics_))
        hub.unsubscribe_errors(this, t);
    for (const QString& t : std::as_const(next))
        if (!tracked_set_.contains(t))
            hub.unsubscribe_errors(this, t);

    // Topics still listed keep their state (an error persists until a value
    // arrives, also across hide/show); the rest are forgotten.
    for (auto it = topic_errors_.begin(); it != topic_errors_.end();)
        it = next_set.contains(it.key()) ? std::next(it) : topic_errors_.erase(it);
    for (auto it = valued_topics_.begin(); it != valued_topics_.end();)
        it = next_set.contains(*it) ? std::next(it) : valued_topics_.erase(it);

    tracked_topics_ = next;
    tracked_set_ = next_set;
    topic_drive_loading_ = drive_loading_overlay;

    // The hub may hand a late subscriber the topic's current error straight
    // from subscribe_errors(): record those, evaluate once at the end.
    topic_tracking_quiet_ = true;
    for (const QString& t : std::as_const(next))
        hub.subscribe_errors(this, t, [this, t](const QString& error) { on_tracked_topic_error(t, error); });
    topic_tracking_quiet_ = false;
    refresh_topic_state();
}

void BaseWidget::untrack_topics() {
    auto& hub = datahub::DataHub::instance();
    for (const QString& t : std::as_const(tracked_topics_))
        hub.unsubscribe_errors(this, t);
    tracked_topics_.clear();
    tracked_set_.clear();
    topic_errors_.clear();
    valued_topics_.clear();
    topic_failure_resolved_ = topic_failure_expected_ = -1;
    topic_retry_pending_ = false;
    update_topic_status_label();
    // A failure about topics nobody tracks any more must not stay up; the
    // subclass re-arms its own loading state for whatever comes next.
    if (topic_error_shown_)
        set_loading(true);
}

void BaseWidget::note_topic_value(const QString& topic) {
    const bool had_error = topic_errors_.remove(topic) > 0;
    const bool first_value = !valued_topics_.contains(topic);
    valued_topics_.insert(topic); // pruned to the tracked set by the next track_topics()
    if (topic_tracking_quiet_)
        return;
    topic_retry_pending_ = false;
    if (had_error || first_value || topic_error_shown_)
        refresh_topic_state();
}

void BaseWidget::note_topic_error(const QString& topic, const QString& error) {
    on_tracked_topic_error(topic, error);
}

void BaseWidget::on_tracked_topic_error(const QString& topic, const QString& error) {
    if (!tracked_set_.contains(topic))
        return;
    QString reason = base_widget_clip_reason(error);
    if (reason.isEmpty())
        reason = tr("The data source reported an error.");
    const bool changed = topic_errors_.value(topic) != reason;
    topic_errors_.insert(topic, reason);
    if (changed)
        on_topic_errors_changed();
    // An error handed over while (re)subscribing is the hub's last known one,
    // not the outcome of a retry; anything after that is.
    if (!topic_tracking_quiet_) {
        topic_retry_pending_ = false;
        refresh_topic_state();
    }
}

int BaseWidget::failed_topic_count() const {
    int n = 0;
    for (auto it = topic_errors_.constBegin(); it != topic_errors_.constEnd(); ++it)
        if (tracked_set_.contains(it.key()))
            ++n;
    return n;
}

QString BaseWidget::topic_error_tooltip(const QString& topic) const {
    const QString err = topic_errors_.value(topic);
    if (err.isEmpty())
        return {};
    return valued_topics_.contains(topic)
               ? tr("Last refresh failed \xe2\x80\x94 showing the last data received.\n%1").arg(err)
               : tr("No data: %1").arg(err);
}

QString BaseWidget::topic_error_lines(int max_groups) const {
    QStringList reasons; // first-seen order, following the declared topic order
    QHash<QString, QStringList> labels_by_reason;
    for (const QString& t : tracked_topics_) {
        const QString e = topic_errors_.value(t);
        if (e.isEmpty())
            continue;
        if (!labels_by_reason.contains(e))
            reasons.append(e);
        labels_by_reason[e].append(base_widget_topic_label(t));
    }
    if (reasons.isEmpty())
        return {};
    // One tile-wide reason (single topic, or everything failed the same way —
    // "Python is not set up yet", offline, rate limited): say it once.
    const int failed = failed_topic_count();
    if (reasons.size() == 1 && (tracked_topics_.size() == 1 || failed == tracked_topics_.size()))
        return reasons.first();

    QStringList lines;
    const int shown = std::min(max_groups, static_cast<int>(reasons.size()));
    for (int i = 0; i < shown; ++i) {
        const QStringList& labels = labels_by_reason.value(reasons[i]);
        QString who = labels.mid(0, 4).join(QStringLiteral(", "));
        if (labels.size() > 4)
            who += tr(" +%1").arg(labels.size() - 4);
        lines.append(who + QStringLiteral(": ") + reasons[i]);
    }
    if (reasons.size() > shown)
        lines.append(tr("\xe2\x80\xa6" "and %1 more").arg(reasons.size() - shown));
    return lines.join(QLatin1Char('\n'));
}

void BaseWidget::show_topic_failure() {
    const int expected = static_cast<int>(tracked_topics_.size());
    const int failed = failed_topic_count();
    if (failed == 0 || topic_retry_pending_)
        return; // retrying: the spinner stays until a new outcome or the watchdog
    QString msg;
    if (failed < expected)
        msg = tr("No data yet \xe2\x80\x94 %1 of %2 failed:").arg(failed).arg(expected) + QLatin1Char('\n');
    msg += topic_error_lines(3);
    msg += QStringLiteral("\n\n") + tr("Click refresh to retry.");
    set_error(msg);
    topic_error_shown_ = true;
    topic_failure_resolved_ = static_cast<int>(valued_topics_.size()) + failed;
    topic_failure_expected_ = expected;
}

void BaseWidget::update_topic_loading() {
    const int expected = static_cast<int>(tracked_topics_.size());
    if (expected == 0) {
        // Nothing tracked (an empty symbol list, e.g. an empty saved watchlist):
        // nothing will ever arrive, so nothing may keep spinning — it used to sit
        // there for 20 s and end in "No data yet", with a refresh that did nothing.
        // The tile says why it is empty (its own empty state).
        if (topic_error_shown_ || loading_overlay_->is_active() || !loading_label_->isHidden())
            set_loading(false);
        return;
    }
    if (has_topic_data()) {
        // Never cover delivered data: rows that are still pending show their
        // own placeholder, failed ones their reason, the title its marker.
        if (topic_error_shown_ || loading_overlay_->is_active() || !loading_label_->isHidden())
            set_loading(false);
        return;
    }
    const int failed = failed_topic_count(); // no data, so resolved == failed
    if (failed >= expected) {
        show_topic_failure();
        return;
    }
    if (topic_error_shown_ && failed == topic_failure_resolved_ && expected == topic_failure_expected_) {
        show_topic_failure(); // same picture (watchdog put it up): refresh the text, don't re-spin
        return;
    }
    // Nothing to show and nothing covering the tile: raise the spinner first
    // (set_loading_progress() deliberately never re-raises for a partial count).
    if (!loading_overlay_->is_active())
        set_loading(true);
    if (topic_error_shown_ || failed != last_progress_loaded_ || expected != last_progress_expected_)
        set_loading_progress(failed, expected);
}

void BaseWidget::refresh_topic_state() {
    if (topic_tracking_quiet_)
        return;
    if (topic_drive_loading_) {
        update_topic_loading();
    } else if (!tracked_topics_.isEmpty()) {
        const int expected = static_cast<int>(tracked_topics_.size());
        const int failed = failed_topic_count();
        if (!has_topic_data()) {
            if (failed >= expected)
                show_topic_failure();
            else if (topic_error_shown_ && !(failed == topic_failure_resolved_ && expected == topic_failure_expected_))
                set_loading(true); // the tracked set changed under the failure: back to waiting
        } else if (topic_error_shown_) {
            set_loading(false);
        }
    }
    update_topic_status_label();
}

void BaseWidget::update_topic_status_label() {
    if (!topic_status_label_)
        return;
    const int failed = failed_topic_count();
    if (failed == 0 || !has_topic_data()) {
        topic_status_label_->setVisible(false);
        topic_status_label_->setToolTip(QString());
        return;
    }
    int stale = 0; // failed but still showing an older value
    for (auto it = topic_errors_.constBegin(); it != topic_errors_.constEnd(); ++it)
        if (tracked_set_.contains(it.key()) && valued_topics_.contains(it.key()))
            ++stale;
    const int expected = static_cast<int>(tracked_topics_.size());
    topic_status_label_->setText(stale == failed ? tr("STALE") : tr("%1/%2 FAILED").arg(failed).arg(expected));
    const QString header = stale == failed
                               ? tr("Last refresh failed \xe2\x80\x94 showing the last data received.")
                               : tr("Some items could not be loaded.");
    topic_status_label_->setToolTip(header + QLatin1Char('\n') + topic_error_lines(8) + QLatin1Char('\n') +
                                    tr("Click refresh to retry."));
    topic_status_label_->setVisible(true);
}

bool BaseWidget::is_waiting_for_first_data() const {
    return loading_overlay_ && loading_overlay_->is_active() && !error_shown_ && last_progress_loaded_ == 0;
}

void BaseWidget::set_title(const QString& title) {
    title_label_->setText(title);
    title_label_->setToolTip(title); // the header cuts a long title in a narrow tile
    // Screen readers announce the tile by its title; without this the whole
    // dashboard reads as a wall of unnamed frames.
    setAccessibleName(title);
    title_label_->setAccessibleName(title);
}

void BaseWidget::set_configurable(bool configurable) {
    if (config_btn_)
        config_btn_->setVisible(configurable);
}

void BaseWidget::set_refreshable(bool refreshable) {
    if (refresh_btn_)
        refresh_btn_->setVisible(refreshable);
}

void BaseWidget::set_agent_risk(const char* kind) {
    agent_risk_ = kind ? QString::fromLatin1(kind) : QString();
    // Markers are per control: the tile's CONTENT carries the kind AND inherit=true, so every
    // unmarked control inside it (row links, filters, LOAD, Enter in its fields) is covered too.
    // Not the whole tile: the title bar's close / configure buttons spend nothing; its refresh
    // button is marked on its own.
    ui::EmptyState::mark_agent_risk(content_, agent_risk_);
    content_->setProperty(fincept::core::agent::kAgentRiskInheritProperty,
                          agent_risk_.isEmpty() ? QVariant() : QVariant(true));
    ui::EmptyState::mark_agent_risk(refresh_btn_, agent_risk_);
    // The empty state is built on first use: show_empty_state() hands it the kind then.
    if (empty_state_)
        empty_state_->set_primary_agent_risk_default(agent_risk_);
}

void BaseWidget::open_config() {
    on_config_clicked();
}

// ── Designed empty state ─────────────────────────────────────────────────────

bool BaseWidget::is_empty_state_shown() const {
    return empty_cover_ && !empty_cover_->isHidden();
}

void BaseWidget::show_empty_state(const ui::EmptyState::Spec& spec, std::function<void()> on_primary,
                                  std::function<void()> on_secondary) {
    if (!empty_cover_) {
        empty_cover_ = new BaseWidgetEmptyCover(content_);
        empty_state_ = new ui::EmptyState(empty_cover_);
        empty_state_->set_compact(true);
        // Over the tile's live content: paints the tile surface behind itself.
        empty_state_->set_opaque(true, ui::EmptyState::Backdrop::Surface);
        // A billed tile's LOAD spends credits (set_agent_risk); kept across apply().
        if (!agent_risk_.isEmpty())
            empty_state_->set_primary_agent_risk_default(agent_risk_);
        empty_cover_->layout()->addWidget(empty_state_);
        // Copies: a handler may itself call show_empty_state() / hide_empty_state(),
        // which replaces the stored function while it runs.
        connect(empty_state_, &ui::EmptyState::primary_clicked, this, [this]() {
            const auto fn = empty_primary_;
            if (fn)
                fn();
        });
        connect(empty_state_, &ui::EmptyState::secondary_clicked, this, [this]() {
            const auto fn = empty_secondary_;
            if (fn)
                fn();
        });
    }
    empty_primary_ = std::move(on_primary);
    empty_secondary_ = std::move(on_secondary);
    empty_state_->apply(spec);
    // Nothing is loading while the tile explains why it is empty: no spinner,
    // no watchdog turning it into "No data yet" 20 s later.
    set_loading(false);
    empty_cover_->setGeometry(content_->rect());
    empty_cover_->raise();
    empty_cover_->show();
}

void BaseWidget::hide_empty_state() {
    broker_wait_ = nullptr;
    if (!empty_cover_ || empty_cover_->isHidden())
        return;
    empty_cover_->hide();
}

void BaseWidget::show_no_broker_account(const QString& what_it_shows, std::function<void()> on_account_available) {
    ui::EmptyState::Spec spec = ui::EmptyState::no_data_spec(
        tr("No broker account connected"),
        what_it_shows + QLatin1Char(' ') +
            tr("Connect an account in Equity Trading — this tile fills in as soon as one is active."),
        tr("OPEN EQUITY TRADING"));
    spec.glyph = QStringLiteral("⇄"); // ⇄
    spec.glyph_tone = ui::EmptyState::Tone::Info;
    show_empty_state(spec, []() {
        EventBus::instance().publish("nav.switch_screen",
                                     QVariantMap{{"screen_id", QStringLiteral("equity_trading")}, {"exclusive", true}});
    });
    broker_wait_ = std::move(on_account_available);
    if (!broker_watch_connected_) {
        broker_watch_connected_ = true;
        auto& am = trading::AccountManager::instance();
        connect(&am, &trading::AccountManager::account_added, this, [this](const QString&) { on_broker_accounts_changed(); });
        connect(&am, &trading::AccountManager::account_updated, this,
                [this](const QString&) { on_broker_accounts_changed(); });
    }
}

void BaseWidget::on_broker_accounts_changed() {
    if (!broker_wait_ || !is_empty_state_shown())
        return;
    if (trading::AccountManager::instance().active_accounts().isEmpty())
        return;
    // Once: the tile re-resolves its account (and hides this state itself).
    const auto fn = std::move(broker_wait_);
    broker_wait_ = nullptr;
    fn();
}

void BaseWidget::confirm_async(QMessageBox::Icon icon, const QString& title, const QString& text,
                               std::function<void()> on_yes) {
    auto* box = new QMessageBox(icon, title, text, QMessageBox::Yes | QMessageBox::No, this);
    box->setDefaultButton(QMessageBox::No);
    box->setAttribute(Qt::WA_DeleteOnClose);
    // buttonClicked fires only for a real click / Escape (the No button) — never
    // while the box is being destroyed with this tile.
    connect(box, &QMessageBox::buttonClicked, this, [box, on_yes = std::move(on_yes)](QAbstractButton* button) {
        if (box->standardButton(button) == QMessageBox::Yes && on_yes)
            on_yes();
    });
    box->open();
}

void BaseWidget::notify_async(QMessageBox::Icon icon, const QString& title, const QString& text) {
    auto* box = new QMessageBox(icon, title, text, QMessageBox::Ok, this);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->open();
}

QString BaseWidget::broker_account_hint(const QString& account_id, const QString& topic) {
    if (account_id.isEmpty())
        return {};
    const auto acct = trading::AccountManager::instance().get_account(account_id);
    const QString name = acct.display_name.isEmpty() ? account_id : acct.display_name;
    // Paper accounts trade on the local engine whatever the broker session says.
    if (acct.trading_mode == QLatin1String("paper"))
        return name;
    const auto state = trading::AccountManager::instance().connection_state(account_id);
    QString tag;
    QString reason;
    if (state == trading::ConnectionState::TokenExpired) {
        tag = tr("TOKEN EXPIRED \xe2\x80\x94 reconnect in Equity Trading");
        reason = tr("The broker session has expired. Reconnect the account in Equity Trading.");
    } else if (state == trading::ConnectionState::Error) {
        tag = tr("CONNECTION ERROR \xe2\x80\x94 reconnect in Equity Trading");
        reason = acct.error_message.trimmed().isEmpty()
                     ? tr("The broker connection failed. Reconnect the account in Equity Trading.")
                     : tr("The broker connection failed: %1. Reconnect the account in Equity Trading.")
                           .arg(acct.error_message.trimmed());
    }
    if (tag.isEmpty())
        return name;
    if (!topic.isEmpty())
        note_topic_error(topic, reason);
    return name + QStringLiteral(" \xc2\xb7 ") + tag;
}

void BaseWidget::schedule_render(std::function<void()> fn) {
    pending_render_ = std::move(fn);
    if (render_scheduled_)
        return;
    render_scheduled_ = true;
    // `this` as context object: Qt drops the pending call if the widget dies.
    QTimer::singleShot(0, this, [this]() {
        render_scheduled_ = false;
        // Move out first so a render that itself schedules another one
        // doesn't get clobbered.
        auto fn = std::move(pending_render_);
        pending_render_ = nullptr;
        if (fn)
            fn();
    });
}

void BaseWidget::on_config_clicked() {
    QDialog* dlg = make_config_dialog(this);
    if (!dlg)
        return;
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    // Subclasses emit config_changed() inside the dialog's accept flow, so
    // nothing here waits for the result: open() is window-modal without the
    // nested event loop exec() ran on the GUI thread (P1).
    dlg->open();
}

void BaseWidget::showEvent(QShowEvent* event) {
    QFrame::showEvent(event);
    // Subclasses commonly call set_loading(true) from their constructor,
    // which arms the watchdog before the widget is mounted. During
    // dashboard layout restore that gap is small but non-zero, and any
    // widget that's constructed but not immediately shown (collapsed
    // workspace, off-screen tile) would otherwise burn its 20 s budget
    // before the subscription is even dispatched. Restart the timer so
    // the budget begins when the user can actually see the spinner.
    // hideEvent() disarms it, so a spinner that was up when the tile was hidden
    // gets a fresh budget here too. Not while an error is on show — the
    // watchdog's generic "no data yet" must not replace a specific error.
    if (loading_overlay_ && loading_overlay_->is_active() && !error_shown_)
        arm_watchdog();
}

void BaseWidget::hideEvent(QHideEvent* event) {
    QFrame::hideEvent(event);
    // See the header: drop every hub subscription this tile owns, whatever
    // the subclass did. Subclasses' own unsubscribe calls become no-ops.
    datahub::DataHub::instance().unsubscribe(this);
    // A watchdog armed for an on-screen spinner must not fire for a tile the
    // user cannot see; showEvent() re-arms it if the spinner is still up.
    disarm_watchdog();
}

void BaseWidget::set_css_cached(QWidget* w, const QString& css) {
    // Against the widget's live sheet, not only the last one applied here: after an
    // appearance change the theme manager has usually rewritten it to exactly this
    // text already, and a sheet set elsewhere in between is never mistaken for it.
    if (!w || w->styleSheet() == css)
        return;
    w->setProperty(kBaseWidgetCssProp, css);
    w->setStyleSheet(css);
}

void BaseWidget::reset_css_cache(QWidget* w) {
    if (w)
        w->setProperty(kBaseWidgetCssProp, QVariant());
}

int BaseWidget::text_row_height(int design_px, int font_offset) {
    // Line height plus the 2px item padding above and below and the row's
    // bottom border.
    return std::max(design_px, QFontMetrics(ui::fonts::font(font_offset)).height() + 6);
}

void BaseWidget::arm_watchdog() {
    // Hidden tiles don't run the budget (constructor-time set_loading(true),
    // a config change while off-screen): showEvent() arms it once the spinner
    // is actually on screen, so it can never fire for a tile nobody sees.
    if (!loading_watchdog_ || !isVisible())
        return;
    loading_watchdog_->start();
}

void BaseWidget::disarm_watchdog() {
    if (!loading_watchdog_)
        return;
    loading_watchdog_->stop();
}

void BaseWidget::on_watchdog_fired() {
    if (!loading_overlay_ || !loading_overlay_->is_active())
        return;
    if (loading_label_) {
        loading_label_->setVisible(false);
        loading_label_->setText(QString());
    }
    // If some data trickled in before the timeout, just hide the spinner so
    // the partial content becomes visible. Otherwise surface a soft error so
    // the user knows the widget hasn't simply hung — the producer's own reason
    // when a tracked topic reported one, the generic text only when nothing did.
    // (For tracked tiles a progress count includes failed topics, so "has
    // data" comes from the topics that delivered a value.)
    const bool has_content = tracked_topics_.isEmpty() ? last_progress_loaded_ > 0 : has_topic_data();
    topic_retry_pending_ = false; // a retry that produced nothing in 20 s: show what we know
    if (has_content) {
        loading_overlay_->finish();
    } else if (failed_topic_count() > 0) {
        show_topic_failure();
    } else {
        loading_overlay_->set_error(tr("No data yet — click refresh to retry"));
    }
}

void BaseWidget::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QFrame::changeEvent(event);
}

void BaseWidget::retranslateUi() {
    if (config_btn_) {
        config_btn_->setToolTip(tr("Configure widget"));
        config_btn_->setAccessibleName(tr("Configure widget"));
    }
    if (refresh_btn_) {
        refresh_btn_->setToolTip(tr("Refresh widget data"));
        refresh_btn_->setAccessibleName(tr("Refresh widget data"));
    }
    if (close_btn_) {
        close_btn_->setToolTip(tr("Close widget"));
        close_btn_->setAccessibleName(tr("Close widget"));
    }
    if (loading_label_ && loading_label_->isVisible())
        loading_label_->setText(tr("LOADING..."));
    update_topic_status_label();
}

void BaseWidget::open_symbol(const QString& symbol, const QString& screen_id, const QString& asset_class,
                             const QString& exchange) {
    const QString sym = symbol.trimmed();
    if (sym.isEmpty() || screen_id.isEmpty())
        return;
    QVariantMap payload{{"screen_id", screen_id}, {"symbol", sym}, {"asset_class", asset_class}};
    if (!exchange.isEmpty())
        payload.insert("exchange", exchange);
    EventBus::instance().publish("nav.open_symbol", payload);
}

void BaseWidget::link_symbol(QWidget* w, const QString& symbol) {
    if (!w)
        return;
    enable_symbol_link(false);
    // Recycled rows call this on every refresh — skip the no-op re-link.
    if (w->property(kBaseWidgetSymbolProp).toString() == symbol)
        return;
    w->setProperty(kBaseWidgetSymbolProp, symbol);
    w->setCursor(Qt::PointingHandCursor);
    w->setToolTip(tr("Click to send %1 to linked panels · double-click to open it in Equity Research · "
                     "right-click for more")
                      .arg(symbol));
    // installEventFilter() on an already-installed filter just moves it to the
    // front of the list, so re-linking a recycled row never stacks filters.
    w->installEventFilter(this);
}

void BaseWidget::show_symbol_menu(const QString& symbol_in, const QPoint& global_pos, const QString& asset_class,
                                  const QString& exchange) {
    const QString symbol = symbol_in.trimmed();
    if (symbol.isEmpty())
        return;
    const bool crypto = asset_class == QLatin1String("crypto");

    // Heap + popup(), not a stack QMenu + exec(): exec() spins a nested event
    // loop, and a tile destroyed while its menu is open (layout reload, MCP
    // remove) would then delete a stack object through its parent.
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    connect(menu->addAction(tr("Open in Equity Research")), &QAction::triggered, this,
            [this, symbol]() { open_symbol(symbol, QStringLiteral("equity_research")); });
    if (crypto) {
        connect(menu->addAction(tr("Open in Crypto Trading")), &QAction::triggered, this,
                [this, symbol, exchange]() {
                    open_symbol(symbol, QStringLiteral("crypto_trading"), QStringLiteral("crypto"), exchange);
                });
    } else if (base_widget_is_tradable_equity(symbol)) {
        connect(menu->addAction(tr("Open in Equity Trading")), &QAction::triggered, this,
                [this, symbol, asset_class, exchange]() {
                    open_symbol(symbol, QStringLiteral("equity_trading"), asset_class, exchange);
                });
    }
    connect(menu->addAction(tr("Open in News")), &QAction::triggered, this,
            [this, symbol]() { open_symbol(symbol, QStringLiteral("news")); });
    if (!crypto) {
        connect(menu->addAction(tr("Backtest This Symbol")), &QAction::triggered, this, [symbol]() {
            QJsonObject config;
            config["symbols"] = QJsonArray{symbol};
            services::backtest::BacktestingService::instance().set_pending_portfolio_config(config);
            EventBus::instance().publish("nav.switch_screen", QVariantMap{{"screen_id", QStringLiteral("backtesting")}, {"exclusive", true}});
        });
    }

    // One entry per saved watchlist (same flow as the Screener's row menu).
    const auto lists = fincept::WatchlistRepository::instance().list_all();
    if (lists.is_ok() && !lists.value().isEmpty()) {
        QMenu* wl_menu = menu->addMenu(tr("Add to Watchlist"));
        for (const auto& wl : lists.value()) {
            connect(wl_menu->addAction(wl.name), &QAction::triggered, this,
                    [this, symbol, list_id = wl.id, list_name = wl.name]() {
                        auto& repo = fincept::WatchlistRepository::instance();
                        const auto stocks = repo.get_stocks(list_id);
                        if (stocks.is_ok()) {
                            for (const auto& s : stocks.value()) {
                                if (s.symbol.compare(symbol, Qt::CaseInsensitive) == 0) {
                                    QToolTip::showText(QCursor::pos(), tr("%1 is already in %2").arg(symbol, list_name),
                                                       this);
                                    return;
                                }
                            }
                        }
                        const auto r = repo.add_stock(list_id, symbol, QString());
                        QToolTip::showText(QCursor::pos(),
                                           r.is_ok() ? tr("Added %1 to %2").arg(symbol, list_name)
                                                     : tr("Could not add %1 to %2").arg(symbol, list_name),
                                           this);
                        if (r.is_ok()) // let an open Watchlist panel pick the change up
                            EventBus::instance().publish(
                                "watchlist.updated",
                                QVariantMap{{"watchlist_id", list_id}, {"action", QStringLiteral("add")},
                                            {"symbol", symbol}});
                    });
        }
    }

    menu->addSeparator();
    connect(menu->addAction(tr("Copy Symbol")), &QAction::triggered, this,
            [symbol]() { QApplication::clipboard()->setText(symbol); });
    menu->popup(global_pos);
}

void BaseWidget::install_symbol_menu(QTableWidget* table, int col, int role) {
    if (!table)
        return;
    table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table, &QWidget::customContextMenuRequested, this, [this, table, col, role](const QPoint& pos) {
        auto* hit = table->itemAt(pos);
        if (!hit)
            return;
        auto* sym_item = table->item(hit->row(), col);
        if (!sym_item)
            return;
        const QString sym = sym_item->data(role).toString();
        if (!sym.isEmpty())
            show_symbol_menu(sym, table->viewport()->mapToGlobal(pos));
    });
    install_symbol_publish(table, col, role);
}

void BaseWidget::install_symbol_publish(QTableWidget* table, int col, int role) {
    if (!table)
        return;
    enable_symbol_link(false);
    connect(table, &QTableWidget::cellClicked, this, [this, table, col, role](int row, int /*column*/) {
        if (auto* it = table->item(row, col))
            publish_symbol(it->data(role).toString());
    });
}

bool BaseWidget::is_tradable_equity(const QString& symbol) {
    return base_widget_is_tradable_equity(symbol);
}

// ── Symbol link group ────────────────────────────────────────────────────────

void BaseWidget::enable_symbol_link(bool follows, bool joins_default_group) {
    if (follows && !link_follows_) {
        link_follows_ = true;
        connect(&SymbolContext::instance(), &SymbolContext::group_symbol_changed, this,
                [this](SymbolGroup g, const SymbolRef& ref, QObject* source) {
                    if (g == SymbolGroup::None || g != link_group_ || source == this || !ref.is_valid())
                        return;
                    on_linked_symbol(ref);
                });
    }
    if (link_enabled_)
        return;
    link_enabled_ = true;
    link_joins_default_ = joins_default_group;

    group_badge_ = new ui::GroupBadge(title_bar_);
    group_badge_->setFixedSize(16, 16);
    group_badge_->setAccessibleName(tr("Symbol link group"));
    if (auto* hl = qobject_cast<QHBoxLayout*>(title_bar_->layout()))
        hl->insertWidget(1, group_badge_); // right after the accent bar
    connect(group_badge_, &ui::GroupBadge::group_change_requested, this, &BaseWidget::on_badge_group_requested);

    link_group_ = link_has_saved_ ? link_saved_ : default_link_group();
    group_badge_->set_group_silent(link_group_);
}

SymbolGroup BaseWidget::default_link_group() const {
    return link_joins_default_ && SymbolGroupRegistry::instance().enabled(SymbolGroup::A) ? SymbolGroup::A
                                                                                          : SymbolGroup::None;
}

void BaseWidget::set_link_group(SymbolGroup g) {
    if (!link_enabled_ || g == link_group_)
        return;
    link_group_ = g;
    if (group_badge_)
        group_badge_->set_group_silent(g);
    emit link_group_changed(g);
}

void BaseWidget::restore_link_group(const QJsonObject& cfg) {
    const QJsonValue v = cfg.value(QLatin1String(kLinkGroupKey));
    link_has_saved_ = v.isString();
    if (link_has_saved_) {
        const QString s = v.toString();
        link_saved_ = s.size() == 1 ? symbol_group_from_char(s.at(0)) : SymbolGroup::None;
    }
    if (!link_enabled_)
        return; // applied when (if) the tile enables linking
    const SymbolGroup g = link_has_saved_ ? link_saved_ : default_link_group();
    if (g != link_group_) {
        link_group_ = g;
        if (group_badge_)
            group_badge_->set_group_silent(g);
    }
}

void BaseWidget::store_link_group(QJsonObject& cfg) const {
    SymbolGroup g = SymbolGroup::None;
    if (link_enabled_)
        g = link_group_;
    else if (link_has_saved_)
        g = link_saved_;
    else
        return;
    cfg.insert(QLatin1String(kLinkGroupKey),
               g == SymbolGroup::None ? QStringLiteral("none") : QString(symbol_group_letter(g)));
}

void BaseWidget::on_badge_group_requested(SymbolGroup g) {
    set_link_group(g);
    if (g == SymbolGroup::None)
        return;
    // Joining a group that already has a symbol: follow it. Joining an empty
    // group: seed it with this tile's own symbol.
    const SymbolRef group_sym = SymbolContext::instance().group_symbol(g);
    if (group_sym.is_valid()) {
        if (link_follows_)
            on_linked_symbol(group_sym);
    } else {
        const SymbolRef own = linked_symbol();
        if (own.is_valid())
            SymbolContext::instance().set_group_symbol(g, own, this);
    }
}

void BaseWidget::publish_symbol(const QString& symbol, const QString& exchange) {
    if (!link_enabled_ || link_group_ == SymbolGroup::None)
        return;
    const QString sym = symbol.trimmed();
    if (sym.isEmpty())
        return;
    SymbolContext::instance().set_group_symbol(link_group_, SymbolRef::equity(sym, exchange), this);
}

void BaseWidget::link_screen(QWidget* w, const QString& screen_id, const QString& tooltip) {
    if (!w)
        return;
    w->setProperty(kBaseWidgetScreenProp, screen_id);
    w->setCursor(Qt::PointingHandCursor);
    w->setToolTip(tooltip);
    w->installEventFilter(this); // idempotent, see link_symbol()
}

bool BaseWidget::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonRelease && static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
        // A plain click sends the symbol to the link group; not consumed.
        const QString sym = obj->property(kBaseWidgetSymbolProp).toString();
        if (!sym.isEmpty())
            publish_symbol(sym);
    }
    if (event->type() == QEvent::ContextMenu) {
        const QString sym = obj->property(kBaseWidgetSymbolProp).toString();
        if (!sym.isEmpty()) {
            show_symbol_menu(sym, static_cast<QContextMenuEvent*>(event)->globalPos());
            return true;
        }
    }
    if (event->type() == QEvent::MouseButtonDblClick) {
        const QVariant sym = obj->property(kBaseWidgetSymbolProp);
        if (sym.isValid() && !sym.toString().isEmpty()) {
            open_symbol(sym.toString());
            return true;
        }
        const QVariant screen = obj->property(kBaseWidgetScreenProp);
        if (screen.isValid() && !screen.toString().isEmpty()) {
            EventBus::instance().publish("nav.switch_screen", QVariantMap{{"screen_id", screen.toString()}, {"exclusive", true}});
            return true;
        }
    }
    return QFrame::eventFilter(obj, event);
}

} // namespace fincept::screens::widgets
