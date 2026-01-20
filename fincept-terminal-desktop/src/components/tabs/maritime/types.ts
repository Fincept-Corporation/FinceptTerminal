// Maritime Tab Types

export interface MarkerData {
  lat: number;
  lng: number;
  title: string;
  type: 'Ship' | 'Port' | 'Industry' | 'Bank' | 'Exchange';
}

export interface TradeRoute {
  name: string;
  value: string;
  status: 'active' | 'delayed' | 'critical';
  vessels: number;
  coordinates: [number, number][];
}

export interface IntelligenceData {
  timestamp: string;
  threat_level: 'low' | 'medium' | 'high' | 'critical';
  active_vessels: number;
  monitored_routes: number;
  trade_volume: string;
}

export interface RealVesselData {
  imo: string;
  name: string;
  lat: number;
  lng: number;
  speed: string;
  angle: string;
  from_port?: string;
  to_port?: string;
  route_progress?: number;
}

export interface PresetLocation {
  name: string;
  lat: number;
  lng: number;
  type: 'Port' | 'Ship' | 'Industry' | 'Bank' | 'Exchange';
}

export interface OceanRoute {
  name: string;
  start: { lat: number; lng: number };
  end: { lat: number; lng: number };
  value: string;
  vessels: number;
  color: string;
  width: number;
}

export interface AirRoute {
  name: string;
  start: { lat: number; lng: number };
  end: { lat: number; lng: number };
  value: string;
  flights: number;
  color: string;
  width: number;
}

export type MapMode = '3D' | '2D';
