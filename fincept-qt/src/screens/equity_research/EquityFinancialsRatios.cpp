// src/screens/equity_research/EquityFinancialsRatios.cpp
#include "screens/equity_research/EquityFinancialsRatios.h"

#include "screens/equity_research/EquityFinancialsCharts.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/SparklineDelegate.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/PanelHeader.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QAbstractTableModel>
#include <QCoreApplication>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStyle>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace {

namespace fr_fmt = fincept::ui::formatting;

struct FrText {
    Q_DECLARE_TR_FUNCTIONS(EquityFinancials)
};

bool fr_ok(double v) {
    return std::isfinite(v);
}

void fr_tone(QLabel* l, const char* tone) {
    if (l->property("tone").toString() == QLatin1String(tone))
        return;
    l->setProperty("tone", QLatin1String(tone));
    l->style()->unpolish(l);
    l->style()->polish(l);
}

/// Distress | grey | safe zones on a horizontal scale with a marker at the value.
class FrZoneBar : public QWidget {
  public:
    explicit FrZoneBar(QWidget* parent) : QWidget(parent) {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    /// `low_cut` < `high_cut`; `higher_is_safe`: above high_cut = safe (Altman), else below
    /// low_cut = safe (Beneish). `labels` = {low zone, middle, high zone}.
    void set_zones(double low_cut, double high_cut, double value, bool higher_is_safe, const QStringList& labels,
                   const QString& value_text) {
        low_ = low_cut;
        high_ = high_cut;
        value_ = value;
        higher_safe_ = higher_is_safe;
        labels_ = labels;
        value_text_ = value_text;
        const double span = high_cut - low_cut;
        lo_ = std::min(low_cut - span * 1.6, fr_ok(value) ? value - span * 0.2 : low_cut);
        hi_ = std::max(high_cut + span * 1.6, fr_ok(value) ? value + span * 0.2 : high_cut);
        setFixedHeight(QFontMetrics(ui::fonts::font(-4)).height() * 3 + 10);
        update();
    }
    QSize sizeHint() const override { return {240, QFontMetrics(ui::fonts::font(-4)).height() * 3 + 10}; }

  protected:
    void paintEvent(QPaintEvent*) override {
        if (!fr_ok(low_) || hi_ <= lo_)
            return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QFont f = ui::fonts::font(-4);
        const QFontMetrics fm(f);
        p.setFont(f);
        const qreal top = fm.height() + 4, h = fm.height() + 2;
        const QRectF track(1, top, width() - 2, h);
        auto x_of = [&](double v) { return track.left() + (std::clamp(v, lo_, hi_) - lo_) / (hi_ - lo_) * track.width(); };
        const QColor danger(ui::colors::DANGER()), warn(ui::colors::WARNING()), ok(ui::colors::SUCCESS());
        const QColor left_c = higher_safe_ ? danger : ok, right_c = higher_safe_ ? ok : danger;
        const QRectF a(track.left(), top, x_of(low_) - track.left(), h), b(x_of(low_), top, x_of(high_) - x_of(low_), h),
            c(x_of(high_), top, track.right() - x_of(high_), h);
        p.setPen(Qt::NoPen);
        p.setBrush(ui::colors::tint_color(left_c, 0.35));
        p.drawRect(a);
        p.setBrush(ui::colors::tint_color(warn, 0.35));
        p.drawRect(b);
        p.setBrush(ui::colors::tint_color(right_c, 0.35));
        p.drawRect(c);
        p.setPen(QColor(ui::colors::TEXT_SECONDARY()));
        const QRectF rects[] = {a, b, c};
        for (int i = 0; i < 3 && i < labels_.size(); ++i) {
            const QString t = fm.elidedText(labels_[i], Qt::ElideRight, static_cast<int>(rects[i].width()) - 4);
            p.drawText(rects[i], Qt::AlignCenter, t);
        }
        // Cut values under the track.
        p.setPen(QColor(ui::colors::TEXT_TERTIARY()));
        const qreal below = track.bottom() + 2;
        for (double cut : {low_, high_}) {
            const QString t = fr_fmt::number(cut, 2);
            const qreal w = fm.horizontalAdvance(t);
            p.drawText(QRectF(x_of(cut) - w / 2, below, w + 2, fm.height()), Qt::AlignCenter, t);
        }
        if (!fr_ok(value_))
            return;
        // Marker: a triangle above the track at the value, the value text above it.
        const qreal x = x_of(value_);
        QPainterPath tri;
        tri.moveTo(x - 5, top - 6);
        tri.lineTo(x + 5, top - 6);
        tri.lineTo(x, top);
        tri.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(ui::colors::TEXT_PRIMARY()));
        p.drawPath(tri);
        p.setPen(QPen(QColor(ui::colors::TEXT_PRIMARY()), 2));
        p.drawLine(QPointF(x, top), QPointF(x, top + h));
        const qreal w = fm.horizontalAdvance(value_text_) + 4;
        const qreal tx = std::clamp(x - w / 2, 0.0, width() - w);
        p.drawText(QRectF(tx, 0, w, fm.height()), Qt::AlignCenter, value_text_);
    }

