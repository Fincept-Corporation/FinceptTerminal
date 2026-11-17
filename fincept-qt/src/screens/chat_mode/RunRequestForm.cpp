// RunRequestForm.cpp — template parameters, task text, engine and budget of a
// run (WP6). See the header.

#include "screens/chat_mode/RunRequestForm.h"

#include "screens/chat_mode/RunViewLogic.h"
#include "screens/chat_mode/RunViewWidgets.h"
#include "screens/portfolio/PortfolioTypes.h"
#include "services/agent_tasks/RunPlanPrompts.h"
#include "services/agent_tasks/RunTemplateLibrary.h"
#include "services/agent_tasks/ScheduleLogic.h"
#include "storage/repositories/PortfolioRepository.h"
#include "storage/repositories/WatchlistRepository.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/ModelBadge.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDate>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPlainTextEdit>
#include <QPointer>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QThreadPool>
#include <QToolButton>
#include <QVBoxLayout>

#include <functional>

namespace fincept::chat_mode {

namespace agt = fincept::agent_tasks;
namespace rv = fincept::chat_mode::run_view;
namespace sl = fincept::agent_tasks::schedule_logic;

namespace run_request_form_detail {

constexpr int kRrfMaxPromptChars = 20000; ///< what a person types (templates add their own text)

QString rrf_style() {
    return QStringLiteral(
               "#runRequestForm{background:transparent;}"
               "#runRequestForm QLabel{background:transparent;border:none;color:%1;font-size:%2px;}"
               "#runRequestForm QLabel#rrfTitle{color:%3;font-size:%4px;font-weight:700;}"
               "#runRequestForm QLabel#rrfHint{color:%5;}"
               "#runRequestForm QLabel#rrfSection{color:%5;font-weight:700;letter-spacing:1px;}"
               "#runRequestForm QLabel#rrfExpected{color:%6;}"
               "#runRequestForm QLabel#rrfEstimate[warn=\"true\"]{color:%7;}"
               "#runRequestForm QLineEdit,#runRequestForm QPlainTextEdit,#runRequestForm QComboBox,"
               "#runRequestForm QSpinBox,#runRequestForm QDoubleSpinBox,#runRequestForm QDateEdit{background:%8;color:%3;"
               "border:1px solid %9;padding:2px 6px;font-size:%2px;}")
               .arg(ui::colors::TEXT_SECONDARY(), ui::fonts::px(-2), ui::colors::TEXT_PRIMARY(), ui::fonts::px(0),
                    ui::colors::TEXT_TERTIARY(), ui::colors::INFO(), ui::colors::WARNING(), ui::colors::BG_BASE(),
                    ui::colors::BORDER_DIM()) +
           QStringLiteral(
               "#runRequestForm QLineEdit:focus,#runRequestForm QPlainTextEdit:focus,#runRequestForm QComboBox:focus,"
               "#runRequestForm QSpinBox:focus,#runRequestForm QDoubleSpinBox:focus,#runRequestForm QDateEdit:focus"
               "{border-color:%1;}"
               "#runRequestForm QPlainTextEdit[readOnly=\"true\"]{color:%2;background:%3;}"
               "#runRequestForm QComboBox QAbstractItemView{background:%3;color:%4;border:1px solid %5;}"
               "#runRequestForm QToolButton{background:%3;color:%2;border:1px solid %5;padding:2px 8px;font-size:%6px;"
               "font-weight:700;}"
               "#runRequestForm QToolButton:hover{color:%4;border-color:%1;}"
               "#runRequestForm QToolButton:checked{background:%1;color:%7;border-color:%1;}"
               "#runRequestForm QToolButton#rrfChip{background:%8;color:%4;font-weight:400;}")
               .arg(ui::colors::AMBER(), ui::colors::TEXT_SECONDARY(), ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(),
                    ui::colors::BORDER_DIM(), ui::fonts::px(-3), ui::colors::TEXT_ON_ACCENT(),
                    ui::colors::tint(QColor(ui::colors::INFO()), 0.12));
}

QLabel* rrf_label(QWidget* parent, const char* name = nullptr) {
    auto* l = new QLabel(parent);
    if (name)
        l->setObjectName(QLatin1String(name));
    l->setWordWrap(true);
    l->setTextFormat(Qt::PlainText);
    return l;
}

/// The engine combo's item data (also stored in values()): append only.
enum RrfEngine { kRrfCloudTurn = 0, kRrfCloudPlan, kRrfLocal, kRrfHarness };

bool rrf_no_credits(int choice) {
    return choice == kRrfLocal || choice == kRrfHarness;
}

QString rrf_executor(int choice) {
    switch (choice) {
        case kRrfCloudPlan:
            return QString::fromLatin1(agt::kExecutorCloudPlan);
        case kRrfLocal:
            return QString::fromLatin1(agt::kExecutorLocal);
        case kRrfHarness:
            return QString::fromLatin1(rv::kHarnessEngineId);
        default:
            break;
    }
    return QString::fromLatin1(agt::kExecutorCloudTurn);
}

/// Portfolios and watchlists of the terminal, read once per form (worker thread).
struct RrfLists {
    QVector<QPair<QString, QString>> portfolios; ///< id, name
    QVector<QPair<QString, QString>> watchlists;
    bool loaded = false;
    QString error;
};

} // namespace run_request_form_detail

using namespace run_request_form_detail;

// ── Impl ─────────────────────────────────────────────────────────────────────

struct RunRequestForm::Impl {
    struct Field {
        agt::RunTemplateParam p;
        std::function<void(QJsonObject&)> write;
        std::function<void(const QJsonObject&)> read;
    };

