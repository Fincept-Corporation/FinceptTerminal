import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { sabrApi } from '../quantlibVolatilityApi';

export default function SabrPanel() {
  // Implied Vol
  const iv = useEndpoint();
  const [ivFwd, setIvFwd] = useState('100');
  const [ivStrike, setIvStrike] = useState('100');
  const [ivExpiry, setIvExpiry] = useState('1');
  const [ivAlpha, setIvAlpha] = useState('0.3');
  const [ivBeta, setIvBeta] = useState('0.5');
  const [ivRho, setIvRho] = useState('-0.2');
  const [ivNu, setIvNu] = useState('0.4');

  // Normal Vol
  const nv = useEndpoint();
  const [nvFwd, setNvFwd] = useState('100');
  const [nvStrike, setNvStrike] = useState('100');
  const [nvExpiry, setNvExpiry] = useState('1');
  const [nvAlpha, setNvAlpha] = useState('0.3');
  const [nvBeta, setNvBeta] = useState('0.5');
  const [nvRho, setNvRho] = useState('-0.2');
  const [nvNu, setNvNu] = useState('0.4');

  // Smile
  const sm = useEndpoint();
  const [smFwd, setSmFwd] = useState('100');
  const [smExpiry, setSmExpiry] = useState('1');
  const [smAlpha, setSmAlpha] = useState('0.3');
  const [smBeta, setSmBeta] = useState('0.5');
  const [smRho, setSmRho] = useState('-0.2');
  const [smNu, setSmNu] = useState('0.4');
  const [smStrikes, setSmStrikes] = useState('80, 90, 95, 100, 105, 110, 120');

  // Calibrate
  const cal = useEndpoint();
  const [calFwd, setCalFwd] = useState('100');
  const [calExpiry, setCalExpiry] = useState('1');
  const [calStrikes, setCalStrikes] = useState('80, 90, 100, 110, 120');
  const [calVols, setCalVols] = useState('0.25, 0.22, 0.20, 0.21, 0.24');
  const [calBeta, setCalBeta] = useState('0.5');
  const [calMethod, setCalMethod] = useState('least_squares');

  // Density
  const den = useEndpoint();
  const [denFwd, setDenFwd] = useState('100');
  const [denExpiry, setDenExpiry] = useState('1');
  const [denAlpha, setDenAlpha] = useState('0.3');
  const [denBeta, setDenBeta] = useState('0.5');
  const [denRho, setDenRho] = useState('-0.2');
  const [denNu, setDenNu] = useState('0.4');
  const [denStrikes, setDenStrikes] = useState('70, 80, 90, 100, 110, 120, 130');

  // Dynamics
  const dyn = useEndpoint();
  const [dynFwd, setDynFwd] = useState('100');
  const [dynExpiry, setDynExpiry] = useState('1');
  const [dynAlpha, setDynAlpha] = useState('0.3');
  const [dynBeta, setDynBeta] = useState('0.5');
  const [dynRho, setDynRho] = useState('-0.2');
  const [dynNu, setDynNu] = useState('0.4');
  const [dynStrike, setDynStrike] = useState('100');
  const [dynShift, setDynShift] = useState('0.01');

  return (
    <>
      <EndpointCard title="SABR Implied Vol" description="Hagan SABR lognormal implied volatility">
        <Row>
          <Field label="Forward"><Input value={ivFwd} onChange={setIvFwd} type="number" /></Field>
          <Field label="Strike"><Input value={ivStrike} onChange={setIvStrike} type="number" /></Field>
          <Field label="Expiry"><Input value={ivExpiry} onChange={setIvExpiry} type="number" /></Field>
          <Field label="Alpha"><Input value={ivAlpha} onChange={setIvAlpha} type="number" /></Field>
          <Field label="Beta"><Input value={ivBeta} onChange={setIvBeta} type="number" /></Field>
          <Field label="Rho"><Input value={ivRho} onChange={setIvRho} type="number" /></Field>
          <Field label="Nu"><Input value={ivNu} onChange={setIvNu} type="number" /></Field>
          <RunButton loading={iv.loading} onClick={() => iv.run(() =>
            sabrApi.impliedVol(Number(ivFwd), Number(ivStrike), Number(ivExpiry), Number(ivAlpha), Number(ivBeta), Number(ivRho), Number(ivNu))
          )} />
        </Row>
        <ResultPanel data={iv.result} error={iv.error} />
      </EndpointCard>

      <EndpointCard title="SABR Normal Vol" description="SABR normal (Bachelier) volatility">
        <Row>
          <Field label="Forward"><Input value={nvFwd} onChange={setNvFwd} type="number" /></Field>
          <Field label="Strike"><Input value={nvStrike} onChange={setNvStrike} type="number" /></Field>
          <Field label="Expiry"><Input value={nvExpiry} onChange={setNvExpiry} type="number" /></Field>
          <Field label="Alpha"><Input value={nvAlpha} onChange={setNvAlpha} type="number" /></Field>
          <Field label="Beta"><Input value={nvBeta} onChange={setNvBeta} type="number" /></Field>
          <Field label="Rho"><Input value={nvRho} onChange={setNvRho} type="number" /></Field>
          <Field label="Nu"><Input value={nvNu} onChange={setNvNu} type="number" /></Field>
          <RunButton loading={nv.loading} onClick={() => nv.run(() =>
            sabrApi.normalVol(Number(nvFwd), Number(nvStrike), Number(nvExpiry), Number(nvAlpha), Number(nvBeta), Number(nvRho), Number(nvNu))
          )} />
        </Row>
        <ResultPanel data={nv.result} error={nv.error} />
      </EndpointCard>

      <EndpointCard title="SABR Smile" description="Full smile across strikes">
        <Row>
          <Field label="Forward"><Input value={smFwd} onChange={setSmFwd} type="number" /></Field>
          <Field label="Expiry"><Input value={smExpiry} onChange={setSmExpiry} type="number" /></Field>
          <Field label="Alpha"><Input value={smAlpha} onChange={setSmAlpha} type="number" /></Field>
          <Field label="Beta"><Input value={smBeta} onChange={setSmBeta} type="number" /></Field>
          <Field label="Rho"><Input value={smRho} onChange={setSmRho} type="number" /></Field>
          <Field label="Nu"><Input value={smNu} onChange={setSmNu} type="number" /></Field>
          <Field label="Strikes" width="250px"><Input value={smStrikes} onChange={setSmStrikes} /></Field>
          <RunButton loading={sm.loading} onClick={() => sm.run(() =>
            sabrApi.smile(Number(smFwd), Number(smExpiry), Number(smAlpha), Number(smBeta), Number(smRho), Number(smNu), smStrikes.split(',').map(Number))
          )} />
        </Row>
        <ResultPanel data={sm.result} error={sm.error} />
      </EndpointCard>

      <EndpointCard title="SABR Calibrate" description="Calibrate SABR to market vols">
        <Row>
          <Field label="Forward"><Input value={calFwd} onChange={setCalFwd} type="number" /></Field>
          <Field label="Expiry"><Input value={calExpiry} onChange={setCalExpiry} type="number" /></Field>
          <Field label="Strikes" width="200px"><Input value={calStrikes} onChange={setCalStrikes} /></Field>
          <Field label="Market Vols" width="200px"><Input value={calVols} onChange={setCalVols} /></Field>
          <Field label="Beta"><Input value={calBeta} onChange={setCalBeta} type="number" /></Field>
          <Field label="Method"><Select value={calMethod} onChange={setCalMethod} options={['least_squares', 'differential_evolution']} /></Field>
          <RunButton loading={cal.loading} onClick={() => cal.run(() =>
            sabrApi.calibrate(Number(calFwd), Number(calExpiry), calStrikes.split(',').map(Number), calVols.split(',').map(Number), Number(calBeta), undefined, calMethod)
          )} />
        </Row>
        <ResultPanel data={cal.result} error={cal.error} />
      </EndpointCard>

      <EndpointCard title="SABR Density" description="Implied probability density from SABR">
        <Row>
          <Field label="Forward"><Input value={denFwd} onChange={setDenFwd} type="number" /></Field>
          <Field label="Expiry"><Input value={denExpiry} onChange={setDenExpiry} type="number" /></Field>
          <Field label="Alpha"><Input value={denAlpha} onChange={setDenAlpha} type="number" /></Field>
          <Field label="Beta"><Input value={denBeta} onChange={setDenBeta} type="number" /></Field>
          <Field label="Rho"><Input value={denRho} onChange={setDenRho} type="number" /></Field>
          <Field label="Nu"><Input value={denNu} onChange={setDenNu} type="number" /></Field>
          <Field label="Strikes" width="250px"><Input value={denStrikes} onChange={setDenStrikes} /></Field>
          <RunButton loading={den.loading} onClick={() => den.run(() =>
            sabrApi.density(Number(denFwd), Number(denExpiry), Number(denAlpha), Number(denBeta), Number(denRho), Number(denNu), denStrikes.split(',').map(Number))
          )} />
        </Row>
        <ResultPanel data={den.result} error={den.error} />
      </EndpointCard>

      <EndpointCard title="SABR Dynamics" description="SABR smile dynamics under forward shift">
        <Row>
          <Field label="Forward"><Input value={dynFwd} onChange={setDynFwd} type="number" /></Field>
          <Field label="Expiry"><Input value={dynExpiry} onChange={setDynExpiry} type="number" /></Field>
          <Field label="Alpha"><Input value={dynAlpha} onChange={setDynAlpha} type="number" /></Field>
          <Field label="Beta"><Input value={dynBeta} onChange={setDynBeta} type="number" /></Field>
          <Field label="Rho"><Input value={dynRho} onChange={setDynRho} type="number" /></Field>
          <Field label="Nu"><Input value={dynNu} onChange={setDynNu} type="number" /></Field>
          <Field label="Strike"><Input value={dynStrike} onChange={setDynStrike} type="number" /></Field>
          <Field label="Fwd Shift"><Input value={dynShift} onChange={setDynShift} type="number" /></Field>
          <RunButton loading={dyn.loading} onClick={() => dyn.run(() =>
            sabrApi.dynamics(Number(dynFwd), Number(dynExpiry), Number(dynAlpha), Number(dynBeta), Number(dynRho), Number(dynNu), Number(dynStrike), Number(dynShift))
          )} />
        </Row>
        <ResultPanel data={dyn.result} error={dyn.error} />
      </EndpointCard>
    </>
  );
}
