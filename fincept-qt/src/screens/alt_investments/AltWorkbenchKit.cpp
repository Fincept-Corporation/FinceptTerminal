// AltWorkbenchKit.cpp — see AltWorkbenchKit.h.
#include "screens/alt_investments/AltWorkbenchKit.h"

#include "ui/charts/ChartExport.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/report/AddToReportButton.h"

#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QDate>
#include <QDateTime>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableView>

#include <algorithm>
#include <cmath>

namespace fincept::screens::alt {

namespace fmt = fincept::ui::formatting;
using namespace fincept::ui;

QString alt_wb_style() {
    const QString s3 = fonts::px(-3);
    const QString s2 = fonts::px(-2);
    const QString s1 = fonts::px(-1);
    const QString s0 = fonts::px(0);
    QString css;
    css += QString("#altWbRoot { background: %1; }").arg(colors::BG_BASE());
    css += QString("#altWbSection { color: %1; font-size: %2px; font-weight: 700; letter-spacing: 0.8px;"
                   " background: transparent; }")
               .arg(colors::TEXT_SECONDARY(), s3);
    css += QString("#altWbTitle { color: %1; font-size: %2px; font-weight: 700; background: transparent; }")
               .arg(colors::TEXT_PRIMARY(), s0);
    css += QString("#altWbHint, #altWbStatus { color: %1; font-size: %2px; background: transparent; }")
               .arg(colors::TEXT_TERTIARY(), s2);
    css += QString("#altWbNote { color: %1; font-size: %2px; background: transparent; }").arg(colors::TEXT_SECONDARY(), s2);
    css += QString("#altWbError { color: %1; font-size: %2px; background: transparent; }").arg(colors::DANGER(), s2);
    css += QString("#altWbFormLabel { color: %1; font-size: %2px; background: transparent; }")
               .arg(colors::TEXT_SECONDARY(), s2);
    css += QString("#altWbBar { background: %1; border-bottom: 1px solid %2; }").arg(colors::BG_RAISED(), colors::BORDER_DIM());
    css += QString("#altWbPanel { background: %1; border: 1px solid %2; }").arg(colors::BG_SURFACE(), colors::BORDER_DIM());
    css += QString("QPushButton#altWbPrimary { background: %1; color: %2; border: none; padding: 4px 12px;"
                   " font-weight: 700; font-size: %3px; }"
                   "QPushButton#altWbPrimary:disabled { background: %4; color: %5; }")
               .arg(colors::AMBER(), colors::TEXT_ON_ACCENT(), s2, colors::BG_RAISED(), colors::TEXT_TERTIARY());
    css += QString("QPushButton#altWbSecondary { background: transparent; color: %1; border: 1px solid %2;"
                   " padding: 3px 10px; font-weight: 600; font-size: %3px; }"
                   "QPushButton#altWbSecondary:hover { color: %4; border-color: %5; }"
                   "QPushButton#altWbSecondary:disabled { color: %6; border-color: %7; }")
               .arg(colors::TEXT_SECONDARY(), colors::BORDER_MED(), s2, colors::TEXT_PRIMARY(), colors::AMBER(),
                    colors::TEXT_DIM(), colors::BORDER_DIM());
    css += QString("#altWbRoot QDoubleSpinBox, #altWbRoot QSpinBox, #altWbRoot QComboBox, #altWbForm QLineEdit"
                   " { background: %1; color: %2; border: 1px solid %3; padding: 3px 6px; font-size: %4px; }")
               .arg(colors::BG_RAISED(), colors::TEXT_PRIMARY(), colors::BORDER_MED(), s1);
    css += QString("QTableView#altWbTable { background: %1; color: %2; border: 1px solid %3; gridline-color: %3;"
                   " alternate-background-color: %4; font-size: %5px; selection-background-color: %6;"
                   " selection-color: %2; }"
                   "QTableView#altWbTable QHeaderView::section { background: %7; color: %8; border: none;"
                   " border-right: 1px solid %3; border-bottom: 1px solid %3; padding: 3px 6px; font-weight: 600;"
                   " font-size: %9px; }")
               .arg(colors::BG_SURFACE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM(), colors::ROW_ALT(), s1,
                    colors::ACCENT_BG(), colors::BG_RAISED(), colors::TEXT_SECONDARY(), s2);
    css += QString("QTabWidget#altWbTabs::pane { border: 1px solid %1; background: %2; }"
                   "QTabWidget#altWbTabs > QTabBar::tab { background: %3; color: %4; padding: 5px 14px;"
                   " border: 1px solid %1; border-bottom: none; font-size: %5px; font-weight: 600; }"
                   "QTabWidget#altWbTabs > QTabBar::tab:selected { background: %2; color: %6;"
                   " border-bottom: 2px solid %6; }")
               .arg(colors::BORDER_DIM(), colors::BG_SURFACE(), colors::BG_RAISED(), colors::TEXT_SECONDARY(), s2,
                    colors::AMBER());
    css += QString("QGroupBox#altWbGroup { color: %1; border: 1px solid %2; margin-top: 14px; padding-top: 4px;"
                   " font-size: %3px; font-weight: 700; }"
                   "QGroupBox#altWbGroup::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }")
               .arg(colors::TEXT_SECONDARY(), colors::BORDER_DIM(), s2);
    css += QString("QSplitter#altWbSplit::handle { background: %1; }").arg(colors::BORDER_DIM());
    css += QString("QScrollArea#altWbScroll { background: transparent; border: none; }"
                   "QWidget#altWbScrollBody { background: transparent; }");
    return css;
}

// ── Rows model ───────────────────────────────────────────────────────────────

AltWbRowsModel::AltWbRowsModel(QObject* parent) : QAbstractTableModel(parent) {}

void AltWbRowsModel::set_rows(const QStringList& headers, const QVector<QStringList>& rows,
                              const QVector<int>& numeric_cols, const QVector<QStringList>& tones) {
    beginResetModel();
    headers_ = headers;
    rows_ = rows;
    numeric_ = numeric_cols;
    tones_ = tones;
    endResetModel();
}

void AltWbRowsModel::clear() {
    set_rows({}, {});
}

int AltWbRowsModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

int AltWbRowsModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(headers_.size());
}

