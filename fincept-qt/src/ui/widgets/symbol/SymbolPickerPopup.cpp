// SymbolPickerPopup.cpp — see SymbolPickerPopup.h. File-scope names carry an spp_
// prefix (unity builds).

#include "ui/widgets/symbol/SymbolPickerPopup.h"

#include "ui/theme/Theme.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QCoreApplication>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QLabel>
#include <QListView>
#include <QPainter>
#include <QScreen>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::ui {

namespace {

namespace ci = core::instrument;

QString spp_tr(const char* text) {
    return QCoreApplication::translate("SymbolSearchBox", text);
}

/// Short class tag shown on a row.
QString spp_class_tag(ci::AssetClass c) {
    switch (c) {
        case ci::AssetClass::Equity:
            return spp_tr("STOCK");
        case ci::AssetClass::Etf:
            return spp_tr("ETF");
        case ci::AssetClass::Fund:
            return spp_tr("FUND");
        case ci::AssetClass::Index:
            return spp_tr("INDEX");
        case ci::AssetClass::Fx:
            return spp_tr("FX");
        case ci::AssetClass::Crypto:
            return spp_tr("CRYPTO");
        case ci::AssetClass::Future:
            return spp_tr("FUTURE");
        case ci::AssetClass::Option:
            return spp_tr("OPTION");
        case ci::AssetClass::Bond:
            return spp_tr("BOND");
        case ci::AssetClass::Commodity:
            return spp_tr("CMDTY");
        case ci::AssetClass::Unknown:
            break;
    }
    return {};
}

/// Venue (or country) and currency: "NSE · INR".
QString spp_meta(const ci::Instrument& inst) {
    QStringList parts;
    if (!inst.venue.isEmpty())
        parts << inst.venue;
    else if (!inst.country.isEmpty())
        parts << inst.country;
    if (!inst.currency.isEmpty())
        parts << inst.currency;
    return parts.join(QString::fromUtf8(" \xc2\xb7 "));
}

/// Width `text` needs in `font`, rounded up (integer metrics round down half the time,
/// which elides text that fits).
int spp_width(const QFont& font, const QString& text) {
    return static_cast<int>(std::ceil(QFontMetricsF(font).horizontalAdvance(text))) + 1;
}

/// Draw `text` into `rect` (vertically centred), elided only when it does not fit, with
/// [from, from + len) in `emph`. Returns the width drawn.
int spp_draw_emphasised(QPainter* p, const QRect& rect, const QFont& font, const QString& text, const QColor& normal,
                        const QColor& emph, qsizetype from, qsizetype len) {
    if (rect.width() <= 0 || text.isEmpty())
        return 0;
    const QFontMetricsF fm(font);
    const QString shown =
        fm.horizontalAdvance(text) <= rect.width() ? text : fm.elidedText(text, Qt::ElideRight, rect.width());
    const bool elided = shown != text;
    const qsizetype visible = elided ? std::max<qsizetype>(0, shown.size() - 1) : shown.size();
    p->setFont(font);
    const qreal baseline = rect.top() + (rect.height() - fm.height()) / 2 + fm.ascent();
    qreal x = rect.left();
    auto segment = [&](qsizetype a, qsizetype b, const QColor& c) {
        if (b <= a)
            return;
        const QString s = shown.mid(a, b - a);
        p->setPen(c);
        p->drawText(QPointF(x, baseline), s);
        x += fm.horizontalAdvance(s);
    };
    if (len <= 0 || from < 0 || from >= visible) {
        segment(0, shown.size(), normal);
    } else {
        const qsizetype end = std::min(from + len, visible);
        segment(0, from, normal);
        segment(from, end, emph);
        segment(end, shown.size(), normal);
    }
    return static_cast<int>(std::ceil(x - rect.left()));
}

} // namespace

// ── Model ───────────────────────────────────────────────────────────────────

void SymbolResultModel::set_rows(QVector<SymbolRow> rows, const QString& code_query, const QString& name_query) {
    beginResetModel();
    rows_ = std::move(rows);
    code_query_ = code_query;
    name_query_ = name_query;
    endResetModel();
}

const SymbolRow* SymbolResultModel::row_at(int row) const {
    return (row >= 0 && row < rows_.size()) ? &rows_.at(row) : nullptr;
}

int SymbolResultModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant SymbolResultModel::data(const QModelIndex& index, int role) const {
    const SymbolRow* row = row_at(index.row());
    if (!row)
        return {};
    switch (role) {
        case Qt::DisplayRole:
            return row->instrument.display;
        case Qt::ToolTipRole:
        case Qt::AccessibleTextRole: {
            QString text = describe_instrument(row->instrument);
            if (!row->marker.isEmpty())
                text += QString::fromUtf8(" \xe2\x80\x94 ") + row->marker;
            return text;
        }
        default:
            break;
    }
    return {};
}