  private:
    double low_ = std::nan("");
    double high_ = std::nan("");
    double value_ = std::nan("");
    double lo_ = 0, hi_ = 1;
    bool higher_safe_ = true;
    QStringList labels_;
    QString value_text_;
};

/// Quantile with linear interpolation on sorted values.
double fr_quantile(const QVector<double>& sorted, double q) {
    if (sorted.isEmpty())
        return std::nan("");
    const double pos = q * (sorted.size() - 1);
    const int i = static_cast<int>(std::floor(pos));
    const double frac = pos - i;
    return i + 1 < sorted.size() ? sorted[i] + (sorted[i + 1] - sorted[i]) * frac : sorted[i];
}

} // namespace

// ── Ratio table model ────────────────────────────────────────────────────────────────────────

class FinRatioTableModel : public QAbstractTableModel {
  public:
    explicit FinRatioTableModel(QObject* parent) : QAbstractTableModel(parent) {}

    void set_data(const fin_an::Analysis& a, const FinViewState& vs, int sel) {
        beginResetModel();
        metrics_ = a.metrics;
        periods_ = a.periods;
        vs_ = vs;
        sel_ = sel;
        rows_.clear();
        hidden_ = 0;
        QString family;
        for (int i = 0; i < metrics_.size(); ++i) {
            if (!metrics_[i].s.any()) {
                ++hidden_;
                continue;
            }
            if (metrics_[i].family != family) {
                family = metrics_[i].family;
                rows_.append({-1, family});
            }
            rows_.append({i, family});
        }
        endResetModel();
    }
    void set_selected(int sel) {
        sel_ = sel;
        if (rowCount() > 0)
            emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
    }
    int hidden() const { return hidden_; }
    int period_count() const { return static_cast<int>(periods_.size()); }

