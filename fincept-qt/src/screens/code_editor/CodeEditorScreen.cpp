// src/screens/code_editor/CodeEditorScreen.cpp
// Fincept Notebook — Obsidian design system implementation.
// Two views: a prebuilt-notebook Library (cards) and the notebook Editor.

// CellWidget + CodeTextEdit lives in CodeEditorScreen_Cells.cpp.
// CellNavigator lives in CodeEditorScreen_Navigator.cpp.
// Library view (cards/filters) lives in CodeEditorScreen_Library.cpp.

#include "screens/code_editor/CodeEditorScreen.h"

#include "core/keys/KeyConfigManager.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "services/file_manager/FileManagerService.h"
#include "services/notebooks/NotebookLibraryService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyle>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

// ── Stylesheet ────────────────────────────────────────────────────────────────
// Object-name stylesheet shared across the screen, mirroring McpServersScreen's
// kStyle() pattern so the Fincept Notebook chrome matches the MCP design system
// (header tab bar, search, button family, cards). Re-applied on theme change.

namespace {
using namespace fincept::ui;

inline QString kStyle() {
    return QString(
               // Screen / header
               "#nbScreen { background:%1; }"
               "#nbHeader { background:%2; border-bottom:2px solid %3; }"
               "#nbHeaderTitle { color:%3; font-weight:700; letter-spacing:1px; background:transparent; }"

               // View tabs (LIBRARY / EDITOR)
               "#nbViewBtn { background:transparent; color:%5; border:1px solid %8; "
               "  font-weight:700; padding:4px 14px; }"
               "#nbViewBtn:hover { color:%4; border-color:%9; }"
               "#nbViewBtn[active=\"true\"] { background:%3; color:%1; border-color:%3; }"

               // Search
               "#nbSearchInput { background:%1; color:%4; border:1px solid %8; padding:4px 8px; min-width:200px; }"
               "#nbSearchInput:focus { border-color:%9; }"

               // Buttons
               "#nbPrimaryBtn { background:%3; color:%1; border:none; padding:5px 16px; font-weight:700; }"
               "#nbPrimaryBtn:hover { background:%10; }"
               "#nbSecondaryBtn { background:transparent; color:%5; border:none; padding:4px 10px; font-weight:600; "
               "  letter-spacing:0.5px; }"
               "#nbSecondaryBtn:hover { color:%4; background:%12; }"
               "#nbAccentBtn { background:transparent; color:%13; border:none; padding:4px 10px; font-weight:600; "
               "  letter-spacing:0.5px; }"
               "#nbAccentBtn:hover { color:%4; background:%12; }"

               // Editor toolbar / status bar
               "#nbToolbar { background:%2; border-bottom:1px solid %8; }"
               "#nbStatusBar { background:%2; border-top:1px solid %8; }"

               // Library filter chips
               "#nbChip { background:transparent; color:%5; border:1px solid %8; font-weight:700; padding:3px 10px; }"
               "#nbChip:hover { color:%4; border-color:%9; }"
               "#nbChip[active=\"true\"] { background:rgba(217,119,6,0.12); color:%3; border-color:%3; }"
               "#nbLibLabel { color:%5; background:transparent; }"

               // Library cards
               "#nbCard { background:%7; border:1px solid %8; }"
               "#nbCard:hover { border-color:%9; }"
               "#nbCardTitle { color:%4; font-weight:700; background:transparent; }"
               "#nbCardSummary { color:%5; background:transparent; }"
               "#nbCardMeta { color:%11; background:transparent; }"
               "#nbCatBadge { color:%13; font-weight:700; background:rgba(8,145,178,0.12); "
               "  padding:1px 7px; border:1px solid rgba(8,145,178,0.35); }"
               "#nbDiffBeginner { color:%6; font-weight:700; background:rgba(22,163,74,0.12); "
               "  padding:1px 7px; border:1px solid rgba(22,163,74,0.35); }"
               "#nbDiffIntermediate { color:%3; font-weight:700; background:rgba(217,119,6,0.12); "
               "  padding:1px 7px; border:1px solid rgba(217,119,6,0.35); }"
               "#nbDiffHard { color:%14; font-weight:700; background:rgba(220,38,38,0.12); "
               "  padding:1px 7px; border:1px solid rgba(220,38,38,0.35); }"
               "#nbCardOpenBtn { background:%3; color:%1; border:none; padding:5px 18px; font-weight:700; }"
               "#nbCardOpenBtn:hover { background:%10; }"

               // Live (streaming) cell output + toolbar selectors
               "#nbLiveOut { background:transparent; color:%4; border:none; padding:2px 0; }"
               "#nbToolCombo { background:%1; color:%5; border:1px solid %8; padding:1px 6px; }"
               "#nbToolCombo:hover { border-color:%9; color:%4; }"
               "#nbStopBtn { background:transparent; color:%14; border:1px solid %8; padding:3px 10px; font-weight:700; }"
               "#nbStopBtn:hover { background:rgba(220,38,38,0.12); }"
               "#nbStopBtn:disabled { color:%11; border-color:%8; }"

               // Scroll
               "QScrollBar:vertical { background:%1; width:6px; }"
               "QScrollBar::handle:vertical { background:%8; min-height:24px; }"
               "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }")
        .arg(colors::BG_BASE())        // %1
        .arg(colors::BG_RAISED())      // %2
        .arg(colors::AMBER())          // %3
        .arg(colors::TEXT_PRIMARY())   // %4
        .arg(colors::TEXT_SECONDARY()) // %5
        .arg(colors::POSITIVE())       // %6
        .arg(colors::BG_SURFACE())     // %7
        .arg(colors::BORDER_DIM())     // %8
        .arg(colors::BORDER_BRIGHT())  // %9
        .arg(colors::AMBER_DIM())      // %10
        .arg(colors::TEXT_DIM())       // %11
        .arg(colors::BG_HOVER())       // %12
        .arg(colors::CYAN())           // %13
        .arg(colors::NEGATIVE());      // %14
}

} // namespace

