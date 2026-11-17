#include "screens/dashboard/widgets/NewsWidget.h"

#include "core/market/MarketProfile.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "services/portfolio/PortfolioService.h"
#include "ui/formatting/DateFormat.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QPointer>
#include <QDateTime>
#include <QDesktopServices>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QUrl>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>

namespace fincept::screens::widgets {

namespace {
constexpr const char* kNewsWidgetTopic = "news:general";
constexpr int kMaxArticles = 40; // headline cap; NewsService publishes the full feed
// Dynamic property carrying a row's article URL (see eventFilter()).
constexpr const char* kNewsRowLinkProp = "fincept_news_link";

int nw_impact_rank(services::Impact i) {
    return i == services::Impact::HIGH ? 2 : i == services::Impact::MEDIUM ? 1 : 0;
}
} // namespace

NewsWidget::NewsWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget(tr("MARKET NEWS"), parent, ui::colors::CYAN) {
    auto* bar = new QWidget(this);
    bar->setObjectName("nwBar");
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(4, 3, 4, 3);
    hl->setSpacing(4);
    region_ = new QComboBox(bar);
    region_->setAccessibleName(tr("News region"));
    region_->setToolTip(tr("Region of the publishing feed"));
    hl->addWidget(region_, 1);
    impact_ = new QComboBox(bar);
    impact_->setAccessibleName(tr("Minimum impact"));
    hl->addWidget(impact_, 1);
    mine_ = new QPushButton(tr("MY SYMBOLS"), bar);
    mine_->setObjectName("nwMine");
    mine_->setCheckable(true);
    mine_->setToolTip(tr("Only articles that name a symbol held in one of your portfolios"));
    hl->addWidget(mine_);
    content_layout()->addWidget(bar);

    scroll_area_ = new QScrollArea;
    scroll_area_->setWidgetResizable(true);
    auto* container = new QWidget(this);
    container->setObjectName("nwList"); // the row / label rules live on this (apply_styles)
    news_layout_ = new QVBoxLayout(container);
    news_layout_->setContentsMargins(4, 2, 4, 4);
    news_layout_->setSpacing(0);
    news_layout_->addStretch();
    scroll_area_->setWidget(container);
    content_layout()->addWidget(scroll_area_, 1);

    auto refilter = [this]() {
        if (filling_)
            return;
        region_filter_ = region_->currentData().toString();
        impact_filter_ = impact_->currentData().toInt();
        mine_only_ = mine_->isChecked();
        populate(last_articles_);
        emit config_changed(config());
    };
    connect(region_, qOverload<int>(&QComboBox::currentIndexChanged), this, refilter);
    connect(impact_, qOverload<int>(&QComboBox::currentIndexChanged), this, refilter);
    connect(mine_, &QPushButton::toggled, this, refilter);
    connect(&core::market::MarketProfileStore::instance(), &core::market::MarketProfileStore::profile_changed, this,
            [this]() { fill_regions(); });

    // Refresh button on the title bar — force-refresh through the hub.
    // Per-producer rate limit still applies, so this can't hammer upstream.
    connect(this, &BaseWidget::refresh_requested, this, [this]() {
        if (is_showing_error())
            set_loading(true); // retry: back to the loading state
        datahub::DataHub::instance().request(QString::fromLatin1(kNewsWidgetTopic), /*force=*/true);
    });

    apply_styles();
    set_loading(true);
    apply_config(cfg);
}

QJsonObject NewsWidget::config() const {
    return QJsonObject{{"region", region_filter_}, {"impact", impact_filter_}, {"mine", mine_only_}};
}

void NewsWidget::apply_config(const QJsonObject& cfg) {
    region_filter_ = cfg.value("region").toString();
    impact_filter_ = std::clamp(cfg.value("impact").toInt(0), 0, 2);
    mine_only_ = cfg.value("mine").toBool(false);
    fill_regions();
    populate(last_articles_);
}

void NewsWidget::fill_regions() {
    filling_ = true;
    region_->clear();
    region_->addItem(tr("All regions"), QString());
    const auto& p = core::market::MarketProfileStore::instance().current();
    QStringList seen;
    if (!p.news_region.isEmpty()) {
        region_->addItem(tr("%1 feeds").arg(core::market::market_profile_name(p)), p.news_region);
        seen << p.news_region;
    }
    // Every other region the feed actually carries.
    QStringList regions;
    for (const auto& a : last_articles_)
        if (!a.region.isEmpty() && !regions.contains(a.region))
            regions << a.region;
    for (const QString& r : {QStringLiteral("GLOBAL"), QStringLiteral("US"), QStringLiteral("EU"), QStringLiteral("UK"),
                             QStringLiteral("ASIA"), QStringLiteral("INDIA")})
        if (!regions.contains(r))
            regions << r;
    for (const auto& r : regions)
        if (!seen.contains(r))
            region_->addItem(r == QLatin1String("GLOBAL") ? tr("Global feeds") : r, r);
    const int idx = region_->findData(region_filter_);
    region_->setCurrentIndex(idx >= 0 ? idx : 0);

    impact_->clear();
    impact_->addItem(tr("Any impact"), 0);
    impact_->addItem(tr("Medium + high impact"), 1);
    impact_->addItem(tr("High impact only"), 2);
    impact_->setCurrentIndex(impact_filter_);
    mine_->setChecked(mine_only_);
    filling_ = false;
}

void NewsWidget::apply_styles() {
    // One sheet for every headline row. populate() used to give each of the up to 40
    // rows and its four labels an inline stylesheet (~200 CSS parses + polishes per
    // feed update): a 300-500 ms GUI stall every time the news refreshed.
    if (auto* list = findChild<QWidget*>(QStringLiteral("nwList")))
        set_css_cached(list, 
            QString("QWidget#nwRow{border-bottom:1px solid %1;}"
                    "QLabel#nwSent{color:%2;font-size:%3px;background:transparent;}"
                    "QLabel#nwSent[tone=\"up\"]{color:%4;}"
                    "QLabel#nwSent[tone=\"down\"]{color:%5;}"
                    "QLabel#nwTime{color:%6;font-size:%3px;background:transparent;}"
                    "QLabel#nwHead{color:%7;font-size:%8px;background:transparent;}"
                    "QLabel#nwHead[high=\"true\"]{font-weight:bold;}"
                    "QLabel#nwMeta{color:%2;font-size:%3px;background:transparent;}"
                    "QLabel#nwEmpty{color:%2;font-size:%8px;padding:12px;background:transparent;}")
                .arg(ui::colors::BORDER_DIM(), ui::colors::TEXT_TERTIARY())
                .arg(ui::fonts::px(-5))
                .arg(ui::colors::POSITIVE(), ui::colors::NEGATIVE(), ui::colors::CYAN(), ui::colors::TEXT_PRIMARY())
                .arg(ui::fonts::px(-3)));
    set_css_cached(scroll_area_, QString("QScrollArea { border: none; background: transparent; }"
                                        "QScrollBar:vertical { width: 6px; background: transparent; }"
                                        "QScrollBar::handle:vertical { background: %1; border-radius: 3px; }"
                                        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
                                    .arg(ui::colors::BORDER_MED()));
    if (auto* bar = findChild<QWidget*>(QStringLiteral("nwBar")))
        set_css_cached(bar, QString("QWidget#nwBar{background:transparent;}"
                                   "QPushButton#nwMine{background:%1;color:%2;border:1px solid %3;padding:1px 8px;"
                                   "font-size:%4px;font-weight:bold;}"
                                   "QPushButton#nwMine:checked{background:%5;color:%6;border-color:%6;}")
                               .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM())
                               .arg(ui::fonts::px(-4))
                               .arg(ui::colors::ACCENT_BG(), ui::colors::AMBER()));
}

void NewsWidget::on_theme_changed() {
    apply_styles();
    // The rows carry colours in their rich text, so they are rebuilt — but not for a
    // tile nobody is looking at: rebuilding every row was a third of the time all
    // theme_changed handlers took while the dashboard was in the background.
    if (!isVisible()) {
        rows_stale_ = true;
        return;
    }
    populate(last_articles_);
}

void NewsWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (rows_stale_) {
        rows_stale_ = false;
        populate(last_articles_);
    }
    if (!hub_active_)
        hub_subscribe();
    // Held symbols for "MY SYMBOLS": a repository read, kept off the GUI thread.
    QPointer<NewsWidget> self(this);
    (void)QtConcurrent::run([self]() {
        const QStringList held = services::PortfolioService::held_symbols();
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, held]() {
                if (!self)
                    return;
                self->held_ = QSet<QString>(held.cbegin(), held.cend());
                if (self->mine_only_)
                    self->populate(self->last_articles_);
            },
            Qt::QueuedConnection);
    });
}

void NewsWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe();
}

void NewsWidget::hub_subscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.subscribe(this, QString::fromLatin1(kNewsWidgetTopic), [this](const QVariant& v) {
        if (!v.canConvert<QVector<services::NewsArticle>>())
            return;
        set_loading(false);
        populate(v.value<QVector<services::NewsArticle>>());
    });
    hub.subscribe_errors(this, QString::fromLatin1(kNewsWidgetTopic), [this](const QString& error) {
        if (is_waiting_for_first_data())
            set_error(tr("News feed failed: %1 — click refresh to retry").arg(error));
    });
    hub_active_ = true;
    // Cold-start cache fallback: render a stale-but-present cached feed at once
    // so the panel never sits blank (subscribe() only delivers fresh values).
    QVariant cached = hub.peek_raw(QString::fromLatin1(kNewsWidgetTopic));
    if (cached.canConvert<QVector<services::NewsArticle>>()) {
        set_loading(false);
        populate(cached.value<QVector<services::NewsArticle>>());
    }
}

void NewsWidget::hub_unsubscribe() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void NewsWidget::populate(const QVector<services::NewsArticle>& articles) {
    const bool regions_changed = [&]() {
        for (const auto& a : articles)
            if (!a.region.isEmpty() && region_->findData(a.region) < 0)
                return true;
        return false;
    }();
    last_articles_ = articles;
    if (regions_changed)
        fill_regions();

    while (news_layout_->count() > 1) {
        auto* item = news_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    QVector<const services::NewsArticle*> shown;
    for (const auto& a : articles) {
        if (a.headline.isEmpty())
            continue;
        if (!region_filter_.isEmpty() && a.region != region_filter_)
            continue;
        if (nw_impact_rank(a.impact) < impact_filter_)
            continue;
        if (mine_only_) {
            bool hit = false;
            for (const auto& t : a.tickers)
                hit = hit || held_.contains(t.toUpper());
            if (!hit)
                continue;
        }
        shown.append(&a);
    }
    std::stable_sort(shown.begin(), shown.end(), [](auto* x, auto* y) { return x->sort_ts > y->sort_ts; });

    // Rows are created inside the list container (styled once, by objectName, from
    // apply_styles()) — no per-widget stylesheet, no reparenting.
    QWidget* list = news_layout_->parentWidget();
    if (shown.isEmpty()) {
        auto* empty = new QLabel(articles.isEmpty() ? tr("No news available.")
                                                    : tr("No headlines match the filters."), list);
        empty->setObjectName("nwEmpty");
        empty->setAlignment(Qt::AlignCenter);
        news_layout_->insertWidget(0, empty);
        return;
    }

    const int time_w = std::max(40, QFontMetrics(ui::fonts::font(-5)).horizontalAdvance(QStringLiteral("00m ago")) + 4);
    int rendered = 0;
    for (const auto* ap : shown) {
        if (rendered >= kMaxArticles)
            break;
        const auto& article = *ap;
        auto* row = new QWidget(list);
        row->setObjectName("nwRow");
        row->setAttribute(Qt::WA_StyledBackground, true); // the border-bottom rule
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(2, 4, 2, 4);
        rl->setSpacing(6);

        // Sentiment marker: bullish / bearish are direction calls.
        const bool bull = article.sentiment == services::Sentiment::BULLISH;
        const bool bear = article.sentiment == services::Sentiment::BEARISH;
        auto* sent = new QLabel(bull ? QString(QChar(0x25B2)) : bear ? QString(QChar(0x25BC)) : QString(QChar(0x2022)),
                                row);
        sent->setObjectName("nwSent");
        sent->setProperty("tone", bull ? "up" : bear ? "down" : "none");
        sent->setToolTip(bull ? tr("Bullish tone") : bear ? tr("Bearish tone") : tr("Neutral tone"));
        rl->addWidget(sent, 0, Qt::AlignTop);

        auto* time_lbl = new QLabel(article.sort_ts > 0 ? ui::formatting::relative(qint64(article.sort_ts))
                                                        : article.time.left(5),
                                    row);
        time_lbl->setObjectName("nwTime");
        time_lbl->setFixedWidth(time_w);
        if (article.sort_ts > 0)
            time_lbl->setToolTip(ui::formatting::date_time(qint64(article.sort_ts)));
        rl->addWidget(time_lbl, 0, Qt::AlignTop);

        auto* text_col = new QWidget(row);
        auto* tl = new QVBoxLayout(text_col);
        tl->setContentsMargins(0, 0, 0, 0);
        tl->setSpacing(1);
        auto* headline = new QLabel(article.headline, text_col);
        headline->setObjectName("nwHead");
        headline->setProperty("high", article.impact == services::Impact::HIGH);
        headline->setWordWrap(true);
        tl->addWidget(headline);
        QStringList meta;
        if (article.impact == services::Impact::HIGH)
            meta << QStringLiteral("<span style='color:%1;font-weight:bold'>%2</span>")
                        .arg(ui::colors::WARNING(), tr("HIGH IMPACT"));
        else if (article.impact == services::Impact::MEDIUM)
            meta << QStringLiteral("<span style='color:%1'>%2</span>").arg(ui::colors::TEXT_SECONDARY(), tr("MED"));
        if (!article.tickers.isEmpty())
            meta << QStringLiteral("<span style='color:%1'>%2</span>")
                        .arg(ui::colors::AMBER(), article.tickers.mid(0, 4).join(QLatin1Char(' ')).toHtmlEscaped());
        if (!article.source.isEmpty())
            meta << article.source.toHtmlEscaped();
        if (!article.region.isEmpty())
            meta << article.region.toHtmlEscaped();
        auto* meta_lbl = new QLabel(meta.join(QStringLiteral(" · ")), text_col);
        meta_lbl->setObjectName("nwMeta");
        meta_lbl->setTextFormat(Qt::RichText);
        tl->addWidget(meta_lbl);
        rl->addWidget(text_col, 1);

        // Click → open the article. Child labels are made transparent to the
        // mouse so the click lands on the row, where eventFilter() sees it.
        if (!article.link.isEmpty()) {
            for (auto* lbl : row->findChildren<QLabel*>())
                lbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            text_col->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            row->setCursor(Qt::PointingHandCursor);
            row->setToolTip(article.summary.isEmpty() ? article.headline : article.summary);
            row->setProperty(kNewsRowLinkProp, article.link);
            row->installEventFilter(this);
        }
        news_layout_->insertWidget(news_layout_->count() - 1, row);
        ++rendered;
    }
}

bool NewsWidget::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonRelease && static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
        const QUrl url(obj->property(kNewsRowLinkProp).toString());
        // Only web links: the feed is third-party data, so never hand an
        // arbitrary scheme (file:, ms-*:, …) to the OS.
        if (url.isValid() && (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https"))) {
            QDesktopServices::openUrl(url);
            return true;
        }
    }
    return BaseWidget::eventFilter(obj, event);
}

void NewsWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("MARKET NEWS"));
    mine_->setText(tr("MY SYMBOLS"));
    fill_regions();
    populate(last_articles_);
}

} // namespace fincept::screens::widgets
