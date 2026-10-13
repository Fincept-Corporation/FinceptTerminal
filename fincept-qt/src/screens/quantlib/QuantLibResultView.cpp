// src/screens/quantlib/QuantLibResultView.cpp
//
// Results pane of the QuantLib screen — see the header. Styling comes from the
// QuantLib screen's root stylesheet (#ql* selectors); the chart is themed
// through the QtCharts API. Nothing here calls setStyleSheet.

#include "screens/quantlib/QuantLibResultView.h"

#include "core/logging/Logger.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QButtonGroup>
#include <QChart>
#include <QChartView>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeySequence>
#include <QLabel>
#include <QLegend>
#include <QLineSeries>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStyle>
#include <QTableWidget>
#include <QTextStream>
#include <QValueAxis>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::screens {

using namespace fincept::ui;

namespace {

constexpr int kQlTableRowCap = 5000;      // rows rendered in the widget (CSV/COPY export all)
constexpr int kQlChartPointCap = 4000;    // points per plotted series (stride-downsampled above)
constexpr int kQlChartSeriesCap = 25;     // series per chart
constexpr int kQlJsonCharCap = 2'000'000; // raw JSON view

QString ql_rv_number(double v) {
    if (!std::isfinite(v))
        return QString::number(v);
    if (std::floor(v) == v && std::fabs(v) < 1e15)
        return QString::number(static_cast<qlonglong>(v));
    return QString::number(v, 'g', 10);
}

QString ql_rv_number_full(double v) {
    if (std::floor(v) == v && std::fabs(v) < 1e15)
        return QString::number(static_cast<qlonglong>(v));
    return QString::number(v, 'g', 15);
}

QString ql_rv_compact(const QJsonValue& v) {
    if (v.isArray())
        return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
    if (v.isObject())
        return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    return {};
}

// Cell text for display (`full` = export precision).
QString ql_rv_cell(const QJsonValue& v, bool full) {
    if (v.isDouble())
        return full ? ql_rv_number_full(v.toDouble()) : ql_rv_number(v.toDouble());
    if (v.isBool())
        return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (v.isNull() || v.isUndefined())
        return full ? QString() : QStringLiteral("--");
    if (v.isString())
        return v.toString();
    return ql_rv_compact(v);
}

QString ql_rv_csv_escape(QString s) {
    if (s.contains(QLatin1Char(',')) || s.contains(QLatin1Char('"')) || s.contains(QLatin1Char('\n'))) {
        s.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        s = QLatin1Char('"') + s + QLatin1Char('"');
    }
    return s;
}

bool ql_rv_numeric_array(const QJsonValue& v, QList<double>* out) {
    if (!v.isArray())
        return false;
    const QJsonArray a = v.toArray();
    if (a.size() < 2)
        return false;
    out->clear();
    out->reserve(a.size());
    for (const auto& e : a) {
        if (!e.isDouble())
            return false;
        out->append(e.toDouble());
    }
    return true;
}

bool ql_rv_numeric_matrix(const QJsonValue& v, QList<QList<double>>* rows) {
    if (!v.isArray())
        return false;
    const QJsonArray a = v.toArray();
    if (a.isEmpty())
        return false;
    rows->clear();
    for (const auto& r : a) {
        if (!r.isArray())
            return false;
        QList<double> row;
        for (const auto& e : r.toArray()) {
            if (!e.isDouble())
                return false;
            row.append(e.toDouble());
        }
        rows->append(row);
    }
    return true;
}

// Column keys of an array of objects, in first-appearance order.
QStringList ql_rv_object_columns(const QJsonArray& arr) {
    QStringList cols;
    const int scan = std::min<int>(arr.size(), 500);
    for (int i = 0; i < scan; ++i) {
        const QJsonObject o = arr.at(i).toObject();
        for (auto it = o.begin(); it != o.end(); ++it) {
            if (!cols.contains(it.key()))
                cols << it.key();
        }
    }
    return cols;
}

QList<QPointF> ql_rv_downsample(const QList<QPointF>& pts, bool* downsampled) {
    if (pts.size() <= kQlChartPointCap)
        return pts;
    *downsampled = true;
    QList<QPointF> out;
    const double stride = double(pts.size()) / kQlChartPointCap;
    out.reserve(kQlChartPointCap + 1);
    for (int i = 0; i < kQlChartPointCap; ++i)
        out.append(pts.at(int(i * stride)));
    out.append(pts.last());
    return out;
}

} // namespace

// ── Construction ─────────────────────────────────────────────────────────────