    int rowCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : static_cast<int>(rows_.size()); }
    int columnCount(const QModelIndex& parent = {}) const override {
        return parent.isValid() || periods_.isEmpty() ? 0 : period_count() + 2;
    }
    QVariant headerData(int section, Qt::Orientation o, int role) const override {
        if (o != Qt::Horizontal)
            return {};
        const int n = period_count();
        if (role == Qt::DisplayRole) {
            if (section == 0)
                return FrText::tr("RATIO");
            if (section >= 1 && section <= n)
                return periods_[section - 1].label;
            return FrText::tr("TREND");
        }
        if (role == Qt::ToolTipRole && section >= 1 && section <= n)
            return periods_[section - 1].tooltip + QStringLiteral("\n") + FrText::tr("Click to select this period");
        if (role == Qt::TextAlignmentRole)
            return section == 0 ? QVariant(Qt::AlignLeft | Qt::AlignVCenter) : QVariant(Qt::AlignCenter);
        return {};
    }
    QVariant data(const QModelIndex& idx, int role) const override {
        if (!idx.isValid() || idx.row() >= rows_.size())
            return {};
        const auto& row = rows_[idx.row()];
        const int c = idx.column();
        const int n = period_count();
        if (row.metric < 0) {
            if (c == 0 && role == Qt::DisplayRole)
                return row.family;
            if (role == Qt::ForegroundRole)
                return QColor(ui::colors::AMBER());
            if (role == Qt::FontRole) {
                QFont f = ui::fonts::font(-3);
                f.setBold(true);
                return f;
            }
            return {};
        }
        const fin_an::Metric& m = metrics_[row.metric];
        if (c == 0) {
            if (role == Qt::DisplayRole)
                return QStringLiteral("   ") + m.label;
            if (role == Qt::ForegroundRole)
                return QColor(ui::colors::TEXT_SECONDARY());
            if (role == Qt::ToolTipRole)
                return m.formula + (m.averaged ? QStringLiteral("\n") + FrText::tr("Uses average balances (this and the previous period end).")
                                               : QString());
            return {};
        }
        if (c >= 1 && c <= n) {
            const int k = c - 1;
            const double v = m.s.at(k);
            if (role == Qt::DisplayRole)
                return fr_ok(v) ? fin_value(v, m.unit, vs_.ccy) : fin_missing_text(m.s.note_at(k));
            if (role == Qt::TextAlignmentRole)
                return QVariant(Qt::AlignRight | Qt::AlignVCenter);
            if (role == Qt::ForegroundRole)
                return fr_ok(v) ? QVariant() : QVariant(QColor(ui::colors::TEXT_TERTIARY()));
            if (role == Qt::BackgroundRole && k == sel_)
                return ui::colors::tint_color(QColor(ui::colors::AMBER()), 0.10);
            if (role == Qt::ToolTipRole) {
                QString t = QStringLiteral("%1 · %2\n%3").arg(m.label, periods_[k].tooltip, m.formula);
                if (!m.s.note_at(k).isEmpty())
                    t += QStringLiteral("\n") + m.s.note_at(k);
                return t;
            }
            return {};
        }
        if (role == ui::SparklineDelegate::kDefaultRole)
            return QVariant::fromValue(m.s.v);
        return {};
    }
    Qt::ItemFlags flags(const QModelIndex& idx) const override {
        return idx.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags;
    }

  private:
    struct Row {
        int metric = -1;
        QString family;
    };
    QVector<fin_an::Metric> metrics_;
    QVector<fin_an::PeriodInfo> periods_;
    FinViewState vs_;
    int sel_ = -1;
    QVector<Row> rows_;
    int hidden_ = 0;
};

// ── Ratios & quality page ────────────────────────────────────────────────────────────────────

FinRatiosPage::FinRatiosPage(QWidget* parent) : QWidget(parent) {
    build_ui();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() { fit_columns(); });
}

