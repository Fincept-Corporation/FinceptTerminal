// src/screens/economics/panels/EconomicCalendarPanel.cpp
// Forex Factory economic calendar scraper — no API key required.
//
// Script: economic_calendar.py <mmmDD.YYYY>  (e.g. "mar28.2026")
// Response: { date, url, events_count, events: [
//   { time, currency, impact:int(1-3), event, actual, forecast, previous }
// ]}
// Impact levels: 1=low, 2=medium, 3=high
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

// Convert QDate to Forex Factory date string: "mar28.2026"
static QString to_ff_date(const QDate& d) {
    static const char* months[] = {"",    "jan", "feb", "mar", "apr", "may", "jun",
                                   "jul", "aug", "sep", "oct", "nov", "dec"};
    return QString("%1%2.%3").arg(months[d.month()]).arg(d.day()).arg(d.year());
}

EconomicCalendarPanel::EconomicCalendarPanel(QWidget* parent)
    : EconPanelBase(kEconomicCalendarSourceId, kEconomicCalendarColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &EconomicCalendarPanel::on_result);
}

void EconomicCalendarPanel::activate() {
    // Default to today on first activation
    if (date_edit_) {
        date_edit_->setDate(QDate::currentDate());
    }
    show_empty("Select a date and click FETCH to load economic events\n"
               "Source: Forex Factory — global economic calendar (no API key required)");
}

void EconomicCalendarPanel::build_controls(QHBoxLayout* thl) {
    auto lbl = [](const QString& t) {
        auto* l = new QLabel(t);
        l->setStyleSheet(ctrl_label_style());
        return l;
    };

    date_edit_ = new QDateEdit(QDate::currentDate());
    date_edit_->setDisplayFormat("MMM dd yyyy");
    date_edit_->setCalendarPopup(true);
    date_edit_->setFixedHeight(26);
    date_edit_->setFixedWidth(120);

    filter_combo_ = new QComboBox;
    filter_combo_->addItem("All Events", 0);
    filter_combo_->addItem("Medium + High", 2);
    filter_combo_->addItem("High Only", 3);
    filter_combo_->setFixedHeight(26);
    filter_combo_->setFixedWidth(110);

    thl->addWidget(lbl("DATE"));
    thl->addWidget(date_edit_);
    thl->addWidget(lbl("IMPACT"));
    thl->addWidget(filter_combo_);
}

void EconomicCalendarPanel::on_fetch() {
    const QDate date = date_edit_->date();
    const QString ff_date = to_ff_date(date);

    show_loading("Fetching economic calendar for " + date.toString("MMM d, yyyy") + "…");

    services::EconomicsService::instance().execute(kEconomicCalendarSourceId, kEconomicCalendarScript, ff_date, {},
                                                   "calendar_" + ff_date);
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
        show_empty("No events found for this date");
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
        show_empty("No events match the selected impact filter");
        return;
    }

    const QString date_str = date_edit_->date().toString("MMM d, yyyy");
    const int total = result.data["events_count"].toInt(events.size());
    const QString title = QString("Economic Calendar — %1  (%2 events)").arg(date_str).arg(total);

    display(events, title);
    LOG_INFO("EconomicCalendarPanel", QString("Displayed %1 events for %2").arg(events.size()).arg(date_str));
}

} // namespace fincept::screens
