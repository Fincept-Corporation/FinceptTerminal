// DataWorkspace.cpp — see DataWorkspace.h.

#include "screens/data_sources/DataWorkspace.h"

#include "services/data_mapping/MappingInbox.h"
#include "services/data_mapping/MappingRunner.h"
#include "ui/charts/CandleChart.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/ThemeHelpers.h"
#include "ui/widgets/report/AddToReportButton.h"

#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStyle>
#include <QTabWidget>
#include <QTableView>
#include <QTimeZone>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::screens::datasources {

namespace ds = fincept::services::datasources;
namespace dm = fincept::services::datamapping;
namespace col = fincept::ui::colors;
namespace fnt = fincept::ui::fonts;

namespace {

constexpr int kDwMaxColumnWidth = 320;
constexpr int kDwReportRows = 200;
constexpr int kDwTabTable = 0;
constexpr int kDwTabChart = 1;

/// Epoch ms of a date / timestamp cell: ISO text, or epoch seconds / ms (numbers above 1e8).
double dw_time_ms(const QJsonValue& v) {
    if (v.isDouble()) {
        const double d = v.toDouble();
        if (!std::isfinite(d) || d < 1e8)
            return std::numeric_limits<double>::quiet_NaN();
        return d < 1e11 ? d * 1000.0 : d;
    }
    if (!v.isString())
        return std::numeric_limits<double>::quiet_NaN();
    const QString s = v.toString().trimmed();
    QDateTime dt = QDateTime::fromString(s, Qt::ISODate);
    if (!dt.isValid() && s.size() >= 10) {
        const QDate d = QDate::fromString(s.left(10), Qt::ISODate);
        if (d.isValid())
            dt = QDateTime(d, QTime(0, 0), QTimeZone::UTC);
    }
    return dt.isValid() ? static_cast<double>(dt.toMSecsSinceEpoch()) : std::numeric_limits<double>::quiet_NaN();
}

QString dw_default_dir() {
    const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return docs.isEmpty() ? QDir::homePath() : docs;
}

QString dw_safe_file_stem(QString s) {
    static const QRegularExpression kBad(QStringLiteral("[^A-Za-z0-9._-]+"));
    s.replace(kBad, QStringLiteral("_"));
    return s.isEmpty() ? QStringLiteral("data") : s.left(80);
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Style + small helpers
// ─────────────────────────────────────────────────────────────────────────────

QString workspace_qss() {
    const QString f_h1 = fnt::px(2);
    const QString f_text = fnt::px(-1);
    const QString f_small = fnt::px(-2);
    const QString f_tiny = fnt::px(-3);
    return QStringLiteral(
               "#dwRoot { background: %1; }"
               "QWidget[dw=\"pane\"] { background: %2; }"
               "QWidget[dw=\"bar\"] { background: %3; border-bottom: 1px solid %4; }"
               "QWidget[dw=\"card\"] { background: %2; border: 1px solid %4; }"
               "QLabel { background: transparent; }"
               "QLabel[dw=\"h1\"] { color: %5; font-size: %20px; font-weight: 700; }"
               "QLabel[dw=\"title\"] { color: %6; font-size: %22px; font-weight: 700; letter-spacing: 0.5px; }"
               "QLabel[dw=\"text\"] { color: %5; font-size: %21px; }"
               "QLabel[dw=\"muted\"] { color: %7; font-size: %22px; }"
               "QLabel[dw=\"note\"] { color: %7; font-size: %23px; }"
               "QLabel[dw=\"field\"] { color: %7; font-size: %22px; }"
               "QLabel[dw=\"ok\"] { color: %8; font-size: %22px; font-weight: 700; }"
               "QLabel[dw=\"error\"] { color: %9; font-size: %22px; font-weight: 700; }"
               "QLabel[dw=\"warn\"] { color: %10; font-size: %22px; font-weight: 700; }"
               "QLabel[dw=\"idle\"] { color: %7; font-size: %22px; font-weight: 700; }"
               "QLabel[dw=\"banner\"] { color: %5; font-size: %22px; padding: 6px 10px; background: %11; "
               "  border-left: 3px solid %10; }"
               "QLabel[dw=\"banner_info\"] { color: %5; font-size: %22px; padding: 6px 10px; background: %12; "
               "  border-left: 3px solid %13; }"
               "QLabel[dw=\"banner_error\"] { color: %5; font-size: %22px; padding: 6px 10px; background: %14; "
               "  border-left: 3px solid %9; }"
               "QLabel[dw=\"code\"] { color: %6; background: %15; border: 1px solid %16; font-weight: 700; "
               "  font-size: %21px; }"
               "QLineEdit, QPlainTextEdit, QComboBox, QSpinBox { background: %1; color: %5; border: 1px solid %16; "
               "  padding: 4px 8px; font-size: %21px; selection-background-color: %15; selection-color: %5; }"
               "QLineEdit:focus, QPlainTextEdit:focus, QComboBox:focus, QSpinBox:focus { border-color: %6; }"
               "QLineEdit[dw=\"bad\"], QComboBox[dw=\"bad\"] { border-color: %9; }"
               "QComboBox QAbstractItemView { background: %2; color: %5; selection-background-color: %15; "
               "  selection-color: %5; border: 1px solid %16; }"
               "QPushButton, QToolButton[dw=\"button\"] { background: %3; color: %5; border: 1px solid %16; "
               "  padding: 5px 12px; font-size: %22px; font-weight: 700; }"
               "QPushButton:hover, QToolButton[dw=\"button\"]:hover { background: %17; border-color: %18; }"
               "QPushButton:disabled, QToolButton[dw=\"button\"]:disabled { color: %19; border-color: %4; }"
               "QPushButton[dw=\"primary\"] { background: %6; color: %24; border-color: %6; }"
               "QPushButton[dw=\"primary\"]:hover { background: %25; border-color: %25; }"
               "QPushButton[dw=\"primary\"]:disabled { background: %3; color: %19; border-color: %4; }"
               "QPushButton[dw=\"danger\"] { background: transparent; color: %9; border-color: %9; }"
               "QPushButton[dw=\"danger\"]:hover { background: %14; }"
               "QPushButton[dw=\"link\"] { background: transparent; border: none; color: %13; padding: 2px 4px; "
               "  text-decoration: underline; }"
               "QCheckBox, QRadioButton { color: %7; font-size: %22px; background: transparent; }"
               "QTableView { background: %2; alternate-background-color: %26; color: %5; gridline-color: %4; "
               "  border: none; font-size: %21px; selection-background-color: %15; selection-color: %5; }"
               "QHeaderView::section { background: %3; color: %7; border: none; border-right: 1px solid %4; "
               "  border-bottom: 1px solid %4; padding: 4px 6px; font-size: %23px; font-weight: 700; }"
               "QTabWidget::pane { border: none; border-top: 1px solid %4; }"
               "QTabBar::tab { background: transparent; color: %7; padding: 6px 14px; font-size: %22px; "
               "  font-weight: 700; border-bottom: 2px solid transparent; }"
               "QTabBar::tab:selected { color: %6; border-bottom-color: %6; }"
               "QTabBar::tab:disabled { color: %19; }"
               "QSplitter::handle { background: %4; }"
               "QScrollArea { background: transparent; border: none; }"
               "QScrollArea > QWidget > QWidget { background: transparent; }")
        .arg(col::BG_BASE())                      // %1
        .arg(col::BG_SURFACE())                   // %2
        .arg(col::BG_RAISED())                    // %3
        .arg(col::BORDER_DIM())                   // %4
        .arg(col::TEXT_PRIMARY())                 // %5
        .arg(col::AMBER())                        // %6
        .arg(col::TEXT_SECONDARY())               // %7
        .arg(col::SUCCESS())                      // %8
        .arg(col::DANGER())                       // %9
        .arg(col::WARNING())                      // %10
        .arg(col::tint(QColor(col::WARNING()), 0.12)) // %11
        .arg(col::tint(QColor(col::INFO()), 0.12))    // %12
        .arg(col::INFO())                         // %13
        .arg(col::tint(QColor(col::DANGER()), 0.12))  // %14
        .arg(col::ACCENT_BG())                    // %15
        .arg(col::BORDER_MED())                   // %16
        .arg(col::BG_HOVER())                     // %17
        .arg(col::BORDER_BRIGHT())                // %18
        .arg(col::TEXT_TERTIARY())                // %19
        .arg(f_h1)                                // %20
        .arg(f_text)                              // %21
        .arg(f_small)                             // %22
        .arg(f_tiny)                              // %23
        .arg(col::TEXT_ON_ACCENT())               // %24
        .arg(col::AMBER_DIM())                    // %25
        .arg(col::ROW_ALT());                     // %26
}

QString amp_safe(const QString& text) {
    QString t = text;
    t.replace(QLatin1Char('&'), QStringLiteral("&&"));
    return t;
}

void set_look(QWidget* w, const char* look) {
    if (!w)
        return;
    const QVariant v(QString::fromLatin1(look));
    if (w->property("dw") == v)
        return;
    const bool polished = w->testAttribute(Qt::WA_WState_Polished);
    w->setProperty("dw", v);
    if (polished) {
        w->style()->unpolish(w);
        w->style()->polish(w);
    }
}

QString relative_time_text(const QString& utc_sql_time) {
    if (utc_sql_time.isEmpty())
        return QString();
    QDateTime dt = QDateTime::fromString(utc_sql_time, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!dt.isValid())
        dt = QDateTime::fromString(utc_sql_time, Qt::ISODate);
    if (!dt.isValid())
        return utc_sql_time;
    dt.setTimeZone(QTimeZone::UTC);
    const qint64 secs = dt.secsTo(QDateTime::currentDateTimeUtc());
    if (secs < 45)
        return QObject::tr("just now");
    if (secs < 3600)
        return QObject::tr("%1 min ago").arg(std::max<qint64>(1, secs / 60));
    if (secs < 24 * 3600)
        return QObject::tr("%1 h ago").arg(secs / 3600);
    return dt.toLocalTime().toString(QStringLiteral("d MMM HH:mm"));
}

ErrorPage error_page(const ds::SourceError& e, bool catalogue) {
    using Tone = fincept::ui::EmptyState::Tone;
    ErrorPage p;
    p.spec.glyph = QStringLiteral("!");
    p.spec.glyph_tone = Tone::Danger;
    p.spec.title = e.title();
    p.spec.message = e.message.isEmpty() ? QObject::tr("The source did not answer.") : e.message;
    if (!e.docs_url.isEmpty())
        p.spec.note = QObject::tr("More: %1").arg(e.docs_url);
    switch (e.code) {
        case ds::ErrorCode::MissingKey:
            p.spec.glyph_tone = Tone::Warning;
            p.env_key = e.env_keys.value(0);
            if (!e.env_keys.isEmpty()) {
                for (const QString& k : e.env_keys)
                    p.spec.details.append({QObject::tr("Key"), k});
                p.spec.details_heading = QObject::tr("NEEDED");
            }
            p.spec.primary = QObject::tr("ADD KEY");
            p.primary = ErrorAction::AddKey;
            p.spec.secondary = QObject::tr("RETRY");
            p.secondary = ErrorAction::Retry;
            break;
        case ds::ErrorCode::AuthFailed:
            p.env_key = e.env_keys.value(0);
            p.spec.primary = catalogue ? QObject::tr("CHECK KEY") : QObject::tr("EDIT SETTINGS");
            p.primary = catalogue ? ErrorAction::AddKey : ErrorAction::EditSettings;
            p.spec.secondary = QObject::tr("RETRY");
            p.secondary = ErrorAction::Retry;
            break;
        case ds::ErrorCode::RateLimited:
            p.spec.glyph_tone = Tone::Warning;
            p.spec.primary = QObject::tr("RETRY");
            p.primary = ErrorAction::Retry;
            p.retry_after_s = e.retry_after_s > 0 ? std::min(e.retry_after_s, 3600) : 30;
            if (p.spec.note.isEmpty())
                p.spec.note = QObject::tr("The service limits how often it may be called");
            break;
        case ds::ErrorCode::UpstreamDown:
            p.spec.glyph_tone = Tone::Warning;
            p.spec.primary = QObject::tr("RETRY");
            p.primary = ErrorAction::Retry;
            break;
        case ds::ErrorCode::BadArgs:
            p.spec.glyph_tone = Tone::Warning;
            if (!e.field.isEmpty())
                p.spec.details = {{QObject::tr("Field"), e.field}};
            p.spec.primary = catalogue ? QObject::tr("EDIT INPUTS") : QObject::tr("EDIT SETTINGS");
            p.primary = catalogue ? ErrorAction::EditInputs : ErrorAction::EditSettings;
            break;
        case ds::ErrorCode::NoData:
            p.spec.glyph = QStringLiteral("∅");
            p.spec.glyph_tone = Tone::Neutral;
            p.spec.title = QObject::tr("No rows for this request");
            p.spec.primary = catalogue ? QObject::tr("EDIT INPUTS") : QString();
            p.primary = catalogue ? ErrorAction::EditInputs : ErrorAction::None;
            p.spec.secondary = QObject::tr("RETRY");
            p.secondary = ErrorAction::Retry;
            break;
        case ds::ErrorCode::NotSupported:
            p.spec.glyph = QStringLiteral("×");
            p.spec.glyph_tone = Tone::Neutral;
            if (!e.fix.isEmpty()) {
                p.spec.note = e.fix;
                p.spec.note_tone = Tone::Info;
            } else if (e.message.contains(QLatin1String("402")) ||
                       e.message.contains(QLatin1String("payment required"), Qt::CaseInsensitive)) {
                // HTTP 402 is classified not_supported by the connector wrapper: a paid endpoint.
                p.spec.title = QObject::tr("Needs a paid plan");
                p.spec.note = QObject::tr("This endpoint is paid; the terminal uses free data only. Try another command.");
                p.spec.note_tone = Tone::Info;
            }
            break;
        case ds::ErrorCode::None:
        case ds::ErrorCode::Unknown:
            p.spec.title = QObject::tr("Could not load");
            p.spec.primary = QObject::tr("RETRY");
            p.primary = ErrorAction::Retry;
            break;
    }
    return p;
}

void open_credentials(const QString& env_key, QWidget* context) {
    fincept::ui::SetupLink link;
    link.screen_id = QStringLiteral("settings");
    link.section = QStringLiteral("credentials");
    link.focus = env_key;
    fincept::ui::open_setup_link(link, context);
}

// ─────────────────────────────────────────────────────────────────────────────
// TabularTableModel
// ─────────────────────────────────────────────────────────────────────────────

TabularTableModel::TabularTableModel(QObject* parent) : QAbstractTableModel(parent) {}

void TabularTableModel::set_table(const ds::DataTable& table) {
    beginResetModel();
    table_ = table;
    marks_.clear();
    marked_rows_.clear();
    endResetModel();
}

void TabularTableModel::set_marks(const QHash<qint64, CellMark>& marks) {
    marks_ = marks;
    marked_rows_.clear();
    for (auto it = marks_.constBegin(); it != marks_.constEnd(); ++it)
        marked_rows_.insert(int(it.key() >> 20));
    if (rowCount() > 0)
        emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
}

void TabularTableModel::clear() {
    set_table(ds::DataTable());
}

void TabularTableModel::refresh_colours() {
    if (rowCount() > 0)
        emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1),
                         {Qt::ForegroundRole, Qt::BackgroundRole});
}

int TabularTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : table_.row_count();
}

int TabularTableModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : table_.column_count();
}

QVariant TabularTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid())
        return {};
    const QJsonValue v = table_.cell(index.row(), index.column());
    const auto mark = marks_.constFind(mark_key(index.row(), index.column()));
    const bool marked = mark != marks_.constEnd();
    switch (role) {
        case Qt::DisplayRole: {
            if (marked && (v.isNull() || v.isUndefined()))
                return mark->raw.isEmpty() ? QStringLiteral("⚠") : QStringLiteral("⚠ ") + mark->raw;
            QString t = ds::cell_text(v);
            if (t.size() > 300)
                t = t.left(300) + QStringLiteral("…");
            return t;
        }
        case Qt::ToolTipRole:
            if (marked)
                return mark->message;
            if (v.isString() && v.toString().size() > 60)
                return v.toString().left(2000);
            return {};
        case Qt::TextAlignmentRole:
            return v.isDouble() ? QVariant(Qt::AlignRight | Qt::AlignVCenter) : QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case Qt::ForegroundRole:
            if (marked)
                return QColor(col::DANGER());
            if (v.isNull() || v.isUndefined())
                return QColor(col::TEXT_TERTIARY());
            return {};
        case Qt::BackgroundRole:
            if (marked)
                return col::tint_color(QColor(col::DANGER()), 0.16);
            return {};
        case kSortRole:
            if (v.isDouble())
                return v.toDouble();
            if (v.isBool())
                return v.toBool() ? 1 : 0;
            return ds::cell_text(v);
        case kMarkedRole:
            return marked;
        default:
            return {};
    }
}

