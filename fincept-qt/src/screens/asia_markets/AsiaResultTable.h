#pragma once
// AsiaResultTable — the result table shared by the Asia Markets and AkShare explorers, whose results
// are raw AkShare DataFrames ({rows, columns}):
//
//   * ResultModel: a model over the rows as they arrived (no per-cell items, no row cap). Cell text is
//     formatted on first paint of a row (ui::formatting, the user's number locale) and kept, with the
//     row's alignment / sign flags; colours come from the live theme tokens in data(), so an
//     appearance change needs no walk over the cells. Numbers in change-like columns (涨跌幅, change,
//     %, 净流入, …) carry an explicit sign and the POSITIVE / NEGATIVE token; every other cell is
//     plain text. Sorting is by value — the clicked column's keys are read once per row
//     (result_detail::column_sort_keys) and an index vector is sorted on them: numbers numerically,
//     then text, missing values last — and section -1 restores the source order.
//   * Helpers both screens share: the "needs an input" text of the scripts' structured answer, the
//     result pane's size estimate of a payload, and how long a remembered result stays fresh.
//   * fit_columns(): one pass over the header and the first rows to give each column a width that
//     fits its text (no resize-to-contents while filling).
//   * default_endpoint(): the endpoint each script opens on — keyless, quick, and a table worth
//     looking at (measured 2026-11-17, see the screens' first-show code).
//
// Header-only and without Q_OBJECT (the model adds no signals), so it needs no build-list entry.

#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"

#include <QAbstractTableModel>
#include <QFontMetrics>
#include <QHash>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>
#include <QTableView>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace fincept::screens::asia_links {

/// The endpoint a script opens on when the user has not picked one: keyless, answers in seconds,
/// and needs no SYMBOL (each was run with exactly the argv the screens send). Empty = none known;
/// the screens then take the first listed endpoint.
inline QString default_endpoint(const QString& script) {
    static const QHash<QString, QString> kDefaults = {
        {QStringLiteral("akshare_stocks_realtime.py"), QStringLiteral("stock_zh_index_spot_sina")},
        {QStringLiteral("akshare_stocks_historical.py"), QStringLiteral("stock_zh_index_daily")},
        {QStringLiteral("akshare_stocks_financial.py"), QStringLiteral("stock_financial_abstract")},
        {QStringLiteral("akshare_stocks_holders.py"), QStringLiteral("stock_main_stock_holder")},
        {QStringLiteral("akshare_stocks_funds.py"), QStringLiteral("stock_fund_flow_industry")},
        {QStringLiteral("akshare_stocks_board.py"), QStringLiteral("stock_board_industry_name_ths")},
        {QStringLiteral("akshare_stocks_margin.py"), QStringLiteral("stock_margin_sse")},
        {QStringLiteral("akshare_stocks_hot.py"), QStringLiteral("stock_news_main_cx")},
        {QStringLiteral("akshare_index.py"), QStringLiteral("index_stock_cons_sina")},
        {QStringLiteral("akshare_futures.py"), QStringLiteral("futures_zh_spot")},
        {QStringLiteral("akshare_bonds.py"), QStringLiteral("bond_cash_summary_sse")},
        {QStringLiteral("akshare_currency.py"), QStringLiteral("currency_boc_sina")},
        {QStringLiteral("akshare_energy.py"), QStringLiteral("energy_oil_hist")},
        {QStringLiteral("akshare_crypto.py"), QStringLiteral("crypto_spot")},
        {QStringLiteral("akshare_news.py"), QStringLiteral("news_cctv")},
        {QStringLiteral("akshare_reits.py"), QStringLiteral("reits_realtime")},
        {QStringLiteral("akshare_data.py"), QStringLiteral("china_cpi")},
        {QStringLiteral("akshare_alternative.py"), QStringLiteral("car_market_total")},
        {QStringLiteral("akshare_analysis.py"), QStringLiteral("get_buffett_indicator")},
        {QStringLiteral("akshare_derivatives.py"), QStringLiteral("option_cffex_hs300_spot_sina")},
        {QStringLiteral("akshare_economics_china.py"), QStringLiteral("cpi")},
        {QStringLiteral("akshare_funds_expanded.py"), QStringLiteral("get_fund_etf_category_sina")},
        {QStringLiteral("akshare_company_info.py"), QStringLiteral("cn_basic")},
        {QStringLiteral("akshare_macro.py"), QStringLiteral("macro_cnbs")},
        {QStringLiteral("akshare_misc.py"), QStringLiteral("spot_golden_benchmark_sge")},
    };
    return kDefaults.value(script);
}

