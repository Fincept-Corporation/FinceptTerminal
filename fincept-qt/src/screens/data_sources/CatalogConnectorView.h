#pragma once
// CatalogConnectorView — the right pane of Data Sources for one catalogue connector (a Python
// data script run through the connector contract):
//   key status + ADD KEY (Settings > Credentials, focused on the key), command picker, one typed
//   control per param (ConnectorParamForm; the argv comes from connectors::build_argv) — or, for
//   a command whose params are unknown, fields read from its args hint (free text when the hint
//   has no clear shape) — RUN / CANCEL, and the result with its table picker, exports, Send to
//   Data Mapping and Add to Report. Failures use the contract's codes, each with its action.
// The catalogue's `sample` is run when the connector is opened — only when it is not null and
// nothing was remembered for that command (through the app-wide AutoLoadGate, cancelled when the
// pane is left). Without a sample the designed state shows instead: needs a key (ADD KEY),
// secure storage unreadable (RETRY — never "add a key"), upstream broken (the reason), or what
// to fill in before RUN.

#include "screens/data_sources/DataWorkspace.h"
#include "services/connectors/ConnectorArgs.h"
#include "services/connectors/ConnectorCatalog.h"
#include "services/connectors/ConnectorService.h"

#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QGridLayout;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QTimer;

namespace fincept::ui {
class EmptyState;
class LoadingSkeleton;
} // namespace fincept::ui

namespace fincept::screens::datasources {

class ConnectorParamForm;

class CatalogConnectorView : public QWidget {
    Q_OBJECT
  public:
    explicit CatalogConnectorView(QWidget* parent = nullptr);

    void show_connector(const QString& name);
    QString connector_name() const { return name_; }
    /// Key presence was re-read (e.g. after the user added a key in Settings): update the key
    /// line, and replace a "needs an API key" page by the RUN preview when the key is there now.
    void refresh_key_state();

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void build();
    void schedule_sample();
    void rebuild_inputs();
    void fill_values(const QStringList& args);
    QStringList field_values() const;
    void set_text_mode(bool on);
    void update_key_line();
    void show_idle();
    void show_unreadable_page();
    void show_broken_page();
    /// The default / sample arguments of the current command into the form.
    void fill_command_args(const QStringList& args);
    void start_run(bool automatic);
    void begin_run(const QStringList& args, bool automatic, quint64 seq);
    void finish_gate();
    void cancel_run();
    void on_result(const fincept::services::connectors::ConnectorResult& r);
    void show_error(const fincept::services::datasources::SourceError& e);
    void run_action(ErrorAction action);
    const fincept::services::connectors::ConnectorInfo* info() const;

    QString name_;
    fincept::services::connectors::ArgsForm form_; ///< hint fallback (params unknown)
    bool typed_ = false;                            ///< the current command has catalogue params
    QString command_;
    QString pending_request_;
    quint64 run_seq_ = 0;
    quint64 gate_ticket_ = 0;
    bool automatic_ = false;
    bool wants_sample_ = false; ///< keyless: run the sample when shown and nothing is on screen
    bool showing_unreadable_ = false; ///< the idle page says secure storage could not be read (RETRY re-reads)
    int retry_left_ = 0;
    ErrorAction primary_ = ErrorAction::None;
    ErrorAction secondary_ = ErrorAction::None;
    QString action_key_;

    QLabel* title_ = nullptr;
    QLabel* meta_ = nullptr;
    QLabel* desc_ = nullptr;
    QLabel* key_line_ = nullptr;
    QPushButton* add_key_btn_ = nullptr;
    QLabel* broken_ = nullptr;
    QComboBox* command_box_ = nullptr;
    QLabel* hint_ = nullptr;
    QWidget* fields_box_ = nullptr;
    QGridLayout* fields_grid_ = nullptr;
    QVector<QWidget*> field_inputs_;
    ConnectorParamForm* param_form_ = nullptr;
    QPlainTextEdit* raw_args_ = nullptr;
    QCheckBox* as_text_ = nullptr;
    QCheckBox* force_ = nullptr;
    QPushButton* run_btn_ = nullptr;
    QLabel* input_error_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    fincept::ui::EmptyState* state_ = nullptr;
    QWidget* loading_ = nullptr;
    fincept::ui::LoadingSkeleton* skeleton_ = nullptr;
    ResultPane* result_ = nullptr;
    QTimer* auto_timer_ = nullptr;
    QTimer* retry_timer_ = nullptr;
};

} // namespace fincept::screens::datasources
