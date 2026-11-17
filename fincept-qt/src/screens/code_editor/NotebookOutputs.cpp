// NotebookOutputs.cpp — see NotebookOutputs.h. Styled by the screen sheet
// (NotebookStyle.h) through object names; nothing here sets a stylesheet.

#include "screens/code_editor/NotebookOutputs.h"

#include "screens/code_editor/NotebookStyle.h"
#include "ui/charts/ChartExport.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QTextBrowser>
#include <QTextCursor>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::screens {

using notebook::CellOutput;
namespace colors = ui::colors;

namespace {

constexpr int kCollapsedLines = 24;   ///< text output height before "Show all"
constexpr int kExpandedLines = 400;   ///< past this the text scrolls inside
constexpr int kKeptLines = 5000;      ///< lines kept on screen (the model keeps all)
constexpr int kTableRowsInCell = 12;  ///< table rows visible in a cell
constexpr int kTableRowsLarge = 60;   ///< ... in the inspector
constexpr int kTracebackCollapseAt = 14;

QToolButton* small_button(QWidget* parent, NbGlyph glyph, const QString& text, const QString& tip) {
    auto* b = new QToolButton(parent);
    b->setObjectName(QStringLiteral("nbBtn"));
    b->setIcon(nb_icon(glyph));
    b->setText(text);
    b->setToolTip(tip);
    b->setAccessibleName(tip);
    b->setToolButtonStyle(text.isEmpty() ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon);
    b->setCursor(Qt::PointingHandCursor);
    b->setIconSize(QSize(12, 12));
    return b;
}

QString grouped(qint64 n) {
    return ui::formatting::number(static_cast<double>(n), 0);
}

int count_lines(const QString& text) {
    if (text.isEmpty())
        return 0;
    const int n = static_cast<int>(text.count(QLatin1Char('\n')));
    return text.endsWith(QLatin1Char('\n')) ? n : n + 1;
}

} // namespace

int nb_output_line_height() {
    return QFontMetrics(ui::fonts::font(-1)).lineSpacing();
}

// ═════════════════════════════════════════════════════════════════════════════
// NbTableModel

NbTableModel::NbTableModel(const QJsonObject& table, QObject* parent) : QAbstractTableModel(parent) {
    columns_ << table.value(QStringLiteral("index_name")).toString();
    for (const QJsonValue& c : table.value(QStringLiteral("columns")).toArray())
        columns_ << c.toString();
    for (const QJsonValue& d : table.value(QStringLiteral("dtypes")).toArray())
        dtypes_ << d.toString();
    index_ = table.value(QStringLiteral("index")).toArray();
    rows_ = table.value(QStringLiteral("rows")).toArray();
    total_rows_ = table.value(QStringLiteral("total_rows")).toInt(static_cast<int>(rows_.size()));
    total_columns_ = table.value(QStringLiteral("total_columns")).toInt(static_cast<int>(columns_.size() - 1));

    // Decimals per numeric column from a sample: whole numbers none; else by magnitude.
    const int ncols = static_cast<int>(columns_.size() - 1);
    decimals_.fill(2, ncols);
    const int sample = std::min<int>(static_cast<int>(rows_.size()), 200);
    for (int c = 0; c < ncols; ++c) {
        const QString dt = c < dtypes_.size() ? dtypes_[c] : QString();
        if (dt == QLatin1String("integer")) {
            decimals_[c] = 0;
            continue;
        }
        double maxabs = 0;
        bool whole = true;
        bool any = false;
        for (int r = 0; r < sample; ++r) {
            const QJsonValue v = rows_.at(r).toArray().at(c);
            if (!v.isDouble())
                continue;
            any = true;
            const double d = v.toDouble();
            maxabs = std::max(maxabs, std::abs(d));
            if (d != std::floor(d))
                whole = false;
        }
        decimals_[c] = !any ? 2 : whole && maxabs < 1e15 ? 0 : maxabs >= 100 ? 2 : maxabs >= 1 ? 4 : 6;
    }
}

int NbTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

int NbTableModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(columns_.size());
}

QString NbTableModel::cell_text(int row, int column, bool raw) const {
    if (column == 0)
        return index_.at(row).toString();
    const QJsonValue v = rows_.at(row).toArray().at(column - 1);
    if (v.isDouble()) {
        const double d = v.toDouble();
        if (raw)
            return QString::number(d, 'g', 15);
        if (d != 0 && std::abs(d) < 1e-4)
            return QString::number(d, 'e', 3);
        return ui::formatting::number(d, decimals_.value(column - 1, 2));
    }
    if (v.isBool())
        return v.toBool() ? QStringLiteral("True") : QStringLiteral("False");
    if (v.isNull() || v.isUndefined())
        return raw ? QString() : QStringLiteral("NaN");
    return v.toString();
}

QVariant NbTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid())
        return {};
    const int r = index.row();
    const int c = index.column();
    switch (role) {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
            return cell_text(r, c, false);
        case SortRole: {
            if (c == 0)
                return r; // the original order
            const QJsonValue v = rows_.at(r).toArray().at(c - 1);
            if (v.isDouble())
                return v.toDouble();
            if (v.isBool())
                return v.toBool() ? 1.0 : 0.0;
            if (v.isNull() || v.isUndefined())
                return -std::numeric_limits<double>::infinity();
            return v.toString();
        }
        case Qt::TextAlignmentRole: {
            if (c == 0)
                return int(Qt::AlignLeft | Qt::AlignVCenter);
            const QJsonValue v = rows_.at(r).toArray().at(c - 1);
            return int((v.isDouble() || v.isNull() ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
        }
        case Qt::ForegroundRole: {
            if (c == 0)
                return QColor(colors::TEXT_SECONDARY());
            const QJsonValue v = rows_.at(r).toArray().at(c - 1);
            if (v.isNull() || v.isUndefined())
                return QColor(colors::TEXT_TERTIARY());
            return {};
        }
        default:
            return {};
    }
}

QVariant NbTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal)
        return {};
    if (role == Qt::DisplayRole)
        return columns_.value(section);
    if (role == Qt::ToolTipRole && section > 0)
        return QStringLiteral("%1 · %2").arg(columns_.value(section), dtypes_.value(section - 1));
    if (role == Qt::TextAlignmentRole) {
        const QString dt = dtypes_.value(section - 1);
        const bool numeric = section > 0 && (dt == QLatin1String("number") || dt == QLatin1String("integer"));
        return int((numeric ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
    }
    return {};
}

QStringList NbTableModel::headers() const {
    return columns_;
}

QVector<QStringList> NbTableModel::rows_text(bool raw) const {
    QVector<QStringList> out;
    out.reserve(rows_.size());
    for (int r = 0; r < rows_.size(); ++r) {
        QStringList row;
        for (int c = 0; c < columns_.size(); ++c)
            row << cell_text(r, c, raw);
        out.append(row);
    }
    return out;
}

int NbTableModel::width_chars(int column) const {
    int w = static_cast<int>(columns_.value(column).size()) + 1;
    const int sample = std::min<int>(static_cast<int>(rows_.size()), 60);
    for (int r = 0; r < sample; ++r)
        w = std::max(w, static_cast<int>(cell_text(r, column, false).size()));
    return std::clamp(w, 4, 30);
}

// ═════════════════════════════════════════════════════════════════════════════
// NbTextOutput

NbTextOutput::NbTextOutput(const CellOutput& out, bool large, QWidget* parent) : NbOutputView(parent), large_(large) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(out.is_stderr ? 8 : 0, 2, 0, 2);
    v->setSpacing(2);
    if (out.kind == CellOutput::Kind::Stream && out.is_stderr) {
        setObjectName(QStringLiteral("nbOutStderr"));
        setAttribute(Qt::WA_StyledBackground, true);
        tag_ = new QLabel(tr("STDERR"), this);
        tag_->setObjectName(QStringLiteral("nbOutTag"));
        tag_->setProperty("tone", QStringLiteral("warn"));
        tag_->setToolTip(tr("Written to the error stream (warnings, logging, tracebacks of caught errors)"));
        v->addWidget(tag_);
    }
    view_ = new QPlainTextEdit(this);
    view_->setObjectName(QStringLiteral("nbOutText"));
    if (out.is_stderr)
        view_->setProperty("stream", QStringLiteral("stderr"));
    view_->setReadOnly(true);
    view_->setFrameShape(QFrame::NoFrame);
    view_->setLineWrapMode(QPlainTextEdit::NoWrap);
    view_->setMaximumBlockCount(kKeptLines);
    view_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    view_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    view_->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    view_->document()->setDocumentMargin(2);
    view_->setAccessibleName(out.is_stderr ? tr("Cell error stream") : tr("Cell output"));
    v->addWidget(view_, large ? 1 : 0);

    more_ = new QPushButton(this);
    more_->setObjectName(QStringLiteral("nbLink"));
    more_->setCursor(Qt::PointingHandCursor);
    more_->setVisible(false);
    connect(more_, &QPushButton::clicked, this, [this]() {
        expanded_ = !expanded_;
        fit();
    });
    v->addWidget(more_, 0, Qt::AlignLeft);
    update_output(out);
}

