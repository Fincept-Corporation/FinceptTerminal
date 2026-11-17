#include "screens/markets/MarketsScreen.h"

#include "core/events/EventBus.h"
#include "core/instrument/VenueSymbols.h"
#include "core/market/MarketSessions.h"
#include "core/symbol/SymbolContext.h"
#include "screens/common/CommandRows.h"
#include "screens/markets/MarketColumns.h"
#include "screens/markets/MarketPanel.h"
#include "screens/markets/MarketPanelModel.h"
#include "screens/markets/MarketPanelStore.h"
#include "screens/markets/MarketPanelView.h"
#include "screens/markets/MarketsCommandBar.h"
#include "screens/markets/MarketsDetailPane.h"
#include "screens/markets/MarketsLogic.h"
#include "screens/markets/MarketsOverviewViews.h"
#include "screens/markets/MarketsPulseRibbon.h"
#include "screens/markets/MarketsQuoteStore.h"
#include "screens/markets/MarketsStyle.h"
#include "screens/markets/MarketsSymbolEntry.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/charts/ChartExport.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/ScreenStatusBar.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QApplication>
#include <QCoreApplication>
#include <QCursor>
#include <QDateTime>
#include <QEvent>
#include <QHideEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShortcut>
#include <QShowEvent>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyle>
#include <QTimeZone>
#include <QThreadPool>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

