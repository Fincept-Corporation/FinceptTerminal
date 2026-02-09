// File: src/components/tabs/TradeVisualizationTab.tsx
// Professional Fincept Terminal-Grade Global Trade Visualization Interface

import React, { useState, useEffect, useCallback } from 'react';
import {
  Globe,
  TrendingUp,
  TrendingDown,
  RefreshCw,
  Download,
  ChevronDown,
  BarChart3,
  PieChart,
  Activity,
  DollarSign,
  Package,
  AlertCircle,
  Info,
  ArrowUpRight,
  ArrowDownRight,
} from 'lucide-react';
import { tradeService, type ChordData, type Country } from '@/services/trading/tradeService';
import { TabFooter } from '@/components/common/TabFooter';
import { useTranslation } from 'react-i18next';
import { TradeChordDiagram } from '@/components/visualization/TradeChordDiagram';

// Fincept Professional Color Palette
const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

export default function TradeVisualizationTab() {
  const { t } = useTranslation('trade');

  // Core state
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [currentTime, setCurrentTime] = useState(new Date());

  // Data state
  const [chordData, setChordData] = useState<ChordData | null>(null);
  const [countries, setCountries] = useState<Country[]>([]);
  const [products, setProducts] = useState<any[]>([]);

  // Selection state
  const [selectedCountry, setSelectedCountry] = useState('ocaus'); // Australia
  const [selectedCountryName, setSelectedCountryName] = useState('Australia');
  const [selectedYear, setSelectedYear] = useState(2019);
  const [selectedProduct, setSelectedProduct] = useState('1'); // All products
  const [topN, setTopN] = useState(15);

  // UI state
  const [showCountryDropdown, setShowCountryDropdown] = useState(false);
  const [showYearDropdown, setShowYearDropdown] = useState(false);
  const [showProductDropdown, setShowProductDropdown] = useState(false);
  const [searchQuery, setSearchQuery] = useState('');
  const [view, setView] = useState<'chord' | 'table' | 'stats'>('chord');

  // Update time every second
  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  // Check API health and load initial data
  useEffect(() => {
    const initializeTab = async () => {
      // First check API health
      const healthStatus = await tradeService.isApiHealthy();
      if (!healthStatus.healthy) {
        setError(`Trade API is currently unavailable: ${healthStatus.error || 'Service temporarily down'}. Please try again later.`);
        return;
      }
      // If healthy, load data
      loadCountries();
      loadProducts();
    };
    initializeTab();
  }, []);

  // Load chord data when parameters change
  useEffect(() => {
    if (selectedCountry) {
      loadChordData();
    }
  }, [selectedCountry, selectedYear, selectedProduct, topN]);

  const loadCountries = async () => {
    try {
      const data = await tradeService.getCountries();
      setCountries(data);
    } catch (err) {
      console.error('Failed to load countries:', err);
    }
  };

  const loadProducts = async () => {
    try {
      const data = await tradeService.getProducts();
      setProducts(data);
    } catch (err) {
      console.error('Failed to load products:', err);
    }
  };

  const loadChordData = async () => {
    setLoading(true);
    setError(null);

    try {
      const data = await tradeService.getChordData(
        selectedCountry,
        selectedYear,
        selectedProduct,
        topN
      );
      setChordData(data);
      setSelectedCountryName(data.country_name);
    } catch (err: any) {
      setError(err.message || 'Failed to load trade data');
      console.error('Failed to load chord data:', err);
    } finally {
      setLoading(false);
    }
  };

  const handleCountrySelect = (country: Country) => {
    setSelectedCountry(country.country_id);
    setSelectedCountryName(country.country_name);
    setShowCountryDropdown(false);
    setSearchQuery('');
  };

  const exportData = () => {
    if (!chordData) return;

    const csv = [
      'Partner,Export Value (USD),Import Value (USD),Total Trade (USD)',
      ...chordData.partners.map(p =>
        `${p.partner_name},${p.export_value},${p.import_value},${p.total_trade}`
      )
    ].join('\n');

    const blob = new Blob([csv], { type: 'text/csv' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `trade_${selectedCountryName}_${selectedYear}_${Date.now()}.csv`;
    a.click();
    URL.revokeObjectURL(url);
  };

  const filteredCountries = countries.filter(c =>
    c.country_name.toLowerCase().includes(searchQuery.toLowerCase()) ||
    c.country_id.toLowerCase().includes(searchQuery.toLowerCase())
  );

  const formatCurrency = (value: number) => {
    if (value >= 1e12) return `$${(value / 1e12).toFixed(2)}T`;
    if (value >= 1e9) return `$${(value / 1e9).toFixed(2)}B`;
    if (value >= 1e6) return `$${(value / 1e6).toFixed(2)}M`;
    return `$${value.toFixed(2)}`;
  };

  return (
    <div style={{
      height: '100%',
      backgroundColor: FINCEPT.DARK_BG,
      color: FINCEPT.WHITE,
      fontFamily: '"Segoe UI", "Helvetica Neue", Arial, sans-serif',
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden'
    }}>
      {/* ========== HEADER ========== */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `2px solid ${FINCEPT.ORANGE}`,
        padding: '10px 16px',
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <Globe size={20} color={FINCEPT.ORANGE} />
          <span style={{
            fontSize: '16px',
            fontWeight: 700,
            color: FINCEPT.ORANGE,
            letterSpacing: '0.5px'
          }}>
            GLOBAL TRADE ANALYSIS
          </span>
          <div style={{ height: '16px', width: '1px', backgroundColor: FINCEPT.BORDER }} />
          <span style={{ fontSize: '11px', color: FINCEPT.GRAY }}>
            Real-time Trade Flow Visualization
          </span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <span style={{ fontSize: '11px', color: FINCEPT.CYAN }}>
            {currentTime.toUTCString()}
          </span>
        </div>
      </div>

      {/* ========== CONTROL BAR ========== */}
      <div style={{
        backgroundColor: FINCEPT.PANEL_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        padding: '12px 16px',
        display: 'flex',
        alignItems: 'center',
        gap: '16px',
        flexShrink: 0
      }}>
        {/* Country Selector */}
        <div style={{ position: 'relative', minWidth: '200px' }}>
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px' }}>COUNTRY</div>
          <div
            onClick={() => setShowCountryDropdown(!showCountryDropdown)}
            style={{
              padding: '8px 12px',
              backgroundColor: FINCEPT.HEADER_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              cursor: 'pointer',
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center'
            }}
          >
            <span style={{ fontSize: '13px', fontWeight: 600, color: FINCEPT.ORANGE }}>
              {selectedCountryName}
            </span>
            <ChevronDown size={14} color={FINCEPT.GRAY} />
          </div>

          {showCountryDropdown && (
            <div style={{
              position: 'absolute',
              top: '100%',
              left: 0,
              right: 0,
              marginTop: '4px',
              backgroundColor: FINCEPT.PANEL_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              maxHeight: '400px',
              overflow: 'hidden',
              zIndex: 1000,
              boxShadow: `0 4px 16px ${FINCEPT.DARK_BG}80`
            }}>
              <div style={{ padding: '8px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                <input
                  type="text"
                  placeholder="Search countries..."
                  value={searchQuery}
                  onChange={(e) => setSearchQuery(e.target.value)}
                  autoFocus
                  style={{
                    width: '100%',
                    padding: '6px 8px',
                    backgroundColor: FINCEPT.HEADER_BG,
                    color: FINCEPT.WHITE,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    fontSize: '11px',
                    outline: 'none'
                  }}
                />
              </div>
              <div style={{ maxHeight: '340px', overflowY: 'auto' }}>
                {filteredCountries.map(country => (
                  <div
                    key={country.country_id}
                    onClick={() => handleCountrySelect(country)}
                    style={{
                      padding: '8px 12px',
                      cursor: 'pointer',
                      fontSize: '11px',
                      backgroundColor: country.country_id === selectedCountry ? `${FINCEPT.ORANGE}20` : 'transparent',
                      color: country.country_id === selectedCountry ? FINCEPT.ORANGE : FINCEPT.WHITE,
                      borderLeft: country.country_id === selectedCountry ? `3px solid ${FINCEPT.ORANGE}` : 'none'
                    }}
                    onMouseEnter={(e) => {
                      if (country.country_id !== selectedCountry) {
                        e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                      }
                    }}
                    onMouseLeave={(e) => {
                      if (country.country_id !== selectedCountry) {
                        e.currentTarget.style.backgroundColor = 'transparent';
                      }
                    }}
                  >
                    <div style={{ fontWeight: 600 }}>{country.country_name}</div>
                    <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                      {country.continent_name}
                    </div>
                  </div>
                ))}
              </div>
            </div>
          )}
        </div>

        {/* Year Selector */}
        <div style={{ minWidth: '100px' }}>
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px' }}>YEAR</div>
          <div
            onClick={() => setShowYearDropdown(!showYearDropdown)}
            style={{
              padding: '8px 12px',
              backgroundColor: FINCEPT.HEADER_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              cursor: 'pointer',
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center',
              position: 'relative'
            }}
          >
            <span style={{ fontSize: '13px', fontWeight: 600, color: FINCEPT.YELLOW }}>
              {selectedYear}
            </span>
            <ChevronDown size={14} color={FINCEPT.GRAY} />

            {showYearDropdown && (
              <div style={{
                position: 'absolute',
                top: '100%',
                left: 0,
                right: 0,
                marginTop: '4px',
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                maxHeight: '300px',
                overflowY: 'auto',
                zIndex: 1000
              }}>
                {Array.from({ length: 29 }, (_, i) => 2023 - i).map(year => (
                  <div
                    key={year}
                    onClick={(e) => {
                      e.stopPropagation();
                      setSelectedYear(year);
                      setShowYearDropdown(false);
                    }}
                    style={{
                      padding: '8px 12px',
                      cursor: 'pointer',
                      fontSize: '11px',
                      backgroundColor: year === selectedYear ? `${FINCEPT.YELLOW}20` : 'transparent',
                      color: year === selectedYear ? FINCEPT.YELLOW : FINCEPT.WHITE
                    }}
                    onMouseEnter={(e) => {
                      if (year !== selectedYear) {
                        e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                      }
                    }}
                    onMouseLeave={(e) => {
                      if (year !== selectedYear) {
                        e.currentTarget.style.backgroundColor = 'transparent';
                      }
                    }}
                  >
                    {year}
                  </div>
                ))}
              </div>
            )}
          </div>
        </div>

        {/* Top N Selector */}
        <div style={{ minWidth: '120px' }}>
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px' }}>TOP PARTNERS</div>
          <select
            value={topN}
            onChange={(e) => setTopN(parseInt(e.target.value))}
            style={{
              padding: '8px 12px',
              backgroundColor: FINCEPT.HEADER_BG,
              color: FINCEPT.CYAN,
              border: `1px solid ${FINCEPT.BORDER}`,
              fontSize: '13px',
              fontWeight: 600,
              cursor: 'pointer',
              outline: 'none',
              width: '100%'
            }}
          >
            <option value="10">Top 10</option>
            <option value="15">Top 15</option>
            <option value="20">Top 20</option>
            <option value="30">Top 30</option>
            <option value="50">Top 50</option>
          </select>
        </div>

        <div style={{ flex: 1 }} />

        {/* View Toggles */}
        <div style={{ display: 'flex', gap: '4px' }}>
          <button
            onClick={() => setView('chord')}
            style={{
              padding: '8px 16px',
              backgroundColor: view === 'chord' ? FINCEPT.ORANGE : FINCEPT.HEADER_BG,
              color: view === 'chord' ? FINCEPT.DARK_BG : FINCEPT.WHITE,
              border: `1px solid ${view === 'chord' ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
              fontSize: '11px',
              fontWeight: 600,
              cursor: 'pointer'
            }}
          >
            <PieChart size={12} style={{ display: 'inline', marginRight: '6px' }} />
            CHORD
          </button>
          <button
            onClick={() => setView('table')}
            style={{
              padding: '8px 16px',
              backgroundColor: view === 'table' ? FINCEPT.ORANGE : FINCEPT.HEADER_BG,
              color: view === 'table' ? FINCEPT.DARK_BG : FINCEPT.WHITE,
              border: `1px solid ${view === 'table' ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
              fontSize: '11px',
              fontWeight: 600,
              cursor: 'pointer'
            }}
          >
            <BarChart3 size={12} style={{ display: 'inline', marginRight: '6px' }} />
            TABLE
          </button>
          <button
            onClick={() => setView('stats')}
            style={{
              padding: '8px 16px',
              backgroundColor: view === 'stats' ? FINCEPT.ORANGE : FINCEPT.HEADER_BG,
              color: view === 'stats' ? FINCEPT.DARK_BG : FINCEPT.WHITE,
              border: `1px solid ${view === 'stats' ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
              fontSize: '11px',
              fontWeight: 600,
              cursor: 'pointer'
            }}
          >
            <Activity size={12} style={{ display: 'inline', marginRight: '6px' }} />
            STATS
          </button>
        </div>

        {/* Actions */}
        <button
          onClick={loadChordData}
          disabled={loading}
          style={{
            padding: '8px 16px',
            backgroundColor: FINCEPT.HEADER_BG,
            color: loading ? FINCEPT.GRAY : FINCEPT.CYAN,
            border: `1px solid ${FINCEPT.BORDER}`,
            fontSize: '11px',
            fontWeight: 600,
            cursor: loading ? 'not-allowed' : 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '6px'
          }}
        >
          <RefreshCw size={12} className={loading ? 'animate-spin' : ''} />
          {loading ? 'LOADING...' : 'REFRESH'}
        </button>

        <button
          onClick={exportData}
          disabled={!chordData}
          style={{
            padding: '8px 16px',
            backgroundColor: FINCEPT.HEADER_BG,
            color: chordData ? FINCEPT.GREEN : FINCEPT.GRAY,
            border: `1px solid ${FINCEPT.BORDER}`,
            fontSize: '11px',
            fontWeight: 600,
            cursor: chordData ? 'pointer' : 'not-allowed',
            display: 'flex',
            alignItems: 'center',
            gap: '6px'
          }}
        >
          <Download size={12} />
          EXPORT
        </button>
      </div>

      {/* ========== MAIN CONTENT ========== */}
      <div style={{ flex: 1, overflow: 'hidden', display: 'flex' }}>
        {/* Main Panel */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
          {error && (
            <div style={{
              padding: '16px 20px',
              backgroundColor: `${FINCEPT.RED}15`,
              border: `1px solid ${FINCEPT.RED}`,
              borderLeft: `4px solid ${FINCEPT.RED}`,
              margin: '16px',
              borderRadius: '4px'
            }}>
              <div style={{ display: 'flex', alignItems: 'flex-start', gap: '12px' }}>
                <AlertCircle size={20} color={FINCEPT.RED} style={{ flexShrink: 0, marginTop: '2px' }} />
                <div>
                  <div style={{ fontSize: '13px', fontWeight: 600, color: FINCEPT.RED, marginBottom: '6px' }}>
                    Trade Data Unavailable
                  </div>
                  <div style={{ fontSize: '11px', color: FINCEPT.GRAY, lineHeight: '1.5' }}>
                    {error}
                  </div>
                  <button
                    onClick={() => {
                      setError(null);
                      loadChordData();
                    }}
                    style={{
                      marginTop: '12px',
                      padding: '6px 12px',
                      backgroundColor: FINCEPT.HEADER_BG,
                      color: FINCEPT.CYAN,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      fontSize: '10px',
                      fontWeight: 600,
                      cursor: 'pointer',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '6px'
                    }}
                  >
                    <RefreshCw size={10} />
                    RETRY
                  </button>
                </div>
              </div>
            </div>
          )}

          {loading && (
            <div style={{
              flex: 1,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              flexDirection: 'column',
              gap: '16px'
            }}>
              <RefreshCw size={32} color={FINCEPT.ORANGE} className="animate-spin" />
              <span style={{ fontSize: '13px', color: FINCEPT.GRAY }}>
                Loading trade data...
              </span>
            </div>
          )}

          {!loading && !error && chordData && view === 'chord' && (
            <div style={{
              flex: 1,
              padding: '16px',
              display: 'flex',
              flexDirection: 'column',
              overflow: 'hidden'
            }}>
              <TradeChordDiagram data={chordData} />
            </div>
          )}

          {!loading && !error && chordData && view === 'table' && (
            <div style={{
              flex: 1,
              padding: '16px',
              overflow: 'auto'
            }}>
              <div style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '4px'
              }}>
                <table style={{ width: '100%', borderCollapse: 'collapse' }}>
                  <thead>
                    <tr style={{ backgroundColor: FINCEPT.HEADER_BG, borderBottom: `2px solid ${FINCEPT.ORANGE}` }}>
                      <th style={{ padding: '12px', textAlign: 'left', fontSize: '11px', fontWeight: 600, color: FINCEPT.ORANGE }}>
                        RANK
                      </th>
                      <th style={{ padding: '12px', textAlign: 'left', fontSize: '11px', fontWeight: 600, color: FINCEPT.ORANGE }}>
                        TRADING PARTNER
                      </th>
                      <th style={{ padding: '12px', textAlign: 'right', fontSize: '11px', fontWeight: 600, color: FINCEPT.ORANGE }}>
                        EXPORTS ($M)
                      </th>
                      <th style={{ padding: '12px', textAlign: 'right', fontSize: '11px', fontWeight: 600, color: FINCEPT.ORANGE }}>
                        IMPORTS ($M)
                      </th>
                      <th style={{ padding: '12px', textAlign: 'right', fontSize: '11px', fontWeight: 600, color: FINCEPT.ORANGE }}>
                        TOTAL TRADE ($M)
                      </th>
                    </tr>
                  </thead>
                  <tbody>
                    {chordData.partners.map((partner, idx) => (
                      <tr
                        key={partner.partner_id}
                        style={{
                          borderBottom: `1px solid ${FINCEPT.BORDER}`,
                          backgroundColor: idx % 2 === 0 ? 'transparent' : `${FINCEPT.BORDER}40`
                        }}
                      >
                        <td style={{ padding: '10px 12px', fontSize: '11px', color: FINCEPT.GRAY }}>
                          {idx + 1}
                        </td>
                        <td style={{ padding: '10px 12px', fontSize: '12px', fontWeight: 600, color: FINCEPT.WHITE }}>
                          {partner.partner_name}
                        </td>
                        <td style={{ padding: '10px 12px', textAlign: 'right', fontSize: '12px', color: FINCEPT.GREEN }}>
                          {formatCurrency(partner.export_value)}
                        </td>
                        <td style={{ padding: '10px 12px', textAlign: 'right', fontSize: '12px', color: FINCEPT.RED }}>
                          {formatCurrency(partner.import_value)}
                        </td>
                        <td style={{ padding: '10px 12px', textAlign: 'right', fontSize: '13px', fontWeight: 600, color: FINCEPT.YELLOW }}>
                          {formatCurrency(partner.total_trade)}
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            </div>
          )}

          {!loading && !error && chordData && view === 'stats' && (
            <div style={{
              flex: 1,
              padding: '16px',
              overflow: 'auto'
            }}>
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(280px, 1fr))', gap: '16px' }}>
                {/* Total Trade */}
                <div style={{
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  padding: '16px',
                  borderRadius: '4px'
                }}>
                  <div style={{ fontSize: '10px', color: FINCEPT.GRAY, marginBottom: '8px' }}>TOTAL TRADE</div>
                  <div style={{ fontSize: '24px', fontWeight: 700, color: FINCEPT.YELLOW }}>
                    {formatCurrency(chordData.total_trade)}
                  </div>
                  <div style={{ fontSize: '10px', color: FINCEPT.GRAY, marginTop: '4px' }}>
                    {selectedYear} • {chordData.partners.length} partners
                  </div>
                </div>

                {/* Total Exports */}
                <div style={{
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  padding: '16px',
                  borderRadius: '4px'
                }}>
                  <div style={{ fontSize: '10px', color: FINCEPT.GRAY, marginBottom: '8px' }}>TOTAL EXPORTS</div>
                  <div style={{ fontSize: '24px', fontWeight: 700, color: FINCEPT.GREEN }}>
                    {formatCurrency(chordData.total_export)}
                  </div>
                  <div style={{ fontSize: '10px', color: FINCEPT.GRAY, marginTop: '4px', display: 'flex', alignItems: 'center', gap: '4px' }}>
                    <ArrowUpRight size={12} color={FINCEPT.GREEN} />
                    Outbound trade flows
                  </div>
                </div>

                {/* Total Imports */}
                <div style={{
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  padding: '16px',
                  borderRadius: '4px'
                }}>
                  <div style={{ fontSize: '10px', color: FINCEPT.GRAY, marginBottom: '8px' }}>TOTAL IMPORTS</div>
                  <div style={{ fontSize: '24px', fontWeight: 700, color: FINCEPT.RED }}>
                    {formatCurrency(chordData.total_import)}
                  </div>
                  <div style={{ fontSize: '10px', color: FINCEPT.GRAY, marginTop: '4px', display: 'flex', alignItems: 'center', gap: '4px' }}>
                    <ArrowDownRight size={12} color={FINCEPT.RED} />
                    Inbound trade flows
                  </div>
                </div>

                {/* Trade Balance */}
                <div style={{
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  padding: '16px',
                  borderRadius: '4px'
                }}>
                  <div style={{ fontSize: '10px', color: FINCEPT.GRAY, marginBottom: '8px' }}>TRADE BALANCE</div>
                  <div style={{
                    fontSize: '24px',
                    fontWeight: 700,
                    color: (chordData.total_export - chordData.total_import) >= 0 ? FINCEPT.GREEN : FINCEPT.RED
                  }}>
                    {formatCurrency(Math.abs(chordData.total_export - chordData.total_import))}
                  </div>
                  <div style={{ fontSize: '10px', color: FINCEPT.GRAY, marginTop: '4px' }}>
                    {(chordData.total_export - chordData.total_import) >= 0 ? 'Surplus' : 'Deficit'}
                  </div>
                </div>

                {/* Top Partner */}
                {chordData.partners.length > 0 && (
                  <div style={{
                    backgroundColor: FINCEPT.PANEL_BG,
                    border: `1px solid ${FINCEPT.ORANGE}`,
                    padding: '16px',
                    borderRadius: '4px'
                  }}>
                    <div style={{ fontSize: '10px', color: FINCEPT.GRAY, marginBottom: '8px' }}>TOP TRADING PARTNER</div>
                    <div style={{ fontSize: '18px', fontWeight: 700, color: FINCEPT.ORANGE }}>
                      {chordData.partners[0].partner_name}
                    </div>
                    <div style={{ fontSize: '14px', color: FINCEPT.YELLOW, marginTop: '4px' }}>
                      {formatCurrency(chordData.partners[0].total_trade)}
                    </div>
                  </div>
                )}

                {/* Average Trade per Partner */}
                <div style={{
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  padding: '16px',
                  borderRadius: '4px'
                }}>
                  <div style={{ fontSize: '10px', color: FINCEPT.GRAY, marginBottom: '8px' }}>AVG PER PARTNER</div>
                  <div style={{ fontSize: '24px', fontWeight: 700, color: FINCEPT.CYAN }}>
                    {formatCurrency(chordData.total_trade / chordData.partners.length)}
                  </div>
                  <div style={{ fontSize: '10px', color: FINCEPT.GRAY, marginTop: '4px' }}>
                    Mean trade value
                  </div>
                </div>
              </div>
            </div>
          )}

          {!loading && !error && !chordData && (
            <div style={{
              flex: 1,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              flexDirection: 'column',
              gap: '16px'
            }}>
              <Globe size={48} color={FINCEPT.MUTED} />
              <span style={{ fontSize: '13px', color: FINCEPT.GRAY }}>
                Select a country to view trade relationships
              </span>
            </div>
          )}
        </div>

        {/* Right Sidebar - Info Panel */}
        {chordData && (
          <div style={{
            width: '320px',
            backgroundColor: FINCEPT.PANEL_BG,
            borderLeft: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden'
          }}>
            <div style={{
              padding: '12px 16px',
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              backgroundColor: FINCEPT.HEADER_BG
            }}>
              <span style={{ fontSize: '12px', fontWeight: 600, color: FINCEPT.ORANGE }}>
                INFORMATION
              </span>
            </div>

            <div style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>
              {/* Country Info */}
              <div style={{
                padding: '12px',
                backgroundColor: FINCEPT.HEADER_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                marginBottom: '12px',
                borderRadius: '4px'
              }}>
                <div style={{ fontSize: '10px', color: FINCEPT.GRAY, marginBottom: '8px' }}>SELECTED COUNTRY</div>
                <div style={{ fontSize: '16px', fontWeight: 700, color: FINCEPT.ORANGE }}>
                  {chordData.country_name}
                </div>
                <div style={{ fontSize: '10px', color: FINCEPT.GRAY, marginTop: '4px' }}>
                  Country ID: {chordData.country_id}
                </div>
              </div>

              {/* Data Info */}
              <div style={{
                padding: '12px',
                backgroundColor: FINCEPT.HEADER_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                marginBottom: '12px',
                borderRadius: '4px'
              }}>
                <div style={{ fontSize: '10px', color: FINCEPT.GRAY, marginBottom: '8px' }}>DATA DETAILS</div>
                <div style={{ fontSize: '11px', color: FINCEPT.WHITE, marginBottom: '6px' }}>
                  Year: <span style={{ color: FINCEPT.YELLOW }}>{chordData.year}</span>
                </div>
                <div style={{ fontSize: '11px', color: FINCEPT.WHITE, marginBottom: '6px' }}>
                  HS Code: <span style={{ color: FINCEPT.CYAN }}>{chordData.hs_code}</span>
                </div>
                <div style={{ fontSize: '11px', color: FINCEPT.WHITE }}>
                  Partners: <span style={{ color: FINCEPT.GREEN }}>{chordData.partners.length}</span>
                </div>
              </div>

              {/* Legend */}
              <div style={{
                padding: '12px',
                backgroundColor: FINCEPT.HEADER_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '4px'
              }}>
                <div style={{ fontSize: '10px', color: FINCEPT.GRAY, marginBottom: '8px' }}>LEGEND</div>
                <div style={{ fontSize: '11px', color: FINCEPT.WHITE, marginBottom: '6px', display: 'flex', alignItems: 'center', gap: '8px' }}>
                  <div style={{ width: '16px', height: '3px', backgroundColor: FINCEPT.GREEN }} />
                  <span>Exports (Outbound)</span>
                </div>
                <div style={{ fontSize: '11px', color: FINCEPT.WHITE, marginBottom: '6px', display: 'flex', alignItems: 'center', gap: '8px' }}>
                  <div style={{ width: '16px', height: '3px', backgroundColor: FINCEPT.RED }} />
                  <span>Imports (Inbound)</span>
                </div>
                <div style={{ fontSize: '11px', color: FINCEPT.WHITE, display: 'flex', alignItems: 'center', gap: '8px' }}>
                  <Info size={12} color={FINCEPT.CYAN} />
                  <span>Hover for details</span>
                </div>
              </div>
            </div>
          </div>
        )}
      </div>

      {/* ========== FOOTER ========== */}
      <TabFooter
        tabName="TRADE VISUALIZATION"
        leftInfo={[
          { label: 'Fincept Trade API', color: FINCEPT.CYAN },
          { label: chordData ? `${chordData.partners.length} Partners` : 'No Data', color: FINCEPT.GRAY },
          { label: `Year: ${selectedYear}`, color: FINCEPT.GRAY },
        ]}
        statusInfo={
          <span style={{ color: FINCEPT.GRAY }}>
            Fincept Terminal Design • Real-time Trade Intelligence
          </span>
        }
        backgroundColor={FINCEPT.PANEL_BG}
        borderColor={FINCEPT.ORANGE}
      />
    </div>
  );
}
