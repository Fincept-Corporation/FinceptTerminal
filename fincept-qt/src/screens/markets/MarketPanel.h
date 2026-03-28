#pragma once
#include "services/markets/MarketDataService.h"
#include "ui/widgets/LoadingOverlay.h"

#include <QLabel>
#include <QTableWidget>
#include <QWidget>

namespace fincept::screens {

/// Single market category panel — header + data table.
/// Fetches quotes and displays symbol/price/change/high/low.
class MarketPanel : public QWidget {
    Q_OBJECT
  public:
    MarketPanel(const QString& title, const QStringList& symbols, bool show_name = false, QWidget* parent = nullptr);

    void set_symbols(const QStringList& symbols);
    void refresh();

  signals:
    void refresh_finished();

  protected:
    void resizeEvent(QResizeEvent* event) override;

  private:
    void populate(const QVector<services::QuoteData>& quotes);

    QString title_;
    QStringList symbols_;
    bool show_name_;

    QLabel* title_label_ = nullptr;
    QLabel* status_label_ = nullptr;
    QTableWidget* table_ = nullptr;
    ui::LoadingOverlay* loading_overlay_ = nullptr;
};

} // namespace fincept::screens
