#include "screens/news/NewsCommandBar.h"

#include "services/news/NewsLanguage.h"
#include "ui/formatting/DateFormat.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/Card.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/StatTile.h"
#include "ui/widgets/StatusBadge.h"

#include <QActionGroup>
#include <QFontMetrics>
#include <QHash>
#include <QLocale>
#include <QMenu>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QStyle>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace {

/// Region filter values (and their display labels, see ncb_region_label).
const QStringList& ncb_regions() {
    static const QStringList regions = {"ALL",   "US",   "EUROPE", "ASIA-PAC", "CHINA",  "JAPAN", "KOREA", "INDIA",
                                        "SEA",   "ANZ",  "LATAM",  "MENA",     "AFRICA", "CIS",   "GLOBAL"};
    return regions;
}

QString ncb_region_label(const QString& code) {
    if (code == QLatin1String("ALL"))
        return QCoreApplication::translate("NewsCommandBar", "ALL REGIONS");
    if (code == QLatin1String("SEA"))
        return QCoreApplication::translate("NewsCommandBar", "SE ASIA");
    if (code == QLatin1String("ANZ"))
        return QCoreApplication::translate("NewsCommandBar", "AUS / NZ");
    if (code == QLatin1String("CIS"))
        return QCoreApplication::translate("NewsCommandBar", "RUSSIA / CIS");
    return code;
}

void ncb_select_data(QComboBox* combo, const QString& value) {
    if (!combo)
        return;
    const int idx = combo->findData(value, Qt::UserRole, Qt::MatchFixedString);
    if (idx < 0)
        return;
    QSignalBlocker block(combo);
    combo->setCurrentIndex(idx);
}

} // namespace

NewsCommandBar::NewsCommandBar(QWidget* parent) : QWidget(parent) {
    setObjectName("newsCommandBar");
    setFixedHeight(kBaseHeight); // command row + intel strip

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    build_command_row(root);
    build_intel_row(root);

    // AI-brief summary row. This used to be a bare child QLabel that was never
    // added to a layout, so show_summary() painted it at (0,0) on top of the
    // command row inside a widget with a hard 60px height — i.e. the AI brief
    // was unreadable and undismissable. It now lives in the layout, with a
    // close button, and the bar grows to fit while it is shown.
    summary_row_ = new QWidget(this);
    summary_row_->setObjectName("newsSummaryRow");
    auto* srl = new QHBoxLayout(summary_row_);
    srl->setContentsMargins(8, 4, 6, 4);
    srl->setSpacing(6);

    summary_label_ = new QLabel(summary_row_);
    summary_label_->setObjectName("newsDetailAiSummary");
    summary_label_->setWordWrap(true);
    summary_label_->setTextFormat(Qt::PlainText); // model output is untrusted text
    summary_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    summary_label_->setAccessibleName(tr("Headline brief"));
    srl->addWidget(summary_label_, 1);

    // Structured brief (show_brief): a scrollable card, filled per brief.
    auto* brief_scroll = new QScrollArea(summary_row_);
    brief_scroll->setWidgetResizable(true);
    brief_scroll->setFrameShape(QFrame::NoFrame);
    brief_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    brief_scroll->hide();
    brief_scroll_ = brief_scroll;
    srl->addWidget(brief_scroll_, 1);

    summary_close_btn_ = new QPushButton("x", summary_row_);
    summary_close_btn_->setObjectName("newsDetailCloseBtn");
    summary_close_btn_->setFixedSize(18, 18);
    summary_close_btn_->setCursor(Qt::PointingHandCursor);
    summary_close_btn_->setToolTip(tr("Dismiss the brief"));
    summary_close_btn_->setAccessibleName(tr("Dismiss headline brief"));
    connect(summary_close_btn_, &QPushButton::clicked, this, &NewsCommandBar::hide_summary);
    srl->addWidget(summary_close_btn_, 0, Qt::AlignTop);

    summary_row_->hide();
    root->addWidget(summary_row_);

    fit_to_font();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { fit_to_font(); });
    // Which controls fit is decided from their size hints, and inside the
    // theme_changed handler those still describe the old font (the controls are
    // styled after the handlers ran, or — on a hidden screen — later still): at
    // 24 px every control stayed in the row and REFRESH was cut off at the edge.
    // So: once the change is in, and again whenever the bar is shown.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        if (isVisible())
            fit_to_width();
    });
}

void NewsCommandBar::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    fit_to_width();
}

void NewsCommandBar::fit_to_font() {
    const QFontMetrics fm(ui::fonts::font(-3));
    const int ctl_h = std::max(18, fm.height() + 4);    // pills, combos
    const int btn_h = std::max(20, fm.height() + 6);    // buttons, search box, badge
    const int row_h = std::max(32, btn_h + 12);         // command row
    const int intel_h = std::max(26, fm.height() + 10); // intel strip
    const auto text_w = [&fm](const QString& s) { return fm.horizontalAdvance(s); };

    command_row_->setFixedHeight(row_h);
    command_scroll_->setFixedHeight(row_h);
    filter_group_->setFixedHeight(row_h);
    intel_row_->setFixedHeight(intel_h);

    QVector<QPushButton*> pills = category_btns_ + time_btns_;
    pills << sort_relevance_ << sort_newest_ << view_wire_ << view_clusters_ << rtl_btn_ << unread_btn_ << charts_btn_;
    for (auto* b : pills)
        b->setFixedHeight(ctl_h);
    for (auto* b : {drawer_btn_, summarize_btn_, sources_btn_, refresh_btn_, live_badge_, mark_read_btn_, saved_btn_,
                    more_btn_, feed_health_btn_})
        b->setFixedHeight(btn_h);

    search_input_->setFixedHeight(btn_h);
    search_input_->setFixedWidth(
        std::max(120, QFontMetrics(ui::fonts::font(-2)).horizontalAdvance(QString(14, QLatin1Char('0'))) + 24));

    // Combo widths: the longest entry plus the drop-down arrow.
    auto fit_combo = [&](QComboBox* c, int min_w) {
        int w = 0;
        for (int i = 0; i < c->count(); ++i)
            w = std::max(w, text_w(c->itemText(i)));
        c->setFixedHeight(ctl_h);
        c->setFixedWidth(std::max(min_w, w + 26));
    };
    fit_combo(lang_filter_combo_, 44);
    fit_combo(region_combo_, 70);
    fit_combo(variant_combo_, 60);
    fit_combo(refresh_combo_, 70);
    fit_combo(my_combo_, 80);
    my_combo_->setFixedWidth(std::min(my_combo_->width(), std::max(110, text_w(QString(16, QLatin1Char('M'))))));

    const int extra = summary_row_->isHidden() ? 0 : height() - base_height_;
    base_height_ = std::max(kBaseHeight, row_h + intel_h + 2);
    setFixedHeight(base_height_ + std::max(0, extra));
}

