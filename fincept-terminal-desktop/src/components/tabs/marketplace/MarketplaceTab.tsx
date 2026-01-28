import React, { useState, useEffect } from 'react';
import { useAuth } from '@/contexts/AuthContext';
import { MarketplaceApiService, Dataset } from '@/services/marketplace/marketplaceApi';
import { useTranslation } from 'react-i18next';
import {
  Search,
  Upload,
  ShoppingCart,
  Package,
  BarChart3,
  DollarSign,
  Shield,
  TrendingUp,
  Download,
  Edit,
  Trash2,
  Check,
  X,
  Clock,
  AlertCircle,
  Filter,
  RefreshCw
} from 'lucide-react';

// Design System Colors
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
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
};

const FONT_FAMILY = '"IBM Plex Mono", "Consolas", monospace';

type ScreenType = 'BROWSE' | 'UPLOAD' | 'MY_PURCHASES' | 'MY_DATASETS' | 'ANALYTICS' | 'PRICING' | 'ADMIN_STATS' | 'ADMIN_PENDING' | 'ADMIN_REVENUE';

const MarketplaceTab: React.FC = () => {
  const { t } = useTranslation('marketplace');
  const { session } = useAuth();

  // State
  const [currentScreen, setCurrentScreen] = useState<ScreenType>('BROWSE');
  const [datasets, setDatasets] = useState<Dataset[]>([]);
  const [myUploadedDatasets, setMyUploadedDatasets] = useState<Dataset[]>([]);
  const [selectedDataset, setSelectedDataset] = useState<Dataset | null>(null);
  const [selectedCategory, setSelectedCategory] = useState('ALL');
  const [searchQuery, setSearchQuery] = useState('');
  const [isLoading, setIsLoading] = useState(false);
  const [currentPage, setCurrentPage] = useState(1);
  const [totalPages, setTotalPages] = useState(1);

  // Upload state
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

  // Fetch datasets
  const fetchDatasets = async () => {
    if (!session?.api_key) return;
    setIsLoading(true);
    try {
      const response = await MarketplaceApiService.browseDatasets(session.api_key, {
        category: selectedCategory === 'ALL' ? undefined : selectedCategory,
        search: searchQuery || undefined,
        page: currentPage,
        limit: 20,
        sort_by: 'uploaded_at',
        sort_order: 'desc'
      });

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

        setDatasets(transformedDatasets);
        setTotalPages(pagination.total_pages || responseData.total_pages || 1);
      }
    } catch (error) {
      console.error('Failed to fetch datasets:', error);
    } finally {
      setIsLoading(false);
    }
  };

  const fetchMyDatasets = async () => {
    if (!session?.api_key) return;
    try {
      const response = await MarketplaceApiService.getMyDatasets(session.api_key, 1, 100);
      if (response.success && response.data) {
        setMyUploadedDatasets(response.data.datasets || []);
      }
    } catch (error) {
      console.error('Failed to fetch my datasets:', error);
    }
  };

  useEffect(() => {
    fetchDatasets();
    fetchMyDatasets();
  }, [session?.api_key, selectedCategory, searchQuery, currentPage]);

  // Get unique categories
  const categories = [
    { name: 'ALL', count: datasets.length },
    ...Object.entries(
      datasets.reduce((acc, d) => {
        acc[d.category] = (acc[d.category] || 0) + 1;
        return acc;
      }, {} as Record<string, number>)
    ).map(([name, count]) => ({ name, count }))
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
      fontFamily: FONT_FAMILY,
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
      <div style={{ flex: 1, overflow: 'hidden' }}>
        {currentScreen === 'BROWSE' && (
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
        )}
        {currentScreen === 'UPLOAD' && (
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
        )}
        {currentScreen === 'MY_PURCHASES' && <MyPurchasesScreen session={session} />}
        {currentScreen === 'MY_DATASETS' && <MyDatasetsScreen session={session} onRefresh={() => { fetchDatasets(); fetchMyDatasets(); }} />}
        {currentScreen === 'ANALYTICS' && <AnalyticsScreen session={session} />}
        {currentScreen === 'PRICING' && <PricingScreen session={session} />}
        {currentScreen === 'ADMIN_STATS' && <AdminStatsScreen session={session} />}
        {currentScreen === 'ADMIN_PENDING' && <AdminPendingScreen session={session} onRefresh={fetchDatasets} />}
        {currentScreen === 'ADMIN_REVENUE' && <AdminRevenueScreen session={session} />}
      </div>

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

// Tab Button Component
const TabButton: React.FC<{ active: boolean; onClick: () => void; icon: React.ReactNode; label: string }> = ({ active, onClick, icon, label }) => (
  <button
    onClick={onClick}
    style={{
      padding: '6px 12px',
      backgroundColor: active ? FINCEPT.ORANGE : 'transparent',
      color: active ? FINCEPT.DARK_BG : FINCEPT.GRAY,
      border: 'none',
      fontSize: '9px',
      fontWeight: 700,
      letterSpacing: '0.5px',
      cursor: 'pointer',
      display: 'flex',
      alignItems: 'center',
      gap: '4px',
      borderRadius: '2px',
      transition: 'all 0.2s',
      fontFamily: FONT_FAMILY
    }}
    onMouseEnter={(e) => {
      if (!active) {
        e.currentTarget.style.color = FINCEPT.WHITE;
        e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
      }
    }}
    onMouseLeave={(e) => {
      if (!active) {
        e.currentTarget.style.color = FINCEPT.GRAY;
        e.currentTarget.style.backgroundColor = 'transparent';
      }
    }}
  >
    {icon}
    {label}
  </button>
);

// Browse Screen Component
const BrowseScreen: React.FC<{
  datasets: Dataset[];
  categories: Array<{ name: string; count: number }>;
  selectedCategory: string;
  onCategorySelect: (cat: string) => void;
  searchQuery: string;
  onSearchChange: (q: string) => void;
  selectedDataset: Dataset | null;
  onDatasetSelect: (d: Dataset | null) => void;
  isLoading: boolean;
  onRefresh: () => void;
}> = ({ datasets, categories, selectedCategory, onCategorySelect, searchQuery, onSearchChange, selectedDataset, onDatasetSelect, isLoading, onRefresh }) => {
  return (
    <div style={{ display: 'flex', height: '100%', overflow: 'hidden' }}>
      {/* Left Panel - Categories & Filters */}
      <div style={{
        width: '280px',
        backgroundColor: FINCEPT.PANEL_BG,
        borderRight: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden'
      }}>
        {/* Section Header */}
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`
        }}>
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
              <Filter size={12} color={FINCEPT.ORANGE} />
              <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
                CATEGORIES
              </span>
            </div>
            <button
              onClick={onRefresh}
              style={{
                padding: '4px',
                backgroundColor: 'transparent',
                border: 'none',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center'
              }}
            >
              <RefreshCw size={10} color={FINCEPT.GRAY} />
            </button>
          </div>
        </div>

        {/* Categories List */}
        <div className="marketplace-scroll" style={{ flex: 1, overflowY: 'auto' }}>
          {categories.map((cat) => (
            <div
              key={cat.name}
              onClick={() => onCategorySelect(cat.name)}
              style={{
                padding: '10px 12px',
                backgroundColor: selectedCategory === cat.name ? `${FINCEPT.ORANGE}15` : 'transparent',
                borderLeft: selectedCategory === cat.name ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                cursor: 'pointer',
                transition: 'all 0.2s',
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center'
              }}
              onMouseEnter={(e) => {
                if (selectedCategory !== cat.name) {
                  e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                }
              }}
              onMouseLeave={(e) => {
                if (selectedCategory !== cat.name) {
                  e.currentTarget.style.backgroundColor = 'transparent';
                }
              }}
            >
              <span style={{
                fontSize: '10px',
                color: selectedCategory === cat.name ? FINCEPT.WHITE : FINCEPT.GRAY,
                fontWeight: 700,
                letterSpacing: '0.5px'
              }}>
                {cat.name}
              </span>
              <span style={{
                fontSize: '9px',
                color: selectedCategory === cat.name ? FINCEPT.CYAN : FINCEPT.MUTED,
                fontWeight: 700
              }}>
                {cat.count}
              </span>
            </div>
          ))}
        </div>
      </div>

      {/* Center Panel - Dataset Grid */}
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
        {/* Search Bar */}
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`
        }}>
          <div style={{ position: 'relative' }}>
            <Search size={14} color={FINCEPT.GRAY} style={{ position: 'absolute', left: '10px', top: '50%', transform: 'translateY(-50%)' }} />
            <input
              type="text"
              placeholder="Search datasets..."
              value={searchQuery}
              onChange={(e) => onSearchChange(e.target.value)}
              style={{
                width: '100%',
                padding: '8px 10px 8px 32px',
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '10px',
                fontFamily: FONT_FAMILY,
                outline: 'none'
              }}
              onFocus={(e) => e.currentTarget.style.borderColor = FINCEPT.ORANGE}
              onBlur={(e) => e.currentTarget.style.borderColor = FINCEPT.BORDER}
            />
          </div>
        </div>

        {/* Dataset Grid */}
        <div className="marketplace-scroll" style={{
          flex: 1,
          overflowY: 'auto',
          padding: '12px',
          display: 'grid',
          gridTemplateColumns: 'repeat(auto-fill, minmax(320px, 1fr))',
          gap: '12px',
          alignContent: 'start'
        }}>
          {isLoading ? (
            <div style={{
              gridColumn: '1 / -1',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              height: '200px',
              color: FINCEPT.MUTED,
              fontSize: '10px'
            }}>
              Loading datasets...
            </div>
          ) : datasets.length === 0 ? (
            <div style={{
              gridColumn: '1 / -1',
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              justifyContent: 'center',
              height: '200px',
              color: FINCEPT.MUTED,
              fontSize: '10px',
              textAlign: 'center'
            }}>
              <Package size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
              <span>No datasets found</span>
            </div>
          ) : (
            datasets.map((dataset) => (
              <DatasetCard
                key={dataset.id}
                dataset={dataset}
                selected={selectedDataset?.id === dataset.id}
                onClick={() => onDatasetSelect(dataset)}
              />
            ))
          )}
        </div>
      </div>

      {/* Right Panel - Dataset Details */}
      <div style={{
        width: '320px',
        backgroundColor: FINCEPT.PANEL_BG,
        borderLeft: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden'
      }}>
        {selectedDataset ? (
          <DatasetDetails dataset={selectedDataset} onClose={() => onDatasetSelect(null)} />
        ) : (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '100%',
            color: FINCEPT.MUTED,
            fontSize: '10px',
            textAlign: 'center',
            padding: '16px'
          }}>
            <Package size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
            <span>Select a dataset to view details</span>
          </div>
        )}
      </div>
    </div>
  );
};

