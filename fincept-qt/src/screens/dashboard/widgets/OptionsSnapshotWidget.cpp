#include "screens/dashboard/widgets/OptionsSnapshotWidget.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "screens/dashboard/widgets/DashboardFormat.h"
#include "services/databento/DatabentoService.h"
#include "services/options/OptionChainService.h"
#include "services/options/OptionChainTypes.h"
#include "trading/AccountManager.h"
#include "trading/BrokerInterface.h"
#include "trading/BrokerRegistry.h"
#include "trading/instruments/InstrumentService.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QPointer>
#include <QStringList>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::widgets {

namespace {

using fincept::services::options::OptionChain;
using fincept::services::options::OptionChainService;
using fincept::trading::AccountManager;
using fincept::trading::ConnectionState;
using fincept::trading::InstrumentService;

const QString kOptSnapDatabento = QStringLiteral("databento");
const QString kOptSnapFnoScreen = QStringLiteral("fno");
const QString kOptSnapDefaultUnderlying = QStringLiteral("NIFTY");
constexpr const char* kOptSnapTag = "OptionsSnapshot";
constexpr const char* kOptSnapStaleProp = "optSnapStale";

QString opt_snap_normalize_underlying(const QString& s) {
    const QString t = s.trimmed().toUpper();
    return t.isEmpty() ? kOptSnapDefaultUnderlying : t;
}

/// Expiry strings are "DD-MMM-YY" (broker instrument masters) or "YYYY-MM-DD"
/// (Databento). The two-digit year is 20xx — QDate's "yy" would read it as 19xx.
QDate opt_snap_parse_expiry(const QString& s) {
    static const char* const kMonths[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                          "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    if (s.size() >= 9 && s.at(2) == QLatin1Char('-') && s.at(6) == QLatin1Char('-')) {
        const int day = QStringView(s).left(2).toInt();
        const QStringView mon = QStringView(s).mid(3, 3);
        const int year = 2000 + QStringView(s).mid(7, 2).toInt();
        for (int m = 0; m < 12; ++m) {
            if (mon.compare(QLatin1String(kMonths[m]), Qt::CaseInsensitive) == 0)
                return QDate(year, m + 1, day);
        }
        return {};
    }
    return QDate::fromString(s, Qt::ISODate);
}

/// Nearest expiry that has not passed. An unparseable label is only used when
/// nothing parses (the order then is the provider's, which lists nearest first).
QString opt_snap_pick_expiry(const QStringList& expiries) {
    const QDate today = QDate::currentDate();
    QString best;
    QDate best_date;
    QString first_unparsed;
    for (const QString& e : expiries) {
        const QDate d = opt_snap_parse_expiry(e);
        if (!d.isValid()) {
            if (first_unparsed.isEmpty())
                first_unparsed = e;
            continue;
        }
        if (d >= today && (!best_date.isValid() || d < best_date)) {
            best_date = d;
            best = e;
        }
    }
    return best.isEmpty() ? first_unparsed : best;
}

/// NFO option chains exist only on Indian brokers (OptionChainService reads the
/// broker's NFO instruments); Databento is handled separately.
bool opt_snap_is_nfo_broker(const QString& broker_id) {
    const auto* broker = fincept::trading::BrokerRegistry::instance().get(broker_id);
    return broker && broker->profile().region == QLatin1String("IN");
}

QString opt_snap_broker_label(const QString& broker_id) {
    if (broker_id == kOptSnapDatabento)
        return QStringLiteral("Databento");
    if (const auto* broker = fincept::trading::BrokerRegistry::instance().get(broker_id)) {
        const QString name = broker->profile().display_name;
        if (!name.isEmpty())
            return name;
    }
    return broker_id;
}

/// ATM IV (decimal) of a chain snapshot — the ATM row's CE/PE IV average, as
/// OptionChainService::publish_atm_iv computes it. `enriched` is false while no
/// leg carries an IV yet (the first, pre-Greeks publish of a REST refresh).
double opt_snap_atm_iv(const OptionChain& chain, bool& enriched) {
    enriched = false;
    bool atm_seen = false;
    double ce = 0;
    double pe = 0;
    for (const auto& row : chain.rows) {
        if (row.ce_iv > 0 || row.pe_iv > 0)
            enriched = true;
        if (row.is_atm && !atm_seen) {
            atm_seen = true;
            ce = row.ce_iv;
            pe = row.pe_iv;
        }
        if (enriched && atm_seen)
            break;
    }
    if (ce > 0 && pe > 0)
        return 0.5 * (ce + pe);
    if (ce > 0)
        return ce;
    return pe > 0 ? pe : 0.0;
}

/// Strike / max-pain label: grouped, with format_strike()'s decimals rule (a
/// fractional stock strike such as 1272.5 keeps its decimals).
QString opt_snap_strike_text(double strike) {
    if (!std::isfinite(strike) || strike <= 0)
        return QStringLiteral("--");
    const bool whole = std::abs(strike - std::round(strike)) < 1e-9;
    return dash_fmt::grouped(strike, (whole && strike >= 100) ? 0 : 2);
}

/// Toggle a boolean dynamic property that the tile stylesheet selects on, and
/// re-polish only when it actually changed (no stylesheet re-parse).
void opt_snap_set_flag(QWidget* w, const char* prop, bool on) {
    if (!w || w->property(prop).toBool() == on)
        return;
    w->setProperty(prop, on);
    w->style()->unpolish(w);
    w->style()->polish(w);
}

} // namespace

