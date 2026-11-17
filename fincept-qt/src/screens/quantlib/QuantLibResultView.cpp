// src/screens/quantlib/QuantLibResultView.cpp
//
// Results pane of the QuantLib screen — see the header. The SUMMARY page is the
// shared ui::ResultView (one renderer for all 590 endpoints: the response's
// shape decides KPIs / chart / table / sections). Toolbar and TABLE / JSON
// pages are styled by the QuantLib screen's root stylesheet (#ql* selectors);
// nothing here calls setStyleSheet. Table colours are read from the theme
// tokens at paint time, so a theme change only repaints.
//
// GUI-thread budget: the TABLE page is a virtual model over the JSON (no item
// per cell, every row available), built only when shown; the JSON text is
// formatted on a worker thread when first shown; CSV export writes on a worker
// thread. A capped simulation answer (several MB) therefore costs the GUI
// thread only the summary.

#include "screens/quantlib/QuantLibResultView.h"

#include "core/logging/Logger.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/LoadingOverlay.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/ResultLayout.h"
#include "ui/widgets/ResultView.h"

#include <QAbstractTableModel>
#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeySequence>
#include <QLabel>
#include <QLocale>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QStyle>
#include <QTableView>
#include <QTextStream>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

namespace fincept::screens {

using namespace fincept::ui;

namespace {

constexpr int kQlJsonCharCap = 1'000'000;   // JSON page text (COPY copies all of it)
constexpr int kQlSectionDepth = 3;          // nested objects listed in the VIEW selector
constexpr int kQlMaxColumns = 1000;         // widest matrix row shown in the TABLE page
constexpr int kQlSortableRows = 50'000;     // above this a header click would sort for too long
constexpr int kQlAutoSizeColumns = 50;      // fit column widths to content up to this many columns
constexpr int kQlSortRole = Qt::UserRole;
constexpr int kQlFullTextRole = Qt::UserRole + 1;

const result::Options& ql_rv_plain_options() {
    static const result::Options o;
    return o;
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

// Cell text: `full` = export precision; otherwise significant digits by
// magnitude with group separators, and "not finite" for a null number.
QString ql_rv_cell(const QJsonValue& v, bool full) {
    if (v.isDouble())
        return full ? ql_rv_number_full(v.toDouble())
                    : result::format_number(v.toDouble(), result::Unit::Plain, ql_rv_plain_options());
    if (v.isBool())
        return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (v.isNull() || v.isUndefined())
        return full ? QString() : result::format_value(QJsonValue(QJsonValue::Null), result::Unit::Plain,
                                                       ql_rv_plain_options());
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

// Column keys of an array of objects, in first-appearance order.
QStringList ql_rv_object_columns(const QJsonArray& arr) {
    QStringList cols;
    const int scan = std::min<int>(arr.size(), 500);
    for (int i = 0; i < scan; ++i) {
        const QJsonObject o = arr.at(i).toObject();
        for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
            if (!cols.contains(it.key()))
                cols << it.key();
        }
    }
    return cols;
}

bool ql_rv_all_objects(const QJsonArray& arr) {
    return !arr.isEmpty() && std::all_of(arr.begin(), arr.end(), [](const QJsonValue& v) { return v.isObject(); });
}

bool ql_rv_all_arrays(const QJsonArray& arr) {
    return !arr.isEmpty() && std::all_of(arr.begin(), arr.end(), [](const QJsonValue& v) { return v.isArray(); });
}

// "rate" is an interest rate on most endpoints, but an FX rate on money/convert
// and an event intensity on the exponential / poisson endpoints: never a percent there.
result::Unit ql_rv_unit(const QString& endpoint, const QString& key) {
    if (key == QLatin1String("rate") &&
        (endpoint == QLatin1String("core/types/money/convert") || endpoint.contains(QLatin1String("/exponential/")) ||
         endpoint.contains(QLatin1String("/poisson/"))))
        return result::Unit::Plain;
    return result::Unit::Auto;
}

// Asset names in the request label per-asset arrays (weights, matrices).
QStringList ql_rv_labels(const QJsonObject& request) {
    static const QStringList kKeys = {"assets", "tickers", "symbols", "asset_names", "names", "labels"};
    for (const QString& k : kKeys) {
        const QJsonArray a = request.value(k).toArray();
        if (a.isEmpty())
            continue;
        QStringList out;
        for (const QJsonValue& v : a) {
            if (!v.isString())
                return {};
            out << v.toString();
        }
        return out;
    }
    return {};
}

// Full rows of `section` as text, with column headers. Pure: runs on the CSV worker.
QList<QStringList> ql_rv_section_rows(const QJsonValue& section, QStringList* headers) {
    QList<QStringList> rows;
    headers->clear();
    if (section.isObject()) {
        *headers = {QStringLiteral("field"), QStringLiteral("value")};
        const QJsonObject obj = section.toObject();
        for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
            rows.append({it.key(), ql_rv_cell(it.value(), true)});
        return rows;
    }
    if (!section.isArray()) {
        *headers = {QStringLiteral("value")};
        rows.append({ql_rv_cell(section, true)});
        return rows;
    }
    const QJsonArray arr = section.toArray();
    if (ql_rv_all_objects(arr)) {
        *headers = ql_rv_object_columns(arr);
        for (const auto& v : arr) {
            const QJsonObject o = v.toObject();
            QStringList row;
            for (const QString& c : *headers)
                row << ql_rv_cell(o.value(c), true);
            rows.append(row);
        }
    } else if (ql_rv_all_arrays(arr)) {
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

// Indented JSON of the payload. Pure: runs on the JSON worker.
QByteArray ql_rv_json_text(const QJsonValue& payload) {
    if (payload.isObject())
        return QJsonDocument(payload.toObject()).toJson(QJsonDocument::Indented);
    if (payload.isArray())
        return QJsonDocument(payload.toArray()).toJson(QJsonDocument::Indented);
    return ql_rv_cell(payload, true).toUtf8();
}

struct QlCsvJob {
    QString path;
    QString endpoint;
    QString section_name;
    QJsonObject request;
    QJsonValue section;
};

// Writes one section to CSV. Pure apart from the file: runs on the CSV worker.
// Returns the rows written, or -1 when the file could not be written.
int ql_rv_write_csv(const QlCsvJob& job) {
    QFile file(job.path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return -1;
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    // Provenance: an export without its inputs is not reproducible.
    out << "# endpoint=" << job.endpoint << "  section=" << job.section_name
        << "  exported=" << QDateTime::currentDateTimeUtc().toString(Qt::ISODate) << "\n";
    out << "# request=" << QString::fromUtf8(QJsonDocument(job.request).toJson(QJsonDocument::Compact)) << "\n";
    QStringList headers;
    const QList<QStringList> rows = ql_rv_section_rows(job.section, &headers);
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
    out.flush();
    file.close();
    return file.error() == QFileDevice::NoError ? int(rows.size()) : -1;
}

} // namespace

// ── Table model ──────────────────────────────────────────────────────────────
//
// A read-only grid over one section of the payload. Cells are produced on
// demand (only the visible ones are ever formatted), so a 100 000-row series
// costs no more to show than a 10-row one.

class QlJsonTableModel : public QAbstractTableModel {
  public:
    using QAbstractTableModel::QAbstractTableModel;

    void set_section(const QJsonValue& section) {
        beginResetModel();
        obj_ = QJsonObject();
        keys_.clear();
        arr_ = QJsonArray();
        columns_.clear();
        scalar_ = QJsonValue();
        if (section.isObject()) {
            shape_ = Shape::Object;
            obj_ = section.toObject();
            keys_ = obj_.keys();
            columns_ = {QuantLibResultView::tr("Field"), QuantLibResultView::tr("Value")};
        } else if (section.isArray()) {
            arr_ = section.toArray();
            if (ql_rv_all_objects(arr_)) {
                shape_ = Shape::Objects;
                columns_ = ql_rv_object_columns(arr_);
            } else if (ql_rv_all_arrays(arr_)) {
                shape_ = Shape::Matrix;
                int width = 0;
                for (const auto& v : arr_)
                    width = std::max(width, int(v.toArray().size()));
                columns_ << QStringLiteral("#");
                for (int c = 0; c < std::min(width, kQlMaxColumns); ++c)
                    columns_ << QString::number(c);
            } else {
                shape_ = Shape::List;
                columns_ = {QStringLiteral("#"), QuantLibResultView::tr("Value")};
            }
        } else {
            shape_ = Shape::Scalar;
            scalar_ = section;
            columns_ = {QuantLibResultView::tr("Value")};
        }
        endResetModel();
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        if (parent.isValid())
            return 0;
        switch (shape_) {
            case Shape::Object:
                return int(keys_.size());
            case Shape::Objects:
            case Shape::Matrix:
            case Shape::List:
                return int(arr_.size());
            case Shape::Scalar:
                return 1;
        }
        return 0;
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : int(columns_.size());
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
        if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section >= 0 && section < columns_.size())
            return columns_.at(section);
        return {};
    }

    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid())
            return {};
        const int row = index.row();
        const int col = index.column();

        // Label cells: the field name (object) or the row number (matrix / list).
        const bool key_cell = shape_ == Shape::Object && col == 0;
        const bool index_cell = (shape_ == Shape::Matrix || shape_ == Shape::List) && col == 0;
        if (key_cell || index_cell) {
            switch (role) {
                case Qt::DisplayRole:
                case kQlFullTextRole:
                    return key_cell ? keys_.at(row) : QString::number(row);
                case kQlSortRole:
                    return key_cell ? QVariant(keys_.at(row)) : QVariant(row);
                case Qt::ForegroundRole:
                    return QColor(colors::TEXT_SECONDARY());
                case Qt::TextAlignmentRole:
                    return index_cell ? QVariant(int(Qt::AlignRight | Qt::AlignVCenter)) : QVariant();
                default:
                    return {};
            }
        }

        const QJsonValue v = value_at(row, col);
        switch (role) {
            case Qt::DisplayRole: {
                const QString text = ql_rv_cell(v, false);
                return (v.isArray() || v.isObject()) && text.size() > 200 ? text.left(200) + QStringLiteral("…")
                                                                          : text;
            }
            case kQlFullTextRole:
                return ql_rv_cell(v, true);
            case kQlSortRole:
                if (v.isDouble())
                    return v.toDouble();
                if (v.isNull() || v.isUndefined())
                    return std::numeric_limits<double>::lowest();
                return ql_rv_cell(v, false);
            case Qt::ToolTipRole:
                if (v.isDouble())
                    return ql_rv_number_full(v.toDouble());
                if (v.isNull())
                    return QuantLibResultView::tr("The server returned null: the value is not a finite number "
                                                  "(±∞ or NaN) for these inputs.");
                if (v.isArray() || v.isObject())
                    return ql_rv_compact(v).left(4000);
                return {};
            case Qt::TextAlignmentRole:
                if (v.isDouble() || v.isNull())
                    return int(Qt::AlignRight | Qt::AlignVCenter);
                return {};
            case Qt::ForegroundRole:
                return QColor(v.isNull() ? colors::TEXT_TERTIARY() : colors::TEXT_PRIMARY());
            default:
                return {};
        }
    }

  private:
    enum class Shape { Object, Objects, Matrix, List, Scalar };

    QJsonValue value_at(int row, int col) const {
        switch (shape_) {
            case Shape::Object:
                return obj_.value(keys_.at(row));
            case Shape::Objects:
                return arr_.at(row).toObject().value(columns_.at(col));
            case Shape::Matrix: {
                const QJsonArray r = arr_.at(row).toArray();
                return col - 1 < r.size() ? r.at(col - 1) : QJsonValue(QJsonValue::Undefined);
            }
            case Shape::List:
                return arr_.at(row);
            case Shape::Scalar:
                return scalar_;
        }
        return {};
    }

    Shape shape_ = Shape::Scalar;
    QJsonObject obj_;
    QStringList keys_;
    QJsonArray arr_;
    QStringList columns_;
    QJsonValue scalar_;
};

// ── Construction ─────────────────────────────────────────────────────────────

QuantLibResultView::QuantLibResultView(QWidget* parent) : QWidget(parent) {
    setObjectName("qlResultPanel");
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Toolbar: RESULTS · endpoint ····· status · SUMMARY TABLE JSON · COPY CSV
    auto* bar = new QWidget(this);
    bar->setObjectName("qlResultBar");
    bar->setMinimumHeight(34);
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
    summary_btn_ = make_btn(tr("SUMMARY"), "qlViewBtn");
    summary_btn_->setToolTip(tr("Headline values, charts and tables built from the response"));
    table_btn_ = make_btn(tr("TABLE"), "qlViewBtn");
    table_btn_->setToolTip(tr("Any part of the response as a plain grid"));
    json_btn_ = make_btn(tr("JSON"), "qlViewBtn");
    auto* group = new QButtonGroup(this);
    group->setExclusive(true);
    for (auto* b : {summary_btn_, table_btn_, json_btn_}) {
        b->setCheckable(true);
        group->addButton(b);
        bl->addWidget(b);
    }
    summary_btn_->setChecked(true);
    connect(summary_btn_, &QPushButton::clicked, this, [this]() { set_view(View::Summary); });
    connect(table_btn_, &QPushButton::clicked, this, [this]() { set_view(View::Table); });
    connect(json_btn_, &QPushButton::clicked, this, [this]() { set_view(View::Json); });

    copy_btn_ = make_btn(tr("COPY"), "qlFormBtn");
    copy_btn_->setToolTip(tr("Copy the current view (summary as an image, table as tab-separated text)"));
    connect(copy_btn_, &QPushButton::clicked, this, &QuantLibResultView::copy_current);
    bl->addWidget(copy_btn_);
    csv_btn_ = make_btn(tr("CSV"), "qlFormBtn");
    csv_btn_->setToolTip(tr("Export the selected section — every row — to a CSV file"));
    connect(csv_btn_, &QPushButton::clicked, this, &QuantLibResultView::export_csv);
    bl->addWidget(csv_btn_);
    vl->addWidget(bar);

    // Section selector (TABLE view, when the response has arrays / nested objects).
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

    // Idle / cancelled / preview states: the shared designed empty state, so the
    // pane is never a blank area with one grey line.
    empty_ = new ui::EmptyState(stack_);
    empty_->setAccessibleName(tr("QuantLib endpoint preview"));
    connect(empty_, &ui::EmptyState::primary_clicked, this, [this]() { dispatch_empty_action(primary_action_); });
    connect(empty_, &ui::EmptyState::secondary_clicked, this,
            [this]() { dispatch_empty_action(secondary_action_); });
    stack_->addWidget(empty_); // kMessagePage

    summary_ = new ui::ResultView(this);
    summary_->set_raw_toggle_visible(false); // the toolbar has JSON
    summary_->setAccessibleName(tr("QuantLib result summary"));
    stack_->addWidget(summary_); // kSummaryPage

    table_model_ = new QlJsonTableModel(this);
    table_proxy_ = new QSortFilterProxyModel(this);
    table_proxy_->setSourceModel(table_model_);
    table_proxy_->setSortRole(kQlSortRole);
    table_ = new QTableView(this);
    table_->setModel(table_proxy_);
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setResizeContentsPrecision(200);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectItems);
    table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->setWordWrap(false);
    table_->setAccessibleName(tr("QuantLib result table"));
    stack_->addWidget(table_); // kTablePage

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
    error_text_->setTextFormat(Qt::PlainText);
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
    error_retry_->setProperty("fincept_agent_risk", "spend"); // agents: repeats a billed QuantLib API call
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

