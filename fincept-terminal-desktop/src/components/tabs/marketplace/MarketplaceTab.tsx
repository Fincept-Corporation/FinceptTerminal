import React, { useState, useEffect, useReducer, useCallback, useRef } from 'react';
import { useAuth } from '@/contexts/AuthContext';
import { MarketplaceApiService } from '@/services/marketplace/marketplaceApi';
import { useTranslation } from 'react-i18next';
import { withTimeout } from '@/services/core/apiUtils';
import { ErrorBoundary } from '@/components/common/ErrorBoundary';
import { showError } from '@/utils/notifications';
import {
  Search,
  Upload,
  ShoppingCart,
  Package,
  BarChart3,
  DollarSign,
  Shield,
  TrendingUp,
  Clock,
  AlertCircle,
} from 'lucide-react';
import {
  FINCEPT,
  API_TIMEOUT_MS,
  ScreenType,
  initialState,
  marketplaceReducer
} from './types';
import { TabButton } from './SharedComponents';
import { BrowseScreen } from './screens/BrowseScreen';
import { UploadScreen } from './screens/UploadScreen';
import { MyPurchasesScreen } from './screens/MyPurchasesScreen';
import { MyDatasetsScreen } from './screens/MyDatasetsScreen';
import { AnalyticsScreen } from './screens/AnalyticsScreen';
import { PricingScreen } from './screens/PricingScreen';
import { AdminStatsScreen, AdminPendingScreen, AdminRevenueScreen } from './screens/AdminScreens';

