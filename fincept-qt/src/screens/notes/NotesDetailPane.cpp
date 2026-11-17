// src/screens/notes/NotesDetailPane.cpp
#include "screens/notes/NotesDetailPane.h"

#include "screens/notes/NotesEditor.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/PanelHeader.h"
#include "ui/widgets/report/AddToReportButton.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDateTime>
#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens::notes {

namespace colors = fincept::ui::colors;
namespace fonts = fincept::ui::fonts;
namespace nb = fincept::notes_board;

namespace {

QColor ndp_token(const fincept::ui::ColorToken& t) {
    return QColor(t.get());
}

QString ndp_escape(QString s) {
    return s.replace(QLatin1Char('&'), QStringLiteral("&&"));
}

const QDate kNoReview(2000, 1, 1); // the date edit's "no review date" value

} // namespace

// ── Quote strip ──────────────────────────────────────────────────────────────

NotesQuoteStrip::NotesQuoteStrip(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("notesQuoteStrip"));
    setMouseTracking(true);
    QSizePolicy sp(QSizePolicy::Expanding, QSizePolicy::Preferred);
    sp.setHeightForWidth(true);
    setSizePolicy(sp);
}

void NotesQuoteStrip::set_symbols(const QStringList& symbols) {
    if (symbols == symbols_)
        return;
    symbols_ = symbols;
    hover_ = -1;
    updateGeometry();
    update();
}

void NotesQuoteStrip::set_empty_text(const QString& text) {
    empty_text_ = text;
    update();
}

int NotesQuoteStrip::cell_width() const {
    const QFontMetrics fm(fonts::font(-3, QFont::DemiBold));
    return fm.averageCharWidth() * 26 + 16;
}

int NotesQuoteStrip::row_height() const {
    const QFontMetrics fm(fonts::font(-3, QFont::DemiBold));
    return fm.height() + 8;
}

QVector<NotesQuoteStrip::Cell> NotesQuoteStrip::layout_cells(int width) const {
    QVector<Cell> out;
    const int cw = cell_width();
    const int rh = row_height();
    const int cols = std::max(1, (width - 16) / cw);
    const int shown = std::min(static_cast<int>(symbols_.size()), cols * 2); // two rows at most
    const int w = (width - 16) / std::max(1, std::min(cols, shown));
    for (int i = 0; i < shown; ++i)
        out << Cell{symbols_.at(i), QRect(8 + (i % cols) * w, 4 + (i / cols) * rh, w - 6, rh - 2)};
    return out;
}

int NotesQuoteStrip::heightForWidth(int w) const {
    const int rh = row_height();
    if (symbols_.isEmpty())
        return rh + 8;
    const int cols = std::max(1, (w - 16) / cell_width());
    const int rows = std::min(2, (static_cast<int>(symbols_.size()) + cols - 1) / cols);
    return rows * rh + 8;
}

QSize NotesQuoteStrip::sizeHint() const {
    return {cell_width() * 2, heightForWidth(width() > 0 ? width() : cell_width() * 2)};
}

QSize NotesQuoteStrip::minimumSizeHint() const {
    return {cell_width(), row_height() + 8};
}

