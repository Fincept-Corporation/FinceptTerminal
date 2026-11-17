// src/screens/equity_research/EquityFinancialsStatements.cpp
#include "screens/equity_research/EquityFinancialsStatements.h"

#include "screens/equity_research/EquityFinancialsCharts.h"
#include "screens/equity_research/EquityResearchUi.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/SparklineDelegate.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/PanelHeader.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QAbstractTableModel>
#include <QCoreApplication>
#include <QEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSet>
#include <QSplitter>
#include <QTableView>
#include <QVBoxLayout>

#include <climits>
#include <cmath>

namespace fincept::screens {

namespace {

namespace fs_fmt = fincept::ui::formatting;

struct FsText {
    Q_DECLARE_TR_FUNCTIONS(EquityFinancials)
};

bool fs_ok(double v) {
    return std::isfinite(v);
}

QString fs_label(const fin_model::Row& r) {
    return r.templated || r.kind == fin_model::Kind::Heading
               ? QCoreApplication::translate("EquityFinancials", r.label.toUtf8().constData())
               : r.label;
}

QString fs_statement_name(int st) {
    return st == 1 ? FsText::tr("balance sheet") : st == 2 ? FsText::tr("cash-flow statement") : FsText::tr("income statement");
}

} // namespace

// ── Model ────────────────────────────────────────────────────────────────────────────────────

class FinStatementTableModel : public QAbstractTableModel {
  public:
    explicit FinStatementTableModel(QObject* parent) : QAbstractTableModel(parent) {}

    void set_table(const fin_model::Table& raw, const fin_model::Table& shown, const QVector<fin_an::PeriodInfo>& periods,
                   int statement, const FinViewState& vs, int sel_col) {
        raw_ = raw;
        shown_ = shown;
        periods_ = periods;
        st_ = statement;
        vs_ = vs;
        sel_col_ = sel_col;
        // Pins and collapsed lines survive a basis / units switch when the lines still exist.
        QVector<int> pins;
        for (const QString& id : pinned_ids_) {
            const int r = fin_model::find_row(raw_.rows, id);
            if (r >= 0)
                pins.append(r);
        }
        pinned_ = pins;
        collapsed_.clear();
        for (const QString& id : collapsed_ids_) {
            const int r = fin_model::find_row(raw_.rows, id);
            if (r >= 0)
                collapsed_.insert(r);
        }
        rebuild();
    }
    void set_selected_col(int c) {
        if (c == sel_col_)
            return;
        sel_col_ = c;
        if (rowCount() > 0)
            emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
    }
    void set_filter(const QString& f) {
        filter_ = f.trimmed();
        rebuild();
    }
    bool collapsible(int src) const {
        if (src < 0 || src + 1 >= raw_.rows.size())
            return false;
        const auto& r = raw_.rows[src];
        const auto& next = raw_.rows[src + 1];
        if (r.kind == fin_model::Kind::Heading)
            return next.kind != fin_model::Kind::Heading;
        return !r.total && next.kind != fin_model::Kind::Heading && next.level > r.level;
    }
    void toggle(int view_row) {
        const int src = src_row(view_row);
        if (!collapsible(src) || !filter_.isEmpty())
            return;
        const QString id = raw_.rows[src].id;
        if (collapsed_.contains(src)) {
            collapsed_.remove(src);
            collapsed_ids_.removeAll(id);
        } else {
            collapsed_.insert(src);
            collapsed_ids_.append(id);
        }
        rebuild();
    }
    void set_collapsed_all(bool collapsed) {
        collapsed_.clear();
        collapsed_ids_.clear();
        if (collapsed)
            for (int i = 0; i < raw_.rows.size(); ++i)
                if (raw_.rows[i].kind == fin_model::Kind::Heading || collapsible(i)) {
                    if (raw_.rows[i].kind != fin_model::Kind::Heading && raw_.rows[i].level == 0)
                        continue;
                    collapsed_.insert(i);
                    collapsed_ids_.append(raw_.rows[i].id);
                }
        rebuild();
    }
    void toggle_pin(int view_row) {
        const int src = src_row(view_row);
        if (src < 0 || raw_.rows[src].kind == fin_model::Kind::Heading)
            return;
        const QString id = raw_.rows[src].id;
        if (pinned_.contains(src)) {
            pinned_.removeAll(src);
            pinned_ids_.removeAll(id);
        } else {
            pinned_.append(src);
            pinned_ids_.append(id);
        }
        rebuild();
    }
    bool is_pinned(int src) const { return pinned_.contains(src); }
    /// Source row of a view row (-1 for the PINNED heading).
    int src_row(int view_row) const {
        return view_row >= 0 && view_row < rows_.size() ? rows_[view_row].src : -1;
    }
    int view_row_of(int src) const {
        for (int i = 0; i < rows_.size(); ++i)
            if (rows_[i].src == src && !rows_[i].pinned)
                return i;
        return -1;
    }
    int period_count() const { return static_cast<int>(raw_.columns.size()); }
    int trend_col() const { return period_count() + 1; }
    int yoy_col() const { return period_count() + 2; }
    int cagr_col() const { return period_count() + 3; }
    const fin_model::Table& raw() const { return raw_; }
    const fin_model::Table& shown() const { return shown_; }
    const QVector<fin_an::PeriodInfo>& periods() const { return periods_; }

