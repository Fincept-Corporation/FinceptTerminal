// NotebookCommandBar.cpp — see NotebookCommandBar.h. Styled by the screen sheet.

#include "screens/code_editor/NotebookCommandBar.h"

#include "screens/code_editor/NotebookStyle.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QAction>
#include <QActionGroup>
#include <QCursor>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QResizeEvent>
#include <QToolButton>

namespace fincept::screens {

namespace {

constexpr int kFoldSecondaryBelow = 1360; ///< labels of file / restart / insert buttons fold to icons
constexpr int kFoldRunBelow = 1080;       ///< ... and of RUN ALL / STOP

QWidget* vsep(QWidget* parent) {
    auto* w = new QWidget(parent);
    w->setObjectName(QStringLiteral("nbVSep"));
    w->setAttribute(Qt::WA_StyledBackground, true);
    w->setFixedSize(1, 18);
    return w;
}

QString fmt_minutes(int seconds) {
    return NotebookCommandBar::tr("%1 min").arg(seconds / 60);
}

} // namespace

NotebookCommandBar::NotebookCommandBar(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("nbCmdBar"));
    setAttribute(Qt::WA_StyledBackground, true);
    setMinimumHeight(ui::metrics::height(38));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    row_ = new QHBoxLayout(this);
    row_->setContentsMargins(12, 3, 8, 3);
    row_->setSpacing(4);

    name_ = new QLabel(this);
    name_->setObjectName(QStringLiteral("nbName"));
    name_->setTextFormat(Qt::PlainText);
    row_->addWidget(name_);
    dirty_ = new QLabel(QStringLiteral("●"), this);
    dirty_->setProperty("tone", QStringLiteral("warn"));
    dirty_->setVisible(false);
    row_->addWidget(dirty_);
    row_->addSpacing(6);
    row_->addWidget(vsep(this));

    new_ = make("nbBtn", int(NbGlyph::NewDoc), int(NbTone::Normal), Cmd::New);
    open_btn_ = make("nbBtn", int(NbGlyph::Open), int(NbTone::Normal), Cmd::Open);
    save_ = make("nbBtn", int(NbGlyph::Save), int(NbTone::Normal), Cmd::Save);
    auto* export_btn = make("nbBtn", int(NbGlyph::Export), int(NbTone::Normal), Cmd::SaveAs);
    export_btn->setObjectName(QStringLiteral("nbBtn"));
    export_btn->disconnect(this);
    connect(export_btn, &QToolButton::clicked, this, &NotebookCommandBar::show_save_menu);
    export_btn->setProperty("nb_role", QStringLiteral("export"));
    row_->addWidget(vsep(this));

    run_ = make("nbRun", int(NbGlyph::Play), int(NbTone::OnAccent), Cmd::RunCell);
    run_->setProperty("fincept_agent_risk", "code");
    run_all_ = make("nbBtn", int(NbGlyph::PlayAll), int(NbTone::Normal), Cmd::RunAll);
    run_all_->setProperty("fincept_agent_risk", "code");
    auto* run_more = make("nbBtn", int(NbGlyph::ChevronDown), int(NbTone::Normal), Cmd::RunAll);
    run_more->disconnect(this);
    run_more->setToolButtonStyle(Qt::ToolButtonIconOnly);
    run_more->setProperty("nb_role", QStringLiteral("run_more"));
    connect(run_more, &QToolButton::clicked, this, [this, run_more]() {
        QMenu menu(this);
        nb_set_sheet(&menu, nb_menu_sheet());
        const struct {
            QString text;
            Cmd cmd;
        } items[] = {{tr("Run selected cell\tCtrl+Enter"), Cmd::RunCell},
                     {tr("Run all cells\tCtrl+Shift+Enter"), Cmd::RunAll},
                     {tr("Run all above the selected cell"), Cmd::RunAbove},
                     {tr("Run the selected cell and all below"), Cmd::RunBelow},
                     {tr("Restart the kernel and run all"), Cmd::RestartRunAll}};
        for (const auto& it : items) {
            QAction* a = menu.addAction(it.text);
            a->setProperty("fincept_agent_risk", "code"); // every item runs cells
            a->setEnabled(is_open_);
            const Cmd cmd = it.cmd;
            connect(a, &QAction::triggered, this, [this, cmd]() { emit command(cmd); });
        }
        menu.exec(run_more->mapToGlobal(QPoint(0, run_more->height())));
    });
    stop_ = make("nbBtn", int(NbGlyph::Stop), int(NbTone::Danger), Cmd::Interrupt);
    stop_->setEnabled(false);
    restart_ = make("nbBtn", int(NbGlyph::Restart), int(NbTone::Normal), Cmd::Restart);
    restart_->setProperty("fincept_agent_risk", "code");
    row_->addWidget(vsep(this));
    insert_ = make("nbBtn", int(NbGlyph::Plus), int(NbTone::Normal), Cmd::InsertCode);
    insert_->disconnect(this);
    connect(insert_, &QToolButton::clicked, this, &NotebookCommandBar::show_insert_menu);

