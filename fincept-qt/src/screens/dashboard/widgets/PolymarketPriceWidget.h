#pragma once
// PolymarketPriceWidget — live prediction-market price strip. Subscribes to
// `polymarket:price:<asset_id>` (push-only). Payload is a bare double (0..1
// probability). Asset IDs must be user-supplied; provide a human label per id.
//
// Config (persisted):
//   { "entries": [ {"asset_id": "...", "label": "..."}, ... ] }

#include "screens/dashboard/widgets/BaseWidget.h"

#include <QHash>
#include <QString>
#include <QVector>

class QLabel;

namespace fincept::screens::widgets {

class PolymarketPriceWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit PolymarketPriceWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    QDialog* make_config_dialog(QWidget* parent) override;

  private:
    struct Entry {
        QString asset_id;
        QString label;
    };
    struct Row {
        QLabel* label = nullptr;
        QLabel* pct = nullptr;
    };

    void apply_styles();
    void build_rows();
    void on_price(const QString& asset_id, double price);
    void hub_resubscribe();
    void hub_unsubscribe_all();

    QVector<Entry> entries_;
    QHash<QString, Row> rows_; // asset_id → widgets
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
