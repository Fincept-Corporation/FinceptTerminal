// HarnessRunOptionsWidget.cpp — the shared run options (see the header).

#include "screens/agent_config/HarnessRunOptionsWidget.h"

#include "core/process/HostExec.h"
#include "screens/agent_config/AgentRunCommon.h"
#include "screens/agent_config/AgentPromptTemplates.h"
#include "services/coding_agents/CodingAgentTypes.h"
#include "services/harness/HarnessRegistry.h"
#include "services/harness/IHarnessAdapter.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFileDialog>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTextCursor>
#include <QTextDocument>
#include <QUrl>
#include <QVBoxLayout>

#include <initializer_list>

namespace fincept::screens {

namespace ca = services::coding_agents;

namespace harness_run_options_detail {

constexpr int kComboMinChars = 10;

QLabel* hro_label(const char* object_name, QWidget* parent) {
    auto* l = new QLabel(parent);
    l->setObjectName(QString::fromLatin1(object_name));
    return l;
}

/// A combo that never sizes itself to its longest item (that pushed the RUN
/// button off the composer); the drop-down list is widened to its items instead.
void hro_narrow_combo(QComboBox* c) {
    c->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    c->setMinimumContentsLength(kComboMinChars);
    c->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void hro_fit_popup(QComboBox* c) {
    if (QAbstractItemView* list = c->view())
        list->setMinimumWidth(list->sizeHintForColumn(0) + 2 * list->frameWidth() + 24);
}

QString hro_html(const QString& s) {
    return s.toHtmlEscaped();
}

} // namespace harness_run_options_detail

using namespace harness_run_options_detail;

HarnessRunOptionsWidget::HarnessRunOptionsWidget(Purpose purpose, QWidget* parent)
    : QWidget(parent), purpose_(purpose) {
    build();
    retranslate();

    auto& registry = harness::HarnessRegistry::instance();
    connect(&registry, &harness::HarnessRegistry::status_changed, this, [this](const QString& id) {
        auto& reg = harness::HarnessRegistry::instance();
        if (harness_auto_ && unknown_harness_.isEmpty()) {
            const QString preferred = reg.preferred_harness();
            if (!preferred.isEmpty() && preferred != harness_) {
                harness_ = preferred;
                rebuild_harness_combo();
                apply_harness_defaults();
                emit harness_changed(harness_);
                emit options_changed();
                return;
            }
        }
        rebuild_harness_combo();
        if (id == harness_) {
            rebuild_models();
            rebuild_efforts();
            refresh_capabilities();
            rebuild_permissions();
            refresh_status_line();
        }
    });
    connect(&registry, &harness::HarnessRegistry::defaults_changed, this, [this]() {
        auto& reg = harness::HarnessRegistry::instance();
        if (harness_auto_ && unknown_harness_.isEmpty()) {
            const QString preferred = reg.preferred_harness();
            if (!preferred.isEmpty() && preferred != harness_) {
                harness_ = preferred;
                rebuild_harness_combo();
                apply_harness_defaults();
                emit harness_changed(harness_);
                emit options_changed();
                return;
            }
        }
        // A default changed in HARNESSES or Settings › AI Agents: follow it unless
        // the user already chose something else here.
        if (!model_touched_) {
            const QString def = reg.default_model(harness_);
            const QSignalBlocker b(model_combo_);
            const int idx = model_combo_->findData(def);
            if (idx >= 0)
                model_combo_->setCurrentIndex(idx);
            else
                model_combo_->setEditText(def);
        }
        rebuild_efforts();
        if (!effort_touched_) {
            const QSignalBlocker b(effort_combo_);
            const int idx = effort_combo_->findData(reg.default_effort(harness_));
            effort_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
        }
        rebuild_permissions();
        refresh_status_line();
    });
    // Rich-text colours and popup widths follow the theme / font — after the
    // appearance change has settled (never inside a theme_changed handler).
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        refresh_status_line();
        hro_fit_popup(harness_combo_);
        hro_fit_popup(model_combo_);
        hro_fit_popup(template_combo_);
    });

    harness_ = registry.preferred_harness();
    rebuild_harness_combo();
    apply_harness_defaults();
}

