// src/screens/code_editor/CodeEditorScreen_Navigator.cpp
//
// CellNavigator widget — the side panel listing cells with selection sync.

#include "screens/code_editor/CodeEditorScreen.h"

#include "core/keys/KeyConfigManager.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "python/PythonRunner.h"
#include "services/file_manager/FileManagerService.h"
#include "ui/theme/Theme.h"

#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QToolButton>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;

namespace {
// Build a clean, human-readable cell name from raw cell source: take the first
// non-empty line and strip markdown header/list/quote markers, code-fence
// backticks, and any leading emoji/symbol — so a banner like
// "# 📘 Fincept Notebook — Time Value of Money" shows as
// "Fincept Notebook — Time Value of Money" and "# Setup" shows as "Setup".
QString clean_cell_name(const QString& source) {
    QString s;
    for (const QString& ln : source.split('\n')) {
        const QString t = ln.trimmed();
        if (!t.isEmpty()) {
            s = t;
            break;
        }
    }
    if (s.isEmpty())
        return {};
    s.remove('`');
    // Strip a leading run of markdown block markers + whitespace.
    int i = 0;
    while (i < s.size() && QStringLiteral("#>*-=+ \t").contains(s.at(i)))
        ++i;
    s = s.mid(i);
    // Drop a leading emoji / symbol run so "📘 Title" becomes "Title".
    while (!s.isEmpty() && !s.at(0).isLetterOrNumber() && s.at(0) != '(' && s.at(0) != '"' && s.at(0) != '\'')
        s.remove(0, 1);
    return s.simplified();
}
} // namespace

CellNavigator::CellNavigator(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* header = new QWidget(this);
    header->setFixedHeight(32);
    header->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(colors::BG_RAISED(), colors::BORDER_DIM()));
    auto* header_layout = new QHBoxLayout(header);
    header_layout->setContentsMargins(10, 0, 10, 0);

    header_title_ = new QLabel(tr("CELLS"), header);
    header_title_->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:%3px; font-weight:700; letter-spacing:1px;")
            .arg(colors::AMBER(), fonts::DATA_FAMILY)
            .arg(fonts::TINY));
    header_layout->addWidget(header_title_);
    layout->addWidget(header);

    list_ = new QListWidget(this);
    list_->setStyleSheet(
        QString("QListWidget { background:%1; border:none; outline:none;"
                " font-family:%2, 'Menlo', 'SF Mono', monospace; font-size:%3px; }"
                "QListWidget::item { color:%4; padding:7px 10px;"
                " border-bottom:1px solid %5; }"
                "QListWidget::item:selected { background:%6; color:%7;"
                " border-left:2px solid %7; }"
                "QListWidget::item:hover { background:%8; }")
            .arg(colors::BG_SURFACE(), fonts::DATA_FAMILY)
            .arg(fonts::TINY)
            .arg(colors::TEXT_SECONDARY(), colors::BORDER_DIM(), colors::BG_HOVER(), colors::AMBER(), colors::BG_HOVER()));
    list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list_->setTextElideMode(Qt::ElideRight);
    list_->setWordWrap(false);
    list_->setUniformItemSizes(true);
    list_->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(list_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0 && row < list_->count()) {
            emit cell_selected(list_->item(row)->data(Qt::UserRole).toString());
        }
    });

    connect(list_, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        auto* item = list_->itemAt(pos);
        if (!item)
            return;

        QMenu menu(this);
        auto* rename_action = menu.addAction(tr("Rename Cell"));
        QAction* chosen = menu.exec(list_->viewport()->mapToGlobal(pos));
        if (chosen == rename_action) {
            emit rename_requested(item->data(Qt::UserRole).toString());
        }
    });

    auto* rename_shortcut = KeyConfigManager::instance().action(KeyAction::RenameCell);
    rename_shortcut->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    list_->addAction(rename_shortcut);
    connect(rename_shortcut, &QAction::triggered, this, [this]() {
        auto* item = list_->currentItem();
        if (!item)
            return;
        emit rename_requested(item->data(Qt::UserRole).toString());
    });

    layout->addWidget(list_, 1);
}

void CellNavigator::rebuild(const QVector<NotebookCell>& cells, const QString& selected_id) {
    // Cache so retranslateUi can re-render after a language change.
    last_cells_ = cells;
    last_selected_id_ = selected_id;

    list_->blockSignals(true);
    list_->clear();

    int selected_row = -1;
    for (int i = 0; i < cells.size(); ++i) {
        const auto& cell = cells[i];
        const QString type_tag = (cell.cell_type == "code") ? "PY" : "MD";
        QString exec_tag;
        if (cell.execution_count > 0)
            exec_tag = QString("  [%1]").arg(cell.execution_count);

        // Prefer a user/authored title; otherwise derive a clean name from the
        // cell source (strips markdown markers + emoji).
        QString name = cell.title.trimmed();
        if (name.isEmpty())
            name = clean_cell_name(cell.source);
        if (name.isEmpty())
            name = tr("(empty)");

        // " 1  PY  Fincept Notebook — Time Value of Money   [3]"
        const QString label = QString("%1  %2  %3%4").arg(i + 1, 2).arg(type_tag, -2).arg(name, exec_tag);
        auto* item = new QListWidgetItem(label, list_);
        item->setData(Qt::UserRole, cell.id);
        item->setToolTip(name);

        if (cell.id == selected_id)
            selected_row = i;
    }

    if (selected_row >= 0)
        list_->setCurrentRow(selected_row);
    list_->blockSignals(false);
}

void CellNavigator::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void CellNavigator::retranslateUi() {
    if (header_title_)
        header_title_->setText(tr("CELLS"));
    // Re-render list rows so the "(empty)" fallback picks up the new language.
    rebuild(last_cells_, last_selected_id_);
}

// ═════════════════════════════════════════════════════════════════════════════
// CodeEditorScreen
// ═════════════════════════════════════════════════════════════════════════════

} // namespace fincept::screens
