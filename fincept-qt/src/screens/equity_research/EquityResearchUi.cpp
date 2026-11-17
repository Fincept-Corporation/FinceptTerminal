// src/screens/equity_research/EquityResearchUi.cpp
#include "screens/equity_research/EquityResearchUi.h"

#include "ui/theme/Theme.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QAbstractButton>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QStyle>
#include <QTableView>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::er_ui {

QString stylesheet() {
    namespace c = ui::colors;
    namespace f = ui::fonts;
    return QStringLiteral(
               "QFrame#erPanel { background:%1; border:1px solid %2; }"
               "QLabel#erPanelTitle { color:%3; font-size:%10px; font-weight:700; letter-spacing:1px; background:transparent; border:0; }"
               "QLabel#erKey { color:%4; font-size:%11px; background:transparent; border:0; }"
               "QLabel#erValue { color:%5; font-size:%12px; font-weight:600; background:transparent; border:0; }"
               "QLabel#erValueBig { color:%5; font-size:%13px; font-weight:700; background:transparent; border:0; }"
               "QLabel#erNote { color:%6; font-size:%14px; background:transparent; border:0; }"
               "QLabel#erWarn { color:%7; font-size:%11px; background:transparent; border:0; }"
               "QLabel[tone=\"good\"] { color:%8; } QLabel[tone=\"bad\"] { color:%9; }"
               "QLabel[tone=\"warn\"] { color:%7; } QLabel[tone=\"muted\"] { color:%6; }")
        .arg(c::BG_SURFACE(), c::BORDER_DIM(), c::AMBER(), c::TEXT_SECONDARY(), c::TEXT_PRIMARY(), c::TEXT_TERTIARY(),
             c::WARNING(), c::SUCCESS(), c::DANGER())
        .arg(f::px(-3), f::px(-3), f::px(-1), f::px(3), f::px(-4)) +
           QStringLiteral(
               "QPushButton#erSeg { background:transparent; color:%1; border:1px solid %2; padding:3px 10px;"
               " font-size:%7px; font-weight:700; }"
               "QPushButton#erSeg:hover { color:%3; border-color:%4; }"
               "QPushButton#erSeg:checked { background:%4; color:%5; border-color:%4; }"
               "QPushButton#erSeg:disabled { color:%6; border-color:%2; }"
               "QPushButton#erBtn { background:transparent; color:%4; border:1px solid %4; padding:3px 12px;"
               " font-size:%7px; font-weight:700; }"
               "QPushButton#erBtn:hover { background:%4; color:%5; }")
               .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(), ui::colors::TEXT_PRIMARY(),
                    ui::colors::AMBER(), ui::colors::TEXT_ON_ACCENT(), ui::colors::TEXT_DIM(), ui::fonts::px(-3)) +
           QStringLiteral(
               "QTableView#erTable { background:%1; alternate-background-color:%2; gridline-color:%3; color:%4;"
               " border:0; font-size:%7px; selection-background-color:%5; selection-color:%4; }"
               "QTableView#erTable QHeaderView::section { background:%6; color:%8; font-size:%9px; font-weight:700;"
               " padding:4px 6px; border:0; border-bottom:1px solid %3; border-right:1px solid %3; }")
               .arg(ui::colors::BG_SURFACE(), ui::colors::ROW_ALT(), ui::colors::BORDER_DIM(),
                    ui::colors::TEXT_PRIMARY(), ui::colors::BG_HOVER(), ui::colors::BG_RAISED(), ui::fonts::px(-3),
                    ui::colors::TEXT_SECONDARY(), ui::fonts::px(-4));
}

namespace {
/// setStyleSheet() re-polishes the whole subtree even for the text already in place, and the
/// theme manager's re-style pass may already have rewritten it: set only a changed sheet.
void erui_set_sheet(QWidget* w, const QString& sheet) {
    if (w->styleSheet() != sheet)
        w->setStyleSheet(sheet);
}
} // namespace