QVariant TabularTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Vertical) {
        if (role == Qt::DisplayRole)
            return section + 1;
        return {};
    }
    if (section < 0 || section >= table_.column_count())
        return {};
    if (role == Qt::DisplayRole)
        return table_.columns.at(section);
    if (role == Qt::ToolTipRole) {
        const QString t = ds::column_type_name(table_.types.value(section));
        return t.isEmpty() ? table_.columns.at(section) : QStringLiteral("%1 · %2").arg(table_.columns.at(section), t);
    }
    return {};
}

// ─────────────────────────────────────────────────────────────────────────────
// TabularFilterProxy
// ─────────────────────────────────────────────────────────────────────────────

TabularFilterProxy::TabularFilterProxy(QObject* parent) : QSortFilterProxyModel(parent) {
    setSortRole(TabularTableModel::kSortRole);
}

void TabularFilterProxy::set_text(const QString& text) {
    terms_ = text.toLower().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    invalidateRowsFilter();
}

void TabularFilterProxy::set_marked_only(bool on) {
    marked_only_ = on;
    invalidateRowsFilter();
}

bool TabularFilterProxy::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const {
    auto* m = qobject_cast<TabularTableModel*>(sourceModel());
    if (!m)
        return true;
    if (marked_only_ && !m->row_marked(source_row))
        return false;
    if (terms_.isEmpty())
        return true;
    QString row_text;
    const int cols = m->columnCount(source_parent);
    for (int c = 0; c < cols; ++c)
        row_text += ds::cell_text(m->table().cell(source_row, c)).toLower() + QLatin1Char(' ');
    for (const QString& t : terms_)
        if (!row_text.contains(t))
            return false;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// ResultPane
// ─────────────────────────────────────────────────────────────────────────────

ResultPane::ResultPane(const QString& report_source, QWidget* parent) : QWidget(parent), report_source_(report_source) {
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* bar = new QWidget(this);
    set_look(bar, "bar");
    bar->setAttribute(Qt::WA_StyledBackground, true);
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(8, 4, 8, 4);
    hl->setSpacing(6);
    table_pick_ = new QComboBox(bar);
    table_pick_->setToolTip(tr("This result has several tables — pick the one to show"));
    table_pick_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    table_pick_->hide();
    hl->addWidget(table_pick_);
    info_ = new QLabel(bar);
    set_look(info_, "muted");
    hl->addWidget(info_);
    hl->addStretch(1);
    filter_ = new QLineEdit(bar);
    filter_->setPlaceholderText(tr("filter rows…"));
    filter_->setClearButtonEnabled(true);
    filter_->setMaximumWidth(fontMetrics().horizontalAdvance(QLatin1Char('M')) * 14);
    hl->addWidget(filter_);
    export_btn_ = new QToolButton(bar);
    set_look(export_btn_, "button");
    export_btn_->setText(tr("EXPORT"));
    export_btn_->setPopupMode(QToolButton::InstantPopup);
    auto* export_menu = new QMenu(export_btn_);
    export_menu->addAction(tr("CSV…"), this, &ResultPane::export_csv);
    export_menu->addAction(tr("JSON…"), this, &ResultPane::export_json);
    export_btn_->setMenu(export_menu);
    hl->addWidget(export_btn_);
    to_mapping_btn_ = new QPushButton(tr("SEND TO DATA MAPPING"), bar);
    to_mapping_btn_->setToolTip(tr("Open these rows in Data Mapping to map them onto typed fields"));
    to_mapping_btn_->setEnabled(false);
    connect(to_mapping_btn_, &QPushButton::clicked, this, &ResultPane::send_to_mapping);
    hl->addWidget(to_mapping_btn_);
    to_report_ = new fincept::ui::AddToReportButton(report_source_, bar);
    to_report_->set_payload_provider([this] { return report_payload(); });
    hl->addWidget(to_report_);
    vl->addWidget(bar);

    note_ = new QLabel(this);
    set_look(note_, "banner_info");
    note_->setWordWrap(true);
    note_->hide();
    vl->addWidget(note_);

    tabs_ = new QTabWidget(this);
    tabs_->setDocumentMode(true);
    model_ = new TabularTableModel(this);
    proxy_ = new TabularFilterProxy(this);
    proxy_->setSourceModel(model_);
    view_ = new QTableView(tabs_);
    view_->setModel(proxy_);
    view_->setSortingEnabled(true);
    view_->setAlternatingRowColors(true);
    view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view_->setWordWrap(false);
    view_->horizontalHeader()->setStretchLastSection(true);
    view_->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    view_->verticalHeader()->setVisible(false);
    fincept::ui::metrics::follow_row_height(view_, 24);
    tabs_->addTab(view_, tr("TABLE"));

    auto* chart_page = new QWidget(tabs_);
    auto* cvl = new QVBoxLayout(chart_page);
    cvl->setContentsMargins(8, 6, 8, 6);
    cvl->setSpacing(6);
    auto* chl = new QHBoxLayout;
    auto* chart_lbl = new QLabel(tr("SERIES"), chart_page);
    set_look(chart_lbl, "field");
    chl->addWidget(chart_lbl);
    chart_column_ = new QComboBox(chart_page);
    chart_column_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    chl->addWidget(chart_column_);
    chl->addStretch(1);
    cvl->addLayout(chl);
    chart_stack_ = new QStackedWidget(chart_page);
    line_chart_ = new fincept::ui::TimeSeriesChart(chart_stack_);
    candle_chart_ = new fincept::ui::CandleChart(chart_stack_);
    chart_message_ = new QLabel(chart_stack_);
    set_look(chart_message_, "muted");
    chart_message_->setAlignment(Qt::AlignCenter);
    chart_message_->setWordWrap(true);
    chart_stack_->addWidget(line_chart_);
    chart_stack_->addWidget(candle_chart_);
    chart_stack_->addWidget(chart_message_);
    cvl->addWidget(chart_stack_, 1);
    tabs_->addTab(chart_page, tr("CHART"));
    vl->addWidget(tabs_, 1);

    connect(table_pick_, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (i >= 0)
            show_table(table_pick_->itemData(i).toInt());
    });
    connect(filter_, &QLineEdit::textChanged, proxy_, &TabularFilterProxy::set_text);
    connect(chart_column_, &QComboBox::currentIndexChanged, this, [this](int) { render_chart(); });
    connect(&fincept::ui::ThemeManager::instance(), &fincept::ui::ThemeManager::theme_changed, model_,
            [this](const fincept::ui::ThemeTokens&) { model_->refresh_colours(); });
    clear();
}

