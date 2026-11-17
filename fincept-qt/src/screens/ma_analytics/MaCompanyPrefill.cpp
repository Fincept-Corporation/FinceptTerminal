// MaCompanyPrefill.cpp — MA-4: start an M&A analysis from a ticker. A "Company" strip above the sub-tabs of the
// Valuation, Merger (acquirer + target) and Range Check modules loads one services::CompanySnapshot and fills the
// inputs that have a market or statement figure, each with a "from <source>" caption. Nothing is invented: a
// figure the sources do not have is listed and the field keeps its value. A field the user edits afterwards keeps
// the user's value when the same company is loaded again. Everything is asynchronous; a reply is applied only if
// it answers the latest request of its role.
#include "screens/ma_analytics/MAModulePanel.h"

#include "core/logging/Logger.h"
#include "services/company_snapshot/CompanySnapshotService.h"
#include "services/equity/EquityResearchService.h"
#include "services/instruments/InstrumentResolver.h"
#include "storage/cache/CacheManager.h"
#include "ui/formatting/DateFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/InputMemory.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace fincept::screens {

using fincept::services::CompanySnapshot;
using fincept::services::CompanySnapshotService;

namespace {

QString ma_pf_role_title(const QString& role) {
    return role == QLatin1String("acquirer") ? QCoreApplication::translate("MAModulePanel", "ACQUIRER")
                                             : QCoreApplication::translate("MAModulePanel", "TARGET");
}

/// "Yahoo Finance, 17 Nov 2026" — the caption beside a filled field.
QString ma_pf_caption(const CompanySnapshot& s, const char* field) {
    const QString src = s.field_source.value(QString::fromLatin1(field));
    const QString who = src == QLatin1String("sec") ? QStringLiteral("SEC") : QStringLiteral("Yahoo");
    return who + QStringLiteral(", ") +
           fincept::ui::formatting::date(s.as_of.isValid() ? s.as_of : QDateTime::currentDateTime(),
                                         fincept::ui::formatting::DateStyle::Short);
}

} // namespace

QWidget* MAModulePanel::build_company_strip(QWidget* parent) {
    auto* strip = new QWidget(parent);
    strip->setObjectName(QStringLiteral("maCompanyStrip"));
    strip->setStyleSheet(QString("QWidget#maCompanyStrip { background:%1; border:1px solid %2; }"
                                 "QLabel#maStripRole { color:%3; font-size:%4px; font-weight:700; letter-spacing:1px;"
                                 " background:transparent; }"
                                 "QLabel#maStripStatus { color:%5; font-size:%6px; background:transparent; }"
                                 "QPushButton#maStripLoad { background:%7; color:%8; border:none; padding:4px 14px;"
                                 " font-size:%6px; font-weight:700; }"
                                 "QPushButton#maStripLoad:disabled { background:%1; color:%5; }"
                                 "QPushButton#maStripPeers { background:transparent; color:%9; border:1px solid %2;"
                                 " padding:3px 10px; font-size:%6px; }")
                             .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM(), ui::colors::legible(module_.color))
                             .arg(ui::fonts::px(-3))
                             .arg(ui::colors::TEXT_SECONDARY())
                             .arg(ui::fonts::px(-2))
                             .arg(ui::colors::legible(module_.color), ui::colors::TEXT_ON_ACCENT(),
                                  ui::colors::TEXT_SECONDARY()));
    auto* vl = new QVBoxLayout(strip);
    vl->setContentsMargins(10, 6, 10, 6);
    vl->setSpacing(4);

    QStringList roles{QStringLiteral("target")};
    if (module_.id == services::ma::ModuleId::Merger)
        roles = {QStringLiteral("acquirer"), QStringLiteral("target")};
    for (const QString& role : roles) {
        auto* line = new QWidget(strip);
        auto* hl = new QHBoxLayout(line);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(8);
        auto* tag = new QLabel(ma_pf_role_title(role), line);
        tag->setObjectName(QStringLiteral("maStripRole"));
        tag->setFixedWidth(QFontMetrics(ui::fonts::font(-3, QFont::Bold)).horizontalAdvance(QStringLiteral("ACQUIRER")) + 12);
        hl->addWidget(tag);

        services::ResolveOptions opts;
        opts.ctx.allowed_classes = {core::instrument::AssetClass::Equity};
        auto* box = new ui::SymbolSearchBox(line);
        // Its text is remembered (module memory, by this object name) and preset on the next start.
        box->setObjectName(QStringLiteral("ma_company_") + role);
        box->set_compact(true);
        box->set_clear_on_pick(false);
        box->set_options(opts);
        box->set_recent_key(QStringLiteral("ma_company"));
        box->set_placeholder(role == QLatin1String("acquirer") ? tr("Acquirer: ticker or company name")
                                                               : tr("Company: ticker or company name"));
        box->setMinimumWidth(QFontMetrics(ui::fonts::font(-1)).averageCharWidth() * 24);
        hl->addWidget(box, 2);

        auto* load = new QPushButton(tr("LOAD"), line);
        load->setObjectName(QStringLiteral("maStripLoad"));
        load->setCursor(Qt::PointingHandCursor);
        load->setToolTip(tr("Fill this module's inputs from the company's market data and latest statements"));
        connect(load, &QPushButton::clicked, this, [this, role]() { load_company(role); });
        connect(box, &ui::SymbolSearchBox::instrument_picked, this,
                [this, role](const fincept::core::instrument::Instrument&) { load_company(role); });
        hl->addWidget(load);

        auto* status = new QLabel(line);
        status->setObjectName(QStringLiteral("maStripStatus"));
        status->setWordWrap(true);
        status->setText(tr("Load a company to fill the inputs from its market data and statements; every filled "
                           "field says where it came from."));
        hl->addWidget(status, 3);

        company_boxes_.insert(role, box);
        company_status_.insert(role, status);
        vl->addWidget(line);
    }
    return strip;
}