    // Loading with nothing to keep on screen: a skeleton of the result to come.
    skeleton_ = new ui::LoadingSkeleton(ui::LoadingSkeleton::Shape::Result, stack_);
    stack_->addWidget(skeleton_); // kSkeletonPage

    vl->addWidget(stack_, 1);

    // Shared loading overlay over the pages while a call is in flight.
    overlay_ = new ui::LoadingOverlay(stack_);

    // Ctrl+C on the table copies the selection (or the whole section).
    auto* copy_sc = new QShortcut(QKeySequence::Copy, table_);
    copy_sc->setContext(Qt::WidgetShortcut);
    connect(copy_sc, &QShortcut::activated, this, &QuantLibResultView::copy_current);

    // The summary page re-renders itself; table colours are read at paint time.
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this,
            [this](const ThemeTokens&) { table_->viewport()->update(); });

    show_idle();
}

void QuantLibResultView::retranslate() {
    title_->setText(tr("RESULTS"));
    summary_btn_->setText(tr("SUMMARY"));
    summary_btn_->setToolTip(tr("Headline values, charts and tables built from the response"));
    table_btn_->setText(tr("TABLE"));
    table_btn_->setToolTip(tr("Any part of the response as a plain grid"));
    json_btn_->setText(tr("JSON"));
    copy_btn_->setText(tr("COPY"));
    copy_btn_->setToolTip(tr("Copy the current view (summary as an image, table as tab-separated text)"));
    csv_btn_->setText(tr("CSV"));
    csv_btn_->setToolTip(tr("Export the selected section — every row — to a CSV file"));
    section_label_->setText(tr("VIEW"));
    error_retry_->setText(tr("RETRY"));
    if (has_failure_) {
        show_failure(endpoint_, failure_);
    } else if (has_result_) {
        build_sections();
        table_gen_ = 0; // translated headers
        render_current();
    } else if (has_preview_) {
        show_preview(preview_); // the screen re-sends it with translated texts
    } else if (stack_->currentIndex() != kSkeletonPage) {
        show_idle();
    }
}

