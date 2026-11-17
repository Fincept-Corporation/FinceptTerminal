#include "ui/widgets/SensitivityTable.h"

#include "ui/charts/ChartExport.h"
#include "ui/charts/MatrixHeatmap.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QAbstractTableModel>
#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::ui {

namespace {

/// Marker put in front of the base row / column labels of the MAP view.
const QString kSensBaseMark = QStringLiteral("◆ "); // ◆

QString sens_style() {
    return QString("#sensTable { background: transparent; }"
                   "#sensTitle { color: %1; font-size: " +
                   fonts::px(0) +
                   "px; font-weight: 700; background: transparent; }"
                   "#sensSub, #sensAxes, #sensNote { color: %2; font-size: " +
                   fonts::px(-2) +
                   "px; background: transparent; }"
                   "QPushButton#sensSeg { background: %3; color: %2; border: 1px solid %4; padding: 2px 8px;"
                   "  font-size: " +
                   fonts::px(-2) +
                   "px; font-weight: 600; }"
                   "QPushButton#sensSeg:checked { background: %5; color: %6; border-color: %5; }"
                   "QPushButton#sensBtn { background: transparent; color: %2; border: 1px solid %4; padding: 2px 8px;"
                   "  font-size: " +
                   fonts::px(-2) +
                   "px; font-weight: 600; }"
                   "QPushButton#sensBtn:hover, QPushButton#sensSeg:hover { color: %1; border-color: %5; }"
                   "QTableView#sensGrid { background: %7; color: %1; border: 1px solid %4; gridline-color: %4;"
                   "  font-family: %8; font-size: " +
                   fonts::px(-1) +
                   "px; }"
                   "QTableView#sensGrid QHeaderView::section { background: %3; color: %2; border: none;"
                   "  border-right: 1px solid %4; border-bottom: 1px solid %4; padding: 3px 6px; font-weight: 600;"
                   "  font-size: " +
                   fonts::px(-2) + "px; }")
        .arg(colors::TEXT_PRIMARY(), colors::TEXT_SECONDARY(), colors::BG_RAISED(), colors::BORDER_DIM(),
             colors::AMBER(), colors::TEXT_ON_ACCENT(), colors::BG_SURFACE(), fonts::DATA_FAMILY());
}

} // namespace

// ── Model ────────────────────────────────────────────────────────────────────
// Read-only view of the owner's grid; colours are computed from the live theme on every data()
// call, so a theme switch needs only a repaint.
class SensitivityTableModel : public QAbstractTableModel {
  public:
    SensitivityTableModel(const SensitivityGrid* grid, const int* metric, const SensitivityTable::Formatter* fallback,
                          QObject* parent)
        : QAbstractTableModel(parent), grid_(grid), metric_(metric), fallback_(fallback) {}

