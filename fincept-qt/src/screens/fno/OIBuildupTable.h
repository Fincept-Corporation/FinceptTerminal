#pragma once
// OIBuildupTable — classifies each strike's intraday CE/PE action into
// Long Build-up / Short Build-up / Short Covering / Long Unwinding based
// on the joint sign of price change and OI change.
//
//   price ↑   OI ↑   →  LONG BUILD-UP    (bullish)
//   price ↓   OI ↑   →  SHORT BUILD-UP   (bearish)
//   price ↑   OI ↓   →  SHORT COVERING   (caution)
//   price ↓   OI ↓   →  LONG UNWINDING   (caution)
//
// Source: chain row `BrokerQuote.change_pct` and `oi_change_pct` — both are
// today-vs-prev-close deltas as the broker reports them.

#include "services/options/OptionChainTypes.h"

#include <QAbstractTableModel>
#include <QTableView>
#include <QVector>

namespace fincept::screens::fno {

class OIBuildupModel : public QAbstractTableModel {
    Q_OBJECT
  public:
    enum Column : int {
        ColStrike = 0,
        ColCeClass,
        ColCeChgPct,
        ColCeOiChgPct,
        ColPeClass,
        ColPeChgPct,
        ColPeOiChgPct,
        ColCount,
    };

    explicit OIBuildupModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role = Qt::DisplayRole) const override;

    void set_chain(const fincept::services::options::OptionChain& chain);

  private:
    enum class Buildup { None, LongBuildup, ShortBuildup, ShortCovering, LongUnwinding };
    static Buildup classify(double price_chg_pct, double oi_chg_pct);
    static QString class_str(Buildup b);
    static QColor class_color(Buildup b);

    fincept::services::options::OptionChain chain_;
};

class OIBuildupTable : public QTableView {
    Q_OBJECT
  public:
    explicit OIBuildupTable(QWidget* parent = nullptr);
    OIBuildupModel* buildup_model() const { return model_; }

  private:
    OIBuildupModel* model_ = nullptr;
};

} // namespace fincept::screens::fno
