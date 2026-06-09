// src/screens/algo_trading/StrategyBuilderPanel.cpp
#include "screens/algo_trading/StrategyBuilderPanel.h"

#include "algo_engine/AlgoEngine.h"
#include "algo_engine/fno/FnoAlgoTypes.h"
#include "algo_engine/fno/FnoStrategyPreview.h"
#include "core/currency/Currency.h"
#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "screens/algo_trading/AlgoDeployDialog.h"
#include "screens/fno/BuilderAnalyticsRibbon.h"
#include "screens/fno/PayoffChartWidget.h"
#include "services/algo_trading/AlgoTradingService.h"
#include "services/options/OptionChainService.h"
#include "services/options/StrategyAnalytics.h"
#include "trading/AccountManager.h"
#include "trading/BrokerAccount.h"
#include "trading/BrokerInterface.h"
#include "trading/BrokerRegistry.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/algo/FnoLegRuleEditor.h"

#include <QDate>
#include <QDateEdit>
#include <QEventLoop>
#include <QFrame>
#include <QFutureWatcher>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <QtConcurrent>

#include <cmath>

namespace fincept::screens {
namespace algo_ns = fincept::algo;

// ── Builder-local styling helpers ─────────────────────────────────────────────
// Keep all the redesign's look-and-feel in one place so the layout code below
// stays about structure. Everything reads from theme tokens; the Builder module's
// signature orange (#FF6B35, matching the BUILDER tab) is the only literal.
namespace {

constexpr const char* kBuilderOrange = "#FF6B35";

// Uppercase, letter-spaced card title with a hairline underline (terminal style).
QString card_header_style() {
    return QString("color:%1;font-size:%2px;font-weight:700;letter-spacing:1px;"
                   "font-family:%3;background:transparent;border:none;"
                   "padding-bottom:3px;border-bottom:1px solid %4;")
        .arg(ui::colors::TEXT_SECONDARY())
        .arg(ui::fonts::TINY)
        .arg(ui::fonts::DATA_FAMILY())
        .arg(ui::colors::BORDER_DIM());
}

// Dim caption used for field labels and toolbar group captions.
QString caption_style() {
    return QString("color:%1;font-size:%2px;font-weight:700;letter-spacing:1px;"
                   "font-family:%3;background:transparent;border:none;")
        .arg(ui::colors::TEXT_TERTIARY())
        .arg(ui::fonts::TINY)
        .arg(ui::fonts::DATA_FAMILY());
}

QLabel* field_label(const QString& text) {
    auto* l = new QLabel(text);
    l->setStyleSheet(caption_style());
    return l;
}

// A titled, bordered card with a colored left accent stripe. The child keeps its
// own internal header; the stripe + border give the clear separation the redesign
// is after. Object-name-scoped so the border does NOT cascade onto children.
QFrame* make_card(QWidget* child, const QString& accent) {
    auto* f = new QFrame();
    f->setObjectName(QStringLiteral("builderCard"));
    f->setStyleSheet(QString("QFrame#builderCard{background:%1;border:1px solid %2;"
                             "border-left:2px solid %3;border-radius:5px;}")
                         .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM())
                         .arg(accent));
    auto* l = new QVBoxLayout(f);
    l->setContentsMargins(10, 8, 10, 10);
    l->setSpacing(6);
    l->addWidget(child);
    return f;
}

// Outlined ghost buttons matching the Dashboard tab's STOP/REMOVE style:
// transparent fill, colored 1px border + bold text, faint colored wash on hover.

// Primary "go live" — the module's signature orange.
QString deploy_btn_style() {
    return QString("QPushButton{background:transparent;color:%1;border:1px solid %1;border-radius:2px;"
                   "padding:5px 18px;font-weight:700;font-size:%2px;font-family:%3;}"
                   "QPushButton:hover{background:rgba(255,107,53,0.12);}"
                   "QPushButton:pressed{background:rgba(255,107,53,0.22);}")
        .arg(kBuilderOrange)
        .arg(ui::fonts::SMALL)
        .arg(ui::fonts::DATA_FAMILY());
}

// Secondary "save" — neutral gray outline.
QString save_btn_style() {
    return QString("QPushButton{background:transparent;color:%1;border:1px solid %1;border-radius:2px;"
                   "padding:5px 16px;font-weight:700;font-size:%2px;font-family:%3;}"
                   "QPushButton:hover{background:rgba(128,128,128,0.14);}")
        .arg(ui::colors::TEXT_SECONDARY())
        .arg(ui::fonts::SMALL)
        .arg(ui::fonts::DATA_FAMILY());
}

// The run-backtest action — amber outline (distinct hue from Deploy's orange,
// and never adjacent to it since it lives in the right-hand setup card).
QString run_btn_style() {
    return QString("QPushButton{background:transparent;color:%1;border:1px solid %1;border-radius:2px;"
                   "padding:7px 16px;font-weight:700;font-size:%2px;font-family:%3;letter-spacing:1px;}"
                   "QPushButton:hover{background:rgba(217,119,6,0.12);}"
                   "QPushButton:pressed{background:rgba(217,119,6,0.22);}"
                   "QPushButton:disabled{color:%4;border-color:%4;}")
        .arg(ui::colors::AMBER())
        .arg(ui::fonts::SMALL)
        .arg(ui::fonts::DATA_FAMILY())
        .arg(ui::colors::TEXT_DIM());
}

QString chip_style() {
    return QString("color:%1;background:%2;border:1px solid %3;border-radius:2px;"
                   "padding:3px 8px;font-size:9px;font-weight:700;letter-spacing:1px;font-family:%4;")
        .arg(ui::colors::AMBER(), ui::colors::ACCENT_BG(), ui::colors::AMBER_DIM())
        .arg(ui::fonts::DATA_FAMILY());
}

QString status_style() {
    return QString("color:%1;font-size:%2px;font-family:%3;background:transparent;border:none;")
        .arg(ui::colors::TEXT_TERTIARY())
        .arg(ui::fonts::TINY)
        .arg(ui::fonts::DATA_FAMILY());
}

// Bounded fetch of the current LTP from the connected broker for deploy-time sanity
// checks. Runs off-thread with a short timeout so the deploy click never hangs.
double fetch_quote_ltp(const QString& broker_id, const QString& account_id, const QString& symbol,
                       int timeout_ms) {
    if (broker_id.isEmpty() || account_id.isEmpty() || symbol.isEmpty())
        return 0.0;
    auto fut = QtConcurrent::run([broker_id, account_id, symbol]() -> double {
        auto* broker = trading::BrokerRegistry::instance().get(broker_id);
        if (!broker)
            return 0.0;
        auto creds = trading::AccountManager::instance().load_credentials(account_id);
        auto resp = broker->get_quotes(creds, {symbol});
        if (resp.success && resp.data.has_value() && !resp.data->isEmpty())
            return resp.data->first().ltp;
        return 0.0;
    });
    QFutureWatcher<double> watcher;
    QEventLoop loop;
    QObject::connect(&watcher, &QFutureWatcher<double>::finished, &loop, &QEventLoop::quit);
    watcher.setFuture(fut);
    QTimer::singleShot(timeout_ms, &loop, &QEventLoop::quit);
    loop.exec();
    return watcher.isFinished() ? watcher.result() : 0.0;
}

// Collects human-readable warnings for price-based leaves that can't fire at `price`
// (crossover already on the wrong side, or threshold wildly far from price). Only
// price-scale indicators are checked — RSI<30 etc. are left alone.
void scan_unreachable(const QJsonArray& conds, const QString& section, double price, QStringList& out) {
    static const QSet<QString> price_inds = {"CLOSE", "OPEN", "HIGH", "LOW", "VWAP"};
    for (const auto& v : conds) {
        const QJsonObject o = v.toObject();
        if (o.contains("children")) {
            scan_unreachable(o.value("children").toArray(), section, price, out);
            continue;
        }
        if (o.value("compare_mode").toString("value") != "value")
            continue;
        const QString ind = o.value("indicator").toString();
        if (!price_inds.contains(ind))
            continue;
        const QString op = o.value("operator").toString();
        const double val = o.value("value").toDouble();
        if (val <= 0)
            continue;
        if (op == "crosses_above" && price > val)
            out << QString("• %1 '%2 crosses above %3' — price %4 is already above; it can never cross up.")
                       .arg(section, ind).arg(val, 0, 'f', 2).arg(price, 0, 'f', 2);
        else if (op == "crosses_below" && price < val)
            out << QString("• %1 '%2 crosses below %3' — price %4 is already below; it can never cross down.")
                       .arg(section, ind).arg(val, 0, 'f', 2).arg(price, 0, 'f', 2);
        else if (std::abs(price - val) / price > 0.25)
            out << QString("• %1 '%2 %3 %4' — threshold is %5% away from price %6.")
                       .arg(section, ind, op).arg(val, 0, 'f', 2)
                       .arg(std::abs(price - val) / price * 100.0, 0, 'f', 0).arg(price, 0, 'f', 2);
    }
}

} // namespace

