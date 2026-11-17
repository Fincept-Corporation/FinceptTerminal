// ComposerContextBar.cpp — the strip above Chat Mode's composer (WP2).
//
//   [+ CONTEXT] [AAPL ×] [Portfolio: Growth ×] [Attached: notes.txt ×]   [ENGINE ▾] [RUN AS TASK] budget…
//
// Context chips go with every message until removed; the attached text goes
// with the next message only (it is cleared once a run started with it). The
// symbol is typed into CcbSymbolEntry — one small private widget, so the shared
// SymbolSearchBox can replace its insides without touching the bar. Portfolios
// load on a worker thread (PortfolioRepository), investment profiles through
// ChatModeService, files are read on a worker thread: nothing blocks the GUI.
// The engine picker offers the multi-step and local engines only when their
// executor is available in this build (run_view::executor_available), and the
// harness engine (Claude Code / Codex on the user's own API key) when a harness
// may run Chat Mode tasks -- else listed disabled with the router's reason. With
// the harness chosen, the shared ModelBadge shows (and changes) which harness and
// model run it, with its billing note.

#include "screens/chat_mode/ComposerContextBar.h"

#include "screens/chat_mode/ChatModeService.h"
#include "screens/chat_mode/RunViewLogic.h"
#include "screens/chat_mode/RunViewWidgets.h"
#include "screens/portfolio/PortfolioTypes.h"
#include "services/agent_tasks/RunPlanPrompts.h"
#include "services/agent_tasks/TaskRunService.h"
#include "storage/repositories/PortfolioRepository.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/ModelBadge.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMimeData>
#include <QPointer>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QThreadPool>
#include <QTimer>
#include <QToolButton>
#include <QUuid>

namespace fincept::chat_mode {

namespace agt = fincept::agent_tasks;
namespace rv = fincept::chat_mode::run_view;

namespace composer_context_bar_detail {

/// What the picker offers: one cloud reply, a planned multi-step cloud task, a
/// local agentic task, a task run by the user's agent harness.
enum class CcbChoice { CloudTurn, CloudPlan, Local, Harness };

/// The engines that are always a task (no "one reply" form, RUN AS TASK stays on).
bool ccb_always_task(CcbChoice c) {
    return c == CcbChoice::Local || c == CcbChoice::Harness;
}

QString ccb_executor_of(CcbChoice c) {
    switch (c) {
        case CcbChoice::CloudPlan:
            return QString::fromLatin1(agt::kExecutorCloudPlan);
        case CcbChoice::Local:
            return QString::fromLatin1(agt::kExecutorLocal);
        case CcbChoice::Harness:
            return QString::fromLatin1(rv::kHarnessEngineId);
        case CcbChoice::CloudTurn:
            break;
    }
    return QString::fromLatin1(agt::kExecutorCloudTurn);
}

QString ccb_choice_text(CcbChoice c) {
    switch (c) {
        case CcbChoice::CloudPlan:
            return ComposerContextBar::tr("Cloud · multi-step task");
        case CcbChoice::Local:
            return ComposerContextBar::tr("Local agent");
        case CcbChoice::Harness:
            return ComposerContextBar::tr("Harness");
        case CcbChoice::CloudTurn:
            break;
    }
    return ComposerContextBar::tr("Cloud · one reply");
}

QString ccb_choice_help(CcbChoice c) {
    switch (c) {
        case CcbChoice::CloudPlan:
            return ComposerContextBar::tr("The cloud agent plans the task, works through it one step (one agent turn) "
                                          "at a time and writes a final synthesis. Stops at its budget.");
        case CcbChoice::Local:
            return ComposerContextBar::tr("The terminal's own agentic engine with your LLM profile: no Fincept "
                                          "credits, steps without tool details.");
        case CcbChoice::Harness:
            return ComposerContextBar::tr("Your agent harness (Claude Code or Codex) works through the task in its own "
                                          "loop with the terminal's tools, read-only and paper only. No Fincept "
                                          "credits: it is billed to your own API key. It cannot pause; cancel stops it.");
        case CcbChoice::CloudTurn:
            break;
    }
    return ComposerContextBar::tr("One reply of the Fincept cloud agent (one agent turn, about 5 credits).");
}

/// A ticker as typed: trimmed, upper case; empty when it cannot be a symbol.
QString ccb_normalize_symbol(const QString& raw) {
    static const QRegularExpression kSymbol(QStringLiteral("^[A-Z0-9][A-Z0-9.\\-^=:/_]{0,23}$"));
    const QString s = raw.trimmed().toUpper();
    return kSymbol.match(s).hasMatch() ? s : QString();
}

struct CcbFileRead {
    bool ok = false;
    QString text;
    QString error;
    qint64 total_chars = 0;
    bool truncated = false;
};

/// Worker thread: up to kMaxAttachmentChars characters of a UTF-8 text file.
CcbFileRead ccb_read_text_file(const QString& path) {
    CcbFileRead out;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        out.error = ComposerContextBar::tr("The file could not be opened: %1").arg(f.errorString());
        return out;
    }
    // UTF-8 needs at most 4 bytes per character; read a little more to know it was cut.
    const qint64 budget = qint64(ComposerContextBar::kMaxAttachmentChars) * 4 + 4;
    const QByteArray raw = f.read(budget);
    const qint64 size = f.size();
    if (raw.left(8192).contains('\0')) {
        out.error = ComposerContextBar::tr("This is not a text file. Attach plain text (.txt, .md, .csv, .json …).");
        return out;
    }
    QString text = QString::fromUtf8(raw);
    out.total_chars = size > raw.size() ? size : text.size(); // an estimate for a file read only in part
    if (text.size() > ComposerContextBar::kMaxAttachmentChars || size > raw.size()) {
        int cap = ComposerContextBar::kMaxAttachmentChars;
        if (cap < text.size() && text.at(cap - 1).isHighSurrogate())
            --cap;
        text = text.left(cap);
        out.truncated = true;
    }
    out.text = text;
    out.ok = true;
    return out;
}

QString ccb_style() {
    return QStringLiteral(
               "#composerContextBar{background:transparent;border:none;}"
               "#composerContextBar QLabel{background:transparent;border:none;color:%3;font-size:%6px;}"
               "#composerContextBar QLabel#ccbBudget[warn=\"true\"]{color:%10;}"
               "#composerContextBar QToolButton{background:%4;color:%3;border:1px solid %5;padding:2px 8px;"
               "font-size:%6px;font-weight:700;}"
               "#composerContextBar QToolButton:hover{color:%2;border-color:%7;}"
               "#composerContextBar QToolButton:focus{border-color:%8;}"
               "#composerContextBar QToolButton:disabled{color:%11;border-color:%5;background:transparent;}"
               "#composerContextBar QToolButton::menu-indicator{image:none;width:0;}"
               "#composerContextBar QToolButton#ccbTask:checked{background:%8;color:%9;border-color:%8;}"
               "#composerContextBar QFrame#ccbChip{background:%12;border:1px solid %13;}"
               "#composerContextBar QFrame#ccbChip QLabel{color:%2;}"
               "#composerContextBar QFrame#ccbChip QToolButton{background:transparent;border:none;padding:0 3px;"
               "color:%3;font-weight:400;}"
               "#composerContextBar QFrame#ccbChip QToolButton:hover{color:%14;}"
               "#composerContextBar QFrame#ccbChip QToolButton:focus{color:%14;border:1px solid %8;}"
               "#composerContextBar QLineEdit{background:%4;color:%2;border:1px solid %5;padding:2px 6px;"
               "font-size:%15px;}"
               "#composerContextBar QLineEdit:focus{border-color:%7;}"
               "#composerContextBar QLineEdit[invalid=\"true\"]{border-color:%14;}"
               "#composerContextBar QMenu{background:%1;color:%2;border:1px solid %13;padding:4px 0;}"
               "#composerContextBar QMenu::item{padding:4px 18px 4px 12px;}"
               "#composerContextBar QMenu::item:selected{background:%4;}"
               "#composerContextBar QMenu::item:disabled{color:%11;}"
               "#composerContextBar QMenu::separator{background:%5;height:1px;margin:4px 8px;}"
               "#composerContextBar QToolButton#ccbBudget[warn=\"true\"]{color:%10;border-color:%10;}"
               "#composerContextBar QToolButton#ccbBudget:checked{background:%4;color:%2;border-color:%8;}"
               "#composerContextBar QFrame#ccbBudgetEditor{background:%4;border:1px solid %13;}"
               "#composerContextBar QSpinBox,#composerContextBar QDoubleSpinBox{background:%1;color:%2;"
               "border:1px solid %5;padding:1px 4px;font-size:%15px;}"
               "#composerContextBar QSpinBox:focus,#composerContextBar QDoubleSpinBox:focus{border-color:%8;}"
               "#composerContextBar QFrame#ccbConfirm{background:%16;border:1px solid %10;}"
               "#composerContextBar QFrame#ccbConfirm QLabel{color:%2;font-size:%15px;}"
               "#composerContextBar QToolButton#ccbConfirmYes{background:%8;color:%9;border-color:%8;}")
        .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_SECONDARY(), ui::colors::BG_RAISED(),
             ui::colors::BORDER_DIM(), ui::fonts::px(-4), ui::colors::BORDER_BRIGHT(), ui::colors::AMBER(),
             ui::colors::TEXT_ON_ACCENT())
        .arg(ui::colors::WARNING(), ui::colors::TEXT_DIM(), ui::colors::tint(QColor(ui::colors::INFO()), 0.12),
             ui::colors::BORDER_MED(), ui::colors::DANGER(), ui::fonts::px(-3),
             ui::colors::tint(QColor(ui::colors::WARNING()), 0.12));
}

