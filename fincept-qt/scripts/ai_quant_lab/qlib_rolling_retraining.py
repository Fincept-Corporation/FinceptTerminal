"""AI Quant Lab - Rolling Retraining: Qlib-backed rolling retrain configs.

A "schedule" is a saved retrain CONFIG: nothing runs it on a timer -- a retrain
starts only when the user clicks RETRAIN / Run Now. Records therefore carry no
frequency / next-run fields (older records may still have them; they are ignored).
"""
import json
import os
import sys
import time
import warnings
from datetime import datetime
from pathlib import Path
from typing import Any, Dict

warnings.filterwarnings("ignore")

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import fincept_json

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(1, _HERE)
import qlib_datasets as qds

# LightGBM in the template and qlib's data loaders would otherwise use every core.
_MAX_THREADS = min(4, os.cpu_count() or 1)
for _v in ("OMP_NUM_THREADS", "MKL_NUM_THREADS", "OPENBLAS_NUM_THREADS", "NUMEXPR_NUM_THREADS",
           "VECLIB_MAXIMUM_THREADS"):
    os.environ.setdefault(_v, str(_MAX_THREADS))

_DATA_MISSING_MSG = qds.no_dataset_message()

# Recent MLflow releases refuse to open Qlib's file-based tracking store unless this is
# set, which made every rolling retrain fail with "filesystem tracking backend ... is
# in maintenance mode".
os.environ.setdefault("MLFLOW_ALLOW_FILE_STORE", "true")

# ── Persistence paths ─────────────────────────────────────────────────────────
# <data root>/script-data/rolling_retraining/ (fincept_paths), resolved at use rather than
# at import. The files releases kept in ~/.fincept are copied in once, never moved.

def _schedules_file() -> Path:
    import fincept_paths
    return fincept_paths.user_data_file("rolling_retraining", "rolling_schedules.json",
                                        legacy="rolling_schedules.json")


def _configs_dir() -> Path:
    """The YAML configs CREATE generates (a user's own config file stays where it is)."""
    import fincept_paths
    return fincept_paths.user_data_dir("rolling_retraining", "configs", legacy="configs")


def _mlruns_dir() -> Path:
    """MLflow tracking folder, shared with qlib_service."""
    import fincept_paths
    return fincept_paths.user_data_dir("mlruns", legacy="mlruns")

# ── Qlib import ───────────────────────────────────────────────────────────────
# On demand: importing qlib.contrib.rolling (qlib, mlflow, ...) takes seconds, and list /
# create / delete and the panel's dataset probe do not need it.
QLIB_AVAILABLE = None
QLIB_ERROR = None
Rolling = qlib = None


def _load_qlib() -> bool:
    global QLIB_AVAILABLE, QLIB_ERROR, Rolling, qlib
    if QLIB_AVAILABLE is None:
        try:
            from qlib.contrib.rolling.base import Rolling as _rolling
            import qlib as _qlib
            import logging
            # Send qlib's verbose logs to stderr so only our JSON lines appear on stdout
            logging.getLogger("qlib").setLevel(logging.WARNING)
            Rolling, qlib = _rolling, _qlib
            QLIB_AVAILABLE = True
        except Exception as e:
            QLIB_AVAILABLE, QLIB_ERROR = False, str(e)
    return QLIB_AVAILABLE


def _qlib_installed() -> bool:
    try:
        import importlib.util
        return importlib.util.find_spec("qlib") is not None
    except (ImportError, ValueError):
        return False

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
            num_threads: {num_threads}
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
    print(fincept_json.dumps(obj), flush=True)


def _friendly_error(e: Exception) -> str:
    """qlib's message for a missing dataset is "instrument: {...} does not contain data
    for day" / "can't find a freq from [] ..."; say what is actually wrong."""
    text = str(e)
    lowered = text.lower()
    if ("does not contain data for day" in lowered or "can't find a freq" in lowered
            or "cannot find a freq" in lowered):
        return _DATA_MISSING_MSG
    return text


# ── Schedule store ─────────────────────────────────────────────────────────────

def load_schedules() -> Dict[str, Any]:
    try:
        path = _schedules_file()
        if not path.exists():
            return {}
        with open(path) as f:
            schedules = json.load(f)
    except Exception:
        return {}
    _adopt_legacy_configs(schedules)
    return schedules


def _adopt_legacy_configs(schedules: Any) -> None:
    """Records copied from releases name their generated YAML in ~/.fincept/configs: point
    them at its copy in _configs_dir() (saved with the next change). Best-effort."""
    try:
        import fincept_paths
        legacy = (fincept_paths.legacy_home_dir() / "configs").resolve()
        configs = None
        for rec in (schedules.values() if isinstance(schedules, dict) else ()):
            if not (isinstance(rec, dict) and rec.get("template_generated") and rec.get("conf_path")):
                continue
            conf = Path(str(rec["conf_path"]))
            if conf.parent.resolve() != legacy:
                continue
            configs = configs or _configs_dir()
            if (configs / conf.name).is_file():
                rec["conf_path"] = str(configs / conf.name)
    except Exception as e:
        print(f"Could not adopt the configs of earlier releases: {e}", file=sys.stderr)


