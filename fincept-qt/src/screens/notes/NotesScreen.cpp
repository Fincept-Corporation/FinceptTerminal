// src/screens/notes/NotesScreen.cpp — construction, layout, styles, menus,
// keyboard, visibility and screen state. Data: NotesScreen_Data.cpp; editor
// session and actions: NotesScreen_Actions.cpp.
#include "screens/notes/NotesScreen.h"

#include "auth/InactivityGuard.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/notes/NotesBoardView.h"
#include "screens/notes/NotesDetailPane.h"
#include "screens/notes/NotesEditor.h"
#include "screens/notes/NotesModels.h"
#include "screens/notes/NotesWallView.h"
#include "services/cloud/CloudSyncEngine.h"
#include "ui/tables/FlashCellDelegate.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/ScreenCommandBar.h"
#include "ui/widgets/ScreenStatusBar.h"
#include "ui/widgets/StatTile.h"
#include "ui/widgets/report/AddToReportButton.h"

#include <QApplication>
#include <QCoreApplication>
#include <QCursor>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QScrollBar>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableView>
#include <QTextBrowser>
#include <QTime>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens {

using notes::BoardViewMode;
using notes::GroupBy;
namespace nb = fincept::notes_board;

namespace {

namespace colors = fincept::ui::colors;
namespace fonts = fincept::ui::fonts;

constexpr int kAutosaveMs = 1200;

/// Give `target` (a widget inside `screen`) the keyboard focus so the screen's
/// own keys fire, unless the user is plainly busy elsewhere: a menu or dialog is
/// open, the focus is already inside the screen, or a text field outside it is
/// being typed into.
void nsc_claim_focus(QWidget* screen, QWidget* target) {
    if (!screen || !target || !screen->isVisible() || !screen->isEnabled())
        return;
    if (QApplication::activePopupWidget() || QApplication::activeModalWidget())
        return;
    const QWidget* held = QApplication::focusWidget();
    if (held && held->isVisible() && held->isEnabled()) {
        if (held == screen || screen->isAncestorOf(held))
            return;
        if (held->window() != screen->window())
            return;
        if (held->inherits("QLineEdit") || held->inherits("QTextEdit") || held->inherits("QPlainTextEdit") ||
            held->inherits("QAbstractSpinBox"))
            return;
    }
    QWidget* receiver = target->isVisibleTo(screen) ? target : screen;
    receiver->setFocus(Qt::OtherFocusReason);
}

} // namespace

// ── Construction ─────────────────────────────────────────────────────────────

NotesScreen::NotesScreen(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("notesScreen"));
    // One stylesheet for the whole screen, set before any child exists (object-name
    // selectors only); the central re-style pass keeps it in step afterwards.
    apply_styles();

    store_ = new notes::NotesStore(this);
    quote_owner_ = new QObject(this);
    boards_ = notes::default_boards(); // until the stored ones arrive

    const auto timer = [this](int ms, auto fn) {
        auto* t = new QTimer(this);
        t->setSingleShot(true);
        t->setInterval(ms);
        connect(t, &QTimer::timeout, this, fn);
        return t;
    };
    autosave_timer_ = timer(kAutosaveMs, [this]() { save_editor_async(); });
    search_timer_ = timer(160, [this]() { apply_search(bar_->command_text()); });
    reload_timer_ = timer(250, [this]() { reload(); });
    build_timer_ = timer(40, [this]() { build_now(); });
    meta_timer_ = timer(300, [this]() { flush_meta(false); });
    wall_timer_ = timer(500, [this]() { flush_wall(false); });
    board_timer_ = timer(600, [this]() { flush_board(false); });
    quote_timer_ = timer(150, [this]() { repaint_quotes(); });
    subscribe_timer_ = timer(250, [this]() { resubscribe_quotes(); });
    flash_timer_ = new QTimer(this);
    flash_timer_->setInterval(60);
    connect(flash_timer_, &QTimer::timeout, this, [this]() {
        if (!table_model_ || !table_model_->tick_flash())
            flash_timer_->stop();
    });

    build_ui();
    retranslateUi();
    reload();

    // A cloud pull updated notes in the local cache.
    connect(&fincept::services::cloud::CloudSyncEngine::instance(),
            &fincept::services::cloud::CloudSyncEngine::cloud_data_changed, this, [this](const QString& entity) {
                if (entity == QLatin1String("note"))
                    request_reload();
            });
    // Theme / font changes: colours are re-read at paint time and stylesheets are
    // re-styled centrally; sizes are re-fitted once every widget has its new style.
    connect(&fincept::ui::ThemeManager::instance(), &fincept::ui::ThemeManager::appearance_settled, this,
            [this]() { refit(); });
    // Locking the terminal writes the open note and the board first.
    connect(&fincept::auth::InactivityGuard::instance(), &fincept::auth::InactivityGuard::terminal_locked_changed, this,
            [this](bool locked) {
                if (locked) {
                    save_editor_async();
                    flush_meta(false);
                    flush_wall(false);
                    flush_board(false);
                    return;
                }
                QTimer::singleShot(0, this, [this]() { nsc_claim_focus(this, board_view_); });
                QTimer::singleShot(400, this, [this]() { nsc_claim_focus(this, board_view_); });
            });
    connect(qApp, &QCoreApplication::aboutToQuit, this, &NotesScreen::flush_for_quit);
}

NotesScreen::~NotesScreen() {
    flush_for_quit();
    unsubscribe_events();
    // Quote subscriptions end with quote_owner_ (DataHub watches the owner's lifetime).
    delete store_; // waits for queued writes before the members they report to go away
    store_ = nullptr;
}

// ── Styles ───────────────────────────────────────────────────────────────────

