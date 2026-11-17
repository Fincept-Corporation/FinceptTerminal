// src/screens/report_builder/ReportBuilderScreen_Dialogs.cpp
//
// Dialogs (all non-modal: Recent files, Theme, Document info, Page setup, Save as
// template), file actions (New / Open / Save / Save As / Import), opening library reports
// and templates, and the sliced, cancellable PDF / HTML exports.
//
// Part of the partial-class split of ReportBuilderScreen.cpp.

#include "core/logging/Logger.h"
#include "core/report/ReportBinding.h"
#include "screens/report_builder/ReportBuilderScreen.h"
#include "screens/report_builder/ReportExportJob.h"
#include "screens/report_builder/ReportRenderer.h"
#include "services/report_builder/ReportAssets.h"
#include "services/report_builder/ReportBuilderService.h"
#include "services/report_builder/ReportBuilderTemplates.h"
#include "services/report_builder/ReportDataBinder.h"
#include "services/report_builder/ReportFileSync.h"
#include "services/report_builder/ReportLibrary.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>

namespace fincept::screens {

namespace rep = ::fincept::report;
using Service = ::fincept::services::ReportBuilderService;
using Binder = ::fincept::services::ReportDataBinder;
using Library = ::fincept::services::ReportLibrary;

namespace {

// A non-modal dialog with one selector-scoped sheet set before its children exist.
QDialog* rbd_dialog(QWidget* parent, const QString& title, int min_width) {
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
                       "#rbDialog QLineEdit, #rbDialog QComboBox, #rbDialog QDoubleSpinBox, #rbDialog QListView"
                       " { background: %4; color: %2; border: 1px solid %5; padding: 3px; font-size: %6px; }"
                       "#rbDialog QCheckBox { color: %2; font-size: %6px; }"
                       "#rbDialog QPushButton { background: %7; color: %2; border: 1px solid %5; padding: 4px 14px;"
                       " font-size: %6px; }"
                       "#rbDialog QPushButton:hover { background: %8; }"
                       "#rbDialog QPushButton:default { border-color: %9; color: %9; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_TERTIARY(), ui::colors::BG_BASE(),
                 ui::colors::BORDER_MED(), ui::fonts::px(-1), ui::colors::BG_RAISED(), ui::colors::BG_HOVER(),
                 ui::colors::AMBER()));
    return dlg;
}

// Bring an already-open dialog forward instead of stacking a second copy.
bool rbd_raise(const QPointer<QDialog>& dlg) {
    if (!dlg)
        return false;
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
    return true;
}

} // namespace

// ── Recent files ────────────────────────────────────────────────────────────

void ReportBuilderScreen::show_recent_dialog() {
    if (rbd_raise(recent_dlg_))
        return;
    auto* dlg = rbd_dialog(this, tr("Recent report files"), 520);
    recent_dlg_ = dlg;
    auto* vl = new QVBoxLayout(dlg);
    auto* model = new QStandardItemModel(dlg);
    auto* list = new QListView(dlg);
    list->setModel(model);
    list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    vl->addWidget(list, 1);
    auto* hint = new QLabel(dlg);
    hint->setProperty("rbRole", QStringLiteral("hint"));
    hint->setWordWrap(true);
    vl->addWidget(hint);
    auto* bb = new QDialogButtonBox(dlg);
    QPushButton* open = bb->addButton(tr("Open"), QDialogButtonBox::AcceptRole);
    QPushButton* prune = bb->addButton(tr("Remove missing"), QDialogButtonBox::ActionRole);
    prune->setProperty("fincept_agent_risk", "delete"); // agents: removes the missing entries
    bb->addButton(QDialogButtonBox::Close);
    vl->addWidget(bb);

    auto fill = [model, hint, open](const QStringList& paths) {
        model->clear();
        int missing = 0;
        for (const QString& p : paths) {
            const bool exists = QFileInfo::exists(p);
            auto* it = new QStandardItem(exists ? p : tr("%1   (file not found)").arg(p));
            it->setData(p, Qt::UserRole);
            if (!exists) {
                it->setFlags(Qt::ItemIsEnabled); // listed, greyed, not openable
                it->setForeground(QColor(ui::colors::TEXT_TERTIARY()));
                ++missing;
            }
            model->appendRow(it);
        }
        hint->setText(paths.isEmpty() ? tr("No report files opened or saved yet. Reports are also kept in the library.")
                      : missing > 0   ? tr("%n file(s) no longer exist.", nullptr, missing)
                                      : QString());
        open->setEnabled(!paths.isEmpty());
    };
    fill(Service::instance().recent_files());
    auto open_selected = [this, list, dlg]() {
        const QModelIndex idx = list->currentIndex();
        if (!idx.isValid() || !(idx.flags() & Qt::ItemIsSelectable))
            return;
        const QString path = idx.data(Qt::UserRole).toString();
        dlg->close();
        open_file_async(path);
    };
    connect(open, &QPushButton::clicked, dlg, open_selected);
    connect(list, &QListView::doubleClicked, dlg, [open_selected](const QModelIndex&) { open_selected(); });
    connect(prune, &QPushButton::clicked, dlg, [fill]() { fill(Service::instance().prune_recent_files()); });
    connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::close);
    dlg->show();
}