namespace fincept::screens {

using namespace fincept::ui;

// ── nbformat output (de)serialisation ────────────────────────────────────────
// Saved notebooks used to carry an empty "outputs" array for every cell, so a
// notebook reopened after a run showed an execution count but no results.

namespace {

// nbformat stores multi-line text as an array of lines, each keeping its "\n".
QJsonArray nb_text_to_lines(const QString& text) {
    QJsonArray lines;
    const QStringList parts = text.split(QLatin1Char('\n'));
    for (int i = 0; i < parts.size(); ++i) {
        const bool last = (i + 1 == parts.size());
        if (last && parts[i].isEmpty())
            break; // text ended with a newline — no phantom empty line
        lines.append(last ? parts[i] : parts[i] + QLatin1Char('\n'));
    }
    return lines;
}

// A text field is either one string or an array of lines. Terminal escape
// sequences (Jupyter colours its tracebacks) are stripped — the output view is
// plain text.
QString nb_lines_to_text(const QJsonValue& v) {
    QString text;
    if (v.isArray()) {
        for (const QJsonValue& line : v.toArray())
            text += line.toString();
    } else {
        text = v.toString();
    }
    static const QRegularExpression kAnsi(QStringLiteral("\\x1b\\[[0-9;]*[A-Za-z]"));
    text.remove(kAnsi);
    return text;
}

QJsonArray nb_outputs_to_json(const QVector<CellOutput>& outputs, int exec_count) {
    QJsonArray arr;
    for (const CellOutput& out : outputs) {
        QJsonObject o;
        if (out.type == QLatin1String("stream")) {
            o["output_type"] = QStringLiteral("stream");
            o["name"] = out.name.isEmpty() ? QStringLiteral("stdout") : out.name;
            o["text"] = nb_text_to_lines(out.text);
        } else if (out.type == QLatin1String("error")) {
            o["output_type"] = QStringLiteral("error");
            o["ename"] = out.error_name;
            o["evalue"] = out.error_value;
            QJsonArray tb;
            for (const QString& line : out.traceback)
                tb.append(line);
            o["traceback"] = tb;
        } else if (out.type == QLatin1String("execute_result")) {
            o["output_type"] = QStringLiteral("execute_result");
            o["execution_count"] = exec_count > 0 ? QJsonValue(exec_count) : QJsonValue();
            QJsonObject data;
            data["text/plain"] = nb_text_to_lines(out.text);
            o["data"] = data;
            o["metadata"] = QJsonObject();
        } else {
            continue;
        }
        arr.append(o);
    }
    return arr;
}

QVector<CellOutput> nb_outputs_from_json(const QJsonArray& arr) {
    QVector<CellOutput> outputs;
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        const QString type = o.value(QStringLiteral("output_type")).toString();
        CellOutput out;
        if (type == QLatin1String("stream")) {
            out.type = QStringLiteral("stream");
            out.name = o.value(QStringLiteral("name")).toString(QStringLiteral("stdout"));
            out.text = nb_lines_to_text(o.value(QStringLiteral("text")));
        } else if (type == QLatin1String("error")) {
            out.type = QStringLiteral("error");
            out.error_name = o.value(QStringLiteral("ename")).toString();
            out.error_value = o.value(QStringLiteral("evalue")).toString();
            for (const QJsonValue& line : o.value(QStringLiteral("traceback")).toArray())
                out.traceback << nb_lines_to_text(line);
            out.text = out.traceback.join(QLatin1Char('\n'));
        } else if (type == QLatin1String("execute_result") || type == QLatin1String("display_data")) {
            // Only the plain-text rendition can be shown; images/HTML are skipped.
            const QJsonValue plain = o.value(QStringLiteral("data")).toObject().value(QStringLiteral("text/plain"));
            if (plain.isUndefined())
                continue;
            out.type = QStringLiteral("execute_result");
            out.text = nb_lines_to_text(plain);
        } else {
            continue;
        }
        outputs.append(out);
    }
    return outputs;
}

} // namespace

// ═════════════════════════════════════════════════════════════════════════════
// CodeTextEdit — editor with keyboard shortcuts

CodeEditorScreen::CodeEditorScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("nbScreen");
    setStyleSheet(kStyle());
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { setStyleSheet(kStyle()); });
    build_ui();
    on_new_notebook();
    populate_library();
    set_view(0); // land on the Library so users discover the prebuilt notebooks
}

void CodeEditorScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    LOG_INFO("CodeEditor", "Screen shown");
}

void CodeEditorScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    LOG_INFO("CodeEditor", "Screen hidden");
}

void CodeEditorScreen::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    relayout_library_cards(); // re-flow the card grid when the panel width changes
}

void CodeEditorScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_header());

    view_stack_ = new QStackedWidget(this);
    library_page_ = build_library_page();
    view_stack_->addWidget(library_page_);       // 0 — Library
    view_stack_->addWidget(build_editor_page()); // 1 — Editor
    root->addWidget(view_stack_, 1);

    // The status bar advertises Ctrl+S but nothing was ever bound to it, and
    // there was no keyboard route to New/Open/Run-all either. Scoped to this
    // screen so they don't fight other tabs' bindings.
    auto bind = [this](const QKeySequence& seq, void (CodeEditorScreen::*slot)()) {
        auto* sc = new QShortcut(seq, this);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, this, slot);
    };
    bind(QKeySequence(QKeySequence::Save), &CodeEditorScreen::on_save_notebook);
    bind(QKeySequence(QKeySequence::Open), &CodeEditorScreen::on_open_notebook);
    bind(QKeySequence(QKeySequence::New), &CodeEditorScreen::on_new_notebook);
    // Deliberately NOT F5 — KeyConfigManager binds F5 to the global Refresh
    // action with Qt::ApplicationShortcut, and a second F5 would make both
    // ambiguous (Qt fires activatedAmbiguously and neither handler runs).
    bind(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Return), &CodeEditorScreen::on_run_all);
    // Esc interrupts running cells (Jupyter's "interrupt kernel").
    auto* esc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    esc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(esc, &QShortcut::activated, this, [this]() {
        if (!active_runs_.isEmpty() || !run_queue_.isEmpty())
            on_stop_all();
    });
}