OptionsSnapshotWidget::OptionsSnapshotWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget(tr("OPTIONS · %1").arg(opt_snap_normalize_underlying(cfg.value("underlying").toString())), parent),
      underlying_(opt_snap_normalize_underlying(cfg.value("underlying").toString())),
      broker_cfg_(cfg.value("broker").toString().trimmed()) {
    auto* vl = content_layout();
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    root_ = new QWidget(this);
    root_->setObjectName("optSnapRoot");
    auto* rl = new QVBoxLayout(root_);
    rl->setContentsMargins(8, 6, 8, 6);
    rl->setSpacing(6);

    // Series line: "NIFTY · 27-NOV-25"            "ZERODHA"
    auto* head = new QHBoxLayout;
    head->setContentsMargins(0, 0, 0, 0);
    head->setSpacing(6);
    series_label_ = new QLabel(root_);
    series_label_->setObjectName("optSnapSeries");
    broker_label_ = new QLabel(root_);
    broker_label_->setObjectName("optSnapBroker");
    broker_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    head->addWidget(series_label_, 1);
    head->addWidget(broker_label_, 0);
    rl->addLayout(head);

    // Three stat boxes. Each box holds exactly a value and a caption label, so
    // screen readers / ui_snapshot read it as one "PCR: 1.12" line (M1).
    auto* stats = new QHBoxLayout;
    stats->setContentsMargins(0, 0, 0, 0);
    stats->setSpacing(6);
    auto make_stat = [this, stats](QLabel*& value, QLabel*& caption) {
        auto* box = new QFrame(root_);
        box->setObjectName("optSnapStat");
        auto* bl = new QVBoxLayout(box);
        bl->setContentsMargins(6, 8, 6, 8);
        bl->setSpacing(2);
        value = new QLabel(QStringLiteral("--"), box);
        value->setObjectName("optSnapValue");
        value->setAlignment(Qt::AlignCenter);
        caption = new QLabel(box);
        caption->setObjectName("optSnapCaption");
        caption->setAlignment(Qt::AlignCenter);
        bl->addWidget(value);
        bl->addWidget(caption);
        stats->addWidget(box, 1);
    };
    make_stat(pcr_value_, pcr_caption_);
    make_stat(pain_value_, pain_caption_);
    make_stat(iv_value_, iv_caption_);
    rl->addLayout(stats);

    footer_label_ = new QLabel(root_);
    footer_label_->setObjectName("optSnapFooter");
    footer_label_->setWordWrap(true);
    rl->addWidget(footer_label_);
    rl->addStretch(1);

    vl->addWidget(root_, 1);

    connect(this, &BaseWidget::refresh_requested, this, &OptionsSnapshotWidget::on_refresh);

    // Bring the tile to life when what it was waiting for shows up, without a
    // manual retry: a broker account added / connected, or the instrument
    // download for the chosen broker finishing (e.g. after the user opens F&O).
    auto& am = AccountManager::instance();
    connect(&am, &AccountManager::connection_state_changed, this,
            [this](const QString& account_id, ConnectionState state) {
                if (!live_ || pending_async_)
                    return;
                if (awaiting_broker_) {
                    resolve_and_subscribe();
                    return;
                }
                if (state != ConnectionState::Connected || have_values_)
                    return;
                // Nothing on screen yet (e.g. the picked account's token was dead).
                // Auto mode: move to a broker that just connected. Same broker:
                // retry the chain now instead of waiting out min_interval.
                if (broker_cfg_.isEmpty() && pick_auto_broker() != broker_) {
                    resolve_and_subscribe();
                } else if (subscribed_ && !chain_topic_.isEmpty() &&
                           AccountManager::instance().get_account(account_id).broker_id == broker_) {
                    set_loading(true);
                    datahub::DataHub::instance().request(chain_topic_, /*force=*/true);
                }
            });
    connect(&am, &AccountManager::account_added, this, [this](const QString& /*account_id*/) {
        if (live_ && !pending_async_ && awaiting_broker_)
            resolve_and_subscribe();
    });
    connect(&InstrumentService::instance(), &InstrumentService::refresh_done, this,
            [this](const QString& broker_id, int /*count*/) {
                if (live_ && !subscribed_ && !pending_async_ && !waiting_instruments_for_.isEmpty() &&
                    broker_id == waiting_instruments_for_)
                    resolve_and_subscribe();
            });

    set_configurable(true);
    set_title(tr("OPTIONS · %1").arg(underlying_));
    apply_texts();
    apply_styles();
    update_header();
    render();
    set_loading(true);
}

