// Government Data Tab Types

// ===== Provider Types =====

export type GovProvider = 'us-treasury' | 'canada-gov' | 'us-congress' | 'openafrica' | 'spain' | 'finland-pxweb' | 'swiss' | 'france' | 'universal-ckan' | 'hk';

export interface GovProviderConfig {
  id: GovProvider;
  name: string;
  fullName: string;
  description: string;
  color: string;
  country: string;
  flag: string;
  endpoints: GovEndpointConfig[];
}

export interface GovEndpointConfig {
  id: string;
  name: string;
  description: string;
  command: string;
}

// ===== US Treasury Types =====

export type USTreasuryEndpoint = 'treasury_prices' | 'treasury_auctions' | 'summary';

export type SecurityTypeFilter = 'all' | 'bill' | 'note' | 'bond' | 'tips' | 'frn' | 'cmb';

export interface TreasuryPriceRecord {
  cusip: string;
  security_type: string;
  rate: number | null;
  maturity_date: string | null;
  call_date: string | null;
  bid: number | null;
  offer: number | null;
  eod_price: number | null;
  date: string;
}

export interface TreasuryPricesResponse {
  success: boolean;
  endpoint: string;
  date: string;
  total_records: number;
  filters: {
    cusip: string | null;
    security_type: string | null;
  };
  data: TreasuryPriceRecord[];
  error?: string;
}

export interface TreasuryAuctionRecord {
  cusip: string;
  issueDate: string | null;
  securityType: string;
  securityTerm: string;
  maturityDate: string | null;
  interestRate: number | null;
  auctionDate: string | null;
  auctionDateYear: string;
  highDiscountRate: number | null;
  highInvestmentRate: number | null;
  highPrice: number | null;
  bidToCoverRatio: number | null;
  competitiveAccepted: number | null;
  competitiveTendered: number | null;
  totalAccepted: number | null;
  totalTendered: number | null;
  offeringAmount: number | null;
  allocationPercentage: number | null;
  directBidderAccepted: number | null;
  directBidderTendered: number | null;
  indirectBidderAccepted: number | null;
  indirectBidderTendered: number | null;
  primaryDealerAccepted: number | null;
  primaryDealerTendered: number | null;
  pricePer100: string | null;
  reopening: string | null;
  type: string;
  term: string;
  [key: string]: any; // Allow extra fields from API
}

export interface TreasuryAuctionsResponse {
  success: boolean;
  endpoint: string;
  page_num: number;
  page_size: number;
  total_records: number;
  filters: {
    start_date: string | null;
    end_date: string | null;
    security_type: string | null;
  };
  data: TreasuryAuctionRecord[];
  error?: string;
}

export interface TreasurySummaryResponse {
  success: boolean;
  endpoint: string;
  date: string;
  total_securities: number;
  security_types: Record<string, number>;
  yield_summary: {
    min_rate: number;
    max_rate: number;
    avg_rate: number;
    total_with_rates: number;
  };
  price_summary: {
    min_price: number;
    max_price: number;
    avg_price: number;
    total_with_prices: number;
  };
  error?: string;
}

// ===== Canada Gov Types =====

export type CanadaGovView = 'publishers' | 'datasets' | 'resources' | 'search' | 'recent';

export interface CanadaPublisher {
  id: string;
  name: string;
  display_name: string;
  dataset_count?: number;
}

export interface CanadaDataset {
  id: string;
  name: string;
  title: string;
  notes: string;
  publisher_id?: string;
  organization?: string | null;
  metadata_created: string | null;
  metadata_modified: string | null;
  state?: string;
  num_resources: number;
  tags: string[];
}

export interface CanadaResource {
  id: string;
  name: string;
  description: string;
  format: string;
  url: string;
  size: number | null;
  mimetype: string | null;
  created: string | null;
  last_modified: string | null;
  resource_type: string | null;
  package_id: string;
  position: number | null;
}

export interface CanadaGovResponse<T> {
  data: T;
  metadata: Record<string, any>;
  error: string | null;
}

// ===== Congress.gov Types =====

export type CongressGovView = 'bills' | 'bill-detail' | 'summary';

export interface CongressBill {
  congress: number;
  bill_type: string;
  bill_number: number;
  bill_url: string;
  title: string;
  origin_chamber: string;
  origin_chamber_code: string;
  update_date: string;
  update_date_including_text: string;
  latest_action_date: string | null;
  latest_action: string | null;
}

export interface CongressBillsResponse {
  success: boolean;
  endpoint: string;
  congress: number;
  filters: {
    bill_type: string | null;
    start_date: string | null;
    end_date: string | null;
    limit: number;
    offset: number;
    sort_by: string;
    get_all: boolean;
  };
  total_bills: number;
  pagination: { count?: number; next?: string };
  bills: CongressBill[];
  error?: string;
}

