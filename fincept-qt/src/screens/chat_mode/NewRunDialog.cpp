// NewRunDialog.cpp — NEW RUN (WP6): templates, parameters, engine, budget,
// inline cost confirmation, start now / schedule. See the header.

#include "screens/chat_mode/NewRunDialog.h"

#include "screens/chat_mode/ChatModeService.h"
#include "screens/chat_mode/RunRequestForm.h"
#include "screens/chat_mode/RunViewLogic.h"
#include "screens/chat_mode/RunViewWidgets.h"
#include "services/agent_tasks/RunTemplateLibrary.h"
#include "services/agent_tasks/TaskRunService.h"
#include "ui/theme/Theme.h"

#include <QCloseEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace fincept::chat_mode {

namespace agt = fincept::agent_tasks;
namespace rv = fincept::chat_mode::run_view;

namespace new_run_dialog_detail {

constexpr const char* kNrdLastKey = "chat_mode/new_run/last";
constexpr int kNrdIdRole = Qt::UserRole + 1;

QString nrd_style() {
    return QStringLiteral(
               "#newRunDialog{background:%1;}"
               "#newRunDialog QLabel{background:transparent;border:none;color:%2;font-size:%3px;}"
               "#newRunDialog QLabel#nrdSection{color:%4;font-weight:700;letter-spacing:1px;}"
               "#newRunDialog QLabel#nrdProblems{color:%5;}"
               "#newRunDialog QLabel#nrdStatus{color:%4;}"
               "#newRunDialog QListWidget{background:%6;color:%7;border:1px solid %8;outline:none;font-size:%3px;}"
               "#newRunDialog QListWidget::item{padding:5px 8px;}"
               "#newRunDialog QListWidget::item:selected{background:%9;color:%7;border-left:2px solid %10;}")
               .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(), ui::fonts::px(-2), ui::colors::TEXT_TERTIARY(),
                    ui::colors::DANGER(), ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(),
                    ui::colors::BG_HOVER())
               .arg(ui::colors::AMBER()) +
           QStringLiteral(
               "#newRunDialog QPushButton{background:%1;color:%2;border:1px solid %3;padding:5px 14px;font-size:%4px;"
               "font-weight:700;}"
               "#newRunDialog QPushButton:hover{color:%5;border-color:%6;}"
               "#newRunDialog QPushButton:focus{border-color:%6;}"
               "#newRunDialog QPushButton:disabled{color:%7;background:transparent;}"
               "#newRunDialog QPushButton#nrdPrimary{background:%6;color:%8;border-color:%6;}"
               "#newRunDialog QPushButton#nrdDanger{color:%9;border-color:%9;}"
               "#newRunDialog QFrame#nrdConfirm{background:%10;border:1px solid %11;}"
               "#newRunDialog QFrame#nrdInline{background:%1;border:1px solid %3;}"
               "#newRunDialog QLineEdit{background:%12;color:%5;border:1px solid %3;padding:3px 6px;font-size:%4px;}"
               "#newRunDialog QScrollArea{background:transparent;border:none;}")
               .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(), ui::fonts::px(-2),
                    ui::colors::TEXT_PRIMARY(), ui::colors::AMBER(), ui::colors::TEXT_DIM(), ui::colors::TEXT_ON_ACCENT(),
                    ui::colors::DANGER())
               .arg(ui::colors::tint(QColor(ui::colors::WARNING()), 0.14), ui::colors::WARNING(), ui::colors::BG_BASE());
}

QPushButton* nrd_button(QWidget* parent, const char* name = nullptr) {
    auto* b = new QPushButton(parent);
    b->setCursor(Qt::PointingHandCursor);
    b->setAutoDefault(false); // Enter in a field never starts a run
    if (name)
        b->setObjectName(QLatin1String(name));
    return b;
}

} // namespace new_run_dialog_detail

using namespace new_run_dialog_detail;

struct NewRunDialog::Impl {
    NewRunDialog* q = nullptr;
    QString session_uuid;
    QJsonObject extra_context;
    double balance = -1;
    bool confirmed = false;
    bool starting = false;
    quint64 start_request = 0; ///< a preparation answering an older START is dropped

