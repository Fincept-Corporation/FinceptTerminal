#pragma once
// AiChatStartPage.h — the designed start page of a new (empty) AI Chat session.
//
//   ▌AI RESEARCH DESK
//   Ask about markets, companies, your portfolio or macro data. Answers can use the
//   terminal's tools; nothing is sent until you press Send.
//   [needs-setup page: no AI model configured — OPEN SETTINGS > LLM CONFIGURATION]  (when so)
//   MARKETS            COMPANY RESEARCH     PORTFOLIO          MACRO            CODE / DATA
//   [card] [card]      [card] [card]        [card] …                                           (re-flows: 5 → 3 → 2 → 1 columns)
//   RECENT SESSIONS
//   Fed path after CPI · 14:32 …
//   Multi-agent research for firms, AI credits included: Fincept Terminal Enterprise ↗
//
// Cards only FILL the composer (prompt_chosen) — they never send. Scrolls when short, so
// nothing is cut or blank at any size.

#include "screens/ai_chat/AiChatTypes.h"

#include <QString>
#include <QVector>
#include <QWidget>

class QGridLayout;
class QLabel;
class QScrollArea;
class QVBoxLayout;

namespace fincept::ui {
class EmptyState;
}

namespace fincept::screens::aichat {

class AicStartPage : public QWidget {
    Q_OBJECT
  public:
    explicit AicStartPage(QWidget* parent = nullptr);
    ~AicStartPage() override;

    /// Up to 5 recent sessions (newest first); empty hides the section.
    void set_recent(const QVector<SessionSummary>& recent);
    /// Show the needs-setup page (ui::EmptyState::needs_setup_spec of Requirement::llm(),
    /// OPEN SETTINGS > LLM CONFIGURATION) above the cards. Applied only on a change.
    void refresh_setup();

  signals:
    void prompt_chosen(const QString& prompt); ///< fill the composer (never sends)
    void session_chosen(const QString& id);

  protected:
    void resizeEvent(QResizeEvent* e) override;
    void changeEvent(QEvent* e) override;

  private:
    QScrollArea* scroll_ = nullptr;
    QWidget* body_ = nullptr;
    QLabel* title_ = nullptr;
    QLabel* lead_ = nullptr;
    ui::EmptyState* setup_ = nullptr;
    QGridLayout* cards_grid_ = nullptr;
    QWidget* recent_box_ = nullptr;
    QVBoxLayout* recent_layout_ = nullptr;
    int setup_state_ = -1; ///< -1 not checked, 0 missing, 1 configured
    int columns_ = 0;

    void build();
    void retranslate();
    void reflow(int width);

    // ── private (AiChatStartPage.cpp) ────────────────────────────────────────
    QWidget* content_ = nullptr;        ///< the scroll area's widget (centres body_)
    QWidget* cards_box_ = nullptr;
    QLabel* recent_title_ = nullptr;
    QLabel* ent_ = nullptr;             ///< the Enterprise mention line
    QVector<QWidget*> group_cols_;      ///< one column per example-card group
    QVector<SessionSummary> recent_;

    void rebuild_cards();
    void rebuild_recent();
    void fit_metrics();
    int body_max_width() const;
};

} // namespace fincept::screens::aichat
