#pragma once
// AddToReportButton — the shared "Add to Report" action any tab drops above a result.
//
//   auto* add = new fincept::ui::AddToReportButton(QStringLiteral("Derivatives"), toolbar);
//   add->set_payload_provider([this] { return build_report_payload(); });   // GUI thread
//
// Main click: add to the report open in the Report Builder (one undo step there). The
// arrow: pick the target — the open report, a recent library report, or a new report.
// Afterwards an inline "Added to “Title” · Open" link (or the reason it failed) shows for
// a few seconds; "Open" switches to the Report Builder on the new blocks. The provider
// only reads results the tab already computed; storing images and writing the library
// happen off the GUI thread (services::ReportInbox). Theme tokens only.
//
// add_send_to_report_action() is the same thing as a context-menu entry.

#include "core/report/ReportBlocks.h"
#include "services/report_builder/ReportInbox.h"

#include <QWidget>

#include <functional>

class QLabel;
class QMenu;
class QTimer;
class QToolButton;

namespace fincept::ui {

class AddToReportButton : public QWidget {
    Q_OBJECT
  public:
    using PayloadProvider = std::function<fincept::report::ReportPayload()>;

    explicit AddToReportButton(const QString& source_label, QWidget* parent = nullptr);

    void set_payload_provider(PayloadProvider provider);
    /// Defaults: heading + provenance, no reveal.
    void set_options(const fincept::services::AppendOptions& options);
    /// Hide the inline result text (e.g. in a crowded toolbar; a toast still reports it).
    void set_show_result(bool on);

    /// Send to `target` now (also what the menu entries call).
    void send(const fincept::services::ReportTarget& target);

  signals:
    void appended(const fincept::services::AppendResult& result);

  protected:
    void changeEvent(QEvent* e) override;

  private:
    void populate_menu();
    void show_result(const fincept::services::AppendResult& r);
    void retranslate();

    QString source_label_;
    PayloadProvider provider_;
    fincept::services::AppendOptions options_;
    QToolButton* button_ = nullptr;
    QMenu* menu_ = nullptr;
    QLabel* result_ = nullptr;
    QTimer* result_timer_ = nullptr;
    bool show_result_ = true;
    bool busy_ = false;
    quint64 menu_request_ = 0;
};

/// A context-menu entry "Add to report" (+ "Add to a new report") that sends
/// `provider()`'s payload; feedback through the toast service.
void add_send_to_report_action(QMenu* menu, const QString& source_label, AddToReportButton::PayloadProvider provider);

} // namespace fincept::ui
