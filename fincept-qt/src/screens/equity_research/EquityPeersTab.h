// src/screens/equity_research/EquityPeersTab.h
//
// Equity Research → Peers. Same-industry companies on the subject's own market (found by
// the region-aware discovery in the service), compared on valuation, returns, margins,
// growth, leverage and liquidity. Market caps listed in different currencies compare in
// USD; every price keeps its own currency (pence, yen). Columns carry percentile bars
// (rank within the group, no absolute "good / bad" thresholds) and a scatter plots any two
// metrics against each other.
#pragma once
#include "services/equity/EquityResearchModels.h"
#include "ui/widgets/LoadingOverlay.h"

#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPushButton>
#include <QTableWidget>
#include <QVector>
#include <QWidget>

class QComboBox;

namespace fincept::ui {
class BarCellDelegate;
}

namespace fincept::screens {

class PeerScatterChart;

class EquityPeersTab : public QWidget {
    Q_OBJECT
  public:
    explicit EquityPeersTab(QWidget* parent = nullptr);
    void set_symbol(const QString& symbol);
    QString peers_text() const;
    void set_peers_text(const QString& text);
    /// Replace the peer list and load it now, as if typed and LOAD pressed.
    void apply_peers(const QString& text);

  signals:
    /// A peer row asked to be researched in this screen (`show_chart` → open its Overview chart).
    void research_requested(QString symbol, bool show_chart);

  protected:
    void changeEvent(QEvent* event) override;

  private slots:
    void on_peers_loaded(QVector<services::equity::PeerData> peers);
    void on_load_clicked();

  private:
    void build_ui();
    void retranslateUi();
    void populate_table(const QVector<services::equity::PeerData>& peers);
    void update_relative_summary(const QVector<services::equity::PeerData>& peers);
    void update_scatter();
    void show_row_menu(const QPoint& pos);
    QString symbol_at_row(int row) const;
    void find_peers();

    QString current_symbol_;
    QStringList requested_symbols_;
    QLineEdit* peers_edit_ = nullptr;
    QLabel* peers_caption_ = nullptr;
    QLabel* status_label_ = nullptr;
    QTableWidget* peer_table_ = nullptr;
    QLabel* relative_lbl_ = nullptr;
    QPushButton* load_btn_ = nullptr;
    QPushButton* find_btn_ = nullptr;
    QPushButton* export_btn_ = nullptr;
    QLabel* units_lbl_ = nullptr;
    QComboBox* scatter_x_ = nullptr;
    QComboBox* scatter_y_ = nullptr;
    PeerScatterChart* scatter_ = nullptr;
    QList<ui::BarCellDelegate*> bar_delegates_;
    QString pending_peers_;
    QVector<services::equity::PeerData> cached_peers_;
    bool peers_loaded_ = false;
    bool awaiting_suggested_peers_ = false;

    ui::LoadingOverlay* loading_overlay_ = nullptr;
};

} // namespace fincept::screens
