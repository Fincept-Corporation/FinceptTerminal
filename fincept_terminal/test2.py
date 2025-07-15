import subprocess
import threading
import time
import os
import queue
import dearpygui.dearpygui as dpg

try:
    import speech_recognition as sr
    import pyaudio

    SPEECH_RECOGNITION_AVAILABLE = True
except ImportError:
    SPEECH_RECOGNITION_AVAILABLE = False
    print("Speech recognition not available. Install with: pip install SpeechRecognition pyaudio")


class YouTubeAudioTranscriber:
    def __init__(self):
        self.vlc_process = None
        self.is_playing = False
        self.is_transcribing = False
        self.transcription_thread = None

    def find_vlc_path(self):
        """Find VLC executable on Windows"""
        possible_paths = [
            r"C:\Program Files\VideoLAN\VLC\vlc.exe",
            r"C:\Program Files (x86)\VideoLAN\VLC\vlc.exe",
            "vlc.exe",
            "vlc"
        ]

        for path in possible_paths:
            if os.path.exists(path):
                return path

        # Try using 'where' command on Windows
        try:
            result = subprocess.run(['where', 'vlc'], capture_output=True, text=True)
            if result.returncode == 0:
                return result.stdout.strip().split('\n')[0]
        except:
            pass

        return None

    def get_audio_url(self, youtube_url):
        """Get direct audio stream URL using yt-dlp"""
        try:
            cmd = ['yt-dlp', '--format', 'bestaudio', '--get-url', '--no-playlist', youtube_url]
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)

            if result.returncode == 0:
                audio_url = result.stdout.strip()
                return audio_url
            else:
                return None
        except Exception as e:
            print(f"Error getting audio URL: {e}")
            return None

    def start_vlc_stream(self, youtube_url):
        """Start VLC audio stream"""

        def stream_worker():
            try:
                # Update status
                dpg.set_value("status_text", "Getting audio URL...")

                # Get audio URL
                audio_url = self.get_audio_url(youtube_url)
                if not audio_url:
                    dpg.set_value("status_text", "Failed to get audio URL")
                    return

                # Find VLC
                vlc_path = self.find_vlc_path()
                if not vlc_path:
                    dpg.set_value("status_text", "VLC not found. Please install VLC Media Player.")
                    return

                dpg.set_value("status_text", "Starting VLC audio stream...")

                # Start VLC with audio stream
                cmd = [vlc_path, '--intf', 'dummy', audio_url]
                self.vlc_process = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                self.is_playing = True

                dpg.set_value("status_text", "VLC audio stream started!")

                # Wait for VLC process
                self.vlc_process.wait()
                self.is_playing = False
                dpg.set_value("status_text", "Audio stream ended")

            except Exception as e:
                dpg.set_value("status_text", f"Error: {e}")
                self.is_playing = False

        # Run in separate thread
        threading.Thread(target=stream_worker, daemon=True).start()

    def stop_stream(self):
        """Stop VLC stream"""
        if self.vlc_process:
            try:
                self.vlc_process.terminate()
                self.vlc_process = None
                self.is_playing = False
                return True
            except:
                pass
        return False

    def start_transcription(self):
        """Start real-time audio transcription"""
        if not SPEECH_RECOGNITION_AVAILABLE:
            dpg.set_value("transcription_text",
                          "Speech recognition not available.\nInstall: pip install SpeechRecognition pyaudio")
            return

        def transcribe_worker():
            try:
                # Initialize speech recognizer
                recognizer = sr.Recognizer()
                microphone = sr.Microphone()

                # Adjust for ambient noise
                dpg.set_value("transcription_text", "Adjusting for ambient noise...")
                with microphone as source:
                    recognizer.adjust_for_ambient_noise(source)

                dpg.set_value("transcription_text", "Listening for audio...\n")
                self.is_transcribing = True

                while self.is_transcribing:
                    try:
                        # Listen for audio
                        with microphone as source:
                            # Short timeout to check if we should stop
                            audio = recognizer.listen(source, timeout=1, phrase_time_limit=5)

                        # Recognize speech
                        text = recognizer.recognize_google(audio)

                        # Update transcription display
                        current_text = dpg.get_value("transcription_text")
                        new_text = current_text + f"[{time.strftime('%H:%M:%S')}] {text}\n"

                        # Keep only last 20 lines
                        lines = new_text.split('\n')
                        if len(lines) > 20:
                            new_text = '\n'.join(lines[-20:])

                        dpg.set_value("transcription_text", new_text)

                    except sr.WaitTimeoutError:
                        # No audio detected, continue
                        pass
                    except sr.UnknownValueError:
                        # Speech not recognized
                        pass
                    except sr.RequestError as e:
                        dpg.set_value("transcription_text", f"Recognition error: {e}")
                        break

            except Exception as e:
                dpg.set_value("transcription_text", f"Transcription error: {e}")

        # Start transcription in separate thread
        self.transcription_thread = threading.Thread(target=transcribe_worker, daemon=True)
        self.transcription_thread.start()

    def stop_transcription(self):
        """Stop transcription"""
        self.is_transcribing = False


