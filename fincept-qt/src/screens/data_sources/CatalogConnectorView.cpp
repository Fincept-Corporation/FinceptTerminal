// CatalogConnectorView.cpp — see CatalogConnectorView.h.

#include "screens/data_sources/CatalogConnectorView.h"

#include "screens/data_sources/ConnectorParamForm.h"
#include "services/connectors/ConnectorCatalog.h"
#include "ui/widgets/AutoLoadGate.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/LoadingSkeleton.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::screens::datasources {

namespace conn = fincept::services::connectors;
namespace dsrc = fincept::services::datasources;
namespace dm = fincept::services::datamapping;

namespace {

QString ccv_age(qint64 ms) {
    const qint64 s = std::max<qint64>(0, ms / 1000);
    if (s < 60)
        return QObject::tr("%1 s").arg(s);
    if (s < 3600)
        return QObject::tr("%1 min").arg(s / 60);
    return QObject::tr("%1 h").arg(s / 3600);
}

} // namespace

CatalogConnectorView::CatalogConnectorView(QWidget* parent) : QWidget(parent) {
    build();
    connect(&conn::ConnectorService::instance(), &conn::ConnectorService::key_status_changed, this,
            &CatalogConnectorView::refresh_key_state);
}

void CatalogConnectorView::build() {
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* header = new QWidget(this);
    set_look(header, "bar");
    header->setAttribute(Qt::WA_StyledBackground, true);
    auto* hvl = new QVBoxLayout(header);
    hvl->setContentsMargins(12, 8, 12, 8);
    hvl->setSpacing(4);
    title_ = new QLabel(header);
    set_look(title_, "h1");
    title_->setTextFormat(Qt::PlainText);
    meta_ = new QLabel(header);
    set_look(meta_, "muted");
    meta_->setTextFormat(Qt::PlainText);
    desc_ = new QLabel(header);
    set_look(desc_, "text");
    desc_->setWordWrap(true);
    desc_->setTextFormat(Qt::PlainText);
    hvl->addWidget(title_);
    hvl->addWidget(meta_);
    hvl->addWidget(desc_);
    auto* krow = new QHBoxLayout;
    krow->setSpacing(8);
    key_line_ = new QLabel(header);
    key_line_->setTextFormat(Qt::PlainText);
    key_line_->setWordWrap(true);
    krow->addWidget(key_line_, 1);
    add_key_btn_ = new QPushButton(tr("ADD KEY"), header);
    add_key_btn_->setToolTip(tr("Opens Settings > Credentials on this key"));
    krow->addWidget(add_key_btn_);
    hvl->addLayout(krow);
    vl->addWidget(header);
    broken_ = new QLabel(this);
    set_look(broken_, "banner");
    broken_->setWordWrap(true);
    broken_->setTextFormat(Qt::PlainText);
    broken_->hide();
    vl->addWidget(broken_);

    auto* inputs = new QWidget(this);
    auto* ivl = new QVBoxLayout(inputs);
    ivl->setContentsMargins(12, 8, 12, 8);
    ivl->setSpacing(6);
    auto* crow = new QHBoxLayout;
    crow->setSpacing(8);
    auto* cmd_lbl = new QLabel(tr("COMMAND"), inputs);
    set_look(cmd_lbl, "field");
    crow->addWidget(cmd_lbl);
    command_box_ = new QComboBox(inputs);
    command_box_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    command_box_->setMaxVisibleItems(20);
    crow->addWidget(command_box_);
    hint_ = new QLabel(inputs);
    set_look(hint_, "note");
    hint_->setTextFormat(Qt::PlainText);
    crow->addWidget(hint_, 1);
    as_text_ = new QCheckBox(tr("Edit as text"), inputs);
    as_text_->setToolTip(tr("Type the arguments as one line instead of one field each"));
    crow->addWidget(as_text_);
    force_ = new QCheckBox(tr("Skip cache"), inputs);
    force_->setToolTip(tr("Fetch again even when a recent result is cached"));
    crow->addWidget(force_);
    run_btn_ = new QPushButton(tr("RUN"), inputs);
    set_look(run_btn_, "primary");
    run_btn_->setToolTip(tr("Run this command (Enter in a field does the same)"));
    crow->addWidget(run_btn_);
    ivl->addLayout(crow);
    fields_box_ = new QWidget(inputs);
    fields_grid_ = new QGridLayout(fields_box_);
    fields_grid_->setContentsMargins(0, 0, 0, 0);
    fields_grid_->setHorizontalSpacing(8);
    fields_grid_->setVerticalSpacing(6);
    fields_grid_->setColumnStretch(1, 1);
    fields_grid_->setColumnStretch(3, 1);
    ivl->addWidget(fields_box_);
    param_form_ = new ConnectorParamForm(inputs);
    param_form_->hide();
    ivl->addWidget(param_form_);
    raw_args_ = new QPlainTextEdit(inputs);
    raw_args_->setMaximumHeight(fontMetrics().lineSpacing() * 3 + 14);
    raw_args_->hide();
    ivl->addWidget(raw_args_);
    input_error_ = new QLabel(inputs);
    set_look(input_error_, "error");
    input_error_->setWordWrap(true);
    input_error_->hide();
    ivl->addWidget(input_error_);
    vl->addWidget(inputs);

    stack_ = new QStackedWidget(this);
    state_ = new fincept::ui::EmptyState(stack_);
    loading_ = new QWidget(stack_);
    auto* lvl = new QVBoxLayout(loading_);
    lvl->setContentsMargins(0, 0, 0, 8);
    skeleton_ = new fincept::ui::LoadingSkeleton(fincept::ui::LoadingSkeleton::Shape::Table, loading_);
    lvl->addWidget(skeleton_, 1);
    auto* cancel_row = new QHBoxLayout;
    cancel_row->addStretch(1);
    auto* cancel_btn = new QPushButton(tr("CANCEL"), loading_);
    cancel_row->addWidget(cancel_btn);
    cancel_row->addStretch(1);
    lvl->addLayout(cancel_row);
    result_ = new ResultPane(QStringLiteral("Data Sources"), stack_);
    stack_->addWidget(state_);
    stack_->addWidget(loading_);
    stack_->addWidget(result_);
    vl->addWidget(stack_, 1);

    auto_timer_ = new QTimer(this);
    auto_timer_->setSingleShot(true);
    auto_timer_->setInterval(350); // users flick through the list; only a pause starts a load
    connect(auto_timer_, &QTimer::timeout, this, [this]() { start_run(true); });
    retry_timer_ = new QTimer(this);
    retry_timer_->setInterval(1000);
    connect(retry_timer_, &QTimer::timeout, this, [this]() {
        if (--retry_left_ <= 0) {
            retry_timer_->stop();
            state_->set_primary_action(tr("RETRY"));
            state_->set_primary_enabled(true);
        } else {
            state_->set_primary_action(tr("RETRY IN %1 s").arg(retry_left_));
        }
    });

    connect(command_box_, &QComboBox::activated, this, [this](int) {
        command_ = command_box_->currentData().toString();
        rebuild_inputs();
        const conn::ConnectorInfo* c = info();
        if (c && c->has_sample && command_ == c->sample_command)
            fill_command_args(c->sample_args);
        else if (c && command_ == c->default_command)
            fill_command_args(c->default_args);
        if (typed_)
            param_form_->attach_memory();
        input_error_->hide();
    });
    connect(param_form_, &ConnectorParamForm::submitted, this, [this]() { start_run(false); });
    connect(as_text_, &QCheckBox::toggled, this, &CatalogConnectorView::set_text_mode);
    connect(run_btn_, &QPushButton::clicked, this, [this]() { start_run(false); });
    connect(cancel_btn, &QPushButton::clicked, this, [this]() {
        cancel_run();
        if (result_->has_result())
            stack_->setCurrentWidget(result_);
        else
            show_idle();
    });
    connect(state_, &fincept::ui::EmptyState::primary_clicked, this, [this]() { run_action(primary_); });
    connect(state_, &fincept::ui::EmptyState::secondary_clicked, this, [this]() { run_action(secondary_); });
    connect(add_key_btn_, &QPushButton::clicked, this, [this]() {
        const conn::ConnectorInfo* c = info();
        if (!c || c->env_keys.isEmpty())
            return;
        QString key = c->env_keys.first();
        for (const QString& k : c->env_keys)
            if (!conn::ConnectorCatalog::instance().cached_key_present(k).value_or(false)) {
                key = k;
                break;
            }
        open_credentials(key, this);
    });
}

