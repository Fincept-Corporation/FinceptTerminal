#include "screens/dashboard/TickerBar.h"

#include "services/markets/MarketDataService.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QMenu>
#include <QPaintEvent>
#include <QPainter>
#include <QPointer>
#include <QResizeEvent>
#include <QtConcurrent>

#include <cmath>

namespace fincept::screens {

static constexpr int kItemSpacing = 40;
static constexpr int kSegmentGap = 8;
static constexpr int kEditBarHeight = 24;
static const char* kSettingsKey = "ticker_bar_symbols";
static const char* kSettingsCategory = "dashboard";

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

TickerBar::TickerBar(QWidget* parent)
    : QWidget(parent), ticker_font_(ui::fonts::DATA_FAMILY(), ui::fonts::font_px(-2)), ticker_fm_(ticker_font_) {
    setFixedHeight(kEditBarHeight);
    setContextMenuPolicy(Qt::DefaultContextMenu);
    setAccessibleName(tr("Market ticker"));
    setAccessibleDescription(tr("Scrolling price ticker. Right-click to edit the symbol list."));
    setToolTip(tr("Right-click to edit ticker symbols"));

    auto apply_bg = [this]() { setStyleSheet(QString("background-color: %1;").arg(ui::colors::BG_BASE())); };
    apply_bg();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this, apply_bg](const ui::ThemeTokens&) {
                apply_bg();
                // Font family/size and every colour are theme tokens, so the
                // cached measurements go stale with the theme.
                rebuild_entry_cache();
                update();
            });

    connect(&scroll_timer_, &QTimer::timeout, this, [this]() {
        offset_ += 1.0;
        if (total_width_ > 0 && offset_ >= total_width_)
            offset_ = 0;
        update();
    });
    scroll_timer_.setInterval(50); // 20fps — started by showEvent (P3)

    // ── Inline edit overlay (hidden by default) ───────────────────────────────
    edit_bar_ = new QWidget(this);
    edit_bar_->setFixedHeight(kEditBarHeight);
    edit_bar_->hide();

    auto* hl = new QHBoxLayout(edit_bar_);
    hl->setContentsMargins(6, 2, 6, 2);
    hl->setSpacing(4);

    edit_label_ = new QLabel(tr("SYMBOLS:"), edit_bar_);
    edit_label_->setStyleSheet(
        QString("color:%1; font-size:9px; font-weight:bold; background:transparent;").arg(ui::colors::TEXT_TERTIARY()));
    hl->addWidget(edit_label_);

    edit_input_ = new QLineEdit(edit_bar_);
    edit_input_->setAccessibleName(tr("Ticker symbols"));
    edit_input_->setPlaceholderText(tr("AAPL, MSFT, ^GSPC, BTC-USD ..."));
    edit_input_->setStyleSheet(
        QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                " font-size:10px; padding:1px 6px; font-family:Consolas; }"
                "QLineEdit:focus { border-color:%4; }")
            .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::AMBER()));
    connect(edit_input_, &QLineEdit::returnPressed, this, &TickerBar::commit_edit);
    hl->addWidget(edit_input_, 1);

    edit_ok_ = new QPushButton(tr("OK"), edit_bar_);
    edit_ok_->setFixedWidth(32);
    edit_ok_->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none;"
                                    " font-size:9px; font-weight:bold; padding:2px; }"
                                    "QPushButton:hover { background:%3; }")
                                .arg(ui::colors::AMBER(), ui::colors::BG_BASE(), ui::colors::AMBER()));
    connect(edit_ok_, &QPushButton::clicked, this, &TickerBar::commit_edit);
    hl->addWidget(edit_ok_);

    edit_cancel_ = new QPushButton("✕", edit_bar_);
    edit_cancel_->setFixedWidth(24);
    edit_cancel_->setAccessibleName(tr("Cancel"));
    edit_ok_->setAccessibleName(tr("Apply ticker symbols"));
    setTabOrder(edit_input_, edit_ok_);
    setTabOrder(edit_ok_, edit_cancel_);
    edit_cancel_->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:none;"
                                        " font-size:11px; padding:2px; }"
                                        "QPushButton:hover { color:%2; }")
                                    .arg(ui::colors::TEXT_TERTIARY(), ui::colors::RED()));
    connect(edit_cancel_, &QPushButton::clicked, this, &TickerBar::hide_edit_bar);
    hl->addWidget(edit_cancel_);

    // Load persisted symbols (falls back to defaults if nothing saved yet).
    load_symbols();
}

