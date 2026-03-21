#pragma once
#include <QWidget>
#include <QLabel>
#include <QTableWidget>
#include <QTimer>
#include "services/markets/MarketDataService.h"

namespace fincept::screens {

/// Single market category panel — header + data table.
/// Fetches quotes and displays symbol/price/change/high/low.
class MarketPanel : public QWidget {
    Q_OBJECT
public:
    MarketPanel(const QString& title, const QStringList& symbols,
                bool show_name = false, QWidget* parent = nullptr);

    void set_symbols(const QStringList& symbols);
    void refresh();

private:
    QString title_;
    QStringList symbols_;
    bool show_name_;

    QLabel* title_label_  = nullptr;
    QLabel* status_label_ = nullptr;
    QTableWidget* table_  = nullptr;

    void populate(const QVector<services::QuoteData>& quotes);
};

} // namespace fincept::screens