// Header bar: title + LIBRARY/EDITOR tabs + Library search + NEW button.
QWidget* CodeEditorScreen::build_header() {
    auto* bar = new QWidget(this);
    bar->setObjectName("nbHeader");
    bar->setFixedHeight(44);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(14, 0, 12, 0);
    hl->setSpacing(8);

    header_title_ = new QLabel(tr("FINCEPT NOTEBOOK"), bar);
    header_title_->setObjectName("nbHeaderTitle");
    header_title_->setStyleSheet(QString("font-family:%1; font-size:%2px;").arg(fonts::DATA_FAMILY).arg(fonts::SMALL));
    hl->addWidget(header_title_);
    hl->addSpacing(10);

    const QStringList tabs = {tr("LIBRARY"), tr("EDITOR")};
    for (int i = 0; i < tabs.size(); ++i) {
        auto* btn = new QPushButton(tabs[i], bar);
        btn->setObjectName("nbViewBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("active", i == 0);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_view_changed(i); });
        hl->addWidget(btn);
        view_btns_.append(btn);
    }

    hl->addStretch(1);

    lib_search_input_ = new QLineEdit(bar);
    lib_search_input_->setObjectName("nbSearchInput");
    lib_search_input_->setPlaceholderText(tr("Search notebooks..."));
    lib_search_input_->setClearButtonEnabled(true);
    connect(lib_search_input_, &QLineEdit::textChanged, this, &CodeEditorScreen::on_library_search);
    hl->addWidget(lib_search_input_);

    header_new_btn_ = new QPushButton(tr("＋  NEW NOTEBOOK"), bar);
    header_new_btn_->setObjectName("nbPrimaryBtn");
    header_new_btn_->setCursor(Qt::PointingHandCursor);
    connect(header_new_btn_, &QPushButton::clicked, this, [this]() {
        on_new_notebook();
        set_view(1);
    });
    hl->addWidget(header_new_btn_);

    return bar;
}

// Editor page: action toolbar + splitter (navigator | cells) + status bar.
QWidget* CodeEditorScreen::build_editor_page() {
    auto* page = new QWidget(this);
    auto* root = new QVBoxLayout(page);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_toolbar());

    splitter_ = new QSplitter(Qt::Horizontal, page);
    splitter_->setHandleWidth(1);
    splitter_->setStyleSheet(QString("QSplitter { background:%1; } QSplitter::handle { background:%2; }")
                                 .arg(colors::BG_BASE(), colors::BORDER_DIM()));

    navigator_ = new CellNavigator(splitter_);
    navigator_->setMinimumWidth(180);
    navigator_->setMaximumWidth(300);
    connect(navigator_, &CellNavigator::cell_selected, this, &CodeEditorScreen::on_select_cell);
    connect(navigator_, &CellNavigator::rename_requested, this, &CodeEditorScreen::on_rename_cell);
    splitter_->addWidget(navigator_);

    scroll_area_ = new QScrollArea(splitter_);
    scroll_area_->setWidgetResizable(true);
    scroll_area_->setStyleSheet(
        QString("QScrollArea { border:none; background:%1; }"
                "QScrollBar:vertical { background:%2; width:6px; margin:0; }"
                "QScrollBar::handle:vertical { background:%3; min-height:40px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
                "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background:transparent; }")
            .arg(colors::BG_BASE(), colors::BG_SURFACE(), colors::BORDER_BRIGHT()));

    cells_container_ = new QWidget(scroll_area_);
    cells_layout_ = new QVBoxLayout(cells_container_);
    cells_layout_->setContentsMargins(8, 4, 12, 40);
    cells_layout_->setSpacing(0);
    cells_layout_->addStretch();

    scroll_area_->setWidget(cells_container_);
    splitter_->addWidget(scroll_area_);

    splitter_->setStretchFactor(0, 0);
    splitter_->setStretchFactor(1, 1);
    splitter_->setSizes({200, 800});

    root->addWidget(splitter_, 1);
    root->addWidget(build_status_bar());
    return page;
}

// ── View switching ─────────────────────────────────────────────────────────

void CodeEditorScreen::set_view(int index) {
    active_view_ = index;
    if (view_stack_)
        view_stack_->setCurrentIndex(index);
    for (int i = 0; i < view_btns_.size(); ++i) {
        view_btns_[i]->setProperty("active", i == index);
        view_btns_[i]->style()->unpolish(view_btns_[i]);
        view_btns_[i]->style()->polish(view_btns_[i]);
    }
    // Search only applies to the Library view.
    if (lib_search_input_)
        lib_search_input_->setVisible(index == 0);
}

void CodeEditorScreen::on_view_changed(int index) {
    set_view(index);
}