void NotesScreen::apply_styles() {
    using namespace colors;
    namespace f = fonts;
    const QString base = BG_BASE(), surface = BG_SURFACE(), raised = BG_RAISED();
    const QString b_dim = BORDER_DIM(), b_med = BORDER_MED();
    const QString t1 = TEXT_PRIMARY(), t2 = TEXT_SECONDARY(), t3 = TEXT_TERTIARY();
    const QString accent = AMBER(), accent_dim = AMBER_DIM(), on_accent = TEXT_ON_ACCENT(), accent_bg = ACCENT_BG();
    const QString danger = DANGER();
    const QString danger_wash = colors::alpha(DANGER, 0.12), danger_line = colors::alpha(DANGER, 0.45);
    const QString family = QString(f::DATA_FAMILY);

    QString s;
    s.reserve(12000);
    s += "QWidget#notesScreen{background:" + base + ";}";
    s += "QSplitter#notesSplit::handle{background:" + b_dim + ";}";
    s += "QWidget#notesLeft,QStackedWidget#notesViews{background:" + base + ";}";
    s += "QWidget#notesDetail,QWidget#notesDetailPage{background:" + base + ";}";
    // Wall toolbar
    s += "QWidget#notesWallBar{background:" + surface + ";border:none;border-bottom:1px solid " + b_dim + ";}";
    s += "QLabel#notesHint{color:" + t3 + ";background:transparent;font-size:" + f::px(-4) + "px;}";
    // Small outlined buttons (Watchlist bar look) and their accent / danger roles
    s += "QPushButton#notesSmallBtn,QToolButton#notesSmallBtn{background:transparent;color:" + t2 +
         ";border:1px solid " + b_dim + ";padding:2px 9px;font-family:" + family + ";font-size:" + f::px(-4) +
         "px;font-weight:600;}";
    s += "QPushButton#notesSmallBtn:hover,QToolButton#notesSmallBtn:hover{color:" + t1 + ";border-color:" + b_med + ";}";
    s += "QPushButton#notesSmallBtn:checked,QToolButton#notesSmallBtn:checked{color:" + accent + ";border-color:" +
         accent + ";background:" + accent_bg + ";}";
    s += "QPushButton#notesSmallBtn:disabled{color:" + t3 + ";}";
    s += "QPushButton#notesSmallBtn::menu-indicator{image:none;width:0px;}";
    s += "QPushButton#notesDangerBtn{background:" + danger_wash + ";color:" + danger + ";border:1px solid " +
         danger_line + ";padding:2px 9px;font-family:" + family + ";font-size:" + f::px(-4) + "px;font-weight:700;}";
    s += "QPushButton#notesDangerBtn:hover{background:" + danger + ";color:" + on_accent + ";}";
    s += "QToolButton#notesHeaderBtn{background:transparent;color:" + t2 +
         ";border:1px solid transparent;padding:0px 6px;font-family:" + family + ";font-size:" + f::px(-4) +
         "px;font-weight:700;}";
    s += "QToolButton#notesHeaderBtn:hover{color:" + accent + ";border-color:" + b_med + ";}";
    s += "QToolButton#notesHeaderBtn::menu-indicator{image:none;width:0px;}";
    // Detail pane strips
    s += "QWidget#notesBoardStrip,QWidget#notesActionsRow{background:" + surface +
         ";border:none;border-bottom:1px solid " + b_dim + ";}";
    s += "QFrame#notesRenameBox{background:" + colors::tint(QColor(WARNING()), 0.14) +
         ";border:none;border-bottom:1px solid " + b_dim + ";}";
    s += "QLabel#notesRenameText{color:" + t1 + ";background:transparent;font-size:" + f::px(-3) + "px;}";
    s += "QLabel#notesLinked{color:" + QString(CYAN()) + ";background:transparent;font-size:" + f::px(-4) + "px;}";
    s += "QLabel#notesBulkTitle{color:" + t1 + ";background:transparent;font-size:" + f::px(2) + "px;font-weight:700;}";
    s += "QDateEdit#notesDateEdit{background:" + surface + ";color:" + t1 + ";border:1px solid " + b_dim +
         ";padding:1px 6px;font-family:" + family + ";font-size:" + f::px(-3) + "px;}";
    s += "QDateEdit#notesDateEdit:focus{border-color:" + accent_dim + ";}";
    s += "QListView#notesHistory{background:" + base + ";border:none;color:" + t2 + ";font-family:" + family +
         ";font-size:" + f::px(-3) + "px;}";
    // LIST view (Watchlist / Markets table look)
    s += "QTableView#notesTable{background:" + base + ";alternate-background-color:" + QString(ROW_ALT()) +
         ";border:none;outline:0;color:" + t1 + ";gridline-color:" + b_dim + ";font-family:" + family +
         ";font-size:" + f::px(-2) + "px;selection-background-color:" + colors::alpha(QColor(accent), 0.14) +
         ";selection-color:" + t1 + ";}";
    s += "QTableView#notesTable::item{border:none;padding:0px 4px;}";
    s += "QTableView#notesTable QHeaderView::section{background:" + surface + ";color:" + t3 +
         ";border:none;border-bottom:1px solid " + b_dim + ";border-right:1px solid " + b_dim +
         ";padding:0px 6px;font-family:" + family + ";font-size:" + f::px(-4) + "px;font-weight:700;}";
    s += "QTableView#notesTable QLineEdit{background:" + raised + ";color:" + t1 + ";border:1px solid " + accent_dim +
         ";}";
    // Quick peek
    s += "QFrame#notesPeek{background:" + raised + ";border:1px solid " + b_med + ";}";
    s += "QLabel#notesPeekTitle{color:" + t1 + ";background:transparent;font-size:" + f::px(0) + "px;font-weight:700;}";
    s += "QTextBrowser#notesPeekBody{background:" + raised + ";color:" + t1 + ";border:none;}";
    // Editor
    s += "QWidget#notesEditor{background:" + base + ";}";
    s += "QWidget#notesEditorBar{background:" + base + ";border-bottom:1px solid " + b_dim + ";}";
    s += "QToolButton#notesPinBtn{color:" + t2 + ";background:transparent;border:1px solid " + b_dim +
         ";padding:3px 10px;font-size:" + f::px(-3) + "px;font-weight:700;}";
    s += "QToolButton#notesPinBtn:hover{color:" + t1 + ";border-color:" + b_med + ";}";
    s += "QToolButton#notesPinBtn:checked{color:" + accent + ";border-color:" + accent_dim + ";background:" +
         accent_bg + ";}";
    s += "QToolButton#notesModeBtn{color:" + t3 + ";background:transparent;border:1px solid " + b_dim +
         ";padding:3px 10px;font-size:" + f::px(-3) + "px;font-weight:700;}";
    s += "QToolButton#notesModeBtn:hover{color:" + t1 + ";}";
    s += "QToolButton#notesModeBtn:checked{color:" + t1 + ";background:" + raised + ";border-color:" + b_med + ";}";
    s += "QLabel#notesSaveState{font-size:" + f::px(-2) + "px;}";
    s += "QToolButton#notesMoreBtn{color:" + t2 + ";background:transparent;border:1px solid " + b_dim +
         ";padding:3px 10px;font-size:" + f::px(-3) + "px;font-weight:700;}";
    s += "QToolButton#notesMoreBtn:hover{color:" + t1 + ";border-color:" + b_med + ";}";
    s += "QToolButton#notesMoreBtn::menu-indicator{image:none;width:0px;}";
    s += "QWidget#notesEditorHead{background:" + base + ";}";
    s += "QLineEdit#notesTitle{background:transparent;border:none;color:" + t1 + ";padding:0px;font-size:" + f::px(6) +
         "px;font-weight:700;selection-background-color:" + accent + ";selection-color:" + on_accent + ";}";
    s += "QLabel#notesCaption{color:" + t3 + ";background:transparent;font-size:" + f::px(-4) + "px;font-weight:700;}";
    s += "QComboBox#notesMetaCombo{background:" + surface + ";color:" + t1 + ";border:1px solid " + b_dim +
         ";padding:1px 8px;font-size:" + f::px(-3) + "px;}";
    s += "QComboBox#notesMetaCombo:hover{border-color:" + b_med + ";}";
    s += "QComboBox#notesMetaCombo:focus{border-color:" + accent_dim + ";}";
    s += "QComboBox#notesMetaCombo QAbstractItemView{background:" + surface + ";color:" + t1 +
         ";selection-background-color:" + raised + ";selection-color:" + t1 + ";border:1px solid " + b_med + ";}";
    s += "QWidget#notesChipBox{background:transparent;}";
    s += "QFrame#notesTickerChip,QFrame#notesTagChip{background:" + raised + ";border:1px solid " + b_med + ";}";
    s += "QFrame#notesTickerChip:hover{border-color:" + accent_dim + ";}";
    s += "QLabel#notesChipText{background:transparent;border:none;font-size:" + f::px(-2) + "px;}";
    s += "QToolButton#notesChipX{color:" + t3 + ";background:transparent;border:none;padding:0px;font-size:" +
         f::px(-1) + "px;}";
    s += "QToolButton#notesChipX:hover{color:" + danger + ";}";
    s += "QLineEdit#notesChipInput{background:transparent;color:" + t1 + ";border:1px dashed " + b_med +
         ";padding:0px 6px;font-size:" + f::px(-2) + "px;}";
    s += "QLineEdit#notesChipInput:focus{border:1px solid " + accent_dim + ";}";
    s += "QLineEdit#notesChipInput[invalid=\"true\"]{border:1px solid " + danger + ";}";
    s += "QFrame#notesRule{background:" + b_dim + ";border:none;}";
    s += "QSplitter#notesBodySplit::handle{background:" + b_dim + ";}";
    s += "QPlainTextEdit#notesBody{background:" + base + ";color:" + t1 + ";border:none;font-size:" + f::px(1) +
         "px;selection-background-color:" + accent + ";selection-color:" + on_accent + ";}";
    s += "QTextBrowser#notesPreview{background:" + base + ";color:" + t1 + ";border:none;}";
    s += "QWidget#notesEditorFooter{background:" + base + ";border-top:1px solid " + b_dim + ";}";
    s += "QLabel#notesFooterText{color:" + t3 + ";font-size:" + f::px(-3) + "px;}";
    if (styleSheet() != s)
        setStyleSheet(s);
}

// ── Building ─────────────────────────────────────────────────────────────────

void NotesScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    build_command_bar();
    root->addWidget(bar_);
    build_ribbon();
    root->addWidget(ribbon_);

    split_ = new QSplitter(Qt::Horizontal, this);
    split_->setObjectName(QStringLiteral("notesSplit"));
    split_->setHandleWidth(1);
    split_->setChildrenCollapsible(true);
    root->addWidget(split_, 1);
    build_views();

    detail_ = new notes::NotesDetailPane(split_);
    split_->addWidget(detail_);
    split_->setCollapsible(0, false);
    split_->setStretchFactor(0, 3);
    split_->setStretchFactor(1, 2);
    {
        // The detail pane gets about 40 % (at least ~52 characters wide) by default.
        const QFontMetrics fm(fonts::font(-1));
        const int detail_w = std::max(380, fm.averageCharWidth() * 52);
        split_->setSizes({900, detail_w});
    }
    connect(split_, &QSplitter::splitterMoved, this, [this]() { notify_state_changed(); });

    build_status_bar();
    root->addWidget(status_);
    build_peek();

    // Detail pane wiring.
    connect(detail_, &notes::NotesDetailPane::status_changed, this, [this](const QString& st) {
        const int id = open_id();
        if (id > 0)
            set_status({id}, st);
        else
            session_meta_.status = st;
    });
    connect(detail_, &notes::NotesDetailPane::review_changed, this, [this](const QDate& d) {
        const int id = open_id();
        if (id > 0)
            set_review({id}, d);
        else
            session_meta_.review_on = d.isValid() ? d.toString(Qt::ISODate) : QString();
    });
    connect(detail_, &notes::NotesDetailPane::mark_reviewed, this, [this]() {
        if (open_id() > 0)
            mark_reviewed(open_id());
    });
    connect(detail_, &notes::NotesDetailPane::open_chat_run, this, &NotesScreen::open_chat_run);
    connect(detail_, &notes::NotesDetailPane::rename_accepted, this, [this](const QString& from, const QString& to) {
        if (editor_open_)
            editor()->replace_ticker(from, to);
    });
    connect(detail_, &notes::NotesDetailPane::rename_dismissed, this,
            [this](const QString& from) { dismissed_renames_.insert(from); });
    connect(detail_, &notes::NotesDetailPane::focus_toggled, this, [this]() { set_focus_mode(!focus_mode_); });
    connect(detail_, &notes::NotesDetailPane::hide_requested, this, [this]() {
        set_focus_mode(false);
        set_detail_visible(false);
    });
    connect(detail_, &notes::NotesDetailPane::symbol_clicked, this, &NotesScreen::show_symbol_menu);
    connect(detail_, &notes::NotesDetailPane::linked_action, this, &NotesScreen::open_symbol_in);
    connect(detail_, &notes::NotesDetailPane::bulk_action, this, &NotesScreen::on_bulk_action);
    connect(detail_, &notes::NotesDetailPane::template_requested, this,
            [this](const QString& id) { new_from_template(id, QString(), QString()); });
    connect(detail_, &notes::NotesDetailPane::new_requested, this,
            [this]() { new_card(board_view_->current_lane_key()); });
    connect(detail_, &notes::NotesDetailPane::import_requested, this, &NotesScreen::import_files);
    connect(detail_, &notes::NotesDetailPane::continue_requested, this, [this](int id) {
        if (find_entry(id)) {
            select_in_views({id}, id, true);
            open_note(id);
        }
    });
    detail_->quote_strip()->set_quotes(&quotes_);
    detail_->report_button()->set_payload_provider([this]() { return report_payload(); });
    editor(); // the editor lives in the detail pane from the start

    setFocusPolicy(Qt::ClickFocus);
    install_shortcuts();
}

void NotesScreen::build_command_bar() {
    using CB = fincept::ui::ScreenCommandBar;
    bar_ = new CB(this);
    const auto menu = [this](void (NotesScreen::*populate)()) {
        auto* m = new QMenu(this);
        m->setStyleSheet(CB::menu_css());
        connect(m, &QMenu::aboutToShow, this, populate);
        return m;
    };
    board_menu_ = menu(&NotesScreen::populate_board_menu);
    template_menu_ = menu(&NotesScreen::populate_template_menu);
    group_menu_ = menu(&NotesScreen::populate_group_menu);
    export_menu_ = menu(&NotesScreen::populate_export_menu);
    priority_menu_ = menu(&NotesScreen::populate_priority_menu);
    tag_menu_ = menu(&NotesScreen::populate_tag_menu);
    date_menu_ = menu(&NotesScreen::populate_date_menu);

    bar_->add_menu_action(QStringLiteral("board"), QStringLiteral("BOARD"), board_menu_, QString(), QString(), 100);
    bar_->set_command_field(QString(), QStringLiteral("/"));
    bar_->set_command_width_chars(42, 18);
    bar_->add_action(QStringLiteral("new"), QStringLiteral("NEW"), QStringLiteral("N"), QString(), CB::Role::Primary, 95);
    bar_->add_menu_action(QStringLiteral("template"), QStringLiteral("TEMPLATE"), template_menu_, QStringLiteral("T"),
                          QString(), 60);
    bar_->add_separator(55);
    bar_->add_segmented(QStringLiteral("view"),
                        {{QStringLiteral("board"), QStringLiteral("BOARD"), QStringLiteral("1"), QString()},
                         {QStringLiteral("wall"), QStringLiteral("WALL"), QStringLiteral("2"), QString()},
                         {QStringLiteral("list"), QStringLiteral("LIST"), QStringLiteral("3"), QString()}},
                        90);
    bar_->add_menu_action(QStringLiteral("group"), QStringLiteral("GROUP"), group_menu_, QStringLiteral("G"), QString(),
                          85);
    bar_->add_separator(30);
    bar_->add_action(QStringLiteral("import"), QStringLiteral("IMPORT"), QString(), QString(), CB::Role::Normal, 20);
    bar_->add_menu_action(QStringLiteral("export"), QStringLiteral("EXPORT"), export_menu_, QString(), QString(), 25);
    bar_->add_toggle(QStringLiteral("detail"), QStringLiteral("DETAIL"), true, QString(), QString(), 40);

    bar_->add_chip(QStringLiteral("pinned"), QStringLiteral("PINNED"), QString(), 80);
    bar_->add_chip(QStringLiteral("ticker"), QStringLiteral("HAS TICKER"), QString(), 70);
    bar_->add_chip(QStringLiteral("due"), QStringLiteral("DUE"), QString(), 75);
    bar_->add_chip(QStringLiteral("linked"), QStringLiteral("FROM CHAT"), QString(), 40);
    bar_->add_menu_chip(QStringLiteral("priority"), QStringLiteral("PRIORITY"), priority_menu_, QString(), 60);
    bar_->add_menu_chip(QStringLiteral("tag"), QStringLiteral("TAG"), tag_menu_, QString(), 55);
    bar_->add_menu_chip(QStringLiteral("date"), QStringLiteral("EDITED"), date_menu_, QString(), 50);
    bar_->add_chip(QStringLiteral("archived"), QStringLiteral("ARCHIVED"), QString(), 30);
    bar_->add_chip(QStringLiteral("clear"), QStringLiteral("CLEAR"), QString(), 90);

    connect(bar_, &CB::action_triggered, this, [this](const QString& id) {
        if (id == QLatin1String("new"))
            new_card(board_view_->current_lane_key());
        else if (id == QLatin1String("import"))
            import_files();
    });
    connect(bar_, &CB::action_toggled, this, [this](const QString& id, bool on) {
        if (id == QLatin1String("detail")) {
            if (!on)
                set_focus_mode(false);
            set_detail_visible(on);
        }
    });
    connect(bar_, &CB::segment_selected, this, [this](const QString& id, const QString& key) {
        if (id == QLatin1String("view"))
            set_view_mode(notes::view_mode_from_key(key));
    });
    connect(bar_, &CB::chip_toggled, this, [this](const QString& id, bool) {
        if (id == QLatin1String("clear")) {
            notes::BoardFilter f;
            bar_->set_command_text(QString());
            apply_search(QString());
            set_filter(f);
            return;
        }
        set_filter(notes::toggle_filter_key(board().filter, id));
    });
    connect(bar_, &CB::command_edited, this, &NotesScreen::on_command_edited);
    connect(bar_, &CB::command_submitted, this, &NotesScreen::run_command);
    connect(bar_, &CB::command_cancelled, this, [this]() {
        QWidget* v = view_stack_->currentWidget();
        if (v == wall_page_)
            wall_view_->setFocus(Qt::ShortcutFocusReason);
        else if (v == table_view_)
            table_view_->setFocus(Qt::ShortcutFocusReason);
        else
            board_view_->setFocus(Qt::ShortcutFocusReason);
    });
    connect(bar_, &CB::command_navigate, this, [this](int) {
        if (search_timer_->isActive()) {
            search_timer_->stop();
            apply_search(bar_->command_text());
        }
        QWidget* v = view_stack_->currentWidget();
        if (v == table_view_) {
            table_view_->setFocus(Qt::ShortcutFocusReason);
            if (!table_view_->currentIndex().isValid() && table_proxy_->rowCount() > 0)
                table_view_->setCurrentIndex(table_proxy_->index(0, 0));
        } else if (v == wall_page_) {
            wall_view_->setFocus(Qt::ShortcutFocusReason);
        } else {
            board_view_->setFocus(Qt::ShortcutFocusReason);
            if (!board_view_->currentIndex().isValid())
                board_view_->focus_lane(0);
        }
    });
}

void NotesScreen::build_ribbon() {
    ribbon_ = new fincept::ui::KpiStrip(this);
    ribbon_->setObjectName(QStringLiteral("notesRibbon"));
    ribbon_->set_max_columns(7);
    ribbon_->set_spacing(1);
    const char* const keys[] = {"notes", "active", "due", "pinned", "tickers", "week", "oldest"};
    for (const char* k : keys) {
        auto* t = ribbon_->add_tile(QString::fromLatin1(k), QString());
        t->set_compact(true);
        t->set_clickable(true);
        const QString key = QString::fromLatin1(k);
        connect(t, &fincept::ui::StatTile::clicked, this, [this, key]() {
            notes::BoardFilter f = board().filter;
            if (key == QLatin1String("notes")) {
                const bool archived = f.show_archived;
                f = notes::BoardFilter();
                f.show_archived = archived;
            } else if (key == QLatin1String("active")) {
                const QStringList open = {nb::kStatusIdea, nb::kStatusResearching, nb::kStatusActive};
                f.statuses = f.statuses == open ? QStringList() : open;
            } else if (key == QLatin1String("due")) {
                f = notes::toggle_filter_key(f, QStringLiteral("due"));
            } else if (key == QLatin1String("pinned")) {
                f = notes::toggle_filter_key(f, QStringLiteral("pinned"));
            } else if (key == QLatin1String("tickers")) {
                f = notes::toggle_filter_key(f, QStringLiteral("ticker"));
                if (f.has_ticker && board().group_by != GroupBy::Ticker)
                    set_group_by(GroupBy::Ticker);
            } else if (key == QLatin1String("week")) {
                f = notes::toggle_filter_key(f, QStringLiteral("week"));
            } else if (key == QLatin1String("oldest")) {
                f = notes::toggle_filter_key(f, QStringLiteral("stale"));
            }
            set_filter(f);
        });
    }
}