void MAModulePanel::load_company(const QString& role) {
    ui::SymbolSearchBox* box = company_boxes_.value(role);
    if (!box)
        return;
    const auto inst = box->current_instrument();
    const QString symbol = (inst.is_valid() && !inst.symbol.isEmpty() ? inst.symbol : box->text().trimmed()).toUpper();
    QLabel* status = company_status_.value(role);
    if (symbol.isEmpty()) {
        if (status)
            status->setText(tr("Enter a ticker or company name first."));
        return;
    }
    // Every form must exist before it is filled (the forms are built lazily).
    build_all_sub_tabs();
    const quint64 seq = company_requests_.value(role) + 1;
    company_requests_.insert(role, seq);
    if (status)
        status->setText(tr("Loading %1…").arg(symbol));
    LOG_INFO("MAAnalytics", QStringLiteral("Company prefill requested module=%1 role=%2").arg(module_.key, role));
    QPointer<MAModulePanel> self = this;
    CompanySnapshotService::instance().fetch(symbol, this, [self, role, seq, symbol](const CompanySnapshot& s) {
        if (!self || self->company_requests_.value(role) != seq)
            return; // a newer LOAD of this role is under way
        QLabel* st = self->company_status_.value(role);
        if (!s.ok) {
            if (st)
                st->setText(tr("No financial data found for %1. Enter the figures by hand.%2")
                                .arg(symbol, s.error.isEmpty() ? QString() : QStringLiteral(" (") + s.error + QLatin1Char(')')));
            return;
        }
        self->apply_snapshot(role, s);
    });
}