    int rowCount(const QModelIndex& parent = {}) const override {
        return parent.isValid() ? 0 : static_cast<int>(rows_.size());
    }
    int columnCount(const QModelIndex& parent = {}) const override {
        return parent.isValid() || raw_.columns.isEmpty() ? 0 : period_count() + 4;
    }

    QVariant headerData(int section, Qt::Orientation o, int role) const override {
        if (o != Qt::Horizontal)
            return {};
        const int n = period_count();
        if (role == Qt::DisplayRole) {
            if (section == 0)
                return FsText::tr("LINE ITEM");
            if (section >= 1 && section <= n)
                return raw_.columns[section - 1].label;
            if (section == trend_col())
                return FsText::tr("TREND");
            if (section == yoy_col())
                return FsText::tr("YOY");
            if (section == cagr_col())
                return FsText::tr("CAGR");
        } else if (role == Qt::ToolTipRole) {
            if (section >= 1 && section <= n)
                return raw_.columns[section - 1].tooltip + QStringLiteral("\n") + FsText::tr("Click to select this period");
            if (section == yoy_col())
                return FsText::tr("Selected period vs the same period a year earlier");
            if (section == cagr_col())
                return FsText::tr("Compound annual growth from the first positive reading to the selected period");
        } else if (role == Qt::TextAlignmentRole) {
            return section == 0 ? QVariant(Qt::AlignLeft | Qt::AlignVCenter) : QVariant(Qt::AlignCenter);
        } else if (role == er_ui::kHeaderDateRole && section >= 1 && section <= n) {
            return raw_.columns[section - 1].end.toString(Qt::ISODate);
        }
        return {};
    }

