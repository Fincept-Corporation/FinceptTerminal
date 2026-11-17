#include "screens/fno/FnoHeaderBar.h"

#include "core/events/EventBus.h"
#include "screens/fno/FnoChainState.h"
#include "screens/fno/FnoFormat.h"
#include "storage/repositories/IvHistoryRepository.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"

#include <QDate>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHash>
#include <QHideEvent>
#include <QLocale>
#include <QMenu>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::fno {

using namespace fincept::ui;
using fincept::services::options::OptionChain;

namespace {

namespace fmt = fincept::ui::formatting;

QWidget* make_kv(QWidget* parent, QLabel*& value_out, QLabel*& key_out, const QString& key) {
    auto* wrap = new QWidget(parent);
    auto* lay = new QVBoxLayout(wrap);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    auto* k = new QLabel(key.toUpper(), wrap);
    k->setObjectName("fnoHdrKey");
    auto* v = new QLabel("--", wrap);
    v->setObjectName("fnoHdrValue");
    lay->addWidget(k);
    lay->addWidget(v);
    value_out = v;
    key_out = k;
    return wrap;
}

/// Where an F&O underlying goes in the rest of the terminal. Equity Research and News work on
/// yfinance symbols / free text; Equity Trading takes the broker's own symbol. Empty = not offered.
struct FnoUnderlyingTargets {
    QString research; // yfinance symbol
    QString news;     // news search text
    QString trading;  // broker symbol (stock underlyings only — an index has no order ticket)
    QString exchange;
};

FnoUnderlyingTargets fno_underlying_targets(const QString& broker_id, const QString& underlying) {
    const QString u = underlying.trimmed().toUpper();
    if (u.isEmpty())
        return {};
    if (fno_is_us_source(broker_id)) { // US listed options: the underlying is a US ticker
        if (fno_is_us_index_root(u)) // a US index (SPX / ^SPX): research + news, no order ticket
            return {fno_us_index_yahoo(u), fno_us_chain_symbol(u), {}, QStringLiteral("US")};
        return {u, u, u, QStringLiteral("US")};
    }
    struct IndexTarget {
        const char* research;
        const char* news;
    };
    static const QHash<QString, IndexTarget> kIndices = {
        {QStringLiteral("NIFTY"), {"^NSEI", "Nifty"}},
        {QStringLiteral("BANKNIFTY"), {"^NSEBANK", "Bank Nifty"}},
        {QStringLiteral("FINNIFTY"), {"NIFTY_FIN_SERVICE.NS", "Fin Nifty"}},
        {QStringLiteral("MIDCPNIFTY"), {"NIFTY_MID_SELECT.NS", "Midcap Nifty"}},
        {QStringLiteral("SENSEX"), {"^BSESN", "Sensex"}},
        {QStringLiteral("BANKEX"), {"", "Bankex"}},
    };
    const auto it = kIndices.constFind(u);
    if (it != kIndices.constEnd())
        return {QString::fromLatin1(it->research), QString::fromLatin1(it->news), {}, QStringLiteral("NSE")};
    return {u + QStringLiteral(".NS"), u, u, QStringLiteral("NSE")};
}

void fno_open_symbol(const QString& screen_id, const QString& symbol, const QString& exchange = {}) {
    QVariantMap payload{{QStringLiteral("screen_id"), screen_id}, {QStringLiteral("symbol"), symbol}};
    if (!exchange.isEmpty())
        payload[QStringLiteral("exchange")] = exchange;
    EventBus::instance().publish(QStringLiteral("nav.open_symbol"), payload);
}

} // namespace