bool MAModulePanel::prefill(const QString& name, double value, const QString& caption) {
    if (!std::isfinite(value))
        return false;
    QAbstractSpinBox* w = double_inputs_.value(name, nullptr);
    if (!w)
        w = int_inputs_.value(name, nullptr);
    if (!w)
        return false;
    if (user_edited_.contains(w))
        return false; // the user's own figure wins for the same company
    prefilling_ = true;
    if (auto* d = qobject_cast<QDoubleSpinBox*>(w)) {
        if (value > d->maximum() || value < d->minimum()) {
            prefilling_ = false;
            return false; // outside what the field accepts: leave it
        }
        d->setValue(value);
    } else if (auto* i = qobject_cast<QSpinBox*>(w)) {
        i->setValue(int(std::lround(value)));
    }
    prefilling_ = false;
    if (QLabel* cap = source_labels_.value(w)) {
        cap->setText(caption);
        cap->setToolTip(tr("Filled from market data / filings; edit the field to use your own figure"));
        cap->setVisible(true);
    }
    if (!w->property("maPrefillHooked").toBool()) {
        w->setProperty("maPrefillHooked", true);
        auto mark = [this, w]() {
            if (prefilling_)
                return;
            user_edited_.insert(w);
            if (QLabel* cap = source_labels_.value(w))
                cap->setVisible(false);
        };
        if (auto* d = qobject_cast<QDoubleSpinBox*>(w))
            connect(d, &QDoubleSpinBox::valueChanged, this, mark);
        else if (auto* i = qobject_cast<QSpinBox*>(w))
            connect(i, &QSpinBox::valueChanged, this, mark);
    }
    return true;
}

