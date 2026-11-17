#include "screens/dashboard/MarketPulsePanel.h"

#include "core/events/EventBus.h"
#include "core/market/MarketProfile.h"
#include "core/market/MarketSessions.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/dashboard/widgets/DashboardFormat.h"
#include "screens/dashboard/widgets/LoadingOverlay.h"
#include "services/markets/MarketDataService.h"
#include "services/markets/RiskCompositeService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QDateTime>
#include <QFrame>
#include <QPalette>
#include <QSet>
#include <QShowEvent>
#include <QTimeZone>

#include <algorithm>

namespace fincept::screens {

// ── Symbols used by this panel ───────────────────────────────────────────────

// Breadth baskets, from the user's MarketProfile. Each is named for what it is
// (the region's large-cap basket, its sector set, the world's headline
// indices) — never presented as exchange-wide advance / decline.
struct PulseGroup {
    QString label;
    QStringList symbols;
};

static QVector<PulseGroup> pulse_breadth_groups() {
    const auto& p = core::market::MarketProfileStore::instance().current();
    const QString region = core::market::market_profile_name(p).toUpper();
    QStringList world;
    for (const auto& g : core::market::global_index_universe())
        world << g.symbol;
    const QStringList sectors = core::market::profile_symbols(p.sectors);
    return {{MarketPulsePanel::tr("%1 LARGE CAPS · %2").arg(region).arg(p.movers.size()), p.movers},
            {MarketPulsePanel::tr("%1 SECTORS · %2").arg(region).arg(sectors.size()), sectors},
            {MarketPulsePanel::tr("WORLD INDICES · %1").arg(world.size()), world}};
}

static QStringList pulse_breadth_symbols() {
    QStringList out;
    for (const auto& g : pulse_breadth_groups())
        for (const auto& s : g.symbols)
            if (!out.contains(s))
                out << s;
    return out;
}

// Movers: the region's large caps (gainers / losers rows).
static QStringList pulse_mover_symbols() {
    return core::market::MarketProfileStore::instance().current().movers;
}

// setStyleSheet() only when the sheet changed — the data-driven colours below
// are re-applied on every delivery burst / minute tick but rarely differ, and
// after an appearance change the theme manager has usually rewritten a sheet
// to exactly the new text already (so the live sheet is what is compared).
static void pulse_set_css_cached(QWidget* w, const QString& css) {
    if (!w || w->styleSheet() == css)
        return;
    w->setProperty("fincept_pulse_css", css);
    w->setStyleSheet(css);
}

static QString pulse_scroll_css() {
    return QString("QScrollArea{border:none;background:transparent;}"
                   "QScrollBar:vertical{width:6px;background:transparent;}"
                   "QScrollBar::handle:vertical{background:%1;border-radius:3px;min-height:20px;}"
                   "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
        .arg(ui::colors::BORDER_MED());
}

// Global snapshot symbols (global context whatever the region)
static const QStringList kSnapshotSymbols = services::MarketDataService::global_snapshot_symbols();

// ── Constructor ──────────────────────────────────────────────────────────────

MarketPulsePanel::MarketPulsePanel(QWidget* parent) : QWidget(parent) {
    setMinimumWidth(180);
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(true);
    // Constrain preferred width so the QSplitter doesn't give this panel
    // the majority of space before the user sees the dashboard.
    // The user can still drag the splitter handle wider if needed.
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    // The root sheet goes on before any child exists, so each child is styled
    // once, at creation (refresh_theme() below then skips the unchanged sheet).
    root_css_ = QString("background:%1;").arg(ui::colors::PANEL());
    setStyleSheet(root_css_);

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_header());

    scroll_area_ = new QScrollArea(this);
    scroll_area_->setWidgetResizable(true);
    // Styled now, before the sections move in (refresh_theme() then finds the
    // sheet unchanged and leaves it, instead of re-polishing every section).
    pulse_set_css_cached(scroll_area_, pulse_scroll_css());

    auto* content = new QWidget(this);
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(0);

    cl->addWidget(build_fear_greed_section());
    cl->addWidget(build_breadth_section());
    cl->addWidget(build_gainers_section());
    cl->addWidget(build_losers_section());
    cl->addWidget(build_global_snapshot_section());
    cl->addWidget(build_market_hours_section());
    cl->addStretch();

    scroll_area_->setWidget(content);
    vl->addWidget(scroll_area_, 1);

    // ── Loading overlay ──
    // Layered over the scroll area so the user sees an animated "LOADING X
    // / Y" while quotes for the breadth/movers/snapshot symbol union are
    // streaming in. Denominator is the unique union size, computed once.
    loading_overlay_ = new widgets::LoadingOverlay(scroll_area_);
    loading_overlay_->attach_to(scroll_area_);

    // ── Timers ──

    hours_timer_ = new QTimer(this);
    hours_timer_->setInterval(30000); // market state + countdowns
    connect(hours_timer_, &QTimer::timeout, this, &MarketPulsePanel::refresh_market_hours);
    // Baskets, movers and venues follow the user's market region.
    connect(&core::market::MarketProfileStore::instance(), &core::market::MarketProfileStore::profile_changed, this,
            [this]() { on_profile_changed(); });

    // Coalesces a burst of ~70 per-symbol hub deliveries into one render pass.
    render_timer_ = new QTimer(this);
    render_timer_->setSingleShot(true);
    render_timer_->setInterval(0);
    connect(render_timer_, &QTimer::timeout, this, &MarketPulsePanel::flush_render);

    // Never inside theme_changed (restyling there stalls a theme switch): once the
    // appearance has settled, each sheet only when its text changed.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() { refresh_theme(); });
    refresh_theme();
}

// ── Theme refresh ────────────────────────────────────────────────────────────

void MarketPulsePanel::refresh_theme() {
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(ui::colors::PANEL()));
    pal.setColor(QPalette::Base, QColor(ui::colors::PANEL()));
    setPalette(pal);

    // ── Root panel ──
    // Only when it changed: re-setting the root sheet re-polishes every child of
    // the panel (most of a ~1.6 s stall when the constructor ended with it).
    const QString root_css = QString("background:%1;").arg(ui::colors::PANEL());
    if (styleSheet() != root_css) {
        root_css_ = root_css;
        pulse_set_css_cached(this, root_css);
    }

