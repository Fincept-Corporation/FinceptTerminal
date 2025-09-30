import React, { useState, useEffect } from 'react';

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
  const [currentScreen, setCurrentScreen] = useState<'BROWSE' | 'DETAILS' | 'CART'>('BROWSE');
  const [selectedProduct, setSelectedProduct] = useState<MarketProduct | null>(null);
  const [selectedCategory, setSelectedCategory] = useState('ALL');
  const [searchQuery, setSearchQuery] = useState('');
  const [cartItems, setCartItems] = useState<string[]>([]);

  // Colors
  const C = {
    ORANGE: '#FFA500',
    WHITE: '#FFFFFF',
    RED: '#FF0000',
    GREEN: '#00C800',
    YELLOW: '#FFFF00',
    GRAY: '#787878',
    BLUE: '#6496FA',
    PURPLE: '#C864FF',
    CYAN: '#00FFFF',
    DARK_BG: '#000000',
    PANEL_BG: '#0a0a0a'
  };

  const products: MarketProduct[] = [
    {
      id: 'MKT001',
      type: 'DATA',
      name: 'Global Equity Real-Time Feed',
      vendor: 'Refinitiv Data Solutions',
      category: 'Market Data',
      price: 4999.00,
      pricingModel: 'MONTHLY',
      shortDescription: 'Real-time equity prices for 60,000+ global securities across 120+ exchanges',
      rating: 4.8,
      reviews: 2847,
      users: 12450,
      accuracy: '99.99%',
      latency: '<1ms',
      verified: true,
      trending: true,
      featured: true
    },
    {
      id: 'MKT002',
      type: 'ALGO',
      name: 'Quantum Alpha Strategy',
      vendor: 'QuantEdge Systems',
      category: 'Trading Algorithms',
      price: 15000.00,
      pricingModel: 'ANNUAL',
      shortDescription: 'ML-powered multi-factor equity strategy with proven 18.7% annual alpha',
      rating: 4.9,
      reviews: 1234,
      users: 3847,
      accuracy: '87.3%',
      latency: '<500ms',
      verified: true,
      trending: true,
      featured: true
    },
    {
      id: 'MKT003',
      type: 'API',
      name: 'Options Analytics Suite',
      vendor: 'DerivativeEdge Inc.',
      category: 'Derivatives',
      price: 0.05,
      pricingModel: 'USAGE_BASED',
      shortDescription: 'Complete options pricing, Greeks, and volatility analytics API',
      rating: 4.7,
      reviews: 3456,
      users: 18923,
      accuracy: '99.5%',
      latency: '<50ms',
      verified: true,
      trending: false,
      featured: true
    },
    {
      id: 'MKT004',
      type: 'MODEL',
      name: 'Credit Risk AI Model',
      vendor: 'RiskLab Analytics',
      category: 'Risk Management',
      price: 8500.00,
      pricingModel: 'MONTHLY',
      shortDescription: 'Deep learning model for corporate credit risk assessment',
      rating: 4.6,
      reviews: 892,
      users: 2341,
      accuracy: '91.2%',
      latency: 'N/A',
      verified: true,
      trending: false,
      featured: false
    },
    {
      id: 'MKT005',
      type: 'SIGNAL',
      name: 'Social Sentiment Signals',
      vendor: 'SentimentAlpha',
      category: 'Alternative Data',
      price: 2999.00,
      pricingModel: 'MONTHLY',
      shortDescription: 'Trading signals from social media sentiment analysis',
      rating: 4.4,
      reviews: 1678,
      users: 7823,
      accuracy: '73.8%',
      latency: '<1hr',
      verified: true,
      trending: true,
      featured: false
    },
    {
      id: 'MKT006',
      type: 'PLUGIN',
      name: 'Portfolio Risk Dashboard',
      vendor: 'Fincept Labs',
      category: 'Analytics',
      price: 0.00,
      pricingModel: 'FREE',
      shortDescription: 'Advanced portfolio risk analytics dashboard plugin',
      rating: 4.9,
      reviews: 5678,
      users: 28934,
      accuracy: '99.9%',
      latency: '<100ms',
      verified: true,
      trending: true,
      featured: true
    }
  ];

  const categories = [
    { name: 'ALL', count: products.length },
    { name: 'Market Data', count: 2 },
    { name: 'Trading Algorithms', count: 1 },
    { name: 'Derivatives', count: 1 },
    { name: 'Risk Management', count: 1 },
    { name: 'Alternative Data', count: 1 },
    { name: 'Analytics', count: 1 }
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
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <span style={{ color: C.ORANGE, fontWeight: 'bold', fontSize: '16px' }}>FINCEPT MARKETPLACE</span>
            <span style={{ color: C.GRAY }}>|</span>
            <span style={{ color: C.CYAN, fontSize: '12px' }}>{products.length} Products Available</span>
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <button onClick={() => setCurrentScreen('BROWSE')} style={{ padding: '6px 12px', backgroundColor: currentScreen === 'BROWSE' ? C.ORANGE : C.DARK_BG, border: `2px solid ${C.GRAY}`, color: currentScreen === 'BROWSE' ? C.DARK_BG : C.WHITE, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
              BROWSE
            </button>
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
                  products.filter(p => cartItems.includes(p.id)).map((product) => (
                    <div key={product.id} style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, borderLeft: `6px solid ${getTypeColor(product.type)}`, padding: '20px', marginBottom: '16px', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                      <div style={{ flex: 1 }}>
                        <div style={{ color: C.WHITE, fontSize: '18px', fontWeight: 'bold', marginBottom: '4px' }}>{product.name}</div>
                        <div style={{ color: C.GRAY, fontSize: '13px', marginBottom: '8px' }}>by {product.vendor}</div>
                      </div>
                      <div style={{ textAlign: 'right', marginLeft: '24px' }}>
                        <div style={{ color: C.GREEN, fontSize: '24px', fontWeight: 'bold', marginBottom: '8px' }}>
                          {product.pricingModel === 'FREE' ? 'FREE' : `$${product.price.toLocaleString()}`}
                        </div>
                        <button onClick={() => setCartItems(cartItems.filter(id => id !== product.id))} style={{ padding: '8px 16px', backgroundColor: C.DARK_BG, border: `2px solid ${C.RED}`, color: C.RED, fontSize: '11px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'Consolas, monospace' }}>
                          REMOVE
                        </button>
                      </div>
                    </div>
                  ))
                )}
              </div>
            </div>
          </>
        )}
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

export default MarketplaceTab;