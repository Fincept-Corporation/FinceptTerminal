// src/screens/code_editor/CodeEditorScreen_Navigator.cpp — the rail's OUTLINE and
// VARIABLES pages. See CodeEditorScreen_Navigator.h. Styled by the screen sheet.

#include "screens/code_editor/CodeEditorScreen_Navigator.h"

#include "core/keys/KeyConfigManager.h"
#include "screens/code_editor/CodeEditorScreen_Cells.h"
#include "screens/code_editor/NotebookStyle.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/widgets/EmptyState.h"

#include <QAction>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace fincept::screens {

namespace colors = ui::colors;

namespace {

constexpr int kRoleCellId = Qt::UserRole + 1;

QString state_glyph(int state, int exec_count) {
    switch (static_cast<CellWidget::RunState>(state)) {
        case CellWidget::RunState::Queued:
            return QStringLiteral("…");
        case CellWidget::RunState::Running:
            return QStringLiteral("▶");
        case CellWidget::RunState::Done:
            return QStringLiteral("✓");
        case CellWidget::RunState::Error:
            return QStringLiteral("✕");
        case CellWidget::RunState::Cancelled:
            return QStringLiteral("■");
        case CellWidget::RunState::Idle:
            break;
    }
    return exec_count > 0 ? QStringLiteral("✓") : QStringLiteral("·");
}

QColor state_color(int state, int exec_count) {
    switch (static_cast<CellWidget::RunState>(state)) {
        case CellWidget::RunState::Error:
            return QColor(colors::DANGER());
        case CellWidget::RunState::Queued:
        case CellWidget::RunState::Running:
        case CellWidget::RunState::Cancelled:
            return QColor(colors::WARNING());
        case CellWidget::RunState::Done:
            return QColor(colors::TEXT_SECONDARY());
        case CellWidget::RunState::Idle:
            break;
    }
    return QColor(exec_count > 0 ? colors::TEXT_SECONDARY() : colors::TEXT_TERTIARY());
}

QTreeWidget* make_tree(QWidget* parent) {
    auto* t = new QTreeWidget(parent);
    t->setObjectName(QStringLiteral("nbTree"));
    t->setUniformRowHeights(true);
    t->setTextElideMode(Qt::ElideRight);
    t->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    return t;
}

} // namespace

// ═════════════════════════════════════════════════════════════════════════════
// NotebookOutlinePanel

NotebookOutlinePanel::NotebookOutlinePanel(QWidget* parent) : QWidget(parent) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 4, 0, 0);
    v->setSpacing(0);
    stack_ = new QStackedWidget(this);
    empty_ = new ui::EmptyState(stack_);
    empty_->set_compact(true);
    stack_->addWidget(empty_);
    tree_ = make_tree(stack_);
    tree_->setHeaderHidden(true);
    tree_->setIndentation(12);
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    tree_->setAccessibleName(tr("Notebook outline"));
    stack_->addWidget(tree_);
    v->addWidget(stack_, 1);

    connect(tree_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* cur, QTreeWidgetItem*) {
        if (updating_ || !cur)
            return;
        const QString id = cur->data(0, kRoleCellId).toString();
        if (!id.isEmpty())
            emit cell_activated(id);
    });
    connect(tree_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* it, int) {
        const QString id = it ? it->data(0, kRoleCellId).toString() : QString();
        if (!id.isEmpty())
            emit cell_activated(id); // also when it is already the current row
    });
    connect(tree_, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QTreeWidgetItem* it = tree_->itemAt(pos);
        if (!it)
            return;
        const QString id = it->data(0, kRoleCellId).toString();
        QMenu menu(this);
        nb_set_sheet(&menu, nb_menu_sheet());
        QAction* rename = menu.addAction(tr("Rename cell…\tF2"));
        if (menu.exec(tree_->viewport()->mapToGlobal(pos)) == rename)
            emit rename_requested(id);
    });
    auto* rename = new QAction(this);
    rename->setShortcut(KeyConfigManager::instance().key(KeyAction::RenameCell));
    rename->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    tree_->addAction(rename);
    connect(rename, &QAction::triggered, this, [this]() {
        if (QTreeWidgetItem* it = tree_->currentItem())
            emit rename_requested(it->data(0, kRoleCellId).toString());
    });
    rebuild();
}