StrategyBuilderPanel::StrategyBuilderPanel(QWidget* parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("strategyBuilderPanel"));
    build_ui();
    connect_service();
}

void StrategyBuilderPanel::build_ui() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);

    main_layout->addWidget(build_top_toolbar());

    validation_banner_ = new QLabel(this);
    validation_banner_->setObjectName(QStringLiteral("builderValidationBanner"));
    validation_banner_->setWordWrap(true);
    validation_banner_->setStyleSheet(
        QString("background:%1;color:%2;border:1px solid %3;border-left:3px solid %2;"
                "padding:6px 12px;font-size:%4px;font-weight:600;font-family:%5;")
            .arg(ui::colors::NEGATIVE_BG(), ui::colors::NEGATIVE(), ui::colors::NEGATIVE_DIM())
            .arg(ui::fonts::TINY)
            .arg(ui::fonts::DATA_FAMILY()));
    validation_banner_->setVisible(false);
    main_layout->addWidget(validation_banner_);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setObjectName(QStringLiteral("builderSplitter"));
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(6);
    splitter->addWidget(build_left_panel());
    splitter->addWidget(build_right_panel());
    splitter->setStretchFactor(0, 46);
    splitter->setStretchFactor(1, 54);

    main_layout->addWidget(splitter, 1);
}

