// AddWidgetModal - Terminal-style widget configuration modal
// Follows Fincept Design System with inline styles, uppercase labels, monospace

import React, { useState, useEffect } from 'react';
import { X, Plus, Newspaper, BarChart3, Eye, MessageSquare, Bitcoin,
  Package, Globe, DollarSign, Ship, Database, TrendingUp,
  Activity, Briefcase, Bell, Calendar, Zap, Shield, LineChart, StickyNote,
  Quote, ArrowUpDown, Search, AlertTriangle, Brain, Bot, Trophy, TestTube,
  BellRing, Cpu, DatabaseZap, Flag, Anchor, Video } from 'lucide-react';
import { useTranslation } from 'react-i18next';
import { WidgetType, DEFAULT_WIDGET_CONFIGS } from './widgets';
import { watchlistService } from '../../../services/core/watchlistService';
import { getAllDataSources } from '../../../services/data-sources/dataSourceRegistry';
import { DataSource } from '../../../services/core/sqliteService';

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
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
};

interface AddWidgetModalProps {
  isOpen: boolean;
  onClose: () => void;
  onAdd: (widgetType: WidgetType, config?: any) => void;
}

interface WidgetOption {
  type: WidgetType;
  label: string;
  description: string;
  icon: React.ReactNode;
  color: string;
  category: 'markets' | 'data' | 'analysis' | 'tools';
}

