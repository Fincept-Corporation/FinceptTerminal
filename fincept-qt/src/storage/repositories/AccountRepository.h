#pragma once
#include "storage/repositories/BaseRepository.h"
#include "trading/BrokerAccount.h"

namespace fincept {

/// Repository for the broker_accounts table.
/// Stores account metadata (credentials live in SecureStorage, not here).
class AccountRepository : public BaseRepository<trading::BrokerAccount> {
  public:
    static AccountRepository& instance();

    Result<void> insert(const trading::BrokerAccount& account);
    Result<void> update(const trading::BrokerAccount& account);
    Result<void> remove(const QString& account_id);

    std::optional<trading::BrokerAccount> find_by_id(const QString& account_id);
    Result<QVector<trading::BrokerAccount>> find_all();
    Result<QVector<trading::BrokerAccount>> find_by_broker(const QString& broker_id);
    Result<QVector<trading::BrokerAccount>> find_active();

  private:
    AccountRepository() = default;
    static trading::BrokerAccount map_row(QSqlQuery& q);
};

} // namespace fincept