QWidget* StrategyBuilderPanel::build_top_toolbar() {
    auto* toolbar = new QWidget(this);
    toolbar->setObjectName(QStringLiteral("builderToolbar"));
    toolbar->setFixedHeight(48);
    toolbar->setStyleSheet(
        QString("QWidget#builderToolbar{background:%1;border-bottom:1px solid %2;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* layout = new QHBoxLayout(toolbar);
    layout->setContentsMargins(12, 6, 12, 6);
    layout->setSpacing(8);

    auto caption = [this](const QString& text) {
        auto* l = new QLabel(text, this);
        l->setStyleSheet(caption_style());
        return l;
    };
    auto divider = [this]() {
        auto* d = new QWidget(this);
        d->setFixedSize(1, 24);
        d->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM()));
        return d;
    };

    // ── Identity group: name + description ──
    name_edit_ = new QLineEdit(this);
    name_edit_->setObjectName(QStringLiteral("builderNameEdit"));
    name_edit_->setPlaceholderText(tr("Strategy Name"));
    name_edit_->setMinimumWidth(150);

    desc_edit_ = new QLineEdit(this);
    desc_edit_->setObjectName(QStringLiteral("builderDescEdit"));
    desc_edit_->setPlaceholderText(tr("Description (optional)"));
    desc_edit_->setMinimumWidth(150);

    // ── Config group: timeframe (a saved strategy property) + template loader ──
    timeframe_combo_ = new QComboBox(this);
    timeframe_combo_->setObjectName(QStringLiteral("builderTimeframe"));
    for (const auto& tf : services::algo::algo_timeframes())
        timeframe_combo_->addItem(tf);
    timeframe_combo_->setCurrentText(QStringLiteral("5m"));
    timeframe_combo_->setToolTip(tr("Bar timeframe the entry/exit rules evaluate on"));

    template_combo_ = new QComboBox(this);
    template_combo_->setObjectName(QStringLiteral("builderTemplateCombo"));
    template_combo_->addItem(tr("Templates…"));
    for (const auto& t : services::algo::algo_strategy_templates())
        template_combo_->addItem(t.name);
    template_combo_->setToolTip(tr("Load a ready-made strategy as a starting point"));

    // ── Instrument-type selector (P2.3) ──
    instrument_type_combo_ = new QComboBox(this);
    instrument_type_combo_->setObjectName(QStringLiteral("builderInstrumentType"));
    instrument_type_combo_->addItem(tr("Equity"),  QStringLiteral("equity"));
    instrument_type_combo_->addItem(tr("Option"),  QStringLiteral("option"));
    instrument_type_combo_->addItem(tr("Future"),  QStringLiteral("future"));
    instrument_type_combo_->setToolTip(tr("Instrument type this strategy trades"));

    // ── State chip + primary actions ──
    state_chip_ = new QLabel(tr("NEW DRAFT"), this);
    state_chip_->setObjectName(QStringLiteral("builderStateChip"));
    state_chip_->setStyleSheet(chip_style());

    save_btn_ = new QPushButton(tr("Save"), this);
    save_btn_->setObjectName(QStringLiteral("builderSaveBtn"));
    save_btn_->setCursor(Qt::PointingHandCursor);
    save_btn_->setToolTip(tr("Save this strategy to My Strategies"));
    save_btn_->setStyleSheet(save_btn_style());

    deploy_btn_ = new QPushButton(tr("Deploy ▸"), this);
    deploy_btn_->setObjectName(QStringLiteral("builderDeployBtn"));
    deploy_btn_->setCursor(Qt::PointingHandCursor);
    deploy_btn_->setToolTip(tr("Go live (paper or real) with this strategy"));
    deploy_btn_->setStyleSheet(deploy_btn_style());

    // backtest_btn_ + symbol_combo_ now live in the right-hand BACKTEST SETUP card,
    // beside the rest of the test inputs — created in build_right_panel().

    layout->addWidget(caption(tr("NAME")));
    layout->addWidget(name_edit_, 1);
    layout->addWidget(caption(tr("DESC")));
    layout->addWidget(desc_edit_, 1);
    layout->addWidget(divider());
    layout->addWidget(caption(tr("TF")));
    layout->addWidget(timeframe_combo_);
    layout->addWidget(template_combo_);
    layout->addWidget(divider());
    layout->addWidget(caption(tr("TYPE")));
    layout->addWidget(instrument_type_combo_);
    layout->addStretch();
    layout->addWidget(state_chip_);
    layout->addWidget(save_btn_);
    layout->addWidget(deploy_btn_);

    connect(save_btn_, &QPushButton::clicked, this, &StrategyBuilderPanel::on_save);
    connect(deploy_btn_, &QPushButton::clicked, this, &StrategyBuilderPanel::on_deploy);
    connect(template_combo_, QOverload<int>::of(&QComboBox::activated), this,
            &StrategyBuilderPanel::load_template);
    connect(instrument_type_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &StrategyBuilderPanel::on_instrument_type_changed);

    return toolbar;
}

QWidget* StrategyBuilderPanel::build_left_panel() {
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("builderLeftScroll"));
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(10, 10, 6, 10);
    layout->setSpacing(10);

    entry_section_ = new ui::algo::ConditionSection(
        ui::algo::ConditionSection::Type::Entry, this);
    exit_section_ = new ui::algo::ConditionSection(
        ui::algo::ConditionSection::Type::Exit, this);
    risk_panel_ = new ui::algo::RiskManagementPanel(this);

    // Color-coded cards build an instant visual language: green = entry/buy,
    // red = exit/sell, orange = risk. Each child keeps its own internal header.
    layout->addWidget(make_card(entry_section_, ui::colors::POSITIVE()));
    layout->addWidget(make_card(exit_section_, ui::colors::NEGATIVE()));
    layout->addWidget(make_card(risk_panel_, QString::fromLatin1(kBuilderOrange)));
    layout->addStretch();

    scroll->setWidget(content);
    return scroll;
}