    QListWidget* list = nullptr;
    QPushButton* save_tpl = nullptr;
    QPushButton* delete_tpl = nullptr;
    QFrame* save_frame = nullptr;
    QLineEdit* save_name = nullptr;
    QPushButton* save_ok = nullptr;
    QPushButton* save_cancel = nullptr;
    QFrame* delete_frame = nullptr;
    QLabel* delete_text = nullptr;
    QPushButton* delete_yes = nullptr;
    QPushButton* delete_no = nullptr;
    RunRequestForm* form = nullptr;
    QLabel* problems = nullptr;
    QFrame* confirm = nullptr;
    QLabel* confirm_text = nullptr;
    QPushButton* confirm_yes = nullptr;
    QPushButton* confirm_change = nullptr;
    QLabel* status = nullptr;
    QPushButton* schedule_btn = nullptr;
    QPushButton* cancel_btn = nullptr;
    QPushButton* start_btn = nullptr;

    void build();
    void wire();
    void retranslate();
    void fill_list();
    void select_template(const QString& id);
    void validate();
    void start();
    void remember() const;
    void restore();
    void save_template();
};

void NewRunDialog::Impl::build() {
    q->setObjectName(QStringLiteral("newRunDialog"));
    q->setStyleSheet(nrd_style()); // before the children exist
    q->setModal(false);
    auto* outer = new QVBoxLayout(q);
    outer->setContentsMargins(14, 12, 14, 12);
    outer->setSpacing(8);

    auto* body = new QHBoxLayout;
    body->setSpacing(12);

    auto* left = new QVBoxLayout;
    left->setSpacing(6);
    auto* tl = new QLabel(q);
    tl->setObjectName(QStringLiteral("nrdSection"));
    tl->setText(NewRunDialog::tr("TEMPLATES"));
    left->addWidget(tl);
    list = new QListWidget(q);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setWordWrap(true);
    left->addWidget(list, 1);
    auto* tpl_row = new QHBoxLayout;
    save_tpl = nrd_button(q);
    delete_tpl = nrd_button(q, "nrdDanger");
    delete_tpl->setProperty("fincept_agent_risk", "delete"); // agents: deletes the run template
    tpl_row->addWidget(save_tpl);
    tpl_row->addWidget(delete_tpl);
    tpl_row->addStretch();
    left->addLayout(tpl_row);
    save_frame = new QFrame(q);
    save_frame->setObjectName(QStringLiteral("nrdInline"));
    auto* sf = new QHBoxLayout(save_frame);
    sf->setContentsMargins(6, 4, 6, 4);
    save_name = new QLineEdit(save_frame);
    save_name->setMaxLength(80);
    save_ok = nrd_button(save_frame, "nrdPrimary");
    save_cancel = nrd_button(save_frame);
    sf->addWidget(save_name, 1);
    sf->addWidget(save_ok);
    sf->addWidget(save_cancel);
    save_frame->setVisible(false);
    left->addWidget(save_frame);
    delete_frame = new QFrame(q);
    delete_frame->setObjectName(QStringLiteral("nrdConfirm"));
    auto* df = new QVBoxLayout(delete_frame);
    df->setContentsMargins(6, 4, 6, 4);
    delete_text = new QLabel(delete_frame);
    delete_text->setWordWrap(true);
    df->addWidget(delete_text);
    auto* dr = new QHBoxLayout;
    delete_yes = nrd_button(delete_frame, "nrdDanger");
    delete_yes->setProperty("fincept_agent_risk", "delete"); // agents: confirms deleting the run template
    delete_no = nrd_button(delete_frame);
    dr->addWidget(delete_yes);
    dr->addWidget(delete_no);
    dr->addStretch();
    df->addLayout(dr);
    delete_frame->setVisible(false);
    left->addWidget(delete_frame);
    body->addLayout(left, 2);

    auto* scroll = new QScrollArea(q);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    form = new RunRequestForm(scroll, /*for_schedule=*/false);
    scroll->setWidget(form);
    body->addWidget(scroll, 5);
    outer->addLayout(body, 1);

    problems = new QLabel(q);
    problems->setObjectName(QStringLiteral("nrdProblems"));
    problems->setWordWrap(true);
    problems->setTextFormat(Qt::PlainText);
    problems->setVisible(false);
    outer->addWidget(problems);

    confirm = new QFrame(q);
    confirm->setObjectName(QStringLiteral("nrdConfirm"));
    auto* cf = new QHBoxLayout(confirm);
    cf->setContentsMargins(8, 6, 8, 6);
    confirm_text = new QLabel(confirm);
    confirm_text->setWordWrap(true);
    confirm_yes = nrd_button(confirm, "nrdPrimary");
    confirm_change = nrd_button(confirm);
    cf->addWidget(confirm_text, 1);
    cf->addWidget(confirm_yes);
    cf->addWidget(confirm_change);
    confirm->setVisible(false);
    outer->addWidget(confirm);

    status = new QLabel(q);
    status->setObjectName(QStringLiteral("nrdStatus"));
    status->setWordWrap(true);
    status->setVisible(false);
    outer->addWidget(status);

    auto* buttons = new QHBoxLayout;
    schedule_btn = nrd_button(q);
    cancel_btn = nrd_button(q);
    start_btn = nrd_button(q, "nrdPrimary");
    start_btn->setProperty("fincept_agent_risk", "spend"); // agents: starts an agent run (model quota)
    buttons->addWidget(schedule_btn);
    buttons->addStretch();
    buttons->addWidget(cancel_btn);
    buttons->addWidget(start_btn);
    outer->addLayout(buttons);

    const QFontMetrics fm(ui::fonts::font(-2));
    q->resize(fm.averageCharWidth() * 120, fm.lineSpacing() * 40);
    q->setMinimumSize(fm.averageCharWidth() * 70, fm.lineSpacing() * 24);
}