// ── Theme ───────────────────────────────────────────────────────────────────

void ReportBuilderScreen::show_theme_dialog() {
    if (rbd_raise(theme_dlg_))
        return;
    auto* dlg = rbd_dialog(this, tr("Report theme"), 380);
    theme_dlg_ = dlg;
    auto* vl = new QVBoxLayout(dlg);
    auto* intro = new QLabel(tr("The report's paper and ink — what is printed and exported. It does not change "
                                "with the terminal's theme."),
                             dlg);
    intro->setWordWrap(true);
    vl->addWidget(intro);
    const QString current = Service::instance().theme().name;
    for (const QString& name : rep::themes::all_names()) {
        const rep::ReportTheme t = rep::themes::by_name(name);
        auto* b = new QPushButton(name, dlg);
        b->setCheckable(true);
        b->setChecked(name == current);
        b->setMinimumHeight(QFontMetrics(b->font()).height() * 2 + 8);
        // A swatch of the REPORT theme (document colours): its own small sheet.
        b->setStyleSheet(QStringLiteral("QPushButton { background: %1; color: %2; border: 2px solid %3; font-weight: bold; }"
                                        "QPushButton:checked { border: 3px solid %4; }")
                             .arg(t.page_bg, t.heading_color, t.divider_color, t.accent_color));
        connect(b, &QPushButton::clicked, dlg, [name, dlg]() {
            if (Service::instance().theme().name != name)
                Service::instance().set_theme(rep::themes::by_name(name));
            for (auto* other : dlg->findChildren<QPushButton*>())
                if (other->isCheckable())
                    other->setChecked(other->text() == name);
        });
        vl->addWidget(b);
    }
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
    connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::close);
    vl->addWidget(bb);
    dlg->show();
}

// ── Document info ───────────────────────────────────────────────────────────

