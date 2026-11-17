#include "services/billing/PlanAccess.h"

#include "auth/AuthManager.h"
#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"
#include "storage/repositories/SettingsRepository.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>

namespace fincept::billing {

namespace {

constexpr const char* kPlanAccessTag = "PlanAccess";
/// Last table the server sent: {"account": "<id>", "data": {…}}.
constexpr const char* kPlanAccessCacheKey = "billing.entitlements_cache";
/// A table for the same account and plan is reused this long. The savings
/// figure inside it moves slowly; a plan change refetches at once.
constexpr qint64 kPlanAccessFreshMs = 15 * 60 * 1000;

QStringList plan_access_strings(const QJsonValue& v) {
    QStringList out;
    for (const auto& item : v.toArray())
        out << item.toString();
    return out;
}

} // namespace

PlanAccess& PlanAccess::instance() {
    static PlanAccess s;
    return s;
}

PlanAccess::PlanAccess() {
    auto& auth = auth::AuthManager::instance();
    connect(&auth, &auth::AuthManager::auth_state_changed, this, &PlanAccess::on_auth_changed);
    on_auth_changed();
}

bool PlanAccess::is_paid() const {
    const auto& auth = auth::AuthManager::instance();
    return auth.is_authenticated() && auth.session().has_paid_plan();
}

const PlanPrice* PlanAccess::price(const QString& category) const {
    if (!has_table_)
        return nullptr;
    for (const auto& p : prices_) {
        if (p.category == category)
            return &p;
    }
    return nullptr;
}

double PlanAccess::effective_cost(const QString& category, double list_credits) const {
    const PlanPrice* p = price(category);
    if (!p || !is_paid())
        return list_credits;
    return list_credits * p->pro_share;
}

bool PlanAccess::included(const QString& category) const {
    const PlanPrice* p = price(category);
    return p && p->included_with_pro && is_paid();
}

QString PlanAccess::upgrade_note(const QString& category) const {
    const PlanPrice* p = price(category);
    if (!p || is_paid() || !p->cheaper_on_pro())
        return {};
    return p->included_with_pro ? tr("%1 is included with Pro ($10/month) — no credits used.").arg(p->label)
                                : tr("%1 is %2 with Pro ($10/month).").arg(p->label, p->pro_price.toLower());
}

QString PlanAccess::with_upgrade_note(const QString& text, const QString& category) const {
    const QString note = upgrade_note(category);
    return note.isEmpty() ? text : text + QLatin1Char(' ') + note;
}

QStringList PlanAccess::included_labels() const {
    QStringList out;
    if (!has_table_)
        return out;
    for (const auto& p : prices_) {
        if (p.included_with_pro)
            out << p.label;
    }
    return out;
}

QString PlanAccess::account_key() const {
    const auto& auth = auth::AuthManager::instance();
    if (!auth.is_authenticated())
        return {};
    const auto& s = auth.session();
    return s.user_info.id > 0 ? QString::number(s.user_info.id) : s.user_info.email.trimmed().toLower();
}

QString PlanAccess::plan_signature() const {
    const auto& s = auth::AuthManager::instance().session();
    return account_key() + QLatin1Char('|') + s.account_type().toLower() + QLatin1Char('|') + s.effective_plan() +
           QLatin1Char('|') + s.user_info.credits_expire_at;
}

void PlanAccess::on_auth_changed() {
    const QString account = account_key();
    if (account.isEmpty()) {
        if (!loaded_for_.isEmpty() || has_table_) {
            clear();
            emit changed();
        }
        return;
    }
    if (account != loaded_for_) {
        clear();
        loaded_for_ = account;
        load_cached(account);
        emit changed();
    }
    // Sign-in, a purchase, a lapsed pass: the plan signature moves and the
    // table is asked for again; an ordinary profile refresh does nothing.
    refresh(plan_signature() != signature_);
}

void PlanAccess::clear() {
    has_table_ = false;
    prices_.clear();
    benefits_.clear();
    savings_ = {};
    rollover_ = {};
    loaded_for_.clear();
    signature_.clear();
    fetched_ms_ = 0;
}

void PlanAccess::load_cached(const QString& account) {
    const auto r = SettingsRepository::instance().get(kPlanAccessCacheKey);
    if (r.is_err() || r.value().isEmpty())
        return;
    const QJsonObject cached = QJsonDocument::fromJson(r.value().toUtf8()).object();
    if (cached.value(QStringLiteral("account")).toString() != account)
        return;
    apply(cached.value(QStringLiteral("data")).toObject());
}

void PlanAccess::apply(const QJsonObject& data) {
    QVector<PlanPrice> prices;
    for (const auto& v : data.value(QStringLiteral("prices")).toArray()) {
        const QJsonObject o = v.toObject();
        PlanPrice p;
        p.category = o.value(QStringLiteral("category")).toString();
        if (p.category.isEmpty())
            continue;
        p.label = o.value(QStringLiteral("label")).toString();
        p.summary = o.value(QStringLiteral("summary")).toString();
        p.free_price = o.value(QStringLiteral("free")).toString();
        p.pro_price = o.value(QStringLiteral("pro")).toString();
        p.pro_share = o.value(QStringLiteral("pro_share")).toDouble(1.0);
        p.included_with_pro = o.value(QStringLiteral("included_with_pro")).toBool();
        p.paths = plan_access_strings(o.value(QStringLiteral("paths")));
        prices.append(p);
    }
    QVector<PlanBenefit> benefits;
    for (const auto& v : data.value(QStringLiteral("benefits")).toArray()) {
        const QJsonObject o = v.toObject();
        PlanBenefit b;
        b.key = o.value(QStringLiteral("key")).toString();
        b.title = o.value(QStringLiteral("title")).toString();
        b.detail = o.value(QStringLiteral("detail")).toString();
        b.active = o.value(QStringLiteral("active")).toBool();
        if (!b.title.isEmpty())
            benefits.append(b);
    }
    const QJsonObject sv = data.value(QStringLiteral("savings")).toObject();
    PlanSavings savings;
    savings.window_days = sv.value(QStringLiteral("window_days")).toInt(30);
    savings.credits_saved = sv.value(QStringLiteral("credits_saved")).toDouble();
    savings.spent_on_included = sv.value(QStringLiteral("credits_spent_on_included")).toDouble();
    savings.would_save_with_pro = sv.value(QStringLiteral("would_save_with_pro")).toDouble();

    const QJsonObject ro = data.value(QStringLiteral("rollover")).toObject();
    PlanRollover rollover;
    rollover.credits = ro.value(QStringLiteral("credits")).toInt();
    rollover.expires_at = ro.value(QStringLiteral("expires_at")).toString();
    rollover.cap = ro.value(QStringLiteral("cap")).toInt();
    rollover.grace_days = ro.value(QStringLiteral("grace_days")).toInt();

    prices_ = prices;
    benefits_ = benefits;
    savings_ = savings;
    rollover_ = rollover;
    has_table_ = !prices_.isEmpty();
}

void PlanAccess::refresh(bool force) {
    const QString account = account_key();
    if (account.isEmpty())
        return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!force && fetched_ms_ > 0 && now - fetched_ms_ < kPlanAccessFreshMs)
        return;
    if (in_flight_) {
        refetch_wanted_ = refetch_wanted_ || force;
        return;
    }
    in_flight_ = true;
    const QString signature = plan_signature();
    fincept::HttpClient::instance().get(
        QStringLiteral("/user/entitlements"),
        [this, account, signature](fincept::Result<QJsonDocument> result) {
            in_flight_ = false;
            fetched_ms_ = QDateTime::currentMSecsSinceEpoch();
            signature_ = signature;
            if (account != account_key()) {
                // Signed out, or another account signed in, while this was in flight.
                refetch_wanted_ = false;
                on_auth_changed();
                return;
            }
            if (result.is_err()) {
                // 404: a server without the route — nothing to show, keep any cached table
                // out of sight too (it describes a server that is not the one answering).
                const int status = fincept::HttpClient::status_from_error(result.error());
                if (status == 404 && has_table_) {
                    has_table_ = false;
                    emit changed();
                }
                LOG_DEBUG(kPlanAccessTag, QString("Plan table not available (HTTP %1)").arg(status));
            } else {
                const QJsonObject body = result.value().object();
                const QJsonObject data = body.value(QStringLiteral("data")).toObject();
                if (body.value(QStringLiteral("success")).toBool(true) && !data.isEmpty()) {
                    apply(data);
                    const QJsonObject cached{{QStringLiteral("account"), account}, {QStringLiteral("data"), data}};
                    const auto saved = SettingsRepository::instance().set(
                        kPlanAccessCacheKey, QString::fromUtf8(QJsonDocument(cached).toJson(QJsonDocument::Compact)),
                        QStringLiteral("billing"));
                    if (saved.is_err())
                        LOG_DEBUG(kPlanAccessTag, "Could not cache the plan table");
                    emit changed();
                }
            }
            if (refetch_wanted_) {
                refetch_wanted_ = false;
                refresh(true);
            }
        },
        this);
}

} // namespace fincept::billing