void MAModulePanel::apply_snapshot(const QString& role, const CompanySnapshot& s) {
    // A different company: every field is filled again (the user's edits belonged to the previous one).
    if (prefilled_symbol_.value(role) != s.symbol) {
        QSet<QWidget*> keep;
        for (QWidget* w : std::as_const(user_edited_)) {
            // Only the inputs this role fills are released; Merger's other side keeps its edits.
            bool mine = false;
            for (auto it = double_inputs_.cbegin(); it != double_inputs_.cend() && !mine; ++it)
                if (it.value() == w)
                    mine = (role == QLatin1String("acquirer")) == it.key().startsWith(QLatin1String("acq_"));
            if (!mine)
                keep.insert(w);
        }
        user_edited_ = keep;
    }
    prefilled_symbol_.insert(role, s.symbol);
    snapshots_.insert(role, s);
    // Kept for the football field's 52-week range after a restart (30 days; the "as of" travels with it).
    fincept::CacheManager::instance().put(QStringLiteral("ma_analytics:snapshot:%1:%2").arg(module_.key, role),
                                          QString::fromUtf8(QJsonDocument(s.to_json()).toJson(QJsonDocument::Compact)),
                                          30 * 24 * 3600, QStringLiteral("ma_analytics"));

    int filled = 0;
    auto fill = [&](const QString& name, const char* field, double scale = 1.0) {
        if (s.has(field) && prefill(name, s.value(field) * scale, ma_pf_caption(s, field)))
            ++filled;
    };
    const bool acq = role == QLatin1String("acquirer");
    switch (module_.id) {
        case services::ma::ModuleId::Valuation: {
            fill(QStringLiteral("dcf_ebit"), CompanySnapshot::kEbit);
            fill(QStringLiteral("dcf_tax"), CompanySnapshot::kTaxRate, 100.0);
            fill(QStringLiteral("dcf_beta"), CompanySnapshot::kBeta);
            fill(QStringLiteral("dcf_shares"), CompanySnapshot::kShares);
            fill(QStringLiteral("dcf_mktcap"), CompanySnapshot::kMarketCap);
            fill(QStringLiteral("dcf_debt"), CompanySnapshot::kTotalDebt);
            fill(QStringLiteral("dcf_cash"), CompanySnapshot::kCash);
            fill(QStringLiteral("dcf_depreciation"), CompanySnapshot::kDepreciation);
            fill(QStringLiteral("lbo_entry"), CompanySnapshot::kEnterpriseValue);
            fill(QStringLiteral("lbof_ebitda"), CompanySnapshot::kEbitda);
            fill(QStringLiteral("ds_ebitda"), CompanySnapshot::kEbitda);
            if (s.has(CompanySnapshot::kEbitda) && s.has(CompanySnapshot::kRevenue) && s.revenue_ltm > 0 &&
                prefill(QStringLiteral("lbof_margin"), s.ebitda_ltm / s.revenue_ltm * 100.0, ma_pf_caption(s, CompanySnapshot::kEbitda)))
                ++filled;
            fill(QStringLiteral("prec_revenue"), CompanySnapshot::kRevenue);
            fill(QStringLiteral("prec_ebitda"), CompanySnapshot::kEbitda);
            if (s.has(CompanySnapshot::kPrice) && prefill(QStringLiteral("ff_price"), s.price, ma_pf_caption(s, CompanySnapshot::kPrice)))
                ++filled;
            if (auto* t = findChild<ui::SymbolSearchBox*>(QStringLiteral("comps_target_symbol")))
                t->set_text(s.symbol);
            // Risk-free rate for the DCF: the 10-year government yield of the company's currency, live only.
            const QString ccy = s.financial_currency.isEmpty() ? s.currency : s.financial_currency;
            if (!ccy.isEmpty()) {
                QPointer<MAModulePanel> self = this;
                const QString symbol = s.symbol;
                services::equity::EquityResearchService::instance().fetch_risk_free(
                    ccy, [self, symbol](bool ok, services::equity::RiskFreeRate rf, QString) {
                        if (!self || self->prefilled_symbol_.value(QStringLiteral("target")) != symbol || !ok)
                            return;
                        if (rf.source == QLatin1String("reference"))
                            return; // a static reference figure is not market data: the field keeps its value
                        self->prefill(QStringLiteral("dcf_risk_free"), rf.rate_pct,
                                      rf.label.left(24) + QStringLiteral(", ") + rf.as_of);
                    });
            }
            break;
        }
        case services::ma::ModuleId::Merger: {
            const QString p = acq ? QStringLiteral("acq_") : QStringLiteral("tgt_");
            fill(p + QStringLiteral("revenue"), CompanySnapshot::kRevenue);
            fill(p + QStringLiteral("net_income"), CompanySnapshot::kNetIncome);
            if (acq) {
                fill(QStringLiteral("acq_ebitda"), CompanySnapshot::kEbitda);
                fill(QStringLiteral("acq_shares"), CompanySnapshot::kShares);
                fill(QStringLiteral("acq_share_price"), CompanySnapshot::kPrice);
                fill(QStringLiteral("acq_tax_rate"), CompanySnapshot::kTaxRate, 100.0);
                fill(QStringLiteral("exch_acq_price"), CompanySnapshot::kPrice);
                fill(QStringLiteral("pay_acq_price"), CompanySnapshot::kPrice);
                fill(QStringLiteral("pay_acq_shares"), CompanySnapshot::kShares);
                fill(QStringLiteral("collar_announce_price"), CompanySnapshot::kPrice);
            } else {
                fill(QStringLiteral("exch_tgt_price"), CompanySnapshot::kPrice);
                fill(QStringLiteral("pay_tgt_shares"), CompanySnapshot::kShares);
                fill(QStringLiteral("collar_tgt_shares"), CompanySnapshot::kShares);
            }
            break;
        }
        case services::ma::ModuleId::Fairness: {
            fill(QStringLiteral("pa_shares"), CompanySnapshot::kShares);
            fill(QStringLiteral("pa_price_52w"), CompanySnapshot::kWeek52High);
            fetch_reference_prices(s.symbol, ma_pf_caption(s, CompanySnapshot::kPrice));
            break;
        }
        default:
            break;
    }

    QStringList parts;
    parts << (s.name.isEmpty() ? s.symbol : s.name);
    if (!s.caption().isEmpty())
        parts << s.caption();
    parts << tr("%n field(s) filled", nullptr, filled);
    if (!s.missing.isEmpty())
        parts << tr("not available: %1").arg(s.missing_labels().mid(0, 5).join(QStringLiteral(", ")) +
                                              (s.missing.size() > 5 ? QStringLiteral("…") : QString()));
    if (!s.notes.isEmpty())
        parts << s.notes.first();
    if (QLabel* st = company_status_.value(role)) {
        st->setText(parts.join(QStringLiteral("  ·  ")));
        st->setToolTip(s.missing_labels().join(QStringLiteral("\n")));
    }
    LOG_INFO("MAAnalytics",
             QStringLiteral("Company prefill applied module=%1 role=%2 filled=%3 missing=%4")
                 .arg(module_.key, role)
                 .arg(filled)
                 .arg(s.missing.size()));
}

