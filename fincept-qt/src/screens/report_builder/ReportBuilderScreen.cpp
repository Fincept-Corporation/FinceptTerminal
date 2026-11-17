// ReportBuilderScreen.cpp — view onto ReportBuilderService: construction, toolbar,
// pages, lifecycle and saved state. See the header for the split.

#include "screens/report_builder/ReportBuilderScreen.h"

#include "core/report/ReportBinding.h"
#include "core/report/ReportVariables.h"
#include "core/session/ScreenStateManager.h"
#include "screens/report_builder/ReportAiPanel.h"
#include "screens/report_builder/ReportExportJob.h"
#include "screens/report_builder/ReportLibraryPage.h"
#include "services/cloud/CloudSyncEngine.h"
#include "services/report_builder/ReportBuilderService.h"
#include "services/report_builder/ReportChartCache.h"
#include "services/report_builder/ReportDataBinder.h"
#include "services/report_builder/ReportInbox.h"
#include "services/report_builder/ReportLibrary.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QPointer>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

namespace fincept::screens {

namespace rep = ::fincept::report;
using Service = ::fincept::services::ReportBuilderService;
using Binder = ::fincept::services::ReportDataBinder;

// Tracks the most recently constructed Report Builder screen so automation can find a
// live instance without walking the widget tree. QPointer auto-nulls on deletion.
static QPointer<ReportBuilderScreen> s_rb_current_screen;

ReportBuilderScreen* ReportBuilderScreen::current() {
    return s_rb_current_screen.data();
}

void ReportBuilderScreen::open_when_ready(const QString& path) {
    if (path.isEmpty())
        return;
    Service::OpenRequest req;
    req.file_path = path;
    Service::instance().request_open(req);
}

ReportBuilderScreen::ReportBuilderScreen(QWidget* parent) : QWidget(parent) {
    s_rb_current_screen = this;
    setObjectName(QStringLiteral("rbScreen"));
    // Selector-scoped, set before any child exists.
    setStyleSheet(
        QStringLiteral(
            "#rbScreen { background: %1; }"
            "#rbToolbar { background: %2; border-bottom: 1px solid %3; }"
            "#rbToolbar QLabel { background: transparent; color: %4; font-size: %5px; }"
            "#rbToolbar QLabel[rbRole=\"brand\"] { color: %6; font-weight: bold; font-size: %5px; }"
            "#rbToolbar QPushButton, #rbToolbar QToolButton { background: transparent; color: %7; border: 1px solid %3;"
            " padding: 3px 9px; font-size: %8px; }"
            "#rbToolbar QPushButton:hover, #rbToolbar QToolButton:hover { background: %9; color: %6; border-color: %6; }"
            "#rbToolbar QPushButton:disabled, #rbToolbar QToolButton:disabled { color: %4; }"
            "#rbToolbar QPushButton:checked { background: %6; color: %10; border-color: %6; }"
            "#rbToolbar QPushButton[rbRole=\"primary\"], #rbToolbar QToolButton[rbRole=\"primary\"] { color: %6;"
            " font-weight: bold; }"
            "#rbToolbar QFrame[rbRole=\"sep\"] { background: %3; border: none; }"
            "#rbNotice { background: %11; border-bottom: 1px solid %3; }"
            "#rbNotice QLabel { background: transparent; color: %7; font-size: %8px; }"
            "#rbNotice QLabel[rbTone=\"warn\"] { color: %12; }"
            "#rbNotice QLabel[rbTone=\"bad\"] { color: %13; }"
            "#rbNotice QLabel[rbTone=\"ok\"] { color: %14; }"
            "#rbNotice QPushButton { background: transparent; color: %6; border: 1px solid %6; padding: 2px 10px;"
            " font-size: %8px; font-weight: bold; }"
            "#rbNotice QPushButton[rbRole=\"close\"] { color: %4; border: none; font-weight: normal; }"
            "#rbExportBar { background: %11; border-bottom: 1px solid %3; }"
            "#rbExportBar QLabel { background: transparent; color: %7; font-size: %8px; }"
            "#rbExportBar QProgressBar { background: %1; border: 1px solid %3; height: 8px; text-align: center; }"
            "#rbExportBar QProgressBar::chunk { background: %6; }"
            "#rbExportBar QPushButton { background: transparent; color: %7; border: 1px solid %3; padding: 2px 10px;"
            " font-size: %8px; }"
            "QSplitter#rbSplitter::handle { background: %3; }"
            "QSplitter#rbRightSplit::handle { background: %3; }")
            .arg(ui::colors::BG_BASE(), ui::colors::HEADER(), ui::colors::BORDER_DIM(), ui::colors::TEXT_TERTIARY(),
                 ui::fonts::px(0), ui::colors::AMBER(), ui::colors::TEXT_PRIMARY(), ui::fonts::px(-1),
                 ui::colors::BG_HOVER())
            .arg(ui::colors::TEXT_ON_ACCENT(), ui::colors::BG_RAISED(), ui::colors::WARNING(), ui::colors::DANGER(),
                 ui::colors::SUCCESS()));

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);
    vl->addWidget(build_toolbar());

