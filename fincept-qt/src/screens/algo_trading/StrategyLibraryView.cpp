// src/screens/algo_trading/StrategyLibraryView.cpp
#include "screens/algo_trading/StrategyLibraryView.h"

#include "core/agent/AgentRiskMarkers.h"
#include "screens/algo_trading/AlgoTabKit.h"
#include "services/algo_trading/AlgoRuleLogic.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/LiveTableModel.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/FlowLayout.h"
#include "ui/widgets/KeySortProxy.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/PanelHeader.h"
#include "ui/widgets/ScreenCommandBar.h"
#include "ui/widgets/ScreenStatusBar.h"
#include "ui/widgets/StatTile.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableView>
#include <QTimeZone>
#include <QVBoxLayout>

#include <cmath>
#include <limits>

namespace fincept::screens {

namespace kit = fincept::screens::algo_kit;
namespace fmt = fincept::ui::formatting;
namespace rules = fincept::services::algo::rules;
using fincept::services::algo::AlgoStrategy;

namespace {

constexpr double kSlvNaN = std::numeric_limits<double>::quiet_NaN();

enum SlvCol {
    kSlvName,
    kSlvCategory,
    kSlvInstrument,
    kSlvTf,
    kSlvRules,
    kSlvReturn,
    kSlvSharpe,
    kSlvMaxDd,
    kSlvAsOf,
    kSlvStatus,
    kSlvUpdated,
    kSlvCount
};

enum SlvPage { kSlvPageTable = 0, kSlvPageState = 1, kSlvPageLoading = 2 };

QString slv_tr(const char* text, const char* disambiguation = nullptr, int n = -1) {
    return QCoreApplication::translate("fincept::screens::StrategyLibraryView", text, disambiguation, n);
}

/// CATEGORY: library strategies keep their category in `description`; a user's strategy
/// keeps a real description there, so it is "Custom".
QString slv_category(const AlgoStrategy& s) {
    if (s.id.startsWith(QLatin1String("LIB-")))
        return s.description;
    return slv_tr("Custom");
}

double slv_num(const QJsonObject& o, const char* key) {
    const QJsonValue v = o.value(QLatin1String(key));
    return v.isDouble() ? v.toDouble() : kSlvNaN;
}

/// "2026-11-17 10:20:11" (SQLite CURRENT_TIMESTAMP, UTC) or ISO 8601 → local time.
QDateTime slv_parse_time(const QString& text) {
    QString t = text.trimmed();
    if (t.isEmpty())
        return {};
    if (t.size() > 10 && t.at(10) == QLatin1Char(' '))
        t[10] = QLatin1Char('T');
    QDateTime d = QDateTime::fromString(t, Qt::ISODate);
    if (!d.isValid())
        return {};
    if (d.timeSpec() == Qt::LocalTime && !t.endsWith(QLatin1Char('Z')) && !t.contains(QLatin1Char('+')))
        d.setTimeZone(QTimeZone::UTC);
    return d.toLocalTime();
}

QString slv_date(const QDateTime& d) {
    return d.isValid() ? fmt::calendar_date(d.date(), fmt::DateStyle::Short) : kit::dash();
}

} // namespace

struct StrategyLibraryView::Row {
    AlgoStrategy s;
    QString category;
    QString instrument;
    QString instrument_tip;
    QString tf;
    QString rules;
    QString rules_tip;
    double ret = kSlvNaN;
    double sharpe = kSlvNaN;
    double mdd = kSlvNaN;
    bool net = true;
    QDateTime as_of;
    QDateTime updated;
    QString status;
    QString status_tip;
    kit::Tone status_tone = kit::Tone::Dim;
    int status_rank = 0; ///< sort: live 3 · running 2 · stopped 1 · none 0
};

StrategyLibraryView::StrategyLibraryView(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("algoStrategiesView"));
    build_ui();
    install_shortcuts();
    retranslate();
    show_loading();
}

StrategyLibraryView::~StrategyLibraryView() = default;

// ── Layout ──────────────────────────────────────────────────────────────────

