// src/screens/ai_quant_lab/QuantModulePanel_DeepAgent.cpp
//
// Deep Agent panel — tabbed: LangGraph Deep Analysis + RD-Agent Research.
// Extracted from QuantModulePanel.cpp to keep that file maintainable.
#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"
#include "services/ai_quant_lab/AIQuantLabService.h"
#include "services/billing/PlanAccess.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/Requirements.h"
#include "ui/widgets/EmptyState.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QTabWidget>
#include <QTimer>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

using namespace fincept::services::quant;
using namespace fincept::screens::quant_styles;
using namespace fincept::screens::quant_common;
using namespace fincept::screens::quant_gs_helpers;

namespace {

// Set on the RUN DEEP ANALYSIS button while check_status reports the deepagents
// library unavailable.
constexpr const char* kDeepUnavailableProp = "deepUnavailable";

// clear_results(), set_run_buttons_enabled() and the deep_agent result / error
// handlers re-enable every run button of the panel (an RD-Agent reply included).
// A button carrying kDeepUnavailableProp is held disabled here instead.
class HoldDisabledFilter final : public QObject {
  public:
    using QObject::QObject;

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::EnabledChange) {
            auto* w = qobject_cast<QWidget*>(watched);
            if (w && w->isEnabled() && w->property(kDeepUnavailableProp).toBool())
                w->setEnabled(false);
        }
        return QObject::eventFilter(watched, event);
    }
};

