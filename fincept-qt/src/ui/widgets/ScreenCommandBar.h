// src/ui/widgets/ScreenCommandBar.h
#pragma once
// ScreenCommandBar — the terminal's command bar across the top of a dense screen (the
// Portfolio / Watchlist / Markets look, made generic so every screen shares one):
//
//   row 1  ▌TITLE  [› command / search field ……… /]  hint   [1 BOARD|2 WALL|3 LIST]
//          [N NEW] [T TEMPLATE ▾] [G GROUP: STATUS ▾] …… (stretch) …… [right area] [⋯]
//   row 2  FILTER [PINNED] [HAS TICKER] [PRIORITY ▾] …… (stretch) …… [chips right area] [⋯]
//          (row 2 exists only once a chip is added)
//
//   * Action buttons carry a key-cap hint ("N", "F5", "Alt+1") painted before the
//     label; menu buttons a chevron. Roles: Normal (outlined), Primary (accent
//     fill), Danger. Checked / active buttons and chips are accent outlined.
//   * The command field: Enter -> command_submitted, Esc -> clears (or
//     command_cancelled when already empty), Down -> command_navigate; every edit ->
//     command_edited. A dim hint label beside it says what Enter would do.
//   * Narrow windows / large fonts: items give way by priority (lowest first) into
//     a "⋯" overflow menu at the end of their row — actions, toggles, menus,
//     segmented switches and chips all appear there. The field shrinks to its
//     minimum first. Visibility changes happen in resizeEvent and after
//     ThemeManager::appearance_settled only (never in a theme_changed handler).
//   * Painted from theme tokens at paint time; one selector stylesheet (field,
//     labels, menus) set on the root before any child exists. Sizes from font
//     metrics. Labels are plain text: '&' shows as '&' (no mnemonics).
//
// USAGE
//   auto* bar = new fincept::ui::ScreenCommandBar(this);
//   bar->set_title(tr("NOTES"));
//   bar->set_command_field(tr("Search or command…  ($TICKER  #tag  new trade idea AAPL)"));
//   bar->add_action("new", tr("NEW"), "N", tr("New card in the focused lane (N)"),
//                   fincept::ui::ScreenCommandBar::Role::Primary, 100);
//   bar->add_segmented("view", {{"board", tr("BOARD"), "1"}, {"wall", tr("WALL"), "2"}}, 90);
//   bar->add_menu_action("group", tr("GROUP: STATUS"), group_menu, "G", {}, 80);
//   bar->add_chip("pinned", tr("PINNED"));
//   connect(bar, &fincept::ui::ScreenCommandBar::action_triggered, this, &Screen::on_action);
//   connect(bar, &fincept::ui::ScreenCommandBar::command_submitted, this, &Screen::run_command);

#include <QHash>
#include <QPointer>
#include <QString>
#include <QVector>
#include <QWidget>

class QHBoxLayout;
class QLabel;
class QLineEdit;
class QMenu;
class QPushButton;
class QToolButton;
class QVBoxLayout;

namespace fincept::ui {

class ScreenCommandBar : public QWidget {
    Q_OBJECT
  public:
    enum class Role { Normal, Primary, Danger };
    struct Segment {
        QString key;
        QString text;
        QString key_hint; ///< painted key cap ("1"); empty = none
        QString tooltip;
    };

    explicit ScreenCommandBar(QWidget* parent = nullptr);
    ~ScreenCommandBar() override;

    // ── Title ───────────────────────────────────────────────────────────────
    /// Accent tick + uppercase label at the start of row 1; empty hides it.
    void set_title(const QString& title);

    // ── Command / search field ──────────────────────────────────────────────
    /// Creates the field (once) after the title. `key_hint` is painted at its end
    /// ("/" = the key that focuses it; the screen installs the shortcut).
    void set_command_field(const QString& placeholder, const QString& key_hint = QStringLiteral("/"));
    QLineEdit* command_field() const;
    /// Replace the text without emitting command_edited.
    void set_command_text(const QString& text);
    QString command_text() const;
    /// Dim text beside the field ("ENTER  new Trade idea · AAPL"); empty hides it.
    void set_command_hint(const QString& hint);
    void focus_command_field(bool select_all = true);
    /// Preferred / minimum width of the field in average characters (default 40 / 16).
    void set_command_width_chars(int preferred, int minimum);

    // ── Segmented switch ────────────────────────────────────────────────────
    void add_segmented(const QString& id, const QVector<Segment>& segments, int priority = 50);
    /// Check `key` without emitting segment_selected.
    void set_segment(const QString& id, const QString& key);
    QString segment(const QString& id) const;
    void set_segment_text(const QString& id, const QString& key, const QString& text, const QString& tooltip = {});