void NotesScreen::build_views() {
    left_ = new QWidget(split_);
    left_->setObjectName(QStringLiteral("notesLeft"));
    auto* lay = new QVBoxLayout(left_);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    view_stack_ = new QStackedWidget(left_);
    view_stack_->setObjectName(QStringLiteral("notesViews"));
    lay->addWidget(view_stack_, 1);
    split_->addWidget(left_);

    // First load: a still skeleton of cards (never a blank area).
    skeleton_ = new fincept::ui::LoadingSkeleton(fincept::ui::LoadingSkeleton::Shape::Cards, view_stack_);
    view_stack_->addWidget(skeleton_);

    // ── BOARD ────────────────────────────────────────────────────────────────
    board_model_ = new notes::NotesBoardModel(this);
    board_view_ = new notes::NotesBoardView(view_stack_);
    board_view_->setProperty("fincept_agent_risk_keys", "Delete=delete;Ctrl+Backspace=delete"); // agents: Delete deletes the selected notes
    board_view_->setModel(board_model_);
    board_view_->set_quotes(&quotes_);
    board_view_->set_drop_validator(
        [this](const QVector<int>& ids, const QString& from, const QString& to) { return check_drop(ids, from, to); });
    board_view_->installEventFilter(this);
    view_stack_->addWidget(board_view_);
    const auto open_from_view = [this](int id, bool full) {
        open_note(id);
        if (full)
            set_focus_mode(true);
        if (editor_ && full)
            editor_->focus_body();
    };
    const auto pin_selection = [this]() {
        const QVector<int> ids = selected_ids();
        bool any_unpinned = false;
        for (int id : ids)
            if (const notes::NoteEntry* e = find_entry(id))
                any_unpinned = any_unpinned || !e->note.is_favorite;
        set_pinned(ids, any_unpinned);
    };
    connect(board_view_, &notes::NotesBoardView::open_requested, this, open_from_view);
    connect(board_view_, &notes::NotesBoardView::new_card_requested, this, &NotesScreen::new_card);
    connect(board_view_, &notes::NotesBoardView::ghost_clicked, this,
            [this](const QString& lane, const QString& tpl) { new_from_template(tpl, lane, QString()); });
    connect(board_view_, &notes::NotesBoardView::peek_requested, this, &NotesScreen::peek);
    connect(board_view_, &notes::NotesBoardView::delete_requested, this, [this]() { delete_notes(selected_ids()); });
    connect(board_view_, &notes::NotesBoardView::pin_requested, this, pin_selection);
    connect(board_view_, &notes::NotesBoardView::search_requested, this, [this]() { bar_->focus_command_field(); });
    connect(board_view_, &notes::NotesBoardView::escape_pressed, this, [this]() {
        hide_peek();
        if (focus_mode_)
            set_focus_mode(false);
    });
    connect(board_view_, &notes::NotesBoardView::move_by_key, this, &NotesScreen::move_by_key);
    connect(board_view_, &notes::NotesBoardView::cards_dropped, this, &NotesScreen::move_cards);
    connect(board_view_, &notes::NotesBoardView::symbol_dropped, this, &NotesScreen::add_symbol_to_note);
    connect(board_view_, &notes::NotesBoardView::lane_toggle_requested, this, &NotesScreen::toggle_lane);
    connect(board_view_, &notes::NotesBoardView::lane_moved, this, &NotesScreen::move_lane);
    connect(board_view_, &notes::NotesBoardView::lane_menu_requested, this, &NotesScreen::show_lane_menu);
    connect(board_view_, &notes::NotesBoardView::card_menu_requested, this, &NotesScreen::show_card_menu);
    connect(board_view_, &notes::NotesBoardView::visible_tickers_changed, this, &NotesScreen::on_visible_tickers);
    connect(board_view_, &notes::NotesBoardView::selection_ids_changed, this, &NotesScreen::on_selection_changed);
    connect(board_view_, &notes::NotesBoardView::current_card_changed, this, [this](int id) {
        if (peek_ && peek_->isVisible() && id > 0)
            peek(id);
    });

    // ── WALL ─────────────────────────────────────────────────────────────────
    wall_page_ = new QWidget(view_stack_);
    wall_page_->setObjectName(QStringLiteral("notesLeft"));
    auto* wl = new QVBoxLayout(wall_page_);
    wl->setContentsMargins(0, 0, 0, 0);
    wl->setSpacing(0);
    auto* wbar = new QWidget(wall_page_);
    wbar->setObjectName(QStringLiteral("notesWallBar"));
    wbar->setAttribute(Qt::WA_StyledBackground, true);
    auto* wbl = new QHBoxLayout(wbar);
    wbl->setContentsMargins(10, 4, 10, 4);
    wbl->setSpacing(4);
    const char* const wall_actions[] = {"zoom_out", "zoom_in", "fit", "reset", "snap", "group"};
    for (const char* a : wall_actions) {
        auto* b = new QToolButton(wbar);
        b->setObjectName(QStringLiteral("notesSmallBtn"));
        b->setCursor(Qt::PointingHandCursor);
        const QString id = QString::fromLatin1(a);
        if (id == QLatin1String("snap") || id == QLatin1String("group"))
            b->setCheckable(true);
        b->setProperty("wall_action", id);
        wbl->addWidget(b);
        wall_btns_ << b;
        connect(b, &QToolButton::clicked, this, [this, id, b]() {
            if (id == QLatin1String("zoom_out")) {
                wall_view_->zoom_by(1.0 / 1.2);
            } else if (id == QLatin1String("zoom_in")) {
                wall_view_->zoom_by(1.2);
            } else if (id == QLatin1String("fit")) {
                wall_view_->zoom_to_fit();
            } else if (id == QLatin1String("reset")) {
                wall_view_->set_zoom(1.0);
            } else if (id == QLatin1String("snap")) {
                wall_view_->set_snap(b->isChecked());
                board().wall_snap = b->isChecked();
                board_changed(false);
            } else if (id == QLatin1String("group")) {
                wall_view_->set_draw_group_mode(b->isChecked());
                wall_view_->setFocus(Qt::ShortcutFocusReason);
            }
        });
    }
    wall_hint_ = new QLabel(wbar);
    wall_hint_->setObjectName(QStringLiteral("notesHint"));
    wbl->addSpacing(8);
    wbl->addWidget(wall_hint_, 1);
    wl->addWidget(wbar);
    wall_view_ = new notes::NotesWallView(wall_page_);
    wall_view_->setProperty("fincept_agent_risk_keys", "Delete=delete;Ctrl+Backspace=delete"); // agents: Delete deletes the selected notes
    wall_view_->set_quotes(&quotes_);
    wall_view_->installEventFilter(this);
    wl->addWidget(wall_view_, 1);
    view_stack_->addWidget(wall_page_);
    connect(wall_view_, &notes::NotesWallView::draw_group_finished, this, [this]() {
        for (QToolButton* b : wall_btns_)
            if (b->property("wall_action").toString() == QLatin1String("group"))
                b->setChecked(false);
    });
    connect(wall_view_, &notes::NotesWallView::positions_changed, this,
            [this](const QVector<nb::NoteWallItem>& moved) {
                if (wall_view_->board_id() <= 0)
                    return;
                if (wall_dirty_board_ != wall_view_->board_id()) {
                    flush_wall(false);
                    wall_dirty_board_ = wall_view_->board_id();
                }
                for (const auto& w : moved)
                    wall_dirty_.insert(w.note_id, w);
                wall_timer_->start();
            });
    connect(wall_view_, &notes::NotesWallView::group_saved, this, [this](const nb::NoteWallGroup& g) {
        nb::NoteWallGroup copy = g;
        const qint64 temp = g.id;
        if (copy.id < 0)
            copy.id = 0;
        QPointer<notes::NotesWallView> wall = wall_view_;
        store_->save_wall_group(copy, [wall, temp](qint64 id) {
            if (wall && temp < 0 && id > 0)
                wall->adopt_group_id(temp, id);
        });
    });
    connect(wall_view_, &notes::NotesWallView::group_removed, this,
            [this](qint64 id) { store_->remove_wall_group(id, nullptr); });
    connect(wall_view_, &notes::NotesWallView::group_title_requested, this,
            [this](qint64 id, const QString& current) {
                bool ok = false;
                const QString t = QInputDialog::getText(this, tr("Group title"), tr("Title of this group on the wall:"),
                                                        QLineEdit::Normal, current, &ok);
                wall_view_->set_group_title(id, ok ? t : current);
            });
    connect(wall_view_, &notes::NotesWallView::view_changed, this, [this](double zoom, const QPointF& c) {
        board().wall_zoom = zoom;
        board().wall_cx = c.x();
        board().wall_cy = c.y();
        board_changed(false);
    });
    connect(wall_view_, &notes::NotesWallView::open_requested, this, open_from_view);
    connect(wall_view_, &notes::NotesWallView::new_card_requested, this, &NotesScreen::new_card_on_wall);
    connect(wall_view_, &notes::NotesWallView::peek_requested, this, &NotesScreen::peek);
    connect(wall_view_, &notes::NotesWallView::delete_requested, this, [this]() { delete_notes(selected_ids()); });
    connect(wall_view_, &notes::NotesWallView::pin_requested, this, pin_selection);
    connect(wall_view_, &notes::NotesWallView::search_requested, this, [this]() { bar_->focus_command_field(); });
    connect(wall_view_, &notes::NotesWallView::selection_ids_changed, this, &NotesScreen::on_selection_changed);
    connect(wall_view_, &notes::NotesWallView::current_card_changed, this, [this](int id) {
        if (id > 0 && selected_ids().size() <= 1)
            open_note(id);
    });
    connect(wall_view_, &notes::NotesWallView::card_menu_requested, this, &NotesScreen::show_card_menu);
    connect(wall_view_, &notes::NotesWallView::visible_tickers_changed, this, &NotesScreen::on_visible_tickers);
    connect(wall_view_, &notes::NotesWallView::symbol_dropped, this, &NotesScreen::add_symbol_to_note);

    // ── LIST ─────────────────────────────────────────────────────────────────
    table_model_ = new notes::NotesTableModel(this);
    table_model_->set_quotes(&quotes_);
    table_proxy_ = new QSortFilterProxyModel(this);
    table_proxy_->setSourceModel(table_model_);
    table_proxy_->setSortRole(notes::NotesTableModel::SortRole);
    table_proxy_->setSortCaseSensitivity(Qt::CaseInsensitive);
    table_view_ = new QTableView(view_stack_);
    table_view_->setProperty("fincept_agent_risk_keys", "Delete=delete"); // agents: Delete deletes the selected notes
    table_view_->setObjectName(QStringLiteral("notesTable"));
    table_view_->setModel(table_proxy_);
    table_view_->setItemDelegate(new fincept::ui::FlashCellDelegate(notes::NotesTableModel::FlashRole, table_view_));
    table_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_view_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_view_->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    table_view_->setSortingEnabled(true);
    table_view_->sortByColumn(-1, Qt::AscendingOrder); // board order until a header is clicked
    table_view_->setAlternatingRowColors(true);
    table_view_->setShowGrid(false);
    table_view_->setWordWrap(false);
    table_view_->setFrameShape(QFrame::NoFrame);
    table_view_->verticalHeader()->setVisible(false);
    table_view_->horizontalHeader()->setStretchLastSection(false);
    table_view_->horizontalHeader()->setSectionsMovable(true);
    table_view_->horizontalHeader()->setHighlightSections(false);
    table_view_->setContextMenuPolicy(Qt::CustomContextMenu);
    table_view_->installEventFilter(this);
    view_stack_->addWidget(table_view_);
    {
        // Column widths from the font (no resize-to-contents on fills).
        const QFontMetrics fm(fonts::font(-2));
        const int c = fm.averageCharWidth();
        const int widths[] = {c * 40, c * 16, c * 18, c * 14, c * 16, c * 10, c * 18, c * 11, c * 11, c * 11};
        for (int i = 0; i < notes::NotesTableModel::ColumnCount; ++i)
            table_view_->setColumnWidth(i, widths[i]);
    }
    connect(table_model_, &notes::NotesTableModel::rename_requested, this, [this](int id, const QString& title) {
        const notes::NoteEntry* e = find_entry(id);
        if (!e)
            return;
        if (editor_open_ && open_id() == id)
            save_editor_async();
        fincept::FinancialNote n = e->note;
        n.title = title;
        n.updated_at = notes::db_time(QDateTime::currentDateTimeUtc());
        upsert_entry(n);
        write_notes({n});
        if (editor_open_ && open_id() == id)
            open_note(id, true); // the editor shows the new title
    });
    connect(table_view_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &NotesScreen::on_selection_changed);
    connect(table_view_, &QTableView::doubleClicked, this, [this](const QModelIndex& idx) {
        const int id = idx.data(notes::NotesTableModel::IdRole).toInt();
        open_note(id);
        set_focus_mode(true);
        if (editor_)
            editor_->focus_body();
    });
    connect(table_view_, &QTableView::customContextMenuRequested, this, [this](const QPoint& pos) {
        const QModelIndex at = table_view_->indexAt(pos);
        if (!at.isValid())
            return;
        if (!table_view_->selectionModel()->isSelected(at))
            table_view_->selectionModel()->select(at, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        show_card_menu(table_view_->viewport()->mapToGlobal(pos));
    });
    connect(table_view_->verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() { subscribe_timer_->start(); });

    view_stack_->setCurrentWidget(skeleton_);
}

void NotesScreen::build_status_bar() {
    using SB = fincept::ui::ScreenStatusBar;
    status_ = new SB(this);
    status_->add_item(QStringLiteral("board"), SB::Segment::Left, QString(), SB::Tone::Accent, 100);
    status_->add_item(QStringLiteral("count"), SB::Segment::Left, QString(), SB::Tone::Normal, 95);
    status_->add_item(QStringLiteral("selection"), SB::Segment::Left, QString(), SB::Tone::Info, 70);
    status_->add_item(QStringLiteral("view"), SB::Segment::Left, QString(), SB::Tone::Dim, 30);
    status_->add_item(QStringLiteral("quotes"), SB::Segment::Right, QString(), SB::Tone::Dim, 40);
    status_->add_item(QStringLiteral("save"), SB::Segment::Right, QString(), SB::Tone::Ok, 90);
    status_->set_chip(QStringLiteral("save"), true);
    status_->set_clock(true, /*seconds=*/true);
}

void NotesScreen::build_peek() {
    peek_ = new QFrame(this, Qt::ToolTip | Qt::FramelessWindowHint);
    peek_->setObjectName(QStringLiteral("notesPeek"));
    peek_->setAttribute(Qt::WA_ShowWithoutActivating, true);
    auto* l = new QVBoxLayout(peek_);
    l->setContentsMargins(12, 10, 12, 10);
    l->setSpacing(6);
    peek_title_ = new QLabel(peek_);
    peek_title_->setObjectName(QStringLiteral("notesPeekTitle"));
    peek_title_->setWordWrap(true);
    peek_title_->setTextFormat(Qt::PlainText);
    l->addWidget(peek_title_);
    peek_body_ = new QTextBrowser(peek_);
    peek_body_->setObjectName(QStringLiteral("notesPeekBody"));
    peek_body_->setOpenLinks(false);
    peek_body_->setFrameShape(QFrame::NoFrame);
    peek_body_->setFocusPolicy(Qt::NoFocus);
    l->addWidget(peek_body_, 1);
    peek_->hide();
}

void NotesScreen::install_shortcuts() {
    const auto add_sc = [this](const QKeySequence& seq, auto fn) {
        auto* sc = new QShortcut(seq, this);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, this, fn);
    };
    add_sc(QKeySequence(QKeySequence::New), [this]() { new_card(board_view_->current_lane_key()); });
    add_sc(QKeySequence(QKeySequence::Find), [this]() { bar_->focus_command_field(); });
    add_sc(QKeySequence(QKeySequence::Save), [this]() {
        flush_meta(false);
        if (!editor_open_)
            return;
        if (editor_ && open_id() > 0 && editor_->revision() == queued_revision_ && saves_in_flight_ == 0)
            editor_->set_save_state(notes::NotesEditor::SaveState::Saved,
                                    QTime::currentTime().toString(QStringLiteral("HH:mm")));
        save_editor_async();
    });
    add_sc(QKeySequence(Qt::CTRL | Qt::Key_D), [this]() {
        const int id = editor_open_ ? open_id() : 0;
        if (id > 0)
            duplicate(id);
    });
    add_sc(QKeySequence(Qt::CTRL | Qt::Key_E), [this]() {
        if (!editor_open_ || !editor_)
            return;
        editor_mode_ = (editor_mode_ + 1) % 3;
        editor_->set_mode(static_cast<notes::NotesEditor::Mode>(editor_mode_));
        notify_state_changed();
    });
    add_sc(QKeySequence(Qt::ALT | Qt::Key_1), [this]() { set_view_mode(BoardViewMode::Board); });
    add_sc(QKeySequence(Qt::ALT | Qt::Key_2), [this]() { set_view_mode(BoardViewMode::Wall); });
    add_sc(QKeySequence(Qt::ALT | Qt::Key_3), [this]() { set_view_mode(BoardViewMode::List); });
    add_sc(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D), [this]() {
        set_focus_mode(false);
        set_detail_visible(!detail_visible_);
    });
    // Full editor <-> board. (Not Esc: text fields and the symbol picker own Esc.)
    add_sc(QKeySequence(Qt::CTRL | Qt::Key_Return), [this]() { set_focus_mode(!focus_mode_); });
}