void ReportBuilderScreen::show_metadata_dialog() {
    if (rbd_raise(meta_dlg_))
        return;
    const rep::ReportMetadata m = Service::instance().metadata();
    auto* dlg = rbd_dialog(this, tr("Document info"), 460);
    meta_dlg_ = dlg;
    auto* vl = new QVBoxLayout(dlg);
    auto* form = new QFormLayout();
    auto* title = new QLineEdit(m.title, dlg);
    auto* author = new QLineEdit(m.author, dlg);
    auto* company = new QLineEdit(m.company, dlg);
    company->setPlaceholderText(tr("Your company (optional)"));
    auto* date = new QLineEdit(m.date, dlg);
    date->setPlaceholderText(QStringLiteral("yyyy-MM-dd"));
    title->setToolTip(tr("{{variables}} work here, e.g. “{{symbol}} — Earnings preview”"));
    form->addRow(tr("Title"), title);
    form->addRow(tr("Author"), author);
    form->addRow(tr("Company"), company);
    form->addRow(tr("Date"), date);
    vl->addLayout(form);
    auto* identity = new QCheckBox(tr("Use this author and company for new reports"), dlg);
    identity->setChecked(!Service::default_company().isEmpty() && Service::default_company() == m.company &&
                         Service::default_author() == m.author);
    vl->addWidget(identity);
    auto* numbering = new QCheckBox(tr("Number the headings (1, 1.1, 1.1.1) — page, contents and exports"), dlg);
    numbering->setChecked(m.number_headings);
    vl->addWidget(numbering);
    auto* hf = new QLabel(tr("RUNNING HEADER / FOOTER"), dlg);
    hf->setProperty("rbRole", QStringLiteral("section"));
    vl->addWidget(hf);
    auto* hf_hint = new QLabel(tr("Placeholders: {page} {total} {title} {date} {author} {company}, and any "
                                  "{{variable}}"),
                               dlg);
    hf_hint->setProperty("rbRole", QStringLiteral("hint"));
    vl->addWidget(hf_hint);
    auto row3 = [dlg](const QString& l, const QString& c, const QString& r, QLineEdit** out) {
        auto* w = new QWidget(dlg);
        auto* hl = new QHBoxLayout(w);
        hl->setContentsMargins(0, 0, 0, 0);
        const QString vals[3] = {l, c, r};
        const QString ph[3] = {tr("Left"), tr("Centre"), tr("Right")};
        for (int i = 0; i < 3; ++i) {
            out[i] = new QLineEdit(vals[i], w);
            out[i]->setPlaceholderText(ph[i]);
            hl->addWidget(out[i]);
        }
        return w;
    };
    QLineEdit* head[3];
    QLineEdit* foot[3];
    auto* hform = new QFormLayout();
    hform->addRow(tr("Header"), row3(m.header_left, m.header_center, m.header_right, head));
    hform->addRow(tr("Footer"), row3(m.footer_left, m.footer_center, m.footer_right, foot));
    vl->addLayout(hform);
    auto* numbers = new QCheckBox(tr("Page numbers when the footer is empty (“3 / 12”)"), dlg);
    numbers->setChecked(m.show_page_numbers);
    vl->addWidget(numbers);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    vl->addWidget(bb);
    QLineEdit* h0 = head[0];
    QLineEdit* h1 = head[1];
    QLineEdit* h2 = head[2];
    QLineEdit* f0 = foot[0];
    QLineEdit* f1 = foot[1];
    QLineEdit* f2 = foot[2];
    connect(bb, &QDialogButtonBox::accepted, dlg, [=]() {
        rep::ReportMetadata nm = Service::instance().metadata(); // keeps page setup / font
        nm.title = title->text().trimmed().isEmpty() ? QStringLiteral("Untitled Report") : title->text().trimmed();
        nm.author = author->text();
        nm.company = company->text();
        nm.date = date->text();
        nm.header_left = h0->text();
        nm.header_center = h1->text();
        nm.header_right = h2->text();
        nm.footer_left = f0->text();
        nm.footer_center = f1->text();
        nm.footer_right = f2->text();
        nm.show_page_numbers = numbers->isChecked();
        nm.number_headings = numbering->isChecked();
        if (identity->isChecked())
            Service::set_identity_defaults(nm.author, nm.company);
        Service::instance().set_metadata(nm);
        dlg->close();
    });
    connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::close);
    dlg->show();
}

// ── Page setup ──────────────────────────────────────────────────────────────