void NewRunDialog::Impl::wire() {
    QObject::connect(list, &QListWidget::currentItemChanged, q, [this](QListWidgetItem* item) {
        if (!item)
            return;
        const QVariant id = item->data(kNrdIdRole);
        if (!id.isValid())
            return; // a section header
        if (id.toString() == form->template_id()) {
            validate(); // already shown (restored inputs stay)
            return;
        }
        form->set_template(id.toString());
        confirmed = false;
        save_frame->setVisible(false);
        delete_frame->setVisible(false);
        validate();
    });
    QObject::connect(form, &RunRequestForm::changed, q, [this]() {
        confirmed = false; // the question was about the old inputs
        confirm->setVisible(false);
        validate();
    });
    QObject::connect(&agt::RunTemplateLibrary::instance(), &agt::RunTemplateLibrary::templates_changed, q,
                     [this]() { fill_list(); });
    // Section headers carry a theme colour and font: rebuilt once a new appearance settled.
    QObject::connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, q, [this]() { fill_list(); });
    QObject::connect(start_btn, &QPushButton::clicked, q, [this]() { start(); });
    QObject::connect(cancel_btn, &QPushButton::clicked, q, [this]() { q->close(); });
    QObject::connect(confirm_yes, &QPushButton::clicked, q, [this]() {
        confirmed = true;
        confirm->setVisible(false);
        start();
    });
    QObject::connect(confirm_change, &QPushButton::clicked, q, [this]() {
        confirm->setVisible(false);
        form->setFocus();
    });
    QObject::connect(schedule_btn, &QPushButton::clicked, q, [this]() {
        const QStringList p = form->problems();
        if (!p.isEmpty()) {
            validate();
            return;
        }
        const agt::RunRequest r = form->request();
        QJsonObject draft;
        draft[QStringLiteral("template_id")] = form->refill_template_id().isEmpty() ? r.template_id : form->refill_template_id();
        draft[QStringLiteral("prompt")] = form->edited_prompt();
        draft[QStringLiteral("engine")] = r.engine;
        draft[QStringLiteral("mode")] = r.mode;
        draft[QStringLiteral("plan_first")] = r.plan_first;
        QJsonObject b;
        b[QStringLiteral("max_credits")] = r.budget.max_credits;
        b[QStringLiteral("max_steps")] = r.budget.max_steps;
        b[QStringLiteral("max_minutes")] = r.budget.max_minutes;
        draft[QStringLiteral("budget")] = b;
        draft[QStringLiteral("context")] = r.context;
        draft[QStringLiteral("name")] = r.title.isEmpty() ? r.prompt.section(QLatin1Char('\n'), 0, 0).left(60) : r.title;
        draft[QStringLiteral("form")] = form->values();
        remember();
        emit q->schedule_requested(draft);
        q->close();
    });
    QObject::connect(save_tpl, &QPushButton::clicked, q, [this]() {
        const auto t = form->current_template();
        save_name->setText(t ? (t->user_defined ? t->title : NewRunDialog::tr("%1 (mine)").arg(t->title))
                             : NewRunDialog::tr("My task"));
        save_frame->setVisible(true);
        delete_frame->setVisible(false);
        save_name->setFocus();
        save_name->selectAll();
    });
    QObject::connect(save_cancel, &QPushButton::clicked, q, [this]() { save_frame->setVisible(false); });
    QObject::connect(save_ok, &QPushButton::clicked, q, [this]() { save_template(); });
    QObject::connect(save_name, &QLineEdit::returnPressed, q, [this]() { save_template(); });
    QObject::connect(delete_tpl, &QPushButton::clicked, q, [this]() {
        const auto t = form->current_template();
        if (!t || !t->user_defined)
            return;
        delete_text->setText(NewRunDialog::tr("Delete your template “%1”? Schedules that use it keep their last task "
                                              "text.")
                                 .arg(t->title));
        delete_frame->setVisible(true);
        save_frame->setVisible(false);
        delete_yes->setFocus();
    });
    QObject::connect(delete_no, &QPushButton::clicked, q, [this]() { delete_frame->setVisible(false); });
    QObject::connect(delete_yes, &QPushButton::clicked, q, [this]() {
        const QString id = form->template_id();
        delete_frame->setVisible(false);
        QPointer<NewRunDialog> self(q);
        agt::RunTemplateLibrary::instance().delete_user_template(id, q, [self](bool ok, const QString& err) {
            if (!self)
                return;
            if (!ok) {
                self->d_->status->setText(err);
                self->d_->status->setVisible(true);
                return;
            }
            self->d_->select_template(QString());
        });
    });
    for (QWidget* w : {static_cast<QWidget*>(save_name), static_cast<QWidget*>(delete_yes)})
        new rv::RvKeyHook(w, [this](const QKeyEvent* ke, bool apply) {
            if (ke->key() != Qt::Key_Escape)
                return false;
            if (apply) {
                save_frame->setVisible(false);
                delete_frame->setVisible(false);
            }
            return true;
        });
}