constexpr const char* kCcbBudgetKey = "chat_mode/task_budget"; ///< "credits,steps,minutes" of the last task budget

// ── Symbol entry ─────────────────────────────────────────────────────────────

/// The symbol entry of "+ CONTEXT": the terminal's shared picker
/// (ui::SymbolSearchBox — ticker, name or ISIN, resolved through
/// InstrumentResolver; never raw typed text). A pick becomes a chip with the
/// instrument's market-data symbol; Esc with the list closed cancels.
class CcbSymbolEntry : public QWidget {
  public:
    explicit CcbSymbolEntry(QWidget* parent) : QWidget(parent) {
        auto* hl = new QHBoxLayout(this);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(0);
        box_ = new ui::SymbolSearchBox(this);
        box_->set_compact(true);
        box_->set_clear_on_pick(true);
        box_->set_recent_key(QStringLiteral("chat_mode"));
        hl->addWidget(box_);
        QObject::connect(box_, &ui::SymbolSearchBox::instrument_picked, this,
                         [this](const core::instrument::Instrument& inst) { commit(inst); });
        QObject::connect(box_, &ui::SymbolSearchBox::unresolved, this,
                         [this](const QStringList&, const QString& message) { box_->setToolTip(message); });
        new rv::RvKeyHook(box_->line_edit(), [this](const QKeyEvent* ke, bool apply) {
            if (ke->key() != Qt::Key_Escape || box_->is_popup_visible())
                return false; // the picker's own keys (Esc first closes its list)
            if (apply)
                cancel();
            return true;
        });
        QObject::connect(box_->line_edit(), &QLineEdit::editingFinished, this, [this]() {
            // Focus moved elsewhere from an empty entry: close it, leave the focus where the user put it.
            if (isVisible() && !box_->line_edit()->hasFocus() && !box_->is_popup_visible() &&
                box_->text().trimmed().isEmpty())
                setVisible(false);
        });
        retranslate();
        setVisible(false);
    }

    void set_on_commit(std::function<void(const QString&)> fn) { on_commit_ = std::move(fn); }
    void set_on_cancel(std::function<void()> fn) { on_cancel_ = std::move(fn); }

    void begin() {
        box_->set_text(QString());
        setVisible(true);
        box_->focus_and_select();
    }

    void retranslate() {
        box_->set_placeholder(ComposerContextBar::tr("Symbol: ticker, name or ISIN"));
        box_->setAccessibleName(ComposerContextBar::tr("Symbol to add as context"));
        box_->setToolTip(ComposerContextBar::tr("Pick a symbol from the list (Enter); Esc closes"));
        refit();
    }

    /// Width for ~22 characters at the current font (re-fitted when the font changes).
    void refit() { box_->setMinimumWidth(QFontMetrics(ui::fonts::font(-3)).horizontalAdvance(QString(22, QLatin1Char('X')))); }

  private:
    void commit(const core::instrument::Instrument& inst) {
        const QString s = inst.symbol.isEmpty() ? inst.display : inst.symbol;
        if (s.isEmpty())
            return;
        setVisible(false);
        if (on_commit_)
            on_commit_(s);
    }

    void cancel() {
        setVisible(false);
        if (on_cancel_)
            on_cancel_();
    }

    ui::SymbolSearchBox* box_ = nullptr;
    std::function<void(const QString&)> on_commit_;
    std::function<void()> on_cancel_;
};

} // namespace composer_context_bar_detail

using namespace composer_context_bar_detail;

// ── Impl ─────────────────────────────────────────────────────────────────────

struct ComposerContextBar::Impl {
    ComposerContextBar* q = nullptr;

    rv::ComposerContextData data;
    QString attachment_id;          ///< matches the run that sent it (then it is cleared)
    bool attachment_truncated = false;
    qint64 attachment_total = 0;
    CcbChoice choice = CcbChoice::CloudTurn;

    QToolButton* add_btn = nullptr;
    QMenu* add_menu = nullptr;
    QMenu* portfolio_menu = nullptr;
    QMenu* profile_menu = nullptr;
    QWidget* chips = nullptr;
    rv::RvFlowLayout* chips_flow = nullptr;
    CcbSymbolEntry* symbol_entry = nullptr;
    QToolButton* engine_btn = nullptr;
    QMenu* engine_menu = nullptr;
    QActionGroup* engine_group = nullptr;
    ui::ModelBadge* harness_badge = nullptr; ///< which harness / model runs a harness task (shown with it)
    QLabel* harness_note = nullptr;          ///< "0 credits · billed to your Anthropic API key"
    rv::HarnessEngineOption harness;         ///< the harness choice as last asked (routes_changed refreshes it)
    QToolButton* task_btn = nullptr;
    QToolButton* budget = nullptr; ///< "BUDGET · up to 8 steps · est. 50 cr ▾": opens the editor
    QFrame* budget_editor = nullptr;
    QDoubleSpinBox* max_credits = nullptr;
    QSpinBox* max_steps = nullptr;
    QSpinBox* max_minutes = nullptr;
    QToolButton* budget_reset = nullptr;
    QFrame* confirm = nullptr;
    QLabel* confirm_text = nullptr;
    QToolButton* confirm_yes = nullptr;
    QToolButton* confirm_change = nullptr;
    QToolButton* confirm_no = nullptr;
    QLabel* note = nullptr;
    agt::RunBudget task_budget{agt::kDefaultMaxCredits, agt::kDefaultMaxSteps, 0};