void NbTextOutput::update_output(const CellOutput& out) {
    QString text = out.text;
    if (text.endsWith(QLatin1Char('\r')))
        text.chop(1); // a pending carriage return is not shown
    // A live stream only grows (a carriage return rewrites at most its last line):
    // compare the tail instead of the whole text, which can be 2 MB.
    const qsizetype probe = std::min<qsizetype>(text_.size(), 256);
    const bool grew = !text_.isEmpty() && text.size() >= text_.size() &&
                      QStringView(text).mid(text_.size() - probe, probe) == QStringView(text_).right(probe);
    if (grew) { // (the view's block limit drops the oldest lines by itself)
        // A live stream grew: append only the new part.
        QString add = text.mid(text_.size());
        QScrollBar* sb = view_->verticalScrollBar();
        const bool at_end = sb->value() >= sb->maximum() - 2;
        QTextCursor cur(view_->document());
        cur.movePosition(QTextCursor::End);
        cur.insertText(add);
        if (at_end)
            sb->setValue(sb->maximum());
    } else {
        // Only the lines the view keeps: setting megabytes of text would stall the window.
        qsizetype start = text.size();
        for (int n = 0; start > 0 && n <= kKeptLines; --start)
            if (text.at(start - 1) == QLatin1Char('\n') && ++n > kKeptLines)
                break;
        QString shown = text.mid(start);
        if (shown.endsWith(QLatin1Char('\n')))
            shown.chop(1);
        view_->setPlainText(shown);
    }
    text_ = text;
    fit();
}

void NbTextOutput::fit() {
    const int lines = count_lines(text_);
    const int shown = std::min(lines, kKeptLines);
    if (large_) {
        view_->setMinimumHeight(nb_output_line_height() * 6);
        view_->setMaximumHeight(QWIDGETSIZE_MAX);
    } else {
        const int limit = expanded_ ? kExpandedLines : kCollapsedLines;
        const int visible = std::clamp(shown, 1, limit);
        int h = visible * nb_output_line_height() + 8;
        // Room for the horizontal bar when a line is wider than the view.
        if (view_->horizontalScrollBar()->maximum() > 0)
            h += view_->horizontalScrollBar()->sizeHint().height();
        view_->setFixedHeight(h);
    }
    const bool long_text = !large_ && lines > kCollapsedLines;
    more_->setVisible(long_text || lines > kKeptLines);
    if (lines > kKeptLines)
        more_->setText(expanded_ ? tr("Show less  ·  only the last %1 of %2 lines are kept on screen")
                                       .arg(grouped(kKeptLines), grouped(lines))
                                 : tr("Show more  ·  last %1 of %2 lines").arg(grouped(kKeptLines), grouped(lines)));
    else if (long_text)
        more_->setText(expanded_ ? tr("Show less") : tr("Show all %1 lines").arg(grouped(lines)));
}

