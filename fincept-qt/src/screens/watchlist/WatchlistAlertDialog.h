#pragma once
// WatchlistAlertDialog — list / add / edit / delete the alerts of one symbol.
// Non-modal; edits go straight to WatchlistAlertService.

#include "services/markets/MarketDataTypes.h"
#include "storage/repositories/WatchlistRepository.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QWidget;

namespace fincept::screens {

class WatchlistAlertDialog : public QDialog {
    Q_OBJECT
  public:
    /// `quote` (may be null) seeds sensible default thresholds.
    WatchlistAlertDialog(const QString& symbol, const QString& watchlist_id, const services::QuoteData* quote,
                         QWidget* parent = nullptr);

  private:
    void build_ui();
    void reload();
    void start_edit(const WatchlistAlert& a);
    void on_kind_changed();
    void save_editor();
    qint64 selected_id() const;
    void refresh_theme();

    QString symbol_;
    QString watchlist_id_;
    double last_price_ = 0;
    int price_digits_ = 2;
    WatchlistAlert editing_;

    QLabel* title_ = nullptr;
    QLabel* status_ = nullptr;
    QTableWidget* table_ = nullptr;
    QPushButton* add_btn_ = nullptr;
    QPushButton* edit_btn_ = nullptr;
    QPushButton* delete_btn_ = nullptr;
    QWidget* editor_ = nullptr;
    QComboBox* kind_ = nullptr;
    QLabel* threshold_label_ = nullptr;
    QDoubleSpinBox* threshold_ = nullptr;
    QLabel* ma_label_ = nullptr;
    QComboBox* ma_period_ = nullptr;
    QCheckBox* repeat_ = nullptr;
    QCheckBox* enabled_ = nullptr;
    QLineEdit* note_ = nullptr;
    QPushButton* save_btn_ = nullptr;
    QPushButton* cancel_btn_ = nullptr;
};

} // namespace fincept::screens
