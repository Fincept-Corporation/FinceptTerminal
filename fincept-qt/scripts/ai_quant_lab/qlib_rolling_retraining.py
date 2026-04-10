"""AI Quant Lab - Rolling Retraining: Real Qlib-backed rolling model scheduler"""
import json
import os
import sys
import time
import warnings
from datetime import datetime, timedelta
from pathlib import Path
from typing import Any, Dict, Optional

warnings.filterwarnings("ignore")

# ── Persistence paths ─────────────────────────────────────────────────────────
FINCEPT_DIR = Path.home() / ".fincept"
SCHEDULES_FILE = FINCEPT_DIR / "rolling_schedules.json"
CONFIGS_DIR = FINCEPT_DIR / "configs"

# ── Qlib import ───────────────────────────────────────────────────────────────
try:
    from qlib.contrib.rolling.base import Rolling
    import qlib
    import logging
    # Send qlib's verbose logs to stderr so only our JSON lines appear on stdout
    logging.getLogger("qlib").setLevel(logging.WARNING)

    QLIB_AVAILABLE = True
except Exception as e:
    QLIB_AVAILABLE = False
    QLIB_ERROR = str(e)

# ── Built-in LightGBM + Alpha158 template ─────────────────────────────────────
BUILTIN_TEMPLATE = """\
qlib_init:
    provider_uri: "{provider_uri}"
    region: {region}

task:
    model:
        class: LGBModel
        module_path: qlib.contrib.model.gbdt
        kwargs:
            loss: mse
            colsample_bytree: 0.8879
            learning_rate: 0.0421
            subsample: 0.8789
            lambda_l1: 205.6999
            lambda_l2: 580.4723
            max_depth: 8
            num_leaves: 210
            num_threads: 20
    dataset:
        class: DatasetH
        module_path: qlib.data.dataset
        kwargs:
            handler:
                class: Alpha158
                module_path: qlib.contrib.data.handler
                kwargs:
                    start_time: {train_start}
                    end_time: {test_end}
                    fit_start_time: {train_start}
                    fit_end_time: {train_end}
                    instruments: {market}
            segments:
                train: [{train_start}, {train_end}]
                valid: [{valid_start}, {valid_end}]
                test: [{test_start}, {test_end}]
    record:
        - class: SignalRecord
          module_path: qlib.workflow.record_temp
          kwargs:
            model: <MODEL>
            dataset: <DATASET>
        - class: SigAnaRecord
          module_path: qlib.workflow.record_temp
          kwargs:
            ana_long_short: False
            ann_scaler: 252
"""


def emit(obj: dict):
    """Print a single JSON line and flush immediately (streaming protocol)."""
    print(json.dumps(obj), flush=True)


# ── Schedule store ─────────────────────────────────────────────────────────────

def load_schedules() -> Dict[str, Any]:
    if SCHEDULES_FILE.exists():
        try:
            with open(SCHEDULES_FILE) as f:
                return json.load(f)
        except Exception:
            pass
    return {}


def save_schedules(schedules: Dict[str, Any]):
    FINCEPT_DIR.mkdir(parents=True, exist_ok=True)
    with open(SCHEDULES_FILE, "w") as f:
        json.dump(schedules, f, indent=2)


def next_run_date(frequency: str) -> str:
    now = datetime.now()
    if frequency == "daily":
        dt = now + timedelta(days=1)
    elif frequency == "weekly":
        dt = now + timedelta(weeks=1)
    else:  # monthly
        month = now.month % 12 + 1
        year = now.year + (1 if now.month == 12 else 0)
        dt = now.replace(year=year, month=month, day=1)
    return dt.strftime("%Y-%m-%dT%H:%M:%S")


# ── Commands ──────────────────────────────────────────────────────────────────

def cmd_list():
    schedules = load_schedules()
    emit({
        "success": True,
        "schedules": schedules,
        "count": len(schedules),
        "qlib_available": QLIB_AVAILABLE,
    })


