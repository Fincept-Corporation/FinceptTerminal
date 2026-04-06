#include "screens/dashboard/widgets/EconomicCalendarWidget.h"

#include "network/http/HttpClient.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QJsonObject>

namespace fincept::screens::widgets {

EconomicCalendarWidget::EconomicCalendarWidget(QWidget* parent)
    : BaseWidget("ECONOMIC CALENDAR", parent, ui::colors::CYAN()) {
    auto* vl = content_layout();
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Column headers
    auto* header = new QWidget;
    header->setStyleSheet(QString("background: %1;").arg(ui::colors::BG_RAISED()));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(8, 4, 8, 4);

    auto make_hdr = [&](const QString& text, int stretch, Qt::Alignment align = Qt::AlignLeft) {
        auto* lbl = new QLabel(text);
        lbl->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;")
                               .arg(ui::colors::TEXT_TERTIARY()));
        lbl->setAlignment(align);
        hl->addWidget(lbl, stretch);
    };
    make_hdr("EVENT", 4);
    make_hdr("CTY", 1);
    make_hdr("DATE", 2);
    make_hdr("ACT", 1, Qt::AlignRight);
    make_hdr("FCST", 1, Qt::AlignRight);
    make_hdr("IMP", 1, Qt::AlignRight);
    vl->addWidget(header);

    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER_DIM()));
    vl->addWidget(sep);

    // Scrollable list
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }"
                          "QScrollBar:vertical { width: 4px; background: transparent; }"
                          "QScrollBar::handle:vertical { background: #222222; border-radius: 2px; min-height: 20px; }"
                          "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");

    auto* list_widget = new QWidget;
    list_widget->setStyleSheet("background: transparent;");
    list_layout_ = new QVBoxLayout(list_widget);
    list_layout_->setContentsMargins(0, 0, 0, 0);
    list_layout_->setSpacing(0);

    status_label_ = new QLabel("Loading...");
    status_label_->setAlignment(Qt::AlignCenter);
    status_label_->setStyleSheet(
        QString("color: %1; font-size: 10px; padding: 16px; background: transparent;").arg(ui::colors::TEXT_TERTIARY()));
    list_layout_->addWidget(status_label_);
    list_layout_->addStretch();

    scroll->setWidget(list_widget);
    vl->addWidget(scroll, 1);

    connect(this, &BaseWidget::refresh_requested, this, &EconomicCalendarWidget::refresh_data);
    set_loading(true);
    refresh_data();
}

void EconomicCalendarWidget::refresh_data() {
    set_loading(true);

    // Response shape: {"success":true,"data":{"events":[...],"total_count":N,...}}
    QString url = "https://api.fincept.in/macro/upcoming-events?limit=25";

    fincept::HttpClient::instance().get(url, [this](fincept::Result<QJsonDocument> result) {
        set_loading(false);
        if (!result.is_ok()) {
            status_label_->setVisible(true);
            status_label_->setText("Failed to load calendar");
            return;
        }

        auto doc = result.value();
        QJsonArray events;

        if (doc.isObject()) {
            auto root = doc.object();
            // {"success":true,"data":{"events":[...]}}
            if (root.contains("data") && root["data"].isObject()) {
                auto data = root["data"].toObject();
                if (data.contains("events") && data["events"].isArray())
                    events = data["events"].toArray();
            }
            // fallback: {"data":[...]}
            if (events.isEmpty() && root.contains("data") && root["data"].isArray())
                events = root["data"].toArray();
            // fallback: {"events":[...]}
            if (events.isEmpty() && root.contains("events") && root["events"].isArray())
                events = root["events"].toArray();
        } else if (doc.isArray()) {
            events = doc.array();
        }

        if (events.isEmpty()) {
            status_label_->setVisible(true);
            status_label_->setText("No events available");
            return;
        }

        status_label_->setVisible(false);
        populate(events);
    });
}

void EconomicCalendarWidget::populate(const QJsonArray& events) {
    // Clear list
    while (list_layout_->count() > 0) {
        auto* item = list_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    bool alt = false;
    int count = 0;

    for (const auto& v : events) {
        if (count >= 25)
            break;
        auto e = v.toObject();

        // Real fields: event, country, date, time, importance, actual, forecast, previous
        QString event_name = e["event"].toString().trimmed();
        if (event_name.isEmpty())
            continue;

        QString country = e["country"].toString().toUpper();
        QString date = e["date"].toString();
        QString time_str = e["time"].toString().trimmed();
        QString actual = e["actual"].toString().trimmed();
        QString forecast = e["forecast"].toString().trimmed();
        int imp_int = e["importance"].toInt(0);

        // Date: show as MMM-DD
        QString date_display = date;
        if (date.length() == 10) { // YYYY-MM-DD
            QStringList parts = date.split('-');
            if (parts.size() == 3) {
                static const char* months[] = {"",    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
                int m = parts[1].toInt();
                if (m >= 1 && m <= 12)
                    date_display = QString("%1-%2").arg(months[m]).arg(parts[2]);
            }
        }
        if (!time_str.isEmpty())
            date_display += " " + time_str.left(5);

        // Importance color: 0=dim, 1=low/dim, 2=medium/amber, 3=high/red
        QString imp_color = imp_int >= 3   ? ui::colors::NEGATIVE()
                            : imp_int == 2 ? ui::colors::WARNING()
                                           : ui::colors::TEXT_TERTIARY();
        QString imp_text = imp_int >= 3 ? "HIGH" : imp_int == 2 ? "MED" : imp_int == 1 ? "LOW" : "--";

        auto* row = new QWidget;
        row->setStyleSheet(QString("background: %1;").arg(alt ? ui::colors::BG_RAISED() : "transparent"));
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(8, 4, 8, 4);

        // Event name
        QString display_name = event_name;
        if (display_name.length() > 28)
            display_name = display_name.left(26) + "…";
        auto* ev_lbl = new QLabel(display_name);
        ev_lbl->setToolTip(event_name); // full name on hover
        ev_lbl->setStyleSheet(
            QString("color: %1; font-size: 10px; background: transparent;").arg(ui::colors::TEXT_PRIMARY()));
        rl->addWidget(ev_lbl, 4);

        auto* cty_lbl = new QLabel(country);
        cty_lbl->setStyleSheet(QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::CYAN()));
        rl->addWidget(cty_lbl, 1);

        auto* date_lbl = new QLabel(date_display);
        date_lbl->setStyleSheet(
            QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::TEXT_SECONDARY()));
        rl->addWidget(date_lbl, 2);

        auto* act_lbl = new QLabel(actual.isEmpty() ? "--" : actual);
        act_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        act_lbl->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;")
                                   .arg(actual.isEmpty() ? ui::colors::TEXT_TERTIARY() : ui::colors::TEXT_PRIMARY()));
        rl->addWidget(act_lbl, 1);

        auto* fcst_lbl = new QLabel(forecast.isEmpty() ? "--" : forecast);
        fcst_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        fcst_lbl->setStyleSheet(
            QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::TEXT_SECONDARY()));
        rl->addWidget(fcst_lbl, 1);

        auto* imp_lbl = new QLabel(imp_text);
        imp_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        imp_lbl->setStyleSheet(
            QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;").arg(imp_color));
        rl->addWidget(imp_lbl, 1);

        list_layout_->addWidget(row);
        alt = !alt;
        ++count;
    }

    list_layout_->addStretch();
}

} // namespace fincept::screens::widgets