void NewRunDialog::Impl::retranslate() {
    q->setWindowTitle(NewRunDialog::tr("New run"));
    save_tpl->setText(NewRunDialog::tr("SAVE AS TEMPLATE"));
    save_tpl->setToolTip(NewRunDialog::tr("Keep this task (text, parameters, engine and budget) as your own template"));
    delete_tpl->setText(NewRunDialog::tr("DELETE"));
    save_name->setPlaceholderText(NewRunDialog::tr("Template name"));
    save_ok->setText(NewRunDialog::tr("SAVE"));
    save_cancel->setText(NewRunDialog::tr("CANCEL"));
    delete_yes->setText(NewRunDialog::tr("DELETE"));
    delete_no->setText(NewRunDialog::tr("KEEP"));
    confirm_yes->setText(NewRunDialog::tr("START ANYWAY"));
    confirm_change->setText(NewRunDialog::tr("CHANGE BUDGET"));
    schedule_btn->setText(NewRunDialog::tr("SCHEDULE…"));
    schedule_btn->setToolTip(NewRunDialog::tr("Run this task on a schedule instead (daily, weekly, every few hours)"));
    cancel_btn->setText(NewRunDialog::tr("CANCEL"));
    start_btn->setText(NewRunDialog::tr("START NOW"));
    list->setAccessibleName(NewRunDialog::tr("Task templates"));
}