QJsonObject OptionsSnapshotWidget::config() const {
    QJsonObject o;
    o.insert("underlying", underlying_);
    o.insert("broker", broker_cfg_);
    return o;
}

void OptionsSnapshotWidget::apply_config(const QJsonObject& cfg) {
    const QString under = opt_snap_normalize_underlying(cfg.value("underlying").toString());
    const QString broker = cfg.value("broker").toString().trimmed();
    if (under == underlying_ && broker == broker_cfg_)
        return;
    underlying_ = under;
    broker_cfg_ = broker;
    set_title(tr("OPTIONS · %1").arg(underlying_));

    drop_series();
    ++gen_; // drop any in-flight expiry / instrument lookup for the old selection
    pending_async_ = false;
    clear_values();
    values_topic_.clear();
    broker_.clear();
    expiry_.clear();
    cached_expiry_.clear();
    update_header();
    render();
    if (live_) {
        set_loading(true);
        resolve_and_subscribe();
    }
}

// ── Resolution ───────────────────────────────────────────────────────────────

QString OptionsSnapshotWidget::pick_auto_broker() const {
    // Same order as the F&O Chain tab: a connected account first, then any
    // active one, then Databento. Non-Indian brokers carry no NFO contracts.
    auto& am = AccountManager::instance();
    QString connected;
    QString active;
    for (const auto& acc : am.active_accounts()) {
        if (!opt_snap_is_nfo_broker(acc.broker_id))
            continue;
        if (active.isEmpty())
            active = acc.broker_id;
        if (connected.isEmpty() && am.connection_state(acc.account_id) == ConnectionState::Connected)
            connected = acc.broker_id;
    }
    if (!connected.isEmpty())
        return connected;
    if (!active.isEmpty())
        return active;
    if (fincept::DatabentoService::instance().has_api_key() &&
        OptionChainService::instance().list_underlyings(kOptSnapDatabento).contains(underlying_))
        return kOptSnapDatabento;
    return {};
}

