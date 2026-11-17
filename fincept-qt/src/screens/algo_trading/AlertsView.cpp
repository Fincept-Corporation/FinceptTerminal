// src/screens/algo_trading/AlertsView.cpp
#include "screens/algo_trading/AlertsView.h"

#include "core/agent/AgentRiskMarkers.h"
#include "services/algo_trading/AlgoTradingTypes.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/FlowLayout.h"
#include "ui/widgets/KeySortProxy.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/PanelHeader.h"
#include "ui/widgets/ScreenCommandBar.h"
#include "ui/widgets/ScreenStatusBar.h"
#include "ui/widgets/algo/RuleTableEditor.h"

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableView>
#include <QVBoxLayout>

namespace fincept::screens {

namespace kit = fincept::screens::algo_kit;
namespace fmt = fincept::ui::formatting;

namespace {

enum AvWatchCol { kAvColName, kAvColSymbols, kAvColTf, kAvColRule, kAvColStatus, kAvColOn, kAvColPoll, kAvColChecked,
                  kAvColFired, kAvColWatchCount };
enum AvFiredCol { kAvColTime, kAvColWatch, kAvColSymbol, kAvColDetail, kAvColFiredCount };

/// Pages of the two stacks (same order in both).
enum AvPage { kAvPageTable = 0, kAvPageState = 1, kAvPageLoading = 2 };

/// Tables side by side from this width (the composer always spans the screen).
constexpr int kAvSideBySidePx = 1200;

QString av_time(qint64 ms) {
    if (ms <= 0)
        return kit::dash();
    return fmt::date_time(ms / 1000, fmt::DateStyle::Short, true);
}

kit::Tone av_status_tone(const QString& status) {
    const QString s = status.toLower();
    if (s == QLatin1String("error") || s == QLatin1String("failed"))
        return kit::Tone::Danger;
    if (s == QLatin1String("watching") || s == QLatin1String("running") || s == QLatin1String("polling"))
        return kit::Tone::Ok;
    if (s == QLatin1String("fired") || s == QLatin1String("triggered"))
        return kit::Tone::Accent;
    return kit::Tone::Dim;
}

QLabel* av_caption_of(QWidget* group) {
    return group ? group->findChild<QLabel*>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
}

} // namespace

AlertsView::AlertsView(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("alertsView"));
    build_ui();
    retranslate();
    set_loading();
}

