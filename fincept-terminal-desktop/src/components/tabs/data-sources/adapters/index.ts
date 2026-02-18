// Data Source Adapter Registry

import { BaseAdapter } from './BaseAdapter';
import { PostgreSQLAdapter } from './PostgreSQLAdapter';
import { MySQLAdapter } from './MySQLAdapter';
import { MariaDBAdapter } from './MariaDBAdapter';
import { MongoDBAdapter } from './MongoDBAdapter';
import { RedisAdapter } from './RedisAdapter';
import { SQLiteAdapter } from './SQLiteAdapter';
import { CSVAdapter } from './CSVAdapter';
import { InfluxDBAdapter } from './InfluxDBAdapter';
import { QuestDBAdapter } from './QuestDBAdapter';
import { TimescaleDBAdapter } from './TimescaleDBAdapter';
import { AlphaVantageAdapter } from './AlphaVantageAdapter';
import { ElasticsearchAdapter } from './ElasticsearchAdapter';
import { CassandraAdapter } from './CassandraAdapter';
import { SnowflakeAdapter } from './SnowflakeAdapter';
import { BigQueryAdapter } from './BigQueryAdapter';
import { KafkaAdapter } from './KafkaAdapter';
import { ClickHouseAdapter } from './ClickHouseAdapter';
import { RedshiftAdapter } from './RedshiftAdapter';
import { PrometheusAdapter } from './PrometheusAdapter';
import { Neo4jAdapter } from './Neo4jAdapter';
import { S3Adapter } from './S3Adapter';
import { GraphQLAdapter } from './GraphQLAdapter';
import { RESTAdapter } from './RESTAdapter';
import { ExcelAdapter } from './ExcelAdapter';
import { JSONAdapter } from './JSONAdapter';
import { ParquetAdapter } from './ParquetAdapter';
import { CoinGeckoAdapter } from './CoinGeckoAdapter';
import { BinanceAdapter } from './BinanceAdapter';
import { RabbitMQAdapter } from './RabbitMQAdapter';
import { WebSocketAdapter } from './WebSocketAdapter';
import { OracleAdapter } from './OracleAdapter';
import { SQLServerAdapter } from './SQLServerAdapter';
import { DatabricksAdapter } from './DatabricksAdapter';
import { GCPStorageAdapter } from './GCPStorageAdapter';
import { AzureBlobAdapter } from './AzureBlobAdapter';
import { OpenSearchAdapter } from './OpenSearchAdapter';
import { SolrAdapter } from './SolrAdapter';
import { VictoriaMetricsAdapter } from './VictoriaMetricsAdapter';
import { OpenTSDBAdapter } from './OpenTSDBAdapter';
import { KDBAdapter } from './KDBAdapter';
import { XMLAdapter } from './XMLAdapter';
import { AvroAdapter } from './AvroAdapter';
import { ORCAdapter } from './ORCAdapter';
import { FeatherAdapter } from './FeatherAdapter';
import { GRPCAdapter } from './GRPCAdapter';
import { SOAPAdapter } from './SOAPAdapter';
import { ODataAdapter } from './ODataAdapter';
import { MQTTAdapter } from './MQTTAdapter';
import { NATSAdapter } from './NATSAdapter';
import { DigitalOceanSpacesAdapter } from './DigitalOceanSpacesAdapter';
import { MinIOAdapter } from './MinIOAdapter';
import { BackblazeB2Adapter } from './BackblazeB2Adapter';
import { WasabiAdapter } from './WasabiAdapter';
import { CloudflareR2Adapter } from './CloudflareR2Adapter';
import { OracleCloudStorageAdapter } from './OracleCloudStorageAdapter';
import { IBMCloudStorageAdapter } from './IBMCloudStorageAdapter';
import { MSGraphAdapter } from './MSGraphAdapter';
import { FinceptDataAdapter } from './FinceptDataAdapter';
import { TradierAdapter } from './TradierAdapter';
import { AlgoliaAdapter } from './AlgoliaAdapter';
import { MeiliSearchAdapter } from './MeiliSearchAdapter';
import { AzureSynapseAdapter } from './AzureSynapseAdapter';
import { CockroachDBAdapter } from './CockroachDBAdapter';
import { VerticaAdapter } from './VerticaAdapter';
import { CouchDBAdapter } from './CouchDBAdapter';
import { DynamoDBAdapter } from './DynamoDBAdapter';
import { ArangoDBAdapter } from './ArangoDBAdapter';
import { MemcachedAdapter } from './MemcachedAdapter';
import { RethinkDBAdapter } from './RethinkDBAdapter';
import { FirestoreAdapter } from './FirestoreAdapter';
import { HBaseAdapter } from './HBaseAdapter';
import { ScyllaDBAdapter } from './ScyllaDBAdapter';
import { OrientDBAdapter } from './OrientDBAdapter';
import { CosmosDBAdapter } from './CosmosDBAdapter';
import { EtcdAdapter } from './EtcdAdapter';
import { FTPAdapter } from './FTPAdapter';
import { SFTPAdapter } from './SFTPAdapter';
import { DataSourceConnection } from '../types';