void NotesQuoteStrip::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), ndp_token(colors::BG_SURFACE));
    p.setPen(ndp_token(colors::BORDER_DIM));
    p.drawLine(0, height() - 1, width(), height() - 1);
    const QFont f = fonts::font(-3, QFont::DemiBold);
    const QFontMetrics fm(f);
    p.setFont(f);
    if (symbols_.isEmpty()) {
        p.setPen(ndp_token(colors::TEXT_TERTIARY));
        p.drawText(rect().adjusted(10, 0, -10, 0), Qt::AlignLeft | Qt::AlignVCenter, empty_text_);
        return;
    }
    const QVector<Cell> cells = layout_cells(width());
    for (int i = 0; i < cells.size(); ++i) {
        const Cell& c = cells.at(i);
        if (i == hover_)
            p.fillRect(c.rect, ndp_token(colors::BG_HOVER));
        int x = c.rect.left() + 4;
        const int base = c.rect.top() + (c.rect.height() - fm.height()) / 2 + fm.ascent();
        const QString sym = QLatin1Char('$') + c.symbol;
        p.setPen(ndp_token(colors::AMBER));
        p.drawText(QPoint(x, base), sym);
        x += fm.horizontalAdvance(sym) + fm.averageCharWidth();
        const CardQuote* q = quotes_ ? quotes_->find(c.symbol) : nullptr;
        if (!q || q->price <= 0) {
            p.setPen(ndp_token(colors::TEXT_TERTIARY));
            p.drawText(QPoint(x, base), QStringLiteral("--"));
            continue;
        }
        const QString last = fincept::ui::formatting::price(q->price);
        p.setPen(ndp_token(colors::TEXT_PRIMARY));
        p.drawText(QPoint(x, base), last);
        x += fm.horizontalAdvance(last) + fm.averageCharWidth();
        if (q->has_change) {
            const QString chg = fincept::ui::formatting::pct(q->change_pct, 2, true);
            p.setPen(q->change_pct > 0   ? ndp_token(colors::POSITIVE)
                     : q->change_pct < 0 ? ndp_token(colors::NEGATIVE)
                                         : ndp_token(colors::TEXT_SECONDARY));
            p.drawText(QPoint(x, base), fm.elidedText(chg, Qt::ElideRight, std::max(0, c.rect.right() - x)));
        }
    }
    if (cells.size() < symbols_.size()) {
        p.setPen(ndp_token(colors::TEXT_TERTIARY));
        p.drawText(rect().adjusted(0, 0, -8, -4), Qt::AlignRight | Qt::AlignBottom,
                   QStringLiteral("+%1").arg(symbols_.size() - cells.size()));
    }
}

void NotesQuoteStrip::mouseMoveEvent(QMouseEvent* e) {
    const QVector<Cell> cells = layout_cells(width());
    int h = -1;
    for (int i = 0; i < cells.size(); ++i)
        if (cells.at(i).rect.contains(e->position().toPoint()))
            h = i;
    if (h != hover_) {
        hover_ = h;
        setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
}

void NotesQuoteStrip::leaveEvent(QEvent*) {
    if (hover_ >= 0) {
        hover_ = -1;
        update();
    }
}

void NotesQuoteStrip::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton)
        return;
    for (const Cell& c : layout_cells(width()))
        if (c.rect.contains(e->position().toPoint())) {
            emit symbol_clicked(c.symbol);
            return;
        }
}

// ── Pane ─────────────────────────────────────────────────────────────────────

NotesDetailPane::NotesDetailPane(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("notesDetail"));
    setAttribute(Qt::WA_StyledBackground, true);
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    stack_ = new QStackedWidget(this);
    root->addWidget(stack_);
    note_page_ = build_note_page();
    stack_->addWidget(note_page_);
    bulk_page_ = build_bulk_page();
    stack_->addWidget(bulk_page_);
    empty_page_ = build_empty_page();
    stack_->addWidget(empty_page_);
    stack_->setCurrentWidget(empty_page_);
    fill_status_combo();
    retranslate();
    refit();
}

