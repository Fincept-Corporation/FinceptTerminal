#include "screens/dashboard/DashboardStatusBar.h"
#include "screens/dashboard/DashboardPaint.h"

#include "core/market/MarketProfile.h"
#include "datahub/DataHub.h"
#include "screens/action_center/PendingOrdersBadge.h"
#include "services/connectivity/ConnectivityService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/NotifBell.h"
#include "ui/widgets/NotifPanel.h"

#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QPalette>
#include <QResizeEvent>
#include <QStyle>

#if defined(Q_OS_WIN)
// windows.h must come first — psapi.h depends on its types (BOOL, WINAPI, …)
// and including psapi.h alone yields C2146 / C2086 cascades.
#    include <windows.h>
#    include <psapi.h>
#elif defined(Q_OS_MAC)
#    include <mach/mach.h>
#elif defined(Q_OS_LINUX)
#    include <unistd.h>
#endif

namespace {

// A quote topic is "live" while its newest publish is within this many ms
// (3 × the 30 s market:quote TTL — one missed refresh is not an outage).
constexpr qint64 kStatusBarFreshQuoteMs = 90 * 1000;

QString status_bar_age_text(qint64 age_ms) {
    const qint64 s = age_ms / 1000;
    if (s < 60)
        return QStringLiteral("%1 s").arg(s);
    if (s < 3600)
        return QStringLiteral("%1 min").arg(s / 60);
    return QStringLiteral("%1 h").arg(s / 3600);
}

} // namespace