    void reset() {
        beginResetModel();
        endResetModel();
    }
    void repaint_all() {
        if (rowCount() > 0 && columnCount() > 0)
            emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
        emit headerDataChanged(Qt::Horizontal, 0, std::max(0, columnCount() - 1));
        emit headerDataChanged(Qt::Vertical, 0, std::max(0, rowCount() - 1));
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() || grid_->metrics.isEmpty() ? 0 : grid_->rows();
    }
    int columnCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() || grid_->metrics.isEmpty() ? 0 : grid_->cols();
    }

    QVariant data(const QModelIndex& idx, int role) const override {
        if (!idx.isValid())
            return {};
        const int m = *metric_;
        if (m < 0 || m >= grid_->metrics.size())
            return {};
        const double v = sensitivity_detail::value_at(grid_->metrics.at(m), idx.row(), idx.column());
        const bool base = grid_->has_base() && idx.row() == grid_->base_row && idx.column() == grid_->base_col;
        switch (role) {
            case Qt::DisplayRole:
            case Qt::ToolTipRole: {
                const QString text = sensitivity_detail::cell_text(*grid_, m, idx.row(), idx.column(), *fallback_);
                if (role == Qt::DisplayRole)
                    return text;
                QString tip = grid_->row_title + QStringLiteral(" ") + grid_->row_labels.value(idx.row()) +
                              QStringLiteral(" · ") + grid_->col_title + QStringLiteral(" ") +
                              grid_->col_labels.value(idx.column()) + QStringLiteral(": ") + text;
                if (base)
                    tip += QStringLiteral(" (") + QObject::tr("base case") + QStringLiteral(")");
                return tip;
            }
            case Qt::TextAlignmentRole:
                return int(Qt::AlignCenter);
            case Qt::BackgroundRole: {
                const double t = sensitivity_detail::tone(*grid_, m, v);
                if (!std::isfinite(t))
                    return QVariant();
                const bool div = grid_->metrics.at(m).diverging;
                return div ? colors::diverging_color(t * 0.85) : colors::sequential_color(t * 0.85);
            }
            case Qt::ForegroundRole: {
                const double t = sensitivity_detail::tone(*grid_, m, v);
                if (!std::isfinite(t))
                    return QColor(colors::TEXT_TERTIARY());
                const bool div = grid_->metrics.at(m).diverging;
                const QColor bg = div ? colors::diverging_color(t * 0.85) : colors::sequential_color(t * 0.85);
                return colors::contrast_text_on(bg);
            }
            case Qt::FontRole:
                if (base) {
                    QFont f = fonts::font(-1, QFont::Bold);
                    f.setFamily(fonts::DATA_FAMILY());
                    return f;
                }
                return {};
            default:
                return {};
        }
    }

    QVariant headerData(int section, Qt::Orientation o, int role) const override {
        const bool horiz = o == Qt::Horizontal;
        if (role == Qt::DisplayRole)
            return horiz ? grid_->col_labels.value(section) : grid_->row_labels.value(section);
        if (role == Qt::ToolTipRole)
            return horiz ? grid_->col_title : grid_->row_title;
        if (role == Qt::FontRole && grid_->has_base() && section == (horiz ? grid_->base_col : grid_->base_row))
            return fonts::font(-2, QFont::Bold);
        return {};
    }

    Qt::ItemFlags flags(const QModelIndex& idx) const override {
        return idx.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags;
    }

  private:
    const SensitivityGrid* grid_;
    const int* metric_;
    const SensitivityTable::Formatter* fallback_;
};

namespace {

/// Paints the base-case outline over the normal cell.
class SensBaseDelegate : public QStyledItemDelegate {
  public:
    SensBaseDelegate(const SensitivityGrid* grid, QObject* parent) : QStyledItemDelegate(parent), grid_(grid) {}

    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const override {
        // The model's background is the scale colour; selection must not hide it.
        QStyleOptionViewItem o(opt);
        o.state &= ~QStyle::State_Selected;
        o.state &= ~QStyle::State_HasFocus;
        const QVariant bg = idx.data(Qt::BackgroundRole);
        if (bg.canConvert<QColor>())
            p->fillRect(o.rect, bg.value<QColor>());
        QStyledItemDelegate::paint(p, o, idx);
        if (opt.state & QStyle::State_Selected) {
            p->save();
            p->setPen(QPen(QColor(colors::TEXT_PRIMARY()), 1, Qt::DotLine));
            p->setBrush(Qt::NoBrush);
            p->drawRect(QRectF(opt.rect).adjusted(1.5, 1.5, -1.5, -1.5));
            p->restore();
        }
        if (grid_->has_base() && idx.row() == grid_->base_row && idx.column() == grid_->base_col) {
            p->save();
            p->setRenderHint(QPainter::Antialiasing, false);
            p->setPen(QPen(QColor(colors::AMBER()), 2));
            p->setBrush(Qt::NoBrush);
            p->drawRect(QRectF(opt.rect).adjusted(1, 1, -2, -2));
            p->restore();
        }
    }

  private:
    const SensitivityGrid* grid_;
};

} // namespace