QWidget* NotesDetailPane::build_note_page() {
    auto* page = new QWidget(stack_);
    page->setObjectName(QStringLiteral("notesDetailPage"));
    auto* lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    header_ = new fincept::ui::PanelHeader(QString(), page);
    focus_btn_ = new QToolButton(header_);
    focus_btn_->setObjectName(QStringLiteral("notesHeaderBtn"));
    focus_btn_->setCursor(Qt::PointingHandCursor);
    connect(focus_btn_, &QToolButton::clicked, this, &NotesDetailPane::focus_toggled);
    header_->add_control(focus_btn_);
    hide_btn_ = new QToolButton(header_);
    hide_btn_->setObjectName(QStringLiteral("notesHeaderBtn"));
    hide_btn_->setCursor(Qt::PointingHandCursor);
    hide_btn_->setText(QStringLiteral("×"));
    connect(hide_btn_, &QToolButton::clicked, this, &NotesDetailPane::hide_requested);
    header_->add_control(hide_btn_);
    lay->addWidget(header_);

    // Board strip: status, review date, mark reviewed, Chat link.
    auto* strip = new QWidget(page);
    strip->setObjectName(QStringLiteral("notesBoardStrip"));
    strip->setAttribute(Qt::WA_StyledBackground, true);
    auto* sl = new QGridLayout(strip);
    sl->setContentsMargins(12, 6, 10, 6);
    sl->setHorizontalSpacing(8);
    sl->setVerticalSpacing(4);
    cap_status_ = new QLabel(strip);
    cap_status_->setObjectName(QStringLiteral("notesCaption"));
    sl->addWidget(cap_status_, 0, 0);
    status_ = new QComboBox(strip);
    status_->setObjectName(QStringLiteral("notesMetaCombo"));
    status_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    connect(status_, &QComboBox::activated, this, [this](int i) {
        if (!loading_ && i >= 0)
            emit status_changed(status_->itemData(i).toString());
    });
    sl->addWidget(status_, 0, 1);
    cap_review_ = new QLabel(strip);
    cap_review_->setObjectName(QStringLiteral("notesCaption"));
    sl->addWidget(cap_review_, 0, 2);
    auto* review_box = new QWidget(strip);
    auto* rl = new QHBoxLayout(review_box);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(2);
    review_ = new QDateEdit(review_box);
    review_->setObjectName(QStringLiteral("notesDateEdit"));
    review_->setCalendarPopup(true);
    review_->setMinimumDate(kNoReview);
    review_->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    review_->setDate(kNoReview);
    connect(review_, &QDateEdit::dateChanged, this, [this](const QDate& d) {
        if (!loading_)
            emit review_changed(d == kNoReview ? QDate() : d);
    });
    rl->addWidget(review_);
    review_menu_btn_ = new QToolButton(review_box);
    review_menu_btn_->setObjectName(QStringLiteral("notesHeaderBtn"));
    review_menu_btn_->setPopupMode(QToolButton::InstantPopup);
    review_menu_btn_->setText(QStringLiteral("▾"));
    review_menu_btn_->setCursor(Qt::PointingHandCursor);
    auto* rmenu = new QMenu(review_menu_btn_);
    connect(rmenu, &QMenu::aboutToShow, this, [this, rmenu]() {
        rmenu->clear();
        const QDate today = QDate::currentDate();
        const auto add = [this, rmenu](const QString& text, const QDate& d) {
            rmenu->addAction(text, this, [this, d]() {
                loading_ = true;
                review_->setDate(d.isValid() ? d : kNoReview);
                loading_ = false;
                emit review_changed(d);
            });
        };
        add(tr("Tomorrow"), today.addDays(1));
        add(tr("In 3 days"), today.addDays(3));
        add(tr("Next week"), today.addDays(7));
        add(tr("In 2 weeks"), today.addDays(14));
        add(tr("Next month"), today.addMonths(1));
        add(tr("After next quarter's earnings (90 days)"), today.addDays(90));
        rmenu->addSeparator();
        add(tr("No review date"), QDate());
    });
    review_menu_btn_->setMenu(rmenu);
    rl->addWidget(review_menu_btn_);
    sl->addWidget(review_box, 0, 3);
    reviewed_btn_ = new QPushButton(strip);
    reviewed_btn_->setObjectName(QStringLiteral("notesSmallBtn"));
    reviewed_btn_->setCursor(Qt::PointingHandCursor);
    connect(reviewed_btn_, &QPushButton::clicked, this, &NotesDetailPane::mark_reviewed);
    sl->addWidget(reviewed_btn_, 0, 4);
    sl->setColumnStretch(5, 1);
    linked_ = new QLabel(strip);
    linked_->setObjectName(QStringLiteral("notesLinked"));
    linked_->setTextFormat(Qt::PlainText);
    linked_->setVisible(false);
    sl->addWidget(linked_, 1, 0, 1, 4);
    open_chat_btn_ = new QPushButton(strip);
    open_chat_btn_->setObjectName(QStringLiteral("notesSmallBtn"));
    open_chat_btn_->setCursor(Qt::PointingHandCursor);
    open_chat_btn_->setVisible(false);
    connect(open_chat_btn_, &QPushButton::clicked, this, &NotesDetailPane::open_chat_run);
    sl->addWidget(open_chat_btn_, 1, 4);
    lay->addWidget(strip);

    // Rename offer (hidden until the resolver reports a renamed symbol).
    rename_box_ = new QFrame(page);
    rename_box_->setObjectName(QStringLiteral("notesRenameBox"));
    auto* nl = new QHBoxLayout(rename_box_);
    nl->setContentsMargins(12, 5, 10, 5);
    nl->setSpacing(8);
    rename_text_ = new QLabel(rename_box_);
    rename_text_->setObjectName(QStringLiteral("notesRenameText"));
    rename_text_->setWordWrap(true);
    nl->addWidget(rename_text_, 1);
    rename_yes_ = new QPushButton(rename_box_);
    rename_yes_->setObjectName(QStringLiteral("notesSmallBtn"));
    rename_yes_->setCursor(Qt::PointingHandCursor);
    connect(rename_yes_, &QPushButton::clicked, this, [this]() {
        const QString f = rename_from_, t = rename_to_;
        show_rename_offer({}, {});
        emit rename_accepted(f, t);
    });
    nl->addWidget(rename_yes_);
    rename_no_ = new QPushButton(rename_box_);
    rename_no_->setObjectName(QStringLiteral("notesSmallBtn"));
    rename_no_->setCursor(Qt::PointingHandCursor);
    connect(rename_no_, &QPushButton::clicked, this, [this]() {
        const QString f = rename_from_;
        show_rename_offer({}, {});
        emit rename_dismissed(f);
    });
    nl->addWidget(rename_no_);
    rename_box_->setVisible(false);
    lay->addWidget(rename_box_);

    quotes_ = new NotesQuoteStrip(page);
    connect(quotes_, &NotesQuoteStrip::symbol_clicked, this, &NotesDetailPane::symbol_clicked);
    lay->addWidget(quotes_);

    // Linked actions for the first ticker + Add to Report.
    actions_row_ = new QWidget(page);
    actions_row_->setObjectName(QStringLiteral("notesActionsRow"));
    actions_row_->setAttribute(Qt::WA_StyledBackground, true);
    auto* al = new QHBoxLayout(actions_row_);
    al->setContentsMargins(12, 4, 10, 4);
    al->setSpacing(4);
    const char* const targets[] = {"equity_research", "equity_trading", "news", "watchlist", "backtesting"};
    for (const char* t : targets) {
        auto* b = new QPushButton(actions_row_);
        b->setObjectName(QStringLiteral("notesSmallBtn"));
        b->setCursor(Qt::PointingHandCursor);
        const QString id = QString::fromLatin1(t);
        connect(b, &QPushButton::clicked, this, [this, id]() {
            if (!primary_symbol_.isEmpty())
                emit linked_action(id, primary_symbol_);
        });
        al->addWidget(b);
        action_btns_ << b;
    }
    al->addStretch(1);
    report_ = new fincept::ui::AddToReportButton(QStringLiteral("Notes"), actions_row_);
    al->addWidget(report_);
    lay->addWidget(actions_row_);

    editor_ = new NotesEditor(page);
    lay->addWidget(editor_, 1);

    history_header_ = new fincept::ui::PanelHeader(QString(), page);
    history_toggle_ = new QToolButton(history_header_);
    history_toggle_->setObjectName(QStringLiteral("notesHeaderBtn"));
    history_toggle_->setCursor(Qt::PointingHandCursor);
    connect(history_toggle_, &QToolButton::clicked, this, [this]() {
        history_open_ = !history_open_;
        history_view_->setVisible(history_open_);
        retranslate();
    });
    history_header_->add_control(history_toggle_);
    lay->addWidget(history_header_);
    history_model_ = new QStandardItemModel(this);
    history_view_ = new QListView(page);
    history_view_->setObjectName(QStringLiteral("notesHistory"));
    history_view_->setModel(history_model_);
    history_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    history_view_->setSelectionMode(QAbstractItemView::NoSelection);
    history_view_->setFrameShape(QFrame::NoFrame);
    history_view_->setUniformItemSizes(true);
    history_view_->setVisible(false);
    lay->addWidget(history_view_);
    return page;
}

