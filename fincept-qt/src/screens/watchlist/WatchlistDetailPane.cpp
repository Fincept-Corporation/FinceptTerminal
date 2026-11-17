#include "screens/watchlist/WatchlistDetailPane.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/watchlist/WatchlistColumns.h"
#include "services/equity/EquityResearchService.h"
#include "services/markets/MarketDataService.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/StatTile.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens {

namespace fmt = fincept::ui::formatting;
namespace colors = fincept::ui::colors;
namespace fonts = fincept::ui::fonts;
namespace wcols = fincept::screens::watchlist_columns;
using services::equity::EquityResearchService;

namespace {

constexpr int kWldNewsCount = 12;
constexpr int kWldUrlRole = Qt::UserRole + 1;

struct WldTileDef {
    const char* key;
    const char* label;
};

// Tile key = registry column id the value is read from.
const WldTileDef kWldTiles[] = {
    {"open", QT_TRANSLATE_NOOP("WatchlistDetailPane", "OPEN")},
    {"prev_close", QT_TRANSLATE_NOOP("WatchlistDetailPane", "PREV CLOSE")},
    {"day_range", QT_TRANSLATE_NOOP("WatchlistDetailPane", "DAY RANGE")},
    {"range_52w", QT_TRANSLATE_NOOP("WatchlistDetailPane", "52W RANGE")},
    {"volume", QT_TRANSLATE_NOOP("WatchlistDetailPane", "VOLUME")},
    {"market_cap", QT_TRANSLATE_NOOP("WatchlistDetailPane", "MKT CAP")},
    {"pe", QT_TRANSLATE_NOOP("WatchlistDetailPane", "P/E")},
    {"div_yield", QT_TRANSLATE_NOOP("WatchlistDetailPane", "YIELD")},
    {"beta", QT_TRANSLATE_NOOP("WatchlistDetailPane", "BETA")},
    {"vol_20d", QT_TRANSLATE_NOOP("WatchlistDetailPane", "20D VOL")},
    {"perf_1y", QT_TRANSLATE_NOOP("WatchlistDetailPane", "1Y")},
    {"earnings", QT_TRANSLATE_NOOP("WatchlistDetailPane", "EARNINGS")},
};

bool wld_needs_info(const QString& key) {
    return key == QLatin1String("market_cap") || key == QLatin1String("pe") || key == QLatin1String("div_yield") ||
           key == QLatin1String("beta") || key == QLatin1String("earnings");
}

bool wld_needs_history(const QString& key) {
    return key == QLatin1String("vol_20d") || key == QLatin1String("perf_1y");
}

/// setStyleSheet() re-polishes the widget and its whole subtree even for unchanged text;
/// the pane and its header get theirs at creation, so refresh_theme() (run at the end of
/// construction, and again by WatchlistScreen) must not re-apply the same text.
void wld_set_css(QWidget* w, const QString& css) {
    if (w && w->styleSheet() != css)
        w->setStyleSheet(css);
}

QString wld_root_css() {
    return QStringLiteral("background:%1;").arg(colors::BG_BASE());
}

QString wld_header_css() {
    return QStringLiteral("background:%1; border-bottom:1px solid %2;").arg(colors::BG_RAISED(), colors::BORDER_DIM());
}

// One-line label that elides its text ("…") to the width it is given instead of clipping mid-word.
class WldElideLabel final : public QLabel {
  public:
    using QLabel::QLabel;
    void set_full_text(const QString& t) {
        full_ = t;
        refit();
    }
    QSize minimumSizeHint() const override { return {0, QLabel::minimumSizeHint().height()}; }

  protected:
    void resizeEvent(QResizeEvent* e) override {
        QLabel::resizeEvent(e);
        refit();
    }
    void changeEvent(QEvent* e) override {
        QLabel::changeEvent(e);
        if (e->type() == QEvent::FontChange || e->type() == QEvent::StyleChange)
            refit();
    }