QuantLibResultView::QuantLibResultView(QWidget* parent) : QWidget(parent) {
    setObjectName("qlResultPanel");
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Toolbar: RESULTS · endpoint ····· status · TABLE CHART JSON · COPY CSV
    auto* bar = new QWidget(this);
    bar->setObjectName("qlResultBar");
    bar->setFixedHeight(34);
    auto* bl = new QHBoxLayout(bar);
    bl->setContentsMargins(12, 0, 8, 0);
    bl->setSpacing(6);
    title_ = new QLabel(tr("RESULTS"), bar);
    title_->setObjectName("qlPanelTitle");
    bl->addWidget(title_);
    endpoint_label_ = new QLabel(bar);
    endpoint_label_->setObjectName("qlResultEndpoint");
    bl->addWidget(endpoint_label_);
    bl->addStretch(1);
    status_ = new QLabel(bar);
    status_->setObjectName("qlResultStatus");
    bl->addWidget(status_);

    auto make_btn = [bar](const QString& text, const char* object_name) {
        auto* b = new QPushButton(text, bar);
        b->setObjectName(object_name);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::TabFocus);
        return b;
    };
    table_btn_ = make_btn(tr("TABLE"), "qlViewBtn");
    chart_btn_ = make_btn(tr("CHART"), "qlViewBtn");
    json_btn_ = make_btn(tr("JSON"), "qlViewBtn");
    auto* group = new QButtonGroup(this);
    group->setExclusive(true);
    for (auto* b : {table_btn_, chart_btn_, json_btn_}) {
        b->setCheckable(true);
        group->addButton(b);
        bl->addWidget(b);
    }
    table_btn_->setChecked(true);
    connect(table_btn_, &QPushButton::clicked, this, [this]() { set_view(View::Table); });
    connect(chart_btn_, &QPushButton::clicked, this, [this]() { set_view(View::Chart); });
    connect(json_btn_, &QPushButton::clicked, this, [this]() { set_view(View::Json); });

    copy_btn_ = make_btn(tr("COPY"), "qlFormBtn");
    copy_btn_->setToolTip(tr("Copy the current view (table as tab-separated text, chart as an image)"));
    connect(copy_btn_, &QPushButton::clicked, this, &QuantLibResultView::copy_current);
    bl->addWidget(copy_btn_);
    csv_btn_ = make_btn(tr("CSV"), "qlFormBtn");
    csv_btn_->setToolTip(tr("Export the selected section — every row — to a CSV file"));
    connect(csv_btn_, &QPushButton::clicked, this, &QuantLibResultView::export_csv);
    bl->addWidget(csv_btn_);
    vl->addWidget(bar);

    // Section selector (only when an object response carries arrays).
    auto* section_bar = new QWidget(this);
    section_bar->setObjectName("qlSectionBar");
    auto* sl = new QHBoxLayout(section_bar);
    sl->setContentsMargins(12, 4, 12, 4);
    sl->setSpacing(6);
    section_label_ = new QLabel(tr("VIEW"), section_bar);
    section_label_->setObjectName("qlLabel");
    section_combo_ = new QComboBox(section_bar);
    section_combo_->setAccessibleName(tr("Result section"));
    connect(section_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { render_current(); });
    sl->addWidget(section_label_);
    sl->addWidget(section_combo_, 1);
    vl->addWidget(section_bar);
    section_bar->hide();

    stack_ = new QStackedWidget(this);

    message_ = new QLabel(this);
    message_->setObjectName("qlResultMessage");
    message_->setAlignment(Qt::AlignCenter);
    message_->setWordWrap(true);
    stack_->addWidget(message_); // kMessagePage

    table_ = new QTableWidget(this);
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectItems);
    table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->setAccessibleName(tr("QuantLib result table"));
    stack_->addWidget(table_); // kTablePage

    chart_view_ = new QChartView(this);
    chart_view_->setRenderHint(QPainter::Antialiasing, true);
    chart_view_->setChart(new QChart());
    stack_->addWidget(chart_view_); // kChartPage

    json_ = new QPlainTextEdit(this);
    json_->setObjectName("qlJsonView");
    json_->setReadOnly(true);
    json_->setLineWrapMode(QPlainTextEdit::NoWrap);
    stack_->addWidget(json_); // kJsonPage

    auto* error_page = new QWidget(this);
    error_page->setObjectName("qlErrorPage");
    auto* el = new QVBoxLayout(error_page);
    el->setContentsMargins(20, 20, 20, 20);
    el->setSpacing(10);
    error_title_ = new QLabel(error_page);
    error_title_->setObjectName("qlErrorTitle");
    error_text_ = new QLabel(error_page);
    error_text_->setObjectName("qlErrorText");
    error_text_->setWordWrap(true);
    error_text_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    error_hint_ = new QLabel(error_page);
    error_hint_->setObjectName("qlErrorHint");
    error_hint_->setWordWrap(true);
    auto* actions = new QHBoxLayout;
    actions->setSpacing(8);
    error_primary_ = new QPushButton(error_page);
    error_primary_->setObjectName("qlExecBtn");
    error_primary_->setCursor(Qt::PointingHandCursor);
    connect(error_primary_, &QPushButton::clicked, this, &QuantLibResultView::upgrade_requested);
    error_retry_ = new QPushButton(tr("RETRY"), error_page);
    error_retry_->setObjectName("qlFormBtn");
    error_retry_->setCursor(Qt::PointingHandCursor);
    connect(error_retry_, &QPushButton::clicked, this, &QuantLibResultView::retry_requested);
    actions->addWidget(error_primary_);
    actions->addWidget(error_retry_);
    actions->addStretch(1);
    el->addWidget(error_title_);
    el->addWidget(error_text_);
    el->addWidget(error_hint_);
    el->addLayout(actions);
    el->addStretch(1);
    stack_->addWidget(error_page); // kErrorPage

    vl->addWidget(stack_, 1);

    // Ctrl+C on the table copies the selection (or the whole section).
    auto* copy_sc = new QShortcut(QKeySequence::Copy, table_);
    copy_sc->setContext(Qt::WidgetShortcut);
    connect(copy_sc, &QShortcut::activated, this, &QuantLibResultView::copy_current);

    show_idle();
}

