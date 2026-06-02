#include "screens/news/NewsCommandBar.h"

#include <QScrollArea>
#include <QStyle>

namespace fincept::screens {

NewsCommandBar::NewsCommandBar(QWidget* parent) : QWidget(parent) {
    setObjectName("newsCommandBar");
    setFixedHeight(60); // 32px command row + 28px intel strip

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    build_command_row(root);
    build_intel_row(root);

    // Summary display label (hidden, shown below command bar)
    summary_label_ = new QLabel(this);
    summary_label_->setObjectName("newsDetailAiSummary");
    summary_label_->setWordWrap(true);
    summary_label_->hide();
}

void NewsCommandBar::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void NewsCommandBar::retranslateUi() {
    // Command row — fixed-label buttons / inputs.
    if (drawer_btn_)    drawer_btn_->setText(tr("INTEL"));
    if (drawer_btn_)    drawer_btn_->setToolTip(tr("Toggle intelligence drawer"));
    if (search_input_)  search_input_->setPlaceholderText(tr("Search..."));
    if (sources_btn_)   sources_btn_->setToolTip(tr("Manage RSS feed sources"));
    if (summarize_btn_) summarize_btn_->setToolTip(tr("AI Brief — summarize headlines"));
    // Pills (category/time/sort/view) and combo entries carry logical code
    // values used in filter logic — intentionally not retranslated.

    // RTL toggle reflects its current checked state.
    if (rtl_btn_)
        rtl_btn_->setText(rtl_btn_->isChecked() ? tr("LTR") : tr("RTL"));

    // Refresh-cadence combo entries (display strings; data is the int).
    if (refresh_combo_) {
        refresh_combo_->setToolTip(tr("Auto-refresh interval"));
        const QStringList labels = {tr("MANUAL"), tr("1 MIN"), tr("5 MIN"), tr("10 MIN"), tr("30 MIN")};
        for (int i = 0; i < refresh_combo_->count() && i < labels.size(); ++i)
            refresh_combo_->setItemText(i, labels[i]);
    }

    // Intel strip — fixed captions.
    if (intel_feeds_lbl_)    intel_feeds_lbl_->setText(tr("FEEDS"));
    if (intel_articles_lbl_) intel_articles_lbl_->setText(tr("ARTS"));
    if (intel_clusters_lbl_) intel_clusters_lbl_->setText(tr("CLST"));
    if (intel_sources_lbl_)  intel_sources_lbl_->setText(tr("SRCS"));
    if (sentiment_caption_)  sentiment_caption_->setText(tr("SENT"));

    // Dynamic count labels (alerts / unseen / monitors / live badge) reflect
    // runtime counts and refresh on the next data update — not re-applied here.
}

void NewsCommandBar::build_command_row(QVBoxLayout* root) {
    // Wrap the command row in a scroll area to prevent overflow
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName("newsCommandRowScroll");
    scroll->setFixedHeight(32);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* row = new QWidget(scroll);
    row->setObjectName("newsCommandRow");
    row->setFixedHeight(32);

    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(6, 0, 6, 0);
    hl->setSpacing(2);

    // Drawer toggle button
    drawer_btn_ = new QPushButton(tr("INTEL"), row);
    drawer_btn_->setObjectName("newsDrawerBtn");
    drawer_btn_->setFixedHeight(20);
    drawer_btn_->setCursor(Qt::PointingHandCursor);
    drawer_btn_->setCheckable(true);
    drawer_btn_->setToolTip(tr("Toggle intelligence drawer"));
    hl->addWidget(drawer_btn_);
    connect(drawer_btn_, &QPushButton::clicked, this, &NewsCommandBar::drawer_toggle_requested);

    hl->addSpacing(4);

    // Search input — compact
    search_input_ = new QLineEdit(row);
    search_input_->setObjectName("newsCommandBarSearch");
    search_input_->setPlaceholderText(tr("Search..."));
    search_input_->setFixedWidth(120);
    search_input_->setFixedHeight(20);
    hl->addWidget(search_input_);

    connect(search_input_, &QLineEdit::textChanged, this, [this](const QString& text) { emit search_changed(text); });

    hl->addSpacing(4);

    // Category pills — compact
    QStringList categories = {"ALL", "MKT", "EARN", "ECO", "TECH", "NRG", "CRPT", "GEO", "DEF"};
    for (const auto& cat : categories) {
        auto* btn = make_pill(cat, cat, hl);
        category_btns_.append(btn);
        connect(btn, &QPushButton::clicked, this, [this, cat]() {
            active_category_ = cat;
            update_pill_group(category_btns_, active_category_);
            emit category_changed(cat);
        });
    }
    update_pill_group(category_btns_, active_category_);

    hl->addSpacing(4);

    // Separator
    auto* sep1 = new QLabel("|", row);
    sep1->setObjectName("newsCommandBarSep");
    hl->addWidget(sep1);

    hl->addSpacing(2);

    // Time range pills
    QStringList time_ranges = {"1H", "6H", "24H", "48H", "7D", "30D"};
    for (const auto& tr : time_ranges) {
        auto* btn = make_pill(tr, tr, hl);
        time_btns_.append(btn);
        connect(btn, &QPushButton::clicked, this, [this, tr]() {
            active_time_ = tr;
            update_pill_group(time_btns_, active_time_);
            emit time_range_changed(tr);
        });
    }
    update_pill_group(time_btns_, active_time_);

    hl->addSpacing(4);

    // Sort toggle
    sort_relevance_ = make_pill("REL", "RELEVANCE", hl);
    sort_newest_ = make_pill("NEW", "NEWEST", hl);
    connect(sort_relevance_, &QPushButton::clicked, this, [this]() {
        active_sort_ = "RELEVANCE";
        update_pill_group({sort_relevance_, sort_newest_}, active_sort_);
        emit sort_changed("RELEVANCE");
    });
    connect(sort_newest_, &QPushButton::clicked, this, [this]() {
        active_sort_ = "NEWEST";
        update_pill_group({sort_relevance_, sort_newest_}, active_sort_);
        emit sort_changed("NEWEST");
    });
    update_pill_group({sort_relevance_, sort_newest_}, active_sort_);

    hl->addSpacing(2);

    // View mode toggle
    view_wire_ = make_pill("WIRE", "WIRE", hl);
    view_clusters_ = make_pill("CLST", "CLUSTERS", hl);
    connect(view_wire_, &QPushButton::clicked, this, [this]() {
        active_view_ = "WIRE";
        update_pill_group({view_wire_, view_clusters_}, active_view_);
        emit view_mode_changed("WIRE");
    });
    connect(view_clusters_, &QPushButton::clicked, this, [this]() {
        active_view_ = "CLUSTERS";
        update_pill_group({view_wire_, view_clusters_}, active_view_);
        emit view_mode_changed("CLUSTERS");
    });
    update_pill_group({view_wire_, view_clusters_}, active_view_);

    hl->addStretch();

    // Unseen count
    unseen_label_ = new QLabel(row);
    unseen_label_->setObjectName("newsCommandBarUnseen");
    unseen_label_->hide();
    hl->addWidget(unseen_label_);

    // Alert count
    alert_label_ = new QLabel(row);
    alert_label_->setObjectName("newsCommandBarAlert");
    alert_label_->hide();
    hl->addWidget(alert_label_);

    // Article count
    count_label_ = new QLabel("0", row);
    count_label_->setObjectName("newsCommandBarCount");
    hl->addWidget(count_label_);

    // Progress indicator
    progress_label_ = new QLabel(row);
    progress_label_->setObjectName("newsCommandBarProgress");
    progress_label_->hide();
    hl->addWidget(progress_label_);

    // Language filter
    lang_filter_combo_ = new QComboBox(row);
    lang_filter_combo_->setObjectName("newsCommandBarCombo");
    lang_filter_combo_->setFixedHeight(18);
    lang_filter_combo_->setFixedWidth(44);
    lang_filter_combo_->addItems({"ALL", "EN", "FR", "DE", "ES", "AR", "ZH", "JA", "HI", "RU"});
    hl->addWidget(lang_filter_combo_);
    connect(lang_filter_combo_, &QComboBox::currentTextChanged, this, &NewsCommandBar::language_filter_changed);

    // Variant selector
    variant_combo_ = new QComboBox(row);
    variant_combo_->setObjectName("newsCommandBarCombo");
    variant_combo_->setFixedHeight(18);
    variant_combo_->setFixedWidth(60);
    variant_combo_->addItems({"FULL", "FINANCE", "CRYPTO", "MACRO"});
    hl->addWidget(variant_combo_);
    connect(variant_combo_, &QComboBox::currentTextChanged, this, &NewsCommandBar::variant_changed);

    // RTL toggle
    rtl_btn_ = new QPushButton(tr("RTL"), row);
    rtl_btn_->setObjectName("newsCommandBarPill");
    rtl_btn_->setFixedHeight(18);
    rtl_btn_->setCursor(Qt::PointingHandCursor);
    rtl_btn_->setCheckable(true);
    hl->addWidget(rtl_btn_);
    connect(rtl_btn_, &QPushButton::toggled, this, [this](bool checked) {
        rtl_btn_->setText(checked ? tr("LTR") : tr("RTL"));
        emit rtl_toggled();
    });

    // Summarize button
    summarize_btn_ = new QPushButton(tr("AI"), row);
    summarize_btn_->setObjectName("newsDetailAnalyzeBtn");
    summarize_btn_->setFixedHeight(20);
    summarize_btn_->setToolTip(tr("AI Brief — summarize headlines"));
    hl->addWidget(summarize_btn_);
    connect(summarize_btn_, &QPushButton::clicked, this, &NewsCommandBar::summarize_clicked);

    // Refresh-cadence selector — drives NewsService::set_refresh_interval.
    // 0 = manual (auto-refresh paused); other values are minutes.
    refresh_combo_ = new QComboBox(row);
    refresh_combo_->setObjectName("newsCommandBarCombo");
    refresh_combo_->setFixedHeight(18);
    refresh_combo_->setFixedWidth(70);
    refresh_combo_->setToolTip(tr("Auto-refresh interval"));
    refresh_combo_->addItem(tr("MANUAL"), 0);
    refresh_combo_->addItem(tr("1 MIN"), 1);
    refresh_combo_->addItem(tr("5 MIN"), 5);
    refresh_combo_->addItem(tr("10 MIN"), 10);
    refresh_combo_->addItem(tr("30 MIN"), 30);
    refresh_combo_->setCurrentIndex(3); // default 10 min — matches existing behavior
    hl->addWidget(refresh_combo_);
    connect(refresh_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) { emit refresh_interval_changed(refresh_combo_->itemData(idx).toInt()); });