// ── States ───────────────────────────────────────────────────────────────────

void QuantLibResultView::set_result_controls_enabled(bool enabled) {
    for (auto* b : {summary_btn_, table_btn_, json_btn_, copy_btn_, csv_btn_})
        b->setEnabled(enabled);
    if (!enabled)
        section_combo_->parentWidget()->hide();
}

void QuantLibResultView::stop_loading() {
    overlay_->hide_loading();
}

void QuantLibResultView::show_empty(const QString& glyph, const QString& title, const QString& message,
                                    const QList<QPair<QString, QString>>& details, const QString& details_heading,
                                    const QString& note, ui::EmptyState::Tone note_tone, const QString& primary_text,
                                    EmptyAction primary, const QString& secondary_text, EmptyAction secondary) {
    stop_loading();
    set_result_controls_enabled(false);
    primary_action_ = primary;
    secondary_action_ = secondary;
    empty_->set_glyph(glyph, ui::EmptyState::Tone::Accent);
    empty_->set_title(title);
    empty_->set_message(message);
    empty_->set_details(details, details_heading);
    empty_->set_note(note, note_tone);
    empty_->set_primary_action(primary == EmptyAction::None ? QString() : primary_text);
    empty_->set_secondary_action(secondary == EmptyAction::None ? QString() : secondary_text);
    // A preview carries a faint still sketch of the result it would produce.
    if (has_preview_)
        empty_->set_preview_shape(ui::LoadingSkeleton::Shape::Result);
    else
        empty_->clear_preview_shape();
    stack_->setCurrentIndex(kMessagePage);
}

