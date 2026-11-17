// src/screens/code_editor/CodeEditorScreen.cpp — see CodeEditorScreen.h.
// Layout, notebook lifecycle (open / save / export / autosave / recovery), cell
// structure and keyboard (command / edit modes), running through the kernel,
// find, help, and the persisted preferences.

#include "screens/code_editor/CodeEditorScreen.h"

#include "core/agent/AgentRiskMarkers.h"
#include "core/config/AppPaths.h"
#include "core/keys/KeyConfigManager.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "python/PythonRunner.h"
#include "python/PythonSetupManager.h"
#include "screens/code_editor/CodeEditorScreen_Navigator.h"
#include "screens/code_editor/NotebookOutputs.h"
#include "screens/code_editor/NotebookStyle.h"
#include "screens/common/CommandRows.h"
#include "services/file_manager/FileManagerService.h"
#include "services/notebooks/NotebookKernel.h"
#include "services/notebooks/NotebookLibraryService.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/ScreenStatusBar.h"
#include "ui/widgets/ThemeHelpers.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTextCursor>
#include <QThreadPool>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace fincept::screens {

using notebook::CellOutput;
using notebook::NotebookCell;
using notebook::NotebookDoc;
using services::NotebookKernel;
using services::NotebookLibraryService;
namespace colors = ui::colors;

namespace {

constexpr const char* kTag = "CodeEditor";
constexpr int kFirstBatch = 12;      ///< cells built before the notebook shows
constexpr int kBatch = 8;            ///< cells built per event-loop turn afterwards
constexpr int kMaxRecent = 12;
constexpr int kMaxUndo = 20;
constexpr int kAutosaveMs = 60 * 1000;
constexpr int kDoubleKeyMs = 800;    ///< D,D / I,I / 0,0
constexpr int kTickMs = 200;

QString fmt_ms(qint64 ms) {
    if (ms < 1000)
        return CodeEditorScreen::tr("%1 ms").arg(ms);
    if (ms < 60000)
        return CodeEditorScreen::tr("%1 s").arg(ms / 1000.0, 0, 'f', ms < 10000 ? 2 : 1);
    return CodeEditorScreen::tr("%1m %2s").arg(ms / 60000).arg((ms / 1000) % 60, 2, 10, QLatin1Char('0'));
}

QString strip_ipynb(QString name) {
    if (name.endsWith(QLatin1String(".ipynb"), Qt::CaseInsensitive))
        name.chop(6);
    return name;
}

const QStringList& python_words() {
    static const QStringList words = {
        // data-only static: keywords and common builtins for completion without a kernel
        QStringLiteral("False"), QStringLiteral("None"), QStringLiteral("True"), QStringLiteral("and"),
        QStringLiteral("assert"), QStringLiteral("break"), QStringLiteral("class"), QStringLiteral("continue"),
        QStringLiteral("def"), QStringLiteral("elif"), QStringLiteral("else"), QStringLiteral("except"),
        QStringLiteral("finally"), QStringLiteral("for"), QStringLiteral("from"), QStringLiteral("import"),
        QStringLiteral("lambda"), QStringLiteral("return"), QStringLiteral("while"), QStringLiteral("with"),
        QStringLiteral("yield"), QStringLiteral("print"), QStringLiteral("range"), QStringLiteral("enumerate"),
        QStringLiteral("isinstance"), QStringLiteral("sorted"), QStringLiteral("display"), QStringLiteral("pandas"),
        QStringLiteral("numpy"), QStringLiteral("matplotlib"), QStringLiteral("yfinance"), QStringLiteral("statsmodels")};
    return words;
}

// ── Agent risk of keys the screen handles itself (core/agent/AgentRiskMarkers.h) ──
// Set on the cell canvas — the command-mode key target and the ancestor of every cell
// editor — so the engine judges these presses: the run keys (configurable) run code,
// D, D deletes a cell, 0, 0 restarts the kernel.
QString nbk_risk_keys() {
    auto& km = KeyConfigManager::instance();
    QStringList pairs;
    for (KeyAction a : {KeyAction::RunCell, KeyAction::RunAndNext}) {
        const QString seq = km.key(a).toString(QKeySequence::PortableText);
        if (!seq.isEmpty())
            pairs << seq + QStringLiteral("=code");
    }
    pairs << QStringLiteral("Alt+Return=code") << QStringLiteral("Alt+Enter=code")
          << QStringLiteral("Ctrl+Shift+Return=code") << QStringLiteral("Ctrl+Shift+Enter=code")
          << QStringLiteral("D=delete") << QStringLiteral("0=code");
    return pairs.join(QLatin1Char(';'));
}

// ── Bottom bar: the shared ui::ScreenStatusBar ──────────────────────────────
// kernel state · environment · last run · cells · cursor · save state · notice ......
// file path. The screen keeps its tone words ("ok" | "warn" | "danger" | "dim"; every
// state is also spelled out in words); an item with nothing to say is hidden, and the
// least important items give way first on a narrow window (path, then environment).

using NbsbTone = ui::ScreenStatusBar::Tone;

NbsbTone nbsb_tone(const QString& tone) {
    if (tone == QLatin1String("ok"))
        return NbsbTone::Ok;
    if (tone == QLatin1String("warn"))
        return NbsbTone::Warn;
    if (tone == QLatin1String("danger"))
        return NbsbTone::Danger;
    if (tone == QLatin1String("dim"))
        return NbsbTone::Dim;
    if (tone == QLatin1String("accent"))
        return NbsbTone::Accent;
    return NbsbTone::Normal;
}

void nbsb_add_items(ui::ScreenStatusBar* sb) {
    using S = ui::ScreenStatusBar::Segment;
    const struct {
        const char* id;
        S segment;
        int priority;
    } items[] = {{"kernel", S::Left, 100}, {"env", S::Left, 40},    {"last_run", S::Left, 80}, {"cells", S::Left, 90},
                 {"cursor", S::Left, 60},  {"save", S::Left, 85},   {"path", S::Right, 10}};
    for (const auto& it : items) {
        const QString id = QString::fromLatin1(it.id);
        sb->add_item(id, it.segment, {}, NbsbTone::Normal, it.priority);
        sb->set_item_visible(id, false); // nothing to say yet
    }
}

void nbsb_set(ui::ScreenStatusBar* sb, const char* id, const QString& text, const QString& tone = {},
              const QString& tip = {}) {
    const QString key = QString::fromLatin1(id);
    sb->set_item(key, text, nbsb_tone(tone));
    sb->set_tooltip(key, tip);
    sb->set_item_visible(key, !text.isEmpty());
}

void nbsb_cursor(ui::ScreenStatusBar* sb, int line, int column) {
    if (line > 0)
        nbsb_set(sb, "cursor", QCoreApplication::translate("NotebookStatusBar", "Ln %1, Col %2").arg(line).arg(column));
    else
        sb->set_item_visible(QStringLiteral("cursor"), false);
}

void nbsb_path(ui::ScreenStatusBar* sb, const QString& path) {
    // Elided in the middle (full path in the tooltip); the first item to give way.
    const QString native = QDir::toNativeSeparators(path);
    QString shown = native;
    if (QLabel* l = sb->item(QStringLiteral("path")))
        shown = l->fontMetrics().elidedText(native, Qt::ElideMiddle, l->fontMetrics().averageCharWidth() * 48);
    nbsb_set(sb, "path", shown, QStringLiteral("dim"), native);
}

} // namespace

// ═════════════════════════════════════════════════════════════════════════════
// Construction

CodeEditorScreen::CodeEditorScreen(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("nbScreen"));
    setAttribute(Qt::WA_StyledBackground, true);
    nb_set_sheet(this, nb_screen_sheet()); // the one sheet, before any child exists

    kernel_ = new NotebookKernel(this);
    kernel_->set_chart_style(nb_chart_style());
    build_ui();
    bind_shortcuts();

    tick_timer_ = new QTimer(this);
    tick_timer_->setInterval(kTickMs);
    connect(tick_timer_, &QTimer::timeout, this, [this]() {
        if (CellWidget* w = widget_at(index_of(running_id_)))
            w->tick(kernel_->current_run_elapsed_ms());
    });
    autosave_timer_ = new QTimer(this);
    autosave_timer_->setInterval(kAutosaveMs);
    connect(autosave_timer_, &QTimer::timeout, this, &CodeEditorScreen::autosave_tick);
    autosave_timer_->start();
    dirty_timer_ = new QTimer(this);
    dirty_timer_->setSingleShot(true);
    dirty_timer_->setInterval(350);
    connect(dirty_timer_, &QTimer::timeout, this, [this]() {
        update_dirty();
        refresh_outline();
    });
    help_timer_ = new QTimer(this);
    help_timer_->setSingleShot(true);
    help_timer_->setInterval(700);
    connect(help_timer_, &QTimer::timeout, this, [this]() {
        CellWidget* w = widget_at(selected_);
        if (!w || !inspector_ || !inspector_visible_ || inspector_->current_tab() != NotebookInspector::Tab::Help)
            return;
        const QString sym = w->editor()->symbol_at_cursor();
        if (!sym.isEmpty() && sym != last_help_symbol_ && kernel_->state() == NotebookKernel::State::Idle)
            request_help(sym);
    });
    find_timer_ = new QTimer(this);
    find_timer_->setSingleShot(true);
    find_timer_->setInterval(200);
    connect(find_timer_, &QTimer::timeout, this, [this]() { run_find(true, false); });

    // Kernel.
    connect(kernel_, &NotebookKernel::state_changed, this, [this](NotebookKernel::State s) {
        refresh_kernel_badge();
        cmd_->set_busy(s == NotebookKernel::State::Busy || s == NotebookKernel::State::Interrupting ||
                       !queue_.isEmpty());
        if (s == NotebookKernel::State::Off || s == NotebookKernel::State::Unavailable) {
            variables_ = QJsonArray();
            variables_total_ = 0;
            if (variables_panel_)
                variables_panel_->set_kernel_alive(false, s == NotebookKernel::State::Unavailable
                                                              ? kernel_->status_reason()
                                                              : QString());
        }
    });
    connect(kernel_, &NotebookKernel::ready, this, [this](const QString&) { refresh_kernel_badge(); });
    connect(kernel_, &NotebookKernel::exec_stream, this, &CodeEditorScreen::on_exec_stream);
    connect(kernel_, &NotebookKernel::exec_event, this, &CodeEditorScreen::on_exec_event);
    connect(kernel_, &NotebookKernel::exec_finished, this, &CodeEditorScreen::on_exec_finished);
    connect(kernel_, &NotebookKernel::reply, this, &CodeEditorScreen::on_kernel_reply);
    connect(kernel_, &NotebookKernel::variables_changed, this, [this](const QJsonArray& items, int total) {
        variables_ = items;
        variables_total_ = total;
        if (variables_panel_) {
            if (kernel_->alive())
                variables_panel_->set_variables(items, total);
            else
                variables_panel_->set_kernel_alive(false);
        }
    });
    connect(kernel_, &NotebookKernel::stopped, this, [this](const QString& reason, bool unexpected) {
        exec_counter_ = 0;
        if (unexpected && !reason.isEmpty())
            status_->show_notice(tr("The kernel stopped: %1").arg(reason.section(QLatin1Char('\n'), 0, 0)), QString(),
                                 {}, 12000, NbsbTone::Danger);
        refresh_kernel_badge();
    });

    // Python setup / availability changes the badge.
    auto& setup = python::PythonSetupManager::instance();
    connect(&setup, &python::PythonSetupManager::running_changed, this, [this](bool) { refresh_kernel_badge(); });
    connect(&setup, &python::PythonSetupManager::setup_complete, this,
            [this](bool, const QString&) { refresh_kernel_badge(); });
    connect(&setup, &python::PythonSetupManager::core_ready, this, [this]() { refresh_kernel_badge(); });
    connect(&python::PythonRunner::instance(), &python::PythonRunner::python_ready, this,
            [this]() { refresh_kernel_badge(); });

    // Appearance: highlighter formats are refreshed after the theme handlers run
    // (no restyle inside them), sizes re-fitted once everything is styled.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this](const ui::ThemeTokens&) {
        if (theme_refresh_pending_)
            return;
        theme_refresh_pending_ = true;
        QTimer::singleShot(0, this, &CodeEditorScreen::on_theme_refresh);
    });
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this,
            &CodeEditorScreen::on_appearance_settled);

    // The rail opens on LIBRARY until restore_state() brings back the user's tab.
    cmd_->set_rail_checked(rail_visible_);
    show_rail_tab(rail_tab_);
    refresh_title();
    refresh_kernel_badge();
    refresh_status();
    refresh_start_view();
}

CodeEditorScreen::~CodeEditorScreen() {
    // Keep an unsaved notebook (recovery copy / autosave) — the widgets still exist here.
    flush_pending_state();
    // The children outlive this body: a focus change while they are deleted (an editor
    // gaining focus emits activated) must not reach the half-destroyed screen.
    for (QObject* child : findChildren<QObject*>())
        child->disconnect(this);
    if (kernel_)
        kernel_->disconnect(this);
}

void CodeEditorScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    cmd_ = new NotebookCommandBar(this);
    connect(cmd_, &NotebookCommandBar::command, this, &CodeEditorScreen::on_command);
    connect(cmd_, &NotebookCommandBar::environment_selected, this, &CodeEditorScreen::set_environment);
    connect(cmd_, &NotebookCommandBar::timeout_selected, this, [this](int s) {
        timeout_s_ = s;
        cmd_->set_timeout(s);
        persist();
    });
    connect(cmd_, &NotebookCommandBar::find_changed, this, [this](const QString& text) {
        find_text_ = text;
        if (text.isEmpty())
            clear_find();
        else
            find_timer_->start();
    });
    connect(cmd_, &NotebookCommandBar::find_next, this, [this](bool back) { run_find(false, back); });
    connect(cmd_, &NotebookCommandBar::find_closed, this, [this]() {
        clear_find();
        if (open_)
            select_cell(std::max(0, selected_), Mode::Command, false);
    });
    root->addWidget(cmd_);

    splitter_ = new QSplitter(Qt::Horizontal, this);
    splitter_->setHandleWidth(1);
    splitter_->setChildrenCollapsible(false);
    rail_ = build_rail();
    splitter_->addWidget(rail_);

    center_ = new QStackedWidget(splitter_);
    start_ = new NotebookStartView(center_);
    connect(start_, &NotebookStartView::new_requested, this, &CodeEditorScreen::new_notebook);
    connect(start_, &NotebookStartView::open_requested, this, &CodeEditorScreen::open_dialog);
    connect(start_, &NotebookStartView::open_template, this, &CodeEditorScreen::open_template);
    connect(start_, &NotebookStartView::open_path, this, [this](const QString& p) { open_notebook_path(p); });
    connect(start_, &NotebookStartView::recover_requested, this, &CodeEditorScreen::recover_unsaved);
    center_->addWidget(start_);

    scroll_ = new QScrollArea(center_);
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    canvas_ = new QWidget;
    canvas_->setObjectName(QStringLiteral("nbCanvas"));
    canvas_->setFocusPolicy(Qt::StrongFocus);
    canvas_->installEventFilter(this);
    canvas_->setProperty(core::agent::kAgentRiskKeysProperty, nbk_risk_keys());
    connect(&KeyConfigManager::instance(), &KeyConfigManager::key_changed, canvas_,
            [this]() { canvas_->setProperty(core::agent::kAgentRiskKeysProperty, nbk_risk_keys()); });
    canvas_->setAccessibleName(tr("Notebook cells — command mode: arrows select, Enter edits"));
    cells_layout_ = new QVBoxLayout(canvas_);
    cells_layout_->setContentsMargins(6, 8, 10, 40);
    cells_layout_->setSpacing(2);
    footer_ = new QWidget(canvas_);
    footer_->setObjectName(QStringLiteral("nbCellFooter"));
    auto* fh = new QHBoxLayout(footer_);
    fh->setContentsMargins(56, 8, 0, 0);
    fh->setSpacing(6);
    add_code_ = new QPushButton(footer_);
    add_code_->setObjectName(QStringLiteral("nbSecondary"));
    add_code_->setCursor(Qt::PointingHandCursor);
    connect(add_code_, &QPushButton::clicked, this,
            [this]() { insert_cell(static_cast<int>(doc_.cells.size()), NotebookCell::Type::Code, QString(), Mode::Edit); });
    fh->addWidget(add_code_);
    add_text_ = new QPushButton(footer_);
    add_text_->setObjectName(QStringLiteral("nbSecondary"));
    add_text_->setCursor(Qt::PointingHandCursor);
    connect(add_text_, &QPushButton::clicked, this, [this]() {
        insert_cell(static_cast<int>(doc_.cells.size()), NotebookCell::Type::Markdown, QString(), Mode::Edit);
    });
    fh->addWidget(add_text_);
    fh->addStretch(1);
    cells_layout_->addWidget(footer_);
    cells_layout_->addStretch(1);
    scroll_->setWidget(canvas_);
    center_->addWidget(scroll_);
    splitter_->addWidget(center_);
    splitter_->setStretchFactor(0, 0);
    splitter_->setStretchFactor(1, 1);
    connect(splitter_, &QSplitter::splitterMoved, this, [this](int, int) {
        const QList<int> sizes = splitter_->sizes();
        if (rail_->isVisible() && sizes.value(0) > 0)
            rail_width_ = sizes.value(0);
        if (inspector_ && inspector_->isVisible() && sizes.size() > 2 && sizes.value(2) > 0)
            inspector_width_ = sizes.value(2);
        persist();
    });
    root->addWidget(splitter_, 1);

    status_ = new ui::ScreenStatusBar(this);
    nbsb_add_items(status_);
    root->addWidget(status_);
    retranslate();
    apply_splitter_sizes();
}

QWidget* CodeEditorScreen::build_rail() {
    auto* rail = new QWidget(this);
    rail->setObjectName(QStringLiteral("nbRail"));
    rail->setAttribute(Qt::WA_StyledBackground, true);
    rail->setMinimumWidth(ui::metrics::width(200));
    auto* v = new QVBoxLayout(rail);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    auto* head = new QWidget(rail);
    head->setObjectName(QStringLiteral("nbPanelHead"));
    head->setAttribute(Qt::WA_StyledBackground, true);
    head->setMinimumHeight(30);
    auto* h = new QHBoxLayout(head);
    h->setContentsMargins(4, 0, 4, 0);
    h->setSpacing(0);
    for (int i = 0; i < 3; ++i) {
        rail_tabs_[i] = new QPushButton(head);
        rail_tabs_[i]->setObjectName(QStringLiteral("nbTab"));
        rail_tabs_[i]->setCursor(Qt::PointingHandCursor);
        connect(rail_tabs_[i], &QPushButton::clicked, this, [this, i]() {
            show_rail_tab(i);
            persist();
        });
        h->addWidget(rail_tabs_[i]);
    }
    h->addStretch(1);
    v->addWidget(head);
    rail_stack_ = new QStackedWidget(rail);
    for (int i = 0; i < 3; ++i) {
        rail_pages_[i] = new QWidget(rail_stack_); // replaced by the real page on first show
        rail_stack_->addWidget(rail_pages_[i]);
    }
    v->addWidget(rail_stack_, 1);
    return rail;
}

void CodeEditorScreen::show_rail_tab(int tab) {
    tab = std::clamp(tab, 0, 2);
    rail_tab_ = tab;
    // Pages are built the first time they are shown.
    if (tab == 0 && !library_) {
        library_ = new NotebookLibraryPanel(rail_stack_);
        connect(library_, &NotebookLibraryPanel::open_template, this, &CodeEditorScreen::open_template);
        connect(library_, &NotebookLibraryPanel::open_path, this, [this](const QString& p) { open_notebook_path(p); });
        rail_stack_->insertWidget(0, library_);
        rail_stack_->removeWidget(rail_pages_[0]);
        rail_pages_[0]->deleteLater();
        rail_pages_[0] = library_;
        library_->refresh(recent_);
    } else if (tab == 1 && !outline_) {
        outline_ = new NotebookOutlinePanel(rail_stack_);
        connect(outline_, &NotebookOutlinePanel::cell_activated, this, [this](const QString& id) {
            const int idx = index_of(id);
            if (idx >= 0)
                select_cell(idx, Mode::Command);
        });
        connect(outline_, &NotebookOutlinePanel::rename_requested, this,
                [this](const QString& id) { rename_cell(index_of(id)); });
        rail_stack_->insertWidget(1, outline_);
        rail_stack_->removeWidget(rail_pages_[1]);
        rail_pages_[1]->deleteLater();
        rail_pages_[1] = outline_;
    } else if (tab == 2 && !variables_panel_) {
        variables_panel_ = new NotebookVariablesPanel(rail_stack_);
        connect(variables_panel_, &NotebookVariablesPanel::preview_requested, this, [this](const QString& name) {
            ensure_inspector();
            set_inspector_visible(true);
            if (kernel_->alive() && kernel_->current_run().isEmpty()) {
                preview_request_ = QStringLiteral("p%1").arg(++req_seq_);
                kernel_->preview(preview_request_, name);
            } else {
                status_->show_notice(tr("The kernel is busy; show %1 again when the cell finishes.").arg(name));
            }
        });
        rail_stack_->addWidget(variables_panel_);
        rail_stack_->removeWidget(rail_pages_[2]);
        rail_pages_[2]->deleteLater();
        rail_pages_[2] = variables_panel_;
        if (kernel_->alive())
            variables_panel_->set_variables(variables_, variables_total_);
        else
            variables_panel_->set_kernel_alive(false);
    }
    rail_stack_->setCurrentWidget(rail_pages_[tab]);
    for (int i = 0; i < 3; ++i)
        nb_set_prop(rail_tabs_[i], "active", i == tab ? QStringLiteral("true") : QStringLiteral("false"));
    if (tab == 1)
        refresh_outline();
}

void CodeEditorScreen::ensure_inspector() {
    if (inspector_)
        return;
    inspector_ = new NotebookInspector(splitter_);
    inspector_->setMinimumWidth(ui::metrics::width(260));
    connect(inspector_, &NotebookInspector::close_requested, this, [this]() { set_inspector_visible(false); });
    connect(inspector_, &NotebookInspector::run_activated, this, [this](const QString& id) {
        const int idx = index_of(id);
        if (idx >= 0)
            select_cell(idx, Mode::Command);
    });
    for (auto it = runs_.crbegin(); it != runs_.crend(); ++it)
        inspector_->add_run(*it);
    splitter_->addWidget(inspector_);
    splitter_->setStretchFactor(2, 0);
    inspector_->setVisible(false);
    inspector_->show_tab(static_cast<NotebookInspector::Tab>(inspector_tab_));
}

void CodeEditorScreen::set_rail_visible(bool on) {
    rail_visible_ = on;
    rail_->setVisible(on);
    cmd_->set_rail_checked(on);
    if (on)
        show_rail_tab(rail_tab_);
    apply_splitter_sizes();
    persist();
}

void CodeEditorScreen::set_inspector_visible(bool on) {
    if (on)
        ensure_inspector();
    inspector_visible_ = on;
    if (inspector_)
        inspector_->setVisible(on);
    cmd_->set_inspector_checked(on);
    apply_splitter_sizes();
    persist();
}

void CodeEditorScreen::apply_splitter_sizes() {
    if (!splitter_)
        return;
    const int total = std::max(splitter_->width(), 800);
    const int rail = rail_visible_ ? (rail_width_ > 0 ? rail_width_ : ui::metrics::width(250)) : 0;
    const int insp = inspector_ && inspector_visible_ ? (inspector_width_ > 0 ? inspector_width_ : ui::metrics::width(360))
                                                      : 0;
    QList<int> sizes{rail, std::max(300, total - rail - insp)};
    if (inspector_)
        sizes << insp;
    splitter_->setSizes(sizes);
}

void CodeEditorScreen::bind_shortcuts() {
    auto bind = [this](const QKeySequence& seq, std::function<void()> fn) {
        auto* sc = new QShortcut(seq, this);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, this, std::move(fn));
        return sc;
    };
    bind(QKeySequence(QKeySequence::Save), [this]() { save(); });
    bind(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S), [this]() { save_as(); });
    bind(QKeySequence(QKeySequence::Open), [this]() { open_dialog(); });
    bind(QKeySequence(QKeySequence::New), [this]() { new_notebook(); });
    // Not F5 — KeyConfigManager binds it app-wide to Refresh. Run all = runs code.
    bind(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Return), [this]() { run_all(); })
        ->setProperty(core::agent::kAgentRiskProperty, QStringLiteral("code"));
    bind(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Enter), [this]() { run_all(); })
        ->setProperty(core::agent::kAgentRiskProperty, QStringLiteral("code"));
    bind(QKeySequence(Qt::CTRL | Qt::Key_F), [this]() {
        if (!open_)
            return;
        cmd_->find_box()->setFocus(Qt::ShortcutFocusReason);
        cmd_->find_box()->selectAll();
    });
    bind(QKeySequence(Qt::CTRL | Qt::Key_B), [this]() { set_rail_visible(!rail_visible_); });
    bind(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I), [this]() { set_inspector_visible(!inspector_visible_); });
    bind(QKeySequence(Qt::ALT | Qt::Key_Up), [this]() { move_cell(selected_, -1); });
    bind(QKeySequence(Qt::ALT | Qt::Key_Down), [this]() { move_cell(selected_, 1); });
}

// ═════════════════════════════════════════════════════════════════════════════
// Notebook lifecycle

bool CodeEditorScreen::open_notebook_path(const QString& path) {
    if (open_ && path == path_)
        return true;
    if (!confirm_discard(tr("Opening another notebook")))
        return false;
    QString why;
    if (!load_path(path, &why)) {
        QMessageBox::warning(this, tr("Open Notebook"),
                             tr("Could not open a notebook from:\n%1\n\n%2").arg(QDir::toNativeSeparators(path), why));
        return false;
    }
    return true;
}

bool CodeEditorScreen::load_path(const QString& path, QString* error) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = f.errorString();
        LOG_WARN(kTag, logf("Notebook could not be opened", {{"path", log_path(path)}, {"error", f.errorString()}}));
        return false;
    }
    NotebookDoc doc;
    QString why;
    if (!notebook::parse_ipynb(f.readAll(), &doc, &why)) {
        if (error)
            *error = tr("The file is not a notebook (.ipynb, nbformat 3 or 4): %1").arg(why);
        return false;
    }
    set_document(doc, QFileInfo(path).absoluteFilePath());
    add_recent(path_);
    LOG_INFO(kTag, logf("Notebook opened", {{"path", log_path(path)}}));
    return true;
}