    QVector<QPair<QString, QString>> portfolios; ///< id, name
    bool portfolios_loaded = false;
    bool portfolios_loading = false;
    QString portfolios_error;
    QVector<AgentProfile> profiles;
    bool profiles_loaded = false;
    bool profiles_loading = false;
    QString profiles_error;
    bool file_reading = false;

    void build();
    void wire();
    void retranslate();
    void rebuild_chips();
    void add_chip(const QString& text, const QString& tip, std::function<void()> on_remove);
    void changed();
    void update_engine();
    void set_choice(CcbChoice c, bool user);
    void fill_add_menu();
    void fill_portfolio_menu();
    void fill_profile_menu();
    void load_portfolios();
    void load_profiles();
    void attach_file();
    void read_file(const QString& path);
    void attach_text(const QString& name, const QString& text, qint64 total_chars, bool truncated);
    void show_note(const QString& text);
    void load_budget();
    void store_budget();
    void budget_to_editor();
};

void ComposerContextBar::Impl::build() {
    q->setObjectName(QStringLiteral("composerContextBar"));
    q->setStyleSheet(ccb_style());
    auto* flow = new rv::RvFlowLayout(q, 6, 4);

    add_btn = new QToolButton(q);
    add_btn->setPopupMode(QToolButton::InstantPopup);
    add_btn->setCursor(Qt::PointingHandCursor);
    add_menu = new QMenu(add_btn);
    add_menu->setToolTipsVisible(true);
    portfolio_menu = new QMenu(add_menu);
    portfolio_menu->setToolTipsVisible(true);
    profile_menu = new QMenu(add_menu);
    profile_menu->setToolTipsVisible(true);
    add_btn->setMenu(add_menu);
    flow->addWidget(add_btn);

    chips = new QWidget(q);
    chips_flow = new rv::RvFlowLayout(chips, 4, 4);
    flow->addWidget(chips);

    symbol_entry = new CcbSymbolEntry(q);
    flow->addWidget(symbol_entry);

    engine_btn = new QToolButton(q);
    engine_btn->setPopupMode(QToolButton::InstantPopup);
    engine_btn->setCursor(Qt::PointingHandCursor);
    engine_menu = new QMenu(engine_btn);
    engine_menu->setToolTipsVisible(true);
    engine_btn->setMenu(engine_menu);
    engine_group = new QActionGroup(engine_menu);
    engine_group->setExclusive(true);
    flow->addWidget(engine_btn);

    harness_badge = new ui::ModelBadge(QString::fromLatin1(ai_chat::kFeatureChatModeHarness), q);
    harness_badge->set_compact(true);
    harness_badge->setVisible(false);
    flow->addWidget(harness_badge);
    harness_note = new QLabel(q);
    harness_note->setTextFormat(Qt::PlainText);
    harness_note->setVisible(false);
    flow->addWidget(harness_note);

    task_btn = new QToolButton(q);
    task_btn->setObjectName(QStringLiteral("ccbTask"));
    task_btn->setCheckable(true);
    task_btn->setCursor(Qt::PointingHandCursor);
    flow->addWidget(task_btn);

    budget = new QToolButton(q);
    budget->setObjectName(QStringLiteral("ccbBudget"));
    budget->setCheckable(true);
    budget->setCursor(Qt::PointingHandCursor);
    budget->setVisible(false);
    flow->addWidget(budget);

    // The task budget, edited inline (no dialog): credits, steps, minutes.
    budget_editor = new QFrame(q);
    budget_editor->setObjectName(QStringLiteral("ccbBudgetEditor"));
    auto* ef = new rv::RvFlowLayout(budget_editor, 6, 4);
    ef->setContentsMargins(6, 3, 6, 3);
    // One labelled field; the input is created under its final parent.
    const auto field = [this, ef](const QString& label_name, auto make) {
        auto* box = new QWidget(budget_editor);
        auto* hl = new QHBoxLayout(box);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(4);
        auto* l = new QLabel(box);
        l->setObjectName(label_name);
        hl->addWidget(l);
        auto* input = make(box);
        hl->addWidget(input);
        ef->addWidget(box);
        return input;
    };
    max_credits = field(QStringLiteral("ccbCreditsLabel"), [](QWidget* p) {
        auto* s = new QDoubleSpinBox(p);
        s->setRange(0, 1000);
        s->setDecimals(0);
        s->setSingleStep(5);
        return s;
    });
    max_steps = field(QStringLiteral("ccbStepsLabel"), [](QWidget* p) {
        auto* s = new QSpinBox(p);
        s->setRange(0, agt::run_plan_prompts::kPlanHardMaxSteps);
        return s;
    });
    max_minutes = field(QStringLiteral("ccbMinutesLabel"), [](QWidget* p) {
        auto* s = new QSpinBox(p);
        s->setRange(0, 24 * 60);
        s->setSingleStep(5);
        return s;
    });
    budget_reset = new QToolButton(budget_editor);
    budget_reset->setCursor(Qt::PointingHandCursor);
    ef->addWidget(budget_reset);
    budget_editor->setVisible(false);
    flow->addWidget(budget_editor);

    // "This run may cost up to N credits — start?" before a costly task goes out.
    confirm = new QFrame(q);
    confirm->setObjectName(QStringLiteral("ccbConfirm"));
    auto* cf = new rv::RvFlowLayout(confirm, 6, 4);
    cf->setContentsMargins(6, 3, 6, 3);
    confirm_text = new QLabel(confirm);
    confirm_text->setWordWrap(true);
    confirm_yes = new QToolButton(confirm);
    confirm_yes->setObjectName(QStringLiteral("ccbConfirmYes"));
    confirm_change = new QToolButton(confirm);
    confirm_no = new QToolButton(confirm);
    for (QToolButton* b : {confirm_yes, confirm_change, confirm_no})
        b->setCursor(Qt::PointingHandCursor);
    cf->addWidget(confirm_text);
    cf->addWidget(confirm_yes);
    cf->addWidget(confirm_change);
    cf->addWidget(confirm_no);
    confirm->setVisible(false);
    flow->addWidget(confirm);

    note = new QLabel(q);
    note->setVisible(false);
    flow->addWidget(note);
}

void ComposerContextBar::Impl::load_budget() {
    // The last budget the user set (QSettings: per user, like the inspector tab).
    const QStringList parts = QSettings().value(QLatin1String(kCcbBudgetKey)).toString().split(QLatin1Char(','));
    if (parts.size() == 3) {
        bool a = false, b = false, c = false;
        const double credits = parts.at(0).toDouble(&a);
        const int steps = parts.at(1).toInt(&b);
        const int minutes = parts.at(2).toInt(&c);
        if (a && b && c && credits >= 0 && steps >= 0 && minutes >= 0) {
            task_budget.max_credits = credits;
            task_budget.max_steps = std::min(steps, agt::run_plan_prompts::kPlanHardMaxSteps);
            task_budget.max_minutes = minutes;
        }
    }
    budget_to_editor();
}

