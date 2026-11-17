#include "screens/dashboard/widgets/MaritimeVesselsWidget.h"

#include "datahub/DataHub.h"
#include "screens/dashboard/widgets/TileBilling.h"
#include "services/maritime/MaritimeTypes.h"
#include "ui/theme/Theme.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QPlainTextEdit>
#include <QTimer>

namespace fincept::screens::widgets {

namespace {
QStringList default_imos() {
    // Mix of well-known mega-containers and tankers — gives the widget a
    // populated default state. Users override via gear icon.
    return {QStringLiteral("9811000"),  // Ever Ace (container)
            QStringLiteral("9839430"),  // MSC Gulsun (container)
            QStringLiteral("9776418"),  // CMA CGM Antoine de Saint Exupery
            QStringLiteral("9728691"),  // OOCL Hong Kong
            QStringLiteral("9302923")}; // Knock Nevis class tanker
}

QString maritime_vessel_topic(const QString& imo) {
    return QStringLiteral("maritime:vessel:") + imo;
}
} // namespace

MaritimeVesselsWidget::MaritimeVesselsWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget(tr("MARITIME VESSELS"), parent, ui::colors::CYAN()) {
    set_agent_risk(tile_billing::agent_kind(QStringLiteral("maritime"))); // refresh / LOAD: billed per vessel ("maritime" credits)
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
    make_hdr(tr("VESSEL"), 3);
    make_hdr(tr("ROUTE"), 4);
    make_hdr(tr("KN"), 1, Qt::AlignRight);
    make_hdr(tr("PROG"), 1, Qt::AlignRight);
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

    status_label_ = new QLabel(tr("Loading…"));
    status_label_->setAlignment(Qt::AlignCenter);
    list_layout_->addWidget(status_label_);
    list_layout_->addStretch();

    scroll_area_->setWidget(list_widget);
    vl->addWidget(scroll_area_, 1);

    // Billing rule: every vessel position costs a credit, so nothing here loads by
    // itself (unless the plan includes maritime data — PlanAccess): the title-bar
    // refresh and the LOAD offer ask once per vessel. The subscriptions that
    // receive the answers end 4 minutes later (the positions' TTL is 10 minutes),
    // so the hub never re-buys positions on its own while the tile is open (it
    // used to, every 10 minutes per vessel).
    connect(this, &BaseWidget::refresh_requested, this, [this]() {
        if (imos_.isEmpty())
            return;
        hide_empty_state();
        if (!hub_active_)
            hub_resubscribe();
        auto& hub = datahub::DataHub::instance();
        for (const auto& imo : imos_)
            hub.request(QStringLiteral("maritime:vessel:") + imo, /*force=*/true);
        auto* window = findChild<QTimer*>(QStringLiteral("maritimeOneShot"), Qt::FindDirectChildrenOnly);
        if (!window) {
            window = new QTimer(this);
            window->setObjectName(QStringLiteral("maritimeOneShot"));
            window->setSingleShot(true);
            window->setInterval(4 * 60 * 1000);
            connect(window, &QTimer::timeout, this, [this]() {
                if (hub_active_ && !tile_billing::plan_includes(QStringLiteral("maritime")))
                    hub_unsubscribe_all();
            });
        }
        window->start(); // a later LOAD restarts the window
    });
    // The plan table arrived / changed: a plan that includes maritime data loads
    // the offered positions now, as a free tile would.
    connect(&billing::PlanAccess::instance(), &billing::PlanAccess::changed, this, [this]() {
        set_agent_risk(tile_billing::agent_kind(QStringLiteral("maritime"))); // agents: an included load is free
        if (isVisible() && is_empty_state_shown() && tile_billing::plan_includes(QStringLiteral("maritime")))
            request_refresh();
    });

    set_configurable(true);
    apply_styles();
    MaritimeVesselsWidget::apply_config(cfg); // qualified: no dynamic dispatch inside a constructor
    set_loading(true);
}

QJsonObject MaritimeVesselsWidget::config() const {
    QJsonObject o;
    QJsonArray arr;
    for (const auto& imo : imos_)
        arr.append(imo);
    o.insert("imos", arr);
    return o;
}

void MaritimeVesselsWidget::apply_config(const QJsonObject& cfg) {
    QStringList new_imos;
    if (cfg.contains("imos")) {
        const QJsonArray arr = cfg.value("imos").toArray();
        for (const auto& v : arr) {
            const QString imo = v.toString().trimmed();
            if (!imo.isEmpty())
                new_imos.append(imo);
        }
    } else {
        new_imos = default_imos();
    }

    imos_ = new_imos;
    build_rows();
    untrack_topics(); // fresh "—" rows: forget the old rows' data / error state

    if (isVisible()) {
        // Billing rule: a new list is not loaded by itself — what the hub holds for
        // these vessels, else the LOAD offer.
        if (hub_active_)
            hub_unsubscribe_all();
        show_remembered_or_offer();
    }
}

void MaritimeVesselsWidget::clear_rows() {
    while (list_layout_->count() > 0) {
        auto* item = list_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    rows_.clear();
    status_label_ = nullptr;
}

void MaritimeVesselsWidget::build_rows() {
    clear_rows();

    if (imos_.isEmpty()) {
        status_label_ = new QLabel(tr("No vessels configured — click gear to add IMOs"));
        status_label_->setAlignment(Qt::AlignCenter);
        status_label_->setStyleSheet(
            QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;padding:16px;background:transparent;").arg(ui::colors::TEXT_TERTIARY()));
        list_layout_->addWidget(status_label_);
        list_layout_->addStretch();
        return;
    }

    bool alt = false;
    for (const auto& imo : imos_) {
        auto* row = new QWidget(this);
        row->setStyleSheet(QString("background: %1;").arg(alt ? ui::colors::ROW_ALT() : "transparent"));
        link_screen(row, QStringLiteral("maritime"), tr("Double-click to open the Maritime screen"));
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(8, 4, 8, 4);

        Row r;
        r.container = row;

        r.name = new QLabel(QStringLiteral("IMO ") + imo);
        r.name->setToolTip(QStringLiteral("IMO ") + imo);
        r.name->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:bold;background:transparent;")
                                  .arg(ui::colors::TEXT_PRIMARY()));
        rl->addWidget(r.name, 3);

        r.route = new QLabel(QStringLiteral("—"));
        r.route->setStyleSheet(
            QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
        rl->addWidget(r.route, 4);

        r.speed = new QLabel(QStringLiteral("—"));
        r.speed->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        r.speed->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:bold;background:transparent;")
                                   .arg(ui::colors::TEXT_PRIMARY()));
        rl->addWidget(r.speed, 1);

        r.progress = new QLabel(QStringLiteral("—"));
        r.progress->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        r.progress->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;background:transparent;").arg(ui::colors::CYAN()));
        rl->addWidget(r.progress, 1);

        list_layout_->addWidget(row);
        rows_.insert(imo, r);
        alt = !alt;
    }

    list_layout_->addStretch();
}

void MaritimeVesselsWidget::hub_resubscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    if (imos_.isEmpty()) {
        // The "No vessels configured" hint is the content — no spinner over it.
        untrack_topics();
        set_loading(false);
        hub_active_ = true;
        return;
    }
    QStringList topics;
    topics.reserve(imos_.size());
    for (const auto& imo : imos_)
        topics.append(maritime_vessel_topic(imo));
    // BaseWidget drives the overlay and the producer-error reporting. The
    // service says why a lookup was refused (signed out, out of credits, IP
    // block, no vessel data) — the tile used to ignore it, leaving every row on
    // "—" and the overlay on the generic "No data yet". Every row failed: the
    // reason replaces the spinner; some failed: those rows show "—" with the
    // reason on hover and the title shows "n/N FAILED".
    track_topics(topics, /*drive_loading_overlay=*/true);
    for (const auto& imo : imos_) {
        const QString topic = maritime_vessel_topic(imo);
        const QString imo_copy = imo;
        hub.subscribe(this, topic, [this, imo_copy](const QVariant& v) { on_vessel(imo_copy, v); });

        // Seed from cache so existing data shows immediately.
        const QVariant cached = hub.peek(topic);
        if (cached.isValid())
            on_vessel(imo, cached);
    }
    hub_active_ = true;
}

void MaritimeVesselsWidget::on_topic_errors_changed() {
    for (const auto& imo : imos_)
        apply_row_error(imo);
}

void MaritimeVesselsWidget::apply_row_error(const QString& imo) {
    auto it = rows_.find(imo);
    if (it == rows_.end())
        return;
    const QString topic = maritime_vessel_topic(imo);
    const QString tip = topic_error_tooltip(topic);
    it->speed->setToolTip(tip);
    it->progress->setToolTip(tip);
    if (!tip.isEmpty() && !topic_has_data(topic)) {
        // Failed with nothing to show: mark the row and say why on hover.
        it->route->setText(failed_value_text());
        it->route->setToolTip(tip);
        it->speed->setText(failed_value_text());
        it->progress->setText(failed_value_text());
    }
}

void MaritimeVesselsWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void MaritimeVesselsWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        show_remembered_or_offer();
}