FnoHeaderBar::FnoHeaderBar(QWidget* parent) : QWidget(parent) {
    setObjectName("fnoHeader");
    setStyleSheet(
        QString(
            "#fnoHeader { background:%1; border-bottom:2px solid %2; }"
            "#fnoHdrKey { color:%3; font-size:" + fincept::ui::fonts::px(-6) + "px; font-weight:700; letter-spacing:0.6px; background:transparent; }"
            "#fnoHdrValue { color:%4; font-size:" + fincept::ui::fonts::px(-1) + "px; font-weight:700; background:transparent; }"
            "#fnoHdrValuePos { color:%5; font-size:" + fincept::ui::fonts::px(-1) + "px; font-weight:700; background:transparent; }"
            "#fnoHdrValueNeg { color:%6; font-size:" + fincept::ui::fonts::px(-1) + "px; font-weight:700; background:transparent; }"
            "#fnoHdrStatus { color:%3; font-size:" + fincept::ui::fonts::px(-5) + "px; background:transparent; }"
            "QComboBox { background:%7; color:%4; border:1px solid %8; padding:3px 8px; font-size:" + fincept::ui::fonts::px(-3) + "px; "
            "min-width:90px; }"
            "QComboBox:hover { border-color:%2; }"
            "QComboBox::drop-down { border:none; width:18px; }"
            "QComboBox QAbstractItemView { background:%1; color:%4; border:1px solid %8; "
            "selection-background-color:%2; selection-color:%9; }"
            "#fnoRefreshBtn { background:%2; color:%9; border:none; padding:5px 12px; font-size:" + fincept::ui::fonts::px(-5) + "px; "
            "                 font-weight:700; letter-spacing:0.5px; }"
            "#fnoRefreshBtn:hover { background:%4; color:%7; }"
            "#fnoOpenBtn { background:transparent; color:%2; border:1px solid %8; padding:4px 10px; font-size:" + fincept::ui::fonts::px(-5) + "px; "
            "              font-weight:700; letter-spacing:0.5px; }"
            "#fnoOpenBtn:hover { border-color:%2; }"
            "#fnoOpenBtn::menu-indicator { image:none; width:0px; }")
            .arg(colors::BG_RAISED(), colors::AMBER(), colors::TEXT_SECONDARY(), colors::TEXT_PRIMARY(),
                 colors::POSITIVE(), colors::NEGATIVE(), colors::BG_BASE(), colors::BORDER_DIM(),
                 colors::TEXT_ON_ACCENT()));

    setup_ui();
    // A currency / number-format change re-formats the chain already held.
    fmt::on_format_changed(this, [this]() {
        if (has_chain_)
            render_chain();
    });
}

