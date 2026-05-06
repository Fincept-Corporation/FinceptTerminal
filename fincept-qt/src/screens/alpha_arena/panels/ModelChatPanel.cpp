#include "screens/alpha_arena/panels/ModelChatPanel.h"

#include "services/alpha_arena/AlphaArenaRepo.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidgetItem>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QString>
#include <QTextEdit>
#include <QVBoxLayout>

namespace fincept::screens::alpha_arena {

using fincept::services::alpha_arena::AlphaArenaRepo;
using fincept::services::alpha_arena::ModelChatRow;

namespace {

QString fmt_tick_time(qint64 utc_ms, int seq) {
    const auto dt = QDateTime::fromMSecsSinceEpoch(utc_ms);
    return QStringLiteral("#%1  %2").arg(seq).arg(dt.toString("HH:mm:ss"));
}

QString row_summary(const ModelChatRow& r) {
    if (!r.parse_error.isEmpty()) {
        return fmt_tick_time(r.tick_utc_ms, r.tick_seq) +
               QStringLiteral("  ⚠  ") + r.parse_error.left(80);
    }
    auto doc = QJsonDocument::fromJson(r.parsed_actions_json.toUtf8());
    int n = doc.isArray() ? doc.array().size() : 0;
    return fmt_tick_time(r.tick_utc_ms, r.tick_seq) +
           QStringLiteral("  ·  ") + QString::number(n) + QStringLiteral(" action(s)") +
           (r.cost_usd > 0 ? QStringLiteral("  $%1").arg(r.cost_usd, 0, 'f', 4) : QString());
}

} // namespace

ModelChatPanel::ModelChatPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("aaModelChatPanel");
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* header = new QWidget;
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(8, 6, 8, 6);
    hl->addWidget(new QLabel("AGENT"));
    agent_picker_ = new QComboBox;
    connect(agent_picker_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { refresh(); });
    hl->addWidget(agent_picker_, 1);
    root->addWidget(header);

    timeline_ = new QListWidget(this);
    timeline_->setObjectName("aaModelChatList");
    timeline_->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(timeline_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* it) {
        if (!it) return;
        show_decision_detail(it->data(Qt::UserRole).toString());
    });
    root->addWidget(timeline_, 1);
}

void ModelChatPanel::set_competition(const QString& competition_id) {
    competition_id_ = competition_id;
    reload_agent_picker();
    refresh();
}

void ModelChatPanel::reload_agent_picker() {
    agent_picker_->blockSignals(true);
    agent_picker_->clear();
    if (competition_id_.isEmpty()) {
        agent_picker_->blockSignals(false);
        return;
    }
    auto r = AlphaArenaRepo::instance().agents_for(competition_id_);
    if (r.is_ok()) {
        for (const auto& a : r.value()) {
            agent_picker_->addItem(a.display_name, a.id);
        }
    }
    agent_picker_->blockSignals(false);
}

void ModelChatPanel::on_decision_received(const QString& /*decision_id*/, const QString& agent_id) {
    if (agent_picker_->currentData().toString() == agent_id) refresh();
}

void ModelChatPanel::refresh() {
    timeline_->clear();
    if (competition_id_.isEmpty() || agent_picker_->currentIndex() < 0) return;
    const QString agent_id = agent_picker_->currentData().toString();
    auto r = AlphaArenaRepo::instance().modelchat_for(competition_id_, agent_id, 200, 0);
    if (r.is_err()) return;
    for (const auto& row : r.value()) {
        auto* item = new QListWidgetItem(row_summary(row));
        item->setData(Qt::UserRole, row.decision_id);
        if (!row.parse_error.isEmpty())
            item->setForeground(QColor(fincept::ui::colors::NEGATIVE()));
        timeline_->addItem(item);
    }
}

void ModelChatPanel::show_decision_detail(const QString& decision_id) {
    if (decision_id.isEmpty()) return;
    // Re-pull a single-row view by walking the cached list.
    if (competition_id_.isEmpty() || agent_picker_->currentIndex() < 0) return;
    const QString agent_id = agent_picker_->currentData().toString();
    auto r = AlphaArenaRepo::instance().modelchat_for(competition_id_, agent_id, 200, 0);
    if (r.is_err()) return;

    ModelChatRow target;
    bool found = false;
    for (const auto& row : r.value()) {
        if (row.decision_id == decision_id) { target = row; found = true; break; }
    }
    if (!found) return;

    auto* dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(QStringLiteral("Tick %1 — %2").arg(target.tick_seq).arg(agent_id.left(8)));
    dlg->resize(800, 600);

    auto* layout = new QVBoxLayout(dlg);

    auto* split = new QSplitter(Qt::Vertical, dlg);

    auto* response = new QPlainTextEdit;
    response->setReadOnly(true);
    response->setPlainText(target.raw_response);
    response->setPlaceholderText(QStringLiteral("(no response)"));

    auto* parsed = new QPlainTextEdit;
    parsed->setReadOnly(true);
    auto pretty = [](const QString& json) {
        auto d = QJsonDocument::fromJson(json.toUtf8());
        if (d.isArray() || d.isObject()) return QString::fromUtf8(d.toJson(QJsonDocument::Indented));
        return json;
    };
    parsed->setPlainText(QStringLiteral("Parsed actions:\n%1\n\nRisk verdicts:\n%2\n\nParse error: %3")
                              .arg(pretty(target.parsed_actions_json),
                                   pretty(target.risk_verdict_json),
                                   target.parse_error.isEmpty() ? QStringLiteral("(none)")
                                                                 : target.parse_error));

    split->addWidget(response);
    split->addWidget(parsed);
    layout->addWidget(split, 1);

    auto* meta = new QLabel(
        QStringLiteral("latency=%1ms  in=%2  out=%3  cost=$%4")
            .arg(target.latency_ms)
            .arg(target.tokens_in)
            .arg(target.tokens_out)
            .arg(target.cost_usd, 0, 'f', 4));
    layout->addWidget(meta);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    layout->addWidget(buttons);

    dlg->show();
}

} // namespace fincept::screens::alpha_arena