void MAModulePanel::fetch_reference_prices(const QString& symbol, const QString& source_caption) {
    const quint64 seq = ++history_request_;
    QPointer<MAModulePanel> self = this;
    services::equity::EquityResearchService::instance().fetch_price_history(
        symbol, QStringLiteral("1y"), QStringLiteral("1d"),
        [self, seq, symbol, source_caption](bool ok, QVector<services::equity::Candle> candles, QString) {
            if (!self || self->history_request_ != seq || !ok || candles.isEmpty())
                return;
            // 1 day prior = the last close; 4-week average = the mean of the last 20 closes; 52-week high = the
            // highest close of the year (the snapshot's intraday high is used when it came first).
            const double last = candles.last().close;
            const int n = std::min<int>(20, static_cast<int>(candles.size()));
            double sum = 0;
            for (int i = static_cast<int>(candles.size()) - n; i < candles.size(); ++i)
                sum += candles.at(i).close;
            double hi = 0;
            for (const auto& c : candles)
                hi = std::max(hi, c.close);
            const qint64 ts = candles.last().timestamp;
            const qint64 secs = ts > 100000000000LL ? ts / 1000 : ts; // seconds, or milliseconds from some sources
            const QString cap = QStringLiteral("Yahoo, ") +
                                fincept::ui::formatting::date(secs, fincept::ui::formatting::DateStyle::Short);
            Q_UNUSED(source_caption);
            self->prefill(QStringLiteral("pa_price_1d"), last, cap);
            self->prefill(QStringLiteral("pa_price_4w"), sum / n, cap + QStringLiteral(" (20-day avg)"));
            if (!self->snapshots_.value(QStringLiteral("target")).has(CompanySnapshot::kWeek52High))
                self->prefill(QStringLiteral("pa_price_52w"), hi, cap + QStringLiteral(" (closes)"));
        });
}

void MAModulePanel::suggest_peers() {
    auto* target = findChild<ui::SymbolSearchBox*>(QStringLiteral("comps_target_symbol"));
    auto* peers = combo_inputs_.value(QStringLiteral("comps_peers"));
    if (!target || !peers)
        return;
    const auto inst = target->current_instrument();
    const QString symbol = (inst.is_valid() && !inst.symbol.isEmpty() ? inst.symbol : target->text().trimmed()).toUpper();
    if (symbol.isEmpty()) {
        status_label_->setText(tr("Pick the target company first."));
        return;
    }
    peers_symbol_ = symbol;
    const quint64 seq = ++peers_request_;
    status_label_->setText(tr("Finding peers of %1…").arg(symbol));
    auto& eq = services::equity::EquityResearchService::instance();
    auto conn = std::make_shared<QMetaObject::Connection>();
    QPointer<MAModulePanel> self = this;
    *conn = connect(&eq, &services::equity::EquityResearchService::suggested_peers_loaded, this,
                    [self, seq, conn, peers](const QString& sym, const QStringList& list) {
                        if (!self || seq != self->peers_request_ || sym.compare(self->peers_symbol_, Qt::CaseInsensitive) != 0)
                            return;
                        QObject::disconnect(*conn);
                        if (list.isEmpty()) {
                            self->status_label_->setText(tr("No peers suggested for %1 - enter them by hand.").arg(sym));
                            return;
                        }
                        peers->setEditText(list.join(QLatin1Char(',')));
                        self->status_label_->setText(tr("%n peer(s) suggested - check them, then RUN.", nullptr,
                                                        int(list.size())));
                    });
    // No answer within 30 s: stop listening (the request may still fill the cache for a later try).
    QTimer::singleShot(30000, this, [self, seq, conn]() {
        if (self && seq == self->peers_request_ && *conn) {
            QObject::disconnect(*conn);
            self->status_label_->setText(tr("No peer suggestion arrived - enter the tickers by hand."));
        }
    });
    eq.fetch_suggested_peers(symbol, 6);
}

} // namespace fincept::screens