    row_->addStretch(1);

    find_ = new QLineEdit(this);
    find_->setObjectName(QStringLiteral("nbSearch"));
    find_->setClearButtonEnabled(true);
    find_->setMinimumWidth(ui::metrics::width(140));
    find_->setMaximumWidth(ui::metrics::width(260));
    find_->installEventFilter(this);
    connect(find_, &QLineEdit::textChanged, this, &NotebookCommandBar::find_changed);
    row_->addWidget(find_, 1);
    find_status_ = new QLabel(this);
    find_status_->setObjectName(QStringLiteral("nbDim"));
    find_status_->setVisible(false);
    row_->addWidget(find_status_);
    row_->addSpacing(4);

    kernel_ = new QToolButton(this);
    kernel_->setObjectName(QStringLiteral("nbKernel"));
    kernel_->setCursor(Qt::PointingHandCursor);
    kernel_->setProperty("tone", QStringLiteral("dim"));
    connect(kernel_, &QToolButton::clicked, this, &NotebookCommandBar::show_kernel_menu);
    row_->addWidget(kernel_);

    rail_ = make("nbBtn", int(NbGlyph::RailLeft), int(NbTone::Normal), Cmd::ToggleRail);
    rail_->setCheckable(true);
    rail_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    inspector_ = make("nbBtn", int(NbGlyph::RailRight), int(NbTone::Normal), Cmd::ToggleInspector);
    inspector_->setCheckable(true);
    inspector_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    more_ = make("nbBtn", int(NbGlyph::Dots), int(NbTone::Normal), Cmd::Shortcuts);
    more_->disconnect(this);
    more_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    connect(more_, &QToolButton::clicked, this, &NotebookCommandBar::show_more_menu);

    retranslate();
    set_notebook(QString(), QString(), false, false);
}

QToolButton* NotebookCommandBar::make(const char* object_name, int glyph, int tone, Cmd cmd) {
    auto* b = new QToolButton(this);
    b->setObjectName(QString::fromLatin1(object_name));
    b->setIcon(nb_icon(static_cast<NbGlyph>(glyph), static_cast<NbTone>(tone)));
    b->setIconSize(QSize(14, 14));
    b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    b->setCursor(Qt::PointingHandCursor);
    b->setMinimumHeight(ui::metrics::height(26));
    connect(b, &QToolButton::clicked, this, [this, cmd]() { emit command(cmd); });
    row_->addWidget(b);
    return b;
}

