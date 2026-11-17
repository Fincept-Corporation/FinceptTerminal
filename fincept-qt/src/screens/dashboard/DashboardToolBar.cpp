#include "screens/dashboard/DashboardToolBar.h"
#include "screens/dashboard/DashboardPaint.h"

#include "core/market/MarketProfile.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/ProAnalytics.h"

#include <QHBoxLayout>

#include <algorithm>
#include <QPalette>
#include <QStyle>

namespace fincept::screens {

DashboardToolBar::DashboardToolBar(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(26); // one text line; grows with a large base font
    setObjectName("dashToolBar");
    dashboard_paint::apply_solid_window_background(this, QColor(ui::colors::BG_SURFACE()));

    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(0);

    left_container_ = new QWidget(this);
    left_container_->setObjectName("dtLeftContainer");
    dashboard_paint::apply_solid_window_background(left_container_, QColor(ui::colors::BG_SURFACE()));
    auto* left = left_container_;
    auto* ll = new QHBoxLayout(left);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(8);

    auto make_sep = [&](QBoxLayout* layout) {
        auto* s = new QLabel("|");
        s->setObjectName("dtSep");
        layout->addWidget(s);
        return s;
    };

    auto* brand = new QLabel("FINCEPT");
    brand->setObjectName("dtBrand");
    ll->addWidget(brand);

    auto* sub = new QLabel("TERMINAL");
    sub->setObjectName("dtSub");
    ll->addWidget(sub);

    // Dropped first when the dashboard is narrow (fit_left_items()).
    fold_groups_.append(QVector<QWidget*>{brand, sub, make_sep(ll)});

    status_text_ = new QLabel(tr("LIVE"));
    status_text_->setObjectName("dtStatus");
    ll->addWidget(status_text_);

    auto* sep_after_status = make_sep(ll);

    clock_btn_ = new QPushButton;
    clock_btn_->setObjectName("dtBtn");
    clock_btn_->setFlat(true);
    clock_btn_->setCursor(Qt::PointingHandCursor);
    clock_btn_->setToolTip(tr("Click to toggle UTC / local time"));
    connect(clock_btn_, &QPushButton::clicked, this, [this]() {
        clock_is_utc_ = !clock_is_utc_;
        update_clock();
    });
    ll->addWidget(clock_btn_);
    update_clock();

    auto* sep_after_clock = make_sep(ll);
    fold_groups_.append(QVector<QWidget*>{clock_btn_, sep_after_clock});

    widget_count_ = new QLabel(tr("%1 WIDGETS").arg(0));
    widget_count_->setObjectName("dtWidgetCount");
    ll->addWidget(widget_count_);
    fold_groups_.append(QVector<QWidget*>{widget_count_, sep_after_status});
    ll->addStretch();

    // The left side gives way to the action buttons: on a narrow dashboard
    // (1280 px and less) the whole bar used to be wider than the panel, so the
    // right-hand buttons (RESET, SAVE) were cut off. Its items fold away in
    // order (fit_left_items()) and the container may shrink to nothing.
    left_container_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    left_container_->setMinimumWidth(0);
    hl->addWidget(left, 1);

    right_container_ = new QWidget(this);
    right_container_->setObjectName("dtRightContainer");
    dashboard_paint::apply_solid_window_background(right_container_, QColor(ui::colors::BG_SURFACE()));
    auto* right = right_container_;
    auto* rl = new QHBoxLayout(right);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(4);

    // Market region: drives every tile's default instruments (MarketProfile).
    region_combo_ = new QComboBox;
    region_combo_->setObjectName("dtRegion");
    region_combo_->setMinimumHeight(20);
    region_combo_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    fill_regions();
    connect(region_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        if (!filling_regions_)
            core::market::MarketProfileStore::instance().set_current(region_combo_->currentData().toString());
    });
    connect(&core::market::MarketProfileStore::instance(), &core::market::MarketProfileStore::profile_changed, this,
            [this]() { fill_regions(); });
    rl->addWidget(region_combo_);

    layouts_btn_ = new QPushButton(tr("LAYOUTS"));
    layouts_btn_->setMinimumHeight(20);
    layouts_btn_->setObjectName("dtBtn");
    layouts_btn_->setToolTip(tr("Named layouts: save, switch, export and import"));
    connect(layouts_btn_, &QPushButton::clicked, this, [this]() { emit layouts_menu_requested(layouts_btn_); });
    rl->addWidget(layouts_btn_);

