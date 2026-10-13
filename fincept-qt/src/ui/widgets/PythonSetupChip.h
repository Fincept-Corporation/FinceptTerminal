// src/ui/widgets/PythonSetupChip.h
//
// Status-bar chip for the Python environment pipeline once the main window is
// up (early launch at "core ready", or a background package sync).
//
//   idle      hidden
//   running   "PYTHON ENV 63%" + slim bar; tooltip lists every stage with its
//             live detail and elapsed time
//   failed    "PYTHON ENV FAILED · RETRY" — click re-runs the pipeline
//             (only the failed lanes run again; downloads are cached)
//   finished  "PYTHON ENV READY" for a few seconds, then hidden
//
// Pure view over PythonSetupManager::snapshot(); styled through objectName
// selectors set once in refresh_theme().
#pragma once

#include <QWidget>

class QEvent;
class QLabel;
class QMouseEvent;
class QProgressBar;

namespace fincept::ui {

class PythonSetupChip : public QWidget {
    Q_OBJECT
  public:
    explicit PythonSetupChip(QWidget* parent = nullptr);

  protected:
    void mouseReleaseEvent(QMouseEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    enum class Mode { Hidden, Running, Failed, Done };

    void refresh();
    void refresh_theme();
    void set_mode(Mode mode);

    Mode mode_ = Mode::Hidden;
    QLabel* text_ = nullptr;
    QProgressBar* bar_ = nullptr;
};

} // namespace fincept::ui
