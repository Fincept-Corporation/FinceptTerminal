#pragma once
// AgentWorkbenchTemplates.h — professional starter prompts for the Agent
// Workbench composer and schedule editor. <PLACEHOLDERS> are highlighted in the
// prompt editor and selected for overtyping when a template is inserted.

#include <QRegularExpression>
#include <QString>
#include <QSyntaxHighlighter>
#include <QVector>

class QTextDocument;

namespace fincept::screens {

struct AgentPromptTemplate {
    QString title;
    QString body;
    bool needs_workspace = false; // asks the agent to write files → switch to Workspace edits mode
};

/// The eight starter prompts (translated at call time).
QVector<AgentPromptTemplate> agent_prompt_templates();

/// Matches a template placeholder such as <TICKER> or <UNIVERSE, e.g. S&P 500>.
const QRegularExpression& agent_placeholder_regex();

/// Highlights <PLACEHOLDERS> in a prompt editor (amber on dark amber).
class AgentPlaceholderHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
  public:
    explicit AgentPlaceholderHighlighter(QTextDocument* document);

  protected:
    void highlightBlock(const QString& text) override;
};

} // namespace fincept::screens
