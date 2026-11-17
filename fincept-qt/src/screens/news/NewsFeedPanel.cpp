#include "screens/news/NewsFeedPanel.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "storage/repositories/WatchlistRepository.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QDateTime>
#include <QDesktopServices>
#include <QFontMetrics>
#include <QMenu>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QScrollBar>
#include <QToolTip>
#include <QUrl>

#if defined(Q_OS_WIN)
#    include <windows.h>
#endif

namespace fincept::screens {

NewsFeedPanel::NewsFeedPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("newsFeedPanel");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Breaking banner (hidden by default)
    build_breaking_banner();
    root->addWidget(banner_widget_);

    // Symbol context bar (hidden until the feed is narrowed to a symbol or the
    // link group offers one) — above every page of the stack below.
    build_context_bar();
    root->addWidget(context_bar_);

    // Use a QStackedWidget so skeleton and list can swap cleanly
    auto* stack = new QStackedWidget(this);
    stack_ = stack; // pages are created inside it (no re-parenting / re-styling)

    // QListView with model/delegate
    model_ = new NewsFeedModel(this);
    delegate_ = new NewsFeedDelegate(this);

    list_view_ = new QListView(stack);
    list_view_->setObjectName("newsFeedList");
    list_view_->setModel(model_);
    list_view_->setItemDelegate(delegate_);
    list_view_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list_view_->setSelectionMode(QAbstractItemView::NoSelection);
    list_view_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list_view_->setMouseTracking(true);
    list_view_->setFrameShape(QFrame::NoFrame);
    list_view_->setUniformItemSizes(true); // WIRE mode: all one height — big perf win (off for cluster cards)

    // Skeleton loading widget
    build_skeleton();

    // Empty-state widget — three kinds (no articles / filtered out / error), see EmptyKind.
    empty_state_ = new QWidget(stack);
    empty_state_->setObjectName("newsEmptyState");
    {
        auto* layout = new QVBoxLayout(empty_state_);
        layout->setContentsMargins(24, 24, 24, 24);
        layout->setSpacing(8);
        layout->addStretch();
        empty_state_title_ = new QLabel(empty_state_);
        empty_state_title_->setObjectName("newsEmptyStateTitle");
        empty_state_title_->setAlignment(Qt::AlignCenter);
        empty_state_hint_ = new QLabel(empty_state_);
        empty_state_hint_->setObjectName("newsEmptyStateHint");
        empty_state_hint_->setAlignment(Qt::AlignCenter);
        empty_state_hint_->setWordWrap(true);
        empty_state_detail_ = new QLabel(empty_state_);
        empty_state_detail_->setObjectName("newsEmptyStateHint");
        empty_state_detail_->setAlignment(Qt::AlignCenter);
        empty_state_detail_->setWordWrap(true);
        empty_state_detail_->setTextFormat(Qt::PlainText); // feed names / server errors
        layout->addWidget(empty_state_title_);
        layout->addWidget(empty_state_hint_);
        layout->addWidget(empty_state_detail_);
        auto* buttons = new QWidget(empty_state_);
        auto* bl = new QHBoxLayout(buttons);
        bl->setContentsMargins(0, 6, 0, 0);
        bl->setSpacing(8);
        bl->addStretch();
        empty_clear_btn_ = new QPushButton(empty_state_);
        empty_clear_btn_->setObjectName("newsCommandBarRefresh");
        empty_clear_btn_->setCursor(Qt::PointingHandCursor);
        connect(empty_clear_btn_, &QPushButton::clicked, this, &NewsFeedPanel::clear_filters_requested);
        empty_retry_btn_ = new QPushButton(empty_state_);
        empty_retry_btn_->setObjectName("newsCommandBarRefresh");
        empty_retry_btn_->setCursor(Qt::PointingHandCursor);
        connect(empty_retry_btn_, &QPushButton::clicked, this, &NewsFeedPanel::retry_requested);
        empty_sources_btn_ = new QPushButton(empty_state_);
        empty_sources_btn_->setObjectName("newsCommandBarSources");
        empty_sources_btn_->setCursor(Qt::PointingHandCursor);
        connect(empty_sources_btn_, &QPushButton::clicked, this, &NewsFeedPanel::manage_sources_requested);
        empty_show_all_btn_ = new QPushButton(empty_state_);
        empty_show_all_btn_->setObjectName("newsCommandBarRefresh");
        empty_show_all_btn_->setCursor(Qt::PointingHandCursor);
        connect(empty_show_all_btn_, &QPushButton::clicked, this, &NewsFeedPanel::context_clear_requested);
        empty_search_btn_ = new QPushButton(empty_state_);
        empty_search_btn_->setObjectName("newsCommandBarRefresh");
        empty_search_btn_->setCursor(Qt::PointingHandCursor);
        connect(empty_search_btn_, &QPushButton::clicked, this,
                [this]() { emit symbol_text_search_requested(empty_symbol_); });
        bl->addWidget(empty_show_all_btn_);
        bl->addWidget(empty_search_btn_);
        bl->addWidget(empty_clear_btn_);
        bl->addWidget(empty_retry_btn_);
        bl->addWidget(empty_sources_btn_);
        bl->addStretch();
        layout->addWidget(buttons);
        layout->addStretch();
        apply_empty_texts();
    }

