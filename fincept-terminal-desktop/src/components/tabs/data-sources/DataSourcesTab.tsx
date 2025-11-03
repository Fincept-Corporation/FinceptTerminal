import React, { useState } from 'react';
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
} from 'lucide-react';
import { DATA_SOURCE_CONFIGS } from './dataSourceConfigs';
import { DataSourceConfig, DataSourceConnection, DataSourceCategory } from './types';
import { hasAdapter } from './adapters';
import { useDataSources } from '../../../contexts/DataSourceContext';

export default function DataSourcesTab() {
  const {
    connections,
    addConnection,
    updateConnection,
    deleteConnection,
    testConnection,
  } = useDataSources();

  const [view, setView] = useState<'gallery' | 'connections'>('gallery');
  const [searchTerm, setSearchTerm] = useState('');
  const [selectedCategory, setSelectedCategory] = useState<DataSourceCategory | 'all'>('all');
  const [showConfigModal, setShowConfigModal] = useState(false);
  const [selectedDataSource, setSelectedDataSource] = useState<DataSourceConfig | null>(null);
  const [configFormData, setConfigFormData] = useState<Record<string, any>>({});
  const [editingConnection, setEditingConnection] = useState<DataSourceConnection | null>(null);

  // Category icons
  const getCategoryIcon = (category: DataSourceCategory) => {
    switch (category) {
      case 'database':
        return <Database size={16} />;
      case 'api':
        return <Globe size={16} />;
      case 'file':
        return <FileText size={16} />;
      case 'streaming':
        return <Activity size={16} />;
      case 'cloud':
        return <Cloud size={16} />;
      case 'timeseries':
        return <TrendingUp size={16} />;
      case 'market-data':
        return <TrendingUp size={16} />;
      default:
        return <Database size={16} />;
    }
  };

  // Filter data sources
  const filteredDataSources = DATA_SOURCE_CONFIGS.filter((ds) => {
    const matchesSearch = ds.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
      ds.description.toLowerCase().includes(searchTerm.toLowerCase());
    const matchesCategory = selectedCategory === 'all' || ds.category === selectedCategory;
    return matchesSearch && matchesCategory;
  });

  // Handle open config modal
  const handleOpenConfig = (dataSource: DataSourceConfig) => {
    setSelectedDataSource(dataSource);
    setEditingConnection(null);
    // Initialize form with default values
    const initialData: Record<string, any> = {};
    dataSource.fields.forEach((field) => {
      if (field.defaultValue !== undefined) {
        initialData[field.name] = field.defaultValue;
      }
    });
    setConfigFormData(initialData);
    setShowConfigModal(true);
  };

  // Handle edit connection
  const handleEditConnection = (connection: DataSourceConnection) => {
    const dataSource = DATA_SOURCE_CONFIGS.find((ds) => ds.type === connection.type);
    if (!dataSource) return;

    setSelectedDataSource(dataSource);
    setEditingConnection(connection);
    setConfigFormData({ ...connection.config, name: connection.name });
    setShowConfigModal(true);
  };

  // Handle save connection
  const handleSaveConnection = () => {
    if (!selectedDataSource) return;

    if (editingConnection) {
      // Update existing connection
      updateConnection(editingConnection.id, {
        name: configFormData.name || editingConnection.name,
        config: { ...configFormData },
      });

      console.log('Updated connection:', editingConnection.id);
    } else {
      // Create new connection
      const newConnection: DataSourceConnection = {
        id: `conn_${Date.now()}`,
        name: configFormData.name || `${selectedDataSource.name} Connection`,
        type: selectedDataSource.type,
        category: selectedDataSource.category,
        config: { ...configFormData },
        status: 'disconnected',
        createdAt: new Date().toISOString(),
        updatedAt: new Date().toISOString(),
      };

      addConnection(newConnection);
      console.log('Saved connection:', newConnection);
    }

    setShowConfigModal(false);
    setSelectedDataSource(null);
    setConfigFormData({});
    setEditingConnection(null);
    setView('connections');
  };

  // Handle delete connection
  const handleDeleteConnectionClick = (id: string) => {
    deleteConnection(id);
  };

  // Handle test connection
  const handleTestConnection = async (connection: DataSourceConnection) => {
    console.log('Testing connection:', connection);

    // Update status to testing
    updateConnection(connection.id, {
      status: 'testing',
      lastTested: new Date().toISOString(),
    });

    // Test connection (context will handle status updates)
    await testConnection(connection);
  };

  return (
    <div
      style={{
        width: '100%',
        height: '100%',
        backgroundColor: '#0a0a0a',
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
      }}
    >
      {/* Header */}
      <div
        style={{
          backgroundColor: '#1a1a1a',
          borderBottom: '1px solid #2d2d2d',
          padding: '12px 16px',
          flexShrink: 0,
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '12px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <span
              style={{
                color: '#ea580c',
                fontSize: '14px',
                fontWeight: 'bold',
                letterSpacing: '0.5px',
              }}
            >
              DATA SOURCES
            </span>
            <div style={{ width: '1px', height: '20px', backgroundColor: '#404040' }}></div>
            <span style={{ color: '#737373', fontSize: '11px' }}>
              {DATA_SOURCE_CONFIGS.length} Available | {connections.length} Connected
            </span>
          </div>

          {/* View Switcher */}
          <div style={{ display: 'flex', gap: '8px' }}>
            <button
              onClick={() => setView('gallery')}
              style={{
                backgroundColor: view === 'gallery' ? '#ea580c' : 'transparent',
                color: view === 'gallery' ? 'white' : '#a3a3a3',
                border: `1px solid ${view === 'gallery' ? '#ea580c' : '#404040'}`,
                padding: '6px 12px',
                fontSize: '11px',
                cursor: 'pointer',
                borderRadius: '3px',
                fontWeight: 'bold',
              }}
            >
              GALLERY
            </button>
            <button
              onClick={() => setView('connections')}
              style={{
                backgroundColor: view === 'connections' ? '#ea580c' : 'transparent',
                color: view === 'connections' ? 'white' : '#a3a3a3',
                border: `1px solid ${view === 'connections' ? '#ea580c' : '#404040'}`,
                padding: '6px 12px',
                fontSize: '11px',
                cursor: 'pointer',
                borderRadius: '3px',
                fontWeight: 'bold',
              }}
            >
              MY CONNECTIONS ({connections.length})
            </button>
          </div>
        </div>

        {/* Search and Filter Bar */}
        {view === 'gallery' && (
          <div style={{ display: 'flex', gap: '12px', alignItems: 'center' }}>
            <div style={{ position: 'relative', flex: 1 }}>
              <Search
                size={16}
                style={{
                  position: 'absolute',
                  left: '10px',
                  top: '50%',
                  transform: 'translateY(-50%)',
                  color: '#737373',
                }}
              />
              <input
                type="text"
                placeholder="Search data sources..."
                value={searchTerm}
                onChange={(e) => setSearchTerm(e.target.value)}
                style={{
                  width: '100%',
                  backgroundColor: '#0a0a0a',
                  border: '1px solid #2d2d2d',
                  color: '#a3a3a3',
                  padding: '8px 12px 8px 36px',
                  fontSize: '11px',
                  borderRadius: '4px',
                  outline: 'none',
                }}
              />
            </div>

            {/* Category Filter */}
            <div style={{ display: 'flex', gap: '6px', flexWrap: 'wrap' }}>
              {['all', 'database', 'api', 'file', 'streaming', 'cloud', 'timeseries', 'market-data'].map((cat) => (
                <button
                  key={cat}
                  onClick={() => setSelectedCategory(cat as any)}
                  style={{
                    backgroundColor: selectedCategory === cat ? '#2d2d2d' : 'transparent',
                    color: selectedCategory === cat ? '#ea580c' : '#737373',
                    border: `1px solid ${selectedCategory === cat ? '#ea580c' : '#2d2d2d'}`,
                    padding: '6px 10px',
                    fontSize: '10px',
                    cursor: 'pointer',
                    borderRadius: '3px',
                    textTransform: 'uppercase',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '4px',
                  }}
                >
                  {cat !== 'all' && getCategoryIcon(cat as DataSourceCategory)}
                  {cat.replace('-', ' ')}
                </button>
              ))}
            </div>
          </div>
        )}
      </div>

      {/* Content Area */}
      <div
        style={{
          flex: 1,
          overflow: 'auto',
          padding: '16px',
          scrollbarWidth: 'none',
          msOverflowStyle: 'none',
        }}
        className="hide-scrollbar"
      >
        <style>{`
          .hide-scrollbar::-webkit-scrollbar {
            display: none;
          }
        `}</style>
        {view === 'gallery' ? (
          /* Data Source Gallery */
          <div
            style={{
              display: 'grid',
              gridTemplateColumns: 'repeat(auto-fill, minmax(280px, 1fr))',
              gap: '16px',
            }}
          >
            {filteredDataSources.map((dataSource) => (
              <div
                key={dataSource.id}
                style={{
                  backgroundColor: '#1a1a1a',
                  border: '1px solid #2d2d2d',
                  borderRadius: '6px',
                  padding: '16px',
                  cursor: 'pointer',
                  transition: 'all 0.2s',
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.borderColor = dataSource.color;
                  e.currentTarget.style.boxShadow = `0 0 12px ${dataSource.color}40`;
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.borderColor = '#2d2d2d';
                  e.currentTarget.style.boxShadow = 'none';
                }}
                onClick={() => handleOpenConfig(dataSource)}
              >
                <div style={{ display: 'flex', alignItems: 'flex-start', gap: '12px', marginBottom: '12px' }}>
                  <span style={{ fontSize: '32px' }}>{dataSource.icon}</span>
                  <div style={{ flex: 1 }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '4px' }}>
                      <h3
                        style={{
                          color: dataSource.color,
                          fontSize: '13px',
                          fontWeight: 'bold',
                        }}
                      >
                        {dataSource.name}
                      </h3>
                      {hasAdapter(dataSource.type) && (
                        <span
                          style={{
                            backgroundColor: '#10b98120',
                            color: '#10b981',
                            padding: '2px 6px',
                            fontSize: '8px',
                            borderRadius: '3px',
                            fontWeight: 'bold',
                          }}
                        >
                          ✓ READY
                        </span>
                      )}
                    </div>
                    <div
                      style={{
                        display: 'inline-block',
                        backgroundColor: `${dataSource.color}20`,
                        color: dataSource.color,
                        padding: '2px 8px',
                        fontSize: '9px',
                        borderRadius: '3px',
                        textTransform: 'uppercase',
                        fontWeight: 'bold',
                      }}
                    >
                      {dataSource.category}
                    </div>
                  </div>
                </div>
                <p
                  style={{
                    color: '#a3a3a3',
                    fontSize: '11px',
                    lineHeight: '1.5',
                    marginBottom: '12px',
                  }}
                >
                  {dataSource.description}
                </p>
                <div style={{ display: 'flex', gap: '8px', fontSize: '10px', color: '#737373' }}>
                  {dataSource.requiresAuth && (
                    <span
                      style={{
                        display: 'flex',
                        alignItems: 'center',
                        gap: '4px',
                      }}
                    >
                      <Settings size={12} /> Auth Required
                    </span>
                  )}
                  {dataSource.testable && (
                    <span
                      style={{
                        display: 'flex',
                        alignItems: 'center',
                        gap: '4px',
                      }}
                    >
                      <Check size={12} /> Testable
                    </span>
                  )}
                </div>
              </div>
            ))}
          </div>
        ) : (
          /* Connections List */
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            {connections.length === 0 ? (
              <div
                style={{
                  textAlign: 'center',
                  padding: '60px 20px',
                  color: '#737373',
                }}
              >
                <Database size={48} style={{ marginBottom: '16px', opacity: 0.3 }} />
                <h3 style={{ fontSize: '14px', marginBottom: '8px', color: '#a3a3a3' }}>
                  No connections configured
                </h3>
                <p style={{ fontSize: '11px', marginBottom: '16px' }}>
                  Add your first data source connection to get started
                </p>
                <button
                  onClick={() => setView('gallery')}
                  style={{
                    backgroundColor: '#ea580c',
                    color: 'white',
                    border: 'none',
                    padding: '8px 16px',
                    fontSize: '11px',
                    cursor: 'pointer',
                    borderRadius: '4px',
                    fontWeight: 'bold',
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
                      backgroundColor: '#1a1a1a',
                      border: '1px solid #2d2d2d',
                      borderRadius: '6px',
                      padding: '16px',
                    }}
                  >
                    <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                        <span style={{ fontSize: '24px' }}>{config?.icon}</span>
                        <div>
                          <h3 style={{ color: '#a3a3a3', fontSize: '13px', fontWeight: 'bold', marginBottom: '4px' }}>
                            {connection.name}
                          </h3>
                          <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
                            <span
                              style={{
                                display: 'inline-block',
                                backgroundColor: `${config?.color}20`,
                                color: config?.color,
                                padding: '2px 8px',
                                fontSize: '9px',
                                borderRadius: '3px',
                                textTransform: 'uppercase',
                                fontWeight: 'bold',
                              }}
                            >
                              {connection.type}
                            </span>
                            <span
                              style={{
                                display: 'flex',
                                alignItems: 'center',
                                gap: '4px',
                                fontSize: '10px',
                                color:
                                  connection.status === 'connected'
                                    ? '#10b981'
                                    : connection.status === 'error'
                                    ? '#ef4444'
                                    : connection.status === 'testing'
                                    ? '#f59e0b'
                                    : '#737373',
                              }}
                            >
                              {connection.status === 'connected' && <Check size={12} />}
                              {connection.status === 'error' && <X size={12} />}
                              {connection.status === 'testing' && <RefreshCw size={12} className="animate-spin" />}
                              {connection.status === 'disconnected' && <AlertCircle size={12} />}
                              {connection.status.toUpperCase()}
                            </span>
                          </div>
                        </div>
                      </div>

                      <div style={{ display: 'flex', gap: '8px' }}>
                        {config?.testable && (
                          <button
                            onClick={() => handleTestConnection(connection)}
                            disabled={connection.status === 'testing'}
                            style={{
                              backgroundColor: 'transparent',
                              color: '#10b981',
                              border: '1px solid #10b981',
                              padding: '6px 12px',
                              fontSize: '10px',
                              cursor: connection.status === 'testing' ? 'not-allowed' : 'pointer',
                              borderRadius: '3px',
                              display: 'flex',
                              alignItems: 'center',
                              gap: '6px',
                              opacity: connection.status === 'testing' ? 0.5 : 1,
                            }}
                          >
                            <RefreshCw size={12} />
                            TEST
                          </button>
                        )}
                        <button
                          onClick={() => handleEditConnection(connection)}
                          style={{
                            backgroundColor: 'transparent',
                            color: '#a3a3a3',
                            border: '1px solid #404040',
                            padding: '6px 12px',
                            fontSize: '10px',
                            cursor: 'pointer',
                            borderRadius: '3px',
                            display: 'flex',
                            alignItems: 'center',
                            gap: '6px',
                          }}
                        >
                          <Edit2 size={12} />
                          EDIT
                        </button>
                        <button
                          onClick={() => handleDeleteConnectionClick(connection.id)}
                          style={{
                            backgroundColor: 'transparent',
                            color: '#ef4444',
                            border: '1px solid #ef4444',
                            padding: '6px 12px',
                            fontSize: '10px',
                            cursor: 'pointer',
                            borderRadius: '3px',
                            display: 'flex',
                            alignItems: 'center',
                            gap: '6px',
                          }}
                        >
                          <Trash2 size={12} />
                          DELETE
                        </button>
                      </div>
                    </div>

                    {connection.lastTested && (
                      <div style={{ marginTop: '12px', fontSize: '10px', color: '#737373' }}>
                        Last tested: {new Date(connection.lastTested).toLocaleString()}
                      </div>
                    )}

                    {connection.errorMessage && (
                      <div
                        style={{
                          marginTop: '12px',
                          padding: '8px',
                          backgroundColor: '#ef444420',
                          border: '1px solid #ef4444',
                          borderRadius: '4px',
                          fontSize: '10px',
                          color: '#ef4444',
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
            backgroundColor: 'rgba(0,0,0,0.8)',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            zIndex: 9999,
          }}
          onClick={() => setShowConfigModal(false)}
        >
          <div
            style={{
              backgroundColor: '#1a1a1a',
              border: '1px solid #2d2d2d',
              borderRadius: '8px',
              width: '90%',
              maxWidth: '600px',
              maxHeight: '80vh',
              overflow: 'auto',
              scrollbarWidth: 'none',
              msOverflowStyle: 'none',
            }}
            className="hide-scrollbar-modal"
            onClick={(e) => e.stopPropagation()}
          >
            <style>{`
              .hide-scrollbar-modal::-webkit-scrollbar {
                display: none;
              }
            `}</style>
            {/* Modal Header */}
            <div
              style={{
                padding: '16px',
                borderBottom: '1px solid #2d2d2d',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
              }}
            >
              <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                <span style={{ fontSize: '32px' }}>{selectedDataSource.icon}</span>
                <div>
                  <h2 style={{ color: selectedDataSource.color, fontSize: '16px', fontWeight: 'bold' }}>
                    {editingConnection ? 'Edit' : 'Configure'} {selectedDataSource.name}
                  </h2>
                  <p style={{ color: '#737373', fontSize: '11px' }}>{selectedDataSource.description}</p>
                </div>
              </div>
              <button
                onClick={() => setShowConfigModal(false)}
                style={{
                  background: 'transparent',
                  border: 'none',
                  color: '#a3a3a3',
                  cursor: 'pointer',
                  padding: '4px',
                }}
              >
                <X size={20} />
              </button>
            </div>

            {/* Modal Body */}
            <div style={{ padding: '16px' }}>
              {/* Connection Name */}
              <div style={{ marginBottom: '16px' }}>
                <label style={{ color: '#a3a3a3', fontSize: '11px', display: 'block', marginBottom: '6px' }}>
                  Connection Name *
                </label>
                <input
                  type="text"
                  placeholder={`My ${selectedDataSource.name} Connection`}
                  value={configFormData.name || ''}
                  onChange={(e) => setConfigFormData({ ...configFormData, name: e.target.value })}
                  style={{
                    width: '100%',
                    backgroundColor: '#0a0a0a',
                    border: '1px solid #2d2d2d',
                    color: '#a3a3a3',
                    padding: '8px 12px',
                    fontSize: '11px',
                    borderRadius: '4px',
                    outline: 'none',
                  }}
                />
              </div>

              {/* Dynamic Fields */}
              {selectedDataSource.fields.map((field) => (
                <div key={field.name} style={{ marginBottom: '16px' }}>
                  <label style={{ color: '#a3a3a3', fontSize: '11px', display: 'block', marginBottom: '6px' }}>
                    {field.label} {field.required && '*'}
                  </label>
                  {field.type === 'select' ? (
                    <select
                      value={configFormData[field.name] || field.defaultValue || ''}
                      onChange={(e) => setConfigFormData({ ...configFormData, [field.name]: e.target.value })}
                      style={{
                        width: '100%',
                        backgroundColor: '#0a0a0a',
                        border: '1px solid #2d2d2d',
                        color: '#a3a3a3',
                        padding: '8px 12px',
                        fontSize: '11px',
                        borderRadius: '4px',
                        outline: 'none',
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
                        backgroundColor: '#0a0a0a',
                        border: '1px solid #2d2d2d',
                        color: '#a3a3a3',
                        padding: '8px 12px',
                        fontSize: '11px',
                        borderRadius: '4px',
                        outline: 'none',
                        fontFamily: 'monospace',
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
                      <span style={{ color: '#737373', fontSize: '11px' }}>{field.placeholder}</span>
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
                        backgroundColor: '#0a0a0a',
                        border: '1px solid #2d2d2d',
                        color: '#a3a3a3',
                        padding: '8px 12px',
                        fontSize: '11px',
                        borderRadius: '4px',
                        outline: 'none',
                      }}
                    />
                  )}
                </div>
              ))}
            </div>

            {/* Modal Footer */}
            <div
              style={{
                padding: '16px',
                borderTop: '1px solid #2d2d2d',
                display: 'flex',
                justifyContent: 'flex-end',
                gap: '12px',
              }}
            >
              <button
                onClick={() => setShowConfigModal(false)}
                style={{
                  backgroundColor: 'transparent',
                  color: '#a3a3a3',
                  border: '1px solid #404040',
                  padding: '8px 16px',
                  fontSize: '11px',
                  cursor: 'pointer',
                  borderRadius: '4px',
                  fontWeight: 'bold',
                }}
              >
                CANCEL
              </button>
              <button
                onClick={handleSaveConnection}
                style={{
                  backgroundColor: '#ea580c',
                  color: 'white',
                  border: 'none',
                  padding: '8px 16px',
                  fontSize: '11px',
                  cursor: 'pointer',
                  borderRadius: '4px',
                  fontWeight: 'bold',
                }}
              >
                {editingConnection ? 'UPDATE CONNECTION' : 'SAVE CONNECTION'}
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
