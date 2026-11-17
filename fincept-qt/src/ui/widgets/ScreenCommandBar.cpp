// src/ui/widgets/ScreenCommandBar.cpp
#include "ui/widgets/ScreenCommandBar.h"

#include "ui/theme/Theme.h"

#include <QAction>
#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QResizeEvent>
#include <QSet>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::ui {

namespace {

/// One button of the bar: painted from the live theme (no per-button stylesheet).
class ScreenCommandBarButton : public QPushButton {
  public:
    enum class Look { Normal, Primary, Danger };
    /// 0 = alone, 1 = first, 2 = middle, 3 = last of a segmented strip.
    explicit ScreenCommandBarButton(QWidget* parent) : QPushButton(parent) {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::TabFocus);
        setAttribute(Qt::WA_Hover, true);
        setFlat(true);
    }

    void set_label(const QString& text) {
        if (label_ == text && !QPushButton::text().isEmpty())
            return;
        label_ = text;
        QString escaped = text;
        escaped.replace(QLatin1Char('&'), QStringLiteral("&&")); // no mnemonics: '&' is a character
        QPushButton::setText(escaped);
        refresh_accessible();
        updateGeometry();
        update();
    }
    QString label() const { return label_; }
    void set_hint(const QString& hint) {
        if (hint_ == hint)
            return;
        hint_ = hint;
        refresh_accessible();
        updateGeometry();
        update();
    }
    QString hint() const { return hint_; }
    void set_look(Look l) {
        if (look_ == l)
            return;
        look_ = l;
        update();
    }
    Look look() const { return look_; }
    void set_chip(bool on) {
        chip_ = on;
        updateGeometry();
    }
    void set_active(bool on) {
        if (active_ == on)
            return;
        active_ = on;
        update();
    }
    bool active() const { return active_; }
    void set_segment_pos(int pos) {
        segment_pos_ = pos;
        update();
    }

    QSize sizeHint() const override {
        const QFont f = label_font();
        const QFontMetrics fm(f);
        const int pad = std::max(chip_ ? 6 : 8, fm.averageCharWidth());
        int w = pad * 2 + fm.horizontalAdvance(label_);
        if (!hint_.isEmpty())
            w += hint_width() + std::max(5, fm.averageCharWidth() / 2 + 2);
        if (menu())
            w += chevron_width();
        const int h = std::max(chip_ ? 20 : 24, fm.height() + (chip_ ? 8 : 10));
        return {w, h};
    }
    QSize minimumSizeHint() const override { return sizeHint(); }

  protected:
    void paintEvent(QPaintEvent*) override {
        namespace c = fincept::ui::colors;
        QPainter p(this);
        const QRect r = rect();
        const bool on = isChecked() || active_;
        const bool hover = underMouse() && isEnabled();
        const bool down = isDown();
        QColor bg = Qt::transparent;
        QColor border = QColor(c::BORDER_DIM());
        QColor fg = QColor(c::TEXT_SECONDARY());
        if (look_ == Look::Primary) {
            bg = hover || down ? QColor(c::ACCENT_BG()) : QColor(c::AMBER());
            border = QColor(c::AMBER());
            fg = hover || down ? QColor(c::AMBER()) : QColor(c::TEXT_ON_ACCENT());
        } else if (look_ == Look::Danger) {
            border = QColor(c::DANGER());
            fg = QColor(c::DANGER());
            if (hover || down)
                bg = c::tint_color(QColor(c::DANGER()), 0.18);
        } else if (on) {
            bg = QColor(c::ACCENT_BG());
            border = QColor(c::AMBER());
            fg = QColor(c::AMBER());
        } else if (hover || down) {
            border = QColor(c::BORDER_MED());
            fg = QColor(c::TEXT_PRIMARY());
            if (down)
                bg = QColor(c::BG_HOVER());
        }
        if (!isEnabled()) {
            bg = Qt::transparent;
            border = QColor(c::BORDER_DIM());
            fg = QColor(c::TEXT_TERTIARY());
        }
        if (bg.alpha() > 0)
            p.fillRect(r, bg);
        // Border: segments share their edges (only the first / checked draw a left edge).
        p.setPen(border);
        const bool draw_left = segment_pos_ <= 1 || on;
        p.drawLine(r.left(), r.top(), r.right(), r.top());
        p.drawLine(r.left(), r.bottom(), r.right(), r.bottom());
        p.drawLine(r.right(), r.top(), r.right(), r.bottom());
        if (draw_left)
            p.drawLine(r.left(), r.top(), r.left(), r.bottom());
        if (hasFocus() && focusPolicy() != Qt::NoFocus) {
            p.setPen(QPen(QColor(c::AMBER_DIM()), 1, Qt::DotLine));
            p.drawRect(r.adjusted(2, 2, -3, -3));
        }

        const QFont f = label_font();
        const QFontMetrics fm(f);
        const int pad = std::max(chip_ ? 6 : 8, fm.averageCharWidth());
        int x = r.left() + pad;
        if (!hint_.isEmpty()) {
            const QFont hf = hint_font();
            const QFontMetrics hm(hf);
            const int hw = hint_width();
            const int hh = std::min(r.height() - 6, hm.height() + 2);
            const QRect cap(x, r.top() + (r.height() - hh) / 2, hw, hh);
            QColor cap_fg = look_ == Look::Primary && !(hover || down) ? QColor(c::TEXT_ON_ACCENT())
                                                                       : QColor(c::TEXT_TERTIARY());
            if (on)
                cap_fg = QColor(c::AMBER());
            QColor cap_border = cap_fg;
            cap_border.setAlphaF(0.55f);
            p.setPen(cap_border);
            p.drawRect(cap.adjusted(0, 0, -1, -1));
            p.setFont(hf);
            p.setPen(cap_fg);
            p.drawText(cap, Qt::AlignCenter, hint_);
            x += hw + std::max(5, fm.averageCharWidth() / 2 + 2);
        }
        const int chev = menu() ? chevron_width() : 0;
        const QRect text_rect(x, r.top(), r.right() - pad - chev - x + 1, r.height());
        p.setFont(f);
        p.setPen(fg);
        p.drawText(text_rect, Qt::AlignLeft | Qt::AlignVCenter, fm.elidedText(label_, Qt::ElideRight, text_rect.width()));
        if (chev > 0) {
            const int s = std::max(3, fm.height() / 5);
            const QPointF c0(r.right() - pad - chev / 2.0 + 2, r.center().y() + 1);
            QPainterPath tri;
            tri.moveTo(c0.x() - s, c0.y() - s / 2.0);
            tri.lineTo(c0.x() + s, c0.y() - s / 2.0);
            tri.lineTo(c0.x(), c0.y() + s / 2.0 + 1);
            tri.closeSubpath();
            p.setRenderHint(QPainter::Antialiasing, true);
            p.fillPath(tri, fg);
        }
    }

