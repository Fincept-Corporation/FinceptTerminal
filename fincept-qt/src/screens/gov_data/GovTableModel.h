// src/screens/gov_data/GovTableModel.h
// The rows of a Gov Data table as a model: the panel hands over the JSON rows it already has (an
// O(1) reset — no item per cell), cells are formatted only when the view asks for them (the visible
// ones), and the ink of a cell is resolved at paint time from the live theme, so a theme change
// needs no recolour walk. Depends on QtCore / QtGui only (no theme, no widgets): the panel supplies
// the cell texts and the ink colours.
#pragma once

#include <QAbstractTableModel>
#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QStringList>

#include <functional>

namespace fincept::screens {

class GovTableModel : public QAbstractTableModel {
  public:
    /// How a cell is inked: the view's text colour, the provider accent, or secondary text.
    enum class Ink { Default, Accent, Secondary };
    /// The text a cell contributes to a CSV export (the URL behind "↗ OPEN"); falls back to the display text.
    static constexpr int ExportRole = Qt::UserRole + 40;

    using TextFn = std::function<QString(const QJsonObject& row)>;
    using TestFn = std::function<bool(const QJsonObject& row)>;
    using InkFn = std::function<QColor(Ink ink)>;

    /// One column. Members in declaration order (designated initialisers).
    struct Column {
        TextFn text{};                                         ///< display text (required)
        TextFn tooltip{};                                      ///< empty = no tooltip
        Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter;
        Ink ink = Ink::Default;
        TestFn ink_if{};                                       ///< empty = always (when ink != Default)
        TextFn export_text{};                                  ///< empty = the display text
    };

    explicit GovTableModel(QObject* parent = nullptr);

    /// The columns (set once, before the model goes on a view).
    void set_columns(const QList<Column>& columns);
    /// Header texts, one per column (re-set on a language change; the visible cells repaint too).
    void set_headers(const QStringList& headers);
    /// Resolves Accent / Secondary to a colour at paint time (the panel's live theme lookup).
    void set_ink(InkFn ink);

    /// Replace the rows (implicitly shared with the caller — no copy, no per-cell work).
    void set_rows(const QJsonArray& rows);
    void clear_rows();
    const QJsonArray& rows() const { return rows_; }
    /// Row `r` as an object (empty when out of range).
    QJsonObject row(int r) const;

    /// The theme changed: the views repaint the visible cells with the new ink.
    void refresh_ink();

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

  private:
    QList<Column> columns_;
    QStringList headers_;
    QJsonArray rows_;
    InkFn ink_;
};

} // namespace fincept::screens