void AlertsView::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Command bar ─────────────────────────────────────────────────────────
    bar_ = kit::make_tab_bar(this, kit::TabAlerts);
    using Role = ui::ScreenCommandBar::Role;
    bar_->add_action(QStringLiteral("save"), tr("SAVE WATCH"), QStringLiteral("Ctrl+S"), {}, Role::Primary, 100);
    bar_->add_action(QStringLiteral("new"), tr("NEW"), {}, {}, Role::Normal, 60);
    bar_->add_action(QStringLiteral("refresh"), tr("REFRESH"), QStringLiteral("Ctrl+R"), {}, Role::Normal, 50);
    bar_->add_action(QStringLiteral("scanner"), tr("OPEN SCANNER"), {}, {}, Role::Normal, 30);
    connect(bar_, &ui::ScreenCommandBar::action_triggered, this, [this](const QString& id) {
        if (id == QLatin1String("save"))
            emit save_requested();
        else if (id == QLatin1String("new"))
            emit new_requested();
        else if (id == QLatin1String("refresh"))
            emit refresh_requested();
        else if (id == QLatin1String("scanner"))
            emit open_scanner_requested();
    });
    root->addWidget(bar_);

    auto shortcut = [this](const QKeySequence& seq, auto fn) {
        auto* sc = new QShortcut(seq, this);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, this, fn);
    };
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_S), [this]() { emit save_requested(); });
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_R), [this]() { emit refresh_requested(); });

    main_split_ = kit::make_splitter(Qt::Vertical, this);
    tables_split_ = kit::make_splitter(Qt::Horizontal, main_split_);

    // ── LIVE WATCHES ────────────────────────────────────────────────────────
    auto wp = kit::make_panel(QString(), tables_split_, 0);
    watches_header_ = wp.header;
    edit_btn_ = kit::make_tool_button(QString(), QString(), wp.header);
    test_btn_ = kit::make_tool_button(QString(), QString(), wp.header);
    toggle_btn_ = kit::make_tool_button(QString(), QString(), wp.header);
    delete_btn_ = kit::make_tool_button(QString(), QString(), wp.header);
    // Deleting a watch drops its configuration (the controller confirms first).
    delete_btn_->setProperty(core::agent::kAgentRiskProperty, "delete");
    kit::set_tone(delete_btn_, kit::Tone::Danger);
    for (QPushButton* b : {edit_btn_, test_btn_, toggle_btn_, delete_btn_})
        wp.header->add_control(b);
    connect(edit_btn_, &QPushButton::clicked, this, [this]() {
        if (const QString id = selected_watch_id(); !id.isEmpty())
            emit edit_requested(id);
    });
    connect(test_btn_, &QPushButton::clicked, this, [this]() {
        if (const QString id = selected_watch_id(); !id.isEmpty())
            emit test_requested(id);
    });
    connect(toggle_btn_, &QPushButton::clicked, this, [this]() {
        const QString id = selected_watch_id();
        if (const AlertsWatchRow* w = watch(id))
            emit active_toggle_requested(id, !w->active);
    });
    connect(delete_btn_, &QPushButton::clicked, this, [this]() {
        if (const QString id = selected_watch_id(); !id.isEmpty())
            emit delete_requested(id);
    });

    watches_stack_ = new QStackedWidget(wp.body_widget);
    watches_table_ = new QTableView(watches_stack_);
    watches_model_ = new ui::LiveTableModel<AlertsWatchRow>(watches_table_);
    watches_model_->set_key([](const AlertsWatchRow& r) { return r.id; });
    watches_model_->set_cell([](const AlertsWatchRow& r, int col, int role) -> QVariant {
        using M = ui::LiveTableModel<AlertsWatchRow>;
        if (role == Qt::DisplayRole) {
            switch (col) {
                case kAvColName:
                    return r.name;
                case kAvColSymbols:
                    return r.symbols_text;
                case kAvColTf:
                    return r.timeframe;
                case kAvColRule:
                    return r.rule_text;
                case kAvColStatus:
                    return r.status.toUpper();
                case kAvColOn:
                    return r.active ? AlertsView::tr("ON") : AlertsView::tr("OFF");
                case kAvColPoll:
                    return AlertsView::tr("%1 s").arg(r.interval_sec);
                case kAvColChecked:
                    return av_time(r.last_eval_at);
                case kAvColFired:
                    return av_time(r.last_fired_at);
                default:
                    break;
            }
            return {};
        }
        if (role == M::SortRole) {
            switch (col) {
                case kAvColSymbols:
                    return r.symbol_count;
                case kAvColOn:
                    return r.active ? 1 : 0;
                case kAvColPoll:
                    return r.interval_sec;
                case kAvColChecked:
                    return r.last_eval_at > 0 ? QVariant(r.last_eval_at) : QVariant();
                case kAvColFired:
                    return r.last_fired_at > 0 ? QVariant(r.last_fired_at) : QVariant();
                case kAvColName:
                    return r.name;
                case kAvColTf:
                    return r.timeframe;
                case kAvColRule:
                    return r.rule_text;
                case kAvColStatus:
                    return r.status;
                default:
                    break;
            }
            return {};
        }
        if (role == Qt::ForegroundRole) {
            if (col == kAvColStatus)
                return kit::tone_color(av_status_tone(r.status));
            if (col == kAvColOn)
                return kit::tone_color(r.active ? kit::Tone::Ok : kit::Tone::Dim);
            return {};
        }
        if (role == Qt::ToolTipRole) {
            if (col == kAvColRule || col == kAvColSymbols)
                return col == kAvColRule ? r.rule_text : r.symbols_text;
            if (col == kAvColOn)
                return r.active ? AlertsView::tr("Watching: checked on every poll")
                                : AlertsView::tr("Disabled: not checked until it is enabled again");
            return {};
        }
        if (role == Qt::TextAlignmentRole && (col == kAvColPoll || col == kAvColChecked || col == kAvColFired))
            return int(Qt::AlignRight | Qt::AlignVCenter);
        return {};
    });
    watches_proxy_ = new ui::KeySortProxy(watches_table_);
    watches_proxy_->setSourceModel(watches_model_);
    watches_proxy_->setSortRole(ui::LiveTableModel<AlertsWatchRow>::SortRole);
    watches_table_->setModel(watches_proxy_);
    kit::setup_table(watches_table_, QStringLiteral("algo_watches"));
    watches_table_->setAccessibleName(tr("Live watches"));
    // Delete on the table deletes the selected watch (after the controller's confirm).
    watches_table_->setProperty(core::agent::kAgentRiskKeysProperty, QStringLiteral("Delete=delete"));
    watches_table_->installEventFilter(this);
    connect(watches_table_, &QTableView::doubleClicked, this, [this](const QModelIndex&) {
        if (const QString id = selected_watch_id(); !id.isEmpty())
            emit edit_requested(id);
    });
    connect(watches_table_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this]() { update_row_actions(); });
    watches_stack_->addWidget(watches_table_); // kAvPageTable
    watches_empty_ = new ui::EmptyState(watches_stack_);
    watches_empty_->set_compact(true);
    connect(watches_empty_, &ui::EmptyState::primary_clicked, this, [this]() {
        if (watches_error_)
            emit retry_watches_requested();
        else
            emit open_scanner_requested();
    });
    connect(watches_empty_, &ui::EmptyState::secondary_clicked, this, [this]() { rules_->focus_first(); });
    watches_stack_->addWidget(watches_empty_); // kAvPageState
    watches_loading_ = new ui::LoadingSkeleton(ui::LoadingSkeleton::Shape::Table, watches_stack_);
    watches_stack_->addWidget(watches_loading_); // kAvPageLoading
    wp.body->addWidget(watches_stack_, 1);
    tables_split_->addWidget(wp.frame);

    // ── FIRED ALERTS ────────────────────────────────────────────────────────
    auto hp = kit::make_panel(QString(), tables_split_, 0);
    history_header_ = hp.header;
    history_stack_ = new QStackedWidget(hp.body_widget);
    history_table_ = new QTableView(history_stack_);
    history_model_ = new ui::LiveTableModel<AlertsFiredRow>(history_table_);
    history_model_->set_key([](const AlertsFiredRow& r) { return r.id; });
    history_model_->set_cell([](const AlertsFiredRow& r, int col, int role) -> QVariant {
        using M = ui::LiveTableModel<AlertsFiredRow>;
        if (role == Qt::DisplayRole) {
            switch (col) {
                case kAvColTime:
                    return av_time(r.fired_at);
                case kAvColWatch:
                    return r.watch_name.isEmpty() ? kit::dash() : r.watch_name;
                case kAvColSymbol:
                    return r.symbol;
                case kAvColDetail:
                    return r.detail;
                default:
                    break;
            }
            return {};
        }
        if (role == M::SortRole) {
            switch (col) {
                case kAvColTime:
                    return r.fired_at;
                case kAvColWatch:
                    return r.watch_name;
                case kAvColSymbol:
                    return r.symbol;
                case kAvColDetail:
                    return r.detail;
                default:
                    break;
            }
            return {};
        }
        if (role == Qt::ToolTipRole && col == kAvColDetail)
            return r.detail;
        if (role == Qt::ForegroundRole && col == kAvColSymbol)
            return kit::tone_color(kit::Tone::Accent);
        return {};
    });
    history_proxy_ = new ui::KeySortProxy(history_table_);
    history_proxy_->setSourceModel(history_model_);
    history_proxy_->setSortRole(ui::LiveTableModel<AlertsFiredRow>::SortRole);
    history_table_->setModel(history_proxy_);
    kit::setup_table(history_table_, QStringLiteral("algo_alert_history"));
    history_table_->setAccessibleName(tr("Fired alerts"));
    history_stack_->addWidget(history_table_); // kAvPageTable
    history_empty_ = new ui::EmptyState(history_stack_);
    history_empty_->set_compact(true);
    connect(history_empty_, &ui::EmptyState::primary_clicked, this, [this]() {
        if (history_error_)
            emit retry_history_requested();
    });
    history_stack_->addWidget(history_empty_); // kAvPageState
    history_loading_ = new ui::LoadingSkeleton(ui::LoadingSkeleton::Shape::List, history_stack_);
    history_stack_->addWidget(history_loading_); // kAvPageLoading
    hp.body->addWidget(history_stack_, 1);
    tables_split_->addWidget(hp.frame);
    tables_split_->setStretchFactor(0, 3);
    tables_split_->setStretchFactor(1, 2);

    // ── COMPOSER ────────────────────────────────────────────────────────────
    main_split_->addWidget(tables_split_);
    main_split_->addWidget(build_composer(main_split_));
    main_split_->setStretchFactor(0, 4);
    main_split_->setStretchFactor(1, 5);
    root->addWidget(main_split_, 1);

    status_bar_ = kit::make_status_bar(this);
    status_bar_->set_text(QStringLiteral("data"), tr("Local database · background watches"));
    root->addWidget(status_bar_);

    // Column widths from the font (set once; the user may resize).
    kit::set_column_chars(watches_table_, {16, 18, 5, 30, 10, 4, 6, 15, 15});
    kit::set_column_chars(history_table_, {15, 16, 12, 30});
    watches_table_->sortByColumn(kAvColName, Qt::AscendingOrder);
    history_table_->sortByColumn(kAvColTime, Qt::DescendingOrder);
    update_row_actions();
}