// ─────────────────────────────────────────────────────────────────────────────
// Symbol persistence
// ─────────────────────────────────────────────────────────────────────────────

void TickerBar::load_symbols() {
    // Build default list: indices + movers
    const QStringList defaults =
        services::MarketDataService::indices_symbols() + services::MarketDataService::mover_symbols();

    // Attempt to load from settings on a background thread; fall back to defaults
    // immediately so the ticker isn't blank while the DB read is in flight.
    symbols_ = defaults;

    QPointer<TickerBar> self = this;
    (void)QtConcurrent::run([self, defaults]() {
        auto result = fincept::SettingsRepository::instance().get(kSettingsKey);
        QStringList loaded;
        if (result.is_ok() && !result.value().isEmpty()) {
            for (const QString& s : result.value().split(',')) {
                const QString t = s.trimmed().toUpper();
                if (!t.isEmpty())
                    loaded << t;
            }
        }
        QMetaObject::invokeMethod(
            self,
            [self, loaded, defaults]() {
                if (!self)
                    return;
                self->symbols_ = loaded.isEmpty() ? defaults : loaded;
                emit self->symbols_changed(self->symbols_);
            },
            Qt::QueuedConnection);
    });
}

void TickerBar::save_symbols() {
    const QString value = symbols_.join(",");
    (void)QtConcurrent::run(
        [value]() { fincept::SettingsRepository::instance().set(kSettingsKey, value, kSettingsCategory); });
}

// ─────────────────────────────────────────────────────────────────────────────
// set_data — called by DashboardScreen after fetch_quotes returns
// ─────────────────────────────────────────────────────────────────────────────

// Derives every string, width and colour paintEvent needs, so the paint path is
// a pure draw. The old code measured each entry here to compute total_width_ and
// then *discarded* the per-entry widths, forcing paintEvent to re-measure all
// three segments per entry per pass at 20 fps — roughly 3,000 text-shaping calls
// a second, forever, on the always-visible dashboard ticker. NewsTickerStrip
// already caches its widths this way; this brings TickerBar in line.
void TickerBar::rebuild_entry_cache() {
    ticker_font_ = QFont(ui::fonts::DATA_FAMILY(), ui::fonts::font_px(-2));
    ticker_fm_ = QFontMetrics(ticker_font_);
    symbol_color_ = QColor(ui::colors::WHITE());
    price_color_ = QColor(ui::colors::GRAY());

    total_width_ = 0;
    for (auto& e : entries_) {
        e.price_text = QString::number(e.price, 'f', 2);
        e.change_text = QString("%1%2%").arg(e.change_pct >= 0 ? "+" : "").arg(e.change_pct, 0, 'f', 2);
        e.change_col = QColor(ui::change_color(e.change_pct));

        e.symbol_width = ticker_fm_.horizontalAdvance(e.symbol);
        e.price_width = ticker_fm_.horizontalAdvance(e.price_text);
        e.change_width = ticker_fm_.horizontalAdvance(e.change_text);
        e.total_width = e.symbol_width + kSegmentGap + e.price_width + kSegmentGap + e.change_width + kItemSpacing;

        total_width_ += e.total_width;
    }
}

void TickerBar::set_data(const QVector<Entry>& entries) {
    const bool symbols_changed = entries.size() != entries_.size();
    entries_ = entries;

    rebuild_entry_cache();

    // Preserve the scroll position across price updates. set_data() is called
    // on EVERY per-symbol hub delivery (rebuild_ticker_from_cache), so zeroing
    // the offset made the ticker visibly snap back to the start several times
    // a second. Only reset when the symbol set itself changed.
    if (symbols_changed || total_width_ <= 0)
        offset_ = 0;
    else if (offset_ >= total_width_)
        offset_ = std::fmod(offset_, static_cast<double>(total_width_));

    if (total_width_ > 0 && isVisible() && !edit_bar_->isVisible())
        scroll_timer_.start();
    else if (total_width_ <= 0)
        scroll_timer_.stop();

    update();
}