void FinRatiosPage::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(0);
    auto* split = new QSplitter(Qt::Horizontal, this);
    split->setChildrenCollapsible(false);
    split->setHandleWidth(8);

    table_panel_ = fin_panel(QString(), split, 0);
    model_ = new FinRatioTableModel(this);
    view_ = new QTableView(table_panel_.frame);
    view_->setObjectName(QStringLiteral("finTable"));
    view_->setModel(model_);
    view_->setAlternatingRowColors(true);
    view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    view_->setSelectionMode(QAbstractItemView::SingleSelection);
    view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view_->setShowGrid(false);
    view_->setWordWrap(false);
    view_->setFrameShape(QFrame::NoFrame);
    view_->verticalHeader()->hide();
    view_->horizontalHeader()->setHighlightSections(false);
    auto* spark = new ui::SparklineDelegate(view_);
    ui::SparklineStyle st;
    st.color_mode = ui::SparklineStyle::ColorMode::Accent;
    st.last_dot = true;
    spark->set_spark_style(st);
    spark->set_width_chars(9);
    view_->setItemDelegate(spark);
    table_panel_.body->addWidget(view_, 1);
    fr_fmt::refresh_model_on_format_change(model_);
    connect(view_->horizontalHeader(), &QHeaderView::sectionClicked, this, [this](int s) {
        if (s >= 1 && s <= model_->period_count())
            emit period_selected(s - 1);
    });
    split->addWidget(table_panel_.frame);

    auto* scroll = new QScrollArea(split);
    scroll->setObjectName(QStringLiteral("finScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* col = new QWidget(scroll);
    col->setObjectName(QStringLiteral("finScrollBody"));
    auto* cl = new QVBoxLayout(col);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(8);

    quality_panel_ = fin_panel(QString(), col, 8);
    auto* qw = new QWidget(quality_panel_.frame);
    quality_rows_ = new QGridLayout(qw);
    quality_rows_->setContentsMargins(0, 0, 0, 0);
    quality_rows_->setHorizontalSpacing(10);
    quality_rows_->setVerticalSpacing(4);
    quality_rows_->setColumnStretch(3, 1);
    quality_panel_.body->addWidget(qw);
    quality_note_ = new QLabel(quality_panel_.frame);
    quality_note_->setObjectName(QStringLiteral("finNote"));
    quality_note_->setWordWrap(true);
    quality_panel_.body->addWidget(quality_note_);
    cl->addWidget(quality_panel_.frame);

    auto make_card = [&](ScoreCard& card, bool bar) {
        card.panel = fin_panel(QString(), col, 8);
        auto* top = new QHBoxLayout;
        top->setSpacing(8);
        card.big = new QLabel(card.panel.frame);
        card.big->setObjectName(QStringLiteral("finBig"));
        top->addWidget(card.big, 0, Qt::AlignBottom);
        card.unit = new QLabel(card.panel.frame);
        card.unit->setObjectName(QStringLiteral("finBigUnit"));
        top->addWidget(card.unit, 0, Qt::AlignBottom);
        top->addSpacing(6);
        card.zone = new QLabel(card.panel.frame);
        card.zone->setObjectName(QStringLiteral("finChip"));
        top->addWidget(card.zone, 0, Qt::AlignVCenter);
        top->addStretch(1);
        card.panel.body->addLayout(top);
        card.variant = new QLabel(card.panel.frame);
        card.variant->setObjectName(QStringLiteral("finNote"));
        card.variant->setWordWrap(true);
        card.panel.body->addWidget(card.variant);
        if (bar) {
            card.bar = new FrZoneBar(card.panel.frame);
            card.panel.body->addWidget(card.bar);
        }
        auto* rw = new QWidget(card.panel.frame);
        card.rows = new QGridLayout(rw);
        card.rows->setContentsMargins(0, 2, 0, 0);
        card.rows->setHorizontalSpacing(10);
        card.rows->setVerticalSpacing(3);
        card.rows->setColumnStretch(1, 1);
        card.panel.body->addWidget(rw);
        cl->addWidget(card.panel.frame);
    };
    make_card(piotroski_, false);
    make_card(altman_, true);
    make_card(beneish_, true);
    cl->addStretch(1);
    scroll->setWidget(col);
    split->addWidget(scroll);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    root->addWidget(split, 1);
    retranslate();
}

void FinRatiosPage::retranslate() {
    table_panel_.header->set_title(tr("RATIOS BY FAMILY"));
    view_->setAccessibleName(tr("Ratios by family across periods"));
    quality_panel_.header->set_title(tr("EARNINGS QUALITY"));
    piotroski_.panel.header->set_title(tr("PIOTROSKI F-SCORE"));
    altman_.panel.header->set_title(tr("ALTMAN Z-SCORE"));
    beneish_.panel.header->set_title(tr("BENEISH M-SCORE"));
}

void FinRatiosPage::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange) {
        retranslate();
        if (bundle_)
            set_data(bundle_, vs_);
    }
    QWidget::changeEvent(e);
}

void FinRatiosPage::fit_columns() {
    if (model_->columnCount() == 0)
        return;
    const QFontMetrics fm(ui::fonts::font(-2));
    const int ch = fm.horizontalAdvance(QLatin1Char('0'));
    auto* h = view_->horizontalHeader();
    h->setSectionResizeMode(0, QHeaderView::Stretch);
    const int n = model_->period_count();
    const int w = std::max(ch * 10, fm.horizontalAdvance(QStringLiteral("LTM Q4 FY26")) + 16);
    for (int c = 1; c <= n; ++c) {
        h->setSectionResizeMode(c, QHeaderView::Fixed);
        view_->setColumnWidth(c, w);
    }
    h->setSectionResizeMode(n + 1, QHeaderView::Fixed);
    view_->setColumnWidth(n + 1, ch * 11);
    view_->verticalHeader()->setDefaultSectionSize(ui::metrics::height(24));
}

void FinRatiosPage::set_data(const FinBundlePtr& bundle, const FinViewState& vs) {
    bundle_ = bundle;
    vs_ = vs;
    if (!bundle_)
        return;
    const auto& a = bundle_->an[vs_.basis];
    const int sel = vs_.selected >= 0 && vs_.selected < a.count() ? vs_.selected : a.latest();
    model_->set_data(a, vs_, sel);
    fit_columns();
    table_panel_.header->set_badge(model_->hidden() > 0
                                       ? tr("%n not computable from the filing", nullptr, model_->hidden())
                                       : QString());
    fill_quality();
    fill_score(piotroski_, a.piotroski, true);
    fill_score(altman_, a.altman, false);
    fill_score(beneish_, a.beneish, false);
}