QWidget* AlertsView::build_composer(QWidget* parent) {
    auto cp = kit::make_panel(QString(), parent, 0);
    composer_header_ = cp.header;

    auto* scroll = new QScrollArea(cp.body_widget);
    scroll->setObjectName(QStringLiteral("algoScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* body = new QWidget;
    auto* v = new QVBoxLayout(body);
    v->setContentsMargins(8, 6, 8, 6);
    v->setSpacing(6);
    scroll->setWidget(body);
    cp.body->addWidget(scroll, 1);

    // Feed settings: one wrapping row.
    auto* fields = new QWidget(body);
    auto* flow = new ui::FlowLayout(fields);
    name_edit_ = new QLineEdit(fields);
    kit::mark_field(name_edit_, tr("Watch name"));
    name_edit_->setMinimumWidth(QFontMetrics(font()).averageCharWidth() * 18);
    timeframe_combo_ = new QComboBox(fields);
    // A poll watch fetches bars; "live" is not a bar size (it fell through to daily).
    QStringList tfs = services::algo::algo_timeframes();
    tfs.removeAll(QStringLiteral("live"));
    timeframe_combo_->addItems(tfs);
    timeframe_combo_->setCurrentIndex(qMax(0, tfs.indexOf(QStringLiteral("1m"))));
    kit::mark_field(timeframe_combo_, tr("Bar timeframe"));
    data_source_combo_ = new QComboBox(fields);
    data_source_combo_->addItem(QString(), QStringLiteral("Broker"));
    data_source_combo_->addItem(QString(), QStringLiteral("Auto"));
    data_source_combo_->addItem(QString(), QStringLiteral("YFinance"));
    kit::mark_field(data_source_combo_, tr("Data source"));
    account_combo_ = new QComboBox(fields);
    account_combo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    account_combo_->setMinimumContentsLength(14);
    kit::mark_field(account_combo_, tr("Broker account"));
    interval_spin_ = new QSpinBox(fields);
    interval_spin_->setRange(30, 86400);
    interval_spin_->setValue(60);
    kit::mark_field(interval_spin_, tr("Poll interval in seconds"));
    cooldown_spin_ = new QSpinBox(this);
    cooldown_spin_->setRange(0, 1440);
    cooldown_spin_->setValue(15);
    // Poll watches are ONE-SHOT (ScanMonitor removes a watch the first time it fires), so a
    // cooldown between fires can never apply: kept for the stored format, never shown.
    cooldown_spin_->setVisible(false);
    providers_check_ = new QCheckBox(fields);
    providers_check_->setObjectName(QStringLiteral("algoCheck"));
    for (QWidget* w : std::initializer_list<QWidget*>{timeframe_combo_, data_source_combo_, account_combo_, interval_spin_})
        kit::guard_wheel(w);

    QWidget* g_name = kit::field_group(fields, QString(QLatin1Char(' ')), {name_edit_});
    QWidget* g_tf = kit::field_group(fields, QString(QLatin1Char(' ')), {timeframe_combo_});
    QWidget* g_src = kit::field_group(fields, QString(QLatin1Char(' ')), {data_source_combo_});
    QWidget* g_acct = kit::field_group(fields, QString(QLatin1Char(' ')), {account_combo_});
    QWidget* g_poll = kit::field_group(fields, QString(QLatin1Char(' ')), {interval_spin_});
    name_lbl_ = av_caption_of(g_name);
    tf_lbl_ = av_caption_of(g_tf);
    source_lbl_ = av_caption_of(g_src);
    account_lbl_ = av_caption_of(g_acct);
    poll_lbl_ = av_caption_of(g_poll);
    for (QWidget* g : {g_name, g_tf, g_src, g_acct, g_poll})
        flow->addWidget(g);
    flow->addWidget(providers_check_);
    v->addWidget(fields);

    // A non-broker source needs no account: disabled with the reason.
    auto sync_account = [this]() {
        const bool needs = data_source_combo_->currentData().toString() != QLatin1String("YFinance");
        account_combo_->setEnabled(needs);
        account_combo_->setToolTip(needs ? tr("Broker account the watch reads bars from")
                                         : tr("Not used: the data source is YFinance only"));
    };
    connect(data_source_combo_, &QComboBox::currentIndexChanged, this, [sync_account](int) { sync_account(); });
    sync_account();

    // Symbols: the controller's chip input goes into the slot; quick lists beside it.
    auto* sym_row = new QWidget(body);
    auto* sh = new QHBoxLayout(sym_row);
    sh->setContentsMargins(0, 0, 0, 0);
    sh->setSpacing(6);
    symbols_lbl_ = new QLabel(sym_row);
    symbols_lbl_->setObjectName(QStringLiteral("algoFieldLabel"));
    sh->addWidget(symbols_lbl_, 0, Qt::AlignTop);
    symbols_slot_ = new QWidget(sym_row);
    auto* slot_l = new QHBoxLayout(symbols_slot_);
    slot_l->setContentsMargins(0, 0, 0, 0);
    slot_l->setSpacing(0);
    sh->addWidget(symbols_slot_, 1);
    // nifty50_symbols() holds the 20 largest NIFTY constituents, not all 50.
    nifty_btn_ = kit::make_tool_button(QString(), QString(), sym_row);
    bank_btn_ = kit::make_tool_button(QString(), QString(), sym_row);
    connect(nifty_btn_, &QPushButton::clicked, this,
            [this]() { emit quick_symbols_requested(services::algo::nifty50_symbols()); });
    connect(bank_btn_, &QPushButton::clicked, this,
            [this]() { emit quick_symbols_requested(services::algo::bank_nifty_symbols()); });
    sh->addWidget(nifty_btn_, 0, Qt::AlignTop);
    sh->addWidget(bank_btn_, 0, Qt::AlignTop);
    v->addWidget(sym_row);

    rules_ = new ui::algo::RuleTableEditor(ui::algo::RuleTableEditor::Kind::Alert, body);
    v->addWidget(rules_);
    cp.header->add_control(rules_->header_controls());
    rules_->header_controls()->show();

    message_ = kit::make_note(QString(), body);
    message_->setVisible(false);
    v->addWidget(message_);
    v->addStretch(1);

    QWidget::setTabOrder(name_edit_, timeframe_combo_);
    QWidget::setTabOrder(timeframe_combo_, data_source_combo_);
    QWidget::setTabOrder(data_source_combo_, account_combo_);
    QWidget::setTabOrder(account_combo_, interval_spin_);
    QWidget::setTabOrder(interval_spin_, providers_check_);
    return cp.frame;
}

void AlertsView::retranslate() {
    kit::retranslate_tab_bar(bar_);
    bar_->set_action_text(QStringLiteral("save"), editing_name_.isEmpty() ? tr("SAVE WATCH") : tr("UPDATE WATCH"));
    bar_->set_action_tooltip(QStringLiteral("save"),
                             tr("Save the composer as a background watch: it alerts once when the rules are met (Ctrl+S)"));
    bar_->set_action_text(QStringLiteral("new"), tr("NEW"));
    bar_->set_action_tooltip(QStringLiteral("new"), tr("Start a new watch (clears the composer)"));
    bar_->set_action_text(QStringLiteral("refresh"), tr("REFRESH"));
    bar_->set_action_tooltip(QStringLiteral("refresh"), tr("Read the watches and fired alerts again (Ctrl+R)"));
    bar_->set_action_text(QStringLiteral("scanner"), tr("OPEN SCANNER"));
    bar_->set_action_tooltip(QStringLiteral("scanner"),
                             tr("Find symbols that match a rule now; + CREATE ALERT sends a scan here"));

    watches_header_->set_title(tr("Live watches"));
    watches_header_->title_label()->setToolTip(tr("Watches are one-shot: a watch is removed when it triggers"));
    edit_btn_->setText(tr("EDIT"));
    edit_btn_->setToolTip(tr("Load the selected watch into the composer (double-click a row)"));
    edit_btn_->setAccessibleName(tr("Edit the selected watch"));
    test_btn_->setText(tr("TEST"));
    test_btn_->setToolTip(tr("Fire a sample alert for the selected watch"));
    test_btn_->setAccessibleName(tr("Fire a test alert for the selected watch"));
    delete_btn_->setText(tr("DELETE"));
    delete_btn_->setToolTip(tr("Delete the selected watch (Delete); its fired alerts are kept"));
    delete_btn_->setAccessibleName(tr("Delete the selected watch"));
    history_header_->set_title(tr("Fired alerts"));
    composer_header_->set_title(editing_name_.isEmpty() ? tr("New watch") : tr("Editing %1").arg(editing_name_));

    watches_model_->set_headers({tr("NAME"), tr("SYMBOLS"), tr("BARS"), tr("RULES"), tr("STATUS"), tr("ON"),
                                 tr("POLL"), tr("LAST CHECK"), tr("LAST FIRED")});
    history_model_->set_headers({tr("TIME"), tr("WATCH"), tr("SYMBOL"), tr("DETAIL")});

    if (name_lbl_)
        name_lbl_->setText(tr("NAME"));
    if (tf_lbl_)
        tf_lbl_->setText(tr("BARS"));
    if (source_lbl_)
        source_lbl_->setText(tr("SOURCE"));
    if (account_lbl_)
        account_lbl_->setText(tr("ACCOUNT"));
    if (poll_lbl_)
        poll_lbl_->setText(tr("POLL"));
    if (poll_lbl_)
        poll_lbl_->setToolTip(tr("Poll interval: the watch checks its rules this often"));
    symbols_lbl_->setText(tr("SYMBOLS"));
    name_edit_->setPlaceholderText(tr("My volume watch"));
    data_source_combo_->setItemText(0, tr("Broker only"));
    data_source_combo_->setItemText(1, tr("Auto (broker → Yahoo)"));
    data_source_combo_->setItemText(2, tr("Yahoo only"));
    interval_spin_->setSuffix(tr(" s"));
    providers_check_->setText(tr("PROVIDERS"));
    providers_check_->setToolTip(tr("Also send the alert to your external notification providers"));
    providers_check_->setAccessibleName(tr("Send to external providers"));
    nifty_btn_->setText(tr("NIFTY TOP 20"));
    nifty_btn_->setToolTip(tr("The 20 largest NIFTY 50 constituents"));
    bank_btn_->setText(tr("BANK NIFTY"));
    bank_btn_->setToolTip(tr("The BANK NIFTY constituents"));
    update_row_actions();
    if (watches_stack_->currentIndex() == kAvPageState)
        show_watches_page();
    if (history_stack_->currentIndex() == kAvPageState)
        show_history_page();
}

void AlertsView::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

void AlertsView::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    status_bar_->start_clock();
}

void AlertsView::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    status_bar_->stop_clock();
}