void ResultPane::set_result(const ds::TabularResult& result, const QString& title) {
    const QString previous = current_table_name();
    result_ = result;
    title_ = title;
    has_result_ = !result.tables.isEmpty();
    {
        const QSignalBlocker block(table_pick_);
        table_pick_->clear();
        for (int i = 0; i < result_.tables.size(); ++i) {
            const ds::DataTable& t = result_.tables[i];
            const QString name = t.name.isEmpty() ? tr("table %1").arg(i + 1) : t.name;
            table_pick_->addItem(tr("%1 · %2 rows").arg(name).arg(t.row_count()), i);
        }
        table_pick_->setVisible(result_.tables.size() > 1);
    }
    int idx = previous.isEmpty() ? -1 : result_.index_of(previous);
    if (idx < 0)
        idx = 0;
    {
        const QSignalBlocker block(table_pick_);
        table_pick_->setCurrentIndex(idx);
    }
    export_btn_->setEnabled(has_result_);
    to_report_->setEnabled(has_result_);
    show_table(idx);
}

void ResultPane::clear() {
    result_ = ds::TabularResult();
    has_result_ = false;
    table_index_ = 0;
    {
        const QSignalBlocker block(table_pick_);
        table_pick_->clear();
        table_pick_->hide();
    }
    model_->clear();
    info_->setText(tr("No rows yet"));
    export_btn_->setEnabled(false);
    to_report_->setEnabled(false);
    to_mapping_btn_->setEnabled(false);
    set_note(QString());
    update_chart_view();
}

