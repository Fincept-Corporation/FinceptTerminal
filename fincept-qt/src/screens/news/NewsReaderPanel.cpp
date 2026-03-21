#include "screens/news/NewsReaderPanel.h"
#include "services/news/NewsService.h"
#include "ui/theme/Theme.h"
#include "core/logging/Logger.h"
#include <QScrollArea>
#include <QFrame>
#include <QPushButton>
#include <QDesktopServices>
#include <QUrl>
#include <QClipboard>
#include <QApplication>
#include <QStackedWidget>
#include <QTimer>

namespace fincept::screens {

using namespace fincept::services;

static QLabel* section_hdr(const QString& text) {
    auto* l = new QLabel(text);
    l->setStyleSheet(
        QString("color:%1;font-size:12px;font-weight:700;letter-spacing:0.5px;"
        "background:transparent;padding:8px 0 4px 0;font-family:'Consolas','Courier New',monospace;")
        .arg(ui::colors::AMBER));
    return l;
}

static QFrame* thin_sep() {
    auto* f = new QFrame;
    f->setFixedHeight(1);
    f->setStyleSheet(QString("background:%1;border:none;").arg(ui::colors::BORDER_DIM));
    return f;
}

static QLabel* badge(const QString& text, const QString& bg, const QString& fg = "#080808") {
    auto* l = new QLabel(text);
    l->setStyleSheet(QString("color:%1;background:%2;font-size:12px;font-weight:700;"
        "padding:1px 6px;font-family:'Consolas','Courier New',monospace;").arg(fg, bg));
    return l;
}

NewsReaderPanel::NewsReaderPanel(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("background:%1;border-left:1px solid %2;")
        .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));

    auto* stack = new QStackedWidget(this);
    auto* root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->addWidget(stack);

    empty_state_ = build_empty_state();
    stack->addWidget(empty_state_);

    // Scrollable article + analysis view
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(
        "QScrollArea{border:none;background:transparent;}"
        "QScrollBar:vertical{width:5px;background:transparent;}"
        "QScrollBar::handle:vertical{background:#222222;}"
        "QScrollBar::handle:vertical:hover{background:#333333;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}");

    auto* content = new QWidget;
    auto* cvl = new QVBoxLayout(content);
    cvl->setContentsMargins(12, 6, 12, 6);
    cvl->setSpacing(0);

    article_view_ = build_article_view();
    cvl->addWidget(article_view_);
    cvl->addWidget(thin_sep());

    analysis_view_ = build_analysis_view();
    analysis_view_->hide();
    cvl->addWidget(analysis_view_);
    cvl->addStretch();

    scroll->setWidget(content);
    stack->addWidget(scroll);
}

QWidget* NewsReaderPanel::build_empty_state() {
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setAlignment(Qt::AlignCenter);
    auto* lbl = new QLabel("SELECT AN ARTICLE");
    lbl->setStyleSheet(QString("color:%1;font-size:13px;font-weight:700;"
        "font-family:'Consolas','Courier New',monospace;background:transparent;")
        .arg(ui::colors::TEXT_TERTIARY));
    lbl->setAlignment(Qt::AlignCenter);
    auto* sub = new QLabel("Click a wire row or top story to read");
    sub->setStyleSheet(QString("color:%1;font-size:12px;font-family:'Consolas','Courier New',monospace;"
        "background:transparent;").arg(ui::colors::TEXT_DIM));
    sub->setAlignment(Qt::AlignCenter);
    vl->addWidget(lbl);
    vl->addWidget(sub);
    return w;
}

