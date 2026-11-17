// SearchPopup.cpp — see SearchPopup.h.
#include "ui/navigation/search/SearchPopup.h"

#include "ui/navigation/search/SearchDelegate.h"
#include "ui/navigation/search/SearchModel.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QFontMetrics>
#include <QItemSelectionModel>
#include <QLabel>
#include <QListView>
#include <QMouseEvent>
#include <QScreen>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::ui::search {

SearchPopup::SearchPopup(SearchModel* model, QWidget* anchor)
    : QFrame(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint | Qt::WindowDoesNotAcceptFocus),
      model_(model), anchor_(anchor) {
    setObjectName(QStringLiteral("cmdSearchPopup"));
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setFocusPolicy(Qt::NoFocus);
    setAccessibleName(tr("Search results"));
    setAccessibleDescription(tr("Use the up and down arrows to move, Enter to open, Tab for more actions, Escape to close."));
    refit(); // stylesheet before children exist

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(1, 1, 1, 1);
    vl->setSpacing(0);

    view_ = new QListView(this);
    view_->setObjectName(QStringLiteral("cmdSearchList"));
    view_->setModel(model_);
    delegate_ = new SearchDelegate(view_);
    view_->setItemDelegate(delegate_);
    view_->setFocusPolicy(Qt::NoFocus);
    view_->setSelectionMode(QAbstractItemView::SingleSelection);
    view_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    view_->setUniformItemSizes(false);
    view_->setMouseTracking(true);
    view_->viewport()->setAttribute(Qt::WA_Hover, true); // hover row + its action buttons
    view_->setTextElideMode(Qt::ElideRight);
    view_->setAccessibleName(tr("Search results"));
    view_->viewport()->installEventFilter(this);
    vl->addWidget(view_, 1);

    legend_ = new QLabel(this);
    legend_->setObjectName(QStringLiteral("cmdSearchLegend"));
    legend_->setTextFormat(Qt::PlainText);
    legend_->setWordWrap(false);
    vl->addWidget(legend_);

    connect(view_, &QListView::entered, this, [this](const QModelIndex& idx) { emit row_hovered(idx.row()); });
}

SearchPopup::~SearchPopup() = default;

void SearchPopup::refit() {
    const QString css =
        QStringLiteral("QFrame#cmdSearchPopup{background:%1;border:1px solid %2;}"
                       "QListView#cmdSearchList{background:transparent;border:none;outline:none;}"
                       "QLabel#cmdSearchLegend{color:%3;background:%4;border:none;border-top:1px solid %5;"
                       "padding:2px 8px;font-size:%6px;}")
            .arg(colors::BG_SURFACE(), colors::BORDER_BRIGHT(), colors::TEXT_TERTIARY(), colors::BG_BASE(),
                 colors::BORDER_DIM())
            .arg(fonts::px(-4));
    if (css != applied_css_) {
        applied_css_ = css;
        setStyleSheet(css);
    }
    if (view_) {
        view_->doItemsLayout(); // row heights follow the font
        if (isVisible())
            place();
    }
}

int SearchPopup::current() const {
    const QModelIndex i = view_->currentIndex();
    return i.isValid() ? i.row() : -1;
}

void SearchPopup::set_current(int row) {
    if (row < 0 || row >= model_->rowCount()) {
        view_->setCurrentIndex({});
        view_->clearSelection();
        return;
    }
    const QModelIndex idx = model_->index(row);
    view_->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect);
    view_->scrollTo(idx, QAbstractItemView::EnsureVisible);
}

void SearchPopup::set_legend(const QString& text) {
    legend_text_ = text;
    if (legend_)
        legend_->setText(text);
}

void SearchPopup::show_status(const QString& text) {
    if (!legend_)
        return;
    legend_->setText(text);
    const int serial = ++status_serial_;
    QTimer::singleShot(4000, this, [this, serial]() {
        if (serial == status_serial_)
            legend_->setText(legend_text_);
    });
}

int SearchPopup::content_height() const {
    int h = 0;
    const int rows = model_->rowCount();
    for (int i = 0; i < rows; ++i)
        h += view_->sizeHintForRow(i);
    h += legend_->sizeHint().height() + 2;
    return h;
}