    // ── Actions (row 1) ─────────────────────────────────────────────────────
    QPushButton* add_action(const QString& id, const QString& text, const QString& key_hint = {},
                            const QString& tooltip = {}, Role role = Role::Normal, int priority = 50);
    /// A button that opens `menu` (the bar does not own the menu's lifetime unless
    /// it has no parent, then it is parented to the button).
    QPushButton* add_menu_action(const QString& id, const QString& text, QMenu* menu, const QString& key_hint = {},
                                 const QString& tooltip = {}, int priority = 50);
    /// A checkable action: action_toggled(id, on).
    QPushButton* add_toggle(const QString& id, const QString& text, bool on, const QString& key_hint = {},
                            const QString& tooltip = {}, int priority = 50);
    /// A thin vertical rule between groups of actions.
    void add_separator(int priority = 50);
    QPushButton* action(const QString& id) const;
    void set_action_text(const QString& id, const QString& text);
    void set_action_hint(const QString& id, const QString& key_hint);
    void set_action_tooltip(const QString& id, const QString& tooltip);
    void set_action_enabled(const QString& id, bool enabled);
    /// Toggles: checked state without emitting action_toggled. Others: accent look.
    void set_action_checked(const QString& id, bool on);
    /// Change an action's role at run time (RUN Primary <-> CANCEL Danger). The
    /// buttons are painted from the live theme, so this is only a repaint — no
    /// stylesheet, no re-layout; safe at any time, including while the theme
    /// changes. Applies to add_action / add_menu_action buttons; toggles keep
    /// Normal (their checked look is the state) and are left unchanged.
    void set_action_role(const QString& id, Role role);
    /// The role an action button paints with (Normal for unknown ids and toggles).
    Role action_role(const QString& id) const;

    // ── Filter chips (row 2) ────────────────────────────────────────────────
    /// Dim caption at the start of row 2 ("FILTER"); empty hides it.
    void set_chips_label(const QString& text);
    QPushButton* add_chip(const QString& id, const QString& text, const QString& tooltip = {}, int priority = 50);
    QPushButton* add_menu_chip(const QString& id, const QString& text, QMenu* menu, const QString& tooltip = {},
                               int priority = 50);
    /// Checked state without emitting chip_toggled (menu chips: the accent look).
    void set_chip_checked(const QString& id, bool on);
    bool chip_checked(const QString& id) const;
    void set_chip_text(const QString& id, const QString& text);
    void set_chip_tooltip(const QString& id, const QString& tooltip);

    // ── Right-hand areas ────────────────────────────────────────────────────
    /// End of row 1, before the overflow button (QHBoxLayout, spacing 6). Widgets put
    /// here never collapse.
    QWidget* right_area() const;
    /// End of row 2 (created with row 2).
    QWidget* chips_right_area();

    /// Tooltip / accessible name of the overflow buttons ("More").
    void set_overflow_text(const QString& tooltip);

    /// The bar's menu look (token built) for menus a caller pops up itself.
    static QString menu_css();

    QSize minimumSizeHint() const override;

  signals:
    void action_triggered(const QString& id);
    void action_toggled(const QString& id, bool on);
    void segment_selected(const QString& id, const QString& key);
    void chip_toggled(const QString& id, bool on);
    void command_edited(const QString& text);
    void command_submitted(const QString& text);
    /// Esc in an empty field (the screen gives the focus back to its content).
    void command_cancelled();
    /// Up / Down / PageDown in the field (the screen moves into its content).
    void command_navigate(int key);

  protected:
    void resizeEvent(QResizeEvent* e) override;
    void changeEvent(QEvent* e) override;
    bool eventFilter(QObject* obj, QEvent* e) override;

  private:
    enum class Kind { Action, MenuAction, Toggle, Segmented, Separator, Chip, MenuChip };
    struct Item {
        QString id;
        Kind kind = Kind::Action;
        QWidget* widget = nullptr; ///< the button, the segment strip or the rule
        int priority = 50;
        int row = 0;
        int order = 0;
        bool collapsed = false;
        QVector<QPushButton*> segments; ///< Segmented
        QVector<Segment> segment_defs;
    };

    Item* find(const QString& id, int row = -1);
    const Item* find(const QString& id, int row = -1) const;
    QPushButton* make_button(QWidget* parent, const QString& text, const QString& key_hint, const QString& tooltip,
                             Role role, bool chip);
    void insert_row_item(Item item);
    void ensure_chip_row();
    void schedule_collapse();
    void apply_collapse();
    void apply_row_collapse(int row);
    void show_overflow(int row);
    void fit_metrics();

    QVBoxLayout* root_ = nullptr;
    QWidget* row1_ = nullptr;
    QHBoxLayout* row1_layout_ = nullptr;
    QWidget* row2_ = nullptr;
    QHBoxLayout* row2_layout_ = nullptr;
    QWidget* title_box_ = nullptr;
    QLabel* title_tick_ = nullptr;
    QLabel* title_ = nullptr;
    QWidget* command_box_ = nullptr;
    QLineEdit* command_ = nullptr;
    QLabel* command_cap_ = nullptr;
    QLabel* command_hint_ = nullptr;
    QWidget* right_ = nullptr;
    QWidget* chips_right_ = nullptr;
    QLabel* chips_label_ = nullptr;
    QToolButton* overflow_[2] = {nullptr, nullptr};
    QVector<Item> items_;
    int next_order_ = 0;
    int command_pref_chars_ = 40;
    int command_min_chars_ = 16;
    bool collapse_pending_ = false;
    bool updating_ = false;
    QString overflow_text_;
};

} // namespace fincept::ui