  private:
    void refit() {
        const QString shown = fontMetrics().elidedText(full_, Qt::ElideRight, qMax(0, contentsRect().width()));
        if (shown != text())
            setText(shown);
    }
    QString full_;
};

} // namespace

WatchlistDetailPane::WatchlistDetailPane(QWidget* parent) : QWidget(parent) {
    load_timer_ = new QTimer(this);
    load_timer_->setSingleShot(true);
    load_timer_->setInterval(350); // arrow-key browsing does not fire a fetch per row
    connect(load_timer_, &QTimer::timeout, this, [this]() {
        if (!isVisible() || symbol_.isEmpty())
            return;
        loaded_symbol_ = symbol_; // before the loads: a cached answer may arrive synchronously
        load_chart();
        load_news();
    });
    notes_timer_ = new QTimer(this);
    notes_timer_->setSingleShot(true);
    notes_timer_->setInterval(800);
    connect(notes_timer_, &QTimer::timeout, this, [this]() {
        if (notes_dirty_ && !symbol_.isEmpty()) {
            notes_dirty_ = false;
            emit notes_edited(symbol_, notes_->toPlainText());
        }
    });

    build_ui();

    auto& ers = EquityResearchService::instance();
    connect(&ers, &EquityResearchService::news_loaded, this,
            [this](QString sym, services::equity::NewsProvider provider, bool,
                   QVector<services::equity::NewsArticle> articles) {
                if (sym != loaded_symbol_ || provider != services::equity::NewsProvider::Auto)
                    return;
                news_->clear();
                int shown = 0;
                for (const auto& a : articles) {
                    if (a.title.trimmed().isEmpty())
                        continue;
                    const qint64 ts = services::equity::news_timestamp(a.published_date);
                    QString meta = a.publisher;
                    if (ts > 0)
                        meta += (meta.isEmpty() ? QString() : QStringLiteral(" · ")) + fmt::relative(ts);
                    auto* item = new QListWidgetItem(a.title.trimmed() + QLatin1Char('\n') + meta);
                    item->setData(kWldUrlRole, a.url);
                    item->setToolTip(a.description.isEmpty() ? a.title : a.description);
                    news_->addItem(item);
                    if (++shown >= kWldNewsCount)
                        break;
                }
                set_news_status(shown == 0 ? tr("No recent headlines for %1.").arg(sym) : QString());
            });
    connect(&ers, &EquityResearchService::load_failed, this, [this](services::equity::LoadFailure f) {
        if (f.symbol == loaded_symbol_ && f.leg == QLatin1String("News"))
            set_news_status(tr("Headlines unavailable: %1").arg(f.message));
    });

    refresh_theme();
    set_row(nullptr);
}

