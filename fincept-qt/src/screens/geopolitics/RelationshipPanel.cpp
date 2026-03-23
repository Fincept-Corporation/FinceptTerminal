// src/screens/geopolitics/RelationshipPanel.cpp
#include "screens/geopolitics/RelationshipPanel.h"
#include "ui/theme/Theme.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QScrollArea>

namespace fincept::screens {

using namespace fincept::services::geo;

// Static relationship data matching the React implementation
static QVector<RelationshipNode> build_nodes() {
    QVector<RelationshipNode> nodes;

    // Primary conflicts
    nodes.append({"ukraine", "Ukraine", "conflict", "critical", 89,
                  {"displacement", "humanitarian", "food_security"}});
    nodes.append({"gaza", "Gaza", "conflict", "critical", 67,
                  {"displacement", "humanitarian", "health"}});
    nodes.append({"sudan", "Sudan", "conflict", "critical", 45,
                  {"displacement", "food_security", "humanitarian"}});
    nodes.append({"yemen", "Yemen", "conflict", "high", 56,
                  {"food_security", "humanitarian", "health"}});
    nodes.append({"syria", "Syria", "conflict", "high", 78,
                  {"displacement", "humanitarian"}});
    nodes.append({"myanmar", "Myanmar", "conflict", "high", 29,
                  {"displacement", "humanitarian"}});

    // Crisis types
    nodes.append({"displacement", "Displacement", "crisis", "high", 234,
                  {"unhcr"}});
    nodes.append({"food_security", "Food Security", "crisis", "high", 189,
                  {"wfp", "fao"}});
    nodes.append({"humanitarian", "Humanitarian", "crisis", "critical", 312,
                  {"unhcr", "wfp", "who"}});
    nodes.append({"health", "Health Crisis", "crisis", "medium", 145,
                  {"who"}});

    // Organizations
    nodes.append({"unhcr", "UNHCR", "organization", "", 234,
                  {"displacement", "humanitarian"}});
    nodes.append({"wfp", "WFP", "organization", "", 189,
                  {"food_security", "humanitarian"}});
    nodes.append({"who", "WHO", "organization", "", 156,
                  {"health", "humanitarian"}});
    nodes.append({"fao", "FAO", "organization", "", 123,
                  {"food_security"}});

    return nodes;
}

static QColor severity_color(const QString& sev) {
    if (sev == "critical") return QColor("#FF0000");
    if (sev == "high")     return QColor("#FFA500");
    if (sev == "medium")   return QColor("#FF9800");
    if (sev == "low")      return QColor("#4CAF50");
    return QColor("#2196F3"); // organization
}

RelationshipPanel::RelationshipPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void RelationshipPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header
    auto* header = new QWidget(this);
    header->setFixedHeight(40);
    header->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;")
        .arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(16, 0, 16, 0);
    auto* title = new QLabel("GEOPOLITICAL RELATIONSHIP NETWORK", header);
    title->setStyleSheet(QString(
        "color:#9D4EDD; font-size:%1px; font-weight:700; font-family:%2; letter-spacing:1px;")
        .arg(ui::fonts::TINY).arg(ui::fonts::DATA_FAMILY));
    hhl->addWidget(title);
    hhl->addStretch();

    // Network stats
    auto nodes = build_nodes();
    int conflicts = 0, orgs = 0;
    for (const auto& n : nodes) {
        if (n.type == "conflict") conflicts++;
        if (n.type == "organization") orgs++;
    }
    auto* stats = new QLabel(
        QString("NODES: %1 | CONFLICTS: %2 | ORGANIZATIONS: %3")
            .arg(nodes.size()).arg(conflicts).arg(orgs), header);
    stats->setStyleSheet(QString("color:%1; font-size:9px; font-family:%2;")
        .arg(ui::colors::TEXT_TERTIARY).arg(ui::fonts::DATA_FAMILY));
    hhl->addWidget(stats);
    root->addWidget(header);