QWidget* NotesDetailPane::build_bulk_page() {
    auto* page = new QWidget(stack_);
    page->setObjectName(QStringLiteral("notesDetailPage"));
    auto* lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    auto* header = new fincept::ui::PanelHeader(tr("SELECTION"), page);
    lay->addWidget(header);
    auto* body = new QWidget(page);
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(18, 18, 18, 18);
    bl->setSpacing(10);
    bulk_title_ = new QLabel(body);
    bulk_title_->setObjectName(QStringLiteral("notesBulkTitle"));
    bl->addWidget(bulk_title_);
    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(8);
    struct B {
        const char* action;
        const char* role;
    };
    const B buttons[] = {{"pin", "notesSmallBtn"},      {"status", "notesSmallBtn"},    {"tag", "notesSmallBtn"},
                         {"review", "notesSmallBtn"},   {"archive", "notesSmallBtn"},   {"export_fm", "notesSmallBtn"},
                         {"export_dir", "notesSmallBtn"}, {"delete", "notesDangerBtn"}};
    int i = 0;
    for (const B& b : buttons) {
        auto* btn = new QPushButton(body);
        btn->setObjectName(QString::fromLatin1(b.role));
        btn->setCursor(Qt::PointingHandCursor);
        const QString action = QString::fromLatin1(b.action);
        if (action == QLatin1String("status") || action == QLatin1String("review")) {
            auto* menu = new QMenu(btn);
            connect(menu, &QMenu::aboutToShow, this, [this, menu, action]() {
                menu->clear();
                if (action == QLatin1String("status")) {
                    for (const QString& code : nb::status_codes())
                        menu->addAction(status_label(code), this,
                                        [this, code]() { emit bulk_action(QStringLiteral("status:") + code); });
                } else {
                    menu->addAction(tr("Tomorrow"), this, [this]() { emit bulk_action(QStringLiteral("review:1")); });
                    menu->addAction(tr("Next week"), this, [this]() { emit bulk_action(QStringLiteral("review:7")); });
                    menu->addAction(tr("Next month"), this, [this]() { emit bulk_action(QStringLiteral("review:30")); });
                    menu->addSeparator();
                    menu->addAction(tr("No review date"), this,
                                    [this]() { emit bulk_action(QStringLiteral("review:clear")); });
                }
            });
            btn->setMenu(menu);
        } else {
            connect(btn, &QPushButton::clicked, this, [this, action]() {
                if (action == QLatin1String("pin"))
                    emit bulk_action(bulk_any_unpinned_ ? QStringLiteral("pin") : QStringLiteral("unpin"));
                else if (action == QLatin1String("archive"))
                    emit bulk_action(bulk_all_archived_ ? QStringLiteral("unarchive") : QStringLiteral("archive"));
                else
                    emit bulk_action(action);
            });
        }
        grid->addWidget(btn, i / 2, i % 2);
        bulk_btns_ << btn;
        ++i;
    }
    bl->addLayout(grid);
    bulk_hint_ = new QLabel(body);
    bulk_hint_->setObjectName(QStringLiteral("notesHint"));
    bulk_hint_->setWordWrap(true);
    bl->addWidget(bulk_hint_);
    bl->addStretch(1);
    lay->addWidget(body, 1);
    return page;
}

