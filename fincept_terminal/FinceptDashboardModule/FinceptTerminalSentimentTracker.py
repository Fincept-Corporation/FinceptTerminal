import asyncio
import re
import time
from pathlib import Path
from textual.app import ComposeResult
from textual.containers import VerticalScroll, Container
from textual.widgets import Input, Button, OptionList, Static, ProgressBar
import json
import torch
import google.generativeai as gemai
from youtube_search import YoutubeSearch
from youtube_transcript_api import YouTubeTranscriptApi
from transformers import AutoModelForSequenceClassification, AutoTokenizer
from transformers.utils import default_cache_path
from huggingface_hub import snapshot_download, HfFolder
from huggingface_hub.utils import LocalEntryNotFoundError, HFValidationError
from fincept_terminal.FinceptSettingModule.FinceptTerminalSettingUtils import get_settings_path

SETTINGS_FILE = get_settings_path()
MODEL_ID = "cardiffnlp/twitter-roberta-base-sentiment"

class ModelDownloader:
    def __init__(self, app, progress_bar, model_id=MODEL_ID, eta_label=None):
        self.app = app
        self.progress_bar = progress_bar
        self.model_id = model_id
        self.download_path = None
        self.eta_label = eta_label  # Optionally, a Static widget for ETA display

    async def download_with_progress(self):
        token = HfFolder.get_token()
        cache_dir = Path(default_cache_path)
        model_dir = cache_dir / f"models--{self.model_id.replace('/', '--')}"
        model_dir.mkdir(parents=True, exist_ok=True)
        snapshots_dir = model_dir / "snapshots"
        snapshots_dir.mkdir(parents=True, exist_ok=True)
        import datetime
        timestamp = datetime.datetime.now().strftime("%Y%m%d%H%M%S")
        snapshot_dir = snapshots_dir / timestamp
        self.app.call_from_thread(self.app.notify, "📥 Starting model download via git...", severity="information")

        start_time = time.time()
        percent = 0

        try:
            repo_url = f"https://{token + '@' if token else ''}huggingface.co/{self.model_id}"
            cmd = ["git", "clone", repo_url, str(snapshot_dir), "--progress"]
            process = await asyncio.create_subprocess_exec(*cmd, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)

            self.download_path = snapshot_dir
            last_bytes = 0
            total_bytes = 0
            bytes_done = 0
            speed = 0
            eta = None
            progress_found = False

            while True:
                line = await process.stderr.readline()
                if not line:
                    break
                line = line.decode(errors="ignore").strip()
                # git sends progress to stderr in lines like: "Receiving objects:  41% (154/372), 42.63 MiB | 10.60 MiB/s"
                percent_val, bytes_done_val, total_bytes_val = self._parse_git_progress(line)
                if percent_val is not None:
                    percent = percent_val
                    progress_found = True
                    # Estimate ETA if bytes found
                    if bytes_done_val and total_bytes_val:
                        if not total_bytes:
                            total_bytes = total_bytes_val
                        now = time.time()
                        time_elapsed = now - start_time
                        if time_elapsed > 0:
                            speed = bytes_done_val / time_elapsed  # bytes/sec
                            bytes_left = total_bytes_val - bytes_done_val
                            if speed > 0:
                                eta = int(bytes_left / speed)
                            else:
                                eta = None
                        else:
                            eta = None
                        # Optionally, update ETA label
                        if self.eta_label is not None:
                            self.app.call_from_thread(self.eta_label.update, f"Estimated time left: {self._format_eta(eta)}")
                    # Update progress bar with percent
                    self.app.call_from_thread(self.progress_bar.update, percent)
            await process.wait()
            if process.returncode != 0:
                self.app.call_from_thread(self.app.notify, f"❌ Git clone failed: exit {process.returncode}", severity="error")
                return None
            self.app.call_from_thread(self.progress_bar.update, 100)
            self.app.call_from_thread(self.app.notify, "✅ Clone complete, pulling LFS...", severity="success")
            ok = await self._run_git_lfs_pull(snapshot_dir)
            return snapshot_dir if ok else None
        except Exception as e:
            self.app.call_from_thread(self.app.notify, f"❌ Error cloning: {e}", severity="error")
            return None

    async def _run_git_lfs_pull(self, repo_dir):
        self.app.call_from_thread(self.progress_bar.update, 0)
        start_time = time.time()
        total_bytes = 0
        bytes_done = 0
        eta = None
        try:
            cmd = ["git", "lfs", "pull", "--progress"]
            process = await asyncio.create_subprocess_exec(*cmd, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE, cwd=str(repo_dir))
            while True:
                line = await process.stderr.readline()
                if not line:
                    break
                line = line.decode(errors="ignore").strip()
                percent_val, bytes_done_val, total_bytes_val = self._parse_git_lfs_progress(line)
                if percent_val is not None:
                    # Estimate ETA if possible
                    if bytes_done_val and total_bytes_val:
                        now = time.time()
                        time_elapsed = now - start_time
                        if time_elapsed > 0:
                            speed = bytes_done_val / time_elapsed  # bytes/sec
                            bytes_left = total_bytes_val - bytes_done_val
                            if speed > 0:
                                eta = int(bytes_left / speed)
                            else:
                                eta = None
                        else:
                            eta = None
                        if self.eta_label is not None:
                            self.app.call_from_thread(self.eta_label.update, f"Estimated time left: {self._format_eta(eta)}")
                    self.app.call_from_thread(self.progress_bar.update, percent_val)
            await process.wait()
            if process.returncode == 0:
                self.app.call_from_thread(self.progress_bar.update, 100)
                self.app.call_from_thread(self.app.notify, "✅ Model download complete!", severity="success")
                if self.eta_label is not None:
                    self.app.call_from_thread(self.eta_label.update, f"Estimated time left: 00:00")
                return True
            else:
                self.app.call_from_thread(self.app.notify, f"❌ LFS pull failed: exit {process.returncode}", severity="error")
                return False
        except Exception as e:
            self.app.call_from_thread(self.app.notify, f"❌ Error during LFS pull: {e}", severity="error")
            return False

    def _parse_git_progress(self, line):
        # Example: "Receiving objects:  41% (154/372), 42.63 MiB | 10.60 MiB/s"
        m = re.search(r"Receiving objects:\s+(\d+)% \((\d+)/(\d+)\), ([\d\.]+) ([KMG]i?)B", line)
        if m:
            percent = int(m.group(1))
            bytes_done = float(m.group(4))
            unit = m.group(5)
            bytes_done = self._to_bytes(bytes_done, unit)
            total = int(m.group(3))
            avg_bytes_per_obj = bytes_done / int(m.group(2)) if int(m.group(2)) > 0 else 0
            total_bytes = avg_bytes_per_obj * total
            return percent, bytes_done, total_bytes
        # Fallback for percent only
        m = re.search(r"(\d+)%", line)
        if m:
            return int(m.group(1)), None, None
        return None, None, None

    def _parse_git_lfs_progress(self, line):
        # Example: "Downloading LFS objects:  41% (123/298), 10 MB, 12 MB total, 2.8 MB/s"
        m = re.search(r"Downloading LFS objects:\s+(\d+)% \((\d+)/(\d+)\), ([\d\.]+) ([KMG]B), ([\d\.]+) ([KMG]B) total", line)
        if m:
            percent = int(m.group(1))
            bytes_done = self._to_bytes(float(m.group(4)), m.group(5))
            total_bytes = self._to_bytes(float(m.group(6)), m.group(7))
            return percent, bytes_done, total_bytes
        m = re.search(r"(\d+)%", line)
        if m:
            return int(m.group(1)), None, None
        return None, None, None

    def _to_bytes(self, value, unit):
        unit = unit.upper()
        if "GB" in unit:
            return value * 1024 ** 3
        if "MB" in unit:
            return value * 1024 ** 2
        if "KB" in unit:
            return value * 1024
        return value

    def _format_eta(self, eta):
        if eta is None:
            return "?"
        mins, secs = divmod(int(eta), 60)
        return f"{mins:02d}:{secs:02d}"

