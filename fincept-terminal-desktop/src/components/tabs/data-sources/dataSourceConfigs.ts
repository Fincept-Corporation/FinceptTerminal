import { DataSourceConfig } from './types';
import { DATABASE_SOURCE_CONFIGS } from './dataSourceConfigsDatabases';
import { MARKET_SOURCE_CONFIGS } from './dataSourceConfigsMarket';

export const DATA_SOURCE_CONFIGS: DataSourceConfig[] = [
  ...DATABASE_SOURCE_CONFIGS,
  ...MARKET_SOURCE_CONFIGS,
];
