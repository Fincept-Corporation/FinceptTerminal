#pragma once
#include "screens/markets/MarketPanelConfig.h"

#include <QDialog>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QString>
#include <QStringList>

namespace fincept::screens {

/// Modal dialog for creating or editing a market panel. Symbols are added through the
/// shared symbol box (MarketsSymbolEntry, multi entry): what is stored is always the
/// resolved instrument's canonical market-data symbol.
class MarketPanelEditor : public QDialog {
    Q_OBJECT
  public:
    explicit MarketPanelEditor(const MarketPanelConfig& config, QWidget* parent = nullptr);

    MarketPanelConfig result_config() const;

    /// Symbols the other panels of the layout already track — for the screen-wide
    /// limit of unique symbols (markets_logic::kMaxSymbolsOnScreen).
    void set_other_symbols(const QStringList& symbols);

  protected:
    void changeEvent(QEvent* event) override;

  private slots:
    void on_add_symbol(const QString& symbol);
    void on_remove_selected();

  private:
    void build_ui();
    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();
    void refresh_ticker_list();
    /// Shows an inline validation message above the OK/Cancel row.
    void show_validation_error(const QString& message);

    MarketPanelConfig config_;
    QStringList other_symbols_;

    QLabel* error_lbl_ = nullptr;
    QLabel* title_lbl_ = nullptr;
    QLabel* tickers_lbl_ = nullptr;
    QLabel* search_lbl_ = nullptr;
    QPushButton* remove_btn_ = nullptr;
    QLineEdit* title_edit_ = nullptr;
    QWidget* entry_ = nullptr; ///< the shared symbol box (MarketsSymbolEntry)
    QListWidget* ticker_list_ = nullptr;
};

} // namespace fincept::screens