void NotebookOutlinePanel::set_entries(const QVector<Entry>& entries, const QString& selected_id) {
    entries_ = entries;
    selected_ = selected_id;
    rebuild();
}

void NotebookOutlinePanel::rebuild() {
    if (entries_.isEmpty()) {
        empty_->set_content(QStringLiteral("≡"), tr("No outline yet"),
                            tr("Headings of text cells and every code cell are listed here once a notebook is open."));
        stack_->setCurrentWidget(empty_);
        return;
    }
    updating_ = true;
    tree_->setUpdatesEnabled(false);
    tree_->clear();
    QVector<QPair<int, QTreeWidgetItem*>> parents; // (level, item) of open headings
    QTreeWidgetItem* select = nullptr;
    for (const Entry& e : entries_) {
        QTreeWidgetItem* item = nullptr;
        if (e.level > 0) {
            while (!parents.isEmpty() && parents.last().first >= e.level)
                parents.removeLast();
            item = parents.isEmpty() ? new QTreeWidgetItem(tree_) : new QTreeWidgetItem(parents.last().second);
            item->setText(0, e.text);
            QFont f = item->font(0);
            f.setBold(e.level <= 2);
            item->setFont(0, f);
            item->setForeground(0, QColor(colors::TEXT_PRIMARY()));
            parents.append({e.level, item});
        } else {
            item = parents.isEmpty() ? new QTreeWidgetItem(tree_) : new QTreeWidgetItem(parents.last().second);
            const QString count = e.exec_count > 0 ? QStringLiteral("[%1]").arg(e.exec_count) : QStringLiteral("[ ]");
            const QString name = e.text.isEmpty() ? tr("(empty cell)") : e.text;
            item->setText(0, QStringLiteral("%1  %2  %3").arg(state_glyph(e.run_state, e.exec_count), count, name));
            item->setForeground(0, state_color(e.run_state, e.exec_count));
        }
        item->setData(0, kRoleCellId, e.cell_id);
        item->setToolTip(0, e.text);
        if (!select && e.cell_id == selected_)
            select = item;
    }
    tree_->expandAll();
    if (select)
        tree_->setCurrentItem(select);
    tree_->setUpdatesEnabled(true);
    stack_->setCurrentWidget(tree_);
    updating_ = false;
}

void NotebookOutlinePanel::set_selected(const QString& cell_id) {
    if (selected_ == cell_id)
        return;
    selected_ = cell_id;
    updating_ = true;
    for (QTreeWidgetItemIterator it(tree_); *it; ++it) {
        if ((*it)->data(0, kRoleCellId).toString() == cell_id) {
            tree_->setCurrentItem(*it);
            tree_->scrollToItem(*it);
            break;
        }
    }
    updating_ = false;
}

void NotebookOutlinePanel::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        rebuild();
    QWidget::changeEvent(e);
}

// ═════════════════════════════════════════════════════════════════════════════
// NotebookVariablesPanel

NotebookVariablesPanel::NotebookVariablesPanel(QWidget* parent) : QWidget(parent) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 6, 0, 0);
    v->setSpacing(4);
    filter_ = new QLineEdit(this);
    filter_->setObjectName(QStringLiteral("nbSearch"));
    filter_->setClearButtonEnabled(true);
    connect(filter_, &QLineEdit::textChanged, this, &NotebookVariablesPanel::apply_filter);
    auto* fr = new QHBoxLayout;
    fr->setContentsMargins(8, 0, 8, 0);
    fr->addWidget(filter_);
    v->addLayout(fr);

    stack_ = new QStackedWidget(this);
    empty_ = new ui::EmptyState(stack_);
    empty_->set_compact(true);
    stack_->addWidget(empty_);
    tree_ = make_tree(stack_);
    tree_->setColumnCount(4);
    tree_->setRootIsDecorated(false);
    tree_->setSortingEnabled(true);
    tree_->sortByColumn(0, Qt::AscendingOrder);
    tree_->header()->setStretchLastSection(true);
    tree_->header()->setSectionResizeMode(QHeaderView::Interactive);
    const int avg = QFontMetrics(ui::fonts::font(-2)).averageCharWidth();
    tree_->header()->resizeSection(0, avg * 12 + 12);
    tree_->header()->resizeSection(1, avg * 10 + 12);
    tree_->header()->resizeSection(2, avg * 9 + 12);
    connect(tree_, &QTreeWidget::itemDoubleClicked, this,
            [this](QTreeWidgetItem* it, int) { emit preview_requested(it->text(0)); });
    stack_->addWidget(tree_);
    v->addWidget(stack_, 1);

    footer_ = new QLabel(this);
    footer_->setObjectName(QStringLiteral("nbDim"));
    footer_->setContentsMargins(8, 0, 8, 4);
    v->addWidget(footer_);
    retranslate();
    update_state();
}

