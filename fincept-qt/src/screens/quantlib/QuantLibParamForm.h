#pragma once
// QuantLibParamForm.h — typed request-body editor for one QuantLib endpoint.
//
// One row per top-level field of the endpoint's example body: numbers get a
// numeric editor with a unit hint ("decimal · 0.20 = 20%", "years", ...),
// call/put and other known enumerations a drop-down, booleans a check box and
// arrays/objects a compact JSON editor. A raw-JSON mode covers optional fields
// the example does not show. Enter in any field asks to run.

#include <QJsonObject>
#include <QList>
#include <QStringList>
#include <QWidget>

class QGridLayout;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;

namespace fincept::screens {

class QuantLibParamForm : public QWidget {
    Q_OBJECT
  public:
    explicit QuantLibParamForm(QWidget* parent = nullptr);

    /// Rebuilds the form for `endpoint`. `example_json` is the example body (it
    /// fixes each field's type, unit hint, row order and the RESET values);
    /// `values` is the body to show — the user's last inputs for this endpoint,
    /// or empty for the example. `takes_body` is false for GET endpoints.
    void set_endpoint(const QString& endpoint, const QString& example_json, const QJsonObject& values,
                      bool takes_body);

    /// The validated request body. On a validation failure returns false, fills
    /// `error` and moves focus to the offending field.
    bool body(QJsonObject* out, QString* error);

    void retranslate();

  signals:
    /// Enter pressed in a field.
    void run_requested();

  private:
    enum class Kind { Number, Integer, Text, Bool, Choice, Json };
    struct Field {
        QString key;
        Kind kind = Kind::Text;
        bool nullable = false; ///< example value was null: an empty editor sends null
        QWidget* editor = nullptr;
    };

    void rebuild(const QJsonObject& values);
    QWidget* make_editor(Field& field, const QJsonValue& value);
    bool collect(QJsonObject* out, QString* error, QWidget** bad_editor) const;
    void set_json_mode(bool on);
    void reset_to_defaults();
    void update_warnings();
    void mark_invalid(QWidget* editor, bool invalid) const;

    QString endpoint_;
    QJsonObject defaults_;
    QStringList key_order_; ///< the example's top-level keys in source order (QJsonObject sorts them)
    bool takes_body_ = true;
    bool json_mode_ = false;
    QList<Field> fields_;

    QStackedWidget* stack_ = nullptr; // 0 = fields, 1 = raw JSON, 2 = no parameters
    QWidget* fields_page_ = nullptr;
    QGridLayout* grid_ = nullptr;
    QPlainTextEdit* json_edit_ = nullptr;
    QLabel* empty_label_ = nullptr;
    QLabel* warn_label_ = nullptr;
    QLabel* title_label_ = nullptr;
    QPushButton* mode_btn_ = nullptr;
    QPushButton* reset_btn_ = nullptr;
};

} // namespace fincept::screens