const conn::ConnectorInfo* CatalogConnectorView::info() const {
    return conn::ConnectorCatalog::instance().find(name_);
}

void CatalogConnectorView::show_connector(const QString& name) {
    cancel_run();
    name_ = name;
    result_->clear();
    input_error_->hide();
    const conn::ConnectorInfo* c = info();
    if (!c) {
        title_->setText(name);
        meta_->clear();
        desc_->setText(tr("This connector is not in the catalogue any more."));
        state_->apply(fincept::ui::EmptyState::error_spec(tr("The catalogue has no connector named %1.").arg(name),
                                                          tr("Unknown connector"), QString()));
        primary_ = secondary_ = ErrorAction::None;
        stack_->setCurrentWidget(state_);
        return;
    }
    title_->setText(c->title);
    // agents: a run of a connector that needs an API key can spend a paid / metered quota
    run_btn_->setProperty("fincept_agent_risk", c->key_required ? QVariant(QStringLiteral("spend")) : QVariant());
    meta_->setText(QStringLiteral("%1 · %2 · %3").arg(c->category, c->region, c->script));
    desc_->setText(c->description.isEmpty() ? tr("No description.") : c->description);
    broken_->setText(c->broken.isEmpty() ? QString() : tr("Upstream problem when last checked: %1").arg(c->broken));
    broken_->setVisible(!c->broken.isEmpty());
    {
        const QSignalBlocker block(command_box_);
        command_box_->clear();
        for (const conn::ConnectorCommand& cmd : c->commands)
            command_box_->addItem(cmd.name, cmd.name);
    }
    // The catalogue's sample command when there is one, else the default command.
    command_ = c->has_sample ? c->sample_command : c->default_command;
    command_box_->setCurrentIndex(std::max(0, command_box_->findData(command_)));
    rebuild_inputs();
    fill_command_args(c->has_sample ? c->sample_args : (command_ == c->default_command ? c->default_args : QStringList()));
    // What the user ran last with this command comes back over the sample / defaults.
    const bool remembered = typed_ && param_form_->attach_memory();
    update_key_line();

    const auto state = conn::ConnectorCatalog::instance().key_state(*c);
    // Sample on open ONLY when the catalogue has one (a verified keyless call — also for a connector
    // whose other commands are broken upstream: the banner says so), the key store answered, and the
    // user has no remembered inputs for that command.
    wants_sample_ = c->has_sample && state != conn::KeyState::Unreadable && !remembered && command_ == c->sample_command;
    if (wants_sample_)
        schedule_sample();
    else if (!c->broken.isEmpty())
        show_broken_page();
    else
        show_idle();
}