void WatchlistDetailPane::build_ui() {
    // Pane and header stylesheets before their children exist (see wld_set_css).
    setStyleSheet(wld_root_css());
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    header_ = new QWidget(this);
    header_->setStyleSheet(wld_header_css());
    auto* hl = new QVBoxLayout(header_);
    hl->setContentsMargins(12, 8, 8, 8);
    hl->setSpacing(2);
    auto* row1 = new QHBoxLayout;
    row1->setSpacing(8);
    sym_label_ = new QLabel(header_);
    name_label_ = new WldElideLabel(header_);
    name_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    name_label_->setMinimumWidth(40);
    name_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    research_btn_ = new QPushButton(tr("RESEARCH"), header_);
    research_btn_->setCursor(Qt::PointingHandCursor);
    connect(research_btn_, &QPushButton::clicked, this, [this]() {
        if (!symbol_.isEmpty())
            emit open_requested(QStringLiteral("equity_research"), symbol_);
    });
    close_btn_ = new QPushButton(QStringLiteral("×"), header_);
    close_btn_->setToolTip(tr("Hide the detail pane"));
    close_btn_->setCursor(Qt::PointingHandCursor);
    connect(close_btn_, &QPushButton::clicked, this, &WatchlistDetailPane::close_requested);
    row1->addWidget(sym_label_);
    row1->addWidget(name_label_, 1);
    row1->addWidget(research_btn_);
    row1->addWidget(close_btn_);
    hl->addLayout(row1);
    auto* row2 = new QHBoxLayout;
    row2->setSpacing(10);
    price_label_ = new QLabel(header_);
    change_label_ = new QLabel(header_);
    session_label_ = new QLabel(header_);
    row2->addWidget(price_label_);
    row2->addWidget(change_label_);
    row2->addStretch();
    row2->addWidget(session_label_);
    hl->addLayout(row2);
    root->addWidget(header_);

    empty_label_ = new QLabel(this);
    empty_label_->setAlignment(Qt::AlignCenter);
    empty_label_->setWordWrap(true);
    root->addWidget(empty_label_, 1);

    auto* scroll = new QScrollArea(this);
    scroll_ = scroll;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    body_ = new QWidget(scroll);
    auto* bl = new QVBoxLayout(body_);
    bl->setContentsMargins(8, 8, 8, 8);
    bl->setSpacing(8);

    chart_ = new ui::TimeSeriesChart(body_);
    chart_->set_range_presets({QStringLiteral("1D"), QStringLiteral("5D"), QStringLiteral("1M"), QStringLiteral("3M"),
                               QStringLiteral("6M"), QStringLiteral("YTD"), QStringLiteral("1Y"), QStringLiteral("5Y")});
    chart_->set_show_legend(false);
    chart_->set_show_mode_buttons(false);
    chart_->setMinimumHeight(220);
    connect(chart_, &ui::TimeSeriesChart::range_preset_selected, this, [this](const QString& preset) {
        preset_ = preset;
        load_chart();
    });
    bl->addWidget(chart_);

    tiles_ = new ui::KpiStrip(body_);
    // Two columns at the default pane width: a range ("2,686.00–4,000.00") needs ~150 px.
    tiles_->set_min_tile_width(150);
    for (const auto& t : kWldTiles) {
        ui::StatTile* tile = tiles_->add_tile(QString::fromLatin1(t.key), QCoreApplication::translate("WatchlistDetailPane", t.label));
        tile->set_compact(true);
    }
    bl->addWidget(tiles_);

    notes_title_ = new QLabel(body_);
    bl->addWidget(notes_title_);
    notes_ = new QPlainTextEdit(body_);
    notes_->setPlaceholderText(tr("Your notes on this symbol (saved automatically)"));
    notes_->setMaximumHeight(90);
    connect(notes_, &QPlainTextEdit::textChanged, this, [this]() {
        if (setting_notes_ || symbol_.isEmpty())
            return;
        notes_dirty_ = true;
        notes_timer_->start();
    });
    bl->addWidget(notes_);

    auto* nh = new QHBoxLayout;
    news_title_ = new QLabel(body_);
    news_status_ = new QLabel(body_);
    news_status_->setWordWrap(true);
    nh->addWidget(news_title_);
    nh->addWidget(news_status_, 1);
    bl->addLayout(nh);
    news_ = new QListWidget(body_);
    news_->setWordWrap(true);
    news_->setMinimumHeight(160);
    news_->setSelectionMode(QAbstractItemView::SingleSelection);
    news_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    connect(news_, &QListWidget::itemActivated, this, [](QListWidgetItem* item) {
        const QString url = item ? item->data(kWldUrlRole).toString() : QString();
        if (!url.isEmpty())
            QDesktopServices::openUrl(QUrl(url));
    });
    bl->addWidget(news_, 1);

    scroll->setWidget(body_);
    root->addWidget(scroll, 1);
    retranslate();
}

void WatchlistDetailPane::retranslate() {
    research_btn_->setText(tr("RESEARCH"));
    research_btn_->setToolTip(tr("Open in Equity Research"));
    close_btn_->setToolTip(tr("Hide the detail pane"));
    notes_title_->setText(tr("NOTES"));
    news_title_->setText(tr("HEADLINES"));
    notes_->setPlaceholderText(tr("Your notes on this symbol (saved automatically)"));
    for (const auto& t : kWldTiles) {
        if (ui::StatTile* tile = tiles_->tile(QString::fromLatin1(t.key)))
            tile->set_label(QCoreApplication::translate("WatchlistDetailPane", t.label));
    }
    empty_label_->setText(tr("Select a row to see its chart, key figures and headlines."));
}

