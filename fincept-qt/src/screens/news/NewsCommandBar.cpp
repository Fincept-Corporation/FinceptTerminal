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
    drawer_btn_ = new QPushButton("INTEL", row);
    drawer_btn_->setObjectName("newsDrawerBtn");
    drawer_btn_->setFixedHeight(20);
    drawer_btn_->setCursor(Qt::PointingHandCursor);
    drawer_btn_->setCheckable(true);
    drawer_btn_->setToolTip("Toggle intelligence drawer");
    hl->addWidget(drawer_btn_);
    connect(drawer_btn_, &QPushButton::clicked, this, &NewsCommandBar::drawer_toggle_requested);

    hl->addSpacing(4);

    // Search input — compact
    search_input_ = new QLineEdit(row);
    search_input_->setObjectName("newsCommandBarSearch");
    search_input_->setPlaceholderText("Search...");
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
        update_pill_group({sort_relevance_, sort_newest_}, "REL");
        emit sort_changed("RELEVANCE");
    });
    connect(sort_newest_, &QPushButton::clicked, this, [this]() {
        active_sort_ = "NEWEST";
        update_pill_group({sort_relevance_, sort_newest_}, "NEW");
        emit sort_changed("NEWEST");
    });
    update_pill_group({sort_relevance_, sort_newest_}, "REL");

    hl->addSpacing(2);

    // View mode toggle
    view_wire_ = make_pill("WIRE", "WIRE", hl);
    view_clusters_ = make_pill("CLST", "CLUSTERS", hl);
    connect(view_wire_, &QPushButton::clicked, this, [this]() {
        active_view_ = "WIRE";
        update_pill_group({view_wire_, view_clusters_}, "WIRE");
        emit view_mode_changed("WIRE");
    });
    connect(view_clusters_, &QPushButton::clicked, this, [this]() {
        active_view_ = "CLUSTERS";
        update_pill_group({view_wire_, view_clusters_}, "CLST");
        emit view_mode_changed("CLUSTERS");
    });
    update_pill_group({view_wire_, view_clusters_}, "WIRE");

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
    auto* rtl_btn = new QPushButton("RTL", row);
    rtl_btn->setObjectName("newsCommandBarPill");
    rtl_btn->setFixedHeight(18);
    rtl_btn->setCursor(Qt::PointingHandCursor);
    rtl_btn->setCheckable(true);
    hl->addWidget(rtl_btn);
    connect(rtl_btn, &QPushButton::toggled, this, [this, rtl_btn](bool checked) {
        rtl_btn->setText(checked ? "LTR" : "RTL");
        emit rtl_toggled();
    });

    // Summarize button
    summarize_btn_ = new QPushButton("AI", row);
    summarize_btn_->setObjectName("newsDetailAnalyzeBtn");
    summarize_btn_->setFixedHeight(20);
    summarize_btn_->setToolTip("AI Brief — summarize headlines");
    hl->addWidget(summarize_btn_);
    connect(summarize_btn_, &QPushButton::clicked, this, &NewsCommandBar::summarize_clicked);

    // Refresh button
    refresh_btn_ = new QPushButton("REFRESH", row);
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
    auto make_intel_stat = [&](const QString& label_text) -> QLabel* {
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
        return val;
    };

    intel_feeds_ = make_intel_stat("FEEDS");
    intel_articles_ = make_intel_stat("ARTS");
    intel_clusters_ = make_intel_stat("CLST");
    intel_sources_ = make_intel_stat("SRCS");

    // Separator
    auto* sep1 = new QLabel("|", row);
    sep1->setObjectName("newsIntelSep");
    hl->addWidget(sep1);

    // Sentiment gauge — compact horizontal bar + score
    auto* sent_container = new QWidget(row);
    auto* sent_layout = new QHBoxLayout(sent_container);
    sent_layout->setContentsMargins(0, 0, 0, 0);
    sent_layout->setSpacing(4);

    auto* sent_label = new QLabel("SENT", sent_container);
    sent_label->setObjectName("newsIntelLabel");
    sent_layout->addWidget(sent_label);

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
    intel_monitors_ = new QLabel("0 WATCHES", row);
    intel_monitors_->setObjectName("newsIntelMonitors");
    hl->addWidget(intel_monitors_);

    // Deviation alerts
    intel_deviations_ = new QLabel("", row);
    intel_deviations_->setObjectName("newsIntelDeviations");
    intel_deviations_->hide();
    hl->addWidget(intel_deviations_);

    hl->addStretch();

    root->addWidget(row);
}

QPushButton* NewsCommandBar::make_pill(const QString& text, const QString& value, QHBoxLayout* layout) {
    Q_UNUSED(value);
    auto* btn = new QPushButton(text, this);
    btn->setObjectName("newsCommandBarPill");
    btn->setFixedHeight(18);
    btn->setCursor(Qt::PointingHandCursor);
    layout->addWidget(btn);
    return btn;
}

void NewsCommandBar::update_pill_group(const QVector<QPushButton*>& btns, const QString& active_value) {
    for (auto* btn : btns) {
        bool active = (btn->text() == active_value);
        btn->setProperty("active", active);
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
    }
}

void NewsCommandBar::set_active_category(const QString& cat) {
    active_category_ = cat;
    update_pill_group(category_btns_, active_category_);
}

void NewsCommandBar::set_loading(bool loading) {
    refresh_btn_->setEnabled(!loading);
    refresh_btn_->setText(loading ? "..." : "REFRESH");
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
        alert_label_->setText(QString("%1 ALERTS").arg(count));
        alert_label_->show();
    } else {
        alert_label_->hide();
    }
}

void NewsCommandBar::set_unseen_count(int count) {
    if (count > 0) {
        unseen_label_->setText(QString("%1 NEW").arg(count));
        unseen_label_->show();
    } else {
        unseen_label_->hide();
    }
}

void NewsCommandBar::show_summary(const QString& summary) {
    summary_label_->setText(summary);
    summary_label_->show();
    summarize_btn_->setText("AI");
    summarize_btn_->setEnabled(true);
}

void NewsCommandBar::hide_summary() {
    summary_label_->hide();
}

void NewsCommandBar::set_summarizing(bool busy) {
    summarize_btn_->setText(busy ? "..." : "AI");
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

void NewsCommandBar::update_monitor_summary(int total_monitors, int active_alerts) {
    if (active_alerts > 0)
        intel_monitors_->setText(QString("%1 WATCHES  %2 HIT").arg(total_monitors).arg(active_alerts));
    else
        intel_monitors_->setText(QString("%1 WATCHES").arg(total_monitors));
}

} // namespace fincept::screens
