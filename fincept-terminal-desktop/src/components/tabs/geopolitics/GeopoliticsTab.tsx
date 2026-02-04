import React, { useReducer, useEffect, useMemo, useCallback } from 'react';
import { useWorkspaceTabState } from '@/hooks/useWorkspaceTabState';
import DeckGL from '@deck.gl/react';
import { ScatterplotLayer } from '@deck.gl/layers';
import { TileLayer } from '@deck.gl/geo-layers';
import { BitmapLayer } from '@deck.gl/layers';
import { useAuth } from '@/contexts/AuthContext';
import { NewsEventsService, NewsEvent, UniqueCity, UniqueCountry, UniqueCategory } from '@/services/news/newsEventsService';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { TabFooter } from '@/components/common/TabFooter';
import { useCache, cacheKey } from '@/hooks/useCache';
import { withTimeout } from '@/services/core/apiUtils';
import { sanitizeInput } from '@/services/core/validators';
import { ErrorBoundary } from '@/components/common/ErrorBoundary';

// ============================================================================
// Constants
// ============================================================================

const INITIAL_VIEW_STATE = {
  longitude: 30.0,
  latitude: 20.0,
  zoom: 2,
  pitch: 0,
  bearing: 0
};

const THEME = {
  ORANGE: '#FF6600',
  BLACK: '#000000',
  DARK_BG: '#0F0F0F',
  BORDER: '#333333',
  WHITE: '#FFFFFF',
  GREEN: '#00FF00',
  RED: '#FF0000',
  YELLOW: '#FFD700',
  CYAN: '#00E5FF',
  GRAY: '#888888',
  PURPLE: '#9333EA'
};

const API_TIMEOUT_MS = 30000;
const REFRESH_INTERVAL_MS = 300000; // 5 minutes

// ============================================================================
// Types
// ============================================================================

interface EventPoint {
  position: [number, number, number];
  color: [number, number, number, number];
  event: NewsEvent;
}

interface ViewState {
  longitude: number;
  latitude: number;
  zoom: number;
  pitch: number;
  bearing: number;
}

// ============================================================================
// State Machine (Point #2: Explicit states)
// ============================================================================

interface GeoState {
  // Filter state
  countryFilter: string;
  cityFilter: string;
  selectedCities: string[];
  categoryFilter: string;
  // Search UI state
  citySearchInput: string;
  showCitySuggestions: boolean;
  showCountrySuggestions: boolean;
  showCategorySuggestions: boolean;
  // Map state
  viewState: ViewState;
  selectedEvent: NewsEvent | null;
  isPanelCollapsed: boolean;
  // Clock
  currentTime: Date;
}

type GeoAction =
  | { type: 'SET_COUNTRY_FILTER'; payload: string }
  | { type: 'SET_CITY_FILTER'; payload: string }
  | { type: 'SET_SELECTED_CITIES'; payload: string[] }
  | { type: 'ADD_CITY'; payload: string }
  | { type: 'REMOVE_CITY'; payload: string }
  | { type: 'SET_CATEGORY_FILTER'; payload: string }
  | { type: 'SET_CITY_SEARCH_INPUT'; payload: string }
  | { type: 'SET_SHOW_CITY_SUGGESTIONS'; payload: boolean }
  | { type: 'SET_SHOW_COUNTRY_SUGGESTIONS'; payload: boolean }
  | { type: 'SET_SHOW_CATEGORY_SUGGESTIONS'; payload: boolean }
  | { type: 'SET_VIEW_STATE'; payload: ViewState }
  | { type: 'SELECT_EVENT'; payload: { event: NewsEvent; viewState: ViewState } }
  | { type: 'CLEAR_SELECTED_EVENT' }
  | { type: 'TOGGLE_PANEL' }
  | { type: 'TICK' }
  | { type: 'CLEAR_FILTERS' }
  | { type: 'SELECT_COUNTRY_SUGGESTION'; payload: string }
  | { type: 'SELECT_CATEGORY_SUGGESTION'; payload: string }
  | { type: 'SELECT_CITY_SUGGESTION'; payload: { city: string; isSelected: boolean } }
  | { type: 'RESTORE_WORKSPACE'; payload: Partial<GeoState> };

const initialState: GeoState = {
  countryFilter: 'Ukraine',
  cityFilter: '',
  selectedCities: [],
  categoryFilter: '',
  citySearchInput: '',
  showCitySuggestions: false,
  showCountrySuggestions: false,
  showCategorySuggestions: false,
  viewState: INITIAL_VIEW_STATE,
  selectedEvent: null,
  isPanelCollapsed: false,
  currentTime: new Date(),
};