void ComposerContextBar::Impl::store_budget() {
    QSettings().setValue(QLatin1String(kCcbBudgetKey), QStringLiteral("%1,%2,%3")
                                                           .arg(task_budget.max_credits, 0, 'f', 0)
                                                           .arg(task_budget.max_steps)
                                                           .arg(task_budget.max_minutes));
}

void ComposerContextBar::Impl::budget_to_editor() {
    const QSignalBlocker b1(max_credits);
    const QSignalBlocker b2(max_steps);
    const QSignalBlocker b3(max_minutes);
    max_credits->setValue(task_budget.max_credits);
    max_steps->setValue(task_budget.max_steps);
    max_minutes->setValue(task_budget.max_minutes);
}

void ComposerContextBar::Impl::wire() {
    QObject::connect(add_menu, &QMenu::aboutToShow, q, [this]() { fill_add_menu(); });
    QObject::connect(portfolio_menu, &QMenu::aboutToShow, q, [this]() {
        if (!portfolios_loaded && !portfolios_loading)
            load_portfolios();
        fill_portfolio_menu();
    });
    QObject::connect(profile_menu, &QMenu::aboutToShow, q, [this]() {
        if (!profiles_loaded && !profiles_loading)
            load_profiles();
        fill_profile_menu();
    });
    QObject::connect(engine_menu, &QMenu::aboutToShow, q, [this]() { update_engine(); });
    // A harness became (un)usable, or another harness / model was picked.
    QObject::connect(&ai_chat::ModelRouter::instance(), &ai_chat::ModelRouter::routes_changed, q,
                     [this]() { update_engine(); });
    QObject::connect(task_btn, &QToolButton::clicked, q, [this](bool on) {
        if (ccb_always_task(choice)) {
            task_btn->setChecked(true); // a local or harness run is always a task
            return;
        }
        set_choice(on ? CcbChoice::CloudPlan : CcbChoice::CloudTurn, true);
    });
    symbol_entry->set_on_commit([this](const QString& symbol) {
        if (!data.symbols.contains(symbol)) {
            data.symbols << symbol;
            changed();
        }
        add_btn->setFocus();
    });
    symbol_entry->set_on_cancel([this]() { add_btn->setFocus(); });

    // The attached text goes with the next message only: once a run started with
    // it (this window's send carries its id in the run context), it is dropped.
    QObject::connect(&agt::TaskRunService::instance(), &agt::TaskRunService::run_added, q, [this](const QString& id) {
        if (attachment_id.isEmpty())
            return;
        const agt::TaskRun* r = agt::TaskRunService::instance().find_run(id);
        if (!r || r->context.value(QLatin1String(agt::kCtxAnswerToPause)).toBool())
            return;
        if (r->context.value(QStringLiteral("attachment")).toObject().value(QStringLiteral("id")).toString() !=
            attachment_id)
            return;
        data.attachment_name.clear();
        data.attachment_text.clear();
        attachment_id.clear();
        attachment_truncated = false;
        attachment_total = 0;
        changed();
        show_note(ComposerContextBar::tr("The attached text was sent with your message."));
    });
    // The bar's stylesheet follows theme and font through the theme manager's
    // re-style pass; sizes from font metrics are re-fitted once it has settled.
    QObject::connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, q,
                     [this]() { symbol_entry->refit(); });

    // ── Task budget editor and the confirmation step ──
    QObject::connect(budget, &QToolButton::toggled, q, [this](bool on) {
        budget_editor->setVisible(on);
        if (on)
            max_credits->setFocus();
    });
    const auto edited = [this]() {
        task_budget.max_credits = max_credits->value();
        task_budget.max_steps = max_steps->value();
        task_budget.max_minutes = max_minutes->value();
        store_budget();
        confirm->setVisible(false); // the question was about the old budget
        update_engine();
    };
    QObject::connect(max_credits, &QDoubleSpinBox::valueChanged, q, edited);
    QObject::connect(max_steps, &QSpinBox::valueChanged, q, edited);
    QObject::connect(max_minutes, &QSpinBox::valueChanged, q, edited);
    QObject::connect(budget_reset, &QToolButton::clicked, q, [this]() {
        task_budget = agt::RunBudget{agt::kDefaultMaxCredits, agt::kDefaultMaxSteps, 0};
        budget_to_editor();
        store_budget();
        update_engine();
    });
    QObject::connect(confirm_yes, &QToolButton::clicked, q, [this]() {
        confirm->setVisible(false);
        emit q->confirmed();
    });
    QObject::connect(confirm_change, &QToolButton::clicked, q, [this]() {
        confirm->setVisible(false);
        budget->setChecked(true);
    });
    QObject::connect(confirm_no, &QToolButton::clicked, q, [this]() { confirm->setVisible(false); });
    for (QToolButton* b : {confirm_yes, confirm_change, confirm_no})
        new rv::RvKeyHook(b, [this](const QKeyEvent* ke, bool apply) {
            if (ke->key() != Qt::Key_Escape)
                return false;
            if (apply)
                confirm->setVisible(false);
            return true;
        });
    new rv::RvEventHook(q, [this](QEvent::Type t) {
        if (t == QEvent::LanguageChange)
            retranslate();
        else if (t == QEvent::Show)
            update_engine(); // executors register after the bar is built
    });
}

void ComposerContextBar::Impl::retranslate() {
    add_btn->setText(ComposerContextBar::tr("+ CONTEXT"));
    add_btn->setToolTip(ComposerContextBar::tr("Add a symbol, portfolio, investment profile or a text file to your "
                                               "messages. Chips go with every message until removed; an attached "
                                               "text goes with the next message only."));
    add_btn->setAccessibleName(ComposerContextBar::tr("Add context to the message"));
    portfolio_menu->setTitle(ComposerContextBar::tr("Portfolio"));
    profile_menu->setTitle(ComposerContextBar::tr("Investment profile"));
    engine_btn->setAccessibleName(ComposerContextBar::tr("Engine for the next message"));
    task_btn->setText(ComposerContextBar::tr("RUN AS TASK"));
    task_btn->setAccessibleName(ComposerContextBar::tr("Run the next message as a multi-step task"));
    if (auto* l = budget_editor->findChild<QLabel*>(QStringLiteral("ccbCreditsLabel")))
        l->setText(ComposerContextBar::tr("Max credits"));
    if (auto* l = budget_editor->findChild<QLabel*>(QStringLiteral("ccbStepsLabel")))
        l->setText(ComposerContextBar::tr("Max steps"));
    if (auto* l = budget_editor->findChild<QLabel*>(QStringLiteral("ccbMinutesLabel")))
        l->setText(ComposerContextBar::tr("Max minutes"));
    max_credits->setSpecialValueText(ComposerContextBar::tr("no limit"));
    max_credits->setToolTip(ComposerContextBar::tr("The run stops before a turn that would go over this many credits"));
    max_steps->setSpecialValueText(ComposerContextBar::tr("auto"));
    max_steps->setToolTip(ComposerContextBar::tr("The most steps the plan may have (auto: up to %1)")
                              .arg(agt::run_plan_prompts::kPlanHardMaxSteps));
    max_minutes->setSpecialValueText(ComposerContextBar::tr("no limit"));
    max_minutes->setToolTip(ComposerContextBar::tr("Working time; pauses and questions to you do not count"));
    budget_reset->setText(ComposerContextBar::tr("DEFAULT"));
    budget_reset->setToolTip(ComposerContextBar::tr("%1 credits, %2 steps, no time limit")
                                 .arg(rv::format_credits(agt::kDefaultMaxCredits))
                                 .arg(agt::kDefaultMaxSteps));
    confirm_yes->setText(ComposerContextBar::tr("START"));
    confirm_change->setText(ComposerContextBar::tr("CHANGE BUDGET"));
    confirm_no->setText(ComposerContextBar::tr("CANCEL"));
    symbol_entry->retranslate();
    rebuild_chips();
    update_engine();
}

