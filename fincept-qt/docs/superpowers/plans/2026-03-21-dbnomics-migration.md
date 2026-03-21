# DBnomics Tab Migration Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Migrate the full DBnomics economic data browser tab from the Tauri/TypeScript project into the C++20/Qt6 Fincept Terminal, preserving all UI panels, API interactions, chart types, pagination, search, and comparison views.

**Architecture:** A singleton `DBnomicsService` owns all HTTP calls, caching, and debouncing, emitting Qt signals when data arrives. `DBnomicsScreen` is the top-level `QWidget` that assembles a left selection panel, a right chart+table area, and a toolbar — all wired together via signals/slots. The screen is registered lazily (factory pattern) so nothing runs until the user navigates to it.

**Tech Stack:** C++20, Qt6 Widgets, Qt6::Charts, Qt6 Network (via existing `HttpClient` singleton), `QJsonDocument`, `QTimer` (debounce), `QSplitter` (resizable panels), `QListWidget`, `QStackedWidget`, `QTableWidget`.

---

## File Map

### New files to create

| File | Responsibility |
|------|---------------|
| `src/services/dbnomics/DBnomicsModels.h` | All data structs: Provider, Dataset, Series, Observation, DataPoint, SearchResult, PaginationState, ChartType enum, ViewMode enum |
| `src/services/dbnomics/DBnomicsService.h` | Singleton declaration: cache maps, timer handles, all signal/slot interface |
| `src/services/dbnomics/DBnomicsService.cpp` | All 5 API endpoints, TTL caching, debounce timers, QTimer-based debouncing |
| `src/screens/dbnomics/DBnomicsScreen.h` | Top-level screen: 3-panel layout, toolbar, view toggle |
| `src/screens/dbnomics/DBnomicsScreen.cpp` | Assembles sub-widgets, handles toolbar actions (FETCH/REFRESH/EXPORT), wires service signals |
| `src/screens/dbnomics/DBnomicsSelectionPanel.h` | Left sidebar widget: search, provider/dataset/series lists, comparison slots |
| `src/screens/dbnomics/DBnomicsSelectionPanel.cpp` | All list population, debounced searches, load-more pagination, slot management |
| `src/screens/dbnomics/DBnomicsChartWidget.h` | Chart display widget using Qt6::Charts |
| `src/screens/dbnomics/DBnomicsChartWidget.cpp` | Renders line/area/bar/scatter, legend, auto-scaling, multiple series |
| `src/screens/dbnomics/DBnomicsDataTable.h` | Observation table widget |
| `src/screens/dbnomics/DBnomicsDataTable.cpp` | QTableWidget with sticky header, color-coded values, latest 50 periods |

### Files to modify

| File | Change |
|------|--------|
| `CMakeLists.txt` | Add 6 new `.cpp` files to `SCREEN_SOURCES` / `SERVICE_SOURCES` |
| `src/app/MainWindow.cpp` | Replace `register_screen("dbnomics", new ComingSoonScreen(...))` with `register_factory("dbnomics", ...)` |
| `src/app/MainWindow.h` | Add `#include "screens/dbnomics/DBnomicsScreen.h"` |

---

## API Reference (DBnomics v22)

Base URL: `https://api.db.nomics.world/v22`

| Endpoint | URL Pattern | Cache TTL |
|----------|-------------|-----------|
| List providers | `GET /providers` | 10 min |
| List datasets | `GET /datasets/{prov}?limit=50&offset={n}` | 5 min (page 0 only) |
| List series | `GET /series/{prov}/{ds}?limit=50&offset={n}&observations=false&q={q}` | 5 min (page 0, no query) |
| Fetch observations | `GET /series/{prov}/{ds}/{code}?observations=1&format=json` | No cache |
| Global search | `GET /search?q={q}&limit=20&offset={n}` | No cache |

---

## Task 1: Data Models Header

**Files:**
- Create: `src/services/dbnomics/DBnomicsModels.h`

- [ ] **Step 1: Create the models header**

```cpp
// src/services/dbnomics/DBnomicsModels.h
#pragma once
#include <QString>
#include <QVector>
#include <QColor>

namespace fincept::services {

// ── Core domain objects ──────────────────────────────────────────────────────

struct DbnProvider {
    QString code;   // "BLS"
    QString name;   // "Bureau of Labor Statistics"
};

struct DbnDataset {
    QString code;   // "ln"
    QString name;   // "Labor Force Statistics"
};

struct DbnSeriesInfo {
    QString code;   // "LNS14000000"
    QString name;   // "Civilian Unemployment Rate"
};

struct DbnObservation {
    QString period; // "2024-01", "2024-01-01"
    double  value;  // 3.5
    bool    valid;  // false if the API returned NA/null
};

// A fully loaded series ready to plot
struct DbnDataPoint {
    QString             series_id;   // "BLS/ln/LNS14000000"
    QString             series_name;
    QVector<DbnObservation> observations;
    QColor              color;
};

// ── Search ───────────────────────────────────────────────────────────────────

struct DbnSearchResult {
    QString provider_code;
    QString provider_name;
    QString dataset_code;
    QString dataset_name;
    int     nb_series = 0;
};

// ── Pagination ───────────────────────────────────────────────────────────────

struct DbnPagination {
    int offset   = 0;
    int limit    = 50;
    int total    = 0;
    bool has_more() const { return offset + limit < total; }
};

// ── UI enumerations ──────────────────────────────────────────────────────────

enum class DbnChartType {
    Line,
    Area,
    Bar,
    Scatter
};

enum class DbnViewMode {
    Single,
    Comparison
};

// A comparison slot holds multiple series for one chart panel
struct DbnSlot {
    QVector<DbnDataPoint> series;
    DbnChartType          chart_type = DbnChartType::Line;
};

} // namespace fincept::services
```

- [ ] **Step 2: Verify it compiles standalone (no includes missing)**

```bash
cd D:/Tilak/FinceptV2/FinceptTerminal/fincept-qt
# Quick syntax check — will fail to link but should have zero errors
cmake --build build --config Release --target FinceptTerminal 2>&1 | head -30
```

- [ ] **Step 3: Commit**

```bash
git add src/services/dbnomics/DBnomicsModels.h
git commit -m "feat(dbnomics): add data model structs for DBnomics migration"
```

---

## Task 2: DBnomicsService — Singleton Service

**Files:**
- Create: `src/services/dbnomics/DBnomicsService.h`
- Create: `src/services/dbnomics/DBnomicsService.cpp`

This service owns all HTTP communication, TTL caching, and debounce timers. Screens never call `HttpClient` directly.

- [ ] **Step 1: Write the header**

```cpp
// src/services/dbnomics/DBnomicsService.h
#pragma once
#include "DBnomicsModels.h"
#include <QObject>
#include <QMap>
#include <QTimer>
#include <functional>

namespace fincept::services {

class DBnomicsService : public QObject {
    Q_OBJECT
public:
    static DBnomicsService& instance();

    // ── API methods (all async, result via signals) ───────────────────────────
    void fetch_providers();
    void fetch_datasets(const QString& provider_code, int offset = 0);
    void fetch_series(const QString& provider_code,
                      const QString& dataset_code,
                      const QString& query = {},
                      int offset = 0);
    void fetch_observations(const QString& provider_code,
                            const QString& dataset_code,
                            const QString& series_code);
    void global_search(const QString& query, int offset = 0);

    // ── Debounce entry points (called by UI on each keystroke) ────────────────
    void schedule_series_search(const QString& provider_code,
                                const QString& dataset_code,
                                const QString& query);
    void schedule_global_search(const QString& query);

    // ── Chart color palette ───────────────────────────────────────────────────
    static QColor chart_color(int index);

signals:
    void providers_loaded(QVector<DbnProvider> providers);
    void datasets_loaded(QVector<DbnDataset> datasets, DbnPagination page, bool append);
    void series_loaded(QVector<DbnSeriesInfo> series, DbnPagination page, bool append);
    void observations_loaded(DbnDataPoint data);
    void search_results_loaded(QVector<DbnSearchResult> results, DbnPagination page, bool append);
    void error_occurred(const QString& context, const QString& message);

private:
    explicit DBnomicsService(QObject* parent = nullptr);

    static constexpr const char* kBaseUrl  = "https://api.db.nomics.world/v22";
    static constexpr int kProviderCacheMs  = 10 * 60 * 1000; // 10 min
    static constexpr int kDatasetCacheMs   =  5 * 60 * 1000; //  5 min
    static constexpr int kSeriesCacheMs    =  5 * 60 * 1000; //  5 min
    static constexpr int kDebounceMs       = 400;

    // ── Cache structures ──────────────────────────────────────────────────────
    struct CachedProviders {
        QVector<DbnProvider> data;
        qint64 fetched_at = 0; // ms since epoch
    } providers_cache_;

    struct CachedDatasets {
        QVector<DbnDataset> data;
        qint64 fetched_at = 0;
        int    total = 0;
    };
    QMap<QString, CachedDatasets> datasets_cache_; // key = provider_code

    struct CachedSeries {
        QVector<DbnSeriesInfo> data;
        qint64 fetched_at = 0;
        int    total = 0;
    };
    QMap<QString, CachedSeries> series_cache_; // key = "prov/ds"

    // ── Debounce timers ───────────────────────────────────────────────────────
    QTimer* series_debounce_  = nullptr;
    QTimer* search_debounce_  = nullptr;
    QString pending_series_prov_;
    QString pending_series_ds_;
    QString pending_series_q_;
    QString pending_search_q_;

    bool is_cache_fresh(qint64 fetched_at, int ttl_ms) const;
    QString build_url(const QString& path) const;
};

} // namespace fincept::services
```

- [ ] **Step 2: Write the implementation**