void QuantLibResultView::dispatch_empty_action(EmptyAction action) {
    switch (action) {
        case EmptyAction::Run:
            emit retry_requested();
            break;
        case EmptyAction::Upgrade:
            emit upgrade_requested();
            break;
        case EmptyAction::None:
            break;
    }
}

void QuantLibResultView::show_idle() {
    has_result_ = false;
    has_failure_ = false;
    has_preview_ = false;
    as_of_ = QDateTime();
    refreshing_ = false;
    endpoint_label_->clear();
    status_->clear();
    show_empty(QStringLiteral("\u03A3"), tr("QuantLib suite"),
               tr("Pick a module and an endpoint on the left. Calculations that are free on your plan run on their "
                  "own with sample inputs; the others show what they return and what a run costs first."),
               {}, QString(), QString(), ui::EmptyState::Tone::Neutral, QString(), EmptyAction::None, QString(),
               EmptyAction::None);
}

void QuantLibResultView::show_preview(const Preview& preview) {
    has_result_ = false;
    has_failure_ = false;
    has_preview_ = true;
    as_of_ = QDateTime();
    refreshing_ = false;
    preview_ = preview;
    endpoint_ = preview.endpoint;
    endpoint_label_->setText(preview.endpoint);
    status_->setText(preview.status);
    const QString run = preview.run_text.isEmpty() ? tr("RUN") : preview.run_text;
    if (preview.upgrade_primary) {
        show_empty(preview.glyph, preview.title, preview.summary, preview.inputs, preview.inputs_heading, preview.note,
                   preview.note_tone, tr("UPGRADE TO PRO"), EmptyAction::Upgrade, run, EmptyAction::Run);
    } else {
        show_empty(preview.glyph, preview.title, preview.summary, preview.inputs, preview.inputs_heading, preview.note,
                   preview.note_tone, run, EmptyAction::Run, preview.plans_secondary ? tr("VIEW PLANS") : QString(),
                   preview.plans_secondary ? EmptyAction::Upgrade : EmptyAction::None);
    }
}