    // ── Header bar ──
    if (header_bar_)
        pulse_set_css_cached(header_bar_, QString("background: %1; border-bottom: 1px solid %2;")
                                       .arg(ui::colors::BG_RAISED(), ui::colors::AMBER_DIM()));
    if (header_icon_)
        pulse_set_css_cached(header_icon_, 
            QString("color: %1; font-size: " + fincept::ui::fonts::px(-2) + "px; background: transparent;").arg(ui::colors::AMBER()));
    if (header_title_)
        pulse_set_css_cached(header_title_, 
            QString("color: %1; font-size: " + fincept::ui::fonts::px(-4) + "px; font-weight: bold; letter-spacing: 1px; background: transparent;")
                .arg(ui::colors::AMBER()));
    live_dot_css_.clear(); // theme colours changed: re-apply the dot's state colour
    apply_live_dot();

    // ── Scroll area ── (cached: re-setting it re-polishes every section)
    if (scroll_area_)
        pulse_set_css_cached(scroll_area_, pulse_scroll_css());

    // ── Section headers ──
    auto style_section = [](SectionHeader& sh, const QString& icon_color) {
        if (sh.container)
            pulse_set_css_cached(sh.container, 
                QString("background: %1; border-bottom: 1px solid %2; border-top: 1px solid %2;")
                    .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
        if (sh.icon)
            pulse_set_css_cached(sh.icon, QString("color: %1; font-size: " + fincept::ui::fonts::px(-4) + "px; background: transparent;").arg(icon_color));
        if (sh.title)
            pulse_set_css_cached(sh.title, 
                QString("color: %1; font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: bold; letter-spacing: 0.5px; background: transparent;")
                    .arg(ui::colors::TEXT_SECONDARY()));
    };

    style_section(sh_breadth_, ui::colors::CYAN());
    style_section(sh_gainers_, ui::colors::POSITIVE());
    style_section(sh_losers_, ui::colors::NEGATIVE());
    style_section(sh_snapshot_, ui::colors::INFO());
    style_section(sh_hours_, ui::colors::WARNING());

    // ── Fear & Greed ──
    if (fg_header_label_)
        pulse_set_css_cached(fg_header_label_, 
            QString("color: %1; font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: bold; letter-spacing: 0.5px; background: transparent;")
                .arg(ui::colors::TEXT_SECONDARY()));
    if (fg_gauge_icon_)
        pulse_set_css_cached(fg_gauge_icon_, 
            QString("color: %1; font-size: " + fincept::ui::fonts::px(-2) + "px; background: transparent;").arg(ui::colors::AMBER()));
    if (fg_gradient_bar_)
        pulse_set_css_cached(fg_gradient_bar_, QString("QFrame { border-radius: 3px; "
                                                "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
                                                "stop:0 %1, stop:0.25 %2, stop:0.5 %3, stop:0.75 %4, stop:1 %5); }")
                                            .arg(ui::colors::NEGATIVE(), ui::colors::AMBER(), ui::colors::WARNING(),
                                                 ui::colors::POSITIVE_DIM(), ui::colors::POSITIVE()));
    if (fg_score_val_)
        pulse_set_css_cached(fg_score_val_, QString("color: %1; font-size: " + fincept::ui::fonts::px(4) + "px; font-weight: bold; background: transparent;")
                                         .arg(ui::colors::TEXT_TERTIARY()));
    if (fg_score_max_)
        pulse_set_css_cached(fg_score_max_, 
            QString("color: %1; font-size: " + fincept::ui::fonts::px(-5) + "px; background: transparent;").arg(ui::colors::TEXT_TERTIARY()));
    if (fg_sentiment_)
        pulse_set_css_cached(fg_sentiment_, 
            QString("color: %1; font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: bold; letter-spacing: 0.5px; background: transparent;")
                .arg(ui::colors::TEXT_TERTIARY()));

    // ── Market Breadth rows ──
    auto style_breadth = [](BreadthRow& row) {
        if (row.name)
            pulse_set_css_cached(row.name, QString("color: %1; font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: bold; background: transparent;")
                                        .arg(ui::colors::TEXT_SECONDARY()));
        if (row.adv)
            pulse_set_css_cached(row.adv, 
                QString("color: %1; font-size: " + fincept::ui::fonts::px(-6) + "px; background: transparent;").arg(ui::colors::POSITIVE()));
        if (row.slash)
            pulse_set_css_cached(row.slash, 
                QString("color: %1; font-size: " + fincept::ui::fonts::px(-6) + "px; background: transparent;").arg(ui::colors::TEXT_TERTIARY()));
        if (row.dec)
            pulse_set_css_cached(row.dec, 
                QString("color: %1; font-size: " + fincept::ui::fonts::px(-6) + "px; background: transparent;").arg(ui::colors::NEGATIVE()));
        if (row.green)
            pulse_set_css_cached(row.green, QString("background: %1; border-radius: 0;").arg(ui::colors::POSITIVE()));
        if (row.red)
            pulse_set_css_cached(row.red, QString("background: %1; border-radius: 0;").arg(ui::colors::NEGATIVE()));
    };

    style_breadth(nyse_row_);
    style_breadth(nasdaq_row_);
    style_breadth(sp500_row_);

    // ── Global Snapshot rows ──
    // Each stat row has a fixed val_color that maps to a specific token.
    // Re-resolve them here so theme changes take effect.
    struct StatDef {
        StatRow& row;
        QString val_color;
    };
    StatDef stat_defs[] = {
        {vix_row_, ui::colors::WARNING()},  {us10y_row_, ui::colors::CYAN()}, {dxy_row_, ui::colors::CYAN()},
        {gold_row_, ui::colors::WARNING()}, {oil_row_, ui::colors::CYAN()},   {btc_row_, ui::colors::AMBER()},
    };

    for (auto& sd : stat_defs) {
        if (sd.row.container)
            pulse_set_css_cached(sd.row.container, QString("border-bottom: 1px solid %1;").arg(ui::colors::BORDER_DIM()));
        if (sd.row.name_lbl)
            pulse_set_css_cached(sd.row.name_lbl, 
                QString("color: %1; font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: bold; letter-spacing: 0.3px; background: transparent;")
                    .arg(ui::colors::TEXT_SECONDARY()));
        if (sd.row.val)
            pulse_set_css_cached(sd.row.val, 
                QString("color: %1; font-size: " + fincept::ui::fonts::px(-4) + "px; font-weight: bold; background: transparent;").arg(sd.val_color));
        if (sd.row.chg)
            sd.row.chg->setProperty("fincept_pulse_css", QVariant());
        if (sd.row.chg)
            pulse_set_css_cached(sd.row.chg, QString("color: %1; font-size: " + fincept::ui::fonts::px(-6) + "px; font-weight: bold; background: transparent;")
                                          .arg(ui::colors::TEXT_TERTIARY()));
    }

    // ── Market Hours rows ──
    for (auto& hr : hours_rows_) {
        if (hr.container)
            pulse_set_css_cached(hr.container, QString("border-bottom: 1px solid %1;").arg(ui::colors::BORDER_DIM()));
        if (hr.name_lbl)
            pulse_set_css_cached(hr.name_lbl, QString("color: %1; font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: bold; background: transparent;")
                                           .arg(ui::colors::TEXT_SECONDARY()));
        // dot and status colors are data-driven (open/closed/pre) —
        // refresh_market_hours() handles those, call it to re-resolve tokens.
    }

    // Re-resolve data-driven status dot/label colors.
    refresh_market_hours();

    // Mover rows keep their chrome in one stylesheet per section.
    style_mover_section(gainers_rows_, /*positive_section=*/true);
    style_mover_section(losers_rows_, /*positive_section=*/false);

    // Force the Fear & Greed labels to re-apply against the new tokens.
    fg_applied_color_.clear();

    if (isVisible()) {
        rebuild_breadth_from_cache();
        rebuild_movers_from_cache();
        rebuild_snapshot_from_cache();
    }
}

void MarketPulsePanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    refresh_theme();
    if (!hub_active_)
        hub_subscribe_all();
    services::RiskCompositeService::instance().attach(this);
    render_risk();
    refresh_market_hours();
    hours_timer_->start();
}