/// The cost line of an LLM run under the selected profile (`llm_cfg` = llm_config_from_combo()).
/// With Fincept's hosted model and the server's plan table: what this account's plan charges for
/// AI calls (category "ai"), after `duration`. Any other provider bills its own tokens, and without
/// a plan table nothing is known about the plan: `fallback` (the line shown before plans existed).
QString deep_llm_cost_note(const QJsonObject& llm_cfg, const QString& fallback, const QString& duration) {
    const auto& access = fincept::billing::PlanAccess::instance();
    const bool fincept_model = llm_cfg.value(QStringLiteral("llm_provider"))
                                   .toString()
                                   .compare(QLatin1String("fincept"), Qt::CaseInsensitive) == 0;
    if (!fincept_model || !access.has_table())
        return fallback;
    if (access.included(QStringLiteral("ai")))
        return QCoreApplication::translate("QuantModulePanel",
                                           "%1 · Fincept's hosted model · included with your plan, no credits used")
            .arg(duration);
    if (const fincept::billing::PlanPrice* row = access.price(QStringLiteral("ai"))) {
        const QString price = access.is_paid() ? row->pro_price : row->free_price;
        if (!price.isEmpty())
            return QCoreApplication::translate("QuantModulePanel",
                                               "%1 · Fincept's hosted model, many calls per run · %2 on your plan")
                .arg(duration, price);
    }
    return fallback;
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════════════
// ═══════════════════════════════════════════════════════════════════════════════
// DEEP AGENT PANEL — tabbed: LangGraph Deep Analysis + RD-Agent Research
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* QuantModulePanel::build_deep_agent_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Shared LLM profile picker (used by both Deep Analysis and RD-Agent) ──
    QComboBox* llm_combo = nullptr;
    auto* llm_bar = new QWidget(w);
    llm_bar->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_MED()));
    auto* llm_bar_l = new QHBoxLayout(llm_bar);
    llm_bar_l->setContentsMargins(16, 8, 16, 8);
    llm_bar_l->setSpacing(0);
    llm_bar_l->addWidget(build_llm_picker(llm_bar, &llm_combo));
    vl->addWidget(llm_bar);

    auto* tabs = make_view_tabs(w);

    // ── Tab 1: LangGraph Deep Analysis ──────────────────────────────────────
    // Several LLM calls, minutes: never runs by itself. Opens on the last report ("as of"), else on
    // a preview of what it returns, with RUN DEEP ANALYSIS; with no LLM profile, on the setup page.
    ViewSpec da_spec;
    da_spec.key = QStringLiteral("deep_analysis");
    da_spec.cost = ViewCost::Heavy;
    da_spec.loading = tr("Running the multi-agent analysis — several LLM calls, typically a few minutes…");
    da_spec.shape = ui::LoadingSkeleton::Shape::Text;
    da_spec.llm = true;
    da_spec.title = tr("Multi-agent deep analysis");
    da_spec.returns = tr("A written research report: the orchestrator delegates the task to specialist agents "
                         "(research, data analysis, risk, trading, portfolio) and combines their findings.");
    da_spec.run_text = tr("RUN DEEP ANALYSIS");
    da_spec.cost_note = tr("Calls the selected LLM profile many times · a few minutes · uses its tokens");
    add_view_tab(tabs, da_spec, tr("Deep Analysis"), [this, llm_combo](QWidget* da_w, QVBoxLayout* da_vl) {
        da_vl->setContentsMargins(16, 16, 16, 16);
        da_vl->setSpacing(12);

        auto* desc =
            new QLabel(tr("Multi-agent financial analysis powered by LangGraph. Delegates to specialist subagents "
                          "(research, data-analyst, trading, risk-analyzer, portfolio-optimizer, backtester, reporter)."),
                       da_w);
        desc->setWordWrap(true);
        desc->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                .arg(ui::colors::TEXT_SECONDARY())
                                .arg(ui::fonts::px(-1))
                                .arg(ui::fonts::DATA_FAMILY));
        da_vl->addWidget(desc);

        auto* task_edit = new QTextEdit(da_w);
        task_edit->setPlaceholderText(tr("Describe your analysis task...\n"
                                         "e.g. \"Conduct a full investment analysis of NVDA: research fundamentals, "
                                         "assess risks, and give a buy/sell/hold recommendation with price target\""));
        task_edit->setPlainText(tr("Conduct a full investment analysis of NVDA: research fundamentals, assess risks, "
                                   "and give a buy/sell/hold recommendation with a price target."));
        task_edit->setFixedHeight(90);
        task_edit->setStyleSheet(QString("QTextEdit { background:%1; color:%2; border:1px solid %3;"
                                         "font-family:%4; font-size:%5px; padding:6px; }")
                                     .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                                     .arg(ui::fonts::DATA_FAMILY)
                                     .arg(ui::fonts::px(-1)));
        da_vl->addWidget(build_input_row(tr("Task"), task_edit, da_w));

        auto* agent_type = new QComboBox(da_w);
        // Shown while list_capabilities loads, and kept if it fails (probes below).
        agent_type->addItems({"general", "research", "trading_strategy", "portfolio_management", "risk_assessment"});
        agent_type->setStyleSheet(combo_ss());
        combo_inputs_["agent_type"] = agent_type;
        da_vl->addWidget(build_input_row(tr("Agent Type"), agent_type, da_w));

        auto* thread_id = new QLineEdit(da_w);
        thread_id->setPlaceholderText(tr("Optional — leave blank to auto-generate"));
        thread_id->setStyleSheet(QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                                         "font-family:%4; font-size:%5px; padding:6px 8px; }")
                                     .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                                     .arg(ui::fonts::DATA_FAMILY)
                                     .arg(ui::fonts::px(-1)));
        text_inputs_["thread_id"] = thread_id;
        da_vl->addWidget(build_input_row(tr("Thread ID"), thread_id, da_w));

        // Availability notice, filled from check_status (probes below); hidden while
        // the library is available or the check has not answered yet.
        auto* da_notice = new QLabel(da_w);
        da_notice->setObjectName(QStringLiteral("deepAgentNotice"));
        da_notice->setWordWrap(true);
        da_notice->setTextInteractionFlags(Qt::TextSelectableByMouse);
        da_notice->setVisible(false);
        da_vl->addWidget(da_notice);

        auto* da_run = make_run_button(tr("RUN DEEP ANALYSIS"), da_w);
        da_run->setProperty("fincept_agent_risk", "spend"); // LLM agent run
        // Re-enabled by the execute_task result / the deep_agent error handler.
        da_run->setProperty("deepRun", true);
        da_run->installEventFilter(new HoldDisabledFilter(da_run));
        set_action(da_run, [this, da_run, task_edit, agent_type, thread_id, llm_combo]() {
            if (da_run->property(kDeepUnavailableProp).toBool()) {
                display_error(tr("The Deep Agent is not available here (see the notice above)."));
                return;
            }
            auto task = task_edit->toPlainText().trimmed();
            if (task.isEmpty()) {
                display_error(tr("Please enter an analysis task."));
                return;
            }
            auto cfg = llm_config_from_combo(llm_combo);
            if (cfg.isEmpty()) {
                display_error(tr("Select an LLM profile before running (Settings > LLM Config)."));
                return;
            }
            show_loading(tr("Running the multi-agent analysis — the orchestrator delegates to specialist agents and "
                            "calls the LLM several times; this typically takes a few minutes…"));
            QJsonObject params;
            params["task"] = task;
            params["agent_type"] = agent_type->currentText();
            params["config"] = cfg;
            auto tid = thread_id->text().trimmed();
            if (!tid.isEmpty())
                params["thread_id"] = tid;
            AIQuantLabService::instance().run_deep_agent(params);
    });
    da_vl->addWidget(da_run);

    // ── Probes: agent types (list_capabilities), then availability (check_status) ──
    // Answered through callbacks, not result_ready (see AIQuantLabService). Run one
    // after the other so the panel never holds two of PythonRunner's slots.
    auto set_notice = [da_notice](const QString& text, const ui::ColorToken& tone) {
        da_notice->setText(text);
        da_notice->setStyleSheet(QString("color:%1; background:%2; border-left:3px solid %1; padding:6px 10px;"
                                         "font-size:" + ui::fonts::px(-1) + "px; font-family:%3;")
                                     .arg(tone(), ui::colors::tint(tone))
                                     .arg(ui::fonts::DATA_FAMILY));
        da_notice->setVisible(!text.isEmpty());
    };
    auto on_status = [da_run, set_notice](const QJsonObject& status) {
        const QJsonValue available = status.value(QStringLiteral("available"));
        if (available.isBool() && !available.toBool()) {
            QString why = status.value(QStringLiteral("error")).toString().trimmed();
            if (why.isEmpty())
                why = tr("the deepagents library could not be loaded");
            // Run stays usable: without the library the script falls back to a
            // single-model run for providers that do not need tool calling, and
            // reports its own error otherwise. Holding the button disabled would
            // block that path.
            const QString text =
                tr("Deep Agent library is not available (%1). A run uses the basic single-model fallback.").arg(why);
            set_notice(text, ui::colors::WARNING);
            da_run->setToolTip(text);
            return;
        }
        if (da_run->property(kDeepUnavailableProp).toBool()) {
            da_run->setProperty(kDeepUnavailableProp, false);
            da_run->setEnabled(true);
            da_run->setToolTip(QString());
        }
        if (available.isBool()) {
            set_notice(QString(), ui::colors::DANGER);
        } else {
            // The check itself failed: availability unknown, so Run stays usable.
            set_notice(tr("Could not check Deep Agent availability: %1")
                           .arg(status.value(QStringLiteral("error")).toString(tr("no diagnostic output"))),
                       ui::colors::WARNING);
        }
    };
    // The callbacks only run while da_w (which owns every widget they touch) exists.
    AIQuantLabService::instance().deep_agent_list_capabilities(
        da_w, [da_w, agent_type, on_status](const QJsonObject& caps) {
            const QJsonObject types = caps.value(QStringLiteral("agent_types")).toObject();
            if (!types.isEmpty()) {
                // QJsonObject keys arrive sorted; "general" (the default) goes first.
                QStringList names = types.keys();
                if (names.removeOne(QStringLiteral("general")))
                    names.prepend(QStringLiteral("general"));
                const QString current = agent_type->currentText();
                agent_type->clear();
                for (const QString& name : names) {
                    QStringList subs;
                    for (const auto& s : types.value(name).toArray())
                        subs << s.toString();
                    agent_type->addItem(name);
                    agent_type->setItemData(agent_type->count() - 1, tr("Subagents: %1").arg(subs.join(", ")),
                                            Qt::ToolTipRole);
                }
                const int keep = agent_type->findText(current);
                agent_type->setCurrentIndex(keep >= 0 ? keep : 0);
            }
            // Otherwise (failed or empty reply) the built-in list stays.
            AIQuantLabService::instance().deep_agent_check_status(da_w, on_status);
        });
    });

    // The preview's cost line follows the selected profile and the account's plan (Fincept's hosted
    // model: the plan's AI price; another provider bills its own tokens — the line above). The view
    // stays manual even when the plan includes AI calls: a run is minutes of agent work.
    {
        const QString da_fallback = da_spec.cost_note;
        auto sync_da_cost = [this, llm_combo, da_fallback]() {
            auto it = views_.find(QStringLiteral("deep_analysis"));
            if (it == views_.end())
                return;
            const QString note =
                deep_llm_cost_note(llm_config_from_combo(llm_combo), da_fallback, tr("A few minutes"));
            if (note == it->spec.cost_note)
                return;
            it->spec.cost_note = note;
            if (pane_ && current_view_ == it->spec.key)
                configure_preview(*it);
        };
        sync_da_cost();
        connect(llm_combo, &QComboBox::currentIndexChanged, w, sync_da_cost);
        connect(&fincept::billing::PlanAccess::instance(), &fincept::billing::PlanAccess::changed, w, sync_da_cost);
    }

    // ── Tab 2: RD-Agent ─────────────────────────────────────────────────────
    // Autonomous research loops (tens of minutes to hours, LLM calls every iteration) running in
    // the background; their progress and results live in the tab's own Task Monitor, so the result
    // pane is hidden for this view. Opening it checks the RD-Agent install (seconds, no LLM).
    ViewSpec rd_spec;
    rd_spec.key = QStringLiteral("rd_agent");
    rd_spec.cost = ViewCost::Heavy;
    rd_spec.pane = false;
    add_view_tab(tabs, rd_spec, tr("RD-Agent"), [this, llm_combo](QWidget* page, QVBoxLayout* layout) {
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->addWidget(build_rd_agent_tab(llm_combo), 1);
        AIQuantLabService::instance().rd_agent_check_status();
        Q_UNUSED(page)
    });

    vl->addWidget(tabs);
    add_result_pane(vl);
    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// RD-AGENT TAB — Autonomous factor/model research (Microsoft RD-Agent)
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* QuantModulePanel::build_rd_agent_tab(QComboBox* llm_combo) {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // shared styles

    auto* sub = new QTabWidget(w);
    sub->setObjectName(QStringLiteral("rdSubTabs")); // a START switches to Task Monitor (on_result)
    sub->setUsesScrollButtons(true);
    sub->setStyleSheet(sub_tab_ss(ui::colors::legible(module_.color)));

    // ── Status bar ───────────────────────────────────────────────────────────
    auto* status_bar = new QWidget(w);
    status_bar->setMinimumHeight(28);
    status_bar->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_MED()));
    auto* sbl = new QHBoxLayout(status_bar);
    sbl->setContentsMargins(12, 0, 12, 0);
    sbl->setSpacing(8);
    auto* status_dot = new QLabel("●", status_bar);
    status_dot->setStyleSheet(QString("color:%1;").arg(ui::colors::TEXT_SECONDARY()));
    sbl->addWidget(status_dot);
    // Not "ready" before anything was checked: opening the tab runs CHECK STATUS (on_result fills it).
    auto* status_txt = new QLabel(tr("Checking the RD-Agent installation…"), status_bar);
    status_txt->setObjectName("rdStatusTxt");
    status_txt->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                  .arg(ui::colors::TEXT_SECONDARY())
                                  .arg(ui::fonts::px(-2))
                                  .arg(ui::fonts::DATA_FAMILY));
    sbl->addWidget(status_txt, 1);
    auto* check_btn = new QPushButton(tr("CHECK STATUS"), status_bar);
    check_btn->setCursor(Qt::PointingHandCursor);
    check_btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %1;"
                                     "font-family:%2; font-size:%3px; padding:2px 8px; border-radius:2px; }"
                                     "QPushButton:hover { background:%1; color:%4; }")
                                 .arg(ui::colors::legible(module_.color))
                                 .arg(ui::fonts::DATA_FAMILY)
                                 .arg(ui::fonts::px(-2))
                                 .arg(ui::colors::TEXT_ON_ACCENT()));
    connect(check_btn, &QPushButton::clicked, this, [status_txt]() {
        status_txt->setText(tr("Checking..."));
        AIQuantLabService::instance().rd_agent_check_status();
    });
    sbl->addWidget(check_btn);

    auto* ui_btn = new QPushButton(tr("OPEN LOG VIEWER"), status_bar);
    ui_btn->setCursor(Qt::PointingHandCursor);
    ui_btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %1;"
                                  "font-family:%2; font-size:%3px; padding:2px 8px; border-radius:2px; }"
                                  "QPushButton:hover { background:%1; color:%4; }")
                              .arg(ui::colors::TEXT_SECONDARY())
                              .arg(ui::fonts::DATA_FAMILY)
                              .arg(ui::fonts::px(-2))
                              .arg(ui::colors::BG_BASE()));
    connect(ui_btn, &QPushButton::clicked, this, [status_txt]() {
        status_txt->setText(tr("Starting log viewer..."));
        AIQuantLabService::instance().rd_agent_start_ui();
    });
    sbl->addWidget(ui_btn);

    // Not checkable: there is no rd_agent "stop MCP server" command, so a
    // toggle that un-checks would claim to have stopped a server that is still
    // running. Plain action + a status query instead.
    auto* mcp_btn = new QPushButton(tr("START MCP TOOLS"), status_bar);
    mcp_btn->setProperty("fincept_agent_risk", "code"); // starts a background tool server process
    mcp_btn->setCursor(Qt::PointingHandCursor);
    mcp_btn->setToolTip(tr("Start the Fincept MCP tool server.\n"
                           "Gives RD-Agent loops access to market data,\n"
                           "financial news and economics tools.\n"
                           "The server keeps running until the terminal exits."));
    mcp_btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %1;"
                                   "font-family:%2; font-size:%3px; padding:2px 8px; border-radius:2px; }"
                                   "QPushButton:hover { background:%1; color:%4; }")
                               .arg(ui::colors::TEXT_SECONDARY())
                               .arg(ui::fonts::DATA_FAMILY)
                               .arg(ui::fonts::px(-2))
                               .arg(ui::colors::BG_BASE()));
    connect(mcp_btn, &QPushButton::clicked, this, [status_txt]() {
        status_txt->setText(tr("Starting MCP tool server..."));
        AIQuantLabService::instance().rd_agent_start_mcp_server();
    });
    sbl->addWidget(mcp_btn);

    auto* mcp_status_btn = new QPushButton(tr("MCP STATUS"), status_bar);
    mcp_status_btn->setCursor(Qt::PointingHandCursor);
    mcp_status_btn->setToolTip(tr("Report whether the MCP tool server is installed and which ports are live."));
    mcp_status_btn->setStyleSheet(mcp_btn->styleSheet());
    connect(mcp_status_btn, &QPushButton::clicked, this, [status_txt]() {
        status_txt->setText(tr("Querying MCP status..."));
        AIQuantLabService::instance().rd_agent_mcp_status();
    });
    sbl->addWidget(mcp_status_btn);

    // Read by the three START handlers below: the RD-Agent CLI only hands the Fincept
    // MCP tools to a loop when its params carry enable_mcp (it starts or reuses the
    // server itself). Nothing in the panel used to set it.
    auto* mcp_check = new QCheckBox(tr("Use Fincept MCP tools"), status_bar);
    mcp_check->setToolTip(tr("Give Factor Mining, Model Optimization and Quant Research runs the Fincept MCP\n"
                             "tools (market data, news, economics). The tool server is started if needed."));
    mcp_check->setStyleSheet(QString("QCheckBox { color:%1; font-family:%2; font-size:%3px; background:transparent; }")
                                 .arg(ui::colors::TEXT_SECONDARY())
                                 .arg(ui::fonts::DATA_FAMILY)
                                 .arg(ui::fonts::px(-2)));
    sbl->addWidget(mcp_check);
    vl->addWidget(status_bar);

    // What a START costs, under each form; with no LLM profile, where to add one.
    // The cost line follows the selected profile and the account's plan (deep_llm_cost_note).
    auto add_cost_card = [this, llm_combo](QWidget* parent, QVBoxLayout* layout, const QString& title,
                                           const QString& what) {
        const ui::RequirementStatus llm = ui::check_requirements({ui::Requirement::llm()});
        auto cost_spec = [this, llm_combo, title, what]() {
            return ui::EmptyState::preview_spec(
                title, what, QString(),
                deep_llm_cost_note(llm_config_from_combo(llm_combo),
                                   tr("Runs in the background for tens of minutes to hours · calls the selected LLM "
                                      "every iteration · follow it in Task Monitor"),
                                   tr("Tens of minutes to hours in the background")),
                ui::EmptyState::Tone::Warning, ui::LoadingSkeleton::Shape::Table);
        };
        ui::EmptyState::Spec spec = llm.ok ? cost_spec() : ui::EmptyState::needs_setup_spec(llm);
        auto* card = ui::EmptyState::create(spec, parent);
        card->set_compact(true);
        card->set_alignment(Qt::AlignTop);
        if (!llm.ok) {
            connect(card, &ui::EmptyState::primary_clicked, this, [card, link = llm.link]() { ui::open_setup_link(link, card); });
        } else {
            // apply() of an unchanged spec does nothing.
            auto refresh = [card, cost_spec]() { card->apply(cost_spec()); };
            connect(llm_combo, &QComboBox::currentIndexChanged, card, refresh);
            connect(&fincept::billing::PlanAccess::instance(), &fincept::billing::PlanAccess::changed, card, refresh);
        }
        layout->addWidget(card);
    };

    auto apply_mcp = [mcp_check](QJsonObject& params) {
        if (mcp_check->isChecked()) {
            params["enable_mcp"] = true;
            params["mcp_port"] = 18765;
        }
    };

    // Validation errors raised from the RD-Agent sub-tabs must NOT go through
    // display_error(): that paints into the Deep Analysis tab's results pane,
    // which is on the other page of the outer QTabWidget and therefore invisible
    // to whoever just clicked. Surface them in this tab instead.
    auto rd_error = [this, status_txt](const QString& msg) {
        status_txt->setText(msg);
        if (rd_agent_output_)
            rd_agent_output_->setPlainText(msg);
        if (status_label_)
            status_label_->setText(tr("Error"));
    };

    // ── Sub-tab 1: Factor Mining ─────────────────────────────────────────────
    auto* fm_w = new QWidget(sub);
    auto* fm_vl = new QVBoxLayout(fm_w);
    fm_vl->setContentsMargins(14, 14, 14, 14);
    fm_vl->setSpacing(10);

    auto* fm_desc = new QLabel(tr("Autonomous alpha factor discovery via FactorRDLoop. The agent proposes, codes, runs "
                                  "and evaluates factors iteratively until the target IC is reached."),
                               fm_w);
    fm_desc->setWordWrap(true);
    fm_desc->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                               .arg(ui::colors::TEXT_SECONDARY())
                               .arg(ui::fonts::px(-1))
                               .arg(ui::fonts::DATA_FAMILY));
    fm_vl->addWidget(fm_desc);

    auto* fm_task = new QTextEdit(fm_w);
    fm_task->setPlaceholderText(
        tr("Describe the factor hypothesis...\ne.g. \"Discover momentum-based alpha factors for US equities\""));
    fm_task->setPlainText(tr("Discover momentum-based alpha factors for US equities"));
    fm_task->setFixedHeight(70);
    fm_task->setStyleSheet(input_ss());
    fm_vl->addWidget(build_input_row(tr("Task Description"), fm_task, fm_w));

    auto* fm_market = new QComboBox(fm_w);
    fm_market->addItems({"US", "CN", "EU", "IN", "JP", "HK", "GLOBAL"});
    fm_market->setStyleSheet(combo_ss());
    fm_vl->addWidget(build_input_row(tr("Target Market"), fm_market, fm_w));

    auto* fm_iter = new QSpinBox(fm_w);
    fm_iter->setRange(1, 100);
    fm_iter->setValue(10);
    fm_iter->setStyleSheet(spinbox_ss());
    fm_vl->addWidget(build_input_row(tr("Max Iterations"), fm_iter, fm_w));

    auto* fm_ic = make_double_spin(0.01, 0.50, 0.05, 3, "", fm_w);
    fm_vl->addWidget(build_input_row(tr("Target IC"), fm_ic, fm_w));

    auto* fm_run = make_run_button(tr("START FACTOR MINING"), fm_w);
    fm_run->setProperty("fincept_agent_risk", "spend"); // RD-Agent LLM loop
    connect(fm_run, &QPushButton::clicked, this,
            [this, fm_task, fm_market, fm_iter, fm_ic, status_txt, llm_combo, rd_error, apply_mcp]() {
                auto desc_text = fm_task->toPlainText().trimmed();
                if (desc_text.isEmpty()) {
                    rd_error(tr("Enter a task description."));
                    return;
                }
                auto cfg = llm_config_from_combo(llm_combo);
                if (cfg.isEmpty()) {
                    rd_error(tr("Select an LLM profile before running (Settings > LLM Config)."));
                    return;
                }
                status_label_->setText(tr("Starting..."));
                status_txt->setText(tr("Factor mining started..."));
                QJsonObject params;
                params["task_description"] = desc_text;
                params["target_market"] = fm_market->currentText();
                params["max_iterations"] = fm_iter->value();
                params["target_ic"] = fm_ic->value();
                params["config"] = cfg;
                apply_mcp(params);
                AIQuantLabService::instance().rd_agent_start_factor_mining(params);
            });
    fm_vl->addWidget(fm_run);
    add_cost_card(fm_w, fm_vl, tr("Factor mining"),
                  tr("Proposes, codes, backtests and refines alpha factors until the target IC is reached; the "
                     "factors it finds appear in Task Monitor (GET FACTORS)."));
    fm_vl->addStretch();
    sub->addTab(fm_w, tr("Factor Mining"));

    // ── Sub-tab 2: Model Optimization ────────────────────────────────────────
    auto* mo_w = new QWidget(sub);
    auto* mo_vl = new QVBoxLayout(mo_w);
    mo_vl->setContentsMargins(14, 14, 14, 14);
    mo_vl->setSpacing(10);

    auto* mo_desc =
        new QLabel(tr("ML model hyperparameter optimization via ModelRDLoop. Supports LightGBM, XGBoost, "
                      "LSTM, GRU, Transformer and TCN. Optimizes for Sharpe, IC, max drawdown, or win rate."),
                   mo_w);
    mo_desc->setWordWrap(true);
    mo_desc->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                               .arg(ui::colors::TEXT_SECONDARY())
                               .arg(ui::fonts::px(-1))
                               .arg(ui::fonts::DATA_FAMILY));
    mo_vl->addWidget(mo_desc);

    auto* mo_model = new QComboBox(mo_w);
    mo_model->addItems({"lightgbm", "xgboost", "lstm", "gru", "transformer", "tcn"});
    mo_model->setStyleSheet(combo_ss());
    mo_vl->addWidget(build_input_row(tr("Model Type"), mo_model, mo_w));

    auto* mo_target = new QComboBox(mo_w);
    mo_target->addItems({"sharpe", "ic", "max_drawdown", "win_rate", "calmar_ratio"});
    mo_target->setStyleSheet(combo_ss());
    mo_vl->addWidget(build_input_row(tr("Optimize For"), mo_target, mo_w));

    auto* mo_iter = new QSpinBox(mo_w);
    mo_iter->setRange(1, 100);
    mo_iter->setValue(10);
    mo_iter->setStyleSheet(spinbox_ss());
    mo_vl->addWidget(build_input_row(tr("Max Iterations"), mo_iter, mo_w));

    auto* mo_run = make_run_button(tr("START MODEL OPTIMIZATION"), mo_w);
    mo_run->setProperty("fincept_agent_risk", "spend"); // RD-Agent LLM loop
    connect(mo_run, &QPushButton::clicked, this,
            [this, mo_model, mo_target, mo_iter, status_txt, llm_combo, rd_error, apply_mcp]() {
                auto cfg = llm_config_from_combo(llm_combo);
                if (cfg.isEmpty()) {
                    rd_error(tr("Select an LLM profile before running (Settings > LLM Config)."));
                    return;
                }
                status_label_->setText(tr("Starting..."));
                status_txt->setText(tr("Model optimization started..."));
                QJsonObject params;
                params["model_type"] = mo_model->currentText();
                params["optimization_target"] = mo_target->currentText();
                params["max_iterations"] = mo_iter->value();
                params["config"] = cfg;
                apply_mcp(params);
                AIQuantLabService::instance().rd_agent_start_model_optimization(params);
            });
    mo_vl->addWidget(mo_run);
    add_cost_card(mo_w, mo_vl, tr("Model optimisation"),
                  tr("Tunes the model iteratively for the chosen target; the best models appear in Task Monitor "
                     "(GET MODEL)."));
    mo_vl->addStretch();
    sub->addTab(mo_w, tr("Model Optimization"));

    // ── Sub-tab 3: Quant Research ─────────────────────────────────────────────
    auto* qr_w = new QWidget(sub);
    auto* qr_vl = new QVBoxLayout(qr_w);
    qr_vl->setContentsMargins(14, 14, 14, 14);
    qr_vl->setSpacing(10);

    auto* qr_desc = new QLabel(tr("Combined factor discovery + model optimization via QuantRDLoop. Runs the full "
                                  "research pipeline end-to-end: propose factors, code them, backtest, refine."),
                               qr_w);
    qr_desc->setWordWrap(true);
    qr_desc->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                               .arg(ui::colors::TEXT_SECONDARY())
                               .arg(ui::fonts::px(-1))
                               .arg(ui::fonts::DATA_FAMILY));
    qr_vl->addWidget(qr_desc);

    auto* qr_goal = new QTextEdit(qr_w);
    qr_goal->setPlaceholderText(
        tr("Research goal...\ne.g. \"Build a quantitative equity strategy for mid-cap US stocks\""));
    qr_goal->setPlainText(tr("Build a quantitative equity strategy for mid-cap US stocks"));
    qr_goal->setFixedHeight(70);
    qr_goal->setStyleSheet(input_ss());
    qr_vl->addWidget(build_input_row(tr("Research Goal"), qr_goal, qr_w));

    auto* qr_market = new QComboBox(qr_w);
    qr_market->addItems({"US", "CN", "EU", "IN", "JP", "HK", "GLOBAL"});
    qr_market->setStyleSheet(combo_ss());
    qr_vl->addWidget(build_input_row(tr("Target Market"), qr_market, qr_w));

    auto* qr_iter = new QSpinBox(qr_w);
    qr_iter->setRange(1, 100);
    qr_iter->setValue(15);
    qr_iter->setStyleSheet(spinbox_ss());
    qr_vl->addWidget(build_input_row(tr("Max Iterations"), qr_iter, qr_w));

    auto* qr_run = make_run_button(tr("START QUANT RESEARCH"), qr_w);
    qr_run->setProperty("fincept_agent_risk", "spend"); // RD-Agent LLM loop
    connect(qr_run, &QPushButton::clicked, this,
            [this, qr_goal, qr_market, qr_iter, status_txt, llm_combo, rd_error, apply_mcp]() {
                auto goal = qr_goal->toPlainText().trimmed();
                if (goal.isEmpty()) {
                    rd_error(tr("Enter a research goal."));
                    return;
                }
                auto cfg = llm_config_from_combo(llm_combo);
                if (cfg.isEmpty()) {
                    rd_error(tr("Select an LLM profile before running (Settings > LLM Config)."));
                    return;
                }
                status_label_->setText(tr("Starting..."));
                status_txt->setText(tr("Quant research started..."));
                QJsonObject params;
                params["research_goal"] = goal;
                params["target_market"] = qr_market->currentText();
                params["max_iterations"] = qr_iter->value();
                params["config"] = cfg;
                apply_mcp(params);
                AIQuantLabService::instance().rd_agent_start_quant_research(params);
            });
    qr_vl->addWidget(qr_run);
    add_cost_card(qr_w, qr_vl, tr("Quant research"),
                  tr("Runs the whole pipeline end to end: factors, a model, a backtest, refined over the "
                     "iterations; results in Task Monitor."));
    qr_vl->addStretch();
    sub->addTab(qr_w, tr("Quant Research"));

    // ── Sub-tab 4: Task Monitor ───────────────────────────────────────────────
    auto* tm_w = new QWidget(sub);
    auto* tm_vl = new QVBoxLayout(tm_w);
    tm_vl->setContentsMargins(14, 14, 14, 14);
    tm_vl->setSpacing(8);

    // Toolbar: filter + buttons
    auto* tm_toolbar = new QWidget(tm_w);
    auto* tm_hl = new QHBoxLayout(tm_toolbar);
    tm_hl->setContentsMargins(0, 0, 0, 0);
    tm_hl->setSpacing(6);

    auto* filter_combo = new QComboBox(tm_toolbar);
    filter_combo->addItem(tr("All"), QStringLiteral("All"));
    filter_combo->addItem(tr("running"), QStringLiteral("running"));
    filter_combo->addItem(tr("completed"), QStringLiteral("completed"));
    filter_combo->addItem(tr("stopped"), QStringLiteral("stopped"));
    filter_combo->addItem(tr("failed"), QStringLiteral("failed"));
    set_text_width(filter_combo, 120);
    filter_combo->setStyleSheet(combo_ss());
    tm_hl->addWidget(filter_combo);

    auto* refresh_btn = new QPushButton(tr("REFRESH"), tm_toolbar);
    refresh_btn->setCursor(Qt::PointingHandCursor);
    const QString refresh_shape = QStringLiteral("border:none; font-weight:700; padding:5px 12px; border-radius:2px;");
    refresh_btn->setProperty(kAccentShapeProp, refresh_shape);
    refresh_btn->setProperty(kAccentFontProp, -2);
    refresh_btn->setStyleSheet(accent_button_ss(module_.color, refresh_shape, -2));
    tm_hl->addWidget(refresh_btn);

    auto* task_id_input = new QLineEdit(tm_toolbar);
    task_id_input->setPlaceholderText(tr("Task ID..."));
    task_id_input->setStyleSheet(input_ss());
    set_text_width(task_id_input, 200);
    tm_hl->addWidget(task_id_input);

    auto* stop_btn = new QPushButton(tr("STOP"), tm_toolbar);
    stop_btn->setCursor(Qt::PointingHandCursor);
    stop_btn->setStyleSheet(
        QString("QPushButton { background:" + QString(ui::colors::NEGATIVE()) +
                "; color:" + QString(ui::colors::TEXT_ON_ACCENT()) +
                "; border:none;"
                "font-family:%1; font-size:%2px; font-weight:700; padding:5px 12px; border-radius:2px; }"
                "QPushButton:hover { background:" +
                QString(ui::colors::NEGATIVE()) + "; }")
            .arg(ui::fonts::DATA_FAMILY)
            .arg(ui::fonts::px(-2)));
    tm_hl->addWidget(stop_btn);

    auto* resume_btn = new QPushButton(tr("RESUME"), tm_toolbar);
    resume_btn->setProperty("fincept_agent_risk", "spend"); // resumes an RD-Agent LLM loop
    resume_btn->setCursor(Qt::PointingHandCursor);
    resume_btn->setStyleSheet(
        QString("QPushButton { background:" + QString(ui::colors::POSITIVE()) +
                "; color:" + QString(ui::colors::TEXT_ON_ACCENT()) +
                "; border:none;"
                "font-family:%1; font-size:%2px; font-weight:700; padding:5px 12px; border-radius:2px; }"
                "QPushButton:hover { background:" +
                QString(ui::colors::POSITIVE()) + "; }")
            .arg(ui::fonts::DATA_FAMILY)
            .arg(ui::fonts::px(-2)));
    tm_hl->addWidget(resume_btn);

    auto* factors_btn = new QPushButton(tr("GET FACTORS"), tm_toolbar);
    factors_btn->setCursor(Qt::PointingHandCursor);
    factors_btn->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:1px solid %1;"
                "font-family:%2; font-size:%3px; font-weight:700; padding:5px 10px; border-radius:2px; }"
                "QPushButton:hover { background:%1; color:%4; }")
            .arg(ui::colors::legible(module_.color))
            .arg(ui::fonts::DATA_FAMILY)
            .arg(ui::fonts::px(-2))
            .arg(ui::colors::TEXT_ON_ACCENT()));
    tm_hl->addWidget(factors_btn);

    // GET MODEL / STATUS were referenced by the output placeholder text and by
    // on_result() (get_optimized_model / get_task_status branches) but had no
    // button — the service calls were unreachable from the UI.
    auto* model_btn = new QPushButton(tr("GET MODEL"), tm_toolbar);
    model_btn->setCursor(Qt::PointingHandCursor);
    model_btn->setStyleSheet(factors_btn->styleSheet());
    tm_hl->addWidget(model_btn);

    auto* status_btn = new QPushButton(tr("TASK STATUS"), tm_toolbar);
    status_btn->setCursor(Qt::PointingHandCursor);
    status_btn->setStyleSheet(factors_btn->styleSheet());
    tm_hl->addWidget(status_btn);

    tm_hl->addStretch();
    tm_vl->addWidget(tm_toolbar);

    // No task in this session yet: what the monitor shows, and how to start one.
    auto* tm_empty = ui::EmptyState::create(
        ui::EmptyState::no_data_spec(tr("No RD-Agent tasks in this session"),
                                     tr("Tasks you start in Factor Mining, Model Optimization or Quant Research are "
                                        "listed here with their progress, best IC and results."),
                                     tr("START FACTOR MINING")),
        tm_w);
    tm_empty->setObjectName(QStringLiteral("rdTasksEmpty"));
    tm_empty->set_compact(true);
    tm_empty->set_alignment(Qt::AlignTop);
    connect(tm_empty, &ui::EmptyState::primary_clicked, this, [sub]() { sub->setCurrentIndex(0); });
    tm_vl->addWidget(tm_empty);

    // Task table
    rd_task_table_ = new QTableWidget(0, 6, tm_w);
    rd_task_table_->setHorizontalHeaderLabels(
        {tr("Task ID"), tr("Type"), tr("Status"), tr("Progress"), tr("Best IC"), tr("Elapsed")});
    rd_task_table_->horizontalHeader()->setStretchLastSection(true);
    rd_task_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    rd_task_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    rd_task_table_->setAlternatingRowColors(true);
    rd_task_table_->setStyleSheet(table_ss());
    rd_task_table_->hide(); // shown with the first task (on_result list_tasks)
    tm_vl->addWidget(rd_task_table_, 1);

    // Output area for factor results
    rd_agent_output_ = new QTextEdit(tm_w);
    rd_agent_output_->setReadOnly(true);
    rd_agent_output_->setPlaceholderText(tr("Select a task and click GET FACTORS / GET MODEL to view results..."));
    rd_agent_output_->hide(); // shown with the first task (on_result list_tasks)
    rd_agent_output_->setFixedHeight(160);
    rd_agent_output_->setStyleSheet(output_ss());
    tm_vl->addWidget(rd_agent_output_);

    // Structured results of TASK STATUS / GET FACTORS / GET MODEL (KPI strip + sortable
    // table), filled by on_result(); hidden until the first such result.
    auto* rd_results = new QWidget(tm_w);
    rd_results->setObjectName(QStringLiteral("rdResultsHost"));
    auto* rd_results_vl = new QVBoxLayout(rd_results);
    rd_results_vl->setContentsMargins(0, 0, 0, 0);
    rd_results_vl->setSpacing(6);
    rd_results->hide();
    tm_vl->addWidget(rd_results);

    // Auto-refresh while a task is running. list_tasks is served from the
    // service's in-memory registry (no Python process), so a 5 s cadence is
    // cheap. The timer only runs while the Task Monitor is on screen (panel
    // show/hide + the visibility check below) and while the last listing had a
    // running task (on_result sets "rdActive"); otherwise progress was only
    // visible after a manual REFRESH.
    auto* rd_poll = new QTimer(this);
    rd_poll->setObjectName(QStringLiteral("rdTaskPollTimer"));
    rd_poll->setInterval(5000);
    connect(rd_poll, &QTimer::timeout, this, [tm_w, filter_combo, rd_poll]() {
        if (!tm_w->isVisible()) {
            rd_poll->stop();
            return;
        }
        const auto filter = filter_combo->currentData().toString();
        AIQuantLabService::instance().rd_agent_list_tasks(filter == "All" ? QString{} : filter);
    });
    connect(sub, &QTabWidget::currentChanged, this, [sub, tm_w, filter_combo, rd_poll](int idx) {
        if (sub->widget(idx) == tm_w) {
            // Entering the monitor: list now (cheap) — on_result decides whether to keep polling.
            const auto filter = filter_combo->currentData().toString();
            AIQuantLabService::instance().rd_agent_list_tasks(filter == "All" ? QString{} : filter);
        } else {
            rd_poll->stop();
        }
    });

    // Wire toolbar buttons
    connect(refresh_btn, &QPushButton::clicked, this, [filter_combo, status_txt]() {
        const auto filter = filter_combo->currentData().toString();
        status_txt->setText(tr("Refreshing task list..."));
        AIQuantLabService::instance().rd_agent_list_tasks(filter == "All" ? QString{} : filter);
    });

    // Resolve the target task: the text field wins, otherwise the selected row.
    // Errors go to rd_agent_output_ (visible in this tab) — display_error()
    // paints into the Deep Analysis tab's results pane, which is hidden here.
    auto resolve_task_id = [this, task_id_input, status_txt]() -> QString {
        QString tid = task_id_input->text().trimmed();
        if (tid.isEmpty() && rd_task_table_->currentRow() >= 0 && rd_task_table_->item(rd_task_table_->currentRow(), 0))
            tid = rd_task_table_->item(rd_task_table_->currentRow(), 0)->text();
        if (tid.isEmpty()) {
            status_txt->setText(tr("Select a row or type a task ID first."));
            if (rd_agent_output_)
                rd_agent_output_->setPlainText(tr("Select a task row (or paste a task ID) before using this action."));
        }
        return tid;
    };

    connect(stop_btn, &QPushButton::clicked, this, [resolve_task_id, status_txt]() {
        const QString tid = resolve_task_id();
        if (tid.isEmpty())
            return;
        status_txt->setText(tr("Stopping %1...").arg(tid));
        AIQuantLabService::instance().rd_agent_stop_task(tid);
    });

    connect(resume_btn, &QPushButton::clicked, this, [this, resolve_task_id, status_txt, llm_combo, rd_error]() {
        const QString tid = resolve_task_id();
        if (tid.isEmpty())
            return;
        // cmd_resume_task() applies config["llm_*"] before rebuilding the loop; without the
        // profile the resumed task had no LLM credentials and failed on its first call.
        const QJsonObject cfg = llm_config_from_combo(llm_combo);
        if (cfg.isEmpty()) {
            rd_error(tr("Select an LLM profile before resuming (Settings > LLM Config)."));
            return;
        }
        status_txt->setText(tr("Resuming %1...").arg(tid));
        AIQuantLabService::instance().rd_agent_resume_task(tid, cfg);
    });

    connect(factors_btn, &QPushButton::clicked, this, [resolve_task_id, status_txt]() {
        const QString tid = resolve_task_id();
        if (tid.isEmpty())
            return;
        status_txt->setText(tr("Loading factors for %1...").arg(tid));
        AIQuantLabService::instance().rd_agent_get_discovered_factors(tid);
    });

    connect(model_btn, &QPushButton::clicked, this, [resolve_task_id, status_txt]() {
        const QString tid = resolve_task_id();
        if (tid.isEmpty())
            return;
        status_txt->setText(tr("Loading optimized model for %1...").arg(tid));
        AIQuantLabService::instance().rd_agent_get_optimized_model(tid);
    });

    connect(status_btn, &QPushButton::clicked, this, [resolve_task_id, status_txt]() {
        const QString tid = resolve_task_id();
        if (tid.isEmpty())
            return;
        status_txt->setText(tr("Querying %1...").arg(tid));
        AIQuantLabService::instance().rd_agent_get_task_status(tid);
    });

    // Populate task_id_input on row click
    connect(rd_task_table_, &QTableWidget::currentCellChanged, this, [task_id_input, this](int row, int, int, int) {
        if (row >= 0 && rd_task_table_->item(row, 0))
            task_id_input->setText(rd_task_table_->item(row, 0)->text());
    });

    sub->addTab(tm_w, tr("Task Monitor"));

    vl->addWidget(sub, 1);
    return w;
}

} // namespace fincept::screens
