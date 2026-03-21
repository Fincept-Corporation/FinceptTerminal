#include "SurfaceDatabentoPanel.h"
#include "core/logging/Logger.h"
#include <QHBoxLayout>
#include <QFrame>
#include <QDateTime>

namespace fincept::surface {

SurfaceDatabentoPanel::SurfaceDatabentoPanel(QWidget* parent) : QWidget(parent) {
    setup_ui();
    update_key_status();
}

void SurfaceDatabentoPanel::setup_ui() {
    setStyleSheet("background:#0d1117; color:#c9d1d9;");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    // ── Section title ─────────────────────────────────────────────────────
    auto* title = new QLabel("DATABENTO DATA SOURCE", this);
    title->setStyleSheet(
        "color:#3399ff; font-size:10px; font-weight:bold;"
        "border-bottom:1px solid #21262d; padding-bottom:4px;");
    layout->addWidget(title);

    // ── API Key row ───────────────────────────────────────────────────────
    auto* key_row = new QWidget(this);
    auto* key_hl = new QHBoxLayout(key_row);
    key_hl->setContentsMargins(0, 0, 0, 0);
    key_hl->setSpacing(4);

    api_key_input_ = new QLineEdit(key_row);
    api_key_input_->setPlaceholderText("Databento API key (db-...)");
    api_key_input_->setEchoMode(QLineEdit::Password);
    api_key_input_->setStyleSheet(
        "QLineEdit { background:#161b22; color:#c9d1d9; border:1px solid #30363d;"
        " padding:4px 8px; font-size:10px; border-radius:2px; }"
        "QLineEdit:focus { border-color:#3399ff; }");
    key_hl->addWidget(api_key_input_, 1);

    save_key_btn_ = new QPushButton("SAVE", key_row);
    save_key_btn_->setStyleSheet(
        "QPushButton { background:#21262d; color:#8b949e; border:1px solid #30363d;"
        " padding:4px 8px; font-size:10px; border-radius:2px; }"
        "QPushButton:hover { background:#30363d; color:#c9d1d9; }");
    connect(save_key_btn_, &QPushButton::clicked, this, &SurfaceDatabentoPanel::on_save_key_clicked);
    key_hl->addWidget(save_key_btn_);

    test_btn_ = new QPushButton("TEST", key_row);
    test_btn_->setStyleSheet(
        "QPushButton { background:#21262d; color:#8b949e; border:1px solid #30363d;"
        " padding:4px 8px; font-size:10px; border-radius:2px; }"
        "QPushButton:hover { background:#30363d; color:#c9d1d9; }");
    connect(test_btn_, &QPushButton::clicked,
            &DatabentoService::instance(), &DatabentoService::test_connection);
    key_hl->addWidget(test_btn_);

    layout->addWidget(key_row);

    // ── Key status ────────────────────────────────────────────────────────
    key_status_lbl_ = new QLabel(this);
    key_status_lbl_->setStyleSheet("font-size:9px; color:#8b949e;");
    layout->addWidget(key_status_lbl_);

    // ── Divider ───────────────────────────────────────────────────────────
    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("border:none; border-top:1px solid #21262d;");
    layout->addWidget(line);

    // ── Fetch button ──────────────────────────────────────────────────────
    fetch_btn_ = new QPushButton("FETCH LIVE DATA", this);
    fetch_btn_->setStyleSheet(
        "QPushButton { background:rgba(31,77,153,255); color:#88bbff;"
        " border:1px solid rgba(64,140,255,100); padding:6px 12px;"
        " font-size:10px; font-weight:bold; border-radius:2px; }"
        "QPushButton:hover { background:rgba(46,102,191,255); }"
        "QPushButton:disabled { background:#161b22; color:#484f58; border-color:#21262d; }");
    connect(fetch_btn_, &QPushButton::clicked, this, &SurfaceDatabentoPanel::on_fetch_clicked);
    layout->addWidget(fetch_btn_);

    // ── Status label ──────────────────────────────────────────────────────
    status_lbl_ = new QLabel(this);
    status_lbl_->setWordWrap(true);
    status_lbl_->setStyleSheet("font-size:9px; color:#8b949e;");
    layout->addWidget(status_lbl_);

    // ── Last fetch timestamp ──────────────────────────────────────────────
    last_fetch_lbl_ = new QLabel(this);
    last_fetch_lbl_->setStyleSheet("font-size:9px; color:#484f58;");
    layout->addWidget(last_fetch_lbl_);

    layout->addStretch();

    // ── Info note ─────────────────────────────────────────────────────────
    auto* note = new QLabel(
        "Databento is a paid institutional data service.\n"
        "Fetches options chains, OHLCV, and futures curves.", this);
    note->setWordWrap(true);
    note->setStyleSheet("font-size:9px; color:#484f58; font-style:italic;");
    layout->addWidget(note);
}

void SurfaceDatabentoPanel::update_key_status() {
    auto& svc = DatabentoService::instance();
    if (svc.has_api_key()) {
        key_status_lbl_->setText("API key stored");
        key_status_lbl_->setStyleSheet("font-size:9px; color:#3fb950;");
        fetch_btn_->setEnabled(true);
        test_btn_->setEnabled(true);
    } else {
        key_status_lbl_->setText("No API key — enter key above");
        key_status_lbl_->setStyleSheet("font-size:9px; color:#f85149;");
        fetch_btn_->setEnabled(false);
        test_btn_->setEnabled(false);
    }

    QDateTime last = svc.last_fetch_time();
    if (last.isValid())
        last_fetch_lbl_->setText("Last fetch: " + last.toString("hh:mm:ss"));
    else
        last_fetch_lbl_->setText("No data fetched yet");
}

void SurfaceDatabentoPanel::set_active_chart(ChartType type, const QString& symbol, float spot) {
    active_chart_  = type;
    active_symbol_ = symbol;
    active_spot_   = spot;

    // Update fetch button label based on what data we'll request
    if (needs_vol_surface())
        fetch_btn_->setText("FETCH OPTIONS CHAIN");
    else if (needs_futures())
        fetch_btn_->setText("FETCH FUTURES CURVE");
    else if (needs_ohlcv())
        fetch_btn_->setText("FETCH OHLCV DATA");
    else
        fetch_btn_->setText("FETCH LIVE DATA");
}

bool SurfaceDatabentoPanel::needs_vol_surface() const {
    return active_chart_ == ChartType::Volatility   ||
           active_chart_ == ChartType::DeltaSurface  ||
           active_chart_ == ChartType::GammaSurface  ||
           active_chart_ == ChartType::VegaSurface   ||
           active_chart_ == ChartType::ThetaSurface  ||
           active_chart_ == ChartType::SkewSurface   ||
           active_chart_ == ChartType::LocalVolSurface||
           active_chart_ == ChartType::LiquidityHeatmap ||
           active_chart_ == ChartType::ImpliedDividend;
}

bool SurfaceDatabentoPanel::needs_ohlcv() const {
    return active_chart_ == ChartType::Correlation   ||
           active_chart_ == ChartType::PCA           ||
           active_chart_ == ChartType::Drawdown       ||
           active_chart_ == ChartType::BetaSurface    ||
           active_chart_ == ChartType::VaR           ||
           active_chart_ == ChartType::FactorExposure;
}

bool SurfaceDatabentoPanel::needs_futures() const {
    return active_chart_ == ChartType::CommodityForward       ||
           active_chart_ == ChartType::ContangoBackwardation  ||
           active_chart_ == ChartType::CrackSpread            ||
           active_chart_ == ChartType::CommodityVol;
}

void SurfaceDatabentoPanel::set_status(const QString& msg, bool is_error) {
    status_lbl_->setText(msg);
    status_lbl_->setStyleSheet(QString("font-size:9px; color:%1;")
        .arg(is_error ? "#f85149" : "#8b949e"));
}

// ── Show/Hide — P3 compliance (no timers here, but connect/disconnect signals) ─
void SurfaceDatabentoPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    auto& svc = DatabentoService::instance();
    connect(&svc, &DatabentoService::connection_tested,
            this, &SurfaceDatabentoPanel::on_connection_tested, Qt::UniqueConnection);
    connect(&svc, &DatabentoService::fetch_started,
            this, &SurfaceDatabentoPanel::on_fetch_started, Qt::UniqueConnection);
    connect(&svc, &DatabentoService::fetch_failed,
            this, &SurfaceDatabentoPanel::on_fetch_failed, Qt::UniqueConnection);
    connect(&svc, &DatabentoService::ohlcv_ready,
            this, &SurfaceDatabentoPanel::on_ohlcv_ready, Qt::UniqueConnection);
    connect(&svc, &DatabentoService::vol_surface_ready,
            this, &SurfaceDatabentoPanel::on_vol_ready, Qt::UniqueConnection);
    connect(&svc, &DatabentoService::futures_ready,
            this, &SurfaceDatabentoPanel::on_futures_ready, Qt::UniqueConnection);
    update_key_status();
}