    QVariant data(const QModelIndex& idx, int role) const override {
        if (!idx.isValid() || idx.row() >= rows_.size())
            return {};
        const VRow& vr = rows_[idx.row()];
        const int c = idx.column();
        const int n = period_count();
        if (vr.src < 0) { // PINNED heading
            if (c == 0 && role == Qt::DisplayRole)
                return FsText::tr("PINNED  (right-click or P to unpin)");
            if (role == Qt::ForegroundRole)
                return QColor(ui::colors::AMBER());
            if (role == Qt::FontRole)
                return bold_font();
            if (role == Qt::BackgroundRole)
                return ui::colors::tint_color(QColor(ui::colors::AMBER()), 0.06);
            return {};
        }
        const fin_model::Row& r = raw_.rows[vr.src];
        const fin_model::Row& s = shown_.rows[vr.src];
        const bool heading = r.kind == fin_model::Kind::Heading;
        if (role == Qt::FontRole)
            return (heading || r.total) ? bold_font() : QVariant();
        if (role == Qt::BackgroundRole) {
            if (vr.pinned)
                return ui::colors::tint_color(QColor(ui::colors::AMBER()), 0.06);
            if (c >= 1 && c <= n && c - 1 == sel_col_)
                return ui::colors::tint_color(QColor(ui::colors::AMBER()), 0.10);
            return {};
        }
        if (c == 0) {
            if (role == Qt::DisplayRole) {
                QString mark = QStringLiteral("   ");
                if (filter_.isEmpty() && collapsible(vr.src))
                    mark = collapsed_.contains(vr.src) ? QStringLiteral("▸  ") : QStringLiteral("▾  ");
                return QString(r.level * 3, QLatin1Char(' ')) + mark + fs_label(r);
            }
            if (role == Qt::ForegroundRole)
                return QColor(heading     ? ui::colors::AMBER()
                              : r.total   ? ui::colors::TEXT_PRIMARY()
                              : r.templated ? ui::colors::TEXT_SECONDARY()
                                            : ui::colors::TEXT_TERTIARY());
            if (role == Qt::ToolTipRole)
                return heading ? FsText::tr("Click to collapse or expand this section")
                               : FsText::tr("%1\nYahoo line: \"%2\" · %3").arg(fs_label(r), r.id, fs_statement_name(st_));
            if (role == Qt::AccessibleTextRole)
                return fs_label(r);
            return {};
        }
        if (heading)
            return {};
        if (c >= 1 && c <= n) {
            const int k = c - 1;
            const double v = s.values.value(k, fin_an::kNaN);
            const double rv = r.values.value(k, fin_an::kNaN);
            if (role == Qt::DisplayRole)
                return cell_text(r, v, k);
            if (role == Qt::TextAlignmentRole)
                return QVariant(Qt::AlignRight | Qt::AlignVCenter);
            if (role == Qt::ForegroundRole) {
                if (!fs_ok(v))
                    return QColor(ui::colors::TEXT_TERTIARY());
                if (vs_.units == kFinGrowth)
                    return QColor(v > 0 ? ui::colors::POSITIVE() : v < 0 ? ui::colors::NEGATIVE() : ui::colors::TEXT_SECONDARY());
                return {};
            }
            if (role == Qt::ToolTipRole) {
                QString t = QStringLiteral("%1 · %2").arg(fs_label(r), raw_.columns[k].tooltip);
                if (fs_ok(rv))
                    t += QStringLiteral("\n") + FsText::tr("As reported: %1 %2").arg(fs_fmt::number(rv, std::fabs(rv) < 100 ? 2 : 0), vs_.ccy);
                else
                    t += QStringLiteral("\n") + FsText::tr("Not reported for this period");
                if (vs_.units == kFinCommonSize && fs_ok(v) && r.kind == fin_model::Kind::Amount)
                    t += QStringLiteral("\n") + (st_ == 1 ? FsText::tr("= %1 of total assets") : FsText::tr("= %1 of revenue"))
                                                    .arg(fs_fmt::fraction_pct(v, 2, false));
                t += QStringLiteral("\n") + FsText::tr("Source: Yahoo Finance · %1 · line \"%2\"").arg(fs_statement_name(st_), r.id);
                return t;
            }
            if (role == er_ui::kRawRole)
                return fs_ok(vs_.units == kFinReported ? rv : v) ? QVariant(vs_.units == kFinReported ? rv : v)
                                                                 : QVariant(QString());
            return {};
        }
        if (c == trend_col()) {
            if (role == ui::SparklineDelegate::kDefaultRole)
                return QVariant::fromValue(r.values);
            if (role == Qt::ToolTipRole)
                return FsText::tr("%1 — as reported, oldest to newest").arg(fs_label(r));
            return {};
        }
        if (c == yoy_col() || c == cagr_col()) {
            QString why;
            double v = fin_an::kNaN;
            if (c == yoy_col()) {
                const int ya = sel_col_ >= 0 && sel_col_ < periods_.size() ? periods_[sel_col_].year_ago : -1;
                if (ya < 0)
                    why = FsText::tr("n/a — no period a year earlier on file");
                else
                    v = fin_an::yoy_change(r.values.value(sel_col_, fin_an::kNaN), r.values.value(ya, fin_an::kNaN), &why);
            } else {
                v = r.kind == fin_model::Kind::Ratio ? fin_an::kNaN : fin_an::cagr(r.values, periods_, sel_col_, &why);
            }
            if (role == Qt::DisplayRole)
                return fs_ok(v) ? fs_fmt::fraction_pct(v, 1, true) : fs_fmt::placeholder();
            if (role == Qt::TextAlignmentRole)
                return QVariant(Qt::AlignRight | Qt::AlignVCenter);
            if (role == Qt::ForegroundRole)
                return QColor(!fs_ok(v) ? ui::colors::TEXT_TERTIARY()
                              : v > 0   ? ui::colors::POSITIVE()
                              : v < 0   ? ui::colors::NEGATIVE()
                                        : ui::colors::TEXT_SECONDARY());
            if (role == Qt::ToolTipRole)
                return fs_ok(v) ? QString() : why;
            return {};
        }
        return {};
    }

    Qt::ItemFlags flags(const QModelIndex& idx) const override {
        return idx.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags;
    }

