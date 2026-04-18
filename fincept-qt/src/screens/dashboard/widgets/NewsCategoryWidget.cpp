#include "screens/dashboard/widgets/NewsCategoryWidget.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "services/news/NewsService.h"
#include "ui/theme/Theme.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDateTime>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QSpinBox>

namespace fincept::screens::widgets {

NewsCategoryWidget::NewsCategoryWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget("NEWS — CATEGORY", parent) {
    auto* vl = content_layout();
    vl->setContentsMargins(8, 6, 8, 6);
    vl->setSpacing(4);

    auto* header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    badge_ = new QLabel();
    header->addWidget(badge_);
    header->addStretch(1);
    vl->addLayout(header);

    list_ = new QListWidget(this);
    list_->setUniformItemSizes(false);
    list_->setWordWrap(true);
    list_->setSelectionMode(QAbstractItemView::NoSelection);
    list_->setFocusPolicy(Qt::NoFocus);
    vl->addWidget(list_, 1);

    set_configurable(true);
    apply_styles();
    apply_config(cfg);
}

QJsonObject NewsCategoryWidget::config() const {
    QJsonObject o;
    o.insert("category", category_);
    o.insert("max_rows", max_rows_);
    return o;
}

void NewsCategoryWidget::apply_config(const QJsonObject& cfg) {
    category_ = cfg.value("category").toString("markets").trimmed();
    if (category_.isEmpty())
        category_ = "markets";
    max_rows_ = qBound(3, cfg.value("max_rows").toInt(12), 50);
    badge_->setText(category_.toUpper());
    list_->clear();

    if (isVisible())
        hub_resubscribe();
}

void NewsCategoryWidget::hub_resubscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    const QString topic = QStringLiteral("news:category:") + category_;
    hub.subscribe(this, topic, [this](const QVariant& v) { on_articles(v); });
    const QVariant snap = hub.peek(topic);
    if (snap.isValid())
        on_articles(snap);
    hub_active_ = true;
}

void NewsCategoryWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void NewsCategoryWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        hub_resubscribe();
}

void NewsCategoryWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void NewsCategoryWidget::on_articles(const QVariant& v) {
    if (!v.canConvert<QVector<fincept::services::NewsArticle>>())
        return;
    const auto articles = v.value<QVector<fincept::services::NewsArticle>>();

    list_->clear();
    int n = std::min<int>(max_rows_, articles.size());
    for (int i = 0; i < n; ++i) {
        const auto& a = articles[i];
        const QString time =
            (a.sort_ts > 0)
                ? QDateTime::fromSecsSinceEpoch(a.sort_ts).toString("HH:mm")
                : a.time;
        const QString line = QString("[%1]  %2").arg(time, a.headline);
        auto* item = new QListWidgetItem(line, list_);
        item->setToolTip(a.summary.isEmpty() ? a.headline : a.summary);
    }
    set_loading(false);
}

QDialog* NewsCategoryWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle("Configure — News Category");
    auto* form = new QFormLayout(dlg);

    auto* edit = new QLineEdit(dlg);
    edit->setText(category_);
    edit->setPlaceholderText("markets | geopolitics | crypto | …");
    form->addRow("Category", edit);

    auto* spin = new QSpinBox(dlg);
    spin->setRange(3, 50);
    spin->setValue(max_rows_);
    form->addRow("Max rows", spin);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    form->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, dlg, [this, dlg, edit, spin]() {
        QJsonObject cfg;
        cfg.insert("category", edit->text().trimmed());
        cfg.insert("max_rows", spin->value());
        apply_config(cfg);
        emit config_changed(cfg);
        dlg->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    return dlg;
}

void NewsCategoryWidget::on_theme_changed() {
    apply_styles();
}

void NewsCategoryWidget::apply_styles() {
    badge_->setStyleSheet(QString("color:%1;background:%2;border:1px solid %3;"
                                  "border-radius:3px;padding:1px 6px;font-size:9px;font-weight:700;")
                              .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_RAISED(),
                                   ui::colors::BORDER_DIM()));
    list_->setStyleSheet(
        QString("QListWidget{background:transparent;color:%1;font-size:10px;border:none;}"
                "QListWidget::item{padding:3px 4px;border-bottom:1px solid %2;}")
            .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM()));
}

} // namespace fincept::screens::widgets
