import React, { useState } from 'react';
import { INVESTMENT_CATEGORIES, InvestmentCategory } from '@/services/alternativeInvestments/api/types';
import { BondsView } from './categories/BondsView';
import { RealEstateView } from './categories/RealEstateView';
import { HedgeFundsView } from './categories/HedgeFundsView';
import { CommoditiesView } from './categories/CommoditiesView';
import { PrivateCapitalView } from './categories/PrivateCapitalView';
import { AnnuitiesView } from './categories/AnnuitiesView';
import { StructuredProductsView } from './categories/StructuredProductsView';
import { InflationProtectedView } from './categories/InflationProtectedView';
import { StrategiesView } from './categories/StrategiesView';
import { CryptoView } from './categories/CryptoView';

/**
 * Alternative Investments Tab
 * Main hub for analyzing 22 different alternative investment types
 */
export const AlternativeInvestmentsTab: React.FC = () => {
  const [selectedCategory, setSelectedCategory] = useState<InvestmentCategory | null>(null);

  return (
    <div className="alternative-investments-tab h-full flex flex-col bg-gray-900 text-white">
      {/* Header */}
      <div className="border-b border-gray-800 p-6">
        <div className="flex items-center justify-between">
          <div>
            <h1 className="text-2xl font-bold mb-2">Alternative Investments Analytics</h1>
            <p className="text-gray-400">
              Comprehensive analysis of 22 alternative investment types powered by CFA-level analytics
            </p>
          </div>
          {selectedCategory && (
            <button
              onClick={() => setSelectedCategory(null)}
              className="px-4 py-2 bg-gray-800 hover:bg-gray-700 rounded-lg transition-colors"
            >
              ← Back to Categories
            </button>
          )}
        </div>
      </div>

      {/* Content */}
      <div className="flex-1 overflow-auto p-6">
        {!selectedCategory ? (
          <CategoryGrid
            categories={INVESTMENT_CATEGORIES}
            onSelectCategory={setSelectedCategory}
          />
        ) : (
          <CategoryDetailView category={selectedCategory} />
        )}
      </div>
    </div>
  );
};

/**
 * Category Grid - Shows all investment categories
 */
interface CategoryGridProps {
  categories: InvestmentCategory[];
  onSelectCategory: (category: InvestmentCategory) => void;
}