void ReportBuilderScreen::show_page_setup_dialog() {
    if (rbd_raise(page_dlg_))
        return;
    const rep::ReportMetadata m = Service::instance().metadata();
    auto* dlg = rbd_dialog(this, tr("Page setup"), 380);
    page_dlg_ = dlg;
    auto* vl = new QVBoxLayout(dlg);
    auto* form = new QFormLayout();
    auto* paper = new QComboBox(dlg);
    paper->addItem(tr("A4 (210 × 297 mm)"), QStringLiteral("A4"));
    paper->addItem(tr("Letter (8.5 × 11 in)"), QStringLiteral("Letter"));
    paper->addItem(tr("Legal (8.5 × 14 in)"), QStringLiteral("Legal"));
    paper->setCurrentIndex(std::max(0, paper->findData(m.page_size)));
    auto* orient = new QComboBox(dlg);
    orient->addItem(tr("Portrait"), false);
    orient->addItem(tr("Landscape"), true);
    orient->setCurrentIndex(m.landscape ? 1 : 0);
    form->addRow(tr("Paper"), paper);
    form->addRow(tr("Orientation"), orient);
    auto margin = [dlg](double v) {
        auto* s = new QDoubleSpinBox(dlg);
        s->setRange(0, 60);
        s->setDecimals(1);
        s->setSingleStep(1);
        s->setSuffix(QStringLiteral(" mm"));
        s->setValue(v);
        return s;
    };
    auto* ml = margin(m.margin_left_mm);
    auto* mr = margin(m.margin_right_mm);
    auto* mt = margin(m.margin_top_mm);
    auto* mb = margin(m.margin_bottom_mm);
    form->addRow(tr("Margin left"), ml);
    form->addRow(tr("Margin right"), mr);
    form->addRow(tr("Margin top"), mt);
    form->addRow(tr("Margin bottom"), mb);
    vl->addLayout(form);
    auto* numbers = new QCheckBox(tr("Page numbers when the footer is empty"), dlg);
    numbers->setChecked(m.show_page_numbers);
    vl->addWidget(numbers);
    auto* marks = new QFormLayout();
    auto* watermark = new QComboBox(dlg);
    watermark->setEditable(true);
    watermark->addItems({QString(), QStringLiteral("DRAFT"), QStringLiteral("CONFIDENTIAL"), QStringLiteral("INTERNAL"),
                         QStringLiteral("SAMPLE")});
    watermark->setCurrentText(m.watermark);
    watermark->lineEdit()->setPlaceholderText(tr("None"));
    auto* confidential = new QLineEdit(m.confidentiality, dlg);
    confidential->setPlaceholderText(tr("e.g. Confidential — for internal use only"));
    marks->addRow(tr("Watermark"), watermark);
    marks->addRow(tr("Confidentiality line"), confidential);
    vl->addLayout(marks);
    auto* hint = new QLabel(tr("The pages on screen, the print view and the PDF all use this setup. The watermark is "
                               "drawn across every page of the PDF; the .odt puts it in the header."),
                            dlg);
    hint->setProperty("rbRole", QStringLiteral("hint"));
    hint->setWordWrap(true);
    vl->addWidget(hint);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    vl->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, dlg, [=]() {
        rep::ReportMetadata nm = Service::instance().metadata();
        nm.page_size = paper->currentData().toString();
        nm.landscape = orient->currentData().toBool();
        nm.margin_left_mm = ml->value();
        nm.margin_right_mm = mr->value();
        nm.margin_top_mm = mt->value();
        nm.margin_bottom_mm = mb->value();
        nm.show_page_numbers = numbers->isChecked();
        nm.watermark = watermark->currentText().trimmed();
        nm.confidentiality = confidential->text().trimmed();
        Service::instance().set_metadata(nm);
        dlg->close();
    });
    connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::close);
    dlg->show();
}

// ── Save as template ────────────────────────────────────────────────────────