namespace fincept::screens {

DashboardStatusBar::DashboardStatusBar(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(24); // one text line; grows with a large base font
    setObjectName("dashStatusBar");
    dashboard_paint::apply_solid_window_background(this, QColor(ui::colors::BG_BASE()));

    start_time_ = QDateTime::currentMSecsSinceEpoch();

    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(0);

    auto make_lbl = [](const QString& text, const QString& name) {
        auto* l = new QLabel(text);
        l->setObjectName(name);
        return l;
    };
    auto make_sep = [&]() { return make_lbl("|", "dsSep"); };

    // ── Left ──
    auto* left = new QWidget(this);
    auto* ll = new QHBoxLayout(left);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(8);

    const QString version_text = QStringLiteral("v") + QApplication::applicationVersion();
    version_label_ = make_lbl(version_text, "dsVersion");
    ll->addWidget(version_label_);
    left_ = left;
    // One entry per group, each starting with its separator (fit_left_groups()).
    const auto begin_group = [&]() {
        QLabel* sep = make_sep();
        ll->addWidget(sep);
        left_groups_.append(QVector<QWidget*>{sep});
    };
    begin_group(); // feed lamps

    // Per-asset-class quote freshness. Each lamp follows the newest publish
    // among a few representative quote topics — the ones the dashboard's own
    // widgets (Market Pulse snapshot, index/FX/commodity/crypto tables, the
    // ticker defaults) keep subscribed.
    const struct {
        const char* code;
        QStringList symbols;
    } lamp_defs[] = {
        {"EQ", {"SPY", "^GSPC", "AAPL", "MSFT"}},
        {"FX", {"DX-Y.NYB", "EURUSD=X", "USDJPY=X"}},
        {"CM", {"GC=F", "CL=F"}},
        {"FI", {"^TNX"}},
        {"CR", {"BTC-USD", "ETH-USD"}},
    };
    for (const auto& def : lamp_defs) {
        FeedLamp lamp;
        lamp.code = QString::fromLatin1(def.code);
        for (const auto& sym : def.symbols)
            lamp.topics.append(QStringLiteral("market:quote:") + sym);
        lamp.label = make_lbl(lamp.code, "dsFeed");
        lamp.label->setProperty("state", QStringLiteral("none"));
        ll->addWidget(lamp.label);
        lamps_.append(lamp);
        left_groups_.last().append(lamp.label);
    }

    begin_group(); // session clock
    session_lbl_ = make_lbl(tr("SESSION:"), "dsLabel");
    ll->addWidget(session_lbl_);
    uptime_label_ = new QLabel("00:00:00");
    uptime_label_->setObjectName("dsUptime");
    ll->addWidget(uptime_label_);
    left_groups_.last() << session_lbl_ << uptime_label_;

    begin_group(); // layout state
    layout_caption_lbl_ = make_lbl(tr("LAYOUT:"), "dsLabel");
    ll->addWidget(layout_caption_lbl_);
    layout_label_ = new QLabel(tr("EMPTY"));
    layout_label_->setObjectName("dsLayout");
    layout_label_->setProperty("tone", QStringLiteral("dim"));
    ll->addWidget(layout_label_);
    left_groups_.last() << layout_caption_lbl_ << layout_label_;

    begin_group(); // API state
    // The probe checks the Fincept API, not the market-data feeds (those are
    // the EQ/FX/… lamps) — the caption used to say "FEEDS".
    feeds_caption_lbl_ = make_lbl(tr("API:"), "dsLabel");
    ll->addWidget(feeds_caption_lbl_);
    feeds_label_ = new QLabel(tr("CHECKING"));
    feeds_label_->setObjectName("dsFeeds");
    feeds_label_->setProperty("tone", QStringLiteral("dim"));
    ll->addWidget(feeds_label_);
    left_groups_.last() << feeds_caption_lbl_ << feeds_label_;
    ll->addStretch();

    // The left side may shrink (groups that no longer fit whole are hidden, see
    // fit_left_groups()) so the bar never makes the dashboard wider than a
    // narrow panel; the right-hand status stays whole.
    left->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    left->setMinimumWidth(0);
    hl->addWidget(left, 1);

    // ── Right ──
    auto* right = new QWidget(this);
    auto* rl = new QHBoxLayout(right);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(8);

    mem_label_ = new QLabel(tr("MEM: ---"));
    mem_label_->setObjectName("dsMem");
    mem_label_->setProperty("tone", QStringLiteral("info"));
    rl->addWidget(mem_label_);
    rl->addWidget(make_sep());

    latency_label_ = new QLabel(tr("LAT: ---"));
    latency_label_->setObjectName("dsLatency");
    latency_label_->setProperty("tone", QStringLiteral("dim"));
    rl->addWidget(latency_label_);

    rl->addWidget(make_sep());
    ready_lbl_ = make_lbl(QString::fromUtf8("● ") + tr("READY"), "dsReady");
    rl->addWidget(ready_lbl_);
    rl->addWidget(make_sep());

    // ── Pending approvals badge (hidden when none) ──────────────────────────
    pending_badge_ = new PendingOrdersBadge(this);
    rl->addWidget(pending_badge_);

    // ── Notification bell ──────────────────────────────────────────────────
    notif_bell_ = new fincept::ui::NotifBell(this);
    rl->addWidget(notif_bell_);

    connect(notif_bell_, &fincept::ui::NotifBell::bell_clicked, this, &DashboardStatusBar::toggle_notif_panel);

    hl->addWidget(right);

    // Never inside theme_changed (restyling there stalls a theme switch): once the
    // appearance has settled — the sheet only when it changed — then the text
    // widths, which follow the font.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        refresh_theme();
        fit_left_groups();
    });

    // P3: only set intervals + connect signals here. Timer.start() is
    // deferred to showEvent so the status bar doesn't tick when the
    // dashboard is hidden.
    uptime_timer_.setInterval(1000);
    connect(&uptime_timer_, &QTimer::timeout, this, &DashboardStatusBar::update_uptime);

    local_probe_timer_.setInterval(5000); // RSS + hub topic ages — both O(1), no I/O
    connect(&local_probe_timer_, &QTimer::timeout, this, [this]() {
        update_memory();
        update_feed_lamps();
    });

    refresh_theme();
    retranslateUi();
}

void DashboardStatusBar::set_tone(QWidget* w, const char* prop, const QString& value) {
    if (!w || w->property(prop).toString() == value)
        return;
    w->setProperty(prop, value);
    w->style()->unpolish(w);
    w->style()->polish(w);
}

