// src/screens/equity_research/EquityNewsTab.h
#pragma once
#include "services/equity/EquityResearchModels.h"
#include "ui/widgets/LoadingOverlay.h"

#include <QEvent>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

class EquityNewsTab : public QWidget {
    Q_OBJECT
  public:
    explicit EquityNewsTab(QWidget* parent = nullptr);
    void set_symbol(const QString& symbol);

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

  private slots:
    void on_news_loaded(QString symbol, QVector<services::equity::NewsArticle> articles);

  private:
    void build_ui();
    void populate(const QVector<services::equity::NewsArticle>& articles);
    void clear_cards();

    QString current_symbol_;
    QWidget* cards_container_ = nullptr;
    QVBoxLayout* cards_layout_ = nullptr;
    QLabel* status_label_ = nullptr;
    QLabel* count_label_ = nullptr;
    QLabel* company_label_ = nullptr;

    ui::LoadingOverlay* loading_overlay_ = nullptr;
};

} // namespace fincept::screens