void FnoHeaderBar::setup_ui() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(12, 8, 12, 8);
    root->setSpacing(16);

    // ── Left — picker controls ────────────────────────────────────────────
    auto* picker_wrap = new QWidget(this);
    auto* picker_lay = new QHBoxLayout(picker_wrap);
    picker_lay->setContentsMargins(0, 0, 0, 0);
    picker_lay->setSpacing(8);

    broker_combo_ = new QComboBox(this);
    under_combo_ = new QComboBox(this);
    expiry_combo_ = new QComboBox(this);
    refresh_btn_ = new QPushButton(tr("REFRESH"), this);
    refresh_btn_->setObjectName("fnoRefreshBtn");
    refresh_btn_->setCursor(Qt::PointingHandCursor);

    lbl_broker_field_ = new QLabel(tr("Source:"), this);
    lbl_under_field_ = new QLabel(tr("Underlying:"), this);
    lbl_expiry_field_ = new QLabel(tr("Expiry:"), this);

    picker_lay->addWidget(lbl_broker_field_);
    picker_lay->addWidget(broker_combo_);
    picker_lay->addSpacing(6);
    picker_lay->addWidget(lbl_under_field_);
    picker_lay->addWidget(under_combo_);
    // Take the underlying to the rest of the terminal (research / chart / news).
    open_btn_ = new QPushButton(tr("OPEN ▾"), this);
    open_btn_->setObjectName("fnoOpenBtn");
    open_btn_->setCursor(Qt::PointingHandCursor);
    open_btn_->setToolTip(tr("Open the underlying in Equity Research, Equity Trading or News"));
    open_btn_->setAccessibleName(tr("Open underlying in another screen"));
    auto* open_menu = new QMenu(open_btn_);
    connect(open_menu, &QMenu::aboutToShow, this, [this, open_menu]() { populate_open_menu(open_menu); });
    open_btn_->setMenu(open_menu);
    picker_lay->addWidget(open_btn_);
    picker_lay->addSpacing(6);
    picker_lay->addWidget(lbl_expiry_field_);
    picker_lay->addWidget(expiry_combo_);
    picker_lay->addSpacing(6);
    picker_lay->addWidget(refresh_btn_);

    root->addWidget(picker_wrap, 0, Qt::AlignVCenter);
    root->addStretch(1);

    // ── Right — live ribbon ────────────────────────────────────────────────
    auto* ribbon = new QWidget(this);
    auto* ribbon_lay = new QHBoxLayout(ribbon);
    ribbon_lay->setContentsMargins(0, 0, 0, 0);
    ribbon_lay->setSpacing(20);
    ribbon_lay->addWidget(make_kv(ribbon, lbl_spot_, key_spot_, tr("Spot")));
    ribbon_lay->addWidget(make_kv(ribbon, lbl_change_, key_change_, tr("Day Change")));
    ribbon_lay->addWidget(make_kv(ribbon, lbl_atm_, key_atm_, tr("ATM")));
    ribbon_lay->addWidget(make_kv(ribbon, lbl_pcr_, key_pcr_, tr("PCR")));
    ribbon_lay->addWidget(make_kv(ribbon, lbl_max_pain_, key_max_pain_, tr("Max Pain")));
    ribbon_lay->addWidget(make_kv(ribbon, lbl_ce_oi_, key_ce_oi_, tr("CE OI")));
    ribbon_lay->addWidget(make_kv(ribbon, lbl_pe_oi_, key_pe_oi_, tr("PE OI")));
    ribbon_lay->addWidget(make_kv(ribbon, lbl_iv_pctile_, key_iv_pctile_, tr("IV Pctile")));
    root->addWidget(ribbon, 0, Qt::AlignVCenter);

    lbl_status_ = new QLabel("", this);
    lbl_status_->setObjectName("fnoHdrStatus");
    root->addWidget(lbl_status_, 0, Qt::AlignVCenter);

    // ── Accessibility + keyboard traversal ────────────────────────────────
    broker_combo_->setAccessibleName(tr("Chain source"));
    under_combo_->setAccessibleName(tr("Underlying"));
    expiry_combo_->setAccessibleName(tr("Expiry"));
    refresh_btn_->setAccessibleName(tr("Refresh option chain"));
    lbl_status_->setAccessibleName(tr("Chain freshness"));
    setTabOrder(broker_combo_, under_combo_);
    setTabOrder(under_combo_, expiry_combo_);
    setTabOrder(expiry_combo_, refresh_btn_);

    // ── Wire signals ──────────────────────────────────────────────────────
    // The picker shows readable source names; the id travels as item data.
    connect(broker_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) { emit broker_changed(broker_id()); });
    connect(under_combo_, &QComboBox::currentTextChanged, this, &FnoHeaderBar::underlying_changed);
    connect(expiry_combo_, &QComboBox::currentTextChanged, this, &FnoHeaderBar::expiry_changed);
    connect(refresh_btn_, &QPushButton::clicked, this, &FnoHeaderBar::refresh_requested);
}

QString FnoHeaderBar::broker_id() const {
    const QVariant id = broker_combo_->currentData();
    return id.isValid() ? id.toString() : broker_combo_->currentText();
}

QString FnoHeaderBar::source_label(const QString& broker_id) {
    if (broker_id == fno_free_source())
        return tr("US options · delayed (free)");
    if (broker_id == QLatin1String("databento"))
        return tr("Databento · US (key)");
    return broker_id;
}

bool FnoHeaderBar::select_broker(const QString& broker_id) {
    const int i = broker_combo_->findData(broker_id);
    if (i < 0)
        return false;
    if (broker_combo_->currentIndex() != i)
        broker_combo_->setCurrentIndex(i); // emits broker_changed
    else
        emit broker_changed(broker_id);
    return true;
}

