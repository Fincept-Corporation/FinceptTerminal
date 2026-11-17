// src/screens/notes/NotesEditor.h
//
// The right-hand pane of the Notes screen: one note, always editable.
//
//   bar      pin · WRITE / SPLIT / READ · save state · more actions
//   title    large, borderless
//   meta     category · priority · sentiment
//   links    ticker chips (with the live quote when the terminal has one) and tag
//            chips, each with quick add / remove; tickers are added through the
//            shared symbol picker (ui::SymbolSearchBox, compact) — never typed
//            text taken as a symbol
//   body     Markdown source (syntax highlighted, undo / redo) and / or the
//            rendered preview, re-rendered off the typing path (debounced)
//   footer   words · characters · created / edited
//
// The editor never touches the database: the screen reads it with collect()
// and is told about every change through edited().
#pragma once

#include "storage/repositories/NotesRepository.h"

#include <QHash>
#include <QStringList>
#include <QUrl>
#include <QWidget>

class QComboBox;
class QCompleter;
class QFrame;
class QLabel;
class QLineEdit;
class QMenu;
class QSplitter;
class QStringListModel;
class QTextBrowser;
class QTimer;
class QToolButton;
class QAction;

namespace fincept::ui {
class SymbolSearchBox;
}

namespace fincept::screens::notes {

class NotesBodyEdit;
class NotesHighlighter;
class FlowLayout;

/// Translated display label of a category code ("TRADE_IDEA" -> "Trade ideas").
QString category_label(const QString& code);
/// The category codes the editor offers, in menu order.
QStringList category_codes();

class NotesEditor : public QWidget {
    Q_OBJECT
  public:
    enum class Mode { Write = 0, Split = 1, Read = 2 };
    enum class SaveState { Idle, Unsaved, Saving, Saved, Failed };

    explicit NotesEditor(QWidget* parent = nullptr);
    ~NotesEditor() override;

    /// Show `note` (id 0 = a new note not stored yet). Resets undo history.
    void load(const fincept::FinancialNote& note);
    /// The editor's fields written over `base` (the stored note), keeping what the
    /// editor does not show (archived, colour, reminder...). Title falls back to
    /// the first line of the body.
    fincept::FinancialNote collect(const fincept::FinancialNote& base) const;
    /// Nothing typed: empty title, body, tags and tickers.
    bool is_blank() const;
    /// Bumped on every edit by the user.
    quint64 revision() const { return revision_; }

    void set_mode(Mode m);
    Mode mode() const { return mode_; }
    void set_save_state(SaveState s, const QString& detail = {});
    void set_pinned(bool pinned);
    void set_archived(bool archived);
    /// Replace the tag chips (a bulk "add tag" on the open note); no edit signal.
    void set_tags(const QStringList& tags);
    void set_dates(const QString& created_at, const QString& updated_at);
    void set_highlight(const QStringList& terms);
    /// A quote for a linked ticker's chip; `price` empty clears it.
    void set_quote(const QString& symbol, const QString& price, double change_pct, bool has_change);
    void clear_quotes();
    /// Tag suggestions (most used first). Ticker suggestions come from the symbol picker.
    void set_completions(const QStringList& tickers, const QStringList& tags);
    /// Tickers the note links: its chips, then $CASHTAGS in the body.
    QStringList linked_tickers() const;
    /// The note's own ticker chips (not the $CASHTAGS of the text).
    QStringList explicit_tickers() const { return explicit_tickers_; }
    /// Link `symbol` (a dropped symbol, a picked instrument); an edit.
    void add_ticker(const QString& symbol);
    /// Replace the chip `from` with `to` (the "X is now Y" offer); an edit.
    void replace_ticker(const QString& from, const QString& to);

    void focus_title();
    void focus_body();
    void refresh_theme();
    void retranslate();

  signals:
    void edited();
    void tickers_changed(const QStringList& tickers);
    void pin_toggled(bool pinned);
    void mode_changed(int mode);
    void symbol_clicked(const QString& symbol);
    void link_clicked(const QUrl& url);
    /// "duplicate", "export_fm", "export_dir", "save_template", "revert", "archive", "delete"
    void action_requested(const QString& action);
    /// A short message for the status bar (symbols that could not be resolved).
    void notice(const QString& text);

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    void build_ui();
    void ensure_preview();
    void render_preview();
    void schedule_preview();
    void update_stats();
    void apply_highlight();
    void mark_edited();
    void rebuild_ticker_chips();
    void rebuild_tag_chips();
    void add_from_input(QLineEdit* input, bool tickers);
    QFrame* make_chip(QWidget* parent, const QString& text_html, const QString& tooltip, bool removable,
                      const QString& key, bool ticker);
    void update_pin_button();
    void update_save_label();
    void fill_combos();
    static void select_code(QComboBox* combo, const QString& code, const QString& fallback);

    // State
    Mode mode_ = Mode::Write;
    SaveState save_state_ = SaveState::Idle;
    QString save_detail_;
    bool loading_ = false;
    bool archived_ = false;
    quint64 revision_ = 0;
    QStringList explicit_tickers_;
    QStringList body_tickers_;
    QStringList tags_;
    QStringList highlight_terms_;
    QHash<QString, QString> quote_html_;
    QHash<QString, QLabel*> ticker_labels_; ///< chip text label per linked ticker
    QString created_at_;
    QString updated_at_;

    // Bar
    QToolButton* pin_btn_ = nullptr;
    QToolButton* mode_btns_[3] = {nullptr, nullptr, nullptr};
    QLabel* save_label_ = nullptr;
    QToolButton* more_btn_ = nullptr;
    QMenu* more_menu_ = nullptr;
    QAction* act_duplicate_ = nullptr;
    QAction* act_export_fm_ = nullptr;
    QAction* act_export_dir_ = nullptr;
    QAction* act_template_ = nullptr;
    QAction* act_revert_ = nullptr;
    QAction* act_archive_ = nullptr;
    QAction* act_delete_ = nullptr;

    // Fields
    QLineEdit* title_ = nullptr;
    QLabel* cap_category_ = nullptr;
    QLabel* cap_priority_ = nullptr;
    QLabel* cap_sentiment_ = nullptr;
    QLabel* cap_tickers_ = nullptr;
    QLabel* cap_tags_ = nullptr;
    QComboBox* category_ = nullptr;
    QComboBox* priority_ = nullptr;
    QComboBox* sentiment_ = nullptr;
    QWidget* ticker_box_ = nullptr;
    QWidget* tag_box_ = nullptr;
    FlowLayout* ticker_flow_ = nullptr;
    FlowLayout* tag_flow_ = nullptr;
    fincept::ui::SymbolSearchBox* ticker_picker_ = nullptr;
    QLineEdit* ticker_input_ = nullptr; ///< the picker's line edit
    QLineEdit* tag_input_ = nullptr;
    QCompleter* tag_completer_ = nullptr;
    QStringListModel* tag_words_ = nullptr;

    QSplitter* body_split_ = nullptr;
    NotesBodyEdit* body_ = nullptr;
    NotesHighlighter* highlighter_ = nullptr;
    QTextBrowser* preview_ = nullptr; // built on first SPLIT / READ
    bool preview_stale_ = true;

    QLabel* stats_label_ = nullptr;
    QLabel* dates_label_ = nullptr;

    QTimer* preview_timer_ = nullptr;
    QTimer* stats_timer_ = nullptr;
};

} // namespace fincept::screens::notes