void ComposerContextBar::Impl::show_note(const QString& text) {
    note->setText(text);
    note->setVisible(!text.isEmpty());
    QPointer<QLabel> n(note);
    QTimer::singleShot(6000, note, [n, text]() {
        if (n && n->text() == text)
            n->setVisible(false);
    });
}

void ComposerContextBar::Impl::changed() {
    rebuild_chips();
    emit q->context_changed();
}

void ComposerContextBar::Impl::add_chip(const QString& text, const QString& tip, std::function<void()> on_remove) {
    auto* chip = new QFrame(chips);
    chip->setObjectName(QStringLiteral("ccbChip"));
    auto* hl = new QHBoxLayout(chip);
    hl->setContentsMargins(6, 1, 2, 1);
    hl->setSpacing(2);
    auto* label = new QLabel(text, chip);
    label->setTextFormat(Qt::PlainText);
    auto* remove = new QToolButton(chip);
    remove->setText(QStringLiteral("×"));
    remove->setCursor(Qt::PointingHandCursor);
    remove->setToolTip(ComposerContextBar::tr("Remove"));
    remove->setAccessibleName(ComposerContextBar::tr("Remove %1").arg(text));
    hl->addWidget(label);
    hl->addWidget(remove);
    chip->setToolTip(tip);
    chip->setAccessibleName(text);
    QObject::connect(remove, &QToolButton::clicked, q, [this, fn = std::move(on_remove)]() {
        fn();
        changed();
        add_btn->setFocus();
    });
    chips_flow->addWidget(chip);
}

void ComposerContextBar::Impl::rebuild_chips() {
    while (QLayoutItem* item = chips_flow->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->hide();
            w->deleteLater();
        }
        delete item;
    }
    for (const QString& s : std::as_const(data.symbols))
        add_chip(s, ComposerContextBar::tr("Symbol %1 goes with every message").arg(s),
                 [this, s]() { data.symbols.removeAll(s); });
    if (!data.portfolio_id.isEmpty() || !data.portfolio_name.isEmpty())
        add_chip(ComposerContextBar::tr("Portfolio: %1").arg(data.portfolio_name.isEmpty() ? data.portfolio_id
                                                                                           : data.portfolio_name),
                 ComposerContextBar::tr("The portfolio's name goes with every message"), [this]() {
                     data.portfolio_id.clear();
                     data.portfolio_name.clear();
                 });
    if (!data.profile_name.isEmpty())
        add_chip(ComposerContextBar::tr("Profile: %1").arg(data.profile_name),
                 ComposerContextBar::tr("The investment profile's name goes with every message. The profile the agent "
                                        "applies is the one picked in the header."),
                 [this]() {
                     data.profile_id = 0;
                     data.profile_name.clear();
                 });
    if (!data.attachment_text.isEmpty()) {
        const QString size = QLocale().toString(static_cast<qlonglong>(data.attachment_text.size()));
        QString text = ComposerContextBar::tr("Attached: %1 · %2 chars").arg(data.attachment_name, size);
        QString tip = ComposerContextBar::tr("Inlined into your next message (at most %1 characters).")
                          .arg(QLocale().toString(kMaxAttachmentChars));
        if (attachment_truncated) {
            text += ComposerContextBar::tr(" (cut)");
            tip += QLatin1Char('\n') + ComposerContextBar::tr("The file has about %1 characters: only the first %2 "
                                                              "are sent.")
                                           .arg(QLocale().toString(attachment_total), size);
        }
        add_chip(text, tip, [this]() {
            data.attachment_name.clear();
            data.attachment_text.clear();
            attachment_id.clear();
            attachment_truncated = false;
            attachment_total = 0;
        });
    }
    chips->setVisible(chips_flow->count() > 0);
    q->updateGeometry();
}

// ── Engine ───────────────────────────────────────────────────────────────────

void ComposerContextBar::Impl::update_engine() {
    harness = rv::harness_engine_option_now();
    engine_menu->clear(); // deletes the old actions; they leave engine_group with it
    for (CcbChoice c : {CcbChoice::CloudTurn, CcbChoice::CloudPlan, CcbChoice::Local, CcbChoice::Harness}) {
        QString why;
        bool ok = false;
        QString text = ccb_choice_text(c);
        if (c == CcbChoice::Harness) {
            if (!harness.listed && choice != CcbChoice::Harness)
                continue; // no harness engine in this build
            ok = harness.enabled;
            why = harness.reason;
            text = ok ? ComposerContextBar::tr("Harness · %1").arg(harness.label)
                      : ComposerContextBar::tr("Harness — %1").arg(harness.state);
        } else {
            ok = rv::executor_available(ccb_executor_of(c), &why);
            if (!ok)
                text = ComposerContextBar::tr("%1 — not available yet").arg(text);
        }
        QAction* a = engine_menu->addAction(text.replace(QLatin1Char('&'), QStringLiteral("&&")));
        a->setCheckable(true);
        a->setChecked(c == choice);
        a->setEnabled(ok || c == choice);
        a->setToolTip(ok ? ccb_choice_help(c) : why);
        engine_group->addAction(a);
        QObject::connect(a, &QAction::triggered, q, [this, c]() { set_choice(c, true); });
    }
    engine_btn->setText(ccb_choice_text(choice) + QStringLiteral(" ▾"));
    engine_btn->setToolTip(ccb_choice_help(choice));

    // The harness that runs a harness task, and who pays for it.
    const bool harness_chosen = choice == CcbChoice::Harness;
    harness_badge->setVisible(harness_chosen);
    if (harness_chosen) {
        harness_note->setText(harness.enabled ? rv::harness_billing_note(harness)
                                              : ComposerContextBar::tr("Harness unavailable: %1").arg(harness.state));
        harness_note->setToolTip(harness.enabled ? ccb_choice_help(choice) : harness.reason);
    }
    harness_note->setVisible(harness_chosen && !harness_note->text().isEmpty());

    QString plan_why;
    const bool plan_ok = rv::executor_available(ccb_executor_of(CcbChoice::CloudPlan), &plan_why);
    task_btn->setChecked(choice != CcbChoice::CloudTurn);
    if (choice == CcbChoice::Local) {
        task_btn->setEnabled(false);
        task_btn->setToolTip(ComposerContextBar::tr("A local agent run is always a multi-step task."));
    } else if (choice == CcbChoice::Harness) {
        task_btn->setEnabled(false);
        task_btn->setToolTip(ComposerContextBar::tr("A harness run is always a task."));
    } else if (!plan_ok && choice == CcbChoice::CloudTurn) {
        task_btn->setEnabled(false);
        task_btn->setToolTip(plan_why);
    } else {
        task_btn->setEnabled(true);
        task_btn->setToolTip(ComposerContextBar::tr("Send the next message as a planned multi-step task instead of "
                                                    "one reply. It keeps going while you work elsewhere; follow it in "
                                                    "PLAN and ACTIVITY."));
    }

    const bool task = choice != CcbChoice::CloudTurn;
    budget->setVisible(task);
    if (!task) {
        budget->setChecked(false);
        budget_editor->setVisible(false);
        confirm->setVisible(false);
    }
    if (task) {
        const agt::RunBudget b = q->budget();
        const rv::BudgetConfirmation c =
            rv::budget_confirmation(ccb_executor_of(choice), b, agt::run_plan_prompts::kPlanHardMaxSteps);
        budget->setText(ComposerContextBar::tr("BUDGET · %1 ▾").arg(c.summary));
        const bool warn = c.ask;
        budget->setToolTip(choice == CcbChoice::Local
                               ? ComposerContextBar::tr("The run stops after its steps (your own model, no credits). "
                                                        "Click to change the budget.")
                           : choice == CcbChoice::Harness
                               ? ComposerContextBar::tr("The harness stops at the time limit (no Fincept credits; your "
                                                        "API key is billed). Click to change the budget.")
                               : ComposerContextBar::tr("A multi-step run costs about (steps + 2) × %1 credits: a plan "
                                                        "turn, one turn per step and a synthesis. It stops at whichever "
                                                        "budget limit comes first. Click to change the budget.")
                                     .arg(rv::format_credits(agt::kCloudTurnCredits)));
        max_credits->setEnabled(!ccb_always_task(choice));
        if (budget->property("warn").toBool() != warn) {
            budget->setProperty("warn", warn);
            budget->style()->unpolish(budget);
            budget->style()->polish(budget);
        }
    }
}

