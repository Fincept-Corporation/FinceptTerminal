// CountrySelector Component - Select country for data fetching

import React from 'react';
import { Search } from 'lucide-react';
import { FINCEPT, COUNTRIES } from '../constants';

interface CountrySelectorProps {
  selectedCountry: string;
  setSelectedCountry: (code: string) => void;
  searchCountry: string;
  setSearchCountry: (search: string) => void;
}

export function CountrySelector({
  selectedCountry,
  setSelectedCountry,
  searchCountry,
  setSearchCountry,
}: CountrySelectorProps) {
  const filteredCountries = COUNTRIES.filter(
    (c) =>
      c.name.toLowerCase().includes(searchCountry.toLowerCase()) ||
      c.code.toLowerCase().includes(searchCountry.toLowerCase())
  );

  return (
    <div style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
      <div
        style={{
          padding: '10px 12px',
          backgroundColor: FINCEPT.HEADER_BG,
          fontSize: '9px',
          fontWeight: 700,
          color: FINCEPT.WHITE,
          letterSpacing: '0.5px',
          display: 'flex',
          justifyContent: 'space-between',
        }}
      >
        <span>COUNTRY</span>
        <span style={{ color: FINCEPT.MUTED }}>{COUNTRIES.length}</span>
      </div>
      <div style={{ padding: '8px' }}>
        <div style={{ position: 'relative', marginBottom: '8px' }}>
          <Search size={12} style={{ position: 'absolute', left: '10px', top: '50%', transform: 'translateY(-50%)', color: FINCEPT.MUTED }} />
          <input
            type="text"
            placeholder="Search countries..."
            value={searchCountry}
            onChange={(e) => setSearchCountry(e.target.value)}
            style={{
              width: '100%',
              padding: '8px 10px 8px 30px',
              backgroundColor: FINCEPT.DARK_BG,
              color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              fontSize: '10px',
              fontFamily: 'inherit',
              outline: 'none',
            }}
          />
        </div>
        <div style={{ maxHeight: '180px', overflowY: 'auto' }}>
          {filteredCountries.map((country) => (
            <div
              key={country.code}
              onClick={() => setSelectedCountry(country.code)}
              style={{
                padding: '6px 10px',
                backgroundColor: selectedCountry === country.code ? `${FINCEPT.ORANGE}15` : 'transparent',
                borderLeft: selectedCountry === country.code ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                cursor: 'pointer',
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
              }}
            >
              <span style={{ fontSize: '10px', color: selectedCountry === country.code ? FINCEPT.WHITE : FINCEPT.GRAY }}>
                {country.name}
              </span>
              <span style={{ fontSize: '9px', color: FINCEPT.YELLOW, fontWeight: 700 }}>{country.code}</span>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}

export default CountrySelector;