// ── Delegate ────────────────────────────────────────────────────────────────

SymbolRowDelegate::SymbolRowDelegate(QObject* parent) : QStyledItemDelegate(parent) {
    refresh_theme();
}

void SymbolRowDelegate::refresh_theme() {
    symbol_font_ = fonts::font(0, QFont::DemiBold);
    meta_font_ = fonts::font(-2);
    tag_font_ = fonts::font(-4, QFont::DemiBold);
    const QFontMetrics fs(symbol_font_);
    const QFontMetrics fm(meta_font_);
    pad_h_ = std::max(6, fm.averageCharWidth());
    pad_v_ = std::max(3, fm.height() / 4);
    row_height_ = pad_v_ * 2 + fs.height() + std::max(1, fm.height() / 8) + fm.height();
    text_ = colors::TEXT_PRIMARY;
    text2_ = colors::TEXT_SECONDARY;
    text3_ = colors::TEXT_TERTIARY;
    accent_ = colors::AMBER;
    accent_bg_ = colors::ACCENT_BG;
    tag_bg_ = colors::BG_RAISED;
    tag_border_ = colors::BORDER_MED;
    warning_ = colors::WARNING;
    danger_ = colors::DANGER;
    success_ = colors::SUCCESS;
}

QSize SymbolRowDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const {
    return {option.rect.width(), row_height_};
}

void SymbolRowDelegate::paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    const auto* model = dynamic_cast<const SymbolResultModel*>(index.model());
    const SymbolRow* row = model ? model->row_at(index.row()) : nullptr;
    if (!row)
        return;
    const ci::Instrument& inst = row->instrument;
    const bool selected = option.state.testFlag(QStyle::State_Selected);
    const bool disabled = row->state == SymbolRowState::Disabled;

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    const QRect r = option.rect;
    const int bar = std::max(2, pad_h_ / 4);
    if (selected) {
        p->fillRect(r, accent_bg_);
        p->fillRect(QRect(r.left(), r.top(), bar, r.height()), disabled ? text3_ : accent_);
    }

    const QFontMetrics fs(symbol_font_);
    const QFontMetrics fm(meta_font_);
    const QFontMetrics ft(tag_font_);
    const int gap = std::max(4, fm.averageCharWidth());
    const QRect inner = r.adjusted(pad_h_ + bar, pad_v_, -pad_h_, -pad_v_);
    const QRect line1(inner.left(), inner.top(), inner.width(), fs.height());
    const QRect line2(inner.left(), line1.bottom() + 1 + std::max(1, fm.height() / 8), inner.width(), fm.height());

    const QColor c_symbol = disabled ? text3_ : text_;
    const QColor c_name = disabled ? text3_ : text2_;
    const QColor c_emph = disabled ? text3_ : accent_;

    // Class tag, right of line 1.
    int right = line1.right();
    const QString tag = spp_class_tag(inst.asset_class);
    if (!tag.isEmpty()) {
        const int tag_pad = std::max(3, ft.averageCharWidth() / 2 + 2);
        const int tw = spp_width(tag_font_, tag) + 2 * tag_pad;
        const int th = ft.height() + 2;
        const QRect tr(right - tw + 1, line1.top() + (line1.height() - th) / 2, tw, th);
        p->setPen(tag_border_);
        p->setBrush(tag_bg_);
        p->drawRoundedRect(QRectF(tr).adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
        p->setFont(tag_font_);
        p->setPen(disabled ? text3_ : text2_);
        p->drawText(tr, Qt::AlignCenter, tag);
        right = tr.left() - gap;
    }

    // Display symbol, the typed code emphasised.
    const QString display = inst.display.isEmpty() ? inst.symbol : inst.display;
    const QString cq = model->code_query();
    qsizetype from = -1;
    qsizetype len = 0;
    if (!cq.isEmpty() && display.startsWith(cq, Qt::CaseInsensitive)) {
        from = 0;
        len = cq.size();
    }
    const int sym_avail = std::max(0, right - line1.left());
    const int sym_w = std::min(sym_avail, spp_width(symbol_font_, display));
    const int used = spp_draw_emphasised(p, QRect(line1.left(), line1.top(), sym_w, line1.height()), symbol_font_,
                                         display, c_symbol, c_emph, from, len);

    // Venue and currency after the symbol.
    const QString meta = spp_meta(inst);
    const int meta_left = line1.left() + used + gap;
    if (!meta.isEmpty() && meta_left < right)
        spp_draw_emphasised(p, QRect(meta_left, line1.top(), right - meta_left, line1.height()), meta_font_, meta,
                            text3_, text3_, -1, 0);

    // Marker, right of line 2.
    int name_right = line2.right();
    if (!row->marker.isEmpty()) {
        QColor tone = text2_;
        switch (row->marker_tone) {
            case SymbolTone::Warning:
                tone = warning_;
                break;
            case SymbolTone::Danger:
                tone = danger_;
                break;
            case SymbolTone::Success:
                tone = success_;
                break;
            case SymbolTone::Busy:
                tone = text3_;
                break;
            case SymbolTone::Neutral:
                break;
        }
        const int mw = std::min(spp_width(meta_font_, row->marker), line2.width() * 3 / 5);
        const QRect mr(line2.right() - mw + 1, line2.top(), mw, line2.height());
        spp_draw_emphasised(p, mr, meta_font_, row->marker, tone, tone, -1, 0);
        name_right = mr.left() - gap;
    }

    // Name, the typed words emphasised.
    // A name that only repeats the display ("NIFTY 50") gives way to the market-data symbol (^NSEI).
    QString name = inst.name;
    if (name.isEmpty() || name.compare(display, Qt::CaseInsensitive) == 0)
        name = inst.symbol != display ? inst.symbol : name;
    const QString nq = model->name_query();
    from = -1;
    len = 0;
    if (nq.size() >= 2) {
        const qsizetype at = name.indexOf(nq, 0, Qt::CaseInsensitive);
        if (at >= 0) {
            from = at;
            len = nq.size();
        }
    }
    if (name_right > line2.left())
        spp_draw_emphasised(p, QRect(line2.left(), line2.top(), name_right - line2.left(), line2.height()), meta_font_,
                            name, c_name, c_emph, from, len);
    p->restore();
}

