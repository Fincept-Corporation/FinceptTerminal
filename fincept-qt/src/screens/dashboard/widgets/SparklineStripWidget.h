#pragma once
// SparklineStripWidget — configurable list of symbols rendered as tiny
// sparkline rows. Subscribes to `market:sparkline:<sym>` (QVector<double>).
//
// Config (persisted): { "symbols": ["AAPL", "MSFT", ...] }

#include "screens/dashboard/widgets/BaseWidget.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

class QLabel;

namespace fincept::screens::widgets {

class SparklineCanvas;

class SparklineStripWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit SparklineStripWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    QDialog* make_config_dialog(QWidget* parent) override;

  private:
    struct Row {
        QLabel* symbol = nullptr;
        SparklineCanvas* spark = nullptr;
        QLabel* last = nullptr;
    };

    void build_rows();
    void apply_styles();
    void on_points(const QString& symbol, const QVector<double>& points);
    void hub_resubscribe();
    void hub_unsubscribe_all();

    QStringList symbols_;
    QHash<QString, Row> rows_;
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
