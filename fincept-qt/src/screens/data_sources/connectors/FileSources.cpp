// File Source connectors (10): CSV, Excel, JSON, Parquet, XML, Avro, ORC, Feather, FTP, SFTP
#include "screens/data_sources/ConnectorRegistry.h"

namespace fincept::screens::datasources {

static QVector<ConnectorConfig> file_configs() {
    return {
        {"csv", "CSV File", "csv", Category::File, "C", "#217346",
         "Import data from CSV files", false, false,
         {{"filepath","File Path",FieldType::Text,"Select CSV file",true,"",{}},
          {"delimiter","Delimiter",FieldType::Select,"",false,",",
           {{"Comma (,)",","},{"Semicolon (;)",";"},{"Tab","\\t"},{"Pipe (|)","|"}}},
          {"hasHeader","Has Header Row",FieldType::Checkbox,"",false,"true",{}},
          {"encoding","Encoding",FieldType::Select,"",false,"utf-8",
           {{"UTF-8","utf-8"},{"ASCII","ascii"},{"Latin-1","latin1"}}}}},

        {"excel", "Excel (XLSX)", "excel", Category::File, "X", "#217346",
         "Import data from Excel files", false, false,
         {{"filepath","File Path",FieldType::Text,"Select Excel file",true,"",{}},
          {"sheet","Sheet Name or Index",FieldType::Text,"Sheet1",false,"",{}},
          {"hasHeader","Has Header Row",FieldType::Checkbox,"",false,"true",{}}}},

        {"json", "JSON File", "json", Category::File, "J", "#F7DF1E",
         "Import data from JSON files", false, false,
         {{"filepath","File Path",FieldType::Text,"Select JSON file",true,"",{}},
          {"jsonPath","JSON Path",FieldType::Text,"$.data",false,"",{}}}},

        {"parquet", "Parquet File", "parquet", Category::File, "P", "#FF6B00",
         "Import data from Apache Parquet files", false, false,
         {{"filepath","File Path",FieldType::Text,"Select Parquet file",true,"",{}}}},

        {"xml", "XML File", "xml", Category::File, "X", "#FF6600",
         "Import data from XML files", false, false,
         {{"filepath","File Path",FieldType::Text,"Select XML file",true,"",{}},
          {"xpath","XPath Expression",FieldType::Text,"//record",false,"",{}}}},

        {"avro", "Avro File", "avro", Category::File, "A", "#0066CC",
         "Import data from Apache Avro files", false, false,
         {{"filepath","File Path",FieldType::Text,"Select Avro file",true,"",{}}}},

        {"orc", "ORC File", "orc", Category::File, "O", "#5B9BD5",
         "Import data from Apache ORC files", false, false,
         {{"filepath","File Path",FieldType::Text,"Select ORC file",true,"",{}}}},

        {"feather", "Feather File", "feather", Category::File, "F", "#FF4081",
         "Import data from Apache Arrow Feather files", false, false,
         {{"filepath","File Path",FieldType::Text,"Select Feather file",true,"",{}}}},

        {"ftp", "FTP", "ftp", Category::File, "F", "#0066CC",
         "File Transfer Protocol", true, true,
         {{"host","Host",FieldType::Text,"ftp.example.com",true,"",{}},
          {"port","Port",FieldType::Number,"21",true,"21",{}},
          {"username","Username",FieldType::Text,"user",true,"",{}},
          {"password","Password",FieldType::Password,"",true,"",{}}}},

        {"sftp", "SFTP", "sftp", Category::File, "S", "#00CC66",
         "Secure File Transfer Protocol", true, true,
         {{"host","Host",FieldType::Text,"sftp.example.com",true,"",{}},
          {"port","Port",FieldType::Number,"22",true,"22",{}},
          {"username","Username",FieldType::Text,"user",true,"",{}},
          {"password","Password",FieldType::Password,"",false,"",{}},
          {"privateKey","Private Key Path",FieldType::Text,"/path/to/key",false,"",{}}}},
    };
}

static bool registered = [] {
    for (auto& c : file_configs()) ConnectorRegistry::instance().add(std::move(c));
    return true;
}();

} // namespace fincept::screens::datasources
