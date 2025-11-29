import React, { useState, useEffect } from 'react';
import GridLayout, { Layout } from 'react-grid-layout';
import { Plus, RotateCcw, Save } from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { sqliteService } from '../../services/sqliteService';
import {
  NewsWidget,
  MarketDataWidget,
  WatchlistWidget,
  ForumWidget,
  CryptoWidget,
  CommoditiesWidget,
  GlobalIndicesWidget,
  ForexWidget,
  MaritimeWidget,
  DataSourceWidget,
  WidgetType,
  WidgetConfig
} from './dashboard/widgets';
import { AddWidgetModal } from './dashboard/AddWidgetModal';
import 'react-grid-layout/css/styles.css';
import 'react-resizable/css/styles.css';

interface WidgetInstance extends WidgetConfig {
  layout: Layout;
}

const DEFAULT_LAYOUT: WidgetInstance[] = [
  // Row 1 - Top indices and major markets
  {
    id: 'indices-1',
    type: 'indices',
    title: 'Global Indices - Top 12',
    config: {},
    layout: { i: 'indices-1', x: 0, y: 0, w: 4, h: 5, minW: 3, minH: 4 }
  },
  {
    id: 'crypto-1',
    type: 'crypto',
    title: 'Cryptocurrency Markets',
    config: {},
    layout: { i: 'crypto-1', x: 4, y: 0, w: 4, h: 5, minW: 3, minH: 4 }
  },
  {
    id: 'news-1',
    type: 'news',
    title: 'Market News',
    config: { newsCategory: 'MARKETS', newsLimit: 5 },
    layout: { i: 'news-1', x: 8, y: 0, w: 4, h: 5, minW: 2, minH: 3 }
  },
  // Row 2 - Forex, Commodities, Tech News
  {
    id: 'forex-1',
    type: 'forex',
    title: 'Forex - Major Pairs',
    config: {},
    layout: { i: 'forex-1', x: 0, y: 5, w: 3, h: 4, minW: 2, minH: 3 }
  },
  {
    id: 'commodities-1',
    type: 'commodities',
    title: 'Commodities',
    config: {},
    layout: { i: 'commodities-1', x: 3, y: 5, w: 3, h: 4, minW: 2, minH: 3 }
  },
  {
    id: 'news-2',
    type: 'news',
    title: 'Tech News',
    config: { newsCategory: 'TECH', newsLimit: 5 },
    layout: { i: 'news-2', x: 6, y: 5, w: 3, h: 4, minW: 2, minH: 3 }
  },
  {
    id: 'news-3',
    type: 'news',
    title: 'Earnings News',
    config: { newsCategory: 'EARNINGS', newsLimit: 5 },
    layout: { i: 'news-3', x: 9, y: 5, w: 3, h: 4, minW: 2, minH: 3 }
  },
  // Row 3 - Forum, Maritime Intelligence, and Tech Stocks
  {
    id: 'forum-1',
    type: 'forum',
    title: 'Trending Forum Posts',
    config: { forumCategoryName: 'Trending', forumLimit: 5 },
    layout: { i: 'forum-1', x: 0, y: 9, w: 4, h: 4, minW: 2, minH: 3 }
  },
  {
    id: 'maritime-1',
    type: 'maritime',
    title: 'Maritime Intelligence',
    config: {},
    layout: { i: 'maritime-1', x: 4, y: 9, w: 4, h: 4, minW: 2, minH: 3 }
  },
  {
    id: 'market-2',
    type: 'market',
    title: 'Tech Stocks',
    config: { marketCategory: 'Tech', marketTickers: ['AAPL', 'MSFT', 'GOOGL', 'AMZN', 'NVDA'] },
    layout: { i: 'market-2', x: 8, y: 9, w: 4, h: 4, minW: 2, minH: 3 }
  }
];

const STORAGE_KEY = 'dashboard-widgets';

interface DashboardTabProps {
  onNavigateToTab?: (tabName: string) => void;
}

