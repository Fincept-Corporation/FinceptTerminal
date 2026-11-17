#pragma once
// Dashboard tiles on the Fincept /data API (FinceptDataService). Every call is
// billed, so nothing here loads by itself (the billing rule: nothing that costs
// credits auto-runs) unless the account's plan includes /data at no credit cost
// (billing::PlanAccess, TileBilling.h). A tile shows the remembered answer of its request — the
// last one loaded, kept 14 days, with its "as of" — and otherwise a compact
// designed state with one LOAD button and the cost note; only LOAD and the
// title-bar refresh send a request (the service still answers from its client
// copy, free, while the server's freshness window lasts). The footer says where
// the data comes from and how old it is (freshness_text).

#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/fincept_data/FinceptDataModels.h"
#include "services/fincept_data/FinceptDataParse.h"

#include <QJsonObject>
#include <QSet>

#include <functional>

class QComboBox;
class QLabel;

namespace fincept::ui {
class DataTable;
class DonutChart;
class KpiStrip;
class TimeSeriesChart;
} // namespace fincept::ui

namespace fincept::screens::widgets {

class QuoteGrid;

/// Shared request / remembered answer / LOAD offer / error / freshness handling
/// for /data tiles.
class DataApiTile : public BaseWidget {
    Q_OBJECT
  public:
    DataApiTile(const QString& title, QWidget* parent, const QString& accent);
    ~DataApiTile() override;

  protected:
    using OkFn = std::function<void(const services::fincept_data::DataResult&)>;
    /// A /data route for this tile. `user` false (a show, a config change): never
    /// sent — `on_ok` gets the remembered answer of `path` (the last one loaded,
    /// footer marked as saved), or, with none, the tile offers LOAD (unless
    /// `quiet`). `user` true (LOAD, the title-bar refresh): sent (billed unless the
    /// service's copy is still fresh); a success is remembered. Errors go to the
    /// overlay (no data yet) or the footer (data kept); a `quiet` request (the
    /// tile's secondary one) reports none.
    void fetch_data(const QString& dataset, const QString& path, const services::fincept_data::QueryParams& query,
                    bool user, OkFn on_ok, bool quiet = false);
    /// What the LOAD offer says: the data in a few words, one sentence of what it
    /// returns, the button text ("LOAD CURVE") and the faint illustration.
    void set_load_offer(const QString& title, const QString& what, const QString& run_text,
                        ui::LoadingSkeleton::Shape shape);
    /// Footer: `prefix` · freshness of `r` (· "saved" for a remembered answer).
    void set_footer(const QString& prefix, const services::fincept_data::DataResult& r);
    void on_theme_changed() override;
    QLabel* footer_ = nullptr;
    bool has_data_ = false;
    /// The /data part is secondary (the tile has live content of its own):
    /// failures and the LOAD offer go to the footer, never over the tile.
    bool errors_in_footer_ = false;

  private:
    /// No remembered answer: a compact designed state with LOAD and the cost note
    /// (errors_in_footer_: a LOAD link in the footer). LOAD = the title-bar refresh.
    void offer_load(const QString& dataset);
    /// "N credit(s) per load …" from the dataset catalogue, else the generic note.
    QString cost_note(const QString& dataset) const;
    static QString remembered_key(const QString& path);
    static void remember(const QString& path, const services::fincept_data::DataResult& r);
    static bool recall(const QString& path, services::fincept_data::DataResult* out);

    QString offer_title_;
    QString offer_what_;
    QString offer_run_;
    ui::LoadingSkeleton::Shape offer_shape_ = ui::LoadingSkeleton::Shape::Result;
    bool delivering_remembered_ = false; ///< inside on_ok for a remembered answer
    bool offering_ = false;              ///< the LOAD offer is up (nothing loaded yet)
};

/// Government yield curve (type id "yield_curve"): US Treasury (Fed H.15, with
/// the curve 1 month and 1 year ago) or the ECB AAA euro-area spot curve,
/// chosen by the MarketProfile region unless configured. KPIs: 10Y, 2s10s,
/// 3m10y and the 10Y change. Config: {"curve": "treasury"|"euro"}.
class YieldCurveWidget : public DataApiTile {
    Q_OBJECT
  public:
    explicit YieldCurveWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);
    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void showEvent(QShowEvent* e) override;

  private:
    void load(bool refresh);
    QString curve() const;
    ui::TimeSeriesChart* chart_ = nullptr;
    ui::KpiStrip* kpis_ = nullptr;
    QComboBox* curve_combo_ = nullptr;
    QString curve_cfg_; ///< empty = region default
};

/// Volatility (type id "volatility"): the VIX family from live quotes (VIX9D,
/// VIX, VIX3M, VIX6M, VVIX, plus the region's own gauge) and the Cboe VIX
/// futures term structure (contango / backwardation, front-to-second spread).
class VolatilityWidget : public DataApiTile {
    Q_OBJECT
  public:
    explicit VolatilityWidget(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void on_topic_errors_changed() override;

  private:
    void subscribe_quotes();
    void load_curve(bool refresh);
    void render_curve();
    QuoteGrid* grid_ = nullptr;
    ui::TimeSeriesChart* chart_ = nullptr;
    QLabel* structure_ = nullptr;
    QVector<QPointF> futures_;
    double spot_ = 0;
    bool hub_active_ = false;
};

/// Crypto overview (type id "crypto_overview"): total market cap and its 24 h
/// change, volume, BTC / ETH dominance (donut) and the trending coins
/// (CoinGecko via /data/crypto/global and /data/crypto/trending).
class CryptoOverviewWidget : public DataApiTile {
    Q_OBJECT
  public:
    explicit CryptoOverviewWidget(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* e) override;

  private:
    void load(bool refresh);
    ui::KpiStrip* kpis_ = nullptr;
    ui::DonutChart* donut_ = nullptr;
    ui::DataTable* trending_ = nullptr;
};

/// Earnings calendar (type id "earnings_calendar"): the next two weeks of
/// earnings reports (/data/calendar/earnings, largest companies first), in the
/// user's locale, filterable to the user's region or to the symbols held in a
/// portfolio or a saved watchlist. Config: {"scope": "all"|"region"|"mine"}.
class EarningsCalendarWidget : public DataApiTile {
    Q_OBJECT
  public:
    explicit EarningsCalendarWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);
    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void showEvent(QShowEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

  private:
    void load(bool refresh);
    void render();
    QComboBox* scope_combo_ = nullptr;
    ui::DataTable* table_ = nullptr;
    QVector<services::fincept_data::CalendarEvent> events_;
    QSet<QString> mine_;
    QString scope_ = QStringLiteral("region");
};

} // namespace fincept::screens::widgets
