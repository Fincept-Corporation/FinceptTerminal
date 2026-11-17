#pragma once
// ConnectorParamForm — one typed control per catalogue param of a connector command:
//   date → date picker (optional dates can stay "(not set)"), enum → combo of the choices,
//   symbol → ui::SymbolSearchBox (sends the picked instrument's Yahoo symbol; a caret index
//   symbol loses its "^" when the param's example has none), country → a country picker that
//   inserts the code system the param uses (ISO2 / ISO3 / name) or free text for lists,
//   int / float → spin boxes when there is a default to start from (else validated text),
//   bool → checkbox, string → line edit; a "path" param gets a file picker, and a "sheet" param of
//   a connector with a "sheets" command gets a sheet picker fed by that command.
// Help is the tooltip, required params are marked, defaults are prefilled, and the inputs are
// remembered per connector + command (ui::InputMemory, view "catalog/<connector>/<command>").
// The argv itself is built by connectors::build_argv (the one implementation of the contract).

#include "services/connectors/ConnectorArgs.h"

#include <QPointer>
#include <QVector>
#include <QWidget>

class QGridLayout;
class QLabel;

namespace fincept::ui {
class InputMemory;
}

namespace fincept::screens::datasources {

class ConnectorParamForm : public QWidget {
    Q_OBJECT
  public:
    explicit ConnectorParamForm(QWidget* parent = nullptr);

    /// Rebuild for `connector` / `command`. `has_sheets_command`: the connector can list sheets.
    void build(const QString& connector, const QString& command, const QVector<fincept::services::connectors::ConnectorParam>& params,
               bool has_sheets_command);
    /// Put `inputs` into the controls (a sample, or a run typed as text).
    void set_inputs(const QVector<fincept::services::connectors::ParamInput>& inputs);
    QVector<fincept::services::connectors::ParamInput> inputs() const;
    /// Start remembering the inputs (call after the defaults / sample are in) and bring back what
    /// was remembered for this connector + command. True when something was applied.
    bool attach_memory();
    void focus_param(int index);
    const QVector<fincept::services::connectors::ConnectorParam>& params() const { return params_; }
    bool empty() const { return params_.isEmpty(); }

  signals:
    /// Enter pressed in a field: run.
    void submitted();

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    QWidget* make_control(const fincept::services::connectors::ConnectorParam& p, int index);
    QWidget* make_path_control(const fincept::services::connectors::ConnectorParam& p, int index);
    void refresh_sheets(const QString& path);
    QString value_of(int index) const;

    QString connector_;
    QString command_;
    QVector<fincept::services::connectors::ConnectorParam> params_;
    QVector<QWidget*> controls_;   ///< the input of each param (for focus / tracking)
    QWidget* body_ = nullptr;
    QGridLayout* grid_ = nullptr;
    QPointer<fincept::ui::InputMemory> memory_;
    int sheet_index_ = -1;
    int path_index_ = -1;
    QString sheets_request_;
};

} // namespace fincept::screens::datasources