void WatchlistDetailPane::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

void WatchlistDetailPane::refresh_theme() {
    wld_set_css(this, wld_root_css());
    wld_set_css(header_, wld_header_css());
    const QString lbl = QStringLiteral("background:transparent; color:%1; font-size:%2px; font-weight:%3;");
    wld_set_css(sym_label_, lbl.arg(colors::AMBER()).arg(fonts::px(2)).arg(700));
    wld_set_css(name_label_, lbl.arg(colors::TEXT_SECONDARY()).arg(fonts::px(-1)).arg(400));
    wld_set_css(price_label_, lbl.arg(colors::TEXT_PRIMARY()).arg(fonts::px(4)).arg(700));
    wld_set_css(session_label_, lbl.arg(colors::TEXT_TERTIARY()).arg(fonts::px(-2)).arg(600));
    const QString title = QStringLiteral("background:transparent; color:%1; font-size:%2px; font-weight:700; "
                                         "letter-spacing:0.5px;");
    wld_set_css(notes_title_, title.arg(colors::TEXT_SECONDARY()).arg(fonts::px(-3)));
    wld_set_css(news_title_, title.arg(colors::TEXT_SECONDARY()).arg(fonts::px(-3)));
    wld_set_css(news_status_, QStringLiteral("background:transparent; color:%1; font-size:%2px;")
                                  .arg(colors::TEXT_TERTIARY())
                                  .arg(fonts::px(-3)));
    wld_set_css(empty_label_, QStringLiteral("background:transparent; color:%1; font-size:%2px;")
                                  .arg(colors::TEXT_TERTIARY())
                                  .arg(fonts::px(-1)));
    const QString btn = QStringLiteral("QPushButton { background:transparent; color:%1; border:1px solid %2; "
                                       "padding:2px 8px; font-size:%3px; font-weight:700; }"
                                       "QPushButton:hover { color:%4; border-color:%4; }");
    wld_set_css(research_btn_, btn.arg(colors::TEXT_SECONDARY(), colors::BORDER_DIM())
                                   .arg(fonts::px(-3))
                                   .arg(colors::AMBER()));
    wld_set_css(close_btn_, btn.arg(colors::TEXT_TERTIARY(), colors::BORDER_DIM())
                                .arg(fonts::px(-1))
                                .arg(colors::TEXT_PRIMARY()));
    wld_set_css(notes_, QStringLiteral("QPlainTextEdit { background:%1; color:%2; border:1px solid %3; "
                                       "font-size:%4px; } QPlainTextEdit:focus { border-color:%5; }")
                            .arg(colors::BG_SURFACE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM())
                            .arg(fonts::px(-1))
                            .arg(colors::BORDER_BRIGHT()));
    wld_set_css(news_, QStringLiteral("QListWidget { background:%1; border:1px solid %2; font-size:%3px; }"
                                      "QListWidget::item { color:%4; padding:5px 6px; border-bottom:1px solid %2; }"
                                      "QListWidget::item:hover { background:%5; }"
                                      "QListWidget::item:selected { background:%5; color:%4; }")
                           .arg(colors::BG_SURFACE(), colors::BORDER_DIM())
                           .arg(fonts::px(-2))
                           .arg(colors::TEXT_PRIMARY(), colors::BG_HOVER()));
    if (model_ && !symbol_.isEmpty()) {
        if (const auto* r = model_->state(symbol_))
            update_header(*r);
    }
}

