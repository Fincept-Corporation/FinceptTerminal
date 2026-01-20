import React, { useState, useEffect } from 'react';
import GridLayout, { Layout } from 'react-grid-layout';
import { Plus, RotateCcw, Save, Info } from 'lucide-react';
import { createDashboardTabTour } from '@/components/tabs/tours/dashboardTabTour';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { useTranslation } from 'react-i18next';
import { sqliteService, saveSetting, getSetting } from '@/services/core/sqliteService';
import { TabFooter } from '@/components/common/TabFooter';
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
  PolymarketWidget,
  EconomicIndicatorsWidget,
  PortfolioSummaryWidget,
  AlertsWidget,
  CalendarWidget,
  QuickTradeWidget,
  GeopoliticsWidget,
  PerformanceWidget,
  WidgetType,
  WidgetConfig
} from './widgets';
import { AddWidgetModal } from './AddWidgetModal';
import 'react-grid-layout/css/styles.css';
import 'react-resizable/css/styles.css';

interface WidgetInstance extends WidgetConfig {
  layout: Layout;
}

// Title keys map to translation keys in dashboard.json
const DEFAULT_LAYOUT: WidgetInstance[] = [
  // Row 1 - Top indices and major markets
  {
    id: 'indices-1',
    type: 'indices',
    title: 'widgets.globalIndices',
    config: {},
    layout: { i: 'indices-1', x: 0, y: 0, w: 6, h: 5, minW: 3, minH: 4 }
  },
  {
    id: 'news-1',
    type: 'news',
    title: 'widgets.marketNews',
    config: { newsCategory: 'MARKETS', newsLimit: 5 },
    layout: { i: 'news-1', x: 6, y: 0, w: 6, h: 5, minW: 2, minH: 3 }
  },
  // Row 2 - Forex, Commodities, Tech News
  {
    id: 'forex-1',
    type: 'forex',
    title: 'widgets.forex',
    config: {},
    layout: { i: 'forex-1', x: 0, y: 5, w: 3, h: 4, minW: 2, minH: 3 }
  },
  {
    id: 'commodities-1',
    type: 'commodities',
    title: 'widgets.commodities',
    config: {},
    layout: { i: 'commodities-1', x: 3, y: 5, w: 3, h: 4, minW: 2, minH: 3 }
  },
  {
    id: 'news-2',
    type: 'news',
    title: 'widgets.secPressReleases',
    config: { newsCategory: 'REGULATORY', newsLimit: 5 },
    layout: { i: 'news-2', x: 6, y: 5, w: 3, h: 4, minW: 2, minH: 3 }
  },
  {
    id: 'news-3',
    type: 'news',
    title: 'widgets.cryptoNews',
    config: { newsCategory: 'CRYPTO', newsLimit: 5 },
    layout: { i: 'news-3', x: 9, y: 5, w: 3, h: 4, minW: 2, minH: 3 }
  },
  // Row 3 - Forum, Maritime Intelligence, and Tech Stocks
  {
    id: 'forum-1',
    type: 'forum',
    title: 'widgets.trendingForum',
    config: { forumCategoryName: 'Trending', forumLimit: 5 },
    layout: { i: 'forum-1', x: 0, y: 9, w: 4, h: 4, minW: 2, minH: 3 }
  },
  {
    id: 'maritime-1',
    type: 'maritime',
    title: 'widgets.maritimeIntelligence',
    config: {},
    layout: { i: 'maritime-1', x: 4, y: 9, w: 4, h: 4, minW: 2, minH: 3 }
  },
  {
    id: 'market-2',
    type: 'market',
    title: 'widgets.techStocks',
    config: { marketCategory: 'Tech', marketTickers: ['AAPL', 'MSFT', 'GOOGL', 'AMZN', 'NVDA'] },
    layout: { i: 'market-2', x: 8, y: 9, w: 4, h: 4, minW: 2, minH: 3 }
  }
];

const STORAGE_KEY = 'dashboard-widgets';

interface DashboardTabProps {
  onNavigateToTab?: (tabName: string) => void;
}