void NbTextOutput::refit() {
    fit();
}

// ═════════════════════════════════════════════════════════════════════════════
// NbTableOutput

NbTableOutput::NbTableOutput(const CellOutput& out, bool large, QWidget* parent) : NbOutputView(parent), large_(large) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 2, 0, 4);
    v->setSpacing(2);

    auto* head = new QWidget(this);
    head->setObjectName(QStringLiteral("nbOutTableHead"));
    auto* h = new QHBoxLayout(head);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(4);
    caption_ = new QLabel(head);
    caption_->setObjectName(QStringLiteral("nbDim"));
    h->addWidget(caption_);
    h->addStretch(1);
    auto* copy = small_button(head, NbGlyph::Copy, tr("COPY"), tr("Copy the rows shown (tab-separated)"));
    connect(copy, &QToolButton::clicked, this, [this]() { copy_rows(); });
    h->addWidget(copy);
    auto* csv = small_button(head, NbGlyph::Export, tr("CSV"), tr("Save the rows shown as CSV"));
    connect(csv, &QToolButton::clicked, this, [this]() { export_csv(); });
    h->addWidget(csv);
    if (!large) {
        auto* expand = small_button(head, NbGlyph::Expand, tr("EXPAND"), tr("Show this table in the inspector"));
        connect(expand, &QToolButton::clicked, this, &NbOutputView::expand_requested);
        h->addWidget(expand);
    }
    v->addWidget(head);

    model_ = new NbTableModel(out.table, this);
    auto* proxy = new QSortFilterProxyModel(this);
    proxy->setSourceModel(model_);
    proxy->setSortRole(NbTableModel::SortRole);
    proxy->setDynamicSortFilter(false);

    view_ = new QTableView(this);
    view_->setObjectName(QStringLiteral("nbTable"));
    view_->setModel(proxy);
    view_->setAlternatingRowColors(true);
    view_->setShowGrid(false);
    view_->setSelectionBehavior(QAbstractItemView::SelectItems);
    view_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view_->setWordWrap(false);
    view_->setTextElideMode(Qt::ElideRight);
    view_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    view_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    view_->verticalHeader()->setVisible(false);
    view_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    view_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    view_->horizontalHeader()->setStretchLastSection(false);
    view_->horizontalHeader()->setHighlightSections(false);
    view_->setSortingEnabled(true);
    view_->horizontalHeader()->setSortIndicatorShown(false);
    connect(view_->horizontalHeader(), &QHeaderView::sortIndicatorChanged, view_->horizontalHeader(),
            [hdr = view_->horizontalHeader()]() { hdr->setSortIndicatorShown(true); });
    view_->setAccessibleName(tr("Table output"));
    v->addWidget(view_, large ? 1 : 0);

    const int shown_rows = model_->rowCount();
    const int shown_cols = model_->columnCount() - 1;
    QString cap = tr("%1 rows × %2 columns").arg(grouped(model_->total_rows()), grouped(model_->total_columns()));
    if (model_->total_rows() > shown_rows || model_->total_columns() > shown_cols)
        cap += QStringLiteral("  ·  ") +
               tr("showing the first %1 rows × %2 columns").arg(grouped(shown_rows), grouped(shown_cols));
    caption_->setText(cap);
    fit();
}