void ReportBuilderScreen::show_save_template_dialog() {
    if (rbd_raise(template_dlg_))
        return;
    auto* dlg = rbd_dialog(this, tr("Save as template"), 420);
    template_dlg_ = dlg;
    auto* vl = new QVBoxLayout(dlg);
    auto* form = new QFormLayout();
    auto* name = new QLineEdit(Service::instance().metadata().title, dlg);
    auto* desc = new QLineEdit(dlg);
    desc->setPlaceholderText(tr("What it is for (optional)"));
    form->addRow(tr("Name"), name);
    form->addRow(tr("Description"), desc);
    vl->addLayout(form);
    auto* hint = new QLabel(tr("The whole report is kept as a starting point — live data blocks keep their "
                               "recipes and refresh in every report made from it. Templates appear on the library page "
                               "under “My Templates”."),
                            dlg);
    hint->setWordWrap(true);
    hint->setProperty("rbRole", QStringLiteral("hint"));
    vl->addWidget(hint);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, dlg);
    vl->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, dlg, [this, dlg, name, desc]() {
        const QString n = name->text().trimmed();
        if (n.isEmpty()) {
            name->setFocus();
            return;
        }
        properties_->flush_pending();
        QPointer<ReportBuilderScreen> self(this);
        Library::instance().save_user_template(n, desc->text().trimmed(), Service::instance().snapshot().to_json(), this,
                                               [self, n](bool ok, const QString& error) {
                                                   if (!self)
                                                       return;
                                                   if (ok)
                                                       self->show_notice(NoticeKind::Info,
                                                                         tr("Saved as the template “%1”.").arg(n),
                                                                         QString(), "ok");
                                                   else
                                                       self->show_notice(NoticeKind::Error,
                                                                         tr("Could not save the template: %1").arg(error),
                                                                         QString(), "bad");
                                               });
        dlg->close();
    });
    connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::close);
    dlg->show();
}

// ── File operations ─────────────────────────────────────────────────────────

bool ReportBuilderScreen::confirm_leave_file(const QString& title) {
    auto& svc = Service::instance();
    const QString file = svc.current_file();
    if (file.isEmpty() || !svc.file_dirty())
        return true; // the library copy is current: nothing can be lost
    properties_->flush_pending();
    const auto answer = QMessageBox::question(
        this, title,
        tr("Save the changes to “%1”?\n\nThe report library keeps them either way; this only decides whether "
           "the file is updated too.")
            .arg(QFileInfo(file).fileName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Cancel)
        return false;
    if (answer == QMessageBox::Save) {
        auto r = svc.save_to(file);
        if (r.is_err()) {
            show_notice(NoticeKind::Error, tr("Could not save %1: %2").arg(file, QString::fromStdString(r.error())),
                        QString(), "bad");
            return false;
        }
        services::report_files::publish(file);
    }
    return true;
}

void ReportBuilderScreen::on_new() {
    if (!confirm_leave_file(tr("New report")))
        return;
    properties_->flush_pending();
    Service::instance().clear_document();
    show_editor();
}

void ReportBuilderScreen::on_open() {
    const QString path = QFileDialog::getOpenFileName(this, tr("Open report"), QString(),
                                                      tr("Fincept Report (*.fincept);;JSON (*.json)"));
    if (!path.isEmpty())
        open_file_async(path);
}

void ReportBuilderScreen::on_import() {
    on_open();
}

void ReportBuilderScreen::open_file_async(const QString& path) {
    // Read and parse off the GUI thread; only the swap happens here.
    QPointer<ReportBuilderScreen> self(this);
    (void)QtConcurrent::run([self, path]() {
        QString error;
        rep::ReportDocument doc;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            error = f.errorString();
        } else {
            auto r = rep::ReportDocument::from_json(QString::fromUtf8(f.readAll()));
            if (r.is_ok()) {
                doc = r.value();
                services::report_assets::materialize(doc); // pictures carried in the report
            } else
                error = QString::fromStdString(r.error());
        }
        QMetaObject::invokeMethod(
            qApp,
            [self, path, doc, error]() {
                if (!self)
                    return;
                if (!error.isEmpty()) {
                    self->show_notice(NoticeKind::Error,
                                      ReportBuilderScreen::tr("Could not open %1: %2").arg(path, error), QString(), "bad");
                    return;
                }
                if (!self->confirm_leave_file(ReportBuilderScreen::tr("Open report")))
                    return;
                Service::instance().open_file_document(doc, path);
                self->show_editor();
            },
            Qt::QueuedConnection);
    });
}