namespace fincept::screens {

namespace colors = fincept::ui::colors;

namespace {

// ── Status bar texts (the shared ui::ScreenStatusBar; strings keep the
//    "MarketsStatusBar" translation context of the bar they replaced) ──────────

using MssBar = fincept::ui::ScreenStatusBar;

QString mss_tr(const char* text, int n = -1) {
    return QCoreApplication::translate("MarketsStatusBar", text, nullptr, n);
}

QString mss_duration(qint64 secs) {
    secs = std::max<qint64>(0, secs);
    const qint64 h = secs / 3600;
    const qint64 m = (secs % 3600) / 60;
    if (h >= 24)
        return mss_tr("%1d %2h").arg(h / 24).arg(h % 24);
    if (h > 0)
        return mss_tr("%1h %2m").arg(h).arg(m);
    return mss_tr("%1m").arg(std::max<qint64>(1, m));
}

QString mss_interval_label(int ms) {
    if (ms <= 0)
        return mss_tr("LIVE");
    if (ms >= 3600000)
        return mss_tr("%1H").arg(ms / 3600000);
    return mss_tr("%1M").arg(ms / 60000);
}

/// Items, left to right: feed state · coverage · failed · breadth (up / down / flat) ·
/// freshness · source ...... sessions · clocks. Priorities = what stays longest when
/// the window is narrow (source first, then clocks, then sessions — as before).
void mss_add_items(MssBar* sb) {
    using S = MssBar::Segment;
    using T = MssBar::Tone;
    sb->add_item(QStringLiteral("feed"), S::Left, {}, T::Ok, 100);
    sb->add_item(QStringLiteral("coverage"), S::Left, {}, T::Normal, 90);
    sb->add_item(QStringLiteral("failed"), S::Left, {}, T::Warn, 85);
    sb->set_item_visible(QStringLiteral("failed"), false);
    sb->add_item(QStringLiteral("up"), S::Left, {}, T::Positive, 60);
    sb->add_item(QStringLiteral("down"), S::Left, {}, T::Negative, 60);
    sb->add_item(QStringLiteral("flat"), S::Left, {}, T::Dim, 60);
    sb->add_item(QStringLiteral("fresh"), S::Left, {}, T::Normal, 70);
    sb->add_item(QStringLiteral("source"), S::Left, {}, T::Normal, 10);
    sb->add_item(QStringLiteral("sessions"), S::Right, {}, T::Normal, 30);
    sb->add_item(QStringLiteral("clocks"), S::Right, {}, T::Normal, 20);
}

void mss_retranslate(MssBar* sb) {
    const QString breadth = mss_tr("Advancing / declining / unchanged among the symbols in your panels");
    for (const char* id : {"up", "down", "flat"})
        sb->set_tooltip(QString::fromLatin1(id), breadth);
    if (QLabel* l = sb->item(QStringLiteral("feed")))
        l->setAccessibleName(mss_tr("Feed state"));
    if (QLabel* l = sb->item(QStringLiteral("coverage")))
        l->setAccessibleName(mss_tr("Quotes loaded"));
}

void mss_render(MssBar* sb, const MarketsQuoteStore::Coverage& c, int interval_ms, bool auto_on,
                const QStringList& failed_lines) {
    // Feed state (state tones: these are states, not price direction; spelled out in words).
    QString feed;
    MssBar::Tone tone = MssBar::Tone::Ok;
    QString feed_tip;
    if (c.cycle_running && c.loading > 0) {
        feed = mss_tr("● LOADING %1/%2").arg(c.total - c.loading).arg(c.total);
        tone = MssBar::Tone::Warn;
    } else if (c.all_failed) {
        feed = mss_tr("● RETRYING");
        tone = MssBar::Tone::Danger;
        feed_tip = mss_tr("No quote could be loaded in the last refresh. Retrying automatically - press F5 to retry now.");
    } else if (!auto_on) {
        feed = mss_tr("|| OFF");
        tone = MssBar::Tone::Dim;
        feed_tip = mss_tr("Automatic refresh is off. Press F5 to refresh.");
    } else {
        feed = QStringLiteral("● ") + mss_interval_label(interval_ms);
        feed_tip = interval_ms <= 0 ? mss_tr("Live: quotes refresh about every 30 s while this screen is visible.")
                                    : mss_tr("Panel quotes refresh every %1; the market pulse is always live.")
                                          .arg(mss_interval_label(interval_ms));
    }
    sb->set_item(QStringLiteral("feed"), feed, tone);
    sb->set_tooltip(QStringLiteral("feed"), feed_tip);

    sb->set_text(QStringLiteral("coverage"), mss_tr("%1/%2 quotes").arg(c.priced).arg(c.total));
    const bool any_failed = c.failed > 0;
    if (any_failed) {
        sb->set_text(QStringLiteral("failed"), mss_tr("%n failed", c.failed));
        QStringList lines = failed_lines.mid(0, 15);
        if (failed_lines.size() > 15)
            lines << mss_tr("...and %1 more").arg(failed_lines.size() - 15);
        sb->set_tooltip(QStringLiteral("failed"), lines.join(QLatin1Char('\n')));
    }
    sb->set_item_visible(QStringLiteral("failed"), any_failed);

    // Direction is carried by the arrow / "=" glyph as well as the tone.
    sb->set_text(QStringLiteral("up"), QString::fromUtf8("\xe2\x96\xb2 ") + QString::number(c.up));
    sb->set_text(QStringLiteral("down"), QString::fromUtf8("\xe2\x96\xbc ") + QString::number(c.down));
    sb->set_text(QStringLiteral("flat"), QStringLiteral("= ") + QString::number(c.flat));

    QString src;
    if (c.max_delay_min > 0)
        src = mss_tr("Yahoo Finance · delayed up to %n min", c.max_delay_min);
    else if (c.max_delay_min == 0)
        src = mss_tr("Yahoo Finance · real time");
    else
        src = mss_tr("Yahoo Finance · delay unknown");
    sb->set_text(QStringLiteral("source"), src);
}

void mss_freshness(MssBar* sb, qint64 newest_ms, int interval_ms, bool auto_on) {
    namespace fmt = fincept::ui::formatting;
    if (newest_ms <= 0) {
        sb->set_item(QStringLiteral("fresh"), mss_tr("Updated --:--"), MssBar::Tone::Dim);
        sb->set_tooltip(QStringLiteral("fresh"), QString());
        return;
    }
    const qint64 age = QDateTime::currentMSecsSinceEpoch() - newest_ms;
    const bool stale = auto_on && age > markets_logic::stale_after_ms(interval_ms);
    sb->set_item(QStringLiteral("fresh"), mss_tr("Updated %1").arg(fmt::time(newest_ms / 1000, true)),
                 stale ? MssBar::Tone::Warn : MssBar::Tone::Normal);
    sb->set_tooltip(QStringLiteral("fresh"), stale ? mss_tr("No quote has arrived for %1 - the data may be stale.")
                                                         .arg(fmt::relative(newest_ms / 1000))
                                                   : QString());
}

void mss_sessions(MssBar* sb) {
    using namespace core::market;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QStringList parts;
    for (const char* code : {"NYSE", "LSE", "TSE"}) {
        const Venue* v = venue_by_code(QString::fromLatin1(code));
        if (!v)
            continue;
        const SessionStatus st = session_status(*v, now);
        QString text = QStringLiteral("%1 %2").arg(v->id, session_state_label(st.state));
        if (st.state == SessionState::Holiday) {
            const QString name = holiday_name(*v, st.trading_date.isValid() ? st.trading_date : now.date());
            if (!name.isEmpty())
                text += QStringLiteral(" (%1)").arg(name);
        } else if (st.next_change_utc.isValid() && parts.isEmpty()) { // the first venue gets the countdown
            const qint64 secs = now.secsTo(st.next_change_utc);
            text += st.state == SessionState::Open ? mss_tr(" · closes in %1").arg(mss_duration(secs))
                                                   : mss_tr(" · opens in %1").arg(mss_duration(secs));
        }
        parts << text;
    }
    sb->set_text(QStringLiteral("sessions"), parts.join(QStringLiteral(" | ")));

    const struct {
        const char* label;
        const char* tz;
    } kClocks[] = {{QT_TRANSLATE_NOOP("MarketsStatusBar", "NY"), "America/New_York"},
                   {QT_TRANSLATE_NOOP("MarketsStatusBar", "LON"), "Europe/London"},
                   {QT_TRANSLATE_NOOP("MarketsStatusBar", "TOK"), "Asia/Tokyo"}};
    QStringList clocks;
    for (const auto& k : kClocks)
        clocks << QStringLiteral("%1 %2").arg(mss_tr(k.label),
                                              now.toTimeZone(QTimeZone(k.tz)).toString(QStringLiteral("HH:mm")));
    sb->set_text(QStringLiteral("clocks"), clocks.join(QStringLiteral("  ")));
}

/// The screen root's one sheet (set before any child exists). Children are styled by
/// objectName / property selectors; splitter handles here too.
QString msc_root_sheet() {
    return QStringLiteral("#msRoot{background:%1;}"
                          "QSplitter::handle{background:%2;}"
                          "QScrollArea#msGridScroll{background:transparent;border:none;}"
                          "#msGridHost,#msMaxPage{background:%1;}"
                          "QLabel#msBanner{background:%3;color:%4;padding:6px 14px;font-family:%5;font-size:%6px;"
                          "font-weight:600;}"
                          "QLabel#msBanner[tone=\"warn\"]{color:%7;}"
                          "QLabel#msNoPanels{color:%8;background:transparent;font-size:%9px;}")
               .arg(colors::BG_BASE(), colors::BORDER_DIM(), colors::BG_RAISED(), colors::DANGER(),
                    QString(ui::fonts::DATA_FAMILY))
               .arg(ui::fonts::px(-2))
               .arg(colors::WARNING(), colors::TEXT_TERTIARY())
               .arg(ui::fonts::px(-1)) +
           // Drag-to-reorder: the accent insertion bar between panels, or a framed empty
           // column (kind="column"), drawn over the grid while a panel header is dragged.
           QStringLiteral("QWidget#msDropBar{background:%1;border:none;}"
                          "QWidget#msDropBar[kind=\"column\"]{background:%2;border:2px solid %1;}")
               .arg(colors::AMBER(), colors::ACCENT_BG()) +
           markets_scrollbar_css();
}

const char* msc_view_key(int view) {
    switch (view) {
        case 1:
            return "heatmap";
        case 2:
            return "movers";
        default:
            return "panels";
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

MarketsScreen::MarketsScreen(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("msRoot"));
    setAttribute(Qt::WA_StyledBackground, true);
    markets_set_sheet(this, msc_root_sheet()); // before any child exists
    load_prefs();

    store_ = new MarketsQuoteStore(this);
    store_->set_interval_ms(interval_ms_);
    store_->set_auto(auto_update_);

    save_timer_ = new QTimer(this);
    save_timer_->setSingleShot(true);
    save_timer_->setInterval(700);
    connect(save_timer_, &QTimer::timeout, this, &MarketsScreen::flush_config_save);
    spark_timer_ = new QTimer(this);
    spark_timer_->setSingleShot(true);
    spark_timer_->setInterval(250);
    connect(spark_timer_, &QTimer::timeout, this, [this]() {
        // Sparklines only for rows on screen (+5 either side), in tables showing the column.
        QStringList want;
        if (view_stack_->currentWidget() == grid_stack_) {
            for (MarketPanel* p : panels_in_reading_order()) {
                if (!p->isVisible() || p->view()->isColumnHidden(mcols::Spark))
                    continue;
                for (const QString& s : p->view()->visible_symbols(5))
                    if (!want.contains(s))
                        want << s;
            }
        } else if (movers_ && view_stack_->currentWidget() == movers_) {
            want = movers_->visible_symbols(5);
        }
        store_->want_sparklines(want);
    });
    reflow_timer_ = new QTimer(this);
    reflow_timer_->setSingleShot(true);
    reflow_timer_->setInterval(150); // re-flow after the width stops changing, never during a drag
    connect(reflow_timer_, &QTimer::timeout, this, &MarketsScreen::apply_reflow);
    vsplit_save_timer_ = new QTimer(this);
    vsplit_save_timer_->setSingleShot(true);
    vsplit_save_timer_->setInterval(700);
    connect(vsplit_save_timer_, &QTimer::timeout, this, [this]() {
        save_vsplit_state();
        save_splitter_state();
    });

    build_ui();
    wire_shortcuts();

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { refresh_theme(); });
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this,
            &MarketsScreen::on_appearance_settled);
    // A currency / number-format change is not a theme change: every part re-renders itself
    // from cached data, deferred and coalesced (MarketPanelModel: refresh_model_on_format_change;
    // pulse ribbon, detail pane, heatmap, status bar: fmt::on_format_changed).
    retranslateUi();
}

MarketsScreen::~MarketsScreen() {
    if (save_timer_ && save_timer_->isActive())
        flush_config_save();
}

void MarketsScreen::load_prefs() {
    auto& settings = SettingsRepository::instance();
    const auto auto_res = settings.get("markets_auto_update");
    if (auto_res.is_ok() && !auto_res.value().isEmpty())
        auto_update_ = auto_res.value() != QLatin1String("0");
    // A stored interval is honoured as written (0 = LIVE); none -> 1M.
    const auto interval_res = settings.get("markets_update_interval_ms");
    if (interval_res.is_ok() && !interval_res.value().isEmpty()) {
        bool ok = false;
        const int ms = interval_res.value().toInt(&ok);
        if (ok && (ms == 0 || (ms >= 60000 && ms <= 86400000)))
            interval_ms_ = std::min(ms, 3600000);
    }
    const auto view = settings.get("markets.view");
    if (view.is_ok())
        view_ = view.value() == QLatin1String("heatmap") ? 1 : (view.value() == QLatin1String("movers") ? 2 : 0);
    const auto detail = settings.get("markets.detail_visible");
    if (detail.is_ok() && !detail.value().isEmpty())
        detail_pref_ = detail.value() == QLatin1String("1") ? 1 : 0;
    const auto pulse = settings.get("markets.pulse_visible");
    if (pulse.is_ok() && !pulse.value().isEmpty())
        pulse_visible_ = pulse.value() != QLatin1String("0");
}

void MarketsScreen::save_pref(const QString& key, const QString& value) {
    // Off the GUI thread (a tab switch must never wait on sqlite).
    (void)QtConcurrent::run([key, value]() { SettingsRepository::instance().set(key, value, "market_data"); });
}

void MarketsScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    command_bar_ = new MarketsCommandBar(this);
    MarketsSymbolEntryOptions opts;
    opts.compact = true;
    opts.clear_on_pick = true;
    opts.placeholder = tr("Search or add a symbol  ( / )");
    opts.recent_key = QStringLiteral("markets");
    search_ = markets_make_symbol_entry(
        command_bar_, opts, [this](const QVector<MarketsSymbolPick>& picks) { on_search_picked(picks); },
        [this](const QStringList& texts, const QString& message) { on_search_unresolved(texts, message); });
    search_->setMinimumWidth(ui::metrics::width(220));
    search_->setMaximumWidth(ui::metrics::width(420));
    search_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    command_bar_->set_symbol_entry(search_);
    command_bar_->set_view(static_cast<MarketsCommandBar::View>(view_));
    command_bar_->set_interval_ms(interval_ms_, auto_update_);
    command_bar_->set_pulse_checked(pulse_visible_);
    root->addWidget(command_bar_);