    // Network view (card grid)
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border:none; background:transparent; }");

    auto* content = new QWidget(scroll);
    auto* gl = new QGridLayout(content);
    gl->setContentsMargins(16, 16, 16, 16);
    gl->setSpacing(12);

    // Section: Conflicts
    auto* conflict_title = new QLabel("ACTIVE CONFLICTS", content);
    conflict_title->setStyleSheet(QString(
        "color:#FF0000; font-size:9px; font-weight:700; font-family:%1;"
        "letter-spacing:1px;").arg(ui::fonts::DATA_FAMILY));
    gl->addWidget(conflict_title, 0, 0, 1, 3);

    int col = 0, row = 1;
    for (const auto& n : nodes) {
        if (n.type != "conflict") continue;
        gl->addWidget(build_node_card(n, content), row, col);
        if (++col >= 3) { col = 0; row++; }
    }

    // Section: Crisis Types
    row++; col = 0;
    auto* crisis_title = new QLabel("CRISIS TYPES", content);
    crisis_title->setStyleSheet(QString(
        "color:#FFA500; font-size:9px; font-weight:700; font-family:%1;"
        "letter-spacing:1px;").arg(ui::fonts::DATA_FAMILY));
    gl->addWidget(crisis_title, row, 0, 1, 3);
    row++;

    for (const auto& n : nodes) {
        if (n.type != "crisis") continue;
        gl->addWidget(build_node_card(n, content), row, col);
        if (++col >= 3) { col = 0; row++; }
    }

    // Section: Organizations
    row++; col = 0;
    auto* org_title = new QLabel("ORGANIZATIONS", content);
    org_title->setStyleSheet(QString(
        "color:#2196F3; font-size:9px; font-weight:700; font-family:%1;"
        "letter-spacing:1px;").arg(ui::fonts::DATA_FAMILY));
    gl->addWidget(org_title, row, 0, 1, 3);
    row++;

    for (const auto& n : nodes) {
        if (n.type != "organization") continue;
        gl->addWidget(build_node_card(n, content), row, col);
        if (++col >= 3) { col = 0; row++; }
    }

    scroll->setWidget(content);
    root->addWidget(scroll, 1);
}

QWidget* RelationshipPanel::build_node_card(const RelationshipNode& node, QWidget* parent) {
    auto color = severity_color(node.severity.isEmpty() ? "organization" : node.severity);

    auto* card = new QWidget(parent);
    card->setStyleSheet(QString(
        "background:%1; border:1px solid rgba(%2,%3,%4,0.3);"
        "border-left:3px solid %5; border-radius:2px;")
        .arg(ui::colors::BG_RAISED)
        .arg(color.red()).arg(color.green()).arg(color.blue())
        .arg(color.name()));

    auto* vl = new QVBoxLayout(card);
    vl->setContentsMargins(12, 10, 12, 10);
    vl->setSpacing(4);

    auto* name = new QLabel(node.label.toUpper(), card);
    name->setStyleSheet(QString("color:%1; font-size:11px; font-weight:700; font-family:%2;")
        .arg(color.name()).arg(ui::fonts::DATA_FAMILY));
    vl->addWidget(name);

    auto* type_lbl = new QLabel(node.type.toUpper(), card);
    type_lbl->setStyleSheet(QString("color:%1; font-size:8px; font-family:%2;")
        .arg(ui::colors::TEXT_TERTIARY).arg(ui::fonts::DATA_FAMILY));
    vl->addWidget(type_lbl);

    if (!node.severity.isEmpty()) {
        auto* sev = new QLabel(QString("Severity: %1").arg(node.severity.toUpper()), card);
        sev->setStyleSheet(QString("color:%1; font-size:9px; font-family:%2;")
            .arg(color.name()).arg(ui::fonts::DATA_FAMILY));
        vl->addWidget(sev);
    }

    auto* ds = new QLabel(QString("Datasets: %1").arg(node.dataset_count), card);
    ds->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; font-family:%2;")
        .arg(ui::colors::TEXT_PRIMARY).arg(ui::fonts::DATA_FAMILY));
    vl->addWidget(ds);

    auto* conns = new QLabel(QString("Connections: %1").arg(node.connections.size()), card);
    conns->setStyleSheet(QString("color:%1; font-size:9px; font-family:%2;")
        .arg(ui::colors::TEXT_SECONDARY).arg(ui::fonts::DATA_FAMILY));
    vl->addWidget(conns);

    return card;
}

} // namespace fincept::screens
