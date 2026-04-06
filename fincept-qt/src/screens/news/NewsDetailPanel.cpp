#include "screens/news/NewsDetailPanel.h"

#include "core/logging/Logger.h"
#include "services/file_manager/FileManagerService.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QScrollArea>
#include <QTextStream>
#include <QTimer>
#include <QUrl>

namespace fincept::screens {

NewsDetailPanel::NewsDetailPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("newsDetailPanel");
    setFixedWidth(340);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    stack_ = new QStackedWidget(this);
    stack_->addWidget(build_empty_state());
    stack_->addWidget(build_content_view());
    stack_->setCurrentIndex(0);

    root->addWidget(stack_);

    // Analyze timeout guard (30s)
    analyze_timeout_ = new QTimer(this);
    analyze_timeout_->setSingleShot(true);
    analyze_timeout_->setInterval(30000);
    connect(analyze_timeout_, &QTimer::timeout, this, [this]() {
        if (analyze_btn_) {
            analyze_btn_->setText("ANALYZE");
            analyze_btn_->setEnabled(true);
        }
    });
}

QWidget* NewsDetailPanel::build_empty_state() {
    auto* empty = new QWidget(this);
    auto* layout = new QVBoxLayout(empty);
    layout->setAlignment(Qt::AlignCenter);
    auto* label = new QLabel("Select an article", empty);
    label->setObjectName("newsDetailEmpty");
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);
    return empty;
}