    // Sources button — opens the RSS feed manager dialog
    sources_btn_ = new QPushButton(tr("SRC"), row);
    sources_btn_->setObjectName("newsCommandBarSources");
    sources_btn_->setFixedHeight(20);
    sources_btn_->setCursor(Qt::PointingHandCursor);
    sources_btn_->setToolTip(tr("Manage RSS feed sources"));
    hl->addWidget(sources_btn_);
    connect(sources_btn_, &QPushButton::clicked, this, &NewsCommandBar::manage_sources_clicked);

    // Refresh button
    refresh_btn_ = new QPushButton(tr("REFRESH"), row);
    refresh_btn_->setObjectName("newsCommandBarRefresh");
    refresh_btn_->setFixedHeight(20);
    hl->addWidget(refresh_btn_);
    connect(refresh_btn_, &QPushButton::clicked, this, &NewsCommandBar::refresh_clicked);

    scroll->setWidget(row);
    root->addWidget(scroll);
}

void NewsCommandBar::build_intel_row(QVBoxLayout* root) {
    auto* row = new QWidget(this);
    row->setObjectName("newsIntelStrip");
    row->setFixedHeight(26);

    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(6, 0, 6, 0);
    hl->setSpacing(8);

    // Stats: FEEDS | ARTICLES | CLUSTERS | SOURCES
    auto make_intel_stat = [&](const QString& label_text, QLabel** caption_out) -> QLabel* {
        auto* container = new QWidget(row);
        auto* cl = new QHBoxLayout(container);
        cl->setContentsMargins(0, 0, 0, 0);
        cl->setSpacing(3);
        auto* lbl = new QLabel(label_text, container);
        lbl->setObjectName("newsIntelLabel");
        auto* val = new QLabel("0", container);
        val->setObjectName("newsIntelValue");
        cl->addWidget(lbl);
        cl->addWidget(val);
        hl->addWidget(container);
        if (caption_out)
            *caption_out = lbl;
        return val;
    };

    intel_feeds_ = make_intel_stat(tr("FEEDS"), &intel_feeds_lbl_);
    intel_articles_ = make_intel_stat(tr("ARTS"), &intel_articles_lbl_);
    intel_clusters_ = make_intel_stat(tr("CLST"), &intel_clusters_lbl_);
    intel_sources_ = make_intel_stat(tr("SRCS"), &intel_sources_lbl_);

    // Separator
    auto* sep1 = new QLabel("|", row);
    sep1->setObjectName("newsIntelSep");
    hl->addWidget(sep1);

    // Sentiment gauge — compact horizontal bar + score
    auto* sent_container = new QWidget(row);
    auto* sent_layout = new QHBoxLayout(sent_container);
    sent_layout->setContentsMargins(0, 0, 0, 0);
    sent_layout->setSpacing(4);

    sentiment_caption_ = new QLabel(tr("SENT"), sent_container);
    sentiment_caption_->setObjectName("newsIntelLabel");
    sent_layout->addWidget(sentiment_caption_);

    auto* bar_frame = new QWidget(sent_container);
    bar_frame->setFixedSize(50, 6);
    auto* bar_layout = new QHBoxLayout(bar_frame);
    bar_layout->setContentsMargins(0, 0, 0, 0);
    bar_layout->setSpacing(1);

    sentiment_bull_ = new QWidget(bar_frame);
    sentiment_bull_->setObjectName("newsSentimentBull");
    sentiment_neut_ = new QWidget(bar_frame);
    sentiment_neut_->setObjectName("newsSentimentNeut");
    sentiment_bear_ = new QWidget(bar_frame);
    sentiment_bear_->setObjectName("newsSentimentBear");

    bar_layout->addWidget(sentiment_bull_, 1);
    bar_layout->addWidget(sentiment_neut_, 1);
    bar_layout->addWidget(sentiment_bear_, 1);

    sent_layout->addWidget(bar_frame);

    sentiment_score_ = new QLabel("+0.00", sent_container);
    sentiment_score_->setObjectName("newsIntelScore");
    sent_layout->addWidget(sentiment_score_);

    hl->addWidget(sent_container);

    // Separator
    auto* sep2 = new QLabel("|", row);
    sep2->setObjectName("newsIntelSep");
    hl->addWidget(sep2);

    // Monitor alerts summary
    intel_monitors_ = new QLabel(tr("0 WATCHES"), row);
    intel_monitors_->setObjectName("newsIntelMonitors");
    hl->addWidget(intel_monitors_);

    // Deviation alerts
    intel_deviations_ = new QLabel("", row);
    intel_deviations_->setObjectName("newsIntelDeviations");
    intel_deviations_->hide();
    hl->addWidget(intel_deviations_);

    hl->addStretch();

    // Live-feed badge (right side of intel strip). Reflects the
    // NewsService WebSocket state and toggles connect/disconnect on click.
    live_badge_ = new QPushButton(tr("OFFLINE"), row);
    live_badge_->setObjectName("newsLiveBadge");
    live_badge_->setFixedHeight(20);
    live_badge_->setCursor(Qt::PointingHandCursor);
    live_badge_->setToolTip(tr("Live feed status — click to toggle WebSocket connection"));
    live_badge_->setStyleSheet(
        "color:#94a3b8; background:transparent; border:1px solid #94a3b8;"
        " padding:0 6px; font-weight:700; font-size:10px;");
    hl->addWidget(live_badge_);
    connect(live_badge_, &QPushButton::clicked, this, &NewsCommandBar::live_toggle_clicked);

    root->addWidget(row);
}

