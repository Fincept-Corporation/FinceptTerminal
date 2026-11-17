// EquityWorkspace.cpp — see header. The responsive body of the Equity Trading screen.
#include "screens/equity_trading/EquityWorkspace.h"

#include "ui/theme/Theme.h"

#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QShortcut>
#include <QShowEvent>
#include <QSplitter>
#include <QTabBar>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace fincept::screens::equity {

namespace {

/// The folded watchlist: a narrow strip with a chevron and the vertical caption.
/// Click, Enter or Space unfolds it.
class eqws_Rail : public QWidget {
  public:
    explicit eqws_Rail(QWidget* parent, std::function<void()> on_open) : QWidget(parent), on_open_(std::move(on_open)) {
        setObjectName(QStringLiteral("eqWsRail"));
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::TabFocus);
        setAttribute(Qt::WA_Hover, true);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    }
    void set_caption(const QString& caption) {
        caption_ = caption;
        setAccessibleName(caption);
        update();
    }

  protected:
    void paintEvent(QPaintEvent*) override {
        namespace c = fincept::ui::colors;
        QPainter p(this);
        const bool hover = underMouse();
        p.fillRect(rect(), QColor(hover ? c::BG_HOVER() : c::BG_SURFACE()));
        p.setPen(QColor(c::BORDER_DIM()));
        p.drawLine(rect().topRight(), rect().bottomRight());
        const QFont f = fincept::ui::fonts::font(-3, QFont::Bold);
        const QFontMetrics fm(f);
        p.setFont(f);
        p.setPen(QColor(hover ? c::AMBER() : c::TEXT_SECONDARY()));
        // Chevron at the top (unfold to the right).
        const QRect top(0, 4, width(), fm.height() + 6);
        p.drawText(top, Qt::AlignCenter, QStringLiteral("\xE2\x80\xBA")); // ›
        // Caption, bottom-to-top.
        p.save();
        p.translate(width() / 2.0 + fm.ascent() / 2.0 - fm.descent() / 2.0, top.bottom() + 10);
        p.rotate(90);
        p.drawText(QPointF(0, 0), fm.elidedText(caption_, Qt::ElideRight, std::max(0, height() - top.bottom() - 16)));
        p.restore();
        if (hasFocus()) {
            p.setPen(QPen(QColor(c::AMBER_DIM()), 1, Qt::DotLine));
            p.drawRect(rect().adjusted(1, 1, -2, -2));
        }
    }
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton && rect().contains(e->position().toPoint()) && on_open_)
            on_open_();
    }
    void keyPressEvent(QKeyEvent* e) override {
        if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter || e->key() == Qt::Key_Space) && on_open_) {
            on_open_();
            return;
        }
        QWidget::keyPressEvent(e);
    }

  private:
    std::function<void()> on_open_;
    QString caption_;
};

QWidget* eqws_host(QWidget* parent, const char* name) {
    auto* w = new QWidget(parent);
    w->setObjectName(QLatin1String(name));
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    return w;
}

QString eqws_sheet() {
    namespace c = fincept::ui::colors;
    return QStringLiteral(
               "QWidget#eqWorkspace{background:%1;}"
               "QSplitter#eqWsMain::handle,QSplitter#eqWsCentre::handle{background:%2;}"
               "QTabBar#eqWsZoneTabs{background:%3;border:none;}"
               "QTabBar#eqWsZoneTabs::tab{background:transparent;color:%4;border:none;border-bottom:2px solid transparent;"
               "padding:5px 14px;font-size:%5px;font-weight:700;letter-spacing:0.8px;}"
               "QTabBar#eqWsZoneTabs::tab:selected{color:%6;border-bottom-color:%6;}"
               "QTabBar#eqWsZoneTabs::tab:hover:!selected{color:%7;}"
               "QWidget#eqWsZoneStrip{background:%3;border:none;border-bottom:1px solid %2;}"
               "QWidget#eqWsDrawerBar{background:%3;border:none;border-bottom:1px solid %2;}"
               "QLabel#eqWsDrawerTitle{color:%4;background:transparent;font-size:%8px;font-weight:700;letter-spacing:0.8px;}"
               "QToolButton#eqWsDrawerClose{color:%4;background:transparent;border:1px solid %2;padding:0px 6px;"
               "font-size:%5px;font-weight:700;}"
               "QToolButton#eqWsDrawerClose:hover{color:%6;border-color:%6;}")
        .arg(c::BG_BASE(), c::BORDER_DIM(), c::BG_SURFACE(), c::TEXT_SECONDARY())
        .arg(fincept::ui::fonts::px(-3))
        .arg(c::AMBER(), c::TEXT_PRIMARY())
        .arg(fincept::ui::fonts::px(-4));
}

