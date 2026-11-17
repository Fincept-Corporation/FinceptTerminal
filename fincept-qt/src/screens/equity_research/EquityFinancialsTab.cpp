// src/screens/equity_research/EquityFinancialsTab.cpp
//
// Service wiring of the Financials tab; the UI is EquityFinancialsView (see the header).
#include "screens/equity_research/EquityFinancialsTab.h"

#include "screens/equity_research/EquityFinancialsView.h"
#include "services/equity/EquityResearchService.h"
#include "ui/formatting/FormatWidgets.h"

#include <QDateTime>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

namespace fincept::screens {

EquityFinancialsTab::EquityFinancialsTab(QWidget* parent) : QWidget(parent) {
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    view_ = new EquityFinancialsView(this);
    lay->addWidget(view_);
    watcher_ = new QFutureWatcher<FinBundlePtr>(this);
    connect(watcher_, &QFutureWatcherBase::finished, this, &EquityFinancialsTab::on_computed);
    connect(view_, &EquityFinancialsView::retry_requested, this, [this]() {
        if (!view_->bundle())
            view_->show_loading(current_symbol_);
        request();
    });
    connect(view_, &EquityFinancialsView::peers_requested, this, &EquityFinancialsTab::request_peers);
    // A currency / number-format change re-formats what is held (no request).
    ui::formatting::on_format_changed(this, [this]() { view_->refresh_format(); });

    auto& svc = services::equity::EquityResearchService::instance();
    connect(&svc, &services::equity::EquityResearchService::financials_loaded, this,
            &EquityFinancialsTab::on_financials_loaded);
    // Sector / industry (template, Altman variant), reporting currency and market value.
    connect(&svc, &services::equity::EquityResearchService::info_loaded, this,
            [this](const services::equity::StockInfo& info) {
                if (info.symbol != current_symbol_)
                    return;
                const bool changed = info.sector != info_.sector || info.industry != info_.industry ||
                                     info.financial_currency != info_.financial_currency ||
                                     info.currency != info_.currency || info.market_cap != info_.market_cap ||
                                     info.company_name != info_.company_name;
                info_ = info;
                if (have_raw_ && changed)
                    compute();
            });
    connect(&svc, &services::equity::EquityResearchService::load_failed, this,
            [this](const services::equity::LoadFailure& f) { on_load_failed(f); });
    // PEERS: the group the Peers / Valuation tabs load, whoever asked for it.
    connect(&svc, &services::equity::EquityResearchService::peers_loaded, this,
            [this](const QVector<services::equity::PeerData>& peers) {
                for (const auto& p : peers)
                    if (p.symbol.compare(current_symbol_, Qt::CaseInsensitive) == 0) {
                        peers_pending_ = false;
                        view_->set_peers(peers);
                        return;
                    }
            });
    connect(&svc, &services::equity::EquityResearchService::suggested_peers_loaded, this,
            [this](const QString& symbol, const QStringList& peers) {
                if (!peers_pending_ || symbol.compare(current_symbol_, Qt::CaseInsensitive) != 0)
                    return;
                if (peers.isEmpty()) {
                    peers_pending_ = false;
                    view_->set_peers_state(2, tr("No same-industry peers found for %1.").arg(symbol));
                    return;
                }
                services::equity::EquityResearchService::instance().fetch_peers(current_symbol_, peers);
            });
}

EquityFinancialsTab::~EquityFinancialsTab() = default;

void EquityFinancialsTab::set_symbol(const QString& symbol) {
    if (symbol == current_symbol_)
        return;
    current_symbol_ = symbol;
    raw_ = {};
    have_raw_ = false;
    peers_pending_ = false;
    pending_failure_.clear();
    pending_failure_code_.clear();
    if (info_.symbol != symbol)
        info_ = {};
    if (symbol.isEmpty()) {
        view_->show_idle();
        return;
    }
    // Remembered this session: on screen at once; refreshed quietly only once a day old.
    if (const FinBundlePtr b = memo_.value(symbol)) {
        info_ = b->info;
        view_->set_bundle(b);
        const qint64 age = QDateTime::currentSecsSinceEpoch() - b->fetched_at;
        if (b->fetched_at > 0 && !b->stale && age < kQuietRefreshSec)
            return;
        request();
        return;
    }
    view_->show_loading(symbol);
    request();
}

void EquityFinancialsTab::show_section(const QString& key) {
    view_->show_section(key);
}

void EquityFinancialsTab::request() {
    if (current_symbol_.isEmpty())
        return;
    auto& svc = services::equity::EquityResearchService::instance();
    svc.load_info_only(current_symbol_);
    svc.fetch_financials(current_symbol_); // cached / in-flight requests are folded by the service
}

void EquityFinancialsTab::on_financials_loaded(services::equity::FinancialsData data) {
    if (data.symbol != current_symbol_)
        return;
    // The same payload broadcast again (another tab asked): nothing new to compute.
    if (have_raw_ && view_->bundle() && data.fetched_at == raw_.fetched_at && data.stale == raw_.stale)
        return;
    raw_ = std::move(data);
    have_raw_ = true;
    if (!raw_.stale) {
        pending_failure_.clear();
        pending_failure_code_.clear();
    }
    compute();
}

void EquityFinancialsTab::compute() {
    // Parsing is done by the service off the GUI thread; the analytics run on a worker too.
    const services::equity::FinancialsData data = raw_;
    const services::equity::StockInfo info = info_;
    watcher_->setFuture(QtConcurrent::run([data, info]() { return fin_bundle(data, info); }));
}

void EquityFinancialsTab::on_computed() {
    const FinBundlePtr b = watcher_->result();
    if (!b || b->symbol != current_symbol_)
        return;
    remember(b);
    view_->set_bundle(b);
    if (!pending_failure_.isEmpty()) {
        view_->show_refresh_failed(pending_failure_);
        pending_failure_.clear();
        pending_failure_code_.clear();
    }
}

void EquityFinancialsTab::remember(const FinBundlePtr& bundle) {
    memo_.insert(bundle->symbol, bundle);
    memo_order_.removeAll(bundle->symbol);
    memo_order_.append(bundle->symbol);
    while (memo_order_.size() > kMemoSize)
        memo_.remove(memo_order_.takeFirst());
}

void EquityFinancialsTab::on_load_failed(const services::equity::LoadFailure& f) {
    if (f.symbol != current_symbol_)
        return;
    if (f.leg == QLatin1String("Peers")) {
        if (peers_pending_) {
            peers_pending_ = false;
            view_->set_peers_state(2, f.message.left(200));
        }
        return;
    }
    if (f.leg != QLatin1String("Financials"))
        return;
    if (view_->bundle()) {
        view_->show_refresh_failed(f.message);
    } else if (watcher_->isRunning()) {
        // The saved copy replayed with this failure is still being computed: say it once it shows.
        pending_failure_ = f.message;
        pending_failure_code_ = f.code;
    } else {
        view_->show_error(current_symbol_, f.message, f.code);
    }
}

void EquityFinancialsTab::request_peers() {
    if (current_symbol_.isEmpty())
        return;
    peers_pending_ = true;
    view_->set_peers_state(1);
    // Same count as the Peers tab, so its later request is served from the shared cache.
    services::equity::EquityResearchService::instance().fetch_suggested_peers(current_symbol_, 7);
}

} // namespace fincept::screens
