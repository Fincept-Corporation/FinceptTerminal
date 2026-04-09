#include "screens/dashboard/DashboardStatusBar.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/NotifBell.h"
#include "ui/widgets/NotifPanel.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace fincept::screens {

DashboardStatusBar::DashboardStatusBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(24);
    setObjectName("dashStatusBar");

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
    auto* left = new QWidget;
    auto* ll = new QHBoxLayout(left);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(8);

    ll->addWidget(make_lbl("v4.0.0", "dsVersion"));
    ll->addWidget(make_sep());

    const char* feed_names[] = {"EQ", "FX", "CM", "FI", "CR"};
    for (auto& f : feed_names) {
        auto* fl = make_lbl(f, "dsFeed");
        // CM gets warning color — handled in refresh_theme via property
        if (QString(f) == "CM") fl->setProperty("warn", true);
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
    auto* right = new QWidget;
    auto* rl = new QHBoxLayout(right);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(8);

    rl->addWidget(make_lbl("MEM: OPTIMAL", "dsMem"));
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

    connect(notif_bell_, &fincept::ui::NotifBell::bell_clicked,
            this, &DashboardStatusBar::toggle_notif_panel);

    hl->addWidget(right);

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed,
            this, [this](const ui::ThemeTokens&) { refresh_theme(); });

    connect(&uptime_timer_, &QTimer::timeout, this, &DashboardStatusBar::update_uptime);
    uptime_timer_.start(1000);

    nam_ = new QNetworkAccessManager(this);
    connect(&ping_timer_, &QTimer::timeout, this, &DashboardStatusBar::ping_api);
    ping_timer_.start(30000);
    ping_api();

    refresh_theme();
}

void DashboardStatusBar::refresh_theme() {
    setStyleSheet(QString(
        "#dashStatusBar { background:%1; border-top:1px solid %2; }"
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
        "#dsReady { color:%5; font-weight:bold; background:transparent; }"
    ).arg(ui::colors::BG_SURFACE())    // %1
     .arg(ui::colors::BORDER_DIM())    // %2
     .arg(ui::colors::BORDER_MED())    // %3
     .arg(ui::colors::TEXT_SECONDARY())// %4
     .arg(ui::colors::POSITIVE())      // %5
     .arg(ui::colors::WARNING())       // %6
     .arg(ui::colors::CYAN()));        // %7
}

void DashboardStatusBar::update_uptime() {
    qint64 elapsed = (QDateTime::currentMSecsSinceEpoch() - start_time_) / 1000;
    int h = static_cast<int>(elapsed / 3600);
    int m = static_cast<int>((elapsed % 3600) / 60);
    int s = static_cast<int>(elapsed % 60);
    uptime_label_->setText(
        QString("%1:%2:%3").arg(h,2,10,QChar('0')).arg(m,2,10,QChar('0')).arg(s,2,10,QChar('0')));
}

void DashboardStatusBar::set_widget_count(int count) {
    layout_label_->setText(count > 0 ? "ACTIVE" : "EMPTY");
    layout_label_->setStyleSheet(
        QString("color:%1;font-weight:bold;background:transparent;")
            .arg(count > 0 ? ui::colors::POSITIVE() : ui::colors::TEXT_SECONDARY()));
}

void DashboardStatusBar::set_connected(bool connected) {
    connected_ = connected;
    feeds_label_->setText(connected ? "CONNECTED" : "DISCONNECTED");
    feeds_label_->setStyleSheet(
        QString("color:%1;font-weight:bold;background:transparent;")
            .arg(connected ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
}

void DashboardStatusBar::ping_api() {
    QNetworkRequest req(QUrl("https://api.fincept.in/health"));
    req.setTransferTimeout(5000);
    ping_elapsed_.restart();
    QNetworkReply* reply = nam_->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        set_latency(reply->error() == QNetworkReply::NoError
            ? static_cast<int>(ping_elapsed_.elapsed()) : -1);
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
    const QString color = ms < 100 ? ui::colors::POSITIVE()
                        : ms < 300 ? ui::colors::AMBER()
                                   : ui::colors::NEGATIVE();
    latency_label_->setStyleSheet(
        QString("color:%1;font-weight:bold;background:transparent;").arg(color));
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