void NbTableOutput::fit() {
    const int row_h = ui::metrics::row_height(-2, 20, 6);
    view_->verticalHeader()->setDefaultSectionSize(row_h);
    const int avg = QFontMetrics(ui::fonts::font(-2)).averageCharWidth();
    for (int c = 0; c < model_->columnCount(); ++c)
        view_->horizontalHeader()->resizeSection(c, model_->width_chars(c) * avg + 18);
    if (large_) {
        view_->setMinimumHeight(row_h * 8);
        view_->setMaximumHeight(QWIDGETSIZE_MAX);
        return;
    }
    const int head_h = QFontMetrics(ui::fonts::font(-3, QFont::Bold)).height() + 8;
    const int rows = std::clamp(model_->rowCount(), 1, large_ ? kTableRowsLarge : kTableRowsInCell);
    int total_w = 0;
    for (int c = 0; c < model_->columnCount(); ++c)
        total_w += view_->horizontalHeader()->sectionSize(c);
    const bool hbar = width() > 0 && total_w > width() - 4;
    view_->setFixedHeight(head_h + rows * row_h + 4 + (hbar ? 8 : 0));
}

void NbTableOutput::refit() {
    fit();
}

void NbTableOutput::refresh_theme() {
    view_->viewport()->update(); // the model reads its colours at paint time
}

void NbTableOutput::copy_rows() {
    QString text = model_->headers().join(QLatin1Char('\t')) + QLatin1Char('\n');
    for (const QStringList& row : model_->rows_text(true))
        text += row.join(QLatin1Char('\t')) + QLatin1Char('\n');
    QApplication::clipboard()->setText(text);
}

void NbTableOutput::export_csv() {
    QStringList headers = model_->headers();
    if (headers.value(0).isEmpty())
        headers[0] = QStringLiteral("index");
    ui::ChartExport::save_csv(headers, model_->rows_text(true), QStringLiteral("notebook_table"), this);
}

// ═════════════════════════════════════════════════════════════════════════════
// NbImageOutput

NbImageOutput::NbImageOutput(const CellOutput& out, bool large, QWidget* parent) : NbOutputView(parent), large_(large) {
    pixmap_.loadFromData(out.png, "PNG");
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 4, 0, 4);
    v->setSpacing(2);

    image_ = new QLabel;
    image_->setObjectName(QStringLiteral("nbImage"));
    image_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    image_->setAccessibleName(tr("Figure"));
    if (large) {
        scroll_ = new QScrollArea(this);
        scroll_->setWidget(image_);
        scroll_->setWidgetResizable(false);
        scroll_->setAlignment(Qt::AlignCenter);
        v->addWidget(scroll_, 1);
    } else {
        image_->setParent(this);
        image_->setCursor(Qt::PointingHandCursor);
        image_->setToolTip(tr("Click to view in the inspector"));
        image_->installEventFilter(this);
        v->addWidget(image_);
    }

    auto* foot = new QWidget(this);
    foot->setObjectName(QStringLiteral("nbOutImageHead"));
    auto* h = new QHBoxLayout(foot);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(4);
    auto* cap = new QLabel(tr("FIGURE  ·  %1 × %2 px").arg(pixmap_.width()).arg(pixmap_.height()), foot);
    cap->setObjectName(QStringLiteral("nbDim"));
    h->addWidget(cap);
    h->addStretch(1);
    if (large) {
        zoom_ = small_button(foot, NbGlyph::Search, tr("100%"), tr("Show the figure at its actual size"));
        zoom_->setCheckable(true);
        connect(zoom_, &QToolButton::toggled, this, [this](bool on) {
            actual_size_ = on;
            zoom_->setText(on ? tr("FIT") : tr("100%"));
            last_width_ = -1;
            rescale();
        });
        h->addWidget(zoom_);
    }
    auto* copy = small_button(foot, NbGlyph::Copy, tr("COPY"), tr("Copy the figure"));
    connect(copy, &QToolButton::clicked, this, [this]() { ui::ChartExport::copy_image(pixmap_); });
    h->addWidget(copy);
    auto* save = small_button(foot, NbGlyph::Save, tr("SAVE PNG"), tr("Save the figure as PNG"));
    connect(save, &QToolButton::clicked, this,
            [this]() { ui::ChartExport::save_png(pixmap_, QStringLiteral("notebook_figure"), this); });
    h->addWidget(save);
    if (!large) {
        auto* expand = small_button(foot, NbGlyph::Expand, tr("EXPAND"), tr("Show this figure in the inspector"));
        connect(expand, &QToolButton::clicked, this, &NbOutputView::expand_requested);
        h->addWidget(expand);
    }
    v->addWidget(foot);
    rescale();
}

