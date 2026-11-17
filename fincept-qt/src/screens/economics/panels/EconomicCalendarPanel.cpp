// src/screens/economics/panels/EconomicCalendarPanel.cpp
// Forex Factory economic calendar — Forex Factory's public weekly JSON export, no browser or API key.
// It covers the current week only and has forecast / previous values but no actuals.
//
// Script: economic_calendar.py day <YYYY-MM-DD>  (the connectors' <command> [args] form; the bare
// "mar28.2026" form still works); day and times in local time
// Response: { date, url, events_count, week_start, week_end, timezone, events: [
//   { time, currency, impact:int(0-3), impact_label, event, forecast, previous, datetime }
// ]}
// Impact levels: 1=low, 2=medium, 3=high, 0=holiday / non-economic
#include "screens/economics/panels/EconomicCalendarPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QDate>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>

namespace fincept::screens {
namespace {

static constexpr const char* kEconomicCalendarScript = "economic_calendar.py";
static constexpr const char* kEconomicCalendarSourceId = "econ_calendar";
static constexpr const char* kEconomicCalendarColor = "#D97706"; // amber
} // namespace

EconomicCalendarPanel::EconomicCalendarPanel(QWidget* parent)
    : EconPanelBase(kEconomicCalendarSourceId, kEconomicCalendarColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &EconomicCalendarPanel::on_result);
}

void EconomicCalendarPanel::activate() {
    // The calendar opens on this week: a remembered week that has passed is replaced (the query runs
    // again on show); a date the user picked inside this week is kept.
    if (!date_edit_)
        return;
    const QDate today = QDate::currentDate();
    int year_now = 0;
    int year_shown = 0;
    const int week_now = today.weekNumber(&year_now);
    const int week_shown = date_edit_->date().weekNumber(&year_shown);
    if (week_now != week_shown || year_now != year_shown) {
        date_edit_->setDate(today);
        invalidate_result();
    }
}

EconPanelBase::LoadPolicy EconomicCalendarPanel::load_policy() const {
    LoadPolicy p;
    p.shape = ui::LoadingSkeleton::Shape::List; // a list of the day's events
    p.max_age_s = 3600; // actual / forecast values are filled in during the week
    return p;
}

void EconomicCalendarPanel::build_controls(QHBoxLayout* thl) {
    auto lbl = [](const QString& t) {
        auto* l = new QLabel(t);
        l->setObjectName(QStringLiteral("econCtrlLabel"));
        return l;
    };

    date_edit_ = new QDateEdit(QDate::currentDate());
    date_edit_->setDisplayFormat("MMM dd yyyy");
    date_edit_->setCalendarPopup(true);
    date_edit_->setMinimumHeight(26);
    set_text_width(date_edit_, 120);

    filter_combo_ = new QComboBox;
    filter_combo_->addItem(tr("All Events"), 0);
    filter_combo_->addItem(tr("Medium + High"), 2);
    filter_combo_->addItem(tr("High Only"), 3);
    filter_combo_->setMinimumHeight(26);
    set_text_width(filter_combo_, 110);

    thl->addWidget(date_lbl_ = lbl(tr("DATE")));
    thl->addWidget(date_edit_);
    thl->addWidget(impact_lbl_ = lbl(tr("IMPACT")));
    thl->addWidget(filter_combo_);
}

void EconomicCalendarPanel::on_fetch() {
    const QDate date = date_edit_->date();
    const QString iso = date.toString(Qt::ISODate);

    show_loading(tr("Fetching economic calendar for %1…").arg(date.toString("MMM d, yyyy")));

    // economic_calendar.py day <date> — the script's <command> [args] form.
    execute_query(kEconomicCalendarSourceId, kEconomicCalendarScript, QStringLiteral("day"), {iso},
                  "calendar_" + iso);
}

void EconomicCalendarPanel::on_result(const QString& request_id, const services::EconomicsResult& result) {
    if (result.source_id != kEconomicCalendarSourceId)
        return;
    if (!request_id.startsWith("calendar_"))
        return;
    if (!result.success) {
        show_error(result.error);
        return;
    }

    QJsonArray events = result.data["events"].toArray();

    // Fallback: service normalised shape
    if (events.isEmpty())
        events = result.data["data"].toArray();

    if (events.isEmpty()) {
        show_empty(tr("No events found for this date"));
        return;
    }

    // Apply impact filter
    const int min_impact = filter_combo_->currentData().toInt();
    if (min_impact > 0) {
        QJsonArray filtered;
        for (const auto& ev : events) {
            if (ev.toObject()["impact"].toInt() >= min_impact)
                filtered.append(ev);
        }
        events = filtered;
    }

    if (events.isEmpty()) {
        show_empty(tr("No events match the selected impact filter"));
        return;
    }

    const QString date_str = date_edit_->date().toString("MMM d, yyyy");
    const int total = result.data["events_count"].toInt(events.size());
    const QString title = tr("Economic Calendar — %1  (%2 events)").arg(date_str).arg(total);

    SeriesMeta meta;
    meta.note = tr("Times in %1 · Forex Factory weekly export: forecast and previous, no actuals")
                    .arg(result.data["timezone"].toString(tr("local time")));
    set_series_meta(meta);
    set_stats_visible(false); // a list of events, not a series
    display(events, title,
            {QStringLiteral("time"), QStringLiteral("currency"), QStringLiteral("impact_label"),
             QStringLiteral("event"), QStringLiteral("forecast"), QStringLiteral("previous"), QStringLiteral("impact")},
            /*keep_row_order=*/true);
    LOG_DEBUG("EconomicCalendarPanel", QString("Displayed %1 events for %2").arg(events.size()).arg(date_str));
}

// ── i18n ──────────────────────────────────────────────────────────────────────

void EconomicCalendarPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    EconPanelBase::changeEvent(event);
}

void EconomicCalendarPanel::retranslateUi() {
    if (date_lbl_)
        date_lbl_->setText(tr("DATE"));
    if (impact_lbl_)
        impact_lbl_->setText(tr("IMPACT"));
    if (filter_combo_) {
        filter_combo_->setItemText(0, tr("All Events"));
        filter_combo_->setItemText(1, tr("Medium + High"));
        filter_combo_->setItemText(2, tr("High Only"));
    }
    EconPanelBase::retranslateUi();
}

} // namespace fincept::screens