void MarketPulsePanel::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    hub_unsubscribe_all();
    services::RiskCompositeService::instance().detach(this);
    hours_timer_->stop();
    if (render_timer_)
        render_timer_->stop();
}

void MarketPulsePanel::schedule_render() {
    if (render_timer_ && !render_timer_->isActive())
        render_timer_->start();
}

void MarketPulsePanel::flush_render() {
    if (breadth_dirty_) {
        breadth_dirty_ = false;
        rebuild_breadth_from_cache();
    }
    if (movers_dirty_) {
        movers_dirty_ = false;
        rebuild_movers_from_cache();
    }
    if (snapshot_dirty_) {
        snapshot_dirty_ = false;
        rebuild_snapshot_from_cache();
    }
    update_loading_progress();
}

// ── Header ───────────────────────────────────────────────────────────────────

QWidget* MarketPulsePanel::build_header() {
    header_bar_ = new QWidget(this);
    header_bar_->setFixedHeight(30);
    // Styling applied by refresh_theme()

    auto* hl = new QHBoxLayout(header_bar_);
    hl->setContentsMargins(12, 0, 12, 0);

    header_icon_ = new QLabel(QChar(0x25C8));
    hl->addWidget(header_icon_);

    header_title_ = new QLabel(tr("MARKET PULSE"));
    hl->addWidget(header_title_);
    hl->addStretch();

    header_live_dot_ = new QLabel;
    header_live_dot_->setFixedSize(6, 6);
    hl->addWidget(header_live_dot_);

    return header_bar_;
}

QWidget* MarketPulsePanel::build_section_header(const QString& title, const QString& icon_char, const QString& color) {
    Q_UNUSED(color)
    auto* w = new QWidget(this);
    w->setFixedHeight(26);
    // Styling applied by refresh_theme() via style_section lambda

    auto* hl = new QHBoxLayout(w);
    hl->setContentsMargins(12, 0, 12, 0);

    auto* icn = new QLabel(icon_char);
    hl->addWidget(icn);

    auto* lbl = new QLabel(tr(title.toUtf8().constData()));
    hl->addWidget(lbl);
    hl->addStretch();

    // Title is the English source key — keep it intact for retranslateUi().
    // Comparison must match the *source* string, not the translated one.
    SectionHeader* sh = nullptr;
    if (title == "BASKET BREADTH")
        sh = &sh_breadth_;
    else if (title == "TOP GAINERS")
        sh = &sh_gainers_;
    else if (title == "TOP LOSERS")
        sh = &sh_losers_;
    else if (title == "GLOBAL SNAPSHOT")
        sh = &sh_snapshot_;
    else if (title == "MARKET HOURS")
        sh = &sh_hours_;

    if (sh) {
        sh->container = w;
        sh->icon = icn;
        sh->title = lbl;
        sh->source_key = title;
    }

    return w;
}

// ── Fear & Greed ─────────────────────────────────────────────────────────────

QWidget* MarketPulsePanel::build_fear_greed_section() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(12, 8, 12, 8);
    vl->setSpacing(6);

    // Header row
    auto* header_row = new QWidget(this);
    auto* hrl = new QHBoxLayout(header_row);
    hrl->setContentsMargins(0, 0, 0, 0);

    // Home-made composite — not CNN's Fear & Greed Index, so not named after it.
    // The app-wide documented composite (RiskCompositeService), not a basket score.
    fg_header_label_ = new QLabel(tr("RISK-ON / RISK-OFF"));
    fg_header_label_->setToolTip(services::RiskCompositeService::methodology());
    connect(&services::RiskCompositeService::instance(), &services::RiskCompositeService::updated, this,
            [this]() { render_risk(); });
    hrl->addWidget(fg_header_label_);
    hrl->addStretch();

    fg_gauge_icon_ = new QLabel(QChar(0x25CE));
    hrl->addWidget(fg_gauge_icon_);

    vl->addWidget(header_row);

    // Gradient bar (red→green, themed)
    fg_gradient_bar_ = new QFrame;
    fg_gradient_bar_->setFixedHeight(6);
    vl->addWidget(fg_gradient_bar_);

    // Score row
    auto* score_row = new QWidget(this);
    auto* srl = new QHBoxLayout(score_row);
    srl->setContentsMargins(0, 0, 0, 0);

    fg_score_val_ = new QLabel("--");
    srl->addWidget(fg_score_val_);

    fg_score_max_ = new QLabel("/100");
    srl->addWidget(fg_score_max_);

    srl->addStretch();

    fg_sentiment_key_ = QStringLiteral("LOADING...");
    fg_sentiment_ = new QLabel(tr("LOADING..."));
    srl->addWidget(fg_sentiment_);
    // All styling applied by refresh_theme()

    vl->addWidget(score_row);
    return w;
}

