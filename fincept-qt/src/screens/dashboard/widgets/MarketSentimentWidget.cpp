#include "screens/dashboard/widgets/MarketSentimentWidget.h"

#include "ui/theme/Theme.h"

namespace fincept::screens::widgets {

MarketSentimentWidget::MarketSentimentWidget(QWidget* parent) : BaseWidget("MARKET SENTIMENT", parent) {
    auto* vl = content_layout();
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(8);

    // ── Score banner ──
    auto* banner = new QWidget;
    banner->setStyleSheet(QString("background: %1; border-radius: 2px;").arg(ui::colors::BG_RAISED()));
    auto* bl = new QVBoxLayout(banner);
    bl->setContentsMargins(12, 10, 12, 10);
    bl->setSpacing(4);
    bl->setAlignment(Qt::AlignCenter);

    auto* score_row = new QWidget;
    auto* srl = new QHBoxLayout(score_row);
    srl->setContentsMargins(0, 0, 0, 0);
    srl->setAlignment(Qt::AlignCenter);
    srl->setSpacing(8);

    auto* arrow = new QLabel(QChar(0x2014)); // em-dash placeholder
    arrow->setStyleSheet(
        QString("font-size: 18px; background: transparent; color: %1;").arg(ui::colors::TEXT_TERTIARY()));
    srl->addWidget(arrow);

    score_label_ = new QLabel("--");
    score_label_->setStyleSheet(QString("color: %1; font-size: 24px; font-weight: bold; background: transparent;")
                                    .arg(ui::colors::TEXT_TERTIARY()));
    srl->addWidget(score_label_);

    bl->addWidget(score_row);

    verdict_label_ = new QLabel("LOADING...");
    verdict_label_->setAlignment(Qt::AlignCenter);
    verdict_label_->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;")
                                      .arg(ui::colors::TEXT_TERTIARY()));
    bl->addWidget(verdict_label_);

    vl->addWidget(banner);

    // ── Sentiment bar ──
    auto* bar_container = new QWidget;
    bar_container->setFixedHeight(8);
    auto* bar_layout = new QHBoxLayout(bar_container);
    bar_layout->setContentsMargins(0, 0, 0, 0);
    bar_layout->setSpacing(1);

    bull_bar_ = new QFrame;
    bull_bar_->setStyleSheet(QString("background: %1; border-radius: 0;").arg(ui::colors::POSITIVE()));
    bar_layout->addWidget(bull_bar_, 1);

    neutral_bar_ = new QFrame;
    neutral_bar_->setStyleSheet(QString("background: %1; border-radius: 0;").arg(ui::colors::TEXT_TERTIARY()));
    bar_layout->addWidget(neutral_bar_, 1);

    bear_bar_ = new QFrame;
    bear_bar_->setStyleSheet(QString("background: %1; border-radius: 0;").arg(ui::colors::NEGATIVE()));
    bar_layout->addWidget(bear_bar_, 1);

    vl->addWidget(bar_container);

    // Labels under bar
    auto* bar_labels = new QWidget;
    auto* bll = new QHBoxLayout(bar_labels);
    bll->setContentsMargins(0, 0, 0, 0);

    bull_label_ = new QLabel("-- BULL");
    bull_label_->setStyleSheet(
        QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::POSITIVE()));
    bll->addWidget(bull_label_);
    bll->addStretch();

    neutral_label_ = new QLabel("-- NEUTRAL");
    neutral_label_->setStyleSheet(
        QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::TEXT_TERTIARY()));
    bll->addWidget(neutral_label_);
    bll->addStretch();

    bear_label_ = new QLabel("-- BEAR");
    bear_label_->setStyleSheet(
        QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::NEGATIVE()));
    bll->addWidget(bear_label_);

    vl->addWidget(bar_labels);

    // ── VIX and Breadth ──
    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER_DIM()));
    vl->addWidget(sep);

    auto* stats = new QWidget;
    auto* stl = new QVBoxLayout(stats);
    stl->setContentsMargins(0, 4, 0, 0);
    stl->setSpacing(4);

    auto make_stat_row = [&](const QString& label, QLabel*& val_out) {
        auto* row = new QWidget;
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);

        auto* lbl = new QLabel(label);
        lbl->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;")
                               .arg(ui::colors::TEXT_TERTIARY()));
        rl->addWidget(lbl);
        rl->addStretch();

        val_out = new QLabel("--");
        val_out->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;")
                                   .arg(ui::colors::TEXT_PRIMARY()));
        rl->addWidget(val_out);

        stl->addWidget(row);
    };

    make_stat_row("VIX (Fear Gauge)", vix_label_);
    make_stat_row("Advance/Decline", breadth_label_);

    vl->addWidget(stats);
    vl->addStretch();

    connect(this, &BaseWidget::refresh_requested, this, &MarketSentimentWidget::refresh_data);

    set_loading(true);
    refresh_data();
}