QWidget* StrategyBuilderPanel::build_right_panel() {
    auto* right = new QWidget(this);
    auto* layout = new QVBoxLayout(right);
    layout->setContentsMargins(6, 10, 10, 10);
    layout->setSpacing(10);

    // ── BACKTEST SETUP card — every test input in one place ──────────────────
    auto* setup_card = new QFrame(right);
    bt_setup_card_ = setup_card;  // store for show/hide in on_instrument_type_changed
    setup_card->setObjectName(QStringLiteral("builderSetupCard"));
    setup_card->setStyleSheet(QString("QFrame#builderSetupCard{background:%1;border:1px solid %2;"
                                       "border-left:2px solid %3;border-radius:5px;}")
                                  .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM())
                                  .arg(ui::colors::CYAN()));
    auto* setup = new QVBoxLayout(setup_card);
    setup->setContentsMargins(12, 10, 12, 12);
    setup->setSpacing(8);

    bt_header_ = new QLabel(tr("BACKTEST SETUP"), this);
    bt_header_->setObjectName(QStringLiteral("builderBtHeader"));
    bt_header_->setStyleSheet(card_header_style());
    setup->addWidget(bt_header_);

    // Symbol moved here from the top toolbar so it sits with the other test inputs.
    symbol_combo_ = new ui::algo::SymbolSearchCombo(this);
    symbol_combo_->setMinimumWidth(140);

    bt_capital_ = new QDoubleSpinBox(this);
    bt_capital_->setObjectName(QStringLiteral("builderBtCapital"));
    bt_capital_->setRange(1000, 100000000);
    bt_capital_->setDecimals(0);
    bt_capital_->setValue(100000);
    cur::bindPrefix(bt_capital_); // backtest capital is user-denominated

    const QDate today = QDate::currentDate();
    bt_start_date_ = new QDateEdit(this);
    bt_start_date_->setObjectName(QStringLiteral("builderBtStart"));
    bt_start_date_->setCalendarPopup(true);
    bt_start_date_->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    bt_start_date_->setMinimumDate(QDate(2000, 1, 1));
    bt_start_date_->setMaximumDate(today);
    bt_start_date_->setDate(today.addYears(-1));

    bt_end_date_ = new QDateEdit(this);
    bt_end_date_->setObjectName(QStringLiteral("builderBtEnd"));
    bt_end_date_->setCalendarPopup(true);
    bt_end_date_->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    bt_end_date_->setMinimumDate(QDate(2000, 1, 1));
    bt_end_date_->setMaximumDate(today);
    bt_end_date_->setDate(today);

    bt_symbol_label_ = field_label(tr("Symbol"));
    bt_capital_label_ = field_label(tr("Capital"));
    bt_start_label_ = field_label(tr("From"));
    bt_end_label_ = field_label(tr("To"));

    auto* bt_grid = new QGridLayout();
    bt_grid->setHorizontalSpacing(8);
    bt_grid->setVerticalSpacing(8);
    bt_grid->setColumnStretch(1, 1);
    bt_grid->setColumnStretch(3, 1);
    bt_grid->addWidget(bt_symbol_label_, 0, 0);
    bt_grid->addWidget(symbol_combo_, 0, 1, 1, 3);
    bt_grid->addWidget(bt_capital_label_, 1, 0);
    bt_grid->addWidget(bt_capital_, 1, 1, 1, 3);
    bt_grid->addWidget(bt_start_label_, 2, 0);
    bt_grid->addWidget(bt_start_date_, 2, 1);
    bt_grid->addWidget(bt_end_label_, 2, 2);
    bt_grid->addWidget(bt_end_date_, 2, 3);
    setup->addLayout(bt_grid);

    backtest_btn_ = new QPushButton(tr("▶  RUN BACKTEST"), this);
    backtest_btn_->setObjectName(QStringLiteral("builderBacktestBtn"));
    backtest_btn_->setCursor(Qt::PointingHandCursor);
    backtest_btn_->setMinimumHeight(32);
    backtest_btn_->setStyleSheet(run_btn_style());
    setup->addWidget(backtest_btn_);

    // Transient feedback lives right under the run button, where the eye already is.
    status_label_ = new QLabel(this);
    status_label_->setObjectName(QStringLiteral("builderStatus"));
    status_label_->setWordWrap(true);
    status_label_->setStyleSheet(status_style());
    setup->addWidget(status_label_);

    layout->addWidget(setup_card);

    // ── F&O section (P2.3) — hidden until instrument type != equity ──────────
    build_fno_section();
    layout->addWidget(fno_section_);
    fno_section_->setVisible(false); // equity is default

    // ── RESULTS card — clearly separated from the controls above ─────────────
    auto* results_card = new QFrame(right);
    results_card->setObjectName(QStringLiteral("builderResultsCard"));
    results_card->setStyleSheet(QString("QFrame#builderResultsCard{background:%1;border:1px solid %2;"
                                        "border-radius:5px;}")
                                    .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* results = new QVBoxLayout(results_card);
    results->setContentsMargins(12, 10, 12, 12);
    results->setSpacing(8);

    results_header_ = new QLabel(tr("RESULTS"), this);
    results_header_->setObjectName(QStringLiteral("builderResultsHeader"));
    results_header_->setStyleSheet(card_header_style());
    results->addWidget(results_header_);

    report_panel_ = new ui::algo::BacktestReportPanel(this);
    results->addWidget(report_panel_, 1);

    layout->addWidget(results_card, 1);

    connect(backtest_btn_, &QPushButton::clicked, this, &StrategyBuilderPanel::on_backtest);

    return right;
}

