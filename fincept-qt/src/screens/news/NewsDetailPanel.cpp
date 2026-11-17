#include "screens/news/NewsDetailPanel.h"

#include "auth/AuthTypes.h"
#include "core/i18n/LanguageManager.h"
#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"
#include "screens/news/NewsFeedPanel.h"
#include "services/file_manager/FileManagerService.h"
#include "storage/repositories/NewsArticleRepository.h"
#include "ui/charts/Gauge.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/DataTable.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/EnterprisePromo.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/StatTile.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFontMetrics>
#include <QCursor>
#include <QGridLayout>
#include <QHeaderView>
#include <QMenu>
#include <QPointer>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QScrollArea>
#include <QSizePolicy>
#include <QTextStream>
#include <QTimer>
#include <QLocale>
#include <QUrl>
#include <QtConcurrent>

#include <cmath>

namespace fincept::screens {

namespace {
// Preferred width of the right-hand detail panel: 420px at the default font
// size, wider when the user's font needs it. The screen's splitter owns the
// actual width (the user can drag it); this is the first-open size.
int panel_width() {
    return std::max(420, QFontMetrics(ui::fonts::font(-3)).horizontalAdvance(QString(66, QLatin1Char('M'))));
}
/// ISO 639-1 code of the UI language ("fr_FR" -> "fr"; Chinese keeps its script
/// variant as Google's translator expects: "zh_CN" -> "zh-CN", "zh_TW" -> "zh-TW").
QString ndp_ui_language() {
    QString code = i18n::LanguageManager::instance().current_language();
    if (code.isEmpty())
        code = QLocale().name();
    if (code.startsWith(QLatin1String("zh")))
        return code.contains(QLatin1String("TW")) || code.contains(QLatin1String("HK")) ? QStringLiteral("zh-TW")
                                                                                         : QStringLiteral("zh-CN");
    const int sep = static_cast<int>(code.indexOf(QRegularExpression(QStringLiteral("[_-]"))));
    return (sep > 0 ? code.left(sep) : code).toLower();
}
/// Below this balance the credit footer turns amber and offers Plans & Pricing.
constexpr double kNewsLowCreditThreshold = 20.0;
/// An ANALYZE still running after this long gets a "still working" notice.
constexpr int kAnalyzeSlowNoticeMs = 30000;

/// The ticker links of the detail panel (rich text; its colour is baked in, so
/// it is rebuilt when the theme changes): each validated tag with its match
/// confidence and an earnings badge, then the unverified look-alikes (no link).
QString ticker_links_html(const services::NewsArticle& a, const QHash<QString, int>& earnings_days) {
    QStringList links;
    links.reserve(a.tickers.size());
    for (int i = 0; i < a.tickers.size(); ++i) {
        const QString& t = a.tickers[i];
        QString item = QString("<a href=\"sym:%1\" style=\"color:%2;text-decoration:none;\">$%3</a>")
                           .arg(QString::fromLatin1(QUrl::toPercentEncoding(t)), ui::colors::WARNING(), t.toHtmlEscaped());
        if (i < a.ticker_confidence.size())
            item += QString("<span style=\"color:%1;\"> %2%</span>")
                        .arg(ui::colors::TEXT_TERTIARY())
                        .arg(std::lround(a.ticker_confidence[i] * 100));
        const auto er = earnings_days.constFind(t.toUpper());
        if (er != earnings_days.constEnd())
            item += QString("<span style=\"color:%1;\"> %2</span>")
                        .arg(ui::colors::INFO(),
                             er.value() == 0 ? NewsDetailPanel::tr("ER today") : NewsDetailPanel::tr("ER %1d").arg(er.value()));
        links << item;
    }
    QString html = links.join("&nbsp;&nbsp;");
    if (!a.unverified_tickers.isEmpty()) {
        html += QString("%1<span style=\"color:%2;\">%3</span>")
                    .arg(html.isEmpty() ? QString() : QStringLiteral("<br>"), ui::colors::TEXT_TERTIARY(),
                         NewsDetailPanel::tr("unverified: %1")
                             .arg(a.unverified_tickers.join(QStringLiteral(", ")).toHtmlEscaped()));
    }
    return html;
}
} // namespace

NewsDetailPanel::NewsDetailPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("newsDetailOverlay");
    // Width belongs to the screen's splitter (draggable); a floor keeps the
    // action grid usable.
    setMinimumWidth(320);
    resize(panel_width(), height());
    hide(); // start hidden — shown on article click

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Panel header with close button
    auto* header = new QWidget(this);
    header->setObjectName("newsDetailHeader");
    header->setMinimumHeight(30);
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
    // The article view (page 1) is built when the first article is shown
    // (ensure_content): the panel starts hidden, and building it with the
    // screen made the first open of the News tab slower for nothing.
    stack_->setCurrentIndex(0);

    root->addWidget(stack_, 1);