/// The sample SYMBOL the screens offer when an endpoint reports that it needs one
/// (Ping An Bank, the placeholder of the SYMBOL box).
inline QString sample_symbol() {
    return QStringLiteral("000001");
}

/// The SYMBOL sent when the box is empty, for scripts whose every endpoint needs one (Company Info:
/// "cn_basic symbol=000001" measured 8 s, 2026-11-17); empty for every other script.
inline QString default_symbol(const QString& script, const QString& endpoint) {
    if (script == QLatin1String("akshare_company_info.py"))
        return endpoint == QLatin1String("hk_profile") ? QStringLiteral("00700") : sample_symbol();
    return {};
}

/// The parameter an endpoint said it lacks, from the scripts' "<endpoint> needs a value for: symbol
/// (SYMBOL box)" answer (akshare_screen_args.py); empty when `error` is not that answer. The fallback
/// for answers without the structured "missing" list (see missing_parameters_text()).
inline QString missing_parameter(const QString& error) {
    static const QString kMarker = QStringLiteral("needs a value for:");
    const int at = error.indexOf(kMarker);
    if (at < 0)
        return {};
    return error.mid(at + kMarker.size()).trimmed();
}

/// The screen box that fills endpoint parameter `param` ("SYMBOL", "START", "END", "PERIOD"), or ""
/// when no box does — the same table as akshare_screen_args.py (_FIELD_TARGETS / _FIELD_LABELS).
inline QString parameter_box(const QString& param) {
    const QString p = param.trimmed().toLower();
    if (p == QLatin1String("stock") || p == QLatin1String("symbol") || p == QLatin1String("fund_code"))
        return QStringLiteral("SYMBOL");
    if (p == QLatin1String("start_date") || p == QLatin1String("start_day") || p == QLatin1String("date") ||
        p == QLatin1String("trade_date"))
        return QStringLiteral("START");
    if (p == QLatin1String("end_date") || p == QLatin1String("end_day"))
        return QStringLiteral("END");
    if (p == QLatin1String("period"))
        return QStringLiteral("PERIOD");
    return {};
}

/// "symbol (SYMBOL box), date (START box)" for the parameter names of the scripts' structured
/// "needs an input" answer (QueryResult::missing) — the wording the scripts' own message uses.
inline QString missing_parameters_text(const QStringList& params) {
    QStringList parts;
    for (const QString& p : params) {
        const QString box = parameter_box(p);
        parts << (box.isEmpty() ? p : QStringLiteral("%1 (%2 box)").arg(p, box));
    }
    return parts.join(QStringLiteral(", "));
}

/// True when one of `params` is filled by the SYMBOL box.
inline bool missing_needs_symbol(const QStringList& params) {
    for (const QString& p : params)
        if (parameter_box(p) == QLatin1String("SYMBOL"))
            return true;
    return false;
}

/// The result pane's size estimate of a payload {rows, columns, approx_bytes, …}: the decode
/// thread's estimate when the payload carries one, else rows × columns × 64 bytes (results
/// remembered before the estimate existed). O(1) — the pane's default estimator walks the whole tree
/// on the GUI thread at every delivery.
inline qint64 result_payload_bytes(const QJsonValue& payload) {
    const QJsonObject p = payload.toObject();
    const double estimate = p.value(QStringLiteral("approx_bytes")).toDouble();
    if (estimate > 0)
        return static_cast<qint64>(estimate);
    const qint64 rows = p.value(QStringLiteral("rows")).toArray().size();
    const qint64 cols = std::max<qint64>(1, p.value(QStringLiteral("columns")).toArray().size());
    return rows * cols * 64;
}

