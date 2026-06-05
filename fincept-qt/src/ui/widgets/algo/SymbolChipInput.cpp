// src/ui/widgets/algo/SymbolChipInput.cpp
#include "ui/widgets/algo/SymbolChipInput.h"

#include "services/markets/MarketDataService.h"
#include "trading/AccountManager.h"
#include "trading/instruments/InstrumentNormalize.h"
#include "trading/instruments/InstrumentService.h"
#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QStringListModel>
#include <QStyle>

namespace fincept::ui::algo {

// ── Minimal flow layout (wraps children to new rows) — standard Qt example ──
class FlowLayout : public QLayout {
  public:
    explicit FlowLayout(QWidget* parent, int margin = 4, int hspace = 4, int vspace = 4)
        : QLayout(parent), hspace_(hspace), vspace_(vspace) { setContentsMargins(margin, margin, margin, margin); }
    ~FlowLayout() override { QLayoutItem* it; while ((it = takeAt(0))) delete it; }
    void addItem(QLayoutItem* item) override { items_.append(item); }
    int count() const override { return items_.size(); }
    QLayoutItem* itemAt(int i) const override { return items_.value(i); }
    QLayoutItem* takeAt(int i) override { return (i >= 0 && i < items_.size()) ? items_.takeAt(i) : nullptr; }
    Qt::Orientations expandingDirections() const override { return {}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override { return do_layout(QRect(0, 0, w, 0), true); }
    void setGeometry(const QRect& r) override { QLayout::setGeometry(r); do_layout(r, false); }
    QSize sizeHint() const override { return minimumSize(); }
    QSize minimumSize() const override {
        QSize s; for (auto* it : items_) s = s.expandedTo(it->minimumSize());
        const QMargins m = contentsMargins(); s += QSize(m.left() + m.right(), m.top() + m.bottom()); return s;
    }
  private:
    int do_layout(const QRect& rect, bool test) const {
        const QMargins m = contentsMargins();
        QRect eff = rect.adjusted(m.left(), m.top(), -m.right(), -m.bottom());
        int x = eff.x(), y = eff.y(), line_h = 0;
        for (auto* it : items_) {
            const QSize sz = it->sizeHint();
            int next_x = x + sz.width() + hspace_;
            if (next_x - hspace_ > eff.right() && line_h > 0) { x = eff.x(); y += line_h + vspace_; next_x = x + sz.width() + hspace_; line_h = 0; }
            if (!test) it->setGeometry(QRect(QPoint(x, y), sz));
            x = next_x; line_h = qMax(line_h, sz.height());
        }
        return y + line_h - rect.y() + m.bottom();
    }
    QList<QLayoutItem*> items_; int hspace_, vspace_;
};

namespace {
QStringList loaded_broker_ids() {
    QStringList ids;
    for (const auto& a : fincept::trading::AccountManager::instance().list_accounts())
        if (!ids.contains(a.broker_id) && fincept::trading::InstrumentService::instance().is_loaded(a.broker_id))
            ids << a.broker_id;
    return ids;
}
} // namespace

SymbolChipInput::SymbolChipInput(QWidget* parent) : QFrame(parent) {
    setStyleSheet(QString("QFrame { background:%1; border:1px solid %2; }")
                      .arg(fincept::ui::colors::BG_SURFACE(), fincept::ui::colors::BORDER_DIM()));
    flow_ = new FlowLayout(this, 6, 4, 4);

    edit_ = new QLineEdit(this);
    edit_->setFrame(false);
    edit_->setPlaceholderText(tr("type a ticker…"));
    edit_->setMinimumWidth(120);
    edit_->setStyleSheet(QString("QLineEdit { background:transparent; border:none; color:%1; font-family:%2; font-size:%3px; }")
        .arg(fincept::ui::colors::TEXT_PRIMARY())
        .arg(QString(fincept::ui::fonts::DATA_FAMILY))
        .arg(fincept::ui::fonts::SMALL));
    edit_->installEventFilter(this);
    flow_->addWidget(edit_);

    model_ = new QStringListModel(this);
    completer_ = new QCompleter(model_, this);
    completer_->setCaseSensitivity(Qt::CaseInsensitive);
    completer_->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    completer_->setMaxVisibleItems(8);
    edit_->setCompleter(completer_);

    // The completer popup is a separate top-level QListView that no global
    // stylesheet covers — without this it renders transparent and the controls
    // behind it bleed through. Give it an opaque, themed look + click-to-add.
    if (auto* popup = completer_->popup()) {
        popup->setObjectName("symChipCompleterPopup");
        popup->setTextElideMode(Qt::ElideNone); // show full label, never "…"
        popup->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        popup->setStyleSheet(
            QString("QListView { background:%1; color:%2; border:1px solid %3; outline:none;"
                    " font-family:%4; font-size:%5px; padding:3px; }"
                    "QListView::item { padding:5px 10px; border:none; border-radius:2px; }"
                    "QListView::item:hover { background:%6; }"
                    "QListView::item:selected { background:%6; color:%7; }"
                    "QScrollBar:vertical { background:%1; width:8px; margin:0; }"
                    "QScrollBar::handle:vertical { background:%3; border-radius:4px; min-height:20px; }"
                    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }")
                .arg(fincept::ui::colors::BG_SURFACE(), fincept::ui::colors::TEXT_PRIMARY(),
                     fincept::ui::colors::BORDER_MED(), fincept::ui::fonts::DATA_FAMILY())
                .arg(fincept::ui::fonts::SMALL)
                .arg(fincept::ui::colors::BG_HOVER(), fincept::ui::colors::AMBER()));
        connect(popup, &QAbstractItemView::clicked, this, [this](const QModelIndex& idx) {
            commit_label(idx.data().toString());
        });
    }

    connect(edit_, &QLineEdit::textEdited, this, &SymbolChipInput::on_text_changed);
    connect(edit_, &QLineEdit::returnPressed, this, [this]() { commit_text(edit_->text()); });
    connect(completer_, QOverload<const QString&>::of(&QCompleter::activated), this,
            [this](const QString& label) { commit_label(label); });
}

// Show only canonical alphabetic exchanges (NSE/BSE/NFO…); hide broker numeric
// segment codes that would otherwise render as a meaningless "· 1".
static QString pretty_exchange(const QString& ex) {
    const QString e = ex.trimmed().toUpper();
    if (e.endsWith(QStringLiteral("_INDEX"))) return e.left(e.size() - 6) + QStringLiteral(" IDX");
    static const QStringList known = {"NSE", "BSE", "NFO", "BFO", "MCX", "CDS", "BCD", "NCDEX"};
    return known.contains(e) ? e : QString();
}

void SymbolChipInput::on_text_changed(const QString& text) {
    const QString q = text.trimmed();
    if (q.isEmpty()) { model_->setStringList({}); sugg_map_.clear(); return; }
    const auto results = fincept::trading::InstrumentService::instance().search_all(q, "", loaded_broker_ids(), 16);
    QStringList labels;
    sugg_map_.clear();
    for (const auto& r : results) {
        // Friendly label (clean F&O / underlying name) instead of the raw canonical
        // symbol; remember the real symbol so a pick commits the right ticker.
        const QString friendly = fincept::trading::norm::display_name(
            r.name, r.instrument_type, r.expiry, r.strike, r.symbol);
        const QString ex = pretty_exchange(r.exchange);
        const QString label = ex.isEmpty() ? friendly : QStringLiteral("%1   ·   %2").arg(friendly, ex);
        if (sugg_map_.contains(label)) continue; // collapse identical labels across brokers
        sugg_map_.insert(label, r.symbol);
        labels << label;
    }
    model_->setStringList(labels);

    // Widen the popup to fit the longest label so nothing is truncated (the
    // popup otherwise inherits the narrow line-edit width).
    if (auto* popup = completer_->popup()) {
        const QFontMetrics fm(edit_->font());
        int w = 0;
        for (const auto& l : labels) w = qMax(w, fm.horizontalAdvance(l));
        popup->setMinimumWidth(qBound(220, w + 56, 560));
    }
}

bool SymbolChipInput::eventFilter(QObject* obj, QEvent* ev) {
    if (obj == edit_ && ev->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(ev);
        if (ke->key() == Qt::Key_Tab) {
            // accept the highlighted/first suggestion if the popup is up, else raw text
            if (completer_->popup() && completer_->popup()->isVisible()) {
                const auto idx = completer_->popup()->currentIndex();
                const QString label = idx.isValid() ? idx.data().toString() : completer_->currentCompletion();
                if (!label.trimmed().isEmpty()) { commit_label(label); return true; }
            }
            if (!edit_->text().trimmed().isEmpty()) { commit_text(edit_->text()); return true; }
        } else if (ke->key() == Qt::Key_Backspace && edit_->text().isEmpty() && !symbols_.isEmpty()) {
            remove_chip(symbols_.last()); return true; // backspace on empty edit removes last chip
        } else if (ke->key() == Qt::Key_Comma) {
            commit_text(edit_->text()); return true;
        }
    }
    return QFrame::eventFilter(obj, ev);
}

void SymbolChipInput::commit_text(const QString& raw) {
    // If the whole text is a picked suggestion label (e.g. "HINDUNILVR · NSE"),
    // resolve it to its single real symbol instead of fragmenting it.
    const QString whole = raw.trimmed();
    if (sugg_map_.contains(whole)) { commit_label(whole); return; }
    // Split ONLY on comma/newline (never space) so a "SYMBOL · EXCH" label is
    // never broken into pieces; a bare ticker has no spaces.
    const auto parts = raw.split(QRegularExpression("[,\\n\\r]+"), Qt::SkipEmptyParts);
    for (const auto& p : parts) {
        const QString t = p.trimmed();
        if (t.isEmpty()) continue;
        add_chip(sugg_map_.contains(t) ? sugg_map_.value(t) : t.toUpper());
    }
    edit_->clear();
    model_->setStringList({});
    sugg_map_.clear();
    if (completer_->popup()) completer_->popup()->hide();
}

void SymbolChipInput::commit_label(const QString& label) {
    // Resolve the friendly suggestion label back to its real canonical symbol.
    QString sym = sugg_map_.value(label);
    if (sym.isEmpty()) sym = label.section(QStringLiteral("   ·   "), 0, 0).trimmed().toUpper();
    add_chip(sym);
    edit_->clear();
    model_->setStringList({});
    sugg_map_.clear();
    if (completer_->popup()) completer_->popup()->hide();
}

void SymbolChipInput::add_chip(const QString& symbol) {
    if (symbol.isEmpty() || symbols_.contains(symbol)) return;
    symbols_ << symbol;

    auto* chip = new QFrame(this);
    chip->setStyleSheet(QString("QFrame { background:%1; border:1px solid %2; border-radius:3px; }")
                            .arg(fincept::ui::colors::BG_RAISED(), fincept::ui::colors::AMBER()));
    auto* hl = new QHBoxLayout(chip);
    hl->setContentsMargins(6, 2, 4, 2); hl->setSpacing(4);
    auto* lbl = new QLabel(symbol, chip);
    lbl->setObjectName("sym");
    lbl->setStyleSheet(QString("color:%1; font-family:%2; font-size:%3px; font-weight:700; border:none; background:transparent;")
        .arg(fincept::ui::colors::TEXT_PRIMARY())
        .arg(QString(fincept::ui::fonts::DATA_FAMILY))
        .arg(fincept::ui::fonts::SMALL));
    auto* x = new QPushButton("✕", chip);
    x->setCursor(Qt::PointingHandCursor);
    x->setFixedSize(16, 16);
    x->setStyleSheet(QString("QPushButton { color:%1; border:none; background:transparent; font-size:%2px; }"
                             "QPushButton:hover { color:%3; }")
        .arg(fincept::ui::colors::TEXT_TERTIARY())
        .arg(fincept::ui::fonts::TINY)
        .arg(fincept::ui::colors::NEGATIVE()));
    connect(x, &QPushButton::clicked, this, [this, symbol]() { remove_chip(symbol); });
    hl->addWidget(lbl); hl->addWidget(x);

    // insert the chip BEFORE the trailing edit_
    flow_->removeWidget(edit_);
    flow_->addWidget(chip);
    flow_->addWidget(edit_);
    chips_ << chip;

    emit symbols_changed();
    fetch_price(symbol);
    edit_->setFocus();
}

void SymbolChipInput::remove_chip(const QString& symbol) {
    const int i = symbols_.indexOf(symbol);
    if (i < 0) return;
    symbols_.removeAt(i);
    QWidget* chip = chips_.takeAt(i);
    flow_->removeWidget(chip);
    chip->deleteLater();
    emit symbols_changed();
}

void SymbolChipInput::fetch_price(const QString& symbol) {
    // Namespace is fincept::services (not fincept::services::markets).
    // QuoteCallback = std::function<void(bool, QVector<QuoteData>)> — by value, not const-ref.
    fincept::services::MarketDataService::instance().fetch_quotes(
        {symbol}, [this, symbol](bool ok, QVector<fincept::services::QuoteData> quotes) {
            if (!ok || quotes.isEmpty()) return;
            const double px = quotes.first().price;
            const int i = symbols_.indexOf(symbol);
            if (i >= 0 && i < chips_.size())
                if (auto* lbl = chips_[i]->findChild<QLabel*>("sym"))
                    lbl->setText(QString("%1  %2").arg(symbol, QString::number(px, 'f', 2)));
            emit price_resolved(symbol, px);
        });
}

QStringList SymbolChipInput::symbols() const { return symbols_; }

void SymbolChipInput::set_symbols(const QStringList& syms) {
    clear();
    for (const auto& s : syms) add_chip(s.trimmed().toUpper());
}

void SymbolChipInput::clear() {
    for (auto* c : chips_) { flow_->removeWidget(c); c->deleteLater(); }
    chips_.clear();
    symbols_.clear();
    emit symbols_changed();
}

} // namespace fincept::ui::algo