    // Analyze progress guard. The request itself may run for up to
    // NewsService::kAnalyzeTimeoutMs (the server fetches the article and runs the
    // model before answering), and the transport always ends it by then with a
    // result or an error. After kAnalyzeSlowNoticeMs the user is told it is still
    // working; only if nothing at all arrived a little after the transport limit
    // is the button re-armed.
    analyze_timeout_ = new QTimer(this);
    analyze_timeout_->setSingleShot(true);
    analyze_timeout_->setInterval(kAnalyzeSlowNoticeMs);
    connect(analyze_timeout_, &QTimer::timeout, this, [this]() {
        if (!analyze_btn_)
            return;
        if (!analyze_slow_notice_shown_) {
            analyze_slow_notice_shown_ = true;
            analyze_btn_->setText(tr("STILL WORKING…"));
            sync_analysis_preview();
            analysis_notice_->setText(tr("Still working — the server is reading the article and running the "
                                         "analysis. This can take up to %1 seconds; the result appears here.")
                                          .arg(services::NewsService::kAnalyzeTimeoutMs / 1000));
            analysis_notice_->show();
            analyze_timeout_->start(services::NewsService::kAnalyzeTimeoutMs - kAnalyzeSlowNoticeMs + 5000);
            return;
        }
        analyze_btn_->setText(tr("ANALYZE"));
        analyze_btn_->setEnabled(true);
        sync_analysis_preview();
        analysis_notice_->setText(tr("No answer from the analysis service. Try again in a moment."));
        analysis_notice_->show();
    });

    // The ticker links are rich text with the theme colour baked in.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this](const ui::ThemeTokens&) {
        if (has_article_)
            tickers_label_->setText(ticker_links_html(current_article_, earnings_days_));
    });
}

QSize NewsDetailPanel::sizeHint() const {
    return {panel_width(), QWidget::sizeHint().height()};
}

void NewsDetailPanel::ensure_content() {
    if (content_built_)
        return;
    content_built_ = true;
    QElapsedTimer clock;
    clock.start();
    stack_->addWidget(build_content_view()); // page 1
    LOG_DEBUG("NewsDetailPanel", QStringLiteral("article view built on first use in %1 ms").arg(clock.elapsed()));
}

void NewsDetailPanel::set_earnings_days(const QHash<QString, int>& days) {
    if (days == earnings_days_)
        return;
    earnings_days_ = days;
    if (has_article_)
        tickers_label_->setText(ticker_links_html(current_article_, earnings_days_));
}

QWidget* NewsDetailPanel::build_empty_state() {
    // The shared designed empty state (the panel is open with no article).
    empty_state_ = new ui::EmptyState(stack_ ? static_cast<QWidget*>(stack_) : this);
    empty_state_->set_compact(true);
    empty_state_->set_glyph(QStringLiteral("☰"), ui::EmptyState::Tone::Info);
    empty_state_->set_title(tr("No article selected"));
    empty_state_->set_message(tr("Pick a story in the feed (J / K move, Enter opens): its summary, tickers, market "
                                 "impact, related coverage and AI analysis appear here."));
    return empty_state_;
}