// ─────────────────────────────────────────────────────────────────────────────
// Show / Hide
// ─────────────────────────────────────────────────────────────────────────────

void TickerBar::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (total_width_ > 0)
        scroll_timer_.start();
}

void TickerBar::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    scroll_timer_.stop();
    hide_edit_bar();
}

void TickerBar::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (edit_bar_)
        edit_bar_->setGeometry(0, 0, width(), kEditBarHeight);
}

// ─────────────────────────────────────────────────────────────────────────────
// Context menu — right-click to edit symbols
// ─────────────────────────────────────────────────────────────────────────────

void TickerBar::contextMenuEvent(QContextMenuEvent* event) {
    QMenu menu(this);
    menu.setStyleSheet(QString("QMenu { background:%1; color:%2; border:1px solid %3; font-size:11px; }"
                               "QMenu::item:selected { background:%4; color:%5; }")
                           .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(),
                                ui::colors::AMBER(), ui::colors::BG_BASE()));

    auto* edit_action = menu.addAction(tr("Edit Symbols..."));
    connect(edit_action, &QAction::triggered, this, &TickerBar::show_edit_bar);

    menu.exec(event->globalPos());
}

void TickerBar::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void TickerBar::retranslateUi() {
    if (edit_label_)
        edit_label_->setText(tr("SYMBOLS:"));
    if (edit_input_)
        edit_input_->setPlaceholderText(tr("AAPL, MSFT, ^GSPC, BTC-USD ..."));
    if (edit_ok_)
        edit_ok_->setText(tr("OK"));
}

// ─────────────────────────────────────────────────────────────────────────────
// Inline edit bar
// ─────────────────────────────────────────────────────────────────────────────

void TickerBar::show_edit_bar() {
    scroll_timer_.stop();
    edit_input_->setText(symbols_.join(", "));
    edit_bar_->setGeometry(0, 0, width(), kEditBarHeight);
    edit_bar_->raise();
    edit_bar_->show();
    edit_input_->setFocus();
    edit_input_->selectAll();
}

void TickerBar::hide_edit_bar() {
    edit_bar_->hide();
    if (total_width_ > 0 && isVisible())
        scroll_timer_.start();
}

void TickerBar::commit_edit() {
    QStringList updated;
    for (const QString& s : edit_input_->text().split(',')) {
        const QString t = s.trimmed().toUpper();
        if (!t.isEmpty())
            updated << t;
    }

    hide_edit_bar();

    if (updated.isEmpty() || updated == symbols_)
        return;

    symbols_ = updated;
    save_symbols();
    emit symbols_changed(symbols_);
}

// ─────────────────────────────────────────────────────────────────────────────
// Paint
// ─────────────────────────────────────────────────────────────────────────────

void TickerBar::paintEvent(QPaintEvent*) {
    if (entries_.isEmpty() || total_width_ <= 0)
        return;

    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing);

    // Font, metrics, strings, widths and colours all come from the cache built in
    // rebuild_entry_cache(). Nothing here formats or measures — see P9.
    p.setFont(ticker_font_);

    const int text_y = (height() + ticker_fm_.ascent() - ticker_fm_.descent()) / 2;
    const int viewport_w = width();
    double x = -offset_;
    const int passes = (viewport_w / total_width_) + 2;

    for (int pass = 0; pass < passes; ++pass) {
        for (const auto& e : entries_) {
            // Skip entries scrolled off either edge. The old loop drew every
            // entry of every pass regardless of visibility.
            if (x + e.total_width < 0 || x > viewport_w) {
                x += e.total_width;
                continue;
            }

            p.setPen(symbol_color_);
            p.drawText(QPointF(x, text_y), e.symbol);
            x += e.symbol_width + kSegmentGap;

            p.setPen(price_color_);
            p.drawText(QPointF(x, text_y), e.price_text);
            x += e.price_width + kSegmentGap;

            p.setPen(e.change_col);
            p.drawText(QPointF(x, text_y), e.change_text);
            x += e.change_width + kItemSpacing;
        }
    }
}

} // namespace fincept::screens