void FinRatiosPage::set_selected(int index) {
    vs_.selected = index;
    if (!bundle_)
        return;
    model_->set_selected(index);
    fill_quality();
}

QLabel* FinRatiosPage::cell(ScoreCard& card, int row, int col) {
    const int key = row * 3 + col;
    while (card.cells.size() <= key)
        card.cells.append(nullptr);
    if (!card.cells[key]) {
        auto* l = new QLabel(card.rows->parentWidget());
        l->setObjectName(col == 1 ? QStringLiteral("finKey") : QStringLiteral("finVal"));
        if (col == 2)
            l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        card.rows->addWidget(l, row, col);
        card.cells[key] = l;
    }
    return card.cells[key];
}

void FinRatiosPage::fill_quality() {
    const auto& a = bundle_->an[vs_.basis];
    const int sel = vs_.selected >= 0 && vs_.selected < a.count() ? vs_.selected : a.latest();
    for (auto* l : quality_cells_)
        l->hide();
    if (a.financial()) {
        quality_note_->setText(tr("Cash-flow quality tests are not meaningful for banks and insurers: their operating "
                                  "cash flow moves with trading, lending and deposit balances."));
        quality_panel_.header->set_badge(QString());
        return;
    }
    struct Q {
        const char* id;
        double ok_at;   ///< at or beyond this: OK
        double warn_at; ///< beyond this: WATCH
        bool higher_good;
        const char* hint;
    };
    static const Q kQ[] = {
        {"cfo_to_ni", 1.0, 0.8, true, QT_TRANSLATE_NOOP("EquityFinancials", "≥ 1.0x: profits backed by cash")},
        {"fcf_conversion", 0.9, 0.6, true, QT_TRANSLATE_NOOP("EquityFinancials", "≥ 90 %: earnings turn into free cash")},
        {"accruals_ratio", 0.0, 0.05, false, QT_TRANSLATE_NOOP("EquityFinancials", "≤ 0: earnings not ahead of cash")},
        {"capex_to_da", 1.0, 0.8, true, QT_TRANSLATE_NOOP("EquityFinancials", "< 0.8x may be under-investing")},
        {"sbc_to_revenue", 0.02, 0.05, false, QT_TRANSLATE_NOOP("EquityFinancials", "> 5 %: heavy non-cash pay")},
        {"ar_vs_revenue", 0.0, 0.10, false, QT_TRANSLATE_NOOP("EquityFinancials", "> 10 pp: receivables outgrow sales")},
    };
    int row = 0;
    auto put = [this](int r, int c, const QString& text, const QString& object, const QString& tip) {
        const int key = r * 4 + c;
        while (quality_cells_.size() <= key)
            quality_cells_.append(nullptr);
        QLabel*& l = quality_cells_[key];
        if (!l) {
            l = new QLabel(quality_rows_->parentWidget());
            quality_rows_->addWidget(l, r, c);
            if (c == 1)
                l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        }
        l->setObjectName(object);
        l->setText(text);
        l->setToolTip(tip);
        l->show();
        return l;
    };
    for (const auto& q : kQ) {
        const fin_an::Metric* m = a.metric(QLatin1String(q.id));
        if (!m)
            continue;
        const double v = m->s.at(sel);
        QString tip = m->formula;
        if (!m->s.note_at(sel).isEmpty())
            tip += QStringLiteral("\n") + m->s.note_at(sel);
        put(row, 0, m->label, QStringLiteral("finKey"), tip);
        put(row, 1, fr_ok(v) ? fin_value(v, m->unit, vs_.ccy) : fin_missing_text(m->s.note_at(sel)), QStringLiteral("finVal"),
            tip);
        QString verdict = fr_fmt::placeholder();
        const char* tone = "dim";
        if (fr_ok(v)) {
            const bool good = q.higher_good ? v >= q.ok_at : v <= q.ok_at;
            const bool bad = q.higher_good ? v < q.warn_at : v > q.warn_at;
            verdict = good ? tr("✓ OK") : bad ? tr("! WATCH") : tr("· FAIR");
            tone = good ? "ok" : bad ? "warn" : "dim";
        }
        QLabel* vl = put(row, 2, verdict, QStringLiteral("finVal"), tip);
        fr_tone(vl, tone);
        put(row, 3, QCoreApplication::translate("EquityFinancials", q.hint), QStringLiteral("finNote"), tip);
        ++row;
    }
    quality_panel_.header->set_badge(a.periods.value(sel).label);
    quality_note_->setText(tr("Thresholds are rules of thumb, not verdicts; hover a line for its formula."));
}

