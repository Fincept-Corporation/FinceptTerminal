// Government Data Tab Constants

import type { GovProviderConfig, GovProvider } from './types';
import { FINCEPT_COLORS } from '@/components/common/charts';

// Re-export shared colors under the local name for backwards-compatibility
export const FINCEPT = FINCEPT_COLORS;

// Provider configurations
export const GOV_PROVIDERS: GovProviderConfig[] = [
  {
    id: 'us-treasury',
    name: 'US Treasury',
    fullName: 'U.S. Department of the Treasury',
    description: 'Treasury securities prices, auctions & market data from TreasuryDirect.gov',
    color: '#3B82F6',
    country: 'United States',
    flag: '\uD83C\uDDFA\uD83C\uDDF8',
    endpoints: [
      {
        id: 'treasury_prices',
        name: 'Treasury Prices',
        description: 'End-of-day prices for all Treasury securities (Bills, Notes, Bonds, TIPS, FRN)',
        command: 'treasury_prices',
      },
      {
        id: 'treasury_auctions',
        name: 'Treasury Auctions',
        description: 'Auction results with bid-to-cover ratios, accepted amounts & pricing',
        command: 'treasury_auctions',
      },
      {
        id: 'summary',
        name: 'Market Summary',
        description: 'Aggregate statistics: security counts, yield & price distributions',
        command: 'summary',
      },
    ],
  },
  {
    id: 'canada-gov',
    name: 'Canada Open Gov',
    fullName: 'Government of Canada Open Data',
    description: 'Open data portal with datasets from Canadian federal departments & agencies',
    color: '#EF4444',
    country: 'Canada',
    flag: '\uD83C\uDDE8\uD83C\uDDE6',
    endpoints: [
      {
        id: 'publishers',
        name: 'Publishers',
        description: 'Federal departments & agencies publishing open data',
        command: 'publishers',
      },
      {
        id: 'datasets',
        name: 'Datasets',
        description: 'Browse datasets from a specific publisher',
        command: 'datasets',
      },
      {
        id: 'search',
        name: 'Search',
        description: 'Search across all Government of Canada datasets',
        command: 'search',
      },
      {
        id: 'recent',
        name: 'Recent',
        description: 'Recently updated datasets across all publishers',
        command: 'recent-datasets',
      },
    ],
  },
  {
    id: 'openafrica',
    name: 'openAFRICA',
    fullName: 'openAFRICA Open Data Portal',
    description: 'African open data portal with datasets from organizations across the continent',
    color: '#F59E0B',
    country: 'Africa',
    flag: '\uD83C\uDF0D',
    endpoints: [
      {
        id: 'organizations',
        name: 'Organizations',
        description: 'Data publishing organizations on openAFRICA',
        command: 'organizations',
      },
      {
        id: 'datasets',
        name: 'Datasets',
        description: 'Browse datasets from a specific organization',
        command: 'org-datasets',
      },
      {
        id: 'search',
        name: 'Search',
        description: 'Search across all openAFRICA datasets',
        command: 'search',
      },
      {
        id: 'recent',
        name: 'Recent',
        description: 'Recently updated datasets across all organizations',
        command: 'recent',
      },
    ],
  },
  {
    id: 'spain',
    name: 'Spain Open Data',
    fullName: 'datos.gob.es - Spain Open Data',
    description: 'Spanish government open data portal with catalogues, datasets & resources',
    color: '#DC2626',
    country: 'Spain',
    flag: '\uD83C\uDDEA\uD83C\uDDF8',
    endpoints: [
      {
        id: 'catalogues',
        name: 'Catalogues',
        description: 'Government publishers/catalogues on datos.gob.es',
        command: 'catalogues',
      },
      {
        id: 'datasets',
        name: 'Datasets',
        description: 'Browse datasets from a specific publisher',
        command: 'datasets',
      },
      {
        id: 'search',
        name: 'Search',
        description: 'Search across all Spanish government datasets',
        command: 'search',
      },
    ],
  },
  {
    id: 'finland-pxweb',
    name: 'Statistics Finland',
    fullName: 'Statistics Finland (PxWeb)',
    description: 'Finnish statistical database via PxWeb API — StatFin tables & time series',
    color: '#0EA5E9',
    country: 'Finland',
    flag: '\uD83C\uDDEB\uD83C\uDDEE',
    endpoints: [
      {
        id: 'browse',
        name: 'Browse',
        description: 'Browse database nodes and subject areas',
        command: 'nodes',
      },
      {
        id: 'metadata',
        name: 'Metadata',
        description: 'View table variables and value lists',
        command: 'metadata',
      },
      {
        id: 'data',
        name: 'Data',
        description: 'Fetch statistical data from tables',
        command: 'data',
      },
    ],
  },
  {
    id: 'swiss',
    name: 'Swiss Open Data',
    fullName: 'opendata.swiss - Swiss Government',
    description: 'Swiss federal open data portal with datasets in DE/FR/IT/EN from opendata.swiss',
    color: '#E11D48',
    country: 'Switzerland',
    flag: '\uD83C\uDDE8\uD83C\uDDED',
    endpoints: [
      {
        id: 'publishers',
        name: 'Publishers',
        description: 'Federal offices and organizations publishing open data',
        command: 'publishers',
      },
      {
        id: 'datasets',
        name: 'Datasets',
        description: 'Browse datasets from a specific publisher',
        command: 'datasets',
      },
      {
        id: 'search',
        name: 'Search',
        description: 'Search across all Swiss government datasets',
        command: 'search',
      },
      {
        id: 'recent',
        name: 'Recent',
        description: 'Recently updated datasets across all publishers',
        command: 'recent-datasets',
      },
    ],
  },
  {
    id: 'france',
    name: 'France Open Data',
    fullName: 'data.gouv.fr - French Government',
    description: 'French government APIs: geographic data, dataset catalog, tabular data & company registry',
    color: '#2563EB',
    country: 'France',
    flag: '\uD83C\uDDEB\uD83C\uDDF7',
    endpoints: [
      {
        id: 'geo',
        name: 'Geography',
        description: 'Municipalities, departments & regions via API Géo',
        command: 'municipalities',
      },
      {
        id: 'datasets',
        name: 'Datasets',
        description: 'Search data.gouv.fr dataset catalog',
        command: 'datasets',
      },
      {
        id: 'tabular',
        name: 'Tabular',
        description: 'Query CSV/Excel resources directly via API Tabulaire',
        command: 'profile',
      },
      {
        id: 'company',
        name: 'Company',
        description: 'French company lookup by SIREN/SIRET via API Entreprise',
        command: 'company',
      },
    ],
  },
  {
    id: 'universal-ckan',
    name: 'CKAN Portals',
    fullName: 'Universal CKAN Open Data Portals',
    description: '8 CKAN portals: US, UK, Australia, Italy, Brazil, Latvia, Slovenia, Uruguay',
    color: '#10B981',
    country: 'Multi-Country',
    flag: '\uD83C\uDF10',
    endpoints: [
      {
        id: 'organizations',
        name: 'Organizations',
        description: 'Browse data publishers / organizations',
        command: 'list_organizations',
      },
      {
        id: 'datasets',
        name: 'Datasets',
        description: 'Browse datasets from a specific organization',
        command: 'search_datasets',
      },
      {
        id: 'search',
        name: 'Search',
        description: 'Search datasets across the selected portal',
        command: 'search_datasets',
      },
    ],
  },
  {
    id: 'us-congress',
    name: 'US Congress',
    fullName: 'United States Congress',
    description: 'Congressional bills, resolutions & legislative activity via Congress.gov API',
    color: '#8B5CF6',
    country: 'United States',
    flag: '\uD83C\uDDFA\uD83C\uDDF8',
    endpoints: [
      {
        id: 'congress_bills',
        name: 'Bills',
        description: 'Browse Congressional bills & resolutions with filters',
        command: 'congress_bills',
      },
      {
        id: 'bill_info',
        name: 'Bill Detail',
        description: 'Detailed bill information with sponsors, actions & summaries',
        command: 'bill_info',
      },
      {
        id: 'summary',
        name: 'Summary',
        description: 'Bills count by type for a given Congress session',
        command: 'summary',
      },
    ],
  },
];

