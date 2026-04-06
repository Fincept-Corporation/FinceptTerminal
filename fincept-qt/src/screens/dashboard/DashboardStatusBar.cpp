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

// Color-only label style — no font-size/family (global QSS handles those)
static QString lbl_ss(const QString& color, bool bold = false) {
    return QString("color:%1;background:transparent;%2")
        .arg(color, bold ? "font-weight:bold;" : "");
}

DashboardStatusBar::DashboardStatusBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(24);

    start_time_ = QDateTime::currentMSecsSinceEpoch();

    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(0);

    auto make_lbl = [](const QString& text, const QString& color, bool bold = false) {
        auto* l = new QLabel(text);
        l->setStyleSheet(lbl_ss(color, bold));
        return l;
    };
    auto make_sep = [&]() { return make_lbl("|", ui::colors::BORDER_MED()); };

    // ── Left ──
    auto* left = new QWidget;
    left->setStyleSheet("background:transparent;");
    auto* ll = new QHBoxLayout(left);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(8);

    ll->addWidget(make_lbl("v4.0.0", ui::colors::TEXT_SECONDARY(), true));
    ll->addWidget(make_sep());

    const struct { const char* label; const char* color; } feeds[] = {
        {"EQ", ui::colors::POSITIVE()}, {"FX", ui::colors::POSITIVE()},
        {"CM", ui::colors::WARNING()},  {"FI", ui::colors::POSITIVE()},
        {"CR", ui::colors::POSITIVE()},
    };
    for (auto& f : feeds)
        ll->addWidget(make_lbl(f.label, f.color, true));

    ll->addWidget(make_sep());
    ll->addWidget(make_lbl("SESSION:", ui::colors::TEXT_SECONDARY()));
    uptime_label_ = new QLabel("00:00:00");
    uptime_label_->setStyleSheet(lbl_ss(ui::colors::CYAN(), true));
    ll->addWidget(uptime_label_);

    ll->addWidget(make_sep());
    ll->addWidget(make_lbl("LAYOUT:", ui::colors::TEXT_SECONDARY()));
    layout_label_ = new QLabel("ACTIVE");
    layout_label_->setStyleSheet(lbl_ss(ui::colors::POSITIVE(), true));
    ll->addWidget(layout_label_);

    ll->addWidget(make_sep());
    ll->addWidget(make_lbl("FEEDS:", ui::colors::TEXT_SECONDARY()));
    feeds_label_ = new QLabel("CONNECTED");
    feeds_label_->setStyleSheet(lbl_ss(ui::colors::POSITIVE(), true));
    ll->addWidget(feeds_label_);

    hl->addWidget(left);
    hl->addStretch();

    // ── Right ──
    auto* right = new QWidget;
    right->setStyleSheet("background:transparent;");
    auto* rl = new QHBoxLayout(right);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(8);

    rl->addWidget(make_lbl("MEM: OPTIMAL", ui::colors::CYAN()));
    rl->addWidget(make_sep());

    latency_label_ = new QLabel("LAT: ---");
    latency_label_->setStyleSheet(lbl_ss(ui::colors::TEXT_SECONDARY(), true));
    rl->addWidget(latency_label_);

    rl->addWidget(make_sep());
    rl->addWidget(make_lbl("● READY", ui::colors::POSITIVE(), true));
    rl->addWidget(make_sep());

    // ── Notification bell ──────────────────────────────────────────────────
    notif_bell_ = new fincept::ui::NotifBell(this);
    rl->addWidget(notif_bell_);

    // Panel is a popup — parented to the top-level window so it can float
    // over all widgets. We create it lazily on first click.
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
    setStyleSheet(QString("background:%1;border-top:1px solid %2;")
        .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
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
        lbl_ss(count > 0 ? ui::colors::POSITIVE() : ui::colors::TEXT_SECONDARY(), true));
}

void DashboardStatusBar::set_connected(bool connected) {
    connected_ = connected;
    feeds_label_->setText(connected ? "CONNECTED" : "DISCONNECTED");
    feeds_label_->setStyleSheet(
        lbl_ss(connected ? ui::colors::POSITIVE() : ui::colors::NEGATIVE(), true));
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
        latency_label_->setStyleSheet(lbl_ss(ui::colors::NEGATIVE(), true));
        return;
    }
    latency_label_->setText(QString("LAT: %1ms").arg(ms));
    const QString color = ms < 100 ? ui::colors::POSITIVE()
                        : ms < 300 ? ui::colors::AMBER()
                                   : ui::colors::NEGATIVE();
    latency_label_->setStyleSheet(lbl_ss(color, true));
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