QWidget* NewsDetailPanel::build_content_view() {
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget(scroll);
    content->setObjectName("newsDetailContent");
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(6);

    // Headline
    headline_label_ = new QLabel(content);
    headline_label_->setObjectName("newsDetailHeadline");
    headline_label_->setWordWrap(true);
    layout->addWidget(headline_label_);

    // Badge row
    auto* badge_row = new QWidget(content);
    auto* badge_layout = new QHBoxLayout(badge_row);
    badge_layout->setContentsMargins(0, 0, 0, 0);
    badge_layout->setSpacing(6);

    priority_badge_ = new QLabel(content);
    priority_badge_->setObjectName("newsDetailPriorityBadge");
    sentiment_badge_ = new QLabel(content);
    sentiment_badge_->setObjectName("newsDetailSentimentBadge");
    tier_badge_ = new QLabel(content);
    tier_badge_->setObjectName("newsDetailTierBadge");
    category_label_ = new QLabel(content);
    category_label_->setObjectName("newsDetailCategory");

    badge_layout->addWidget(priority_badge_);
    badge_layout->addWidget(sentiment_badge_);
    badge_layout->addWidget(tier_badge_);
    badge_layout->addWidget(category_label_);
    badge_layout->addStretch();
    layout->addWidget(badge_row);

    // Source + time
    auto* source_row = new QWidget(content);
    auto* source_layout = new QHBoxLayout(source_row);
    source_layout->setContentsMargins(0, 0, 0, 0);
    source_layout->setSpacing(8);
    source_label_ = new QLabel(content);
    source_label_->setObjectName("newsDetailSource");
    time_label_ = new QLabel(content);
    time_label_->setObjectName("newsDetailTime");
    source_layout->addWidget(source_label_);
    source_layout->addWidget(time_label_);
    source_layout->addStretch();
    layout->addWidget(source_row);

    // Summary
    summary_label_ = new QLabel(content);
    summary_label_->setObjectName("newsDetailSummary");
    summary_label_->setWordWrap(true);
    layout->addWidget(summary_label_);

    // Impact
    impact_label_ = new QLabel(content);
    impact_label_->setObjectName("newsDetailImpact");
    layout->addWidget(impact_label_);

    // Tickers
    tickers_label_ = new QLabel(content);
    tickers_label_->setObjectName("newsDetailTickers");
    layout->addWidget(tickers_label_);

    // Action buttons
    auto* actions = new QWidget(content);
    auto* action_layout = new QHBoxLayout(actions);
    action_layout->setContentsMargins(0, 4, 0, 4);
    action_layout->setSpacing(6);

    open_btn_ = new QPushButton("OPEN", content);
    open_btn_->setObjectName("newsDetailOpenBtn");
    open_btn_->setFixedHeight(24);
    copy_btn_ = new QPushButton("COPY URL", content);
    copy_btn_->setObjectName("newsDetailCopyBtn");
    copy_btn_->setFixedHeight(24);
    analyze_btn_ = new QPushButton("ANALYZE", content);
    analyze_btn_->setObjectName("newsDetailAnalyzeBtn");
    analyze_btn_->setFixedHeight(24);
    save_btn_ = new QPushButton("SAVE", content);
    save_btn_->setObjectName("newsDetailSaveBtn");
    save_btn_->setFixedHeight(24);
    save_btn_->setToolTip("Save article to File Manager");

    action_layout->addWidget(open_btn_);
    action_layout->addWidget(copy_btn_);
    action_layout->addWidget(analyze_btn_);
    action_layout->addWidget(save_btn_);
    layout->addWidget(actions);

    connect(open_btn_, &QPushButton::clicked, this, [this]() {
        if (has_article_)
            QDesktopServices::openUrl(QUrl(current_article_.link));
    });
    connect(copy_btn_, &QPushButton::clicked, this, [this]() {
        if (has_article_)
            QApplication::clipboard()->setText(current_article_.link);
    });
    connect(analyze_btn_, &QPushButton::clicked, this, [this]() {
        if (!has_article_)
            return;
        analyze_btn_->setText("ANALYZING...");
        analyze_btn_->setEnabled(false);
        analyze_timeout_->start();
        emit analyze_requested(current_article_.link);
    });
    connect(save_btn_, &QPushButton::clicked, this, [this]() {
        if (!has_article_) return;

        // Build a safe filename from headline
        QString safe = current_article_.headline;
        safe.replace(QRegularExpression("[^a-zA-Z0-9_\\- ]"), "").simplified();
        safe.replace(' ', '_');
        if (safe.length() > 60) safe = safe.left(60);
        QString ts = QString::number(QDateTime::currentMSecsSinceEpoch());
        QString stored_name = "news_" + safe + "_" + ts + ".txt";
        QString dest = services::FileManagerService::instance().storage_dir() + "/" + stored_name;

        QFile f(dest);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&f);
            out << current_article_.headline << "\n";
            out << current_article_.source << "  |  " << current_article_.time << "\n";
            out << current_article_.link << "\n\n";
            out << current_article_.summary << "\n";
            f.close();
            services::FileManagerService::instance().register_file(
                stored_name, safe + ".txt",
                QFileInfo(dest).size(), "text/plain", "news");
            LOG_INFO("News", "Saved article: " + stored_name);
        }
    });

    // Translate button
    translate_btn_ = new QPushButton("TRANSLATE", content);
    translate_btn_->setObjectName("newsDetailOpenBtn");
    translate_btn_->setFixedHeight(24);
    action_layout->addWidget(translate_btn_);

    connect(translate_btn_, &QPushButton::clicked, this, [this]() {
        if (!has_article_)
            return;
        translate_btn_->setText("...");
        translate_btn_->setEnabled(false);
        services::NewsNlpService::instance().translate_text(
            current_article_.headline + "\n\n" + current_article_.summary, "en",
            [this](bool ok, QString translated, QString detected_lang) {
                translate_btn_->setText("TRANSLATE");
                translate_btn_->setEnabled(true);
                if (ok && !translated.isEmpty()) {
                    // Show translation in summary field
                    summary_label_->setText(QString("[%1 -> EN] %2").arg(detected_lang, translated));
                }
            });
    });

    // Separator
    auto* sep = new QWidget(content);
    sep->setFixedHeight(1);
    sep->setObjectName("newsDetailSep");
    layout->addWidget(sep);

    // AI Analysis section (hidden until populated)
    analysis_section_ = new QWidget(content);
    analysis_section_->setObjectName("newsAnalysisSection");
    analysis_section_->hide();
    auto* analysis_layout = new QVBoxLayout(analysis_section_);
    analysis_layout->setContentsMargins(0, 4, 0, 4);
    analysis_layout->setSpacing(4);

    auto* ai_title = new QLabel("AI ANALYSIS", analysis_section_);
    ai_title->setObjectName("newsDetailSectionTitle");
    analysis_layout->addWidget(ai_title);

    ai_summary_ = new QLabel(analysis_section_);
    ai_summary_->setObjectName("newsDetailAiSummary");
    ai_summary_->setWordWrap(true);
    analysis_layout->addWidget(ai_summary_);

    auto* ai_row = new QWidget(analysis_section_);
    auto* ai_row_layout = new QHBoxLayout(ai_row);
    ai_row_layout->setContentsMargins(0, 0, 0, 0);
    ai_row_layout->setSpacing(8);
    ai_sentiment_ = new QLabel(analysis_section_);
    ai_sentiment_->setObjectName("newsDetailAiSentiment");
    ai_urgency_ = new QLabel(analysis_section_);
    ai_urgency_->setObjectName("newsDetailAiUrgency");
    ai_row_layout->addWidget(ai_sentiment_);
    ai_row_layout->addWidget(ai_urgency_);
    ai_row_layout->addStretch();
    analysis_layout->addWidget(ai_row);

    auto* kp_title = new QLabel("KEY POINTS", analysis_section_);
    kp_title->setObjectName("newsDetailSubTitle");
    analysis_layout->addWidget(kp_title);

    auto* kp_container = new QWidget(analysis_section_);
    key_points_layout_ = new QVBoxLayout(kp_container);
    key_points_layout_->setContentsMargins(0, 0, 0, 0);
    key_points_layout_->setSpacing(2);
    analysis_layout->addWidget(kp_container);

    auto* risk_title = new QLabel("RISK SIGNALS", analysis_section_);
    risk_title->setObjectName("newsDetailSubTitle");
    analysis_layout->addWidget(risk_title);

    auto* risk_container = new QWidget(analysis_section_);
    risk_layout_ = new QVBoxLayout(risk_container);
    risk_layout_->setContentsMargins(0, 0, 0, 0);
    risk_layout_->setSpacing(2);
    analysis_layout->addWidget(risk_container);

    auto* topics_title = new QLabel("TOPICS", analysis_section_);
    topics_title->setObjectName("newsDetailSubTitle");
    analysis_layout->addWidget(topics_title);

    auto* topics_container = new QWidget(analysis_section_);
    topics_layout_ = new QVBoxLayout(topics_container);
    topics_layout_->setContentsMargins(0, 0, 0, 0);
    topics_layout_->setSpacing(2);
    analysis_layout->addWidget(topics_container);

    layout->addWidget(analysis_section_);

    // Monitor matches section
    monitor_section_ = new QWidget(content);
    monitor_section_->hide();
    auto* mon_layout = new QVBoxLayout(monitor_section_);
    mon_layout->setContentsMargins(0, 4, 0, 4);
    mon_layout->setSpacing(2);
    auto* mon_title = new QLabel("MONITOR MATCHES", monitor_section_);
    mon_title->setObjectName("newsDetailSectionTitle");
    mon_layout->addWidget(mon_title);
    auto* mon_container = new QWidget(monitor_section_);
    monitor_matches_layout_ = new QVBoxLayout(mon_container);
    monitor_matches_layout_->setContentsMargins(0, 0, 0, 0);
    monitor_matches_layout_->setSpacing(2);
    mon_layout->addWidget(mon_container);
    layout->addWidget(monitor_section_);

    // Related articles section
    related_section_ = new QWidget(content);
    related_section_->hide();
    auto* rel_layout_outer = new QVBoxLayout(related_section_);
    rel_layout_outer->setContentsMargins(0, 4, 0, 4);
    rel_layout_outer->setSpacing(2);
    auto* rel_title = new QLabel("RELATED", related_section_);
    rel_title->setObjectName("newsDetailSectionTitle");
    rel_layout_outer->addWidget(rel_title);
    auto* rel_container = new QWidget(related_section_);
    related_layout_ = new QVBoxLayout(rel_container);
    related_layout_->setContentsMargins(0, 0, 0, 0);
    related_layout_->setSpacing(2);
    rel_layout_outer->addWidget(rel_container);
    layout->addWidget(related_section_);

    // Entities section
    entities_section_ = new QWidget(content);
    entities_section_->hide();
    auto* ent_layout_outer = new QVBoxLayout(entities_section_);
    ent_layout_outer->setContentsMargins(0, 4, 0, 4);
    ent_layout_outer->setSpacing(2);
    auto* ent_title = new QLabel("ENTITIES", entities_section_);
    ent_title->setObjectName("newsDetailSectionTitle");
    ent_layout_outer->addWidget(ent_title);
    auto* ent_container = new QWidget(entities_section_);
    entities_detail_layout_ = new QVBoxLayout(ent_container);
    entities_detail_layout_->setContentsMargins(0, 0, 0, 0);
    entities_detail_layout_->setSpacing(2);
    ent_layout_outer->addWidget(ent_container);
    layout->addWidget(entities_section_);

    // Infrastructure section
    infra_section_ = new QWidget(content);
    infra_section_->hide();
    auto* infra_layout_outer = new QVBoxLayout(infra_section_);
    infra_layout_outer->setContentsMargins(0, 4, 0, 4);
    infra_layout_outer->setSpacing(2);
    auto* infra_title = new QLabel("NEARBY INFRASTRUCTURE", infra_section_);
    infra_title->setObjectName("newsDetailSectionTitle");
    infra_layout_outer->addWidget(infra_title);
    auto* infra_container = new QWidget(infra_section_);
    infra_layout_ = new QVBoxLayout(infra_container);
    infra_layout_->setContentsMargins(0, 0, 0, 0);
    infra_layout_->setSpacing(2);
    infra_layout_outer->addWidget(infra_container);
    layout->addWidget(infra_section_);

    layout->addStretch();
    scroll->setWidget(content);
    return scroll;
}

