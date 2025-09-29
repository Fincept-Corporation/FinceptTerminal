import React, { useState, useEffect } from 'react';
import { Plus, Trash2, Star, TrendingUp, TrendingDown, AlertTriangle, Settings } from 'lucide-react';

interface WatchlistItem {
  id: string;
  symbol: string;
  name: string;
  price: string;
  change: string;
  changePercent: string;
  volume: string;
  marketCap: string;
  pe: string;
  isPositive: boolean;
  isFavorite: boolean;
  alerts: number;
}

interface Watchlist {
  id: string;
  name: string;
  items: WatchlistItem[];
  color: string;
}

const WatchlistTab: React.FC = () => {
  const [activeWatchlist, setActiveWatchlist] = useState('portfolio');
  const [watchlists, setWatchlists] = useState<Record<string, Watchlist>>({
    portfolio: {
      id: 'portfolio',
      name: 'My Portfolio',
      color: 'text-green-400',
      items: [
        {
          id: '1',
          symbol: 'AAPL',
          name: 'Apple Inc.',
          price: '$175.43',
          change: '+2.34',
          changePercent: '+1.35%',
          volume: '45.2M',
          marketCap: '2.7T',
          pe: '28.4',
          isPositive: true,
          isFavorite: true,
          alerts: 2
        },
        {
          id: '2',
          symbol: 'MSFT',
          name: 'Microsoft Corporation',
          price: '$334.89',
          change: '+4.12',
          changePercent: '+1.24%',
          volume: '28.7M',
          marketCap: '2.5T',
          pe: '32.1',
          isPositive: true,
          isFavorite: true,
          alerts: 0
        },
        {
          id: '3',
          symbol: 'GOOGL',
          name: 'Alphabet Inc.',
          price: '$134.56',
          change: '-1.23',
          changePercent: '-0.91%',
          volume: '32.1M',
          marketCap: '1.7T',
          pe: '24.8',
          isPositive: false,
          isFavorite: false,
          alerts: 1
        }
      ]
    },
    watchlist1: {
      id: 'watchlist1',
      name: 'Tech Giants',
      color: 'text-blue-400',
      items: [
        {
          id: '4',
          symbol: 'TSLA',
          name: 'Tesla Inc.',
          price: '$201.78',
          change: '-5.67',
          changePercent: '-2.73%',
          volume: '67.4M',
          marketCap: '641B',
          pe: '45.6',
          isPositive: false,
          isFavorite: true,
          alerts: 3
        },
        {
          id: '5',
          symbol: 'NVDA',
          name: 'NVIDIA Corporation',
          price: '$478.23',
          change: '+12.45',
          changePercent: '+2.67%',
          volume: '52.3M',
          marketCap: '1.2T',
          pe: '78.9',
          isPositive: true,
          isFavorite: true,
          alerts: 0
        },
        {
          id: '6',
          symbol: 'AMZN',
          name: 'Amazon.com Inc.',
          price: '$145.32',
          change: '+1.89',
          changePercent: '+1.32%',
          volume: '41.8M',
          marketCap: '1.5T',
          pe: '58.3',
          isPositive: true,
          isFavorite: false,
          alerts: 1
        }
      ]
    },
    watchlist2: {
      id: 'watchlist2',
      name: 'Energy Sector',
      color: 'text-yellow-400',
      items: [
        {
          id: '7',
          symbol: 'XOM',
          name: 'Exxon Mobil Corporation',
          price: '$108.45',
          change: '+2.34',
          changePercent: '+2.20%',
          volume: '18.5M',
          marketCap: '458B',
          pe: '14.2',
          isPositive: true,
          isFavorite: false,
          alerts: 0
        },
        {
          id: '8',
          symbol: 'CVX',
          name: 'Chevron Corporation',
          price: '$159.78',
          change: '+1.23',
          changePercent: '+0.78%',
          volume: '12.3M',
          marketCap: '298B',
          pe: '13.8',
          isPositive: true,
          isFavorite: false,
          alerts: 1
        }
      ]
    }
  });

  const [sortBy, setSortBy] = useState('symbol');
  const [sortOrder, setSortOrder] = useState<'asc' | 'desc'>('asc');
  const [lastUpdated, setLastUpdated] = useState(new Date());

  useEffect(() => {
    const timer = setInterval(() => {
      setLastUpdated(new Date());
    }, 5000);

    return () => clearInterval(timer);
  }, []);

  const currentWatchlist = watchlists[activeWatchlist];

  const toggleFavorite = (itemId: string) => {
    setWatchlists(prev => ({
      ...prev,
      [activeWatchlist]: {
        ...prev[activeWatchlist],
        items: prev[activeWatchlist].items.map(item =>
          item.id === itemId ? { ...item, isFavorite: !item.isFavorite } : item
        )
      }
    }));
  };

  const removeFromWatchlist = (itemId: string) => {
    setWatchlists(prev => ({
      ...prev,
      [activeWatchlist]: {
        ...prev[activeWatchlist],
        items: prev[activeWatchlist].items.filter(item => item.id !== itemId)
      }
    }));
  };

  const sortedItems = [...currentWatchlist.items].sort((a, b) => {
    let aValue = a[sortBy as keyof WatchlistItem];
    let bValue = b[sortBy as keyof WatchlistItem];

    if (sortBy === 'price' || sortBy === 'change') {
      aValue = parseFloat(aValue.toString().replace(/[$+,%]/g, ''));
      bValue = parseFloat(bValue.toString().replace(/[$+,%]/g, ''));
    }

    const comparison = aValue < bValue ? -1 : aValue > bValue ? 1 : 0;
    return sortOrder === 'asc' ? comparison : -comparison;
  });

  const handleSort = (column: string) => {
    if (sortBy === column) {
      setSortOrder(sortOrder === 'asc' ? 'desc' : 'asc');
    } else {
      setSortBy(column);
      setSortOrder('asc');
    }
  };

  return (
    <div className="h-full bg-black text-white flex flex-col">
      {/* Watchlist Navigation */}
      <div className="bg-gray-900 border-b border-gray-700 p-2">
        <div className="flex items-center justify-between">
          <div className="flex gap-2">
            {Object.values(watchlists).map((watchlist) => (
              <button
                key={watchlist.id}
                onClick={() => setActiveWatchlist(watchlist.id)}
                className={`px-4 py-2 text-sm font-medium rounded transition-colors ${
                  activeWatchlist === watchlist.id
                    ? 'bg-orange-600 text-white'
                    : 'bg-gray-800 text-gray-300 hover:bg-gray-700'
                }`}
              >
                <span className={watchlist.color}>●</span>
                <span className="ml-2">{watchlist.name}</span>
                <span className="ml-1 text-xs text-gray-400">({watchlist.items.length})</span>
              </button>
            ))}
          </div>
          <button className="flex items-center gap-2 px-3 py-2 bg-orange-600 text-white text-sm font-medium rounded hover:bg-orange-700 transition-colors">
            <Plus size={16} />
            <span>Add Symbol</span>
          </button>
        </div>
      </div>

      {/* Header */}
      <div className="bg-gray-800 border-b border-gray-700 p-4">
        <div className="flex justify-between items-center">
          <div>
            <h1 className="text-xl font-bold text-orange-500">
              {currentWatchlist.name.toUpperCase()}
            </h1>
            <div className="text-sm text-gray-400 mt-1">
              {currentWatchlist.items.length} securities •
              {currentWatchlist.items.filter(item => item.isPositive).length} gaining •
              {currentWatchlist.items.filter(item => !item.isPositive).length} declining
            </div>
          </div>
          <div className="flex items-center gap-4 text-sm text-gray-400">
            <span>Last Updated: {lastUpdated.toLocaleTimeString()}</span>
            <div className="text-green-400">● LIVE</div>
          </div>
        </div>
      </div>

      {/* Controls */}
      <div className="bg-gray-900 border-b border-gray-700 p-3">
        <div className="flex justify-between items-center">
          <div className="flex items-center gap-4">
            <span className="text-sm text-gray-400">Sort by:</span>
            <select
              value={sortBy}
              onChange={(e) => setSortBy(e.target.value)}
              className="bg-gray-800 border border-gray-600 rounded px-2 py-1 text-sm text-white"
            >
              <option value="symbol">Symbol</option>
              <option value="price">Price</option>
              <option value="change">Change</option>
              <option value="changePercent">% Change</option>
              <option value="volume">Volume</option>
            </select>
            <button
              onClick={() => setSortOrder(sortOrder === 'asc' ? 'desc' : 'asc')}
              className="text-sm text-orange-400 hover:text-orange-300"
            >
              {sortOrder === 'asc' ? '↑' : '↓'}
            </button>
          </div>
          <div className="flex items-center gap-2">
            <button className="flex items-center gap-1 px-2 py-1 bg-gray-800 text-sm text-gray-300 rounded hover:bg-gray-700">
              <Settings size={14} />
              <span>Settings</span>
            </button>
          </div>
        </div>
      </div>

      {/* Table Header */}
      <div className="bg-gray-900 border-b border-gray-700 p-3">
        <div className="grid grid-cols-9 gap-4 text-sm font-semibold text-gray-300">
          <div className="cursor-pointer hover:text-white" onClick={() => handleSort('symbol')}>
            SYMBOL
          </div>
          <div>NAME</div>
          <div className="text-right cursor-pointer hover:text-white" onClick={() => handleSort('price')}>
            PRICE
          </div>
          <div className="text-right cursor-pointer hover:text-white" onClick={() => handleSort('change')}>
            CHANGE
          </div>
          <div className="text-right cursor-pointer hover:text-white" onClick={() => handleSort('changePercent')}>
            % CHANGE
          </div>
          <div className="text-right cursor-pointer hover:text-white" onClick={() => handleSort('volume')}>
            VOLUME
          </div>
          <div className="text-right">MKT CAP</div>
          <div className="text-right">P/E</div>
          <div className="text-center">ACTIONS</div>
        </div>
      </div>

      {/* Watchlist Items */}
      <div className="flex-1 overflow-y-auto">
        <div className="p-3 space-y-1">
          {sortedItems.map((item) => (
            <div
              key={item.id}
              className="grid grid-cols-9 gap-4 py-3 px-2 rounded hover:bg-gray-900 transition-colors border-b border-gray-800"
            >
              <div className="flex items-center gap-2">
                <span className="font-semibold text-white">{item.symbol}</span>
                {item.alerts > 0 && (
                  <div className="flex items-center gap-1">
                    <AlertTriangle size={14} className="text-yellow-400" />
                    <span className="text-xs text-yellow-400">{item.alerts}</span>
                  </div>
                )}
              </div>
              <div className="text-sm text-gray-300 truncate">{item.name}</div>
              <div className="text-right font-mono">{item.price}</div>
              <div className={`text-right font-mono ${item.isPositive ? 'text-green-400' : 'text-red-400'}`}>
                {item.change}
              </div>
              <div className={`text-right font-mono flex items-center justify-end gap-1 ${item.isPositive ? 'text-green-400' : 'text-red-400'}`}>
                {item.isPositive ? <TrendingUp size={14} /> : <TrendingDown size={14} />}
                {item.changePercent}
              </div>
              <div className="text-right text-gray-400">{item.volume}</div>
              <div className="text-right text-gray-400">{item.marketCap}</div>
              <div className="text-right text-gray-400">{item.pe}</div>
              <div className="flex justify-center items-center gap-2">
                <button
                  onClick={() => toggleFavorite(item.id)}
                  className={`p-1 rounded transition-colors ${
                    item.isFavorite ? 'text-yellow-400 hover:text-yellow-300' : 'text-gray-400 hover:text-yellow-400'
                  }`}
                >
                  <Star size={14} fill={item.isFavorite ? 'currentColor' : 'none'} />
                </button>
                <button
                  onClick={() => removeFromWatchlist(item.id)}
                  className="p-1 text-gray-400 hover:text-red-400 transition-colors"
                >
                  <Trash2 size={14} />
                </button>
              </div>
            </div>
          ))}
        </div>
      </div>

      {/* Footer */}
      <div className="bg-gray-900 border-t border-gray-700 p-3">
        <div className="flex justify-between items-center text-sm">
          <div className="text-gray-400">
            Portfolio Value: $1,234,567.89 | Daily P&L: +$23,456.78 (+1.89%)
          </div>
          <div className="text-gray-400">
            Auto-refresh: 5s
          </div>
        </div>
      </div>
    </div>
  );
};

export default WatchlistTab;