// ── Market Breadth ────────────────────────────────────────────────────────────

QWidget* MarketPulsePanel::build_breadth_section() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // A breadth PROXY over the 50-name basket above — not exchange A/D (NYSE
    // alone lists ~3,000 issues). Header and rows say so.
    vl->addWidget(build_section_header("BASKET BREADTH", QChar(0x2593), ui::colors::CYAN()));

    auto* bars = new QWidget(this);
    auto* bl = new QVBoxLayout(bars);
    bl->setContentsMargins(0, 6, 0, 6);
    bl->setSpacing(0);

    auto make_row = [&](const QString& name, BreadthRow& row) {
        auto* rw = new QWidget(this);
        auto* rl = new QVBoxLayout(rw);
        rl->setContentsMargins(12, 4, 12, 4);
        rl->setSpacing(3);

        auto* top = new QWidget(this);
        auto* tl = new QHBoxLayout(top);
        tl->setContentsMargins(0, 0, 0, 0);

        row.name = new QLabel(name);
        tl->addWidget(row.name);
        tl->addStretch();

        row.adv = new QLabel("--");
        tl->addWidget(row.adv);

        row.slash = new QLabel("/");
        tl->addWidget(row.slash);

        row.dec = new QLabel("--");
        tl->addWidget(row.dec);
        // All label styling applied by refresh_theme() via style_breadth lambda

        rl->addWidget(top);

        auto* bar_container = new QWidget(this);
        bar_container->setFixedHeight(4);
        auto* bar_layout = new QHBoxLayout(bar_container);
        bar_layout->setContentsMargins(0, 0, 0, 0);
        bar_layout->setSpacing(0);

        auto* green = new QFrame;
        bar_layout->addWidget(green, 1);
        row.green = green;

        auto* red = new QFrame;
        bar_layout->addWidget(red, 1);
        row.red = red;
        // Bar styling applied by refresh_theme() via style_breadth lambda

        rl->addWidget(bar_container);
        bl->addWidget(rw);
    };

    // Rows are relabelled from the region's baskets in rebuild_breadth_from_cache().
    const auto groups = pulse_breadth_groups();
    make_row(groups.value(0).label, nyse_row_);
    make_row(groups.value(1).label, nasdaq_row_);
    make_row(groups.value(2).label, sp500_row_);
    bars->setToolTip(tr("Up / down today within three named baskets: your region's large caps, its sector set and "
                        "the world's headline indices. Not exchange-wide breadth — no source in the terminal "
                        "publishes advancers / decliners for a whole exchange."));

    vl->addWidget(bars);
    return w;
}

// ── Top Movers ────────────────────────────────────────────────────────────────

// Builds ONE reusable mover row. All styling comes from the section-level
// stylesheet installed by style_mover_section() — no per-row setStyleSheet.
MarketPulsePanel::MoverRow MarketPulsePanel::make_mover_row(QWidget* parent, QVBoxLayout* into, bool /*positive*/) {
    MoverRow r;
    r.container = new QWidget(parent);
    r.container->setObjectName("pulseMoverRow");
    // Double-click opens the row's symbol (see eventFilter()). The symbol itself
    // is stamped on the container by fill_mover_row().
    r.container->setCursor(Qt::PointingHandCursor);
    r.container->installEventFilter(this);

    auto* hl = new QHBoxLayout(r.container);
    hl->setContentsMargins(12, 5, 12, 5);
    hl->setSpacing(4);

    r.symbol = new QLabel(r.container);
    r.symbol->setObjectName("pulseMoverSym");
    hl->addWidget(r.symbol);
    hl->addStretch();

    r.arrow = new QLabel(r.container);
    r.arrow->setObjectName("pulseMoverArrow");
    hl->addWidget(r.arrow);

    r.change = new QLabel(r.container);
    r.change->setObjectName("pulseMoverChg");
    hl->addWidget(r.change);

    r.volume = new QLabel(r.container);
    r.volume->setObjectName("pulseMoverVol");
    hl->addWidget(r.volume);

    into->addWidget(r.container);
    return r;
}

void MarketPulsePanel::fill_mover_row(const MoverRow& row, const QString& symbol, double change,
                                      const QString& volume) {
    if (!row.container)
        return;
    row.container->setVisible(true);
    row.container->setProperty("pulse_symbol", symbol);
    row.container->setToolTip(tr("Double-click to open %1 in Equity Research").arg(symbol));
    row.symbol->setText(symbol);
    row.arrow->setText(change >= 0 ? QString(QChar(0x25B2)) : QString(QChar(0x25BC)));
    row.change->setText(QString("%1%2%").arg(change >= 0 ? "+" : "").arg(change, 0, 'f', 2));
    row.volume->setText(volume.isEmpty() ? QString() : tr("VOL: %1").arg(volume));
}

void MarketPulsePanel::clear_mover_row(const MoverRow& row) {
    if (row.container)
        row.container->setVisible(false);
}

void MarketPulsePanel::style_mover_section(QWidget* container, bool positive_section) {
    if (!container)
        return;
    const QString accent = positive_section ? ui::colors::POSITIVE() : ui::colors::NEGATIVE();
    container->setStyleSheet(
        QString("QWidget#pulseMoverRow{border-bottom:1px solid %1;}"
                "QWidget#pulseMoverRow QLabel{background:transparent;}"
                "QLabel#pulseMoverSym{color:%2;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:bold;}"
                "QLabel#pulseMoverArrow{color:%3;font-size:" + fincept::ui::fonts::px(-6) + "px;}"
                "QLabel#pulseMoverChg{color:%3;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:bold;}"
                "QLabel#pulseMoverVol{color:%4;font-size:" + fincept::ui::fonts::px(-6) + "px;}")
            .arg(ui::colors::BORDER_DIM(), ui::colors::TEXT_PRIMARY(), accent, ui::colors::TEXT_TERTIARY()));
}