    RunRequestForm* q = nullptr;
    bool for_schedule = false;
    std::optional<agt::RunTemplate> tmpl;
    QString template_id;
    QJsonObject extra; ///< context chips / profile id
    QHash<QString, QJsonObject> remembered; ///< template id -> params entered (this session)
    QVector<Field> fields;
    RrfLists lists;
    bool lists_loading = false;
    QVector<std::function<void()>> on_lists; ///< combos to fill when the lists arrive
    double balance = -1;
    bool building = false;
    bool initialized = false; ///< set_template() ran once

    QLabel* title = nullptr;
    QLabel* description = nullptr;
    QLabel* expected = nullptr;
    QWidget* params_box = nullptr;
    QGridLayout* params_grid = nullptr;
    QLabel* text_head = nullptr;
    QToolButton* edit_btn = nullptr;
    QPlainTextEdit* text = nullptr;
    QLabel* text_note = nullptr;
    QComboBox* engine = nullptr;
    QComboBox* mode = nullptr;
    QWidget* harness_row = nullptr;          ///< with the harness chosen: which harness / model, and the bill
    ui::ModelBadge* harness_badge = nullptr;
    QLabel* harness_note = nullptr;
    rv::HarnessEngineOption harness;         ///< as last asked (ModelRouter::routes_changed refreshes it)
    QDoubleSpinBox* credits = nullptr;
    QSpinBox* steps = nullptr;
    QSpinBox* minutes = nullptr;
    QLabel* estimate = nullptr;

