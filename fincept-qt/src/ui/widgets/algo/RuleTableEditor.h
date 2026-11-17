// src/ui/widgets/algo/RuleTableEditor.h
#pragma once
// RuleTableEditor — the compact rule table of the Algo Trading tab (Builder entry / exit,
// Scanner condition set, Alerts composer). ONE line per condition, columns aligned:
//
//   #  IF ……………………… [params]  IS ……………  THAN ………………… [params]  AGO   ✕
//   1  SMA ▾            50        crosses above  SMA ▾          200       0 · 0  ✕
//   2  RSI ▾            14        <              Value ▾        30        0      ✕
//   ┃ GROUP 3 · ANY ▾ · NOT ☐                                   + RULE   ✕
//   ┃ 3.1 Close ▾                 >              EMA ▾          20        0 · 0  ✕
//   Enter when SMA(50) crosses above SMA(200) and RSI(14) < 30 and (Close > EMA(20))
//
// * Operands: one combo lists every indicator OUTPUT ("MACD signal line", "Bollinger lower");
//   parameters in one compact field ("12, 26, 9"; tooltip names them, the range is checked).
//   The right operand's combo starts with "Value" (a number; "between" takes two).
// * Keyboard: Enter in a row adds a rule below it (inside its group) and focuses it; Delete on
//   a row's combo (Ctrl+Delete anywhere in the row) removes it; Tab walks the row left to right.
// * Every row is checked (services::algo::rules::check_conditions): a problem marks the row
//   (danger "!" with the reason as tooltip / accessible description) and is listed under the
//   table; the plain-language sentence (rules_sentence) sits under the rows.
// * JSON in / out is exactly ConditionSection's (the engine's ConditionEvaluator format); an
//   empty table is an empty array (there is NO placeholder row).
// * Styling: object names of the Algo kit root stylesheet (#algoField inputs, #algoRuleGroup,
//   #algoSentence, #algoNote, tone property) — no setStyleSheet; sizes from font metrics.

#include "services/algo_trading/AlgoRuleLogic.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QWidget>

class QComboBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QVBoxLayout;

namespace fincept::ui::algo {

class RuleTableEditor : public QWidget {
    Q_OBJECT
  public:
    enum class Kind { Entry, Exit, Scan, Alert };

    explicit RuleTableEditor(Kind kind, QWidget* parent = nullptr);
    ~RuleTableEditor() override;

    /// Condition JSON (ConditionEvaluator format), groups as {children, logic, negate}.
    QJsonArray conditions() const;
    /// "AND" | "OR" — how the top-level rows combine.
    QString combined_logic() const;
    /// Replaces every row (an empty array leaves the table empty). Emits conditions_changed once.
    void set_conditions(const QJsonArray& conditions, const QString& logic);
    void clear_all();

    /// The sentence under the table ("Enter when …").
    QString sentence() const;
    /// The current problems (row-located) and whether one blocks RUN / SAVE.
    QVector<fincept::services::algo::rules::RuleIssue> issues() const;
    bool has_blocking_issue() const;
    int row_count() const;

    /// Controls for the host's panel header: [ALL | ANY] segments, + RULE, + GROUP (parented to
    /// this editor until the host adds it to its header).
    QWidget* header_controls() const { return header_controls_; }

    /// Append a rule (SMA(20) crosses above SMA(50) for Entry / Scan / Alert, below for Exit) at
    /// the end, or inside group `group_index`; focuses its first field.
    void add_rule();
    void add_group();
    /// Focus the first row's first field (or + RULE when empty).
    void focus_first();
    /// Kind-specific sentence prefix ("Enter when", "Exit when", "Match when", "Alert when").
    Kind kind() const { return kind_; }

  signals:
    /// Any edit (debounced to one emission per event-loop turn).
    void conditions_changed();

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    struct Row;
    struct Group;
    struct Node;
    Row* make_row(QWidget* parent, const QJsonObject& c, int group);
    void destroy_row(Row* r);
    Group* make_group(const QJsonObject& g);
    void insert_rule(int top_index, int group_child_index, int group, const QJsonObject& c, bool focus);
    void remove_row(Row* r);
    void remove_group(int group);
    void relayout();
    void refresh_row_widgets(Row* r);
    QJsonObject row_json(const Row* r) const;
    void schedule_changed();
    void update_feedback();
    void retranslate();
    QJsonObject default_rule() const;
    void populate_operand_combo(QComboBox* c, bool with_value) const;
    void set_operand(QComboBox* c, QLineEdit* params, const QString& indicator, const QString& field,
                     const QJsonObject& p) const;
    Row* row_of(QObject* o) const;

    Kind kind_;
    QString logic_ = QStringLiteral("AND");
    QVector<Node*> nodes_; ///< top level in order (a rule or a group)
    QWidget* header_controls_ = nullptr;
    QWidget* logic_seg_ = nullptr;
    QPushButton* add_rule_btn_ = nullptr;
    QPushButton* add_group_btn_ = nullptr;
    QWidget* grid_host_ = nullptr;
    QGridLayout* grid_ = nullptr;
    QVector<QLabel*> captions_;
    QLabel* empty_ = nullptr;
    QLabel* sentence_ = nullptr;
    QLabel* problems_ = nullptr;
    QVector<fincept::services::algo::rules::RuleIssue> issues_;
    bool change_pending_ = false;
    bool loading_ = false;
};

} // namespace fincept::ui::algo
