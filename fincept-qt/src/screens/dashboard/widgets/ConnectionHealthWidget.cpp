#include "screens/dashboard/widgets/ConnectionHealthWidget.h"

#include "core/events/EventBus.h"
#include "datahub/DataHub.h"
#include "python/PythonSetupManager.h"
#include "services/connectivity/ConnectivityService.h"
#include "trading/AccountManager.h"
#include "trading/BrokerTopic.h"
#include "trading/DataStreamManager.h"
#include "trading/ExchangeSession.h"
#include "trading/ExchangeSessionManager.h"
#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QColor>
#include <QDateTime>
#include <QHeaderView>
#include <QLabel>
#include <QStringList>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens::widgets {

namespace {

constexpr int kConnHealthColSource = 0;
constexpr int kConnHealthColState = 1;
constexpr int kConnHealthColDetail = 2;
constexpr int kConnHealthRowHeight = 22;
// A failed Python run carries a uv log tail; the tooltip keeps its head.
constexpr int kConnHealthMaxErrorChars = 600;

QString connhealth_clock(qint64 ms) {
    return ms > 0 ? QDateTime::fromMSecsSinceEpoch(ms).toString(QStringLiteral("HH:mm:ss")) : QString();
}

/// Newest publish across an account's broker:* portfolio channels, 0 when none
/// arrived this session. Reads the hub's bookkeeping only — never subscribes,
/// so it cannot start a broker fetch.
qint64 connhealth_last_broker_data_ms(const QString& broker_id, const QString& account_id) {
    auto& hub = fincept::datahub::DataHub::instance();
    qint64 newest = 0;
    for (const char* channel : {"positions", "orders", "balance", "holdings"}) {
        const QString topic = fincept::trading::broker_topic(broker_id, account_id, QString::fromLatin1(channel));
        newest = std::max(newest, hub.last_publish_ms(topic));
    }
    return newest;
}

QTableWidgetItem* connhealth_cell(QTableWidget* table, int row, int col) {
    QTableWidgetItem* item = table->item(row, col);
    if (!item) {
        item = new QTableWidgetItem;
        table->setItem(row, col, item);
    }
    return item;
}

} // namespace

ConnectionHealthWidget::ConnectionHealthWidget(const QJsonObject& /*cfg*/, QWidget* parent)
    : BaseWidget(tr("CONNECTION HEALTH"), parent) {
    set_title(tr("CONNECTION HEALTH"));

    auto* vl = content_layout();
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // One body widget carries the tile's only stylesheet (objectName selectors).
    body_ = new QWidget(this);
    body_->setObjectName(QStringLiteral("connHealthBody"));
    auto* bl = new QVBoxLayout(body_);
    bl->setContentsMargins(0, 0, 0, 0);
    bl->setSpacing(0);

    summary_ = new QLabel(body_);
    summary_->setObjectName(QStringLiteral("connHealthSummary"));
    summary_->setProperty("tone", QStringLiteral("dim"));
    bl->addWidget(summary_);

    table_ = new QTableWidget(0, 3, body_);
    table_->setObjectName(QStringLiteral("connHealthTable"));
    table_->setAccessibleName(tr("Connection health"));
    table_->verticalHeader()->setVisible(false);
    table_->verticalHeader()->setDefaultSectionSize(kConnHealthRowHeight);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setFocusPolicy(Qt::NoFocus);
    table_->setShowGrid(false);
    table_->setWordWrap(false);
    table_->setTextElideMode(Qt::ElideRight);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(kConnHealthColSource, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(kConnHealthColState, QHeaderView::ResizeToContents);
    bl->addWidget(table_, 1);

    vl->addWidget(body_, 1);

    connect(table_, &QTableWidget::cellDoubleClicked, this,
            [this](int row, int /*col*/) { on_row_double_clicked(row); });

    // Title-bar refresh: re-probe the API now (the producer's 1 req/s cap still
    // applies) and re-read every local source.
    connect(this, &BaseWidget::refresh_requested, this, [this]() {
        fincept::datahub::DataHub::instance().request(fincept::services::ConnectivityService::health_topic(),
                                                      /*force=*/true);
        request_render();
    });

    // State-change notifications from the services that own each status. They
    // are cheap no-ops while the tile is hidden; showEvent() re-reads everything.
    const auto on_change = [this]() { request_render(); };
    auto& am = fincept::trading::AccountManager::instance();
    connect(&am, &fincept::trading::AccountManager::account_added, this, on_change);
    connect(&am, &fincept::trading::AccountManager::account_removed, this, on_change);
    connect(&am, &fincept::trading::AccountManager::account_updated, this, on_change);
    connect(&am, &fincept::trading::AccountManager::connection_state_changed, this, on_change);
    connect(&fincept::trading::DataStreamManager::instance(), &fincept::trading::DataStreamManager::stream_restarted,
            this, on_change);
    connect(&fincept::trading::ExchangeSessionManager::instance(),
            &fincept::trading::ExchangeSessionManager::ws_state_changed, this, on_change);
    auto& psm = fincept::python::PythonSetupManager::instance();
    connect(&psm, &fincept::python::PythonSetupManager::running_changed, this, on_change);
    connect(&psm, &fincept::python::PythonSetupManager::core_ready, this, on_change);
    connect(&psm, &fincept::python::PythonSetupManager::setup_complete, this, on_change);
    connect(&psm, &fincept::python::PythonSetupManager::progress_changed, this, on_change);

    retranslateUi();
    apply_styles();
}

// ── Lifecycle ───────────────────────────────────────────────────────────────

void ConnectionHealthWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        hub_subscribe();
    schedule_render([this]() { render(); }); // local sources may have moved while hidden
}

void ConnectionHealthWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e); // drops every hub subscription this tile owns
    hub_active_ = false;
}

void ConnectionHealthWidget::hub_subscribe() {
    auto& svc = fincept::services::ConnectivityService::instance();
    // Same registration the dashboard status bar uses — idempotent, and it makes
    // the tile self-sufficient when the status bar has not subscribed yet.
    svc.ensure_registered_with_hub();
    auto& hub = fincept::datahub::DataHub::instance();
    const QString topic = fincept::services::ConnectivityService::health_topic();
    if (last_health_.isEmpty()) {
        // A stale reading beats "CHECKING"; subscribe() re-probes when it is old.
        const QJsonObject cached = hub.peek_raw(topic).toJsonObject();
        if (!cached.isEmpty())
            last_health_ = cached;
    }
    hub.subscribe(this, topic, [this](const QVariant& v) { on_health(v); });
    hub_active_ = true;
}

void ConnectionHealthWidget::on_health(const QVariant& v) {
    const QJsonObject o = v.toJsonObject();
    if (o.isEmpty())
        return;
    last_health_ = o;
    request_render();
}

void ConnectionHealthWidget::request_render() {
    if (!isVisible())
        return;
    schedule_render([this]() { render(); });
}

// ── Rows ────────────────────────────────────────────────────────────────────

void ConnectionHealthWidget::append_api_row(QVector<Row>& rows) const {
    Row r;
    r.source = tr("FINCEPT API");
    if (last_health_.isEmpty()) {
        r.state = tr("CHECKING");
        r.detail = tr("waiting for the first health probe");
        r.tooltip = r.detail;
        r.tone = Tone::Dim;
        rows.append(r);
        return;
    }
    const bool ok = last_health_.value("ok").toBool();
    const QString endpoint = last_health_.value("endpoint").toString();
    const QString when = connhealth_clock(last_health_.value("checked_at_ms").toInteger());
    if (ok) {
        const int latency = last_health_.value("latency_ms").toInt(-1);
        r.state = tr("ONLINE");
        r.tone = Tone::Pos;
        r.detail = tr("%1 ms · %2 · %3").arg(latency).arg(endpoint, when);
        r.tooltip = tr("Fincept API %1 reachable — round trip %2 ms (checked %3)").arg(endpoint).arg(latency).arg(when);
    } else {
        const QString error = last_health_.value("error").toString();
        r.state = tr("OFFLINE");
        r.tone = Tone::Neg;
        r.detail = tr("%1 · %2").arg(error, when);
        r.tooltip = tr("Fincept API %1 unreachable: %2 (checked %3)").arg(endpoint, error, when);
    }
    rows.append(r);
}

