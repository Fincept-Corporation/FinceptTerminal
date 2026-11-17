// src/screens/equity_research/EquityResearchUi.h
//
// Building blocks shared by the Equity Research sub-tabs so every tab looks and behaves
// the same: one stylesheet keyed by objectName, a status strip with the same loading /
// error + RETRY / empty / note states everywhere, a segmented toggle, a table with a
// frozen first column, and CSV export that writes raw numbers (zeros kept).
//
// Everything reads colours and fonts from the active theme and is re-applied on
// ThemeManager::appearance_settled (a stylesheet only when its text changed).
#pragma once

#include <QFrame>
#include <QHash>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <functional>

class QAbstractButton;
class QButtonGroup;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QStandardItemModel;
class QTableView;
class QVBoxLayout;

namespace fincept::screens::er_ui {

/// Item role holding a cell's raw number (QStandardItem merges Display and Edit roles):
/// read by CSV export and by in-cell delegates (BarCellDelegate::set_value_role).
inline constexpr int kRawRole = Qt::UserRole + 77;
/// Horizontal header role holding a column's ISO period end, appended to CSV headers.
inline constexpr int kHeaderDateRole = Qt::UserRole + 78;

/// Rules for the objectNames below (append to a tab's own sheet):
///   erPanel (QFrame card)  erPanelTitle  erKey  erValue  erValueBig  erNote  erWarn
///   erSeg (checkable toggle button)  erBtn (accent outline button)  erTable (QTableView)
///   QLabel[tone="good"|"bad"|"warn"|"muted"] — status tones (success / danger, never
///   the price-direction colours).
QString stylesheet();

/// setStyleSheet(css()) now and again once every theme / font change has settled.
void apply_style(QWidget* w, std::function<QString()> css);

/// A titled card (objectName erPanel) with a vertical body; `title_out` receives the title
/// label for retranslation.
QFrame* panel(const QString& title, QVBoxLayout** body, QLabel** title_out = nullptr, QWidget* parent = nullptr);

/// Status line used by every sub-tab: loading, error (+ RETRY), empty, or an info note.
class StatusStrip : public QFrame {
    Q_OBJECT
  public:
    explicit StatusStrip(QWidget* parent = nullptr);
    void show_loading(const QString& text);
    /// `detail` goes to the tooltip (the full reason).
    void show_error(const QString& text, const QString& detail = {});
    void show_empty(const QString& text);
    void show_info(const QString& text, const QString& detail = {});
    void clear();
    bool is_error() const { return mode_ == 2; }

  signals:
    void retry_requested();

  protected:
    void changeEvent(QEvent* e) override;

  private:
    void set_mode(int mode, const QString& text, const QString& detail);
    QLabel* text_ = nullptr;
    QPushButton* retry_ = nullptr;
    int mode_ = 0; ///< 0 hidden, 1 loading, 2 error, 3 empty, 4 info
};

/// A row of exclusive checkable buttons (objectName erSeg).
class Segmented : public QWidget {
    Q_OBJECT
  public:
    explicit Segmented(QWidget* parent = nullptr);
    void add(const QString& key, const QString& text, const QString& tooltip = {});
    void set_text(const QString& key, const QString& text, const QString& tooltip = {});
    void set_current(const QString& key, bool emit_signal = false);
    void set_enabled(const QString& key, bool enabled);
    QString current() const { return current_; }

  signals:
    void changed(const QString& key);

  private:
    QHBoxLayout* layout_ = nullptr;
    QHash<QString, QPushButton*> buttons_;
    QString current_;
};

/// A table whose first column stays put while the rest scrolls horizontally: two views
/// over one model, sharing the vertical scroll position and the selection.
class FrozenTable : public QWidget {
    Q_OBJECT
  public:
    explicit FrozenTable(QWidget* parent = nullptr);
    QStandardItemModel* model() const { return model_; }
    QTableView* frozen_view() const { return frozen_; }
    QTableView* main_view() const { return main_; }
    /// Re-fit the frozen column and the value columns after the model changed.
    void refit(int min_value_width = 0);
    /// Raw-number CSV: header row from the model's horizontal headers; each cell's EditRole
    /// number when it has one (a reported zero is written as 0), else its text.
    void csv(QStringList& headers, QVector<QStringList>& rows, const QVector<int>& skip_columns = {}) const;

  protected:
    void changeEvent(QEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* e) override;

  private:
    void sync_geometry();
    /// Widen the value columns (not the in-cell chart at column 1) to fill a viewport
    /// wider than the fitted table, so the rows do not stop half-way across the screen.
    void spread();
    QStandardItemModel* model_ = nullptr;
    QTableView* frozen_ = nullptr;
    QTableView* main_ = nullptr;
    QVector<int> fit_widths_; ///< per column, as fitted by refit()
};

/// The home-market benchmark index of a listing (VOD.L → ^FTSE "FTSE 100", 7203.T → ^N225,
/// RELIANCE.NS → ^NSEI, a bare US ticker → ^GSPC). Empty symbol when unknown.
struct Benchmark {
    QString symbol;
    QString name;
};
Benchmark benchmark_for(const QString& symbol);

/// Locale-independent CSV number ("1234567.89", "0"); empty for NaN / inf.
QString csv_number(double v);
/// Ask for a file and write the table (UTF-8, RFC 4180 quoting). Returns false when the
/// user cancelled or the write failed (a message box says why).
bool export_csv(QWidget* parent, const QString& suggested_name, const QStringList& headers,
                const QVector<QStringList>& rows);

} // namespace fincept::screens::er_ui
