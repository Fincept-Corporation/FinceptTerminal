import React, { useState, useEffect } from 'react';
import { useAuth } from '@/contexts/AuthContext';
import { MarketplaceApiService, Dataset } from '@/services/marketplaceApi';
import { useTerminalTheme } from '@/contexts/ThemeContext';

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
  const [currentScreen, setCurrentScreen] = useState<'BROWSE' | 'DETAILS' | 'CART' | 'UPLOAD' | 'MY_PURCHASES' | 'ANALYTICS' | 'ADMIN_STATS'>('BROWSE');
  const [selectedProduct, setSelectedProduct] = useState<MarketProduct | null>(null);
  const [selectedCategory, setSelectedCategory] = useState('ALL');
  const [searchQuery, setSearchQuery] = useState('');
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
        // Handle nested data structure
        const datasetsArray = (response.data as any).data?.datasets || (response.data as any).datasets || [];
        const totalPagesCount = (response.data as any).data?.pagination?.total_pages || (response.data as any).total_pages || 1;

        console.log('📦 Datasets found:', datasetsArray.length, datasetsArray);

        setDatasets(datasetsArray);
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
      const response = await MarketplaceApiService.getDatasetAnalytics(session.api_key);
      console.log('📊 My uploaded datasets analytics:', response);

      if (response.success && response.data) {
        const myDatasets = (response.data as any).data?.datasets || (response.data as any).datasets || [];
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

  const products: MarketProduct[] = datasets.map(convertToMarketProduct);

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
            <button onClick={() => setCurrentScreen('ANALYTICS')} style={{ padding: '6px 12px', backgroundColor: currentScreen === 'ANALYTICS' ? C.ORANGE : C.DARK_BG, border: `2px solid ${C.GRAY}`, color: currentScreen === 'ANALYTICS' ? C.DARK_BG : C.WHITE, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
              📊 ANALYTICS
            </button>
            {(session?.user_info as any)?.is_admin && (
              <button onClick={() => setCurrentScreen('ADMIN_STATS')} style={{ padding: '6px 12px', backgroundColor: currentScreen === 'ADMIN_STATS' ? C.ORANGE : C.DARK_BG, border: `2px solid ${C.GRAY}`, color: currentScreen === 'ADMIN_STATS' ? C.DARK_BG : C.WHITE, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
              🔧 ADMIN
            </button>
            )}
            <button onClick={() => setCurrentScreen('CART')} style={{ padding: '6px 12px', backgroundColor: currentScreen === 'CART' ? C.ORANGE : C.DARK_BG, border: `2px solid ${C.GRAY}`, color: currentScreen === 'CART' ? C.DARK_BG : C.WHITE, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
              🛒 CART {cartItems.length > 0 && `(${cartItems.length})`}
            </button>
          </div>
        </div>
      </div>

      {/* Content */}
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', minHeight: 0 }}>
        {currentScreen === 'BROWSE' && (
          <>
            {/* Search */}
            <div style={{ padding: '12px', backgroundColor: C.PANEL_BG, borderBottom: `2px solid ${C.GRAY}` }}>
              <div style={{ display: 'flex', gap: '12px', marginBottom: '12px' }}>
                <input type="text" placeholder="Search products..." value={searchQuery} onChange={(e) => setSearchQuery(e.target.value)} style={{ flex: 1, padding: '10px 16px', backgroundColor: C.DARK_BG, border: `2px solid ${C.GRAY}`, color: C.WHITE, fontSize: '14px', fontFamily: 'Consolas, monospace' }} />
                <button style={{ padding: '10px 24px', backgroundColor: C.ORANGE, border: 'none', color: C.DARK_BG, fontSize: '14px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>SEARCH</button>
              </div>
              <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap' }}>
                {categories.map((cat) => (
                  <button key={cat.name} onClick={() => setSelectedCategory(cat.name)} style={{ padding: '8px 16px', backgroundColor: selectedCategory === cat.name ? C.ORANGE : C.DARK_BG, border: `2px solid ${selectedCategory === cat.name ? C.ORANGE : C.GRAY}`, color: selectedCategory === cat.name ? C.DARK_BG : C.WHITE, fontSize: '12px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                    {cat.name.toUpperCase()} ({cat.count})
                  </button>
                ))}
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
                  {filteredProducts.map((product) => (
                  <div key={product.id} style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, borderLeft: `6px solid ${getTypeColor(product.type)}`, padding: '16px' }}>
                    <div style={{ display: 'inline-block', backgroundColor: getTypeColor(product.type), color: C.DARK_BG, padding: '4px 10px', fontSize: '10px', fontWeight: 'bold', marginBottom: '8px' }}>
                      {product.type}
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
                  ))}
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
        {currentScreen === 'ANALYTICS' && <AnalyticsScreen session={session} C={C} />}
        {currentScreen === 'ADMIN_STATS' && <AdminStatsScreen session={session} C={C} />}
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

export default MarketplaceTab;