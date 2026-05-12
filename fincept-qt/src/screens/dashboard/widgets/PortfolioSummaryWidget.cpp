#include "screens/dashboard/widgets/PortfolioSummaryWidget.h"

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

namespace fincept::screens::widgets {

namespace {
constexpr const char* kConfigKeyPortfolioId = "portfolio_id";
} // namespace

PortfolioSummaryWidget::PortfolioSummaryWidget(QWidget* parent)
    : BaseWidget("PORTFOLIO SUMMARY", parent, ui::colors::POSITIVE) {
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
    auto* sl = new QGridLayout(summary_card_);
    sl->setContentsMargins(10, 8, 10, 8);
    sl->setHorizontalSpacing(16);
    sl->setVerticalSpacing(4);

    auto make_metric = [&](const QString& label, QLabel*& value_out, int row, int col) {
        auto* lbl = new QLabel(label);
        metric_labels_.append(lbl);
        sl->addWidget(lbl, row * 2, col);

        value_out = new QLabel("--");
        metric_values_.append(value_out);
        sl->addWidget(value_out, row * 2 + 1, col);
    };

    make_metric("TOTAL VALUE", total_value_lbl_, 0, 0);
    make_metric("DAY P&L", day_pnl_lbl_, 0, 1);
    make_metric("TOTAL P&L", total_pnl_lbl_, 1, 0);
    make_metric("HOLDINGS", num_holdings_lbl_, 1, 1);

    vl->addWidget(summary_card_);

    // ── Holdings list header ──
    header_row_ = new QWidget(this);
    auto* hl = new QHBoxLayout(header_row_);
    hl->setContentsMargins(8, 3, 8, 3);

    auto make_hdr_lbl = [&](const QString& t, int s, Qt::Alignment a = Qt::AlignLeft) {
        auto* l = new QLabel(t);
        l->setAlignment(a);
        header_labels_.append(l);
        hl->addWidget(l, s);
    };
    make_hdr_lbl("SYM", 1);
    make_hdr_lbl("SHARES", 1, Qt::AlignRight);
    make_hdr_lbl("PRICE", 1, Qt::AlignRight);
    make_hdr_lbl("VALUE", 1, Qt::AlignRight);
    make_hdr_lbl("P&L", 1, Qt::AlignRight);
    vl->addWidget(header_row_);

    // Scrollable holdings list
    scroll_area_ = new QScrollArea;
    scroll_area_->setWidgetResizable(true);

    auto* list_widget = new QWidget(this);
    list_widget->setStyleSheet("background: transparent;");
    list_layout_ = new QVBoxLayout(list_widget);
    list_layout_->setContentsMargins(0, 0, 0, 0);
    list_layout_->setSpacing(0);
    list_layout_->addStretch();

    scroll_area_->setWidget(list_widget);
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
    connect(&svc, &services::PortfolioService::asset_added, this,
            [this](const QString& pid) {
                if (pid == selected_portfolio_id_)
                    load_holdings();
            });
    connect(&svc, &services::PortfolioService::asset_sold, this,
            [this](const QString& pid) {
                if (pid == selected_portfolio_id_)
                    load_holdings();
            });
    connect(&svc, &services::PortfolioService::portfolio_created, this,
            [this](const portfolio::Portfolio&) {
                refresh_portfolio_cache();
                // If we had no selection yet, pick the first one now.
                if (selected_portfolio_id_.isEmpty())
                    load_holdings();
            });
    connect(&svc, &services::PortfolioService::portfolio_deleted, this,
            [this](const QString& deleted_id) {
                refresh_portfolio_cache();
                if (deleted_id == selected_portfolio_id_) {
                    selected_portfolio_id_.clear();
                    selected_portfolio_name_.clear();
                    emit config_changed(config());
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
    summary_card_->setStyleSheet(QString("background: %1; border-radius: 2px;").arg(ui::colors::BG_RAISED()));
    for (auto* lbl : metric_labels_)
        lbl->setStyleSheet(
            QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::TEXT_TERTIARY()));
    for (auto* val : metric_values_)
        val->setStyleSheet(QString("color: %1; font-size: 13px; font-weight: bold; background: transparent;")
                               .arg(ui::colors::TEXT_PRIMARY()));
    header_row_->setStyleSheet(QString("background: %1;").arg(ui::colors::BG_RAISED()));
    for (auto* lbl : header_labels_)
        lbl->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;")
                               .arg(ui::colors::TEXT_TERTIARY()));
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
        render_empty("No portfolios yet.\nCreate one from the Portfolio tab.");
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
        render_empty(QStringLiteral("'%1' has no holdings.\nAdd positions from the Portfolio tab.")
                         .arg(picked->name));
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

    // Wipe rows + zero the summary counters so old numbers don't linger
    // after a portfolio switch / deletion.
    while (list_layout_->count() > 0) {
        auto* item = list_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    auto* empty = new QLabel(message);
    empty->setAlignment(Qt::AlignCenter);
    empty->setWordWrap(true);
    empty->setStyleSheet(QString("color: %1; font-size: 11px; padding: 20px; background: transparent;")
                             .arg(ui::colors::TEXT_TERTIARY()));
    list_layout_->addWidget(empty);
    list_layout_->addStretch();
    total_value_lbl_->setText(QStringLiteral("$0"));
    day_pnl_lbl_->setText(QStringLiteral("$0"));
    total_pnl_lbl_->setText(QStringLiteral("$0"));
    num_holdings_lbl_->setText(QStringLiteral("0"));

    // Reset the colour overrides applied during a populated render so the
    // zeroed-out P&L labels don't get stuck red/green from the prior state.
    const QString default_value_css =
        QString("color: %1; font-size: 13px; font-weight: bold; background: transparent;")
            .arg(ui::colors::TEXT_PRIMARY());
    day_pnl_lbl_->setStyleSheet(default_value_css);
    total_pnl_lbl_->setStyleSheet(default_value_css);

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
            rebuild_from_cache();
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

void PortfolioSummaryWidget::render(const QVector<Holding>& holdings,
                                    const QVector<services::QuoteData>& quotes) {
    last_holdings_ = holdings;
    last_quotes_ = quotes;

    QMap<QString, const services::QuoteData*> qmap;
    for (const auto& q : last_quotes_)
        qmap[q.symbol] = &q;

    double total_value = 0;
    double total_cost = 0;
    double day_pnl = 0;

    // Clear list
    while (list_layout_->count() > 0) {
        auto* item = list_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    bool alt = false;
    for (const auto& h : holdings) {
        const services::QuoteData* q = qmap.value(h.symbol, nullptr);
        double price = q ? q->price : 0;
        double value = price * h.shares;
        double cost = h.avg_cost * h.shares;
        double pnl = value - cost;
        double day_chg = q ? (q->change * h.shares) : 0;

        total_value += value;
        total_cost += cost;
        day_pnl += day_chg;

        auto* row = new QWidget(this);
        row->setStyleSheet(QString("background: %1;").arg(alt ? ui::colors::BG_RAISED() : "transparent"));
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(8, 4, 8, 4);

        auto cell = [&](const QString& text, int stretch, Qt::Alignment align, const QString& color) {
            auto* lbl = new QLabel(text);
            lbl->setAlignment(align);
            lbl->setStyleSheet(QString("color: %1; font-size: 10px; background: transparent;").arg(color));
            rl->addWidget(lbl, stretch);
        };

        cell(h.symbol, 1, Qt::AlignLeft, ui::colors::TEXT_PRIMARY);
        cell(QString::number(h.shares, 'f', h.shares == (int)h.shares ? 0 : 2), 1, Qt::AlignRight,
             ui::colors::TEXT_SECONDARY);
        cell(price > 0 ? QString("$%1").arg(price, 0, 'f', 2) : "--", 1, Qt::AlignRight, ui::colors::TEXT_PRIMARY);
        cell(value > 0 ? QString("$%1").arg(value, 0, 'f', 0) : "--", 1, Qt::AlignRight, ui::colors::TEXT_PRIMARY);

        QString pnl_str = pnl >= 0 ? QString("+$%1").arg(pnl, 0, 'f', 0) : QString("-$%1").arg(-pnl, 0, 'f', 0);
        cell(pnl_str, 1, Qt::AlignRight, pnl >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE);

        list_layout_->addWidget(row);
        alt = !alt;
    }
    list_layout_->addStretch();

    total_value_lbl_->setText(QString("$%1").arg(total_value, 0, 'f', 0));
    num_holdings_lbl_->setText(QString::number(holdings.size()));

    double total_pnl = total_value - total_cost;
    QString day_str = day_pnl >= 0 ? QString("+$%1").arg(day_pnl, 0, 'f', 0) : QString("-$%1").arg(-day_pnl, 0, 'f', 0);
    QString tot_str =
        total_pnl >= 0 ? QString("+$%1").arg(total_pnl, 0, 'f', 0) : QString("-$%1").arg(-total_pnl, 0, 'f', 0);

    day_pnl_lbl_->setText(day_str);
    day_pnl_lbl_->setStyleSheet(QString("color: %1; font-size: 13px; font-weight: bold; background: transparent;")
                                    .arg(day_pnl >= 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));

    total_pnl_lbl_->setText(tot_str);
    total_pnl_lbl_->setStyleSheet(QString("color: %1; font-size: 13px; font-weight: bold; background: transparent;")
                                      .arg(total_pnl >= 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
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
            const QString label = p.name + (p.currency.isEmpty() ? QString() : QStringLiteral("  (%1)").arg(p.currency));
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

} // namespace fincept::screens::widgets
