#include "screens/dashboard/widgets/DashboardCandleWidget.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFontMetrics>
#include <QFormLayout>
#include <QLineEdit>
#include <QPainter>

#include <algorithm>

namespace fincept::screens::widgets {

// ══════════════════════════════════════════════════════════════════════════════
//  CandleCanvas — QPainter candlestick chart (candles + price axis + time axis)
// ══════════════════════════════════════════════════════════════════════════════

CandleCanvas::CandleCanvas(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void CandleCanvas::set_candles(const QVector<services::HistoryPoint>& candles) {
    candles_ = candles;
    dirty_ = true;
    update();
}

void CandleCanvas::clear() {
    candles_.clear();
    dirty_ = true;
    update();
}

void CandleCanvas::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    dirty_ = true;
}

void CandleCanvas::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        dirty_ = true;
        update();
    }
    QWidget::changeEvent(event);
}

void CandleCanvas::paintEvent(QPaintEvent*) {
    if (dirty_) {
        rebuild_cache();
        dirty_ = false;
    }
    QPainter p(this);
    p.drawPixmap(0, 0, cache_);
}

void CandleCanvas::rebuild_cache() {
    const int W = width();
    const int H = height();
    if (W <= 0 || H <= 0)
        return;

    cache_ = QPixmap(W, H);
    cache_.fill(QColor(ui::colors::BG_BASE()));

    QPainter p(&cache_);
    p.setRenderHint(QPainter::Antialiasing, false);

    const int total = candles_.size();
    if (total == 0) {
        p.setPen(QColor(ui::colors::TEXT_SECONDARY()));
        p.setFont(QFont("Consolas", 10));
        p.drawText(cache_.rect(), Qt::AlignCenter, tr("Waiting for data..."));
        return;
    }

    const int start = qMax(0, total - MAX_VISIBLE);
    const int count = total - start;

    // Price range over the visible window.
    double lo = 1e18, hi = 0.0;
    for (int i = start; i < total; ++i) {
        lo = std::min(lo, candles_[i].low);
        hi = std::max(hi, candles_[i].high);
    }
    if (lo >= hi)
        return;

    const double margin = (hi - lo) * 0.06;
    lo -= margin;
    hi += margin;

    const int plot_w = W - PRICE_AXIS_W;
    const int plot_h = H - TIME_AXIS_H;
    if (plot_w <= 0 || plot_h <= 0)
        return;

    auto py = [&](double price) -> int {
        return static_cast<int>(plot_h - (price - lo) / (hi - lo) * plot_h);
    };

    // Grid lines.
    p.setPen(QPen(QColor(ui::colors::BORDER_DIM()), 1, Qt::DotLine));
    for (int g = 1; g < 6; ++g) {
        int gy = plot_h * g / 6;
        p.drawLine(0, gy, plot_w, gy);
    }

    // Candles.
    const double slot_w = static_cast<double>(plot_w) / count;
    const int body_w = qMax(1, static_cast<int>(slot_w * 0.65));
    const int half = body_w / 2;

    const QColor bull_color(ui::colors::POSITIVE.get());
    const QColor bear_color(ui::colors::NEGATIVE.get());
    const QColor wick_bull("#2a9d5c");
    const QColor wick_bear("#b83a3a");

    for (int i = 0; i < count; ++i) {
        const auto& c = candles_[start + i];
        const int cx = static_cast<int>((i + 0.5) * slot_w);
        const bool bull = c.close >= c.open;
        const QColor& col = bull ? bull_color : bear_color;
        const QColor& wcol = bull ? wick_bull : wick_bear;

        const int open_y = py(c.open);
        const int close_y = py(c.close);
        const int high_y = py(c.high);
        const int low_y = py(c.low);

        const int body_top = std::min(open_y, close_y);
        const int body_bot = std::max(open_y, close_y);
        const int body_h = qMax(1, body_bot - body_top);

        p.setPen(QPen(wcol, 1));
        p.drawLine(cx, high_y, cx, body_top);
        p.drawLine(cx, body_bot, cx, low_y);

        p.fillRect(cx - half, body_top, body_w, body_h, col);
    }

    // Price axis (right).
    p.setPen(QPen(QColor(ui::colors::BORDER_DIM.get()), 1));
    p.drawLine(plot_w, 0, plot_w, plot_h);

    QFont lbl_font("Consolas", 8);
    p.setFont(lbl_font);
    QFontMetrics fm(lbl_font);
    p.setPen(QColor(ui::colors::TEXT_SECONDARY.get()));

    const bool is_large = hi > 1000;
    for (int g = 0; g <= 6; ++g) {
        double price = lo + (hi - lo) * g / 6.0;
        int gy = py(price);
        QString txt = is_large ? QString::number(price, 'f', 0) : QString::number(price, 'f', 2);
        p.drawText(plot_w + 4, gy + fm.ascent() / 2, txt);
    }

    // Time axis (bottom).
    p.setPen(QPen(QColor(ui::colors::BORDER_DIM.get()), 1));
    p.drawLine(0, plot_h, plot_w, plot_h);

    p.setPen(QColor(ui::colors::TEXT_SECONDARY.get()));
    const qint64 span_sec = candles_.last().timestamp - candles_.first().timestamp;
    const int label_step = qMax(1, count / 6);

    for (int i = 0; i < count; i += label_step) {
        const auto& c = candles_[start + i];
        QDateTime dt = QDateTime::fromSecsSinceEpoch(c.timestamp);
        QString label = span_sec > 365LL * 86400 ? dt.toString("MMM yy") : dt.toString("dd MMM");

        int lx = static_cast<int>((i + 0.5) * slot_w);
        int tw = fm.horizontalAdvance(label);
        if (lx - tw / 2 > 0 && lx + tw / 2 < plot_w)
            p.drawText(lx - tw / 2, plot_h + TIME_AXIS_H - 4, label);
    }

    // Last close reference line.
    double last_close = candles_.last().close;
    int ly = py(last_close);
    p.setPen(QPen(QColor(ui::colors::AMBER.get()), 1, Qt::DashLine));
    p.drawLine(0, ly, plot_w, ly);
}

