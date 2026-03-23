// src/screens/setup/SetupScreen.h
// First-run setup screen — shown when Python environment is not installed.
// Displays setup steps with progress bars and a BEGIN SETUP button.
#pragma once

#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::python {
struct SetupProgress;
}

namespace fincept::screens {

class SetupScreen : public QWidget {
    Q_OBJECT
  public:
    explicit SetupScreen(QWidget* parent = nullptr);

  signals:
    void setup_complete();

  private slots:
    void on_begin_setup();
    void on_progress(const fincept::python::SetupProgress& progress);
    void on_setup_done(bool success, const QString& error);

  private:
    void build_ui();
    QWidget* build_step_row(const QString& label, const QString& object_name);

    QPushButton* begin_btn_ = nullptr;
    QLabel* status_label_ = nullptr;

    // Step progress bars (keyed by step name)
    struct StepUI {
        QLabel* label = nullptr;
        QProgressBar* bar = nullptr;
        QLabel* status = nullptr;
    };
    QMap<QString, StepUI> steps_;
};

} // namespace fincept::screens