    make_sep(rl);

    compact_btn_ = new QPushButton(tr("COMPACT"));
    compact_btn_->setMinimumHeight(20);
    compact_btn_->setObjectName("dtBtn");
    compact_btn_->setCheckable(true);
    compact_btn_->setChecked(true); // templates and new dashboards are compact
    compact_btn_->setToolTip(tr("Compact density (on: denser rows, the templates' design) — click for the roomier "
                                "layout"));
    // The screen owns the state (the layout's row height); the button only asks.
    connect(compact_btn_, &QPushButton::clicked, this, [this]() {
        compact_btn_->setChecked(compact_);
        emit toggle_compact_clicked();
    });
    rl->addWidget(compact_btn_);

    pulse_btn_ = new QPushButton(tr("PULSE"));
    pulse_btn_->setMinimumHeight(20);
    pulse_btn_->setObjectName("dtBtn");
    // No "(Ctrl+P)": that is the global Screenshot shortcut; the pulse panel has none.
    pulse_btn_->setToolTip(tr("Show / hide the Market Pulse panel"));
    connect(pulse_btn_, &QPushButton::clicked, this, &DashboardToolBar::toggle_pulse_clicked);
    rl->addWidget(pulse_btn_);
    // Pro analytics: the server's cross-asset market pulse, in its own window (opens on
    // click only; hidden while the server does not offer Pro analytics).
    rl->addWidget(ui::pro::make_button(ui::pro::Kind::MarketPulse, {}, right));

    make_sep(rl);

    refresh_btn_ = new QPushButton(tr("REFRESH"));
    refresh_btn_->setProperty("fincept_agent_risk", "spend"); // also reloads billed tiles (credits)
    refresh_btn_->setMinimumHeight(20);
    refresh_btn_->setObjectName("dtBtn");
    refresh_btn_->setToolTip(tr("Force-refresh all live data on the dashboard (F5)"));
    connect(refresh_btn_, &QPushButton::clicked, this, &DashboardToolBar::refresh_clicked);
    rl->addWidget(refresh_btn_);

    add_btn_ = new QPushButton(tr("+ ADD"));
    add_btn_->setMinimumHeight(20);
    add_btn_->setObjectName("dtAddBtn");
    add_btn_->setToolTip(tr("Add a widget to the dashboard (Ctrl+N)"));
    connect(add_btn_, &QPushButton::clicked, this, &DashboardToolBar::add_widget_clicked);
    rl->addWidget(add_btn_);

    save_btn_ = new QPushButton(tr("SAVE"));
    save_btn_->setMinimumHeight(20);
    save_btn_->setObjectName("dtBtn");
    save_btn_->setToolTip(tr("Save the current widget layout (Ctrl+S)"));
    connect(save_btn_, &QPushButton::clicked, this, &DashboardToolBar::save_layout_clicked);
    rl->addWidget(save_btn_);

    reset_btn_ = new QPushButton(tr("RESET"));
    reset_btn_->setMinimumHeight(20);
    reset_btn_->setObjectName("dtResetBtn");
    reset_btn_->setToolTip(tr("Replace the current layout with a template"));
    connect(reset_btn_, &QPushButton::clicked, this, &DashboardToolBar::reset_layout_clicked);
    rl->addWidget(reset_btn_);

    // Accessible names + an explicit tab order: the terminal shipped with zero
    // of either, so the toolbar was unreachable/unannounced by keyboard.
    apply_accessibility();

    hl->addWidget(right);

    // Never inside theme_changed (restyling there stalls a theme switch): once the
    // appearance has settled, the sheet only when it changed.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() { refresh_theme(); });

    // P3: timer interval is set here, but start() is deferred to showEvent()
    // so the toolbar doesn't tick when the dashboard tab isn't visible.
    clock_timer_.setInterval(1000);
    connect(&clock_timer_, &QTimer::timeout, this, &DashboardToolBar::update_clock);

    refresh_theme();
}

