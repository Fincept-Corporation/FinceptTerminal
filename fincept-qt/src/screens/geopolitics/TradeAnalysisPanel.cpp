// src/screens/geopolitics/TradeAnalysisPanel.cpp
#include "screens/geopolitics/TradeAnalysisPanel.h"

#include "services/geopolitics/GeopoliticsService.h"
#include "ui/charts/BarChart.h"
#include "ui/charts/DonutChart.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/markdown/MarkdownRenderer.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/Card.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/StatTile.h"
#include "ui/widgets/MnemonicText.h"

#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QSet>
#include <QTableWidget>

namespace fincept::screens {

using namespace fincept::services::geo;

TradeAnalysisPanel::TradeAnalysisPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
    connect_service();
}

void TradeAnalysisPanel::connect_service() {
    auto& svc = GeopoliticsService::instance();
    connect(&svc, &GeopoliticsService::trade_result_ready, this, &TradeAnalysisPanel::on_trade_result);
    connect(&svc, &GeopoliticsService::error_occurred, this, &TradeAnalysisPanel::on_error);
}

// ── Shared style helpers ──────────────────────────────────────────────────────
static QString combo_style() {
    return QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                   "font-family:%4; font-size:%5px; padding:4px 8px; }"
                   "QComboBox:focus { border-color:%6; }"
                   "QComboBox::drop-down { border:none; width:18px; }"
                   "QComboBox QAbstractItemView { background:%1; color:%2; "
                   "selection-background-color:%7; border:1px solid %3; }")
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
        .arg(ui::fonts::DATA_FAMILY)
        .arg(ui::fonts::SMALL.get())
        .arg(ui::colors::WARNING())
        .arg(ui::colors::alpha(ui::colors::WARNING, 0.15));
}

/// RUN ANALYSIS: a filled warning-colour button.
static QString run_button_style() {
    return QString("QPushButton { background:%1; color:%2; font-family:%3; font-size:%4px;"
                   "font-weight:700; border:none; padding:6px 16px; }"
                   "QPushButton:hover { background:%5; }")
        .arg(ui::colors::WARNING())
        .arg(ui::colors::TEXT_ON_ACCENT())
        .arg(ui::fonts::DATA_FAMILY())
        .arg(ui::fonts::SMALL.get())
        .arg(ui::colors::alpha(ui::colors::WARNING, 0.85));
}

static QString spin_style() {
    return QString("QDoubleSpinBox { background:%1; color:%2; border:1px solid %3;"
                   "font-family:%4; font-size:%5px; padding:4px 6px; }"
                   "QDoubleSpinBox:focus { border-color:%6; }")
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
        .arg(ui::fonts::DATA_FAMILY)
        .arg(ui::fonts::SMALL.get())
        .arg(ui::colors::WARNING());
}

namespace {
/// Per analysis (tab index): the service context its result arrives under and
/// the pane view key (also the persistent cache sub-key).
constexpr const char* kTapContexts[] = {"trade_benefits", "trade_restrictions", "trade_blocs", "trade_barrier"};
constexpr const char* kTapViews[] = {"benefits_costs", "restrictions", "trading_blocs", "barrier_removal"};
constexpr int kTapCount = 4;

int tap_index_of_context(const QString& context) {
    for (int i = 0; i < kTapCount; ++i)
        if (context == QLatin1String(kTapContexts[i]))
            return i;
    return -1;
}
} // namespace

void TradeAnalysisPanel::clear_results() {
    while (results_layout_->count() > 0) {
        auto* item = results_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
}

static QWidget* make_field(const QString& label_text, QWidget* input, QWidget* parent, const QString& hint = {}) {
    auto* group = new QWidget(parent);
    auto* vl = new QVBoxLayout(group);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(3);

    auto* lbl = new QLabel(label_text, group);
    lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3; letter-spacing:1px;")
                           .arg(ui::colors::TEXT_SECONDARY())
                           .arg(ui::fonts::TINY.get())
                           .arg(ui::fonts::DATA_FAMILY));
    vl->addWidget(lbl);

    input->setParent(group);
    vl->addWidget(input);

    if (!hint.isEmpty()) {
        auto* h = new QLabel(hint, group);
        h->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                             .arg(ui::colors::TEXT_TERTIARY())
                             .arg(ui::fonts::TINY.get())
                             .arg(ui::fonts::DATA_FAMILY));
        h->setWordWrap(true);
        vl->addWidget(h);
    }
    return group;
}

/// Put a remembered value back into a spin box / combo without firing its signals.
static void tap_set_spin(QDoubleSpinBox* spin, const QJsonObject& in, const char* key) {
    const QJsonValue v = in.value(QLatin1String(key));
    if (!v.isDouble())
        return;
    const QSignalBlocker block(spin);
    spin->setValue(v.toDouble());
}

static void tap_set_combo(QComboBox* combo, const QJsonObject& in, const char* key) {
    const int idx = combo->findData(in.value(QLatin1String(key)).toString());
    if (idx < 0)
        return;
    const QSignalBlocker block(combo);
    combo->setCurrentIndex(idx);
}

void TradeAnalysisPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header
    auto* header = new QWidget(this);
    header->setMinimumHeight(48);
    header->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(16, 0, 16, 0);
    title_lbl_ = new QLabel(tr("TRADE GEOPOLITICS ANALYSIS"), header);
    title_lbl_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3; letter-spacing:1px;")
                                  .arg(ui::colors::WARNING())
                                  .arg(ui::fonts::TINY.get())
                                  .arg(ui::fonts::DATA_FAMILY()));
    hhl->addWidget(title_lbl_);
    hhl->addStretch();
    // "as of 14:02", "as of 14:02 · refreshing…" — kept by the pane.
    status_label_ = new QLabel(header);
    status_label_->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                     .arg(ui::colors::TEXT_SECONDARY())
                                     .arg(ui::fonts::SMALL.get())
                                     .arg(ui::fonts::DATA_FAMILY));
    hhl->addWidget(status_label_);
    root->addWidget(header);

    // Body: inputs (left, scrolls on its own) | result pane (right).
    auto* body = new QWidget(this);
    auto* bhl = new QHBoxLayout(body);
    bhl->setContentsMargins(0, 0, 0, 0);
    bhl->setSpacing(0);
    root->addWidget(body, 1);

    auto* scroll = new QScrollArea(body);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFixedWidth(std::max(340, QFontMetrics(ui::fonts::font(-1)).horizontalAdvance(QString(36, QLatin1Char('M')))));
    scroll->setStyleSheet(QString("QScrollArea { border:none; border-right:1px solid %1; background:transparent; }")
                              .arg(ui::colors::BORDER_DIM()));
    bhl->addWidget(scroll);

    auto* content = new QWidget(scroll);
    auto* cvl = new QVBoxLayout(content);
    cvl->setContentsMargins(16, 16, 16, 16);
    cvl->setSpacing(12);

    // ── Analysis Type selector ────────────────────────────────────────────────
    type_combo_ = new QComboBox;
    type_combo_->setStyleSheet(combo_style());
    type_combo_->addItem(tr("Benefits & Costs of Trade"), "benefits_costs");
    type_combo_->addItem(tr("Trade Restrictions Analysis"), "restrictions");
    type_combo_->addItem(tr("Trading Blocs Analysis"), "trading_blocs");
    type_combo_->addItem(tr("Trade Barrier Removal Impact"), "barrier_removal");
    auto* type_combo = type_combo_; // keep local name for the signal wiring below
    cvl->addWidget(make_field(tr("ANALYSIS TYPE"), type_combo, content));

    // ── Tab widget for params per analysis type ───────────────────────────────
    tabs_ = new QTabWidget(content);
    tabs_->setStyleSheet(QString("QTabWidget::pane { border:1px solid %1; background:%2; }"
                                 "QTabBar::tab { background:%3; color:%4; padding:5px 10px;"
                                 "font-family:%5; font-size:%6px; border:1px solid %1; border-bottom:none; }"
                                 "QTabBar::tab:selected { background:%2; color:%7; font-weight:700;"
                                 "border-bottom:2px solid %7; }")
                             .arg(ui::colors::BORDER_DIM(), ui::colors::BG_SURFACE(), ui::colors::BG_RAISED())
                             .arg(ui::colors::TEXT_SECONDARY())
                             .arg(ui::fonts::DATA_FAMILY)
                             .arg(ui::fonts::SMALL.get())
                             .arg(ui::colors::WARNING()));
    tabs_->setUsesScrollButtons(true);

    // Result area: the shared AutoLoadPane (free, local calculation → AutoRun).
    pane_ = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::AutoRun, body);
    auto* results_scroll = new QScrollArea(pane_->content_parent());
    results_scroll->setWidgetResizable(true);
    results_scroll->setFrameShape(QFrame::NoFrame);
    results_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    results_container_ = new QWidget(results_scroll);
    results_layout_ = new QVBoxLayout(results_container_);
    results_layout_->setContentsMargins(16, 12, 16, 16);
    results_layout_->setSpacing(8);
    results_scroll->setWidget(results_container_);
    pane_->set_content(results_scroll);
    bhl->addWidget(pane_, 1);

    // Every page: a hint, its fields, then RUN ANALYSIS (re-runs the pane).
    auto add_page = [&](const QString& hint_tr, const char* hint_src, const QString& tab_text) {
        auto* p = new QWidget(tabs_);
        auto* pl = new QVBoxLayout(p);
        pl->setContentsMargins(12, 12, 12, 12);
        pl->setSpacing(10);
        auto* hint = new QLabel(hint_tr, p);
        hint->setWordWrap(true);
        hint->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                .arg(ui::colors::TEXT_SECONDARY())
                                .arg(ui::fonts::SMALL.get())
                                .arg(ui::fonts::DATA_FAMILY));
        i18n_labels_.append({hint, QString::fromUtf8(hint_src)});
        pl->addWidget(hint);
        tabs_->addTab(p, tab_text);
        return std::pair<QWidget*, QVBoxLayout*>{p, pl};
    };
    auto add_run = [&](QWidget* p, QVBoxLayout* pl) {
        pl->addStretch();
        auto* run = new QPushButton(tr("RUN ANALYSIS"), p);
        run->setCursor(Qt::PointingHandCursor);
        run->setStyleSheet(run_button_style());
        connect(run, &QPushButton::clicked, pane_, &ui::AutoLoadPane::run);
        pl->addWidget(run);
        run_buttons_.append(run);
    };

    // ── Page 0: Benefits & Costs ──────────────────────────────────────────────
    {
        auto [p0, p0l] = add_page(tr("Analyzes efficiency gains, consumer benefits, growth effects, and adjustment "
                                     "costs of international trade."),
                                  "Analyzes efficiency gains, consumer benefits, growth effects, and adjustment costs "
                                  "of international trade.",
                                  fincept::ui::literal_ampersands(tr("Benefits & Costs")));
        auto* vol_spin = new QDoubleSpinBox;
        vol_spin->setRange(0, 100);
        vol_spin->setValue(30);
        vol_spin->setSuffix(tr("% of GDP"));
        vol_spin->setStyleSheet(spin_style());
        p0l->addWidget(make_field(tr("TRADE VOLUME"), vol_spin, p0, tr("Trade as % of GDP (world avg ~60%)")));
        auto* price_spin = new QDoubleSpinBox;
        price_spin->setRange(0, 50);
        price_spin->setValue(5);
        price_spin->setSuffix("%");
        price_spin->setStyleSheet(spin_style());
        p0l->addWidget(make_field(tr("EXPECTED PRICE REDUCTION"), price_spin, p0,
                                  tr("Consumer price reduction from trade openness")));
        auto* cons_spin = new QDoubleSpinBox;
        cons_spin->setRange(0, 100);
        cons_spin->setValue(30);
        cons_spin->setSuffix(tr("% of consumption"));
        cons_spin->setStyleSheet(spin_style());
        p0l->addWidget(
            make_field(tr("TRADED GOODS SHARE"), cons_spin, p0, tr("Share of consumption from tradeable goods")));
        add_run(p0, p0l);
        params_fns_.append([vol_spin, price_spin, cons_spin]() {
            QJsonObject p;
            p["trade_volume_gdp"] = vol_spin->value();
            p["price_reduction_percent"] = price_spin->value();
            p["traded_goods_consumption"] = cons_spin->value();
            return p;
        });
        apply_fns_.append([vol_spin, price_spin, cons_spin](const QJsonObject& in) {
            tap_set_spin(vol_spin, in, "trade_volume_gdp");
            tap_set_spin(price_spin, in, "price_reduction_percent");
            tap_set_spin(cons_spin, in, "traded_goods_consumption");
        });
    }

    // ── Page 1: Restrictions ──────────────────────────────────────────────────
    {
        auto [p1, p1l] = add_page(
            tr("Analyzes economic impact of tariffs, quotas, export subsidies, and non-tariff barriers."),
            "Analyzes economic impact of tariffs, quotas, export subsidies, and non-tariff barriers.",
            tr("Restrictions"));
        auto* tariff_spin = new QDoubleSpinBox;
        tariff_spin->setRange(0, 200);
        tariff_spin->setValue(10);
        tariff_spin->setSuffix("%");
        tariff_spin->setStyleSheet(spin_style());
        p1l->addWidget(
            make_field(tr("TARIFF RATE"), tariff_spin, p1, tr("Import duty rate (e.g. 10% = standard MFN tariff)")));
        // No QUOTA VOLUME input: a quantity limit alone (without demand and world-price
        // data) cannot be turned into a tariff equivalent or quota rent, so the
        // analysis treats quotas qualitatively and the field changed nothing.
        auto* subsidy_spin = new QDoubleSpinBox;
        subsidy_spin->setRange(0, 100);
        subsidy_spin->setValue(5);
        subsidy_spin->setSuffix("%");
        subsidy_spin->setStyleSheet(spin_style());
        p1l->addWidget(
            make_field(tr("EXPORT SUBSIDY RATE"), subsidy_spin, p1, tr("Government subsidy as % of export value")));
        auto* dev_combo = new QComboBox;
        dev_combo->setStyleSheet(combo_style());
        dev_combo->addItem(tr("Developed Economy"), "developed");
        dev_combo->addItem(tr("Developing Economy"), "developing");
        dev_combo->addItem(tr("Middle Income"), "middle");
        p1l->addWidget(make_field(tr("DEVELOPMENT LEVEL"), dev_combo, p1));
        auto* maturity_combo = new QComboBox;
        maturity_combo->setStyleSheet(combo_style());
        maturity_combo->addItem(tr("Mature Industry"), "mature");
        maturity_combo->addItem(tr("Infant Industry"), "infant");
        maturity_combo->addItem(tr("Emerging Industry"), "emerging");
        p1l->addWidget(make_field(tr("INDUSTRY MATURITY"), maturity_combo, p1));
        add_run(p1, p1l);
        params_fns_.append([tariff_spin, subsidy_spin, dev_combo, maturity_combo]() {
            QJsonObject p;
            p["tariff_rate"] = tariff_spin->value();
            p["subsidy_rate"] = subsidy_spin->value();
            p["development_level"] = dev_combo->currentData().toString();
            p["industry_maturity"] = maturity_combo->currentData().toString();
            return p;
        });
        apply_fns_.append([tariff_spin, subsidy_spin, dev_combo, maturity_combo](const QJsonObject& in) {
            tap_set_spin(tariff_spin, in, "tariff_rate");
            tap_set_spin(subsidy_spin, in, "subsidy_rate");
            tap_set_combo(dev_combo, in, "development_level");
            tap_set_combo(maturity_combo, in, "industry_maturity");
        });
    }

    // ── Page 2: Trading Blocs ─────────────────────────────────────────────────
    {
        auto [p2, p2l] = add_page(
            tr("Analyzes trade creation vs. diversion effects for regional trade blocs and economic unions."),
            "Analyzes trade creation vs. diversion effects for regional trade blocs and economic unions.",
            tr("Trading Blocs"));
        auto* bloc_combo = new QComboBox;
        bloc_combo->setStyleSheet(combo_style());
        bloc_combo->addItem(tr("Free Trade Area (e.g. USMCA, ASEAN)"), "fta");
        bloc_combo->addItem(tr("Customs Union (e.g. EU, Mercosur)"), "customs_union");
        bloc_combo->addItem(tr("Common Market (e.g. EU Single Market)"), "common_market");
        bloc_combo->addItem(tr("Economic Union (e.g. European Union)"), "economic_union");
        p2l->addWidget(make_field(tr("INTEGRATION TYPE"), bloc_combo, p2));
        auto* tc_spin = new QDoubleSpinBox;
        tc_spin->setRange(0, 1000);
        tc_spin->setValue(100);
        tc_spin->setSuffix(QLatin1Char(' ') + fincept::ui::formatting::scaled_unit(
                                                  QStringLiteral("USD"), fincept::ui::formatting::Scale::Billion));
        tc_spin->setStyleSheet(spin_style());
        p2l->addWidget(
            make_field(tr("TRADE CREATION ESTIMATE"), tc_spin, p2, tr("Expected new trade generated within bloc")));
        auto* td_spin = new QDoubleSpinBox;
        td_spin->setRange(0, 1000);
        td_spin->setValue(30);
        td_spin->setSuffix(QLatin1Char(' ') + fincept::ui::formatting::scaled_unit(
                                                  QStringLiteral("USD"), fincept::ui::formatting::Scale::Billion));
        td_spin->setStyleSheet(spin_style());
        p2l->addWidget(
            make_field(tr("TRADE DIVERSION ESTIMATE"), td_spin, p2, tr("Trade diverted from efficient non-members")));
        add_run(p2, p2l);
        params_fns_.append([bloc_combo, tc_spin, td_spin]() {
            QJsonObject p;
            p["integration_type"] = bloc_combo->currentData().toString();
            p["trade_creation"] = tc_spin->value();
            p["trade_diversion"] = td_spin->value();
            return p;
        });
        apply_fns_.append([bloc_combo, tc_spin, td_spin](const QJsonObject& in) {
            tap_set_combo(bloc_combo, in, "integration_type");
            tap_set_spin(tc_spin, in, "trade_creation");
            tap_set_spin(td_spin, in, "trade_diversion");
        });
    }

    // ── Page 3: Barrier Removal ───────────────────────────────────────────────
    {
        auto [p3, p3l] =
            add_page(tr("Assesses FDI, employment, wage, and GDP impact of removing trade barriers."),
                     "Assesses FDI, employment, wage, and GDP impact of removing trade barriers.",
                     tr("Barrier Removal"));
        auto* lib_combo = new QComboBox;
        lib_combo->setStyleSheet(combo_style());
        lib_combo->addItem(tr("Unilateral Liberalization"), "unilateral");
        lib_combo->addItem(tr("Bilateral Agreement"), "bilateral");
        lib_combo->addItem(tr("Regional Agreement"), "regional");
        lib_combo->addItem(tr("Multilateral (WTO Round)"), "multilateral");
        p3l->addWidget(make_field(tr("LIBERALIZATION SCOPE"), lib_combo, p3));
        auto* tariff_cut_spin = new QDoubleSpinBox;
        tariff_cut_spin->setRange(0, 100);
        tariff_cut_spin->setValue(50);
        tariff_cut_spin->setSuffix(tr("% reduction"));
        tariff_cut_spin->setStyleSheet(spin_style());
        p3l->addWidget(make_field(tr("TARIFF REDUCTION"), tariff_cut_spin, p3, tr("% cut in existing tariff rates")));
        auto* gdp_spin = new QDoubleSpinBox;
        gdp_spin->setRange(0, 100000);
        gdp_spin->setValue(500);
        gdp_spin->setSuffix(QLatin1Char(' ') + fincept::ui::formatting::scaled_unit(
                                                   QStringLiteral("USD"), fincept::ui::formatting::Scale::Billion));
        gdp_spin->setStyleSheet(spin_style());
        p3l->addWidget(make_field(tr("ECONOMY SIZE (GDP)"), gdp_spin, p3));
        add_run(p3, p3l);
        params_fns_.append([lib_combo, tariff_cut_spin, gdp_spin]() {
            QJsonObject p;
            const QString type = lib_combo->currentData().toString();
            p["liberalization_type"] = type;
            p["tariff_reduction"] = tariff_cut_spin->value();
            p["gdp_size"] = gdp_spin->value();
            // The backend's FDI estimate is keyed on a limited / moderate / comprehensive
            // scope: the more parties to the agreement, the broader the scope.
            p["liberalization_scope"] = type == QLatin1String("unilateral")     ? QStringLiteral("limited")
                                        : type == QLatin1String("multilateral") ? QStringLiteral("comprehensive")
                                                                                : QStringLiteral("moderate");
            return p;
        });
        apply_fns_.append([lib_combo, tariff_cut_spin, gdp_spin](const QJsonObject& in) {
            tap_set_combo(lib_combo, in, "liberalization_type");
            tap_set_spin(tariff_cut_spin, in, "tariff_reduction");
            tap_set_spin(gdp_spin, in, "gdp_size");
        });
    }

    // Sync analysis type combo → tab
    connect(type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), tabs_, &QTabWidget::setCurrentIndex);
    connect(tabs_, &QTabWidget::currentChanged, type_combo, &QComboBox::setCurrentIndex);
    // Each analysis is its own pane view: its own remembered result (never the
    // tiles of another analysis under these inputs), its own first-show run.
    connect(tabs_, &QTabWidget::currentChanged, this, [this](int index) {
        if (index >= 0 && index < kTapCount)
            pane_->set_view(QString::fromLatin1(kTapViews[index]));
    });

    cvl->addWidget(tabs_);
    cvl->addStretch();
    scroll->setWidget(content);

    pane_->set_cache_key(QStringLiteral("geopolitics/trade"));
    pane_->set_loading_text(tr("RUNNING ANALYSIS…"));
    pane_->set_status_label(status_label_);
    pane_->set_auto_delay(250); // flicking through the sub-tabs runs only the one left open
    pane_->set_start([this](quint64 id) { start_analysis(id); });
    pane_->set_cancel([this](quint64 id) {
        for (auto it = pending_.begin(); it != pending_.end(); ++it) {
            if (it.value() == id) {
                pending_inputs_.remove(it.key());
                pending_.erase(it);
                break;
            }
        }
    });
    pane_->set_render([this](const QJsonValue& payload, const QDateTime&, bool from_cache) {
        render_payload(payload, from_cache);
    });
    // One Python run at a time from this panel: RUN waits for the one in flight.
    connect(pane_, &ui::AutoLoadPane::running_changed, this, [this](bool running) {
        for (auto* btn : run_buttons_)
            if (btn)
                btn->setEnabled(!running);
    });
    pane_->set_view(QString::fromLatin1(kTapViews[0]));
}