void HarnessRunOptionsWidget::build() {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(6);

    harness_lbl_ = hro_label("awbLabel", this);
    v->addWidget(harness_lbl_);
    harness_combo_ = new QComboBox(this);
    harness_model_ = new QStandardItemModel(harness_combo_);
    harness_combo_->setModel(harness_model_);
    hro_narrow_combo(harness_combo_);
    connect(harness_combo_, &QComboBox::activated, this, &HarnessRunOptionsWidget::on_harness_activated);
    v->addWidget(harness_combo_);

    status_lbl_ = hro_label("awbHint", this);
    status_lbl_->setTextFormat(Qt::RichText);
    status_lbl_->setWordWrap(true);
    status_lbl_->setTextInteractionFlags(Qt::TextBrowserInteraction);
    status_lbl_->setOpenExternalLinks(false);
    connect(status_lbl_, &QLabel::linkActivated, this, &HarnessRunOptionsWidget::on_status_link);
    v->addWidget(status_lbl_);

    model_lbl_ = hro_label("awbLabel", this);
    v->addWidget(model_lbl_);
    model_combo_ = new QComboBox(this);
    model_combo_->setEditable(true);
    model_combo_->setInsertPolicy(QComboBox::NoInsert);
    hro_narrow_combo(model_combo_);
    connect(model_combo_, &QComboBox::activated, this, [this](int) {
        model_touched_ = true;
        rebuild_efforts();
        emit options_changed();
    });
    connect(model_combo_->lineEdit(), &QLineEdit::textEdited, this, [this](const QString&) { model_touched_ = true; });
    connect(model_combo_->lineEdit(), &QLineEdit::editingFinished, this, [this]() {
        rebuild_efforts();
        emit options_changed();
    });
    v->addWidget(model_combo_);

    effort_lbl_ = hro_label("awbLabel", this);
    v->addWidget(effort_lbl_);
    effort_combo_ = new QComboBox(this);
    hro_narrow_combo(effort_combo_);
    connect(effort_combo_, &QComboBox::activated, this, [this](int) {
        effort_touched_ = true;
        emit options_changed();
    });
    v->addWidget(effort_combo_);

    mode_lbl_ = hro_label("awbLabel", this);
    v->addWidget(mode_lbl_);
    mode_combo_ = new QComboBox(this);
    // agents: picks what the run may do (read-only / workspace edits / full access) — a permission choice
    mode_combo_->setProperty("fincept_agent_risk", "account");
    hro_narrow_combo(mode_combo_);
    connect(mode_combo_, &QComboBox::activated, this, &HarnessRunOptionsWidget::on_permission_activated);
    v->addWidget(mode_combo_);

    tools_check_ = new QCheckBox(this);
    tools_check_->setChecked(true);
    connect(tools_check_, &QCheckBox::toggled, this, [this](bool) { emit options_changed(); });
    v->addWidget(tools_check_);
    tools_hint_ = hro_label("awbHint", this);
    tools_hint_->setWordWrap(true);
    v->addWidget(tools_hint_);

    limits_lbl_ = hro_label("awbLabel", this);
    v->addWidget(limits_lbl_);
    limits_grid_ = new QGridLayout;
    limits_grid_->setHorizontalSpacing(8);
    limits_grid_->setVerticalSpacing(4);
    budget_lbl_ = hro_label("awbHint", this);
    budget_spin_ = new QDoubleSpinBox(this);
    budget_spin_->setRange(0.0, 1000.0);
    budget_spin_->setDecimals(2);
    budget_spin_->setSingleStep(0.5);
    budget_spin_->setPrefix(QStringLiteral("$ "));
    time_lbl_ = hro_label("awbHint", this);
    time_spin_ = new QSpinBox(this);
    time_spin_->setRange(0, ca::kRunTimeLimitMaxSeconds / 60);
    time_spin_->setSingleStep(5);
    turns_lbl_ = hro_label("awbHint", this);
    turns_spin_ = new QSpinBox(this);
    turns_spin_->setRange(0, ca::kRunMaxTurnsLimit);
    limits_grid_->addWidget(budget_lbl_, 0, 0);
    limits_grid_->addWidget(budget_spin_, 0, 1);
    limits_grid_->addWidget(time_lbl_, 1, 0);
    limits_grid_->addWidget(time_spin_, 1, 1);
    limits_grid_->addWidget(turns_lbl_, 2, 0);
    limits_grid_->addWidget(turns_spin_, 2, 1);
    limits_grid_->setColumnStretch(1, 1);
    v->addLayout(limits_grid_);

    folder_lbl_ = hro_label("awbLabel", this);
    v->addWidget(folder_lbl_);
    auto* folder_row = new QHBoxLayout;
    folder_row->setSpacing(4);
    folder_edit_ = new QLineEdit(this);
    folder_edit_->setClearButtonEnabled(true);
    folder_row->addWidget(folder_edit_, 1);
    folder_browse_btn_ = new QPushButton(this);
    folder_browse_btn_->setObjectName(QStringLiteral("awbBtn"));
    folder_browse_btn_->setCursor(Qt::PointingHandCursor);
    connect(folder_browse_btn_, &QPushButton::clicked, this, &HarnessRunOptionsWidget::on_browse_folder);
    folder_row->addWidget(folder_browse_btn_);
    v->addLayout(folder_row);

    template_lbl_ = hro_label("awbLabel", this);
    v->addWidget(template_lbl_);
    template_combo_ = new QComboBox(this);
    hro_narrow_combo(template_combo_);
    connect(template_combo_, &QComboBox::activated, this, &HarnessRunOptionsWidget::on_template_activated);
    v->addWidget(template_combo_);

    prompt_lbl_ = hro_label("awbLabel", this);
    v->addWidget(prompt_lbl_);
    prompt_edit_ = new QPlainTextEdit(this);
    prompt_edit_->setObjectName(QStringLiteral("awbPrompt"));
    prompt_edit_->setMinimumHeight(purpose_ == Purpose::Composer ? 180 : 140);
    prompt_edit_->setTabChangesFocus(true);
    highlighter_ = new AgentPlaceholderHighlighter(prompt_edit_->document());
    v->addWidget(prompt_edit_, 1);
    if (purpose_ == Purpose::Composer) {
        for (const auto& seq : {QKeySequence(Qt::CTRL | Qt::Key_Return), QKeySequence(Qt::CTRL | Qt::Key_Enter)}) {
            auto* sc = new QShortcut(seq, prompt_edit_);
            sc->setContext(Qt::WidgetShortcut);
            connect(sc, &QShortcut::activated, this, &HarnessRunOptionsWidget::run_requested);
        }
    }
}