void NewRunDialog::Impl::fill_list() {
    const QString keep = form->template_id();
    const QSignalBlocker block(list);
    list->clear();
    const auto header = [this](const QString& text) {
        auto* item = new QListWidgetItem(text.toUpper(), list);
        item->setFlags(Qt::NoItemFlags);
        QFont f = ui::fonts::font(-4, QFont::Bold);
        item->setFont(f);
        item->setForeground(QColor(ui::colors::TEXT_TERTIARY()));
    };
    const auto add = [this](const QString& id, const QString& title, const QString& tip) {
        auto* item = new QListWidgetItem(title, list);
        item->setData(kNrdIdRole, id);
        item->setToolTip(tip);
    };
    header(NewRunDialog::tr("Start from scratch"));
    add(QString(), NewRunDialog::tr("Custom task"), NewRunDialog::tr("Describe a task in your own words"));
    QString last_category;
    for (const agt::RunTemplate& t : agt::RunTemplateLibrary::instance().templates()) {
        const QString cat = t.user_defined ? NewRunDialog::tr("My templates") : t.category;
        if (cat != last_category) {
            header(cat);
            last_category = cat;
        }
        add(t.id, t.title, t.description);
    }
    // Keep the selection.
    for (int i = 0; i < list->count(); ++i) {
        QListWidgetItem* item = list->item(i);
        if (item->data(kNrdIdRole).isValid() && item->data(kNrdIdRole).toString() == keep) {
            list->setCurrentItem(item);
            break;
        }
    }
}

void NewRunDialog::Impl::select_template(const QString& id) {
    for (int i = 0; i < list->count(); ++i) {
        QListWidgetItem* item = list->item(i);
        if (item->data(kNrdIdRole).isValid() && item->data(kNrdIdRole).toString() == id) {
            list->setCurrentItem(item); // currentItemChanged shows it
            if (form->template_id() != id)
                form->set_template(id);
            return;
        }
    }
    form->set_template(id);
}

void NewRunDialog::Impl::validate() {
    const auto t = form->current_template();
    delete_tpl->setVisible(t && t->user_defined);
    const QStringList p = form->problems();
    problems->setText(p.isEmpty() ? QString() : QStringLiteral("• ") + p.join(QStringLiteral("\n• ")));
    problems->setVisible(!p.isEmpty());
    start_btn->setEnabled(!starting);
    start_btn->setToolTip(p.isEmpty() ? NewRunDialog::tr("Start the run now; follow it in RUNS, PLAN and ACTIVITY")
                                      : NewRunDialog::tr("Fix what is listed above first"));
}

void NewRunDialog::Impl::start() {
    if (starting)
        return;
    const QStringList p = form->problems();
    if (!p.isEmpty()) {
        validate();
        return;
    }
    const rv::BudgetConfirmation c = form->confirmation(balance);
    if (c.ask && !confirmed) {
        // The plan's confirmation above kConfirmAboveCredits, inline.
        confirm_text->setText(c.question);
        confirm->setVisible(true);
        confirm_yes->setFocus();
        return;
    }
    starting = true;
    const quint64 request_id = ++start_request;
    start_btn->setEnabled(false);
    status->setText(NewRunDialog::tr("Preparing the task…"));
    status->setVisible(true);
    agt::RunRequest req = form->request();
    req.session_uuid = session_uuid;
    remember();
    QPointer<NewRunDialog> self(q);
    agt::RunTemplateLibrary::instance().prepare_request(
        req, form->refill_template_id(), q, [self, request_id](const agt::RunTemplateLibrary::Prepared& prepared) {
            if (!self || request_id != self->d_->start_request)
                return;
            Impl* d = self->d_.get();
            d->starting = false;
            d->confirmed = false;
            if (!prepared.ok) {
                d->status->setText(prepared.error);
                d->start_btn->setEnabled(true);
                return;
            }
            QString err;
            const QString run_id = agt::TaskRunService::instance().start_run(prepared.request, &err);
            if (run_id.isEmpty()) {
                d->status->setText(err);
                d->start_btn->setEnabled(true);
                return;
            }
            emit self->run_started(run_id);
            self->close();
        });
}