void TradeAnalysisPanel::start_analysis(quint64 id) {
    const int tab = tabs_ ? tabs_->currentIndex() : 0;
    if (tab < 0 || tab >= kTapCount || tab >= params_fns_.size()) {
        pane_->fail(id, tr("Unknown analysis."));
        return;
    }
    const QString context = QString::fromLatin1(kTapContexts[tab]);
    const QJsonObject params = params_fns_[tab]();
    pending_[context] = id;
    pending_inputs_[context] = params;
    auto& svc = GeopoliticsService::instance();
    switch (tab) {
        case 0:
            svc.analyze_trade_benefits(params);
            break;
        case 1:
            svc.analyze_trade_restrictions(params);
            break;
        case 2:
            // Dedicated trading_blocs mode: reads trade_creation / trade_diversion.
            svc.analyze_trading_blocs(params);
            break;
        default:
            svc.analyze_barrier_removal(params);
            break;
    }
}

void TradeAnalysisPanel::render_payload(const QJsonValue& payload, bool from_cache) {
    const QJsonObject o = payload.toObject();
    const int tab = tabs_ ? tabs_->currentIndex() : 0;
    if (tab < 0 || tab >= kTapCount)
        return;
    // A result remembered from an earlier session brings its inputs back, so the
    // form and the result agree; within a session the form keeps what was typed.
    if (from_cache && tab < apply_fns_.size())
        apply_fns_[tab](o.value(QStringLiteral("inputs")).toObject());
    display_result(QString::fromLatin1(kTapContexts[tab]), o.value(QStringLiteral("result")).toObject());
}

