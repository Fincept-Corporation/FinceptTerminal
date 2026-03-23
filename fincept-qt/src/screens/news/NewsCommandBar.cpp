#include "screens/news/NewsCommandBar.h"

#include "ui/theme/Theme.h"

#include <QStyle>

namespace fincept::screens {

NewsCommandBar::NewsCommandBar(QWidget* parent) : QWidget(parent) {
    setObjectName("newsCommandBar");
    setFixedHeight(32);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(8, 0, 8, 0);
    root->setSpacing(4);

    // Search input
    search_input_ = new QLineEdit(this);
    search_input_->setObjectName("newsCommandBarSearch");
    search_input_->setPlaceholderText("Search news...");
    search_input_->setFixedWidth(160);
    search_input_->setFixedHeight(22);
    root->addWidget(search_input_);

    connect(search_input_, &QLineEdit::textChanged, this, [this](const QString& text) {
        emit search_changed(text);
    });

    root->addSpacing(8);

    // Category pills
    QStringList categories = {"ALL", "MKT", "EARN", "ECO", "TECH", "NRG", "CRPT", "GEO", "DEF"};
    for (const auto& cat : categories) {
        auto* btn = make_pill(cat, cat, root);
        category_btns_.append(btn);
        connect(btn, &QPushButton::clicked, this, [this, cat]() {
            active_category_ = cat;
            update_pill_group(category_btns_, active_category_);
            emit category_changed(cat);
        });
    }
    update_pill_group(category_btns_, active_category_);

    root->addSpacing(12);

    // Separator
    auto* sep1 = new QLabel("|", this);
    sep1->setObjectName("newsCommandBarSep");
    root->addWidget(sep1);

    root->addSpacing(4);

    // Time range pills
    QStringList time_ranges = {"1H", "6H", "24H", "48H", "7D"};
    for (const auto& tr : time_ranges) {
        auto* btn = make_pill(tr, tr, root);
        time_btns_.append(btn);
        connect(btn, &QPushButton::clicked, this, [this, tr]() {
            active_time_ = tr;
            update_pill_group(time_btns_, active_time_);
            emit time_range_changed(tr);
        });
    }
    update_pill_group(time_btns_, active_time_);

    root->addSpacing(12);

    // Sort toggle
    sort_relevance_ = make_pill("RELEVANCE", "RELEVANCE", root);
    sort_newest_ = make_pill("NEWEST", "NEWEST", root);
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

    root->addSpacing(8);

    // Separator
    auto* sep2 = new QLabel("|", this);
    sep2->setObjectName("newsCommandBarSep");
    root->addWidget(sep2);

    root->addSpacing(4);

    // View mode toggle
    view_wire_ = make_pill("WIRE", "WIRE", root);
    view_clusters_ = make_pill("CLUSTERS", "CLUSTERS", root);
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

    root->addStretch();

    // Unseen count
    unseen_label_ = new QLabel(this);
    unseen_label_->setObjectName("newsCommandBarUnseen");
    unseen_label_->hide();
    root->addWidget(unseen_label_);

    // Alert count
    alert_label_ = new QLabel(this);
    alert_label_->setObjectName("newsCommandBarAlert");
    alert_label_->hide();
    root->addWidget(alert_label_);

    // Article count
    count_label_ = new QLabel("0", this);
    count_label_->setObjectName("newsCommandBarCount");
    root->addWidget(count_label_);

    // Progress indicator
    progress_label_ = new QLabel(this);
    progress_label_->setObjectName("newsCommandBarProgress");
    progress_label_->hide();
    root->addWidget(progress_label_);

    // Language filter
    lang_filter_combo_ = new QComboBox(this);
    lang_filter_combo_->setObjectName("newsCommandBarCombo");
    lang_filter_combo_->setFixedHeight(20);
    lang_filter_combo_->addItems({"ALL", "EN", "FR", "DE", "ES", "AR", "ZH", "JA", "HI", "RU"});
    root->addWidget(lang_filter_combo_);
    connect(lang_filter_combo_, &QComboBox::currentTextChanged, this, &NewsCommandBar::language_filter_changed);

    // Variant selector
    variant_combo_ = new QComboBox(this);
    variant_combo_->setObjectName("newsCommandBarCombo");
    variant_combo_->setFixedHeight(20);
    variant_combo_->addItems({"FULL", "FINANCE", "CRYPTO", "MACRO"});
    root->addWidget(variant_combo_);
    connect(variant_combo_, &QComboBox::currentTextChanged, this, &NewsCommandBar::variant_changed);

    // RTL toggle
    auto* rtl_btn = new QPushButton("RTL", this);
    rtl_btn->setObjectName("newsCommandBarPill");
    rtl_btn->setFixedHeight(20);
    rtl_btn->setCursor(Qt::PointingHandCursor);
    rtl_btn->setCheckable(true);
    root->addWidget(rtl_btn);
    connect(rtl_btn, &QPushButton::toggled, this, [this, rtl_btn](bool checked) {
        rtl_btn->setText(checked ? "LTR" : "RTL");
        emit rtl_toggled();
    });

    // Summarize button
    summarize_btn_ = new QPushButton("AI BRIEF", this);
    summarize_btn_->setObjectName("newsDetailAnalyzeBtn");
    summarize_btn_->setFixedHeight(22);
    root->addWidget(summarize_btn_);
    connect(summarize_btn_, &QPushButton::clicked, this, &NewsCommandBar::summarize_clicked);

    // Refresh button
    refresh_btn_ = new QPushButton("REFRESH", this);
    refresh_btn_->setObjectName("newsCommandBarRefresh");
    refresh_btn_->setFixedHeight(22);
    root->addWidget(refresh_btn_);
    connect(refresh_btn_, &QPushButton::clicked, this, &NewsCommandBar::refresh_clicked);

    // Summary display label (hidden, shown between command bar and feed)
    summary_label_ = new QLabel(this);
    summary_label_->setObjectName("newsDetailAiSummary");
    summary_label_->setWordWrap(true);
    summary_label_->hide();
}

QPushButton* NewsCommandBar::make_pill(const QString& text, const QString& value, QHBoxLayout* layout) {
    Q_UNUSED(value);
    auto* btn = new QPushButton(text, this);
    btn->setObjectName("newsCommandBarPill");
    btn->setFixedHeight(20);
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
    summarize_btn_->setText("AI BRIEF");
    summarize_btn_->setEnabled(true);
}

void NewsCommandBar::hide_summary() {
    summary_label_->hide();
}

void NewsCommandBar::set_summarizing(bool busy) {
    summarize_btn_->setText(busy ? "..." : "AI BRIEF");
    summarize_btn_->setEnabled(!busy);
}

} // namespace fincept::screens
