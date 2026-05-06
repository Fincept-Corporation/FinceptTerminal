#include "screens/dashboard/widgets/MarketSentimentWidget.h"

#include "ui/theme/Theme.h"

#    include "datahub/DataHub.h"
#    include "datahub/DataHubMetaTypes.h"

namespace fincept::screens::widgets {

namespace {
inline const QStringList kSentimentSymbols = {
    "^VIX", "AAPL", "MSFT", "GOOGL", "AMZN", "NVDA", "META", "TSLA", "NFLX",
    "AMD",  "INTC", "JPM",  "GS",    "BAC",  "WMT",  "DIS",  "BA",   "XOM",
    "CVX",  "COIN", "PLTR", "SOFI",  "NKE",  "PFE",  "PYPL"};
}

MarketSentimentWidget::MarketSentimentWidget(QWidget* parent) : BaseWidget("MARKET SENTIMENT", parent) {
    auto* vl = content_layout();
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(8);

    // ── Score banner ──
    banner_ = new QWidget(this);
    auto* bl = new QVBoxLayout(banner_);
    bl->setContentsMargins(12, 10, 12, 10);
    bl->setSpacing(4);
    bl->setAlignment(Qt::AlignCenter);

    auto* score_row = new QWidget(this);
    auto* srl = new QHBoxLayout(score_row);
    srl->setContentsMargins(0, 0, 0, 0);
    srl->setAlignment(Qt::AlignCenter);
    srl->setSpacing(8);

    arrow_label_ = new QLabel(QChar(0x2014)); // em-dash placeholder
    srl->addWidget(arrow_label_);

    score_label_ = new QLabel("--");
    srl->addWidget(score_label_);

    bl->addWidget(score_row);

    verdict_label_ = new QLabel("LOADING...");
    verdict_label_->setAlignment(Qt::AlignCenter);
    bl->addWidget(verdict_label_);

    vl->addWidget(banner_);

    // ── Sentiment bar ──
    auto* bar_container = new QWidget(this);
    bar_container->setFixedHeight(8);
    auto* bar_layout = new QHBoxLayout(bar_container);
    bar_layout->setContentsMargins(0, 0, 0, 0);
    bar_layout->setSpacing(1);

    bull_bar_ = new QFrame;
    bar_layout->addWidget(bull_bar_, 1);

    neutral_bar_ = new QFrame;
    bar_layout->addWidget(neutral_bar_, 1);

    bear_bar_ = new QFrame;
    bar_layout->addWidget(bear_bar_, 1);

    vl->addWidget(bar_container);

    // Labels under bar
    auto* bar_labels = new QWidget(this);
    auto* bll = new QHBoxLayout(bar_labels);
    bll->setContentsMargins(0, 0, 0, 0);

    bull_label_ = new QLabel("-- BULL");
    bll->addWidget(bull_label_);
    bll->addStretch();

    neutral_label_ = new QLabel("-- NEUTRAL");
    bll->addWidget(neutral_label_);
    bll->addStretch();

    bear_label_ = new QLabel("-- BEAR");
    bll->addWidget(bear_label_);

    vl->addWidget(bar_labels);

    // ── VIX and Breadth ──
    separator_ = new QFrame;
    separator_->setFixedHeight(1);
    vl->addWidget(separator_);

    auto* stats = new QWidget(this);
    auto* stl = new QVBoxLayout(stats);
    stl->setContentsMargins(0, 4, 0, 0);
    stl->setSpacing(4);

    auto make_stat_row = [&](const QString& label, QLabel*& title_out, QLabel*& val_out) {
        auto* row = new QWidget(this);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);

        title_out = new QLabel(label);
        rl->addWidget(title_out);
        rl->addStretch();

        val_out = new QLabel("--");
        rl->addWidget(val_out);

        stl->addWidget(row);
    };

