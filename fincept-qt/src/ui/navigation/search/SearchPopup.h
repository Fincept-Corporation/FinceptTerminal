#pragma once
// SearchPopup — the results panel under the header search box: a frameless tool
// window that never takes focus (typing stays in the field), one QListView over
// SearchModel painted by SearchDelegate, and a one-line key legend / status at the
// bottom. Mouse: hover highlights and shows a row's action buttons, click opens,
// right-click lists every action. Width and height come from the window and the
// font; it re-fits on ThemeManager::appearance_settled.

#include <QFrame>
#include <QPointer>

class QLabel;
class QListView;

namespace fincept::ui::search {

class SearchModel;
class SearchDelegate;

class SearchPopup : public QFrame {
    Q_OBJECT
  public:
    explicit SearchPopup(SearchModel* model, QWidget* anchor);
    ~SearchPopup() override;

    QListView* view() const { return view_; }
    /// Current row (-1 = none). set_current() scrolls it into view.
    int current() const;
    void set_current(int row);
    /// Show under the anchor (or re-place when already shown).
    void present();
    void dismiss();
    /// Bottom line: the key legend, or a short status ("Added AAPL to Tech") for a few seconds.
    void set_legend(const QString& text);
    void show_status(const QString& text);
    /// Re-apply colours, fonts and size from the live theme (appearance_settled / language change).
    void refit();

  signals:
    void row_activated(int row, int action, Qt::KeyboardModifiers modifiers); ///< action -1 = the row's default
    void row_context_menu(int row, const QPoint& global_pos);
    void row_hovered(int row);

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    void place();
    int content_height() const;

    SearchModel* model_ = nullptr;
    SearchDelegate* delegate_ = nullptr;
    QPointer<QWidget> anchor_;
    QPointer<QWidget> watched_window_;
    QListView* view_ = nullptr;
    QLabel* legend_ = nullptr;
    QString legend_text_;
    QString applied_css_;
    int status_serial_ = 0;
};

} // namespace fincept::ui::search