/// Memory budget of each explorer's result pane (estimated bytes of the results it keeps per view).
/// Measured default results (approx_json_bytes of the rows, 2026-11-17): macro_shipping_bdi 9,588
/// rows ≈ 3.4 MB, stock_zh_index_daily 8,764 rows ≈ 2.3 MB, bond_zh_cov 1,067 × 19 ≈ 1.0 MB,
/// get_buffett_indicator 5,247 rows ≈ 0.9 MB, most others 0.1–0.6 MB. A result over the 2 MB persist
/// limit (macro_shipping_bdi: 2.1 MB of compact JSON) is not written to the cache, so dropping it
/// from memory means running it again on the next visit (110 s). 64 MB keeps ~18 of the largest, or
/// every typical result of a long browsing session (the 48-view cap bounds the count); results that
/// reload from the cache without a run are dropped first, so the unpersisted large ones go last.
constexpr qint64 kResultMemoryBudgetBytes = 64LL * 1024 * 1024;

/// Seconds a remembered result of `endpoint` (in source / category `source`) stays fresh before a
/// later visit refreshes it quietly:
///   * 5 min — quotes that move during the session: spot / realtime / tick / minute endpoints and the
///     realtime source (a 15-minute-old quote table is stale on a revisit);
///   * 6 h — daily history, financial statements, shareholders, company facts and macro series, which
///     change at most once a day (macro_shipping_bdi takes 110 s: re-running it every 15 minutes on a
///     revisit only cost time and rate limit);
///   * 15 min — everything else (rankings, fund flows, boards, margin, news), as before.
inline int result_max_age_s(const QString& source, const QString& endpoint) {
    constexpr int kLive = 5 * 60;
    constexpr int kSlow = 6 * 3600;
    constexpr int kDefault = 15 * 60;
    const QString e = endpoint.toLower();
    for (const char* k : {"spot", "realtime", "tick", "minute", "_min", "intraday"})
        if (e.contains(QLatin1String(k)))
            return kLive;
    if (source == QLatin1String("stocks_realtime") || source == QLatin1String("realtime"))
        return kLive;
    for (const char* k : {"hist", "daily", "monthly", "yearly", "annual", "quarter", "macro"})
        if (e.contains(QLatin1String(k)))
            return kSlow;
    for (const char* s : {"stocks_historical", "historical", "stocks_financial", "financial", "stocks_holders",
                          "holders", "company_info", "macro", "economics_china", "data", "analysis", "alternative"})
        if (source == QLatin1String(s))
            return kSlow;
    return kDefault;
}