    void build();
    void retranslate();
    void rebuild_params();
    void add_field_row(const agt::RunTemplateParam& p, QWidget* input, int row);
    QJsonObject context() const;
    void refresh_preview();
    void update_engine_state();
    /// The harness item of the engine combo and the harness row, from the router now.
    void refresh_harness();
    void update_estimate();
    void load_lists();
    void changed() {
        if (building)
            return;
        refresh_preview();
        update_estimate();
        emit q->changed();
    }
    agt::RunBudget budget() const {
        return {credits->value(), steps->value(), minutes->value()};
    }
};

void RunRequestForm::Impl::build() {
    q->setObjectName(QStringLiteral("runRequestForm"));
    q->setStyleSheet(rrf_style()); // before the children exist
    auto* vl = new QVBoxLayout(q);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(8);

    title = rrf_label(q, "rrfTitle");
    description = rrf_label(q, "rrfHint");
    expected = rrf_label(q, "rrfExpected");
    vl->addWidget(title);
    vl->addWidget(description);
    vl->addWidget(expected);

    params_box = new QWidget(q);
    params_grid = new QGridLayout(params_box);
    params_grid->setContentsMargins(0, 0, 0, 0);
    params_grid->setHorizontalSpacing(10);
    params_grid->setVerticalSpacing(6);
    params_grid->setColumnStretch(1, 1);
    vl->addWidget(params_box);

    auto* head = new QHBoxLayout;
    text_head = rrf_label(q, "rrfSection");
    edit_btn = new QToolButton(q);
    edit_btn->setCheckable(true);
    edit_btn->setCursor(Qt::PointingHandCursor);
    head->addWidget(text_head, 1);
    head->addWidget(edit_btn);
    vl->addLayout(head);
    text = new QPlainTextEdit(q);
    text->setTabChangesFocus(true);
    text->setMinimumHeight(QFontMetrics(ui::fonts::font(-2)).lineSpacing() * 6 + 12);
    vl->addWidget(text, 1);
    text_note = rrf_label(q, "rrfHint");
    vl->addWidget(text_note);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(6);
    auto* engine_lbl = rrf_label(q, "rrfSection");
    engine_lbl->setObjectName(QStringLiteral("rrfEngineLabel"));
    engine = new QComboBox(q);
    for (int i = kRrfCloudTurn; i <= kRrfHarness; ++i)
        engine->addItem(QString(), i);
    mode = new QComboBox(q);
    mode->addItem(QString(), QStringLiteral("lite"));
    mode->addItem(QString(), QStringLiteral("deep"));
    grid->addWidget(engine_lbl, 0, 0);
    grid->addWidget(engine, 0, 1);
    grid->addWidget(mode, 0, 2);
    harness_row = new QWidget(q);
    auto* hr = new rv::RvFlowLayout(harness_row, 8, 4);
    harness_badge = new ui::ModelBadge(QString::fromLatin1(ai_chat::kFeatureChatModeHarness), harness_row);
    hr->addWidget(harness_badge);
    harness_note = rrf_label(harness_row, "rrfHint");
    hr->addWidget(harness_note);
    harness_row->setVisible(false);
    grid->addWidget(harness_row, 1, 1, 1, 2);
    auto* budget_lbl = rrf_label(q, "rrfSection");
    budget_lbl->setObjectName(QStringLiteral("rrfBudgetLabel"));
    auto* budget_row = new QWidget(q);
    auto* bf = new rv::RvFlowLayout(budget_row, 8, 4);
    const auto spin_field = [budget_row, bf](const QString& name, auto make) {
        auto* box = new QWidget(budget_row);
        auto* hl = new QHBoxLayout(box);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(4);
        auto* l = new QLabel(box);
        l->setObjectName(name);
        hl->addWidget(l);
        auto* input = make(box);
        hl->addWidget(input);
        bf->addWidget(box);
        return input;
    };
    credits = spin_field(QStringLiteral("rrfCredits"), [](QWidget* p) {
        auto* s = new QDoubleSpinBox(p);
        s->setRange(0, 1000);
        s->setDecimals(0);
        s->setSingleStep(5);
        return s;
    });
    steps = spin_field(QStringLiteral("rrfSteps"), [](QWidget* p) {
        auto* s = new QSpinBox(p);
        s->setRange(0, agt::run_plan_prompts::kPlanHardMaxSteps);
        return s;
    });
    minutes = spin_field(QStringLiteral("rrfMinutes"), [](QWidget* p) {
        auto* s = new QSpinBox(p);
        s->setRange(0, 24 * 60);
        s->setSingleStep(5);
        return s;
    });
    grid->addWidget(budget_lbl, 2, 0, Qt::AlignTop);
    grid->addWidget(budget_row, 2, 1, 1, 2);
    estimate = rrf_label(q, "rrfEstimate");
    grid->addWidget(estimate, 3, 1, 1, 2);
    grid->setColumnStretch(1, 1);
    vl->addLayout(grid);

    QObject::connect(text, &QPlainTextEdit::textChanged, q, [this]() {
        if (!text->isReadOnly())
            changed();
    });
    QObject::connect(edit_btn, &QToolButton::toggled, q, [this](bool on) {
        text->setReadOnly(!on && tmpl.has_value());
        text->style()->unpolish(text);
        text->style()->polish(text);
        if (on && tmpl) {
            // Start from the filled task text itself (without the preview's notes).
            const QSignalBlocker block(text);
            text->setPlainText(
                agt::request_from_template(*tmpl, sl::expand_relative_dates(context(), QDate::currentDate())).prompt);
        }
        if (!on)
            refresh_preview(); // back to the template's text
        changed();
    });
    QObject::connect(engine, &QComboBox::currentIndexChanged, q, [this]() {
        update_engine_state();
        changed();
    });
    QObject::connect(mode, &QComboBox::currentIndexChanged, q, [this]() { changed(); });
    // A harness became (un)usable, or another harness / model was picked in the badge.
    QObject::connect(&ai_chat::ModelRouter::instance(), &ai_chat::ModelRouter::routes_changed, q, [this]() {
        refresh_harness();
        changed();
    });
    QObject::connect(credits, &QDoubleSpinBox::valueChanged, q, [this]() { changed(); });
    QObject::connect(steps, &QSpinBox::valueChanged, q, [this]() { changed(); });
    QObject::connect(minutes, &QSpinBox::valueChanged, q, [this]() { changed(); });
}

void RunRequestForm::Impl::retranslate() {
    text_head->setText(RunRequestForm::tr("WHAT THE AGENT IS ASKED"));
    edit_btn->setText(RunRequestForm::tr("EDIT TEXT"));
    edit_btn->setToolTip(RunRequestForm::tr("Change the task text yourself (it then no longer follows the parameters)"));
    if (auto* l = q->findChild<QLabel*>(QStringLiteral("rrfEngineLabel")))
        l->setText(RunRequestForm::tr("ENGINE"));
    if (auto* l = q->findChild<QLabel*>(QStringLiteral("rrfBudgetLabel")))
        l->setText(RunRequestForm::tr("BUDGET"));
    if (auto* l = q->findChild<QLabel*>(QStringLiteral("rrfCredits")))
        l->setText(RunRequestForm::tr("Max credits"));
    if (auto* l = q->findChild<QLabel*>(QStringLiteral("rrfSteps")))
        l->setText(RunRequestForm::tr("Max steps"));
    if (auto* l = q->findChild<QLabel*>(QStringLiteral("rrfMinutes")))
        l->setText(RunRequestForm::tr("Max minutes"));
    credits->setSpecialValueText(RunRequestForm::tr("no limit"));
    steps->setSpecialValueText(RunRequestForm::tr("auto"));
    minutes->setSpecialValueText(RunRequestForm::tr("no limit"));
    minutes->setToolTip(RunRequestForm::tr("Working time: pauses and questions to you do not count"));
    const QStringList names = {RunRequestForm::tr("Cloud · one reply"), RunRequestForm::tr("Cloud · multi-step task"),
                               RunRequestForm::tr("Local agent · your own model")};
    for (int i = 0; i < 3; ++i) {
        QString why;
        const bool ok = rv::executor_available(rrf_executor(i), &why);
        engine->setItemText(i, ok ? names.at(i) : RunRequestForm::tr("%1 — not available").arg(names.at(i)));
        engine->setItemData(i, ok ? QVariant() : QVariant(why), Qt::ToolTipRole);
    }
    refresh_harness();
    mode->setItemText(0, RunRequestForm::tr("Lite"));
    mode->setItemText(1, RunRequestForm::tr("Deep"));
    mode->setToolTip(RunRequestForm::tr("Deep works longer per turn (several work windows); both cost one turn"));
}

void RunRequestForm::Impl::load_lists() {
    if (lists.loaded || lists_loading)
        return;
    lists_loading = true;
    QPointer<RunRequestForm> self(q);
    QThreadPool::globalInstance()->start([self]() {
        RrfLists out;
        auto p = PortfolioRepository::instance().list_portfolios();
        if (p.is_ok())
            for (const portfolio::Portfolio& x : p.value())
                out.portfolios.append({x.id, x.name});
        else
            out.error = QString::fromStdString(p.error());
        auto w = WatchlistRepository::instance().list_all();
        if (w.is_ok())
            for (const Watchlist& x : w.value())
                out.watchlists.append({x.id, x.name});
        out.loaded = true;
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, out]() {
                if (!self)
                    return;
                Impl* d = self->d_.get();
                d->lists = out;
                d->lists_loading = false;
                const auto fill = std::exchange(d->on_lists, {});
                for (const auto& fn : fill)
                    fn();
            },
            Qt::QueuedConnection);
    });
}

void RunRequestForm::Impl::add_field_row(const agt::RunTemplateParam& p, QWidget* input, int row) {
    auto* l = rrf_label(params_box);
    l->setText(p.required ? p.label : RunRequestForm::tr("%1 (optional)").arg(p.label));
    l->setWordWrap(false);
    params_grid->addWidget(l, row, 0, Qt::AlignTop);
    params_grid->addWidget(input, row, 1);
    if (!p.hint.isEmpty())
        input->setToolTip(p.hint);
}