// ── F&O Section (P2.3) ───────────────────────────────────────────────────────

void StrategyBuilderPanel::build_fno_section() {
    fno_section_ = new QWidget(this);
    fno_section_->setObjectName(QStringLiteral("builderFnoSection"));

    auto* layout = new QVBoxLayout(fno_section_);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    // ── Header row: underlying + expiry mode ──
    auto* header_row = new QHBoxLayout();
    header_row->setSpacing(8);

    underlying_combo_ = new QComboBox(fno_section_);
    underlying_combo_->setObjectName(QStringLiteral("builderFnoUnderlying"));
    underlying_combo_->setToolTip(tr("Underlying instrument"));
    underlying_combo_->setMinimumWidth(100);
    populate_underlyings();

    expiry_mode_combo_ = new QComboBox(fno_section_);
    expiry_mode_combo_->setObjectName(QStringLiteral("builderFnoExpiryMode"));
    expiry_mode_combo_->setToolTip(tr("Expiry selection rule"));
    expiry_mode_combo_->addItem(tr("Weekly"),      QStringLiteral("weekly"));
    expiry_mode_combo_->addItem(tr("Monthly"),     QStringLiteral("monthly"));
    expiry_mode_combo_->addItem(tr("Nearest DTE"), QStringLiteral("nearest_dte"));
    expiry_mode_combo_->addItem(tr("Absolute"),    QStringLiteral("absolute"));

    header_row->addWidget(field_label(tr("Underlying")));
    header_row->addWidget(underlying_combo_, 1);
    header_row->addWidget(field_label(tr("Expiry")));
    header_row->addWidget(expiry_mode_combo_, 1);
    layout->addLayout(header_row);

    // ── Leg rule editor ──
    leg_editor_ = new fincept::ui::algo::FnoLegRuleEditor(fno_section_);
    layout->addWidget(leg_editor_);

    // ── Analytics ribbon + payoff chart ──
    analytics_ribbon_ = new fincept::screens::fno::BuilderAnalyticsRibbon(fno_section_);
    layout->addWidget(analytics_ribbon_);

    payoff_chart_ = new fincept::screens::fno::PayoffChartWidget(fno_section_);
    payoff_chart_->setMinimumHeight(200);
    layout->addWidget(payoff_chart_, 1);

    // Connect all preview-trigger signals to refresh_fno_preview()
    connect(leg_editor_, &fincept::ui::algo::FnoLegRuleEditor::legs_changed,
            this, &StrategyBuilderPanel::refresh_fno_preview);
    connect(underlying_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StrategyBuilderPanel::refresh_fno_preview);
    connect(expiry_mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StrategyBuilderPanel::refresh_fno_preview);
    connect(&fincept::services::options::OptionChainService::instance(),
            &fincept::services::options::OptionChainService::chain_published,
            this, [this](const fincept::services::options::OptionChain&) {
                refresh_fno_preview();
            });
}

void StrategyBuilderPanel::populate_underlyings() {
    if (!underlying_combo_)
        return;
    const QStringList list =
        fincept::services::options::OptionChainService::instance().list_underlyings(fno_broker_id());
    {
        QSignalBlocker blocker(underlying_combo_);
        underlying_combo_->clear();
        if (list.isEmpty()) {
            underlying_combo_->addItem(tr("(connect broker)"));
            underlying_combo_->setEnabled(false);
        } else {
            for (const auto& u : list)
                underlying_combo_->addItem(u);
            underlying_combo_->setEnabled(true);
        }
    }
}

void StrategyBuilderPanel::refresh_fno_preview() {
    namespace an = fincept::services::options::analytics;
    const auto& chain = fincept::services::options::OptionChainService::instance().last_chain();
    const auto legs = leg_editor_->legs();
    if (legs.isEmpty() || chain.rows.isEmpty()) {
        analytics_ribbon_->clear();
        payoff_chart_->clear_payoff();
        return;
    }
    fincept::services::options::Strategy s = fincept::algo::fno::build_preview_strategy(legs, chain);
    an::PayoffComputeOptions opts;
    opts.current_spot = chain.spot;
    const auto curve = an::compute_payoff(s, opts);
    const auto bes = an::compute_breakevens(curve);
    const auto a = an::compute_all(s, chain, opts);
    payoff_chart_->set_payoff(curve, chain.spot, bes);
    analytics_ribbon_->update_from(s, a);
}

void StrategyBuilderPanel::on_instrument_type_changed() {
    if (!instrument_type_combo_)
        return;
    const QString type = instrument_type_combo_->currentData().toString();
    if (type == QLatin1String("equity")) {
        if (bt_setup_card_) bt_setup_card_->setVisible(true);
        if (fno_section_)   fno_section_->setVisible(false);
    } else {
        if (bt_setup_card_) bt_setup_card_->setVisible(false);
        if (fno_section_)   fno_section_->setVisible(true);
        if (underlying_combo_) populate_underlyings();
        refresh_fno_preview();
    }
}