    stack->addWidget(list_view_);        // index 0 = feed
    stack->addWidget(skeleton_overlay_); // index 1 = skeleton
    stack->addWidget(empty_state_);      // index 2 = empty state
    stack->setCurrentIndex(0);
    stack_ = stack;

    root->addWidget(stack, 1);

    // Accessibility — a news reader is exactly the surface a screen reader
    // user needs named, and the list must be reachable by keyboard.
    list_view_->setAccessibleName(tr("News feed"));
    list_view_->setAccessibleDescription(
        tr("Article list. Use the arrow keys to move, Enter to open the article in a browser."));
    list_view_->setFocusPolicy(Qt::StrongFocus);
    empty_state_->setAccessibleName(tr("Empty news feed"));
    banner_widget_->setAccessibleName(tr("Breaking news banner"));

    // Right-click: open the story / open or filter by one of its tickers.
    list_view_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(list_view_, &QWidget::customContextMenuRequested, this, &NewsFeedPanel::show_context_menu);

    // Connect clicks
    connect(list_view_, &QListView::clicked, this, &NewsFeedPanel::on_item_clicked);
    // Keyboard: Enter/Return on the highlighted row opens it, same as a click.
    connect(list_view_, &QListView::activated, this, &NewsFeedPanel::on_item_clicked);

    // Preserve the reading position across model resets (see the header note).
    connect(model_, &QAbstractItemModel::modelAboutToBeReset, this, &NewsFeedPanel::capture_scroll_anchor);
    connect(model_, &QAbstractItemModel::modelReset, this, &NewsFeedPanel::restore_scroll_anchor);

    // Scroll-to-bottom detection for lazy loading
    connect(list_view_->verticalScrollBar(), &QScrollBar::valueChanged, this, &NewsFeedPanel::check_scroll_position);

    // Banner dismiss timer
    banner_dismiss_timer_ = new QTimer(this);
    banner_dismiss_timer_->setSingleShot(true);
    connect(banner_dismiss_timer_, &QTimer::timeout, this, &NewsFeedPanel::clear_breaking);
}

void NewsFeedPanel::build_breaking_banner() {
    banner_widget_ = new QWidget(this);
    banner_widget_->setObjectName("newsBreakingBanner");
    // Minimums, not fixed sizes: the banner and its tag grow with the user's font.
    banner_widget_->setMinimumHeight(28);
    banner_widget_->hide();

    auto* layout = new QHBoxLayout(banner_widget_);
    layout->setContentsMargins(8, 0, 8, 0);
    layout->setSpacing(8);

    banner_tag_ = new QLabel(tr("FLASH"), banner_widget_);
    banner_tag_->setObjectName("newsBreakingTag");
    banner_tag_->setMinimumWidth(48);
    banner_tag_->setAlignment(Qt::AlignCenter);

    // Headline and source come straight from the feed: pin them to plain text so
    // a headline that looks like markup can't be rendered as rich text.
    banner_headline_ = new QLabel(banner_widget_);
    banner_headline_->setObjectName("newsBreakingHeadline");
    banner_headline_->setTextFormat(Qt::PlainText);

    banner_source_ = new QLabel(banner_widget_);
    banner_source_->setObjectName("newsBreakingSource");
    banner_source_->setTextFormat(Qt::PlainText);
    banner_source_->setFixedWidth(std::max(80, QFontMetrics(ui::fonts::font(-3)).horizontalAdvance(QString(10, QLatin1Char('M')))));
    banner_source_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto* dismiss_btn = new QPushButton("x", banner_widget_);
    dismiss_btn->setObjectName("newsBreakingDismiss");
    dismiss_btn->setMinimumSize(20, 20);
    dismiss_btn->setToolTip(tr("Dismiss"));
    connect(dismiss_btn, &QPushButton::clicked, this, &NewsFeedPanel::clear_breaking);

    // Pager over several breaking stories (‹ 1/3 ›).
    banner_prev_ = new QPushButton(QString::fromUtf8("\xe2\x80\xb9"), banner_widget_);
    banner_prev_->setObjectName("newsBreakingDismiss");
    banner_prev_->setMinimumSize(20, 20);
    banner_prev_->setToolTip(tr("Previous breaking story"));
    connect(banner_prev_, &QPushButton::clicked, this, [this]() { show_banner_item(banner_index_ - 1); });
    banner_pos_ = new QLabel(banner_widget_);
    banner_pos_->setObjectName("newsBreakingSource");
    banner_next_ = new QPushButton(QString::fromUtf8("\xe2\x80\xba"), banner_widget_);
    banner_next_->setObjectName("newsBreakingDismiss");
    banner_next_->setMinimumSize(20, 20);
    banner_next_->setToolTip(tr("Next breaking story"));
    connect(banner_next_, &QPushButton::clicked, this, [this]() { show_banner_item(banner_index_ + 1); });

    layout->addWidget(banner_tag_);
    layout->addWidget(banner_headline_, 1);
    layout->addWidget(banner_source_);
    layout->addWidget(banner_prev_);
    layout->addWidget(banner_pos_);
    layout->addWidget(banner_next_);
    layout->addWidget(dismiss_btn);

    // The whole banner (tag, headline, source) opens the story; the labels do
    // not take mouse input, so a press anywhere but "x" reaches the banner.
    banner_widget_->setCursor(Qt::PointingHandCursor);
    banner_widget_->setToolTip(tr("Open this story"));
    banner_widget_->installEventFilter(this);
}