void HarnessRunOptionsWidget::retranslate() {
    harness_lbl_->setText(tr("HARNESS"));
    harness_combo_->setToolTip(tr("Installed harnesses first; the others are listed under “More” with how to install them."));
    model_lbl_->setText(tr("MODEL"));
    model_combo_->setToolTip(tr("Pick from the list or type any model id the CLI accepts."));
    if (model_combo_->lineEdit())
        model_combo_->lineEdit()->setPlaceholderText(tr("CLI default"));
    effort_lbl_->setText(tr("REASONING EFFORT"));
    mode_lbl_->setText(tr("MODE"));
    limits_lbl_->setText(tr("LIMITS"));
    budget_lbl_->setText(tr("Budget"));
    budget_spin_->setSpecialValueText(tr("No cap"));
    budget_spin_->setToolTip(tr("The run stops at this spend"));
    time_lbl_->setText(tr("Time limit"));
    time_spin_->setSuffix(tr(" min"));
    time_spin_->setSpecialValueText(tr("4 h (maximum)"));
    time_spin_->setToolTip(tr("The terminal stops the run after this long, whatever the harness"));
    turns_lbl_->setText(tr("Max turns"));
    turns_spin_->setSpecialValueText(tr("CLI default"));
    folder_lbl_->setText(tr("WORKING FOLDER"));
    folder_edit_->setPlaceholderText(tr("Auto — a new folder per run"));
    folder_browse_btn_->setText(tr("BROWSE"));
    template_lbl_->setText(tr("TEMPLATE"));
    {
        const QSignalBlocker block(template_combo_);
        template_combo_->clear();
        template_combo_->addItem(tr("Insert a starter prompt…"));
        for (const auto& t : agent_prompt_templates())
            template_combo_->addItem(t.title);
        template_combo_->setCurrentIndex(0);
        hro_fit_popup(template_combo_);
    }
    prompt_lbl_->setText(tr("PROMPT"));
    prompt_edit_->setPlaceholderText(purpose_ == Purpose::Composer
                                         ? tr("Describe the task — e.g. “Earnings preview for NVDA”. Templates insert a "
                                              "starting point; <PLACEHOLDERS> are highlighted.")
                                         : tr("What should the agent do on every run?"));
    rebuild_permissions();
    refresh_capabilities();
    refresh_status_line();
}

void HarnessRunOptionsWidget::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(event);
}

void HarnessRunOptionsWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    refresh_status_line(); // "rate limited until" and "detecting" age
}

// ── Harness ──────────────────────────────────────────────────────────────────