    make_stat_row("VIX (Fear Gauge)", vix_title_label_, vix_label_);
    make_stat_row("Advance/Decline", breadth_title_label_, breadth_label_);

    vl->addWidget(stats);
    vl->addStretch();

    connect(this, &BaseWidget::refresh_requested, this, &MarketSentimentWidget::refresh_data);

    apply_styles();
    set_loading(true);

}

void MarketSentimentWidget::apply_styles() {
    banner_->setStyleSheet(QString("background: %1; border-radius: 2px;").arg(ui::colors::BG_RAISED()));
    arrow_label_->setStyleSheet(
        QString("font-size: 18px; background: transparent; color: %1;").arg(ui::colors::TEXT_TERTIARY()));
    score_label_->setStyleSheet(QString("color: %1; font-size: 24px; font-weight: bold; background: transparent;")
                                    .arg(ui::colors::TEXT_TERTIARY()));
    verdict_label_->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;")
                                      .arg(ui::colors::TEXT_TERTIARY()));

    bull_bar_->setStyleSheet(QString("background: %1; border-radius: 0;").arg(ui::colors::POSITIVE()));
    neutral_bar_->setStyleSheet(QString("background: %1; border-radius: 0;").arg(ui::colors::TEXT_TERTIARY()));
    bear_bar_->setStyleSheet(QString("background: %1; border-radius: 0;").arg(ui::colors::NEGATIVE()));

    bull_label_->setStyleSheet(
        QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::POSITIVE()));
    neutral_label_->setStyleSheet(
        QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::TEXT_TERTIARY()));
    bear_label_->setStyleSheet(
        QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::NEGATIVE()));

    separator_->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER_DIM()));

    vix_title_label_->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;")
                                        .arg(ui::colors::TEXT_TERTIARY()));
    breadth_title_label_->setStyleSheet(
        QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;")
            .arg(ui::colors::TEXT_TERTIARY()));
    vix_label_->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;")
                                  .arg(ui::colors::TEXT_PRIMARY()));
    breadth_label_->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;")
                                      .arg(ui::colors::TEXT_PRIMARY()));
}

void MarketSentimentWidget::on_theme_changed() {
    apply_styles();
}

void MarketSentimentWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        hub_subscribe_all();
}

void MarketSentimentWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void MarketSentimentWidget::refresh_data() {
    // User-triggered refresh — force past min_interval; producer rate limit still paces upstream.
    auto& hub = datahub::DataHub::instance();
    QStringList topics;
    topics.reserve(kSentimentSymbols.size());
    for (const auto& sym : kSentimentSymbols)
        topics.append(QStringLiteral("market:quote:") + sym);
    hub.request(topics, /*force=*/true);
}


void MarketSentimentWidget::hub_subscribe_all() {
    auto& hub = datahub::DataHub::instance();
    set_loading_progress(row_cache_.size(), kSentimentSymbols.size());
    for (const auto& sym : kSentimentSymbols) {
        const QString topic = QStringLiteral("market:quote:") + sym;
        hub.subscribe(this, topic, [this, sym](const QVariant& v) {
            if (!v.canConvert<services::QuoteData>())
                return;
            row_cache_.insert(sym, v.value<services::QuoteData>());
            set_loading_progress(row_cache_.size(), kSentimentSymbols.size());
            rebuild_from_cache();
        });
    }
    hub_active_ = true;
}

void MarketSentimentWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void MarketSentimentWidget::rebuild_from_cache() {
    QVector<services::QuoteData> quotes;
    quotes.reserve(row_cache_.size());
    for (const auto& sym : kSentimentSymbols) {
        auto it = row_cache_.constFind(sym);
        if (it != row_cache_.constEnd())
            quotes.append(it.value());
    }
    if (!quotes.isEmpty())
        populate(quotes);
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
    QString score_color = score > 20    ? ui::colors::POSITIVE()
                          : score < -20 ? ui::colors::NEGATIVE()
                                        : ui::colors::WARNING();
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
