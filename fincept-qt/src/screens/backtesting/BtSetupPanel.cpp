// src/screens/backtesting/BtSetupPanel.cpp
#include "screens/backtesting/BtSetupPanel.h"

#include "core/instrument/Instrument.h"
#include "screens/backtesting/BacktestResultData.h"
#include "screens/backtesting/BtTabKit.h"
#include "services/instruments/InstrumentResolver.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/widgets/FlowLayout.h"
#include "ui/widgets/InputMemory.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QScreen>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace fincept::screens::bt {

using namespace fincept::services::backtest;

namespace {

constexpr int kPickerIdRole = Qt::UserRole + 1;
constexpr int kPickerSearchRole = Qt::UserRole + 2;

/// Wheel over an unfocused spin box / combo scrolls the panel instead of changing the value.
class BtWheelGuard : public QObject {
  public:
    explicit BtWheelGuard(QScrollArea* scroll) : QObject(scroll), scroll_(scroll) {}

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() != QEvent::Wheel)
            return false;
        auto* w = qobject_cast<QWidget*>(watched);
        if (!w || w->hasFocus())
            return false;
        if (scroll_ && scroll_->verticalScrollBar())
            QCoreApplication::sendEvent(scroll_->verticalScrollBar(), event);
        return true;
    }

  private:
    QPointer<QScrollArea> scroll_;
};

void bt_sp_guard(QWidget* w, QObject* guard) {
    if (!w || !guard)
        return;
    w->setFocusPolicy(Qt::StrongFocus);
    w->installEventFilter(guard);
}

QWidget* bt_sp_group(QWidget* parent, const QString& caption, std::initializer_list<QWidget*> widgets) {
    auto* g = new QWidget(parent);
    g->setObjectName(QStringLiteral("btStripGroup"));
    auto* h = new QHBoxLayout(g);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(4);
    if (!caption.isEmpty()) {
        auto* l = new QLabel(caption, g);
        l->setObjectName(QStringLiteral("btFieldLabel"));
        h->addWidget(l);
    }
    for (QWidget* w : widgets) {
        w->setParent(g);
        h->addWidget(w);
    }
    return g;
}

int bt_sp_decimals_for(double step) {
    if (!(step > 0) || step >= 1)
        return 0;
    int d = 0;
    double s = step;
    while (d < 6 && std::fabs(s - std::round(s)) > 1e-9) {
        s *= 10;
        ++d;
    }
    return d;
}

QString bt_sp_engine_label(const QString& label, const QString& slug) {
    return label.isEmpty() ? slug : label;
}

} // namespace

// ═════════════════════════════════════════════════════════════════════════════
// BtStrategyPicker
// ═════════════════════════════════════════════════════════════════════════════

BtStrategyPicker::BtStrategyPicker(QWidget* parent) : QFrame(parent, Qt::Popup) {
    setObjectName(QStringLiteral("btPicker"));
    setAttribute(Qt::WA_StyledBackground, true);
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(6);
    counts_ = new QLabel(this);
    counts_->setObjectName(QStringLiteral("btNote"));
    counts_->setWordWrap(true);
    v->addWidget(counts_);
    search_ = new QLineEdit(this);
    search_->setObjectName(QStringLiteral("btFilter"));
    search_->setPlaceholderText(tr("Search name, id, category or description…"));
    search_->setClearButtonEnabled(true);
    search_->setAccessibleName(tr("Search strategies"));
    search_->installEventFilter(this);
    v->addWidget(search_);
    model_ = new QStandardItemModel(this);
    proxy_ = new QSortFilterProxyModel(this);
    proxy_->setSourceModel(model_);
    proxy_->setFilterRole(kPickerSearchRole);
    proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
    list_ = new QListView(this);
    list_->setObjectName(QStringLiteral("btPickerList"));
    list_->setModel(proxy_);
    list_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    list_->setUniformItemSizes(true);
    list_->setAccessibleName(tr("Strategies"));
    list_->installEventFilter(this);
    v->addWidget(list_, 1);
    auto* hint = new QLabel(tr("ENTER pick · ESC close · names in grey cannot run here (hover for the reason)"), this);
    hint->setObjectName(QStringLiteral("btNote"));
    v->addWidget(hint);
    connect(search_, &QLineEdit::textChanged, this, &BtStrategyPicker::apply_filter);
    connect(list_, &QListView::activated, this, [this](const QModelIndex&) { pick_current(); });
    connect(list_, &QListView::clicked, this, [this](const QModelIndex&) { pick_current(); });
}

void BtStrategyPicker::set_catalog(const QString& engine_label, const StrategyCatalog& catalog,
                                   const QString& current_id) {
    model_->clear();
    const QColor dim = tone_color(Tone::Dim);
    int unsupported = 0;
    QModelIndex current;
    for (const Strategy& s : catalog.strategies) {
        QString text = s.name + QStringLiteral("   ·   ") + s.category;
        if (!s.params.isEmpty())
            text += QStringLiteral("   ·   ") + tr("%n parameter(s)", nullptr, static_cast<int>(s.params.size()));
        auto* item = new QStandardItem(text);
        item->setData(s.id, kPickerIdRole);
        item->setData(s.name + QLatin1Char(' ') + s.id + QLatin1Char(' ') + s.category + QLatin1Char(' ') +
                          s.description + QLatin1Char(' ') + s.reason,
                      kPickerSearchRole);
        QString tip = s.description;
        if (!s.supported) {
            ++unsupported;
            item->setForeground(dim);
            item->setText(text + QStringLiteral("   ·   ") + tr("cannot run here"));
            tip += (tip.isEmpty() ? QString() : QStringLiteral("\n\n")) + tr("Cannot run here: %1").arg(s.reason);
        }
        item->setToolTip(tip);
        item->setEditable(false);
        model_->appendRow(item);
        if (s.id == current_id)
            current = model_->indexFromItem(item);
    }
    QString counts = tr("%1 · %2 strategies").arg(engine_label).arg(catalog.strategies.size());
    if (catalog.supported >= 0 && catalog.supported != catalog.total)
        counts += QStringLiteral(" · ") + tr("%1 supported by the engine").arg(catalog.supported);
    if (catalog.runnable >= 0)
        counts += QStringLiteral(" · ") + tr("%1 runnable in this environment").arg(catalog.runnable);
    if (!catalog.missing_packages.isEmpty()) {
        QStringList pk = catalog.missing_packages.keys();
        std::sort(pk.begin(), pk.end());
        counts += QStringLiteral(" — ") + tr("install %1 to enable more").arg(pk.join(QStringLiteral(", ")));
    } else if (unsupported > 0) {
        counts += QStringLiteral(" — ") + tr("%n listed but not runnable (reason on hover)", nullptr, unsupported);
    }
    counts_->setText(counts);
    search_->clear();
    if (current.isValid())
        list_->setCurrentIndex(proxy_->mapFromSource(current));
}

void BtStrategyPicker::popup_at(QWidget* anchor) {
    const QFontMetrics fm(font());
    const int w = std::max(fm.averageCharWidth() * 64, anchor ? anchor->width() : 0);
    const int h = fm.height() * 22;
    resize(w, h);
    QPoint at;
    if (anchor) {
        at = anchor->mapToGlobal(QPoint(0, anchor->height() + 2));
    } else if (parentWidget()) {
        const QWidget* win = parentWidget()->window();
        at = win->mapToGlobal(QPoint((win->width() - w) / 2, win->height() / 6));
    }
    if (const QScreen* s = QApplication::screenAt(at)) {
        const QRect avail = s->availableGeometry();
        at.setX(std::clamp(at.x(), avail.left(), std::max(avail.left(), avail.right() - w)));
        at.setY(std::clamp(at.y(), avail.top(), std::max(avail.top(), avail.bottom() - h)));
    }
    move(at);
    show();
    search_->setFocus();
    if (list_->currentIndex().isValid())
        list_->scrollTo(list_->currentIndex(), QAbstractItemView::PositionAtCenter);
}

void BtStrategyPicker::apply_filter(const QString& text) {
    proxy_->setFilterFixedString(text.trimmed());
    if (proxy_->rowCount() > 0 && !list_->currentIndex().isValid())
        list_->setCurrentIndex(proxy_->index(0, 0));
}

void BtStrategyPicker::pick_current() {
    QModelIndex idx = list_->currentIndex();
    if (!idx.isValid() && proxy_->rowCount() > 0)
        idx = proxy_->index(0, 0);
    if (!idx.isValid())
        return;
    const QString id = idx.data(kPickerIdRole).toString();
    hide();
    if (!id.isEmpty())
        emit picked(id);
}

