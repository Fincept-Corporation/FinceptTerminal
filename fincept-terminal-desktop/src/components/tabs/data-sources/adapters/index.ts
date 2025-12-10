// Data Source Adapter Registry
// This file exports all adapters and provides a factory function to create the appropriate adapter

import { BaseAdapter } from './BaseAdapter';
import { PostgreSQLAdapter } from './PostgreSQLAdapter';
import { MySQLAdapter } from './MySQLAdapter';
import { MariaDBAdapter } from './MariaDBAdapter';
import { MongoDBAdapter } from './MongoDBAdapter';
import { RedisAdapter } from './RedisAdapter';
import { SQLiteAdapter } from './SQLiteAdapter';
import { CSVAdapter } from './CSVAdapter';
import { YahooFinanceAdapter } from './YahooFinanceAdapter';
import { InfluxDBAdapter } from './InfluxDBAdapter';
import { QuestDBAdapter } from './QuestDBAdapter';
import { TimescaleDBAdapter } from './TimescaleDBAdapter';
import { AlphaVantageAdapter } from './AlphaVantageAdapter';
import { PolygonAdapter } from './PolygonAdapter';
import { ElasticsearchAdapter } from './ElasticsearchAdapter';
import { CassandraAdapter } from './CassandraAdapter';
import { SnowflakeAdapter } from './SnowflakeAdapter';
import { FinnhubAdapter } from './FinnhubAdapter';
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
import { CoinbaseAdapter } from './CoinbaseAdapter';
import { RabbitMQAdapter } from './RabbitMQAdapter';
import { WebSocketAdapter } from './WebSocketAdapter';
import { OracleAdapter } from './OracleAdapter';
import { SQLServerAdapter } from './SQLServerAdapter';
import { DatabricksAdapter } from './DatabricksAdapter';
import { GCPStorageAdapter } from './GCPStorageAdapter';
import { AzureBlobAdapter } from './AzureBlobAdapter';
import { IEXCloudAdapter } from './IEXCloudAdapter';
import { CoinMarketCapAdapter } from './CoinMarketCapAdapter';
import { TwelveDataAdapter } from './TwelveDataAdapter';
import { QuandlAdapter } from './QuandlAdapter';
import { EODHistoricalDataAdapter } from './EODHistoricalDataAdapter';
import { TiingoAdapter } from './TiingoAdapter';
import { IntrinioAdapter } from './IntrinioAdapter';
import { MarketstackAdapter } from './MarketstackAdapter';
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
import { BloombergAdapter } from './BloombergAdapter';
import { RefinitivAdapter } from './RefinitivAdapter';
import { FinageAdapter } from './FinageAdapter';
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

// Map of data source types to their adapter classes
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

  // Analytical/Columnar Databases
  clickhouse: ClickHouseAdapter,

  // Data Warehouses
  snowflake: SnowflakeAdapter,
  bigquery: BigQueryAdapter,
  redshift: RedshiftAdapter,
  databricks: DatabricksAdapter,
  synapse: AzureSynapseAdapter,

  // Time Series Databases
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

  // Market Data APIs
  'yahoo-finance': YahooFinanceAdapter,
  'alpha-vantage': AlphaVantageAdapter,
  'polygon-io': PolygonAdapter,
  finnhub: FinnhubAdapter,
  coingecko: CoinGeckoAdapter,
  binance: BinanceAdapter,
  coinbase: CoinbaseAdapter,
  'iex-cloud': IEXCloudAdapter,
  coinmarketcap: CoinMarketCapAdapter,
  'twelve-data': TwelveDataAdapter,
  quandl: QuandlAdapter,
  'eod-historical': EODHistoricalDataAdapter,
  tiingo: TiingoAdapter,
  intrinio: IntrinioAdapter,
  marketstack: MarketstackAdapter,
  bloomberg: BloombergAdapter,
  reuters: RefinitivAdapter,
  finage: FinageAdapter,
  tradier: TradierAdapter,

  // Search & Analytics
  elasticsearch: ElasticsearchAdapter,
  opensearch: OpenSearchAdapter,
  solr: SolrAdapter,
  algolia: AlgoliaAdapter,
  meilisearch: MeiliSearchAdapter,
};

