/**
 * GeopoliticsTab - State Machine, Types, and Constants
 */

import { NewsEvent } from '@/services/news/newsEventsService';

// ============================================================================
// Constants
// ============================================================================

export const INITIAL_VIEW_STATE = {
  longitude: 30.0,
  latitude: 20.0,
  zoom: 2,
  pitch: 0,
  bearing: 0
};

export const API_TIMEOUT_MS = 30000;
export const REFRESH_INTERVAL_MS = 300000; // 5 minutes

// ============================================================================
// Types
// ============================================================================

export interface EventPoint {
  position: [number, number, number];
  color: [number, number, number, number];
  event: NewsEvent;
}

export interface ViewState {
  longitude: number;
  latitude: number;
  zoom: number;
  pitch: number;
  bearing: number;
}

// ============================================================================
// State Machine
// ============================================================================

export interface GeoState {
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

export type GeoAction =
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

export const initialState: GeoState = {
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

export function geoReducer(state: GeoState, action: GeoAction): GeoState {
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

export function getCategoryColor(category?: string): [number, number, number, number] {
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