void CatalogConnectorView::fill_command_args(const QStringList& args) {
    if (!typed_) {
        fill_values(args);
        return;
    }
    const conn::ConnectorInfo* c = info();
    const conn::ConnectorCommand* cmd = c ? c->command(command_) : nullptr;
    if (!cmd || args.isEmpty())
        return;
    QVector<conn::ParamInput> inputs;
    if (conn::inputs_from_args(cmd->params, args, &inputs))
        param_form_->set_inputs(inputs);
}

void CatalogConnectorView::show_broken_page() {
    const conn::ConnectorInfo* c = info();
    if (!c)
        return;
    retry_timer_->stop();
    showing_unreadable_ = false;
    fincept::ui::EmptyState::Spec spec;
    spec.glyph = QStringLiteral("×");
    spec.glyph_tone = fincept::ui::EmptyState::Tone::Neutral;
    spec.title = tr("Not working upstream");
    spec.message = c->broken;
    spec.note = tr("Checked when the catalogue was built — RUN still tries, in case it works again.");
    spec.note_tone = fincept::ui::EmptyState::Tone::Info;
    spec.secondary = tr("RUN ANYWAY");
    primary_ = ErrorAction::None;
    secondary_ = ErrorAction::Retry;
    state_->apply(spec);
    stack_->setCurrentWidget(state_);
}

void CatalogConnectorView::show_unreadable_page() {
    const conn::ConnectorInfo* c = info();
    if (!c)
        return;
    retry_timer_->stop();
    auto& catalog = conn::ConnectorCatalog::instance();
    // The key may be stored: a retryable problem, never "add a key".
    fincept::ui::EmptyState::Spec spec = fincept::ui::EmptyState::error_spec(
        tr("Could not read secure storage: %1. The %2 key may already be saved — nothing needs to be entered again.")
            .arg(catalog.key_state_reason(*c), c->env_keys.join(QStringLiteral(" / "))),
        tr("Could not read the stored key"), tr("RETRY"));
    spec.glyph_tone = fincept::ui::EmptyState::Tone::Warning;
    primary_ = ErrorAction::Retry; // run_action re-reads the keys first
    secondary_ = ErrorAction::None;
    showing_unreadable_ = true;
    state_->apply(spec);
    state_->set_primary_enabled(true);
    stack_->setCurrentWidget(state_);
}