// Dataset Card Component
const DatasetCard: React.FC<{ dataset: Dataset; selected: boolean; onClick: () => void }> = ({ dataset, selected, onClick }) => {
  const statusColor = dataset.status === 'approved' ? FINCEPT.GREEN : dataset.status === 'pending' ? FINCEPT.YELLOW : FINCEPT.RED;

  return (
    <div
      onClick={onClick}
      style={{
        backgroundColor: selected ? `${FINCEPT.ORANGE}10` : FINCEPT.PANEL_BG,
        border: selected ? `1px solid ${FINCEPT.ORANGE}` : `1px solid ${FINCEPT.BORDER}`,
        borderRadius: '2px',
        padding: '12px',
        cursor: 'pointer',
        transition: 'all 0.2s'
      }}
      onMouseEnter={(e) => {
        if (!selected) {
          e.currentTarget.style.borderColor = FINCEPT.ORANGE;
        }
      }}
      onMouseLeave={(e) => {
        if (!selected) {
          e.currentTarget.style.borderColor = FINCEPT.BORDER;
        }
      }}
    >
      {/* Header */}
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: '8px' }}>
        <div style={{
          padding: '2px 6px',
          backgroundColor: `${FINCEPT.CYAN}20`,
          color: FINCEPT.CYAN,
          fontSize: '8px',
          fontWeight: 700,
          borderRadius: '2px',
          letterSpacing: '0.5px'
        }}>
          {dataset.category.toUpperCase()}
        </div>
        {dataset.status && dataset.status !== 'approved' && (
          <div style={{
            padding: '2px 6px',
            backgroundColor: `${statusColor}20`,
            color: statusColor,
            fontSize: '8px',
            fontWeight: 700,
            borderRadius: '2px',
            letterSpacing: '0.5px'
          }}>
            {dataset.status.toUpperCase()}
          </div>
        )}
      </div>

      {/* Title */}
      <div style={{
        fontSize: '11px',
        fontWeight: 700,
        color: FINCEPT.WHITE,
        marginBottom: '4px',
        overflow: 'hidden',
        textOverflow: 'ellipsis',
        whiteSpace: 'nowrap'
      }}>
        {dataset.title}
      </div>

      {/* Author */}
      <div style={{
        fontSize: '9px',
        color: FINCEPT.GRAY,
        marginBottom: '8px'
      }}>
        by <span style={{ color: FINCEPT.CYAN }}>{dataset.uploader_username}</span>
      </div>

      {/* Description */}
      <div style={{
        fontSize: '10px',
        color: FINCEPT.GRAY,
        lineHeight: '1.4',
        marginBottom: '12px',
        overflow: 'hidden',
        textOverflow: 'ellipsis',
        display: '-webkit-box',
        WebkitLineClamp: 2,
        WebkitBoxOrient: 'vertical'
      }}>
        {dataset.description}
      </div>

      {/* Stats */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: '1fr 1fr 1fr',
        gap: '8px',
        padding: '8px',
        backgroundColor: FINCEPT.DARK_BG,
        borderRadius: '2px'
      }}>
        <div style={{ textAlign: 'center' }}>
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '2px' }}>DOWNLOADS</div>
          <div style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.CYAN }}>{dataset.downloads_count}</div>
        </div>
        <div style={{ textAlign: 'center' }}>
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '2px' }}>RATING</div>
          <div style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.YELLOW }}>★ {dataset.rating.toFixed(1)}</div>
        </div>
        <div style={{ textAlign: 'center' }}>
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '2px' }}>PRICE</div>
          <div style={{ fontSize: '11px', fontWeight: 700, color: dataset.price_credits > 0 ? FINCEPT.GREEN : FINCEPT.GRAY }}>
            {dataset.price_credits > 0 ? `${dataset.price_credits}c` : 'FREE'}
          </div>
        </div>
      </div>
    </div>
  );
};

// Dataset Details Component
const DatasetDetails: React.FC<{ dataset: Dataset; onClose: () => void }> = ({ dataset, onClose }) => {
  const { session } = useAuth();
  const [isDownloading, setIsDownloading] = useState(false);

  const handleDownload = async () => {
    if (!session?.api_key) return;
    setIsDownloading(true);
    try {
      const response = await MarketplaceApiService.downloadDataset(session.api_key, dataset.id);
      if (response.success && response.data?.download_url) {
        window.open(response.data.download_url, '_blank');
      } else {
        alert('Download failed');
      }
    } catch (error) {
      alert('Download failed');
    } finally {
      setIsDownloading(false);
    }
  };

  const handlePurchase = async () => {
    if (!session?.api_key) return;
    try {
      const response = await MarketplaceApiService.purchaseDataset(session.api_key, dataset.id);
      if (response.success) {
        alert('Purchase successful!');
      } else {
        alert('Purchase failed');
      }
    } catch (error) {
      alert('Purchase failed');
    }
  };

  return (
    <>
      {/* Header */}
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center'
      }}>
        <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
          DATASET DETAILS
        </span>
        <button
          onClick={onClose}
          style={{
            padding: '4px',
            backgroundColor: 'transparent',
            border: 'none',
            cursor: 'pointer'
          }}
        >
          <X size={12} color={FINCEPT.GRAY} />
        </button>
      </div>

      {/* Content */}
      <div className="marketplace-scroll" style={{ flex: 1, overflowY: 'auto', padding: '12px' }}>
        {/* Category Badge */}
        <div style={{
          padding: '2px 6px',
          backgroundColor: `${FINCEPT.CYAN}20`,
          color: FINCEPT.CYAN,
          fontSize: '8px',
          fontWeight: 700,
          borderRadius: '2px',
          display: 'inline-block',
          marginBottom: '12px',
          letterSpacing: '0.5px'
        }}>
          {dataset.category.toUpperCase()}
        </div>

        {/* Title */}
        <h3 style={{
          fontSize: '13px',
          fontWeight: 700,
          color: FINCEPT.WHITE,
          marginBottom: '4px',
          lineHeight: '1.4'
        }}>
          {dataset.title}
        </h3>

        {/* Author */}
        <div style={{
          fontSize: '9px',
          color: FINCEPT.GRAY,
          marginBottom: '16px'
        }}>
          by <span style={{ color: FINCEPT.CYAN }}>{dataset.uploader_username}</span>
        </div>

        {/* Price */}
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.DARK_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
          marginBottom: '16px',
          textAlign: 'center'
        }}>
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px' }}>PRICE</div>
          <div style={{
            fontSize: '20px',
            fontWeight: 700,
            color: dataset.price_credits > 0 ? FINCEPT.GREEN : FINCEPT.ORANGE
          }}>
            {dataset.price_credits > 0 ? `${dataset.price_credits} CREDITS` : 'FREE'}
          </div>
        </div>

        {/* Stats Grid */}
        <div style={{
          display: 'grid',
          gridTemplateColumns: '1fr 1fr',
          gap: '8px',
          marginBottom: '16px'
        }}>
          <StatBox label="DOWNLOADS" value={dataset.downloads_count.toString()} color={FINCEPT.CYAN} />
          <StatBox label="RATING" value={`★ ${dataset.rating.toFixed(1)}`} color={FINCEPT.YELLOW} />
          <StatBox label="FILE SIZE" value={`${(dataset.file_size / 1024 / 1024).toFixed(2)} MB`} color={FINCEPT.GRAY} />
          <StatBox label="UPLOADED" value={new Date(dataset.uploaded_at).toLocaleDateString()} color={FINCEPT.GRAY} />
        </div>

        {/* Description */}
        <div style={{ marginBottom: '16px' }}>
          <div style={{
            fontSize: '9px',
            fontWeight: 700,
            color: FINCEPT.GRAY,
            marginBottom: '8px',
            letterSpacing: '0.5px'
          }}>
            DESCRIPTION
          </div>
          <div style={{
            fontSize: '10px',
            color: FINCEPT.WHITE,
            lineHeight: '1.6',
            backgroundColor: FINCEPT.DARK_BG,
            padding: '12px',
            borderRadius: '2px'
          }}>
            {dataset.description}
          </div>
        </div>

        {/* Tags */}
        {dataset.tags && dataset.tags.length > 0 && (
          <div style={{ marginBottom: '16px' }}>
            <div style={{
              fontSize: '9px',
              fontWeight: 700,
              color: FINCEPT.GRAY,
              marginBottom: '8px',
              letterSpacing: '0.5px'
            }}>
              TAGS
            </div>
            <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px' }}>
              {dataset.tags.map((tag, idx) => (
                <span key={idx} style={{
                  padding: '4px 8px',
                  backgroundColor: `${FINCEPT.PURPLE}20`,
                  color: FINCEPT.PURPLE,
                  fontSize: '9px',
                  fontWeight: 700,
                  borderRadius: '2px'
                }}>
                  #{tag}
                </span>
              ))}
            </div>
          </div>
        )}

        {/* Actions */}
        <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
          {dataset.price_credits > 0 ? (
            <button
              onClick={handlePurchase}
              style={{
                padding: '8px 16px',
                backgroundColor: FINCEPT.GREEN,
                color: FINCEPT.DARK_BG,
                border: 'none',
                borderRadius: '2px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: 'pointer',
                fontFamily: FONT_FAMILY,
                letterSpacing: '0.5px'
              }}
            >
              PURCHASE FOR {dataset.price_credits} CREDITS
            </button>
          ) : (
            <button
              onClick={handleDownload}
              disabled={isDownloading}
              style={{
                padding: '8px 16px',
                backgroundColor: FINCEPT.ORANGE,
                color: FINCEPT.DARK_BG,
                border: 'none',
                borderRadius: '2px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: isDownloading ? 'not-allowed' : 'pointer',
                fontFamily: FONT_FAMILY,
                letterSpacing: '0.5px',
                opacity: isDownloading ? 0.7 : 1,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '6px'
              }}
            >
              <Download size={10} />
              {isDownloading ? 'DOWNLOADING...' : 'DOWNLOAD FREE'}
            </button>
          )}
        </div>
      </div>
    </>
  );
};