void RunRequestForm::Impl::rebuild_params() {
    building = true;
    while (QLayoutItem* item = params_grid->takeAt(0)) {
        if (QWidget* w = item->widget())
            w->deleteLater();
        delete item;
    }
    fields.clear();
    if (!tmpl) {
        params_box->setVisible(false);
        building = false;
        return;
    }
    params_box->setVisible(!tmpl->params.isEmpty());
    int row = 0;
    for (const agt::RunTemplateParam& p : tmpl->params) {
        Field f;
        f.p = p;
        QWidget* input = nullptr;
        switch (p.type) {
            case agt::TemplateParamType::Symbol:
            case agt::TemplateParamType::Symbols: {
                // The shared picker; a pick becomes a chip (× removes it).
                const bool multi = p.type == agt::TemplateParamType::Symbols;
                auto* box = new QWidget(params_box);
                auto* bl = new QVBoxLayout(box);
                bl->setContentsMargins(0, 0, 0, 0);
                bl->setSpacing(4);
                auto* picker = new ui::SymbolSearchBox(box);
                picker->set_compact(true);
                picker->set_clear_on_pick(true);
                picker->set_multi(multi);
                picker->set_recent_key(QStringLiteral("chat_mode_runs"));
                picker->set_placeholder(p.hint.isEmpty() ? RunRequestForm::tr("Ticker, name or ISIN") : p.hint);
                auto* chips = new QWidget(box);
                auto* cf = new rv::RvFlowLayout(chips, 4, 4);
                bl->addWidget(picker);
                bl->addWidget(chips);
                auto values = std::make_shared<QStringList>();
                const QString key = p.key;
                std::shared_ptr<std::function<void()>> redraw = std::make_shared<std::function<void()>>();
                const std::weak_ptr<std::function<void()>> redraw_weak = redraw; // no cycle through itself
                *redraw = [this, chips, cf, values, redraw_weak]() {
                    while (QLayoutItem* it = cf->takeAt(0)) {
                        if (QWidget* w = it->widget())
                            w->deleteLater();
                        delete it;
                    }
                    for (const QString& s : std::as_const(*values)) {
                        auto* chip = new QToolButton(chips);
                        chip->setObjectName(QStringLiteral("rrfChip"));
                        chip->setText(s + QStringLiteral("  ×"));
                        chip->setToolTip(RunRequestForm::tr("Remove %1").arg(s));
                        chip->setCursor(Qt::PointingHandCursor);
                        const std::weak_ptr<std::function<void()>> weak = redraw_weak;
                        QObject::connect(chip, &QToolButton::clicked, q, [this, values, s, weak]() {
                            values->removeAll(s);
                            if (auto r = weak.lock())
                                (*r)();
                            changed();
                        });
                        cf->addWidget(chip);
                    }
                    chips->setVisible(!values->isEmpty());
                };
                const auto add = [this, values, redraw, multi](const core::instrument::Instrument& inst) {
                    const QString s = inst.symbol.isEmpty() ? inst.display : inst.symbol;
                    if (s.isEmpty())
                        return;
                    if (!multi)
                        values->clear();
                    if (!values->contains(s))
                        values->append(s);
                    (*redraw)();
                    changed();
                };
                QObject::connect(picker, &ui::SymbolSearchBox::instrument_picked, q, add);
                QObject::connect(picker, &ui::SymbolSearchBox::instruments_picked, q,
                                 [add](const QVector<core::instrument::Instrument>& list) {
                                     for (const auto& i : list)
                                         add(i);
                                 });
                QObject::connect(picker, &ui::SymbolSearchBox::unresolved, q,
                                 [picker](const QStringList&, const QString& message) { picker->setToolTip(message); });
                f.write = [values, key, multi](QJsonObject& ctx) {
                    ctx[key] = multi ? values->join(QStringLiteral(", ")) : values->value(0);
                };
                f.read = [values, key, redraw](const QJsonObject& ctx) {
                    values->clear();
                    for (const QString& s : ctx.value(key).toString().split(QLatin1Char(','), Qt::SkipEmptyParts))
                        if (!s.trimmed().isEmpty())
                            values->append(s.trimmed());
                    (*redraw)();
                };
                (*redraw)();
                input = box;
                break;
            }
            case agt::TemplateParamType::Portfolio:
            case agt::TemplateParamType::Watchlist: {
                const bool is_portfolio = p.type == agt::TemplateParamType::Portfolio;
                auto* combo = new QComboBox(params_box);
                combo->addItem(RunRequestForm::tr("Loading…"), QString());
                const QString id_key = is_portfolio ? QStringLiteral("portfolio_id") : QStringLiteral("watchlist_id");
                const QString name_key = is_portfolio ? QStringLiteral("portfolio_name") : QStringLiteral("watchlist_name");
                auto pending = std::make_shared<QString>();
                QPointer<QComboBox> guard(combo);
                const auto fill = [this, guard, is_portfolio, pending]() {
                    if (!guard)
                        return;
                    const QSignalBlocker block(guard.data());
                    guard->clear();
                    const auto& list = is_portfolio ? lists.portfolios : lists.watchlists;
                    if (list.isEmpty()) {
                        guard->addItem(is_portfolio ? RunRequestForm::tr("No portfolios — create one in Portfolio")
                                                    : RunRequestForm::tr("No watchlists — create one in Watchlist"),
                                       QString());
                    } else {
                        guard->addItem(RunRequestForm::tr("Choose…"), QString());
                        for (const auto& e : list)
                            guard->addItem(e.second, e.first);
                    }
                    const int at = guard->findData(*pending);
                    guard->setCurrentIndex(at >= 0 ? at : 0);
                    changed();
                };
                if (lists.loaded)
                    fill();
                else {
                    on_lists.append(fill);
                    load_lists();
                }
                QObject::connect(combo, &QComboBox::currentIndexChanged, q, [this, combo, pending]() {
                    *pending = combo->currentData().toString();
                    changed();
                });
                f.write = [combo, id_key, name_key](QJsonObject& ctx) {
                    const QString id = combo->currentData().toString();
                    ctx[id_key] = id;
                    ctx[name_key] = id.isEmpty() ? QString() : combo->currentText();
                };
                f.read = [combo, pending, id_key](const QJsonObject& ctx) {
                    *pending = ctx.value(id_key).toVariant().toString();
                    const int at = combo->findData(*pending);
                    if (at >= 0) {
                        const QSignalBlocker block(combo);
                        combo->setCurrentIndex(at);
                    }
                };
                input = combo;
                break;
            }
            case agt::TemplateParamType::DateRange: {
                auto* box = new QWidget(params_box);
                auto* hl = new QHBoxLayout(box);
                hl->setContentsMargins(0, 0, 0, 0);
                hl->setSpacing(6);
                auto* range = new QComboBox(box);
                for (const QString& code : sl::date_range_codes())
                    range->addItem(sl::date_range_label(code), code);
                auto* from = new QDateEdit(QDate::currentDate().addMonths(-1), box);
                auto* to = new QDateEdit(QDate::currentDate(), box);
                for (QDateEdit* e : {from, to}) {
                    e->setCalendarPopup(true);
                    e->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
                }
                hl->addWidget(range);
                hl->addWidget(from);
                hl->addWidget(to);
                hl->addStretch();
                const auto custom_visible = [range, from, to]() {
                    const bool custom = range->currentData().toString() == QLatin1String("custom");
                    from->setVisible(custom);
                    to->setVisible(custom);
                };
                const int def = range->findData(p.default_value.isEmpty() ? QStringLiteral("30d") : p.default_value);
                range->setCurrentIndex(def >= 0 ? def : 2);
                custom_visible();
                QObject::connect(range, &QComboBox::currentIndexChanged, q, [this, custom_visible]() {
                    custom_visible();
                    changed();
                });
                QObject::connect(from, &QDateEdit::dateChanged, q, [this]() { changed(); });
                QObject::connect(to, &QDateEdit::dateChanged, q, [this]() { changed(); });
                f.write = [range, from, to](QJsonObject& ctx) {
                    const QString code = range->currentData().toString();
                    ctx[QStringLiteral("date_range")] = code;
                    if (code == QLatin1String("custom")) {
                        ctx[QStringLiteral("date_from")] = from->date().toString(Qt::ISODate);
                        ctx[QStringLiteral("date_to")] = to->date().toString(Qt::ISODate);
                    }
                };
                f.read = [range, from, to, custom_visible](const QJsonObject& ctx) {
                    const int at = range->findData(ctx.value(QStringLiteral("date_range")).toString());
                    if (at >= 0)
                        range->setCurrentIndex(at);
                    const QDate a = QDate::fromString(ctx.value(QStringLiteral("date_from")).toString(), Qt::ISODate);
                    const QDate b = QDate::fromString(ctx.value(QStringLiteral("date_to")).toString(), Qt::ISODate);
                    if (a.isValid())
                        from->setDate(a);
                    if (b.isValid())
                        to->setDate(b);
                    custom_visible();
                };
                input = box;
                break;
            }
            case agt::TemplateParamType::Choice: {
                auto* combo = new QComboBox(params_box);
                for (int i = 0; i < p.choices.size(); ++i)
                    combo->addItem(p.choice_labels.value(i, p.choices.at(i)), p.choices.at(i));
                const int def = combo->findData(p.default_value);
                combo->setCurrentIndex(def >= 0 ? def : 0);
                QObject::connect(combo, &QComboBox::currentIndexChanged, q, [this]() { changed(); });
                const QString key = p.key;
                f.write = [combo, key](QJsonObject& ctx) { ctx[key] = combo->currentData().toString(); };
                f.read = [combo, key](const QJsonObject& ctx) {
                    const int at = combo->findData(ctx.value(key).toString());
                    if (at >= 0)
                        combo->setCurrentIndex(at);
                };
                input = combo;
                break;
            }
            case agt::TemplateParamType::LongText: {
                auto* edit = new QPlainTextEdit(params_box);
                edit->setTabChangesFocus(true);
                edit->setPlainText(p.default_value);
                edit->setPlaceholderText(p.hint);
                edit->setFixedHeight(QFontMetrics(ui::fonts::font(-2)).lineSpacing() * 3 + 12);
                QObject::connect(edit, &QPlainTextEdit::textChanged, q, [this]() { changed(); });
                const QString key = p.key;
                f.write = [edit, key](QJsonObject& ctx) { ctx[key] = edit->toPlainText().trimmed(); };
                f.read = [edit, key](const QJsonObject& ctx) {
                    if (ctx.contains(key))
                        edit->setPlainText(ctx.value(key).toString());
                };
                input = edit;
                break;
            }
            case agt::TemplateParamType::Text: {
                auto* edit = new QLineEdit(params_box);
                edit->setText(p.default_value);
                edit->setPlaceholderText(p.hint);
                edit->setMaxLength(500);
                QObject::connect(edit, &QLineEdit::textChanged, q, [this]() { changed(); });
                const QString key = p.key;
                f.write = [edit, key](QJsonObject& ctx) { ctx[key] = edit->text().trimmed(); };
                f.read = [edit, key](const QJsonObject& ctx) {
                    if (ctx.contains(key))
                        edit->setText(ctx.value(key).toString());
                };
                input = edit;
                break;
            }
        }
        add_field_row(p, input, row++);
        fields.append(f);
    }
    building = false;
}

