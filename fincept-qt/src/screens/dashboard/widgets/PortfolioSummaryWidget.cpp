#include "screens/dashboard/widgets/PortfolioSummaryWidget.h"

#include "core/currency/Currency.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "services/portfolio/PortfolioService.h"
#include "storage/repositories/PortfolioRepository.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QScrollArea>
#include <QStyle>

namespace fincept::screens::widgets {

namespace {
constexpr const char* kConfigKeyPortfolioId = "portfolio_id";

// P&L colour is a "tone" property ("pos" / "neg", empty = default) matched by
// the card / list stylesheets; re-polish only when it changes — render() runs
// on every quote burst and used to re-parse CSS for both P&L labels each time.
void ps_set_tone(QLabel* label, const char* tone) {
    if (label->property("tone").toString() == QLatin1String(tone))
        return;
    label->setProperty("tone", QString::fromLatin1(tone));
    label->style()->unpolish(label);
    label->style()->polish(label);
}
} // namespace

PortfolioSummaryWidget::PortfolioSummaryWidget(QWidget* parent)
    : BaseWidget(tr("PORTFOLIO SUMMARY"), parent, ui::colors::POSITIVE) {
    auto* vl = content_layout();
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    // ── Selected-portfolio label ────────────────────────────────────────────
    // Sits above the summary card so the user always knows which portfolio
    // the numbers belong to. Updated by load_holdings(); hidden until then.
    portfolio_name_lbl_ = new QLabel(this);
    portfolio_name_lbl_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    portfolio_name_lbl_->hide();
    vl->addWidget(portfolio_name_lbl_);

    // ── Summary card ──
    summary_card_ = new QWidget(this);
    summary_card_->setObjectName("psCard");
    auto* sl = new QGridLayout(summary_card_);
    sl->setContentsMargins(10, 8, 10, 8);
    sl->setHorizontalSpacing(16);
    sl->setVerticalSpacing(4);

    auto make_metric = [&](const QString& label, QLabel*& value_out, int row, int col) {
        auto* lbl = new QLabel(label);
        lbl->setObjectName("psMetricLbl");
        sl->addWidget(lbl, row * 2, col);

        value_out = new QLabel("--");
        value_out->setObjectName("psMetricVal");
        sl->addWidget(value_out, row * 2 + 1, col);
    };

    make_metric(tr("TOTAL VALUE"), total_value_lbl_, 0, 0);
    make_metric(tr("DAY P&L"), day_pnl_lbl_, 0, 1);
    make_metric(tr("TOTAL P&L"), total_pnl_lbl_, 1, 0);
    make_metric(tr("HOLDINGS"), num_holdings_lbl_, 1, 1);

    vl->addWidget(summary_card_);

    // ── Holdings list header ──
    header_row_ = new QWidget(this);
    header_row_->setObjectName("psHeader");
    auto* hl = new QHBoxLayout(header_row_);
    hl->setContentsMargins(8, 3, 8, 3);

    auto make_hdr_lbl = [&](const QString& t, int s, Qt::Alignment a = Qt::AlignLeft) {
        auto* l = new QLabel(t);
        l->setAlignment(a);
        hl->addWidget(l, s);
    };
    make_hdr_lbl(tr("SYM"), 1);
    make_hdr_lbl(tr("SHARES"), 1, Qt::AlignRight);
    make_hdr_lbl(tr("PRICE"), 1, Qt::AlignRight);
    make_hdr_lbl(tr("VALUE"), 1, Qt::AlignRight);
    make_hdr_lbl(tr("P&L"), 1, Qt::AlignRight);
    vl->addWidget(header_row_);

    // Scrollable holdings list
    scroll_area_ = new QScrollArea;
    scroll_area_->setWidgetResizable(true);

    list_widget_ = new QWidget(this);
    list_widget_->setObjectName("psList");
    list_layout_ = new QVBoxLayout(list_widget_);
    list_layout_->setContentsMargins(0, 0, 0, 0);
    list_layout_->setSpacing(0);
    // Layout: [empty-state label][holding rows ...][stretch]
    empty_lbl_ = new QLabel(list_widget_);
    empty_lbl_->setObjectName("psEmpty");
    empty_lbl_->setAlignment(Qt::AlignCenter);
    empty_lbl_->setWordWrap(true);
    empty_lbl_->hide();
    list_layout_->addWidget(empty_lbl_);
    list_layout_->addStretch();

    scroll_area_->setWidget(list_widget_);
    vl->addWidget(scroll_area_, 1);

    // Enable the BaseWidget gear icon — make_config_dialog() handles the rest.
    set_configurable(true);

    // Manual refresh button on the title bar → reload holdings.
    connect(this, &BaseWidget::refresh_requested, this, [this] { load_holdings(); });

    // ── Cross-tab sync ─────────────────────────────────────────────────────
    // When the Portfolio screen mutates data (add/sell asset, create/delete
    // portfolio), the user expects this tile to update without a manual
    // refresh. PortfolioService is the single source of truth; we listen to
    // its signals rather than polling.
    auto& svc = services::PortfolioService::instance();
    // load_holdings() (re)subscribes to the hub, so these reloads only run while
    // the tile is visible — otherwise a hidden tile would take a subscription
    // that hideEvent() has already released, and keep it. showEvent() reloads
    // when hub_active_ is false, so a change made while hidden is picked up then.
    connect(&svc, &services::PortfolioService::asset_added, this, [this](const QString& pid) {
        if (pid == selected_portfolio_id_ && isVisible())
            load_holdings();
    });
    connect(&svc, &services::PortfolioService::asset_sold, this, [this](const QString& pid) {
        if (pid == selected_portfolio_id_ && isVisible())
            load_holdings();
    });
    connect(&svc, &services::PortfolioService::portfolio_created, this, [this](const portfolio::Portfolio&) {
        refresh_portfolio_cache();
        // If we had no selection yet, pick the first one now.
        if (selected_portfolio_id_.isEmpty() && isVisible())
            load_holdings();
    });
    connect(&svc, &services::PortfolioService::portfolio_deleted, this, [this](const QString& deleted_id) {
        refresh_portfolio_cache();
        if (deleted_id == selected_portfolio_id_) {
            selected_portfolio_id_.clear();
            selected_portfolio_name_.clear();
            emit config_changed(config());
            if (isVisible())
                load_holdings();
        }
    });

    apply_styles();
    set_loading(true);
}

QJsonObject PortfolioSummaryWidget::config() const {
    QJsonObject cfg;
    if (!selected_portfolio_id_.isEmpty())
        cfg.insert(kConfigKeyPortfolioId, selected_portfolio_id_);
    return cfg;
}

void PortfolioSummaryWidget::apply_config(const QJsonObject& cfg) {
    const QString pid = cfg.value(kConfigKeyPortfolioId).toString();
    if (pid == selected_portfolio_id_)
        return;
    selected_portfolio_id_ = pid;
    selected_portfolio_name_.clear();
    if (isVisible())
        load_holdings();
}

void PortfolioSummaryWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        load_holdings();
}

void PortfolioSummaryWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void PortfolioSummaryWidget::apply_styles() {
    // The card and header used to carry selector-less sheets, which Qt applies
    // to their labels as well — the "... QLabel" rules keep that cascade intact.
    summary_card_->setStyleSheet(
        QString("QWidget#psCard{background:%1;border-radius:2px;}"
                "QWidget#psCard QLabel{border-radius:2px;background:transparent;}"
                "QLabel#psMetricLbl{color:%2;font-size:9px;}"
                "QLabel#psMetricVal{color:%3;font-size:13px;font-weight:bold;}"
                "QLabel#psMetricVal[tone=\"pos\"]{color:%4;}"
                "QLabel#psMetricVal[tone=\"neg\"]{color:%5;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_TERTIARY(), ui::colors::TEXT_PRIMARY(),
                 ui::colors::POSITIVE(), ui::colors::NEGATIVE()));
    header_row_->setStyleSheet(
        QString("QWidget#psHeader{background:%1;}"
                "QWidget#psHeader QLabel{color:%2;font-size:9px;font-weight:bold;background:transparent;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_TERTIARY()));
    if (portfolio_name_lbl_)
        portfolio_name_lbl_->setStyleSheet(
            QString("color: %1; font-size: 10px; font-weight: 600; background: transparent; padding: 0 2px;")
                .arg(ui::colors::TEXT_SECONDARY()));
    scroll_area_->setStyleSheet(
        QString("QScrollArea { border: none; background: transparent; }"
                "QScrollBar:vertical { width: 4px; background: transparent; }"
                "QScrollBar::handle:vertical { background: %1; border-radius: 2px; min-height: 20px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
            .arg(ui::colors::BORDER_MED()));

    // Holdings-row chrome, hoisted (P7). render() draws 5 cells per holding
    // and used to call setStyleSheet on every one of them on every re-render.
    if (list_widget_)
        list_widget_->setStyleSheet(QString("#psList{background:transparent;}"
                                            "#psList QWidget#psRow{background:transparent;}"
                                            "#psList QWidget#psRowAlt{background:%1;}"
                                            "#psList QLabel{font-size:10px;background:transparent;}"
                                            "#psList QLabel#psSym{color:%2;}"
                                            "#psList QLabel#psShares{color:%3;}"
                                            "#psList QLabel#psNum{color:%2;}"
                                            "#psList QLabel#psPnl{color:%2;}"
                                            "#psList QLabel#psPnl[tone=\"pos\"]{color:%4;}"
                                            "#psList QLabel#psPnl[tone=\"neg\"]{color:%5;}"
                                            "#psList QLabel#psEmpty{color:%6;font-size:11px;padding:20px;}")
                                        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(),
                                             ui::colors::TEXT_SECONDARY(), ui::colors::POSITIVE(),
                                             ui::colors::NEGATIVE(), ui::colors::TEXT_TERTIARY()));
}

void PortfolioSummaryWidget::on_theme_changed() {
    apply_styles();
    if (!last_holdings_.isEmpty() && !last_quotes_.isEmpty())
        render(last_holdings_, last_quotes_);
}

void PortfolioSummaryWidget::refresh_portfolio_cache() {
    auto r = fincept::PortfolioRepository::instance().list_portfolios();
    if (r.is_ok())
        portfolio_cache_ = r.value();
    else
        portfolio_cache_.clear();
}

void PortfolioSummaryWidget::load_holdings() {
    refresh_portfolio_cache();

    if (portfolio_cache_.isEmpty()) {
        render_empty(tr("No portfolios yet.\nCreate one from the Portfolio tab."));
        return;
    }

    // Resolve selection. If the configured portfolio no longer exists
    // (deleted from another tab), fall through to the first available.
    const portfolio::Portfolio* picked = nullptr;
    if (!selected_portfolio_id_.isEmpty()) {
        for (const auto& p : portfolio_cache_) {
            if (p.id == selected_portfolio_id_) {
                picked = &p;
                break;
            }
        }
    }
    if (!picked) {
        picked = &portfolio_cache_.first();
        selected_portfolio_id_ = picked->id;
        emit config_changed(config());
    }
    selected_portfolio_name_ = picked->name;
    // This tile shows the portfolio's OWN currency (intrinsic) — not the global
    // preferred currency. Values are not converted; we only use the right symbol.
    selected_portfolio_currency_ = picked->currency.isEmpty() ? QStringLiteral("USD") : picked->currency;
    portfolio_name_lbl_->setText(picked->name.toUpper());
    portfolio_name_lbl_->setVisible(true);

    auto assets_r = fincept::PortfolioRepository::instance().get_assets(selected_portfolio_id_);
    QVector<Holding> holdings;
    if (assets_r.is_ok()) {
        for (const auto& a : assets_r.value()) {
            if (a.symbol.isEmpty() || a.quantity <= 0)
                continue;
            Holding h;
            h.symbol = a.symbol;
            h.shares = a.quantity;
            h.avg_cost = a.avg_buy_price;
            holdings.append(h);
        }
    }

    if (holdings.isEmpty()) {
        render_empty(tr("'%1' has no holdings.\nAdd positions from the Portfolio tab.").arg(picked->name));
        return;
    }

    last_holdings_ = holdings;
    hub_resubscribe(holdings);
}

void PortfolioSummaryWidget::render_empty(const QString& message) {
    set_loading(false);
    last_holdings_.clear();
    last_quotes_.clear();
    hub_unsubscribe_all();

    // Hide the rows + zero the summary counters so old numbers don't linger
    // after a portfolio switch / deletion.
    for (const auto& r : rows_)
        r.row->setVisible(false);
    empty_lbl_->setText(message);
    empty_lbl_->setVisible(true);
    const QString sym = cur::symbol_for(selected_portfolio_currency_);
    total_value_lbl_->setText(sym + QStringLiteral("0"));
    day_pnl_lbl_->setText(sym + QStringLiteral("0"));
    total_pnl_lbl_->setText(sym + QStringLiteral("0"));
    num_holdings_lbl_->setText(QStringLiteral("0"));

    // Reset the colour overrides applied during a populated render so the
    // zeroed-out P&L labels don't get stuck red/green from the prior state.
    ps_set_tone(day_pnl_lbl_, "");
    ps_set_tone(total_pnl_lbl_, "");

    if (!selected_portfolio_name_.isEmpty()) {
        portfolio_name_lbl_->setText(selected_portfolio_name_.toUpper());
        portfolio_name_lbl_->setVisible(true);
    } else {
        portfolio_name_lbl_->setVisible(false);
    }
}

void PortfolioSummaryWidget::hub_resubscribe(const QVector<Holding>& holdings) {
    auto& hub = datahub::DataHub::instance();
    // Holdings set may have changed since last subscribe — wipe all and
    // re-register so we don't leave stale topic subs behind.
    hub.unsubscribe(this);
    row_cache_.clear();
    for (const auto& h : holdings) {
        const QString sym = h.symbol;
        const QString topic = QStringLiteral("market:quote:") + sym;
        hub.subscribe(this, topic, [this, sym](const QVariant& v) {
            if (!v.canConvert<services::QuoteData>())
                return;
            row_cache_.insert(sym, v.value<services::QuoteData>());
            set_loading(false);
            // One holdings-table rebuild per delivery burst, not one per symbol.
            schedule_render([this]() { rebuild_from_cache(); });
        });
    }
    hub_active_ = true;
}

void PortfolioSummaryWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void PortfolioSummaryWidget::rebuild_from_cache() {
    QVector<services::QuoteData> quotes;
    quotes.reserve(row_cache_.size());
    for (const auto& h : last_holdings_) {
        auto it = row_cache_.constFind(h.symbol);
        if (it != row_cache_.constEnd())
            quotes.append(it.value());
    }
    if (!quotes.isEmpty())
        render(last_holdings_, quotes);
}

void PortfolioSummaryWidget::render(const QVector<Holding>& holdings, const QVector<services::QuoteData>& quotes) {
    last_holdings_ = holdings;
    last_quotes_ = quotes;

    // Portfolio's own currency symbol (intrinsic — not the global preference,
    // and values are not converted: symbol-only).
    const QString sym = cur::symbol_for(selected_portfolio_currency_);

    QMap<QString, const services::QuoteData*> qmap;
    for (const auto& q : last_quotes_)
        qmap[q.symbol] = &q;

    double total_value = 0;
    double total_cost = 0;
    double day_pnl = 0;

    empty_lbl_->setVisible(false);

    for (int i = 0; i < holdings.size(); ++i) {
        const Holding& h = holdings[i];
        const services::QuoteData* q = qmap.value(h.symbol, nullptr);
        double price = q ? q->price : 0;
        double value = price * h.shares;
        double cost = h.avg_cost * h.shares;
        // A holding with no price yet (quotes stream in one symbol at a time,
        // and a delisted/unknown symbol never prices) used to contribute its
        // full cost basis to total_cost with zero value — so TOTAL P&L read as a
        // huge loss until every quote landed, and forever if one never did.
        // Leave unpriced holdings out of the value/cost/P&L totals entirely.
        const bool priced = price > 0;
        double pnl = priced ? value - cost : 0;
        double day_chg = q ? (q->change * h.shares) : 0;

        if (priced) {
            total_value += value;
            total_cost += cost;
        }
        day_pnl += day_chg;

        // Refill the pooled row in place (QLabel::setText skips unchanged text).
        // No setStyleSheet here — colours come from the single stylesheet on
        // list_widget_ (see apply_styles) via object names / the "tone" property.
        if (i >= rows_.size())
            rows_.append(make_row(i));
        const PsRow& r = rows_[i];
        link_symbol(r.row, h.symbol); // double-click a holding → Equity Research
        r.sym->setText(h.symbol);
        r.shares->setText(QString::number(h.shares, 'f', h.shares == (int)h.shares ? 0 : 2));
        r.price->setText(price > 0 ? sym + QString::number(price, 'f', 2) : QStringLiteral("--"));
        r.value->setText(value > 0 ? sym + QString::number(value, 'f', 0) : QStringLiteral("--"));

        QString pnl_str = pnl >= 0 ? QStringLiteral("+") + sym + QString::number(pnl, 'f', 0)
                                   : QStringLiteral("-") + sym + QString::number(-pnl, 'f', 0);
        if (!priced)
            pnl_str = QStringLiteral("--");
        r.pnl->setText(pnl_str);
        ps_set_tone(r.pnl, !priced ? "" : (pnl >= 0 ? "pos" : "neg"));
        r.row->setVisible(true);
    }
    for (int i = static_cast<int>(holdings.size()); i < rows_.size(); ++i)
        rows_[i].row->setVisible(false);

    total_value_lbl_->setText(sym + QString::number(total_value, 'f', 0));
    num_holdings_lbl_->setText(QString::number(holdings.size()));

    double total_pnl = total_value - total_cost;
    QString day_str = day_pnl >= 0 ? QStringLiteral("+") + sym + QString::number(day_pnl, 'f', 0)
                                   : QStringLiteral("-") + sym + QString::number(-day_pnl, 'f', 0);
    QString tot_str = total_pnl >= 0 ? QStringLiteral("+") + sym + QString::number(total_pnl, 'f', 0)
                                     : QStringLiteral("-") + sym + QString::number(-total_pnl, 'f', 0);

    day_pnl_lbl_->setText(day_str);
    ps_set_tone(day_pnl_lbl_, day_pnl >= 0 ? "pos" : "neg");

    total_pnl_lbl_->setText(tot_str);
    ps_set_tone(total_pnl_lbl_, total_pnl >= 0 ? "pos" : "neg");
}

PortfolioSummaryWidget::PsRow PortfolioSummaryWidget::make_row(int index) {
    PsRow r;
    r.row = new QWidget(list_widget_);
    r.row->setObjectName(index % 2 ? QStringLiteral("psRowAlt") : QStringLiteral("psRow"));
    auto* rl = new QHBoxLayout(r.row);
    rl->setContentsMargins(8, 4, 8, 4);

    auto cell = [&r, rl](Qt::Alignment align, const QString& object_name) {
        auto* lbl = new QLabel(r.row);
        lbl->setObjectName(object_name);
        lbl->setAlignment(align);
        rl->addWidget(lbl, 1);
        return lbl;
    };
    r.sym = cell(Qt::AlignLeft, QStringLiteral("psSym"));
    r.shares = cell(Qt::AlignRight, QStringLiteral("psShares"));
    r.price = cell(Qt::AlignRight, QStringLiteral("psNum"));
    r.value = cell(Qt::AlignRight, QStringLiteral("psNum"));
    r.pnl = cell(Qt::AlignRight, QStringLiteral("psPnl"));

    // Rows sit between the empty-state label and the trailing stretch.
    list_layout_->insertWidget(list_layout_->count() - 1, r.row);
    return r;
}

QDialog* PortfolioSummaryWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle(tr("Configure — Portfolio Summary"));
    auto* form = new QFormLayout(dlg);

    // Ensure the cache reflects the current state of the DB (the Portfolio
    // tab may have added/renamed entries since the widget was constructed).
    refresh_portfolio_cache();

    auto* combo = new QComboBox(dlg);
    if (portfolio_cache_.isEmpty()) {
        combo->addItem(tr("(no portfolios — create one in the Portfolio tab)"), QString());
        combo->setEnabled(false);
    } else {
        for (const auto& p : portfolio_cache_) {
            const QString label =
                p.name + (p.currency.isEmpty() ? QString() : QStringLiteral("  (%1)").arg(p.currency));
            combo->addItem(label, p.id);
            if (p.id == selected_portfolio_id_)
                combo->setCurrentIndex(combo->count() - 1);
        }
    }
    form->addRow(tr("Portfolio"), combo);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    form->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, dlg, [this, dlg, combo]() {
        const QString picked = combo->currentData().toString();
        if (!picked.isEmpty() && picked != selected_portfolio_id_) {
            QJsonObject cfg;
            cfg.insert(kConfigKeyPortfolioId, picked);
            apply_config(cfg);
            emit config_changed(cfg);
        }
        dlg->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    return dlg;
}

void PortfolioSummaryWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("PORTFOLIO SUMMARY"));
    rebuild_from_cache(); // re-renders header/metric labels in the new language
}

} // namespace fincept::screens::widgets