// ── Widget ───────────────────────────────────────────────────────────────────

SensitivityTable::SensitivityTable(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("sensTable"));
    setStyleSheet(sens_style()); // before any child exists
    build_ui();
    // Cell colours are read from the theme when painted: a repaint is all a theme switch needs.
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this, [this](const ThemeTokens&) {
        if (model_)
            model_->repaint_all();
    });
    // Column widths follow the font: re-fit once the appearance change has settled.
    connect(&ThemeManager::instance(), &ThemeManager::appearance_settled, this, [this]() { refit(); });
}

SensitivityTable::~SensitivityTable() = default;

void SensitivityTable::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(4);

    auto* head = new QWidget(this);
    auto* hl = new QHBoxLayout(head);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(6);
    auto* titles = new QVBoxLayout;
    titles->setSpacing(0);
    title_label_ = new QLabel(head);
    title_label_->setObjectName(QStringLiteral("sensTitle"));
    title_label_->setVisible(false);
    subtitle_label_ = new QLabel(head);
    subtitle_label_->setObjectName(QStringLiteral("sensSub"));
    subtitle_label_->setVisible(false);
    titles->addWidget(title_label_);
    titles->addWidget(subtitle_label_);
    hl->addLayout(titles, 1);

    metric_box_ = new QWidget(head);
    metric_layout_ = new QHBoxLayout(metric_box_);
    metric_layout_->setContentsMargins(0, 0, 0, 0);
    metric_layout_->setSpacing(0);
    metric_group_ = new QButtonGroup(this);
    metric_group_->setExclusive(true);
    connect(metric_group_, &QButtonGroup::idClicked, this, [this](int id) { set_current_metric(id); });
    metric_box_->setVisible(false);
    hl->addWidget(metric_box_);

    auto make_btn = [head](const QString& text, const char* name, bool checkable) {
        auto* b = new QPushButton(text, head);
        b->setObjectName(QString::fromLatin1(name));
        b->setCursor(Qt::PointingHandCursor);
        b->setCheckable(checkable);
        b->setFocusPolicy(Qt::TabFocus);
        return b;
    };
    table_btn_ = make_btn(tr("TABLE"), "sensSeg", true);
    map_btn_ = make_btn(tr("MAP"), "sensSeg", true);
    table_btn_->setChecked(true);
    table_btn_->setToolTip(tr("Exact values; the outlined cell is the base case"));
    map_btn_->setToolTip(tr("Colour map of the same values"));
    auto* view_group = new QButtonGroup(this);
    view_group->setExclusive(true);
    view_group->addButton(table_btn_);
    view_group->addButton(map_btn_);
    connect(table_btn_, &QPushButton::clicked, this, [this]() { set_view(View::Table); });
    connect(map_btn_, &QPushButton::clicked, this, [this]() { set_view(View::Map); });
    copy_btn_ = make_btn(tr("COPY"), "sensBtn", false);
    copy_btn_->setToolTip(tr("Copy the grid as tab-separated text (pastes into a spreadsheet)"));
    csv_btn_ = make_btn(tr("CSV"), "sensBtn", false);
    csv_btn_->setToolTip(tr("Save the grid as a CSV file"));
    connect(copy_btn_, &QPushButton::clicked, this, [this]() { copy_to_clipboard(); });
    connect(csv_btn_, &QPushButton::clicked, this, [this]() { save_csv(); });
    hl->addSpacing(6);
    hl->addWidget(table_btn_);
    hl->addWidget(map_btn_);
    hl->addSpacing(6);
    hl->addWidget(copy_btn_);
    hl->addWidget(csv_btn_);
    root->addWidget(head);

    axes_label_ = new QLabel(this);
    axes_label_->setObjectName(QStringLiteral("sensAxes"));
    axes_label_->setWordWrap(true);
    root->addWidget(axes_label_);

    stack_ = new QStackedWidget(this);
    table_ = new QTableView(stack_);
    table_->setObjectName(QStringLiteral("sensGrid"));
    model_ = new SensitivityTableModel(&grid_, &current_, &default_format_, table_);
    table_->setModel(model_);
    table_->setItemDelegate(new SensBaseDelegate(&grid_, table_));
    table_->setSelectionMode(QAbstractItemView::ContiguousSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setWordWrap(false);
    table_->setCornerButtonEnabled(false);
    table_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->horizontalHeader()->setHighlightSections(false);
    table_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table_->verticalHeader()->setHighlightSections(false);
    connect(table_, &QTableView::clicked, this, [this](const QModelIndex& i) { emit cell_clicked(i.row(), i.column()); });
    stack_->addWidget(table_);

    map_ = new MatrixHeatmap(stack_);
    map_->set_min_cell_size(18);
    connect(map_, &MatrixHeatmap::cell_clicked, this, &SensitivityTable::cell_clicked);
    stack_->addWidget(map_);
    root->addWidget(stack_, 1);

    note_label_ = new QLabel(this);
    note_label_->setObjectName(QStringLiteral("sensNote"));
    note_label_->setWordWrap(true);
    note_label_->setVisible(false);
    root->addWidget(note_label_);

    ChartExport::MenuSpec spec;
    spec.suggested_name = export_name_;
    spec.csv = [this]() { return ChartExport::Table{headers(), rows()}; };
    ChartExport::install_context_menu(table_, spec);
}