def cmd_create(params: dict):
    model_id = params.get("model_id", "").strip()
    if not model_id:
        emit({"success": False, "error": "model_id is required"})
        return

    frequency = params.get("frequency", "weekly")
    window = int(params.get("window", 252))
    step = int(params.get("step", 20))
    horizon = int(params.get("horizon", 20))
    conf_path = params.get("conf_path", "").strip()
    region = params.get("region", "us")
    market = params.get("market", "sp500")

    # Generate built-in template if no conf_path provided
    if not conf_path:
        CONFIGS_DIR.mkdir(parents=True, exist_ok=True)
        conf_path = str(CONFIGS_DIR / f"{model_id}.yaml")
        provider_uri = (Path.home() / ".qlib" / "qlib_data" / f"{region}_data").as_posix()

        # Detect the actual calendar end date for this region to avoid boundary errors
        _end_date = _detect_calendar_end(provider_uri, region)
        yaml_content = BUILTIN_TEMPLATE.format(
            provider_uri=provider_uri,
            region=region,
            market=market,
            train_start="2010-01-01",
            train_end="2017-12-31",
            valid_start="2018-01-01",
            valid_end="2018-12-31",
            test_start="2019-01-01",
            test_end=_end_date,
        )
        with open(conf_path, "w") as f:
            f.write(yaml_content)
        template_generated = True
    else:
        if not Path(conf_path).exists():
            emit({"success": False, "error": f"Config file not found: {conf_path}"})
            return
        template_generated = False

    schedules = load_schedules()
    schedules[model_id] = {
        "conf_path": conf_path,
        "frequency": frequency,
        "window": window,
        "step": step,
        "horizon": horizon,
        "next_run": next_run_date(frequency),
        "last_run": None,
        "last_status": "pending",
        "template_generated": template_generated,
        "created_at": datetime.now().isoformat(),
    }
    save_schedules(schedules)

    emit({
        "success": True,
        "model_id": model_id,
        "schedule": schedules[model_id],
        "template_generated": template_generated,
        "conf_path": conf_path,
    })


def _detect_calendar_end(provider_uri: str, region: str) -> str:
    """Return the last available calendar date for the given data provider."""
    try:
        import qlib as _qlib
        from qlib.data import D
        _qlib.init(provider_uri=provider_uri, region=region)
        cal = D.calendar(freq="day")
        return str(cal[-1])[:10]
    except Exception:
        return "2020-11-01"


def _qlib_init_from_conf(conf_path: str):
    """Read qlib_init section from YAML and call qlib.init()."""
    from ruamel.yaml import YAML as _YAML
    yaml = _YAML(typ="safe", pure=True)
    with open(conf_path) as f:
        cfg = yaml.load(f)
    init_cfg = cfg.get("qlib_init", {})
    provider_uri = init_cfg.get("provider_uri", str(Path.home() / ".qlib" / "qlib_data" / "us_data"))
    region = init_cfg.get("region", "us")
    qlib.init(provider_uri=provider_uri, region=region)


def cmd_preview(params: dict):
    model_id = params.get("model_id", "").strip()
    conf_path = params.get("conf_path", "").strip()
    step = int(params.get("step", 20))
    horizon = int(params.get("horizon", 20))

    # Resolve conf_path from schedule store if model_id given
    if model_id and not conf_path:
        schedules = load_schedules()
        if model_id not in schedules:
            emit({"success": False, "error": f"No schedule found for model_id: {model_id}"})
            return
        sched = schedules[model_id]
        conf_path = sched["conf_path"]
        step = sched.get("step", step)
        horizon = sched.get("horizon", horizon)

    if not conf_path or not Path(conf_path).exists():
        emit({"success": False, "error": f"Config file not found: {conf_path}"})
        return

    if not QLIB_AVAILABLE:
        emit({"success": False, "error": f"Qlib not available: {QLIB_ERROR}"})
        return

    try:
        # Init qlib from the config's provider_uri
        _qlib_init_from_conf(conf_path)
        rolling = Rolling(conf_path=conf_path, step=step, horizon=horizon)
        task_list = rolling.get_task_list()

        windows = []
        for t in task_list:
            segs = t.get("dataset", {}).get("kwargs", {}).get("segments", {})
            train_seg = segs.get("train", [None, None])
            test_seg = segs.get("test", [None, None])
            windows.append({
                "train_start": str(train_seg[0]) if train_seg[0] else None,
                "train_end": str(train_seg[1]) if train_seg[1] else None,
                "test_start": str(test_seg[0]) if test_seg[0] else None,
                "test_end": str(test_seg[1]) if test_seg[1] else None,
            })

        emit({
            "success": True,
            "conf_path": conf_path,
            "total_windows": len(windows),
            "step": step,
            "horizon": horizon,
            "windows": windows,
        })
    except Exception as e:
        emit({"success": False, "error": str(e)})