void QuantLibResultView::show_loading(const QString& endpoint, const QString& cost_note) {
    has_failure_ = false;
    has_preview_ = false;
    refreshing_ = false;
    endpoint_label_->setText(endpoint);
    status_->setText(cost_note.isEmpty() ? tr("Computing… (Esc cancels)")
                                         : tr("Computing… (%1 · Esc cancels)").arg(cost_note));
    set_result_controls_enabled(false);
    const QString caption = tr("COMPUTING %1").arg(endpoint.section(QLatin1Char('/'), -1).toUpper());
    // A previous result of this endpoint stays visible (dimmed) under the
    // overlay; with nothing to keep, a skeleton of the result to come.
    const int page = stack_->currentIndex();
    const bool result_on_screen = has_result_ && endpoint == endpoint_ &&
                                  (page == kSummaryPage || page == kTablePage || page == kJsonPage);
    if (result_on_screen) {
        overlay_->show_loading(caption);
        return;
    }
    has_result_ = false;
    endpoint_ = endpoint;
    overlay_->hide_loading();
    skeleton_->set_message(cost_note.isEmpty() ? caption + QStringLiteral("…")
                                               : tr("%1… · %2").arg(caption, cost_note));
    stack_->setCurrentIndex(kSkeletonPage);
}

void QuantLibResultView::set_refreshing(bool refreshing) {
    refreshing_ = refreshing;
    if (has_result_)
        update_status();
}

