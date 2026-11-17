#pragma once
// DerivChainModel — the option chain as a table model (virtualised: only the rows a
// view paints are formatted, so a 30,000-contract SPX chain scrolls freely).
//
//   ByExpiry: one row per strike of the chosen expiry (inside an ATM window):
//             calls on the left, the strike in the middle, puts on the right.
//   Flat:     one row per contract of every expiry, sortable on any column
//             (sorted in the model on numeric keys, not on display text).
//
// The model holds a shared, immutable FreeChain; switching expiry / window / mode
// only rebuilds an index vector.

#include "services/options/FreeOptionChainService.h"

#include <QAbstractTableModel>
#include <QVector>

namespace fincept::screens::deriv {

class DerivChainModel : public QAbstractTableModel {
    Q_OBJECT
  public:
    enum class Mode { ByExpiry, Flat };
    using ChainPtr = services::options::FreeOptionChainService::ChainPtr;

    /// ByExpiry columns.
    enum ExpiryCol {
        kCOi, kCVol, kCIv, kCDelta, kCBid, kCAsk, kCMid, kStrike, kPMid, kPBid, kPAsk, kPDelta, kPIv, kPVol, kPOi,
        kExpiryCols
    };
    /// Flat columns.
    enum FlatCol {
        kFExpiry, kFDays, kFType, kFStrike, kFBid, kFAsk, kFMid, kFLast, kFIv, kFDelta, kFGamma, kFTheta, kFVega,
        kFVol, kFOi, kFlatCols
    };
    /// Numeric value of a cell (for sorting and exports); Qt::UserRole.
    static constexpr int kNumberRole = Qt::UserRole + 1;

    explicit DerivChainModel(QObject* parent = nullptr);

    void set_chain(const ChainPtr& chain);
    void set_mode(Mode mode);
    void set_expiry(int expiry);
    /// Strikes either side of the ATM strike (ByExpiry); 0 = all.
    void set_window(int strikes_each_side);
    Mode mode() const { return mode_; }
    int expiry() const { return expiry_; }
    const ChainPtr& chain() const { return chain_; }

    /// Contract under (row, column): its quote index, or −1. In ByExpiry the call
    /// side, strike column → call; put side → put.
    int quote_at(int row, int column) const;
    /// Row of the ATM strike (ByExpiry), or −1.
    int atm_row() const { return atm_row_; }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

  private:
    void rebuild();
    void sort_flat(); ///< reorders flat_ by sort_col_ / sort_order_ (no model signals)
    double number(int row, int column) const;

    ChainPtr chain_;
    Mode mode_ = Mode::ByExpiry;
    int expiry_ = 0;
    int window_ = 20;
    // ByExpiry: per strike row the call / put quote index (−1 = not listed).
    QVector<double> strikes_;
    QVector<int> call_idx_;
    QVector<int> put_idx_;
    int atm_row_ = -1;
    // Flat: quote indexes in display order.
    QVector<int> flat_;
    int sort_col_ = -1;
    Qt::SortOrder sort_order_ = Qt::AscendingOrder;
};

} // namespace fincept::screens::deriv
