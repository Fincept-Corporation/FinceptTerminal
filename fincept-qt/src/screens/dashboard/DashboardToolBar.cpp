#include "screens/dashboard/DashboardToolBar.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QHBoxLayout>
#include <QPalette>
#include <QStyle>

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

DashboardToolBar::DashboardToolBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(26);
    setObjectName("dashToolBar");
    apply_solid_background(this, QColor(ui::colors::BG_SURFACE()));

    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(0);

    left_container_ = new QWidget(this);
    left_container_->setObjectName("dtLeftContainer");
    apply_solid_background(left_container_, QColor(ui::colors::BG_SURFACE()));
    auto* left = left_container_;
    auto* ll = new QHBoxLayout(left);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(8);

    auto make_sep = [&](QBoxLayout* layout) {
        auto* s = new QLabel("|");
        s->setObjectName("dtSep");
        layout->addWidget(s);
    };

    auto* brand = new QLabel("FINCEPT");
    brand->setObjectName("dtBrand");
    ll->addWidget(brand);

    auto* sub = new QLabel("TERMINAL");
    sub->setObjectName("dtSub");
    ll->addWidget(sub);

    make_sep(ll);

    status_text_ = new QLabel("LIVE");
    status_text_->setObjectName("dtStatus");
    ll->addWidget(status_text_);

    make_sep(ll);

    clock_btn_ = new QPushButton;
    clock_btn_->setObjectName("dtBtn");
    clock_btn_->setFlat(true);
    clock_btn_->setCursor(Qt::PointingHandCursor);
    clock_btn_->setToolTip("Click to toggle UTC / local time");
    connect(clock_btn_, &QPushButton::clicked, this, [this]() {
        clock_is_utc_ = !clock_is_utc_;
        update_clock();
    });
    ll->addWidget(clock_btn_);
    update_clock();

    make_sep(ll);

    widget_count_ = new QLabel("0 WIDGETS");
    widget_count_->setObjectName("dtWidgetCount");
    ll->addWidget(widget_count_);

    hl->addWidget(left);
    hl->addStretch();

    right_container_ = new QWidget(this);
    right_container_->setObjectName("dtRightContainer");
    apply_solid_background(right_container_, QColor(ui::colors::BG_SURFACE()));
    auto* right = right_container_;
    auto* rl = new QHBoxLayout(right);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(4);

    compact_btn_ = new QPushButton("COMPACT");
    compact_btn_->setFixedHeight(20);
    compact_btn_->setObjectName("dtBtn");
    connect(compact_btn_, &QPushButton::clicked, this, &DashboardToolBar::toggle_compact_clicked);
    rl->addWidget(compact_btn_);

    pulse_btn_ = new QPushButton("PULSE");
    pulse_btn_->setFixedHeight(20);
    pulse_btn_->setObjectName("dtBtn");
    connect(pulse_btn_, &QPushButton::clicked, this, &DashboardToolBar::toggle_pulse_clicked);
    rl->addWidget(pulse_btn_);

    make_sep(rl);

    auto* refresh_btn = new QPushButton("REFRESH");
    refresh_btn->setFixedHeight(20);
    refresh_btn->setObjectName("dtBtn");
    refresh_btn->setToolTip(QStringLiteral("Force-refresh all live data on the dashboard"));
    connect(refresh_btn, &QPushButton::clicked, this, &DashboardToolBar::refresh_clicked);
    rl->addWidget(refresh_btn);

    auto* add_btn = new QPushButton("+ ADD");
    add_btn->setFixedHeight(20);
    add_btn->setObjectName("dtAddBtn");
    connect(add_btn, &QPushButton::clicked, this, &DashboardToolBar::add_widget_clicked);
    rl->addWidget(add_btn);

    auto* save_btn = new QPushButton("SAVE");
    save_btn->setFixedHeight(20);
    save_btn->setObjectName("dtBtn");
    connect(save_btn, &QPushButton::clicked, this, &DashboardToolBar::save_layout_clicked);
    rl->addWidget(save_btn);

    auto* reset_btn = new QPushButton("RESET");
    reset_btn->setFixedHeight(20);
    reset_btn->setObjectName("dtResetBtn");
    connect(reset_btn, &QPushButton::clicked, this, &DashboardToolBar::reset_layout_clicked);
    rl->addWidget(reset_btn);

    hl->addWidget(right);

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { refresh_theme(); });

    // P3: timer interval is set here, but start() is deferred to showEvent()
    // so the toolbar doesn't tick when the dashboard tab isn't visible.
    clock_timer_.setInterval(1000);
    connect(&clock_timer_, &QTimer::timeout, this, &DashboardToolBar::update_clock);

    refresh_theme();
}

