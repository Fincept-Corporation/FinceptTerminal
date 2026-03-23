#include "screens/news/NewsSidePanel.h"

#include "ui/theme/Theme.h"

#include <QScrollArea>

namespace fincept::screens {

NewsSidePanel::NewsSidePanel(QWidget* parent) : QWidget(parent) {
    setObjectName("newsSidePanel");
    setFixedWidth(240);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* scroll = new QScrollArea(this);
    scroll->setObjectName("newsSidePanelScroll");
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget(scroll);
    content->setObjectName("newsSidePanelContent");
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(10);

    build_stats_section(layout);
    build_sentiment_section(layout);
    build_top_stories_section(layout);
    build_categories_section(layout);
    build_monitors_section(layout);
    build_deviations_section(layout);

    // ── New sections for 20-feature integration ──
    auto build_hidden_section = [&](const QString& title, QVBoxLayout*& out_layout) -> QWidget* {
        auto* section = new QWidget(this);
        section->hide();
        auto* inner = new QVBoxLayout(section);
        inner->setContentsMargins(0, 0, 0, 0);
        inner->setSpacing(2);
        auto* lbl = new QLabel(title, section);
        lbl->setObjectName("newsSidePanelTitle");
        inner->addWidget(lbl);
        auto* container = new QWidget(section);
        out_layout = new QVBoxLayout(container);
        out_layout->setContentsMargins(0, 0, 0, 0);
        out_layout->setSpacing(1);
        inner->addWidget(container);
        layout->addWidget(section);
        return section;
    };

    entities_section_ = build_hidden_section("ENTITIES", entities_layout_);
    locations_section_ = build_hidden_section("LOCATIONS", locations_layout_);
    signals_section_ = build_hidden_section("SIGNALS", signals_layout_);
    cii_section_ = build_hidden_section("INSTABILITY", cii_layout_);
    predictions_section_ = build_hidden_section("PREDICTIONS", predictions_layout_);

    layout->addStretch();
    scroll->setWidget(content);
    outer->addWidget(scroll);
}

void NewsSidePanel::build_stats_section(QVBoxLayout* parent) {
    auto* title = new QLabel("FEED STATUS", this);
    title->setObjectName("newsSidePanelTitle");
    parent->addWidget(title);

    auto* grid = new QWidget(this);
    auto* gl = new QHBoxLayout(grid);
    gl->setContentsMargins(0, 0, 0, 0);
    gl->setSpacing(4);

    auto make_stat = [&](const QString& label_text) -> QLabel* {
        auto* box = new QWidget(grid);
        box->setObjectName("newsSidePanelStatBox");
        auto* vl = new QVBoxLayout(box);
        vl->setContentsMargins(4, 4, 4, 4);
        vl->setSpacing(1);
        auto* val = new QLabel("0", box);
        val->setObjectName("newsSidePanelStatValue");
        val->setAlignment(Qt::AlignCenter);
        auto* lbl = new QLabel(label_text, box);
        lbl->setObjectName("newsSidePanelStatLabel");
        lbl->setAlignment(Qt::AlignCenter);
        vl->addWidget(val);
        vl->addWidget(lbl);
        gl->addWidget(box);
        return val;
    };

    feeds_value_ = make_stat("FEEDS");
    articles_value_ = make_stat("ARTS");
    clusters_value_ = make_stat("CLUST");
    sources_value_ = make_stat("SRCS");

    parent->addWidget(grid);
}

void NewsSidePanel::build_sentiment_section(QVBoxLayout* parent) {
    auto* title = new QLabel("SENTIMENT", this);
    title->setObjectName("newsSidePanelTitle");
    parent->addWidget(title);

    // Horizontal bar
    auto* bar_container = new QWidget(this);
    bar_container->setFixedHeight(8);
    auto* bar_layout = new QHBoxLayout(bar_container);
    bar_layout->setContentsMargins(0, 0, 0, 0);
    bar_layout->setSpacing(1);

    bull_bar_ = new QWidget(bar_container);
    bull_bar_->setObjectName("newsSentimentBull");
    neut_bar_ = new QWidget(bar_container);
    neut_bar_->setObjectName("newsSentimentNeut");
    bear_bar_ = new QWidget(bar_container);
    bear_bar_->setObjectName("newsSentimentBear");

    bar_layout->addWidget(bull_bar_, 1);
    bar_layout->addWidget(neut_bar_, 1);
    bar_layout->addWidget(bear_bar_, 1);

    parent->addWidget(bar_container);

    sentiment_score_ = new QLabel("0.00", this);
    sentiment_score_->setObjectName("newsSentimentScore");
    parent->addWidget(sentiment_score_);
}

void NewsSidePanel::build_top_stories_section(QVBoxLayout* parent) {
    auto* title = new QLabel("TOP STORIES", this);
    title->setObjectName("newsSidePanelTitle");
    parent->addWidget(title);

    auto* container = new QWidget(this);
    top_stories_layout_ = new QVBoxLayout(container);
    top_stories_layout_->setContentsMargins(0, 0, 0, 0);
    top_stories_layout_->setSpacing(2);
    parent->addWidget(container);
}

void NewsSidePanel::build_categories_section(QVBoxLayout* parent) {
    auto* title = new QLabel("CATEGORIES", this);
    title->setObjectName("newsSidePanelTitle");
    parent->addWidget(title);

    auto* container = new QWidget(this);
    categories_layout_ = new QVBoxLayout(container);
    categories_layout_->setContentsMargins(0, 0, 0, 0);
    categories_layout_->setSpacing(1);
    parent->addWidget(container);
}

void NewsSidePanel::build_monitors_section(QVBoxLayout* parent) {
    auto* title = new QLabel("MONITORS", this);
    title->setObjectName("newsSidePanelTitle");
    parent->addWidget(title);

    auto* container = new QWidget(this);
    monitors_layout_ = new QVBoxLayout(container);
    monitors_layout_->setContentsMargins(0, 0, 0, 0);
    monitors_layout_->setSpacing(2);
    parent->addWidget(container);

    // Add monitor input
    auto* add_row = new QWidget(this);
    auto* add_layout = new QHBoxLayout(add_row);
    add_layout->setContentsMargins(0, 4, 0, 0);
    add_layout->setSpacing(4);

    monitor_input_ = new QLineEdit(add_row);
    monitor_input_->setObjectName("newsMonitorInput");
    monitor_input_->setPlaceholderText("label: kw1, kw2");
    monitor_input_->setFixedHeight(22);

    auto* add_btn = new QPushButton("+", add_row);
    add_btn->setObjectName("newsMonitorAddBtn");
    add_btn->setFixedSize(22, 22);

    add_layout->addWidget(monitor_input_, 1);
    add_layout->addWidget(add_btn);

    connect(add_btn, &QPushButton::clicked, this, [this]() {
        QString text = monitor_input_->text().trimmed();
        if (text.isEmpty())
            return;
        int colon = text.indexOf(':');
        QString label = colon > 0 ? text.left(colon).trimmed() : text;
        QStringList keywords;
        if (colon > 0) {
            QString kw_str = text.mid(colon + 1).trimmed();
            keywords = kw_str.split(',', Qt::SkipEmptyParts);
            for (auto& kw : keywords)
                kw = kw.trimmed();
        } else {
            keywords = {label};
        }
        emit monitor_added(label, keywords);
        monitor_input_->clear();
    });

    connect(monitor_input_, &QLineEdit::returnPressed, add_btn, &QPushButton::click);

    parent->addWidget(add_row);
}

void NewsSidePanel::build_deviations_section(QVBoxLayout* parent) {
    deviations_section_ = new QWidget(this);
    deviations_section_->hide();

    auto* inner = new QVBoxLayout(deviations_section_);
    inner->setContentsMargins(0, 0, 0, 0);
    inner->setSpacing(2);

    auto* title = new QLabel("DEVIATIONS", deviations_section_);
    title->setObjectName("newsSidePanelTitle");
    inner->addWidget(title);

    auto* container = new QWidget(deviations_section_);
    deviations_layout_ = new QVBoxLayout(container);
    deviations_layout_->setContentsMargins(0, 0, 0, 0);
    deviations_layout_->setSpacing(1);
    inner->addWidget(container);

    parent->addWidget(deviations_section_);
}

// ── Update methods ──────────────────────────────────────────────────────────

void NewsSidePanel::update_stats(int feed_count, int article_count, int cluster_count, int source_count) {
    feeds_value_->setText(QString::number(feed_count));
    articles_value_->setText(QString::number(article_count));
    clusters_value_->setText(QString::number(cluster_count));
    sources_value_->setText(QString::number(source_count));
}

void NewsSidePanel::update_sentiment(int bullish, int bearish, int neutral) {
    int total = bullish + bearish + neutral;
    if (total == 0)
        total = 1;

    int bull_w = std::max(1, bullish * 100 / total);
    int bear_w = std::max(1, bearish * 100 / total);
    int neut_w = 100 - bull_w - bear_w;

    auto* bar_layout = bull_bar_->parentWidget()->layout();
    if (auto* hl = qobject_cast<QHBoxLayout*>(bar_layout)) {
        hl->setStretch(0, bull_w);
        hl->setStretch(1, neut_w);
        hl->setStretch(2, bear_w);
    }

    double score = total > 0 ? static_cast<double>(bullish - bearish) / total : 0.0;
    sentiment_score_->setText(QString("%1%2")
                                  .arg(score >= 0 ? "+" : "")
                                  .arg(score, 0, 'f', 2));
}

void NewsSidePanel::update_top_stories(const QVector<services::NewsArticle>& top) {
    // Clear existing
    while (top_stories_layout_->count() > 0) {
        auto* item = top_stories_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    for (int i = 0; i < top.size(); ++i) {
        const auto& article = top[i];
        auto* btn = new QPushButton(this);
        btn->setObjectName("newsTopStoryBtn");
        btn->setCursor(Qt::PointingHandCursor);

        QString pcolor = services::priority_color(article.priority);
        QString label = QString("%1. %2").arg(i + 1).arg(article.headline.left(45));
        btn->setText(label);
        btn->setToolTip(article.headline);

        // Priority dot via left border color
        btn->setStyleSheet(QString("border-left: 3px solid %1;").arg(pcolor));

        connect(btn, &QPushButton::clicked, this, [this, article]() {
            emit article_clicked(article);
        });

        top_stories_layout_->addWidget(btn);
    }
}

void NewsSidePanel::update_categories(const QMap<QString, int>& counts) {
    while (categories_layout_->count() > 0) {
        auto* item = categories_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    for (auto it = counts.begin(); it != counts.end(); ++it) {
        auto* btn = new QPushButton(this);
        btn->setObjectName("newsCategoryBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setText(QString("%1  %2").arg(it.key(), -10).arg(it.value()));

        QString cat = it.key();
        connect(btn, &QPushButton::clicked, this, [this, cat]() {
            emit category_clicked(cat);
        });

        categories_layout_->addWidget(btn);
    }
}

void NewsSidePanel::update_monitors(const QVector<services::NewsMonitor>& monitors,
                                     const QMap<QString, QVector<services::NewsArticle>>& matches) {
    while (monitors_layout_->count() > 0) {
        auto* item = monitors_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    for (const auto& monitor : monitors) {
        auto* row = new QWidget(this);
        row->setObjectName("newsMonitorRow");
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(4);

        // Color dot
        auto* dot = new QLabel(this);
        dot->setFixedSize(8, 8);
        dot->setStyleSheet(QString("background: %1; border-radius: 0;").arg(monitor.color));
        hl->addWidget(dot);

        // Label + match count
        int match_count = matches.contains(monitor.id) ? matches[monitor.id].size() : 0;
        auto* label = new QLabel(QString("%1 (%2)").arg(monitor.label).arg(match_count), this);
        label->setObjectName("newsMonitorLabel");
        hl->addWidget(label, 1);

        // Toggle button
        auto* toggle = new QPushButton(monitor.enabled ? "ON" : "OFF", this);
        toggle->setObjectName(monitor.enabled ? "newsMonitorToggleOn" : "newsMonitorToggleOff");
        toggle->setFixedSize(28, 18);
        toggle->setCursor(Qt::PointingHandCursor);
        QString mid = monitor.id;
        connect(toggle, &QPushButton::clicked, this, [this, mid]() {
            emit monitor_toggled(mid);
        });
        hl->addWidget(toggle);

        // Delete button
        auto* del_btn = new QPushButton("x", this);
        del_btn->setObjectName("newsMonitorDeleteBtn");
        del_btn->setFixedSize(18, 18);
        del_btn->setCursor(Qt::PointingHandCursor);
        connect(del_btn, &QPushButton::clicked, this, [this, mid]() {
            emit monitor_deleted(mid);
        });
        hl->addWidget(del_btn);

        monitors_layout_->addWidget(row);
    }
}

void NewsSidePanel::update_deviations(const QVector<QPair<QString, double>>& deviations) {
    while (deviations_layout_->count() > 0) {
        auto* item = deviations_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    if (deviations.isEmpty()) {
        deviations_section_->hide();
        return;
    }

    deviations_section_->show();

    for (const auto& [category, z_score] : deviations) {
        auto* row = new QWidget(this);
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(4);

        auto* cat_label = new QLabel(category, this);
        cat_label->setObjectName("newsDeviationCategory");
        hl->addWidget(cat_label);

        hl->addStretch();

        auto* score_label = new QLabel(QString("%1x").arg(z_score, 0, 'f', 1), this);
        score_label->setObjectName("newsDeviationScore");
        hl->addWidget(score_label);

        deviations_layout_->addWidget(row);
    }
}

// ── New feature update methods ──────────────────────────────────────────────

void NewsSidePanel::update_entities(const services::NerResult& ner) {
    while (entities_layout_->count() > 0) {
        auto* item = entities_layout_->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    bool has_data = false;

    // Top countries
    for (const auto& c : ner.top_countries) {
        auto* lbl = new QLabel(QString("%1  %2").arg(c.name, -6).arg(c.count), this);
        lbl->setObjectName("newsCategoryBtn");
        entities_layout_->addWidget(lbl);
        has_data = true;
    }
    // Top organizations
    for (const auto& o : ner.top_organizations) {
        auto* lbl = new QLabel(QString("%1  %2").arg(o.name.left(16), -16).arg(o.count), this);
        lbl->setObjectName("newsMonitorLabel");
        entities_layout_->addWidget(lbl);
        has_data = true;
    }
    // Top people
    for (const auto& p : ner.top_people) {
        auto* lbl = new QLabel(QString("%1  %2").arg(p.name.left(16), -16).arg(p.count), this);
        lbl->setObjectName("newsMonitorLabel");
        entities_layout_->addWidget(lbl);
        has_data = true;
    }

    entities_section_->setVisible(has_data);
}

void NewsSidePanel::update_locations(const QVector<services::ArticleGeo>& geo) {
    while (locations_layout_->count() > 0) {
        auto* item = locations_layout_->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    if (geo.isEmpty()) {
        locations_section_->hide();
        return;
    }
    locations_section_->show();

    // Show unique locations
    QMap<QString, int> loc_counts;
    for (const auto& g : geo) {
        for (const auto& l : g.locations)
            loc_counts[l.name]++;
    }

    auto sorted = loc_counts.keys();
    std::sort(sorted.begin(), sorted.end(), [&](const QString& a, const QString& b) {
        return loc_counts[a] > loc_counts[b];
    });

    for (int i = 0; i < std::min(8, static_cast<int>(sorted.size())); ++i) {
        auto* lbl = new QLabel(QString("%1  %2").arg(sorted[i].left(18), -18).arg(loc_counts[sorted[i]]), this);
        lbl->setObjectName("newsMonitorLabel");
        locations_layout_->addWidget(lbl);
    }
}

void NewsSidePanel::update_signals(const QVector<services::CorrelationSignal>& sigs) {
    while (signals_layout_->count() > 0) {
        auto* item = signals_layout_->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    if (sigs.isEmpty()) {
        signals_section_->hide();
        return;
    }
    signals_section_->show();

    for (int i = 0; i < std::min(8, static_cast<int>(sigs.size())); ++i) {
        const auto& sig = sigs[i];
        QString color = sig.severity == "critical" ? "#dc2626"
                        : (sig.severity == "high" ? "#f97316" : "#eab308");
        auto* lbl = new QLabel(sig.detail.left(35), this);
        lbl->setObjectName("newsDeviationCategory");
        lbl->setStyleSheet(QString("color: %1; font-size: 10px; background: transparent;").arg(color));
        lbl->setToolTip(sig.detail);
        signals_layout_->addWidget(lbl);
    }
}

void NewsSidePanel::update_instability(const QString& country, const services::InstabilityScore& score) {
    // Add or update CII entry
    cii_section_->show();

    // Check if already exists
    for (int i = 0; i < cii_layout_->count(); ++i) {
        auto* w = cii_layout_->itemAt(i)->widget();
        if (w && w->property("country").toString() == country) {
            auto* lbl = qobject_cast<QLabel*>(w);
            if (lbl) {
                QString color = score.level == "CRITICAL" ? "#dc2626"
                                : (score.level == "HIGH" ? "#f97316"
                                   : (score.level == "ELEVATED" ? "#eab308" : "#22c55e"));
                lbl->setText(QString("%1  %2  %3").arg(country, -4).arg(score.cii_score, 3).arg(score.level));
                lbl->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: 700; background: transparent;").arg(color));
            }
            return;
        }
    }

    // New entry
    QString color = score.level == "CRITICAL" ? "#dc2626"
                    : (score.level == "HIGH" ? "#f97316"
                       : (score.level == "ELEVATED" ? "#eab308" : "#22c55e"));
    auto* lbl = new QLabel(QString("%1  %2  %3").arg(country, -4).arg(score.cii_score, 3).arg(score.level), this);
    lbl->setObjectName("newsDeviationScore");
    lbl->setProperty("country", country);
    lbl->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: 700; background: transparent;").arg(color));
    cii_layout_->addWidget(lbl);
}

void NewsSidePanel::update_predictions(const QVector<services::PredictionMarket>& predictions) {
    while (predictions_layout_->count() > 0) {
        auto* item = predictions_layout_->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    if (predictions.isEmpty()) {
        predictions_section_->hide();
        return;
    }
    predictions_section_->show();

    for (int i = 0; i < std::min(6, static_cast<int>(predictions.size())); ++i) {
        const auto& pm = predictions[i];
        int pct = static_cast<int>(pm.yes_price * 100);
        QString color = pct >= 70 ? "#16a34a" : (pct <= 30 ? "#dc2626" : "#ca8a04");
        auto* lbl = new QLabel(QString("%1%  %2").arg(pct).arg(pm.question.left(28)), this);
        lbl->setObjectName("newsMonitorLabel");
        lbl->setStyleSheet(QString("color: %1; font-size: 10px; background: transparent;").arg(color));
        lbl->setToolTip(pm.question);
        predictions_layout_->addWidget(lbl);
    }
}

} // namespace fincept::screens