QWidget* NewsDetailPanel::build_content_view() {
    // Created inside stack_ (its final parent): built under the panel and then
    // moved into the stack, the whole content tree was re-styled a second time
    // by the screen's style sheet while the News tab was being opened (a GUI
    // stall sampled there).
    auto* scroll = new QScrollArea(stack_ ? static_cast<QWidget*>(stack_) : this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget(scroll);
    content->setObjectName("newsDetailContent");
    // The panel's width is set by the screen's splitter; the scroll area (with
    // widgetResizable and no horizontal bar) makes word-wrapped labels wrap
    // against it instead of growing the panel.
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(8);

    // Headline. Feed-supplied text is untrusted: QLabel's default Qt::AutoText
    // renders anything that looks like markup as rich text, so a publisher (or
    // an injected item) could restyle/mangle the panel or hide content behind
    // tags. Every label that carries feed or model text is pinned to PlainText.
    headline_label_ = new QLabel(content);
    headline_label_->setObjectName("newsDetailHeadline");
    headline_label_->setWordWrap(true);
    headline_label_->setTextFormat(Qt::PlainText);
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

    // Summary (untrusted feed text — see the headline note).
    summary_label_ = new QLabel(content);
    summary_label_->setObjectName("newsDetailSummary");
    summary_label_->setWordWrap(true);
    summary_label_->setTextFormat(Qt::PlainText);
    layout->addWidget(summary_label_);

    // Impact
    impact_label_ = new QLabel(content);
    impact_label_->setObjectName("newsDetailImpact");
    layout->addWidget(impact_label_);

    // Tickers — each one is a link that opens the shared ticker menu (Equity
    // Research / Equity Trading / Backtesting / watchlist / filter). Tickers are
    // A-Z tokens extracted by the parser, but the label is still built from
    // escaped / percent-encoded parts.
    tickers_label_ = new QLabel(content);
    tickers_label_->setObjectName("newsDetailTickers");
    tickers_label_->setTextFormat(Qt::RichText);
    tickers_label_->setTextInteractionFlags(Qt::TextBrowserInteraction);
    tickers_label_->setOpenExternalLinks(false);
    tickers_label_->setWordWrap(true);
    tickers_label_->setToolTip(tr("Click a ticker for research, chart, backtest and watchlist actions"));
    connect(tickers_label_, &QLabel::linkActivated, this, [this](const QString& link) {
        // Only ever act on our own scheme.
        if (!link.startsWith(QLatin1String("sym:")))
            return;
        const QString symbol = QUrl::fromPercentEncoding(link.mid(4).toUtf8());
        if (symbol.isEmpty())
            return;
        QMenu menu(this);
        QPointer<NewsDetailPanel> self = this;
        NewsFeedPanel::add_ticker_actions(&menu, symbol, this, [self](const QString& t) {
            if (self)
                emit self->ticker_filter_requested(t);
        });
        menu.exec(QCursor::pos());
    });
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
    analyze_btn_->setProperty("fincept_agent_risk", "spend"); // agents: a Fincept AI analysis (credits)
    analyze_btn_->setObjectName("newsDetailAnalyzeBtn");
    analyze_btn_->setToolTip(tr("Fincept AI analysis of the full article — uses Fincept API credits "
                                "(the cost and remaining balance are shown after each run)"));
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
        b->setMinimumHeight(24);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    QPushButton* action_btns[] = {open_btn_, copy_btn_,     copy_title_btn_, analyze_btn_,
                                  save_btn_, bookmark_btn_, translate_btn_};
    constexpr int kCols = 3;
    for (int i = 0; i < 7; ++i)
        action_layout->addWidget(action_btns[i], i / kCols, i % kCols);
    for (int c = 0; c < kCols; ++c)
        action_layout->setColumnStretch(c, 1);
    layout->addWidget(actions);

    // Plans & Pricing shortcut — only shown when the analysis credits ran out / run low.
    plans_btn_ = new QPushButton(content);
    plans_btn_->setObjectName("newsDetailOpenBtn");
    plans_btn_->setMinimumHeight(24);
    plans_btn_->setCursor(Qt::PointingHandCursor);
    plans_btn_->hide();
    connect(plans_btn_, &QPushButton::clicked, this, [this]() { ui::billing::open_plans(this); });
    layout->addWidget(plans_btn_);

    // Why ANALYZE produced no analysis (styled like the fetch-note banner).
    analysis_notice_ = new QLabel(content);
    analysis_notice_->setObjectName("newsDetailAiFetchNote");
    analysis_notice_->setWordWrap(true);
    analysis_notice_->setTextFormat(Qt::PlainText); // carries server text
    analysis_notice_->hide();
    layout->addWidget(analysis_notice_);

    connect(open_btn_, &QPushButton::clicked, this, [this]() {
        if (!has_article_)
            return;
        // Feed links are untrusted — never hand a non-web scheme to the shell.
        const QUrl url(current_article_.link);
        if (url.isValid() && (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https")))
            QDesktopServices::openUrl(url);
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
        QTimer::singleShot(1000, this, [this]() { copy_title_btn_->setText(tr("COPY TITLE")); });
    });
    connect(analyze_btn_, &QPushButton::clicked, this, [this]() {
        if (!has_article_)
            return;
        analyze_btn_->setText(tr("ANALYZING..."));
        analyze_btn_->setEnabled(false);
        sync_analysis_preview();
        analysis_notice_->hide();
        analyze_slow_notice_shown_ = false;
        analyze_timeout_->start(kAnalyzeSlowNoticeMs);
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
        if (safe.isEmpty()) // non-Latin headlines are stripped to nothing above
            safe = QStringLiteral("article");
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
            save_btn_->setText(tr("SAVED"));
        } else {
            LOG_WARN("News", "Could not save article to " + dest + ": " + f.errorString());
            save_btn_->setText(tr("FAILED"));
        }
        // Brief confirmation, then revert (parented to the button so it dies with it).
        QTimer::singleShot(1500, save_btn_, [this]() { save_btn_->setText(tr("SAVE")); });
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
        // The service callback is not lifetime-bound to this widget, so a raw
        // [this] capture would dangle if the panel is destroyed while the
        // translation is in flight (P8). Guard with QPointer, and pin the
        // article id so a late result can't overwrite a different article.
        QPointer<NewsDetailPanel> self = this;
        const QString for_article = current_article_.id;
        const QString source_text = current_article_.headline + "\n\n" + current_article_.summary;
        // Into the language the terminal is displayed in (English when it is English).
        const QString target = ndp_ui_language();
        services::NewsNlpService::instance().translate_text(
            source_text, target,
            [self, for_article, source_text, target](bool ok, QString translated, QString detected_lang) {
                if (!self)
                    return;
                self->translate_btn_->setEnabled(true);
                if (self->current_article_.id != for_article) {
                    self->translate_btn_->setText(self->tr("TRANSLATE"));
                    return; // user moved on to another article
                }
                // Say what happened instead of silently resetting the button.
                QString result_label = self->tr("TRANSLATE");
                if (!ok || translated.isEmpty()) {
                    result_label = self->tr("FAILED");
                } else if (translated.trimmed() == source_text.trimmed()) {
                    result_label = self->tr("ALREADY %1").arg(target.toUpper()); // nothing to translate
                } else {
                    self->summary_label_->setText(
                        self->tr("[%1 -> %2] %3").arg(detected_lang.toUpper(), target.toUpper(), translated));
                }
                self->translate_btn_->setText(result_label);
                if (result_label != self->tr("TRANSLATE")) {
                    QPointer<NewsDetailPanel> guard = self;
                    QTimer::singleShot(1500, self->translate_btn_, [guard]() {
                        if (guard)
                            guard->translate_btn_->setText(guard->tr("TRANSLATE"));
                    });
                }
            });
    });

    // Separator
    auto* sep = new QWidget(content);
    sep->setFixedHeight(1);
    sep->setObjectName("newsDetailSep");
    layout->addWidget(sep);

    // Where the AI analysis goes until one exists for this article: what it
    // returns and that it costs credits — it is never run automatically (a
    // saved analysis of the article is shown instead, free).
    analysis_preview_ = new ui::EmptyState(content);
    analysis_preview_->set_compact(true);
    analysis_preview_->set_alignment(Qt::AlignTop);
    analysis_preview_->set_glyph(QStringLiteral("◆"), ui::EmptyState::Tone::Accent);
    analysis_preview_->set_title(tr("AI analysis of this article"));
    analysis_preview_->set_message(tr("Sentiment and confidence, urgency and outlook, the key points, risk signals and "
                                      "the companies, people and places in the full article."));
    analysis_preview_->set_note(tr("Uses Fincept credits — the cost and your balance are shown after the run"),
                                ui::EmptyState::Tone::Warning);
    analysis_preview_->set_primary_action(tr("ANALYZE"));
    connect(analysis_preview_, &ui::EmptyState::primary_clicked, this, [this]() {
        if (analyze_btn_ && analyze_btn_->isEnabled())
            analyze_btn_->click();
    });
    layout->addWidget(analysis_preview_);

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

    // Headline KPIs: sentiment, confidence, urgency, outlook (two per row in
    // the fixed-width panel), then the sentiment score on a -1 … +1 gauge.
    ai_kpis_ = new ui::KpiStrip(analysis_section_);
    ai_kpis_->set_max_columns(2);
    ai_kpis_->set_spacing(4);
    const std::pair<const char*, QString> kpi_tiles[] = {{"sentiment", tr("SENTIMENT")},
                                                         {"confidence", tr("CONFIDENCE")},
                                                         {"urgency", tr("URGENCY")},
                                                         {"outlook", tr("OUTLOOK")}};
    for (const auto& [key, label] : kpi_tiles)
        ai_kpis_->add_tile(QString::fromLatin1(key), label)->set_compact(true);
    analysis_layout->addWidget(ai_kpis_);

    ai_gauge_ = new ui::Gauge(analysis_section_);
    ai_gauge_->set_style(ui::Gauge::Style::Linear);
    ai_gauge_->set_range(-1.0, 1.0);
    ai_gauge_->set_zones({{-1.0, -0.1, ui::GaugeZone::Tone::Down, tr("Negative")},
                          {-0.1, 0.1, ui::GaugeZone::Tone::Neutral, tr("Neutral")},
                          {0.1, 1.0, ui::GaugeZone::Tone::Up, tr("Positive")}});
    ai_gauge_->set_value_formatter([](double v) { return ui::formatting::signed_number(v, 2); });
    ai_gauge_->setMinimumHeight(52);
    analysis_layout->addWidget(ai_gauge_);

    ai_summary_ = new QLabel(analysis_section_);
    ai_summary_->setObjectName("newsDetailAiSummary");
    ai_summary_->setWordWrap(true);
    ai_summary_->setTextFormat(Qt::PlainText); // model output is untrusted text
    analysis_layout->addWidget(ai_summary_);

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
    ai_credits_->setObjectName("newsDetailAiCredits"); // colours: rules in the news screen sheet
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
    ensure_content();
    current_article_ = article;
    has_article_ = true;
    stack_->setCurrentIndex(1);
    open_panel();

    headline_label_->setText(article.headline);

    // Priority badge
    QString pstr = services::priority_string(article.priority);
    // A chip tinted with the priority colour, its text in that colour.
    const QColor pcolor(services::priority_color(article.priority));
    priority_badge_->setText(pstr);
    priority_badge_->setStyleSheet(QString("background: %1; color: %2; border: 1px solid %2; padding: 1px 6px;")
                                       .arg(ui::colors::tint(pcolor, 0.22), ui::colors::legible(pcolor)));

    // Sentiment badge — bullish / bearish is market direction. Articles in a
    // language without a tone model say so instead of reading NEUTRAL.
    if (!article.sentiment_scored) {
        sentiment_badge_->setText(tr("TONE NOT SCORED"));
        sentiment_badge_->setToolTip(
            article.lang.isEmpty() || article.lang == QLatin1String("en")
                ? tr("No tone is stored for this article (it was saved before tone scores were kept).")
                : tr("Tone is measured for English text only; this article is in %1. Use TRANSLATE to read it.")
                      .arg(article.lang.toUpper()));
        sentiment_badge_->setStyleSheet(
            QString("color: %1; padding: 1px 4px;").arg(ui::colors::TEXT_TERTIARY()));
    } else {
        QString sstr = services::sentiment_string(article.sentiment);
        const QString scolor = article.sentiment == services::Sentiment::BULLISH   ? ui::colors::POSITIVE()
                               : article.sentiment == services::Sentiment::BEARISH ? ui::colors::NEGATIVE()
                                                                                   : ui::colors::TEXT_SECONDARY();
        // The label alone hid how strong the tone is (+0.16 and +0.93 both read BULLISH).
        sstr += QStringLiteral(" %1%2").arg(article.sentiment_score > 0 ? "+" : "").arg(article.sentiment_score, 0, 'f', 2);
        sentiment_badge_->setText(sstr);
        sentiment_badge_->setToolTip(
            tr("Headline tone, finance-tuned VADER: -1 bearish … +1 bullish (±0.15 = neutral)"));
        sentiment_badge_->setStyleSheet(QString("color: %1; padding: 1px 4px;").arg(scolor));
    }

    // Tier badge
    QStringList tier_names = {"", "WIRE", "MAJOR", "SPECIALTY", "BLOG"};
    QString tier_text = (article.tier >= 1 && article.tier <= 4) ? tier_names[article.tier] : "OTHER";
    tier_badge_->setText(tier_text);

    category_label_->setText(article.category);
    // Absolute time in the display zone, with the zone and how long ago.
    {
        namespace fmt = ui::formatting;
        const QDateTime dt = QDateTime::fromSecsSinceEpoch(article.sort_ts);
        QStringList parts;
        // An undated item's time is only when we first saw it — not an age.
        if (article.undated)
            parts << tr("Undated");
        else
            parts << fmt::date_time(dt) + QLatin1Char(' ') + fmt::zone_label(dt)
                  << fmt::relative(static_cast<qint64>(article.sort_ts));
        if (!article.lang.isEmpty())
            parts << article.lang.toUpper();
        if (!article.region.isEmpty())
            parts << article.region;
        time_label_->setText(parts.join(QStringLiteral(" · ")));
        time_label_->setToolTip(article.undated
                                    ? tr("The feed gives no publication time for this item; first seen %1.")
                                          .arg(fmt::date_time(dt, fmt::DateStyle::Long, true))
                                    : tr("Published %1 (UTC %2)")
                                          .arg(fmt::date_time(dt, fmt::DateStyle::Long, true),
                                               dt.toUTC().toString(Qt::ISODate)));
    }

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

    // Market impact (what the event can do to prices) and threat (security /
    // physical / systemic risk) are separate judgements, shown separately.
    {
        QString text = tr("Market impact: %1").arg(services::impact_string(article.impact));
        if (!article.event_type.isEmpty())
            text += QStringLiteral(" · ") + article.event_type;
        if (article.threat.level != services::ThreatLevel::INFO) {
            text += QStringLiteral("\n") + tr("Threat: %1 (%2, %3% conf)")
                                               .arg(services::threat_level_string(article.threat.level),
                                                    article.threat.category)
                                               .arg(static_cast<int>(article.threat.confidence * 100));
            const QString threat_color =
                ui::colors::legible(QColor(services::threat_level_color(article.threat.level)));
            impact_label_->setStyleSheet(QString("color: %1; background: transparent;").arg(threat_color));
        } else {
            impact_label_->setStyleSheet(QString("color: %1; background: transparent;")
                                             .arg(article.impact == services::Impact::HIGH ? ui::colors::AMBER()
                                                                                            : ui::colors::TEXT_SECONDARY()));
        }
        impact_label_->setText(text);
        impact_label_->setToolTip(tr("Market impact: the kind of event (rate decision, earnings, M&A, rating "
                                     "change …), the source's tier and the strength of the tone.\nThreat: conflict, "
                                     "cyber, natural disasters, sanctions and market-wide breakdowns only."));
    }

    tickers_label_->setText(ticker_links_html(article, earnings_days_));
    tickers_label_->setVisible(!article.tickers.isEmpty() || !article.unverified_tickers.isEmpty());

    // Reset analysis
    analysis_section_->hide();
    analyze_btn_->setText(tr("ANALYZE"));
    // Link-less items (the parser drops non-web links) have nothing to open,
    // copy or send to the analysis endpoint.
    const bool has_link = !article.link.isEmpty();
    analyze_btn_->setEnabled(has_link);
    analysis_preview_->show();
    sync_analysis_preview();
    // A previous article's failure reason must not stay on the button.
    analyze_btn_->setToolTip(tr("Fincept AI analysis of the full article — uses Fincept API credits "
                                "(the cost and remaining balance are shown after each run)"));
    plans_btn_->hide();
    analysis_notice_->hide();
    open_btn_->setEnabled(has_link);
    copy_btn_->setEnabled(has_link);
    analyze_timeout_->stop();

    // Reflect saved state from DB. load_saved() is a SQLite read; running it
    // synchronously here stuttered the UI on every article click (P1). Run it
    // on a worker and post the answer back, guarded so a late result can't
    // stamp the wrong article's bookmark state (P8).
    bookmark_btn_->setChecked(false);
    bookmark_btn_->setText(tr("BOOKMARK"));
    refresh_bookmark_state();

    // Clear related, monitors and the previous article's NER / infrastructure
    // (the caller re-shows them when this article has its own).
    show_related({});
    monitor_section_->hide();
    entities_section_->hide();
    infra_section_->hide();
}

void NewsDetailPanel::refresh_bookmark_state() {
    if (!has_article_)
        return;
    QPointer<NewsDetailPanel> self = this;
    const QString article_id = current_article_.id;
    (void)QtConcurrent::run([self, article_id]() {
        // One primary-key lookup (this used to load every bookmarked row).
        auto r = fincept::NewsArticleRepository::instance().is_saved(article_id);
        const bool is_saved = r.is_ok() && r.value();
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, article_id, is_saved]() {
                if (!self || self->current_article_.id != article_id)
                    return;
                self->bookmark_btn_->setChecked(is_saved);
                self->bookmark_btn_->setText(is_saved ? self->tr("BOOKMARKED") : self->tr("BOOKMARK"));
            },
            Qt::QueuedConnection);
    });
}

