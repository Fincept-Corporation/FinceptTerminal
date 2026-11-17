#include "screens/dashboard/widgets/NewsCategoryWidget.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "services/news/NewsService.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QSpinBox>
#include <QUrl>

namespace fincept::screens::widgets {

namespace {
// The keys NewsService publishes `news:category:<CATEGORY>` under: the feed
// table's categories and the headline classifier's, always UPPER CASE. DataHub
// topics are case-sensitive, so a lower-case "markets" (the old default and the
// old placeholder's spelling) subscribed to a topic nobody ever publishes.
const QStringList kNewsCategories = {QStringLiteral("MARKETS"),  QStringLiteral("GEOPOLITICS"),
                                     QStringLiteral("CRYPTO"),   QStringLiteral("EARNINGS"),
                                     QStringLiteral("ECONOMIC"), QStringLiteral("ENERGY"),
                                     QStringLiteral("TECH"),     QStringLiteral("DEFENSE"),
                                     QStringLiteral("REGULATORY")};
} // namespace

NewsCategoryWidget::NewsCategoryWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget(tr("NEWS — CATEGORY"), parent) {
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
    list_->setToolTip(tr("Click a headline to open the article"));
    vl->addWidget(list_, 1);

    // Headline → article. The URL rides on the item (Qt::UserRole); only
    // http(s) links are opened.
    connect(list_, &QListWidget::itemClicked, this, [](QListWidgetItem* item) {
        const QUrl url(item->data(Qt::UserRole).toString());
        if (url.isValid() && (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https")))
            QDesktopServices::openUrl(url);
    });

    // The title-bar refresh button was not connected here at all (the
    // watchdog's "click refresh to retry" pointed at a dead button).
    connect(this, &BaseWidget::refresh_requested, this, [this]() {
        if (is_showing_error())
            set_loading(true);
        datahub::DataHub::instance().request(QStringLiteral("news:category:") + category_, /*force=*/true);
    });

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
    // Normalised to the producer's upper-case key — this also repairs tiles
    // saved with the old lower-case default.
    category_ = cfg.value("category").toString(QStringLiteral("MARKETS")).trimmed().toUpper();
    if (category_.isEmpty())
        category_ = QStringLiteral("MARKETS");
    max_rows_ = qBound(3, cfg.value("max_rows").toInt(12), 50);
    badge_->setText(category_);
    list_->clear();
    // Show the loading overlay until the first delivery for this category —
    // the list used to sit blank (indistinguishable from "broken") while the
    // topic fetched.
    set_loading(true);

    if (isVisible())
        hub_resubscribe();
}

void NewsCategoryWidget::hub_resubscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    const QString topic = QStringLiteral("news:category:") + category_;
    hub.subscribe(this, topic, [this](const QVariant& v) { on_articles(v); });
    hub.subscribe_errors(this, topic, [this](const QString& error) {
        if (is_waiting_for_first_data())
            set_error(tr("News feed failed: %1 — click refresh to retry").arg(error));
    });
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
        const QString time = (a.sort_ts > 0) ? QDateTime::fromSecsSinceEpoch(a.sort_ts).toString("HH:mm") : a.time;
        const QString line = QString("[%1]  %2").arg(time, a.headline);
        auto* item = new QListWidgetItem(line, list_);
        item->setToolTip(a.summary.isEmpty() ? a.headline : a.summary);
        item->setData(Qt::UserRole, a.link);
    }
    // Empty state — an unknown/quiet category rendered a blank list.
    if (n == 0) {
        auto* empty = new QListWidgetItem(tr("No articles in this category"), list_);
        empty->setFlags(Qt::NoItemFlags);
        empty->setTextAlignment(Qt::AlignCenter);
    }
    set_loading(false);
}

QDialog* NewsCategoryWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle(tr("Configure — News Category"));
    auto* form = new QFormLayout(dlg);

    // A fixed list of the categories the news service actually publishes — free
    // text let the user type a key that never matches a topic.
    auto* combo = new QComboBox(dlg);
    combo->addItems(kNewsCategories);
    if (!kNewsCategories.contains(category_))
        combo->addItem(category_); // keep an unusual saved key selectable
    combo->setCurrentText(category_);
    form->addRow(tr("Category"), combo);

    auto* spin = new QSpinBox(dlg);
    spin->setRange(3, 50);
    spin->setValue(max_rows_);
    form->addRow(tr("Max rows"), spin);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    form->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, dlg, [this, dlg, combo, spin]() {
        QJsonObject cfg;
        cfg.insert("category", combo->currentText().trimmed().toUpper());
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
    set_css_cached(badge_, QString("color:%1;background:%2;border:1px solid %3;"
                                  "border-radius:3px;padding:1px 6px;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:700;")
                              .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    set_css_cached(list_, QString("QListWidget{background:transparent;color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;border:none;}"
                                 "QListWidget::item{padding:3px 4px;border-bottom:1px solid %2;}")
                             .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM()));
}

void NewsCategoryWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("NEWS — CATEGORY"));
    // Re-render from the hub — but only while subscribed. This used to call
    // hub_resubscribe() unconditionally, so a language switch made every hidden
    // tile subscribe (and stay subscribed until its next hide).
    if (hub_active_)
        hub_resubscribe();
}

} // namespace fincept::screens::widgets
