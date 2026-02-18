// WidgetSettingsModal - Edit widget configuration after creation
// Allows users to modify widget settings without removing and re-adding

import React, { useState, useEffect } from 'react';
import { X, Save } from 'lucide-react';
import { WidgetType } from './widgets';
import { portfolioService } from '@/services/portfolio/portfolioService';

const FC = {
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

interface WidgetSettingsModalProps {
  isOpen: boolean;
  widgetId: string;
  widgetType: WidgetType;
  currentConfig: any;
  onClose: () => void;
  onSave: (widgetId: string, newConfig: any) => void;
  availableWatchlists?: Array<{ id: string; name: string }>;
  availableDataSources?: Array<{ alias: string; display_name: string }>;
}

export const WidgetSettingsModal: React.FC<WidgetSettingsModalProps> = ({
  isOpen,
  widgetId,
  widgetType,
  currentConfig,
  onClose,
  onSave,
  availableWatchlists = [],
  availableDataSources = []
}) => {
  const [config, setConfig] = useState(currentConfig || {});
  const [portfolios, setPortfolios] = useState<Array<{ id: string; name: string }>>([]);

  useEffect(() => {
    setConfig(currentConfig || {});
  }, [currentConfig, isOpen]);

  useEffect(() => {
    if (isOpen && widgetType === 'performance') {
      portfolioService.initialize().then(() =>
        portfolioService.getPortfolios()
      ).then(ps => setPortfolios(ps.map(p => ({ id: p.id, name: p.name }))))
       .catch(() => setPortfolios([]));
    }
  }, [isOpen, widgetType]);

  if (!isOpen) return null;

  const handleSave = () => {
    onSave(widgetId, config);
    onClose();
  };

  const renderConfigForm = () => {
    switch (widgetType) {
      case 'news':
        return (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            <div>
              <label style={{ fontSize: '9px', color: FC.GRAY, fontWeight: 700, display: 'block', marginBottom: '4px' }}>
                NEWS CATEGORY
              </label>
              <select
                value={config.newsCategory || 'ALL'}
                onChange={(e) => setConfig({ ...config, newsCategory: e.target.value })}
                style={{
                  width: '100%',
                  padding: '6px 8px',
                  backgroundColor: FC.DARK_BG,
                  border: `1px solid ${FC.BORDER}`,
                  color: FC.WHITE,
                  fontSize: '10px',
                  borderRadius: '2px',
                  fontFamily: '"IBM Plex Mono", monospace',
                }}
              >
                <option value="ALL">ALL NEWS</option>
                <option value="MARKETS">MARKETS</option>
                <option value="TECHNOLOGY">TECHNOLOGY</option>
                <option value="CRYPTO">CRYPTO</option>
                <option value="COMMODITIES">COMMODITIES</option>
              </select>
            </div>
            <div>
              <label style={{ fontSize: '9px', color: FC.GRAY, fontWeight: 700, display: 'block', marginBottom: '4px' }}>
                ARTICLES LIMIT
              </label>
              <input
                type="number"
                min="3"
                max="20"
                value={config.newsLimit || 5}
                onChange={(e) => setConfig({ ...config, newsLimit: parseInt(e.target.value) })}
                style={{
                  width: '100%',
                  padding: '6px 8px',
                  backgroundColor: FC.DARK_BG,
                  border: `1px solid ${FC.BORDER}`,
                  color: FC.WHITE,
                  fontSize: '10px',
                  borderRadius: '2px',
                  fontFamily: '"IBM Plex Mono", monospace',
                }}
              />
            </div>
          </div>
        );

      case 'market':
        return (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            <div>
              <label style={{ fontSize: '9px', color: FC.GRAY, fontWeight: 700, display: 'block', marginBottom: '4px' }}>
                MARKET CATEGORY
              </label>
              <select
                value={config.marketCategory || 'Indices'}
                onChange={(e) => {
                  const category = e.target.value;
                  const tickers = category === 'Indices' ? ['^GSPC', '^IXIC', '^DJI', '^RUT']
                    : category === 'Tech' ? ['AAPL', 'MSFT', 'GOOGL', 'AMZN', 'NVDA']
                    : ['GC=F', 'CL=F', 'SI=F'];
                  setConfig({ ...config, marketCategory: category, marketTickers: tickers });
                }}
                style={{
                  width: '100%',
                  padding: '6px 8px',
                  backgroundColor: FC.DARK_BG,
                  border: `1px solid ${FC.BORDER}`,
                  color: FC.WHITE,
                  fontSize: '10px',
                  borderRadius: '2px',
                  fontFamily: '"IBM Plex Mono", monospace',
                }}
              >
                <option value="Indices">INDICES</option>
                <option value="Tech">TECH STOCKS</option>
                <option value="Commodities">COMMODITIES</option>
              </select>
            </div>
          </div>
        );

      case 'watchlist':
        return (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            <div>
              <label style={{ fontSize: '9px', color: FC.GRAY, fontWeight: 700, display: 'block', marginBottom: '4px' }}>
                SELECT WATCHLIST
              </label>
              {availableWatchlists.length > 0 ? (
                <select
                  value={config.watchlistId || ''}
                  onChange={(e) => {
                    const wl = availableWatchlists.find(w => w.id === e.target.value);
                    setConfig({ ...config, watchlistId: e.target.value, watchlistName: wl?.name });
                  }}
                  style={{
                    width: '100%',
                    padding: '6px 8px',
                    backgroundColor: FC.DARK_BG,
                    border: `1px solid ${FC.BORDER}`,
                    color: FC.WHITE,
                    fontSize: '10px',
                    borderRadius: '2px',
                    fontFamily: '"IBM Plex Mono", monospace',
                  }}
                >
                  {availableWatchlists.map(wl => (
                    <option key={wl.id} value={wl.id}>{wl.name}</option>
                  ))}
                </select>
              ) : (
                <div style={{ fontSize: '9px', color: FC.MUTED, padding: '8px' }}>No watchlists available</div>
              )}
            </div>
          </div>
        );

      case 'forum':
        return (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            <div>
              <label style={{ fontSize: '9px', color: FC.GRAY, fontWeight: 700, display: 'block', marginBottom: '4px' }}>
                FORUM CATEGORY
              </label>
              <select
                value={config.forumCategoryName || 'Trending'}
                onChange={(e) => setConfig({ ...config, forumCategoryName: e.target.value })}
                style={{
                  width: '100%',
                  padding: '6px 8px',
                  backgroundColor: FC.DARK_BG,
                  border: `1px solid ${FC.BORDER}`,
                  color: FC.WHITE,
                  fontSize: '10px',
                  borderRadius: '2px',
                  fontFamily: '"IBM Plex Mono", monospace',
                }}
              >
                <option value="Trending">TRENDING</option>
                <option value="General">GENERAL</option>
                <option value="Trading">TRADING</option>
                <option value="Analysis">ANALYSIS</option>
              </select>
            </div>
            <div>
              <label style={{ fontSize: '9px', color: FC.GRAY, fontWeight: 700, display: 'block', marginBottom: '4px' }}>
                POSTS LIMIT
              </label>
              <input
                type="number"
                min="3"
                max="10"
                value={config.forumLimit || 5}
                onChange={(e) => setConfig({ ...config, forumLimit: parseInt(e.target.value) })}
                style={{
                  width: '100%',
                  padding: '6px 8px',
                  backgroundColor: FC.DARK_BG,
                  border: `1px solid ${FC.BORDER}`,
                  color: FC.WHITE,
                  fontSize: '10px',
                  borderRadius: '2px',
                  fontFamily: '"IBM Plex Mono", monospace',
                }}
              />
            </div>
          </div>
        );

      case 'calendar':
        return (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            <div>
              <label style={{ fontSize: '9px', color: FC.GRAY, fontWeight: 700, display: 'block', marginBottom: '4px' }}>
                COUNTRY
              </label>
              <select
                value={config.country || 'US'}
                onChange={(e) => setConfig({ ...config, country: e.target.value })}
                style={{
                  width: '100%',
                  padding: '6px 8px',
                  backgroundColor: FC.DARK_BG,
                  border: `1px solid ${FC.BORDER}`,
                  color: FC.WHITE,
                  fontSize: '10px',
                  borderRadius: '2px',
                  fontFamily: '"IBM Plex Mono", monospace',
                }}
              >
                <option value="US">United States</option>
                <option value="EUR">Europe</option>
                <option value="GB">United Kingdom</option>
                <option value="JP">Japan</option>
                <option value="CN">China</option>
                <option value="CA">Canada</option>
                <option value="AU">Australia</option>
              </select>
            </div>
            <div>
              <label style={{ fontSize: '9px', color: FC.GRAY, fontWeight: 700, display: 'block', marginBottom: '4px' }}>
                EVENTS LIMIT
              </label>
              <input
                type="number"
                min="5"
                max="20"
                value={config.limit || 10}
                onChange={(e) => setConfig({ ...config, limit: parseInt(e.target.value) })}
                style={{
                  width: '100%',
                  padding: '6px 8px',
                  backgroundColor: FC.DARK_BG,
                  border: `1px solid ${FC.BORDER}`,
                  color: FC.WHITE,
                  fontSize: '10px',
                  borderRadius: '2px',
                  fontFamily: '"IBM Plex Mono", monospace',
                }}
              />
            </div>
          </div>
        );

      case 'datasource':
        return (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            <div>
              <label style={{ fontSize: '9px', color: FC.GRAY, fontWeight: 700, display: 'block', marginBottom: '4px' }}>
                SELECT DATA SOURCE
              </label>
              {availableDataSources.length > 0 ? (
                <select
                  value={config.dataSourceAlias || ''}
                  onChange={(e) => {
                    const ds = availableDataSources.find(d => d.alias === e.target.value);
                    setConfig({ ...config, dataSourceAlias: e.target.value, dataSourceDisplayName: ds?.display_name });
                  }}
                  style={{
                    width: '100%',
                    padding: '6px 8px',
                    backgroundColor: FC.DARK_BG,
                    border: `1px solid ${FC.BORDER}`,
                    color: FC.WHITE,
                    fontSize: '10px',
                    borderRadius: '2px',
                    fontFamily: '"IBM Plex Mono", monospace',
                  }}
                >
                  {availableDataSources.map(ds => (
                    <option key={ds.alias} value={ds.alias}>{ds.display_name}</option>
                  ))}
                </select>
              ) : (
                <div style={{ fontSize: '9px', color: FC.MUTED, padding: '8px' }}>No data sources available</div>
              )}
            </div>
          </div>
        );

      case 'performance':
        return (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            <div>
              <label style={{ fontSize: '9px', color: FC.GRAY, fontWeight: 700, display: 'block', marginBottom: '4px' }}>
                SELECT PORTFOLIO
              </label>
              {portfolios.length > 0 ? (
                <select
                  value={config.portfolioId || ''}
                  onChange={(e) => {
                    const p = portfolios.find(p => p.id === e.target.value);
                    setConfig({ ...config, portfolioId: e.target.value, portfolioName: p?.name });
                  }}
                  style={{
                    width: '100%',
                    padding: '6px 8px',
                    backgroundColor: FC.DARK_BG,
                    border: `1px solid ${FC.BORDER}`,
                    color: FC.WHITE,
                    fontSize: '10px',
                    borderRadius: '2px',
                    fontFamily: '"IBM Plex Mono", monospace',
                  }}
                >
                  <option value="">— Select Portfolio —</option>
                  {portfolios.map(p => (
                    <option key={p.id} value={p.id}>{p.name}</option>
                  ))}
                </select>
              ) : (
                <div style={{ fontSize: '9px', color: FC.MUTED, padding: '8px' }}>No portfolios available</div>
              )}
            </div>
          </div>
        );

      case 'stockquote':
        return (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            <div>
              <label style={{ fontSize: '9px', color: FC.GRAY, fontWeight: 700, display: 'block', marginBottom: '4px' }}>
                TICKER SYMBOL
              </label>
              <input
                type="text"
                value={config.stockSymbol || 'AAPL'}
                onChange={(e) => setConfig({ ...config, stockSymbol: e.target.value.toUpperCase() })}
                placeholder="e.g. AAPL, MSFT, TSLA"
                style={{
                  width: '100%',
                  padding: '6px 8px',
                  backgroundColor: FC.DARK_BG,
                  border: `1px solid ${FC.BORDER}`,
                  color: FC.WHITE,
                  fontSize: '10px',
                  borderRadius: '2px',
                  fontFamily: '"IBM Plex Mono", monospace',
                  boxSizing: 'border-box',
                }}
              />
              <div style={{ fontSize: '8px', color: FC.MUTED, marginTop: '4px' }}>
                Enter any valid stock ticker (NYSE, NASDAQ, etc.)
              </div>
            </div>
          </div>
        );

      case 'screener':
        return (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            <div>
              <label style={{ fontSize: '9px', color: FC.GRAY, fontWeight: 700, display: 'block', marginBottom: '4px' }}>
                SCREEN PRESET
              </label>
              <select
                value={config.screenerPreset || 'value'}
                onChange={(e) => setConfig({ ...config, screenerPreset: e.target.value })}
                style={{
                  width: '100%',
                  padding: '6px 8px',
                  backgroundColor: FC.DARK_BG,
                  border: `1px solid ${FC.BORDER}`,
                  color: FC.WHITE,
                  fontSize: '10px',
                  borderRadius: '2px',
                  fontFamily: '"IBM Plex Mono", monospace',
                }}
              >
                <option value="value">VALUE STOCKS — Low P/E, High Div Yield</option>
                <option value="growth">GROWTH STOCKS — High Revenue Growth</option>
                <option value="momentum">MOMENTUM STOCKS — Strong Price Action</option>
              </select>
            </div>
          </div>
        );

      case 'watchlistalerts':
        return (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            <div>
              <label style={{ fontSize: '9px', color: FC.GRAY, fontWeight: 700, display: 'block', marginBottom: '4px' }}>
                ALERT THRESHOLD (%)
              </label>
              <input
                type="number"
                min="1"
                max="20"
                step="0.5"
                value={config.alertThreshold ?? 3}
                onChange={(e) => setConfig({ ...config, alertThreshold: parseFloat(e.target.value) })}
                style={{
                  width: '100%',
                  padding: '6px 8px',
                  backgroundColor: FC.DARK_BG,
                  border: `1px solid ${FC.BORDER}`,
                  color: FC.WHITE,
                  fontSize: '10px',
                  borderRadius: '2px',
                  fontFamily: '"IBM Plex Mono", monospace',
                }}
              />
              <div style={{ fontSize: '8px', color: FC.MUTED, marginTop: '4px' }}>
                Alert when a watchlist ticker moves by this % or more
              </div>
            </div>
          </div>
        );

      case 'dbnomics':
        return (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            <div>
              <label style={{ fontSize: '9px', color: FC.GRAY, fontWeight: 700, display: 'block', marginBottom: '4px' }}>
                SERIES ID
              </label>
              <input
                type="text"
                value={config.dbnomicsSeriesId || 'FRED/UNRATE'}
                onChange={(e) => setConfig({ ...config, dbnomicsSeriesId: e.target.value })}
                placeholder="e.g. FRED/UNRATE"
                style={{
                  width: '100%',
                  padding: '6px 8px',
                  backgroundColor: FC.DARK_BG,
                  border: `1px solid ${FC.BORDER}`,
                  color: FC.WHITE,
                  fontSize: '10px',
                  borderRadius: '2px',
                  fontFamily: '"IBM Plex Mono", monospace',
                  boxSizing: 'border-box',
                }}
              />
              <div style={{ fontSize: '8px', color: FC.MUTED, marginTop: '4px' }}>
                Format: PROVIDER/DATASET/SERIES — e.g. FRED/UNRATE, ECB/EXR/M.USD.EUR.SP00.A
              </div>
            </div>
            <div>
              <label style={{ fontSize: '9px', color: FC.GRAY, fontWeight: 700, display: 'block', marginBottom: '6px' }}>
                COMMON SERIES
              </label>
              <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                {[
                  { id: 'FRED/UNRATE', label: 'US Unemployment Rate' },
                  { id: 'FRED/GDP', label: 'US GDP' },
                  { id: 'FRED/CPIAUCSL', label: 'US CPI (Inflation)' },
                  { id: 'FRED/FEDFUNDS', label: 'Fed Funds Rate' },
                  { id: 'FRED/DGS10', label: 'US 10Y Treasury Yield' },
                ].map(s => (
                  <button
                    key={s.id}
                    onClick={() => setConfig({ ...config, dbnomicsSeriesId: s.id })}
                    style={{
                      padding: '5px 8px',
                      backgroundColor: config.dbnomicsSeriesId === s.id ? FC.ORANGE + '20' : FC.DARK_BG,
                      border: `1px solid ${config.dbnomicsSeriesId === s.id ? FC.ORANGE : FC.BORDER}`,
                      color: config.dbnomicsSeriesId === s.id ? FC.ORANGE : FC.GRAY,
                      fontSize: '9px',
                      borderRadius: '2px',
                      cursor: 'pointer',
                      fontFamily: '"IBM Plex Mono", monospace',
                      textAlign: 'left',
                      display: 'flex',
                      justifyContent: 'space-between',
                    }}
                  >
                    <span>{s.label}</span>
                    <span style={{ color: FC.MUTED }}>{s.id}</span>
                  </button>
                ))}
              </div>
            </div>
          </div>
        );

      case 'notes':
        return (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            <div>
              <label style={{ fontSize: '9px', color: FC.GRAY, fontWeight: 700, display: 'block', marginBottom: '4px' }}>
                NOTES CATEGORY
              </label>
              <select
                value={config.notesCategory || 'all'}
                onChange={(e) => setConfig({ ...config, notesCategory: e.target.value })}
                style={{
                  width: '100%',
                  padding: '6px 8px',
                  backgroundColor: FC.DARK_BG,
                  border: `1px solid ${FC.BORDER}`,
                  color: FC.WHITE,
                  fontSize: '10px',
                  borderRadius: '2px',
                  fontFamily: '"IBM Plex Mono", monospace',
                }}
              >
                <option value="all">ALL CATEGORIES</option>
                <option value="research">RESEARCH</option>
                <option value="trade">TRADE NOTES</option>
                <option value="watchlist">WATCHLIST</option>
                <option value="general">GENERAL</option>
              </select>
            </div>
            <div>
              <label style={{ fontSize: '9px', color: FC.GRAY, fontWeight: 700, display: 'block', marginBottom: '4px' }}>
                NOTES LIMIT
              </label>
              <input
                type="number"
                min="3"
                max="20"
                value={config.notesLimit || 10}
                onChange={(e) => setConfig({ ...config, notesLimit: parseInt(e.target.value) })}
                style={{
                  width: '100%',
                  padding: '6px 8px',
                  backgroundColor: FC.DARK_BG,
                  border: `1px solid ${FC.BORDER}`,
                  color: FC.WHITE,
                  fontSize: '10px',
                  borderRadius: '2px',
                  fontFamily: '"IBM Plex Mono", monospace',
                }}
              />
            </div>
          </div>
        );

      default:
        return (
          <div style={{ fontSize: '9px', color: FC.MUTED, padding: '12px', textAlign: 'center' }}>
            This widget type has no configurable settings.
          </div>
        );
    }
  };

  // Non-configurable widgets
  const isConfigurable = [
    'news', 'market', 'watchlist', 'forum', 'datasource', 'calendar', 'notes',
    'stockquote', 'screener', 'watchlistalerts', 'dbnomics', 'performance'
  ].includes(widgetType);

  return (
    <div style={{
      position: 'fixed',
      top: 0,
      left: 0,
      right: 0,
      bottom: 0,
      backgroundColor: 'rgba(0, 0, 0, 0.85)',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      zIndex: 10000,
      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
    }}>
      <div style={{
        width: '420px',
        maxHeight: '80vh',
        backgroundColor: FC.PANEL_BG,
        border: `1px solid ${FC.BORDER}`,
        borderRadius: '2px',
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
        boxShadow: `0 4px 24px rgba(0,0,0,0.5), 0 0 1px ${FC.ORANGE}40`,
      }}>
        {/* Header */}
        <div style={{
          padding: '10px 12px',
          backgroundColor: FC.HEADER_BG,
          borderBottom: `2px solid ${FC.ORANGE}`,
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
        }}>
          <span style={{
            fontSize: '11px',
            fontWeight: 700,
            color: FC.ORANGE,
            letterSpacing: '0.5px',
          }}>
            WIDGET SETTINGS
          </span>
          <button
            onClick={onClose}
            style={{
              background: 'none',
              border: 'none',
              color: FC.GRAY,
              cursor: 'pointer',
              padding: '2px',
              display: 'flex',
              alignItems: 'center',
            }}
          >
            <X size={14} />
          </button>
        </div>

        {/* Content */}
        <div style={{
          flex: 1,
          overflow: 'auto',
          padding: '16px',
        }}>
          <div style={{
            marginBottom: '12px',
            paddingBottom: '12px',
            borderBottom: `1px solid ${FC.BORDER}`,
          }}>
            <div style={{ fontSize: '8px', color: FC.MUTED, marginBottom: '4px' }}>WIDGET TYPE</div>
            <div style={{ fontSize: '10px', color: FC.CYAN, fontWeight: 700, textTransform: 'uppercase' }}>
              {widgetType}
            </div>
          </div>

          {isConfigurable ? renderConfigForm() : (
            <div style={{
              padding: '24px',
              textAlign: 'center',
              backgroundColor: FC.DARK_BG,
              border: `1px solid ${FC.BORDER}`,
              borderRadius: '2px',
            }}>
              <div style={{ fontSize: '10px', color: FC.GRAY, marginBottom: '6px' }}>
                This widget has no configurable settings.
              </div>
              <div style={{ fontSize: '8px', color: FC.MUTED }}>
                You can only resize, move, or remove this widget.
              </div>
            </div>
          )}
        </div>

        {/* Footer */}
        {isConfigurable && (
          <div style={{
            padding: '10px 12px',
            backgroundColor: FC.HEADER_BG,
            borderTop: `1px solid ${FC.BORDER}`,
            display: 'flex',
            justifyContent: 'flex-end',
            gap: '8px',
          }}>
            <button
              onClick={onClose}
              style={{
                padding: '6px 12px',
                backgroundColor: 'transparent',
                border: `1px solid ${FC.BORDER}`,
                color: FC.GRAY,
                fontSize: '9px',
                fontWeight: 700,
                borderRadius: '2px',
                cursor: 'pointer',
                fontFamily: '"IBM Plex Mono", monospace',
                letterSpacing: '0.5px',
              }}
            >
              CANCEL
            </button>
            <button
              onClick={handleSave}
              style={{
                padding: '6px 12px',
                backgroundColor: FC.ORANGE,
                border: 'none',
                color: FC.DARK_BG,
                fontSize: '9px',
                fontWeight: 700,
                borderRadius: '2px',
                cursor: 'pointer',
                fontFamily: '"IBM Plex Mono", monospace',
                letterSpacing: '0.5px',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
              }}
            >
              <Save size={10} />
              SAVE CHANGES
            </button>
          </div>
        )}
      </div>
    </div>
  );
};