void DashboardToolBar::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    clock_timer_.stop();
}

void DashboardToolBar::refresh_theme() {
    const QColor surface(ui::colors::BG_SURFACE());
    apply_solid_background(this, surface);
    apply_solid_background(left_container_, surface);
    apply_solid_background(right_container_, surface);

    setStyleSheet(
        QString("#dashToolBar { background:%1; border-bottom:1px solid %2; }"
                "#dtLeftContainer, #dtRightContainer { background:%1; }"
                "#dtSep { color:%3; background:transparent; }"
                "#dtBrand { color:%4; font-weight:bold; letter-spacing:1px; background:transparent; }"
                "#dtSub { color:%5; font-weight:bold; background:transparent; }"
                "#dtStatus { color:%6; font-weight:bold; background:transparent; }"
                "#dtClock { color:%7; background:transparent; }"
                "#dtWidgetCount { color:%8; font-weight:bold; background:transparent; }"
                "#dtBtn { background:%9; border:1px solid %3; color:%7; padding:0 10px; font-weight:bold; }"
                "#dtBtn:hover { background:%10; color:%11; border-color:%12; }"
                "#dtAddBtn { background:%9; border:1px solid %13; color:%4; padding:0 10px; font-weight:bold; }"
                "#dtAddBtn:hover { background:%10; color:%11; border-color:%12; }"
                "#dtResetBtn { background:%9; border:1px solid %14; color:%14; padding:0 10px; font-weight:bold; }"
                "#dtResetBtn:hover { background:%10; color:%11; border-color:%12; }")
            .arg(ui::colors::BG_SURFACE())     // %1
            .arg(ui::colors::BORDER_DIM())     // %2
            .arg(ui::colors::BORDER_MED())     // %3
            .arg(ui::colors::AMBER())          // %4
            .arg(ui::colors::TEXT_TERTIARY())  // %5
            .arg(ui::colors::POSITIVE())       // %6
            .arg(ui::colors::TEXT_SECONDARY()) // %7
            .arg(ui::colors::CYAN())           // %8
            .arg(ui::colors::BG_RAISED())      // %9
            .arg(ui::colors::BG_HOVER())       // %10
            .arg(ui::colors::TEXT_PRIMARY())   // %11
            .arg(ui::colors::BORDER_BRIGHT())  // %12
            .arg(ui::colors::AMBER_DIM())      // %13
            .arg(ui::colors::NEGATIVE()));     // %14

    // Force child containers to re-evaluate their background after stylesheet change.
    // Without this, Qt may repaint them with the default palette grey on show events.
    auto repolish = [](QWidget* w) {
        if (!w)
            return;
        w->style()->unpolish(w);
        w->style()->polish(w);
        w->update();
    };
    repolish(left_container_);
    repolish(right_container_);
    repolish(this);
}

void DashboardToolBar::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    refresh_theme();
    update_clock();
    if (!clock_timer_.isActive())
        clock_timer_.start();
}

void DashboardToolBar::update_clock() {
    const QString suffix = clock_is_utc_ ? " UTC" : " LOC";
    const QDateTime now = clock_is_utc_ ? QDateTime::currentDateTimeUtc() : QDateTime::currentDateTime();
    clock_btn_->setText(now.toString("yyyy-MM-dd HH:mm:ss") + suffix);
}

void DashboardToolBar::set_widget_count(int count) {
    widget_count_->setText(QString("%1 WIDGETS").arg(count));
}

void DashboardToolBar::set_connected(bool connected) {
    status_text_->setText(connected ? "LIVE" : "OFFLINE");
    // refresh_theme handles the base color; override just this label dynamically
    status_text_->setStyleSheet(QString("color:%1;font-weight:bold;background:transparent;")
                                    .arg(connected ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
}

} // namespace fincept::screens