// Point #1 & #9: Atomic updates via reducer, no global mutation
function geoReducer(state: GeoState, action: GeoAction): GeoState {
  switch (action.type) {
    case 'SET_COUNTRY_FILTER':
      return { ...state, countryFilter: action.payload };
    case 'SET_CITY_FILTER':
      return { ...state, cityFilter: action.payload };
    case 'SET_SELECTED_CITIES':
      return { ...state, selectedCities: action.payload };
    case 'ADD_CITY':
      if (state.selectedCities.includes(action.payload)) return state;
      return { ...state, selectedCities: [...state.selectedCities, action.payload] };
    case 'REMOVE_CITY':
      return { ...state, selectedCities: state.selectedCities.filter(c => c !== action.payload) };
    case 'SET_CATEGORY_FILTER':
      return { ...state, categoryFilter: action.payload };
    case 'SET_CITY_SEARCH_INPUT':
      return { ...state, citySearchInput: action.payload };
    case 'SET_SHOW_CITY_SUGGESTIONS':
      return { ...state, showCitySuggestions: action.payload };
    case 'SET_SHOW_COUNTRY_SUGGESTIONS':
      return { ...state, showCountrySuggestions: action.payload };
    case 'SET_SHOW_CATEGORY_SUGGESTIONS':
      return { ...state, showCategorySuggestions: action.payload };
    case 'SET_VIEW_STATE':
      return { ...state, viewState: action.payload };
    case 'SELECT_EVENT':
      return {
        ...state,
        selectedEvent: action.payload.event,
        viewState: action.payload.viewState,
      };
    case 'CLEAR_SELECTED_EVENT':
      return { ...state, selectedEvent: null };
    case 'TOGGLE_PANEL':
      return { ...state, isPanelCollapsed: !state.isPanelCollapsed };
    case 'TICK':
      return { ...state, currentTime: new Date() };
    case 'CLEAR_FILTERS':
      return {
        ...state,
        countryFilter: 'Ukraine',
        cityFilter: '',
        selectedCities: [],
        citySearchInput: '',
        categoryFilter: '',
      };
    case 'SELECT_COUNTRY_SUGGESTION':
      return { ...state, countryFilter: action.payload, showCountrySuggestions: false };
    case 'SELECT_CATEGORY_SUGGESTION':
      return { ...state, categoryFilter: action.payload, showCategorySuggestions: false };
    case 'SELECT_CITY_SUGGESTION': {
      const newCities = action.payload.isSelected
        ? state.selectedCities.filter(c => c !== action.payload.city)
        : [...state.selectedCities, action.payload.city];
      return {
        ...state,
        selectedCities: newCities,
        citySearchInput: '',
        showCitySuggestions: false,
      };
    }
    case 'RESTORE_WORKSPACE':
      return { ...state, ...action.payload };
    default:
      return state;
  }
}

// ============================================================================
// Helpers
// ============================================================================

function getCategoryColor(category?: string): [number, number, number, number] {
  if (!category) return [136, 136, 136, 200];
  const cat = category.toLowerCase();
  if (cat.includes('armed_conflict')) return [255, 0, 0, 220];
  if (cat.includes('terrorism')) return [255, 69, 0, 220];
  if (cat.includes('protests')) return [255, 215, 0, 200];
  if (cat.includes('civilian_violence')) return [255, 100, 100, 200];
  if (cat.includes('riots')) return [255, 165, 0, 200];
  if (cat.includes('political_violence')) return [147, 51, 234, 200];
  if (cat.includes('crisis')) return [0, 229, 255, 200];
  if (cat.includes('explosions')) return [255, 20, 147, 220];
  if (cat.includes('strategic')) return [100, 149, 237, 200];
  return [136, 136, 136, 200];
}

// ============================================================================
// Inner Component (wrapped by ErrorBoundary)
// ============================================================================

