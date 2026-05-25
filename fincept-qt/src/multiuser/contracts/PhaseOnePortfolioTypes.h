#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QUrlQuery>
#include <QVector>

namespace fincept::multiuser {

struct PhaseOnePortfolioRecord {
    QString id;
    QString name;
    QString owner;
    QString currency;
    QString description;
    QString created_at;
    QString updated_at;
    QString broker_account_id;

    QJsonObject to_json() const {
        return QJsonObject{{"id", id},
                           {"name", name},
                           {"owner", owner},
                           {"currency", currency},
                           {"description", description},
                           {"created_at", created_at},
                           {"updated_at", updated_at},
                           {"broker_account_id", broker_account_id}};
    }

    static PhaseOnePortfolioRecord from_json(const QJsonObject& object) {
        PhaseOnePortfolioRecord record;
        record.id = object.value("id").toString();
        record.name = object.value("name").toString();
        record.owner = object.value("owner").toString();
        record.currency = object.value("currency").toString();
        record.description = object.value("description").toString();
        record.created_at = object.value("created_at").toString();
        record.updated_at = object.value("updated_at").toString();
        record.broker_account_id = object.value("broker_account_id").toString();
        return record;
    }
};

struct PhaseOnePortfolioListResponse {
    QVector<PhaseOnePortfolioRecord> portfolios;

    static PhaseOnePortfolioListResponse from_json(const QJsonObject& object) {
        PhaseOnePortfolioListResponse response;
        const QJsonArray array = object.value("portfolios").toArray();
        response.portfolios.reserve(array.size());
        for (const QJsonValue& value : array)
            response.portfolios.append(PhaseOnePortfolioRecord::from_json(value.toObject()));
        return response;
    }
};

struct PhaseOneCreatePortfolioRequest {
    QString name;
    QString owner;
    QString currency;
    QString description;
    QString broker_account_id;

    QJsonObject to_json() const {
        return QJsonObject{{"name", name},
                           {"owner", owner},
                           {"currency", currency},
                           {"description", description},
                           {"broker_account_id", broker_account_id}};
    }
};

struct PhaseOneUpdatePortfolioRequest {
    QString id;
    QString name;
    QString owner;
    QString currency;
    QString description;

    QJsonObject to_json() const {
        return QJsonObject{{"id", id},
                           {"name", name},
                           {"owner", owner},
                           {"currency", currency},
                           {"description", description}};
    }
};

struct PhaseOneDeletePortfolioRequest {
    QString id;

    QJsonObject to_json() const { return QJsonObject{{"id", id}}; }
};

struct PhaseOneHoldingRecord {
    int id = 0;
    QString portfolio_id;
    QString symbol;
    QString name;
    double shares = 0;
    double avg_cost = 0;
    bool active = true;
    QString added_at;
    QString updated_at;
    QString sector;
    QString broker_symbol;
    QString exchange;

    QJsonObject to_json() const {
        return QJsonObject{{"id", id},
                           {"portfolio_id", portfolio_id},
                           {"symbol", symbol},
                           {"name", name},
                           {"shares", shares},
                           {"avg_cost", avg_cost},
                           {"active", active},
                           {"added_at", added_at},
                           {"updated_at", updated_at},
                           {"sector", sector},
                           {"broker_symbol", broker_symbol},
                           {"exchange", exchange}};
    }

    static PhaseOneHoldingRecord from_json(const QJsonObject& object) {
        PhaseOneHoldingRecord record;
        record.id = object.value("id").toInt();
        record.portfolio_id = object.value("portfolio_id").toString();
        record.symbol = object.value("symbol").toString();
        record.name = object.value("name").toString();
        record.shares = object.value("shares").toDouble();
        record.avg_cost = object.value("avg_cost").toDouble();
        record.active = object.value("active").toBool(true);
        record.added_at = object.value("added_at").toString();
        record.updated_at = object.value("updated_at").toString();
        record.sector = object.value("sector").toString();
        record.broker_symbol = object.value("broker_symbol").toString();
        record.exchange = object.value("exchange").toString();
        return record;
    }
};

struct PhaseOneHoldingsListResponse {
    QVector<PhaseOneHoldingRecord> holdings;

    static PhaseOneHoldingsListResponse from_json(const QJsonObject& object) {
        PhaseOneHoldingsListResponse response;
        const QJsonArray array = object.value("holdings").toArray();
        response.holdings.reserve(array.size());
        for (const QJsonValue& value : array)
            response.holdings.append(PhaseOneHoldingRecord::from_json(value.toObject()));
        return response;
    }
};

struct PhaseOneCreateHoldingRequest {
    QString portfolio_id;
    QString symbol;
    QString name;
    double shares = 0;
    double avg_cost = 0;
    QString acquired_at;
    QString sector;
    QString broker_symbol;
    QString exchange;

    QJsonObject to_json() const {
        return QJsonObject{{"portfolio_id", portfolio_id},
                           {"symbol", symbol},
                           {"name", name},
                           {"shares", shares},
                           {"avg_cost", avg_cost},
                           {"acquired_at", acquired_at},
                           {"sector", sector},
                           {"broker_symbol", broker_symbol},
                           {"exchange", exchange}};
    }
};

struct PhaseOneUpdateHoldingRequest {
    int id = 0;
    QString portfolio_id;
    double shares = 0;
    double avg_cost = 0;
    QString sector;

    QJsonObject to_json() const {
        return QJsonObject{{"id", id},
                           {"portfolio_id", portfolio_id},
                           {"shares", shares},
                           {"avg_cost", avg_cost},
                           {"sector", sector}};
    }
};

struct PhaseOneRemoveHoldingRequest {
    int id = 0;
    QString portfolio_id;

    QJsonObject to_json() const { return QJsonObject{{"id", id}, {"portfolio_id", portfolio_id}}; }
};

struct PhaseOneHoldingQuery {
    QString portfolio_id;

    QString to_query_string() const {
        QUrlQuery query;
        if (!portfolio_id.isEmpty())
            query.addQueryItem(QStringLiteral("portfolio_id"), portfolio_id);
        return query.toString(QUrl::FullyEncoded);
    }
};

} // namespace fincept::multiuser
