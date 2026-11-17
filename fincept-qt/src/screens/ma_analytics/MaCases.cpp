// MaCases.cpp — MA-8: named cases. SAVE CASE stores the visible analysis's inputs (and the module's company /
// deal-grid inputs) with the result on screen; CASES lists the module's saved cases to reopen (inputs restored, the
// analysis re-run), to compare with the result on screen (headline figures side by side with the difference), or to
// delete. Storage: services::ma::MaCaseStore (SQL on a worker thread).
#include "screens/ma_analytics/MAModulePanel.h"

#include "screens/ma_analytics/MaResultFormat.h"
#include "services/ma_analytics/MaCaseStore.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/InputMemory.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QLabel>
#include <QMenu>
#include <QPlainTextEdit>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens {

using fincept::services::ma::MaCase;
using fincept::services::ma::MaCaseStore;

namespace {

/// The value at a dotted path of a flattened result ("accretion_dilution.pro_forma_eps").
QJsonValue ma_case_at(const QJsonObject& root, const QString& path) {
    QJsonValue v = root;
    for (const QString& part : path.split(QLatin1Char('.'))) {
        if (!v.isObject())
            return {};
        v = v.toObject().value(part);
    }
    return v;
}

} // namespace

void MAModulePanel::build_case_buttons(QHBoxLayout* header) {
    if (module_.id == services::ma::ModuleId::Deals)
        return; // the deal database keeps its own records
    auto make = [this](const QString& text, const QString& tip) {
        auto* b = new QPushButton(text, header_bar_);
        b->setObjectName(QStringLiteral("maCaseBtn"));
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(tip);
        b->setStyleSheet(QString("QPushButton#maCaseBtn { background:transparent; color:%1; border:1px solid %2;"
                                 " padding:2px 10px; font-size:%3px; font-weight:600; }"
                                 "QPushButton#maCaseBtn:hover { color:%4; border-color:%5; }")
                             .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED())
                             .arg(ui::fonts::px(-3))
                             .arg(ui::colors::TEXT_PRIMARY(), ui::colors::AMBER()));
        return b;
    };
    save_case_btn_ = make(tr("SAVE CASE"), tr("Save the inputs and result of the analysis on screen under a name"));
    cases_btn_ = make(tr("CASES"), tr("Open, compare or delete saved cases of this module"));
    connect(save_case_btn_, &QPushButton::clicked, this, &MAModulePanel::save_case);
    connect(cases_btn_, &QPushButton::clicked, this, &MAModulePanel::show_cases_menu);
    header->addWidget(save_case_btn_);
    header->addWidget(cases_btn_);
}

void MAModulePanel::save_case() {
    if (!pane_)
        return;
    const QString context = pane_->view();
    ui::InputMemory* mem = memories_.value(context);
    if (context.isEmpty() || !mem)
        return;
    bool ok = false;
    const QString suggested = tab_title(sub_tabs_ ? sub_tabs_->currentIndex() : -1) + QStringLiteral(" ") +
                              QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    const QString name = QInputDialog::getText(this, tr("Save case"), tr("Name of this case:"), QLineEdit::Normal,
                                               suggested, &ok)
                             .trimmed();
    if (!ok || name.isEmpty())
        return;
    MaCase c;
    c.name = name.left(120);
    c.module = module_.key;
    c.context = context;
    c.inputs.insert(QStringLiteral("view"), QJsonObject::fromVariantMap(mem->values()));
    if (module_memory_)
        c.inputs.insert(QStringLiteral("module"), QJsonObject::fromVariantMap(module_memory_->values()));
    if (pane_->has_result())
        c.result = pane_->payload().toObject();
    status_label_->setText(tr("Saving case…"));
    MaCaseStore::instance().save(c, this, [this](bool done, const MaCase& saved, const QString& error) {
        status_label_->setText(done ? tr("Case \"%1\" saved").arg(saved.name) : error);
    });
}

void MAModulePanel::show_cases_menu() {
    status_label_->setText(tr("Reading saved cases…"));
    MaCaseStore::instance().list(module_.key, this, [this](bool ok, const QVector<MaCase>& cases, const QString& error) {
        if (!ok) {
            status_label_->setText(error);
            return;
        }
        status_label_->setText(QString());
        auto* menu = new QMenu(this);
        menu->setAttribute(Qt::WA_DeleteOnClose);
        if (cases.isEmpty()) {
            menu->addAction(tr("No saved cases yet - use SAVE CASE"))->setEnabled(false);
        } else {
            QMenu* open = menu->addMenu(tr("Open"));
            QMenu* cmp = menu->addMenu(tr("Compare with the result on screen"));
            QMenu* del = menu->addMenu(tr("Delete"));
            const QString view = pane_ ? pane_->view() : QString();
            for (const MaCase& c : cases) {
                QString title = c.name;
                title.replace(QLatin1Char('&'), QLatin1String("&&"));
                const QString when = QDateTime::fromMSecsSinceEpoch(c.updated_at).toString(QStringLiteral("dd MMM HH:mm"));
                open->addAction(title + QStringLiteral("  ·  ") + when, this, [this, c]() { load_case(c); });
                QAction* a = cmp->addAction(title, this, [this, c]() { compare_case(c); });
                a->setEnabled(c.context == view && !c.result.isEmpty());
                del->addAction(title, this, [this, c]() {
                    MaCaseStore::instance().remove(c.id, this, [this, c](bool done, const QString& error) {
                        status_label_->setText(done ? tr("Case \"%1\" deleted").arg(c.name) : error);
                    });
                });
            }
        }
        menu->popup(cases_btn_ ? cases_btn_->mapToGlobal(QPoint(0, cases_btn_->height())) : QCursor::pos());
    });
}

