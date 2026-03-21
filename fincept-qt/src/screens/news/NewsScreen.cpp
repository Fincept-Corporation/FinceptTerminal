#include "screens/news/NewsScreen.h"
#include "screens/news/NewsFilterBar.h"
#include "screens/news/NewsSidebarPanel.h"
#include "screens/news/NewsWirePanel.h"
#include "screens/news/NewsReaderPanel.h"
#include "services/news/NewsService.h"
#include "services/news/NewsClusterService.h"
#include "ui/theme/Theme.h"
#include "core/logging/Logger.h"
#include <QVBoxLayout>
#include <QSplitter>
#include <QDateTime>
#include <QShowEvent>
#include <QHideEvent>

namespace fincept::screens {

using namespace fincept::services;

NewsScreen::NewsScreen(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Filter bar ──────────────────────────────────────────────────────────
    filter_bar_ = new NewsFilterBar;
    root->addWidget(filter_bar_);

    connect(filter_bar_, &NewsFilterBar::category_changed,   this, &NewsScreen::on_filter_changed);
    connect(filter_bar_, &NewsFilterBar::time_range_changed, this, &NewsScreen::on_time_range_changed);
    connect(filter_bar_, &NewsFilterBar::view_mode_changed,  this, &NewsScreen::on_view_mode_changed);
    connect(filter_bar_, &NewsFilterBar::search_changed,     this, &NewsScreen::on_search_changed);
    connect(filter_bar_, &NewsFilterBar::refresh_clicked,    this, &NewsScreen::on_refresh);

    // ── Three-panel splitter ────────────────────────────────────────────────
    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setStyleSheet(
        "QSplitter{background:transparent;border:none;}"
        "QSplitter::handle{background:#1a1a1a;width:1px;}");

    sidebar_    = new NewsSidebarPanel;
    wire_panel_ = new NewsWirePanel;
    reader_     = new NewsReaderPanel;

    sidebar_->setFixedWidth(260);
    reader_->setFixedWidth(320);

    splitter->addWidget(sidebar_);
    splitter->addWidget(wire_panel_);
    splitter->addWidget(reader_);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);

    root->addWidget(splitter, 1);

    // ── Sidebar clicks → wire filter ────────────────────────────────────────
    connect(sidebar_, &NewsSidebarPanel::category_clicked, this, &NewsScreen::on_filter_changed);
    connect(sidebar_, &NewsSidebarPanel::article_clicked,  this, &NewsScreen::on_article_selected);

    // ── Wire article selection → reader ─────────────────────────────────────
    connect(wire_panel_, &NewsWirePanel::article_selected, this, &NewsScreen::on_article_selected);

    // ── Initial load ────────────────────────────────────────────────────────
    on_refresh();

    // Auto-refresh via service
    NewsService::instance().start_auto_refresh();
    connect(&NewsService::instance(), &NewsService::articles_updated, this,
        [this](QVector<NewsArticle> articles) {
            all_articles_ = std::move(articles);
            apply_filters();
        });
}

// ── Slots ───────────────────────────────────────────────────────────────────

void NewsScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    services::NewsService::instance().start_auto_refresh();
    if (all_articles_.isEmpty()) on_refresh();  // fetch on first show
}

void NewsScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    services::NewsService::instance().stop_auto_refresh();
}

void NewsScreen::on_refresh() {
    loading_ = true;
    filter_bar_->set_loading(true);

    NewsService::instance().fetch_all_news(true,
        [this](bool ok, QVector<NewsArticle> articles) {
            loading_ = false;
            filter_bar_->set_loading(false);
            if (!ok) {
                LOG_ERROR("NewsScreen", "Failed to fetch news");
                return;
            }
            all_articles_ = std::move(articles);
            apply_filters();
        });
}

void NewsScreen::on_filter_changed(const QString& category) {
    active_filter_ = category;
    filter_bar_->set_active_category(category);
    apply_filters();
}