void NewsDetailPanel::show_analysis_failed(const QString& error) {
    ensure_content();
    // Every branch below ends with the preview's action mirroring the button.
    const auto sync_preview = qScopeGuard([this]() { sync_analysis_preview(); });
    analyze_timeout_->stop();
    analyze_btn_->setEnabled(true);
    analysis_notice_->hide(); // a "still working" notice; the branches below set their own
    const int status = HttpClient::status_from_error(error.toStdString());
    // Out of credits: retrying cannot help — say so and offer the plans.
    if (status == 402) {
        analyze_btn_->setText(tr("NO CREDITS"));
        analyze_btn_->setToolTip(HttpClient::message_from_error(error.toStdString()));
        plans_btn_->setText(tr("INSUFFICIENT CREDITS — SEE PLANS"));
        plans_btn_->show();
        return;
    }
    // 422 article_content_unavailable (the article page could not be read and no
    // usable feed text went with the request) and 404 (the article is gone): the
    // server analysed nothing and charged nothing. Not a malfunction, so no
    // "FAILED - RETRY"; the server's reason is shown under the explanation.
    if (status == 422 || status == 404) {
        QString server = HttpClient::message_from_error(error.toStdString());
        server.remove(QStringLiteral("No credits were charged.")); // said below already
        server = server.trimmed();
        QString text;
        if (status == 404)
            text = tr("The article could not be found at its address (it may have been moved or removed), so no "
                      "analysis was made — you were not charged.");
        else if (server.startsWith(QLatin1String("body ->"))) // a request field the server refused
            text = tr("The server did not accept this request, so no analysis was made — you were not charged.");
        else
            text = tr("The article text could not be retrieved, so no analysis was made — you were not charged.");
        if (!server.isEmpty())
            text += QLatin1Char('\n') + server;
        analyze_btn_->setText(tr("NOT ANALYZED"));
        analyze_btn_->setToolTip(text);
        analysis_notice_->setText(text);
        analysis_notice_->show();
        return;
    }
    // Busy (429: AI requests already running) or the AI service briefly unavailable.
    if (status == 429 || status == 502 || status == 503 || status == 504) {
        const QString server = HttpClient::message_from_error(error.toStdString()).trimmed();
        const QString text = (server.isEmpty() ? tr("The AI analysis service is busy or temporarily unavailable.")
                                               : server) +
                             QLatin1Char(' ') + tr("This is temporary: try again in a moment.");
        analyze_btn_->setText(tr("BUSY - RETRY"));
        analyze_btn_->setToolTip(text);
        analysis_notice_->setText(text);
        analysis_notice_->show();
        return;
    }
    analyze_btn_->setText(tr("FAILED - RETRY"));
    if (!error.isEmpty())
        analyze_btn_->setToolTip(tr("Analysis failed: %1").arg(error.left(200)));
}