static QString format_volume(double vol) {
    if (vol >= 1e9)
        return QString("%1B").arg(vol / 1e9, 0, 'f', 1);
    if (vol >= 1e6)
        return QString("%1M").arg(vol / 1e6, 0, 'f', 1);
    if (vol >= 1e3)
        return QString("%1K").arg(vol / 1e3, 0, 'f', 1);
    return QString::number(static_cast<int>(vol));
}

QWidget* MarketPulsePanel::build_gainers_section() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_section_header("TOP GAINERS", QChar(0x2191), ui::colors::POSITIVE()));

    gainers_rows_ = new QWidget(this);
    gainers_layout_ = new QVBoxLayout(gainers_rows_);
    gainers_layout_->setContentsMargins(0, 0, 0, 0);
    gainers_layout_->setSpacing(0);

    for (int i = 0; i < kMoverRows; ++i) {
        MoverRow r = make_mover_row(gainers_rows_, gainers_layout_, /*positive_section=*/true);
        // Placeholder text until the first quotes land.
        r.symbol->setText(QStringLiteral("..."));
        gainer_rows_.append(r);
    }
    style_mover_section(gainers_rows_, /*positive_section=*/true);

    vl->addWidget(gainers_rows_);
    return w;
}

QWidget* MarketPulsePanel::build_losers_section() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_section_header("TOP LOSERS", QChar(0x2193), ui::colors::NEGATIVE()));

    losers_rows_ = new QWidget(this);
    losers_layout_ = new QVBoxLayout(losers_rows_);
    losers_layout_->setContentsMargins(0, 0, 0, 0);
    losers_layout_->setSpacing(0);

    for (int i = 0; i < kMoverRows; ++i) {
        MoverRow r = make_mover_row(losers_rows_, losers_layout_, /*positive_section=*/false);
        r.symbol->setText(QStringLiteral("..."));
        loser_rows_.append(r);
    }
    style_mover_section(losers_rows_, /*positive_section=*/false);

    vl->addWidget(losers_rows_);
    return w;
}

// ── Global Snapshot ───────────────────────────────────────────────────────────

QWidget* MarketPulsePanel::build_global_snapshot_section() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_section_header("GLOBAL SNAPSHOT", QChar(0x25CB), ui::colors::INFO()));

    // Build stat rows — colors are applied by refresh_theme()
    struct RowDef {
        const char* label;
        StatRow& row;
        const char* symbol; // quote symbol the row is fed from (see rebuild_snapshot_from_cache)
    };
    RowDef defs[] = {
        {"VIX", vix_row_, "^VIX"},       {"US 10Y", us10y_row_, "^TNX"}, {"DXY", dxy_row_, "DX-Y.NYB"},
        {"GOLD", gold_row_, "GC=F"},     {"OIL WTI", oil_row_, "CL=F"},  {"BTC", btc_row_, "BTC-USD"},
    };

    for (auto& d : defs) {
        auto* rw = new QWidget(this);
        rw->setProperty("pulse_symbol", QString::fromLatin1(d.symbol));
        rw->setToolTip(tr("Double-click to open %1 in Equity Research").arg(QString::fromLatin1(d.symbol)));
        rw->setCursor(Qt::PointingHandCursor);
        rw->installEventFilter(this);
        auto* hl = new QHBoxLayout(rw);
        hl->setContentsMargins(12, 4, 12, 4);

        auto* lbl = new QLabel(d.label);
        hl->addWidget(lbl);
        hl->addStretch();

        d.row.container = rw;
        d.row.name_lbl = lbl;

        d.row.val = new QLabel("--");
        hl->addWidget(d.row.val);

        d.row.chg = new QLabel("");
        hl->addWidget(d.row.chg);
        // All styling applied by refresh_theme()

        vl->addWidget(rw);
    }

    return w;
}

// ── Market Hours ─────────────────────────────────────────────────────────────

QWidget* MarketPulsePanel::build_market_hours_section() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_section_header("MARKET HOURS", QChar(0x26A1), ui::colors::WARNING()));

    hours_content_ = new QWidget(this);
    auto* cl = new QVBoxLayout(hours_content_);
    cl->setContentsMargins(12, 6, 12, 6);
    cl->setSpacing(0);
    rebuild_hours_rows();

    vl->addWidget(hours_content_);
    return w;
}

void MarketPulsePanel::rebuild_hours_rows() {
    if (!hours_content_)
        return;
    auto* cl = qobject_cast<QVBoxLayout*>(hours_content_->layout());
    for (auto& hr : hours_rows_)
        if (hr.container)
            hr.container->deleteLater();
    hours_rows_.clear();

    // The region's venues first, then the world's main ones (MarketSessions ids).
    QStringList venues = core::market::MarketProfileStore::instance().current().venues;
    for (const char* v : {"NYSE", "LSE", "XETRA", "TSE", "HKEX", "NSE"})
        if (!venues.contains(QLatin1String(v)))
            venues << QString::fromLatin1(v);
    venues = venues.mid(0, 7);

    for (const QString& id : venues) {
        const auto* venue = core::market::venue_by_code(id);
        if (!venue)
            continue;
        auto* row = new QWidget(hours_content_);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 3, 0, 3);

        auto* name = new QLabel(venue->id, row);
        name->setToolTip(core::market::venue_display_name(*venue));
        rl->addWidget(name);
        rl->addStretch();

        HoursRow hr;
        hr.container = row;
        hr.name_lbl = name;
        hr.region = venue->id; // a MarketSessions venue id
        hr.name_source_key = venue->id;

        hr.dot = new QLabel(row);
        hr.dot->setFixedSize(5, 5);
        rl->addWidget(hr.dot);

        hr.status = new QLabel(row);
        rl->addWidget(hr.status);
        // Same chrome refresh_theme() gives the rows (it re-applies on theme change).
        row->setStyleSheet(QString("border-bottom: 1px solid %1;").arg(ui::colors::BORDER_DIM()));
        name->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold; background: transparent;")
                                .arg(ui::colors::TEXT_SECONDARY())
                                .arg(ui::fonts::px(-5)));
        hours_rows_.append(hr);
        cl->addWidget(row);
    }
}

