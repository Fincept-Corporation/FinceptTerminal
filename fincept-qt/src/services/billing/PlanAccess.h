#pragma once
// PlanAccess.h — what the signed-in account's plan includes, as the server
// states it (GET /user/entitlements).
//
// The server owns the plan table: which categories of calls are included with
// Pro, which are discounted, what each costs on Free, the Pro-only benefits,
// the credits the plan saved this month and any credit rollover waiting for a
// renewal. Every "Included with Pro" badge, locked row and price hint in the
// terminal reads it from here, so the app never advertises a benefit the
// server does not grant: against an older server that has no such route,
// has_table() is false and those surfaces stay hidden.
//
// The table is fetched after sign-in and whenever the plan changes, kept for
// the next start (per account), and announced with changed().

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::billing {

/// One billable category and its price on both plans.
struct PlanPrice {
    QString category;   ///< stable key: "market_data", "macro", "news_events", "maritime", "ai", …
    QString label;      ///< "Market data"
    QString summary;    ///< one line: what the category covers
    QString free_price; ///< "0.35 credits per call"
    QString pro_price;  ///< "Included", "Half price", or the list price
    double pro_share = 1.0; ///< share of the list price a Pro account pays (0 = included)
    bool included_with_pro = false;
    QStringList paths; ///< request path prefixes the category prices

    bool cheaper_on_pro() const { return pro_share < 1.0; }
};

/// One thing Pro adds; `active` when the account has it now.
struct PlanBenefit {
    QString key;
    QString title;
    QString detail;
    bool active = false;
};

struct PlanSavings {
    int window_days = 30;
    double credits_saved = 0.0;       ///< Pro: credits the plan waived this calendar month
    double spent_on_included = 0.0;   ///< Free: credits paid in the window for categories Pro prices lower
    double would_save_with_pro = 0.0; ///< Free: the part of that Pro would not have charged
};

struct PlanRollover {
    int credits = 0;    ///< credits set aside from the last pass; 0 = nothing waiting
    QString expires_at; ///< ISO stamp the offer ends at (empty when nothing is waiting)
    int cap = 0;
    int grace_days = 0;
};

class PlanAccess : public QObject {
    Q_OBJECT
  public:
    static PlanAccess& instance();

    /// The server sent its plan table for the signed-in account.
    bool has_table() const { return has_table_; }

    /// Paid entitlements right now (the session's effective plan).
    bool is_paid() const;

    const QVector<PlanPrice>& prices() const { return prices_; }
    const QVector<PlanBenefit>& benefits() const { return benefits_; }
    const PlanSavings& savings() const { return savings_; }
    const PlanRollover& rollover() const { return rollover_; }

    /// Price row of a category; nullptr when unknown or no table was sent.
    const PlanPrice* price(const QString& category) const;

    /// What a call with list price `list_credits` in `category` costs this
    /// account right now: the list price on Free (and whenever no table was
    /// sent), the plan's share of it on Pro (0 for an included category).
    /// Client-side "enough credits?" checks must use this, never the list price.
    double effective_cost(const QString& category, double list_credits) const;

    /// The plan of this account includes `category` at no credit cost.
    bool included(const QString& category) const;

    /// One sentence for an "out of credits" message about `category`, e.g.
    /// "Market data is included with Pro ($10/month) — no credits used." Empty
    /// when the account is on Pro, Pro pays the same, or no table was sent.
    QString upgrade_note(const QString& category) const;

    /// `text` followed by upgrade_note(category) when there is one.
    QString with_upgrade_note(const QString& text, const QString& category) const;

    /// Categories the plan includes at no credit cost, as labels ("Market data", …).
    QStringList included_labels() const;

    /// Ask the server again. Without `force` a table younger than the reuse
    /// window for the same account and plan is kept.
    void refresh(bool force = false);

  signals:
    /// The table, the savings figure or the paid state changed.
    void changed();

  private:
    PlanAccess();
    Q_DISABLE_COPY(PlanAccess)

    void on_auth_changed();
    void apply(const QJsonObject& data);
    void clear();
    void load_cached(const QString& account);
    QString account_key() const;
    QString plan_signature() const;

    bool has_table_ = false;
    QVector<PlanPrice> prices_;
    QVector<PlanBenefit> benefits_;
    PlanSavings savings_;
    PlanRollover rollover_;

    QString loaded_for_;    ///< account the table belongs to
    QString signature_;     ///< plan signature the last request was made for
    qint64 fetched_ms_ = 0; ///< last answered request (success or not)
    bool in_flight_ = false;
    bool refetch_wanted_ = false;
};

} // namespace fincept::billing
