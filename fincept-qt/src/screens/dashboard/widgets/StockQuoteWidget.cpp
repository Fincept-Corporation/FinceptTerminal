#include "screens/dashboard/widgets/StockQuoteWidget.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "ui/theme/Theme.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QLineEdit>

namespace fincept::screens::widgets {

namespace {
QString normalize_symbol(const QString& s) {
    const QString t = s.trimmed().toUpper();
    return t.isEmpty() ? QStringLiteral("AAPL") : t;
}
} // namespace

StockQuoteWidget::StockQuoteWidget(const QString& symbol, QWidget* parent)
    : BaseWidget(tr("QUOTE: %1").arg(symbol.toUpper()), parent), symbol_(symbol.toUpper()) {
    auto* vl = content_layout();
    vl->setContentsMargins(12, 8, 12, 8);
    vl->setSpacing(8);

    // ── Price row ──
    auto* price_row = new QWidget(this);
    auto* prl = new QHBoxLayout(price_row);
    prl->setContentsMargins(0, 0, 0, 0);
    prl->setSpacing(8);

    price_label_ = new QLabel("--");
    prl->addWidget(price_label_);

    auto* change_col = new QWidget(this);
    auto* ccl = new QVBoxLayout(change_col);
    ccl->setContentsMargins(0, 4, 0, 0);
    ccl->setSpacing(0);

    arrow_label_ = new QLabel;
    ccl->addWidget(arrow_label_);

    change_label_ = new QLabel("--");
    ccl->addWidget(change_label_);

    prl->addWidget(change_col);
    prl->addStretch();

    ticker_label_ = new QLabel(symbol_);
    prl->addWidget(ticker_label_);

    vl->addWidget(price_row);

    // ── Separator ──
    sep_ = new QFrame;
    sep_->setFixedHeight(1);
    vl->addWidget(sep_);

    // ── Stats grid ──
    auto* stats = new QWidget(this);
    auto* gl = new QGridLayout(stats);
    gl->setContentsMargins(0, 0, 0, 0);
    gl->setSpacing(6);

    auto make_stat = [&](int row, int col, const char* key, QLabel*& val_out) {
        auto* cell = new QWidget(this);
        stat_cells_.append(cell);
        auto* cl = new QVBoxLayout(cell);
        cl->setContentsMargins(8, 6, 8, 6);
        cl->setSpacing(2);

        auto* lbl = new QLabel(tr(key));
        stat_labels_.append(lbl);
        stat_label_keys_.append(QString::fromLatin1(key));
        cl->addWidget(lbl);

        val_out = new QLabel("--");
        stat_values_.append(val_out);
        cl->addWidget(val_out);

        gl->addWidget(cell, row, col);
    };

    make_stat(0, 0, QT_TR_NOOP("OPEN"), open_val_);
    make_stat(0, 1, QT_TR_NOOP("PREV CLOSE"), prev_val_);
    make_stat(1, 0, QT_TR_NOOP("HIGH"), high_val_);
    make_stat(1, 1, QT_TR_NOOP("LOW"), low_val_);
    make_stat(2, 0, QT_TR_NOOP("VOLUME"), volume_val_);

    vl->addWidget(stats);
    vl->addStretch();

    set_configurable(true);
    connect(this, &BaseWidget::refresh_requested, this, &StockQuoteWidget::refresh_data);

    apply_styles();
    set_loading(true);
}

QJsonObject StockQuoteWidget::config() const {
    QJsonObject o;
    o.insert("symbol", symbol_);
    return o;
}

void StockQuoteWidget::apply_config(const QJsonObject& cfg) {
    const QString next = normalize_symbol(cfg.value("symbol").toString(symbol_));
    if (next == symbol_)
        return;
    set_symbol(next);
}

QDialog* StockQuoteWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle(tr("Configure — Stock Quote"));
    auto* form = new QFormLayout(dlg);

    auto* edit = new QLineEdit(dlg);
    edit->setText(symbol_);
    edit->setPlaceholderText(tr("e.g. AAPL"));
    edit->setAccessibleName(tr("Symbol"));
    form->addRow(tr("Symbol"), edit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    form->addRow(buttons);
    dlg->setTabOrder(edit, buttons);

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

void StockQuoteWidget::apply_styles() {
    price_label_->setStyleSheet(QString("color: %1; font-size: 28px; font-weight: bold; background: transparent;")
                                    .arg(ui::colors::TEXT_PRIMARY()));
    arrow_label_->setStyleSheet("font-size: 12px; background: transparent;");
    change_label_->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; background: transparent;")
                                     .arg(ui::colors::TEXT_SECONDARY()));
    ticker_label_->setStyleSheet(
        QString("color: %1; font-size: 14px; font-weight: bold; background: transparent;").arg(ui::colors::AMBER()));
    sep_->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER_DIM()));

    for (auto* cell : stat_cells_)
        cell->setStyleSheet(QString("background: %1; border-radius: 2px;").arg(ui::colors::BG_RAISED()));
    for (auto* lbl : stat_labels_)
        lbl->setStyleSheet(
            QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::TEXT_TERTIARY()));
    for (auto* val : stat_values_)
        val->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; background: transparent;")
                               .arg(ui::colors::TEXT_PRIMARY()));
}