void FnoHeaderBar::set_delayed_source(const QString& source_label) {
    if (delayed_source_ == source_label)
        return;
    delayed_source_ = source_label;
    update_freshness();
}
QString FnoHeaderBar::underlying() const {
    return under_combo_->currentText();
}
QString FnoHeaderBar::expiry() const {
    return expiry_combo_->currentText();
}

void FnoHeaderBar::set_brokers(const QStringList& broker_ids, const QString& selected) {
    QSignalBlocker block(broker_combo_);
    broker_combo_->clear();
    for (const QString& id : broker_ids) {
        broker_combo_->addItem(source_label(id), id);
        if (id == fno_free_source())
            broker_combo_->setItemData(broker_combo_->count() - 1,
                                       tr("Cboe delayed quotes (Yahoo Finance fallback) — no broker or key "
                                          "needed. Read-only: orders need a broker chain."),
                                       Qt::ToolTipRole);
    }
    if (!selected.isEmpty()) {
        const int i = broker_combo_->findData(selected);
        if (i >= 0)
            broker_combo_->setCurrentIndex(i);
    }
}

void FnoHeaderBar::set_underlyings(const QStringList& names, const QString& selected) {
    QSignalBlocker block(under_combo_);
    under_combo_->clear();
    under_combo_->addItems(names);
    if (!selected.isEmpty()) {
        const int i = under_combo_->findText(selected);
        if (i >= 0)
            under_combo_->setCurrentIndex(i);
    }
}

bool FnoHeaderBar::select_underlying(const QString& underlying) {
    if (!under_combo_ || underlying.isEmpty())
        return false;
    const int idx = under_combo_->findText(underlying);
    if (idx < 0)
        return false;
    if (under_combo_->currentIndex() != idx)
        under_combo_->setCurrentIndex(idx);
    return true;
}

void FnoHeaderBar::set_underlying_picker_visible(bool visible) {
    if (lbl_under_field_ && lbl_under_field_->isVisibleTo(this) != visible)
        lbl_under_field_->setVisible(visible);
    if (under_combo_ && under_combo_->isVisibleTo(this) != visible)
        under_combo_->setVisible(visible);
}

void FnoHeaderBar::set_expiries(const QStringList& exps, const QString& selected) {
    QSignalBlocker block(expiry_combo_);
    expiry_combo_->clear();
    expiry_combo_->addItems(exps);
    if (!selected.isEmpty()) {
        const int i = expiry_combo_->findText(selected);
        if (i >= 0)
            expiry_combo_->setCurrentIndex(i);
    }
}

void FnoHeaderBar::update_from_chain(const OptionChain& chain) {
    last_chain_ = chain;
    has_chain_ = true;
    render_chain();
}