void SensitivityTable::set_title(const QString& title, const QString& subtitle) {
    title_ = title;
    subtitle_ = subtitle;
    title_label_->setText(title);
    title_label_->setVisible(!title.isEmpty());
    subtitle_label_->setText(subtitle);
    subtitle_label_->setVisible(!subtitle.isEmpty());
}

void SensitivityTable::set_axes(const QString& row_title, const QStringList& row_labels, const QString& col_title,
                                const QStringList& col_labels) {
    grid_.row_title = row_title;
    grid_.col_title = col_title;
    grid_.row_labels = row_labels;
    grid_.col_labels = col_labels;
    refresh();
}

void SensitivityTable::set_axes(const QString& row_title, const QVector<double>& row_values, const QString& col_title,
                                const QVector<double>& col_values, const Formatter& row_format,
                                const Formatter& col_format) {
    auto labels = [](const QVector<double>& vals, const Formatter& f) {
        QStringList out;
        out.reserve(vals.size());
        for (double v : vals)
            out << (f ? f(v) : QString::number(v, 'g', 6));
        return out;
    };
    set_axes(row_title, labels(row_values, row_format), col_title, labels(col_values, col_format));
}

void SensitivityTable::set_matrix(const QVector<QVector<double>>& values, const Formatter& format) {
    grid_.metrics.clear();
    SensitivityMetric m;
    m.values = values;
    m.format = format;
    grid_.metrics << m;
    current_ = 0;
    rebuild_metric_buttons();
    refresh();
}

void SensitivityTable::add_metric(const SensitivityMetric& metric) {
    grid_.metrics << metric;
    if (current_ < 0 || current_ >= grid_.metrics.size())
        current_ = 0;
    rebuild_metric_buttons();
    refresh();
}

void SensitivityTable::add_metric(const QString& name, const QVector<QVector<double>>& values, const Formatter& format,
                                  bool higher_is_better) {
    SensitivityMetric m;
    m.name = name;
    m.values = values;
    m.format = format;
    m.higher_is_better = higher_is_better;
    add_metric(m);
}

void SensitivityTable::clear() {
    grid_ = SensitivityGrid{};
    current_ = 0;
    rebuild_metric_buttons();
    refresh();
}

void SensitivityTable::set_current_metric(int index) {
    if (index < 0 || index >= grid_.metrics.size() || index == current_)
        return;
    current_ = index;
    if (auto* b = metric_group_->button(index))
        b->setChecked(true);
    refresh();
    emit metric_changed(index);
}