bool NbImageOutput::eventFilter(QObject* watched, QEvent* event) {
    if (watched == image_ && event->type() == QEvent::MouseButtonRelease)
        emit expand_requested();
    return NbOutputView::eventFilter(watched, event);
}

void NbImageOutput::rescale() {
    if (pixmap_.isNull())
        return;
    const int avail = large_ && scroll_ ? scroll_->viewport()->width() - 4 : width();
    if (avail <= 0 || avail == last_width_)
        return;
    last_width_ = avail;
    QPixmap shown = pixmap_;
    if (!actual_size_ && pixmap_.width() > avail)
        shown = pixmap_.scaledToWidth(avail, Qt::SmoothTransformation);
    image_->setPixmap(shown);
    image_->setFixedSize(shown.size());
}

void NbImageOutput::resizeEvent(QResizeEvent* e) {
    NbOutputView::resizeEvent(e);
    rescale();
}

void NbImageOutput::refit() {
    last_width_ = -1;
    rescale();
}

// ═════════════════════════════════════════════════════════════════════════════
// NbErrorOutput

NbErrorOutput::NbErrorOutput(const CellOutput& out, bool large, QWidget* parent)
    : NbOutputView(parent), out_(out), large_(large) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 2, 0, 2);
    auto* frame = new QFrame(this);
    frame->setObjectName(QStringLiteral("nbOutErr"));
    outer->addWidget(frame);
    auto* v = new QVBoxLayout(frame);
    v->setContentsMargins(10, 6, 10, 6);
    v->setSpacing(4);

    auto* title = new QLabel(frame);
    title->setObjectName(QStringLiteral("nbErrTitle"));
    title->setTextFormat(Qt::PlainText);
    title->setWordWrap(true);
    title->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QString msg = out.evalue.trimmed();
    title->setText(msg.isEmpty() ? out.ename : out.ename + QStringLiteral(": ") + msg);
    v->addWidget(title);

    const bool has_tb = !out.traceback.isEmpty();
    if (out.error_line > 0 || has_tb) {
        auto* row = new QHBoxLayout;
        row->setSpacing(10);
        if (out.error_line > 0) {
            auto* where = new QLabel(tr("Failed at line %1 of this cell").arg(out.error_line), frame);
            where->setObjectName(QStringLiteral("nbErrWhere"));
            row->addWidget(where);
            auto* go = new QPushButton(tr("GO TO LINE"), frame);
            go->setObjectName(QStringLiteral("nbLink"));
            go->setCursor(Qt::PointingHandCursor);
            const int line = out.error_line;
            connect(go, &QPushButton::clicked, this, [this, line]() { emit jump_to_line(line); });
            row->addWidget(go);
        }
        row->addStretch(1);
        if (has_tb) {
            toggle_ = new QPushButton(frame);
            toggle_->setObjectName(QStringLiteral("nbLink"));
            toggle_->setCursor(Qt::PointingHandCursor);
            row->addWidget(toggle_);
        }
        v->addLayout(row);
    }
    if (has_tb) {
        traceback_ = new QTextBrowser(frame);
        traceback_->setObjectName(QStringLiteral("nbTraceback"));
        traceback_->setLineWrapMode(QTextEdit::NoWrap);
        traceback_->setFrameShape(QFrame::NoFrame);
        traceback_->setOpenLinks(false);
        traceback_->setAccessibleName(tr("Traceback"));
        v->addWidget(traceback_);
        int lines = 0;
        for (const QString& l : out.traceback)
            lines += static_cast<int>(l.count(QLatin1Char('\n'))) + 1;
        const bool show = large || lines <= kTracebackCollapseAt;
        traceback_->setVisible(show);
        toggle_->setText(show ? tr("HIDE TRACEBACK") : tr("SHOW TRACEBACK (%1 lines)").arg(lines));
        connect(toggle_, &QPushButton::clicked, this, [this, lines]() {
            const bool show_now = !traceback_->isVisible();
            traceback_->setVisible(show_now);
            toggle_->setText(show_now ? tr("HIDE TRACEBACK") : tr("SHOW TRACEBACK (%1 lines)").arg(lines));
            fit();
        });
        render_traceback();
    }
}

