// Peer Comparison Panel Component - Fincept Style
// Main UI for peer analysis and comparison

import React, { useState } from 'react';
import { RadarChart, PolarGrid, PolarAngleAxis, PolarRadiusAxis, Radar, Legend, ResponsiveContainer, Tooltip, ComposedChart, Bar, XAxis, YAxis, CartesianGrid } from 'recharts';
import type { PeerCompany, CompanyMetrics, ViewMode, PercentileRanking } from '@/types/peer';
import { formatMetricValue, getMetricValue, getAllMetricNames } from '../../../../types/peer';
import { usePeerComparison } from '@/hooks/usePeerComparison';
import { PercentileRankMini } from './PercentileRankChart';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';

interface PeerComparisonPanelProps {
  initialSymbol?: string;
}

export const PeerComparisonPanel: React.FC<PeerComparisonPanelProps> = ({ initialSymbol }) => {
  const {
    targetSymbol,
    selectedPeers,
    availablePeers,
    comparisonData,
    loading,
    error,
    setTargetSymbol,
    addPeer,
    removePeer,
    runComparison,
  } = usePeerComparison(initialSymbol);

  const [viewMode, setViewMode] = useState<ViewMode>('table');
  const [selectedMetrics, setSelectedMetrics] = useState<string[]>([
    'P/E Ratio',
    'ROE',
    'Revenue Growth',
    'Debt/Equity',
  ]);
  const [manualPeerSymbol, setManualPeerSymbol] = useState('');

  const handleAddManualPeer = () => {
    if (!manualPeerSymbol.trim()) return;

    // Create a peer object with the symbol
    const newPeer: PeerCompany = {
      symbol: manualPeerSymbol.toUpperCase(),
      name: manualPeerSymbol.toUpperCase(), // Will be updated when data is fetched
      sector: '',
      industry: '',
      marketCap: 0,
      marketCapCategory: 'Large Cap',
      similarityScore: 1.0, // Manually added peers are assumed to be good matches
      matchDetails: {
        sectorMatch: true,
        industryMatch: true,
        marketCapSimilarity: 1.0,
        geographicMatch: true,
      },
    };

    addPeer(newPeer);
    setManualPeerSymbol('');
  };

  const handleRunComparison = () => {
    runComparison();
  };

  const toggleMetric = (metric: string) => {
    setSelectedMetrics((prev) =>
      prev.includes(metric) ? prev.filter((m) => m !== metric) : [...prev, metric]
    );
  };

  const handleExportToCSV = () => {
    if (!comparisonData) return;

    // Prepare CSV data
    const headers = ['Metric', comparisonData.target.symbol, ...comparisonData.peers.map((p: CompanyMetrics) => p.symbol)];
    const rows: string[][] = [];

    // Add all metrics
    const allMetrics = getAllMetricNames(comparisonData.target);
    allMetrics.forEach(metric => {
      const targetVal = getMetricValue(comparisonData.target, metric);
      const peerVals = comparisonData.peers.map((p: CompanyMetrics) => getMetricValue(p, metric));
      const row = [
        metric,
        targetVal !== undefined ? targetVal.toString() : 'N/A',
        ...peerVals.map((v: number | undefined) => v !== undefined ? v.toString() : 'N/A')
      ];
      rows.push(row);
    });

    // Convert to CSV string
    const csvContent = [
      headers.join(','),
      ...rows.map(row => row.join(','))
    ].join('\n');

    // Create download
    const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
    const link = document.createElement('a');
    const url = URL.createObjectURL(blob);
    link.setAttribute('href', url);
    link.setAttribute('download', `peer_comparison_${comparisonData.target.symbol}_${new Date().toISOString().split('T')[0]}.csv`);
    link.style.visibility = 'hidden';
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
  };

  // Prepare radar chart data with normalized values
  const radarData = React.useMemo(() => {
    if (!comparisonData) return [];

    return selectedMetrics.map((metric) => {
      const data: any = { metric };

      // Collect all values for this metric
      const targetValue = getMetricValue(comparisonData.target, metric) || 0;
      const peerValues = comparisonData.peers.map((peer: CompanyMetrics, index: number) => ({
        index,
        value: getMetricValue(peer, metric) || 0,
      }));

      // Find min and max for normalization
      const allValues = [targetValue, ...peerValues.map((p: { index: number; value: number }) => p.value)];
      const minVal = Math.min(...allValues);
      const maxVal = Math.max(...allValues);
      const range = maxVal - minVal;

      // Normalize to 0-100 scale (avoid division by zero)
      const normalize = (val: number) => {
        if (range === 0) return 50; // All values the same
        return ((val - minVal) / range) * 100;
      };

      data.target = normalize(targetValue);
      peerValues.forEach(({ index, value }: { index: number; value: number }) => {
        data[`peer${index + 1}`] = normalize(value);
      });

      // Store original values for tooltip
      data.targetOriginal = targetValue;
      peerValues.forEach(({ index, value }: { index: number; value: number }) => {
        data[`peer${index + 1}Original`] = value;
      });

      return data;
    });
  }, [comparisonData, selectedMetrics]);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.LARGE }}>
      {/* Header */}
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', gap: SPACING.LARGE }}>
        <div style={{ flex: 1 }}>
          <h2 style={{
            ...COMMON_STYLES.sectionHeader,
            fontSize: TYPOGRAPHY.LARGE,
            marginBottom: SPACING.MEDIUM,
          }}>
            PEER COMPARISON
          </h2>

          {/* Target Symbol Input */}
          <div style={{ display: 'flex', gap: SPACING.SMALL, maxWidth: '500px' }}>
            <div style={{ flex: 1 }}>
              <label style={{
                ...COMMON_STYLES.dataLabel,
                display: 'block',
                marginBottom: SPACING.TINY,
              }}>
                TARGET COMPANY SYMBOL
              </label>
              <input
                type="text"
                value={targetSymbol}
                onChange={(e) => setTargetSymbol(e.target.value.toUpperCase())}
                placeholder="e.g., AAPL, GOOGL"
                style={{
                  ...COMMON_STYLES.inputField,
                  fontSize: TYPOGRAPHY.BODY,
                }}
                onFocus={(e) => e.currentTarget.style.borderColor = FINCEPT.ORANGE}
                onBlur={(e) => e.currentTarget.style.borderColor = FINCEPT.BORDER}
              />
            </div>
          </div>
          <p style={{
            color: FINCEPT.GRAY,
            fontSize: TYPOGRAPHY.SMALL,
            marginTop: SPACING.SMALL,
          }}>
            Select target company and add peer companies to compare
          </p>
        </div>

        <div style={{ display: 'flex', gap: SPACING.SMALL }}>
          {comparisonData && (
            <button
              onClick={handleExportToCSV}
              style={{
                ...COMMON_STYLES.buttonSecondary,
                display: 'flex',
                alignItems: 'center',
                gap: SPACING.SMALL,
              }}
              onMouseEnter={(e) => {
                e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                e.currentTarget.style.borderColor = FINCEPT.ORANGE;
              }}
              onMouseLeave={(e) => {
                e.currentTarget.style.backgroundColor = 'transparent';
                e.currentTarget.style.borderColor = FINCEPT.BORDER;
              }}
            >
              <svg style={{ width: '14px', height: '14px' }} fill="none" viewBox="0 0 24 24" stroke="currentColor">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 10v6m0 0l-3-3m3 3l3-3m2 8H7a2 2 0 01-2-2V5a2 2 0 012-2h5.586a1 1 0 01.707.293l5.414 5.414a1 1 0 01.293.707V19a2 2 0 01-2 2z" />
              </svg>
              <span>EXPORT CSV</span>
            </button>
          )}
          <button
            onClick={handleRunComparison}
            disabled={loading || !targetSymbol || selectedPeers.length === 0}
            style={{
              ...COMMON_STYLES.buttonPrimary,
              opacity: (loading || !targetSymbol || selectedPeers.length === 0) ? 0.5 : 1,
              cursor: (loading || !targetSymbol || selectedPeers.length === 0) ? 'not-allowed' : 'pointer',
            }}
            onMouseEnter={(e) => {
              if (!loading && targetSymbol && selectedPeers.length > 0) {
                e.currentTarget.style.opacity = '0.85';
              }
            }}
            onMouseLeave={(e) => {
              if (!loading && targetSymbol && selectedPeers.length > 0) {
                e.currentTarget.style.opacity = '1';
              }
            }}
          >
            {loading ? 'COMPARING...' : 'RUN COMPARISON'}
          </button>
        </div>
      </div>

      {/* Error Display */}
      {error && (
        <div style={{
          backgroundColor: `${FINCEPT.RED}15`,
          border: `1px solid ${FINCEPT.RED}`,
          padding: SPACING.DEFAULT,
        }}>
          <p style={{ color: FINCEPT.RED, fontSize: TYPOGRAPHY.BODY }}>{error}</p>
        </div>
      )}

      {/* Peer Selection */}
      <div style={{
        ...COMMON_STYLES.panel,
        padding: SPACING.LARGE,
      }}>
        <h3 style={{
          color: FINCEPT.WHITE,
          fontSize: TYPOGRAPHY.SUBHEADING,
          fontWeight: TYPOGRAPHY.BOLD,
          marginBottom: SPACING.MEDIUM,
          letterSpacing: TYPOGRAPHY.WIDE,
          textTransform: 'uppercase',
        }}>
          SELECTED PEERS ({selectedPeers.length})
        </h3>

        {selectedPeers.length === 0 ? (
          <p style={{
            color: FINCEPT.GRAY,
            fontSize: TYPOGRAPHY.BODY,
          }}>
            No peers selected. Add peers manually using the input below.
          </p>
        ) : (
          <div style={{
            display: 'grid',
            gridTemplateColumns: 'repeat(auto-fill, minmax(250px, 1fr))',
            gap: SPACING.MEDIUM,
          }}>
            {selectedPeers.map((peer: PeerCompany) => (
              <div
                key={peer.symbol}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'space-between',
                  padding: SPACING.MEDIUM,
                  backgroundColor: FINCEPT.DARK_BG,
                  border: BORDERS.STANDARD,
                }}
              >
                <div style={{ flex: 1 }}>
                  <span style={{
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: FINCEPT.WHITE,
                    fontSize: TYPOGRAPHY.DEFAULT,
                  }}>
                    {peer.symbol}
                  </span>
                  {peer.name && peer.name !== peer.symbol && (
                    <p style={{
                      fontSize: TYPOGRAPHY.SMALL,
                      color: FINCEPT.GRAY,
                      marginTop: SPACING.TINY,
                      overflow: 'hidden',
                      textOverflow: 'ellipsis',
                      whiteSpace: 'nowrap',
                    }}>
                      {peer.name}
                    </p>
                  )}
                </div>
                <button
                  onClick={() => removePeer(peer.symbol)}
                  style={{
                    marginLeft: SPACING.SMALL,
                    padding: SPACING.TINY,
                    backgroundColor: 'transparent',
                    border: 'none',
                    cursor: 'pointer',
                    color: FINCEPT.GRAY,
                    transition: 'color 0.2s',
                  }}
                  onMouseEnter={(e) => e.currentTarget.style.color = FINCEPT.RED}
                  onMouseLeave={(e) => e.currentTarget.style.color = FINCEPT.GRAY}
                >
                  <svg style={{ width: '16px', height: '16px' }} fill="none" viewBox="0 0 24 24" stroke="currentColor">
                    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M6 18L18 6M6 6l12 12" />
                  </svg>
                </button>
              </div>
            ))}
          </div>
        )}

        {/* Manual Peer Input */}
        <div style={{ marginTop: SPACING.LARGE }}>
          <h4 style={{
            ...COMMON_STYLES.dataLabel,
            marginBottom: SPACING.SMALL,
          }}>
            ADD PEER MANUALLY
          </h4>
          <div style={{ display: 'flex', gap: SPACING.SMALL }}>
            <input
              type="text"
              value={manualPeerSymbol}
              onChange={(e) => setManualPeerSymbol(e.target.value.toUpperCase())}
              onKeyPress={(e) => e.key === 'Enter' && handleAddManualPeer()}
              placeholder="e.g., MSFT, GOOGL"
              style={{
                ...COMMON_STYLES.inputField,
                flex: 1,
                fontSize: TYPOGRAPHY.BODY,
              }}
              onFocus={(e) => e.currentTarget.style.borderColor = FINCEPT.ORANGE}
              onBlur={(e) => e.currentTarget.style.borderColor = FINCEPT.BORDER}
            />
            <button
              onClick={handleAddManualPeer}
              disabled={!manualPeerSymbol.trim()}
              style={{
                ...COMMON_STYLES.buttonPrimary,
                opacity: !manualPeerSymbol.trim() ? 0.5 : 1,
                cursor: !manualPeerSymbol.trim() ? 'not-allowed' : 'pointer',
              }}
              onMouseEnter={(e) => {
                if (manualPeerSymbol.trim()) {
                  e.currentTarget.style.opacity = '0.85';
                }
              }}
              onMouseLeave={(e) => {
                if (manualPeerSymbol.trim()) {
                  e.currentTarget.style.opacity = '1';
                }
              }}
            >
              ADD PEER
            </button>
          </div>
          <p style={{
            fontSize: TYPOGRAPHY.SMALL,
            color: FINCEPT.MUTED,
            marginTop: SPACING.TINY,
          }}>
            Tip: Add companies from the same industry/sector for better comparison
          </p>
        </div>

        {/* Available Peers */}
        {availablePeers.length > 0 && (
          <div style={{ marginTop: SPACING.LARGE }}>
            <h4 style={{
              ...COMMON_STYLES.dataLabel,
              marginBottom: SPACING.SMALL,
            }}>
              AVAILABLE PEERS
            </h4>
            <div style={{ display: 'flex', flexWrap: 'wrap', gap: SPACING.SMALL }}>
              {availablePeers
                .filter((peer: PeerCompany) => !selectedPeers.find((p: PeerCompany) => p.symbol === peer.symbol))
                .map((peer: PeerCompany) => (
                  <button
                    key={peer.symbol}
                    onClick={() => addPeer(peer)}
                    style={{
                      padding: `${SPACING.TINY} ${SPACING.MEDIUM}`,
                      fontSize: TYPOGRAPHY.BODY,
                      backgroundColor: FINCEPT.DARK_BG,
                      border: BORDERS.STANDARD,
                      color: FINCEPT.WHITE,
                      cursor: 'pointer',
                      transition: 'all 0.2s',
                      fontWeight: TYPOGRAPHY.SEMIBOLD,
                    }}
                    onMouseEnter={(e) => {
                      e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                      e.currentTarget.style.borderColor = FINCEPT.ORANGE;
                    }}
                    onMouseLeave={(e) => {
                      e.currentTarget.style.backgroundColor = FINCEPT.DARK_BG;
                      e.currentTarget.style.borderColor = FINCEPT.BORDER;
                    }}
                  >
                    + {peer.symbol}
                  </button>
                ))}
            </div>
          </div>
        )}
      </div>

      {/* View Mode Selector */}
      {comparisonData && (
        <div style={{
          display: 'flex',
          gap: SPACING.SMALL,
          borderBottom: BORDERS.STANDARD,
        }}>
          {(['table', 'chart', 'radar'] as ViewMode[]).map((mode) => (
            <button
              key={mode}
              onClick={() => setViewMode(mode)}
              style={{
                padding: `${SPACING.MEDIUM} ${SPACING.LARGE}`,
                fontWeight: TYPOGRAPHY.BOLD,
                fontSize: TYPOGRAPHY.BODY,
                textTransform: 'uppercase',
                letterSpacing: TYPOGRAPHY.NORMAL,
                transition: 'all 0.2s',
                backgroundColor: 'transparent',
                border: 'none',
                borderBottom: viewMode === mode ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                color: viewMode === mode ? FINCEPT.ORANGE : FINCEPT.GRAY,
                cursor: 'pointer',
              }}
              onMouseEnter={(e) => {
                if (viewMode !== mode) {
                  e.currentTarget.style.color = FINCEPT.WHITE;
                }
              }}
              onMouseLeave={(e) => {
                if (viewMode !== mode) {
                  e.currentTarget.style.color = FINCEPT.GRAY;
                }
              }}
            >
              {mode.toUpperCase()}
            </button>
          ))}
        </div>
      )}

      {/* Comparison Results */}
      {comparisonData && (
        <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.LARGE }}>
          {/* Valuation Assessment */}
          {(() => {
            // Calculate valuation status based on percentiles
            const peValue = comparisonData.target.valuation.peRatio;
            const pbValue = comparisonData.target.valuation.pbRatio;
            const psValue = comparisonData.target.valuation.psRatio;

            const pePeers = comparisonData.peers.map((p: CompanyMetrics) => p.valuation.peRatio).filter((v: number | undefined): v is number => v !== undefined);
            const pbPeers = comparisonData.peers.map((p: CompanyMetrics) => p.valuation.pbRatio).filter((v: number | undefined): v is number => v !== undefined);
            const psPeers = comparisonData.peers.map((p: CompanyMetrics) => p.valuation.psRatio).filter((v: number | undefined): v is number => v !== undefined);

            const calcPercentile = (val: number | undefined, peers: number[]) => {
              if (val === undefined || peers.length === 0) return 50;
              const allVals = [val, ...peers];
              const below = allVals.filter(v => v < val).length;
              return (below / allVals.length) * 100;
            };

            const pePercentile = calcPercentile(peValue, pePeers);
            const pbPercentile = calcPercentile(pbValue, pbPeers);
            const psPercentile = calcPercentile(psValue, psPeers);

            // Lower percentiles = cheaper valuation = better value
            let score = 0;
            if (pePercentile < 33) score++;
            else if (pePercentile > 66) score--;
            if (pbPercentile < 33) score++;
            else if (pbPercentile > 66) score--;
            if (psPercentile < 33) score++;
            else if (psPercentile > 66) score--;

            const getStatus = () => {
              if (score >= 2) return { label: 'UNDERVALUED', bgColor: `${FINCEPT.GREEN}15`, textColor: FINCEPT.GREEN, borderColor: FINCEPT.GREEN };
              if (score >= 1) return { label: 'POTENTIALLY UNDERVALUED', bgColor: `${FINCEPT.GREEN}10`, textColor: FINCEPT.GREEN, borderColor: FINCEPT.GREEN };
              if (score <= -2) return { label: 'OVERVALUED', bgColor: `${FINCEPT.RED}15`, textColor: FINCEPT.RED, borderColor: FINCEPT.RED };
              if (score <= -1) return { label: 'POTENTIALLY OVERVALUED', bgColor: `${FINCEPT.YELLOW}15`, textColor: FINCEPT.YELLOW, borderColor: FINCEPT.YELLOW };
              return { label: 'FAIRLY VALUED', bgColor: `${FINCEPT.CYAN}15`, textColor: FINCEPT.CYAN, borderColor: FINCEPT.CYAN };
            };

            const status = getStatus();

            return (
              <div style={{
                padding: SPACING.LARGE,
                border: `1px solid ${status.borderColor}`,
                backgroundColor: status.bgColor,
              }}>
                <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
                  <div>
                    <h3 style={{
                      fontSize: TYPOGRAPHY.SUBHEADING,
                      fontWeight: TYPOGRAPHY.BOLD,
                      color: FINCEPT.WHITE,
                      marginBottom: SPACING.TINY,
                      letterSpacing: TYPOGRAPHY.WIDE,
                      textTransform: 'uppercase',
                    }}>
                      VALUATION ASSESSMENT
                    </h3>
                    <p style={{
                      fontSize: TYPOGRAPHY.BODY,
                      color: FINCEPT.GRAY,
                    }}>
                      Based on P/E, P/B, and P/S percentile rankings vs peers
                    </p>
                  </div>
                  <div style={{ textAlign: 'right' }}>
                    <span style={{
                      display: 'inline-block',
                      padding: `${SPACING.SMALL} ${SPACING.LARGE}`,
                      fontWeight: TYPOGRAPHY.BOLD,
                      fontSize: TYPOGRAPHY.SUBHEADING,
                      color: status.textColor,
                      border: `1px solid ${status.borderColor}`,
                      letterSpacing: TYPOGRAPHY.WIDE,
                    }}>
                      {status.label}
                    </span>
                    <p style={{
                      fontSize: TYPOGRAPHY.SMALL,
                      color: FINCEPT.GRAY,
                      marginTop: SPACING.TINY,
                    }}>
                      P/E: {pePercentile.toFixed(0)}th • P/B: {pbPercentile.toFixed(0)}th • P/S: {psPercentile.toFixed(0)}th percentile
                    </p>
                  </div>
                </div>
              </div>
            );
          })()}

          {/* Sector Benchmarks Card */}
          <div style={{
            ...COMMON_STYLES.panel,
            padding: SPACING.XLARGE,
          }}>
            <div style={{
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
              marginBottom: SPACING.LARGE,
            }}>
              <h3 style={{
                fontSize: TYPOGRAPHY.SUBHEADING,
                fontWeight: TYPOGRAPHY.BOLD,
                color: FINCEPT.WHITE,
                letterSpacing: TYPOGRAPHY.WIDE,
                textTransform: 'uppercase',
              }}>
                SECTOR BENCHMARKS
              </h3>
              <span style={{
                fontSize: TYPOGRAPHY.SMALL,
                padding: `${SPACING.TINY} ${SPACING.SMALL}`,
                backgroundColor: `${FINCEPT.CYAN}20`,
                color: FINCEPT.CYAN,
                border: `1px solid ${FINCEPT.CYAN}`,
                fontWeight: TYPOGRAPHY.BOLD,
              }}>
                {comparisonData.benchmarks.sector}
              </span>
            </div>
            <div style={{
              display: 'grid',
              gridTemplateColumns: 'repeat(auto-fit, minmax(120px, 1fr))',
              gap: SPACING.LARGE,
            }}>
              <div style={{
                backgroundColor: FINCEPT.DARK_BG,
                border: BORDERS.STANDARD,
                padding: SPACING.MEDIUM,
              }}>
                <p style={{
                  ...COMMON_STYLES.dataLabel,
                  marginBottom: SPACING.TINY,
                }}>
                  MEDIAN P/E
                </p>
                <p style={{
                  fontSize: TYPOGRAPHY.LARGE,
                  fontWeight: TYPOGRAPHY.BOLD,
                  color: FINCEPT.WHITE,
                  fontFamily: TYPOGRAPHY.MONO,
                }}>
                  {comparisonData.benchmarks.medianPe?.toFixed(1) || 'N/A'}
                </p>
              </div>
              <div style={{
                backgroundColor: FINCEPT.DARK_BG,
                border: BORDERS.STANDARD,
                padding: SPACING.MEDIUM,
              }}>
                <p style={{
                  ...COMMON_STYLES.dataLabel,
                  marginBottom: SPACING.TINY,
                }}>
                  MEDIAN P/B
                </p>
                <p style={{
                  fontSize: TYPOGRAPHY.LARGE,
                  fontWeight: TYPOGRAPHY.BOLD,
                  color: FINCEPT.WHITE,
                  fontFamily: TYPOGRAPHY.MONO,
                }}>
                  {comparisonData.benchmarks.medianPb?.toFixed(1) || 'N/A'}
                </p>
              </div>
              <div style={{
                backgroundColor: FINCEPT.DARK_BG,
                border: BORDERS.STANDARD,
                padding: SPACING.MEDIUM,
              }}>
                <p style={{
                  ...COMMON_STYLES.dataLabel,
                  marginBottom: SPACING.TINY,
                }}>
                  MEDIAN ROE
                </p>
                <p style={{
                  fontSize: TYPOGRAPHY.LARGE,
                  fontWeight: TYPOGRAPHY.BOLD,
                  color: FINCEPT.WHITE,
                  fontFamily: TYPOGRAPHY.MONO,
                }}>
                  {comparisonData.benchmarks.medianRoe?.toFixed(1) || 'N/A'}%
                </p>
              </div>
              <div style={{
                backgroundColor: FINCEPT.DARK_BG,
                border: BORDERS.STANDARD,
                padding: SPACING.MEDIUM,
              }}>
                <p style={{
                  ...COMMON_STYLES.dataLabel,
                  marginBottom: SPACING.TINY,
                }}>
                  MEDIAN REV GROWTH
                </p>
                <p style={{
                  fontSize: TYPOGRAPHY.LARGE,
                  fontWeight: TYPOGRAPHY.BOLD,
                  color: FINCEPT.WHITE,
                  fontFamily: TYPOGRAPHY.MONO,
                }}>
                  {comparisonData.benchmarks.medianRevenueGrowth?.toFixed(1) || 'N/A'}%
                </p>
              </div>
              <div style={{
                backgroundColor: FINCEPT.DARK_BG,
                border: BORDERS.STANDARD,
                padding: SPACING.MEDIUM,
              }}>
                <p style={{
                  ...COMMON_STYLES.dataLabel,
                  marginBottom: SPACING.TINY,
                }}>
                  MEDIAN D/E
                </p>
                <p style={{
                  fontSize: TYPOGRAPHY.LARGE,
                  fontWeight: TYPOGRAPHY.BOLD,
                  color: FINCEPT.WHITE,
                  fontFamily: TYPOGRAPHY.MONO,
                }}>
                  {comparisonData.benchmarks.medianDebtToEquity?.toFixed(2) || 'N/A'}
                </p>
              </div>
            </div>
          </div>

          {viewMode === 'table' && (
            <ComparisonTable
              target={comparisonData.target}
              peers={comparisonData.peers}
              selectedMetrics={selectedMetrics}
              onToggleMetric={toggleMetric}
            />
          )}

          {viewMode === 'chart' && (
            <div style={{
              ...COMMON_STYLES.panel,
              padding: SPACING.XLARGE,
            }}>
              <div style={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
                marginBottom: SPACING.LARGE,
              }}>
                <h3 style={{
                  fontSize: TYPOGRAPHY.SUBHEADING,
                  fontWeight: TYPOGRAPHY.BOLD,
                  color: FINCEPT.WHITE,
                  letterSpacing: TYPOGRAPHY.WIDE,
                  textTransform: 'uppercase',
                }}>
                  METRIC COMPARISON - BAR CHARTS
                </h3>
                <p style={{
                  fontSize: TYPOGRAPHY.SMALL,
                  color: FINCEPT.GRAY,
                }}>
                  Compare key financial metrics across all companies
                </p>
              </div>
              <div style={{
                display: 'grid',
                gridTemplateColumns: 'repeat(auto-fit, minmax(400px, 1fr))',
                gap: SPACING.LARGE,
              }}>
                {selectedMetrics.map((metric) => {
                  const targetValue = getMetricValue(comparisonData.target, metric);
                  const chartData = [
                    {
                      name: comparisonData.target.symbol,
                      value: targetValue || 0,
                      fill: FINCEPT.ORANGE,
                    },
                    ...comparisonData.peers.map((peer: CompanyMetrics, index: number) => ({
                      name: peer.symbol,
                      value: getMetricValue(peer, metric) || 0,
                      fill: [FINCEPT.CYAN, FINCEPT.GREEN, FINCEPT.YELLOW, FINCEPT.PURPLE, FINCEPT.BLUE][index % 5],
                    })),
                  ];

                  return (
                    <div key={metric} style={{
                      backgroundColor: FINCEPT.DARK_BG,
                      border: BORDERS.STANDARD,
                      padding: SPACING.LARGE,
                    }}>
                      <h4 style={{
                        fontSize: TYPOGRAPHY.DEFAULT,
                        fontWeight: TYPOGRAPHY.BOLD,
                        color: FINCEPT.WHITE,
                        marginBottom: SPACING.MEDIUM,
                      }}>{metric}</h4>
                      <ResponsiveContainer width="100%" height={200}>
                        <ComposedChart data={chartData} layout="vertical">
                          <CartesianGrid strokeDasharray="3 3" stroke={FINCEPT.BORDER} />
                          <XAxis
                            type="number"
                            stroke={FINCEPT.GRAY}
                            tick={{ fill: FINCEPT.GRAY, fontSize: 11 }}
                          />
                          <YAxis
                            type="category"
                            dataKey="name"
                            stroke={FINCEPT.GRAY}
                            tick={{ fill: FINCEPT.GRAY, fontSize: 11 }}
                            width={80}
                          />
                          <Tooltip
                            contentStyle={{
                              backgroundColor: FINCEPT.PANEL_BG,
                              border: `1px solid ${FINCEPT.BORDER}`,
                              borderRadius: '2px',
                              color: FINCEPT.WHITE,
                            }}
                            formatter={(value: any) => [value.toFixed(2), metric]}
                          />
                          <Bar dataKey="value" radius={[0, 2, 2, 0]} />
                        </ComposedChart>
                      </ResponsiveContainer>
                    </div>
                  );
                })}
              </div>
            </div>
          )}

          {viewMode === 'radar' && (
            <div style={{
              ...COMMON_STYLES.panel,
              padding: SPACING.XLARGE,
            }}>
              <div style={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
                marginBottom: SPACING.LARGE,
              }}>
                <h3 style={{
                  fontSize: TYPOGRAPHY.SUBHEADING,
                  fontWeight: TYPOGRAPHY.BOLD,
                  color: FINCEPT.WHITE,
                  letterSpacing: TYPOGRAPHY.WIDE,
                  textTransform: 'uppercase',
                }}>
                  METRIC COMPARISON
                </h3>
                <p style={{
                  fontSize: TYPOGRAPHY.SMALL,
                  color: FINCEPT.GRAY,
                }}>
                  Values normalized 0-100 for visualization
                </p>
              </div>
              <ResponsiveContainer width="100%" height={400}>
                <RadarChart data={radarData}>
                  <PolarGrid stroke={FINCEPT.BORDER} />
                  <PolarAngleAxis
                    dataKey="metric"
                    stroke={FINCEPT.GRAY}
                    tick={{ fill: FINCEPT.GRAY, fontSize: 12 }}
                  />
                  <PolarRadiusAxis
                    stroke={FINCEPT.GRAY}
                    tick={{ fill: FINCEPT.MUTED, fontSize: 10 }}
                    domain={[0, 100]}
                  />
                  <Radar
                    name={comparisonData.target.symbol}
                    dataKey="target"
                    stroke={FINCEPT.ORANGE}
                    fill={FINCEPT.ORANGE}
                    fillOpacity={0.3}
                    strokeWidth={2}
                  />
                  {comparisonData.peers.map((peer: CompanyMetrics, index: number) => (
                    <Radar
                      key={peer.symbol}
                      name={peer.symbol}
                      dataKey={`peer${index + 1}`}
                      stroke={[FINCEPT.CYAN, FINCEPT.GREEN, FINCEPT.YELLOW, FINCEPT.PURPLE, FINCEPT.BLUE][index % 5]}
                      fill={[FINCEPT.CYAN, FINCEPT.GREEN, FINCEPT.YELLOW, FINCEPT.PURPLE, FINCEPT.BLUE][index % 5]}
                      fillOpacity={0.1}
                      strokeWidth={2}
                    />
                  ))}
                  <Legend wrapperStyle={{ paddingTop: '20px', color: FINCEPT.WHITE }} />
                  <Tooltip
                    contentStyle={{
                      backgroundColor: FINCEPT.PANEL_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      borderRadius: '2px',
                      color: FINCEPT.WHITE,
                    }}
                    formatter={(value: any, name: string, props: any) => {
                      // Show original value in tooltip
                      const originalKey = `${name}Original`;
                      const originalValue = props.payload[originalKey];
                      return [originalValue?.toFixed(2) || value.toFixed(2), name];
                    }}
                  />
                </RadarChart>
              </ResponsiveContainer>
            </div>
          )}

          {/* Percentile Rankings Section */}
          <div style={{
            ...COMMON_STYLES.panel,
            padding: SPACING.XLARGE,
            marginTop: SPACING.LARGE,
          }}>
            <div style={{
              display: 'flex',
              alignItems: 'flex-start',
              justifyContent: 'space-between',
              marginBottom: SPACING.LARGE,
            }}>
              <div>
                <h3 style={{
                  fontSize: TYPOGRAPHY.SUBHEADING,
                  fontWeight: TYPOGRAPHY.BOLD,
                  color: FINCEPT.WHITE,
                  marginBottom: SPACING.SMALL,
                  letterSpacing: TYPOGRAPHY.WIDE,
                  textTransform: 'uppercase',
                }}>
                  PERCENTILE RANKINGS
                </h3>
                <p style={{
                  fontSize: TYPOGRAPHY.BODY,
                  color: FINCEPT.GRAY,
                }}>
                  {comparisonData.target.symbol}'s position within selected peer group
                </p>
              </div>
              {comparisonData.peers.length < 5 && (
                <div style={{
                  backgroundColor: `${FINCEPT.YELLOW}15`,
                  border: `1px solid ${FINCEPT.YELLOW}`,
                  padding: `${SPACING.SMALL} ${SPACING.MEDIUM}`,
                  maxWidth: '400px',
                }}>
                  <div style={{ display: 'flex', alignItems: 'flex-start', gap: SPACING.SMALL }}>
                    <svg style={{ width: '16px', height: '16px', color: FINCEPT.YELLOW, flexShrink: 0, marginTop: '2px' }} fill="none" viewBox="0 0 24 24" stroke="currentColor">
                      <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 9v2m0 4h.01m-6.938 4h13.856c1.54 0 2.502-1.667 1.732-3L13.732 4c-.77-1.333-2.694-1.333-3.464 0L3.34 16c-.77 1.333.192 3 1.732 3z" />
                    </svg>
                    <div style={{ fontSize: TYPOGRAPHY.SMALL, color: FINCEPT.YELLOW }}>
                      <p style={{ fontWeight: TYPOGRAPHY.BOLD, marginBottom: SPACING.TINY }}>LIMITED PEER SAMPLE</p>
                      <p>Add more peers (5-10 recommended) for statistically significant percentile rankings. With only {comparisonData.peers.length} {comparisonData.peers.length === 1 ? 'peer' : 'peers'}, percentiles will only show 0%, 50%, or 100%.</p>
                    </div>
                  </div>
                </div>
              )}
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.LARGE }}>
              {selectedMetrics.map((metric) => {
                const targetValue = getMetricValue(comparisonData.target, metric);
                const peerValues = comparisonData.peers.map((p: CompanyMetrics) => getMetricValue(p, metric)).filter((v: number | undefined): v is number => v !== undefined);

                if (targetValue === undefined || peerValues.length === 0) return null;

                // Calculate percentile and statistics
                const allValues = [targetValue, ...peerValues];
                const sortedValues = [...allValues].sort((a, b) => a - b);
                const countBelow = allValues.filter(v => v < targetValue).length;
                const percentile = (countBelow / allValues.length) * 100;

                // Calculate mean, median, and std dev
                const mean = allValues.reduce((sum, v) => sum + v, 0) / allValues.length;
                const median = sortedValues[Math.floor(sortedValues.length / 2)];
                const variance = allValues.reduce((sum, v) => sum + Math.pow(v - mean, 2), 0) / allValues.length;
                const stdDev = Math.sqrt(variance);
                const zScore = stdDev > 0 ? (targetValue - mean) / stdDev : 0;

                const ranking = {
                  metricName: metric,
                  value: targetValue,
                  percentile: percentile,
                  zScore: zScore,
                  peerMedian: median,
                  peerMean: mean,
                  peerStdDev: stdDev,
                };

                return (
                  <PercentileRankMini
                    key={metric}
                    ranking={ranking}
                  />
                );
              })}
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

