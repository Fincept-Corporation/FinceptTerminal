#include "screens/watchlist/WatchlistAnalyticsViews.h"

#include "ui/charts/HeatmapGrid.h"
#include "ui/charts/MatrixHeatmap.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::screens {

namespace fmt = fincept::ui::formatting;
namespace colors = fincept::ui::colors;
namespace fonts = fincept::ui::fonts;

namespace {

struct WlaSeries {
    QVector<qint64> days; ///< UTC day number of each return
    QVector<double> rets; ///< log return from the previous bar
};

WlaSeries wla_returns(const QVector<services::HistoryPoint>& bars, qint64 from_day) {
    QVector<QPair<qint64, double>> closes;
    closes.reserve(bars.size());
    for (const auto& b : bars) {
        if (b.timestamp > 0 && b.close > 0)
            closes.append({b.timestamp / 86400, b.close});
    }
    std::sort(closes.begin(), closes.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    WlaSeries s;
    for (int i = 1; i < closes.size(); ++i) {
        if (closes[i].first == closes[i - 1].first || closes[i].first < from_day)
            continue;
        s.days.append(closes[i].first);
        s.rets.append(std::log(closes[i].second / closes[i - 1].second));
    }
    return s;
}

double wla_pearson(const WlaSeries& a, const WlaSeries& b, int min_overlap) {
    // Merge-intersect the two sorted day lists.
    double sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0;
    int n = 0;
    int i = 0;
    int j = 0;
    while (i < a.days.size() && j < b.days.size()) {
        if (a.days[i] < b.days[j]) {
            ++i;
        } else if (b.days[j] < a.days[i]) {
            ++j;
        } else {
            const double x = a.rets[i];
            const double y = b.rets[j];
            sx += x;
            sy += y;
            sxx += x * x;
            syy += y * y;
            sxy += x * y;
            ++n;
            ++i;
            ++j;
        }
    }
    if (n < min_overlap)
        return std::numeric_limits<double>::quiet_NaN();
    const double cov = sxy - sx * sy / n;
    const double vx = sxx - sx * sx / n;
    const double vy = syy - sy * sy / n;
    if (vx <= 0 || vy <= 0)
        return std::numeric_limits<double>::quiet_NaN();
    return std::clamp(cov / std::sqrt(vx * vy), -1.0, 1.0);
}

QString wla_combo_style() {
    return QStringLiteral("QComboBox { background:%1; color:%2; border:1px solid %3; padding:2px 6px; font-size:%4px; }")
        .arg(colors::BG_SURFACE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM())
        .arg(fonts::px(-2));
}

QString wla_label_style() {
    return QStringLiteral("background:transparent; color:%1; font-size:%2px; font-weight:700;")
        .arg(colors::TEXT_SECONDARY())
        .arg(fonts::px(-3));
}

QString wla_bar_style() {
    return QStringLiteral("background:%1; border-bottom:1px solid %2;").arg(colors::BG_SURFACE(), colors::BORDER_DIM());
}

/// setStyleSheet() re-polishes the widget and its subtree even for unchanged text. Each view
/// runs refresh_theme() at the end of its constructor and WatchlistScreen runs it again right
/// after: the second pass (and any theme signal that changes nothing here) is now free.
void wla_set_css(QWidget* w, const QString& css) {
    if (w && w->styleSheet() != css)
        w->setStyleSheet(css);
}

} // namespace

QVector<QVector<double>> watchlist_correlation(const QStringList& symbols,
                                               const QHash<QString, QVector<services::HistoryPoint>>& series,
                                               int window_days, int min_overlap) {
    qint64 last_day = 0;
    for (const QString& s : symbols) {
        for (const auto& b : series.value(s))
            last_day = std::max(last_day, b.timestamp / 86400);
    }
    const qint64 from_day = last_day - window_days;
    QVector<WlaSeries> rets;
    rets.reserve(symbols.size());
    for (const QString& s : symbols)
        rets.append(wla_returns(series.value(s), from_day));

    const int n = static_cast<int>(symbols.size());
    QVector<QVector<double>> m(n, QVector<double>(n, std::numeric_limits<double>::quiet_NaN()));
    for (int i = 0; i < n; ++i) {
        if (rets[i].rets.size() >= min_overlap)
            m[i][i] = 1.0;
        for (int j = i + 1; j < n; ++j) {
            const double r = wla_pearson(rets[i], rets[j], min_overlap);
            m[i][j] = r;
            m[j][i] = r;
        }
    }
    return m;
}

// ── WatchlistMapView ────────────────────────────────────────────────────────

double WatchlistMapView::clamp_for(const QString& horizon) {
    if (horizon == QLatin1String("1w"))
        return 5.0;
    if (horizon == QLatin1String("1m"))
        return 10.0;
    if (horizon == QLatin1String("3m"))
        return 20.0;
    if (horizon == QLatin1String("ytd"))
        return 30.0;
    if (horizon == QLatin1String("1y"))
        return 40.0;
    return 3.0;
}

WatchlistMapView::WatchlistMapView(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    bar_ = new QWidget(this);
    bar_->setStyleSheet(wla_bar_style()); // before its children exist
    auto* bl = new QHBoxLayout(bar_);
    bl->setContentsMargins(12, 4, 12, 4);
    bl->setSpacing(6);
    size_label_ = new QLabel(bar_);
    size_box_ = new QComboBox(bar_);
    size_box_->addItem(QString(), QStringLiteral("cap"));
    size_box_->addItem(QString(), QStringLiteral("equal"));
    horizon_label_ = new QLabel(bar_);
    horizon_box_ = new QComboBox(bar_);
    for (const char* h : {"1d", "1w", "1m", "3m", "ytd", "1y"})
        horizon_box_->addItem(QString(), QString::fromLatin1(h));
    note_ = new QLabel(bar_);
    bl->addWidget(size_label_);
    bl->addWidget(size_box_);
    bl->addSpacing(10);
    bl->addWidget(horizon_label_);
    bl->addWidget(horizon_box_);
    bl->addSpacing(10);
    bl->addWidget(note_, 1);
    root->addWidget(bar_);

    stack_ = new QStackedWidget(this);
    treemap_ = new ui::Treemap(stack_);
    grid_ = new ui::HeatmapGrid(stack_);
    grid_->set_show_legend(true);
    stack_->addWidget(treemap_);
    auto* grid_scroll = new QScrollArea(stack_);
    grid_scroll->setWidgetResizable(true);
    grid_scroll->setFrameShape(QFrame::NoFrame);
    grid_scroll->setWidget(grid_);
    stack_->addWidget(grid_scroll);
    root->addWidget(stack_, 1);

    const auto pct_fmt = [](double v) { return fmt::pct(v, 2); };
    treemap_->set_value_formatter(pct_fmt);
    grid_->set_value_formatter(pct_fmt);
    treemap_->set_weight_formatter([](double v) { return fmt::compact(v); });
    connect(treemap_, &ui::Treemap::item_clicked, this, &WatchlistMapView::symbol_clicked);
    connect(treemap_, &ui::Treemap::item_double_clicked, this, &WatchlistMapView::symbol_activated);
    connect(grid_, &ui::HeatmapGrid::item_clicked, this, &WatchlistMapView::symbol_clicked);
    connect(grid_, &ui::HeatmapGrid::item_double_clicked, this, &WatchlistMapView::symbol_activated);

    const auto changed = [this]() {
        stack_->setCurrentIndex(size_mode() == QLatin1String("equal") ? 1 : 0);
        treemap_->set_color_clamp(clamp_for(horizon()));
        grid_->set_color_clamp(clamp_for(horizon()));
        emit mode_changed(size_mode(), horizon());
    };
    connect(size_box_, &QComboBox::currentIndexChanged, this, changed);
    connect(horizon_box_, &QComboBox::currentIndexChanged, this, changed);

    retranslate();
    refresh_theme();
}

void WatchlistMapView::retranslate() {
    size_label_->setText(tr("SIZE"));
    horizon_label_->setText(tr("COLOUR"));
    size_box_->setItemText(0, tr("Market cap"));
    size_box_->setItemText(1, tr("Equal"));
    const QStringList horizons = {tr("1 day"), tr("1 week"), tr("1 month"), tr("3 months"), tr("Year to date"),
                                  tr("1 year")};
    for (int i = 0; i < horizons.size() && i < horizon_box_->count(); ++i)
        horizon_box_->setItemText(i, horizons[i]);
}

void WatchlistMapView::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

void WatchlistMapView::refresh_theme() {
    wla_set_css(bar_, wla_bar_style());
    wla_set_css(size_label_, wla_label_style());
    wla_set_css(horizon_label_, wla_label_style());
    wla_set_css(note_, QStringLiteral("background:transparent; color:%1; font-size:%2px;")
                           .arg(colors::TEXT_TERTIARY())
                           .arg(fonts::px(-3)));
    wla_set_css(size_box_, wla_combo_style());
    wla_set_css(horizon_box_, wla_combo_style());
}

void WatchlistMapView::set_mode(const QString& size, const QString& horizon) {
    QSignalBlocker b1(size_box_);
    QSignalBlocker b2(horizon_box_);
    const int si = size_box_->findData(size);
    size_box_->setCurrentIndex(si >= 0 ? si : 0);
    const int hi = horizon_box_->findData(horizon);
    horizon_box_->setCurrentIndex(hi >= 0 ? hi : 0);
    stack_->setCurrentIndex(size_mode() == QLatin1String("equal") ? 1 : 0);
    treemap_->set_color_clamp(clamp_for(this->horizon()));
    grid_->set_color_clamp(clamp_for(this->horizon()));
}

QString WatchlistMapView::size_mode() const {
    return size_box_->currentData().toString();
}

QString WatchlistMapView::horizon() const {
    return horizon_box_->currentData().toString();
}

void WatchlistMapView::set_items(const QVector<ui::TreemapItem>& items, bool grouped, const QString& note) {
    note_->setText(note);
    treemap_->set_grouped(grouped);
    treemap_->set_items(items);
    grid_->set_items(items);
    const ui::PaintedChart::State st = items.isEmpty() ? ui::PaintedChart::Empty : ui::PaintedChart::Ready;
    treemap_->set_state(st, items.isEmpty() ? tr("No symbols to show") : QString());
    grid_->set_state(st, items.isEmpty() ? tr("No symbols to show") : QString());
}

void WatchlistMapView::set_loading(const QString& message) {
    treemap_->set_state(ui::PaintedChart::Loading, message);
    grid_->set_state(ui::PaintedChart::Loading, message);
}

// ── WatchlistCorrelationView ────────────────────────────────────────────────

WatchlistCorrelationView::WatchlistCorrelationView(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    bar_ = new QWidget(this);
    bar_->setStyleSheet(wla_bar_style()); // before its children exist
    auto* bl = new QHBoxLayout(bar_);
    bl->setContentsMargins(12, 4, 12, 4);
    bl->setSpacing(6);
    window_label_ = new QLabel(bar_);
    window_box_ = new QComboBox(bar_);
    for (int days : {31, 92, 183, 365})
        window_box_->addItem(QString(), days);
    window_box_->setCurrentIndex(1);
    cluster_box_ = new QCheckBox(bar_);
    cluster_box_->setChecked(true);
    status_ = new QLabel(bar_);
    status_->setWordWrap(false);
    bl->addWidget(window_label_);
    bl->addWidget(window_box_);
    bl->addSpacing(10);
    bl->addWidget(cluster_box_);
    bl->addSpacing(10);
    bl->addWidget(status_, 1);
    root->addWidget(bar_);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    matrix_ = new ui::MatrixHeatmap(scroll);
    matrix_->set_diverging_scale(-1.0, 1.0, 0.0);
    matrix_->set_cluster(true);
    matrix_->set_value_formatter([](double v) { return fmt::number(v, 2); });
    scroll->setWidget(matrix_);
    root->addWidget(scroll, 1);

    connect(window_box_, &QComboBox::currentIndexChanged, this, [this]() {
        recompute();
        emit window_changed();
    });
    connect(cluster_box_, &QCheckBox::toggled, this, [this](bool on) { matrix_->set_cluster(on); });
    connect(matrix_, &ui::MatrixHeatmap::cell_clicked, this, [this](int r, int c) {
        if (r >= 0 && c >= 0 && r < shown_.size() && c < shown_.size())
            emit symbol_pair_clicked(shown_[r], shown_[c]);
    });

    retranslate();
    refresh_theme();
    matrix_->set_state(ui::PaintedChart::Loading, tr("Loading daily history…"));
}

void WatchlistCorrelationView::retranslate() {
    window_label_->setText(tr("WINDOW"));
    const QStringList labels = {tr("1 month"), tr("3 months"), tr("6 months"), tr("1 year")};
    for (int i = 0; i < labels.size() && i < window_box_->count(); ++i)
        window_box_->setItemText(i, labels[i]);
    cluster_box_->setText(tr("Cluster similar symbols"));
    cluster_box_->setToolTip(tr("Order rows and columns so that symbols that move together sit together"));
}

void WatchlistCorrelationView::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

void WatchlistCorrelationView::refresh_theme() {
    wla_set_css(bar_, wla_bar_style());
    wla_set_css(window_label_, wla_label_style());
    wla_set_css(window_box_, wla_combo_style());
    wla_set_css(cluster_box_, QStringLiteral("QCheckBox { background:transparent; color:%1; font-size:%2px; }")
                                  .arg(colors::TEXT_SECONDARY())
                                  .arg(fonts::px(-2)));
    wla_set_css(status_, QStringLiteral("background:transparent; color:%1; font-size:%2px;")
                             .arg(colors::TEXT_TERTIARY())
                             .arg(fonts::px(-3)));
}

int WatchlistCorrelationView::window_days() const {
    return window_box_->currentData().toInt();
}

void WatchlistCorrelationView::set_data(const QStringList& symbols,
                                        const QHash<QString, QVector<services::HistoryPoint>>& series, int pending,
                                        const QStringList& failed) {
    symbols_ = symbols;
    series_ = series;
    pending_ = pending;
    failed_ = failed;
    recompute();
}

void WatchlistCorrelationView::recompute() {
    shown_.clear();
    for (const QString& s : symbols_) {
        if (series_.contains(s))
            shown_ << s;
    }
    QStringList notes;
    if (pending_ > 0)
        notes << tr("Loading history for %n more symbol(s)…", "", pending_);
    if (!failed_.isEmpty())
        notes << tr("No history: %1").arg(failed_.join(QStringLiteral(", ")));
    notes << tr("Daily log returns, Pearson");
    status_->setText(notes.join(QStringLiteral("  ·  ")));
    if (shown_.size() < 2) {
        matrix_->clear();
        matrix_->set_state(pending_ > 0 ? ui::PaintedChart::Loading : ui::PaintedChart::Empty,
                           pending_ > 0 ? tr("Loading daily history…")
                                        : tr("Needs at least two symbols with price history"));
        return;
    }
    const auto m = watchlist_correlation(shown_, series_, window_days());
    matrix_->set_matrix(shown_, shown_, m);
    matrix_->set_state(ui::PaintedChart::Ready);
}

} // namespace fincept::screens
