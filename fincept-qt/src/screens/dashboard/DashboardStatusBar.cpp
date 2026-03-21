#include "screens/dashboard/DashboardStatusBar.h"
#include "ui/theme/Theme.h"
#include <QHBoxLayout>
#include <QDateTime>

namespace fincept::screens {

DashboardStatusBar::DashboardStatusBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(22);
    setStyleSheet(QString("background: %1; border-top: 1px solid %2;")
        .arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));

    start_time_ = QDateTime::currentMSecsSinceEpoch();

    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(0);

    // ── Left status ──
    auto* left = new QWidget;
    auto* ll = new QHBoxLayout(left);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(8);

    auto lbl_style = QString("font-size: 9px; background: transparent; font-family: Consolas;");

    auto* version = new QLabel("FINCEPT TERMINAL v4.0.0");
    version->setStyleSheet(QString("color: %1; font-weight: bold; %2").arg(ui::colors::AMBER, lbl_style));
    ll->addWidget(version);

    auto* sep1 = new QLabel("|");
    sep1->setStyleSheet(QString("color: %1; %2").arg(ui::colors::BORDER_MED, lbl_style));
    ll->addWidget(sep1);

    auto* sess = new QLabel("SESSION:");
    sess->setStyleSheet(QString("color: %1; %2").arg(ui::colors::TEXT_SECONDARY, lbl_style));
    ll->addWidget(sess);

    uptime_label_ = new QLabel("00:00:00");
    uptime_label_->setStyleSheet(QString("color: %1; font-weight: bold; %2").arg(ui::colors::CYAN, lbl_style));
    ll->addWidget(uptime_label_);

    auto* sep2 = new QLabel("|");
    sep2->setStyleSheet(QString("color: %1; %2").arg(ui::colors::BORDER_MED, lbl_style));
    ll->addWidget(sep2);

    auto* layout_lbl = new QLabel("LAYOUT:");
    layout_lbl->setStyleSheet(QString("color: %1; %2").arg(ui::colors::TEXT_SECONDARY, lbl_style));
    ll->addWidget(layout_lbl);

    layout_label_ = new QLabel("ACTIVE");
    layout_label_->setStyleSheet(QString("color: %1; font-weight: bold; %2").arg(ui::colors::POSITIVE, lbl_style));
    ll->addWidget(layout_label_);

    auto* sep3 = new QLabel("|");
    sep3->setStyleSheet(QString("color: %1; %2").arg(ui::colors::BORDER_MED, lbl_style));
    ll->addWidget(sep3);

    auto* feeds_lbl = new QLabel("FEEDS:");
    feeds_lbl->setStyleSheet(QString("color: %1; %2").arg(ui::colors::TEXT_SECONDARY, lbl_style));
    ll->addWidget(feeds_lbl);

    feeds_label_ = new QLabel("CONNECTED");
    feeds_label_->setStyleSheet(QString("color: %1; font-weight: bold; %2").arg(ui::colors::POSITIVE, lbl_style));
    ll->addWidget(feeds_lbl);
    ll->addWidget(feeds_label_);

    hl->addWidget(left);
    hl->addStretch();

    // ── Center: Feed quality indicators ──
    auto* center = new QWidget;
    auto* cl = new QHBoxLayout(center);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(8);

    struct Feed { const char* label; const char* color; };
    Feed feeds[] = {
        {"EQ", ui::colors::POSITIVE}, {"FX", ui::colors::POSITIVE},
        {"CM", ui::colors::WARNING},  {"CR", ui::colors::POSITIVE},
        {"FI", ui::colors::POSITIVE},
    };

    for (auto& f : feeds) {
        cl->addWidget(build_feed_indicator(f.label, f.color));
    }

    hl->addWidget(center);
    hl->addStretch();

    // ── Right status ──
    auto* right = new QWidget;
    auto* rl = new QHBoxLayout(right);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(8);

    auto* mem = new QLabel("MEM: OPTIMAL");
    mem->setStyleSheet(QString("color: %1; %2").arg(ui::colors::CYAN, lbl_style));
    rl->addWidget(mem);

    auto* sep4 = new QLabel("|");
    sep4->setStyleSheet(QString("color: %1; %2").arg(ui::colors::BORDER_MED, lbl_style));
    rl->addWidget(sep4);

    auto* lat = new QLabel("LAT: 12ms");
    lat->setStyleSheet(QString("color: %1; %2").arg(ui::colors::POSITIVE, lbl_style));
    rl->addWidget(lat);

    auto* sep5 = new QLabel("|");
    sep5->setStyleSheet(QString("color: %1; %2").arg(ui::colors::BORDER_MED, lbl_style));
    rl->addWidget(sep5);

    auto* ready = new QLabel("READY");
    ready->setStyleSheet(QString("color: %1; font-weight: bold; %2").arg(ui::colors::AMBER, lbl_style));
    rl->addWidget(ready);

    hl->addWidget(right);

    // Uptime timer
    connect(&uptime_timer_, &QTimer::timeout, this, &DashboardStatusBar::update_uptime);
    uptime_timer_.start(1000);
}

void DashboardStatusBar::update_uptime() {
    qint64 elapsed = (QDateTime::currentMSecsSinceEpoch() - start_time_) / 1000;
    int h = static_cast<int>(elapsed / 3600);
    int m = static_cast<int>((elapsed % 3600) / 60);
    int s = static_cast<int>(elapsed % 60);
    uptime_label_->setText(QString("%1:%2:%3")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0')));
}

QWidget* DashboardStatusBar::build_feed_indicator(const QString& label, const QString& color) {
    auto* w = new QWidget;
    auto* hl = new QHBoxLayout(w);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(3);

    auto* dot = new QLabel;
    dot->setFixedSize(4, 4);
    dot->setStyleSheet(QString("background: %1; border-radius: 2px;").arg(color));
    hl->addWidget(dot);

    auto* lbl = new QLabel(label);
    lbl->setStyleSheet(QString("color: %1; font-size: 8px; font-weight: bold; background: transparent;").arg(color));
    hl->addWidget(lbl);

    return w;
}

void DashboardStatusBar::set_widget_count(int count) {
    layout_label_->setText(count > 0 ? "ACTIVE" : "EMPTY");
    layout_label_->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 9px; background: transparent; font-family: Consolas;")
        .arg(count > 0 ? ui::colors::POSITIVE : ui::colors::TEXT_SECONDARY));
}

void DashboardStatusBar::set_connected(bool connected) {
    connected_ = connected;
    feeds_label_->setText(connected ? "CONNECTED" : "DISCONNECTED");
    feeds_label_->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 9px; background: transparent; font-family: Consolas;")
        .arg(connected ? ui::colors::POSITIVE : ui::colors::NEGATIVE));
}

} // namespace fincept::screens