void MaritimeVesselsWidget::show_remembered_or_offer() {
    if (imos_.isEmpty() || tile_billing::plan_includes(QStringLiteral("maritime"))) {
        // No vessels: the "No vessels configured" hint (nothing is subscribed).
        // Included with the plan: kept fresh by the hub, as before.
        hub_resubscribe();
        return;
    }
    // Billing rule: no request on show. The positions the hub already holds (this
    // session's last load, here or in the Maritime screen), else the LOAD offer.
    auto& hub = datahub::DataHub::instance();
    bool any = false;
    for (const auto& imo : imos_) {
        const QVariant cached = hub.peek_raw(maritime_vessel_topic(imo));
        if (cached.isValid()) {
            on_vessel(imo, cached);
            any = true;
        }
    }
    set_loading(false);
    if (any) {
        hide_empty_state();
        return;
    }
    ui::EmptyState::Spec spec = ui::EmptyState::preview_spec(
        tr("Vessel positions"),
        tr("Position, route, speed and voyage progress of the vessels in this tile's list."),
        tr("LOAD POSITIONS"),
        tile_billing::cost_note(QStringLiteral("maritime"), 1.0,
                                tr("1 credit per vessel per load (%n vessel(s))", nullptr,
                                   static_cast<int>(imos_.size())),
                                tr("vessel")),
        ui::EmptyState::Tone::Warning, ui::LoadingSkeleton::Shape::List);
    show_empty_state(spec, [this]() { request_refresh(); });
}

void MaritimeVesselsWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void MaritimeVesselsWidget::on_vessel(const QString& imo, const QVariant& v) {
    using fincept::services::maritime::VesselData;
    if (!v.canConvert<VesselData>())
        return;
    const VesselData vd = v.value<VesselData>();

    auto it = rows_.find(imo);
    if (it == rows_.end())
        return;

    const QString display_name = vd.name.isEmpty() ? (QStringLiteral("IMO ") + imo) : vd.name;
    it->name->setText(display_name);
    it->name->setToolTip(QStringLiteral("IMO ") + imo +
                         (vd.last_updated.isEmpty() ? QString() : QStringLiteral("\nUpdated: ") + vd.last_updated));

    QString from = vd.from_port.isEmpty() ? QStringLiteral("?") : vd.from_port;
    QString to = vd.to_port.isEmpty() ? QStringLiteral("?") : vd.to_port;
    if (from.length() > 14)
        from = from.left(13) + QStringLiteral("…");
    if (to.length() > 14)
        to = to.left(13) + QStringLiteral("…");
    it->route->setText(from + QStringLiteral(" → ") + to);
    it->route->setToolTip(vd.from_port + QStringLiteral(" → ") + vd.to_port);

    it->speed->setText(QString::number(vd.speed, 'f', 1));

    const double pct = qBound(0.0, vd.route_progress, 100.0);
    it->progress->setText(QString::number(pct, 'f', 0) + QStringLiteral("%"));
    QColor pc = ui::colors::CYAN();
    if (pct >= 90)
        pc = ui::colors::SUCCESS(); // voyage nearly done — a state, not a price move
    else if (pct < 25)
        pc = ui::colors::WARNING();
    it->progress->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:bold;background:transparent;").arg(pc.name()));

    // Tracked topic (see hub_resubscribe): clears its error and lets BaseWidget
    // take the loading overlay down.
    const QString topic = maritime_vessel_topic(imo);
    const bool had_error = !topic_error(topic).isEmpty();
    note_topic_value(topic);
    if (had_error)
        apply_row_error(imo); // drop the failure tooltips
}

QDialog* MaritimeVesselsWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle(tr("Configure — Maritime Vessels"));
    auto* form = new QFormLayout(dlg);

    auto* edit = new QPlainTextEdit(dlg);
    edit->setPlaceholderText(tr("One IMO per line (e.g. 9811000)"));
    edit->setPlainText(imos_.join('\n'));
    edit->setFixedHeight(140);
    form->addRow(tr("IMO numbers"), edit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    form->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, dlg, [this, dlg, edit]() {
        QJsonArray arr;
        const QStringList lines = edit->toPlainText().split('\n', Qt::SkipEmptyParts);
        for (const auto& line : lines) {
            const QString imo = line.trimmed();
            if (!imo.isEmpty())
                arr.append(imo);
        }
        QJsonObject cfg;
        cfg.insert("imos", arr);
        apply_config(cfg);
        emit config_changed(cfg);
        dlg->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    return dlg;
}

void MaritimeVesselsWidget::on_theme_changed() {
    apply_styles();
}

void MaritimeVesselsWidget::apply_styles() {
    set_css_cached(header_widget_, QString("background:%1;").arg(ui::colors::BG_RAISED()));
    for (auto* lbl : header_labels_)
        set_css_cached(lbl, QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:bold;background:transparent;")
                               .arg(ui::colors::TEXT_SECONDARY()));
    set_css_cached(header_sep_, QString("background:%1;").arg(ui::colors::BORDER_DIM()));
    set_css_cached(scroll_area_, QString("QScrollArea{border:none;background:transparent;}"
                                        "QScrollBar:vertical{width:4px;background:transparent;}"
                                        "QScrollBar::handle:vertical{background:%1;border-radius:2px;min-height:20px;}"
                                        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
                                    .arg(ui::colors::BORDER_MED()));
    if (status_label_)
        set_css_cached(status_label_, 
            QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;padding:16px;background:transparent;").arg(ui::colors::TEXT_TERTIARY()));
}

void MaritimeVesselsWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("MARITIME VESSELS"));
    const QStringList headers = {tr("VESSEL"), tr("ROUTE"), tr("KN"), tr("PROG")};
    for (int i = 0; i < header_labels_.size() && i < headers.size(); ++i)
        header_labels_[i]->setText(headers[i]);
    // Only the "no vessels configured" placeholder carries translatable text.
    // Rebuilding populated rows (as this used to) blanked every vessel back to
    // "—" until its next hub publish.
    if (imos_.isEmpty())
        build_rows();
}

} // namespace fincept::screens::widgets