const CategoryGrid: React.FC<CategoryGridProps> = ({ categories, onSelectCategory }) => {
  return (
    <div className="space-y-6">
      {/* Overview Section */}
      <div className="bg-gray-800/50 rounded-lg p-6 border border-gray-700">
        <h2 className="text-xl font-semibold mb-3">📊 Investment Categories</h2>
        <p className="text-gray-300 mb-4">
          Select a category below to analyze specific alternative investments. Each analyzer provides
          comprehensive metrics, risk analysis, and expert recommendations.
        </p>
        <div className="grid grid-cols-2 gap-4 text-sm">
          <div className="bg-gray-900/50 p-3 rounded">
            <span className="text-green-400 font-semibold">✓ 22 Analyzers</span>
            <p className="text-gray-400 mt-1">Complete coverage of alternative investments</p>
          </div>
          <div className="bg-gray-900/50 p-3 rounded">
            <span className="text-blue-400 font-semibold">✓ CFA Standards</span>
            <p className="text-gray-400 mt-1">Professional-grade financial analysis</p>
          </div>
          <div className="bg-gray-900/50 p-3 rounded">
            <span className="text-purple-400 font-semibold">✓ Expert Verdicts</span>
            <p className="text-gray-400 mt-1">Clear recommendations (GOOD, BAD, FLAWED, UGLY)</p>
          </div>
          <div className="bg-gray-900/50 p-3 rounded">
            <span className="text-yellow-400 font-semibold">✓ Real-time Analysis</span>
            <p className="text-gray-400 mt-1">Instant results powered by Python analytics</p>
          </div>
        </div>
      </div>

      {/* Category Cards Grid */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4 gap-4">
        {categories.map((category) => (
          <CategoryCard
            key={category.id}
            category={category}
            onClick={() => onSelectCategory(category)}
          />
        ))}
      </div>
    </div>
  );
};

/**
 * Category Card - Individual category display
 */
interface CategoryCardProps {
  category: InvestmentCategory;
  onClick: () => void;
}

const CategoryCard: React.FC<CategoryCardProps> = ({ category, onClick }) => {
  const colorClasses: Record<string, string> = {
    blue: 'border-blue-500/50 hover:border-blue-400 hover:bg-blue-900/20',
    green: 'border-green-500/50 hover:border-green-400 hover:bg-green-900/20',
    purple: 'border-purple-500/50 hover:border-purple-400 hover:bg-purple-900/20',
    yellow: 'border-yellow-500/50 hover:border-yellow-400 hover:bg-yellow-900/20',
    red: 'border-red-500/50 hover:border-red-400 hover:bg-red-900/20',
    orange: 'border-orange-500/50 hover:border-orange-400 hover:bg-orange-900/20',
    pink: 'border-pink-500/50 hover:border-pink-400 hover:bg-pink-900/20',
    cyan: 'border-cyan-500/50 hover:border-cyan-400 hover:bg-cyan-900/20',
    indigo: 'border-indigo-500/50 hover:border-indigo-400 hover:bg-indigo-900/20',
    violet: 'border-violet-500/50 hover:border-violet-400 hover:bg-violet-900/20',
  };

  return (
    <button
      onClick={onClick}
      className={`
        bg-gray-800/50 rounded-lg p-5 border-2 transition-all
        cursor-pointer hover:shadow-lg hover:scale-[1.02]
        ${colorClasses[category.color] || 'border-gray-600 hover:border-gray-500'}
      `}
    >
      <div className="flex flex-col items-start h-full">
        {/* Icon/Emoji */}
        <div className="text-3xl mb-3">{getIconForCategory(category.id)}</div>

        {/* Title */}
        <h3 className="text-lg font-semibold mb-2 text-left">{category.name}</h3>

        {/* Description */}
        <p className="text-sm text-gray-400 mb-3 text-left flex-1">{category.description}</p>

        {/* Analyzer Count */}
        <div className="flex items-center gap-2 text-xs text-gray-500">
          <span className="px-2 py-1 bg-gray-700 rounded">
            {category.analyzers.length} analyzer{category.analyzers.length > 1 ? 's' : ''}
          </span>
        </div>
      </div>
    </button>
  );
};

/**
 * Category Detail View - Shows analyzers in selected category
 */
interface CategoryDetailViewProps {
  category: InvestmentCategory;
}

const CategoryDetailView: React.FC<CategoryDetailViewProps> = ({ category }) => {
  return (
    <div className="space-y-6">
      {/* Category Header */}
      <div className="bg-gray-800/50 rounded-lg p-6 border border-gray-700">
        <div className="flex items-center gap-4 mb-4">
          <div className="text-4xl">{getIconForCategory(category.id)}</div>
          <div>
            <h2 className="text-2xl font-bold">{category.name}</h2>
            <p className="text-gray-400">{category.description}</p>
          </div>
        </div>
        <div className="text-sm text-gray-500">
          {category.analyzers.length} analyzer{category.analyzers.length > 1 ? 's' : ''} available
        </div>
      </div>

      {/* Category-specific views */}
      {category.id === 'bonds' && <BondsView />}
      {category.id === 'real-estate' && <RealEstateView />}
      {category.id === 'hedge-funds' && <HedgeFundsView />}
      {category.id === 'commodities' && <CommoditiesView />}
      {category.id === 'private-capital' && <PrivateCapitalView />}
      {category.id === 'annuities' && <AnnuitiesView />}
      {category.id === 'structured' && <StructuredProductsView />}
      {category.id === 'inflation-protected' && <InflationProtectedView />}
      {category.id === 'strategies' && <StrategiesView />}
      {category.id === 'crypto' && <CryptoView />}
    </div>
  );
};

/**
 * Helper function to get icon/emoji for category
 */
function getIconForCategory(categoryId: string): string {
  const icons: Record<string, string> = {
    bonds: '📈',
    'real-estate': '🏢',
    'hedge-funds': '🛡️',
    commodities: '💎',
    'private-capital': '🔒',
    annuities: '📅',
    structured: '📦',
    'inflation-protected': '🛡️',
    strategies: '🎯',
    crypto: '₿',
  };

  return icons[categoryId] || '📊';
}

export default AlternativeInvestmentsTab;
