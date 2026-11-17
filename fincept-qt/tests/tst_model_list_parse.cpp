// Unit tests for src/services/llm/ModelListParse.{h,cpp} (pure, Qt Core only).
//
// parse_model_list() is what LlmService::fetch_models() runs on a worker thread
// (it used to run on the GUI thread; OpenRouter's list alone is over 1 MB): a
// provider's "list models" answer → the sorted ids the settings model picker shows.
//
// MSVC: raw string literals are kept out of macro arguments (QVERIFY / QCOMPARE).

#include "services/llm/ModelListParse.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QString>
#include <QStringList>
#include <QTest>

using fincept::ai_chat::parse_model_list;
using fincept::ai_chat::ParsedModelList;

class TstModelListParse : public QObject {
    Q_OBJECT

  private slots:
    void openai_data_ids_sorted_case_insensitively();
    void anthropic_data_ids();
    void openrouter_large_list();
    void ollama_tags();
    void gemini_only_generate_content_and_prefix_removed();
    void fincept_shapes_and_fallback();
    void entries_without_an_id_are_skipped();
    void error_object_in_200_parses_to_an_empty_list();
    void unreadable_bodies_are_not_parsed();
};

void TstModelListParse::openai_data_ids_sorted_case_insensitively() {
    const QByteArray body = R"({"object":"list","data":[)"
                            R"({"id":"gpt-4o","object":"model","created":1715367049,"owned_by":"system"},)"
                            R"({"id":"o3-mini","object":"model","created":1737146383,"owned_by":"system"},)"
                            R"({"id":"dall-e-3","object":"model","created":1698785189,"owned_by":"system"},)"
                            R"({"id":"Babbage-002","object":"model","created":1692634615,"owned_by":"system"}]})";
    const ParsedModelList r = parse_model_list(QStringLiteral("openai"), body);
    QVERIFY(r.parsed);
    const QStringList expected = {QStringLiteral("Babbage-002"), QStringLiteral("dall-e-3"), QStringLiteral("gpt-4o"),
                                  QStringLiteral("o3-mini")};
    QCOMPARE(r.models, expected);
    // Any OpenAI-compatible provider (and a custom base URL) reads the same shape.
    QCOMPARE(parse_model_list(QStringLiteral("groq"), body).models, expected);
}

void TstModelListParse::anthropic_data_ids() {
    const QByteArray body =
        R"({"data":[)"
        R"({"type":"model","id":"claude-sonnet-4-5","display_name":"Claude Sonnet 4.5","created_at":"2025-09-29T00:00:00Z"},)"
        R"({"type":"model","id":"claude-opus-4-1","display_name":"Claude Opus 4.1","created_at":"2025-08-05T00:00:00Z"},)"
        R"({"type":"model","id":"claude-haiku-4-5","display_name":"Claude Haiku 4.5","created_at":"2025-10-01T00:00:00Z"}],)"
        R"("has_more":false,"first_id":"claude-sonnet-4-5","last_id":"claude-haiku-4-5"})";
    const ParsedModelList r = parse_model_list(QStringLiteral("Anthropic"), body);
    QVERIFY(r.parsed);
    const QStringList expected = {QStringLiteral("claude-haiku-4-5"), QStringLiteral("claude-opus-4-1"),
                                  QStringLiteral("claude-sonnet-4-5")};
    QCOMPARE(r.models, expected);
}

void TstModelListParse::openrouter_large_list() {
    // OpenRouter's /api/v1/models: ~300+ entries with pricing, architecture and
    // parameter lists — well over 1 MB. Built here at that size.
    QByteArray body = R"({"data":[)";
    const int n = 3000;
    for (int i = n - 1; i >= 0; --i) {
        const QByteArray id = "vendor" + QByteArray::number(i % 37) + "/model-" + QByteArray::number(i);
        body += R"({"id":")" + id + R"(","canonical_slug":")" + id + R"(","name":"Vendor: Model )" +
                QByteArray::number(i) +
                R"(","created":1731000000,"description":"A long description of the model that goes on for a while )"
                R"(to make the payload realistic in size, mentioning context windows and tool use.",)"
                R"("context_length":131072,"architecture":{"modality":"text->text","input_modalities":["text"],)"
                R"("output_modalities":["text"],"tokenizer":"Other","instruct_type":null},)"
                R"("pricing":{"prompt":"0.0000003","completion":"0.0000012","request":"0","image":"0"},)"
                R"("top_provider":{"context_length":131072,"max_completion_tokens":8192,"is_moderated":false},)"
                R"("per_request_limits":null,"supported_parameters":["tools","tool_choice","max_tokens",)"
                R"("temperature","top_p","stop","frequency_penalty","presence_penalty","seed","response_format"]})";
        if (i > 0)
            body += ',';
    }
    body += "]}";
    QVERIFY(body.size() > 1024 * 1024);

    QElapsedTimer t;
    t.start();
    const ParsedModelList r = parse_model_list(QStringLiteral("openrouter"), body);
    qInfo("openrouter-sized list: %lld bytes parsed in %lld ms", static_cast<long long>(body.size()),
          static_cast<long long>(t.elapsed()));
    QVERIFY(r.parsed);
    QCOMPARE(r.models.size(), n);
    for (int i = 1; i < r.models.size(); ++i)
        QVERIFY(QString::compare(r.models.at(i - 1), r.models.at(i), Qt::CaseInsensitive) <= 0);
    QVERIFY(r.models.contains(QStringLiteral("vendor0/model-0")));
}