void StrategyLibraryView::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    using Role = ui::ScreenCommandBar::Role;
    bar_ = kit::make_tab_bar(this, kit::TabStrategies);
    bar_->add_action(QStringLiteral("open"), QString(), QStringLiteral("↵"), QString(), Role::Primary, 100);
    bar_->add_action(QStringLiteral("backtest"), QString(), QStringLiteral("B"), QString(), Role::Normal, 80);
    bar_->add_action(QStringLiteral("deploy"), QString(), QStringLiteral("D"), QString(), Role::Normal, 70);
    bar_->add_separator(60);
    bar_->add_action(QStringLiteral("duplicate"), QString(), QStringLiteral("Ctrl+D"), QString(), Role::Normal, 45);
    bar_->add_action(QStringLiteral("export"), QString(), QStringLiteral("Ctrl+E"), QString(), Role::Normal, 40);
    bar_->add_action(QStringLiteral("delete"), QString(), QStringLiteral("Del"), QString(), Role::Danger, 30);
    // Deletes the user's strategy (after a confirmation): marked like the detail button.
    if (QPushButton* del = bar_->action(QStringLiteral("delete")))
        del->setProperty("fincept_agent_risk", "delete");
    connect(bar_, &ui::ScreenCommandBar::action_triggered, this, [this](const QString& id) { trigger(id); });
    root->addWidget(bar_);

    // ── Strip: search + filters ──
    ui::FlowLayout* flow = nullptr;
    QWidget* strip = kit::make_strip(this, &flow);
    search_ = new QLineEdit(strip);
    search_->setClearButtonEnabled(true);
    search_->setMinimumWidth(QFontMetrics(search_->font()).averageCharWidth() * 26);
    category_ = new QComboBox(strip);
    category_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    show_ = new QComboBox(strip);
    show_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    for (int i = 0; i < 4; ++i)
        show_->addItem(QString(), i);
    count_ = new QLabel(strip);
    count_->setObjectName(QStringLiteral("algoNote"));
    flow->addWidget(kit::field_group(strip, slv_tr("SEARCH"), {search_}));
    flow->addWidget(kit::field_group(strip, slv_tr("CATEGORY"), {category_}));
    flow->addWidget(kit::field_group(strip, slv_tr("SHOW"), {show_}));
    flow->addWidget(count_);
    flow->setAlignment(count_, Qt::AlignVCenter);
    root->addWidget(strip);

    // ── KPI ribbon ──
    auto* ribbon_box = new QWidget(this);
    ribbon_box->setObjectName(QStringLiteral("algoRibbon"));
    ribbon_box->setAttribute(Qt::WA_StyledBackground, true);
    ribbon_box->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto* rh = new QHBoxLayout(ribbon_box);
    rh->setContentsMargins(6, 4, 6, 4);
    ribbon_ = new ui::KpiStrip(ribbon_box);
    ribbon_->set_spacing(6);
    ribbon_->set_max_columns(4);
    for (const char* key : {"count", "tested", "running", "best"}) {
        ui::StatTile* t = ribbon_->add_tile(QLatin1String(key), QString());
        t->set_compact(true);
    }
    ribbon_->tile(QStringLiteral("best"))->set_sparkline_color(ui::StatTile::SparkColor::Direction);
    rh->addWidget(ribbon_);
    root->addWidget(ribbon_box);

    // ── Panels ──
    auto* split = kit::make_splitter(Qt::Horizontal, this);

    kit::Panel list = kit::make_panel(QString(), split, 0);
    list_header_ = list.header;
    stack_ = new QStackedWidget(list.body_widget);
    table_ = new QTableView(stack_);
    model_ = new ui::LiveTableModel<Row>(table_);
    model_->set_key([](const Row& r) { return r.s.id; });
    model_->set_cell([](const Row& r, int col, int role) -> QVariant {
        using Model = ui::LiveTableModel<Row>;
        const bool numeric = col == kSlvReturn || col == kSlvSharpe || col == kSlvMaxDd;
        if (role == Qt::TextAlignmentRole)
            return numeric ? Qt::Alignment(Qt::AlignRight | Qt::AlignVCenter).toInt()
                           : Qt::Alignment(Qt::AlignLeft | Qt::AlignVCenter).toInt();
        if (role == Qt::DisplayRole) {
            switch (col) {
                case kSlvName:
                    return r.s.name;
                case kSlvCategory:
                    return r.category;
                case kSlvInstrument:
                    return r.instrument;
                case kSlvTf:
                    return r.tf;
                case kSlvRules:
                    return r.rules;
                case kSlvReturn:
                    return std::isfinite(r.ret) ? fmt::pct(r.ret, 1, true) + (r.net ? QString() : slv_tr(" gross"))
                                                : slv_tr("not run");
                case kSlvSharpe:
                    return std::isfinite(r.sharpe) ? fmt::number(r.sharpe, 2) : kit::dash();
                case kSlvMaxDd:
                    return std::isfinite(r.mdd) ? fmt::pct(-std::abs(r.mdd), 1, true) : kit::dash();
                case kSlvAsOf:
                    return slv_date(r.as_of);
                case kSlvStatus:
                    return r.status;
                case kSlvUpdated:
                    return slv_date(r.updated);
                default:
                    break;
            }
            return {};
        }
        if (role == Model::SortRole) {
            switch (col) {
                case kSlvReturn:
                    return r.ret;
                case kSlvSharpe:
                    return r.sharpe;
                case kSlvMaxDd:
                    return std::isfinite(r.mdd) ? QVariant(std::abs(r.mdd)) : QVariant(kSlvNaN);
                case kSlvAsOf:
                    return r.as_of;
                case kSlvUpdated:
                    return r.updated;
                case kSlvStatus:
                    return r.status_rank > 0 ? QVariant(r.status_rank) : QVariant();
                case kSlvName:
                    return r.s.name;
                case kSlvCategory:
                    return r.category;
                case kSlvInstrument:
                    return r.instrument == kit::dash() ? QString() : r.instrument;
                case kSlvTf:
                    return r.tf;
                case kSlvRules:
                    return r.rules;
                default:
                    break;
            }
            return {};
        }
        if (role == Qt::ForegroundRole) {
            if (col == kSlvReturn)
                return std::isfinite(r.ret) ? QVariant(kit::tone_color(kit::sign_tone(r.ret)))
                                            : QVariant(kit::tone_color(kit::Tone::Dim));
            if (col == kSlvStatus)
                return kit::tone_color(r.status_tone);
            if ((col == kSlvAsOf || col == kSlvSharpe || col == kSlvMaxDd) && !std::isfinite(r.ret))
                return kit::tone_color(kit::Tone::Dim);
            return {};
        }
        if (role == Qt::ToolTipRole) {
            switch (col) {
                case kSlvName:
                    return r.s.description.isEmpty() ? r.s.id : r.s.description + QStringLiteral("\n") + r.s.id;
                case kSlvRules:
                    return r.rules_tip;
                case kSlvInstrument:
                    return r.instrument_tip;
                case kSlvStatus:
                    return r.status_tip;
                case kSlvReturn:
                    return r.net ? slv_tr("Net of costs, last backtest") : slv_tr("GROSS — that backtest applied no costs");
                case kSlvTf:
                    return r.s.entry_side == QLatin1String("SELL") ? slv_tr("Short entries") : slv_tr("Long entries");
                default:
                    break;
            }
            return {};
        }
        return {};
    });
    proxy_ = new ui::KeySortProxy(table_);
    proxy_->setSourceModel(model_);
    proxy_->setSortRole(ui::LiveTableModel<Row>::SortRole);
    proxy_->set_filter_columns({kSlvName, kSlvCategory, kSlvInstrument, kSlvRules});
    proxy_->set_row_filter([this](int source_row, const QModelIndex&) {
        if (source_row < 0 || source_row >= model_->rows().size())
            return true;
        const Row& r = model_->rows().at(source_row);
        if (category_->currentIndex() > 0 && r.category != category_->currentText())
            return false;
        switch (show_->currentData().toInt()) {
            case 1:
                return r.status_rank >= 2; // running
            case 2:
                return std::isfinite(r.ret); // backtested
            case 3:
                return !std::isfinite(r.ret); // not backtested
            default:
                break;
        }
        return true;
    });
    table_->setModel(proxy_);
    kit::setup_table(table_, QStringLiteral("algo_strategies"));
    kit::set_column_chars(table_, {22, 12, 9, 5, 34, 9, 6, 8, 11, 12, 11});
    table_->sortByColumn(kSlvName, Qt::AscendingOrder);
    stack_->addWidget(table_);

    state_ = new ui::EmptyState(stack_);
    connect(state_, &ui::EmptyState::primary_clicked, this, [this]() {
        if (state_kind_ == QLatin1String("error"))
            emit retry_requested();
        else if (state_kind_ == QLatin1String("empty"))
            emit builder_requested();
        else if (state_kind_ == QLatin1String("nomatch")) {
            search_->clear();
            category_->setCurrentIndex(0);
            show_->setCurrentIndex(0);
        }
    });
    stack_->addWidget(state_);
    skeleton_ = new ui::LoadingSkeleton(ui::LoadingSkeleton::Shape::Table, stack_);
    skeleton_->set_row_count(12);
    stack_->addWidget(skeleton_);
    list.body->addWidget(stack_, 1);

    // ── Detail ──
    kit::Panel detail = kit::make_panel(QString(), split, 0);
    detail_header_ = detail.header;
    auto* scroll = new QScrollArea(detail.body_widget);
    scroll->setObjectName(QStringLiteral("algoScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    detail_body_ = new QWidget(scroll);
    detail_body_->setObjectName(QStringLiteral("algoDetail"));
    detail_body_->setAttribute(Qt::WA_StyledBackground, true);
    auto* dv = new QVBoxLayout(detail_body_);
    dv->setContentsMargins(10, 8, 10, 10);
    dv->setSpacing(6);
    detail_empty_ = kit::make_note(QString(), detail_body_);
    detail_empty_->setAlignment(Qt::AlignCenter);
    dv->addWidget(detail_empty_);

    detail_content_ = new QWidget(detail_body_);
    auto* cv = new QVBoxLayout(detail_content_);
    cv->setContentsMargins(0, 0, 0, 0);
    cv->setSpacing(6);
    d_name_ = new QLabel(detail_content_);
    d_name_->setObjectName(QStringLiteral("algoValue"));
    d_name_->setWordWrap(true);
    d_name_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    cv->addWidget(d_name_);
    d_meta_ = kit::make_note(QString(), detail_content_);
    cv->addWidget(d_meta_);

    auto* actions = new QWidget(detail_content_);
    auto* af = new ui::FlowLayout(actions);
    d_open_ = kit::make_primary_button(QString(), QString(), actions);
    d_backtest_ = kit::make_tool_button(QString(), QString(), actions);
    d_deploy_ = kit::make_tool_button(QString(), QString(), actions);
    d_duplicate_ = kit::make_tool_button(QString(), QString(), actions);
    d_export_ = kit::make_tool_button(QString(), QString(), actions);
    d_delete_ = kit::make_tool_button(QString(), QString(), actions);
    kit::set_tone(d_delete_, kit::Tone::Danger);
    d_delete_->setProperty("fincept_agent_risk", "delete");
    for (QPushButton* b : {d_open_, d_backtest_, d_deploy_, d_duplicate_, d_export_, d_delete_})
        af->addWidget(b);
    connect(d_open_, &QPushButton::clicked, this, [this]() { trigger(QStringLiteral("open")); });
    connect(d_backtest_, &QPushButton::clicked, this, [this]() { trigger(QStringLiteral("backtest")); });
    connect(d_deploy_, &QPushButton::clicked, this, [this]() { trigger(QStringLiteral("deploy")); });
    connect(d_duplicate_, &QPushButton::clicked, this, [this]() { trigger(QStringLiteral("duplicate")); });
    connect(d_export_, &QPushButton::clicked, this, [this]() { trigger(QStringLiteral("export")); });
    connect(d_delete_, &QPushButton::clicked, this, [this]() { trigger(QStringLiteral("delete")); });
    cv->addWidget(actions);

    auto caption = [](QWidget* parent) {
        auto* l = new QLabel(parent);
        l->setObjectName(QStringLiteral("algoFieldLabel"));
        return l;
    };
    auto sentence = [](QWidget* parent) {
        auto* l = new QLabel(parent);
        l->setObjectName(QStringLiteral("algoSentence"));
        l->setWordWrap(true);
        l->setTextInteractionFlags(Qt::TextSelectableByMouse);
        return l;
    };
    auto* rule1 = new QFrame(detail_content_);
    rule1->setObjectName(QStringLiteral("algoHairline"));
    cv->addWidget(rule1);
    d_entry_cap_ = caption(detail_content_);
    cv->addWidget(d_entry_cap_);
    d_entry_ = sentence(detail_content_);
    cv->addWidget(d_entry_);
    d_exit_cap_ = caption(detail_content_);
    cv->addWidget(d_exit_cap_);
    d_exit_ = sentence(detail_content_);
    cv->addWidget(d_exit_);
    d_risk_cap_ = caption(detail_content_);
    cv->addWidget(d_risk_cap_);
    d_risk_ = kit::make_note(QString(), detail_content_);
    cv->addWidget(d_risk_);
    auto* rule2 = new QFrame(detail_content_);
    rule2->setObjectName(QStringLiteral("algoHairline"));
    cv->addWidget(rule2);
    d_bt_caption_ = caption(detail_content_);
    cv->addWidget(d_bt_caption_);
    d_bt_note_ = kit::make_note(QString(), detail_content_);
    cv->addWidget(d_bt_note_);
    d_tiles_ = new ui::KpiStrip(detail_content_);
    d_tiles_->set_spacing(4);
    d_tiles_->set_max_columns(4);
    for (const char* key : {"ret", "sharpe", "mdd", "trades"}) {
        ui::StatTile* t = d_tiles_->add_tile(QLatin1String(key), QString());
        t->set_compact(true);
    }
    d_tiles_->tile(QStringLiteral("mdd"))->set_sparkline_color(ui::StatTile::SparkColor::Neutral);
    cv->addWidget(d_tiles_);
    d_dep_cap_ = caption(detail_content_);
    cv->addWidget(d_dep_cap_);
    d_dep_ = kit::make_note(QString(), detail_content_);
    cv->addWidget(d_dep_);
    dv->addWidget(detail_content_);
    dv->addStretch(1);
    scroll->setWidget(detail_body_);
    detail.body->addWidget(scroll, 1);

    split->addWidget(list.frame);
    split->addWidget(detail.frame);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    root->addWidget(split, 1);

    status_ = kit::make_status_bar(this);
    root->addWidget(status_);

    // ── Wiring ──
    connect(search_, &QLineEdit::textChanged, this, [this](const QString& text) {
        proxy_->set_filter_text(text);
        update_page();
    });
    connect(category_, &QComboBox::currentIndexChanged, this, [this](int) { refilter(); });
    connect(show_, &QComboBox::currentIndexChanged, this, [this](int) { refilter(); });
    connect(table_->selectionModel(), &QItemSelectionModel::currentRowChanged, this, [this]() {
        update_detail();
        update_actions();
    });
    connect(table_, &QTableView::doubleClicked, this, [this](const QModelIndex&) { trigger(QStringLiteral("open")); });
    // The proxy re-filters / re-sorts: the selection may have gone.
    connect(proxy_, &QAbstractItemModel::modelReset, this, [this]() {
        update_detail();
        update_actions();
    });
}

void StrategyLibraryView::install_shortcuts() {
    auto add = [this](QWidget* scope, const QKeySequence& seq, Qt::ShortcutContext ctx, const QString& action) {
        auto* sc = new QShortcut(seq, scope);
        sc->setContext(ctx);
        connect(sc, &QShortcut::activated, this, [this, action]() { trigger(action); });
        return sc;
    };
    // Letters / Enter / Delete act on the table's row only while the table has the focus
    // (typing in SEARCH stays typing).
    add(table_, QKeySequence(Qt::Key_Return), Qt::WidgetShortcut, QStringLiteral("open"));
    add(table_, QKeySequence(Qt::Key_Enter), Qt::WidgetShortcut, QStringLiteral("open"));
    add(table_, QKeySequence(Qt::Key_B), Qt::WidgetShortcut, QStringLiteral("backtest"));
    add(table_, QKeySequence(Qt::Key_D), Qt::WidgetShortcut, QStringLiteral("deploy"));
    QShortcut* del = add(table_, QKeySequence(Qt::Key_Delete), Qt::WidgetShortcut, QStringLiteral("delete"));
    del->setProperty(fincept::core::agent::kAgentRiskProperty, "delete");
    add(this, QKeySequence(Qt::CTRL | Qt::Key_D), Qt::WidgetWithChildrenShortcut, QStringLiteral("duplicate"));
    add(this, QKeySequence(Qt::CTRL | Qt::Key_E), Qt::WidgetWithChildrenShortcut, QStringLiteral("export"));
}

void StrategyLibraryView::retranslate() {
    bar_->set_action_text(QStringLiteral("open"), slv_tr("OPEN"));
    bar_->set_action_tooltip(QStringLiteral("open"), slv_tr("Open the selected strategy in the Builder (Enter)"));
    bar_->set_action_text(QStringLiteral("backtest"), slv_tr("BACKTEST…"));
    bar_->set_action_tooltip(QStringLiteral("backtest"),
                             slv_tr("Pick a symbol and a period, then backtest in the Builder (B)"));
    bar_->set_action_text(QStringLiteral("deploy"), slv_tr("DEPLOY (PAPER)…"));
    bar_->set_action_tooltip(QStringLiteral("deploy"),
                             slv_tr("Open the deploy dialog — paper by default; nothing starts until you confirm (D)"));
    bar_->set_action_text(QStringLiteral("duplicate"), slv_tr("DUPLICATE"));
    bar_->set_action_tooltip(QStringLiteral("duplicate"), slv_tr("Save a copy of the selected strategy (Ctrl+D)"));
    bar_->set_action_text(QStringLiteral("export"), slv_tr("EXPORT…"));
    bar_->set_action_tooltip(QStringLiteral("export"), slv_tr("Save the strategy as a JSON file (Ctrl+E)"));
    bar_->set_action_text(QStringLiteral("delete"), slv_tr("DELETE…"));
    bar_->set_action_tooltip(QStringLiteral("delete"), slv_tr("Delete the selected strategy, after a confirmation (Del)"));

    search_->setPlaceholderText(slv_tr("Name, rule, symbol or category"));
    kit::mark_field(search_, slv_tr("Search strategies"), slv_tr("Filters by name, rules, instrument and category"));
    kit::mark_field(category_, slv_tr("Category"));
    kit::mark_field(show_, slv_tr("Show"), slv_tr("Only running, backtested or never backtested strategies"));
    const QStringList shows = {slv_tr("All"), slv_tr("Running"), slv_tr("Backtested"), slv_tr("Not backtested")};
    for (int i = 0; i < shows.size() && i < show_->count(); ++i)
        show_->setItemText(i, shows[i]);
    if (category_->count() > 0)
        category_->setItemText(0, slv_tr("All"));

    ribbon_->tile(QStringLiteral("count"))->set_label(slv_tr("STRATEGIES"));
    ribbon_->tile(QStringLiteral("tested"))->set_label(slv_tr("BACKTESTED"));
    ribbon_->tile(QStringLiteral("running"))->set_label(slv_tr("RUNNING"));
    ribbon_->tile(QStringLiteral("best"))->set_label(slv_tr("BEST LAST RETURN"));
    ribbon_->tile(QStringLiteral("best"))
        ->set_tooltip_text(slv_tr("Highest net return among the strategies' last backtests (different symbols and "
                                  "periods — a pointer, not a ranking)"));

    list_header_->set_title(slv_tr("Strategies"));
    detail_header_->set_title(slv_tr("Detail"));
    detail_empty_->setText(slv_tr("Select a strategy to see its rules in plain language, its last backtest and its "
                                  "deployments."));
    d_open_->setText(slv_tr("OPEN"));
    d_open_->setToolTip(slv_tr("Open in the Builder (Enter)"));
    d_backtest_->setText(slv_tr("BACKTEST…"));
    d_backtest_->setToolTip(slv_tr("Pick a symbol and a period, then backtest in the Builder"));
    d_deploy_->setText(slv_tr("DEPLOY (PAPER)…"));
    d_deploy_->setToolTip(slv_tr("Open the deploy dialog — nothing starts until you confirm it"));
    d_duplicate_->setText(slv_tr("DUPLICATE"));
    d_duplicate_->setToolTip(slv_tr("Save a copy named “… (copy)”"));
    d_export_->setText(slv_tr("EXPORT…"));
    d_export_->setToolTip(slv_tr("Save as a JSON file"));
    d_delete_->setText(slv_tr("DELETE…"));
    d_delete_->setToolTip(slv_tr("Delete this strategy (asks first)"));
    for (QPushButton* b : {d_open_, d_backtest_, d_deploy_, d_duplicate_, d_export_, d_delete_}) {
        QString name = b->text();
        name.remove(QStringLiteral("…"));
        b->setAccessibleName(name);
    }
    d_entry_cap_->setText(slv_tr("ENTRY"));
    d_exit_cap_->setText(slv_tr("EXIT"));
    d_risk_cap_->setText(slv_tr("RISK & COSTS"));
    d_bt_caption_->setText(slv_tr("LAST BACKTEST"));
    d_dep_cap_->setText(slv_tr("DEPLOYMENTS"));
    d_tiles_->tile(QStringLiteral("ret"))->set_label(slv_tr("NET RETURN"));
    d_tiles_->tile(QStringLiteral("sharpe"))->set_label(slv_tr("SHARPE"));
    d_tiles_->tile(QStringLiteral("mdd"))->set_label(slv_tr("MAX DD"));
    d_tiles_->tile(QStringLiteral("trades"))->set_label(slv_tr("TRADES"));

    model_->set_headers({slv_tr("NAME"), slv_tr("CATEGORY"), slv_tr("INSTRUMENT"), slv_tr("TF"), slv_tr("ENTRY RULE"),
                         slv_tr("RETURN"), slv_tr("SHARPE"), slv_tr("MAX DD"), slv_tr("AS OF"), slv_tr("STATUS"),
                         slv_tr("UPDATED")});
    status_->set_text(QStringLiteral("data"), slv_tr("LIBRARY · this computer"));
    status_->set_tooltip(QStringLiteral("data"), slv_tr("Saved strategies and the curated library, stored locally"));
}

void StrategyLibraryView::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslate();
        refresh_text();
    }
    QWidget::changeEvent(event);
}