QPushButton* NewsCommandBar::make_pill(const QString& text, const QString& value, QHBoxLayout* layout) {
    auto* btn = new QPushButton(text, this);
    btn->setObjectName("newsCommandBarPill");
    btn->setFixedHeight(18);
    btn->setCursor(Qt::PointingHandCursor);
    // Carry the logical value (e.g. "RELEVANCE" for the "REL" pill) as a
    // property so update_pill_group can match on intent rather than the
    // abbreviated label — lets callers pass the canonical value.
    btn->setProperty("pillValue", value);
    layout->addWidget(btn);
    return btn;
}

void NewsCommandBar::update_pill_group(const QVector<QPushButton*>& btns, const QString& active_value) {
    for (auto* btn : btns) {
        // Match against the pill's logical value, falling back to its label so
        // groups created before pillValue was set still behave.
        const QString value = btn->property("pillValue").toString();
        bool active = (value == active_value) || (btn->text() == active_value);
        btn->setProperty("active", active);
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
    }
}

void NewsCommandBar::set_active_category(const QString& cat) {
    active_category_ = cat;
    update_pill_group(category_btns_, active_category_);
}

void NewsCommandBar::set_active_sort(const QString& sort) {
    active_sort_ = sort;
    update_pill_group({sort_relevance_, sort_newest_}, active_sort_);
}