/**
 * Create an adapter instance for a given data source connection
 * @param connection The data source connection configuration
 * @returns An instance of the appropriate adapter, or null if no adapter is available
 */
export function createAdapter(connection: DataSourceConnection): BaseAdapter | null {
  const AdapterClass = ADAPTER_MAP[connection.type];

  if (!AdapterClass) {
    console.warn(`No adapter found for data source type: ${connection.type}`);
    return null;
  }

  return new AdapterClass(connection);
}

/**
 * Check if an adapter is available for a given data source type
 * @param type The data source type
 * @returns True if an adapter is available, false otherwise
 */
export function hasAdapter(type: string): boolean {
  return type in ADAPTER_MAP;
}

/**
 * Get list of all supported data source types that have adapters
 * @returns Array of supported data source type strings
 */
export function getSupportedTypes(): string[] {
  return Object.keys(ADAPTER_MAP);
}

/**
 * Test a connection using the appropriate adapter
 * @param connection The data source connection to test
 * @returns Test result with success status and message
 */
export async function testConnection(connection: DataSourceConnection) {
  const adapter = createAdapter(connection);

  if (!adapter) {
    return {
      success: false,
      message: `No adapter available for ${connection.type}. This data source type is not yet implemented.`,
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

// Export all adapters for direct use if needed
export {
  BaseAdapter,
  PostgreSQLAdapter,
  MySQLAdapter,
  MariaDBAdapter,
  MongoDBAdapter,
  RedisAdapter,
  SQLiteAdapter,
  CSVAdapter,
  YahooFinanceAdapter,
  InfluxDBAdapter,
  QuestDBAdapter,
  TimescaleDBAdapter,
  AlphaVantageAdapter,
  PolygonAdapter,
  ElasticsearchAdapter,
  CassandraAdapter,
  SnowflakeAdapter,
  FinnhubAdapter,
  BigQueryAdapter,
  KafkaAdapter,
  ClickHouseAdapter,
  RedshiftAdapter,
  PrometheusAdapter,
  Neo4jAdapter,
  S3Adapter,
  GraphQLAdapter,
  RESTAdapter,
  ExcelAdapter,
  JSONAdapter,
  ParquetAdapter,
  CoinGeckoAdapter,
  BinanceAdapter,
  CoinbaseAdapter,
  RabbitMQAdapter,
  WebSocketAdapter,
  OracleAdapter,
  SQLServerAdapter,
  DatabricksAdapter,
  GCPStorageAdapter,
  AzureBlobAdapter,
  IEXCloudAdapter,
  CoinMarketCapAdapter,
  TwelveDataAdapter,
  QuandlAdapter,
  EODHistoricalDataAdapter,
  TiingoAdapter,
  IntrinioAdapter,
  MarketstackAdapter,
  OpenSearchAdapter,
  SolrAdapter,
  VictoriaMetricsAdapter,
  OpenTSDBAdapter,
  KDBAdapter,
  XMLAdapter,
  AvroAdapter,
  ORCAdapter,
  FeatherAdapter,
  GRPCAdapter,
  SOAPAdapter,
  ODataAdapter,
  MQTTAdapter,
  NATSAdapter,
  DigitalOceanSpacesAdapter,
  MinIOAdapter,
  BackblazeB2Adapter,
  WasabiAdapter,
  CloudflareR2Adapter,
  OracleCloudStorageAdapter,
  IBMCloudStorageAdapter,
  MSGraphAdapter,
  BloombergAdapter,
  RefinitivAdapter,
  FinageAdapter,
  TradierAdapter,
  AlgoliaAdapter,
  MeiliSearchAdapter,
  AzureSynapseAdapter,
  CockroachDBAdapter,
  VerticaAdapter,
  CouchDBAdapter,
  DynamoDBAdapter,
  ArangoDBAdapter,
  MemcachedAdapter,
  RethinkDBAdapter,
  FirestoreAdapter,
  HBaseAdapter,
  ScyllaDBAdapter,
  OrientDBAdapter,
  CosmosDBAdapter,
  EtcdAdapter,
  FTPAdapter,
  SFTPAdapter,
};

// Export adapter types
export type AdapterType = InstanceType<typeof BaseAdapter>;