// Stat Box Component
const StatBox: React.FC<{ label: string; value: string; color: string }> = ({ label, value, color }) => (
  <div style={{
    padding: '8px',
    backgroundColor: FINCEPT.DARK_BG,
    border: `1px solid ${FINCEPT.BORDER}`,
    borderRadius: '2px',
    textAlign: 'center'
  }}>
    <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px', letterSpacing: '0.5px' }}>
      {label}
    </div>
    <div style={{ fontSize: '11px', fontWeight: 700, color }}>
      {value}
    </div>
  </div>
);

// Upload Screen Component
const UploadScreen: React.FC<{
  uploadFile: File | null;
  onFileChange: (file: File | null) => void;
  uploadMetadata: any;
  onMetadataChange: (metadata: any) => void;
  isLoading: boolean;
  session: any;
  onUploadComplete: () => void;
}> = ({ uploadFile, onFileChange, uploadMetadata, onMetadataChange, isLoading, session, onUploadComplete }) => {
  const handleUpload = async () => {
    if (!session?.api_key || !uploadFile) {
      alert('Please select a file and fill in all required fields');
      return;
    }

    try {
      const response = await MarketplaceApiService.uploadDataset(
        session.api_key,
        uploadFile,
        {
          title: uploadMetadata.title,
          description: uploadMetadata.description,
          category: uploadMetadata.category,
          price_tier: uploadMetadata.price_tier,
          tags: uploadMetadata.tags ? uploadMetadata.tags.split(',').map((t: string) => t.trim()) : []
        }
      );

      if (response.success) {
        alert('Dataset uploaded successfully!');
        onFileChange(null);
        onMetadataChange({
          title: '',
          description: '',
          category: 'stocks',
          price_tier: 'free',
          tags: ''
        });
        setTimeout(() => onUploadComplete(), 1000);
      } else {
        alert(`Upload failed: ${response.error}`);
      }
    } catch (error) {
      alert('Upload failed');
    }
  };

  return (
    <div style={{ display: 'flex', height: '100%', overflow: 'hidden' }}>
      {/* Left Panel - Guidelines */}
      <div style={{
        width: '320px',
        backgroundColor: FINCEPT.PANEL_BG,
        borderRight: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden'
      }}>
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`
        }}>
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
            UPLOAD GUIDELINES
          </span>
        </div>
        <div className="marketplace-scroll" style={{ flex: 1, overflowY: 'auto', padding: '12px' }}>
          <div style={{ marginBottom: '16px' }}>
            <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.ORANGE, marginBottom: '8px' }}>
              SUPPORTED FORMATS
            </div>
            <div style={{ fontSize: '10px', color: FINCEPT.GRAY, lineHeight: '1.6' }}>
              CSV, JSON, XLSX, Parquet, HDF5
            </div>
          </div>
          <div style={{ marginBottom: '16px' }}>
            <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.ORANGE, marginBottom: '8px' }}>
              FILE SIZE LIMIT
            </div>
            <div style={{ fontSize: '10px', color: FINCEPT.GRAY, lineHeight: '1.6' }}>
              Maximum 500 MB per file
            </div>
          </div>
          <div style={{ marginBottom: '16px' }}>
            <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.ORANGE, marginBottom: '8px' }}>
              APPROVAL PROCESS
            </div>
            <div style={{ fontSize: '10px', color: FINCEPT.GRAY, lineHeight: '1.6' }}>
              All datasets undergo admin review before becoming publicly available. You'll be notified via email once approved.
            </div>
          </div>
          <div style={{ marginBottom: '16px' }}>
            <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.ORANGE, marginBottom: '8px' }}>
              PRICING TIERS
            </div>
            <div style={{ fontSize: '10px', color: FINCEPT.GRAY, lineHeight: '1.6' }}>
              • FREE: No cost<br />
              • BASIC: 10 credits<br />
              • PREMIUM: 50 credits<br />
              • ENTERPRISE: 100 credits
            </div>
          </div>
        </div>
      </div>

      {/* Center Panel - Upload Form */}
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`
        }}>
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
            UPLOAD NEW DATASET
          </span>
        </div>
        <div className="marketplace-scroll" style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>
          <div style={{ maxWidth: '600px', margin: '0 auto' }}>
            {/* Title Input */}
            <div style={{ marginBottom: '16px' }}>
              <label style={{
                display: 'block',
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.GRAY,
                marginBottom: '6px',
                letterSpacing: '0.5px'
              }}>
                DATASET TITLE *
              </label>
              <input
                type="text"
                value={uploadMetadata.title}
                onChange={(e) => onMetadataChange({ ...uploadMetadata, title: e.target.value })}
                placeholder="Enter descriptive title..."
                style={{
                  width: '100%',
                  padding: '8px 10px',
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '10px',
                  fontFamily: FONT_FAMILY,
                  outline: 'none'
                }}
                onFocus={(e) => e.currentTarget.style.borderColor = FINCEPT.ORANGE}
                onBlur={(e) => e.currentTarget.style.borderColor = FINCEPT.BORDER}
              />
            </div>

            {/* Description Textarea */}
            <div style={{ marginBottom: '16px' }}>
              <label style={{
                display: 'block',
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.GRAY,
                marginBottom: '6px',
                letterSpacing: '0.5px'
              }}>
                DESCRIPTION *
              </label>
              <textarea
                value={uploadMetadata.description}
                onChange={(e) => onMetadataChange({ ...uploadMetadata, description: e.target.value })}
                placeholder="Describe your dataset, its contents, and use cases..."
                style={{
                  width: '100%',
                  padding: '8px 10px',
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '10px',
                  fontFamily: FONT_FAMILY,
                  minHeight: '120px',
                  outline: 'none',
                  resize: 'vertical'
                }}
                onFocus={(e) => e.currentTarget.style.borderColor = FINCEPT.ORANGE}
                onBlur={(e) => e.currentTarget.style.borderColor = FINCEPT.BORDER}
              />
            </div>

            {/* Category & Price Tier Row */}
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px', marginBottom: '16px' }}>
              <div>
                <label style={{
                  display: 'block',
                  fontSize: '9px',
                  fontWeight: 700,
                  color: FINCEPT.GRAY,
                  marginBottom: '6px',
                  letterSpacing: '0.5px'
                }}>
                  CATEGORY *
                </label>
                <select
                  value={uploadMetadata.category}
                  onChange={(e) => onMetadataChange({ ...uploadMetadata, category: e.target.value })}
                  style={{
                    width: '100%',
                    padding: '8px 10px',
                    backgroundColor: FINCEPT.DARK_BG,
                    color: FINCEPT.WHITE,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderRadius: '2px',
                    fontSize: '10px',
                    fontFamily: FONT_FAMILY,
                    outline: 'none'
                  }}
                  onFocus={(e) => e.currentTarget.style.borderColor = FINCEPT.ORANGE}
                  onBlur={(e) => e.currentTarget.style.borderColor = FINCEPT.BORDER}
                >
                  <option value="stocks">Stocks</option>
                  <option value="forex">Forex</option>
                  <option value="crypto">Cryptocurrency</option>
                  <option value="commodities">Commodities</option>
                  <option value="bonds">Bonds</option>
                  <option value="indices">Indices</option>
                  <option value="economic_data">Economic Data</option>
                </select>
              </div>
              <div>
                <label style={{
                  display: 'block',
                  fontSize: '9px',
                  fontWeight: 700,
                  color: FINCEPT.GRAY,
                  marginBottom: '6px',
                  letterSpacing: '0.5px'
                }}>
                  PRICE TIER *
                </label>
                <select
                  value={uploadMetadata.price_tier}
                  onChange={(e) => onMetadataChange({ ...uploadMetadata, price_tier: e.target.value })}
                  style={{
                    width: '100%',
                    padding: '8px 10px',
                    backgroundColor: FINCEPT.DARK_BG,
                    color: FINCEPT.WHITE,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderRadius: '2px',
                    fontSize: '10px',
                    fontFamily: FONT_FAMILY,
                    outline: 'none'
                  }}
                  onFocus={(e) => e.currentTarget.style.borderColor = FINCEPT.ORANGE}
                  onBlur={(e) => e.currentTarget.style.borderColor = FINCEPT.BORDER}
                >
                  <option value="free">Free (0 credits)</option>
                  <option value="basic">Basic (10 credits)</option>
                  <option value="premium">Premium (50 credits)</option>
                  <option value="enterprise">Enterprise (100 credits)</option>
                </select>
              </div>
            </div>

            {/* Tags Input */}
            <div style={{ marginBottom: '16px' }}>
              <label style={{
                display: 'block',
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.GRAY,
                marginBottom: '6px',
                letterSpacing: '0.5px'
              }}>
                TAGS (COMMA SEPARATED)
              </label>
              <input
                type="text"
                value={uploadMetadata.tags}
                onChange={(e) => onMetadataChange({ ...uploadMetadata, tags: e.target.value })}
                placeholder="stocks, equity, real-time, historical"
                style={{
                  width: '100%',
                  padding: '8px 10px',
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '10px',
                  fontFamily: FONT_FAMILY,
                  outline: 'none'
                }}
                onFocus={(e) => e.currentTarget.style.borderColor = FINCEPT.ORANGE}
                onBlur={(e) => e.currentTarget.style.borderColor = FINCEPT.BORDER}
              />
            </div>

            {/* File Upload */}
            <div style={{ marginBottom: '24px' }}>
              <label style={{
                display: 'block',
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.GRAY,
                marginBottom: '6px',
                letterSpacing: '0.5px'
              }}>
                DATASET FILE *
              </label>
              <div style={{
                border: `2px dashed ${uploadFile ? FINCEPT.GREEN : FINCEPT.BORDER}`,
                borderRadius: '2px',
                padding: '24px',
                textAlign: 'center',
                backgroundColor: FINCEPT.DARK_BG,
                cursor: 'pointer',
                transition: 'all 0.2s'
              }}
                onMouseEnter={(e) => e.currentTarget.style.borderColor = FINCEPT.ORANGE}
                onMouseLeave={(e) => e.currentTarget.style.borderColor = uploadFile ? FINCEPT.GREEN : FINCEPT.BORDER}
              >
                <input
                  type="file"
                  onChange={(e) => onFileChange(e.target.files?.[0] || null)}
                  style={{ display: 'none' }}
                  id="file-upload"
                />
                <label htmlFor="file-upload" style={{ cursor: 'pointer', display: 'block' }}>
                  {uploadFile ? (
                    <>
                      <Check size={24} color={FINCEPT.GREEN} style={{ marginBottom: '8px' }} />
                      <div style={{ fontSize: '10px', color: FINCEPT.WHITE, marginBottom: '4px' }}>
                        {uploadFile.name}
                      </div>
                      <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                        {(uploadFile.size / 1024 / 1024).toFixed(2)} MB
                      </div>
                    </>
                  ) : (
                    <>
                      <Upload size={24} color={FINCEPT.GRAY} style={{ marginBottom: '8px', opacity: 0.5 }} />
                      <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>
                        Click to select file or drag and drop
                      </div>
                    </>
                  )}
                </label>
              </div>
            </div>

            {/* Upload Button */}
            <button
              onClick={handleUpload}
              disabled={isLoading || !uploadFile || !uploadMetadata.title || !uploadMetadata.description}
              style={{
                width: '100%',
                padding: '12px',
                backgroundColor: isLoading || !uploadFile || !uploadMetadata.title || !uploadMetadata.description ? FINCEPT.MUTED : FINCEPT.ORANGE,
                color: FINCEPT.DARK_BG,
                border: 'none',
                borderRadius: '2px',
                fontSize: '9px',
                fontWeight: 700,
                letterSpacing: '0.5px',
                cursor: isLoading || !uploadFile || !uploadMetadata.title || !uploadMetadata.description ? 'not-allowed' : 'pointer',
                fontFamily: FONT_FAMILY,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '8px'
              }}
            >
              <Upload size={12} />
              {isLoading ? 'UPLOADING...' : 'UPLOAD DATASET'}
            </button>
          </div>
        </div>
      </div>
    </div>
  );
};

