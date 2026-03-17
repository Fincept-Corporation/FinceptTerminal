#pragma once
// Text Preprocessor — cleans text for natural speech output
// Mirrors Tauri ttsService.ts cleanText() pipeline:
//   LaTeX formulas → spoken math
//   Markdown tables → spoken rows
//   Financial abbreviations → expanded
//   Dates/numbers → natural language
//   Markdown/emoji → stripped

#include <string>

namespace fincept::voice {

class TextPreprocessor {
public:
    // Full pipeline: apply all transforms
    static std::string clean(const std::string& text);

private:
    static std::string convert_formulas(const std::string& text);
    static std::string convert_tables(const std::string& text);
    static std::string convert_dates(const std::string& text);
    static std::string convert_numbers(const std::string& text);
    static std::string expand_abbreviations(const std::string& text);
    static std::string strip_markdown(const std::string& text);
};

} // namespace fincept::voice