QString ResultPane::current_table_name() const {
    const ds::DataTable* t = current_table();
    return t ? t->name : QString();
}

const ds::DataTable* ResultPane::current_table() const {
    if (!has_result_ || table_index_ < 0 || table_index_ >= result_.tables.size())
        return nullptr;
    return &result_.tables[table_index_];
}

void ResultPane::set_mapping_source(const dm::SourceRef& source, const QString& label) {
    mapping_source_ = source;
    mapping_label_ = label;
    to_mapping_btn_->setEnabled(has_result_ && !source.kind.isEmpty());
}

void ResultPane::set_note(const QString& text) {
    note_->setText(text);
    note_->setVisible(!text.isEmpty());
}

void ResultPane::show_table(int index) {
    table_index_ = index;
    const ds::DataTable* t = current_table();
    view_->setSortingEnabled(false);
    model_->set_table(t ? *t : ds::DataTable());
    proxy_->sort(-1);
    view_->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    view_->setSortingEnabled(true);
    // Size columns from a sample of rows only (never a full pass over a big table).
    if (t && t->row_count() > 0) {
        const QFontMetrics fm(view_->font());
        for (int c = 0; c < t->column_count(); ++c) {
            int w = fm.horizontalAdvance(t->columns.at(c)) + 24;
            for (int r = 0; r < std::min(t->row_count(), 40); ++r)
                w = std::max(w, fm.horizontalAdvance(ds::cell_text(t->cell(r, c)).left(60)) + 16);
            view_->setColumnWidth(c, std::min(w, kDwMaxColumnWidth));
        }
    }
    if (t) {
        QString text = tr("%1 rows × %2 columns").arg(t->row_count()).arg(t->column_count());
        if (result_.truncated)
            text += tr(" · more available at the source");
        if (result_.as_of.isValid())
            text += tr(" · as of %1").arg(result_.as_of.toLocalTime().toString(QStringLiteral("d MMM HH:mm")));
        info_->setText(text);
    } else {
        info_->setText(has_result_ ? tr("Empty result") : tr("No rows yet"));
    }
    to_mapping_btn_->setEnabled(has_result_ && !mapping_source_.kind.isEmpty());
    update_chart_view();
    emit table_changed(t ? t->name : QString());
}