const ADAPTER_MAP: Record<string, new (connection: any) => BaseAdapter> = {
  // Relational Databases
  postgresql: PostgreSQLAdapter,
  mysql: MySQLAdapter,
  mariadb: MariaDBAdapter,
  sqlite: SQLiteAdapter,
  oracle: OracleAdapter,
  sqlserver: SQLServerAdapter,
  mssql: SQLServerAdapter,
  cockroachdb: CockroachDBAdapter,
  vertica: VerticaAdapter,

  // NoSQL Databases
  mongodb: MongoDBAdapter,
  redis: RedisAdapter,
  cassandra: CassandraAdapter,
  neo4j: Neo4jAdapter,
  couchdb: CouchDBAdapter,
  dynamodb: DynamoDBAdapter,
  arangodb: ArangoDBAdapter,
  memcached: MemcachedAdapter,
  rethinkdb: RethinkDBAdapter,
  firestore: FirestoreAdapter,
  hbase: HBaseAdapter,
  scylladb: ScyllaDBAdapter,
  orientdb: OrientDBAdapter,
  cosmosdb: CosmosDBAdapter,
  etcd: EtcdAdapter,

  // Analytical/Columnar
  clickhouse: ClickHouseAdapter,

  // Data Warehouses
  snowflake: SnowflakeAdapter,
  bigquery: BigQueryAdapter,
  redshift: RedshiftAdapter,
  databricks: DatabricksAdapter,
  synapse: AzureSynapseAdapter,

  // Time Series
  influxdb: InfluxDBAdapter,
  questdb: QuestDBAdapter,
  timescaledb: TimescaleDBAdapter,
  prometheus: PrometheusAdapter,
  victoriametrics: VictoriaMetricsAdapter,
  opentsdb: OpenTSDBAdapter,
  kdb: KDBAdapter,

  // Streaming
  kafka: KafkaAdapter,
  rabbitmq: RabbitMQAdapter,
  mqtt: MQTTAdapter,
  nats: NATSAdapter,

  // Cloud Storage
  's3': S3Adapter,
  'aws-s3': S3Adapter,
  'google-cloud-storage': GCPStorageAdapter,
  'azure-blob': AzureBlobAdapter,
  'digitalocean-spaces': DigitalOceanSpacesAdapter,
  minio: MinIOAdapter,
  'backblaze-b2': BackblazeB2Adapter,
  wasabi: WasabiAdapter,
  'cloudflare-r2': CloudflareR2Adapter,
  'oracle-cloud-storage': OracleCloudStorageAdapter,
  'ibm-cloud-storage': IBMCloudStorageAdapter,

  // File Sources
  csv: CSVAdapter,
  excel: ExcelAdapter,
  json: JSONAdapter,
  parquet: ParquetAdapter,
  xml: XMLAdapter,
  avro: AvroAdapter,
  orc: ORCAdapter,
  feather: FeatherAdapter,

  // API Sources
  graphql: GraphQLAdapter,
  'rest-api': RESTAdapter,
  websocket: WebSocketAdapter,
  grpc: GRPCAdapter,
  soap: SOAPAdapter,
  odata: ODataAdapter,
  'microsoft-graph': MSGraphAdapter,
  ftp: FTPAdapter,
  sftp: SFTPAdapter,

  // Market Data — all backed by Rust/Python commands
  'alpha-vantage': AlphaVantageAdapter,
  coingecko: CoinGeckoAdapter,
  binance: BinanceAdapter,
  tradier: TradierAdapter,
  fincept: FinceptDataAdapter,

  // Search & Analytics
  elasticsearch: ElasticsearchAdapter,
  opensearch: OpenSearchAdapter,
  solr: SolrAdapter,
  algolia: AlgoliaAdapter,
  meilisearch: MeiliSearchAdapter,
};