void ComposerContextBar::Impl::set_choice(CcbChoice c, bool user) {
    if (c == choice) {
        update_engine();
        return;
    }
    if (user) {
        QString why;
        const rv::HarnessEngineOption h = rv::harness_engine_option_now();
        const bool ok = c == CcbChoice::Harness ? h.enabled : rv::executor_available(ccb_executor_of(c), &why);
        if (!ok) {
            show_note(c == CcbChoice::Harness ? h.reason : why);
            update_engine();
            return;
        }
    }
    const QString old_engine = q->engine();
    const bool old_task = q->run_as_task();
    choice = c;
    update_engine();
    if (q->engine() != old_engine)
        emit q->engine_changed(q->engine());
    if (q->run_as_task() != old_task)
        emit q->run_as_task_toggled(q->run_as_task());
}

// ── Context menu and loaders ─────────────────────────────────────────────────

void ComposerContextBar::Impl::fill_add_menu() {
    add_menu->clear();
    QAction* sym = add_menu->addAction(ComposerContextBar::tr("Symbol…"));
    sym->setToolTip(ComposerContextBar::tr("Type a ticker the agent should focus on"));
    QObject::connect(sym, &QAction::triggered, q, [this]() { symbol_entry->begin(); });
    add_menu->addMenu(portfolio_menu);
    add_menu->addMenu(profile_menu);
    add_menu->addSeparator();
    QAction* file = add_menu->addAction(file_reading ? ComposerContextBar::tr("Reading the file…")
                                                     : ComposerContextBar::tr("Attach a text file…"));
    file->setEnabled(!file_reading);
    file->setToolTip(ComposerContextBar::tr("Plain text up to %1 characters, inlined into your next message")
                         .arg(QLocale().toString(kMaxAttachmentChars)));
    QObject::connect(file, &QAction::triggered, q, [this]() { attach_file(); });
    const QMimeData* mime = QApplication::clipboard() ? QApplication::clipboard()->mimeData() : nullptr;
    const bool has_text = mime && mime->hasText() && !mime->text().trimmed().isEmpty();
    QAction* paste = add_menu->addAction(ComposerContextBar::tr("Attach the clipboard text"));
    paste->setEnabled(has_text);
    paste->setToolTip(has_text ? ComposerContextBar::tr("Inline the copied text into your next message")
                               : ComposerContextBar::tr("The clipboard holds no text"));
    QObject::connect(paste, &QAction::triggered, q, [this]() {
        const QString text = QApplication::clipboard() ? QApplication::clipboard()->text() : QString();
        if (text.trimmed().isEmpty()) {
            show_note(ComposerContextBar::tr("The clipboard holds no text."));
            return;
        }
        int cap = std::min<int>(static_cast<int>(text.size()), kMaxAttachmentChars);
        if (cap < text.size() && cap > 0 && text.at(cap - 1).isHighSurrogate())
            --cap;
        attach_text(ComposerContextBar::tr("clipboard"), text.left(cap), text.size(), cap < text.size());
    });
    add_menu->addSeparator();
    const bool any = !data.symbols.isEmpty() || !data.portfolio_id.isEmpty() || !data.profile_name.isEmpty() ||
                     !data.attachment_text.isEmpty();
    QAction* clear = add_menu->addAction(ComposerContextBar::tr("Remove all context"));
    clear->setEnabled(any);
    clear->setToolTip(any ? QString() : ComposerContextBar::tr("No context added"));
    QObject::connect(clear, &QAction::triggered, q, [this]() { q->clear_context(); });
}

void ComposerContextBar::Impl::fill_portfolio_menu() {
    portfolio_menu->clear();
    if (portfolios_loading && !portfolios_loaded) {
        portfolio_menu->addAction(ComposerContextBar::tr("Loading…"))->setEnabled(false);
        return;
    }
    if (!portfolios_error.isEmpty()) {
        QAction* a = portfolio_menu->addAction(ComposerContextBar::tr("Portfolios could not be loaded"));
        a->setEnabled(false);
        a->setToolTip(portfolios_error);
        QAction* retry = portfolio_menu->addAction(ComposerContextBar::tr("Try again"));
        QObject::connect(retry, &QAction::triggered, q, [this]() { load_portfolios(); });
        return;
    }
    if (portfolios.isEmpty()) {
        QAction* a = portfolio_menu->addAction(ComposerContextBar::tr("No portfolios yet"));
        a->setEnabled(false);
        a->setToolTip(ComposerContextBar::tr("Create one in the Portfolio screen"));
        return;
    }
    for (const auto& p : std::as_const(portfolios)) {
        QAction* a = portfolio_menu->addAction(p.second);
        a->setCheckable(true);
        a->setChecked(p.first == data.portfolio_id);
        const QString id = p.first;
        const QString name = p.second;
        QObject::connect(a, &QAction::triggered, q, [this, id, name]() {
            data.portfolio_id = id;
            data.portfolio_name = name;
            changed();
        });
    }
}

void ComposerContextBar::Impl::fill_profile_menu() {
    profile_menu->clear();
    if (profiles_loading && !profiles_loaded) {
        profile_menu->addAction(ComposerContextBar::tr("Loading…"))->setEnabled(false);
        return;
    }
    if (!profiles_error.isEmpty()) {
        QAction* a = profile_menu->addAction(ComposerContextBar::tr("Profiles could not be loaded"));
        a->setEnabled(false);
        a->setToolTip(profiles_error);
        QAction* retry = profile_menu->addAction(ComposerContextBar::tr("Try again"));
        QObject::connect(retry, &QAction::triggered, q, [this]() { load_profiles(); });
        return;
    }
    if (profiles.isEmpty()) {
        QAction* a = profile_menu->addAction(ComposerContextBar::tr("No investment profiles"));
        a->setEnabled(false);
        a->setToolTip(ComposerContextBar::tr("Create profiles on the Fincept website"));
        return;
    }
    for (const AgentProfile& p : std::as_const(profiles)) {
        QAction* a = profile_menu->addAction(p.is_default ? ComposerContextBar::tr("%1 (default)").arg(p.name) : p.name);
        a->setCheckable(true);
        a->setChecked(p.id == data.profile_id);
        const int id = p.id;
        const QString name = p.name;
        QObject::connect(a, &QAction::triggered, q, [this, id, name]() {
            data.profile_id = id;
            data.profile_name = name;
            changed();
        });
    }
}

