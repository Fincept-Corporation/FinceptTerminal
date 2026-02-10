import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { binomialApi, poissonApi, geometricApi, hypergeometricApi, negBinomialApi, pgfApi } from '../quantlibStatisticsApi';

export default function DiscreteDistPanel() {
  // --- Binomial ---
  const binProp = useEndpoint();
  const binPmf = useEndpoint();
  const binCdf = useEndpoint();
  const [binN, setBinN] = useState('10');
  const [binP, setBinP] = useState('0.5');
  const [binK, setBinK] = useState('5');
  const [binAction, setBinAction] = useState('properties');

  // --- Poisson ---
  const poiProp = useEndpoint();
  const poiPmf = useEndpoint();
  const poiCdf = useEndpoint();
  const [poiLam, setPoiLam] = useState('3');
  const [poiK, setPoiK] = useState('2');
  const [poiAction, setPoiAction] = useState('properties');

  // --- Geometric ---
  const geoProp = useEndpoint();
  const geoPmf = useEndpoint();
  const geoCdf = useEndpoint();
  const geoPpf = useEndpoint();
  const [geoP, setGeoP] = useState('0.3');
  const [geoK, setGeoK] = useState('3');
  const [geoQ, setGeoQ] = useState('0.5');
  const [geoAction, setGeoAction] = useState('properties');

  // --- Hypergeometric ---
  const hypProp = useEndpoint();
  const hypPmf = useEndpoint();
  const hypCdf = useEndpoint();
  const [hypN, setHypN] = useState('50');
  const [hypK, setHypK] = useState('10');
  const [hypn, setHypn] = useState('5');
  const [hypk, setHypk] = useState('2');
  const [hypAction, setHypAction] = useState('properties');

  // --- Negative Binomial ---
  const nbProp = useEndpoint();
  const nbPmf = useEndpoint();
  const nbCdf = useEndpoint();
  const [nbR, setNbR] = useState('5');
  const [nbP, setNbP] = useState('0.5');
  const [nbK, setNbK] = useState('3');
  const [nbAction, setNbAction] = useState('properties');

  // --- PGF ---
  const pgf = useEndpoint();
  const [pgfDist, setPgfDist] = useState('binomial');
  const [pgfZ, setPgfZ] = useState('0.5');
  const [pgfN, setPgfN] = useState('10');
  const [pgfP, setPgfP] = useState('0.5');
  const [pgfLam, setPgfLam] = useState('3');

  return (
    <>
      {/* Binomial */}
      <EndpointCard title="Binomial Distribution" description="Bin(n, p) — properties, PMF, CDF">
        <Row>
          <Field label="n"><Input value={binN} onChange={setBinN} type="number" /></Field>
          <Field label="p"><Input value={binP} onChange={setBinP} type="number" /></Field>
          <Field label="Action"><Select value={binAction} onChange={setBinAction} options={['properties', 'pmf', 'cdf']} /></Field>
          {(binAction === 'pmf' || binAction === 'cdf') && <Field label="k"><Input value={binK} onChange={setBinK} type="number" /></Field>}
          <RunButton loading={binAction === 'properties' ? binProp.loading : binAction === 'pmf' ? binPmf.loading : binCdf.loading}
            onClick={() => {
              if (binAction === 'properties') binProp.run(() => binomialApi.properties(Number(binN), Number(binP)));
              else if (binAction === 'pmf') binPmf.run(() => binomialApi.pmf(Number(binN), Number(binP), Number(binK)));
              else binCdf.run(() => binomialApi.cdf(Number(binN), Number(binP), Number(binK)));
            }} />
        </Row>
        <ResultPanel data={binAction === 'properties' ? binProp.result : binAction === 'pmf' ? binPmf.result : binCdf.result}
          error={binAction === 'properties' ? binProp.error : binAction === 'pmf' ? binPmf.error : binCdf.error} />
      </EndpointCard>

      {/* Poisson */}
      <EndpointCard title="Poisson Distribution" description="Poi(lambda) — properties, PMF, CDF">
        <Row>
          <Field label="Lambda"><Input value={poiLam} onChange={setPoiLam} type="number" /></Field>
          <Field label="Action"><Select value={poiAction} onChange={setPoiAction} options={['properties', 'pmf', 'cdf']} /></Field>
          {(poiAction === 'pmf' || poiAction === 'cdf') && <Field label="k"><Input value={poiK} onChange={setPoiK} type="number" /></Field>}
          <RunButton loading={poiAction === 'properties' ? poiProp.loading : poiAction === 'pmf' ? poiPmf.loading : poiCdf.loading}
            onClick={() => {
              if (poiAction === 'properties') poiProp.run(() => poissonApi.properties(Number(poiLam)));
              else if (poiAction === 'pmf') poiPmf.run(() => poissonApi.pmf(Number(poiLam), Number(poiK)));
              else poiCdf.run(() => poissonApi.cdf(Number(poiLam), Number(poiK)));
            }} />
        </Row>
        <ResultPanel data={poiAction === 'properties' ? poiProp.result : poiAction === 'pmf' ? poiPmf.result : poiCdf.result}
          error={poiAction === 'properties' ? poiProp.error : poiAction === 'pmf' ? poiPmf.error : poiCdf.error} />
      </EndpointCard>

      {/* Geometric */}
      <EndpointCard title="Geometric Distribution" description="Geo(p) — properties, PMF, CDF, PPF">
        <Row>
          <Field label="p"><Input value={geoP} onChange={setGeoP} type="number" /></Field>
          <Field label="Action"><Select value={geoAction} onChange={setGeoAction} options={['properties', 'pmf', 'cdf', 'ppf']} /></Field>
          {(geoAction === 'pmf' || geoAction === 'cdf') && <Field label="k"><Input value={geoK} onChange={setGeoK} type="number" /></Field>}
          {geoAction === 'ppf' && <Field label="q"><Input value={geoQ} onChange={setGeoQ} type="number" /></Field>}
          <RunButton loading={geoAction === 'properties' ? geoProp.loading : geoAction === 'pmf' ? geoPmf.loading : geoAction === 'cdf' ? geoCdf.loading : geoPpf.loading}
            onClick={() => {
              if (geoAction === 'properties') geoProp.run(() => geometricApi.properties(Number(geoP)));
              else if (geoAction === 'pmf') geoPmf.run(() => geometricApi.pmf(Number(geoP), Number(geoK)));
              else if (geoAction === 'cdf') geoCdf.run(() => geometricApi.cdf(Number(geoP), Number(geoK)));
              else geoPpf.run(() => geometricApi.ppf(Number(geoP), Number(geoQ)));
            }} />
        </Row>
        <ResultPanel data={geoAction === 'properties' ? geoProp.result : geoAction === 'pmf' ? geoPmf.result : geoAction === 'cdf' ? geoCdf.result : geoPpf.result}
          error={geoAction === 'properties' ? geoProp.error : geoAction === 'pmf' ? geoPmf.error : geoAction === 'cdf' ? geoCdf.error : geoPpf.error} />
      </EndpointCard>

      {/* Hypergeometric */}
      <EndpointCard title="Hypergeometric Distribution" description="HyperGeo(N, K, n) — properties, PMF, CDF">
        <Row>
          <Field label="N (pop)"><Input value={hypN} onChange={setHypN} type="number" /></Field>
          <Field label="K (success)"><Input value={hypK} onChange={setHypK} type="number" /></Field>
          <Field label="n (draws)"><Input value={hypn} onChange={setHypn} type="number" /></Field>
          <Field label="Action"><Select value={hypAction} onChange={setHypAction} options={['properties', 'pmf', 'cdf']} /></Field>
          {(hypAction === 'pmf' || hypAction === 'cdf') && <Field label="k"><Input value={hypk} onChange={setHypk} type="number" /></Field>}
          <RunButton loading={hypAction === 'properties' ? hypProp.loading : hypAction === 'pmf' ? hypPmf.loading : hypCdf.loading}
            onClick={() => {
              if (hypAction === 'properties') hypProp.run(() => hypergeometricApi.properties(Number(hypN), Number(hypK), Number(hypn)));
              else if (hypAction === 'pmf') hypPmf.run(() => hypergeometricApi.pmf(Number(hypN), Number(hypK), Number(hypn), Number(hypk)));
              else hypCdf.run(() => hypergeometricApi.cdf(Number(hypN), Number(hypK), Number(hypn), Number(hypk)));
            }} />
        </Row>
        <ResultPanel data={hypAction === 'properties' ? hypProp.result : hypAction === 'pmf' ? hypPmf.result : hypCdf.result}
          error={hypAction === 'properties' ? hypProp.error : hypAction === 'pmf' ? hypPmf.error : hypCdf.error} />
      </EndpointCard>

      {/* Negative Binomial */}
      <EndpointCard title="Negative Binomial Distribution" description="NegBin(r, p) — properties, PMF, CDF">
        <Row>
          <Field label="r"><Input value={nbR} onChange={setNbR} type="number" /></Field>
          <Field label="p"><Input value={nbP} onChange={setNbP} type="number" /></Field>
          <Field label="Action"><Select value={nbAction} onChange={setNbAction} options={['properties', 'pmf', 'cdf']} /></Field>
          {(nbAction === 'pmf' || nbAction === 'cdf') && <Field label="k"><Input value={nbK} onChange={setNbK} type="number" /></Field>}
          <RunButton loading={nbAction === 'properties' ? nbProp.loading : nbAction === 'pmf' ? nbPmf.loading : nbCdf.loading}
            onClick={() => {
              if (nbAction === 'properties') nbProp.run(() => negBinomialApi.properties(Number(nbR), Number(nbP)));
              else if (nbAction === 'pmf') nbPmf.run(() => negBinomialApi.pmf(Number(nbR), Number(nbP), Number(nbK)));
              else nbCdf.run(() => negBinomialApi.cdf(Number(nbR), Number(nbP), Number(nbK)));
            }} />
        </Row>
        <ResultPanel data={nbAction === 'properties' ? nbProp.result : nbAction === 'pmf' ? nbPmf.result : nbCdf.result}
          error={nbAction === 'properties' ? nbProp.error : nbAction === 'pmf' ? nbPmf.error : nbCdf.error} />
      </EndpointCard>

      {/* PGF */}
      <EndpointCard title="Probability Generating Function" description="G(z) for discrete distributions">
        <Row>
          <Field label="Distribution"><Select value={pgfDist} onChange={setPgfDist} options={['binomial', 'poisson', 'geometric', 'negative_binomial']} /></Field>
          <Field label="z"><Input value={pgfZ} onChange={setPgfZ} type="number" /></Field>
          {(pgfDist === 'binomial' || pgfDist === 'negative_binomial') && <Field label="n/r"><Input value={pgfN} onChange={setPgfN} type="number" /></Field>}
          {(pgfDist === 'binomial' || pgfDist === 'geometric' || pgfDist === 'negative_binomial') && <Field label="p"><Input value={pgfP} onChange={setPgfP} type="number" /></Field>}
          {pgfDist === 'poisson' && <Field label="Lambda"><Input value={pgfLam} onChange={setPgfLam} type="number" /></Field>}
          <RunButton loading={pgf.loading} onClick={() => pgf.run(() => pgfApi.evaluate(pgfDist, Number(pgfZ),
            (pgfDist === 'binomial' || pgfDist === 'negative_binomial') ? Number(pgfN) : undefined,
            (pgfDist !== 'poisson') ? Number(pgfP) : undefined,
            pgfDist === 'poisson' ? Number(pgfLam) : undefined
          ))} />
        </Row>
        <ResultPanel data={pgf.result} error={pgf.error} />
      </EndpointCard>
    </>
  );
}