const GeopoliticsTabInner: React.FC = () => {
  const { colors, fontFamily } = useTerminalTheme();
  const { session } = useAuth();
  const apiKey = session?.api_key || null;

  // Point #1 & #9: Single reducer for all related state
  const [state, dispatch] = useReducer(geoReducer, initialState);

  const {
    countryFilter, cityFilter, selectedCities, categoryFilter,
    citySearchInput, showCitySuggestions, showCountrySuggestions,
    showCategorySuggestions, viewState, selectedEvent, isPanelCollapsed,
    currentTime,
  } = state;

  // ---- Workspace tab state persistence ----
  const getWorkspaceState = useCallback(() => ({
    countryFilter, categoryFilter, isPanelCollapsed,
  }), [countryFilter, categoryFilter, isPanelCollapsed]);

  const setWorkspaceState = useCallback((ws: Record<string, unknown>) => {
    const patch: Partial<GeoState> = {};
    if (typeof ws.countryFilter === 'string') patch.countryFilter = ws.countryFilter;
    if (typeof ws.categoryFilter === 'string') patch.categoryFilter = ws.categoryFilter;
    if (typeof ws.isPanelCollapsed === 'boolean') patch.isPanelCollapsed = ws.isPanelCollapsed;
    dispatch({ type: 'RESTORE_WORKSPACE', payload: patch });
  }, []);

  useWorkspaceTabState('geopolitics', getWorkspaceState, setWorkspaceState);

  // ---- Build cache key from current filters ----
  const eventsCacheKey = useMemo(() => cacheKey(
    'geopolitics-events',
    sanitizeInput(countryFilter),
    sanitizeInput(cityFilter),
    selectedCities.map(c => sanitizeInput(c)).join(','),
    sanitizeInput(categoryFilter),
  ), [countryFilter, cityFilter, selectedCities, categoryFilter]);

  // Point #10: Validate/sanitize inputs before API calls
  const sanitizedFilters = useMemo(() => ({
    country: sanitizeInput(countryFilter) || undefined,
    city: sanitizeInput(cityFilter) || undefined,
    cities: selectedCities.length > 0 ? selectedCities.map(c => sanitizeInput(c)).join(',') : undefined,
    event_category: sanitizeInput(categoryFilter) || undefined,
  }), [countryFilter, cityFilter, selectedCities, categoryFilter]);

  // ---- Points #1-4, #7-8: useCache for events data ----
  // Handles race conditions, state machine, cleanup, dedup, cache, timeout
  const {
    data: eventsResponse,
    isLoading: eventsLoading,
    error: eventsError,
    refresh: refreshEvents,
    isStale: eventsStale,
  } = useCache({
    key: eventsCacheKey,
    category: 'geopolitics',
    fetcher: async () => {
      if (!apiKey) throw new Error('No API key available');
      // Point #8: Timeout protection
      return withTimeout(
        NewsEventsService.getNewsEvents(apiKey, {
          ...sanitizedFilters,
          limit: 100,
        }),
        API_TIMEOUT_MS,
      );
    },
    enabled: !!apiKey,
    ttl: REFRESH_INTERVAL_MS,
    staleWhileRevalidate: true,
    refetchInterval: REFRESH_INTERVAL_MS,
    onSuccess: (response) => {
      // Auto-zoom to events location on success
      if (response.success && response.events.length > 0) {
        const firstEvent = response.events[0];
        if (firstEvent.latitude && firstEvent.longitude) {
          dispatch({
            type: 'SET_VIEW_STATE',
            payload: {
              longitude: firstEvent.longitude,
              latitude: firstEvent.latitude,
              zoom: cityFilter ? 12 : (countryFilter ? 6 : 4),
              pitch: 0,
              bearing: 0,
            },
          });
        }
      }
    },
  });

  const events = useMemo(() => {
    if (!eventsResponse?.success) return [];
    return eventsResponse.events || [];
  }, [eventsResponse]);

  const eventsErrorMessage = useMemo(() => {
    if (eventsError) return eventsError.message;
    if (eventsResponse && !eventsResponse.success) return eventsResponse.message || 'API Error';
    if (eventsResponse?.success && events.length === 0) {
      const filterDesc = countryFilter || cityFilter || categoryFilter
        ? `No events found for: ${[countryFilter, cityFilter, categoryFilter].filter(f => f).join(', ')}`
        : 'No events found with valid coordinates. Try filtering by country (e.g., Ukraine, Syria, Israel)';
      return filterDesc;
    }
    return null;
  }, [eventsError, eventsResponse, events, countryFilter, cityFilter, categoryFilter]);

  // ---- Points #1-4, #7: useCache for reference data (cities, countries, categories) ----
  const { data: uniqueCities } = useCache<UniqueCity[]>({
    key: cacheKey('geopolitics-unique-cities'),
    category: 'geopolitics',
    fetcher: async () => {
      if (!apiKey) throw new Error('No API key');
      return withTimeout(NewsEventsService.getUniqueCities(apiKey), API_TIMEOUT_MS);
    },
    enabled: !!apiKey,
    ttl: 600000, // 10 min TTL for reference data
    staleWhileRevalidate: true,
  });

  const { data: uniqueCountries } = useCache<UniqueCountry[]>({
    key: cacheKey('geopolitics-unique-countries'),
    category: 'geopolitics',
    fetcher: async () => {
      if (!apiKey) throw new Error('No API key');
      return withTimeout(NewsEventsService.getUniqueCountries(apiKey), API_TIMEOUT_MS);
    },
    enabled: !!apiKey,
    ttl: 600000,
    staleWhileRevalidate: true,
  });

  const { data: uniqueCategories } = useCache<UniqueCategory[]>({
    key: cacheKey('geopolitics-unique-categories'),
    category: 'geopolitics',
    fetcher: async () => {
      if (!apiKey) throw new Error('No API key');
      return withTimeout(NewsEventsService.getUniqueCategories(apiKey), API_TIMEOUT_MS);
    },
    enabled: !!apiKey,
    ttl: 600000,
    staleWhileRevalidate: true,
  });

  // ---- Derived suggestion lists (no useEffect needed) ----
  const citySuggestions = useMemo(() => {
    if (citySearchInput.length < 2 || !uniqueCities) return [];
    return uniqueCities
      .filter(c => c.city && c.city.toLowerCase().includes(citySearchInput.toLowerCase()))
      .slice(0, 10);
  }, [citySearchInput, uniqueCities]);

  const countrySuggestions = useMemo(() => {
    if (countryFilter.length < 2 || !uniqueCountries) return [];
    return uniqueCountries
      .filter(c => c.country && c.country.toLowerCase().includes(countryFilter.toLowerCase()))
      .slice(0, 10);
  }, [countryFilter, uniqueCountries]);

  const categorySuggestions = useMemo(() => {
    if (categoryFilter.length < 2 || !uniqueCategories) return [];
    return uniqueCategories
      .filter(c => c.event_category && c.event_category.toLowerCase().includes(categoryFilter.toLowerCase()))
      .slice(0, 10);
  }, [categoryFilter, uniqueCategories]);

  // Show/hide suggestions based on derived data
  useEffect(() => {
    dispatch({ type: 'SET_SHOW_CITY_SUGGESTIONS', payload: citySuggestions.length > 0 && citySearchInput.length >= 2 });
  }, [citySuggestions, citySearchInput]);

  useEffect(() => {
    dispatch({ type: 'SET_SHOW_COUNTRY_SUGGESTIONS', payload: countrySuggestions.length > 0 && countryFilter.length >= 2 });
  }, [countrySuggestions, countryFilter]);

  useEffect(() => {
    dispatch({ type: 'SET_SHOW_CATEGORY_SUGGESTIONS', payload: categorySuggestions.length > 0 && categoryFilter.length >= 2 });
  }, [categorySuggestions, categoryFilter]);

  // ---- Point #3: Clock timer with proper cleanup ----
  useEffect(() => {
    const timer = setInterval(() => dispatch({ type: 'TICK' }), 1000);
    return () => clearInterval(timer);
  }, []);

  // ---- Memoized event points ----
  const eventPoints: EventPoint[] = useMemo(() => {
    const coordGroups = new Map<string, NewsEvent[]>();

    events.forEach(event => {
      const coordKey = `${event.latitude},${event.longitude}`;
      if (!coordGroups.has(coordKey)) {
        coordGroups.set(coordKey, []);
      }
      coordGroups.get(coordKey)!.push(event);
    });

    const points: EventPoint[] = [];

    coordGroups.forEach((groupEvents) => {
      groupEvents.forEach((event) => {
        const randomOffset = Math.random() * 0.05;
        const randomAngle = Math.random() * Math.PI * 2;

        points.push({
          position: [
            event.longitude! + randomOffset * Math.cos(randomAngle),
            event.latitude! + randomOffset * Math.sin(randomAngle),
            0
          ] as [number, number, number],
          color: getCategoryColor(event.event_category),
          event
        });
      });
    });

    return points;
  }, [events]);

  // ---- Memoized layers ----
  const layers = useMemo(() => {
    const allLayers: any[] = [
      new TileLayer({
        id: 'basemap',
        data: 'https://basemaps.cartocdn.com/dark_all/{z}/{x}/{y}.png',
        minZoom: 0,
        maxZoom: 19,
        tileSize: 256,
        renderSubLayers: (props: any) => {
          const { boundingBox } = props.tile;
          return new BitmapLayer(props, {
            data: undefined,
            image: props.data,
            bounds: [boundingBox[0][0], boundingBox[0][1], boundingBox[1][0], boundingBox[1][1]]
          });
        }
      })
    ];

    if (eventPoints.length > 0) {
      allLayers.push(
        new ScatterplotLayer({
          id: 'events',
          data: eventPoints,
          getPosition: (d: any) => d.position,
          getFillColor: (d: any) => d.color,
          getRadius: 150,
          radiusMinPixels: 5,
          radiusMaxPixels: 30,
          pickable: true,
          autoHighlight: true,
          highlightColor: [255, 255, 255, 150],
          onClick: (info: any) => {
            if (info.object) {
              dispatch({
                type: 'SELECT_EVENT',
                payload: {
                  event: info.object.event,
                  viewState: {
                    ...viewState,
                    longitude: info.object.event.longitude,
                    latitude: info.object.event.latitude
                  },
                },
              });
            }
          }
        })
      );
    }

    return allLayers;
  }, [eventPoints, viewState]);

  // ---- Category & country statistics ----
  const categoryStats = useMemo(() => {
    const stats: { [key: string]: number } = {};
    events.forEach(e => {
      const cat = e.event_category || 'Unknown';
      stats[cat] = (stats[cat] || 0) + 1;
    });
    return Object.entries(stats)
      .sort((a, b) => b[1] - a[1])
      .slice(0, 5);
  }, [events]);

  const countryStats = useMemo(() => {
    const stats: { [key: string]: number } = {};
    events.forEach(e => {
      const country = e.country || 'Unknown';
      stats[country] = (stats[country] || 0) + 1;
    });
    return Object.entries(stats)
      .sort((a, b) => b[1] - a[1])
      .slice(0, 5);
  }, [events]);

  // ---- Handlers ----
  const handleApply = useCallback(() => {
    refreshEvents();
  }, [refreshEvents]);

  return (
    <div style={{
      width: '100%',
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      background: colors.background,
      color: colors.text,
      fontFamily: fontFamily,
      position: 'relative',
      overflow: 'hidden'
    }}>
      {/* Header */}
      <div style={{
        position: 'absolute',
        top: 0,
        left: 0,
        right: 0,
        background: THEME.BLACK,
        borderBottom: `2px solid ${THEME.ORANGE}`,
        zIndex: 1000,
        fontFamily: 'Consolas, monospace',
        fontSize: '12px'
      }}>
        {/* Title Bar */}
        <div style={{
          padding: '6px 12px',
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          borderBottom: `1px solid ${THEME.BORDER}`,
          flexWrap: 'wrap'
        }}>
          <div style={{ color: THEME.ORANGE, fontWeight: 'bold', fontSize: '13px' }}>
            GEO&gt;
          </div>
          <div style={{ color: THEME.WHITE, fontWeight: 'bold', fontSize: '12px' }}>
            GEOPOLITICAL CONFLICT MONITOR
          </div>
          <div style={{ color: events.length > 0 ? THEME.GREEN : THEME.RED, fontWeight: 'bold', fontSize: '11px' }}>
            {events.length} EVENTS
          </div>
          <div style={{ color: THEME.CYAN, fontSize: '10px' }}>
            {currentTime.toISOString().substring(0, 19)} UTC
          </div>
          {eventsStale && (
            <div style={{ color: THEME.YELLOW, fontSize: '10px' }}>
              STALE
            </div>
          )}
        </div>

        {/* Filter Controls */}
        <div style={{
          padding: '6px 12px',
          display: 'flex',
          gap: '6px',
          alignItems: 'center',
          flexWrap: 'wrap'
        }}>
          <div style={{ position: 'relative', minWidth: '100px', flex: '0 1 auto' }}>
            <input
              type="text"
              placeholder="Country..."
              value={countryFilter}
              onChange={(e) => dispatch({ type: 'SET_COUNTRY_FILTER', payload: e.target.value })}
              onKeyDown={(e) => {
                if (e.key === 'Enter' && countrySuggestions.length > 0) {
                  dispatch({ type: 'SELECT_COUNTRY_SUGGESTION', payload: countrySuggestions[0].country });
                  handleApply();
                } else if (e.key === 'Enter') {
                  handleApply();
                } else if (e.key === 'Escape') {
                  dispatch({ type: 'SET_SHOW_COUNTRY_SUGGESTIONS', payload: false });
                }
              }}
              onFocus={() => countrySuggestions.length > 0 && dispatch({ type: 'SET_SHOW_COUNTRY_SUGGESTIONS', payload: true })}
              style={{
                padding: '5px 8px',
                background: '#1a1a1a',
                border: '1px solid #333',
                color: '#fff',
                fontFamily: 'Consolas, monospace',
                fontSize: '11px',
                width: '100%',
                outline: 'none',
                borderRadius: '2px'
              }}
            />
            {showCountrySuggestions && countrySuggestions.length > 0 && (
              <div style={{
                position: 'absolute',
                top: '100%',
                left: 0,
                width: '200px',
                background: '#1a1a1a',
                border: '1px solid #333',
                borderTop: 'none',
                maxHeight: '200px',
                overflowY: 'auto',
                zIndex: 2000,
                marginTop: '1px'
              }}>
                {countrySuggestions.map((suggestion, idx) => (
                  <div
                    key={idx}
                    onClick={() => dispatch({ type: 'SELECT_COUNTRY_SUGGESTION', payload: suggestion.country })}
                    style={{
                      padding: '6px 8px',
                      cursor: 'pointer',
                      fontSize: '10px',
                      borderBottom: idx < countrySuggestions.length - 1 ? '1px solid #333' : 'none',
                      color: '#ccc',
                      background: '#1a1a1a'
                    }}
                    onMouseEnter={(e) => {
                      e.currentTarget.style.background = '#2a2a2a';
                      e.currentTarget.style.color = '#fff';
                    }}
                    onMouseLeave={(e) => {
                      e.currentTarget.style.background = '#1a1a1a';
                      e.currentTarget.style.color = '#ccc';
                    }}
                  >
                    {suggestion.country} ({suggestion.event_count} events)
                  </div>
                ))}
              </div>
            )}
          </div>

          <div style={{ position: 'relative', minWidth: '100px', flex: '0 1 auto' }}>
            <input
              type="text"
              placeholder={selectedCities.length > 0 ? `${selectedCities.length} cities` : "Cities..."}
              value={citySearchInput}
              onChange={(e) => dispatch({ type: 'SET_CITY_SEARCH_INPUT', payload: e.target.value })}
              onKeyDown={(e) => {
                if (e.key === 'Enter' && citySuggestions.length > 0) {
                  dispatch({ type: 'SELECT_CITY_SUGGESTION', payload: { city: citySuggestions[0].city, isSelected: selectedCities.includes(citySuggestions[0].city) } });
                } else if (e.key === 'Escape') {
                  dispatch({ type: 'SET_SHOW_CITY_SUGGESTIONS', payload: false });
                }
              }}
              onFocus={() => citySuggestions.length > 0 && dispatch({ type: 'SET_SHOW_CITY_SUGGESTIONS', payload: true })}
              style={{
                padding: '5px 8px',
                paddingRight: selectedCities.length > 0 ? '24px' : '8px',
                background: '#1a1a1a',
                border: `1px solid ${selectedCities.length > 0 ? THEME.PURPLE : '#333'}`,
                color: '#fff',
                fontFamily: 'Consolas, monospace',
                fontSize: '11px',
                width: '100%',
                outline: 'none',
                borderRadius: '2px'
              }}
            />
            {selectedCities.length > 0 && (
              <div
                onClick={() => dispatch({ type: 'SET_SELECTED_CITIES', payload: [] })}
                style={{
                  position: 'absolute',
                  right: '4px',
                  top: '50%',
                  transform: 'translateY(-50%)',
                  cursor: 'pointer',
                  color: THEME.RED,
                  fontWeight: 'bold',
                  fontSize: '12px',
                  lineHeight: '1',
                  padding: '2px'
                }}
                title="Clear all cities"
              >
                ×
              </div>
            )}
            {showCitySuggestions && citySuggestions.length > 0 && (
              <div style={{
                position: 'absolute',
                top: '100%',
                left: 0,
                width: '240px',
                background: '#1a1a1a',
                border: '1px solid #333',
                borderTop: 'none',
                maxHeight: '200px',
                overflowY: 'auto',
                zIndex: 2000,
                marginTop: '1px'
              }}>
                {citySuggestions.map((suggestion, idx) => {
                  const isSelected = selectedCities.includes(suggestion.city);
                  return (
                    <div
                      key={idx}
                      onClick={() => dispatch({ type: 'SELECT_CITY_SUGGESTION', payload: { city: suggestion.city, isSelected } })}
                      style={{
                        padding: '6px 8px',
                        cursor: 'pointer',
                        fontSize: '10px',
                        borderBottom: idx < citySuggestions.length - 1 ? '1px solid #333' : 'none',
                        color: isSelected ? THEME.PURPLE : '#ccc',
                        background: isSelected ? 'rgba(147, 51, 234, 0.2)' : '#1a1a1a',
                        display: 'flex',
                        justifyContent: 'space-between',
                        alignItems: 'center'
                      }}
                      onMouseEnter={(e) => {
                        if (!isSelected) {
                          e.currentTarget.style.background = '#2a2a2a';
                          e.currentTarget.style.color = '#fff';
                        }
                      }}
                      onMouseLeave={(e) => {
                        if (!isSelected) {
                          e.currentTarget.style.background = '#1a1a1a';
                          e.currentTarget.style.color = '#ccc';
                        }
                      }}
                    >
                      <span>{suggestion.city}, {suggestion.country}</span>
                      {isSelected && <span style={{ color: THEME.PURPLE }}>✓</span>}
                    </div>
                  );
                })}
              </div>
            )}
          </div>

          <div style={{ position: 'relative', minWidth: '110px', flex: '0 1 auto' }}>
            <input
              type="text"
              placeholder="Category..."
              value={categoryFilter}
              onChange={(e) => dispatch({ type: 'SET_CATEGORY_FILTER', payload: e.target.value })}
              onKeyDown={(e) => {
                if (e.key === 'Enter' && categorySuggestions.length > 0) {
                  dispatch({ type: 'SELECT_CATEGORY_SUGGESTION', payload: categorySuggestions[0].event_category });
                  handleApply();
                } else if (e.key === 'Enter') {
                  handleApply();
                } else if (e.key === 'Escape') {
                  dispatch({ type: 'SET_SHOW_CATEGORY_SUGGESTIONS', payload: false });
                }
              }}
              onFocus={() => categorySuggestions.length > 0 && dispatch({ type: 'SET_SHOW_CATEGORY_SUGGESTIONS', payload: true })}
              style={{
                padding: '5px 8px',
                background: '#1a1a1a',
                border: '1px solid #333',
                color: '#fff',
                fontFamily: 'Consolas, monospace',
                fontSize: '11px',
                width: '100%',
                outline: 'none',
                borderRadius: '2px'
              }}
            />
            {showCategorySuggestions && categorySuggestions.length > 0 && (
              <div style={{
                position: 'absolute',
                top: '100%',
                left: 0,
                width: '200px',
                background: '#1a1a1a',
                border: '1px solid #333',
                borderTop: 'none',
                maxHeight: '200px',
                overflowY: 'auto',
                zIndex: 2000,
                marginTop: '1px'
              }}>
                {categorySuggestions.map((suggestion, idx) => (
                  <div
                    key={idx}
                    onClick={() => dispatch({ type: 'SELECT_CATEGORY_SUGGESTION', payload: suggestion.event_category })}
                    style={{
                      padding: '6px 8px',
                      cursor: 'pointer',
                      fontSize: '10px',
                      borderBottom: idx < categorySuggestions.length - 1 ? '1px solid #333' : 'none',
                      color: '#ccc',
                      background: '#1a1a1a'
                    }}
                    onMouseEnter={(e) => {
                      e.currentTarget.style.background = '#2a2a2a';
                      e.currentTarget.style.color = '#fff';
                    }}
                    onMouseLeave={(e) => {
                      e.currentTarget.style.background = '#1a1a1a';
                      e.currentTarget.style.color = '#ccc';
                    }}
                  >
                    {suggestion.event_category} ({suggestion.event_count} events)
                  </div>
                ))}
              </div>
            )}
          </div>

          <button
            onClick={handleApply}
            disabled={eventsLoading}
            style={{
              padding: '5px 14px',
              background: THEME.ORANGE,
              border: 'none',
              color: '#000',
              fontFamily: 'Consolas, monospace',
              fontSize: '10px',
              fontWeight: 'bold',
              cursor: eventsLoading ? 'not-allowed' : 'pointer',
              opacity: eventsLoading ? 0.5 : 1,
              borderRadius: '2px',
              minWidth: '60px'
            }}
          >
            {eventsLoading ? 'LOADING...' : 'APPLY'}
          </button>

          {(countryFilter || cityFilter || selectedCities.length > 0 || categoryFilter) && (
            <button
              onClick={() => dispatch({ type: 'CLEAR_FILTERS' })}
              style={{
                padding: '5px 12px',
                background: '#000',
                border: '1px solid #666',
                color: '#fff',
                fontFamily: 'Consolas, monospace',
                fontSize: '10px',
                fontWeight: 'bold',
                cursor: 'pointer',
                borderRadius: '2px',
                minWidth: '55px'
              }}
            >
              CLEAR
            </button>
          )}

          {/* Selected Cities Display */}
          {selectedCities.length > 0 && (
            <div style={{
              display: 'flex',
              gap: '4px',
              flexWrap: 'wrap',
              alignItems: 'center',
              flex: '1 1 100%',
              marginTop: '4px'
            }}>
              <span style={{ fontSize: '9px', color: THEME.GRAY }}>Selected:</span>
              {selectedCities.map((city, idx) => (
                <div
                  key={idx}
                  style={{
                    padding: '3px 8px',
                    background: THEME.PURPLE,
                    color: THEME.WHITE,
                    fontSize: '9px',
                    borderRadius: '3px',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '4px'
                  }}
                >
                  {city}
                  <span
                    onClick={() => dispatch({ type: 'REMOVE_CITY', payload: city })}
                    style={{ cursor: 'pointer', fontWeight: 'bold', color: THEME.WHITE, fontSize: '11px' }}
                  >
                    ×
                  </span>
                </div>
              ))}
            </div>
          )}
        </div>
      </div>

      {/* Error Display - only show meaningful errors, not filter-related */}
      {eventsErrorMessage && !eventsErrorMessage.includes('No events found for:') && (
        <div style={{
          position: 'absolute',
          top: '80px',
          left: '20px',
          right: '20px',
          padding: '12px 20px',
          background: 'rgba(255, 0, 0, 0.2)',
          border: '1px solid #ff0000',
          borderRadius: '4px',
          color: '#ff6b6b',
          fontFamily: 'Consolas, monospace',
          fontSize: '13px',
          zIndex: 10,
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
        }}>
          <span>ERROR: {eventsErrorMessage}</span>
          <button
            onClick={handleApply}
            style={{
              background: 'transparent',
              border: '1px solid #ff6b6b',
              color: '#ff6b6b',
              padding: '4px 12px',
              cursor: 'pointer',
              fontFamily: 'Consolas, monospace',
              fontSize: '11px',
              borderRadius: '2px',
            }}
          >
            RETRY
          </button>
        </div>
      )}

      {/* Side Panel */}
      <div style={{
        position: 'absolute',
        top: '110px',
        right: '10px',
        bottom: '60px',
        width: isPanelCollapsed ? '40px' : 'min(280px, calc(100vw - 20px))',
        maxWidth: isPanelCollapsed ? '40px' : '90vw',
        background: 'rgba(0, 0, 0, 0.95)',
        border: `1px solid ${THEME.BORDER}`,
        borderRadius: '4px',
        zIndex: 999,
        overflowY: isPanelCollapsed ? 'hidden' : 'auto',
        overflowX: 'hidden',
        fontFamily: 'Consolas, monospace',
        fontSize: '10px',
        backdropFilter: 'blur(10px)',
        transition: 'width 0.3s ease'
      }}>
        {/* Collapse Button */}
        <div style={{
          padding: '8px',
          borderBottom: `1px solid ${THEME.BORDER}`,
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center'
        }}>
          {!isPanelCollapsed && (
            <div style={{ color: THEME.ORANGE, fontWeight: 'bold', fontSize: '11px' }}>
              INFO PANEL
            </div>
          )}
          <button
            onClick={() => dispatch({ type: 'TOGGLE_PANEL' })}
            style={{
              background: 'transparent',
              border: 'none',
              color: THEME.ORANGE,
              cursor: 'pointer',
              fontSize: '14px',
              padding: '4px',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center'
            }}
            title={isPanelCollapsed ? 'Expand panel' : 'Collapse panel'}
          >
            {isPanelCollapsed ? '◀' : '▶'}
          </button>
        </div>

        {!isPanelCollapsed && (
          <>
            {/* Events Loaded */}
            <div style={{ padding: '10px', borderBottom: `1px solid ${THEME.BORDER}` }}>
              <div style={{ color: THEME.ORANGE, fontWeight: 'bold', marginBottom: '8px', fontSize: '11px' }}>
                EVENTS LOADED
              </div>
              <div style={{ color: THEME.YELLOW, fontSize: '14px', fontWeight: 'bold', marginBottom: '4px' }}>
                {events.length} events
              </div>
              <div style={{ color: THEME.GRAY, fontSize: '9px' }}>
                on map visualization
              </div>
            </div>

            {/* Statistics */}
            <div style={{ padding: '10px', borderBottom: `1px solid ${THEME.BORDER}` }}>
              <div style={{ color: THEME.ORANGE, fontWeight: 'bold', marginBottom: '8px', fontSize: '11px' }}>
                TOP CATEGORIES
              </div>
              {categoryStats.map(([cat, count], idx) => (
                <div key={idx} style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  marginBottom: '4px',
                  color: THEME.WHITE,
                  gap: '8px'
                }}>
                  <span style={{
                    overflow: 'hidden',
                    textOverflow: 'ellipsis',
                    whiteSpace: 'nowrap',
                    flex: 1
                  }}>{cat}</span>
                  <span style={{ color: THEME.YELLOW, flexShrink: 0 }}>{count}</span>
                </div>
              ))}
            </div>

            <div style={{ padding: '10px', borderBottom: `1px solid ${THEME.BORDER}` }}>
              <div style={{ color: THEME.ORANGE, fontWeight: 'bold', marginBottom: '8px', fontSize: '11px' }}>
                TOP COUNTRIES
              </div>
              {countryStats.map(([country, count], idx) => (
                <div key={idx} style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  marginBottom: '4px',
                  color: THEME.WHITE,
                  gap: '8px'
                }}>
                  <span style={{
                    overflow: 'hidden',
                    textOverflow: 'ellipsis',
                    whiteSpace: 'nowrap',
                    flex: 1
                  }}>{country}</span>
                  <span style={{ color: THEME.YELLOW, flexShrink: 0 }}>{count}</span>
                </div>
              ))}
            </div>

            {/* Selected Event Details */}
            {selectedEvent && (
              <div style={{ padding: '10px' }}>
                <div style={{ color: THEME.ORANGE, fontWeight: 'bold', marginBottom: '8px', fontSize: '11px' }}>
                  EVENT DETAILS
                </div>
                <div style={{ color: THEME.WHITE, lineHeight: '1.6', wordBreak: 'break-word' }}>
                  {selectedEvent.event_category && (
                    <div style={{ marginBottom: '6px', fontWeight: 'bold', color: THEME.YELLOW, fontSize: '10px' }}>
                      {selectedEvent.event_category.toUpperCase().replace(/_/g, ' ')}
                    </div>
                  )}
                  {selectedEvent.country && (
                    <div style={{ marginBottom: '4px', fontSize: '9px' }}>
                      <span style={{ color: THEME.GRAY }}>Country:</span> {selectedEvent.country}
                    </div>
                  )}
                  {selectedEvent.city && (
                    <div style={{ marginBottom: '4px', fontSize: '9px' }}>
                      <span style={{ color: THEME.GRAY }}>City:</span> {selectedEvent.city}
                    </div>
                  )}
                  {selectedEvent.matched_keywords && (
                    <div style={{ marginBottom: '4px', fontSize: '9px' }}>
                      <span style={{ color: THEME.GRAY }}>Keywords:</span> {selectedEvent.matched_keywords}
                    </div>
                  )}
                  {selectedEvent.latitude !== undefined && selectedEvent.longitude !== undefined && (
                    <div style={{ marginBottom: '4px', fontSize: '8px' }}>
                      <span style={{ color: THEME.GRAY }}>Coords:</span> {selectedEvent.latitude.toFixed(4)}, {selectedEvent.longitude.toFixed(4)}
                    </div>
                  )}
                  {selectedEvent.extracted_date && (
                    <div style={{ marginBottom: '4px', fontSize: '8px' }}>
                      <span style={{ color: THEME.GRAY }}>Date:</span> {new Date(selectedEvent.extracted_date).toLocaleString()}
                    </div>
                  )}
                  {selectedEvent.url && (
                    <div style={{ marginBottom: '4px', fontSize: '8px' }}>
                      <span style={{ color: THEME.GRAY }}>Source:</span>{' '}
                      <a
                        href={selectedEvent.url}
                        target="_blank"
                        rel="noopener noreferrer"
                        style={{
                          color: THEME.CYAN,
                          textDecoration: 'underline',
                          cursor: 'pointer',
                          wordBreak: 'break-all'
                        }}
                      >
                        View Article
                      </a>
                    </div>
                  )}
                  <button
                    onClick={() => dispatch({ type: 'CLEAR_SELECTED_EVENT' })}
                    style={{
                      marginTop: '8px',
                      padding: '6px 10px',
                      background: THEME.BORDER,
                      border: 'none',
                      color: THEME.WHITE,
                      fontFamily: 'Consolas, monospace',
                      fontSize: '9px',
                      cursor: 'pointer',
                      width: '100%',
                      borderRadius: '2px'
                    }}
                  >
                    CLOSE
                  </button>
                </div>
              </div>
            )}

            {/* Legend */}
            <div style={{ padding: '10px', borderTop: `1px solid ${THEME.BORDER}` }}>
              <div style={{ color: THEME.ORANGE, fontWeight: 'bold', marginBottom: '8px', fontSize: '11px' }}>
                EVENT CATEGORIES
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px' }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                  <div style={{ width: '8px', height: '8px', borderRadius: '50%', background: '#FF0000', flexShrink: 0 }}></div>
                  <span style={{ color: THEME.GRAY, fontSize: '8px' }}>Armed Conflict</span>
                </div>
                <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                  <div style={{ width: '8px', height: '8px', borderRadius: '50%', background: '#FF4500', flexShrink: 0 }}></div>
                  <span style={{ color: THEME.GRAY, fontSize: '8px' }}>Terrorism</span>
                </div>
                <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                  <div style={{ width: '8px', height: '8px', borderRadius: '50%', background: '#FFD700', flexShrink: 0 }}></div>
                  <span style={{ color: THEME.GRAY, fontSize: '8px' }}>Protests</span>
                </div>
                <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                  <div style={{ width: '8px', height: '8px', borderRadius: '50%', background: '#FF6464', flexShrink: 0 }}></div>
                  <span style={{ color: THEME.GRAY, fontSize: '8px' }}>Civilian Violence</span>
                </div>
                <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                  <div style={{ width: '8px', height: '8px', borderRadius: '50%', background: '#FFA500', flexShrink: 0 }}></div>
                  <span style={{ color: THEME.GRAY, fontSize: '8px' }}>Riots</span>
                </div>
                <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                  <div style={{ width: '8px', height: '8px', borderRadius: '50%', background: '#9333EA', flexShrink: 0 }}></div>
                  <span style={{ color: THEME.GRAY, fontSize: '8px' }}>Political Violence</span>
                </div>
                <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                  <div style={{ width: '8px', height: '8px', borderRadius: '50%', background: '#00E5FF', flexShrink: 0 }}></div>
                  <span style={{ color: THEME.GRAY, fontSize: '8px' }}>Crisis</span>
                </div>
                <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                  <div style={{ width: '8px', height: '8px', borderRadius: '50%', background: '#FF1493', flexShrink: 0 }}></div>
                  <span style={{ color: THEME.GRAY, fontSize: '8px' }}>Explosions</span>
                </div>
                <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                  <div style={{ width: '8px', height: '8px', borderRadius: '50%', background: '#6495ED', flexShrink: 0 }}></div>
                  <span style={{ color: THEME.GRAY, fontSize: '8px' }}>Strategic</span>
                </div>
                <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                  <div style={{ width: '8px', height: '8px', borderRadius: '50%', background: '#888888', flexShrink: 0 }}></div>
                  <span style={{ color: THEME.GRAY, fontSize: '8px' }}>Unclassified</span>
                </div>
              </div>
            </div>
          </>
        )}
      </div>

      {/* Deck.gl Map */}
      <div style={{ flex: 1, position: 'relative', background: '#000', minHeight: 0 }}>
        <DeckGL
          viewState={viewState}
          onViewStateChange={({ viewState: vs }: any) => dispatch({ type: 'SET_VIEW_STATE', payload: vs })}
          controller={true}
          layers={layers}
          width="100%"
          height="100%"
          style={{ position: 'absolute', inset: '0' }}
          getTooltip={({ object }: any) => object && {
            html: `<div style="max-width: 250px;">
                   <strong>${object.event.event_category?.toUpperCase() || 'EVENT'}</strong><br/>
                   ${object.event.country || ''}${object.event.city ? ', ' + object.event.city : ''}<br/>
                   Keywords: ${object.event.matched_keywords || 'N/A'}<br/>
                   ${object.event.extracted_date ? new Date(object.event.extracted_date).toLocaleDateString() : ''}</div>`,
            style: {
              backgroundColor: '#000',
              color: '#fff',
              padding: '8px',
              borderRadius: '4px',
              border: '1px solid #ff6600',
              fontFamily: 'Consolas, monospace',
              fontSize: '10px'
            }
          }}
        />
      </div>

      {/* Footer */}
      <div style={{ flexShrink: 0 }}>
        <TabFooter
          tabName="GEOPOLITICAL CONFLICT MONITOR"
          leftInfo={[
            { label: 'EVENTS', value: `${events.length}`, color: events.length > 0 ? '#10b981' : '#ef4444' },
            { label: 'ENGINE', value: 'DECK.GL v9.2', color: '#06b6d4' },
            { label: 'SOURCE', value: 'NEWS-EVENTS API', color: '#fbbf24' },
            ...(selectedEvent ? [{ label: 'SELECTED', value: selectedEvent.country || 'N/A', color: '#ffd700' }] : [])
          ]}
          statusInfo={`REAL-TIME GLOBAL INTELLIGENCE | AUTO-REFRESH: 5MIN | ${currentTime.toLocaleTimeString()}`}
          backgroundColor="#000000"
          borderColor="#ff6600"
        />
      </div>
    </div>
  );
};

// Point #5: Wrap with ErrorBoundary
const GeopoliticsTab: React.FC = () => (
  <ErrorBoundary name="GeopoliticsTab" variant="default">
    <GeopoliticsTabInner />
  </ErrorBoundary>
);

export default GeopoliticsTab;