// ── Public methods ──────────────────────────────────────────────────────────

void NewsDetailPanel::show_article(const services::NewsArticle& article) {
    current_article_ = article;
    has_article_ = true;
    stack_->setCurrentIndex(1);

    headline_label_->setText(article.headline);

    // Priority badge
    QString pstr = services::priority_string(article.priority);
    QString pcolor = services::priority_color(article.priority);
    priority_badge_->setText(pstr);
    priority_badge_->setStyleSheet(
        QString("background: %1; color: %2; padding: 1px 6px;").arg(pcolor, ui::colors::TEXT_PRIMARY));

    // Sentiment badge
    QString sstr = services::sentiment_string(article.sentiment);
    QString scolor = services::sentiment_color(article.sentiment);
    sentiment_badge_->setText(sstr);
    sentiment_badge_->setStyleSheet(QString("color: %1; padding: 1px 4px;").arg(scolor));

    // Tier badge
    QStringList tier_names = {"", "WIRE", "MAJOR", "SPECIALTY", "BLOG"};
    QString tier_text = (article.tier >= 1 && article.tier <= 4) ? tier_names[article.tier] : "OTHER";
    tier_badge_->setText(tier_text);

    category_label_->setText(article.category);
    time_label_->setText(services::relative_time(article.sort_ts));

    // Source with credibility flag
    if (article.source_flag != services::SourceFlag::NONE) {
        QString flag_text = services::NewsService::source_flag_label(article.source_flag);
        QString flag_color = article.source_flag == services::SourceFlag::STATE_MEDIA ? "#f97316" : ui::colors::WARNING();
        source_label_->setText(article.source.toUpper() + "  [" + flag_text + "]");
        source_label_->setStyleSheet(
            QString("color: %1; font-size: 12px; font-weight: 700; background: transparent;").arg(flag_color));
    } else {
        source_label_->setText(article.source.toUpper());
        source_label_->setStyleSheet(
            QString("color: %1; font-size: 12px; font-weight: 700; background: transparent;").arg(ui::colors::CYAN));
    }

    // Threat classification in impact label
    if (article.threat.level != services::ThreatLevel::INFO) {
        QString threat_text = services::threat_level_string(article.threat.level);
        QString threat_color = services::threat_level_color(article.threat.level);
        impact_label_->setText(QString("Threat: %1 (%2, %3% conf)")
                                   .arg(threat_text, article.threat.category)
                                   .arg(static_cast<int>(article.threat.confidence * 100)));
        impact_label_->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent;").arg(threat_color));
    }
    summary_label_->setText(article.summary.isEmpty() ? "No summary available." : article.summary);
    impact_label_->setText(QString("Impact: %1").arg(services::impact_string(article.impact)));

    if (!article.tickers.isEmpty())
        tickers_label_->setText("$" + article.tickers.join("  $"));
    else
        tickers_label_->clear();

    // Reset analysis
    analysis_section_->hide();
    analyze_btn_->setText("ANALYZE");
    analyze_btn_->setEnabled(true);
    analyze_timeout_->stop();

    // Clear related and monitors
    show_related({});
    monitor_section_->hide();
}