QWidget* CodeEditorScreen::build_toolbar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("nbToolbar");
    bar->setFixedHeight(36);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(10, 0, 10, 0);
    hl->setSpacing(2);

    auto add_sep = [&]() {
        auto* sep = new QWidget(bar);
        sep->setFixedSize(1, 18);
        sep->setStyleSheet(QString("background:%1;").arg(colors::BORDER_MED()));
        hl->addWidget(sep);
        hl->addSpacing(4);
    };

    auto make_btn = [&](const QString& text, const char* obj = "nbSecondaryBtn") -> QPushButton* {
        auto* btn = new QPushButton(text, bar);
        btn->setObjectName(obj);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString("font-family:%1; font-size:%2px;").arg(fonts::DATA_FAMILY).arg(fonts::TINY));
        return btn;
    };

    btn_new_ = make_btn(tr("NEW"));
    btn_new_->setAccessibleName(tr("New notebook"));
    btn_new_->setToolTip(tr("New notebook (%1)").arg(QKeySequence(QKeySequence::New).toString()));
    connect(btn_new_, &QPushButton::clicked, this, &CodeEditorScreen::on_new_notebook);
    hl->addWidget(btn_new_);

    btn_open_ = make_btn(tr("OPEN"));
    btn_open_->setAccessibleName(tr("Open notebook"));
    btn_open_->setToolTip(tr("Open notebook (%1)").arg(QKeySequence(QKeySequence::Open).toString()));
    connect(btn_open_, &QPushButton::clicked, this, &CodeEditorScreen::on_open_notebook);
    hl->addWidget(btn_open_);

    btn_save_ = make_btn(tr("SAVE"));
    btn_save_->setAccessibleName(tr("Save notebook"));
    btn_save_->setToolTip(tr("Save notebook (%1)").arg(QKeySequence(QKeySequence::Save).toString()));
    connect(btn_save_, &QPushButton::clicked, this, &CodeEditorScreen::on_save_notebook);
    hl->addWidget(btn_save_);

    add_sep();

    btn_add_cell_ = make_btn(tr("+ CELL"), "nbAccentBtn");
    btn_add_cell_->setAccessibleName(tr("Add cell below the selected cell"));
    connect(btn_add_cell_, &QPushButton::clicked, this, &CodeEditorScreen::on_add_cell);
    hl->addWidget(btn_add_cell_);

    btn_clear_out_ = make_btn(tr("CLEAR OUT"));
    btn_clear_out_->setAccessibleName(tr("Clear all cell outputs"));
    connect(btn_clear_out_, &QPushButton::clicked, this, &CodeEditorScreen::on_clear_outputs);
    hl->addWidget(btn_clear_out_);

    add_sep();

    btn_run_all_ = make_btn(tr("▶  RUN ALL"), "nbPrimaryBtn");
    btn_run_all_->setStyleSheet(QString("font-family:%1; font-size:%2px;").arg(fonts::DATA_FAMILY).arg(fonts::TINY));
    btn_run_all_->setAccessibleName(tr("Run every code cell"));
    btn_run_all_->setToolTip(tr("Run every code cell (Ctrl+Shift+Enter). Cells execute as separate processes and "
                                "do not share state with each other."));
    connect(btn_run_all_, &QPushButton::clicked, this, &CodeEditorScreen::on_run_all);
    hl->addWidget(btn_run_all_);

    hl->addStretch();

    btn_sidebar_ = make_btn(tr("SIDEBAR"));
    btn_sidebar_->setAccessibleName(tr("Toggle the cell navigator sidebar"));
    connect(btn_sidebar_, &QPushButton::clicked, this, &CodeEditorScreen::on_toggle_sidebar);
    hl->addWidget(btn_sidebar_);

    add_sep();

    kernel_label_ = new QLabel(tr("KERNEL: IDLE"), bar);
    kernel_label_->setStyleSheet(QString("color:%1; font-family:%2; font-size:10px; font-weight:600;"
                                         " letter-spacing:0.5px; padding:0 8px;")
                                     .arg(colors::POSITIVE(), fonts::DATA_FAMILY));
    hl->addWidget(kernel_label_);

    btn_stop_ = new QPushButton(tr("■ STOP"), bar);
    btn_stop_->setObjectName("nbStopBtn");
    btn_stop_->setCursor(Qt::PointingHandCursor);
    btn_stop_->setEnabled(false);
    btn_stop_->setAccessibleName(tr("Interrupt running cells"));
    btn_stop_->setToolTip(tr("Interrupt running cells and cancel the RUN ALL queue (Esc)"));
    connect(btn_stop_, &QPushButton::clicked, this, &CodeEditorScreen::on_stop_all);
    hl->addWidget(btn_stop_);

    kernel_combo_ = new QComboBox(bar);
    kernel_combo_->setObjectName("nbToolCombo");
    kernel_combo_->addItem(tr("PY · NUMPY2"), QStringLiteral("numpy2"));
    kernel_combo_->addItem(tr("PY · NUMPY1"), QStringLiteral("numpy1"));
    kernel_combo_->setAccessibleName(tr("Python environment for this notebook"));
    kernel_combo_->setToolTip(tr("Python environment the cells run in.\n"
                                 "NUMPY2 — default: pandas, numpy 2, scipy, statsmodels, yfinance, …\n"
                                 "NUMPY1 — vectorbt, backtesting.py, zipline, bt, ffn, PyPortfolioOpt, financepy"));
    connect(kernel_combo_, &QComboBox::currentIndexChanged, this, [this](int) {
        kernel_ = kernel_combo_->currentData().toString();
        ScreenStateManager::instance().notify_changed(this);
    });
    hl->addWidget(kernel_combo_);

    timeout_combo_ = new QComboBox(bar);
    timeout_combo_->setObjectName("nbToolCombo");
    for (const int minutes : {1, 5, 15, 30, 60, 120})
        timeout_combo_->addItem(tr("LIMIT %1 MIN").arg(minutes), minutes * 60);
    timeout_combo_->setCurrentIndex(3); // 30 min
    timeout_combo_->setAccessibleName(tr("Time limit per cell"));
    timeout_combo_->setToolTip(tr("A cell still running after this long is interrupted and reports TimeoutError"));
    connect(timeout_combo_, &QComboBox::currentIndexChanged, this, [this](int) {
        timeout_s_ = timeout_combo_->currentData().toInt();
        ScreenStateManager::instance().notify_changed(this);
    });
    hl->addWidget(timeout_combo_);

    return bar;
}

QWidget* CodeEditorScreen::build_status_bar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("nbStatusBar");
    bar->setFixedHeight(24);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(10, 0, 10, 0);

    status_label_ = new QLabel(tr("READY"), bar);
    status_label_->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:10px; font-weight:600; letter-spacing:0.5px;")
            .arg(colors::TEXT_SECONDARY(), fonts::DATA_FAMILY));
    hl->addWidget(status_label_);
    hl->addStretch();

    shortcuts_label_ =
        new QLabel(tr("Ctrl+Enter: RUN  |  Shift+Enter: RUN & NEXT  |  Tab: 4 SPACES  |  Ctrl+S: SAVE  |  "
                   "Ctrl+Shift+Enter: RUN ALL  |  Esc: STOP"),
                   bar);
    shortcuts_label_->setStyleSheet(QString("color:%1; font-family:%2; font-size:10px; letter-spacing:0.3px;")
                                        .arg(colors::TEXT_DIM(), fonts::DATA_FAMILY));
    hl->addWidget(shortcuts_label_);

    return bar;
}

// ─────────────────────────────────────────────────────────────────────────────
// Notebook management
// ─────────────────────────────────────────────────────────────────────────────

// ── Unsaved-change tracking ──────────────────────────────────────────────────

void CodeEditorScreen::sync_cells_from_widgets() {
    for (int i = 0; i < cells_.size(); ++i) {
        auto* cw = find_cell_widget(cells_[i].id);
        if (!cw)
            continue;
        const NotebookCell live = cw->cell_data();
        cells_[i].source = live.source;
        cells_[i].cell_type = live.cell_type;
        cells_[i].title = live.title;
    }
}

QString CodeEditorScreen::notebook_fingerprint() const {
    QString fp;
    fp.reserve(1024);
    for (const auto& c : cells_) {
        fp += c.cell_type;
        fp += QLatin1Char('\x1f');
        fp += c.title;
        fp += QLatin1Char('\x1f');
        fp += c.source;
        fp += QLatin1Char('\x1e');
    }
    return fp;
}