namespace {

// "trade_creation_vs_diversion" -> "Trade creation vs diversion".
QString trade_humanize(QString key) {
    key.replace(QLatin1Char('_'), QLatin1Char(' '));
    if (!key.isEmpty())
        key[0] = key[0].toUpper();
    return key;
}

/// A JSON value as Markdown bullet lines (nested objects become sub-lists).
void trade_markdown(const QJsonValue& v, int depth, QString& out) {
    namespace fmtx = fincept::ui::formatting;
    const QString indent(depth * 2, QLatin1Char(' '));
    auto scalar = [](const QJsonValue& x) -> QString {
        if (x.isDouble())
            return fmtx::number(x.toDouble(), 2);
        if (x.isBool())
            return x.toBool() ? QCoreApplication::translate("TradeAnalysisPanel", "Yes")
                              : QCoreApplication::translate("TradeAnalysisPanel", "No");
        return x.toString();
    };
    if (v.isObject()) {
        const QJsonObject o = v.toObject();
        for (auto it = o.begin(); it != o.end(); ++it) {
            if (it.value().isObject() || it.value().isArray()) {
                out += indent + QStringLiteral("- **%1**\n").arg(trade_humanize(it.key()));
                trade_markdown(it.value(), depth + 1, out);
            } else {
                out += indent + QStringLiteral("- **%1:** %2\n").arg(trade_humanize(it.key()), scalar(it.value()));
            }
        }
    } else if (v.isArray()) {
        for (const auto& x : v.toArray()) {
            if (x.isObject() || x.isArray())
                trade_markdown(x, depth + 1, out);
            else
                out += indent + QStringLiteral("- %1\n").arg(scalar(x));
        }
    } else {
        out += indent + QStringLiteral("- %1\n").arg(scalar(v));
    }
}

} // namespace