    // Export progress strip (hidden until an export runs).
    export_bar_ = new QWidget(this);
    export_bar_->setObjectName(QStringLiteral("rbExportBar"));
    export_bar_->setAttribute(Qt::WA_StyledBackground);
    auto* el = new QHBoxLayout(export_bar_);
    el->setContentsMargins(10, 4, 10, 4);
    el->setSpacing(10);
    export_lbl_ = new QLabel(export_bar_);
    el->addWidget(export_lbl_, 1);
    export_progress_ = new QProgressBar(export_bar_);
    export_progress_->setRange(0, 100);
    export_progress_->setTextVisible(false);
    export_progress_->setMaximumWidth(260);
    el->addWidget(export_progress_, 1);
    export_cancel_ = new QPushButton(tr("Cancel"), export_bar_);
    el->addWidget(export_cancel_);
    vl->addWidget(export_bar_);
    export_bar_->hide();
    connect(export_cancel_, &QPushButton::clicked, this, [this]() {
        if (export_job_)
            export_job_->cancel();
    });

    // Notice strip (new blocks from other tabs, stale data, saves, exports).
    notice_bar_ = new QWidget(this);
    notice_bar_->setObjectName(QStringLiteral("rbNotice"));
    notice_bar_->setAttribute(Qt::WA_StyledBackground);
    auto* nl = new QHBoxLayout(notice_bar_);
    nl->setContentsMargins(10, 4, 6, 4);
    nl->setSpacing(10);
    notice_lbl_ = new QLabel(notice_bar_);
    notice_lbl_->setWordWrap(true);
    nl->addWidget(notice_lbl_, 1);
    notice_action_ = new QPushButton(notice_bar_);
    nl->addWidget(notice_action_);
    notice_close_ = new QPushButton(QStringLiteral("✕"), notice_bar_);
    notice_close_->setProperty("rbRole", QStringLiteral("close"));
    notice_close_->setToolTip(tr("Dismiss"));
    nl->addWidget(notice_close_);
    vl->addWidget(notice_bar_);
    notice_bar_->hide();
    connect(notice_action_, &QPushButton::clicked, this, &ReportBuilderScreen::on_notice_action);
    connect(notice_close_, &QPushButton::clicked, this, [this]() {
        if (notice_kind_ == NoticeKind::Inbox)
            Service::instance().mark_inbox_seen();
        clear_notice();
    });

    pages_ = new QStackedWidget(this);
    library_page_ = new ReportLibraryPage(pages_);
    pages_->addWidget(library_page_);
    editor_page_ = build_editor_page();
    pages_->addWidget(editor_page_);
    vl->addWidget(pages_, 1);

