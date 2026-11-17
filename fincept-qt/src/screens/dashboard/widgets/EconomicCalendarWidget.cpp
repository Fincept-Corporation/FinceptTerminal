#include "screens/dashboard/widgets/EconomicCalendarWidget.h"

#include "core/market/MarketProfile.h"
#include "datahub/DataHub.h"
#include "screens/dashboard/widgets/RegionPin.h"
#include "screens/dashboard/widgets/TileBilling.h"
#include "ui/formatting/DateFormat.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QResizeEvent>
#include <QTimeZone>
#include <QTimer>

#include <algorithm>
#include <utility>

namespace {
constexpr const char* kEconCalTopic = "econ:fincept:upcoming_events";
constexpr int kEconMaxRows = 200;

/// Release instant of a row: the source's date + time read as UTC (Trading
/// Economics calendar rows carry no zone). Invalid when the row has no time.
QDateTime econ_release_utc(const QString& date, const QString& time) {
    const QDate d = QDate::fromString(date.left(10), Qt::ISODate);
    const QTime t = QTime::fromString(time.trimmed().left(5), QStringLiteral("HH:mm"));
    if (!d.isValid() || !t.isValid())
        return {};
    return QDateTime(d, t, QTimeZone::UTC);
}

// The fixed columns below need ~460 px. In a narrow tile (4 of 12 columns is
// ~350 px) the EVENT column — the only flexible one — collapsed to nothing and
// the row overflowed sideways: dates and numbers, no event names. EVENT keeps a
// minimum width and the low-priority columns drop out as the tile narrows.
constexpr int kColWidthEventMin = 110;
constexpr int kCalendarShowRefConsW = 520;  ///< REF + CONS shown from this tile width
constexpr int kCalendarShowFcstPrevW = 400; ///< FCST + PREV shown from this tile width

// Fixed column widths so header + rows line up regardless of event-name
// length. QHBoxLayout's stretch factors alone don't align columns when
// each row has different text widths — long event names push their
// neighbours rightward, breaking the table look. EVENT is the only
// variable column; it takes whatever leftover space the layout has.
constexpr int kColWidthCty = 32;
constexpr int kColWidthDate = 86;
constexpr int kColWidthRef = 40;
constexpr int kColWidthAct = 56;
constexpr int kColWidthFcst = 48;
constexpr int kColWidthCons = 48;
constexpr int kColWidthPrev = 48;
constexpr int kColWidthImp = 40;

// Header-label property holding its design column width (0 = the EVENT column).
constexpr const char* kEconColWidthProp = "econ_col_w";

// Every width above is the design size at the default base font. They widen in
// step with a larger user font, so the fixed columns never clip their text and
// the width tiers drop columns at the same text density.
int econ_col_w(int design_px) {
    const int base = std::max(fincept::ui::fonts::font_px(), fincept::ui::ThemeManager::kDefaultFontPx);
    return design_px * base / fincept::ui::ThemeManager::kDefaultFontPx;
}
} // namespace