def cmd_retrain(params: dict):
    model_id = params.get("model_id", "").strip()
    if not model_id:
        emit({"success": False, "error": "model_id is required"})
        return

    schedules = load_schedules()
    if model_id not in schedules:
        emit({"success": False, "error": f"No schedule found for model_id: {model_id}. Create it first."})
        return

    if not QLIB_AVAILABLE:
        emit({"success": False, "error": f"Qlib not available: {QLIB_ERROR}"})
        return

    sched = schedules[model_id]
    conf_path = sched["conf_path"]

    if not Path(conf_path).exists():
        emit({"success": False, "error": f"Config file not found: {conf_path}"})
        return

    step = sched.get("step", 20)
    horizon = sched.get("horizon", 20)

    try:
        _qlib_init_from_conf(conf_path)
        rolling = Rolling(
            conf_path=conf_path,
            exp_name=f"{model_id}_rolling",
            step=step,
            horizon=horizon,
            rolling_exp=f"{model_id}_rolling_models",
        )

        # Get task list first to report total windows
        task_list = rolling.get_task_list()
        total = len(task_list)
        emit({"event": "start", "model_id": model_id, "total_windows": total})

        t_start = time.time()

        # Patch trainer to emit per-window progress
        # We intercept by running tasks one at a time
        from qlib.model.trainer import TrainerR
        from qlib.workflow import R

        # Delete previous rolling experiment
        try:
            R.delete_exp(experiment_name=f"{model_id}_rolling_models")
        except (ValueError, Exception):
            pass

        trainer = TrainerR(experiment_name=f"{model_id}_rolling_models")

        for i, task in enumerate(task_list):
            segs = task.get("dataset", {}).get("kwargs", {}).get("segments", {})
            test_seg = segs.get("test", [None, None])
            train_seg = segs.get("train", [None, None])
            emit({
                "event": "window",
                "index": i + 1,
                "total": total,
                "train_end": str(train_seg[1]) if train_seg[1] else "",
                "test_start": str(test_seg[0]) if test_seg[0] else "",
                "test_end": str(test_seg[1]) if test_seg[1] else "",
            })
            trainer([task])

        # Ensemble and evaluate
        emit({"event": "ensemble", "model_id": model_id, "message": "Combining rolling results..."})
        rolling._ens_rolling()
        rolling._update_rolling_rec()

        elapsed = round(time.time() - t_start, 1)

        # Update schedule persistence
        schedules[model_id]["last_run"] = datetime.now().isoformat()
        schedules[model_id]["last_status"] = "completed"
        schedules[model_id]["next_run"] = next_run_date(sched["frequency"])
        save_schedules(schedules)

        emit({
            "event": "done",
            "success": True,
            "model_id": model_id,
            "elapsed_sec": elapsed,
            "windows_trained": total,
            "exp_name": f"{model_id}_rolling",
        })

    except Exception as e:
        # Update schedule with failure status
        try:
            schedules[model_id]["last_status"] = "failed"
            schedules[model_id]["last_run"] = datetime.now().isoformat()
            save_schedules(schedules)
        except Exception:
            pass
        emit({"event": "error", "success": False, "error": str(e)})


def cmd_delete(params: dict):
    model_id = params.get("model_id", "").strip()
    if not model_id:
        emit({"success": False, "error": "model_id is required"})
        return

    schedules = load_schedules()
    if model_id not in schedules:
        emit({"success": False, "error": f"No schedule found for model_id: {model_id}"})
        return

    removed = schedules.pop(model_id)
    save_schedules(schedules)
    emit({"success": True, "model_id": model_id, "removed": removed})


# ── Entry point ───────────────────────────────────────────────────────────────

def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else "help"
    try:
        params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
    except (json.JSONDecodeError, IndexError):
        params = {}

    if cmd == "list":
        cmd_list()
    elif cmd == "create":
        cmd_create(params)
    elif cmd == "preview":
        cmd_preview(params)
    elif cmd == "retrain":
        cmd_retrain(params)
    elif cmd == "delete":
        cmd_delete(params)
    else:
        emit({
            "success": False,
            "error": f"Unknown command: {cmd}",
            "available": ["list", "create", "preview", "retrain", "delete"],
            "qlib_available": QLIB_AVAILABLE,
        })


if __name__ == "__main__":
    main()
