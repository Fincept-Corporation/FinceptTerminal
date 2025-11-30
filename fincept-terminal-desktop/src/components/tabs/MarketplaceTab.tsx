import React, { useState, useEffect } from 'react';
import { useAuth } from '@/contexts/AuthContext';
import { MarketplaceApiService, Dataset } from '@/services/marketplaceApi';
import { useTerminalTheme } from '@/contexts/ThemeContext';

interface PricingTier {
  tier: string;
  price_credits: number;
  description: string;
  features?: string[];
}

interface RevenueAnalytics {
  total_revenue: number;
  total_transactions: number;
  revenue_by_tier?: Record<string, number>;
  top_datasets?: Array<{
    id: number;
    title: string;
    downloads: number;
    revenue: number;
  }>;
}

interface MarketProduct {
  id: string;
  type: 'DATA' | 'ALGO' | 'API' | 'MODEL' | 'SIGNAL' | 'PLUGIN';
  name: string;
  vendor: string;
  category: string;
  price: number;
  pricingModel: 'ONE_TIME' | 'MONTHLY' | 'ANNUAL' | 'USAGE_BASED' | 'FREE';
  shortDescription: string;
  rating: number;
  reviews: number;
  users: number;
  accuracy: string;
  latency: string;
  verified: boolean;
  trending: boolean;
  featured: boolean;
}