void NotebookCommandBar::set_notebook(const QString& name, const QString& tooltip, bool dirty, bool open) {
    is_open_ = open;
    const QString shown = open ? name : tr("NO NOTEBOOK OPEN");
    name_->setProperty("full_text", shown);
    const int max_w = width() < kFoldRunBelow ? ui::metrics::width(140) : ui::metrics::width(260);
    const QString elided = name_->fontMetrics().elidedText(shown, Qt::ElideMiddle, max_w);
    if (name_->text() != elided)
        name_->setText(elided);
    name_->setToolTip(tooltip);
    dirty_->setVisible(open && dirty);
    dirty_->setToolTip(tr("Unsaved changes"));
    dirty_->setAccessibleName(tr("Unsaved changes"));
    for (QToolButton* b : {save_, run_, run_all_, restart_, insert_})
        b->setEnabled(open);
    find_->setEnabled(open);
    if (!open)
        stop_->setEnabled(false);
}

void NotebookCommandBar::set_kernel(const QString& text, const QString& short_text, const QString& tone,
                                    const QString& tooltip) {
    kernel_text_ = text;
    kernel_short_ = short_text;
    const QString shown = collapse_level_ >= 1 ? short_text : text;
    if (kernel_->text() != shown)
        kernel_->setText(shown);
    kernel_->setToolTip(tooltip);
    kernel_->setAccessibleName(tr("Kernel: %1").arg(text));
    nb_set_prop(kernel_, "tone", tone);
}

void NotebookCommandBar::set_busy(bool busy) {
    busy_ = busy;
    stop_->setEnabled(busy && is_open_);
    nb_set_prop(stop_, "tone", busy ? QStringLiteral("danger") : QString());
}

void NotebookCommandBar::set_environment(const QString& env) {
    env_ = env;
}

void NotebookCommandBar::set_timeout(int seconds) {
    timeout_s_ = seconds;
}

void NotebookCommandBar::set_rail_checked(bool on) {
    rail_->setChecked(on);
}

void NotebookCommandBar::set_inspector_checked(bool on) {
    inspector_->setChecked(on);
}

void NotebookCommandBar::set_find_status(const QString& text) {
    find_status_->setText(text);
    find_status_->setVisible(!text.isEmpty());
}