void ResultPane::update_chart_view() {
    chart_time_col_ = -1;
    chart_value_cols_.clear();
    chart_has_ohlc_ = false;
    const ds::DataTable* t = current_table();
    const int rows = t ? t->row_count() : 0;
    const int probe = std::min(rows, 50);
    auto parses_as_time = [&](int c) {
        int ok = 0, seen = 0;
        for (int r = 0; r < probe; ++r) {
            const QJsonValue v = t->cell(r, c);
            if (v.isNull() || v.isUndefined())
                continue;
            ++seen;
            if (std::isfinite(dw_time_ms(v)))
                ++ok;
        }
        return seen > 0 && ok * 10 >= seen * 9;
    };
    if (t) {
        static const QRegularExpression kTimeName(QStringLiteral("date|time|period|^ts$|day"),
                                                  QRegularExpression::CaseInsensitiveOption);
        for (int c = 0; c < t->column_count() && chart_time_col_ < 0; ++c) {
            const ds::ColumnType ty = t->types.value(c);
            if ((ty == ds::ColumnType::Date || ty == ds::ColumnType::DateTime) && parses_as_time(c))
                chart_time_col_ = c;
        }
        for (int c = 0; c < t->column_count() && chart_time_col_ < 0; ++c)
            if (kTimeName.match(t->columns.at(c)).hasMatch() && parses_as_time(c))
                chart_time_col_ = c;
        if (chart_time_col_ >= 0) {
            for (int c = 0; c < t->column_count(); ++c) {
                if (c == chart_time_col_)
                    continue;
                int num = 0, seen = 0;
                for (int r = 0; r < probe; ++r) {
                    const QJsonValue v = t->cell(r, c);
                    if (v.isNull() || v.isUndefined())
                        continue;
                    ++seen;
                    if (v.isDouble())
                        ++num;
                }
                if (seen > 0 && num * 10 >= seen * 9)
                    chart_value_cols_.append(c);
            }
        }
    }
    auto has_col = [&](const char* name) {
        for (int c : chart_value_cols_)
            if (t->columns.at(c).compare(QLatin1String(name), Qt::CaseInsensitive) == 0)
                return true;
        return false;
    };
    chart_has_ohlc_ = t && has_col("open") && has_col("high") && has_col("low") && has_col("close");
    const bool chartable = t && chart_time_col_ >= 0 && !chart_value_cols_.isEmpty() && rows >= 2;
    tabs_->setTabEnabled(kDwTabChart, chartable);
    tabs_->setTabToolTip(kDwTabChart, chartable ? QString() : tr("Needs a date / timestamp column and a numeric column"));
    if (!chartable && tabs_->currentIndex() == kDwTabChart)
        tabs_->setCurrentIndex(kDwTabTable);
    {
        const QSignalBlocker block(chart_column_);
        chart_column_->clear();
        if (chartable) {
            if (chart_has_ohlc_)
                chart_column_->addItem(tr("OHLC (candles)"), -1);
            int preferred = -1;
            for (int c : chart_value_cols_) {
                chart_column_->addItem(t->columns.at(c), c);
                if (preferred < 0 && (t->columns.at(c).compare(QLatin1String("close"), Qt::CaseInsensitive) == 0 ||
                                      t->columns.at(c).compare(QLatin1String("value"), Qt::CaseInsensitive) == 0))
                    preferred = chart_column_->count() - 1;
            }
            chart_column_->setCurrentIndex(chart_has_ohlc_ ? 0 : std::max(0, preferred));
        }
    }
    render_chart();
}

