// Maritime Tab Constants

import type { TradeRoute, PresetLocation, OceanRoute, AirRoute } from './types';

// Trade Routes for display in the sidebar
export const TRADE_ROUTES: TradeRoute[] = [
  { name: 'Mumbai → Rotterdam', value: '$45B', status: 'active', vessels: 23, coordinates: [[18.9388, 72.8354], [51.9553, 4.1392]] },
  { name: 'Mumbai → Shanghai', value: '$156B', status: 'active', vessels: 89, coordinates: [[18.9388, 72.8354], [31.3548, 121.6431]] },
  { name: 'Mumbai → Singapore', value: '$89B', status: 'active', vessels: 45, coordinates: [[18.9388, 72.8354], [1.2644, 103.8224]] },
  { name: 'Chennai → Tokyo', value: '$67B', status: 'delayed', vessels: 34, coordinates: [[13.0827, 80.2707], [35.4437, 139.6380]] },
  { name: 'Kolkata → Hong Kong', value: '$45B', status: 'active', vessels: 28, coordinates: [[22.5726, 88.3639], [22.2855, 114.1577]] },
  { name: 'Mumbai → Dubai', value: '$78B', status: 'critical', vessels: 12, coordinates: [[18.9388, 72.8354], [24.9857, 55.0272]] },
  { name: 'Mumbai → New York', value: '$123B', status: 'active', vessels: 56, coordinates: [[18.9388, 72.8354], [40.6655, -74.0781]] },
  { name: 'Chennai → Sydney', value: '$54B', status: 'active', vessels: 31, coordinates: [[13.0827, 80.2707], [-33.9544, 151.2093]] },
  { name: 'Cochin → Klang', value: '$34B', status: 'active', vessels: 19, coordinates: [[9.9667, 76.2667], [3.0048, 101.3975]] },
  { name: 'Mumbai → Cape Town', value: '$28B', status: 'delayed', vessels: 8, coordinates: [[18.9388, 72.8354], [-33.9055, 18.4232]] }
];

// Preset locations for quick marker addition
export const PRESET_LOCATIONS: PresetLocation[] = [
  { name: 'Mumbai Port', lat: 18.9388, lng: 72.8354, type: 'Port' },
  { name: 'Shanghai Port', lat: 31.3548, lng: 121.6431, type: 'Port' },
  { name: 'Singapore Port', lat: 1.2644, lng: 103.8224, type: 'Port' },
  { name: 'Hong Kong Port', lat: 22.2855, lng: 114.1577, type: 'Port' },
  { name: 'Rotterdam Port', lat: 51.9553, lng: 4.1392, type: 'Port' },
  { name: 'Dubai Port', lat: 24.9857, lng: 55.0272, type: 'Port' },
];

// Ocean routes for 3D globe visualization
export const OCEAN_ROUTES: OceanRoute[] = [
  { name: "Mumbai → Rotterdam", start: {lat: 18.9388, lng: 72.8354}, end: {lat: 51.9553, lng: 4.1392}, value: "$45B", vessels: 23, color: 'rgba(100, 180, 255, 0.6)', width: 0.8 },
  { name: "Mumbai → Shanghai", start: {lat: 18.9388, lng: 72.8354}, end: {lat: 31.3548, lng: 121.6431}, value: "$156B", vessels: 89, color: 'rgba(120, 200, 255, 0.7)', width: 1.2 },
  { name: "Mumbai → Singapore", start: {lat: 18.9388, lng: 72.8354}, end: {lat: 1.2644, lng: 103.8224}, value: "$89B", vessels: 45, color: 'rgba(100, 180, 255, 0.65)', width: 1.0 },
  { name: "Chennai → Tokyo", start: {lat: 13.0827, lng: 80.2707}, end: {lat: 35.4437, lng: 139.6380}, value: "$67B", vessels: 34, color: 'rgba(150, 220, 255, 0.6)', width: 0.9 },
  { name: "Kolkata → Hong Kong", start: {lat: 22.5726, lng: 88.3639}, end: {lat: 22.2855, lng: 114.1577}, value: "$45B", vessels: 28, color: 'rgba(100, 180, 255, 0.55)', width: 0.8 },
  { name: "Mumbai → Dubai", start: {lat: 18.9388, lng: 72.8354}, end: {lat: 24.9857, lng: 55.0272}, value: "$78B", vessels: 12, color: 'rgba(255, 160, 100, 0.6)', width: 0.9 },
  { name: "Mumbai → New York", start: {lat: 18.9388, lng: 72.8354}, end: {lat: 40.6655, lng: -74.0781}, value: "$123B", vessels: 56, color: 'rgba(120, 200, 255, 0.75)', width: 1.2 },
  { name: "Chennai → Sydney", start: {lat: 13.0827, lng: 80.2707}, end: {lat: -33.9544, lng: 151.2093}, value: "$54B", vessels: 31, color: 'rgba(130, 190, 255, 0.6)', width: 0.9 },
  { name: "Cochin → Klang", start: {lat: 9.9667, lng: 76.2667}, end: {lat: 3.0048, lng: 101.3975}, value: "$34B", vessels: 19, color: 'rgba(180, 200, 255, 0.5)', width: 0.7 },
  { name: "Mumbai → Cape Town", start: {lat: 18.9388, lng: 72.8354}, end: {lat: -33.9055, lng: 18.4232}, value: "$28B", vessels: 8, color: 'rgba(160, 180, 255, 0.5)', width: 0.7 }
];