void NewsCommandBar::set_active_view(const QString& view) {
    active_view_ = view;
    update_pill_group({view_wire_, view_clusters_}, active_view_);
}

void NewsCommandBar::set_active_time_range(const QString& range) {
    active_time_ = range;
    update_pill_group(time_btns_, active_time_);
}

void NewsCommandBar::set_loading(bool loading) {
    refresh_btn_->setEnabled(!loading);
    refresh_btn_->setText(loading ? tr("...") : tr("REFRESH"));
}

void NewsCommandBar::set_loading_progress(int done, int total) {
    if (total > 0) {
        progress_label_->setText(QString("%1/%2").arg(done).arg(total));
        progress_label_->show();
    } else {
        progress_label_->hide();
    }
}

void NewsCommandBar::set_article_count(int count) {
    count_label_->setText(QString::number(count));
}

void NewsCommandBar::set_alert_count(int count) {
    if (count > 0) {
        alert_label_->setText(tr("%1 ALERTS").arg(count));
        alert_label_->show();
    } else {
        alert_label_->hide();
    }
}

void NewsCommandBar::set_unseen_count(int count) {
    if (count > 0) {
        unseen_label_->setText(tr("%1 NEW").arg(count));
        unseen_label_->show();
    } else {
        unseen_label_->hide();
    }
}