  private:
    struct VRow {
        int src = -1;
        bool pinned = false;
    };

    static QFont bold_font() {
        QFont f = ui::fonts::font(-2);
        f.setBold(true);
        return f;
    }

    QString cell_text(const fin_model::Row& r, double v, int) const {
        if (!fs_ok(v))
            return fs_fmt::placeholder();
        if (vs_.units == kFinGrowth)
            return fs_fmt::fraction_pct(v, 1, true);
        if (vs_.units == kFinCommonSize && r.kind == fin_model::Kind::Amount)
            return fs_fmt::fraction_pct(v, 1, false);
        switch (r.kind) {
            case fin_model::Kind::PerShare:
                return fs_fmt::number(v, 2);
            case fin_model::Kind::Shares:
                return fs_fmt::compact(v, fs_fmt::Compact::FromSettings);
            case fin_model::Kind::Ratio:
                return fs_fmt::fraction_pct(v, 1, false);
            default:
                return fin_amount(v, vs_);
        }
    }

    bool matches(const fin_model::Row& r) const {
        return fs_label(r).contains(filter_, Qt::CaseInsensitive) || r.id.contains(filter_, Qt::CaseInsensitive);
    }

    void rebuild() {
        beginResetModel();
        rows_.clear();
        if (!pinned_.isEmpty()) {
            rows_.append({-1, false});
            for (int p : pinned_)
                rows_.append({p, true});
        }
        const bool filtering = !filter_.isEmpty();
        // Headings that have a match below them (filter mode).
        QSet<int> live_headings;
        if (filtering) {
            int heading = -1;
            for (int i = 0; i < raw_.rows.size(); ++i) {
                if (raw_.rows[i].kind == fin_model::Kind::Heading)
                    heading = i;
                else if (heading >= 0 && matches(raw_.rows[i]))
                    live_headings.insert(heading);
            }
        }
        bool heading_collapsed = false;
        int collapse_level = INT_MAX;
        for (int i = 0; i < raw_.rows.size(); ++i) {
            const auto& r = raw_.rows[i];
            if (r.kind == fin_model::Kind::Heading) {
                heading_collapsed = !filtering && collapsed_.contains(i);
                collapse_level = INT_MAX;
                if (!filtering || live_headings.contains(i))
                    rows_.append({i, false});
                continue;
            }
            if (filtering) {
                if (matches(r))
                    rows_.append({i, false});
                continue;
            }
            if (heading_collapsed)
                continue;
            if (r.level > collapse_level)
                continue;
            collapse_level = INT_MAX;
            rows_.append({i, false});
            if (collapsed_.contains(i) && collapsible(i))
                collapse_level = r.level;
        }
        endResetModel();
    }

    fin_model::Table raw_;
    fin_model::Table shown_;
    QVector<fin_an::PeriodInfo> periods_;
    int st_ = 0;
    FinViewState vs_;
    int sel_col_ = -1;
    QSet<int> collapsed_;
    QStringList collapsed_ids_;
    QVector<int> pinned_;
    QStringList pinned_ids_;
    QString filter_;
    QVector<VRow> rows_;
};

// ── Page ─────────────────────────────────────────────────────────────────────────────────────

FinStatementPage::FinStatementPage(int statement, QWidget* parent) : QWidget(parent), statement_(statement) {
    build_ui();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() { fit_columns(); });
}