void FnoHeaderBar::render_chain() {
    const OptionChain& chain = last_chain_;
    // The underlying's level / price in the chain's currency (rupees on NSE,
    // dollars on Databento's US options); no unit — an index level is not money.
    const QString ccy = fno_chain_ccy(chain.broker_id);
    const fmt::InstrumentFmt px = fno_price_fmt(ccy);
    lbl_spot_->setText(chain.spot > 0 ? fmt::price(chain.spot, px) : "--");

    // Day change — underlying's absolute + % move, plumbed through OptionChain
    // from the spot quote (REST refresh; WS keeps option legs current). Coloured
    // green/red via the #fnoHdrValuePos / #fnoHdrValueNeg style selectors; the
    // sign is always printed as well.
    if (chain.spot_change != 0.0 || chain.spot_change_pct != 0.0) {
        const bool up = chain.spot_change >= 0.0;
        lbl_change_->setText(QStringLiteral("%1 (%2)").arg(fmt::price_change(chain.spot_change, chain.spot, px),
                                                           fmt::pct(chain.spot_change_pct, 2)));
        lbl_change_->setObjectName(up ? "fnoHdrValuePos" : "fnoHdrValueNeg");
    } else {
        lbl_change_->setText("--");
        lbl_change_->setObjectName("fnoHdrValue");
    }
    // objectName drives the QSS colour selector — re-polish so it repaints.
    lbl_change_->style()->unpolish(lbl_change_);
    lbl_change_->style()->polish(lbl_change_);

    lbl_atm_->setText(chain.atm_strike > 0 ? fincept::services::options::format_strike(chain.atm_strike) : "--");
    lbl_pcr_->setText(chain.pcr > 0 ? fmt::number(chain.pcr, 3) : "--");
    lbl_max_pain_->setText(chain.max_pain > 0 ? fincept::services::options::format_strike(chain.max_pain) : "--");
    // Contract counts, abbreviated per the user's scheme (lakh / crore on an NSE chain by default).
    lbl_ce_oi_->setText(fno_fmt_oi(qint64(chain.total_ce_oi), ccy));
    lbl_pe_oi_->setText(fno_fmt_oi(qint64(chain.total_pe_oi), ccy));

    // ── IV Pctile pill ────────────────────────────────────────────────────
    // Compute today's ATM IV from the chain's ATM row, then percentile-rank
    // it against the trailing 90-day history in `iv_history_daily`. Needs
    // ≥30 days of accumulated data — empty/dim until then.
    //
    // P1/P11: get_window() is a synchronous SQLite read and this function runs
    // on every chain publish (seconds apart, on the UI thread). The percentile
    // only changes when the day rolls or the underlying changes, so the result
    // is memoised on (underlying, date) and the query runs once per day.
    //
    // A DELAYED chain (the free Cboe feed) has no IV history: the daily ATM IV is
    // recorded only from chains published by a live source, so the pill reads "—"
    // and says why — never "…0/30", which would suggest a history that never builds.
    if (lbl_iv_pctile_ && !delayed_source_.isEmpty()) {
        const QString why = tr("IV percentile ranks today's at-the-money IV against this terminal's own daily IV "
                               "history, which is recorded from a live broker chain. The free delayed feed has no IV "
                               "history, so there is no percentile for %1.")
                                .arg(chain.underlying.isEmpty() ? tr("this chain") : chain.underlying);
        lbl_iv_pctile_->setText(QStringLiteral("—"));
        lbl_iv_pctile_->setToolTip(why);
        if (key_iv_pctile_)
            key_iv_pctile_->setToolTip(why);
    } else if (lbl_iv_pctile_) {
        if (key_iv_pctile_)
            key_iv_pctile_->setToolTip(QString());
        double atm_iv = 0;
        for (const auto& row : chain.rows) {
            if (!row.is_atm)
                continue;
            if (row.ce_iv > 0 && row.pe_iv > 0)
                atm_iv = 0.5 * (row.ce_iv + row.pe_iv);
            else if (row.ce_iv > 0)
                atm_iv = row.ce_iv;
            else
                atm_iv = row.pe_iv;
            break;
        }
        if (atm_iv > 0 && !chain.underlying.isEmpty()) {
            const QDate today = QDate::currentDate();
            const QString key = chain.underlying + QLatin1Char('|') + today.toString(Qt::ISODate);
            if (key != iv_pctile_cache_key_) {
                iv_pctile_cache_key_ = key;
                iv_pctile_hist_days_ = -1;
                iv_pctile_value_ = 0.0;
                iv_pctile_atm_iv_ = atm_iv;
                const QString since = today.addDays(-90).toString(Qt::ISODate);
                auto r = fincept::IvHistoryRepository::instance().get_window(chain.underlying, since);
                if (r.is_ok()) {
                    const auto& hist = r.value();
                    iv_pctile_hist_days_ = int(hist.size());
                    if (hist.size() >= 30) {
                        int below = 0;
                        for (const auto& row : hist)
                            if (row.atm_iv < atm_iv)
                                ++below;
                        iv_pctile_value_ = 100.0 * double(below) / double(hist.size());
                    }
                }
            }
            QString text;
            QString tip;
            if (iv_pctile_hist_days_ < 0) {
                text = QStringLiteral("—");
            } else if (iv_pctile_hist_days_ >= 30) {
                text = fmt::pct(iv_pctile_value_, 0, false);
                tip = tr("Current ATM IV %1 ranks at %2th percentile of %3 days of history.")
                          .arg(fmt::pct(iv_pctile_atm_iv_ * 100.0, 1, false), fmt::number(iv_pctile_value_, 0))
                          .arg(iv_pctile_hist_days_);
            } else {
                // Not enough history yet — show accumulation progress instead
                // of a blank dash so it's clear it's building, not broken.
                text = tr("…%1/30").arg(iv_pctile_hist_days_);
                tip = tr("Building IV history — %1 of 30 days needed for a percentile.").arg(iv_pctile_hist_days_);
            }
            lbl_iv_pctile_->setText(text);
            lbl_iv_pctile_->setToolTip(tip);
        } else {
            lbl_iv_pctile_->setText("—");
            lbl_iv_pctile_->setToolTip(QString());
        }
    }

    last_chain_ts_ms_ = chain.timestamp_ms;
    update_freshness();
}