void HarnessRunOptionsWidget::rebuild_harness_combo() {
    auto& registry = harness::HarnessRegistry::instance();
    // Detection reports many small changes at start-up; never rebuild the list
    // under an open drop-down (it would close in the user's hand). The next
    // change, or the user's choice, rebuilds it.
    if (harness_combo_->view() && harness_combo_->view()->isVisible())
        return;
    const QSignalBlocker block(harness_combo_);
    harness_model_->clear();
    const QStringList installed = registry.installed_ids();
    QStringList others;
    for (const QString& id : registry.ids()) {
        if (!installed.contains(id))
            others << id;
    }
    auto add = [this, &registry](const QString& id, bool is_installed) {
        const harness::IHarnessAdapter* a = registry.adapter(id);
        QString text = registry.display_name(id);
        if (is_installed) {
            const harness::HarnessStatus st = registry.status(id);
            if (st.installation && !st.installation->version.isEmpty())
                text += QStringLiteral(" · ") + st.installation->version;
        }
        if (a && a->descriptor().beta)
            text += QStringLiteral(" · ") + tr("BETA");
        auto* item = new QStandardItem(text);
        item->setData(id, Qt::UserRole);
        item->setToolTip(awb_harness_state(id).sentence);
        harness_model_->appendRow(item);
    };
    for (const QString& id : installed)
        add(id, true);
    if (!others.isEmpty()) {
        auto* header = new QStandardItem(tr("More — not installed"));
        header->setFlags(Qt::NoItemFlags);
        harness_model_->appendRow(header);
        for (const QString& id : others)
            add(id, false);
    }
    if (!unknown_harness_.isEmpty()) {
        auto* item = new QStandardItem(registry.display_name(unknown_harness_));
        item->setData(unknown_harness_, Qt::UserRole);
        harness_model_->appendRow(item);
    }
    const int idx = harness_combo_->findData(harness_, Qt::UserRole);
    harness_combo_->setCurrentIndex(idx);
    hro_fit_popup(harness_combo_);
}

void HarnessRunOptionsWidget::on_harness_activated(int index) {
    const QString id = harness_combo_->itemData(index, Qt::UserRole).toString();
    if (id.isEmpty()) { // the "More" header
        rebuild_harness_combo();
        return;
    }
    harness_auto_ = false;
    if (id == harness_)
        return;
    harness_ = id;
    if (id != unknown_harness_)
        unknown_harness_.clear();
    rebuild_harness_combo();
    apply_harness_defaults();
    emit harness_changed(id);
    emit options_changed();
}