void ReportBuilderScreen::on_save() {
    properties_->flush_pending();
    auto& svc = Service::instance();
    const QString file = svc.current_file();
    if (!file.isEmpty()) {
        auto r = svc.save_to(file);
        if (r.is_err()) {
            show_notice(NoticeKind::Error, tr("Could not save to %1: %2").arg(file, QString::fromStdString(r.error())),
                        QString(), "bad");
            return;
        }
        // One entry in Files per path (refreshed, not re-imported on every save).
        services::report_files::publish(file);
    } else {
        svc.save_to_library();
        show_notice(NoticeKind::Info, tr("Saved to the report library. Save As also writes a .fincept file."), QString(),
                    "ok");
        QTimer::singleShot(4000, this, [this]() { clear_notice(NoticeKind::Info); });
    }
    if (auto* stack = svc.undo_stack())
        stack->setClean();
    update_title();
}

void ReportBuilderScreen::on_save_as() {
    properties_->flush_pending();
    auto& svc = Service::instance();
    QString path = QFileDialog::getSaveFileName(this, tr("Save report as"), svc.metadata().title,
                                                tr("Fincept Report (*.fincept);;JSON (*.json)"));
    if (path.isEmpty())
        return;
    // QFileDialog does not append the filter suffix on every platform.
    if (!path.endsWith(QLatin1String(".fincept"), Qt::CaseInsensitive) &&
        !path.endsWith(QLatin1String(".json"), Qt::CaseInsensitive))
        path += QLatin1String(".fincept");
    auto r = svc.fork_to_file(path);
    if (r.is_err()) {
        show_notice(NoticeKind::Error, tr("Could not save to %1: %2").arg(path, QString::fromStdString(r.error())),
                    QString(), "bad");
        return;
    }
    services::report_files::publish(path);
    update_title();
}

void ReportBuilderScreen::open_library_report(qint64 id, int select_block_id) {
    auto& svc = Service::instance();
    if (id <= 0 || id == svc.library_id()) {
        show_editor();
        if (select_block_id > 0)
            select_component(select_block_id, true);
        return;
    }
    if (!confirm_leave_file(tr("Open report")))
        return;
    QPointer<ReportBuilderScreen> self(this);
    Library::instance().load(id, this,
                             [self, id, select_block_id](bool ok, const rep::ReportDocument& doc, const QString&,
                                                         const QString& error) {
                                 if (!self)
                                     return;
                                 if (!ok) {
                                     self->show_notice(NoticeKind::Error, error, QString(), "bad");
                                     return;
                                 }
                                 Service::instance().open_document(doc, id, QString());
                                 self->show_editor();
                                 if (select_block_id > 0)
                                     QTimer::singleShot(0, self, [self, select_block_id]() {
                                         if (self)
                                             self->select_component(select_block_id, true);
                                     });
                             });
}

void ReportBuilderScreen::apply_template(const QString& template_id, const QVariantMap& params) {
    if (!confirm_leave_file(tr("New report from template")))
        return;
    properties_->flush_pending();
    auto refresh_new = []() {
        // Self-filling templates fetch right away (the user just asked for this report).
        QTimer::singleShot(0, &Binder::instance(), []() { Binder::instance().refresh_all(); });
    };
    if (template_id.startsWith(QLatin1String("user:"))) {
        QPointer<ReportBuilderScreen> self(this);
        Library::instance().load_user_template(
            template_id, this,
            [self, refresh_new](bool ok, const rep::ReportDocument& tpl, const QString&, const QString& error) {
                if (!self)
                    return;
                if (!ok) {
                    self->show_notice(NoticeKind::Error, error, QString(), "bad");
                    return;
                }
                rep::ReportDocument doc = tpl;
                doc.metadata.date = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd"));
                Service::instance().open_document(doc, 0, QString());
                self->show_editor();
                bool live = false;
                for (const auto& c : doc.components)
                    live = live || rep::is_bound(c);
                if (live)
                    refresh_new();
            });
        return;
    }
    Service::instance().apply_template(template_id, params);
    show_editor();
    for (const auto& t : services::report_templates::catalogue()) {
        if (t.name == template_id && t.live) {
            refresh_new();
            break;
        }
    }
}