QWidget* NewsReaderPanel::build_article_view() {
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(4);

    // Headline
    headline_label_ = new QLabel;
    headline_label_->setStyleSheet(QString("color:%1;font-size:14px;font-weight:700;background:transparent;"
        "font-family:'Consolas','Courier New',monospace;").arg(ui::colors::TEXT_PRIMARY));
    headline_label_->setWordWrap(true);
    vl->addWidget(headline_label_);

    // Badges row
    auto* badges = new QHBoxLayout;
    badges->setSpacing(4);
    priority_badge_ = badge("", ui::colors::TEXT_TERTIARY);
    sentiment_badge_ = badge("", ui::colors::TEXT_TERTIARY);
    category_badge_ = badge("", ui::colors::BG_RAISED, ui::colors::TEXT_SECONDARY);
    badges->addWidget(priority_badge_);
    badges->addWidget(sentiment_badge_);
    badges->addWidget(category_badge_);
    badges->addStretch();
    vl->addLayout(badges);

    // Source + time row
    auto* meta = new QHBoxLayout;
    meta->setSpacing(8);
    source_label_ = new QLabel;
    source_label_->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;background:transparent;"
        "font-family:'Consolas','Courier New',monospace;").arg(ui::colors::CYAN));
    time_label_ = new QLabel;
    time_label_->setStyleSheet(QString("color:%1;font-size:12px;background:transparent;"
        "font-family:'Consolas','Courier New',monospace;").arg(ui::colors::TEXT_DIM));
    meta->addWidget(source_label_);
    meta->addWidget(time_label_);
    meta->addStretch();
    vl->addLayout(meta);

    vl->addWidget(thin_sep());

    // Summary
    vl->addWidget(section_hdr("SUMMARY"));
    summary_label_ = new QLabel;
    summary_label_->setStyleSheet(QString("color:%1;font-size:13px;background:transparent;"
        "font-family:'Consolas','Courier New',monospace;").arg(ui::colors::TEXT_SECONDARY));
    summary_label_->setWordWrap(true);
    vl->addWidget(summary_label_);

    // Impact
    vl->addWidget(section_hdr("IMPACT"));
    impact_label_ = new QLabel;
    impact_label_->setStyleSheet(QString("color:%1;font-size:13px;background:transparent;"
        "font-family:'Consolas','Courier New',monospace;").arg(ui::colors::TEXT_SECONDARY));
    vl->addWidget(impact_label_);

    // Tickers
    tickers_label_ = new QLabel;
    tickers_label_->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;background:transparent;"
        "font-family:'Consolas','Courier New',monospace;").arg(ui::colors::WARNING));
    vl->addWidget(tickers_label_);

    // Action buttons
    auto* actions = new QHBoxLayout;
    actions->setSpacing(4);

    auto* open_btn = new QPushButton("OPEN");
    open_btn->setFixedHeight(22);
    open_btn->setCursor(Qt::PointingHandCursor);
    open_btn->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:1px solid %3;"
        "font-size:12px;font-weight:700;padding:0 10px;font-family:'Consolas','Courier New',monospace;}"
        "QPushButton:hover{color:%4;background:%5;}")
        .arg(ui::colors::BG_RAISED, ui::colors::INFO, ui::colors::BORDER_DIM,
             ui::colors::TEXT_PRIMARY, ui::colors::BG_HOVER));
    connect(open_btn, &QPushButton::clicked, this, [this]() {
        if (has_article_ && !current_article_.link.isEmpty())
            QDesktopServices::openUrl(QUrl(current_article_.link));
    });
    actions->addWidget(open_btn);

    auto* copy_btn = new QPushButton("COPY URL");
    copy_btn->setFixedHeight(22);
    copy_btn->setCursor(Qt::PointingHandCursor);
    copy_btn->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:1px solid %3;"
        "font-size:12px;font-weight:700;padding:0 10px;font-family:'Consolas','Courier New',monospace;}"
        "QPushButton:hover{color:%4;background:%5;}")
        .arg(ui::colors::BG_RAISED, ui::colors::TEXT_SECONDARY, ui::colors::BORDER_DIM,
             ui::colors::TEXT_PRIMARY, ui::colors::BG_HOVER));
    connect(copy_btn, &QPushButton::clicked, this, [this]() {
        if (has_article_ && !current_article_.link.isEmpty())
            QApplication::clipboard()->setText(current_article_.link);
    });
    actions->addWidget(copy_btn);

    auto* ai_btn = new QPushButton("AI ANALYZE");
    ai_btn->setFixedHeight(22);
    ai_btn->setCursor(Qt::PointingHandCursor);
    ai_btn->setStyleSheet(
        QString("QPushButton{background:rgba(217,119,6,0.1);color:%1;border:1px solid %2;"
        "font-size:12px;font-weight:700;padding:0 10px;font-family:'Consolas','Courier New',monospace;}"
        "QPushButton:hover{background:%3;color:#080808;}")
        .arg(ui::colors::AMBER, ui::colors::AMBER_DIM, ui::colors::AMBER));
    connect(ai_btn, &QPushButton::clicked, this, [this, ai_btn]() {
        if (!has_article_) return;
        if (current_article_.link.isEmpty()) {
            LOG_WARN("NewsReader", "No article link available for analysis");
            ai_btn->setText("NO LINK");
            QTimer::singleShot(2000, ai_btn, [ai_btn]() { ai_btn->setText("AI ANALYZE"); });
            return;
        }
        ai_btn->setText("ANALYZING...");
        ai_btn->setEnabled(false);
        LOG_INFO("NewsReader", "Analyzing: " + current_article_.link);
        NewsService::instance().analyze_article(current_article_.link,
            [this, ai_btn](bool ok, NewsAnalysis analysis) {
                ai_btn->setEnabled(true);
                if (ok) {
                    ai_btn->setText("AI ANALYZE");
                    show_analysis(analysis);
                } else {
                    LOG_ERROR("NewsReader", "Analysis request failed");
                    ai_btn->setText("FAILED");
                    QTimer::singleShot(2000, ai_btn, [ai_btn]() { ai_btn->setText("AI ANALYZE"); });
                }
            });
    });
    actions->addWidget(ai_btn);
    actions->addStretch();

    vl->addSpacing(6);
    vl->addLayout(actions);

    return w;
}