void QuantLibResultView::retranslate() {
    title_->setText(tr("RESULTS"));
    table_btn_->setText(tr("TABLE"));
    chart_btn_->setText(tr("CHART"));
    json_btn_->setText(tr("JSON"));
    copy_btn_->setText(tr("COPY"));
    copy_btn_->setToolTip(tr("Copy the current view (table as tab-separated text, chart as an image)"));
    csv_btn_->setText(tr("CSV"));
    csv_btn_->setToolTip(tr("Export the selected section — every row — to a CSV file"));
    section_label_->setText(tr("VIEW"));
    error_retry_->setText(tr("RETRY"));
    if (has_failure_)
        show_failure(endpoint_, failure_);
    else if (!has_result_)
        show_idle();
}

// ── States ───────────────────────────────────────────────────────────────────

void QuantLibResultView::set_result_controls_enabled(bool enabled) {
    for (auto* b : {table_btn_, chart_btn_, json_btn_, copy_btn_, csv_btn_})
        b->setEnabled(enabled);
    if (!enabled)
        section_combo_->parentWidget()->hide();
}

void QuantLibResultView::show_idle() {
    has_result_ = false;
    has_failure_ = false;
    endpoint_label_->clear();
    status_->clear();
    set_result_controls_enabled(false);
    message_->setText(tr("Pick a module and an endpoint, set the parameters, then press Enter or EXECUTE."));
    stack_->setCurrentIndex(kMessagePage);
}

void QuantLibResultView::show_loading(const QString& endpoint, const QString& cost_note) {
    has_failure_ = false;
    endpoint_label_->setText(endpoint);
    status_->setText(cost_note.isEmpty() ? tr("Computing…") : tr("Computing… (%1)").arg(cost_note));
    set_result_controls_enabled(false);
    message_->setText(tr("Computing %1 …").arg(endpoint));
    stack_->setCurrentIndex(kMessagePage);
}

void QuantLibResultView::show_input_error(const QString& message) {
    has_failure_ = false;
    status_->setText(tr("Not sent"));
    show_error_page(tr("INVALID INPUT"), QStringLiteral("error"), message,
                    tr("Nothing was sent — fix the highlighted field and run again."), QString(), false);
}

void QuantLibResultView::show_error_page(const QString& title, const QString& tone, const QString& message,
                                         const QString& hint, const QString& primary_text, bool show_retry) {
    set_result_controls_enabled(false);
    error_title_->setText(title);
    error_title_->setProperty("tone", tone);
    error_title_->style()->unpolish(error_title_);
    error_title_->style()->polish(error_title_);
    error_text_->setText(message);
    error_hint_->setText(hint);
    error_hint_->setVisible(!hint.isEmpty());
    error_primary_->setText(primary_text);
    error_primary_->setVisible(!primary_text.isEmpty());
    error_retry_->setVisible(show_retry);
    stack_->setCurrentIndex(kErrorPage);
}

void QuantLibResultView::show_failure(const QString& endpoint, const services::QuantLibFailure& failure) {
    using Kind = services::QuantLibFailureKind;
    endpoint_ = endpoint;
    failure_ = failure;
    has_failure_ = true;
    has_result_ = false;
    endpoint_label_->setText(endpoint);
    status_->setText(failure.http_status > 0 ? tr("HTTP %1").arg(failure.http_status) : tr("Failed"));

    switch (failure.kind) {
        case Kind::NeedsPro:
            show_error_page(tr("THIS MODULE NEEDS PRO"), QStringLiteral("pro"), failure.message,
                            tr("Pro ($10/month) includes every QuantLib module — all endpoints at 0 credits per "
                               "call."),
                            tr("UPGRADE TO PRO"), false);
            break;
        case Kind::OutOfCredits:
            show_error_page(tr("OUT OF CREDITS"), QStringLiteral("warn"), failure.message,
                            tr("On the Free plan this endpoint costs 1 credit per call. Pro includes every QuantLib "
                               "endpoint at 0 credits."),
                            tr("VIEW PLANS"), true);
            break;
        case Kind::AuthRequired:
            show_error_page(tr("SIGN-IN REQUIRED"), QStringLiteral("warn"), failure.message,
                            tr("Your session is missing or has expired. Sign in again, then retry."), QString(),
                            true);
            break;
        case Kind::RateLimited:
            show_error_page(tr("RATE LIMITED"), QStringLiteral("warn"), failure.message,
                            tr("Too many requests right now — wait a moment, then retry."), QString(), true);
            break;
        case Kind::InvalidInput:
            show_error_page(tr("INVALID INPUT"), QStringLiteral("error"), failure.message,
                            tr("The server rejected the request body. Check the field names and units (decimals: "
                               "0.05 = 5%)."),
                            QString(), false);
            break;
        case Kind::Network:
            show_error_page(tr("NETWORK ERROR"), QStringLiteral("error"), failure.message,
                            tr("The request did not reach the QuantLib service. Check your connection and retry."),
                            QString(), true);
            break;
        case Kind::Server:
            show_error_page(tr("SERVICE ERROR"), QStringLiteral("error"), failure.message,
                            tr("The QuantLib service did not return a usable answer. Retry in a moment."), QString(),
                            true);
            break;
        case Kind::Computation:
            show_error_page(tr("COMPUTATION FAILED"), QStringLiteral("error"), failure.message,
                            tr("The model could not be evaluated with these inputs. Adjust the parameters and run "
                               "again."),
                            QString(), false);
            break;
    }
}

