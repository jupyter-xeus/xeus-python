import sys
import json
from pathlib import Path

from ._version import __version__

HERE = Path(__file__).parent.resolve()


def _jupyter_labextension_paths():
    prefix = sys.prefix
    # For when in dev mode
    if (
        HERE.parent
        / "share"
        / "jupyter"
        / "labextensions"
        / "@jupyterlite"
        / "xeus-python-kernel"
    ).parent.exists():
        prefix = HERE.parent

    return [
        {
            "src": prefix
            / "share"
            / "jupyter"
            / "labextensions"
            / "@jupyterlite"
            / "xeus-python-kernel",
            "dest": "@jupyterlite/xeus-python-kernel",
        }
    ]