void apply_style(QWidget* w, std::function<QString()> css) {
    if (!w || !css)
        return;
    w->setStyleSheet(css());
    // Re-derived once an appearance change has settled (never inside theme_changed).
    QObject::connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, w,
                     [w, css]() { erui_set_sheet(w, css()); });
}

QFrame* panel(const QString& title, QVBoxLayout** body, QLabel** title_out, QWidget* parent) {
    auto* f = new QFrame(parent);
    f->setObjectName(QStringLiteral("erPanel"));
    auto* vl = new QVBoxLayout(f);
    vl->setContentsMargins(10, 8, 10, 10);
    vl->setSpacing(6);
    auto* t = new QLabel(title, f);
    t->setObjectName(QStringLiteral("erPanelTitle"));
    // A caller that asks for the title label fills it later (retranslateUi), so it
    // stays visible; only an untitled panel nobody will name hides it. Hiding it on
    // an empty initial text left the Technicals, Dividends, Earnings and 52-week
    // panels without their titles.
    t->setVisible(!title.isEmpty() || title_out != nullptr);
    vl->addWidget(t);
    if (title_out)
        *title_out = t;
    if (body)
        *body = vl;
    return f;
}

// ── StatusStrip ──────────────────────────────────────────────────────────────

StatusStrip::StatusStrip(QWidget* parent) : QFrame(parent) {
    setObjectName(QStringLiteral("erStatus"));
    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(10, 4, 10, 4);
    hl->setSpacing(10);
    text_ = new QLabel(this);
    text_->setWordWrap(true);
    text_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    hl->addWidget(text_, 1);
    retry_ = new QPushButton(tr("RETRY"), this);
    retry_->setObjectName(QStringLiteral("erBtn"));
    retry_->setCursor(Qt::PointingHandCursor);
    connect(retry_, &QPushButton::clicked, this, &StatusStrip::retry_requested);
    hl->addWidget(retry_);
    auto restyle = [this]() {
        erui_set_sheet(this, QStringLiteral("QFrame#erStatus { background:%1; border:1px solid %2; }"
                                            "QFrame#erStatus QLabel { background:transparent; border:0; font-size:%3px; color:%4; }"
                                            "QFrame#erStatus[mode=\"error\"] { border-color:%5; }"
                                            "QFrame#erStatus[mode=\"error\"] QLabel { color:%5; }")
                                 .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM(), ui::fonts::px(-3),
                                      ui::colors::TEXT_SECONDARY(), ui::colors::DANGER()) +
                             stylesheet());
    };
    restyle();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, restyle);
    set_mode(0, {}, {});
}

void StatusStrip::set_mode(int mode, const QString& text, const QString& detail) {
    mode_ = mode;
    static const char* const kModes[] = {"", "loading", "error", "empty", "info"};
    setProperty("mode", QString::fromLatin1(kModes[mode]));
    style()->unpolish(this);
    style()->polish(this);
    text_->setText(text);
    setToolTip(detail);
    retry_->setVisible(mode == 2);
    setVisible(mode != 0);
}

void StatusStrip::show_loading(const QString& text) {
    set_mode(1, text, {});
}
void StatusStrip::show_error(const QString& text, const QString& detail) {
    set_mode(2, text, detail);
}
void StatusStrip::show_empty(const QString& text) {
    set_mode(3, text, {});
}
void StatusStrip::show_info(const QString& text, const QString& detail) {
    set_mode(4, text, detail);
}
void StatusStrip::clear() {
    set_mode(0, {}, {});
}

void StatusStrip::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange && retry_)
        retry_->setText(tr("RETRY"));
    QFrame::changeEvent(e);
}

// ── Segmented ────────────────────────────────────────────────────────────────

Segmented::Segmented(QWidget* parent) : QWidget(parent) {
    layout_ = new QHBoxLayout(this);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->setSpacing(0);
}