void QuantLibResultView::show_result(const QString& endpoint, const QJsonObject& request, const QJsonValue& payload,
                                     qint64 elapsed_ms) {
    endpoint_ = endpoint;
    request_ = request;
    payload_ = payload;
    elapsed_ms_ = elapsed_ms;
    has_result_ = true;
    has_failure_ = false;
    endpoint_label_->setText(endpoint);
    set_result_controls_enabled(true);
    build_sections();
    render_current();
}

// ── Sections ─────────────────────────────────────────────────────────────────

void QuantLibResultView::build_sections() {
    sections_.clear();
    if (payload_.isObject()) {
        const QJsonObject obj = payload_.toObject();
        sections_.append({tr("Summary"), payload_});
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            if (it.value().isArray() && !it.value().toArray().isEmpty())
                sections_.append({it.key(), it.value()});
        }
    } else {
        sections_.append({tr("Result"), payload_});
    }
    {
        const QSignalBlocker block(section_combo_);
        section_combo_->clear();
        for (const auto& s : sections_) {
            const int n = s.second.isArray() ? int(s.second.toArray().size()) : -1;
            section_combo_->addItem(n >= 0 ? tr("%1  (%2 rows)").arg(s.first).arg(n) : s.first);
        }
        section_combo_->setCurrentIndex(0);
    }
    section_combo_->parentWidget()->setVisible(sections_.size() > 1);
}

void QuantLibResultView::render_current() {
    if (!has_result_ || sections_.isEmpty())
        return;
    const int idx = std::clamp(section_combo_->currentIndex(), 0, int(sections_.size()) - 1);
    const QJsonValue section = sections_.at(idx).second;

    // Status: size of what is shown + round-trip time.
    QString size_text;
    if (section.isArray()) {
        const int n = int(section.toArray().size());
        size_text = n > kQlTableRowCap ? tr("showing %1 of %2 rows").arg(kQlTableRowCap).arg(n) : tr("%1 rows").arg(n);
    } else if (section.isObject()) {
        size_text = tr("%1 fields").arg(section.toObject().size());
    } else {
        size_text = tr("1 value");
    }
    status_->setText(tr("%1 · %2 ms").arg(size_text).arg(elapsed_ms_));

    render_table(section);
    QString x_title;
    const QList<Series> series = chart_series(section, &x_title);
    chart_btn_->setEnabled(!series.isEmpty());
    chart_btn_->setToolTip(series.isEmpty() ? tr("No numeric series in this section to chart") : QString());
    if (!series.isEmpty())
        render_chart(series, x_title);
    render_json();

    View v = view_;
    if (v == View::Chart && series.isEmpty())
        v = View::Table;
    set_view(v);
}

void QuantLibResultView::set_view(View view) {
    view_ = view;
    const QSignalBlocker b1(table_btn_), b2(chart_btn_), b3(json_btn_);
    table_btn_->setChecked(view == View::Table);
    chart_btn_->setChecked(view == View::Chart);
    json_btn_->setChecked(view == View::Json);
    if (!has_result_)
        return;
    stack_->setCurrentIndex(view == View::Table ? kTablePage : view == View::Chart ? kChartPage : kJsonPage);
}

// ── Table ────────────────────────────────────────────────────────────────────