void FinRatiosPage::fill_score(ScoreCard& card, const fin_an::Score& s, bool piotroski) {
    for (auto* l : card.cells)
        if (l)
            l->hide();
    card.panel.header->set_badge(s.period_label);
    const bool ok = fr_ok(s.value);
    card.big->setText(ok ? (piotroski ? QString::number(static_cast<int>(s.value)) : fr_fmt::number(s.value, 2))
                         : tr("n/a"));
    card.unit->setText(piotroski && ok ? tr("/ %1").arg(s.available) : QString());
    card.unit->setVisible(piotroski && ok);
    card.big->setToolTip(s.formula);
    // Zone chip: words as well as colour.
    QString zone;
    const char* tone = "dim";
    if (ok) {
        if (s.id == QLatin1String("piotroski")) {
            zone = s.zone == fin_an::Zone::Safe ? tr("STRONG") : s.zone == fin_an::Zone::Distress ? tr("WEAK") : tr("MIXED");
        } else if (s.id == QLatin1String("beneish")) {
            zone = s.zone == fin_an::Zone::Safe       ? tr("UNLIKELY MANIPULATOR")
                   : s.zone == fin_an::Zone::Distress ? tr("FLAG: LIKELY MANIPULATOR")
                                                      : tr("GREY ZONE");
        } else {
            zone = fin_an::zone_text(s.zone).toUpper();
        }
        tone = s.zone == fin_an::Zone::Safe ? "ok" : s.zone == fin_an::Zone::Distress ? "danger" : "warn";
    }
    card.zone->setText(zone);
    card.zone->setVisible(ok);
    fr_tone(card.zone, tone);
    QStringList variant;
    if (!s.variant.isEmpty())
        variant << s.variant;
    if (!s.note.isEmpty())
        variant << s.note;
    if (variant.isEmpty())
        variant << s.formula;
    card.variant->setText(variant.join(QStringLiteral(" · ")));
    card.variant->setToolTip(s.formula);
    if (card.bar) {
        auto* bar = static_cast<FrZoneBar*>(card.bar);
        if (ok && fr_ok(s.safe) && fr_ok(s.distress)) {
            const bool higher_safe = s.id != QLatin1String("beneish");
            const double lo = std::min(s.safe, s.distress), hi = std::max(s.safe, s.distress);
            const QStringList labels = higher_safe ? QStringList{tr("DISTRESS"), tr("GREY"), tr("SAFE")}
                                                   : QStringList{tr("UNLIKELY"), tr("GREY"), tr("LIKELY")};
            bar->set_zones(lo, hi, s.value, higher_safe, labels, fr_fmt::number(s.value, 2));
            bar->show();
        } else {
            bar->hide();
        }
    }
    int row = 0;
    for (const auto& c : s.parts) {
        QString value_text;
        if (piotroski) {
            const bool share = c.id == QLatin1String("no_dilution");
            const bool multiple = c.id == QLatin1String("liquidity_up") || c.id == QLatin1String("turnover_up");
            const fin_an::Unit u = share ? fin_an::Unit::Shares : multiple ? fin_an::Unit::Multiple : fin_an::Unit::Percent;
            value_text = fin_value(c.value, u, vs_.ccy);
            if (fr_ok(c.prior))
                value_text += tr(" vs %1").arg(fin_value(c.prior, u, vs_.ccy));
            QLabel* mark = cell(card, row, 0);
            mark->setText(c.pass == 1 ? tr("✓ PASS") : c.pass == 0 ? tr("✗ FAIL") : tr("– N/A"));
            fr_tone(mark, c.pass == 1 ? "ok" : c.pass == 0 ? "danger" : "dim");
            mark->setToolTip(c.note);
            mark->show();
        } else {
            value_text = fr_ok(c.value) ? fr_fmt::number(c.value, 3) : fin_missing_text(c.note);
            QLabel* mark = cell(card, row, 0);
            mark->setText(QString());
            mark->hide();
        }
        QLabel* label = cell(card, row, 1);
        label->setText(c.label);
        label->setToolTip(c.formula + (c.note.isEmpty() ? QString() : QStringLiteral("\n") + c.note));
        label->show();
        QLabel* val = cell(card, row, 2);
        val->setText(c.pass < 0 && piotroski && !fr_ok(c.value) ? fin_missing_text(c.note) : value_text);
        val->setToolTip(label->toolTip());
        val->show();
        ++row;
    }
}