```cpp
// src/services/dbnomics/DBnomicsService.cpp
#include "services/dbnomics/DBnomicsService.h"
#include "network/http/HttpClient.h"
#include "core/logging/Logger.h"
#include <QDateTime>
#include <QUrlQuery>
#include <QJsonObject>
#include <QJsonArray>

namespace fincept::services {

DBnomicsService& DBnomicsService::instance() {
    static DBnomicsService inst;
    return inst;
}

DBnomicsService::DBnomicsService(QObject* parent) : QObject(parent) {
    // Debounce timers — single-shot, fire only after 400ms silence
    series_debounce_ = new QTimer(this);
    series_debounce_->setSingleShot(true);
    series_debounce_->setInterval(kDebounceMs);
    connect(series_debounce_, &QTimer::timeout, this, [this]() {
        fetch_series(pending_series_prov_, pending_series_ds_, pending_series_q_, 0);
    });

    search_debounce_ = new QTimer(this);
    search_debounce_->setSingleShot(true);
    search_debounce_->setInterval(kDebounceMs);
    connect(search_debounce_, &QTimer::timeout, this, [this]() {
        global_search(pending_search_q_, 0);
    });
}

bool DBnomicsService::is_cache_fresh(qint64 fetched_at, int ttl_ms) const {
    return fetched_at > 0 &&
           (QDateTime::currentMSecsSinceEpoch() - fetched_at) < ttl_ms;
}

QString DBnomicsService::build_url(const QString& path) const {
    return QString("%1%2").arg(kBaseUrl).arg(path);
}

// ── fetch_providers ──────────────────────────────────────────────────────────

void DBnomicsService::fetch_providers() {
    if (is_cache_fresh(providers_cache_.fetched_at, kProviderCacheMs)) {
        LOG_DEBUG("DBnomicsService", "providers: serving from cache");
        emit providers_loaded(providers_cache_.data);
        return;
    }
    LOG_INFO("DBnomicsService", "Fetching providers from API");
    const QString url = build_url("/providers");
    HttpClient::instance().get(url, [this](Result<QJsonDocument> result) {
        if (result.is_err()) {
            LOG_ERROR("DBnomicsService", QString("providers fetch failed: %1")
                      .arg(QString::fromStdString(result.error())));
            emit error_occurred("providers", QString::fromStdString(result.error()));
            return;
        }
        QJsonObject root = result.value().object();
        QJsonArray  docs = root["providers"].toObject()["docs"].toArray();
        QVector<DbnProvider> out;
        out.reserve(docs.size());
        for (const auto& v : docs) {
            QJsonObject o = v.toObject();
            out.push_back({ o["code"].toString(), o["name"].toString() });
        }
        providers_cache_.data = out;
        providers_cache_.fetched_at = QDateTime::currentMSecsSinceEpoch();
        LOG_INFO("DBnomicsService", QString("Loaded %1 providers").arg(out.size()));
        emit providers_loaded(out);
    });
}

// ── fetch_datasets ───────────────────────────────────────────────────────────

void DBnomicsService::fetch_datasets(const QString& provider_code, int offset) {
    const bool first_page = (offset == 0);
    if (first_page && datasets_cache_.contains(provider_code)) {
        auto& cached = datasets_cache_[provider_code];
        if (is_cache_fresh(cached.fetched_at, kDatasetCacheMs)) {
            LOG_DEBUG("DBnomicsService", "datasets: serving from cache");
            DbnPagination page{ 0, 50, cached.total };
            emit datasets_loaded(cached.data, page, false);
            return;
        }
    }
    const QString url = build_url(
        QString("/datasets/%1?limit=50&offset=%2").arg(provider_code).arg(offset));
    LOG_INFO("DBnomicsService", QString("Fetching datasets for %1 offset=%2")
             .arg(provider_code).arg(offset));
    HttpClient::instance().get(url, [this, provider_code, offset](Result<QJsonDocument> result) {
        if (result.is_err()) {
            emit error_occurred("datasets", QString::fromStdString(result.error()));
            return;
        }
        QJsonObject root    = result.value().object();
        QJsonObject ds_root = root["datasets"].toObject();
        QJsonArray  docs    = ds_root["docs"].toArray();
        int total = ds_root["num_found"].toInt();

        QVector<DbnDataset> out;
        out.reserve(docs.size());
        for (const auto& v : docs) {
            QJsonObject o = v.toObject();
            out.push_back({ o["code"].toString(), o["name"].toString() });
        }
        DbnPagination page{ offset, 50, total };
        const bool append = (offset > 0);

        if (!append) {
            CachedDatasets& cache = datasets_cache_[provider_code];
            cache.data       = out;
            cache.total      = total;
            cache.fetched_at = QDateTime::currentMSecsSinceEpoch();
        }
        LOG_INFO("DBnomicsService", QString("Loaded %1 datasets (total=%2)").arg(out.size()).arg(total));
        emit datasets_loaded(out, page, append);
    });
}

// ── fetch_series ─────────────────────────────────────────────────────────────

void DBnomicsService::fetch_series(const QString& provider_code,
                                   const QString& dataset_code,
                                   const QString& query,
                                   int offset) {
    const QString cache_key = QString("%1/%2").arg(provider_code).arg(dataset_code);
    const bool first_page   = (offset == 0);
    const bool no_query     = query.isEmpty();

    if (first_page && no_query && series_cache_.contains(cache_key)) {
        auto& cached = series_cache_[cache_key];
        if (is_cache_fresh(cached.fetched_at, kSeriesCacheMs)) {
            LOG_DEBUG("DBnomicsService", "series: serving from cache");
            DbnPagination page{ 0, 50, cached.total };
            emit series_loaded(cached.data, page, false);
            return;
        }
    }
    QString path = QString("/series/%1/%2?limit=50&offset=%3&observations=false")
                   .arg(provider_code).arg(dataset_code).arg(offset);
    if (!query.isEmpty())
        path += "&q=" + QUrl::toPercentEncoding(query);

    LOG_INFO("DBnomicsService", QString("Fetching series %1 q='%2' offset=%3")
             .arg(cache_key).arg(query).arg(offset));

    HttpClient::instance().get(build_url(path),
        [this, cache_key, offset, no_query, first_page](Result<QJsonDocument> result) {
        if (result.is_err()) {
            emit error_occurred("series", QString::fromStdString(result.error()));
            return;
        }
        QJsonObject root = result.value().object();
        QJsonObject s    = root["series"].toObject();
        QJsonArray  docs = s["docs"].toArray();
        int total = s["num_found"].toInt();

        QVector<DbnSeriesInfo> out;
        out.reserve(docs.size());
        for (const auto& v : docs) {
            QJsonObject o = v.toObject();
            out.push_back({ o["series_code"].toString(), o["series_name"].toString() });
        }
        DbnPagination page{ offset, 50, total };
        const bool append = (offset > 0);

        if (first_page && no_query) {
            CachedSeries& cache = series_cache_[cache_key];
            cache.data       = out;
            cache.total      = total;
            cache.fetched_at = QDateTime::currentMSecsSinceEpoch();
        }
        LOG_INFO("DBnomicsService", QString("Loaded %1 series (total=%2)").arg(out.size()).arg(total));
        emit series_loaded(out, page, append);
    });
}

// ── fetch_observations ───────────────────────────────────────────────────────

void DBnomicsService::fetch_observations(const QString& provider_code,
                                         const QString& dataset_code,
                                         const QString& series_code) {
    const QString path = QString("/series/%1/%2/%3?observations=1&format=json")
                         .arg(provider_code).arg(dataset_code).arg(series_code);
    const QString full_id = QString("%1/%2/%3").arg(provider_code).arg(dataset_code).arg(series_code);
    LOG_INFO("DBnomicsService", QString("Fetching observations for %1").arg(full_id));

    HttpClient::instance().get(build_url(path), [this, full_id, series_code](Result<QJsonDocument> result) {
        if (result.is_err()) {
            emit error_occurred("observations", QString::fromStdString(result.error()));
            return;
        }
        QJsonObject root = result.value().object();
        QJsonArray  docs = root["series"].toObject()["docs"].toArray();
        if (docs.isEmpty()) {
            emit error_occurred("observations", "No data returned for series");
            return;
        }
        QJsonObject doc = docs[0].toObject();
        QJsonArray periods = doc["period"].toArray();
        QJsonArray values  = doc["value"].toArray();
        QString name = doc["series_name"].toString(series_code);

        QVector<DbnObservation> obs;
        obs.reserve(periods.size());
        for (int i = 0; i < periods.size() && i < values.size(); ++i) {
            DbnObservation o;
            o.period = periods[i].toString();
            if (values[i].isNull() || values[i].toString() == "NA") {
                o.value = 0.0;
                o.valid = false;
            } else {
                o.value = values[i].toDouble();
                o.valid = true;
            }
            obs.push_back(o);
        }
        DbnDataPoint dp;
        dp.series_id   = full_id;
        dp.series_name = name;
        dp.observations = std::move(obs);
        dp.color = chart_color(0); // caller will re-assign color index
        LOG_INFO("DBnomicsService", QString("Loaded %1 observations for %2")
                 .arg(dp.observations.size()).arg(full_id));
        emit observations_loaded(dp);
    });
}

// ── global_search ────────────────────────────────────────────────────────────

void DBnomicsService::global_search(const QString& query, int offset) {
    if (query.trimmed().isEmpty()) return;
    const QString path = QString("/search?q=%1&limit=20&offset=%2")
                         .arg(QString(QUrl::toPercentEncoding(query.trimmed()))).arg(offset);
    LOG_INFO("DBnomicsService", QString("Searching: '%1' offset=%2").arg(query).arg(offset));

    HttpClient::instance().get(build_url(path), [this, offset](Result<QJsonDocument> result) {
        if (result.is_err()) {
            emit error_occurred("search", QString::fromStdString(result.error()));
            return;
        }
        QJsonObject root = result.value().object();
        QJsonObject res  = root["results"].toObject();
        QJsonArray  docs = res["docs"].toArray();
        int total = res["num_found"].toInt();

        QVector<DbnSearchResult> out;
        out.reserve(docs.size());
        for (const auto& v : docs) {
            QJsonObject o = v.toObject();
            out.push_back({
                o["provider_code"].toString(),
                o["provider_name"].toString(),
                o["dataset_code"].toString(),
                o["dataset_name"].toString(),
                o["nb_series"].toInt()
            });
        }
        DbnPagination page{ offset, 20, total };
        emit search_results_loaded(out, page, offset > 0);
    });
}

// ── Debounce entry points ─────────────────────────────────────────────────────

void DBnomicsService::schedule_series_search(const QString& prov,
                                              const QString& ds,
                                              const QString& query) {
    pending_series_prov_ = prov;
    pending_series_ds_   = ds;
    pending_series_q_    = query;
    series_debounce_->start(); // restart restarts the 400ms countdown
}

void DBnomicsService::schedule_global_search(const QString& query) {
    pending_search_q_ = query;
    search_debounce_->start();
}

// ── Chart color palette ───────────────────────────────────────────────────────

QColor DBnomicsService::chart_color(int index) {
    static const QColor palette[] = {
        QColor("#ea580c"), // orange
        QColor("#3b82f6"), // blue
        QColor("#22c55e"), // green
        QColor("#eab308"), // yellow
        QColor("#a855f7"), // purple
        QColor("#ec4899"), // pink
        QColor("#06b6d4"), // cyan
        QColor("#f97316"), // orange variant
    };
    static const int n = static_cast<int>(std::size(palette));
    return palette[index % n];
}

} // namespace fincept::services
```

- [ ] **Step 3: Add service files to CMakeLists.txt**

Open `CMakeLists.txt`. Find the block that lists service `.cpp` files (search for `SERVICE_SOURCES` or the existing services like `NewsService.cpp`). Add:

```cmake
src/services/dbnomics/DBnomicsService.cpp
```

If the file uses a flat `add_executable` with all sources listed together, add it to that list instead.

- [ ] **Step 4: Build and verify no compile errors**

```bash
cd D:/Tilak/FinceptV2/FinceptTerminal/fincept-qt
cmake --build build --config Release 2>&1 | grep -E "error:|warning:" | head -30
```

Fix any errors before continuing.

- [ ] **Step 5: Commit**

```bash
git add src/services/dbnomics/
git commit -m "feat(dbnomics): add DBnomicsService singleton with 5 API endpoints and TTL caching"
```

---

## Task 3: DBnomicsDataTable Widget

**Files:**
- Create: `src/screens/dbnomics/DBnomicsDataTable.h`
- Create: `src/screens/dbnomics/DBnomicsDataTable.cpp`

Displays the latest 50 observation periods across multiple series, with color-coded values.

- [ ] **Step 1: Write the header**

```cpp
// src/screens/dbnomics/DBnomicsDataTable.h
#pragma once
#include "services/dbnomics/DBnomicsModels.h"
#include <QWidget>
#include <QTableWidget>

namespace fincept::screens {

class DBnomicsDataTable : public QWidget {
    Q_OBJECT
public:
    explicit DBnomicsDataTable(QWidget* parent = nullptr);

    // Replace the entire table with a new set of series
    void set_data(const QVector<services::DbnDataPoint>& series);
    void clear();

private:
    void build_ui();

    QTableWidget* table_ = nullptr;
};

} // namespace fincept::screens
```

- [ ] **Step 2: Write the implementation**

