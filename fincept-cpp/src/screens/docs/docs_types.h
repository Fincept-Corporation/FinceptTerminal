#pragma once
// Documentation types — sections and subsections for static docs viewer

#include <string>
#include <vector>
#include <optional>

namespace fincept::docs {

struct DocSubsection {
    std::string id;
    std::string title;
    std::string content;
    std::optional<std::string> code_example;
};

struct DocSection {
    std::string id;
    std::string title;
    std::string icon_label; // e.g. "[FS]", "[TAB]", "[FT]", "[API]", "[TUT]"
    std::vector<DocSubsection> subsections;
};

} // namespace fincept::docs
