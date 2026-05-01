#include "ui/debug/DebugOverlay.h"

#include "app/WindowFrame.h"
#include "core/panel/PanelRegistry.h"
#include "datahub/DataHub.h"

#include <QDateTime>
#include <QHBoxLayout>

namespace fincept::ui {

DebugOverlay::DebugOverlay(QWidget* parent) : QFrame(parent) {
    setObjectName("debugOverlay");
    setFrameShape(QFrame::NoFrame);
    setStyleSheet(
        "QFrame#debugOverlay {"
        "  background: rgba(15, 23, 42, 0.85);"
        "  border: 1px solid #374151;"
        "  border-radius: 4px;"
        "}");
    setAttribute(Qt::WA_TransparentForMouseEvents);
    // Hidden by default — user toggles via debug.overlay action.
    hide();

    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(8, 4, 8, 4);
    hl->setSpacing(0);

    label_ = new QLabel(this);
    label_->setStyleSheet(
        "color: #9ca3af; font-family: 'Consolas', monospace; font-size: 10px;");
    label_->setTextFormat(Qt::PlainText);
    hl->addWidget(label_);

    refresh_timer_.setInterval(1000);
    connect(&refresh_timer_, &QTimer::timeout, this, &DebugOverlay::refresh);

    last_msgs_sample_ms_ = QDateTime::currentMSecsSinceEpoch();
}

void DebugOverlay::reposition() {
    if (!parentWidget())
        return;
    adjustSize();
    const QSize ps = parentWidget()->size();
    move(ps.width() - width() - 8, ps.height() - height() - 8);
}

void DebugOverlay::toggle_visible() {
    if (isVisible()) {
        hide();
        refresh_timer_.stop();
    } else {
        refresh();
        reposition();
        show();
        raise();
        refresh_timer_.start();
    }
}

void DebugOverlay::refresh() {
    QString frame_info;
    if (auto* frame = qobject_cast<fincept::WindowFrame*>(window())) {
        const auto frame_uuid = frame->frame_uuid();
        const QString uuid_short = frame_uuid.to_string().left(8);
        const int frame_panels =
            fincept::PanelRegistry::instance().find_by_frame(frame_uuid).size();
        const bool active = frame->is_active_for_work();
        frame_info = QString("frame %1  panels:%2  active:%3")
                         .arg(uuid_short)
                         .arg(frame_panels)
                         .arg(active ? "yes" : "no");
    } else {
        frame_info = "frame ?";
    }

    const int total_panels = fincept::PanelRegistry::instance().size();

    // DataHub stats. Approximate msgs/sec as delta(total publishes) over
    // the elapsed window. Uses the existing DataHub topic_stats view —
    // walk all topics and sum total_publishes.
    qint64 total_publishes = 0;
    int topic_count = 0;
    {
        const auto stats = fincept::datahub::DataHub::instance().stats();
        topic_count = stats.size();
        for (const auto& s : stats)
            total_publishes += s.total_publishes;
    }
    const qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
    const qint64 dt_ms = qMax<qint64>(1, now_ms - last_msgs_sample_ms_);
    const qint64 dpub = qMax<qint64>(0, total_publishes - last_msgs_count_);
    const double rate = (double)dpub / ((double)dt_ms / 1000.0);
    last_msgs_count_ = total_publishes;
    last_msgs_sample_ms_ = now_ms;

    const QString hub_info = QString("hub topics:%1  rate:%2/s")
                                 .arg(topic_count)
                                 .arg(QString::number(rate, 'f', 1));

    label_->setText(QString("%1\nall panels:%2  %3")
                        .arg(frame_info)
                        .arg(total_panels)
                        .arg(hub_info));
    adjustSize();
    reposition();
}

} // namespace fincept::ui