QList<int> eqws_parse(const QVariant& v) {
    QList<int> out;
    for (const QString& part : v.toString().split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        bool ok = false;
        const int n = part.trimmed().toInt(&ok);
        if (!ok)
            return {};
        out << n;
    }
    return out;
}

QString eqws_join(const QList<int>& sizes) {
    QStringList parts;
    for (int s : sizes)
        parts << QString::number(s);
    return parts.join(QLatin1Char(','));
}

} // namespace

EquityWorkspace::EquityWorkspace(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("eqWorkspace"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(eqws_sheet()); // before any child exists

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Zone tabs (Compact only).
    auto* strip = new QWidget(this);
    strip->setObjectName(QStringLiteral("eqWsZoneStrip"));
    strip->setAttribute(Qt::WA_StyledBackground, true);
    strip->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed); // a header: never takes body height
    auto* sl = new QHBoxLayout(strip);
    sl->setContentsMargins(6, 0, 6, 0);
    sl->setSpacing(0);
    zone_tabs_ = new QTabBar(strip);
    zone_tabs_->setObjectName(QStringLiteral("eqWsZoneTabs"));
    zone_tabs_->setDrawBase(false);
    zone_tabs_->setExpanding(false);
    zone_tabs_->addTab(tr("CHART \xC2\xB7 BLOTTER"));
    zone_tabs_->addTab(tr("WATCHLIST"));
    zone_tabs_->addTab(tr("TICKET"));
    zone_tabs_->setAccessibleName(tr("Screen zones"));
    sl->addWidget(zone_tabs_);
    sl->addStretch(1);
    strip->hide();
    root->addWidget(strip);
    connect(zone_tabs_, &QTabBar::currentChanged, this, [this](int i) {
        compact_zone_ = std::clamp(i, 0, 2);
        if (plan_.single_column && !applying_)
            apply_visibility();
    });

    main_ = new QSplitter(Qt::Horizontal, this);
    main_->setObjectName(QStringLiteral("eqWsMain"));
    main_->setHandleWidth(1);
    main_->setChildrenCollapsible(false);
    root->addWidget(main_, 1);

    // LEFT: watchlist host + rail.
    left_zone_ = new QWidget(main_);
    left_zone_->setObjectName(QStringLiteral("eqWsLeft"));
    auto* ll = new QHBoxLayout(left_zone_);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(0);
    rail_ = new eqws_Rail(left_zone_, [this]() { set_watchlist_collapsed(false); });
    static_cast<eqws_Rail*>(rail_)->set_caption(tr("WATCHLIST"));
    rail_->setToolTip(tr("Show the watchlist (W)"));
    rail_->hide();
    ll->addWidget(rail_);
    watchlist_host_ = eqws_host(left_zone_, "eqWsWatchlistHost");
    ll->addWidget(watchlist_host_, 1);
    main_->addWidget(left_zone_);

    // CENTRE: chart over blotter.
    centre_ = new QSplitter(Qt::Vertical, main_);
    centre_->setObjectName(QStringLiteral("eqWsCentre"));
    centre_->setHandleWidth(1);
    centre_->setChildrenCollapsible(false);
    chart_host_ = eqws_host(centre_, "eqWsChartHost");
    centre_->addWidget(chart_host_);
    blotter_host_ = eqws_host(centre_, "eqWsBlotterHost");
    centre_->addWidget(blotter_host_);
    centre_->setStretchFactor(0, 1);
    centre_->setStretchFactor(1, 0);
    main_->addWidget(centre_);

    // RIGHT: drawer bar (Narrow) + quote / depth + ticket.
    right_zone_ = new QWidget(main_);
    right_zone_->setObjectName(QStringLiteral("eqWsRight"));
    auto* rl = new QVBoxLayout(right_zone_);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(0);
    drawer_bar_ = new QWidget(right_zone_);
    drawer_bar_->setObjectName(QStringLiteral("eqWsDrawerBar"));
    drawer_bar_->setAttribute(Qt::WA_StyledBackground, true);
    drawer_bar_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* dl = new QHBoxLayout(drawer_bar_);
    dl->setContentsMargins(8, 3, 6, 3);
    dl->setSpacing(6);
    drawer_title_ = new QLabel(tr("TRADE \xC2\xB7 Esc closes"), drawer_bar_);
    drawer_title_->setObjectName(QStringLiteral("eqWsDrawerTitle"));
    dl->addWidget(drawer_title_, 1);
    drawer_close_ = new QToolButton(drawer_bar_);
    drawer_close_->setObjectName(QStringLiteral("eqWsDrawerClose"));
    drawer_close_->setText(QStringLiteral("\xE2\x9C\x95")); // ✕
    drawer_close_->setCursor(Qt::PointingHandCursor);
    drawer_close_->setToolTip(tr("Close the order ticket (Esc)"));
    drawer_close_->setAccessibleName(tr("Close the order ticket"));
    connect(drawer_close_, &QToolButton::clicked, this, &EquityWorkspace::close_drawer);
    dl->addWidget(drawer_close_);
    drawer_bar_->hide();
    rl->addWidget(drawer_bar_);
    quote_host_ = eqws_host(right_zone_, "eqWsQuoteHost");
    rl->addWidget(quote_host_);
    ticket_host_ = eqws_host(right_zone_, "eqWsTicketHost");
    rl->addWidget(ticket_host_, 1);
    main_->addWidget(right_zone_);

    main_->setStretchFactor(0, 0);
    main_->setStretchFactor(1, 1);
    main_->setStretchFactor(2, 0);

    // Esc closes the drawer — scoped to the drawer, so it never takes Esc elsewhere.
    auto* esc = new QShortcut(QKeySequence(Qt::Key_Escape), right_zone_);
    esc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(esc, &QShortcut::activated, this, [this]() {
        if (drawer_open_)
            close_drawer();
    });

    connect(main_, &QSplitter::splitterMoved, this, [this](int, int) { remember_main_sizes(); });
    connect(centre_, &QSplitter::splitterMoved, this, [this](int, int) { remember_centre_sizes(); });
    // A font change moves the breakpoints and every floor: re-classify once styles are final.
    connect(&fincept::ui::ThemeManager::instance(), &fincept::ui::ThemeManager::appearance_settled, this,
            [this]() { apply(true); });
}

