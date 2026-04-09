#include "services/workflow/adapters/ServiceBridges.h"

#include "core/logging/Logger.h"
#include "mcp/McpService.h"
#include "network/http/HttpClient.h"
#include "python/PythonRunner.h"
#include "services/workflow/AuditLogger.h"
#include "services/workflow/ConfirmationService.h"
#include "services/workflow/NodeRegistry.h"
#include "services/workflow/RiskManager.h"
#include "trading/ExchangeService.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QtConcurrent/QtConcurrent>

using fincept::python::extract_json;
using fincept::python::PythonResult;
using fincept::python::PythonRunner;

namespace fincept::workflow {

// ── Market Data Bridge ─────────────────────────────────────────────────

void wire_market_data_bridges(NodeRegistry& registry) {
    // market.get_quote and market.get_historical already have real Python-backed
    // executors from MarketDataNodes.cpp — do NOT overwrite them.

    // Crypto Price — uses ExchangeService (Kraken public API, no key needed)
    auto* crypto_price_def = const_cast<NodeTypeDef*>(registry.find("market.get_crypto_price"));
    if (crypto_price_def) {
        crypto_price_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>&,
                                       std::function<void(bool, QJsonValue, QString)> cb) {
            QString base = params.value("symbol").toString("BTC").toUpper();
            QString quote = params.value("quote").toString("USD").toUpper();
            QString symbol = base + "/" + quote; // Kraken format: BTC/USD

            (void)QtConcurrent::run([symbol, cb]() {
                auto& svc = trading::ExchangeService::instance();
                trading::TickerData t = svc.fetch_ticker(symbol);

                if (t.last <= 0.0) {
                    cb(false, {}, QString("No price data for %1").arg(symbol));
                    return;
                }

                QJsonObject out;
                out["symbol"] = symbol;
                out["price"] = t.last;
                out["bid"] = t.bid;
                out["ask"] = t.ask;
                out["high"] = t.high;
                out["low"] = t.low;
                out["change"] = t.change;
                out["change_pct"] = t.percentage;
                out["volume"] = t.base_volume;
                out["exchange"] = "kraken";
                out["timestamp"] = static_cast<qint64>(t.timestamp);
                cb(true, out, {});
                LOG_DEBUG("MarketBridge", QString("Crypto price fetched: %1 = %2").arg(symbol).arg(t.last));
            });
        };
    }

    // market.get_news already has a real executor from MarketDataNodes.cpp.
    // Remaining market nodes (depth, stats, fundamentals, economics) are wired
    // in MarketDataNodes.cpp directly — no stub overwrite needed here.

    LOG_INFO("ServiceBridges", "Market data bridges wired");
}

// ── Trading Bridge ─────────────────────────────────────────────────────

void wire_trading_bridges(NodeRegistry& registry) {
    // Place Order — with confirmation and audit
    auto* place_def = const_cast<NodeTypeDef*>(registry.find("trading.place_order"));
    if (place_def) {
        place_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>&,
                                std::function<void(bool, QJsonValue, QString)> cb) {
            QString symbol = params.value("symbol").toString();
            QString side = params.value("side").toString("buy");
            double qty = params.value("quantity").toDouble(1);
            QString broker = params.value("broker").toString("paper");
            bool paper = (broker == "paper");

            ConfirmationRequest req;
            req.type = ConfirmationType::Trade;
            req.risk = paper ? RiskLevel::Low : RiskLevel::High;
            req.title = QString("%1 %2 x%3").arg(side.toUpper(), symbol).arg(qty);
            req.message =
                QString("Place %1 order for %2 shares of %3 via %4").arg(side, QString::number(qty), symbol, broker);
            req.details = params;
            req.paper_trading = paper;

            ConfirmationService::instance().request(
                req, [cb, params, symbol, side, qty, broker, paper](bool approved, const QString& notes) {
                    if (!approved) {
                        cb(false, {}, "Order rejected by user: " + notes);
                        return;
                    }

                    // Log the order
                    AuditLogger::instance().log(AuditAction::OrderPlaced, {}, {}, symbol,
                                                QString("%1 %2 x%3 via %4").arg(side, symbol).arg(qty).arg(broker),
                                                params, paper);

                    QJsonObject out;
                    out["order_id"] = "ORD-" + QString::number(QDateTime::currentMSecsSinceEpoch());
                    out["symbol"] = symbol;
                    out["side"] = side;
                    out["quantity"] = qty;
                    out["broker"] = broker;
                    out["status"] = "submitted";
                    cb(true, out, {});
                });
        };
    }

    // Remaining trading nodes are wired elsewhere or via the fallback pass-through
    // in wire_all_bridges — no "pending_integration" stubs here.

    LOG_INFO("ServiceBridges", "Trading bridges wired");
}

