#pragma once
// AgentRail.h — the left rail of the agents screen: AGENTS (by category),
// TEAMS, HARNESSES and ACTIVE RUNS in one tree, each row with a state chip
// (READY / NEEDS KEY / SIGNED OUT / NOT INSTALLED / RUNNING n …). Model/view
// (QStandardItemModel + a painting delegate — no widget per row); the screen's
// search field filters it. Clicking a row opens that item in the centre (the
// screen decides how); a group with nothing in it shows what to do instead
// ("Create your first agent", "Connect a coding agent").
//
// Reads only in-memory state (AgentService's cached agent list, HarnessRegistry,
// CodingAgentService, TaskRunService) plus the saved-team rows (one small
// AgentConfigRepository query, coalesced, only while the rail is visible).

#include <QStyledItemDelegate>
#include <QWidget>

class QLabel;
class QSortFilterProxyModel;
class QStandardItemModel;
class QTimer;
class QTreeView;

namespace fincept::screens {

class AgentRailDelegate : public QStyledItemDelegate {
    Q_OBJECT
  public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

class AgentRail : public QWidget {
    Q_OBJECT
  public:
    /// Item roles.
    static constexpr int kKindRole = Qt::UserRole + 1;  ///< "agent" "team" "harness" "run" "group" "state" "action"
    static constexpr int kIdRole = Qt::UserRole + 2;    ///< agent / team / harness id, run key, action id
    static constexpr int kChipRole = Qt::UserRole + 3;  ///< chip text ("" = none)
    static constexpr int kToneRole = Qt::UserRole + 4;  ///< 0 dim, 1 ok, 2 warn, 3 danger, 4 busy, 5 accent

    explicit AgentRail(QWidget* parent = nullptr);

    void set_filter_text(const QString& text);
    /// Discovery state of the agent list (shown when it has no agents yet).
    void set_discovery_state(bool discovering, const QString& error);
    /// Rebuild soon (coalesced).
    void request_refresh();
    /// Highlight an item (no signal).
    void set_current(const QString& kind, const QString& id);

  signals:
    /// A row was clicked / activated: kind + id (see the roles).
    void item_activated(const QString& kind, const QString& id);

  protected:
    void showEvent(QShowEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    void rebuild();
    void fit();

    QLabel* title_ = nullptr;
    QTreeView* tree_ = nullptr;
    QStandardItemModel* model_ = nullptr;
    QSortFilterProxyModel* proxy_ = nullptr;
    QTimer* refresh_timer_ = nullptr;
    QString current_kind_;
    QString current_id_;
    QString discover_error_;
    bool discovering_ = false;
    bool dirty_ = true;
};

} // namespace fincept::screens
