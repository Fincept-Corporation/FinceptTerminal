import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { cashflowApi, spreadApi, basisApi, analysisApi } from '../quantlibSolverApi';

export default function CashflowPanel() {
  // PV01
  const pv01 = useEndpoint();
  const [pvNotional, setPvNotional] = useState('1000000');
  const [pvDFs, setPvDFs] = useState('0.97, 0.94, 0.91, 0.88, 0.85');
  const [pvTimes, setPvTimes] = useState('1, 2, 3, 4, 5');
  const [pvFreq, setPvFreq] = useState('2');

  // IRR
  const irr = useEndpoint();
  const [irrCF, setIrrCF] = useState('-1000, 300, 300, 300, 300, 300');
  const [irrT, setIrrT] = useState('');
  const [irrGuess, setIrrGuess] = useState('0.1');

  // XIRR
  const xirr = useEndpoint();
  const [xirrCF, setXirrCF] = useState('-1000, 300, 300, 300, 400');
  const [xirrDates, setXirrDates] = useState('2024-01-01, 2024-07-01, 2025-01-01, 2025-07-01, 2026-01-01');

  // G-Spread
  const gs = useEndpoint();
  const [gsBY, setGsBY] = useState('0.055');
  const [gsGY, setGsGY] = useState('0.04');

  // I-Spread
  const is_ = useEndpoint();
  const [isBY, setIsBY] = useState('0.055');
  const [isSR, setIsSR] = useState('0.045');

  // Z-Spread
  const zs = useEndpoint();
  const [zsPrice, setZsPrice] = useState('95');
  const [zsCF, setZsCF] = useState('5, 5, 5, 5, 105');
  const [zsTimes, setZsTimes] = useState('1, 2, 3, 4, 5');
  const [zsRates, setZsRates] = useState('0.04, 0.042, 0.045, 0.048, 0.05');

  // Basis
  const bas = useEndpoint();
  const [basSpot, setBasSpot] = useState('100');
  const [basFut, setBasFut] = useState('101.5');

  // Carry
  const carry = useEndpoint();
  const [carSpot, setCarSpot] = useState('100');
  const [carFut, setCarFut] = useState('101.5');
  const [carT, setCarT] = useState('0.25');

  // Implied Repo
  const repo = useEndpoint();
  const [repoSpot, setRepoSpot] = useState('100');
  const [repoFut, setRepoFut] = useState('101.5');
  const [repoT, setRepoT] = useState('0.25');
  const [repoInc, setRepoInc] = useState('0');

  // NPV
  const npv = useEndpoint();
  const [npvCF, setNpvCF] = useState('-1000, 300, 300, 300, 300, 300');
  const [npvRate, setNpvRate] = useState('0.08');

  // Loan Amortization
  const loan = useEndpoint();
  const [lnPrinc, setLnPrinc] = useState('100000');
  const [lnRate, setLnRate] = useState('0.06');
  const [lnN, setLnN] = useState('360');
  const [lnFreq, setLnFreq] = useState('12');

  // Annuity PV
  const ann = useEndpoint();
  const [annPay, setAnnPay] = useState('1000');
  const [annRate, setAnnRate] = useState('0.05');
  const [annN, setAnnN] = useState('10');

  // Perpetuity PV
  const perp = useEndpoint();
  const [perpPay, setPerpPay] = useState('1000');
  const [perpRate, setPerpRate] = useState('0.05');
  const [perpG, setPerpG] = useState('0.02');

  // Future Value
  const fv = useEndpoint();
  const [fvPV, setFvPV] = useState('1000');
  const [fvRate, setFvRate] = useState('0.05');
  const [fvN, setFvN] = useState('10');

  return (
    <>
      <EndpointCard title="PV01" description="Price value of a basis point">
        <Row>
          <Field label="Notional"><Input value={pvNotional} onChange={setPvNotional} type="number" /></Field>
          <Field label="Discount Factors" width="250px"><Input value={pvDFs} onChange={setPvDFs} /></Field>
          <Field label="Times" width="200px"><Input value={pvTimes} onChange={setPvTimes} /></Field>
          <Field label="Frequency"><Input value={pvFreq} onChange={setPvFreq} type="number" /></Field>
          <RunButton loading={pv01.loading} onClick={() => pv01.run(() => cashflowApi.pv01(Number(pvNotional), pvDFs.split(',').map(Number), pvTimes.split(',').map(Number), Number(pvFreq)))} />
        </Row>
        <ResultPanel data={pv01.result} error={pv01.error} />
      </EndpointCard>

      <EndpointCard title="IRR" description="Internal rate of return">
        <Row>
          <Field label="Cashflows" width="350px"><Input value={irrCF} onChange={setIrrCF} /></Field>
          <Field label="Times (opt)" width="200px"><Input value={irrT} onChange={setIrrT} placeholder="auto" /></Field>
          <Field label="Guess"><Input value={irrGuess} onChange={setIrrGuess} type="number" /></Field>
          <RunButton loading={irr.loading} onClick={() => irr.run(() => cashflowApi.irr(irrCF.split(',').map(Number), irrT ? irrT.split(',').map(Number) : undefined, Number(irrGuess)))} />
        </Row>
        <ResultPanel data={irr.result} error={irr.error} />
      </EndpointCard>

      <EndpointCard title="XIRR" description="Extended IRR with irregular dates">
        <Row>
          <Field label="Cashflows" width="300px"><Input value={xirrCF} onChange={setXirrCF} /></Field>
          <Field label="Dates" width="350px"><Input value={xirrDates} onChange={setXirrDates} /></Field>
          <RunButton loading={xirr.loading} onClick={() => xirr.run(() => cashflowApi.xirr(xirrCF.split(',').map(Number), xirrDates.split(',').map(s => s.trim())))} />
        </Row>
        <ResultPanel data={xirr.result} error={xirr.error} />
      </EndpointCard>

      <EndpointCard title="G-Spread" description="Government spread (bond yield − govt yield)">
        <Row>
          <Field label="Bond Yield"><Input value={gsBY} onChange={setGsBY} type="number" /></Field>
          <Field label="Govt Yield"><Input value={gsGY} onChange={setGsGY} type="number" /></Field>
          <RunButton loading={gs.loading} onClick={() => gs.run(() => spreadApi.gSpread(Number(gsBY), Number(gsGY)))} />
        </Row>
        <ResultPanel data={gs.result} error={gs.error} />
      </EndpointCard>

      <EndpointCard title="I-Spread" description="Interpolated spread (bond yield − swap rate)">
        <Row>
          <Field label="Bond Yield"><Input value={isBY} onChange={setIsBY} type="number" /></Field>
          <Field label="Swap Rate"><Input value={isSR} onChange={setIsSR} type="number" /></Field>
          <RunButton loading={is_.loading} onClick={() => is_.run(() => spreadApi.iSpread(Number(isBY), Number(isSR)))} />
        </Row>
        <ResultPanel data={is_.result} error={is_.error} />
      </EndpointCard>

      <EndpointCard title="Z-Spread" description="Zero-volatility spread over term structure">
        <Row>
          <Field label="Price"><Input value={zsPrice} onChange={setZsPrice} type="number" /></Field>
          <Field label="Cashflows" width="200px"><Input value={zsCF} onChange={setZsCF} /></Field>
          <Field label="Times" width="150px"><Input value={zsTimes} onChange={setZsTimes} /></Field>
          <Field label="Base Rates" width="250px"><Input value={zsRates} onChange={setZsRates} /></Field>
          <RunButton loading={zs.loading} onClick={() => zs.run(() => spreadApi.zSpread(Number(zsPrice), zsCF.split(',').map(Number), zsTimes.split(',').map(Number), zsRates.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={zs.result} error={zs.error} />
      </EndpointCard>

      <EndpointCard title="Basis" description="Spot-futures basis">
        <Row>
          <Field label="Spot"><Input value={basSpot} onChange={setBasSpot} type="number" /></Field>
          <Field label="Futures"><Input value={basFut} onChange={setBasFut} type="number" /></Field>
          <RunButton loading={bas.loading} onClick={() => bas.run(() => basisApi.basis(Number(basSpot), Number(basFut)))} />
        </Row>
        <ResultPanel data={bas.result} error={bas.error} />
      </EndpointCard>

      <EndpointCard title="Cost of Carry" description="Implied cost of carry from spot/futures">
        <Row>
          <Field label="Spot"><Input value={carSpot} onChange={setCarSpot} type="number" /></Field>
          <Field label="Futures"><Input value={carFut} onChange={setCarFut} type="number" /></Field>
          <Field label="Time to Expiry"><Input value={carT} onChange={setCarT} type="number" /></Field>
          <RunButton loading={carry.loading} onClick={() => carry.run(() => basisApi.carry(Number(carSpot), Number(carFut), Number(carT)))} />
        </Row>
        <ResultPanel data={carry.result} error={carry.error} />
      </EndpointCard>

      <EndpointCard title="Implied Repo Rate" description="Implied repo rate from futures/spot">
        <Row>
          <Field label="Spot"><Input value={repoSpot} onChange={setRepoSpot} type="number" /></Field>
          <Field label="Futures"><Input value={repoFut} onChange={setRepoFut} type="number" /></Field>
          <Field label="Time to Expiry"><Input value={repoT} onChange={setRepoT} type="number" /></Field>
          <Field label="Income"><Input value={repoInc} onChange={setRepoInc} type="number" /></Field>
          <RunButton loading={repo.loading} onClick={() => repo.run(() => basisApi.impliedRepo(Number(repoSpot), Number(repoFut), Number(repoT), Number(repoInc)))} />
        </Row>
        <ResultPanel data={repo.result} error={repo.error} />
      </EndpointCard>

      <EndpointCard title="NPV" description="Net present value of cashflows">
        <Row>
          <Field label="Cashflows" width="350px"><Input value={npvCF} onChange={setNpvCF} /></Field>
          <Field label="Discount Rate"><Input value={npvRate} onChange={setNpvRate} type="number" /></Field>
          <RunButton loading={npv.loading} onClick={() => npv.run(() => analysisApi.npv(npvCF.split(',').map(Number), Number(npvRate)))} />
        </Row>
        <ResultPanel data={npv.result} error={npv.error} />
      </EndpointCard>

      <EndpointCard title="Loan Amortization" description="Full amortization schedule">
        <Row>
          <Field label="Principal"><Input value={lnPrinc} onChange={setLnPrinc} type="number" /></Field>
          <Field label="Annual Rate"><Input value={lnRate} onChange={setLnRate} type="number" /></Field>
          <Field label="N Periods"><Input value={lnN} onChange={setLnN} type="number" /></Field>
          <Field label="Frequency"><Input value={lnFreq} onChange={setLnFreq} type="number" /></Field>
          <RunButton loading={loan.loading} onClick={() => loan.run(() => analysisApi.loan(Number(lnPrinc), Number(lnRate), Number(lnN), Number(lnFreq)))} />
        </Row>
        <ResultPanel data={loan.result} error={loan.error} />
      </EndpointCard>

      <EndpointCard title="Annuity PV" description="Present value of an annuity">
        <Row>
          <Field label="Payment"><Input value={annPay} onChange={setAnnPay} type="number" /></Field>
          <Field label="Rate"><Input value={annRate} onChange={setAnnRate} type="number" /></Field>
          <Field label="N Periods"><Input value={annN} onChange={setAnnN} type="number" /></Field>
          <RunButton loading={ann.loading} onClick={() => ann.run(() => analysisApi.annuity(Number(annPay), Number(annRate), Number(annN)))} />
        </Row>
        <ResultPanel data={ann.result} error={ann.error} />
      </EndpointCard>

      <EndpointCard title="Perpetuity PV" description="Present value of a growing perpetuity">
        <Row>
          <Field label="Payment"><Input value={perpPay} onChange={setPerpPay} type="number" /></Field>
          <Field label="Rate"><Input value={perpRate} onChange={setPerpRate} type="number" /></Field>
          <Field label="Growth"><Input value={perpG} onChange={setPerpG} type="number" /></Field>
          <RunButton loading={perp.loading} onClick={() => perp.run(() => analysisApi.perpetuity(Number(perpPay), Number(perpRate), Number(perpG)))} />
        </Row>
        <ResultPanel data={perp.result} error={perp.error} />
      </EndpointCard>

      <EndpointCard title="Future Value" description="Future value of a present amount">
        <Row>
          <Field label="PV"><Input value={fvPV} onChange={setFvPV} type="number" /></Field>
          <Field label="Rate"><Input value={fvRate} onChange={setFvRate} type="number" /></Field>
          <Field label="N Periods"><Input value={fvN} onChange={setFvN} type="number" /></Field>
          <RunButton loading={fv.loading} onClick={() => fv.run(() => analysisApi.fv(Number(fvPV), Number(fvRate), Number(fvN)))} />
        </Row>
        <ResultPanel data={fv.result} error={fv.error} />
      </EndpointCard>
    </>
  );
}
