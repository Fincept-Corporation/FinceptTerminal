#pragma once
// SurfaceDataInspector — bottom strip of Surface Analytics.
// Replaces the old API-key + status SurfaceDatabentoPanel. Three columns:
//
//   ┌──────────────────────────┬─────────────────┬──────────────┐
//   │ RAW DATA TABLE (tabs:    │ LINEAGE         │ STATUS /     │
//   │  definition, cbbo-1s, …) │  dataset / …    │ ERRORS       │
//   └──────────────────────────┴─────────────────┴──────────────┘

#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QHBoxLayout;
class QLabel;
class QPushButton;
class QStandardItemModel;
class QTabBar;
class QTableView;

namespace fincept::surface {

class SurfaceDataInspector : public QWidget {
    Q_OBJECT
  public:
    explicit SurfaceDataInspector(QWidget* parent = nullptr);

    // Push a tabular schema view (definition / ohlcv / statistics / ...).
    // Each name is shown as a tab. Headers seed the columns, rows seed cells.
    void show_table(const QString& tab_name, const QStringList& headers,
                    const QVector<QStringList>& rows);

    // Set the lineage block (dataset / schema / symbology / symbols / dates / count / cost).
    void set_lineage(const QString& dataset, const QString& schema,
                     const QString& symbology, const QString& symbols,
                     const QString& date_range, qint64 row_count, double cost_usd);

    // Status / errors area
    void set_status(const QString& message, bool ok);
    void set_error(const QString& error);
    void set_raw_output(const QString& raw_stdout); // full Python stdout for the modal

    void clear();

  private slots:
    void on_tab_changed(int index);
    void on_view_raw_clicked();
    void on_export_csv_clicked();

  private:
    void setup_ui();

    struct TableSnapshot {
        QString name;
        QStringList headers;
        QVector<QStringList> rows;
    };

    QTabBar* tab_bar_ = nullptr;
    QTableView* table_view_ = nullptr;
    QStandardItemModel* table_model_ = nullptr;
    QPushButton* export_btn_ = nullptr;

    QLabel* lin_dataset_ = nullptr;
    QLabel* lin_schema_ = nullptr;
    QLabel* lin_symbology_ = nullptr;
    QLabel* lin_symbols_ = nullptr;
    QLabel* lin_dates_ = nullptr;
    QLabel* lin_count_ = nullptr;
    QLabel* lin_cost_ = nullptr;

    QLabel* status_label_ = nullptr;
    QLabel* status_dot_ = nullptr;
    QLabel* error_label_ = nullptr;
    QPushButton* view_raw_btn_ = nullptr;
    QString raw_output_;

    QVector<TableSnapshot> snapshots_;
};

} // namespace fincept::surface