void EquityWorkspace::place(QWidget* host, QWidget* w, int stretch) {
    if (auto* box = qobject_cast<QBoxLayout*>(host ? host->layout() : nullptr))
        box->addWidget(w, stretch);
}

int EquityWorkspace::char_w() const {
    return std::max(5, QFontMetrics(fincept::ui::fonts::font()).averageCharWidth());
}

int EquityWorkspace::line_h() const {
    return std::max(12, QFontMetrics(fincept::ui::fonts::font()).height());
}

int EquityWorkspace::rail_width() const {
    return std::max(line_h() + 10, char_w() * 3);
}

void EquityWorkspace::set_watchlist_preferred_width(int px) {
    watchlist_pref_ = std::max(0, px);
}

void EquityWorkspace::set_right_minimum_width(int px) {
    right_min_ = std::max(0, px);
    if (classified_)
        update_minimums();
}

QList<int> EquityWorkspace::main_minimums() const {
    return {left_zone_->minimumWidth(), centre_->minimumWidth(), right_zone_->minimumWidth()};
}

void EquityWorkspace::update_minimums() {
    const int cw = char_w();
    const int lh = line_h();
    if (plan_.single_column) {
        // One zone at a time: it takes the whole width; floors would only overflow.
        left_zone_->setMinimumWidth(0);
        left_zone_->setMaximumWidth(QWIDGETSIZE_MAX);
        centre_->setMinimumWidth(0);
        right_zone_->setMinimumWidth(0);
    } else {
        if (rail_shown_) {
            left_zone_->setMinimumWidth(rail_width());
            left_zone_->setMaximumWidth(rail_width());
        } else {
            left_zone_->setMaximumWidth(QWIDGETSIZE_MAX);
            left_zone_->setMinimumWidth(cw * 20);
        }
        centre_->setMinimumWidth(cw * 34);
        right_zone_->setMinimumWidth(std::max(right_min_, cw * 30));
    }
    rail_->setFixedWidth(rail_width());
    chart_host_->setMinimumHeight(lh * 9);
    blotter_host_->setMinimumHeight(lh * 7);
}

