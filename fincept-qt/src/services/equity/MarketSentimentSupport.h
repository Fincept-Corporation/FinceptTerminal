#pragma once

#include "services/equity/EquityResearchModels.h"

#include <QJsonObject>
#include <QStringList>

namespace fincept::services::equity::sentiment {

inline constexpr const char* kProviderId = "adanos-market-sentiment";

QStringList source_ids();
QString source_label(const QString& source_id);
bool source_has_signal(const SentimentSourceSnapshot& snapshot);
SentimentSourceSnapshot parse_compare_payload(const QString& source_id, const QJsonObject& payload);
QString compute_source_alignment(const QVector<SentimentSourceSnapshot>& sources);
QJsonObject snapshot_to_json(const MarketSentimentSnapshot& snapshot);
MarketSentimentSnapshot snapshot_from_json(const QJsonObject& object);

} // namespace fincept::services::equity::sentiment