// Air routes for 3D globe visualization
export const AIR_ROUTES: AirRoute[] = [
  { name: "Mumbai → London", start: {lat: 19.0896, lng: 72.8656}, end: {lat: 51.4700, lng: -0.4543}, value: "$12B", flights: 45, color: 'rgba(255, 200, 100, 0.7)', width: 0.6 },
  { name: "Delhi → New York", start: {lat: 28.5562, lng: 77.1000}, end: {lat: 40.6413, lng: -73.7781}, value: "$18B", flights: 67, color: 'rgba(255, 200, 100, 0.75)', width: 0.8 },
  { name: "Mumbai → Dubai", start: {lat: 19.0896, lng: 72.8656}, end: {lat: 25.2532, lng: 55.3657}, value: "$22B", flights: 123, color: 'rgba(255, 200, 100, 0.8)', width: 0.9 },
  { name: "Bangalore → Singapore", start: {lat: 13.1979, lng: 77.7063}, end: {lat: 1.3644, lng: 103.9915}, value: "$8B", flights: 89, color: 'rgba(255, 200, 100, 0.65)', width: 0.6 },
  { name: "Delhi → Frankfurt", start: {lat: 28.5562, lng: 77.1000}, end: {lat: 50.0379, lng: 8.5622}, value: "$15B", flights: 56, color: 'rgba(255, 200, 100, 0.7)', width: 0.7 },
  { name: "Mumbai → Hong Kong", start: {lat: 19.0896, lng: 72.8656}, end: {lat: 22.3080, lng: 113.9185}, value: "$11B", flights: 78, color: 'rgba(255, 200, 100, 0.65)', width: 0.6 },
  { name: "Chennai → Tokyo", start: {lat: 12.9941, lng: 80.1709}, end: {lat: 35.7720, lng: 140.3929}, value: "$9B", flights: 34, color: 'rgba(255, 200, 100, 0.6)', width: 0.6 },
  { name: "Delhi → Sydney", start: {lat: 28.5562, lng: 77.1000}, end: {lat: -33.9399, lng: 151.1753}, value: "$7B", flights: 28, color: 'rgba(255, 200, 100, 0.6)', width: 0.5 }
];

// Default intelligence data
export const DEFAULT_INTELLIGENCE = {
  timestamp: new Date().toISOString(),
  threat_level: 'low' as const,
  active_vessels: 0,
  monitored_routes: 48,
  trade_volume: '$847.3B'
};

// API search area (Mumbai port region)
export const MUMBAI_SEARCH_AREA = {
  min_lat: 18.5,
  max_lat: 19.5,
  min_lng: 72.0,
  max_lng: 73.5
};

// Refresh interval for vessel data (5 minutes)
export const VESSEL_REFRESH_INTERVAL = 300000;

// Timestamp update interval (5 seconds)
export const TIMESTAMP_UPDATE_INTERVAL = 5000;

// Maximum vessels to display for performance
export const MAX_VESSELS_DISPLAY = 500;

// Marker type icons
export const MARKER_ICONS: Record<string, string> = {
  'Ship': '🚢',
  'Port': '⚓',
  'Industry': '🏭',
  'Bank': '🏦',
  'Exchange': '💱'
};
