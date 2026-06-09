// src/screens/equity_research/EquityNewsTab.h
#pragma once
#include "services/equity/EquityResearchModels.h"
#include "ui/widgets/LoadingOverlay.h"

#include <QComboBox>
#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

namespace fincept::screens {

class EquityNewsTab : public QWidget {
    Q_OBJECT
  public:
    explicit EquityNewsTab(QWidget* parent = nullptr);
    void set_symbol(const QString& symbol);

    /// Selected news provider as a persistable token ("auto" | "newsapi").
    /// Used by EquityResearchScreen save_state/restore_state.
    QString provider_key() const;
    void set_provider_key(const QString& key);

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void changeEvent(QEvent* event) override;

  private slots:
    void on_news_loaded(QString symbol, QVector<services::equity::NewsArticle> articles);

  private:
    void build_ui();
    void retranslateUi();
    void populate(const QVector<services::equity::NewsArticle>& articles);
    void clear_cards();
    void start_fetch();                    ///< (re)fetch current symbol with the selected provider
    void refresh_provider_availability();  ///< enable/disable the NewsAPI item by key presence

    QString current_symbol_;
    bool use_newsapi_ = false;             ///< false = Auto (GNews → Yahoo), true = NewsAPI
    bool suppress_provider_signal_ = false; ///< guard programmatic combo changes
    QWidget* cards_container_ = nullptr;
    QVBoxLayout* cards_layout_ = nullptr;
    QLabel* status_label_ = nullptr;
    QLabel* count_label_ = nullptr;
    QLabel* company_label_ = nullptr;
    QLabel* title_lbl_ = nullptr;
    QComboBox* provider_combo_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;

    QVector<services::equity::NewsArticle> cached_articles_;
    bool news_loaded_ = false;

    ui::LoadingOverlay* loading_overlay_ = nullptr;
};

} // namespace fincept::screens
