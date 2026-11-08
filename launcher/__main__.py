"""python -m launcher [--selftest [--shots DIR]] — the launcher window (launcher/ui/app.py)."""
import sys

from .ui.app import main

sys.exit(main())
