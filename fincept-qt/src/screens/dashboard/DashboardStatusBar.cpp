#include "screens/dashboard/DashboardStatusBar.h"

#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace fincept::screens {

DashboardStatusBar::DashboardStatusBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(24);
    setStyleSheet(
        QString("background: %1; border-top: 1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    start_time_ = QDateTime::currentMSecsSinceEpoch();

    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(0);

    static const auto lbl = [](const QString& text, const QString& color, bool bold = false) {
        auto* l = new QLabel(text);
        l->setStyleSheet(QString("color: %1; font-size: 11px; %2 background: transparent; "
                                 "font-family: 'Consolas','Courier New',monospace;")
                             .arg(color, bold ? "font-weight: bold; " : ""));
        return l;
    };
    static const auto sep = [&]() { return lbl("|", ui::colors::BORDER_MED()); };

    // ── Left: version, session, layout, feeds ──
    auto* left = new QWidget;
    left->setStyleSheet("background: transparent;");
    auto* ll = new QHBoxLayout(left);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(8);

    ll->addWidget(lbl("v4.0.0", ui::colors::TEXT_SECONDARY(), true));
    ll->addWidget(sep());

    struct Feed {
        const char* label;
        const char* color;
    };
    const Feed feeds[] = {
        {"EQ", ui::colors::POSITIVE()}, {"FX", ui::colors::POSITIVE()}, {"CM", ui::colors::WARNING()},
        {"FI", ui::colors::POSITIVE()}, {"CR", ui::colors::POSITIVE()},
    };
    for (auto& f : feeds) {
        ll->addWidget(lbl(f.label, f.color, true));
    }

    ll->addWidget(sep());
    ll->addWidget(lbl("SESSION:", ui::colors::TEXT_SECONDARY()));
    uptime_label_ = new QLabel("00:00:00");
    uptime_label_->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; background: transparent; "
                                         "font-family: 'Consolas','Courier New',monospace;")
                                     .arg(ui::colors::CYAN()));
    ll->addWidget(uptime_label_);

    ll->addWidget(sep());
    ll->addWidget(lbl("LAYOUT:", ui::colors::TEXT_SECONDARY()));
    layout_label_ = new QLabel("ACTIVE");
    layout_label_->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; background: transparent; "
                                         "font-family: 'Consolas','Courier New',monospace;")
                                     .arg(ui::colors::POSITIVE()));
    ll->addWidget(layout_label_);

    ll->addWidget(sep());
    ll->addWidget(lbl("FEEDS:", ui::colors::TEXT_SECONDARY()));
    feeds_label_ = new QLabel("CONNECTED");
    feeds_label_->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; background: transparent; "
                                        "font-family: 'Consolas','Courier New',monospace;")
                                    .arg(ui::colors::POSITIVE()));
    ll->addWidget(feeds_label_);

    hl->addWidget(left);
    hl->addStretch();

    // ── Right: mem, latency, ready ──
    auto* right = new QWidget;
    right->setStyleSheet("background: transparent;");
    auto* rl = new QHBoxLayout(right);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(8);

    rl->addWidget(lbl("MEM: OPTIMAL", ui::colors::CYAN()));
    rl->addWidget(sep());

    latency_label_ = new QLabel("LAT: ---");
    latency_label_->setStyleSheet(
        QString("color: %1; font-size: 11px; font-weight: bold; background: transparent;"
                "font-family: 'Consolas','Courier New',monospace;").arg(ui::colors::TEXT_SECONDARY()));
    rl->addWidget(latency_label_);

    rl->addWidget(sep());
    rl->addWidget(lbl("● READY", ui::colors::POSITIVE(), true));

    hl->addWidget(right);

    // Uptime timer
    connect(&uptime_timer_, &QTimer::timeout, this, &DashboardStatusBar::update_uptime);
    uptime_timer_.start(1000);

    // API latency ping — fire immediately then every 30s
    nam_ = new QNetworkAccessManager(this);
    connect(&ping_timer_, &QTimer::timeout, this, &DashboardStatusBar::ping_api);
    ping_timer_.start(30000);
    ping_api();
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
    layout_label_->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; background: transparent; "
                                         "font-family: 'Consolas','Courier New',monospace;")
                                     .arg(count > 0 ? ui::colors::POSITIVE() : ui::colors::TEXT_SECONDARY()));
}

void DashboardStatusBar::set_connected(bool connected) {
    connected_ = connected;
    feeds_label_->setText(connected ? "CONNECTED" : "DISCONNECTED");
    feeds_label_->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; background: transparent; "
                                        "font-family: 'Consolas','Courier New',monospace;")
                                    .arg(connected ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
}

void DashboardStatusBar::ping_api() {
    QNetworkRequest req(QUrl("https://api.fincept.in/health"));
    req.setTransferTimeout(5000);
    ping_elapsed_.restart();
    QNetworkReply* reply = nam_->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            set_latency(static_cast<int>(ping_elapsed_.elapsed()));
        } else {
            set_latency(-1);
        }
    });
}

void DashboardStatusBar::set_latency(int ms) {
    if (ms < 0) {
        latency_label_->setText("LAT: ERR");
        latency_label_->setStyleSheet(
            QString("color: %1; font-size: 11px; font-weight: bold; background: transparent;"
                    "font-family: 'Consolas','Courier New',monospace;").arg(ui::colors::NEGATIVE()));
        return;
    }
    latency_label_->setText(QString("LAT: %1ms").arg(ms));
    // Color: green <100ms, amber 100-300ms, red >300ms
    const QString color = ms < 100 ? ui::colors::POSITIVE()
                        : ms < 300 ? ui::colors::AMBER()
                                   : ui::colors::NEGATIVE();
    latency_label_->setStyleSheet(
        QString("color: %1; font-size: 11px; font-weight: bold; background: transparent;"
                "font-family: 'Consolas','Courier New',monospace;").arg(color));
}

} // namespace fincept::screens
