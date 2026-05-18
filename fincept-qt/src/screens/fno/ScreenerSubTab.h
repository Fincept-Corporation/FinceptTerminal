#pragma once
// ScreenerSubTab — filter the live chain by IV / strike-distance / OI.
//
// Filters
// ───────
//   IV range (% absolute)      — pass if max(CE IV, PE IV) inside the band
//   Strike distance (rows)     — pass if |row_index − atm_index| ≤ N
//   CE OI minimum (lakhs)      — pass if CE OI ≥ threshold × 100 000
//   PE OI minimum (lakhs)      — pass if PE OI ≥ threshold × 100 000
//
// Body: condensed table view (Strike + CE/PE IV/OI/Δ%) over a custom
// QAbstractTableModel that swaps its internal vector on every filter or
// chain change.

#include "screens/common/IStatefulScreen.h"
#include "services/options/OptionChainTypes.h"

#include <QAbstractTableModel>
#include <QPointer>
#include <QString>
#include <QVariantMap>
#include <QVector>
#include <QWidget>

class QDoubleSpinBox;
class QLabel;
class QSpinBox;
class QTableView;

namespace fincept::screens::fno {

class ScreenedChainModel : public QAbstractTableModel {
    Q_OBJECT
  public:
    enum Column : int {
        ColStrike = 0,
        ColCeIv,
        ColCeOi,
        ColCeChg,
        ColPeChg,
        ColPeOi,
        ColPeIv,
        ColCount,
    };

    explicit ScreenedChainModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role = Qt::DisplayRole) const override;

    void set_rows(const QVector<fincept::services::options::OptionChainRow>& rows);

  private:
    QVector<fincept::services::options::OptionChainRow> rows_;
};

class ScreenerSubTab : public QWidget {
    Q_OBJECT
  public:
    explicit ScreenerSubTab(QWidget* parent = nullptr);
    ~ScreenerSubTab() override;

    QVariantMap save_state() const;
    void restore_state(const QVariantMap& state);

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private slots:
    void on_filter_changed();

  private:
    void setup_ui();
    void on_chain_published(const QVariant& v);
    void apply_filters();

    // Filter controls
    QDoubleSpinBox* iv_min_ = nullptr;
    QDoubleSpinBox* iv_max_ = nullptr;
    QSpinBox* distance_ = nullptr;
    QSpinBox* ce_oi_min_ = nullptr;
    QSpinBox* pe_oi_min_ = nullptr;

    QTableView* table_ = nullptr;
    ScreenedChainModel* model_ = nullptr;
    QLabel* count_label_ = nullptr;

    fincept::services::options::OptionChain last_chain_;
    bool subscribed_ = false;
};

} // namespace fincept::screens::fno
