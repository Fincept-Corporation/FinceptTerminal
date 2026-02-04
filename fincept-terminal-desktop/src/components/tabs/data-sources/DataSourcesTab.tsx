import React, { useState, useCallback } from 'react';
import { useWorkspaceTabState } from '@/hooks/useWorkspaceTabState';
import {
  Plus,
  Search,
  Database,
  Cloud,
  FileText,
  Activity,
  TrendingUp,
  Globe,
  Settings,
  Trash2,
  Check,
  X,
  AlertCircle,
  Edit2,
  RefreshCw,
  Loader2,
} from 'lucide-react';
import { DATA_SOURCE_CONFIGS } from './dataSourceConfigs';
import { DataSourceConfig, DataSourceConnection, DataSourceCategory } from './types';
import { hasAdapter } from './adapters';
import { useDataSources } from '../../../contexts/DataSourceContext';
import { TabFooter } from '@/components/common/TabFooter';
import { ErrorBoundary } from '@/components/common/ErrorBoundary';

// Design system colors
const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
};

function DataSourcesTabContent() {
  const {
    connections,
    loadState,
    error,
    addConnection,
    updateConnection,
    deleteConnection,
    testConnection,
    refresh,
  } = useDataSources();

  const [view, setView] = useState<'gallery' | 'connections'>('gallery');
  const [searchTerm, setSearchTerm] = useState('');
  const [selectedCategory, setSelectedCategory] = useState<DataSourceCategory | 'all'>('all');
  const [showConfigModal, setShowConfigModal] = useState(false);
  const [selectedDataSource, setSelectedDataSource] = useState<DataSourceConfig | null>(null);
  const [configFormData, setConfigFormData] = useState<Record<string, any>>({});
  const [editingConnection, setEditingConnection] = useState<DataSourceConnection | null>(null);
  const [saveError, setSaveError] = useState<string | null>(null);

  // Workspace tab state
  const getWorkspaceState = useCallback(() => ({
    view, selectedCategory,
  }), [view, selectedCategory]);

  const setWorkspaceState = useCallback((state: any) => {
    if (state.view === "gallery" || state.view === "connections") setView(state.view);
    if (typeof state.selectedCategory === "string") setSelectedCategory(state.selectedCategory);
  }, []);

  useWorkspaceTabState("datasources", getWorkspaceState, setWorkspaceState);

  const getCategoryIcon = (category: DataSourceCategory) => {
    const iconProps = { size: 12, color: FINCEPT.GRAY };
    switch (category) {
      case 'database':
        return <Database {...iconProps} />;
      case 'api':
        return <Globe {...iconProps} />;
      case 'file':
        return <FileText {...iconProps} />;
      case 'streaming':
        return <Activity {...iconProps} />;
      case 'cloud':
        return <Cloud {...iconProps} />;
      case 'timeseries':
      case 'market-data':
        return <TrendingUp {...iconProps} />;
      default:
        return <Database {...iconProps} />;
    }
  };

  const filteredDataSources = DATA_SOURCE_CONFIGS.filter((ds) => {
    const matchesSearch =
      ds.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
      ds.description.toLowerCase().includes(searchTerm.toLowerCase());
    const matchesCategory = selectedCategory === 'all' || ds.category === selectedCategory;
    return matchesSearch && matchesCategory;
  });

  const handleOpenConfig = (dataSource: DataSourceConfig) => {
    setSelectedDataSource(dataSource);
    setEditingConnection(null);
    setSaveError(null);
    const initialData: Record<string, any> = {};
    dataSource.fields.forEach((field) => {
      if (field.defaultValue !== undefined) {
        initialData[field.name] = field.defaultValue;
      }
    });
    setConfigFormData(initialData);
    setShowConfigModal(true);
  };

  const handleEditConnection = (connection: DataSourceConnection) => {
    const dataSource = DATA_SOURCE_CONFIGS.find((ds) => ds.type === connection.type);
    if (!dataSource) return;

    setSelectedDataSource(dataSource);
    setEditingConnection(connection);
    setSaveError(null);
    setConfigFormData({ ...connection.config, name: connection.name });
    setShowConfigModal(true);
  };

  const handleSaveConnection = async () => {
    if (!selectedDataSource) return;

    // Validate required fields
    const connectionName = configFormData.name?.trim();
    if (!connectionName) {
      setSaveError('Connection name is required');
      return;
    }

    setSaveError(null);

    try {
      if (editingConnection) {
        await updateConnection(editingConnection.id, {
          name: connectionName,
          config: { ...configFormData },
        });
      } else {
        const newConnection: DataSourceConnection = {
          id: `conn_${Date.now()}`,
          name: connectionName,
          type: selectedDataSource.type,
          category: selectedDataSource.category,
          config: { ...configFormData },
          status: 'disconnected',
          createdAt: new Date().toISOString(),
          updatedAt: new Date().toISOString(),
        };
        await addConnection(newConnection);
      }

      setShowConfigModal(false);
      setSelectedDataSource(null);
      setConfigFormData({});
      setEditingConnection(null);
      setView('connections');
    } catch (err) {
      setSaveError(err instanceof Error ? err.message : 'Failed to save connection');
    }
  };

  const handleDeleteConnectionClick = async (id: string) => {
    try {
      await deleteConnection(id);
    } catch (err) {
      console.error('Delete failed:', err);
    }
  };

  const handleTestConnection = async (connection: DataSourceConnection) => {
    await testConnection(connection);
  };

  // Loading state
  if (loadState === 'loading') {
    return (
      <div
        style={{
          width: '100%',
          height: '100%',
          backgroundColor: FINCEPT.DARK_BG,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          flexDirection: 'column',
          gap: '12px',
        }}
      >
        <Loader2 size={24} color={FINCEPT.ORANGE} style={{ animation: 'spin 1s linear infinite' }} />
        <span style={{ fontSize: '10px', color: FINCEPT.GRAY, fontFamily: '"IBM Plex Mono", monospace' }}>
          LOADING DATA SOURCES...
        </span>
      </div>
    );
  }

  // Error state
  if (loadState === 'error' && error) {
    return (
      <div
        style={{
          width: '100%',
          height: '100%',
          backgroundColor: FINCEPT.DARK_BG,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          flexDirection: 'column',
          gap: '12px',
        }}
      >
        <AlertCircle size={24} color={FINCEPT.RED} />
        <span style={{ fontSize: '10px', color: FINCEPT.RED, fontFamily: '"IBM Plex Mono", monospace' }}>
          {error}
        </span>
        <button
          onClick={refresh}
          style={{
            padding: '6px 12px',
            backgroundColor: FINCEPT.ORANGE,
            color: FINCEPT.DARK_BG,
            border: 'none',
            borderRadius: '2px',
            fontSize: '9px',
            fontWeight: 700,
            cursor: 'pointer',
            fontFamily: '"IBM Plex Mono", monospace',
          }}
        >
          RETRY
        </button>
      </div>
    );
  }

  return (
    <div
      style={{
        width: '100%',
        height: '100%',
        backgroundColor: FINCEPT.DARK_BG,
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
        fontFamily: '"IBM Plex Mono", "Consolas", monospace',
      }}
    >
      {/* Top Nav Bar */}
      <div
        style={{
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `2px solid ${FINCEPT.ORANGE}`,
          padding: '8px 16px',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          boxShadow: `0 2px 8px ${FINCEPT.ORANGE}20`,
          flexShrink: 0,
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <span
            style={{
              color: FINCEPT.ORANGE,
              fontSize: '11px',
              fontWeight: 700,
              letterSpacing: '0.5px',
            }}
          >
            DATA SOURCES
          </span>
          <div style={{ width: '1px', height: '16px', backgroundColor: FINCEPT.BORDER }} />
          <span style={{ color: FINCEPT.GRAY, fontSize: '9px', letterSpacing: '0.5px' }}>
            {DATA_SOURCE_CONFIGS.length} AVAILABLE | {connections.length} CONNECTED
          </span>
        </div>

        {/* View Switcher */}
        <div style={{ display: 'flex', gap: '4px' }}>
          <button
            onClick={() => setView('gallery')}
            style={{
              padding: '6px 12px',
              backgroundColor: view === 'gallery' ? FINCEPT.ORANGE : 'transparent',
              color: view === 'gallery' ? FINCEPT.DARK_BG : FINCEPT.GRAY,
              border: 'none',
              fontSize: '9px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              cursor: 'pointer',
              borderRadius: '2px',
            }}
          >
            GALLERY
          </button>
          <button
            onClick={() => setView('connections')}
            style={{
              padding: '6px 12px',
              backgroundColor: view === 'connections' ? FINCEPT.ORANGE : 'transparent',
              color: view === 'connections' ? FINCEPT.DARK_BG : FINCEPT.GRAY,
              border: 'none',
              fontSize: '9px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              cursor: 'pointer',
              borderRadius: '2px',
            }}
          >
            MY CONNECTIONS ({connections.length})
          </button>
        </div>
      </div>

      {/* Search and Filter Bar */}
      {view === 'gallery' && (
        <div
          style={{
            padding: '12px 16px',
            backgroundColor: FINCEPT.HEADER_BG,
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            gap: '12px',
            alignItems: 'center',
            flexShrink: 0,
          }}
        >
          <div style={{ position: 'relative', flex: 1, maxWidth: '300px' }}>
            <Search
              size={12}
              style={{
                position: 'absolute',
                left: '10px',
                top: '50%',
                transform: 'translateY(-50%)',
                color: FINCEPT.GRAY,
              }}
            />
            <input
              type="text"
              placeholder="Search data sources..."
              value={searchTerm}
              onChange={(e) => setSearchTerm(e.target.value)}
              style={{
                width: '100%',
                padding: '8px 10px 8px 32px',
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '10px',
                fontFamily: '"IBM Plex Mono", monospace',
                outline: 'none',
              }}
            />
          </div>

          {/* Category Filter */}
          <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap' }}>
            {['all', 'database', 'api', 'file', 'streaming', 'cloud', 'timeseries', 'market-data'].map((cat) => (
              <button
                key={cat}
                onClick={() => setSelectedCategory(cat as DataSourceCategory | 'all')}
                style={{
                  padding: '6px 10px',
                  backgroundColor: selectedCategory === cat ? `${FINCEPT.ORANGE}15` : 'transparent',
                  color: selectedCategory === cat ? FINCEPT.ORANGE : FINCEPT.GRAY,
                  border: `1px solid ${selectedCategory === cat ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '9px',
                  fontWeight: 700,
                  letterSpacing: '0.5px',
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px',
                  textTransform: 'uppercase',
                }}
              >
                {cat !== 'all' && getCategoryIcon(cat as DataSourceCategory)}
                {cat.replace('-', ' ')}
              </button>
            ))}
          </div>
        </div>
      )}

      {/* Content Area */}
      <div
        style={{
          flex: 1,
          overflow: 'auto',
          padding: '16px',
        }}
      >
        {view === 'gallery' ? (
          /* Data Source Gallery */
          <div
            style={{
              display: 'grid',
              gridTemplateColumns: 'repeat(auto-fill, minmax(260px, 1fr))',
              gap: '12px',
            }}
          >
            {filteredDataSources.map((dataSource) => (
              <div
                key={dataSource.id}
                style={{
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  padding: '12px',
                  cursor: 'pointer',
                  transition: 'all 0.2s',
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.borderColor = FINCEPT.ORANGE;
                  e.currentTarget.style.boxShadow = `0 0 8px ${FINCEPT.ORANGE}30`;
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.borderColor = FINCEPT.BORDER;
                  e.currentTarget.style.boxShadow = 'none';
                }}
                onClick={() => handleOpenConfig(dataSource)}
              >
                <div style={{ display: 'flex', alignItems: 'flex-start', gap: '10px', marginBottom: '10px' }}>
                  <span style={{ fontSize: '24px' }}>{dataSource.icon}</span>
                  <div style={{ flex: 1 }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '4px' }}>
                      <h3
                        style={{
                          color: FINCEPT.WHITE,
                          fontSize: '11px',
                          fontWeight: 700,
                          margin: 0,
                        }}
                      >
                        {dataSource.name}
                      </h3>
                      {hasAdapter(dataSource.type) && (
                        <span
                          style={{
                            padding: '2px 6px',
                            backgroundColor: `${FINCEPT.GREEN}20`,
                            color: FINCEPT.GREEN,
                            fontSize: '8px',
                            fontWeight: 700,
                            borderRadius: '2px',
                          }}
                        >
                          READY
                        </span>
                      )}
                    </div>
                    <span
                      style={{
                        display: 'inline-block',
                        padding: '2px 6px',
                        backgroundColor: `${FINCEPT.CYAN}20`,
                        color: FINCEPT.CYAN,
                        fontSize: '8px',
                        fontWeight: 700,
                        borderRadius: '2px',
                        textTransform: 'uppercase',
                      }}
                    >
                      {dataSource.category}
                    </span>
                  </div>
                </div>
                <p
                  style={{
                    color: FINCEPT.GRAY,
                    fontSize: '10px',
                    lineHeight: '1.4',
                    margin: '0 0 10px 0',
                  }}
                >
                  {dataSource.description}
                </p>
                <div style={{ display: 'flex', gap: '8px', fontSize: '9px', color: FINCEPT.MUTED }}>
                  {dataSource.requiresAuth && (
                    <span style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                      <Settings size={10} /> AUTH
                    </span>
                  )}
                  {dataSource.testable && (
                    <span style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                      <Check size={10} /> TESTABLE
                    </span>
                  )}
                </div>
              </div>
            ))}
          </div>
        ) : (
          /* Connections List */
          <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
            {connections.length === 0 ? (
              <div
                style={{
                  display: 'flex',
                  flexDirection: 'column',
                  alignItems: 'center',
                  justifyContent: 'center',
                  height: '300px',
                  color: FINCEPT.MUTED,
                }}
              >
                <Database size={32} style={{ marginBottom: '12px', opacity: 0.5 }} />
                <span style={{ fontSize: '11px', marginBottom: '8px', color: FINCEPT.GRAY }}>
                  NO CONNECTIONS CONFIGURED
                </span>
                <span style={{ fontSize: '10px', marginBottom: '16px' }}>
                  Add your first data source connection to get started
                </span>
                <button
                  onClick={() => setView('gallery')}
                  style={{
                    padding: '8px 16px',
                    backgroundColor: FINCEPT.ORANGE,
                    color: FINCEPT.DARK_BG,
                    border: 'none',
                    borderRadius: '2px',
                    fontSize: '9px',
                    fontWeight: 700,
                    cursor: 'pointer',
                  }}
                >
                  BROWSE DATA SOURCES
                </button>
              </div>
            ) : (
              connections.map((connection: DataSourceConnection) => {
                const config = DATA_SOURCE_CONFIGS.find((ds) => ds.type === connection.type);
                return (
                  <div
                    key={connection.id}
                    style={{
                      backgroundColor: FINCEPT.PANEL_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      borderRadius: '2px',
                      padding: '12px',
                    }}
                  >
                    <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
                        <span style={{ fontSize: '20px' }}>{config?.icon}</span>
                        <div>
                          <h3
                            style={{
                              color: FINCEPT.WHITE,
                              fontSize: '11px',
                              fontWeight: 700,
                              margin: '0 0 4px 0',
                            }}
                          >
                            {connection.name}
                          </h3>
                          <div style={{ display: 'flex', gap: '6px', alignItems: 'center' }}>
                            <span
                              style={{
                                padding: '2px 6px',
                                backgroundColor: `${FINCEPT.CYAN}20`,
                                color: FINCEPT.CYAN,
                                fontSize: '8px',
                                fontWeight: 700,
                                borderRadius: '2px',
                                textTransform: 'uppercase',
                              }}
                            >
                              {connection.type}
                            </span>
                            <span
                              style={{
                                display: 'flex',
                                alignItems: 'center',
                                gap: '4px',
                                fontSize: '9px',
                                color:
                                  connection.status === 'connected'
                                    ? FINCEPT.GREEN
                                    : connection.status === 'error'
                                      ? FINCEPT.RED
                                      : connection.status === 'testing'
                                        ? FINCEPT.ORANGE
                                        : FINCEPT.GRAY,
                              }}
                            >
                              {connection.status === 'connected' && <Check size={10} />}
                              {connection.status === 'error' && <X size={10} />}
                              {connection.status === 'testing' && (
                                <RefreshCw size={10} style={{ animation: 'spin 1s linear infinite' }} />
                              )}
                              {connection.status === 'disconnected' && <AlertCircle size={10} />}
                              {connection.status.toUpperCase()}
                            </span>
                          </div>
                        </div>
                      </div>

                      <div style={{ display: 'flex', gap: '6px' }}>
                        {config?.testable && (
                          <button
                            onClick={() => handleTestConnection(connection)}
                            disabled={connection.status === 'testing'}
                            style={{
                              padding: '6px 10px',
                              backgroundColor: 'transparent',
                              border: `1px solid ${FINCEPT.GREEN}`,
                              color: FINCEPT.GREEN,
                              fontSize: '9px',
                              fontWeight: 700,
                              borderRadius: '2px',
                              cursor: connection.status === 'testing' ? 'not-allowed' : 'pointer',
                              opacity: connection.status === 'testing' ? 0.5 : 1,
                              display: 'flex',
                              alignItems: 'center',
                              gap: '4px',
                            }}
                          >
                            <RefreshCw size={10} />
                            TEST
                          </button>
                        )}
                        <button
                          onClick={() => handleEditConnection(connection)}
                          style={{
                            padding: '6px 10px',
                            backgroundColor: 'transparent',
                            border: `1px solid ${FINCEPT.BORDER}`,
                            color: FINCEPT.GRAY,
                            fontSize: '9px',
                            fontWeight: 700,
                            borderRadius: '2px',
                            cursor: 'pointer',
                            display: 'flex',
                            alignItems: 'center',
                            gap: '4px',
                          }}
                        >
                          <Edit2 size={10} />
                          EDIT
                        </button>
                        <button
                          onClick={() => handleDeleteConnectionClick(connection.id)}
                          style={{
                            padding: '6px 10px',
                            backgroundColor: 'transparent',
                            border: `1px solid ${FINCEPT.RED}`,
                            color: FINCEPT.RED,
                            fontSize: '9px',
                            fontWeight: 700,
                            borderRadius: '2px',
                            cursor: 'pointer',
                            display: 'flex',
                            alignItems: 'center',
                            gap: '4px',
                          }}
                        >
                          <Trash2 size={10} />
                          DELETE
                        </button>
                      </div>
                    </div>

                    {connection.lastTested && (
                      <div style={{ marginTop: '8px', fontSize: '9px', color: FINCEPT.MUTED }}>
                        Last tested: {new Date(connection.lastTested).toLocaleString()}
                      </div>
                    )}

                    {connection.errorMessage && (
                      <div
                        style={{
                          marginTop: '8px',
                          padding: '6px 8px',
                          backgroundColor: `${FINCEPT.RED}15`,
                          border: `1px solid ${FINCEPT.RED}`,
                          borderRadius: '2px',
                          fontSize: '9px',
                          color: FINCEPT.RED,
                        }}
                      >
                        Error: {connection.errorMessage}
                      </div>
                    )}
                  </div>
                );
              })
            )}
          </div>
        )}
      </div>

      {/* Configuration Modal */}
      {showConfigModal && selectedDataSource && (
        <div
          style={{
            position: 'fixed',
            top: 0,
            left: 0,
            right: 0,
            bottom: 0,
            backgroundColor: 'rgba(0,0,0,0.85)',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            zIndex: 9999,
          }}
          onClick={() => setShowConfigModal(false)}
        >
          <div
            style={{
              backgroundColor: FINCEPT.HEADER_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              width: '90%',
              maxWidth: '500px',
              maxHeight: '80vh',
              overflow: 'auto',
            }}
            onClick={(e) => e.stopPropagation()}
          >
            {/* Modal Header */}
            <div
              style={{
                padding: '12px 16px',
                borderBottom: `1px solid ${FINCEPT.BORDER}`,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
              }}
            >
              <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
                <span style={{ fontSize: '24px' }}>{selectedDataSource.icon}</span>
                <div>
                  <h2
                    style={{
                      color: FINCEPT.ORANGE,
                      fontSize: '12px',
                      fontWeight: 700,
                      margin: 0,
                    }}
                  >
                    {editingConnection ? 'EDIT' : 'CONFIGURE'} {selectedDataSource.name.toUpperCase()}
                  </h2>
                  <p style={{ color: FINCEPT.GRAY, fontSize: '9px', margin: '4px 0 0 0' }}>
                    {selectedDataSource.description}
                  </p>
                </div>
              </div>
              <button
                onClick={() => setShowConfigModal(false)}
                style={{
                  background: 'transparent',
                  border: 'none',
                  color: FINCEPT.GRAY,
                  cursor: 'pointer',
                  padding: '4px',
                }}
              >
                <X size={16} />
              </button>
            </div>

            {/* Modal Body */}
            <div style={{ padding: '16px' }}>
              {saveError && (
                <div
                  style={{
                    marginBottom: '12px',
                    padding: '8px',
                    backgroundColor: `${FINCEPT.RED}15`,
                    border: `1px solid ${FINCEPT.RED}`,
                    borderRadius: '2px',
                    fontSize: '9px',
                    color: FINCEPT.RED,
                  }}
                >
                  {saveError}
                </div>
              )}

              {/* Connection Name */}
              <div style={{ marginBottom: '12px' }}>
                <label
                  style={{
                    display: 'block',
                    marginBottom: '6px',
                    fontSize: '9px',
                    fontWeight: 700,
                    color: FINCEPT.GRAY,
                    letterSpacing: '0.5px',
                  }}
                >
                  CONNECTION NAME *
                </label>
                <input
                  type="text"
                  placeholder={`My ${selectedDataSource.name} Connection`}
                  value={configFormData.name || ''}
                  onChange={(e) => setConfigFormData({ ...configFormData, name: e.target.value })}
                  style={{
                    width: '100%',
                    padding: '8px 10px',
                    backgroundColor: FINCEPT.DARK_BG,
                    color: FINCEPT.WHITE,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderRadius: '2px',
                    fontSize: '10px',
                    fontFamily: '"IBM Plex Mono", monospace',
                    outline: 'none',
                    boxSizing: 'border-box',
                  }}
                />
              </div>

              {/* Dynamic Fields */}
              {selectedDataSource.fields.map((field) => (
                <div key={field.name} style={{ marginBottom: '12px' }}>
                  <label
                    style={{
                      display: 'block',
                      marginBottom: '6px',
                      fontSize: '9px',
                      fontWeight: 700,
                      color: FINCEPT.GRAY,
                      letterSpacing: '0.5px',
                    }}
                  >
                    {field.label.toUpperCase()} {field.required && '*'}
                  </label>
                  {field.type === 'select' ? (
                    <select
                      value={configFormData[field.name] || field.defaultValue || ''}
                      onChange={(e) => setConfigFormData({ ...configFormData, [field.name]: e.target.value })}
                      style={{
                        width: '100%',
                        padding: '8px 10px',
                        backgroundColor: FINCEPT.DARK_BG,
                        color: FINCEPT.WHITE,
                        border: `1px solid ${FINCEPT.BORDER}`,
                        borderRadius: '2px',
                        fontSize: '10px',
                        fontFamily: '"IBM Plex Mono", monospace',
                        outline: 'none',
                        boxSizing: 'border-box',
                      }}
                    >
                      {field.options?.map((opt) => (
                        <option key={opt.value} value={opt.value}>
                          {opt.label}
                        </option>
                      ))}
                    </select>
                  ) : field.type === 'textarea' ? (
                    <textarea
                      placeholder={field.placeholder}
                      value={configFormData[field.name] || ''}
                      onChange={(e) => setConfigFormData({ ...configFormData, [field.name]: e.target.value })}
                      rows={4}
                      style={{
                        width: '100%',
                        padding: '8px 10px',
                        backgroundColor: FINCEPT.DARK_BG,
                        color: FINCEPT.WHITE,
                        border: `1px solid ${FINCEPT.BORDER}`,
                        borderRadius: '2px',
                        fontSize: '10px',
                        fontFamily: '"IBM Plex Mono", monospace',
                        outline: 'none',
                        resize: 'vertical',
                        boxSizing: 'border-box',
                      }}
                    />
                  ) : field.type === 'checkbox' ? (
                    <label style={{ display: 'flex', alignItems: 'center', gap: '8px', cursor: 'pointer' }}>
                      <input
                        type="checkbox"
                        checked={configFormData[field.name] || field.defaultValue || false}
                        onChange={(e) => setConfigFormData({ ...configFormData, [field.name]: e.target.checked })}
                        style={{ cursor: 'pointer' }}
                      />
                      <span style={{ color: FINCEPT.GRAY, fontSize: '10px' }}>{field.placeholder}</span>
                    </label>
                  ) : (
                    <input
                      type={field.type}
                      placeholder={field.placeholder}
                      value={configFormData[field.name] || ''}
                      onChange={(e) =>
                        setConfigFormData({
                          ...configFormData,
                          [field.name]: field.type === 'number' ? Number(e.target.value) : e.target.value,
                        })
                      }
                      style={{
                        width: '100%',
                        padding: '8px 10px',
                        backgroundColor: FINCEPT.DARK_BG,
                        color: FINCEPT.WHITE,
                        border: `1px solid ${FINCEPT.BORDER}`,
                        borderRadius: '2px',
                        fontSize: '10px',
                        fontFamily: '"IBM Plex Mono", monospace',
                        outline: 'none',
                        boxSizing: 'border-box',
                      }}
                    />
                  )}
                </div>
              ))}
            </div>

            {/* Modal Footer */}
            <div
              style={{
                padding: '12px 16px',
                borderTop: `1px solid ${FINCEPT.BORDER}`,
                display: 'flex',
                justifyContent: 'flex-end',
                gap: '8px',
              }}
            >
              <button
                onClick={() => setShowConfigModal(false)}
                style={{
                  padding: '6px 12px',
                  backgroundColor: 'transparent',
                  border: `1px solid ${FINCEPT.BORDER}`,
                  color: FINCEPT.GRAY,
                  fontSize: '9px',
                  fontWeight: 700,
                  borderRadius: '2px',
                  cursor: 'pointer',
                }}
              >
                CANCEL
              </button>
              <button
                onClick={handleSaveConnection}
                style={{
                  padding: '6px 12px',
                  backgroundColor: FINCEPT.ORANGE,
                  color: FINCEPT.DARK_BG,
                  border: 'none',
                  fontSize: '9px',
                  fontWeight: 700,
                  borderRadius: '2px',
                  cursor: 'pointer',
                }}
              >
                {editingConnection ? 'UPDATE CONNECTION' : 'SAVE CONNECTION'}
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Status Bar */}
      <div
        style={{
          backgroundColor: FINCEPT.HEADER_BG,
          borderTop: `1px solid ${FINCEPT.BORDER}`,
          padding: '4px 16px',
          fontSize: '9px',
          color: FINCEPT.GRAY,
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          flexShrink: 0,
        }}
      >
        <div style={{ display: 'flex', gap: '16px' }}>
          <span>SOURCES: {DATA_SOURCE_CONFIGS.length}</span>
          <span>CONNECTIONS: {connections.length}</span>
          <span>VIEW: {view.toUpperCase()}</span>
        </div>
        <span>
          CONNECTED: {connections.filter((c) => c.status === 'connected').length} | ADAPTERS AVAILABLE
        </span>
      </div>

      {/* CSS for spin animation */}
      <style>{`
        @keyframes spin {
          from { transform: rotate(0deg); }
          to { transform: rotate(360deg); }
        }
      `}</style>
    </div>
  );
}

export default function DataSourcesTab() {
  return (
    <ErrorBoundary name="DataSourcesTab" variant="minimal">
      <DataSourcesTabContent />
    </ErrorBoundary>
  );
}
