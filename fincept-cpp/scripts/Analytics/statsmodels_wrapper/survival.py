import pandas as pd
import numpy as np
from typing import Dict, List, Optional, Union, Any
import json
import statsmodels.api as sm
from statsmodels.duration.hazard_regression import PHReg

def fit_cox_ph(
    time: Union[List, np.ndarray, pd.Series],
    status: Union[List, np.ndarray, pd.Series],
    X: Union[np.ndarray, pd.DataFrame],
    add_constant: bool = False
) -> Dict[str, Any]:
    """Fit Cox Proportional Hazards model"""
    time = np.array(time) if not isinstance(time, (np.ndarray, pd.Series)) else time
    status = np.array(status) if not isinstance(status, (np.ndarray, pd.Series)) else status
    X = np.array(X) if not isinstance(X, (np.ndarray, pd.DataFrame)) else X

    if add_constant:
        X = sm.add_constant(X)

    model = PHReg(time, X, status=status)
    results = model.fit()

    return {
        'params': results.params.tolist() if hasattr(results.params, 'tolist') else list(results.params),
        'bse': results.bse.tolist() if hasattr(results.bse, 'tolist') else list(results.bse),
        'tvalues': results.tvalues.tolist() if hasattr(results.tvalues, 'tolist') else list(results.tvalues),
        'pvalues': results.pvalues.tolist() if hasattr(results.pvalues, 'tolist') else list(results.pvalues),
        'llf': float(results.llf)
    }

def survdiff(
    time: Union[List, np.ndarray],
    status: Union[List, np.ndarray],
    group: Union[List, np.ndarray]
) -> Dict[str, Any]:
    """Log-rank test for comparing survival curves"""
    from statsmodels.duration.survfunc import survdiff as sd

    time = np.array(time) if not isinstance(time, np.ndarray) else time
    status = np.array(status) if not isinstance(status, np.ndarray) else status
    group = np.array(group) if not isinstance(group, np.ndarray) else group

    result = sd(time, status, group)

    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def kaplan_meier(
    time: Union[List, np.ndarray],
    status: Union[List, np.ndarray]
) -> Dict[str, Any]:
    """Kaplan-Meier survival function estimation"""
    from statsmodels.duration.survfunc import SurvfuncRight

    time = np.array(time) if not isinstance(time, np.ndarray) else time
    status = np.array(status) if not isinstance(status, np.ndarray) else status

    sf = SurvfuncRight(time, status)

    return {
        'surv_times': sf.surv_times.tolist() if hasattr(sf.surv_times, 'tolist') else list(sf.surv_times),
        'surv_prob': sf.surv_prob.tolist() if hasattr(sf.surv_prob, 'tolist') else list(sf.surv_prob),
        'n_risk': sf.n_risk.tolist() if hasattr(sf.n_risk, 'tolist') else list(sf.n_risk),
        'n_events': sf.n_events.tolist() if hasattr(sf.n_events, 'tolist') else list(sf.n_events)
    }

def main():
    print("Testing statsmodels survival wrapper")

    np.random.seed(42)
    n = 100

    X = np.random.randn(n, 2)
    hazard = np.exp(0.5 * X[:, 0] - 0.3 * X[:, 1])
    time = np.random.exponential(1 / hazard)
    status = np.random.binomial(1, 0.8, n)

    cox_result = fit_cox_ph(time, status, X)
    print("Cox PH params: {}".format([round(p, 4) for p in cox_result['params']]))
    print("Cox PH llf: {:.4f}".format(cox_result['llf']))

    km_result = kaplan_meier(time, status)
    print("KM survival times: {}".format(len(km_result['surv_times'])))

    group = np.random.choice([0, 1], n)
    survdiff_result = survdiff(time, status, group)
    print("Log-rank test p-value: {:.4f}".format(survdiff_result['pvalue']))

    print("Test: PASSED")

if __name__ == "__main__":
    main()
