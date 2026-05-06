#pragma once
// LoggingSection.h — global log level, JSON output mode, log file path,
// and per-tag level overrides for fincept::Logger.

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

class LoggingSection : public QWidget {
    Q_OBJECT
  public:
    explicit LoggingSection(QWidget* parent = nullptr);

  private:
    void build_ui();

    QComboBox*   log_global_level_ = nullptr;
    QWidget*     log_tag_list_     = nullptr; // VBox container for tag rows
    QVBoxLayout* log_tag_layout_   = nullptr;
    QCheckBox*   log_json_mode_    = nullptr;
    QLabel*      log_path_label_   = nullptr;
};

} // namespace fincept::screens