    ribbon_ = new MarketsPulseRibbon(store_, this);
    ribbon_->setVisible(pulse_visible_);
    store_->set_live_symbols(pulse_visible_ ? ribbon_->symbols() : QStringList{});
    root->addWidget(ribbon_);

    banner_ = new QLabel(this);
    banner_->setObjectName(QStringLiteral("msBanner"));
    banner_->setWordWrap(true);
    banner_->setVisible(false);
    root->addWidget(banner_);

    body_split_ = new QSplitter(Qt::Horizontal, this);
    body_split_->setHandleWidth(3);
    body_split_->setChildrenCollapsible(false);
    view_stack_ = new QStackedWidget(body_split_);
    grid_stack_ = new QStackedWidget(view_stack_);
    grid_scroll_ = new QScrollArea(grid_stack_);
    grid_scroll_->setObjectName(QStringLiteral("msGridScroll"));
    grid_scroll_->setWidgetResizable(true);
    grid_scroll_->setFrameShape(QFrame::NoFrame);
    grid_scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    grid_host_ = new QWidget(grid_scroll_);
    grid_host_->setObjectName(QStringLiteral("msGridHost"));
    auto* gl = new QVBoxLayout(grid_host_);
    gl->setContentsMargins(0, 0, 0, 0);
    gl->setSpacing(0);
    no_panels_ = new QLabel(grid_host_);
    no_panels_->setObjectName(QStringLiteral("msNoPanels"));
    no_panels_->setAlignment(Qt::AlignCenter);
    no_panels_->setWordWrap(true);
    no_panels_->setVisible(false);
    gl->addWidget(no_panels_);
    grid_scroll_->setWidget(grid_host_);
    grid_stack_->addWidget(grid_scroll_);
    max_page_ = new QWidget(grid_stack_);
    max_page_->setObjectName(QStringLiteral("msMaxPage"));
    auto* ml = new QVBoxLayout(max_page_);
    ml->setContentsMargins(0, 0, 0, 0);
    grid_stack_->addWidget(max_page_);
    view_stack_->addWidget(grid_stack_);
    body_split_->addWidget(view_stack_);
    body_split_->setStretchFactor(0, 1);
    connect(body_split_, &QSplitter::splitterMoved, this, [this]() { vsplit_save_timer_->start(); });
    root->addWidget(body_split_, 1);

    build_status_bar();
    root->addWidget(status_bar_);

    configs_ = MarketPanelStore::instance().load();
    create_panels();
    build_grid();
    push_panel_symbols();

