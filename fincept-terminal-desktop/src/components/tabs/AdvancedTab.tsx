import React, { useState, useEffect } from 'react';
import { TrendingUp, Calculator, BarChart3, PieChart, Activity, Database, Settings, Filter } from 'lucide-react';

interface AnalyticsData {
  metric: string;
  value: string;
  change: string;
  trend: 'up' | 'down' | 'neutral';
}

interface ToolSection {
  id: string;
  title: string;
  icon: React.ReactNode;
  description: string;
  status: 'active' | 'inactive' | 'processing';
}

const AdvancedTab: React.FC = () => {
  const [activeSection, setActiveSection] = useState('analytics');
  const [analyticsData, setAnalyticsData] = useState<AnalyticsData[]>([
    { metric: 'Portfolio Value', value: '$1,234,567.89', change: '+2.34%', trend: 'up' },
    { metric: 'Daily P&L', value: '+$23,456.78', change: '+1.89%', trend: 'up' },
    { metric: 'Market Exposure', value: '78.45%', change: '-0.23%', trend: 'down' },
    { metric: 'Beta Coefficient', value: '1.23', change: '+0.05', trend: 'up' },
    { metric: 'Sharpe Ratio', value: '1.67', change: '+0.12', trend: 'up' },
    { metric: 'Max Drawdown', value: '-8.45%', change: '+1.23%', trend: 'up' },
    { metric: 'Win Rate', value: '67.8%', change: '+2.1%', trend: 'up' },
    { metric: 'Average Trade', value: '$1,234.56', change: '-$45.23', trend: 'down' }
  ]);

  const [tools, setTools] = useState<ToolSection[]>([
    {
      id: 'risk-analysis',
      title: 'Risk Analysis Engine',
      icon: <Activity size={20} />,
      description: 'Advanced portfolio risk assessment and VaR calculations',
      status: 'active'
    },
    {
      id: 'backtesting',
      title: 'Strategy Backtesting',
      icon: <BarChart3 size={20} />,
      description: 'Historical strategy performance analysis and optimization',
      status: 'inactive'
    },
    {
      id: 'correlation',
      title: 'Correlation Matrix',
      icon: <PieChart size={20} />,
      description: 'Asset correlation analysis and diversification metrics',
      status: 'processing'
    },
    {
      id: 'monte-carlo',
      title: 'Monte Carlo Simulation',
      icon: <TrendingUp size={20} />,
      description: 'Probabilistic portfolio outcome modeling',
      status: 'inactive'
    },
    {
      id: 'options-pricing',
      title: 'Options Pricing Models',
      icon: <Calculator size={20} />,
      description: 'Black-Scholes and advanced options valuation',
      status: 'active'
    },
    {
      id: 'data-mining',
      title: 'Market Data Mining',
      icon: <Database size={20} />,
      description: 'Advanced pattern recognition and signal detection',
      status: 'processing'
    }
  ]);

  const sections = [
    { key: 'analytics', label: 'ANALYTICS', icon: <BarChart3 size={16} /> },
    { key: 'tools', label: 'TOOLS', icon: <Settings size={16} /> },
    { key: 'models', label: 'MODELS', icon: <Calculator size={16} /> },
    { key: 'signals', label: 'SIGNALS', icon: <Activity size={16} /> }
  ];

  const [lastUpdated, setLastUpdated] = useState(new Date());

  useEffect(() => {
    const timer = setInterval(() => {
      setLastUpdated(new Date());
    }, 10000);

    return () => clearInterval(timer);
  }, []);

  const getTrendColor = (trend: string) => {
    switch (trend) {
      case 'up': return 'text-green-400';
      case 'down': return 'text-red-400';
      default: return 'text-gray-400';
    }
  };

  const getTrendIcon = (trend: string) => {
    switch (trend) {
      case 'up': return '↗';
      case 'down': return '↘';
      default: return '→';
    }
  };

  const getStatusColor = (status: string) => {
    switch (status) {
      case 'active': return 'text-green-400';
      case 'processing': return 'text-yellow-400';
      default: return 'text-gray-400';
    }
  };

  const getStatusIndicator = (status: string) => {
    switch (status) {
      case 'active': return '●';
      case 'processing': return '◐';
      default: return '○';
    }
  };

  const renderAnalytics = () => (
    <div className="grid grid-cols-1 lg:grid-cols-2 xl:grid-cols-4 gap-4 p-4">
      {analyticsData.map((data, index) => (
        <div key={index} className="bg-gray-900 border border-gray-700 rounded-lg p-4">
          <div className="flex justify-between items-start mb-2">
            <h3 className="text-sm font-medium text-gray-300">{data.metric}</h3>
            <span className={`text-sm ${getTrendColor(data.trend)}`}>
              {getTrendIcon(data.trend)}
            </span>
          </div>
          <div className="text-xl font-bold text-white mb-1">{data.value}</div>
          <div className={`text-sm ${getTrendColor(data.trend)}`}>
            {data.change}
          </div>
        </div>
      ))}
    </div>
  );

  const renderTools = () => (
    <div className="p-4">
      <div className="grid grid-cols-1 lg:grid-cols-2 gap-4">
        {tools.map((tool) => (
          <div key={tool.id} className="bg-gray-900 border border-gray-700 rounded-lg p-4 hover:bg-gray-800 transition-colors">
            <div className="flex items-start justify-between mb-3">
              <div className="flex items-center gap-3">
                <div className="text-orange-400">{tool.icon}</div>
                <h3 className="text-lg font-semibold text-white">{tool.title}</h3>
              </div>
              <div className="flex items-center gap-1">
                <span className={`text-sm ${getStatusColor(tool.status)}`}>
                  {getStatusIndicator(tool.status)}
                </span>
                <span className={`text-xs uppercase ${getStatusColor(tool.status)}`}>
                  {tool.status}
                </span>
              </div>
            </div>
            <p className="text-gray-300 text-sm mb-4">{tool.description}</p>
            <button className="w-full bg-orange-600 hover:bg-orange-700 text-white py-2 px-4 rounded text-sm font-medium transition-colors">
              {tool.status === 'active' ? 'Open Tool' : tool.status === 'processing' ? 'View Progress' : 'Activate'}
            </button>
          </div>
        ))}
      </div>
    </div>
  );

  const renderModels = () => (
    <div className="p-4">
      <div className="bg-gray-900 border border-gray-700 rounded-lg p-6">
        <h3 className="text-lg font-bold text-orange-500 mb-4">FINANCIAL MODELS</h3>
        <div className="space-y-4">
          <div className="border border-gray-700 rounded p-4">
            <h4 className="font-semibold text-white mb-2">Black-Scholes Option Pricing</h4>
            <div className="grid grid-cols-2 gap-4 text-sm">
              <div>
                <span className="text-gray-400">Spot Price: </span>
                <span className="text-white">$150.00</span>
              </div>
              <div>
                <span className="text-gray-400">Strike Price: </span>
                <span className="text-white">$155.00</span>
              </div>
              <div>
                <span className="text-gray-400">Time to Expiry: </span>
                <span className="text-white">30 days</span>
              </div>
              <div>
                <span className="text-gray-400">Call Option Value: </span>
                <span className="text-green-400">$3.45</span>
              </div>
            </div>
          </div>
          <div className="border border-gray-700 rounded p-4">
            <h4 className="font-semibold text-white mb-2">CAPM Analysis</h4>
            <div className="grid grid-cols-2 gap-4 text-sm">
              <div>
                <span className="text-gray-400">Risk-Free Rate: </span>
                <span className="text-white">4.5%</span>
              </div>
              <div>
                <span className="text-gray-400">Market Return: </span>
                <span className="text-white">12.3%</span>
              </div>
              <div>
                <span className="text-gray-400">Beta: </span>
                <span className="text-white">1.23</span>
              </div>
              <div>
                <span className="text-gray-400">Expected Return: </span>
                <span className="text-green-400">14.1%</span>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );

  const renderSignals = () => (
    <div className="p-4">
      <div className="bg-gray-900 border border-gray-700 rounded-lg p-6">
        <h3 className="text-lg font-bold text-orange-500 mb-4">TRADING SIGNALS</h3>
        <div className="space-y-3">
          {[
            { asset: 'AAPL', signal: 'BUY', strength: 'Strong', price: '$175.43', target: '$185.00' },
            { asset: 'TSLA', signal: 'SELL', strength: 'Moderate', price: '$201.78', target: '$190.00' },
            { asset: 'MSFT', signal: 'HOLD', strength: 'Weak', price: '$334.89', target: '$340.00' },
            { asset: 'GOOGL', signal: 'BUY', strength: 'Strong', price: '$134.56', target: '$145.00' }
          ].map((signal, index) => (
            <div key={index} className="flex justify-between items-center p-3 border border-gray-700 rounded">
              <div className="flex items-center gap-4">
                <span className="font-semibold text-white">{signal.asset}</span>
                <span className={`px-2 py-1 rounded text-xs font-medium ${
                  signal.signal === 'BUY' ? 'bg-green-900 text-green-400' :
                  signal.signal === 'SELL' ? 'bg-red-900 text-red-400' :
                  'bg-gray-800 text-gray-400'
                }`}>
                  {signal.signal}
                </span>
                <span className="text-sm text-gray-400">{signal.strength}</span>
              </div>
              <div className="text-right">
                <div className="text-sm text-white">{signal.price}</div>
                <div className="text-xs text-gray-400">Target: {signal.target}</div>
              </div>
            </div>
          ))}
        </div>
      </div>
    </div>
  );

  const renderContent = () => {
    switch (activeSection) {
      case 'analytics': return renderAnalytics();
      case 'tools': return renderTools();
      case 'models': return renderModels();
      case 'signals': return renderSignals();
      default: return renderAnalytics();
    }
  };

  return (
    <div className="h-full bg-black text-white flex flex-col">
      {/* Section Navigation */}
      <div className="bg-gray-900 border-b border-gray-700 p-2">
        <div className="flex gap-2">
          {sections.map((section) => (
            <button
              key={section.key}
              onClick={() => setActiveSection(section.key)}
              className={`flex items-center gap-2 px-4 py-2 text-sm font-medium rounded transition-colors ${
                activeSection === section.key
                  ? 'bg-orange-600 text-white'
                  : 'bg-gray-800 text-gray-300 hover:bg-gray-700'
              }`}
            >
              {section.icon}
              <span>{section.label}</span>
            </button>
          ))}
        </div>
      </div>

      {/* Header */}
      <div className="bg-gray-800 border-b border-gray-700 p-4">
        <div className="flex justify-between items-center">
          <h1 className="text-xl font-bold text-orange-500">
            ADVANCED ANALYTICS & TOOLS
          </h1>
          <div className="flex items-center gap-4 text-sm text-gray-400">
            <span>Last Updated: {lastUpdated.toLocaleTimeString()}</span>
            <div className="text-green-400">● REAL-TIME</div>
          </div>
        </div>
      </div>

      {/* Content */}
      <div className="flex-1 overflow-y-auto">
        {renderContent()}
      </div>

      {/* Footer */}
      <div className="bg-gray-900 border-t border-gray-700 p-3">
        <div className="flex justify-between items-center text-sm text-gray-400">
          <span>Advanced features require Professional subscription</span>
          <span>CPU Usage: 23% | Memory: 1.2GB</span>
        </div>
      </div>
    </div>
  );
};

export default AdvancedTab;