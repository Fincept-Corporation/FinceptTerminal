#pragma once
// PortfolioTileBase — shared plumbing of the portfolio tiles (performance, risk):
// a portfolio picker bar persisted as {"portfolio_id": "<id>"}, the first
// portfolio as the fallback, and a reload when portfolios are created / deleted.
//
// Depends on PortfolioRepository::list_portfolios() and the PortfolioService
// signals portfolios_loaded / portfolio_created / portfolio_deleted only.

#include "screens/dashboard/widgets/BaseWidget.h"
#include "screens/portfolio/PortfolioTypes.h"

#include <QJsonObject>

class QComboBox;

namespace fincept::screens::widgets {

class PortfolioTileBase : public BaseWidget {
    Q_OBJECT
  public:
    PortfolioTileBase(const QString& title, QWidget* parent, const QString& accent);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    /// The selected portfolio changed (or was first resolved). portfolio() is
    /// valid unless has_portfolio() is false (no portfolios at all).
    virtual void on_portfolio_selected() = 0;

    bool has_portfolio() const { return !portfolio_.id.isEmpty(); }
    const portfolio::Portfolio& portfolio() const { return portfolio_; }
    QString portfolio_id() const { return portfolio_.id; }
    /// The portfolio's base currency, upper case.
    QString base_currency() const;
    void retranslateUi() override;
    /// "No portfolio yet" over the content (BaseWidget::show_empty_state) while
    /// no portfolio exists at all, with the one action that fixes it (OPEN
    /// PORTFOLIO); hidden otherwise. Subclasses showing their own empty state
    /// for a portfolio call it to put the right one back.
    void update_empty_state();

  private:
    void reload_portfolios();
    void select(const QString& id, bool notify);

    QComboBox* picker_ = nullptr;
    QVector<portfolio::Portfolio> portfolios_;
    portfolio::Portfolio portfolio_;
    QString wanted_id_;
    bool filling_ = false;
};

} // namespace fincept::screens::widgets