    // Command bar
    connect(command_bar_, &MarketsCommandBar::view_selected, this,
            [this](MarketsCommandBar::View v) { set_view(static_cast<int>(v), true); });
    connect(command_bar_, &MarketsCommandBar::detail_toggled, this, [this](bool on) { set_detail_visible(on, true); });
    connect(command_bar_, &MarketsCommandBar::refresh_requested, this, &MarketsScreen::on_refresh);
    connect(command_bar_, &MarketsCommandBar::interval_changed, this, &MarketsScreen::on_interval_changed);
    connect(command_bar_, &MarketsCommandBar::add_panel_requested, this, [this]() { add_panel(); });
    connect(command_bar_, &MarketsCommandBar::reset_requested, this, &MarketsScreen::reset_panels);
    connect(command_bar_, &MarketsCommandBar::equalise_requested, this, &MarketsScreen::equalise_panels);
    connect(command_bar_, &MarketsCommandBar::pulse_toggled, this, [this](bool on) { set_pulse_visible(on, true); });
    connect(command_bar_, &MarketsCommandBar::export_all_requested, this, &MarketsScreen::export_all);
    connect(command_bar_, &MarketsCommandBar::columns_for_all_changed, this, &MarketsScreen::apply_columns_for_all);
    connect(command_bar_, &MarketsCommandBar::reset_widths_requested, this, &MarketsScreen::reset_all_widths);

    // Ribbon
    connect(ribbon_, &MarketsPulseRibbon::symbol_selected, this, &MarketsScreen::show_symbol);
    connect(ribbon_, &MarketsPulseRibbon::symbol_activated, this, &MarketsScreen::on_symbol_activated);

    // Store
    connect(store_, &MarketsQuoteStore::coverage_changed, this, &MarketsScreen::on_coverage_changed);
    connect(store_, &MarketsQuoteStore::resort_due, this, [this]() {
        for (MarketPanel* p : std::as_const(panels_))
            p->view()->resort();
        if (movers_ && movers_->isVisible())
            movers_->recompute();
    });
    connect(store_, &MarketsQuoteStore::cycle_finished, this, [this]() {
        if (!resolved_stored_) {
            resolved_stored_ = true;
            run_resolve_stored(); // once per session, never ahead of the first paint
        }
    });

    if (view_ != 0)
        set_view(view_, false);
}

void MarketsScreen::wire_shortcuts() {
    // Screen-wide, only while focus is inside Markets. F5 is the app's Refresh action,
    // which reaches on_refresh() through WindowFrame::refresh_focused_panel; Ctrl+1..3
    // are the app's window-focus keys, so the views use Alt+1..3.
    auto add = [this](const QKeySequence& keys, auto&& fn) {
        auto* s = new QShortcut(keys, this);
        s->setContext(Qt::WidgetWithChildrenShortcut);
        connect(s, &QShortcut::activated, this, fn);
    };
    add(QKeySequence(Qt::Key_Slash), [this]() { focus_search(); });
    add(QKeySequence(Qt::CTRL | Qt::Key_F), [this]() { focus_search(); });
    add(QKeySequence(Qt::CTRL | Qt::Key_R), [this]() { on_refresh(); });
    // Auto-refresh on/off sits next to Refresh (F5 / Ctrl+R). Not F9: that is the shell's
    // Chat Mode key everywhere, and a screen must not take a reserved function key.
    add(QKeySequence(Qt::SHIFT | Qt::Key_F5), [this]() { toggle_auto(); });
    add(QKeySequence(Qt::CTRL | Qt::Key_D), [this]() {
        set_detail_visible(!(detail_ && detail_->isVisible()), true);
    });
    add(QKeySequence(Qt::ALT | Qt::Key_1), [this]() { set_view(0, true); });
    add(QKeySequence(Qt::ALT | Qt::Key_2), [this]() { set_view(1, true); });
    add(QKeySequence(Qt::ALT | Qt::Key_3), [this]() { set_view(2, true); });
    add(QKeySequence(Qt::CTRL | Qt::Key_E), [this]() { export_focused_panel(); });
}

// ---------------------------------------------------------------------------
// Selection, activation, linking
// ---------------------------------------------------------------------------

void MarketsScreen::on_symbol_selected(const QString& panel_id, const QString& symbol) {
    // One selected row on the whole screen.
    for (MarketPanel* p : std::as_const(panels_))
        if (p->panel_id() != panel_id)
            p->view()->clear_selection_silently();
    if (MarketPanel* p = panel_by_id(panel_id))
        last_table_ = p->view();
    selected_symbol_ = symbol;
    if (detail_ && detail_->isVisible()) {
        store_->set_detail_symbol(symbol);
        detail_->set_symbol(symbol);
        detail_->set_listed(true);
    }
    // Published one event-loop turn later so arrow-key repeat does not flood the group.
    pending_publish_ = symbol;
    QTimer::singleShot(0, this, [this]() {
        const QString s = pending_publish_;
        pending_publish_.clear();
        if (!s.isEmpty())
            publish_row_symbol(s);
    });
}

void MarketsScreen::show_symbol(const QString& symbol) {
    if (symbol.isEmpty())
        return;
    selected_symbol_ = symbol;
    const bool listed = select_in_panels(symbol, false);
    if (detail_ && detail_->isVisible()) {
        store_->set_detail_symbol(symbol);
        detail_->set_symbol(symbol);
        detail_->set_listed(listed);
    }
    pending_publish_ = symbol;
    QTimer::singleShot(0, this, [this]() {
        const QString s = pending_publish_;
        pending_publish_.clear();
        if (!s.isEmpty())
            publish_row_symbol(s);
    });
}

void MarketsScreen::on_symbol_activated(const QString& symbol) {
    const MarketRow* r = store_->row(symbol);
    // asset_class is passed so an index does not arrive as an equity.
    fincept::EventBus::instance().publish(
        "nav.open_symbol",
        {{"screen_id", QStringLiteral("equity_research")},
         {"symbol", symbol},
         {"asset_class", core::instrument::asset_class_slug(
                             core::instrument::asset_class_of(symbol, r ? r->quote.quote_type : QString()))}});
}

void MarketsScreen::on_detail_requested(const QString& symbol, bool toggle) {
    if (toggle && detail_ && detail_->isVisible() && detail_->symbol() == symbol) {
        set_detail_visible(false, true);
        return;
    }
    selected_symbol_ = symbol;
    set_detail_visible(true, true);
}

void MarketsScreen::publish_row_symbol(const QString& symbol) {
    if (link_group_ == fincept::SymbolGroup::None)
        return;
    // Only equities / ETFs drive the group: the linked screens (Equity Trading's order
    // ticket, News' headline search, the Watchlist) take stock tickers, and an index /
    // future / FX / crypto quote would switch them to something they cannot use. Those
    // rows still reach Equity Research through double-click / Enter / "Open in".
    const MarketRow* r = store_->row(symbol);
    const auto cls = core::instrument::asset_class_of(symbol, r ? r->quote.quote_type : QString());
    if (cls != core::instrument::AssetClass::Equity && cls != core::instrument::AssetClass::Etf)
        return;
    fincept::SymbolRef ref;
    ref.symbol = symbol;
    ref.asset_class = core::instrument::asset_class_slug(cls);
    fincept::SymbolContext::instance().set_group_symbol(link_group_, ref, this);
}

