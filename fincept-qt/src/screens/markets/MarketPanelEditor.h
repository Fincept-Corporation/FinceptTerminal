#pragma once
#include "screens/markets/MarketPanelConfig.h"
#include "services/markets/MarketSearchService.h"

#include <QDialog>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QListWidget>
#include <QPushButton>
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
    void changeEvent(QEvent* event) override;

  private slots:
    void on_search_text_changed(const QString& text);
    void on_add_symbol(const QString& symbol);
    void on_remove_selected();

  private:
    void build_ui();
    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();
    void fire_search(const QString& query);
    void on_search_results(const QList<services::MarketSearchService::Item>& results);
    void refresh_ticker_list();
    void reposition_dropdown();
    void hide_dropdown();

    MarketPanelConfig config_;

    QLabel*      title_lbl_        = nullptr;
    QLabel*      tickers_lbl_      = nullptr;
    QLabel*      search_lbl_       = nullptr;
    QPushButton* remove_btn_       = nullptr;
    QLineEdit*   title_edit_       = nullptr;
    QLineEdit*   search_edit_      = nullptr;
    QListWidget* ticker_list_      = nullptr;
    QListWidget* dropdown_         = nullptr;
    QTimer*      search_debounce_  = nullptr;
    QString      pending_query_;
    bool         search_connected_ = false;
};

} // namespace fincept::screens
