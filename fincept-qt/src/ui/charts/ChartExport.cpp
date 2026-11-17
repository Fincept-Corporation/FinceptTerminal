#include "ui/charts/ChartExport.h"

#include "core/logging/Logger.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QAction>
#include <QClipboard>
#include <QDate>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTableView>
#include <QTreeView>
#include <QWidget>

namespace fincept::ui {

namespace {

/// Directory of the last successful save this session ("" until then).
QString& cex_last_dir() {
    static QString dir;
    return dir;
}

QString cex_sanitize(const QString& name) {
    static const QString kBad = QStringLiteral("\\/:*?\"<>|");
    QString out;
    out.reserve(name.size());
    bool last_us = false;
    for (const QChar c : name) {
        const bool bad = c.isSpace() || kBad.contains(c) || c.category() == QChar::Other_Control;
        if (bad) {
            if (!last_us)
                out.append(QLatin1Char('_'));
            last_us = true;
        } else {
            out.append(c);
            last_us = false;
        }
    }
    while (out.startsWith(QLatin1Char('_')) || out.startsWith(QLatin1Char('.')))
        out.remove(0, 1);
    while (out.endsWith(QLatin1Char('_')) || out.endsWith(QLatin1Char('.')))
        out.chop(1);
    if (out.isEmpty())
        out = QStringLiteral("chart");
    return out;
}

QString cex_default_path(const QString& name, const QString& ext, QStandardPaths::StandardLocation fallback) {
    QString dir = cex_last_dir();
    if (dir.isEmpty() || !QDir(dir).exists())
        dir = QStandardPaths::writableLocation(fallback);
    if (dir.isEmpty())
        dir = QDir::homePath();
    const QString file = cex_sanitize(name) + QLatin1Char('_') +
                         QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")) + QLatin1Char('.') + ext;
    return QDir(dir).filePath(file);
}

void cex_remember_dir(const QString& path) {
    cex_last_dir() = QFileInfo(path).absolutePath();
}

/// True when `s` parses as a plain number (C or current locale) — such cells
/// may start with '+'/'-' without being a formula.
bool cex_is_number(const QString& s) {
    bool ok = false;
    s.toDouble(&ok);
    if (ok)
        return true;
    QLocale().toDouble(s, &ok);
    return ok;
}

} // namespace

QPixmap ChartExport::grab(QWidget* w) {
    return w ? w->grab() : QPixmap();
}

bool ChartExport::save_png(QWidget* w, const QString& suggested_name) {
    return save_png(grab(w), suggested_name, w);
}

bool ChartExport::save_png(const QPixmap& image, const QString& suggested_name, QWidget* dialog_parent) {
    if (image.isNull())
        return false;
    QString path = QFileDialog::getSaveFileName(
        dialog_parent, tr("Save Chart Image"),
        cex_default_path(suggested_name, QStringLiteral("png"), QStandardPaths::PicturesLocation),
        tr("PNG image (*.png)"));
    if (path.isEmpty())
        return false;
    if (!path.endsWith(QLatin1String(".png"), Qt::CaseInsensitive))
        path += QStringLiteral(".png");
    if (!image.save(path, "PNG")) {
        QMessageBox::warning(dialog_parent, tr("Save Failed"),
                             tr("Could not write %1.").arg(QDir::toNativeSeparators(path)));
        return false;
    }
    cex_remember_dir(path);
    return true;
}

void ChartExport::copy_image(QWidget* w) {
    copy_image(grab(w));
}

void ChartExport::copy_image(const QPixmap& image) {
    if (image.isNull())
        return;
    if (QClipboard* cb = QGuiApplication::clipboard())
        cb->setImage(image.toImage());
}

QString ChartExport::csv_cell(const QString& cell) {
    QString s = cell;
    if (!s.isEmpty()) {
        const QChar c0 = s.at(0);
        const bool formula_like = c0 == QLatin1Char('=') || c0 == QLatin1Char('@') || c0 == QLatin1Char('\t') ||
                                  c0 == QLatin1Char('\r') ||
                                  ((c0 == QLatin1Char('+') || c0 == QLatin1Char('-')) && !cex_is_number(s));
        if (formula_like)
            s.prepend(QLatin1Char('\''));
    }
    const bool needs_quotes = s.contains(QLatin1Char(',')) || s.contains(QLatin1Char('"')) ||
                              s.contains(QLatin1Char('\n')) || s.contains(QLatin1Char('\r')) ||
                              (!s.isEmpty() && (s.front().isSpace() || s.back().isSpace()));
    if (!needs_quotes)
        return s;
    s.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + s + QLatin1Char('"');
}

QString ChartExport::to_csv(const QStringList& headers, const QVector<QStringList>& rows) {
    QString out;
    auto add_line = [&out](const QStringList& cells) {
        for (int i = 0; i < cells.size(); ++i) {
            if (i > 0)
                out.append(QLatin1Char(','));
            out.append(csv_cell(cells.at(i)));
        }
        out.append(QStringLiteral("\r\n"));
    };
    if (!headers.isEmpty())
        add_line(headers);
    for (const auto& r : rows)
        add_line(r);
    return out;
}

bool ChartExport::write_csv(const QString& path, const QStringList& headers, const QVector<QStringList>& rows) {
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        LOG_WARN("ChartExport", logf("Chart CSV export failed", {{"path", log_path(path)}, {"error", f.errorString()}}));
        return false;
    }
    static const char kCexBom[] = "\xEF\xBB\xBF";
    f.write(kCexBom, 3);
    f.write(to_csv(headers, rows).toUtf8());
    const bool committed = f.commit();
    if (!committed)
        LOG_WARN("ChartExport", logf("Chart CSV export failed", {{"path", log_path(path)}, {"error", f.errorString()}}));
    return committed;
}