const DashboardTab: React.FC<DashboardTabProps> = ({ onNavigateToTab }) => {
  const { colors, fontSize, fontFamily, fontStyle, fontWeight } = useTerminalTheme();
  const { t } = useTranslation('dashboard');
  const [widgets, setWidgets] = useState<WidgetInstance[]>([]);
  const [showAddModal, setShowAddModal] = useState(false);
  const [currentTime, setCurrentTime] = useState(new Date());
  const [nextId, setNextId] = useState(1);
  const [containerWidth, setContainerWidth] = useState(1200);
  const [dbInitialized, setDbInitialized] = useState(false);
  const [initialLoadComplete, setInitialLoadComplete] = useState(false);
  const containerRef = React.useRef<HTMLDivElement>(null);

  // Initialize database for caching
  useEffect(() => {
    const initDatabase = async () => {
      try {
        await sqliteService.initialize();
        const healthCheck = await sqliteService.healthCheck();
        if (healthCheck.healthy) {
          setDbInitialized(true);
        } else {
          setDbInitialized(true); // Allow dashboard to load anyway
        }
      } catch (error) {
        setDbInitialized(true); // Allow dashboard to load anyway
      }
    };
    initDatabase();
  }, []);

  // Load widgets from storage on mount
  useEffect(() => {
    const loadWidgets = async () => {
      try {
        const saved = await getSetting(STORAGE_KEY);
        if (saved) {
          const parsed = JSON.parse(saved);
          setWidgets(parsed.widgets);
          setNextId(parsed.nextId || 1);
        } else {
          setWidgets(DEFAULT_LAYOUT);
        }
      } catch (error) {
        setWidgets(DEFAULT_LAYOUT);
      }
    };
    loadWidgets();
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

  // Save layout to storage
  const saveLayout = async () => {
    try {
      await saveSetting(STORAGE_KEY, JSON.stringify({ widgets, nextId }), 'dashboard');
      alert(t('messages.layoutSaved'));
    } catch (error) {
      alert('Failed to save layout');
    }
  };

  // Reset to default layout
  const resetLayout = async () => {
    if (confirm(t('messages.resetConfirm'))) {
      setWidgets(DEFAULT_LAYOUT);
      await saveSetting(STORAGE_KEY, '', 'dashboard');
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
            onNavigateToTab={onNavigateToTab}
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
      case 'polymarket':
        return (
          <PolymarketWidget
            id={widget.id}
            onRemove={() => handleRemoveWidget(widget.id)}
            onNavigate={() => onNavigateToTab?.('polymarket')}
          />
        );
      case 'economic':
        return (
          <EconomicIndicatorsWidget
            id={widget.id}
            onRemove={() => handleRemoveWidget(widget.id)}
            onNavigate={() => onNavigateToTab?.('economics')}
          />
        );
      case 'portfolio':
        return (
          <PortfolioSummaryWidget
            id={widget.id}
            onRemove={() => handleRemoveWidget(widget.id)}
            onNavigate={() => onNavigateToTab?.('portfolio')}
          />
        );
      case 'alerts':
        return (
          <AlertsWidget
            id={widget.id}
            onRemove={() => handleRemoveWidget(widget.id)}
            onNavigate={() => onNavigateToTab?.('monitoring')}
          />
        );
      case 'calendar':
        return (
          <CalendarWidget
            id={widget.id}
            onRemove={() => handleRemoveWidget(widget.id)}
          />
        );
      case 'quicktrade':
        return (
          <QuickTradeWidget
            id={widget.id}
            onRemove={() => handleRemoveWidget(widget.id)}
            onNavigate={() => onNavigateToTab?.('trading')}
          />
        );
      case 'geopolitics':
        return (
          <GeopoliticsWidget
            id={widget.id}
            onRemove={() => handleRemoveWidget(widget.id)}
            onNavigate={() => onNavigateToTab?.('geopolitics')}
          />
        );
      case 'performance':
        return (
          <PerformanceWidget
            id={widget.id}
            onRemove={() => handleRemoveWidget(widget.id)}
            onNavigate={() => onNavigateToTab?.('portfolio')}
          />
        );
      default:
        return <div>{t('widgets.unknown')}</div>;
    }
  };

  // Show loading screen while database initializes
  if (!dbInitialized) {
    return (
      <div style={{
        height: '100%',
        backgroundColor: colors.background,
        color: colors.text,
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        justifyContent: 'center',
        gap: '20px'
      }}>
        <div style={{
          width: '60px',
          height: '60px',
          border: '4px solid #404040',
          borderTop: '4px solid #ea580c',
          borderRadius: '50%',
          animation: 'spin 1s linear infinite'
        }} />
        <style>{`
          @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
          }
        `}</style>
        <div style={{ textAlign: 'center', maxWidth: '500px' }}>
          <h3 style={{ color: '#ea580c', fontSize: '18px', marginBottom: '10px' }}>
            {t('loading.title')}
          </h3>
          <p style={{ color: '#a3a3a3', fontSize: '13px', lineHeight: '1.5' }}>
            {t('loading.description')}
          </p>
          <p style={{ color: '#787878', fontSize: '11px', marginTop: '10px' }}>
            {t('loading.note')}
          </p>
        </div>
      </div>
    );
  }

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
              {t('header.title')}
            </span>
            <span style={{ color: colors.text }}>|</span>
            <span style={{ color: colors.secondary, fontSize: fontSize.small }}>
              ● {t('header.live')}
            </span>
            <span style={{ color: colors.text }}>|</span>
            <span style={{ color: colors.warning, fontSize: fontSize.body }}>
              {currentTime.toISOString().replace('T', ' ').substring(0, 19)} UTC
            </span>
            <span style={{ color: colors.text }}>|</span>
            <span style={{ color: colors.textMuted, fontSize: fontSize.small }}>
              {t('header.widgets')}: {widgets.length}
            </span>
          </div>

          <div style={{ display: 'flex', gap: '8px' }}>
            <button
              onClick={() => {
                const tour = createDashboardTabTour();
                tour.drive();
              }}
              style={{
                background: colors.info,
                color: colors.background,
                border: 'none',
                padding: '6px 10px',
                fontSize: fontSize.small,
                fontWeight: 'bold',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                borderRadius: '2px'
              }}
              title="Start dashboard tour"
            >
              <Info size={14} />
              HELP
            </button>
            <button
              id="dashboard-add-widget"
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
              {t('buttons.addWidget')}
            </button>
            <button
              id="dashboard-save"
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
              {t('buttons.saveLayout')}
            </button>
            <button
              id="dashboard-reset"
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
              {t('buttons.reset')}
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
          <span>💡 <span style={{ color: colors.text }}>{t('tips.drag')}</span></span>
          <span>🔧 <span style={{ color: colors.text }}>{t('tips.resize')}</span></span>
          <span>✖️ <span style={{ color: colors.text }}>{t('tips.remove')}</span></span>
          <span>📐 <span style={{ color: colors.text }}>{t('tips.responsive')}</span></span>
        </div>
      </div>

      {/* Main Content - Grid Layout */}
      <div
        id="dashboard-grid"
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
              {t('empty.noWidgets')}
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
              {t('empty.addFirst')}
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
              <div key={widget.id} className={`widget-${widget.type}`}>
                {renderWidget(widget)}
              </div>
            ))}
          </GridLayout>
        )}
      </div>

      {/* Status Bar */}
      <TabFooter
        tabName="DASHBOARD"
        leftInfo={[
          { label: t('status.version'), color: colors.textMuted },
          { label: `${t('header.widgets')}: ${widgets.length}`, color: colors.textMuted },
          { label: `${t('status.layout')}: ${widgets.length > 0 ? t('status.custom') : t('status.empty')}`, color: colors.textMuted },
        ]}
        statusInfo={
          <>
            {t('status.label')}: <span style={{ color: colors.secondary }}>{t('status.active')}</span>
            {dbInitialized && (
              <span style={{ marginLeft: '8px', color: colors.secondary }}>
                | {t('status.cache')}
              </span>
            )}
          </>
        }
        backgroundColor={colors.panel}
        borderColor={colors.textMuted}
      />

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