bool CodeEditorScreen::confirm_discard_changes(const QString& action) {
    sync_cells_from_widgets();
    if (notebook_fingerprint() == clean_fingerprint_)
        return true;

    const QString name = notebook_path_.isEmpty() ? tr("this notebook") : QFileInfo(notebook_path_).fileName();
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
    if (choice == QMessageBox::Save) {
        on_save_notebook();
        // on_save_notebook() only updates clean_fingerprint_ on a real write —
        // a cancelled Save dialog must not silently drop the edits.
        return notebook_fingerprint() == clean_fingerprint_;
    }
    return true;
}

void CodeEditorScreen::on_new_notebook() {
    // Guarded, except for the very first call from the constructor (no widgets
    // exist yet, so the fingerprint is empty and the guard passes silently).
    if (!confirm_discard_changes(tr("Creating a new notebook")))
        return;

    cells_.clear();
    execution_counter_ = 0;
    notebook_path_.clear();

    NotebookCell cell;
    cell.id = new_cell_id();
    cell.cell_type = "code";
    cell.source = "# Fincept Notebook\n"
                  "# Write Python code and press Ctrl+Enter to run\n"
                  "\n"
                  "print(\"Hello from Fincept Terminal!\")";
    cells_.append(cell);

    selected_cell_id_ = cell.id;
    rebuild_cells();
    clean_fingerprint_ = notebook_fingerprint();
    update_status();
    update_navigator();
}

void CodeEditorScreen::on_add_cell() {
    QString after_id = selected_cell_id_;
    if (after_id.isEmpty() && !cells_.isEmpty())
        after_id = cells_.last().id;
    add_cell_after(after_id, "code");
}

void CodeEditorScreen::on_insert_below(const QString& cell_id) {
    add_cell_after(cell_id, "code");
}

void CodeEditorScreen::add_cell_after(const QString& after_id, const QString& type) {
    // rebuild_cells() re-creates every widget from cells_, which only learns about
    // typing on run/save — pull the live text back first or adding a cell wipes
    // whatever was typed since.
    sync_cells_from_widgets();
    NotebookCell cell;
    cell.id = new_cell_id();
    cell.cell_type = type;

    int idx = find_cell_index(after_id);
    if (idx >= 0)
        cells_.insert(idx + 1, cell);
    else
        cells_.append(cell);

    selected_cell_id_ = cell.id;
    rebuild_cells();
    update_status();
    update_navigator();
}

void CodeEditorScreen::on_delete_cell(const QString& cell_id) {
    if (cells_.size() <= 1)
        return;

    // There is no undo for cell deletion — confirm before dropping real content.
    if (auto* cw = find_cell_widget(cell_id)) {
        if (!cw->cell_data().source.trimmed().isEmpty()) {
            if (QMessageBox::question(this, tr("Delete Cell"),
                                      tr("Delete this cell and its contents?\n\nThis cannot be undone."),
                                      QMessageBox::Yes | QMessageBox::Cancel,
                                      QMessageBox::Cancel) != QMessageBox::Yes)
                return;
        }
    }

    sync_cells_from_widgets(); // the rebuild below must not drop other cells' unsaved typing
    int idx = find_cell_index(cell_id);
    if (idx >= 0) {
        cells_.removeAt(idx);
        if (idx < cells_.size())
            selected_cell_id_ = cells_[idx].id;
        else if (!cells_.isEmpty())
            selected_cell_id_ = cells_.last().id;

        rebuild_cells();
        update_status();
        update_navigator();
    }
}

void CodeEditorScreen::on_move_cell_up(const QString& cell_id) {
    int idx = find_cell_index(cell_id);
    if (idx <= 0)
        return;

    // Sync EVERY cell (not just the moved one) — rebuild_cells() re-creates all
    // widgets from cells_ — and keep outputs, which cell_data() does not carry.
    sync_cells_from_widgets();

    cells_.swapItemsAt(idx, idx - 1);
    rebuild_cells();
    update_navigator();
}

void CodeEditorScreen::on_move_cell_down(const QString& cell_id) {
    int idx = find_cell_index(cell_id);
    if (idx < 0 || idx >= cells_.size() - 1)
        return;

    sync_cells_from_widgets();

    cells_.swapItemsAt(idx, idx + 1);
    rebuild_cells();
    update_navigator();
}

void CodeEditorScreen::on_toggle_cell_type(const QString& cell_id) {
    int idx = find_cell_index(cell_id);
    if (idx < 0)
        return;

    sync_cells_from_widgets();

    cells_[idx].cell_type = (cells_[idx].cell_type == "code") ? "markdown" : "code";
    cells_[idx].outputs.clear();
    cells_[idx].execution_count = 0;

    rebuild_cells();
    update_navigator();
}

void CodeEditorScreen::on_select_cell(const QString& cell_id) {
    if (cell_id == selected_cell_id_)
        return;

    auto* old_cw = find_cell_widget(selected_cell_id_);
    if (old_cw)
        old_cw->set_selected(false);

    selected_cell_id_ = cell_id;

    auto* new_cw = find_cell_widget(cell_id);
    if (new_cw) {
        new_cw->set_selected(true);
        scroll_area_->ensureWidgetVisible(new_cw, 50, 50);
    }
    update_navigator();
}

void CodeEditorScreen::on_rename_cell(const QString& cell_id) {
    int idx = find_cell_index(cell_id);
    if (idx < 0)
        return;

    QString current_name = cells_[idx].title.trimmed();
    if (current_name.isEmpty()) {
        current_name = cells_[idx].source.split('\n').first().trimmed();
        if (current_name.isEmpty())
            current_name = tr("Cell %1").arg(idx + 1);
    }

    bool ok = false;
    const QString name =
        QInputDialog::getText(this, tr("Rename Cell"), tr("Cell name:"), QLineEdit::Normal, current_name, &ok)
            .trimmed();
    if (!ok)
        return;

    // Empty name means "use auto preview from source"
    cells_[idx].title = name;
    if (auto* cw = find_cell_widget(cell_id))
        cw->set_title(name); // else the next sync/save reads the stale title back
    update_navigator();
}

void CodeEditorScreen::on_clear_outputs() {
    sync_cells_from_widgets(); // rebuild below must keep unsaved typing
    for (auto& cell : cells_) {
        cell.outputs.clear();
        cell.execution_count = 0;
    }
    execution_counter_ = 0;
    rebuild_cells();
    update_status();
    update_navigator();
}

