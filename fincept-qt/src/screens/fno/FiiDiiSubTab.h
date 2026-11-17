#pragma once
// FiiDiiSubTab — F&O FII / DII content widget. Replaces the placeholder
// for SubTab::FiiDii in FnoScreen.
//
// Subscribes to `fno:fii_dii:daily`. The producer (FiiDiiService) publishes
// the rolling last-30-days window as `QVector<FiiDiiDay>`. The sub-tab
// renders a date-stamped table on the left and a daily net-flow bar chart
// on the right. A REFRESH button forces a producer fetch on demand —
// useful right after market close when the user wants today's number.

#include "screens/common/IStatefulScreen.h"
#include "services/options/FiiDiiTypes.h"

#include <QEvent>
#include <QPointer>
#include <QString>
#include <QVariantMap>
#include <QWidget>

class QLabel;
class QPushButton;
class QTableWidget;

namespace fincept::ui {
class KpiStrip;
}

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
    void changeEvent(QEvent* event) override;

  private slots:
    void on_refresh_clicked();

  private:
    void setup_ui();
    void retranslateUi();
    void on_data_arrived(const QVariant& v);
    void apply_data(const QVector<fincept::services::options::FiiDiiDay>& rows);
    /// Fill the table from rows_ (new data, or a currency / number-format change —
    /// nothing is fetched again).
    void render_table();
    /// Column headers with the crore-of-rupees unit as the user's settings print it.
    void apply_headers();
    /// Re-colour the net-flow cells and re-size the rows for the active theme and font.
    void refresh_theme();
    /// Status line: the data's real as-of date, its age, and — when the last fetch
    /// failed or the source returned nothing — a clear "source unavailable" state.
    void update_status_line();
    /// Headline tiles: latest FII / DII net, 5-day and month-to-date sums, each
    /// with a sparkline of the last 20 daily nets.
    void update_kpis();
    void label_kpis();

    QLabel* lbl_status_ = nullptr;
    QString source_error_; // last fetch error for the topic (cleared by a good publish)
    QPushButton* refresh_btn_ = nullptr;
    QTableWidget* table_ = nullptr;
    FiiDiiChart* chart_ = nullptr;
    fincept::ui::KpiStrip* kpis_ = nullptr;
    QVector<fincept::services::options::FiiDiiDay> rows_; // as last rendered, ascending by date

    bool subscribed_ = false;
};

} // namespace fincept::screens::fno