export interface CongressBillDetail {
  success: boolean;
  endpoint: string;
  bill_url: string;
  bill_data: Record<string, any>;
  markdown_content: string;
  error?: string;
}

export interface CongressBillSummary {
  success: boolean;
  endpoint: string;
  congress: number;
  total_bills: number;
  bill_types: Record<string, { count: number; latest_bills?: CongressBill[]; error?: string }>;
  error?: string;
}

// ===== openAFRICA Types =====

export type OpenAfricaView = 'organizations' | 'datasets' | 'resources' | 'search' | 'recent';

export interface OpenAfricaOrganization {
  id: string;
  name: string;
  title: string;
  description: string;
  image_url: string;
  created: string;
  dataset_count: number;
  state: string;
  display_name: string;
}

export interface OpenAfricaDataset {
  id: string;
  name: string;
  title: string;
  notes: string;
  organization: Record<string, any>;
  author: string;
  license: string;
  created: string;
  modified: string;
  tags: string[];
  resource_count: number;
}

export interface OpenAfricaResource {
  id: string;
  name: string;
  description: string;
  url: string;
  format: string;
  size: string | number | null;
  created: string;
  last_modified: string;
  package_id: string;
  position: number;
}

export interface OpenAfricaResponse<T> {
  data: T;
  metadata: Record<string, any>;
  error: string | null;
}

// ===== Spain Open Data Types =====

export type SpainDataView = 'catalogues' | 'datasets' | 'resources' | 'search';

export interface SpainCatalogue {
  uri: string;
  extracted_id: string;
}

export interface SpainDataset {
  uri?: string;
  extracted_id?: string;
  title: string | Array<{ _lang: string; _value: string }>;
  description?: string | Array<{ _lang: string; _value: string }>;
  title_translation?: { original: string; translated: string };
  description_translation?: { original: string; translated: string };
  distribution?: SpainResource[];
  publisher?: any;
  modified?: string;
  issued?: string;
  theme?: string[];
  keyword?: string[];
  [key: string]: any;
}

export interface SpainResource {
  accessURL?: string;
  format?: string | { label: string };
  title?: string | Array<{ _lang: string; _value: string }>;
  title_translation?: { original: string; translated: string };
  description?: string;
  byteSize?: number;
  resource_index?: number;
  download_available?: boolean;
  [key: string]: any;
}

export interface SpainDataResponse<T> {
  data: T;
  metadata: Record<string, any>;
  error: string | null;
}

// ===== PxWeb (Statistics Finland) Types =====

export type PxWebView = 'browse' | 'metadata' | 'data';

/** A node in the PxWeb database tree — either a folder (type "l") or table (type "t") */
export interface PxWebNode {
  dbid: string;
  id: string;
  type: 'l' | 't';  // l = level/folder, t = table
  text: string;
  description?: string;
  updated?: string;
}

/** Variable/dimension returned by table metadata */
export interface PxWebVariable {
  code: string;
  text: string;
  values: string[];
  valueTexts: string[];
  elimination?: boolean;
  time?: boolean;
}

/** json-stat2 data response */
export interface PxWebStatData {
  class?: string;
  label?: string;
  source?: string;
  updated?: string;
  id?: string[];
  size?: number[];
  dimension?: Record<string, { label: string; category: { index: Record<string, number>; label: Record<string, string> } }>;
  value?: (number | null)[];
}

export interface PxWebResponse {
  success: boolean;
  total_available?: number;
  total_fetched?: number;
  data: any;
  cap_reached?: boolean;
  timestamp?: number;
  error?: string;
}

// ===== Swiss Government (opendata.swiss) Types =====

export type SwissGovView = 'publishers' | 'datasets' | 'resources' | 'search' | 'recent';

export interface SwissPublisher {
  id: string;
  name: string;
  original_name: string;
  display_name: string;
}

export interface SwissDataset {
  id: string;
  name: string;
  title: string;
  original_title: string;
  notes: string;
  original_notes: string;
  publisher_id?: string;
  organization?: string | null;
  metadata_created: string | null;
  metadata_modified: string | null;
  state?: string;
  num_resources: number;
  tags: string[];
  language_detected?: { title: string; notes: string };
}

export interface SwissResource {
  id: string;
  name: string;
  original_name: string;
  description: string;
  original_description: string;
  format: string;
  url: string;
  size: number | null;
  mimetype: string | null;
  created: string | null;
  last_modified: string | null;
  resource_type: string | null;
  package_id: string;
  position: number | null;
  language_detected?: { name: string; description: string };
}