void Segmented::add(const QString& key, const QString& text, const QString& tooltip) {
    auto* b = new QPushButton(text, this);
    b->setObjectName(QStringLiteral("erSeg"));
    b->setCheckable(true);
    b->setCursor(Qt::PointingHandCursor);
    b->setToolTip(tooltip);
    connect(b, &QPushButton::clicked, this, [this, key]() { set_current(key, true); });
    layout_->addWidget(b);
    buttons_.insert(key, b);
    if (current_.isEmpty())
        set_current(key);
}

void Segmented::set_text(const QString& key, const QString& text, const QString& tooltip) {
    if (auto* b = buttons_.value(key)) {
        b->setText(text);
        b->setToolTip(tooltip);
    }
}

void Segmented::set_current(const QString& key, bool emit_signal) {
    if (!buttons_.contains(key))
        return;
    const bool changed_key = key != current_;
    current_ = key;
    for (auto it = buttons_.begin(); it != buttons_.end(); ++it)
        it.value()->setChecked(it.key() == key);
    if (emit_signal && changed_key)
        emit changed(key);
}

void Segmented::set_enabled(const QString& key, bool enabled) {
    if (auto* b = buttons_.value(key))
        b->setEnabled(enabled);
}

// ── FrozenTable ──────────────────────────────────────────────────────────────

FrozenTable::FrozenTable(QWidget* parent) : QWidget(parent) {
    model_ = new QStandardItemModel(this);
    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);
    auto make = [this]() {
        auto* v = new QTableView(this);
        v->setObjectName(QStringLiteral("erTable"));
        v->setModel(model_);
        v->setAlternatingRowColors(true);
        v->setEditTriggers(QAbstractItemView::NoEditTriggers);
        v->setSelectionBehavior(QAbstractItemView::SelectRows);
        v->setSelectionMode(QAbstractItemView::SingleSelection);
        v->verticalHeader()->hide();
        v->setWordWrap(false);
        v->setFrameShape(QFrame::NoFrame);
        v->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        ui::metrics::follow_row_height(v, 24);
        return v;
    };
    frozen_ = make();
    main_ = make();
    // One selection for both halves of a row.
    QItemSelectionModel* old = main_->selectionModel();
    main_->setSelectionModel(frozen_->selectionModel());
    delete old;
    frozen_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    frozen_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    frozen_->horizontalHeader()->setStretchLastSection(true);
    main_->horizontalHeader()->setStretchLastSection(false);
    connect(main_->verticalScrollBar(), &QScrollBar::valueChanged, frozen_->verticalScrollBar(),
            &QScrollBar::setValue);
    connect(frozen_->verticalScrollBar(), &QScrollBar::valueChanged, main_->verticalScrollBar(),
            &QScrollBar::setValue);
    hl->addWidget(frozen_);
    hl->addWidget(main_, 1);
    main_->viewport()->installEventFilter(this);
    connect(model_, &QStandardItemModel::modelReset, this, [this]() { sync_geometry(); });
    connect(model_, &QStandardItemModel::columnsInserted, this, [this]() { sync_geometry(); });
    connect(model_, &QStandardItemModel::columnsRemoved, this, [this]() { sync_geometry(); });
    // Column widths follow the font: re-fit once an appearance change has settled.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() { refit(); });
}

void FrozenTable::sync_geometry() {
    const int cols = model_->columnCount();
    for (int c = 0; c < cols; ++c) {
        frozen_->setColumnHidden(c, c != 0);
        main_->setColumnHidden(c, c == 0);
    }
}

void FrozenTable::refit(int min_value_width) {
    sync_geometry();
    if (model_->columnCount() == 0)
        return;
    frozen_->resizeColumnToContents(0);
    const int w = std::min(std::max(frozen_->columnWidth(0) + 16, ui::metrics::width(180)), ui::metrics::width(380));
    frozen_->setColumnWidth(0, w);
    frozen_->setFixedWidth(w + 2);
    fit_widths_ = QVector<int>(model_->columnCount(), 0);
    for (int c = 1; c < model_->columnCount(); ++c) {
        main_->resizeColumnToContents(c);
        if (min_value_width > 0 && main_->columnWidth(c) < min_value_width)
            main_->setColumnWidth(c, min_value_width);
        fit_widths_[c] = main_->columnWidth(c);
    }
    spread();
}

