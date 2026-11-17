#include "screens/dashboard/widgets/TodayPnLWidget.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "trading/BrokerTopic.h"
#include "trading/DataStreamManager.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QJsonObject>
#include <QLabel>

namespace fincept::screens::widgets {

namespace {
// Signed P&L in the broker account's own currency (empty when the broker is not
// known: no unit). Full below 1 lakh / 1 million, abbreviated above in the user's
// scheme (Settings > Currency & Number Format; on Automatic lakh / crore only for
// rupee-family accounts — a USD broker once printed 150,000 as "+1.50L").
QString tpl_pnl(double v, const QString& ccy) {
    namespace tpl_fmt = fincept::ui::formatting;
    const bool south_asian =
        tpl_fmt::resolve_compact(tpl_fmt::Compact::FromSettings, ccy) == tpl_fmt::Compact::LakhCrore;
    const bool big = std::abs(v) >= (south_asian ? 1.0e5 : 1.0e6);
    return tpl_fmt::signed_money(v, ccy, big ? tpl_fmt::Compact::FromSettings : tpl_fmt::Compact::None);
}
} // namespace

TodayPnLWidget::TodayPnLWidget(const QJsonObject& cfg, QWidget* parent) : BaseWidget(tr("TODAY P&L"), parent) {
    auto* vl = content_layout();
    vl->setContentsMargins(10, 8, 10, 8);
    vl->setSpacing(6);

    header_hint_ = new QLabel("—");
    vl->addWidget(header_hint_);

    // The hero is the broker's position P&L summed over every row (open and
    // closed today) — under a "TODAY P&L" title it read as the day's P&L,
    // which is the "Day" row below. Caption it.
    hero_caption_ = new QLabel(tr("NET P&L · ALL POSITIONS"));
    hero_caption_->setAlignment(Qt::AlignCenter);
    hero_caption_->setObjectName("todayPnlRowLabel");
    vl->addWidget(hero_caption_);

    total_pnl_label_ = new QLabel("—");
    total_pnl_label_->setAlignment(Qt::AlignCenter);
    total_pnl_label_->setObjectName("todayPnlHero");
    vl->addWidget(total_pnl_label_);

    auto* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(2);

    auto make_row = [grid](int row, const QString& label_text, QLabel*& label_out, QLabel*& value_out) {
        label_out = new QLabel(label_text);
        label_out->setObjectName("todayPnlRowLabel");
        value_out = new QLabel("—");
        value_out->setObjectName("todayPnlRowValue");
        value_out->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(label_out, row, 0);
        grid->addWidget(value_out, row, 1);
    };

    make_row(0, tr("Day"), day_pnl_label_, day_pnl_value_);
    make_row(1, tr("Realized"), realized_pnl_label_, realized_pnl_value_);
    make_row(2, tr("Positions"), positions_label_, positions_value_);

    vl->addLayout(grid);
    vl->addStretch(1);

    // Title-bar refresh = "retry". It used to be wired to nothing, so the 20 s
    // "No data yet - click refresh to retry" prompt pointed at a dead button.
    // The account stream polls on its own cadence, so the only thing a retry can
    // do is (re)start that stream, show the loading state again, and ask the hub.
    connect(this, &BaseWidget::refresh_requested, this, [this]() {
        if (broker_id_.isEmpty() || account_id_.isEmpty())
            return;
        ensure_stream_running();
        if (positions_value_ && positions_value_->text() == QStringLiteral("—"))
            set_loading(true);
        datahub::DataHub::instance().request(positions_topic(), /*force=*/true);
        update_session_state(); // a still-dead session says so again instead of spinning
    });

    // A dead broker session (expired token, connection error) used to end in the
    // generic "No data yet": the poll fails silently. Name it in the header and,
    // with nothing on screen, on the overlay.
    connect(&trading::AccountManager::instance(), &trading::AccountManager::connection_state_changed, this,
            [this](const QString& account_id, trading::ConnectionState) {
                if (account_id == account_id_)
                    update_session_state();
            });

    // Settings > Currency & Number Format: re-format the hub's last positions, no fetch.
    ui::formatting::on_format_changed(this, [this]() {
        if (!hub_active_)
            return;
        const QVariant v = datahub::DataHub::instance().peek_raw(positions_topic());
        if (v.canConvert<QVector<trading::BrokerPosition>>())
            populate(v.value<QVector<trading::BrokerPosition>>());
    });

    set_configurable(true);
    apply_styles();
    apply_config(cfg);
}

QString TodayPnLWidget::positions_topic() const {
    if (broker_id_.isEmpty() || account_id_.isEmpty())
        return {};
    return trading::broker_topic(broker_id_, account_id_, QStringLiteral("positions"));
}

void TodayPnLWidget::update_session_state() {
    if (header_hint_ && !account_id_.isEmpty())
        header_hint_->setText(broker_account_hint(account_id_, positions_topic()));
}

QJsonObject TodayPnLWidget::config() const {
    QJsonObject o;
    if (!account_id_.isEmpty())
        o.insert("account_id", account_id_);
    return o;
}

void TodayPnLWidget::apply_config(const QJsonObject& cfg) {
    account_id_ = cfg.value("account_id").toString();
    if (account_id_.isEmpty())
        account_id_ = resolve_account_id();

    broker_id_.clear();
    if (!account_id_.isEmpty()) {
        hide_empty_state();
        broker_id_ = trading::AccountManager::instance().get_account(account_id_).broker_id;
        update_session_state();
    } else {
        header_hint_->setText(tr("No active account — click gear to configure"));
        show_no_account_state();
    }

    if (isVisible() && !broker_id_.isEmpty() && !account_id_.isEmpty()) {
        ensure_stream_running();
        hub_resubscribe();
    }
}

void TodayPnLWidget::show_no_account_state() {
    // Class C (needs a broker login): explain and point to Equity Trading, never
    // connect anything here. An account that becomes active fills the tile.
    show_no_broker_account(tr("This tile sums a broker account's P&L — net, day and realized, with the count of "
                              "open positions."),
                           [this]() { apply_config({}); });
}

QString TodayPnLWidget::resolve_account_id() const {
    const auto active = trading::AccountManager::instance().active_accounts();
    return active.isEmpty() ? QString() : active.first().account_id;
}

void TodayPnLWidget::ensure_stream_running() {
    auto& mgr = trading::DataStreamManager::instance();
    if (!mgr.has_stream(account_id_))
        mgr.start_stream(account_id_);
}

void TodayPnLWidget::hub_resubscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    if (broker_id_.isEmpty() || account_id_.isEmpty())
        return;
    const QString topic = positions_topic();
    if (positions_value_ && positions_value_->text() == QStringLiteral("—"))
        set_loading(true); // nothing shown yet — see OpenPositionsWidget::hub_resubscribe
    // Producer errors on the positions topic replace the spinner / mark the tile STALE.
    track_topics({topic});
    hub.subscribe(this, topic, [this, topic](const QVariant& v) {
        if (!v.canConvert<QVector<trading::BrokerPosition>>())
            return;
        note_topic_value(topic);
        populate(v.value<QVector<trading::BrokerPosition>>());
    });
    hub_active_ = true;
    update_session_state();
}

void TodayPnLWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void TodayPnLWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!broker_id_.isEmpty() && !account_id_.isEmpty()) {
        ensure_stream_running();
        if (!hub_active_)
            hub_resubscribe();
    }
}

void TodayPnLWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void TodayPnLWidget::populate(const QVector<trading::BrokerPosition>& rows) {
    double total_pnl = 0.0;
    double day_pnl = 0.0;
    int open_positions = 0;
    bool any_day_pnl = false;
    bool any_pnl = false;
    for (const auto& p : rows) {
        total_pnl += p.pnl;
        day_pnl += p.day_pnl;
        any_day_pnl = any_day_pnl || p.day_pnl != 0.0;
        any_pnl = any_pnl || p.pnl != 0.0;
        if (std::abs(p.quantity) > 0.0)
            ++open_positions;
    }
    // Most broker adapters never fill BrokerPosition::day_pnl, so it arrives
    // as an exact 0 on every row. Printing "+0.00" then states a flat day that
    // nobody measured — show "n/a" when every row has P&L but none a day P&L.
    const bool day_unreported = !any_day_pnl && any_pnl;
    // Realized ≈ total P&L on zero-quantity rows (closed intraday)
    double realized = 0.0;
    for (const auto& p : rows) {
        if (std::abs(p.quantity) < 1e-9)
            realized += p.pnl;
    }

    // The account's own currency; unknown broker -> no unit (never a guessed one).
    const auto* broker = trading::BrokerRegistry::instance().get(broker_id_);
    const QString ccy = broker ? broker->profile().currency : QString();

    auto tone = [](double v) {
        return v > 0 ? ui::colors::POSITIVE() : v < 0 ? ui::colors::NEGATIVE() : ui::colors::TEXT_PRIMARY();
    };
    total_pnl_label_->setText(rows.isEmpty() ? QStringLiteral("—") : tpl_pnl(total_pnl, ccy));
    set_css_cached(total_pnl_label_,
                   QString("color:%1;font-size:" + fincept::ui::fonts::px(10) + "px;font-weight:700;background:transparent;padding:4px 0;")
                       .arg(rows.isEmpty() ? ui::colors::TEXT_TERTIARY() : tone(total_pnl)));

    if (day_unreported) {
        day_pnl_value_->setText(tr("n/a"));
        day_pnl_value_->setToolTip(tr("This broker's positions feed does not report day (mark-to-market) P&L."));
        set_css_cached(day_pnl_value_, QString("color:%1;font-size:" + fincept::ui::fonts::px(-2) + "px;font-weight:600;background:transparent;")
                                           .arg(ui::colors::TEXT_TERTIARY()));
    } else {
        day_pnl_value_->setText(tpl_pnl(day_pnl, ccy));
        day_pnl_value_->setToolTip(QString());
        set_css_cached(day_pnl_value_, QString("color:%1;font-size:" + fincept::ui::fonts::px(-2) + "px;font-weight:600;background:transparent;")
                                           .arg(tone(day_pnl)));
    }

    realized_pnl_value_->setText(tpl_pnl(realized, ccy));
    set_css_cached(realized_pnl_value_,
                   QString("color:%1;font-size:" + fincept::ui::fonts::px(-2) + "px;font-weight:600;background:transparent;").arg(tone(realized)));

    positions_value_->setText(QString::number(open_positions));

    set_loading(false);
}

