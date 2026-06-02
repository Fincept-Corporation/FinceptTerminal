#include "screens/news/NewsDetailPanel.h"

#include "core/logging/Logger.h"
#include "services/file_manager/FileManagerService.h"
#include "storage/repositories/NewsArticleRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSizePolicy>
#include <QTextStream>
#include <QTimer>
#include <QUrl>

namespace fincept::screens {

namespace {
// Fixed width of the right-hand detail panel. The scroll content is capped to
// this so word-wrapped labels wrap against it instead of reporting their full
// unwrapped width — otherwise revealing the AI-analysis section balloons the
// panel's size hint and pushes it past the screen edge.
constexpr int kPanelWidth = 420;
} // namespace

NewsDetailPanel::NewsDetailPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("newsDetailOverlay");
    setFixedWidth(kPanelWidth);
    hide(); // start hidden — shown on article click

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Panel header with close button
    auto* header = new QWidget(this);
    header->setObjectName("newsDetailHeader");
    header->setFixedHeight(30);
    auto* header_layout = new QHBoxLayout(header);
    header_layout->setContentsMargins(10, 0, 6, 0);
    header_layout->setSpacing(0);

    header_title_ = new QLabel(tr("ARTICLE DETAIL"), header);
    header_title_->setObjectName("newsDetailHeaderTitle");
    header_layout->addWidget(header_title_);
    header_layout->addStretch();

    close_btn_ = new QPushButton("x", header);
    close_btn_->setObjectName("newsDetailCloseBtn");
    close_btn_->setFixedSize(22, 22);
    close_btn_->setCursor(Qt::PointingHandCursor);
    connect(close_btn_, &QPushButton::clicked, this, &NewsDetailPanel::close_panel);
    header_layout->addWidget(close_btn_);
    root->addWidget(header);

    stack_ = new QStackedWidget(this);
    stack_->addWidget(build_empty_state());
    stack_->addWidget(build_content_view());
    stack_->setCurrentIndex(0);

    root->addWidget(stack_, 1);

    // Analyze timeout guard (30s)
    analyze_timeout_ = new QTimer(this);
    analyze_timeout_->setSingleShot(true);
    analyze_timeout_->setInterval(30000);
    connect(analyze_timeout_, &QTimer::timeout, this, [this]() {
        if (analyze_btn_) {
            analyze_btn_->setText(tr("ANALYZE"));
            analyze_btn_->setEnabled(true);
        }
    });
}

QWidget* NewsDetailPanel::build_empty_state() {
    auto* empty = new QWidget(this);
    auto* layout = new QVBoxLayout(empty);
    layout->setAlignment(Qt::AlignCenter);
    empty_label_ = new QLabel(tr("Select an article"), empty);
    empty_label_->setObjectName("newsDetailEmpty");
    empty_label_->setAlignment(Qt::AlignCenter);
    layout->addWidget(empty_label_);
    return empty;
}