void ResultPane::render_chart() {
    const ds::DataTable* t = current_table();
    if (!t || chart_time_col_ < 0 || chart_column_->count() == 0) {
        chart_message_->setText(tr("No date / timestamp column with numeric values in this result."));
        chart_stack_->setCurrentWidget(chart_message_);
        return;
    }
    const int rows = t->row_count();
    const int value_col = chart_column_->currentData().toInt();
    const QString export_name = dw_safe_file_stem(title_);
    if (value_col < 0) {
        auto col_of = [&](const char* name) {
            for (int c : chart_value_cols_)
                if (t->columns.at(c).compare(QLatin1String(name), Qt::CaseInsensitive) == 0)
                    return c;
            return -1;
        };
        const int o = col_of("open"), h = col_of("high"), l = col_of("low"), c = col_of("close"), v = col_of("volume");
        QVector<fincept::ui::CandleChart::Bar> bars;
        bars.reserve(rows);
        const double nan = std::numeric_limits<double>::quiet_NaN();
        for (int r = 0; r < rows; ++r) {
            fincept::ui::CandleChart::Bar b;
            b.time_ms = dw_time_ms(t->cell(r, chart_time_col_));
            b.open = t->cell(r, o).toDouble(nan);
            b.high = t->cell(r, h).toDouble(nan);
            b.low = t->cell(r, l).toDouble(nan);
            b.close = t->cell(r, c).toDouble(nan);
            b.volume = v >= 0 ? t->cell(r, v).toDouble(0) : 0.0;
            if (std::isfinite(b.time_ms))
                bars.append(b);
        }
        std::sort(bars.begin(), bars.end(), [](const auto& a, const auto& b) { return a.time_ms < b.time_ms; });
        candle_chart_->set_bars(bars);
        candle_chart_->set_show_volume(v >= 0);
        candle_chart_->set_title(title_);
        candle_chart_->set_export_name(export_name);
        candle_chart_->reset_view();
        chart_stack_->setCurrentWidget(candle_chart_);
        return;
    }
    fincept::ui::TimeSeriesChart::SeriesSpec spec;
    spec.name = t->columns.value(value_col);
    spec.points.reserve(rows);
    for (int r = 0; r < rows; ++r) {
        const double x = dw_time_ms(t->cell(r, chart_time_col_));
        const QJsonValue y = t->cell(r, value_col);
        if (std::isfinite(x) && y.isDouble())
            spec.points.append(QPointF(x, y.toDouble()));
    }
    std::sort(spec.points.begin(), spec.points.end(), [](const QPointF& a, const QPointF& b) { return a.x() < b.x(); });
    line_chart_->clear();
    if (spec.points.size() < 2) {
        line_chart_->set_state(fincept::ui::TimeSeriesChart::State::Empty, tr("Fewer than two dated values in this column"));
    } else {
        line_chart_->set_state(fincept::ui::TimeSeriesChart::State::Ready);
        line_chart_->add_series(spec);
        line_chart_->reset_x_range();
    }
    line_chart_->set_title(spec.name);
    line_chart_->set_export_name(export_name);
    chart_stack_->setCurrentWidget(line_chart_);
}