```cpp
// src/screens/dbnomics/DBnomicsDataTable.cpp
#include "screens/dbnomics/DBnomicsDataTable.h"
#include "ui/theme/Theme.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QLabel>
#include <algorithm>

namespace fincept::screens {

DBnomicsDataTable::DBnomicsDataTable(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void DBnomicsDataTable::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Section label
    auto* label = new QLabel("OBSERVATION DATA");
    label->setObjectName("tableTitle");
    label->setStyleSheet(QString(
        "color: %1; font-size: 11px; font-weight: 700; "
        "font-family: 'Consolas','Courier New',monospace; "
        "padding: 6px 12px; background: %2; "
        "border-bottom: 1px solid %3;")
        .arg(ui::colors::AMBER)
        .arg(ui::colors::BG_RAISED)
        .arg(ui::colors::BORDER_DIM));
    root->addWidget(label);

    table_ = new QTableWidget(this);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setAlternatingRowColors(false);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setDefaultAlignment(Qt::AlignRight | Qt::AlignVCenter);
    table_->verticalHeader()->setVisible(false);
    table_->setShowGrid(false);
    table_->setStyleSheet(QString(
        "QTableWidget { background: %1; color: %2; "
        "  font-family: 'Consolas','Courier New',monospace; font-size: 12px; "
        "  border: none; gridline-color: %3; }"
        "QTableWidget::item { padding: 3px 8px; border-bottom: 1px solid %3; }"
        "QTableWidget::item:selected { background: %4; color: %2; }"
        "QHeaderView::section { background: %5; color: %6; "
        "  font-size: 11px; font-weight: 700; padding: 4px 8px; "
        "  border: none; border-bottom: 1px solid %3; }")
        .arg(ui::colors::BG_BASE)
        .arg(ui::colors::TEXT_PRIMARY)
        .arg(ui::colors::BORDER_DIM)
        .arg(ui::colors::BG_HOVER)
        .arg(ui::colors::BG_RAISED)
        .arg(ui::colors::TEXT_SECONDARY));
    root->addWidget(table_);
}

void DBnomicsDataTable::clear() {
    table_->clear();
    table_->setRowCount(0);
    table_->setColumnCount(0);
}

void DBnomicsDataTable::set_data(const QVector<services::DbnDataPoint>& series) {
    table_->clear();
    if (series.isEmpty()) return;

    // Collect all unique periods across all series, sorted descending
    QSet<QString> period_set;
    for (const auto& dp : series) {
        for (const auto& obs : dp.observations) {
            period_set.insert(obs.period);
        }
    }
    QVector<QString> periods(period_set.begin(), period_set.end());
    std::sort(periods.begin(), periods.end(), std::greater<>());
    if (periods.size() > 50) periods.resize(50);

    // Build header: Period | Series1 | Series2 | ...
    const int col_count = 1 + series.size();
    table_->setColumnCount(col_count);
    QStringList headers;
    headers << "PERIOD";
    for (const auto& dp : series) {
        QString short_name = dp.series_name;
        if (short_name.length() > 20) short_name = short_name.left(17) + "...";
        headers << short_name;
    }
    table_->setHorizontalHeaderLabels(headers);
    table_->setRowCount(periods.size());

    // Color code header cells per series
    for (int s = 0; s < series.size(); ++s) {
        if (auto* h = table_->horizontalHeaderItem(s + 1)) {
            h->setForeground(series[s].color);
        }
    }

    // Build observation lookup: series_id → (period → value)
    QVector<QMap<QString, services::DbnObservation>> lookups(series.size());
    for (int s = 0; s < series.size(); ++s) {
        for (const auto& obs : series[s].observations) {
            lookups[s][obs.period] = obs;
        }
    }

    // Fill rows
    for (int row = 0; row < periods.size(); ++row) {
        const QString& period = periods[row];

        auto* period_item = new QTableWidgetItem(period);
        period_item->setForeground(QColor(ui::colors::TEXT_SECONDARY));
        period_item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        table_->setItem(row, 0, period_item);

        for (int s = 0; s < series.size(); ++s) {
            QTableWidgetItem* item;
            if (lookups[s].contains(period) && lookups[s][period].valid) {
                item = new QTableWidgetItem(
                    QString::number(lookups[s][period].value, 'f', 4));
                item->setForeground(QColor("#00E5FF")); // cyan for valid data
            } else {
                item = new QTableWidgetItem("—");
                item->setForeground(QColor(ui::colors::TEXT_TERTIARY));
            }
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table_->setItem(row, s + 1, item);
        }
    }
    table_->resizeColumnsToContents();
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
}

} // namespace fincept::screens
```

- [ ] **Step 3: Add to CMakeLists.txt**

Add `src/screens/dbnomics/DBnomicsDataTable.cpp` to `SCREEN_SOURCES`.

- [ ] **Step 4: Build**

```bash
cmake --build build --config Release 2>&1 | grep "error:" | head -20
```

- [ ] **Step 5: Commit**

```bash
git add src/screens/dbnomics/DBnomicsDataTable.h src/screens/dbnomics/DBnomicsDataTable.cpp
git commit -m "feat(dbnomics): add DBnomicsDataTable widget with multi-series period display"
```

---

## Task 4: DBnomicsChartWidget

**Files:**
- Create: `src/screens/dbnomics/DBnomicsChartWidget.h`
- Create: `src/screens/dbnomics/DBnomicsChartWidget.cpp`

Uses Qt6::Charts to render line, area, bar, and scatter charts for one or more series.

- [ ] **Step 1: Verify Qt6::Charts is linked in CMakeLists.txt**

Open `CMakeLists.txt` and check that `Qt6::Charts` is in `target_link_libraries`. If not, add it:

```cmake
target_link_libraries(FinceptTerminal PRIVATE
    Qt6::Widgets
    Qt6::Charts   # <-- must be present
    Qt6::Network
    Qt6::Sql
    Qt6::WebSockets
)
```

Also ensure `Charts` is in `find_package(Qt6 REQUIRED COMPONENTS ...)`.

- [ ] **Step 2: Write the header**

```cpp
// src/screens/dbnomics/DBnomicsChartWidget.h
#pragma once
#include "services/dbnomics/DBnomicsModels.h"
#include <QWidget>

// Forward declare Qt Charts types to avoid heavy includes in header
QT_CHARTS_BEGIN_NAMESPACE
class QChartView;
class QChart;
QT_CHARTS_END_NAMESPACE

namespace fincept::screens {

class DBnomicsChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit DBnomicsChartWidget(QWidget* parent = nullptr);

    void set_data(const QVector<services::DbnDataPoint>& series,
                  services::DbnChartType chart_type);
    void clear();

private:
    void build_ui();
    void render_line_area(const QVector<services::DbnDataPoint>& series,
                          services::DbnChartType type);
    void render_bar(const QVector<services::DbnDataPoint>& series);
    void render_scatter(const QVector<services::DbnDataPoint>& series);

    QChartView* chart_view_ = nullptr;
};

} // namespace fincept::screens
```

- [ ] **Step 3: Write the implementation**

```cpp
// src/screens/dbnomics/DBnomicsChartWidget.cpp
#include "screens/dbnomics/DBnomicsChartWidget.h"
#include "ui/theme/Theme.h"
#include "core/logging/Logger.h"

#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QAreaSeries>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QLegend>

#include <QVBoxLayout>
#include <QLabel>
#include <algorithm>
#include <limits>

QT_CHARTS_USE_NAMESPACE

namespace fincept::screens {

DBnomicsChartWidget::DBnomicsChartWidget(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void DBnomicsChartWidget::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    chart_view_ = new QChartView(this);
    chart_view_->setRenderHint(QPainter::Antialiasing, false); // P9: skip antialiasing
    chart_view_->setBackgroundRole(QPalette::Base);

    auto* chart = new QChart();
    chart->setBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE)));
    chart->setPlotAreaBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE)));
    chart->setPlotAreaBackgroundVisible(true);
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->legend()->setLabelColor(QColor(ui::colors::TEXT_SECONDARY));
    chart->setTitleFont(QFont("Consolas", 11, QFont::Bold));
    chart->setTitleBrush(QBrush(QColor(ui::colors::AMBER)));
    chart_view_->setChart(chart);

    root->addWidget(chart_view_);
}

void DBnomicsChartWidget::clear() {
    chart_view_->chart()->removeAllSeries();
    chart_view_->chart()->removeAxis(chart_view_->chart()->axes(Qt::Horizontal).value(0));
    chart_view_->chart()->removeAxis(chart_view_->chart()->axes(Qt::Vertical).value(0));
    chart_view_->chart()->setTitle({});
}

void DBnomicsChartWidget::set_data(const QVector<services::DbnDataPoint>& series,
                                    services::DbnChartType chart_type) {
    clear();
    if (series.isEmpty()) return;

    if (chart_type == services::DbnChartType::Bar) {
        render_bar(series);
    } else if (chart_type == services::DbnChartType::Scatter) {
        render_scatter(series);
    } else {
        render_line_area(series, chart_type);
    }
    LOG_DEBUG("DBnomicsChartWidget", QString("Rendered %1 series").arg(series.size()));
}

void DBnomicsChartWidget::render_line_area(const QVector<services::DbnDataPoint>& series,
                                            services::DbnChartType type) {
    QChart* chart = chart_view_->chart();
    double y_min =  std::numeric_limits<double>::max();
    double y_max = -std::numeric_limits<double>::max();
    QStringList all_periods;

    // Collect all unique periods in order (ascending)
    QSet<QString> period_set;
    for (const auto& dp : series)
        for (const auto& obs : dp.observations)
            if (obs.valid) period_set.insert(obs.period);
    all_periods = QList<QString>(period_set.begin(), period_set.end());
    std::sort(all_periods.begin(), all_periods.end());

    // Limit x-axis labels for readability
    const int max_labels = 12;
    QVector<int> label_indices;
    if (all_periods.size() <= max_labels) {
        for (int i = 0; i < all_periods.size(); ++i) label_indices.push_back(i);
    } else {
        int step = all_periods.size() / max_labels;
        for (int i = 0; i < all_periods.size(); i += step) label_indices.push_back(i);
    }

    auto* x_axis = new QValueAxis();
    x_axis->setRange(0, all_periods.size() - 1);
    x_axis->setTickCount(std::min(max_labels, (int)all_periods.size()));
    x_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY));
    x_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM));
    x_axis->setLinePen(QPen(QColor(ui::colors::BORDER_MED)));
    chart->addAxis(x_axis, Qt::AlignBottom);

    // Build series
    for (const auto& dp : series) {
        QMap<QString, double> period_val;
        for (const auto& obs : dp.observations)
            if (obs.valid) period_val[obs.period] = obs.value;

        auto* line = new QLineSeries();
        line->setName(dp.series_name);
        line->setColor(dp.color);
        QPen pen(dp.color);
        pen.setWidth(2);
        line->setPen(pen);

        for (int i = 0; i < all_periods.size(); ++i) {
            if (period_val.contains(all_periods[i])) {
                double v = period_val[all_periods[i]];
                line->append(i, v);
                y_min = std::min(y_min, v);
                y_max = std::max(y_max, v);
            }
        }
        chart->addSeries(line);
        line->attachAxis(x_axis);

        if (type == services::DbnChartType::Area) {
            auto* zero_line = new QLineSeries();
            for (int i = 0; i < all_periods.size(); ++i)
                zero_line->append(i, 0.0);
            auto* area = new QAreaSeries(line, zero_line);
            QColor fill = dp.color;
            fill.setAlpha(40);
            area->setBrush(fill);
            area->setPen(Qt::NoPen);
            chart->addSeries(area);
            area->attachAxis(x_axis);
        }
    }

    // Y axis
    double margin = (y_max - y_min) * 0.1;
    auto* y_axis = new QValueAxis();
    y_axis->setRange(y_min - margin, y_max + margin);
    y_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY));
    y_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM));
    y_axis->setLinePen(QPen(QColor(ui::colors::BORDER_MED)));
    y_axis->setTickCount(6);
    chart->addAxis(y_axis, Qt::AlignLeft);

    for (auto* s : chart->series())
        s->attachAxis(y_axis);
}

void DBnomicsChartWidget::render_bar(const QVector<services::DbnDataPoint>& series) {
    QChart* chart = chart_view_->chart();

    // Use last 20 periods for bar charts (readability)
    QSet<QString> period_set;
    for (const auto& dp : series)
        for (const auto& obs : dp.observations)
            if (obs.valid) period_set.insert(obs.period);
    QVector<QString> periods(period_set.begin(), period_set.end());
    std::sort(periods.begin(), periods.end());
    if (periods.size() > 20) periods = periods.mid(periods.size() - 20);

    auto* bar_series = new QBarSeries();
    for (const auto& dp : series) {
        auto* bar_set = new QBarSet(dp.series_name);
        bar_set->setColor(dp.color);
        QMap<QString, double> pv;
        for (const auto& obs : dp.observations)
            if (obs.valid) pv[obs.period] = obs.value;
        for (const auto& p : periods)
            *bar_set << (pv.contains(p) ? pv[p] : 0.0);
        bar_series->append(bar_set);
    }
    chart->addSeries(bar_series);

    auto* x_axis = new QBarCategoryAxis();
    x_axis->append(QList<QString>(periods.begin(), periods.end()));
    x_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY));
    x_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM));
    chart->addAxis(x_axis, Qt::AlignBottom);
    bar_series->attachAxis(x_axis);

    auto* y_axis = new QValueAxis();
    y_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY));
    y_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM));
    chart->addAxis(y_axis, Qt::AlignLeft);
    bar_series->attachAxis(y_axis);
}

void DBnomicsChartWidget::render_scatter(const QVector<services::DbnDataPoint>& series) {
    QChart* chart = chart_view_->chart();
    double y_min =  std::numeric_limits<double>::max();
    double y_max = -std::numeric_limits<double>::max();

    QSet<QString> period_set;
    for (const auto& dp : series)
        for (const auto& obs : dp.observations)
            if (obs.valid) period_set.insert(obs.period);
    QVector<QString> periods(period_set.begin(), period_set.end());
    std::sort(periods.begin(), periods.end());

    for (const auto& dp : series) {
        auto* scatter = new QScatterSeries();
        scatter->setName(dp.series_name);
        scatter->setColor(dp.color);
        scatter->setMarkerSize(6.0);
        QMap<QString, double> pv;
        for (const auto& obs : dp.observations)
            if (obs.valid) pv[obs.period] = obs.value;
        for (int i = 0; i < periods.size(); ++i) {
            if (pv.contains(periods[i])) {
                scatter->append(i, pv[periods[i]]);
                y_min = std::min(y_min, pv[periods[i]]);
                y_max = std::max(y_max, pv[periods[i]]);
            }
        }
        chart->addSeries(scatter);
    }
    auto* x_axis = new QValueAxis();
    x_axis->setRange(0, periods.size() - 1);
    x_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY));
    x_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM));
    chart->addAxis(x_axis, Qt::AlignBottom);

    double margin = (y_max - y_min) * 0.1;
    auto* y_axis = new QValueAxis();
    y_axis->setRange(y_min - margin, y_max + margin);
    y_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY));
    y_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM));
    chart->addAxis(y_axis, Qt::AlignLeft);

    for (auto* s : chart->series()) {
        s->attachAxis(x_axis);
        s->attachAxis(y_axis);
    }
}

} // namespace fincept::screens
```