void HarnessRunOptionsWidget::apply_harness_defaults() {
    auto& registry = harness::HarnessRegistry::instance();
    model_touched_ = false;
    effort_touched_ = false;
    rebuild_models();
    {
        const QSignalBlocker b(model_combo_);
        const QString def = registry.default_model(harness_);
        const int idx = def.isEmpty() ? 0 : model_combo_->findData(def);
        if (idx >= 0)
            model_combo_->setCurrentIndex(idx);
        else
            model_combo_->setEditText(def);
    }
    rebuild_efforts();
    {
        const QSignalBlocker b(effort_combo_);
        const int idx = effort_combo_->findData(registry.default_effort(harness_));
        effort_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    refresh_capabilities();
    rebuild_permissions();
    refresh_status_line();
}

QString HarnessRunOptionsWidget::current_model() const {
    const QString text = model_combo_->currentText().trimmed();
    if (text.isEmpty())
        return {};
    const int idx = model_combo_->findText(text, Qt::MatchExactly);
    if (idx >= 0)
        return model_combo_->itemData(idx).toString();
    return text; // a custom model id
}

void HarnessRunOptionsWidget::rebuild_models() {
    auto& registry = harness::HarnessRegistry::instance();
    if (model_combo_->view() && model_combo_->view()->isVisible())
        return; // a model list arriving must not close the open drop-down
    const QString keep = current_model();
    const QSignalBlocker b(model_combo_);
    model_combo_->clear();
    model_combo_->addItem(tr("CLI default"), QString());
    const harness::HarnessStatus st = registry.status(harness_);
    for (const harness::ModelInfo& m : st.models) {
        const QString label = (m.label.isEmpty() || m.label == m.id) ? m.id : QStringLiteral("%1 (%2)").arg(m.label, m.id);
        model_combo_->addItem(m.is_default ? label + QStringLiteral(" — ") + tr("default") : label, m.id);
        if (!m.description.isEmpty())
            model_combo_->setItemData(model_combo_->count() - 1, m.description, Qt::ToolTipRole);
    }
    if (keep.isEmpty()) {
        model_combo_->setCurrentIndex(0);
    } else {
        const int idx = model_combo_->findData(keep);
        if (idx >= 0)
            model_combo_->setCurrentIndex(idx);
        else
            model_combo_->setEditText(keep);
    }
    if (model_combo_->lineEdit())
        model_combo_->lineEdit()->setPlaceholderText(tr("CLI default"));
    hro_fit_popup(model_combo_);
}

void HarnessRunOptionsWidget::rebuild_efforts() {
    auto& registry = harness::HarnessRegistry::instance();
    const harness::IHarnessAdapter* adapter = registry.adapter(harness_);
    const QString model = current_model();
    const QStringList efforts =
        (adapter && adapter->descriptor().caps.effort) ? registry.efforts(harness_, model) : QStringList{};
    QString default_effort;
    for (const harness::ModelInfo& m : registry.status(harness_).models) {
        if (m.id == model) {
            default_effort = m.default_effort;
            break;
        }
    }
    const QString keep = effort_combo_->currentData().toString();
    const QSignalBlocker b(effort_combo_);
    effort_combo_->clear();
    effort_combo_->addItem(default_effort.isEmpty() ? tr("Default") : tr("Default (%1)").arg(default_effort), QString());
    for (const QString& e : efforts)
        effort_combo_->addItem(e, e);
    const int idx = effort_combo_->findData(keep);
    effort_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
    const bool show = !efforts.isEmpty();
    effort_lbl_->setVisible(show);
    effort_combo_->setVisible(show);
}

void HarnessRunOptionsWidget::rebuild_permissions() {
    auto& registry = harness::HarnessRegistry::instance();
    const harness::IHarnessAdapter* adapter = registry.adapter(harness_);
    const bool full = registry.allow_full_access() && adapter && adapter->descriptor().caps.full_access;
    if (permission_ == harness::PermissionMode::FullAccess && !full)
        permission_ = harness::PermissionMode::ReadOnly;
    const QSignalBlocker b(mode_combo_);
    mode_combo_->clear();
    mode_combo_->addItem(tr("Read-only analysis"), harness::permission_id(harness::PermissionMode::ReadOnly));
    mode_combo_->addItem(tr("Workspace edits (may write files)"),
                         harness::permission_id(harness::PermissionMode::WorkspaceWrite));
    if (full)
        mode_combo_->addItem(tr("Full access (no sandbox, no approvals)"),
                             harness::permission_id(harness::PermissionMode::FullAccess));
    const int idx = mode_combo_->findData(harness::permission_id(permission_));
    mode_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
    hro_fit_popup(mode_combo_);
}

void HarnessRunOptionsWidget::on_permission_activated(int index) {
    const auto chosen = harness::permission_from_id(mode_combo_->itemData(index).toString())
                            .value_or(harness::PermissionMode::ReadOnly);
    if (chosen == harness::PermissionMode::FullAccess && permission_ != harness::PermissionMode::FullAccess) {
        const auto answer = QMessageBox::warning(
            this, tr("Full access"),
            tr("A full-access run has no sandbox and asks no approvals: the agent can run any command, change or delete "
               "any file your account can reach, and use the network.\n\nUse it only for a task you would run yourself "
               "in a terminal. Continue?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            const QSignalBlocker b(mode_combo_);
            mode_combo_->setCurrentIndex(qMax(0, mode_combo_->findData(harness::permission_id(permission_))));
            return;
        }
    }
    permission_ = chosen;
    emit options_changed();
}

void HarnessRunOptionsWidget::refresh_capabilities() {
    const harness::IHarnessAdapter* adapter = harness::HarnessRegistry::instance().adapter(harness_);
    const harness::Capabilities caps = adapter ? adapter->descriptor().caps : harness::Capabilities{};
    const QString name = awb_harness_name(harness_);
    budget_lbl_->setVisible(caps.budget_usd);
    budget_spin_->setVisible(caps.budget_usd);
    turns_lbl_->setVisible(caps.max_turns);
    turns_spin_->setVisible(caps.max_turns);
    tools_check_->setEnabled(caps.per_run_mcp && !read_only_);
    tools_check_->setText(tr("Terminal tools (MCP)"));
    tools_hint_->setText(caps.per_run_mcp
                             ? tr("Live quotes, history, fundamentals, filings, news, macro and portfolio data from this "
                                  "terminal. Orders still need your approval.")
                             : tr("%1 cannot be given the terminal's MCP server for one run — add the terminal to %1 on "
                                  "the CONNECT tab to use the tools in all its sessions.")
                                   .arg(name));
}

void HarnessRunOptionsWidget::refresh_status_line() {
    auto& registry = harness::HarnessRegistry::instance();
    const QString warn = QString(ui::colors::WARNING());
    const QString link = QString(ui::colors::AMBER());
    const harness::IHarnessAdapter* adapter = registry.adapter(harness_);
    const AwbHarnessState state = awb_harness_state(harness_);
    QStringList lines;
    QString head = QStringLiteral("<span style='color:%1'>●</span> <b>%2</b>")
                       .arg(awb_tone_color(state.tone), hro_html(state.word));
    if (!adapter) {
        lines << head + QStringLiteral(" — ") + awb_breakable_html(tr("this run was made with a harness this build does "
                                                                       "not have (%1); it is shown read-only.")
                                                                        .arg(harness_));
        status_lbl_->setText(lines.join(QStringLiteral("<br>")));
        return;
    }
    const harness::HarnessDescriptor& d = adapter->descriptor();
    const harness::HarnessStatus st = registry.status(harness_);
    if (st.installation) {
        const harness::Installation& inst = *st.installation;
        if (!inst.version.isEmpty())
            head += QStringLiteral(" · v") + hro_html(inst.version);
        // Paths and commands may break at separators, or one long path would set
        // the composer column's minimum width.
        head += QStringLiteral(" · ") + awb_breakable_html(QDir::toNativeSeparators(inst.path));
        if (inst.via == harness::Installation::NodeScript || inst.via == harness::Installation::CmdShim)
            head += QStringLiteral(" · ") + hro_html(tr("npm launcher"));
        lines << head;
        if (state.tone == AwbHarnessTone::Warning) {
            QString w = QStringLiteral("<span style='color:%1'>%2 %3</span>")
                            .arg(warn, hro_html(tr("WARNING:")), awb_breakable_html(state.sentence));
            const QString docs = d.auth_docs_url.isEmpty() ? d.docs_url : d.auth_docs_url;
            if (!docs.isEmpty())
                w += QStringLiteral(" <a style='color:%1' href='%2'>%3</a>").arg(link, docs.toHtmlEscaped(), hro_html(tr("HOW TO SIGN IN")));
            if (!d.signin_command.isEmpty())
                w += QStringLiteral(" · <a style='color:%1' href='copy:signin'>%2</a> %3")
                         .arg(link, hro_html(tr("COPY")), awb_breakable_html(d.signin_command));
            lines << w;
        } else if (state.tone == AwbHarnessTone::Busy) {
            lines << awb_breakable_html(state.sentence);
        }
        if (st.auth.api_key_env_present && !d.api_key_only) {
            if (registry.api_key_reaches_child(harness_))
                lines << QStringLiteral("<span style='color:%1'>%2</span>")
                             .arg(warn, hro_html(tr("API key in environment (%1): runs are billed to that key, not to "
                                                    "your subscription. Change it on HARNESSES.")
                                                     .arg(d.api_key_env_vars.join(QStringLiteral(", ")))));
            else
                lines << hro_html(tr("The API key in the environment is not passed to runs (subscription only)."));
        }
        if (!d.tested_version.isEmpty() && !inst.version.isEmpty() &&
            ca::compare_cli_versions(inst.version, d.tested_version) > 0)
            lines << hro_html(tr("Version %1 is newer than the tested %2 — the output format may have changed.")
                                  .arg(inst.version, d.tested_version));
    } else {
        lines << head + QStringLiteral(" — ") + awb_breakable_html(state.sentence);
        QString w;
        if (!d.install_url.isEmpty())
            w += QStringLiteral("<a style='color:%1' href='%2'>%3</a>").arg(link, d.install_url.toHtmlEscaped(), hro_html(tr("INSTALL GUIDE")));
        if (!d.install_command.isEmpty())
            w += (w.isEmpty() ? QString() : QStringLiteral(" · ")) +
                 QStringLiteral("<a style='color:%1' href='copy:install'>%2</a> %3")
                     .arg(link, hro_html(tr("COPY")), awb_breakable_html(d.install_command));
        if (!w.isEmpty())
            lines << w;
    }
    if (d.beta)
        lines << QStringLiteral("<span style='color:%1'>%2</span> %3")
                     .arg(warn, hro_html(tr("BETA")),
                          awb_breakable_html(d.beta_note.isEmpty() ? tr("Not yet checked against a recorded run.")
                                                                   : d.beta_note));
    if (!st.models_error.isEmpty())
        lines << awb_breakable_html(st.models_error);
    status_lbl_->setText(lines.join(QStringLiteral("<br>")));
}

void HarnessRunOptionsWidget::on_status_link(const QString& link) {
    const harness::IHarnessAdapter* adapter = harness::HarnessRegistry::instance().adapter(harness_);
    if (link == QLatin1String("copy:signin") && adapter) {
        QGuiApplication::clipboard()->setText(adapter->descriptor().signin_command);
        return;
    }
    if (link == QLatin1String("copy:install") && adapter) {
        QGuiApplication::clipboard()->setText(adapter->descriptor().install_command);
        return;
    }
    if (link.startsWith(QLatin1String("https://")) || link.startsWith(QLatin1String("http://")))
        QDesktopServices::openUrl(QUrl(link));
}

// ── Template, folder, prompt ─────────────────────────────────────────────────

void HarnessRunOptionsWidget::on_template_activated(int index) {
    if (index <= 0)
        return;
    const auto templates = agent_prompt_templates();
    if (index - 1 >= templates.size())
        return;
    const AgentPromptTemplate& t = templates[index - 1];
    if (prompt_edit_->toPlainText().trimmed().isEmpty() || purpose_ == Purpose::Schedule) {
        prompt_edit_->setPlainText(t.body);
    } else {
        QTextCursor c = prompt_edit_->textCursor();
        c.movePosition(QTextCursor::End);
        c.insertText(QStringLiteral("\n\n") + t.body);
    }
    if (t.needs_workspace && permission_ == harness::PermissionMode::ReadOnly) {
        permission_ = harness::PermissionMode::WorkspaceWrite; // the template asks for files in the working folder
        rebuild_permissions();
    }
    {
        const QSignalBlocker b(template_combo_);
        template_combo_->setCurrentIndex(0);
    }
    select_first_placeholder();
    prompt_edit_->setFocus();
    emit template_inserted(t.title);
    emit options_changed();
}

void HarnessRunOptionsWidget::select_first_placeholder() {
    const QTextCursor c = prompt_edit_->document()->find(agent_placeholder_regex());
    if (!c.isNull())
        prompt_edit_->setTextCursor(c);
}

void HarnessRunOptionsWidget::insert_template(int index) {
    if (read_only_)
        return;
    on_template_activated(index + 1); // row 0 of the list is "Insert a starter prompt…"
}

void HarnessRunOptionsWidget::open_templates() {
    template_combo_->setFocus();
    template_combo_->showPopup();
}

void HarnessRunOptionsWidget::set_prompt(const QString& prompt) {
    prompt_edit_->setPlainText(prompt);
    select_first_placeholder();
}

void HarnessRunOptionsWidget::on_browse_folder() {
    const QString typed = folder_edit_->text().trimmed();
    const QString start = typed.isEmpty() ? QDir::homePath() : typed;
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Choose the agent's working folder"), start);
    // Flatpak: the dialog answers with a document-portal path; the agent runs on
    // the host, in the folder's real path.
    if (!dir.isEmpty())
        folder_edit_->setText(QDir::toNativeSeparators(process::host::document_host_path(dir)));
}

// ── Values ───────────────────────────────────────────────────────────────────

QString HarnessRunOptionsWidget::harness() const {
    return harness_;
}

bool HarnessRunOptionsWidget::harness_known() const {
    return harness::HarnessRegistry::instance().is_known(harness_);
}

bool HarnessRunOptionsWidget::harness_runnable(QString* why) const {
    auto& registry = harness::HarnessRegistry::instance();
    if (harness_.isEmpty()) {
        if (why)
            *why = tr("No harness is selected.");
        return false;
    }
    if (!registry.is_known(harness_)) {
        if (why)
            *why = tr("%1 is not available in this build.").arg(registry.display_name(harness_));
        return false;
    }
    if (const harness::HarnessStatus st = registry.status(harness_); !st.installed()) {
        if (why)
            *why = st.host_blocked ? st.detect_error
                                   : tr("%1 is not installed on this computer.").arg(registry.display_name(harness_));
        return false;
    }
    return true;
}

HarnessRunOptions HarnessRunOptionsWidget::options() const {
    HarnessRunOptions o;
    o.harness = harness_;
    o.model = current_model();
    o.effort = effort_combo_->isHidden() ? QString() : effort_combo_->currentData().toString();
    o.permission = permission_;
    o.tools = tools_check_->isChecked();
    o.budget_usd = budget_spin_->isHidden() ? 0.0 : budget_spin_->value();
    o.time_limit_s = time_spin_->value() * 60;
    o.max_turns = turns_spin_->isHidden() ? 0 : turns_spin_->value();
    o.workdir = QDir::fromNativeSeparators(folder_edit_->text().trimmed());
    o.prompt = prompt_edit_->toPlainText().trimmed();
    return o;
}

void HarnessRunOptionsWidget::set_options(const HarnessRunOptions& o) {
    auto& registry = harness::HarnessRegistry::instance();
    const QString id = o.harness.trimmed();
    unknown_harness_ = (!id.isEmpty() && !registry.is_known(id)) ? id : QString();
    harness_ = id.isEmpty() ? registry.preferred_harness() : id;
    harness_auto_ = id.isEmpty();
    rebuild_harness_combo();
    rebuild_models();
    {
        const QSignalBlocker b(model_combo_);
        const int idx = o.model.isEmpty() ? 0 : model_combo_->findData(o.model);
        if (idx >= 0)
            model_combo_->setCurrentIndex(idx);
        else
            model_combo_->setEditText(o.model);
    }
    model_touched_ = true; // an explicit value, e.g. a saved schedule's
    rebuild_efforts();
    {
        const QSignalBlocker b(effort_combo_);
        const int idx = effort_combo_->findData(o.effort);
        effort_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    effort_touched_ = true;
    permission_ = o.permission;
    rebuild_permissions();
    {
        const QSignalBlocker b(tools_check_);
        tools_check_->setChecked(o.tools);
    }
    budget_spin_->setValue(o.budget_usd);
    time_spin_->setValue(o.time_limit_s / 60);
    turns_spin_->setValue(o.max_turns);
    folder_edit_->setText(QDir::toNativeSeparators(o.workdir));
    prompt_edit_->setPlainText(o.prompt);
    refresh_capabilities();
    refresh_status_line();
}

void HarnessRunOptionsWidget::reset_to_defaults() {
    HarnessRunOptions o;
    set_options(o);
    harness_auto_ = true;
    apply_harness_defaults();
}

void HarnessRunOptionsWidget::set_read_only(bool read_only) {
    read_only_ = read_only;
    for (QWidget* w : std::initializer_list<QWidget*>{harness_combo_, model_combo_, effort_combo_, mode_combo_,
                                                      budget_spin_, time_spin_, turns_spin_, folder_edit_,
                                                      folder_browse_btn_, template_combo_})
        w->setEnabled(!read_only);
    prompt_edit_->setReadOnly(read_only);
    refresh_capabilities();
}

QVariantMap HarnessRunOptionsWidget::save_state() const {
    const HarnessRunOptions o = options();
    QVariantMap m{{"model", o.model},
                  {"effort", o.effort},
                  {"permission", harness::permission_id(o.permission)},
                  {"tools", o.tools},
                  {"budget", o.budget_usd},
                  {"time_limit_s", o.time_limit_s},
                  {"max_turns", o.max_turns}};
    if (!harness_auto_)
        m.insert(QStringLiteral("harness"), o.harness); // a choice the user made; else follow the default
    return m;
}

void HarnessRunOptionsWidget::restore_state(const QVariantMap& state) {
    HarnessRunOptions o;
    // State version 1 stored "agent" ("claude" | "codex") and "mode".
    o.harness = ca::normalize_harness_id(state.value("harness", state.value("agent")).toString());
    if (!o.harness.isEmpty() && !harness::HarnessRegistry::instance().is_known(o.harness))
        o.harness.clear(); // a composer never starts on a harness this build does not have
    o.model = state.value("model").toString();
    o.effort = state.value("effort").toString();
    o.permission = ca::permission_from_columns(state.value("permission").toString(), state.value("mode").toString());
    o.tools = state.value("tools", true).toBool();
    o.budget_usd = state.value("budget", 0.0).toDouble();
    o.time_limit_s = state.value("time_limit_s", 0).toInt();
    o.max_turns = state.value("max_turns", 0).toInt();
    o.prompt = prompt_edit_->toPlainText();
    o.workdir = QDir::fromNativeSeparators(folder_edit_->text().trimmed());
    set_options(o);
    if (o.model.isEmpty())
        model_touched_ = false; // follow the registry default
    if (o.effort.isEmpty())
        effort_touched_ = false;
}

} // namespace fincept::screens