void OptionsSnapshotWidget::resolve_and_subscribe() {
    drop_series();
    const int gen = ++gen_;
    pending_async_ = false;
    awaiting_broker_ = false;
    waiting_instruments_for_.clear();
    if (!live_)
        return;

    const QString broker = broker_cfg_.isEmpty() ? pick_auto_broker() : broker_cfg_;
    if (broker.isEmpty()) {
        awaiting_broker_ = true;
        broker_.clear();
        expiry_.clear();
        update_header();
        set_error(tr("No F&O broker connected.\nConnect an Indian broker account in Equity Trading, or set a "
                     "Databento key (Settings › Credentials) and pick a US underlying such as SPY via the gear icon."));
        return;
    }

    const QString label = opt_snap_broker_label(broker);
    auto& svc = OptionChainService::instance();
    const bool databento = broker == kOptSnapDatabento;

    // Cheap validity checks first — they also guard the cached-expiry shortcut.
    if (databento) {
        if (!fincept::DatabentoService::instance().has_api_key()) {
            set_error(tr("Databento is selected but no API key is set.\nEnter it in Settings › Credentials, or pick "
                         "another broker via the gear icon."));
            return;
        }
        if (!svc.list_underlyings(kOptSnapDatabento).contains(underlying_)) {
            set_error(
                tr("%1 is not in the Databento options list.\nPick a US underlying such as SPY via the gear icon.")
                    .arg(underlying_));
            return;
        }
    } else if (AccountManager::instance().list_accounts(broker).isEmpty()) {
        awaiting_broker_ = true;
        set_error(tr("No %1 account configured.\nConnect one in Equity Trading, or switch broker via the gear icon.")
                      .arg(label));
        return;
    }

    // Same broker + underlying as the last resolution and its expiry still
    // ahead: reuse it instead of walking the instrument cache again.
    if (broker == cached_broker_ && underlying_ == cached_underlying_ && !cached_expiry_.isEmpty() &&
        (!cached_expiry_date_.isValid() || cached_expiry_date_ >= QDate::currentDate())) {
        subscribe_series(broker, cached_expiry_);
        return;
    }

    broker_ = broker;
    expiry_.clear();
    update_header();
    if (!have_values_)
        set_loading(true);

    if (databento) {
        const QStringList cached = svc.list_expiries(kOptSnapDatabento, underlying_);
        if (!cached.isEmpty()) {
            subscribe_with_expiries(broker, cached);
            return;
        }
        pending_async_ = true;
        QPointer<OptionsSnapshotWidget> self = this;
        svc.list_databento_expiries(underlying_, [self, gen](const QStringList& expiries) {
            if (!self || gen != self->gen_)
                return;
            self->pending_async_ = false;
            if (!self->live_)
                return; // hidden meanwhile — the next show resolves from the now-cached list
            if (expiries.isEmpty()) {
                self->set_error(OptionsSnapshotWidget::tr("No Databento expiries found for %1.\nCheck the API key and "
                                                          "OPRA access in Settings › Credentials.")
                                    .arg(self->underlying_));
                return;
            }
            self->subscribe_with_expiries(kOptSnapDatabento, expiries);
        });
        return;
    }

    auto& instruments = InstrumentService::instance();
    if (instruments.is_loaded(broker)) {
        resolve_expiry(broker);
        return;
    }
    // Cold instrument cache: load it from SQLite off the UI thread. The broker
    // download itself is left to the F&O screen — a dashboard tile should not
    // pull a full instrument master on every restore.
    pending_async_ = true;
    waiting_instruments_for_ = broker;
    QPointer<OptionsSnapshotWidget> self = this;
    instruments.load_from_db_async(broker, [self, gen, broker, label](int count) {
        if (!self || gen != self->gen_)
            return;
        self->pending_async_ = false;
        if (!self->live_)
            return; // hidden meanwhile — the next show resolves against the loaded cache
        if (count <= 0) {
            // waiting_instruments_for_ stays set: refresh_done(broker) re-resolves.
            self->set_error(OptionsSnapshotWidget::tr("No %1 instruments downloaded yet.\nOpen F&O once to download "
                                                      "the instrument list — this tile then fills in.")
                                .arg(label));
            return;
        }
        self->waiting_instruments_for_.clear();
        self->resolve_expiry(broker);
    });
}

void OptionsSnapshotWidget::resolve_expiry(const QString& broker) {
    const QStringList expiries = OptionChainService::instance().list_expiries(broker, underlying_);
    if (expiries.isEmpty()) {
        set_error(tr("No %1 option contracts in the %2 instrument list.\nPick another underlying via the gear icon, "
                     "or open F&O to refresh the instruments.")
                      .arg(underlying_, opt_snap_broker_label(broker)));
        return;
    }
    subscribe_with_expiries(broker, expiries);
}

void OptionsSnapshotWidget::subscribe_with_expiries(const QString& broker, const QStringList& expiries) {
    const QString expiry = opt_snap_pick_expiry(expiries);
    if (expiry.isEmpty()) {
        set_error(tr("Every listed %1 expiry has passed — the %2 instrument list is out of date.\nOpen F&O to refresh "
                     "it.")
                      .arg(underlying_, opt_snap_broker_label(broker)));
        return;
    }
    subscribe_series(broker, expiry);
}

