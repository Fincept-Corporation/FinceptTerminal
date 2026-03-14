#include "notebook_io.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>

namespace fincept::code_editor::io {

using json = nlohmann::json;

// =============================================================================
// Helper: split string by newlines, preserving trailing newlines for Jupyter format
// =============================================================================
static std::vector<std::string> split_source(const std::string& s) {
    std::vector<std::string> lines;
    if (s.empty()) return lines;

    size_t pos = 0;
    while (pos < s.size()) {
        size_t nl = s.find('\n', pos);
        if (nl == std::string::npos) {
            lines.push_back(s.substr(pos));
            break;
        }
        lines.push_back(s.substr(pos, nl - pos + 1)); // include the \n
        pos = nl + 1;
    }
    return lines;
}

// =============================================================================
// Load .ipynb
// =============================================================================
Notebook load_notebook(const std::string& file_path) {
    Notebook nb;

    std::ifstream f(file_path);
    if (!f.is_open()) return nb;

    json j;
    try {
        f >> j;
    } catch (...) {
        return nb;
    }

    // Metadata
    if (j.contains("metadata")) {
        auto& meta = j["metadata"];
        if (meta.contains("kernelspec")) {
            nb.kernel_name = meta["kernelspec"].value("name", "python3");
        }
        if (meta.contains("language_info")) {
            nb.language = meta["language_info"].value("name", "python");
        }
    }
    nb.nbformat = j.value("nbformat", 4);
    nb.nbformat_minor = j.value("nbformat_minor", 5);

    // Cells
    if (j.contains("cells") && j["cells"].is_array()) {
        for (auto& cell_j : j["cells"]) {
            NotebookCell cell;
            cell.id = cell_j.value("id", generate_cell_id());
            cell.cell_type = cell_j.value("cell_type", "code");

            // Source: array of strings → join
            if (cell_j.contains("source")) {
                if (cell_j["source"].is_array()) {
                    std::string src;
                    for (auto& line : cell_j["source"]) {
                        src += line.get<std::string>();
                    }
                    cell.source = src;
                } else if (cell_j["source"].is_string()) {
                    cell.source = cell_j["source"].get<std::string>();
                }
            }

            // Execution count
            if (cell_j.contains("execution_count") && !cell_j["execution_count"].is_null()) {
                cell.execution_count = cell_j["execution_count"].get<int>();
            }

            // Outputs
            if (cell_j.contains("outputs") && cell_j["outputs"].is_array()) {
                for (auto& out_j : cell_j["outputs"]) {
                    CellOutput out;
                    out.output_type = out_j.value("output_type", "");

                    // Text (stream)
                    if (out_j.contains("text")) {
                        if (out_j["text"].is_array()) {
                            for (auto& t : out_j["text"])
                                out.text += t.get<std::string>();
                        } else if (out_j["text"].is_string()) {
                            out.text = out_j["text"].get<std::string>();
                        }
                    }

                    // Data (execute_result, display_data)
                    if (out_j.contains("data") && out_j["data"].is_object()) {
                        if (out_j["data"].contains("text/plain")) {
                            auto& tp = out_j["data"]["text/plain"];
                            if (tp.is_array()) {
                                for (auto& t : tp)
                                    out.text += t.get<std::string>();
                            } else if (tp.is_string()) {
                                out.text = tp.get<std::string>();
                            }
                        }
                    }

                    // Error
                    out.ename = out_j.value("ename", "");
                    out.evalue = out_j.value("evalue", "");
                    if (out_j.contains("traceback") && out_j["traceback"].is_array()) {
                        for (auto& t : out_j["traceback"]) {
                            if (!out.traceback.empty()) out.traceback += "\n";
                            out.traceback += t.get<std::string>();
                        }
                    }

                    cell.outputs.push_back(std::move(out));
                }
            }

            nb.cells.push_back(std::move(cell));
        }
    }

    return nb;
}

// =============================================================================
// Save .ipynb
// =============================================================================
bool save_notebook(const std::string& file_path, const Notebook& nb) {
    json j;

    // Metadata
    j["metadata"]["kernelspec"] = {
        {"display_name", "Python 3"},
        {"language", nb.language},
        {"name", nb.kernel_name}
    };
    j["metadata"]["language_info"] = {
        {"name", nb.language},
        {"version", "3.10"},
        {"mimetype", "text/x-python"},
        {"file_extension", ".py"}
    };

    j["nbformat"] = nb.nbformat;
    j["nbformat_minor"] = nb.nbformat_minor;

    // Cells
    j["cells"] = json::array();
    for (auto& cell : nb.cells) {
        json cell_j;
        cell_j["id"] = cell.id;
        cell_j["cell_type"] = cell.cell_type;
        cell_j["source"] = split_source(cell.source);
        cell_j["metadata"] = json::object();

        if (cell.cell_type == "code") {
            cell_j["execution_count"] = (cell.execution_count >= 0) ?
                json(cell.execution_count) : json(nullptr);

            cell_j["outputs"] = json::array();
            for (auto& out : cell.outputs) {
                json out_j;
                out_j["output_type"] = out.output_type;

                if (out.output_type == "stream") {
                    out_j["name"] = "stdout";
                    out_j["text"] = split_source(out.text);
                } else if (out.output_type == "error") {
                    out_j["ename"] = out.ename;
                    out_j["evalue"] = out.evalue;
                    out_j["traceback"] = split_source(out.traceback);
                } else if (out.output_type == "execute_result" || out.output_type == "display_data") {
                    out_j["data"] = {{"text/plain", split_source(out.text)}};
                    out_j["metadata"] = json::object();
                    if (out.output_type == "execute_result") {
                        out_j["execution_count"] = cell.execution_count;
                    }
                }

                cell_j["outputs"].push_back(out_j);
            }
        }

        j["cells"].push_back(cell_j);
    }

    std::ofstream f(file_path);
    if (!f.is_open()) return false;

    f << j.dump(1);
    return f.good();
}

// =============================================================================
// Create new notebook
// =============================================================================
Notebook create_new_notebook() {
    Notebook nb;

    // Branded markdown cell
    NotebookCell brand;
    brand.id = generate_cell_id();
    brand.cell_type = "markdown";
    brand.source = "# Fincept Terminal - Notebook\n\n"
                   "**Fincept Terminal** | Professional Financial Intelligence Platform\n\n"
                   "---\n\n"
                   "> *CFA-Level Analytics | AI-Powered Automation | Global Market Intelligence*\n\n"
                   "Use this notebook to run Python-based financial analysis, "
                   "build quantitative models, and explore market data.\n";
    nb.cells.push_back(std::move(brand));

    // Empty code cell
    NotebookCell code;
    code.id = generate_cell_id();
    code.cell_type = "code";
    code.source = "# Enter Python code here\nimport sys\nprint(f\"Python {sys.version}\")\n";
    nb.cells.push_back(std::move(code));

    return nb;
}

// =============================================================================
// Export to .py — concatenate all code cells
// =============================================================================
bool export_to_python(const std::string& file_path, const Notebook& nb) {
    std::ofstream f(file_path);
    if (!f.is_open()) return false;

    f << "# -*- coding: utf-8 -*-\n";
    f << "# Exported from Fincept Terminal Notebook\n";
    f << "# " << std::string(70, '=') << "\n\n";

    int cell_num = 0;
    for (auto& cell : nb.cells) {
        if (cell.cell_type == "code" && !cell.source.empty()) {
            cell_num++;
            f << "# --- Cell " << cell_num << " ---\n";
            f << cell.source;
            if (!cell.source.empty() && cell.source.back() != '\n') f << '\n';
            f << '\n';
        } else if (cell.cell_type == "markdown" && !cell.source.empty()) {
            // Write markdown as comments
            f << "# --- Markdown ---\n";
            std::istringstream iss(cell.source);
            std::string line;
            while (std::getline(iss, line)) {
                f << "# " << line << '\n';
            }
            f << '\n';
        }
    }

    return f.good();
}

} // namespace fincept::code_editor::io