bool BtStrategyPicker::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            pick_current();
            return true;
        }
        if (watched == search_ && (ke->key() == Qt::Key_Down || ke->key() == Qt::Key_Up)) {
            const int rows = proxy_->rowCount();
            if (rows > 0) {
                int r = list_->currentIndex().isValid() ? list_->currentIndex().row() : -1;
                r = std::clamp(r + (ke->key() == Qt::Key_Down ? 1 : -1), 0, rows - 1);
                list_->setCurrentIndex(proxy_->index(r, 0));
            }
            return true;
        }
    }
    return QFrame::eventFilter(watched, event);
}

// ═════════════════════════════════════════════════════════════════════════════
// BtInputStrip
// ═════════════════════════════════════════════════════════════════════════════

BtInputStrip::BtInputStrip(ui::InputMemory* memory, QWidget* parent) : QWidget(parent), memory_(memory) {
    setObjectName(QStringLiteral("btInputStrip"));
    setAttribute(Qt::WA_StyledBackground, true);
    auto* flow = new ui::FlowLayout(this, 12, 4); // the shared wrapping row (was a local copy)
    flow->set_compact_hint(true);                  // sizeHint == minimumSize, as the copy had
    flow->setContentsMargins(8, 4, 8, 4);
    auto* guard = new BtWheelGuard(nullptr);
    guard->setParent(this);

    // SYMBOLS: chips + search box
    chips_box_ = new QWidget(this);
    chips_layout_ = new QHBoxLayout(chips_box_);
    chips_layout_->setContentsMargins(0, 0, 0, 0);
    chips_layout_->setSpacing(4);
    symbol_box_ = new ui::SymbolSearchBox(this);
    symbol_box_->set_multi(true);
    symbol_box_->set_clear_on_pick(true);
    symbol_box_->set_compact(true);
    symbol_box_->set_recent_key(QStringLiteral("backtesting"));
    symbol_box_->set_placeholder(tr("Add symbol: ticker, name or ISIN"));
    services::ResolveOptions opts;
    using core::instrument::AssetClass;
    opts.ctx.allowed_classes = {AssetClass::Equity, AssetClass::Etf, AssetClass::Index,
                                AssetClass::Fx,     AssetClass::Crypto, AssetClass::Future};
    symbol_box_->set_options(opts);
    symbol_box_->setProperty("inputMemoryIgnore", true);
    connect(symbol_box_, &ui::SymbolSearchBox::instrument_picked, this,
            [this](const core::instrument::Instrument& inst) { add_symbol(inst.symbol); });
    connect(symbol_box_, &ui::SymbolSearchBox::instruments_picked, this,
            [this](const QVector<core::instrument::Instrument>& list) {
                for (const auto& inst : list)
                    add_symbol(inst.symbol);
            });
    connect(symbol_box_, &ui::SymbolSearchBox::unresolved, this,
            [this](const QStringList&, const QString& message) { emit unresolved(message); });
    flow->addWidget(bt_sp_group(this, tr("SYMBOLS"), {chips_box_, symbol_box_}));

    // PERIOD
    start_edit_ = new QDateEdit(this);
    end_edit_ = new QDateEdit(this);
    for (QDateEdit* e : {start_edit_, end_edit_}) {
        e->setCalendarPopup(true);
        e->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
        e->setMinimumDate(QDate(1970, 1, 1));
        e->setMaximumDate(QDate::currentDate());
        bt_sp_guard(e, guard);
    }
    start_edit_->setAccessibleName(tr("Start date"));
    end_edit_->setAccessibleName(tr("End date"));
    auto* arrow = new QLabel(QStringLiteral("→"), this);
    arrow->setObjectName(QStringLiteral("btFieldLabel"));
    flow->addWidget(bt_sp_group(this, tr("PERIOD"), {start_edit_, arrow, end_edit_}));

    interval_combo_ = new QComboBox(this);
    interval_combo_->setObjectName(QStringLiteral("btCombo"));
    for (const char* iv : {"1d", "1wk", "1mo", "1h", "30m", "15m", "5m"})
        interval_combo_->addItem(QLatin1String(iv), QLatin1String(iv));
    interval_combo_->setAccessibleName(tr("Interval"));
    bt_sp_guard(interval_combo_, guard);
    flow->addWidget(bt_sp_group(this, tr("BARS"), {interval_combo_}));

    capital_spin_ = new QDoubleSpinBox(this);
    capital_spin_->setRange(100, 1e12);
    capital_spin_->setDecimals(0);
    capital_spin_->setSingleStep(10000);
    capital_spin_->setValue(100000);
    capital_spin_->setGroupSeparatorShown(true);
    capital_spin_->setAccessibleName(tr("Initial capital"));
    bt_sp_guard(capital_spin_, guard);
    capital_ccy_ = new QLabel(this);
    capital_ccy_->setObjectName(QStringLiteral("btFieldLabel"));
    flow->addWidget(bt_sp_group(this, tr("CAPITAL"), {capital_spin_, capital_ccy_}));

    benchmark_box_ = new ui::SymbolSearchBox(this);
    benchmark_box_->set_clear_on_pick(false);
    benchmark_box_->set_compact(true);
    benchmark_box_->set_recent_key(QStringLiteral("backtesting_benchmark"));
    benchmark_box_->set_placeholder(tr("Benchmark (none)"));
    services::ResolveOptions bopts;
    bopts.ctx.allowed_classes = {AssetClass::Index, AssetClass::Etf, AssetClass::Equity, AssetClass::Crypto};
    benchmark_box_->set_options(bopts);
    connect(benchmark_box_, &ui::SymbolSearchBox::instrument_picked, this, [this](const core::instrument::Instrument&) {
        benchmark_user_set_ = true;
        if (memory_)
            memory_->set_value(QStringLiteral("benchmark_user_set"), true);
        if (!updating_)
            emit changed();
    });
    flow->addWidget(bt_sp_group(this, tr("BENCHMARK"), {benchmark_box_}));

    state_area_ = new QWidget(this);
    auto* sh = new QHBoxLayout(state_area_);
    sh->setContentsMargins(0, 0, 0, 0);
    sh->setSpacing(8);
    flow->addWidget(state_area_);

    // Defaults: the locale's index, 5 years to yesterday.
    symbols_ = {btview::default_symbol_for_locale()};
    apply_range_preset(QStringLiteral("5Y"));

    // Remembered inputs.
    if (memory_) {
        memory_->track(QStringLiteral("start"), start_edit_);
        memory_->track(QStringLiteral("end"), end_edit_);
        memory_->track(QStringLiteral("interval"), interval_combo_);
        memory_->track(QStringLiteral("capital"), capital_spin_);
        memory_->track(QStringLiteral("benchmark"), benchmark_box_);
        const QStringList stored = memory_->value(QStringLiteral("symbols")).toStringList();
        if (!stored.isEmpty())
            symbols_ = stored;
        preset_ = memory_->value(QStringLiteral("range_preset"), preset_).toString();
        benchmark_user_set_ = memory_->value(QStringLiteral("benchmark_user_set"), false).toBool();
        if (!preset_.isEmpty() && preset_ != QLatin1String("CUSTOM"))
            apply_range_preset(preset_); // a remembered preset rolls forward with the calendar
    }
    rebuild_chips();

    auto edited = [this]() {
        if (updating_)
            return;
        mark_period();
        emit changed();
    };
    connect(start_edit_, &QDateEdit::dateChanged, this, [this, edited]() {
        if (!updating_) {
            preset_ = QStringLiteral("CUSTOM");
            if (memory_)
                memory_->set_value(QStringLiteral("range_preset"), preset_);
            emit range_preset_changed(preset_);
        }
        edited();
    });
    connect(end_edit_, &QDateEdit::dateChanged, this, [this, edited]() {
        if (!updating_) {
            preset_ = QStringLiteral("CUSTOM");
            if (memory_)
                memory_->set_value(QStringLiteral("range_preset"), preset_);
            emit range_preset_changed(preset_);
        }
        edited();
    });
    connect(interval_combo_, &QComboBox::currentIndexChanged, this, edited);
    connect(capital_spin_, &QDoubleSpinBox::valueChanged, this, edited);
}

QStringList BtInputStrip::symbols() const {
    return symbols_;
}

void BtInputStrip::set_symbols(const QStringList& symbols) {
    QStringList clean;
    for (const QString& s : symbols) {
        const QString t = s.trimmed();
        if (!t.isEmpty() && !clean.contains(t, Qt::CaseInsensitive))
            clean << t;
    }
    if (clean == symbols_)
        return;
    symbols_ = clean;
    rebuild_chips();
    persist_symbols();
    emit symbols_changed();
    emit changed();
}