// Congress.gov bill type options
export const CONGRESS_BILL_TYPES = [
  { value: '', label: 'All Types' },
  { value: 'hr', label: 'House Bill' },
  { value: 's', label: 'Senate Bill' },
  { value: 'hjres', label: 'House Joint Res.' },
  { value: 'sjres', label: 'Senate Joint Res.' },
  { value: 'hconres', label: 'House Con. Res.' },
  { value: 'sconres', label: 'Senate Con. Res.' },
  { value: 'hres', label: 'House Simple Res.' },
  { value: 'sres', label: 'Senate Simple Res.' },
];

// Security type options for US Treasury
export const SECURITY_TYPES = [
  { value: 'all', label: 'All Securities' },
  { value: 'bill', label: 'Treasury Bills' },
  { value: 'note', label: 'Treasury Notes' },
  { value: 'bond', label: 'Treasury Bonds' },
  { value: 'tips', label: 'TIPS' },
  { value: 'frn', label: 'Floating Rate Notes' },
  { value: 'cmb', label: 'Cash Management Bills' },
];

// Auction security type options (different casing from API)
export const AUCTION_SECURITY_TYPES = [
  { value: 'all', label: 'All Types' },
  { value: 'Bill', label: 'Bills' },
  { value: 'Note', label: 'Notes' },
  { value: 'Bond', label: 'Bonds' },
  { value: 'TIPS', label: 'TIPS' },
  { value: 'FRN', label: 'FRN' },
  { value: 'CMB', label: 'CMB' },
];

