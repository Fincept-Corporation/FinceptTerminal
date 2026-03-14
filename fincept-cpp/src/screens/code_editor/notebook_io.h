#pragma once
// Notebook I/O — load/save .ipynb (Jupyter JSON format), export to .py

#include "notebook_types.h"
#include <string>

namespace fincept::code_editor::io {

// Parse .ipynb JSON file into Notebook struct
Notebook load_notebook(const std::string& file_path);

// Serialize Notebook to .ipynb JSON and write to file
bool save_notebook(const std::string& file_path, const Notebook& nb);

// Create a new empty notebook with branded markdown cell + one code cell
Notebook create_new_notebook();

// Export all code cells to a single .py file
bool export_to_python(const std::string& file_path, const Notebook& nb);

} // namespace fincept::code_editor::io
