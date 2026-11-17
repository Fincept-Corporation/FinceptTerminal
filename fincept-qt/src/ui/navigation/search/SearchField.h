#pragma once
// SearchField — the header's search box: a line edit that looks like one (search
// glyph, hover / focus states, clear button, the Ctrl+K hint while idle) and turns
// the popup's keys into signals. While the popup is open it claims ↑ ↓ PgUp PgDn
// Enter Tab Esc (accepting the ShortcutOverride — KeyRouting rule 1), so no shell
// shortcut steals them; F-keys and Ctrl/Alt shortcuts still reach the shell.

#include <QLineEdit>
#include <QPointer>

class QAction;

namespace fincept::ui::search {

class SearchField : public QLineEdit {
    Q_OBJECT
  public:
    explicit SearchField(QWidget* parent = nullptr);

    /// The popup is showing: the list keys belong to it.
    void set_popup_open(bool open) { popup_open_ = open; }
    bool popup_open() const { return popup_open_; }
    /// Key hint painted at the right while the box is empty and not focused ("Ctrl+K").
    void set_shortcut_hint(const QString& hint);
    /// Re-apply the look (colours, height) from the live theme. Never call from a theme_changed handler.
    void refit();

  signals:
    void navigate(int step);                         ///< ↑ (-1) ↓ (+1) PgUp (-page) PgDn (+page)
    void accept(Qt::KeyboardModifiers modifiers);    ///< Enter / Ctrl+Enter
    void more_actions();                             ///< Tab, or → with the cursor at the end
    void escape();                                   ///< Esc
    void focus_entered(Qt::FocusReason reason);
    void focus_left(Qt::FocusReason reason);

  protected:
    bool event(QEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void focusInEvent(QFocusEvent* e) override;
    void focusOutEvent(QFocusEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    void changeEvent(QEvent* e) override;

  private:
    bool claims(const QKeyEvent* e) const;
    void update_clear_action();

    bool popup_open_ = false;
    QString hint_;
    QString applied_css_;
    QAction* glyph_ = nullptr;
    QAction* clear_ = nullptr;
};

} // namespace fincept::ui::search