void BtInputStrip::add_symbol(const QString& symbol) {
    const QString t = symbol.trimmed();
    if (t.isEmpty() || symbols_.contains(t, Qt::CaseInsensitive))
        return;
    symbols_ << t;
    rebuild_chips();
    persist_symbols();
    emit symbols_changed();
    emit changed();
}

void BtInputStrip::remove_symbol(const QString& symbol) {
    if (!symbols_.removeOne(symbol))
        return;
    rebuild_chips();
    persist_symbols();
    emit symbols_changed();
    emit changed();
}

void BtInputStrip::persist_symbols() {
    if (memory_)
        memory_->set_value(QStringLiteral("symbols"), symbols_);
}

void BtInputStrip::rebuild_chips() {
    while (QLayoutItem* item = chips_layout_->takeAt(0)) {
        if (QWidget* w = item->widget())
            w->deleteLater();
        delete item;
    }
    for (int i = 0; i < symbols_.size(); ++i) {
        const QString sym = symbols_.at(i);
        auto* b = new QToolButton(chips_box_);
        b->setObjectName(QStringLiteral("btSymbolChip"));
        b->setText(sym + QStringLiteral("  ×"));
        b->setCursor(Qt::PointingHandCursor);
        b->setAccessibleName(tr("Remove %1").arg(sym));
        const bool ignored = !multi_symbol_ && i > 0;
        b->setProperty("dim", ignored);
        b->setToolTip(ignored ? tr("%1 tests the first symbol only — %2 is ignored").arg(engine_label_, sym)
                              : tr("Remove %1 from the backtest").arg(sym));
        connect(b, &QToolButton::clicked, this, [this, sym]() { remove_symbol(sym); });
        chips_layout_->addWidget(b);
    }
    chips_box_->setVisible(!symbols_.isEmpty());
    updateGeometry();
}

QDate BtInputStrip::start() const {
    return start_edit_->date();
}

QDate BtInputStrip::end() const {
    return end_edit_->date();
}

void BtInputStrip::set_period(const QDate& start, const QDate& end) {
    updating_ = true;
    if (start.isValid())
        start_edit_->setDate(start);
    if (end.isValid())
        end_edit_->setDate(end);
    updating_ = false;
    preset_ = QStringLiteral("CUSTOM");
    if (memory_)
        memory_->set_value(QStringLiteral("range_preset"), preset_);
    mark_period();
    emit range_preset_changed(preset_);
    emit changed();
}

void BtInputStrip::apply_range_preset(const QString& preset) {
    const QDate end = QDate::currentDate().addDays(-1);
    QDate start;
    if (preset == QLatin1String("1Y"))
        start = end.addYears(-1);
    else if (preset == QLatin1String("3Y"))
        start = end.addYears(-3);
    else if (preset == QLatin1String("10Y"))
        start = end.addYears(-10);
    else if (preset == QLatin1String("MAX"))
        start = QDate(1990, 1, 1);
    else
        start = end.addYears(-5);
    const bool was = updating_;
    updating_ = true;
    start_edit_->setDate(start);
    end_edit_->setDate(end);
    updating_ = was;
    preset_ = preset;
    if (memory_)
        memory_->set_value(QStringLiteral("range_preset"), preset_);
    mark_period();
    if (!was)
        emit changed();
}

QString BtInputStrip::interval() const {
    return interval_combo_->currentData().toString();
}

void BtInputStrip::set_interval(const QString& interval) {
    const int i = interval_combo_->findData(interval);
    if (i >= 0)
        interval_combo_->setCurrentIndex(i);
}

double BtInputStrip::capital() const {
    return capital_spin_->value();
}

void BtInputStrip::set_capital(double capital) {
    capital_spin_->setValue(capital);
}

QString BtInputStrip::benchmark() const {
    return benchmark_box_->text().trimmed();
}

void BtInputStrip::set_benchmark(const QString& symbol, bool user_set) {
    if (!user_set && benchmark_user_set_)
        return;
    updating_ = true;
    benchmark_box_->set_text(symbol);
    updating_ = false;
    benchmark_user_set_ = user_set;
    if (memory_)
        memory_->set_value(QStringLiteral("benchmark_user_set"), user_set);
}

void BtInputStrip::set_currency(const QString& ccy) {
    if (ccy == ccy_)
        return;
    ccy_ = ccy;
    capital_ccy_->setText(ccy.isEmpty() ? QString() : ccy);
    capital_ccy_->setToolTip(tr("Account currency = the first symbol's trading currency (the engines do not convert)"));
}

void BtInputStrip::set_multi_symbol(bool supported, const QString& engine_label) {
    multi_symbol_ = supported;
    engine_label_ = engine_label;
    rebuild_chips();
}

void BtInputStrip::set_intraday(bool supported, const QString& engine_label) {
    auto* model = qobject_cast<QStandardItemModel*>(interval_combo_->model());
    for (int i = 0; i < interval_combo_->count(); ++i) {
        const QString iv = interval_combo_->itemData(i).toString();
        const bool intraday = iv.endsWith(QLatin1Char('h')) || iv.endsWith(QLatin1Char('m')) ? !iv.endsWith(QLatin1String("mo")) : false;
        if (model && model->item(i)) {
            model->item(i)->setEnabled(!intraday || supported);
            model->item(i)->setToolTip(intraday && !supported ? tr("%1 has no intraday support").arg(engine_label)
                                                               : QString());
        }
    }
    if (!supported) {
        const QString iv = interval();
        if ((iv.endsWith(QLatin1Char('h')) || iv.endsWith(QLatin1Char('m'))) && !iv.endsWith(QLatin1String("mo")))
            set_interval(QStringLiteral("1d"));
    }
}

void BtInputStrip::focus_symbols() {
    symbol_box_->focus_and_select();
}

void BtInputStrip::focus_benchmark() {
    benchmark_box_->focus_and_select();
}

QString BtInputStrip::period_error() const {
    if (start_edit_->date() >= end_edit_->date())
        return tr("The start date must be before the end date.");
    return {};
}

