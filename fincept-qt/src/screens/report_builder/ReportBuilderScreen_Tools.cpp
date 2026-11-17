// src/screens/report_builder/ReportBuilderScreen_Tools.cpp
//
// Variables, scheduled regeneration, stale-save conflicts (two computers), the export
// formats and AI drafting — all non-modal, nothing blocks the GUI thread.
//
// Part of the partial-class split of ReportBuilderScreen.cpp.

#include "core/logging/Logger.h"
#include "core/report/ReportBinding.h"
#include "core/report/ReportSchedule.h"
#include "core/report/ReportVariables.h"
#include "screens/report_builder/ReportAiPanel.h"
#include "screens/report_builder/ReportBuilderScreen.h"
#include "services/report_builder/ReportAiSession.h"
#include "services/report_builder/ReportBuilderService.h"
#include "services/report_builder/ReportDataBinder.h"
#include "services/report_builder/ReportScheduler.h"
#include "ui/theme/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QStyle>
#include <QTimeZone>
#include <QTableView>
#include <QTimeEdit>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <optional>

namespace fincept::screens {

namespace rep = ::fincept::report;
namespace sch = ::fincept::report::schedule;
using Service = ::fincept::services::ReportBuilderService;
using Scheduler = ::fincept::services::ReportScheduler;

namespace {

// A non-modal dialog with one selector-scoped sheet set before its children exist.
QDialog* rbx_dialog(QWidget* parent, const QString& title, int min_width) {
    auto* dlg = new QDialog(parent);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setObjectName(QStringLiteral("rbDialog"));
    dlg->setWindowTitle(title);
    dlg->setModal(false);
    dlg->setMinimumWidth(min_width);
    dlg->setStyleSheet(
        QStringLiteral("#rbDialog { background: %1; }"
                       "#rbDialog QLabel { color: %2; background: transparent; font-size: %6px; }"
                       "#rbDialog QLabel[rbRole=\"section\"] { color: %3; font-weight: bold; padding-top: 8px; }"
                       "#rbDialog QLabel[rbRole=\"hint\"] { color: %3; }"
                       "#rbDialog QLabel[rbRole=\"bad\"] { color: %10; }"
                       "#rbDialog QLineEdit, #rbDialog QComboBox, #rbDialog QTimeEdit, #rbDialog QListView,"
                       " #rbDialog QTableView { background: %4; color: %2; border: 1px solid %5; padding: 3px;"
                       " font-size: %6px; }"
                       "#rbDialog QHeaderView::section { background: %7; color: %3; border: none; padding: 3px;"
                       " font-size: %6px; }"
                       "#rbDialog QCheckBox { color: %2; font-size: %6px; }"
                       "#rbDialog QPushButton { background: %7; color: %2; border: 1px solid %5; padding: 4px 14px;"
                       " font-size: %6px; }"
                       "#rbDialog QPushButton:hover { background: %8; }"
                       "#rbDialog QPushButton:default, #rbDialog QPushButton[rbRole=\"primary\"] { border-color: %9;"
                       " color: %9; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_TERTIARY(), ui::colors::BG_BASE(),
                 ui::colors::BORDER_MED(), ui::fonts::px(-1), ui::colors::BG_RAISED(), ui::colors::BG_HOVER(),
                 ui::colors::AMBER())
            .arg(ui::colors::DANGER()));
    return dlg;
}

bool rbx_raise(const QPointer<QDialog>& dlg) {
    if (!dlg)
        return false;
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
    return true;
}

QString rbx_default_folder() {
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QStringLiteral("/Fincept Reports");
}

} // namespace

// ── Exports in every format ─────────────────────────────────────────────────

bool ReportBuilderScreen::confirm_unresolved(const QString& title) {
    const QStringList names = rep::vars::unresolved_in(Service::instance().snapshot());
    if (names.isEmpty())
        return true;
    QStringList refs;
    for (const QString& n : names)
        refs << QStringLiteral("{{%1}}").arg(n);
    QMessageBox box(QMessageBox::Warning, title,
                    tr("%n variable(s) have no value and would print as typed: %1", nullptr, static_cast<int>(names.size()))
                        .arg(refs.join(QStringLiteral(", "))),
                    QMessageBox::NoButton, this);
    QPushButton* set = box.addButton(tr("Set variables…"), QMessageBox::ActionRole);
    QPushButton* go = box.addButton(tr("Export anyway"), QMessageBox::AcceptRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(set);
    box.exec();
    if (box.clickedButton() == set) {
        show_variables_dialog();
        return false;
    }
    return box.clickedButton() == go;
}

void ReportBuilderScreen::on_export(ReportExportJob::Format format) {
    properties_->flush_pending();
    const QString name = ReportExportJob::display_name(format);
    if (!confirm_unfilled(tr("Export %1").arg(name)) || !confirm_unresolved(tr("Export %1").arg(name)))
        return;
    const QString ext = ReportExportJob::extension(format);
    QString filter;
    switch (format) {
        case ReportExportJob::Format::Pdf:
            filter = tr("PDF (*.pdf)");
            break;
        case ReportExportJob::Format::Html:
            filter = tr("Web page (*.html)");
            break;
        case ReportExportJob::Format::Odt:
            filter = tr("OpenDocument text — opens in Word and LibreOffice (*.odt)");
            break;
        case ReportExportJob::Format::Markdown:
            filter = tr("Markdown (*.md)");
            break;
    }
    QString path = QFileDialog::getSaveFileName(this, tr("Export %1").arg(name),
                                                rep::schedule::safe_file_stem(rep::vars::display_title(Service::instance().metadata())),
                                                filter);
    if (path.isEmpty())
        return;
    if (QFileInfo(path).suffix().compare(ext, Qt::CaseInsensitive) != 0 &&
        !(format == ReportExportJob::Format::Html && path.endsWith(QLatin1String(".htm"), Qt::CaseInsensitive)))
        path += QLatin1Char('.') + ext;
    start_export(format, path, {});
}

// ── Variables ───────────────────────────────────────────────────────────────

void ReportBuilderScreen::show_variables_dialog() {
    if (rbx_raise(vars_dlg_))
        return;
    auto* dlg = rbx_dialog(this, tr("Variables"), 520);
    vars_dlg_ = dlg;
    auto* vl = new QVBoxLayout(dlg);
    auto* intro = new QLabel(tr("Write {{name}} in any text, heading, table cell, chart title, the cover, header or "
                                "footer: it prints the value set here. Data blocks bound to {{symbol}} follow it after a "
                                "refresh."),
                             dlg);
    intro->setWordWrap(true);
    vl->addWidget(intro);

    const rep::ReportDocument doc = Service::instance().snapshot();
    const QMap<QString, QString> ctx = rep::vars::context(doc.metadata);
    QHash<QString, int> uses;
    for (const rep::ReportComponent& c : doc.components) {
        for (const QString& key : rep::vars::text_keys(c.type))
            for (const QString& n : rep::vars::references(key == QLatin1String("@content") ? c.content : c.config.value(key)))
                ++uses[n];
        for (const QString& n : rep::vars::references(c.config.value(QLatin1String(rep::keys::kBind))))
            ++uses[n];
    }

    auto* model = new QStandardItemModel(0, 3, dlg);
    model->setHorizontalHeaderLabels({tr("Variable"), tr("Value"), tr("Used")});
    auto add_row = [model, &uses](const QString& name, const QString& value, bool builtin) {
        auto* n = new QStandardItem(name);
        auto* v = new QStandardItem(value);
        auto* u = new QStandardItem(uses.value(name) > 0 ? QString::number(uses.value(name)) : QString());
        u->setFlags(Qt::ItemIsEnabled);
        if (builtin) {
            n->setFlags(Qt::ItemIsEnabled);
            v->setFlags(Qt::ItemIsEnabled);
            n->setForeground(QColor(ui::colors::TEXT_TERTIARY()));
            v->setForeground(QColor(ui::colors::TEXT_TERTIARY()));
            v->setToolTip(tr("Set in Document info"));
            n->setData(true, Qt::UserRole);
        }
        model->appendRow({n, v, u});
    };
    for (const QString& b : rep::vars::builtin_names())
        add_row(b, ctx.value(b), true);
    QStringList custom = doc.metadata.variables.keys();
    for (const QString& s : rep::vars::suggested_names())
        if (!custom.contains(s))
            custom << s;
    for (const QString& n : rep::vars::used_in(doc))
        if (!custom.contains(n) && !rep::vars::builtin_names().contains(n))
            custom << n; // referenced but never set: listed so it can be filled
    for (const QString& n : custom)
        add_row(n, doc.metadata.variables.value(n), false);

    auto* table = new QTableView(dlg);
    table->setModel(model);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    const QFontMetrics fm(table->font());
    table->setColumnWidth(0, fm.horizontalAdvance(QStringLiteral("variable_name_x")) + 16);
    table->setColumnWidth(2, fm.horizontalAdvance(QStringLiteral("Used")) + 20);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                           QAbstractItemView::SelectedClicked);
    table->setMinimumHeight(fm.height() * 12);
    vl->addWidget(table, 1);

    auto* status = new QLabel(dlg);
    status->setWordWrap(true);
    status->setProperty("rbRole", QStringLiteral("hint"));
    vl->addWidget(status);
    auto update_status = [model, status]() {
        QStringList empty;
        for (int r = 0; r < model->rowCount(); ++r) {
            const QString n = model->item(r, 0)->text();
            if (model->item(r, 0)->data(Qt::UserRole).toBool())
                continue;
            if (model->item(r, 1)->text().trimmed().isEmpty() && !model->item(r, 2)->text().isEmpty())
                empty << QStringLiteral("{{%1}}").arg(n);
        }
        status->setText(empty.isEmpty() ? tr("Every variable the report uses has a value.")
                                        : tr("No value yet (marked on the page, exports ask first): %1")
                                              .arg(empty.join(QStringLiteral(", "))));
    };
    update_status();
    connect(model, &QStandardItemModel::itemChanged, dlg, [update_status](QStandardItem*) { update_status(); });

    auto* bb = new QDialogButtonBox(dlg);
    QPushButton* add = bb->addButton(tr("Add variable"), QDialogButtonBox::ActionRole);
    QPushButton* remove = bb->addButton(tr("Remove"), QDialogButtonBox::ActionRole);
    QPushButton* apply = bb->addButton(tr("Apply"), QDialogButtonBox::AcceptRole);
    apply->setProperty("rbRole", QStringLiteral("primary"));
    bb->addButton(QDialogButtonBox::Close);
    vl->addWidget(bb);
    connect(add, &QPushButton::clicked, dlg, [model, table]() {
        auto* n = new QStandardItem(QStringLiteral("name"));
        auto* v = new QStandardItem();
        auto* u = new QStandardItem();
        u->setFlags(Qt::ItemIsEnabled);
        model->appendRow({n, v, u});
        table->setCurrentIndex(model->indexFromItem(n));
        table->edit(model->indexFromItem(n));
    });
    connect(remove, &QPushButton::clicked, dlg, [model, table, update_status]() {
        const QModelIndex idx = table->currentIndex();
        if (!idx.isValid() || model->item(idx.row(), 0)->data(Qt::UserRole).toBool())
            return; // built-ins stay
        model->removeRow(idx.row());
        update_status();
    });
    connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::close);
    connect(apply, &QPushButton::clicked, dlg, [this, model, status, dlg]() {
        QMap<QString, QString> vars;
        for (int r = 0; r < model->rowCount(); ++r) {
            if (model->item(r, 0)->data(Qt::UserRole).toBool())
                continue;
            const QString name = rep::vars::normalise(model->item(r, 0)->text());
            if (name.isEmpty())
                continue;
            if (!rep::vars::is_valid_name(name) || rep::vars::builtin_names().contains(name)) {
                status->setText(tr("“%1” is not a usable name: a letter first, then letters, digits or _ (and not "
                                   "title, author, company or date).")
                                    .arg(name));
                status->setProperty("rbRole", QStringLiteral("bad"));
                status->style()->unpolish(status);
                status->style()->polish(status);
                return;
            }
            vars.insert(name, model->item(r, 1)->text().trimmed());
        }
        auto& svc = Service::instance();
        rep::ReportMetadata meta = svc.metadata();
        // Empty suggested names are not stored (they are offered again next time).
        for (auto it = vars.begin(); it != vars.end();) {
            if (it.value().isEmpty() && rep::vars::suggested_names().contains(it.key()) && !meta.variables.contains(it.key()))
                it = vars.erase(it);
            else
                ++it;
        }
        if (vars != meta.variables) {
            const bool symbol_changed = vars.value(QStringLiteral("symbol")) != meta.variables.value(QStringLiteral("symbol"));
            meta.variables = vars;
            svc.set_metadata(meta); // one undo step
            if (symbol_changed) {
                bool follows = false;
                for (const rep::ReportComponent& c : svc.components())
                    follows = follows || c.config.value(QLatin1String(rep::keys::kBind)).contains(QLatin1String("{{symbol}}"));
                if (follows)
                    show_notice(NoticeKind::Stale,
                                tr("{{symbol}} is now %1 — the data blocks that follow it still show the old numbers.")
                                    .arg(vars.value(QStringLiteral("symbol"))),
                                tr("REFRESH DATA"), "warn");
            }
        }
        dlg->close();
    });
    dlg->show();
}