const WIDGET_OPTIONS: WidgetOption[] = [
  { type: 'indices', label: 'GLOBAL INDICES', description: 'Major world indices with real-time prices', icon: <Globe size={14} />, color: FC.CYAN, category: 'markets' },
  { type: 'forex', label: 'FOREX', description: 'Major currency pairs and exchange rates', icon: <DollarSign size={14} />, color: FC.GREEN, category: 'markets' },
  { type: 'commodities', label: 'COMMODITIES', description: 'Gold, oil, silver and raw materials', icon: <Package size={14} />, color: FC.YELLOW, category: 'markets' },
  { type: 'crypto', label: 'CRYPTO', description: 'Cryptocurrency prices and market cap', icon: <Bitcoin size={14} />, color: FC.ORANGE, category: 'markets' },
  { type: 'heatmap', label: 'SECTOR HEATMAP', description: 'S&P 500 sector performance visualization', icon: <BarChart3 size={14} />, color: FC.ORANGE, category: 'markets' },
  { type: 'market', label: 'MARKET DATA', description: 'Custom stock/index quotes', icon: <BarChart3 size={14} />, color: FC.BLUE, category: 'markets' },
  { type: 'news', label: 'NEWS FEED', description: 'Real-time financial news and alerts', icon: <Newspaper size={14} />, color: FC.WHITE, category: 'data' },
  { type: 'watchlist', label: 'WATCHLIST', description: 'Your saved watchlist instruments', icon: <Eye size={14} />, color: FC.CYAN, category: 'data' },
  { type: 'forum', label: 'FORUM', description: 'Community trending discussions', icon: <MessageSquare size={14} />, color: FC.PURPLE, category: 'data' },
  { type: 'datasource', label: 'DATA SOURCE', description: 'Custom data source integration', icon: <Database size={14} />, color: FC.GRAY, category: 'data' },
  { type: 'notes', label: 'NOTES', description: 'Financial notes and research tracker', icon: <StickyNote size={14} />, color: FC.YELLOW, category: 'data' },
  { type: 'polymarket', label: 'POLYMARKET', description: 'Prediction market probabilities', icon: <TrendingUp size={14} />, color: FC.PURPLE, category: 'analysis' },
  { type: 'economic', label: 'ECONOMICS', description: 'GDP, CPI, unemployment indicators', icon: <Activity size={14} />, color: FC.CYAN, category: 'analysis' },
  { type: 'calendar', label: 'ECONOMIC CALENDAR', description: 'Upcoming economic events', icon: <Calendar size={14} />, color: FC.ORANGE, category: 'analysis' },
  { type: 'portfolio', label: 'PORTFOLIO', description: 'Portfolio summary and P&L', icon: <Briefcase size={14} />, color: FC.BLUE, category: 'analysis' },
  { type: 'performance', label: 'PERFORMANCE', description: 'Trading P&L and win rate tracker', icon: <LineChart size={14} />, color: FC.GREEN, category: 'analysis' },
  { type: 'geopolitics', label: 'GEOPOLITICS', description: 'Geopolitical risk monitor', icon: <Shield size={14} />, color: FC.RED, category: 'analysis' },
  { type: 'quicktrade', label: 'QUICK TRADE', description: 'One-click trading panel', icon: <Zap size={14} />, color: FC.GREEN, category: 'tools' },
  { type: 'stockquote', label: 'STOCK QUOTE', description: 'Single ticker price and stats', icon: <Quote size={14} />, color: FC.CYAN, category: 'markets' },
  { type: 'topmovers', label: 'TOP MOVERS', description: 'Biggest market gainers and losers', icon: <ArrowUpDown size={14} />, color: FC.ORANGE, category: 'markets' },
  { type: 'maritime', label: 'MARITIME', description: 'Shipping sector stocks and ETFs', icon: <Anchor size={14} />, color: FC.BLUE, category: 'markets' },
  { type: 'akshare', label: 'CHINA MARKETS', description: 'Chinese market indices (AkShare)', icon: <Flag size={14} />, color: FC.RED, category: 'markets' },
  { type: 'screener', label: 'SCREENER', description: 'Value/Growth/Momentum stock screener', icon: <Search size={14} />, color: FC.PURPLE, category: 'analysis' },
  { type: 'riskmetrics', label: 'RISK METRICS', description: 'Sharpe, Beta, VaR and drawdown', icon: <AlertTriangle size={14} />, color: FC.RED, category: 'analysis' },
  { type: 'sentiment', label: 'MARKET SENTIMENT', description: 'News-based market sentiment score', icon: <Brain size={14} />, color: FC.CYAN, category: 'analysis' },
  { type: 'backtestsummary', label: 'BACKTEST SUMMARY', description: 'Recent backtest results overview', icon: <TestTube size={14} />, color: FC.PURPLE, category: 'analysis' },
  { type: 'dbnomics', label: 'DBNOMICS', description: 'Economic series from DBnomics', icon: <DatabaseZap size={14} />, color: FC.CYAN, category: 'data' },
  { type: 'algostatus', label: 'ALGO STATUS', description: 'Live algo trading deployment status', icon: <Bot size={14} />, color: FC.GREEN, category: 'tools' },
  { type: 'alphaarena', label: 'ALPHA ARENA', description: 'AI model competition leaderboard', icon: <Trophy size={14} />, color: FC.YELLOW, category: 'tools' },
  { type: 'watchlistalerts', label: 'WATCHLIST ALERTS', description: 'Big movers in your watchlists', icon: <BellRing size={14} />, color: FC.ORANGE, category: 'tools' },
  { type: 'livesignals', label: 'LIVE SIGNALS', description: 'AI Quant Lab trading signals', icon: <Cpu size={14} />, color: FC.GREEN, category: 'tools' },
  { type: 'videoplayer', label: 'VIDEO PLAYER', description: 'Live financial TV streams and custom video URLs', icon: <Video size={14} />, color: FC.PURPLE, category: 'tools' },
];

const CATEGORIES = [
  { key: 'all', label: 'ALL' },
  { key: 'markets', label: 'MARKETS' },
  { key: 'data', label: 'DATA' },
  { key: 'analysis', label: 'ANALYSIS' },
  { key: 'tools', label: 'TOOLS' },
];