void FinStatementPage::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(0);
    auto* split = new QSplitter(Qt::Horizontal, this);
    split->setChildrenCollapsible(false);
    split->setHandleWidth(8);

    table_panel_ = fin_panel(QString(), split, 0);
    filter_ = new QLineEdit;
    filter_->setObjectName(QStringLiteral("finFilter"));
    filter_->setClearButtonEnabled(true);
    filter_->setMinimumWidth(ui::metrics::width(170));
    table_panel_.header->add_control(filter_);
    expand_btn_ = new QPushButton;
    expand_btn_->setObjectName(QStringLiteral("finBtn"));
    expand_btn_->setCursor(Qt::PointingHandCursor);
    table_panel_.header->add_control(expand_btn_);

    model_ = new FinStatementTableModel(this);
    view_ = new QTableView(table_panel_.frame);
    view_->setObjectName(QStringLiteral("finTable"));
    view_->setModel(model_);
    view_->setAlternatingRowColors(true);
    view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    view_->setSelectionMode(QAbstractItemView::SingleSelection);
    view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view_->setWordWrap(false);
    view_->setShowGrid(false);
    view_->setFrameShape(QFrame::NoFrame);
    view_->verticalHeader()->hide();
    view_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    view_->setContextMenuPolicy(Qt::CustomContextMenu);
    view_->horizontalHeader()->setHighlightSections(false);
    view_->horizontalHeader()->setSectionsClickable(true);
    auto* spark = new ui::SparklineDelegate(view_);
    ui::SparklineStyle st;
    st.color_mode = ui::SparklineStyle::ColorMode::Accent;
    st.last_dot = true;
    spark->set_spark_style(st);
    spark->set_width_chars(9);
    view_->setItemDelegate(spark); // cells without a series paint as text
    view_->installEventFilter(this);
    table_panel_.body->addWidget(view_, 1);
    hint_ = new QLabel(table_panel_.frame);
    hint_->setObjectName(QStringLiteral("finNote"));
    hint_->setContentsMargins(8, 4, 8, 4);
    table_panel_.body->addWidget(hint_);
    split->addWidget(table_panel_.frame);

    line_panel_ = fin_panel(QString(), split, 8);
    line_chart_ = new fin_charts::FinComboChart(line_panel_.frame);
    line_chart_->setMinimumHeight(ui::metrics::height(220));
    line_panel_.body->addWidget(line_chart_, 1);
    line_stats_ = new QLabel(line_panel_.frame);
    line_stats_->setObjectName(QStringLiteral("finCaption"));
    line_stats_->setWordWrap(true);
    line_stats_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    line_panel_.body->addWidget(line_stats_);
    split->addWidget(line_panel_.frame);
    split->setStretchFactor(0, 13);
    split->setStretchFactor(1, 6);
    root->addWidget(split, 1);

    fs_fmt::refresh_model_on_format_change(model_);
    connect(filter_, &QLineEdit::textChanged, this, [this](const QString& t) { model_->set_filter(t); });
    connect(expand_btn_, &QPushButton::clicked, this, [this]() {
        all_collapsed_ = !all_collapsed_;
        model_->set_collapsed_all(all_collapsed_);
        retranslate();
    });
    connect(view_, &QTableView::clicked, this, [this](const QModelIndex& idx) {
        if (idx.column() == 0)
            model_->toggle(idx.row());
    });
    connect(view_->horizontalHeader(), &QHeaderView::sectionClicked, this, [this](int section) {
        if (section >= 1 && section <= model_->period_count()) {
            const int a = analysis_for(section - 1);
            if (a >= 0)
                emit period_selected(a);
        }
    });
    connect(view_, &QTableView::doubleClicked, this, [this](const QModelIndex& idx) {
        if (idx.column() >= 1 && idx.column() <= model_->period_count()) {
            const int a = analysis_for(idx.column() - 1);
            if (a >= 0)
                emit period_selected(a);
        }
    });
    connect(view_, &QWidget::customContextMenuRequested, this, &FinStatementPage::context_menu);
    connect(model_, &QAbstractItemModel::modelReset, this, [this]() {
        // Keep the charted line selected across a re-filter / collapse / pin.
        const int row = model_->view_row_of(current_src_row_);
        if (row >= 0)
            view_->selectRow(row);
    });
    connect(view_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex& cur) { show_line(cur.row()); });
    retranslate();
}

void FinStatementPage::retranslate() {
    const QString title = statement_ == 1 ? tr("BALANCE SHEET") : statement_ == 2 ? tr("CASH FLOW STATEMENT")
                                                                                   : tr("INCOME STATEMENT");
    table_panel_.header->set_title(title);
    view_->setAccessibleName(title);
    filter_->setPlaceholderText(tr("Filter lines…"));
    filter_->setAccessibleName(tr("Filter statement lines"));
    expand_btn_->setText(all_collapsed_ ? tr("EXPAND ALL") : tr("COLLAPSE ALL"));
    hint_->setText(tr("▸ / ▾ collapses a section · select a row to chart it · right-click or P pins it · click a "
                      "period header to select that period everywhere"));
    if (line_panel_.header->title().isEmpty())
        line_panel_.header->set_title(tr("LINE DETAIL"));
}

void FinStatementPage::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange) {
        retranslate();
        if (bundle_)
            set_data(bundle_, vs_);
    }
    QWidget::changeEvent(e);
}