void WatchlistDetailPane::set_row(const WatchlistRowState* row) {
    const bool has = row != nullptr;
    header_->setVisible(has);
    scroll_->setVisible(has);
    empty_label_->setVisible(!has);
    if (!has) {
        if (notes_dirty_ && !symbol_.isEmpty()) {
            notes_dirty_ = false;
            emit notes_edited(symbol_, notes_->toPlainText());
        }
        symbol_.clear();
        return;
    }
    if (row->stock.symbol != symbol_) {
        if (notes_dirty_ && !symbol_.isEmpty()) {
            notes_timer_->stop();
            notes_dirty_ = false;
            emit notes_edited(symbol_, notes_->toPlainText());
        }
        symbol_ = row->stock.symbol;
        setting_notes_ = true;
        notes_->setPlainText(row->stock.notes);
        setting_notes_ = false;
        news_->clear();
        set_news_status(tr("Loading headlines…"));
        chart_->clear();
        series_ = -1;
        chart_->set_state(ui::ChartState::Loading);
        load_timer_->start();
    }
    update_row(*row);
}

void WatchlistDetailPane::update_row(const WatchlistRowState& row) {
    if (row.stock.symbol != symbol_)
        return;
    update_header(row);
    update_tiles(row);
    if (!notes_dirty_ && !notes_->hasFocus() && notes_->toPlainText() != row.stock.notes) {
        setting_notes_ = true;
        notes_->setPlainText(row.stock.notes);
        setting_notes_ = false;
    }
    const QString ccy = row.currency();
    const int digits = row.has_quote ? row.quote.price_digits : -1;
    if (ccy != price_ccy_ || digits != price_digits_) {
        price_ccy_ = ccy;
        price_digits_ = digits;
        fmt::InstrumentFmt inst = fmt::instrument_for_symbol(row.stock.symbol, ccy);
        if (digits >= 0)
            inst.decimals = digits;
        chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, [inst](double v) { return fmt::price(v, inst); });
    }
}

void WatchlistDetailPane::update_header(const WatchlistRowState& row) {
    sym_label_->setText(row.stock.symbol);
    QStringList bits;
    if (!row.display_name().isEmpty())
        bits << row.display_name();
    if (row.has_quote && !row.quote.exchange_name.isEmpty())
        bits << row.quote.exchange_name;
    if (row.has_info && !row.info.sector.isEmpty())
        bits << row.info.sector;
    const QString full_name = bits.join(QStringLiteral(" · "));
    static_cast<WldElideLabel*>(name_label_)->set_full_text(full_name);
    name_label_->setToolTip(row.has_info && !row.info.industry.isEmpty()
                                ? QStringLiteral("%1\n%2").arg(full_name, row.info.industry)
                                : full_name);
    if (!model_)
        return;
    const int c_last = wcols::index_of(QStringLiteral("last"));
    const int c_chg = wcols::index_of(QStringLiteral("change"));
    const int c_pct = wcols::index_of(QStringLiteral("change_pct"));
    const int c_state = wcols::index_of(QStringLiteral("state"));
    price_label_->setText(model_->display_text(row, c_last));
    price_label_->setToolTip(model_->data(model_->index_of_symbol(row.stock.symbol, c_last), Qt::ToolTipRole).toString());
    if (row.has_quote && row.quote.price > 0) {
        change_label_->setText(QStringLiteral("%1  (%2)").arg(model_->display_text(row, c_chg),
                                                              model_->display_text(row, c_pct)));
        const QColor c = row.quote.change > 0   ? QColor(colors::POSITIVE())
                         : row.quote.change < 0 ? QColor(colors::NEGATIVE())
                                                : QColor(colors::TEXT_SECONDARY());
        change_label_->setStyleSheet(QStringLiteral("background:transparent; color:%1; font-size:%2px; "
                                                    "font-weight:600;")
                                         .arg(c.name())
                                         .arg(fonts::px(0)));
    } else {
        change_label_->setText(row.error.isEmpty() ? QString() : tr("No quote"));
        change_label_->setStyleSheet(QStringLiteral("background:transparent; color:%1; font-size:%2px;")
                                         .arg(colors::TEXT_TERTIARY())
                                         .arg(fonts::px(0)));
    }
    const QModelIndex sidx = model_->index_of_symbol(row.stock.symbol, c_state);
    session_label_->setText(model_->display_text(row, c_state));
    session_label_->setToolTip(model_->data(sidx, Qt::ToolTipRole).toString());
    const QColor sc = model_->data(sidx, Qt::ForegroundRole).value<QColor>();
    session_label_->setStyleSheet(QStringLiteral("background:transparent; color:%1; font-size:%2px; font-weight:600;")
                                      .arg(sc.isValid() ? sc.name() : QString(colors::TEXT_TERTIARY()))
                                      .arg(fonts::px(-2)));
}