void CatalogConnectorView::schedule_sample() {
    skeleton_->set_message(tr("Loading a sample of %1…").arg(title_->text()));
    stack_->setCurrentWidget(loading_);
    auto_timer_->start();
}

void CatalogConnectorView::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (wants_sample_ && !result_->has_result() && pending_request_.isEmpty() && !auto_timer_->isActive() &&
        stack_->currentWidget() != state_)
        schedule_sample();
}

void CatalogConnectorView::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    // An automatic sample does not outlive the pane; a run the user started keeps going.
    if (auto_timer_->isActive() || (automatic_ && !pending_request_.isEmpty()) || gate_ticket_) {
        cancel_run();
        if (!result_->has_result())
            stack_->setCurrentWidget(loading_); // re-armed by showEvent
    }
}

void CatalogConnectorView::refresh_key_state() {
    update_key_line();
    const conn::ConnectorInfo* c = info();
    if (!c || stack_->currentWidget() != state_ || (primary_ != ErrorAction::AddKey && !showing_unreadable_))
        return;
    const auto state = conn::ConnectorCatalog::instance().key_state(*c);
    if (state == conn::KeyState::Ready || state == conn::KeyState::NotNeeded || showing_unreadable_)
        show_idle(); // re-decides: RUN preview, "needs a key", or still unreadable
}

void CatalogConnectorView::update_key_line() {
    const conn::ConnectorInfo* c = info();
    if (!c) {
        key_line_->clear();
        add_key_btn_->hide();
        return;
    }
    auto& catalog = conn::ConnectorCatalog::instance();
    const auto state = catalog.key_state(*c);
    QString text = conn::key_requirement_text(*c);
    const char* look = "muted";
    switch (state) {
        case conn::KeyState::NotNeeded:
            look = "ok";
            break;
        case conn::KeyState::Ready:
            text = tr("Key stored: %1").arg(c->env_keys.join(QStringLiteral(", ")));
            look = "ok";
            break;
        case conn::KeyState::Partial:
            text += tr(" · some keys are not stored");
            look = "warn";
            break;
        case conn::KeyState::Missing:
            text += c->key_required ? tr(" · not stored yet") : tr(" · not stored (optional)");
            look = c->key_required ? "warn" : "muted";
            break;
        case conn::KeyState::Unknown:
            break;
        case conn::KeyState::Unreadable:
            text = tr("Could not read secure storage: %1. The key may already be saved.")
                       .arg(catalog.key_state_reason(*c));
            look = "warn";
            break;
    }
    if (!c->key_note.isEmpty())
        text += QStringLiteral(" — ") + c->key_note;
    key_line_->setText(text);
    set_look(key_line_, look);
    add_key_btn_->setVisible(!c->env_keys.isEmpty());
    // Unreadable: a key may exist — the button opens Credentials to look, it does not ask to add one.
    add_key_btn_->setText(state == conn::KeyState::Ready         ? tr("CHANGE KEY")
                          : state == conn::KeyState::Unreadable ? tr("CHECK KEY")
                                                                 : tr("ADD KEY"));
}