  private:
    QFont label_font() const { return fonts::font(-3, QFont::DemiBold); }
    static QFont hint_font() { return fonts::font(-5, QFont::Bold); }
    int hint_width() const {
        const QFontMetrics hm(hint_font());
        return std::max(hm.height(), hm.horizontalAdvance(hint_) + 6);
    }
    int chevron_width() const {
        const QFontMetrics fm(label_font());
        return std::max(10, fm.height() / 2 + 6);
    }
    void refresh_accessible() {
        setAccessibleName(hint_.isEmpty() ? label_ : QStringLiteral("%1 (%2)").arg(label_, hint_));
    }

    QString label_;
    QString hint_;
    Look look_ = Look::Normal;
    bool chip_ = false;
    bool active_ = false;
    int segment_pos_ = 0;
};

ScreenCommandBarButton* cb_button(QPushButton* b) {
    return static_cast<ScreenCommandBarButton*>(b);
}

QString cb_sheet() {
    namespace c = fincept::ui::colors;
    return QStringLiteral(
               "QWidget#uiCommandBar{background:%1;border:none;border-bottom:1px solid %2;}"
               "QWidget#uiCommandBarRow1,QWidget#uiCommandBarRow2{background:transparent;}"
               "QWidget#uiCommandBarRow2{border:none;border-top:1px solid %2;}"
               "QLabel#uiCommandBarTick{background:%3;border:none;}"
               "QLabel#uiCommandBarTitle{color:%4;background:transparent;font-family:%5;font-size:%6px;"
               "font-weight:700;letter-spacing:1.2px;}"
               "QWidget#uiCommandBox{background:%7;border:1px solid %8;}"
               "QLineEdit#uiCommandField{background:transparent;border:none;color:%4;font-family:%5;font-size:%9px;"
               "padding:0px 2px;selection-background-color:%3;selection-color:%10;}"
               "QLabel#uiCommandPrompt{color:%3;background:transparent;font-family:%5;font-size:%9px;font-weight:700;}"
               "QLabel#uiCommandCap{color:%11;background:transparent;border:1px solid %8;font-family:%5;"
               "font-size:%12px;font-weight:700;padding:0px 4px;}"
               "QLabel#uiCommandHint{color:%11;background:transparent;font-family:%5;font-size:%12px;}"
               "QLabel#uiChipsLabel{color:%11;background:transparent;font-family:%5;font-size:%12px;"
               "font-weight:700;letter-spacing:1px;}"
               "QFrame#uiCommandRule{background:%2;border:none;}"
               "QWidget#uiCommandRight{background:transparent;}"
               "QToolButton#uiCommandOverflow{background:transparent;color:%13;border:1px solid %2;"
               "font-family:%5;font-size:%9px;font-weight:700;padding:0px 6px;}"
               "QToolButton#uiCommandOverflow:hover{color:%3;border-color:%8;}"
               "QToolButton#uiCommandOverflow::menu-indicator{image:none;width:0px;}")
        .arg(c::BG_SURFACE(), c::BORDER_DIM(), c::AMBER(), c::TEXT_PRIMARY(), QString(fonts::DATA_FAMILY))
        .arg(fonts::px(-3))
        .arg(c::BG_BASE(), c::BORDER_MED())
        .arg(fonts::px(-2))
        .arg(c::TEXT_ON_ACCENT(), c::TEXT_TERTIARY())
        .arg(fonts::px(-5))
        .arg(c::TEXT_SECONDARY());
}

} // namespace

