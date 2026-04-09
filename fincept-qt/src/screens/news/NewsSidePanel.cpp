#include "screens/news/NewsSidePanel.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QPushButton>
#include <QScrollArea>

namespace fincept::screens {

NewsSidePanel::NewsSidePanel(QWidget* parent) : QWidget(parent) {
    setObjectName("newsDrawerPanel");
    setFixedWidth(280);
    hide(); // start hidden — toggled by INTEL button

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // Drawer header with close button
    auto* header = new QWidget(this);
    header->setObjectName("newsDrawerHeader");
    header->setFixedHeight(30);
    auto* header_layout = new QHBoxLayout(header);
    header_layout->setContentsMargins(10, 0, 6, 0);
    header_layout->setSpacing(0);

    auto* title = new QLabel("INTELLIGENCE", header);
    title->setObjectName("newsDrawerTitle");
    header_layout->addWidget(title);
    header_layout->addStretch();

    auto* close_btn = new QPushButton("x", header);
    close_btn->setObjectName("newsDrawerCloseBtn");
    close_btn->setFixedSize(22, 22);
    close_btn->setCursor(Qt::PointingHandCursor);
    connect(close_btn, &QPushButton::clicked, this, [this]() {
        toggle_drawer();
        emit close_requested();
    });
    header_layout->addWidget(close_btn);
    outer->addWidget(header);

    // Scrollable content area
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName("newsDrawerScroll");
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget(scroll);
    content->setObjectName("newsDrawerContent");
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(10);

    build_top_stories_section(layout);
    build_categories_section(layout);
    build_monitors_section(layout);
    build_deviations_section(layout);

    // Hidden intelligence sections
    auto build_hidden_section = [&](const QString& section_title, QVBoxLayout*& out_layout) -> QWidget* {
        auto* section = new QWidget(this);
        section->hide();
        auto* inner = new QVBoxLayout(section);
        inner->setContentsMargins(0, 0, 0, 0);
        inner->setSpacing(2);
        auto* lbl = new QLabel(section_title, section);
        lbl->setObjectName("newsDrawerSectionTitle");
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
    saved_section_ = build_hidden_section("BOOKMARKS", saved_layout_);

    layout->addStretch();
    scroll->setWidget(content);
    outer->addWidget(scroll);
}

void NewsSidePanel::toggle_drawer() {
    drawer_open_ = !drawer_open_;
    setVisible(drawer_open_);
}

void NewsSidePanel::build_top_stories_section(QVBoxLayout* parent) {
    auto* title = new QLabel("TOP STORIES", this);
    title->setObjectName("newsDrawerSectionTitle");
    parent->addWidget(title);

    auto* container = new QWidget(this);
    top_stories_layout_ = new QVBoxLayout(container);
    top_stories_layout_->setContentsMargins(0, 0, 0, 0);
    top_stories_layout_->setSpacing(2);
    parent->addWidget(container);
}

void NewsSidePanel::build_categories_section(QVBoxLayout* parent) {
    auto* title = new QLabel("CATEGORIES", this);
    title->setObjectName("newsDrawerSectionTitle");
    parent->addWidget(title);

    auto* container = new QWidget(this);
    categories_layout_ = new QVBoxLayout(container);
    categories_layout_->setContentsMargins(0, 0, 0, 0);
    categories_layout_->setSpacing(1);
    parent->addWidget(container);
}

void NewsSidePanel::build_monitors_section(QVBoxLayout* parent) {
    auto* title = new QLabel("KEYWORD MONITORS", this);
    title->setObjectName("newsDrawerSectionTitle");
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
    title->setObjectName("newsDrawerSectionTitle");
    inner->addWidget(title);

    auto* container = new QWidget(deviations_section_);
    deviations_layout_ = new QVBoxLayout(container);
    deviations_layout_->setContentsMargins(0, 0, 0, 0);
    deviations_layout_->setSpacing(1);
    inner->addWidget(container);

    parent->addWidget(deviations_section_);
}

// ── Update methods ──────────────────────────────────────────────────────────

void NewsSidePanel::update_stats(int /*feed_count*/, int /*article_count*/, int /*cluster_count*/,
                                 int /*source_count*/) {
    // Stats now shown in intel strip — no-op here for backward compat
}

void NewsSidePanel::update_sentiment(int /*bullish*/, int /*bearish*/, int /*neutral*/) {
    // Sentiment now shown in intel strip — no-op here for backward compat
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
        QString label = QString("%1. %2").arg(i + 1).arg(article.headline.left(50));
        btn->setText(label);
        btn->setToolTip(article.headline);

        // Priority dot via left border color
        btn->setStyleSheet(QString("border-left: 3px solid %1;").arg(pcolor));

        connect(btn, &QPushButton::clicked, this, [this, article]() { emit article_clicked(article); });

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
        connect(btn, &QPushButton::clicked, this, [this, cat]() { emit category_clicked(cat); });

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
        connect(toggle, &QPushButton::clicked, this, [this, mid]() { emit monitor_toggled(mid); });
        hl->addWidget(toggle);

        // Delete button
        auto* del_btn = new QPushButton("x", this);
        del_btn->setObjectName("newsMonitorDeleteBtn");
        del_btn->setFixedSize(18, 18);
        del_btn->setCursor(Qt::PointingHandCursor);
        connect(del_btn, &QPushButton::clicked, this, [this, mid]() { emit monitor_deleted(mid); });
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

// ── Intelligence section update methods ────────────────────────────────────

void NewsSidePanel::update_entities(const services::NerResult& ner) {
    while (entities_layout_->count() > 0) {
        auto* item = entities_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    bool has_data = false;

    for (const auto& c : ner.top_countries) {
        auto* lbl = new QLabel(QString("%1  %2").arg(c.name, -6).arg(c.count), this);
        lbl->setObjectName("newsCategoryBtn");
        entities_layout_->addWidget(lbl);
        has_data = true;
    }
    for (const auto& o : ner.top_organizations) {
        auto* lbl = new QLabel(QString("%1  %2").arg(o.name.left(16), -16).arg(o.count), this);
        lbl->setObjectName("newsMonitorLabel");
        entities_layout_->addWidget(lbl);
        has_data = true;
    }
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
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    if (geo.isEmpty()) {
        locations_section_->hide();
        return;
    }
    locations_section_->show();

    QMap<QString, int> loc_counts;
    for (const auto& g : geo) {
        for (const auto& l : g.locations)
            loc_counts[l.name]++;
    }

    auto sorted = loc_counts.keys();
    std::sort(sorted.begin(), sorted.end(),
              [&](const QString& a, const QString& b) { return loc_counts[a] > loc_counts[b]; });

    for (int i = 0; i < std::min(8, static_cast<int>(sorted.size())); ++i) {
        auto* lbl = new QLabel(QString("%1  %2").arg(sorted[i].left(18), -18).arg(loc_counts[sorted[i]]), this);
        lbl->setObjectName("newsMonitorLabel");
        locations_layout_->addWidget(lbl);
    }
}

void NewsSidePanel::update_signals(const QVector<services::CorrelationSignal>& sigs) {
    while (signals_layout_->count() > 0) {
        auto* item = signals_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    if (sigs.isEmpty()) {
        signals_section_->hide();
        return;
    }
    signals_section_->show();

    for (int i = 0; i < std::min(8, static_cast<int>(sigs.size())); ++i) {
        const auto& sig = sigs[i];
        QString color = sig.severity == "critical"
                            ? "" + QString(ui::colors::NEGATIVE()) + ""
                            : (sig.severity == "high" ? "" + QString(ui::colors::WARNING()) + ""
                                                      : "" + QString(ui::colors::WARNING()) + "");
        auto* lbl = new QLabel(sig.detail.left(40), this);
        lbl->setObjectName("newsDeviationCategory");
        lbl->setStyleSheet(QString("color: %1; background: transparent;").arg(color));
        lbl->setToolTip(sig.detail);
        signals_layout_->addWidget(lbl);
    }
}

void NewsSidePanel::update_instability(const QString& country, const services::InstabilityScore& score) {
    cii_section_->show();

    // Check if already exists
    for (int i = 0; i < cii_layout_->count(); ++i) {
        auto* w = cii_layout_->itemAt(i)->widget();
        if (w && w->property("country").toString() == country) {
            auto* lbl = qobject_cast<QLabel*>(w);
            if (lbl) {
                QString color = score.level == "CRITICAL"
                                    ? "" + QString(ui::colors::NEGATIVE()) + ""
                                    : (score.level == "HIGH"
                                           ? "" + QString(ui::colors::WARNING()) + ""
                                           : (score.level == "ELEVATED" ? "" + QString(ui::colors::WARNING()) + ""
                                                                        : "" + QString(ui::colors::POSITIVE()) + ""));
                lbl->setText(QString("%1  %2  %3").arg(country, -4).arg(score.cii_score, 3).arg(score.level));
                lbl->setStyleSheet(QString("color: %1; font-weight: 700; background: transparent;").arg(color));
            }
            return;
        }
    }

    // New entry
    QString color =
        score.level == "CRITICAL"
            ? "" + QString(ui::colors::NEGATIVE()) + ""
            : (score.level == "HIGH" ? "" + QString(ui::colors::WARNING()) + ""
                                     : (score.level == "ELEVATED" ? "" + QString(ui::colors::WARNING()) + ""
                                                                  : "" + QString(ui::colors::POSITIVE()) + ""));
    auto* lbl = new QLabel(QString("%1  %2  %3").arg(country, -4).arg(score.cii_score, 3).arg(score.level), this);
    lbl->setObjectName("newsDeviationScore");
    lbl->setProperty("country", country);
    lbl->setStyleSheet(QString("color: %1; font-weight: 700; background: transparent;").arg(color));
    cii_layout_->addWidget(lbl);
}

void NewsSidePanel::update_predictions(const QVector<services::PredictionMarket>& predictions) {
    while (predictions_layout_->count() > 0) {
        auto* item = predictions_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
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
        QString color = pct >= 70 ? "" + QString(ui::colors::POSITIVE()) + ""
                                  : (pct <= 30 ? "" + QString(ui::colors::NEGATIVE()) + ""
                                               : "" + QString(ui::colors::WARNING()) + "");
        auto* lbl = new QLabel(QString("%1%  %2").arg(pct).arg(pm.question.left(32)), this);
        lbl->setObjectName("newsMonitorLabel");
        lbl->setStyleSheet(QString("color: %1; background: transparent;").arg(color));
        lbl->setToolTip(pm.question);
        predictions_layout_->addWidget(lbl);
    }
}

void NewsSidePanel::update_saved(const QVector<services::NewsArticle>& saved) {
    while (saved_layout_->count() > 0) {
        auto* item = saved_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    if (saved.isEmpty()) {
        saved_section_->hide();
        return;
    }
    saved_section_->show();

    for (int i = 0; i < std::min(10, static_cast<int>(saved.size())); ++i) {
        const auto& a = saved[i];
        QString title = a.headline.left(40) + (a.headline.size() > 40 ? "..." : "");
        auto* btn = new QPushButton(title, this);
        btn->setObjectName("newsTopStoryBtn");
        btn->setToolTip(a.headline + "\n" + a.source);
        btn->setFlat(true);
        btn->setCursor(Qt::PointingHandCursor);
        connect(btn, &QPushButton::clicked, this, [this, a]() { emit article_clicked(a); });
        saved_layout_->addWidget(btn);
    }
}

} // namespace fincept::screens