const MarketplaceTab: React.FC = () => {
  const { colors, fontSize, fontFamily, fontWeight, fontStyle } = useTerminalTheme();
  const C = {
    ORANGE: colors.primary,
    WHITE: colors.text,
    RED: colors.alert,
    GREEN: colors.secondary,
    YELLOW: colors.warning,
    GRAY: colors.textMuted,
    BLUE: colors.info,
    PURPLE: colors.purple,
    CYAN: colors.accent,
    DARK_BG: colors.background,
    PANEL_BG: colors.panel
  };
  const { session } = useAuth();
  const [currentScreen, setCurrentScreen] = useState<'BROWSE' | 'DETAILS' | 'CART' | 'UPLOAD' | 'MY_PURCHASES' | 'MY_DATASETS' | 'ANALYTICS' | 'ADMIN_STATS' | 'ADMIN_PENDING' | 'ADMIN_REVENUE' | 'PRICING'>('BROWSE');
  const [selectedProduct, setSelectedProduct] = useState<MarketProduct | null>(null);
  const [selectedCategory, setSelectedCategory] = useState('ALL');
  const [searchQuery, setSearchQuery] = useState('');
  const [showAllDatasets, setShowAllDatasets] = useState(false); // Toggle for showing all datasets including pending
  const [cartItems, setCartItems] = useState<string[]>([]);
  const [datasets, setDatasets] = useState<Dataset[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [currentPage, setCurrentPage] = useState(1);
  const [totalPages, setTotalPages] = useState(1);
  const [myUploadedDatasets, setMyUploadedDatasets] = useState<any[]>([]);

  // Upload state
  const [uploadFile, setUploadFile] = useState<File | null>(null);
  const [uploadMetadata, setUploadMetadata] = useState({
    title: '',
    description: '',
    category: 'stocks',
    price_tier: 'free',
    tags: ''
  });

  // Fetch datasets from API
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

      console.log('📊 Browse datasets response:', response);

      if (response.success && response.data) {
        // API returns: { success: true, data: { datasets: [...], pagination: { current_page, total_pages, ... } } }
        const responseData = response.data as any;
        const rawDatasets = responseData.data?.datasets || responseData.datasets || [];
        const pagination = responseData.data?.pagination || responseData.pagination || {};
        const totalPagesCount = pagination.total_pages || responseData.total_pages || 1;

        console.log('📦 Raw datasets from API:', rawDatasets);

        // Transform API response to match our Dataset interface
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
          file_size: (apiDataset.metadata?.file_size_mb || 0) * 1024 * 1024, // Convert MB to bytes
          downloads_count: apiDataset.statistics?.download_count || 0,
          rating: apiDataset.statistics?.rating || 0,
          tags: apiDataset.tags || [],
          status: apiDataset.status || 'approved', // Assume approved if not specified
          rejection_reason: apiDataset.rejection_reason,
          uploaded_at: apiDataset.uploaded_at,
          updated_at: apiDataset.updated_at || apiDataset.uploaded_at
        }));

        console.log('📦 Transformed datasets:', transformedDatasets.length, transformedDatasets);

        setDatasets(transformedDatasets);
        setTotalPages(totalPagesCount);
      } else {
        console.error('❌ Failed to fetch datasets:', response.error);
        setDatasets([]);
      }
    } catch (error) {
      console.error('💥 Failed to fetch datasets:', error);
      setDatasets([]);
    } finally {
      setIsLoading(false);
    }
  };

  // Fetch MY uploaded datasets
  const fetchMyDatasets = async () => {
    if (!session?.api_key) return;

    try {
      const response = await MarketplaceApiService.getMyDatasets(session.api_key, 1, 100);
      console.log('📊 My uploaded datasets response:', response);

      if (response.success && response.data) {
        // API returns: { success: true, data: { datasets: [...], total: X, page: Y, total_pages: Z } }
        const responseData = response.data as any;
        const myDatasets = responseData.datasets || [];
        console.log('📦 My uploaded datasets:', myDatasets);
        setMyUploadedDatasets(myDatasets);
      }
    } catch (error) {
      console.error('Failed to fetch my datasets:', error);
    }
  };

  useEffect(() => {
    fetchDatasets();
    fetchMyDatasets();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [session?.api_key, selectedCategory, searchQuery, currentPage]);

  // Convert Dataset to MarketProduct format for UI compatibility
  const convertToMarketProduct = (dataset: Dataset): MarketProduct => {
    const typeMap: Record<string, MarketProduct['type']> = {
      'stocks': 'DATA',
      'forex': 'DATA',
      'crypto': 'DATA',
      'commodities': 'DATA',
      'bonds': 'DATA',
      'indices': 'DATA',
      'economic_data': 'SIGNAL'
    };

    const pricingMap: Record<string, MarketProduct['pricingModel']> = {
      'free': 'FREE',
      'basic': 'MONTHLY',
      'premium': 'ANNUAL',
      'enterprise': 'USAGE_BASED'
    };

    return {
      id: dataset.id.toString(),
      type: typeMap[dataset.category] || 'DATA',
      name: dataset.title,
      vendor: dataset.uploader_username,
      category: dataset.category,
      price: dataset.price_credits,
      pricingModel: pricingMap[dataset.price_tier] || 'FREE',
      shortDescription: dataset.description,
      rating: dataset.rating || 0,
      reviews: dataset.downloads_count || 0,
      users: dataset.downloads_count || 0,
      accuracy: 'N/A',
      latency: 'N/A',
      verified: true,
      trending: dataset.downloads_count > 100,
      featured: dataset.rating > 4.5
    };
  };

  // Merge datasets with myUploadedDatasets if showAllDatasets is enabled
  const allDatasets = showAllDatasets
    ? [...datasets, ...myUploadedDatasets.filter(myDataset => !datasets.some(d => d.id === myDataset.id))]
    : datasets;

  const products: MarketProduct[] = allDatasets.map(convertToMarketProduct);

  // Get unique categories from datasets
  const categoryCounts = datasets.reduce((acc, dataset) => {
    acc[dataset.category] = (acc[dataset.category] || 0) + 1;
    return acc;
  }, {} as Record<string, number>);

  const categories = [
    { name: 'ALL', count: datasets.length },
    ...Object.entries(categoryCounts).map(([name, count]) => ({ name, count }))
  ];

  const filteredProducts = products.filter(p =>
    (selectedCategory === 'ALL' || p.category === selectedCategory) &&
    (searchQuery === '' || p.name.toLowerCase().includes(searchQuery.toLowerCase()) || p.vendor.toLowerCase().includes(searchQuery.toLowerCase()))
  );

  const getTypeColor = (type: string) => {
    const colors: Record<string, string> = {
      DATA: C.BLUE, ALGO: C.GREEN, API: C.PURPLE, MODEL: C.CYAN, SIGNAL: C.YELLOW, PLUGIN: C.ORANGE
    };
    return colors[type] || C.WHITE;
  };

  // Handle purchase
  const handlePurchase = async (datasetId: string) => {
    if (!session?.api_key) return;

    try {
      const response = await MarketplaceApiService.purchaseDataset(
        session.api_key,
        parseInt(datasetId)
      );

      if (response.success) {
        alert('Purchase successful! Dataset access granted.');
        // Remove from cart after purchase
        setCartItems(cartItems.filter(id => id !== datasetId));
      } else {
        alert(`Purchase failed: ${response.error || 'Unknown error'}`);
      }
    } catch (error) {
      console.error('Purchase error:', error);
      alert('Purchase failed. Please try again.');
    }
  };

  // Handle download
  const handleDownload = async (datasetId: string) => {
    if (!session?.api_key) return;

    try {
      const response = await MarketplaceApiService.downloadDataset(
        session.api_key,
        parseInt(datasetId)
      );

      if (response.success && response.data?.download_url) {
        window.open(response.data.download_url, '_blank');
      } else {
        alert(`Download failed: ${response.error || 'Unknown error'}`);
      }
    } catch (error) {
      console.error('Download error:', error);
      alert('Download failed. Please try again.');
    }
  };

  // Handle upload
  const handleUpload = async () => {
    if (!session?.api_key || !uploadFile) {
      alert('Please select a file and fill in all details');
      return;
    }

    setIsLoading(true);
    try {
      const response = await MarketplaceApiService.uploadDataset(
        session.api_key,
        uploadFile,
        {
          title: uploadMetadata.title,
          description: uploadMetadata.description,
          category: uploadMetadata.category,
          price_tier: uploadMetadata.price_tier,
          tags: uploadMetadata.tags ? uploadMetadata.tags.split(',').map(t => t.trim()) : []
        }
      );

      console.log('📤 Upload response:', response);
      console.log('📤 Upload response.data:', response.data);
      console.log('📤 Upload response full:', JSON.stringify(response, null, 2));

      if (response.success) {
        const uploadedDataset = (response.data as any)?.data || response.data;
        console.log('✅ Dataset uploaded successfully:', uploadedDataset);
        console.log('✅ Dataset ID:', (uploadedDataset as any)?.id);
        console.log('✅ Dataset full object:', JSON.stringify(uploadedDataset, null, 2));

        alert(`Dataset uploaded successfully! ID: ${(uploadedDataset as any)?.id || 'Unknown'}\nTitle: ${(uploadedDataset as any)?.title || 'Unknown'}`);

        setUploadFile(null);
        setUploadMetadata({
          title: '',
          description: '',
          category: 'stocks',
          price_tier: 'free',
          tags: ''
        });

        // Immediately try to fetch the specific dataset
        if ((uploadedDataset as any)?.id && session?.api_key) {
          console.log(`🔍 Attempting to fetch dataset ${(uploadedDataset as any).id} directly...`);
          MarketplaceApiService.getDatasetDetails(session.api_key, (uploadedDataset as any).id)
            .then(detailsResponse => {
              console.log('📋 Dataset details response:', detailsResponse);
            });
        }

        // Wait a bit for backend to process, then refresh and switch to browse
        setTimeout(() => {
          console.log('🔄 Refreshing datasets list...');
          fetchDatasets();
          fetchMyDatasets();
          setCurrentScreen('BROWSE');
        }, 1500);
      } else {
        console.error('❌ Upload failed:', response.error);
        alert(`Upload failed: ${response.error || 'Unknown error'}`);
      }
    } catch (error) {
      console.error('Upload error:', error);
      alert('Upload failed. Please try again.');
    } finally {
      setIsLoading(false);
    }
  };

  // Check if user is a guest
  const isGuestUser = session?.user_type === 'guest' || session?.api_key?.startsWith('fk_guest_');

  return (
    <div style={{ width: '100%', height: '100%', backgroundColor: C.DARK_BG, color: C.WHITE, fontFamily: 'Consolas, monospace', display: 'flex', flexDirection: 'column' }}>
      <style>{`
        .marketplace-scroll::-webkit-scrollbar { width: 8px; }
        .marketplace-scroll::-webkit-scrollbar-track { background: #1a1a1a; }
        .marketplace-scroll::-webkit-scrollbar-thumb { background: #404040; border-radius: 4px; }
        .marketplace-scroll::-webkit-scrollbar-thumb:hover { background: #525252; }
      `}</style>


      {/* Header */}
      <div style={{ backgroundColor: C.PANEL_BG, borderBottom: `2px solid ${C.GRAY}`, padding: '8px 16px' }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', flexWrap: 'wrap', gap: '8px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <span style={{ color: C.ORANGE, fontWeight: 'bold', fontSize: '16px' }}>FINCEPT MARKETPLACE</span>
            <span style={{ color: C.GRAY }}>|</span>
            <span style={{ color: C.CYAN, fontSize: '12px' }}>{datasets.length} Datasets Available</span>
            {currentScreen === 'BROWSE' && (
              <>
                <span style={{ color: C.GRAY }}>|</span>
                <span style={{ color: C.YELLOW, fontSize: '12px' }}>My Uploads: {myUploadedDatasets.length}</span>
                <span style={{ color: C.GRAY }}>|</span>
                <button
                  onClick={() => {
                    fetchDatasets();
                    fetchMyDatasets();
                  }}
                  disabled={isLoading}
                  style={{ padding: '4px 8px', backgroundColor: C.DARK_BG, border: `2px solid ${C.BLUE}`, color: C.BLUE, fontSize: '10px', fontWeight: 'bold', cursor: isLoading ? 'not-allowed' : 'pointer', fontFamily: 'Consolas, monospace' }}
                >
                  {isLoading ? '⟳ LOADING...' : '🔄 REFRESH'}
                </button>
              </>
            )}
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px', flexWrap: 'wrap' }}>
            <button onClick={() => setCurrentScreen('BROWSE')} style={{ padding: '6px 12px', backgroundColor: currentScreen === 'BROWSE' ? C.ORANGE : C.DARK_BG, border: `2px solid ${C.GRAY}`, color: currentScreen === 'BROWSE' ? C.DARK_BG : C.WHITE, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
              BROWSE
            </button>
            <button onClick={() => setCurrentScreen('UPLOAD')} style={{ padding: '6px 12px', backgroundColor: currentScreen === 'UPLOAD' ? C.ORANGE : C.DARK_BG, border: `2px solid ${C.GRAY}`, color: currentScreen === 'UPLOAD' ? C.DARK_BG : C.WHITE, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
              📤 UPLOAD
            </button>
            <button onClick={() => setCurrentScreen('MY_PURCHASES')} style={{ padding: '6px 12px', backgroundColor: currentScreen === 'MY_PURCHASES' ? C.ORANGE : C.DARK_BG, border: `2px solid ${C.GRAY}`, color: currentScreen === 'MY_PURCHASES' ? C.DARK_BG : C.WHITE, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
              📦 MY PURCHASES
            </button>
            <button onClick={() => setCurrentScreen('MY_DATASETS')} style={{ padding: '6px 12px', backgroundColor: currentScreen === 'MY_DATASETS' ? C.ORANGE : C.DARK_BG, border: `2px solid ${C.GRAY}`, color: currentScreen === 'MY_DATASETS' ? C.DARK_BG : C.WHITE, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
              📁 MY DATASETS
            </button>
            <button onClick={() => setCurrentScreen('ANALYTICS')} style={{ padding: '6px 12px', backgroundColor: currentScreen === 'ANALYTICS' ? C.ORANGE : C.DARK_BG, border: `2px solid ${C.GRAY}`, color: currentScreen === 'ANALYTICS' ? C.DARK_BG : C.WHITE, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
              📊 ANALYTICS
            </button>
            <button onClick={() => setCurrentScreen('PRICING')} style={{ padding: '6px 12px', backgroundColor: currentScreen === 'PRICING' ? C.ORANGE : C.DARK_BG, border: `2px solid ${C.GRAY}`, color: currentScreen === 'PRICING' ? C.DARK_BG : C.WHITE, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
              💰 PRICING
            </button>
            {(session?.user_info as any)?.is_admin && (
              <>
                <button onClick={() => setCurrentScreen('ADMIN_STATS')} style={{ padding: '6px 12px', backgroundColor: currentScreen === 'ADMIN_STATS' ? C.ORANGE : C.DARK_BG, border: `2px solid ${C.GRAY}`, color: currentScreen === 'ADMIN_STATS' ? C.DARK_BG : C.WHITE, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                  🔧 STATS
                </button>
                <button onClick={() => setCurrentScreen('ADMIN_PENDING')} style={{ padding: '6px 12px', backgroundColor: currentScreen === 'ADMIN_PENDING' ? C.ORANGE : C.DARK_BG, border: `2px solid ${C.GRAY}`, color: currentScreen === 'ADMIN_PENDING' ? C.DARK_BG : C.WHITE, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                  ⏳ PENDING
                </button>
                <button onClick={() => setCurrentScreen('ADMIN_REVENUE')} style={{ padding: '6px 12px', backgroundColor: currentScreen === 'ADMIN_REVENUE' ? C.ORANGE : C.DARK_BG, border: `2px solid ${C.GRAY}`, color: currentScreen === 'ADMIN_REVENUE' ? C.DARK_BG : C.WHITE, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                  💵 REVENUE
                </button>
              </>
            )}
            <button onClick={() => setCurrentScreen('CART')} style={{ padding: '6px 12px', backgroundColor: currentScreen === 'CART' ? C.ORANGE : C.DARK_BG, border: `2px solid ${C.GRAY}`, color: currentScreen === 'CART' ? C.DARK_BG : C.WHITE, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
              🛒 CART {cartItems.length > 0 && `(${cartItems.length})`}
            </button>
          </div>
        </div>
      </div>

      {/* Content */}
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', minHeight: 0 }}>
        {/* Guest User Warning Banner */}
        {isGuestUser && (
          <div style={{ backgroundColor: C.ORANGE, padding: '12px 20px', borderBottom: `2px solid ${C.GRAY}` }}>
            <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', gap: '16px' }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '12px', flex: 1 }}>
                <span style={{ fontSize: '20px' }}>🔒</span>
                <div>
                  <div style={{ color: C.DARK_BG, fontSize: '14px', fontWeight: 'bold', marginBottom: '2px' }}>
                    MARKETPLACE REQUIRES REGISTERED ACCOUNT
                  </div>
                  <div style={{ color: 'rgba(0,0,0,0.8)', fontSize: '12px' }}>
                    Guest users cannot access marketplace features. Please register to upload, purchase, and trade datasets.
                  </div>
                </div>
              </div>
            </div>
          </div>
        )}

        {currentScreen === 'BROWSE' && (
          <>
            {/* Search */}
            <div style={{ padding: '12px', backgroundColor: C.PANEL_BG, borderBottom: `2px solid ${C.GRAY}` }}>
              <div style={{ display: 'flex', gap: '12px', marginBottom: '12px' }}>
                <input type="text" placeholder="Search products..." value={searchQuery} onChange={(e) => setSearchQuery(e.target.value)} style={{ flex: 1, padding: '10px 16px', backgroundColor: C.DARK_BG, border: `2px solid ${C.GRAY}`, color: C.WHITE, fontSize: '14px', fontFamily: 'Consolas, monospace' }} />
                <button style={{ padding: '10px 24px', backgroundColor: C.ORANGE, border: 'none', color: C.DARK_BG, fontSize: '14px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>SEARCH</button>
              </div>
              <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap', alignItems: 'center', marginBottom: '8px' }}>
                {categories.map((cat) => (
                  <button key={cat.name} onClick={() => setSelectedCategory(cat.name)} style={{ padding: '8px 16px', backgroundColor: selectedCategory === cat.name ? C.ORANGE : C.DARK_BG, border: `2px solid ${selectedCategory === cat.name ? C.ORANGE : C.GRAY}`, color: selectedCategory === cat.name ? C.DARK_BG : C.WHITE, fontSize: '12px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                    {cat.name.toUpperCase()} ({cat.count})
                  </button>
                ))}
              </div>
              <div style={{ display: 'flex', alignItems: 'center', gap: '12px', padding: '8px', backgroundColor: 'rgba(255,255,255,0.05)', borderRadius: '4px' }}>
                <label style={{ display: 'flex', alignItems: 'center', gap: '8px', cursor: 'pointer' }}>
                  <input
                    type="checkbox"
                    checked={showAllDatasets}
                    onChange={(e) => setShowAllDatasets(e.target.checked)}
                    style={{ cursor: 'pointer' }}
                  />
                  <span style={{ color: C.WHITE, fontSize: '12px' }}>
                    Show ALL datasets (including pending/my uploads)
                  </span>
                </label>
                <span style={{ color: C.GRAY, fontSize: '11px' }}>
                  | API returned: {datasets.length} datasets | Filtered: {filteredProducts.length} visible
                </span>
              </div>
            </div>

            {/* Products */}
            <div className="marketplace-scroll" style={{ flex: 1, overflowY: 'auto', backgroundColor: '#050505', padding: '16px' }}>
              {isLoading ? (
                <div style={{ textAlign: 'center', padding: '40px', color: C.GRAY }}>
                  <div style={{ fontSize: '18px', marginBottom: '12px' }}>Loading datasets...</div>
                </div>
              ) : filteredProducts.length === 0 ? (
                <div style={{ textAlign: 'center', padding: '40px', color: C.GRAY }}>
                  <div style={{ fontSize: '18px', marginBottom: '12px' }}>No datasets found</div>
                  <div style={{ fontSize: '14px' }}>Try adjusting your filters or search query</div>
                </div>
              ) : (
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(380px, 1fr))', gap: '16px' }}>
                  {filteredProducts.map((product) => {
                    const dataset = allDatasets.find(d => d.id.toString() === product.id);
                    const status = dataset?.status || 'approved';
                    const statusColor = status === 'approved' ? C.GREEN : status === 'pending' ? C.YELLOW : C.RED;
                    return (
                    <div key={product.id} style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, borderLeft: `6px solid ${getTypeColor(product.type)}`, padding: '16px' }}>
                      <div style={{ display: 'flex', gap: '8px', marginBottom: '8px' }}>
                        <div style={{ display: 'inline-block', backgroundColor: getTypeColor(product.type), color: C.DARK_BG, padding: '4px 10px', fontSize: '10px', fontWeight: 'bold' }}>
                          {product.type}
                        </div>
                        {showAllDatasets && (
                          <div style={{ display: 'inline-block', backgroundColor: statusColor, color: C.DARK_BG, padding: '4px 10px', fontSize: '10px', fontWeight: 'bold' }}>
                            {status.toUpperCase()}
                          </div>
                        )}
                      </div>
                      <div style={{ color: C.WHITE, fontSize: '16px', fontWeight: 'bold', marginBottom: '4px' }}>{product.name}</div>
                      <div style={{ color: C.GRAY, fontSize: '12px', marginBottom: '8px' }}>by <span style={{ color: C.CYAN }}>{product.vendor}</span></div>
                    <div style={{ color: C.WHITE, fontSize: '13px', lineHeight: '1.5', marginBottom: '12px', minHeight: '60px' }}>{product.shortDescription}</div>
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', marginBottom: '12px', padding: '10px', backgroundColor: 'rgba(255,255,255,0.03)', border: `1px solid ${C.GRAY}` }}>
                      <div>
                        <div style={{ color: C.GRAY, fontSize: '9px' }}>RATING</div>
                        <div style={{ color: C.YELLOW, fontSize: '13px', fontWeight: 'bold' }}>★ {product.rating} ({product.reviews})</div>
                      </div>
                      <div>
                        <div style={{ color: C.GRAY, fontSize: '9px' }}>USERS</div>
                        <div style={{ color: C.CYAN, fontSize: '13px', fontWeight: 'bold' }}>{product.users.toLocaleString()}</div>
                      </div>
                    </div>
                    <div style={{ display: 'flex', gap: '8px' }}>
                      <button onClick={() => { setSelectedProduct(product); setCurrentScreen('DETAILS'); }} style={{ flex: 1, padding: '10px', backgroundColor: C.DARK_BG, border: `2px solid ${C.BLUE}`, color: C.BLUE, fontSize: '12px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                        VIEW DETAILS
                      </button>
                      <button onClick={() => !cartItems.includes(product.id) && setCartItems([...cartItems, product.id])} style={{ flex: 1, padding: '10px', backgroundColor: cartItems.includes(product.id) ? C.GREEN : C.DARK_BG, border: `2px solid ${C.GREEN}`, color: cartItems.includes(product.id) ? C.DARK_BG : C.GREEN, fontSize: '12px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                        {cartItems.includes(product.id) ? '✓ IN CART' : 'ADD TO CART'}
                      </button>
                    </div>
                  </div>
                  );
                  })}
                </div>
              )}
            </div>
          </>
        )}

        {currentScreen === 'DETAILS' && selectedProduct && (
          <>
            <div style={{ padding: '12px', backgroundColor: C.PANEL_BG, borderBottom: `2px solid ${C.GRAY}` }}>
              <button onClick={() => setCurrentScreen('BROWSE')} style={{ padding: '8px 16px', backgroundColor: C.DARK_BG, border: `2px solid ${C.GRAY}`, color: C.WHITE, fontSize: '12px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                ← BACK TO MARKETPLACE
              </button>
            </div>
            <div className="marketplace-scroll" style={{ flex: 1, overflowY: 'auto', backgroundColor: '#050505', padding: '20px' }}>
              <div style={{ maxWidth: '1200px', margin: '0 auto' }}>
                <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, borderLeft: `8px solid ${getTypeColor(selectedProduct.type)}`, padding: '24px', marginBottom: '20px' }}>
                  <div style={{ display: 'inline-block', backgroundColor: getTypeColor(selectedProduct.type), color: C.DARK_BG, padding: '6px 14px', fontSize: '12px', fontWeight: 'bold', marginBottom: '12px' }}>
                    {selectedProduct.type}
                  </div>
                  <div style={{ color: C.WHITE, fontSize: '28px', fontWeight: 'bold', marginBottom: '8px' }}>{selectedProduct.name}</div>
                  <div style={{ color: C.GRAY, fontSize: '16px', marginBottom: '12px' }}>
                    by <span style={{ color: C.CYAN, fontWeight: 'bold' }}>{selectedProduct.vendor}</span> • <span style={{ color: C.BLUE }}>{selectedProduct.category}</span>
                  </div>
                  <div style={{ color: C.GREEN, fontSize: '36px', fontWeight: 'bold', marginTop: '16px' }}>
                    {selectedProduct.pricingModel === 'FREE' ? 'FREE' : `$${selectedProduct.price.toLocaleString()}`}
                    <span style={{ color: C.GRAY, fontSize: '13px', marginLeft: '8px' }}>
                      {selectedProduct.pricingModel === 'MONTHLY' && '/ month'}
                      {selectedProduct.pricingModel === 'ANNUAL' && '/ year'}
                      {selectedProduct.pricingModel === 'USAGE_BASED' && '/ request'}
                    </span>
                  </div>
                  <button onClick={() => !cartItems.includes(selectedProduct.id) && setCartItems([...cartItems, selectedProduct.id])} style={{ marginTop: '16px', padding: '14px 32px', backgroundColor: cartItems.includes(selectedProduct.id) ? C.GREEN : C.DARK_BG, border: `2px solid ${C.GREEN}`, color: cartItems.includes(selectedProduct.id) ? C.DARK_BG : C.GREEN, fontSize: '14px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                    {cartItems.includes(selectedProduct.id) ? '✓ ADDED TO CART' : 'ADD TO CART'}
                  </button>
                </div>
                <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, padding: '20px' }}>
                  <div style={{ color: C.ORANGE, fontSize: '16px', fontWeight: 'bold', marginBottom: '12px' }}>PRODUCT DESCRIPTION</div>
                  <div style={{ color: C.WHITE, fontSize: '14px', lineHeight: '1.7' }}>{selectedProduct.shortDescription}</div>
                </div>
              </div>
            </div>
          </>
        )}

        {currentScreen === 'CART' && (
          <>
            <div style={{ padding: '16px', backgroundColor: C.PANEL_BG, borderBottom: `2px solid ${C.GRAY}` }}>
              <div style={{ color: C.ORANGE, fontSize: '20px', fontWeight: 'bold' }}>SHOPPING CART ({cartItems.length} items)</div>
            </div>
            <div className="marketplace-scroll" style={{ flex: 1, overflowY: 'auto', backgroundColor: '#050505', padding: '20px' }}>
              <div style={{ maxWidth: '1000px', margin: '0 auto' }}>
                {cartItems.length === 0 ? (
                  <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, padding: '40px', textAlign: 'center' }}>
                    <div style={{ color: C.GRAY, fontSize: '18px', marginBottom: '16px' }}>Your cart is empty</div>
                    <button onClick={() => setCurrentScreen('BROWSE')} style={{ padding: '12px 24px', backgroundColor: C.ORANGE, border: 'none', color: C.DARK_BG, fontSize: '14px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                      BROWSE MARKETPLACE
                    </button>
                  </div>
                ) : (
                  <>
                    {products.filter(p => cartItems.includes(p.id)).map((product) => (
                      <div key={product.id} style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, borderLeft: `6px solid ${getTypeColor(product.type)}`, padding: '20px', marginBottom: '16px', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                        <div style={{ flex: 1 }}>
                          <div style={{ color: C.WHITE, fontSize: '18px', fontWeight: 'bold', marginBottom: '4px' }}>{product.name}</div>
                          <div style={{ color: C.GRAY, fontSize: '13px', marginBottom: '8px' }}>by {product.vendor}</div>
                        </div>
                        <div style={{ textAlign: 'right', marginLeft: '24px' }}>
                          <div style={{ color: C.GREEN, fontSize: '24px', fontWeight: 'bold', marginBottom: '8px' }}>
                            {product.pricingModel === 'FREE' ? 'FREE' : `${product.price} Credits`}
                          </div>
                          <div style={{ display: 'flex', gap: '8px' }}>
                            <button onClick={() => handlePurchase(product.id)} style={{ padding: '8px 16px', backgroundColor: C.DARK_BG, border: `2px solid ${C.GREEN}`, color: C.GREEN, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                              PURCHASE
                            </button>
                            <button onClick={() => setCartItems(cartItems.filter(id => id !== product.id))} style={{ padding: '8px 16px', backgroundColor: C.DARK_BG, border: `2px solid ${C.RED}`, color: C.RED, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                              REMOVE
                            </button>
                          </div>
                        </div>
                      </div>
                    ))}
                  </>
                )}
              </div>
            </div>
          </>
        )}

        {currentScreen === 'UPLOAD' && (
          <>
            <div style={{ padding: '12px', backgroundColor: C.PANEL_BG, borderBottom: `2px solid ${C.GRAY}` }}>
              <div style={{ color: C.ORANGE, fontSize: '20px', fontWeight: 'bold' }}>UPLOAD DATASET</div>
            </div>
            <div className="marketplace-scroll" style={{ flex: 1, overflowY: 'auto', backgroundColor: '#050505', padding: '20px' }}>
              <div style={{ maxWidth: '800px', margin: '0 auto', backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, padding: '24px' }}>
                <div style={{ marginBottom: '20px' }}>
                  <label style={{ display: 'block', color: C.WHITE, fontSize: '14px', fontWeight: 'bold', marginBottom: '8px' }}>Dataset Title</label>
                  <input
                    type="text"
                    value={uploadMetadata.title}
                    onChange={(e) => setUploadMetadata({ ...uploadMetadata, title: e.target.value })}
                    style={{ width: '100%', padding: '10px', backgroundColor: C.DARK_BG, border: `2px solid ${C.GRAY}`, color: C.WHITE, fontSize: '14px', fontFamily: 'Consolas, monospace' }}
                    placeholder="Enter dataset title"
                  />
                </div>
                <div style={{ marginBottom: '20px' }}>
                  <label style={{ display: 'block', color: C.WHITE, fontSize: '14px', fontWeight: 'bold', marginBottom: '8px' }}>Description</label>
                  <textarea
                    value={uploadMetadata.description}
                    onChange={(e) => setUploadMetadata({ ...uploadMetadata, description: e.target.value })}
                    style={{ width: '100%', padding: '10px', backgroundColor: C.DARK_BG, border: `2px solid ${C.GRAY}`, color: C.WHITE, fontSize: '14px', fontFamily: 'Consolas, monospace', minHeight: '100px' }}
                    placeholder="Describe your dataset"
                  />
                </div>
                <div style={{ marginBottom: '20px' }}>
                  <label style={{ display: 'block', color: C.WHITE, fontSize: '14px', fontWeight: 'bold', marginBottom: '8px' }}>Category</label>
                  <select
                    value={uploadMetadata.category}
                    onChange={(e) => setUploadMetadata({ ...uploadMetadata, category: e.target.value })}
                    style={{ width: '100%', padding: '10px', backgroundColor: C.DARK_BG, border: `2px solid ${C.GRAY}`, color: C.WHITE, fontSize: '14px', fontFamily: 'Consolas, monospace' }}
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
                <div style={{ marginBottom: '20px' }}>
                  <label style={{ display: 'block', color: C.WHITE, fontSize: '14px', fontWeight: 'bold', marginBottom: '8px' }}>Price Tier</label>
                  <select
                    value={uploadMetadata.price_tier}
                    onChange={(e) => setUploadMetadata({ ...uploadMetadata, price_tier: e.target.value })}
                    style={{ width: '100%', padding: '10px', backgroundColor: C.DARK_BG, border: `2px solid ${C.GRAY}`, color: C.WHITE, fontSize: '14px', fontFamily: 'Consolas, monospace' }}
                  >
                    <option value="free">Free (0 credits)</option>
                    <option value="basic">Basic (10 credits)</option>
                    <option value="premium">Premium (50 credits)</option>
                    <option value="enterprise">Enterprise (100 credits)</option>
                  </select>
                </div>
                <div style={{ marginBottom: '20px' }}>
                  <label style={{ display: 'block', color: C.WHITE, fontSize: '14px', fontWeight: 'bold', marginBottom: '8px' }}>Tags (comma separated)</label>
                  <input
                    type="text"
                    value={uploadMetadata.tags}
                    onChange={(e) => setUploadMetadata({ ...uploadMetadata, tags: e.target.value })}
                    style={{ width: '100%', padding: '10px', backgroundColor: C.DARK_BG, border: `2px solid ${C.GRAY}`, color: C.WHITE, fontSize: '14px', fontFamily: 'Consolas, monospace' }}
                    placeholder="stocks, real-time, equity"
                  />
                </div>
                <div style={{ marginBottom: '20px' }}>
                  <label style={{ display: 'block', color: C.WHITE, fontSize: '14px', fontWeight: 'bold', marginBottom: '8px' }}>Upload File</label>
                  <input
                    type="file"
                    onChange={(e) => setUploadFile(e.target.files?.[0] || null)}
                    style={{ width: '100%', padding: '10px', backgroundColor: C.DARK_BG, border: `2px solid ${C.GRAY}`, color: C.WHITE, fontSize: '14px', fontFamily: 'Consolas, monospace' }}
                  />
                  {uploadFile && (
                    <div style={{ marginTop: '8px', color: C.CYAN, fontSize: '12px' }}>
                      Selected: {uploadFile.name} ({(uploadFile.size / 1024 / 1024).toFixed(2)} MB)
                    </div>
                  )}
                </div>
                <button
                  onClick={handleUpload}
                  disabled={isLoading}
                  style={{ padding: '12px 32px', backgroundColor: isLoading ? C.GRAY : C.ORANGE, border: 'none', color: C.DARK_BG, fontSize: '14px', fontWeight: 'bold', cursor: isLoading ? 'not-allowed' : 'pointer', fontFamily: 'Consolas, monospace' }}
                >
                  {isLoading ? 'UPLOADING...' : 'UPLOAD DATASET'}
                </button>
              </div>
            </div>
          </>
        )}

        {currentScreen === 'MY_PURCHASES' && <MyPurchasesScreen session={session} C={C} />}
        {currentScreen === 'MY_DATASETS' && <MyDatasetsScreen session={session} C={C} fetchMyDatasets={fetchMyDatasets} />}
        {currentScreen === 'ANALYTICS' && <AnalyticsScreen session={session} C={C} />}
        {currentScreen === 'PRICING' && <PricingScreen session={session} C={C} />}
        {currentScreen === 'ADMIN_STATS' && <AdminStatsScreen session={session} C={C} />}
        {currentScreen === 'ADMIN_PENDING' && <AdminPendingScreen session={session} C={C} />}
        {currentScreen === 'ADMIN_REVENUE' && <AdminRevenueScreen session={session} C={C} />}
      </div>

      {/* Footer */}
      <div style={{ borderTop: `3px solid ${C.ORANGE}`, backgroundColor: C.PANEL_BG, padding: '12px 16px', fontSize: '11px' }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
            <span style={{ color: C.ORANGE, fontWeight: 'bold', fontSize: '13px' }}>FINCEPT MARKETPLACE v3.0</span>
            <span style={{ color: C.GRAY }}>|</span>
            <span style={{ color: C.CYAN }}>🔒 SECURE PRODUCTS</span>
            <span style={{ color: C.GRAY }}>|</span>
            <span style={{ color: C.GREEN }}>✓ VERIFIED VENDORS</span>
          </div>
          <div style={{ color: C.GRAY }}>
            Screen: <span style={{ color: C.ORANGE, fontWeight: 'bold' }}>{currentScreen}</span> | Showing: {filteredProducts.length} / {products.length}
          </div>
        </div>
      </div>
    </div>
  );
};

// My Purchases Screen Component
const MyPurchasesScreen: React.FC<{ session: any; C: any }> = ({ session, C }) => {
  const [purchases, setPurchases] = useState<any[]>([]);
  const [isLoading, setIsLoading] = useState(false);

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
      }
    } catch (error) {
      alert('Download failed');
    }
  };

  return (
    <>
      <div style={{ padding: '12px', backgroundColor: C.PANEL_BG, borderBottom: `2px solid ${C.GRAY}` }}>
        <div style={{ color: C.ORANGE, fontSize: '20px', fontWeight: 'bold' }}>MY PURCHASES ({purchases.length})</div>
      </div>
      <div className="marketplace-scroll" style={{ flex: 1, overflowY: 'auto', backgroundColor: '#050505', padding: '20px' }}>
        <div style={{ maxWidth: '1200px', margin: '0 auto' }}>
          {isLoading ? (
            <div style={{ textAlign: 'center', padding: '40px', color: C.GRAY }}>Loading...</div>
          ) : purchases.length === 0 ? (
            <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, padding: '40px', textAlign: 'center' }}>
              <div style={{ color: C.GRAY, fontSize: '18px' }}>No purchases yet</div>
            </div>
          ) : (
            purchases.map((purchase) => (
              <div key={purchase.id} style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, borderLeft: `6px solid ${C.GREEN}`, padding: '20px', marginBottom: '16px' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                  <div>
                    <div style={{ color: C.WHITE, fontSize: '18px', fontWeight: 'bold', marginBottom: '4px' }}>{purchase.dataset_title}</div>
                    <div style={{ color: C.GRAY, fontSize: '13px', marginBottom: '8px' }}>
                      Purchased: {new Date(purchase.purchased_at).toLocaleDateString()} | Paid: {purchase.price_paid} credits
                    </div>
                  </div>
                  <button onClick={() => handleDownload(purchase.dataset_id)} style={{ padding: '10px 20px', backgroundColor: C.DARK_BG, border: `2px solid ${C.BLUE}`, color: C.BLUE, fontSize: '12px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                    📥 DOWNLOAD
                  </button>
                </div>
              </div>
            ))
          )}
        </div>
      </div>
    </>
  );
};

// Analytics Screen Component
const AnalyticsScreen: React.FC<{ session: any; C: any }> = ({ session, C }) => {
  const [analytics, setAnalytics] = useState<any>(null);
  const [isLoading, setIsLoading] = useState(false);

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
    <>
      <div style={{ padding: '12px', backgroundColor: C.PANEL_BG, borderBottom: `2px solid ${C.GRAY}` }}>
        <div style={{ color: C.ORANGE, fontSize: '20px', fontWeight: 'bold' }}>MY DATASET ANALYTICS</div>
      </div>
      <div className="marketplace-scroll" style={{ flex: 1, overflowY: 'auto', backgroundColor: '#050505', padding: '20px' }}>
        <div style={{ maxWidth: '1200px', margin: '0 auto' }}>
          {isLoading ? (
            <div style={{ textAlign: 'center', padding: '40px', color: C.GRAY }}>Loading...</div>
          ) : !analytics ? (
            <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, padding: '40px', textAlign: 'center' }}>
              <div style={{ color: C.GRAY, fontSize: '18px' }}>No analytics data available</div>
            </div>
          ) : (
            <>
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '16px', marginBottom: '24px' }}>
                <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.BLUE}`, padding: '20px', textAlign: 'center' }}>
                  <div style={{ color: C.BLUE, fontSize: '36px', fontWeight: 'bold' }}>{analytics.total_datasets || 0}</div>
                  <div style={{ color: C.WHITE, fontSize: '14px', marginTop: '8px' }}>Total Datasets</div>
                </div>
                <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GREEN}`, padding: '20px', textAlign: 'center' }}>
                  <div style={{ color: C.GREEN, fontSize: '36px', fontWeight: 'bold' }}>{analytics.total_downloads || 0}</div>
                  <div style={{ color: C.WHITE, fontSize: '14px', marginTop: '8px' }}>Total Downloads</div>
                </div>
                <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.ORANGE}`, padding: '20px', textAlign: 'center' }}>
                  <div style={{ color: C.ORANGE, fontSize: '36px', fontWeight: 'bold' }}>{analytics.total_revenue || 0}</div>
                  <div style={{ color: C.WHITE, fontSize: '14px', marginTop: '8px' }}>Total Revenue (credits)</div>
                </div>
              </div>
              {analytics.datasets && analytics.datasets.length > 0 && (
                <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, padding: '20px' }}>
                  <div style={{ color: C.ORANGE, fontSize: '16px', fontWeight: 'bold', marginBottom: '16px' }}>DATASET PERFORMANCE</div>
                  {analytics.datasets.map((dataset: any) => (
                    <div key={dataset.id} style={{ borderBottom: `1px solid ${C.GRAY}`, padding: '12px 0' }}>
                      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                        <div style={{ color: C.WHITE, fontSize: '14px' }}>{dataset.title}</div>
                        <div style={{ display: 'flex', gap: '24px' }}>
                          <div style={{ textAlign: 'center' }}>
                            <div style={{ color: C.CYAN, fontSize: '18px', fontWeight: 'bold' }}>{dataset.downloads}</div>
                            <div style={{ color: C.GRAY, fontSize: '11px' }}>Downloads</div>
                          </div>
                          <div style={{ textAlign: 'center' }}>
                            <div style={{ color: C.GREEN, fontSize: '18px', fontWeight: 'bold' }}>{dataset.revenue}</div>
                            <div style={{ color: C.GRAY, fontSize: '11px' }}>Revenue</div>
                          </div>
                          <div style={{ textAlign: 'center' }}>
                            <div style={{ color: C.YELLOW, fontSize: '18px', fontWeight: 'bold' }}>★ {dataset.rating.toFixed(1)}</div>
                            <div style={{ color: C.GRAY, fontSize: '11px' }}>Rating</div>
                          </div>
                        </div>
                      </div>
                    </div>
                  ))}
                </div>
              )}
            </>
          )}
        </div>
      </div>
    </>
  );
};

// Admin Stats Screen Component
const AdminStatsScreen: React.FC<{ session: any; C: any }> = ({ session, C }) => {
  const [stats, setStats] = useState<any>(null);
  const [isLoading, setIsLoading] = useState(false);

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
        console.error('Failed to fetch admin stats:', error);
      } finally {
        setIsLoading(false);
      }
    };
    fetchStats();
  }, [session?.api_key]);

  return (
    <>
      <div style={{ padding: '12px', backgroundColor: C.PANEL_BG, borderBottom: `2px solid ${C.GRAY}` }}>
        <div style={{ color: C.ORANGE, fontSize: '20px', fontWeight: 'bold' }}>🔧 ADMIN MARKETPLACE STATISTICS</div>
      </div>
      <div className="marketplace-scroll" style={{ flex: 1, overflowY: 'auto', backgroundColor: '#050505', padding: '20px' }}>
        <div style={{ maxWidth: '1200px', margin: '0 auto' }}>
          {isLoading ? (
            <div style={{ textAlign: 'center', padding: '40px', color: C.GRAY }}>Loading...</div>
          ) : !stats ? (
            <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, padding: '40px', textAlign: 'center' }}>
              <div style={{ color: C.GRAY, fontSize: '18px' }}>No statistics available</div>
            </div>
          ) : (
            <>
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '16px', marginBottom: '24px' }}>
                <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.BLUE}`, padding: '20px', textAlign: 'center' }}>
                  <div style={{ color: C.BLUE, fontSize: '36px', fontWeight: 'bold' }}>{stats.total_datasets || 0}</div>
                  <div style={{ color: C.WHITE, fontSize: '14px', marginTop: '8px' }}>Total Datasets</div>
                </div>
                <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GREEN}`, padding: '20px', textAlign: 'center' }}>
                  <div style={{ color: C.GREEN, fontSize: '36px', fontWeight: 'bold' }}>{stats.total_purchases || 0}</div>
                  <div style={{ color: C.WHITE, fontSize: '14px', marginTop: '8px' }}>Total Purchases</div>
                </div>
                <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.ORANGE}`, padding: '20px', textAlign: 'center' }}>
                  <div style={{ color: C.ORANGE, fontSize: '36px', fontWeight: 'bold' }}>{stats.total_revenue || 0}</div>
                  <div style={{ color: C.WHITE, fontSize: '14px', marginTop: '8px' }}>Total Revenue (credits)</div>
                </div>
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '16px' }}>
                <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.CYAN}`, padding: '20px', textAlign: 'center' }}>
                  <div style={{ color: C.CYAN, fontSize: '36px', fontWeight: 'bold' }}>{stats.total_uploads || 0}</div>
                  <div style={{ color: C.WHITE, fontSize: '14px', marginTop: '8px' }}>Total Uploads</div>
                </div>
                <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.PURPLE}`, padding: '20px', textAlign: 'center' }}>
                  <div style={{ color: C.PURPLE, fontSize: '36px', fontWeight: 'bold' }}>{stats.active_users || 0}</div>
                  <div style={{ color: C.WHITE, fontSize: '14px', marginTop: '8px' }}>Active Users</div>
                </div>
              </div>
              {stats.popular_categories && (
                <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, padding: '20px', marginTop: '24px' }}>
                  <div style={{ color: C.ORANGE, fontSize: '16px', fontWeight: 'bold', marginBottom: '16px' }}>POPULAR CATEGORIES</div>
                  {Object.entries(stats.popular_categories).map(([category, count]: [string, any]) => (
                    <div key={category} style={{ borderBottom: `1px solid ${C.GRAY}`, padding: '12px 0', display: 'flex', justifyContent: 'space-between' }}>
                      <div style={{ color: C.WHITE, fontSize: '14px' }}>{category}</div>
                      <div style={{ color: C.CYAN, fontSize: '14px', fontWeight: 'bold' }}>{count} datasets</div>
                    </div>
                  ))}
                </div>
              )}
            </>
          )}
        </div>
      </div>
    </>
  );
};

// My Datasets Screen Component (with Edit/Delete)
const MyDatasetsScreen: React.FC<{ session: any; C: any; fetchMyDatasets: () => void }> = ({ session, C, fetchMyDatasets }) => {
  const [myDatasets, setMyDatasets] = useState<Dataset[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [editingDataset, setEditingDataset] = useState<Dataset | null>(null);
  const [editForm, setEditForm] = useState({ title: '', description: '', category: '', price_tier: '', tags: '' });

  useEffect(() => {
    loadMyDatasets();
  }, [session?.api_key]);

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
        fetchMyDatasets();
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
    if (!confirm(`Are you sure you want to delete "${datasetTitle}"? This action cannot be undone.`)) return;

    setIsLoading(true);
    try {
      const response = await MarketplaceApiService.deleteDataset(session.api_key, datasetId);
      if (response.success) {
        alert('Dataset deleted successfully!');
        loadMyDatasets();
        fetchMyDatasets();
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
      case 'approved': return C.GREEN;
      case 'pending': return C.YELLOW;
      case 'rejected': return C.RED;
      default: return C.GRAY;
    }
  };

  return (
    <>
      <div style={{ padding: '12px', backgroundColor: C.PANEL_BG, borderBottom: `2px solid ${C.GRAY}` }}>
        <div style={{ color: C.ORANGE, fontSize: '20px', fontWeight: 'bold' }}>📁 MY UPLOADED DATASETS ({myDatasets.length})</div>
      </div>
      <div className="marketplace-scroll" style={{ flex: 1, overflowY: 'auto', backgroundColor: '#050505', padding: '20px' }}>
        <div style={{ maxWidth: '1200px', margin: '0 auto' }}>
          {editingDataset ? (
            <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.ORANGE}`, padding: '24px', marginBottom: '20px' }}>
              <div style={{ color: C.ORANGE, fontSize: '18px', fontWeight: 'bold', marginBottom: '20px' }}>EDIT DATASET</div>
              <div style={{ marginBottom: '16px' }}>
                <label style={{ display: 'block', color: C.WHITE, fontSize: '13px', fontWeight: 'bold', marginBottom: '6px' }}>Title</label>
                <input type="text" value={editForm.title} onChange={(e) => setEditForm({ ...editForm, title: e.target.value })} style={{ width: '100%', padding: '8px', backgroundColor: C.DARK_BG, border: `2px solid ${C.GRAY}`, color: C.WHITE, fontSize: '13px', fontFamily: 'Consolas, monospace' }} />
              </div>
              <div style={{ marginBottom: '16px' }}>
                <label style={{ display: 'block', color: C.WHITE, fontSize: '13px', fontWeight: 'bold', marginBottom: '6px' }}>Description</label>
                <textarea value={editForm.description} onChange={(e) => setEditForm({ ...editForm, description: e.target.value })} style={{ width: '100%', padding: '8px', backgroundColor: C.DARK_BG, border: `2px solid ${C.GRAY}`, color: C.WHITE, fontSize: '13px', fontFamily: 'Consolas, monospace', minHeight: '80px' }} />
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px', marginBottom: '16px' }}>
                <div>
                  <label style={{ display: 'block', color: C.WHITE, fontSize: '13px', fontWeight: 'bold', marginBottom: '6px' }}>Category</label>
                  <select value={editForm.category} onChange={(e) => setEditForm({ ...editForm, category: e.target.value })} style={{ width: '100%', padding: '8px', backgroundColor: C.DARK_BG, border: `2px solid ${C.GRAY}`, color: C.WHITE, fontSize: '13px', fontFamily: 'Consolas, monospace' }}>
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
                  <label style={{ display: 'block', color: C.WHITE, fontSize: '13px', fontWeight: 'bold', marginBottom: '6px' }}>Price Tier</label>
                  <select value={editForm.price_tier} onChange={(e) => setEditForm({ ...editForm, price_tier: e.target.value })} style={{ width: '100%', padding: '8px', backgroundColor: C.DARK_BG, border: `2px solid ${C.GRAY}`, color: C.WHITE, fontSize: '13px', fontFamily: 'Consolas, monospace' }}>
                    <option value="free">Free</option>
                    <option value="basic">Basic</option>
                    <option value="premium">Premium</option>
                    <option value="enterprise">Enterprise</option>
                  </select>
                </div>
              </div>
              <div style={{ marginBottom: '20px' }}>
                <label style={{ display: 'block', color: C.WHITE, fontSize: '13px', fontWeight: 'bold', marginBottom: '6px' }}>Tags (comma separated)</label>
                <input type="text" value={editForm.tags} onChange={(e) => setEditForm({ ...editForm, tags: e.target.value })} style={{ width: '100%', padding: '8px', backgroundColor: C.DARK_BG, border: `2px solid ${C.GRAY}`, color: C.WHITE, fontSize: '13px', fontFamily: 'Consolas, monospace' }} />
              </div>
              <div style={{ display: 'flex', gap: '12px' }}>
                <button onClick={handleUpdate} disabled={isLoading} style={{ padding: '10px 20px', backgroundColor: isLoading ? C.GRAY : C.GREEN, border: 'none', color: C.DARK_BG, fontSize: '12px', fontWeight: 'bold', cursor: isLoading ? 'not-allowed' : 'pointer', fontFamily: 'Consolas, monospace' }}>
                  {isLoading ? 'UPDATING...' : '💾 SAVE CHANGES'}
                </button>
                <button onClick={() => setEditingDataset(null)} style={{ padding: '10px 20px', backgroundColor: C.DARK_BG, border: `2px solid ${C.GRAY}`, color: C.WHITE, fontSize: '12px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                  ❌ CANCEL
                </button>
              </div>
            </div>
          ) : null}

          {isLoading && !editingDataset ? (
            <div style={{ textAlign: 'center', padding: '40px', color: C.GRAY }}>Loading...</div>
          ) : myDatasets.length === 0 ? (
            <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, padding: '40px', textAlign: 'center' }}>
              <div style={{ color: C.GRAY, fontSize: '18px' }}>No datasets uploaded yet</div>
            </div>
          ) : (
            myDatasets.map((dataset) => (
              <div key={dataset.id} style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, borderLeft: `6px solid ${getStatusColor(dataset.status || 'pending')}`, padding: '20px', marginBottom: '16px' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: '12px' }}>
                  <div style={{ flex: 1 }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '4px' }}>
                      <div style={{ color: C.WHITE, fontSize: '18px', fontWeight: 'bold' }}>{dataset.title}</div>
                      <div style={{ display: 'inline-block', backgroundColor: getStatusColor(dataset.status || 'pending'), color: C.DARK_BG, padding: '2px 8px', fontSize: '10px', fontWeight: 'bold', borderRadius: '3px' }}>
                        {(dataset.status || 'pending').toUpperCase()}
                      </div>
                    </div>
                    <div style={{ color: C.GRAY, fontSize: '13px', marginBottom: '8px' }}>
                      {dataset.category} • {dataset.price_tier} • {dataset.downloads_count} downloads • Uploaded: {new Date(dataset.uploaded_at).toLocaleDateString()}
                    </div>
                    <div style={{ color: C.WHITE, fontSize: '13px', lineHeight: '1.5' }}>{dataset.description}</div>
                    {dataset.rejection_reason && (
                      <div style={{ marginTop: '12px', backgroundColor: 'rgba(255,0,0,0.1)', border: `1px solid ${C.RED}`, padding: '10px', borderRadius: '4px' }}>
                        <div style={{ color: C.RED, fontSize: '12px', fontWeight: 'bold', marginBottom: '4px' }}>❌ REJECTION REASON:</div>
                        <div style={{ color: C.RED, fontSize: '12px' }}>{dataset.rejection_reason}</div>
                      </div>
                    )}
                  </div>
                  <div style={{ display: 'flex', gap: '8px', marginLeft: '20px' }}>
                    {(dataset.status === 'pending' || dataset.status === 'rejected') && (
                      <button onClick={() => handleEdit(dataset)} style={{ padding: '8px 16px', backgroundColor: C.DARK_BG, border: `2px solid ${C.BLUE}`, color: C.BLUE, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                        ✏️ EDIT
                      </button>
                    )}
                    <button onClick={() => handleDelete(dataset.id, dataset.title)} style={{ padding: '8px 16px', backgroundColor: C.DARK_BG, border: `2px solid ${C.RED}`, color: C.RED, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                      🗑️ DELETE
                    </button>
                  </div>
                </div>
                {dataset.tags && dataset.tags.length > 0 && (
                  <div style={{ display: 'flex', gap: '6px', flexWrap: 'wrap' }}>
                    {dataset.tags.map((tag, idx) => (
                      <span key={idx} style={{ display: 'inline-block', backgroundColor: 'rgba(255,255,255,0.1)', color: C.CYAN, padding: '2px 8px', fontSize: '10px', borderRadius: '3px' }}>
                        #{tag}
                      </span>
                    ))}
                  </div>
                )}
              </div>
            ))
          )}
        </div>
      </div>
    </>
  );
};

// Pricing Screen Component
const PricingScreen: React.FC<{ session: any; C: any }> = ({ session, C }) => {
  const [pricingTiers, setPricingTiers] = useState<PricingTier[]>([]);
  const [isLoading, setIsLoading] = useState(false);

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
    <>
      <div style={{ padding: '12px', backgroundColor: C.PANEL_BG, borderBottom: `2px solid ${C.GRAY}` }}>
        <div style={{ color: C.ORANGE, fontSize: '20px', fontWeight: 'bold' }}>💰 PRICING TIERS</div>
      </div>
      <div className="marketplace-scroll" style={{ flex: 1, overflowY: 'auto', backgroundColor: '#050505', padding: '20px' }}>
        <div style={{ maxWidth: '1200px', margin: '0 auto' }}>
          {isLoading ? (
            <div style={{ textAlign: 'center', padding: '40px', color: C.GRAY }}>Loading...</div>
          ) : pricingTiers.length === 0 ? (
            <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, padding: '40px', textAlign: 'center' }}>
              <div style={{ color: C.GRAY, fontSize: '18px' }}>No pricing information available</div>
            </div>
          ) : (
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(280px, 1fr))', gap: '20px' }}>
              {pricingTiers.map((tier, idx) => (
                <div key={idx} style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.ORANGE}`, padding: '24px', textAlign: 'center' }}>
                  <div style={{ color: C.ORANGE, fontSize: '20px', fontWeight: 'bold', textTransform: 'uppercase', marginBottom: '12px' }}>{tier.tier}</div>
                  <div style={{ color: C.GREEN, fontSize: '42px', fontWeight: 'bold', marginBottom: '8px' }}>{tier.price_credits}</div>
                  <div style={{ color: C.GRAY, fontSize: '13px', marginBottom: '16px' }}>Credits</div>
                  <div style={{ color: C.WHITE, fontSize: '13px', lineHeight: '1.6', marginBottom: '20px' }}>{tier.description}</div>
                  {tier.features && tier.features.length > 0 && (
                    <div style={{ textAlign: 'left', borderTop: `1px solid ${C.GRAY}`, paddingTop: '16px' }}>
                      <div style={{ color: C.CYAN, fontSize: '12px', fontWeight: 'bold', marginBottom: '12px' }}>FEATURES:</div>
                      {tier.features.map((feature, fidx) => (
                        <div key={fidx} style={{ color: C.WHITE, fontSize: '12px', marginBottom: '6px', display: 'flex', alignItems: 'center', gap: '8px' }}>
                          <span style={{ color: C.GREEN }}>✓</span>
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
    </>
  );
};

// Admin Pending Screen Component
const AdminPendingScreen: React.FC<{ session: any; C: any }> = ({ session, C }) => {
  const [pendingDatasets, setPendingDatasets] = useState<Dataset[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [rejectionReason, setRejectionReason] = useState('');
  const [rejectingDatasetId, setRejectingDatasetId] = useState<number | null>(null);

  useEffect(() => {
    loadPendingDatasets();
  }, [session?.api_key]);

  const loadPendingDatasets = async () => {
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

  const handleApprove = async (datasetId: number, datasetTitle: string) => {
    if (!session?.api_key) return;
    if (!confirm(`Approve dataset "${datasetTitle}"?`)) return;

    setIsLoading(true);
    try {
      const response = await MarketplaceApiService.approveDataset(session.api_key, datasetId);
      if (response.success) {
        alert('Dataset approved successfully!');
        loadPendingDatasets();
      } else {
        alert(`Approval failed: ${response.error}`);
      }
    } catch (error) {
      alert('Approval failed');
    } finally {
      setIsLoading(false);
    }
  };

  const handleReject = async (datasetId: number) => {
    if (!session?.api_key || !rejectionReason.trim()) {
      alert('Please provide a rejection reason');
      return;
    }

    setIsLoading(true);
    try {
      const response = await MarketplaceApiService.rejectDataset(session.api_key, datasetId, rejectionReason);
      if (response.success) {
        alert('Dataset rejected successfully!');
        setRejectingDatasetId(null);
        setRejectionReason('');
        loadPendingDatasets();
      } else {
        alert(`Rejection failed: ${response.error}`);
      }
    } catch (error) {
      alert('Rejection failed');
    } finally {
      setIsLoading(false);
    }
  };

  return (
    <>
      <div style={{ padding: '12px', backgroundColor: C.PANEL_BG, borderBottom: `2px solid ${C.GRAY}` }}>
        <div style={{ color: C.ORANGE, fontSize: '20px', fontWeight: 'bold' }}>⏳ PENDING DATASETS APPROVAL ({pendingDatasets.length})</div>
      </div>
      <div className="marketplace-scroll" style={{ flex: 1, overflowY: 'auto', backgroundColor: '#050505', padding: '20px' }}>
        <div style={{ maxWidth: '1200px', margin: '0 auto' }}>
          {isLoading ? (
            <div style={{ textAlign: 'center', padding: '40px', color: C.GRAY }}>Loading...</div>
          ) : pendingDatasets.length === 0 ? (
            <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, padding: '40px', textAlign: 'center' }}>
              <div style={{ color: C.GRAY, fontSize: '18px' }}>No pending datasets</div>
            </div>
          ) : (
            pendingDatasets.map((dataset) => (
              <div key={dataset.id} style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.YELLOW}`, borderLeft: `6px solid ${C.YELLOW}`, padding: '20px', marginBottom: '16px' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: '12px' }}>
                  <div style={{ flex: 1 }}>
                    <div style={{ color: C.WHITE, fontSize: '18px', fontWeight: 'bold', marginBottom: '4px' }}>{dataset.title}</div>
                    <div style={{ color: C.GRAY, fontSize: '13px', marginBottom: '8px' }}>
                      by {dataset.uploader_username} • {dataset.category} • {dataset.price_tier} • {(dataset.file_size / 1024 / 1024).toFixed(2)} MB
                    </div>
                    <div style={{ color: C.WHITE, fontSize: '13px', lineHeight: '1.5', marginBottom: '12px' }}>{dataset.description}</div>
                    {dataset.tags && dataset.tags.length > 0 && (
                      <div style={{ display: 'flex', gap: '6px', flexWrap: 'wrap', marginBottom: '12px' }}>
                        {dataset.tags.map((tag, idx) => (
                          <span key={idx} style={{ display: 'inline-block', backgroundColor: 'rgba(255,255,255,0.1)', color: C.CYAN, padding: '2px 8px', fontSize: '10px', borderRadius: '3px' }}>
                            #{tag}
                          </span>
                        ))}
                      </div>
                    )}
                  </div>
                </div>

                {rejectingDatasetId === dataset.id ? (
                  <div style={{ backgroundColor: 'rgba(255,0,0,0.1)', border: `1px solid ${C.RED}`, padding: '16px', borderRadius: '4px', marginTop: '12px' }}>
                    <div style={{ color: C.RED, fontSize: '14px', fontWeight: 'bold', marginBottom: '8px' }}>REJECTION REASON:</div>
                    <textarea
                      value={rejectionReason}
                      onChange={(e) => setRejectionReason(e.target.value)}
                      placeholder="Enter reason for rejection..."
                      style={{ width: '100%', padding: '10px', backgroundColor: C.DARK_BG, border: `2px solid ${C.GRAY}`, color: C.WHITE, fontSize: '13px', fontFamily: 'Consolas, monospace', minHeight: '80px', marginBottom: '12px' }}
                    />
                    <div style={{ display: 'flex', gap: '8px' }}>
                      <button onClick={() => handleReject(dataset.id)} disabled={!rejectionReason.trim()} style={{ padding: '8px 16px', backgroundColor: C.RED, border: 'none', color: C.WHITE, fontSize: '11px', fontWeight: 'bold', cursor: rejectionReason.trim() ? 'pointer' : 'not-allowed', fontFamily: 'Consolas, monospace' }}>
                        ❌ CONFIRM REJECT
                      </button>
                      <button onClick={() => { setRejectingDatasetId(null); setRejectionReason(''); }} style={{ padding: '8px 16px', backgroundColor: C.DARK_BG, border: `2px solid ${C.GRAY}`, color: C.WHITE, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                        CANCEL
                      </button>
                    </div>
                  </div>
                ) : (
                  <div style={{ display: 'flex', gap: '8px' }}>
                    <button onClick={() => handleApprove(dataset.id, dataset.title)} style={{ padding: '10px 20px', backgroundColor: C.DARK_BG, border: `2px solid ${C.GREEN}`, color: C.GREEN, fontSize: '12px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                      ✅ APPROVE
                    </button>
                    <button onClick={() => setRejectingDatasetId(dataset.id)} style={{ padding: '10px 20px', backgroundColor: C.DARK_BG, border: `2px solid ${C.RED}`, color: C.RED, fontSize: '12px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                      ❌ REJECT
                    </button>
                  </div>
                )}
              </div>
            ))
          )}
        </div>
      </div>
    </>
  );
};

// Admin Revenue Analytics Screen Component
const AdminRevenueScreen: React.FC<{ session: any; C: any }> = ({ session, C }) => {
  const [revenueData, setRevenueData] = useState<RevenueAnalytics | null>(null);
  const [isLoading, setIsLoading] = useState(false);

  useEffect(() => {
    const fetchRevenue = async () => {
      if (!session?.api_key) return;
      setIsLoading(true);
      try {
        const response = await MarketplaceApiService.getAdminRevenueAnalytics(session.api_key);
        if (response.success && response.data) {
          setRevenueData(response.data);
        }
      } catch (error) {
        console.error('Failed to fetch revenue analytics:', error);
      } finally {
        setIsLoading(false);
      }
    };
    fetchRevenue();
  }, [session?.api_key]);

  return (
    <>
      <div style={{ padding: '12px', backgroundColor: C.PANEL_BG, borderBottom: `2px solid ${C.GRAY}` }}>
        <div style={{ color: C.ORANGE, fontSize: '20px', fontWeight: 'bold' }}>💵 REVENUE ANALYTICS</div>
      </div>
      <div className="marketplace-scroll" style={{ flex: 1, overflowY: 'auto', backgroundColor: '#050505', padding: '20px' }}>
        <div style={{ maxWidth: '1200px', margin: '0 auto' }}>
          {isLoading ? (
            <div style={{ textAlign: 'center', padding: '40px', color: C.GRAY }}>Loading...</div>
          ) : !revenueData ? (
            <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, padding: '40px', textAlign: 'center' }}>
              <div style={{ color: C.GRAY, fontSize: '18px' }}>No revenue data available</div>
            </div>
          ) : (
            <>
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '16px', marginBottom: '24px' }}>
                <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GREEN}`, padding: '24px', textAlign: 'center' }}>
                  <div style={{ color: C.GREEN, fontSize: '48px', fontWeight: 'bold' }}>{revenueData.total_revenue || 0}</div>
                  <div style={{ color: C.WHITE, fontSize: '16px', marginTop: '8px' }}>Total Revenue (credits)</div>
                </div>
                <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.BLUE}`, padding: '24px', textAlign: 'center' }}>
                  <div style={{ color: C.BLUE, fontSize: '48px', fontWeight: 'bold' }}>{revenueData.total_transactions || 0}</div>
                  <div style={{ color: C.WHITE, fontSize: '16px', marginTop: '8px' }}>Total Transactions</div>
                </div>
              </div>

              {revenueData.revenue_by_tier && Object.keys(revenueData.revenue_by_tier).length > 0 && (
                <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, padding: '20px', marginBottom: '24px' }}>
                  <div style={{ color: C.ORANGE, fontSize: '16px', fontWeight: 'bold', marginBottom: '16px' }}>REVENUE BY TIER</div>
                  {Object.entries(revenueData.revenue_by_tier).map(([tier, revenue]: [string, any]) => (
                    <div key={tier} style={{ borderBottom: `1px solid ${C.GRAY}`, padding: '12px 0', display: 'flex', justifyContent: 'space-between' }}>
                      <div style={{ color: C.WHITE, fontSize: '14px', textTransform: 'uppercase' }}>{tier}</div>
                      <div style={{ color: C.GREEN, fontSize: '14px', fontWeight: 'bold' }}>{revenue} credits</div>
                    </div>
                  ))}
                </div>
              )}

              {revenueData.top_datasets && revenueData.top_datasets.length > 0 && (
                <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, padding: '20px' }}>
                  <div style={{ color: C.ORANGE, fontSize: '16px', fontWeight: 'bold', marginBottom: '16px' }}>TOP PERFORMING DATASETS</div>
                  {revenueData.top_datasets.map((dataset, idx) => (
                    <div key={dataset.id} style={{ borderBottom: `1px solid ${C.GRAY}`, padding: '12px 0', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                      <div style={{ flex: 1 }}>
                        <div style={{ color: C.WHITE, fontSize: '14px', fontWeight: 'bold', marginBottom: '4px' }}>
                          #{idx + 1} {dataset.title}
                        </div>
                        <div style={{ color: C.GRAY, fontSize: '12px' }}>{dataset.downloads} downloads</div>
                      </div>
                      <div style={{ color: C.GREEN, fontSize: '18px', fontWeight: 'bold', marginLeft: '20px' }}>
                        {dataset.revenue} credits
                      </div>
                    </div>
                  ))}
                </div>
              )}
            </>
          )}
        </div>
      </div>
    </>
  );
};

export default MarketplaceTab;