void FnoHeaderBar::update_freshness() {
    if (!lbl_status_)
        return;
    if (last_chain_ts_ms_ <= 0) {
        lbl_status_->setText(QString());
        lbl_status_->setToolTip(QString());
        return;
    }
    if (!delayed_source_.isEmpty()) {
        // A delayed feed is never "live": say so and when the quotes are from, without
        // the live-feed staleness alarm (a 15-minute-old Cboe quote is the expected state).
        const QDateTime at = QDateTime::fromMSecsSinceEpoch(last_chain_ts_ms_);
        const QString when = at.date() == QDate::currentDate() ? at.toString(QStringLiteral("HH:mm"))
                                                                : QLocale().toString(at, QLocale::ShortFormat);
        const QString text = tr("DELAYED · as of %1").arg(when);
        const QString tip = tr("Delayed quotes from %1 (about 15 minutes behind the market), as of %2. "
                               "Free source — no broker or key needed. REFRESH fetches a newer copy.")
                                .arg(delayed_source_, QLocale().toString(at, QLocale::LongFormat));
        if (lbl_status_->text() != text)
            lbl_status_->setText(text);
        if (lbl_status_->toolTip() != tip)
            lbl_status_->setToolTip(tip);
        const QString sheet = QString("color:%1;font-size:%2px;font-weight:700;background:transparent;")
                                  .arg(colors::WARNING(), fincept::ui::fonts::px(-5));
        if (lbl_status_->styleSheet() != sheet)
            lbl_status_->setStyleSheet(sheet);
        return;
    }
    const qint64 age_ms = QDateTime::currentMSecsSinceEpoch() - last_chain_ts_ms_;
    const qint64 age_s = age_ms / 1000;
    const QString stamp = QDateTime::fromMSecsSinceEpoch(last_chain_ts_ms_).toString("hh:mm:ss");

    // Thresholds: an option quote is meaningfully stale within ~15 s during
    // market hours and outright unusable past a minute. The label never claims
    // freshness it can't back with a timestamp.
    QString colour = colors::TEXT_SECONDARY();
    QString text = tr("Updated %1").arg(stamp);
    QString tip = tr("Last chain snapshot at %1 (%2 s ago).").arg(stamp).arg(age_s);
    if (age_s >= 60) {
        colour = colors::NEGATIVE();
        text = tr("STALE · %1 (%2m old)").arg(stamp).arg(age_s / 60);
        tip = tr("No chain update for %1 seconds. The market may be closed, or the feed may be down — "
                 "press REFRESH to re-request.")
                  .arg(age_s);
    } else if (age_s >= 15) {
        colour = colors::AMBER();
        text = tr("Updated %1 (%2s)").arg(stamp).arg(age_s);
    }
    // P7: this runs once a second while the tab is visible — only touch the
    // widget when something actually changed, so we don't reparse the sheet or
    // schedule a repaint on every tick.
    if (lbl_status_->text() != text)
        lbl_status_->setText(text);
    if (lbl_status_->toolTip() != tip)
        lbl_status_->setToolTip(tip);
    const QString sheet = QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;background:transparent;").arg(colour);
    if (lbl_status_->styleSheet() != sheet)
        lbl_status_->setStyleSheet(sheet);
}

