#pragma once
// ReportDocument.h — Canonical report data model shared by Report Builder
// screen, ReportBuilderService, MCP tools, and disk persistence.
//
// Lives in core/ (not screens/) so non-UI consumers (services, MCP tools)
// can include it without dragging in the Report Builder UI.

#include "core/result/Result.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QVector>

namespace fincept::report {

// ── Component ────────────────────────────────────────────────────────────────
//
// Stable id is the contract between LLM tool calls and the live document. The
// LLM is given an id when it adds a component; subsequent update/remove/move
// calls reference that id. Indices change as components are inserted/removed
// (manually or by the LLM) — ids do not.

struct ReportComponent {
    int id = 0;
    // Types: heading, text, table, image, chart, sparkline, stats_block,
    //        callout, code, divider, quote, list, market_data, page_break, toc
    QString type;
    QString content;
    QMap<QString, QString> config;
};

struct ReportMetadata {
    QString title = "Untitled Report";
    QString author = "Analyst";
    QString company = "Fincept Corporation";
    QString date;
    QString header_left;
    QString header_center;
    QString header_right;
    QString footer_left;
    QString footer_center = "Page {page}";
    QString footer_right;
    bool show_page_numbers = true;
};

struct ReportTheme {
    QString name;
    QString heading_color;
    QString accent_color;
    QString page_bg;
    QString text_color;
    QString table_header_bg;
    QString table_header_fg;
    QString code_bg;
    QString quote_bg;
    QString meta_color;
    QString divider_color;
};

namespace themes {

inline ReportTheme light_professional() {
    return {"Light Professional",
            "#1a1a1a", "#d97706", "#ffffff", "#333333", "#f0f0f0",
            "#1a1a1a", "#f5f5f5", "#f9f9f9", "#666666", "#cccccc"};
}
inline ReportTheme dark_corporate() {
    return {"Dark Corporate",
            "#e5e5e5", "#d97706", "#1e1e1e", "#cccccc", "#2a2a2a",
            "#e5e5e5", "#111111", "#1a1a1a", "#808080", "#444444"};
}
inline ReportTheme bloomberg() {
    return {"Bloomberg Terminal",
            "#ff8c00", "#ff8c00", "#0a0a0a", "#d4d4d4", "#1a0a00",
            "#ff8c00", "#050505", "#0f0f0f", "#888888", "#333300"};
}
inline ReportTheme midnight_blue() {
    return {"Midnight Blue",
            "#e2e8f0", "#3b82f6", "#0f172a", "#cbd5e1", "#1e3a5f",
            "#e2e8f0", "#0d1b2e", "#132033", "#64748b", "#1e3a5f"};
}

// Resolve a theme name (case-sensitive, matches the .name field). Returns
// light_professional() for unknown names so the document never ends up with
// an empty/garbage theme.
inline ReportTheme by_name(const QString& name) {
    if (name == "Dark Corporate")
        return dark_corporate();
    if (name == "Bloomberg Terminal")
        return bloomberg();
    if (name == "Midnight Blue")
        return midnight_blue();
    return light_professional();
}

inline QStringList all_names() {
    return {"Light Professional", "Dark Corporate", "Bloomberg Terminal", "Midnight Blue"};
}

} // namespace themes

// ── Document ─────────────────────────────────────────────────────────────────

struct ReportDocument {
    QVector<ReportComponent> components;
    ReportMetadata metadata;
    ReportTheme theme = themes::light_professional();
    int next_id = 1;

    // Find component by stable id. Returns -1 if not found.
    int index_of(int id) const {
        for (int i = 0; i < components.size(); ++i) {
            if (components[i].id == id)
                return i;
        }
        return -1;
    }

    QJsonObject to_json_obj() const {
        QJsonObject root;

        QJsonObject meta;
        meta["title"] = metadata.title;
        meta["author"] = metadata.author;
        meta["company"] = metadata.company;
        meta["date"] = metadata.date;
        meta["header_left"] = metadata.header_left;
        meta["header_center"] = metadata.header_center;
        meta["header_right"] = metadata.header_right;
        meta["footer_left"] = metadata.footer_left;
        meta["footer_center"] = metadata.footer_center;
        meta["footer_right"] = metadata.footer_right;
        meta["show_page_numbers"] = metadata.show_page_numbers;
        root["metadata"] = meta;

        root["theme"] = theme.name;

        QJsonArray comps;
        for (const auto& c : components) {
            QJsonObject obj;
            obj["id"] = c.id;
            obj["type"] = c.type;
            obj["content"] = c.content;
            QJsonObject cfg;
            for (auto it = c.config.cbegin(); it != c.config.cend(); ++it)
                cfg[it.key()] = it.value();
            obj["config"] = cfg;
            comps.append(obj);
        }
        root["components"] = comps;
        root["next_id"] = next_id;
        root["schema_version"] = 1;
        return root;
    }

    QString to_json() const {
        return QString::fromUtf8(QJsonDocument(to_json_obj()).toJson(QJsonDocument::Indented));
    }

    static Result<ReportDocument> from_json(const QString& json) {
        QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
        if (!doc.isObject())
            return Result<ReportDocument>::err("Not a JSON object");
        return from_json_obj(doc.object());
    }

    static Result<ReportDocument> from_json_obj(const QJsonObject& root) {
        ReportDocument out;

        if (root.contains("metadata")) {
            QJsonObject m = root["metadata"].toObject();
            out.metadata.title = m.value("title").toString("Untitled Report");
            out.metadata.author = m.value("author").toString("Analyst");
            out.metadata.company = m.value("company").toString("Fincept Corporation");
            out.metadata.date = m.value("date").toString();
            out.metadata.header_left = m.value("header_left").toString();
            out.metadata.header_center = m.value("header_center").toString();
            out.metadata.header_right = m.value("header_right").toString();
            out.metadata.footer_left = m.value("footer_left").toString();
            out.metadata.footer_center = m.value("footer_center").toString("Page {page}");
            out.metadata.footer_right = m.value("footer_right").toString();
            out.metadata.show_page_numbers = m.value("show_page_numbers").toBool(true);
        }

        if (root.contains("theme"))
            out.theme = themes::by_name(root["theme"].toString());

        out.next_id = 1;
        if (root.contains("components")) {
            for (const auto& val : root["components"].toArray()) {
                QJsonObject obj = val.toObject();
                ReportComponent c;
                c.id = obj["id"].toInt(out.next_id);
                c.type = obj["type"].toString();
                c.content = obj["content"].toString();
                QJsonObject cfg = obj["config"].toObject();
                for (const auto& k : cfg.keys())
                    c.config[k] = cfg[k].toString();
                out.components.append(c);
                if (c.id >= out.next_id)
                    out.next_id = c.id + 1;
            }
        }
        if (root.contains("next_id"))
            out.next_id = root["next_id"].toInt(out.next_id);

        return Result<ReportDocument>::ok(std::move(out));
    }

    // Allocate the next stable id and bump the counter. Used by both manual
    // adds (from the screen) and LLM-driven adds (from MCP tools).
    int allocate_id() { return next_id++; }
};

} // namespace fincept::report