export interface SwissGovResponse<T> {
  data: T;
  metadata: Record<string, any>;
  error: string | null;
}

// ===== French Government (data.gouv.fr) Types =====

export type FrenchGovView = 'geo' | 'datasets' | 'tabular' | 'company';

export interface FrenchMunicipality {
  code: string;
  name: string;
  original_name: string;
  postal_codes: string[];
  department_code: string | null;
  region_code: string | null;
  population: number | null;
  area: number | null;
  center: any | null;
  search_score: number;
}

export interface FrenchDepartment {
  code: string;
  name: string;
  original_name: string;
  region_code: string | null;
  search_score: number;
}

export interface FrenchRegion {
  code: string;
  name: string;
  original_name: string;
  search_score: number;
}

export interface FrenchDataset {
  id: string;
  title: string;
  original_title: string;
  description: string;
  original_description: string;
  url: string | null;
  owner: string | null;
  organization: string | null;
  created_at: string | null;
  last_modified: string | null;
  tags: string[];
  frequency: string | null;
  license: string | null;
  resources_count: number;
}

export interface FrenchResourceProfile {
  resource_id: string;
  name: string;
  original_name: string;
  title: string;
  original_title: string;
  format: string | null;
  size: number | null;
  columns: FrenchResourceColumn[];
  total_rows: number | null;
  encoding: string | null;
  delimiter: string | null;
}

export interface FrenchResourceColumn {
  name: string;
  original_name: string;
  title: string;
  original_title: string;
  type: string | null;
  format: string | null;
  description: string;
  original_description: string;
}

export interface FrenchResourceLines {
  resource_id: string;
  lines: Record<string, any>[];
  total_rows: number;
  page: number;
  page_size: number;
  has_more: boolean;
}

export interface FrenchCompany {
  siren: string | null;
  siret: string | null;
  complete_name: string;
  original_complete_name: string;
  company_name: string;
  original_company_name: string;
  company_category: string | null;
  creation_date: string | null;
  administrative_status: string | null;
  legal_form: string;
  original_legal_form: string;
  main_activity: string;
  original_main_activity: string;
  employee_size_range: string | null;
  address: any | null;
  matching_score: number | null;
}

export interface FrenchGovResponse<T> {
  data: T;
  metadata: Record<string, any>;
  error: string | null;
}

// ===== Universal CKAN Types =====

export type CkanView = 'organizations' | 'datasets' | 'resources' | 'search';

export type CkanPortalCode = 'us' | 'uk' | 'au' | 'it' | 'br' | 'lv' | 'si' | 'uy';

export interface CkanPortalInfo {
  code: CkanPortalCode;
  name: string;
  flag: string;
  url: string;
}

export interface CkanOrganization {
  name: string;
  display_name: string;
  country: string;
}

export interface CkanDataset {
  id?: string;
  name: string;
  display_name?: string;
  title?: string;
  notes?: string;
  organization_name?: string;
  organization?: Record<string, any>;
  num_resources?: number;
  metadata_created?: string;
  metadata_modified?: string;
  tags?: Array<{ name: string } | string>;
  resource_types?: string[];
  resources?: any[];
  country?: string;
  [key: string]: any;
}

export interface CkanResource {
  id: string;
  name: string;
  description: string;
  format: string;
  url: string;
  size: number | null;
  mimetype: string | null;
  created: string | null;
  last_modified: string | null;
  dataset_name?: string;
  dataset_title?: string;
  organization?: string;
  [key: string]: any;
}

export interface CkanResponse<T> {
  data: T;
  metadata: Record<string, any>;
  error: string | null;
}

// ===== Hong Kong Government (data.gov.hk) Types =====

export type HkGovView = 'categories' | 'datasets' | 'resources' | 'historical' | 'search';

export interface HkCategory {
  id: string;
  name: string;
  display_name: string;
  title: string;
  description: string;
  dataset_count: number;
}

export interface HkDataset {
  id: string;
  name: string;
  title: string;
  notes: string;
  organization?: Record<string, any>;
  num_resources: number;
  metadata_created: string | null;
  metadata_modified: string | null;
  tags: string[];
  resources?: any[];
}

export interface HkResource {
  id: string;
  name: string;
  description: string;
  format: string;
  url: string;
  size: number | null;
  mimetype: string | null;
  created: string | null;
  last_modified: string | null;
  package_id: string;
}

export interface HkHistoricalFile {
  name: string;
  url: string;
  format: string;
  category: string;
  date: string;
  size: number | null;
  [key: string]: any;
}

export interface HkGovResponse<T> {
  data: T;
  metadata: Record<string, any>;
  error: string | null;
}
