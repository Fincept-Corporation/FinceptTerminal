#pragma once
// MarketQuoteStripWidget — configurable ticker strip. Subscribes to
// `market:quote:<sym>` for a user-defined symbol list via MarketDataService
// (DataHub producer). No broker account required.
//
// Config (persisted): { "symbols": ["AAPL", "MSFT", ...] }
// When no config is supplied, defaults to a tech-index starter set.

#include "screens/dashboard/widgets/BaseWidget.h"

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

class QLabel;

namespace fincept::services {
struct QuoteData;
}

namespace fincept::screens::widgets {

class MarketQuoteStripWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit MarketQuoteStripWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void retranslateUi() override;
    QDialog* make_config_dialog(QWidget* parent) override;
    /// A symbol's quote failed: mark its row ("—" + reason on hover).
    void on_topic_errors_changed() override;

  private:
    struct Row {
        QLabel* symbol = nullptr;
        QLabel* price = nullptr;
        QLabel* change = nullptr;
    };

    void build_rows();
    void apply_styles();
    void on_quote(const QString& sym, const fincept::services::QuoteData& q);
    /// Tooltip / failed marker for `sym`'s row from its topic's error state.
    void apply_row_error(const QString& sym);
    void hub_resubscribe();
    void hub_unsubscribe_all();

    QStringList symbols_;
    QWidget* rows_host_ = nullptr; // owns the rows + the one stylesheet styling them
    QHash<QString, Row> rows_;     // symbol → widgets
    // Loading progress / received tracking now lives in BaseWidget::track_topics().
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