# -- The rest of your code remains unchanged except for two things: --
# 1. Add an ETA label widget below the progress bar.
# 2. Pass this widget to ModelDownloader

class YouTubeTranscriptApp(Container):
    # ... [all previous fields and methods]

    def compose(self) -> ComposeResult:
        yield Static("🔎 Enter Search Keyword:", id="search_label")
        yield Input(placeholder="Enter keyword...", id="search_input")
        yield Button("Search", id="search_button", variant="primary")
        yield Static("🎥 Select a Video:", id="video_label", classes="hidden")
        yield OptionList(id="video_list")
        with VerticalScroll(id="analysis_results", classes="analysis_results"):
            yield Static("Transcript", id="transcript_label", classes="hidden")
            yield Static("", id="transcript_display")
            yield Static("Sentiment Analysis", id="sentiment_label", classes="hidden")
            yield Static("", id="sentiment_display")
            yield Button("Download Sentiment Model", id="download_model_button", variant="primary", classes="hidden")
            yield Static("Model Download Progress:", id="download_progress_label", classes="hidden")
            yield ProgressBar(id="download_progress_bar", show_percentage=True, show_eta=False, classes="hidden")
            yield Static("Estimated time left: ?", id="download_eta_label", classes="hidden")  # New ETA label

    # ... [other unchanged methods]

    async def download_sentiment_model(self):
        if self.download_in_progress:
            self.app.notify("⚠ Already in progress.", severity="warning")
            return
        self.download_in_progress = True
        btn = self.query_one("#download_model_button", Button)
        btn.disabled = True
        btn.remove_class("hidden")
        label = self.query_one("#download_progress_label", Static)
        bar = self.query_one("#download_progress_bar", ProgressBar)
        eta_label = self.query_one("#download_eta_label", Static)
        label.remove_class("hidden")
        bar.remove_class("hidden")
        eta_label.remove_class("hidden")
        bar.update(0)
        eta_label.update("Estimated time left: ?")
        self.app.notify("🔄 Downloading model...", severity="information")
        try:
            downloader = ModelDownloader(self.app, bar, MODEL_ID, eta_label)
            path = await downloader.download_with_progress()
            if path:
                self.tokenizer = await asyncio.to_thread(AutoTokenizer.from_pretrained, path, local_files_only=True)
                self.model = await asyncio.to_thread(AutoModelForSequenceClassification.from_pretrained, path, local_files_only=True)
                self.model_loaded = True
                self.app.notify("✅ Model ready!", severity="success")
                btn.add_class("hidden")
            else:
                self.app.notify("❌ Download failed.", severity="error")
                btn.disabled = False
        except Exception as e:
            self.app.notify(f"❌ Error: {e}", severity="error")
            btn.disabled = False
        finally:
            self.download_in_progress = False
            self.query_one("#download_progress_label", Static).add_class("hidden")
            self.query_one("#download_progress_bar", ProgressBar).add_class("hidden")
            self.query_one("#download_eta_label", Static).add_class("hidden")
