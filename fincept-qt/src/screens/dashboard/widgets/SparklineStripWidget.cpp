#include "screens/dashboard/widgets/SparklineStripWidget.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "ui/theme/Theme.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QWidget>
#include <algorithm>

namespace fincept::screens::widgets {

// ── Inline sparkline canvas ──────────────────────────────────────────────────
class SparklineCanvas : public QWidget {
  public:
    explicit SparklineCanvas(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumHeight(18);
    }
    void set_points(const QVector<double>& pts) {
        pts_ = pts;
        update();
    }

  protected:
    void paintEvent(QPaintEvent*) override {
        if (pts_.size() < 2)
            return;
        QPainter p(this);
        const QRectF r = rect();
        double lo = pts_.first(), hi = pts_.first();
        for (double v : pts_) {
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }
        const double span = std::max(1e-9, hi - lo);
        QPainterPath path;
        const double dx = r.width() / (pts_.size() - 1);
        for (int i = 0; i < pts_.size(); ++i) {
            const double x = r.left() + i * dx;
            const double y = r.bottom() - ((pts_[i] - lo) / span) * r.height();
            if (i == 0)
                path.moveTo(x, y);
            else
                path.lineTo(x, y);
        }
        QColor col = (pts_.last() >= pts_.first()) ? ui::colors::POSITIVE() : ui::colors::NEGATIVE();
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(QPen(col, 1.2));
        p.drawPath(path);
    }

  private:
    QVector<double> pts_;
};

namespace {
const QStringList kDefaultSymbols = {"AAPL", "MSFT", "GOOGL", "NVDA"};
} // namespace

SparklineStripWidget::SparklineStripWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget("SPARKLINES", parent) {
    auto* vl = content_layout();
    vl->setContentsMargins(10, 8, 10, 8);
    vl->setSpacing(4);

    set_configurable(true);
    apply_styles();
    apply_config(cfg);
}

QJsonObject SparklineStripWidget::config() const {
    QJsonObject o;
    QJsonArray arr;
    for (const auto& s : symbols_)
        arr.append(s);
    o.insert("symbols", arr);
    return o;
}

void SparklineStripWidget::apply_config(const QJsonObject& cfg) {
    QStringList next;
    const QJsonArray arr = cfg.value("symbols").toArray();
    for (const auto& v : arr) {
        const QString s = v.toString().trimmed().toUpper();
        if (!s.isEmpty())
            next.append(s);
    }
    if (next.isEmpty())
        next = kDefaultSymbols;

    symbols_ = next;
    build_rows();

    if (isVisible())
        hub_resubscribe();
}

void SparklineStripWidget::build_rows() {
    auto* vl = content_layout();
    while (QLayoutItem* it = vl->takeAt(0)) {
        if (auto* w = it->widget())
            w->deleteLater();
        else if (auto* sub = it->layout()) {
            while (QLayoutItem* ci = sub->takeAt(0)) {
                if (auto* cw = ci->widget())
                    cw->deleteLater();
                delete ci;
            }
            delete sub;
        }
        delete it;
    }
    rows_.clear();

    auto* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(3);
    grid->setColumnStretch(1, 1);

    for (int i = 0; i < symbols_.size(); ++i) {
        const QString& sym = symbols_[i];
        Row r;
        r.symbol = new QLabel(sym);
        r.spark = new SparklineCanvas();
        r.last = new QLabel("—");
        r.last->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(r.symbol, i, 0);
        grid->addWidget(r.spark, i, 1);
        grid->addWidget(r.last, i, 2);
        rows_.insert(sym, r);
    }
    vl->addLayout(grid);
    vl->addStretch(1);
    apply_styles();
}

void SparklineStripWidget::hub_resubscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    for (const auto& sym : symbols_) {
        const QString topic = QStringLiteral("market:sparkline:") + sym;
        const QString sym_copy = sym;
        hub.subscribe(this, topic, [this, sym_copy](const QVariant& v) {
            if (!v.canConvert<QVector<double>>())
                return;
            on_points(sym_copy, v.value<QVector<double>>());
        });
    }
    hub_active_ = true;
}

void SparklineStripWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void SparklineStripWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        hub_resubscribe();
}

void SparklineStripWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void SparklineStripWidget::on_points(const QString& symbol, const QVector<double>& points) {
    auto it = rows_.find(symbol);
    if (it == rows_.end() || points.isEmpty())
        return;
    it->spark->set_points(points);
    it->last->setText(QString::number(points.last(), 'f', 2));
    set_loading(false);
}

QDialog* SparklineStripWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle("Configure — Sparklines");
    auto* form = new QFormLayout(dlg);

    auto* edit = new QLineEdit(dlg);
    edit->setText(symbols_.join(", "));
    edit->setPlaceholderText("e.g. AAPL, MSFT, NVDA");
    form->addRow("Symbols", edit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    form->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, dlg, [this, dlg, edit]() {
        QJsonArray arr;
        const QStringList pieces = edit->text().split(',', Qt::SkipEmptyParts);
        for (const auto& p : pieces) {
            const QString s = p.trimmed().toUpper();
            if (!s.isEmpty())
                arr.append(s);
        }
        QJsonObject cfg;
        cfg.insert("symbols", arr);
        apply_config(cfg);
        emit config_changed(cfg);
        dlg->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    return dlg;
}

void SparklineStripWidget::on_theme_changed() {
    apply_styles();
}

void SparklineStripWidget::apply_styles() {
    const QString sym_css =
        QString("color:%1;font-size:11px;font-weight:700;background:transparent;")
            .arg(ui::colors::TEXT_PRIMARY());
    const QString last_css =
        QString("color:%1;font-size:11px;font-weight:600;background:transparent;")
            .arg(ui::colors::TEXT_PRIMARY());
    for (auto it = rows_.begin(); it != rows_.end(); ++it) {
        if (it->symbol)
            it->symbol->setStyleSheet(sym_css);
        if (it->last)
            it->last->setStyleSheet(last_css);
        if (it->spark)
            it->spark->update();
    }
}

} // namespace fincept::screens::widgets
