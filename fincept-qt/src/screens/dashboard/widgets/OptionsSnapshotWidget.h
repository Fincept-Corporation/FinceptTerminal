#pragma once
// OptionsSnapshotWidget — dashboard tile with the headline F&O gauges for one
// underlying: Put/Call Ratio, Max Pain and ATM implied volatility.
//
// Pure DataHub consumer (D1/D3/D4) — no timers, no Python, no broker calls:
//   - Broker + expiry are picked the way the F&O Chain tab (ChainSubTab) picks
//     them: the configured broker, else the first connected Indian broker
//     account, else any active one, else Databento (API key set and a US
//     underlying). Expiry = the nearest listed expiry that has not passed.
//   - Subscribes to `option:chain:<broker>:<underlying>:<expiry>`. That
//     subscription is what makes OptionChainService refresh the chain: the
//     derived topics below are push-only side effects of a chain refresh.
//   - PCR from `fno:pcr:<b>:<u>:<e>`, Max Pain from `fno:max_pain:<b>:<u>:<e>`.
//   - ATM IV from the subscribed chain's own ATM row (avg of CE/PE IV — the
//     formula OptionChainService::publish_atm_iv uses). `option:atm_iv:<b>:<u>`
//     is keyed per underlying, not per expiry, so it alternates between expiries
//     whenever the F&O screen drives a different expiry of the same underlying.
//
// Double-click opens the F&O screen.
//
// Config (persisted): { "underlying": "NIFTY", "broker": "" }   ("" = auto)

#include "screens/dashboard/widgets/BaseWidget.h"

#include <QDate>
#include <QString>
#include <QStringList>
#include <QVariant>

class QLabel;

namespace fincept::screens::widgets {

class OptionsSnapshotWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit OptionsSnapshotWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    QDialog* make_config_dialog(QWidget* parent) override;
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void retranslateUi() override;

  private:
    void apply_styles();
    void apply_texts();
    void on_refresh();

    /// Pick broker + expiry for the configured underlying, then subscribe.
    /// Ends in subscribe_series() or a set_error() that says what to do.
    void resolve_and_subscribe();
    /// Instruments for `broker` are in memory: read its expiries and subscribe.
    void resolve_expiry(const QString& broker);
    /// Pick the nearest live expiry out of `expiries` and subscribe to it.
    void subscribe_with_expiries(const QString& broker, const QStringList& expiries);
    void subscribe_series(const QString& broker, const QString& expiry);
    /// Drop every hub subscription (data + errors) and the live topic names.
    void drop_series();
    void clear_values();
    QString pick_auto_broker() const;

    void on_chain(const QString& topic, const QVariant& v);
    void on_chain_error(const QString& topic, const QString& error);
    void on_scalar(const QString& topic, const QVariant& v, bool is_pcr);
    void render();
    void update_header();

    // ── Config ──
    QString underlying_ = QStringLiteral("NIFTY");
    QString broker_cfg_; ///< "" = auto

    // ── Resolved series (empty while unresolved) ──
    QString broker_;
    QString expiry_;
    QString chain_topic_;
    QString pcr_topic_;
    QString max_pain_topic_;
    /// Chain topic the values on screen belong to — survives hide/show, so a
    /// re-show of the same series keeps its numbers visible (P11).
    QString values_topic_;
    /// Last successful resolution — reused on re-show so a dashboard tab switch
    /// doesn't walk the whole instrument cache again.
    QString cached_broker_;
    QString cached_underlying_;
    QString cached_expiry_;
    QDate cached_expiry_date_;
    /// Broker whose instrument download we are waiting for (refresh_done re-resolves).
    QString waiting_instruments_for_;

    int gen_ = 0;                  ///< bumped on every resolve / config change — drops stale async callbacks
    bool live_ = false;            ///< tile visible — subscriptions wanted
    bool subscribed_ = false;      ///< hub subscriptions for the current series are up
    bool pending_async_ = false;   ///< an instrument load / Databento expiry fetch is in flight
    bool awaiting_broker_ = false; ///< stopped at "no broker / no account" — account events retry

    // ── Values for the current series ──
    double pcr_ = 0;
    double max_pain_ = 0;
    double atm_iv_ = 0;
    double spot_ = 0;
    double atm_strike_ = 0;
    bool have_values_ = false;
    bool refresh_failed_ = false;

    // ── UI ──
    QWidget* root_ = nullptr;
    QLabel* series_label_ = nullptr;
    QLabel* broker_label_ = nullptr;
    QLabel* pcr_value_ = nullptr;
    QLabel* pcr_caption_ = nullptr;
    QLabel* pain_value_ = nullptr;
    QLabel* pain_caption_ = nullptr;
    QLabel* iv_value_ = nullptr;
    QLabel* iv_caption_ = nullptr;
    QLabel* footer_label_ = nullptr;
};

} // namespace fincept::screens::widgets
