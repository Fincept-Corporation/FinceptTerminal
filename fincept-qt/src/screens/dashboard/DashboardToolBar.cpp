#include "screens/dashboard/DashboardToolBar.h"

#include "ui/theme/Theme.h"

#include <QHBoxLayout>

namespace fincept::screens {

DashboardToolBar::DashboardToolBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(26);
    setStyleSheet(
        QString("background: %1; border-bottom: 1px solid %2;").arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));

    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(0);

    // ── Left: Terminal Identity ──
    auto* left = new QWidget;
    left->setStyleSheet("background: transparent;");
    auto* ll = new QHBoxLayout(left);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(8);

    auto* brand = new QLabel("■ FINCEPT");
    brand->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; letter-spacing: 1px; "
                                 "background: transparent; font-family: 'Consolas','Courier New',monospace;")
                             .arg(ui::colors::AMBER));
    ll->addWidget(brand);

    auto* sub = new QLabel("TERMINAL");
    sub->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; "
                               "background: transparent; font-family: 'Consolas','Courier New',monospace;")
                           .arg(ui::colors::TEXT_TERTIARY));
    ll->addWidget(sub);

    // Separator
    auto* sep1 = new QLabel("|");
    sep1->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent; "
                                "font-family: 'Consolas','Courier New',monospace;")
                            .arg(ui::colors::BORDER_MED));
    ll->addWidget(sep1);

    // Connection status — text-only, no dot, no border-radius
    status_text_ = new QLabel("● LIVE");
    status_text_->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; "
                                        "background: transparent; font-family: 'Consolas','Courier New',monospace;")
                                    .arg(ui::colors::POSITIVE));
    ll->addWidget(status_text_);

    auto* sep2 = new QLabel("|");
    sep2->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent; "
                                "font-family: 'Consolas','Courier New',monospace;")
                            .arg(ui::colors::BORDER_MED));
    ll->addWidget(sep2);

    clock_label_ = new QLabel;
    clock_label_->setStyleSheet(QString("color: %1; font-size: 11px; "
                                        "background: transparent; font-family: 'Consolas','Courier New',monospace;")
                                    .arg(ui::colors::TEXT_SECONDARY));
    ll->addWidget(clock_label_);
    update_clock();

    auto* sep3 = new QLabel("|");
    sep3->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent; "
                                "font-family: 'Consolas','Courier New',monospace;")
                            .arg(ui::colors::BORDER_MED));
    ll->addWidget(sep3);

    // Widget count
    widget_count_ = new QLabel("0 WIDGETS");
    widget_count_->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; "
                                         "background: transparent; font-family: 'Consolas','Courier New',monospace;")
                                     .arg(ui::colors::CYAN));
    ll->addWidget(widget_count_);

    hl->addWidget(left);
    hl->addStretch();

    // ── Right: Action buttons ──
    auto* right = new QWidget;
    right->setStyleSheet("background: transparent;");
    auto* rl = new QHBoxLayout(right);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(4);

    static const auto btn_style = [](const char* border_color, const char* text_color) {
        return QString("QPushButton { background: #111111; border: 1px solid %1; color: %2; "
                       "padding: 0 10px; font-size: 11px; font-weight: bold; "
                       "font-family: 'Consolas','Courier New',monospace; }"
                       "QPushButton:hover { background: #161616; color: #e5e5e5; border-color: #333333; }")
            .arg(border_color, text_color);
    };

    auto btn_primary = QString("QPushButton { background: rgba(217,119,6,0.1); border: 1px solid %1; color: %2; "
                               "padding: 0 10px; font-size: 11px; font-weight: bold; "
                               "font-family: 'Consolas','Courier New',monospace; }"
                               "QPushButton:hover { background: %2; color: #080808; }")
                           .arg(ui::colors::AMBER_DIM, ui::colors::AMBER);

    auto btn_danger = QString("QPushButton { background: rgba(220,38,38,0.1); border: 1px solid #7f1d1d; color: %1; "
                              "padding: 0 10px; font-size: 11px; font-weight: bold; "
                              "font-family: 'Consolas','Courier New',monospace; }"
                              "QPushButton:hover { background: %1; color: #e5e5e5; }")
                          .arg(ui::colors::NEGATIVE);

    compact_btn_ = new QPushButton("COMPACT");
    compact_btn_->setFixedHeight(20);
    compact_btn_->setStyleSheet(btn_style(ui::colors::BORDER_DIM, ui::colors::TEXT_SECONDARY));
    connect(compact_btn_, &QPushButton::clicked, this, &DashboardToolBar::toggle_compact_clicked);
    rl->addWidget(compact_btn_);

    pulse_btn_ = new QPushButton("PULSE");
    pulse_btn_->setFixedHeight(20);
    pulse_btn_->setStyleSheet(btn_style(ui::colors::BORDER_DIM, ui::colors::TEXT_SECONDARY));
    connect(pulse_btn_, &QPushButton::clicked, this, &DashboardToolBar::toggle_pulse_clicked);
    rl->addWidget(pulse_btn_);

    auto* sep4 = new QLabel("|");
    sep4->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent; "
                                "font-family: 'Consolas','Courier New',monospace;")
                            .arg(ui::colors::BORDER_MED));
    rl->addWidget(sep4);

    auto* add_btn = new QPushButton("+ ADD");
    add_btn->setFixedHeight(20);
    add_btn->setStyleSheet(btn_primary);
    connect(add_btn, &QPushButton::clicked, this, &DashboardToolBar::add_widget_clicked);
    rl->addWidget(add_btn);

    auto* save_btn = new QPushButton("SAVE");
    save_btn->setFixedHeight(20);
    save_btn->setStyleSheet(btn_style(ui::colors::BORDER_DIM, ui::colors::TEXT_SECONDARY));
    connect(save_btn, &QPushButton::clicked, this, &DashboardToolBar::save_layout_clicked);
    rl->addWidget(save_btn);

    auto* reset_btn = new QPushButton("RESET");
    reset_btn->setFixedHeight(20);
    reset_btn->setStyleSheet(btn_danger);
    connect(reset_btn, &QPushButton::clicked, this, &DashboardToolBar::reset_layout_clicked);
    rl->addWidget(reset_btn);

    hl->addWidget(right);

    // Clock timer
    connect(&clock_timer_, &QTimer::timeout, this, &DashboardToolBar::update_clock);
    clock_timer_.start(1000);
}

void DashboardToolBar::update_clock() {
    auto now = QDateTime::currentDateTimeUtc();
    clock_label_->setText(now.toString("yyyy-MM-dd HH:mm:ss") + " UTC");
}

void DashboardToolBar::set_widget_count(int count) {
    widget_count_->setText(QString("%1 WIDGETS").arg(count));
}

void DashboardToolBar::set_connected(bool connected) {
    connected_ = connected;
    const char* color = connected ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
    const char* label = connected ? "● LIVE" : "○ OFFLINE";
    status_text_->setText(label);
    status_text_->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; "
                                        "background: transparent; font-family: 'Consolas','Courier New',monospace;")
                                    .arg(color));
}

} // namespace fincept::screens
