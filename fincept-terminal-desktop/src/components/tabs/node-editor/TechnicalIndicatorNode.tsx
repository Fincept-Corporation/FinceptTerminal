// TechnicalIndicatorNode.tsx - Node for calculating technical indicators
import React, { useState } from 'react';
import { Handle, Position } from 'reactflow';
import { Play, Settings, CheckCircle, AlertCircle, Loader, TrendingUp } from 'lucide-react';

interface TechnicalIndicatorNodeProps {
  id: string;
  data: {
    label: string;
    dataSource: 'yfinance' | 'csv' | 'json' | 'input';
    symbol?: string;
    period?: string;
    csvPath?: string;
    jsonData?: string;
    categories: string[]; // ['momentum', 'volume', 'volatility', 'trend', 'others']
    status: 'idle' | 'running' | 'completed' | 'error';
    result?: any;
    error?: string;
    onExecute?: (nodeId: string) => void;
    onParameterChange?: (params: Record<string, any>) => void;
  };
  selected: boolean;
}

const TechnicalIndicatorNode: React.FC<TechnicalIndicatorNodeProps> = ({ id, data, selected }) => {
  const [showSettings, setShowSettings] = useState(false);
  const [localParams, setLocalParams] = useState({
    dataSource: data.dataSource || 'yfinance',
    symbol: data.symbol || 'AAPL',
    period: data.period || '1y',
    csvPath: data.csvPath || '',
    jsonData: data.jsonData || '',
    categories: data.categories || ['momentum', 'volume', 'volatility', 'trend', 'others']
  });

  // Bloomberg colors
  const ORANGE = '#FFA500';
  const WHITE = '#FFFFFF';
  const GRAY = '#787878';
  const DARK_BG = '#0a0a0a';
  const PANEL_BG = '#1a1a1a';
  const BORDER = '#2d2d2d';
  const GREEN = '#10b981';
  const RED = '#ef4444';

  const getStatusColor = () => {
    switch (data.status) {
      case 'running': return ORANGE;
      case 'completed': return GREEN;
      case 'error': return RED;
      default: return GRAY;
    }
  };

  const getStatusIcon = () => {
    switch (data.status) {
      case 'running': return <Loader size={14} className="animate-spin" color={ORANGE} />;
      case 'completed': return <CheckCircle size={14} color={GREEN} />;
      case 'error': return <AlertCircle size={14} color={RED} />;
      default: return <TrendingUp size={14} color={GRAY} />;
    }
  };

  const handleParamChange = (key: string, value: any) => {
    const newParams = { ...localParams, [key]: value };
    setLocalParams(newParams);
    data.onParameterChange?.(newParams);
  };

  const handleCategoryToggle = (category: string) => {
    const newCategories = localParams.categories.includes(category)
      ? localParams.categories.filter(c => c !== category)
      : [...localParams.categories, category];
    handleParamChange('categories', newCategories);
  };

  const availableCategories = ['momentum', 'volume', 'volatility', 'trend', 'others'];

  return (
    <div style={{
      backgroundColor: PANEL_BG,
      border: `2px solid ${selected ? ORANGE : getStatusColor()}`,
      borderRadius: '8px',
      padding: '12px',
      minWidth: '280px',
      maxWidth: '350px',
      fontFamily: 'Consolas, monospace',
      boxShadow: selected ? `0 0 16px ${ORANGE}60` : `0 2px 8px rgba(0,0,0,0.3)`
    }}>
      {/* Input Handle - Can receive data from upstream nodes */}
      <Handle
        type="target"
        position={Position.Left}
        style={{
          background: ORANGE,
          width: '12px',
          height: '12px',
          border: `2px solid ${DARK_BG}`,
          left: '-6px'
        }}
      />

      {/* Header */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: '8px',
        marginBottom: '10px',
        paddingBottom: '8px',
        borderBottom: `1px solid ${BORDER}`
      }}>
        <span style={{ fontSize: '20px' }}>📊</span>
        <div style={{ flex: 1 }}>
          <div style={{
            color: WHITE,
            fontSize: '12px',
            fontWeight: 'bold',
            marginBottom: '2px'
          }}>
            {data.label}
          </div>
          <div style={{
            color: GRAY,
            fontSize: '9px',
            textTransform: 'uppercase'
          }}>
            TECHNICAL ANALYSIS
          </div>
        </div>
        {getStatusIcon()}
      </div>

      {/* Data Source Summary */}
      <div style={{
        backgroundColor: DARK_BG,
        border: `1px solid ${BORDER}`,
        borderRadius: '4px',
        padding: '6px',
        marginBottom: '8px'
      }}>
        <div style={{
          color: ORANGE,
          fontSize: '9px',
          fontWeight: 'bold',
          marginBottom: '4px'
        }}>
          DATA SOURCE
        </div>
        <div style={{
          color: GRAY,
          fontSize: '8px',
          display: 'flex',
          justifyContent: 'space-between',
          marginBottom: '2px'
        }}>
          <span>Type:</span>
          <span style={{ color: WHITE }}>{localParams.dataSource.toUpperCase()}</span>
        </div>
        {localParams.dataSource === 'yfinance' && (
          <>
            <div style={{
              color: GRAY,
              fontSize: '8px',
              display: 'flex',
              justifyContent: 'space-between',
              marginBottom: '2px'
            }}>
              <span>Symbol:</span>
              <span style={{ color: WHITE }}>{localParams.symbol}</span>
            </div>
            <div style={{
              color: GRAY,
              fontSize: '8px',
              display: 'flex',
              justifyContent: 'space-between'
            }}>
              <span>Period:</span>
              <span style={{ color: WHITE }}>{localParams.period}</span>
            </div>
          </>
        )}
        {localParams.dataSource === 'csv' && (
          <div style={{
            color: GRAY,
            fontSize: '8px',
            display: 'flex',
            justifyContent: 'space-between'
          }}>
            <span>File:</span>
            <span style={{ color: WHITE, maxWidth: '150px', overflow: 'hidden', textOverflow: 'ellipsis' }}>
              {localParams.csvPath || 'Not set'}
            </span>
          </div>
        )}
        {localParams.dataSource === 'input' && (
          <div style={{
            color: GRAY,
            fontSize: '8px'
          }}>
            Waiting for upstream data...
          </div>
        )}
      </div>

      {/* Categories Summary */}
      <div style={{
        backgroundColor: DARK_BG,
        border: `1px solid ${BORDER}`,
        borderRadius: '4px',
        padding: '6px',
        marginBottom: '8px'
      }}>
        <div style={{
          color: ORANGE,
          fontSize: '9px',
          fontWeight: 'bold',
          marginBottom: '4px'
        }}>
          INDICATORS ({localParams.categories.length}/5)
        </div>
        <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
          {localParams.categories.map(cat => (
            <span key={cat} style={{
              backgroundColor: `${ORANGE}30`,
              border: `1px solid ${ORANGE}`,
              color: ORANGE,
              fontSize: '7px',
              padding: '2px 6px',
              borderRadius: '3px',
              textTransform: 'uppercase'
            }}>
              {cat}
            </span>
          ))}
        </div>
      </div>

      {/* Result Preview */}
      {data.status === 'completed' && data.result && (
        <div style={{
          backgroundColor: `${GREEN}15`,
          border: `1px solid ${GREEN}`,
          borderRadius: '4px',
          padding: '6px',
          marginBottom: '8px'
        }}>
          <div style={{
            color: GREEN,
            fontSize: '9px',
            fontWeight: 'bold',
            marginBottom: '4px',
            display: 'flex',
            alignItems: 'center',
            gap: '4px'
          }}>
            <CheckCircle size={10} />
            RESULT
          </div>
          <div style={{
            color: WHITE,
            fontSize: '8px',
            fontFamily: 'monospace'
          }}>
            {Array.isArray(data.result)
              ? `${data.result.length} data points calculated`
              : 'Analysis complete'}
          </div>
        </div>
      )}

      {/* Error Display */}
      {data.status === 'error' && data.error && (
        <div style={{
          backgroundColor: `${RED}15`,
          border: `1px solid ${RED}`,
          borderRadius: '4px',
          padding: '6px',
          marginBottom: '8px'
        }}>
          <div style={{
            color: RED,
            fontSize: '9px',
            fontWeight: 'bold',
            marginBottom: '4px',
            display: 'flex',
            alignItems: 'center',
            gap: '4px'
          }}>
            <AlertCircle size={10} />
            ERROR
          </div>
          <div style={{
            color: RED,
            fontSize: '8px',
            wordBreak: 'break-word'
          }}>
            {data.error}
          </div>
        </div>
      )}

      {/* Settings Panel */}
      {showSettings && (
        <div style={{
          backgroundColor: DARK_BG,
          border: `1px solid ${ORANGE}`,
          borderRadius: '4px',
          padding: '8px',
          marginBottom: '8px',
          maxHeight: '300px',
          overflowY: 'auto'
        }}>
          <div style={{
            color: ORANGE,
            fontSize: '10px',
            fontWeight: 'bold',
            marginBottom: '8px'
          }}>
            CONFIGURATION
          </div>

          {/* Data Source Selection */}
          <div style={{ marginBottom: '8px' }}>
            <label style={{
              color: GRAY,
              fontSize: '8px',
              display: 'block',
              marginBottom: '4px'
            }}>
              Data Source
            </label>
            <select
              value={localParams.dataSource}
              onChange={(e) => handleParamChange('dataSource', e.target.value)}
              style={{
                width: '100%',
                backgroundColor: PANEL_BG,
                border: `1px solid ${BORDER}`,
                color: WHITE,
                padding: '4px',
                fontSize: '8px',
                borderRadius: '3px'
              }}
            >
              <option value="yfinance">Yahoo Finance</option>
              <option value="csv">CSV File</option>
              <option value="json">JSON Data</option>
              <option value="input">Upstream Input</option>
            </select>
          </div>

          {/* YFinance Params */}
          {localParams.dataSource === 'yfinance' && (
            <>
              <div style={{ marginBottom: '8px' }}>
                <label style={{
                  color: GRAY,
                  fontSize: '8px',
                  display: 'block',
                  marginBottom: '4px'
                }}>
                  Symbol
                </label>
                <input
                  type="text"
                  value={localParams.symbol}
                  onChange={(e) => handleParamChange('symbol', e.target.value)}
                  placeholder="AAPL"
                  style={{
                    width: '100%',
                    backgroundColor: PANEL_BG,
                    border: `1px solid ${BORDER}`,
                    color: WHITE,
                    padding: '4px',
                    fontSize: '8px',
                    borderRadius: '3px'
                  }}
                />
              </div>
              <div style={{ marginBottom: '8px' }}>
                <label style={{
                  color: GRAY,
                  fontSize: '8px',
                  display: 'block',
                  marginBottom: '4px'
                }}>
                  Period
                </label>
                <select
                  value={localParams.period}
                  onChange={(e) => handleParamChange('period', e.target.value)}
                  style={{
                    width: '100%',
                    backgroundColor: PANEL_BG,
                    border: `1px solid ${BORDER}`,
                    color: WHITE,
                    padding: '4px',
                    fontSize: '8px',
                    borderRadius: '3px'
                  }}
                >
                  <option value="1d">1 Day</option>
                  <option value="5d">5 Days</option>
                  <option value="1mo">1 Month</option>
                  <option value="3mo">3 Months</option>
                  <option value="6mo">6 Months</option>
                  <option value="1y">1 Year</option>
                  <option value="2y">2 Years</option>
                  <option value="5y">5 Years</option>
                  <option value="max">Max</option>
                </select>
              </div>
            </>
          )}

          {/* CSV Path */}
          {localParams.dataSource === 'csv' && (
            <div style={{ marginBottom: '8px' }}>
              <label style={{
                color: GRAY,
                fontSize: '8px',
                display: 'block',
                marginBottom: '4px'
              }}>
                CSV File Path
              </label>
              <input
                type="text"
                value={localParams.csvPath}
                onChange={(e) => handleParamChange('csvPath', e.target.value)}
                placeholder="/path/to/data.csv"
                style={{
                  width: '100%',
                  backgroundColor: PANEL_BG,
                  border: `1px solid ${BORDER}`,
                  color: WHITE,
                  padding: '4px',
                  fontSize: '8px',
                  borderRadius: '3px'
                }}
              />
            </div>
          )}

          {/* Categories Selection */}
          <div style={{ marginBottom: '8px' }}>
            <label style={{
              color: GRAY,
              fontSize: '8px',
              display: 'block',
              marginBottom: '6px'
            }}>
              Indicator Categories
            </label>
            {availableCategories.map(category => (
              <label key={category} style={{
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
                marginBottom: '6px',
                cursor: 'pointer'
              }}>
                <input
                  type="checkbox"
                  checked={localParams.categories.includes(category)}
                  onChange={() => handleCategoryToggle(category)}
                  style={{
                    accentColor: ORANGE
                  }}
                />
                <span style={{
                  color: WHITE,
                  fontSize: '8px',
                  textTransform: 'capitalize'
                }}>
                  {category}
                </span>
              </label>
            ))}
          </div>

          <button
            onClick={(e) => {
              e.stopPropagation();
              setShowSettings(false);
            }}
            style={{
              width: '100%',
              backgroundColor: ORANGE,
              color: 'black',
              border: 'none',
              padding: '6px',
              fontSize: '9px',
              fontWeight: 'bold',
              cursor: 'pointer',
              borderRadius: '3px'
            }}
          >
            CLOSE
          </button>
        </div>
      )}

      {/* Action Buttons */}
      <div style={{ display: 'flex', gap: '6px' }}>
        <button
          onClick={() => data.onExecute?.(id)}
          disabled={data.status === 'running'}
          style={{
            flex: 1,
            backgroundColor: data.status === 'running' ? GRAY : ORANGE,
            color: 'black',
            border: 'none',
            padding: '8px',
            fontSize: '10px',
            fontWeight: 'bold',
            cursor: data.status === 'running' ? 'not-allowed' : 'pointer',
            borderRadius: '4px',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '4px',
            opacity: data.status === 'running' ? 0.6 : 1
          }}
        >
          {data.status === 'running' ? (
            <>
              <Loader size={12} className="animate-spin" />
              RUNNING...
            </>
          ) : (
            <>
              <Play size={12} />
              CALCULATE
            </>
          )}
        </button>
        <button
          onClick={() => setShowSettings(!showSettings)}
          style={{
            backgroundColor: showSettings ? ORANGE : DARK_BG,
            border: `1px solid ${BORDER}`,
            color: showSettings ? 'black' : GRAY,
            padding: '8px',
            fontSize: '10px',
            cursor: 'pointer',
            borderRadius: '4px',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center'
          }}
        >
          <Settings size={14} />
        </button>
      </div>

      {/* Output Handle */}
      <Handle
        type="source"
        position={Position.Right}
        style={{
          background: data.status === 'completed' ? GREEN : ORANGE,
          width: '12px',
          height: '12px',
          border: `2px solid ${DARK_BG}`,
          right: '-6px'
        }}
      />
    </div>
  );
};

export default TechnicalIndicatorNode;
