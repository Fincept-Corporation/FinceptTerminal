#include "services/workflow/nodes/FileNodes.h"

#include "services/workflow/NodeRegistry.h"

namespace fincept::workflow {

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
                {"format", "Format", "select", "json", {"json", "csv", "xlsx", "xml", "pdf"}, ""},
                {"path", "Output Path", "string", "", {}, "", true},
            },
        .execute = nullptr,
    });

    registry.register_type({
        .type_id = "file.compress",
        .display_name = "Compress",
        .category = "Files",
        .description = "Compress or decompress files",
        .icon_text = "F",
        .accent_color = "#525252",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"operation", "Operation", "select", "compress", {"compress", "decompress"}, ""},
                {"format", "Format", "select", "zip", {"zip", "gzip", "tar.gz"}, ""},
                {"path", "File Path", "string", "", {}, "", true},
            },
        .execute = nullptr,
    });
}

} // namespace fincept::workflow