void ComposerContextBar::Impl::load_portfolios() {
    portfolios_loading = true;
    portfolios_error.clear();
    QPointer<ComposerContextBar> self(q);
    QThreadPool::globalInstance()->start([self]() {
        auto r = PortfolioRepository::instance().list_portfolios();
        QVector<QPair<QString, QString>> list;
        QString err;
        if (r.is_ok()) {
            for (const portfolio::Portfolio& p : r.value())
                list.append({p.id, p.name});
        } else {
            err = QString::fromStdString(r.error());
        }
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, list, err]() {
                if (!self)
                    return;
                Impl* d = self->d_.get();
                d->portfolios = list;
                d->portfolios_error = err;
                d->portfolios_loaded = err.isEmpty();
                d->portfolios_loading = false;
                if (d->portfolio_menu->isVisible())
                    d->fill_portfolio_menu();
            },
            Qt::QueuedConnection);
    });
}

void ComposerContextBar::Impl::load_profiles() {
    profiles_loading = true;
    profiles_error.clear();
    QPointer<ComposerContextBar> self(q);
    ChatModeService::instance().list_profiles([self](bool ok, QVector<AgentProfile> list, QString err) {
        if (!self)
            return;
        Impl* d = self->d_.get();
        d->profiles_loading = false;
        d->profiles_loaded = ok;
        d->profiles = ok ? list : QVector<AgentProfile>{};
        d->profiles_error = ok ? QString() : (err.isEmpty() ? ComposerContextBar::tr("The server did not answer.") : err);
        if (d->profile_menu->isVisible())
            d->fill_profile_menu();
    });
}

void ComposerContextBar::Impl::attach_file() {
    // Non-modal: open() returns at once; the choice arrives through fileSelected.
    auto* dlg = new QFileDialog(q, ComposerContextBar::tr("Attach a text file"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setFileMode(QFileDialog::ExistingFile);
    dlg->setNameFilters({ComposerContextBar::tr("Text files (*.txt *.md *.csv *.tsv *.json *.log *.xml *.yaml *.yml)"),
                         ComposerContextBar::tr("All files (*)")});
    QPointer<ComposerContextBar> self(q);
    QObject::connect(dlg, &QFileDialog::fileSelected, q, [this, self](const QString& path) {
        if (!self || path.isEmpty())
            return;
        read_file(path);
    });
    dlg->open();
}

void ComposerContextBar::Impl::read_file(const QString& path) {
    if (file_reading) {
        show_note(ComposerContextBar::tr("Still reading the previous file…"));
        return;
    }
    file_reading = true;
    const QString name = QFileInfo(path).fileName();
    QPointer<ComposerContextBar> self(q);
    QThreadPool::globalInstance()->start([self, path, name]() {
        const CcbFileRead r = ccb_read_text_file(path);
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, r, name]() {
                if (!self)
                    return;
                Impl* d = self->d_.get();
                d->file_reading = false;
                if (!r.ok) {
                    d->show_note(r.error);
                    return;
                }
                if (r.text.trimmed().isEmpty()) {
                    d->show_note(ComposerContextBar::tr("The file is empty."));
                    return;
                }
                d->attach_text(name, r.text, r.total_chars, r.truncated);
            },
            Qt::QueuedConnection);
    });
}

void ComposerContextBar::Impl::attach_text(const QString& name, const QString& text, qint64 total_chars,
                                           bool truncated) {
    data.attachment_name = name;
    data.attachment_text = text;
    attachment_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    attachment_truncated = truncated;
    attachment_total = total_chars;
    changed();
    if (truncated)
        show_note(ComposerContextBar::tr("Only the first %1 characters are attached.")
                      .arg(QLocale().toString(static_cast<qlonglong>(text.size()))));
}

// ── ComposerContextBar ───────────────────────────────────────────────────────

ComposerContextBar::ComposerContextBar(QWidget* parent) : QWidget(parent), d_(std::make_unique<Impl>()) {
    d_->q = this;
    d_->build();
    d_->load_budget();
    d_->wire();
    d_->retranslate();
}

ComposerContextBar::~ComposerContextBar() = default;

QJsonObject ComposerContextBar::context() const {
    const rv::ComposerContextData& c = d_->data;
    QJsonObject o;
    if (!c.symbols.isEmpty())
        o.insert(QStringLiteral("symbols"), QJsonArray::fromStringList(c.symbols));
    if (!c.portfolio_id.isEmpty())
        o.insert(QStringLiteral("portfolio_id"), c.portfolio_id);
    if (!c.portfolio_name.isEmpty())
        o.insert(QStringLiteral("portfolio_name"), c.portfolio_name);
    if (c.profile_id > 0)
        o.insert(QStringLiteral("profile_id"), c.profile_id);
    if (!c.profile_name.isEmpty())
        o.insert(QStringLiteral("profile_name"), c.profile_name);
    if (!c.attachment_text.isEmpty()) {
        // The text itself is in the prompt (compose_prompt), never in the run's context.
        QJsonObject a;
        a.insert(QStringLiteral("name"), c.attachment_name);
        a.insert(QStringLiteral("chars"), static_cast<int>(c.attachment_text.size()));
        a.insert(QStringLiteral("id"), d_->attachment_id);
        if (d_->attachment_truncated)
            a.insert(QStringLiteral("truncated"), true);
        o.insert(QStringLiteral("attachment"), a);
    }
    return o;
}

void ComposerContextBar::set_context(const QJsonObject& context) {
    rv::ComposerContextData c;
    const QJsonValue symbols = context.value(QStringLiteral("symbols"));
    QStringList raw;
    if (symbols.isArray()) {
        for (const QJsonValue& v : symbols.toArray())
            raw << v.toString();
    } else if (symbols.isString()) {
        raw = symbols.toString().split(QRegularExpression(QStringLiteral("[,\\s]+")), Qt::SkipEmptyParts);
    }
    if (context.contains(QStringLiteral("symbol")))
        raw << context.value(QStringLiteral("symbol")).toString();
    for (const QString& s : std::as_const(raw)) {
        const QString n = ccb_normalize_symbol(s);
        if (!n.isEmpty() && !c.symbols.contains(n))
            c.symbols << n;
    }
    c.portfolio_id = context.value(QStringLiteral("portfolio_id")).toVariant().toString();
    c.portfolio_name = context.value(QStringLiteral("portfolio_name")).toString();
    c.profile_id = context.value(QStringLiteral("profile_id")).toInt();
    c.profile_name = context.value(QStringLiteral("profile_name")).toString();
    // An attachment comes with its text ({"name", "text"}); one without text
    // (a run's stored context) cannot be inlined again and is left out.
    const QJsonObject a = context.value(QStringLiteral("attachment")).toObject();
    const QString text = a.value(QStringLiteral("text")).toString();
    d_->attachment_id.clear();
    d_->attachment_truncated = false;
    d_->attachment_total = 0;
    if (!text.isEmpty()) {
        int cap = std::min<int>(static_cast<int>(text.size()), kMaxAttachmentChars);
        if (cap < text.size() && cap > 0 && text.at(cap - 1).isHighSurrogate())
            --cap;
        c.attachment_name = a.value(QStringLiteral("name")).toString();
        c.attachment_text = text.left(cap);
        d_->attachment_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        d_->attachment_truncated = cap < text.size();
        d_->attachment_total = text.size();
    }
    d_->data = c;
    d_->changed();
}