void CatalogConnectorView::rebuild_inputs() {
    const conn::ConnectorInfo* c = info();
    const conn::ConnectorCommand* cmd = c ? c->command(command_) : nullptr;
    // Fields are rebuilt under their final parent; the old ones are removed first.
    for (QWidget* w : fields_box_->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
        w->hide();
        w->deleteLater();
    }
    field_inputs_.clear();
    raw_args_->clear();
    typed_ = cmd && cmd->params_known;
    if (typed_) {
        // The catalogue's typed params: one control each; argv by connectors::build_argv.
        form_ = conn::ArgsForm();
        param_form_->build(name_, command_, cmd->params, c->has_command(QStringLiteral("sheets")));
        hint_->setText(cmd->params.isEmpty() ? tr("no arguments")
                       : cmd->args_hint.isEmpty() ? QString()
                                                  : tr("args: %1").arg(cmd->args_hint));
        raw_args_->setPlaceholderText(cmd->args_hint.isEmpty() ? tr("No arguments needed") : cmd->args_hint);
        as_text_->setEnabled(!cmd->params.isEmpty());
        {
            const QSignalBlocker block(as_text_);
            as_text_->setChecked(false);
        }
        fields_box_->hide();
        raw_args_->hide();
        param_form_->setVisible(!cmd->params.isEmpty());
        return;
    }
    param_form_->hide();
    form_ = conn::args_form_from_hint(cmd ? cmd->args_hint : QString());
    hint_->setText(form_.hint.isEmpty() ? (form_.fields.isEmpty() ? tr("no arguments") : QString())
                                        : tr("args: %1").arg(form_.hint));
    int i = 0;
    for (const conn::ArgField& f : form_.fields) {
        auto* lbl = new QLabel(amp_safe(f.required ? f.label + QStringLiteral(" *") : f.label), fields_box_);
        set_look(lbl, "field");
        QWidget* input = nullptr;
        if (f.flag) {
            input = new QCheckBox(tr("on"), fields_box_);
        } else if (!f.choices.isEmpty()) {
            auto* combo = new QComboBox(fields_box_);
            if (!f.required)
                combo->addItem(f.default_value.isEmpty() ? tr("(not set)") : tr("(default: %1)").arg(f.default_value),
                               QString());
            for (const QString& ch : f.choices)
                combo->addItem(ch, ch);
            input = combo;
        } else {
            auto* edit = new QLineEdit(fields_box_);
            edit->setPlaceholderText(f.placeholder);
            edit->setClearButtonEnabled(true);
            connect(edit, &QLineEdit::returnPressed, this, [this]() { start_run(false); });
            input = edit;
        }
        QString tip = f.type == QLatin1String("text") ? QString() : tr("Type: %1").arg(f.type);
        if (!f.default_value.isEmpty())
            tip += (tip.isEmpty() ? QString() : QStringLiteral(" · ")) + tr("default %1").arg(f.default_value);
        if (f.variadic)
            tip += (tip.isEmpty() ? QString() : QStringLiteral(" · ")) + tr("several values: separate with spaces");
        input->setToolTip(tip);
        lbl->setBuddy(input);
        const int row = i / 2;
        const int colx = (i % 2) * 2;
        fields_grid_->addWidget(lbl, row, colx);
        fields_grid_->addWidget(input, row, colx + 1);
        field_inputs_.append(input);
        ++i;
    }
    raw_args_->setPlaceholderText(form_.hint.isEmpty() ? tr("No arguments needed") : form_.hint);
    const bool structured = form_.structured;
    as_text_->setEnabled(structured && !form_.fields.isEmpty());
    {
        const QSignalBlocker block(as_text_);
        as_text_->setChecked(!structured);
    }
    fields_box_->setVisible(structured && !form_.fields.isEmpty());
    raw_args_->setVisible(!structured);
}

QStringList CatalogConnectorView::field_values() const {
    QStringList out;
    for (QWidget* w : field_inputs_) {
        if (auto* edit = qobject_cast<QLineEdit*>(w))
            out << edit->text();
        else if (auto* combo = qobject_cast<QComboBox*>(w))
            out << combo->currentData().toString();
        else if (auto* check = qobject_cast<QCheckBox*>(w))
            out << (check->isChecked() ? QStringLiteral("true") : QString());
        else
            out << QString();
    }
    return out;
}

void CatalogConnectorView::fill_values(const QStringList& args) {
    if (!form_.structured) {
        raw_args_->setPlainText(conn::join_args_text(args));
        return;
    }
    QStringList vals;
    if (!conn::values_from_args(form_, args, &vals)) {
        const QSignalBlocker block(as_text_);
        as_text_->setChecked(true);
        fields_box_->hide();
        raw_args_->setPlainText(conn::join_args_text(args));
        raw_args_->show();
        return;
    }
    for (int i = 0; i < field_inputs_.size() && i < vals.size(); ++i) {
        QWidget* w = field_inputs_[i];
        if (auto* edit = qobject_cast<QLineEdit*>(w)) {
            edit->setText(vals[i]);
        } else if (auto* combo = qobject_cast<QComboBox*>(w)) {
            const int idx = combo->findData(vals[i]);
            combo->setCurrentIndex(idx >= 0 ? idx : 0);
        } else if (auto* check = qobject_cast<QCheckBox*>(w)) {
            check->setChecked(vals[i] == QLatin1String("true"));
        }
    }
}