bool QuantLibResultView::show_cancelled(const QString& endpoint) {
    stop_loading();
    refreshing_ = false;
    if (has_result_) {
        // Back to the previous answer, which never went away.
        endpoint_label_->setText(endpoint_);
        set_result_controls_enabled(true);
        render_current();
        status_->setText(tr("Cancelled %1 — showing the previous result").arg(endpoint.section(QLatin1Char('/'), -1)));
        return true;
    }
    endpoint_label_->setText(endpoint);
    status_->setText(tr("Cancelled"));
    return false;
}

void QuantLibResultView::show_input_error(const QString& message) {
    has_failure_ = false;
    has_preview_ = false;
    status_->setText(tr("Not sent"));
    show_error_page(tr("INVALID INPUT"), QStringLiteral("error"), message,
                    tr("Nothing was sent — fix the highlighted field and run again."), QString(), false);
}

void QuantLibResultView::show_error_page(const QString& title, const QString& tone, const QString& message,
                                         const QString& hint, const QString& primary_text, bool show_retry) {
    stop_loading();
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
    if (failure.kind == Kind::Cancelled) {
        show_cancelled(endpoint);
        return;
    }
    endpoint_ = endpoint;
    failure_ = failure;
    has_failure_ = true;
    has_result_ = false;
    has_preview_ = false;
    refreshing_ = false;
    as_of_ = QDateTime();
    endpoint_label_->setText(endpoint);
    status_->setText(failure.http_status > 0 ? tr("HTTP %1").arg(failure.http_status) : tr("Failed"));

    switch (failure.kind) {
        case Kind::NeedsPro:
            show_error_page(tr("THIS MODULE NEEDS PRO"), QStringLiteral("pro"), failure.message,
                            // Plan price: always USD, never converted.
                            tr("Pro (%1/month) includes every QuantLib module — all endpoints at 0 credits per "
                               "call.")
                                .arg(fincept::ui::formatting::plan_price(10)),
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
        case Kind::Blocked:
            show_error_page(tr("ACCESS PAUSED"), QStringLiteral("warn"), failure.message,
                            tr("Requests from this network are paused after repeated invalid requests. Wait a few "
                               "minutes, then retry."),
                            QString(), true);
            break;
        case Kind::InvalidInput:
            show_error_page(tr("INVALID INPUT"), QStringLiteral("error"), failure.message,
                            tr("The server rejected the request. Check the named fields and their units (decimals: "
                               "0.05 = 5%)."),
                            QString(), false);
            break;
        case Kind::Network:
            show_error_page(tr("NETWORK ERROR"), QStringLiteral("error"), failure.message,
                            tr("The request did not reach the QuantLib service. Check your connection and retry."),
                            QString(), true);
            break;
        case Kind::Timeout:
            show_error_page(tr("TIMED OUT"), QStringLiteral("warn"), failure.message,
                            tr("The calculation did not finish in time. Reduce the size of the inputs (paths, steps, "
                               "points) and run again, or retry."),
                            QString(), true);
            break;
        case Kind::Server:
            show_error_page(tr("SERVICE ERROR"), QStringLiteral("error"), failure.message,
                            failure.retryable
                                ? tr("The QuantLib service did not return a usable answer. Retry in a moment.")
                                : tr("This terminal and the QuantLib service disagree about this endpoint. Update "
                                     "the terminal or report it."),
                            QString(), failure.retryable);
            break;
        case Kind::Computation:
            show_error_page(tr("COMPUTATION FAILED"), QStringLiteral("error"), failure.message,
                            tr("The model could not be evaluated with these inputs. Adjust the parameters and run "
                               "again."),
                            QString(), false);
            break;
        case Kind::Cancelled:
            break; // handled above
    }
}

void QuantLibResultView::show_result(const QString& endpoint, const QJsonObject& request, const QJsonValue& payload,
                                     qint64 elapsed_ms, const QDateTime& as_of) {
    stop_loading();
    endpoint_ = endpoint;
    request_ = request;
    payload_ = payload;
    elapsed_ms_ = elapsed_ms;
    as_of_ = as_of;
    refreshing_ = false;
    has_result_ = true;
    has_failure_ = false;
    has_preview_ = false;
    ++result_gen_; // the TABLE / JSON pages are rebuilt when next shown
    json_full_.clear();
    endpoint_label_->setText(endpoint);
    set_result_controls_enabled(true);
    build_sections();
    render_summary();
    // A new result always opens on its summary.
    view_ = View::Summary;
    render_current();
}

// ── Sections ─────────────────────────────────────────────────────────────────

void QuantLibResultView::build_sections() {
    sections_.clear();
    if (payload_.isObject()) {
        sections_.append({tr("Top-level fields"), payload_});
        // Every array and nested object (recursively, "parent / child") is its own grid.
        std::function<void(const QJsonObject&, const QString&, int)> add = [&](const QJsonObject& obj,
                                                                                  const QString& prefix, int depth) {
            for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
                const QJsonValue v = it.value();
                const QString name = prefix.isEmpty() ? it.key() : prefix + QStringLiteral(" / ") + it.key();
                if (v.isArray() && !v.toArray().isEmpty()) {
                    sections_.append({name, v});
                } else if (v.isObject() && !v.toObject().isEmpty()) {
                    sections_.append({name, v});
                    if (depth < kQlSectionDepth)
                        add(v.toObject(), name, depth + 1);
                }
            }
        };
        add(payload_.toObject(), QString(), 1);
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
}

int QuantLibResultView::current_section() const {
    return std::clamp(section_combo_->currentIndex(), 0, std::max(0, int(sections_.size()) - 1));
}

void QuantLibResultView::render_summary() {
    result::Options o;
    o.request = request_;
    const QString ep = endpoint_;
    o.unit_for = [ep](const QString&, const QString& key, double) { return ql_rv_unit(ep, key); };
    o.series_labels = ql_rv_labels(request_);
    // Sampling / exact-simulation endpoints return draws: show them as distributions.
    o.samples_default = ep.contains(QLatin1String("sampling/")) || ep.contains(QLatin1String("/exact/")) ||
                        ep.contains(QLatin1String("winsor")) || ep.contains(QLatin1String("bootstrap"));
    summary_->set_options(o);
    // The raw payload argument only fills ResultView's own RAW page, which is hidden here (the toolbar's JSON
    // view replaces it, formatted off the GUI thread). Passing the payload would make ResultView indent it and
    // load it into a text editor on the GUI thread for nothing: ~90 ms plus the editor for a 3.7 MB answer.
    summary_->show_result(payload_, QJsonValue(QJsonObject{}));
}

void QuantLibResultView::update_status() {
    // Status: size of what is shown + round-trip time.
    const QJsonValue shown = view_ == View::Table ? sections_.at(current_section()).second : payload_;
    QString size_text;
    if (shown.isArray()) {
        const int n = int(shown.toArray().size());
        size_text = n == 0 ? tr("no rows") : tr("%1 rows").arg(n);
    } else if (shown.isObject()) {
        size_text = tr("%1 fields").arg(shown.toObject().size());
    } else {
        size_text = tr("1 value");
    }
    if (!as_of_.isValid()) {
        status_->setText(tr("%1 · %2 ms").arg(size_text).arg(elapsed_ms_));
        return;
    }
    // A remembered result: say when it was computed (and that a fresh copy is on its way).
    const QDateTime local = as_of_.toLocalTime();
    const QString when = local.date() == QDate::currentDate()
                             ? QLocale().toString(local.time(), QLocale::ShortFormat)
                             : QLocale().toString(local, QLocale::ShortFormat);
    status_->setText(refreshing_ ? tr("%1 · as of %2 · refreshing…").arg(size_text, when)
                                 : tr("%1 · as of %2").arg(size_text, when));
}

void QuantLibResultView::render_current() {
    if (!has_result_ || sections_.isEmpty())
        return;
    update_status();
    set_view(view_);
}

void QuantLibResultView::set_view(View view) {
    view_ = view;
    const QSignalBlocker b1(summary_btn_), b2(table_btn_), b3(json_btn_);
    summary_btn_->setChecked(view == View::Summary);
    table_btn_->setChecked(view == View::Table);
    json_btn_->setChecked(view == View::Json);
    if (!has_result_)
        return;
    section_combo_->parentWidget()->setVisible(view == View::Table && sections_.size() > 1);
    if (view == View::Table)
        ensure_table();
    else if (view == View::Json)
        ensure_json();
    update_status();
    stack_->setCurrentIndex(view == View::Summary ? kSummaryPage : view == View::Table ? kTablePage : kJsonPage);
}

// ── Table ────────────────────────────────────────────────────────────────────

void QuantLibResultView::ensure_table() {
    const int idx = current_section();
    if (table_gen_ == result_gen_ && table_section_ == idx)
        return;
    table_gen_ = result_gen_;
    table_section_ = idx;

    table_->setSortingEnabled(false);
    table_->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    table_proxy_->sort(-1);
    table_model_->set_section(sections_.at(idx).second);
    // Sorting a very long section on a header click would stall the GUI thread.
    table_->setSortingEnabled(table_model_->rowCount() <= kQlSortableRows);
    table_->horizontalHeader()->setToolTip(table_model_->rowCount() > kQlSortableRows
                                               ? tr("Sorting is off above %L1 rows — export to CSV to sort.")
                                                     .arg(kQlSortableRows)
                                               : QString());
    if (table_model_->columnCount() <= kQlAutoSizeColumns)
        table_->resizeColumnsToContents(); // samples the first rows only (resizeContentsPrecision)
    table_->scrollToTop();
}

// ── JSON ─────────────────────────────────────────────────────────────────────

void QuantLibResultView::ensure_json() {
    if (json_gen_ == result_gen_)
        return; // shown, or still formatting
    json_gen_ = result_gen_;
    json_full_.clear();
    json_->setPlainText(tr("Formatting…"));

    const quint64 gen = result_gen_;
    auto* watcher = new QFutureWatcher<QByteArray>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, gen]() {
        QByteArray text = watcher->result();
        watcher->deleteLater();
        if (gen != result_gen_ || gen != json_gen_)
            return; // a newer result arrived meanwhile
        json_full_ = text;
        if (text.size() > kQlJsonCharCap) {
            const qsizetype total = text.size();
            text.truncate(kQlJsonCharCap);
            text += "\n… " + tr("truncated: %1 of %2 bytes shown — COPY copies all of it, CSV exports a section")
                                 .arg(kQlJsonCharCap)
                                 .arg(total)
                                 .toUtf8();
        }
        json_->setPlainText(QString::fromUtf8(text));
    });
    const QJsonValue payload = payload_;
    watcher->setFuture(QtConcurrent::run([payload]() { return ql_rv_json_text(payload); }));
}

