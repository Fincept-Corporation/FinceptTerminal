// src/screens/polymarket/PolymarketViewState.h
#pragma once
// PmViewState — one area of the prediction-markets screen (the browse list, a
// detail tab) that shows exactly one of: its content, a loading skeleton in
// the content's shape, or a designed state (no data / failure / setup) built
// from ui::EmptyState. The feeds of this screen are pushed by the exchange
// adapters (not started by the area itself), so the shared ui::AutoLoadPane
// (which owns its start action) does not fit; this keeps the same look with
// the same shared widgets.
//
//   state_ = PmViewState(stack, table, ui::LoadingSkeleton::Shape::Table, tr("Loading trades…"));
//   state_.show_loading();                                   // request sent
//   state_.show_content();                                   // rows arrived
//   state_.show_state(ui::EmptyState::error_spec(msg), [this] { emit reload_requested(); });
//
// The skeleton and the EmptyState are created on first use and reused (each
// parses its stylesheet once). GUI thread. Header-only, no Q_OBJECT.

#include "ui/widgets/EmptyState.h"
#include "ui/widgets/LoadingSkeleton.h"

#include <QPointer>
#include <QStackedWidget>
#include <QString>
#include <QWidget>

#include <functional>
#include <memory>

namespace fincept::screens::polymarket {

class PmViewState {
  public:
    using Action = std::function<void()>;

    PmViewState() = default;
    /// `stack` already holds `content`; the other pages are added to it on first use.
    PmViewState(QStackedWidget* stack, QWidget* content, ui::LoadingSkeleton::Shape shape, const QString& loading_text)
        : stack_(stack), content_(content), shape_(shape), loading_text_(loading_text),
          actions_(std::make_shared<Actions>()) {}

    bool valid() const { return stack_ && content_; }

    void show_content() {
        if (valid())
            stack_->setCurrentWidget(content_);
    }

    void show_loading(const QString& text = QString()) {
        if (!valid())
            return;
        if (!skeleton_) {
            skeleton_ = new ui::LoadingSkeleton(shape_, stack_);
            stack_->addWidget(skeleton_);
        }
        skeleton_->set_message(text.isEmpty() ? loading_text_ : text);
        stack_->setCurrentWidget(skeleton_);
    }

    /// A designed state; `primary` / `secondary` run when its buttons are pressed.
    void show_state(const ui::EmptyState::Spec& spec, Action primary = Action(), Action secondary = Action(),
                    bool compact = false) {
        if (!valid())
            return;
        if (!state_) {
            state_ = new ui::EmptyState(stack_);
            stack_->addWidget(state_);
            // The actions live in a shared block the lambdas hold, so the handlers stay
            // valid whatever happens to this (copyable) helper object.
            std::shared_ptr<Actions> acts = actions_;
            QObject::connect(state_, &ui::EmptyState::primary_clicked, state_, [acts]() {
                if (acts->primary)
                    acts->primary();
            });
            QObject::connect(state_, &ui::EmptyState::secondary_clicked, state_, [acts]() {
                if (acts->secondary)
                    acts->secondary();
            });
        }
        actions_->primary = std::move(primary);
        actions_->secondary = std::move(secondary);
        state_->apply(spec);
        state_->set_compact(compact);
        stack_->setCurrentWidget(state_);
    }

    bool showing_content() const { return valid() && stack_->currentWidget() == content_; }
    bool showing_loading() const { return valid() && skeleton_ && stack_->currentWidget() == skeleton_; }
    bool showing_state() const { return valid() && state_ && stack_->currentWidget() == state_; }
    ui::LoadingSkeleton* skeleton() const { return skeleton_; }

  private:
    struct Actions {
        Action primary;
        Action secondary;
    };

    QPointer<QStackedWidget> stack_;
    QPointer<QWidget> content_;
    ui::LoadingSkeleton::Shape shape_ = ui::LoadingSkeleton::Shape::Table;
    QString loading_text_;
    QPointer<ui::LoadingSkeleton> skeleton_;
    QPointer<ui::EmptyState> state_;
    std::shared_ptr<Actions> actions_;
};

} // namespace fincept::screens::polymarket