namespace fincept::screens::widgets {

EconomicCalendarWidget::EconomicCalendarWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget(tr("ECONOMIC CALENDAR"), parent, ui::colors::CYAN()) {
    set_agent_risk(tile_billing::agent_kind(QStringLiteral("macro"))); // refresh / LOAD: billed ("macro" credits)
    auto* vl = content_layout();
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Filters: country (the user's region first) and minimum importance.
    auto* filters = new QWidget(this);
    auto* fl = new QHBoxLayout(filters);
    fl->setContentsMargins(6, 3, 6, 3);
    fl->setSpacing(4);
    country_combo_ = new QComboBox(filters);
    country_combo_->setAccessibleName(tr("Country filter"));
    fl->addWidget(country_combo_, 1);
    importance_combo_ = new QComboBox(filters);
    importance_combo_->setAccessibleName(tr("Importance filter"));
    fl->addWidget(importance_combo_, 1);
    vl->addWidget(filters);
    auto refilter = [this]() {
        if (filling_filters_)
            return;
        country_filter_ = country_combo_->currentData().toString();
        min_importance_ = importance_combo_->currentData().toInt();
        if (!last_events_.isEmpty())
            populate(last_events_);
        emit config_changed(config());
    };
    connect(country_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, refilter);
    connect(importance_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, refilter);
    connect(&core::market::MarketProfileStore::instance(), &core::market::MarketProfileStore::profile_changed, this,
            [this]() {
                if (!region_pin_.isEmpty())
                    return; // pinned to its own region (RegionPin.h)
                fill_filters();
                if (!last_events_.isEmpty())
                    populate(last_events_);
            });
    region_pin_ = region_pin_from(cfg);
    country_filter_ = cfg.value("country").toString(QStringLiteral("@region"));
    min_importance_ = std::clamp(cfg.value("importance").toInt(0), 0, 3);

    // Column headers
    header_widget_ = new QWidget(this);
    auto* hl = new QHBoxLayout(header_widget_);
    hl->setContentsMargins(8, 4, 8, 4);

    auto make_hdr = [&](const QString& text, int fixed_w, int stretch, Qt::Alignment align = Qt::AlignLeft) {
        auto* lbl = new QLabel(text);
        lbl->setAlignment(align);
        lbl->setProperty(kEconColWidthProp, fixed_w);
        if (fixed_w > 0) {
            lbl->setFixedWidth(econ_col_w(fixed_w));
        } else {
            lbl->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            lbl->setMinimumWidth(econ_col_w(kColWidthEventMin));
        }
        header_labels_.append(lbl);
        hl->addWidget(lbl, stretch);
    };
    make_hdr(tr("EVENT"), /*fixed_w=*/0, /*stretch=*/1);
    make_hdr(tr("CTY"), kColWidthCty, 0);
    make_hdr(tr("DATE"), kColWidthDate, 0);
    make_hdr(tr("REF"), kColWidthRef, 0);
    make_hdr(tr("ACT"), kColWidthAct, 0, Qt::AlignRight);
    make_hdr(tr("FCST"), kColWidthFcst, 0, Qt::AlignRight);
    make_hdr(tr("CONS"), kColWidthCons, 0, Qt::AlignRight);
    make_hdr(tr("PREV"), kColWidthPrev, 0, Qt::AlignRight);
    make_hdr(tr("IMP"), kColWidthImp, 0, Qt::AlignRight);
    vl->addWidget(header_widget_);

    header_sep_ = new QFrame;
    header_sep_->setFixedHeight(1);
    vl->addWidget(header_sep_);

    // Scrollable list
    scroll_area_ = new QScrollArea;
    scroll_area_->setWidgetResizable(true);

    auto* list_widget = new QWidget(this);
    list_widget->setStyleSheet("background: transparent;");
    list_layout_ = new QVBoxLayout(list_widget);
    list_layout_->setContentsMargins(0, 0, 0, 0);
    list_layout_->setSpacing(0);

    status_label_ = new QLabel(tr("Loading..."));
    status_label_->setAlignment(Qt::AlignCenter);
    status_label_->setWordWrap(true); // error explanations are a sentence long
    list_layout_->addWidget(status_label_);
    list_layout_->addStretch();

    scroll_area_->setWidget(list_widget);
    vl->addWidget(scroll_area_, 1);

    // User-driven refresh button on the BaseWidget title bar — force the
    // hub to refresh the topic. Per-producer rate limit (2/sec) still
    // applies, so rage-clicking can't hammer api.fincept.in.
    //
    // We must show the loading overlay BEFORE the request — otherwise the
    // click feels dead when the producer returns the same cached payload
    // (TTL=5min: the API genuinely has nothing new most of the time) or
    // when the producer is currently in_flight and the hub silently drops
    // the duplicate request. Capturing `this` keeps the slot bound to the
    // widget lifetime so set_loading is safe across re-shows.
    //
    // Billing rule: every uncached call of this feed costs a credit, so nothing
    // here loads by itself (unless the plan includes macro data — PlanAccess):
    // the title-bar refresh and the LOAD offer ask once. The subscription that
    // receives the answer ends 4 minutes later (the topic's TTL is 15 minutes,
    // its minimum interval 5), so the hub never re-fetches on its own while the
    // tile is open. A cached value is delivered first, so the first delivery
    // cannot end it.
    connect(this, &BaseWidget::refresh_requested, this, [this]() {
        hide_empty_state();
        set_loading(true);
        if (!hub_active_)
            hub_subscribe();
        datahub::DataHub::instance().request(QString::fromLatin1(kEconCalTopic), /*force=*/true);
        auto* window = findChild<QTimer*>(QStringLiteral("econCalOneShot"), Qt::FindDirectChildrenOnly);
        if (!window) {
            window = new QTimer(this);
            window->setObjectName(QStringLiteral("econCalOneShot"));
            window->setSingleShot(true);
            window->setInterval(4 * 60 * 1000);
            connect(window, &QTimer::timeout, this, [this]() {
                if (hub_active_ && !tile_billing::plan_includes(QStringLiteral("macro")))
                    hub_unsubscribe();
            });
        }
        window->start(); // a later LOAD restarts the window
    });
    // The plan table arrived / changed: a plan that includes macro data loads the
    // offered calendar now, as a free tile would.
    connect(&billing::PlanAccess::instance(), &billing::PlanAccess::changed, this, [this]() {
        set_agent_risk(tile_billing::agent_kind(QStringLiteral("macro"))); // agents: an included load is free
        if (isVisible() && is_empty_state_shown() && tile_billing::plan_includes(QStringLiteral("macro")))
            request_refresh();
    });

    fill_filters();
    apply_styles();
    set_loading(true);
}

QJsonObject EconomicCalendarWidget::config() const {
    QJsonObject o{{"country", country_filter_}, {"importance", min_importance_}};
    store_region_pin(o, region_pin_);
    return o;
}

void EconomicCalendarWidget::apply_config(const QJsonObject& cfg) {
    region_pin_ = region_pin_from(cfg);
    country_filter_ = cfg.value("country").toString(QStringLiteral("@region"));
    min_importance_ = std::clamp(cfg.value("importance").toInt(0), 0, 3);
    fill_filters();
    if (!last_events_.isEmpty())
        populate(last_events_);
}

void EconomicCalendarWidget::fill_filters() {
    filling_filters_ = true;
    const auto& p = tile_profile(region_pin_);
    country_combo_->clear();
    country_combo_->addItem(tr("All countries"), QString());
    if (!p.countries.isEmpty())
        country_combo_->addItem(tr("%1 (%2)").arg(core::market::market_profile_name(p), p.countries.join(QLatin1Char('/'))),
                                QStringLiteral("@region"));
    QStringList seen;
    for (const auto& v : std::as_const(last_events_)) {
        const QString c = v.toObject().value("country").toString().toUpper().trimmed();
        if (!c.isEmpty() && !seen.contains(c))
            seen << c;
    }
    std::sort(seen.begin(), seen.end());
    for (const auto& c : seen)
        country_combo_->addItem(c, c);
    int idx = country_combo_->findData(country_filter_);
    if (idx < 0 && !country_filter_.isEmpty() && country_filter_ != QLatin1String("@region")) {
        country_combo_->addItem(country_filter_, country_filter_); // saved filter not in today's data
        idx = country_combo_->count() - 1;
    }
    country_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
    country_filter_ = country_combo_->currentData().toString();

    importance_combo_->clear();
    importance_combo_->addItem(tr("Any importance"), 0);
    importance_combo_->addItem(tr("Medium + high"), 2);
    importance_combo_->addItem(tr("High only"), 3);
    const int ii = importance_combo_->findData(min_importance_);
    importance_combo_->setCurrentIndex(ii >= 0 ? ii : 0);
    filling_filters_ = false;
}

void EconomicCalendarWidget::apply_styles() {
    set_css_cached(header_widget_, QString("background: %1;").arg(ui::colors::BG_RAISED()));
    for (auto* lbl : header_labels_) {
        set_css_cached(lbl, QString("color: %1; font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: bold; background: transparent;")
                               .arg(ui::colors::TEXT_SECONDARY()));
        const int design_w = lbl->property(kEconColWidthProp).toInt();
        if (design_w > 0)
            lbl->setFixedWidth(econ_col_w(design_w));
        else
            lbl->setMinimumWidth(econ_col_w(kColWidthEventMin));
    }
    set_css_cached(header_sep_, QString("background: %1;").arg(ui::colors::BORDER_DIM()));
    set_css_cached(scroll_area_, 
        QString("QScrollArea { border: none; background: transparent; }"
                "QScrollBar:vertical { width: 4px; background: transparent; }"
                "QScrollBar::handle:vertical { background: %1; border-radius: 2px; min-height: 20px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
            .arg(ui::colors::BORDER_MED()));
    set_css_cached(status_label_, QString("color: %1; font-size: " + fincept::ui::fonts::px(-4) + "px; padding: 16px; background: transparent;")
                                     .arg(ui::colors::TEXT_TERTIARY()));
}

void EconomicCalendarWidget::on_theme_changed() {
    apply_styles();
    apply_column_visibility(); // the width tiers follow the font size
    if (!last_events_.isEmpty())
        populate(last_events_);
}

void EconomicCalendarWidget::clear_list() {
    while (list_layout_->count() > 0) {
        auto* item = list_layout_->takeAt(0);
        if (auto* widget = item->widget()) {
            if (widget == status_label_) {
                status_label_->hide();
            } else {
                widget->deleteLater();
            }
        }
        delete item;
    }
    // The rows (and their cells) are going away with deleteLater().
    ref_cells_.clear();
    fcst_cells_.clear();
    cons_cells_.clear();
    prev_cells_.clear();
}

void EconomicCalendarWidget::resizeEvent(QResizeEvent* e) {
    BaseWidget::resizeEvent(e);
    const int tier = width() >= econ_col_w(kCalendarShowRefConsW) ? 2 : width() >= econ_col_w(kCalendarShowFcstPrevW) ? 1 : 0;
    if (tier != column_tier_) {
        column_tier_ = tier;
        apply_column_visibility();
    }
}

void EconomicCalendarWidget::apply_column_visibility() {
    const bool show_ref_cons = width() >= econ_col_w(kCalendarShowRefConsW);
    const bool show_fcst_prev = width() >= econ_col_w(kCalendarShowFcstPrevW);
    // header_labels_: EVENT, CTY, DATE, REF, ACT, FCST, CONS, PREV, IMP
    if (header_labels_.size() >= 8) {
        header_labels_[3]->setVisible(show_ref_cons);
        header_labels_[5]->setVisible(show_fcst_prev);
        header_labels_[6]->setVisible(show_ref_cons);
        header_labels_[7]->setVisible(show_fcst_prev);
    }
    for (auto* l : std::as_const(ref_cells_))
        l->setVisible(show_ref_cons);
    for (auto* l : std::as_const(cons_cells_))
        l->setVisible(show_ref_cons);
    for (auto* l : std::as_const(fcst_cells_))
        l->setVisible(show_fcst_prev);
    for (auto* l : std::as_const(prev_cells_))
        l->setVisible(show_fcst_prev);
}

void EconomicCalendarWidget::show_status(const QString& text) {
    clear_list();
    status_label_->setText(text);
    status_label_->setVisible(true);
    list_layout_->addWidget(status_label_);
    list_layout_->addStretch();
}

void EconomicCalendarWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (hub_active_)
        return; // a LOAD / refresh is in flight
    if (tile_billing::plan_includes(QStringLiteral("macro"))) {
        hub_subscribe(); // included with the plan: kept fresh by the hub, as before
        return;
    }
    // Billing rule: no request on show. What the hub already holds (this session's
    // last load, here or in the Economics screen), else the LOAD offer.
    const QVariant cached = datahub::DataHub::instance().peek_raw(QString::fromLatin1(kEconCalTopic));
    const QJsonArray events = cached.canConvert<QJsonArray>() ? cached.value<QJsonArray>() : QJsonArray();
    set_loading(false);
    if (!events.isEmpty()) {
        hide_empty_state();
        populate(events);
        return;
    }
    if (!last_events_.isEmpty())
        return; // rows on screen already
    ui::EmptyState::Spec spec = ui::EmptyState::preview_spec(
        tr("Economic calendar"),
        tr("Upcoming macro releases with actual, forecast, consensus and previous values, filtered to your region."),
        tr("LOAD CALENDAR"),
        tile_billing::cost_note(QStringLiteral("macro"), 1.0,
                                tr("Up to 1 credit per load — free when Fincept answers from its cache")),
        ui::EmptyState::Tone::Warning, ui::LoadingSkeleton::Shape::List);
    show_empty_state(spec, [this]() { request_refresh(); });
}

void EconomicCalendarWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe();
}

void EconomicCalendarWidget::hub_subscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.subscribe(this, QString::fromLatin1(kEconCalTopic), [this](const QVariant& v) {
        set_loading(false);
        QJsonArray events;
        if (v.canConvert<QJsonArray>())
            events = v.value<QJsonArray>();
        if (events.isEmpty()) {
            show_status(tr("No events available"));
            return;
        }
        populate(events);
    });
    // Per-topic error subscription — fires only when *our* topic errors,
    // unlike the global topic_error signal which fans every error to every
    // listener. Keeps the widget out of the global error broadcast path.
    hub.subscribe_errors(this, QString::fromLatin1(kEconCalTopic), [this](const QString& error) {
        set_loading(false);
        // The producer's text says why and what fixes it (sign in, out of
        // credits, IP block) — a refused topic is not retried on a timer, so
        // this is what the panel shows until the user acts.
        // A bare HTTP reason ("Not Found") said nothing about what failed.
        const QString e = error.trimmed();
        show_status(e.isEmpty()       ? tr("Failed to load calendar")
                    : e.size() < 40 ? tr("Could not load the economic calendar: %1").arg(e)
                                    : e);
    });
    hub_active_ = true;
    // Cold-cache fallback: if the producer warmed the topic earlier, paint
    // it now even if it's slightly stale — beats a blank panel.
    QVariant cached = hub.peek_raw(QString::fromLatin1(kEconCalTopic));
    if (cached.canConvert<QJsonArray>()) {
        const auto events = cached.value<QJsonArray>();
        if (!events.isEmpty()) {
            set_loading(false);
            populate(events);
        }
    }
}