void CatalogConnectorView::set_text_mode(bool on) {
    input_error_->hide();
    if (typed_) {
        const conn::ConnectorInfo* c = info();
        const conn::ConnectorCommand* cmd = c ? c->command(command_) : nullptr;
        if (!cmd)
            return;
        if (on) {
            const conn::ArgvResult b = conn::build_argv(command_, cmd->params, param_form_->inputs());
            QStringList args = b.args();
            if (!b.ok) {
                args.clear();
                for (const conn::ParamInput& in : param_form_->inputs())
                    if (in.given)
                        args << in.value.trimmed();
            }
            raw_args_->setPlainText(conn::join_args_text(args));
            param_form_->hide();
            raw_args_->show();
            return;
        }
        QVector<conn::ParamInput> inputs;
        if (!conn::inputs_from_args(cmd->params, conn::split_args_text(raw_args_->toPlainText()), &inputs)) {
            const QSignalBlocker block(as_text_);
            as_text_->setChecked(true);
            input_error_->setText(tr("These arguments do not fit the fields — keep editing them as text."));
            input_error_->show();
            return;
        }
        param_form_->set_inputs(inputs);
        raw_args_->hide();
        param_form_->show();
        return;
    }
    if (on) {
        const conn::BuiltArgs b = conn::build_args(form_, field_values());
        QStringList args = b.args;
        if (!b.ok) {
            args.clear();
            for (const QString& v : field_values())
                if (!v.trimmed().isEmpty())
                    args << v.trimmed();
        }
        raw_args_->setPlainText(conn::join_args_text(args));
        fields_box_->hide();
        raw_args_->show();
        return;
    }
    QStringList vals;
    if (!conn::values_from_args(form_, conn::split_args_text(raw_args_->toPlainText()), &vals)) {
        const QSignalBlocker block(as_text_);
        as_text_->setChecked(true);
        input_error_->setText(tr("These arguments do not fit the fields — keep editing them as text."));
        input_error_->show();
        return;
    }
    raw_args_->hide();
    fields_box_->show();
    fill_values(conn::split_args_text(raw_args_->toPlainText()));
}

void CatalogConnectorView::show_idle() {
    const conn::ConnectorInfo* c = info();
    if (!c)
        return;
    retry_timer_->stop();
    auto& catalog = conn::ConnectorCatalog::instance();
    const auto state = catalog.key_state(*c);
    fincept::ui::EmptyState::Spec spec;
    showing_unreadable_ = false;
    if (c->key_required && state == conn::KeyState::Unreadable) {
        show_unreadable_page(); // the key may be stored: RETRY, never "add a key"
        return;
    }
    const conn::ConnectorCommand* cmd = c->command(command_);
    if (c->key_required && (state == conn::KeyState::Missing || state == conn::KeyState::Partial)) {
        spec.glyph = QStringLiteral("!");
        spec.glyph_tone = fincept::ui::EmptyState::Tone::Warning;
        spec.title = tr("Needs an API key");
        spec.message = tr("%1 returns data only with your key. Add it once in Settings > Credentials; every tab and "
                          "the AI tools use the same key.")
                           .arg(c->title);
        for (const QString& k : c->env_keys)
            spec.details.append({tr("Key"), k});
        spec.details_heading = tr("NEEDED");
        if (!c->docs_url.isEmpty())
            spec.note = tr("Get a key: %1").arg(c->docs_url);
        spec.primary = tr("ADD KEY");
        primary_ = ErrorAction::AddKey;
        secondary_ = ErrorAction::None;
        action_key_ = c->env_keys.value(0);
        for (const QString& k : c->env_keys)
            if (!catalog.cached_key_present(k).value_or(false)) {
                action_key_ = k;
                break;
            }
    } else if (typed_ && cmd && !conn::runs_without_input(cmd->params)) {
        // No keyless sample can show this connector: say what to fill in.
        QStringList needed;
        for (const conn::ConnectorParam& p : cmd->params)
            if (p.required)
                needed << p.name;
        spec = fincept::ui::EmptyState::preview_spec(
            tr("Fill in the inputs, then RUN"),
            tr("%1 needs %2 — enter it above.").arg(c->title, needed.join(QStringLiteral(", "))), tr("RUN"),
            c->env_keys.isEmpty() ? tr("Runs %1 on this computer").arg(c->script)
                                  : tr("Uses your %1 key").arg(c->env_keys.join(QStringLiteral(", "))),
            fincept::ui::EmptyState::Tone::Info, fincept::ui::LoadingSkeleton::Shape::Table);
        primary_ = ErrorAction::Retry;
        secondary_ = ErrorAction::None;
    } else {
        spec = fincept::ui::EmptyState::preview_spec(
            tr("Run %1").arg(command_), c->description.isEmpty() ? tr("Fetches rows from %1.").arg(c->title) : c->description,
            tr("RUN"),
            c->env_keys.isEmpty() ? tr("Runs %1 on this computer · results are cached").arg(c->script)
                                  : tr("Uses your %1 key · results are cached").arg(c->env_keys.join(QStringLiteral(", "))),
            fincept::ui::EmptyState::Tone::Info, fincept::ui::LoadingSkeleton::Shape::Table);
        primary_ = ErrorAction::Retry;
        secondary_ = ErrorAction::None;
    }
    state_->apply(spec);
    state_->set_primary_enabled(true);
    stack_->setCurrentWidget(state_);
}

