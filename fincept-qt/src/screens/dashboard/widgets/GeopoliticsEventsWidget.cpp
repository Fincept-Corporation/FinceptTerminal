#include "screens/dashboard/widgets/GeopoliticsEventsWidget.h"

#include "datahub/DataHub.h"
#include "services/geopolitics/GeopoliticsTypes.h"
#include "ui/theme/Theme.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QSpinBox>

namespace fincept::screens::widgets {

namespace {
QString format_relative(const QString& iso) {
    if (iso.isEmpty())
        return QStringLiteral("--");
    const QDateTime dt = QDateTime::fromString(iso, Qt::ISODate);
    if (!dt.isValid())
        return iso.left(10);
    const qint64 secs = dt.secsTo(QDateTime::currentDateTimeUtc());
    if (secs < 60)
        return QStringLiteral("now");
    if (secs < 3600)
        return QString::number(secs / 60) + QStringLiteral("m");
    if (secs < 86400)
        return QString::number(secs / 3600) + QStringLiteral("h");
    return QString::number(secs / 86400) + QStringLiteral("d");
}
} // namespace

GeopoliticsEventsWidget::GeopoliticsEventsWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget("GEOPOLITICS EVENTS", parent, ui::colors::NEGATIVE()) {
    auto* vl = content_layout();
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Column headers
    header_widget_ = new QWidget(this);
    auto* hl = new QHBoxLayout(header_widget_);
    hl->setContentsMargins(8, 4, 8, 4);

    auto make_hdr = [&](const QString& text, int stretch, Qt::Alignment align = Qt::AlignLeft) {
        auto* lbl = new QLabel(text);
        lbl->setAlignment(align);
        header_labels_.append(lbl);
        hl->addWidget(lbl, stretch);
    };
    make_hdr("CAT", 1);
    make_hdr("EVENT", 5);
    make_hdr("LOCATION", 2);
    make_hdr("SRC", 1);
    make_hdr("AGE", 1, Qt::AlignRight);
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

    status_label_ = new QLabel("Loading…");
    status_label_->setAlignment(Qt::AlignCenter);
    list_layout_->addWidget(status_label_);
    list_layout_->addStretch();

    scroll_area_->setWidget(list_widget);
    vl->addWidget(scroll_area_, 1);

    connect(this, &BaseWidget::refresh_requested, this, []() {
        datahub::DataHub::instance().request(QStringLiteral("geopolitics:events"), /*force=*/true);
    });

    set_configurable(true);
    apply_styles();
    apply_config(cfg);
    set_loading(true);
}

QJsonObject GeopoliticsEventsWidget::config() const {
    QJsonObject o;
    o.insert("max_rows", max_rows_);
    return o;
}

void GeopoliticsEventsWidget::apply_config(const QJsonObject& cfg) {
    max_rows_ = qBound(5, cfg.value("max_rows").toInt(15), 100);
    if (!last_payload_.isNull())
        populate(last_payload_);
}

void GeopoliticsEventsWidget::hub_resubscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    hub.subscribe(this, QStringLiteral("geopolitics:events"),
                  [this](const QVariant& v) { populate(v); });

    // Seed from cache so the widget never shows blank while waiting.
    const QVariant cached = hub.peek(QStringLiteral("geopolitics:events"));
    if (cached.isValid())
        populate(cached);
    hub_active_ = true;
}

void GeopoliticsEventsWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void GeopoliticsEventsWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        hub_resubscribe();
}

void GeopoliticsEventsWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void GeopoliticsEventsWidget::clear_rows() {
    while (list_layout_->count() > 0) {
        auto* item = list_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    status_label_ = nullptr;
}

void GeopoliticsEventsWidget::populate(const QVariant& payload) {
    last_payload_ = payload;

    using fincept::services::geo::EventsPage;
    if (!payload.canConvert<EventsPage>()) {
        clear_rows();
        status_label_ = new QLabel("Awaiting events…");
        status_label_->setAlignment(Qt::AlignCenter);
        status_label_->setStyleSheet(QString("color:%1;font-size:10px;padding:16px;background:transparent;")
                                         .arg(ui::colors::TEXT_TERTIARY()));
        list_layout_->addWidget(status_label_);
        list_layout_->addStretch();
        return;
    }

    const EventsPage page = payload.value<EventsPage>();
    clear_rows();

    if (page.events.isEmpty()) {
        status_label_ = new QLabel("No events available");
        status_label_->setAlignment(Qt::AlignCenter);
        status_label_->setStyleSheet(QString("color:%1;font-size:10px;padding:16px;background:transparent;")
                                         .arg(ui::colors::TEXT_TERTIARY()));
        list_layout_->addWidget(status_label_);
        list_layout_->addStretch();
        set_loading(false);
        return;
    }

    bool alt = false;
    int count = 0;

    for (const auto& ev : page.events) {
        if (count >= max_rows_)
            break;

        QString title = ev.title.trimmed();
        if (title.isEmpty())
            continue;

        QString location;
        if (!ev.city.isEmpty() && !ev.country.isEmpty())
            location = ev.city + ", " + ev.country;
        else if (!ev.country.isEmpty())
            location = ev.country;
        else if (!ev.city.isEmpty())
            location = ev.city;
        else
            location = QStringLiteral("--");

        const QColor cat_col = fincept::services::geo::category_color(ev.event_category);
        const QString cat_label = fincept::services::geo::pretty_category(ev.event_category);

        auto* row = new QWidget(this);
        row->setStyleSheet(QString("background: %1;").arg(alt ? ui::colors::BG_RAISED() : "transparent"));
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(8, 4, 8, 4);

        // Category color dot
        auto* cat_lbl = new QLabel(QStringLiteral("●"));
        cat_lbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        cat_lbl->setToolTip(cat_label);
        cat_lbl->setStyleSheet(
            QString("color:%1;font-size:12px;background:transparent;").arg(cat_col.name()));
        rl->addWidget(cat_lbl, 1);

        // Title
        QString display_title = title;
        if (display_title.length() > 60)
            display_title = display_title.left(58) + QStringLiteral("…");
        auto* title_lbl = new QLabel(display_title);
        title_lbl->setToolTip(title);
        title_lbl->setStyleSheet(QString("color:%1;font-size:10px;background:transparent;")
                                     .arg(ui::colors::TEXT_PRIMARY()));
        rl->addWidget(title_lbl, 5);

        // Location
        auto* loc_lbl = new QLabel(location);
        loc_lbl->setStyleSheet(QString("color:%1;font-size:9px;background:transparent;")
                                   .arg(ui::colors::CYAN()));
        rl->addWidget(loc_lbl, 2);

        // Source
        QString src = ev.source;
        if (src.length() > 8)
            src = src.left(8);
        auto* src_lbl = new QLabel(src.toUpper());
        src_lbl->setToolTip(ev.source);
        src_lbl->setStyleSheet(QString("color:%1;font-size:9px;background:transparent;")
                                   .arg(ui::colors::TEXT_SECONDARY()));
        rl->addWidget(src_lbl, 1);

        // Age
        const QString age =
            !ev.created_at.isEmpty() ? format_relative(ev.created_at) : format_relative(ev.extracted_date);
        auto* age_lbl = new QLabel(age);
        age_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        age_lbl->setStyleSheet(QString("color:%1;font-size:9px;background:transparent;")
                                   .arg(ui::colors::TEXT_TERTIARY()));
        rl->addWidget(age_lbl, 1);

        list_layout_->addWidget(row);
        alt = !alt;
        ++count;
    }

    list_layout_->addStretch();
    set_loading(false);
}

QDialog* GeopoliticsEventsWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle("Configure — Geopolitics Events");
    auto* form = new QFormLayout(dlg);

    auto* spin = new QSpinBox(dlg);
    spin->setRange(5, 100);
    spin->setValue(max_rows_);
    form->addRow("Max rows", spin);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    form->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, dlg, [this, dlg, spin]() {
        QJsonObject cfg;
        cfg.insert("max_rows", spin->value());
        apply_config(cfg);
        emit config_changed(cfg);
        dlg->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    return dlg;
}

void GeopoliticsEventsWidget::on_theme_changed() {
    apply_styles();
    if (!last_payload_.isNull())
        populate(last_payload_);
}

void GeopoliticsEventsWidget::apply_styles() {
    header_widget_->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_RAISED()));
    for (auto* lbl : header_labels_)
        lbl->setStyleSheet(QString("color:%1;font-size:9px;font-weight:bold;background:transparent;")
                               .arg(ui::colors::TEXT_TERTIARY()));
    header_sep_->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM()));
    scroll_area_->setStyleSheet(
        QString("QScrollArea{border:none;background:transparent;}"
                "QScrollBar:vertical{width:4px;background:transparent;}"
                "QScrollBar::handle:vertical{background:%1;border-radius:2px;min-height:20px;}"
                "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
            .arg(ui::colors::BORDER_MED()));
    if (status_label_)
        status_label_->setStyleSheet(QString("color:%1;font-size:10px;padding:16px;background:transparent;")
                                         .arg(ui::colors::TEXT_TERTIARY()));
}

} // namespace fincept::screens::widgets
