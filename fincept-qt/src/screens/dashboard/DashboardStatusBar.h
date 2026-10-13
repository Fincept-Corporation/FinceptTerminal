#pragma once
#include <QHideEvent>
#include <QLabel>
#include <QShowEvent>
#include <QTimer>
#include <QVariant>
#include <QVector>
#include <QWidget>

// Forward declarations — avoids pulling notification headers into every TU
// that includes DashboardStatusBar.h
namespace fincept::ui {
class NotifBell;
class NotifPanel;
} // namespace fincept::ui

namespace fincept::screens {

class PendingOrdersBadge;

/// Bottom status bar — session uptime, per-asset-class quote freshness, API
/// health/latency, memory, and the notification bell (right side).
///
/// API health comes from the `system:health` DataHub topic
/// (services::ConnectivityService); the bar owns no network code and no
/// data-refresh timer — the hub schedules the probe while the bar is visible.
class DashboardStatusBar : public QWidget {
    Q_OBJECT
  public:
    explicit DashboardStatusBar(QWidget* parent = nullptr);

    void set_widget_count(int count);

  signals:
    /// Emitted when the API health probe flips between reachable and
    /// unreachable. DashboardScreen forwards it to the toolbar so the "LIVE"
    /// badge stops claiming a live feed while the backend is down.
    void connectivity_changed(bool connected);

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    /// One per-asset-class freshness indicator ("EQ", "FX", …). Lit from the
    /// age of the newest quote among `topics` — the letters used to be static
    /// labels painted green (CM permanently amber) whatever the feeds did.
    struct FeedLamp {
        QLabel* label = nullptr;
        QString code;       ///< "EQ", "FX", … (not translated — terminal mnemonics)
        QStringList topics; ///< representative market:quote:* topics
    };

    void refresh_theme();
    void update_uptime();
    void update_memory();
    void update_feed_lamps();
    void on_health(const QVariant& v);
    void set_api_state(int state); // -1 unknown/checking, 0 offline, 1 online
    void toggle_notif_panel();
    void retranslateUi();
    void hub_subscribe();
    void hub_unsubscribe();
    /// Set a styling property and re-polish only when it actually changed —
    /// the bar-level stylesheet carries every colour, so no per-label
    /// setStyleSheet (and no CSS re-parse) happens on updates.
    static void set_tone(QWidget* w, const char* prop, const QString& value);

    QLabel* version_label_ = nullptr;
    QLabel* uptime_label_ = nullptr;
    QLabel* layout_label_ = nullptr;
    QLabel* feeds_label_ = nullptr;
    QLabel* latency_label_ = nullptr;
    QLabel* mem_label_ = nullptr;
    QLabel* session_lbl_ = nullptr;
    QLabel* layout_caption_lbl_ = nullptr;
    QLabel* feeds_caption_lbl_ = nullptr;
    QLabel* ready_lbl_ = nullptr;
    QVector<FeedLamp> lamps_;
    int layout_count_ = 0;
    int api_state_ = -1;       // -1 checking, 0 offline, 1 online
    int last_latency_ms_ = -2; // -2 = no probe yet, -1 = error
    QString last_health_error_;
    qint64 last_health_checked_ms_ = 0;
    bool hub_active_ = false;

    PendingOrdersBadge* pending_badge_ = nullptr;
    fincept::ui::NotifBell* notif_bell_ = nullptr;
    fincept::ui::NotifPanel* notif_panel_ = nullptr;

    QTimer uptime_timer_;
    /// Local-only probes (process RSS, hub topic ages) — no I/O.
    QTimer local_probe_timer_;

    qint64 start_time_ = 0;
};

} // namespace fincept::screens
