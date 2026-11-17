#pragma once
// DBnomicsDecode — the pure half of DBnomicsService: DBnomics REST answers (parsed JSON) and the
// service's cached copies of the catalogue lists into the model types, and those cached copies' text.
//
// QtCore only (plus the model header). No logging, cache, hub, network or widgets: it runs on the
// service's decode thread, and a standalone harness can link and time it.

#include "services/dbnomics/DBnomicsModels.h"

#include <QJsonDocument>
#include <QString>
#include <QVector>

namespace fincept::services::dbnomics {

/// One page of a provider's datasets: the rows and the provider's dataset count.
struct DbnDatasetPage {
    QVector<DbnDataset> datasets;
    int total = 0;
};

/// One page of a dataset's series. `ok` = false: the answer had no "series" envelope (a rate limit or
/// an outage answered 2xx) — a failure, never an honest "no series".
struct DbnSeriesPage {
    bool ok = true;
    QVector<DbnSeriesInfo> series;
    int total = 0;
};

/// One page of the global search.
struct DbnSearchPage {
    QVector<DbnSearchResult> results;
    int total = 0;
};

/// A series' observations: `point` when `ok`, else `error` ("No data returned for series").
struct DbnObservationsResult {
    bool ok = false;
    QString error;
    DbnDataPoint point;
};

// ── /providers ──
QVector<DbnProvider> decode_providers(const QJsonDocument& response);
QVector<DbnProvider> decode_providers_cache(const QString& cached);
QString encode_providers_cache(const QVector<DbnProvider>& providers);

// ── /datasets/<provider> ──
DbnDatasetPage decode_datasets(const QJsonDocument& response);
DbnDatasetPage decode_datasets_cache(const QString& cached);
QString encode_datasets_cache(const DbnDatasetPage& page);

// ── /series/<provider>/<dataset> (observations=false) ──
DbnSeriesPage decode_series(const QJsonDocument& response);
DbnSeriesPage decode_series_cache(const QString& cached);
QString encode_series_cache(const DbnSeriesPage& page);

// ── /search ──
DbnSearchPage decode_search(const QJsonDocument& response);

/// /series/<provider>/<dataset>/<series>?observations=1 → the series ready to plot: observations
/// with their period start day (QDate), ascending by period start, NA / non-numeric values as gaps,
/// and the frequency / unit / seasonal-adjustment / provider labels. `series_id` = "PROV/DS/CODE",
/// `series_code` names the series when the answer does not.
DbnObservationsResult decode_observations(const QJsonDocument& response, const QString& series_id,
                                          const QString& series_code);

} // namespace fincept::services::dbnomics