void ConnectionHealthWidget::append_python_row(QVector<Row>& rows) const {
    auto& psm = fincept::python::PythonSetupManager::instance();
    const fincept::python::SetupSnapshot snap = psm.snapshot();
    const bool last_run_failed = snap.finished && !snap.succeeded && !snap.cancelled;
    const QString error_head = snap.error.left(kConnHealthMaxErrorChars);

    Row r;
    r.source = tr("PYTHON");
    r.screen_id = QStringLiteral("settings");
    if (psm.is_running()) {
        const bool full = psm.running_scope() == fincept::python::SetupScope::Full;
        r.state = tr("INSTALLING");
        r.tone = Tone::Warn;
        r.detail = tr("%1% · %2").arg(psm.overall_percent()).arg(full ? tr("full library set") : tr("essentials"));
        r.tooltip = tr("The Python environment is being installed — features that need it wait until it is ready.");
    } else if (!psm.is_core_ready()) {
        r.state = last_run_failed ? tr("FAILED") : tr("NOT SET UP");
        r.tone = Tone::Neg;
        r.detail = tr("analytics and market data need it");
        r.tooltip = last_run_failed ? tr("Installing the Python environment failed:\n%1").arg(error_head)
                                    : tr("Python is not set up yet. Install it from Settings › Python Env.");
    } else {
        r.state = tr("READY");
        r.tone = last_run_failed ? Tone::Warn : Tone::Pos;
        r.detail = psm.is_full_installed() ? tr("full library set") : tr("essentials");
        if (last_run_failed) {
            r.detail = tr("%1 · last install failed").arg(r.detail);
            r.tooltip = tr("The core environment works, but the last install run failed:\n%1").arg(error_head);
        } else {
            r.tooltip = psm.is_full_installed() ? tr("All Python environments are installed and ready.")
                                                : tr("The essential Python libraries are installed.");
        }
    }
    rows.append(r);
}

