#pragma once
#include "services/markets/MarketDataService.h"

#include <QLabel>
#include <QTableWidget>
#include <QTimer>
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

  private:
    void populate(const QVector<services::QuoteData>& quotes);
    void show_skeleton();
    void pulse_skeleton();

    QString title_;
    QStringList symbols_;
    bool show_name_;

    QLabel* title_label_ = nullptr;
    QLabel* status_label_ = nullptr;
    QTableWidget* table_ = nullptr;
    QTimer* skeleton_timer_ = nullptr;
    int skeleton_offset_ = 0; // shimmer position 0..100
};

} // namespace fincept::screens