void SurfaceDatabentoPanel::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    auto& svc = DatabentoService::instance();
    disconnect(&svc, nullptr, this, nullptr);
}

// ── Slots ──────────────────────────────────────────────────────────────────
void SurfaceDatabentoPanel::on_save_key_clicked() {
    QString key = api_key_input_->text().trimmed();
    if (key.isEmpty()) {
        set_status("Enter a valid API key", true);
        return;
    }
    DatabentoService::instance().set_api_key(key);
    api_key_input_->clear();
    update_key_status();
    set_status("API key saved");
}

void SurfaceDatabentoPanel::on_fetch_clicked() {
    set_status("Fetching...");
    fetch_btn_->setEnabled(false);

    if (needs_vol_surface()) {
        DatabentoService::instance().fetch_options_surface(active_symbol_, active_spot_);
    } else if (needs_futures()) {
        QStringList commodities = {"CL", "BRN", "NG", "GC", "SI", "HG"};
        DatabentoService::instance().fetch_futures_term_structure(commodities);
    } else {
        // Default: OHLCV for correlation assets
        QStringList syms = {"SPY","QQQ","IWM","DIA","GLD","TLT","IEF","HYG"};
        DatabentoService::instance().fetch_ohlcv(syms, 60);
    }
}

void SurfaceDatabentoPanel::on_connection_tested(bool ok, const QString& msg) {
    connected_ = ok;
    set_status(ok ? "Connected: " + msg : "Failed: " + msg, !ok);
    if (ok) {
        key_status_lbl_->setText("API key valid");
        key_status_lbl_->setStyleSheet("font-size:9px; color:#3fb950;");
    }
}