void NbErrorOutput::render_traceback() {
    if (!traceback_)
        return;
    const QString frame_color = colors::DANGER();
    const QString code_bg = colors::tint(QColor(colors::DANGER()), 0.22);
    QString html = QStringLiteral("<pre style=\"margin:0;\">");
    bool next_is_code = false;
    for (const QString& entry : out_.traceback) {
        for (const QString& line : entry.split(QLatin1Char('\n'))) {
            const QString esc = line.toHtmlEscaped();
            if (line.contains(QLatin1String("File \"<cell>\""))) {
                html += QStringLiteral("<span style=\"color:%1;font-weight:700;\">%2</span>\n").arg(frame_color, esc);
                next_is_code = true;
            } else if (next_is_code && !line.trimmed().isEmpty()) {
                html += QStringLiteral("<span style=\"background:%1;\">%2</span>\n").arg(code_bg, esc);
                next_is_code = false;
            } else {
                html += esc + QLatin1Char('\n');
                next_is_code = false;
            }
        }
    }
    html += QStringLiteral("</pre>");
    traceback_->setHtml(html);
    fit();
}

void NbErrorOutput::fit() {
    if (!traceback_)
        return;
    traceback_->document()->adjustSize();
    const int doc_h = static_cast<int>(traceback_->document()->size().height()) + 6;
    const int cap = large_ ? 100000 : 360;
    int h = std::min(doc_h, cap);
    if (traceback_->horizontalScrollBar()->maximum() > 0)
        h += 8;
    traceback_->setFixedHeight(std::max(h, nb_output_line_height() + 6));
}

void NbErrorOutput::refresh_theme() {
    render_traceback(); // the HTML carries the colours it was built with
}

void NbErrorOutput::refit() {
    fit();
}

// ═════════════════════════════════════════════════════════════════════════════

NbOutputView* make_output_view(const CellOutput& out, bool large, QWidget* parent) {
    switch (out.kind) {
        case CellOutput::Kind::Table:
            if (!out.table.isEmpty())
                return new NbTableOutput(out, large, parent);
            return new NbTextOutput(out, large, parent);
        case CellOutput::Kind::Image:
            return new NbImageOutput(out, large, parent);
        case CellOutput::Kind::Error:
            return new NbErrorOutput(out, large, parent);
        case CellOutput::Kind::Result:
            if (out.text.isEmpty() && !out.extra_data.isEmpty()) {
                CellOutput note = out;
                note.text = NbOutputView::tr("[%1 output — not shown here]")
                                .arg(out.extra_data.keys().join(QStringLiteral(", ")));
                return new NbTextOutput(note, large, parent);
            }
            return new NbTextOutput(out, large, parent);
        case CellOutput::Kind::Stream:
            break;
    }
    return new NbTextOutput(out, large, parent);
}

} // namespace fincept::screens