    refresh_timer_ = new QTimer(this);
    refresh_timer_->setSingleShot(true);
    refresh_timer_->setInterval(0); // a burst of service signals renders once
    connect(refresh_timer_, &QTimer::timeout, this, &ReportBuilderScreen::do_refresh);

    wire_services();
    retranslateUi();

    // Chart image cache: capped size, plus the PNGs older builds left in the temp folder.
    // Once per session, off the GUI thread.
    static bool s_rb_cache_trimmed = false;
    if (!s_rb_cache_trimmed) {
        s_rb_cache_trimmed = true;
        (void)QtConcurrent::run([]() { services::ReportChartCache::trim(); });
    }
}

ReportBuilderScreen::~ReportBuilderScreen() {
    if (export_job_)
        export_job_->cancel();
    if (s_rb_current_screen == this)
        s_rb_current_screen.clear();
}

QWidget* ReportBuilderScreen::build_editor_page() {
    auto* page = new QWidget(pages_);
    auto* l = new QVBoxLayout(page);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    splitter_ = new QSplitter(Qt::Horizontal, page);
    splitter_->setObjectName(QStringLiteral("rbSplitter"));
    splitter_->setHandleWidth(1);
    splitter_->setChildrenCollapsible(false);
    comp_toolbar_ = new ComponentToolbar(splitter_);
    canvas_ = new DocumentCanvas(splitter_);
    // Right column: the properties, and under them the AI proposal panel when it is in use.
    right_split_ = new QSplitter(Qt::Vertical, splitter_);
    right_split_->setObjectName(QStringLiteral("rbRightSplit"));
    right_split_->setHandleWidth(1);
    right_split_->setChildrenCollapsible(false);
    properties_ = new PropertiesPanel(right_split_);
    ai_panel_ = new ReportAiPanel(right_split_);
    right_split_->addWidget(properties_);
    right_split_->addWidget(ai_panel_);
    ai_panel_->setVisible(false);
    splitter_->addWidget(comp_toolbar_);
    splitter_->addWidget(canvas_);
    splitter_->addWidget(right_split_);
    splitter_->setStretchFactor(0, 0);
    splitter_->setStretchFactor(1, 1);
    splitter_->setStretchFactor(2, 0);
    splitter_->setSizes({comp_toolbar_->preferred_width(), 700, properties_->preferred_width()});
    l->addWidget(splitter_, 1);
    // Screen-only notes under data blocks: refreshing / failed.
    canvas_->set_note_provider([](int id) {
        rb::BlockNote n;
        const auto st = Binder::instance().state(id);
        if (st.state == Binder::State::Loading || st.state == Binder::State::Queued) {
            n.kind = rb::BlockNote::Kind::Loading;
        } else if (st.state == Binder::State::Failed) {
            n.kind = rb::BlockNote::Kind::Failed;
            n.text = st.message;
        }
        return n;
    });
    return page;
}

// ── Toolbar ─────────────────────────────────────────────────────────────────