void OptionsSnapshotWidget::subscribe_series(const QString& broker, const QString& expiry) {
    pending_async_ = false;
    awaiting_broker_ = false;
    waiting_instruments_for_.clear();

    const QString chain = OptionChainService::instance().chain_topic(broker, underlying_, expiry);
    if (chain != values_topic_) {
        // Never show one series' numbers under another series' header.
        clear_values();
        values_topic_ = chain;
    }
    const QString series = broker + QLatin1Char(':') + underlying_ + QLatin1Char(':') + expiry;
    broker_ = broker;
    expiry_ = expiry;
    chain_topic_ = chain;
    pcr_topic_ = QStringLiteral("fno:pcr:") + series;
    max_pain_topic_ = QStringLiteral("fno:max_pain:") + series;
    cached_broker_ = broker;
    cached_underlying_ = underlying_;
    cached_expiry_ = expiry;
    cached_expiry_date_ = opt_snap_parse_expiry(expiry);

    update_header();
    render();
    set_loading(!have_values_); // values for this series already on screen stay visible (P11)

    auto& hub = datahub::DataHub::instance();
    QPointer<OptionsSnapshotWidget> self = this;
    const QString pcr_topic = pcr_topic_;
    const QString pain_topic = max_pain_topic_;
    // The derived topics are push-only: they carry data only because the chain
    // subscription below makes OptionChainService refresh this chain.
    hub.subscribe(this, pcr_topic, [self, pcr_topic](const QVariant& v) {
        if (self)
            self->on_scalar(pcr_topic, v, /*is_pcr=*/true);
    });
    hub.subscribe(this, pain_topic, [self, pain_topic](const QVariant& v) {
        if (self)
            self->on_scalar(pain_topic, v, /*is_pcr=*/false);
    });
    // Chain last — subscribing it fires the cold-start refresh when no fresh
    // snapshot is cached, and the hub scheduler keeps it refreshed while the
    // tile is visible (TTL / min_interval set by OptionChainService).
    hub.subscribe(this, chain, [self, chain](const QVariant& v) {
        if (self)
            self->on_chain(chain, v);
    });
    hub.subscribe_errors(this, chain, [self, chain](const QString& error) {
        if (self)
            self->on_chain_error(chain, error);
    });
    subscribed_ = true;
    LOG_INFO(kOptSnapTag, QString("Subscribed to %1").arg(chain));
}

void OptionsSnapshotWidget::drop_series() {
    // Drops data AND error subscriptions owned by this tile.
    datahub::DataHub::instance().unsubscribe(this);
    subscribed_ = false;
    chain_topic_.clear();
    pcr_topic_.clear();
    max_pain_topic_.clear();
}

void OptionsSnapshotWidget::clear_values() {
    pcr_ = 0;
    max_pain_ = 0;
    atm_iv_ = 0;
    spot_ = 0;
    atm_strike_ = 0;
    have_values_ = false;
    refresh_failed_ = false;
}

// ── Hub deliveries ──────────────────────────────────────────────────────────

void OptionsSnapshotWidget::on_scalar(const QString& topic, const QVariant& v, bool is_pcr) {
    // A queued delivery for a series we have since left must not land here.
    if (topic.isEmpty() || topic != (is_pcr ? pcr_topic_ : max_pain_topic_))
        return;
    bool ok = false;
    const double value = v.toDouble(&ok);
    if (!ok || !std::isfinite(value))
        return;
    if (is_pcr)
        pcr_ = value;
    else
        max_pain_ = value;
    have_values_ = true;
    set_loading(false);
    schedule_render([this]() { render(); });
}

void OptionsSnapshotWidget::on_chain(const QString& topic, const QVariant& v) {
    if (topic.isEmpty() || topic != chain_topic_ || !v.canConvert<OptionChain>())
        return;
    const OptionChain chain = v.value<OptionChain>();
    spot_ = chain.spot;
    atm_strike_ = chain.atm_strike;
    bool enriched = false;
    const double iv = opt_snap_atm_iv(chain, enriched);
    // The first publish of a REST refresh carries no IV yet (Greeks follow in a
    // republish): keep the last solved value instead of flashing "--".
    if (iv > 0 || enriched)
        atm_iv_ = iv;
    refresh_failed_ = false;
    have_values_ = true;
    set_loading(false);
    schedule_render([this]() { render(); });
}

