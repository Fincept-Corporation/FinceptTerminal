/**
 * Node Palette Component
 *
 * Displays available nodes from the NodeRegistry that can be added to the workflow.
 * Organized by category with search functionality.
 *
 * Features:
 * - Search nodes by name
 * - Filter by category
 * - Drag and drop to canvas
 * - Node descriptions on hover
 * - Category-based organization
 */

import React, { useState, useMemo } from 'react';
import { NodeRegistry } from '@/services/nodeSystem';
import type { INodeTypeDescription } from '@/services/nodeSystem';
import {
  Search,
  Database,
  Workflow,
  FileText,
  Code,
  GitBranch,
  Filter,
  Settings,
  ChevronRight,
  ChevronDown,
} from 'lucide-react';

interface NodePaletteProps {
  onNodeAdd: (nodeType: string) => void;
  onDragStart?: (nodeType: string, event: React.DragEvent) => void;
}

export const NodePalette: React.FC<NodePaletteProps> = ({
  onNodeAdd,
  onDragStart,
}) => {
  const [searchQuery, setSearchQuery] = useState('');
  const [expandedCategories, setExpandedCategories] = useState<Set<string>>(
    new Set(['Core'])
  );

  // Get all registered nodes
  const allNodes = useMemo(() => {
    return NodeRegistry.getAllNodes();
  }, []);

  // Group nodes by category
  const nodesByCategory = useMemo(() => {
    const grouped = new Map<string, INodeTypeDescription[]>();

    for (const node of allNodes) {
      const category = node.group || ['Uncategorized'];
      const categoryName = Array.isArray(category) ? category[0] : category;

      if (!grouped.has(categoryName)) {
        grouped.set(categoryName, []);
      }

      grouped.get(categoryName)!.push(node);
    }

    // Sort each category alphabetically
    for (const nodes of grouped.values()) {
      nodes.sort((a, b) => a.displayName.localeCompare(b.displayName));
    }

    return grouped;
  }, [allNodes]);

  // Filter nodes based on search query
  const filteredCategories = useMemo(() => {
    if (!searchQuery.trim()) {
      return nodesByCategory;
    }

    const query = searchQuery.toLowerCase();
    const filtered = new Map<string, INodeTypeDescription[]>();

    for (const [category, nodes] of nodesByCategory.entries()) {
      const matchingNodes = nodes.filter(
        (node) =>
          node.displayName.toLowerCase().includes(query) ||
          node.name.toLowerCase().includes(query) ||
          node.description?.toLowerCase().includes(query)
      );

      if (matchingNodes.length > 0) {
        filtered.set(category, matchingNodes);
      }
    }

    return filtered;
  }, [nodesByCategory, searchQuery]);

  // Toggle category expansion
  const toggleCategory = (category: string) => {
    const newExpanded = new Set(expandedCategories);
    if (newExpanded.has(category)) {
      newExpanded.delete(category);
    } else {
      newExpanded.add(category);
    }
    setExpandedCategories(newExpanded);
  };

  // Get category icon
  const getCategoryIcon = (category: string) => {
    switch (category.toLowerCase()) {
      case 'core':
        return <Workflow size={16} />;
      case 'data':
        return <Database size={16} />;
      case 'transform':
        return <Filter size={16} />;
      case 'control':
        return <GitBranch size={16} />;
      case 'output':
        return <FileText size={16} />;
      case 'developer':
        return <Code size={16} />;
      default:
        return <Settings size={16} />;
    }
  };

  // Get node icon color
  const getNodeColor = (node: INodeTypeDescription) => {
    return node.defaults?.color || '#888888';
  };

  // Handle drag start
  const handleDragStart = (nodeType: string, event: React.DragEvent) => {
    event.dataTransfer.setData('application/reactflow', nodeType);
    event.dataTransfer.effectAllowed = 'move';
    onDragStart?.(nodeType, event);
  };

  return (
    <div className="w-80 bg-gray-900 border-r border-gray-800 flex flex-col h-full">
      {/* Palette Header */}
      <div className="p-4 border-b border-gray-800">
        <h3 className="text-sm font-semibold text-gray-200 mb-3 flex items-center gap-2">
          <Database size={16} className="text-blue-400" />
          Node Palette
        </h3>

        {/* Search Input */}
        <div className="relative">
          <Search size={16} className="absolute left-3 top-1/2 -translate-y-1/2 text-gray-500" />
          <input
            type="text"
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            placeholder="Search nodes..."
            className="w-full pl-10 pr-3 py-2 bg-gray-800 border border-gray-700 rounded text-sm text-gray-200 placeholder-gray-500 focus:border-blue-500 focus:outline-none"
          />
        </div>

        <div className="text-xs text-gray-500 mt-2">
          {allNodes.length} nodes available
        </div>
      </div>

      {/* Node Categories */}
      <div className="flex-1 overflow-y-auto">
        {Array.from(filteredCategories.entries()).map(([category, nodes]) => (
          <div key={category} className="border-b border-gray-800 last:border-0">
            {/* Category Header */}
            <button
              onClick={() => toggleCategory(category)}
              className="w-full px-4 py-3 flex items-center justify-between hover:bg-gray-800/50 transition-colors"
            >
              <div className="flex items-center gap-2 text-sm text-gray-300">
                {getCategoryIcon(category)}
                <span className="font-medium">{category}</span>
                <span className="text-xs text-gray-600">({nodes.length})</span>
              </div>
              {expandedCategories.has(category) ? (
                <ChevronDown size={16} className="text-gray-500" />
              ) : (
                <ChevronRight size={16} className="text-gray-500" />
              )}
            </button>

            {/* Category Nodes */}
            {expandedCategories.has(category) && (
              <div className="pb-2">
                {nodes.map((node) => (
                  <div
                    key={node.name}
                    draggable
                    onDragStart={(e) => handleDragStart(node.name, e)}
                    onClick={() => onNodeAdd(node.name)}
                    className="mx-2 mb-1 p-3 bg-gray-800/30 hover:bg-gray-800 rounded cursor-pointer transition-colors group"
                    title={node.description}
                  >
                    <div className="flex items-start gap-3">
                      {/* Node Icon */}
                      <div
                        className="w-8 h-8 rounded flex items-center justify-center flex-shrink-0 mt-0.5"
                        style={{
                          background: `${getNodeColor(node)}20`,
                          border: `1px solid ${getNodeColor(node)}40`,
                        }}
                      >
                        <div
                          className="text-xs font-bold"
                          style={{ color: getNodeColor(node) }}
                        >
                          {node.displayName[0]}
                        </div>
                      </div>

                      {/* Node Info */}
                      <div className="flex-1 min-w-0">
                        <div
                          className="text-sm font-medium mb-0.5"
                          style={{ color: getNodeColor(node) }}
                        >
                          {node.displayName}
                        </div>

                        {node.description && (
                          <div className="text-xs text-gray-500 line-clamp-2">
                            {node.description}
                          </div>
                        )}

                        {node.version && (
                          <div className="text-xs text-gray-600 mt-1">
                            v{node.version}
                          </div>
                        )}
                      </div>
                    </div>
                  </div>
                ))}
              </div>
            )}
          </div>
        ))}

        {/* No Results */}
        {filteredCategories.size === 0 && (
          <div className="p-8 text-center">
            <Search size={32} className="mx-auto mb-2 text-gray-600" />
            <p className="text-sm text-gray-500">No nodes found</p>
            <p className="text-xs text-gray-600 mt-1">
              Try a different search term
            </p>
          </div>
        )}
      </div>

      {/* Palette Footer */}
      <div className="p-4 border-t border-gray-800 bg-gray-900">
        <div className="text-xs text-gray-500 text-center">
          Drag nodes to the canvas or click to add
        </div>
      </div>
    </div>
  );
};

export default NodePalette;