def save_schedules(schedules: Dict[str, Any]):
    import fincept_paths
    fincept_paths.atomic_write_text(_schedules_file(), json.dumps(schedules, indent=2))


def _strip_timer_fields(record: Dict[str, Any]) -> Dict[str, Any]:
    """Records written before configs stopped pretending to be timers carry a
    frequency / next_run that nothing ever acted on; do not hand them out."""
    return {k: v for k, v in record.items() if k not in ("frequency", "next_run")}


# ── Commands ──────────────────────────────────────────────────────────────────

def cmd_list():
    schedules = {k: _strip_timer_fields(v) for k, v in load_schedules().items()}
    emit({
        "success": True,
        "schedules": schedules,
        "count": len(schedules),
        "qlib_available": _qlib_installed(),
        "note": "Retrain configs run only when you start them (RETRAIN / Run Now); nothing runs them on a timer.",
    })


def _window_param(params: dict):
    """(window in trading days or None, error). Training window: it sets the built-in
    template's training segment; a user config file defines its own segments."""
    window = params.get("window")
    try:
        window = int(window) if window not in (None, "") else None
    except (TypeError, ValueError):
        return None, f"window must be a whole number of trading days (got {window!r})"
    if window is not None and window <= 0:
        return None, f"window must be a positive number of trading days (got {window})"
    return window, None


def _builtin_template(params: dict, window):
    """The built-in LightGBM + Alpha158 config for a dataset, as (spec, error).

    spec = {"yaml", "dataset_id", "dataset", "market", "train_start", "segments", "note"}.
    The dataset is params["dataset"] (the panel's dataset bar), else the freshest one (the
    public CN dataset for an old caller that asks for region cn). Shared by create (which
    saves the YAML) and preview (which only needs the windows it would train)."""
    region = params.get("region", "us")
    market = (params.get("market") or "").strip().lower() or None
    want = params.get("dataset") or ("qlib_cn" if str(region).lower() == "cn" else None)
    info, err = qds.resolve(want)
    if info is None:
        return None, err or _DATA_MISSING_MSG
    dataset_id, provider_uri, region = info["id"], info["provider_uri"], info["region"]
    market = market or info["default_market"]
    if market not in info["markets"]:
        return None, (f"The {dataset_id} dataset has no market '{market}' "
                      f"(it has: {', '.join(info['markets'])}).")

    # Segments follow the dataset's calendar (they used to be pinned to train <=
    # 2017-12-31 / valid 2018 / test 2019-): test = the last year, valid = the year
    # before it, train = `window` trading days (default: up to ~5 years) before that.
    cal = qds._calendar(Path(info["path"]))
    d = qds.suggested_dates(cal)
    if not d.get("train"):
        return None, (f"The {dataset_id} dataset is too short for a train / valid / test "
                      f"split ({len(cal)} trading days).")
    (valid_start, valid_end), (test_start, test_end) = d["valid"], d["test"]
    train_end = d["train"][1]
    note = None
    if window is not None:
        end_i = cal.index(train_end)
        if window > end_i + 1:
            note = (f"Window {window} is longer than the {end_i + 1} trading days before validation in "
                    f"{dataset_id}; training starts at its first day.")
        train_start = cal[max(0, end_i - window + 1)]
    else:
        train_start = d["train"][0]
    yaml_content = BUILTIN_TEMPLATE.format(
        provider_uri=provider_uri,
        region=region,
        market=market,
        num_threads=_MAX_THREADS,
        train_start=train_start,
        train_end=train_end,
        valid_start=valid_start,
        valid_end=valid_end,
        test_start=test_start,
        test_end=test_end,
    )
    return {
        "yaml": yaml_content,
        "dataset_id": dataset_id,
        "dataset": info,
        "market": market,
        "train_start": train_start,
        "segments": {"train": [train_start, train_end], "valid": [valid_start, valid_end],
                     "test": [test_start, test_end]},
        "note": note,
    }, None