QVariant AltWbRowsModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= rows_.size())
        return {};
    const QStringList& r = rows_.at(index.row());
    switch (role) {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
            return r.value(index.column());
        case Qt::TextAlignmentRole:
            return int((numeric_.contains(index.column()) ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
        case Qt::ForegroundRole: {
            const QString tone = index.row() < tones_.size() ? tones_.at(index.row()).value(index.column()) : QString();
            if (tone == QLatin1String("good"))
                return QColor(colors::SUCCESS());
            if (tone == QLatin1String("bad"))
                return QColor(colors::DANGER());
            if (tone == QLatin1String("warn"))
                return QColor(colors::WARNING());
            if (tone == QLatin1String("dim"))
                return QColor(colors::TEXT_TERTIARY());
            return {};
        }
        default:
            return {};
    }
}

QVariant AltWbRowsModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return headers_.value(section);
    if (orientation == Qt::Horizontal && role == Qt::TextAlignmentRole)
        return int((numeric_.contains(section) ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
    return {};
}

QTableView* alt_wb_make_table(QWidget* parent, AltWbRowsModel* model, const QString& export_name) {
    auto* view = new QTableView(parent);
    view->setObjectName(QStringLiteral("altWbTable"));
    view->setModel(model);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view->setAlternatingRowColors(true);
    view->setWordWrap(false);
    view->setCornerButtonEnabled(false);
    view->verticalHeader()->setVisible(false);
    view->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    view->verticalHeader()->setDefaultSectionSize(QFontMetrics(fonts::font(-1)).height() + 8);
    view->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    view->horizontalHeader()->setMinimumSectionSize(QFontMetrics(fonts::font(-1)).averageCharWidth() * 9);
    view->horizontalHeader()->setHighlightSections(false);
    ChartExport::install_table_context_menu(view, export_name);
    return view;
}

void alt_wb_fit_table(QTableView* view, int max_rows) {
    if (!view || !view->model())
        return;
    const int row_h = QFontMetrics(fonts::font(-1)).height() + 8;
    view->verticalHeader()->setDefaultSectionSize(row_h);
    const int header_h = QFontMetrics(fonts::font(-2, QFont::DemiBold)).height() + 10;
    const int n = std::clamp(view->model()->rowCount(), 1, std::max(1, max_rows));
    view->setFixedHeight(header_h + n * row_h + 2 * view->frameWidth() + 2);
}

// ── Actions row ──────────────────────────────────────────────────────────────

QPushButton* alt_wb_button(const QString& text, bool primary, QWidget* parent) {
    auto* b = new QPushButton(QString(text).replace(QLatin1Char('&'), QLatin1String("&&")), parent);
    b->setObjectName(primary ? QStringLiteral("altWbPrimary") : QStringLiteral("altWbSecondary"));
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

QLabel* alt_wb_label(const QString& text, const char* object_name, QWidget* parent) {
    auto* l = new QLabel(text, parent);
    l->setObjectName(QString::fromLatin1(object_name));
    return l;
}

AltWbActions alt_wb_make_actions(QWidget* parent, const QString& title) {
    AltWbActions a;
    a.row = new QWidget(parent);
    a.row->setObjectName(QStringLiteral("altWbBar"));
    auto* hl = new QHBoxLayout(a.row);
    hl->setContentsMargins(10, 4, 6, 4);
    hl->setSpacing(6);
    a.title = alt_wb_label(title, "altWbSection", a.row);
    hl->addWidget(a.title);
    a.status = alt_wb_label(QString(), "altWbStatus", a.row);
    hl->addWidget(a.status, 1);
    a.copy = alt_wb_button(QObject::tr("COPY"), false, a.row);
    a.copy->setToolTip(QObject::tr("Copy the figures, inputs and table as tab-separated text"));
    a.csv = alt_wb_button(QObject::tr("CSV"), false, a.row);
    a.csv->setToolTip(QObject::tr("Save the figures, inputs and table as a CSV file"));
    a.image = alt_wb_button(QObject::tr("IMAGE"), false, a.row);
    a.image->setToolTip(QObject::tr("Save the result as a PNG image"));
    a.report = new ui::AddToReportButton(QStringLiteral("Alternative Investments"), a.row);
    a.report->set_show_result(false);
    hl->addWidget(a.copy);
    hl->addWidget(a.csv);
    hl->addWidget(a.image);
    hl->addWidget(a.report);
    alt_wb_set_actions_enabled(a, false);
    return a;
}

void alt_wb_set_actions_enabled(const AltWbActions& a, bool on) {
    for (QWidget* w : {static_cast<QWidget*>(a.copy), static_cast<QWidget*>(a.csv), static_cast<QWidget*>(a.image),
                       static_cast<QWidget*>(a.report)})
        if (w)
            w->setEnabled(on);
}

// ── Numbers ──────────────────────────────────────────────────────────────────

double alt_wb_num(const QJsonValue& v) {
    return v.isDouble() ? v.toDouble() : kAltWbNaN;
}

QString alt_wb_money(double v, const QString& ccy, bool compact) {
    if (!std::isfinite(v))
        return QStringLiteral("-");
    const QString code = ccy.trimmed().isEmpty() ? fmt::display_currency() : ccy.trimmed().toUpper();
    return fmt::money(v, code, compact ? fmt::Compact::FromSettings : fmt::Compact::None, compact ? -1 : 0);
}

QString alt_wb_pct(double fraction, int decimals, bool signed_) {
    if (!std::isfinite(fraction))
        return QStringLiteral("-");
    return fmt::fraction_pct(fraction, decimals, signed_);
}

QString alt_wb_multiple(double v, int decimals) {
    if (!std::isfinite(v))
        return QStringLiteral("-");
    return fmt::ratio(v, decimals);
}

QString alt_wb_number(double v, int decimals) {
    if (!std::isfinite(v))
        return QStringLiteral("-");
    return fmt::number(v, decimals);
}

double alt_wb_date_ms(const QString& iso) {
    const QDate d = QDate::fromString(iso.left(10), Qt::ISODate);
    if (!d.isValid())
        return kAltWbNaN;
    return double(QDateTime(d, QTime(0, 0)).toMSecsSinceEpoch());
}

// ── Export ───────────────────────────────────────────────────────────────────

QString alt_wb_tsv(const AltWbExport& e) {
    QStringList lines;
    if (!e.title.isEmpty())
        lines << e.title;
    for (const auto& f : e.figures)
        lines << f.first + QLatin1Char('\t') + f.second;
    if (!e.inputs.isEmpty()) {
        lines << QString() << QObject::tr("Inputs");
        for (const auto& f : e.inputs)
            lines << f.first + QLatin1Char('\t') + f.second;
    }
    if (!e.headers.isEmpty()) {
        lines << QString() << e.headers.join(QLatin1Char('\t'));
        for (const QStringList& r : e.rows)
            lines << r.join(QLatin1Char('\t'));
    }
    if (!e.notes.isEmpty())
        lines << QString() << e.notes;
    return lines.join(QLatin1Char('\n'));
}

void alt_wb_copy(const AltWbExport& e) {
    if (auto* cb = QApplication::clipboard())
        cb->setText(alt_wb_tsv(e));
}

bool alt_wb_save_csv(const AltWbExport& e, const QString& base_name, QWidget* dialog_parent) {
    const int cols = std::max<int>(2, static_cast<int>(e.headers.size()));
    auto padded = [cols](QStringList r) {
        while (r.size() < cols)
            r << QString();
        return r;
    };
    QStringList header = padded({QObject::tr("Figure"), QObject::tr("Value")});
    QVector<QStringList> rows;
    for (const auto& f : e.figures)
        rows << padded({f.first, f.second});
    for (const auto& f : e.inputs)
        rows << padded({QObject::tr("Input: %1").arg(f.first), f.second});
    if (!e.headers.isEmpty()) {
        rows << padded({});
        rows << padded(e.headers);
        for (const QStringList& r : e.rows)
            rows << padded(r);
    }
    return ChartExport::save_csv(header, rows, base_name, dialog_parent);
}

fincept::report::ReportPayload alt_wb_payload(const AltWbExport& e, const QVector<QPair<QString, QWidget*>>& charts) {
    fincept::report::ReportPayload p;
    p.title = e.title;
    p.source = QStringLiteral("Alternative Investments");
    p.as_of = QDateTime::currentDateTime();
    for (const auto& f : e.figures) {
        fincept::report::PayloadKpi k;
        k.label = f.first;
        k.value = f.second;
        p.kpis << k;
    }
    if (!e.headers.isEmpty()) {
        fincept::report::PayloadTable t;
        t.title = e.title;
        t.columns = e.headers;
        t.rows = e.rows;
        p.tables << t;
    }
    for (const auto& c : charts) {
        if (!c.second || !c.second->isVisible())
            continue;
        const QPixmap pm = ChartExport::grab(c.second);
        if (pm.isNull())
            continue;
        QByteArray bytes;
        QBuffer buf(&bytes);
        buf.open(QIODevice::WriteOnly);
        pm.save(&buf, "PNG");
        fincept::report::PayloadChart ch;
        ch.title = c.first;
        ch.image = bytes;
        ch.image_format = QStringLiteral("png");
        p.charts << ch;
    }
    p.inputs = e.inputs;
    p.notes = e.notes;
    return p;
}

} // namespace fincept::screens::alt