void SensitivityTable::set_base_cell(int row, int col) {
    grid_.base_row = row;
    grid_.base_col = col;
    refresh();
}

void SensitivityTable::set_formatter(const Formatter& format) {
    default_format_ = format;
    refresh();
}

void SensitivityTable::set_note(const QString& note) {
    note_label_->setText(note);
    note_label_->setVisible(!note.isEmpty());
}

void SensitivityTable::set_view(View view) {
    view_ = view;
    table_btn_->setChecked(view == View::Table);
    map_btn_->setChecked(view == View::Map);
    stack_->setCurrentIndex(view == View::Table ? 0 : 1);
}

void SensitivityTable::set_export_buttons_visible(bool on) {
    copy_btn_->setVisible(on);
    csv_btn_->setVisible(on);
}

void SensitivityTable::rebuild_metric_buttons() {
    for (QAbstractButton* b : metric_group_->buttons()) {
        metric_group_->removeButton(b);
        b->deleteLater();
    }
    const bool several = grid_.metrics.size() > 1;
    if (several) {
        for (int i = 0; i < grid_.metrics.size(); ++i) {
            QString text = grid_.metrics.at(i).name.toUpper();
            text.replace(QLatin1Char('&'), QLatin1String("&&"));
            auto* b = new QPushButton(text, metric_box_);
            b->setObjectName(QStringLiteral("sensSeg"));
            b->setCheckable(true);
            b->setCursor(Qt::PointingHandCursor);
            b->setChecked(i == current_);
            metric_group_->addButton(b, i);
            metric_layout_->addWidget(b);
        }
    }
    metric_box_->setVisible(several);
}

QString SensitivityTable::formatted(double v) const {
    if (!std::isfinite(v))
        return QStringLiteral("-");
    return default_format_ ? default_format_(v) : QString::number(v, 'g', 6);
}

void SensitivityTable::refresh() {
    model_->reset();
    QStringList axes;
    if (!grid_.row_title.isEmpty())
        axes << tr("Rows: %1").arg(grid_.row_title);
    if (!grid_.col_title.isEmpty())
        axes << tr("Columns: %1").arg(grid_.col_title);
    if (grid_.has_base())
        axes << tr("outlined cell = base case");
    axes_label_->setText(axes.join(QStringLiteral("  ·  ")));
    axes_label_->setVisible(!axes.isEmpty());
    copy_btn_->setEnabled(!grid_.is_empty());
    csv_btn_->setEnabled(!grid_.is_empty());

    // MAP: the same numbers through the chart kit's heatmap. A "smaller is better" metric is drawn
    // negated (its formatter undoes the sign), so the scale's good end means the same in both views.
    if (grid_.is_empty() || current_ < 0 || current_ >= grid_.metrics.size()) {
        map_->clear();
    } else {
        const SensitivityMetric& m = grid_.metrics.at(current_);
        const double sign = m.higher_is_better ? 1.0 : -1.0;
        QVector<QVector<double>> vals(grid_.rows());
        for (int r = 0; r < grid_.rows(); ++r) {
            vals[r].resize(grid_.cols());
            for (int c = 0; c < grid_.cols(); ++c)
                vals[r][c] = sign * sensitivity_detail::value_at(m, r, c);
        }
        QStringList rl = grid_.row_labels;
        QStringList cl = grid_.col_labels;
        if (grid_.has_base()) {
            rl[grid_.base_row] = kSensBaseMark + rl.at(grid_.base_row);
            cl[grid_.base_col] = kSensBaseMark + cl.at(grid_.base_col);
        }
        const Formatter fmt = m.format ? m.format : default_format_;
        map_->set_value_formatter([fmt, sign](double v) {
            const double x = sign * v;
            return fmt ? fmt(x) : QString::number(x, 'g', 6);
        });
        map_->set_matrix(rl, cl, vals);
        double lo = 0, hi = 0;
        if (sensitivity_detail::finite_range(m, &lo, &hi)) {
            if (m.diverging) {
                const double mid = sensitivity_detail::resolved_midpoint(grid_, current_);
                const double span = std::max(std::fabs(hi - mid), std::fabs(mid - lo));
                const double s = span > 0 ? span : 1.0;
                map_->set_diverging_scale(sign * mid - s, sign * mid + s, sign * mid);
            } else {
                map_->set_sequential_scale(std::min(sign * lo, sign * hi), std::max(sign * lo, sign * hi));
            }
        }
    }
    refit();
}