void StockQuoteWidget::on_theme_changed() {
    apply_styles();
}

void StockQuoteWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        hub_resubscribe();
}

void StockQuoteWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void StockQuoteWidget::set_symbol(const QString& symbol) {
    symbol_ = normalize_symbol(symbol);
    set_title(tr("QUOTE: %1").arg(symbol_));
    ticker_label_->setText(symbol_);

    // Blank the previous symbol's numbers immediately — leaving AAPL's price
    // under an "MSFT" heading until the first delivery is actively misleading.
    price_label_->setText(QStringLiteral("--"));
    change_label_->setText(QStringLiteral("--"));
    arrow_label_->setText(QString());
    for (auto* v : stat_values_)
        v->setText(QStringLiteral("--"));

    // Re-subscribe to the new topic; old sub for previous symbol is
    // dropped wholesale by `unsubscribe(this)` inside hub_resubscribe().
    if (isVisible()) {
        set_loading(true);
        hub_resubscribe();
    }
}

void StockQuoteWidget::refresh_data() {
    datahub::DataHub::instance().request(QStringLiteral("market:quote:") + symbol_);
}

void StockQuoteWidget::hub_resubscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    const QString topic = QStringLiteral("market:quote:") + symbol_;
    hub.subscribe(this, topic, [this](const QVariant& v) {
        if (!v.canConvert<services::QuoteData>())
            return;
        set_loading(false);
        populate(v.value<services::QuoteData>());
    });
    hub_active_ = true;
}

void StockQuoteWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void StockQuoteWidget::populate(const services::QuoteData& q) {
    price_label_->setText(QString("$%1").arg(q.price, 0, 'f', 2));

    bool positive = q.change_pct >= 0;
    QString color = positive ? ui::colors::POSITIVE() : ui::colors::NEGATIVE();

    arrow_label_->setText(positive ? QString(QChar(0x25B2)) : QString(QChar(0x25BC)));
    arrow_label_->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(color));

    change_label_->setText(QString("%1%2 (%3%4%)")
                               .arg(positive ? "+" : "")
                               .arg(q.change, 0, 'f', 2)
                               .arg(positive ? "+" : "")
                               .arg(q.change_pct, 0, 'f', 2));
    change_label_->setStyleSheet(
        QString("color: %1; font-size: 11px; font-weight: bold; background: transparent;").arg(color));

    price_label_->setStyleSheet(
        QString("color: %1; font-size: 28px; font-weight: bold; background: transparent;").arg(color));

    auto fmt = [](double v) { return v > 0 ? QString("$%1").arg(v, 0, 'f', 2) : QString("--"); };
    // The batch quote snapshot carries last/change/high/low/volume but no
    // session open. Showing `high` here (as this did previously) prints a
    // wrong number under an "OPEN" heading — on a trading terminal that is
    // worse than showing nothing.
    open_val_->setText(QStringLiteral("--"));
    open_val_->setToolTip(tr("Session open is not available in the batch quote feed"));
    high_val_->setText(fmt(q.high));
    low_val_->setText(fmt(q.low));
    prev_val_->setText(fmt(q.price - q.change));

    // Format volume
    if (q.volume >= 1e9)
        volume_val_->setText(QString("%1B").arg(q.volume / 1e9, 0, 'f', 1));
    else if (q.volume >= 1e6)
        volume_val_->setText(QString("%1M").arg(q.volume / 1e6, 0, 'f', 1));
    else if (q.volume >= 1e3)
        volume_val_->setText(QString("%1K").arg(q.volume / 1e3, 0, 'f', 1));
    else
        volume_val_->setText(QString::number(static_cast<int>(q.volume)));
}

void StockQuoteWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("QUOTE: %1").arg(symbol_));
    // Re-run tr() on the kept stat-cell headings. (This previously called
    // refresh_data(), which fires a network request and never touched a
    // single label.)
    for (int i = 0; i < stat_labels_.size() && i < stat_label_keys_.size(); ++i)
        stat_labels_[i]->setText(tr(stat_label_keys_[i].toUtf8().constData()));
}

} // namespace fincept::screens::widgets