namespace result_detail {

inline bool header_has(const QString& header, std::initializer_list<const char*> needles) {
    const QString h = header.toLower();
    for (const char* n : needles)
        if (h.contains(QString::fromUtf8(n)))
            return true;
    return false;
}

/// Columns whose numbers are codes, counters or dates: printed without grouping ("2024", "600519").
inline bool plain_column(const QString& header) {
    return header_has(header, {"代码", "序号", "编号", "年", "日期", "时间", "期", "code", "symbol", "year", "date",
                               "time", "rank", "排名", "id"});
}

/// Columns whose sign is a direction (a change, a net flow): explicit "+" and the gain / loss colour.
inline bool direction_column(const QString& header) {
    return header_has(header, {"涨跌", "涨幅", "跌幅", "增减", "变动", "变化", "净流入", "净额", "净买", "同比", "环比",
                               "change", "chg", "pct", "%", "diff", "yoy", "mom"});
}

/// The number in a cell: a JSON number, or a string that is one ("12.5" — several akshare frames
/// carry prices as text). False for anything else.
inline bool cell_number(const QJsonValue& v, double* out) {
    if (v.isDouble()) {
        *out = v.toDouble();
        return true;
    }
    if (!v.isString())
        return false;
    bool ok = false;
    *out = v.toString().trimmed().toDouble(&ok);
    return ok;
}

/// Decimals to show for `v`: as many as it has, up to four (trailing zeros dropped).
inline int decimals_of(double v) {
    const double a = std::abs(v);
    double scale = 1.0;
    for (int d = 0; d < 4; ++d) {
        const double scaled = a * scale;
        if (std::abs(scaled - std::round(scaled)) < 1e-9 * std::max(1.0, scaled))
            return d;
        scale *= 10.0;
    }
    return 4;
}

/// One column's sort keys, by source row — read once per row, so a sort compares plain values
/// instead of looking two JSON cells up (and converting them) per comparison.
struct ColumnSortKeys {
    enum Rank : quint8 { Number = 0, Text = 1, Missing = 2 };
    QVector<quint8> rank;   ///< per source row
    QVector<double> number; ///< the value of Number rows
    QVector<QString> text;  ///< the text of Text rows (empty for the others)
};

/// The keys of `column`: a number when cell_number() reads one (JSON numbers and numeric text), else
/// the cell's text; null / absent cells (and NaN) are missing. Pure; any thread.
inline ColumnSortKeys column_sort_keys(const QVector<QJsonObject>& rows, const QString& column) {
    ColumnSortKeys k;
    const qsizetype n = rows.size();
    k.rank.resize(n);
    k.number.resize(n);
    k.text.resize(n);
    for (qsizetype i = 0; i < n; ++i) {
        const QJsonValue v = rows.at(i).value(column);
        double d = 0;
        if (v.isNull() || v.isUndefined()) {
            k.rank[i] = ColumnSortKeys::Missing;
        } else if (cell_number(v, &d) && !std::isnan(d)) {
            k.rank[i] = ColumnSortKeys::Number;
            k.number[i] = d;
        } else {
            k.rank[i] = ColumnSortKeys::Text;
            k.text[i] = v.isString() ? v.toString() : v.toVariant().toString();
        }
    }
    return k;
}

/// Stable sort of `order` (view row -> source row) by `keys`: numbers by value, then text, then
/// missing values — numbers and text follow the direction, missing values stay last either way (a
/// total order, also for a column that mixes numbers and text). Ties keep their current order.
inline void sort_by_keys(QVector<int>& order, const ColumnSortKeys& keys, bool ascending) {
    std::stable_sort(order.begin(), order.end(), [&keys, ascending](int a, int b) {
        const quint8 ra = keys.rank.at(a);
        const quint8 rb = keys.rank.at(b);
        if (ra != rb)
            return ra < rb;
        if (ra == ColumnSortKeys::Number) {
            const double da = keys.number.at(a);
            const double db = keys.number.at(b);
            return ascending ? da < db : da > db;
        }
        if (ra == ColumnSortKeys::Text) {
            const int c = keys.text.at(a).compare(keys.text.at(b));
            return ascending ? c < 0 : c > 0;
        }
        return false; // both missing
    });
}

} // namespace result_detail

class ResultModel : public QAbstractTableModel {
  public:
    using QAbstractTableModel::QAbstractTableModel;

    /// Replaces the table. `columns` is the script's ordered column list (QJsonObject sorts its
    /// keys, so a list read off the first row would scramble the DataFrame's order).
    void set_result(const QJsonArray& rows, const QStringList& columns) {
        beginResetModel();
        rows_.clear();
        rows_.reserve(rows.size());
        for (const QJsonValue& r : rows)
            rows_.append(r.isObject() ? r.toObject() : QJsonObject{{QStringLiteral("value"), r}});
        columns_ = columns;
        if (columns_.isEmpty() && !rows_.isEmpty()) {
            for (auto it = rows_.first().constBegin(); it != rows_.first().constEnd(); ++it)
                columns_ << it.key();
        }
        plain_.resize(columns_.size());
        direction_.resize(columns_.size());
        for (int c = 0; c < columns_.size(); ++c) {
            plain_[c] = result_detail::plain_column(columns_[c]);
            direction_[c] = !plain_[c] && result_detail::direction_column(columns_[c]);
        }
        order_.resize(rows_.size());
        std::iota(order_.begin(), order_.end(), 0);
        text_.clear();
        text_.resize(rows_.size());
        cell_flags_.fill(0, rows_.size() * columns_.size());
        sort_keys_.clear();
        endResetModel();
    }