void DashboardStatusBar::refresh_theme() {
    dashboard_paint::apply_solid_window_background(this, QColor(ui::colors::BG_BASE()));

    // Every data-driven colour is a property selector here, so updates only
    // flip a property (set_tone) instead of re-parsing a per-label sheet. The
    // tones are states (live / degraded / down), so they take the status
    // colours, not the price-direction ones.
    const QString sheet = QString("#dashStatusBar { background:%1; border-top:1px solid %2; }"
                          "#dsSep { color:%3; background:transparent; }"
                          "#dsVersion { color:%4; font-weight:bold; background:transparent; }"
                          "#dsLabel { color:%4; background:transparent; }"
                          "#dsFeed { font-weight:bold; background:transparent; }"
                          "#dsFeed[state=\"live\"] { color:%5; }"
                          "#dsFeed[state=\"stale\"] { color:%6; }"
                          "#dsFeed[state=\"none\"] { color:%8; }"
                          "#dsUptime { color:%7; font-weight:bold; background:transparent; }"
                          "#dsLayout, #dsFeeds, #dsLatency { font-weight:bold; background:transparent; }"
                          "#dsMem { background:transparent; }"
                          "#dsLayout[tone=\"pos\"], #dsFeeds[tone=\"pos\"], #dsLatency[tone=\"pos\"],"
                          " #dsMem[tone=\"pos\"] { color:%5; }"
                          "#dsLatency[tone=\"warn\"], #dsMem[tone=\"warn\"] { color:%6; }"
                          "#dsFeeds[tone=\"neg\"], #dsLatency[tone=\"neg\"], #dsMem[tone=\"neg\"] { color:%9; }"
                          "#dsLayout[tone=\"dim\"], #dsFeeds[tone=\"dim\"], #dsLatency[tone=\"dim\"] { color:%4; }"
                          "#dsMem[tone=\"info\"] { color:%7; }"
                          "#dsReady { color:%5; font-weight:bold; background:transparent; }"
                          "#dsReady[tone=\"neg\"] { color:%9; }"
                          "#dsReady[tone=\"dim\"] { color:%4; }")
                      .arg(ui::colors::BG_BASE())        // %1 — match global status bar
                      .arg(ui::colors::BORDER_DIM())     // %2
                      .arg(ui::colors::BORDER_MED())     // %3
                      .arg(ui::colors::TEXT_SECONDARY()) // %4
                      .arg(ui::colors::SUCCESS())        // %5
                      .arg(ui::colors::WARNING())        // %6
                      .arg(ui::colors::CYAN())           // %7
                      .arg(ui::colors::TEXT_DIM())       // %8
                      .arg(ui::colors::DANGER());        // %9
    if (styleSheet() != sheet) // re-styles the whole bar even for an identical sheet
        setStyleSheet(sheet);
}

void DashboardStatusBar::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    fit_left_groups();
}

void DashboardStatusBar::fit_left_groups() {
    // Squeezed, every label lost its tail at once ("SESSIO", "API: OFFLIN" run
    // into the memory figure at a 24 px font). Whole groups give way instead,
    // last first: API state, layout state, session clock, feed lamps.
    // Not while the theme_changed handlers run (ThemeManager::notifying()):
    // showing a label there is expensive; appearance_settled() re-fits.
    auto* hl = qobject_cast<QHBoxLayout*>(layout());
    if (!left_ || !hl || left_groups_.isEmpty() || width() <= 0 || ui::ThemeManager::instance().notifying())
        return;
    const auto hint = [](QWidget* w) {
        w->ensurePolished(); // font weight / padding from the stylesheet are part of the hint
        return w->sizeHint().width();
    };
    // Room for the left side: the bar minus its margins and the right-hand side.
    const QMargins m = hl->contentsMargins();
    int room = width() - m.left() - m.right();
    for (int i = 0; i < hl->count(); ++i) {
        QWidget* w = hl->itemAt(i)->widget();
        if (w && w != left_ && !w->isHidden())
            room -= hint(w) + hl->spacing();
    }
    const int gap = left_->layout() ? left_->layout()->spacing() : 0;
    int needed = version_label_ ? hint(version_label_) : 0;
    int shown = 0;
    for (const auto& group : std::as_const(left_groups_)) {
        int group_w = 0;
        for (QWidget* w : group)
            group_w += hint(w) + gap;
        if (needed + group_w > room)
            break;
        needed += group_w;
        ++shown;
    }
    for (int g = 0; g < left_groups_.size(); ++g) {
        for (QWidget* w : std::as_const(left_groups_[g])) {
            if (w->isHidden() == (g < shown))
                w->setVisible(g < shown);
        }
    }
}

void DashboardStatusBar::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    refresh_theme();
    fit_left_groups();
    // P3: start every owned QTimer here, stop in hideEvent.
    if (!uptime_timer_.isActive())
        uptime_timer_.start();
    if (!local_probe_timer_.isActive())
        local_probe_timer_.start();
    // Kick once immediately so the bar isn't blank on first show.
    update_uptime();
    update_memory();
    update_feed_lamps();
    hub_subscribe();
}

void DashboardStatusBar::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    uptime_timer_.stop();
    local_probe_timer_.stop();
    hub_unsubscribe();
}

void DashboardStatusBar::hub_subscribe() {
    if (hub_active_)
        return;
    auto& svc = services::ConnectivityService::instance();
    svc.ensure_registered_with_hub();
    // subscribe() hands back the cached reading when it is still fresh and
    // otherwise cold-starts a probe; the hub re-probes on the topic TTL.
    datahub::DataHub::instance().subscribe(this, services::ConnectivityService::health_topic(),
                                           [this](const QVariant& v) { on_health(v); });
    hub_active_ = true;
}

