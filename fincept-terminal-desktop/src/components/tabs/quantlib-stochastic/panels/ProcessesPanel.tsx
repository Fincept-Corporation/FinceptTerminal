import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { processesApi } from '../quantlibStochasticApi';

export default function ProcessesPanel() {
  // GBM Simulate
  const gbm = useEndpoint();
  const [gbmS0, setGbmS0] = useState('100');
  const [gbmMu, setGbmMu] = useState('0.05');
  const [gbmSigma, setGbmSigma] = useState('0.2');
  const [gbmT, setGbmT] = useState('1');
  const [gbmSteps, setGbmSteps] = useState('252');
  const [gbmPaths, setGbmPaths] = useState('5');

  // GBM Properties
  const gbmP = useEndpoint();
  const [gpS0, setGpS0] = useState('100');
  const [gpMu, setGpMu] = useState('0.05');
  const [gpSigma, setGpSigma] = useState('0.2');
  const [gpT, setGpT] = useState('1');

  // OU
  const ou = useEndpoint();
  const [ouX0, setOuX0] = useState('0.05');
  const [ouKappa, setOuKappa] = useState('5');
  const [ouTheta, setOuTheta] = useState('0.04');
  const [ouSigma, setOuSigma] = useState('0.01');
  const [ouT, setOuT] = useState('1');
  const [ouSteps, setOuSteps] = useState('252');
  const [ouPaths, setOuPaths] = useState('3');

  // CIR Simulate
  const cir = useEndpoint();
  const [cirR0, setCirR0] = useState('0.03');
  const [cirKappa, setCirKappa] = useState('0.5');
  const [cirTheta, setCirTheta] = useState('0.05');
  const [cirSigma, setCirSigma] = useState('0.01');
  const [cirT, setCirT] = useState('1');
  const [cirSteps, setCirSteps] = useState('252');
  const [cirPaths, setCirPaths] = useState('3');

  // CIR Bond Price
  const cirBP = useEndpoint();
  const [cbR0, setCbR0] = useState('0.03');
  const [cbKappa, setCbKappa] = useState('0.5');
  const [cbTheta, setCbTheta] = useState('0.05');
  const [cbSigma, setCbSigma] = useState('0.01');
  const [cbT, setCbT] = useState('5');

  // Vasicek Simulate
  const vas = useEndpoint();
  const [vasR0, setVasR0] = useState('0.03');
  const [vasKappa, setVasKappa] = useState('0.3');
  const [vasTheta, setVasTheta] = useState('0.05');
  const [vasSigma, setVasSigma] = useState('0.01');
  const [vasT, setVasT] = useState('1');
  const [vasSteps, setVasSteps] = useState('252');
  const [vasPaths, setVasPaths] = useState('3');

  // Vasicek Bond
  const vasB = useEndpoint();
  const [vbR0, setVbR0] = useState('0.03');
  const [vbKappa, setVbKappa] = useState('0.3');
  const [vbTheta, setVbTheta] = useState('0.05');
  const [vbSigma, setVbSigma] = useState('0.01');
  const [vbT, setVbT] = useState('5');

  // Heston
  const hes = useEndpoint();
  const [hesS0, setHesS0] = useState('100');
  const [hesV0, setHesV0] = useState('0.04');
  const [hesR, setHesR] = useState('0.05');
  const [hesKappa, setHesKappa] = useState('2');
  const [hesTheta, setHesTheta] = useState('0.04');
  const [hesSigV, setHesSigV] = useState('0.3');
  const [hesRho, setHesRho] = useState('-0.7');
  const [hesT, setHesT] = useState('1');
  const [hesPaths, setHesPaths] = useState('3');

  // Merton
  const mer = useEndpoint();
  const [merS0, setMerS0] = useState('100');
  const [merMu, setMerMu] = useState('0.05');
  const [merSigma, setMerSigma] = useState('0.2');
  const [merLam, setMerLam] = useState('1');
  const [merJMean, setMerJMean] = useState('-0.1');
  const [merJStd, setMerJStd] = useState('0.15');
  const [merT, setMerT] = useState('1');
  const [merPaths, setMerPaths] = useState('3');

  // Poisson
  const poi = useEndpoint();
  const [poiLam, setPoiLam] = useState('5');
  const [poiT, setPoiT] = useState('1');
  const [poiSteps, setPoiSteps] = useState('1000');
  const [poiPaths, setPoiPaths] = useState('3');

  // Wiener
  const wie = useEndpoint();
  const [wieT, setWieT] = useState('1');
  const [wieSteps, setWieSteps] = useState('1000');
  const [wiePaths, setWiePaths] = useState('5');

  // Brownian Bridge
  const bb = useEndpoint();
  const [bbT, setBbT] = useState('1');
  const [bbA, setBbA] = useState('0');
  const [bbB, setBbB] = useState('0');
  const [bbSteps, setBbSteps] = useState('1000');

  // Correlated BM
  const cbm = useEndpoint();
  const [cbmN, setCbmN] = useState('2');
  const [cbmCorr, setCbmCorr] = useState('[[1,0.5],[0.5,1]]');
  const [cbmT, setCbmT] = useState('1');
  const [cbmSteps, setCbmSteps] = useState('252');

  // Variance Gamma
  const vg = useEndpoint();
  const [vgS0, setVgS0] = useState('100');
  const [vgR, setVgR] = useState('0.05');
  const [vgSigma, setVgSigma] = useState('0.2');
  const [vgTheta, setVgTheta] = useState('-0.1');
  const [vgNu, setVgNu] = useState('0.2');
  const [vgT, setVgT] = useState('1');
  const [vgPaths, setVgPaths] = useState('3');

  return (
    <>
      <EndpointCard title="GBM Simulate" description="Geometric Brownian Motion paths">
        <Row>
          <Field label="S0"><Input value={gbmS0} onChange={setGbmS0} type="number" /></Field>
          <Field label="Mu"><Input value={gbmMu} onChange={setGbmMu} type="number" /></Field>
          <Field label="Sigma"><Input value={gbmSigma} onChange={setGbmSigma} type="number" /></Field>
          <Field label="T"><Input value={gbmT} onChange={setGbmT} type="number" /></Field>
          <Field label="Steps"><Input value={gbmSteps} onChange={setGbmSteps} type="number" /></Field>
          <Field label="Paths"><Input value={gbmPaths} onChange={setGbmPaths} type="number" /></Field>
          <RunButton loading={gbm.loading} onClick={() => gbm.run(() => processesApi.gbmSimulate(Number(gbmS0), Number(gbmMu), Number(gbmSigma), Number(gbmT), Number(gbmSteps), Number(gbmPaths)))} />
        </Row>
        <ResultPanel data={gbm.result} error={gbm.error} />
      </EndpointCard>

      <EndpointCard title="GBM Properties" description="Analytical mean, var, quantiles">
        <Row>
          <Field label="S0"><Input value={gpS0} onChange={setGpS0} type="number" /></Field>
          <Field label="Mu"><Input value={gpMu} onChange={setGpMu} type="number" /></Field>
          <Field label="Sigma"><Input value={gpSigma} onChange={setGpSigma} type="number" /></Field>
          <Field label="T"><Input value={gpT} onChange={setGpT} type="number" /></Field>
          <RunButton loading={gbmP.loading} onClick={() => gbmP.run(() => processesApi.gbmProperties(Number(gpS0), Number(gpMu), Number(gpSigma), Number(gpT)))} />
        </Row>
        <ResultPanel data={gbmP.result} error={gbmP.error} />
      </EndpointCard>

      <EndpointCard title="OU Process" description="Ornstein-Uhlenbeck simulation">
        <Row>
          <Field label="X0"><Input value={ouX0} onChange={setOuX0} type="number" /></Field>
          <Field label="Kappa"><Input value={ouKappa} onChange={setOuKappa} type="number" /></Field>
          <Field label="Theta"><Input value={ouTheta} onChange={setOuTheta} type="number" /></Field>
          <Field label="Sigma"><Input value={ouSigma} onChange={setOuSigma} type="number" /></Field>
          <Field label="T"><Input value={ouT} onChange={setOuT} type="number" /></Field>
          <Field label="Steps"><Input value={ouSteps} onChange={setOuSteps} type="number" /></Field>
          <Field label="Paths"><Input value={ouPaths} onChange={setOuPaths} type="number" /></Field>
          <RunButton loading={ou.loading} onClick={() => ou.run(() => processesApi.ouSimulate(Number(ouX0), Number(ouKappa), Number(ouTheta), Number(ouSigma), Number(ouT), Number(ouSteps), Number(ouPaths)))} />
        </Row>
        <ResultPanel data={ou.result} error={ou.error} />
      </EndpointCard>

      <EndpointCard title="CIR Simulate" description="Cox-Ingersoll-Ross paths">
        <Row>
          <Field label="r0"><Input value={cirR0} onChange={setCirR0} type="number" /></Field>
          <Field label="Kappa"><Input value={cirKappa} onChange={setCirKappa} type="number" /></Field>
          <Field label="Theta"><Input value={cirTheta} onChange={setCirTheta} type="number" /></Field>
          <Field label="Sigma"><Input value={cirSigma} onChange={setCirSigma} type="number" /></Field>
          <Field label="T"><Input value={cirT} onChange={setCirT} type="number" /></Field>
          <Field label="Steps"><Input value={cirSteps} onChange={setCirSteps} type="number" /></Field>
          <Field label="Paths"><Input value={cirPaths} onChange={setCirPaths} type="number" /></Field>
          <RunButton loading={cir.loading} onClick={() => cir.run(() => processesApi.cirSimulate(Number(cirR0), Number(cirKappa), Number(cirTheta), Number(cirSigma), Number(cirT), Number(cirSteps), Number(cirPaths)))} />
        </Row>
        <ResultPanel data={cir.result} error={cir.error} />
      </EndpointCard>

      <EndpointCard title="CIR Bond Price" description="Zero-coupon bond from CIR model">
        <Row>
          <Field label="r0"><Input value={cbR0} onChange={setCbR0} type="number" /></Field>
          <Field label="Kappa"><Input value={cbKappa} onChange={setCbKappa} type="number" /></Field>
          <Field label="Theta"><Input value={cbTheta} onChange={setCbTheta} type="number" /></Field>
          <Field label="Sigma"><Input value={cbSigma} onChange={setCbSigma} type="number" /></Field>
          <Field label="T"><Input value={cbT} onChange={setCbT} type="number" /></Field>
          <RunButton loading={cirBP.loading} onClick={() => cirBP.run(() => processesApi.cirBondPrice(Number(cbR0), Number(cbKappa), Number(cbTheta), Number(cbSigma), Number(cbT)))} />
        </Row>
        <ResultPanel data={cirBP.result} error={cirBP.error} />
      </EndpointCard>

      <EndpointCard title="Vasicek Simulate" description="Vasicek short rate paths">
        <Row>
          <Field label="r0"><Input value={vasR0} onChange={setVasR0} type="number" /></Field>
          <Field label="Kappa"><Input value={vasKappa} onChange={setVasKappa} type="number" /></Field>
          <Field label="Theta"><Input value={vasTheta} onChange={setVasTheta} type="number" /></Field>
          <Field label="Sigma"><Input value={vasSigma} onChange={setVasSigma} type="number" /></Field>
          <Field label="T"><Input value={vasT} onChange={setVasT} type="number" /></Field>
          <Field label="Steps"><Input value={vasSteps} onChange={setVasSteps} type="number" /></Field>
          <Field label="Paths"><Input value={vasPaths} onChange={setVasPaths} type="number" /></Field>
          <RunButton loading={vas.loading} onClick={() => vas.run(() => processesApi.vasicekSimulate(Number(vasR0), Number(vasKappa), Number(vasTheta), Number(vasSigma), Number(vasT), Number(vasSteps), Number(vasPaths)))} />
        </Row>
        <ResultPanel data={vas.result} error={vas.error} />
      </EndpointCard>

      <EndpointCard title="Vasicek Bond Price" description="Zero-coupon bond from Vasicek">
        <Row>
          <Field label="r0"><Input value={vbR0} onChange={setVbR0} type="number" /></Field>
          <Field label="Kappa"><Input value={vbKappa} onChange={setVbKappa} type="number" /></Field>
          <Field label="Theta"><Input value={vbTheta} onChange={setVbTheta} type="number" /></Field>
          <Field label="Sigma"><Input value={vbSigma} onChange={setVbSigma} type="number" /></Field>
          <Field label="T"><Input value={vbT} onChange={setVbT} type="number" /></Field>
          <RunButton loading={vasB.loading} onClick={() => vasB.run(() => processesApi.vasicekBondPrice(Number(vbR0), Number(vbKappa), Number(vbTheta), Number(vbSigma), Number(vbT)))} />
        </Row>
        <ResultPanel data={vasB.result} error={vasB.error} />
      </EndpointCard>

      <EndpointCard title="Heston Simulate" description="Stochastic volatility paths">
        <Row>
          <Field label="S0"><Input value={hesS0} onChange={setHesS0} type="number" /></Field>
          <Field label="v0"><Input value={hesV0} onChange={setHesV0} type="number" /></Field>
          <Field label="r"><Input value={hesR} onChange={setHesR} type="number" /></Field>
          <Field label="Kappa"><Input value={hesKappa} onChange={setHesKappa} type="number" /></Field>
          <Field label="Theta"><Input value={hesTheta} onChange={setHesTheta} type="number" /></Field>
          <Field label="Sigma_v"><Input value={hesSigV} onChange={setHesSigV} type="number" /></Field>
          <Field label="Rho"><Input value={hesRho} onChange={setHesRho} type="number" /></Field>
          <Field label="T"><Input value={hesT} onChange={setHesT} type="number" /></Field>
          <Field label="Paths"><Input value={hesPaths} onChange={setHesPaths} type="number" /></Field>
          <RunButton loading={hes.loading} onClick={() => hes.run(() => processesApi.hestonSimulate(Number(hesS0), Number(hesV0), Number(hesR), Number(hesKappa), Number(hesTheta), Number(hesSigV), Number(hesRho), Number(hesT), 252, Number(hesPaths)))} />
        </Row>
        <ResultPanel data={hes.result} error={hes.error} />
      </EndpointCard>

      <EndpointCard title="Merton Jump-Diffusion" description="Jump-diffusion simulation">
        <Row>
          <Field label="S0"><Input value={merS0} onChange={setMerS0} type="number" /></Field>
          <Field label="Mu"><Input value={merMu} onChange={setMerMu} type="number" /></Field>
          <Field label="Sigma"><Input value={merSigma} onChange={setMerSigma} type="number" /></Field>
          <Field label="Lambda"><Input value={merLam} onChange={setMerLam} type="number" /></Field>
          <Field label="Jump Mean"><Input value={merJMean} onChange={setMerJMean} type="number" /></Field>
          <Field label="Jump Std"><Input value={merJStd} onChange={setMerJStd} type="number" /></Field>
          <Field label="T"><Input value={merT} onChange={setMerT} type="number" /></Field>
          <Field label="Paths"><Input value={merPaths} onChange={setMerPaths} type="number" /></Field>
          <RunButton loading={mer.loading} onClick={() => mer.run(() => processesApi.mertonSimulate(Number(merS0), Number(merMu), Number(merSigma), Number(merLam), Number(merJMean), Number(merJStd), Number(merT), 252, Number(merPaths)))} />
        </Row>
        <ResultPanel data={mer.result} error={mer.error} />
      </EndpointCard>

      <EndpointCard title="Poisson Process" description="Poisson counting process">
        <Row>
          <Field label="Lambda"><Input value={poiLam} onChange={setPoiLam} type="number" /></Field>
          <Field label="T"><Input value={poiT} onChange={setPoiT} type="number" /></Field>
          <Field label="Steps"><Input value={poiSteps} onChange={setPoiSteps} type="number" /></Field>
          <Field label="Paths"><Input value={poiPaths} onChange={setPoiPaths} type="number" /></Field>
          <RunButton loading={poi.loading} onClick={() => poi.run(() => processesApi.poissonSimulate(Number(poiLam), Number(poiT), Number(poiSteps), Number(poiPaths)))} />
        </Row>
        <ResultPanel data={poi.result} error={poi.error} />
      </EndpointCard>

      <EndpointCard title="Wiener Process" description="Standard Brownian motion">
        <Row>
          <Field label="T"><Input value={wieT} onChange={setWieT} type="number" /></Field>
          <Field label="Steps"><Input value={wieSteps} onChange={setWieSteps} type="number" /></Field>
          <Field label="Paths"><Input value={wiePaths} onChange={setWiePaths} type="number" /></Field>
          <RunButton loading={wie.loading} onClick={() => wie.run(() => processesApi.wienerSimulate(Number(wieT), Number(wieSteps), Number(wiePaths)))} />
        </Row>
        <ResultPanel data={wie.result} error={wie.error} />
      </EndpointCard>

      <EndpointCard title="Brownian Bridge" description="Conditioned Brownian motion">
        <Row>
          <Field label="T"><Input value={bbT} onChange={setBbT} type="number" /></Field>
          <Field label="a (start)"><Input value={bbA} onChange={setBbA} type="number" /></Field>
          <Field label="b (end)"><Input value={bbB} onChange={setBbB} type="number" /></Field>
          <Field label="Steps"><Input value={bbSteps} onChange={setBbSteps} type="number" /></Field>
          <RunButton loading={bb.loading} onClick={() => bb.run(() => processesApi.brownianBridge(Number(bbT), Number(bbA), Number(bbB), Number(bbSteps)))} />
        </Row>
        <ResultPanel data={bb.result} error={bb.error} />
      </EndpointCard>

      <EndpointCard title="Correlated Brownian Motion" description="Multi-asset correlated BM">
        <Row>
          <Field label="N Assets"><Input value={cbmN} onChange={setCbmN} type="number" /></Field>
          <Field label="Corr Matrix (JSON)" width="250px"><Input value={cbmCorr} onChange={setCbmCorr} /></Field>
          <Field label="T"><Input value={cbmT} onChange={setCbmT} type="number" /></Field>
          <Field label="Steps"><Input value={cbmSteps} onChange={setCbmSteps} type="number" /></Field>
          <RunButton loading={cbm.loading} onClick={() => cbm.run(() => processesApi.correlatedBm(Number(cbmN), JSON.parse(cbmCorr), Number(cbmT), Number(cbmSteps)))} />
        </Row>
        <ResultPanel data={cbm.result} error={cbm.error} />
      </EndpointCard>

      <EndpointCard title="Variance Gamma" description="VG process simulation">
        <Row>
          <Field label="S0"><Input value={vgS0} onChange={setVgS0} type="number" /></Field>
          <Field label="r"><Input value={vgR} onChange={setVgR} type="number" /></Field>
          <Field label="Sigma"><Input value={vgSigma} onChange={setVgSigma} type="number" /></Field>
          <Field label="Theta VG"><Input value={vgTheta} onChange={setVgTheta} type="number" /></Field>
          <Field label="Nu"><Input value={vgNu} onChange={setVgNu} type="number" /></Field>
          <Field label="T"><Input value={vgT} onChange={setVgT} type="number" /></Field>
          <Field label="Paths"><Input value={vgPaths} onChange={setVgPaths} type="number" /></Field>
          <RunButton loading={vg.loading} onClick={() => vg.run(() => processesApi.vgSimulate(Number(vgS0), Number(vgR), Number(vgSigma), Number(vgTheta), Number(vgNu), Number(vgT), 252, Number(vgPaths)))} />
        </Row>
        <ResultPanel data={vg.result} error={vg.error} />
      </EndpointCard>
    </>
  );
}