QString StrategyBuilderPanel::fno_broker_id() const {
    // Mirror ChainSubTab: prefer a Connected account's broker_id, else "databento".
    for (const auto& acc : fincept::trading::AccountManager::instance().active_accounts()) {
        if (fincept::trading::AccountManager::instance().connection_state(acc.account_id) ==
            fincept::trading::ConnectionState::Connected)
            return acc.broker_id;
    }
    return QStringLiteral("databento");
}

void StrategyBuilderPanel::connect_service() {
    auto& svc = services::algo::AlgoTradingService::instance();
    connect(&svc, &services::algo::AlgoTradingService::backtest_result,
            this, &StrategyBuilderPanel::on_backtest_result);
    connect(&svc, &services::algo::AlgoTradingService::error_occurred,
            this, &StrategyBuilderPanel::on_error);
    connect(&svc, &services::algo::AlgoTradingService::strategy_saved,
            this, [this](const QString& id) {
                status_label_->setText(tr("Strategy saved: %1").arg(id));
            });
}

services::algo::AlgoStrategy StrategyBuilderPanel::build_strategy() {
    if (loaded_strategy_id_.isEmpty())
        loaded_strategy_id_ = QUuid::createUuid().toString(QUuid::WithoutBraces);

    services::algo::AlgoStrategy strat;
    strat.id = loaded_strategy_id_;
    strat.name = name_edit_->text().trimmed();
    strat.description = desc_edit_->text().trimmed();
    strat.timeframe = timeframe_combo_->currentText();
    strat.entry_conditions = entry_section_->conditions();
    strat.exit_conditions = exit_section_->conditions();
    strat.entry_logic = entry_section_->combined_logic();
    strat.exit_logic = exit_section_->combined_logic();
    strat.stop_loss = risk_panel_->stop_loss();
    strat.take_profit = risk_panel_->take_profit();
    strat.trailing_stop = risk_panel_->trailing_stop();
    strat.position_size_pct = risk_panel_->capital_pct();
    // P2.3: persist instrument type and F&O leg rules
    strat.instrument_type = instrument_type_combo_ ? instrument_type_combo_->currentData().toString()
                                                   : QStringLiteral("equity");
    if (strat.instrument_type != QLatin1String("equity") && leg_editor_)
        strat.legs = fincept::algo::fno::fno_legs_to_json(leg_editor_->legs());
    return strat;
}

void StrategyBuilderPanel::on_save() {
    if (name_edit_->text().trimmed().isEmpty()) {
        status_label_->setText(tr("Please enter a strategy name."));
        return;
    }

    services::algo::AlgoTradingService::instance().save_strategy(build_strategy());
    status_label_->setText(tr("Saving strategy..."));
}

void StrategyBuilderPanel::on_backtest() {
    // Backtesting is non-persisting, so it needs no name — only valid rules and
    // a sane date range. (Save/Deploy still require a name.)
    if (bt_start_date_->date() >= bt_end_date_->date()) {
        status_label_->setText(tr("Start date must be before end date."));
        return;
    }
    if (const QString err = validate(); !err.isEmpty()) {
        validation_banner_->setText(err);
        validation_banner_->setVisible(true);
        return;
    }
    validation_banner_->setVisible(false);

    // Backtest the current UI state directly, without persisting — backtesting is
    // non-mutating; the user saves explicitly via Save. (Avoids silently rewriting
    // library rows / bumping updated_at on every run.)
    auto strat = build_strategy();

    QString symbol = symbol_combo_->currentText().trimmed();
    if (symbol.isEmpty()) symbol = QStringLiteral("RELIANCE");

    services::algo::AlgoTradingService::instance().run_backtest(
        strat, symbol, bt_start_date_->date().toString(QStringLiteral("yyyy-MM-dd")),
        bt_end_date_->date().toString(QStringLiteral("yyyy-MM-dd")), bt_capital_->value());
    status_label_->setText(tr("Running backtest..."));
}