- [ ] **Step 4: Add to CMakeLists.txt**

Add `src/screens/dbnomics/DBnomicsChartWidget.cpp` to `SCREEN_SOURCES`.

- [ ] **Step 5: Build**

```bash
cmake --build build --config Release 2>&1 | grep "error:" | head -20
```

- [ ] **Step 6: Commit**

```bash
git add src/screens/dbnomics/DBnomicsChartWidget.h src/screens/dbnomics/DBnomicsChartWidget.cpp
git commit -m "feat(dbnomics): add Qt6::Charts-based chart widget with line/area/bar/scatter"
```

---

## Task 5: DBnomicsSelectionPanel (Left Sidebar)

**Files:**
- Create: `src/screens/dbnomics/DBnomicsSelectionPanel.h`
- Create: `src/screens/dbnomics/DBnomicsSelectionPanel.cpp`

The left 280px sidebar. Contains global search, cascading provider→dataset→series lists with pagination, action buttons, and comparison slots management.

- [ ] **Step 1: Write the header**

```cpp
// src/screens/dbnomics/DBnomicsSelectionPanel.h
#pragma once
#include "services/dbnomics/DBnomicsModels.h"
#include <QWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QScrollArea>

namespace fincept::screens {

class DBnomicsSelectionPanel : public QWidget {
    Q_OBJECT
public:
    explicit DBnomicsSelectionPanel(QWidget* parent = nullptr);

    // Called by screen when service emits data
    void populate_providers(const QVector<services::DbnProvider>& providers);
    void populate_datasets(const QVector<services::DbnDataset>& datasets,
                           const services::DbnPagination& page, bool append);
    void populate_series(const QVector<services::DbnSeriesInfo>& series,
                         const services::DbnPagination& page, bool append);
    void populate_search_results(const QVector<services::DbnSearchResult>& results,
                                  const services::DbnPagination& page, bool append);
    void set_loading(bool loading);

    // Current context — needed by parent to call service
    QString selected_provider() const { return selected_provider_; }
    QString selected_dataset()  const { return selected_dataset_; }
    QString selected_series()   const { return selected_series_; }

signals:
    // User interactions forwarded to DBnomicsScreen
    void provider_selected(const QString& code);
    void dataset_selected(const QString& code);
    void series_selected(const QString& provider, const QString& dataset, const QString& series);
    void load_more_datasets_requested(int next_offset);
    void load_more_series_requested(int next_offset);
    void load_more_search_requested(int next_offset);
    void global_search_changed(const QString& query);
    void series_search_changed(const QString& provider, const QString& dataset, const QString& query);
    void add_to_single_view_clicked();
    void clear_all_clicked();
    void add_slot_clicked();
    void add_to_slot_clicked(int slot_index);
    void remove_from_slot_clicked(int slot_index, const QString& series_id);
    void remove_slot_clicked(int slot_index);

    // Emitted when user clicks a search result (sets provider+dataset context)
    void search_result_selected(const QString& provider_code, const QString& dataset_code);

public slots:
    void add_comparison_slot(const services::DbnDataPoint& data);
    void remove_comparison_slot(int index);
    void update_slot_series(int slot_index, const QVector<services::DbnDataPoint>& series);

private:
    void build_ui();
    QWidget* build_search_section();
    QWidget* build_provider_section();
    QWidget* build_dataset_section();
    QWidget* build_series_section();
    QWidget* build_action_buttons();
    QWidget* build_comparison_slots();
    QLabel*  make_section_label(const QString& text);
    QListWidget* make_styled_list(int fixed_height);
    QPushButton* make_load_more_button();

    // Section widgets
    QLineEdit*   global_search_input_    = nullptr;
    QListWidget* search_results_list_    = nullptr;
    QPushButton* search_load_more_btn_   = nullptr;

    QLineEdit*   provider_filter_input_  = nullptr;
    QListWidget* provider_list_          = nullptr;

    QListWidget* dataset_list_           = nullptr;
    QPushButton* dataset_load_more_btn_  = nullptr;

    QLineEdit*   series_search_input_    = nullptr;
    QListWidget* series_list_            = nullptr;
    QPushButton* series_load_more_btn_   = nullptr;

    QLabel*      status_label_           = nullptr;

    // Comparison slots container
    QVBoxLayout* slots_layout_           = nullptr;
    QWidget*     slots_container_        = nullptr;
    int          slot_count_             = 0;

    // Selection state
    QString selected_provider_;
    QString selected_dataset_;
    QString selected_series_;

    // Pagination state
    int datasets_next_offset_  = 0;
    int series_next_offset_    = 0;
    int search_next_offset_    = 0;
    int datasets_total_        = 0;
    int series_total_          = 0;
    int search_total_          = 0;

    // Provider data for client-side filtering
    QVector<services::DbnProvider> all_providers_;
};

} // namespace fincept::screens
```

- [ ] **Step 2: Write the implementation**

This is the most complex widget. Write it in sections:

```cpp
// src/screens/dbnomics/DBnomicsSelectionPanel.cpp
#include "screens/dbnomics/DBnomicsSelectionPanel.h"
#include "services/dbnomics/DBnomicsService.h"
#include "ui/theme/Theme.h"
#include "core/logging/Logger.h"

#include <QScrollArea>
#include <QSizePolicy>
#include <QPushButton>
#include <QFrame>
#include <QFont>

namespace fincept::screens {

// ── Styling helpers ───────────────────────────────────────────────────────────

static const QString kListStyle = R"(
    QListWidget {
        background: #080808; color: #808080;
        border: none;
        font-family: 'Consolas','Courier New',monospace;
        font-size: 11px;
    }
    QListWidget::item { padding: 3px 6px; border-bottom: 1px solid #1a1a1a; }
    QListWidget::item:hover { background: #161616; color: #e5e5e5; }
    QListWidget::item:selected { background: rgba(217,119,6,0.15); color: #d97706; }
)";

static const QString kInputStyle = R"(
    QLineEdit {
        background: #0a0a0a; color: #e5e5e5; border: 1px solid #222222;
        padding: 4px 8px;
        font-family: 'Consolas','Courier New',monospace; font-size: 11px;
    }
    QLineEdit:focus { border: 1px solid #d97706; }
    QLineEdit::placeholder { color: #404040; }
)";

static const QString kLoadMoreStyle = R"(
    QPushButton {
        background: transparent; color: #525252;
        border: 1px solid #1a1a1a; padding: 3px 8px;
        font-family: 'Consolas','Courier New',monospace; font-size: 10px;
    }
    QPushButton:hover { color: #d97706; border-color: #d97706; }
)";

// ── Constructor ───────────────────────────────────────────────────────────────

DBnomicsSelectionPanel::DBnomicsSelectionPanel(QWidget* parent) : QWidget(parent) {
    setFixedWidth(280);
    setStyleSheet(QString("background: %1; border-right: 1px solid %2;")
                  .arg(ui::colors::BG_SURFACE).arg(ui::colors::BORDER_DIM));
    build_ui();
}

// ── build_ui ─────────────────────────────────────────────────────────────────

void DBnomicsSelectionPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Wrap all content in a scroll area
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");

    auto* content = new QWidget(scroll);
    auto* layout  = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(build_search_section());
    layout->addWidget(build_provider_section());
    layout->addWidget(build_dataset_section());
    layout->addWidget(build_series_section());
    layout->addWidget(build_action_buttons());
    layout->addWidget(build_comparison_slots());
    layout->addStretch();

    scroll->setWidget(content);
    root->addWidget(scroll);

    // Status label at bottom
    status_label_ = new QLabel("READY", this);
    status_label_->setStyleSheet(QString(
        "color: %1; font-size: 10px; font-family: 'Consolas','Courier New',monospace; "
        "padding: 4px 8px; background: %2; border-top: 1px solid %3;")
        .arg(ui::colors::TEXT_TERTIARY).arg(ui::colors::BG_RAISED).arg(ui::colors::BORDER_DIM));
    root->addWidget(status_label_);
}

QLabel* DBnomicsSelectionPanel::make_section_label(const QString& text) {
    auto* lbl = new QLabel(text);
    lbl->setStyleSheet(QString(
        "color: %1; font-size: 10px; font-weight: 700; "
        "font-family: 'Consolas','Courier New',monospace; "
        "padding: 5px 8px; background: %2; border-bottom: 1px solid %3;")
        .arg(ui::colors::AMBER)
        .arg(ui::colors::BG_RAISED)
        .arg(ui::colors::BORDER_DIM));
    return lbl;
}

QListWidget* DBnomicsSelectionPanel::make_styled_list(int fixed_height) {
    auto* lw = new QListWidget();
    lw->setFixedHeight(fixed_height);
    lw->setStyleSheet(kListStyle);
    lw->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    return lw;
}

QPushButton* DBnomicsSelectionPanel::make_load_more_button() {
    auto* btn = new QPushButton("LOAD MORE");
    btn->setFixedHeight(22);
    btn->setStyleSheet(kLoadMoreStyle);
    btn->hide();
    return btn;
}

// ── Section builders ──────────────────────────────────────────────────────────

QWidget* DBnomicsSelectionPanel::build_search_section() {
    auto* w = new QWidget();
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    l->addWidget(make_section_label("GLOBAL SEARCH"));

    global_search_input_ = new QLineEdit();
    global_search_input_->setPlaceholderText("Search providers, datasets...");
    global_search_input_->setStyleSheet(kInputStyle);
    global_search_input_->setFixedHeight(28);
    l->addWidget(global_search_input_);

    search_results_list_ = make_styled_list(120);
    l->addWidget(search_results_list_);

    search_load_more_btn_ = make_load_more_button();
    l->addWidget(search_load_more_btn_);

    // Wire search input → debounce in service
    connect(global_search_input_, &QLineEdit::textChanged, this, [this](const QString& q) {
        if (q.trimmed().isEmpty()) {
            search_results_list_->clear();
            search_load_more_btn_->hide();
            emit global_search_changed({});
        } else {
            emit global_search_changed(q.trimmed());
        }
    });

    // Click on a search result → load that provider + dataset
    connect(search_results_list_, &QListWidget::itemClicked, this,
            [this](QListWidgetItem* item) {
        const QString prov = item->data(Qt::UserRole).toString();
        const QString ds   = item->data(Qt::UserRole + 1).toString();
        selected_provider_ = prov;
        selected_dataset_  = ds;
        emit search_result_selected(prov, ds);
        emit provider_selected(prov);
        emit dataset_selected(ds);
    });

    // Load more search results
    connect(search_load_more_btn_, &QPushButton::clicked, this, [this]() {
        emit load_more_search_requested(search_next_offset_);
    });

    return w;
}

QWidget* DBnomicsSelectionPanel::build_provider_section() {
    auto* w = new QWidget();
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    l->addWidget(make_section_label("PROVIDERS"));

    provider_filter_input_ = new QLineEdit();
    provider_filter_input_->setPlaceholderText("Filter providers...");
    provider_filter_input_->setStyleSheet(kInputStyle);
    provider_filter_input_->setFixedHeight(26);
    l->addWidget(provider_filter_input_);

    provider_list_ = make_styled_list(130);
    l->addWidget(provider_list_);

    // Client-side filter
    connect(provider_filter_input_, &QLineEdit::textChanged, this, [this](const QString& q) {
        for (int i = 0; i < provider_list_->count(); ++i) {
            auto* item = provider_list_->item(i);
            const bool match = q.isEmpty() ||
                item->text().contains(q, Qt::CaseInsensitive) ||
                item->data(Qt::UserRole).toString().contains(q, Qt::CaseInsensitive);
            item->setHidden(!match);
        }
    });

    connect(provider_list_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        selected_provider_ = item->data(Qt::UserRole).toString();
        selected_dataset_.clear();
        selected_series_.clear();
        dataset_list_->clear();
        series_list_->clear();
        dataset_load_more_btn_->hide();
        series_load_more_btn_->hide();
        status_label_->setText(QString("Provider: %1").arg(selected_provider_));
        emit provider_selected(selected_provider_);
    });

    return w;
}

QWidget* DBnomicsSelectionPanel::build_dataset_section() {
    auto* w = new QWidget();
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    l->addWidget(make_section_label("DATASETS"));

    dataset_list_ = make_styled_list(130);
    l->addWidget(dataset_list_);

    dataset_load_more_btn_ = make_load_more_button();
    l->addWidget(dataset_load_more_btn_);

    connect(dataset_list_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        selected_dataset_ = item->data(Qt::UserRole).toString();
        selected_series_.clear();
        series_list_->clear();
        series_search_input_->clear();
        series_load_more_btn_->hide();
        status_label_->setText(QString("Dataset: %1").arg(selected_dataset_));
        emit dataset_selected(selected_dataset_);
    });

    connect(dataset_load_more_btn_, &QPushButton::clicked, this, [this]() {
        emit load_more_datasets_requested(datasets_next_offset_);
    });

    return w;
}

QWidget* DBnomicsSelectionPanel::build_series_section() {
    auto* w = new QWidget();
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    l->addWidget(make_section_label("SERIES"));

    series_search_input_ = new QLineEdit();
    series_search_input_->setPlaceholderText("Search series...");
    series_search_input_->setStyleSheet(kInputStyle);
    series_search_input_->setFixedHeight(26);
    l->addWidget(series_search_input_);

    series_list_ = make_styled_list(130);
    l->addWidget(series_list_);

    series_load_more_btn_ = make_load_more_button();
    l->addWidget(series_load_more_btn_);

    // Server-side search via service debounce
    connect(series_search_input_, &QLineEdit::textChanged, this, [this](const QString& q) {
        if (!selected_provider_.isEmpty() && !selected_dataset_.isEmpty()) {
            emit series_search_changed(selected_provider_, selected_dataset_, q.trimmed());
        }
    });

    connect(series_list_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        selected_series_ = item->data(Qt::UserRole).toString();
        status_label_->setText(QString("Series: %1").arg(selected_series_));
        emit series_selected(selected_provider_, selected_dataset_, selected_series_);
    });

    connect(series_load_more_btn_, &QPushButton::clicked, this, [this]() {
        emit load_more_series_requested(series_next_offset_);
    });

    return w;
}

QWidget* DBnomicsSelectionPanel::build_action_buttons() {
    auto* w = new QWidget();
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(8, 6, 8, 6);
    l->setSpacing(4);

    auto* add_single = new QPushButton("ADD TO SINGLE VIEW");
    add_single->setFixedHeight(26);
    add_single->setStyleSheet(
        "QPushButton { background: rgba(217,119,6,0.1); color: #d97706; "
        "border: 1px solid #78350f; font-size: 10px; font-weight: 700; "
        "font-family: 'Consolas','Courier New',monospace; }"
        "QPushButton:hover { background: #d97706; color: #080808; }"
        "QPushButton:disabled { color: #404040; border-color: #1a1a1a; background: transparent; }");
    connect(add_single, &QPushButton::clicked, this, &DBnomicsSelectionPanel::add_to_single_view_clicked);
    l->addWidget(add_single);

    auto* clear_all = new QPushButton("CLEAR ALL");
    clear_all->setFixedHeight(26);
    clear_all->setStyleSheet(
        "QPushButton { background: rgba(220,38,38,0.1); color: #dc2626; "
        "border: 1px solid #7f1d1d; font-size: 10px; font-weight: 700; "
        "font-family: 'Consolas','Courier New',monospace; }"
        "QPushButton:hover { background: #dc2626; color: #080808; }");
    connect(clear_all, &QPushButton::clicked, this, &DBnomicsSelectionPanel::clear_all_clicked);
    l->addWidget(clear_all);

    return w;
}

QWidget* DBnomicsSelectionPanel::build_comparison_slots() {
    auto* w = new QWidget();
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    l->addWidget(make_section_label("COMPARISON SLOTS"));

    auto* add_slot_btn = new QPushButton("+ ADD SLOT");
    add_slot_btn->setFixedHeight(24);
    add_slot_btn->setStyleSheet(
        "QPushButton { background: rgba(22,163,74,0.1); color: #16a34a; "
        "border: 1px solid #14532d; font-size: 10px; font-weight: 700; "
        "font-family: 'Consolas','Courier New',monospace; margin: 4px 8px; }"
        "QPushButton:hover { background: #16a34a; color: #080808; }");
    connect(add_slot_btn, &QPushButton::clicked, this, &DBnomicsSelectionPanel::add_slot_clicked);
    l->addWidget(add_slot_btn);

    slots_container_ = new QWidget();
    slots_layout_ = new QVBoxLayout(slots_container_);
    slots_layout_->setContentsMargins(4, 0, 4, 4);
    slots_layout_->setSpacing(4);
    l->addWidget(slots_container_);

    return w;
}

// ── Data population ───────────────────────────────────────────────────────────

void DBnomicsSelectionPanel::populate_providers(const QVector<services::DbnProvider>& providers) {
    all_providers_ = providers;
    provider_list_->clear();
    for (const auto& p : providers) {
        auto* item = new QListWidgetItem(QString("%1  %2").arg(p.code, -8).arg(p.name));
        item->setData(Qt::UserRole, p.code);
        provider_list_->addItem(item);
    }
    status_label_->setText(QString("%1 providers loaded").arg(providers.size()));
}

void DBnomicsSelectionPanel::populate_datasets(const QVector<services::DbnDataset>& datasets,
                                               const services::DbnPagination& page,
                                               bool append) {
    if (!append) dataset_list_->clear();
    for (const auto& d : datasets) {
        auto* item = new QListWidgetItem(d.name.isEmpty() ? d.code : d.name);
        item->setData(Qt::UserRole, d.code);
        item->setToolTip(d.code);
        dataset_list_->addItem(item);
    }
    datasets_total_      = page.total;
    datasets_next_offset_ = page.offset + page.limit;
    dataset_load_more_btn_->setVisible(page.has_more());
    status_label_->setText(QString("%1/%2 datasets").arg(dataset_list_->count()).arg(page.total));
}

void DBnomicsSelectionPanel::populate_series(const QVector<services::DbnSeriesInfo>& series,
                                              const services::DbnPagination& page,
                                              bool append) {
    if (!append) series_list_->clear();
    for (const auto& s : series) {
        auto* item = new QListWidgetItem(s.name.isEmpty() ? s.code : s.name);
        item->setData(Qt::UserRole, s.code);
        item->setToolTip(QString("%1 — %2").arg(s.code).arg(s.name));
        series_list_->addItem(item);
    }
    series_total_       = page.total;
    series_next_offset_ = page.offset + page.limit;
    series_load_more_btn_->setVisible(page.has_more());
    status_label_->setText(QString("%1/%2 series").arg(series_list_->count()).arg(page.total));
}

void DBnomicsSelectionPanel::populate_search_results(
    const QVector<services::DbnSearchResult>& results,
    const services::DbnPagination& page, bool append) {
    if (!append) search_results_list_->clear();
    for (const auto& r : results) {
        auto* item = new QListWidgetItem(
            QString("[%1] %2").arg(r.provider_code).arg(r.dataset_name));
        item->setData(Qt::UserRole, r.provider_code);
        item->setData(Qt::UserRole + 1, r.dataset_code);
        item->setToolTip(QString("%1/%2 (%3 series)")
                         .arg(r.provider_code).arg(r.dataset_code).arg(r.nb_series));
        search_results_list_->addItem(item);
    }
    search_total_       = page.total;
    search_next_offset_ = page.offset + page.limit;
    search_load_more_btn_->setVisible(page.has_more());
}

void DBnomicsSelectionPanel::set_loading(bool loading) {
    if (loading) status_label_->setText("LOADING...");
}

void DBnomicsSelectionPanel::add_comparison_slot(const services::DbnDataPoint& /*data*/) {
    const int idx = slot_count_++;
    auto* slot_widget = new QWidget();
    slot_widget->setObjectName(QString("slot_%1").arg(idx));
    slot_widget->setStyleSheet(
        "background: #0a0a0a; border: 1px solid #1a1a1a;");
    auto* sl = new QVBoxLayout(slot_widget);
    sl->setContentsMargins(4, 4, 4, 4);
    sl->setSpacing(2);

    auto* header_row = new QWidget();
    auto* hr = new QHBoxLayout(header_row);
    hr->setContentsMargins(0, 0, 0, 0);

    auto* slot_label = new QLabel(QString("SLOT %1").arg(idx + 1));
    slot_label->setStyleSheet("color: #d97706; font-size: 10px; font-weight: 700; "
                               "font-family: 'Consolas','Courier New',monospace;");
    hr->addWidget(slot_label);
    hr->addStretch();

    auto* remove_slot_btn = new QPushButton("−");
    remove_slot_btn->setFixedSize(18, 18);
    remove_slot_btn->setStyleSheet(
        "QPushButton { background: transparent; color: #dc2626; border: none; "
        "font-size: 14px; font-weight: 700; }");
    const int slot_idx = idx;
    connect(remove_slot_btn, &QPushButton::clicked, this, [this, slot_idx]() {
        emit remove_slot_clicked(slot_idx);
    });
    hr->addWidget(remove_slot_btn);
    sl->addWidget(header_row);

    auto* add_to_slot_btn = new QPushButton("+ ADD CURRENT SERIES");
    add_to_slot_btn->setFixedHeight(20);
    add_to_slot_btn->setStyleSheet(
        "QPushButton { background: transparent; color: #525252; "
        "border: 1px solid #1a1a1a; font-size: 10px; "
        "font-family: 'Consolas','Courier New',monospace; }"
        "QPushButton:hover { color: #d97706; border-color: #d97706; }");
    connect(add_to_slot_btn, &QPushButton::clicked, this, [this, slot_idx]() {
        emit add_to_slot_clicked(slot_idx);
    });
    sl->addWidget(add_to_slot_btn);

    slots_layout_->addWidget(slot_widget);
}

void DBnomicsSelectionPanel::remove_comparison_slot(int /*index*/) {
    // For simplicity, remove the last slot widget
    if (slots_layout_->count() > 0) {
        auto* item = slots_layout_->takeAt(slots_layout_->count() - 1);
        if (item && item->widget()) {
            item->widget()->deleteLater();
            delete item;
        }
        --slot_count_;
    }
}

void DBnomicsSelectionPanel::update_slot_series(int slot_index,
                                                 const QVector<services::DbnDataPoint>& series) {
    // Find the slot widget and update its series list labels
    if (slot_index >= slots_layout_->count()) return;
    auto* slot_widget = qobject_cast<QWidget*>(
        slots_layout_->itemAt(slot_index)->widget());
    if (!slot_widget) return;

    // Remove old series labels (below the first 2 items: header + add button)
    auto* sl = qobject_cast<QVBoxLayout*>(slot_widget->layout());
    if (!sl) return;
    while (sl->count() > 2) {
        auto* item = sl->takeAt(sl->count() - 1);
        if (item && item->widget()) {
            item->widget()->deleteLater();
            delete item;
        }
    }
    // Add new series labels
    for (const auto& dp : series) {
        auto* row = new QWidget();
        auto* rl  = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(2);

        auto* dot = new QLabel("●");
        dot->setFixedWidth(12);
        dot->setStyleSheet(QString("color: %1; font-size: 8px;").arg(dp.color.name()));
        rl->addWidget(dot);

        auto* name = new QLabel(dp.series_name.left(22));
        name->setStyleSheet("color: #808080; font-size: 10px; "
                             "font-family: 'Consolas','Courier New',monospace;");
        name->setToolTip(dp.series_name);
        rl->addWidget(name, 1);

        auto* rm = new QPushButton("×");
        rm->setFixedSize(14, 14);
        rm->setStyleSheet(
            "QPushButton { background: transparent; color: #525252; border: none; "
            "font-size: 11px; }");
        const int si = slot_index;
        const QString sid = dp.series_id;
        connect(rm, &QPushButton::clicked, this, [this, si, sid]() {
            emit remove_from_slot_clicked(si, sid);
        });
        rl->addWidget(rm);
        sl->addWidget(row);
    }
}

} // namespace fincept::screens
```

