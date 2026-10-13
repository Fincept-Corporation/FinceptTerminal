#pragma once
// ScreenerCriteriaBuilder — the screener's "new screen" form.
//
// Universe pickers (region → exchange, sector → industry, peer group), numeric criteria rows
// (field, operator, value or min/max) and the server-side sort. Page size lives in the results
// toolbar because it applies to predefined screens too. Every choice it offers comes from
// the script's `fields` catalogue, so the UI cannot ask Yahoo for a field, exchange or industry
// value the screener does not actually match.

#include "services/screener/ScreenerService.h"

#include <QHash>
#include <QJsonObject>
#include <QVector>
#include <QWidget>

class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QToolButton;
class QVBoxLayout;

namespace fincept::screens {

class ScreenerCriteriaBuilder : public QWidget {
    Q_OBJECT
  public:
    explicit ScreenerCriteriaBuilder(QWidget* parent = nullptr);

    void set_catalog(const QJsonObject& catalog);
    bool has_catalog() const { return !fields_by_id_.isEmpty(); }

    /// Current definition as a custom query (count/offset untouched). Returns false with a
    /// user-facing *error when a value does not parse or nothing would be filtered; with
    /// `mark_invalid` the offending input is highlighted and focused.
    bool build_query(services::ScreenerQuery* out, QString* error, bool mark_invalid = true);
    /// Show a custom query in the form.
    void load_query(const services::ScreenerQuery& q);
    /// The default new screen: US major exchanges, market cap >= 2B, largest first.
    void reset_to_default();

    /// Plain-English label of a catalogue field id (falls back to the id).
    QString field_label(const QString& id) const;

  signals:
    void edited();        ///< the user changed the definition
    void run_requested(); ///< Enter pressed in a value field

  protected:
    void changeEvent(QEvent* e) override;

  private:
    struct CriterionRow {
        QWidget* box = nullptr;
        QComboBox* field = nullptr;
        QComboBox* op = nullptr;
        QLineEdit* lo = nullptr;
        QLineEdit* hi = nullptr;
        QLabel* unit = nullptr;
        QToolButton* remove = nullptr;
    };

    void build_ui();
    void retranslate();
    void populate_field_combo(QComboBox* combo) const;
    void populate_operator_combo(QComboBox* combo) const;
    void populate_exchanges();
    void populate_industries();
    void add_criterion(const QString& field_id, const QString& op, const QString& lo, const QString& hi);
    void remove_criterion(QWidget* box);
    void clear_criteria();
    void update_row(const CriterionRow& row);
    static void set_invalid(QWidget* w, bool invalid);
    static bool parse_value(const QString& text, double* out);
    static QString format_value(double v);

    QJsonObject catalog_;
    QHash<QString, QJsonObject> fields_by_id_;
    bool loading_ = false; ///< suppresses edited() while the form is filled programmatically

    QLabel* universe_lbl_ = nullptr;
    QLabel* criteria_lbl_ = nullptr;
    QLabel* output_lbl_ = nullptr;
    QFormLayout* universe_form_ = nullptr;
    QFormLayout* output_form_ = nullptr;
    QComboBox* region_ = nullptr;
    QComboBox* exchange_ = nullptr;
    QComboBox* sector_ = nullptr;
    QComboBox* industry_ = nullptr;
    QComboBox* peer_ = nullptr;
    QComboBox* sort_field_ = nullptr;
    QComboBox* sort_dir_ = nullptr;
    QVBoxLayout* rows_layout_ = nullptr;
    QPushButton* add_btn_ = nullptr;
    QLabel* hint_lbl_ = nullptr;
    QVector<CriterionRow> rows_;
};

} // namespace fincept::screens