void EconomicCalendarWidget::hub_unsubscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this, QString::fromLatin1(kEconCalTopic));
    hub.unsubscribe_errors(this, QString::fromLatin1(kEconCalTopic));
    hub_active_ = false;
}

void EconomicCalendarWidget::populate(const QJsonArray& events) {
    const bool first = last_events_.isEmpty();
    last_events_ = events;
    if (first)
        fill_filters(); // the country list comes from the data

    clear_list();

    bool alt = false;
    int count = 0;
    const QStringList region_countries = tile_profile(region_pin_).countries;

    for (const auto& v : events) {
        if (count >= kEconMaxRows)
            break;
        auto e = v.toObject();
        const QString row_country = e["country"].toString().toUpper().trimmed();
        if (country_filter_ == QLatin1String("@region")) {
            // "EU" rows count for the Eurozone; an empty region list = all.
            if (!region_countries.isEmpty() && !region_countries.contains(row_country))
                continue;
        } else if (!country_filter_.isEmpty() && row_country != country_filter_) {
            continue;
        }
        if (e["importance"].toInt(0) < min_importance_)
            continue;

        // Fincept upcoming-events schema (May 2026 endpoint update): each row
        // carries event, category, country, date/time, reference_period,
        // actual, forecast, consensus, previous, revised_from, sentiment,
        // symbol, importance, url, chart_url. Tile shows the most important
        // columns inline; the rest is packed into the tooltip so power users
        // get the full data without sacrificing the table layout.
        QString event_name = e["event"].toString().trimmed();
        if (event_name.isEmpty())
            continue;

        QString category = e["category"].toString().trimmed();
        QString country = e["country"].toString().toUpper();
        QString date = e["date"].toString();
        QString time_str = e["time"].toString().trimmed();
        QString reference_period = e["reference_period"].toString().trimmed();
        QString actual = e["actual"].toString().trimmed();
        QString forecast = e["forecast"].toString().trimmed();
        QString consensus = e["consensus"].toString().trimmed();
        QString previous = e["previous"].toString().trimmed();
        QString revised_from = e["revised_from"].toString().trimmed();
        QString sentiment = e["sentiment"].toString().trimmed();
        QString symbol = e["symbol"].toString().trimmed();
        int imp_int = e["importance"].toInt(0);

        // Date / time in the user's locale and display zone. The source's time
        // is read as UTC; a date-only row stays a calendar date (no shift).
        QString date_display = date;
        const QDateTime release = econ_release_utc(date, time_str);
        if (release.isValid()) {
            const QDateTime local = ui::formatting::to_display_zone(release);
            date_display = local.date() == ui::formatting::to_display_zone(QDateTime::currentDateTimeUtc()).date()
                               ? ui::formatting::time(release)
                               : ui::formatting::date(release, ui::formatting::DateStyle::Short) +
                                     QLatin1Char(' ') + ui::formatting::time(release);
        } else if (const QDate d = QDate::fromString(date.left(10), Qt::ISODate); d.isValid()) {
            date_display = ui::formatting::calendar_date(d, ui::formatting::DateStyle::Short);
        }

        // Importance color: 0=dim, 1=low/dim, 2=medium/amber, 3=high/red
        QString imp_color = imp_int >= 3   ? ui::colors::DANGER()
                            : imp_int == 2 ? ui::colors::WARNING()
                                           : ui::colors::TEXT_TERTIARY();
        QString imp_text = imp_int >= 3 ? tr("HIGH") : imp_int == 2 ? tr("MED") : imp_int == 1 ? tr("LOW") : "--";

        auto* row = new QWidget(this);
        row->setStyleSheet(QString("background: %1;").arg(alt ? ui::colors::ROW_ALT() : "transparent"));
        link_screen(row, QStringLiteral("economics"), tr("Double-click to open the Economics screen"));
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(8, 4, 8, 4);

        // Sentiment colours the event-name label so the user can scan
        // positive/negative releases at a glance. Falls back to the standard
        // primary text colour when sentiment is missing or "neutral".
        QString event_color = ui::colors::TEXT_PRIMARY();
        if (sentiment == QLatin1String("positive"))
            event_color = ui::colors::POSITIVE();
        else if (sentiment == QLatin1String("negative"))
            event_color = ui::colors::NEGATIVE();

        // Build a rich tooltip with every field that doesn't fit inline:
        // category, sentiment, symbol, revised_from. Tooltip plain-text only
        // (no HTML) to keep rendering cheap.
        QStringList tip_lines;
        tip_lines << event_name;
        if (!category.isEmpty())
            tip_lines << QStringLiteral("Category: %1").arg(category);
        if (!symbol.isEmpty())
            tip_lines << QStringLiteral("Symbol: %1").arg(symbol);
        if (!reference_period.isEmpty())
            tip_lines << QStringLiteral("Period: %1").arg(reference_period);
        if (!sentiment.isEmpty())
            tip_lines << QStringLiteral("Sentiment: %1").arg(sentiment);
        if (!revised_from.isEmpty())
            tip_lines << QStringLiteral("Revised from: %1").arg(revised_from);
        if (release.isValid())
            tip_lines << tr("Release: %1 %2 (source time read as UTC)")
                             .arg(ui::formatting::date_time(release, ui::formatting::DateStyle::Medium),
                                  ui::formatting::zone_label(release));
        const QString tooltip = tip_lines.join('\n');

        // Event name — Ignored size policy so it doesn't claim its sizeHint
        // (which varies per row and was pushing the other columns sideways).
        // The stretch=1 below + fixed-width neighbours mean it gets exactly
        // the leftover space.
        QString display_name = event_name;
        if (display_name.length() > 28)
            display_name = display_name.left(26) + "…";
        auto* ev_lbl = new QLabel(display_name);
        ev_lbl->setToolTip(tooltip);
        ev_lbl->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        ev_lbl->setMinimumWidth(econ_col_w(kColWidthEventMin)); // never squeezed to nothing (see kColWidthEventMin)
        ev_lbl->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-4) + "px; background: transparent;").arg(event_color));
        rl->addWidget(ev_lbl, 1);

        auto* cty_lbl = new QLabel(country);
        cty_lbl->setFixedWidth(econ_col_w(kColWidthCty));
        cty_lbl->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-5) + "px; background: transparent;").arg(ui::colors::CYAN()));
        rl->addWidget(cty_lbl, 0);

        auto* date_lbl = new QLabel(date_display);
        date_lbl->setFixedWidth(econ_col_w(kColWidthDate));
        date_lbl->setStyleSheet(
            QString("color: %1; font-size: " + fincept::ui::fonts::px(-5) + "px; background: transparent;").arg(ui::colors::TEXT_SECONDARY()));
        rl->addWidget(date_lbl, 0);

        auto* ref_lbl = new QLabel(reference_period.isEmpty() ? "--" : reference_period);
        ref_lbl->setFixedWidth(econ_col_w(kColWidthRef));
        ref_lbl->setStyleSheet(
            QString("color: %1; font-size: " + fincept::ui::fonts::px(-5) + "px; background: transparent;")
                .arg(reference_period.isEmpty() ? ui::colors::TEXT_TERTIARY() : ui::colors::TEXT_SECONDARY()));
        rl->addWidget(ref_lbl, 0);

        auto* act_lbl = new QLabel(actual.isEmpty() ? "--" : actual);
        act_lbl->setFixedWidth(econ_col_w(kColWidthAct));
        act_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        act_lbl->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-4) + "px; font-weight: bold; background: transparent;")
                                   .arg(actual.isEmpty() ? ui::colors::TEXT_TERTIARY() : ui::colors::TEXT_PRIMARY()));
        rl->addWidget(act_lbl, 0);

        auto* fcst_lbl = new QLabel(forecast.isEmpty() ? "--" : forecast);
        fcst_lbl->setFixedWidth(econ_col_w(kColWidthFcst));
        fcst_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        fcst_lbl->setStyleSheet(
            QString("color: %1; font-size: " + fincept::ui::fonts::px(-5) + "px; background: transparent;").arg(ui::colors::TEXT_SECONDARY()));
        rl->addWidget(fcst_lbl, 0);

        auto* cons_lbl = new QLabel(consensus.isEmpty() ? "--" : consensus);
        cons_lbl->setFixedWidth(econ_col_w(kColWidthCons));
        cons_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        cons_lbl->setStyleSheet(
            QString("color: %1; font-size: " + fincept::ui::fonts::px(-5) + "px; background: transparent;")
                .arg(consensus.isEmpty() ? ui::colors::TEXT_TERTIARY() : ui::colors::TEXT_SECONDARY()));
        rl->addWidget(cons_lbl, 0);

        auto* prev_lbl = new QLabel(previous.isEmpty() ? "--" : previous);
        prev_lbl->setFixedWidth(econ_col_w(kColWidthPrev));
        prev_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        prev_lbl->setStyleSheet(
            QString("color: %1; font-size: " + fincept::ui::fonts::px(-5) + "px; background: transparent;")
                .arg(previous.isEmpty() ? ui::colors::TEXT_TERTIARY() : ui::colors::TEXT_SECONDARY()));
        rl->addWidget(prev_lbl, 0);

        auto* imp_lbl = new QLabel(imp_text);
        imp_lbl->setFixedWidth(econ_col_w(kColWidthImp));
        imp_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        imp_lbl->setStyleSheet(
            QString("color: %1; font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: bold; background: transparent;").arg(imp_color));
        rl->addWidget(imp_lbl, 0);

        // Low-priority columns, hidden by tile width (apply_column_visibility).
        ref_cells_.append(ref_lbl);
        fcst_cells_.append(fcst_lbl);
        cons_cells_.append(cons_lbl);
        prev_cells_.append(prev_lbl);

        list_layout_->addWidget(row);
        alt = !alt;
        ++count;
    }

    if (count == 0) {
        show_status(events.isEmpty() ? tr("No events available")
                                     : tr("No events match the filters (%n event(s) in the feed)", "",
                                          int(events.size())));
        return;
    }

    list_layout_->addStretch();
    apply_column_visibility(); // the new rows follow the tile's current width
}

void EconomicCalendarWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("ECONOMIC CALENDAR"));
    if (!last_events_.isEmpty())
        populate(last_events_); // re-renders header + row labels in the new language
}

} // namespace fincept::screens::widgets