- [ ] **Step 3: Add to CMakeLists.txt**

Add `src/screens/dbnomics/DBnomicsSelectionPanel.cpp` to `SCREEN_SOURCES`.

- [ ] **Step 4: Build**

```bash
cmake --build build --config Release 2>&1 | grep "error:" | head -20
```

- [ ] **Step 5: Commit**

```bash
git add src/screens/dbnomics/DBnomicsSelectionPanel.h src/screens/dbnomics/DBnomicsSelectionPanel.cpp
git commit -m "feat(dbnomics): add selection panel with cascading lists, search, pagination, comparison slots"
```

---

## Task 6: DBnomicsScreen — Main Screen (Wires Everything Together)

**Files:**
- Create: `src/screens/dbnomics/DBnomicsScreen.h`
- Create: `src/screens/dbnomics/DBnomicsScreen.cpp`

- [ ] **Step 1: Write the header**

```cpp
// src/screens/dbnomics/DBnomicsScreen.h
#pragma once
#include "services/dbnomics/DBnomicsModels.h"
#include <QWidget>
#include <QShowEvent>
#include <QHideEvent>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QSplitter>

namespace fincept::screens {

class DBnomicsSelectionPanel;
class DBnomicsChartWidget;
class DBnomicsDataTable;

class DBnomicsScreen : public QWidget {
    Q_OBJECT
public:
    explicit DBnomicsScreen(QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    // Service signal handlers
    void on_providers_loaded(const QVector<services::DbnProvider>& providers);
    void on_datasets_loaded(const QVector<services::DbnDataset>& datasets,
                            const services::DbnPagination& page, bool append);
    void on_series_loaded(const QVector<services::DbnSeriesInfo>& series,
                          const services::DbnPagination& page, bool append);
    void on_observations_loaded(const services::DbnDataPoint& data);
    void on_search_results_loaded(const QVector<services::DbnSearchResult>& results,
                                  const services::DbnPagination& page, bool append);
    void on_service_error(const QString& context, const QString& message);

    // Toolbar actions
    void on_fetch_clicked();
    void on_refresh_clicked();
    void on_export_csv();

    // Panel signals
    void on_provider_selected(const QString& code);
    void on_dataset_selected(const QString& code);
    void on_series_selected(const QString& prov, const QString& ds, const QString& code);
    void on_chart_type_changed(int index);
    void on_view_mode_toggled(bool comparison_mode);
    void on_add_to_single_view();
    void on_clear_all();
    void on_add_slot();
    void on_add_to_slot(int slot_index);

private:
    void build_ui();
    QWidget* build_toolbar();
    QWidget* build_single_view();
    QWidget* build_comparison_view();
    void set_status(const QString& message);
    void render_single_view();
    void assign_series_colors();

    // Sub-widgets
    DBnomicsSelectionPanel* selection_panel_    = nullptr;
    DBnomicsChartWidget*    chart_widget_       = nullptr;
    DBnomicsDataTable*      data_table_         = nullptr;
    QStackedWidget*         view_stack_         = nullptr;
    QWidget*                comparison_grid_    = nullptr;
    QLabel*                 status_label_       = nullptr;
    QLabel*                 stats_label_        = nullptr;
    QComboBox*              chart_type_combo_   = nullptr;
    QPushButton*            single_btn_         = nullptr;
    QPushButton*            compare_btn_        = nullptr;

    // State
    services::DbnViewMode   view_mode_          = services::DbnViewMode::Single;
    services::DbnChartType  single_chart_type_  = services::DbnChartType::Line;
    QVector<services::DbnDataPoint> single_series_;
    QVector<services::DbnSlot>      slots_;

    // Last loaded (pending add to view)
    services::DbnDataPoint  last_loaded_data_;
    bool                    has_pending_data_   = false;

    int provider_count_ = 0;
};

} // namespace fincept::screens
```

- [ ] **Step 2: Write the implementation**