void MarketSentimentWidget::refresh_data() {
    set_loading(true);

    // Fetch a broad basket: VIX + 24 liquid stocks for breadth
    QStringList syms = {"^VIX", "AAPL", "MSFT", "GOOGL", "AMZN", "NVDA", "META", "TSLA", "NFLX",
                        "AMD",  "INTC", "JPM",  "GS",    "BAC",  "WMT",  "DIS",  "BA",   "XOM",
                        "CVX",  "COIN", "PLTR", "SOFI",  "NKE",  "PFE",  "PYPL"};

    services::MarketDataService::instance().fetch_quotes(syms, [this](bool ok, QVector<services::QuoteData> quotes) {
        set_loading(false);
        if (!ok || quotes.isEmpty())
            return;
        populate(quotes);
    });
}

void MarketSentimentWidget::populate(const QVector<services::QuoteData>& quotes) {
    int bullish = 0, bearish = 0, neutral = 0;
    double vix_price = -1;

    for (const auto& q : quotes) {
        if (q.symbol == "^VIX") {
            vix_price = q.price;
            continue;
        }
        if (q.change_pct > 0.5)
            ++bullish;
        else if (q.change_pct < -0.5)
            ++bearish;
        else
            ++neutral;
    }

    int total = bullish + bearish + neutral;
    if (total == 0)
        total = 1;

    // Score: (bullish - bearish) / total * 100
    int score = static_cast<int>(((bullish - bearish) / static_cast<double>(total)) * 100);

    // Adjust score based on VIX if available
    if (vix_price > 0) {
        if (vix_price > 30)
            score -= 20;
        else if (vix_price > 25)
            score -= 10;
        else if (vix_price < 15)
            score += 10;
    }
    score = qBound(-100, score, 100);

    // Update UI
    QString score_color = score > 20 ? ui::colors::POSITIVE() : score < -20 ? ui::colors::NEGATIVE() : ui::colors::WARNING();
    QString verdict = score > 40    ? "STRONGLY BULLISH"
                      : score > 20  ? "BULLISH"
                      : score > -20 ? "NEUTRAL"
                      : score > -40 ? "BEARISH"
                                    : "STRONGLY BEARISH";

    score_label_->setText(QString("%1%2").arg(score > 0 ? "+" : "").arg(score));
    score_label_->setStyleSheet(
        QString("color: %1; font-size: 24px; font-weight: bold; background: transparent;").arg(score_color));

    verdict_label_->setText(verdict);
    verdict_label_->setStyleSheet(
        QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;").arg(score_color));

    // Update bar proportions
    auto* bar_layout = qobject_cast<QHBoxLayout*>(bull_bar_->parentWidget()->layout());
    if (bar_layout) {
        bar_layout->setStretch(0, bullish);
        bar_layout->setStretch(1, neutral);
        bar_layout->setStretch(2, bearish);
    }

    bull_label_->setText(QString("%1 %2 BULL").arg(QChar(0x25B2)).arg(bullish));
    neutral_label_->setText(QString("%1 NEUTRAL").arg(neutral));
    bear_label_->setText(QString("%1 %2 BEAR").arg(QChar(0x25BC)).arg(bearish));

    // VIX
    if (vix_price > 0) {
        vix_label_->setText(QString::number(vix_price, 'f', 2));
        QString vix_color = vix_price > 25   ? ui::colors::NEGATIVE()
                            : vix_price > 18 ? ui::colors::WARNING()
                                             : ui::colors::POSITIVE();
        vix_label_->setStyleSheet(
            QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;").arg(vix_color));
    }

    // Breadth
    breadth_label_->setText(QString("%1A / %2D").arg(bullish).arg(bearish));
    breadth_label_->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;")
                                      .arg(bullish > bearish ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
}

} // namespace fincept::screens::widgets
