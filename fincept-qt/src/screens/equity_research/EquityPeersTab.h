// src/screens/equity_research/EquityPeersTab.h
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

namespace fincept::screens {

class EquityPeersTab : public QWidget {
    Q_OBJECT
  public:
    explicit EquityPeersTab(QWidget* parent = nullptr);
    void set_symbol(const QString& symbol);
    QString peers_text() const;
    void set_peers_text(const QString& text);

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
    void show_row_menu(const QPoint& pos);
    QString symbol_at_row(int row) const;
    QStringList default_peers(const QString& symbol) const;

    QString current_symbol_;
    /// Anchor + peers of the request in flight — peers_loaded() carries no symbol, so a
    /// response is matched against this to drop one that belongs to an older request.
    QStringList requested_symbols_;
    QLineEdit* peers_edit_ = nullptr;
    QLabel* peers_caption_ = nullptr; ///< "PEERS (comma-separated):"
    QLabel* status_label_ = nullptr;
    QTableWidget* peer_table_ = nullptr;
    QPushButton* load_btn_ = nullptr;
    QList<QLabel*> legend_text_lbls_; ///< Legend captions for retranslate
    QLabel* units_lbl_ = nullptr;
    QString pending_peers_; ///< restored peer list, applied on the first set_symbol()
    QVector<services::equity::PeerData> cached_peers_;
    bool peers_loaded_ = false;
    bool awaiting_suggested_peers_ = false; ///< industry-peer lookup in flight for current_symbol_

    ui::LoadingOverlay* loading_overlay_ = nullptr;
};

} // namespace fincept::screens