void SearchPopup::place() {
    if (!anchor_)
        return;
    QWidget* win = anchor_->window();
    const QRect win_rect = win ? QRect(win->mapToGlobal(QPoint(0, 0)), win->size()) : QRect();
    QScreen* scr = anchor_->screen();
    const QRect avail = scr ? scr->availableGeometry() : win_rect;
    const QFontMetrics fm(fonts::font(0));
    // Wide enough for two-line rows with action buttons; never wider than the window.
    const int want_w = std::max(anchor_->width(), fm.averageCharWidth() * 74);
    const int max_w = std::max(anchor_->width(), (win_rect.isValid() ? win_rect.width() : avail.width()) - 16);
    const int w = std::min(want_w, max_w);
    const QPoint below = anchor_->mapToGlobal(QPoint(0, anchor_->height() + 1));
    int x = below.x();
    if (layoutDirection() == Qt::RightToLeft || anchor_->layoutDirection() == Qt::RightToLeft)
        x = anchor_->mapToGlobal(QPoint(anchor_->width(), 0)).x() - w;
    const int right_limit = (win_rect.isValid() ? std::min(win_rect.right(), avail.right()) : avail.right()) - 4;
    if (x + w > right_limit)
        x = std::max(avail.left(), right_limit - w);
    const int bottom_limit = (win_rect.isValid() ? std::min(win_rect.bottom(), avail.bottom()) : avail.bottom()) - 8;
    const int max_h = std::max(fm.height() * 6, std::min(bottom_limit - below.y(),
                                                         (win_rect.isValid() ? win_rect.height() : avail.height()) * 7 / 10));
    const int h = std::clamp(content_height(), fm.height() * 3, max_h);
    setGeometry(x, below.y(), w, h);
}

void SearchPopup::present() {
    // Follow the anchor's CURRENT window (the header is built before it joins its frame).
    QWidget* win = anchor_ ? anchor_->window() : nullptr;
    if (win != watched_window_.data()) {
        if (watched_window_)
            watched_window_->removeEventFilter(this);
        watched_window_ = win;
        if (win)
            win->installEventFilter(this);
    }
    place();
    if (!isVisible()) {
        show();
        raise();
    }
}

void SearchPopup::dismiss() {
    if (isVisible())
        hide();
}

bool SearchPopup::eventFilter(QObject* watched, QEvent* event) {
    if (watched == view_->viewport()) {
        switch (event->type()) {
            case QEvent::MouseButtonRelease: {
                auto* me = static_cast<QMouseEvent*>(event);
                const QModelIndex idx = view_->indexAt(me->position().toPoint());
                if (!idx.isValid())
                    break;
                if (me->button() == Qt::LeftButton) {
                    QStyleOptionViewItem opt;
                    opt.initFrom(view_);
                    opt.rect = view_->visualRect(idx);
                    opt.state |= QStyle::State_MouseOver;
                    opt.direction = view_->layoutDirection();
                    const int action = delegate_->action_at(opt, idx, me->position().toPoint());
                    emit row_activated(idx.row(), action, me->modifiers());
                    return true;
                }
                if (me->button() == Qt::RightButton) {
                    emit row_context_menu(idx.row(), me->globalPosition().toPoint());
                    return true;
                }
                break;
            }
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonDblClick:
                return true; // selection follows hover / keys; activation on release
            case QEvent::ContextMenu:
                return true;
            default:
                break;
        }
        return false;
    }
    // The anchor's window moved / resized / hid: follow it, or close.
    if (watched_window_ && watched == watched_window_.data() && isVisible()) {
        switch (event->type()) {
            case QEvent::Move:
            case QEvent::Resize:
                QTimer::singleShot(0, this, [this]() {
                    if (isVisible())
                        place();
                });
                break;
            case QEvent::Hide:
            case QEvent::WindowStateChange:
            case QEvent::WindowDeactivate:
                QTimer::singleShot(0, this, [this]() {
                    // Deactivation by a click on this popup does not happen (it never takes focus);
                    // anything else that deactivates the window closes the results.
                    if (anchor_ && anchor_->window() && !anchor_->window()->isActiveWindow())
                        dismiss();
                });
                break;
            default:
                break;
        }
    }
    return false;
}

} // namespace fincept::ui::search
