#pragma once
#include "screens/markets/MarketPanelConfig.h"

#include <QDialog>
#include <QJsonArray>
#include <QLineEdit>
#include <QListWidget>
#include <QString>
#include <QTimer>

namespace fincept::screens {

/// Modal dialog for creating or editing a market panel.
/// Symbol search uses the same /market/search API as the command bar.
class MarketPanelEditor : public QDialog {
    Q_OBJECT
  public:
    explicit MarketPanelEditor(const MarketPanelConfig& config, QWidget* parent = nullptr);

    MarketPanelConfig result_config() const;

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void moveEvent(QMoveEvent* event) override;

  private slots:
    void on_search_text_changed(const QString& text);
    void on_add_symbol(const QString& symbol);
    void on_remove_selected();

  private:
    void build_ui();
    void fire_search(const QString& query);
    void on_search_results(const QJsonArray& results);
    void refresh_ticker_list();
    void reposition_dropdown();
    void hide_dropdown();

    MarketPanelConfig config_;

    QLineEdit*   title_edit_      = nullptr;
    QLineEdit*   search_edit_     = nullptr;
    QListWidget* ticker_list_     = nullptr;
    QListWidget* dropdown_        = nullptr;
    QTimer*      search_debounce_ = nullptr;
    QString      pending_query_;
};

} // namespace fincept::screens