void TstModelListParse::ollama_tags() {
    const QByteArray body = R"({"models":[)"
                            R"({"name":"llama3.2:latest","model":"llama3.2:latest","modified_at":"2025-01-01T00:00:00Z",)"
                            R"("size":2019393189,"digest":"a80c4f17","details":{"family":"llama","parameter_size":"3.2B"}},)"
                            R"({"name":"Qwen2.5:7b","model":"Qwen2.5:7b","size":4683087332},)"
                            R"({"name":"deepseek-r1:8b","model":"deepseek-r1:8b"}]})";
    const ParsedModelList r = parse_model_list(QStringLiteral("Ollama"), body);
    QVERIFY(r.parsed);
    const QStringList expected = {QStringLiteral("deepseek-r1:8b"), QStringLiteral("llama3.2:latest"),
                                  QStringLiteral("Qwen2.5:7b")};
    QCOMPARE(r.models, expected);
    // The OpenAI-compatible reading of the same body finds nothing ("data" absent).
    QVERIFY(parse_model_list(QStringLiteral("openai"), body).models.isEmpty());
}

void TstModelListParse::gemini_only_generate_content_and_prefix_removed() {
    const QByteArray body =
        R"({"models":[)"
        R"({"name":"models/gemini-2.5-pro","supportedGenerationMethods":["generateContent","countTokens"]},)"
        R"({"name":"models/text-embedding-004","supportedGenerationMethods":["embedContent"]},)"
        R"({"name":"models/gemini-2.0-flash","supportedGenerationMethods":["generateContent"]},)"
        R"({"name":"aqa","supportedGenerationMethods":["generateAnswer"]}],"nextPageToken":"x"})";
    const QStringList expected = {QStringLiteral("gemini-2.0-flash"), QStringLiteral("gemini-2.5-pro")};
    QCOMPARE(parse_model_list(QStringLiteral("gemini"), body).models, expected);
    QCOMPARE(parse_model_list(QStringLiteral("google"), body).models, expected);
}

void TstModelListParse::fincept_shapes_and_fallback() {
    const QByteArray nested = R"({"success":true,"data":{"models":["MiniMax-M2.5","Custom-A"]}})";
    const QByteArray flat = R"({"data":[{"id":"x-model"},"y-model"]})";
    const QByteArray empty = R"({"data":{}})";
    const QStringList nested_expected = {QStringLiteral("Custom-A"), QStringLiteral("MiniMax-M2.5")};
    const QStringList flat_expected = {QStringLiteral("x-model"), QStringLiteral("y-model")};
    const QStringList fallback = {QStringLiteral("MiniMax-M2.5"), QStringLiteral("MiniMax-M2.7"),
                                  QStringLiteral("MiniMax-M2.7-highspeed")};
    QCOMPARE(parse_model_list(QStringLiteral("fincept"), nested).models, nested_expected);
    QCOMPARE(parse_model_list(QStringLiteral("fincept"), flat).models, flat_expected);
    QCOMPARE(parse_model_list(QStringLiteral("fincept"), empty).models, fallback);
}

void TstModelListParse::entries_without_an_id_are_skipped() {
    const QByteArray body = R"({"data":[{"id":""},{"name":"no id"},"bare string",{"id":"kept"}]})";
    const QStringList expected = {QStringLiteral("kept")};
    QCOMPARE(parse_model_list(QStringLiteral("xai"), body).models, expected);
}

void TstModelListParse::error_object_in_200_parses_to_an_empty_list() {
    // fetch_models() turns an empty list into "No models found".
    const QByteArray body = R"({"error":{"message":"Invalid API key","type":"invalid_request_error"}})";
    const ParsedModelList r = parse_model_list(QStringLiteral("openai"), body);
    QVERIFY(r.parsed);
    QVERIFY(r.models.isEmpty());
}

void TstModelListParse::unreadable_bodies_are_not_parsed() {
    const QByteArray array_root = R"([{"id":"gpt-4o"}])";
    for (const QByteArray& body : {QByteArray(), QByteArray("<html>502 Bad Gateway</html>"), QByteArray("{broken"),
                                   array_root}) {
        const ParsedModelList r = parse_model_list(QStringLiteral("openai"), body);
        QVERIFY2(!r.parsed, body.constData());
        QVERIFY(r.models.isEmpty());
    }
    // Not even the Fincept fallback for a body that is not a JSON object (as before).
    QVERIFY(parse_model_list(QStringLiteral("fincept"), "nope").models.isEmpty());
}

QTEST_GUILESS_MAIN(TstModelListParse)
#include "tst_model_list_parse.moc"