void CatalogConnectorView::start_run(bool automatic) {
    const conn::ConnectorInfo* c = info();
    if (!c || command_.isEmpty())
        return;
    input_error_->hide();
    QStringList args;
    const conn::ConnectorCommand* cmd = c->command(command_);
    if (automatic && c->has_sample && command_ == c->sample_command) {
        args = c->sample_args; // the catalogue's verified keyless call, exactly as written
    } else if (as_text_->isChecked() || (!typed_ && !form_.structured)) {
        args = conn::split_args_text(raw_args_->toPlainText());
    } else if (typed_ && cmd) {
        const conn::ArgvResult b = conn::build_argv(command_, cmd->params, param_form_->inputs());
        if (!b.ok) {
            input_error_->setText(b.error);
            input_error_->show();
            param_form_->focus_param(b.param);
            if (automatic)
                show_idle();
            return;
        }
        args = b.args();
    } else {
        const conn::BuiltArgs b = conn::build_args(form_, field_values());
        if (!b.ok) {
            input_error_->setText(b.error);
            input_error_->show();
            if (b.field >= 0 && b.field < field_inputs_.size())
                field_inputs_[b.field]->setFocus(Qt::OtherFocusReason);
            if (automatic)
                show_idle();
            return;
        }
        args = b.args;
    }
    cancel_run();
    const quint64 seq = ++run_seq_;
    automatic_ = automatic;
    retry_timer_->stop();
    skeleton_->set_message(tr("Running %1 %2…").arg(c->title, command_));
    stack_->setCurrentWidget(loading_);
    if (!automatic) {
        begin_run(args, false, seq);
        return;
    }
    QPointer<CatalogConnectorView> self(this);
    auto& gate = fincept::ui::AutoLoadGate::instance();
    if (gate.enter(
            this,
            [self, args, seq]() {
                if (self && seq == self->run_seq_)
                    self->begin_run(args, true, seq);
            },
            0, &gate_ticket_)) {
        begin_run(args, true, seq);
    } else {
        skeleton_->set_message(tr("Queued — other loads are running"));
    }
}

void CatalogConnectorView::begin_run(const QStringList& args, bool automatic, quint64 seq) {
    if (automatic)
        skeleton_->set_message(tr("Loading a sample of %1…").arg(title_->text()));
    conn::ConnectorService::RunOptions opts;
    opts.force = !automatic && force_->isChecked();
    QPointer<CatalogConnectorView> self(this);
    const QString name = name_;
    const QString command = command_;
    pending_request_ = conn::ConnectorService::instance().run_with(
        name, command, args, opts, [self, seq](const conn::ConnectorResult& r) {
            if (!self || seq != self->run_seq_)
                return;
            self->on_result(r);
        });
}

void CatalogConnectorView::finish_gate() {
    if (gate_ticket_) {
        fincept::ui::AutoLoadGate::instance().leave(gate_ticket_);
        gate_ticket_ = 0;
    }
}

void CatalogConnectorView::cancel_run() {
    ++run_seq_;
    auto_timer_->stop();
    retry_timer_->stop();
    if (!pending_request_.isEmpty()) {
        conn::ConnectorService::instance().cancel(pending_request_);
        pending_request_.clear();
    }
    finish_gate();
    automatic_ = false;
}

