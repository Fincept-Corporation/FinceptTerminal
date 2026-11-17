// src/screens/notes/NotesDetailPane.h
//
// The right-hand pane of the Notes board (resizable, collapsible; the screen's
// focus mode widens it to the full editor):
//
//   NOTE header      ▌ NOTE · <status>                        [⤢ FULL] [×]
//   board strip      STATUS [ACTIVE ▾]  REVIEW [2026-11-20 ▾]  [MARK REVIEWED]  ⇄ FROM CHAT RUN … [OPEN CHAT]
//   rename offer     "TATAMOTORS is now TMPV"  [REPLACE] [KEEP]          (only when the resolver says so)
//   quote strip      $AAPL  189.20  +1.20%   $MSFT  410.10  −0.30% …   (live, click = symbol menu)
//   linked actions   RESEARCH  TRADE  NEWS  WATCHLIST  BACKTEST  ……  [ADD TO REPORT ▾]
//   editor           NotesEditor (WRITE / SPLIT / READ, autosave state, tags, tickers)
//   HISTORY          created / edited / status changes (collapsible)
//
// Several cards selected: the bulk page (pin, status, tag, review, archive,
// export, delete). Nothing selected: a designed empty state with the last note,
// NEW and the four templates — never a blank pane.
#pragma once

#include "screens/notes/NotesCardPainter.h"
#include "screens/notes/NotesStore.h"
#include "storage/repositories/NoteBoardTypes.h"

#include <QDate>
#include <QPointer>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QComboBox;
class QDateEdit;
class QFrame;
class QLabel;
class QListView;
class QPushButton;
class QStackedWidget;
class QStandardItemModel;
class QToolButton;

namespace fincept::ui {
class AddToReportButton;
class EmptyState;
class PanelHeader;
} // namespace fincept::ui

namespace fincept::screens::notes {

class NotesEditor;

/// One line per linked ticker: symbol, last, change (painted; click = menu).
class NotesQuoteStrip : public QWidget {
    Q_OBJECT
  public:
    explicit NotesQuoteStrip(QWidget* parent = nullptr);
    void set_symbols(const QStringList& symbols);
    void set_quotes(const QuoteBook* quotes) { quotes_ = quotes; }
    void set_empty_text(const QString& text);
    void refresh() { update(); }
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override;

  signals:
    void symbol_clicked(const QString& symbol);

  protected:
    void paintEvent(QPaintEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent* e) override;

  private:
    struct Cell {
        QString symbol;
        QRect rect;
    };
    QVector<Cell> layout_cells(int width) const;
    int cell_width() const;
    int row_height() const;

    QStringList symbols_;
    const QuoteBook* quotes_ = nullptr;
    QString empty_text_;
    int hover_ = -1;
};

class NotesDetailPane : public QWidget {
    Q_OBJECT
  public:
    enum class Page { Note, Bulk, Empty };

    explicit NotesDetailPane(QWidget* parent = nullptr);

    NotesEditor* editor() const { return editor_; }
    fincept::ui::AddToReportButton* report_button() const { return report_; }
    NotesQuoteStrip* quote_strip() const { return quotes_; }

    void show_page(Page p);
    Page page() const { return page_; }

    /// The open note's board row and origin (no signal emitted).
    void set_note_meta(const NoteEntry& e, bool archived);
    void set_new_note(const QString& status);
    void set_tickers(const QStringList& symbols);
    void set_history(const QVector<fincept::notes_board::NoteBoardEvent>& events, const QString& created_at,
                     const QString& updated_at);
    /// "X is now Y" — offered, never applied silently. Empty `from` hides it.
    void show_rename_offer(const QString& from, const QString& to);
    QString rename_from() const { return rename_from_; }
    void set_focus_mode(bool on);

    void set_bulk(int count, bool any_unpinned, bool all_archived);
    /// The empty page: a new notebook, or "pick a card" with the last edited note.
    void set_empty(bool notebook_empty, int last_id, const QString& last_title);

    void retranslate();
    /// Re-measure (after ThemeManager::appearance_settled).
    void refit();

  signals:
    void status_changed(const QString& status);
    void review_changed(const QDate& date); ///< invalid = cleared
    void mark_reviewed();
    void open_chat_run();
    void rename_accepted(const QString& from, const QString& to);
    void rename_dismissed(const QString& from);
    void focus_toggled();
    void hide_requested();
    void symbol_clicked(const QString& symbol);
    void linked_action(const QString& screen_id, const QString& symbol);
    /// "pin", "unpin", "status:<CODE>", "tag", "review:<days>|clear", "archive", "unarchive",
    /// "export_fm", "export_dir", "delete"
    void bulk_action(const QString& action);
    void template_requested(const QString& template_id);
    void new_requested();
    void import_requested();
    void continue_requested(int id);

  protected:
    void changeEvent(QEvent* e) override;

  private:
    QWidget* build_note_page();
    QWidget* build_bulk_page();
    QWidget* build_empty_page();
    void fill_status_combo();
    void update_linked();

    Page page_ = Page::Empty;
    QStackedWidget* stack_ = nullptr;
    QWidget* note_page_ = nullptr;
    QWidget* bulk_page_ = nullptr;
    QWidget* empty_page_ = nullptr;

    // Note page
    fincept::ui::PanelHeader* header_ = nullptr;
    QToolButton* focus_btn_ = nullptr;
    QToolButton* hide_btn_ = nullptr;
    QLabel* cap_status_ = nullptr;
    QComboBox* status_ = nullptr;
    QLabel* cap_review_ = nullptr;
    QDateEdit* review_ = nullptr;
    QToolButton* review_menu_btn_ = nullptr;
    QPushButton* reviewed_btn_ = nullptr;
    QLabel* linked_ = nullptr;
    QPushButton* open_chat_btn_ = nullptr;
    QFrame* rename_box_ = nullptr;
    QLabel* rename_text_ = nullptr;
    QPushButton* rename_yes_ = nullptr;
    QPushButton* rename_no_ = nullptr;
    QString rename_from_;
    QString rename_to_;
    NotesQuoteStrip* quotes_ = nullptr;
    QWidget* actions_row_ = nullptr;
    QList<QPushButton*> action_btns_;
    fincept::ui::AddToReportButton* report_ = nullptr;
    NotesEditor* editor_ = nullptr;
    fincept::ui::PanelHeader* history_header_ = nullptr;
    QToolButton* history_toggle_ = nullptr;
    QListView* history_view_ = nullptr;
    QStandardItemModel* history_model_ = nullptr;
    bool history_open_ = false;
    QString primary_symbol_;
    bool loading_ = false;
    bool linked_on_ = false;
    QString linked_ref_;
    bool focus_mode_ = false;

    // Bulk page
    QLabel* bulk_title_ = nullptr;
    QLabel* bulk_hint_ = nullptr;
    QList<QPushButton*> bulk_btns_;
    bool bulk_any_unpinned_ = true;
    bool bulk_all_archived_ = false;
    int bulk_count_ = 0;

    // Empty page
    fincept::ui::EmptyState* empty_ = nullptr;
    QLabel* empty_or_ = nullptr;
    QList<QPushButton*> template_btns_;
    QLabel* empty_keys_ = nullptr;
    int empty_last_id_ = 0;
    bool empty_notebook_ = true;
    QString empty_last_title_;
};

} // namespace fincept::screens::notes
