#include "screens/fno/FnoHeaderBar.h"

#include "storage/repositories/IvHistoryRepository.h"
#include "ui/theme/Theme.h"

#include <QDate>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLocale>
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

QString fmt_compact(qint64 v) {
    const double a = std::abs(double(v));
    if (a >= 1e7)
        return QString::number(v / 1.0e7, 'f', 2) + "Cr";
    if (a >= 1e5)
        return QString::number(v / 1.0e5, 'f', 2) + "L";
    if (a >= 1e3)
        return QString::number(v / 1.0e3, 'f', 1) + "k";
    return QLocale(QLocale::English).toString(v);
}

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

} // namespace

FnoHeaderBar::FnoHeaderBar(QWidget* parent) : QWidget(parent) {
    setObjectName("fnoHeader");
    setStyleSheet(
        QStringLiteral(
            "#fnoHeader { background:%1; border-bottom:2px solid %2; }"
            "#fnoHdrKey { color:%3; font-size:8px; font-weight:700; letter-spacing:0.6px; background:transparent; }"
            "#fnoHdrValue { color:%4; font-size:13px; font-weight:700; background:transparent; }"
            "#fnoHdrValuePos { color:%5; font-size:13px; font-weight:700; background:transparent; }"
            "#fnoHdrValueNeg { color:%6; font-size:13px; font-weight:700; background:transparent; }"
            "#fnoHdrStatus { color:%3; font-size:9px; background:transparent; }"
            "QComboBox { background:%7; color:%4; border:1px solid %8; padding:3px 8px; font-size:11px; "
            "min-width:90px; }"
            "QComboBox:hover { border-color:%2; }"
            "QComboBox::drop-down { border:none; width:18px; }"
            "QComboBox QAbstractItemView { background:%1; color:%4; border:1px solid %8; "
            "selection-background-color:%2; }"
            "#fnoRefreshBtn { background:%2; color:%7; border:none; padding:5px 12px; font-size:9px; "
            "                 font-weight:700; letter-spacing:0.5px; }"
            "#fnoRefreshBtn:hover { background:%4; color:%7; }")
            .arg(colors::BG_RAISED(), colors::AMBER(), colors::TEXT_SECONDARY(), colors::TEXT_PRIMARY(),
                 colors::POSITIVE(), colors::NEGATIVE(), colors::BG_BASE(), colors::BORDER_DIM()));

    setup_ui();
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

    lbl_broker_field_ = new QLabel(tr("Broker:"), this);
    lbl_under_field_ = new QLabel(tr("Underlying:"), this);
    lbl_expiry_field_ = new QLabel(tr("Expiry:"), this);

    picker_lay->addWidget(lbl_broker_field_);
    picker_lay->addWidget(broker_combo_);
    picker_lay->addSpacing(6);
    picker_lay->addWidget(lbl_under_field_);
    picker_lay->addWidget(under_combo_);
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
    broker_combo_->setAccessibleName(tr("Broker"));
    under_combo_->setAccessibleName(tr("Underlying"));
    expiry_combo_->setAccessibleName(tr("Expiry"));
    refresh_btn_->setAccessibleName(tr("Refresh option chain"));
    lbl_status_->setAccessibleName(tr("Chain freshness"));
    setTabOrder(broker_combo_, under_combo_);
    setTabOrder(under_combo_, expiry_combo_);
    setTabOrder(expiry_combo_, refresh_btn_);

    // ── Wire signals ──────────────────────────────────────────────────────
    connect(broker_combo_, &QComboBox::currentTextChanged, this, &FnoHeaderBar::broker_changed);
    connect(under_combo_, &QComboBox::currentTextChanged, this, &FnoHeaderBar::underlying_changed);
    connect(expiry_combo_, &QComboBox::currentTextChanged, this, &FnoHeaderBar::expiry_changed);
    connect(refresh_btn_, &QPushButton::clicked, this, &FnoHeaderBar::refresh_requested);
}