bool FinStatementPage::eventFilter(QObject* watched, QEvent* e) {
    if (watched == view_ && e->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(e);
        if (ke->key() == Qt::Key_P && ke->modifiers() == Qt::NoModifier) {
            model_->toggle_pin(view_->currentIndex().row());
            return true;
        }
        if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter || ke->key() == Qt::Key_Space) &&
            view_->currentIndex().isValid()) {
            model_->toggle(view_->currentIndex().row());
            return true;
        }
    }
    return QWidget::eventFilter(watched, e);
}

void FinStatementPage::fit_columns() {
    if (!view_ || model_->columnCount() == 0)
        return;
    const QFontMetrics fm(ui::fonts::font(-2));
    const int ch = fm.horizontalAdvance(QLatin1Char('0'));
    auto* h = view_->horizontalHeader();
    h->setMinimumSectionSize(ch * 4);
    h->setSectionResizeMode(0, QHeaderView::Stretch);
    const int n = model_->period_count();
    const int period_w = std::max(ch * (vs_.units == kFinReported ? 11 : 9), fm.horizontalAdvance(QStringLiteral("LTM Q4 FY26")) + 16);
    for (int c = 1; c <= n; ++c) {
        h->setSectionResizeMode(c, QHeaderView::Fixed);
        view_->setColumnWidth(c, period_w);
    }
    h->setSectionResizeMode(model_->trend_col(), QHeaderView::Fixed);
    view_->setColumnWidth(model_->trend_col(), ch * 11);
    for (int c : {model_->yoy_col(), model_->cagr_col()}) {
        h->setSectionResizeMode(c, QHeaderView::Fixed);
        view_->setColumnWidth(c, ch * 9);
    }
    view_->verticalHeader()->setDefaultSectionSize(ui::metrics::height(24));
    // Stretch needs a label column at least this wide before the period columns scroll.
    view_->setMinimumWidth(std::min(ch * 34 + period_w * n + ch * 29, ui::metrics::width(900)));
}

int FinStatementPage::column_for(int analysis_index) const {
    if (!bundle_)
        return -1;
    const auto& a = bundle_->an[vs_.basis];
    if (analysis_index < 0 || analysis_index >= a.count())
        return -1;
    const QDate d = a.periods[analysis_index].end;
    const auto& cols = bundle_->stmt[vs_.basis][statement_].columns;
    for (int i = 0; i < cols.size(); ++i)
        if (qAbs(cols[i].end.daysTo(d)) <= 12)
            return i;
    return -1;
}

int FinStatementPage::analysis_for(int column) const {
    if (!bundle_)
        return -1;
    const auto& cols = bundle_->stmt[vs_.basis][statement_].columns;
    if (column < 0 || column >= cols.size())
        return -1;
    const auto& a = bundle_->an[vs_.basis];
    for (int i = 0; i < a.count(); ++i)
        if (qAbs(a.periods[i].end.daysTo(cols[column].end)) <= 12)
            return i;
    return -1;
}

void FinStatementPage::set_data(const FinBundlePtr& bundle, const FinViewState& vs) {
    bundle_ = bundle;
    vs_ = vs;
    if (!bundle_)
        return;
    const fin_model::Table& raw = bundle_->stmt[vs_.basis][statement_];
    const fin_model::Statement st = statement_ == 1   ? fin_model::Statement::Balance
                                    : statement_ == 2 ? fin_model::Statement::CashFlow
                                                      : fin_model::Statement::Income;
    const fin_model::View view = vs_.units == kFinCommonSize ? fin_model::View::CommonSize
                                 : vs_.units == kFinGrowth   ? fin_model::View::Growth
                                                             : fin_model::View::AsReported;
    int sel = column_for(vs_.selected);
    if (sel < 0)
        sel = static_cast<int>(raw.columns.size()) - 1;
    model_->set_table(raw, fin_model::apply_view(raw, st, view), bundle_->stmt_periods[vs_.basis][statement_], statement_,
                      vs_, sel);
    fit_columns();
    const QString unit = vs_.units == kFinGrowth       ? tr("YoY change")
                         : vs_.units == kFinCommonSize ? (statement_ == 1 ? tr("% of total assets") : tr("% of revenue"))
                                                       : tr("%1 %2").arg(vs_.ccy, vs_.scale_word);
    table_panel_.header->set_badge(raw.columns.isEmpty() ? tr("not reported for this basis")
                                                         : tr("%1 · %n period(s)", nullptr, static_cast<int>(raw.columns.size()))
                                                               .arg(unit));
    if (raw.rows.isEmpty()) {
        line_chart_->clear();
        line_chart_->set_state(ui::PaintedChart::Empty, tr("No %1 figures for this basis.").arg(fs_statement_name(statement_)));
        line_stats_->clear();
        return;
    }
    // Chart the line that was charted before (same id), else the first total (revenue / total assets / CFO).
    int src = -1;
    if (current_src_row_ >= 0)
        src = fin_model::find_row(raw.rows, line_panel_.header->property("finLineId").toString());
    if (src < 0)
        for (int i = 0; i < raw.rows.size(); ++i)
            if (raw.rows[i].total && raw.rows[i].kind == fin_model::Kind::Amount) {
                src = i;
                break;
            }
    current_src_row_ = src;
    const int row = model_->view_row_of(src);
    if (row >= 0) {
        view_->selectRow(row);
        show_line(row);
    }
}

