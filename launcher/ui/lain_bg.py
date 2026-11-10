"""The looping music video behind the window, and its song.

Assets live in a folder named lain: builder-bg.mp4 (960 x 540, 30 fps, no sound) and builder-song.wav (the same 101.8
seconds). It is looked for in $LAIN_ASSETS, then next to the launcher's files (<app>/lain), then in the install root
(<root>/lain). ffmpeg is $LAIN_FFMPEG, then lain/ffmpeg/ffmpeg.exe, then the one on PATH. Without the video or ffmpeg
the window keeps its still backdrop and runs as it always did.

A reader thread runs ffmpeg at the video's own speed (-re) and keeps only the newest frame, as ready-made PPM bytes, in
a one-slot queue. The Tk thread copies that frame into a small photo and from there, doubled in size by Tk itself,
into the picture under the canvas: one copy per frame. The song is played from the thread when the first frame of
each pass arrives, and a pass that reaches its end starts both again, so picture and song loop together for as long
as the window is open and never drift apart.
"""
import atexit
import os
import queue
import re
import shutil
import subprocess
import threading
import tkinter as tk
from pathlib import Path

VIDEO, SONG = 'builder-bg.mp4', 'builder-song.wav'
POLL_MS = 16                    # how often the Tk thread looks for a new frame; the video makes one every 33 ms
NO_WINDOW = 0x08000000          # CREATE_NO_WINDOW: ffmpeg must not flash a console


def find_assets(layout):
    """The folder with the video, or None: $LAIN_ASSETS, <app>/lain, <install root>/lain, the first that has it."""
    for d in (os.environ.get('LAIN_ASSETS'), layout.app / 'lain', layout.root / 'lain'):
        if d and (Path(d) / VIDEO).is_file():
            return Path(d)
    return None


def find_ffmpeg(folder):
    for f in (os.environ.get('LAIN_FFMPEG'), folder / 'ffmpeg' / 'ffmpeg.exe', shutil.which('ffmpeg')):
        if f and Path(f).is_file():
            return str(f)
    return None