QWidget* NotesDetailPane::build_empty_page() {
    auto* page = new QWidget(stack_);
    page->setObjectName(QStringLiteral("notesDetailPage"));
    auto* lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    auto* header = new fincept::ui::PanelHeader(tr("NOTE"), page);
    lay->addWidget(header);
    empty_ = new fincept::ui::EmptyState(page);
    empty_->set_compact(true);
    connect(empty_, &fincept::ui::EmptyState::primary_clicked, this, [this]() {
        if (!empty_notebook_ && empty_last_id_ > 0)
            emit continue_requested(empty_last_id_);
        else
            emit new_requested();
    });
    connect(empty_, &fincept::ui::EmptyState::secondary_clicked, this, [this]() {
        if (!empty_notebook_ && empty_last_id_ > 0)
            emit new_requested();
        else
            emit import_requested();
    });
    lay->addWidget(empty_, 1);
    auto* tpl = new QWidget(page);
    auto* tl = new QVBoxLayout(tpl);
    tl->setContentsMargins(18, 0, 18, 18);
    tl->setSpacing(8);
    empty_or_ = new QLabel(tpl);
    empty_or_->setObjectName(QStringLiteral("notesHint"));
    empty_or_->setAlignment(Qt::AlignHCenter);
    tl->addWidget(empty_or_);
    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(8);
    int i = 0;
    for (const auto& t : builtin_templates()) {
        auto* b = new QPushButton(tpl);
        b->setObjectName(QStringLiteral("notesSmallBtn"));
        b->setCursor(Qt::PointingHandCursor);
        b->setProperty("template_id", t.id);
        const QString id = t.id;
        connect(b, &QPushButton::clicked, this, [this, id]() { emit template_requested(id); });
        grid->addWidget(b, i / 2, i % 2);
        template_btns_ << b;
        ++i;
    }
    tl->addLayout(grid);
    empty_keys_ = new QLabel(tpl);
    empty_keys_->setObjectName(QStringLiteral("notesHint"));
    empty_keys_->setWordWrap(true);
    empty_keys_->setAlignment(Qt::AlignHCenter);
    tl->addWidget(empty_keys_);
    lay->addWidget(tpl);
    return page;
}

