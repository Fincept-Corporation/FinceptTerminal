// src/screens/equity_research/EquityPeersTab.h
#pragma once
#include "services/equity/EquityResearchModels.h"
#include "ui/widgets/LoadingOverlay.h"

#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include <QWidget>

namespace fincept::screens {

class EquityPeersTab : public QWidget {
    Q_OBJECT
  public:
    explicit EquityPeersTab(QWidget* parent = nullptr);
    void set_symbol(const QString& symbol);

  private slots:
    void on_peers_loaded(QVector<services::equity::PeerData> peers);
    void on_load_clicked();

  private:
    void build_ui();
    void populate_table(const QVector<services::equity::PeerData>& peers);
    QStringList default_peers(const QString& symbol) const;

    QString current_symbol_;
    QLineEdit* peers_edit_ = nullptr;
    QLabel* status_label_ = nullptr;
    QTableWidget* peer_table_ = nullptr;

    ui::LoadingOverlay* loading_overlay_ = nullptr;
};

} // namespace fincept::screens