void DashboardStatusBar::hub_unsubscribe() {
    if (!hub_active_)
        return;
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void DashboardStatusBar::on_health(const QVariant& v) {
    const QJsonObject o = v.toJsonObject();
    if (o.isEmpty())
        return;
    const bool ok = o.value("ok").toBool();
    last_health_error_ = o.value("error").toString();
    last_health_checked_ms_ = static_cast<qint64>(o.value("checked_at_ms").toDouble());
    last_latency_ms_ = ok ? o.value("latency_ms").toInt(-1) : -1;

    if (last_latency_ms_ < 0) {
        latency_label_->setText(tr("LAT: ERR"));
        set_tone(latency_label_, "tone", QStringLiteral("neg"));
    } else {
        latency_label_->setText(tr("LAT: %1ms").arg(last_latency_ms_));
        set_tone(latency_label_, "tone",
                 last_latency_ms_ < 100   ? QStringLiteral("pos")
                 : last_latency_ms_ < 300 ? QStringLiteral("warn")
                                          : QStringLiteral("neg"));
    }

    const QString when = last_health_checked_ms_ > 0
                             ? QDateTime::fromMSecsSinceEpoch(last_health_checked_ms_).toString("HH:mm:ss")
                             : QString();
    const QString tip = ok ? tr("Fincept API %1 reachable — round trip %2 ms (checked %3)")
                                 .arg(o.value("endpoint").toString())
                                 .arg(last_latency_ms_)
                                 .arg(when)
                           : tr("Fincept API %1 unreachable: %2 (checked %3)")
                                 .arg(o.value("endpoint").toString(), last_health_error_, when);
    latency_label_->setToolTip(tip);
    feeds_label_->setToolTip(tip);

    set_api_state(ok ? 1 : 0);
}

void DashboardStatusBar::set_api_state(int state) {
    const bool changed = state != api_state_;
    api_state_ = state;
    feeds_label_->setText(state > 0 ? tr("ONLINE") : state == 0 ? tr("OFFLINE") : tr("CHECKING"));
    set_tone(feeds_label_, "tone",
             state > 0 ? QStringLiteral("pos") : state == 0 ? QStringLiteral("neg") : QStringLiteral("dim"));
    update_ready_label();
    if (changed && state >= 0)
        emit connectivity_changed(state > 0);
}

void DashboardStatusBar::update_ready_label() {
    // "● READY" used to be a constant green label that nothing drove — it stayed
    // green with the API offline. It now follows the API health probe.
    if (!ready_lbl_)
        return;
    ready_lbl_->setText(QString::fromUtf8("● ") + (api_state_ > 0    ? tr("READY")
                                                    : api_state_ == 0 ? tr("OFFLINE")
                                                                      : tr("CHECKING")));
    set_tone(ready_lbl_, "tone",
             api_state_ > 0 ? QStringLiteral("pos") : api_state_ == 0 ? QStringLiteral("neg") : QStringLiteral("dim"));
}

void DashboardStatusBar::update_feed_lamps() {
    auto& hub = datahub::DataHub::instance();
    // The lamps follow the instruments the region's tiles keep subscribed
    // (MarketProfile), plus the fixed global ones.
    const auto& p = core::market::MarketProfileStore::instance().current();
    auto topics_of = [](const QStringList& syms) {
        QStringList t;
        for (const auto& s : syms)
            t << QStringLiteral("market:quote:") + s;
        return t;
    };
    for (auto& lamp : lamps_) {
        QStringList extra;
        if (lamp.code == QLatin1String("EQ"))
            extra = QStringList{p.benchmark} + core::market::profile_symbols(p.indices) + p.movers.mid(0, 4);
        else if (lamp.code == QLatin1String("FX"))
            extra = core::market::profile_symbols(p.fx);
        else if (lamp.code == QLatin1String("CM"))
            extra = core::market::profile_symbols(p.commodities);
        else if (lamp.code == QLatin1String("CR"))
            extra = core::market::profile_symbols(p.crypto);
        for (const auto& t : topics_of(extra))
            if (!lamp.topics.contains(t))
                lamp.topics.append(t);
    }
    for (auto& lamp : lamps_) {
        qint64 best = -1;
        QString best_topic;
        for (const auto& t : lamp.topics) {
            const qint64 age = hub.age_ms(t);
            if (age >= 0 && (best < 0 || age < best)) {
                best = age;
                best_topic = t;
            }
        }
        // A closed market's quote is kept for its whole closed session (not re-published every
        // 30 s): still inside its freshness window it is current, however old the publish.
        const bool current = best > kStatusBarFreshQuoteMs && hub.peek(best_topic).isValid();
        const QString state = best < 0                                    ? QStringLiteral("none")
                              : (best <= kStatusBarFreshQuoteMs || current) ? QStringLiteral("live")
                                                                          : QStringLiteral("stale");
        set_tone(lamp.label, "state", state);
        const QString sym = best_topic.mid(QStringLiteral("market:quote:").size());
        lamp.label->setToolTip(best < 0 ? tr("%1 — no quote received this session (no widget on screen is "
                                             "subscribed to this asset class)")
                                              .arg(lamp.code)
                                        : tr("%1 — newest quote %2 ago (%3)")
                                              .arg(lamp.code, status_bar_age_text(best), sym));
    }
}

void DashboardStatusBar::update_uptime() {
    qint64 elapsed = (QDateTime::currentMSecsSinceEpoch() - start_time_) / 1000;
    int h = static_cast<int>(elapsed / 3600);
    int m = static_cast<int>((elapsed % 3600) / 60);
    int s = static_cast<int>(elapsed % 60);
    uptime_label_->setText(
        QString("%1:%2:%3").arg(h, 2, 10, QChar('0')).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0')));
}

void DashboardStatusBar::set_widget_count(int count) {
    layout_count_ = count;
    layout_label_->setText(count > 0 ? tr("ACTIVE") : tr("EMPTY"));
    set_tone(layout_label_, "tone", count > 0 ? QStringLiteral("pos") : QStringLiteral("dim"));
}

void DashboardStatusBar::update_memory() {
    if (!mem_label_)
        return;
    double rss_mb = -1.0;
#if defined(Q_OS_WIN)
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        rss_mb = static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
    }
#elif defined(Q_OS_MAC)
    mach_task_basic_info_data_t info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) ==
        KERN_SUCCESS) {
        rss_mb = static_cast<double>(info.resident_size) / (1024.0 * 1024.0);
    }