void NewsCommandBar::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    fit_to_width();
}

void NewsCommandBar::fit_to_width() {
    if (!control_group_)
        return;
    // Least important first: hidden first when the bar gets narrow. Each one
    // stays available in the ⋯ menu. The filter group keeps at least ~40% of
    // the row (it scrolls inside its own area).
    QWidget* const optional[] = {rtl_btn_,      sources_btn_,  refresh_combo_, variant_combo_, region_combo_,
                                 lang_filter_combo_, my_combo_, mark_read_btn_, saved_btn_,  charts_btn_,
                                 unread_btn_,   count_label_};
    for (QWidget* w : optional)
        w->setVisible(true);
    const int budget = std::max(200, width() - std::max(260, width() * 2 / 5));
    for (QWidget* w : optional) {
        if (control_group_->sizeHint().width() <= budget)
            break;
        w->setVisible(false);
    }
}

bool NewsCommandBar::eventFilter(QObject* watched, QEvent* event) {
    // The filter group scrolls sideways with the mouse wheel (its scrollbar is
    // hidden to keep the row one line high).
    if (watched == command_scroll_->viewport() && event->type() == QEvent::Wheel) {
        auto* we = static_cast<QWheelEvent*>(event);
        const int delta = we->angleDelta().y() != 0 ? we->angleDelta().y() : we->angleDelta().x();
        auto* bar = command_scroll_->horizontalScrollBar();
        bar->setValue(bar->value() - delta / 2);
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void NewsCommandBar::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void NewsCommandBar::retranslateUi() {
    // Command row — fixed-label buttons / inputs.
    if (drawer_btn_) {
        drawer_btn_->setText(tr("INTEL"));
        drawer_btn_->setToolTip(tr("Toggle intelligence drawer"));
    }
    if (search_input_)
        search_input_->setPlaceholderText(tr("Search..."));
    if (sources_btn_) {
        sources_btn_->setText(tr("SRC"));
        sources_btn_->setToolTip(tr("Manage RSS feed sources"));
    }
    if (summarize_btn_) {
        summarize_btn_->setText(tr("BRIEF"));
        summarize_btn_->setToolTip(tr("Headline brief — the top stories of the current filter, most important "
                                      "first, grouped by category (built locally, no AI, no credits)"));
    }
    if (summary_close_btn_) {
        summary_close_btn_->setToolTip(tr("Dismiss the brief"));
        summary_close_btn_->setAccessibleName(tr("Dismiss headline brief"));
    }
    if (summary_label_)
        summary_label_->setAccessibleName(tr("Headline brief"));
    if (unread_btn_) {
        unread_btn_->setText(tr("UNREAD"));
        unread_btn_->setToolTip(tr("Show only articles you have not seen yet"));
    }
    if (mark_read_btn_) {
        mark_read_btn_->setText(tr("MARK READ"));
        mark_read_btn_->setToolTip(tr("Mark every article in the current list as read"));
    }
    if (charts_btn_) {
        charts_btn_->setText(tr("CHARTS"));
        charts_btn_->setToolTip(tr("News analytics: volume and tone over time, trending tickers, tone heatmaps"));
    }
    if (saved_btn_) {
        saved_btn_->setText(tr("SAVED"));
        saved_btn_->setToolTip(tr("Saved searches and alerts"));
    }
    if (more_btn_)
        more_btn_->setToolTip(tr("All news controls"));
    if (lang_filter_combo_)
        lang_filter_combo_->setToolTip(tr("Article language (detected from the text)"));
    if (region_combo_) {
        region_combo_->setToolTip(tr("Region of the source"));
        for (int i = 0; i < region_combo_->count(); ++i)
            region_combo_->setItemText(i, ncb_region_label(region_combo_->itemData(i).toString()));
    }
    if (my_combo_) {
        my_combo_->setToolTip(tr("Only news about your holdings and / or watchlists"));
        set_watchlists(watchlists_); // rebuilds the entries in the new language
    }
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
    if (intel_feeds_lbl_)
        intel_feeds_lbl_->setText(tr("FEEDS"));
    if (intel_articles_lbl_)
        intel_articles_lbl_->setText(tr("ARTS"));
    if (intel_clusters_lbl_)
        intel_clusters_lbl_->setText(tr("CLST"));
    if (intel_sources_lbl_)
        intel_sources_lbl_->setText(tr("SRCS"));
    if (sentiment_caption_)
        sentiment_caption_->setText(tr("TONE"));

    // Dynamic count labels (alerts / unseen / monitors / live badge) reflect
    // runtime counts and refresh on the next data update — not re-applied here.
}

void NewsCommandBar::fill_language_combo() {
    QSignalBlocker block(lang_filter_combo_);
    lang_filter_combo_->clear();
    lang_filter_combo_->addItem(tr("ALL"), QStringLiteral("ALL"));
    for (const QString& code : services::news_lang::filter_languages())
        lang_filter_combo_->addItem(code.toUpper(), code.toUpper());
}

void NewsCommandBar::fill_region_combo() {
    QSignalBlocker block(region_combo_);
    region_combo_->clear();
    for (const QString& code : ncb_regions())
        region_combo_->addItem(ncb_region_label(code), code);
}

void NewsCommandBar::build_command_row(QVBoxLayout* root) {
    auto* row = new QWidget(this);
    row->setObjectName("newsCommandRow");
    command_row_ = row;
    auto* row_layout = new QHBoxLayout(row);
    row_layout->setContentsMargins(0, 0, 0, 0);
    row_layout->setSpacing(0);

    // ── Left: the filter group, in a scroll area (wheel scrolls it sideways) ──
    auto* scroll = new QScrollArea(row);
    scroll->setObjectName("newsCommandRowScroll");
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->installEventFilter(this);
    command_scroll_ = scroll;

    auto* left = new QWidget(scroll);
    left->setObjectName("newsCommandRow");
    filter_group_ = left;
    auto* hl = new QHBoxLayout(left);
    hl->setContentsMargins(6, 0, 6, 0);
    hl->setSpacing(2);

    // Drawer toggle button
    drawer_btn_ = new QPushButton(tr("INTEL"), left);
    drawer_btn_->setObjectName("newsDrawerBtn");
    drawer_btn_->setCursor(Qt::PointingHandCursor);
    drawer_btn_->setCheckable(true);
    drawer_btn_->setToolTip(tr("Toggle intelligence drawer"));
    hl->addWidget(drawer_btn_);
    connect(drawer_btn_, &QPushButton::clicked, this, &NewsCommandBar::drawer_toggle_requested);

    hl->addSpacing(4);

    // Search input — compact
    search_input_ = new QLineEdit(left);
    search_input_->setObjectName("newsCommandBarSearch");
    search_input_->setPlaceholderText(tr("Search..."));
    search_input_->setAccessibleName(tr("Search headlines"));
    search_input_->setClearButtonEnabled(true);
    hl->addWidget(search_input_);

    // Every keystroke used to start a full filter + cluster pass over the whole
    // article list (and a DB search on the 7D/30D ranges). Coalesce typing; an
    // emptied box (clear button / backspace-to-empty) still applies immediately.
    search_debounce_ = new QTimer(this);
    search_debounce_->setSingleShot(true);
    search_debounce_->setInterval(200);
    connect(search_debounce_, &QTimer::timeout, this, [this]() { emit search_changed(search_input_->text()); });
    connect(search_input_, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (text.isEmpty()) {
            search_debounce_->stop();
            emit search_changed(text);
            return;
        }
        search_debounce_->start();
    });

    hl->addSpacing(4);

    // Category pills — compact
    QStringList categories = {"ALL", "MKT", "EARN", "ECO", "REG", "TECH", "NRG", "CRPT", "GEO", "DEF"};
    for (const auto& cat : categories) {
        auto* btn = make_pill(cat, cat, hl);
        btn->setAccessibleName(tr("Category %1").arg(cat));
        category_btns_.append(btn);
        connect(btn, &QPushButton::clicked, this, [this, cat]() {
            active_category_ = cat;
            update_pill_group(category_btns_, active_category_);
            emit category_changed(cat);
        });
    }
    update_pill_group(category_btns_, active_category_);

    hl->addSpacing(4);
    auto* sep1 = new QLabel("|", left);
    sep1->setObjectName("newsCommandBarSep");
    hl->addWidget(sep1);
    hl->addSpacing(2);

    // Time range pills. NOTE: the loop variable was previously named `tr`,
    // which shadows QObject::tr() inside the loop body.
    QStringList time_ranges = {"1H", "6H", "24H", "48H", "7D", "30D"};
    for (const auto& range : time_ranges) {
        auto* btn = make_pill(range, range, hl);
        btn->setAccessibleName(tr("Time range %1").arg(range));
        time_btns_.append(btn);
        connect(btn, &QPushButton::clicked, this, [this, range]() {
            active_time_ = range;
            update_pill_group(time_btns_, active_time_);
            emit time_range_changed(range);
        });
    }
    update_pill_group(time_btns_, active_time_);

    hl->addSpacing(4);

    // Sort toggle
    sort_relevance_ = make_pill("REL", "RELEVANCE", hl);
    sort_newest_ = make_pill("NEW", "NEWEST", hl);
    sort_relevance_->setToolTip(tr("Rank by importance: priority, market impact, source tier, threat, recency"));
    sort_newest_->setToolTip(tr("Newest first"));
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
    view_wire_->setToolTip(tr("Every headline, duplicates across sources collapsed"));
    view_clusters_->setToolTip(tr("Stories: headlines about the same event grouped, with momentum"));
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

    scroll->setWidget(left);
    row_layout->addWidget(scroll, 1);

    // ── Right: the control group — never scrolled away ──────────────────────
    auto* right = new QWidget(row);
    right->setObjectName("newsCommandRow");
    control_group_ = right;
    auto* rl = new QHBoxLayout(right);
    rl->setContentsMargins(4, 0, 6, 0);
    rl->setSpacing(3);

    unseen_label_ = new QLabel(right);
    unseen_label_->setObjectName("newsCommandBarUnseen");
    unseen_label_->hide();
    rl->addWidget(unseen_label_);

    alert_label_ = new QLabel(right);
    alert_label_->setObjectName("newsCommandBarAlert");
    alert_label_->hide();
    rl->addWidget(alert_label_);

    count_label_ = new QLabel("0", right);
    count_label_->setObjectName("newsCommandBarCount");
    rl->addWidget(count_label_);

    progress_label_ = new QLabel(right);
    progress_label_->setObjectName("newsCommandBarProgress");
    progress_label_->hide();
    rl->addWidget(progress_label_);

    unread_btn_ = make_pill(tr("UNREAD"), "UNREAD", rl);
    unread_btn_->setCheckable(true);
    connect(unread_btn_, &QPushButton::toggled, this, [this](bool on) {
        unread_btn_->setProperty("active", on);
        unread_btn_->style()->unpolish(unread_btn_);
        unread_btn_->style()->polish(unread_btn_);
        emit unread_only_toggled(on);
    });

    mark_read_btn_ = new QPushButton(tr("MARK READ"), right);
    mark_read_btn_->setObjectName("newsCommandBarSources");
    mark_read_btn_->setCursor(Qt::PointingHandCursor);
    rl->addWidget(mark_read_btn_);
    connect(mark_read_btn_, &QPushButton::clicked, this, &NewsCommandBar::mark_all_read_clicked);

    my_combo_ = new QComboBox(right);
    my_combo_->setObjectName("newsCommandBarCombo");
    rl->addWidget(my_combo_);
    connect(my_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        active_my_ = my_combo_->itemData(idx).toString();
        emit my_filter_changed(active_my_);
    });

    lang_filter_combo_ = new QComboBox(right);
    lang_filter_combo_->setObjectName("newsCommandBarCombo");
    fill_language_combo();
    rl->addWidget(lang_filter_combo_);
    connect(lang_filter_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) { emit language_filter_changed(lang_filter_combo_->itemData(idx).toString()); });

    region_combo_ = new QComboBox(right);
    region_combo_->setObjectName("newsCommandBarCombo");
    fill_region_combo();
    rl->addWidget(region_combo_);
    connect(region_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) { emit region_filter_changed(region_combo_->itemData(idx).toString()); });

    variant_combo_ = new QComboBox(right);
    variant_combo_->setObjectName("newsCommandBarCombo");
    variant_combo_->addItems({"FULL", "FINANCE", "CRYPTO", "MACRO"});
    rl->addWidget(variant_combo_);
    connect(variant_combo_, &QComboBox::currentTextChanged, this, &NewsCommandBar::variant_changed);

    charts_btn_ = make_pill(tr("CHARTS"), "CHARTS", rl);
    charts_btn_->setCheckable(true);
    connect(charts_btn_, &QPushButton::toggled, this, [this](bool on) {
        charts_btn_->setProperty("active", on);
        charts_btn_->style()->unpolish(charts_btn_);
        charts_btn_->style()->polish(charts_btn_);
        emit charts_toggled(on);
    });

    saved_btn_ = new QPushButton(tr("SAVED"), right);
    saved_btn_->setObjectName("newsCommandBarSources");
    saved_btn_->setCursor(Qt::PointingHandCursor);
    rl->addWidget(saved_btn_);
    connect(saved_btn_, &QPushButton::clicked, this, &NewsCommandBar::saved_searches_clicked);

    // RTL toggle
    rtl_btn_ = new QPushButton(tr("RTL"), right);
    rtl_btn_->setObjectName("newsCommandBarPill");
    rtl_btn_->setCursor(Qt::PointingHandCursor);
    rtl_btn_->setCheckable(true);
    rl->addWidget(rtl_btn_);
    connect(rtl_btn_, &QPushButton::toggled, this, [this](bool checked) {
        rtl_btn_->setText(checked ? tr("LTR") : tr("RTL"));
        emit rtl_toggled();
    });

    // Headline brief — NewsService builds it locally (no AI, no credits).
    summarize_btn_ = new QPushButton(tr("BRIEF"), right);
    summarize_btn_->setObjectName("newsDetailAnalyzeBtn");
    rl->addWidget(summarize_btn_);
    connect(summarize_btn_, &QPushButton::clicked, this, &NewsCommandBar::summarize_clicked);

    // Refresh-cadence selector — drives NewsService::set_refresh_interval.
    // 0 = manual (auto-refresh paused); other values are minutes.
    refresh_combo_ = new QComboBox(right);
    refresh_combo_->setObjectName("newsCommandBarCombo");
    refresh_combo_->addItem(tr("MANUAL"), 0);
    refresh_combo_->addItem(tr("1 MIN"), 1);
    refresh_combo_->addItem(tr("5 MIN"), 5);
    refresh_combo_->addItem(tr("10 MIN"), 10);
    refresh_combo_->addItem(tr("30 MIN"), 30);
    refresh_combo_->setCurrentIndex(3); // default 10 min — matches existing behavior
    rl->addWidget(refresh_combo_);
    connect(refresh_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) { emit refresh_interval_changed(refresh_combo_->itemData(idx).toInt()); });

    // Sources button — opens the RSS feed manager dialog
    sources_btn_ = new QPushButton(tr("SRC"), right);
    sources_btn_->setObjectName("newsCommandBarSources");
    sources_btn_->setCursor(Qt::PointingHandCursor);
    rl->addWidget(sources_btn_);
    connect(sources_btn_, &QPushButton::clicked, this, &NewsCommandBar::manage_sources_clicked);

    // Refresh button
    refresh_btn_ = new QPushButton(tr("REFRESH"), right);
    refresh_btn_->setObjectName("newsCommandBarRefresh");
    refresh_btn_->setAccessibleName(tr("Refresh news feed"));
    rl->addWidget(refresh_btn_);
    connect(refresh_btn_, &QPushButton::clicked, this, &NewsCommandBar::refresh_clicked);

    // ⋯ — every control, whatever the width.
    more_btn_ = new QPushButton(QString::fromUtf8("\xe2\x8b\xaf"), right);
    more_btn_->setObjectName("newsCommandBarSources");
    more_btn_->setCursor(Qt::PointingHandCursor);
    more_btn_->setAccessibleName(tr("More news controls"));
    rl->addWidget(more_btn_);
    connect(more_btn_, &QPushButton::clicked, this, &NewsCommandBar::show_overflow_menu);

    row_layout->addWidget(right, 0);
    set_watchlists({});
    retranslateUi();

    // Keyboard order across the bar: search → category pills → time pills →
    // sort → view → controls.
    QWidget* prev = search_input_;
    for (auto* b : category_btns_) {
        QWidget::setTabOrder(prev, b);
        prev = b;
    }
    for (auto* b : time_btns_) {
        QWidget::setTabOrder(prev, b);
        prev = b;
    }
    for (QWidget* w : {static_cast<QWidget*>(sort_relevance_), static_cast<QWidget*>(sort_newest_),
                       static_cast<QWidget*>(view_wire_), static_cast<QWidget*>(view_clusters_),
                       static_cast<QWidget*>(unread_btn_), static_cast<QWidget*>(my_combo_),
                       static_cast<QWidget*>(lang_filter_combo_), static_cast<QWidget*>(region_combo_),
                       static_cast<QWidget*>(variant_combo_), static_cast<QWidget*>(charts_btn_),
                       static_cast<QWidget*>(saved_btn_), static_cast<QWidget*>(summarize_btn_),
                       static_cast<QWidget*>(refresh_combo_), static_cast<QWidget*>(sources_btn_),
                       static_cast<QWidget*>(refresh_btn_), static_cast<QWidget*>(more_btn_)}) {
        QWidget::setTabOrder(prev, w);
        prev = w;
    }

    root->addWidget(row);
}