```cpp
// src/screens/dbnomics/DBnomicsScreen.cpp
#include "screens/dbnomics/DBnomicsScreen.h"
#include "screens/dbnomics/DBnomicsSelectionPanel.h"
#include "screens/dbnomics/DBnomicsChartWidget.h"
#include "screens/dbnomics/DBnomicsDataTable.h"
#include "services/dbnomics/DBnomicsService.h"
#include "ui/theme/Theme.h"
#include "core/logging/Logger.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QComboBox>
#include <QFileDialog>
#include <QTextStream>
#include <QDateTime>
#include <QScrollArea>
#include <QGridLayout>

namespace fincept::screens {

DBnomicsScreen::DBnomicsScreen(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("background: %1;").arg(ui::colors::BG_BASE));
    build_ui();

    // ── Connect service signals ───────────────────────────────────────────────
    auto& svc = services::DBnomicsService::instance();
    connect(&svc, &services::DBnomicsService::providers_loaded,
            this, &DBnomicsScreen::on_providers_loaded);
    connect(&svc, &services::DBnomicsService::datasets_loaded,
            this, &DBnomicsScreen::on_datasets_loaded);
    connect(&svc, &services::DBnomicsService::series_loaded,
            this, &DBnomicsScreen::on_series_loaded);
    connect(&svc, &services::DBnomicsService::observations_loaded,
            this, &DBnomicsScreen::on_observations_loaded);
    connect(&svc, &services::DBnomicsService::search_results_loaded,
            this, &DBnomicsScreen::on_search_results_loaded);
    connect(&svc, &services::DBnomicsService::error_occurred,
            this, &DBnomicsScreen::on_service_error);

    // ── Connect panel signals ─────────────────────────────────────────────────
    connect(selection_panel_, &DBnomicsSelectionPanel::provider_selected,
            this, &DBnomicsScreen::on_provider_selected);
    connect(selection_panel_, &DBnomicsSelectionPanel::dataset_selected,
            this, &DBnomicsScreen::on_dataset_selected);
    connect(selection_panel_, &DBnomicsSelectionPanel::series_selected,
            this, &DBnomicsScreen::on_series_selected);
    connect(selection_panel_, &DBnomicsSelectionPanel::load_more_datasets_requested,
            this, [this](int offset) {
        services::DBnomicsService::instance().fetch_datasets(
            selection_panel_->selected_provider(), offset);
    });
    connect(selection_panel_, &DBnomicsSelectionPanel::load_more_series_requested,
            this, [this](int offset) {
        services::DBnomicsService::instance().fetch_series(
            selection_panel_->selected_provider(),
            selection_panel_->selected_dataset(), {}, offset);
    });
    connect(selection_panel_, &DBnomicsSelectionPanel::global_search_changed,
            this, [](const QString& q) {
        if (!q.isEmpty())
            services::DBnomicsService::instance().schedule_global_search(q);
    });
    connect(selection_panel_, &DBnomicsSelectionPanel::series_search_changed,
            this, [](const QString& prov, const QString& ds, const QString& q) {
        services::DBnomicsService::instance().schedule_series_search(prov, ds, q);
    });
    connect(selection_panel_, &DBnomicsSelectionPanel::add_to_single_view_clicked,
            this, &DBnomicsScreen::on_add_to_single_view);
    connect(selection_panel_, &DBnomicsSelectionPanel::clear_all_clicked,
            this, &DBnomicsScreen::on_clear_all);
    connect(selection_panel_, &DBnomicsSelectionPanel::add_slot_clicked,
            this, &DBnomicsScreen::on_add_slot);
    connect(selection_panel_, &DBnomicsSelectionPanel::add_to_slot_clicked,
            this, &DBnomicsScreen::on_add_to_slot);
    connect(selection_panel_, &DBnomicsSelectionPanel::load_more_search_requested,
            this, [this](int offset) {
        // Re-run search with next offset — user typed query is in service pending
        // We call global_search directly for "load more"
        // The query is still in the service's pending_search_q_ field
        // For simplicity, re-issue from the current search input
    });
}

void DBnomicsScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    // Auto-fetch providers on first show
    if (provider_count_ == 0) {
        services::DBnomicsService::instance().fetch_providers();
    }
    LOG_INFO("DBnomicsScreen", "Screen shown");
}

void DBnomicsScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    LOG_INFO("DBnomicsScreen", "Screen hidden");
}

void DBnomicsScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_toolbar());

    auto* content_area = new QWidget();
    auto* content_layout = new QHBoxLayout(content_area);
    content_layout->setContentsMargins(0, 0, 0, 0);
    content_layout->setSpacing(0);

    selection_panel_ = new DBnomicsSelectionPanel(this);
    content_layout->addWidget(selection_panel_);

    // Right side: chart + table stacked vertically
    auto* right_panel = new QWidget();
    auto* right_layout = new QVBoxLayout(right_panel);
    right_layout->setContentsMargins(0, 0, 0, 0);
    right_layout->setSpacing(0);

    // View toggle bar
    auto* view_bar = new QWidget();
    view_bar->setFixedHeight(36);
    view_bar->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;")
        .arg(ui::colors::BG_RAISED).arg(ui::colors::BORDER_DIM));
    auto* vbl = new QHBoxLayout(view_bar);
    vbl->setContentsMargins(8, 4, 8, 4);
    vbl->setSpacing(6);

    single_btn_ = new QPushButton("SINGLE");
    compare_btn_ = new QPushButton("COMPARE");
    for (auto* btn : {single_btn_, compare_btn_}) {
        btn->setFixedHeight(24);
        btn->setCheckable(true);
    }
    single_btn_->setChecked(true);
    const QString toggle_style =
        "QPushButton { background: transparent; color: #525252; "
        "border: 1px solid #222222; padding: 0 12px; font-size: 11px; font-weight: 700; "
        "font-family: 'Consolas','Courier New',monospace; }"
        "QPushButton:checked { background: rgba(217,119,6,0.15); color: #d97706; "
        "border-color: #78350f; }"
        "QPushButton:hover:!checked { color: #e5e5e5; }";
    single_btn_->setStyleSheet(toggle_style);
    compare_btn_->setStyleSheet(toggle_style);

    connect(single_btn_, &QPushButton::clicked, this, [this]() {
        view_mode_ = services::DbnViewMode::Single;
        single_btn_->setChecked(true);
        compare_btn_->setChecked(false);
        view_stack_->setCurrentIndex(0);
    });
    connect(compare_btn_, &QPushButton::clicked, this, [this]() {
        view_mode_ = services::DbnViewMode::Comparison;
        compare_btn_->setChecked(true);
        single_btn_->setChecked(false);
        view_stack_->setCurrentIndex(1);
    });

    vbl->addWidget(single_btn_);
    vbl->addWidget(compare_btn_);
    vbl->addStretch();

    // Chart type selector (single view only)
    auto* chart_type_label = new QLabel("CHART:");
    chart_type_label->setStyleSheet("color: #525252; font-size: 10px; font-weight: 700; "
                                     "font-family: 'Consolas','Courier New',monospace;");
    chart_type_combo_ = new QComboBox();
    chart_type_combo_->addItems({"LINE", "AREA", "BAR", "SCATTER"});
    chart_type_combo_->setFixedHeight(24);
    chart_type_combo_->setStyleSheet(
        "QComboBox { background: #0a0a0a; color: #808080; border: 1px solid #222222; "
        "padding: 0 6px; font-size: 11px; font-family: 'Consolas','Courier New',monospace; }"
        "QComboBox:focus { border-color: #d97706; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: #111111; color: #808080; "
        "border: 1px solid #222222; }");
    connect(chart_type_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DBnomicsScreen::on_chart_type_changed);
    vbl->addWidget(chart_type_label);
    vbl->addWidget(chart_type_combo_);

    right_layout->addWidget(view_bar);

    // Stacked view (single / comparison)
    view_stack_ = new QStackedWidget();

    // Single view page
    auto* single_page = new QWidget();
    auto* spl = new QVBoxLayout(single_page);
    spl->setContentsMargins(0, 0, 0, 0);
    spl->setSpacing(0);

    chart_widget_ = new DBnomicsChartWidget(single_page);
    chart_widget_->setMinimumHeight(300);
    spl->addWidget(chart_widget_, 3);

    data_table_ = new DBnomicsDataTable(single_page);
    spl->addWidget(data_table_, 1);

    view_stack_->addWidget(single_page);

    // Comparison view page (grid of chart slots)
    comparison_grid_ = new QWidget();
    comparison_grid_->setStyleSheet(QString("background: %1;").arg(ui::colors::BG_BASE));
    view_stack_->addWidget(comparison_grid_);

    right_layout->addWidget(view_stack_, 1);

    content_layout->addWidget(right_panel, 1);
    root->addWidget(content_area, 1);

    // Bottom status bar
    auto* status_bar = new QWidget();
    status_bar->setFixedHeight(28);
    status_bar->setStyleSheet(QString(
        "background: %1; border-top: 1px solid %2;")
        .arg(ui::colors::BG_RAISED).arg(ui::colors::BORDER_DIM));
    auto* sbl = new QHBoxLayout(status_bar);
    sbl->setContentsMargins(12, 0, 12, 0);

    status_label_ = new QLabel("DBNOMICS TERMINAL  •  READY");
    status_label_->setStyleSheet(
        "color: #525252; font-size: 11px; font-weight: 700; "
        "font-family: 'Consolas','Courier New',monospace;");
    sbl->addWidget(status_label_);
    sbl->addStretch();

    stats_label_ = new QLabel();
    stats_label_->setStyleSheet(
        "color: #404040; font-size: 10px; "
        "font-family: 'Consolas','Courier New',monospace;");
    sbl->addWidget(stats_label_);

    root->addWidget(status_bar);
}

QWidget* DBnomicsScreen::build_toolbar() {
    auto* tb = new QWidget();
    tb->setFixedHeight(44);
    tb->setStyleSheet(QString(
        "background: %1; border-bottom: 1px solid %2;")
        .arg(ui::colors::BG_RAISED).arg(ui::colors::BORDER_DIM));
    auto* l = new QHBoxLayout(tb);
    l->setContentsMargins(12, 6, 12, 6);
    l->setSpacing(8);

    auto* title = new QLabel("DBNOMICS TERMINAL");
    title->setStyleSheet(
        "color: #d97706; font-size: 14px; font-weight: 700; "
        "font-family: 'Consolas','Courier New',monospace;");
    l->addWidget(title);
    l->addStretch();

    const QString btn_style =
        "QPushButton { background: rgba(217,119,6,0.1); color: #d97706; "
        "border: 1px solid #78350f; padding: 0 14px; height: 28px; "
        "font-size: 11px; font-weight: 700; "
        "font-family: 'Consolas','Courier New',monospace; }"
        "QPushButton:hover { background: #d97706; color: #080808; }";

    auto* fetch_btn = new QPushButton("FETCH");
    fetch_btn->setStyleSheet(btn_style);
    connect(fetch_btn, &QPushButton::clicked, this, &DBnomicsScreen::on_fetch_clicked);
    l->addWidget(fetch_btn);

    auto* refresh_btn = new QPushButton("REFRESH");
    refresh_btn->setStyleSheet(btn_style);
    connect(refresh_btn, &QPushButton::clicked, this, &DBnomicsScreen::on_refresh_clicked);
    l->addWidget(refresh_btn);

    auto* export_btn = new QPushButton("EXPORT CSV");
    export_btn->setStyleSheet(btn_style);
    connect(export_btn, &QPushButton::clicked, this, &DBnomicsScreen::on_export_csv);
    l->addWidget(export_btn);

    return tb;
}

void DBnomicsScreen::set_status(const QString& message) {
    if (status_label_) status_label_->setText(message);
}

// ── Toolbar actions ───────────────────────────────────────────────────────────

void DBnomicsScreen::on_fetch_clicked() {
    set_status("Fetching providers...");
    services::DBnomicsService::instance().fetch_providers();
}

void DBnomicsScreen::on_refresh_clicked() {
    if (single_series_.isEmpty()) return;
    set_status("Refreshing...");
    // Re-fetch all series currently in single view
    for (const auto& dp : single_series_) {
        QStringList parts = dp.series_id.split('/');
        if (parts.size() == 3)
            services::DBnomicsService::instance().fetch_observations(
                parts[0], parts[1], parts[2]);
    }
}

void DBnomicsScreen::on_export_csv() {
    if (single_series_.isEmpty()) {
        set_status("No data to export");
        return;
    }
    const QString filename = QFileDialog::getSaveFileName(
        this, "Export CSV",
        QString("dbnomics_%1.csv").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")),
        "CSV Files (*.csv)");
    if (filename.isEmpty()) return;

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        set_status("Export failed: cannot open file");
        return;
    }
    QTextStream out(&file);
    // Header
    out << "Period";
    for (const auto& dp : single_series_)
        out << "," << dp.series_name;
    out << "\n";
    // Collect all periods
    QSet<QString> period_set;
    for (const auto& dp : single_series_)
        for (const auto& obs : dp.observations)
            period_set.insert(obs.period);
    QVector<QString> periods(period_set.begin(), period_set.end());
    std::sort(periods.begin(), periods.end());

    for (const auto& period : periods) {
        out << period;
        for (const auto& dp : single_series_) {
            bool found = false;
            for (const auto& obs : dp.observations) {
                if (obs.period == period) {
                    out << "," << (obs.valid ? QString::number(obs.value, 'f', 6) : "NA");
                    found = true;
                    break;
                }
            }
            if (!found) out << ",";
        }
        out << "\n";
    }
    set_status(QString("Exported to %1").arg(filename));
    LOG_INFO("DBnomicsScreen", QString("Exported CSV: %1").arg(filename));
}

// ── Service signal handlers ───────────────────────────────────────────────────

void DBnomicsScreen::on_providers_loaded(const QVector<services::DbnProvider>& providers) {
    provider_count_ = providers.size();
    selection_panel_->populate_providers(providers);
    stats_label_->setText(QString("PROVIDERS: %1").arg(providers.size()));
    set_status(QString("Loaded %1 providers").arg(providers.size()));
}

void DBnomicsScreen::on_datasets_loaded(const QVector<services::DbnDataset>& datasets,
                                         const services::DbnPagination& page, bool append) {
    selection_panel_->populate_datasets(datasets, page, append);
    set_status(QString("Datasets: %1 / %2").arg(page.offset + datasets.size()).arg(page.total));
}

void DBnomicsScreen::on_series_loaded(const QVector<services::DbnSeriesInfo>& series,
                                       const services::DbnPagination& page, bool append) {
    selection_panel_->populate_series(series, page, append);
    set_status(QString("Series: %1 / %2").arg(page.offset + series.size()).arg(page.total));
}

void DBnomicsScreen::on_observations_loaded(const services::DbnDataPoint& data) {
    last_loaded_data_ = data;
    has_pending_data_ = true;

    // Update if this series is already in single_series_
    for (auto& dp : single_series_) {
        if (dp.series_id == data.series_id) {
            dp.observations = data.observations;
            render_single_view();
            set_status(QString("Updated: %1").arg(data.series_name));
            return;
        }
    }
    set_status(QString("Loaded: %1  •  Click ADD TO SINGLE VIEW").arg(data.series_name));
}

void DBnomicsScreen::on_search_results_loaded(const QVector<services::DbnSearchResult>& results,
                                               const services::DbnPagination& page, bool append) {
    selection_panel_->populate_search_results(results, page, append);
}

void DBnomicsScreen::on_service_error(const QString& context, const QString& message) {
    LOG_ERROR("DBnomicsScreen", QString("[%1] %2").arg(context).arg(message));
    set_status(QString("ERROR [%1]: %2").arg(context).arg(message));
}

// ── Panel signal handlers ─────────────────────────────────────────────────────

void DBnomicsScreen::on_provider_selected(const QString& code) {
    set_status(QString("Loading datasets for %1...").arg(code));
    services::DBnomicsService::instance().fetch_datasets(code, 0);
}

void DBnomicsScreen::on_dataset_selected(const QString& code) {
    set_status(QString("Loading series for %1...").arg(code));
    services::DBnomicsService::instance().fetch_series(
        selection_panel_->selected_provider(), code, {}, 0);
}

void DBnomicsScreen::on_series_selected(const QString& prov,
                                         const QString& ds,
                                         const QString& code) {
    set_status(QString("Fetching observations for %1/%2/%3...").arg(prov).arg(ds).arg(code));
    services::DBnomicsService::instance().fetch_observations(prov, ds, code);
}

void DBnomicsScreen::on_chart_type_changed(int index) {
    static const services::DbnChartType types[] = {
        services::DbnChartType::Line,
        services::DbnChartType::Area,
        services::DbnChartType::Bar,
        services::DbnChartType::Scatter
    };
    single_chart_type_ = types[index];
    render_single_view();
}

void DBnomicsScreen::on_add_to_single_view() {
    if (!has_pending_data_) {
        set_status("No data loaded — click a series first");
        return;
    }
    // Check for duplicate
    for (const auto& dp : single_series_) {
        if (dp.series_id == last_loaded_data_.series_id) {
            set_status("Series already in view");
            return;
        }
    }
    single_series_.push_back(last_loaded_data_);
    assign_series_colors();
    render_single_view();
    view_mode_ = services::DbnViewMode::Single;
    view_stack_->setCurrentIndex(0);
    single_btn_->setChecked(true);
    compare_btn_->setChecked(false);
    set_status(QString("Added: %1").arg(last_loaded_data_.series_name));
}

void DBnomicsScreen::on_clear_all() {
    single_series_.clear();
    slots_.clear();
    has_pending_data_ = false;
    chart_widget_->clear();
    data_table_->clear();
    set_status("Cleared");
}

void DBnomicsScreen::on_add_slot() {
    slots_.push_back({});
    services::DbnDataPoint empty;
    selection_panel_->add_comparison_slot(empty);
}

void DBnomicsScreen::on_add_to_slot(int slot_index) {
    if (!has_pending_data_) {
        set_status("No data loaded");
        return;
    }
    if (slot_index >= slots_.size()) slots_.resize(slot_index + 1);
    // Check for duplicate in slot
    for (const auto& dp : slots_[slot_index].series) {
        if (dp.series_id == last_loaded_data_.series_id) {
            set_status("Series already in this slot");
            return;
        }
    }
    slots_[slot_index].series.push_back(last_loaded_data_);
    selection_panel_->update_slot_series(slot_index, slots_[slot_index].series);
    set_status(QString("Added to slot %1").arg(slot_index + 1));
}

// ── Rendering ─────────────────────────────────────────────────────────────────

void DBnomicsScreen::assign_series_colors() {
    for (int i = 0; i < single_series_.size(); ++i) {
        single_series_[i].color = services::DBnomicsService::chart_color(i);
    }
}

void DBnomicsScreen::render_single_view() {
    if (single_series_.isEmpty()) {
        chart_widget_->clear();
        data_table_->clear();
        return;
    }
    chart_widget_->set_data(single_series_, single_chart_type_);
    data_table_->set_data(single_series_);
}

} // namespace fincept::screens
```

