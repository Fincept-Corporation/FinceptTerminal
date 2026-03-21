#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "ui/tables/DataTable.h"
#include "services/markets/MarketDataService.h"
#include <QMap>
#include <functional>

namespace fincept::screens::widgets {

/// Reusable quote-table widget. Fetches symbols via MarketDataService and
/// displays them in a DataTable with customizable formatting.
class QuoteTableWidget : public BaseWidget {
    Q_OBJECT
public:
    /// label_map: maps raw symbol -> display name (e.g. "^GSPC" -> "S&P 500").
    /// price_decimals: number of decimal places for price column.
    explicit QuoteTableWidget(
        const QString& title,
        const QStringList& symbols,
        const QMap<QString, QString>& label_map = {},
        int price_decimals = 2,
        const QString& accent_color = {},
        QWidget* parent = nullptr);

    void refresh_data();

private:
    void populate(const QVector<fincept::services::QuoteData>& quotes);

    QStringList              symbols_;
    QMap<QString, QString>   label_map_;
    int                      price_decimals_;
    ui::DataTable*           table_ = nullptr;
};

} // namespace fincept::screens::widgets