void ReportBuilderScreen::handle_open_request() {
    Service::OpenRequest req;
    if (!Service::instance().take_open_request(&req))
        return;
    if (!req.file_path.isEmpty()) {
        open_file_async(req.file_path);
        return;
    }
    open_library_report(req.library_id, req.select_block_id);
    if (req.select_block_id > 0 && Service::instance().unseen_ids().contains(req.select_block_id)) {
        Service::instance().mark_inbox_seen();
        clear_notice(NoticeKind::Inbox);
    }
}

// ── Exports ─────────────────────────────────────────────────────────────────

bool ReportBuilderScreen::confirm_unfilled(const QString& title) {
    QStringList names;
    for (const auto& c : Service::instance().components()) {
        if (!rb::is_unfilled(c))
            continue;
        QString label = c.type;
        label.replace(QLatin1Char('_'), QLatin1Char(' '));
        if (!label.isEmpty())
            label[0] = label[0].toUpper();
        names << label;
    }
    if (names.isEmpty())
        return true;
    const auto answer = QMessageBox::question(
        this, title,
        tr("%n empty block(s) will be left out of the export: %1", nullptr, static_cast<int>(names.size()))
            .arg(names.join(QStringLiteral(", "))),
        QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Ok);
    return answer == QMessageBox::Ok;
}

void ReportBuilderScreen::on_export_pdf() {
    on_export(ReportExportJob::Format::Pdf);
}

void ReportBuilderScreen::on_export_html() {
    on_export(ReportExportJob::Format::Html);
}

void ReportBuilderScreen::export_pdf_to(const QString& path, ExportDone done) {
    start_export(ReportExportJob::Format::Pdf, path, std::move(done));
}

void ReportBuilderScreen::export_html_to(const QString& path, ExportDone done) {
    start_export(ReportExportJob::Format::Html, path, std::move(done));
}

void ReportBuilderScreen::start_export(ReportExportJob::Format format, const QString& path, ExportDone done) {
    if (path.isEmpty() || (export_job_ && export_job_->running())) {
        if (done)
            done(false, path, tr("Another export is still running."));
        return;
    }
    auto* job = new ReportExportJob(format, path, Service::instance().snapshot(), this);
    export_job_ = job;
    export_btn_->setEnabled(false);
    const QString kind = ReportExportJob::display_name(format);
    export_lbl_->setText(tr("Exporting %1…").arg(kind));
    export_progress_->setValue(0);
    export_bar_->show();
    connect(job, &ReportExportJob::progress, this, [this, kind](const QString& text, int percent) {
        export_lbl_->setText(tr("Exporting %1 — %2").arg(kind, text));
        export_progress_->setValue(percent);
    });
    connect(job, &ReportExportJob::finished, this,
            [this, job, kind, done](bool ok, bool cancelled, const QString& out, const QString& error) {
                export_bar_->hide();
                export_btn_->setEnabled(true);
                job->deleteLater();
                if (ok) {
                    services::report_files::publish(out); // one Files entry per path
                    notice_path_ = out;
                    QString text = tr("%1 saved: %2").arg(kind, QDir::toNativeSeparators(out));
                    if (out.endsWith(QLatin1String(".md"), Qt::CaseInsensitive))
                        text += QLatin1Char(' ') + tr("(pictures in the folder %1_files next to it)")
                                                       .arg(QFileInfo(out).completeBaseName());
                    show_notice(NoticeKind::Export, text, tr("OPEN"), "ok");
                } else if (!cancelled) {
                    show_notice(NoticeKind::Error, tr("Export failed: %1").arg(error), QString(), "bad");
                }
                if (done)
                    done(ok, out, error);
            });
    job->start();
}

} // namespace fincept::screens