// ── Appearance ───────────────────────────────────────────────────────────────

void NotesScreen::refit() {
    // After every widget has its new style: re-measure what is sized from fonts.
    board_view_->refit();
    wall_view_->refit();
    detail_->refit();
    if (editor_)
        editor_->refresh_theme();
    const QFontMetrics fm(fonts::font(-2));
    table_view_->verticalHeader()->setDefaultSectionSize(std::max(22, fm.height() + 8));
    table_model_->refresh_all();
    const QFontMetrics hm(fonts::font(-4, QFont::Bold));
    table_view_->horizontalHeader()->setMinimumHeight(std::max(22, hm.height() + 10));
    const int bh = std::max(22, QFontMetrics(fonts::font(-4, QFont::DemiBold)).height() + 8);
    for (QToolButton* b : wall_btns_)
        b->setMinimumHeight(bh);
    update_ribbon();
}

// ── Translation ──────────────────────────────────────────────────────────────

void NotesScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void NotesScreen::retranslateUi() {
    bar_->set_command_field(tr("Search or command   $TICKER  #tag   new trade idea AAPL   group by ticker   ? help"),
                            QStringLiteral("/"));
    bar_->set_overflow_text(tr("More"));
    bar_->set_action_text(QStringLiteral("new"), tr("NEW"));
    bar_->set_action_tooltip(QStringLiteral("new"),
                             tr("New card in the focused lane (N on the board, Ctrl+N anywhere)"));
    bar_->set_action_text(QStringLiteral("template"), tr("TEMPLATE"));
    bar_->set_action_tooltip(QStringLiteral("template"), tr("New card from a template (T on the board)"));
    bar_->set_segment_text(QStringLiteral("view"), QStringLiteral("board"), tr("BOARD"), tr("Cards in lanes (Alt+1)"));
    bar_->set_segment_text(QStringLiteral("view"), QStringLiteral("wall"), tr("WALL"),
                           tr("Free-form wall: place, group and zoom (Alt+2)"));
    bar_->set_segment_text(QStringLiteral("view"), QStringLiteral("list"), tr("LIST"), tr("Sortable table (Alt+3)"));
    bar_->set_action_tooltip(QStringLiteral("group"),
                             tr("Lanes by status, category, ticker, priority, tag or date (G on the board)"));
    bar_->set_action_text(QStringLiteral("import"), tr("IMPORT"));
    bar_->set_action_tooltip(QStringLiteral("import"), tr("Import Markdown / text files as notes"));
    bar_->set_action_text(QStringLiteral("export"), tr("EXPORT"));
    bar_->set_action_tooltip(QStringLiteral("export"), tr("Export the selection or the board as Markdown"));
    bar_->set_action_text(QStringLiteral("detail"), tr("DETAIL"));
    bar_->set_action_tooltip(QStringLiteral("detail"), tr("Show the detail pane (Ctrl+Shift+D)"));
    bar_->set_chips_label(tr("FILTER"));
    bar_->set_chip_text(QStringLiteral("pinned"), tr("PINNED"));
    bar_->set_chip_tooltip(QStringLiteral("pinned"), tr("Only pinned notes (the dashboard's favourites)"));
    bar_->set_chip_text(QStringLiteral("ticker"), tr("HAS TICKER"));
    bar_->set_chip_tooltip(QStringLiteral("ticker"), tr("Only notes that link a ticker"));
    bar_->set_chip_text(QStringLiteral("due"), tr("DUE"));
    bar_->set_chip_tooltip(QStringLiteral("due"), tr("Only notes whose review date has come"));
    bar_->set_chip_text(QStringLiteral("linked"), tr("FROM CHAT"));
    bar_->set_chip_tooltip(QStringLiteral("linked"), tr("Only notes saved from a Chat Mode run"));
    bar_->set_chip_text(QStringLiteral("archived"), tr("ARCHIVED"));
    bar_->set_chip_tooltip(QStringLiteral("archived"),
                           tr("Show archived notes (an ARCHIVED lane when grouped by status)"));
    bar_->set_chip_text(QStringLiteral("clear"), tr("CLEAR"));
    bar_->set_chip_tooltip(QStringLiteral("clear"), tr("Clear the search and every filter of this board"));

    const auto tile = [this](const char* key, const QString& label, const QString& tip) {
        if (auto* t = ribbon_->tile(QString::fromLatin1(key))) {
            t->set_label(label);
            t->set_tooltip_text(tip);
        }
    };
    tile("notes", tr("NOTES"), tr("Notes in the notebook (archived ones not counted). Click: clear the filters."));
    tile("active", tr("ACTIVE IDEAS"), tr("Open notes in IDEA, RESEARCHING or ACTIVE. Click: show only those."));
    tile("due", tr("DUE FOR REVIEW"), tr("Open notes whose review date is today or past. Click: show only those."));
    tile("pinned", tr("PINNED"), tr("Pinned notes (the dashboard's favourites). Click: show only those."));
    tile("tickers", tr("TICKERS"),
         tr("Distinct tickers linked by notes, with how many are up / down today among those with a quote. Click: "
            "notes with a ticker, by ticker."));
    tile("week", tr("EDITED THIS WEEK"), tr("Notes edited in the last 7 days. Click: show only those."));
    tile("oldest", tr("OLDEST UNREVIEWED"),
         tr("Days since the open note you looked at longest ago (edited or marked reviewed). Click: open notes "
            "untouched for 30 days or more."));

    board_view_->setAccessibleName(tr("Notes board"));
    board_view_->set_hint_text(tr("+ Add card"), tr("+ New card"),
                               tr("N or double-click adds a card to this lane. Drag cards here to move them."));
    wall_view_->setAccessibleName(tr("Notes wall"));
    const QStringList wall_labels = {QStringLiteral("−"), QStringLiteral("+"), tr("FIT"), tr("100%"), tr("SNAP"),
                                     tr("DRAW GROUP")};
    const QStringList wall_tips = {tr("Zoom out (Ctrl+-)"),      tr("Zoom in (Ctrl+=)"),
                                   tr("Fit every card (F)"),      tr("Actual size (Ctrl+0)"),
                                   tr("Snap cards to the grid"), tr("Drag a box on empty space to make a titled group")};
    for (int i = 0; i < wall_btns_.size() && i < wall_labels.size(); ++i) {
        wall_btns_.at(i)->setText(wall_labels.at(i));
        wall_btns_.at(i)->setToolTip(wall_tips.at(i));
        wall_btns_.at(i)->setAccessibleName(wall_tips.at(i));
    }
    wall_hint_->setText(tr("Drag cards anywhere · Ctrl+wheel zoom · Space+drag or middle button pan · arrows nudge · "
                           "double-click empty space for a new card"));
    table_view_->setAccessibleName(tr("Notes list"));
    skeleton_->set_message(tr("Loading notes…"));
    update_chips();
    update_ribbon();
    update_status_bar();
    update_ghosts();
    update_empty_detail();
    if (loaded_)
        apply_layout(); // lane titles are translated
}