def cmd_create(params: dict):
    model_id = params.get("model_id", "").strip()
    if not model_id:
        emit({"success": False, "error": "model_id is required"})
        return

    # Training window in trading days. It sets the built-in template's training
    # segment (it used to be stored and displayed but never used); a user config
    # file defines its own segments, so there it does not apply.
    window, err = _window_param(params)
    if err:
        emit({"success": False, "error": err})
        return
    step = int(params.get("step", 20))
    horizon = int(params.get("horizon", 20))
    conf_path = params.get("conf_path", "").strip()
    market = (params.get("market") or "").strip().lower() or None
    note = None
    train_start = None
    dataset_id = None
    segments = None

    # Generate built-in template if no conf_path provided
    if not conf_path:
        spec, err = _builtin_template(params, window)
        if err:
            emit({"success": False, "error": err})
            return
        dataset_id, market, train_start = spec["dataset_id"], spec["market"], spec["train_start"]
        segments, note = spec["segments"], spec["note"]

        conf_path = str(_configs_dir() / f"{model_id}.yaml")
        with open(conf_path, "w") as f:
            f.write(spec["yaml"])
        template_generated = True
    else:
        if not Path(conf_path).exists():
            emit({"success": False, "error": f"Config file not found: {conf_path}"})
            return
        template_generated = False
        if window is not None:
            note = "Window ignored: the config file defines its own training segment."
            window = None

    schedules = load_schedules()
    schedules[model_id] = {
        "conf_path": conf_path,
        "window": window,
        "train_start": train_start,
        "step": step,
        "horizon": horizon,
        "last_run": None,
        "last_status": "pending",
        "template_generated": template_generated,
        "created_at": datetime.now().isoformat(),
        "dataset": dataset_id,
        "market": market if template_generated else None,
        "segments": segments,
    }
    save_schedules(schedules)

    out = {
        "success": True,
        "model_id": model_id,
        "schedule": schedules[model_id],
        "template_generated": template_generated,
        "conf_path": conf_path,
        "dataset_id": dataset_id,
        "segments": segments,
    }
    if note:
        out["note"] = note
    emit(out)


def _qlib_init_from_conf(conf_path: str):
    """Read qlib_init section from YAML and call qlib.init()."""
    from ruamel.yaml import YAML as _YAML
    yaml = _YAML(typ="safe", pure=True)
    with open(conf_path) as f:
        cfg = yaml.load(f)
    init_cfg = cfg.get("qlib_init", {})
    # No provider_uri in the config: the public US dataset (qlib_datasets.public_dataset_dir).
    provider_uri = (init_cfg["provider_uri"] if "provider_uri" in init_cfg
                    else str(qds.public_dataset_dir("us")))
    region = init_cfg.get("region", "us")
    # Experiment tracking in the edition's data root rather than ./mlruns in the process's cwd.
    # kernels: qlib's joblib data-loading pool (default cpu_count() - 2 processes).
    qlib.init(provider_uri=provider_uri, region=region, kernels=_MAX_THREADS,
              exp_manager={"class": "MLflowExpManager",
                           "module_path": "qlib.workflow.expm",
                           "kwargs": {"uri": _mlruns_dir().as_uri(),
                                      "default_exp_name": "Experiment"}})


def _dataset_of_conf(conf_path: str):
    """The registered dataset a config's provider_uri points at, or None."""
    try:
        from ruamel.yaml import YAML as _YAML
        with open(conf_path) as f:
            uri = (_YAML(typ="safe", pure=True).load(f) or {}).get("qlib_init", {}).get("provider_uri")
        if not uri:
            return None
        target = Path(str(uri)).expanduser().resolve()
        return next((i for i in qds.list_datasets() if Path(i["path"]).resolve() == target), None)
    except Exception:
        return None


def cmd_preview(params: dict):
    """The rolling windows a config would train, without training anything.

    Sources, first match wins:
      conf_path                      that YAML file;
      model_id with a saved config   the saved config (its own step / horizon);
      anything else                  a new id (or none): the built-in LightGBM + Alpha158
                                     config CREATE would write for params["dataset"] (else
                                     the freshest dataset), params["window"] / ["market"] --
                                     generated into a temporary file, nothing is saved.
    """
    model_id = (params.get("model_id") or "").strip()
    conf_path = (params.get("conf_path") or "").strip()
    try:
        step = int(params.get("step", 20))
        horizon = int(params.get("horizon", 20))
    except (TypeError, ValueError):
        emit({"success": False, "error": "step and horizon must be whole numbers of trading days"})
        return
    if step <= 0 or horizon <= 0:
        emit({"success": False, "error": f"step and horizon must be positive (got step {step}, horizon {horizon})"})
        return

    saved = False
    template = None
    if model_id and not conf_path:
        sched = load_schedules().get(model_id)
        if sched:
            saved = True
            conf_path = sched["conf_path"]
            step = sched.get("step", step)
            horizon = sched.get("horizon", horizon)

    if not conf_path:
        window, err = _window_param(params)
        if err:
            emit({"success": False, "error": err})
            return
        template, err = _builtin_template(params, window)
        if err:
            emit({"success": False, "error": err})
            return

    if template is None and not Path(conf_path).exists():
        emit({"success": False, "error": f"Config file not found: {conf_path}"})
        return

    if not _load_qlib():
        emit({"success": False, "error": f"Qlib not available: {QLIB_ERROR}"})
        return

    tmp_conf = None
    try:
        if template is not None:
            import tempfile
            fd, tmp_conf = tempfile.mkstemp(prefix="fincept_rolling_preview_", suffix=".yaml")
            with os.fdopen(fd, "w") as f:
                f.write(template["yaml"])
            conf_path = tmp_conf
        _cmd_preview_windows(conf_path, step, horizon, model_id, saved, template)
    finally:
        if tmp_conf:
            try:
                os.unlink(tmp_conf)
            except OSError:
                pass