// My Purchases Screen Component
const MyPurchasesScreen: React.FC<{ session: any }> = ({ session }) => {
  const [purchases, setPurchases] = useState<any[]>([]);
  const [isLoading, setIsLoading] = useState(true);

  useEffect(() => {
    const fetchPurchases = async () => {
      if (!session?.api_key) return;
      setIsLoading(true);
      try {
        const response = await MarketplaceApiService.getUserPurchases(session.api_key, 1, 50);
        if (response.success && response.data) {
          setPurchases(response.data.purchases || []);
        }
      } catch (error) {
        console.error('Failed to fetch purchases:', error);
      } finally {
        setIsLoading(false);
      }
    };
    fetchPurchases();
  }, [session?.api_key]);

  const handleDownload = async (datasetId: number) => {
    if (!session?.api_key) return;
    try {
      const response = await MarketplaceApiService.downloadDataset(session.api_key, datasetId);
      if (response.success && response.data?.download_url) {
        window.open(response.data.download_url, '_blank');
      } else {
        alert('Download failed');
      }
    } catch (error) {
      alert('Download failed');
    }
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <ShoppingCart size={12} color={FINCEPT.ORANGE} />
            <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
              MY PURCHASES ({purchases.length})
            </span>
          </div>
        </div>
      </div>

      <div className="marketplace-scroll" style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>
        {isLoading ? (
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            height: '200px',
            color: FINCEPT.MUTED,
            fontSize: '10px'
          }}>
            Loading purchases...
          </div>
        ) : purchases.length === 0 ? (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '200px',
            color: FINCEPT.MUTED,
            fontSize: '10px',
            textAlign: 'center'
          }}>
            <ShoppingCart size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
            <span>No purchases yet</span>
          </div>
        ) : (
          <div style={{ maxWidth: '1000px', margin: '0 auto', display: 'flex', flexDirection: 'column', gap: '12px' }}>
            {purchases.map((purchase) => (
              <div key={purchase.id} style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderLeft: `4px solid ${FINCEPT.GREEN}`,
                borderRadius: '2px',
                padding: '16px',
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center'
              }}>
                <div style={{ flex: 1 }}>
                  <div style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE, marginBottom: '6px' }}>
                    {purchase.dataset_title}
                  </div>
                  <div style={{ fontSize: '9px', color: FINCEPT.GRAY, display: 'flex', gap: '16px' }}>
                    <span>PURCHASED: {new Date(purchase.purchased_at).toLocaleDateString()}</span>
                    <span>PAID: <span style={{ color: FINCEPT.GREEN }}>{purchase.price_paid} CREDITS</span></span>
                  </div>
                </div>
                <button
                  onClick={() => handleDownload(purchase.dataset_id)}
                  style={{
                    padding: '8px 16px',
                    backgroundColor: FINCEPT.ORANGE,
                    color: FINCEPT.DARK_BG,
                    border: 'none',
                    borderRadius: '2px',
                    fontSize: '9px',
                    fontWeight: 700,
                    cursor: 'pointer',
                    fontFamily: FONT_FAMILY,
                    letterSpacing: '0.5px',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '6px'
                  }}
                >
                  <Download size={10} />
                  DOWNLOAD
                </button>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
};