bool ChartExport::save_csv(const QStringList& headers, const QVector<QStringList>& rows,
                           const QString& suggested_name, QWidget* dialog_parent) {
    QString path = QFileDialog::getSaveFileName(
        dialog_parent, tr("Export CSV"),
        cex_default_path(suggested_name, QStringLiteral("csv"), QStandardPaths::DocumentsLocation),
        tr("CSV file (*.csv)"));
    if (path.isEmpty())
        return false;
    if (!path.endsWith(QLatin1String(".csv"), Qt::CaseInsensitive))
        path += QStringLiteral(".csv");
    if (!write_csv(path, headers, rows)) {
        QMessageBox::warning(dialog_parent, tr("Export Failed"),
                             tr("Could not write %1.").arg(QDir::toNativeSeparators(path)));
        return false;
    }
    cex_remember_dir(path);
    return true;
}

ChartExport::Table ChartExport::table_from_model(const QAbstractItemModel* model, const QVector<int>& skip_columns) {
    Table t;
    if (!model)
        return t;
    const int cols = model->columnCount();
    const int rows = model->rowCount();
    for (int c = 0; c < cols; ++c) {
        if (!skip_columns.contains(c))
            t.headers << model->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString();
    }
    t.rows.reserve(rows);
    for (int r = 0; r < rows; ++r) {
        QStringList line;
        line.reserve(cols);
        for (int c = 0; c < cols; ++c) {
            if (!skip_columns.contains(c))
                line << model->data(model->index(r, c), Qt::DisplayRole).toString();
        }
        t.rows.append(line);
    }
    return t;
}

void ChartExport::populate_menu(QMenu* menu, QWidget* w, const MenuSpec& spec) {
    if (!menu)
        return;
    const QPointer<QWidget> guard(w);
    const QString name = spec.suggested_name;
    if (spec.offer_image) {
        const auto image_fn = spec.image;
        QAction* copy = menu->addAction(tr("Copy Image"));
        QObject::connect(copy, &QAction::triggered, copy, [guard, image_fn]() {
            if (image_fn)
                copy_image(image_fn());
            else if (guard)
                copy_image(guard.data());
        });
        QAction* png = menu->addAction(tr("Save as PNG…"));
        QObject::connect(png, &QAction::triggered, png, [guard, image_fn, name]() {
            const QPixmap pm = image_fn ? image_fn() : grab(guard.data());
            save_png(pm, name, guard.data());
        });
    }
    if (spec.csv) {
        const auto csv_fn = spec.csv;
        QAction* csv = menu->addAction(tr("Save as CSV…"));
        QObject::connect(csv, &QAction::triggered, csv, [guard, csv_fn, name]() {
            const Table t = csv_fn();
            save_csv(t.headers, t.rows, name, guard.data());
        });
    }
    if (spec.extra) {
        if (!menu->isEmpty())
            menu->addSeparator();
        spec.extra(menu);
    }
}

void ChartExport::install_context_menu(QWidget* w, MenuSpec spec) {
    if (!w)
        return;
    w->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(w, &QWidget::customContextMenuRequested, w, [w, spec](const QPoint& pos) {
        QMenu menu(w);
        populate_menu(&menu, w, spec);
        if (menu.isEmpty())
            return;
        // Scroll areas (tables) report the position in viewport coordinates.
        QWidget* origin = w;
        if (auto* area = qobject_cast<QAbstractScrollArea*>(w))
            origin = area->viewport();
        menu.exec(origin->mapToGlobal(pos));
    });
}

void ChartExport::install_table_context_menu(QAbstractItemView* view, const QString& suggested_name) {
    if (!view)
        return;
    MenuSpec spec;
    spec.suggested_name = suggested_name;
    const QPointer<QAbstractItemView> guard(view);
    spec.csv = [guard]() -> Table {
        if (!guard)
            return {};
        QVector<int> skip;
        if (const auto* model = guard->model()) {
            for (int c = 0; c < model->columnCount(); ++c) {
                bool hidden = false;
                if (const auto* tv = qobject_cast<const QTableView*>(guard.data()))
                    hidden = tv->isColumnHidden(c);
                else if (const auto* tree = qobject_cast<const QTreeView*>(guard.data()))
                    hidden = tree->isColumnHidden(c);
                if (hidden)
                    skip.append(c);
            }
        }
        return table_from_model(guard->model(), skip);
    };
    install_context_menu(view, spec);
}

} // namespace fincept::ui