void QuantLibResultView::render_table(const QJsonValue& section) {
    table_->setSortingEnabled(false);
    table_->clear();

    auto numeric_item = [](const QJsonValue& v) -> QTableWidgetItem* {
        const QString text = ql_rv_cell(v, false);
        QTableWidgetItem* item = v.isDouble() ? new ui::NumericTableWidgetItem(text, v.toDouble())
                                              : new QTableWidgetItem(text);
        if (v.isDouble()) {
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            item->setForeground(QColor(v.toDouble() < 0 ? colors::NEGATIVE() : colors::CYAN()));
        } else if (v.isArray() || v.isObject()) {
            item->setToolTip(text.left(4000));
            if (text.size() > 200)
                item->setText(text.left(200) + QStringLiteral("…"));
        }
        return item;
    };

    if (section.isObject()) {
        const QJsonObject obj = section.toObject();
        table_->setColumnCount(2);
        table_->setHorizontalHeaderLabels({tr("Field"), tr("Value")});
        table_->setRowCount(int(obj.size()));
        int r = 0;
        for (auto it = obj.begin(); it != obj.end(); ++it, ++r) {
            auto* key_item = new QTableWidgetItem(it.key());
            key_item->setForeground(QColor(colors::TEXT_SECONDARY()));
            table_->setItem(r, 0, key_item);
            table_->setItem(r, 1, numeric_item(it.value()));
        }
    } else if (section.isArray()) {
        const QJsonArray arr = section.toArray();
        const int rows = std::min<int>(arr.size(), kQlTableRowCap);
        const bool objects = !arr.isEmpty() && std::all_of(arr.begin(), arr.end(), [](const QJsonValue& v) {
            return v.isObject();
        });
        const bool matrix = !arr.isEmpty() && !objects && std::all_of(arr.begin(), arr.end(), [](const QJsonValue& v) {
            return v.isArray();
        });
        if (objects) {
            const QStringList cols = ql_rv_object_columns(arr);
            table_->setColumnCount(int(cols.size()));
            table_->setHorizontalHeaderLabels(cols);
            table_->setRowCount(rows);
            for (int r = 0; r < rows; ++r) {
                const QJsonObject o = arr.at(r).toObject();
                for (int c = 0; c < cols.size(); ++c)
                    table_->setItem(r, c, numeric_item(o.value(cols.at(c))));
            }
        } else if (matrix) {
            int width = 0;
            for (int r = 0; r < rows; ++r)
                width = std::max(width, int(arr.at(r).toArray().size()));
            width = std::min(width, 200);
            QStringList headers{QStringLiteral("#")};
            for (int c = 0; c < width; ++c)
                headers << QString::number(c);
            table_->setColumnCount(width + 1);
            table_->setHorizontalHeaderLabels(headers);
            table_->setRowCount(rows);
            for (int r = 0; r < rows; ++r) {
                table_->setItem(r, 0, new ui::NumericTableWidgetItem(QString::number(r), r));
                const QJsonArray row = arr.at(r).toArray();
                for (int c = 0; c < width && c < row.size(); ++c)
                    table_->setItem(r, c + 1, numeric_item(row.at(c)));
            }
        } else {
            table_->setColumnCount(2);
            table_->setHorizontalHeaderLabels({QStringLiteral("#"), tr("Value")});
            table_->setRowCount(rows);
            for (int r = 0; r < rows; ++r) {
                table_->setItem(r, 0, new ui::NumericTableWidgetItem(QString::number(r), r));
                table_->setItem(r, 1, numeric_item(arr.at(r)));
            }
        }
    } else {
        table_->setColumnCount(1);
        table_->setHorizontalHeaderLabels({tr("Value")});
        table_->setRowCount(1);
        table_->setItem(0, 0, numeric_item(section));
    }
    table_->resizeColumnsToContents();
    table_->setSortingEnabled(true);
}

QList<QStringList> QuantLibResultView::section_rows(const QJsonValue& section, QStringList* headers) const {
    QList<QStringList> rows;
    headers->clear();
    if (section.isObject()) {
        *headers = {QStringLiteral("field"), QStringLiteral("value")};
        const QJsonObject obj = section.toObject();
        for (auto it = obj.begin(); it != obj.end(); ++it)
            rows.append({it.key(), ql_rv_cell(it.value(), true)});
        return rows;
    }
    if (!section.isArray()) {
        *headers = {QStringLiteral("value")};
        rows.append({ql_rv_cell(section, true)});
        return rows;
    }
    const QJsonArray arr = section.toArray();
    const bool objects =
        !arr.isEmpty() && std::all_of(arr.begin(), arr.end(), [](const QJsonValue& v) { return v.isObject(); });
    const bool matrix =
        !arr.isEmpty() && !objects && std::all_of(arr.begin(), arr.end(), [](const QJsonValue& v) { return v.isArray(); });
    if (objects) {
        *headers = ql_rv_object_columns(arr);
        for (const auto& v : arr) {
            const QJsonObject o = v.toObject();
            QStringList row;
            for (const QString& c : *headers)
                row << ql_rv_cell(o.value(c), true);
            rows.append(row);
        }
    } else if (matrix) {
        int width = 0;
        for (const auto& v : arr)
            width = std::max(width, int(v.toArray().size()));
        *headers = {QStringLiteral("row")};
        for (int c = 0; c < width; ++c)
            *headers << QString::number(c);
        int r = 0;
        for (const auto& v : arr) {
            QStringList row{QString::number(r++)};
            for (const auto& e : v.toArray())
                row << ql_rv_cell(e, true);
            rows.append(row);
        }
    } else {
        *headers = {QStringLiteral("index"), QStringLiteral("value")};
        int r = 0;
        for (const auto& v : arr)
            rows.append({QString::number(r++), ql_rv_cell(v, true)});
    }
    return rows;
}