// My Datasets Screen Component
const MyDatasetsScreen: React.FC<{ session: any; onRefresh: () => void }> = ({ session, onRefresh }) => {
  const [myDatasets, setMyDatasets] = useState<Dataset[]>([]);
  const [isLoading, setIsLoading] = useState(true);
  const [editingDataset, setEditingDataset] = useState<Dataset | null>(null);
  const [editForm, setEditForm] = useState({
    title: '',
    description: '',
    category: '',
    price_tier: '',
    tags: ''
  });

  const loadMyDatasets = async () => {
    if (!session?.api_key) return;
    setIsLoading(true);
    try {
      const response = await MarketplaceApiService.getMyDatasets(session.api_key, 1, 100);
      if (response.success && response.data) {
        setMyDatasets(response.data.datasets || []);
      }
    } catch (error) {
      console.error('Failed to fetch my datasets:', error);
    } finally {
      setIsLoading(false);
    }
  };

  useEffect(() => {
    loadMyDatasets();
  }, [session?.api_key]);

  const handleEdit = (dataset: Dataset) => {
    setEditingDataset(dataset);
    setEditForm({
      title: dataset.title,
      description: dataset.description,
      category: dataset.category,
      price_tier: dataset.price_tier,
      tags: dataset.tags.join(', ')
    });
  };

  const handleUpdate = async () => {
    if (!session?.api_key || !editingDataset) return;
    setIsLoading(true);
    try {
      const response = await MarketplaceApiService.updateDataset(session.api_key, editingDataset.id, {
        title: editForm.title,
        description: editForm.description,
        category: editForm.category,
        price_tier: editForm.price_tier,
        tags: editForm.tags.split(',').map(t => t.trim()).filter(t => t)
      });
      if (response.success) {
        alert('Dataset updated successfully!');
        setEditingDataset(null);
        loadMyDatasets();
        onRefresh();
      } else {
        alert(`Update failed: ${response.error}`);
      }
    } catch (error) {
      alert('Update failed');
    } finally {
      setIsLoading(false);
    }
  };

  const handleDelete = async (datasetId: number, datasetTitle: string) => {
    if (!session?.api_key) return;
    if (!confirm(`Are you sure you want to delete "${datasetTitle}"?`)) return;

    setIsLoading(true);
    try {
      const response = await MarketplaceApiService.deleteDataset(session.api_key, datasetId);
      if (response.success) {
        alert('Dataset deleted successfully!');
        loadMyDatasets();
        onRefresh();
      } else {
        alert(`Delete failed: ${response.error}`);
      }
    } catch (error) {
      alert('Delete failed');
    } finally {
      setIsLoading(false);
    }
  };

  const getStatusColor = (status: string) => {
    switch (status) {
      case 'approved': return FINCEPT.GREEN;
      case 'pending': return FINCEPT.YELLOW;
      case 'rejected': return FINCEPT.RED;
      default: return FINCEPT.GRAY;
    }
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Package size={12} color={FINCEPT.ORANGE} />
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
            MY DATASETS ({myDatasets.length})
          </span>
        </div>
      </div>

      <div className="marketplace-scroll" style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>
        {editingDataset && (
          <div style={{
            backgroundColor: FINCEPT.PANEL_BG,
            border: `2px solid ${FINCEPT.ORANGE}`,
            borderRadius: '2px',
            padding: '16px',
            marginBottom: '16px',
            maxWidth: '800px',
            margin: '0 auto 16px'
          }}>
            <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.ORANGE, marginBottom: '16px', letterSpacing: '0.5px' }}>
              EDIT DATASET
            </div>

            <div style={{ marginBottom: '12px' }}>
              <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '6px', letterSpacing: '0.5px' }}>
                TITLE
              </label>
              <input
                type="text"
                value={editForm.title}
                onChange={(e) => setEditForm({ ...editForm, title: e.target.value })}
                style={{
                  width: '100%',
                  padding: '8px 10px',
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '10px',
                  fontFamily: FONT_FAMILY,
                  outline: 'none'
                }}
              />
            </div>

            <div style={{ marginBottom: '12px' }}>
              <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '6px', letterSpacing: '0.5px' }}>
                DESCRIPTION
              </label>
              <textarea
                value={editForm.description}
                onChange={(e) => setEditForm({ ...editForm, description: e.target.value })}
                style={{
                  width: '100%',
                  padding: '8px 10px',
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '10px',
                  fontFamily: FONT_FAMILY,
                  minHeight: '80px',
                  outline: 'none'
                }}
              />
            </div>

            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', marginBottom: '16px' }}>
              <div>
                <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '6px', letterSpacing: '0.5px' }}>
                  CATEGORY
                </label>
                <select
                  value={editForm.category}
                  onChange={(e) => setEditForm({ ...editForm, category: e.target.value })}
                  style={{
                    width: '100%',
                    padding: '8px 10px',
                    backgroundColor: FINCEPT.DARK_BG,
                    color: FINCEPT.WHITE,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderRadius: '2px',
                    fontSize: '10px',
                    fontFamily: FONT_FAMILY
                  }}
                >
                  <option value="stocks">Stocks</option>
                  <option value="forex">Forex</option>
                  <option value="crypto">Cryptocurrency</option>
                  <option value="commodities">Commodities</option>
                  <option value="bonds">Bonds</option>
                  <option value="indices">Indices</option>
                  <option value="economic_data">Economic Data</option>
                </select>
              </div>
              <div>
                <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '6px', letterSpacing: '0.5px' }}>
                  PRICE TIER
                </label>
                <select
                  value={editForm.price_tier}
                  onChange={(e) => setEditForm({ ...editForm, price_tier: e.target.value })}
                  style={{
                    width: '100%',
                    padding: '8px 10px',
                    backgroundColor: FINCEPT.DARK_BG,
                    color: FINCEPT.WHITE,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderRadius: '2px',
                    fontSize: '10px',
                    fontFamily: FONT_FAMILY
                  }}
                >
                  <option value="free">Free</option>
                  <option value="basic">Basic</option>
                  <option value="premium">Premium</option>
                  <option value="enterprise">Enterprise</option>
                </select>
              </div>
            </div>

            <div style={{ marginBottom: '16px' }}>
              <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '6px', letterSpacing: '0.5px' }}>
                TAGS
              </label>
              <input
                type="text"
                value={editForm.tags}
                onChange={(e) => setEditForm({ ...editForm, tags: e.target.value })}
                style={{
                  width: '100%',
                  padding: '8px 10px',
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '10px',
                  fontFamily: FONT_FAMILY,
                  outline: 'none'
                }}
              />
            </div>

            <div style={{ display: 'flex', gap: '8px' }}>
              <button
                onClick={handleUpdate}
                disabled={isLoading}
                style={{
                  padding: '8px 16px',
                  backgroundColor: isLoading ? FINCEPT.MUTED : FINCEPT.GREEN,
                  color: FINCEPT.DARK_BG,
                  border: 'none',
                  borderRadius: '2px',
                  fontSize: '9px',
                  fontWeight: 700,
                  cursor: isLoading ? 'not-allowed' : 'pointer',
                  fontFamily: FONT_FAMILY,
                  letterSpacing: '0.5px'
                }}
              >
                {isLoading ? 'SAVING...' : 'SAVE CHANGES'}
              </button>
              <button
                onClick={() => setEditingDataset(null)}
                style={{
                  padding: '8px 16px',
                  backgroundColor: 'transparent',
                  color: FINCEPT.GRAY,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '9px',
                  fontWeight: 700,
                  cursor: 'pointer',
                  fontFamily: FONT_FAMILY,
                  letterSpacing: '0.5px'
                }}
              >
                CANCEL
              </button>
            </div>
          </div>
        )}

        {isLoading && !editingDataset ? (
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            height: '200px',
            color: FINCEPT.MUTED,
            fontSize: '10px'
          }}>
            Loading datasets...
          </div>
        ) : myDatasets.length === 0 ? (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '200px',
            color: FINCEPT.MUTED,
            fontSize: '10px',
            textAlign: 'center'
          }}>
            <Package size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
            <span>No datasets uploaded yet</span>
          </div>
        ) : (
          <div style={{ maxWidth: '1000px', margin: '0 auto', display: 'flex', flexDirection: 'column', gap: '12px' }}>
            {myDatasets.map((dataset) => {
              const statusColor = getStatusColor(dataset.status || 'pending');
              return (
                <div key={dataset.id} style={{
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderLeft: `4px solid ${statusColor}`,
                  borderRadius: '2px',
                  padding: '16px'
                }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: '12px' }}>
                    <div style={{ flex: 1 }}>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '6px' }}>
                        <div style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>
                          {dataset.title}
                        </div>
                        <div style={{
                          padding: '2px 6px',
                          backgroundColor: `${statusColor}20`,
                          color: statusColor,
                          fontSize: '8px',
                          fontWeight: 700,
                          borderRadius: '2px',
                          letterSpacing: '0.5px'
                        }}>
                          {(dataset.status || 'pending').toUpperCase()}
                        </div>
                      </div>
                      <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '8px' }}>
                        {dataset.category} • {dataset.price_tier} • {dataset.downloads_count} downloads • {new Date(dataset.uploaded_at).toLocaleDateString()}
                      </div>
                      <div style={{ fontSize: '10px', color: FINCEPT.GRAY, lineHeight: '1.5' }}>
                        {dataset.description}
                      </div>
                      {dataset.rejection_reason && (
                        <div style={{
                          marginTop: '12px',
                          padding: '8px',
                          backgroundColor: `${FINCEPT.RED}10`,
                          border: `1px solid ${FINCEPT.RED}`,
                          borderRadius: '2px'
                        }}>
                          <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.RED, marginBottom: '4px', letterSpacing: '0.5px' }}>
                            REJECTION REASON
                          </div>
                          <div style={{ fontSize: '9px', color: FINCEPT.RED }}>
                            {dataset.rejection_reason}
                          </div>
                        </div>
                      )}
                    </div>
                    <div style={{ display: 'flex', gap: '8px', marginLeft: '16px' }}>
                      {(dataset.status === 'pending' || dataset.status === 'rejected') && (
                        <button
                          onClick={() => handleEdit(dataset)}
                          style={{
                            padding: '6px 12px',
                            backgroundColor: 'transparent',
                            border: `1px solid ${FINCEPT.BLUE}`,
                            color: FINCEPT.BLUE,
                            borderRadius: '2px',
                            fontSize: '9px',
                            fontWeight: 700,
                            cursor: 'pointer',
                            fontFamily: FONT_FAMILY,
                            letterSpacing: '0.5px',
                            display: 'flex',
                            alignItems: 'center',
                            gap: '4px'
                          }}
                        >
                          <Edit size={10} />
                          EDIT
                        </button>
                      )}
                      <button
                        onClick={() => handleDelete(dataset.id, dataset.title)}
                        style={{
                          padding: '6px 12px',
                          backgroundColor: 'transparent',
                          border: `1px solid ${FINCEPT.RED}`,
                          color: FINCEPT.RED,
                          borderRadius: '2px',
                          fontSize: '9px',
                          fontWeight: 700,
                          cursor: 'pointer',
                          fontFamily: FONT_FAMILY,
                          letterSpacing: '0.5px',
                          display: 'flex',
                          alignItems: 'center',
                          gap: '4px'
                        }}
                      >
                        <Trash2 size={10} />
                        DELETE
                      </button>
                    </div>
                  </div>
                  {dataset.tags && dataset.tags.length > 0 && (
                    <div style={{ display: 'flex', gap: '6px', flexWrap: 'wrap' }}>
                      {dataset.tags.map((tag, idx) => (
                        <span key={idx} style={{
                          padding: '2px 6px',
                          backgroundColor: `${FINCEPT.PURPLE}20`,
                          color: FINCEPT.PURPLE,
                          fontSize: '8px',
                          fontWeight: 700,
                          borderRadius: '2px'
                        }}>
                          #{tag}
                        </span>
                      ))}
                    </div>
                  )}
                </div>
              );
            })}
          </div>
        )}
      </div>
    </div>
  );
};

