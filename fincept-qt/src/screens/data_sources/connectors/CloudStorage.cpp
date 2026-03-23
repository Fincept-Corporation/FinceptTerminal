// Cloud Storage connectors (10): AWS S3, GCS, Azure Blob, DigitalOcean Spaces, MinIO,
// Backblaze B2, Wasabi, Cloudflare R2, Oracle Cloud, IBM Cloud
#include "screens/data_sources/ConnectorRegistry.h"

namespace fincept::screens::datasources {

static QVector<ConnectorConfig> cloud_configs() {
    return {
        {"aws-s3", "AWS S3", "aws-s3", Category::Cloud, "A", "#FF9900",
         "Amazon S3 object storage", true, true,
         {{"region","Region",FieldType::Text,"us-east-1",true,"",{}},
          {"bucket","Bucket Name",FieldType::Text,"my-bucket",true,"",{}},
          {"accessKeyId","Access Key ID",FieldType::Text,"AKIAIOSFODNN7EXAMPLE",true,"",{}},
          {"secretAccessKey","Secret Access Key",FieldType::Password,"",true,"",{}},
          {"prefix","Prefix",FieldType::Text,"data/",false,"",{}}}},

        {"google-cloud-storage", "Google Cloud Storage", "google-cloud-storage", Category::Cloud, "G", "#4285F4",
         "Google Cloud object storage", true, true,
         {{"projectId","Project ID",FieldType::Text,"my-project",true,"",{}},
          {"bucket","Bucket Name",FieldType::Text,"my-bucket",true,"",{}},
          {"credentials","Service Account JSON",FieldType::Textarea,"Paste JSON credentials",true,"",{}}}},

        {"azure-blob", "Azure Blob Storage", "azure-blob", Category::Cloud, "A", "#0078D4",
         "Microsoft Azure blob storage", true, true,
         {{"accountName","Account Name",FieldType::Text,"mystorageaccount",true,"",{}},
          {"accountKey","Account Key",FieldType::Password,"",true,"",{}},
          {"container","Container Name",FieldType::Text,"my-container",true,"",{}}}},

        {"digitalocean-spaces", "DigitalOcean Spaces", "digitalocean-spaces", Category::Cloud, "D", "#0080FF",
         "DigitalOcean object storage", true, true,
         {{"region","Region",FieldType::Text,"nyc3",true,"",{}},
          {"space","Space Name",FieldType::Text,"my-space",true,"",{}},
          {"accessKey","Access Key",FieldType::Text,"your-access-key",true,"",{}},
          {"secretKey","Secret Key",FieldType::Password,"",true,"",{}}}},

        {"minio", "MinIO", "minio", Category::Cloud, "M", "#C72E49",
         "Self-hosted S3-compatible object storage", true, true,
         {{"endpoint","Endpoint",FieldType::Url,"http://localhost:9000",true,"",{}},
          {"bucket","Bucket Name",FieldType::Text,"my-bucket",true,"",{}},
          {"accessKey","Access Key",FieldType::Text,"minioadmin",true,"",{}},
          {"secretKey","Secret Key",FieldType::Password,"",true,"",{}}}},

        {"backblaze-b2", "Backblaze B2", "backblaze-b2", Category::Cloud, "B", "#FF0000",
         "Backblaze B2 cloud storage", true, true,
         {{"keyId","Application Key ID",FieldType::Text,"your-key-id",true,"",{}},
          {"applicationKey","Application Key",FieldType::Password,"",true,"",{}},
          {"bucketId","Bucket ID",FieldType::Text,"bucket-id",true,"",{}}}},

        {"wasabi", "Wasabi", "wasabi", Category::Cloud, "W", "#00B388",
         "Wasabi hot cloud storage", true, true,
         {{"region","Region",FieldType::Text,"us-east-1",true,"",{}},
          {"bucket","Bucket Name",FieldType::Text,"my-bucket",true,"",{}},
          {"accessKey","Access Key",FieldType::Text,"your-access-key",true,"",{}},
          {"secretKey","Secret Key",FieldType::Password,"",true,"",{}}}},

        {"cloudflare-r2", "Cloudflare R2", "cloudflare-r2", Category::Cloud, "C", "#F38020",
         "Cloudflare R2 storage", true, true,
         {{"accountId","Account ID",FieldType::Text,"your-account-id",true,"",{}},
          {"bucket","Bucket Name",FieldType::Text,"my-bucket",true,"",{}},
          {"accessKeyId","Access Key ID",FieldType::Text,"access-key",true,"",{}},
          {"secretAccessKey","Secret Access Key",FieldType::Password,"",true,"",{}}}},

        {"oracle-cloud-storage", "Oracle Cloud Storage", "oracle-cloud-storage", Category::Cloud, "O", "#F80000",
         "Oracle Cloud Infrastructure object storage", true, true,
         {{"region","Region",FieldType::Text,"us-phoenix-1",true,"",{}},
          {"namespace","Namespace",FieldType::Text,"my-namespace",true,"",{}},
          {"bucket","Bucket Name",FieldType::Text,"my-bucket",true,"",{}},
          {"accessKey","Access Key",FieldType::Text,"access-key",true,"",{}},
          {"secretKey","Secret Key",FieldType::Password,"",true,"",{}}}},

        {"ibm-cloud-storage", "IBM Cloud Object Storage", "ibm-cloud-storage", Category::Cloud, "I", "#0F62FE",
         "IBM Cloud object storage", true, true,
         {{"endpoint","Endpoint",FieldType::Url,"https://s3.us.cloud-object-storage.appdomain.cloud",true,"",{}},
          {"bucket","Bucket Name",FieldType::Text,"my-bucket",true,"",{}},
          {"apiKey","API Key",FieldType::Password,"",true,"",{}},
          {"serviceInstanceId","Service Instance ID",FieldType::Text,"crn:v1:...",true,"",{}}}},
    };
}

static bool registered = [] {
    for (auto& c : cloud_configs()) ConnectorRegistry::instance().add(std::move(c));
    return true;
}();

} // namespace fincept::screens::datasources