void SurfaceDatabentoPanel::on_fetch_started(const QString& desc) {
    set_status(desc);
    fetch_btn_->setEnabled(false);
}

void SurfaceDatabentoPanel::on_fetch_failed(const QString& err) {
    set_status(err, true);
    fetch_btn_->setEnabled(true);
}

void SurfaceDatabentoPanel::on_ohlcv_ready(const fincept::DatabentoOhlcvResult& r) {
    fetch_btn_->setEnabled(true);
    if (!r.success) { set_status(r.error, true); return; }
    set_status(QString("OHLCV ready: %1 symbols").arg(r.data.size()));
    last_fetch_lbl_->setText("Last fetch: " +
        DatabentoService::instance().last_fetch_time().toString("hh:mm:ss"));
    emit ohlcv_received(r);
}

void SurfaceDatabentoPanel::on_vol_ready(const fincept::DatabentoVolSurfaceResult& r) {
    fetch_btn_->setEnabled(true);
    if (!r.success) { set_status(r.error, true); return; }
    set_status(QString("Options chain ready: %1 strikes x %2 expiries")
        .arg(r.vol.strikes.size()).arg(r.vol.expirations.size()));
    last_fetch_lbl_->setText("Last fetch: " +
        DatabentoService::instance().last_fetch_time().toString("hh:mm:ss"));
    emit vol_surface_received(r);
}

void SurfaceDatabentoPanel::on_futures_ready(const fincept::DatabentoFuturesResult& r) {
    fetch_btn_->setEnabled(true);
    if (!r.success) { set_status(r.error, true); return; }
    set_status(QString("Futures ready: %1 contracts")
        .arg(r.forward.commodities.size()));
    last_fetch_lbl_->setText("Last fetch: " +
        DatabentoService::instance().last_fetch_time().toString("hh:mm:ss"));
    emit futures_received(r);
}

} // namespace fincept::surface