// ── Menus ────────────────────────────────────────────────────────────────────

void NotesScreen::populate_board_menu() {
    board_menu_->clear();
    board_menu_->addSection(tr("Boards"));
    for (int i = 0; i < boards_.size(); ++i) {
        QString name = board_display_name(boards_.at(i));
        name.replace(QLatin1Char('&'), QStringLiteral("&&"));
        QAction* a = board_menu_->addAction(name);
        a->setCheckable(true);
        a->setChecked(i == board_idx_);
        connect(a, &QAction::triggered, this, [this, i]() { switch_board(i); });
    }
    board_menu_->addSeparator();
    board_menu_->addAction(tr("New board…"), this, &NotesScreen::new_board);
    board_menu_->addAction(tr("Duplicate this board…"), this, &NotesScreen::duplicate_board);
    board_menu_->addAction(tr("Rename this board…"), this, &NotesScreen::rename_board);
    QAction* up = board_menu_->addAction(tr("Move this board up"), this, [this]() { move_board(-1); });
    up->setEnabled(loaded_ && board_idx_ > 0);
    QAction* down = board_menu_->addAction(tr("Move this board down"), this, [this]() { move_board(1); });
    down->setEnabled(loaded_ && board_idx_ < boards_.size() - 1);
    QAction* del = board_menu_->addAction(tr("Delete this board…"), this, &NotesScreen::delete_board);
    del->setProperty("fincept_agent_risk", "delete"); // agents: deletes the board and its notes
    del->setEnabled(boards_.size() > 1);
    if (!board_tables_) {
        board_menu_->addSeparator();
        QAction* note = board_menu_->addAction(tr("Boards cannot be saved: the database is older than this build"));
        note->setEnabled(false);
    }
}