#elif defined(Q_OS_LINUX)
    QFile statm(QStringLiteral("/proc/self/statm"));
    if (statm.open(QIODevice::ReadOnly)) {
        const auto parts = QString::fromLatin1(statm.readLine()).split(QChar(' '), Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            const long page_kb = sysconf(_SC_PAGESIZE) / 1024;
            rss_mb = parts[1].toDouble() * page_kb / 1024.0;
        }
    }
#endif
    if (rss_mb < 0) {
        mem_label_->setText(tr("MEM: ---"));
        set_tone(mem_label_, "tone", QStringLiteral("info"));
        return;
    }
    // Colour: green < 512 MB, amber < 1024 MB, red beyond.
    mem_label_->setText(tr("MEM: %1 MB").arg(rss_mb, 0, 'f', 0));
    set_tone(mem_label_, "tone",
             rss_mb < 512.0    ? QStringLiteral("pos")
             : rss_mb < 1024.0 ? QStringLiteral("warn")
                               : QStringLiteral("neg"));
}

void DashboardStatusBar::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void DashboardStatusBar::retranslateUi() {
    if (session_lbl_)
        session_lbl_->setText(tr("SESSION:"));
    if (layout_caption_lbl_)
        layout_caption_lbl_->setText(tr("LAYOUT:"));
    if (feeds_caption_lbl_) {
        feeds_caption_lbl_->setText(tr("API:"));
        feeds_caption_lbl_->setToolTip(tr("Fincept API health — probed every 30 s while the dashboard is visible"));
    }
    update_ready_label();
    if (layout_label_)
        layout_label_->setText(layout_count_ > 0 ? tr("ACTIVE") : tr("EMPTY"));
    if (feeds_label_)
        feeds_label_->setText(api_state_ > 0 ? tr("ONLINE") : api_state_ == 0 ? tr("OFFLINE") : tr("CHECKING"));
    if (latency_label_) {
        if (last_latency_ms_ == -2)
            latency_label_->setText(tr("LAT: ---"));
        else if (last_latency_ms_ < 0)
            latency_label_->setText(tr("LAT: ERR"));
        else
            latency_label_->setText(tr("LAT: %1ms").arg(last_latency_ms_));
    }
    // mem_label_ refreshes on its own timer; immediate re-tick keeps the label fresh.
    update_memory();
    update_feed_lamps();
}

void DashboardStatusBar::toggle_notif_panel() {
    // Lazy-create the panel parented to the window so it floats freely
    if (!notif_panel_) {
        QWidget* root = window();
        notif_panel_ = new fincept::ui::NotifPanel(root ? root : this);
    }
    if (notif_panel_->isVisible()) {
        notif_panel_->hide();
    } else {
        notif_panel_->anchor_to(notif_bell_);
        notif_panel_->show();
        notif_panel_->raise();
    }
}

} // namespace fincept::screens
