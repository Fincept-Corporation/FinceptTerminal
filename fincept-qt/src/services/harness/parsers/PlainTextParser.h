#pragma once
// PlainTextParser.h — for Transport::PlainText harnesses, whose headless mode
// prints the answer as plain text (e.g. GitHub Copilot CLI `-p`, whose JSONL
// envelope is not documented).
//
// Every stdout line (terminal escape sequences removed) becomes part of ONE
// Text message: partial events while it grows, the complete event and
// facts().final_text in finish(). Lines that read like error messages also set
// facts().last_error. There is no session id, usage or result line.

#include "services/harness/IHarnessAdapter.h"
#include "services/harness/parsers/StreamParserSupport.h"

namespace fincept::harness {

class PlainTextParser final : public IStreamParser {
  public:
    QVector<services::coding_agents::AgentEvent> parse_line(const QString& line) override;
    QVector<services::coding_agents::AgentEvent> finish() override;
    const services::coding_agents::StreamFacts& facts() const override { return facts_; }

  private:
    services::coding_agents::StreamFacts facts_;
    stream_support::DeltaAccumulator acc_;
    int pending_blank_lines_ = 0; // blank lines kept only between text lines
};

} // namespace fincept::harness