void NewsDetailPanel::show_analysis(const services::NewsAnalysis& analysis) {
    analyze_btn_->setText("ANALYZE");
    analyze_btn_->setEnabled(true);
    analyze_timeout_->stop();

    ai_summary_->setText(analysis.summary.isEmpty() ? "No AI summary available." : analysis.summary);

    double score = std::clamp(analysis.sentiment.score, -1.0, 1.0);
    QString sent_color =
        score > 0.1 ? ui::colors::POSITIVE : (score < -0.1 ? ui::colors::NEGATIVE : ui::colors::WARNING);
    ai_sentiment_->setText(QString("Sentiment: %1%2").arg(score >= 0 ? "+" : "").arg(score, 0, 'f', 2));
    ai_sentiment_->setStyleSheet(QString("color: %1;").arg(sent_color));

    ai_urgency_->setText(QString("Urgency: %1").arg(analysis.market_impact.urgency));

    // Key points
    while (key_points_layout_->count() > 0) {
        auto* item = key_points_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    for (const auto& point : analysis.key_points) {
        auto* lbl = new QLabel(QString("- %1").arg(point), analysis_section_);
        lbl->setObjectName("newsDetailKeyPoint");
        lbl->setWordWrap(true);
        key_points_layout_->addWidget(lbl);
    }

    // Risk signals
    while (risk_layout_->count() > 0) {
        auto* item = risk_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    auto add_risk = [&](const QString& name, const services::RiskSignal& sig) {
        if (sig.level.isEmpty() || sig.level == "none")
            return;
        auto* lbl = new QLabel(QString("%1: %2").arg(name, sig.level), analysis_section_);
        lbl->setObjectName("newsDetailRisk");
        lbl->setToolTip(sig.details);
        risk_layout_->addWidget(lbl);
    };
    add_risk("Regulatory", analysis.regulatory);
    add_risk("Geopolitical", analysis.geopolitical);
    add_risk("Operational", analysis.operational);
    add_risk("Market", analysis.market);

    // Topics
    while (topics_layout_->count() > 0) {
        auto* item = topics_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    auto* topics_row = new QWidget(analysis_section_);
    auto* topics_flow = new QHBoxLayout(topics_row);
    topics_flow->setContentsMargins(0, 0, 0, 0);
    topics_flow->setSpacing(4);
    for (const auto& topic : analysis.topics) {
        auto* badge = new QLabel(topic, analysis_section_);
        badge->setObjectName("newsDetailTopicBadge");
        topics_flow->addWidget(badge);
    }
    topics_flow->addStretch();
    topics_layout_->addWidget(topics_row);

    analysis_section_->show();
}

void NewsDetailPanel::show_related(const QVector<services::NewsArticle>& related) {
    while (related_layout_->count() > 0) {
        auto* item = related_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    if (related.isEmpty()) {
        related_section_->hide();
        return;
    }

    related_section_->show();
    for (const auto& article : related) {
        auto* btn = new QPushButton(related_section_);
        btn->setObjectName("newsRelatedBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setText(QString("%1 - %2").arg(article.source, article.headline.left(50)));
        btn->setToolTip(article.headline);

        connect(btn, &QPushButton::clicked, this, [this, article]() { emit related_article_clicked(article); });

        related_layout_->addWidget(btn);
    }
}

void NewsDetailPanel::show_monitor_matches(const QVector<QPair<services::NewsMonitor, QStringList>>& matches) {
    while (monitor_matches_layout_->count() > 0) {
        auto* item = monitor_matches_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    if (matches.isEmpty()) {
        monitor_section_->hide();
        return;
    }

    monitor_section_->show();
    for (const auto& [monitor, keywords] : matches) {
        auto* row = new QWidget(monitor_section_);
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(4);

        auto* dot = new QLabel(row);
        dot->setFixedSize(6, 6);
        dot->setStyleSheet(QString("background: %1;").arg(monitor.color));
        hl->addWidget(dot);

        auto* lbl = new QLabel(QString("%1: %2").arg(monitor.label, keywords.join(", ")), row);
        lbl->setObjectName("newsMonitorMatchLabel");
        lbl->setWordWrap(true);
        hl->addWidget(lbl, 1);

        monitor_matches_layout_->addWidget(row);
    }
}

void NewsDetailPanel::show_entities(const services::EntityResult& entities) {
    while (entities_detail_layout_->count() > 0) {
        auto* item = entities_detail_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    bool has_data = false;

    // Countries
    for (const auto& [name, code] : entities.countries) {
        auto* lbl = new QLabel(QString("Country: %1 (%2)").arg(name, code), entities_section_);
        lbl->setObjectName("newsDetailKeyPoint");
        entities_detail_layout_->addWidget(lbl);
        has_data = true;
    }
    // Organizations
    for (const auto& org : entities.organizations) {
        auto* lbl = new QLabel(QString("Org: %1").arg(org), entities_section_);
        lbl->setObjectName("newsDetailKeyPoint");
        entities_detail_layout_->addWidget(lbl);
        has_data = true;
    }
    // People
    for (const auto& person : entities.people) {
        auto* lbl = new QLabel(QString("Person: %1").arg(person), entities_section_);
        lbl->setObjectName("newsDetailKeyPoint");
        entities_detail_layout_->addWidget(lbl);
        has_data = true;
    }

    entities_section_->setVisible(has_data);
}

void NewsDetailPanel::show_infrastructure(const QVector<services::InfrastructureItem>& items) {
    while (infra_layout_->count() > 0) {
        auto* item = infra_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    if (items.isEmpty()) {
        infra_section_->hide();
        return;
    }
    infra_section_->show();

    for (const auto& inf : items) {
        QString type_icon = inf.type == "airport"
                                ? "AIR"
                                : (inf.type == "military" ? "MIL" : (inf.type == "power_plant" ? "PWR" : "PRT"));
        auto* lbl =
            new QLabel(QString("[%1] %2 — %3 km").arg(type_icon, inf.name.left(20)).arg(inf.distance_km, 0, 'f', 1),
                       infra_section_);
        lbl->setObjectName("newsDetailKeyPoint");
        infra_layout_->addWidget(lbl);
    }
}

void NewsDetailPanel::clear() {
    has_article_ = false;
    stack_->setCurrentIndex(0);
    analysis_section_->hide();
    related_section_->hide();
    monitor_section_->hide();
    entities_section_->hide();
    infra_section_->hide();
    analyze_timeout_->stop();
}

} // namespace fincept::screens