QJsonObject RunRequestForm::Impl::context() const {
    QJsonObject ctx;
    for (const Field& f : fields)
        if (f.write)
            f.write(ctx);
    return ctx;
}

void RunRequestForm::Impl::refresh_preview() {
    if (!tmpl) {
        text->setReadOnly(false);
        text_note->setText(RunRequestForm::tr("Say what to research, which figures and tables you need and what the final "
                                              "report should answer. A multi-step task is planned first."));
        return;
    }
    if (edit_btn->isChecked()) {
        text_note->setText(RunRequestForm::tr("Your own text is sent as it is: the parameters above no longer change it."));
        return;
    }
    const QJsonObject ctx = sl::expand_relative_dates(context(), QDate::currentDate());
    const agt::RunRequest r = agt::request_from_template(*tmpl, ctx);
    QString preview = r.prompt;
    QStringList notes;
    const QString pname = ctx.value(QStringLiteral("portfolio_name")).toString();
    if (!pname.isEmpty())
        notes << RunRequestForm::tr("+ the holdings of “%1” as they are when the run starts").arg(pname);
    const QString wname = ctx.value(QStringLiteral("watchlist_name")).toString();
    if (!wname.isEmpty())
        notes << RunRequestForm::tr("+ the symbols of the watchlist “%1”").arg(wname);
    if (!notes.isEmpty())
        preview += QStringLiteral("\n\n(") + notes.join(QStringLiteral("; ")) + QLatin1Char(')');
    if (text->toPlainText() != preview) {
        const QSignalBlocker block(text);
        text->setPlainText(preview);
    }
    text->setReadOnly(true);
    text_note->setText(for_schedule ? RunRequestForm::tr("Periods such as “last 7 days” are counted from the day each "
                                                         "scheduled run starts.")
                                    : RunRequestForm::tr("This is the text the agent gets. EDIT TEXT to change it."));
}