void NewsCommandBar::show_summary(const QString& summary) {
    summary_label_->setText(summary);
    summary_label_->show();
    summarize_btn_->setText(tr("AI"));
    summarize_btn_->setEnabled(true);
}

void NewsCommandBar::hide_summary() {
    summary_label_->hide();
}

void NewsCommandBar::set_summarizing(bool busy) {
    summarize_btn_->setText(busy ? tr("...") : tr("AI"));
    summarize_btn_->setEnabled(!busy);
}

// ── Intel strip updates ────────────────────────────────────────────────────

void NewsCommandBar::update_stats(int feeds, int articles, int clusters, int sources) {
    intel_feeds_->setText(QString::number(feeds));
    intel_articles_->setText(QString::number(articles));
    intel_clusters_->setText(QString::number(clusters));
    intel_sources_->setText(QString::number(sources));
}

void NewsCommandBar::update_sentiment(int bullish, int bearish, int neutral) {
    int total = bullish + bearish + neutral;
    if (total == 0)
        total = 1;

    int bull_w = std::max(1, bullish * 100 / total);
    int bear_w = std::max(1, bearish * 100 / total);
    int neut_w = 100 - bull_w - bear_w;

    auto* bar_layout = sentiment_bull_->parentWidget()->layout();
    if (auto* hl = qobject_cast<QHBoxLayout*>(bar_layout)) {
        hl->setStretch(0, bull_w);
        hl->setStretch(1, neut_w);
        hl->setStretch(2, bear_w);
    }

    double score = total > 0 ? static_cast<double>(bullish - bearish) / total : 0.0;
    sentiment_score_->setText(QString("%1%2").arg(score >= 0 ? "+" : "").arg(score, 0, 'f', 2));
}

void NewsCommandBar::update_deviations(const QVector<QPair<QString, double>>& deviations) {
    if (deviations.isEmpty()) {
        intel_deviations_->hide();
        return;
    }
    QStringList parts;
    for (int i = 0; i < std::min(3, static_cast<int>(deviations.size())); ++i)
        parts << QString("%1 %2x").arg(deviations[i].first.left(4)).arg(deviations[i].second, 0, 'f', 1);
    intel_deviations_->setText(parts.join("  "));
    intel_deviations_->show();
}

void NewsCommandBar::set_refresh_interval_minutes(int minutes) {
    if (!refresh_combo_)
        return;
    for (int i = 0; i < refresh_combo_->count(); ++i) {
        if (refresh_combo_->itemData(i).toInt() == minutes) {
            QSignalBlocker block(refresh_combo_);
            refresh_combo_->setCurrentIndex(i);
            return;
        }
    }
}

void NewsCommandBar::set_live_state(bool connected) {
    if (!live_badge_)
        return;
    if (connected) {
        live_badge_->setText(tr("● LIVE"));
        live_badge_->setStyleSheet(
            "color:#16a34a; background:transparent; border:1px solid #16a34a;"
            " padding:0 6px; font-weight:700; font-size:10px;");
    } else {
        live_badge_->setText(tr("OFFLINE"));
        live_badge_->setStyleSheet(
            "color:#94a3b8; background:transparent; border:1px solid #94a3b8;"
            " padding:0 6px; font-weight:700; font-size:10px;");
    }
}

void NewsCommandBar::update_monitor_summary(int total_monitors, int active_alerts) {
    if (active_alerts > 0)
        intel_monitors_->setText(tr("%1 WATCHES  %2 HIT").arg(total_monitors).arg(active_alerts));
    else
        intel_monitors_->setText(tr("%1 WATCHES").arg(total_monitors));
}

} // namespace fincept::screens