// ── Copy / export ────────────────────────────────────────────────────────────

void QuantLibResultView::copy_current() {
    if (!has_result_ || sections_.isEmpty())
        return;
    QClipboard* cb = QApplication::clipboard();
    if (view_ == View::Summary) {
        cb->setPixmap(summary_->content_widget()->grab());
        status_->setText(tr("Summary copied as an image"));
        return;
    }
    if (view_ == View::Json) {
        if (json_full_.isEmpty()) {
            status_->setText(tr("Still formatting the JSON — copy again in a moment"));
            return;
        }
        cb->setText(QString::fromUtf8(json_full_));
        status_->setText(tr("JSON copied"));
        return;
    }
    // Table: the selected cells when there is a selection, else the whole section.
    QStringList lines;
    const QItemSelection selection = table_->selectionModel() ? table_->selectionModel()->selection()
                                                              : QItemSelection();
    if (!selection.isEmpty()) {
        const QItemSelectionRange& r = selection.first();
        for (int row = r.top(); row <= r.bottom(); ++row) {
            QStringList cells;
            for (int col = r.left(); col <= r.right(); ++col)
                cells << table_proxy_->index(row, col).data(kQlFullTextRole).toString();
            lines << cells.join(QLatin1Char('\t'));
        }
    } else {
        QStringList headers;
        const QList<QStringList> rows = ql_rv_section_rows(sections_.at(current_section()).second, &headers);
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
    // From SUMMARY / JSON the whole top level is exported; from TABLE the selected section.
    const int idx = view_ == View::Table ? current_section() : 0;
    QString base = endpoint_;
    base.replace(QLatin1Char('/'), QLatin1Char('_'));
    const QString suggested = QStringLiteral("quantlib_%1_%2.csv")
                                  .arg(base, QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString path = QFileDialog::getSaveFileName(this, tr("Export CSV"), suggested, tr("CSV Files (*.csv)"));
    if (path.isEmpty())
        return;

    QlCsvJob job{path, endpoint_, sections_.at(idx).first, request_, sections_.at(idx).second};
    csv_btn_->setEnabled(false);
    status_->setText(tr("Exporting…"));
    auto* watcher = new QFutureWatcher<int>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, path, endpoint = endpoint_]() {
        const int rows = watcher->result();
        watcher->deleteLater();
        csv_btn_->setEnabled(has_result_);
        if (rows < 0) {
            status_->setText(tr("Could not write %1").arg(path));
            LOG_ERROR("QuantLib", QString("CSV export failed: cannot write %1").arg(path));
            return;
        }
        status_->setText(tr("Exported %1 rows").arg(rows));
        LOG_INFO("QuantLib", QString("Exported %1 rows of %2 to CSV").arg(rows).arg(endpoint));
    });
    watcher->setFuture(QtConcurrent::run([job]() { return ql_rv_write_csv(job); }));
}

} // namespace fincept::screens