bool NewsFeedPanel::eventFilter(QObject* watched, QEvent* event) {
    if (watched == banner_widget_ && event->type() == QEvent::MouseButtonRelease) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton && !banner_article_.id.isEmpty()) {
            const services::NewsArticle article = banner_article_;
            emit article_clicked(article);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void NewsFeedPanel::show_banner_item(int index) {
    if (banner_items_.isEmpty())
        return;
    const int n = static_cast<int>(banner_items_.size());
    banner_index_ = ((index % n) + n) % n;
    const auto& lead = banner_items_[banner_index_];
    banner_tag_->setText(lead.priority == services::Priority::FLASH ? tr("FLASH") : tr("BREAKING"));
    banner_headline_->setText(lead.headline);
    banner_headline_->setToolTip(lead.headline);
    banner_source_->setText(lead.source.toUpper());
    banner_article_ = lead;
    const bool several = n > 1;
    banner_prev_->setVisible(several);
    banner_next_->setVisible(several);
    banner_pos_->setVisible(several);
    banner_pos_->setText(QStringLiteral("%1/%2").arg(banner_index_ + 1).arg(n));
}

void NewsFeedPanel::show_breaking(const QVector<services::NewsCluster>& breaking_clusters) {
    if (breaking_clusters.isEmpty())
        return;

    int64_t now = QDateTime::currentSecsSinceEpoch();

    // Global cooldown
    if (now < global_cooldown_until_)
        return;

    // The freshest breaking stories not shown recently (the clusters arrive
    // breaking-first, newest first).
    QVector<services::NewsArticle> items;
    QStringList keys;
    for (const auto& cluster : breaking_clusters) {
        const auto& lead = cluster.lead_article;
        const QString key = lead.headline.left(50).toLower();
        if (is_banner_duplicate(key) || keys.contains(key))
            continue;
        items.append(lead);
        keys.append(key);
        if (items.size() >= kMaxBannerItems)
            break;
    }
    if (items.isEmpty())
        return;
    const auto lead = items.first();

    banner_items_ = items;
    show_banner_item(0);
    banner_widget_->show();

    // Record for dedup
    for (const auto& key : std::as_const(keys))
        recent_banners_.append({key, now});
    global_cooldown_until_ = now + BANNER_GLOBAL_COOLDOWN_SEC;

    // Cleanup old entries
    QVector<BreakingEntry> fresh;
    for (const auto& entry : recent_banners_) {
        if (now - entry.shown_at < BANNER_DEDUP_WINDOW_SEC)
            fresh.append(entry);
    }
    recent_banners_ = fresh;

    // Auto-dismiss: 60s for FLASH, 30s for BREAKING
    int dismiss_ms = (lead.priority == services::Priority::FLASH) ? 60000 : 30000;
    banner_dismiss_timer_->start(dismiss_ms);

    // Sound notification with 5-min cooldown
    if (now - last_sound_at_ >= SOUND_COOLDOWN_SEC) {
        last_sound_at_ = now;
#if defined(Q_OS_WIN)
        MessageBeep(MB_ICONEXCLAMATION);
#else
        QApplication::beep();
#endif
    }

    LOG_DEBUG("NewsFeedPanel", QString("Breaking banner: %1").arg(lead.headline.left(60)));
}

void NewsFeedPanel::clear_breaking() {
    banner_widget_->hide();
    banner_dismiss_timer_->stop();
}

bool NewsFeedPanel::is_banner_duplicate(const QString& headline) const {
    int64_t now = QDateTime::currentSecsSinceEpoch();
    for (const auto& entry : recent_banners_) {
        if (now - entry.shown_at < BANNER_DEDUP_WINDOW_SEC && entry.headline_key == headline)
            return true;
    }
    return false;
}

void NewsFeedPanel::build_skeleton() {
    // The loading page. It used to be 20 bare rows whose only colour was a 4-8 %
    // white veil applied by a timer half a second after it appeared — on the
    // dark theme an empty black area, often for the whole fetch. Now it says
    // what is happening from the first frame: a title, live progress, and —
    // when articles have arrived but the filters hide them all — a way out.
    skeleton_overlay_ = new QWidget(stack_ ? static_cast<QWidget*>(stack_) : this);
    skeleton_overlay_->setObjectName("newsSkeletonOverlay");
    // No hide() — managed by QStackedWidget

    auto* skel_layout = new QVBoxLayout(skeleton_overlay_);
    skel_layout->setContentsMargins(0, 0, 0, 0);
    skel_layout->setSpacing(1);

    auto* head = new QWidget(skeleton_overlay_);
    head->setObjectName("newsLoadingHead");
    head->setAttribute(Qt::WA_StyledBackground, true);
    auto* head_layout = new QVBoxLayout(head);
    head_layout->setContentsMargins(12, 10, 12, 10);
    head_layout->setSpacing(4);
    loading_title_ = new QLabel(head);
    loading_title_->setObjectName("newsLoadingTitle");
    loading_progress_ = new QLabel(head);
    loading_progress_->setObjectName("newsLoadingHint");
    loading_filtered_note_ = new QLabel(head);
    loading_filtered_note_->setObjectName("newsLoadingHint");
    loading_filtered_note_->setWordWrap(true);
    loading_clear_btn_ = new QPushButton(head);
    loading_clear_btn_->setObjectName("newsCommandBarRefresh");
    loading_clear_btn_->setCursor(Qt::PointingHandCursor);
    connect(loading_clear_btn_, &QPushButton::clicked, this, &NewsFeedPanel::clear_filters_requested);
    auto* note_row = new QHBoxLayout();
    note_row->setContentsMargins(0, 0, 0, 0);
    note_row->setSpacing(8);
    note_row->addWidget(loading_filtered_note_, 1);
    note_row->addWidget(loading_clear_btn_, 0, Qt::AlignVCenter);
    head_layout->addWidget(loading_title_);
    head_layout->addWidget(loading_progress_);
    head_layout->addLayout(note_row);
    loading_filtered_note_->hide();
    loading_clear_btn_->hide();
    skel_layout->addWidget(head);

    // Placeholder rows, the height of a WIRE row of NewsFeedDelegate.
    const int row_h = std::max(26, QFontMetrics(ui::fonts::font(-2)).height() + 10);
    for (int i = 0; i < 12; ++i) {
        auto* row = new QWidget(skeleton_overlay_);
        row->setFixedHeight(row_h);
        row->setObjectName("newsSkeletonRow");
        row->setAttribute(Qt::WA_StyledBackground, true);
        skel_layout->addWidget(row);
    }
    skel_layout->addStretch();
    skeleton_overlay_->setAccessibleName(tr("Loading news"));
    apply_loading_texts();
}

void NewsFeedPanel::apply_loading_texts() {
    if (!loading_title_)
        return;
    loading_title_->setText(tr("LOADING NEWS"));
    QString progress;
    if (loading_total_ > 0)
        progress = tr("%1 of %2 sources answered").arg(loading_done_).arg(loading_total_);
    else
        progress = tr("Contacting news sources…");
    if (loading_articles_ > 0)
        progress += QStringLiteral(" · ") + tr("%n article(s) received", "", loading_articles_);
    loading_progress_->setText(progress);
    loading_filtered_note_->setText(
        tr("Articles have arrived, but none match the current filters (time range, category, region, language, "
           "search). More sources are still loading."));
    loading_clear_btn_->setText(tr("CLEAR FILTERS"));
}

void NewsFeedPanel::remove_skeleton() {
    stack_->setCurrentWidget(list_view_);
}

void NewsFeedPanel::set_loading(bool loading) {
    is_loading_ = loading;
    if (loading) {
        stack_->setCurrentWidget(skeleton_overlay_);
    } else {
        loading_done_ = loading_total_ = loading_articles_ = 0;
        set_loading_filtered_out(false);
        apply_loading_texts();
        if (stack_->currentWidget() == skeleton_overlay_)
            remove_skeleton();
    }
}

void NewsFeedPanel::set_loading_progress(int sources_done, int sources_total, int articles) {
    if (loading_done_ == sources_done && loading_total_ == sources_total && loading_articles_ == articles)
        return;
    loading_done_ = sources_done;
    loading_total_ = sources_total;
    loading_articles_ = articles;
    apply_loading_texts();
}

void NewsFeedPanel::set_loading_filtered_out(bool filtered_out) {
    if (!loading_filtered_note_)
        return;
    loading_filtered_note_->setVisible(filtered_out);
    loading_clear_btn_->setVisible(filtered_out);
}

void NewsFeedPanel::show_list() {
    is_loading_ = false;
    set_loading_filtered_out(false);
    if (stack_->currentWidget() != list_view_)
        stack_->setCurrentWidget(list_view_);
}

void NewsFeedPanel::set_empty_state(bool empty, EmptyKind kind, const QString& detail) {
    // The screen decides between list / loading / empty; an empty state ends
    // the loading page.
    if (empty) {
        is_loading_ = false;
        empty_kind_ = kind;
        // The symbol empty state names its symbol in the title, not a detail line.
        empty_symbol_ = kind == EmptyKind::Symbol ? detail : QString();
        empty_state_detail_->setText(kind == EmptyKind::Symbol ? QString() : detail);
        empty_state_detail_->setVisible(kind != EmptyKind::Symbol && !detail.isEmpty());
        apply_empty_texts();
        stack_->setCurrentWidget(empty_state_);
    } else if (stack_->currentWidget() == empty_state_) {
        stack_->setCurrentWidget(list_view_);
    }
}

void NewsFeedPanel::apply_empty_texts() {
    switch (empty_kind_) {
        case EmptyKind::Filtered:
            empty_state_title_->setText(tr("No articles match the current filters"));
            empty_state_hint_->setText(tr("Widen the time range, pick another category, region or language, or "
                                          "clear the search."));
            break;
        case EmptyKind::Error:
            empty_state_title_->setText(tr("News could not be loaded"));
            empty_state_hint_->setText(tr("Every enabled feed failed on the last refresh. Check the network, then "
                                          "retry; failing feeds are listed below."));
            break;
        case EmptyKind::NoArticles:
            empty_state_title_->setText(tr("No articles yet"));
            empty_state_hint_->setText(tr("The enabled feeds returned no articles. Enable more sources or refresh."));
            break;
        case EmptyKind::Symbol:
            empty_state_title_->setText(tr("No stories tagged %1").arg(empty_symbol_));
            empty_state_hint_->setText(tr("No article in the current time range and filters names %1 as a security. "
                                          "Show all news, widen the time range, or search the headlines for it as "
                                          "text.")
                                           .arg(empty_symbol_));
            break;
    }
    const bool symbol = empty_kind_ == EmptyKind::Symbol;
    const bool problem = empty_kind_ == EmptyKind::NoArticles || empty_kind_ == EmptyKind::Error;
    empty_clear_btn_->setText(tr("CLEAR FILTERS"));
    empty_retry_btn_->setText(tr("RETRY"));
    empty_sources_btn_->setText(tr("SOURCES"));
    empty_show_all_btn_->setText(tr("SHOW ALL NEWS"));
    empty_search_btn_->setText(tr("SEARCH HEADLINES"));
    empty_search_btn_->setToolTip(tr("Search the headlines for \"%1\" as text").arg(empty_symbol_));
    empty_clear_btn_->setVisible(empty_kind_ == EmptyKind::Filtered);
    empty_retry_btn_->setVisible(problem);
    empty_sources_btn_->setVisible(problem);
    empty_show_all_btn_->setVisible(symbol);
    empty_search_btn_->setVisible(symbol);
}

// ── Symbol context bar ──────────────────────────────────────────────────────

void NewsFeedPanel::build_context_bar() {
    context_bar_ = new QWidget(this);
    context_bar_->setObjectName("newsContextBar");
    context_bar_->setAttribute(Qt::WA_StyledBackground, true);
    context_bar_->setMinimumHeight(28);
    context_bar_->hide();

    auto* layout = new QHBoxLayout(context_bar_);
    layout->setContentsMargins(8, 2, 6, 2);
    layout->setSpacing(6);

    context_lead_ = new QLabel(context_bar_);
    context_lead_->setObjectName("newsContextLead");
    context_symbol_ = new QLabel(context_bar_);
    context_symbol_->setObjectName("newsContextSymbol");
    context_symbol_->setTextFormat(Qt::PlainText);
    context_note_ = new QLabel(context_bar_);
    context_note_->setObjectName("newsContextNote");
    context_note_->setTextFormat(Qt::PlainText);
    // The note gives way first on a narrow feed; the buttons stay reachable.
    context_note_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    context_note_->setMinimumWidth(0);

    context_show_btn_ = new QPushButton(context_bar_);
    context_show_btn_->setObjectName("newsCommandBarRefresh");
    context_show_btn_->setCursor(Qt::PointingHandCursor);
    connect(context_show_btn_, &QPushButton::clicked, this, &NewsFeedPanel::context_show_requested);

    context_follow_btn_ = new QPushButton(context_bar_);
    context_follow_btn_->setObjectName("newsContextFollow");
    context_follow_btn_->setCheckable(true);
    context_follow_btn_->setCursor(Qt::PointingHandCursor);
    connect(context_follow_btn_, &QPushButton::toggled, this, &NewsFeedPanel::follow_link_toggled);

    context_clear_btn_ = new QPushButton(context_bar_);
    context_clear_btn_->setObjectName("newsCommandBarRefresh");
    context_clear_btn_->setCursor(Qt::PointingHandCursor);
    connect(context_clear_btn_, &QPushButton::clicked, this, &NewsFeedPanel::context_clear_requested);

    layout->addWidget(context_lead_);
    layout->addWidget(context_symbol_);
    layout->addWidget(context_note_, 1);
    layout->addWidget(context_show_btn_);
    layout->addWidget(context_follow_btn_);
    layout->addWidget(context_clear_btn_);
    context_bar_->setAccessibleName(tr("Symbol filter"));
}

void NewsFeedPanel::set_context_bar(const ContextBar& state) {
    context_ = state;
    if (state.mode == ContextBar::Mode::Hidden || state.symbol.isEmpty()) {
        context_bar_->hide();
        return;
    }
    apply_context_texts();
    context_bar_->show();
}

void NewsFeedPanel::apply_context_texts() {
    if (!context_bar_)
        return;
    const bool active = context_.mode == ContextBar::Mode::Active;
    const bool linked = !context_.group.isNull();
    context_symbol_->setText(context_.symbol);
    if (active) {
        context_lead_->setText(tr("NEWS FOR"));
        QString note;
        if (context_.from_link && context_.fallback)
            note = tr("nothing tagged %1 yet — showing all news").arg(context_.symbol);
        else if (context_.from_link)
            note = tr("following link group %1").arg(context_.group);
        else
            note = tr("stories tagged with this symbol");
        context_note_->setText(note);
        context_note_->setToolTip(note);
        context_clear_btn_->setText(tr("SHOW ALL NEWS"));
        context_clear_btn_->setToolTip(tr("Remove the symbol filter"));
    } else {
        context_lead_->setText(tr("GROUP %1 IS ON").arg(context_.group));
        const QString note = tr("the feed is not filtered");
        context_note_->setText(note);
        context_note_->setToolTip(tr("Another panel in link group %1 selected %2. The news feed only narrows to it "
                                     "when you ask (SHOW ITS NEWS) or follow the link group.")
                                      .arg(context_.group)
                                      .arg(context_.symbol));
        context_clear_btn_->setText(QString(QChar(0x00D7))); // ×
        context_clear_btn_->setToolTip(tr("Dismiss"));
    }
    context_show_btn_->setText(tr("SHOW ITS NEWS"));
    context_show_btn_->setToolTip(tr("Show only stories tagged %1").arg(context_.symbol));
    context_show_btn_->setVisible(!active);
    // FOLLOW LINK only means something while the screen is in a link group.
    context_follow_btn_->setText(tr("FOLLOW LINK"));
    context_follow_btn_->setToolTip(
        tr("Follow link group %1: when another panel in the group selects a symbol, show that symbol's news here "
           "(the whole feed stays visible while nothing is tagged with it).")
            .arg(context_.group));
    context_follow_btn_->setVisible(linked);
    {
        QSignalBlocker block(context_follow_btn_); // reflect state without re-emitting
        context_follow_btn_->setChecked(context_.follow);
    }
}

void NewsFeedPanel::set_view_mode_hint(const QString& mode) {
    list_view_->setUniformItemSizes(mode != QLatin1String("CLUSTERS"));
}

void NewsFeedPanel::capture_scroll_anchor() {
    anchor_article_id_.clear();
    current_article_id_.clear();
    anchor_was_at_top_ = true;
    if (!list_view_ || !model_)
        return;
    auto* sb = list_view_->verticalScrollBar();
    anchor_was_at_top_ = !sb || sb->value() <= 0;

    // Anchor on whatever row is currently at the top of the viewport. Doing
    // this by article id (not pixel offset) keeps the anchor correct even when
    // rows are inserted above it by the refresh.
    const QModelIndex top = list_view_->indexAt(QPoint(4, 4));
    if (top.isValid())
        anchor_article_id_ = model_->article_at(top.row()).id;

    const QModelIndex cur = list_view_->currentIndex();
    if (cur.isValid())
        current_article_id_ = model_->article_at(cur.row()).id;
}

void NewsFeedPanel::restore_scroll_anchor() {
    if (!list_view_ || !model_)
        return;
    // Restore the keyboard cursor first — setCurrentIndex() ensure-visible
    // scrolls, so doing it after the anchor scroll would undo it.
    if (!current_article_id_.isEmpty()) {
        const auto cur = model_->index_for_article(current_article_id_);
        if (cur.isValid())
            list_view_->setCurrentIndex(cur);
    }
    // At the very top the user wants to see the newest items — leave it there.
    if (anchor_was_at_top_ || anchor_article_id_.isEmpty())
        return;
    const auto anchor = model_->index_for_article(anchor_article_id_);
    if (anchor.isValid())
        list_view_->scrollTo(anchor, QAbstractItemView::PositionAtTop);
}

void NewsFeedPanel::scroll_to(const QString& article_id) {
    auto idx = model_->index_for_article(article_id);
    if (idx.isValid())
        list_view_->scrollTo(idx, QAbstractItemView::EnsureVisible);
}

void NewsFeedPanel::set_selected(const QString& article_id) {
    model_->set_selected_id(article_id);
    scroll_to(article_id);
}

void NewsFeedPanel::select_next() {
    auto current = list_view_->currentIndex();
    int next_row = current.isValid() ? current.row() + 1 : 0;
    if (next_row < model_->rowCount()) {
        auto next = model_->index(next_row, 0);
        list_view_->setCurrentIndex(next);
        list_view_->scrollTo(next);
        on_item_clicked(next);
    }
}

void NewsFeedPanel::select_previous() {
    auto current = list_view_->currentIndex();
    int prev_row = current.isValid() ? current.row() - 1 : 0;
    if (prev_row >= 0) {
        auto prev = model_->index(prev_row, 0);
        list_view_->setCurrentIndex(prev);
        list_view_->scrollTo(prev);
        on_item_clicked(prev);
    }
}

void NewsFeedPanel::on_item_clicked(const QModelIndex& index) {
    if (!index.isValid())
        return;

    auto article = model_->article_at(index.row());
    model_->set_selected_id(article.id);
    model_->mark_seen(article.id);

    emit article_clicked(article);

    if (model_->view_mode() == "CLUSTERS") {
        auto cluster = model_->cluster_at(index.row());
        emit cluster_clicked(cluster);
    }
}

void NewsFeedPanel::show_context_menu(const QPoint& pos) {
    const QModelIndex index = list_view_->indexAt(pos);
    if (!index.isValid())
        return;
    const auto article = model_->article_at(index.row());
    if (article.id.isEmpty())
        return;

    // Feed links are untrusted (cached/DB rows may predate link validation):
    // only offer web URLs.
    const QUrl url(article.link);
    const bool has_link =
        url.isValid() && (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https"));

    QMenu menu(this);
    auto* open_act = menu.addAction(tr("Open article in browser"));
    open_act->setEnabled(has_link);
    connect(open_act, &QAction::triggered, this, [url]() { QDesktopServices::openUrl(url); });
    auto* copy_act = menu.addAction(tr("Copy link"));
    copy_act->setEnabled(has_link);
    connect(copy_act, &QAction::triggered, this, [link = article.link]() { QApplication::clipboard()->setText(link); });

    if (!article.tickers.isEmpty()) {
        menu.addSeparator();
        QPointer<NewsFeedPanel> self = this;
        auto filter_cb = [self](const QString& t) {
            if (self)
                emit self->ticker_filter_requested(t);
        };
        if (article.tickers.size() == 1) {
            add_ticker_actions(&menu, article.tickers.first(), this, filter_cb);
        } else {
            // One submenu per ticker keeps a multi-ticker story's menu short.
            for (const QString& ticker : article.tickers)
                add_ticker_actions(menu.addMenu(QStringLiteral("$") + ticker), ticker, this, filter_cb);
        }
    }
    menu.exec(list_view_->viewport()->mapToGlobal(pos));
}

void NewsFeedPanel::add_ticker_actions(QMenu* menu, const QString& ticker, QWidget* host,
                                       const std::function<void(const QString&)>& filter_cb) {
    if (!menu || ticker.isEmpty())
        return;
    const QString shown = QStringLiteral("$") + ticker;
    // nav.open_symbol navigates first (constructing the target screen when needed)
    // and then hands it the ticker through IGroupLinked.
    auto open_in = [ticker](const QString& screen_id) {
        EventBus::instance().publish("nav.open_symbol", QVariantMap{{"screen_id", screen_id}, {"symbol", ticker}});
    };
    QObject::connect(menu->addAction(tr("Open %1 in Equity Research").arg(shown)), &QAction::triggered, host,
                     [open_in]() { open_in(QStringLiteral("equity_research")); });
    QObject::connect(menu->addAction(tr("Open %1 chart / order ticket (Equity Trading)").arg(shown)),
                     &QAction::triggered, host, [open_in]() { open_in(QStringLiteral("equity_trading")); });
    QObject::connect(menu->addAction(tr("Backtest %1").arg(shown)), &QAction::triggered, host,
                     [open_in]() { open_in(QStringLiteral("backtesting")); });

    // One entry per watchlist (same pattern as the Screener / Markets menus).
    QMenu* wl_menu = menu->addMenu(tr("Add %1 to Watchlist").arg(shown));
    const auto lists = fincept::WatchlistRepository::instance().list_all();
    if (!lists.is_ok() || lists.value().isEmpty()) {
        wl_menu->addAction(tr("No watchlists yet — create one on the Watchlist screen"))->setEnabled(false);
    } else {
        for (const auto& wl : lists.value()) {
            QObject::connect(wl_menu->addAction(wl.name), &QAction::triggered, host,
                             [host, ticker, list_id = wl.id, list_name = wl.name]() {
                                 auto& repo = fincept::WatchlistRepository::instance();
                                 if (const auto stocks = repo.get_stocks(list_id); stocks.is_ok()) {
                                     for (const auto& st : stocks.value()) {
                                         if (st.symbol.compare(ticker, Qt::CaseInsensitive) == 0) {
                                             QToolTip::showText(QCursor::pos(),
                                                                tr("%1 is already in %2").arg(ticker, list_name), host);
                                             return;
                                         }
                                     }
                                 }
                                 const auto r = repo.add_stock(list_id, ticker);
                                 QToolTip::showText(QCursor::pos(),
                                                    r.is_ok() ? tr("Added %1 to %2").arg(ticker, list_name)
                                                              : tr("Could not add %1 to %2").arg(ticker, list_name),
                                                    host);
                                 if (r.is_ok()) // an open Watchlist screen picks the change up
                                     EventBus::instance().publish(
                                         "watchlist.updated", QVariantMap{{"action", QStringLiteral("add")},
                                                                          {"symbol", ticker}});
                             });
        }
    }

    menu->addSeparator();
    if (filter_cb)
        QObject::connect(menu->addAction(tr("Filter feed by %1").arg(shown)), &QAction::triggered, host,
                         [filter_cb, ticker]() { filter_cb(ticker); });
    QObject::connect(menu->addAction(tr("Copy %1").arg(ticker)), &QAction::triggered, host,
                     [ticker]() { QApplication::clipboard()->setText(ticker); });
}

void NewsFeedPanel::check_scroll_position() {
    auto* sb = list_view_->verticalScrollBar();
    if (!sb)
        return;
    int remaining = sb->maximum() - sb->value();
    if (remaining < 200 && sb->maximum() > 0)
        emit near_bottom();
}

void NewsFeedPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void NewsFeedPanel::retranslateUi() {
    if (empty_state_title_)
        apply_empty_texts();
    apply_loading_texts();
    if (context_bar_ && context_bar_->isVisible())
        apply_context_texts();
    // banner_tag_ reflects the live FLASH/BREAKING priority of the current
    // banner and refreshes on the next show_breaking() — not forced here.
}

} // namespace fincept::screens