void RunRequestForm::Impl::refresh_harness() {
    harness = rv::harness_engine_option_now();
    const int at = engine->findData(kRrfHarness);
    if (at < 0)
        return;
    const bool chosen = engine->currentData().toInt() == kRrfHarness;
    QString text = harness.enabled ? RunRequestForm::tr("Harness · %1").arg(harness.label)
                                   : RunRequestForm::tr("Harness — %1").arg(harness.state);
    engine->setItemText(at, text);
    engine->setItemData(at,
                        harness.enabled ? RunRequestForm::tr("Your agent harness works through the task in its own "
                                                             "loop with the terminal's tools, read-only and paper only. "
                                                             "No Fincept credits: billed to your own API key. It cannot "
                                                             "pause.")
                                        : harness.reason,
                        Qt::ToolTipRole);
    // Disabled (still listed, with its reason) while no harness may run tasks; kept
    // selectable while it is the current choice, so a restored choice is not lost.
    if (auto* m = qobject_cast<QStandardItemModel*>(engine->model()))
        if (QStandardItem* item = m->item(at))
            item->setEnabled(harness.enabled || chosen);
    if (auto* view = qobject_cast<QListView*>(engine->view()))
        view->setRowHidden(at, !harness.listed && !chosen); // no harness engine in this build
    harness_row->setVisible(chosen);
    if (chosen) {
        harness_note->setText(harness.enabled ? rv::harness_billing_note(harness)
                                              : RunRequestForm::tr("Not available: %1").arg(harness.reason));
    }
}

void RunRequestForm::Impl::update_engine_state() {
    const int choice = engine->currentData().toInt();
    mode->setEnabled(!rrf_no_credits(choice));
    credits->setEnabled(!rrf_no_credits(choice));
    steps->setEnabled(choice != kRrfCloudTurn);
    credits->setToolTip(choice == kRrfLocal     ? RunRequestForm::tr("Local runs use your own model: no Fincept credits")
                        : choice == kRrfHarness ? RunRequestForm::tr("Harness runs are billed to your own API key: no "
                                                                     "Fincept credits")
                                                : RunRequestForm::tr("The run stops before a turn that would go over this"));
    refresh_harness();
    steps->setToolTip(choice == kRrfCloudTurn ? RunRequestForm::tr("One reply has no steps")
                                              : RunRequestForm::tr("The most steps the plan may have (auto: up to %1)")
                                                    .arg(agt::run_plan_prompts::kPlanHardMaxSteps));
}

void RunRequestForm::Impl::update_estimate() {
    const rv::BudgetConfirmation c = rv::budget_confirmation(rrf_executor(engine->currentData().toInt()), budget(),
                                                             agt::run_plan_prompts::kPlanHardMaxSteps, balance);
    QString text_est = RunRequestForm::tr("Estimate: %1").arg(c.summary);
    if (balance >= 0 && !rrf_no_credits(engine->currentData().toInt()))
        text_est += QStringLiteral(" · ") + RunRequestForm::tr("balance %1").arg(rv::format_credits(balance));
    if (c.ask)
        text_est += QStringLiteral(" · ") + RunRequestForm::tr("asks before it starts");
    estimate->setText(text_est);
    if (estimate->property("warn").toBool() != c.ask) {
        estimate->setProperty("warn", c.ask);
        estimate->style()->unpolish(estimate);
        estimate->style()->polish(estimate);
    }
}

// ── RunRequestForm ───────────────────────────────────────────────────────────

RunRequestForm::RunRequestForm(QWidget* parent, bool for_schedule) : QWidget(parent), d_(std::make_unique<Impl>()) {
    d_->q = this;
    d_->for_schedule = for_schedule;
    d_->build();
    d_->retranslate();
    new rv::RvEventHook(this, [this](QEvent::Type t) {
        if (t == QEvent::LanguageChange)
            d_->retranslate();
    });
    set_template(QString());
}

RunRequestForm::~RunRequestForm() = default;