void NewsDetailPanel::sync_analysis_preview() {
    if (!analysis_preview_ || !analyze_btn_)
        return;
    analysis_preview_->set_primary_action(analyze_btn_->text());
    analysis_preview_->set_primary_enabled(analyze_btn_->isEnabled());
}

void NewsDetailPanel::show_analysis(const services::NewsAnalysis& analysis) {
    ensure_content();
    analyze_btn_->setText(tr("ANALYZE"));
    analyze_btn_->setEnabled(true);
    analyze_timeout_->stop();
    analysis_notice_->hide();
    analysis_preview_->hide(); // the analysis itself takes its place

    // What the analysis rests on. Newer servers analyse the feed's headline and
    // summary when the article page cannot be read (analysis_basis "supplied_text"):
    // a short note, the server's full explanation on hover. Older servers' fetch
    // note flags a metadata-only analysis.
    if (analysis.basis == QLatin1String("supplied_text")) {
        ai_fetch_note_->setText(tr("Based on the feed headline and summary — the article page could not be read."));
        ai_fetch_note_->setToolTip(analysis.content.fetch_note);
        ai_fetch_note_->show();
    } else if (!analysis.content.fetch_note.isEmpty()) {
        ai_fetch_note_->setText("⚠  " + analysis.content.fetch_note);
        ai_fetch_note_->setToolTip({});
        ai_fetch_note_->show();
    } else {
        ai_fetch_note_->setToolTip({});
        ai_fetch_note_->hide();
    }

    ai_summary_->setText(analysis.summary.isEmpty() ? tr("No AI summary available.") : analysis.summary);

    // Headline KPIs + gauge.
    using Tone = ui::StatTile::Tone;
    const double score = std::clamp(analysis.sentiment.score, -1.0, 1.0);
    if (auto* t = ai_kpis_->tile(QStringLiteral("sentiment"))) {
        t->set_value(ui::formatting::signed_number(score, 2));
        t->set_tone(score > 0.1 ? Tone::Positive : (score < -0.1 ? Tone::Negative : Tone::Neutral));
        t->set_sub_label(tr("intensity %1").arg(ui::formatting::number(analysis.sentiment.intensity, 2)));
    }
    if (auto* t = ai_kpis_->tile(QStringLiteral("confidence")))
        t->set_value(ui::formatting::fraction_pct(analysis.sentiment.confidence, 0, false));
    const QString urg = analysis.market_impact.urgency.toUpper();
    if (auto* t = ai_kpis_->tile(QStringLiteral("urgency"))) {
        // BREAKING is the server's most urgent level (LOW < MEDIUM < HIGH < BREAKING).
        t->set_value(urg.isEmpty() ? ui::formatting::placeholder() : urg);
        t->set_tone(urg.isEmpty()                                                    ? Tone::Neutral
                    : (urg == "HIGH" || urg == "BREAKING" || urg == "CRITICAL") ? Tone::Negative
                    : urg == "MEDIUM"                                           ? Tone::Warning
                                                                                : Tone::Positive);
    }
    // API sends snake_case (e.g. "moderate_positive"); show it as readable words.
    const QString pred = analysis.market_impact.prediction;
    if (auto* t = ai_kpis_->tile(QStringLiteral("outlook"))) {
        t->set_value(pred.isEmpty() ? tr("neutral") : QString(pred).replace('_', ' '));
        t->set_tone(pred.contains("positive") ? Tone::Positive
                                              : (pred.contains("negative") ? Tone::Negative : Tone::Neutral));
    }
    ai_gauge_->set_value(score);
    ai_gauge_->set_caption(score > 0.1 ? tr("Positive") : (score < -0.1 ? tr("Negative") : tr("Neutral")));

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
        lbl->setTextFormat(Qt::PlainText);
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
            return ui::colors::DANGER;
        if (l == "medium" || l == "moderate")
            return ui::colors::WARNING;
        if (l == "low")
            return ui::colors::SUCCESS;
        return ui::colors::TEXT_TERTIARY; // "none" / unknown
    };
    {
        // Signal | Level | Detail — all four rows, "none" shown muted.
        const QPair<QString, services::RiskSignal> risk_rows[] = {{tr("Regulatory"), analysis.regulatory},
                                                                 {tr("Geopolitical"), analysis.geopolitical},
                                                                 {tr("Operational"), analysis.operational},
                                                                 {tr("Market"), analysis.market}};
        bool any = false;
        for (const auto& sig : risk_rows)
            any = any || !sig.second.level.isEmpty();
        if (any) {
            auto* table = new ui::DataTable(analysis_section_);
            table->set_headers({tr("Signal"), tr("Level"), tr("Detail")});
            table->setWordWrap(true);
            table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
            table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            table->setRowCount(4);
            for (int r = 0; r < 4; ++r) {
                const auto& sig = risk_rows[r].second;
                const QString level = sig.level.isEmpty() ? tr("none") : sig.level;
                auto* name = new QTableWidgetItem(risk_rows[r].first);
                name->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
                auto* lvl = new QTableWidgetItem(level.toUpper());
                lvl->setForeground(QColor(risk_color(level)));
                QFont f = lvl->font();
                f.setBold(true);
                lvl->setFont(f);
                auto* det = new QTableWidgetItem(sig.details);
                det->setToolTip(sig.details);
                det->setForeground(QColor(ui::colors::TEXT_PRIMARY()));
                table->setItem(r, 0, name);
                table->setItem(r, 1, lvl);
                table->setItem(r, 2, det);
            }
            table->resizeColumnToContents(0);
            table->resizeColumnToContents(1);
            table->resizeRowsToContents();
            int h = table->horizontalHeader()->height() + 2 * table->frameWidth();
            for (int r = 0; r < 4; ++r)
                h += table->rowHeight(r);
            table->setFixedHeight(h);
            risk_layout_->addWidget(table);
        }
    }
    // No table when the analysis carries no risk levels at all — hide the
    // heading + container in that case.
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
            text += QString(" (%1)").arg(e.detail); // people: role; locations: country code
        if (!e.sector.isEmpty())
            text += QString(" · %1").arg(e.sector);
        auto* lbl = new QLabel(text, analysis_section_);
        lbl->setObjectName("newsDetailEntity");
        lbl->setWordWrap(true);
        lbl->setTextFormat(Qt::PlainText);
        ai_entities_layout_->addWidget(lbl);
    };
    if (!analysis.organizations.isEmpty()) {
        auto* table = new ui::DataTable(analysis_section_);
        table->set_headers({tr("Organisation"), tr("Ticker"), tr("Sector"), tr("Sentiment")});
        table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        table->setRowCount(analysis.organizations.size());
        for (int r = 0; r < analysis.organizations.size(); ++r) {
            const auto& o = analysis.organizations[r];
            auto* name = new QTableWidgetItem(o.name);
            name->setToolTip(o.name);
            name->setForeground(QColor(ui::colors::TEXT_PRIMARY()));
            auto* ticker = new QTableWidgetItem(o.detail);
            if (!o.detail.isEmpty()) {
                ticker->setForeground(QColor(ui::colors::WARNING()));
                ticker->setToolTip(tr("Click for research, chart, backtest and watchlist actions"));
            }
            auto* sector = new QTableWidgetItem(o.sector);
            sector->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
            auto* sent = new ui::NumericTableWidgetItem(ui::formatting::signed_number(o.sentiment, 2), o.sentiment);
            sent->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            sent->setForeground(o.sentiment > 0   ? QColor(ui::colors::POSITIVE())
                                : o.sentiment < 0 ? QColor(ui::colors::NEGATIVE())
                                                  : QColor(ui::colors::TEXT_SECONDARY()));
            table->setItem(r, 0, name);
            table->setItem(r, 1, ticker);
            table->setItem(r, 2, sector);
            table->setItem(r, 3, sent);
        }
        table->setSortingEnabled(true);
        table->resizeColumnsToContents();
        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        table->setFixedHeight(table->horizontalHeader()->height() + 2 * table->frameWidth() +
                              table->verticalHeader()->defaultSectionSize() *
                                  std::min<int>(static_cast<int>(analysis.organizations.size()), 8));
        // A ticker opens the shared ticker menu, like the article's ticker links.
        connect(table, &QTableWidget::cellClicked, this, [this, table](int row, int col) {
            if (col != 1)
                return;
            auto* it = table->item(row, 1);
            const QString symbol = it ? it->text().trimmed() : QString();
            if (symbol.isEmpty())
                return;
            QMenu menu(this);
            QPointer<NewsDetailPanel> self = this;
            NewsFeedPanel::add_ticker_actions(&menu, symbol, this, [self](const QString& t) {
                if (self)
                    emit self->ticker_filter_requested(t);
            });
            menu.exec(QCursor::pos());
        });
        ai_entities_layout_->addWidget(table);
    }
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

    // Credits footer — fractional balances (one call can cost 0.35 credits), shown
    // only when the backend reported them (negative = not reported).
    if (analysis.credits_used >= 0 || analysis.credits_remaining >= 0) {
        QStringList parts;
        if (analysis.credits_used >= 0)
            parts << tr("Credits used: %1").arg(auth::format_credits(analysis.credits_used));
        if (analysis.credits_remaining >= 0)
            parts << tr("remaining: %1").arg(auth::format_credits(analysis.credits_remaining));
        ai_credits_->setText(parts.join(QStringLiteral("  •  ")));
        const bool low = analysis.credits_remaining >= 0 && analysis.credits_remaining < kNewsLowCreditThreshold;
        ai_credits_->setProperty("low", low);
        ai_credits_->style()->unpolish(ai_credits_);
        ai_credits_->style()->polish(ai_credits_);
        ai_credits_->show();
        if (low) {
            plans_btn_->setText(tr("LOW CREDITS — SEE PLANS"));
            plans_btn_->show();
        } else {
            plans_btn_->hide();
        }
    } else {
        ai_credits_->hide();
    }

    analysis_section_->show();
}

