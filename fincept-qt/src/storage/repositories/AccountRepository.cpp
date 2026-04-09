#include "storage/repositories/AccountRepository.h"

namespace fincept {

AccountRepository& AccountRepository::instance() {
    static AccountRepository s;
    return s;
}

trading::BrokerAccount AccountRepository::map_row(QSqlQuery& q) {
    trading::BrokerAccount a;
    a.account_id         = q.value(0).toString();
    a.broker_id          = q.value(1).toString();
    a.display_name       = q.value(2).toString();
    a.paper_portfolio_id = q.value(3).toString();
    a.trading_mode       = q.value(4).toString();
    a.is_active          = q.value(5).toBool();
    a.created_at         = q.value(6).toString();
    return a;
}

Result<void> AccountRepository::insert(const trading::BrokerAccount& account) {
    return exec_write(
        "INSERT INTO broker_accounts (id, broker_id, display_name, paper_portfolio_id, trading_mode, is_active, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)",
        {account.account_id, account.broker_id, account.display_name,
         account.paper_portfolio_id, account.trading_mode,
         account.is_active ? 1 : 0, account.created_at});
}

Result<void> AccountRepository::update(const trading::BrokerAccount& account) {
    return exec_write(
        "UPDATE broker_accounts SET broker_id = ?, display_name = ?, paper_portfolio_id = ?, "
        "trading_mode = ?, is_active = ? WHERE id = ?",
        {account.broker_id, account.display_name, account.paper_portfolio_id,
         account.trading_mode, account.is_active ? 1 : 0, account.account_id});
}

Result<void> AccountRepository::remove(const QString& account_id) {
    return exec_write("DELETE FROM broker_accounts WHERE id = ?", {account_id});
}

std::optional<trading::BrokerAccount> AccountRepository::find_by_id(const QString& account_id) {
    return query_optional(
        "SELECT id, broker_id, display_name, paper_portfolio_id, trading_mode, is_active, created_at "
        "FROM broker_accounts WHERE id = ?",
        {account_id}, map_row);
}

Result<QVector<trading::BrokerAccount>> AccountRepository::find_all() {
    return query_list(
        "SELECT id, broker_id, display_name, paper_portfolio_id, trading_mode, is_active, created_at "
        "FROM broker_accounts ORDER BY created_at",
        {}, map_row);
}

Result<QVector<trading::BrokerAccount>> AccountRepository::find_by_broker(const QString& broker_id) {
    return query_list(
        "SELECT id, broker_id, display_name, paper_portfolio_id, trading_mode, is_active, created_at "
        "FROM broker_accounts WHERE broker_id = ? ORDER BY created_at",
        {broker_id}, map_row);
}

Result<QVector<trading::BrokerAccount>> AccountRepository::find_active() {
    return query_list(
        "SELECT id, broker_id, display_name, paper_portfolio_id, trading_mode, is_active, created_at "
        "FROM broker_accounts WHERE is_active = 1 ORDER BY created_at",
        {}, map_row);
}

} // namespace fincept