QWidget* ReportBuilderScreen::build_toolbar() {
    auto* bar = new QWidget(this);
    toolbar_ = bar;
    bar->setObjectName(QStringLiteral("rbToolbar"));
    bar->setAttribute(Qt::WA_StyledBackground);
    bar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(8, 4, 8, 4);
    hl->setSpacing(6);

    auto sep = [bar, hl]() {
        auto* f = new QFrame(bar); // a real 1-px rule (it used to be a disabled "|" button)
        f->setFixedWidth(1);
        f->setProperty("rbRole", QStringLiteral("sep"));
        hl->addWidget(f);
    };
    auto btn = [bar, hl](const QString& text) {
        auto* b = new QPushButton(text, bar);
        b->setCursor(Qt::PointingHandCursor);
        hl->addWidget(b);
        return b;
    };

    left_toggle_btn_ = btn(QStringLiteral("‹"));
    connect(left_toggle_btn_, &QPushButton::clicked, this, [this]() { apply_left_collapsed(!left_collapsed_, true); });
    toolbar_title_ = new QLabel(tr("REPORT BUILDER"), bar);
    toolbar_title_->setProperty("rbRole", QStringLiteral("brand"));
    hl->addWidget(toolbar_title_);
    doc_title_ = new QLabel(bar);
    doc_title_->setTextFormat(Qt::PlainText);
    doc_title_->setMinimumWidth(40);
    doc_title_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); // elides instead of widening the bar
    hl->addWidget(doc_title_, 1);

    library_btn_ = btn(tr("Library"));
    connect(library_btn_, &QPushButton::clicked, this, &ReportBuilderScreen::show_library);
    sep();

    auto* undo_stack = Service::instance().undo_stack();
    undo_btn_ = btn(tr("Undo"));
    undo_btn_->setEnabled(undo_stack->canUndo());
    connect(undo_stack, &QUndoStack::canUndoChanged, undo_btn_, &QPushButton::setEnabled);
    connect(undo_btn_, &QPushButton::clicked, undo_stack, &QUndoStack::undo);
    redo_btn_ = btn(tr("Redo"));
    redo_btn_->setEnabled(undo_stack->canRedo());
    connect(undo_stack, &QUndoStack::canRedoChanged, redo_btn_, &QPushButton::setEnabled);
    connect(redo_btn_, &QPushButton::clicked, undo_stack, &QUndoStack::redo);
    sep();

    open_btn_ = btn(tr("Open"));
    connect(open_btn_, &QPushButton::clicked, this, &ReportBuilderScreen::on_open);
    save_btn_ = new QToolButton(bar);
    save_btn_->setPopupMode(QToolButton::MenuButtonPopup);
    save_btn_->setCursor(Qt::PointingHandCursor);
    auto* save_menu = new QMenu(save_btn_);
    QAction* save_as = save_menu->addAction(tr("Save As…  (Ctrl+Shift+S)"));
    QAction* save_tpl = save_menu->addAction(tr("Save as template…"));
    save_btn_->setMenu(save_menu);
    hl->addWidget(save_btn_);
    connect(save_btn_, &QToolButton::clicked, this, &ReportBuilderScreen::on_save);
    connect(save_as, &QAction::triggered, this, &ReportBuilderScreen::on_save_as);
    connect(save_tpl, &QAction::triggered, this, &ReportBuilderScreen::show_save_template_dialog);

    refresh_data_btn_ = btn(tr("Refresh Data"));
    connect(refresh_data_btn_, &QPushButton::clicked, this, &ReportBuilderScreen::refresh_all_data);
    sep();

    preview_btn_ = btn(tr("Print view"));
    preview_btn_->setCheckable(true);
    connect(preview_btn_, &QPushButton::toggled, this, [this](bool on) {
        canvas_->set_preview_mode(on);
        canvas_->set_selected_id(on ? 0 : selected_id_, false);
    });
    export_btn_ = new QToolButton(bar);
    export_btn_->setPopupMode(QToolButton::MenuButtonPopup);
    export_btn_->setProperty("rbRole", QStringLiteral("primary"));
    export_btn_->setCursor(Qt::PointingHandCursor);
    auto* export_menu = new QMenu(export_btn_);
    QAction* odt = export_menu->addAction(tr("Export Word document (.odt — opens in Word and LibreOffice)…"));
    QAction* md = export_menu->addAction(tr("Export Markdown (.md + pictures folder)…"));
    QAction* html = export_menu->addAction(tr("Export HTML (self-contained web page)…"));
    export_menu->addSeparator();
    QAction* schedule = export_menu->addAction(tr("Schedule regeneration…"));
    export_btn_->setMenu(export_menu);
    hl->addWidget(export_btn_);
    connect(export_btn_, &QToolButton::clicked, this, &ReportBuilderScreen::on_export_pdf);
    connect(odt, &QAction::triggered, this, [this]() { on_export(ReportExportJob::Format::Odt); });
    connect(md, &QAction::triggered, this, [this]() { on_export(ReportExportJob::Format::Markdown); });
    connect(html, &QAction::triggered, this, &ReportBuilderScreen::on_export_html);
    connect(schedule, &QAction::triggered, this, &ReportBuilderScreen::show_schedule_dialog);

    right_toggle_btn_ = btn(QStringLiteral("›"));
    connect(right_toggle_btn_, &QPushButton::clicked, this, [this]() { apply_right_collapsed(!right_collapsed_, true); });

    // ── Shortcuts (while the tab has focus) ─────────────────────────────────
    auto add_sc = [this](const QKeySequence& key, auto fn) {
        auto* sc = new QShortcut(key, this);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, this, fn);
    };
    add_sc(QKeySequence(QKeySequence::Save), [this]() { on_save(); });
    add_sc(QKeySequence(QStringLiteral("Ctrl+Shift+S")), [this]() { on_save_as(); });
    add_sc(QKeySequence(QKeySequence::Open), [this]() { on_open(); });
    add_sc(QKeySequence(QKeySequence::New), [this]() { on_new(); });
    add_sc(QKeySequence(QKeySequence::Print), [this]() { preview_btn_->toggle(); });
    add_sc(QKeySequence(QKeySequence::Undo), [undo_stack]() { undo_stack->undo(); });
    add_sc(QKeySequence(QKeySequence::Redo), [undo_stack]() { undo_stack->redo(); });
    add_sc(QKeySequence(QStringLiteral("F5")), [this]() { refresh_all_data(); });
    add_sc(QKeySequence(QStringLiteral("Ctrl+B")), [this]() { apply_left_collapsed(!left_collapsed_, true); });
    add_sc(QKeySequence(QStringLiteral("Ctrl+Shift+B")), [this]() { apply_right_collapsed(!right_collapsed_, true); });

    return bar;
}