export const AddWidgetModal: React.FC<AddWidgetModalProps> = ({
  isOpen,
  onClose,
  onAdd
}) => {
  const [selectedType, setSelectedType] = useState<WidgetType>('indices');
  const [activeCategory, setActiveCategory] = useState('all');
  const [watchlists, setWatchlists] = useState<any[]>([]);
  const [dataSources, setDataSources] = useState<DataSource[]>([]);
  const [newsCategory, setNewsCategory] = useState('ALL');
  const [marketCategory, setMarketCategory] = useState('Indices');
  const [selectedWatchlist, setSelectedWatchlist] = useState('');
  const [forumCategory, setForumCategory] = useState('Trending');
  const [selectedDataSource, setSelectedDataSource] = useState('');
  const [stockSymbol, setStockSymbol] = useState('AAPL');
  const [screenerPreset, setScreenerPreset] = useState<'value' | 'growth' | 'momentum'>('value');
  const [alertThreshold, setAlertThreshold] = useState(3);
  const [dbnomicsSeriesId, setDbnomicsSeriesId] = useState('FRED/UNRATE');
  const [videoUrl, setVideoUrl] = useState('');
  const [videoTitle, setVideoTitle] = useState('Video Player');
  const { t } = useTranslation('dashboard');

  useEffect(() => {
    if (isOpen && selectedType === 'watchlist') loadWatchlists();
    if (isOpen && selectedType === 'datasource') loadDataSources();
  }, [isOpen, selectedType]);

  const loadWatchlists = async () => {
    try {
      await watchlistService.initialize();
      const wls = await watchlistService.getWatchlistsWithCounts();
      setWatchlists(wls);
      if (wls.length > 0) setSelectedWatchlist(wls[0].id);
    } catch (error) {
      console.error('Failed to load watchlists:', error);
    }
  };

  const loadDataSources = async () => {
    try {
      const sources = await getAllDataSources();
      const enabled = sources.filter(s => s.enabled);
      setDataSources(enabled);
      if (enabled.length > 0) setSelectedDataSource(enabled[0].alias);
    } catch (error) {
      console.error('Failed to load data sources:', error);
    }
  };

  const handleAdd = () => {
    let config = {};
    switch (selectedType) {
      case 'news':
        config = { newsCategory, newsLimit: 5 };
        break;
      case 'market': {
        const tickers = marketCategory === 'Indices' ? ['^GSPC', '^IXIC', '^DJI', '^RUT']
          : marketCategory === 'Tech' ? ['AAPL', 'MSFT', 'GOOGL', 'AMZN', 'NVDA']
          : ['GC=F', 'CL=F', 'SI=F'];
        config = { marketCategory, marketTickers: tickers };
        break;
      }
      case 'watchlist': {
        const wl = watchlists.find(w => w.id === selectedWatchlist);
        config = { watchlistId: selectedWatchlist, watchlistName: wl?.name || 'Watchlist' };
        break;
      }
      case 'forum':
        config = { forumCategoryName: forumCategory, forumLimit: 5 };
        break;
      case 'datasource': {
        const ds = dataSources.find(d => d.alias === selectedDataSource);
        config = { dataSourceAlias: selectedDataSource, dataSourceDisplayName: ds?.display_name };
        break;
      }
      case 'stockquote':
        config = { stockSymbol };
        break;
      case 'screener':
        config = { screenerPreset };
        break;
      case 'watchlistalerts':
        config = { alertThreshold };
        break;
      case 'dbnomics':
        config = { dbnomicsSeriesId };
        break;
      case 'videoplayer':
        config = { videoUrl: videoUrl.trim(), videoTitle: videoTitle.trim() || 'Video Player' };
        break;
    }
    onAdd(selectedType, config);
    onClose();
  };

  if (!isOpen) return null;

  const filteredOptions = activeCategory === 'all'
    ? WIDGET_OPTIONS
    : WIDGET_OPTIONS.filter(w => w.category === activeCategory);

  const selectedOption = WIDGET_OPTIONS.find(w => w.type === selectedType);

  return (
    <div style={{
      position: 'fixed', top: 0, left: 0, right: 0, bottom: 0,
      backgroundColor: 'rgba(0, 0, 0, 0.85)',
      display: 'flex', alignItems: 'center', justifyContent: 'center',
      zIndex: 1000,
      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
    }}
      onClick={(e) => { if (e.target === e.currentTarget) onClose(); }}
    >
      <div style={{
        backgroundColor: FC.PANEL_BG,
        border: `1px solid ${FC.BORDER}`,
        borderRadius: '2px',
        width: '640px',
        maxHeight: '80vh',
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
        boxShadow: `0 0 30px ${FC.ORANGE}15, 0 4px 20px rgba(0,0,0,0.5)`,
      }}>
        {/* Header */}
        <div style={{
          backgroundColor: FC.HEADER_BG,
          borderBottom: `2px solid ${FC.ORANGE}`,
          padding: '10px 16px',
          display: 'flex', justifyContent: 'space-between', alignItems: 'center',
          flexShrink: 0,
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Plus size={14} color={FC.ORANGE} />
            <span style={{ color: FC.ORANGE, fontSize: '11px', fontWeight: 700, letterSpacing: '1px' }}>
              ADD WIDGET
            </span>
          </div>
          <button
            onClick={onClose}
            style={{
              background: 'none', border: 'none', color: FC.GRAY,
              cursor: 'pointer', padding: '4px', display: 'flex',
              borderRadius: '2px',
            }}
            onMouseEnter={(e) => { (e.currentTarget as HTMLElement).style.color = FC.WHITE; }}
            onMouseLeave={(e) => { (e.currentTarget as HTMLElement).style.color = FC.GRAY; }}
          >
            <X size={16} />
          </button>
        </div>

        {/* Category Tabs */}
        <div style={{
          display: 'flex', gap: '0',
          borderBottom: `1px solid ${FC.BORDER}`,
          backgroundColor: FC.DARK_BG,
          flexShrink: 0,
        }}>
          {CATEGORIES.map(cat => (
            <button
              key={cat.key}
              onClick={() => setActiveCategory(cat.key)}
              style={{
                flex: 1,
                padding: '8px 0',
                backgroundColor: activeCategory === cat.key ? FC.ORANGE : 'transparent',
                color: activeCategory === cat.key ? FC.DARK_BG : FC.GRAY,
                border: 'none',
                borderRight: `1px solid ${FC.BORDER}`,
                fontSize: '9px',
                fontWeight: 700,
                letterSpacing: '0.5px',
                cursor: 'pointer',
                fontFamily: '"IBM Plex Mono", monospace',
                transition: 'all 0.15s',
              }}
            >
              {cat.label}
            </button>
          ))}
        </div>

        {/* Widget Grid */}
        <div style={{
          flex: 1,
          overflow: 'auto',
          padding: '12px',
        }}>
          <div style={{
            display: 'grid',
            gridTemplateColumns: 'repeat(3, 1fr)',
            gap: '6px',
          }}>
            {filteredOptions.map(option => {
              const isSelected = selectedType === option.type;
              return (
                <button
                  key={option.type}
                  onClick={() => setSelectedType(option.type)}
                  style={{
                    backgroundColor: isSelected ? `${option.color}15` : FC.DARK_BG,
                    border: `1px solid ${isSelected ? option.color : FC.BORDER}`,
                    borderRadius: '2px',
                    padding: '10px 8px',
                    cursor: 'pointer',
                    textAlign: 'left',
                    display: 'flex',
                    flexDirection: 'column',
                    gap: '4px',
                    transition: 'all 0.15s',
                    position: 'relative',
                    overflow: 'hidden',
                  }}
                  onMouseEnter={(e) => {
                    if (!isSelected) {
                      (e.currentTarget as HTMLElement).style.borderColor = `${option.color}60`;
                      (e.currentTarget as HTMLElement).style.backgroundColor = FC.HOVER;
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (!isSelected) {
                      (e.currentTarget as HTMLElement).style.borderColor = FC.BORDER;
                      (e.currentTarget as HTMLElement).style.backgroundColor = FC.DARK_BG;
                    }
                  }}
                >
                  {/* Selection indicator */}
                  {isSelected && (
                    <div style={{
                      position: 'absolute', top: 0, left: 0, right: 0,
                      height: '2px', backgroundColor: option.color,
                    }} />
                  )}
                  <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                    <div style={{ color: option.color }}>{option.icon}</div>
                    <span style={{
                      color: isSelected ? FC.WHITE : FC.GRAY,
                      fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px',
                      fontFamily: '"IBM Plex Mono", monospace',
                    }}>
                      {option.label}
                    </span>
                  </div>
                  <span style={{
                    color: FC.MUTED, fontSize: '8px', lineHeight: '1.3',
                    fontFamily: '"IBM Plex Mono", monospace',
                  }}>
                    {option.description}
                  </span>
                </button>
              );
            })}
          </div>
        </div>

        {/* Configuration Section */}
        <div style={{
          borderTop: `1px solid ${FC.BORDER}`,
          padding: '12px 16px',
          backgroundColor: FC.HEADER_BG,
          flexShrink: 0,
        }}>
          <div style={{ fontSize: '9px', fontWeight: 700, color: FC.GRAY, letterSpacing: '0.5px', marginBottom: '8px' }}>
            CONFIGURATION — {selectedOption?.label || selectedType.toUpperCase()}
          </div>

          {/* Config options based on type */}
          {selectedType === 'news' && (
            <SelectInput
              value={newsCategory}
              onChange={setNewsCategory}
              options={[
                { value: 'ALL', label: 'ALL NEWS' },
                { value: 'MARKETS', label: 'MARKETS' },
                { value: 'TECH', label: 'TECHNOLOGY' },
                { value: 'EARNINGS', label: 'EARNINGS' },
                { value: 'ECONOMIC', label: 'ECONOMIC' },
                { value: 'REGULATORY', label: 'REGULATORY' },
                { value: 'CRYPTO', label: 'CRYPTO' },
              ]}
            />
          )}

          {selectedType === 'market' && (
            <SelectInput
              value={marketCategory}
              onChange={setMarketCategory}
              options={[
                { value: 'Indices', label: 'MARKET INDICES' },
                { value: 'Tech', label: 'TECH STOCKS' },
                { value: 'Commodities', label: 'COMMODITIES' },
              ]}
            />
          )}

          {selectedType === 'watchlist' && (
            watchlists.length > 0 ? (
              <SelectInput
                value={selectedWatchlist}
                onChange={setSelectedWatchlist}
                options={watchlists.map(wl => ({ value: wl.id, label: wl.name.toUpperCase() }))}
              />
            ) : (
              <InfoText text="No watchlists available. Create one in the Watchlist tab first." />
            )
          )}

          {selectedType === 'forum' && (
            <SelectInput
              value={forumCategory}
              onChange={setForumCategory}
              options={[
                { value: 'Trending', label: 'TRENDING POSTS' },
                { value: 'Recent', label: 'RECENT POSTS' },
              ]}
            />
          )}

          {selectedType === 'datasource' && (
            dataSources.length > 0 ? (
              <SelectInput
                value={selectedDataSource}
                onChange={setSelectedDataSource}
                options={dataSources.map(ds => ({
                  value: ds.alias,
                  label: `${ds.display_name} (${ds.alias})`
                }))}
              />
            ) : (
              <InfoText text="No data sources configured. Add one in Settings first." />
            )
          )}

          {selectedType === 'stockquote' && (
            <input
              type="text"
              value={stockSymbol}
              onChange={(e) => setStockSymbol(e.target.value.toUpperCase())}
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
          )}

          {selectedType === 'screener' && (
            <SelectInput
              value={screenerPreset}
              onChange={(v) => setScreenerPreset(v as 'value' | 'growth' | 'momentum')}
              options={[
                { value: 'value', label: 'VALUE — Low P/E, High Dividend Yield' },
                { value: 'growth', label: 'GROWTH — High Revenue Growth' },
                { value: 'momentum', label: 'MOMENTUM — Strong Price Action' },
              ]}
            />
          )}

          {selectedType === 'watchlistalerts' && (
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <input
                type="number"
                min="1"
                max="20"
                step="0.5"
                value={alertThreshold}
                onChange={(e) => setAlertThreshold(parseFloat(e.target.value))}
                style={{
                  width: '80px',
                  padding: '6px 8px',
                  backgroundColor: FC.DARK_BG,
                  border: `1px solid ${FC.BORDER}`,
                  color: FC.WHITE,
                  fontSize: '10px',
                  borderRadius: '2px',
                  fontFamily: '"IBM Plex Mono", monospace',
                }}
              />
              <span style={{ fontSize: '9px', color: FC.GRAY }}>% move threshold to trigger alert</span>
            </div>
          )}

          {selectedType === 'dbnomics' && (
            <input
              type="text"
              value={dbnomicsSeriesId}
              onChange={(e) => setDbnomicsSeriesId(e.target.value)}
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
          )}

          {selectedType === 'videoplayer' && (
            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
              <input
                type="text"
                value={videoTitle}
                onChange={(e) => setVideoTitle(e.target.value)}
                placeholder="Widget title (e.g. Bloomberg TV)"
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
              <input
                type="text"
                value={videoUrl}
                onChange={(e) => setVideoUrl(e.target.value)}
                placeholder="YouTube, HLS (.m3u8) or MP4 URL — leave blank to pick preset"
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
              <div style={{ fontSize: '9px', color: FC.MUTED }}>
                Supports YouTube links, HLS streams (.m3u8), and MP4 files. Leave blank to choose from preset financial channels.
              </div>
            </div>
          )}

          {!['news', 'market', 'watchlist', 'forum', 'datasource', 'stockquote', 'screener', 'watchlistalerts', 'dbnomics', 'videoplayer'].includes(selectedType) && (
            <InfoText text={selectedOption?.description || 'No additional configuration needed.'} />
          )}

          {/* Actions */}
          <div style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end', marginTop: '12px' }}>
            <button
              onClick={onClose}
              style={{
                backgroundColor: 'transparent',
                border: `1px solid ${FC.BORDER}`,
                color: FC.GRAY,
                padding: '6px 16px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: 'pointer',
                borderRadius: '2px',
                letterSpacing: '0.5px',
                fontFamily: '"IBM Plex Mono", monospace',
                transition: 'all 0.15s',
              }}
              onMouseEnter={(e) => { (e.currentTarget as HTMLElement).style.borderColor = FC.ORANGE; (e.currentTarget as HTMLElement).style.color = FC.WHITE; }}
              onMouseLeave={(e) => { (e.currentTarget as HTMLElement).style.borderColor = FC.BORDER; (e.currentTarget as HTMLElement).style.color = FC.GRAY; }}
            >
              CANCEL
            </button>
            <button
              onClick={handleAdd}
              disabled={(selectedType === 'watchlist' && watchlists.length === 0) || (selectedType === 'datasource' && dataSources.length === 0)}
              style={{
                backgroundColor: FC.ORANGE,
                border: `1px solid ${FC.ORANGE}`,
                color: FC.DARK_BG,
                padding: '6px 16px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: 'pointer',
                borderRadius: '2px',
                letterSpacing: '0.5px',
                fontFamily: '"IBM Plex Mono", monospace',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                transition: 'all 0.15s',
                opacity: ((selectedType === 'watchlist' && watchlists.length === 0) || (selectedType === 'datasource' && dataSources.length === 0)) ? 0.4 : 1,
              }}
            >
              <Plus size={11} />
              ADD WIDGET
            </button>
          </div>
        </div>
      </div>
    </div>
  );
};

// Styled select input
const SelectInput: React.FC<{
  value: string;
  onChange: (v: string) => void;
  options: { value: string; label: string }[];
}> = ({ value, onChange, options }) => (
  <select
    value={value}
    onChange={(e) => onChange(e.target.value)}
    style={{
      width: '100%',
      backgroundColor: FC.DARK_BG,
      border: `1px solid ${FC.BORDER}`,
      color: FC.WHITE,
      padding: '6px 8px',
      fontSize: '10px',
      fontFamily: '"IBM Plex Mono", monospace',
      fontWeight: 700,
      borderRadius: '2px',
      cursor: 'pointer',
      outline: 'none',
    }}
    onFocus={(e) => { (e.currentTarget as HTMLElement).style.borderColor = FC.ORANGE; }}
    onBlur={(e) => { (e.currentTarget as HTMLElement).style.borderColor = FC.BORDER; }}
  >
    {options.map(opt => (
      <option key={opt.value} value={opt.value}>{opt.label}</option>
    ))}
  </select>
);

// Info text
const InfoText: React.FC<{ text: string }> = ({ text }) => (
  <div style={{
    color: FC.MUTED,
    fontSize: '9px',
    padding: '6px 0',
    lineHeight: '1.4',
    fontStyle: 'italic',
  }}>
    {text}
  </div>
);