const DashboardTab: React.FC<DashboardTabProps> = ({ onNavigateToTab }) => {
  const { colors, fontSize, fontFamily, fontWeight, fontStyle } = useTerminalTheme();
  const [widgets, setWidgets] = useState<WidgetInstance[]>([]);
  const [showAddModal, setShowAddModal] = useState(false);
  const [currentTime, setCurrentTime] = useState(new Date());
  const [nextId, setNextId] = useState(1);
  const [containerWidth, setContainerWidth] = useState(1200);
  const [dbInitialized, setDbInitialized] = useState(false);
  const containerRef = React.useRef<HTMLDivElement>(null);

  // Initialize database for caching
  useEffect(() => {
    const initDatabase = async () => {
      try {
        await sqliteService.initialize();
        const healthCheck = await sqliteService.healthCheck();
        if (healthCheck.healthy) {
          setDbInitialized(true);
          console.log('[Dashboard] SQLite database initialized for caching');
        } else {
          console.warn('[Dashboard] Database not healthy:', healthCheck.message);
        }
      } catch (error) {
        console.error('[Dashboard] Database initialization error:', error);
      }
    };
    initDatabase();
  }, []);

  // Load widgets from localStorage on mount
  useEffect(() => {
    const saved = localStorage.getItem(STORAGE_KEY);
    if (saved) {
      try {
        const parsed = JSON.parse(saved);
        setWidgets(parsed.widgets);
        setNextId(parsed.nextId || 1);
      } catch (error) {
        console.error('Failed to load dashboard layout:', error);
        setWidgets(DEFAULT_LAYOUT);
      }
    } else {
      setWidgets(DEFAULT_LAYOUT);
    }
  }, []);

  // Update time
  useEffect(() => {
    const timer = setInterval(() => {
      setCurrentTime(new Date());
    }, 1000);
    return () => clearInterval(timer);
  }, []);

  // Handle responsive width
  useEffect(() => {
    const updateWidth = () => {
      if (containerRef.current) {
        const width = containerRef.current.offsetWidth;
        console.log('[Dashboard] Container width updated:', width);
        setContainerWidth(width);
      }
    };

    // Initial width
    updateWidth();

    // Update on window resize
    window.addEventListener('resize', updateWidth);

    // Use ResizeObserver for more accurate tracking
    const resizeObserver = new ResizeObserver(() => {
      updateWidth();
    });

    if (containerRef.current) {
      resizeObserver.observe(containerRef.current);
    }

    // Also check after a short delay (for initial render)
    const timeout = setTimeout(updateWidth, 100);

    return () => {
      window.removeEventListener('resize', updateWidth);
      resizeObserver.disconnect();
      clearTimeout(timeout);
    };
  }, []);

  // Save layout to localStorage
  const saveLayout = () => {
    localStorage.setItem(STORAGE_KEY, JSON.stringify({ widgets, nextId }));
    alert('Dashboard layout saved!');
  };

  // Reset to default layout
  const resetLayout = () => {
    if (confirm('Reset dashboard to default layout? This will remove all custom widgets.')) {
      setWidgets(DEFAULT_LAYOUT);
      localStorage.removeItem(STORAGE_KEY);
    }
  };

  // Add new widget
  const handleAddWidget = (widgetType: WidgetType, config?: any) => {
    const newWidget: WidgetInstance = {
      id: `${widgetType}-${nextId}`,
      type: widgetType,
      title: config.watchlistName || config.marketCategory || config.newsCategory || config.forumCategoryName || widgetType,
      config,
      layout: {
        i: `${widgetType}-${nextId}`,
        x: (widgets.length * 3) % 12,
        y: Infinity, // Put at bottom
        w: 4,
        h: 4,
        minW: 2,
        minH: 3
      }
    };

    setWidgets([...widgets, newWidget]);
    setNextId(nextId + 1);
  };

  // Remove widget
  const handleRemoveWidget = (id: string) => {
    setWidgets(widgets.filter(w => w.id !== id));
  };

  // Handle layout change
  const handleLayoutChange = (layout: Layout[]) => {
    setWidgets(widgets.map(widget => {
      const newLayout = layout.find(l => l.i === widget.id);
      return newLayout ? { ...widget, layout: newLayout } : widget;
    }));
  };

  // Render widget based on type
  const renderWidget = (widget: WidgetInstance) => {
    switch (widget.type) {
      case 'news':
        return (
          <NewsWidget
            id={widget.id}
            category={widget.config?.newsCategory}
            limit={widget.config?.newsLimit}
            onRemove={() => handleRemoveWidget(widget.id)}
          />
        );
      case 'market':
        return (
          <MarketDataWidget
            id={widget.id}
            category={widget.config?.marketCategory}
            tickers={widget.config?.marketTickers}
            onRemove={() => handleRemoveWidget(widget.id)}
          />
        );
      case 'watchlist':
        return (
          <WatchlistWidget
            id={widget.id}
            watchlistId={widget.config?.watchlistId}
            watchlistName={widget.config?.watchlistName}
            onRemove={() => handleRemoveWidget(widget.id)}
          />
        );
      case 'forum':
        return (
          <ForumWidget
            id={widget.id}
            categoryId={widget.config?.forumCategoryId}
            categoryName={widget.config?.forumCategoryName}
            limit={widget.config?.forumLimit}
            onRemove={() => handleRemoveWidget(widget.id)}
          />
        );
      case 'crypto':
        return (
          <CryptoWidget
            id={widget.id}
            onRemove={() => handleRemoveWidget(widget.id)}
          />
        );
      case 'commodities':
        return (
          <CommoditiesWidget
            id={widget.id}
            onRemove={() => handleRemoveWidget(widget.id)}
          />
        );
      case 'indices':
        return (
          <GlobalIndicesWidget
            id={widget.id}
            onRemove={() => handleRemoveWidget(widget.id)}
          />
        );
      case 'forex':
        return (
          <ForexWidget
            id={widget.id}
            onRemove={() => handleRemoveWidget(widget.id)}
          />
        );
      case 'maritime':
        return (
          <MaritimeWidget
            id={widget.id}
            onRemove={() => handleRemoveWidget(widget.id)}
            onNavigate={() => onNavigateToTab?.('maritime')}
          />
        );
      case 'datasource':
        return (
          <DataSourceWidget
            id={widget.id}
            alias={widget.config?.dataSourceAlias || ''}
            displayName={widget.config?.dataSourceDisplayName}
            onRemove={() => handleRemoveWidget(widget.id)}
          />
        );
      default:
        return <div>Unknown widget type</div>;
    }
  };

  return (
    <div style={{
      height: '100%',
      backgroundColor: colors.background,
      color: colors.text,
      fontFamily: fontFamily,
      fontWeight: fontWeight,
      fontStyle: fontStyle,
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column'
    }}>
      <style>{`
        /* Scrollbar styling */
        *::-webkit-scrollbar {
          width: 8px;
          height: 8px;
        }
        *::-webkit-scrollbar-track {
          background: #1a1a1a;
        }
        *::-webkit-scrollbar-thumb {
          background: #404040;
          border-radius: 4px;
        }
        *::-webkit-scrollbar-thumb:hover {
          background: #525252;
        }

        /* Grid layout drag handle styling */
        .react-grid-item.react-grid-placeholder {
          background: ${colors.primary} !important;
          opacity: 0.3;
          border-radius: 4px;
        }

        .react-grid-item.resizing {
          opacity: 0.9;
          z-index: 100;
        }

        .react-grid-item {
          transition: all 200ms ease;
          transition-property: left, top, width, height;
        }

        .react-grid-item.cssTransforms {
          transition-property: transform, width, height;
        }

        .react-grid-item.react-draggable-dragging {
          transition: none;
          z-index: 100;
          will-change: transform;
        }

        .react-resizable-handle {
          opacity: 0;
          transition: opacity 0.2s;
        }

        .react-grid-item:hover .react-resizable-handle {
          opacity: 0.5;
        }

        .react-resizable-handle::after {
          border-right: 2px solid ${colors.primary};
          border-bottom: 2px solid ${colors.primary};
        }
      `}</style>

      {/* Header Bar */}
      <div style={{
        backgroundColor: colors.panel,
        borderBottom: `1px solid ${colors.textMuted}`,
        padding: '8px 12px',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <span style={{ color: colors.primary, fontWeight: 'bold', fontSize: fontSize.subheading }}>
              CUSTOMIZABLE DASHBOARD
            </span>
            <span style={{ color: colors.text }}>|</span>
            <span style={{ color: colors.secondary, fontSize: fontSize.small }}>
              ● LIVE
            </span>
            <span style={{ color: colors.text }}>|</span>
            <span style={{ color: colors.warning, fontSize: fontSize.body }}>
              {currentTime.toISOString().replace('T', ' ').substring(0, 19)} UTC
            </span>
            <span style={{ color: colors.text }}>|</span>
            <span style={{ color: colors.textMuted, fontSize: fontSize.small }}>
              WIDGETS: {widgets.length}
            </span>
          </div>

          <div style={{ display: 'flex', gap: '8px' }}>
            <button
              onClick={() => setShowAddModal(true)}
              style={{
                background: colors.secondary,
                color: 'black',
                border: 'none',
                padding: '6px 12px',
                fontSize: fontSize.small,
                fontWeight: 'bold',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                borderRadius: '2px'
              }}
            >
              <Plus size={12} />
              ADD WIDGET
            </button>
            <button
              onClick={saveLayout}
              style={{
                background: colors.primary,
                color: 'black',
                border: 'none',
                padding: '6px 12px',
                fontSize: fontSize.small,
                fontWeight: 'bold',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                borderRadius: '2px'
              }}
            >
              <Save size={12} />
              SAVE LAYOUT
            </button>
            <button
              onClick={resetLayout}
              style={{
                background: colors.textMuted,
                color: 'black',
                border: 'none',
                padding: '6px 12px',
                fontSize: fontSize.small,
                fontWeight: 'bold',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                borderRadius: '2px'
              }}
            >
              <RotateCcw size={12} />
              RESET
            </button>
          </div>
        </div>
      </div>

      {/* Info Bar */}
      <div style={{
        backgroundColor: colors.panel,
        borderBottom: `1px solid ${colors.textMuted}`,
        padding: '4px 12px',
        fontSize: fontSize.small,
        color: colors.textMuted,
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', gap: '16px' }}>
          <span>💡 <span style={{ color: colors.text }}>Drag widgets to rearrange</span></span>
          <span>🔧 <span style={{ color: colors.text }}>Resize from bottom-right corner</span></span>
          <span>✖️ <span style={{ color: colors.text }}>Remove widgets with X button</span></span>
          <span>📐 <span style={{ color: colors.text }}>Responsive layout - uses full width</span></span>
        </div>
      </div>

      {/* Main Content - Grid Layout */}
      <div
        ref={containerRef}
        style={{
          flex: 1,
          overflow: 'auto',
          padding: '8px',
          backgroundColor: '#050505'
        }}
      >
        {widgets.length === 0 ? (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '100%',
            gap: '16px'
          }}>
            <div style={{ color: colors.textMuted, fontSize: fontSize.subheading }}>
              No widgets on dashboard
            </div>
            <button
              onClick={() => setShowAddModal(true)}
              style={{
                background: colors.primary,
                color: 'black',
                border: 'none',
                padding: '12px 24px',
                fontSize: fontSize.body,
                fontWeight: 'bold',
                cursor: 'pointer',
                borderRadius: '4px'
              }}
            >
              ADD YOUR FIRST WIDGET
            </button>
          </div>
        ) : (
          <GridLayout
            className="layout"
            layout={widgets.map(w => w.layout)}
            cols={12}
            rowHeight={60}
            width={containerWidth - 16}
            onLayoutChange={handleLayoutChange}
            draggableHandle=".widget-drag-handle"
            isDraggable={true}
            isResizable={true}
            compactType="vertical"
            preventCollision={false}
          >
            {widgets.map(widget => (
              <div key={widget.id}>
                {renderWidget(widget)}
              </div>
            ))}
          </GridLayout>
        )}
      </div>

      {/* Status Bar */}
      <div style={{
        borderTop: `1px solid ${colors.textMuted}`,
        backgroundColor: colors.panel,
        padding: '4px 12px',
        fontSize: fontSize.tiny,
        color: colors.textMuted,
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <span>Fincept Customizable Dashboard v2.0.0 | Drag & drop workspace</span>
          <span>
            Widgets: {widgets.length} | Layout: {widgets.length > 0 ? 'Custom' : 'Empty'} |
            Status: <span style={{ color: colors.secondary }}>ACTIVE</span>
            {dbInitialized && (
              <span style={{ marginLeft: '8px', color: colors.secondary }}>
                | Cache: ENABLED
              </span>
            )}
          </span>
        </div>
      </div>

      {/* Add Widget Modal */}
      <AddWidgetModal
        isOpen={showAddModal}
        onClose={() => setShowAddModal(false)}
        onAdd={handleAddWidget}
      />
    </div>
  );
};

export default DashboardTab;