// ── Scheduled regeneration ──────────────────────────────────────────────────

void ReportBuilderScreen::show_schedule_dialog() {
    if (rbx_raise(schedule_dlg_))
        return;
    auto& svc = Service::instance();
    auto& scheduler = Scheduler::instance();
    const qint64 id = svc.library_id();
    auto* dlg = rbx_dialog(this, tr("Schedule — %1").arg(rep::vars::display_title(svc.metadata())), 540);
    schedule_dlg_ = dlg;
    auto* vl = new QVBoxLayout(dlg);

    if (id <= 0) {
        auto* msg = new QLabel(tr("A report is scheduled from the library. This one is not saved there yet — it is "
                                  "saved a few seconds after an edit, or now:"),
                               dlg);
        msg->setWordWrap(true);
        vl->addWidget(msg);
        auto* bb = new QDialogButtonBox(dlg);
        QPushButton* save = bb->addButton(tr("Save to the library"), QDialogButtonBox::AcceptRole);
        bb->addButton(QDialogButtonBox::Close);
        vl->addWidget(bb);
        connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::close);
        connect(save, &QPushButton::clicked, dlg, [this, dlg]() {
            Service::instance().save_to_library();
            // Re-open on the saved report once it has its library id.
            auto* once = new QObject(dlg);
            connect(&Service::instance(), &Service::library_id_changed, once, [this, dlg, once](qint64 nid) {
                if (nid <= 0)
                    return;
                once->deleteLater();
                dlg->close();
                QTimer::singleShot(0, this, [this]() { show_schedule_dialog(); });
            });
        });
        dlg->show();
        return;
    }

    sch::Schedule s;
    const bool existing = scheduler.schedule_for(id, &s);
    if (!existing) {
        s.report_id = id;
        s.folder = rbx_default_folder();
        s.enabled = true;
    }

    auto* intro = new QLabel(tr("Refresh this report's data and export it to a folder on a schedule. Runs only while "
                                "Fincept Terminal is open; a time missed while it was closed runs once when it next "
                                "opens. Runs never overlap. AI drafting is never run automatically."),
                             dlg);
    intro->setWordWrap(true);
    intro->setProperty("rbRole", QStringLiteral("hint"));
    vl->addWidget(intro);

    auto* enabled = new QCheckBox(tr("Regenerate this report automatically"), dlg);
    enabled->setChecked(!existing || s.enabled);
    vl->addWidget(enabled);
    auto* form = new QFormLayout();
    auto* freq = new QComboBox(dlg);
    freq->addItem(tr("Every day"), QStringLiteral("daily"));
    freq->addItem(tr("Every week"), QStringLiteral("weekly"));
    freq->addItem(tr("When the terminal starts (at most every 12 hours)"), QStringLiteral("on_start"));
    freq->setCurrentIndex(std::max(0, freq->findData(sch::frequency_id(s.frequency))));
    auto* time = new QTimeEdit(s.time, dlg);
    time->setDisplayFormat(QStringLiteral("HH:mm"));
    auto* day = new QComboBox(dlg);
    for (int d = 1; d <= 7; ++d)
        day->addItem(QLocale().dayName(d), d);
    day->setCurrentIndex(std::clamp(s.weekday, 1, 7) - 1);
    auto* folder_row = new QWidget(dlg);
    auto* frl = new QHBoxLayout(folder_row);
    frl->setContentsMargins(0, 0, 0, 0);
    auto* folder = new QLineEdit(QDir::toNativeSeparators(s.folder), folder_row);
    auto* browse = new QPushButton(tr("Browse…"), folder_row);
    frl->addWidget(folder, 1);
    frl->addWidget(browse);
    auto* fmt_row = new QWidget(dlg);
    auto* fml = new QHBoxLayout(fmt_row);
    fml->setContentsMargins(0, 0, 0, 0);
    QHash<QString, QCheckBox*> formats;
    for (const auto& f : QVector<QPair<QString, QString>>{{QStringLiteral("pdf"), QStringLiteral("PDF")},
                                                          {QStringLiteral("odt"), tr("Word (.odt)")},
                                                          {QStringLiteral("md"), QStringLiteral("Markdown")},
                                                          {QStringLiteral("html"), QStringLiteral("HTML")}}) {
        auto* cb = new QCheckBox(f.second, fmt_row);
        cb->setChecked(s.formats.contains(f.first));
        formats.insert(f.first, cb);
        fml->addWidget(cb);
    }
    fml->addStretch();
    auto* refresh = new QCheckBox(tr("Refresh the live data first"), dlg);
    refresh->setChecked(s.refresh_data);
    form->addRow(tr("How often"), freq);
    form->addRow(tr("Time"), time);
    form->addRow(tr("Day"), day);
    form->addRow(tr("Folder"), folder_row);
    form->addRow(tr("Formats"), fmt_row);
    form->addRow(QString(), refresh);
    vl->addLayout(form);
    auto* next = new QLabel(dlg);
    next->setProperty("rbRole", QStringLiteral("hint"));
    vl->addWidget(next);
    auto* error = new QLabel(dlg);
    error->setProperty("rbRole", QStringLiteral("bad"));
    error->setWordWrap(true);
    vl->addWidget(error);
    auto sync_enabled = [freq, time, day]() {
        const QString f = freq->currentData().toString();
        time->setEnabled(f != QLatin1String("on_start"));
        day->setEnabled(f == QLatin1String("weekly"));
    };
    sync_enabled();
    connect(freq, &QComboBox::currentIndexChanged, dlg, [sync_enabled](int) { sync_enabled(); });
    connect(browse, &QPushButton::clicked, dlg, [dlg, folder]() {
        const QString d = QFileDialog::getExistingDirectory(dlg, tr("Export folder"), folder->text());
        if (!d.isEmpty())
            folder->setText(QDir::toNativeSeparators(d));
    });

    auto* actions = new QWidget(dlg);
    auto* al = new QHBoxLayout(actions);
    al->setContentsMargins(0, 0, 0, 0);
    auto* save = new QPushButton(tr("Save schedule"), actions);
    save->setProperty("rbRole", QStringLiteral("primary"));
    auto* run = new QPushButton(tr("Run now"), actions);
    auto* remove = new QPushButton(tr("Remove schedule"), actions);
    remove->setProperty("fincept_agent_risk", "delete"); // agents: removes the report's schedule
    auto* close = new QPushButton(tr("Close"), actions);
    al->addWidget(save);
    al->addWidget(run);
    al->addWidget(remove);
    al->addStretch();
    al->addWidget(close);
    vl->addWidget(actions);

    auto* sec = new QLabel(tr("RESULTS"), dlg);
    sec->setProperty("rbRole", QStringLiteral("section"));
    vl->addWidget(sec);
    auto* model = new QStandardItemModel(dlg);
    auto* list = new QListView(dlg);
    list->setModel(model);
    list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    list->setMinimumHeight(QFontMetrics(list->font()).height() * 6);
    vl->addWidget(list, 1);
    auto* res_row = new QWidget(dlg);
    auto* rrl = new QHBoxLayout(res_row);
    rrl->setContentsMargins(0, 0, 0, 0);
    auto* open_file = new QPushButton(tr("Open"), res_row);
    auto* reveal = new QPushButton(tr("Show in folder"), res_row);
    open_file->setEnabled(false);
    reveal->setEnabled(false);
    rrl->addWidget(open_file);
    rrl->addWidget(reveal);
    rrl->addStretch();
    vl->addWidget(res_row);

    auto refresh_view = [id, model, next, run]() {
        auto& sc = Scheduler::instance();
        model->clear();
        for (const sch::RunRecord& r : sc.runs(id)) {
            QString line = r.started.toLocalTime().toString(QStringLiteral("d MMM yyyy HH:mm")) + QStringLiteral(" · ") +
                           r.trigger + QStringLiteral(" · ") +
                           (r.ok ? tr("%n file(s)", nullptr, static_cast<int>(r.files.size())) : tr("failed: %1").arg(r.error));
            if (!r.warnings.isEmpty())
                line += QStringLiteral(" · ") + tr("%n warning(s)", nullptr, static_cast<int>(r.warnings.size()));
            auto* it = new QStandardItem(line);
            it->setData(r.files, Qt::UserRole);
            it->setToolTip((r.files + r.warnings).join(QLatin1Char('\n')));
            if (!r.ok)
                it->setForeground(QColor(ui::colors::DANGER()));
            else if (!r.warnings.isEmpty())
                it->setForeground(QColor(ui::colors::WARNING()));
            model->appendRow(it);
        }
        if (model->rowCount() == 0)
            model->appendRow(new QStandardItem(tr("No runs yet.")));
        sch::Schedule cur;
        if (sc.is_queued_or_running(id)) {
            next->setText(tr("Running now…"));
        } else if (sc.schedule_for(id, &cur) && cur.enabled) {
            const QDateTime n = sc.next_run(id);
            next->setText(n.isValid() ? tr("Next run: %1").arg(QLocale().toString(n, QLocale::ShortFormat))
                                      : tr("Next run: when the terminal next starts"));
        } else {
            next->setText(tr("Not scheduled."));
        }
        run->setEnabled(!sc.is_queued_or_running(id) && sc.schedule_for(id, nullptr));
    };
    refresh_view();
    connect(&scheduler, &Scheduler::changed, dlg, refresh_view);

    auto build = [id, enabled, freq, time, day, folder, formats, refresh, error]() -> std::optional<sch::Schedule> {
        sch::Schedule n;
        n.report_id = id;
        n.title = rep::vars::display_title(Service::instance().metadata());
        n.enabled = enabled->isChecked();
        n.frequency = sch::frequency_from_id(freq->currentData().toString());
        n.time = time->time();
        n.weekday = day->currentData().toInt();
        n.folder = QDir::fromNativeSeparators(folder->text().trimmed());
        n.formats.clear();
        for (const QString& f : sch::known_formats())
            if (formats.value(f) && formats.value(f)->isChecked())
                n.formats << f;
        n.refresh_data = refresh->isChecked();
        if (n.formats.isEmpty()) {
            error->setText(tr("Pick at least one format."));
            return std::nullopt;
        }
        if (n.folder.isEmpty() || !QDir::isAbsolutePath(n.folder)) {
            error->setText(tr("Choose a folder for the files."));
            return std::nullopt;
        }
        if (!QDir().mkpath(n.folder)) {
            error->setText(tr("The folder cannot be created: %1").arg(QDir::toNativeSeparators(n.folder)));
            return std::nullopt;
        }
        error->clear();
        return n;
    };
    connect(save, &QPushButton::clicked, dlg, [build]() {
        if (const auto n = build())
            Scheduler::instance().set_schedule(*n);
    });
    connect(run, &QPushButton::clicked, dlg, [id, build]() {
        if (const auto n = build()) {
            Scheduler::instance().set_schedule(*n);
            Scheduler::instance().run_now(id);
        }
    });
    connect(remove, &QPushButton::clicked, dlg, [id]() { Scheduler::instance().remove_schedule(id); });
    connect(close, &QPushButton::clicked, dlg, &QDialog::close);
    auto current_files = [list]() {
        const QModelIndex idx = list->currentIndex();
        return idx.isValid() ? idx.data(Qt::UserRole).toStringList() : QStringList();
    };
    connect(list->selectionModel(), &QItemSelectionModel::currentChanged, dlg,
            [open_file, reveal, current_files](const QModelIndex&, const QModelIndex&) {
                const bool has = !current_files().isEmpty();
                open_file->setEnabled(has);
                reveal->setEnabled(has);
            });
    connect(open_file, &QPushButton::clicked, dlg, [current_files]() {
        const QStringList files = current_files();
        if (!files.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(files.first()));
    });
    connect(reveal, &QPushButton::clicked, dlg, [current_files]() {
        const QStringList files = current_files();
        if (!files.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(files.first()).absolutePath()));
    });
    dlg->show();
}

