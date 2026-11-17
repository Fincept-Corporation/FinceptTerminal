// src/screens/equity_research/EquityFinancialsRatios.h
//
// Financials → RATIOS & QUALITY: the ratio families across periods (model/view, a trend
// sparkline per ratio, the formula and any "n/a — <missing input>" in the tooltip) beside the
// earnings-quality block — cash-flow quality readings, Piotroski F (nine tests, pass / fail with
// values), Altman Z (variant and zones) and Beneish M.
// Financials → PEERS: this company's margins, growth, returns and leverage against the peer
// median as bullets — from the free Yahoo peer group the Peers / Valuation tabs load (shared
// service + cache; nothing billed).
#pragma once

#include "screens/equity_research/EquityFinancialsCommon.h"

#include <QWidget>

class QGridLayout;
class QLabel;
class QPushButton;
class QTableView;

namespace fincept::screens {

namespace fin_charts {
class FinBulletChart;
}

class FinRatioTableModel;

class FinRatiosPage : public QWidget {
    Q_OBJECT
  public:
    explicit FinRatiosPage(QWidget* parent = nullptr);
    void set_data(const FinBundlePtr& bundle, const FinViewState& vs);
    void set_selected(int index);

  signals:
    void period_selected(int index);

  protected:
    void changeEvent(QEvent* e) override;

  private:
    /// One score card: title, big value, zone chip, variant line, detail rows.
    struct ScoreCard {
        FinPanel panel;
        QLabel* big = nullptr;
        QLabel* unit = nullptr;
        QLabel* zone = nullptr;
        QLabel* variant = nullptr;
        QWidget* bar = nullptr; ///< zone bar (Altman / Beneish)
        QGridLayout* rows = nullptr;
        QVector<QLabel*> cells; ///< reused labels of `rows`
    };

    void build_ui();
    void retranslate();
    void fill_quality();
    void fill_score(ScoreCard& card, const fin_an::Score& s, bool piotroski);
    void fit_columns();
    QLabel* cell(ScoreCard& card, int row, int col);

    FinBundlePtr bundle_;
    FinViewState vs_;
    FinRatioTableModel* model_ = nullptr;
    QTableView* view_ = nullptr;
    FinPanel table_panel_;
    FinPanel quality_panel_;
    QGridLayout* quality_rows_ = nullptr;
    QVector<QLabel*> quality_cells_;
    QLabel* quality_note_ = nullptr;
    ScoreCard piotroski_;
    ScoreCard altman_;
    ScoreCard beneish_;
};

class FinPeersPage : public QWidget {
    Q_OBJECT
  public:
    enum class PeerState { None, Loading, Failed };

    explicit FinPeersPage(QWidget* parent = nullptr);
    void set_data(const FinBundlePtr& bundle, const FinViewState& vs);
    /// The peer group (the subject included) as the Peers tab loads it.
    void set_peers(const QVector<services::equity::PeerData>& peers);
    void set_state(PeerState state, const QString& message = {});

  signals:
    /// LOAD PEER GROUP: find the industry peers and load their ratios (free Yahoo data, cached).
    void peers_requested();

  protected:
    void changeEvent(QEvent* e) override;

  private:
    void retranslate();
    void render();

    FinBundlePtr bundle_;
    FinViewState vs_;
    QVector<services::equity::PeerData> peers_;
    PeerState state_ = PeerState::None;
    QString message_;
    FinPanel panel_;
    fin_charts::FinBulletChart* bullets_ = nullptr;
    QLabel* note_ = nullptr;
    QPushButton* load_btn_ = nullptr;
};

} // namespace fincept::screens
