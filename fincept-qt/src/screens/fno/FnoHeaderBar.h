#pragma once
// FnoHeaderBar — top-of-screen control + live ribbon for F&O.
//
// Left section: broker | underlying | expiry pickers + refresh button.
// Right section: live ribbon — Spot, Day Change, ATM, PCR, Max Pain,
// Total CE/PE OI. The ribbon is updated whenever a chain publish arrives
// (parent calls update_from_chain).

#include "services/options/OptionChainTypes.h"

#include <QComboBox>
#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QWidget>

class QTimer;

namespace fincept::screens::fno {

class FnoHeaderBar : public QWidget {
    Q_OBJECT
  public:
    explicit FnoHeaderBar(QWidget* parent = nullptr);

    // ── Picker access ───────────────────────────────────────────────────────
    QString broker_id() const;
    QString underlying() const;
    QString expiry() const;

    // ── Population API (called by ChainSubTab) ─────────────────────────────
    /// Source ids (broker ids, "databento", fno_free_source()); each is shown by
    /// its readable name (source_label) and broker_id() returns the id.
    void set_brokers(const QStringList& broker_ids, const QString& selected = {});
    /// Select a listed source by id (emits broker_changed). False when not listed.
    bool select_broker(const QString& broker_id);
    /// Readable name of a source id in the picker ("US options · delayed (free)").
    static QString source_label(const QString& broker_id);
    /// The data source of a DELAYED chain ("Cboe delayed quotes"): the freshness
    /// status then reads "DELAYED · as of 14:02" instead of a live-feed age.
    void set_delayed_source(const QString& source_label);
    void set_underlyings(const QStringList& names, const QString& selected = {});
    void set_expiries(const QStringList& exps, const QString& selected = {});

    /// Selection-only setter — flips the underlying combo's currentIndex
    /// to `underlying` if it's already in the dropdown. Returns false (and
    /// is a no-op) when the value isn't present, e.g. the broker hasn't
    /// loaded that instrument yet. Used by the Yellow-group sync path to
    /// follow ticker changes without rebuilding the full picker.
    bool select_underlying(const QString& underlying);

    /// Show or hide the "Underlying:" picker. The free delayed US chain hides it: its
    /// underlying is picked in the Chain tab's symbol strip (any US ticker), while the
    /// hidden combo still carries the selection (underlying()).
    void set_underlying_picker_visible(bool visible);

    /// Bulk update the ribbon labels from a chain snapshot.
    void update_from_chain(const fincept::services::options::OptionChain& chain);

    /// Empty-state ribbon (no broker / no chain yet).
    void clear_ribbon();

  signals:
    void broker_changed(const QString& broker_id);
    void underlying_changed(const QString& underlying);
    void expiry_changed(const QString& expiry);
    void refresh_requested();

  protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private:
    void setup_ui();
    void retranslateUi();
    /// Fill the ribbon from last_chain_ (a new chain, or a currency / number-format
    /// change — nothing is fetched or queried again).
    void render_chain();

    /// Re-render the "Updated hh:mm:ss" status with an age-based freshness
    /// state (live / stale / no feed). Driven by `freshness_timer_`, which is a
    /// pure display timer — it never requests data (D3).
    void update_freshness();

    QComboBox* broker_combo_ = nullptr;
    QComboBox* under_combo_ = nullptr;
    QComboBox* expiry_combo_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    /// "OPEN ▾" next to the underlying: hands it to Equity Research / Equity Trading / News.
    QPushButton* open_btn_ = nullptr;
    void populate_open_menu(class QMenu* menu);

    // Picker field labels (cached for retranslateUi)
    QLabel* lbl_broker_field_ = nullptr;
    QLabel* lbl_under_field_ = nullptr;
    QLabel* lbl_expiry_field_ = nullptr;

    // Ribbon value labels (text is data, not translated)
    QLabel* lbl_spot_ = nullptr;
    QLabel* lbl_change_ = nullptr;
    QLabel* lbl_atm_ = nullptr;
    QLabel* lbl_pcr_ = nullptr;
    QLabel* lbl_max_pain_ = nullptr;
    QLabel* lbl_ce_oi_ = nullptr;
    QLabel* lbl_pe_oi_ = nullptr;
    QLabel* lbl_iv_pctile_ = nullptr;
    QLabel* lbl_status_ = nullptr;

    // Ribbon key labels (cached for retranslateUi)
    QLabel* key_spot_ = nullptr;
    QLabel* key_change_ = nullptr;
    QLabel* key_atm_ = nullptr;
    QLabel* key_pcr_ = nullptr;
    QLabel* key_max_pain_ = nullptr;
    QLabel* key_ce_oi_ = nullptr;
    QLabel* key_pe_oi_ = nullptr;
    QLabel* key_iv_pctile_ = nullptr;

    /// Timestamp of the last chain snapshot rendered (0 = none yet). Drives the
    /// stale-quote indicator — an option quote is worthless within seconds, so
    /// "when did this last tick" has to be on screen, not inferable.
    qint64 last_chain_ts_ms_ = 0;
    QTimer* freshness_timer_ = nullptr;

    /// One-entry memo for the IV-percentile query. `update_from_chain` runs on
    /// every publish and the percentile only moves once per trading day, so the
    /// synchronous SQLite window read is done once per (underlying, day). The
    /// figures are kept, not their text, so a format change re-renders them.
    QString iv_pctile_cache_key_;
    int iv_pctile_hist_days_ = -1; ///< days of history found; -1 = the query failed
    double iv_pctile_value_ = 0.0;  ///< percentile (0..100) when hist_days >= 30
    double iv_pctile_atm_iv_ = 0.0; ///< the ATM IV (decimal) that was ranked

    /// The last chain rendered (implicitly shared rows: a cheap copy).
    fincept::services::options::OptionChain last_chain_;
    bool has_chain_ = false;
    /// Source of a delayed chain (set_delayed_source); empty for live sources.
    QString delayed_source_;
};

} // namespace fincept::screens::fno