void OptionsSnapshotWidget::on_chain_error(const QString& topic, const QString& error) {
    if (topic.isEmpty() || topic != chain_topic_)
        return;
    LOG_WARN(kOptSnapTag, QString("Chain refresh failed for %1: %2").arg(topic, error));
    if (have_values_) {
        // Last-known-good stays visible (P11); the footer flags the failure.
        refresh_failed_ = true;
        schedule_render([this]() { render(); });
        return;
    }
    if (broker_ == kOptSnapDatabento) {
        set_error(tr("Option chain unavailable: %1\nCheck the Databento API key in Settings › Credentials, then click "
                     "refresh.")
                      .arg(error));
    } else {
        set_error(tr("Option chain unavailable: %1\nCheck that your %2 account is connected in Equity Trading, then "
                     "click refresh.")
                      .arg(error, opt_snap_broker_label(broker_)));
    }
}

void OptionsSnapshotWidget::on_refresh() {
    if (!live_)
        return;
    if (subscribed_ && !chain_topic_.isEmpty()) {
        if (!have_values_ || is_showing_error())
            set_loading(true);
        datahub::DataHub::instance().request(chain_topic_, /*force=*/true);
        return;
    }
    set_loading(true);
    if (pending_async_)
        return; // an instrument load / expiry fetch is already on its way
    // Unresolved (no broker / instruments / expiry): start over and re-read the
    // expiry list rather than trusting the cached pick.
    cached_expiry_.clear();
    resolve_and_subscribe();
}

// ── Rendering ───────────────────────────────────────────────────────────────

void OptionsSnapshotWidget::update_header() {
    series_label_->setText(expiry_.isEmpty() ? underlying_ : underlying_ + QStringLiteral(" · ") + expiry_);
    broker_label_->setText(broker_.isEmpty() ? QString() : opt_snap_broker_label(broker_).toUpper());
}

void OptionsSnapshotWidget::render() {
    pcr_value_->setText(pcr_ > 0 ? QString::number(pcr_, 'f', 2) : QStringLiteral("--"));
    pain_value_->setText(opt_snap_strike_text(max_pain_));
    iv_value_->setText(atm_iv_ > 0 ? QString::number(atm_iv_ * 100.0, 'f', 1) + QLatin1Char('%')
                                   : QStringLiteral("--"));

    if (max_pain_ > 0 && spot_ > 0) {
        const double gap_pct = (max_pain_ - spot_) / spot_ * 100.0;
        pain_value_->setToolTip(tr("Max pain %1 is %2% %3 spot %4")
                                    .arg(opt_snap_strike_text(max_pain_))
                                    .arg(std::abs(gap_pct), 0, 'f', 2)
                                    .arg(gap_pct >= 0 ? tr("above") : tr("below"))
                                    .arg(dash_fmt::price(spot_)));
    } else {
        pain_value_->setToolTip(QString());
    }

    QStringList parts;
    parts << tr("SPOT %1").arg(dash_fmt::price(spot_));
    parts << tr("ATM %1").arg(opt_snap_strike_text(atm_strike_));
    if (!values_topic_.isEmpty()) {
        auto& hub = datahub::DataHub::instance();
        const qint64 ms = std::max(hub.last_publish_ms(values_topic_), hub.last_publish_ms(pcr_topic_));
        if (ms > 0)
            parts << tr("UPD %1").arg(QDateTime::fromMSecsSinceEpoch(ms).toString(QStringLiteral("HH:mm:ss")));
    }
    if (refresh_failed_)
        parts << tr("REFRESH FAILED — last good values shown");
    footer_label_->setText(parts.join(QStringLiteral(" · ")));
    opt_snap_set_flag(footer_label_, kOptSnapStaleProp, refresh_failed_);
}

void OptionsSnapshotWidget::apply_texts() {
    pcr_caption_->setText(tr("PCR"));
    pcr_caption_->setToolTip(tr("Put/Call Ratio — total put open interest ÷ total call open interest, this expiry"));
    pain_caption_->setText(tr("MAX PAIN"));
    pain_caption_->setToolTip(tr("Max Pain — the strike at which option writers pay out the least at expiry"));
    iv_caption_->setText(tr("ATM IV"));
    iv_caption_->setToolTip(tr("Implied volatility at the at-the-money strike (average of call and put)"));
    link_screen(root_, kOptSnapFnoScreen, tr("Double-click to open F&O"));
}