void NewsScreen::on_time_range_changed(const QString& range) {
    time_range_ = range;
    apply_filters();
}

void NewsScreen::on_view_mode_changed(const QString& mode) {
    view_mode_ = mode;
    wire_panel_->set_view_mode(mode);
    apply_filters();
}

void NewsScreen::on_search_changed(const QString& query) {
    search_query_ = query;
    apply_filters();
}

void NewsScreen::on_article_selected(const NewsArticle& article) {
    wire_panel_->set_selected_article(article.id);
    reader_->show_article(article);
}

// ── Filtering logic ─────────────────────────────────────────────────────────

void NewsScreen::apply_filters() {
    // Map time range to seconds
    int64_t window_sec = 86400;
    if (time_range_ == "1H")  window_sec = 3600;
    else if (time_range_ == "6H")  window_sec = 21600;
    else if (time_range_ == "24H") window_sec = 86400;
    else if (time_range_ == "48H") window_sec = 172800;
    else if (time_range_ == "7D")  window_sec = 604800;

    auto now = QDateTime::currentSecsSinceEpoch();
    auto cutoff = now - window_sec;

    // Category mapping
    static const QMap<QString, QStringList> cat_map = {
        {"MKT",  {"Markets", "Stock", "Equity"}},
        {"EARN", {"Earnings", "Revenue", "Profit"}},
        {"ECO",  {"Economy", "Economic", "GDP", "Inflation", "Employment"}},
        {"TECH", {"Technology", "Tech", "AI", "Software"}},
        {"NRG",  {"Energy", "Oil", "Gas", "Renewable"}},
        {"CRPT", {"Crypto", "Bitcoin", "Ethereum", "Blockchain"}},
        {"GEO",  {"Geopolitics", "War", "Conflict", "Sanctions"}},
        {"DEF",  {"Defense", "Military", "Security"}},
    };

    filtered_articles_.clear();
    for (const auto& a : all_articles_) {
        // Time filter
        if (a.sort_ts > 0 && a.sort_ts < cutoff) continue;

        // Category filter
        if (active_filter_ != "ALL") {
            auto it = cat_map.find(active_filter_);
            if (it != cat_map.end()) {
                bool matches = false;
                for (const auto& kw : it.value()) {
                    if (a.category.contains(kw, Qt::CaseInsensitive) ||
                        a.headline.contains(kw, Qt::CaseInsensitive)) {
                        matches = true; break;
                    }
                }
                if (!matches) continue;
            }
        }

        // Search filter
        if (!search_query_.isEmpty()) {
            if (!a.headline.contains(search_query_, Qt::CaseInsensitive) &&
                !a.summary.contains(search_query_, Qt::CaseInsensitive) &&
                !a.source.contains(search_query_, Qt::CaseInsensitive)) {
                continue;
            }
        }

        filtered_articles_.append(a);
    }

    // Build clusters
    clusters_ = cluster_articles(filtered_articles_);

    // Count per category for sidebar
    QMap<QString, int> category_counts;
    for (const auto& a : all_articles_) {
        category_counts[a.category]++;
    }

    // Count sentiments
    int bull = 0, bear = 0, neut = 0;
    for (const auto& a : filtered_articles_) {
        if (a.sentiment == Sentiment::BULLISH) bull++;
        else if (a.sentiment == Sentiment::BEARISH) bear++;
        else neut++;
    }

    // Update panels
    sidebar_->update_data(filtered_articles_, category_counts, bull, bear, neut,
                          NewsService::instance().feed_count(),
                          NewsService::instance().active_sources().size(),
                          clusters_.size());

    wire_panel_->set_articles(filtered_articles_, clusters_);

    filter_bar_->set_article_count(filtered_articles_.size());
    filter_bar_->set_alert_count(get_breaking_clusters(clusters_).size());
}

} // namespace fincept::screens
