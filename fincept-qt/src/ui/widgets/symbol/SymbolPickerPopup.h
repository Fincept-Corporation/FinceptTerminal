#pragma once
// SymbolPickerPopup — the drop-down result list of ui::SymbolSearchBox (symbol system
// WP4). A frameless tool-tip window owned by the box: it never takes focus, the box's
// line edit keeps the keyboard and drives it (Up / Down / PageUp / PageDown / Enter /
// Tab / Esc); the mouse picks a row with one click.
//
// Model / view: one QListView over SymbolResultModel, painted by SymbolRowDelegate
// (no widget per row). Each row: display symbol (the query's matched part in the
// accent colour), venue and currency, an asset-class tag, the name, and a marker for
// "former symbol -> now X", delisted / renamed, "no quote data" or the caller's reason.
// Sizes come from font metrics (base font 9-24 px); colours and fonts only from
// ui/theme, cached and refreshed on ThemeManager::theme_changed (refresh_theme()).
// Not meant to be used on its own.

#include "core/instrument/Instrument.h"
#include "core/instrument/InstrumentRank.h"

#include <QAbstractListModel>
#include <QColor>
#include <QFont>
#include <QFrame>
#include <QPointer>
#include <QString>
#include <QStyledItemDelegate>
#include <QVector>

#include <functional>

class QLabel;
class QListView;

namespace fincept::ui {

/// Whether a result row can be picked.
enum class SymbolRowState : quint8 {
    Pickable, ///< normal
    Marked,   ///< pickable, with a note (e.g. "not tradeable with Zerodha")
    Disabled, ///< shown greyed with the reason; Enter / click do not pick it
};

/// Tone of a row marker / the status line.
enum class SymbolTone : quint8 { Neutral, Busy, Warning, Danger, Success };

struct SymbolRow {
    core::instrument::Instrument instrument;
    core::instrument::MatchKind match = core::instrument::MatchKind::None;
    QString matched_text;
    bool via_former_symbol = false;
    SymbolRowState state = SymbolRowState::Pickable;
    QString marker;                       ///< short note shown at the right of the name line
    SymbolTone marker_tone = SymbolTone::Neutral;
};

class SymbolResultModel : public QAbstractListModel {
  public:
    using QAbstractListModel::QAbstractListModel;

    /// `code_query` / `name_query`: what to emphasise in the symbol / the name.
    void set_rows(QVector<SymbolRow> rows, const QString& code_query, const QString& name_query);
    const SymbolRow* row_at(int row) const;
    QString code_query() const { return code_query_; }
    QString name_query() const { return name_query_; }

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;

  private:
    QVector<SymbolRow> rows_;
    QString code_query_;
    QString name_query_;
};

class SymbolRowDelegate : public QStyledItemDelegate {
  public:
    explicit SymbolRowDelegate(QObject* parent = nullptr);

    /// Re-read colours and fonts from the active theme.
    void refresh_theme();
    int row_height() const { return row_height_; }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

  private:
    QFont symbol_font_;
    QFont meta_font_;
    QFont tag_font_;
    int row_height_ = 40;
    int pad_h_ = 8;
    int pad_v_ = 4;
    QColor text_, text2_, text3_, accent_, accent_bg_, tag_bg_, tag_border_, warning_, danger_, success_;
};

class SymbolPickerPopup : public QFrame {
  public:
    /// `owner`: the box; the popup is a top-level tool-tip window parented to it.
    explicit SymbolPickerPopup(QWidget* owner);

    void set_rows(QVector<SymbolRow> rows, const QString& code_query, const QString& name_query, int current);
    /// One line above the rows ("Searching…", "Recent", a note, an error); empty hides it.
    void set_status(const QString& text, SymbolTone tone);
    QString status_text() const { return status_text_; }

    int count() const;
    int current() const;
    void set_current(int row);
    void move_current(int delta);
    const SymbolRow* row_at(int row) const;
    const SymbolRow* current_row() const;
    QString code_query() const;

    /// Size for the rows / status and place under `anchor` (above it when the screen has no room).
    void place(QWidget* anchor);
    void refresh_theme();
    int visible_rows() const { return kMaxVisibleRows; }

    QListView* view() const { return view_; }
    /// Mouse click on a row.
    std::function<void(int row)> on_clicked;

  private:
    static constexpr int kMaxVisibleRows = 8;
    void apply_styles();

    QLabel* status_ = nullptr;
    QListView* view_ = nullptr;
    SymbolResultModel* model_ = nullptr;
    SymbolRowDelegate* delegate_ = nullptr;
    QString status_text_;
    SymbolTone status_tone_ = SymbolTone::Neutral;
    QPointer<QWidget> anchor_;
};

} // namespace fincept::ui