void MarketPulsePanel::on_profile_changed() {
    breadth_cache_.clear();
    movers_cache_.clear();
    if (hub_active_) {
        hub_unsubscribe_all();
        hub_subscribe_all();
        refresh_data();
    }
    rebuild_hours_rows();
    refresh_market_hours();
    rebuild_breadth_from_cache();
}

// ── Market status helper ─────────────────────────────────────────────────────

namespace {

/// "45m", "3h 12m", "2d 4h".
QString pulse_countdown(qint64 secs) {
    const qint64 m = std::max<qint64>(1, secs / 60);
    if (m < 60)
        return QStringLiteral("%1m").arg(m);
    const qint64 h = m / 60;
    if (h < 48)
        return QStringLiteral("%1h %2m").arg(h).arg(m % 60, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1d %2h").arg(h / 24).arg(h % 24);
}

} // namespace

QString MarketPulsePanel::market_status(const QString& venue_id) {
    // English source key from the exchange calendar (core/market/MarketSessions:
    // DST, lunch breaks, holidays where the calendar covers the venue).
    const auto* v = core::market::venue_by_code(venue_id);
    if (!v)
        return QStringLiteral("CLOSED");
    return core::market::session_state_key(core::market::session_status(*v, QDateTime::currentDateTimeUtc()).state);
}

// ── Refresh ───────────────────────────────────────────────────────────────────

void MarketPulsePanel::refresh_market_hours() {
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (auto& hr : hours_rows_) {
        const auto* v = core::market::venue_by_code(hr.region);
        if (!v || !hr.status)
            continue;
        const auto st = core::market::session_status(*v, now);
        // Session state, not price direction: the status colours.
        const QString color = st.state == core::market::SessionState::Open ? ui::colors::SUCCESS()
                              : (st.state == core::market::SessionState::Pre || st.state == core::market::SessionState::Post ||
                                 st.state == core::market::SessionState::Lunch || st.state == core::market::SessionState::Holiday)
                                  ? ui::colors::WARNING()
                                  : ui::colors::TEXT_TERTIARY();
        pulse_set_css_cached(hr.dot, QString("background: %1; border-radius: 2px;").arg(color));
        QString text = core::market::session_state_label(st.state);
        if (st.next_change_utc.isValid())
            text += QStringLiteral(" · ") + pulse_countdown(now.secsTo(st.next_change_utc));
        hr.status->setText(text);
        QString tip = core::market::venue_display_name(*v) + QStringLiteral("\n") +
                      tr("Local time %1").arg(st.venue_local_now.toString(QStringLiteral("HH:mm")));
        if (st.next_change_utc.isValid())
            tip += QStringLiteral("\n") + tr("%1 in %2").arg(core::market::session_state_label(st.next_state),
                                                             pulse_countdown(now.secsTo(st.next_change_utc)));
        const QString hol = core::market::holiday_name(*v, st.trading_date);
        if (!hol.isEmpty())
            tip += QStringLiteral("\n") + hol;
        hr.status->setToolTip(tip);
        pulse_set_css_cached(hr.status,
            QString("color: %1; font-size: " + fincept::ui::fonts::px(-6) + "px; font-weight: bold; background: transparent;").arg(color));
    }
}

void MarketPulsePanel::rebuild_breadth_from_cache() {

    // Advancers / decliners (up / down today) per named basket.
    const auto groups = pulse_breadth_groups();
    auto count = [this](const QStringList& syms, int* adv, int* dec) {
        *adv = *dec = 0;
        for (const auto& s : syms) {
            const auto it = breadth_cache_.constFind(s);
            if (it == breadth_cache_.constEnd())
                continue;
            if (it->change_pct > 0)
                ++*adv;
            else if (it->change_pct < 0)
                ++*dec;
        }
    };
    BreadthRow* rows[] = {&nyse_row_, &nasdaq_row_, &sp500_row_};
    for (int g = 0; g < 3 && g < groups.size(); ++g) {
        int adv = 0, dec = 0;
        count(groups[g].symbols, &adv, &dec);
        if (rows[g]->name)
            rows[g]->name->setText(groups[g].label);
        if (!rows[g]->adv)
            continue;
        rows[g]->adv->setText(QString::number(adv));
        rows[g]->dec->setText(QString::number(dec));
        const int total = std::max(1, adv + dec);
        const int adv_pct = static_cast<int>((double(adv) / total) * 100);
        if (auto* layout = qobject_cast<QHBoxLayout*>(rows[g]->green->parentWidget()->layout())) {
            layout->setStretch(0, adv_pct);
            layout->setStretch(1, 100 - adv_pct);
        }
    }
}

void MarketPulsePanel::render_risk() {
    // One documented risk-on / risk-off score app-wide (RiskCompositeService);
    // the panel's own basket score is gone.
    const auto& c = services::RiskCompositeService::instance().current();
    QString text, color, key;
    if (!c.complete) {
        key = QStringLiteral("INCOMPLETE");
        text = c.inputs.isEmpty() ? tr("LOADING...") : tr("INCOMPLETE");
        color = ui::colors::TEXT_TERTIARY();
        if (fg_score_val_) {
            fg_score_val_->setText(QStringLiteral("--"));
            fg_score_val_->setToolTip(c.missing.isEmpty() ? QString()
                                                          : tr("Missing: %1").arg(c.missing.join(QStringLiteral(", "))));
        }
    } else {
        key = c.regime;
        text = c.regime.toUpper();
        color = c.score >= 55 ? ui::colors::POSITIVE() : c.score <= 45 ? ui::colors::NEGATIVE() : ui::colors::WARNING();
        if (fg_score_val_) {
            fg_score_val_->setText(QString::number(qRound(c.score)));
            fg_score_val_->setToolTip(QString());
        }
    }
    fg_sentiment_key_ = key;
    if (fg_sentiment_)
        fg_sentiment_->setText(text);
    if (color != fg_applied_color_) {
        fg_applied_color_ = color;
        if (fg_score_val_)
            fg_score_val_->setStyleSheet(
                QString("color: %1; font-size: " + fincept::ui::fonts::px(4) + "px; font-weight: bold; background: transparent;")
                    .arg(color));
        if (fg_sentiment_)
            fg_sentiment_->setStyleSheet(
                QString("color: %1; font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: bold; letter-spacing: 0.5px; background: transparent;")
                    .arg(color));
    }
}

void MarketPulsePanel::rebuild_movers_from_cache() {
    if (movers_cache_.isEmpty() || gainer_rows_.isEmpty() || loser_rows_.isEmpty())
        return;

    QVector<services::QuoteData> quotes;
    quotes.reserve(movers_cache_.size());
    for (const auto& sym : pulse_mover_symbols()) {
        if (movers_cache_.contains(sym))
            quotes.append(movers_cache_.value(sym));
    }
    std::sort(quotes.begin(), quotes.end(), [](const auto& a, const auto& b) { return a.change_pct > b.change_pct; });

    // Reuse the fixed row pool — text-only updates, no widget churn.
    int gainers_added = 0;
    for (const auto& q : quotes) {
        if (q.change_pct <= 0 || gainers_added >= kMoverRows)
            break;
        fill_mover_row(gainer_rows_[gainers_added], q.symbol, q.change_pct, format_volume(q.volume));
        ++gainers_added;
    }
    for (int i = gainers_added; i < gainer_rows_.size(); ++i)
        clear_mover_row(gainer_rows_[i]);

    int losers_added = 0;
    for (int i = static_cast<int>(quotes.size()) - 1; i >= 0 && losers_added < kMoverRows; --i) {
        if (quotes[i].change_pct >= 0)
            continue;
        fill_mover_row(loser_rows_[losers_added], quotes[i].symbol, quotes[i].change_pct,
                       format_volume(quotes[i].volume));
        ++losers_added;
    }
    for (int i = losers_added; i < loser_rows_.size(); ++i)
        clear_mover_row(loser_rows_[i]);
}

void MarketPulsePanel::rebuild_snapshot_from_cache() {
    // Full grouped prices: "$2.6K" for gold hid a $30 move, and a "$" on the
    // 10Y yield / DXY / VIX was simply wrong.
    auto fmt_price = [](const services::QuoteData& q) -> QString {
        if (q.price >= 10000)
            return widgets::dash_fmt::grouped(q.price, 0);
        return widgets::dash_fmt::price(q.price, q.symbol);
    };
    auto fmt_chg = [](const services::QuoteData& q) -> QString {
        // ^TNX is a yield in percent: its move is quoted in basis points
        // (a % change of a yield is not a number anyone trades on).
        if (q.symbol == QLatin1String("^TNX")) {
            const double bp = q.change * 100.0;
            return QString("%1%2bp").arg(bp > 0 ? "+" : "").arg(bp, 0, 'f', 1);
        }
        return widgets::dash_fmt::signed_pct(q.change_pct);
    };
    auto update_stat = [&](const QString& sym, StatRow& row) {
        if (!row.val)
            return;
        if (!snapshot_cache_.contains(sym)) {
            // Still "--": hovering it says why (empty while merely loading).
            row.val->setToolTip(pulse_errors_.value(sym));
            return;
        }
        const auto& q = snapshot_cache_.value(sym);
        row.val->setToolTip(QString());
        row.val->setText(fmt_price(q));
        row.chg->setText(fmt_chg(q));
        // The VIX is a fear gauge: a rising VIX is bad news, so its move is
        // coloured inverted (it was painted positive when fear rose).
        const double tone = sym == QLatin1String("^VIX") ? -q.change_pct : q.change_pct;
        QString chg_color = tone > 0   ? ui::colors::POSITIVE()
                            : tone < 0 ? ui::colors::NEGATIVE()
                                       : ui::colors::TEXT_TERTIARY();
        pulse_set_css_cached(row.chg, 
            QString("color: %1; font-size: " + fincept::ui::fonts::px(-6) + "px; font-weight: bold; background: transparent;").arg(chg_color));
    };
    update_stat("^VIX", vix_row_);
    update_stat("^TNX", us10y_row_);
    update_stat("DX-Y.NYB", dxy_row_);
    update_stat("GC=F", gold_row_);
    update_stat("CL=F", oil_row_);
    update_stat("BTC-USD", btc_row_);
}

void MarketPulsePanel::hub_subscribe_all() {
    auto& hub = datahub::DataHub::instance();

    // Union of all three symbol sets — but dispatch per-set so each cache
    // only holds its own universe (keeps rebuild_* loops cheap).
    const QStringList breadth_syms = pulse_breadth_symbols();
    const QStringList mover_syms = pulse_mover_symbols();
    QSet<QString> all_syms;
    for (const auto& s : breadth_syms)
        all_syms.insert(s);
    for (const auto& s : mover_syms)
        all_syms.insert(s);
    for (const auto& s : kSnapshotSymbols)
        all_syms.insert(s);

    total_expected_symbols_ = all_syms.size();
    update_loading_progress();

    for (const QString& sym : all_syms) {
        const QString topic = QStringLiteral("market:quote:") + sym;
        const bool in_breadth = breadth_syms.contains(sym);
        const bool in_movers = mover_syms.contains(sym);
        const bool in_snapshot = kSnapshotSymbols.contains(sym);

        hub.subscribe(this, topic, [this, sym, in_breadth, in_movers, in_snapshot](const QVariant& v) {
            if (!v.canConvert<services::QuoteData>())
                return;
            const auto q = v.value<services::QuoteData>();
            pulse_errors_.remove(sym); // a value clears the symbol's failure
            if (in_breadth) {
                breadth_cache_.insert(sym, q);
                breadth_dirty_ = true;
            }
            if (in_movers) {
                movers_cache_.insert(sym, q);
                movers_dirty_ = true;
            }
            if (in_snapshot) {
                snapshot_cache_.insert(sym, q);
                snapshot_dirty_ = true;
            }
            // Render once for the whole delivery burst.
            schedule_render();
        });
        // Without an error channel a failed fetch left the panel on "LOADING n / N"
        // forever (or a row on "--") with no reason. Same owner as the value
        // subscription, so hub_unsubscribe_all()'s unsubscribe(this) removes both.
        hub.subscribe_errors(this, topic, [this, sym](const QString& error) {
            pulse_errors_.insert(sym, error);
            pulse_last_error_ = error;
            snapshot_dirty_ = true; // a "--" row shows the reason in its tooltip
            schedule_render();
        });
    }
    hub_active_ = true;
}

void MarketPulsePanel::apply_live_dot() {
    // The header dot was painted POSITIVE once and never touched again — green
    // while the panel itself said "Market data unavailable". It now follows the
    // quote caches: green when data is in and nothing failed, amber when some
    // symbols failed, red when nothing loaded and the hub reported failures,
    // dim while the first quotes are still on their way.
    if (!header_live_dot_)
        return;
    const bool has_data = !breadth_cache_.isEmpty() || !movers_cache_.isEmpty() || !snapshot_cache_.isEmpty();
    QString color;
    QString tip;
    if (has_data && pulse_errors_.isEmpty()) {
        color = ui::colors::SUCCESS();
        tip = tr("Market data live");
    } else if (has_data) {
        color = ui::colors::WARNING();
        tip = tr("%1 symbol(s) failed to load").arg(pulse_errors_.size());
    } else if (!pulse_errors_.isEmpty()) {
        color = ui::colors::DANGER();
        tip = tr("Market data unavailable — %1").arg(pulse_last_error_);
    } else {
        color = ui::colors::TEXT_TERTIARY();
        tip = tr("Loading market data…");
    }
    header_live_dot_->setToolTip(tip);
    const QString css = QString("background: %1; border-radius: 3px;").arg(color);
    if (css == live_dot_css_)
        return;
    live_dot_css_ = css;
    header_live_dot_->setStyleSheet(css);
}

void MarketPulsePanel::update_loading_progress() {
    apply_live_dot();
    if (!loading_overlay_)
        return;
    // Numerator = unique symbols received across the three caches. Each
    // cache only holds its own subset, but a symbol can be in more than
    // one — union via QSet to avoid double-counting.
    QSet<QString> seen;
    for (auto it = breadth_cache_.constBegin(); it != breadth_cache_.constEnd(); ++it)
        seen.insert(it.key());
    for (auto it = movers_cache_.constBegin(); it != movers_cache_.constEnd(); ++it)
        seen.insert(it.key());
    for (auto it = snapshot_cache_.constBegin(); it != snapshot_cache_.constEnd(); ++it)
        seen.insert(it.key());
    if (seen.isEmpty() && !pulse_errors_.isEmpty()) {
        // Nothing loaded and the hub said why: show it where the panel would be
        // (cleared by the next value — set_progress() leaves error mode).
        loading_overlay_->set_error(tr("Market data unavailable — %1").arg(pulse_last_error_));
        return;
    }
    // A symbol that failed is settled too: one dead ticker must not hold the
    // overlay at "LOADING 69 / 70" over data that did arrive.
    int settled = static_cast<int>(seen.size());
    for (auto it = pulse_errors_.constBegin(); it != pulse_errors_.constEnd(); ++it) {
        if (!seen.contains(it.key()))
            ++settled;
    }
    loading_overlay_->set_progress(settled, total_expected_symbols_);
}

void MarketPulsePanel::hub_unsubscribe_all() {
    if (!hub_active_)
        return;
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void MarketPulsePanel::refresh_data() {
    // Hub owns cadence. Force a kick so consumers see data immediately
    // (e.g., on theme-triggered refresh while visible).
    auto& hub = datahub::DataHub::instance();
    QStringList topics;
    QSet<QString> seen;
    auto push = [&](const QStringList& syms) {
        for (const auto& s : syms) {
            if (seen.contains(s))
                continue;
            seen.insert(s);
            topics.append(QStringLiteral("market:quote:") + s);
        }
    };
    push(pulse_breadth_symbols());
    push(pulse_mover_symbols());
    push(kSnapshotSymbols);
    hub.request(topics, /*force=*/true); // user-triggered refresh
    services::RiskCompositeService::instance().refresh();
}

bool MarketPulsePanel::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonDblClick) {
        const QString sym = obj->property("pulse_symbol").toString();
        if (!sym.isEmpty()) {
            EventBus::instance().publish("nav.open_symbol",
                                         QVariantMap{{"screen_id", "equity_research"}, {"symbol", sym}});
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void MarketPulsePanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void MarketPulsePanel::retranslateUi() {
    if (header_title_)
        header_title_->setText(tr("MARKET PULSE"));
    if (fg_header_label_)
        fg_header_label_->setText(tr("RISK-ON / RISK-OFF"));
    if (fg_header_label_)
        fg_header_label_->setToolTip(services::RiskCompositeService::methodology());

    auto set_sh = [](SectionHeader& sh) {
        if (!sh.title)
            return;
        const QString& k = sh.source_key;
        if (k == QLatin1String("BASKET BREADTH"))
            sh.title->setText(tr("BASKET BREADTH"));
        else if (k == QLatin1String("TOP GAINERS"))
            sh.title->setText(tr("TOP GAINERS"));
        else if (k == QLatin1String("TOP LOSERS"))
            sh.title->setText(tr("TOP LOSERS"));
        else if (k == QLatin1String("GLOBAL SNAPSHOT"))
            sh.title->setText(tr("GLOBAL SNAPSHOT"));
        else if (k == QLatin1String("MARKET HOURS"))
            sh.title->setText(tr("MARKET HOURS"));
    };
    set_sh(sh_breadth_);
    set_sh(sh_gainers_);
    set_sh(sh_losers_);
    set_sh(sh_snapshot_);
    set_sh(sh_hours_);

    // The risk regime label comes translated from RiskCompositeService.
    render_risk();
    // Venue rows show venue ids (proper names); their tooltips are re-resolved here.
    for (auto& hr : hours_rows_)
        if (hr.name_lbl)
            if (const auto* v = core::market::venue_by_code(hr.region))
                hr.name_lbl->setToolTip(core::market::venue_display_name(*v));
    rebuild_breadth_from_cache(); // basket labels
    refresh_market_hours();
    if (isVisible())
        rebuild_movers_from_cache(); // mover rows have "VOL: %1" labels rebuilt here
}

} // namespace fincept::screens