// ── Chart ────────────────────────────────────────────────────────────────────

QList<QuantLibResultView::Series> QuantLibResultView::chart_series(const QJsonValue& section, QString* x_title) const {
    QList<Series> out;
    *x_title = tr("index");
    auto by_index = [](const QList<double>& ys) {
        QList<QPointF> pts;
        pts.reserve(ys.size());
        for (int i = 0; i < ys.size(); ++i)
            pts.append(QPointF(i, ys.at(i)));
        return pts;
    };
    auto against = [](const QList<double>& xs, const QList<double>& ys) {
        QList<QPointF> pts;
        pts.reserve(ys.size());
        for (int i = 0; i < ys.size() && i < xs.size(); ++i)
            pts.append(QPointF(xs.at(i), ys.at(i)));
        return pts;
    };
    static const QStringList kXKeys = {"times",  "time",   "t",       "tenors",     "tenor",      "x",
                                       "x_data", "query_x", "query_tenors", "strikes", "strike",  "maturities",
                                       "maturity", "grid", "t_grid",  "year",       "step",       "period",
                                       "volatility", "risk"};

    QList<double> ys;
    QList<QList<double>> mat;

    if (ql_rv_numeric_array(section, &ys)) {
        out.append({tr("value"), by_index(ys)});
        return out;
    }
    if (ql_rv_numeric_matrix(section, &mat)) {
        const bool pairs = mat.size() >= 2 && std::all_of(mat.begin(), mat.end(), [](const QList<double>& r) {
            return r.size() == 2;
        });
        if (pairs) {
            QList<QPointF> pts;
            for (const auto& r : mat)
                pts.append(QPointF(r.at(0), r.at(1)));
            *x_title = QStringLiteral("x");
            out.append({tr("y"), pts});
            return out;
        }
        for (int i = 0; i < mat.size() && out.size() < kQlChartSeriesCap; ++i) {
            if (mat.at(i).size() >= 2)
                out.append({tr("row %1").arg(i + 1), by_index(mat.at(i))});
        }
        return out;
    }
    if (section.isArray()) {
        // Array of objects: numeric columns.
        const QJsonArray arr = section.toArray();
        if (arr.size() < 2 || !arr.at(0).isObject())
            return out;
        QStringList numeric;
        for (const QString& c : ql_rv_object_columns(arr)) {
            const bool all_num = std::all_of(arr.begin(), arr.end(), [&c](const QJsonValue& v) {
                return v.toObject().value(c).isDouble();
            });
            if (all_num)
                numeric << c;
        }
        if (numeric.isEmpty())
            return out;
        QString x_key;
        for (const QString& k : kXKeys) {
            if (numeric.contains(k)) {
                x_key = k;
                break;
            }
        }
        if (x_key.isEmpty() && numeric.size() >= 2)
            x_key = numeric.first();
        QList<double> xs;
        if (!x_key.isEmpty()) {
            for (const auto& v : arr)
                xs.append(v.toObject().value(x_key).toDouble());
            *x_title = x_key;
        }
        for (const QString& k : numeric) {
            if (k == x_key || out.size() >= 6)
                continue;
            QList<double> col;
            for (const auto& v : arr)
                col.append(v.toObject().value(k).toDouble());
            out.append({k, x_key.isEmpty() ? by_index(col) : against(xs, col)});
        }
        return out;
    }
    if (!section.isObject())
        return out;

    // Object: numeric arrays plotted against an x-like array of the same length.
    const QJsonObject obj = section.toObject();
    QHash<QString, QList<double>> arrays;
    QHash<QString, QList<QList<double>>> matrices;
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        if (ql_rv_numeric_array(it.value(), &ys))
            arrays.insert(it.key(), ys);
        else if (ql_rv_numeric_matrix(it.value(), &mat))
            matrices.insert(it.key(), mat);
    }
    if (arrays.isEmpty() && matrices.isEmpty())
        return out;

    QString x_key;
    for (const QString& k : kXKeys) {
        if (arrays.contains(k)) {
            x_key = k;
            break;
        }
    }
    const QList<double> xs = x_key.isEmpty() ? QList<double>{} : arrays.value(x_key);

    // Without an x array, plot the length group with the most arrays so series
    // of unrelated lengths (e.g. one path + per-path final values) never share axes.
    int target_len = int(xs.size());
    if (x_key.isEmpty()) {
        QHash<int, int> counts;
        for (auto it = arrays.begin(); it != arrays.end(); ++it)
            counts[int(it.value().size())] += 1;
        int best = -1;
        for (auto it = counts.begin(); it != counts.end(); ++it) {
            if (best < 0 || it.value() > counts.value(best) || (it.value() == counts.value(best) && it.key() > best))
                best = it.key();
        }
        target_len = best;
    } else {
        *x_title = x_key;
    }

    QStringList keys = arrays.keys();
    keys.sort();
    for (const QString& k : keys) {
        if (k == x_key || out.size() >= kQlChartSeriesCap)
            continue;
        const QList<double>& col = arrays.value(k);
        if (int(col.size()) != target_len)
            continue;
        out.append({k, x_key.isEmpty() ? by_index(col) : against(xs, col)});
    }
    QStringList mkeys = matrices.keys();
    mkeys.sort();
    for (const QString& k : mkeys) {
        const auto& rows = matrices.value(k);
        if (!xs.isEmpty() && !rows.isEmpty() && int(rows.first().size()) == int(xs.size())) {
            // One row per path, sampled on the x grid.
            for (int i = 0; i < rows.size() && out.size() < kQlChartSeriesCap; ++i)
                out.append({rows.size() == 1 ? k : tr("%1 %2").arg(k).arg(i + 1), against(xs, rows.at(i))});
        } else if (!xs.isEmpty() && int(rows.size()) == int(xs.size()) && !rows.first().isEmpty() &&
                   rows.first().size() <= 10) {
            // One row per time step (multi-dimensional path): one series per dimension.
            const int dims = int(rows.first().size());
            for (int d = 0; d < dims && out.size() < kQlChartSeriesCap; ++d) {
                QList<double> col;
                for (const auto& r : rows)
                    col.append(d < r.size() ? r.at(d) : std::numeric_limits<double>::quiet_NaN());
                out.append({tr("%1[%2]").arg(k).arg(d), against(xs, col)});
            }
        }
    }
    return out;
}