void ComposerContextBar::clear_context() {
    d_->data = {};
    d_->attachment_id.clear();
    d_->attachment_truncated = false;
    d_->attachment_total = 0;
    d_->changed();
}

QString ComposerContextBar::compose_prompt(const QString& user_text) const {
    return rv::compose_run_prompt(user_text, d_->data, kMaxAttachmentChars, agt::kRunPromptMaxChars).text;
}

QString ComposerContextBar::engine() const {
    switch (d_->choice) {
        case CcbChoice::Local:
            return QString::fromLatin1(agt::kEngineLocal);
        case CcbChoice::Harness:
            return QString::fromLatin1(rv::kHarnessEngineId);
        case CcbChoice::CloudTurn:
        case CcbChoice::CloudPlan:
            break;
    }
    return QString::fromLatin1(agt::kEngineCloud);
}

void ComposerContextBar::set_engine(const QString& engine) {
    if (engine == QLatin1String(agt::kEngineLocal))
        d_->set_choice(CcbChoice::Local, false);
    else if (engine == QLatin1String(rv::kHarnessEngineId))
        d_->set_choice(CcbChoice::Harness, false);
    else if (ccb_always_task(d_->choice))
        d_->set_choice(CcbChoice::CloudTurn, false);
}

bool ComposerContextBar::run_as_task() const {
    return d_->choice != CcbChoice::CloudTurn;
}

void ComposerContextBar::set_run_as_task(bool on) {
    if (ccb_always_task(d_->choice))
        return; // always a task
    d_->set_choice(on ? CcbChoice::CloudPlan : CcbChoice::CloudTurn, false);
}

agent_tasks::RunBudget ComposerContextBar::budget() const {
    agt::RunBudget b;
    if (!run_as_task())
        return b;
    b = d_->task_budget;
    if (ccb_always_task(d_->choice))
        b.max_credits = 0; // the user's own model / API key: no Fincept credits
    return b;
}

double ComposerContextBar::estimated_credits() const {
    return agt::estimate_run_cost(ccb_executor_of(d_->choice), budget(), agt::run_plan_prompts::kPlanHardMaxSteps).credits;
}

void ComposerContextBar::set_budget(const agent_tasks::RunBudget& b) {
    d_->task_budget.max_credits = std::max(0.0, b.max_credits);
    d_->task_budget.max_steps = std::clamp(b.max_steps, 0, agt::run_plan_prompts::kPlanHardMaxSteps);
    d_->task_budget.max_minutes = std::max(0, b.max_minutes);
    d_->budget_to_editor();
    d_->store_budget();
    d_->update_engine();
}

bool ComposerContextBar::needs_confirmation() const {
    if (!run_as_task())
        return false;
    return rv::budget_confirmation(ccb_executor_of(d_->choice), budget(), agt::run_plan_prompts::kPlanHardMaxSteps).ask;
}

void ComposerContextBar::ask_confirmation(double balance) {
    const rv::BudgetConfirmation c =
        rv::budget_confirmation(ccb_executor_of(d_->choice), budget(), agt::run_plan_prompts::kPlanHardMaxSteps, balance);
    if (!c.ask) {
        emit confirmed();
        return;
    }
    d_->confirm_text->setText(c.question);
    d_->confirm->setVisible(true);
    d_->confirm_yes->setFocus();
}

// ── 2026-11-17 revamp: chips edited from the composer and the CONTEXT page ───

QStringList ComposerContextBar::symbols() const {
    return d_->data.symbols;
}

void ComposerContextBar::add_symbol(const QString& symbol) {
    const QString s = ccb_normalize_symbol(symbol);
    if (s.isEmpty() || d_->data.symbols.contains(s))
        return;
    d_->data.symbols << s;
    d_->changed();
}

void ComposerContextBar::remove_symbol(const QString& symbol) {
    if (d_->data.symbols.removeAll(symbol) > 0)
        d_->changed();
}

QString ComposerContextBar::portfolio_name() const {
    return d_->data.portfolio_name.isEmpty() ? d_->data.portfolio_id : d_->data.portfolio_name;
}

void ComposerContextBar::clear_portfolio() {
    if (d_->data.portfolio_id.isEmpty() && d_->data.portfolio_name.isEmpty())
        return;
    d_->data.portfolio_id.clear();
    d_->data.portfolio_name.clear();
    d_->changed();
}

QString ComposerContextBar::profile_name() const {
    return d_->data.profile_name;
}

void ComposerContextBar::clear_profile() {
    if (d_->data.profile_id == 0 && d_->data.profile_name.isEmpty())
        return;
    d_->data.profile_id = 0;
    d_->data.profile_name.clear();
    d_->changed();
}

QString ComposerContextBar::attachment_name() const {
    return d_->data.attachment_text.isEmpty() ? QString() : d_->data.attachment_name;
}

int ComposerContextBar::attachment_chars() const {
    return static_cast<int>(d_->data.attachment_text.size());
}

void ComposerContextBar::clear_attachment() {
    if (d_->data.attachment_text.isEmpty())
        return;
    d_->data.attachment_name.clear();
    d_->data.attachment_text.clear();
    d_->attachment_id.clear();
    d_->attachment_truncated = false;
    d_->attachment_total = 0;
    d_->changed();
}

void ComposerContextBar::attach_text(const QString& name, const QString& text) {
    if (text.trimmed().isEmpty())
        return;
    int cap = std::min<int>(static_cast<int>(text.size()), kMaxAttachmentChars);
    if (cap < text.size() && cap > 0 && text.at(cap - 1).isHighSurrogate())
        --cap;
    d_->attach_text(name.trimmed().isEmpty() ? tr("pasted text") : name.trimmed(), text.left(cap), text.size(),
                    cap < text.size());
}

void ComposerContextBar::attach_file(const QString& path) {
    if (!path.isEmpty())
        d_->read_file(path);
}

void ComposerContextBar::open_attach_dialog() {
    d_->attach_file();
}

void ComposerContextBar::begin_symbol_entry() {
    d_->symbol_entry->begin();
}

void ComposerContextBar::popup_add_menu(const QPoint& global_pos) {
    d_->fill_add_menu();
    d_->add_menu->popup(global_pos); // non-blocking
}

QMenu* ComposerContextBar::engine_menu() const {
    return d_->engine_menu;
}

QString ComposerContextBar::engine_label() const {
    if (d_->choice == CcbChoice::Harness)
        return d_->harness.enabled ? tr("Harness · %1").arg(d_->harness.label) : tr("Harness — %1").arg(d_->harness.state);
    return ccb_choice_text(d_->choice);
}

QString ComposerContextBar::executor_id() const {
    return ccb_executor_of(d_->choice);
}

QString ComposerContextBar::budget_summary() const {
    return rv::budget_confirmation(ccb_executor_of(d_->choice), budget(), agt::run_plan_prompts::kPlanHardMaxSteps).summary;
}

} // namespace fincept::chat_mode