// ── Popup ───────────────────────────────────────────────────────────────────

SymbolPickerPopup::SymbolPickerPopup(QWidget* owner)
    : QFrame(owner, Qt::ToolTip | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint) {
    setObjectName(QStringLiteral("symbolPickerPopup"));
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::NoFocus);
    setAccessibleName(spp_tr("Matching instruments"));

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(1, 1, 1, 1);
    vl->setSpacing(0);

    status_ = new QLabel(this);
    status_->setObjectName(QStringLiteral("symbolPickerStatus"));
    status_->setWordWrap(true);
    status_->setFocusPolicy(Qt::NoFocus);
    status_->setTextFormat(Qt::PlainText);
    status_->hide();
    vl->addWidget(status_);

    view_ = new QListView(this);
    view_->setObjectName(QStringLiteral("symbolPickerList"));
    view_->setAccessibleName(spp_tr("Matching instruments"));
    view_->setFocusPolicy(Qt::NoFocus);
    view_->setUniformItemSizes(true);
    view_->setMouseTracking(true);
    view_->setSelectionMode(QAbstractItemView::SingleSelection);
    view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view_->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    view_->setFrameShape(QFrame::NoFrame);
    model_ = new SymbolResultModel(this);
    delegate_ = new SymbolRowDelegate(this);
    view_->setModel(model_);
    view_->setItemDelegate(delegate_);
    vl->addWidget(view_);

    connect(view_, &QAbstractItemView::clicked, this, [this](const QModelIndex& index) {
        if (on_clicked && index.isValid())
            on_clicked(index.row());
    });
    connect(view_, &QAbstractItemView::entered, this, [this](const QModelIndex& index) {
        if (index.isValid())
            set_current(index.row());
    });
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this,
            [this](const ThemeTokens&) { refresh_theme(); });
    connect(&ThemeManager::instance(), &ThemeManager::appearance_settled, this, [this]() {
        if (isVisible() && anchor_)
            place(anchor_);
    });
    apply_styles();
}

void SymbolPickerPopup::apply_styles() {
    const QString frame = QStringLiteral("QFrame#symbolPickerPopup { background: %1; border: 1px solid %2; }"
                                         "QListView#symbolPickerList { background: transparent; border: none; outline: 0; }")
                              .arg(QString::fromLatin1(colors::BG_SURFACE()), QString::fromLatin1(colors::BORDER_BRIGHT()));
    if (styleSheet() != frame)
        setStyleSheet(frame);

    const char* tone = colors::TEXT_SECONDARY();
    switch (status_tone_) {
        case SymbolTone::Busy:
            tone = colors::TEXT_TERTIARY();
            break;
        case SymbolTone::Warning:
            tone = colors::WARNING();
            break;
        case SymbolTone::Danger:
            tone = colors::DANGER();
            break;
        case SymbolTone::Success:
            tone = colors::SUCCESS();
            break;
        case SymbolTone::Neutral:
            break;
    }
    const QFontMetrics fm(fonts::font(-2));
    const QString status = QStringLiteral("QLabel#symbolPickerStatus { color: %1; background: %2; font-size: %3px;"
                                          " padding: %4px %5px; border-bottom: 1px solid %6; }")
                               .arg(QString::fromLatin1(tone), QString::fromLatin1(colors::BG_RAISED()), fonts::px(-2),
                                    QString::number(std::max(2, fm.height() / 5)),
                                    QString::number(std::max(6, fm.averageCharWidth())),
                                    QString::fromLatin1(colors::BORDER_DIM()));
    if (status_->styleSheet() != status)
        status_->setStyleSheet(status);
}