void QuantLibResultView::render_chart(const QList<Series>& series, const QString& x_title) {
    auto* chart = new QChart();
    chart->setBackgroundBrush(QBrush(QColor(colors::BG_BASE())));
    chart->setPlotAreaBackgroundBrush(QBrush(QColor(colors::BG_BASE())));
    chart->setPlotAreaBackgroundVisible(true);
    chart->setMargins(QMargins(6, 6, 6, 6));
    chart->setTitleBrush(QBrush(QColor(colors::AMBER())));
    QFont title_font(fonts::DATA_FAMILY, 9, QFont::Bold);
    chart->setTitleFont(title_font);

    const QList<QColor> palette = {QColor(colors::AMBER()),   QColor(colors::CYAN()),     QColor(colors::POSITIVE()),
                                   QColor(colors::INFO()),    QColor(colors::WARNING()),  QColor(colors::NEGATIVE()),
                                   QColor(colors::TEXT_SECONDARY())};

    double min_x = std::numeric_limits<double>::max(), max_x = std::numeric_limits<double>::lowest();
    double min_y = min_x, max_y = max_x;
    bool downsampled = false;
    QList<QLineSeries*> lines;
    for (int i = 0; i < series.size(); ++i) {
        const QList<QPointF> pts = ql_rv_downsample(series.at(i).points, &downsampled);
        for (const QPointF& p : pts) {
            if (!std::isfinite(p.x()) || !std::isfinite(p.y()))
                continue;
            min_x = std::min(min_x, p.x());
            max_x = std::max(max_x, p.x());
            min_y = std::min(min_y, p.y());
            max_y = std::max(max_y, p.y());
        }
        auto* line = new QLineSeries(chart);
        line->setName(series.at(i).name);
        QPen pen(palette.at(i % palette.size()));
        pen.setWidthF(series.size() > 8 ? 1.0 : 1.6);
        line->setPen(pen);
        line->replace(pts); // one call per series — never point-by-point appends
        chart->addSeries(line);
        lines.append(line);
    }
    if (min_x > max_x) { // nothing finite to plot
        min_x = 0;
        max_x = 1;
        min_y = 0;
        max_y = 1;
    }
    if (min_x == max_x) {
        min_x -= 0.5;
        max_x += 0.5;
    }
    if (min_y == max_y) {
        const double pad = std::fabs(min_y) > 0 ? std::fabs(min_y) * 0.05 : 1.0;
        min_y -= pad;
        max_y += pad;
    } else {
        const double pad = (max_y - min_y) * 0.05;
        min_y -= pad;
        max_y += pad;
    }

    auto style_axis = [](QValueAxis* ax) {
        ax->setLabelsColor(QColor(colors::TEXT_TERTIARY()));
        ax->setGridLineColor(QColor(colors::BORDER_DIM()));
        ax->setLinePen(QPen(QColor(colors::BORDER_MED())));
        ax->setTitleBrush(QBrush(QColor(colors::TEXT_SECONDARY())));
        ax->setLabelFormat("%.4g");
        ax->setMinorTickCount(0);
    };
    auto* ax_x = new QValueAxis(chart);
    ax_x->setRange(min_x, max_x);
    ax_x->setTitleText(x_title);
    style_axis(ax_x);
    auto* ax_y = new QValueAxis(chart);
    ax_y->setRange(min_y, max_y);
    style_axis(ax_y);
    chart->addAxis(ax_x, Qt::AlignBottom);
    chart->addAxis(ax_y, Qt::AlignLeft);
    for (auto* line : lines) {
        line->attachAxis(ax_x);
        line->attachAxis(ax_y);
    }

    const bool legend = series.size() <= 8;
    chart->legend()->setVisible(legend);
    if (legend) {
        chart->legend()->setAlignment(Qt::AlignBottom);
        chart->legend()->setLabelColor(QColor(colors::TEXT_SECONDARY()));
    }
    QString title = series.size() == 1 ? series.first().name : tr("%1 series").arg(series.size());
    if (downsampled)
        title += tr(" · downsampled to %1 points").arg(kQlChartPointCap);
    chart->setTitle(title);

    QChart* old = chart_view_->chart();
    chart_view_->setChart(chart);
    delete old; // QChartView releases (does not delete) the previous chart
}