void FinStatementPage::set_selected(int index) {
    vs_.selected = index;
    if (!bundle_)
        return;
    const int col = column_for(index);
    model_->set_selected_col(col >= 0 ? col : model_->period_count() - 1);
    if (current_src_row_ >= 0)
        show_line(model_->view_row_of(current_src_row_));
}

void FinStatementPage::show_line(int view_row) {
    const int src = model_->src_row(view_row);
    if (src < 0 || !bundle_)
        return;
    const fin_model::Table& raw = model_->raw();
    const fin_model::Row& r = raw.rows[src];
    if (r.kind == fin_model::Kind::Heading)
        return;
    current_src_row_ = src;
    const QString label = fs_label(r);
    line_panel_.header->set_title(label);
    line_panel_.header->setProperty("finLineId", r.id);
    const auto& periods = model_->periods();
    QStringList cats, tips;
    for (const auto& c : raw.columns) {
        cats << c.label;
        tips << c.tooltip;
    }
    const int n = static_cast<int>(cats.size());
    QVector<double> share(n, fin_an::kNaN), growth(n, fin_an::kNaN);
    for (int i = 0; i < n; ++i) {
        const double b = raw.common_base.value(i, fin_an::kNaN);
        if (r.kind == fin_model::Kind::Amount && fs_ok(r.values[i]) && fs_ok(b) && b != 0.0)
            share[i] = r.values[i] / std::fabs(b);
        const int ya = i < periods.size() ? periods[i].year_ago : -1;
        if (ya >= 0)
            growth[i] = fin_an::yoy_change(r.values[i], r.values[ya]);
    }
    line_chart_->set_categories(cats, tips);
    line_chart_->set_bars({{label, r.values, 0, {}}});
    QVector<fin_charts::ComboLine> lines;
    const QString share_name = statement_ == 1 ? tr("% of total assets") : tr("% of revenue");
    if (r.kind == fin_model::Kind::Amount)
        lines.append({share_name, share, 3, true, false, true});
    lines.append({tr("YoY growth"), growth, 4, true, true, true});
    line_chart_->set_lines(lines);
    const QString ccy = vs_.ccy;
    if (r.kind == fin_model::Kind::PerShare)
        line_chart_->set_primary_formatter([](double v) { return fs_fmt::number(v, 2); });
    else if (r.kind == fin_model::Kind::Shares)
        line_chart_->set_primary_formatter([](double v) { return fs_fmt::compact(v, fs_fmt::Compact::FromSettings); });
    else if (r.kind == fin_model::Kind::Ratio)
        line_chart_->set_primary_formatter([](double v) { return fs_fmt::fraction_pct(v, 1, false); });
    else
        line_chart_->set_primary_formatter([ccy](double v) { return fs_fmt::axis_money(v, ccy); });
    line_chart_->set_secondary_formatter([](double v) { return fs_fmt::fraction_pct(v, 1, false); });
    int sel = column_for(vs_.selected);
    if (sel < 0)
        sel = n - 1;
    line_chart_->set_selected(sel);
    line_chart_->set_state(ui::PaintedChart::Ready);
    disconnect(line_chart_, &fin_charts::FinChartBase::category_clicked, this, nullptr);
    connect(line_chart_, &fin_charts::FinChartBase::category_clicked, this, [this](int c) {
        const int a = analysis_for(c);
        if (a >= 0)
            emit period_selected(a);
    });

    // Key figures of the line at the selected period.
    const double v = r.values.value(sel, fin_an::kNaN);
    auto value_of = [&](double x) {
        switch (r.kind) {
            case fin_model::Kind::PerShare:
                return fin_value(x, fin_an::Unit::PerShare, ccy);
            case fin_model::Kind::Shares:
                return fin_value(x, fin_an::Unit::Shares, ccy);
            case fin_model::Kind::Ratio:
                return fin_value(x, fin_an::Unit::Percent, ccy);
            default:
                return fin_money(x, ccy);
        }
    };
    QString why_yoy, why_cagr;
    const int ya = sel >= 0 && sel < periods.size() ? periods[sel].year_ago : -1;
    const double yoy = ya >= 0 ? fin_an::yoy_change(v, r.values.value(ya, fin_an::kNaN), &why_yoy) : fin_an::kNaN;
    const double cg = fin_an::cagr(r.values, periods, sel, &why_cagr);
    double lo = fin_an::kNaN, hi = fin_an::kNaN;
    for (double x : r.values)
        if (fs_ok(x)) {
            lo = fs_ok(lo) ? std::min(lo, x) : x;
            hi = fs_ok(hi) ? std::max(hi, x) : x;
        }
    QStringList parts;
    parts << tr("%1: %2").arg(cats.value(sel), value_of(v));
    parts << tr("YoY %1").arg(fs_ok(yoy) ? fin_pct_signed(yoy) : (ya < 0 ? tr("n/a (no year-earlier period)") : fin_missing_text(why_yoy)));
    parts << tr("CAGR %1").arg(fs_ok(cg) ? fin_pct_signed(cg) : tr("n/a"));
    if (fs_ok(share.value(sel, fin_an::kNaN)))
        parts << QStringLiteral("%1 %2").arg(fs_fmt::fraction_pct(share[sel], 1, false), share_name);
    if (fs_ok(lo) && n > 1)
        parts << tr("range %1 – %2").arg(value_of(lo), value_of(hi));
    const QString text = parts.join(QStringLiteral(" · "));
    line_stats_->setText(text);
    line_chart_->set_summary(tr("%1 by period. %2").arg(label, text));
}