// Analytics Screen Component
const AnalyticsScreen: React.FC<{ session: any }> = ({ session }) => {
  const [analytics, setAnalytics] = useState<any>(null);
  const [isLoading, setIsLoading] = useState(true);

  useEffect(() => {
    const fetchAnalytics = async () => {
      if (!session?.api_key) return;
      setIsLoading(true);
      try {
        const response = await MarketplaceApiService.getDatasetAnalytics(session.api_key);
        if (response.success && response.data) {
          setAnalytics(response.data);
        }
      } catch (error) {
        console.error('Failed to fetch analytics:', error);
      } finally {
        setIsLoading(false);
      }
    };
    fetchAnalytics();
  }, [session?.api_key]);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <BarChart3 size={12} color={FINCEPT.ORANGE} />
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
            DATASET ANALYTICS
          </span>
        </div>
      </div>

      <div className="marketplace-scroll" style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>
        {isLoading ? (
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            height: '200px',
            color: FINCEPT.MUTED,
            fontSize: '10px'
          }}>
            Loading analytics...
          </div>
        ) : !analytics ? (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '200px',
            color: FINCEPT.MUTED,
            fontSize: '10px',
            textAlign: 'center'
          }}>
            <BarChart3 size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
            <span>No analytics data available</span>
          </div>
        ) : (
          <div style={{ maxWidth: '1200px', margin: '0 auto' }}>
            {/* Summary Stats */}
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '12px', marginBottom: '24px' }}>
              <div style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `2px solid ${FINCEPT.BLUE}`,
                borderRadius: '2px',
                padding: '16px',
                textAlign: 'center'
              }}>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '6px', letterSpacing: '0.5px' }}>
                  TOTAL DATASETS
                </div>
                <div style={{ fontSize: '24px', fontWeight: 700, color: FINCEPT.BLUE }}>
                  {analytics.total_datasets || 0}
                </div>
              </div>
              <div style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `2px solid ${FINCEPT.GREEN}`,
                borderRadius: '2px',
                padding: '16px',
                textAlign: 'center'
              }}>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '6px', letterSpacing: '0.5px' }}>
                  TOTAL DOWNLOADS
                </div>
                <div style={{ fontSize: '24px', fontWeight: 700, color: FINCEPT.GREEN }}>
                  {analytics.total_downloads || 0}
                </div>
              </div>
              <div style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `2px solid ${FINCEPT.ORANGE}`,
                borderRadius: '2px',
                padding: '16px',
                textAlign: 'center'
              }}>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '6px', letterSpacing: '0.5px' }}>
                  TOTAL REVENUE
                </div>
                <div style={{ fontSize: '24px', fontWeight: 700, color: FINCEPT.ORANGE }}>
                  {analytics.total_revenue || 0}
                </div>
              </div>
            </div>

            {/* Dataset Performance */}
            {analytics.datasets && analytics.datasets.length > 0 && (
              <div style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                padding: '16px'
              }}>
                <div style={{
                  fontSize: '10px',
                  fontWeight: 700,
                  color: FINCEPT.ORANGE,
                  marginBottom: '16px',
                  letterSpacing: '0.5px'
                }}>
                  DATASET PERFORMANCE
                </div>
                {analytics.datasets.map((dataset: any) => (
                  <div key={dataset.id} style={{
                    borderBottom: `1px solid ${FINCEPT.BORDER}`,
                    padding: '12px 0'
                  }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                      <div style={{ fontSize: '10px', color: FINCEPT.WHITE, flex: 1 }}>
                        {dataset.title}
                      </div>
                      <div style={{ display: 'flex', gap: '24px' }}>
                        <div style={{ textAlign: 'center' }}>
                          <div style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.CYAN }}>
                            {dataset.downloads}
                          </div>
                          <div style={{ fontSize: '8px', color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
                            DOWNLOADS
                          </div>
                        </div>
                        <div style={{ textAlign: 'center' }}>
                          <div style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.GREEN }}>
                            {dataset.revenue}
                          </div>
                          <div style={{ fontSize: '8px', color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
                            REVENUE
                          </div>
                        </div>
                        <div style={{ textAlign: 'center' }}>
                          <div style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.YELLOW }}>
                            ★ {dataset.rating.toFixed(1)}
                          </div>
                          <div style={{ fontSize: '8px', color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
                            RATING
                          </div>
                        </div>
                      </div>
                    </div>
                  </div>
                ))}
              </div>
            )}
          </div>
        )}
      </div>
    </div>
  );
};