void CodeEditorScreen::on_toggle_sidebar() {
    sidebar_visible_ = !sidebar_visible_;
    navigator_->setVisible(sidebar_visible_);
}

void CodeEditorScreen::advance_to_next(const QString& current_id) {
    int idx = find_cell_index(current_id);
    if (idx < 0)
        return;

    if (idx + 1 < cells_.size()) {
        on_select_cell(cells_[idx + 1].id);
    } else {
        // Add a new cell at the end and select it
        add_cell_after(current_id, "code");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Cell execution
// ─────────────────────────────────────────────────────────────────────────────

void CodeEditorScreen::on_run_cell(const QString& cell_id) {
    int idx = find_cell_index(cell_id);
    if (idx < 0 || active_runs_.contains(cell_id))
        return; // already running

    auto* cw = find_cell_widget(cell_id);
    if (cw) {
        const auto outputs = cells_[idx].outputs;
        cells_[idx] = cw->cell_data();
        cells_[idx].outputs = outputs;
    }

    // Bail out BEFORE flipping the running indicator — marking a markdown or
    // empty cell as running left its gutter stuck on "[*]" forever.
    if (cells_[idx].cell_type != "code" || cells_[idx].source.trimmed().isEmpty()) {
        run_next_queued();
        return;
    }

    if (cw) {
        cw->set_running(true);
        cw->begin_stream();
    }

    ++execution_counter_;
    const int exec_num = execution_counter_;
    kernel_busy_ = true;
    refresh_kernel_label();

    services::NotebookRunRequest req;
    req.code = cells_[idx].source;
    req.kernel = kernel_;
    req.timeout_s = timeout_s_;
    // Relative paths in a cell resolve against the notebook's folder, as in Jupyter.
    req.working_dir = notebook_path_.isEmpty() ? services::FileManagerService::instance().storage_dir()
                                               : QFileInfo(notebook_path_).absolutePath();

    QPointer<CodeEditorScreen> self = this;
    const QString cid = cell_id;
    const QString run_id = services::NotebookLibraryService::instance().run_cell(
        req, this,
        [self, cid](const QString& text, bool is_stderr) {
            if (!self)
                return;
            if (auto* w = self->find_cell_widget(cid))
                w->append_stream(text, is_stderr);
        },
        [self, cid, exec_num](const services::NotebookRunResult& result) {
            if (!self)
                return;
            self->active_runs_.remove(cid);
            self->kernel_busy_ = !self->active_runs_.isEmpty();
            self->refresh_kernel_label();
            if (self->btn_stop_)
                self->btn_stop_->setEnabled(!self->active_runs_.isEmpty() || !self->run_queue_.isEmpty());

            using S = services::NotebookRunResult::Status;
            const int idx = self->find_cell_index(cid);
            if (idx >= 0) {
                QVector<CellOutput> outputs;
                for (const auto& [is_stderr, text] : result.streams) {
                    CellOutput out;
                    out.type = "stream";
                    out.name = is_stderr ? "stderr" : "stdout";
                    out.text = text.endsWith(QLatin1Char('\n')) ? text.chopped(1) : text;
                    outputs.append(out);
                }
                if (result.output_truncated) {
                    CellOutput out;
                    out.type = "stream";
                    out.name = "stderr";
                    out.text = tr("[output truncated — only the first 2 MB was kept]");
                    outputs.append(out);
                }
                if (!result.result_text.isEmpty()) {
                    CellOutput out;
                    out.type = "execute_result";
                    out.text = result.result_text;
                    outputs.append(out);
                }
                if (result.status != S::Ok) {
                    CellOutput out;
                    out.type = "error";
                    if (result.status == S::Failed) {
                        out.error_name = QStringLiteral("KernelError");
                        out.error_value = result.failure;
                    } else {
                        out.error_name = result.error_name.isEmpty() ? QStringLiteral("Error") : result.error_name;
                        out.error_value = result.error_value;
                        out.traceback = result.traceback;
                    }
                    out.text = out.traceback.join(QLatin1Char('\n'));
                    outputs.append(out);
                }
                self->cells_[idx].outputs = outputs;
                self->cells_[idx].execution_count = exec_num;
                self->cells_[idx].running = false;
                if (auto* w = self->find_cell_widget(cid)) {
                    w->set_outputs(outputs, exec_num);
                    w->set_running(false);
                    w->set_elapsed(result.elapsed_ms);
                }
                self->update_status();
                self->update_navigator();
            }

            // RUN ALL stops at the first cell that does not finish cleanly.
            if (result.status == S::Ok) {
                self->run_next_queued();
            } else if (!self->run_queue_.isEmpty()) {
                const int left = static_cast<int>(self->run_queue_.size());
                self->run_queue_.clear();
                self->status_label_->setText(tr("RUN ALL STOPPED — a cell failed; %n cell(s) not run", "", left));
                if (self->btn_stop_)
                    self->btn_stop_->setEnabled(!self->active_runs_.isEmpty());
            }
        });
    active_runs_.insert(cell_id, run_id);
    if (btn_stop_)
        btn_stop_->setEnabled(true);
}

void CodeEditorScreen::run_next_queued() {
    if (run_queue_.isEmpty())
        return;
    const QString next = run_queue_.takeFirst();
    on_run_cell(next);
}

void CodeEditorScreen::on_stop_all() {
    run_queue_.clear();
    const auto runs = active_runs_.values();
    for (const QString& run_id : runs)
        services::NotebookLibraryService::instance().cancel_run(run_id);
    if (!runs.isEmpty())
        status_label_->setText(tr("INTERRUPTING %n CELL(S)…", "", static_cast<int>(runs.size())));
}

void CodeEditorScreen::on_run_and_advance(const QString& cell_id) {
    on_run_cell(cell_id);
    advance_to_next(cell_id);
}

void CodeEditorScreen::on_run_all() {
    // One cell at a time, top to bottom, stopping at the first failure — the
    // order a reader expects and only one Python slot used (cells used to all
    // start at once, in no particular order, taking every shared slot).
    if (!run_queue_.isEmpty() || !active_runs_.isEmpty())
        return; // already running
    run_queue_.clear();
    for (const auto& cell : cells_) {
        if (cell.cell_type == "code" && !cell.source.trimmed().isEmpty())
            run_queue_ << cell.id;
    }
    if (btn_stop_ && !run_queue_.isEmpty())
        btn_stop_->setEnabled(true);
    run_next_queued();
}

// ─────────────────────────────────────────────────────────────────────────────
// File I/O
// ─────────────────────────────────────────────────────────────────────────────

void CodeEditorScreen::on_open_notebook() {
    if (!confirm_discard_changes(tr("Opening another notebook")))
        return;
    const QString path =
        QFileDialog::getOpenFileName(this, tr("Open Notebook"), {}, tr("Fincept Notebooks (*.ipynb);;All Files (*)"));
    if (path.isEmpty())
        return;
    if (!open_notebook_path(path)) {
        QMessageBox::warning(this, tr("Open Notebook"),
                             tr("Could not read a notebook from:\n%1\n\n"
                                "The file must be valid JSON in .ipynb (nbformat) shape.")
                                 .arg(path));
    }
}

// Public entry point: load an .ipynb from disk and switch to the editor view.
bool CodeEditorScreen::open_notebook_path(const QString& path) {
    const bool ok = load_notebook_from_path(path);
    if (ok)
        set_view(1); // surface the editor so the loaded notebook is visible
    return ok;
}

// Parse an .ipynb into the cell model and refresh the editor. Shared by the
// Open dialog, the Library, the File Manager, and session restore.
bool CodeEditorScreen::load_notebook_from_path(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_WARN("CodeEditor", "Cannot open notebook: " + path);
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (doc.isNull())
        return false;

    const QJsonArray json_cells = doc.object()["cells"].toArray();
    const QString spec = doc.object()["metadata"].toObject()["kernelspec"].toObject()["name"].toString();
    if (kernel_combo_)
        kernel_combo_->setCurrentIndex(spec == QLatin1String("fincept-numpy1") ? 1 : 0);

    cells_.clear();
    execution_counter_ = 0;

    for (const auto& jc : json_cells) {
        QJsonObject co = jc.toObject();
        NotebookCell cell;
        cell.id = new_cell_id();
        cell.cell_type = co["cell_type"].toString("code");

        QJsonValue src = co["source"];
        if (src.isArray()) {
            QStringList lines;
            for (const auto& line : src.toArray())
                lines << line.toString();
            cell.source = lines.join("");
        } else {
            cell.source = src.toString();
        }

        QJsonObject cell_metadata = co["metadata"].toObject();
        cell.title = cell_metadata["fincept_title"].toString().trimmed();
        cell.execution_count = co["execution_count"].toInt(0);
        if (cell.cell_type == "code") {
            cell.outputs = nb_outputs_from_json(co["outputs"].toArray());
            // Keep numbering monotonic with what the file already shows.
            execution_counter_ = qMax(execution_counter_, cell.execution_count);
        }
        cells_.append(cell);
    }

    if (cells_.isEmpty()) {
        NotebookCell cell;
        cell.id = new_cell_id();
        cell.cell_type = "code";
        cells_.append(cell);
    }

    notebook_path_ = path;
    selected_cell_id_ = cells_.first().id;
    rebuild_cells();
    clean_fingerprint_ = notebook_fingerprint(); // freshly loaded == no unsaved edits
    update_status();
    update_navigator();
    LOG_INFO("CodeEditor", "Opened notebook: " + path);
    ScreenStateManager::instance().notify_changed(this);
    return true;
}

void CodeEditorScreen::on_save_notebook() {
    for (int i = 0; i < cells_.size(); ++i) {
        auto* cw = find_cell_widget(cells_[i].id);
        if (cw) {
            // cell_data() does not carry outputs — preserve the executed
            // results instead of dropping them on every save.
            const auto outputs = cells_[i].outputs;
            cells_[i] = cw->cell_data();
            cells_[i].outputs = outputs;
        }
    }

    QString path = notebook_path_;
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, tr("Save Notebook"), "notebook.ipynb",
                                            tr("Fincept Notebooks (*.ipynb)"));
        if (path.isEmpty())
            return;
    }

    QJsonArray json_cells;
    for (const auto& cell : cells_) {
        QJsonObject co;
        co["cell_type"] = cell.cell_type;

        QJsonArray src_lines;
        for (const auto& line : cell.source.split('\n'))
            src_lines.append(line + "\n");
        co["source"] = src_lines;
        QJsonObject cell_metadata;
        if (!cell.title.trimmed().isEmpty())
            cell_metadata["fincept_title"] = cell.title.trimmed();
        co["metadata"] = cell_metadata;

        if (cell.cell_type == "code") {
            co["execution_count"] = cell.execution_count > 0 ? QJsonValue(cell.execution_count) : QJsonValue();
            co["outputs"] = nb_outputs_to_json(cell.outputs, cell.execution_count);
        }
        json_cells.append(co);
    }

    QJsonObject root;
    root["cells"] = json_cells;
    root["nbformat"] = 4;
    root["nbformat_minor"] = 5;

    QJsonObject metadata;
    QJsonObject kernelspec;
    const bool numpy1 = kernel_ == QLatin1String("numpy1");
    kernelspec["display_name"] = numpy1 ? "Python 3 (Fincept numpy1)" : "Python 3";
    kernelspec["language"] = "python";
    kernelspec["name"] = numpy1 ? "fincept-numpy1" : "python3";
    metadata["kernelspec"] = kernelspec;
    root["metadata"] = metadata;

    // QSaveFile commits via rename: a failed write can no longer leave the user's
    // only copy of the notebook truncated (library notebooks are edited in place).
    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0 &&
        file.commit()) {
        notebook_path_ = path;
        clean_fingerprint_ = notebook_fingerprint();
        LOG_INFO("CodeEditor", "Saved notebook: " + path);
        services::FileManagerService::instance().import_file(path, "code_editor");
    } else {
        // Silently failing a save is how work gets lost — say what happened.
        LOG_ERROR("CodeEditor", "Save failed: " + path + " — " + file.errorString());
        QMessageBox::warning(this, tr("Save Notebook"),
                             tr("Could not write to:\n%1\n\n%2").arg(path, file.errorString()));
        return;
    }
    update_status();
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