void FinStatementPage::context_menu(const QPoint& pos) {
    const QModelIndex idx = view_->indexAt(pos);
    const int src = model_->src_row(idx.row());
    if (!idx.isValid() || src < 0 || model_->raw().rows[src].kind == fin_model::Kind::Heading)
        return;
    QMenu menu(this);
    const bool pinned = model_->is_pinned(src);
    QAction* pin = menu.addAction(pinned ? tr("Unpin row") : tr("Pin row to the top"));
    QAction* chart = menu.addAction(tr("Chart this line"));
    QAction* chosen = menu.exec(view_->viewport()->mapToGlobal(pos));
    if (chosen == pin)
        model_->toggle_pin(idx.row());
    else if (chosen == chart)
        view_->selectRow(idx.row());
}

void FinStatementPage::export_rows(QStringList& headers, QVector<QStringList>& rows) const {
    headers.clear();
    rows.clear();
    if (!bundle_)
        return;
    const fin_model::Table& raw = model_->raw();
    const fin_model::Table& shown = model_->shown();
    const bool reported = vs_.units == kFinReported;
    headers << tr("Statement") << tr("Line item") << tr("Source line") << tr("Unit");
    for (const auto& c : raw.columns)
        headers << QStringLiteral("%1 (%2)").arg(c.label, c.end.toString(Qt::ISODate));
    for (int i = 0; i < raw.rows.size(); ++i) {
        const auto& r = raw.rows[i];
        QStringList row;
        row << fs_statement_name(statement_) << fs_label(r) << (r.kind == fin_model::Kind::Heading ? QString() : r.id);
        QString unit;
        if (r.kind != fin_model::Kind::Heading) {
            if (!reported && r.kind == fin_model::Kind::Amount)
                unit = QStringLiteral("fraction");
            else if (r.kind == fin_model::Kind::Shares)
                unit = QStringLiteral("shares");
            else if (r.kind == fin_model::Kind::Ratio || vs_.units == kFinGrowth)
                unit = QStringLiteral("fraction");
            else
                unit = vs_.ccy;
        }
        row << unit;
        for (int c = 0; c < raw.columns.size(); ++c) {
            if (r.kind == fin_model::Kind::Heading) {
                row << QString();
                continue;
            }
            const double v = reported ? r.values.value(c, fin_an::kNaN) : shown.rows[i].values.value(c, fin_an::kNaN);
            row << fs_fmt::export_number(v);
        }
        rows << row;
    }
}

} // namespace fincept::screens