void DashboardToolBar::fill_regions() {
    if (!region_combo_)
        return;
    filling_regions_ = true;
    auto& store = core::market::MarketProfileStore::instance();
    region_combo_->clear();
    for (const auto& p : core::market::all_market_profiles())
        region_combo_->addItem(core::market::market_profile_name(p), p.id);
    region_combo_->setCurrentIndex(std::max(0, region_combo_->findData(store.current_id())));
    region_combo_->setToolTip(
        store.is_user_choice()
            ? tr("Market region — sets the default indices, FX, commodities, sectors, movers and venues of every tile")
            : tr("Market region (inferred from your system locale) — sets the default indices, FX, commodities, "
                 "sectors, movers and venues of every tile"));
    filling_regions_ = false;
}

void DashboardToolBar::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    clock_timer_.stop();
}

void DashboardToolBar::apply_accessibility() {
    struct Named {
        QWidget* w;
        QString name;
    };
    const Named named[] = {
        {clock_btn_, tr("Clock — click to toggle UTC / local time")},
        {status_text_, tr("Feed status")},
        {widget_count_, tr("Widget count")},
        {compact_btn_, tr("Compact density")},
        {pulse_btn_, tr("Show or hide the Market Pulse panel")},
        {refresh_btn_, tr("Refresh all dashboard data")},
        {add_btn_, tr("Add widget")},
        {save_btn_, tr("Save layout")},
        {reset_btn_, tr("Reset layout from a template")},
        {region_combo_, tr("Market region")},
        {layouts_btn_, tr("Named layouts")},
    };
    for (const auto& n : named) {
        if (n.w)
            n.w->setAccessibleName(n.name);
    }

    // Left-to-right tab order across the action buttons.
    QWidget* chain[] = {clock_btn_,   region_combo_, layouts_btn_, compact_btn_, pulse_btn_,
                        refresh_btn_, add_btn_,      save_btn_,    reset_btn_};
    QWidget* prev = nullptr;
    for (QWidget* w : chain) {
        if (!w)
            continue;
        if (prev)
            setTabOrder(prev, w);
        prev = w;
    }
}

void DashboardToolBar::refresh_theme() {
    const QColor surface(ui::colors::BG_SURFACE());
    dashboard_paint::apply_solid_window_background(this, surface);
    dashboard_paint::apply_solid_window_background(left_container_, surface);
    dashboard_paint::apply_solid_window_background(right_container_, surface);

    const QString sheet =
        QString("#dashToolBar { background:%1; border-bottom:1px solid %2; }"
                "#dtLeftContainer, #dtRightContainer { background:%1; }"
                "#dtSep { color:%3; background:transparent; }"
                "#dtBrand { color:%4; font-weight:bold; letter-spacing:1px; background:transparent; }"
                "#dtSub { color:%5; font-weight:bold; background:transparent; }"
                "#dtStatus { color:%6; font-weight:bold; background:transparent; }"
                "#dtWidgetCount { color:%8; font-weight:bold; background:transparent; }"
                "#dtBtn { background:%9; border:1px solid %3; color:%7; padding:0 10px; font-weight:bold; }"
                "#dtBtn:hover { background:%10; color:%11; border-color:%12; }"
                "#dtBtn:checked { background:%15; color:%4; border-color:%13; }"
                "#dtAddBtn { background:%9; border:1px solid %13; color:%4; padding:0 10px; font-weight:bold; }"
                "#dtAddBtn:hover { background:%10; color:%11; border-color:%12; }"
                "#dtResetBtn { background:%9; border:1px solid %14; color:%14; padding:0 10px; font-weight:bold; }"
                "#dtResetBtn:hover { background:%10; color:%11; border-color:%12; }"
                "#dtRegion { background:%9; border:1px solid %3; color:%4; padding:0 6px; font-weight:bold; }"
                "#dtRegion:hover { border-color:%12; }")
            .arg(ui::colors::BG_SURFACE())     // %1
            .arg(ui::colors::BORDER_DIM())     // %2
            .arg(ui::colors::BORDER_MED())     // %3
            .arg(ui::colors::AMBER())          // %4
            .arg(ui::colors::TEXT_TERTIARY())  // %5
            .arg(ui::colors::SUCCESS())        // %6
            .arg(ui::colors::TEXT_SECONDARY()) // %7
            .arg(ui::colors::CYAN())           // %8
            .arg(ui::colors::BG_RAISED())      // %9
            .arg(ui::colors::BG_HOVER())       // %10
            .arg(ui::colors::TEXT_PRIMARY())   // %11
            .arg(ui::colors::BORDER_BRIGHT())  // %12
            .arg(ui::colors::AMBER_DIM())      // %13
            .arg(ui::colors::DANGER())         // %14
            .arg(ui::colors::ACCENT_BG());     // %15
    // setStyleSheet() re-styles the whole bar even for an identical sheet.
    if (styleSheet() == sheet)
        return;
    setStyleSheet(sheet);

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
    const QString suffix = clock_is_utc_ ? tr(" UTC") : tr(" LOC");
    const QDateTime now = clock_is_utc_ ? QDateTime::currentDateTimeUtc() : QDateTime::currentDateTime();
    clock_btn_->setText(now.toString("yyyy-MM-dd HH:mm:ss") + suffix);
}