void NotebookVariablesPanel::set_kernel_alive(bool alive, const QString& why) {
    alive_ = alive;
    why_ = why;
    if (!alive) {
        items_ = QJsonArray();
        total_ = 0;
        tree_->clear();
    }
    update_state();
}

void NotebookVariablesPanel::set_variables(const QJsonArray& items, int total) {
    items_ = items;
    total_ = total;
    if (!items.isEmpty())
        alive_ = true;
    tree_->setUpdatesEnabled(false);
    tree_->setSortingEnabled(false);
    tree_->clear();
    for (const QJsonValue& v : items) {
        const QJsonObject o = v.toObject();
        auto* it = new QTreeWidgetItem(tree_, {o.value(QStringLiteral("name")).toString(),
                                               o.value(QStringLiteral("type")).toString(),
                                               o.value(QStringLiteral("shape")).toString(),
                                               o.value(QStringLiteral("preview")).toString()});
        it->setToolTip(3, o.value(QStringLiteral("preview")).toString());
        it->setToolTip(0, tr("Double-click to see the value in the inspector"));
        it->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
    }
    tree_->setSortingEnabled(true);
    tree_->setUpdatesEnabled(true);
    apply_filter();
    update_state();
}

void NotebookVariablesPanel::apply_filter() {
    const QString needle = filter_->text().trimmed();
    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
        QTreeWidgetItem* it = tree_->topLevelItem(i);
        it->setHidden(!needle.isEmpty() && !it->text(0).contains(needle, Qt::CaseInsensitive) &&
                      !it->text(1).contains(needle, Qt::CaseInsensitive));
    }
}

void NotebookVariablesPanel::update_state() {
    if (!alive_) {
        empty_->set_content(QStringLiteral("x="), tr("No variables yet"),
                            why_.isEmpty() ? tr("Variables appear here after you run a cell. The kernel keeps them "
                                                "until it is restarted or another notebook is opened.")
                                           : why_);
        stack_->setCurrentWidget(empty_);
        footer_->clear();
        return;
    }
    if (items_.isEmpty()) {
        empty_->set_content(QStringLiteral("x="), tr("The kernel has no variables"),
                            tr("Assign something in a cell — prices = …, df = … — and run it."));
        stack_->setCurrentWidget(empty_);
        footer_->clear();
        return;
    }
    stack_->setCurrentWidget(tree_);
    const QString n = ui::formatting::number(static_cast<double>(total_), 0);
    footer_->setText(total_ > items_.size()
                         ? tr("Showing %1 of %2 variables").arg(ui::formatting::number(static_cast<double>(items_.size()), 0), n)
                         : tr("%n variable(s) in the kernel", "", total_));
}

void NotebookVariablesPanel::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange) {
        retranslate();
        update_state();
    }
    QWidget::changeEvent(e);
}

void NotebookVariablesPanel::retranslate() {
    tree_->setHeaderLabels({tr("NAME"), tr("TYPE"), tr("SHAPE"), tr("VALUE")});
    filter_->setPlaceholderText(tr("Filter variables…"));
    filter_->setAccessibleName(tr("Filter variables"));
    tree_->setAccessibleName(tr("Kernel variables"));
}

} // namespace fincept::screens