// ── Peers page ───────────────────────────────────────────────────────────────────────────────

FinPeersPage::FinPeersPage(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);
    panel_ = fin_panel(QString(), this, 10);
    load_btn_ = new QPushButton;
    load_btn_->setObjectName(QStringLiteral("finBtn"));
    load_btn_->setCursor(Qt::PointingHandCursor);
    panel_.header->add_control(load_btn_);
    connect(load_btn_, &QPushButton::clicked, this, &FinPeersPage::peers_requested);
    bullets_ = new fin_charts::FinBulletChart(panel_.frame);
    panel_.body->addWidget(bullets_);
    note_ = new QLabel(panel_.frame);
    note_->setObjectName(QStringLiteral("finNote"));
    note_->setWordWrap(true);
    panel_.body->addWidget(note_);
    panel_.body->addStretch(1);
    root->addWidget(panel_.frame, 1);
    retranslate();
    render();
}

void FinPeersPage::retranslate() {
    panel_.header->set_title(tr("PEERS IN CONTEXT"));
    load_btn_->setText(peers_.isEmpty() ? tr("LOAD PEER GROUP") : tr("RELOAD PEERS"));
    load_btn_->setToolTip(tr("Finds same-industry peers and loads their ratios from Yahoo Finance (free, cached and "
                             "shared with the Peers and Valuation tabs)"));
    bullets_->setAccessibleName(tr("Company versus peer median"));
}

void FinPeersPage::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange) {
        retranslate();
        render();
    }
    QWidget::changeEvent(e);
}

void FinPeersPage::set_data(const FinBundlePtr& bundle, const FinViewState& vs) {
    bundle_ = bundle;
    vs_ = vs;
    render();
}

void FinPeersPage::set_peers(const QVector<services::equity::PeerData>& peers) {
    peers_ = peers;
    state_ = PeerState::None;
    message_.clear();
    retranslate();
    render();
}

void FinPeersPage::set_state(PeerState state, const QString& message) {
    state_ = state;
    message_ = message;
    load_btn_->setEnabled(state != PeerState::Loading);
    render();
}

