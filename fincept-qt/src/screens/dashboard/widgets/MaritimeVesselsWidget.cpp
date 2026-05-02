#include "screens/dashboard/widgets/MaritimeVesselsWidget.h"

#include "datahub/DataHub.h"
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
} // namespace

MaritimeVesselsWidget::MaritimeVesselsWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget("MARITIME VESSELS", parent, ui::colors::CYAN()) {
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
    make_hdr("VESSEL", 3);
    make_hdr("ROUTE", 4);
    make_hdr("KN", 1, Qt::AlignRight);
    make_hdr("PROG", 1, Qt::AlignRight);
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

    connect(this, &BaseWidget::refresh_requested, this, [this]() {
        auto& hub = datahub::DataHub::instance();
        for (const auto& imo : imos_)
            hub.request(QStringLiteral("maritime:vessel:") + imo, /*force=*/true);
    });

    set_configurable(true);
    apply_styles();
    apply_config(cfg);
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

    if (isVisible())
        hub_resubscribe();
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
        status_label_ = new QLabel("No vessels configured — click gear to add IMOs");
        status_label_->setAlignment(Qt::AlignCenter);
        status_label_->setStyleSheet(QString("color:%1;font-size:10px;padding:16px;background:transparent;")
                                         .arg(ui::colors::TEXT_TERTIARY()));
        list_layout_->addWidget(status_label_);
        list_layout_->addStretch();
        return;
    }

    bool alt = false;
    for (const auto& imo : imos_) {
        auto* row = new QWidget(this);
        row->setStyleSheet(QString("background: %1;").arg(alt ? ui::colors::BG_RAISED() : "transparent"));
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(8, 4, 8, 4);

        Row r;
        r.container = row;

        r.name = new QLabel(QStringLiteral("IMO ") + imo);
        r.name->setToolTip(QStringLiteral("IMO ") + imo);
        r.name->setStyleSheet(QString("color:%1;font-size:10px;font-weight:bold;background:transparent;")
                                  .arg(ui::colors::TEXT_PRIMARY()));
        rl->addWidget(r.name, 3);

        r.route = new QLabel(QStringLiteral("—"));
        r.route->setStyleSheet(QString("color:%1;font-size:9px;background:transparent;")
                                   .arg(ui::colors::TEXT_SECONDARY()));
        rl->addWidget(r.route, 4);

        r.speed = new QLabel(QStringLiteral("—"));
        r.speed->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        r.speed->setStyleSheet(QString("color:%1;font-size:10px;font-weight:bold;background:transparent;")
                                   .arg(ui::colors::TEXT_PRIMARY()));
        rl->addWidget(r.speed, 1);

        r.progress = new QLabel(QStringLiteral("—"));
        r.progress->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        r.progress->setStyleSheet(QString("color:%1;font-size:9px;background:transparent;")
                                      .arg(ui::colors::CYAN()));
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
    for (const auto& imo : imos_) {
        const QString topic = QStringLiteral("maritime:vessel:") + imo;
        const QString imo_copy = imo;
        hub.subscribe(this, topic, [this, imo_copy](const QVariant& v) { on_vessel(imo_copy, v); });

        // Seed from cache so existing data shows immediately.
        const QVariant cached = hub.peek(topic);
        if (cached.isValid())
            on_vessel(imo, cached);
    }
    hub_active_ = true;
}

void MaritimeVesselsWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void MaritimeVesselsWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        hub_resubscribe();
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
        pc = ui::colors::POSITIVE();
    else if (pct < 25)
        pc = ui::colors::WARNING();
    it->progress->setStyleSheet(QString("color:%1;font-size:9px;font-weight:bold;background:transparent;")
                                    .arg(pc.name()));

    set_loading(false);
}

QDialog* MaritimeVesselsWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle("Configure — Maritime Vessels");
    auto* form = new QFormLayout(dlg);

    auto* edit = new QPlainTextEdit(dlg);
    edit->setPlaceholderText("One IMO per line (e.g. 9811000)");
    edit->setPlainText(imos_.join('\n'));
    edit->setFixedHeight(140);
    form->addRow("IMO numbers", edit);

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