void AlertsView::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    const bool narrow = e->size().width() < kAvSideBySidePx;
    if (narrow == narrow_ && tables_split_->orientation() == (narrow ? Qt::Vertical : Qt::Horizontal))
        return;
    narrow_ = narrow;
    tables_split_->setOrientation(narrow ? Qt::Vertical : Qt::Horizontal);
}

bool AlertsView::eventFilter(QObject* watched, QEvent* event) {
    if (watched == watches_table_ && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Delete) {
            if (const QString id = selected_watch_id(); !id.isEmpty())
                emit delete_requested(id);
            return true;
        }
        if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) && ke->modifiers() == Qt::NoModifier) {
            if (const QString id = selected_watch_id(); !id.isEmpty())
                emit edit_requested(id);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

// ── Composer ────────────────────────────────────────────────────────────────

void AlertsView::set_accounts(const QVector<QPair<QString, QString>>& accounts) {
    const QString keep = account_combo_->currentData().toString();
    QSignalBlocker blocker(account_combo_);
    account_combo_->clear();
    for (const auto& a : accounts)
        account_combo_->addItem(a.first, a.second);
    const int idx = account_combo_->findData(keep);
    if (idx >= 0)
        account_combo_->setCurrentIndex(idx);
}

void AlertsView::set_editing(const QString& name) {
    editing_name_ = name;
    bar_->set_action_text(QStringLiteral("save"), name.isEmpty() ? tr("SAVE WATCH") : tr("UPDATE WATCH"));
    composer_header_->set_title(name.isEmpty() ? tr("New watch") : tr("Editing %1").arg(name));
    composer_header_->set_badge(name.isEmpty() ? QString() : tr("UPDATE replaces it and restarts it"));
}

void AlertsView::set_message(const QString& text, kit::Tone tone) {
    message_->setText(text);
    kit::set_tone(message_, tone);
    message_->setVisible(!text.isEmpty());
}

// ── Tables ──────────────────────────────────────────────────────────────────

void AlertsView::set_loading() {
    // Only before the first answer: a re-read keeps the rows on screen (no flicker).
    if (!watches_loaded_) {
        watches_stack_->setCurrentIndex(kAvPageLoading);
        kit::set_state(status_bar_, kit::RunState::Running, tr("LOADING"));
    }
    if (!history_loaded_)
        history_stack_->setCurrentIndex(kAvPageLoading);
}

void AlertsView::set_watches(const QVector<AlertsWatchRow>& rows) {
    const QString keep = selected_watch_id();
    watches_loaded_ = true;
    watches_error_ = false;
    watches_model_->upsert(rows);
    watches_proxy_->resort();
    // Keep the selection by identity; else the first row.
    const int row = watches_model_->index_of_key(keep);
    QModelIndex pick;
    if (row >= 0)
        pick = watches_proxy_->mapFromSource(watches_model_->index(row, 0));
    else if (watches_proxy_->rowCount() > 0)
        pick = watches_proxy_->index(0, 0);
    if (pick.isValid())
        watches_table_->selectionModel()->setCurrentIndex(pick, QItemSelectionModel::ClearAndSelect |
                                                                    QItemSelectionModel::Rows);
    watches_header_->set_badge(QString::number(rows.size()));
    show_watches_page();
    update_row_actions();
    update_info();
    kit::set_state(status_bar_, kit::RunState::Ready);
}

void AlertsView::set_watches_error(const QString& error) {
    watches_loaded_ = true;
    watches_error_ = true;
    watches_model_->clear();
    watches_header_->set_badge(QString());
    watches_empty_->apply(ui::EmptyState::error_spec(tr("Your live watches could not be read from the local database."),
                                                     tr("Could not load watches"), QString(), error));
    watches_stack_->setCurrentIndex(kAvPageState);
    update_row_actions();
    kit::set_state(status_bar_, kit::RunState::Failed, tr("READ FAILED"));
}

void AlertsView::show_watches_page() {
    if (watches_error_) {
        watches_stack_->setCurrentIndex(kAvPageState);
        return;
    }
    if (watches_model_->rowCount() > 0) {
        watches_stack_->setCurrentIndex(kAvPageTable);
        return;
    }
    ui::EmptyState::Spec spec = ui::EmptyState::no_data_spec(
        tr("No live watches"),
        tr("A watch checks its rules on every poll and alerts once when they are met. Build the rules in the "
           "composer below and press SAVE WATCH, or send a scan here from SCANNER with + CREATE ALERT."),
        tr("OPEN SCANNER"));
    spec.glyph = QStringLiteral("◎");
    spec.secondary = tr("BUILD A RULE");
    watches_empty_->apply(spec);
    watches_stack_->setCurrentIndex(kAvPageState);
}

void AlertsView::set_history(const QVector<AlertsFiredRow>& rows) {
    history_loaded_ = true;
    history_error_ = false;
    history_model_->upsert(rows);
    history_proxy_->resort();
    history_header_->set_badge(QString::number(rows.size()));
    show_history_page();
    update_info();
}

void AlertsView::set_history_error(const QString& error) {
    history_loaded_ = true;
    history_error_ = true;
    history_model_->clear();
    history_header_->set_badge(QString());
    history_empty_->apply(ui::EmptyState::error_spec(tr("The alert history could not be read from the local database."),
                                                     tr("Could not load the history"), QString(), error));
    history_stack_->setCurrentIndex(kAvPageState);
}

void AlertsView::show_history_page() {
    if (history_error_) {
        history_stack_->setCurrentIndex(kAvPageState);
        return;
    }
    if (history_model_->rowCount() > 0) {
        history_stack_->setCurrentIndex(kAvPageTable);
        return;
    }
    ui::EmptyState::Spec spec = ui::EmptyState::no_data_spec(
        tr("No alerts fired yet"),
        tr("Each time a watch triggers, its time, symbol and the values that met the rules are listed here. TEST on "
           "a watch fires a sample alert."));
    spec.glyph = QStringLiteral("◷");
    history_empty_->apply(spec);
    history_stack_->setCurrentIndex(kAvPageState);
}

void AlertsView::set_as_of(const QDateTime& when) {
    status_bar_->set_text(QStringLiteral("asof"),
                          when.isValid() ? tr("as of %1").arg(when.toString(QStringLiteral("HH:mm:ss"))) : QString());
}

QString AlertsView::selected_watch_id() const {
    if (!watches_table_->selectionModel())
        return {};
    const QModelIndexList rows = watches_table_->selectionModel()->selectedRows();
    if (rows.isEmpty())
        return {};
    const QModelIndex src = watches_proxy_->mapToSource(rows.first());
    const AlertsWatchRow* r = watches_model_->row_at(src);
    return r ? r->id : QString();
}

const AlertsWatchRow* AlertsView::watch(const QString& id) const {
    return id.isEmpty() ? nullptr : watches_model_->row_for_key(id);
}

void AlertsView::update_row_actions() {
    const AlertsWatchRow* w = watch(selected_watch_id());
    for (QPushButton* b : {edit_btn_, test_btn_, toggle_btn_, delete_btn_})
        b->setEnabled(w != nullptr);
    toggle_btn_->setText(w && !w->active ? tr("ENABLE") : tr("DISABLE"));
    toggle_btn_->setToolTip(w && !w->active ? tr("Start checking the selected watch again")
                                            : tr("Stop checking the selected watch (it stays saved)"));
    toggle_btn_->setAccessibleName(toggle_btn_->text());
}

void AlertsView::update_info() {
    int on = 0;
    for (const auto& r : watches_model_->rows())
        on += r.active ? 1 : 0;
    status_bar_->set_text(QStringLiteral("info"), tr("%1 watches · %2 on · %3 fired alerts")
                                                      .arg(watches_model_->rowCount())
                                                      .arg(on)
                                                      .arg(history_model_->rowCount()));
}

} // namespace fincept::screens
