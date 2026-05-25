#include "screens/dashboard/widgets/EconomicCalendarWidget.h"

#include "datahub/DataHub.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QJsonObject>

namespace {
constexpr const char* kTopic = "econ:fincept:upcoming_events";

// Fixed column widths so header + rows line up regardless of event-name
// length. QHBoxLayout's stretch factors alone don't align columns when
// each row has different text widths — long event names push their
// neighbours rightward, breaking the table look. EVENT is the only
// variable column; it takes whatever leftover space the layout has.
constexpr int kColWidthCty   = 32;
constexpr int kColWidthDate  = 86;
constexpr int kColWidthRef   = 40;
constexpr int kColWidthAct   = 56;
constexpr int kColWidthFcst  = 48;
constexpr int kColWidthCons  = 48;
constexpr int kColWidthPrev  = 48;
constexpr int kColWidthImp   = 40;
} // namespace

namespace fincept::screens::widgets {

EconomicCalendarWidget::EconomicCalendarWidget(QWidget* parent)
    : BaseWidget(tr("ECONOMIC CALENDAR"), parent, ui::colors::CYAN()) {
    auto* vl = content_layout();
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Column headers
    header_widget_ = new QWidget(this);
    auto* hl = new QHBoxLayout(header_widget_);
    hl->setContentsMargins(8, 4, 8, 4);

    auto make_hdr = [&](const QString& text, int fixed_w, int stretch, Qt::Alignment align = Qt::AlignLeft) {
        auto* lbl = new QLabel(text);
        lbl->setAlignment(align);
        if (fixed_w > 0)
            lbl->setFixedWidth(fixed_w);
        else
            lbl->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
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
    connect(this, &BaseWidget::refresh_requested, this, [this]() {
        set_loading(true);
        datahub::DataHub::instance().request(QString::fromLatin1(kTopic), /*force=*/true);
    });

    apply_styles();
    set_loading(true);
}

void EconomicCalendarWidget::apply_styles() {
    header_widget_->setStyleSheet(QString("background: %1;").arg(ui::colors::BG_RAISED()));
    for (auto* lbl : header_labels_)
        lbl->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;")
                               .arg(ui::colors::TEXT_TERTIARY()));
    header_sep_->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER_DIM()));
    scroll_area_->setStyleSheet(
        QString("QScrollArea { border: none; background: transparent; }"
                "QScrollBar:vertical { width: 4px; background: transparent; }"
                "QScrollBar::handle:vertical { background: %1; border-radius: 2px; min-height: 20px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
            .arg(ui::colors::BORDER_MED()));
    status_label_->setStyleSheet(QString("color: %1; font-size: 10px; padding: 16px; background: transparent;")
                                     .arg(ui::colors::TEXT_TERTIARY()));
}

void EconomicCalendarWidget::on_theme_changed() {
    apply_styles();
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
    if (!hub_active_)
        hub_subscribe();
}

void EconomicCalendarWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe();
}

void EconomicCalendarWidget::hub_subscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.subscribe(this, QString::fromLatin1(kTopic), [this](const QVariant& v) {
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
    hub.subscribe_errors(this, QString::fromLatin1(kTopic),
        [this](const QString& /*error*/) {
            set_loading(false);
            show_status(tr("Failed to load calendar"));
        });
    hub_active_ = true;
    // Cold-cache fallback: if the producer warmed the topic earlier, paint
    // it now even if it's slightly stale — beats a blank panel.
    QVariant cached = hub.peek_raw(QString::fromLatin1(kTopic));
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
    hub.unsubscribe(this, QString::fromLatin1(kTopic));
    hub.unsubscribe_errors(this, QString::fromLatin1(kTopic));
    hub_active_ = false;
}

void EconomicCalendarWidget::populate(const QJsonArray& events) {
    last_events_ = events;

    clear_list();

    bool alt = false;
    int count = 0;

    for (const auto& v : events) {
        if (count >= 25)
            break;
        auto e = v.toObject();

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
        QString imp_text = imp_int >= 3 ? tr("HIGH") : imp_int == 2 ? tr("MED") : imp_int == 1 ? tr("LOW") : "--";

        auto* row = new QWidget(this);
        row->setStyleSheet(QString("background: %1;").arg(alt ? ui::colors::BG_RAISED() : "transparent"));
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
        ev_lbl->setStyleSheet(
            QString("color: %1; font-size: 10px; background: transparent;").arg(event_color));
        rl->addWidget(ev_lbl, 1);

        auto* cty_lbl = new QLabel(country);
        cty_lbl->setFixedWidth(kColWidthCty);
        cty_lbl->setStyleSheet(QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::CYAN()));
        rl->addWidget(cty_lbl, 0);

        auto* date_lbl = new QLabel(date_display);
        date_lbl->setFixedWidth(kColWidthDate);
        date_lbl->setStyleSheet(
            QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::TEXT_SECONDARY()));
        rl->addWidget(date_lbl, 0);

        auto* ref_lbl = new QLabel(reference_period.isEmpty() ? "--" : reference_period);
        ref_lbl->setFixedWidth(kColWidthRef);
        ref_lbl->setStyleSheet(
            QString("color: %1; font-size: 9px; background: transparent;")
                .arg(reference_period.isEmpty() ? ui::colors::TEXT_TERTIARY() : ui::colors::TEXT_SECONDARY()));
        rl->addWidget(ref_lbl, 0);

        auto* act_lbl = new QLabel(actual.isEmpty() ? "--" : actual);
        act_lbl->setFixedWidth(kColWidthAct);
        act_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        act_lbl->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;")
                                   .arg(actual.isEmpty() ? ui::colors::TEXT_TERTIARY() : ui::colors::TEXT_PRIMARY()));
        rl->addWidget(act_lbl, 0);

        auto* fcst_lbl = new QLabel(forecast.isEmpty() ? "--" : forecast);
        fcst_lbl->setFixedWidth(kColWidthFcst);
        fcst_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        fcst_lbl->setStyleSheet(
            QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::TEXT_SECONDARY()));
        rl->addWidget(fcst_lbl, 0);

        auto* cons_lbl = new QLabel(consensus.isEmpty() ? "--" : consensus);
        cons_lbl->setFixedWidth(kColWidthCons);
        cons_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        cons_lbl->setStyleSheet(
            QString("color: %1; font-size: 9px; background: transparent;")
                .arg(consensus.isEmpty() ? ui::colors::TEXT_TERTIARY() : ui::colors::TEXT_SECONDARY()));
        rl->addWidget(cons_lbl, 0);

        auto* prev_lbl = new QLabel(previous.isEmpty() ? "--" : previous);
        prev_lbl->setFixedWidth(kColWidthPrev);
        prev_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        prev_lbl->setStyleSheet(
            QString("color: %1; font-size: 9px; background: transparent;")
                .arg(previous.isEmpty() ? ui::colors::TEXT_TERTIARY() : ui::colors::TEXT_SECONDARY()));
        rl->addWidget(prev_lbl, 0);

        auto* imp_lbl = new QLabel(imp_text);
        imp_lbl->setFixedWidth(kColWidthImp);
        imp_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        imp_lbl->setStyleSheet(
            QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;").arg(imp_color));
        rl->addWidget(imp_lbl, 0);

        list_layout_->addWidget(row);
        alt = !alt;
        ++count;
    }

    if (count == 0) {
        show_status(tr("No events available"));
        return;
    }

    list_layout_->addStretch();
}

void EconomicCalendarWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("ECONOMIC CALENDAR"));
    if (!last_events_.isEmpty())
        populate(last_events_); // re-renders header + row labels in the new language
}

} // namespace fincept::screens::widgets