void EquityWorkspace::apply(bool force) {
    if (width() <= 0)
        return;
    const EqWidthClass cls = eq_width_class(width(), fincept::ui::fonts::font_px(0));
    const bool changed = !classified_ || cls != plan_.cls;
    if (!changed && !force)
        return;
    plan_ = eq_layout_plan(cls);
    classified_ = true;
    rail_shown_ = !plan_.single_column && rail_choice_.value(key(), plan_.watchlist_rail);
    if (changed)
        drawer_open_ = false;
    applying_ = true;
    update_minimums();
    apply_visibility();
    applying_ = false;
    // Sizes need the new visibility laid out first (handles, zone widths).
    QTimer::singleShot(0, this, [this]() {
        applying_ = true;
        apply_main_sizes();
        apply_centre_sizes();
        applying_ = false;
    });
    if (changed)
        emit width_class_changed(cls);
}

void EquityWorkspace::apply_visibility() {
    const bool single = plan_.single_column;
    zone_tabs_->parentWidget()->setVisible(single);
    if (single) {
        if (zone_tabs_->currentIndex() != compact_zone_) {
            const bool was = applying_;
            applying_ = true;
            zone_tabs_->setCurrentIndex(compact_zone_);
            applying_ = was;
        }
        rail_->hide();
        watchlist_host_->show();
        drawer_bar_->hide();
        left_zone_->setVisible(compact_zone_ == int(Zone::Watchlist));
        centre_->setVisible(compact_zone_ == int(Zone::Centre));
        right_zone_->setVisible(compact_zone_ == int(Zone::Ticket));
        return;
    }
    left_zone_->show();
    centre_->show();
    rail_->setVisible(rail_shown_);
    watchlist_host_->setVisible(!rail_shown_);
    drawer_bar_->setVisible(plan_.right_drawer);
    right_zone_->setVisible(!plan_.right_drawer || drawer_open_);
}

void EquityWorkspace::apply_main_sizes() {
    if (plan_.single_column)
        return; // one zone: QSplitter gives it everything
    const QList<bool> shown = {left_zone_->isVisibleTo(this), centre_->isVisibleTo(this),
                               right_zone_->isVisibleTo(this)};
    int handles = -1;
    for (bool s : shown)
        handles += s ? 1 : 0;
    const int total = main_->width() - std::max(0, handles) * main_->handleWidth();
    if (total <= 0)
        return;
    const QList<int> mins = main_minimums();
    QList<int> sizes;
    if (!rail_shown_)
        sizes = eq_restore_sizes(main_sizes_.value(key()), mins, shown, total);
    if (sizes.isEmpty()) {
        const int cw = char_w();
        int left = 0;
        if (shown[0]) {
            if (rail_shown_)
                left = rail_width();
            else if (plan_.cls == EqWidthClass::Wide)
                left = std::clamp(watchlist_pref_, cw * plan_.watchlist_chars, cw * 40);
            else
                left = cw * std::max(20, plan_.watchlist_chars);
        }
        const int right = shown[2] ? std::max(right_min_, cw * plan_.right_chars) : 0;
        sizes = eq_fit_sizes({left, 0, right}, mins, 1, total);
    } else if (rail_shown_ && shown[0]) {
        sizes[1] += sizes[0] - rail_width();
        sizes[0] = rail_width();
    }
    main_->setSizes(sizes);
}

void EquityWorkspace::apply_centre_sizes() {
    const int total = centre_->height() - centre_->handleWidth();
    if (total <= 0)
        return;
    const QList<int> mins = {chart_host_->minimumHeight(), blotter_host_->minimumHeight()};
    QList<int> sizes = eq_restore_sizes(centre_sizes_.value(key()), mins, {true, true}, total);
    if (sizes.isEmpty()) {
        // Chart-dominant: the blotter takes ~38 % (never under its floor).
        const int blotter = std::max(mins[1], int(total * 0.38));
        sizes = eq_fit_sizes({0, blotter}, mins, 0, total);
    }
    centre_->setSizes(sizes);
}