void CodeEditorScreen::set_document(const NotebookDoc& doc, const QString& path) {
    abort_queue(QString());
    kernel_->shutdown(); // each notebook gets its own fresh session
    clear_find();
    doc_ = doc;
    if (doc_.cells.isEmpty()) {
        NotebookCell c;
        c.id = notebook::new_cell_id();
        doc_.cells.append(c);
    }
    path_ = path;
    open_ = true;
    exec_counter_ = 0;
    deleted_.clear();
    outputs_dirty_ = false;
    saved_at_ = path.isEmpty() ? QDateTime() : QFileInfo(path).lastModified();
    autosaved_at_ = QDateTime();
    save_error_.clear();
    clean_fp_ = notebook::fingerprint(doc_);
    dirty_ = false;
    kernel_->set_environment(doc_.environment());
    kernel_->set_working_dir(working_dir());
    cmd_->set_environment(doc_.environment());
    selected_ = -1;
    build_cell_widgets();
    center_->setCurrentWidget(scroll_);
    scroll_->verticalScrollBar()->setValue(0);
    select_cell(0, Mode::Command, false);
    refresh_title();
    refresh_kernel_badge();
    refresh_status();
    refresh_outline();
    persist();
}

void CodeEditorScreen::new_notebook() {
    if (!confirm_discard(tr("Creating a new notebook")))
        return;
    NotebookDoc doc;
    doc.set_environment(QStringLiteral("numpy2"));
    NotebookCell title;
    title.id = notebook::new_cell_id();
    title.type = NotebookCell::Type::Markdown;
    title.source = tr("# Untitled notebook\n\nWhat this notebook is for, and where its data comes from.");
    NotebookCell code;
    code.id = notebook::new_cell_id();
    code.source = QStringLiteral("import pandas as pd\nimport numpy as np\nimport matplotlib.pyplot as plt\n");
    doc.cells = {title, code};
    QFile::remove(recovery_path());
    set_document(doc, QString());
    select_cell(1, Mode::Edit);
    if (CellWidget* w = widget_at(1))
        w->focus_editor(true);
}

void CodeEditorScreen::close_notebook() {
    if (!open_ || !confirm_discard(tr("Closing the notebook")))
        return;
    abort_queue(QString());
    kernel_->shutdown();
    clear_cell_widgets();
    doc_ = NotebookDoc();
    path_.clear();
    open_ = false;
    dirty_ = false;
    outputs_dirty_ = false;
    selected_ = -1;
    QFile::remove(recovery_path());
    center_->setCurrentWidget(start_);
    refresh_start_view();
    refresh_title();
    refresh_kernel_badge();
    refresh_status();
    refresh_outline();
    persist();
}

void CodeEditorScreen::open_dialog() {
    if (!confirm_discard(tr("Opening another notebook")))
        return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Open Notebook"), default_save_dir(),
                                                      tr("Notebooks (*.ipynb);;All files (*)"));
    if (path.isEmpty())
        return;
    QString why;
    if (!load_path(path, &why))
        QMessageBox::warning(this, tr("Open Notebook"),
                             tr("Could not open a notebook from:\n%1\n\n%2").arg(QDir::toNativeSeparators(path), why));
}

void CodeEditorScreen::open_template(int catalog_index) {
    auto& lib = NotebookLibraryService::instance();
    const auto& cat = lib.catalog();
    if (catalog_index < 0 || catalog_index >= cat.size())
        return;
    const services::NotebookCatalogEntry& e = cat[catalog_index];
    if (!confirm_discard(tr("Opening this template")))
        return;
    const QString path = lib.working_copy_for(e);
    QString why;
    if (path.isEmpty() || !load_path(path, &why)) {
        QMessageBox::warning(this, tr("Open Template"),
                             tr("Could not open \"%1\".\n\n%2").arg(e.title, why.isEmpty() ? tr("The bundled notebook is "
                                                                                                "missing from this build.")
                                                                                          : why));
        return;
    }
    status_->show_notice(tr("Opened your copy of \"%1\". Nothing has run — press Ctrl+Shift+Enter to run it all.")
                             .arg(e.title));
}

void CodeEditorScreen::recover_unsaved() {
    if (!confirm_discard(tr("Restoring the unsaved notebook")))
        return;
    QFile f(recovery_path());
    NotebookDoc doc;
    if (!f.open(QIODevice::ReadOnly) || !notebook::parse_ipynb(f.readAll(), &doc)) {
        status_->show_notice(tr("The unsaved notebook could not be read."), QString(), {}, 7000, NbsbTone::Danger);
        return;
    }
    const QDateTime when = QFileInfo(f).lastModified();
    f.close();
    set_document(doc, QString());
    clean_fp_.clear(); // still unsaved
    update_dirty();
    status_->show_notice(tr("Restored the notebook you had not saved (%1). Save it to keep it.")
                             .arg(QLocale().toString(when, QLocale::ShortFormat)));
}

QString CodeEditorScreen::working_dir() const {
    if (!path_.isEmpty())
        return QFileInfo(path_).absolutePath();
    return services::FileManagerService::instance().storage_dir();
}

QString CodeEditorScreen::default_save_dir() const {
    const QString storage = QDir::cleanPath(services::FileManagerService::instance().storage_dir());
    if (!path_.isEmpty()) {
        const QString dir = QDir::cleanPath(QFileInfo(path_).absolutePath());
        if (dir.compare(storage, Qt::CaseInsensitive) != 0)
            return dir;
    }
    const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return docs.isEmpty() ? QDir::homePath() : docs;
}

QString CodeEditorScreen::suggested_base_name() const {
    QString name;
    if (!path_.isEmpty())
        name = strip_ipynb(NotebookLibraryService::instance().display_name_for(path_));
    if (name.isEmpty()) {
        for (const auto& e : notebook::outline(doc_))
            if (e.level >= 1) {
                name = e.text;
                break;
            }
    }
    name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9 _.-]+")), QStringLiteral(" "));
    name = name.simplified().replace(QLatin1Char(' '), QLatin1Char('_')).left(60);
    return name.isEmpty() ? QStringLiteral("notebook") : name;
}

bool CodeEditorScreen::confirm_discard(const QString& action) {
    if (!open_)
        return true;
    update_dirty();
    if (!dirty_)
        return true;
    const QString name = path_.isEmpty() ? tr("This notebook") : strip_ipynb(NotebookLibraryService::instance().display_name_for(path_));
    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(tr("Unsaved Changes"));
    box.setText(tr("%1 has unsaved changes.").arg(name));
    box.setInformativeText(tr("%1 will discard them.").arg(action));
    box.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    box.setDefaultButton(QMessageBox::Save);
    const int choice = box.exec();
    if (choice == QMessageBox::Cancel)
        return false;
    if (choice == QMessageBox::Save)
        return save(); // a cancelled Save dialog keeps the edits
    if (path_.isEmpty())
        QFile::remove(recovery_path());
    return true;
}

bool CodeEditorScreen::save() {
    if (!open_)
        return false;
    if (path_.isEmpty())
        return save_as();
    return write_to(path_, false);
}

bool CodeEditorScreen::save_as() {
    if (!open_)
        return false;
    QString path = QFileDialog::getSaveFileName(this, tr("Save Notebook As"),
                                                QDir(default_save_dir()).filePath(suggested_base_name() + ".ipynb"),
                                                tr("Notebooks (*.ipynb)"));
    if (path.isEmpty())
        return false;
    if (!path.endsWith(QLatin1String(".ipynb"), Qt::CaseInsensitive))
        path += QStringLiteral(".ipynb");
    const bool was_untitled = path_.isEmpty();
    if (!write_to(path, false))
        return false;
    path_ = QFileInfo(path).absoluteFilePath();
    kernel_->set_working_dir(working_dir()); // takes effect at the next kernel start
    if (was_untitled)
        QFile::remove(recovery_path());
    add_recent(path_);
    refresh_title();
    persist();
    return true;
}

bool CodeEditorScreen::write_to(const QString& path, bool quiet) {
    sync_sources();
    const QByteArray bytes = notebook::serialize_ipynb(doc_);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        save_error_ = file.errorString();
        LOG_ERROR(kTag, logf("Notebook could not be saved", {{"path", log_path(path)}, {"autosave", quiet}, {"error", save_error_}}));
        if (!quiet)
            QMessageBox::warning(this, tr("Save Notebook"),
                                 tr("Could not write to:\n%1\n\n%2").arg(QDir::toNativeSeparators(path), save_error_));
        refresh_save_state();
        return false;
    }
    save_error_.clear();
    clean_fp_ = notebook::fingerprint(doc_);
    dirty_ = false;
    outputs_dirty_ = false;
    if (quiet)
        autosaved_at_ = QDateTime::currentDateTime();
    else
        saved_at_ = QDateTime::currentDateTime();
    // A notebook in the terminal's file storage: refresh its File Manager record.
    // (Files elsewhere are not copied in — every save used to add a duplicate.)
    const QString storage = QDir::cleanPath(services::FileManagerService::instance().storage_dir());
    if (!quiet && QDir::cleanPath(QFileInfo(path).absolutePath()).compare(storage, Qt::CaseInsensitive) == 0)
        services::FileManagerService::instance().import_file(path, "code_editor");
    if (quiet)
        LOG_DEBUG(kTag, logf("Notebook autosaved", {{"path", log_path(path)}}));
    else
        LOG_INFO(kTag, logf("Notebook saved", {{"path", log_path(path)}}));
    refresh_title();
    refresh_save_state();
    return true;
}

void CodeEditorScreen::export_python() {
    if (!open_)
        return;
    sync_sources();
    const QString path = QFileDialog::getSaveFileName(this, tr("Export as Python Script"),
                                                      QDir(default_save_dir()).filePath(suggested_base_name() + ".py"),
                                                      tr("Python scripts (*.py)"));
    if (path.isEmpty())
        return;
    QSaveFile f(path);
    const QByteArray bytes = notebook::export_python(doc_, suggested_base_name()).toUtf8();
    if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size() || !f.commit()) {
        QMessageBox::warning(this, tr("Export"), tr("Could not write to:\n%1\n\n%2").arg(path, f.errorString()));
        return;
    }
    status_->show_notice(tr("Exported %1").arg(QDir::toNativeSeparators(path)));
}

void CodeEditorScreen::export_html() {
    if (!open_)
        return;
    sync_sources();
    const QString path = QFileDialog::getSaveFileName(this, tr("Export as HTML"),
                                                      QDir(default_save_dir()).filePath(suggested_base_name() + ".html"),
                                                      tr("HTML pages (*.html)"));
    if (path.isEmpty())
        return;
    QSaveFile f(path);
    const QByteArray bytes = notebook::export_html(doc_, suggested_base_name(), nb_html_palette()).toUtf8();
    if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size() || !f.commit()) {
        QMessageBox::warning(this, tr("Export"), tr("Could not write to:\n%1\n\n%2").arg(path, f.errorString()));
        return;
    }
    status_->show_notice(tr("Exported %1 with its saved outputs").arg(QDir::toNativeSeparators(path)));
}

QString CodeEditorScreen::recovery_path() const {
    return AppPaths::data() + QStringLiteral("/notebooks/recovery/untitled.ipynb");
}

bool CodeEditorScreen::write_recovery() {
    if (!open_ || !path_.isEmpty())
        return false;
    sync_sources();
    QDir().mkpath(QFileInfo(recovery_path()).absolutePath());
    QSaveFile f(recovery_path());
    const QByteArray bytes = notebook::serialize_ipynb(doc_);
    if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size() || !f.commit())
        return false;
    autosaved_at_ = QDateTime::currentDateTime();
    refresh_save_state();
    return true;
}

void CodeEditorScreen::autosave_tick() {
    if (!open_ || !running_id_.isEmpty())
        return; // never write a half-produced output set
    update_dirty();
    if (!dirty_ && !outputs_dirty_)
        return;
    if (path_.isEmpty()) {
        if (dirty_)
            write_recovery();
    } else if (autosave_) {
        write_to(path_, true);
    }
}

void CodeEditorScreen::flush_pending_state() {
    if (!open_ || !cmd_)
        return;
    update_dirty();
    if (!dirty_ && !outputs_dirty_)
        return;
    if (path_.isEmpty()) {
        if (dirty_)
            write_recovery();
    } else if (autosave_ && running_id_.isEmpty()) {
        write_to(path_, true);
    }
}

void CodeEditorScreen::add_recent(const QString& path) {
    if (path.isEmpty())
        return;
    const QString abs = QFileInfo(path).absoluteFilePath();
    recent_.erase(std::remove_if(recent_.begin(), recent_.end(),
                                 [&abs](const RecentNotebook& r) { return r.path.compare(abs, Qt::CaseInsensitive) == 0; }),
                  recent_.end());
    recent_.prepend({abs, QDateTime::currentDateTime()});
    while (recent_.size() > kMaxRecent)
        recent_.removeLast();
    if (library_)
        library_->refresh(recent_);
    persist();
}

// ═════════════════════════════════════════════════════════════════════════════
// Cell widgets