void WatchlistDetailPane::update_tiles(const WatchlistRowState& row) {
    if (!model_)
        return;
    const QString dash = fmt::placeholder();
    for (const auto& t : kWldTiles) {
        const QString key = QString::fromLatin1(t.key);
        ui::StatTile* tile = tiles_->tile(key);
        if (!tile)
            continue;
        const int col = wcols::index_of(key);
        const bool info = wld_needs_info(key);
        const bool hist = wld_needs_history(key);
        if (info && !row.has_info) {
            tile->set_state(ui::StatTile::State::Loading);
            continue;
        }
        if (hist && row.history != WatchlistHistoryState::Ready) {
            tile->set_state(row.history == WatchlistHistoryState::Failed ? ui::StatTile::State::Error
                                                                         : ui::StatTile::State::Loading,
                            row.history == WatchlistHistoryState::Failed ? tr("Daily history unavailable")
                                                                         : QString());
            continue;
        }
        if (!info && !hist && !row.has_quote) {
            if (row.error.isEmpty())
                tile->set_state(ui::StatTile::State::Loading);
            else
                tile->set_state(ui::StatTile::State::Error, row.error);
            continue;
        }
        QString text;
        QString sub;
        const services::QuoteData& q = row.quote;
        fmt::InstrumentFmt inst = fmt::instrument_for_symbol(row.stock.symbol, row.currency());
        if (q.price_digits >= 0)
            inst.decimals = q.price_digits;
        if (key == QLatin1String("day_range")) {
            text = q.low > 0 && q.high > 0 ? QStringLiteral("%1–%2").arg(fmt::price(q.low, inst),
                                                                               fmt::price(q.high, inst))
                                           : dash;
        } else if (key == QLatin1String("range_52w")) {
            text = std::isfinite(q.fifty_two_week_low) && std::isfinite(q.fifty_two_week_high)
                       ? QStringLiteral("%1–%2").arg(fmt::price(q.fifty_two_week_low, inst),
                                                           fmt::price(q.fifty_two_week_high, inst))
                       : dash;
            const QString from_hi = model_->display_text(row, wcols::index_of(QStringLiteral("from_high_52w")));
            if (!from_hi.isEmpty() && from_hi != dash)
                sub = tr("%1 from high").arg(from_hi);
        } else if (key == QLatin1String("volume")) {
            text = model_->display_text(row, col);
            const QString rel = model_->display_text(row, wcols::index_of(QStringLiteral("rel_volume")));
            if (!rel.isEmpty() && rel != dash)
                sub = tr("%1 of average").arg(rel);
        } else {
            text = model_->display_text(row, col);
        }
        if (text.isEmpty())
            text = dash;
        tile->set_state(ui::StatTile::State::Normal);
        tile->set_value(text);
        tile->set_sub_label(sub);
        if (key == QLatin1String("perf_1y") && std::isfinite(row.perf.change_1y_pct))
            tile->set_tone(row.perf.change_1y_pct >= 0 ? ui::StatTile::Tone::Positive : ui::StatTile::Tone::Negative);
        else
            tile->set_tone(ui::StatTile::Tone::Neutral);
    }
}

