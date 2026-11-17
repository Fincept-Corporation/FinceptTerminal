// src/ui/widgets/ScreenStatusBar.h
#pragma once
// ScreenStatusBar — the terminal's bottom bar for a dense screen (the Portfolio / Markets
// status bars, made generic so every screen shares one):
//
//   LEFT  BOARD · 124 / 2,031 · 3 SELECTED  |  CENTRE  notice text  UNDO  |  RIGHT  ● SAVED 14:02 | 14:02:31 IST
//
//   * Three segments (left, centre, right) of labelled items; a hairline "|" is
//     drawn between neighbouring visible items of a segment.
//   * Every item has a tone (Normal, Dim, Accent, Ok, Warn, Danger, Positive,
//     Negative, Info) set through a dynamic property, so the bar's ONE stylesheet
//     (token built, re-styled centrally) covers theme changes; set_chip() frames
//     an item as a tone chip. POSITIVE / NEGATIVE are for price direction only,
//     OK / WARN / DANGER for state.
//   * A transient notice in the centre with one action button (UNDO, RETRY …),
//     auto-dismissed after `ms`; `on_expire` runs when it times out or is replaced
//     (e.g. "the delete becomes final"), never when its action ran.
//   * An optional clock at the right end (local time + zone), ticking only between
//     start_clock() / stop_clock() — call them from showEvent / hideEvent.
//   * Narrow windows: items give way by priority (lowest first) in resizeEvent.
//     Nothing is shown / hidden in a theme handler; sizes follow the font.
//
// USAGE
//   auto* sb = new fincept::ui::ScreenStatusBar(this);
//   sb->add_item("board", ScreenStatusBar::Segment::Left, tr("TRADE IDEAS"), ScreenStatusBar::Tone::Accent, 100);
//   sb->add_item("count", ScreenStatusBar::Segment::Left, {}, ScreenStatusBar::Tone::Normal, 90);
//   sb->add_item("save", ScreenStatusBar::Segment::Right, tr("● SAVED"), ScreenStatusBar::Tone::Ok, 80);
//   sb->set_chip("save", true);
//   sb->set_clock(true, /*seconds=*/true);
//   sb->set_text("count", tr("%1 / %2 notes").arg(shown).arg(total));
//   sb->show_notice(tr("Deleted 3 notes"), tr("UNDO"), [this] { undo(); }, 8000,
//                   ScreenStatusBar::Tone::Normal, [this] { commit_delete(); });

#include <QHash>
#include <QString>
#include <QVector>
#include <QWidget>

#include <functional>

class QHBoxLayout;
class QLabel;
class QPushButton;
class QTimer;

namespace fincept::ui {

class ScreenStatusBar : public QWidget {
    Q_OBJECT
  public:
    enum class Segment { Left, Centre, Right };
    enum class Tone { Normal, Dim, Accent, Ok, Warn, Danger, Positive, Negative, Info };

    explicit ScreenStatusBar(QWidget* parent = nullptr);
    ~ScreenStatusBar() override;

    /// Append an item to a segment. Higher priority stays visible longer.
    QLabel* add_item(const QString& id, Segment segment, const QString& text = {}, Tone tone = Tone::Normal,
                     int priority = 50);
    QLabel* item(const QString& id) const;
    void set_text(const QString& id, const QString& text);
    void set_tone(const QString& id, Tone tone);
    void set_item(const QString& id, const QString& text, Tone tone);
    void set_tooltip(const QString& id, const QString& tooltip);
    /// Frame the item as a tone chip.
    void set_chip(const QString& id, bool chip);
    /// The caller's own visibility (an item that has nothing to say).
    void set_item_visible(const QString& id, bool visible);

    /// The right-most clock ("14:02:31 IST"); `seconds` ticks every second, else every 20 s.
    void set_clock(bool enabled, bool seconds = false);
    void start_clock();
    void stop_clock();

    /// Show a notice (replacing one still showing — whose on_expire then runs).
    void show_notice(const QString& text, const QString& action_label = {}, std::function<void()> action = {},
                     int ms = 7000, Tone tone = Tone::Normal, std::function<void()> on_expire = {});
    /// Hide the notice now, running its on_expire.
    void finish_notice();
    /// Hide the notice now without running anything.
    void clear_notice();
    /// Run the notice's action now (e.g. Ctrl+Z), as if its button was clicked.
    bool trigger_notice_action();
    bool notice_active() const;
    bool notice_has_action() const;

    QSize minimumSizeHint() const override;

  protected:
    void resizeEvent(QResizeEvent* e) override;
    void changeEvent(QEvent* e) override;
    void showEvent(QShowEvent* e) override;

  private:
    struct Entry {
        QString id;
        Segment segment = Segment::Left;
        QLabel* label = nullptr;
        QLabel* sep = nullptr;
        int priority = 50;
        int order = 0;
        bool user_visible = true;
        bool dropped = false;
    };
    Entry* find(const QString& id);
    const Entry* find(const QString& id) const;
    QHBoxLayout* layout_for(Segment s) const;
    static const char* tone_key(Tone t);
    static void apply_property(QWidget* w, const char* name, const QString& value);
    void relayout();
    void update_clock();
    void fit_metrics();

    QHBoxLayout* row_ = nullptr;
    QHBoxLayout* left_ = nullptr;
    QHBoxLayout* centre_ = nullptr;
    QHBoxLayout* right_ = nullptr;
    QVector<Entry> entries_;
    int next_order_ = 0;
    QLabel* notice_ = nullptr;
    QPushButton* notice_btn_ = nullptr;
    QTimer* notice_timer_ = nullptr;
    std::function<void()> notice_action_;
    std::function<void()> notice_expire_;
    QLabel* clock_ = nullptr;
    QLabel* clock_sep_ = nullptr;
    QTimer* clock_timer_ = nullptr;
    bool clock_seconds_ = false;
    QString tz_abbrev_;
    qint64 tz_hour_ = -1;
};

} // namespace fincept::ui