class LainBackground:
    def __init__(self, root, canvas, layout, settings, size, log, on_fail):
        """size is the canvas in pixels; on_fail is called on the Tk thread when the video stops for good."""
        self.root, self.canvas, self.settings, self.log, self.on_fail = root, canvas, settings, log, on_fail
        self.item = None
        self.live = False            # the video is (or is about to be) playing
        self.music_on = bool(settings.get('music', True))
        self.wav = None
        self.winsound = None
        self.ffmpeg = self.video = None
        self.q = queue.Queue(1)
        self.lock = threading.Lock()
        self.proc = None
        self.gen = 0                 # counts restarts and pauses: a pass of an older generation ends at once
        self.go = threading.Event()  # cleared while paused
        self.stopped = threading.Event()
        self.thread = None
        self.failed = False
        self.bars = ''
        folder = find_assets(layout)
        if not folder:
            log('lain: no assets folder (LAIN_ASSETS, <app>/lain or <root>/lain); still backdrop')
            return
        self.ffmpeg = find_ffmpeg(folder)
        if not self.ffmpeg:
            log(f'lain: no ffmpeg (LAIN_FFMPEG, {folder / "ffmpeg"} or PATH); still backdrop')
            return
        self.video = folder / VIDEO
        if (folder / SONG).is_file():
            try:
                import winsound
                self.winsound, self.wav = winsound, folder / SONG
            except ImportError:
                log('lain: no winsound here; the video plays without its song')
        else:
            log(f'lain: {SONG} is missing from {folder}; the video plays without its song')
        # half the canvas, rounded up to even: Tk doubles it, and the window crops the overhang
        self.w = (size[0] + 1) // 2 + (size[0] + 1) // 2 % 2
        self.h = (size[1] + 1) // 2 + (size[1] + 1) // 2 % 2
        self.header = b'P6 %d %d 255\n' % (self.w, self.h)
        self.fsize = self.w * self.h * 3
        self.src = tk.PhotoImage(master=root, width=self.w, height=self.h)
        self.dst = tk.PhotoImage(master=root, width=self.w * 2, height=self.h * 2)
        self.live = True
        log(f'lain: {self.video} at {self.w}x{self.h} through {self.ffmpeg}'
            + (', with its song' if self.wav else ''))

    @property
    def has_song(self):
        return self.wav is not None

    def start(self):
        if not self.live:
            return
        self.item = self.canvas.create_image(0, 0, anchor='nw', image=self.dst, tags='lainbg')
        self.canvas.tag_lower('lainbg')
        self.go.set()
        self.thread = threading.Thread(target=self.read_loop, name='lain-video', daemon=True)
        self.thread.start()
        atexit.register(self.stop)
        self.root.after(POLL_MS, self.pump)

    def lower(self):
        """Back to the bottom of the canvas after the static pictures were drawn again."""
        if self.item is not None:
            self.canvas.tag_lower('lainbg')

    # ---------------------------------------------------------------- reader thread

    def detect_bars(self):
        """The crop that leaves out the black bars the video carries at its sides, from three seconds of it; '' when
        there are none or ffmpeg cannot say."""
        try:
            out = subprocess.run([self.ffmpeg, '-hide_banner', '-ss', '5', '-t', '3', '-i', str(self.video), '-vf',
                                  'cropdetect=24:2:0', '-f', 'null', '-'], stdin=subprocess.DEVNULL,
                                 stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=20,
                                 creationflags=NO_WINDOW if os.name == 'nt' else 0).stderr.decode('utf-8', 'replace')
        except (OSError, subprocess.SubprocessError):
            return ''
        found = re.findall(r'crop=(\d+):(\d+):(\d+):(\d+)', out)
        if not found:
            return ''
        cw, ch, cx, cy = (int(v) for v in found[-1])
        return f'crop={cw}:{ch}:{cx}:{cy},' if cw >= 320 and ch >= 180 else ''

    def spawn(self):
        # cut off the bars, then scale to cover the window and crop what overhangs, so the picture keeps its shape
        vf = (self.bars + f'scale={self.w}:{self.h}:force_original_aspect_ratio=increase:flags=bilinear,'
              f'crop={self.w}:{self.h}')
        cmd = [self.ffmpeg, '-hide_banner', '-loglevel', 'error', '-re', '-i', str(self.video), '-an', '-vf', vf,
               '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-']
        return subprocess.Popen(cmd, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                                creationflags=NO_WINDOW if os.name == 'nt' else 0)

    def kill(self):
        with self.lock:
            p = self.proc
        if p is not None:
            try:
                p.kill()
            except OSError:
                pass

    def put(self, ppm):
        """The newest frame replaces an older one the Tk thread has not taken yet."""
        try:
            self.q.put_nowait(ppm)
        except queue.Full:
            try:
                self.q.get_nowait()
            except queue.Empty:
                pass
            try:
                self.q.put_nowait(ppm)
            except queue.Full:
                pass

    def play_song(self):
        if self.wav and self.music_on:
            try:
                self.winsound.PlaySound(str(self.wav), self.winsound.SND_FILENAME | self.winsound.SND_ASYNC)
            except (RuntimeError, OSError) as e:
                self.log(f'lain: the song did not play: {e}')

    def read_loop(self):
        """One pass of the video after another; the thread sleeps on the pause event and in the pipe's read."""
        self.bars = self.detect_bars()
        self.log(f'lain: bars cut by {self.bars.rstrip(",") or "nothing"}')
        while not self.stopped.is_set():
            self.go.wait()
            if self.stopped.is_set():
                return
            gen = self.gen
            try:
                with self.lock:
                    p = self.proc = self.spawn()
                    if gen != self.gen or not self.go.is_set():   # paused or restarted while it started
                        p.kill()
            except OSError as e:
                self.log(f'lain: ffmpeg did not start: {e}')
                self.fail()
                return
            frames = 0
            while True:
                data = p.stdout.read(self.fsize)   # blocks until a whole frame, which ffmpeg paces at 30 a second
                if gen != self.gen or self.stopped.is_set() or len(data) < self.fsize:
                    break
                if frames == 0:
                    self.play_song()               # the song starts with the first frame of the pass
                frames += 1
                self.put(self.header + data)
            try:
                p.kill()
                p.wait(2)
                p.stdout.close()
            except (OSError, subprocess.TimeoutExpired):
                pass
            if frames == 0 and gen == self.gen and self.go.is_set() and not self.stopped.is_set():
                self.log('lain: ffmpeg gave no picture; still backdrop')
                self.fail()
                return
            # the video ended (gen unchanged): the next pass starts the song again too; after a pause or restart the
            # loop waits on the pause event first

    def fail(self):
        self.failed = True
        self.stop_sound()
        try:
            self.root.after(0, self.failed_ui)
        except (tk.TclError, RuntimeError):
            pass

    # ---------------------------------------------------------------- Tk thread

    def pump(self):
        if self.stopped.is_set() or self.failed:
            return
        try:
            ppm = self.q.get_nowait()
        except queue.Empty:
            ppm = None
        if ppm is not None:
            try:
                self.src.configure(data=ppm, format='ppm')
                self.root.tk.call(self.dst, 'copy', self.src, '-zoom', 2, 2)
            except tk.TclError as e:
                self.log(f'lain: a frame was refused: {e}')
                self.failed = True
                self.kill()
                self.stop_sound()
                self.failed_ui()
                return
        self.root.after(POLL_MS, self.pump)

    def failed_ui(self):
        if self.item is not None:
            self.canvas.delete(self.item)
            self.item = None
        self.live = False
        self.on_fail()

    # ---------------------------------------------------------------- song, pause, end

    def stop_sound(self):
        if self.winsound:
            try:
                self.winsound.PlaySound(None, 0)
            except (RuntimeError, OSError):
                pass

    def restart(self):
        """Both from their start again: the video cannot be cut in at the song's position, so they begin together."""
        with self.lock:
            self.gen += 1
        self.kill()
        self.go.set()

    def toggle_music(self):
        self.music_on = not self.music_on
        self.settings['music'] = self.music_on
        try:
            self.settings.save()
        except OSError as e:
            self.log(f'lain: the music choice was not saved: {e}')
        if self.music_on:
            self.restart()
        else:
            self.stop_sound()
        return self.music_on

    def pause(self):
        """While the game runs: no picture, no song, no ffmpeg."""
        if not self.live:
            return
        self.go.clear()
        with self.lock:
            self.gen += 1
        self.kill()
        self.stop_sound()

    def resume(self):
        if self.live:
            self.go.set()

    def stop(self):
        """On every way out: ffmpeg ends and the song stops."""
        self.stopped.set()
        self.go.set()
        self.kill()
        self.stop_sound()
