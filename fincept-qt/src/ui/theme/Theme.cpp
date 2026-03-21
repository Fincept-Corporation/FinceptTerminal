#include "ui/theme/Theme.h"
#include <QApplication>

namespace fincept::ui {

QString change_color(double value) {
    return value >= 0 ? colors::POSITIVE : colors::NEGATIVE;
}

void apply_global_stylesheet() {
    QString ss = QString(R"(
        * {
            font-family: 'Consolas', 'Courier New', monospace;
            font-size: 16px;
        }
        QWidget {
            background-color: #080808;
            color: #e5e5e5;
        }
        QScrollBar:vertical {
            background: transparent; width: 5px; margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #222222; min-height: 20px;
        }
        QScrollBar::handle:vertical:hover { background: #333333; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar:horizontal {
            background: transparent; height: 5px; margin: 0;
        }
        QScrollBar::handle:horizontal {
            background: #222222; min-width: 20px;
        }
        QScrollBar::handle:horizontal:hover { background: #333333; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
        QLineEdit, QTextEdit, QPlainTextEdit {
            background: #0a0a0a; color: #e5e5e5; border: 1px solid #1a1a1a;
            padding: 3px 6px; selection-background-color: #d97706; selection-color: #080808;
        }
        QLineEdit:focus, QTextEdit:focus { border: 1px solid #333333; }
        QPushButton {
            background: #0a0a0a; color: #808080; border: 1px solid #1a1a1a;
            padding: 4px 12px; font-size: 14px;
        }
        QPushButton:hover { background: #111111; color: #e5e5e5; }
        QPushButton:pressed { background: #1a1a1a; }
        QPushButton:disabled { color: #333333; }
        QComboBox {
            background: #0a0a0a; color: #e5e5e5; border: 1px solid #1a1a1a;
            padding: 3px 8px; font-size: 14px;
        }
        QComboBox::drop-down { border: none; width: 16px; }
        QComboBox QAbstractItemView {
            background: #0a0a0a; color: #e5e5e5; border: 1px solid #1a1a1a;
            selection-background-color: #111111;
        }
        QHeaderView::section {
            background: #0a0a0a; color: #404040; border: none;
            border-bottom: 1px solid #1a1a1a; padding: 2px 4px;
            font-size: 13px; font-weight: 600;
        }
        QTableView, QTreeView, QListView {
            background: #080808; alternate-background-color: #0c0c0c;
            gridline-color: #111111; border: none; font-size: 14px;
        }
        QTableView::item, QTreeView::item, QListView::item {
            padding: 1px 4px; border-bottom: 1px solid #111111;
        }
        QTableView::item:selected, QTreeView::item:selected {
            background: #161616; color: #e5e5e5;
        }
        QSplitter::handle { background: #1a1a1a; }
        QSplitter::handle:hover { background: #333333; }
        QTabBar::tab {
            background: #0a0a0a; color: #404040; border: 1px solid #1a1a1a;
            padding: 4px 12px; font-size: 9px;
        }
        QTabBar::tab:selected {
            background: #111111; color: #e5e5e5; border-bottom: 1px solid #d97706;
        }
        QToolTip {
            background: #0a0a0a; color: #e5e5e5; border: 1px solid #1a1a1a;
            font-size: 9px; padding: 3px 6px;
        }
        QCheckBox { color: #808080; font-size: 14px; }
        QCheckBox::indicator {
            width: 12px; height: 12px; border: 1px solid #333333; background: #0a0a0a;
        }
        QCheckBox::indicator:checked { background: #d97706; border-color: #d97706; }
        QProgressBar { background: #0a0a0a; border: 1px solid #1a1a1a; color: #404040; font-size: 8px; }
        QProgressBar::chunk { background: #d97706; }
        QMenu { background: #0a0a0a; color: #e5e5e5; border: 1px solid #1a1a1a; }
        QMenu::item:selected { background: #161616; }
        QMenu::separator { background: #1a1a1a; height: 1px; }
    )");

    qApp->setStyleSheet(ss);
}

} // namespace fincept::ui