void BtInputStrip::mark_period() {
    const bool bad = start_edit_->date() >= end_edit_->date();
    for (QDateEdit* e : {start_edit_, end_edit_}) {
        if (e->property("error").toBool() == bad)
            continue;
        e->setProperty("error", bad);
        e->style()->unpolish(e);
        e->style()->polish(e);
        e->setToolTip(bad ? tr("The start date must be before the end date") : QString());
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// BtSetupPanel
// ═════════════════════════════════════════════════════════════════════════════

BtSetupPanel::BtSetupPanel(ui::InputMemory* memory, QWidget* parent) : QScrollArea(parent), memory_(memory) {
    setObjectName(QStringLiteral("btSetup"));
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
    auto* guard = new BtWheelGuard(this);
    wheel_guard_ = guard;

    body_ = new QWidget(this);
    body_->setObjectName(QStringLiteral("btSetupBody"));
    body_layout_ = new QVBoxLayout(body_);
    body_layout_->setContentsMargins(0, 0, 0, 0);
    body_layout_->setSpacing(0);

    // ── STRATEGY ──
    QVBoxLayout* sv = nullptr;
    add_section(QStringLiteral("strategy"), tr("STRATEGY"), &sv);
    strategy_btn_ = new QPushButton(tr("Loading strategies…"), body_);
    strategy_btn_->setObjectName(QStringLiteral("btStrategyButton"));
    strategy_btn_->setCursor(Qt::PointingHandCursor);
    strategy_btn_->setToolTip(tr("Pick a strategy (F4)"));
    strategy_btn_->setAccessibleName(tr("Strategy"));
    connect(strategy_btn_, &QPushButton::clicked, this, [this]() { open_picker(strategy_btn_); });
    sv->addWidget(strategy_btn_);
    strategy_desc_ = make_note(QString(), body_);
    sv->addWidget(strategy_desc_);
    strategy_note_ = make_note(QString(), body_, Tone::Warn);
    strategy_note_->hide();
    sv->addWidget(strategy_note_);
    params_box_ = new QWidget(body_);
    params_layout_ = new QVBoxLayout(params_box_);
    params_layout_->setContentsMargins(0, 0, 0, 0);
    params_layout_->setSpacing(4);
    sv->addWidget(params_box_);
    params_error_ = make_note(QString(), body_, Tone::Danger);
    params_error_->hide();
    sv->addWidget(params_error_);
    params_reset_ = make_tool_button(tr("RESET PARAMETERS"), tr("Back to the strategy's defaults"), body_);
    connect(params_reset_, &QPushButton::clicked, this, [this]() {
        QJsonObject defaults;
        for (const ParamEditor& e : editors_)
            defaults.insert(e.param.name, e.param.type == QLatin1String("choice") ? QJsonValue(e.param.default_text)
                                                                                  : QJsonValue(e.param.default_val));
        apply_params(defaults);
    });
    sv->addWidget(params_reset_, 0, Qt::AlignLeft);

    // ── SIZING & RISK ──
    QVBoxLayout* zv = nullptr;
    add_section(QStringLiteral("sizing"), tr("SIZING && RISK"), &zv);
    sizing_combo_ = new QComboBox(body_);
    sizing_combo_->setObjectName(QStringLiteral("btCombo"));
    for (const BtChoice& c : sizing_method_choices())
        sizing_combo_->addItem(c.label, c.id);
    zv->addWidget(field_row(body_, tr("Method"), sizing_combo_));
    sizing_value_ = new QDoubleSpinBox(body_);
    sizing_value_->setRange(0.01, 1e12);
    sizing_value_->setDecimals(2);
    sizing_value_->setValue(100);
    zv->addWidget(field_row(body_, tr("Size"), sizing_value_, &sizing_value_label_));
    leverage_spin_ = new QDoubleSpinBox(body_);
    leverage_spin_->setRange(0.1, 20);
    leverage_spin_->setSingleStep(0.5);
    leverage_spin_->setDecimals(1);
    leverage_spin_->setValue(1.0);
    leverage_spin_->setSuffix(QStringLiteral("x"));
    zv->addWidget(field_row(body_, tr("Leverage"), leverage_spin_));
    short_check_ = new QCheckBox(tr("Allow short selling"), body_);
    zv->addWidget(short_check_);
    stop_spin_ = new QDoubleSpinBox(body_);
    stop_spin_->setRange(0, 95);
    stop_spin_->setDecimals(2);
    stop_spin_->setSuffix(QStringLiteral(" %"));
    stop_spin_->setSpecialValueText(tr("off"));
    zv->addWidget(field_row(body_, tr("Stop-loss"), stop_spin_));
    take_spin_ = new QDoubleSpinBox(body_);
    take_spin_->setRange(0, 1000);
    take_spin_->setDecimals(2);
    take_spin_->setSuffix(QStringLiteral(" %"));
    take_spin_->setSpecialValueText(tr("off"));
    zv->addWidget(field_row(body_, tr("Take-profit"), take_spin_));
    sizing_note_ = make_note(QString(), body_);
    sizing_note_->hide();
    zv->addWidget(sizing_note_);

    // ── COSTS & EXECUTION ──
    QVBoxLayout* cv = nullptr;
    add_section(QStringLiteral("costs"), tr("COSTS && EXECUTION"), &cv);
    cost_combo_ = new QComboBox(body_);
    cost_combo_->setObjectName(QStringLiteral("btCombo"));
    for (const BtChoice& c : cost_presets()) {
        cost_combo_->addItem(c.label, c.id);
        cost_combo_->setItemData(cost_combo_->count() - 1, c.detail, Qt::ToolTipRole);
    }
    cv->addWidget(field_row(body_, tr("Cost model"), cost_combo_));
    cost_detail_ = make_note(QString(), body_);
    cv->addWidget(cost_detail_);
    commission_spin_ = new QDoubleSpinBox(body_);
    commission_spin_->setRange(0, 5);
    commission_spin_->setDecimals(3);
    commission_spin_->setSingleStep(0.01);
    commission_spin_->setValue(0.10);
    commission_spin_->setSuffix(QStringLiteral(" %"));
    commission_spin_->setToolTip(tr("Commission, percent of notional per fill"));
    cv->addWidget(field_row(body_, tr("Commission"), commission_spin_));
    slippage_spin_ = new QDoubleSpinBox(body_);
    slippage_spin_->setRange(0, 5);
    slippage_spin_->setDecimals(3);
    slippage_spin_->setSingleStep(0.01);
    slippage_spin_->setValue(0.05);
    slippage_spin_->setSuffix(QStringLiteral(" %"));
    slippage_spin_->setToolTip(tr("Slippage, percent of price per fill"));
    cv->addWidget(field_row(body_, tr("Slippage"), slippage_spin_));
    fill_combo_ = new QComboBox(body_);
    fill_combo_->setObjectName(QStringLiteral("btCombo"));
    for (const BtChoice& c : fill_timings()) {
        fill_combo_->addItem(c.label, c.id);
        fill_combo_->setItemData(fill_combo_->count() - 1, c.detail, Qt::ToolTipRole);
    }
    cv->addWidget(field_row(body_, tr("Fill"), fill_combo_));
    fill_detail_ = make_note(QString(), body_);
    cv->addWidget(fill_detail_);
    rf_source_combo_ = new QComboBox(body_);
    rf_source_combo_->setObjectName(QStringLiteral("btCombo"));
    for (const BtChoice& c : risk_free_sources()) {
        rf_source_combo_->addItem(c.label, c.id);
        rf_source_combo_->setItemData(rf_source_combo_->count() - 1, c.detail, Qt::ToolTipRole);
    }
    cv->addWidget(field_row(body_, tr("Risk-free"), rf_source_combo_));
    rf_spin_ = new QDoubleSpinBox(body_);
    rf_spin_->setRange(-5, 50);
    rf_spin_->setDecimals(2);
    rf_spin_->setSingleStep(0.25);
    rf_spin_->setValue(4.0);
    rf_spin_->setSuffix(QStringLiteral(" %"));
    rf_spin_->setToolTip(tr("Annual risk-free rate: used for the whole period with \"Fixed rate\", and as the fallback "
                            "when no T-bill series exists for the currency"));
    cv->addWidget(field_row(body_, tr("Rate"), rf_spin_));
    costs_note_ = make_note(QString(), body_);
    costs_note_->hide();
    cv->addWidget(costs_note_);

    // ── DATA ──
    QVBoxLayout* dv = nullptr;
    add_section(QStringLiteral("data"), tr("DATA"), &dv);
    data_note_ = make_note(tr("Daily bars from Yahoo Finance, split + dividend adjusted (an Indian broker's candles "
                              "when one is connected, VectorBT). The result states the source, adjustment and bars it "
                              "actually used (ASSUMPTIONS)."),
                           body_);
    dv->addWidget(data_note_);
    synthetic_check_ = new QCheckBox(tr("Allow synthetic data when real prices cannot be loaded"), body_);
    synthetic_check_->setProperty("inputMemoryIgnore", true); // never remembered: an explicit, per-session choice
    synthetic_check_->setToolTip(tr("Off by default. A run on random-walk prices is marked SYNTHETIC everywhere."));
    dv->addWidget(synthetic_check_);

    // ── footer ──
    auto* foot = new QWidget(body_);
    foot->setObjectName(QStringLiteral("btSetupFooter"));
    auto* fh = new QHBoxLayout(foot);
    fh->setContentsMargins(8, 6, 8, 8);
    reset_all_ = make_tool_button(tr("RESET"), tr("Sizing, costs and execution back to the defaults"), foot);
    changed_label_ = new QLabel(foot);
    changed_label_->setObjectName(QStringLiteral("btNote"));
    fh->addWidget(reset_all_);
    fh->addWidget(changed_label_, 1);
    body_layout_->addWidget(foot);
    body_layout_->addStretch(1);
    connect(reset_all_, &QPushButton::clicked, this, [this]() {
        updating_ = true;
        sizing_combo_->setCurrentIndex(0);
        sizing_value_->setValue(100);
        leverage_spin_->setValue(1.0);
        short_check_->setChecked(false);
        stop_spin_->setValue(0);
        take_spin_->setValue(0);
        fill_combo_->setCurrentIndex(0);
        rf_source_combo_->setCurrentIndex(0);
        cost_user_set_ = false;
        if (memory_)
            memory_->set_value(QStringLiteral("cost_user_set"), false);
        updating_ = false;
        update_cost_fields();
        emit_changed();
    });

    setWidget(body_);

    for (QWidget* w : std::initializer_list<QWidget*>{sizing_combo_, sizing_value_, leverage_spin_, stop_spin_, take_spin_,
                                                      cost_combo_, commission_spin_, slippage_spin_, fill_combo_,
                                                      rf_source_combo_, rf_spin_})
        bt_sp_guard(w, guard);

    // Remembered inputs.
    if (memory_) {
        memory_->track(QStringLiteral("sizing"), sizing_combo_);
        memory_->track(QStringLiteral("sizing_value"), sizing_value_);
        memory_->track(QStringLiteral("leverage"), leverage_spin_);
        memory_->track(QStringLiteral("allow_short"), short_check_);
        memory_->track(QStringLiteral("stop_loss"), stop_spin_);
        memory_->track(QStringLiteral("take_profit"), take_spin_);
        memory_->track(QStringLiteral("cost_preset"), cost_combo_);
        memory_->track(QStringLiteral("commission"), commission_spin_);
        memory_->track(QStringLiteral("slippage"), slippage_spin_);
        memory_->track(QStringLiteral("fill_timing"), fill_combo_);
        memory_->track(QStringLiteral("rf_source"), rf_source_combo_);
        memory_->track(QStringLiteral("rf_pct"), rf_spin_);
        cost_user_set_ = memory_->value(QStringLiteral("cost_user_set"), false).toBool();
        for (auto it = section_heads_.begin(); it != section_heads_.end(); ++it) {
            const bool open = memory_->value(QStringLiteral("open_") + it.key(), true).toBool();
            it.value()->setChecked(open);
        }
    }

    connect(cost_combo_, &QComboBox::currentIndexChanged, this, [this]() {
        if (!updating_) {
            cost_user_set_ = true;
            if (memory_)
                memory_->set_value(QStringLiteral("cost_user_set"), true);
        }
        update_cost_fields();
        emit_changed();
    });
    connect(fill_combo_, &QComboBox::currentIndexChanged, this, [this]() {
        fill_detail_->setText(fill_combo_->currentData(Qt::ToolTipRole).toString());
        set_tone(fill_detail_, fill_combo_->currentData().toString() == QLatin1String("same_close") ? Tone::Warn : Tone::Dim);
        emit_changed();
    });
    connect(sizing_combo_, &QComboBox::currentIndexChanged, this, [this]() {
        const QString m = sizing_combo_->currentData().toString();
        if (sizing_value_label_)
            sizing_value_label_->setText(m == QLatin1String("fixed")       ? tr("Amount")
                                         : m == QLatin1String("vol_target") ? tr("Target vol %")
                                         : m == QLatin1String("risk")       ? tr("Risk %")
                                         : m == QLatin1String("kelly")      ? tr("Kelly scale %")
                                                                            : tr("Size %"));
        emit_changed();
    });
    for (QDoubleSpinBox* s : {sizing_value_, leverage_spin_, stop_spin_, take_spin_, commission_spin_, slippage_spin_, rf_spin_})
        connect(s, &QDoubleSpinBox::valueChanged, this, [this]() { emit_changed(); });
    connect(rf_source_combo_, &QComboBox::currentIndexChanged, this, [this]() { emit_changed(); });
    connect(short_check_, &QCheckBox::toggled, this, [this]() { emit_changed(); });
    connect(synthetic_check_, &QCheckBox::toggled, this, [this]() { emit_changed(); });
    update_cost_fields();
    fill_detail_->setText(fill_combo_->currentData(Qt::ToolTipRole).toString());

    picker_ = new BtStrategyPicker(this);
    connect(picker_, &BtStrategyPicker::picked, this, &BtSetupPanel::select_strategy);
}

BtSetupPanel::~BtSetupPanel() = default;

QWidget* BtSetupPanel::add_section(const QString& id, const QString& title, QVBoxLayout** body) {
    auto* head = new QToolButton(body_);
    head->setObjectName(QStringLiteral("btSectionHead"));
    head->setCheckable(true);
    head->setChecked(true);
    head->setToolButtonStyle(Qt::ToolButtonTextOnly);
    head->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    head->setCursor(Qt::PointingHandCursor);
    QString shown = title;
    shown.replace(QStringLiteral("&&"), QStringLiteral("&"));
    head->setAccessibleName(shown);
    auto* box = new QWidget(body_);
    box->setObjectName(QStringLiteral("btSectionBody"));
    auto* v = new QVBoxLayout(box);
    v->setContentsMargins(10, 6, 10, 10);
    v->setSpacing(5);
    auto set_text = [head, title](bool open) {
        head->setText((open ? QStringLiteral("▾  ") : QStringLiteral("▸  ")) + title);
    };
    set_text(true);
    connect(head, &QToolButton::toggled, this, [this, box, id, set_text](bool open) {
        box->setVisible(open);
        set_text(open);
        if (memory_)
            memory_->set_value(QStringLiteral("open_") + id, open);
    });
    body_layout_->addWidget(head);
    body_layout_->addWidget(box);
    section_bodies_.insert(id, box);
    section_heads_.insert(id, head);
    *body = v;
    return box;
}

QWidget* BtSetupPanel::field_row(QWidget* parent, const QString& label, QWidget* editor, QLabel** label_out) {
    auto* row = new QWidget(parent);
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(6);
    auto* l = new QLabel(label, row);
    l->setObjectName(QStringLiteral("btFieldLabel"));
    l->setBuddy(editor);
    editor->setParent(row);
    editor->setAccessibleName(label);
    h->addWidget(l);
    h->addStretch(1);
    h->addWidget(editor, 2);
    if (label_out)
        *label_out = l;
    return row;
}

void BtSetupPanel::set_engine(const QString& slug, const QString& display_name) {
    const bool changed_engine = slug != engine_;
    engine_ = slug;
    engine_label_ = display_name;
    if (changed_engine) {
        engine_caps_ = QJsonObject(); // the previous engine's; the host sends this one's when known
        catalog_ready_ = false;
        catalog_ = StrategyCatalog{};
        strategy_id_.clear();
        editors_.clear();
        rebuild_params();
        strategy_btn_->setText(tr("Loading %1 strategies…").arg(bt_sp_engine_label(display_name, slug)));
        delete engine_memory_;
        engine_memory_ = new ui::InputMemory(QStringLiteral("backtesting"), QStringLiteral("engine/") + slug, this);
    }
    update_support();
}

void BtSetupPanel::set_engine_capabilities(const QJsonObject& caps) {
    if (caps == engine_caps_)
        return;
    engine_caps_ = caps;
    update_support();
}

void BtSetupPanel::set_catalog(const StrategyCatalog& catalog) {
    catalog_ = catalog;
    catalog_ready_ = true;
    QString want = engine_memory_ ? engine_memory_->value(QStringLiteral("strategy")).toString() : QString();
    const Strategy* found = nullptr;
    for (const Strategy& s : catalog_.strategies)
        if (s.id == want)
            found = &s;
    if (!found) {
        // First runnable strategy that trades: prefer the classic SMA crossover.
        for (const Strategy& s : catalog_.strategies)
            if (s.supported && (s.id == QLatin1String("sma_crossover") || s.id == QLatin1String("sma_cross")))
                found = &s;
        if (!found)
            for (const Strategy& s : catalog_.strategies)
                if (s.supported && s.id != QLatin1String("buy_and_hold") && !s.params.isEmpty()) {
                    found = &s;
                    break;
                }
        if (!found && !catalog_.strategies.isEmpty())
            found = &catalog_.strategies.first();
    }
    picker_->set_catalog(bt_sp_engine_label(engine_label_, engine_), catalog_, found ? found->id : QString());
    if (found)
        select_strategy(found->id);
    else
        update_strategy_header();
}

void BtSetupPanel::select_strategy(const QString& id) {
    const Strategy* s = nullptr;
    for (const Strategy& x : catalog_.strategies)
        if (x.id == id)
            s = &x;
    if (!s)
        return;
    const bool same = id == strategy_id_;
    strategy_id_ = id;
    if (engine_memory_)
        engine_memory_->set_value(QStringLiteral("strategy"), id);
    if (!same)
        rebuild_params();
    update_strategy_header();
    emit strategy_changed();
    emit_changed();
}

const Strategy* BtSetupPanel::strategy() const {
    for (const Strategy& s : catalog_.strategies)
        if (s.id == strategy_id_)
            return &s;
    return nullptr;
}

QVector<StrategyParam> BtSetupPanel::params_schema() const {
    const Strategy* s = strategy();
    return s ? s->params : QVector<StrategyParam>{};
}

void BtSetupPanel::update_strategy_header() {
    const Strategy* s = strategy();
    if (!s) {
        strategy_btn_->setText(catalog_ready_ ? tr("Pick a strategy  ▾") : tr("Loading strategies…"));
        strategy_desc_->clear();
        strategy_desc_->hide();
        strategy_note_->hide();
        return;
    }
    strategy_btn_->setText(s->name + QStringLiteral("  ·  ") + s->category + QStringLiteral("  ▾"));
    strategy_btn_->setToolTip(tr("%1 (%2) — pick another strategy (F4)").arg(s->name, s->id));
    strategy_desc_->setText(s->description);
    strategy_desc_->setVisible(!s->description.isEmpty());
    if (!s->supported) {
        strategy_note_->setText(tr("Cannot run here: %1").arg(s->reason.isEmpty() ? tr("not supported") : s->reason));
        set_tone(strategy_note_, Tone::Danger);
        strategy_note_->show();
    } else if (s->id == QLatin1String("custom_combo")) {
        strategy_note_->setText(tr("Entries need every condition (AND) or any (OR); exits use the opposite side."));
        set_tone(strategy_note_, Tone::Dim);
        strategy_note_->show();
    } else {
        strategy_note_->hide();
    }
}

void BtSetupPanel::rebuild_params() {
    // Remove old editors.
    while (QLayoutItem* item = params_layout_->takeAt(0)) {
        if (QWidget* w = item->widget())
            w->deleteLater();
        delete item;
    }
    editors_.clear();
    for (ComboRow& r : combo_rows_)
        r = ComboRow{};
    combo_logic_ = nullptr;
    combo_third_ = nullptr;
    delete params_memory_;
    params_memory_ = nullptr;
    const Strategy* s = strategy();
    if (!s) {
        params_reset_->hide();
        return;
    }
    params_memory_ = new ui::InputMemory(QStringLiteral("backtesting"),
                                         QStringLiteral("params/%1/%2").arg(engine_, s->id), this);
    params_memory_->restore(); // load the document: editors tracked below get their stored values at once
    QObject* guard = wheel_guard_;
    if (s->id == QLatin1String("custom_combo")) {
        build_combo_builder(params_layout_);
        params_reset_->hide();
    } else {
        for (const StrategyParam& p : s->params) {
            ParamEditor e;
            e.param = p;
            e.row = new QWidget(params_box_);
            auto* h = new QHBoxLayout(e.row);
            h->setContentsMargins(0, 0, 0, 0);
            h->setSpacing(4);
            e.label = new QLabel(p.label.isEmpty() ? p.name : p.label, e.row);
            e.label->setObjectName(QStringLiteral("btFieldLabel"));
            QString tip = p.description;
            if (p.type == QLatin1String("choice")) {
                auto* c = new QComboBox(e.row);
                c->setObjectName(QStringLiteral("btCombo"));
                c->addItems(p.choices);
                c->setCurrentText(p.default_text);
                e.editor = c;
                connect(c, &QComboBox::currentIndexChanged, this, [this]() { update_param_marks(); emit_changed(); });
                tip += (tip.isEmpty() ? QString() : QStringLiteral("\n")) + tr("default %1").arg(p.default_text);
            } else if (p.type == QLatin1String("bool")) {
                auto* c = new QCheckBox(e.row);
                c->setChecked(p.default_val != 0);
                e.editor = c;
                connect(c, &QCheckBox::toggled, this, [this]() { update_param_marks(); emit_changed(); });
            } else if (p.type == QLatin1String("int")) {
                auto* sp = new QSpinBox(e.row);
                sp->setRange(static_cast<int>(std::floor(p.min_val)), static_cast<int>(std::ceil(p.max_val)));
                sp->setSingleStep(std::max(1, static_cast<int>(std::llround(p.step))));
                sp->setValue(static_cast<int>(std::llround(p.default_val)));
                e.editor = sp;
                connect(sp, &QSpinBox::valueChanged, this, [this]() { update_param_marks(); emit_changed(); });
                tip += (tip.isEmpty() ? QString() : QStringLiteral("\n")) +
                       tr("default %1, range %2–%3").arg(p.default_val).arg(p.min_val).arg(p.max_val);
            } else {
                auto* sp = new QDoubleSpinBox(e.row);
                sp->setDecimals(std::max(2, bt_sp_decimals_for(p.step)));
                sp->setRange(p.min_val, p.max_val);
                sp->setSingleStep(p.step > 0 ? p.step : 0.1);
                sp->setValue(p.default_val);
                e.editor = sp;
                connect(sp, &QDoubleSpinBox::valueChanged, this, [this]() { update_param_marks(); emit_changed(); });
                tip += (tip.isEmpty() ? QString() : QStringLiteral("\n")) +
                       tr("default %1, range %2–%3").arg(p.default_val).arg(p.min_val).arg(p.max_val);
            }
            e.editor->setAccessibleName(e.label->text());
            e.editor->setToolTip(tip);
            e.label->setToolTip(tip);
            bt_sp_guard(e.editor, guard);
            e.reset = new QToolButton(e.row);
            e.reset->setObjectName(QStringLiteral("btParamReset"));
            e.reset->setText(QStringLiteral("↺"));
            e.reset->setToolTip(tr("Back to the default"));
            e.reset->setAccessibleName(tr("Reset %1").arg(e.label->text()));
            e.reset->setCursor(Qt::PointingHandCursor);
            const QString pname = p.name;
            connect(e.reset, &QToolButton::clicked, this, [this, pname]() {
                for (const ParamEditor& x : editors_)
                    if (x.param.name == pname)
                        set_param_value(x, x.param.type == QLatin1String("choice") ? QJsonValue(x.param.default_text)
                                                                                     : QJsonValue(x.param.default_val));
            });
            h->addWidget(e.label);
            h->addStretch(1);
            h->addWidget(e.reset);
            h->addWidget(e.editor, 2);
            params_layout_->addWidget(e.row);
            params_memory_->track(p.name, e.editor);
            editors_.append(e);
        }
        params_reset_->setVisible(!editors_.isEmpty());
        if (editors_.isEmpty()) {
            auto* none = make_note(tr("This strategy has no parameters."), params_box_);
            params_layout_->addWidget(none);
        }
    }
    connect(params_memory_, &ui::InputMemory::changed, this, [this]() { update_param_marks(); });
    if (!pending_params_.isEmpty()) {
        const QJsonObject p = pending_params_;
        pending_params_ = QJsonObject();
        apply_params(p);
    }
    update_param_marks();
}

void BtSetupPanel::build_combo_builder(QVBoxLayout* into) {
    static const QStringList kTypes = {QStringLiteral("rsi"),        QStringLiteral("sma"),        QStringLiteral("ema"),
                                       QStringLiteral("macd_hist"),  QStringLiteral("bbands_pctb"), QStringLiteral("stochastic"),
                                       QStringLiteral("atr")};
    static const QStringList kConds = {QStringLiteral("below"), QStringLiteral("above"), QStringLiteral("cross_up"),
                                       QStringLiteral("cross_down")};
    QObject* guard = wheel_guard_;
    struct Def {
        const char* type;
        int period;
        const char* cond;
        double value;
    };
    const Def defs[3] = {{"rsi", 14, "below", 35}, {"sma", 200, "above", 0}, {"macd_hist", 26, "above", 0}};
    for (int i = 0; i < 3; ++i) {
        ComboRow& r = combo_rows_[i];
        auto* title = new QLabel(tr("CONDITION %1").arg(i + 1), params_box_);
        title->setObjectName(QStringLiteral("btSectionTitle"));
        r.type = new QComboBox(params_box_);
        r.type->setObjectName(QStringLiteral("btCombo"));
        r.type->addItems(kTypes);
        r.type->setCurrentText(QLatin1String(defs[i].type));
        r.period = new QSpinBox(params_box_);
        r.period->setRange(2, 400);
        r.period->setValue(defs[i].period);
        r.cond = new QComboBox(params_box_);
        r.cond->setObjectName(QStringLiteral("btCombo"));
        r.cond->addItems(kConds);
        r.cond->setCurrentText(QLatin1String(defs[i].cond));
        r.value = new QDoubleSpinBox(params_box_);
        r.value->setRange(-1000, 1000);
        r.value->setDecimals(2);
        r.value->setValue(defs[i].value);
        for (QWidget* w : std::initializer_list<QWidget*>{r.type, r.period, r.cond, r.value})
            bt_sp_guard(w, guard);
        auto* box = new QWidget(params_box_);
        auto* v = new QVBoxLayout(box);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(3);
        v->addWidget(title);
        v->addWidget(field_row(box, tr("Indicator"), r.type));
        v->addWidget(field_row(box, tr("Period"), r.period));
        v->addWidget(field_row(box, tr("Condition"), r.cond));
        v->addWidget(field_row(box, tr("Threshold"), r.value));
        if (i == 2) {
            combo_third_ = new QCheckBox(tr("Use a third condition"), params_box_);
            into->addWidget(combo_third_);
            box->setVisible(false);
            connect(combo_third_, &QCheckBox::toggled, box, &QWidget::setVisible);
            connect(combo_third_, &QCheckBox::toggled, this, [this]() { emit_changed(); });
            params_memory_->track(QStringLiteral("use_third"), combo_third_);
        }
        into->addWidget(box);
        const QString n = QString::number(i + 1);
        params_memory_->track(QStringLiteral("c") + n + QStringLiteral("_type"), r.type);
        params_memory_->track(QStringLiteral("c") + n + QStringLiteral("_period"), r.period);
        params_memory_->track(QStringLiteral("c") + n + QStringLiteral("_cond"), r.cond);
        params_memory_->track(QStringLiteral("c") + n + QStringLiteral("_value"), r.value);
        connect(r.type, &QComboBox::currentIndexChanged, this, [this]() { emit_changed(); });
        connect(r.cond, &QComboBox::currentIndexChanged, this, [this]() { emit_changed(); });
        connect(r.period, &QSpinBox::valueChanged, this, [this]() { emit_changed(); });
        connect(r.value, &QDoubleSpinBox::valueChanged, this, [this]() { emit_changed(); });
    }
    combo_logic_ = new QComboBox(params_box_);
    combo_logic_->setObjectName(QStringLiteral("btCombo"));
    combo_logic_->addItem(tr("AND (all)"), QStringLiteral("and"));
    combo_logic_->addItem(tr("OR (any)"), QStringLiteral("or"));
    bt_sp_guard(combo_logic_, guard);
    into->addWidget(field_row(params_box_, tr("Combine"), combo_logic_));
    params_memory_->track(QStringLiteral("logic"), combo_logic_);
    connect(combo_logic_, &QComboBox::currentIndexChanged, this, [this]() { emit_changed(); });
}

double BtSetupPanel::param_value(const ParamEditor& e) const {
    if (auto* sp = qobject_cast<QSpinBox*>(e.editor))
        return sp->value();
    if (auto* dp = qobject_cast<QDoubleSpinBox*>(e.editor))
        return dp->value();
    if (auto* cb = qobject_cast<QCheckBox*>(e.editor))
        return cb->isChecked() ? 1.0 : 0.0;
    if (auto* co = qobject_cast<QComboBox*>(e.editor))
        return co->currentIndex();
    return kBtNaN;
}

void BtSetupPanel::set_param_value(const ParamEditor& e, const QJsonValue& v) {
    if (auto* sp = qobject_cast<QSpinBox*>(e.editor))
        sp->setValue(static_cast<int>(std::llround(v.toDouble(sp->value()))));
    else if (auto* dp = qobject_cast<QDoubleSpinBox*>(e.editor))
        dp->setValue(v.toDouble(dp->value()));
    else if (auto* cb = qobject_cast<QCheckBox*>(e.editor))
        cb->setChecked(v.isBool() ? v.toBool() : v.toDouble() != 0);
    else if (auto* co = qobject_cast<QComboBox*>(e.editor))
        co->setCurrentText(v.toString(co->currentText()));
}

QJsonObject BtSetupPanel::param_values() const {
    const Strategy* s = strategy();
    if (s && s->id == QLatin1String("custom_combo") && combo_rows_[0].type) {
        QJsonArray inds;
        const int n = (combo_third_ && combo_third_->isChecked()) ? 3 : 2;
        for (int i = 0; i < n; ++i) {
            const ComboRow& r = combo_rows_[i];
            QJsonObject ind;
            ind[QStringLiteral("type")] = r.type->currentText();
            ind[QStringLiteral("period")] = r.period->value();
            const QString cond = r.cond->currentText();
            ind[QStringLiteral("condition")] = cond;
            ind[QStringLiteral("threshold")] = r.value->value();
            ind[QStringLiteral("exitCondition")] =
                (cond == QLatin1String("below") || cond == QLatin1String("cross_up")) ? QStringLiteral("above")
                                                                                      : QStringLiteral("below");
            ind[QStringLiteral("exitThreshold")] = r.type->currentText() == QLatin1String("rsi") ? 70.0 : r.value->value();
            inds.append(ind);
        }
        QJsonObject cfg;
        cfg[QStringLiteral("indicators")] = inds;
        cfg[QStringLiteral("logic")] = combo_logic_ ? combo_logic_->currentData().toString() : QStringLiteral("and");
        return QJsonObject{{QStringLiteral("comboConfig"), cfg}};
    }
    QJsonObject out;
    for (const ParamEditor& e : editors_) {
        if (auto* sp = qobject_cast<QSpinBox*>(e.editor))
            out.insert(e.param.name, sp->value());
        else if (auto* dp = qobject_cast<QDoubleSpinBox*>(e.editor))
            out.insert(e.param.name, dp->value());
        else if (auto* cb = qobject_cast<QCheckBox*>(e.editor))
            out.insert(e.param.name, cb->isChecked());
        else if (auto* co = qobject_cast<QComboBox*>(e.editor))
            out.insert(e.param.name, co->currentText());
    }
    return out;
}

void BtSetupPanel::update_param_marks() {
    // ↺ on changed values, error marks on broken cross-parameter rules.
    QHash<QString, double> vals;
    for (const ParamEditor& e : editors_) {
        const double v = param_value(e);
        vals.insert(e.param.name, v);
        bool differs = false;
        if (e.param.type == QLatin1String("choice")) {
            if (auto* co = qobject_cast<QComboBox*>(e.editor))
                differs = co->currentText() != e.param.default_text;
        } else {
            differs = std::fabs(v - e.param.default_val) > 1e-9;
        }
        if (e.reset)
            e.reset->setVisible(differs);
        if (e.label->property("changed").toBool() != differs) {
            e.label->setProperty("changed", differs);
            e.label->style()->unpolish(e.label);
            e.label->style()->polish(e.label);
        }
    }
    QStringList errors;
    QSet<QString> bad;
    if (const Strategy* s = strategy()) {
        for (const StrategyConstraint& c : s->constraints) {
            if (!vals.contains(c.less) || !vals.contains(c.greater))
                continue;
            if (vals.value(c.less) >= vals.value(c.greater)) {
                errors << (c.message.isEmpty() ? tr("%1 must be below %2").arg(c.less, c.greater) : c.message);
                bad.insert(c.less);
                bad.insert(c.greater);
            }
        }
    }
    for (const ParamEditor& e : editors_) {
        const bool err = bad.contains(e.param.name);
        if (e.label->property("error").toBool() != err) {
            e.label->setProperty("error", err);
            e.label->style()->unpolish(e.label);
            e.label->style()->polish(e.label);
        }
    }
    params_error_->setText(errors.join(QLatin1Char('\n')));
    params_error_->setVisible(!errors.isEmpty());
}

void BtSetupPanel::update_cost_fields() {
    const QString id = cost_combo_->currentData().toString();
    const bool custom = id == QLatin1String("custom");
    commission_spin_->setEnabled(custom);
    slippage_spin_->setEnabled(custom);
    double c = 0, s = 0;
    if (!custom && cost_preset_fractions(id, &c, &s)) {
        const bool was = updating_;
        updating_ = true;
        commission_spin_->blockSignals(true);
        slippage_spin_->blockSignals(true);
        commission_spin_->setValue(c * 100.0);
        slippage_spin_->setValue(s * 100.0);
        commission_spin_->blockSignals(false);
        slippage_spin_->blockSignals(false);
        updating_ = was;
    }
    cost_detail_->setText(cost_combo_->currentData(Qt::ToolTipRole).toString());
    const bool gross = id == QLatin1String("none") || (custom && commission_spin_->value() <= 0 && slippage_spin_->value() <= 0);
    set_tone(cost_detail_, gross ? Tone::Warn : Tone::Dim);
}

void BtSetupPanel::update_support() {
    const ProviderSupport sup = provider_support_from_capabilities(engine_, engine_caps_);
    const QString eng = bt_sp_engine_label(engine_label_, engine_);
    const QString na = tr("%1 does not apply this setting").arg(eng);
    auto gate = [&na](QWidget* w, bool ok) {
        w->setEnabled(ok);
        w->setToolTip(ok ? QString() : na);
    };
    gate(sizing_combo_, sup.sizing);
    gate(sizing_value_, sup.sizing);
    gate(leverage_spin_, sup.leverage);
    gate(short_check_, sup.short_selling);
    gate(stop_spin_, sup.stops);
    gate(take_spin_, sup.stops);
    QStringList ignored;
    if (!sup.sizing)
        ignored << tr("sizing");
    if (!sup.leverage)
        ignored << tr("leverage");
    if (!sup.short_selling)
        ignored << tr("short selling");
    if (!sup.stops)
        ignored << tr("stop-loss / take-profit");
    sizing_note_->setText(ignored.isEmpty() ? QString()
                                            : tr("%1 does not apply: %2 (shown as n/a; the result lists ignored settings).")
                                                  .arg(eng, ignored.join(QStringLiteral(", "))));
    sizing_note_->setVisible(!ignored.isEmpty());
    if (!sup.commission || !sup.slippage) {
        costs_note_->setText(!sup.slippage ? tr("%1 applies commission only; slippage is not modelled.").arg(eng)
                                           : tr("%1 applies slippage only.").arg(eng));
        costs_note_->show();
    } else {
        costs_note_->hide();
    }
}

void BtSetupPanel::fill_setup(BtSetup& s) const {
    s.engine = engine_;
    if (const Strategy* st = strategy()) {
        s.strategy_id = st->id;
        s.strategy_name = st->name;
        s.strategy_category = st->category;
    }
    s.strategy_params = param_values();
    s.sizing = sizing_combo_->currentData().toString();
    s.sizing_value = sizing_value_->value();
    s.leverage = leverage_spin_->value();
    s.allow_short = short_check_->isChecked();
    s.stop_loss_pct = stop_spin_->value();
    s.take_profit_pct = take_spin_->value();
    s.cost_preset = cost_combo_->currentData().toString();
    s.commission_pct = commission_spin_->value();
    s.slippage_pct = slippage_spin_->value();
    s.fill_timing = fill_combo_->currentData().toString();
    s.risk_free_source = rf_source_combo_->currentData().toString();
    s.risk_free_pct = rf_spin_->value();
    s.allow_synthetic = synthetic_check_->isChecked();
}

void BtSetupPanel::apply_setup(const BtSetup& s) {
    updating_ = true;
    auto pick = [](QComboBox* c, const QString& id) {
        const int i = c->findData(id);
        if (i >= 0)
            c->setCurrentIndex(i);
    };
    pick(sizing_combo_, s.sizing);
    sizing_value_->setValue(s.sizing_value);
    leverage_spin_->setValue(s.leverage);
    short_check_->setChecked(s.allow_short);
    stop_spin_->setValue(s.stop_loss_pct);
    take_spin_->setValue(s.take_profit_pct);
    pick(cost_combo_, s.cost_preset);
    commission_spin_->setValue(s.commission_pct);
    slippage_spin_->setValue(s.slippage_pct);
    pick(fill_combo_, s.fill_timing);
    pick(rf_source_combo_, s.risk_free_source);
    rf_spin_->setValue(s.risk_free_pct);
    cost_user_set_ = true;
    if (memory_)
        memory_->set_value(QStringLiteral("cost_user_set"), true);
    updating_ = false;
    update_cost_fields();
    if (!s.strategy_id.isEmpty()) {
        pending_params_ = s.strategy_params;
        if (s.strategy_id == strategy_id_) {
            const QJsonObject p = pending_params_;
            pending_params_ = QJsonObject();
            apply_params(p);
        } else {
            select_strategy(s.strategy_id); // rebuild_params() applies pending_params_
        }
    }
    emit_changed();
}

void BtSetupPanel::apply_params(const QJsonObject& params) {
    if (params.contains(QStringLiteral("comboConfig")) && combo_rows_[0].type) {
        const QJsonObject cfg = params.value(QStringLiteral("comboConfig")).toObject();
        const QJsonArray inds = cfg.value(QStringLiteral("indicators")).toArray();
        for (int i = 0; i < 3 && i < inds.size(); ++i) {
            const QJsonObject o = inds.at(i).toObject();
            combo_rows_[i].type->setCurrentText(o.value(QStringLiteral("type")).toString());
            combo_rows_[i].period->setValue(o.value(QStringLiteral("period")).toInt(14));
            combo_rows_[i].cond->setCurrentText(o.value(QStringLiteral("condition")).toString());
            combo_rows_[i].value->setValue(o.value(QStringLiteral("threshold")).toDouble());
        }
        if (combo_third_)
            combo_third_->setChecked(inds.size() >= 3);
        if (combo_logic_) {
            const int li = combo_logic_->findData(cfg.value(QStringLiteral("logic")).toString(QStringLiteral("and")));
            if (li >= 0)
                combo_logic_->setCurrentIndex(li);
        }
        return;
    }
    for (const ParamEditor& e : editors_)
        if (params.contains(e.param.name))
            set_param_value(e, params.value(e.param.name));
    update_param_marks();
}

void BtSetupPanel::set_venue_cost_default(const QString& preset_id) {
    if (cost_user_set_)
        return;
    const int i = cost_combo_->findData(preset_id);
    if (i < 0 || i == cost_combo_->currentIndex())
        return;
    updating_ = true;
    cost_combo_->setCurrentIndex(i);
    updating_ = false;
    update_cost_fields();
}

QString BtSetupPanel::validation_error() const {
    const Strategy* s = strategy();
    if (!catalog_ready_)
        return tr("The strategy list is still loading.");
    if (!s)
        return tr("Pick a strategy.");
    if (!s->supported)
        return tr("%1 cannot run here: %2").arg(s->name, s->reason.isEmpty() ? tr("not supported") : s->reason);
    if (params_error_->isVisible() && !params_error_->text().isEmpty())
        return params_error_->text();
    return {};
}

QString BtSetupPanel::strategy_label() const {
    const Strategy* s = strategy();
    return s ? s->name : QString();
}

QString BtSetupPanel::cost_short() const {
    return cost_preset_short(cost_combo_->currentData().toString(), commission_spin_->value(), slippage_spin_->value());
}

QString BtSetupPanel::fill_short() const {
    return fill_timing_text(fill_combo_->currentData().toString());
}

QString BtSetupPanel::risk_free_short() const {
    if (rf_source_combo_->currentData().toString() == QLatin1String("rate"))
        return tr("rf %1 fixed").arg(pct(rf_spin_->value() / 100.0, 2, false));
    return tr("rf T-bill series");
}

QString BtSetupPanel::sizing_short() const {
    const QString m = sizing_combo_->currentData().toString();
    if (!provider_support_from_capabilities(engine_, engine_caps_).sizing)
        return tr("sizing by the strategy");
    if (m == QLatin1String("fixed"))
        return tr("fixed %1").arg(num(sizing_value_->value(), 0));
    if (m == QLatin1String("vol_target"))
        return tr("vol target %1").arg(pct(sizing_value_->value() / 100.0, 0, false));
    if (m == QLatin1String("risk"))
        return tr("risk %1 / trade").arg(pct(sizing_value_->value() / 100.0, 1, false));
    if (m == QLatin1String("kelly"))
        return tr("Kelly × %1").arg(pct(sizing_value_->value() / 100.0, 0, false));
    return tr("%1 of equity").arg(pct(sizing_value_->value() / 100.0, 0, false));
}

int BtSetupPanel::changed_count() const {
    int n = 0;
    n += sizing_combo_->currentIndex() != 0 ? 1 : 0;
    n += std::fabs(sizing_value_->value() - 100.0) > 1e-9 ? 1 : 0;
    n += std::fabs(leverage_spin_->value() - 1.0) > 1e-9 ? 1 : 0;
    n += short_check_->isChecked() ? 1 : 0;
    n += stop_spin_->value() > 0 ? 1 : 0;
    n += take_spin_->value() > 0 ? 1 : 0;
    n += fill_combo_->currentIndex() != 0 ? 1 : 0;
    n += rf_source_combo_->currentIndex() != 0 ? 1 : 0;
    n += synthetic_check_->isChecked() ? 1 : 0;
    n += cost_user_set_ ? 1 : 0;
    return n;
}

void BtSetupPanel::open_picker(QWidget* anchor) {
    if (!catalog_ready_)
        return;
    picker_->popup_at(anchor ? anchor : strategy_btn_);
}

void BtSetupPanel::reveal_section(const QString& id) {
    if (QToolButton* head = section_heads_.value(id))
        head->setChecked(true);
    if (QWidget* head = section_heads_.value(id))
        QTimer::singleShot(0, this, [this, head]() { ensureWidgetVisible(head, 0, 40); });
}

void BtSetupPanel::emit_changed() {
    if (updating_)
        return;
    const int n = changed_count();
    changed_label_->setText(n > 0 ? tr("%n setting(s) differ from the defaults", nullptr, n) : tr("Default settings"));
    emit changed();
}

} // namespace fincept::screens::bt