void FnoHeaderBar::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!freshness_timer_) {
        freshness_timer_ = new QTimer(this);
        freshness_timer_->setInterval(1000);
        connect(freshness_timer_, &QTimer::timeout, this, &FnoHeaderBar::update_freshness);
    }
    update_freshness();
    freshness_timer_->start();
}

void FnoHeaderBar::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (freshness_timer_)
        freshness_timer_->stop();
}

void FnoHeaderBar::clear_ribbon() {
    for (QLabel* l :
         {lbl_spot_, lbl_change_, lbl_atm_, lbl_pcr_, lbl_max_pain_, lbl_ce_oi_, lbl_pe_oi_, lbl_iv_pctile_})
        if (l)
            l->setText("--");
    last_chain_ts_ms_ = 0;
    has_chain_ = false;
    last_chain_ = OptionChain{};
    if (lbl_status_) {
        lbl_status_->setText("");
        lbl_status_->setToolTip(QString());
        lbl_status_->setStyleSheet(QString());
    }
}

void FnoHeaderBar::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void FnoHeaderBar::populate_open_menu(QMenu* menu) {
    menu->clear();
    const QString under = underlying();
    const FnoUnderlyingTargets t = fno_underlying_targets(broker_id(), under);
    if (under.isEmpty()) {
        menu->addAction(tr("Select an underlying first"))->setEnabled(false);
        return;
    }
    if (!t.research.isEmpty()) {
        const QString sym = t.research;
        connect(menu->addAction(tr("Open %1 in Equity Research").arg(sym)), &QAction::triggered, this,
                [sym]() { fno_open_symbol(QStringLiteral("equity_research"), sym); });
    }
    if (!t.trading.isEmpty()) {
        const QString sym = t.trading;
        const QString exch = t.exchange;
        connect(menu->addAction(tr("Open %1 chart in Equity Trading").arg(sym)), &QAction::triggered, this,
                [sym, exch]() { fno_open_symbol(QStringLiteral("equity_trading"), sym, exch); });
    }
    if (!t.news.isEmpty()) {
        const QString query = t.news;
        connect(menu->addAction(tr("Open %1 in News").arg(query)), &QAction::triggered, this,
                [query]() { fno_open_symbol(QStringLiteral("news"), query); });
    }
}

void FnoHeaderBar::retranslateUi() {
    if (refresh_btn_)
        refresh_btn_->setText(tr("REFRESH"));
    if (open_btn_) {
        open_btn_->setText(tr("OPEN ▾"));
        open_btn_->setToolTip(tr("Open the underlying in Equity Research, Equity Trading or News"));
    }
    if (lbl_broker_field_)
        lbl_broker_field_->setText(tr("Source:"));
    if (lbl_under_field_)
        lbl_under_field_->setText(tr("Underlying:"));
    if (lbl_expiry_field_)
        lbl_expiry_field_->setText(tr("Expiry:"));

    // Ribbon keys mirror make_kv's .toUpper() rendering.
    if (key_spot_)
        key_spot_->setText(tr("Spot").toUpper());
    if (key_change_)
        key_change_->setText(tr("Day Change").toUpper());
    if (key_atm_)
        key_atm_->setText(tr("ATM").toUpper());
    if (key_pcr_)
        key_pcr_->setText(tr("PCR").toUpper());
    if (key_max_pain_)
        key_max_pain_->setText(tr("Max Pain").toUpper());
    if (key_ce_oi_)
        key_ce_oi_->setText(tr("CE OI").toUpper());
    if (key_pe_oi_)
        key_pe_oi_->setText(tr("PE OI").toUpper());
    if (key_iv_pctile_)
        key_iv_pctile_->setText(tr("IV Pctile").toUpper());
}

} // namespace fincept::screens::fno