CellWidget* CodeEditorScreen::create_cell_widget(const NotebookCell& cell) {
    auto* w = new CellWidget(cell, canvas_);
    w->set_line_numbers_visible(line_numbers_);
    connect(w, &CellWidget::activated, this, &CodeEditorScreen::on_cell_activated);
    connect(w, &CellWidget::action, this, &CodeEditorScreen::on_cell_action);
    connect(w, &CellWidget::run_key, this, &CodeEditorScreen::on_run_key);
    connect(w, &CellWidget::escape_pressed, this, [this](const QString& id) {
        const int idx = index_of(id);
        if (idx >= 0)
            select_cell(idx, Mode::Command, false);
    });
    connect(w, &CellWidget::leave_edge, this, [this](const QString& id, int delta) {
        const int idx = index_of(id) + delta;
        if (idx < 0 || idx >= doc_.cells.size())
            return;
        select_cell(idx, Mode::Edit);
        if (CellWidget* n = widget_at(idx)) {
            QTextCursor c = n->editor()->textCursor();
            c.movePosition(delta < 0 ? QTextCursor::End : QTextCursor::Start);
            n->editor()->setTextCursor(c);
        }
    });
    connect(w, &CellWidget::edited, this, [this](const QString&) { dirty_timer_->start(); });
    connect(w, &CellWidget::completion_requested, this, &CodeEditorScreen::on_completion_requested);
    connect(w, &CellWidget::inspect_requested, this, [this](const QString&, const QString& sym) {
        set_inspector_visible(true);
        request_help(sym);
    });
    connect(w, &CellWidget::cursor_moved, this, [this, w](const QString&, int line, int col) {
        nbsb_cursor(status_, line, col);
        // Long cells do not scroll inside: keep the cursor in the notebook's view.
        const QRect r = w->editor()->cursorRect();
        const QPoint p = w->editor()->viewport()->mapTo(canvas_, r.center());
        scroll_->ensureVisible(p.x(), p.y(), 20, r.height() + 12);
        if (inspector_visible_)
            help_timer_->start();
    });
    connect(w, &CellWidget::output_expand_requested, this, [this](const QString& id, int index) {
        const int idx = index_of(id);
        if (idx < 0)
            return;
        const QVector<CellOutput>& outs = id == running_id_ ? running_outputs_ : doc_.cells[idx].outputs;
        if (index < 0 || index >= outs.size())
            return;
        set_inspector_visible(true);
        const int n = doc_.cells[idx].execution_count;
        inspector_->show_output(outs[index], tr("Cell %1%2  ·  output %3")
                                                 .arg(idx + 1)
                                                 .arg(n > 0 ? QStringLiteral(" [%1]").arg(n) : QString())
                                                 .arg(index + 1));
    });
    connect(w, &CellWidget::menu_requested, this, &CodeEditorScreen::show_cell_menu);
    return w;
}

void CodeEditorScreen::clear_cell_widgets() {
    ++build_gen_; // stops a batched build in flight
    for (CellWidget* w : cells_) {
        cells_layout_->removeWidget(w);
        w->deleteLater();
    }
    cells_.clear();
}

void CodeEditorScreen::build_cell_widgets() {
    clear_cell_widgets();
    // The top of the notebook first; the rest a few cells per event-loop turn so a
    // long notebook never freezes the window while it opens.
    const int first = std::min(static_cast<int>(doc_.cells.size()), kFirstBatch);
    for (int i = 0; i < first; ++i) {
        CellWidget* w = create_cell_widget(doc_.cells[i]);
        cells_.append(w);
        cells_layout_->insertWidget(i, w);
    }
    if (cells_.size() < doc_.cells.size()) {
        const quint64 gen = build_gen_;
        QTimer::singleShot(0, this, [this, gen]() { build_more_cells(gen); });
    }
}

void CodeEditorScreen::build_more_cells(quint64 generation) {
    if (generation != build_gen_)
        return;
    const int end = std::min(static_cast<int>(doc_.cells.size()), static_cast<int>(cells_.size()) + kBatch);
    for (int i = static_cast<int>(cells_.size()); i < end; ++i) {
        CellWidget* w = create_cell_widget(doc_.cells[i]);
        cells_.append(w);
        cells_layout_->insertWidget(i, w);
    }
    if (cells_.size() < doc_.cells.size())
        QTimer::singleShot(0, this, [this, generation]() { build_more_cells(generation); });
    else
        refresh_outline();
}

void CodeEditorScreen::ensure_all_built() {
    while (cells_.size() < doc_.cells.size()) {
        const int i = static_cast<int>(cells_.size());
        CellWidget* w = create_cell_widget(doc_.cells[i]);
        cells_.append(w);
        cells_layout_->insertWidget(i, w);
    }
}

int CodeEditorScreen::index_of(const QString& id) const {
    return id.isEmpty() ? -1 : doc_.index_of(id);
}

CellWidget* CodeEditorScreen::widget_at(int index) const {
    return index >= 0 && index < cells_.size() ? cells_[index] : nullptr;
}

void CodeEditorScreen::sync_sources() {
    for (int i = 0; i < cells_.size() && i < doc_.cells.size(); ++i)
        doc_.cells[i].source = cells_[i]->source();
}

void CodeEditorScreen::mark_changed() {
    update_dirty();
    refresh_outline();
    refresh_status();
}

void CodeEditorScreen::update_dirty() {
    if (!open_) {
        dirty_ = false;
        return;
    }
    sync_sources();
    const bool d = notebook::fingerprint(doc_) != clean_fp_;
    if (d != dirty_) {
        dirty_ = d;
        refresh_title();
    }
    refresh_save_state();
}

void CodeEditorScreen::select_cell(int index, Mode mode, bool scroll, bool take_focus) {
    if (!open_ || index < 0 || index >= doc_.cells.size())
        return;
    if (index >= cells_.size())
        ensure_all_built();
    CellWidget* w = widget_at(index);
    if (!w)
        return;
    if (selected_ != index) {
        if (CellWidget* prev = widget_at(selected_)) {
            prev->set_selection(CellWidget::Selection::None);
            if (prev->type() != NotebookCell::Type::Code && prev->markdown_editing() &&
                !prev->source().trimmed().isEmpty())
                prev->set_markdown_editing(false);
        }
    }
    selected_ = index;
    mode_ = mode;
    w->set_selection(mode == Mode::Edit ? CellWidget::Selection::Edit : CellWidget::Selection::Command);
    if (mode == Mode::Edit) {
        if (!w->editor()->hasFocus())
            w->focus_editor();
    } else {
        if (w->type() != NotebookCell::Type::Code && w->markdown_editing() && !w->source().trimmed().isEmpty())
            w->set_markdown_editing(false);
        if (take_focus)
            canvas_->setFocus(Qt::OtherFocusReason);
        nbsb_cursor(status_, 0, 0);
    }
    if (scroll)
        scroll_to(w);
    if (outline_)
        outline_->set_selected(doc_.cells[index].id);
    refresh_status();
}

void CodeEditorScreen::scroll_to(CellWidget* w) {
    QPointer<CellWidget> guard = w;
    // After the layout has placed a just-inserted cell.
    QTimer::singleShot(0, this, [this, guard]() {
        if (!guard)
            return;
        QScrollBar* vs = scroll_->verticalScrollBar();
        const int top = guard->y();
        const int view = scroll_->viewport()->height();
        const int visible = std::min(guard->height(), std::max(60, view - 40));
        if (top < vs->value())
            vs->setValue(std::max(0, top - 6));
        else if (top + visible > vs->value() + view)
            vs->setValue(top + visible - view + 6);
    });
}

void CodeEditorScreen::on_cell_activated(const QString& id, bool edit) {
    const int idx = index_of(id);
    if (idx < 0)
        return;
    CellWidget* w = widget_at(idx);
    if (edit) {
        if (w && w->type() != NotebookCell::Type::Code && !w->markdown_editing())
            w->set_markdown_editing(true);
        select_cell(idx, Mode::Edit, false);
    } else if (idx != selected_ || mode_ != Mode::Command) {
        select_cell(idx, Mode::Command, false);
    }
}

void CodeEditorScreen::insert_cell(int index, NotebookCell::Type type, const QString& source, Mode mode) {
    if (!open_)
        return;
    ensure_all_built();
    sync_sources();
    index = std::clamp(index, 0, static_cast<int>(doc_.cells.size()));
    NotebookCell c;
    c.id = notebook::new_cell_id();
    c.type = type;
    c.source = source;
    doc_.cells.insert(index, c);
    CellWidget* w = create_cell_widget(c);
    cells_.insert(index, w);
    cells_layout_->insertWidget(index, w);
    if (type != NotebookCell::Type::Code)
        w->set_markdown_editing(source.trimmed().isEmpty() || mode == Mode::Edit);
    select_cell(index, mode);
    mark_changed();
}

void CodeEditorScreen::delete_cell(int index) {
    if (!open_ || index < 0 || index >= doc_.cells.size())
        return;
    ensure_all_built();
    sync_sources();
    const QString id = doc_.cells[index].id;
    if (id == running_id_) {
        status_->show_notice(tr("This cell is running — stop it (I, I) before deleting it."), QString(), {}, 6000,
                             NbsbTone::Warn);
        return;
    }
    queue_.removeAll(id);
    deleted_.append({doc_.cells[index], index});
    if (deleted_.size() > kMaxUndo)
        deleted_.removeFirst();
    CellWidget* w = cells_.takeAt(index);
    cells_layout_->removeWidget(w);
    w->deleteLater();
    doc_.cells.removeAt(index);
    selected_ = -1;
    if (doc_.cells.isEmpty()) {
        insert_cell(0, NotebookCell::Type::Code, QString(), Mode::Command);
    } else {
        select_cell(std::min(index, static_cast<int>(doc_.cells.size()) - 1), Mode::Command);
        mark_changed();
    }
    status_->show_notice(tr("Cell deleted."), tr("UNDO"), [this]() { undo_delete(); });
}

void CodeEditorScreen::undo_delete() {
    if (!open_ || deleted_.isEmpty())
        return;
    ensure_all_built();
    sync_sources();
    const Deleted d = deleted_.takeLast();
    NotebookCell cell = d.cell;
    if (doc_.index_of(cell.id) >= 0)
        cell.id = notebook::new_cell_id();
    const int index = std::clamp(d.index, 0, static_cast<int>(doc_.cells.size()));
    doc_.cells.insert(index, cell);
    CellWidget* w = create_cell_widget(cell);
    cells_.insert(index, w);
    cells_layout_->insertWidget(index, w);
    select_cell(index, Mode::Command);
    mark_changed();
}

void CodeEditorScreen::move_cell(int index, int delta) {
    if (!open_ || index < 0 || index >= doc_.cells.size())
        return;
    const int target = index + delta;
    if (target < 0 || target >= doc_.cells.size())
        return;
    ensure_all_built();
    sync_sources();
    doc_.cells.move(index, target);
    CellWidget* w = cells_.takeAt(index);
    cells_.insert(target, w);
    cells_layout_->removeWidget(w);
    cells_layout_->insertWidget(target, w);
    selected_ = target;
    select_cell(target, mode_, true, mode_ == Mode::Command);
    if (mode_ == Mode::Edit)
        w->editor()->setFocus(Qt::OtherFocusReason);
    mark_changed();
}

void CodeEditorScreen::set_cell_type(int index, NotebookCell::Type type) {
    CellWidget* w = widget_at(index);
    if (!w || doc_.cells[index].type == type)
        return;
    if (doc_.cells[index].id == running_id_)
        return;
    sync_sources();
    NotebookCell& c = doc_.cells[index];
    c.type = type;
    if (type != NotebookCell::Type::Code) {
        c.outputs.clear();
        c.execution_count = 0;
        w->set_outputs({});
        w->set_exec_count(0);
        w->set_run_state(CellWidget::RunState::Idle);
    }
    w->set_type(type);
    if (type != NotebookCell::Type::Code)
        w->set_markdown_editing(mode_ == Mode::Edit || c.source.trimmed().isEmpty());
    select_cell(index, mode_, false);
    mark_changed();
}

void CodeEditorScreen::copy_cell(int index, bool cut) {
    if (!open_ || index < 0 || index >= doc_.cells.size())
        return;
    sync_sources();
    clipboard_ = {doc_.cells[index]};
    if (cut)
        delete_cell(index);
    else
        status_->show_notice(tr("Cell copied — V pastes it below the selected cell."));
}

void CodeEditorScreen::paste_cells(int index) {
    if (!open_ || clipboard_.isEmpty())
        return;
    ensure_all_built();
    sync_sources();
    index = std::clamp(index, 0, static_cast<int>(doc_.cells.size()));
    for (NotebookCell c : clipboard_) {
        c.id = notebook::new_cell_id();
        doc_.cells.insert(index, c);
        CellWidget* w = create_cell_widget(c);
        cells_.insert(index, w);
        cells_layout_->insertWidget(index, w);
        ++index;
    }
    select_cell(index - 1, Mode::Command);
    mark_changed();
}

void CodeEditorScreen::rename_cell(int index) {
    CellWidget* w = widget_at(index);
    if (!w)
        return;
    sync_sources();
    NotebookCell& c = doc_.cells[index];
    QString current = notebook::cell_display_name(c);
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Rename Cell"),
                                               tr("Name shown in the outline (empty = its first line):"),
                                               QLineEdit::Normal, c.title.isEmpty() ? current : c.title, &ok)
                             .trimmed();
    if (!ok)
        return;
    c.title = name;
    w->set_title(name);
    mark_changed();
}

