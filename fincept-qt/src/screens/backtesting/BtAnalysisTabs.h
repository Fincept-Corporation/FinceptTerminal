// src/screens/backtesting/BtAnalysisTabs.h
//
// The heavy analyses of the Backtesting screen as tabs of their own: OPTIMISE, WALK-FWD, MONTE CARLO.
// Each is a settings strip (what only that analysis needs) + its primary button + a cost / time note,
// over a ui::AutoLoadPane in Policy::Manual (never runs by itself): on open it shows the last result of
// this engine + strategy from the run store with "as of" (or a designed preview with one RUN button),
// "Inputs changed — RUN AGAIN" when the setup or the settings differ from that result's, a skeleton with
// the streamed progress while running, CANCEL at once, a time-out, and a failure page with the engine's
// sentence + DETAILS. The result views are BtOptimizeView / BtWalkForwardView / BtMonteCarloView.
#pragma once

#include "screens/backtesting/BtHost.h"
#include "screens/backtesting/BtRunController.h"
#include "ui/widgets/EmptyState.h"

#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QGridLayout;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QSpinBox;

namespace fincept::ui {
class AutoLoadPane;
class InputMemory;
} // namespace fincept::ui

namespace fincept::screens::bt {

class BtOptimizeView;
class BtWalkForwardView;
class BtMonteCarloView;

/// Title of a failure page by contract error class (DATA "No market data", INPUT "Check the inputs", …).
QString bt_failure_title(const QString& error_code);

/// min / max / step per numeric parameter of the strategy, remembered per engine + strategy.
class BtRangeEditor : public QWidget {
    Q_OBJECT
  public:
    explicit BtRangeEditor(QWidget* parent = nullptr);
    void set_params(const QString& engine, const QString& strategy_id,
                    const QVector<services::backtest::StrategyParam>& params);
    /// {name: {min, max, step}} of the rows (numeric parameters only).
    QJsonObject ranges() const;
    /// Number of combinations of the ranges (product of the value counts).
    qint64 combinations() const;
    bool empty() const { return rows_.isEmpty(); }

  signals:
    void changed();

  private:
    struct Row {
        QString name;
        QDoubleSpinBox* min = nullptr;
        QDoubleSpinBox* max = nullptr;
        QDoubleSpinBox* step = nullptr;
        QLabel* count = nullptr;
    };
    void update_counts();
    QGridLayout* grid_ = nullptr;
    QLabel* empty_note_ = nullptr;
    QVector<Row> rows_;
    ui::InputMemory* memory_ = nullptr;
    QString key_;
};

/// Shared base of the three analysis tabs.
class BtAnalysisTab : public QWidget {
    Q_OBJECT
  public:
    BtAnalysisTab(BtHost* host, BtKind kind, QWidget* parent = nullptr);
    ~BtAnalysisTab() override;

    /// The setup / strategy / engine changed: new view key, signature, preview and note.
    void refresh();
    /// The primary action (RUN …; Ctrl+Enter on this tab).
    void run();
    /// Cancel this tab's run (queued or running).
    void cancel();
    bool is_running() const;
    quint64 token() const { return token_; }
    BtKind kind() const { return kind_; }
    /// The parsed result on screen, if any.
    BtBundlePtr current() const { return current_; }

  signals:
    void running_changed(bool running);

  protected:
    virtual QJsonObject build_request(const services::backtest::BtSetup& s) const = 0;
    virtual void render(const BtBundlePtr& bundle) = 0;
    /// "25 combinations · about 12 s · 4 workers".
    virtual QString work_note(const services::backtest::BtSetup& s) const = 0;
    virtual ui::EmptyState::Spec preview_spec(const services::backtest::BtSetup& s) const = 0;
    /// Why this analysis cannot run now in addition to the setup's own problems (empty = fine).
    virtual QString prerequisite_problem() const { return {}; }
    virtual int timeout_seconds() const { return 1800; }
    /// Settings changed: signature + note refresh.
    void settings_changed();

    BtHost* host_ = nullptr;
    BtKind kind_;
    QWidget* strip_ = nullptr;          ///< settings strip (wrapping); subclasses add fields with add_field()
    ui::InputMemory* memory_ = nullptr; ///< "optimize" | "walk_forward" | "monte_carlo"
    ui::AutoLoadPane* pane_ = nullptr;
    QWidget* extra_ = nullptr;          ///< under the strip, above the pane (range editor)
    void add_field(const QString& caption, QWidget* editor, const QString& tooltip = {});

  private:
    void start(quint64 pane_request);
    void show_bundle(const BtBundlePtr& b);
    void update_note();
    QString run_text() const;

    QPushButton* run_btn_ = nullptr;
    QLabel* note_ = nullptr;
    QLabel* as_of_ = nullptr;
    quint64 token_ = 0;
    quint64 pane_request_ = 0;
    BtBundlePtr current_;
    QString view_key_;
};

class BtOptimizeTab : public BtAnalysisTab {
    Q_OBJECT
  public:
    explicit BtOptimizeTab(BtHost* host, QWidget* parent = nullptr);

  protected:
    QJsonObject build_request(const services::backtest::BtSetup& s) const override;
    void render(const BtBundlePtr& bundle) override;
    QString work_note(const services::backtest::BtSetup& s) const override;
    ui::EmptyState::Spec preview_spec(const services::backtest::BtSetup& s) const override;
    QString prerequisite_problem() const override;

  private:
    QComboBox* objective_ = nullptr;
    QComboBox* method_ = nullptr;
    QSpinBox* max_runs_ = nullptr;
    QComboBox* holdout_ = nullptr;
    QSpinBox* seed_ = nullptr;
    BtRangeEditor* ranges_ = nullptr;
    BtOptimizeView* view_ = nullptr;
};

class BtWalkForwardTab : public BtAnalysisTab {
    Q_OBJECT
  public:
    explicit BtWalkForwardTab(BtHost* host, QWidget* parent = nullptr);

  protected:
    QJsonObject build_request(const services::backtest::BtSetup& s) const override;
    void render(const BtBundlePtr& bundle) override;
    QString work_note(const services::backtest::BtSetup& s) const override;
    ui::EmptyState::Spec preview_spec(const services::backtest::BtSetup& s) const override;
    QString prerequisite_problem() const override;

  private:
    QSpinBox* windows_ = nullptr;
    QSpinBox* train_ = nullptr;
    QComboBox* mode_ = nullptr;
    QSpinBox* purge_ = nullptr;
    QSpinBox* embargo_ = nullptr;
    QComboBox* objective_ = nullptr;
    BtRangeEditor* ranges_ = nullptr;
    BtWalkForwardView* view_ = nullptr;
};

class BtMonteCarloTab : public BtAnalysisTab {
    Q_OBJECT
  public:
    explicit BtMonteCarloTab(BtHost* host, QWidget* parent = nullptr);

  protected:
    QJsonObject build_request(const services::backtest::BtSetup& s) const override;
    void render(const BtBundlePtr& bundle) override;
    QString work_note(const services::backtest::BtSetup& s) const override;
    ui::EmptyState::Spec preview_spec(const services::backtest::BtSetup& s) const override;
    QString prerequisite_problem() const override;
    int timeout_seconds() const override { return 300; }

  private:
    QComboBox* method_ = nullptr;
    QSpinBox* paths_ = nullptr;
    QSpinBox* block_ = nullptr;
    QSpinBox* ruin_ = nullptr;
    QSpinBox* seed_ = nullptr;
    BtMonteCarloView* view_ = nullptr;
};

} // namespace fincept::screens::bt
