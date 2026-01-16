import React, { useState, useEffect, useMemo } from 'react';
import DeckGL from '@deck.gl/react';
import { ScatterplotLayer } from '@deck.gl/layers';
import { TileLayer } from '@deck.gl/geo-layers';
import { BitmapLayer } from '@deck.gl/layers';
import { useAuth } from '@/contexts/AuthContext';
import { NewsEventsService, NewsEvent, UniqueCity, UniqueCountry, UniqueCategory } from '@/services/newsEventsService';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { TabFooter } from '@/components/common/TabFooter';

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

interface EventPoint {
  position: [number, number, number];
  color: [number, number, number, number];
  event: NewsEvent;
}

const GeopoliticsTab: React.FC = () => {
  const { colors, fontFamily } = useTerminalTheme();
  const { session } = useAuth();
  const apiKey = session?.api_key || null;

  const [events, setEvents] = useState<NewsEvent[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [viewState, setViewState] = useState(INITIAL_VIEW_STATE);
  const [selectedEvent, setSelectedEvent] = useState<NewsEvent | null>(null);
  const [isPanelCollapsed, setIsPanelCollapsed] = useState(false);

  // Filter controls
  const [countryFilter, setCountryFilter] = useState('Ukraine'); // Default to Ukraine to avoid NaN issues
  const [cityFilter, setCityFilter] = useState('');
  const [selectedCities, setSelectedCities] = useState<string[]>([]); // Multiple cities
  const [categoryFilter, setCategoryFilter] = useState('');
  const [currentTime, setCurrentTime] = useState(new Date());

  // City search
  const [uniqueCities, setUniqueCities] = useState<UniqueCity[]>([]);
  const [citySearchInput, setCitySearchInput] = useState('');
  const [citySuggestions, setCitySuggestions] = useState<UniqueCity[]>([]);
  const [showCitySuggestions, setShowCitySuggestions] = useState(false);

  // Country search
  const [uniqueCountries, setUniqueCountries] = useState<UniqueCountry[]>([]);
  const [countrySuggestions, setCountrySuggestions] = useState<UniqueCountry[]>([]);
  const [showCountrySuggestions, setShowCountrySuggestions] = useState(false);

  // Category search
  const [uniqueCategories, setUniqueCategories] = useState<UniqueCategory[]>([]);
  const [categorySuggestions, setCategorySuggestions] = useState<UniqueCategory[]>([]);
  const [showCategorySuggestions, setShowCategorySuggestions] = useState(false);

  // Event category colors - comprehensive mapping
  const getCategoryColor = (category?: string): [number, number, number, number] => {
    if (!category) return [136, 136, 136, 200]; // Gray
    const cat = category.toLowerCase();

    // Main categories
    if (cat.includes('armed_conflict')) return [255, 0, 0, 220]; // Red
    if (cat.includes('terrorism')) return [255, 69, 0, 220]; // Orange-red
    if (cat.includes('protests')) return [255, 215, 0, 200]; // Gold
    if (cat.includes('civilian_violence')) return [255, 100, 100, 200]; // Light red
    if (cat.includes('riots')) return [255, 165, 0, 200]; // Orange
    if (cat.includes('political_violence')) return [147, 51, 234, 200]; // Purple
    if (cat.includes('crisis')) return [0, 229, 255, 200]; // Cyan
    if (cat.includes('explosions')) return [255, 20, 147, 220]; // Deep pink
    if (cat.includes('strategic')) return [100, 149, 237, 200]; // Cornflower blue

    return [136, 136, 136, 200]; // Default gray for unclassified
  };

  // Load events from API
  const loadEvents = async () => {
    if (!apiKey) {
      setError('No API key available');
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      // Load more events to get better map coverage
      const response = await NewsEventsService.getNewsEvents(apiKey, {
        country: countryFilter || undefined,
        city: cityFilter || undefined,
        cities: selectedCities.length > 0 ? selectedCities.join(',') : undefined,
        event_category: categoryFilter || undefined,
        limit: 100
      });

      if (response.success) {
        if (response.events && response.events.length > 0) {
          setEvents(response.events);
          setError(null);

          // Auto-zoom to the events location
          const firstEvent = response.events[0];
          if (firstEvent.latitude && firstEvent.longitude) {
            setViewState({
              longitude: firstEvent.longitude,
              latitude: firstEvent.latitude,
              zoom: cityFilter ? 12 : (countryFilter ? 6 : 4),
              pitch: 0,
              bearing: 0
            });
          }
        } else if (response.events && response.events.length === 0) {
          setEvents([]);
          const filterDesc = countryFilter || cityFilter || categoryFilter
            ? `No events found for: ${[countryFilter, cityFilter, categoryFilter].filter(f => f).join(', ')}`
            : 'No events found with valid coordinates. Try filtering by country (e.g., Ukraine, Syria, Israel)';
          setError(filterDesc);
        } else {
          setEvents([]);
          setError('No events found. Try using filters.');
        }
      } else {
        setError(response.message || 'API Error: Try filtering by country to avoid data issues');
        setEvents([]);
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error');
      setEvents([]);
    } finally {
      setIsLoading(false);
    }
  };

  // Load unique data on mount
  useEffect(() => {
    const loadUniqueData = async () => {
      if (apiKey) {
        const [cities, countries, categories] = await Promise.all([
          NewsEventsService.getUniqueCities(apiKey),
          NewsEventsService.getUniqueCountries(apiKey),
          NewsEventsService.getUniqueCategories(apiKey)
        ]);
        setUniqueCities(cities);
        setUniqueCountries(countries);
        setUniqueCategories(categories);
      }
    };
    loadUniqueData();
  }, [apiKey]);

  // Filter city suggestions
  useEffect(() => {
    if (citySearchInput.length >= 2) {
      const filtered = uniqueCities
        .filter(c => c.city && c.city.toLowerCase().includes(citySearchInput.toLowerCase()))
        .slice(0, 10);
      setCitySuggestions(filtered);
      setShowCitySuggestions(filtered.length > 0);
    } else {
      setCitySuggestions([]);
      setShowCitySuggestions(false);
    }
  }, [citySearchInput, uniqueCities]);

  // Filter country suggestions
  useEffect(() => {
    if (countryFilter.length >= 2) {
      const filtered = uniqueCountries
        .filter(c => c.country && c.country.toLowerCase().includes(countryFilter.toLowerCase()))
        .slice(0, 10);
      setCountrySuggestions(filtered);
      setShowCountrySuggestions(filtered.length > 0);
    } else {
      setCountrySuggestions([]);
      setShowCountrySuggestions(false);
    }
  }, [countryFilter, uniqueCountries]);

  // Filter category suggestions
  useEffect(() => {
    if (categoryFilter.length >= 2) {
      const filtered = uniqueCategories
        .filter(c => c.event_category && c.event_category.toLowerCase().includes(categoryFilter.toLowerCase()))
        .slice(0, 10);
      setCategorySuggestions(filtered);
      setShowCategorySuggestions(filtered.length > 0);
    } else {
      setCategorySuggestions([]);
      setShowCategorySuggestions(false);
    }
  }, [categoryFilter, uniqueCategories]);

  // Update clock
  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  // Auto-load events on mount (only when APPLY button is clicked, not on filter change)
  useEffect(() => {
    if (apiKey && countryFilter === 'Ukraine' && !cityFilter && selectedCities.length === 0 && !categoryFilter) {
      // Only auto-load on initial mount with default filter
      loadEvents();
    }
    const interval = setInterval(loadEvents, 300000); // Refresh every 5 minutes
    return () => clearInterval(interval);
  }, [apiKey]);

  // Memoized event points with random spread within city bounds
  const eventPoints: EventPoint[] = useMemo(() => {
    // Group events by coordinate to spread them properly
    const coordGroups = new Map<string, NewsEvent[]>();

    events.forEach(event => {
      const coordKey = `${event.latitude},${event.longitude}`;
      if (!coordGroups.has(coordKey)) {
        coordGroups.set(coordKey, []);
      }
      coordGroups.get(coordKey)!.push(event);
    });

    const points: EventPoint[] = [];

    coordGroups.forEach((groupEvents, coordKey) => {
      groupEvents.forEach((event) => {
        // Spread events randomly within city bounds (~2-5km radius)
        const randomOffset = Math.random() * 0.05; // Random distance up to 5km
        const randomAngle = Math.random() * Math.PI * 2; // Random direction

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

  // Memoized layers
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
            data: null,
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
              setSelectedEvent(info.object.event);
              // Don't change zoom, just center on the event
              setViewState({
                ...viewState,
                longitude: info.object.event.longitude,
                latitude: info.object.event.latitude
              });
            }
          }
        })
      );
    }

    return allLayers;
  }, [eventPoints, viewState]);

  // Get category statistics
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

  // Get country statistics
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
              onChange={(e) => setCountryFilter(e.target.value)}
              onKeyDown={(e) => {
                if (e.key === 'Enter' && countrySuggestions.length > 0) {
                  setCountryFilter(countrySuggestions[0].country);
                  setShowCountrySuggestions(false);
                  loadEvents();
                } else if (e.key === 'Enter') {
                  loadEvents();
                } else if (e.key === 'Escape') {
                  setShowCountrySuggestions(false);
                }
              }}
              onFocus={() => countrySuggestions.length > 0 && setShowCountrySuggestions(true)}
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
                    onClick={() => {
                      setCountryFilter(suggestion.country);
                      setShowCountrySuggestions(false);
                    }}
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
              onChange={(e) => setCitySearchInput(e.target.value)}
              onKeyDown={(e) => {
                if (e.key === 'Enter' && citySuggestions.length > 0) {
                  const city = citySuggestions[0].city;
                  if (!selectedCities.includes(city)) {
                    setSelectedCities([...selectedCities, city]);
                  }
                  setCitySearchInput('');
                  setShowCitySuggestions(false);
                } else if (e.key === 'Escape') {
                  setShowCitySuggestions(false);
                }
              }}
              onFocus={() => citySuggestions.length > 0 && setShowCitySuggestions(true)}
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
                onClick={() => setSelectedCities([])}
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
                      onClick={() => {
                        if (isSelected) {
                          setSelectedCities(selectedCities.filter(c => c !== suggestion.city));
                        } else {
                          setSelectedCities([...selectedCities, suggestion.city]);
                        }
                        setCitySearchInput('');
                        setShowCitySuggestions(false);
                      }}
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
              onChange={(e) => setCategoryFilter(e.target.value)}
              onKeyDown={(e) => {
                if (e.key === 'Enter' && categorySuggestions.length > 0) {
                  setCategoryFilter(categorySuggestions[0].event_category);
                  setShowCategorySuggestions(false);
                  loadEvents();
                } else if (e.key === 'Enter') {
                  loadEvents();
                } else if (e.key === 'Escape') {
                  setShowCategorySuggestions(false);
                }
              }}
              onFocus={() => categorySuggestions.length > 0 && setShowCategorySuggestions(true)}
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
                    onClick={() => {
                      setCategoryFilter(suggestion.event_category);
                      setShowCategorySuggestions(false);
                    }}
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
            onClick={loadEvents}
            disabled={isLoading}
            style={{
              padding: '5px 14px',
              background: THEME.ORANGE,
              border: 'none',
              color: '#000',
              fontFamily: 'Consolas, monospace',
              fontSize: '10px',
              fontWeight: 'bold',
              cursor: isLoading ? 'not-allowed' : 'pointer',
              opacity: isLoading ? 0.5 : 1,
              borderRadius: '2px',
              minWidth: '60px'
            }}
          >
            {isLoading ? 'LOADING...' : 'APPLY'}
          </button>

          {(countryFilter || cityFilter || selectedCities.length > 0 || categoryFilter) && (
            <button
              onClick={() => {
                setCountryFilter('Ukraine');
                setCityFilter('');
                setSelectedCities([]);
                setCitySearchInput('');
                setCategoryFilter('');
              }}
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
                    onClick={() => setSelectedCities(selectedCities.filter(c => c !== city))}
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
      {error && !error.includes('No events found for:') && (
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
          zIndex: 10
        }}>
          ERROR: {error}
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
            onClick={() => setIsPanelCollapsed(!isPanelCollapsed)}
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
            {/* Events Loaded - First */}
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
                onClick={() => setSelectedEvent(null)}
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
          onViewStateChange={({ viewState: vs }: any) => setViewState(vs)}
          controller={true}
          layers={layers}
          width="100%"
          height="100%"
          style={{ position: 'absolute', inset: 0 }}
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

export default GeopoliticsTab;