void MarketsScreen::set_group(fincept::SymbolGroup g) {
    link_group_ = g;
    for (MarketPanel* p : std::as_const(panels_))
        p->set_link_group(static_cast<int>(g)); // the highlight colour follows the group
}

void MarketsScreen::on_group_symbol_changed(const fincept::SymbolRef& ref) {
    if (!ref.is_valid())
        return;
    // Not re-published: marked in every panel that lists it (model state), the first
    // listing panel scrolls to it, and the detail pane follows when it is open.
    const QString sym = markets_logic::canonical_or_kept(ref.symbol);
    store_->set_linked_symbol(sym);
    for (MarketPanel* p : panels_in_reading_order()) {
        const int row = p->model()->row_of(sym);
        if (row < 0)
            continue;
        p->view()->scrollTo(p->view()->panel_proxy()->mapFromSource(p->model()->index(row, 0)),
                            QAbstractItemView::EnsureVisible);
        break;
    }
    if (detail_ && detail_->isVisible()) {
        store_->set_detail_symbol(sym);
        detail_->set_symbol(sym);
        detail_->set_listed(!all_panel_symbols().isEmpty() && all_panel_symbols().contains(sym));
    }
}

fincept::SymbolRef MarketsScreen::current_symbol() const {
    if (selected_symbol_.isEmpty())
        return {};
    const MarketRow* r = store_->row(selected_symbol_);
    const auto cls = core::instrument::asset_class_of(selected_symbol_, r ? r->quote.quote_type : QString());
    if (cls != core::instrument::AssetClass::Equity && cls != core::instrument::AssetClass::Etf)
        return {};
    fincept::SymbolRef ref;
    ref.symbol = selected_symbol_;
    ref.asset_class = core::instrument::asset_class_slug(cls);
    return ref;
}