void ConnectionHealthWidget::append_broker_rows(QVector<Row>& rows) const {
    using fincept::trading::ConnectionState;
    QVector<fincept::trading::BrokerAccount> accounts = fincept::trading::AccountManager::instance().active_accounts();
    if (accounts.isEmpty()) {
        Row r;
        r.source = tr("BROKERS");
        r.state = tr("NONE");
        r.detail = tr("no active broker account");
        r.tooltip = tr("Add a broker account in Equity Trading.");
        r.screen_id = QStringLiteral("equity_trading");
        r.tone = Tone::Dim;
        rows.append(r);
        return;
    }
    std::sort(accounts.begin(), accounts.end(),
              [](const fincept::trading::BrokerAccount& a, const fincept::trading::BrokerAccount& b) {
                  return a.display_name.compare(b.display_name, Qt::CaseInsensitive) < 0;
              });

    auto& dsm = fincept::trading::DataStreamManager::instance();
    for (const auto& acct : accounts) {
        Row r;
        r.source = acct.display_name.isEmpty() ? acct.broker_id : acct.display_name;
        r.screen_id = QStringLiteral("equity_trading");
        switch (acct.state) {
            case ConnectionState::Connected:
                r.state = tr("CONNECTED");
                r.tone = Tone::Pos;
                break;
            case ConnectionState::Connecting:
                r.state = tr("CONNECTING");
                r.tone = Tone::Warn;
                break;
            case ConnectionState::TokenExpired:
                r.state = tr("TOKEN EXPIRED");
                r.tone = Tone::Neg;
                break;
            case ConnectionState::Error:
                r.state = tr("ERROR");
                r.tone = Tone::Neg;
                break;
            case ConnectionState::Disconnected:
                // Streams start on demand, so an idle account is not an outage.
                r.state = tr("DISCONNECTED");
                r.tone = Tone::Dim;
                break;
        }

        const fincept::trading::AccountDataStream* stream = dsm.stream_for(acct.account_id);
        const bool streaming = stream && stream->is_running();
        const qint64 data_ms = connhealth_last_broker_data_ms(acct.broker_id, acct.account_id);

        QStringList parts{acct.broker_id,
                          acct.trading_mode == QLatin1String("live") ? tr("live mode") : tr("paper mode"),
                          streaming ? tr("stream on") : tr("stream off")};
        if (data_ms > 0)
            parts << tr("data %1").arg(connhealth_clock(data_ms));
        r.detail = parts.join(QStringLiteral(" · "));

        QStringList tip{tr("%1 (%2) — %3").arg(r.source, acct.broker_id, r.state)};
        if (!acct.error_message.isEmpty())
            tip << acct.error_message;
        tip << (data_ms > 0
                    ? tr("Last broker data: %1")
                          .arg(QDateTime::fromMSecsSinceEpoch(data_ms).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")))
                    : tr("No broker data received this session"));
        r.tooltip = tip.join(QLatin1Char('\n'));
        rows.append(r);
    }
}

void ConnectionHealthWidget::append_exchange_rows(QVector<Row>& rows) const {
    auto& esm = fincept::trading::ExchangeSessionManager::instance();
    QStringList ids = esm.active_exchange_ids();
    if (ids.isEmpty()) {
        Row r;
        r.source = tr("EXCHANGE WS");
        r.state = tr("NOT STARTED");
        r.detail = tr("no exchange stream opened this session");
        r.tooltip = tr("Crypto exchange WebSockets start when Crypto Trading or a live crypto tile needs them.");
        r.screen_id = QStringLiteral("crypto_trading");
        r.tone = Tone::Dim;
        rows.append(r);
        return;
    }
    ids.sort();

    const QStringList& crypto_ids = fincept::trading::ExchangeSessionManager::supported_exchange_ids();
    for (const QString& id : ids) {
        // session() only creates for unknown ids; every id here is already live.
        const fincept::trading::ExchangeSession* s = esm.session(id);
        if (!s)
            continue;
        const bool crypto = crypto_ids.contains(id);
        const bool active = s->is_ws_active();

        Row r;
        r.source = id.toUpper();
        r.screen_id = crypto ? QStringLiteral("crypto_trading") : QStringLiteral("equity_trading");
        QStringList parts{crypto ? tr("crypto WS") : tr("broker WS")};
        if (s->is_ws_connected()) {
            r.state = tr("LIVE");
            r.tone = Tone::Pos;
        } else if (active) {
            r.state = tr("CONNECTING");
            r.tone = Tone::Warn;
        } else if (s->is_ws_wanted()) {
            r.state = tr("DOWN");
            r.tone = Tone::Neg;
            parts << tr("stream dropped");
        } else {
            r.state = tr("STOPPED");
            r.tone = Tone::Dim;
            parts << tr("no stream running");
        }
        if (active || s->is_ws_wanted()) {
            const int pairs = static_cast<int>(s->ws_symbols().size());
            if (pairs > 0)
                parts << tr("%1 pairs").arg(pairs);
            if (s->is_ws_hub_owned())
                parts << tr("dashboard feed");
        }
        r.detail = parts.join(QStringLiteral(" · "));
        r.tooltip = tr("%1 WebSocket — %2").arg(r.source, r.state);
        rows.append(r);
    }
}

// ── Render ──────────────────────────────────────────────────────────────────

void ConnectionHealthWidget::render() {
    QVector<Row> rows;
    append_api_row(rows);
    append_python_row(rows);
    append_broker_rows(rows);
    append_exchange_rows(rows);

    const QColor pos = ui::colors::POSITIVE();
    const QColor warn = ui::colors::WARNING();
    const QColor neg = ui::colors::NEGATIVE();
    const QColor dim = ui::colors::TEXT_TERTIARY();
    const QColor primary = ui::colors::TEXT_PRIMARY();
    const QColor secondary = ui::colors::TEXT_SECONDARY();

    const int n = static_cast<int>(rows.size());
    table_->setRowCount(n);
    QStringList down, degraded;
    for (int i = 0; i < n; ++i) {
        const Row& r = rows[i];
        const QColor& tone = r.tone == Tone::Pos ? pos : r.tone == Tone::Warn ? warn : r.tone == Tone::Neg ? neg : dim;
        QTableWidgetItem* src = connhealth_cell(table_, i, kConnHealthColSource);
        src->setText(r.source);
        src->setForeground(primary);
        src->setData(Qt::UserRole, r.screen_id);
        src->setToolTip(r.tooltip);

        QTableWidgetItem* state = connhealth_cell(table_, i, kConnHealthColState);
        state->setText(r.state);
        state->setForeground(tone);
        state->setToolTip(r.tooltip);

        QTableWidgetItem* detail = connhealth_cell(table_, i, kConnHealthColDetail);
        detail->setText(r.detail);
        detail->setForeground(secondary);
        detail->setToolTip(r.tooltip);

        if (r.tone == Tone::Neg)
            down << r.source;
        else if (r.tone == Tone::Warn)
            degraded << r.source;
    }

    if (!down.isEmpty())
        set_summary(tr("%1 DOWN · %2").arg(down.size()).arg((down + degraded).join(QStringLiteral(", "))),
                    QStringLiteral("neg"));
    else if (!degraded.isEmpty())
        set_summary(tr("%1 DEGRADED · %2").arg(degraded.size()).arg(degraded.join(QStringLiteral(", "))),
                    QStringLiteral("warn"));
    else if (last_health_.isEmpty())
        set_summary(tr("CHECKING…"), QStringLiteral("dim"));
    else
        set_summary(tr("NO ISSUES"), QStringLiteral("pos"));
}

void ConnectionHealthWidget::set_summary(const QString& text, const QString& tone) {
    summary_->setText(text);
    summary_->setToolTip(text);
    if (summary_->property("tone").toString() == tone)
        return;
    summary_->setProperty("tone", tone);
    // Re-evaluate the [tone=…] selector of the body stylesheet.
    summary_->style()->unpolish(summary_);
    summary_->style()->polish(summary_);
}

void ConnectionHealthWidget::on_row_double_clicked(int row) {
    const QTableWidgetItem* item = table_->item(row, kConnHealthColSource);
    const QString screen = item ? item->data(Qt::UserRole).toString() : QString();
    if (!screen.isEmpty())
        EventBus::instance().publish(QStringLiteral("nav.switch_screen"), QVariantMap{{"screen_id", screen}});
}

// ── Theme / language ────────────────────────────────────────────────────────

void ConnectionHealthWidget::apply_styles() {
    body_->setStyleSheet(
        QString("#connHealthBody{background:transparent;}"
                "#connHealthSummary{color:%1;background:%2;border-bottom:1px solid %3;padding:3px 8px;"
                "font-size:9px;font-weight:bold;letter-spacing:0.5px;}"
                "#connHealthSummary[tone=\"pos\"]{color:%4;}"
                "#connHealthSummary[tone=\"warn\"]{color:%5;}"
                "#connHealthSummary[tone=\"neg\"]{color:%6;}"
                "QTableWidget#connHealthTable{background:transparent;color:%7;border:none;font-size:10px;}"
                "QTableWidget#connHealthTable QHeaderView::section{background:%2;color:%1;border:none;"
                "border-bottom:1px solid %3;padding:2px 4px;font-size:9px;font-weight:bold;}"
                "QTableWidget#connHealthTable::item{padding:0px 4px;border-bottom:1px solid %3;}")
            .arg(ui::colors::TEXT_TERTIARY(), ui::colors::BG_RAISED(), ui::colors::BORDER_DIM(), ui::colors::POSITIVE(),
                 ui::colors::WARNING(), ui::colors::NEGATIVE(), ui::colors::TEXT_PRIMARY()));
}

void ConnectionHealthWidget::on_theme_changed() {
    apply_styles();
    request_render(); // row colours come from the theme tokens
}

void ConnectionHealthWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("CONNECTION HEALTH"));
    table_->setHorizontalHeaderLabels({tr("SOURCE"), tr("STATE"), tr("DETAIL")});
    table_->setAccessibleName(tr("Connection health"));
    table_->setToolTip(tr("Double-click a row to open the screen that manages it"));
    render(); // every row and the summary are translated text
}

} // namespace fincept::screens::widgets
