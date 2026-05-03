#include "screens/dashboard/DashboardStatusBar.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/NotifBell.h"
#include "ui/widgets/NotifPanel.h"

#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QHBoxLayout>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPalette>
#include <QUrl>

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

void apply_solid_background(QWidget* widget, const QColor& color) {
    if (!widget)
        return;

    widget->setAttribute(Qt::WA_StyledBackground, true);
    widget->setAutoFillBackground(true);

    QPalette pal = widget->palette();
    pal.setColor(QPalette::Window, color);
    widget->setPalette(pal);
}

} // namespace

namespace fincept::screens {

DashboardStatusBar::DashboardStatusBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(24);
    setObjectName("dashStatusBar");
    apply_solid_background(this, QColor(ui::colors::BG_BASE()));

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
    ll->addWidget(make_sep());

    const char* feed_names[] = {"EQ", "FX", "CM", "FI", "CR"};
    for (auto& f : feed_names) {
        auto* fl = make_lbl(f, "dsFeed");
        // CM gets warning color — handled in refresh_theme via property
        if (QString(f) == "CM")
            fl->setProperty("warn", true);
        ll->addWidget(fl);
    }

    ll->addWidget(make_sep());
    ll->addWidget(make_lbl("SESSION:", "dsLabel"));
    uptime_label_ = new QLabel("00:00:00");
    uptime_label_->setObjectName("dsUptime");
    ll->addWidget(uptime_label_);

    ll->addWidget(make_sep());
    ll->addWidget(make_lbl("LAYOUT:", "dsLabel"));
    layout_label_ = new QLabel("ACTIVE");
    layout_label_->setObjectName("dsLayout");
    ll->addWidget(layout_label_);

    ll->addWidget(make_sep());
    ll->addWidget(make_lbl("FEEDS:", "dsLabel"));
    feeds_label_ = new QLabel("CONNECTED");
    feeds_label_->setObjectName("dsFeeds");
    ll->addWidget(feeds_label_);

    hl->addWidget(left);
    hl->addStretch();

    // ── Right ──
    auto* right = new QWidget(this);
    auto* rl = new QHBoxLayout(right);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(8);

    mem_label_ = new QLabel(QStringLiteral("MEM: ---"));
    mem_label_->setObjectName("dsMem");
    rl->addWidget(mem_label_);
    rl->addWidget(make_sep());

    latency_label_ = new QLabel("LAT: ---");
    latency_label_->setObjectName("dsLatency");
    rl->addWidget(latency_label_);

    rl->addWidget(make_sep());
    rl->addWidget(make_lbl(QString::fromUtf8("● READY"), "dsReady"));
    rl->addWidget(make_sep());

    // ── Notification bell ──────────────────────────────────────────────────
    notif_bell_ = new fincept::ui::NotifBell(this);
    rl->addWidget(notif_bell_);

    connect(notif_bell_, &fincept::ui::NotifBell::bell_clicked, this, &DashboardStatusBar::toggle_notif_panel);

    hl->addWidget(right);

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { refresh_theme(); });

    // P3: only set intervals + connect signals here. Timer.start() is
    // deferred to showEvent so the status bar doesn't tick when the
    // dashboard is hidden.
    uptime_timer_.setInterval(1000);
    connect(&uptime_timer_, &QTimer::timeout, this, &DashboardStatusBar::update_uptime);

    nam_ = new QNetworkAccessManager(this);
    ping_timer_.setInterval(30000);
    connect(&ping_timer_, &QTimer::timeout, this, &DashboardStatusBar::ping_api);

    mem_timer_.setInterval(5000); // 5s — memory probe is cheap; this is just a status hint
    connect(&mem_timer_, &QTimer::timeout, this, &DashboardStatusBar::update_memory);

    refresh_theme();
}