    void clear() { set_result({}, {}); }

    const QStringList& columns() const { return columns_; }
    /// Display text of a cell (view row = model row; no proxy in between). A row not painted yet is
    /// not formatted whole for it (lookups walk every row: the symbol column of a linked symbol).
    QString text(int row, int column) const {
        if (row < 0 || row >= order_.size() || column < 0 || column >= columns_.size())
            return {};
        const int src = order_[row];
        if (!text_[src].isEmpty())
            return text_[src].value(column);
        return cell_text(rows_[src].value(columns_[column]), column);
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : static_cast<int>(rows_.size());
    }
    int columnCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : static_cast<int>(columns_.size());
    }

    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid() || index.row() >= order_.size() || index.column() >= columns_.size())
            return {};
        const int src = order_[index.row()];
        const int col = index.column();
        switch (role) {
            case Qt::DisplayRole:
                return row_text(src).value(col);
            case Qt::ToolTipRole: {
                const QString t = row_text(src).value(col);
                return t.size() > 24 ? QVariant(t) : QVariant();
            }
            case Qt::TextAlignmentRole: {
                // Numbers (and numeric text outside code / date columns) right-aligned, text left.
                const bool numeric = (row_flags(src)[col] & kCellNumeric) != 0;
                return static_cast<int>((numeric ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
            }
            case Qt::ForegroundRole: {
                if (!direction_[col])
                    return {};
                const quint8 f = row_flags(src)[col];
                if (!(f & (kCellPositive | kCellNegative)))
                    return {};
                // Live tokens: the theme in effect when the cell is painted.
                return QColor((f & kCellPositive) ? ui::colors::POSITIVE() : ui::colors::NEGATIVE());
            }
            default:
                return {};
        }
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
        if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section >= 0 && section < columns_.size())
            return columns_[section];
        return QAbstractTableModel::headerData(section, orientation, role);
    }

    /// Sort by `column`'s values; -1 restores the order the source sent. The column's keys are read
    /// once (and kept until the next result, so the other direction is only the sort itself).
    void sort(int column, Qt::SortOrder order) override {
        emit layoutAboutToBeChanged({}, QAbstractItemModel::VerticalSortHint);
        const QModelIndexList before = persistentIndexList();
        QVector<int> old_order = order_;
        if (column < 0 || column >= columns_.size()) {
            std::iota(order_.begin(), order_.end(), 0);
        } else {
            auto it = sort_keys_.find(column);
            if (it == sort_keys_.end()) {
                if (sort_keys_.size() >= kMaxKeptSortKeys)
                    sort_keys_.clear();
                it = sort_keys_.insert(column, result_detail::column_sort_keys(rows_, columns_[column]));
            }
            result_detail::sort_by_keys(order_, it.value(), order == Qt::AscendingOrder);
        }
        // Keep selections on the same source rows.
        QVector<int> new_pos(order_.size());
        for (int i = 0; i < order_.size(); ++i)
            new_pos[order_[i]] = i;
        QModelIndexList after;
        after.reserve(before.size());
        for (const QModelIndex& idx : before) {
            const int src = idx.row() < old_order.size() ? old_order[idx.row()] : -1;
            after << (src >= 0 ? index(new_pos[src], idx.column()) : QModelIndex());
        }
        changePersistentIndexList(before, after);
        emit layoutChanged({}, QAbstractItemModel::VerticalSortHint);
    }

  private:
    /// Per-cell flags, filled with the row's text: right-aligned number, positive / negative value of
    /// a direction column (painted with the live POSITIVE / NEGATIVE token).
    static constexpr quint8 kCellNumeric = 0x1;
    static constexpr quint8 kCellPositive = 0x2;
    static constexpr quint8 kCellNegative = 0x4;
    /// Columns whose sort keys are kept at once (a header clicked again only re-sorts).
    static constexpr int kMaxKeptSortKeys = 4;

    const QStringList& row_text(int src) const {
        QStringList& cached = text_[src];
        if (cached.isEmpty() && !columns_.isEmpty()) {
            cached.reserve(columns_.size());
            const QJsonObject& row = rows_[src];
            quint8* flags = cell_flags_.data() + qsizetype(src) * columns_.size();
            for (int c = 0; c < columns_.size(); ++c) {
                const QJsonValue v = row.value(columns_[c]);
                cached << cell_text(v, c);
                flags[c] = cell_flags(v, c);
            }
        }
        return cached;
    }

    /// The flags of source row `src` (formats the row first when it was not yet).
    const quint8* row_flags(int src) const {
        row_text(src);
        return cell_flags_.constData() + qsizetype(src) * columns_.size();
    }

    quint8 cell_flags(const QJsonValue& v, int col) const {
        double d = 0;
        const bool number = result_detail::cell_number(v, &d);
        quint8 f = 0;
        if (v.isDouble() || (!plain_[col] && number))
            f |= kCellNumeric;
        if (direction_[col] && number && d != 0.0 && !std::isnan(d))
            f |= d > 0 ? kCellPositive : kCellNegative;
        return f;
    }

    QString cell_text(const QJsonValue& v, int col) const {
        namespace fmt = fincept::ui::formatting;
        if (v.isDouble()) {
            const double d = v.toDouble();
            if (!std::isfinite(d))
                return fmt::placeholder();
            const int decimals = result_detail::decimals_of(d);
            if (plain_[col])
                return QString::number(d, 'f', decimals);
            return direction_[col] ? fmt::signed_number(d, decimals) : fmt::number(d, decimals);
        }
        if (v.isNull() || v.isUndefined())
            return fmt::placeholder();
        if (v.isBool())
            return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        if (v.isString())
            return v.toString();
        // Nested values (rare): compact JSON.
        return QString::fromUtf8(v.isArray() ? QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact)
                                             : QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    }

    QVector<QJsonObject> rows_;
    QStringList columns_;
    QVector<bool> plain_;
    QVector<bool> direction_;
    QVector<int> order_;               ///< view row -> source row
    mutable QVector<QStringList> text_; ///< per source row, formatted on first paint
    mutable QVector<quint8> cell_flags_; ///< source row × column, filled with the row's text
    QHash<int, result_detail::ColumnSortKeys> sort_keys_; ///< column -> its keys (this result only)
};