void FrozenTable::spread() {
    const int cols = model_->columnCount();
    if (cols < 2 || fit_widths_.size() != cols)
        return;
    int total = 0;
    for (int c = 1; c < cols; ++c)
        total += fit_widths_[c];
    // Column 1 holds the trend sparkline when there are value columns after it.
    const int first = cols > 2 ? 2 : 1;
    const int n = cols - first;
    const int extra = main_->viewport()->width() - total;
    const int add = (extra > 0 && n > 0) ? extra / n : 0;
    for (int c = 1; c < cols; ++c)
        main_->setColumnWidth(c, fit_widths_[c] + (c >= first ? add : 0));
}

bool FrozenTable::eventFilter(QObject* watched, QEvent* e) {
    if (watched == main_->viewport() && e->type() == QEvent::Resize)
        spread();
    return QWidget::eventFilter(watched, e);
}

void FrozenTable::csv(QStringList& headers, QVector<QStringList>& rows, const QVector<int>& skip) const {
    headers.clear();
    rows.clear();
    const int cols = model_->columnCount();
    for (int c = 0; c < cols; ++c) {
        if (skip.contains(c))
            continue;
        QString h = model_->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString();
        const QString date = model_->headerData(c, Qt::Horizontal, kHeaderDateRole).toString();
        if (!date.isEmpty())
            h += QStringLiteral(" (") + date + QLatin1Char(')');
        headers << h;
    }
    for (int r = 0; r < model_->rowCount(); ++r) {
        QStringList row;
        for (int c = 0; c < cols; ++c) {
            if (skip.contains(c))
                continue;
            const QModelIndex idx = model_->index(r, c);
            const QVariant raw = model_->data(idx, kRawRole);
            if (raw.isValid() && raw.metaType().id() == QMetaType::Double)
                row << csv_number(raw.toDouble()); // a reported zero is written as 0
            else if (raw.isValid())
                row << QString(); // a cell without a value
            else
                row << model_->data(idx, Qt::DisplayRole).toString().trimmed();
        }
        rows << row;
    }
}

void FrozenTable::changeEvent(QEvent* e) {
    QWidget::changeEvent(e);
    if (e->type() == QEvent::FontChange)
        refit();
}

// ── Benchmarks ───────────────────────────────────────────────────────────────