void RunRequestForm::set_template(const QString& template_id) {
    // The same template again (new context chips): its parameters are re-read, but
    // the engine, budget and an edited text stay as the user set them.
    const bool switching = !d_->initialized || template_id != d_->template_id;
    d_->initialized = true;
    if (!d_->template_id.isEmpty() || d_->tmpl)
        d_->remembered.insert(d_->template_id, d_->context());
    d_->template_id = template_id;
    d_->tmpl = agt::RunTemplateLibrary::instance().find(template_id);
    if (!d_->tmpl)
        d_->template_id.clear();
    d_->building = true;
    if (switching) {
        const QSignalBlocker block(d_->edit_btn);
        d_->edit_btn->setChecked(false);
    }
    d_->edit_btn->setVisible(d_->tmpl.has_value());
    if (!switching) {
        // keep engine / mode / budget / text
    } else if (d_->tmpl) {
        const agt::RunTemplate& t = *d_->tmpl;
        d_->title->setText(t.title);
        d_->description->setText(t.description);
        d_->expected->setText(t.expected_artefacts.isEmpty()
                                  ? QString()
                                  : tr("Produces: %1").arg(t.expected_artefacts.join(QStringLiteral(" · "))));
        d_->expected->setVisible(!t.expected_artefacts.isEmpty());
        // The template's engine, mode and budget.
        d_->engine->setCurrentIndex(d_->engine->findData(engine_value(t.engine, t.plan_first)));
        d_->mode->setCurrentIndex(std::max(0, d_->mode->findData(t.mode)));
        d_->credits->setValue(t.budget.max_credits);
        d_->steps->setValue(t.budget.max_steps);
        d_->minutes->setValue(t.budget.max_minutes);
        d_->text->setPlaceholderText(QString());
    } else {
        d_->title->setText(tr("Custom task"));
        d_->description->setText(tr("Describe a task in your own words. As a multi-step task the agent plans it, works "
                                    "through the steps and writes a final report."));
        d_->expected->setVisible(false);
        d_->engine->setCurrentIndex(d_->engine->findData(kRrfCloudPlan));
        d_->mode->setCurrentIndex(d_->mode->findData(QStringLiteral("deep")));
        d_->credits->setValue(agt::kDefaultMaxCredits);
        d_->steps->setValue(agt::kDefaultMaxSteps);
        d_->minutes->setValue(0);
        d_->text->clear();
        d_->text->setPlaceholderText(tr("e.g. Compare the last four quarters of NVDA and AMD data-centre revenue, "
                                        "margins and guidance; end with which one has more upside and why."));
    }
    d_->building = false;
    d_->rebuild_params();
    // This template's values from earlier in the session, then the extra context.
    QJsonObject ctx = d_->tmpl ? agt::template_default_context(*d_->tmpl) : QJsonObject{};
    const QJsonObject before = d_->remembered.value(d_->template_id);
    for (auto it = before.constBegin(); it != before.constEnd(); ++it)
        ctx.insert(it.key(), it.value());
    const QJsonArray syms = d_->extra.value(QStringLiteral("symbols")).toArray();
    if (!ctx.contains(QStringLiteral("symbol")) || ctx.value(QStringLiteral("symbol")).toString().isEmpty())
        if (!syms.isEmpty())
            ctx.insert(QStringLiteral("symbol"), syms.first().toString());
    for (const char* k : {"portfolio_id", "portfolio_name"})
        if (d_->extra.contains(QLatin1String(k)) && ctx.value(QLatin1String(k)).toString().isEmpty())
            ctx.insert(QLatin1String(k), d_->extra.value(QLatin1String(k)));
    d_->building = true;
    for (const Impl::Field& f : std::as_const(d_->fields))
        if (f.read)
            f.read(ctx);
    d_->building = false;
    d_->update_engine_state();
    d_->changed();
}

QString RunRequestForm::template_id() const {
    return d_->template_id;
}

std::optional<agt::RunTemplate> RunRequestForm::current_template() const {
    return d_->tmpl;
}

QJsonObject RunRequestForm::values() const {
    QJsonObject o;
    o[QStringLiteral("template_id")] = d_->template_id;
    o[QStringLiteral("params")] = d_->context();
    const bool edited = !d_->tmpl || d_->edit_btn->isChecked();
    o[QStringLiteral("prompt_edited")] = edited && d_->tmpl.has_value();
    if (edited)
        o[QStringLiteral("prompt")] = d_->text->toPlainText();
    o[QStringLiteral("engine")] = d_->engine->currentData().toInt();
    o[QStringLiteral("mode")] = d_->mode->currentData().toString();
    QJsonObject b;
    b[QStringLiteral("max_credits")] = d_->credits->value();
    b[QStringLiteral("max_steps")] = d_->steps->value();
    b[QStringLiteral("max_minutes")] = d_->minutes->value();
    o[QStringLiteral("budget")] = b;
    return o;
}

void RunRequestForm::set_values(const QJsonObject& v) {
    const QString id = v.value(QStringLiteral("template_id")).toString();
    d_->remembered.insert(id, v.value(QStringLiteral("params")).toObject());
    set_template(id);
    d_->building = true;
    int wanted = v.value(QStringLiteral("engine")).toInt(kRrfCloudPlan);
    if (wanted == kRrfHarness && !d_->harness.listed)
        wanted = kRrfCloudPlan; // remembered from a build that had the harness engine
    const int engine_at = d_->engine->findData(wanted);
    if (engine_at >= 0)
        d_->engine->setCurrentIndex(engine_at);
    const int mode_at = d_->mode->findData(v.value(QStringLiteral("mode")).toString());
    if (mode_at >= 0)
        d_->mode->setCurrentIndex(mode_at);
    const QJsonObject b = v.value(QStringLiteral("budget")).toObject();
    if (!b.isEmpty()) {
        d_->credits->setValue(b.value(QStringLiteral("max_credits")).toDouble());
        d_->steps->setValue(b.value(QStringLiteral("max_steps")).toInt());
        d_->minutes->setValue(b.value(QStringLiteral("max_minutes")).toInt());
    }
    const QString prompt = v.value(QStringLiteral("prompt")).toString();
    if (!d_->tmpl) {
        d_->text->setPlainText(prompt);
    } else if (v.value(QStringLiteral("prompt_edited")).toBool() && !prompt.isEmpty()) {
        d_->edit_btn->setChecked(true);
        d_->text->setReadOnly(false);
        d_->text->setPlainText(prompt);
    }
    d_->building = false;
    d_->update_engine_state();
    d_->changed();
}