/// Gives every column a width that fits its header and the first rows' text (one pass, after the
/// model was filled — never resize-to-contents during a fill). Widths stay user-adjustable.
inline void fit_columns(QTableView* view, ResultModel* model) {
    QHeaderView* h = view->horizontalHeader();
    h->setSectionResizeMode(QHeaderView::Interactive);
    const QFontMetrics cell_fm(view->font());
    QFont hf = view->font();
    hf.setBold(true);
    const QFontMetrics head_fm(hf);
    const int rows = std::min(model->rowCount(), 60);
    const int pad = cell_fm.horizontalAdvance(QLatin1Char('M')) * 2 + 8;
    const int min_w = cell_fm.horizontalAdvance(QStringLiteral("0000")) + pad;
    const int max_w = cell_fm.horizontalAdvance(QLatin1Char('M')) * 28 + pad;
    for (int c = 0; c < model->columnCount(); ++c) {
        int w = head_fm.horizontalAdvance(model->columns().value(c)) + pad;
        for (int r = 0; r < rows; ++r)
            w = std::max(w, cell_fm.horizontalAdvance(model->text(r, c)) + pad);
        h->resizeSection(c, std::clamp(w, min_w, max_w));
    }
}

/// Data-table row height for the table's font size (offset -3): 22 px at the default size.
inline int result_row_height() {
    return std::max(22, QFontMetrics(ui::fonts::font(-3)).height() + 6);
}

} // namespace fincept::screens::asia_links