void NewsCommandBar::show_overflow_menu() {
    QMenu menu(this);
    auto add_choice_menu = [&menu](const QString& title, const QVector<QPair<QString, QString>>& items,
                                   const QString& current, const std::function<void(const QString&)>& pick) {
        QMenu* sub = menu.addMenu(title);
        auto* group = new QActionGroup(sub);
        for (const auto& item : items) {
            const QString value = item.first; // a plain copy: lambdas capture it portably
            QAction* a = sub->addAction(item.second);
            a->setCheckable(true);
            a->setChecked(value == current);
            group->addAction(a);
            QObject::connect(a, &QAction::triggered, sub, [pick, value]() { pick(value); });
        }
    };
    auto pills_items = [](const QVector<QPushButton*>& btns) {
        QVector<QPair<QString, QString>> items;
        for (auto* b : btns)
            items.append({b->property("pillValue").toString(), b->text()});
        return items;
    };
    add_choice_menu(tr("Category"), pills_items(category_btns_), active_category_, [this](const QString& v) {
        set_active_category(v);
        emit category_changed(v);
    });
    add_choice_menu(tr("Time range"), pills_items(time_btns_), active_time_, [this](const QString& v) {
        set_active_time_range(v);
        emit time_range_changed(v);
    });
    add_choice_menu(tr("Sort"), {{"RELEVANCE", tr("Relevance")}, {"NEWEST", tr("Newest")}}, active_sort_,
                    [this](const QString& v) {
                        set_active_sort(v);
                        emit sort_changed(v);
                    });
    add_choice_menu(tr("View"), {{"WIRE", tr("Wire (every headline)")}, {"CLUSTERS", tr("Stories (clusters)")}},
                    active_view_, [this](const QString& v) {
                        set_active_view(v);
                        emit view_mode_changed(v);
                    });
    menu.addSeparator();
    auto combo_items = [](QComboBox* c) {
        QVector<QPair<QString, QString>> items;
        for (int i = 0; i < c->count(); ++i)
            items.append({c->itemData(i).isValid() ? c->itemData(i).toString() : c->itemText(i), c->itemText(i)});
        return items;
    };
    auto combo_value = [](QComboBox* c) {
        return c->currentData().isValid() ? c->currentData().toString() : c->currentText();
    };
    add_choice_menu(tr("My news"), combo_items(my_combo_), combo_value(my_combo_),
                    [this](const QString& v) {
                        set_active_my(v);
                        emit my_filter_changed(v);
                    });
    add_choice_menu(tr("Language"), combo_items(lang_filter_combo_), combo_value(lang_filter_combo_),
                    [this](const QString& v) {
                        set_active_language(v);
                        emit language_filter_changed(v);
                    });
    add_choice_menu(tr("Region"), combo_items(region_combo_), combo_value(region_combo_), [this](const QString& v) {
        set_active_region(v);
        emit region_filter_changed(v);
    });
    add_choice_menu(tr("Coverage"), combo_items(variant_combo_), variant_combo_->currentText(),
                    [this](const QString& v) {
                        set_active_variant(v);
                        emit variant_changed(v);
                    });
    QVector<QPair<QString, QString>> cadence;
    for (int i = 0; i < refresh_combo_->count(); ++i)
        cadence.append({QString::number(refresh_combo_->itemData(i).toInt()), refresh_combo_->itemText(i)});
    add_choice_menu(tr("Auto-refresh"), cadence, QString::number(refresh_combo_->currentData().toInt()),
                    [this](const QString& v) {
                        set_refresh_interval_minutes(v.toInt());
                        emit refresh_interval_changed(v.toInt());
                    });
    menu.addSeparator();
    QAction* unread = menu.addAction(tr("Unread only"));
    unread->setCheckable(true);
    unread->setChecked(unread_btn_->isChecked());
    connect(unread, &QAction::toggled, this, [this](bool on) { unread_btn_->setChecked(on); });
    connect(menu.addAction(tr("Mark all as read")), &QAction::triggered, this, &NewsCommandBar::mark_all_read_clicked);
    QAction* charts = menu.addAction(tr("Analytics charts"));
    charts->setCheckable(true);
    charts->setChecked(charts_btn_->isChecked());
    connect(charts, &QAction::toggled, this, [this](bool on) { charts_btn_->setChecked(on); });
    connect(menu.addAction(tr("Saved searches and alerts…")), &QAction::triggered, this,
            &NewsCommandBar::saved_searches_clicked);
    connect(menu.addAction(tr("Headline brief")), &QAction::triggered, this, &NewsCommandBar::summarize_clicked);
    menu.addSeparator();
    connect(menu.addAction(tr("Manage sources…")), &QAction::triggered, this, &NewsCommandBar::manage_sources_clicked);
    connect(menu.addAction(tr("Source health…")), &QAction::triggered, this, &NewsCommandBar::feed_health_clicked);
    QAction* rtl = menu.addAction(tr("Right-to-left layout"));
    rtl->setCheckable(true);
    rtl->setChecked(rtl_btn_->isChecked());
    connect(rtl, &QAction::toggled, this, [this](bool on) { rtl_btn_->setChecked(on); });
    menu.exec(more_btn_->mapToGlobal(QPoint(0, more_btn_->height())));
}

