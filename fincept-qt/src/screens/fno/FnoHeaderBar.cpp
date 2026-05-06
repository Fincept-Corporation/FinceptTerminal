#include "screens/fno/FnoHeaderBar.h"

#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLocale>
#include <QSignalBlocker>
#include <QVBoxLayout>

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

QWidget* make_kv(QWidget* parent, QLabel*& value_out, const QString& key) {
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
    return wrap;
}

}  // namespace

FnoHeaderBar::FnoHeaderBar(QWidget* parent) : QWidget(parent) {
    setObjectName("fnoHeader");
    setStyleSheet(QStringLiteral(
        "#fnoHeader { background:%1; border-bottom:2px solid %2; }"
        "#fnoHdrKey { color:%3; font-size:8px; font-weight:700; letter-spacing:0.6px; background:transparent; }"
        "#fnoHdrValue { color:%4; font-size:13px; font-weight:700; background:transparent; }"
        "#fnoHdrValuePos { color:%5; font-size:13px; font-weight:700; background:transparent; }"
        "#fnoHdrValueNeg { color:%6; font-size:13px; font-weight:700; background:transparent; }"
        "#fnoHdrStatus { color:%3; font-size:9px; background:transparent; }"
        "QComboBox { background:%7; color:%4; border:1px solid %8; padding:3px 8px; font-size:11px; min-width:90px; }"
        "QComboBox:hover { border-color:%2; }"
        "QComboBox::drop-down { border:none; width:18px; }"
        "QComboBox QAbstractItemView { background:%1; color:%4; border:1px solid %8; selection-background-color:%2; }"
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
    refresh_btn_ = new QPushButton("REFRESH", this);
    refresh_btn_->setObjectName("fnoRefreshBtn");
    refresh_btn_->setCursor(Qt::PointingHandCursor);

    picker_lay->addWidget(new QLabel("Broker:", this));
    picker_lay->addWidget(broker_combo_);
    picker_lay->addSpacing(6);
    picker_lay->addWidget(new QLabel("Underlying:", this));
    picker_lay->addWidget(under_combo_);
    picker_lay->addSpacing(6);
    picker_lay->addWidget(new QLabel("Expiry:", this));
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
    ribbon_lay->addWidget(make_kv(ribbon, lbl_spot_, "Spot"));
    ribbon_lay->addWidget(make_kv(ribbon, lbl_change_, "Day Change"));
    ribbon_lay->addWidget(make_kv(ribbon, lbl_atm_, "ATM"));
    ribbon_lay->addWidget(make_kv(ribbon, lbl_pcr_, "PCR"));
    ribbon_lay->addWidget(make_kv(ribbon, lbl_max_pain_, "Max Pain"));
    ribbon_lay->addWidget(make_kv(ribbon, lbl_ce_oi_, "CE OI"));
    ribbon_lay->addWidget(make_kv(ribbon, lbl_pe_oi_, "PE OI"));
    root->addWidget(ribbon, 0, Qt::AlignVCenter);

    lbl_status_ = new QLabel("", this);
    lbl_status_->setObjectName("fnoHdrStatus");
    root->addWidget(lbl_status_, 0, Qt::AlignVCenter);

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

    // Day change is derived from the underlying quote — but we don't carry
    // the underlying BrokerQuote in OptionChain (only spot). For Phase 2
    // we leave change blank; Phase 3's WS path will plumb spot quote +
    // day change through the chain payload.
    lbl_change_->setText("--");
    lbl_change_->setObjectName("fnoHdrValue");

    lbl_atm_->setText(chain.atm_strike > 0 ? QString::number(chain.atm_strike, 'f', 0) : "--");
    lbl_pcr_->setText(chain.pcr > 0 ? QString::number(chain.pcr, 'f', 3) : "--");
    lbl_max_pain_->setText(chain.max_pain > 0 ? QString::number(chain.max_pain, 'f', 0) : "--");
    lbl_ce_oi_->setText(fmt_compact(qint64(chain.total_ce_oi)));
    lbl_pe_oi_->setText(fmt_compact(qint64(chain.total_pe_oi)));

    const QDateTime ts = QDateTime::fromMSecsSinceEpoch(chain.timestamp_ms);
    lbl_status_->setText(QString("Updated %1").arg(ts.toString("hh:mm:ss")));
}

void FnoHeaderBar::clear_ribbon() {
    for (QLabel* l : {lbl_spot_, lbl_change_, lbl_atm_, lbl_pcr_, lbl_max_pain_, lbl_ce_oi_, lbl_pe_oi_})
        if (l)
            l->setText("--");
    if (lbl_status_)
        lbl_status_->setText("");
}

} // namespace fincept::screens::fno