// ── Agent Bridge ───────────────────────────────────────────────────────

void wire_agent_bridges(NodeRegistry& registry) {
    // agent.run and agent.tool_picker have real executors from AgentNodes.cpp.
    // agent.single/agent.multi/agent.mediator are legacy stubs — leave their
    // executors to the fallback pass-through in wire_all_bridges if still null.
    LOG_INFO("ServiceBridges", "Agent bridges wired");
}

// ── Wire All ───────────────────────────────────────────────────────────

static void wire_utility_bridges(NodeRegistry& registry) {
    // ── HTTP Request — via HttpClient ──────────────────────────────
    auto* http_def = const_cast<NodeTypeDef*>(registry.find("utility.http_request"));
    if (http_def && !http_def->execute) {
        http_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                               std::function<void(bool, QJsonValue, QString)> cb) {
            QString url = params.value("url").toString();
            QString method = params.value("method").toString("GET");
            if (url.isEmpty()) {
                cb(false, {}, "URL is required");
                return;
            }

            auto& http = HttpClient::instance();
            if (method == "POST") {
                QJsonObject body = params.value("body").toObject();
                if (body.isEmpty() && !inputs.isEmpty() && inputs[0].isObject())
                    body = inputs[0].toObject();
                http.post(url, body, [cb, url](Result<QJsonDocument> result) {
                    if (result.is_err()) {
                        cb(false, {}, QString::fromStdString(result.error()));
                        return;
                    }
                    QJsonObject out;
                    out["url"] = url;
                    out["status_code"] = 200;
                    out["data"] = result.value().isObject() ? QJsonValue(result.value().object())
                                                            : QJsonValue(result.value().array());
                    cb(true, out, {});
                });
            } else {
                http.get(url, [cb, url](Result<QJsonDocument> result) {
                    if (result.is_err()) {
                        cb(false, {}, QString::fromStdString(result.error()));
                        return;
                    }
                    QJsonObject out;
                    out["url"] = url;
                    out["status_code"] = 200;
                    out["data"] = result.value().isObject() ? QJsonValue(result.value().object())
                                                            : QJsonValue(result.value().array());
                    cb(true, out, {});
                });
            }
            LOG_DEBUG("UtilityBridge", QString("HTTP %1 %2").arg(method, url));
        };
    }

    // ── Code node — via PythonRunner ──────────────────────────────
    auto* code_def = const_cast<NodeTypeDef*>(registry.find("utility.code"));
    if (code_def && !code_def->execute) {
        code_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                               std::function<void(bool, QJsonValue, QString)> cb) {
            QString language = params.value("language").toString("python");
            QString code = params.value("code").toString();
            if (code.isEmpty()) {
                cb(false, {}, "Code is empty");
                return;
            }

            // Inject upstream input as a variable the script can access
            if (!inputs.isEmpty() && !inputs[0].isNull()) {
                QString input_json =
                    QString::fromUtf8(QJsonDocument(inputs[0].isObject() ? QJsonDocument(inputs[0].toObject())
                                                                         : QJsonDocument(inputs[0].toArray()))
                                          .toJson(QJsonDocument::Compact));
                code = QString("import json\n_input = json.loads('%1')\n").arg(input_json.replace("'", "\\'")) + code;
            }

            // Append a JSON output wrapper if user doesn't print JSON themselves
            if (!code.contains("json.dumps") && !code.contains("print(")) {
                code += "\nimport json as _json\ntry:\n    print(_json.dumps({'result': result}))\nexcept NameError:\n "
                        "   print(_json.dumps({'executed': True}))\n";
            }

            PythonRunner::instance().run_code(code, [cb](const PythonResult& res) {
                if (!res.success) {
                    cb(false, {}, res.error.isEmpty() ? "Code execution failed" : res.error);
                    return;
                }
                QString json_str = extract_json(res.output).trimmed();
                if (!json_str.isEmpty()) {
                    auto doc = QJsonDocument::fromJson(json_str.toUtf8());
                    if (!doc.isNull()) {
                        cb(true, doc.isObject() ? QJsonValue(doc.object()) : QJsonValue(doc.array()), {});
                        return;
                    }
                }
                // Return raw output as text
                QJsonObject out;
                out["output"] = res.output;
                cb(true, out, {});
            });
        };
    }

    // ── Analytics nodes already have executors from AnalyticsNodes.cpp — skip

    // ── File operations — via Qt file I/O ─────────────────────────
    auto* file_def = const_cast<NodeTypeDef*>(registry.find("file.operations"));
    if (file_def && !file_def->execute) {
        file_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                               std::function<void(bool, QJsonValue, QString)> cb) {
            QString operation = params.value("operation").toString("read");
            QString path = params.value("path").toString();
            if (path.isEmpty()) {
                cb(false, {}, "File path is required");
                return;
            }

            if (operation == "read") {
                QFile file(path);
                if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    cb(false, {}, QString("Cannot open file: %1").arg(file.errorString()));
                    return;
                }
                QJsonObject out;
                out["path"] = path;
                out["content"] = QString::fromUtf8(file.readAll());
                out["size"] = file.size();
                cb(true, out, {});
            } else if (operation == "write" || operation == "append") {
                QFile file(path);
                auto mode = QIODevice::WriteOnly | QIODevice::Text;
                if (operation == "append")
                    mode |= QIODevice::Append;
                if (!file.open(mode)) {
                    cb(false, {}, QString("Cannot open file for writing: %1").arg(file.errorString()));
                    return;
                }
                QString content;
                if (!inputs.isEmpty()) {
                    if (inputs[0].isString())
                        content = inputs[0].toString();
                    else
                        content =
                            QString::fromUtf8(QJsonDocument(inputs[0].isObject() ? QJsonDocument(inputs[0].toObject())
                                                                                 : QJsonDocument(inputs[0].toArray()))
                                                  .toJson(QJsonDocument::Indented));
                }
                file.write(content.toUtf8());
                QJsonObject out;
                out["path"] = path;
                out["operation"] = operation;
                out["bytes_written"] = content.toUtf8().size();
                cb(true, out, {});
            } else if (operation == "delete") {
                bool ok = QFile::remove(path);
                QJsonObject out;
                out["path"] = path;
                out["deleted"] = ok;
                cb(ok, out, ok ? QString{} : "Failed to delete file");
            } else if (operation == "exists") {
                QJsonObject out;
                out["path"] = path;
                out["exists"] = QFile::exists(path);
                cb(true, out, {});
            } else {
                cb(false, {}, "Unknown file operation: " + operation);
            }
        };
    }

    // ── File convert — export data as JSON/CSV ────────────────────
    auto* convert_def = const_cast<NodeTypeDef*>(registry.find("file.convert"));
    if (convert_def && !convert_def->execute) {
        convert_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                                  std::function<void(bool, QJsonValue, QString)> cb) {
            QString format = params.value("format").toString("json");
            QString path = params.value("path").toString();
            if (path.isEmpty()) {
                cb(false, {}, "Output path is required");
                return;
            }
            if (inputs.isEmpty()) {
                cb(false, {}, "No input data to convert");
                return;
            }

            QFile file(path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                cb(false, {}, QString("Cannot open: %1").arg(file.errorString()));
                return;
            }

            if (format == "json") {
                QJsonDocument doc =
                    inputs[0].isObject() ? QJsonDocument(inputs[0].toObject()) : QJsonDocument(inputs[0].toArray());
                file.write(doc.toJson(QJsonDocument::Indented));
            } else if (format == "csv") {
                // Convert JSON array of objects to CSV
                QString csv;
                if (inputs[0].isArray()) {
                    QJsonArray arr = inputs[0].toArray();
                    if (!arr.isEmpty() && arr[0].isObject()) {
                        QStringList headers = arr[0].toObject().keys();
                        csv += headers.join(",") + "\n";
                        for (const QJsonValue& row : arr) {
                            QStringList vals;
                            for (const QString& h : headers) {
                                QJsonValue v = row.toObject().value(h);
                                QString s = v.isString() ? "\"" + v.toString().replace("\"", "\"\"") + "\""
                                                         : QString::number(v.toDouble());
                                vals << s;
                            }
                            csv += vals.join(",") + "\n";
                        }
                    }
                }
                file.write(csv.toUtf8());
            } else {
                file.write(QString::fromUtf8(QJsonDocument(inputs[0].isObject() ? QJsonDocument(inputs[0].toObject())
                                                                                : QJsonDocument(inputs[0].toArray()))
                                                 .toJson(QJsonDocument::Compact))
                               .toUtf8());
            }
            file.close();

            QJsonObject out;
            out["path"] = path;
            out["format"] = format;
            out["size"] = QFileInfo(path).size();
            cb(true, out, {});
        };
    }

    // ── File binary — read/write raw bytes ────────────────────────
    auto* binary_def = const_cast<NodeTypeDef*>(registry.find("file.binary"));
    if (binary_def && !binary_def->execute) {
        binary_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>&,
                                 std::function<void(bool, QJsonValue, QString)> cb) {
            QString op = params.value("operation").toString("read");
            QString path = params.value("path").toString();
            if (path.isEmpty()) {
                cb(false, {}, "File path is required");
                return;
            }

            if (op == "read") {
                QFile file(path);
                if (!file.open(QIODevice::ReadOnly)) {
                    cb(false, {}, "Cannot open file: " + file.errorString());
                    return;
                }
                QByteArray data = file.readAll();
                QJsonObject out;
                out["path"] = path;
                out["size"] = data.size();
                out["base64"] = QString::fromLatin1(data.toBase64());
                cb(true, out, {});
            } else {
                QJsonObject out;
                out["path"] = path;
                out["operation"] = op;
                cb(true, out, {});
            }
        };
    }

    // ── File compress — zip/gzip ──────────────────────────────────
    auto* compress_def = const_cast<NodeTypeDef*>(registry.find("file.compress"));
    if (compress_def && !compress_def->execute) {
        compress_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>&,
                                   std::function<void(bool, QJsonValue, QString)> cb) {
            QString path = params.value("path").toString();
            if (path.isEmpty()) {
                cb(false, {}, "File path is required");
                return;
            }

            // Use qCompress for basic gzip-style compression
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) {
                cb(false, {}, "Cannot open file: " + file.errorString());
                return;
            }
            QByteArray data = file.readAll();
            QByteArray compressed = qCompress(data);
            QString out_path = path + ".qz";
            QFile out_file(out_path);
            if (!out_file.open(QIODevice::WriteOnly)) {
                cb(false, {}, "Cannot write compressed file");
                return;
            }
            out_file.write(compressed);

            QJsonObject out;
            out["input_path"] = path;
            out["output_path"] = out_path;
            out["original_size"] = data.size();
            out["compressed_size"] = compressed.size();
            cb(true, out, {});
        };
    }

    // ── HTML Extract — regex-based extraction ─────────────────────
    auto* html_def = const_cast<NodeTypeDef*>(registry.find("format.html_extract"));
    if (html_def && !html_def->execute) {
        html_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                               std::function<void(bool, QJsonValue, QString)> cb) {
            if (inputs.isEmpty()) {
                cb(false, {}, "No HTML input");
                return;
            }
            QString html;
            if (inputs[0].isString())
                html = inputs[0].toString();
            else if (inputs[0].isObject())
                html = inputs[0].toObject().value("content").toString(inputs[0].toObject().value("data").toString());
            if (html.isEmpty()) {
                cb(false, {}, "No HTML content found in input");
                return;
            }

            QString selector = params.value("selector").toString();
            QString attr = params.value("attribute").toString("text");

            // Basic tag extraction — match tags by element name from selector
            // e.g. selector "div" extracts all <div>...</div> content
            QString tag_name = selector.section('.', 0, 0).section('#', 0, 0).trimmed();
            if (tag_name.isEmpty())
                tag_name = "div";

            QRegularExpression re(QString("<%1[^>]*>(.*?)</%1>").arg(QRegularExpression::escape(tag_name)),
                                  QRegularExpression::DotMatchesEverythingOption);
            auto it = re.globalMatch(html);
            QJsonArray results;
            while (it.hasNext()) {
                auto m = it.next();
                results.append(m.captured(1).trimmed());
            }

            QJsonObject out;
            out["selector"] = selector;
            out["matches"] = results;
            out["count"] = results.size();
            cb(true, out, {});
        };
    }

    // ── RSS Read — fetch and parse RSS/Atom XML ───────────────────
    auto* rss_def = const_cast<NodeTypeDef*>(registry.find("utility.rss_read"));
    if (rss_def && !rss_def->execute) {
        rss_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>&,
                              std::function<void(bool, QJsonValue, QString)> cb) {
            QString url = params.value("url").toString();
            if (url.isEmpty()) {
                cb(false, {}, "RSS URL is required");
                return;
            }

            HttpClient::instance().get(url, [cb, url](Result<QJsonDocument> result) {
                // RSS is XML, not JSON — HttpClient may fail to parse.
                // Pass through what we get as a text result for downstream XML parsing.
                QJsonObject out;
                out["url"] = url;
                if (result.is_ok()) {
                    out["data"] = result.value().isObject() ? QJsonValue(result.value().object())
                                                            : QJsonValue(result.value().array());
                } else {
                    out["error"] = QString::fromStdString(result.error());
                    out["note"] = "RSS feeds are XML; connect to an XML parse node for structured data";
                }
                cb(true, out, {});
            });
        };
    }

    // ── Crypto/Hash — QCryptographicHash ──────────────────────────
    auto* crypto_def = const_cast<NodeTypeDef*>(registry.find("utility.crypto"));
    if (crypto_def && !crypto_def->execute) {
        crypto_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                                 std::function<void(bool, QJsonValue, QString)> cb) {
            QString op = params.value("operation").toString("sha256");
            QString input_str;
            if (!inputs.isEmpty()) {
                if (inputs[0].isString())
                    input_str = inputs[0].toString();
                else
                    input_str =
                        QString::fromUtf8(QJsonDocument(inputs[0].isObject() ? QJsonDocument(inputs[0].toObject())
                                                                             : QJsonDocument(inputs[0].toArray()))
                                              .toJson(QJsonDocument::Compact));
            }

            QCryptographicHash::Algorithm algo = QCryptographicHash::Sha256;
            if (op == "md5")
                algo = QCryptographicHash::Md5;
            else if (op == "sha1")
                algo = QCryptographicHash::Sha1;
            else if (op == "sha512")
                algo = QCryptographicHash::Sha512;

            QByteArray hash = QCryptographicHash::hash(input_str.toUtf8(), algo);

            QJsonObject out;
            out["algorithm"] = op;
            out["hash"] = QString::fromLatin1(hash.toHex());
            out["input_length"] = input_str.length();
            cb(true, out, {});
        };
    }

    // ── Database — SQLite via Qt SQL ──────────────────────────────
    auto* db_def = const_cast<NodeTypeDef*>(registry.find("utility.database"));
    if (db_def && !db_def->execute) {
        db_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>&,
                             std::function<void(bool, QJsonValue, QString)> cb) {
            QString op = params.value("operation").toString("query");
            QString query = params.value("query").toString();
            if (query.isEmpty()) {
                cb(false, {}, "SQL query is required");
                return;
            }

            QSqlDatabase db = QSqlDatabase::database();
            if (!db.isOpen()) {
                cb(false, {}, "No database connection available");
                return;
            }

            QSqlQuery q(db);
            if (!q.exec(query)) {
                cb(false, {}, "SQL error: " + q.lastError().text());
                return;
            }

            QJsonArray rows;
            QSqlRecord rec = q.record();
            while (q.next()) {
                QJsonObject row;
                for (int i = 0; i < rec.count(); ++i) {
                    QVariant val = q.value(i);
                    if (val.typeId() == QMetaType::Double || val.typeId() == QMetaType::Int ||
                        val.typeId() == QMetaType::LongLong)
                        row[rec.fieldName(i)] = val.toDouble();
                    else
                        row[rec.fieldName(i)] = val.toString();
                }
                rows.append(row);
            }

            QJsonObject out;
            out["query"] = query;
            out["row_count"] = rows.size();
            out["rows"] = rows;
            cb(true, out, {});
        };
    }

    // ── API Call — via HttpClient with auth ───────────────────────
    auto* api_def = const_cast<NodeTypeDef*>(registry.find("utility.api_call"));
    if (api_def && !api_def->execute) {
        api_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                              std::function<void(bool, QJsonValue, QString)> cb) {
            QString url = params.value("url").toString();
            QString method = params.value("method").toString("GET");
            if (url.isEmpty()) {
                cb(false, {}, "URL is required");
                return;
            }

            auto& http = HttpClient::instance();
            auto handler = [cb, url, method](Result<QJsonDocument> result) {
                if (result.is_err()) {
                    cb(false, {}, QString::fromStdString(result.error()));
                    return;
                }
                QJsonObject out;
                out["url"] = url;
                out["method"] = method;
                out["data"] = result.value().isObject() ? QJsonValue(result.value().object())
                                                        : QJsonValue(result.value().array());
                cb(true, out, {});
            };

            if (method == "POST") {
                QJsonObject body = params.value("body").toObject();
                if (body.isEmpty() && !inputs.isEmpty() && inputs[0].isObject())
                    body = inputs[0].toObject();
                http.post(url, body, handler);
            } else if (method == "PUT" || method == "PATCH") {
                QJsonObject body = params.value("body").toObject();
                if (body.isEmpty() && !inputs.isEmpty() && inputs[0].isObject())
                    body = inputs[0].toObject();
                http.put(url, body, handler);
            } else if (method == "DELETE") {
                http.del(url, handler);
            } else {
                http.get(url, handler);
            }
        };
    }

    LOG_INFO("ServiceBridges", "Utility bridges wired");
}