bool NotebookCommandBar::eventFilter(QObject* watched, QEvent* event) {
    if (watched == find_ && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            emit find_next(ke->modifiers().testFlag(Qt::ShiftModifier));
            return true;
        }
        if (ke->key() == Qt::Key_Escape) {
            find_->clear();
            emit find_closed();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void NotebookCommandBar::show_save_menu() {
    auto* btn = qobject_cast<QToolButton*>(sender());
    QMenu menu(this);
    nb_set_sheet(&menu, nb_menu_sheet());
    auto add = [&](const QString& text, Cmd cmd, bool enabled) {
        QAction* a = menu.addAction(text);
        a->setEnabled(enabled);
        connect(a, &QAction::triggered, this, [this, cmd]() { emit command(cmd); });
    };
    add(tr("Save as…\tCtrl+Shift+S"), Cmd::SaveAs, is_open_);
    menu.addSeparator();
    add(tr("Export as Python script (.py)…"), Cmd::ExportPy, is_open_);
    add(tr("Export as HTML page (with outputs)…"), Cmd::ExportHtml, is_open_);
    menu.addSeparator();
    add(tr("Close notebook"), Cmd::Close, is_open_);
    menu.exec(btn ? btn->mapToGlobal(QPoint(0, btn->height())) : QCursor::pos());
}

void NotebookCommandBar::show_insert_menu() {
    QMenu menu(this);
    nb_set_sheet(&menu, nb_menu_sheet());
    auto add = [&](const QString& text, Cmd cmd) {
        connect(menu.addAction(text), &QAction::triggered, this, [this, cmd]() { emit command(cmd); });
    };
    add(tr("Code cell below\tB"), Cmd::InsertCode);
    add(tr("Text (markdown) cell below"), Cmd::InsertText);
    menu.addSection(tr("Data snippets"));
    add(tr("Load prices for a symbol…"), Cmd::InsertPrices);
    add(tr("Plot the price series"), Cmd::InsertPlot);
    add(tr("Returns & risk summary table"), Cmd::InsertReturns);
    add(tr("Call a data connector"), Cmd::InsertConnector);
    add(tr("Read a CSV file"), Cmd::InsertCsv);
    menu.exec(insert_->mapToGlobal(QPoint(0, insert_->height())));
}

void NotebookCommandBar::show_kernel_menu() {
    QMenu menu(this);
    nb_set_sheet(&menu, nb_menu_sheet());
    menu.addSection(tr("Python environment"));
    auto* group = new QActionGroup(&menu);
    const struct {
        QString id;
        QString text;
        QString tip;
    } envs[] = {{QStringLiteral("numpy2"), tr("NUMPY2  ·  pandas, numpy 2, scipy, statsmodels, yfinance"), QString()},
                {QStringLiteral("numpy1"), tr("NUMPY1  ·  vectorbt, backtesting.py, bt, ffn, PyPortfolioOpt"), QString()}};
    for (const auto& e : envs) {
        QAction* a = menu.addAction(e.text);
        a->setCheckable(true);
        a->setChecked(env_ == e.id);
        group->addAction(a);
        const QString id = e.id;
        connect(a, &QAction::triggered, this, [this, id]() { emit environment_selected(id); });
    }
    QMenu* limit = menu.addMenu(tr("Time limit per cell (%1)").arg(fmt_minutes(timeout_s_)));
    nb_set_sheet(limit, nb_menu_sheet());
    for (const int minutes : {1, 5, 15, 30, 60, 120}) {
        QAction* a = limit->addAction(fmt_minutes(minutes * 60));
        a->setCheckable(true);
        a->setChecked(timeout_s_ == minutes * 60);
        connect(a, &QAction::triggered, this, [this, minutes]() { emit timeout_selected(minutes * 60); });
    }
    menu.addSeparator();
    auto add = [&](const QString& text, Cmd cmd, bool enabled) {
        QAction* a = menu.addAction(text);
        a->setEnabled(enabled);
        connect(a, &QAction::triggered, this, [this, cmd]() { emit command(cmd); });
    };
    add(tr("Interrupt the running cell\tI, I"), Cmd::Interrupt, busy_);
    add(tr("Restart the kernel (clears variables)\t0, 0"), Cmd::Restart, is_open_);
    menu.actions().constLast()->setProperty("fincept_agent_risk", "code");
    add(tr("Restart and run all"), Cmd::RestartRunAll, is_open_);
    menu.actions().constLast()->setProperty("fincept_agent_risk", "code");
    add(tr("Shut the kernel down"), Cmd::Shutdown, is_open_);
    menu.actions().constLast()->setProperty("fincept_agent_risk", "destructive"); // drops the kernel's state
    menu.exec(kernel_->mapToGlobal(QPoint(0, kernel_->height())));
}

void NotebookCommandBar::show_more_menu() {
    QMenu menu(this);
    nb_set_sheet(&menu, nb_menu_sheet());
    QAction* clear = menu.addAction(tr("Clear all outputs"));
    clear->setEnabled(is_open_);
    connect(clear, &QAction::triggered, this, [this]() { emit command(Cmd::ClearOutputs); });
    menu.addSeparator();
    QAction* lines = menu.addAction(tr("Line numbers\tL"));
    lines->setCheckable(true);
    lines->setChecked(line_numbers_);
    connect(lines, &QAction::triggered, this, [this]() { emit command(Cmd::ToggleLineNumbers); });
    QAction* autosave = menu.addAction(tr("Autosave every minute"));
    autosave->setCheckable(true);
    autosave->setChecked(autosave_);
    connect(autosave, &QAction::triggered, this, [this]() { emit command(Cmd::ToggleAutosave); });
    menu.addSeparator();
    connect(menu.addAction(tr("Keyboard shortcuts")), &QAction::triggered, this,
            [this]() { emit command(Cmd::Shortcuts); });
    menu.exec(more_->mapToGlobal(QPoint(0, more_->height())));
}

void NotebookCommandBar::apply_collapse() {
    const int w = width();
    const int level = w <= 0 ? 0 : w < kFoldRunBelow ? 2 : w < kFoldSecondaryBelow ? 1 : 0;
    if (level == collapse_level_)
        return;
    collapse_level_ = level;
    QList<QToolButton*> secondary{new_, open_btn_, save_, restart_, insert_};
    for (QToolButton* b : findChildren<QToolButton*>())
        if (b->property("nb_role").toString() == QLatin1String("export"))
            secondary << b;
    for (QToolButton* b : secondary)
        b->setToolButtonStyle(level >= 1 ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon);
    for (QToolButton* b : {run_all_, stop_})
        b->setToolButtonStyle(level >= 2 ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon);
    kernel_->setText(level >= 1 ? kernel_short_ : kernel_text_);
    set_notebook(name_->property("full_text").toString(), name_->toolTip(), dirty_->isVisible(), is_open_);
}

QSize NotebookCommandBar::minimumSizeHint() const {
    return {0, QWidget::minimumSizeHint().height()};
}

void NotebookCommandBar::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    apply_collapse(); // label folding happens here only, never in a theme handler
}

void NotebookCommandBar::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

void NotebookCommandBar::retranslate() {
    const struct {
        QToolButton* b;
        QString text;
        QString tip;
    } items[] = {
        {new_, tr("NEW"), tr("New notebook (Ctrl+N)")},
        {open_btn_, tr("OPEN"), tr("Open a notebook file (Ctrl+O)")},
        {save_, tr("SAVE"), tr("Save (Ctrl+S)")},
        {run_, tr("RUN"), tr("Run the selected cell (Ctrl+Enter; Shift+Enter runs and moves on)")},
        {run_all_, tr("RUN ALL"), tr("Run every code cell, top to bottom, stopping at the first error (Ctrl+Shift+Enter)")},
        {stop_, tr("STOP"), tr("Interrupt the running cell and cancel the queued ones (I, I)")},
        {restart_, tr("RESTART"), tr("Restart the kernel: a fresh Python session, variables cleared (0, 0)")},
        {insert_, tr("INSERT"), tr("Insert a code or text cell, or a ready-made data snippet")},
    };
    for (const auto& it : items) {
        it.b->setText(it.text);
        it.b->setToolTip(it.tip);
        it.b->setAccessibleName(it.tip);
    }
    for (QToolButton* b : findChildren<QToolButton*>()) {
        const QString role = b->property("nb_role").toString();
        if (role == QLatin1String("export")) {
            b->setText(tr("EXPORT"));
            b->setToolTip(tr("Save as, export as a Python script or an HTML page, close"));
            b->setAccessibleName(b->toolTip());
        } else if (role == QLatin1String("run_more")) {
            b->setToolTip(tr("More ways to run: above, below, restart and run all"));
            b->setAccessibleName(b->toolTip());
        }
    }
    rail_->setToolTip(tr("Show or hide the library / outline / variables rail (Ctrl+B)"));
    rail_->setAccessibleName(rail_->toolTip());
    inspector_->setToolTip(tr("Show or hide the inspector: help, large output view, run history (Ctrl+Shift+I)"));
    inspector_->setAccessibleName(inspector_->toolTip());
    more_->setToolTip(tr("More: clear outputs, line numbers, autosave, keyboard shortcuts"));
    more_->setAccessibleName(more_->toolTip());
    find_->setPlaceholderText(tr("Find in notebook  (Ctrl+F)"));
    find_->setAccessibleName(tr("Find in notebook"));
    if (!is_open_)
        set_notebook(QString(), QString(), false, false);
}

} // namespace fincept::screens