void CodeEditorScreen::clear_outputs(int index) {
    if (!open_)
        return;
    for (int i = 0; i < doc_.cells.size(); ++i) {
        if (index >= 0 && i != index)
            continue;
        NotebookCell& c = doc_.cells[i];
        if (c.id == running_id_ || (c.outputs.isEmpty() && c.execution_count == 0))
            continue;
        c.outputs.clear();
        c.execution_count = 0;
        if (CellWidget* w = widget_at(i)) {
            w->set_outputs({});
            w->set_exec_count(0);
            w->set_run_state(CellWidget::RunState::Idle);
        }
        outputs_dirty_ = true;
    }
    refresh_outline();
    refresh_save_state();
}

void CodeEditorScreen::toggle_output(int index) {
    CellWidget* w = widget_at(index);
    if (!w || !doc_.cells[index].is_code())
        return;
    doc_.cells[index].output_collapsed = !doc_.cells[index].output_collapsed;
    w->set_output_collapsed(doc_.cells[index].output_collapsed);
    mark_changed();
}

void CodeEditorScreen::toggle_input(int index) {
    CellWidget* w = widget_at(index);
    if (!w)
        return;
    doc_.cells[index].input_collapsed = !doc_.cells[index].input_collapsed;
    w->set_input_collapsed(doc_.cells[index].input_collapsed);
    mark_changed();
}

void CodeEditorScreen::on_cell_action(const QString& id, CellWidget::Action action) {
    const int idx = index_of(id);
    if (idx < 0)
        return;
    if (idx != selected_)
        select_cell(idx, Mode::Command, false);
    switch (action) {
        case CellWidget::Action::Run:
            run_selected(AfterRun::Stay);
            break;
        case CellWidget::Action::MoveUp:
            move_cell(idx, -1);
            break;
        case CellWidget::Action::MoveDown:
            move_cell(idx, 1);
            break;
        case CellWidget::Action::ToggleType:
            set_cell_type(idx, doc_.cells[idx].is_code() ? NotebookCell::Type::Markdown : NotebookCell::Type::Code);
            break;
        case CellWidget::Action::Delete:
            delete_cell(idx);
            break;
        case CellWidget::Action::ToggleInput:
            toggle_input(idx);
            break;
        case CellWidget::Action::ToggleOutput:
            toggle_output(idx);
            break;
        case CellWidget::Action::Menu:
            break;
    }
}

void CodeEditorScreen::show_cell_menu(const QString& id, const QPoint& global_pos) {
    const int idx = index_of(id);
    if (idx < 0)
        return;
    if (idx != selected_)
        select_cell(idx, Mode::Command, false);
    const bool code = doc_.cells[idx].is_code();
    QMenu menu(this);
    nb_set_sheet(&menu, nb_menu_sheet());
    auto add = [&](const QString& text, std::function<void()> fn, bool enabled = true) {
        QAction* a = menu.addAction(text);
        a->setEnabled(enabled);
        connect(a, &QAction::triggered, this, std::move(fn));
    };
    add(code ? tr("Run cell\tCtrl+Enter") : tr("Render text\tCtrl+Enter"), [this]() { run_selected(AfterRun::Stay); });
    if (code)
        menu.actions().constLast()->setProperty("fincept_agent_risk", "code");
    add(tr("Run all above"), [this]() { run_above(); }, idx > 0);
    menu.actions().constLast()->setProperty("fincept_agent_risk", "code");
    add(tr("Run this cell and all below"), [this]() { run_below(); });
    menu.actions().constLast()->setProperty("fincept_agent_risk", "code");
    menu.addSeparator();
    add(tr("Insert code cell above\tA"), [this, idx]() { insert_cell(idx, NotebookCell::Type::Code, QString(), Mode::Edit); });
    add(tr("Insert code cell below\tB"),
        [this, idx]() { insert_cell(idx + 1, NotebookCell::Type::Code, QString(), Mode::Edit); });
    add(tr("Insert text cell below"),
        [this, idx]() { insert_cell(idx + 1, NotebookCell::Type::Markdown, QString(), Mode::Edit); });
    menu.addSeparator();
    add(code ? tr("Make it a text (markdown) cell\tM") : tr("Make it a code cell\tY"), [this, idx, code]() {
        set_cell_type(idx, code ? NotebookCell::Type::Markdown : NotebookCell::Type::Code);
    });
    add(doc_.cells[idx].input_collapsed ? tr("Show the code") : tr("Hide the code"), [this, idx]() { toggle_input(idx); });
    if (code) {
        add(doc_.cells[idx].output_collapsed ? tr("Show the output\tO") : tr("Hide the output\tO"),
            [this, idx]() { toggle_output(idx); }, !doc_.cells[idx].outputs.isEmpty());
        add(tr("Clear the output"), [this, idx]() { clear_outputs(idx); }, !doc_.cells[idx].outputs.isEmpty());
    }
    menu.addSeparator();
    add(tr("Rename…\tF2"), [this, idx]() { rename_cell(idx); });
    add(tr("Copy cell\tC"), [this, idx]() { copy_cell(idx, false); });
    add(tr("Cut cell\tX"), [this, idx]() { copy_cell(idx, true); });
    add(tr("Paste below\tV"), [this, idx]() { paste_cells(idx + 1); }, !clipboard_.isEmpty());
    menu.addSeparator();
    add(tr("Move up\tAlt+Up"), [this, idx]() { move_cell(idx, -1); }, idx > 0);
    add(tr("Move down\tAlt+Down"), [this, idx]() { move_cell(idx, 1); }, idx + 1 < doc_.cells.size());
    add(tr("Delete cell\tD, D"), [this, idx]() { delete_cell(idx); });
    menu.exec(global_pos);
}

void CodeEditorScreen::on_run_key(const QString& id, CodeTextEdit::RunKey key) {
    const int idx = index_of(id);
    if (idx < 0)
        return;
    if (idx != selected_)
        select_cell(idx, mode_, false, false);
    run_selected(key == CodeTextEdit::RunKey::Run          ? AfterRun::Stay
                 : key == CodeTextEdit::RunKey::RunAdvance ? AfterRun::Advance
                                                           : AfterRun::Insert);
}

bool CodeEditorScreen::eventFilter(QObject* watched, QEvent* event) {
    if (watched == canvas_ && event->type() == QEvent::KeyPress && open_)
        return handle_command_key(static_cast<QKeyEvent*>(event));
    return QWidget::eventFilter(watched, event);
}

bool CodeEditorScreen::handle_command_key(QKeyEvent* e) {
    if (selected_ < 0 || selected_ >= doc_.cells.size())
        return false;
    auto& km = KeyConfigManager::instance();
    const QKeySequence pressed(e->keyCombination());
    const Qt::KeyboardModifiers mods = e->modifiers() & ~Qt::KeypadModifier;
    const int key = e->key();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    auto twice = [&](int k) {
        const bool hit = pending_key_ == k && now - pending_key_at_ < kDoubleKeyMs;
        pending_key_ = hit ? 0 : k;
        pending_key_at_ = now;
        return hit;
    };
    if (pressed == km.key(KeyAction::RunCell)) {
        run_selected(AfterRun::Stay);
        return true;
    }
    if (pressed == km.key(KeyAction::RunAndNext)) {
        run_selected(AfterRun::Advance);
        return true;
    }
    if (pressed == km.key(KeyAction::RenameCell)) {
        rename_cell(selected_);
        return true;
    }
    if (mods == Qt::AltModifier && (key == Qt::Key_Return || key == Qt::Key_Enter)) {
        run_selected(AfterRun::Insert);
        return true;
    }
    if (mods == Qt::ShiftModifier && key == Qt::Key_V) {
        paste_cells(selected_);
        return true;
    }
    if (mods != Qt::NoModifier)
        return false;
    const int last = static_cast<int>(doc_.cells.size()) - 1;
    switch (key) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (CellWidget* w = widget_at(selected_)) {
                if (w->type() != NotebookCell::Type::Code)
                    w->set_markdown_editing(true);
                select_cell(selected_, Mode::Edit);
            }
            return true;
        case Qt::Key_Up:
        case Qt::Key_K:
            select_cell(std::max(0, selected_ - 1), Mode::Command);
            return true;
        case Qt::Key_Down:
        case Qt::Key_J:
            select_cell(std::min(last, selected_ + 1), Mode::Command);
            return true;
        case Qt::Key_Home:
            select_cell(0, Mode::Command);
            return true;
        case Qt::Key_End:
            select_cell(last, Mode::Command);
            return true;
        case Qt::Key_A:
            insert_cell(selected_, NotebookCell::Type::Code, QString(), Mode::Command);
            return true;
        case Qt::Key_B:
            insert_cell(selected_ + 1, NotebookCell::Type::Code, QString(), Mode::Command);
            return true;
        case Qt::Key_D:
            if (twice(Qt::Key_D))
                delete_cell(selected_);
            return true;
        case Qt::Key_Z:
            undo_delete();
            return true;
        case Qt::Key_X:
            copy_cell(selected_, true);
            return true;
        case Qt::Key_C:
            copy_cell(selected_, false);
            return true;
        case Qt::Key_V:
            paste_cells(selected_ + 1);
            return true;
        case Qt::Key_M:
            set_cell_type(selected_, NotebookCell::Type::Markdown);
            return true;
        case Qt::Key_Y:
            set_cell_type(selected_, NotebookCell::Type::Code);
            return true;
        case Qt::Key_O:
            toggle_output(selected_);
            return true;
        case Qt::Key_L:
            set_line_numbers(!line_numbers_);
            return true;
        case Qt::Key_I:
            if (twice(Qt::Key_I))
                interrupt();
            return true;
        case Qt::Key_0:
            if (twice(Qt::Key_0))
                restart_kernel(true, false);
            return true;
        case Qt::Key_Escape:
            if (!running_id_.isEmpty() || !queue_.isEmpty())
                interrupt(); // the old STOP key still stops
            return true;
        default:
            return false;
    }
}

void CodeEditorScreen::set_line_numbers(bool on) {
    line_numbers_ = on;
    for (CellWidget* w : cells_)
        w->set_line_numbers_visible(on);
    cmd_->set_line_numbers_checked(on);
    persist();
}

// ── Snippets ─────────────────────────────────────────────────────────────────

QString CodeEditorScreen::ask_symbol() {
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Load Prices for a Symbol"));
    auto* v = new QVBoxLayout(&dlg);
    auto* label = new QLabel(tr("Find the instrument — stocks, ETFs, indices, FX, crypto, futures:"), &dlg);
    label->setWordWrap(true);
    v->addWidget(label);
    auto* box = new ui::SymbolSearchBox(&dlg);
    box->set_placeholder(tr("Symbol or name, e.g. AAPL, Reliance, EUR/USD, BTC"));
    box->set_clear_on_pick(false);
    v->addWidget(box);
    auto* note = new QLabel(&dlg);
    note->setWordWrap(true);
    note->setProperty("tone", QStringLiteral("warn"));
    v->addWidget(note);
    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto* cancel = new QPushButton(tr("Cancel"), &dlg);
    buttons->addWidget(cancel);
    v->addLayout(buttons);
    QString picked;
    QString name;
    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(box, &ui::SymbolSearchBox::instrument_picked, &dlg,
            [&](const core::instrument::Instrument& inst) {
                if (inst.symbol.isEmpty()) {
                    note->setText(tr("No market data symbol is known for this instrument."));
                    return;
                }
                picked = inst.symbol;
                name = inst.name;
                dlg.accept();
            });
    connect(box, &ui::SymbolSearchBox::unresolved, &dlg,
            [note](const QStringList&, const QString& message) { note->setText(message); });
    dlg.resize(ui::metrics::width(460), dlg.sizeHint().height());
    box->setFocus();
    if (dlg.exec() != QDialog::Accepted)
        return {};
    return name.isEmpty() ? picked : picked + QLatin1Char('\x1f') + name;
}

void CodeEditorScreen::insert_snippet(NotebookCommandBar::Cmd cmd) {
    if (!open_)
        return;
    QString code;
    switch (cmd) {
        case NotebookCommandBar::Cmd::InsertPrices: {
            const QString pick = ask_symbol();
            if (pick.isEmpty())
                return;
            const QString sym = pick.section(QLatin1Char('\x1f'), 0, 0);
            const QString name = pick.section(QLatin1Char('\x1f'), 1);
            QString sym_lit = sym;
            sym_lit.replace(QLatin1Char('\\'), QStringLiteral("\\\\")).replace(QLatin1Char('"'), QStringLiteral("\\\""));
            code = QStringLiteral("import yfinance as yf\n\n"
                                  "SYMBOL = \"%1\"%2\n"
                                  "prices = yf.download(SYMBOL, period=\"1y\", auto_adjust=True, progress=False)\n"
                                  "prices.columns = prices.columns.get_level_values(0)  # one ticker: flat columns\n"
                                  "if prices.empty:\n"
                                  "    raise ValueError(f\"No price data for {SYMBOL}\")\n"
                                  "prices.tail()")
                       .arg(sym_lit, name.isEmpty() ? QString() : QStringLiteral("  # ") + name.simplified());
            break;
        }
        case NotebookCommandBar::Cmd::InsertPlot:
            code = QStringLiteral("import matplotlib.pyplot as plt\n\n"
                                  "ax = prices[\"Close\"].plot(figsize=(9, 4), title=f\"{SYMBOL} close\")\n"
                                  "ax.set_ylabel(\"Price\")\n"
                                  "plt.show()");
            break;
        case NotebookCommandBar::Cmd::InsertReturns:
            code = QStringLiteral("import numpy as np\nimport pandas as pd\n\n"
                                  "returns = prices[\"Close\"].pct_change().dropna()\n"
                                  "summary = {\n"
                                  "    \"Annual return\": (1 + returns.mean()) ** 252 - 1,\n"
                                  "    \"Annual volatility\": returns.std() * np.sqrt(252),\n"
                                  "    \"Sharpe (rf = 0)\": returns.mean() / returns.std() * np.sqrt(252),\n"
                                  "    \"Max drawdown\": (prices[\"Close\"] / prices[\"Close\"].cummax() - 1).min(),\n"
                                  "}\n"
                                  "pd.Series(summary, name=SYMBOL).to_frame()");
            break;
        case NotebookCommandBar::Cmd::InsertConnector:
            code = QStringLiteral("import fincept_connector as fc\nimport pandas as pd\n\n"
                                  "# Any connector in the terminal's scripts folder: script, command, arguments.\n"
                                  "result = fc.run_enveloped([\"yfinance_data.py\", \"quote\", \"AAPL\"])\n"
                                  "if not result[\"ok\"]:\n"
                                  "    raise RuntimeError(f\"{result.get('code')}: {result['error']}\")\n"
                                  "pd.DataFrame(result[\"data\"], columns=result.get(\"columns\"))");
            break;
        case NotebookCommandBar::Cmd::InsertCsv:
            code = QStringLiteral("import pandas as pd\n\n"
                                  "# Relative paths start in the notebook's folder.\n"
                                  "df = pd.read_csv(\"data.csv\", parse_dates=True)\n"
                                  "df.head()");
            break;
        default:
            return;
    }
    insert_cell(selected_ + 1, NotebookCell::Type::Code, code, Mode::Command);
    status_->show_notice(tr("Snippet inserted — nothing ran. Ctrl+Enter runs it."));
}