Benchmark benchmark_for(const QString& symbol) {
    struct Entry {
        const char* suffix;
        const char* index;
        const char* name;
    };
    static const Entry kEntries[] = {
        {"", "^GSPC", "S&P 500"},          {"L", "^FTSE", "FTSE 100"},        {"T", "^N225", "Nikkei 225"},
        {"NS", "^NSEI", "NIFTY 50"},        {"BO", "^BSESN", "S&P BSE Sensex"}, {"DE", "^GDAXI", "DAX"},
        {"F", "^GDAXI", "DAX"},             {"PA", "^FCHI", "CAC 40"},         {"HK", "^HSI", "Hang Seng"},
        {"TO", "^GSPTSE", "S&P/TSX"},       {"V", "^GSPTSE", "S&P/TSX"},       {"AX", "^AXJO", "S&P/ASX 200"},
        {"KS", "^KS11", "KOSPI"},           {"KQ", "^KQ11", "KOSDAQ"},         {"SW", "^SSMI", "SMI"},
        {"AS", "^AEX", "AEX"},              {"MI", "FTSEMIB.MI", "FTSE MIB"},  {"MC", "^IBEX", "IBEX 35"},
        {"SS", "000001.SS", "SSE Composite"}, {"SZ", "399001.SZ", "SZSE Component"}, {"TW", "^TWII", "TAIEX"},
        {"SA", "^BVSP", "Ibovespa"},        {"ST", "^OMX", "OMX Stockholm 30"}, {"SI", "^STI", "Straits Times"},
        {"JO", "^J203.JO", "JSE All Share"}, {"MX", "^MXX", "S&P/BMV IPC"},    {"BR", "^BFX", "BEL 20"},
        {"CO", "^OMXC25", "OMX Copenhagen 25"}, {"OL", "OSEBX.OL", "Oslo Børs"}, {"HE", "^OMXH25", "OMX Helsinki 25"},
        {"NZ", "^NZ50", "S&P/NZX 50"},
    };
    const QString s = symbol.trimmed().toUpper();
    if (s.startsWith(QLatin1Char('^')) || s.contains(QLatin1Char('=')))
        return {};
    // A bare ticker with a crypto pair dash ("BTC-USD") is not a US equity.
    if (s.endsWith(QLatin1String("-USD")) || s.endsWith(QLatin1String("-USDT")))
        return {};
    const qsizetype dot = s.lastIndexOf(QLatin1Char('.'));
    const QString suffix = dot >= 0 ? s.mid(dot + 1) : QString();
    for (const auto& e : kEntries)
        if (suffix == QLatin1String(e.suffix))
            return {QString::fromLatin1(e.index), QString::fromUtf8(e.name)};
    // "BRK.B" style US class shares: a one-letter suffix that is not a venue.
    if (suffix.size() == 1)
        return {QStringLiteral("^GSPC"), QStringLiteral("S&P 500")};
    return {};
}

// ── CSV ──────────────────────────────────────────────────────────────────────

QString csv_number(double v) {
    if (!std::isfinite(v))
        return QString();
    QString s = QString::number(v, 'f', 8);
    if (s.contains(QLatin1Char('.'))) {
        while (s.endsWith(QLatin1Char('0')))
            s.chop(1);
        if (s.endsWith(QLatin1Char('.')))
            s.chop(1);
    }
    if (s == QLatin1String("-0"))
        s = QStringLiteral("0");
    return s;
}

namespace {
QString er_csv_field(const QString& f) {
    if (f.contains(QLatin1Char(',')) || f.contains(QLatin1Char('"')) || f.contains(QLatin1Char('\n')))
        return QLatin1Char('"') + QString(f).replace(QLatin1Char('"'), QStringLiteral("\"\"")) + QLatin1Char('"');
    return f;
}
} // namespace

bool export_csv(QWidget* parent, const QString& suggested_name, const QStringList& headers,
                const QVector<QStringList>& rows) {
    QString safe;
    for (const QChar ch : suggested_name)
        safe += (ch.isLetterOrNumber() || ch == QLatin1Char('-') || ch == QLatin1Char('_')) ? ch : QChar('_');
    const QString path = QFileDialog::getSaveFileName(parent, QObject::tr("Export CSV"), safe + QStringLiteral(".csv"),
                                                      QObject::tr("CSV Files (*.csv)"));
    if (path.isEmpty())
        return false;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::warning(parent, QObject::tr("Export failed"),
                             QObject::tr("Could not open the file for writing:\n%1").arg(file.errorString()));
        return false;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    QStringList h;
    for (const auto& x : headers)
        h << er_csv_field(x);
    out << h.join(QLatin1Char(',')) << '\n';
    for (const auto& r : rows) {
        QStringList cells;
        for (const auto& x : r)
            cells << er_csv_field(x);
        out << cells.join(QLatin1Char(',')) << '\n';
    }
    out.flush();
    if (out.status() != QTextStream::Ok || file.error() != QFile::NoError) {
        QMessageBox::warning(parent, QObject::tr("Export failed"), QObject::tr("Writing the CSV file failed:\n%1").arg(path));
        return false;
    }
    return true;
}

} // namespace fincept::screens::er_ui
