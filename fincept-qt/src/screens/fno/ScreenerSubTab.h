#pragma once
// ScreenerSubTab — filter the live chain by IV / strike-distance / OI.
//
// Filters
// ───────
//   IV range (% absolute)      — pass if max(CE IV, PE IV) inside the band
//   Strike distance (rows)     — pass if |row_index − atm_index| ≤ N
//   CE OI minimum              — Indian (NSE / BSE) chain: lakhs, CE OI ≥ threshold × 100 000;
//                                US chain (free delayed / Databento): contracts, CE OI ≥ threshold
//   PE OI minimum              — the same for the puts
//   The spin boxes show the unit of the chain on screen (" L" / " contracts"); each
//   unit keeps its own floors (US default kUsOiFloorDefault contracts), saved apart.
//
// Body: condensed table view (Strike + CE/PE IV/OI/Δ%) over a custom
// QAbstractTableModel that swaps its internal vector on every filter or
// chain change.

#include "screens/common/IStatefulScreen.h"
#include "screens/fno/FnoChainState.h"
#include "services/options/OptionChainTypes.h"

#include <QAbstractTableModel>
#include <QEvent>
#include <QPointer>
#include <QString>
#include <QVariantMap>
#include <QVector>
#include <QWidget>

class QDoubleSpinBox;
class QLabel;
class QSortFilterProxyModel;
class QSpinBox;
class QStackedWidget;
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

    /// Raw strike of a row. Lets the view remember "which strike was selected"
    /// instead of "which row index", which is meaningless once the filter set
    /// changes length.
    enum Role : int {
        StrikeRole = Qt::UserRole + 1,
        /// Raw number of the cell (IV, OI, change %) — the header-sort key, so a
        /// column ranks by magnitude instead of by its display text ("1.25L").
        SortRole = Qt::UserRole + 2,
    };

    explicit ScreenedChainModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role = Qt::DisplayRole) const override;

    /// `chain_ccy` = the chain's premium currency (FnoFormat.h fno_chain_ccy); it
    /// picks the compact scheme the OI counts use while the user is on Automatic
    /// (lakh / crore for an NSE chain). Empty = NSE rupees.
    void set_rows(const QVector<fincept::services::options::OptionChainRow>& rows,
                  const QString& chain_ccy = QString());

  private:
    QVector<fincept::services::options::OptionChainRow> rows_;
    QString ccy_;
};

class ScreenerSubTab : public QWidget {
    Q_OBJECT
  public:
    explicit ScreenerSubTab(QWidget* parent = nullptr);
    ~ScreenerSubTab() override;

    QVariantMap save_state() const;
    void restore_state(const QVariantMap& state);

    /// The Chain tab's state (FnoScreen relays it): the skeleton / "needs an option
    /// chain" page while this tab holds no chain.
    void set_chain_status(FnoChainStatus status, const QString& reason);
    /// A chain that does not travel on DataHub (the free delayed US chain), or an
    /// empty chain when the Chain tab left it.
    void set_external_chain(const fincept::services::options::OptionChain& chain);

  signals:
    /// OPEN CHAIN on this tab's page.
    void open_chain_requested();

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void changeEvent(QEvent* event) override;

  private slots:
    void on_filter_changed();

  private:
    void setup_ui();
    void retranslateUi();
    void on_chain_published(const QVariant& v);
    void apply_filters();
    /// Table, skeleton or the no-chain page, from last_chain_ and the chain status.
    void update_body();
    /// Put the OI floor boxes in the unit of the chain on screen: contracts on a US
    /// chain, lakhs of contracts on an Indian one. The floors of the unit left are
    /// kept (oi_floor_lakh_ / oi_floor_us_) and come back with it. No-op when unchanged.
    void apply_oi_unit(bool us);
    /// Suffix, tooltip and accessible name of the OI boxes for the current unit.
    void label_oi_unit();

    /// Default US OI floor: a few hundred contracts keeps the strikes that trade.
    static constexpr int kUsOiFloorDefault = 200;
    bool oi_us_ = false;                                         ///< the boxes are in contracts
    int oi_floor_lakh_[2] = {0, 0};                              ///< CE, PE — lakhs (Indian chains)
    int oi_floor_us_[2] = {kUsOiFloorDefault, kUsOiFloorDefault}; ///< CE, PE — contracts (US chains)

    // Filter controls
    QDoubleSpinBox* iv_min_ = nullptr;
    QDoubleSpinBox* iv_max_ = nullptr;
    QSpinBox* distance_ = nullptr;
    QSpinBox* ce_oi_min_ = nullptr;
    QSpinBox* pe_oi_min_ = nullptr;

    // Filter field labels (cached for retranslateUi)
    QLabel* iv_min_label_ = nullptr;
    QLabel* iv_max_label_ = nullptr;
    QLabel* distance_label_ = nullptr;
    QLabel* ce_oi_label_ = nullptr;
    QLabel* pe_oi_label_ = nullptr;

    QTableView* table_ = nullptr;
    ScreenedChainModel* model_ = nullptr;
    QSortFilterProxyModel* proxy_ = nullptr; // header-click ranking (SortRole)
    QLabel* count_label_ = nullptr;
    QStackedWidget* body_ = nullptr; ///< table / skeleton / no-chain page
    fincept::ui::LoadingSkeleton* skeleton_ = nullptr;
    fincept::ui::EmptyState* no_chain_ = nullptr;
    FnoChainStatus chain_status_ = FnoChainStatus::Loading;
    QString chain_reason_;

    fincept::services::options::OptionChain last_chain_;
    bool subscribed_ = false;
};

} // namespace fincept::screens::fno