void RunRequestForm::set_extra_context(const QJsonObject& context) {
    d_->extra = context;
    // Re-apply to the template shown (fills an empty symbol / portfolio).
    set_template(d_->template_id);
}

void RunRequestForm::set_custom_prompt(const QString& prompt) {
    set_template(QString());
    d_->text->setPlainText(prompt);
}

QStringList RunRequestForm::problems() const {
    QStringList out;
    const QString exec = executor_id();
    QString why;
    if (exec == QLatin1String(rv::kHarnessEngineId)) {
        if (!d_->harness.enabled)
            out << tr("No agent harness may run this task now: %1").arg(d_->harness.reason);
    } else if (!rv::executor_available(exec, &why)) {
        out << why;
    }
    if (d_->tmpl && !d_->edit_btn->isChecked()) {
        out << agt::validate_template_params(*d_->tmpl, d_->context());
    } else {
        const QString t = d_->text->toPlainText().trimmed();
        if (t.isEmpty())
            out << tr("Write the task the agent should do.");
        else if (t.size() > kRrfMaxPromptChars)
            out << tr("The task text is %1 characters long; at most %2.").arg(t.size()).arg(kRrfMaxPromptChars);
        else if (!agt::run_template_placeholders(t).isEmpty() && d_->tmpl)
            out << agt::validate_template_params(*d_->tmpl, d_->context()); // its placeholders still need values
    }
    if (exec == QLatin1String(agt::kExecutorCloudPlan) && d_->credits->value() > 0 &&
        d_->credits->value() + 1e-9 < agt::estimate_cloud_plan_credits(1))
        out << tr("A multi-step task needs at least %1 credits (a plan, one step and the report). Raise the credit "
                  "limit or choose one reply.")
                   .arg(rv::format_credits(agt::estimate_cloud_plan_credits(1)));
    if (exec == QLatin1String(agt::kExecutorCloudTurn) && d_->credits->value() > 0 &&
        d_->credits->value() + 1e-9 < agt::kCloudTurnCredits)
        out << tr("One reply costs about %1 credits: raise the credit limit.").arg(rv::format_credits(agt::kCloudTurnCredits));
    out.removeDuplicates();
    return out;
}

agt::RunRequest RunRequestForm::request() const {
    agt::RunRequest r;
    const QJsonObject ctx = d_->context();
    if (d_->tmpl && !d_->edit_btn->isChecked()) {
        r = agt::request_from_template(*d_->tmpl, ctx);
    } else {
        r.prompt = d_->text->toPlainText().trimmed();
        r.template_id = d_->template_id;
        r.title = d_->tmpl ? d_->tmpl->title : QString(); // a custom task is titled after its first line
        QJsonObject c;
        for (auto it = ctx.constBegin(); it != ctx.constEnd(); ++it)
            if (!(it.value().isString() && it.value().toString().isEmpty()))
                c.insert(it.key(), it.value());
        r.context = c;
    }
    const int choice = d_->engine->currentData().toInt();
    r.engine = QString::fromLatin1(choice == kRrfLocal     ? agt::kEngineLocal
                                   : choice == kRrfHarness ? rv::kHarnessEngineId
                                                           : agt::kEngineCloud);
    r.plan_first = choice != kRrfCloudTurn;
    r.mode = d_->mode->currentData().toString();
    r.budget = d_->budget();
    if (rrf_no_credits(choice))
        r.budget.max_credits = 0;
    if (choice == kRrfCloudTurn)
        r.budget.max_steps = 0;
    r.origin = QString::fromLatin1(agt::kOriginUser);
    // Context chips that are not parameters (the investment profile) go along.
    for (const char* k : {"profile_id", "profile_name"})
        if (d_->extra.contains(QLatin1String(k)) && !r.context.contains(QLatin1String(k)))
            r.context.insert(QLatin1String(k), d_->extra.value(QLatin1String(k)));
    r.profile_id = d_->extra.value(QStringLiteral("profile_id")).toInt();
    return r;
}

QString RunRequestForm::refill_template_id() const {
    return d_->tmpl && !d_->edit_btn->isChecked() ? d_->template_id : QString();
}

QString RunRequestForm::edited_prompt() const {
    return !d_->tmpl || d_->edit_btn->isChecked() ? d_->text->toPlainText().trimmed() : QString();
}

int RunRequestForm::engine_value(const QString& engine, bool plan_first) {
    if (engine == QLatin1String(agt::kEngineLocal))
        return kRrfLocal;
    if (engine == QLatin1String(rv::kHarnessEngineId))
        return kRrfHarness;
    return plan_first ? kRrfCloudPlan : kRrfCloudTurn;
}

QString RunRequestForm::executor_id() const {
    return rrf_executor(d_->engine->currentData().toInt());
}

rv::BudgetConfirmation RunRequestForm::confirmation(double balance) const {
    return rv::budget_confirmation(executor_id(), d_->budget(), agt::run_plan_prompts::kPlanHardMaxSteps, balance);
}

void RunRequestForm::set_balance(double credits) {
    d_->balance = credits;
    d_->update_estimate();
}

} // namespace fincept::chat_mode