void DashboardStatusBar::refresh_theme() {
    apply_solid_background(this, QColor(ui::colors::BG_BASE()));

    setStyleSheet(QString("#dashStatusBar { background:%1; border-top:1px solid %2; }"
                          "#dsSep { color:%3; background:transparent; }"
                          "#dsVersion { color:%4; font-weight:bold; background:transparent; }"
                          "#dsLabel { color:%4; background:transparent; }"
                          "#dsFeed { color:%5; font-weight:bold; background:transparent; }"
                          "#dsFeed[warn=\"true\"] { color:%6; }"
                          "#dsUptime { color:%7; font-weight:bold; background:transparent; }"
                          "#dsLayout { color:%5; font-weight:bold; background:transparent; }"
                          "#dsFeeds { color:%5; font-weight:bold; background:transparent; }"
                          "#dsMem { color:%7; background:transparent; }"
                          "#dsLatency { color:%4; font-weight:bold; background:transparent; }"
                          "#dsReady { color:%5; font-weight:bold; background:transparent; }")
                      .arg(ui::colors::BG_BASE())         // %1 — match global status bar
                      .arg(ui::colors::BORDER_DIM())     // %2
                      .arg(ui::colors::BORDER_MED())     // %3
                      .arg(ui::colors::TEXT_SECONDARY()) // %4
                      .arg(ui::colors::POSITIVE())       // %5
                      .arg(ui::colors::WARNING())        // %6
                      .arg(ui::colors::CYAN()));         // %7
}

void DashboardStatusBar::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    refresh_theme();
    // P3: start every owned QTimer here, stop in hideEvent.
    if (!uptime_timer_.isActive())
        uptime_timer_.start();
    if (!ping_timer_.isActive())
        ping_timer_.start();
    if (!mem_timer_.isActive())
        mem_timer_.start();
    // Kick once immediately so the bar isn't blank on first show.
    update_uptime();
    update_memory();
    ping_api();
}

void DashboardStatusBar::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    uptime_timer_.stop();
    ping_timer_.stop();
    mem_timer_.stop();
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
    layout_label_->setText(count > 0 ? "ACTIVE" : "EMPTY");
    layout_label_->setStyleSheet(QString("color:%1;font-weight:bold;background:transparent;")
                                     .arg(count > 0 ? ui::colors::POSITIVE() : ui::colors::TEXT_SECONDARY()));
}

void DashboardStatusBar::set_connected(bool connected) {
    feeds_label_->setText(connected ? "CONNECTED" : "DISCONNECTED");
    feeds_label_->setStyleSheet(QString("color:%1;font-weight:bold;background:transparent;")
                                    .arg(connected ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
}

void DashboardStatusBar::update_memory() {
    if (!mem_label_)
        return;
    double rss_mb = -1.0;
#if defined(Q_OS_WIN)
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        rss_mb = static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
    }
#elif defined(Q_OS_MAC)
    mach_task_basic_info_data_t info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS) {
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
        mem_label_->setText(QStringLiteral("MEM: ---"));
        return;
    }
    // Colour: green < 512 MB, amber < 1024 MB, red beyond.
    const QString color = rss_mb < 512.0 ? ui::colors::POSITIVE()
                          : rss_mb < 1024.0 ? ui::colors::AMBER()
                                            : ui::colors::NEGATIVE();
    mem_label_->setText(QStringLiteral("MEM: %1 MB").arg(rss_mb, 0, 'f', 0));
    mem_label_->setStyleSheet(QString("color:%1;background:transparent;").arg(color));
}

void DashboardStatusBar::ping_api() {
    QNetworkRequest req(QUrl("https://api.fincept.in/health"));
    req.setTransferTimeout(5000);
    ping_elapsed_.restart();
    QNetworkReply* reply = nam_->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        set_latency(reply->error() == QNetworkReply::NoError ? static_cast<int>(ping_elapsed_.elapsed()) : -1);
    });
}

void DashboardStatusBar::set_latency(int ms) {
    if (ms < 0) {
        latency_label_->setText("LAT: ERR");
        latency_label_->setStyleSheet(
            QString("color:%1;font-weight:bold;background:transparent;").arg(ui::colors::NEGATIVE()));
        return;
    }
    latency_label_->setText(QString("LAT: %1ms").arg(ms));
    const QString color = ms < 100 ? ui::colors::POSITIVE() : ms < 300 ? ui::colors::AMBER() : ui::colors::NEGATIVE();
    latency_label_->setStyleSheet(QString("color:%1;font-weight:bold;background:transparent;").arg(color));
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