void DashboardToolBar::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    fit_left_items();
}

void DashboardToolBar::fit_left_items() {
    if (!left_container_ || !right_container_ || !left_container_->layout())
        return;
    for (const auto& group : std::as_const(fold_groups_))
        for (auto* w : group)
            w->setVisible(true);
    const auto m = layout() ? layout()->contentsMargins() : QMargins();
    const int avail = width() - m.left() - m.right() - right_container_->sizeHint().width();
    for (const auto& group : std::as_const(fold_groups_)) {
        if (left_container_->layout()->sizeHint().width() <= avail)
            break;
        for (auto* w : group)
            w->setVisible(false);
    }
}

void DashboardToolBar::set_compact(bool compact) {
    compact_ = compact;
    if (compact_btn_)
        compact_btn_->setChecked(compact);
}

void DashboardToolBar::set_widget_count(int count) {
    widget_count_value_ = count;
    widget_count_->setText(tr("%1 WIDGETS").arg(count));
}

void DashboardToolBar::set_connected(bool connected) {
    connected_ = connected;
    status_text_->setText(connected ? tr("LIVE") : tr("OFFLINE"));
    // refresh_theme handles the base color; override just this label dynamically
    status_text_->setStyleSheet(QString("color:%1;font-weight:bold;background:transparent;")
                                    .arg(connected ? ui::colors::SUCCESS() : ui::colors::DANGER()));
}

void DashboardToolBar::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void DashboardToolBar::retranslateUi() {
    if (status_text_)
        status_text_->setText(connected_ ? tr("LIVE") : tr("OFFLINE"));
    if (clock_btn_)
        clock_btn_->setToolTip(tr("Click to toggle UTC / local time"));
    if (widget_count_)
        widget_count_->setText(tr("%1 WIDGETS").arg(widget_count_value_));
    if (compact_btn_)
        compact_btn_->setText(tr("COMPACT"));
    if (pulse_btn_)
        pulse_btn_->setText(tr("PULSE"));
    if (refresh_btn_) {
        refresh_btn_->setText(tr("REFRESH"));
        refresh_btn_->setToolTip(tr("Force-refresh all live data on the dashboard (F5)"));
    }
    if (add_btn_) {
        add_btn_->setText(tr("+ ADD"));
        add_btn_->setToolTip(tr("Add a widget to the dashboard (Ctrl+N)"));
    }
    if (save_btn_) {
        save_btn_->setText(tr("SAVE"));
        save_btn_->setToolTip(tr("Save the current widget layout (Ctrl+S)"));
    }
    if (reset_btn_) {
        reset_btn_->setText(tr("RESET"));
        reset_btn_->setToolTip(tr("Replace the current layout with a template"));
    }
    if (compact_btn_)
        compact_btn_->setToolTip(tr("Compact density (on: denser rows, the templates' design) — click for the roomier "
                                    "layout"));
    if (pulse_btn_)
        pulse_btn_->setToolTip(tr("Show / hide the Market Pulse panel"));
    if (layouts_btn_) {
        layouts_btn_->setText(tr("LAYOUTS"));
        layouts_btn_->setToolTip(tr("Named layouts: save, switch, export and import"));
    }
    fill_regions();
    apply_accessibility();
    update_clock();
}

} // namespace fincept::screens