void MAModulePanel::load_case(const MaCase& c) {
    if (!sub_tabs_)
        return;
    int index = -1;
    for (int i = 0; i < sub_tabs_->count(); ++i)
        if (context_of_tab(i) == c.context)
            index = i;
    if (index < 0) {
        status_label_->setText(tr("This case belongs to an analysis that no longer exists."));
        return;
    }
    build_all_sub_tabs();
    auto apply = [this](const QJsonObject& values) {
        for (auto it = values.begin(); it != values.end(); ++it) {
            const QString key = it.key();
            if (key.startsWith(QLatin1Char('@')))
                continue; // positional inputs are not portable between layouts
            if (auto* d = double_inputs_.value(key, nullptr)) {
                d->setValue(it.value().toVariant().toDouble());
            } else if (auto* i = int_inputs_.value(key, nullptr)) {
                i->setValue(it.value().toVariant().toInt());
            } else if (auto* cb = combo_inputs_.value(key, nullptr)) {
                const QString t = it.value().toVariant().toString();
                if (cb->isEditable())
                    cb->setEditText(t);
                else if (cb->findText(t) >= 0)
                    cb->setCurrentText(t);
            } else if (auto* box = findChild<ui::SymbolSearchBox*>(key)) {
                box->set_text(it.value().toVariant().toString());
            } else if (auto* mirror = findChild<QPlainTextEdit*>(key)) {
                mirror->setPlainText(it.value().toVariant().toString()); // a deal grid's rows
            }
        }
    };
    apply(c.inputs.value(QStringLiteral("module")).toObject());
    apply(c.inputs.value(QStringLiteral("view")).toObject());
    sub_tabs_->setCurrentIndex(index);
    status_label_->setText(tr("Case \"%1\" opened").arg(c.name));
    if (pane_ && pane_->view() == c.context)
        pane_->run(); // the inputs are restored; the result is computed again from them
}

void MAModulePanel::compare_case(const MaCase& c) {
    if (!pane_ || !pane_->has_result())
        return;
    const QString context = pane_->view();
    const QJsonObject now = ma_fmt::ma_flatten_result(pane_->payload().toObject());
    const QJsonObject then = ma_fmt::ma_flatten_result(c.result);
    const QString intrinsic = module_.id == services::ma::ModuleId::Deals ? QStringLiteral("USD") : QString();
    const fincept::ui::result::Options o = ma_fmt::ma_result_options(context, now, intrinsic);
    QStringList paths = ma_fmt::ma_headline_paths(context, now);
    for (const QString& p : ma_fmt::ma_headline_paths(context, then))
        if (!paths.contains(p))
            paths << p;

    auto* model = new QStandardItemModel(0, 4, this);
    model->setHorizontalHeaderLabels({tr("Figure"), tr("On screen"), c.name, tr("Difference")});
    for (const QString& path : paths) {
        const QJsonValue a = ma_case_at(now, path);
        const QJsonValue b = ma_case_at(then, path);
        if (a.isUndefined() && b.isUndefined())
            continue;
        const QString key = path.section(QLatin1Char('.'), -1);
        const double av = a.isDouble() ? a.toDouble() : std::nan("");
        const double bv = b.isDouble() ? b.toDouble() : std::nan("");
        const auto unit = fincept::ui::result::unit_for(o, path, key, std::isfinite(av) ? av : bv);
        QList<QStandardItem*> row;
        row << new QStandardItem(fincept::ui::result::humanize(key));
        row << new QStandardItem(a.isUndefined() ? QStringLiteral("-") : fincept::ui::result::format_value(a, unit, o));
        row << new QStandardItem(b.isUndefined() ? QStringLiteral("-") : fincept::ui::result::format_value(b, unit, o));
        row << new QStandardItem(std::isfinite(av) && std::isfinite(bv)
                                     ? fincept::ui::result::format_number(av - bv, unit, o, /*sign=*/true)
                                     : QString());
        for (int i = 1; i < row.size(); ++i)
            row[i]->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        model->appendRow(row);
    }

    auto* dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setObjectName(QStringLiteral("maCompareDialog"));
    dlg->setWindowTitle(tr("Compare with \"%1\"").arg(c.name));
    dlg->setStyleSheet(QString("QDialog#maCompareDialog { background:%1; }"
                               "QLabel#maCompareNote { color:%2; font-size:%3px; }"
                               "QTableView#maCompareTable { background:%4; color:%5; gridline-color:%6; border:1px solid %6; }")
                           .arg(ui::colors::BG_BASE(), ui::colors::TEXT_SECONDARY())
                           .arg(ui::fonts::px(-2))
                           .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM()));
    auto* vl = new QVBoxLayout(dlg);
    auto* note = new QLabel(tr("Headline figures of the result on screen against the saved case (saved %1). "
                               "Difference = on screen minus saved.")
                                .arg(QDateTime::fromMSecsSinceEpoch(c.updated_at).toString(QStringLiteral("dd MMM yyyy HH:mm"))),
                            dlg);
    note->setObjectName(QStringLiteral("maCompareNote"));
    note->setWordWrap(true);
    vl->addWidget(note);
    auto* view = new QTableView(dlg);
    view->setObjectName(QStringLiteral("maCompareTable"));
    model->setParent(view);
    view->setModel(model);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->verticalHeader()->setVisible(false);
    view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int i = 1; i < 4; ++i)
        view->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch);
    vl->addWidget(view, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::close);
    vl->addWidget(buttons);
    const QFontMetrics fm(ui::fonts::font(-1));
    dlg->resize(std::max(560, fm.averageCharWidth() * 90), std::min(560, (model->rowCount() + 5) * (fm.height() + 12)));
    dlg->show();
}

} // namespace fincept::screens