QWidget* NewsReaderPanel::build_analysis_view() {
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 4, 0, 0);
    vl->setSpacing(4);

    vl->addWidget(section_hdr("AI ANALYSIS"));

    ai_summary_label_ = new QLabel;
    ai_summary_label_->setStyleSheet(QString("color:%1;font-size:13px;background:transparent;"
        "font-family:'Consolas','Courier New',monospace;").arg(ui::colors::TEXT_SECONDARY));
    ai_summary_label_->setWordWrap(true);
    vl->addWidget(ai_summary_label_);

    auto* row = new QHBoxLayout;
    row->setSpacing(8);
    ai_sentiment_label_ = new QLabel;
    ai_sentiment_label_->setStyleSheet(QString("color:%1;font-size:13px;font-weight:700;background:transparent;"
        "font-family:'Consolas','Courier New',monospace;").arg(ui::colors::TEXT_PRIMARY));
    ai_urgency_label_ = new QLabel;
    ai_urgency_label_->setStyleSheet(QString("color:%1;font-size:13px;font-weight:700;background:transparent;"
        "font-family:'Consolas','Courier New',monospace;").arg(ui::colors::TEXT_PRIMARY));
    row->addWidget(ai_sentiment_label_);
    row->addWidget(ai_urgency_label_);
    row->addStretch();
    vl->addLayout(row);

    vl->addWidget(section_hdr("KEY POINTS"));
    key_points_layout_ = new QVBoxLayout;
    key_points_layout_->setContentsMargins(0, 0, 0, 0);
    key_points_layout_->setSpacing(2);
    vl->addLayout(key_points_layout_);

    vl->addWidget(section_hdr("RISK SIGNALS"));
    risk_layout_ = new QVBoxLayout;
    risk_layout_->setContentsMargins(0, 0, 0, 0);
    risk_layout_->setSpacing(2);
    vl->addLayout(risk_layout_);

    vl->addWidget(section_hdr("TOPICS"));
    topics_layout_ = new QVBoxLayout;
    topics_layout_->setContentsMargins(0, 0, 0, 0);
    topics_layout_->setSpacing(0);
    vl->addLayout(topics_layout_);

    vl->addWidget(section_hdr("KEYWORDS"));
    keywords_layout_ = new QVBoxLayout;
    keywords_layout_->setContentsMargins(0, 0, 0, 0);
    keywords_layout_->setSpacing(0);
    vl->addLayout(keywords_layout_);

    return w;
}

void NewsReaderPanel::show_article(const NewsArticle& article) {
    current_article_ = article;
    has_article_ = true;

    auto* stack = qobject_cast<QStackedWidget*>(layout()->itemAt(0)->widget());
    if (stack) stack->setCurrentIndex(1);

    update_article_view(article);
    analysis_view_->hide();
}