void NewRunDialog::Impl::remember() const {
    QSettings().setValue(QLatin1String(kNrdLastKey),
                         QString::fromUtf8(QJsonDocument(form->values()).toJson(QJsonDocument::Compact)));
}

void NewRunDialog::Impl::restore() {
    const QJsonObject last =
        QJsonDocument::fromJson(QSettings().value(QLatin1String(kNrdLastKey)).toString().toUtf8()).object();
    if (last.isEmpty()) {
        select_template(QStringLiteral("equity_deep_dive")); // a useful start, never a blank form
        return;
    }
    form->set_values(last);
    select_template(form->template_id());
}

void NewRunDialog::Impl::save_template() {
    const QString name = save_name->text().trimmed();
    if (name.isEmpty()) {
        save_name->setFocus();
        return;
    }
    agt::RunTemplate t;
    const auto current = form->current_template();
    const agt::RunRequest r = form->request();
    if (current) {
        t = *current;
        if (!current->user_defined) {
            t.id = current->id; // saved as a new user template based on it
        }
        const QString edited = form->edited_prompt();
        if (!edited.isEmpty())
            t.prompt = edited;
    } else {
        t.prompt = form->edited_prompt();
        t.description = NewRunDialog::tr("Your own task.");
    }
    if (current && current->user_defined && current->title != name)
        t.id.clear(); // a different name: a new template, the old one stays
    t.title = name;
    t.engine = r.engine;
    t.mode = r.mode;
    t.plan_first = r.plan_first;
    t.budget = r.budget;
    QPointer<NewRunDialog> self(q);
    save_ok->setEnabled(false);
    agt::RunTemplateLibrary::instance().save_user_template(t, q, [self](bool ok, const QString& id, const QString& err) {
        if (!self)
            return;
        Impl* d = self->d_.get();
        d->save_ok->setEnabled(true);
        if (!ok) {
            d->status->setText(err);
            d->status->setVisible(true);
            return;
        }
        d->save_frame->setVisible(false);
        d->status->setText(NewRunDialog::tr("Template saved."));
        d->status->setVisible(true);
        d->select_template(id);
    });
}

// ── NewRunDialog ─────────────────────────────────────────────────────────────

NewRunDialog::NewRunDialog(QWidget* parent) : QDialog(parent), d_(std::make_unique<Impl>()) {
    d_->q = this;
    d_->build();
    d_->wire();
    d_->retranslate();
    agt::RunTemplateLibrary::instance().ensure_loaded();
    d_->fill_list();
    d_->restore();
    d_->validate();
    // The balance for the estimate and the confirmation (one request, no polling).
    QPointer<NewRunDialog> self(this);
    ChatModeService::instance().get_credits([self](bool ok, double credits, QString) {
        if (!self || !ok)
            return;
        self->d_->balance = credits;
        self->d_->form->set_balance(credits);
    });
}

NewRunDialog::~NewRunDialog() = default;

void NewRunDialog::set_template(const QString& template_id) {
    d_->select_template(template_id);
    d_->validate();
}

void NewRunDialog::set_prompt(const QString& prompt) {
    d_->select_template(QString());
    d_->form->set_custom_prompt(prompt);
    d_->validate();
}

void NewRunDialog::set_context(const QJsonObject& context) {
    d_->extra_context = context;
    d_->form->set_extra_context(context);
    d_->validate();
}

void NewRunDialog::set_session(const QString& session_uuid) {
    d_->session_uuid = session_uuid;
}

void NewRunDialog::closeEvent(QCloseEvent* event) {
    d_->remember();
    QDialog::closeEvent(event);
}

} // namespace fincept::chat_mode
