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
    void set_brokers(const QStringList& broker_ids, const QString& selected = {});
    void set_underlyings(const QStringList& names, const QString& selected = {});
    void set_expiries(const QStringList& exps, const QString& selected = {});

    /// Selection-only setter — flips the underlying combo's currentIndex
    /// to `underlying` if it's already in the dropdown. Returns false (and
    /// is a no-op) when the value isn't present, e.g. the broker hasn't
    /// loaded that instrument yet. Used by the Yellow-group sync path to
    /// follow ticker changes without rebuilding the full picker.
    bool select_underlying(const QString& underlying);

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

  private:
    void setup_ui();
    void retranslateUi();

    QComboBox* broker_combo_ = nullptr;
    QComboBox* under_combo_ = nullptr;
    QComboBox* expiry_combo_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;

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
};

} // namespace fincept::screens::fno