void NotesScreen::populate_template_menu() {
    template_menu_->clear();
    template_menu_->addSection(tr("Templates"));
    const QString lane = board_view_->current_lane_key();
    for (const auto& t : notes::builtin_templates()) {
        const QString id = t.id;
        template_menu_->addAction(QCoreApplication::translate("NotesBoard", t.name.toUtf8().constData()), this,
                                  [this, id, lane]() { new_from_template(id, lane, QString()); });
    }
    if (!user_templates_.isEmpty()) {
        template_menu_->addSection(tr("My templates"));
        for (int i = 0; i < user_templates_.size(); ++i) {
            QString name = user_templates_.at(i).name;
            name.replace(QLatin1Char('&'), QStringLiteral("&&"));
            template_menu_->addAction(name, this, [this, i]() { new_from_user_template(i); });
        }
    }
    template_menu_->addSeparator();
    QAction* save =
        template_menu_->addAction(tr("Save the open note as a template…"), this, &NotesScreen::save_as_template);
    save->setEnabled(editor_open_);
}

void NotesScreen::populate_group_menu() {
    group_menu_->clear();
    const struct {
        GroupBy g;
        QString label;
    } groups[] = {{GroupBy::Status, tr("Status (workflow)")}, {GroupBy::Category, tr("Category")},
                  {GroupBy::Ticker, tr("Ticker")},            {GroupBy::Priority, tr("Priority")},
                  {GroupBy::Tag, tr("Tag")},                  {GroupBy::Date, tr("Date edited")}};
    for (const auto& g : groups) {
        QAction* a = group_menu_->addAction(g.label);
        a->setCheckable(true);
        a->setChecked(board().group_by == g.g);
        const GroupBy gb = g.g;
        connect(a, &QAction::triggered, this, [this, gb]() { set_group_by(gb); });
    }
    group_menu_->addSeparator();
    group_menu_->addAction(tr("Expand every lane"), this, [this]() {
        board().collapsed.remove(notes::group_by_key(board().group_by));
        board_changed();
    });
    group_menu_->addAction(tr("Reset lane order"), this, [this]() {
        board().lane_order.remove(notes::group_by_key(board().group_by));
        board_changed();
    });
}

void NotesScreen::populate_export_menu() {
    export_menu_->clear();
    const QVector<int> sel = selected_ids();
    QVector<int> shown;
    for (const auto& lane : layout_.lanes)
        for (const auto& e : lane.cards)
            shown << e.note.id;
    QAction* a = export_menu_->addAction(tr("Selected notes (%1) to the File Manager").arg(sel.size()), this,
                                         [this, sel]() { export_notes(sel, true); });
    a->setEnabled(!sel.isEmpty());
    a = export_menu_->addAction(tr("Selected notes (%1) to a folder…").arg(sel.size()), this,
                                [this, sel]() { export_notes(sel, false); });
    a->setEnabled(!sel.isEmpty());
    export_menu_->addSeparator();
    a = export_menu_->addAction(tr("This board (%1 notes) to the File Manager").arg(shown.size()), this,
                                [this, shown]() { export_notes(shown, true); });
    a->setEnabled(!shown.isEmpty());
    a = export_menu_->addAction(tr("This board (%1 notes) to a folder…").arg(shown.size()), this,
                                [this, shown]() { export_notes(shown, false); });
    a->setEnabled(!shown.isEmpty());
}

void NotesScreen::populate_priority_menu() {
    priority_menu_->clear();
    for (const QString& code : notes::priority_codes()) {
        QAction* a = priority_menu_->addAction(notes::priority_label(code));
        a->setCheckable(true);
        a->setChecked(board().filter.priorities.contains(code));
        connect(a, &QAction::triggered, this,
                [this, code]() { set_filter(notes::toggle_filter_key(board().filter, code.toLower())); });
    }
    priority_menu_->addSeparator();
    priority_menu_->addAction(tr("Any priority"), this, [this]() {
        notes::BoardFilter f = board().filter;
        f.priorities.clear();
        set_filter(f);
    });
}

void NotesScreen::populate_tag_menu() {
    tag_menu_->clear();
    const QString current = board().filter.tag;
    QAction* any = tag_menu_->addAction(tr("Any tag"));
    any->setCheckable(true);
    any->setChecked(current.isEmpty());
    connect(any, &QAction::triggered, this, [this]() {
        notes::BoardFilter f = board().filter;
        f.tag.clear();
        set_filter(f);
    });
    if (!layout_.top_tags.isEmpty())
        tag_menu_->addSeparator();
    for (int i = 0; i < layout_.top_tags.size() && i < 40; ++i) {
        const QString tag = layout_.top_tags.at(i).first;
        QString label = QStringLiteral("#%1  (%2)").arg(tag).arg(layout_.top_tags.at(i).second);
        label.replace(QLatin1Char('&'), QStringLiteral("&&"));
        QAction* a = tag_menu_->addAction(label);
        a->setCheckable(true);
        a->setChecked(current.compare(tag, Qt::CaseInsensitive) == 0);
        connect(a, &QAction::triggered, this, [this, tag]() {
            notes::BoardFilter f = board().filter;
            f.tag = f.tag.compare(tag, Qt::CaseInsensitive) == 0 ? QString() : tag;
            set_filter(f);
        });
    }
}

void NotesScreen::populate_date_menu() {
    date_menu_->clear();
    const struct {
        int days;
        QString label;
    } choices[] = {{0, tr("Any time")},
                   {1, tr("Edited today")},
                   {7, tr("Edited in the last 7 days")},
                   {30, tr("Edited in the last 30 days")},
                   {90, tr("Edited in the last 90 days")}};
    for (const auto& c : choices) {
        QAction* a = date_menu_->addAction(c.label);
        a->setCheckable(true);
        a->setChecked(board().filter.edited_days == c.days);
        const int days = c.days;
        connect(a, &QAction::triggered, this, [this, days]() {
            notes::BoardFilter f = board().filter;
            f.edited_days = days;
            set_filter(f);
        });
    }
}

// ── Keyboard on the views ────────────────────────────────────────────────────

bool NotesScreen::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::FocusOut && (obj == board_view_ || obj == wall_view_ || obj == table_view_))
        hide_peek();
    if (event->type() != QEvent::KeyPress)
        return QWidget::eventFilter(obj, event);
    auto* ke = static_cast<QKeyEvent*>(event);
    const bool plain = !(ke->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
    // (While a title is renamed in the list, keys go to its line edit, not to the view.)
    if (obj == board_view_ || obj == wall_view_ || obj == table_view_) {
        if (ke->matches(QKeySequence::Undo) && status_->notice_has_action()) {
            status_->trigger_notice_action();
            return true;
        }
        if (plain && ke->key() == Qt::Key_T) {
            QPushButton* b = bar_->action(QStringLiteral("template"));
            template_menu_->exec(b && b->isVisible() ? b->mapToGlobal(QPoint(0, b->height())) : QCursor::pos());
            return true;
        }
        if (plain && ke->key() == Qt::Key_G) {
            QPushButton* b = bar_->action(QStringLiteral("group"));
            group_menu_->exec(b && b->isVisible() ? b->mapToGlobal(QPoint(0, b->height())) : QCursor::pos());
            return true;
        }
    }
    if (obj == table_view_) {
        const QModelIndex cur = table_view_->currentIndex();
        switch (ke->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (cur.isValid()) {
                open_note(cur.data(notes::NotesTableModel::IdRole).toInt());
                set_focus_mode(true);
                if (editor_)
                    editor_->focus_body();
            }
            return true;
        case Qt::Key_Delete:
            delete_notes(selected_ids());
            return true;
        case Qt::Key_Slash:
            bar_->focus_command_field();
            return true;
        case Qt::Key_Space:
            if (cur.isValid())
                peek(cur.data(notes::NotesTableModel::IdRole).toInt());
            return true;
        case Qt::Key_P:
            if (plain) {
                const QVector<int> ids = selected_ids();
                bool any_unpinned = false;
                for (int id : ids)
                    if (const notes::NoteEntry* e = find_entry(id))
                        any_unpinned = any_unpinned || !e->note.is_favorite;
                set_pinned(ids, any_unpinned);
                return true;
            }
            break;
        case Qt::Key_N:
            if (plain) {
                new_card(QString());
                return true;
            }
            break;
        default:
            break;
        }
    }
    return QWidget::eventFilter(obj, event);
}

// ── Visibility ───────────────────────────────────────────────────────────────

void NotesScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    const auto claim = [this]() {
        QWidget* v = view_stack_->currentWidget();
        nsc_claim_focus(this, v == wall_page_ ? static_cast<QWidget*>(wall_view_) : v);
    };
    QTimer::singleShot(0, this, claim);
    QTimer::singleShot(300, this, claim);
    subscribe_events();
    status_->start_clock();
    // notes.* events are only received while visible: pick up what changed meanwhile.
    if (loaded_ && (!last_load_.isValid() || last_load_.elapsed() > 1500))
        request_reload();
    subscribe_timer_->start();
    fincept::services::cloud::CloudSyncEngine::instance().request_pull(QStringLiteral("note"));
}

void NotesScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    save_editor_async(); // a tab switch never loses typing
    flush_meta(false);
    flush_wall(false);
    flush_board(false);
    hide_peek();
    status_->stop_clock();
    unsubscribe_events();
    unsubscribe_all_quotes();
}

void NotesScreen::notify_state_changed() {
    ScreenStateManager::instance().notify_changed(this);
}

void NotesScreen::subscribe_events() {
    if (!event_subs_.isEmpty())
        return;
    QPointer<NotesScreen> self = this;
    auto on_changed = [self](const QVariantMap& payload) {
        if (!self || payload.value(QStringLiteral("source")).toString() == QLatin1String("notes_screen"))
            return;
        // Chat Mode "Send to Notes": remember the run the note came from (when the event carries it).
        const int id = payload.value(QStringLiteral("id")).toInt();
        QString run;
        if (const auto from_chat = notes::chat_run_from_event(payload))
            run = from_chat->isEmpty() ? QStringLiteral(" ") : *from_chat; // " ": linked, run id unknown
        QMetaObject::invokeMethod(
            self.data(),
            [self, id, run]() {
                if (!self)
                    return;
                if (id > 0 && !run.isEmpty())
                    self->pending_origin_.insert(id, run.trimmed());
                self->request_reload();
            },
            Qt::QueuedConnection);
    };
    auto& bus = EventBus::instance();
    event_subs_.append(bus.subscribe(this, "notes.created", on_changed));
    event_subs_.append(bus.subscribe(this, "notes.deleted", on_changed));
    event_subs_.append(bus.subscribe(this, "notes.updated", on_changed));
}

void NotesScreen::unsubscribe_events() {
    auto& bus = EventBus::instance();
    for (auto id : event_subs_)
        bus.unsubscribe(id);
    event_subs_.clear();
}

// ── IStatefulScreen ──────────────────────────────────────────────────────────

QVariantMap NotesScreen::save_state() const {
    QVariantMap state{
        {"board_id", board().id},
        {"board_index", board_idx_},
        {"note_id", open_id() > 0 ? open_id() : -1},
        {"mode", editor_mode_},
        {"detail", detail_visible_},
        {"focus", focus_mode_},
        {"search", search_text_},
    };
    if (split_)
        state["split"] = QString::fromLatin1(split_->saveState().toBase64());
    return state;
}

void NotesScreen::restore_state(const QVariantMap& state) {
    if (state.contains("mode")) {
        editor_mode_ = std::clamp(state.value("mode").toInt(), 0, 2);
        if (editor_)
            editor_->set_mode(static_cast<notes::NotesEditor::Mode>(editor_mode_));
    }
    if (state.contains("split") && split_)
        split_->restoreState(QByteArray::fromBase64(state.value("split").toString().toLatin1()));
    if (state.contains("detail"))
        set_detail_visible(state.value("detail").toBool());
    if (!loaded_) {
        // Board, search and the open note are applied once the library has loaded.
        pending_restore_ = state;
        has_pending_restore_ = true;
        return;
    }
    // The board (by id, else by index).
    const qint64 bid = state.value("board_id").toLongLong();
    int idx = -1;
    for (int i = 0; i < boards_.size(); ++i)
        if (bid > 0 && boards_.at(i).id == bid)
            idx = i;
    if (idx < 0 && state.contains("board_index"))
        idx = std::clamp(state.value("board_index").toInt(), 0, static_cast<int>(boards_.size()) - 1);
    if (idx >= 0 && idx != board_idx_)
        switch_board(idx);
    // Version 1 state (the three-pane screen): its view becomes a filter on this board.
    if (state.contains("view") && !state.contains("board_id")) {
        const notes::ViewFilter v = notes::ViewFilter::from_string(state.value("view").toString());
        notes::BoardFilter f = board().filter;
        switch (v.kind) {
        case notes::ViewKind::Pinned:
            f.pinned_only = true;
            break;
        case notes::ViewKind::Archived:
            f.show_archived = true;
            break;
        case notes::ViewKind::Category:
            f.categories = {v.key};
            break;
        case notes::ViewKind::Tag:
            f.tag = v.key;
            break;
        case notes::ViewKind::Ticker:
            f.ticker = v.key;
            break;
        case notes::ViewKind::All:
            break;
        }
        if (f != board().filter)
            set_filter(f);
    }
    const QString search = state.value("search").toString();
    if (!search.isEmpty()) {
        bar_->set_command_text(search);
        apply_search(search);
    }
    const int note_id = state.value("note_id", -1).toInt();
    if (note_id > 0 && find_entry(note_id)) {
        select_in_views({note_id}, note_id, true);
        open_note(note_id);
        if (state.value("focus").toBool())
            set_focus_mode(true);
    } else if (state.value("editing", false).toBool() && note_id <= 0) {
        // An unsaved new note from an early version: bring its draft back as a note.
        fincept::FinancialNote draft;
        draft.title = state.value("draft_title").toString();
        draft.content = state.value("draft_content").toString();
        draft.tags = state.value("draft_tags").toString();
        draft.tickers = state.value("draft_tickers").toString();
        draft.priority = state.value("draft_priority", "MEDIUM").toString();
        draft.sentiment = state.value("draft_sentiment", "NEUTRAL").toString();
        draft.category = QStringLiteral("GENERAL");
        if (!draft.title.trimmed().isEmpty() || !draft.content.trimmed().isEmpty()) {
            nb::NoteBoardMeta m;
            m.status = nb::kStatusIdea;
            m.rank = top_rank_of_lane(nb::kStatusIdea);
            open_new_note(draft, true, m);
        }
    }
}

// ── ISectionLinked ───────────────────────────────────────────────────────────

bool NotesScreen::open_section(const QString& section, const QString& focus) {
    // The command bar's own paths: the BOARD | WALL | LIST segment (set_view_mode), NEW
    // (new_card), the board menu (switch_board) and a card click (select + open_note).
    const QString key = section.trimmed().toLower();
    const QString want = focus.trimmed();
    bool ok = false;
    int note_id = 0;
    if (key.startsWith(QLatin1String("note:"))) {
        note_id = key.mid(5).trimmed().toInt(&ok);
        if (!ok || note_id <= 0)
            return false;
    } else if (!key.isEmpty() && key != QLatin1String("board") && key != QLatin1String("wall") &&
               key != QLatin1String("list") && key != QLatin1String("new")) {
        return false;
    }
    qint64 board_id = 0;
    if (want.startsWith(QLatin1String("board:"), Qt::CaseInsensitive)) {
        board_id = want.mid(6).trimmed().toLongLong(&ok);
        if (!ok)
            board_id = 0;
    } else if (note_id <= 0 && !want.isEmpty()) {
        const QString n = want.startsWith(QLatin1String("note:"), Qt::CaseInsensitive) ? want.mid(5).trimmed() : want;
        const int id = n.toInt(&ok);
        if (ok && id > 0)
            note_id = id;
    }
    if (!loaded_) {
        // Applied by on_loaded() once the library is in memory.
        pending_link_section_ = section;
        pending_link_focus_ = focus;
        has_pending_link_ = true;
        return true;
    }
    if (key.startsWith(QLatin1String("note:")) && !find_entry(note_id))
        return false; // no such note: nothing changes
    if (board_id > 0) {
        for (int i = 0; i < boards_.size(); ++i)
            if (boards_.at(i).id == board_id) {
                if (i != board_idx_)
                    switch_board(i);
                break;
            }
    }
    if (key == QLatin1String("board") || key == QLatin1String("wall") || key == QLatin1String("list"))
        set_view_mode(notes::view_mode_from_key(key));
    if (key == QLatin1String("new")) {
        new_card(board_view_->current_lane_key());
        return true;
    }
    if (note_id > 0 && find_entry(note_id)) {
        select_in_views({note_id}, note_id, true);
        open_note(note_id);
    }
    return true;
}

void NotesScreen::flush_pending_state() {
    // Called for every layout capture, including the periodic session snapshot.
    // Board rows go to the worker; the open note is written synchronously only
    // when it has unsaved typing (otherwise waiting would freeze for nothing).
    flush_meta(false);
    flush_wall(false);
    flush_board(false);
    if (!editor_ || !editor_open_ || (editor_->revision() == queued_revision_ && saves_in_flight_ == 0))
        return;
    flush_editor_sync();
}

} // namespace fincept::screens