QString FnoHeaderBar::broker_id() const {
    return broker_combo_->currentText();
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
    broker_combo_->addItems(broker_ids);
    if (!selected.isEmpty()) {
        const int i = broker_combo_->findText(selected);
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
    lbl_spot_->setText(chain.spot > 0 ? QString::number(chain.spot, 'f', 2) : "--");

    // Day change — underlying's absolute + % move, plumbed through OptionChain
    // from the spot quote (REST refresh; WS keeps option legs current). Coloured
    // green/red via the #fnoHdrValuePos / #fnoHdrValueNeg style selectors.
    if (chain.spot_change != 0.0 || chain.spot_change_pct != 0.0) {
        const bool up = chain.spot_change >= 0.0;
        const QString sign = up ? QStringLiteral("+") : QString();
        lbl_change_->setText(sign + QString::number(chain.spot_change, 'f', 2) + " (" + sign +
                             QString::number(chain.spot_change_pct, 'f', 2) + "%)");
        lbl_change_->setObjectName(up ? "fnoHdrValuePos" : "fnoHdrValueNeg");
    } else {
        lbl_change_->setText("--");
        lbl_change_->setObjectName("fnoHdrValue");
    }
    // objectName drives the QSS colour selector — re-polish so it repaints.
    lbl_change_->style()->unpolish(lbl_change_);
    lbl_change_->style()->polish(lbl_change_);

    lbl_atm_->setText(chain.atm_strike > 0 ? QString::number(chain.atm_strike, 'f', 0) : "--");
    lbl_pcr_->setText(chain.pcr > 0 ? QString::number(chain.pcr, 'f', 3) : "--");
    lbl_max_pain_->setText(chain.max_pain > 0 ? QString::number(chain.max_pain, 'f', 0) : "--");
    lbl_ce_oi_->setText(fmt_compact(qint64(chain.total_ce_oi)));
    lbl_pe_oi_->setText(fmt_compact(qint64(chain.total_pe_oi)));

    // ── IV Pctile pill ────────────────────────────────────────────────────
    // Compute today's ATM IV from the chain's ATM row, then percentile-rank
    // it against the trailing 90-day history in `iv_history_daily`. Needs
    // ≥30 days of accumulated data — empty/dim until then.
    //
    // P1/P11: get_window() is a synchronous SQLite read and this function runs
    // on every chain publish (seconds apart, on the UI thread). The percentile
    // only changes when the day rolls or the underlying changes, so the result
    // is memoised on (underlying, date) and the query runs once per day.
    if (lbl_iv_pctile_) {
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
                iv_pctile_cache_tip_.clear();
                const QString since = today.addDays(-90).toString(Qt::ISODate);
                auto r = fincept::IvHistoryRepository::instance().get_window(chain.underlying, since);
                if (r.is_ok()) {
                    const auto& hist = r.value();
                    if (hist.size() >= 30) {
                        int below = 0;
                        for (const auto& row : hist)
                            if (row.atm_iv < atm_iv)
                                ++below;
                        const double pctile = 100.0 * double(below) / double(hist.size());
                        iv_pctile_cache_text_ = QString::number(pctile, 'f', 0) + "%";
                        iv_pctile_cache_tip_ = tr("Current ATM IV %1 ranks at %2th percentile of %3 days of history.")
                                                   .arg(atm_iv * 100.0, 0, 'f', 1)
                                                   .arg(pctile, 0, 'f', 0)
                                                   .arg(hist.size());
                    } else {
                        // Not enough history yet — show accumulation progress instead
                        // of a blank dash so it's clear it's building, not broken.
                        iv_pctile_cache_text_ = tr("…%1/30").arg(hist.size());
                        iv_pctile_cache_tip_ =
                            tr("Building IV history — %1 of 30 days needed for a percentile.").arg(hist.size());
                    }
                } else {
                    iv_pctile_cache_text_ = QStringLiteral("—");
                }
            }
            lbl_iv_pctile_->setText(iv_pctile_cache_text_);
            lbl_iv_pctile_->setToolTip(iv_pctile_cache_tip_);
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
    const QString sheet = QStringLiteral("color:%1;font-size:9px;background:transparent;").arg(colour);
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

void FnoHeaderBar::retranslateUi() {
    if (refresh_btn_)
        refresh_btn_->setText(tr("REFRESH"));
    if (lbl_broker_field_)
        lbl_broker_field_->setText(tr("Broker:"));
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