void StrategyBuilderPanel::on_deploy() {
    // Guards use a visible dialog/banner: the status label lives in the right-hand
    // backtest card, far from the Deploy button, so a silent setText() there reads
    // as "Deploy did nothing".
    if (name_edit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Deploy"),
                             tr("Enter a strategy name before deploying."));
        name_edit_->setFocus();
        return;
    }
    if (const QString err = validate(); !err.isEmpty()) {
        validation_banner_->setText(err);
        validation_banner_->setVisible(true);
        return;
    }
    validation_banner_->setVisible(false);

    // Build strategy from current UI state
    auto strat = build_strategy();

    auto* dialog = new AlgoDeployDialog(strat, this);
    dialog->set_symbol(symbol_combo_->currentText()); // carry the builder's symbol in
    if (instrument_type_combo_ && instrument_type_combo_->currentData().toString() != QLatin1String("equity")) {
        dialog->set_fno_context(instrument_type_combo_->currentData().toString(),
                                underlying_combo_ ? underlying_combo_->currentText() : QString(),
                                expiry_mode_combo_ ? expiry_mode_combo_->currentData().toString() : QString());
    }
    if (dialog->exec() == QDialog::Accepted) {
        auto deployment = dialog->deployment();
        deployment.quantity = risk_panel_->quantity();
        deployment.max_order_value = risk_panel_->max_order_value();

        // Sanity check (non-blocking, bounded): warn if a rule can't fire at the
        // current price — e.g. 'crosses_above 280.45' on a stock trading at 1204.
        const double cur_price = fetch_quote_ltp(deployment.broker_id, deployment.broker_account_id,
                                                 deployment.symbol, 2500);
        if (cur_price > 0) {
            QStringList warns;
            scan_unreachable(strat.entry_conditions, tr("Entry"), cur_price, warns);
            scan_unreachable(strat.exit_conditions, tr("Exit"), cur_price, warns);
            if (!warns.isEmpty()) {
                const auto btn = QMessageBox::warning(
                    this, tr("Deploy — check conditions"),
                    tr("%1 is at %2, but some rules may never trigger:\n\n%3\n\nDeploy anyway?")
                        .arg(deployment.symbol).arg(cur_price, 0, 'f', 2).arg(warns.join("\n\n")),
                    QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
                if (btn != QMessageBox::Yes) {
                    dialog->deleteLater();
                    return;
                }
            }
        }

        // Confirm before deploying an exact duplicate of an already-running setup.
        if (algo_ns::AlgoEngine::instance().has_active_duplicate(
                deployment.strategy_id, deployment.symbol, deployment.mode, deployment.entry_side)) {
            const auto btn = QMessageBox::question(
                this, tr("Already deployed"),
                tr("An identical deployment is already running:\n\n%1 · %2 · %3 · %4\n\n"
                   "Deploy another copy anyway?")
                    .arg(deployment.strategy_name, deployment.symbol, deployment.mode.toUpper(),
                         deployment.entry_side),
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
            if (btn != QMessageBox::Yes) {
                dialog->deleteLater();
                return;
            }
        }

        // Save strategy first
        services::algo::AlgoTradingService::instance().save_strategy(strat);

        LOG_INFO("AlgoTrading",
                 QString("Deploy requested: id=%1 strategy='%2' symbol=%3 mode=%4 backend=%5 "
                         "broker=%6 acct=%7 qty=%8")
                     .arg(deployment.id, strat.name, deployment.symbol, deployment.mode,
                          deployment.backend, deployment.broker_id, deployment.broker_account_id)
                     .arg(deployment.quantity));

        // Deploy via C++ engine (persists the row, then starts the runner). Feedback
        // is the jump to the Dashboard below — NOT a label on the backtest card, which
        // is unrelated to deployment and read as "stuck on Deploying…".
        algo_ns::AlgoEngine::instance().start_deployment(deployment, strat);

        // Tell the parent screen to switch to the Dashboard and refresh.
        emit deployed();
    }
    dialog->deleteLater();
}

void StrategyBuilderPanel::load_template(int index) {
    const auto tpls = services::algo::algo_strategy_templates();
    const int i = index - 1; // index 0 is the "Templates…" placeholder
    if (i < 0 || i >= tpls.size())
        return;
    const auto& t = tpls[i];

    loaded_strategy_id_.clear(); // a template seeds a brand-new strategy
    name_edit_->setText(t.name);
    desc_edit_->setText(t.description);
    timeframe_combo_->setCurrentText(t.timeframe);
    entry_section_->set_conditions(t.entry, t.entry_logic);
    exit_section_->set_conditions(t.exit, t.exit_logic);
    risk_panel_->set_values(t.stop_loss, t.take_profit, 0.0, 1.0, 0.0);
    report_panel_->clear();
    if (state_chip_) state_chip_->setText(tr("NEW DRAFT"));
    status_label_->setText(tr("Loaded template: %1").arg(t.name));
    template_combo_->setCurrentIndex(0); // reset to placeholder
}

void StrategyBuilderPanel::load_strategy(const services::algo::AlgoStrategy& s) {
    loaded_strategy_id_ = s.id; // keep id → Save updates this strategy in place
    name_edit_->setText(s.name);
    desc_edit_->setText(s.description);
    timeframe_combo_->setCurrentText(s.timeframe.isEmpty() ? QStringLiteral("5m") : s.timeframe);
    entry_section_->set_conditions(s.entry_conditions, s.entry_logic.isEmpty() ? QStringLiteral("AND") : s.entry_logic);
    exit_section_->set_conditions(s.exit_conditions, s.exit_logic.isEmpty() ? QStringLiteral("AND") : s.exit_logic);
    risk_panel_->set_values(s.stop_loss, s.take_profit, s.trailing_stop, 1.0, 0.0,
                            s.position_size_pct > 0 ? s.position_size_pct : 100.0);
    // P2.3: restore instrument type and F&O leg rules
    if (instrument_type_combo_) {
        const int idx = instrument_type_combo_->findData(s.instrument_type.isEmpty()
                                                             ? QStringLiteral("equity") : s.instrument_type);
        {
            QSignalBlocker blocker(instrument_type_combo_);
            instrument_type_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
        }
    }
    if (leg_editor_)
        leg_editor_->set_legs(fincept::algo::fno::fno_legs_from_json(s.legs));
    on_instrument_type_changed();
    // Size the backtest range to the strategy's timeframe (daily needs years for
    // long indicators; intraday must stay under Yahoo's history cap).
    const QDate today = QDate::currentDate();
    bt_start_date_->setDate(today.addDays(-services::algo::algo_default_lookback_days(s.timeframe)));
    bt_end_date_->setDate(today);
    report_panel_->clear();
    validation_banner_->setVisible(false);
    if (state_chip_) state_chip_->setText(tr("EDITING"));
    status_label_->setText(tr("Editing: %1").arg(s.name));
}

void StrategyBuilderPanel::load_and_backtest(const services::algo::AlgoStrategy& s,
                                             const QString& symbol, const QString& start_date,
                                             const QString& end_date) {
    load_strategy(s);
    if (!symbol.isEmpty())
        symbol_combo_->setCurrentText(symbol);
    const QDate sd = QDate::fromString(start_date, QStringLiteral("yyyy-MM-dd"));
    const QDate ed = QDate::fromString(end_date, QStringLiteral("yyyy-MM-dd"));
    if (sd.isValid())
        bt_start_date_->setDate(sd);
    if (ed.isValid())
        bt_end_date_->setDate(ed);
    on_backtest(); // reuses validation + service call; report renders in the right panel
}

QString StrategyBuilderPanel::validate() const {
    if (entry_section_->conditions().isEmpty())
        return tr("Add at least one entry condition.");
    const bool no_exit = exit_section_->conditions().isEmpty();
    if (no_exit && risk_panel_->stop_loss() <= 0.0 && risk_panel_->take_profit() <= 0.0)
        return tr("Strategy never exits: add an exit condition or set a stop-loss / take-profit.");
    return QString();
}

void StrategyBuilderPanel::on_backtest_result(const QJsonObject& payload) {
    status_label_->setText(tr("Backtest complete."));
    display_backtest_result(payload);
    LOG_INFO("AlgoTrading", "Backtest result displayed");
}

void StrategyBuilderPanel::on_error(const QString& context, const QString& msg) {
    status_label_->setText(QStringLiteral("[%1] %2").arg(context, msg));
}

void StrategyBuilderPanel::display_backtest_result(const QJsonObject& payload) {
    report_panel_->set_result(payload);
}

void StrategyBuilderPanel::clear_results() {
    report_panel_->clear();
}

QVariantMap StrategyBuilderPanel::save_draft() const {
    QVariantMap draft;
    draft["id"] = loaded_strategy_id_;
    draft["name"] = name_edit_->text();
    draft["description"] = desc_edit_->text();
    draft["symbol"] = symbol_combo_->currentText();
    draft["timeframe"] = timeframe_combo_->currentText();
    draft["entry_conditions"] = QJsonDocument(entry_section_->conditions()).toJson(QJsonDocument::Compact);
    draft["exit_conditions"] = QJsonDocument(exit_section_->conditions()).toJson(QJsonDocument::Compact);
    draft["entry_logic"] = entry_section_->combined_logic();
    draft["exit_logic"] = exit_section_->combined_logic();
    draft["stop_loss"] = risk_panel_->stop_loss();
    draft["take_profit"] = risk_panel_->take_profit();
    draft["trailing_stop"] = risk_panel_->trailing_stop();
    draft["quantity"] = risk_panel_->quantity();
    draft["max_order_value"] = risk_panel_->max_order_value();
    draft["capital_pct"] = risk_panel_->capital_pct();
    return draft;
}

void StrategyBuilderPanel::restore_draft(const QVariantMap& draft) {
    loaded_strategy_id_ = draft.value("id").toString();
    name_edit_->setText(draft.value("name").toString());
    desc_edit_->setText(draft.value("description").toString());
    symbol_combo_->setCurrentText(draft.value("symbol").toString());
    timeframe_combo_->setCurrentText(draft.value("timeframe", "5m").toString());

    auto entry_json = QJsonDocument::fromJson(draft.value("entry_conditions").toByteArray()).array();
    auto exit_json = QJsonDocument::fromJson(draft.value("exit_conditions").toByteArray()).array();
    entry_section_->set_conditions(entry_json, draft.value("entry_logic", "AND").toString());
    exit_section_->set_conditions(exit_json, draft.value("exit_logic", "AND").toString());

    risk_panel_->set_values(
        draft.value("stop_loss", 2.0).toDouble(),
        draft.value("take_profit", 5.0).toDouble(),
        draft.value("trailing_stop", 0.0).toDouble(),
        draft.value("quantity", 1.0).toDouble(),
        draft.value("max_order_value", 0.0).toDouble(),
        draft.value("capital_pct", 100.0).toDouble());
}

// ── Live language switch ──────────────────────────────────────────────────────

void StrategyBuilderPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void StrategyBuilderPanel::retranslateUi() {
    if (name_edit_) name_edit_->setPlaceholderText(tr("Strategy Name"));
    if (desc_edit_) desc_edit_->setPlaceholderText(tr("Description (optional)"));

    // template_combo_ item 0 is the "Templates…" placeholder; rest are data names.
    if (template_combo_ && template_combo_->count() > 0)
        template_combo_->setItemText(0, tr("Templates…"));

    if (save_btn_)     save_btn_->setText(tr("Save"));
    if (backtest_btn_) backtest_btn_->setText(tr("▶  RUN BACKTEST"));
    if (deploy_btn_)   deploy_btn_->setText(tr("Deploy ▸"));

    if (bt_header_)        bt_header_->setText(tr("BACKTEST SETUP"));
    if (bt_symbol_label_)  bt_symbol_label_->setText(tr("Symbol"));
    if (bt_capital_label_) bt_capital_label_->setText(tr("Capital"));
    if (bt_start_label_)   bt_start_label_->setText(tr("From"));
    if (bt_end_label_)     bt_end_label_->setText(tr("To"));
    if (results_header_)   results_header_->setText(tr("RESULTS"));
    // status_label_ carries transient action feedback — left as-is on language switch.
    // Child widgets (ConditionSection / RiskManagementPanel / BacktestReportPanel)
    // own their own retranslate via their changeEvent overrides.
}

} // namespace fincept::screens