void TradeAnalysisPanel::display_result(const QString& context, const QJsonObject& payload) {
    namespace fmtx = fincept::ui::formatting;
    using Tone = ui::StatTile::Tone;
    clear_results();

    auto* hdr = new QLabel(tr("ANALYSIS RESULTS"));
    hdr->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;"
                               "letter-spacing:1px; padding:8px 0 4px 0;")
                           .arg(ui::colors::WARNING())
                           .arg(ui::fonts::TINY.get())
                           .arg(ui::fonts::DATA_FAMILY()));
    results_layout_->addWidget(hdr);

    const QJsonObject h = payload.value(QStringLiteral("headline")).toObject();
    auto num = [&](const char* k) { return h.value(QLatin1String(k)).toDouble(); };
    auto pct = [](double v, bool signed_ = false) { return fmtx::pct(v, 2, signed_); };
    // The dataset is in billions of USD: always USD (never converted), abbreviated per the user's scheme.
    auto busd = [](double v) { return fmtx::money(v * 1e9, QStringLiteral("USD"), fmtx::Compact::FromSettings); };
    auto add_strip = [&]() {
        auto* s = new ui::KpiStrip(results_container_);
        results_layout_->addWidget(s);
        return s;
    };
    auto add_tile = [](ui::KpiStrip* s, const QString& label, const QString& value, Tone tone = Tone::Neutral,
                       const QString& sub = {}) {
        auto* t = s->add_tile(label);
        t->set_value(value);
        t->set_tone(tone);
        if (!sub.isEmpty())
            t->set_sub_label(sub);
        return t;
    };
    // A card holding Markdown (bullet lists of the script's sentences).
    auto add_md_card = [&](const QString& title, const QString& markdown) {
        if (markdown.trimmed().isEmpty())
            return;
        auto* card = new ui::Card(title, results_container_);
        auto* body = new QLabel(card);
        body->setTextFormat(Qt::RichText);
        body->setWordWrap(true);
        body->setTextInteractionFlags(Qt::TextSelectableByMouse);
        body->setText(ui::MarkdownRenderer::render(markdown));
        card->content_layout()->addWidget(body);
        connect(card, &ui::Card::close_requested, card, &QWidget::hide);
        results_layout_->addWidget(card);
    };
    auto section_md = [&](const char* key) {
        QString md;
        if (payload.contains(QLatin1String(key)))
            trade_markdown(payload.value(QLatin1String(key)), 0, md);
        return md;
    };

    QSet<QString> handled = {QStringLiteral("headline"), QStringLiteral("metadata"), QStringLiteral("error")};
    if (context == QLatin1String("trade_benefits") && !h.isEmpty()) {
        auto* s = add_strip();
        add_tile(s, tr("EFFICIENCY GAIN"), pct(num("efficiency_gain_pct_gdp")), Tone::Positive,
                 tr("vs autarky, % of real income"));
        add_tile(s, tr("CONSUMER SURPLUS GAIN"), pct(num("consumer_surplus_gain_pct")), Tone::Positive,
                 tr("% of consumption"));
        add_tile(s, tr("TRADE VOLUME"), pct(num("trade_volume_pct_gdp")), Tone::Neutral, tr("% of GDP (input)"));
        add_md_card(tr("BENEFITS"), section_md("trade_benefits"));
        add_md_card(tr("COSTS"), section_md("trade_costs"));
        add_md_card(tr("NET WELFARE"), section_md("net_welfare_assessment"));
        handled << QStringLiteral("trade_benefits") << QStringLiteral("trade_costs")
                << QStringLiteral("net_welfare_assessment");
    } else if (context == QLatin1String("trade_restrictions") && !h.isEmpty()) {
        auto* s = add_strip();
        add_tile(s, tr("PRICE RISE (TARIFF)"), pct(num("tariff_price_rise_pct"), true), Tone::Negative);
        add_tile(s, tr("IMPORTS (TARIFF)"), pct(-num("tariff_import_fall_pct"), true), Tone::Negative,
                 tr("elasticity 1.5"));
        add_tile(s, tr("DOMESTIC OUTPUT"), pct(num("tariff_domestic_output_pct"), true), Tone::Positive);
        // The script reports the loss as a share of GDP only when the caller supplied the
        // import share; otherwise it is null and the share of the import bill is shown.
        const QJsonValue dwl_gdp = h.value(QStringLiteral("tariff_deadweight_loss_pct_gdp"));
        if (dwl_gdp.isDouble())
            add_tile(s, tr("DEADWEIGHT LOSS"), pct(dwl_gdp.toDouble()), Tone::Warning, tr("% of GDP"));
        else
            add_tile(s, tr("DEADWEIGHT LOSS"), pct(num("tariff_deadweight_loss_pct_imports")), Tone::Warning,
                     tr("% of pre-tariff import bill"));
        add_tile(s, tr("EXPORTS (SUBSIDY)"), pct(num("subsidy_export_rise_pct"), true), Tone::Positive);
        add_tile(s, tr("FISCAL COST"), pct(num("subsidy_fiscal_cost_pct_exports")), Tone::Warning,
                 tr("% of export value"));
        // Tariff vs export subsidy on the two effects both share (percent changes).
        auto* chart = new ui::BarChart(results_container_);
        chart->setMinimumHeight(200);
        chart->set_categories({tr("Domestic price"), tr("Trade volume")});
        chart->set_series({{tr("Tariff"), {num("tariff_price_rise_pct"), -num("tariff_import_fall_pct")}, QColor()},
                           {tr("Export subsidy"),
                            {num("subsidy_domestic_price_rise_pct"), num("subsidy_export_rise_pct")},
                            QColor()}});
        chart->set_show_values(true);
        chart->set_value_formatter([](double v) { return fmtx::pct(v, 1); });
        results_layout_->addWidget(chart);
        add_md_card(tr("TARIFFS"), section_md("tariffs"));
        add_md_card(tr("QUOTAS"), section_md("quotas"));
        add_md_card(tr("EXPORT SUBSIDIES"), section_md("export_subsidies"));
        add_md_card(tr("NON-TARIFF BARRIERS"), section_md("non_tariff_barriers"));
        add_md_card(tr("COMPARISON"), section_md("restriction_comparison"));
        add_md_card(tr("RECOMMENDATION"), section_md("optimal_policy_recommendation"));
        handled << QStringLiteral("tariffs") << QStringLiteral("quotas") << QStringLiteral("export_subsidies")
                << QStringLiteral("non_tariff_barriers") << QStringLiteral("restriction_comparison")
                << QStringLiteral("optimal_policy_recommendation");
    } else if (context == QLatin1String("trade_blocs") && !h.isEmpty()) {
        const double creation = num("trade_creation_busd");
        const double diversion = num("trade_diversion_busd");
        const double net = num("net_trade_effect_busd");
        auto* s = add_strip();
        add_tile(s, tr("TRADE CREATION"), busd(creation), Tone::Positive);
        add_tile(s, tr("TRADE DIVERSION"), busd(diversion), Tone::Negative);
        add_tile(s, tr("NET EFFECT"), busd(net), net > 0 ? Tone::Positive : (net < 0 ? Tone::Negative : Tone::Neutral),
                 tr("creation − diversion"));
        auto* donut = new ui::DonutChart(results_container_);
        donut->setMinimumHeight(200);
        donut->set_slices({{QStringLiteral("creation"), tr("Trade creation"), creation, QColor(ui::colors::POSITIVE())},
                           {QStringLiteral("diversion"), tr("Trade diversion"), diversion,
                            QColor(ui::colors::NEGATIVE())}});
        donut->set_center_text(fmtx::scaled_unit(QStringLiteral("USD"), fmtx::Scale::Billion));
        donut->set_value_formatter([](double v) { return fmtx::number(v, 1); });
        results_layout_->addWidget(donut);
        add_md_card(tr("INTEGRATION LEVEL"), section_md("integration_levels"));
        add_md_card(tr("CREATION VS DIVERSION"), section_md("trade_creation_vs_diversion"));
        add_md_card(tr("MOTIVATIONS"), section_md("motivations_for_integration"));
        add_md_card(tr("SUCCESS FACTORS"), section_md("success_factors"));
        handled << QStringLiteral("integration_type") << QStringLiteral("integration_levels")
                << QStringLiteral("trade_creation_vs_diversion") << QStringLiteral("motivations_for_integration")
                << QStringLiteral("success_factors");
    } else if (context == QLatin1String("trade_barrier") && !h.isEmpty()) {
        auto band = [&](const char* lo, const char* hi, int dec) {
            return QStringLiteral("%1 – %2%").arg(fmtx::number(num(lo), dec), fmtx::number(num(hi), dec));
        };
        auto* s = add_strip();
        add_tile(s, tr("GDP LEVEL GAIN"), band("gdp_gain_low_pct", "gdp_gain_high_pct", 2), Tone::Positive,
                 tr("permanent, over 10-15 years"));
        if (num("gdp_gain_high_busd") > 0)
            add_tile(s, fmtx::header_scaled(tr("GDP GAIN"), QStringLiteral("USD"), fmtx::Scale::Billion),
                     QStringLiteral("%1 – %2").arg(fmtx::number(num("gdp_gain_low_busd"), 1),
                                                   fmtx::number(num("gdp_gain_high_busd"), 1)),
                     Tone::Positive);
        add_tile(s, tr("FDI INCREASE"), band("fdi_increase_low_pct", "fdi_increase_high_pct", 0), Tone::Positive,
                 tr("over 5 years"));
        add_tile(s, tr("PRODUCTIVITY"), band("productivity_gain_low_pct", "productivity_gain_high_pct", 1),
                 Tone::Positive, tr("over 5-10 years"));
        add_md_card(tr("RECOMMENDED SUPPORTING POLICIES"), section_md("policy_recommendations"));
        add_md_card(tr("INVESTMENT"), section_md("capital_investment_effects"));
        add_md_card(tr("EMPLOYMENT & WAGES"), section_md("employment_wage_effects"));
        add_md_card(tr("GROWTH"), section_md("growth_effects"));
        handled << QStringLiteral("policy_recommendations") << QStringLiteral("capital_investment_effects")
                << QStringLiteral("employment_wage_effects") << QStringLiteral("growth_effects");
    }

    // Anything else in the result (or every section of an unexpected shape):
    // one card per top-level key, never a blank pane.
    for (auto it = payload.begin(); it != payload.end(); ++it) {
        if (handled.contains(it.key()))
            continue;
        QString md;
        trade_markdown(it.value(), 0, md);
        add_md_card(trade_humanize(it.key()).toUpper(), md);
    }

    // Raw JSON — secondary, behind a toggle.
    auto* raw_btn = new QPushButton(tr("SHOW RAW JSON"), results_container_);
    raw_btn->setCheckable(true);
    raw_btn->setCursor(Qt::PointingHandCursor);
    raw_btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                                   "font-family:%3; font-size:%4px; padding:4px 10px; }"
                                   "QPushButton:checked, QPushButton:hover { color:%5; border-color:%5; }")
                               .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED())
                               .arg(ui::fonts::DATA_FAMILY())
                               .arg(ui::fonts::SMALL.get())
                               .arg(ui::colors::WARNING()));
    auto* raw = new QPlainTextEdit(results_container_);
    raw->setReadOnly(true);
    raw->setPlainText(QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Indented)));
    raw->setMinimumHeight(220);
    raw->setStyleSheet(QString("QPlainTextEdit { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; }")
                           .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM())
                           .arg(ui::fonts::DATA_FAMILY())
                           .arg(ui::fonts::SMALL.get()));
    raw->hide();
    connect(raw_btn, &QPushButton::toggled, raw, &QWidget::setVisible);
    connect(raw_btn, &QPushButton::toggled, raw_btn,
            [raw_btn](bool on) { raw_btn->setText(on ? tr("HIDE RAW JSON") : tr("SHOW RAW JSON")); });
    results_layout_->addWidget(raw_btn, 0, Qt::AlignLeft);
    results_layout_->addWidget(raw);
}