// Pricing Screen Component
const PricingScreen: React.FC<{ session: any }> = ({ session }) => {
  const [pricingTiers, setPricingTiers] = useState<any[]>([]);
  const [isLoading, setIsLoading] = useState(true);

  useEffect(() => {
    const fetchPricing = async () => {
      if (!session?.api_key) return;
      setIsLoading(true);
      try {
        const response = await MarketplaceApiService.getPricingTiers(session.api_key);
        if (response.success && response.data) {
          setPricingTiers(response.data.tiers || []);
        }
      } catch (error) {
        console.error('Failed to fetch pricing tiers:', error);
      } finally {
        setIsLoading(false);
      }
    };
    fetchPricing();
  }, [session?.api_key]);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <DollarSign size={12} color={FINCEPT.ORANGE} />
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
            PRICING TIERS
          </span>
        </div>
      </div>

      <div className="marketplace-scroll" style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>
        {isLoading ? (
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            height: '200px',
            color: FINCEPT.MUTED,
            fontSize: '10px'
          }}>
            Loading pricing...
          </div>
        ) : pricingTiers.length === 0 ? (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '200px',
            color: FINCEPT.MUTED,
            fontSize: '10px',
            textAlign: 'center'
          }}>
            <DollarSign size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
            <span>No pricing information available</span>
          </div>
        ) : (
          <div style={{
            maxWidth: '1200px',
            margin: '0 auto',
            display: 'grid',
            gridTemplateColumns: 'repeat(auto-fit, minmax(280px, 1fr))',
            gap: '16px'
          }}>
            {pricingTiers.map((tier, idx) => (
              <div key={idx} style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `2px solid ${FINCEPT.ORANGE}`,
                borderRadius: '2px',
                padding: '20px',
                textAlign: 'center'
              }}>
                <div style={{
                  fontSize: '12px',
                  fontWeight: 700,
                  color: FINCEPT.ORANGE,
                  marginBottom: '12px',
                  letterSpacing: '0.5px'
                }}>
                  {tier.tier.toUpperCase()}
                </div>
                <div style={{
                  fontSize: '32px',
                  fontWeight: 700,
                  color: FINCEPT.GREEN,
                  marginBottom: '4px'
                }}>
                  {tier.price_credits}
                </div>
                <div style={{
                  fontSize: '9px',
                  color: FINCEPT.GRAY,
                  marginBottom: '16px',
                  letterSpacing: '0.5px'
                }}>
                  CREDITS
                </div>
                <div style={{
                  fontSize: '10px',
                  color: FINCEPT.WHITE,
                  lineHeight: '1.6',
                  marginBottom: '20px'
                }}>
                  {tier.description}
                </div>
                {tier.features && tier.features.length > 0 && (
                  <div style={{
                    textAlign: 'left',
                    borderTop: `1px solid ${FINCEPT.BORDER}`,
                    paddingTop: '16px'
                  }}>
                    <div style={{
                      fontSize: '9px',
                      fontWeight: 700,
                      color: FINCEPT.CYAN,
                      marginBottom: '12px',
                      letterSpacing: '0.5px'
                    }}>
                      FEATURES
                    </div>
                    {tier.features.map((feature: string, fidx: number) => (
                      <div key={fidx} style={{
                        fontSize: '9px',
                        color: FINCEPT.WHITE,
                        marginBottom: '6px',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '8px'
                      }}>
                        <Check size={10} color={FINCEPT.GREEN} />
                        <span>{feature}</span>
                      </div>
                    ))}
                  </div>
                )}
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
};

// Admin Stats Screen
const AdminStatsScreen: React.FC<{ session: any }> = ({ session }) => {
  const [stats, setStats] = useState<any>(null);
  const [isLoading, setIsLoading] = useState(true);

  useEffect(() => {
    const fetchStats = async () => {
      if (!session?.api_key) return;
      setIsLoading(true);
      try {
        const response = await MarketplaceApiService.getMarketplaceStats(session.api_key);
        if (response.success && response.data) {
          setStats(response.data);
        }
      } catch (error) {
        console.error('Failed to fetch stats:', error);
      } finally {
        setIsLoading(false);
      }
    };
    fetchStats();
  }, [session?.api_key]);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Shield size={12} color={FINCEPT.ORANGE} />
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
            MARKETPLACE STATISTICS
          </span>
        </div>
      </div>

      <div className="marketplace-scroll" style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>
        {isLoading ? (
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            height: '200px',
            color: FINCEPT.MUTED,
            fontSize: '10px'
          }}>
            Loading statistics...
          </div>
        ) : !stats ? (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '200px',
            color: FINCEPT.MUTED,
            fontSize: '10px',
            textAlign: 'center'
          }}>
            <BarChart3 size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
            <span>No statistics available</span>
          </div>
        ) : (
          <div style={{ maxWidth: '1200px', margin: '0 auto' }}>
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '12px', marginBottom: '16px' }}>
              <StatCard label="TOTAL DATASETS" value={stats.total_datasets || 0} color={FINCEPT.BLUE} />
              <StatCard label="TOTAL PURCHASES" value={stats.total_purchases || 0} color={FINCEPT.GREEN} />
              <StatCard label="TOTAL REVENUE" value={stats.total_revenue || 0} color={FINCEPT.ORANGE} />
            </div>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', marginBottom: '24px' }}>
              <StatCard label="TOTAL UPLOADS" value={stats.total_uploads || 0} color={FINCEPT.CYAN} />
              <StatCard label="ACTIVE USERS" value={stats.active_users || 0} color={FINCEPT.PURPLE} />
            </div>

            {stats.popular_categories && Object.keys(stats.popular_categories).length > 0 && (
              <div style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                padding: '16px'
              }}>
                <div style={{
                  fontSize: '10px',
                  fontWeight: 700,
                  color: FINCEPT.ORANGE,
                  marginBottom: '16px',
                  letterSpacing: '0.5px'
                }}>
                  POPULAR CATEGORIES
                </div>
                {Object.entries(stats.popular_categories).map(([category, count]: [string, any]) => (
                  <div key={category} style={{
                    borderBottom: `1px solid ${FINCEPT.BORDER}`,
                    padding: '12px 0',
                    display: 'flex',
                    justifyContent: 'space-between'
                  }}>
                    <div style={{ fontSize: '10px', color: FINCEPT.WHITE }}>
                      {category.toUpperCase()}
                    </div>
                    <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.CYAN }}>
                      {count} datasets
                    </div>
                  </div>
                ))}
              </div>
            )}
          </div>
        )}
      </div>
    </div>
  );
};

// Admin Pending Screen
const AdminPendingScreen: React.FC<{ session: any; onRefresh: () => void }> = ({ session, onRefresh }) => {
  const [pendingDatasets, setPendingDatasets] = useState<Dataset[]>([]);
  const [isLoading, setIsLoading] = useState(true);
  const [rejectingId, setRejectingId] = useState<number | null>(null);
  const [rejectionReason, setRejectionReason] = useState('');

  const loadPending = async () => {
    if (!session?.api_key) return;
    setIsLoading(true);
    try {
      const response = await MarketplaceApiService.getPendingDatasets(session.api_key);
      if (response.success && response.data) {
        setPendingDatasets(response.data.datasets || []);
      }
    } catch (error) {
      console.error('Failed to fetch pending datasets:', error);
    } finally {
      setIsLoading(false);
    }
  };

  useEffect(() => {
    loadPending();
  }, [session?.api_key]);

  const handleApprove = async (datasetId: number, title: string) => {
    if (!confirm(`Approve dataset "${title}"?`)) return;
    setIsLoading(true);
    try {
      const response = await MarketplaceApiService.approveDataset(session.api_key, datasetId);
      if (response.success) {
        alert('Dataset approved!');
        loadPending();
        onRefresh();
      } else {
        alert('Approval failed');
      }
    } catch (error) {
      alert('Approval failed');
    } finally {
      setIsLoading(false);
    }
  };

  const handleReject = async (datasetId: number) => {
    if (!rejectionReason.trim()) {
      alert('Please provide a rejection reason');
      return;
    }
    setIsLoading(true);
    try {
      const response = await MarketplaceApiService.rejectDataset(session.api_key, datasetId, rejectionReason);
      if (response.success) {
        alert('Dataset rejected!');
        setRejectingId(null);
        setRejectionReason('');
        loadPending();
        onRefresh();
      } else {
        alert('Rejection failed');
      }
    } catch (error) {
      alert('Rejection failed');
    } finally {
      setIsLoading(false);
    }
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Clock size={12} color={FINCEPT.ORANGE} />
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
            PENDING APPROVALS ({pendingDatasets.length})
          </span>
        </div>
      </div>

      <div className="marketplace-scroll" style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>
        {isLoading ? (
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            height: '200px',
            color: FINCEPT.MUTED,
            fontSize: '10px'
          }}>
            Loading pending datasets...
          </div>
        ) : pendingDatasets.length === 0 ? (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '200px',
            color: FINCEPT.MUTED,
            fontSize: '10px',
            textAlign: 'center'
          }}>
            <Clock size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
            <span>No pending datasets</span>
          </div>
        ) : (
          <div style={{ maxWidth: '1000px', margin: '0 auto', display: 'flex', flexDirection: 'column', gap: '12px' }}>
            {pendingDatasets.map((dataset) => (
              <div key={dataset.id} style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `2px solid ${FINCEPT.YELLOW}`,
                borderRadius: '2px',
                padding: '16px'
              }}>
                <div style={{ marginBottom: '12px' }}>
                  <div style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE, marginBottom: '6px' }}>
                    {dataset.title}
                  </div>
                  <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '8px' }}>
                    by {dataset.uploader_username} • {dataset.category} • {dataset.price_tier} • {(dataset.file_size / 1024 / 1024).toFixed(2)} MB
                  </div>
                  <div style={{ fontSize: '10px', color: FINCEPT.WHITE, lineHeight: '1.5', marginBottom: '12px' }}>
                    {dataset.description}
                  </div>
                  {dataset.tags && dataset.tags.length > 0 && (
                    <div style={{ display: 'flex', gap: '6px', flexWrap: 'wrap' }}>
                      {dataset.tags.map((tag, idx) => (
                        <span key={idx} style={{
                          padding: '2px 6px',
                          backgroundColor: `${FINCEPT.CYAN}20`,
                          color: FINCEPT.CYAN,
                          fontSize: '8px',
                          fontWeight: 700,
                          borderRadius: '2px'
                        }}>
                          #{tag}
                        </span>
                      ))}
                    </div>
                  )}
                </div>

                {rejectingId === dataset.id ? (
                  <div style={{
                    backgroundColor: `${FINCEPT.RED}10`,
                    border: `1px solid ${FINCEPT.RED}`,
                    borderRadius: '2px',
                    padding: '12px',
                    marginTop: '12px'
                  }}>
                    <div style={{
                      fontSize: '9px',
                      fontWeight: 700,
                      color: FINCEPT.RED,
                      marginBottom: '8px',
                      letterSpacing: '0.5px'
                    }}>
                      REJECTION REASON
                    </div>
                    <textarea
                      value={rejectionReason}
                      onChange={(e) => setRejectionReason(e.target.value)}
                      placeholder="Enter reason for rejection..."
                      style={{
                        width: '100%',
                        padding: '8px',
                        backgroundColor: FINCEPT.DARK_BG,
                        color: FINCEPT.WHITE,
                        border: `1px solid ${FINCEPT.BORDER}`,
                        borderRadius: '2px',
                        fontSize: '10px',
                        fontFamily: FONT_FAMILY,
                        minHeight: '60px',
                        marginBottom: '8px',
                        outline: 'none'
                      }}
                    />
                    <div style={{ display: 'flex', gap: '8px' }}>
                      <button
                        onClick={() => handleReject(dataset.id)}
                        disabled={!rejectionReason.trim()}
                        style={{
                          padding: '6px 12px',
                          backgroundColor: FINCEPT.RED,
                          color: FINCEPT.WHITE,
                          border: 'none',
                          borderRadius: '2px',
                          fontSize: '9px',
                          fontWeight: 700,
                          cursor: rejectionReason.trim() ? 'pointer' : 'not-allowed',
                          fontFamily: FONT_FAMILY,
                          letterSpacing: '0.5px',
                          opacity: rejectionReason.trim() ? 1 : 0.5
                        }}
                      >
                        CONFIRM REJECT
                      </button>
                      <button
                        onClick={() => { setRejectingId(null); setRejectionReason(''); }}
                        style={{
                          padding: '6px 12px',
                          backgroundColor: 'transparent',
                          color: FINCEPT.GRAY,
                          border: `1px solid ${FINCEPT.BORDER}`,
                          borderRadius: '2px',
                          fontSize: '9px',
                          fontWeight: 700,
                          cursor: 'pointer',
                          fontFamily: FONT_FAMILY,
                          letterSpacing: '0.5px'
                        }}
                      >
                        CANCEL
                      </button>
                    </div>
                  </div>
                ) : (
                  <div style={{ display: 'flex', gap: '8px' }}>
                    <button
                      onClick={() => handleApprove(dataset.id, dataset.title)}
                      style={{
                        padding: '8px 16px',
                        backgroundColor: 'transparent',
                        border: `1px solid ${FINCEPT.GREEN}`,
                        color: FINCEPT.GREEN,
                        borderRadius: '2px',
                        fontSize: '9px',
                        fontWeight: 700,
                        cursor: 'pointer',
                        fontFamily: FONT_FAMILY,
                        letterSpacing: '0.5px',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '4px'
                      }}
                    >
                      <Check size={10} />
                      APPROVE
                    </button>
                    <button
                      onClick={() => setRejectingId(dataset.id)}
                      style={{
                        padding: '8px 16px',
                        backgroundColor: 'transparent',
                        border: `1px solid ${FINCEPT.RED}`,
                        color: FINCEPT.RED,
                        borderRadius: '2px',
                        fontSize: '9px',
                        fontWeight: 700,
                        cursor: 'pointer',
                        fontFamily: FONT_FAMILY,
                        letterSpacing: '0.5px',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '4px'
                      }}
                    >
                      <X size={10} />
                      REJECT
                    </button>
                  </div>
                )}
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
};