# Global transcriber instance
transcriber = YouTubeAudioTranscriber()


def start_stream_callback():
    url = dpg.get_value("url_input")
    if url.strip():
        transcriber.start_vlc_stream(url.strip())
    else:
        dpg.set_value("status_text", "Enter YouTube URL")


def stop_stream_callback():
    success = transcriber.stop_stream()
    if success:
        dpg.set_value("status_text", "VLC stream stopped")
    else:
        dpg.set_value("status_text", "No stream to stop")


def start_transcription_callback():
    if not transcriber.is_transcribing:
        transcriber.start_transcription()
        dpg.configure_item("start_transcription_btn", enabled=False)
        dpg.configure_item("stop_transcription_btn", enabled=True)
    else:
        dpg.set_value("status_text", "Transcription already running")


def stop_transcription_callback():
    transcriber.stop_transcription()
    dpg.configure_item("start_transcription_btn", enabled=True)
    dpg.configure_item("stop_transcription_btn", enabled=False)
    dpg.set_value("status_text", "Transcription stopped")


def clear_transcription_callback():
    dpg.set_value("transcription_text", "")


def main():
    dpg.create_context()

    with dpg.window(label="YouTube Audio Transcriber", width=800, height=600, tag="main_window"):
        dpg.add_text("🎵 YouTube Audio + Real-time Transcription", color=(100, 255, 100))
        dpg.add_separator()

        # Audio streaming section
        with dpg.group():
            dpg.add_text("YouTube URL:")
            dpg.add_input_text(
                tag="url_input",
                width=750,
                hint="https://www.youtube.com/watch?v=...",
                default_value="https://www.youtube.com/watch?v=goQriEBukVk"
            )

            dpg.add_separator()

            with dpg.group(horizontal=True):
                dpg.add_button(label="Start VLC Stream", callback=start_stream_callback, width=130)
                dpg.add_button(label="Stop Stream", callback=stop_stream_callback, width=100)

            dpg.add_separator()

            dpg.add_text("Status:", color=(255, 200, 100))
            dpg.add_text("Ready", tag="status_text", color=(200, 200, 200))

        dpg.add_separator()

        # Transcription section
        with dpg.group():
            dpg.add_text("Real-time Transcription:", color=(100, 255, 255))

            with dpg.group(horizontal=True):
                dpg.add_button(
                    label="Start Transcription",
                    callback=start_transcription_callback,
                    width=130,
                    tag="start_transcription_btn"
                )
                dpg.add_button(
                    label="Stop Transcription",
                    callback=stop_transcription_callback,
                    width=130,
                    tag="stop_transcription_btn",
                    enabled=False
                )
                dpg.add_button(label="Clear", callback=clear_transcription_callback, width=70)

            dpg.add_separator()

            # Transcription display
            dpg.add_input_text(
                tag="transcription_text",
                multiline=True,
                readonly=True,
                width=750,
                height=200,
                default_value="Transcription will appear here..."
            )

        dpg.add_separator()

        # Instructions
        with dpg.collapsing_header(label="Instructions & Requirements"):
            dpg.add_text("Requirements:", color=(255, 100, 100))
            dpg.add_text("• pip install yt-dlp dearpygui SpeechRecognition pyaudio")
            dpg.add_text("• VLC Media Player installed")
            dpg.add_text("• Working microphone for transcription")

            dpg.add_separator()

            dpg.add_text("How to use:", color=(255, 200, 100))
            dpg.add_text("1. Click 'Start VLC Stream' to play audio")
            dpg.add_text("2. Click 'Start Transcription' to transcribe microphone")
            dpg.add_text("3. Speak near microphone to see transcription")
            dpg.add_text("4. Note: This transcribes YOUR voice, not the stream")

            dpg.add_separator()

            if SPEECH_RECOGNITION_AVAILABLE:
                dpg.add_text("✓ Speech Recognition Available", color=(100, 255, 100))
            else:
                dpg.add_text("✗ Speech Recognition Not Available", color=(255, 100, 100))
                dpg.add_text("Install: pip install SpeechRecognition pyaudio")

    dpg.create_viewport(title="YouTube Audio Transcriber", width=820, height=640)
    dpg.setup_dearpygui()
    dpg.set_primary_window("main_window", True)
    dpg.show_viewport()
    dpg.start_dearpygui()
    dpg.destroy_context()


if __name__ == "__main__":
    main()