#include "screens/dashboard/widgets/PolymarketPriceWidget.h"

#include "datahub/DataHub.h"
#include "services/polymarket/PolymarketWebSocket.h"
#include "ui/theme/Theme.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPlainTextEdit>
#include <QStyle>
#include <QVBoxLayout>

namespace fincept::screens::widgets {

namespace {
// The probability label's colour band ("hi" >= 65 %, "lo" <= 35 %, else "mid")
// is a property matched in the rows host's stylesheet. Every WS price message
// used to call setStyleSheet() (full CSS re-parse); now the label is only
// re-polished when the band actually changes (CLAUDE.md P7).
void pm_price_set_band(QLabel* label, const char* band) {
    if (label->property("lvl").toString() == QLatin1String(band))
        return;
    label->setProperty("lvl", QString::fromLatin1(band));
    label->style()->unpolish(label);
    label->style()->polish(label);
}
} // namespace

PolymarketPriceWidget::PolymarketPriceWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget(tr("POLYMARKET"), parent) {
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

    // One host carries the stylesheet for the hint / every row label
    // (objectName + "lvl" property selectors) — see apply_styles().
    rows_host_ = new QWidget(this);
    rows_host_->setObjectName("pmPriceRows");

    if (entries_.isEmpty()) {
        auto* host_layout = new QVBoxLayout(rows_host_);
        host_layout->setContentsMargins(0, 0, 0, 0);
        auto* hint = new QLabel(tr("No markets configured — click gear to add"), rows_host_);
        hint->setObjectName("pmPriceHint");
        host_layout->addWidget(hint);
        vl->addWidget(rows_host_);
        vl->addStretch(1);
        apply_styles();
        return;
    }

    auto* grid = new QGridLayout(rows_host_);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(3);
    for (int i = 0; i < entries_.size(); ++i) {
        const auto& e = entries_[i];
        Row r;
        r.label = new QLabel(e.label, rows_host_);
        r.label->setObjectName("pmPriceLabel");
        r.label->setToolTip(e.asset_id);
        r.pct = new QLabel("—", rows_host_);
        r.pct->setObjectName("pmPricePct");
        r.pct->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(r.label, i, 0);
        grid->addWidget(r.pct, i, 1);
        // The label's tooltip used to be the asset id; keep it in the new one.
        link_screen(r.label, QStringLiteral("polymarket"),
                    e.asset_id + QStringLiteral(" | ") + tr("Double-click to open the Polymarket screen"));
        link_screen(r.pct, QStringLiteral("polymarket"), tr("Double-click to open the Polymarket screen"));
        rows_.insert(e.asset_id, r);
    }
    vl->addWidget(rows_host_);
    vl->addStretch(1);
    apply_styles();
}

void PolymarketPriceWidget::hub_resubscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);

    // `prediction:polymarket:price:*` is push-only: the hub never fetches it,
    // PolymarketWebSocket publishes only for tokens it has been asked to stream.
    // Nothing on the dashboard asked, so the tile sat at "—" forever unless the
    // Polymarket screen happened to be streaming the same tokens. Ask for them
    // here. This is additive and idempotent (the socket keeps a set); we never
    // unsubscribe because the token set is shared with the Polymarket screen.
    QStringList token_ids;
    token_ids.reserve(entries_.size());
    for (const auto& e : entries_)
        token_ids.append(e.asset_id);
    if (!token_ids.isEmpty())
        services::polymarket::PolymarketWebSocket::instance().subscribe(token_ids);

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
    pm_price_set_band(it->pct, pct >= 65 ? "hi" : (pct <= 35 ? "lo" : "mid"));
    set_loading(false);
}

QDialog* PolymarketPriceWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle(tr("Configure — Polymarket"));
    auto* form = new QFormLayout(dlg);

    auto* edit = new QPlainTextEdit(dlg);
    edit->setPlaceholderText(tr("One per line:  <asset_id> | <label>"));
    QStringList lines;
    for (const auto& e : entries_)
        lines.append(e.asset_id + " | " + e.label);
    edit->setPlainText(lines.join("\n"));
    edit->setFixedHeight(120);
    form->addRow(tr("Markets"), edit);

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
    if (!rows_host_)
        return; // ctor runs this before the first build_rows()
    // A probability label with no "lvl" yet (still "—") keeps the tertiary colour.
    rows_host_->setStyleSheet(
        QString("QLabel#pmPriceHint{color:%2;font-size:10px;background:transparent;padding:4px 0;}"
                "QLabel#pmPriceLabel{color:%1;font-size:11px;font-weight:600;background:transparent;}"
                "QLabel#pmPricePct{color:%2;font-size:11px;font-weight:700;background:transparent;}"
                "QLabel#pmPricePct[lvl=\"hi\"]{color:%3;}"
                "QLabel#pmPricePct[lvl=\"lo\"]{color:%4;}"
                "QLabel#pmPricePct[lvl=\"mid\"]{color:%1;}")
            .arg(ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_TERTIARY(), ui::colors::POSITIVE(),
                 ui::colors::NEGATIVE()));
}

void PolymarketPriceWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("POLYMARKET"));
    // Only the "no markets configured" hint is translatable; rebuilding
    // populated rows blanked every probability until the next push.
    if (entries_.isEmpty())
        build_rows();
}

} // namespace fincept::screens::widgets