QDialog* TodayPnLWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle(tr("Configure — Today P&L"));
    auto* form = new QFormLayout(dlg);

    auto* combo = new QComboBox(dlg);
    const auto accts = trading::AccountManager::instance().list_accounts();
    for (const auto& a : accts) {
        const QString label = a.display_name.isEmpty() ? (a.broker_id + " — " + a.account_id) : a.display_name;
        combo->addItem(label, a.account_id);
        if (a.account_id == account_id_)
            combo->setCurrentIndex(combo->count() - 1);
    }
    form->addRow(tr("Broker account"), combo);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    form->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, dlg, [this, dlg, combo]() {
        const QString picked = combo->currentData().toString();
        if (!picked.isEmpty() && picked != account_id_) {
            QJsonObject cfg;
            cfg.insert("account_id", picked);
            apply_config(cfg);
            emit config_changed(cfg);
        }
        dlg->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    return dlg;
}

void TodayPnLWidget::on_theme_changed() {
    apply_styles();
}

void TodayPnLWidget::apply_styles() {
    set_css_cached(header_hint_, 
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;background:transparent;padding:2px 0;").arg(ui::colors::TEXT_TERTIARY()));

    const QString row_label_css =
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;background:transparent;").arg(ui::colors::TEXT_TERTIARY());
    if (hero_caption_)
        set_css_cached(hero_caption_, row_label_css + "letter-spacing:0.5px;");
    reset_css_cache(total_pnl_label_);
    reset_css_cache(day_pnl_value_);
    reset_css_cache(realized_pnl_value_);
    if (day_pnl_label_)
        set_css_cached(day_pnl_label_, row_label_css);
    if (realized_pnl_label_)
        set_css_cached(realized_pnl_label_, row_label_css);
    if (positions_label_)
        set_css_cached(positions_label_, row_label_css);
    if (positions_value_)
        set_css_cached(positions_value_, 
            QString("color:%1;font-size:" + fincept::ui::fonts::px(-2) + "px;font-weight:600;background:transparent;").arg(ui::colors::TEXT_PRIMARY()));
}

void TodayPnLWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("TODAY P&L"));
    // Re-translate the kept captions instead of tearing down and rebuilding
    // the hub subscription (which the old implementation did, and which
    // translated nothing).
    if (day_pnl_label_)
        day_pnl_label_->setText(tr("Day"));
    if (realized_pnl_label_)
        realized_pnl_label_->setText(tr("Realized"));
    if (hero_caption_)
        hero_caption_->setText(tr("NET P&L · ALL POSITIONS"));
    if (positions_label_)
        positions_label_->setText(tr("Positions"));
    if (header_hint_ && account_id_.isEmpty()) {
        header_hint_->setText(tr("No active account — click gear to configure"));
        show_no_account_state();
    } else {
        update_session_state();
    }
}

} // namespace fincept::screens::widgets