def _cmd_preview_windows(conf_path: str, step: int, horizon: int, model_id: str, saved: bool, template):
    try:
        # Init qlib from the config's provider_uri
        _qlib_init_from_conf(conf_path)
        rolling = Rolling(conf_path=conf_path, step=step, horizon=horizon)
        # Only the window dates: Rolling.get_task_list() first builds the whole Alpha158
        # handler over the config's span and pickles it next to the config (minutes, and a
        # 600 MB file for sp500) -- the training run does that, a preview need not.
        from qlib.workflow.task.gen import RollingGen, task_generator
        task_list = task_generator(rolling.basic_task(enable_handler_cache=False),
                                   RollingGen(step=step, trunc_days=horizon + 1))

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

        out = {
            "success": True,
            "conf_path": None if template is not None else conf_path,
            "total_windows": len(windows),
            "step": step,
            "horizon": horizon,
            "windows": windows,
            # saved: the windows of the saved config of model_id; template: the built-in
            # config CREATE CONFIG would write (not saved yet).
            "source": "saved_config" if saved else ("template" if template is not None else "config_file"),
            "saved": saved,
        }
        if model_id:
            out["model_id"] = model_id
        if template is not None:
            out["template_generated"] = True
            out["dataset_id"] = template["dataset_id"]
            out["market"] = template["market"]
            out["segments"] = template["segments"]
            out["dataset"] = qds.brief(template["dataset"])
            if template["note"]:
                out["note"] = template["note"]
        else:
            match = _dataset_of_conf(conf_path)
            if match:
                out["dataset"] = qds.brief(match)
        emit(out)
    except Exception as e:
        emit({"success": False, "error": _friendly_error(e)})


def cmd_retrain(params: dict):
    # Retrain speaks the streaming protocol (start / window / ensemble / done / error):
    # the early refusals are "error" events too, so the panel ends the run (it keeps
    # its launch buttons disabled until it sees a terminal event).
    model_id = params.get("model_id", "").strip()
    if not model_id:
        emit({"event": "error", "success": False, "error": "model_id is required"})
        return

    schedules = load_schedules()
    if model_id not in schedules:
        emit({"event": "error", "success": False,
              "error": f"No schedule found for model_id: {model_id}. Create it first."})
        return

    if not _load_qlib():
        emit({"event": "error", "success": False, "error": f"Qlib not available: {QLIB_ERROR}"})
        return

    sched = schedules[model_id]
    conf_path = sched["conf_path"]

    if not Path(conf_path).exists():
        emit({"event": "error", "success": False, "error": f"Config file not found: {conf_path}"})
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
        schedules[model_id] = _strip_timer_fields(schedules[model_id])
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
        emit({"event": "error", "success": False, "error": _friendly_error(e)})


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
    # The YAML this module generated for the config goes with it (a user's own
    # config file is never touched).
    removed_file = None
    if removed.get("template_generated") and removed.get("conf_path"):
        conf = Path(removed["conf_path"])
        try:
            if conf.parent.resolve() == _configs_dir().resolve() and conf.is_file():
                conf.unlink()
                removed_file = str(conf)
        except OSError as e:
            print(f"Could not delete {conf}: {e}", file=sys.stderr)
    emit({"success": True, "model_id": model_id, "removed": _strip_timer_fields(removed),
          "removed_config_file": removed_file})


# ── Entry point ───────────────────────────────────────────────────────────────

def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else "help"
    try:
        params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
    except (json.JSONDecodeError, IndexError):
        params = {}

    if cmd in qds.COMMANDS:
        # The panel's dataset bar (probe, BUILD FROM YAHOO, UPDATE TO TODAY). yfinance /
        # pandas chatter goes to stderr; stdout carries the one JSON answer.
        real = os.dup(1)
        os.dup2(2, 1)
        try:
            result = qds.run_command(cmd, params)
        finally:
            sys.stdout.flush()
            os.dup2(real, 1)
            os.close(real)
        emit(result)
    elif cmd == "list":
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
            "available": ["list", "create", "preview", "retrain", "delete", *qds.COMMANDS],
            "qlib_available": _qlib_installed(),
        })


if __name__ == "__main__":
    main()
