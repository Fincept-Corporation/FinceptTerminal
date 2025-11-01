// Unified Data Sources Panel for Settings Tab
// Displays and manages both WebSocket and REST API data sources

import React, { useState, useEffect } from 'react';
import { Plus, Wifi, Database, Edit3, Trash2, Power, PowerOff, Search, Filter, RefreshCw } from 'lucide-react';
import { DataSource } from '@/services/sqliteService';
import {
  getAllDataSources,
  toggleDataSource,
  deleteDataSource,
  createWebSocketDataSource,
  updateDataSource,
  getDataSourceStats,
  getWebSocketTopic,
  parseDataSourceConfig
} from '@/services/dataSourceRegistry';
import { getAvailableProviders } from '@/services/websocket';

interface DataSourcesPanelProps {
  colors: any;
}

export function DataSourcesPanel({ colors }: DataSourcesPanelProps) {
  const [dataSources, setDataSources] = useState<DataSource[]>([]);
  const [filteredSources, setFilteredSources] = useState<DataSource[]>([]);
  const [loading, setLoading] = useState(true);
  const [stats, setStats] = useState({ total: 0, websocket: 0, restApi: 0, enabled: 0, disabled: 0 });
  const [searchQuery, setSearchQuery] = useState('');
  const [filterType, setFilterType] = useState<'all' | 'websocket' | 'rest_api'>('all');
  const [showAddDialog, setShowAddDialog] = useState(false);
  const [showEditDialog, setShowEditDialog] = useState(false);
  const [editingSource, setEditingSource] = useState<DataSource | null>(null);

  // Add WebSocket form state
  const [newWSForm, setNewWSForm] = useState({
    alias: '',
    displayName: '',
    description: '',
    provider: 'kraken',
    channel: 'ticker',
    symbol: ''
  });

  // Edit form state
  const [editWSForm, setEditWSForm] = useState({
    alias: '',
    displayName: '',
    description: '',
    provider: 'kraken',
    channel: 'ticker',
    symbol: ''
  });

  const availableProviders = getAvailableProviders();

  useEffect(() => {
    loadDataSources();
  }, []);

  useEffect(() => {
    applyFilters();
  }, [dataSources, searchQuery, filterType]);

  async function loadDataSources() {
    try {
      setLoading(true);
      const sources = await getAllDataSources();
      const statistics = await getDataSourceStats();
      setDataSources(sources);
      setStats(statistics);
    } catch (error) {
      console.error('Failed to load data sources:', error);
    } finally {
      setLoading(false);
    }
  }

  function applyFilters() {
    let filtered = [...dataSources];

    // Filter by type
    if (filterType !== 'all') {
      filtered = filtered.filter(s => s.type === filterType);
    }

    // Filter by search query
    if (searchQuery) {
      const query = searchQuery.toLowerCase();
      filtered = filtered.filter(s =>
        s.alias.toLowerCase().includes(query) ||
        s.display_name.toLowerCase().includes(query) ||
        s.description?.toLowerCase().includes(query) ||
        s.provider.toLowerCase().includes(query)
      );
    }

    setFilteredSources(filtered);
  }

  async function handleToggle(id: string) {
    try {
      await toggleDataSource(id);
      await loadDataSources();
    } catch (error) {
      console.error('Failed to toggle data source:', error);
      alert('Failed to toggle data source');
    }
  }

  async function handleDelete(id: string) {
    if (!confirm('Delete this data source? This cannot be undone.')) {
      return;
    }

    try {
      await deleteDataSource(id);
      await loadDataSources();
    } catch (error) {
      console.error('Failed to delete data source:', error);
      alert('Failed to delete data source');
    }
  }

  async function handleAddWebSocket() {
    if (!newWSForm.alias || !newWSForm.displayName || !newWSForm.provider || !newWSForm.channel) {
      alert('Please fill in all required fields');
      return;
    }

    try {
      const result = await createWebSocketDataSource({
        alias: newWSForm.alias,
        displayName: newWSForm.displayName,
        description: newWSForm.description,
        provider: newWSForm.provider,
        channel: newWSForm.channel,
        symbol: newWSForm.symbol || undefined,
        enabled: true
      });

      if (result.success) {
        setShowAddDialog(false);
        setNewWSForm({
          alias: '',
          displayName: '',
          description: '',
          provider: 'kraken',
          channel: 'ticker',
          symbol: ''
        });
        await loadDataSources();
      } else {
        alert(result.message);
      }
    } catch (error) {
      console.error('Failed to create data source:', error);
      alert('Failed to create data source');
    }
  }

  function handleEditClick(source: DataSource) {
    setEditingSource(source);

    if (source.type === 'websocket') {
      const config = parseDataSourceConfig(source);
      if (config) {
        setEditWSForm({
          alias: source.alias,
          displayName: source.display_name,
          description: source.description || '',
          provider: (config as any).provider || 'kraken',
          channel: (config as any).channel || 'ticker',
          symbol: (config as any).symbol || ''
        });
      }
    }

    setShowEditDialog(true);
  }

  async function handleUpdateWebSocket() {
    if (!editingSource || !editWSForm.alias || !editWSForm.displayName || !editWSForm.provider || !editWSForm.channel) {
      alert('Please fill in all required fields');
      return;
    }

    try {
      const updatedSource: DataSource = {
        ...editingSource,
        alias: editWSForm.alias,
        display_name: editWSForm.displayName,
        description: editWSForm.description,
        provider: editWSForm.provider,
        config: JSON.stringify({
          provider: editWSForm.provider,
          channel: editWSForm.channel,
          symbol: editWSForm.symbol || undefined
        })
      };

      const result = await updateDataSource(updatedSource);

      if (result.success) {
        setShowEditDialog(false);
        setEditingSource(null);
        setEditWSForm({
          alias: '',
          displayName: '',
          description: '',
          provider: 'kraken',
          channel: 'ticker',
          symbol: ''
        });
        await loadDataSources();
      } else {
        alert(result.message);
      }
    } catch (error) {
      console.error('Failed to update data source:', error);
      alert('Failed to update data source');
    }
  }

  function getSourceIcon(type: string) {
    return type === 'websocket' ? Wifi : Database;
  }

  function getSourceStatus(source: DataSource) {
    if (!source.enabled) return { color: 'text-gray-500', text: 'Disabled' };
    if (source.type === 'websocket') return { color: 'text-green-500', text: 'Live' };
    return { color: 'text-blue-500', text: 'Ready' };
  }

  return (
    <div className="space-y-6">
      {/* Header */}
      <div>
        <h2 style={{ color: colors.primary, fontSize: '12px', fontWeight: 'bold', marginBottom: '8px' }}>
          DATA SOURCES
        </h2>
        <p style={{ color: colors.text, fontSize: '10px' }}>
          Manage WebSocket connections and REST API integrations. Create aliases to easily reference data sources across the terminal.
        </p>
      </div>

      {/* Statistics */}
      <div className="grid grid-cols-5 gap-4">
        <div style={{ backgroundColor: colors.panel, border: `1px solid ${colors.primary}` }} className="p-3 rounded">
          <div style={{ color: colors.text, fontSize: '10px', fontWeight: 'bold' }}>TOTAL</div>
          <div style={{ color: colors.primary, fontSize: '20px', fontWeight: 'bold' }}>{stats.total}</div>
        </div>
        <div style={{ backgroundColor: colors.panel, border: `1px solid ${colors.primary}` }} className="p-3 rounded">
          <div style={{ color: colors.text, fontSize: '10px', fontWeight: 'bold' }}>WEBSOCKET</div>
          <div style={{ color: '#10b981', fontSize: '20px', fontWeight: 'bold' }}>{stats.websocket}</div>
        </div>
        <div style={{ backgroundColor: colors.panel, border: `1px solid ${colors.primary}` }} className="p-3 rounded">
          <div style={{ color: colors.text, fontSize: '10px', fontWeight: 'bold' }}>REST API</div>
          <div style={{ color: '#3b82f6', fontSize: '20px', fontWeight: 'bold' }}>{stats.restApi}</div>
        </div>
        <div style={{ backgroundColor: colors.panel, border: `1px solid ${colors.primary}` }} className="p-3 rounded">
          <div style={{ color: colors.text, fontSize: '10px', fontWeight: 'bold' }}>ENABLED</div>
          <div style={{ color: '#10b981', fontSize: '20px', fontWeight: 'bold' }}>{stats.enabled}</div>
        </div>
        <div style={{ backgroundColor: colors.panel, border: `1px solid ${colors.primary}` }} className="p-3 rounded">
          <div style={{ color: colors.text, fontSize: '10px', fontWeight: 'bold' }}>DISABLED</div>
          <div style={{ color: colors.text, fontSize: '20px', fontWeight: 'bold' }}>{stats.disabled}</div>
        </div>
      </div>

      {/* Toolbar */}
      <div className="flex items-center gap-3">
        <div className="flex-1 relative">
          <Search size={14} className="absolute left-3 top-1/2 transform -translate-y-1/2" style={{ color: colors.primary }} />
          <input
            type="text"
            placeholder="Search data sources..."
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            style={{
              backgroundColor: colors.panel,
              border: `1px solid ${colors.primary}`,
              color: colors.text,
              fontSize: '11px',
              padding: '8px 8px 8px 36px',
              borderRadius: '4px',
              width: '100%'
            }}
          />
        </div>

        <select
          value={filterType}
          onChange={(e) => setFilterType(e.target.value as any)}
          style={{
            backgroundColor: colors.panel,
            border: `1px solid ${colors.primary}`,
            color: colors.text,
            fontSize: '11px',
            padding: '8px 12px',
            borderRadius: '4px'
          }}
        >
          <option style={{ backgroundColor: colors.panel, color: colors.text }} value="all">All Types</option>
          <option style={{ backgroundColor: colors.panel, color: colors.text }} value="websocket">WebSocket</option>
          <option style={{ backgroundColor: colors.panel, color: colors.text }} value="rest_api">REST API</option>
        </select>

        <button
          onClick={loadDataSources}
          style={{
            backgroundColor: colors.panel,
            border: `1px solid ${colors.primary}`,
            color: colors.text,
            fontSize: '11px',
            fontWeight: 'bold',
            padding: '8px 12px',
            borderRadius: '4px',
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            cursor: 'pointer'
          }}
        >
          <RefreshCw size={14} />
          REFRESH
        </button>

        <button
          onClick={() => setShowAddDialog(true)}
          style={{
            backgroundColor: colors.primary,
            color: colors.background,
            fontSize: '11px',
            fontWeight: 'bold',
            padding: '8px 12px',
            borderRadius: '4px',
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            cursor: 'pointer'
          }}
        >
          <Plus size={14} />
          ADD SOURCE
        </button>
      </div>

      {/* Data Sources List */}
      {loading ? (
        <div style={{ textAlign: 'center', padding: '40px', color: colors.text }}>
          Loading data sources...
        </div>
      ) : filteredSources.length === 0 ? (
        <div style={{ textAlign: 'center', padding: '40px' }}>
          <p style={{ color: colors.text, marginBottom: '16px' }}>
            {searchQuery || filterType !== 'all' ? 'No data sources match your filters' : 'No data sources yet'}
          </p>
          {!searchQuery && filterType === 'all' && (
            <button
              onClick={() => setShowAddDialog(true)}
              style={{
                backgroundColor: colors.primary,
                color: colors.background,
                fontSize: '11px',
                fontWeight: 'bold',
                padding: '10px 16px',
                borderRadius: '4px',
                cursor: 'pointer'
              }}
            >
              Create First Data Source
            </button>
          )}
        </div>
      ) : (
        <div className="space-y-3">
          {filteredSources.map((source) => {
            const Icon = getSourceIcon(source.type);
            const status = getSourceStatus(source);
            const topic = source.type === 'websocket' ? getWebSocketTopic(source) : null;

            return (
              <div
                key={source.id}
                style={{
                  backgroundColor: colors.panel,
                  border: `1px solid ${source.enabled ? colors.primary : '#444'}`,
                  borderRadius: '6px',
                  padding: '16px'
                }}
              >
                <div className="flex items-start justify-between">
                  <div className="flex items-start gap-3 flex-1">
                    <Icon size={18} style={{ color: source.enabled ? colors.primary : colors.text, marginTop: '2px' }} />

                    <div className="flex-1">
                      <div className="flex items-center gap-2 mb-1">
                        <h3 style={{ color: colors.text, fontSize: '13px', fontWeight: 'bold' }}>
                          {source.display_name}
                        </h3>
                        <code style={{
                          backgroundColor: colors.background,
                          color: colors.primary,
                          fontSize: '10px',
                          padding: '2px 6px',
                          borderRadius: '3px'
                        }}>
                          {source.alias}
                        </code>
                        <span style={{ fontSize: '10px', color: status.color, fontWeight: 'bold' }}>
                          ● {status.text}
                        </span>
                      </div>

                      {source.description && (
                        <p style={{ color: colors.text, fontSize: '11px', marginBottom: '8px' }}>
                          {source.description}
                        </p>
                      )}

                      <div className="flex items-center gap-4" style={{ fontSize: '10px' }}>
                        <span style={{ color: colors.text }}>
                          <strong style={{ color: colors.primary }}>Type:</strong> {source.type === 'websocket' ? 'WebSocket' : 'REST API'}
                        </span>
                        <span style={{ color: colors.text }}>
                          <strong style={{ color: colors.primary }}>Provider:</strong> {source.provider}
                        </span>
                        {topic && (
                          <span style={{ color: colors.text }}>
                            <strong style={{ color: colors.primary }}>Topic:</strong> <code style={{ color: colors.primary }}>{topic}</code>
                          </span>
                        )}
                      </div>
                    </div>
                  </div>

                  <div className="flex items-center gap-2">
                    <button
                      onClick={() => handleEditClick(source)}
                      style={{
                        padding: '6px',
                        borderRadius: '4px',
                        backgroundColor: colors.background,
                        border: `1px solid ${colors.primary}`,
                        color: colors.primary,
                        cursor: 'pointer'
                      }}
                      title="Edit"
                    >
                      <Edit3 size={14} />
                    </button>

                    <button
                      onClick={() => handleToggle(source.id)}
                      style={{
                        padding: '6px',
                        borderRadius: '4px',
                        backgroundColor: source.enabled ? colors.background : colors.primary,
                        border: `1px solid ${colors.primary}`,
                        color: source.enabled ? colors.text : colors.background,
                        cursor: 'pointer'
                      }}
                      title={source.enabled ? 'Disable' : 'Enable'}
                    >
                      {source.enabled ? <Power size={14} /> : <PowerOff size={14} />}
                    </button>

                    <button
                      onClick={() => handleDelete(source.id)}
                      style={{
                        padding: '6px',
                        borderRadius: '4px',
                        backgroundColor: colors.background,
                        border: `1px solid #ef4444`,
                        color: '#ef4444',
                        cursor: 'pointer'
                      }}
                      title="Delete"
                    >
                      <Trash2 size={14} />
                    </button>
                  </div>
                </div>
              </div>
            );
          })}
        </div>
      )}

      {/* Add WebSocket Dialog */}
      {showAddDialog && (
        <div style={{
          position: 'fixed',
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          backgroundColor: 'rgba(0, 0, 0, 0.9)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 1000
        }}>
          <div style={{
            backgroundColor: colors.panel,
            border: `2px solid ${colors.primary}`,
            borderRadius: '8px',
            padding: '24px',
            maxWidth: '500px',
            width: '100%'
          }}>
            <h3 style={{ color: colors.primary, fontSize: '14px', fontWeight: 'bold', marginBottom: '16px' }}>
              Add WebSocket Data Source
            </h3>

            <div className="space-y-4">
              <div>
                <label style={{ color: colors.text, fontSize: '10px', fontWeight: 'bold', display: 'block', marginBottom: '6px' }}>
                  ALIAS (short name)
                </label>
                <input
                  type="text"
                  placeholder="btc_live"
                  value={newWSForm.alias}
                  onChange={(e) => setNewWSForm({ ...newWSForm, alias: e.target.value })}
                  style={{
                    width: '100%',
                    backgroundColor: colors.background,
                    border: `1px solid ${colors.primary}`,
                    color: colors.text,
                    fontSize: '11px',
                    padding: '8px 12px',
                    borderRadius: '4px'
                  }}
                />
              </div>

              <div>
                <label style={{ color: colors.text, fontSize: '10px', fontWeight: 'bold', display: 'block', marginBottom: '6px' }}>
                  DISPLAY NAME
                </label>
                <input
                  type="text"
                  placeholder="Bitcoin Price"
                  value={newWSForm.displayName}
                  onChange={(e) => setNewWSForm({ ...newWSForm, displayName: e.target.value })}
                  style={{
                    width: '100%',
                    backgroundColor: colors.background,
                    border: `1px solid ${colors.primary}`,
                    color: colors.text,
                    fontSize: '11px',
                    padding: '8px 12px',
                    borderRadius: '4px'
                  }}
                />
              </div>

              <div>
                <label style={{ color: colors.text, fontSize: '10px', fontWeight: 'bold', display: 'block', marginBottom: '6px' }}>
                  DESCRIPTION
                </label>
                <input
                  type="text"
                  placeholder="Real-time Bitcoin price from Kraken"
                  value={newWSForm.description}
                  onChange={(e) => setNewWSForm({ ...newWSForm, description: e.target.value })}
                  style={{
                    width: '100%',
                    backgroundColor: colors.background,
                    border: `1px solid ${colors.primary}`,
                    color: colors.text,
                    fontSize: '11px',
                    padding: '8px 12px',
                    borderRadius: '4px'
                  }}
                />
              </div>

              <div className="grid grid-cols-2 gap-4">
                <div>
                  <label style={{ color: colors.text, fontSize: '10px', fontWeight: 'bold', display: 'block', marginBottom: '6px' }}>
                    PROVIDER
                  </label>
                  <select
                    value={newWSForm.provider}
                    onChange={(e) => setNewWSForm({ ...newWSForm, provider: e.target.value })}
                    style={{
                      width: '100%',
                      backgroundColor: colors.background,
                      border: `1px solid ${colors.primary}`,
                      color: colors.text,
                      fontSize: '11px',
                      padding: '8px 12px',
                      borderRadius: '4px'
                    }}
                  >
                    {availableProviders.map(p => (
                      <option key={p} value={p} style={{ backgroundColor: colors.background, color: colors.text }}>{p}</option>
                    ))}
                  </select>
                </div>

                <div>
                  <label style={{ color: colors.text, fontSize: '10px', fontWeight: 'bold', display: 'block', marginBottom: '6px' }}>
                    CHANNEL
                  </label>
                  <input
                    type="text"
                    placeholder="ticker"
                    value={newWSForm.channel}
                    onChange={(e) => setNewWSForm({ ...newWSForm, channel: e.target.value })}
                    style={{
                      width: '100%',
                      backgroundColor: colors.background,
                      border: `1px solid ${colors.primary}`,
                      color: colors.text,
                      fontSize: '11px',
                      padding: '8px 12px',
                      borderRadius: '4px'
                    }}
                  />
                </div>
              </div>

              <div>
                <label style={{ color: colors.text, fontSize: '10px', fontWeight: 'bold', display: 'block', marginBottom: '6px' }}>
                  SYMBOL (optional)
                </label>
                <input
                  type="text"
                  placeholder="BTC/USD"
                  value={newWSForm.symbol}
                  onChange={(e) => setNewWSForm({ ...newWSForm, symbol: e.target.value })}
                  style={{
                    width: '100%',
                    backgroundColor: colors.background,
                    border: `1px solid ${colors.primary}`,
                    color: colors.text,
                    fontSize: '11px',
                    padding: '8px 12px',
                    borderRadius: '4px'
                  }}
                />
              </div>
            </div>

            <div className="flex gap-3 mt-6">
              <button
                onClick={handleAddWebSocket}
                style={{
                  flex: 1,
                  backgroundColor: colors.primary,
                  color: colors.background,
                  fontSize: '11px',
                  fontWeight: 'bold',
                  padding: '10px',
                  borderRadius: '4px',
                  border: 'none',
                  cursor: 'pointer'
                }}
              >
                CREATE DATA SOURCE
              </button>
              <button
                onClick={() => setShowAddDialog(false)}
                style={{
                  flex: 1,
                  backgroundColor: colors.background,
                  border: `1px solid ${colors.primary}`,
                  color: colors.text,
                  fontSize: '11px',
                  fontWeight: 'bold',
                  padding: '10px',
                  borderRadius: '4px',
                  cursor: 'pointer'
                }}
              >
                CANCEL
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Edit WebSocket Dialog */}
      {showEditDialog && editingSource && (
        <div style={{
          position: 'fixed',
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          backgroundColor: 'rgba(0, 0, 0, 0.9)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 1000
        }}>
          <div style={{
            backgroundColor: colors.panel,
            border: `2px solid ${colors.primary}`,
            borderRadius: '8px',
            padding: '24px',
            maxWidth: '500px',
            width: '100%'
          }}>
            <h3 style={{ color: colors.primary, fontSize: '14px', fontWeight: 'bold', marginBottom: '16px' }}>
              Edit WebSocket Data Source
            </h3>

            <div className="space-y-4">
              <div>
                <label style={{ color: colors.text, fontSize: '10px', fontWeight: 'bold', display: 'block', marginBottom: '6px' }}>
                  ALIAS (short name)
                </label>
                <input
                  type="text"
                  placeholder="btc_live"
                  value={editWSForm.alias}
                  onChange={(e) => setEditWSForm({ ...editWSForm, alias: e.target.value })}
                  style={{
                    width: '100%',
                    backgroundColor: colors.background,
                    border: `1px solid ${colors.primary}`,
                    color: colors.text,
                    fontSize: '11px',
                    padding: '8px 12px',
                    borderRadius: '4px'
                  }}
                />
              </div>

              <div>
                <label style={{ color: colors.text, fontSize: '10px', fontWeight: 'bold', display: 'block', marginBottom: '6px' }}>
                  DISPLAY NAME
                </label>
                <input
                  type="text"
                  placeholder="Bitcoin Price"
                  value={editWSForm.displayName}
                  onChange={(e) => setEditWSForm({ ...editWSForm, displayName: e.target.value })}
                  style={{
                    width: '100%',
                    backgroundColor: colors.background,
                    border: `1px solid ${colors.primary}`,
                    color: colors.text,
                    fontSize: '11px',
                    padding: '8px 12px',
                    borderRadius: '4px'
                  }}
                />
              </div>

              <div>
                <label style={{ color: colors.text, fontSize: '10px', fontWeight: 'bold', display: 'block', marginBottom: '6px' }}>
                  DESCRIPTION
                </label>
                <input
                  type="text"
                  placeholder="Real-time Bitcoin price from Kraken"
                  value={editWSForm.description}
                  onChange={(e) => setEditWSForm({ ...editWSForm, description: e.target.value })}
                  style={{
                    width: '100%',
                    backgroundColor: colors.background,
                    border: `1px solid ${colors.primary}`,
                    color: colors.text,
                    fontSize: '11px',
                    padding: '8px 12px',
                    borderRadius: '4px'
                  }}
                />
              </div>

              <div className="grid grid-cols-2 gap-4">
                <div>
                  <label style={{ color: colors.text, fontSize: '10px', fontWeight: 'bold', display: 'block', marginBottom: '6px' }}>
                    PROVIDER
                  </label>
                  <select
                    value={editWSForm.provider}
                    onChange={(e) => setEditWSForm({ ...editWSForm, provider: e.target.value })}
                    style={{
                      width: '100%',
                      backgroundColor: colors.background,
                      border: `1px solid ${colors.primary}`,
                      color: colors.text,
                      fontSize: '11px',
                      padding: '8px 12px',
                      borderRadius: '4px'
                    }}
                  >
                    {availableProviders.map(p => (
                      <option key={p} value={p} style={{ backgroundColor: colors.background, color: colors.text }}>{p}</option>
                    ))}
                  </select>
                </div>

                <div>
                  <label style={{ color: colors.text, fontSize: '10px', fontWeight: 'bold', display: 'block', marginBottom: '6px' }}>
                    CHANNEL
                  </label>
                  <input
                    type="text"
                    placeholder="ticker"
                    value={editWSForm.channel}
                    onChange={(e) => setEditWSForm({ ...editWSForm, channel: e.target.value })}
                    style={{
                      width: '100%',
                      backgroundColor: colors.background,
                      border: `1px solid ${colors.primary}`,
                      color: colors.text,
                      fontSize: '11px',
                      padding: '8px 12px',
                      borderRadius: '4px'
                    }}
                  />
                </div>
              </div>

              <div>
                <label style={{ color: colors.text, fontSize: '10px', fontWeight: 'bold', display: 'block', marginBottom: '6px' }}>
                  SYMBOL (optional)
                </label>
                <input
                  type="text"
                  placeholder="BTC/USD"
                  value={editWSForm.symbol}
                  onChange={(e) => setEditWSForm({ ...editWSForm, symbol: e.target.value })}
                  style={{
                    width: '100%',
                    backgroundColor: colors.background,
                    border: `1px solid ${colors.primary}`,
                    color: colors.text,
                    fontSize: '11px',
                    padding: '8px 12px',
                    borderRadius: '4px'
                  }}
                />
              </div>
            </div>

            <div className="flex gap-3 mt-6">
              <button
                onClick={handleUpdateWebSocket}
                style={{
                  flex: 1,
                  backgroundColor: colors.primary,
                  color: colors.background,
                  fontSize: '11px',
                  fontWeight: 'bold',
                  padding: '10px',
                  borderRadius: '4px',
                  border: 'none',
                  cursor: 'pointer'
                }}
              >
                UPDATE DATA SOURCE
              </button>
              <button
                onClick={() => {
                  setShowEditDialog(false);
                  setEditingSource(null);
                }}
                style={{
                  flex: 1,
                  backgroundColor: colors.background,
                  border: `1px solid ${colors.primary}`,
                  color: colors.text,
                  fontSize: '11px',
                  fontWeight: 'bold',
                  padding: '10px',
                  borderRadius: '4px',
                  cursor: 'pointer'
                }}
              >
                CANCEL
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