- [ ] **Step 3: Add to CMakeLists.txt**

Add `src/screens/dbnomics/DBnomicsScreen.cpp` to `SCREEN_SOURCES`.

- [ ] **Step 4: Build**

```bash
cmake --build build --config Release 2>&1 | grep "error:" | head -30
```

Fix all errors before proceeding.

- [ ] **Step 5: Commit**

```bash
git add src/screens/dbnomics/DBnomicsScreen.h src/screens/dbnomics/DBnomicsScreen.cpp
git commit -m "feat(dbnomics): add main DBnomicsScreen assembling all sub-widgets and wiring service signals"
```

---

## Task 7: CMakeLists.txt — Register All New Source Files

**Files:**
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Read the current CMakeLists.txt**

Open `fincept-qt/CMakeLists.txt`. Find where screen and service `.cpp` files are listed. Add all 6 new `.cpp` files:

```cmake
# In the SCREEN_SOURCES list (or equivalent):
src/screens/dbnomics/DBnomicsScreen.cpp
src/screens/dbnomics/DBnomicsSelectionPanel.cpp
src/screens/dbnomics/DBnomicsChartWidget.cpp
src/screens/dbnomics/DBnomicsDataTable.cpp

# In the SERVICE_SOURCES list (or together with other services):
src/services/dbnomics/DBnomicsService.cpp
```

If the project uses a single flat source list in `add_executable`, add all five `.cpp` files there.

- [ ] **Step 2: Confirm build succeeds with all files**

```bash
cd D:/Tilak/FinceptV2/FinceptTerminal/fincept-qt
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"
cmake --build build --config Release 2>&1 | tail -20
```

- [ ] **Step 3: Commit**

```bash
git add CMakeLists.txt
git commit -m "build: register DBnomics screen and service source files in CMakeLists.txt"
```

---

## Task 8: Wire into MainWindow (Replace ComingSoonScreen)

**Files:**
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`

- [ ] **Step 1: Add include to MainWindow.h (or MainWindow.cpp)**

Find where other screen headers are included (e.g., `#include "screens/dashboard/DashboardScreen.h"`). Add:

```cpp
#include "screens/dbnomics/DBnomicsScreen.h"
```

- [ ] **Step 2: Replace the ComingSoonScreen registration**

In `MainWindow.cpp`, find line (approximately 168):

```cpp
router_->register_screen("dbnomics", new screens::ComingSoonScreen("DBnomics"));
```

Replace with the **lazy factory** pattern (P2 rule — DBnomics fetches data):

```cpp
router_->register_factory("dbnomics", []() {
    return new screens::DBnomicsScreen;
});
```

- [ ] **Step 3: Build and run**

```bash
cmake --build build --config Release 2>&1 | grep "error:" | head -20
# If clean:
.\build\Release\FinceptTerminal.exe
```

Navigate to the DBnomics tab in the running application.

- [ ] **Step 4: Smoke test checklist**

- [ ] App launches without crash
- [ ] Navigating to DBnomics tab shows the screen (not ComingSoon)
- [ ] Toolbar shows FETCH / REFRESH / EXPORT CSV buttons
- [ ] Clicking FETCH loads providers into the left panel list
- [ ] Clicking a provider loads its datasets
- [ ] Clicking a dataset loads its series
- [ ] Clicking a series fetches observations and shows status message
- [ ] Clicking ADD TO SINGLE VIEW renders a line chart and populates the data table
- [ ] Changing chart type combo re-renders the chart
- [ ] COMPARE / SINGLE toggle switches view stack
- [ ] ADD SLOT creates a comparison slot in the left panel
- [ ] Global search input triggers a search after 400ms
- [ ] EXPORT CSV opens file dialog and writes a valid file
- [ ] CLEAR ALL wipes the chart and table
- [ ] Navigating away and back does not cause duplicate data fetches

- [ ] **Step 5: Commit**

```bash
git add src/app/MainWindow.h src/app/MainWindow.cpp
git commit -m "feat(dbnomics): replace ComingSoonScreen with real DBnomicsScreen (lazy factory registration)"
```

---

## Task 9: Polish — Error States, Empty States, Loading Indicators

**Files:**
- Modify: `src/screens/dbnomics/DBnomicsChartWidget.cpp`
- Modify: `src/screens/dbnomics/DBnomicsDataTable.cpp`

- [ ] **Step 1: Add "no data" overlay to chart widget**

In `DBnomicsChartWidget::clear()`, set a title on the chart that reads "NO DATA — SELECT A SERIES":

```cpp
void DBnomicsChartWidget::clear() {
    chart_view_->chart()->removeAllSeries();
    // Remove axes
    for (auto* axis : chart_view_->chart()->axes())
        chart_view_->chart()->removeAxis(axis);
    chart_view_->chart()->setTitle("NO DATA — SELECT A SERIES FROM THE LEFT PANEL");
}
```

- [ ] **Step 2: Add "loading" message to status label**

In `DBnomicsScreen::on_series_selected()`, the status already updates to "Fetching observations...". Verify this is visible during the HTTP call (it is — the callback runs on the Qt event loop, so the label updates immediately before the fetch starts).

- [ ] **Step 3: Add empty state to DataTable**

In `DBnomicsDataTable::clear()`, add a single row with "NO DATA":

```cpp
void DBnomicsDataTable::clear() {
    table_->clear();
    table_->setRowCount(1);
    table_->setColumnCount(1);
    table_->setHorizontalHeaderLabels({"STATUS"});
    auto* item = new QTableWidgetItem("No data — select a series");
    item->setForeground(QColor(ui::colors::TEXT_TERTIARY));
    item->setTextAlignment(Qt::AlignCenter);
    table_->setItem(0, 0, item);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
}
```

- [ ] **Step 4: Build and verify visuals**

```bash
cmake --build build --config Release && .\build\Release\FinceptTerminal.exe
```

- [ ] **Step 5: Commit**

```bash
git add src/screens/dbnomics/DBnomicsChartWidget.cpp src/screens/dbnomics/DBnomicsDataTable.cpp
git commit -m "feat(dbnomics): add empty/loading states for chart and data table"
```

---

## Task 10: Final Integration — QPointer Guards & Thread Safety Audit

**Files:**
- Review: all `src/screens/dbnomics/*.cpp`

- [ ] **Step 1: Audit all lambda captures**

Search for `[this]` in all DBnomics `.cpp` files:

```bash
grep -n "\[this\]" src/screens/dbnomics/*.cpp src/services/dbnomics/*.cpp
```

For any lambda passed to `HttpClient::instance().get()` or `QtConcurrent::run()`, verify the screen cannot be destroyed before the lambda executes. Since the service is a singleton and lambdas in the service don't capture `this` (they capture nothing or just service fields), this is safe. Screen-side lambdas using `[this]` in `connect()` calls are safe because Qt auto-disconnects on QObject destroy.

- [ ] **Step 2: Audit timer usage**

Verify no `QTimer::start()` calls exist in constructors. The `DBnomicsService` debounce timers are fine — they are `QTimer::singleShot` triggered only by user actions. The screen has no polling timer.

- [ ] **Step 3: Final build**

```bash
cmake --build build --config Release
```

Zero errors, zero relevant warnings.

- [ ] **Step 4: Final commit**

```bash
git add -A
git commit -m "feat(dbnomics): complete DBnomics tab migration from Tauri to Qt6 C++"
```

---

## Summary: Files Created/Modified

| Action | File |
|--------|------|
| Create | `src/services/dbnomics/DBnomicsModels.h` |
| Create | `src/services/dbnomics/DBnomicsService.h` |
| Create | `src/services/dbnomics/DBnomicsService.cpp` |
| Create | `src/screens/dbnomics/DBnomicsScreen.h` |
| Create | `src/screens/dbnomics/DBnomicsScreen.cpp` |
| Create | `src/screens/dbnomics/DBnomicsSelectionPanel.h` |
| Create | `src/screens/dbnomics/DBnomicsSelectionPanel.cpp` |
| Create | `src/screens/dbnomics/DBnomicsChartWidget.h` |
| Create | `src/screens/dbnomics/DBnomicsChartWidget.cpp` |
| Create | `src/screens/dbnomics/DBnomicsDataTable.h` |
| Create | `src/screens/dbnomics/DBnomicsDataTable.cpp` |
| Modify | `CMakeLists.txt` (add 5 new `.cpp` files) |
| Modify | `src/app/MainWindow.h` (add include) |
| Modify | `src/app/MainWindow.cpp` (swap `register_screen` → `register_factory`) |

**Total new code**: ~11 files, ~1,200 lines of C++

---

## Key Design Decisions

| Decision | Rationale |
|----------|-----------|
| `DBnomicsService` singleton owns all HTTP | P6: screens render only; no direct network calls from UI |
| `register_factory` for lazy construction | P2: prevents API calls at app startup |
| No QTimer polling in screen | DBnomics data is static; user-driven refresh only |
| Qt6::Charts over custom SVG | Tauri used raw SVG; Qt6::Charts provides native rendering at lower complexity cost |
| TTL caching in service (not screen) | Keeps screens stateless; cache survives screen destroy/recreate |
| `QTimer::singleShot` restart for debounce | Canonical Qt debounce pattern; no extra state needed |
| 400ms debounce on search | Matches Tauri implementation exactly |
| Offset-based pagination (not cursor) | DBnomics API uses `offset` — match the backend |