bool MarketsScreen::select_in_panels(const QString& symbol, bool focus) {
    for (MarketPanel* p : panels_in_reading_order()) {
        if (p->model()->row_of(symbol) < 0)
            continue;
        if (focus && view_ != 0)
            set_view(0, true);
        for (MarketPanel* other : std::as_const(panels_))
            if (other != p)
                other->view()->clear_selection_silently();
        p->view()->select_symbol(symbol, true);
        if (focus) {
            p->view()->setFocus(Qt::OtherFocusReason);
            last_table_ = p->view();
        }
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Views, detail pane, ribbon
// ---------------------------------------------------------------------------

void MarketsScreen::set_view(int view, bool save) {
    view = std::clamp(view, 0, 2);
    QWidget* page = grid_stack_;
    if (view == 1) {
        ensure_heatmap();
        page = heatmap_;
    } else if (view == 2) {
        ensure_movers();
        page = movers_;
    }
    view_ = view;
    if (view_stack_->currentWidget() != page)
        view_stack_->setCurrentWidget(page);
    command_bar_->set_view(static_cast<MarketsCommandBar::View>(view));
    if (save)
        save_pref(QStringLiteral("markets.view"), QString::fromLatin1(msc_view_key(view)));
    schedule_sparklines();
}

void MarketsScreen::ensure_heatmap() {
    if (heatmap_)
        return;
    heatmap_ = new MarketsHeatmapView(store_, view_stack_); // built on first show only
    view_stack_->addWidget(heatmap_);
    heatmap_->set_panels(configs_);
    connect(heatmap_, &MarketsHeatmapView::symbol_selected, this, &MarketsScreen::show_symbol);
    connect(heatmap_, &MarketsHeatmapView::symbol_activated, this, &MarketsScreen::on_symbol_activated);
    connect(heatmap_, &MarketsHeatmapView::panel_requested, this, [this](const QString& id) {
        set_view(0, true);
        if (MarketPanel* p = panel_by_id(id))
            p->view()->setFocus(Qt::OtherFocusReason);
    });
}

void MarketsScreen::ensure_movers() {
    if (movers_)
        return;
    movers_ = new MarketsMoversView(store_, view_stack_); // built on first show only
    view_stack_->addWidget(movers_);
    movers_->set_panels(configs_);
    connect(movers_, &MarketsMoversView::symbol_selected, this, &MarketsScreen::show_symbol);
    connect(movers_, &MarketsMoversView::symbol_activated, this, &MarketsScreen::on_symbol_activated);
    connect(movers_, &MarketsMoversView::viewport_rows_changed, this, &MarketsScreen::schedule_sparklines);
}

void MarketsScreen::ensure_detail() {
    if (detail_)
        return;
    detail_ = new MarketsDetailPane(store_, body_split_); // built on first show only
    body_split_->addWidget(detail_);
    body_split_->setStretchFactor(1, 0);
    detail_->set_panel_titles([this]() {
        QStringList t;
        for (MarketPanel* p : panels_in_reading_order())
            t << p->config().title;
        return t;
    }());
    connect(detail_, &MarketsDetailPane::close_requested, this, [this]() { set_detail_visible(false, true); });
    connect(detail_, &MarketsDetailPane::open_requested, this, [this](const QString& screen_id, const QString& sym) {
        const MarketRow* r = store_->row(sym);
        fincept::EventBus::instance().publish(
            "nav.open_symbol",
            {{"screen_id", screen_id},
             {"symbol", sym},
             {"asset_class", core::instrument::asset_class_slug(
                                 core::instrument::asset_class_of(sym, r ? r->quote.quote_type : QString()))}});
    });
    connect(detail_, &MarketsDetailPane::add_to_panel_requested, this, [this](const QString& sym, int index) {
        const QVector<MarketPanel*> order = panels_in_reading_order();
        if (index < 0 || index >= order.size())
            add_panel({sym});
        else if (add_symbol(order.at(index)->panel_id(), sym, true))
            detail_->set_listed(true);
    });
    // Sizes: the saved split, else a 360 px pane.
    const auto saved = SettingsRepository::instance().get("markets_body_split");
    if (!(saved.is_ok() && !saved.value().isEmpty() &&
          body_split_->restoreState(QByteArray::fromBase64(saved.value().toLatin1())))) {
        const int w = std::max(width(), 800);
        const int pane = ui::metrics::width(360);
        body_split_->setSizes({std::max(300, w - pane), pane});
    }
}

void MarketsScreen::set_detail_visible(bool on, bool save) {
    if (save) {
        detail_pref_ = on ? 1 : 0;
        save_pref(QStringLiteral("markets.detail_visible"), on ? QStringLiteral("1") : QStringLiteral("0"));
    }
    apply_detail_visibility();
}

void MarketsScreen::apply_detail_visibility() {
    const int w = width();
    // Default on at >= 1600 px; the user's choice wins from 1100 px; always closed below.
    const bool want = w >= 1100 && (detail_pref_ < 0 ? w >= 1600 : detail_pref_ == 1);
    command_bar_->set_detail_checked(want);
    if (want) {
        ensure_detail();
        if (!detail_->isVisible())
            detail_->setVisible(true);
        const QString sym = !selected_symbol_.isEmpty() ? selected_symbol_ : store_->linked_symbol();
        store_->set_detail_symbol(sym);
        detail_->set_symbol(sym);
        detail_->set_listed(sym.isEmpty() || all_panel_symbols().contains(sym));
    } else if (detail_ && detail_->isVisible()) {
        detail_->setVisible(false); // drops its history subscriptions
        store_->set_detail_symbol(QString());
    }
}

void MarketsScreen::set_pulse_visible(bool on, bool save) {
    pulse_visible_ = on;
    if (ribbon_->isVisible() != on)
        ribbon_->setVisible(on);
    store_->set_live_symbols(on ? ribbon_->symbols() : QStringList{});
    command_bar_->set_pulse_checked(on);
    if (save)
        save_pref(QStringLiteral("markets.pulse_visible"), on ? QStringLiteral("1") : QStringLiteral("0"));
}

// ---------------------------------------------------------------------------
// Refresh model
// ---------------------------------------------------------------------------

void MarketsScreen::on_refresh() {
    store_->refresh_now(); // forced cycle; ignored while a forced cycle is already running
}

void MarketsScreen::on_interval_changed(int ms) {
    if (ms < 0) { // OFF: markets_auto_update = "0", the interval is remembered underneath
        auto_update_ = false;
        store_->set_auto(false);
        save_pref(QStringLiteral("markets_auto_update"), QStringLiteral("0"));
    } else {
        interval_ms_ = ms;
        auto_update_ = true;
        store_->set_interval_ms(ms);
        store_->set_auto(true);
        save_pref(QStringLiteral("markets_auto_update"), QStringLiteral("1"));
        save_pref(QStringLiteral("markets_update_interval_ms"), QString::number(ms));
    }
    on_coverage_changed();
}

void MarketsScreen::toggle_auto() {
    on_interval_changed(auto_update_ ? -1 : interval_ms_);
    command_bar_->set_interval_ms(interval_ms_, auto_update_);
}

void MarketsScreen::on_coverage_changed() {
    const MarketsQuoteStore::Coverage c = store_->coverage();
    status_have_coverage_ = true;
    status_newest_ms_ = c.newest_quote_ms;
    mss_render(status_bar_, c, interval_ms_, auto_update_, store_->failed_lines());
    mss_freshness(status_bar_, status_newest_ms_, interval_ms_, auto_update_);
    command_bar_->set_refreshing(c.cycle_running && c.cycle_forced);
    update_banner();
}

void MarketsScreen::build_status_bar() {
    // The shared bottom bar (feed state · coverage · breadth · freshness · source ·
    // notice ...... sessions · clocks). Its own clock stays off: Markets shows world clocks.
    status_bar_ = new ui::ScreenStatusBar(this);
    mss_add_items(status_bar_);
    status_tick_ = new QTimer(this);
    status_tick_->setInterval(20'000); // runs between showEvent and hideEvent only
    connect(status_tick_, &QTimer::timeout, this, &MarketsScreen::tick_status_bar);
    mss_retranslate(status_bar_);
    mss_sessions(status_bar_);
    // "Updated 14:32" follows the clock / time-zone format: re-render from the last coverage.
    ui::formatting::on_format_changed(this, [this]() {
        if (status_have_coverage_)
            mss_freshness(status_bar_, status_newest_ms_, interval_ms_, auto_update_);
    });
}

void MarketsScreen::tick_status_bar() {
    mss_sessions(status_bar_);
    if (status_have_coverage_)
        mss_freshness(status_bar_, status_newest_ms_, interval_ms_, auto_update_);
}

void MarketsScreen::update_banner() {
    QString text;
    const char* tone = "";
    if (MarketPanelStore::instance().load_failed()) {
        text = tr("Your saved panels could not be read. Showing the default panels; changes will not be saved this "
                  "session.");
        tone = "warn";
    } else {
        const MarketsQuoteStore::Coverage c = store_->coverage();
        if (c.all_failed && !c.cycle_running) {
            const QString reason = store_->shared_failure_reason();
            if (c.priced == 0) {
                text = (reason.isEmpty() ? tr("No quote could be loaded for any symbol - hover a row for its reason.")
                                         : reason) +
                       QLatin1Char(' ') + tr("Press F5 to retry.");
            } else {
                text = (reason.isEmpty() ? QString() : reason + QLatin1Char(' ')) +
                       tr("Showing the last quotes received.") + QLatin1Char(' ') + tr("Press F5 to retry.");
                tone = "warn";
            }
        }
    }
    if (banner_->property("tone").toString() != QLatin1String(tone)) {
        banner_->setProperty("tone", QString::fromLatin1(tone));
        banner_->style()->unpolish(banner_);
        banner_->style()->polish(banner_);
    }
    if (banner_->text() != text)
        banner_->setText(text);
    if (banner_->isVisible() != !text.isEmpty())
        banner_->setVisible(!text.isEmpty());
}

void MarketsScreen::schedule_sparklines() {
    spark_timer_->start();
}

void MarketsScreen::push_panel_symbols() {
    // Reading order, a maximised panel first (it is the one on screen).
    QVector<QStringList> per_panel;
    if (MarketPanel* m = panel_by_id(maximised_id_))
        per_panel.append(m->config().symbols);
    for (MarketPanel* p : panels_in_reading_order())
        if (p->panel_id() != maximised_id_)
            per_panel.append(p->config().symbols);
    store_->set_panel_symbols(per_panel);
    if (heatmap_)
        heatmap_->set_panels(configs_);
    if (movers_)
        movers_->set_panels(configs_);
    if (detail_) {
        QStringList titles;
        for (MarketPanel* p : panels_in_reading_order())
            titles << p->config().title;
        detail_->set_panel_titles(titles);
    }
    QStringList hint;
    if (!configs_.isEmpty())
        hint = configs_.first().columns;
    command_bar_->set_columns_hint(hint);
}

void MarketsScreen::schedule_config_save() {
    save_timer_->start(); // 700 ms debounce; written off the GUI thread
}

void MarketsScreen::flush_config_save() {
    save_timer_->stop();
    const QVector<MarketPanelConfig> snapshot = configs_;
    (void)QtConcurrent::run([snapshot]() { MarketPanelStore::instance().save(snapshot); });
}

// ---------------------------------------------------------------------------
// Symbol search (command bar)
// ---------------------------------------------------------------------------

void MarketsScreen::focus_search() {
    markets_symbol_entry_focus(search_);
}

void MarketsScreen::on_search_picked(const QVector<MarketsSymbolPick>& picks) {
    if (picks.isEmpty())
        return;
    const MarketsSymbolPick& pick = picks.first();
    markets_learn(pick);
    const QString sym = pick.symbol;
    selected_symbol_ = sym;
    if (select_in_panels(sym, true)) {
        if (detail_ && detail_->isVisible()) {
            store_->set_detail_symbol(sym);
            detail_->set_symbol(sym);
            detail_->set_listed(true);
        }
        return;
    }
    // Not listed: never added by itself — shown live in the detail pane, with an
    // explicit "Add to panel" as the second step.
    if (width() >= 1100) {
        ensure_detail();
        command_bar_->set_detail_checked(true);
        if (!detail_->isVisible())
            detail_->setVisible(true);
        store_->set_detail_symbol(sym);
        detail_->set_symbol(sym);
        detail_->set_listed(false);
    }
    status_bar_->show_notice(tr("%1 is not in any panel.").arg(pick.display.isEmpty() ? sym : pick.display),
                             tr("Add to panel"), [this, sym]() { show_add_to_panel_menu(sym); });
}

void MarketsScreen::on_search_unresolved(const QStringList& texts, const QString& message) {
    status_bar_->show_notice(message.isEmpty() ? tr("No instrument found for: %1").arg(texts.join(QStringLiteral(", ")))
                                               : message,
                             {}, {}, 7000, ui::ScreenStatusBar::Tone::Warn);
}

void MarketsScreen::show_add_to_panel_menu(const QString& symbol) {
    QMenu menu(this);
    markets_set_sheet(&menu, markets_menu_css());
    for (MarketPanel* p : panels_in_reading_order()) {
        const QString id = p->panel_id();
        connect(menu.addAction(p->config().title), &QAction::triggered, this, [this, id, symbol]() {
            if (add_symbol(id, symbol, true) && detail_)
                detail_->set_listed(true);
        });
    }
    menu.addSeparator();
    connect(menu.addAction(tr("New panel...")), &QAction::triggered, this, [this, symbol]() { add_panel({symbol}); });
    menu.exec(QCursor::pos());
}

void MarketsScreen::run_resolve_stored() {
    // Saved symbols through the resolver (local index only, follows renames, never guesses).
    // A rename ("X is now Y") or a delisting is OFFERED: the row gets a ">" marker with the
    // resolver's note in its tooltip and a one-click "Replace with Y" in its menu. A user's
    // panel is never rewritten silently (the v1 default-ticker repair in MarketPanelStore is
    // the one exception).
    markets_resolve_stored(all_panel_symbols(), this,
                           [this](const QString& stored, const MarketsSymbolPick& now, const QString& note,
                                  bool renamed, bool inactive) {
                               if (!renamed && !inactive)
                                   return;
                               if (!note.isEmpty())
                                   resolver_notes_.insert(stored, note);
                               if (renamed && !now.symbol.isEmpty() && now.symbol != stored)
                                   resolver_successors_.insert(stored, now.symbol);
                               for (MarketPanel* p : std::as_const(panels_))
                                   if (p->model()->row_of(stored) >= 0)
                                       p->set_resolver_findings(resolver_notes_, resolver_successors_);
                           });
}

void MarketsScreen::replace_symbol(const QString& panel_id, const QString& from, const QString& to) {
    MarketPanel* p = panel_by_id(panel_id);
    if (!p || to.isEmpty())
        return;
    const int at = static_cast<int>(p->config().symbols.indexOf(from));
    if (at < 0)
        return;
    const bool had_to = p->config().symbols.contains(to);
    p->remove_symbol(from);
    if (!had_to)
        p->insert_symbol(to, at); // same row position
    p->view()->select_symbol(to, true);
    resolver_notes_.remove(from);
    resolver_successors_.remove(from);
    p->set_resolver_findings(resolver_notes_, resolver_successors_);
    status_bar_->show_notice(tr("Updated retired tickers: %1.").arg(QStringLiteral("%1 -> %2").arg(from, to)),
                             tr("UNDO"), [this, panel_id, from, to, at, had_to]() {
                                 MarketPanel* q = panel_by_id(panel_id);
                                 if (!q)
                                     return;
                                 if (!had_to)
                                     q->remove_symbol(to);
                                 q->insert_symbol(from, at);
                             });
}

// ---------------------------------------------------------------------------
// Export / columns
// ---------------------------------------------------------------------------

void MarketsScreen::export_focused_panel() {
    QWidget* f = QApplication::focusWidget();
    for (MarketPanel* p : std::as_const(panels_))
        if (f && (p == f || p->isAncestorOf(f))) {
            p->export_csv();
            return;
        }
    if (auto* v = qobject_cast<MarketPanelView*>(last_table_.data()))
        for (MarketPanel* p : std::as_const(panels_))
            if (p->view() == v) {
                p->export_csv();
                return;
            }
}

void MarketsScreen::export_all() {
    QStringList headers;
    QVector<QStringList> rows;
    // One header row (the first panel's columns with a PANEL column first); every panel
    // writes its own visible columns, so panels with other columns line up by position.
    for (MarketPanel* p : panels_in_reading_order())
        p->export_table(true, &headers, &rows);
    ui::ChartExport::save_csv(headers, rows, QStringLiteral("markets_all_panels"), this);
}

void MarketsScreen::apply_columns_for_all(const QStringList& ids) {
    for (MarketPanel* p : std::as_const(panels_))
        p->set_columns(ids);
    schedule_sparklines();
}

void MarketsScreen::reset_all_widths() {
    for (MarketPanel* p : std::as_const(panels_))
        p->view()->reset_widths();
}

// ---------------------------------------------------------------------------
// Show / hide, resize
// ---------------------------------------------------------------------------

void MarketsScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!h_restored_) {
        h_restored_ = true;
        restore_splitter_state();
    }
    apply_detail_visibility();
    // Subscriptions only while visible: the store warm-starts rows from the hub and runs
    // a cycle when the data is older than the interval (OFF: older than 5 min).
    store_->set_active(true);
    tick_status_bar();
    status_tick_->start();
    on_coverage_changed();
    schedule_reflow();
    schedule_sparklines();
}

void MarketsScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    store_->set_active(false);
    status_tick_->stop();
    if (save_timer_->isActive())
        flush_config_save();
    vsplit_save_timer_->stop();
    save_vsplit_state();
    save_splitter_state();
    if (detail_ && body_split_->count() > 1)
        save_pref(QStringLiteral("markets_body_split"), QString::fromLatin1(body_split_->saveState().toBase64()));
}