void NotesDetailPane::show_page(Page p) {
    page_ = p;
    QWidget* w = p == Page::Note ? note_page_ : p == Page::Bulk ? bulk_page_ : empty_page_;
    if (stack_->currentWidget() != w)
        stack_->setCurrentWidget(w);
}

void NotesDetailPane::fill_status_combo() {
    const QString cur = status_->currentData().toString();
    loading_ = true;
    status_->clear();
    for (const QString& code : nb::status_codes())
        status_->addItem(status_label(code), code);
    const int i = status_->findData(cur);
    status_->setCurrentIndex(i >= 0 ? i : 0);
    loading_ = false;
}

void NotesDetailPane::set_note_meta(const NoteEntry& e, bool archived) {
    loading_ = true;
    const int i = status_->findData(e.meta.status);
    status_->setCurrentIndex(i >= 0 ? i : 0);
    review_->setDate(e.review.isValid() ? e.review : kNoReview);
    loading_ = false;
    linked_on_ = e.linked;
    linked_ref_ = e.meta.origin_ref.isEmpty() && e.linked ? chat_run_ref_from_body(e.note.content) : e.meta.origin_ref;
    header_->set_title(archived ? tr("NOTE · ARCHIVED") : tr("NOTE · %1").arg(status_label(e.meta.status)));
    update_linked();
}

void NotesDetailPane::set_new_note(const QString& status) {
    loading_ = true;
    const int i = status_->findData(status);
    status_->setCurrentIndex(i >= 0 ? i : 0);
    review_->setDate(kNoReview);
    loading_ = false;
    linked_on_ = false;
    linked_ref_.clear();
    header_->set_title(tr("NEW NOTE · %1").arg(status_label(status)));
    history_model_->clear();
    update_linked();
    show_rename_offer({}, {});
}

void NotesDetailPane::update_linked() {
    if (linked_on_)
        linked_->setText(linked_ref_.isEmpty() ? tr("⇄ Saved from a Chat Mode run")
                                               : tr("⇄ Saved from Chat Mode run %1").arg(linked_ref_.left(8)));
    if (linked_->isVisibleTo(this) != linked_on_)
        linked_->setVisible(linked_on_);
    if (open_chat_btn_->isVisibleTo(this) != linked_on_)
        open_chat_btn_->setVisible(linked_on_);
}

void NotesDetailPane::set_tickers(const QStringList& symbols) {
    quotes_->set_symbols(symbols);
    primary_symbol_ = symbols.value(0);
    for (QPushButton* b : action_btns_)
        b->setEnabled(!primary_symbol_.isEmpty());
    retranslate();
}