void StrategyLibraryView::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    status_->start_clock();
}

void StrategyLibraryView::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    status_->stop_clock();
}

// ── Data ────────────────────────────────────────────────────────────────────

void StrategyLibraryView::set_strategies(const QVector<AlgoStrategy>& strategies) {
    strategies_ = strategies;
    loaded_ = true;
    state_kind_.clear();
    fill_categories();
    rebuild_rows();
    status_->set_text(QStringLiteral("asof"),
                      slv_tr("loaded %1").arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm"))));
    kit::set_state(status_, kit::RunState::Ready);
}

void StrategyLibraryView::set_deployments(const QHash<QString, Deployments>& by_strategy_id) {
    deployments_ = by_strategy_id;
    // The STATUS tooltips print P&L per currency; mixed currencies name every unit.
    fmt::CurrencyMix mix;
    bool unknown = false;
    for (const auto& d : std::as_const(deployments_))
        for (auto p = d.pnl_by_ccy.constBegin(); p != d.pnl_by_ccy.constEnd(); ++p) {
            unknown = unknown || p.key().isEmpty();
            mix.add(p.key());
        }
    live_mixed_ = mix.mixed() || (unknown && !mix.isEmpty());
    if (loaded_)
        rebuild_rows();
}

void StrategyLibraryView::set_last_backtest(const QString& strategy_id, const QJsonObject& summary) {
    for (auto& s : strategies_)
        if (s.id == strategy_id)
            s.last_backtest = summary;
    if (loaded_)
        rebuild_rows();
}

void StrategyLibraryView::show_loading() {
    loaded_ = false;
    stack_->setCurrentIndex(kSlvPageLoading);
    kit::set_state(status_, kit::RunState::Running, slv_tr("LOADING"));
    update_detail();
    update_actions();
}

void StrategyLibraryView::show_error(const QString& message) {
    state_kind_ = QStringLiteral("error");
    state_->apply(ui::EmptyState::error_spec(message, slv_tr("Could not load your strategies")));
    stack_->setCurrentIndex(kSlvPageState);
    kit::set_state(status_, kit::RunState::Failed);
}

void StrategyLibraryView::refresh_text() {
    if (loaded_)
        rebuild_rows();
    update_detail();
}

void StrategyLibraryView::fill_categories() {
    const QString keep = category_->currentIndex() > 0 ? category_->currentText() : QString();
    QStringList cats;
    for (const auto& s : std::as_const(strategies_)) {
        const QString c = slv_category(s);
        if (!c.isEmpty() && !cats.contains(c))
            cats << c;
    }
    cats.sort(Qt::CaseInsensitive);
    QSignalBlocker block(category_);
    category_->clear();
    category_->addItem(slv_tr("All"));
    category_->addItems(cats);
    const int idx = keep.isEmpty() ? 0 : category_->findText(keep);
    category_->setCurrentIndex(idx >= 0 ? idx : 0);
}

void StrategyLibraryView::rebuild_rows() {
    const QString keep = selected_id();
    QVector<Row> rows;
    rows.reserve(strategies_.size());
    for (const auto& s : std::as_const(strategies_)) {
        Row r;
        r.s = s;
        r.category = slv_category(s);
        const QJsonObject& b = s.last_backtest;
        const QString bt_symbol = b.value(QStringLiteral("symbol")).toString();
        if (s.instrument_type == QLatin1String("option")) {
            r.instrument = slv_tr("OPTIONS");
            r.instrument_tip = slv_tr("Option legs on an underlying (deploy from the Builder)");
        } else if (s.instrument_type == QLatin1String("future")) {
            r.instrument = slv_tr("FUTURES");
            r.instrument_tip = slv_tr("Futures legs on an underlying (deploy from the Builder)");
        } else {
            r.instrument = bt_symbol.isEmpty() ? kit::dash() : bt_symbol;
            r.instrument_tip = bt_symbol.isEmpty() ? slv_tr("Not backtested yet — any symbol")
                                                   : slv_tr("Last backtested on %1").arg(bt_symbol);
        }
        r.tf = s.timeframe.toUpper() + (s.entry_side == QLatin1String("SELL") ? QStringLiteral(" ↓") : QString());
        r.rules = rules::rules_sentence(s.entry_conditions, s.entry_logic, rules::Side::Entry);
        r.rules_tip = r.rules + QLatin1Char('\n') + rules::rules_sentence(s.exit_conditions, s.exit_logic, rules::Side::Exit);
        if (b.contains(QStringLiteral("total_return"))) {
            r.ret = slv_num(b, "total_return");
            r.sharpe = slv_num(b, "sharpe_ratio");
            r.mdd = slv_num(b, "max_drawdown");
            r.net = b.value(QStringLiteral("costs_applied")).toBool(true);
            r.as_of = slv_parse_time(b.value(QStringLiteral("run_at")).toString());
            if (!r.as_of.isValid())
                r.as_of = slv_parse_time(s.last_backtest_at);
        }
        r.updated = slv_parse_time(s.updated_at.isEmpty() ? s.created_at : s.updated_at);
        const auto it = deployments_.constFind(s.id);
        if (it == deployments_.constEnd() || it->total == 0) {
            r.status = kit::dash();
            r.status_tone = kit::Tone::Dim;
            r.status_tip = slv_tr("Never deployed");
        } else {
            QStringList pnl;
            for (auto p = it->pnl_by_ccy.constBegin(); p != it->pnl_by_ccy.constEnd(); ++p)
                pnl << fmt::money_in_column(p.value(), p.key(), live_mixed_, fmt::Compact::None, -1, true);
            if (it->live_running > 0) {
                r.status = slv_tr("▶ %1 LIVE").arg(it->live_running);
                if (it->running > it->live_running)
                    r.status += slv_tr(" · %1 PAPER").arg(it->running - it->live_running);
                r.status_tone = kit::Tone::Danger; // real money
                r.status_rank = 3;
            } else if (it->running > 0) {
                r.status = slv_tr("▶ %1 PAPER").arg(it->running);
                r.status_tone = kit::Tone::Ok;
                r.status_rank = 2;
            } else {
                r.status = slv_tr("■ STOPPED");
                r.status_tone = kit::Tone::Dim;
                r.status_rank = 1;
            }
            r.status_tip = slv_tr("%1 deployment(s), %2 running, %3 trades — net P&L (realised + unrealised): %4")
                               .arg(it->total)
                               .arg(it->running)
                               .arg(it->trades)
                               .arg(pnl.isEmpty() ? kit::dash() : pnl.join(QStringLiteral(" / ")));
        }
        rows.append(r);
    }
    model_->upsert(rows);
    proxy_->refilter();
    if (!keep.isEmpty())
        select_id(keep);
    if (!table_->selectionModel()->currentIndex().isValid() && proxy_->rowCount() > 0)
        table_->selectionModel()->setCurrentIndex(proxy_->index(0, 0),
                                                  QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    update_ribbon();
    update_page();
    update_detail();
    update_actions();
}

void StrategyLibraryView::refilter() {
    proxy_->refilter();
    update_page();
}

void StrategyLibraryView::update_page() {
    if (!loaded_)
        return;
    const int shown = proxy_->rowCount();
    const int total = model_->rowCount();
    count_->setText(slv_tr("%1 of %2").arg(shown).arg(total));
    list_header_->set_badge(QString::number(shown));
    if (state_kind_ == QLatin1String("error"))
        return;
    if (total == 0) {
        if (state_kind_ != QLatin1String("empty")) {
            state_kind_ = QStringLiteral("empty");
            auto spec = ui::EmptyState::no_data_spec(
                slv_tr("No strategies yet"),
                slv_tr("Build one in the Builder and press SAVE — it is listed here with its last backtest and its "
                       "deployments."),
                slv_tr("OPEN BUILDER"));
            spec.illustration = true;
            spec.illustration_shape = ui::LoadingSkeleton::Shape::Table;
            state_->apply(spec);
        }
        stack_->setCurrentIndex(kSlvPageState);
    } else if (shown == 0) {
        state_kind_ = QStringLiteral("nomatch");
        const QString text = search_->text().trimmed();
        state_->apply(ui::EmptyState::no_data_spec(
            slv_tr("No strategy matches"),
            text.isEmpty() ? slv_tr("Nothing matches these filters.")
                           : slv_tr("Nothing matches “%1” with these filters.").arg(text),
            slv_tr("CLEAR FILTERS")));
        stack_->setCurrentIndex(kSlvPageState);
    } else {
        state_kind_.clear();
        stack_->setCurrentIndex(kSlvPageTable);
    }
    int tested = 0, running = 0;
    for (const auto& r : model_->rows()) {
        tested += std::isfinite(r.ret) ? 1 : 0;
        running += r.status_rank >= 2 ? 1 : 0;
    }
    status_->set_text(QStringLiteral("info"), slv_tr("%1 strategies · %2 backtested · %3 deployed and running")
                                                   .arg(total)
                                                   .arg(tested)
                                                   .arg(running));
}

void StrategyLibraryView::update_ribbon() {
    const auto& rows = model_->rows();
    int custom = 0, tested = 0, running = 0, live = 0;
    const Row* best = nullptr;
    for (const auto& r : rows) {
        custom += r.s.id.startsWith(QLatin1String("LIB-")) ? 0 : 1;
        if (std::isfinite(r.ret)) {
            ++tested;
            if (!best || r.ret > best->ret)
                best = &r;
        }
    }
    for (const auto& d : std::as_const(deployments_)) {
        running += d.running;
        live += d.live_running;
    }
    ui::StatTile* t = ribbon_->tile(QStringLiteral("count"));
    t->set_value(QString::number(rows.size()));
    t->set_sub_label(slv_tr("%1 yours · %2 library").arg(custom).arg(rows.size() - custom));
    t = ribbon_->tile(QStringLiteral("tested"));
    t->set_value(QString::number(tested));
    t->set_sub_label(slv_tr("%1 never run").arg(rows.size() - tested));
    t = ribbon_->tile(QStringLiteral("running"));
    t->set_value(QString::number(running));
    t->set_sub_label(live > 0 ? slv_tr("%1 live — real money").arg(live) : slv_tr("paper only"));
    t->set_tone(live > 0 ? ui::StatTile::Tone::Warning : ui::StatTile::Tone::Neutral);
    t = ribbon_->tile(QStringLiteral("best"));
    if (best) {
        t->set_value(fmt::pct(best->ret, 1, true));
        t->set_sub_label(best->s.name);
        t->set_tone(best->ret > 0 ? ui::StatTile::Tone::Positive
                                  : (best->ret < 0 ? ui::StatTile::Tone::Negative : ui::StatTile::Tone::Neutral));
        QVector<double> spark;
        for (const auto& v : best->s.last_backtest.value(QStringLiteral("equity_spark")).toArray())
            spark.append(v.toDouble());
        if (spark.size() > 1)
            t->set_sparkline(spark, spark.first());
        else
            t->clear_sparkline();
    } else {
        t->set_value(kit::dash());
        t->set_sub_label(slv_tr("no backtest yet"));
        t->set_tone(ui::StatTile::Tone::Neutral);
        t->clear_sparkline();
    }
}

void StrategyLibraryView::update_detail() {
    AlgoStrategy s;
    const bool have = loaded_ && selected_strategy(&s);
    detail_empty_->setVisible(!have);
    detail_content_->setVisible(have);
    detail_header_->set_badge(have ? s.name : QString());
    if (!have)
        return;
    d_name_->setText(s.name);
    QStringList meta = {slv_category(s), s.timeframe.toUpper(),
                        s.entry_side == QLatin1String("SELL") ? slv_tr("short") : slv_tr("long"),
                        s.instrument_type.isEmpty() ? slv_tr("equity") : s.instrument_type};
    if (!s.id.startsWith(QLatin1String("LIB-")) && !s.description.isEmpty())
        meta.prepend(s.description);
    d_meta_->setText(meta.join(QStringLiteral(" · ")));
    d_meta_->setToolTip(s.id);
    d_entry_->setText(rules::rules_sentence(s.entry_conditions, s.entry_logic, rules::Side::Entry));
    d_exit_->setText(rules::rules_sentence(s.exit_conditions, s.exit_logic, rules::Side::Exit));
    auto pct_or_off = [](double v) { return v > 0 ? fmt::pct(v, 2, false) : slv_tr("off"); };
    QString costs = s.cost_model.value(QStringLiteral("preset")).toString();
    if (costs.isEmpty())
        costs = slv_tr("venue default");
    d_risk_->setText(slv_tr("Stop-loss %1 · take-profit %2 · trailing %3 · size %4 of capital · costs: %5")
                         .arg(pct_or_off(s.stop_loss), pct_or_off(s.take_profit), pct_or_off(s.trailing_stop),
                              fmt::pct(s.position_size_pct > 0 ? s.position_size_pct : 100.0, 0, false), costs));

    const QJsonObject& b = s.last_backtest;
    const bool tested = b.contains(QStringLiteral("total_return"));
    d_tiles_->setVisible(tested);
    if (!tested) {
        d_bt_note_->setText(slv_tr("Not backtested yet — BACKTEST picks a symbol and a period and runs it in the Builder."));
    } else {
        const QDateTime at = slv_parse_time(b.value(QStringLiteral("run_at")).toString());
        const QString ccy = b.value(QStringLiteral("currency")).toString();
        d_bt_note_->setText(slv_tr("%1 · %2 · as of %3 · %4 · buy & hold %5")
                                .arg(b.value(QStringLiteral("symbol")).toString(),
                                     b.value(QStringLiteral("timeframe")).toString(),
                                     at.isValid() ? fmt::calendar_date(at.date(), fmt::DateStyle::Medium) : kit::dash(),
                                     b.value(QStringLiteral("costs_applied")).toBool(true)
                                         ? slv_tr("net of costs (%1)").arg(b.value(QStringLiteral("cost_preset")).toString())
                                         : slv_tr("GROSS — no costs"),
                                     std::isfinite(slv_num(b, "benchmark_return"))
                                         ? fmt::pct(slv_num(b, "benchmark_return"), 1, true)
                                         : kit::dash()));
        const double ret = slv_num(b, "total_return");
        ui::StatTile* t = d_tiles_->tile(QStringLiteral("ret"));
        t->set_value(std::isfinite(ret) ? fmt::pct(ret, 1, true) : kit::dash());
        t->set_tone(ret > 0 ? ui::StatTile::Tone::Positive
                            : (ret < 0 ? ui::StatTile::Tone::Negative : ui::StatTile::Tone::Neutral));
        QVector<double> spark;
        for (const auto& v : b.value(QStringLiteral("equity_spark")).toArray())
            spark.append(v.toDouble());
        const double init = slv_num(b, "initial_capital");
        if (spark.size() > 1)
            t->set_sparkline(spark, std::isfinite(init) ? init : spark.first());
        else
            t->clear_sparkline();
        t->set_sub_label(ccy.isEmpty() || !std::isfinite(slv_num(b, "final_value"))
                             ? QString()
                             : slv_tr("final %1").arg(fmt::money(slv_num(b, "final_value"), ccy)));
        const double sharpe = slv_num(b, "sharpe_ratio");
        t = d_tiles_->tile(QStringLiteral("sharpe"));
        t->set_value(std::isfinite(sharpe) ? fmt::number(sharpe, 2) : kit::dash());
        t->set_tone(std::isfinite(sharpe) && sharpe < 0 ? ui::StatTile::Tone::Warning : ui::StatTile::Tone::Neutral);
        const double mdd = slv_num(b, "max_drawdown");
        t = d_tiles_->tile(QStringLiteral("mdd"));
        t->set_value(std::isfinite(mdd) ? fmt::pct(-std::abs(mdd), 1, true) : kit::dash());
        t->set_tone(std::abs(mdd) >= 25 ? ui::StatTile::Tone::Danger
                                        : (std::abs(mdd) >= 10 ? ui::StatTile::Tone::Warning : ui::StatTile::Tone::Neutral));
        t = d_tiles_->tile(QStringLiteral("trades"));
        t->set_value(QString::number(b.value(QStringLiteral("total_trades")).toInt()));
        const double wr = slv_num(b, "win_rate");
        t->set_sub_label(std::isfinite(wr) ? slv_tr("win rate %1").arg(fmt::pct(wr, 0, false)) : QString());
    }

    const auto it = deployments_.constFind(s.id);
    if (it == deployments_.constEnd() || it->total == 0) {
        d_dep_->setText(slv_tr("Never deployed. DEPLOY (PAPER) opens the deploy dialog; nothing starts until you "
                               "confirm it there."));
        kit::set_tone(d_dep_, kit::Tone::Dim);
    } else {
        QStringList pnl;
        for (auto p = it->pnl_by_ccy.constBegin(); p != it->pnl_by_ccy.constEnd(); ++p)
            pnl << fmt::money_in_column(p.value(), p.key(), live_mixed_, fmt::Compact::None, -1, true);
        d_dep_->setText(slv_tr("%1 deployment(s): %2 running (%3 live), %4 trades · net P&L %5 — see DASHBOARD (Alt+5)")
                            .arg(it->total)
                            .arg(it->running)
                            .arg(it->live_running)
                            .arg(it->trades)
                            .arg(pnl.isEmpty() ? kit::dash() : pnl.join(QStringLiteral(" / "))));
        kit::set_tone(d_dep_, it->live_running > 0 ? kit::Tone::Danger : kit::Tone::None);
    }
}

void StrategyLibraryView::update_actions() {
    const bool have = loaded_ && !selected_id().isEmpty();
    for (const char* id : {"open", "backtest", "deploy", "duplicate", "export", "delete"})
        bar_->set_action_enabled(QLatin1String(id), have);
}

// ── Selection / actions ─────────────────────────────────────────────────────

QString StrategyLibraryView::selected_id() const {
    if (!table_ || !table_->selectionModel())
        return {};
    const QModelIndex cur = table_->selectionModel()->currentIndex();
    if (!cur.isValid())
        return {};
    return proxy_->mapToSource(cur).siblingAtColumn(0).data(ui::LiveTableModel<Row>::KeyRole).toString();
}

bool StrategyLibraryView::selected_strategy(AlgoStrategy* out) const {
    const QString id = selected_id();
    if (id.isEmpty())
        return false;
    const Row* r = model_->row_for_key(id);
    if (!r)
        return false;
    *out = r->s;
    return true;
}

void StrategyLibraryView::select_id(const QString& strategy_id) {
    const int src = model_->index_of_key(strategy_id);
    if (src < 0)
        return;
    const QModelIndex idx = proxy_->mapFromSource(model_->index(src, 0));
    if (!idx.isValid())
        return;
    table_->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    table_->scrollTo(idx);
}

void StrategyLibraryView::trigger(const QString& action) {
    const QString id = selected_id();
    if (id.isEmpty())
        return;
    emit action_requested(action, id);
}

} // namespace fincept::screens