void CatalogConnectorView::on_result(const conn::ConnectorResult& r) {
    pending_request_.clear();
    finish_gate();
    automatic_ = false;
    if (!r.success) {
        dsrc::SourceError e = r.failure;
        if (e.cancelled) {
            if (result_->has_result())
                stack_->setCurrentWidget(result_);
            else
                show_idle();
            return;
        }
        if (e.code == dsrc::ErrorCode::None)
            e = dsrc::SourceError::make(dsrc::ErrorCode::Unknown, r.error);
        show_error(e);
        return;
    }
    if (r.tabular.is_empty()) {
        show_error(dsrc::SourceError::make(dsrc::ErrorCode::NoData,
                                           tr("%1 %2 answered without rows.").arg(r.connector, r.command)));
        return;
    }
    const conn::ConnectorInfo* c = info();
    const QString title = QStringLiteral("%1 · %2").arg(c ? c->title : r.connector, r.command);
    result_->set_result(r.tabular, title);
    dm::SourceRef src;
    src.kind = QStringLiteral("connector");
    src.ref = r.connector;
    src.entity = r.command;
    src.args = r.args;
    result_->set_mapping_source(src, title);
    QStringList notes;
    if (r.from_cache)
        notes << tr("From the cache · fetched %1 ago (tick Skip cache to fetch again)")
                     .arg(ccv_age(QDateTime::currentMSecsSinceEpoch() - r.fetched_at_ms));
    else
        notes << tr("Live · %1 ms").arg(r.duration_ms);
    if (!r.warning.isEmpty())
        notes << tr("Upstream warning: %1").arg(r.warning);
    // The wrapper leaves fields over 2 KB out of meta.extra and names them in extra.omitted.
    const QJsonArray omitted =
        r.tabular.meta.value(QStringLiteral("extra")).toObject().value(QStringLiteral("omitted")).toArray();
    if (!omitted.isEmpty()) {
        QStringList names;
        for (const QJsonValue& v : omitted)
            names << v.toString();
        notes << tr("Large detail fields left out: %1").arg(names.join(QStringLiteral(", ")));
    }
    result_->set_note(notes.join(QStringLiteral(" · ")));
    stack_->setCurrentWidget(result_);
}

void CatalogConnectorView::show_error(const dsrc::SourceError& e) {
    showing_unreadable_ = false;
    // A script that ran without its key because the store could not be read says missing_key:
    // the unreadable store takes precedence — RETRY, never "add a key" over one that may exist.
    if (e.code == dsrc::ErrorCode::MissingKey) {
        if (const conn::ConnectorInfo* c = info()) {
            if (conn::ConnectorCatalog::instance().key_state(*c) == conn::KeyState::Unreadable) {
                show_unreadable_page();
                return;
            }
        }
    }
    const ErrorPage p = error_page(e, true);
    primary_ = p.primary;
    secondary_ = p.secondary;
    action_key_ = p.env_key;
    if (action_key_.isEmpty()) {
        if (const conn::ConnectorInfo* c = info())
            action_key_ = c->env_keys.value(0);
    }
    state_->apply(p.spec);
    if (p.retry_after_s > 0 && p.primary == ErrorAction::Retry) {
        retry_left_ = p.retry_after_s;
        state_->set_primary_action(tr("RETRY IN %1 s").arg(retry_left_));
        state_->set_primary_enabled(false);
        retry_timer_->start();
    }
    if (e.code == dsrc::ErrorCode::BadArgs && !e.field.isEmpty()) {
        if (typed_) {
            const auto& params = param_form_->params();
            for (int i = 0; i < params.size(); ++i)
                if (params[i].name == e.field)
                    param_form_->focus_param(i);
        } else {
            for (int i = 0; i < form_.fields.size() && i < field_inputs_.size(); ++i)
                if (form_.fields[i].name == e.field)
                    field_inputs_[i]->setFocus(Qt::OtherFocusReason);
        }
    }
    stack_->setCurrentWidget(state_);
}

void CatalogConnectorView::run_action(ErrorAction action) {
    switch (action) {
        case ErrorAction::Retry:
            if (showing_unreadable_) {
                // Read the keys again (off the GUI thread); refresh_key_state() redraws.
                state_->set_primary_enabled(false);
                conn::ConnectorService::instance().refresh_key_status();
                break;
            }
            start_run(false);
            break;
        case ErrorAction::AddKey:
            open_credentials(action_key_, this);
            break;
        case ErrorAction::EditInputs:
            if (typed_ && param_form_->isVisible())
                param_form_->focus_param(0);
            else if (!field_inputs_.isEmpty() && fields_box_->isVisible())
                field_inputs_.first()->setFocus(Qt::OtherFocusReason);
            else
                raw_args_->setFocus(Qt::OtherFocusReason);
            break;
        case ErrorAction::EditSettings:
        case ErrorAction::None:
            break;
    }
}

} // namespace fincept::screens::datasources