void ResultPane::export_csv() {
    const ds::DataTable* t = current_table();
    if (!t)
        return;
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export CSV"), QDir(dw_default_dir()).filePath(dw_safe_file_stem(title_) + QStringLiteral(".csv")),
        tr("CSV files (*.csv);;All files (*)"));
    if (path.isEmpty())
        return;
    QPointer<ResultPane> self(this);
    dm::MappingRunner::instance().export_csv(*t, path, this, [self, path](const ds::SourceError& e) {
        if (self)
            self->set_note(e.ok() ? tr("Saved %1").arg(QDir::toNativeSeparators(path)) : e.message);
    });
}

void ResultPane::export_json() {
    const ds::DataTable* t = current_table();
    if (!t)
        return;
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export JSON"), QDir(dw_default_dir()).filePath(dw_safe_file_stem(title_) + QStringLiteral(".json")),
        tr("JSON files (*.json);;All files (*)"));
    if (path.isEmpty())
        return;
    QPointer<ResultPane> self(this);
    dm::MappingRunner::instance().export_json(*t, path, this, [self, path](const ds::SourceError& e) {
        if (self)
            self->set_note(e.ok() ? tr("Saved %1").arg(QDir::toNativeSeparators(path)) : e.message);
    });
}

void ResultPane::send_to_mapping() {
    if (!has_result_ || mapping_source_.kind.isEmpty())
        return;
    dm::MappingHandoff h;
    h.source = mapping_source_;
    h.source.table = current_table_name();
    h.sample = result_;
    h.label = mapping_label_;
    dm::MappingInbox::instance().post(h);
}

fincept::report::ReportPayload ResultPane::report_payload() const {
    fincept::report::ReportPayload p;
    p.title = title_;
    p.source = report_source_;
    p.as_of = result_.as_of.isValid() ? result_.as_of.toLocalTime() : QDateTime::currentDateTime();
    const ds::DataTable* t = current_table();
    if (!t)
        return p;
    fincept::report::PayloadTable pt;
    pt.title = t->name.isEmpty() ? title_ : t->name;
    pt.columns = t->columns;
    const int n = std::min(t->row_count(), kDwReportRows);
    pt.rows.reserve(n);
    for (int r = 0; r < n; ++r) {
        QStringList row;
        row.reserve(t->column_count());
        for (int c = 0; c < t->column_count(); ++c)
            row << ds::cell_text(t->cell(r, c));
        pt.rows.append(row);
    }
    if (t->row_count() > n)
        pt.note = tr("First %1 of %2 rows").arg(n).arg(t->row_count());
    p.tables.append(pt);
    return p;
}

} // namespace fincept::screens::datasources
