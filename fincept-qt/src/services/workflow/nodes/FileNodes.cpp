#include "services/workflow/nodes/FileNodes.h"

#include "services/workflow/NodeRegistry.h"

namespace fincept::workflow {

// accent_color: node-category identity colours, written for the dark theme;
// the node editor draws them through ui::colors::legible().
void register_file_nodes(NodeRegistry& registry) {
    registry.register_type({
        .type_id = "file.operations",
        .display_name = "File Operations",
        .category = "Files",
        .description = "Read, write, or append to files",
        .icon_text = "F",
        .accent_color = "#525252",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"operation", "Operation", "select", "read", {"read", "write", "append", "delete", "exists"}, "", true},
                {"path", "File Path", "string", "", {}, "Path to file", true},
                {"encoding", "Encoding", "select", "utf-8", {"utf-8", "ascii", "latin-1"}, ""},
            },
        .execute = nullptr,
    });

    registry.register_type({
        .type_id = "file.binary",
        .display_name = "Binary File",
        .category = "Files",
        .description = "Read/write binary files",
        .icon_text = "F",
        .accent_color = "#525252",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"operation", "Operation", "select", "read", {"read", "write"}, ""},
                {"path", "File Path", "string", "", {}, "", true},
            },
        .execute = nullptr,
    });

    registry.register_type({
        .type_id = "file.convert",
        .display_name = "Convert to File",
        .category = "Files",
        .description = "Convert data to a downloadable file format",
        .icon_text = "F",
        .accent_color = "#525252",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "File", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                // Only JSON and CSV are written (the bridge refuses anything else); xlsx / xml /
                // pdf were offered here and always failed at run time.
                {"format", "Format", "select", "json", {"json", "csv"}, ""},
                {"path", "Output Path", "string", "", {}, "", true},
            },
        .execute = nullptr,
    });

    registry.register_type({
        .type_id = "file.compress",
        .display_name = "Compress (.qz)",
        .category = "Files",
        // The file written is Qt's qCompress container (<path>.qz), readable by this node's
        // Decompress only. The Format select (zip / gzip / tar.gz) was never read and is gone.
        .description = "Compress a file to <path>.qz (Qt format) or decompress a .qz file",
        .icon_text = "F",
        .accent_color = "#525252",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"operation", "Operation", "select", "compress", {"compress", "decompress"}, ""},
                {"path", "File Path", "string", "", {}, "", true},
            },
        .execute = nullptr,
    });
}

} // namespace fincept::workflow
