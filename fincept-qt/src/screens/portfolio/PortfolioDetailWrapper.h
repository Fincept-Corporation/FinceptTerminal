// src/screens/portfolio/PortfolioDetailWrapper.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QWidget>

#include <functional>

namespace fincept::screens {

/// Container for full-screen detail views with a 36px header bar (back button + title).
/// Detail view widgets are lazily constructed on first navigation (P2 principle).
class PortfolioDetailWrapper : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioDetailWrapper(QWidget* parent = nullptr);

    void show_view(portfolio::DetailView view, const portfolio::PortfolioSummary& summary, const QString& currency);
    void update_data(const portfolio::PortfolioSummary& summary, const QString& currency);
    void update_snapshots(const QVector<portfolio::PortfolioSnapshot>& snapshots);
    void update_metrics(const portfolio::ComputedMetrics& metrics);

  signals:
    void back_requested();

  private:
    void build_ui();
    QWidget* get_or_create_view(portfolio::DetailView view);
    QString view_title(portfolio::DetailView view) const;

    QLabel* title_label_ = nullptr;
    QLabel* portfolio_label_ = nullptr;
    QPushButton* back_btn_ = nullptr;
    QStackedWidget* view_stack_ = nullptr;

    // Lazily created views
    QHash<int, QWidget*> views_;
    portfolio::DetailView current_view_ = portfolio::DetailView::AnalyticsSectors;

    // Data passed to views
    portfolio::PortfolioSummary current_summary_;
    portfolio::ComputedMetrics current_metrics_;
    QString current_currency_;
    QVector<portfolio::PortfolioSnapshot> current_snapshots_;
};

} // namespace fincept::screens