// Admin Revenue Screen
const AdminRevenueScreen: React.FC<{ session: any }> = ({ session }) => {
  const [revenue, setRevenue] = useState<any>(null);
  const [isLoading, setIsLoading] = useState(true);

  useEffect(() => {
    const fetchRevenue = async () => {
      if (!session?.api_key) return;
      setIsLoading(true);
      try {
        const response = await MarketplaceApiService.getAdminRevenueAnalytics(session.api_key);
        if (response.success && response.data) {
          setRevenue(response.data);
        }
      } catch (error) {
        console.error('Failed to fetch revenue:', error);
      } finally {
        setIsLoading(false);
      }
    };
    fetchRevenue();
  }, [session?.api_key]);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <TrendingUp size={12} color={FINCEPT.ORANGE} />
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
            REVENUE ANALYTICS
          </span>
        </div>
      </div>

      <div className="marketplace-scroll" style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>
        {isLoading ? (
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            height: '200px',
            color: FINCEPT.MUTED,
            fontSize: '10px'
          }}>
            Loading revenue data...
          </div>
        ) : !revenue ? (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '200px',
            color: FINCEPT.MUTED,
            fontSize: '10px',
            textAlign: 'center'
          }}>
            <TrendingUp size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
            <span>No revenue data available</span>
          </div>
        ) : (
          <div style={{ maxWidth: '1200px', margin: '0 auto' }}>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', marginBottom: '24px' }}>
              <div style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `2px solid ${FINCEPT.GREEN}`,
                borderRadius: '2px',
                padding: '24px',
                textAlign: 'center'
              }}>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '8px', letterSpacing: '0.5px' }}>
                  TOTAL REVENUE
                </div>
                <div style={{ fontSize: '36px', fontWeight: 700, color: FINCEPT.GREEN }}>
                  {revenue.total_revenue || 0}
                </div>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginTop: '4px', letterSpacing: '0.5px' }}>
                  CREDITS
                </div>
              </div>
              <div style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `2px solid ${FINCEPT.BLUE}`,
                borderRadius: '2px',
                padding: '24px',
                textAlign: 'center'
              }}>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '8px', letterSpacing: '0.5px' }}>
                  TOTAL TRANSACTIONS
                </div>
                <div style={{ fontSize: '36px', fontWeight: 700, color: FINCEPT.BLUE }}>
                  {revenue.total_transactions || 0}
                </div>
              </div>
            </div>

            {revenue.revenue_by_tier && Object.keys(revenue.revenue_by_tier).length > 0 && (
              <div style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                padding: '16px',
                marginBottom: '24px'
              }}>
                <div style={{
                  fontSize: '10px',
                  fontWeight: 700,
                  color: FINCEPT.ORANGE,
                  marginBottom: '16px',
                  letterSpacing: '0.5px'
                }}>
                  REVENUE BY TIER
                </div>
                {Object.entries(revenue.revenue_by_tier).map(([tier, amount]: [string, any]) => (
                  <div key={tier} style={{
                    borderBottom: `1px solid ${FINCEPT.BORDER}`,
                    padding: '12px 0',
                    display: 'flex',
                    justifyContent: 'space-between'
                  }}>
                    <div style={{ fontSize: '10px', color: FINCEPT.WHITE }}>
                      {tier.toUpperCase()}
                    </div>
                    <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.GREEN }}>
                      {amount} credits
                    </div>
                  </div>
                ))}
              </div>
            )}

            {revenue.top_datasets && revenue.top_datasets.length > 0 && (
              <div style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                padding: '16px'
              }}>
                <div style={{
                  fontSize: '10px',
                  fontWeight: 700,
                  color: FINCEPT.ORANGE,
                  marginBottom: '16px',
                  letterSpacing: '0.5px'
                }}>
                  TOP PERFORMING DATASETS
                </div>
                {revenue.top_datasets.map((dataset: any, idx: number) => (
                  <div key={dataset.id} style={{
                    borderBottom: `1px solid ${FINCEPT.BORDER}`,
                    padding: '12px 0',
                    display: 'flex',
                    justifyContent: 'space-between',
                    alignItems: 'center'
                  }}>
                    <div style={{ flex: 1 }}>
                      <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.WHITE, marginBottom: '4px' }}>
                        #{idx + 1} {dataset.title}
                      </div>
                      <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                        {dataset.downloads} downloads
                      </div>
                    </div>
                    <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.GREEN, marginLeft: '16px' }}>
                      {dataset.revenue} credits
                    </div>
                  </div>
                ))}
              </div>
            )}
          </div>
        )}
      </div>
    </div>
  );
};

// Stat Card Component
const StatCard: React.FC<{ label: string; value: number; color: string }> = ({ label, value, color }) => (
  <div style={{
    backgroundColor: FINCEPT.PANEL_BG,
    border: `2px solid ${color}`,
    borderRadius: '2px',
    padding: '16px',
    textAlign: 'center'
  }}>
    <div style={{
      fontSize: '9px',
      color: FINCEPT.GRAY,
      marginBottom: '6px',
      letterSpacing: '0.5px'
    }}>
      {label}
    </div>
    <div style={{
      fontSize: '28px',
      fontWeight: 700,
      color
    }}>
      {value.toLocaleString()}
    </div>
  </div>
);

export default MarketplaceTab;
