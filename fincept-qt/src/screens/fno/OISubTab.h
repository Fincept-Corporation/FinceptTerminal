#pragma once
// OISubTab — F&O OI Analytics content widget. Replaces the placeholder in
// FnoScreen for SubTab::OI.
//
// Layout (2 × 2 splitter)
// ───────────────────────
//   ┌────────────────────────┬──────────────────────────────┐
//   │ MultiStrikeOIChart     │  MaxPainChart                │
//   ├────────────────────────┼──────────────────────────────┤
//   │ OIBuildupTable         │  [Strike ▾] IntradayOIChart  │
//   └────────────────────────┴──────────────────────────────┘
//
// Subscribes to `option:chain:*` (pattern) — pushes the latest publish into
// the three chain-derived widgets and re-seeds the IntradayOIChart's strike
// combo if the underlying changed. Visibility-driven sub/unsub per D3/D4.

#include "screens/IStatefulScreen.h"
#include "services/options/OptionChainTypes.h"

#include <QPointer>
#include <QString>
#include <QVariantMap>
#include <QWidget>

class QComboBox;

namespace fincept::screens::fno {

class IntradayOIChart;
class MaxPainChart;
class MultiStrikeOIChart;
class OIBuildupTable;

class OISubTab : public QWidget {
    Q_OBJECT
  public:
    explicit OISubTab(QWidget* parent = nullptr);
    ~OISubTab() override;

    QVariantMap save_state() const;
    void restore_state(const QVariantMap& state);

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private slots:
    void on_strike_combo_changed(int index);

  private:
    void setup_ui();
    void on_chain_published(const QString& topic, const QVariant& v);
    void rebuild_strike_combo(const fincept::services::options::OptionChain& chain);
    void apply_strike_subscription();

    MultiStrikeOIChart* oi_bars_ = nullptr;
    MaxPainChart* pain_chart_ = nullptr;
    OIBuildupTable* buildup_ = nullptr;
    IntradayOIChart* intraday_ = nullptr;
    QComboBox* strike_combo_ = nullptr;

    fincept::services::options::OptionChain last_chain_;
    QString current_underlying_;
    bool subscribed_ = false;
};

} // namespace fincept::screens::fno