void CodeEditorScreen::rebuild_cells() {
    while (cells_layout_->count() > 1) {
        auto* item = cells_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    for (int i = 0; i < cells_.size(); ++i) {
        auto* cw = new CellWidget(cells_[i], i, cells_container_);
        cw->set_selected(cells_[i].id == selected_cell_id_);

        connect(cw, &CellWidget::run_requested, this, &CodeEditorScreen::on_run_cell);
        connect(cw, &CellWidget::run_and_advance, this, &CodeEditorScreen::on_run_and_advance);
        connect(cw, &CellWidget::delete_requested, this, &CodeEditorScreen::on_delete_cell);
        connect(cw, &CellWidget::move_up_requested, this, &CodeEditorScreen::on_move_cell_up);
        connect(cw, &CellWidget::move_down_requested, this, &CodeEditorScreen::on_move_cell_down);
        connect(cw, &CellWidget::toggle_type_requested, this, &CodeEditorScreen::on_toggle_cell_type);
        connect(cw, &CellWidget::cell_clicked, this, &CodeEditorScreen::on_select_cell);
        connect(cw, &CellWidget::insert_below_requested, this, &CodeEditorScreen::on_insert_below);

        cells_layout_->insertWidget(i, cw);
    }
}

int CodeEditorScreen::find_cell_index(const QString& cell_id) const {
    for (int i = 0; i < cells_.size(); ++i)
        if (cells_[i].id == cell_id)
            return i;
    return -1;
}

CellWidget* CodeEditorScreen::find_cell_widget(const QString& cell_id) const {
    for (int i = 0; i < cells_layout_->count(); ++i) {
        auto* item = cells_layout_->itemAt(i);
        if (!item || !item->widget())
            continue;
        auto* cw = qobject_cast<CellWidget*>(item->widget());
        if (cw && cw->cell_id() == cell_id)
            return cw;
    }
    return nullptr;
}

QString CodeEditorScreen::new_cell_id() const {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void CodeEditorScreen::update_status() {
    int code_count = 0, md_count = 0, executed = 0;
    for (const auto& c : cells_) {
        if (c.cell_type == "code")
            ++code_count;
        else
            ++md_count;
        if (c.execution_count > 0)
            ++executed;
    }

    QString info = tr("CELLS: %1 CODE  %2 MD  |  EXECUTED: %3").arg(code_count).arg(md_count).arg(executed);
    if (!notebook_path_.isEmpty())
        info += "  |  " + QFileInfo(notebook_path_).fileName();
    status_label_->setText(info);
}

void CodeEditorScreen::update_navigator() {
    if (navigator_)
        navigator_->rebuild(cells_, selected_cell_id_);
}

void CodeEditorScreen::refresh_kernel_label() {
    if (!kernel_label_)
        return;
    kernel_label_->setText(kernel_busy_ ? tr("KERNEL: BUSY") : tr("KERNEL: IDLE"));
    kernel_label_->setStyleSheet(QString("color:%1; font-family:%2; font-size:10px; font-weight:600;"
                                         " letter-spacing:0.5px; padding:0 8px;")
                                     .arg(kernel_busy_ ? colors::WARNING() : colors::POSITIVE(), fonts::DATA_FAMILY));
}

// ── Live language switch ─────────────────────────────────────────────────────

void CodeEditorScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void CodeEditorScreen::retranslateUi() {
    // Header
    if (header_title_)
        header_title_->setText(tr("FINCEPT NOTEBOOK"));
    const QStringList tab_labels = {tr("LIBRARY"), tr("EDITOR")};
    for (int i = 0; i < view_btns_.size() && i < tab_labels.size(); ++i)
        view_btns_[i]->setText(tab_labels[i]);
    if (lib_search_input_)
        lib_search_input_->setPlaceholderText(tr("Search notebooks..."));
    if (header_new_btn_)
        header_new_btn_->setText(tr("＋  NEW NOTEBOOK"));

    // Toolbar
    if (btn_new_)
        btn_new_->setText(tr("NEW"));
    if (btn_open_)
        btn_open_->setText(tr("OPEN"));
    if (btn_save_)
        btn_save_->setText(tr("SAVE"));
    if (btn_add_cell_)
        btn_add_cell_->setText(tr("+ CELL"));
    if (btn_clear_out_)
        btn_clear_out_->setText(tr("CLEAR OUT"));
    if (btn_run_all_)
        btn_run_all_->setText(tr("▶  RUN ALL"));
    if (btn_sidebar_)
        btn_sidebar_->setText(tr("SIDEBAR"));
    if (btn_stop_)
        btn_stop_->setText(tr("■ STOP"));
    refresh_kernel_label();
    if (lib_toolbar_lbl_)
        lib_toolbar_lbl_->setText(tr("FINCEPT NOTEBOOK LIBRARY — curated finance, economics, trading, "
                                     "investing, portfolio & quant notebooks"));

    // Status bar
    if (shortcuts_label_)
        shortcuts_label_->setText(
            tr("Ctrl+Enter: RUN  |  Shift+Enter: RUN & NEXT  |  Tab: 4 SPACES  |  Ctrl+S: SAVE  |  "
                   "Ctrl+Shift+Enter: RUN ALL  |  Esc: STOP"));
    update_status(); // re-renders the cell-count summary in the new language

    // Cell widgets carry their own translatable chrome — rebuild so they
    // re-create with the new language. rebuild_cells() renders from cells_, so
    // pull the live editor text back first; without this, switching language
    // mid-edit silently reverted every cell to its last-committed source.
    sync_cells_from_widgets();
    rebuild_cells();
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap CodeEditorScreen::save_state() const {
    return {{"notebook_path", notebook_path_}, {"kernel", kernel_}, {"timeout_s", timeout_s_}};
}

void CodeEditorScreen::restore_state(const QVariantMap& state) {
    if (timeout_combo_) {
        const int idx = timeout_combo_->findData(state.value("timeout_s", timeout_s_).toInt());
        if (idx >= 0)
            timeout_combo_->setCurrentIndex(idx);
    }
    if (kernel_combo_ && state.contains("kernel")) {
        const int idx = kernel_combo_->findData(state.value("kernel").toString());
        if (idx >= 0)
            kernel_combo_->setCurrentIndex(idx);
    }
    const QString path = state.value("notebook_path").toString();
    if (!path.isEmpty() && notebook_path_ != path && QFileInfo::exists(path)) {
        // Fully reload the notebook (cells + outputs metadata), not just the label.
        load_notebook_from_path(path);
    }
}

} // namespace fincept::screens
