#include "screens/dashboard/widgets/PolymarketPriceWidget.h"

#include "datahub/DataHub.h"
#include "ui/theme/Theme.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPlainTextEdit>

namespace fincept::screens::widgets {

PolymarketPriceWidget::PolymarketPriceWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget("POLYMARKET", parent) {
    auto* vl = content_layout();
    vl->setContentsMargins(10, 8, 10, 8);
    vl->setSpacing(4);

    set_configurable(true);
    apply_styles();
    apply_config(cfg);
}

QJsonObject PolymarketPriceWidget::config() const {
    QJsonObject o;
    QJsonArray arr;
    for (const auto& e : entries_) {
        QJsonObject item;
        item.insert("asset_id", e.asset_id);
        item.insert("label", e.label);
        arr.append(item);
    }
    o.insert("entries", arr);
    return o;
}

void PolymarketPriceWidget::apply_config(const QJsonObject& cfg) {
    entries_.clear();
    const QJsonArray arr = cfg.value("entries").toArray();
    for (const auto& v : arr) {
        const QJsonObject obj = v.toObject();
        Entry e;
        e.asset_id = obj.value("asset_id").toString().trimmed();
        e.label = obj.value("label").toString().trimmed();
        if (e.label.isEmpty())
            e.label = e.asset_id.left(12);
        if (!e.asset_id.isEmpty())
            entries_.append(e);
    }

    build_rows();
    if (isVisible())
        hub_resubscribe();
}

void PolymarketPriceWidget::build_rows() {
    auto* vl = content_layout();
    while (QLayoutItem* it = vl->takeAt(0)) {
        if (auto* w = it->widget())
            w->deleteLater();
        else if (auto* sub = it->layout()) {
            while (QLayoutItem* ci = sub->takeAt(0)) {
                if (auto* cw = ci->widget())
                    cw->deleteLater();
                delete ci;
            }
            delete sub;
        }
        delete it;
    }
    rows_.clear();

    if (entries_.isEmpty()) {
        auto* hint = new QLabel("No markets configured — click gear to add");
        hint->setStyleSheet(
            QString("color:%1;font-size:10px;background:transparent;padding:4px 0;")
                .arg(ui::colors::TEXT_TERTIARY()));
        vl->addWidget(hint);
        vl->addStretch(1);
        return;
    }

    auto* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(3);
    for (int i = 0; i < entries_.size(); ++i) {
        const auto& e = entries_[i];
        Row r;
        r.label = new QLabel(e.label);
        r.label->setToolTip(e.asset_id);
        r.pct = new QLabel("—");
        r.pct->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(r.label, i, 0);
        grid->addWidget(r.pct, i, 1);
        rows_.insert(e.asset_id, r);
    }
    vl->addLayout(grid);
    vl->addStretch(1);
    apply_styles();
}

void PolymarketPriceWidget::hub_resubscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    for (const auto& e : entries_) {
        const QString topic = QStringLiteral("prediction:polymarket:price:") + e.asset_id;
        const QString asset_copy = e.asset_id;
        hub.subscribe(this, topic, [this, asset_copy](const QVariant& v) {
            bool ok = false;
            const double p = v.toDouble(&ok);
            if (ok)
                on_price(asset_copy, p);
        });
    }
    hub_active_ = true;
}

void PolymarketPriceWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void PolymarketPriceWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        hub_resubscribe();
}

void PolymarketPriceWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void PolymarketPriceWidget::on_price(const QString& asset_id, double price) {
    auto it = rows_.find(asset_id);
    if (it == rows_.end())
        return;
    const double pct = qBound(0.0, price, 1.0) * 100.0;
    it->pct->setText(QString::number(pct, 'f', 1) + "%");
    QColor col = ui::colors::TEXT_PRIMARY();
    if (pct >= 65)
        col = ui::colors::POSITIVE();
    else if (pct <= 35)
        col = ui::colors::NEGATIVE();
    it->pct->setStyleSheet(
        QString("color:%1;font-size:11px;font-weight:700;background:transparent;").arg(col.name()));
    set_loading(false);
}

QDialog* PolymarketPriceWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle("Configure — Polymarket");
    auto* form = new QFormLayout(dlg);

    auto* edit = new QPlainTextEdit(dlg);
    edit->setPlaceholderText("One per line:  <asset_id> | <label>");
    QStringList lines;
    for (const auto& e : entries_)
        lines.append(e.asset_id + " | " + e.label);
    edit->setPlainText(lines.join("\n"));
    edit->setFixedHeight(120);
    form->addRow("Markets", edit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    form->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, dlg, [this, dlg, edit]() {
        QJsonArray arr;
        const QStringList rows = edit->toPlainText().split('\n', Qt::SkipEmptyParts);
        for (const auto& line : rows) {
            const QStringList parts = line.split('|');
            if (parts.isEmpty())
                continue;
            QJsonObject item;
            item.insert("asset_id", parts.value(0).trimmed());
            item.insert("label", parts.value(1).trimmed());
            arr.append(item);
        }
        QJsonObject cfg;
        cfg.insert("entries", arr);
        apply_config(cfg);
        emit config_changed(cfg);
        dlg->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    return dlg;
}

void PolymarketPriceWidget::on_theme_changed() {
    apply_styles();
}

void PolymarketPriceWidget::apply_styles() {
    const QString label_css =
        QString("color:%1;font-size:11px;font-weight:600;background:transparent;")
            .arg(ui::colors::TEXT_PRIMARY());
    const QString pct_css =
        QString("color:%1;font-size:11px;font-weight:700;background:transparent;")
            .arg(ui::colors::TEXT_TERTIARY());
    for (auto it = rows_.begin(); it != rows_.end(); ++it) {
        if (it->label)
            it->label->setStyleSheet(label_css);
        if (it->pct && it->pct->text() == "—")
            it->pct->setStyleSheet(pct_css);
    }
}

} // namespace fincept::screens::widgets