void OptionsSnapshotWidget::apply_styles() {
    // One stylesheet for the whole tile; children are styled by objectName.
    root_->setStyleSheet(
        QString("#optSnapRoot{background:transparent;}"
                "QLabel#optSnapSeries{color:%1;font-size:11px;font-weight:700;letter-spacing:0.5px;"
                "background:transparent;}"
                "QLabel#optSnapBroker{color:%2;font-size:9px;font-weight:700;letter-spacing:0.5px;"
                "background:transparent;}"
                "QFrame#optSnapStat{background:%3;border:1px solid %4;}"
                "QLabel#optSnapValue{color:%5;font-size:16px;font-weight:700;background:transparent;border:none;}"
                "QLabel#optSnapCaption{color:%2;font-size:9px;font-weight:700;letter-spacing:0.5px;"
                "background:transparent;border:none;}"
                "QLabel#optSnapFooter{color:%6;font-size:9px;background:transparent;}"
                "QLabel#optSnapFooter[optSnapStale=\"true\"]{color:%7;}")
            .arg(ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_TERTIARY(), ui::colors::BG_RAISED(),
                 ui::colors::BORDER_DIM(), ui::colors::CYAN(), ui::colors::TEXT_SECONDARY(), ui::colors::WARNING()));
}

void OptionsSnapshotWidget::on_theme_changed() {
    apply_styles();
}

void OptionsSnapshotWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("OPTIONS · %1").arg(underlying_));
    apply_texts();
    render();
}

// ── Visibility ──────────────────────────────────────────────────────────────

void OptionsSnapshotWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    live_ = true;
    if (!subscribed_ && !pending_async_)
        resolve_and_subscribe();
}

void OptionsSnapshotWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e); // drops every hub subscription this tile owns
    live_ = false;
    subscribed_ = false;
    // An instrument load / expiry fetch still in flight is left to finish: its
    // callback sees live_ == false and stops, and the next show resolves against
    // the warmed cache instead of starting a second load.
}

// ── Config dialog ───────────────────────────────────────────────────────────

QDialog* OptionsSnapshotWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle(tr("Configure — Options Snapshot"));
    auto* form = new QFormLayout(dlg);

    // Underlying: the chosen broker's full NFO list when its instruments are in
    // memory, else the index names every Indian broker lists.
    QStringList choices = {QStringLiteral("NIFTY"), QStringLiteral("BANKNIFTY"), QStringLiteral("FINNIFTY"),
                           QStringLiteral("MIDCPNIFTY")};
    const QString list_broker = !broker_cfg_.isEmpty() ? broker_cfg_ : broker_;
    if (list_broker == kOptSnapDatabento) {
        choices = OptionChainService::instance().list_underlyings(kOptSnapDatabento);
    } else if (!list_broker.isEmpty() && InstrumentService::instance().is_loaded(list_broker)) {
        const QStringList all = OptionChainService::instance().list_underlyings(list_broker);
        if (!all.isEmpty())
            choices = all;
    }
    if (!choices.contains(underlying_))
        choices.prepend(underlying_);

    auto* under = new QComboBox(dlg);
    under->setEditable(true);
    under->setInsertPolicy(QComboBox::NoInsert);
    under->addItems(choices);
    under->setCurrentText(underlying_);
    under->setAccessibleName(tr("Underlying"));
    form->addRow(tr("Underlying"), under);

    auto* broker = new QComboBox(dlg);
    broker->setAccessibleName(tr("Broker"));
    broker->addItem(tr("Auto — first connected F&O broker"), QString());
    QStringList seen;
    for (const auto& acc : AccountManager::instance().list_accounts()) {
        if (seen.contains(acc.broker_id))
            continue;
        seen.append(acc.broker_id);
        broker->addItem(opt_snap_broker_label(acc.broker_id), acc.broker_id);
    }
    broker->addItem(tr("Databento (US options)"), kOptSnapDatabento);
    const int current = broker->findData(broker_cfg_);
    broker->setCurrentIndex(current >= 0 ? current : 0);
    form->addRow(tr("Broker"), broker);

    auto* note = new QLabel(tr("Uses the nearest expiry and the same option chain as the F&O screen."), dlg);
    note->setWordWrap(true);
    form->addRow(note);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    form->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, dlg, [this, dlg, under, broker]() {
        QJsonObject cfg;
        cfg.insert("underlying", opt_snap_normalize_underlying(under->currentText()));
        cfg.insert("broker", broker->currentData().toString());
        apply_config(cfg);
        emit config_changed(cfg);
        dlg->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    return dlg;
}

} // namespace fincept::screens::widgets
