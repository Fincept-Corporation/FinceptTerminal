#pragma once
// HarnessListWidget.h — every agent harness the terminal knows (Claude Code,
// Codex, Grok, …), one table row each, with the selected harness's settings
// below. The SAME widget is the Agent Workbench's HARNESSES tab and the
// "Agent harnesses" section of Settings › AI Agents, and it reads and writes
// harness::HarnessRegistry only — a default changed in one place shows in the
// other at once (status_changed / defaults_changed).
//
//   row      state dot + word, name (+ BETA), version, sign-in state · method ·
//            plan, default model, default effort, "Use for terminal AI"
//            (stored now; Chat and the other AI features use it in a later
//            release), default harness, path
//   details  status sentence with INSTALL GUIDE / HOW TO SIGN IN (opens the
//            vendor page; the commands can be copied — the terminal never signs
//            in and never asks for a key), API key in the environment + the
//            per-harness policy (inherit | subscription only), default model and
//            effort, path override, RE-DETECT
//
// Styles itself (one object-name stylesheet on its root, set before any child
// exists), so it looks right in either host. Model/view: no widget per row.

#include <QAbstractTableModel>
#include <QStringList>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableView;

namespace fincept::screens {

class HarnessTableModel : public QAbstractTableModel {
    Q_OBJECT
  public:
    enum Column { ColState = 0, ColName, ColVersion, ColSignIn, ColModel, ColEffort, ColTerminal, ColDefault, ColPath, ColCount };
    static constexpr int kIdRole = Qt::UserRole + 1;

    explicit HarnessTableModel(QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    QString id_at(int row) const;
    int row_of(const QString& id) const;
    void refresh_row(const QString& id);
    void refresh_all();
    void retranslate_headers();

  private:
    QStringList ids_;
};

class HarnessListWidget : public QWidget {
    Q_OBJECT
  public:
    explicit HarnessListWidget(QWidget* parent = nullptr);

    /// Select a harness's row (e.g. from a "HOW TO SIGN IN" link elsewhere).
    void select_harness(const QString& id);

  protected:
    void showEvent(QShowEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    void build();
    void retranslate();
    void fit_table();
    void refresh_summary();
    void refresh_detail();
    QString selected_id() const;
    void on_full_access_toggled(bool on);
    void on_apply_path();
    void on_browse_path();
    void on_model_committed();
    void on_link(const QString& link);

    // Header
    QLabel* title_ = nullptr;
    QLabel* summary_ = nullptr;
    QCheckBox* full_access_check_ = nullptr;
    QPushButton* redetect_all_btn_ = nullptr;

    QTableView* table_ = nullptr;
    HarnessTableModel* model_ = nullptr;

    // Details of the selected harness
    QLabel* name_lbl_ = nullptr;
    QLabel* state_lbl_ = nullptr;
    QLabel* model_lbl_ = nullptr;
    QComboBox* model_combo_ = nullptr;
    QLabel* effort_lbl_ = nullptr;
    QComboBox* effort_combo_ = nullptr;
    QLabel* keys_lbl_ = nullptr;
    QComboBox* policy_combo_ = nullptr;
    QLabel* keys_note_ = nullptr;
    QLabel* path_lbl_ = nullptr;
    QLineEdit* path_edit_ = nullptr;
    QPushButton* browse_btn_ = nullptr;
    QPushButton* apply_path_btn_ = nullptr;
    QCheckBox* terminal_check_ = nullptr;
    QLabel* terminal_hint_ = nullptr; // word-wrapped (a check box label cannot wrap)
    QCheckBox* default_check_ = nullptr;
    QPushButton* redetect_btn_ = nullptr;
    QPushButton* install_btn_ = nullptr;
    QPushButton* signin_btn_ = nullptr;
    QLabel* notes_lbl_ = nullptr;

    QString shown_id_; // the harness the details show
};

} // namespace fincept::screens
