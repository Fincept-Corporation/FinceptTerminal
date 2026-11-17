#pragma once
// ChartExport — "Copy image / Save PNG / Save CSV" for any chart or table.
//
//   ui::ChartExport::save_png(chart, "AAPL_price");
//   ui::ChartExport::save_csv({"Date", "Close"}, rows, "AAPL_price");
//
//   // Right-click menu on any widget (a chart, a QTableView, a panel):
//   ui::ChartExport::MenuSpec spec;
//   spec.suggested_name = "positions";
//   spec.csv = [this] { return build_table(); };
//   ui::ChartExport::install_context_menu(widget, spec);
//
//   // A QTableView / QTreeView in one line (CSV from its model, image of the view):
//   ui::ChartExport::install_table_context_menu(view, "positions");
//
// File dialogs start in the last directory used this session (Pictures for
// images, Documents for CSV the first time). CSV is UTF-8 with a BOM (so Excel
// opens non-ASCII names correctly), RFC 4180 quoting, and cells that would be
// read as a spreadsheet formula are neutralised with a leading apostrophe.

#include <QCoreApplication>
#include <QPixmap>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

class QAbstractItemModel;
class QAbstractItemView;
class QMenu;
class QWidget;

namespace fincept::ui {

class ChartExport {
    Q_DECLARE_TR_FUNCTIONS(ChartExport)

  public:
    /// A table to export: header row + data rows (already formatted as text).
    struct Table {
        QStringList headers;
        QVector<QStringList> rows;
    };

    /// What a context menu offers. Only `suggested_name` is required.
    struct MenuSpec {
        QString suggested_name;                ///< base file name, no extension ("AAPL_price")
        std::function<Table()> csv;            ///< set → "Save CSV…" is offered
        std::function<QPixmap()> image;        ///< unset → the widget is grabbed
        std::function<void(QMenu*)> extra;     ///< appended after the export actions
        bool offer_image = true;               ///< "Copy image" + "Save PNG…"
    };

    /// The widget as an image at its device pixel ratio.
    static QPixmap grab(QWidget* w);

    /// Ask for a file name and save `w` (or `image`) as PNG. False when
    /// cancelled or the write failed (the user is told about a failure).
    static bool save_png(QWidget* w, const QString& suggested_name);
    static bool save_png(const QPixmap& image, const QString& suggested_name, QWidget* dialog_parent);

    /// Put `w` (or `image`) on the clipboard.
    static void copy_image(QWidget* w);
    static void copy_image(const QPixmap& image);

    /// Ask for a file name and write a CSV. False when cancelled / failed.
    static bool save_csv(const QStringList& headers, const QVector<QStringList>& rows,
                         const QString& suggested_name, QWidget* dialog_parent = nullptr);
    /// Write a CSV to `path` without asking. False on an I/O error.
    static bool write_csv(const QString& path, const QStringList& headers, const QVector<QStringList>& rows);
    /// The CSV text (no BOM) — for tests and clipboard use.
    static QString to_csv(const QStringList& headers, const QVector<QStringList>& rows);
    /// One CSV cell: quoted when needed, formula-looking text neutralised.
    static QString csv_cell(const QString& cell);

    /// DisplayRole text of every row / column of a flat model (header from
    /// headerData). `skip_columns` are omitted (e.g. an action-button column).
    static Table table_from_model(const QAbstractItemModel* model, const QVector<int>& skip_columns = {});

    /// Add the export actions described by `spec` to `menu` (for widgets that
    /// build their own context menu). `w` is the widget grabbed for images.
    static void populate_menu(QMenu* menu, QWidget* w, const MenuSpec& spec);

    /// Give `w` a right-click menu built from `spec`. Sets
    /// Qt::CustomContextMenu on `w`; for a scroll area (tables) the menu is
    /// requested from the viewport, which Qt handles transparently.
    static void install_context_menu(QWidget* w, MenuSpec spec);

    /// install_context_menu for an item view: CSV = table_from_model(view->model()).
    static void install_table_context_menu(QAbstractItemView* view, const QString& suggested_name);
};

} // namespace fincept::ui
