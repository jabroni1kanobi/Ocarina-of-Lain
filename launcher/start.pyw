"""What NX2navi.exe runs: python\\pythonw.exe app\\<version>\\launcher\\start.pyw [arguments].

The embeddable Python puts only its ._pth entries on sys.path, so the release folder (the parent of launcher/) goes
first here. A failure before the window is up is written to data/logs/crash.log, shown in a message box and ends the
process with exit code 86, which tells the stub to go back to the previous version (launcher/native/nx2navi.c).
"""
import os
import sys
import traceback

APP = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, APP)
STARTUP_FAILED = 86


def crash(text):
    root = os.environ.get('NX2NAVI_LAUNCHER_ROOT') or os.path.dirname(os.path.dirname(APP))
    try:
        logs = os.path.join(root, 'data', 'logs')
        os.makedirs(logs, exist_ok=True)
        with open(os.path.join(logs, 'crash.log'), 'a', encoding='utf-8') as f:
            f.write(text + '\n')
    except OSError:
        pass
    try:
        import ctypes
        ctypes.windll.user32.MessageBoxW(None, 'NX2navi could not start.\n\n' + text[-1500:], 'NX2navi', 0x10)
    except Exception:
        print(text, file=sys.stderr)


try:
    from launcher.ui.app import main
except Exception:
    crash(traceback.format_exc())
    sys.exit(STARTUP_FAILED)

try:
    code = main()
except SystemExit:
    raise
except Exception:
    crash(traceback.format_exc())
    sys.exit(1)
sys.exit(code or 0)
