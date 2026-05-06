#include "screens/launchpad/OnboardingTour.h"

#include "core/logging/Logger.h"
#include "storage/repositories/SettingsRepository.h"

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace fincept::screens {

namespace {
constexpr const char* kTourTag = "OnboardingTour";
constexpr const char* kSeenKey = "onboarding.tour_seen";

// Process-wide guard so re-entrant show_for() calls don't stack two
// dialogs. Pointer cleared via QObject's destroyed signal in show_for.
QPointer<OnboardingTour> s_active_tour;
} // namespace

bool OnboardingTour::has_been_seen() {
    auto r = SettingsRepository::instance().get(kSeenKey);
    return r.is_ok() && r.value() == "true";
}

void OnboardingTour::mark_seen() {
    auto r = SettingsRepository::instance().set(kSeenKey, "true");
    if (r.is_err())
        LOG_WARN(kTourTag, QString("Failed to set %1: %2")
                              .arg(kSeenKey, QString::fromStdString(r.error())));
}

void OnboardingTour::reset_seen() {
    auto r = SettingsRepository::instance().set(kSeenKey, "false");
    if (r.is_err())
        LOG_WARN(kTourTag, QString("Failed to reset %1: %2")
                              .arg(kSeenKey, QString::fromStdString(r.error())));
}

void OnboardingTour::show_for(QWidget* parent) {
    if (s_active_tour) {
        s_active_tour->raise();
        s_active_tour->activateWindow();
        return;
    }
    auto* tour = new OnboardingTour(parent);
    tour->setAttribute(Qt::WA_DeleteOnClose);
    s_active_tour = tour;
    QObject::connect(tour, &QObject::destroyed, []() { s_active_tour = nullptr; });
    tour->show();
}

OnboardingTour::OnboardingTour(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Welcome to Fincept Terminal"));
    setModal(true);
    setSizeGripEnabled(false);
    resize(560, 360);
    build_ui();
    update_buttons();
}

void OnboardingTour::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 16);
    root->setSpacing(16);

    auto* title = new QLabel(tr("A 30-second tour"), this);
    QFont tf = title->font();
    tf.setPointSizeF(tf.pointSizeF() * 1.4);
    tf.setBold(true);
    title->setFont(tf);
    root->addWidget(title);

    steps_ = new QStackedWidget(this);
    steps_->addWidget(build_step(
        tr("Command bar (Ctrl+\\)"),
        tr("Type a function code or verb to do anything in the terminal — "
           "e.g. \"AAPL\", \"layout switch \\\"Morning\\\"\", or "
           "\"link panel red\". Press Ctrl+K for a fuzzy palette of every "
           "action."),
        tr("Tip: type \"?\" to list available actions for whatever you "
           "type next.")));
    steps_->addWidget(build_step(
        tr("Link panels with a colour"),
        tr("Click the coloured dot in any panel header to add it to a link "
           "group. Panels in the same group share their selected symbol "
           "across windows — pick AAPL in a watchlist and your charts, "
           "research, and trading panels all switch."),
        tr("Tip: groups are shared across windows, not just the active "
           "one.")));
    steps_->addWidget(build_step(
        tr("Tear off panels into new windows"),
        tr("Right-click a panel tab → \"Tear off into new window\" to spawn "
           "a fresh frame on the next monitor. Or drag a panel to another "
           "frame's tab bar to move it. Each frame keeps its own dock "
           "layout — save the whole arrangement as a named layout when "
           "you've got it the way you like."),
        tr("Tip: Ctrl+Shift+N opens a fresh window on your next "
           "monitor.")));
    steps_->addWidget(build_step(
        tr("Settings & shortcuts"),
        tr("Open Settings (gear icon) to tune theme, hotkeys, telemetry "
           "opt-in, and broker credentials. Hotkeys are rebindable — "
           "every action in the registry can be assigned a key."),
        tr("Tip: F11 toggles fullscreen on the focused window.")));
    step_count_ = steps_->count();
    root->addWidget(steps_, /*stretch=*/1);

    progress_ = new QLabel(this);
    progress_->setAlignment(Qt::AlignCenter);
    root->addWidget(progress_);

    auto* button_row = new QHBoxLayout;
    btn_skip_ = new QPushButton(tr("Skip"), this);
    btn_back_ = new QPushButton(tr("Back"), this);
    btn_next_ = new QPushButton(tr("Next"), this);
    btn_next_->setDefault(true);
    button_row->addWidget(btn_skip_);
    button_row->addStretch();
    button_row->addWidget(btn_back_);
    button_row->addWidget(btn_next_);
    root->addLayout(button_row);

    connect(btn_skip_, &QPushButton::clicked, this, &OnboardingTour::on_skip);
    connect(btn_back_, &QPushButton::clicked, this, &OnboardingTour::on_back);
    connect(btn_next_, &QPushButton::clicked, this, &OnboardingTour::on_next);
}

QWidget* OnboardingTour::build_step(const QString& title, const QString& body,
                                    const QString& tip) {
    auto* w = new QWidget;
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(10);

    auto* h = new QLabel(title, w);
    QFont hf = h->font();
    hf.setBold(true);
    hf.setPointSizeF(hf.pointSizeF() * 1.15);
    h->setFont(hf);
    l->addWidget(h);

    auto* b = new QLabel(body, w);
    b->setWordWrap(true);
    l->addWidget(b);

    auto* t = new QLabel(tip, w);
    t->setWordWrap(true);
    QFont tf = t->font();
    tf.setItalic(true);
    t->setFont(tf);
    l->addWidget(t);

    l->addStretch();
    return w;
}

void OnboardingTour::on_next() {
    if (!steps_)
        return;
    if (current_step_ + 1 >= step_count_) {
        on_finish();
        return;
    }
    ++current_step_;
    steps_->setCurrentIndex(current_step_);
    update_buttons();
}

void OnboardingTour::on_back() {
    if (!steps_ || current_step_ == 0)
        return;
    --current_step_;
    steps_->setCurrentIndex(current_step_);
    update_buttons();
}

void OnboardingTour::on_skip() {
    finished_ = true;
    mark_seen();
    accept();
}

void OnboardingTour::on_finish() {
    finished_ = true;
    mark_seen();
    accept();
}

void OnboardingTour::update_buttons() {
    if (!btn_back_ || !btn_next_)
        return;
    btn_back_->setEnabled(current_step_ > 0);
    const bool last = (current_step_ + 1 >= step_count_);
    btn_next_->setText(last ? tr("Got it!") : tr("Next"));
    if (progress_)
        progress_->setText(QString("Step %1 of %2").arg(current_step_ + 1).arg(step_count_));
}

void OnboardingTour::closeEvent(QCloseEvent* e) {
    // Treat OS close as "skip" so the user doesn't get hit with the tour
    // again on next launch.
    if (!finished_)
        mark_seen();
    QDialog::closeEvent(e);
}

} // namespace fincept::screens