void SensitivityTable::refit() {
    // Row height and the table's height from the font; columns stretch (never resized to contents
    // on a fill). The widest cell text sets the minimum column width so values are never cut.
    const QFontMetrics fm(fonts::font(-1));
    const int row_h = fm.height() + 10;
    table_->verticalHeader()->setDefaultSectionSize(row_h);
    int widest = fm.horizontalAdvance(QStringLiteral("-00.00%"));
    for (const QStringList& r : rows())
        for (int c = 1; c < r.size(); ++c)
            widest = std::max(widest, fm.horizontalAdvance(r.at(c)));
    for (const QString& s : grid_.col_labels)
        widest = std::max(widest, QFontMetrics(fonts::font(-2, QFont::Bold)).horizontalAdvance(s));
    table_->horizontalHeader()->setMinimumSectionSize(widest + 16);
    int vh_w = 0;
    for (const QString& s : grid_.row_labels)
        vh_w = std::max(vh_w, QFontMetrics(fonts::font(-2, QFont::Bold)).horizontalAdvance(s));
    table_->verticalHeader()->setMinimumWidth(vh_w + 16);
    const int header_h = QFontMetrics(fonts::font(-2, QFont::Bold)).height() + 10;
    const int body = std::max(1, grid_.rows()) * row_h;
    const int h = header_h + body + 2 * table_->frameWidth() + 2;
    table_->setMinimumHeight(std::min(h, header_h + 12 * row_h));
    table_->setMaximumHeight(h + 2);
    map_->setMinimumHeight(std::min(std::max(160, h), 520));
    stack_->setMinimumHeight(table_->minimumHeight());
}

QString SensitivityTable::title() const {
    return title_;
}
QString SensitivityTable::subtitle() const {
    return subtitle_;
}

QStringList SensitivityTable::headers() const {
    return sensitivity_detail::headers(grid_);
}

QVector<QStringList> SensitivityTable::rows() const {
    return sensitivity_detail::rows(grid_, current_, default_format_);
}

QString SensitivityTable::to_tsv() const {
    return sensitivity_detail::to_tsv(grid_, current_, default_format_);
}

QPixmap SensitivityTable::grab_image() {
    return ChartExport::grab(this);
}

void SensitivityTable::copy_to_clipboard() const {
    if (grid_.is_empty())
        return;
    if (auto* cb = QApplication::clipboard())
        cb->setText(to_tsv());
}

bool SensitivityTable::save_csv() {
    if (grid_.is_empty())
        return false;
    QString name = export_name_;
    if (grid_.metrics.size() > 1 && !grid_.metrics.at(current_).name.isEmpty())
        name += QLatin1Char('_') + grid_.metrics.at(current_).name.toLower().replace(QLatin1Char(' '), QLatin1Char('_'));
    return ChartExport::save_csv(headers(), rows(), name, this);
}

QSize SensitivityTable::sizeHint() const {
    const QFontMetrics fm(fonts::font(-1));
    const int w = std::max(360, (std::max(grid_.cols(), 3) + 1) * (fm.horizontalAdvance(QStringLiteral("-00.00%")) + 24));
    return {w, QWidget::sizeHint().height()};
}

void SensitivityTable::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange) {
        table_btn_->setText(tr("TABLE"));
        map_btn_->setText(tr("MAP"));
        copy_btn_->setText(tr("COPY"));
        csv_btn_->setText(tr("CSV"));
        refresh();
    }
    QWidget::changeEvent(e);
}

} // namespace fincept::ui