// ── JSON ─────────────────────────────────────────────────────────────────────

void QuantLibResultView::render_json() {
    QByteArray text;
    if (payload_.isObject())
        text = QJsonDocument(payload_.toObject()).toJson(QJsonDocument::Indented);
    else if (payload_.isArray())
        text = QJsonDocument(payload_.toArray()).toJson(QJsonDocument::Indented);
    else
        text = ql_rv_cell(payload_, true).toUtf8();
    if (text.size() > kQlJsonCharCap) {
        const qsizetype total = text.size();
        text.truncate(kQlJsonCharCap);
        text += "\n… " + tr("truncated: %1 of %2 bytes shown — use CSV to export everything")
                             .arg(kQlJsonCharCap)
                             .arg(total)
                             .toUtf8();
    }
    json_->setPlainText(QString::fromUtf8(text));
}

// ── Copy / export ────────────────────────────────────────────────────────────

void QuantLibResultView::copy_current() {
    if (!has_result_ || sections_.isEmpty())
        return;
    QClipboard* cb = QApplication::clipboard();
    if (view_ == View::Chart) {
        cb->setPixmap(chart_view_->grab());
        status_->setText(tr("Chart copied as an image"));
        return;
    }
    if (view_ == View::Json) {
        cb->setText(json_->toPlainText());
        status_->setText(tr("JSON copied"));
        return;
    }
    // Table: the selected cells when there is a selection, else the whole section
    // (from the data, so rows beyond the table's display cap are included).
    const auto ranges = table_->selectedRanges();
    QStringList lines;
    if (!ranges.isEmpty()) {
        const auto& r = ranges.first();
        for (int row = r.topRow(); row <= r.bottomRow(); ++row) {
            QStringList cells;
            for (int col = r.leftColumn(); col <= r.rightColumn(); ++col) {
                const QTableWidgetItem* it = table_->item(row, col);
                cells << (it ? (it->toolTip().isEmpty() ? it->text() : it->toolTip()) : QString());
            }
            lines << cells.join(QLatin1Char('\t'));
        }
    } else {
        const int idx = std::clamp(section_combo_->currentIndex(), 0, int(sections_.size()) - 1);
        QStringList headers;
        const QList<QStringList> rows = section_rows(sections_.at(idx).second, &headers);
        lines << headers.join(QLatin1Char('\t'));
        for (const auto& row : rows)
            lines << row.join(QLatin1Char('\t'));
    }
    cb->setText(lines.join(QLatin1Char('\n')));
    status_->setText(tr("%1 lines copied").arg(lines.size()));
}

void QuantLibResultView::export_csv() {
    if (!has_result_ || sections_.isEmpty())
        return;
    const int idx = std::clamp(section_combo_->currentIndex(), 0, int(sections_.size()) - 1);
    QString base = endpoint_;
    base.replace(QLatin1Char('/'), QLatin1Char('_'));
    const QString suggested = QStringLiteral("quantlib_%1_%2.csv")
                                  .arg(base, QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString path = QFileDialog::getSaveFileName(this, tr("Export CSV"), suggested, tr("CSV Files (*.csv)"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        status_->setText(tr("Could not write %1").arg(path));
        LOG_ERROR("QuantLib", QString("CSV export failed: cannot open %1").arg(path));
        return;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    // Provenance: an export without its inputs is not reproducible.
    out << "# endpoint=" << endpoint_ << "  section=" << sections_.at(idx).first
        << "  exported=" << QDateTime::currentDateTimeUtc().toString(Qt::ISODate) << "\n";
    out << "# request=" << QString::fromUtf8(QJsonDocument(request_).toJson(QJsonDocument::Compact)) << "\n";
    QStringList headers;
    const QList<QStringList> rows = section_rows(sections_.at(idx).second, &headers);
    QStringList escaped;
    for (const QString& h : headers)
        escaped << ql_rv_csv_escape(h);
    out << escaped.join(QLatin1Char(',')) << "\n";
    for (const auto& row : rows) {
        escaped.clear();
        for (const QString& c : row)
            escaped << ql_rv_csv_escape(c);
        out << escaped.join(QLatin1Char(',')) << "\n";
    }
    file.close();
    status_->setText(tr("Exported %1 rows").arg(rows.size()));
    LOG_INFO("QuantLib", QString("Exported %1 rows of %2 to CSV").arg(rows.size()).arg(endpoint_));
}

} // namespace fincept::screens