void WatchlistDetailPane::load_chart() {
    if (symbol_.isEmpty())
        return;
    QString topic;
    if (preset_ == QLatin1String("1D"))
        topic = services::MarketDataService::history_topic(symbol_, QStringLiteral("1d"), QStringLiteral("5m"));
    else if (preset_ == QLatin1String("5D"))
        topic = services::MarketDataService::history_topic(symbol_, QStringLiteral("5d"), QStringLiteral("30m"));
    else if (preset_ == QLatin1String("5Y"))
        topic = services::MarketDataService::history_topic(symbol_, QStringLiteral("5y"), QStringLiteral("1wk"));
    else
        topic = services::MarketDataService::performance_history_topic(symbol_); // shared with the table
    if (topic == chart_topic_ && series_ >= 0) {
        chart_->set_active_preset(preset_);
        return;
    }
    auto& hub = datahub::DataHub::instance();
    if (!chart_topic_.isEmpty()) {
        hub.unsubscribe(this, chart_topic_);
        hub.unsubscribe_errors(this, chart_topic_);
    }
    chart_topic_ = topic;
    chart_->set_state(ui::ChartState::Loading);
    hub.subscribe(this, topic, [this, topic](const QVariant& v) { apply_history(topic, v); });
    hub.subscribe_errors(this, topic, [this, topic](const QString& err) {
        if (topic == chart_topic_ && series_ < 0)
            chart_->set_state(ui::ChartState::Error, err);
    });
    hub.request(topic);
}

void WatchlistDetailPane::apply_history(const QString& topic, const QVariant& value) {
    if (topic != chart_topic_ || !value.canConvert<QVector<services::HistoryPoint>>())
        return;
    const auto points = value.value<QVector<services::HistoryPoint>>();
    QVector<double> xs;
    QVector<double> ys;
    xs.reserve(points.size());
    ys.reserve(points.size());
    for (const auto& p : points) {
        if (p.timestamp <= 0 || !(p.close > 0))
            continue;
        xs.append(static_cast<double>(p.timestamp) * 1000.0);
        ys.append(p.close);
    }
    chart_->clear();
    series_ = -1;
    if (ys.size() < 2) {
        chart_->set_state(ui::ChartState::Empty, tr("No price history for this range"));
        return;
    }
    ui::TimeSeriesChart::SeriesSpec spec;
    spec.name = symbol_;
    spec.xs = xs;
    spec.ys = ys;
    spec.style = ui::TimeSeriesChart::Style::Area;
    spec.color = ys.last() >= ys.first() ? ui::ChartColorRole::Positive : ui::ChartColorRole::Negative;
    series_ = chart_->add_series(spec);
    chart_->set_export_name(symbol_ + QLatin1Char('_') + preset_);
    chart_->set_state(ui::ChartState::Ready);
    const bool intraday = preset_ == QLatin1String("1D") || preset_ == QLatin1String("5D");
    chart_->set_active_preset(intraday || preset_ == QLatin1String("5Y") ? QString() : preset_);
}

void WatchlistDetailPane::load_news() {
    if (symbol_.isEmpty())
        return;
    set_news_status(tr("Loading headlines…"));
    EquityResearchService::instance().fetch_news(symbol_, kWldNewsCount);
}

void WatchlistDetailPane::set_news_status(const QString& text) {
    news_status_->setText(text);
    news_status_->setVisible(!text.isEmpty());
}

void WatchlistDetailPane::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (!symbol_.isEmpty() && loaded_symbol_ != symbol_)
        load_timer_->start();
}

void WatchlistDetailPane::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    // No chart subscription while nobody can see it; reloaded on the next show.
    if (!chart_topic_.isEmpty()) {
        auto& hub = datahub::DataHub::instance();
        hub.unsubscribe(this, chart_topic_);
        hub.unsubscribe_errors(this, chart_topic_);
        chart_topic_.clear();
        series_ = -1;
    }
    loaded_symbol_.clear();
    load_timer_->stop();
    if (notes_dirty_ && !symbol_.isEmpty()) {
        notes_timer_->stop();
        notes_dirty_ = false;
        emit notes_edited(symbol_, notes_->toPlainText());
    }
}

} // namespace fincept::screens