// Key auction fields to display in the table
export const AUCTION_DISPLAY_FIELDS = [
  { key: 'cusip', label: 'CUSIP', width: 100 },
  { key: 'securityType', label: 'Type', width: 60 },
  { key: 'securityTerm', label: 'Term', width: 80 },
  { key: 'auctionDate', label: 'Auction Date', width: 110 },
  { key: 'issueDate', label: 'Issue Date', width: 110 },
  { key: 'maturityDate', label: 'Maturity', width: 110 },
  { key: 'highDiscountRate', label: 'High Rate', width: 90, format: 'rate' },
  { key: 'highPrice', label: 'High Price', width: 90, format: 'price' },
  { key: 'bidToCoverRatio', label: 'Bid/Cover', width: 80, format: 'ratio' },
  { key: 'offeringAmount', label: 'Offering ($)', width: 120, format: 'currency' },
  { key: 'totalAccepted', label: 'Accepted ($)', width: 120, format: 'currency' },
  { key: 'allocationPercentage', label: 'Alloc %', width: 70, format: 'pct' },
];

// Universal CKAN portal list
export const CKAN_PORTALS: { code: string; name: string; flag: string; url: string }[] = [
  { code: 'us', name: 'United States', flag: '\uD83C\uDDFA\uD83C\uDDF8', url: 'catalog.data.gov' },
  { code: 'uk', name: 'United Kingdom', flag: '\uD83C\uDDEC\uD83C\uDDE7', url: 'data.gov.uk' },
  { code: 'au', name: 'Australia', flag: '\uD83C\uDDE6\uD83C\uDDFA', url: 'data.gov.au' },
  { code: 'it', name: 'Italy', flag: '\uD83C\uDDEE\uD83C\uDDF9', url: 'dati.gov.it' },
  { code: 'br', name: 'Brazil', flag: '\uD83C\uDDE7\uD83C\uDDF7', url: 'dados.gov.br' },
  { code: 'lv', name: 'Latvia', flag: '\uD83C\uDDF1\uD83C\uDDFB', url: 'data.gov.lv' },
  { code: 'si', name: 'Slovenia', flag: '\uD83C\uDDF8\uD83C\uDDEE', url: 'podatki.gov.si' },
  { code: 'uy', name: 'Uruguay', flag: '\uD83C\uDDFA\uD83C\uDDFE', url: 'catalogodatos.gub.uy' },
];

export const getProviderConfig = (id: GovProvider): GovProviderConfig => {
  return GOV_PROVIDERS.find(p => p.id === id) || GOV_PROVIDERS[0];
};