void ReportBuilderScreen::retranslateUi() {
    toolbar_title_->setText(tr("REPORT BUILDER"));
    library_btn_->setText(tr("Library"));
    library_btn_->setToolTip(tr("All your reports and templates"));
    undo_btn_->setText(tr("Undo"));
    undo_btn_->setToolTip(tr("Undo  (Ctrl+Z)"));
    redo_btn_->setText(tr("Redo"));
    redo_btn_->setToolTip(tr("Redo  (Ctrl+Y)"));
    open_btn_->setText(tr("Open"));
    open_btn_->setToolTip(tr("Open a .fincept file  (Ctrl+O)"));
    save_btn_->setText(tr("Save"));
    save_btn_->setToolTip(tr("Save to the library — and to its file when it has one  (Ctrl+S)"));
    preview_btn_->setText(tr("Print view"));
    preview_btn_->setToolTip(tr("Show the pages exactly as they export: no hints, no selection  (Ctrl+P)"));
    export_btn_->setText(tr("Export PDF"));
    export_btn_->setToolTip(tr("Export a PDF (arrow: Word .odt, Markdown, HTML, schedule)"));
    update_refresh_button();
    left_toggle_btn_->setToolTip(left_collapsed_ ? tr("Show the blocks panel  (Ctrl+B)") : tr("Hide the blocks panel  (Ctrl+B)"));
    right_toggle_btn_->setToolTip(right_collapsed_ ? tr("Show the properties panel  (Ctrl+Shift+B)")
                                                   : tr("Hide the properties panel  (Ctrl+Shift+B)"));
    export_cancel_->setText(tr("Cancel"));
    update_title();
}