void MarketsScreen::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    schedule_reflow(); // 150 ms after the width settles: column count + detail pane
}

// ---------------------------------------------------------------------------
// Theme / language
// ---------------------------------------------------------------------------

void MarketsScreen::refresh_theme() {
    // theme_changed: repaint only — models read the tokens live, painted widgets read
    // the theme at paint time, and the central re-style pass rewrites every sheet. No
    // setStyleSheet / show / size hints here: those happen on appearance_settled.
    for (MarketPanel* p : std::as_const(panels_))
        p->refresh_theme();
    ribbon_->refresh_theme();
    command_bar_->refresh_theme(); // the status bar's tone sheet is re-styled centrally
    if (detail_)
        detail_->refresh_theme();
    if (heatmap_)
        heatmap_->refresh_theme();
    if (movers_)
        movers_->refresh_theme();
}

void MarketsScreen::on_appearance_settled() {
    // Font-derived sizes, once every widget has its new style.
    for (MarketPanel* p : std::as_const(panels_))
        p->refresh_metrics();
    if (movers_)
        movers_->refresh_metrics();
    command_bar_->setMinimumHeight(ui::metrics::height(36)); // the status bar re-fits itself
    search_->setMinimumWidth(ui::metrics::width(220));
    search_->setMaximumWidth(ui::metrics::width(420));
    apply_compact_heights();
}

void MarketsScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void MarketsScreen::retranslateUi() {
    markets_symbol_entry_set_placeholder(search_, tr("Search or add a symbol  ( / )"));
    search_->setAccessibleName(tr("Search or add a symbol"));
    no_panels_->setText(tr("No market panels.\nUse LAYOUT > Add panel, or LAYOUT > Reset to default panels."));
    mss_retranslate(status_bar_);
    mss_sessions(status_bar_);
    if (status_have_coverage_)
        on_coverage_changed(); // re-renders the status texts in the new language
    else
        update_banner();
}

// ---------------------------------------------------------------------------
// Deep links (ISectionLinked) + the search box's rows
// ---------------------------------------------------------------------------

bool MarketsScreen::open_section(const QString& section, const QString& focus) {
    const QString raw = section.trimmed();
    const QString key = raw.toLower();
    const QString f = focus.trimmed();
    if (key.isEmpty()) {
        if (!f.isEmpty())
            select_in_panels(markets_logic::canonical_or_kept(f), true); // a symbol: the row, if listed
        return true;
    }
    QString rest;
    if (command_rows::split_kind(raw, "panel", &rest)) {
        MarketPanel* p = panel_by_id(rest);
        if (!p)
            for (const MarketPanelConfig& c : std::as_const(configs_))
                if (c.title.compare(rest, Qt::CaseInsensitive) == 0) {
                    p = panel_by_id(c.id);
                    break;
                }
        if (!p)
            return false;
        set_view(0, true);
        if (!maximised_id_.isEmpty() && maximised_id_ != p->panel_id())
            restore_maximised();
        if (maximised_id_.isEmpty() && grid_scroll_)
            grid_scroll_->ensureWidgetVisible(p);
        p->view()->setFocus(Qt::OtherFocusReason);
        last_table_ = p->view();
        return true;
    }
    // The command bar's PANELS | HEATMAP | MOVERS.
    if (key == QLatin1String("panels") || key == QLatin1String("overview") || key == QLatin1String("grid"))
        set_view(0, true);
    else if (key == QLatin1String("heatmap"))
        set_view(1, true);
    else if (key == QLatin1String("movers"))
        set_view(2, true);
    else
        return false;
    return true;
}

namespace {

QList<command::CommandItem> mkt_command_rows() {
    namespace rows = command_rows;
    const QString screen = QStringLiteral("markets");
    const QString where = MarketsScreen::tr("Markets");
    return {
        rows::section(screen, QStringLiteral("panels"), MarketsScreen::tr("Market panels"),
                      {QStringLiteral("markets overview"), QStringLiteral("quotes"), QStringLiteral("indices"),
                       QStringLiteral("world markets"), QStringLiteral("market grid")},
                      where),
        rows::section(screen, QStringLiteral("heatmap"), MarketsScreen::tr("Market heatmap"),
                      {QStringLiteral("heat map"), QStringLiteral("heatmap"), QStringLiteral("sectors"),
                       QStringLiteral("market map")},
                      where),
        rows::section(screen, QStringLiteral("movers"), MarketsScreen::tr("Top movers"),
                      {QStringLiteral("gainers"), QStringLiteral("losers"), QStringLiteral("movers"),
                       QStringLiteral("biggest moves"), QStringLiteral("top gainers")},
                      where),
    };
}

/// The user's saved market panels (titles + ids from the stored panel set, read on a worker; the
/// built-in set is matched by title by open_section). Nothing is fetched.
void mkt_query_panels(const QString& query, quint64, command::ResultSink sink) {
    QThreadPool::globalInstance()->start([query, sink]() {
        QList<command::CommandItem> all;
        const auto stored = SettingsRepository::instance().get(QStringLiteral("markets_panel_configs"));
        if (stored.is_ok() && !stored.value().isEmpty()) {
            const QJsonDocument doc = QJsonDocument::fromJson(stored.value().toUtf8());
            const QString where = MarketsScreen::tr("Markets");
            for (const QJsonValue& v : doc.array()) {
                const QJsonObject o = v.toObject();
                const QString id = o.value(QStringLiteral("id")).toString();
                const QString title = o.value(QStringLiteral("title")).toString().trimmed();
                if (id.isEmpty() || title.isEmpty())
                    continue;
                command::CommandItem it = command_rows::section(
                    QStringLiteral("markets"), QStringLiteral("panel:") + id, title,
                    {QStringLiteral("panel"), QStringLiteral("market panel"), QStringLiteral("markets")}, where,
                    MarketsScreen::tr("Markets › panel %1").arg(title));
                it.id = QStringLiteral("markets.panel.") + id;
                it.group = QString::fromLatin1(command::groups::kSaved);
                all.append(it);
                if (all.size() >= 60)
                    break;
            }
        }
        sink(command_rows::best_matches(all, query, 6), true);
    });
}

void mkt_install_commands() {
    command_rows::install(QStringLiteral("markets"), &mkt_command_rows);
    command::ProviderSpec p;
    p.id = QStringLiteral("markets.panels");
    p.group = QString::fromLatin1(command::groups::kSaved);
    p.query = &mkt_query_panels;
    p.owner = QStringLiteral("markets");
    p.cap = 6;
    p.min_chars = 2;
    p.slow = true; // one settings read on a worker
    command::CommandRegistry::instance().register_provider(p);
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(mkt_install_commands)

} // namespace fincept::screens