void NewsDetailPanel::show_related(const QVector<services::NewsArticle>& related) {
    ensure_content();
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
        // Strip '&' so a headline containing one doesn't render as an
        // accidental mnemonic underline (and swallow the character).
        btn->setText(QString("%1 - %2").arg(article.source, article.headline.left(55)).replace('&', "&&"));
        btn->setToolTip(article.headline);
        btn->setAccessibleName(tr("Related article: %1").arg(article.headline.left(80)));

        connect(btn, &QPushButton::clicked, this, [this, article]() { emit related_article_clicked(article); });

        related_layout_->addWidget(btn);
    }
}

void NewsDetailPanel::show_monitor_matches(const QVector<QPair<services::NewsMonitor, QStringList>>& matches) {
    ensure_content();
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
        // The monitor's user-chosen colour, shown as a swatch.
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
    ensure_content();
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
    ensure_content();
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
        QString type_icon =
            inf.type == "airport"
                ? tr("AIR")
                : (inf.type == "military" ? tr("MIL") : (inf.type == "power_plant" ? tr("PWR") : tr("PRT")));
        auto* lbl = new QLabel(tr("[%1] %2 — %3 km").arg(type_icon, inf.name.left(20)).arg(inf.distance_km, 0, 'f', 1),
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
    if (header_title_)
        header_title_->setText(tr("ARTICLE DETAIL"));
    if (empty_state_) {
        empty_state_->set_title(tr("No article selected"));
        empty_state_->set_message(tr("Pick a story in the feed (J / K move, Enter opens): its summary, tickers, market "
                                     "impact, related coverage and AI analysis appear here."));
    }
    if (analysis_preview_) {
        analysis_preview_->set_title(tr("AI analysis of this article"));
        analysis_preview_->set_message(tr("Sentiment and confidence, urgency and outlook, the key points, risk signals "
                                          "and the companies, people and places in the full article."));
        analysis_preview_->set_note(tr("Uses Fincept credits — the cost and your balance are shown after the run"),
                                    ui::EmptyState::Tone::Warning);
    }
    if (ai_title_)
        ai_title_->setText(tr("AI ANALYSIS"));
    if (key_points_title_)
        key_points_title_->setText(tr("KEY POINTS"));
    if (risk_title_)
        risk_title_->setText(tr("RISK SIGNALS"));
    if (topics_title_)
        topics_title_->setText(tr("TOPICS"));
    if (ai_entities_title_)
        ai_entities_title_->setText(tr("ENTITIES"));
    if (monitor_title_)
        monitor_title_->setText(tr("MONITOR MATCHES"));
    if (related_title_)
        related_title_->setText(tr("RELATED"));
    if (entities_section_title_)
        entities_section_title_->setText(tr("ENTITIES"));
    if (infra_title_)
        infra_title_->setText(tr("NEARBY INFRASTRUCTURE"));

    // Always-static action buttons (tooltips + labels).
    if (open_btn_)
        open_btn_->setText(tr("OPEN"));
    if (copy_btn_)
        copy_btn_->setText(tr("COPY URL"));
    if (copy_title_btn_) {
        copy_title_btn_->setText(tr("COPY TITLE"));
        copy_title_btn_->setToolTip(tr("Copy article headline to clipboard"));
    }
    if (save_btn_) {
        save_btn_->setText(tr("SAVE"));
        save_btn_->setToolTip(tr("Save article to File Manager"));
    }
    if (translate_btn_)
        translate_btn_->setText(tr("TRANSLATE"));
    if (bookmark_btn_)
        bookmark_btn_->setToolTip(tr("Bookmark article"));
    if (tickers_label_)
        tickers_label_->setToolTip(tr("Click a ticker for research, chart, backtest and watchlist actions"));
    // analyze_btn_ / bookmark_btn_ labels are state-dependent and refresh when
    // the next article is shown — intentionally not forced here. Per-row dynamic
    // content (badges, metrics, entities) re-renders from live data.
}

void NewsDetailPanel::clear() {
    has_article_ = false;
    stack_->setCurrentIndex(0);
    analyze_timeout_->stop();
    if (!content_built_)
        return; // nothing shown yet
    analysis_section_->hide();
    related_section_->hide();
    monitor_section_->hide();
    entities_section_->hide();
    infra_section_->hide();
    analyze_timeout_->stop();
}

} // namespace fincept::screens