void ReportBuilderScreen::update_title() {
    if (!doc_title_)
        return;
    auto& svc = Service::instance();
    const QString text = rep::vars::display_title(svc.metadata());
    const QString file = svc.current_file();
    QString state;
    if (library_saving_ && (svc.library_id() > 0 || !svc.components().isEmpty()))
        state = tr("saving…");
    else if (svc.library_id() > 0)
        state = tr("saved in the library");
    else if (svc.components().isEmpty())
        state = tr("new");
    else
        state = tr("not saved yet");
    if (!file.isEmpty())
        state += QStringLiteral(" · ") + QFileInfo(file).fileName() + (svc.file_dirty() ? QStringLiteral(" *") : QString());
    doc_title_->setText(QStringLiteral("%1   ·  %2").arg(text, state));
    doc_title_->setToolTip(file.isEmpty() ? text : text + QLatin1Char('\n') + file);
}

// ── Pages ───────────────────────────────────────────────────────────────────

void ReportBuilderScreen::show_library() {
    auto& svc = Service::instance();
    properties_->flush_pending();
    library_page_->set_current(svc.library_id(), svc.metadata().title, static_cast<int>(svc.components().size()));
    pages_->setCurrentWidget(library_page_);
    library_btn_->setEnabled(false);
    ScreenStateManager::instance().notify_changed(this);
}

void ReportBuilderScreen::show_editor() {
    pages_->setCurrentWidget(editor_page_);
    library_btn_->setEnabled(true);
    ScreenStateManager::instance().notify_changed(this);
}

// ── Side panels ─────────────────────────────────────────────────────────────

void ReportBuilderScreen::apply_left_collapsed(bool collapsed, bool animate) {
    left_collapsed_ = collapsed;
    const int target = collapsed ? 0 : comp_toolbar_->preferred_width();
    if (!left_anim_) {
        left_anim_ = new QPropertyAnimation(comp_toolbar_, "maximumWidth", this);
        left_anim_->setDuration(180);
        left_anim_->setEasingCurve(QEasingCurve::OutCubic);
    }
    left_anim_->stop();
    if (animate) {
        left_anim_->setStartValue(comp_toolbar_->maximumWidth());
        left_anim_->setEndValue(target);
        left_anim_->start();
    } else {
        comp_toolbar_->setMaximumWidth(target);
    }
    left_toggle_btn_->setText(collapsed ? QStringLiteral("›") : QStringLiteral("‹"));
    left_toggle_btn_->setToolTip(collapsed ? tr("Show the blocks panel  (Ctrl+B)") : tr("Hide the blocks panel  (Ctrl+B)"));
    ScreenStateManager::instance().notify_changed(this);
}

void ReportBuilderScreen::apply_right_collapsed(bool collapsed, bool animate) {
    right_collapsed_ = collapsed;
    const int target = collapsed ? 0 : properties_->preferred_width();
    if (!right_anim_) {
        right_anim_ = new QPropertyAnimation(right_split_, "maximumWidth", this);
        right_anim_->setDuration(180);
        right_anim_->setEasingCurve(QEasingCurve::OutCubic);
    }
    right_anim_->stop();
    if (animate) {
        right_anim_->setStartValue(right_split_->maximumWidth());
        right_anim_->setEndValue(target);
        right_anim_->start();
    } else {
        right_split_->setMaximumWidth(target);
    }
    right_toggle_btn_->setText(collapsed ? QStringLiteral("‹") : QStringLiteral("›"));
    right_toggle_btn_->setToolTip(collapsed ? tr("Show the properties panel  (Ctrl+Shift+B)")
                                            : tr("Hide the properties panel  (Ctrl+Shift+B)"));
    ScreenStateManager::instance().notify_changed(this);
}

void ReportBuilderScreen::refit_panels() {
    // Widths follow the UI font (run once the appearance has settled).
    if (!left_collapsed_)
        comp_toolbar_->setMaximumWidth(comp_toolbar_->preferred_width());
    if (!right_collapsed_)
        right_split_->setMaximumWidth(properties_->preferred_width());
    fit_toolbar();
}

