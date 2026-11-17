#pragma once
// MaDealGrid — the M&A tab's editable table of deals / comparables (replaces the JSON text boxes): typed columns
// over the shared ui::DataGridEditor (type, paste from a spreadsheet, import CSV, row validation with the reason
// on each bad row), plus what only M&A has: LOAD VERIFIED DEALS / LOAD ALL DEALS from the Deal Database, the
// labelled sample deals, and PASTE JSON for the old habit of pasting a JSON array.
//
//   Comparison  - acquirer, target, value (millions), premium %, EV/Revenue, EV/EBITDA, cash %, stock %, industry
//   Precedent   - date, acquirer, target, deal value, EV/Revenue, EV/EBITDA, revenue, EBITDA, premium %
//   Regression  - company, EV, revenue, EBITDA, growth %
// Every kind has a SOURCE column ("Sample (illustrative)", "Deal Database (verified)", ...) that is shown and
// exported but never sent to a script.
//
// Persistence: the rows are mirrored as JSON into a hidden QPlainTextEdit (memory_field()) that the panel's
// ui::InputMemory tracks by name, so the grid comes back after a restart like any other input.

#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QPushButton;

namespace fincept::ui {
class DataGridEditor;
}

namespace fincept::screens {

class MaDealGrid : public QWidget {
    Q_OBJECT
  public:
    enum class Kind { Comparison, Precedent, Regression };

    /// `memory_name` = the stable InputMemory key of the hidden mirror (e.g. "cmp_deals_grid").
    MaDealGrid(Kind kind, const QString& memory_name, QWidget* parent = nullptr);

    Kind kind() const { return kind_; }
    /// Valid rows, typed as the scripts expect (percent growth -> fraction for the regression), without SOURCE.
    QJsonArray deals() const;
    int valid_count() const;
    int invalid_count() const;
    int row_count() const;
    /// Every non-empty row as entered (strings), for saving.
    QJsonArray raw_rows() const;
    void set_rows(const QJsonArray& rows);
    void load_sample();

    QString memory_name() const { return memory_name_; }
    QPlainTextEdit* memory_field() const { return mirror_; }

    QStringList headers() const;
    QVector<QStringList> rows_text() const;

  signals:
    void changed();

  private:
    void load_from_database(bool verified_only);
    void on_deals_loaded(quint64 request_id, const QJsonArray& deals);
    void paste_json();
    void sync_mirror();
    void apply_mirror();
    void set_note(const QString& text);
    QJsonObject row_from_deal(const QJsonObject& deal) const;

    Kind kind_;
    QString memory_name_;
    fincept::ui::DataGridEditor* grid_ = nullptr;
    QPlainTextEdit* mirror_ = nullptr;
    QLabel* note_ = nullptr;
    QPushButton* load_verified_btn_ = nullptr;
    QPushButton* load_all_btn_ = nullptr;
    quint64 load_request_ = 0;
    bool load_verified_only_ = true;
    bool syncing_ = false;
};

} // namespace fincept::screens