void FinPeersPage::render() {
    const QString symbol = bundle_ ? bundle_->symbol : QString();
    const services::equity::PeerData* self = nullptr;
    QVector<const services::equity::PeerData*> others;
    for (const auto& p : peers_) {
        if (p.symbol.compare(symbol, Qt::CaseInsensitive) == 0)
            self = &p;
        else
            others.append(&p);
    }
    if (state_ == PeerState::Loading) {
        bullets_->clear();
        bullets_->set_state(ui::PaintedChart::Loading, tr("Finding industry peers and loading their ratios…"));
        note_->clear();
        return;
    }
    if (state_ == PeerState::Failed) {
        bullets_->clear();
        bullets_->set_state(ui::PaintedChart::Empty, message_.isEmpty() ? tr("Peer data could not be loaded.") : message_);
        note_->setText(tr("Press RELOAD PEERS to try again."));
        return;
    }
    if (!self || others.size() < 2) {
        bullets_->clear();
        bullets_->set_state(ui::PaintedChart::Empty, tr("No peer group loaded for %1 yet.").arg(symbol));
        note_->setText(tr("Peer medians come from the same free Yahoo peer group the Peers and Valuation tabs use. "
                          "LOAD PEER GROUP fetches it once; it is cached and shared, so opening the Peers tab "
                          "afterwards costs nothing. If the Peers tab already loaded it, it appears here by itself."));
        panel_.header->set_badge(QString());
        return;
    }
    struct Def {
        const char* label;
        double (*get)(const services::equity::PeerData&);
        bool percent;
        bool higher_better;
    };
    static const Def kDefs[] = {
        {QT_TRANSLATE_NOOP("EquityFinancials", "Gross margin"), [](const services::equity::PeerData& p) { return p.gross_margin; }, true, true},
        {QT_TRANSLATE_NOOP("EquityFinancials", "Operating margin"), [](const services::equity::PeerData& p) { return p.operating_margin; }, true, true},
        {QT_TRANSLATE_NOOP("EquityFinancials", "Net margin"), [](const services::equity::PeerData& p) { return p.profit_margin; }, true, true},
        {QT_TRANSLATE_NOOP("EquityFinancials", "Revenue growth"), [](const services::equity::PeerData& p) { return p.revenue_growth; }, true, true},
        {QT_TRANSLATE_NOOP("EquityFinancials", "Earnings growth"), [](const services::equity::PeerData& p) { return p.earnings_growth; }, true, true},
        {QT_TRANSLATE_NOOP("EquityFinancials", "Return on equity"), [](const services::equity::PeerData& p) { return p.roe; }, true, true},
        {QT_TRANSLATE_NOOP("EquityFinancials", "Return on assets"), [](const services::equity::PeerData& p) { return p.roa; }, true, true},
        {QT_TRANSLATE_NOOP("EquityFinancials", "Debt ÷ equity"), [](const services::equity::PeerData& p) { return p.debt_to_equity / 100.0; }, false, false},
        {QT_TRANSLATE_NOOP("EquityFinancials", "Current ratio"), [](const services::equity::PeerData& p) { return p.current_ratio; }, false, true},
    };
    // Yahoo leaves a missing ratio at 0: zero is "not reported" here.
    auto usable = [](double v) { return std::isfinite(v) && v != 0.0; };
    QVector<fin_charts::FinBulletChart::Row> rows;
    QStringList summary;
    for (const auto& d : kDefs) {
        const double v = d.get(*self);
        QVector<double> vals;
        for (const auto* p : others)
            if (usable(d.get(*p)))
                vals.append(d.get(*p));
        if (!usable(v) || vals.size() < 2)
            continue;
        std::sort(vals.begin(), vals.end());
        fin_charts::FinBulletChart::Row r;
        r.label = QCoreApplication::translate("EquityFinancials", d.label);
        r.value = v;
        r.median = fr_quantile(vals, 0.5);
        r.lo = vals.first();
        r.hi = vals.last();
        if (vals.size() >= 4) {
            r.q1 = fr_quantile(vals, 0.25);
            r.q3 = fr_quantile(vals, 0.75);
        }
        r.higher_is_better = d.higher_better;
        const fin_an::Unit u = d.percent ? fin_an::Unit::Percent : fin_an::Unit::Multiple;
        r.value_text = fin_value(v, u, {});
        r.median_text = fin_value(r.median, u, {});
        r.tip = tr("%1 of %2 peers report it · range %3 – %4 · trailing figures from Yahoo Finance for every company")
                    .arg(vals.size())
                    .arg(others.size())
                    .arg(fin_value(r.lo, u, {}), fin_value(r.hi, u, {}));
        rows.append(r);
        if (summary.size() < 3)
            summary << tr("%1 %2 vs median %3").arg(r.label, r.value_text, r.median_text);
    }
    QStringList names;
    for (const auto* p : others)
        names << p->symbol;
    panel_.header->set_badge(tr("%n peer(s)", nullptr, static_cast<int>(others.size())));
    if (rows.isEmpty()) {
        bullets_->clear();
        bullets_->set_state(ui::PaintedChart::Empty, tr("The peers report none of the compared ratios."));
    } else {
        bullets_->set_state(ui::PaintedChart::Ready);
        bullets_->set_rows(rows);
        bullets_->set_summary(summary.join(QStringLiteral("; ")));
        bullets_->setMinimumHeight(bullets_->sizeHint().height());
    }
    note_->setText(tr("Peers: %1. Trailing-twelve-month ratios from Yahoo Finance for this company and its peers alike "
                      "(so they may differ slightly from the statement-based figures on the other sections). Band = "
                      "peer range, darker band = middle half, tick = median.")
                       .arg(names.join(QStringLiteral(", "))));
}

} // namespace fincept::screens