void EquityWorkspace::remember_main_sizes() {
    if (applying_ || plan_.single_column || rail_shown_)
        return;
    main_sizes_.insert(key(), main_->sizes());
}

void EquityWorkspace::remember_centre_sizes() {
    if (applying_)
        return;
    centre_sizes_.insert(key(), centre_->sizes());
}

void EquityWorkspace::set_watchlist_collapsed(bool collapsed) {
    if (plan_.single_column) {
        if (!collapsed)
            reveal_zone(Zone::Watchlist);
        return;
    }
    if (collapsed == rail_shown_)
        return;
    rail_shown_ = collapsed;
    rail_choice_.insert(key(), collapsed);
    applying_ = true;
    update_minimums();
    apply_visibility();
    applying_ = false;
    QTimer::singleShot(0, this, [this]() {
        applying_ = true;
        apply_main_sizes();
        applying_ = false;
    });
    emit watchlist_collapsed_changed(collapsed);
}

void EquityWorkspace::reveal_ticket() {
    if (plan_.single_column) {
        reveal_zone(Zone::Ticket);
        return;
    }
    if (!plan_.right_drawer || drawer_open_)
        return;
    drawer_open_ = true;
    applying_ = true;
    apply_visibility();
    applying_ = false;
    QTimer::singleShot(0, this, [this]() {
        applying_ = true;
        apply_main_sizes();
        applying_ = false;
    });
    emit drawer_changed(true);
}

void EquityWorkspace::close_drawer() {
    if (!plan_.right_drawer || !drawer_open_)
        return;
    drawer_open_ = false;
    applying_ = true;
    apply_visibility();
    applying_ = false;
    emit drawer_changed(false);
}

bool EquityWorkspace::ticket_on_screen() const {
    return right_zone_->isVisibleTo(this);
}

void EquityWorkspace::reveal_zone(Zone zone) {
    if (plan_.single_column) {
        compact_zone_ = int(zone);
        zone_tabs_->setCurrentIndex(compact_zone_); // -> apply_visibility()
        return;
    }
    if (zone == Zone::Watchlist)
        set_watchlist_collapsed(false);
    else if (zone == Zone::Ticket)
        reveal_ticket();
}

QVariantMap EquityWorkspace::save_layout() const {
    QVariantMap main, centre, rail;
    for (auto it = main_sizes_.cbegin(); it != main_sizes_.cend(); ++it)
        main.insert(it.key(), eqws_join(it.value()));
    for (auto it = centre_sizes_.cbegin(); it != centre_sizes_.cend(); ++it)
        centre.insert(it.key(), eqws_join(it.value()));
    for (auto it = rail_choice_.cbegin(); it != rail_choice_.cend(); ++it)
        rail.insert(it.key(), it.value());
    return {{QStringLiteral("ws_main"), main},
            {QStringLiteral("ws_centre"), centre},
            {QStringLiteral("ws_rail"), rail},
            {QStringLiteral("ws_zone"), compact_zone_}};
}

void EquityWorkspace::restore_layout(const QVariantMap& state) {
    const QVariantMap main = state.value(QStringLiteral("ws_main")).toMap();
    const QVariantMap centre = state.value(QStringLiteral("ws_centre")).toMap();
    const QVariantMap rail = state.value(QStringLiteral("ws_rail")).toMap();
    for (auto it = main.cbegin(); it != main.cend(); ++it) {
        const QList<int> s = eqws_parse(it.value());
        if (s.size() == 3)
            main_sizes_.insert(it.key(), s);
    }
    for (auto it = centre.cbegin(); it != centre.cend(); ++it) {
        const QList<int> s = eqws_parse(it.value());
        if (s.size() == 2)
            centre_sizes_.insert(it.key(), s);
    }
    for (auto it = rail.cbegin(); it != rail.cend(); ++it)
        rail_choice_.insert(it.key(), it.value().toBool());
    compact_zone_ = std::clamp(state.value(QStringLiteral("ws_zone"), 0).toInt(), 0, 2);
    if (classified_)
        apply(true);
}

void EquityWorkspace::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    apply(false);
}

void EquityWorkspace::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!classified_)
        apply(true);
}

} // namespace fincept::screens::equity