void NewsReaderPanel::update_article_view(const NewsArticle& article) {
    headline_label_->setText(article.headline);

    auto pc = priority_color(article.priority);
    priority_badge_->setText(priority_string(article.priority));
    priority_badge_->setStyleSheet(QString("color:#080808;background:%1;font-size:12px;font-weight:700;"
        "padding:1px 6px;font-family:'Consolas','Courier New',monospace;").arg(pc));

    auto sc = sentiment_color(article.sentiment);
    sentiment_badge_->setText(sentiment_string(article.sentiment));
    sentiment_badge_->setStyleSheet(QString("color:#080808;background:%1;font-size:12px;font-weight:700;"
        "padding:1px 6px;font-family:'Consolas','Courier New',monospace;").arg(sc));

    category_badge_->setText(article.category);

    source_label_->setText(article.source);
    time_label_->setText(relative_time(article.sort_ts));
    summary_label_->setText(article.summary.isEmpty() ? "No summary available." : article.summary);
    impact_label_->setText(impact_string(article.impact));

    if (!article.tickers.isEmpty())
        tickers_label_->setText("$" + article.tickers.join("  $"));
    else
        tickers_label_->clear();
}

void NewsReaderPanel::show_analysis(const NewsAnalysis& analysis) {
    update_analysis_view(analysis);
    analysis_view_->show();
}

void NewsReaderPanel::update_analysis_view(const NewsAnalysis& analysis) {
    ai_summary_label_->setText(analysis.summary);

    QString sent_text = QString("Sentiment: %1").arg(analysis.sentiment.score, 0, 'f', 2);
    ai_sentiment_label_->setText(sent_text);
    ai_sentiment_label_->setStyleSheet(QString("color:%1;font-size:13px;font-weight:700;background:transparent;"
        "font-family:'Consolas','Courier New',monospace;")
        .arg(analysis.sentiment.score > 0 ? ui::colors::POSITIVE
           : analysis.sentiment.score < 0 ? ui::colors::NEGATIVE
           : ui::colors::WARNING));

    ai_urgency_label_->setText("Urgency: " + analysis.market_impact.urgency);

    auto clear_layout = [](QVBoxLayout* layout) {
        while (layout->count() > 0) {
            auto* item = layout->takeAt(0);
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
    };

    clear_layout(key_points_layout_);
    for (const auto& pt : analysis.key_points) {
        auto* l = new QLabel("\xe2\x80\xa2 " + pt);
        l->setStyleSheet(QString("color:%1;font-size:12px;background:transparent;"
            "font-family:'Consolas','Courier New',monospace;").arg(ui::colors::TEXT_SECONDARY));
        l->setWordWrap(true);
        key_points_layout_->addWidget(l);
    }

    clear_layout(risk_layout_);
    auto add_risk = [this](const QString& name, const RiskSignal& rs) {
        if (rs.level.isEmpty() && rs.details.isEmpty()) return;
        auto* l = new QLabel(QString("%1: %2 — %3").arg(name, rs.level, rs.details));
        QString color = rs.level.contains("HIGH", Qt::CaseInsensitive) ? ui::colors::NEGATIVE
                      : rs.level.contains("MEDIUM", Qt::CaseInsensitive) ? ui::colors::WARNING
                      : ui::colors::TEXT_TERTIARY;
        l->setStyleSheet(QString("color:%1;font-size:12px;background:transparent;"
            "font-family:'Consolas','Courier New',monospace;").arg(color));
        l->setWordWrap(true);
        risk_layout_->addWidget(l);
    };
    add_risk("Regulatory", analysis.regulatory);
    add_risk("Geopolitical", analysis.geopolitical);
    add_risk("Operational", analysis.operational);
    add_risk("Market", analysis.market);

    clear_layout(topics_layout_);
    if (!analysis.topics.isEmpty()) {
        auto* flow = new QHBoxLayout;
        flow->setSpacing(4);
        for (const auto& t : analysis.topics) {
            auto* l = badge(t, ui::colors::BG_RAISED, ui::colors::TEXT_SECONDARY);
            flow->addWidget(l);
        }
        flow->addStretch();
        topics_layout_->addLayout(flow);
    }

    clear_layout(keywords_layout_);
    if (!analysis.keywords.isEmpty()) {
        auto* flow = new QHBoxLayout;
        flow->setSpacing(4);
        for (const auto& kw : analysis.keywords) {
            auto* l = badge(kw, ui::colors::BG_SURFACE, ui::colors::WARNING);
            flow->addWidget(l);
        }
        flow->addStretch();
        keywords_layout_->addLayout(flow);
    }
}

} // namespace fincept::screens