void ReportBuilderScreen::fit_toolbar() {
    // On a narrow window / large UI font the brand label gives way first, so every action
    // keeps its full text (the document title already elides).
    if (!toolbar_ || !toolbar_title_)
        return;
    const int brand = toolbar_title_->sizeHint().width() + 6;
    const int need = toolbar_->sizeHint().width() + (toolbar_title_->isVisible() ? 0 : brand);
    const bool show_brand = width() >= need;
    if (toolbar_title_->isVisible() != show_brand)
        toolbar_title_->setVisible(show_brand);
}

void ReportBuilderScreen::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    fit_toolbar();
}

// ── Lifecycle ───────────────────────────────────────────────────────────────

void ReportBuilderScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    auto& svc = Service::instance();
    if (!started_) {
        started_ = true;
        // Never a blank tab: an empty, unsaved report opens on the library / start page.
        if (svc.components().isEmpty() && svc.library_id() <= 0)
            show_library();
        else
            show_editor();
        do_refresh();
        check_stale_notice();
    } else if (hidden_dirty_) {
        do_refresh();
    }
    handle_open_request();
    check_inbox_notice();
    if (Service::instance().has_library_conflict())
        show_conflict_dialog();
    // Rate-gated pull of cloud reports on screen entry (no-op when sync is off).
    services::cloud::CloudSyncEngine::instance().request_pull(QStringLiteral("report"));
}

void ReportBuilderScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    properties_->flush_pending();
    // Leaving the tab: the library copy should not wait for the idle timer.
    Service::instance().save_to_library();
}

void ReportBuilderScreen::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(e);
}

// ── IStatefulScreen ─────────────────────────────────────────────────────────

QVariantMap ReportBuilderScreen::save_state() const {
    return {
        {"selected_id", selected_id_},
        {"left_collapsed", left_collapsed_},
        {"right_collapsed", right_collapsed_},
        {"page", pages_ && pages_->currentWidget() == library_page_ ? QStringLiteral("library") : QStringLiteral("editor")},
    };
}

void ReportBuilderScreen::restore_state(const QVariantMap& state) {
    if (state.contains("selected_id")) {
        const int id = state.value("selected_id").toInt(0);
        if (Service::instance().index_of(id) >= 0)
            selected_id_ = id;
    }
    if (state.contains("left_collapsed"))
        apply_left_collapsed(state.value("left_collapsed").toBool(), /*animate=*/false);
    if (state.contains("right_collapsed"))
        apply_right_collapsed(state.value("right_collapsed").toBool(), /*animate=*/false);
    if (state.value("page").toString() == QLatin1String("library")) {
        started_ = true;
        show_library();
    } else if (state.contains("page") && !Service::instance().components().isEmpty()) {
        started_ = true;
        show_editor();
    }
    schedule_refresh();
}

// ── ISectionLinked ──────────────────────────────────────────────────────────

bool ReportBuilderScreen::open_section(const QString& section, const QString& focus) {
    // The toolbar's own paths: LIBRARY (show_library), the editor (show_editor), NEW (on_new) and
    // the library's OPEN (open_library_report). Validated before anything changes.
    const QString key = section.trimmed().toLower();
    qint64 report_id = 0;
    bool ok = false;
    if (key.startsWith(QLatin1String("report:"))) {
        report_id = key.mid(7).trimmed().toLongLong(&ok);
        if (!ok || report_id <= 0)
            return false;
    } else if (!key.isEmpty() && key != QLatin1String("library") && key != QLatin1String("editor") &&
               key != QLatin1String("new")) {
        return false;
    }
    if (report_id <= 0 && key != QLatin1String("new")) {
        const qint64 id = focus.trimmed().toLongLong(&ok);
        if (ok && id > 0)
            report_id = id;
    }
    if (report_id > 0)
        open_library_report(report_id, 0);
    else if (key == QLatin1String("new"))
        on_new();
    else if (key == QLatin1String("library"))
        show_library();
    else if (key == QLatin1String("editor"))
        show_editor();
    return true;
}

} // namespace fincept::screens