void NotesDetailPane::set_history(const QVector<nb::NoteBoardEvent>& events, const QString& created_at,
                                  const QString& updated_at) {
    history_model_->clear();
    const auto add = [this](qint64 ms, const QString& text) {
        const QString when = ms > 0 ? QDateTime::fromMSecsSinceEpoch(ms).toLocalTime().toString(QStringLiteral(
                                          "yyyy-MM-dd HH:mm"))
                                    : QString();
        // No cached colour: the list's stylesheet colours the text (theme changes follow).
        history_model_->appendRow(new QStandardItem(when.isEmpty() ? text : when + QStringLiteral("   ") + text));
    };
    const QDateTime up = parse_db_time(updated_at);
    const QDateTime cr = parse_db_time(created_at);
    if (up.isValid() && up != cr)
        add(up.toMSecsSinceEpoch(), tr("Last edited"));
    for (const auto& ev : events) {
        if (ev.kind == QLatin1String("status"))
            add(ev.at_ms, tr("Status %1 → %2").arg(status_label(ev.from_value), status_label(ev.to_value)));
        else if (ev.kind == QLatin1String("review"))
            add(ev.at_ms, ev.to_value.isEmpty() ? tr("Review date cleared") : tr("Review set for %1").arg(ev.to_value));
        else if (ev.kind == QLatin1String("reviewed"))
            add(ev.at_ms, tr("Marked reviewed"));
        else if (ev.kind == QLatin1String("archived"))
            add(ev.at_ms, ev.to_value == QLatin1String("1") ? tr("Archived") : tr("Restored from the archive"));
        else
            add(ev.at_ms, ev.kind);
    }
    if (cr.isValid())
        add(cr.toMSecsSinceEpoch(), tr("Created"));
    retranslate();
}

void NotesDetailPane::show_rename_offer(const QString& from, const QString& to) {
    rename_from_ = from;
    rename_to_ = to;
    const bool show = !from.isEmpty() && !to.isEmpty();
    if (show) {
        rename_text_->setText(tr("$%1 is now $%2 (renamed listing). Replace the ticker on this note? "
                                 "Text that mentions $%1 stays as written.")
                                  .arg(from, to));
        rename_yes_->setText(ndp_escape(tr("REPLACE WITH $%1").arg(to)));
        rename_no_->setText(tr("KEEP $%1").arg(from));
    }
    if (rename_box_->isVisibleTo(this) != show)
        rename_box_->setVisible(show);
}

void NotesDetailPane::set_focus_mode(bool on) {
    focus_mode_ = on;
    retranslate();
}

void NotesDetailPane::set_bulk(int count, bool any_unpinned, bool all_archived) {
    bulk_count_ = count;
    bulk_any_unpinned_ = any_unpinned;
    bulk_all_archived_ = all_archived;
    retranslate();
}

void NotesDetailPane::set_empty(bool notebook_empty, int last_id, const QString& last_title) {
    empty_notebook_ = notebook_empty;
    empty_last_id_ = last_id;
    empty_last_title_ = last_title;
    retranslate();
}

void NotesDetailPane::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange) {
        fill_status_combo();
        retranslate();
    }
    QWidget::changeEvent(e);
}

