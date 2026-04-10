#pragma once
#include "screens/markets/MarketPanelConfig.h"
#include "services/markets/MarketDataService.h"
#include "ui/widgets/LoadingOverlay.h"

#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QWidget>

namespace fincept::screens {

/// Single market category panel — header + data table.
/// Fetches quotes and displays symbol/price/change/high/low.
/// Emits edit_requested / delete_requested for the parent screen to handle.
class MarketPanel : public QWidget {
    Q_OBJECT
  public:
    explicit MarketPanel(const MarketPanelConfig& config, QWidget* parent = nullptr);

    void refresh();
    const QString& panel_id() const { return config_.id; }

  signals:
    void refresh_finished();
    void edit_requested(const QString& panel_id);
    void delete_requested(const QString& panel_id);

  protected:
    void resizeEvent(QResizeEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

  private:
    void populate(const QVector<services::QuoteData>& quotes);
    void refresh_theme();

    MarketPanelConfig config_;

    QLabel*      title_label_   = nullptr;
    QLabel*      status_label_  = nullptr;
    QPushButton* edit_btn_      = nullptr;
    QPushButton* delete_btn_    = nullptr;
    QTableWidget* table_        = nullptr;
    ui::LoadingOverlay* loading_overlay_ = nullptr;
};

} // namespace fincept::screens