void SymbolPickerPopup::refresh_theme() {
    delegate_->refresh_theme();
    apply_styles();
    const int cur = current();
    // Row heights are cached by the view (uniform sizes): re-announce the rows.
    QVector<SymbolRow> rows;
    for (int i = 0; i < model_->rowCount(); ++i)
        rows.push_back(*model_->row_at(i));
    model_->set_rows(std::move(rows), model_->code_query(), model_->name_query());
    set_current(cur);
}

void SymbolPickerPopup::set_rows(QVector<SymbolRow> rows, const QString& code_query, const QString& name_query,
                                 int current) {
    model_->set_rows(std::move(rows), code_query, name_query);
    set_current(current);
}

void SymbolPickerPopup::set_status(const QString& text, SymbolTone tone) {
    const bool changed = text != status_text_ || tone != status_tone_;
    status_text_ = text;
    status_tone_ = tone;
    if (!changed)
        return;
    status_->setText(text);
    status_->setAccessibleName(text);
    status_->setVisible(!text.isEmpty());
    apply_styles();
}

int SymbolPickerPopup::count() const {
    return model_->rowCount();
}

int SymbolPickerPopup::current() const {
    const QModelIndex i = view_->currentIndex();
    return i.isValid() ? i.row() : -1;
}

void SymbolPickerPopup::set_current(int row) {
    if (row < 0 || row >= count()) {
        view_->setCurrentIndex({});
        view_->clearSelection();
        return;
    }
    const QModelIndex i = model_->index(row, 0);
    view_->setCurrentIndex(i);
    view_->selectionModel()->select(i, QItemSelectionModel::ClearAndSelect);
    view_->scrollTo(i, QAbstractItemView::EnsureVisible);
}

void SymbolPickerPopup::move_current(int delta) {
    const int n = count();
    if (n <= 0)
        return;
    const int cur = current();
    const int next = cur < 0 ? (delta > 0 ? 0 : n - 1) : std::clamp(cur + delta, 0, n - 1);
    set_current(next);
}

const SymbolRow* SymbolPickerPopup::row_at(int row) const {
    return model_->row_at(row);
}

const SymbolRow* SymbolPickerPopup::current_row() const {
    return model_->row_at(current());
}

QString SymbolPickerPopup::code_query() const {
    return model_->code_query();
}

void SymbolPickerPopup::place(QWidget* anchor) {
    if (!anchor)
        return;
    anchor_ = anchor;
    const int rows = std::min(count(), kMaxVisibleRows);
    const QFontMetrics fm(fonts::font(-2));
    const int width = std::max(anchor->width(), fm.averageCharWidth() * 58);
    const int list_h = rows * delegate_->row_height();
    view_->setFixedHeight(list_h);
    view_->setVisible(rows > 0);
    int status_h = 0;
    if (status_->isVisibleTo(this)) {
        status_->setFixedWidth(width - 2);
        status_h = status_->heightForWidth(width - 2);
        if (status_h <= 0)
            status_h = status_->sizeHint().height();
    }
    const int height = list_h + status_h + 2;
    if (rows == 0 && status_h == 0) {
        hide();
        return;
    }
    QPoint pos = anchor->mapToGlobal(QPoint(0, anchor->height()));
    const QScreen* screen = anchor->screen() ? anchor->screen() : QGuiApplication::primaryScreen();
    if (screen) {
        const QRect avail = screen->availableGeometry();
        if (pos.y() + height > avail.bottom() && anchor->mapToGlobal(QPoint(0, 0)).y() - height >= avail.top())
            pos.setY(anchor->mapToGlobal(QPoint(0, 0)).y() - height); // no room below: open upwards
        pos.setX(std::clamp(pos.x(), avail.left(), std::max(avail.left(), avail.right() - width + 1)));
    }
    setFixedSize(width, height);
    move(pos);
    if (!isVisible())
        show(); // a tool-tip window is shown on top of its owner's window
}

} // namespace fincept::ui
