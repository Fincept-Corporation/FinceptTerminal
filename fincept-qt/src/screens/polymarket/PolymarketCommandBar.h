#pragma once

#include "services/polymarket/PolymarketTypes.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

namespace fincept::screens::polymarket {

/// Top command bar: view pills, category filters, search, sort, refresh, WS status.
class PolymarketCommandBar : public QWidget {
    Q_OBJECT
  public:
    explicit PolymarketCommandBar(QWidget* parent = nullptr);

    void set_categories(const QVector<services::polymarket::Tag>& tags);
    void set_active_view(const QString& view);
    void set_active_category(const QString& cat);
    void set_loading(bool loading);
    void set_ws_status(bool connected);
    void set_market_count(int count);

  signals:
    void view_changed(const QString& view);
    void category_changed(const QString& category);
    void search_submitted(const QString& query);
    void sort_changed(const QString& sort_by);
    void refresh_clicked();

  private:
    void build_ui();

    QList<QPushButton*> view_btns_;
    QWidget* category_container_ = nullptr;
    QLineEdit* search_input_ = nullptr;
    QComboBox* sort_combo_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QLabel* ws_indicator_ = nullptr;
    QLabel* count_label_ = nullptr;
    QString active_view_ = "MARKETS";
    QString active_category_ = "ALL";
};

} // namespace fincept::screens::polymarket