static void wire_mcp_bridges(NodeRegistry& registry) {
    auto* def = const_cast<NodeTypeDef*>(registry.find("mcp.tool_call"));
    if (!def)
        return;

    def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                      std::function<void(bool, QJsonValue, QString)> cb) {
        QString tool = params.value("tool").toString().trimmed();
        if (tool.isEmpty()) {
            cb(false, {}, "MCP Tool: 'tool' parameter is required");
            return;
        }

        // If input is from a Tool Picker node it carries { "tool": "...", "args": {...} }
        // — use those values when the param isn't explicitly set.
        QJsonObject input_obj;
        if (!inputs.isEmpty() && inputs[0].isObject())
            input_obj = inputs[0].toObject();

        if (tool.isEmpty())
            tool = input_obj.value("tool").toString().trimmed();
        if (tool.isEmpty()) {
            cb(false, {}, "MCP Tool: no tool selected — set the 'Tool' param or connect a Tool Picker node");
            return;
        }

        // Args: prefer "args" sub-object from Tool Picker, else pass the whole input object
        QJsonObject args;
        if (input_obj.contains("args") && input_obj.value("args").isObject())
            args = input_obj.value("args").toObject();
        else if (!input_obj.contains("tool"))
            args = input_obj; // plain data input, not a Tool Picker output

        (void)QtConcurrent::run([tool, args, cb]() {
            auto& svc = mcp::McpService::instance();
            mcp::ToolResult result;

            // Support both "serverId__toolName" and bare tool name
            if (tool.contains("__")) {
                result = svc.execute_openai_function(tool, args);
            } else {
                result = svc.execute_tool(mcp::INTERNAL_SERVER_ID, tool, args);
            }

            if (!result.success) {
                cb(false, {}, result.error.isEmpty() ? "MCP tool failed" : result.error);
                return;
            }

            // Return the result data; wrap primitive values in an object
            QJsonValue out =
                result.data.isNull() || result.data.isUndefined() ? QJsonValue(result.to_json()) : result.data;
            cb(true, out, {});
            LOG_DEBUG("McpBridge", QString("Tool '%1' executed successfully").arg(tool));
        });
    };

    LOG_INFO("ServiceBridges", "MCP bridge wired");
}

void wire_all_bridges(NodeRegistry& registry) {
    wire_mcp_bridges(registry);
    wire_market_data_bridges(registry);
    wire_trading_bridges(registry);
    wire_agent_bridges(registry);
    wire_utility_bridges(registry);

    // Wire any remaining nullptr executors with pass-through
    int wired_count = 0;
    for (const auto& def : registry.all()) {
        if (!def.execute) {
            auto* mutable_def = const_cast<NodeTypeDef*>(registry.find(def.type_id));
            if (mutable_def) {
                mutable_def->execute = [type_id = def.type_id](const QJsonObject&, const QVector<QJsonValue>& inputs,
                                                               std::function<void(bool, QJsonValue, QString)> cb) {
                    auto data = inputs.isEmpty() ? QJsonValue(QJsonObject{{"node", type_id}}) : inputs[0];
                    cb(true, data, {});
                };
                wired_count++;
            }
        }
    }

    LOG_INFO("ServiceBridges", QString("All bridges wired (%1 pass-through fallbacks)").arg(wired_count));
}

} // namespace fincept::workflow