export function createAdapter(connection: DataSourceConnection): BaseAdapter | null {
  const AdapterClass = ADAPTER_MAP[connection.type];
  if (!AdapterClass) {
    console.warn(`No adapter found for data source type: ${connection.type}`);
    return null;
  }
  return new AdapterClass(connection);
}

export function hasAdapter(type: string): boolean {
  return type in ADAPTER_MAP;
}

export function getSupportedTypes(): string[] {
  return Object.keys(ADAPTER_MAP);
}

export async function testConnection(connection: DataSourceConnection) {
  const adapter = createAdapter(connection);
  if (!adapter) {
    return {
      success: false,
      message: `No adapter available for ${connection.type}.`,
    };
  }
  try {
    return await adapter.testConnection();
  } catch (error) {
    return {
      success: false,
      message: `Adapter error: ${error instanceof Error ? error.message : String(error)}`,
    };
  }
}

export {
  BaseAdapter,
  PostgreSQLAdapter, MySQLAdapter, MariaDBAdapter, MongoDBAdapter, RedisAdapter, SQLiteAdapter,
  CSVAdapter, InfluxDBAdapter, QuestDBAdapter, TimescaleDBAdapter,
  AlphaVantageAdapter, ElasticsearchAdapter, CassandraAdapter, SnowflakeAdapter,
  BigQueryAdapter, KafkaAdapter, ClickHouseAdapter, RedshiftAdapter, PrometheusAdapter,
  Neo4jAdapter, S3Adapter, GraphQLAdapter, RESTAdapter, ExcelAdapter, JSONAdapter, ParquetAdapter,
  CoinGeckoAdapter, BinanceAdapter, RabbitMQAdapter, WebSocketAdapter,
  OracleAdapter, SQLServerAdapter, DatabricksAdapter, GCPStorageAdapter, AzureBlobAdapter,
  OpenSearchAdapter, SolrAdapter, VictoriaMetricsAdapter, OpenTSDBAdapter, KDBAdapter,
  XMLAdapter, AvroAdapter, ORCAdapter, FeatherAdapter, GRPCAdapter, SOAPAdapter, ODataAdapter,
  MQTTAdapter, NATSAdapter, DigitalOceanSpacesAdapter, MinIOAdapter, BackblazeB2Adapter,
  WasabiAdapter, CloudflareR2Adapter, OracleCloudStorageAdapter, IBMCloudStorageAdapter,
  MSGraphAdapter, FinceptDataAdapter, TradierAdapter, AlgoliaAdapter, MeiliSearchAdapter,
  AzureSynapseAdapter, CockroachDBAdapter, VerticaAdapter, CouchDBAdapter, DynamoDBAdapter,
  ArangoDBAdapter, MemcachedAdapter, RethinkDBAdapter, FirestoreAdapter, HBaseAdapter,
  ScyllaDBAdapter, OrientDBAdapter, CosmosDBAdapter, EtcdAdapter, FTPAdapter, SFTPAdapter,
};

export type AdapterType = InstanceType<typeof BaseAdapter>;