void NewsCommandBar::build_intel_row(QVBoxLayout* root) {
    auto* row = new QWidget(this);
    row->setObjectName("newsIntelStrip");
    intel_row_ = row;

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

    // Feed health: "84/87 OK" — click for the per-feed list.
    feed_health_btn_ = new QPushButton(row);
    feed_health_btn_->setObjectName("newsFeedHealthBtn");
    feed_health_btn_->setCursor(Qt::PointingHandCursor);
    feed_health_btn_->setFlat(true);
    feed_health_btn_->hide();
    hl->addWidget(feed_health_btn_);
    connect(feed_health_btn_, &QPushButton::clicked, this, &NewsCommandBar::feed_health_clicked);

    auto* sep1 = new QLabel("|", row);
    sep1->setObjectName("newsIntelSep");
    hl->addWidget(sep1);

    // Tone gauge — proportion bar (bull / neutral / bear / not scored) + mean
    // measured tone + counts. Stretches with the strip instead of a 50×6 sliver.
    auto* sent_container = new QWidget(row);
    auto* sent_layout = new QHBoxLayout(sent_container);
    sent_layout->setContentsMargins(0, 0, 0, 0);
    sent_layout->setSpacing(4);

    sentiment_caption_ = new QLabel(tr("TONE"), sent_container);
    sentiment_caption_->setObjectName("newsIntelLabel");
    sent_layout->addWidget(sentiment_caption_);

    auto* bar_frame = new QWidget(sent_container);
    bar_frame->setMinimumWidth(90);
    bar_frame->setMaximumWidth(220);
    bar_frame->setFixedHeight(8);
    auto* bar_layout = new QHBoxLayout(bar_frame);
    bar_layout->setContentsMargins(0, 0, 0, 0);
    bar_layout->setSpacing(1);

    sentiment_bull_ = new QWidget(bar_frame);
    sentiment_bull_->setObjectName("newsSentimentBull");
    sentiment_neut_ = new QWidget(bar_frame);
    sentiment_neut_->setObjectName("newsSentimentNeut");
    sentiment_bear_ = new QWidget(bar_frame);
    sentiment_bear_->setObjectName("newsSentimentBear");
    sentiment_unscored_ = new QWidget(bar_frame);
    sentiment_unscored_->setObjectName("newsSentimentUnscored");

    bar_layout->addWidget(sentiment_bull_, 1);
    bar_layout->addWidget(sentiment_neut_, 1);
    bar_layout->addWidget(sentiment_bear_, 1);
    bar_layout->addWidget(sentiment_unscored_, 0);

    sent_layout->addWidget(bar_frame, 1);

    sentiment_score_ = new QLabel("+0.00", sent_container);
    sentiment_score_->setObjectName("newsIntelScore");
    sent_layout->addWidget(sentiment_score_);
    sentiment_counts_ = new QLabel(sent_container);
    sentiment_counts_->setObjectName("newsIntelLabel");
    sent_layout->addWidget(sentiment_counts_);

    hl->addWidget(sent_container, 1);

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

    // Key events of the mentioned companies (earnings dates from their profiles).
    intel_events_ = new QLabel(row);
    intel_events_->setObjectName("newsIntelMonitors");
    intel_events_->hide();
    hl->addWidget(intel_events_);

    hl->addStretch();

    // Live-feed badge (right side of intel strip). Reflects the NewsService
    // WebSocket state and toggles connect/disconnect on click. Hidden by the
    // screen while no live-feed server exists (no permanent "OFFLINE").
    live_badge_ = new QPushButton(tr("OFFLINE"), row);
    live_badge_->setObjectName("newsLiveBadge");
    live_badge_->setCursor(Qt::PointingHandCursor);
    live_badge_->setToolTip(tr("Live feed status — click to toggle WebSocket connection"));
    live_badge_->setProperty("live", false); // colours: #newsLiveBadge rules in the screen's sheet
    hl->addWidget(live_badge_);
    connect(live_badge_, &QPushButton::clicked, this, &NewsCommandBar::live_toggle_clicked);

    root->addWidget(row);
}