void NotesDetailPane::retranslate() {
    focus_btn_->setText(focus_mode_ ? tr("◂ BOARD") : tr("⤢ FULL"));
    focus_btn_->setToolTip(focus_mode_ ? tr("Back to the board (Ctrl+Enter)")
                                       : tr("Open the full editor (Ctrl+Enter, or Enter on a card)"));
    focus_btn_->setAccessibleName(focus_btn_->toolTip());
    hide_btn_->setToolTip(tr("Hide the detail pane (Ctrl+Shift+D)"));
    hide_btn_->setAccessibleName(tr("Hide the detail pane"));
    cap_status_->setText(tr("STATUS"));
    cap_review_->setText(tr("REVIEW"));
    status_->setAccessibleName(tr("Note status"));
    review_->setSpecialValueText(tr("none"));
    review_->setAccessibleName(tr("Review date"));
    review_->setToolTip(tr("When to look at this note again. Due notes are marked on the board and counted in the "
                           "ribbon."));
    review_menu_btn_->setToolTip(tr("Quick review dates"));
    review_menu_btn_->setAccessibleName(tr("Quick review dates"));
    reviewed_btn_->setText(tr("MARK REVIEWED"));
    reviewed_btn_->setToolTip(tr("Record that you reviewed this note today (clears a due review date)"));
    open_chat_btn_->setText(tr("OPEN CHAT MODE"));
    open_chat_btn_->setToolTip(tr("Open the Chat Mode run that produced this note"));
    update_linked();
    const QStringList labels = {tr("RESEARCH"), tr("TRADE"), tr("NEWS"), tr("WATCHLIST"), tr("BACKTEST")};
    const QStringList tips = {tr("Open %1 in Equity Research"), tr("Chart / order ticket for %1 (Equity Trading)"),
                              tr("News for %1"), tr("Open %1 in Watchlist"), tr("Backtest %1")};
    for (int i = 0; i < action_btns_.size() && i < labels.size(); ++i) {
        action_btns_.at(i)->setText(labels.at(i));
        action_btns_.at(i)->setToolTip(primary_symbol_.isEmpty() ? tr("Link a ticker to this note first")
                                                                 : tips.at(i).arg(QLatin1Char('$') + primary_symbol_));
    }
    quotes_->set_empty_text(tr("No ticker linked — add one below or type $AAPL in the note"));
    history_header_->set_title(tr("HISTORY"));
    history_header_->set_badge(history_model_->rowCount() > 0 ? QString::number(history_model_->rowCount())
                                                              : QString());
    history_toggle_->setText(history_open_ ? tr("HIDE") : tr("SHOW"));
    history_toggle_->setAccessibleName(history_open_ ? tr("Hide history") : tr("Show history"));

    // Bulk
    bulk_title_->setText(tr("%n note(s) selected", nullptr, bulk_count_));
    const QStringList bulk_labels = {
        bulk_any_unpinned_ ? tr("PIN") : tr("UNPIN"), tr("STATUS"), tr("ADD TAG…"), tr("REVIEW"),
        bulk_all_archived_ ? tr("UNARCHIVE") : tr("ARCHIVE"), tr("EXPORT TO FILE MANAGER"), tr("EXPORT TO FOLDER…"),
        tr("DELETE")};
    for (int i = 0; i < bulk_btns_.size() && i < bulk_labels.size(); ++i)
        bulk_btns_.at(i)->setText(bulk_labels.at(i));
    bulk_hint_->setText(tr("Drag the selection to another lane to move it. Ctrl- or Shift-click cards (or drag a "
                           "box on empty space) to change the selection. Delete can be undone."));

    // Empty
    QString title;
    if (empty_notebook_) {
        empty_->set_content(QStringLiteral("☰"), tr("Your research board"),
                            tr("Trade ideas, earnings prep and a daily journal as cards in a workflow. Link tickers "
                               "with $AAPL, set review dates, drag cards between lanes — everything saves as you type."),
                            tr("NEW NOTE"));
        empty_->set_secondary_action(tr("IMPORT MARKDOWN…"));
    } else {
        QString t = empty_last_title_.trimmed();
        if (t.isEmpty())
            t = tr("Untitled note");
        if (t.size() > 36)
            t = t.left(35) + QChar(0x2026);
        empty_->set_content(QStringLiteral("☰"), tr("No card selected"),
                            tr("Pick a card on the board, continue where you left off, or start a new one."),
                            empty_last_id_ > 0 ? tr("CONTINUE “%1”").arg(t) : tr("NEW NOTE"));
        empty_->set_secondary_action(empty_last_id_ > 0 ? tr("NEW NOTE") : QString());
    }
    empty_or_->setText(tr("Start from a template"));
    for (QPushButton* b : template_btns_) {
        const QString id = b->property("template_id").toString();
        for (const auto& t : builtin_templates())
            if (t.id == id)
                b->setText(ndp_escape(QCoreApplication::translate("NotesBoard", t.name.toUtf8().constData())));
    }
    empty_keys_->setText(tr("N new card · Enter open · Space peek · Shift+arrows move · P pin · / search · "
                            "Del delete (with undo)"));
}

void NotesDetailPane::refit() {
    const QFontMetrics fm(fonts::font(-3, QFont::DemiBold));
    const int h = std::max(22, fm.height() + 8);
    for (QPushButton* b : action_btns_)
        b->setMinimumHeight(h);
    for (QPushButton* b : bulk_btns_)
        b->setMinimumHeight(h + 4);
    for (QPushButton* b : template_btns_)
        b->setMinimumHeight(h + 4);
    reviewed_btn_->setMinimumHeight(h);
    open_chat_btn_->setMinimumHeight(h);
    rename_yes_->setMinimumHeight(h);
    rename_no_->setMinimumHeight(h);
    status_->setMinimumHeight(h);
    review_->setMinimumHeight(h);
    const QFontMetrics hm(fonts::font(-2));
    history_view_->setFixedHeight(std::max(80, (hm.height() + 6) * 6));
    quotes_->updateGeometry();
}

} // namespace fincept::screens::notes