void TradeAnalysisPanel::on_trade_result(const QString& context, const QJsonObject& payload) {
    if (tap_index_of_context(context) < 0)
        return;
    // Only the run this panel started for that analysis (a cancelled or
    // superseded one is no longer pending; the pane drops stale ids as well).
    const quint64 id = pending_.take(context);
    const QJsonObject inputs = pending_inputs_.take(context);
    if (id == 0)
        return;
    // The script reports bad input as {"error": "..."} with a success exit code.
    if (payload.contains(QStringLiteral("error"))) {
        const QString err = payload.value(QStringLiteral("error")).toString();
        pane_->fail(id, err.isEmpty() ? tr("The analysis did not return a result.") : err);
        return;
    }
    QJsonObject stored;
    stored[QStringLiteral("inputs")] = inputs;
    stored[QStringLiteral("result")] = payload;
    pane_->deliver(id, stored);
}

void TradeAnalysisPanel::on_error(const QString& context, const QString& message) {
    if (tap_index_of_context(context) < 0)
        return;
    const quint64 id = pending_.take(context);
    pending_inputs_.remove(context);
    if (id != 0)
        pane_->fail(id, message.isEmpty() ? tr("The analysis did not return a result. Check the inputs and run it "
                                               "again.")
                                          : message);
}

void TradeAnalysisPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void TradeAnalysisPanel::retranslateUi() {
    if (title_lbl_)
        title_lbl_->setText(tr("TRADE GEOPOLITICS ANALYSIS"));

    // Analysis-type combo display labels (data keys preserved).
    if (type_combo_ && type_combo_->count() >= 4) {
        type_combo_->setItemText(0, tr("Benefits & Costs of Trade"));
        type_combo_->setItemText(1, tr("Trade Restrictions Analysis"));
        type_combo_->setItemText(2, tr("Trading Blocs Analysis"));
        type_combo_->setItemText(3, tr("Trade Barrier Removal Impact"));
    }

    // Tab labels (declared order matches addTab order).
    if (tabs_ && tabs_->count() >= 4) {
        tabs_->setTabText(0, fincept::ui::literal_ampersands(tr("Benefits & Costs")));
        tabs_->setTabText(1, tr("Restrictions"));
        tabs_->setTabText(2, tr("Trading Blocs"));
        tabs_->setTabText(3, tr("Barrier Removal"));
    }

    // Run buttons
    for (auto* btn : run_buttons_)
        if (btn)
            btn->setText(tr("RUN ANALYSIS"));

    // Field captions + hints captured during build_ui().
    for (const auto& pair : i18n_labels_)
        if (pair.first)
            pair.first->setText(tr(pair.second.toUtf8().constData()));

    // status_label_ and the results table reflect the last action and refresh
    // on the next run; per-field combo item labels are set at build time.
}

} // namespace fincept::screens