// ═════════════════════════════════════════════════════════════════════════════
// Running

void CodeEditorScreen::run_cells(const QStringList& ids) {
    if (!open_)
        return;
    ensure_all_built();
    sync_sources();
    for (const QString& id : ids) {
        const int idx = index_of(id);
        if (idx < 0)
            continue;
        NotebookCell& c = doc_.cells[idx];
        CellWidget* w = widget_at(idx);
        if (!c.is_code()) {
            if (w && w->markdown_editing() && !c.source.trimmed().isEmpty())
                w->set_markdown_editing(false); // "running" a text cell renders it
            continue;
        }
        if (!c.runnable() || queue_.contains(id) || running_id_ == id)
            continue;
        queue_ << id;
        if (w)
            w->set_run_state(CellWidget::RunState::Queued);
    }
    cmd_->set_busy(!queue_.isEmpty() || !running_id_.isEmpty());
    start_next();
    refresh_outline();
}

void CodeEditorScreen::run_selected(AfterRun after) {
    if (!open_ || selected_ < 0)
        return;
    const int idx = selected_;
    const bool was_edit = mode_ == Mode::Edit;
    run_cells({doc_.cells[idx].id});
    const int last = static_cast<int>(doc_.cells.size()) - 1;
    switch (after) {
        case AfterRun::Stay:
            if (!doc_.cells[idx].is_code())
                select_cell(idx, Mode::Command, false);
            else if (!was_edit)
                select_cell(idx, Mode::Command, false);
            break;
        case AfterRun::Advance:
            if (idx < last)
                select_cell(idx + 1, Mode::Command);
            else
                insert_cell(idx + 1, NotebookCell::Type::Code, QString(), Mode::Edit);
            break;
        case AfterRun::Insert:
            insert_cell(idx + 1, NotebookCell::Type::Code, QString(), Mode::Edit);
            break;
    }
}

void CodeEditorScreen::run_all() {
    if (!open_)
        return;
    QStringList ids;
    for (const NotebookCell& c : doc_.cells)
        ids << c.id;
    run_cells(ids);
}

void CodeEditorScreen::run_above() {
    QStringList ids;
    for (int i = 0; i < selected_ && i < doc_.cells.size(); ++i)
        ids << doc_.cells[i].id;
    run_cells(ids);
}

void CodeEditorScreen::run_below() {
    QStringList ids;
    for (int i = std::max(0, selected_); i < doc_.cells.size(); ++i)
        ids << doc_.cells[i].id;
    run_cells(ids);
}

void CodeEditorScreen::start_next() {
    if (!running_id_.isEmpty())
        return;
    while (!queue_.isEmpty()) {
        const QString id = queue_.takeFirst();
        const int idx = index_of(id);
        CellWidget* w = widget_at(idx);
        if (idx < 0 || !w)
            continue;
        doc_.cells[idx].source = w->source();
        if (!doc_.cells[idx].runnable()) {
            w->set_run_state(CellWidget::RunState::Idle);
            continue;
        }
        const QString token = QStringLiteral("%1#%2").arg(id).arg(++run_seq_);
        kernel_->set_environment(doc_.environment());
        kernel_->set_working_dir(working_dir());
        QString why;
        if (!kernel_->execute(token, doc_.cells[idx].source, timeout_s_, &why)) {
            CellOutput err;
            err.kind = CellOutput::Kind::Error;
            err.ename = tr("Python is not ready");
            err.evalue = why;
            doc_.cells[idx].outputs = {err};
            w->set_outputs(doc_.cells[idx].outputs);
            w->set_run_state(CellWidget::RunState::Error, 0);
            abort_queue(QString());
            status_->show_notice(why, QString(), {}, 12000, NbsbTone::Danger);
            refresh_kernel_badge();
            cmd_->set_busy(false);
            return;
        }
        running_id_ = id;
        running_token_ = token;
        running_outputs_.clear();
        w->set_outputs({});
        w->set_run_state(CellWidget::RunState::Running, 0);
        tick_timer_->start();
        cmd_->set_busy(true);
        refresh_status();
        return;
    }
    tick_timer_->stop();
    cmd_->set_busy(false);
}

void CodeEditorScreen::on_exec_stream(const QString& token, const QString& text, bool is_stderr) {
    if (token != running_token_)
        return;
    const QJsonObject ev{{QStringLiteral("type"), QStringLiteral("stream")},
                         {QStringLiteral("name"), is_stderr ? QStringLiteral("stderr") : QStringLiteral("stdout")},
                         {QStringLiteral("text"), text}};
    const int i = notebook::append_kernel_event(running_outputs_, ev);
    if (CellWidget* w = widget_at(index_of(running_id_)); w && i >= 0)
        w->apply_output(i, running_outputs_[i]);
}

void CodeEditorScreen::on_exec_event(const QString& token, const QJsonObject& event) {
    if (token != running_token_)
        return;
    const int i = notebook::append_kernel_event(running_outputs_, event);
    if (CellWidget* w = widget_at(index_of(running_id_)); w && i >= 0)
        w->apply_output(i, running_outputs_[i]);
}

void CodeEditorScreen::on_exec_finished(const QString& token, const QString& status, qint64 elapsed_ms,
                                        bool truncated, bool kernel_lost) {
    if (token != running_token_)
        return;
    const QString id = running_id_;
    const int idx = index_of(id);
    CellWidget* w = widget_at(idx);
    if (truncated) {
        CellOutput note;
        note.is_stderr = true;
        note.text = tr("[output truncated — only the first 2 MB were kept]\n");
        running_outputs_.append(note);
        if (w)
            w->apply_output(static_cast<int>(running_outputs_.size() - 1), note);
    }
    ++exec_counter_;
    const bool ok = status == QLatin1String("ok");
    const CellWidget::RunState state = ok ? CellWidget::RunState::Done
                                       : status == QLatin1String("error") ? CellWidget::RunState::Error
                                                                          : CellWidget::RunState::Cancelled;
    if (idx >= 0) {
        doc_.cells[idx].outputs = running_outputs_;
        doc_.cells[idx].execution_count = exec_counter_;
        if (w) {
            w->set_exec_count(exec_counter_);
            w->set_run_state(state, elapsed_ms);
        }
        outputs_dirty_ = true;
        NotebookRunRecord rec{id, notebook::cell_display_name(doc_.cells[idx]), QDateTime::currentDateTime(),
                              elapsed_ms, status, exec_counter_};
        if (rec.label.isEmpty())
            rec.label = tr("Cell %1").arg(idx + 1);
        runs_.prepend(rec);
        if (runs_.size() > 500)
            runs_.removeLast();
        if (inspector_)
            inspector_->add_run(rec);
    }
    last_run_text_ = ok ? tr("Last run %1").arg(fmt_ms(elapsed_ms))
                   : status == QLatin1String("error") ? tr("Last run failed after %1").arg(fmt_ms(elapsed_ms))
                   : status == QLatin1String("timeout") ? tr("Last run hit the time limit")
                                                        : tr("Last run interrupted after %1").arg(fmt_ms(elapsed_ms));
    last_run_tone_ = ok ? QString() : status == QLatin1String("error") ? QStringLiteral("danger") : QStringLiteral("warn");
    running_id_.clear();
    running_token_.clear();
    running_outputs_.clear();
    if (kernel_lost) {
        exec_counter_ = 0;
        status_->show_notice(tr("The kernel was restarted — run the cells above again to restore their variables."),
                             tr("RUN ABOVE"), [this]() { run_above(); }, 12000, NbsbTone::Warn);
    }
    if (ok) {
        start_next();
    } else {
        abort_queue(status == QLatin1String("error") ? tr("Stopped at the first error")
                                                     : tr("Stopped"));
        tick_timer_->stop();
        cmd_->set_busy(false);
    }
    if (queue_.isEmpty() && running_id_.isEmpty()) {
        tick_timer_->stop();
        cmd_->set_busy(false);
    }
    refresh_status();
    refresh_outline();
    refresh_save_state();
}

void CodeEditorScreen::abort_queue(const QString& why) {
    const int n = static_cast<int>(queue_.size());
    for (const QString& id : queue_)
        if (CellWidget* w = widget_at(index_of(id)))
            w->set_run_state(CellWidget::RunState::Idle);
    queue_.clear();
    if (n > 0 && !why.isEmpty())
        status_->show_notice(tr("%1 — %n queued cell(s) not run.", "", n).arg(why), QString(), {}, 9000,
                             NbsbTone::Warn);
}

void CodeEditorScreen::interrupt() {
    abort_queue(QString());
    if (!running_id_.isEmpty()) {
        kernel_->interrupt();
        status_->show_notice(tr("Interrupting…"), QString(), {}, 4000, NbsbTone::Warn);
    }
    cmd_->set_busy(!running_id_.isEmpty());
}

void CodeEditorScreen::restart_kernel(bool confirm, bool run_all_after) {
    if (!open_)
        return;
    if (confirm && kernel_->alive() &&
        QMessageBox::question(this, tr("Restart Kernel"),
                              tr("Restart the kernel? Its variables are lost; the outputs on the page stay."),
                              QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Yes) != QMessageBox::Yes)
        return;
    abort_queue(QString());
    kernel_->set_environment(doc_.environment());
    kernel_->set_working_dir(working_dir());
    kernel_->restart(); // a run in flight finishes as stopped
    exec_counter_ = 0;
    status_->show_notice(tr("Kernel restarted — a fresh Python session, no variables."));
    if (run_all_after)
        run_all();
}

void CodeEditorScreen::set_environment(const QString& env) {
    if (!open_ || env == doc_.environment())
        return;
    if (kernel_->alive() &&
        QMessageBox::question(this, tr("Switch Environment"),
                              tr("Switching to %1 restarts the kernel; its variables are lost.").arg(env.toUpper()),
                              QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Yes) != QMessageBox::Yes) {
        cmd_->set_environment(doc_.environment());
        return;
    }
    abort_queue(QString());
    kernel_->shutdown();
    doc_.set_environment(env);
    kernel_->set_environment(env);
    cmd_->set_environment(env);
    exec_counter_ = 0;
    mark_changed();
    refresh_kernel_badge();
}

