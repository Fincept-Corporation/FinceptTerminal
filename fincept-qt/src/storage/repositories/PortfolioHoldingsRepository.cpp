#include "storage/repositories/PortfolioHoldingsRepository.h"

#include "multiuser/client/PhaseOneClientTransport.h"
#include "multiuser/client/PhaseOnePortfolioApi.h"
#include "multiuser/contracts/PhaseOnePortfolioTypes.h"

#include <QEventLoop>

namespace fincept {

namespace {

using fincept::auth::ApiResponse;
using fincept::multiuser::PhaseOneCreateHoldingRequest;
using fincept::multiuser::PhaseOneHoldingRecord;
using fincept::multiuser::PhaseOneHoldingsListResponse;
using fincept::multiuser::PhaseOnePortfolioApi;
using fincept::multiuser::PhaseOneRemoveHoldingRequest;
using fincept::multiuser::PhaseOneUpdateHoldingRequest;

bool use_phase_one_server() {
    return !fincept::multiuser::PhaseOneClientTransport::instance().session_id().isEmpty();
}

template <typename T>
Result<T> run_phase_one_request(const std::function<void(std::function<void(ApiResponse)>)>& start,
                                const std::function<Result<T>(const ApiResponse&)>& convert) {
    QEventLoop loop;
    bool completed = false;
    Result<T> result = Result<T>::err("phase_one_request_not_started");

    start([&](ApiResponse response) {
        result = convert(response);
        completed = true;
        loop.quit();
    });

    if (!completed)
        loop.exec();
    return result;
}

std::string response_error(const ApiResponse& response) {
    const QString message = !response.error.isEmpty() ? response.error : response.data.value("message").toString();
    return (message.isEmpty() ? QStringLiteral("Phase one holdings request failed.") : message).toStdString();
}

PortfolioHolding from_record(const PhaseOneHoldingRecord& record) {
    return {record.id, record.symbol, record.name, record.shares, record.avg_cost,
            record.active, record.added_at, record.updated_at};
}

} // namespace

PortfolioHoldingsRepository& PortfolioHoldingsRepository::instance() {
    static PortfolioHoldingsRepository s;
    return s;
}

PortfolioHolding PortfolioHoldingsRepository::map_row(QSqlQuery& q) {
    return {
        q.value(0).toInt(),    q.value(1).toString(), q.value(2).toString(), q.value(3).toDouble(),
        q.value(4).toDouble(), q.value(5).toBool(),   q.value(6).toString(), q.value(7).toString(),
    };
}

Result<QVector<PortfolioHolding>> PortfolioHoldingsRepository::get_active() {
    if (use_phase_one_server()) {
        const auto result = list_all();
        if (result.is_err())
            return result;

        QVector<PortfolioHolding> active;
        for (const auto& holding : result.value()) {
            if (holding.active)
                active.append(holding);
        }
        return Result<QVector<PortfolioHolding>>::ok(active);
    }

    return query_list("SELECT id, symbol, name, shares, avg_cost, active, added_at, updated_at "
                      "FROM portfolio_holdings WHERE active = 1 ORDER BY symbol",
                      {}, map_row);
}

Result<QVector<PortfolioHolding>> PortfolioHoldingsRepository::list_all() {
    if (use_phase_one_server()) {
        return run_phase_one_request<QVector<PortfolioHolding>>(
            [](auto cb) { PhaseOnePortfolioApi::instance().list_holdings({}, std::move(cb)); },
            [](const ApiResponse& response) {
                if (!response.success)
                    return Result<QVector<PortfolioHolding>>::err(response_error(response));

                const auto parsed = PhaseOneHoldingsListResponse::from_json(response.data);
                QVector<PortfolioHolding> holdings;
                holdings.reserve(parsed.holdings.size());
                for (const auto& record : parsed.holdings) {
                    if (!record.portfolio_id.isEmpty())
                        continue;
                    holdings.append(from_record(record));
                }
                return Result<QVector<PortfolioHolding>>::ok(holdings);
            });
    }

    return query_list("SELECT id, symbol, name, shares, avg_cost, active, added_at, updated_at "
                      "FROM portfolio_holdings ORDER BY symbol",
                      {}, map_row);
}

Result<PortfolioHolding> PortfolioHoldingsRepository::get_by_id(int id) {
    if (use_phase_one_server()) {
        const auto result = list_all();
        if (result.is_err())
            return Result<PortfolioHolding>::err(result.error());
        for (const auto& holding : result.value()) {
            if (holding.id == id)
                return Result<PortfolioHolding>::ok(holding);
        }
        return Result<PortfolioHolding>::err("Not found");
    }

    return query_one("SELECT id, symbol, name, shares, avg_cost, active, added_at, updated_at "
                     "FROM portfolio_holdings WHERE id = ?",
                     {id}, map_row);
}

Result<qint64> PortfolioHoldingsRepository::add(const QString& symbol, double shares, double avg_cost,
                                                 const QString& name) {
    if (use_phase_one_server()) {
        return run_phase_one_request<qint64>(
            [symbol, shares, avg_cost, name](auto cb) {
                PhaseOneCreateHoldingRequest request;
                request.symbol = symbol;
                request.name = name;
                request.shares = shares;
                request.avg_cost = avg_cost;
                PhaseOnePortfolioApi::instance().create_holding(request, std::move(cb));
            },
            [](const ApiResponse& response) {
                if (!response.success)
                    return Result<qint64>::err(response_error(response));
                return Result<qint64>::ok(
                    fincept::multiuser::PhaseOneHoldingRecord::from_json(response.data.value("holding").toObject()).id);
            });
    }

    return exec_insert("INSERT INTO portfolio_holdings (symbol, name, shares, avg_cost) VALUES (?, ?, ?, ?)",
                       {symbol.toUpper(), name, shares, avg_cost});
}

Result<void> PortfolioHoldingsRepository::update(int id, double shares, double avg_cost) {
    if (use_phase_one_server()) {
        return run_phase_one_request<void>(
            [id, shares, avg_cost](auto cb) {
                PhaseOneUpdateHoldingRequest request;
                request.id = id;
                request.shares = shares;
                request.avg_cost = avg_cost;
                PhaseOnePortfolioApi::instance().update_holding(request, std::move(cb));
            },
            [](const ApiResponse& response) {
                if (!response.success)
                    return Result<void>::err(response_error(response));
                return Result<void>::ok();
            });
    }

    return exec_write("UPDATE portfolio_holdings SET shares = ?, avg_cost = ?, updated_at = datetime('now') "
                      "WHERE id = ?",
                      {shares, avg_cost, id});
}

Result<void> PortfolioHoldingsRepository::deactivate(int id) {
    return exec_write("UPDATE portfolio_holdings SET active = 0, updated_at = datetime('now') WHERE id = ?", {id});
}

Result<void> PortfolioHoldingsRepository::remove(int id) {
    if (use_phase_one_server()) {
        return run_phase_one_request<void>(
            [id](auto cb) {
                PhaseOneRemoveHoldingRequest request;
                request.id = id;
                PhaseOnePortfolioApi::instance().remove_holding(request, std::move(cb));
            },
            [](const ApiResponse& response) {
                if (!response.success)
                    return Result<void>::err(response_error(response));
                return Result<void>::ok();
            });
    }

    return exec_write("DELETE FROM portfolio_holdings WHERE id = ?", {id});
}

} // namespace fincept