QWidget* NewsDetailPanel::build_content_view() {
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget(scroll);
    content->setObjectName("newsDetailContent");
    // Cap the content to the panel width so word-wrapped labels wrap against it
    // rather than reporting their full single-line width — without this, showing
    // the AI-analysis section grows the panel beyond the screen edge.
    content->setMaximumWidth(kPanelWidth);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(8);

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

    // Action buttons — a 3-column grid so the row wraps within the fixed
    // 420px panel width instead of forcing a horizontal scrollbar.
    auto* actions = new QWidget(content);
    auto* action_layout = new QGridLayout(actions);
    action_layout->setContentsMargins(0, 4, 0, 4);
    action_layout->setHorizontalSpacing(6);
    action_layout->setVerticalSpacing(6);

    open_btn_ = new QPushButton(tr("OPEN"), content);
    open_btn_->setObjectName("newsDetailOpenBtn");
    copy_btn_ = new QPushButton(tr("COPY URL"), content);
    copy_btn_->setObjectName("newsDetailCopyBtn");
    copy_title_btn_ = new QPushButton(tr("COPY TITLE"), content);
    copy_title_btn_->setObjectName("newsDetailCopyTitleBtn");
    copy_title_btn_->setToolTip(tr("Copy article headline to clipboard"));
    analyze_btn_ = new QPushButton(tr("ANALYZE"), content);
    analyze_btn_->setObjectName("newsDetailAnalyzeBtn");
    save_btn_ = new QPushButton(tr("SAVE"), content);
    save_btn_->setObjectName("newsDetailSaveBtn");
    save_btn_->setToolTip(tr("Save article to File Manager"));

    bookmark_btn_ = new QPushButton(tr("BOOKMARK"), content);
    bookmark_btn_->setObjectName("newsDetailSaveBtn");
    bookmark_btn_->setToolTip(tr("Bookmark article"));
    bookmark_btn_->setCheckable(true);

    // Translate button
    translate_btn_ = new QPushButton(tr("TRANSLATE"), content);
    translate_btn_->setObjectName("newsDetailOpenBtn");

    // All action buttons share a uniform height and expand to fill their grid
    // cell — no fixed widths, so nothing can overflow the panel.
    for (QPushButton* b :
         {open_btn_, copy_btn_, copy_title_btn_, analyze_btn_, save_btn_, bookmark_btn_, translate_btn_}) {
        b->setFixedHeight(24);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    QPushButton* action_btns[] = {open_btn_,    copy_btn_, copy_title_btn_, analyze_btn_,
                                  save_btn_,     bookmark_btn_, translate_btn_};
    constexpr int kCols = 3;
    for (int i = 0; i < 7; ++i)
        action_layout->addWidget(action_btns[i], i / kCols, i % kCols);
    for (int c = 0; c < kCols; ++c)
        action_layout->setColumnStretch(c, 1);
    layout->addWidget(actions);

    connect(open_btn_, &QPushButton::clicked, this, [this]() {
        if (has_article_)
            QDesktopServices::openUrl(QUrl(current_article_.link));
    });
    connect(copy_btn_, &QPushButton::clicked, this, [this]() {
        if (has_article_)
            QApplication::clipboard()->setText(current_article_.link);
    });
    connect(copy_title_btn_, &QPushButton::clicked, this, [this]() {
        if (!has_article_ || current_article_.headline.isEmpty())
            return;
        QApplication::clipboard()->setText(current_article_.headline);

        // Brief visual confirmation — flip label for ~1 s, then revert.
        copy_title_btn_->setText(tr("COPIED"));
        QTimer::singleShot(1000, this, [this]() {
            copy_title_btn_->setText(tr("COPY TITLE"));
        });
    });
    connect(analyze_btn_, &QPushButton::clicked, this, [this]() {
        if (!has_article_)
            return;
        analyze_btn_->setText(tr("ANALYZING..."));
        analyze_btn_->setEnabled(false);
        analyze_timeout_->start();
        emit analyze_requested(current_article_.link);
    });
    connect(save_btn_, &QPushButton::clicked, this, [this]() {
        if (!has_article_)
            return;

        // Build a safe filename from headline
        QString safe = current_article_.headline;
        safe = safe.replace(QRegularExpression("[^a-zA-Z0-9_\\- ]"), "").simplified();
        safe.replace(' ', '_');
        if (safe.length() > 60)
            safe = safe.left(60);
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
            services::FileManagerService::instance().register_file(stored_name, safe + ".txt", QFileInfo(dest).size(),
                                                                   "text/plain", "news");
            LOG_INFO("News", "Saved article: " + stored_name);
        }
    });
    connect(bookmark_btn_, &QPushButton::clicked, this, [this]() {
        if (!has_article_)
            return;
        emit bookmark_requested(current_article_);
    });
    connect(translate_btn_, &QPushButton::clicked, this, [this]() {
        if (!has_article_)
            return;
        translate_btn_->setText(tr("..."));
        translate_btn_->setEnabled(false);
        services::NewsNlpService::instance().translate_text(
            current_article_.headline + "\n\n" + current_article_.summary, "en",
            [this](bool ok, QString translated, QString detected_lang) {
                translate_btn_->setText(tr("TRANSLATE"));
                translate_btn_->setEnabled(true);
                if (ok && !translated.isEmpty()) {
                    summary_label_->setText(tr("[%1 -> EN] %2").arg(detected_lang, translated));
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

    ai_title_ = new QLabel(tr("AI ANALYSIS"), analysis_section_);
    ai_title_->setObjectName("newsDetailSectionTitle");
    analysis_layout->addWidget(ai_title_);

    // Fetch-note banner — shown only when the publisher blocked content and
    // the analysis is metadata-only, so the numbers aren't read as gospel.
    ai_fetch_note_ = new QLabel(analysis_section_);
    ai_fetch_note_->setObjectName("newsDetailAiFetchNote");
    ai_fetch_note_->setWordWrap(true);
    ai_fetch_note_->hide();
    analysis_layout->addWidget(ai_fetch_note_);

    ai_summary_ = new QLabel(analysis_section_);
    ai_summary_->setObjectName("newsDetailAiSummary");
    ai_summary_->setWordWrap(true);
    analysis_layout->addWidget(ai_summary_);

    // Metric pills — laid out in a wrapping 2-column grid so a long urgency /
    // prediction string can never push past the panel's fixed width.
    auto* ai_row = new QWidget(analysis_section_);
    auto* ai_row_layout = new QGridLayout(ai_row);
    ai_row_layout->setContentsMargins(0, 0, 0, 0);
    ai_row_layout->setHorizontalSpacing(6);
    ai_row_layout->setVerticalSpacing(4);
    ai_sentiment_ = new QLabel(analysis_section_);
    ai_sentiment_->setObjectName("newsDetailAiSentiment");
    ai_sentiment_->setWordWrap(true);
    ai_urgency_ = new QLabel(analysis_section_);
    ai_urgency_->setObjectName("newsDetailAiUrgency");
    ai_urgency_->setWordWrap(true);
    ai_prediction_ = new QLabel(analysis_section_);
    ai_prediction_->setObjectName("newsDetailAiUrgency");
    ai_prediction_->setWordWrap(true);
    ai_confidence_ = new QLabel(analysis_section_);
    ai_confidence_->setObjectName("newsDetailAiConfidence");
    ai_confidence_->setWordWrap(true);
    ai_row_layout->addWidget(ai_sentiment_, 0, 0);
    ai_row_layout->addWidget(ai_confidence_, 0, 1);
    ai_row_layout->addWidget(ai_urgency_, 1, 0);
    ai_row_layout->addWidget(ai_prediction_, 1, 1);
    ai_row_layout->setColumnStretch(0, 1);
    ai_row_layout->setColumnStretch(1, 1);
    analysis_layout->addWidget(ai_row);

    // Keywords row (populated in show_analysis).
    ai_keywords_ = new QLabel(analysis_section_);
    ai_keywords_->setObjectName("newsDetailAiKeywords");
    ai_keywords_->setWordWrap(true);
    ai_keywords_->hide();
    analysis_layout->addWidget(ai_keywords_);

    key_points_title_ = new QLabel(tr("KEY POINTS"), analysis_section_);
    key_points_title_->setObjectName("newsDetailSubTitle");
    analysis_layout->addWidget(key_points_title_);

    auto* kp_container = new QWidget(analysis_section_);
    key_points_layout_ = new QVBoxLayout(kp_container);
    key_points_layout_->setContentsMargins(0, 0, 0, 0);
    key_points_layout_->setSpacing(2);
    analysis_layout->addWidget(kp_container);

    risk_title_ = new QLabel(tr("RISK SIGNALS"), analysis_section_);
    risk_title_->setObjectName("newsDetailSubTitle");
    analysis_layout->addWidget(risk_title_);

    auto* risk_container = new QWidget(analysis_section_);
    risk_layout_ = new QVBoxLayout(risk_container);
    risk_layout_->setContentsMargins(0, 0, 0, 0);
    risk_layout_->setSpacing(2);
    analysis_layout->addWidget(risk_container);

    topics_title_ = new QLabel(tr("TOPICS"), analysis_section_);
    topics_title_->setObjectName("newsDetailSubTitle");
    analysis_layout->addWidget(topics_title_);

    auto* topics_container = new QWidget(analysis_section_);
    topics_layout_ = new QVBoxLayout(topics_container);
    topics_layout_->setContentsMargins(0, 0, 0, 0);
    topics_layout_->setSpacing(2);
    analysis_layout->addWidget(topics_container);

    // Entities extracted by the analyze endpoint (orgs / people / locations).
    // Title hides itself when there are no entities (handled in show_analysis).
    ai_entities_title_ = new QLabel(tr("ENTITIES"), analysis_section_);
    ai_entities_title_->setObjectName("newsDetailSubTitle");
    analysis_layout->addWidget(ai_entities_title_);

    auto* ai_ent_container = new QWidget(analysis_section_);
    ai_entities_layout_ = new QVBoxLayout(ai_ent_container);
    ai_entities_layout_->setContentsMargins(0, 0, 0, 0);
    ai_entities_layout_->setSpacing(2);
    analysis_layout->addWidget(ai_ent_container);

    // Credits footer (e.g. "Credits used: 1 / remaining: 9").
    ai_credits_ = new QLabel(analysis_section_);
    ai_credits_->setObjectName("newsDetailAiCredits");
    ai_credits_->setStyleSheet("color:#94a3b8; font-size:10px;");
    ai_credits_->hide();
    analysis_layout->addWidget(ai_credits_);

    layout->addWidget(analysis_section_);

    // Monitor matches section
    monitor_section_ = new QWidget(content);
    monitor_section_->hide();
    auto* mon_layout = new QVBoxLayout(monitor_section_);
    mon_layout->setContentsMargins(0, 4, 0, 4);
    mon_layout->setSpacing(2);
    monitor_title_ = new QLabel(tr("MONITOR MATCHES"), monitor_section_);
    monitor_title_->setObjectName("newsDetailSectionTitle");
    mon_layout->addWidget(monitor_title_);
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
    related_title_ = new QLabel(tr("RELATED"), related_section_);
    related_title_->setObjectName("newsDetailSectionTitle");
    rel_layout_outer->addWidget(related_title_);
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
    entities_section_title_ = new QLabel(tr("ENTITIES"), entities_section_);
    entities_section_title_->setObjectName("newsDetailSectionTitle");
    ent_layout_outer->addWidget(entities_section_title_);
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
    infra_title_ = new QLabel(tr("NEARBY INFRASTRUCTURE"), infra_section_);
    infra_title_->setObjectName("newsDetailSectionTitle");
    infra_layout_outer->addWidget(infra_title_);
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

// ── Panel open/close ───────────────────────────────────────────────────────

void NewsDetailPanel::open_panel() {
    if (!panel_open_) {
        panel_open_ = true;
        show();
    }
}

void NewsDetailPanel::close_panel() {
    if (panel_open_) {
        panel_open_ = false;
        hide();
        emit panel_closed();
    }
}

// ── Public methods ──────────────────────────────────────────────────────────

void NewsDetailPanel::show_article(const services::NewsArticle& article) {
    current_article_ = article;
    has_article_ = true;
    stack_->setCurrentIndex(1);
    open_panel();

    headline_label_->setText(article.headline);

    // Priority badge
    QString pstr = services::priority_string(article.priority);
    QString pcolor = services::priority_color(article.priority);
    priority_badge_->setText(pstr);
    priority_badge_->setStyleSheet(
        QString("background: %1; color: %2; padding: 1px 6px;").arg(pcolor, ui::colors::TEXT_PRIMARY()));

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
        QString flag_color = article.source_flag == services::SourceFlag::STATE_MEDIA
                                 ? "" + QString(ui::colors::WARNING()) + ""
                                 : ui::colors::WARNING();
        source_label_->setText(article.source.toUpper() + "  [" + flag_text + "]");
        source_label_->setStyleSheet(QString("color: %1; font-weight: 700; background: transparent;").arg(flag_color));
    } else {
        source_label_->setText(article.source.toUpper());
        source_label_->setStyleSheet(
            QString("color: %1; font-weight: 700; background: transparent;").arg(ui::colors::CYAN()));
    }

    summary_label_->setText(article.summary.isEmpty() ? tr("No summary available.") : article.summary);

    // Impact + (optional) threat classification share the impact label.
    // Threat takes precedence when present so users see the more specific
    // signal — the bare impact level alone wasn't useful here.
    if (article.threat.level != services::ThreatLevel::INFO) {
        const QString threat_text = services::threat_level_string(article.threat.level);
        const QString threat_color = services::threat_level_color(article.threat.level);
        impact_label_->setText(tr("Impact: %1  •  Threat: %2 (%3, %4% conf)")
                                   .arg(services::impact_string(article.impact))
                                   .arg(threat_text, article.threat.category)
                                   .arg(static_cast<int>(article.threat.confidence * 100)));
        impact_label_->setStyleSheet(QString("color: %1; background: transparent;").arg(threat_color));
    } else {
        impact_label_->setText(tr("Impact: %1").arg(services::impact_string(article.impact)));
        impact_label_->setStyleSheet("background: transparent;");
    }

    if (!article.tickers.isEmpty())
        tickers_label_->setText("$" + article.tickers.join("  $"));
    else
        tickers_label_->clear();

    // Reset analysis
    analysis_section_->hide();
    analyze_btn_->setText(tr("ANALYZE"));
    analyze_btn_->setEnabled(true);
    analyze_timeout_->stop();

    // Reflect saved state from DB
    {
        auto r = fincept::NewsArticleRepository::instance().load_saved();
        bool is_saved = false;
        if (r.is_ok()) {
            for (const auto& a : r.value()) {
                if (a.id == article.id) {
                    is_saved = true;
                    break;
                }
            }
        }
        bookmark_btn_->setChecked(is_saved);
        bookmark_btn_->setText(is_saved ? tr("BOOKMARKED") : tr("BOOKMARK"));
    }

    // Clear related and monitors
    show_related({});
    monitor_section_->hide();
}

void NewsDetailPanel::show_analysis(const services::NewsAnalysis& analysis) {
    analyze_btn_->setText("ANALYZE");
    analyze_btn_->setEnabled(true);
    analyze_timeout_->stop();

    // Fetch-note banner — flag when the analysis is metadata-only.
    if (!analysis.content.fetch_note.isEmpty()) {
        ai_fetch_note_->setText("⚠  " + analysis.content.fetch_note);
        ai_fetch_note_->show();
    } else {
        ai_fetch_note_->hide();
    }

    ai_summary_->setText(analysis.summary.isEmpty() ? tr("No AI summary available.") : analysis.summary);

    double score = std::clamp(analysis.sentiment.score, -1.0, 1.0);
    QString sent_color =
        score > 0.1 ? ui::colors::POSITIVE : (score < -0.1 ? ui::colors::NEGATIVE : ui::colors::WARNING);
    ai_sentiment_->setText(tr("Sentiment %1%2  •  int %3")
                               .arg(score >= 0 ? "+" : "")
                               .arg(score, 0, 'f', 2)
                               .arg(analysis.sentiment.intensity, 0, 'f', 2));
    ai_sentiment_->setStyleSheet(QString("color: %1; font-weight: 700;").arg(sent_color));

    // Urgency pill — color tracks severity.
    const QString urg = analysis.market_impact.urgency.toUpper();
    QString urg_color = urg == "HIGH" ? ui::colors::NEGATIVE
                                       : (urg == "MEDIUM" ? ui::colors::WARNING : ui::colors::POSITIVE);
    ai_urgency_->setText(tr("Urgency: %1").arg(urg.isEmpty() ? QStringLiteral("—") : urg));
    ai_urgency_->setStyleSheet(QString("color: %1;").arg(urg_color));

    // Prediction pill — directional market impact.
    const QString pred = analysis.market_impact.prediction;
    QString pred_color = pred.contains("positive") ? ui::colors::POSITIVE
                                                    : (pred.contains("negative") ? ui::colors::NEGATIVE
                                                                                  : ui::colors::WARNING);
    // API sends snake_case (e.g. "moderate_positive"); show it as readable words.
    QString pred_text = pred.isEmpty() ? tr("neutral") : QString(pred).replace('_', ' ');
    ai_prediction_->setText(tr("Outlook: %1").arg(pred_text));
    ai_prediction_->setStyleSheet(QString("color: %1;").arg(pred_color));

    ai_confidence_->setText(
        tr("Confidence: %1%").arg(static_cast<int>(analysis.sentiment.confidence * 100)));

    // Key points
    while (key_points_layout_->count() > 0) {
        auto* item = key_points_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    for (const auto& point : analysis.key_points) {
        auto* lbl = new QLabel(QString("•  %1").arg(point), analysis_section_);
        lbl->setObjectName("newsDetailKeyPoint");
        lbl->setWordWrap(true);
        key_points_layout_->addWidget(lbl);
    }
    // Hide the heading + container entirely when there's nothing to show, so
    // the panel doesn't render a bare "KEY POINTS" label over empty space.
    const bool has_key_points = !analysis.key_points.isEmpty();
    key_points_title_->setVisible(has_key_points);
    key_points_layout_->parentWidget()->setVisible(has_key_points);

    // Risk signals — each shows level (color-coded) plus the detail text
    // inline (was tooltip-only before), wrapped so it never overflows.
    while (risk_layout_->count() > 0) {
        auto* item = risk_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    auto risk_color = [](const QString& level) -> QString {
        const QString l = level.toLower();
        if (l == "critical" || l == "high")
            return ui::colors::NEGATIVE;
        if (l == "medium" || l == "moderate")
            return ui::colors::WARNING;
        if (l == "low")
            return ui::colors::POSITIVE;
        return ui::colors::TEXT_TERTIARY; // "none" / unknown
    };
    auto add_risk = [&](const QString& name, const services::RiskSignal& sig) {
        if (sig.level.isEmpty() || sig.level.compare("none", Qt::CaseInsensitive) == 0)
            return;
        auto* row = new QWidget(analysis_section_);
        auto* rl = new QVBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(0);
        auto* head = new QLabel(tr("%1 — %2").arg(name, sig.level.toUpper()), row);
        head->setObjectName("newsDetailRisk");
        head->setStyleSheet(QString("color: %1; font-weight: 700;").arg(risk_color(sig.level)));
        rl->addWidget(head);
        if (!sig.details.isEmpty()) {
            auto* det = new QLabel(sig.details, row);
            det->setObjectName("newsDetailRiskDetail");
            det->setWordWrap(true);
            rl->addWidget(det);
        }
        risk_layout_->addWidget(row);
    };
    add_risk(tr("Regulatory"), analysis.regulatory);
    add_risk(tr("Geopolitical"), analysis.geopolitical);
    add_risk(tr("Operational"), analysis.operational);
    add_risk(tr("Market"), analysis.market);
    // add_risk skips "none"/empty levels, so an all-clear article adds no rows —
    // hide the heading + container in that case.
    const bool has_risks = risk_layout_->count() > 0;
    risk_title_->setVisible(has_risks);
    risk_layout_->parentWidget()->setVisible(has_risks);

    // Topics — wrapping grid of badges (a QHBoxLayout would overflow the
    // panel's fixed width with no way to wrap).
    while (topics_layout_->count() > 0) {
        auto* item = topics_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    if (!analysis.topics.isEmpty()) {
        auto* topics_row = new QWidget(analysis_section_);
        auto* topics_grid = new QGridLayout(topics_row);
        topics_grid->setContentsMargins(0, 0, 0, 0);
        topics_grid->setHorizontalSpacing(4);
        topics_grid->setVerticalSpacing(4);
        constexpr int kTopicCols = 3;
        for (int i = 0; i < analysis.topics.size(); ++i) {
            auto* badge = new QLabel(analysis.topics[i], topics_row);
            badge->setObjectName("newsDetailTopicBadge");
            badge->setAlignment(Qt::AlignCenter);
            topics_grid->addWidget(badge, i / kTopicCols, i % kTopicCols);
        }
        for (int c = 0; c < kTopicCols; ++c)
            topics_grid->setColumnStretch(c, 1);
        topics_layout_->addWidget(topics_row);
    }
    const bool has_topics = !analysis.topics.isEmpty();
    topics_title_->setVisible(has_topics);
    topics_layout_->parentWidget()->setVisible(has_topics);

    // Entities — organizations (with ticker/sector), people, locations.
    while (ai_entities_layout_->count() > 0) {
        auto* item = ai_entities_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    auto add_entity = [&](const QString& prefix, const services::AnalysisEntity& e) {
        QString text = prefix + e.name;
        if (!e.detail.isEmpty())
            text += QString(" (%1)").arg(e.detail);
        if (!e.sector.isEmpty())
            text += QString(" · %1").arg(e.sector);
        auto* lbl = new QLabel(text, analysis_section_);
        lbl->setObjectName("newsDetailEntity");
        lbl->setWordWrap(true);
        ai_entities_layout_->addWidget(lbl);
    };
    for (const auto& o : analysis.organizations)
        add_entity(tr("ORG  "), o);
    for (const auto& p : analysis.people)
        add_entity(tr("PER  "), p);
    for (const auto& l : analysis.locations)
        add_entity(tr("LOC  "), l);
    const bool has_entities =
        !analysis.organizations.isEmpty() || !analysis.people.isEmpty() || !analysis.locations.isEmpty();
    ai_entities_title_->setVisible(has_entities);
    ai_entities_layout_->parentWidget()->setVisible(has_entities);

    // Keywords — show as a single hashtagged line under topics.
    if (!analysis.keywords.isEmpty()) {
        QStringList tagged;
        tagged.reserve(analysis.keywords.size());
        for (const auto& k : analysis.keywords) {
            const QString trimmed = k.trimmed();
            if (!trimmed.isEmpty())
                tagged << "#" + trimmed;
        }
        ai_keywords_->setText(tagged.join("  "));
        ai_keywords_->show();
    } else {
        ai_keywords_->hide();
    }

    // Credits footer — only shown when the backend reported a credit
    // count (zero/zero means the API didn't surface it).
    if (analysis.credits_used > 0 || analysis.credits_remaining > 0) {
        ai_credits_->setText(tr("Credits used: %1  •  remaining: %2")
                                 .arg(analysis.credits_used)
                                 .arg(analysis.credits_remaining));
        ai_credits_->show();
    } else {
        ai_credits_->hide();
    }

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
        btn->setText(QString("%1 - %2").arg(article.source, article.headline.left(55)));
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

    for (const auto& [name, code] : entities.countries) {
        auto* lbl = new QLabel(tr("Country: %1 (%2)").arg(name, code), entities_section_);
        lbl->setObjectName("newsDetailKeyPoint");
        entities_detail_layout_->addWidget(lbl);
        has_data = true;
    }
    for (const auto& org : entities.organizations) {
        auto* lbl = new QLabel(tr("Org: %1").arg(org), entities_section_);
        lbl->setObjectName("newsDetailKeyPoint");
        entities_detail_layout_->addWidget(lbl);
        has_data = true;
    }
    for (const auto& person : entities.people) {
        auto* lbl = new QLabel(tr("Person: %1").arg(person), entities_section_);
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
                                ? tr("AIR")
                                : (inf.type == "military" ? tr("MIL") : (inf.type == "power_plant" ? tr("PWR") : tr("PRT")));
        auto* lbl =
            new QLabel(tr("[%1] %2 — %3 km").arg(type_icon, inf.name.left(20)).arg(inf.distance_km, 0, 'f', 1),
                       infra_section_);
        lbl->setObjectName("newsDetailKeyPoint");
        infra_layout_->addWidget(lbl);
    }
}

void NewsDetailPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void NewsDetailPanel::retranslateUi() {
    // Static header / empty-state / section titles.
    if (header_title_)            header_title_->setText(tr("ARTICLE DETAIL"));
    if (empty_label_)             empty_label_->setText(tr("Select an article"));
    if (ai_title_)                ai_title_->setText(tr("AI ANALYSIS"));
    if (key_points_title_)        key_points_title_->setText(tr("KEY POINTS"));
    if (risk_title_)              risk_title_->setText(tr("RISK SIGNALS"));
    if (topics_title_)            topics_title_->setText(tr("TOPICS"));
    if (ai_entities_title_)       ai_entities_title_->setText(tr("ENTITIES"));
    if (monitor_title_)           monitor_title_->setText(tr("MONITOR MATCHES"));
    if (related_title_)           related_title_->setText(tr("RELATED"));
    if (entities_section_title_)  entities_section_title_->setText(tr("ENTITIES"));
    if (infra_title_)             infra_title_->setText(tr("NEARBY INFRASTRUCTURE"));

    // Always-static action buttons (tooltips + labels).
    if (open_btn_)        open_btn_->setText(tr("OPEN"));
    if (copy_btn_)        copy_btn_->setText(tr("COPY URL"));
    if (copy_title_btn_) {
        copy_title_btn_->setText(tr("COPY TITLE"));
        copy_title_btn_->setToolTip(tr("Copy article headline to clipboard"));
    }
    if (save_btn_) {
        save_btn_->setText(tr("SAVE"));
        save_btn_->setToolTip(tr("Save article to File Manager"));
    }
    if (translate_btn_)   translate_btn_->setText(tr("TRANSLATE"));
    if (bookmark_btn_)    bookmark_btn_->setToolTip(tr("Bookmark article"));
    // analyze_btn_ / bookmark_btn_ labels are state-dependent and refresh when
    // the next article is shown — intentionally not forced here. Per-row dynamic
    // content (badges, metrics, entities) re-renders from live data.
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