// ── Stale saves: the report changed on another computer ─────────────────────

void ReportBuilderScreen::show_conflict_dialog() {
    auto& svc = Service::instance();
    if (!svc.has_library_conflict() || rbx_raise(conflict_dlg_))
        return;
    const Service::LibraryConflict c = svc.library_conflict();
    QDateTime when = QDateTime::fromString(c.theirs_updated, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    when.setTimeZone(QTimeZone::UTC);
    auto* box = new QMessageBox(QMessageBox::Warning, tr("Report changed elsewhere"),
                                tr("“%1” was saved on another computer (%2) after you opened it here. Your edits are "
                                   "kept safe meanwhile — choose which version the library keeps.")
                                    .arg(rep::vars::display_title(svc.metadata()),
                                         when.isValid() ? QLocale().toString(when.toLocalTime(), QLocale::ShortFormat)
                                                        : tr("recently")),
                                QMessageBox::NoButton, this);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->setInformativeText(tr("Keep mine: your version replaces theirs.\nTake theirs: open their version (your edits "
                               "since then are dropped).\nKeep both: yours becomes a separate report “… (my copy)”."));
    QPushButton* mine = box->addButton(tr("Keep mine"), QMessageBox::AcceptRole);
    QPushButton* theirs = box->addButton(tr("Take theirs"), QMessageBox::DestructiveRole);
    QPushButton* both = box->addButton(tr("Keep both"), QMessageBox::ActionRole);
    box->setDefaultButton(both);
    conflict_dlg_ = box;
    connect(box, &QMessageBox::buttonClicked, this, [mine, theirs, both](QAbstractButton* b) {
        auto& s = Service::instance();
        if (b == mine)
            s.resolve_library_conflict(Service::ConflictChoice::KeepMine);
        else if (b == theirs)
            s.resolve_library_conflict(Service::ConflictChoice::TakeTheirs);
        else if (b == both)
            s.resolve_library_conflict(Service::ConflictChoice::KeepBoth);
    });
    box->setModal(false); // the terminal keeps working while the user decides
    box->show();
}

// ── AI drafting ─────────────────────────────────────────────────────────────

void ReportBuilderScreen::open_ai_panel(int block_id, const QString& action, const QString& tone, int sel_start,
                                        int sel_end) {
    namespace ai = ::fincept::report::ai;
    auto& svc = Service::instance();
    services::AiRequest req;
    req.doc = svc.snapshot();
    req.tone = tone;
    req.sel_start = sel_start;
    req.sel_end = sel_end;
    if (action == QLatin1String("summary")) {
        req.action = ai::Action::SummariseReport;
    } else if (action == QLatin1String("title")) {
        req.action = ai::Action::SuggestTitle;
    } else {
        rep::ReportComponent c;
        if (!svc.component(block_id, &c))
            return;
        req.block_id = block_id;
        req.block_text = c.content;
        req.action = action == QLatin1String("draft")      ? ai::Action::DraftFromData
                     : action == QLatin1String("shorten")  ? ai::Action::Shorten
                     : action == QLatin1String("expand")   ? ai::Action::Expand
                     : action == QLatin1String("grammar")  ? ai::Action::FixGrammar
                     : action == QLatin1String("tone")     ? ai::Action::ChangeTone
                                                           : ai::Action::Rewrite;
    }
    if (right_collapsed_)
        apply_right_collapsed(false, true);
    ai_panel_->open(req); // shows what would be sent; nothing leaves until the user presses Send
    ai_panel_->setVisible(true);
    const int h = right_split_->height();
    right_split_->setSizes({h * 2 / 5, h * 3 / 5});
}

void ReportBuilderScreen::apply_ai_result(const services::ApplyResult& r) {
    auto& svc = Service::instance();
    switch (r.kind) {
        case services::ApplyResult::Kind::ReplaceText: {
            rep::ReportComponent c;
            if (!svc.component(r.block_id, &c)) {
                show_notice(NoticeKind::Info, tr("The block was deleted meanwhile — nothing was changed."), QString(), "warn");
                return;
            }
            if (!r.old_content.isNull() && c.content != r.old_content) {
                show_notice(NoticeKind::Info,
                            tr("The block's text changed after the proposal was made — nothing was applied. Retry to "
                               "draft from the current text."),
                            QString(), "warn");
                return;
            }
            properties_->flush_pending();
            svc.patch_component(r.block_id, &r.new_content, {}, r.undo_text.isEmpty() ? tr("AI: edit text") : r.undo_text,
                                /*mergeable=*/false); // one undo step
            select_component(r.block_id, true);
            break;
        }
        case services::ApplyResult::Kind::InsertBlocks: {
            if (r.blocks.isEmpty())
                return;
            const int at = r.after_block_id > 0 ? svc.index_of(r.after_block_id) + 1 : 0;
            const QVector<int> ids = svc.append_components(r.blocks, tr("AI drafting"), at, false); // one undo step
            if (!ids.isEmpty())
                select_component(ids.first(), true);
            break;
        }
        case services::ApplyResult::Kind::SetTitle: {
            rep::ReportMetadata meta = svc.metadata();
            if (r.title.trimmed().isEmpty() || meta.title == r.title.trimmed())
                return;
            meta.title = r.title.trimmed();
            svc.set_metadata(meta); // one undo step
            break;
        }
    }
}

} // namespace fincept::screens