// Comparison Table Component
interface ComparisonTableProps {
  target: CompanyMetrics;
  peers: CompanyMetrics[];
  selectedMetrics: string[];
  onToggleMetric: (metric: string) => void;
}

const ComparisonTable: React.FC<ComparisonTableProps> = ({
  target,
  peers,
  selectedMetrics,
  onToggleMetric,
}) => {
  const allMetrics = getAllMetricNames(target);

  return (
    <div style={{
      ...COMMON_STYLES.panel,
      padding: 0,
      overflow: 'hidden',
    }}>
      {/* Metric Selector */}
      <div style={{
        padding: SPACING.LARGE,
        borderBottom: BORDERS.STANDARD,
      }}>
        <h4 style={{
          ...COMMON_STYLES.dataLabel,
          marginBottom: SPACING.MEDIUM,
        }}>SELECT METRICS</h4>
        <div style={{ display: 'flex', flexWrap: 'wrap', gap: SPACING.SMALL }}>
          {allMetrics.map((metric) => (
            <button
              key={metric}
              onClick={() => onToggleMetric(metric)}
              style={{
                padding: `${SPACING.SMALL} ${SPACING.MEDIUM}`,
                fontSize: TYPOGRAPHY.BODY,
                backgroundColor: selectedMetrics.includes(metric) ? FINCEPT.ORANGE : FINCEPT.DARK_BG,
                border: `1px solid ${selectedMetrics.includes(metric) ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                color: selectedMetrics.includes(metric) ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                cursor: 'pointer',
                transition: 'all 0.2s',
                fontWeight: selectedMetrics.includes(metric) ? TYPOGRAPHY.BOLD : TYPOGRAPHY.REGULAR,
              }}
            >
              {metric}
            </button>
          ))}
        </div>
      </div>

      {/* Table */}
      <div style={{ overflowX: 'auto' }}>
        <table style={{ width: '100%', borderCollapse: 'collapse' }}>
          <thead>
            <tr style={{ backgroundColor: FINCEPT.HEADER_BG }}>
              <th style={{
                ...COMMON_STYLES.tableHeader,
                padding: SPACING.MEDIUM,
              }}>
                METRIC
              </th>
              <th style={{
                ...COMMON_STYLES.tableHeader,
                padding: SPACING.MEDIUM,
                textAlign: 'right',
                color: FINCEPT.ORANGE,
              }}>
                {target.symbol} (TARGET)
              </th>
              {peers.map((peer) => (
                <th
                  key={peer.symbol}
                  style={{
                    ...COMMON_STYLES.tableHeader,
                    padding: SPACING.MEDIUM,
                    textAlign: 'right',
                  }}
                >
                  {peer.symbol}
                </th>
              ))}
            </tr>
          </thead>
          <tbody>
            {selectedMetrics.map((metric, idx) => {
              const targetValue = getMetricValue(target, metric);
              const peerValues = peers.map((p) => getMetricValue(p, metric));
              const allValues = [targetValue, ...peerValues].filter((v) => v !== undefined) as number[];
              const max = Math.max(...allValues);
              const min = Math.min(...allValues);

              return (
                <tr
                  key={metric}
                  style={{
                    borderBottom: BORDERS.STANDARD,
                    backgroundColor: idx % 2 === 0 ? 'transparent' : 'rgba(255,255,255,0.02)',
                    transition: 'background-color 0.2s',
                  }}
                >
                  <td style={{
                    padding: SPACING.MEDIUM,
                    whiteSpace: 'nowrap',
                    fontSize: TYPOGRAPHY.BODY,
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: FINCEPT.WHITE,
                  }}>
                    {metric}
                  </td>
                  <td style={{
                    padding: SPACING.MEDIUM,
                    whiteSpace: 'nowrap',
                    fontSize: TYPOGRAPHY.BODY,
                    textAlign: 'right',
                    fontWeight: TYPOGRAPHY.SEMIBOLD,
                    fontFamily: TYPOGRAPHY.MONO,
                    color: targetValue === max ? FINCEPT.GREEN : targetValue === min ? FINCEPT.RED : FINCEPT.CYAN,
                  }}>
                    {formatMetricValue(targetValue, 'ratio')}
                  </td>
                  {peerValues.map((value, index) => (
                    <td key={index} style={{
                      padding: SPACING.MEDIUM,
                      whiteSpace: 'nowrap',
                      fontSize: TYPOGRAPHY.BODY,
                      textAlign: 'right',
                      fontFamily: TYPOGRAPHY.MONO,
                      color: FINCEPT.GRAY,
                    }}>
                      {formatMetricValue(value, 'ratio')}
                    </td>
                  ))}
                </tr>
              );
            })}
          </tbody>
        </table>
      </div>
    </div>
  );
};