const MarketplaceTab: React.FC = () => {
  const { t } = useTranslation('marketplace');
  const { session } = useAuth();

  // State machine
  const [state, dispatch] = useReducer(marketplaceReducer, initialState);
  const {
    status,
    currentScreen,
    datasets,
    myUploadedDatasets,
    selectedDataset,
    selectedCategory,
    searchQuery,
    currentPage,
    totalPages,
    error
  } = state;

  // Cleanup refs for race condition prevention
  const mountedRef = useRef(true);
  const fetchIdRef = useRef(0);

  const isLoading = status === 'loading';

  // Upload state (kept separate as form state)
  const [uploadFile, setUploadFile] = useState<File | null>(null);
  const [uploadMetadata, setUploadMetadata] = useState({
    title: '',
    description: '',
    category: 'stocks',
    price_tier: 'free',
    tags: ''
  });

  const isGuestUser = session?.user_type === 'guest' || session?.api_key?.startsWith('fk_guest_');
  const isAdmin = (session?.user_info as any)?.is_admin;

  // Cleanup on unmount
  useEffect(() => {
    mountedRef.current = true;
    return () => {
      mountedRef.current = false;
    };
  }, []);

  // Fetch datasets with race condition prevention
  const fetchDatasets = useCallback(async () => {
    if (!session?.api_key) return;

    const currentFetchId = ++fetchIdRef.current;
    dispatch({ type: 'SET_LOADING' });

    try {
      const response = await withTimeout(
        MarketplaceApiService.browseDatasets(session.api_key, {
          category: selectedCategory === 'ALL' ? undefined : selectedCategory,
          search: searchQuery || undefined,
          page: currentPage,
          limit: 20,
          sort_by: 'uploaded_at',
          sort_order: 'desc'
        }),
        API_TIMEOUT_MS,
        'Browse datasets timeout'
      );

      // Prevent stale updates
      if (!mountedRef.current || currentFetchId !== fetchIdRef.current) return;

      if (response.success && response.data) {
        const responseData = response.data as any;
        const rawDatasets = responseData.data?.datasets || responseData.datasets || [];
        const pagination = responseData.data?.pagination || responseData.pagination || {};

        const transformedDatasets = rawDatasets.map((apiDataset: any) => ({
          id: apiDataset.id,
          title: apiDataset.title,
          description: apiDataset.description,
          category: apiDataset.category,
          price_tier: apiDataset.pricing?.tier || 'free',
          price_credits: apiDataset.pricing?.price_usd || 0,
          uploader_id: apiDataset.uploader?.id || 0,
          uploader_username: apiDataset.uploader?.username || 'Unknown',
          file_name: apiDataset.metadata?.file_name || '',
          file_size: (apiDataset.metadata?.file_size_mb || 0) * 1024 * 1024,
          downloads_count: apiDataset.statistics?.download_count || 0,
          rating: apiDataset.statistics?.rating || 0,
          tags: apiDataset.tags || [],
          status: apiDataset.status || 'approved',
          rejection_reason: apiDataset.rejection_reason,
          uploaded_at: apiDataset.uploaded_at,
          updated_at: apiDataset.updated_at || apiDataset.uploaded_at
        }));

        dispatch({ type: 'SET_DATASETS', datasets: transformedDatasets, totalPages: pagination.total_pages || responseData.total_pages || 1 });
        dispatch({ type: 'SET_READY' });
      } else {
        dispatch({ type: 'SET_READY' });
      }
    } catch (err) {
      if (!mountedRef.current || currentFetchId !== fetchIdRef.current) return;
      console.error('Failed to fetch datasets:', err);
      dispatch({ type: 'SET_ERROR', error: err instanceof Error ? err.message : 'Failed to fetch datasets' });
    }
  }, [session?.api_key, selectedCategory, searchQuery, currentPage]);

  const fetchMyDatasets = useCallback(async () => {
    if (!session?.api_key) return;

    try {
      const response = await withTimeout(
        MarketplaceApiService.getMyDatasets(session.api_key, 1, 100),
        API_TIMEOUT_MS,
        'Fetch my datasets timeout'
      );

      if (!mountedRef.current) return;

      if (response.success && response.data) {
        dispatch({ type: 'SET_MY_DATASETS', datasets: response.data.datasets || [] });
      }
    } catch (err) {
      // Non-critical, don't show error
      console.error('Failed to fetch my datasets:', err);
    }
  }, [session?.api_key]);

  useEffect(() => {
    fetchDatasets();
    fetchMyDatasets();
  }, [fetchDatasets, fetchMyDatasets]);

  // Handler functions for state updates
  const setCurrentScreen = useCallback((screen: ScreenType) => {
    dispatch({ type: 'SET_SCREEN', screen });
  }, []);

  const setSelectedDataset = useCallback((dataset: any | null) => {
    dispatch({ type: 'SET_SELECTED_DATASET', dataset });
  }, []);

  const setSelectedCategory = useCallback((category: string) => {
    dispatch({ type: 'SET_CATEGORY', category });
  }, []);

  const setSearchQuery = useCallback((query: string) => {
    dispatch({ type: 'SET_SEARCH', query });
  }, []);

  const setCurrentPage = useCallback((page: number) => {
    dispatch({ type: 'SET_PAGE', page });
  }, []);

  // Get unique categories
  const categories = [
    { name: 'ALL', count: datasets.length },
    ...Object.entries(
      datasets.reduce((acc, d) => {
        acc[d.category] = (acc[d.category] || 0) + 1;
        return acc;
      }, {} as Record<string, number>)
    ).map(([name, count]) => ({ name, count: count as number }))
  ];

  const filteredDatasets = datasets.filter(d =>
    (selectedCategory === 'ALL' || d.category === selectedCategory) &&
    (searchQuery === '' || d.title.toLowerCase().includes(searchQuery.toLowerCase()))
  );

  return (
    <div style={{
      width: '100%',
      height: '100%',
      backgroundColor: FINCEPT.DARK_BG,
      color: FINCEPT.WHITE,
      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden'
    }}>
      {/* Custom Scrollbar */}
      <style>{`
        .marketplace-scroll::-webkit-scrollbar { width: 6px; height: 6px; }
        .marketplace-scroll::-webkit-scrollbar-track { background: ${FINCEPT.DARK_BG}; }
        .marketplace-scroll::-webkit-scrollbar-thumb { background: ${FINCEPT.BORDER}; border-radius: 3px; }
        .marketplace-scroll::-webkit-scrollbar-thumb:hover { background: ${FINCEPT.MUTED}; }
      `}</style>

      {/* Top Navigation Bar */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `2px solid ${FINCEPT.ORANGE}`,
        padding: '8px 16px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        boxShadow: `0 2px 8px ${FINCEPT.ORANGE}20`,
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <span style={{
            fontSize: '12px',
            fontWeight: 700,
            color: FINCEPT.ORANGE,
            letterSpacing: '0.5px'
          }}>
            MARKETPLACE
          </span>
          <span style={{ color: FINCEPT.BORDER }}>|</span>
          <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
            <Package size={12} color={FINCEPT.CYAN} />
            <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
              {datasets.length} DATASETS
            </span>
          </div>
          {currentScreen === 'BROWSE' && (
            <>
              <span style={{ color: FINCEPT.BORDER }}>|</span>
              <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                <Upload size={12} color={FINCEPT.YELLOW} />
                <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                  {myUploadedDatasets.length} MY UPLOADS
                </span>
              </div>
            </>
          )}
        </div>

        {/* Tab Buttons */}
        <div style={{ display: 'flex', gap: '4px' }}>
          <TabButton
            active={currentScreen === 'BROWSE'}
            onClick={() => setCurrentScreen('BROWSE')}
            icon={<Search size={10} />}
            label="BROWSE"
          />
          <TabButton
            active={currentScreen === 'UPLOAD'}
            onClick={() => setCurrentScreen('UPLOAD')}
            icon={<Upload size={10} />}
            label="UPLOAD"
          />
          <TabButton
            active={currentScreen === 'MY_PURCHASES'}
            onClick={() => setCurrentScreen('MY_PURCHASES')}
            icon={<ShoppingCart size={10} />}
            label="PURCHASES"
          />
          <TabButton
            active={currentScreen === 'MY_DATASETS'}
            onClick={() => setCurrentScreen('MY_DATASETS')}
            icon={<Package size={10} />}
            label="MY DATASETS"
          />
          <TabButton
            active={currentScreen === 'ANALYTICS'}
            onClick={() => setCurrentScreen('ANALYTICS')}
            icon={<BarChart3 size={10} />}
            label="ANALYTICS"
          />
          <TabButton
            active={currentScreen === 'PRICING'}
            onClick={() => setCurrentScreen('PRICING')}
            icon={<DollarSign size={10} />}
            label="PRICING"
          />
          {isAdmin && (
            <>
              <TabButton
                active={currentScreen === 'ADMIN_STATS'}
                onClick={() => setCurrentScreen('ADMIN_STATS')}
                icon={<BarChart3 size={10} />}
                label="ADMIN"
              />
              <TabButton
                active={currentScreen === 'ADMIN_PENDING'}
                onClick={() => setCurrentScreen('ADMIN_PENDING')}
                icon={<Clock size={10} />}
                label="PENDING"
              />
              <TabButton
                active={currentScreen === 'ADMIN_REVENUE'}
                onClick={() => setCurrentScreen('ADMIN_REVENUE')}
                icon={<TrendingUp size={10} />}
                label="REVENUE"
              />
            </>
          )}
        </div>
      </div>

      {/* Guest Warning Banner */}
      {isGuestUser && (
        <div style={{
          backgroundColor: `${FINCEPT.ORANGE}20`,
          borderBottom: `1px solid ${FINCEPT.ORANGE}`,
          padding: '8px 16px',
          display: 'flex',
          alignItems: 'center',
          gap: '12px',
          flexShrink: 0
        }}>
          <AlertCircle size={16} color={FINCEPT.ORANGE} />
          <span style={{ fontSize: '9px', color: FINCEPT.WHITE, fontWeight: 700 }}>
            GUEST MODE: Register to upload datasets and make purchases
          </span>
        </div>
      )}

      {/* Main Content */}
      <ErrorBoundary name="MarketplaceContent" variant="minimal">
      <div style={{ flex: 1, overflow: 'hidden' }}>
        {currentScreen === 'BROWSE' && (
          <ErrorBoundary name="BrowseScreen" variant="minimal">
          <BrowseScreen
            datasets={filteredDatasets}
            categories={categories}
            selectedCategory={selectedCategory}
            onCategorySelect={setSelectedCategory}
            searchQuery={searchQuery}
            onSearchChange={setSearchQuery}
            selectedDataset={selectedDataset}
            onDatasetSelect={setSelectedDataset}
            isLoading={isLoading}
            onRefresh={() => { fetchDatasets(); fetchMyDatasets(); }}
          />
          </ErrorBoundary>
        )}
        {currentScreen === 'UPLOAD' && (
          <ErrorBoundary name="UploadScreen" variant="minimal">
          <UploadScreen
            uploadFile={uploadFile}
            onFileChange={setUploadFile}
            uploadMetadata={uploadMetadata}
            onMetadataChange={setUploadMetadata}
            isLoading={isLoading}
            session={session}
            onUploadComplete={() => {
              fetchDatasets();
              fetchMyDatasets();
              setCurrentScreen('BROWSE');
            }}
          />
          </ErrorBoundary>
        )}
        {currentScreen === 'MY_PURCHASES' && (
          <ErrorBoundary name="MyPurchasesScreen" variant="minimal">
            <MyPurchasesScreen session={session} />
          </ErrorBoundary>
        )}
        {currentScreen === 'MY_DATASETS' && (
          <ErrorBoundary name="MyDatasetsScreen" variant="minimal">
            <MyDatasetsScreen session={session} onRefresh={() => { fetchDatasets(); fetchMyDatasets(); }} />
          </ErrorBoundary>
        )}
        {currentScreen === 'ANALYTICS' && (
          <ErrorBoundary name="AnalyticsScreen" variant="minimal">
            <AnalyticsScreen session={session} />
          </ErrorBoundary>
        )}
        {currentScreen === 'PRICING' && (
          <ErrorBoundary name="PricingScreen" variant="minimal">
            <PricingScreen session={session} />
          </ErrorBoundary>
        )}
        {currentScreen === 'ADMIN_STATS' && (
          <ErrorBoundary name="AdminStatsScreen" variant="minimal">
            <AdminStatsScreen session={session} />
          </ErrorBoundary>
        )}
        {currentScreen === 'ADMIN_PENDING' && (
          <ErrorBoundary name="AdminPendingScreen" variant="minimal">
            <AdminPendingScreen session={session} onRefresh={fetchDatasets} />
          </ErrorBoundary>
        )}
        {currentScreen === 'ADMIN_REVENUE' && (
          <ErrorBoundary name="AdminRevenueScreen" variant="minimal">
            <AdminRevenueScreen session={session} />
          </ErrorBoundary>
        )}
      </div>
      </ErrorBoundary>

      {/* Status Bar */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderTop: `1px solid ${FINCEPT.BORDER}`,
        padding: '4px 16px',
        fontSize: '9px',
        color: FINCEPT.GRAY,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', gap: '16px' }}>
          <span>SCREEN: <span style={{ color: FINCEPT.ORANGE }}>{currentScreen}</span></span>
          <span>DATASETS: <span style={{ color: FINCEPT.CYAN }}>{filteredDatasets.length}</span></span>
        </div>
        <div style={{ display: 'flex', gap: '16px', alignItems: 'center' }}>
          <span>STATUS: <span style={{ color: FINCEPT.GREEN }}>CONNECTED</span></span>
          <Shield size={10} color={FINCEPT.GREEN} />
        </div>
      </div>
    </div>
  );
};

export default MarketplaceTab;
