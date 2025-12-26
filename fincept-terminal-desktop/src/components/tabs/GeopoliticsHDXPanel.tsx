import React, { useState, useEffect } from 'react';
import { BarChart, Bar, LineChart, Line, PieChart, Pie, Cell, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer } from 'recharts';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import * as HDXService from '@/services/hdxService';
import {
  HDXDataset,
  HDXDatasetDetails,
  HDXSearchResult,
  HDXConflictResult,
  HDXQueries
} from '@/services/hdxService';
import GeopoliticsRelationshipMap from './GeopoliticsRelationshipMap';

interface HDXPanelProps {
  selectedRegion?: string;
}

const GeopoliticsHDXPanel: React.FC<HDXPanelProps> = ({ selectedRegion = '' }) => {
  const { colors, fontSize, fontFamily } = useTerminalTheme();

  // State
  const [loading, setLoading] = useState(true);
  const [activeView, setActiveView] = useState<'overview' | 'map' | 'conflicts' | 'humanitarian' | 'datasets' | 'explorer'>('overview');
  const [searchQuery, setSearchQuery] = useState('');
  const [selectedDataset, setSelectedDataset] = useState<HDXDatasetDetails | null>(null);
  const [selectedCountry, setSelectedCountry] = useState<string>('');
  const [selectedTopic, setSelectedTopic] = useState<string>('');
  const [countries, setCountries] = useState<Array<{ id: string; name: string; title: string; package_count: number }>>([]);
  const [topics, setTopics] = useState<Array<{ id: string; name: string }>>([]);

  // Data states
  const [conflictData, setConflictData] = useState<HDXSearchResult | null>(null);
  const [humanitarianData, setHumanitarianData] = useState<HDXSearchResult | null>(null);
  const [displacementData, setDisplacementData] = useState<HDXSearchResult | null>(null);
  const [foodSecurityData, setFoodSecurityData] = useState<HDXSearchResult | null>(null);
  const [searchResults, setSearchResults] = useState<HDXDataset[]>([]);
  const [organizations, setOrganizations] = useState<Array<{ id: string; name: string; title: string; description: string; package_count: number }>>([]);

  // Conflict regions tracking
  const [conflictRegions, setConflictRegions] = useState<Map<string, HDXConflictResult>>(new Map());

  // Critical regions to monitor
  const criticalRegions = [
    'Ukraine', 'Gaza', 'Sudan', 'Yemen', 'Syria', 'Afghanistan',
    'Myanmar', 'Ethiopia', 'Haiti', 'Somalia', 'Venezuela'
  ];

  useEffect(() => {
    loadHDXData();
  }, []);

  const loadHDXData = async () => {
    setLoading(true);
    try {
      // Load overview data in parallel
      const [conflicts, humanitarian, displacement, foodSec, orgs, countryList, topicList] = await Promise.all([
        HDXService.searchDatasets(HDXQueries.GLOBAL_CONFLICTS, 20),
        HDXService.searchDatasets(HDXQueries.HUMANITARIAN_CRISIS, 20),
        HDXService.searchDatasets(HDXQueries.DISPLACEMENT, 20),
        HDXService.searchDatasets(HDXQueries.FOOD_SECURITY, 20),
        HDXService.listOrganizations(30),
        HDXService.listCountries(300),
        HDXService.listTopics(100)
      ]);

      setConflictData(conflicts);
      setHumanitarianData(humanitarian);
      setDisplacementData(displacement);
      setFoodSecurityData(foodSec);
      setOrganizations(orgs);
      setCountries(countryList.sort((a, b) => b.package_count - a.package_count));
      setTopics(topicList);

      // Load conflict data for critical regions
      const regionData = await HDXService.getConflictDataByCountries(criticalRegions);
      setConflictRegions(regionData);
    } catch (error) {
      console.error('Failed to load HDX data:', error);
    } finally {
      setLoading(false);
    }
  };

  const handleSearch = async () => {
    if (!searchQuery.trim()) return;

    setLoading(true);
    try {
      const results = await HDXService.searchDatasets(searchQuery, 50);
      setSearchResults(results.datasets);
      setActiveView('datasets');
    } catch (error) {
      console.error('Search error:', error);
    } finally {
      setLoading(false);
    }
  };

  const handleDatasetClick = async (datasetId: string) => {
    setLoading(true);
    try {
      const dataset = await HDXService.getDataset(datasetId);
      setSelectedDataset(dataset);
    } catch (error) {
      console.error('Failed to load dataset:', error);
    } finally {
      setLoading(false);
    }
  };

  // Calculate statistics
  const getConflictStats = () => {
    const activeConflicts = conflictRegions.size;
    const totalDatasets = conflictData?.count || 0;
    const affectedCountries = new Set<string>();

    conflictData?.datasets.forEach(ds => {
      ds.tags.forEach(tag => {
        if (tag.includes('conflict') || tag.includes('violence')) {
          affectedCountries.add(ds.organization);
        }
      });
    });

    return {
      activeConflicts,
      totalDatasets,
      dataProviders: affectedCountries.size
    };
  };

  const getCrisisBreakdown = () => {
    const crisisTypes = [
      { name: 'Armed Conflict', value: conflictData?.count || 0, color: colors.alert },
      { name: 'Humanitarian', value: humanitarianData?.count || 0, color: colors.warning },
      { name: 'Displacement', value: displacementData?.count || 0, color: colors.info },
      { name: 'Food Insecurity', value: foodSecurityData?.count || 0, color: colors.secondary }
    ];
    return crisisTypes;
  };

  const getRegionalConflictData = () => {
    return Array.from(conflictRegions.entries()).map(([country, data]) => ({
      country,
      datasets: data.count,
      severity: data.count > 10 ? 'Critical' : data.count > 5 ? 'High' : 'Moderate'
    })).sort((a, b) => b.datasets - a.datasets);
  };

  const stats = getConflictStats();
  const crisisBreakdown = getCrisisBreakdown();
  const regionalData = getRegionalConflictData();

  // Render functions for different views
  const renderOverview = () => (
    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', height: '100%' }}>
      {/* Left Column */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
        {/* Global Statistics */}
        <div style={{
          backgroundColor: colors.panel,
          border: `1px solid ${colors.textMuted}`,
          padding: '12px',
          borderRadius: '4px'
        }}>
          <div style={{ color: colors.primary, fontSize: '12px', fontWeight: 'bold', marginBottom: '12px' }}>
            GLOBAL CRISIS OVERVIEW
          </div>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '12px', fontSize: '11px' }}>
            <div>
              <div style={{ color: colors.textMuted }}>Active Conflicts</div>
              <div style={{ color: colors.alert, fontSize: '24px', fontWeight: 'bold' }}>{stats.activeConflicts}</div>
            </div>
            <div>
              <div style={{ color: colors.textMuted }}>Total Datasets</div>
              <div style={{ color: colors.secondary, fontSize: '24px', fontWeight: 'bold' }}>{stats.totalDatasets}</div>
            </div>
            <div>
              <div style={{ color: colors.textMuted }}>Data Providers</div>
              <div style={{ color: colors.info, fontSize: '24px', fontWeight: 'bold' }}>{organizations.length}</div>
            </div>
          </div>
        </div>

        {/* Crisis Type Breakdown */}
        <div style={{
          backgroundColor: colors.panel,
          border: `1px solid ${colors.textMuted}`,
          padding: '12px',
          borderRadius: '4px',
          flex: 1
        }}>
          <div style={{ color: colors.primary, fontSize: '12px', fontWeight: 'bold', marginBottom: '8px' }}>
            CRISIS TYPE BREAKDOWN
          </div>
          <ResponsiveContainer width="100%" height={250}>
            <PieChart>
              <Pie
                data={crisisBreakdown}
                cx="50%"
                cy="50%"
                labelLine={false}
                label={(entry) => `${entry.name}: ${entry.value}`}
                outerRadius={80}
                fill="#8884d8"
                dataKey="value"
              >
                {crisisBreakdown.map((entry, index) => (
                  <Cell key={`cell-${index}`} fill={entry.color} />
                ))}
              </Pie>
              <Tooltip
                contentStyle={{
                  backgroundColor: colors.background,
                  border: `1px solid ${colors.textMuted}`,
                  color: colors.text,
                  fontSize: '10px'
                }}
              />
            </PieChart>
          </ResponsiveContainer>
        </div>
      </div>

      {/* Right Column */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
        {/* Regional Conflict Intensity */}
        <div style={{
          backgroundColor: colors.panel,
          border: `1px solid ${colors.textMuted}`,
          padding: '12px',
          borderRadius: '4px',
          flex: 1
        }}>
          <div style={{ color: colors.primary, fontSize: '12px', fontWeight: 'bold', marginBottom: '8px' }}>
            REGIONAL CONFLICT INTENSITY
          </div>
          <ResponsiveContainer width="100%" height={300}>
            <BarChart data={regionalData} layout="vertical">
              <CartesianGrid strokeDasharray="3 3" stroke={colors.textMuted} opacity={0.3} />
              <XAxis type="number" stroke={colors.text} fontSize={9} />
              <YAxis dataKey="country" type="category" stroke={colors.text} fontSize={9} width={80} />
              <Tooltip
                contentStyle={{
                  backgroundColor: colors.background,
                  border: `1px solid ${colors.textMuted}`,
                  color: colors.text,
                  fontSize: '10px'
                }}
              />
              <Bar dataKey="datasets" fill={colors.alert} />
            </BarChart>
          </ResponsiveContainer>
        </div>

        {/* Top Organizations */}
        <div style={{
          backgroundColor: colors.panel,
          border: `1px solid ${colors.textMuted}`,
          padding: '12px',
          borderRadius: '4px',
          maxHeight: '250px',
          overflow: 'auto'
        }}>
          <div style={{ color: colors.primary, fontSize: '12px', fontWeight: 'bold', marginBottom: '8px' }}>
            TOP DATA PROVIDERS
          </div>
          <div style={{ fontSize: '10px' }}>
            {organizations.slice(0, 15).map((org, index) => (
              <div key={index} style={{
                padding: '6px',
                borderBottom: `1px solid ${colors.textMuted}`,
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center'
              }}>
                <div style={{ flex: 1 }}>
                  <div style={{ color: colors.text, fontWeight: 'bold' }}>{org.title}</div>
                  <div style={{ color: colors.textMuted, fontSize: '8px', marginTop: '2px' }}>
                    {org.package_count} datasets
                  </div>
                </div>
                <span style={{
                  color: colors.secondary,
                  fontSize: '9px',
                  backgroundColor: 'rgba(255,255,255,0.1)',
                  padding: '2px 6px',
                  borderRadius: '2px'
                }}>
                  #{index + 1}
                </span>
              </div>
            ))}
          </div>
        </div>
      </div>
    </div>
  );

  const renderConflicts = () => (
    <div style={{ height: '100%', overflow: 'auto' }}>
      <div style={{
        backgroundColor: colors.panel,
        border: `1px solid ${colors.textMuted}`,
        padding: '12px',
        borderRadius: '4px',
        marginBottom: '12px'
      }}>
        <div style={{ color: colors.primary, fontSize: '12px', fontWeight: 'bold', marginBottom: '8px' }}>
          CONFLICT DATASETS ({conflictData?.count || 0})
        </div>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '8px' }}>
          {conflictData?.datasets.map((dataset, index) => (
            <div
              key={index}
              onClick={() => handleDatasetClick(dataset.id)}
              style={{
                padding: '10px',
                backgroundColor: 'rgba(255,255,255,0.03)',
                border: `1px solid ${colors.textMuted}`,
                borderRadius: '4px',
                cursor: 'pointer',
                transition: 'all 0.2s'
              }}
              onMouseEnter={(e) => e.currentTarget.style.backgroundColor = 'rgba(255,255,255,0.08)'}
              onMouseLeave={(e) => e.currentTarget.style.backgroundColor = 'rgba(255,255,255,0.03)'}
            >
              <div style={{ color: colors.secondary, fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>
                {dataset.title}
              </div>
              <div style={{ color: colors.textMuted, fontSize: '9px', marginBottom: '4px' }}>
                {dataset.organization}
              </div>
              <div style={{ color: colors.text, fontSize: '9px', marginBottom: '6px' }}>
                {dataset.notes?.substring(0, 100)}...
              </div>
              <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap' }}>
                {dataset.tags.slice(0, 3).map((tag, i) => (
                  <span key={i} style={{
                    fontSize: '8px',
                    backgroundColor: colors.background,
                    color: colors.info,
                    padding: '2px 6px',
                    borderRadius: '2px'
                  }}>
                    {tag}
                  </span>
                ))}
              </div>
              <div style={{ color: colors.textMuted, fontSize: '8px', marginTop: '4px' }}>
                {dataset.dataset_date} • {dataset.num_resources || 0} resources
              </div>
            </div>
          ))}
        </div>
      </div>
    </div>
  );

  const renderHumanitarian = () => (
    <div style={{ height: '100%', overflow: 'auto' }}>
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
        {/* Humanitarian Crisis */}
        <div style={{
          backgroundColor: colors.panel,
          border: `1px solid ${colors.textMuted}`,
          padding: '12px',
          borderRadius: '4px'
        }}>
          <div style={{ color: colors.warning, fontSize: '12px', fontWeight: 'bold', marginBottom: '8px' }}>
            HUMANITARIAN CRISES ({humanitarianData?.count || 0})
          </div>
          <div style={{ maxHeight: '400px', overflow: 'auto' }}>
            {humanitarianData?.datasets.map((dataset, index) => (
              <div
                key={index}
                onClick={() => handleDatasetClick(dataset.id)}
                style={{
                  padding: '8px',
                  borderBottom: `1px solid ${colors.textMuted}`,
                  cursor: 'pointer',
                  fontSize: '10px'
                }}
                onMouseEnter={(e) => e.currentTarget.style.backgroundColor = 'rgba(255,255,255,0.05)'}
                onMouseLeave={(e) => e.currentTarget.style.backgroundColor = 'transparent'}
              >
                <div style={{ color: colors.text, fontWeight: 'bold' }}>{dataset.title}</div>
                <div style={{ color: colors.textMuted, fontSize: '9px' }}>{dataset.organization}</div>
              </div>
            ))}
          </div>
        </div>

        {/* Displacement & Food Security */}
        <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
          <div style={{
            backgroundColor: colors.panel,
            border: `1px solid ${colors.textMuted}`,
            padding: '12px',
            borderRadius: '4px'
          }}>
            <div style={{ color: colors.info, fontSize: '12px', fontWeight: 'bold', marginBottom: '8px' }}>
              DISPLACEMENT & REFUGEES ({displacementData?.count || 0})
            </div>
            <div style={{ maxHeight: '180px', overflow: 'auto', fontSize: '10px' }}>
              {displacementData?.datasets.slice(0, 8).map((dataset, index) => (
                <div
                  key={index}
                  onClick={() => handleDatasetClick(dataset.id)}
                  style={{
                    padding: '6px',
                    borderBottom: `1px solid ${colors.textMuted}`,
                    cursor: 'pointer'
                  }}
                  onMouseEnter={(e) => e.currentTarget.style.backgroundColor = 'rgba(255,255,255,0.05)'}
                  onMouseLeave={(e) => e.currentTarget.style.backgroundColor = 'transparent'}
                >
                  <div style={{ color: colors.text, fontWeight: 'bold' }}>{dataset.title}</div>
                </div>
              ))}
            </div>
          </div>

          <div style={{
            backgroundColor: colors.panel,
            border: `1px solid ${colors.textMuted}`,
            padding: '12px',
            borderRadius: '4px'
          }}>
            <div style={{ color: colors.secondary, fontSize: '12px', fontWeight: 'bold', marginBottom: '8px' }}>
              FOOD SECURITY ({foodSecurityData?.count || 0})
            </div>
            <div style={{ maxHeight: '180px', overflow: 'auto', fontSize: '10px' }}>
              {foodSecurityData?.datasets.slice(0, 8).map((dataset, index) => (
                <div
                  key={index}
                  onClick={() => handleDatasetClick(dataset.id)}
                  style={{
                    padding: '6px',
                    borderBottom: `1px solid ${colors.textMuted}`,
                    cursor: 'pointer'
                  }}
                  onMouseEnter={(e) => e.currentTarget.style.backgroundColor = 'rgba(255,255,255,0.05)'}
                  onMouseLeave={(e) => e.currentTarget.style.backgroundColor = 'transparent'}
                >
                  <div style={{ color: colors.text, fontWeight: 'bold' }}>{dataset.title}</div>
                </div>
              ))}
            </div>
          </div>
        </div>
      </div>
    </div>
  );

  const renderDatasets = () => (
    <div style={{ height: '100%', overflow: 'auto' }}>
      <div style={{
        backgroundColor: colors.panel,
        border: `1px solid ${colors.textMuted}`,
        padding: '12px',
        borderRadius: '4px'
      }}>
        <div style={{ color: colors.primary, fontSize: '12px', fontWeight: 'bold', marginBottom: '8px' }}>
          DATASET EXPLORER ({searchResults.length} results)
        </div>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '8px' }}>
          {searchResults.map((dataset, index) => (
            <div
              key={index}
              onClick={() => handleDatasetClick(dataset.id)}
              style={{
                padding: '10px',
                backgroundColor: 'rgba(255,255,255,0.03)',
                border: `1px solid ${colors.textMuted}`,
                borderRadius: '4px',
                cursor: 'pointer',
                fontSize: '10px'
              }}
              onMouseEnter={(e) => e.currentTarget.style.backgroundColor = 'rgba(255,255,255,0.08)'}
              onMouseLeave={(e) => e.currentTarget.style.backgroundColor = 'rgba(255,255,255,0.03)'}
            >
              <div style={{ color: colors.secondary, fontWeight: 'bold', marginBottom: '4px' }}>
                {dataset.title}
              </div>
              <div style={{ color: colors.textMuted, fontSize: '9px' }}>
                {dataset.organization}
              </div>
            </div>
          ))}
        </div>
      </div>
    </div>
  );

  const renderExplorer = () => (
    <div style={{ height: '100%', display: 'flex', gap: '12px' }}>
      {/* Filter Panel */}
      <div style={{
        width: '250px',
        backgroundColor: colors.panel,
        border: `1px solid ${colors.textMuted}`,
        padding: '12px',
        borderRadius: '4px',
        display: 'flex',
        flexDirection: 'column',
        gap: '12px'
      }}>
        <div>
          <div style={{ color: colors.primary, fontSize: '11px', fontWeight: 'bold', marginBottom: '6px' }}>
            FILTER BY COUNTRY
          </div>
          <select
            value={selectedCountry}
            onChange={(e) => setSelectedCountry(e.target.value)}
            style={{
              width: '100%',
              backgroundColor: colors.background,
              color: colors.text,
              border: `1px solid ${colors.textMuted}`,
              padding: '6px',
              fontSize: '10px',
              borderRadius: '2px'
            }}
          >
            <option value="">All Countries</option>
            {countries.slice(0, 50).map((country) => (
              <option key={country.id} value={country.name}>
                {country.title} ({country.package_count})
              </option>
            ))}
          </select>
        </div>

        <div>
          <div style={{ color: colors.primary, fontSize: '11px', fontWeight: 'bold', marginBottom: '6px' }}>
            FILTER BY TOPIC
          </div>
          <select
            value={selectedTopic}
            onChange={(e) => setSelectedTopic(e.target.value)}
            style={{
              width: '100%',
              backgroundColor: colors.background,
              color: colors.text,
              border: `1px solid ${colors.textMuted}`,
              padding: '6px',
              fontSize: '10px',
              borderRadius: '2px'
            }}
          >
            <option value="">All Topics</option>
            {topics.slice(0, 50).map((topic) => (
              <option key={topic.id} value={topic.name}>
                {topic.name}
              </option>
            ))}
          </select>
        </div>

        <button
          onClick={async () => {
            setLoading(true);
            try {
              let results;
              if (selectedCountry && selectedTopic) {
                results = await HDXService.advancedSearch({
                  groups: selectedCountry,
                  vocab_Topics: selectedTopic
                }, 50);
              } else if (selectedCountry) {
                results = await HDXService.searchByCountry(selectedCountry, 50);
              } else if (selectedTopic) {
                results = await HDXService.searchByTopic(selectedTopic, 50);
              } else {
                results = await HDXService.searchDatasets('*:*', 50);
              }
              setSearchResults(results.datasets);
            } catch (error) {
              console.error('Filter error:', error);
            } finally {
              setLoading(false);
            }
          }}
          style={{
            backgroundColor: colors.secondary,
            color: colors.background,
            border: 'none',
            padding: '8px',
            fontSize: '10px',
            cursor: 'pointer',
            borderRadius: '2px',
            fontWeight: 'bold'
          }}
        >
          APPLY FILTERS
        </button>

        <button
          onClick={() => {
            setSelectedCountry('');
            setSelectedTopic('');
            setSearchResults([]);
          }}
          style={{
            backgroundColor: colors.textMuted,
            color: colors.background,
            border: 'none',
            padding: '6px',
            fontSize: '9px',
            cursor: 'pointer',
            borderRadius: '2px',
            fontWeight: 'bold'
          }}
        >
          CLEAR FILTERS
        </button>
      </div>

      {/* Results Panel */}
      <div style={{ flex: 1, overflow: 'auto' }}>
        {renderDatasets()}
      </div>
    </div>
  );

  if (loading && !conflictData) {
    return (
      <div style={{
        height: '100%',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        color: colors.text,
        fontFamily
      }}>
        Loading HDX Data...
      </div>
    );
  }

  return (
    <div style={{
      height: '100%',
      backgroundColor: colors.background,
      color: colors.text,
      fontFamily,
      display: 'flex',
      flexDirection: 'column',
      fontSize: fontSize.body
    }}>
      {/* Header */}
      <div style={{
        backgroundColor: colors.panel,
        borderBottom: `1px solid ${colors.textMuted}`,
        padding: '8px 12px',
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center'
      }}>
        <div style={{ display: 'flex', gap: '12px', alignItems: 'center' }}>
          <span style={{ color: colors.primary, fontWeight: 'bold', fontSize: '13px' }}>
            HDX HUMANITARIAN DATA
          </span>
          <div style={{ display: 'flex', gap: '6px' }}>
            {['overview', 'map', 'conflicts', 'humanitarian', 'explorer', 'datasets'].map((view) => (
              <button
                key={view}
                onClick={() => setActiveView(view as any)}
                style={{
                  backgroundColor: activeView === view ? colors.primary : colors.background,
                  color: activeView === view ? colors.background : colors.text,
                  border: `1px solid ${colors.textMuted}`,
                  padding: '4px 12px',
                  fontSize: '10px',
                  cursor: 'pointer',
                  borderRadius: '2px',
                  fontWeight: 'bold',
                  textTransform: 'uppercase'
                }}
              >
                {view}
              </button>
            ))}
          </div>
        </div>

        {/* Search Bar */}
        <div style={{ display: 'flex', gap: '6px' }}>
          <input
            type="text"
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            onKeyPress={(e) => e.key === 'Enter' && handleSearch()}
            placeholder="Search HDX datasets..."
            style={{
              backgroundColor: colors.background,
              color: colors.text,
              border: `1px solid ${colors.textMuted}`,
              padding: '4px 8px',
              fontSize: '10px',
              borderRadius: '2px',
              width: '200px'
            }}
          />
          <button
            onClick={handleSearch}
            disabled={loading}
            style={{
              backgroundColor: colors.secondary,
              color: colors.background,
              border: 'none',
              padding: '4px 12px',
              fontSize: '10px',
              cursor: loading ? 'not-allowed' : 'pointer',
              borderRadius: '2px',
              fontWeight: 'bold'
            }}
          >
            SEARCH
          </button>
          <button
            onClick={loadHDXData}
            disabled={loading}
            style={{
              backgroundColor: colors.info,
              color: colors.background,
              border: 'none',
              padding: '4px 12px',
              fontSize: '10px',
              cursor: loading ? 'not-allowed' : 'pointer',
              borderRadius: '2px',
              fontWeight: 'bold'
            }}
          >
            ↻ REFRESH
          </button>
        </div>
      </div>

      {/* Content */}
      <div style={{ flex: 1, padding: '12px', overflow: 'auto' }}>
        {activeView === 'overview' && renderOverview()}
        {activeView === 'map' && <GeopoliticsRelationshipMap width={1200} height={700} />}
        {activeView === 'conflicts' && renderConflicts()}
        {activeView === 'humanitarian' && renderHumanitarian()}
        {activeView === 'explorer' && renderExplorer()}
        {activeView === 'datasets' && renderDatasets()}
      </div>

      {/* Dataset Detail Modal */}
      {selectedDataset && (
        <div style={{
          position: 'fixed',
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          backgroundColor: 'rgba(0,0,0,0.8)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 1000
        }}>
          <div style={{
            backgroundColor: colors.panel,
            border: `2px solid ${colors.primary}`,
            borderRadius: '8px',
            padding: '20px',
            maxWidth: '800px',
            maxHeight: '80vh',
            overflow: 'auto',
            width: '90%'
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'start', marginBottom: '16px' }}>
              <div>
                <h2 style={{ color: colors.primary, fontSize: '16px', margin: '0 0 8px 0' }}>
                  {selectedDataset.title}
                </h2>
                <div style={{ color: colors.textMuted, fontSize: '11px' }}>
                  {selectedDataset.organization}
                </div>
              </div>
              <button
                onClick={() => setSelectedDataset(null)}
                style={{
                  backgroundColor: colors.alert,
                  color: colors.background,
                  border: 'none',
                  padding: '6px 12px',
                  fontSize: '11px',
                  cursor: 'pointer',
                  borderRadius: '4px',
                  fontWeight: 'bold'
                }}
              >
                CLOSE
              </button>
            </div>

            <div style={{ fontSize: '11px', lineHeight: '1.6' }}>
              <div style={{ marginBottom: '12px' }}>
                <strong style={{ color: colors.secondary }}>Dataset Date:</strong> {selectedDataset.dataset_date}
              </div>
              <div style={{ marginBottom: '12px' }}>
                <strong style={{ color: colors.secondary }}>Last Modified:</strong> {selectedDataset.last_modified}
              </div>
              <div style={{ marginBottom: '12px' }}>
                <strong style={{ color: colors.secondary }}>Update Frequency:</strong> Every {selectedDataset.update_frequency} days
              </div>
              <div style={{ marginBottom: '12px' }}>
                <strong style={{ color: colors.secondary }}>License:</strong> {selectedDataset.license}
              </div>

              <div style={{ marginBottom: '12px' }}>
                <strong style={{ color: colors.secondary }}>Tags:</strong>
                <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap', marginTop: '4px' }}>
                  {selectedDataset.tags.map((tag, i) => (
                    <span key={i} style={{
                      fontSize: '9px',
                      backgroundColor: colors.background,
                      color: colors.info,
                      padding: '2px 8px',
                      borderRadius: '2px'
                    }}>
                      {tag}
                    </span>
                  ))}
                </div>
              </div>

              <div style={{ marginBottom: '12px' }}>
                <strong style={{ color: colors.secondary }}>Affected Regions:</strong>
                <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap', marginTop: '4px' }}>
                  {selectedDataset.groups.slice(0, 15).map((group, i) => (
                    <span key={i} style={{
                      fontSize: '9px',
                      backgroundColor: colors.background,
                      color: colors.warning,
                      padding: '2px 8px',
                      borderRadius: '2px'
                    }}>
                      {group}
                    </span>
                  ))}
                  {selectedDataset.groups.length > 15 && (
                    <span style={{ fontSize: '9px', color: colors.textMuted }}>
                      +{selectedDataset.groups.length - 15} more
                    </span>
                  )}
                </div>
              </div>

              <div style={{ marginBottom: '12px' }}>
                <strong style={{ color: colors.secondary }}>Description:</strong>
                <div style={{ color: colors.text, fontSize: '10px', marginTop: '4px', maxHeight: '150px', overflow: 'auto' }}>
                  {selectedDataset.notes}
                </div>
              </div>

              <div>
                <strong style={{ color: colors.secondary }}>Resources ({selectedDataset.resources.length}):</strong>
                <div style={{ marginTop: '8px' }}>
                  {selectedDataset.resources.map((resource, i) => (
                    <div key={i} style={{
                      padding: '8px',
                      backgroundColor: colors.background,
                      border: `1px solid ${colors.textMuted}`,
                      borderRadius: '4px',
                      marginBottom: '6px'
                    }}>
                      <div style={{ color: colors.text, fontWeight: 'bold', marginBottom: '4px' }}>
                        {resource.name}
                      </div>
                      <div style={{ color: colors.textMuted, fontSize: '9px', marginBottom: '4px' }}>
                        Format: {resource.format} • Size: {(Number(resource.size) / 1024 / 1024).toFixed(2)} MB
                      </div>
                      <div style={{ color: colors.textMuted, fontSize: '9px', marginBottom: '6px' }}>
                        {resource.description}
                      </div>
                      <a
                        href={resource.url}
                        target="_blank"
                        rel="noopener noreferrer"
                        style={{
                          color: colors.info,
                          fontSize: '9px',
                          textDecoration: 'underline'
                        }}
                      >
                        Download Resource →
                      </a>
                    </div>
                  ))}
                </div>
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default GeopoliticsHDXPanel;