QString ScreenCommandBar::menu_css() {
    namespace c = fincept::ui::colors;
    return QStringLiteral("QMenu{background:%1;border:1px solid %2;color:%3;font-family:%4;font-size:%5px;}"
                          "QMenu::item{padding:4px 22px 4px 14px;}"
                          "QMenu::item:selected{background:%6;color:%3;}"
                          "QMenu::item:disabled{color:%7;}"
                          "QMenu::section{color:%7;font-weight:700;padding:4px 14px 2px 14px;}"
                          "QMenu::separator{height:1px;background:%8;margin:4px 8px;}"
                          "QMenu::indicator{width:12px;height:12px;}")
        .arg(c::BG_RAISED(), c::BORDER_MED(), c::TEXT_PRIMARY(), QString(fonts::DATA_FAMILY))
        .arg(fonts::px(-3))
        .arg(c::BG_HOVER(), c::TEXT_TERTIARY(), c::BORDER_DIM());
}

ScreenCommandBar::ScreenCommandBar(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("uiCommandBar"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(cb_sheet()); // before any child exists
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    overflow_text_ = tr("More");

    root_ = new QVBoxLayout(this);
    root_->setContentsMargins(0, 0, 0, 0);
    root_->setSpacing(0);

    row1_ = new QWidget(this);
    row1_->setObjectName(QStringLiteral("uiCommandBarRow1"));
    row1_layout_ = new QHBoxLayout(row1_);
    row1_layout_->setContentsMargins(10, 5, 8, 5);
    row1_layout_->setSpacing(6);
    root_->addWidget(row1_);

    // Title (hidden until set).
    title_box_ = new QWidget(row1_);
    auto* tl = new QHBoxLayout(title_box_);
    tl->setContentsMargins(0, 0, 6, 0);
    tl->setSpacing(7);
    title_tick_ = new QLabel(title_box_);
    title_tick_->setObjectName(QStringLiteral("uiCommandBarTick"));
    tl->addWidget(title_tick_, 0, Qt::AlignVCenter);
    title_ = new QLabel(title_box_);
    title_->setObjectName(QStringLiteral("uiCommandBarTitle"));
    title_->setTextFormat(Qt::PlainText);
    tl->addWidget(title_, 0, Qt::AlignVCenter);
    title_box_->setVisible(false);
    row1_layout_->addWidget(title_box_);

    row1_layout_->addStretch(1); // items are inserted before it

    right_ = new QWidget(row1_);
    right_->setObjectName(QStringLiteral("uiCommandRight"));
    auto* rl = new QHBoxLayout(right_);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(6);
    row1_layout_->addWidget(right_);

    overflow_[0] = new QToolButton(row1_);
    overflow_[0]->setObjectName(QStringLiteral("uiCommandOverflow"));
    overflow_[0]->setText(QStringLiteral("⋯"));
    overflow_[0]->setCursor(Qt::PointingHandCursor);
    overflow_[0]->setVisible(false);
    connect(overflow_[0], &QToolButton::clicked, this, [this]() { show_overflow(0); });
    row1_layout_->addWidget(overflow_[0]);

    fit_metrics();
    set_overflow_text(overflow_text_);
    // A font change alters every width: re-fit and re-collapse once styles are final.
    connect(&ThemeManager::instance(), &ThemeManager::appearance_settled, this, [this]() {
        fit_metrics();
        updateGeometry();
        schedule_collapse();
    });
}

ScreenCommandBar::~ScreenCommandBar() = default;

// ── Title / field ────────────────────────────────────────────────────────────

void ScreenCommandBar::set_title(const QString& title) {
    title_->setText(title.toUpper());
    title_->setAccessibleName(title);
    title_box_->setVisible(!title.isEmpty());
    schedule_collapse();
}

void ScreenCommandBar::set_command_field(const QString& placeholder, const QString& key_hint) {
    if (!command_box_) {
        command_box_ = new QWidget(row1_);
        command_box_->setObjectName(QStringLiteral("uiCommandBox"));
        command_box_->setAttribute(Qt::WA_StyledBackground, true);
        auto* l = new QHBoxLayout(command_box_);
        l->setContentsMargins(6, 0, 4, 0);
        l->setSpacing(4);
        auto* prompt = new QLabel(QStringLiteral("›"), command_box_);
        prompt->setObjectName(QStringLiteral("uiCommandPrompt"));
        l->addWidget(prompt, 0, Qt::AlignVCenter);
        command_ = new QLineEdit(command_box_);
        command_->setObjectName(QStringLiteral("uiCommandField"));
        command_->setFrame(false);
        command_->setClearButtonEnabled(true);
        command_->installEventFilter(this);
        l->addWidget(command_, 1);
        command_cap_ = new QLabel(command_box_);
        command_cap_->setObjectName(QStringLiteral("uiCommandCap"));
        command_cap_->setAlignment(Qt::AlignCenter);
        l->addWidget(command_cap_, 0, Qt::AlignVCenter);
        connect(command_, &QLineEdit::textEdited, this, [this](const QString& t) { emit command_edited(t); });
        // The clear button changes the text without textEdited.
        connect(command_, &QLineEdit::textChanged, this, [this](const QString& t) {
            if (t.isEmpty() && !updating_)
                emit command_edited(t);
        });
        // After the title.
        row1_layout_->insertWidget(row1_layout_->indexOf(title_box_) + 1, command_box_, 0);
        command_hint_ = new QLabel(row1_);
        command_hint_->setObjectName(QStringLiteral("uiCommandHint"));
        command_hint_->setTextFormat(Qt::PlainText);
        command_hint_->setVisible(false);
        row1_layout_->insertWidget(row1_layout_->indexOf(command_box_) + 1, command_hint_, 0);
    }
    command_->setPlaceholderText(placeholder);
    command_->setAccessibleName(placeholder);
    command_cap_->setText(key_hint);
    command_cap_->setVisible(!key_hint.isEmpty());
    fit_metrics();
    schedule_collapse();
}

QLineEdit* ScreenCommandBar::command_field() const {
    return command_;
}

void ScreenCommandBar::set_command_text(const QString& text) {
    if (!command_ || command_->text() == text)
        return;
    updating_ = true;
    command_->setText(text);
    updating_ = false;
}

QString ScreenCommandBar::command_text() const {
    return command_ ? command_->text() : QString();
}

void ScreenCommandBar::set_command_hint(const QString& hint) {
    if (!command_hint_)
        return;
    if (command_hint_->text() != hint)
        command_hint_->setText(hint);
    const bool show = !hint.isEmpty() && width() >= 700;
    if (command_hint_->isVisibleTo(row1_) != show)
        command_hint_->setVisible(show);
}

void ScreenCommandBar::focus_command_field(bool select_all) {
    if (!command_)
        return;
    command_->setFocus(Qt::ShortcutFocusReason);
    if (select_all)
        command_->selectAll();
}

void ScreenCommandBar::set_command_width_chars(int preferred, int minimum) {
    command_pref_chars_ = std::max(8, preferred);
    command_min_chars_ = std::clamp(minimum, 6, command_pref_chars_);
    fit_metrics();
    schedule_collapse();
}

// ── Items ────────────────────────────────────────────────────────────────────

ScreenCommandBar::Item* ScreenCommandBar::find(const QString& id, int row) {
    for (auto& it : items_)
        if (it.id == id && (row < 0 || it.row == row))
            return &it;
    return nullptr;
}

const ScreenCommandBar::Item* ScreenCommandBar::find(const QString& id, int row) const {
    for (const auto& it : items_)
        if (it.id == id && (row < 0 || it.row == row))
            return &it;
    return nullptr;
}

QPushButton* ScreenCommandBar::make_button(QWidget* parent, const QString& text, const QString& key_hint,
                                           const QString& tooltip, Role role, bool chip) {
    auto* b = new ScreenCommandBarButton(parent);
    b->set_chip(chip);
    b->set_label(text);
    b->set_hint(key_hint);
    b->set_look(role == Role::Primary  ? ScreenCommandBarButton::Look::Primary
                : role == Role::Danger ? ScreenCommandBarButton::Look::Danger
                                       : ScreenCommandBarButton::Look::Normal);
    if (!tooltip.isEmpty())
        b->setToolTip(tooltip);
    return b;
}

void ScreenCommandBar::insert_row_item(Item item) {
    item.order = next_order_++;
    QHBoxLayout* lay = item.row == 0 ? row1_layout_ : row2_layout_;
    // Row 1: before the stretch; row 2: before its stretch too.
    int stretch_at = -1;
    for (int i = 0; i < lay->count(); ++i)
        if (lay->itemAt(i)->spacerItem()) {
            stretch_at = i;
            break;
        }
    lay->insertWidget(stretch_at < 0 ? lay->count() : stretch_at, item.widget, 0, Qt::AlignVCenter);
    items_.append(item);
    schedule_collapse();
}

void ScreenCommandBar::add_segmented(const QString& id, const QVector<Segment>& segments, int priority) {
    auto* strip = new QWidget(row1_);
    auto* l = new QHBoxLayout(strip);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    Item item;
    item.id = id;
    item.kind = Kind::Segmented;
    item.widget = strip;
    item.priority = priority;
    item.row = 0;
    item.segment_defs = segments;
    for (int i = 0; i < segments.size(); ++i) {
        const Segment& s = segments.at(i);
        QPushButton* b = make_button(strip, s.text, s.key_hint, s.tooltip, Role::Normal, false);
        b->setCheckable(true);
        cb_button(b)->set_segment_pos(segments.size() == 1 ? 0 : i == 0 ? 1 : i == segments.size() - 1 ? 3 : 2);
        if (i == 0)
            b->setChecked(true);
        const QString key = s.key;
        connect(b, &QPushButton::clicked, this, [this, id, key]() {
            set_segment(id, key);
            emit segment_selected(id, key);
        });
        l->addWidget(b);
        item.segments << b;
    }
    insert_row_item(item);
}

void ScreenCommandBar::set_segment(const QString& id, const QString& key) {
    Item* it = find(id, 0);
    if (!it || it->kind != Kind::Segmented)
        return;
    for (int i = 0; i < it->segments.size(); ++i) {
        const bool on = it->segment_defs.at(i).key == key;
        if (it->segments.at(i)->isChecked() != on)
            it->segments.at(i)->setChecked(on);
    }
}

QString ScreenCommandBar::segment(const QString& id) const {
    const Item* it = find(id, 0);
    if (!it || it->kind != Kind::Segmented)
        return {};
    for (int i = 0; i < it->segments.size(); ++i)
        if (it->segments.at(i)->isChecked())
            return it->segment_defs.at(i).key;
    return {};
}

void ScreenCommandBar::set_segment_text(const QString& id, const QString& key, const QString& text,
                                        const QString& tooltip) {
    Item* it = find(id, 0);
    if (!it || it->kind != Kind::Segmented)
        return;
    for (int i = 0; i < it->segments.size(); ++i)
        if (it->segment_defs.at(i).key == key) {
            it->segment_defs[i].text = text;
            cb_button(it->segments.at(i))->set_label(text);
            if (!tooltip.isEmpty()) {
                it->segment_defs[i].tooltip = tooltip;
                it->segments.at(i)->setToolTip(tooltip);
            }
        }
    schedule_collapse();
}

QPushButton* ScreenCommandBar::add_action(const QString& id, const QString& text, const QString& key_hint,
                                          const QString& tooltip, Role role, int priority) {
    QPushButton* b = make_button(row1_, text, key_hint, tooltip, role, false);
    connect(b, &QPushButton::clicked, this, [this, id]() { emit action_triggered(id); });
    Item item;
    item.id = id;
    item.kind = Kind::Action;
    item.widget = b;
    item.priority = priority;
    insert_row_item(item);
    return b;
}

QPushButton* ScreenCommandBar::add_menu_action(const QString& id, const QString& text, QMenu* menu,
                                               const QString& key_hint, const QString& tooltip, int priority) {
    QPushButton* b = make_button(row1_, text, key_hint, tooltip, Role::Normal, false);
    if (menu) {
        if (!menu->parent())
            menu->setParent(b, menu->windowFlags());
        if (menu->styleSheet().isEmpty())
            menu->setStyleSheet(menu_css());
        b->setMenu(menu);
    }
    Item item;
    item.id = id;
    item.kind = Kind::MenuAction;
    item.widget = b;
    item.priority = priority;
    insert_row_item(item);
    return b;
}

QPushButton* ScreenCommandBar::add_toggle(const QString& id, const QString& text, bool on, const QString& key_hint,
                                          const QString& tooltip, int priority) {
    QPushButton* b = make_button(row1_, text, key_hint, tooltip, Role::Normal, false);
    b->setCheckable(true);
    b->setChecked(on);
    connect(b, &QPushButton::toggled, this, [this, id](bool checked) {
        if (!updating_)
            emit action_toggled(id, checked);
    });
    Item item;
    item.id = id;
    item.kind = Kind::Toggle;
    item.widget = b;
    item.priority = priority;
    insert_row_item(item);
    return b;
}

void ScreenCommandBar::add_separator(int priority) {
    auto* rule = new QFrame(row1_);
    rule->setObjectName(QStringLiteral("uiCommandRule"));
    rule->setFixedWidth(1);
    rule->setMinimumHeight(16);
    Item item;
    item.id = QStringLiteral("__sep%1").arg(next_order_);
    item.kind = Kind::Separator;
    item.widget = rule;
    item.priority = priority;
    insert_row_item(item);
}

QPushButton* ScreenCommandBar::action(const QString& id) const {
    const Item* it = find(id, 0);
    if (!it || it->kind == Kind::Segmented || it->kind == Kind::Separator)
        return nullptr;
    return static_cast<QPushButton*>(it->widget);
}

void ScreenCommandBar::set_action_text(const QString& id, const QString& text) {
    if (QPushButton* b = action(id)) {
        if (cb_button(b)->label() == text)
            return;
        cb_button(b)->set_label(text);
        schedule_collapse();
    }
}

void ScreenCommandBar::set_action_hint(const QString& id, const QString& key_hint) {
    if (QPushButton* b = action(id)) {
        cb_button(b)->set_hint(key_hint);
        schedule_collapse();
    }
}

void ScreenCommandBar::set_action_tooltip(const QString& id, const QString& tooltip) {
    if (QPushButton* b = action(id))
        b->setToolTip(tooltip);
}

void ScreenCommandBar::set_action_enabled(const QString& id, bool enabled) {
    if (QPushButton* b = action(id))
        b->setEnabled(enabled);
}

void ScreenCommandBar::set_action_checked(const QString& id, bool on) {
    QPushButton* b = action(id);
    if (!b)
        return;
    if (b->isCheckable()) {
        updating_ = true;
        b->setChecked(on);
        updating_ = false;
    } else {
        cb_button(b)->set_active(on);
    }
}

void ScreenCommandBar::set_action_role(const QString& id, Role role) {
    const Item* it = find(id, 0);
    if (!it || (it->kind != Kind::Action && it->kind != Kind::MenuAction))
        return;
    // Painted from the live theme: changing the look is a repaint only (the size
    // hint does not depend on it), so no stylesheet and no re-collapse.
    cb_button(static_cast<QPushButton*>(it->widget))
        ->set_look(role == Role::Primary  ? ScreenCommandBarButton::Look::Primary
                   : role == Role::Danger ? ScreenCommandBarButton::Look::Danger
                                          : ScreenCommandBarButton::Look::Normal);
}

ScreenCommandBar::Role ScreenCommandBar::action_role(const QString& id) const {
    const Item* it = find(id, 0);
    if (!it || (it->kind != Kind::Action && it->kind != Kind::MenuAction))
        return Role::Normal;
    switch (cb_button(static_cast<QPushButton*>(it->widget))->look()) {
        case ScreenCommandBarButton::Look::Primary:
            return Role::Primary;
        case ScreenCommandBarButton::Look::Danger:
            return Role::Danger;
        case ScreenCommandBarButton::Look::Normal:
            break;
    }
    return Role::Normal;
}

// ── Chips ────────────────────────────────────────────────────────────────────

void ScreenCommandBar::ensure_chip_row() {
    if (row2_)
        return;
    row2_ = new QWidget(this);
    row2_->setObjectName(QStringLiteral("uiCommandBarRow2"));
    row2_->setAttribute(Qt::WA_StyledBackground, true);
    row2_layout_ = new QHBoxLayout(row2_);
    row2_layout_->setContentsMargins(10, 4, 8, 4);
    row2_layout_->setSpacing(5);
    chips_label_ = new QLabel(row2_);
    chips_label_->setObjectName(QStringLiteral("uiChipsLabel"));
    chips_label_->setVisible(false);
    row2_layout_->addWidget(chips_label_);
    row2_layout_->addStretch(1);
    chips_right_ = new QWidget(row2_);
    chips_right_->setObjectName(QStringLiteral("uiCommandRight"));
    auto* rl = new QHBoxLayout(chips_right_);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(6);
    row2_layout_->addWidget(chips_right_);
    overflow_[1] = new QToolButton(row2_);
    overflow_[1]->setObjectName(QStringLiteral("uiCommandOverflow"));
    overflow_[1]->setText(QStringLiteral("⋯"));
    overflow_[1]->setCursor(Qt::PointingHandCursor);
    overflow_[1]->setVisible(false);
    overflow_[1]->setToolTip(overflow_text_);
    overflow_[1]->setAccessibleName(overflow_text_);
    connect(overflow_[1], &QToolButton::clicked, this, [this]() { show_overflow(1); });
    row2_layout_->addWidget(overflow_[1]);
    root_->addWidget(row2_);
    fit_metrics();
}

void ScreenCommandBar::set_chips_label(const QString& text) {
    ensure_chip_row();
    chips_label_->setText(text.toUpper());
    chips_label_->setVisible(!text.isEmpty());
}

QPushButton* ScreenCommandBar::add_chip(const QString& id, const QString& text, const QString& tooltip, int priority) {
    ensure_chip_row();
    QPushButton* b = make_button(row2_, text, {}, tooltip, Role::Normal, true);
    b->setCheckable(true);
    connect(b, &QPushButton::toggled, this, [this, id](bool on) {
        if (!updating_)
            emit chip_toggled(id, on);
    });
    Item item;
    item.id = id;
    item.kind = Kind::Chip;
    item.widget = b;
    item.priority = priority;
    item.row = 1;
    insert_row_item(item);
    return b;
}

QPushButton* ScreenCommandBar::add_menu_chip(const QString& id, const QString& text, QMenu* menu,
                                             const QString& tooltip, int priority) {
    ensure_chip_row();
    QPushButton* b = make_button(row2_, text, {}, tooltip, Role::Normal, true);
    if (menu) {
        if (!menu->parent())
            menu->setParent(b, menu->windowFlags());
        if (menu->styleSheet().isEmpty())
            menu->setStyleSheet(menu_css());
        b->setMenu(menu);
    }
    Item item;
    item.id = id;
    item.kind = Kind::MenuChip;
    item.widget = b;
    item.priority = priority;
    item.row = 1;
    insert_row_item(item);
    return b;
}

void ScreenCommandBar::set_chip_checked(const QString& id, bool on) {
    Item* it = find(id, 1);
    if (!it)
        return;
    auto* b = static_cast<QPushButton*>(it->widget);
    if (b->isCheckable()) {
        if (b->isChecked() == on)
            return;
        updating_ = true;
        b->setChecked(on);
        updating_ = false;
    } else {
        cb_button(b)->set_active(on);
    }
}

bool ScreenCommandBar::chip_checked(const QString& id) const {
    const Item* it = find(id, 1);
    if (!it)
        return false;
    auto* b = static_cast<QPushButton*>(it->widget);
    return b->isCheckable() ? b->isChecked() : static_cast<ScreenCommandBarButton*>(b)->active();
}

void ScreenCommandBar::set_chip_text(const QString& id, const QString& text) {
    Item* it = find(id, 1);
    if (!it)
        return;
    auto* b = cb_button(static_cast<QPushButton*>(it->widget));
    if (b->label() == text)
        return;
    b->set_label(text);
    schedule_collapse();
}

void ScreenCommandBar::set_chip_tooltip(const QString& id, const QString& tooltip) {
    if (Item* it = find(id, 1))
        it->widget->setToolTip(tooltip);
}

QWidget* ScreenCommandBar::right_area() const {
    return right_;
}

QWidget* ScreenCommandBar::chips_right_area() {
    ensure_chip_row();
    return chips_right_;
}

void ScreenCommandBar::set_overflow_text(const QString& tooltip) {
    overflow_text_ = tooltip;
    for (QToolButton* b : overflow_)
        if (b) {
            b->setToolTip(tooltip);
            b->setAccessibleName(tooltip);
        }
}

// ── Metrics / collapse ───────────────────────────────────────────────────────

void ScreenCommandBar::fit_metrics() {
    const QFontMetrics fm(fonts::font(-3, QFont::DemiBold));
    const int h = std::max(24, fm.height() + 10);
    const QFontMetrics tm(fonts::font(-3, QFont::Bold));
    title_tick_->setFixedSize(3, std::max(10, tm.capHeight() + 4));
    for (QToolButton* b : overflow_)
        if (b)
            b->setFixedHeight(h);
    if (command_box_) {
        const QFontMetrics cm(fonts::font(-2));
        command_box_->setFixedHeight(h);
        command_box_->setMinimumWidth(cm.averageCharWidth() * command_min_chars_ + 40);
        command_box_->setMaximumWidth(cm.averageCharWidth() * command_pref_chars_ + 60);
        command_box_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        const QFontMetrics capm(fonts::font(-5, QFont::Bold));
        command_cap_->setMinimumWidth(capm.height() + 2);
    }
}

void ScreenCommandBar::schedule_collapse() {
    if (collapse_pending_)
        return;
    collapse_pending_ = true;
    QTimer::singleShot(0, this, [this]() {
        collapse_pending_ = false;
        apply_collapse();
    });
}

void ScreenCommandBar::apply_collapse() {
    apply_row_collapse(0);
    if (row2_)
        apply_row_collapse(1);
    if (command_hint_) {
        const bool show = !command_hint_->text().isEmpty() && width() >= 700;
        if (command_hint_->isVisibleTo(row1_) != show)
            command_hint_->setVisible(show);
    }
}

void ScreenCommandBar::apply_row_collapse(int row) {
    QWidget* host = row == 0 ? row1_ : row2_;
    QHBoxLayout* lay = row == 0 ? row1_layout_ : row2_layout_;
    if (!host || !lay || host->width() <= 0)
        return;
    const QMargins m = lay->contentsMargins();
    const int spacing = lay->spacing();
    int fixed = m.left() + m.right();
    int count = 0;
    // Everything that never collapses.
    const auto add_fixed = [&](QWidget* w, bool use_min) {
        if (!w || !w->isVisibleTo(host))
            return;
        fixed += use_min ? w->minimumSizeHint().width() : w->sizeHint().width();
        ++count;
    };
    if (row == 0) {
        add_fixed(title_box_, false);
        if (command_box_) {
            fixed += command_box_->minimumWidth(); // the field shrinks to its minimum before anything collapses
            ++count;
        }
        add_fixed(right_->layout() && right_->layout()->count() > 0 ? right_ : nullptr, false);
    } else {
        add_fixed(chips_label_, false);
        add_fixed(chips_right_ && chips_right_->layout()->count() > 0 ? chips_right_ : nullptr, false);
    }
    QVector<Item*> row_items;
    for (auto& it : items_)
        if (it.row == row)
            row_items << &it;
    int needed = fixed;
    for (Item* it : row_items)
        needed += it->widget->sizeHint().width() + spacing;
    needed += spacing * count;
    const int avail = host->width();
    // Lowest priority first; among equals the later item gives way first.
    QVector<Item*> order = row_items;
    std::sort(order.begin(), order.end(), [](const Item* a, const Item* b) {
        if (a->priority != b->priority)
            return a->priority < b->priority;
        return a->order > b->order;
    });
    QSet<Item*> hide;
    QToolButton* more = overflow_[row];
    const int more_w = more ? std::max(more->sizeHint().width(), 24) + spacing : 0;
    if (needed > avail) {
        needed += more_w;
        for (Item* it : order) {
            if (needed <= avail)
                break;
            hide.insert(it);
            needed -= it->widget->sizeHint().width() + spacing;
        }
    }
    // A separator with nothing visible on one side is noise.
    for (int i = 0; i < row_items.size(); ++i) {
        Item* it = row_items.at(i);
        if (it->kind != Kind::Separator || hide.contains(it))
            continue;
        bool left = false, right = false;
        for (int j = i - 1; j >= 0 && !left; --j)
            left = row_items.at(j)->kind != Kind::Separator && !hide.contains(row_items.at(j));
        for (int j = i + 1; j < row_items.size() && !right; ++j)
            right = row_items.at(j)->kind != Kind::Separator && !hide.contains(row_items.at(j));
        if (!left || !right)
            hide.insert(it);
    }
    bool any = false;
    for (Item* it : row_items) {
        const bool collapse = hide.contains(it);
        it->collapsed = collapse;
        if (it->widget->isVisibleTo(host) == collapse)
            it->widget->setVisible(!collapse);
        any = any || (collapse && it->kind != Kind::Separator);
    }
    if (more && more->isVisibleTo(host) != any)
        more->setVisible(any);
}

void ScreenCommandBar::show_overflow(int row) {
    QToolButton* more = overflow_[row];
    if (!more)
        return;
    QMenu menu(this);
    menu.setStyleSheet(menu_css());
    for (Item& it : items_) {
        if (it.row != row || !it.collapsed || it.kind == Kind::Separator)
            continue;
        auto* b = static_cast<ScreenCommandBarButton*>(it.kind == Kind::Segmented ? nullptr : it.widget);
        const QString id = it.id;
        switch (it.kind) {
        case Kind::Action: {
            QAction* a = menu.addAction(b->label() + (b->hint().isEmpty() ? QString() : QLatin1Char('\t') + b->hint()));
            a->setEnabled(b->isEnabled());
            // An action folded into the menu keeps its agent-risk marker: an agent
            // choosing it here needs the same approval as pressing the button.
            if (const QVariant risk = b->property("fincept_agent_risk"); risk.isValid())
                a->setProperty("fincept_agent_risk", risk);
            connect(a, &QAction::triggered, this, [this, id]() { emit action_triggered(id); });
            break;
        }
        case Kind::Toggle:
        case Kind::Chip: {
            QAction* a = menu.addAction(b->label() + (b->hint().isEmpty() ? QString() : QLatin1Char('\t') + b->hint()));
            a->setCheckable(true);
            a->setChecked(b->isChecked());
            a->setEnabled(b->isEnabled());
            // An action folded into the menu keeps its agent-risk marker: an agent
            // choosing it here needs the same approval as pressing the button.
            if (const QVariant risk = b->property("fincept_agent_risk"); risk.isValid())
                a->setProperty("fincept_agent_risk", risk);
            QPointer<QPushButton> target = b;
            connect(a, &QAction::triggered, this, [target](bool on) {
                if (target)
                    target->setChecked(on); // emits through the button's own connection
            });
            break;
        }
        case Kind::MenuAction:
        case Kind::MenuChip:
            if (QMenu* sub = b->menu()) {
                sub->setTitle(b->label());
                menu.addMenu(sub);
            }
            break;
        case Kind::Segmented: {
            menu.addSeparator();
            for (int i = 0; i < it.segments.size(); ++i) {
                const Segment& s = it.segment_defs.at(i);
                QAction* a = menu.addAction(s.text + (s.key_hint.isEmpty() ? QString() : QLatin1Char('\t') + s.key_hint));
                a->setCheckable(true);
                a->setChecked(it.segments.at(i)->isChecked());
                const QString key = s.key;
                connect(a, &QAction::triggered, this, [this, id, key]() {
                    set_segment(id, key);
                    emit segment_selected(id, key);
                });
            }
            menu.addSeparator();
            break;
        }
        case Kind::Separator:
            break;
        }
    }
    if (menu.isEmpty())
        return;
    menu.exec(more->mapToGlobal(QPoint(0, more->height())));
}

QSize ScreenCommandBar::minimumSizeHint() const {
    // Never demands its full-content width: items collapse into the overflow menu.
    return {0, QWidget::minimumSizeHint().height()};
}

void ScreenCommandBar::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    apply_collapse(); // visibility changes here (and after appearance_settled) only
}

void ScreenCommandBar::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        schedule_collapse();
    QWidget::changeEvent(e);
}

bool ScreenCommandBar::eventFilter(QObject* obj, QEvent* e) {
    if (obj == command_ && e->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(e);
        switch (ke->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            emit command_submitted(command_->text());
            return true;
        case Qt::Key_Escape:
            if (!command_->text().isEmpty()) {
                command_->clear();
                emit command_edited(QString());
            } else {
                emit command_cancelled();
            }
            return true;
        case Qt::Key_Down:
        case Qt::Key_Up:
        case Qt::Key_PageDown:
            emit command_navigate(ke->key());
            return true;
        default:
            break;
        }
    }
    return QWidget::eventFilter(obj, e);
}

} // namespace fincept::ui