void CodeEditorScreen::on_kernel_reply(const QJsonObject& event) {
    const QString type = event.value(QStringLiteral("type")).toString();
    const QString id = event.value(QStringLiteral("id")).toString();
    if (type == QLatin1String("complete")) {
        const auto req = completion_requests_.take(id);
        if (CellWidget* w = widget_at(index_of(req.first))) {
            QStringList matches;
            for (const QJsonValue& v : event.value(QStringLiteral("matches")).toArray())
                matches << v.toString();
            w->editor()->show_completions(req.second, matches);
        }
    } else if (type == QLatin1String("inspect")) {
        if (id == help_request_ && inspector_)
            inspector_->set_help(event);
    } else if (type == QLatin1String("preview")) {
        if (id == preview_request_ && inspector_) {
            set_inspector_visible(true);
            inspector_->show_preview(event);
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Help, completion, find

void CodeEditorScreen::request_help(const QString& symbol) {
    ensure_inspector();
    inspector_->show_tab(NotebookInspector::Tab::Help);
    last_help_symbol_ = symbol;
    if (symbol.isEmpty()) {
        inspector_->show_shortcuts();
        return;
    }
    if (kernel_->alive() && kernel_->current_run().isEmpty() && kernel_->state() == NotebookKernel::State::Idle) {
        help_request_ = QStringLiteral("h%1").arg(++req_seq_);
        kernel_->inspect(help_request_, symbol);
        inspector_->set_help_pending(symbol);
    } else if (kernel_->alive()) {
        inspector_->set_help_unavailable(symbol, tr("The kernel is busy running a cell; ask again (Ctrl+I) when it finishes."));
    } else {
        inspector_->set_help_unavailable(symbol, tr("Help comes from the live kernel. Run a cell first — the one that "
                                                    "imports or defines %1 — then press Ctrl+I again.")
                                                     .arg(symbol));
    }
}

void CodeEditorScreen::on_completion_requested(const QString& id, const QString& prefix) {
    CellWidget* w = widget_at(index_of(id));
    if (!w)
        return;
    if (kernel_->alive() && kernel_->current_run().isEmpty()) {
        const QString req = QStringLiteral("c%1").arg(++req_seq_);
        completion_requests_.insert(req, {id, prefix});
        kernel_->complete(req, prefix);
        return;
    }
    // No kernel (or it is busy): names from the notebook's text and Python keywords.
    if (prefix.contains(QLatin1Char('.')))
        return;
    sync_sources();
    QStringList words = python_words();
    for (const NotebookCell& c : doc_.cells)
        words << notebook::identifiers_in(c.source);
    QStringList matches;
    for (const QString& word : words)
        if (word.startsWith(prefix) && word != prefix && !matches.contains(word))
            matches << word;
    std::sort(matches.begin(), matches.end(), [](const QString& a, const QString& b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    w->editor()->show_completions(prefix, matches.mid(0, 200));
}

void CodeEditorScreen::run_find(bool restart, bool backwards) {
    if (!open_ || find_text_.isEmpty())
        return;
    ensure_all_built();
    sync_sources();
    find_hits_.clear();
    for (int i = 0; i < doc_.cells.size(); ++i) {
        QVector<QPair<int, int>> ranges;
        const QString& src = doc_.cells[i].source;
        for (qsizetype p = src.indexOf(find_text_, 0, Qt::CaseInsensitive); p >= 0;
             p = src.indexOf(find_text_, p + std::max<qsizetype>(1, find_text_.size()), Qt::CaseInsensitive)) {
            find_hits_.append({i, static_cast<int>(p)});
            ranges.append({static_cast<int>(p), static_cast<int>(find_text_.size())});
        }
        cells_[i]->editor()->set_search_ranges(ranges);
    }
    if (find_hits_.isEmpty()) {
        find_pos_ = -1;
        cmd_->set_find_status(tr("no matches"));
        return;
    }
    const int n = static_cast<int>(find_hits_.size());
    if (restart || find_pos_ < 0 || find_pos_ >= n) {
        find_pos_ = 0;
        for (int k = 0; k < n; ++k)
            if (find_hits_[k].first >= std::max(0, selected_)) {
                find_pos_ = k;
                break;
            }
    } else {
        find_pos_ = (find_pos_ + (backwards ? n - 1 : 1)) % n;
    }
    const auto [cell, pos] = find_hits_[find_pos_];
    CellWidget* w = widget_at(cell);
    if (w->type() != NotebookCell::Type::Code)
        w->set_markdown_editing(true);
    if (w->input_collapsed()) {
        doc_.cells[cell].input_collapsed = false;
        w->set_input_collapsed(false);
    }
    select_cell(cell, Mode::Command, false, false);
    QTextCursor c(w->editor()->document());
    c.setPosition(pos);
    c.setPosition(pos + static_cast<int>(find_text_.size()), QTextCursor::KeepAnchor);
    w->editor()->setTextCursor(c);
    const QRect r = w->editor()->cursorRect(c);
    QTimer::singleShot(0, this, [this, w = QPointer<CellWidget>(w), r]() {
        if (!w)
            return;
        const QPoint p = w->editor()->viewport()->mapTo(canvas_, r.center());
        scroll_->ensureVisible(p.x(), p.y(), 40, scroll_->viewport()->height() / 3);
    });
    cmd_->set_find_status(tr("%1 / %2").arg(find_pos_ + 1).arg(n));
}

void CodeEditorScreen::clear_find() {
    find_text_.clear();
    find_hits_.clear();
    find_pos_ = -1;
    for (CellWidget* w : cells_)
        w->editor()->set_search_ranges({});
    if (cmd_)
        cmd_->set_find_status(QString());
}

// ═════════════════════════════════════════════════════════════════════════════
// Commands, badge, status

void CodeEditorScreen::on_command(NotebookCommandBar::Cmd cmd) {
    using C = NotebookCommandBar::Cmd;
    switch (cmd) {
        case C::New:
            new_notebook();
            break;
        case C::Open:
            open_dialog();
            break;
        case C::Save:
            save();
            break;
        case C::SaveAs:
            save_as();
            break;
        case C::ExportPy:
            export_python();
            break;
        case C::ExportHtml:
            export_html();
            break;
        case C::Close:
            close_notebook();
            break;
        case C::RunCell:
            run_selected(AfterRun::Stay);
            break;
        case C::RunAll:
            run_all();
            break;
        case C::RunAbove:
            run_above();
            break;
        case C::RunBelow:
            run_below();
            break;
        case C::Interrupt:
            interrupt();
            break;
        case C::Restart:
            restart_kernel(true, false);
            break;
        case C::RestartRunAll:
            restart_kernel(true, true);
            break;
        case C::Shutdown:
            abort_queue(QString());
            kernel_->shutdown();
            break;
        case C::InsertCode:
            insert_cell(selected_ + 1, NotebookCell::Type::Code, QString(), Mode::Edit);
            break;
        case C::InsertText:
            insert_cell(selected_ + 1, NotebookCell::Type::Markdown, QString(), Mode::Edit);
            break;
        case C::InsertPrices:
        case C::InsertPlot:
        case C::InsertReturns:
        case C::InsertConnector:
        case C::InsertCsv:
            insert_snippet(cmd);
            break;
        case C::ToggleRail:
            set_rail_visible(!rail_visible_);
            break;
        case C::ToggleInspector:
            set_inspector_visible(!inspector_visible_);
            break;
        case C::ClearOutputs:
            clear_outputs(-1);
            break;
        case C::ToggleLineNumbers:
            set_line_numbers(!line_numbers_);
            break;
        case C::ToggleAutosave:
            autosave_ = !autosave_;
            cmd_->set_autosave_checked(autosave_);
            refresh_save_state();
            persist();
            break;
        case C::Shortcuts:
            set_inspector_visible(true);
            inspector_->show_tab(NotebookInspector::Tab::Help);
            inspector_->show_shortcuts();
            break;
    }
}

void CodeEditorScreen::refresh_kernel_badge() {
    if (!cmd_)
        return;
    const QString env = (open_ ? doc_.environment() : QStringLiteral("numpy2")).toUpper();
    const QString ver = kernel_->python_version();
    const QString base = ver.isEmpty() ? tr("PY · %1").arg(env) : tr("PY %1 · %2").arg(ver.section('.', 0, 1), env);
    QString state;
    QString tone;
    QString tip;
    switch (kernel_->state()) {
        case NotebookKernel::State::Off:
        case NotebookKernel::State::Unavailable: {
            QString problem = kernel_->state() == NotebookKernel::State::Unavailable ? kernel_->status_reason()
                                                                                      : QString();
            if (problem.isEmpty())
                problem = NotebookKernel::start_problem(env.toLower());
            if (!problem.isEmpty()) {
                const bool installing = python::PythonSetupManager::instance().is_running();
                state = installing ? tr("PYTHON INSTALLING") : tr("PYTHON NOT SET UP");
                tone = installing ? QStringLiteral("warn") : QStringLiteral("danger");
                tip = problem;
            } else {
                state = tr("OFF");
                tone = QStringLiteral("dim");
                tip = tr("The kernel starts when you run a cell and keeps your variables until it is restarted.");
            }
            break;
        }
        case NotebookKernel::State::Starting:
            state = tr("STARTING");
            tone = QStringLiteral("warn");
            tip = tr("Starting Python…");
            break;
        case NotebookKernel::State::Idle:
            state = tr("IDLE");
            tone = QStringLiteral("ok");
            tip = tr("Ready. Variables are kept between cells.");
            break;
        case NotebookKernel::State::Busy:
            state = tr("BUSY");
            tone = QStringLiteral("warn");
            tip = tr("Running a cell — I, I or STOP interrupts it.");
            break;
        case NotebookKernel::State::Interrupting:
            state = tr("INTERRUPTING");
            tone = QStringLiteral("warn");
            tip = tr("Waiting for the cell to stop.");
            break;
    }
    const QString dot = tone == QLatin1String("dim") ? QStringLiteral("○ ") : QStringLiteral("● ");
    tip += QLatin1Char('\n') + tr("Environment %1 — click for environment, time limit, restart.").arg(env);
    cmd_->set_kernel(dot + base + QStringLiteral("  ·  ") + state, dot + state, tone, tip);
    nbsb_set(status_, "kernel", dot + tr("KERNEL %1").arg(state), tone, tip);
    nbsb_set(status_, "env", base, QString(),
             env == QLatin1String("NUMPY1") ? tr("numpy 1 environment: vectorbt, backtesting.py, bt, ffn, PyPortfolioOpt")
                                            : tr("Default environment: pandas, numpy 2, scipy, statsmodels, yfinance"));
}

void CodeEditorScreen::refresh_title() {
    if (!cmd_)
        return;
    if (!open_) {
        cmd_->set_notebook(QString(), QString(), false, false);
        nbsb_path(status_, QString());
        return;
    }
    const QString name = path_.isEmpty() ? tr("Untitled notebook")
                                         : strip_ipynb(NotebookLibraryService::instance().display_name_for(path_));
    cmd_->set_notebook(name, path_.isEmpty() ? tr("Not saved yet — Ctrl+S saves it") : QDir::toNativeSeparators(path_),
                       dirty_, true);
    nbsb_path(status_, path_);
}

void CodeEditorScreen::refresh_save_state() {
    if (!status_)
        return;
    if (!open_) {
        nbsb_set(status_, "save", QString());
        return;
    }
    const QString time_fmt = QStringLiteral("HH:mm");
    if (!save_error_.isEmpty()) {
        nbsb_set(status_, "save", tr("Save failed"), QStringLiteral("danger"), save_error_);
    } else if (path_.isEmpty()) {
        nbsb_set(status_, "save", dirty_ ? tr("Not saved yet") : tr("New notebook"),
                 dirty_ ? QStringLiteral("warn") : QString(),
                 autosaved_at_.isValid()
                     ? tr("A recovery copy was kept at %1").arg(QLocale().toString(autosaved_at_.time(), time_fmt))
                     : tr("Ctrl+S saves it"));
    } else if (dirty_) {
        nbsb_set(status_, "save", tr("Unsaved changes"), QStringLiteral("warn"),
                 autosave_ ? tr("Saved automatically within a minute") : tr("Autosave is off — Ctrl+S"));
    } else if (outputs_dirty_) {
        nbsb_set(status_, "save", tr("Outputs not saved"), QString(),
                 autosave_ ? tr("Saved automatically within a minute") : tr("Ctrl+S saves them"));
    } else {
        const QDateTime t = autosaved_at_.isValid() && autosaved_at_ > saved_at_ ? autosaved_at_ : saved_at_;
        nbsb_set(status_, "save",
                 t.isValid() ? (t == autosaved_at_ ? tr("Autosaved %1") : tr("Saved %1"))
                                   .arg(QLocale().toString(t.time(), time_fmt))
                             : tr("Saved"),
                 QStringLiteral("dim"), QDir::toNativeSeparators(path_));
    }
}

void CodeEditorScreen::refresh_status() {
    if (!status_)
        return;
    refresh_save_state();
    if (!open_) {
        nbsb_set(status_, "cells", tr("No notebook open"));
        nbsb_set(status_, "last_run", QString());
        nbsb_cursor(status_, 0, 0);
        return;
    }
    int code = 0;
    int text = 0;
    for (const NotebookCell& c : doc_.cells)
        (c.is_code() ? code : text) += 1;
    nbsb_set(status_, "cells",
             tr("Cell %1 / %2  ·  %3 code  ·  %4 text").arg(selected_ + 1).arg(doc_.cells.size()).arg(code).arg(text));
    if (!running_id_.isEmpty()) {
        const int n = static_cast<int>(queue_.size());
        nbsb_set(status_, "last_run",
                 n > 0 ? tr("Running cell %1 · %n queued", "", n).arg(index_of(running_id_) + 1)
                       : tr("Running cell %1").arg(index_of(running_id_) + 1),
                 QStringLiteral("warn"));
    } else {
        nbsb_set(status_, "last_run", last_run_text_.isEmpty() ? tr("Not run yet") : last_run_text_, last_run_tone_);
    }
}

void CodeEditorScreen::refresh_outline() {
    if (!outline_ || !outline_->isVisibleTo(this))
        return; // refreshed when its tab is shown
    if (!open_) {
        outline_->set_entries({}, QString());
        return;
    }
    sync_sources();
    QVector<NotebookOutlinePanel::Entry> entries;
    for (const auto& e : notebook::outline(doc_)) {
        const NotebookCell& c = doc_.cells[e.cell_index];
        NotebookOutlinePanel::Entry en;
        en.cell_id = c.id;
        en.level = e.level;
        en.text = e.text;
        en.exec_count = c.execution_count;
        const CellWidget* w = widget_at(e.cell_index);
        en.run_state = w ? static_cast<int>(w->run_state()) : 0;
        entries.append(en);
    }
    outline_->set_entries(entries, selected_ >= 0 && selected_ < doc_.cells.size() ? doc_.cells[selected_].id : QString());
}

void CodeEditorScreen::refresh_start_view() {
    if (!start_)
        return;
    const QFileInfo rec(recovery_path());
    start_->refresh(recent_, !open_ && rec.exists() ? rec.lastModified() : QDateTime());
}

// ═════════════════════════════════════════════════════════════════════════════
// Appearance, language, state

void CodeEditorScreen::on_theme_refresh() {
    theme_refresh_pending_ = false;
    for (CellWidget* w : cells_)
        w->editor()->refresh_theme(); // highlighter formats, line-number colours
    kernel_->set_chart_style(nb_chart_style());
}

void CodeEditorScreen::on_appearance_settled() {
    for (CellWidget* w : cells_) {
        w->refresh_theme(); // markdown and traceback HTML carry their colours
        w->refit();
    }
    if (inspector_)
        inspector_->refresh_theme();
    if (library_)
        library_->refresh(recent_);
    refresh_outline();
    apply_splitter_sizes();
}

void CodeEditorScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    flush_pending_state();
}

void CodeEditorScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(event);
}

void CodeEditorScreen::retranslate() {
    const QString tabs[3] = {tr("LIBRARY"), tr("OUTLINE"), tr("VARIABLES")};
    const QString tips[3] = {tr("Templates, recent and saved notebooks"),
                             tr("Headings and cells — click to jump"),
                             tr("Variables in the running kernel")};
    for (int i = 0; i < 3; ++i) {
        rail_tabs_[i]->setText(tabs[i]);
        rail_tabs_[i]->setToolTip(tips[i]);
    }
    add_code_->setText(tr("+  CODE"));
    add_code_->setToolTip(tr("Add a code cell at the end"));
    add_text_->setText(tr("+  TEXT"));
    add_text_->setToolTip(tr("Add a text (markdown) cell at the end"));
    refresh_title();
    refresh_kernel_badge();
    refresh_status();
}

void CodeEditorScreen::persist() {
    ScreenStateManager::instance().notify_changed(this);
}

QVariantMap CodeEditorScreen::save_state() const {
    QVariantList recent;
    for (const RecentNotebook& r : recent_)
        recent << QVariantMap{{"path", r.path}, {"opened", r.opened.toString(Qt::ISODate)}};
    return {{"notebook_path", open_ ? path_ : QString()},
            {"kernel", open_ ? doc_.environment() : QStringLiteral("numpy2")},
            {"timeout_s", timeout_s_},
            {"rail_visible", rail_visible_},
            {"rail_tab", rail_tab_},
            {"rail_width", rail_width_},
            {"inspector_visible", inspector_visible_},
            {"inspector_tab", inspector_ ? static_cast<int>(inspector_->current_tab()) : inspector_tab_},
            {"inspector_width", inspector_width_},
            {"autosave", autosave_},
            {"line_numbers", line_numbers_},
            {"recovery_open", open_ && path_.isEmpty() && dirty_},
            {"recent", recent}};
}

void CodeEditorScreen::restore_state(const QVariantMap& state) {
    timeout_s_ = std::clamp(state.value("timeout_s", timeout_s_).toInt(), 60, 7200);
    cmd_->set_timeout(timeout_s_);
    autosave_ = state.value("autosave", true).toBool();
    cmd_->set_autosave_checked(autosave_);
    line_numbers_ = state.value("line_numbers", true).toBool();
    cmd_->set_line_numbers_checked(line_numbers_);
    for (CellWidget* w : cells_)
        w->set_line_numbers_visible(line_numbers_);
    rail_width_ = state.value("rail_width", 0).toInt();
    inspector_width_ = state.value("inspector_width", 0).toInt();
    rail_tab_ = state.value("rail_tab", 0).toInt();
    inspector_tab_ = std::clamp(state.value("inspector_tab", 0).toInt(), 0, 2);
    recent_.clear();
    for (const QVariant& v : state.value("recent").toList()) {
        const QVariantMap m = v.toMap();
        const QString p = m.value("path").toString();
        if (!p.isEmpty())
            recent_.append({p, QDateTime::fromString(m.value("opened").toString(), Qt::ISODate)});
    }
    set_rail_visible(state.value("rail_visible", true).toBool());
    if (state.value("inspector_visible", false).toBool())
        set_inspector_visible(true);

    // Open the last notebook — unless something was opened already (File Manager).
    if (!open_) {
        const QString path = state.value("notebook_path").toString();
        QString why;
        if (!path.isEmpty() && QFileInfo::exists(path) && load_path(path, &why)) {
            // the v1 "kernel" key is superseded by the notebook's own kernelspec
        } else if (state.value("recovery_open").toBool() && QFileInfo::exists(recovery_path())) {
            recover_unsaved();
        }
    }
    refresh_start_view();
}

namespace {

/// The .ipynb a "notebook:<id>" link names: a File Manager id (exact, then any case) or an
/// absolute path to an existing .ipynb file. Empty when there is none.
QString ce_notebook_path(const QString& id) {
    if (id.isEmpty())
        return {};
    auto& fm = services::FileManagerService::instance();
    const QJsonArray files = fm.all_files();
    for (const Qt::CaseSensitivity cs : {Qt::CaseSensitive, Qt::CaseInsensitive}) {
        for (const QJsonValue& v : files) {
            const QJsonObject o = v.toObject();
            const QString name = o.value(QStringLiteral("name")).toString();
            if (o.value(QStringLiteral("id")).toString().compare(id, cs) != 0 ||
                !name.endsWith(QLatin1String(".ipynb"), Qt::CaseInsensitive))
                continue;
            const QString path = fm.full_path(name);
            return !path.isEmpty() && QFileInfo(path).isFile() ? path : QString();
        }
    }
    const QFileInfo fi(id);
    if (fi.isAbsolute() && fi.suffix().compare(QLatin1String("ipynb"), Qt::CaseInsensitive) == 0 && fi.isFile())
        return fi.absoluteFilePath();
    return {};
}

} // namespace

bool CodeEditorScreen::open_section(const QString& section, const QString& focus) {
    Q_UNUSED(focus); // nothing in the editor takes a focus from a link
    const QString key = section.trimmed();
    if (key.isEmpty())
        return true;
    const QString lower = key.toLower();
    const QLatin1String rail_ids[] = {QLatin1String("library"), QLatin1String("outline"), QLatin1String("variables")};
    for (int i = 0; i < 3; ++i) {
        if (lower == rail_ids[i]) {
            if (!rail_visible_)
                set_rail_visible(true);
            rail_tabs_[i]->click(); // the rail tab's own click path (page built on first show, state saved)
            return true;
        }
    }
    // New / template / notebook may ask about unsaved changes in a modal: run them after the link has landed.
    if (lower == QLatin1String("new")) {
        QMetaObject::invokeMethod(this, [this]() { new_notebook(); }, Qt::QueuedConnection);
        return true;
    }
    QString rest;
    if (command_rows::split_kind(key, "template", &rest)) {
        const auto& cat = NotebookLibraryService::instance().catalog();
        int index = -1;
        for (const Qt::CaseSensitivity cs : {Qt::CaseSensitive, Qt::CaseInsensitive})
            for (int i = 0; i < cat.size() && index < 0; ++i)
                if (cat[i].id.compare(rest, cs) == 0)
                    index = i;
        if (index < 0)
            return false;
        // The template's OPEN: makes the user's own copy and opens it; nothing runs.
        QMetaObject::invokeMethod(this, [this, index]() { open_template(index); }, Qt::QueuedConnection);
        return true;
    }
    if (command_rows::split_kind(key, "notebook", &rest)) {
        const QString path = ce_notebook_path(rest);
        if (path.isEmpty())
            return false;
        // Opening shows the saved outputs and never runs a cell.
        QMetaObject::invokeMethod(this, [this, path]() { open_notebook_path(path); }, Qt::QueuedConnection);
        return true;
    }
    return false;
}

// ── Search box rows (core/command/CommandRegistry.h; screens/common/CommandRows.h) ──
namespace {

QList<command::CommandItem> ce_command_rows() {
    namespace rows = command_rows;
    const QString where = CodeEditorScreen::tr("Code Editor");
    const QString screen = QStringLiteral("code_editor");
    return {
        rows::section(screen, QStringLiteral("library"), CodeEditorScreen::tr("Notebook library"),
                      {QStringLiteral("library"), QStringLiteral("templates"), QStringLiteral("my notebooks"),
                       QStringLiteral("recent notebooks"), QStringLiteral("jupyter")},
                      where),
        rows::section(screen, QStringLiteral("outline"), CodeEditorScreen::tr("Notebook outline"),
                      {QStringLiteral("outline"), QStringLiteral("headings"), QStringLiteral("table of contents"),
                       QStringLiteral("cells")},
                      where),
        rows::section(screen, QStringLiteral("variables"), CodeEditorScreen::tr("Notebook variables"),
                      {QStringLiteral("variables"), QStringLiteral("variable explorer"), QStringLiteral("dataframes"),
                       QStringLiteral("kernel state")},
                      where),
    };
}

/// Templates of the bundled library (in memory once loaded) as Data rows.
void ce_query_templates(const QString& query, quint64, command::ResultSink sink) {
    const auto& cat = NotebookLibraryService::instance().catalog();
    const QString context = CodeEditorScreen::tr("Notebook template");
    const QString subtitle = CodeEditorScreen::tr("Opens your own copy in the Code Editor; nothing runs");
    QList<command::CommandItem> all;
    all.reserve(cat.size());
    for (const services::NotebookCatalogEntry& e : cat) {
        command::CommandItem it;
        it.id = QStringLiteral("code_editor.template.") + e.id;
        it.title = e.title;
        it.keywords = e.tags;
        it.keywords << e.category << QStringLiteral("template") << QStringLiteral("notebook") << QStringLiteral("example");
        it.group = QString::fromLatin1(command::groups::kData);
        it.subtitle = subtitle;
        it.screen_id = QStringLiteral("code_editor");
        it.section = QStringLiteral("template:") + e.id;
        it.owner = QStringLiteral("code_editor");
        it.context = context;
        all.append(it);
    }
    sink(command_rows::best_matches(all, query, 6), true);
}

/// The user's notebooks in managed storage (the Library's MY NOTEBOOKS). The File Manager's
/// in-memory index is read here (GUI thread); the files are checked on a worker.
void ce_query_notebooks(const QString& query, quint64, command::ResultSink sink) {
    struct Candidate {
        QString row_id;
        QString title;
        QString path;
    };
    auto& fm = services::FileManagerService::instance();
    QList<Candidate> candidates;
    for (const QJsonValue& v : fm.files_for_screen(QStringLiteral("code_editor"))) {
        const QJsonObject o = v.toObject();
        const QString stored = o.value(QStringLiteral("name")).toString();
        if (!stored.endsWith(QLatin1String(".ipynb"), Qt::CaseInsensitive))
            continue;
        const QString path = fm.full_path(stored);
        if (path.isEmpty())
            continue;
        QString title = o.value(QStringLiteral("originalName")).toString();
        if (title.isEmpty())
            title = stored;
        candidates.append({QStringLiteral("code_editor.notebook.") + stored, strip_ipynb(title), path});
    }
    const QString context = CodeEditorScreen::tr("Notebook");
    const QString subtitle = CodeEditorScreen::tr("Opens this notebook in the Code Editor; nothing runs");
    const QString q = query.trimmed();
    QThreadPool::globalInstance()->start([candidates, context, subtitle, q, sink]() {
        struct Found {
            QDateTime modified;
            command::CommandItem row;
        };
        QList<Found> found;
        for (const Candidate& c : candidates) {
            const QFileInfo fi(c.path);
            if (!fi.isFile())
                continue;
            command::CommandItem it;
            it.id = c.row_id;
            it.title = c.title;
            it.keywords = {QStringLiteral("notebook"), QStringLiteral("ipynb"), QStringLiteral("jupyter")};
            it.group = QString::fromLatin1(command::groups::kSaved);
            it.subtitle = subtitle;
            it.screen_id = QStringLiteral("code_editor");
            it.section = QStringLiteral("notebook:") + fi.absoluteFilePath();
            it.owner = QStringLiteral("code_editor");
            it.context = context;
            found.append({fi.lastModified(), it});
        }
        std::stable_sort(found.begin(), found.end(),
                         [](const Found& a, const Found& b) { return a.modified > b.modified; }); // newest first on ties
        QList<command::CommandItem> rows;
        rows.reserve(found.size());
        for (const Found& f : found)
            rows.append(f.row);
        sink(command_rows::best_matches(rows, q, 6), true);
    });
}

void ce_install_commands() {
    command_rows::install(QStringLiteral("code_editor"), &ce_command_rows);
    command::ProviderSpec notebooks;
    notebooks.id = QStringLiteral("code_editor.notebooks");
    notebooks.group = QString::fromLatin1(command::groups::kSaved);
    notebooks.query = &ce_query_notebooks;
    notebooks.owner = QStringLiteral("code_editor");
    notebooks.cap = 6;
    notebooks.min_chars = 2;
    notebooks.slow = true; // stats the files on a worker
    command::CommandRegistry::instance().register_provider(notebooks);
    command::ProviderSpec templates;
    templates.id = QStringLiteral("code_editor.templates");
    templates.group = QString::fromLatin1(command::groups::kData);
    templates.query = &ce_query_templates;
    templates.owner = QStringLiteral("code_editor");
    templates.cap = 6;
    templates.min_chars = 2;
    command::CommandRegistry::instance().register_provider(templates);
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(ce_install_commands)

} // namespace fincept::screens