QPushButton* NewsCommandBar::make_pill(const QString& text, const QString& value, QHBoxLayout* layout) {
    auto* btn = new QPushButton(text, this);
    btn->setObjectName("newsCommandBarPill");
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

void NewsCommandBar::set_active_variant(const QString& variant) {
    if (!variant_combo_)
        return;
    const int idx = variant_combo_->findText(variant);
    if (idx < 0)
        return;
    QSignalBlocker block(variant_combo_);
    variant_combo_->setCurrentIndex(idx);
}

void NewsCommandBar::set_active_language(const QString& lang) {
    ncb_select_data(lang_filter_combo_, lang.toUpper());
}

void NewsCommandBar::set_active_region(const QString& region) {
    ncb_select_data(region_combo_, region.toUpper());
}

void NewsCommandBar::set_active_my(const QString& value) {
    active_my_ = value;
    ncb_select_data(my_combo_, value);
}

void NewsCommandBar::set_watchlists(const QVector<QPair<QString, QString>>& lists) {
    watchlists_ = lists;
    if (!my_combo_)
        return;
    QSignalBlocker block(my_combo_);
    my_combo_->clear();
    my_combo_->addItem(tr("ALL NEWS"), QStringLiteral("ALL"));
    my_combo_->addItem(tr("MY HOLDINGS"), QStringLiteral("HOLDINGS"));
    my_combo_->addItem(tr("MY WATCHLISTS"), QStringLiteral("WATCHLISTS"));
    my_combo_->addItem(tr("HOLDINGS + WATCHLISTS"), QStringLiteral("MY"));
    for (const auto& [id, name] : lists)
        my_combo_->addItem(tr("WL: %1").arg(name), QStringLiteral("WL:") + id);
    const int idx = my_combo_->findData(active_my_);
    my_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
}

void NewsCommandBar::set_unread_only(bool on) {
    if (!unread_btn_ || unread_btn_->isChecked() == on)
        return;
    QSignalBlocker block(unread_btn_);
    unread_btn_->setChecked(on);
    unread_btn_->setProperty("active", on);
    unread_btn_->style()->unpolish(unread_btn_);
    unread_btn_->style()->polish(unread_btn_);
}

void NewsCommandBar::set_charts_checked(bool on) {
    if (!charts_btn_ || charts_btn_->isChecked() == on)
        return;
    QSignalBlocker block(charts_btn_);
    charts_btn_->setChecked(on);
    charts_btn_->setProperty("active", on);
    charts_btn_->style()->unpolish(charts_btn_);
    charts_btn_->style()->polish(charts_btn_);
}

void NewsCommandBar::set_search_text(const QString& text) {
    if (!search_input_ || search_input_->text() == text)
        return;
    if (search_debounce_)
        search_debounce_->stop();
    QSignalBlocker block(search_input_);
    search_input_->setText(text);
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
    if (summary.trimmed().isEmpty()) {
        hide_summary();
        return;
    }
    summary_label_->setText(summary);
    if (brief_scroll_)
        brief_scroll_->hide();
    summary_label_->show();
    summary_row_->show();
    // Grow the bar to fit the wrapped brief (capped so a long summary can't
    // swallow the feed), then restore the fixed height on dismiss.
    const int text_w = qMax(120, width() - 40);
    const int text_h = summary_label_->heightForWidth(text_w);
    setFixedHeight(base_height_ + qBound(20, text_h + 10, 140));
    summarize_btn_->setText(tr("BRIEF"));
    summarize_btn_->setEnabled(true);
}

void NewsCommandBar::show_brief(const QVector<services::NewsArticle>& items) {
    auto* scroll = qobject_cast<QScrollArea*>(brief_scroll_);
    if (items.isEmpty() || !scroll) {
        show_summary(tr("No headlines to brief for the current filter."));
        return;
    }
    namespace fmtx = fincept::ui::formatting;

    // Group by category, keeping the importance order inside and between groups.
    QStringList categories;
    QHash<QString, QVector<services::NewsArticle>> by_cat;
    int urgent = 0;
    int high_impact = 0;
    for (const auto& a : items) {
        const QString cat = a.category.trimmed().isEmpty() ? tr("GENERAL") : a.category.trimmed().toUpper();
        if (!by_cat.contains(cat))
            categories.append(cat);
        by_cat[cat].append(a);
        if (a.priority == services::Priority::FLASH || a.priority == services::Priority::URGENT)
            ++urgent;
        else if (a.impact == services::Impact::HIGH)
            ++high_impact;
    }

    auto* card = new ui::Card(tr("HEADLINE BRIEF — TOP %1 OF THE CURRENT FILTER").arg(items.size()));
    connect(card, &ui::Card::close_requested, this, &NewsCommandBar::hide_summary);
    auto* cl = card->content_layout();
    cl->setSpacing(4);

    auto* kpis = new ui::KpiStrip(card);
    kpis->set_max_columns(4);
    auto add_kpi = [kpis](const QString& label, int value, ui::StatTile::Tone tone) {
        auto* t = kpis->add_tile(label);
        t->set_compact(true);
        t->set_value(QLocale().toString(value));
        t->set_tone(tone);
    };
    add_kpi(tr("HEADLINES"), static_cast<int>(items.size()), ui::StatTile::Tone::Neutral);
    add_kpi(tr("CATEGORIES"), static_cast<int>(categories.size()), ui::StatTile::Tone::Neutral);
    add_kpi(tr("URGENT"), urgent, urgent > 0 ? ui::StatTile::Tone::Negative : ui::StatTile::Tone::Neutral);
    add_kpi(tr("HIGH IMPACT"), high_impact, high_impact > 0 ? ui::StatTile::Tone::Warning : ui::StatTile::Tone::Neutral);
    cl->addWidget(kpis);

    for (const QString& cat : std::as_const(categories)) {
        auto* head = new QLabel(cat, card);
        head->setStyleSheet(QString("color:%1; font-weight:700; letter-spacing:1px; padding-top:4px; background:transparent;")
                                .arg(ui::colors::TEXT_TERTIARY()));
        cl->addWidget(head);
        for (const auto& a : by_cat.value(cat)) {
            auto* row = new QWidget(card);
            auto* hl = new QHBoxLayout(row);
            hl->setContentsMargins(0, 0, 0, 0);
            hl->setSpacing(6);
            // Urgency / impact badge in the status colours.
            if (a.priority == services::Priority::FLASH || a.priority == services::Priority::URGENT) {
                auto* badge = new ui::StatusBadge(row);
                badge->set_custom(services::priority_string(a.priority), ui::colors::DANGER());
                hl->addWidget(badge);
            } else if (a.impact == services::Impact::HIGH) {
                auto* badge = new ui::StatusBadge(row);
                badge->set_custom(tr("HIGH IMPACT"), ui::colors::WARNING());
                hl->addWidget(badge);
            }
            // The headline opens the article (same style as the RELATED list).
            auto* link = new QPushButton(QString(a.headline.trimmed()).replace('&', "&&"), row);
            link->setObjectName("newsRelatedBtn");
            link->setCursor(Qt::PointingHandCursor);
            link->setToolTip(a.headline);
            link->setAccessibleName(tr("Open article: %1").arg(a.headline.left(80)));
            link->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            const services::NewsArticle article = a;
            connect(link, &QPushButton::clicked, this, [this, article]() { emit brief_article_clicked(article); });
            hl->addWidget(link, 1);
            QStringList meta;
            if (!a.source.isEmpty())
                meta << a.source.toUpper();
            if (a.sort_ts > 0 && !a.undated) // an undated item has no age to show
                meta << fmtx::relative(static_cast<qint64>(a.sort_ts));
            auto* meta_lbl = new QLabel(meta.join(QStringLiteral(" · ")), row);
            meta_lbl->setTextFormat(Qt::PlainText);
            meta_lbl->setStyleSheet(QString("color:%1; background:transparent;").arg(ui::colors::TEXT_TERTIARY()));
            hl->addWidget(meta_lbl);
            cl->addWidget(row);
        }
    }

    // setWidget() deletes the previous brief's card.
    scroll->setWidget(card);
    summary_label_->hide();
    summary_close_btn_->hide(); // the card has its own close button
    scroll->show();
    summary_row_->show();
    const int want = card->sizeHint().height() + 8;
    setFixedHeight(base_height_ + qBound(80, want, 320));
    summarize_btn_->setText(tr("BRIEF"));
    summarize_btn_->setEnabled(true);
}

void NewsCommandBar::hide_summary() {
    if (summary_row_)
        summary_row_->hide();
    if (summary_close_btn_)
        summary_close_btn_->show();
    setFixedHeight(base_height_);
}

void NewsCommandBar::set_summarizing(bool busy) {
    summarize_btn_->setText(busy ? tr("...") : tr("BRIEF"));
    summarize_btn_->setEnabled(!busy);
}

// ── Intel strip updates ────────────────────────────────────────────────────

void NewsCommandBar::update_stats(int feeds, int articles, int clusters, int sources) {
    intel_feeds_->setText(QString::number(feeds));
    intel_articles_->setText(QString::number(articles));
    intel_clusters_->setText(QString::number(clusters));
    intel_sources_->setText(QString::number(sources));
}

void NewsCommandBar::update_sentiment(int bullish, int bearish, int neutral, int not_scored, double mean_tone) {
    const int scored_total = bullish + bearish + neutral;
    const int total = std::max(1, scored_total + not_scored);

    auto share = [total](int n) { return n > 0 ? std::max(1, n * 100 / total) : 0; };
    const int bull_w = share(bullish);
    const int bear_w = share(bearish);
    const int unscored_w = share(not_scored);
    const int neut_w = std::max(0, 100 - bull_w - bear_w - unscored_w);

    auto* bar_layout = sentiment_bull_->parentWidget()->layout();
    if (auto* hl = qobject_cast<QHBoxLayout*>(bar_layout)) {
        hl->setStretch(0, bull_w);
        hl->setStretch(1, neut_w);
        hl->setStretch(2, bear_w);
        hl->setStretch(3, unscored_w);
    }
    sentiment_bull_->setVisible(bull_w > 0);
    sentiment_bear_->setVisible(bear_w > 0);
    sentiment_unscored_->setVisible(unscored_w > 0);

    // Mean of the MEASURED tones — not-scored (non-English) articles are left
    // out instead of counting as neutral.
    if (std::isnan(mean_tone)) {
        sentiment_score_->setText(QStringLiteral("—"));
    } else {
        sentiment_score_->setText(QString("%1%2").arg(mean_tone >= 0 ? "+" : "").arg(mean_tone, 0, 'f', 2));
    }
    sentiment_counts_->setText(tr("%1▲ %2▼").arg(bullish).arg(bearish));
    const QString tip = tr("Feed tone: %1 bullish · %2 bearish · %3 neutral · %4 not scored.\n"
                           "Score = mean finance-VADER tone of the scored articles in the current filter "
                           "(-1 bearish … +1 bullish).\n"
                           "Only English headlines are scored; other languages are shown as not scored.")
                            .arg(bullish)
                            .arg(bearish)
                            .arg(neutral)
                            .arg(not_scored);
    sentiment_score_->setToolTip(tip);
    sentiment_counts_->setToolTip(tip);
    if (auto* bar = sentiment_bull_->parentWidget())
        bar->setToolTip(tip);
}

void NewsCommandBar::update_deviations(const QVector<QPair<QString, double>>& deviations) {
    if (deviations.isEmpty()) {
        intel_deviations_->hide();
        return;
    }
    QStringList parts;
    for (int i = 0; i < std::min(3, static_cast<int>(deviations.size())); ++i)
        parts << QString("%1 +%2σ").arg(deviations[i].first.left(4)).arg(deviations[i].second, 0, 'f', 1);
    intel_deviations_->setText(parts.join("  "));
    intel_deviations_->setToolTip(tr("News-volume spikes: last-hour article count vs the category's hourly "
                                     "baseline (stored history of the last 7 days), in standard deviations"));
    intel_deviations_->show();
}

void NewsCommandBar::update_feed_health(int ok, int total, int backing_off, const QString& tooltip) {
    if (total <= 0) {
        feed_health_btn_->hide();
        return;
    }
    const bool degraded = ok < total;
    feed_health_btn_->setText(backing_off > 0 ? tr("SRC %1/%2 · %3 PAUSED").arg(ok).arg(total).arg(backing_off)
                                              : tr("SRC %1/%2").arg(ok).arg(total));
    feed_health_btn_->setProperty("degraded", degraded);
    feed_health_btn_->style()->unpolish(feed_health_btn_);
    feed_health_btn_->style()->polish(feed_health_btn_);
    feed_health_btn_->setToolTip(tooltip);
    feed_health_btn_->show();
}

void NewsCommandBar::update_events(const QString& text, const QString& tooltip) {
    intel_events_->setText(text);
    intel_events_->setToolTip(tooltip);
    intel_events_->setVisible(!text.isEmpty());
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
    live_badge_->setText(connected ? tr("● LIVE") : tr("OFFLINE"));
    live_badge_->setProperty("live", connected);
    live_badge_->style()->unpolish(live_badge_);
    live_badge_->style()->polish(live_badge_);
}

void NewsCommandBar::set_drawer_checked(bool checked) {
    if (drawer_btn_ && drawer_btn_->isChecked() != checked) {
        QSignalBlocker block(drawer_btn_);
        drawer_btn_->setChecked(checked);
    }
}

void NewsCommandBar::update_monitor_summary(int total_monitors, int active_alerts) {
    if (active_alerts > 0)
        intel_monitors_->setText(tr("%1 WATCHES  %2 HIT").arg(total_monitors).arg(active_alerts));
    else
        intel_monitors_->setText(tr("%1 WATCHES").arg(total_monitors));
}

} // namespace fincept::screens
