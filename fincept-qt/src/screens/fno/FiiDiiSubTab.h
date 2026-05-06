#pragma once
// FiiDiiSubTab — F&O FII / DII content widget. Replaces the placeholder
// for SubTab::FiiDii in FnoScreen.
//
// Subscribes to `fno:fii_dii:daily`. The producer (FiiDiiService) publishes
// the rolling last-30-days window as `QVector<FiiDiiDay>`. The sub-tab
// renders a date-stamped table on the left and a daily net-flow bar chart
// on the right. A REFRESH button forces a producer fetch on demand —
// useful right after market close when the user wants today's number.

#include "screens/IStatefulScreen.h"
#include "services/options/FiiDiiTypes.h"

#include <QPointer>
#include <QString>
#include <QVariantMap>
#include <QWidget>

class QLabel;
class QPushButton;
class QTableWidget;

namespace fincept::screens::fno {

class FiiDiiChart;

class FiiDiiSubTab : public QWidget {
    Q_OBJECT
  public:
    explicit FiiDiiSubTab(QWidget* parent = nullptr);
    ~FiiDiiSubTab() override;

    QVariantMap save_state() const;
    void restore_state(const QVariantMap& state);

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private slots:
    void on_refresh_clicked();

  private:
    void setup_ui();
    void on_data_arrived(const QVariant& v);
    void apply_data(const QVector<fincept::services::options::FiiDiiDay>& rows);

    QLabel* lbl_status_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QTableWidget* table_ = nullptr;
    FiiDiiChart* chart_ = nullptr;

    bool subscribed_ = false;
};

} // namespace fincept::screens::fno