// ══════════════════════════════════════════════════════════════════════════════
//  DashboardCandleWidget
// ══════════════════════════════════════════════════════════════════════════════

namespace {
QString normalize_symbol(const QString& s) {
    const QString t = s.trimmed().toUpper();
    return t.isEmpty() ? QStringLiteral("AAPL") : t;
}
} // namespace

DashboardCandleWidget::DashboardCandleWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget(tr("CANDLE \xc2\xb7 %1").arg(normalize_symbol(cfg.value("symbol").toString())), parent),
      symbol_(normalize_symbol(cfg.value("symbol").toString())) {
    auto* vl = content_layout();
    vl->setContentsMargins(6, 4, 6, 4);
    vl->setSpacing(0);

    canvas_ = new CandleCanvas(this);
    vl->addWidget(canvas_, 1);

    set_configurable(true);
    connect(this, &BaseWidget::refresh_requested, this, &DashboardCandleWidget::refresh_data);

    set_loading(true);
}

QString DashboardCandleWidget::history_topic() const {
    return QStringLiteral("market:history:") + symbol_ + QStringLiteral(":1y:1d");
}

QJsonObject DashboardCandleWidget::config() const {
    QJsonObject o;
    o.insert("symbol", symbol_);
    return o;
}

void DashboardCandleWidget::apply_config(const QJsonObject& cfg) {
    const QString next = normalize_symbol(cfg.value("symbol").toString());
    if (next == symbol_)
        return;
    symbol_ = next;
    set_title(tr("CANDLE \xc2\xb7 %1").arg(symbol_));
    canvas_->clear();
    if (isVisible()) {
        set_loading(true);
        hub_resubscribe();
    }
}

QDialog* DashboardCandleWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle(tr("Configure \xe2\x80\x94 Candle Chart"));
    auto* form = new QFormLayout(dlg);

    auto* edit = new QLineEdit(dlg);
    edit->setText(symbol_);
    edit->setPlaceholderText(tr("e.g. AAPL"));
    form->addRow(tr("Symbol"), edit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    form->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, dlg, [this, dlg, edit]() {
        QJsonObject cfg;
        cfg.insert("symbol", normalize_symbol(edit->text()));
        apply_config(cfg);
        emit config_changed(cfg);
        dlg->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    return dlg;
}

void DashboardCandleWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        hub_resubscribe();
}

void DashboardCandleWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_) {
        datahub::DataHub::instance().unsubscribe(this);
        hub_active_ = false;
    }
}

void DashboardCandleWidget::hub_resubscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    hub.subscribe<QVector<services::HistoryPoint>>(
        this, history_topic(), [this](const QVector<services::HistoryPoint>& pts) {
            set_loading(false);
            canvas_->set_candles(pts);
        });
    hub_active_ = true;
}

void DashboardCandleWidget::refresh_data() {
    datahub::DataHub::instance().request(history_topic(), /*force=*/true);
}

void DashboardCandleWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("CANDLE \xc2\xb7 %1").arg(symbol_));
}

} // namespace fincept::screens::widgets
