#pragma once
// PlanHint — the shared surfaces that show what the Pro plan of this terminal
// is worth, where the user is about to spend credits.
//
//  * PlanHintLabel — one line beside a billable action. On Free it states the
//    price and what Pro changes ("0.35 credits per call · Included with Pro",
//    the second part opens Plans & Pricing); on Pro it confirms the benefit
//    ("PRO  Market data included — no credits used").
//  * PlanBenefitsView — the full picture for Profile → Billing and Plans &
//    Pricing: what the plan saved (or would save), a credit rollover waiting
//    for a renewal, what Pro adds, and the price of every category per plan.
//
// Both read services/billing/PlanAccess, i.e. the server's own plan table, and
// stay hidden until the server has sent one — the terminal never states a
// benefit the server does not grant. Fincept Terminal Enterprise (the private
// edition) is a separate product with its own surfaces in EnterprisePromo.

#include <QLabel>
#include <QString>
#include <QWidget>

class QEvent;
class QVBoxLayout;

namespace fincept::ui {

namespace plan {

/// Tooltip for plan chrome (toolbar plan badge, menus): the plan in one
/// paragraph — what is included, what it saved or would save. Falls back to a
/// plain "Plans & pricing" line when the server sent no plan table.
QString badge_tooltip();

/// Tooltip for a credit balance: what credits are spent on under this plan.
QString credits_tooltip();

/// One sentence to append to an "out of credits" message for `category`, e.g.
/// "Market data is included with Pro ($10/month)." Empty when the account is
/// already on Pro, the category costs the same on Pro, or no table was sent.
QString out_of_credits_note(const QString& category);

} // namespace plan

class PlanHintLabel : public QLabel {
    Q_OBJECT
  public:
    /// `category` is a PlanAccess price key: "market_data", "macro",
    /// "research_library", "news_events", "ai_tools", "maritime", "ai".
    explicit PlanHintLabel(const QString& category, QWidget* parent = nullptr);

    void set_category(const QString& category);
    /// Compact drops the list price on Free ("Included with Pro" only) — for
    /// toolbars that already print the cost next to the hint.
    void set_compact(bool compact);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void render();

    QString category_;
    bool compact_ = false;
};

class PlanBenefitsView : public QWidget {
    Q_OBJECT
  public:
    explicit PlanBenefitsView(QWidget* parent = nullptr);

    /// Leave out the UPGRADE TO PRO button (for hosts that sit beside one).
    void set_show_upgrade_button(bool show);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void rebuild();

    QVBoxLayout* layout_ = nullptr;
    bool show_upgrade_button_ = true;
};

} // namespace fincept::ui
