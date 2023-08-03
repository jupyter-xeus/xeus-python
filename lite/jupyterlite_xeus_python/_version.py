import json
import sys
from pathlib import Path

__all__ = ["__version__"]


def _fetchVersion():
    prefix = Path(sys.prefix)
    package = prefix / "share" / "jupyter" / "labextensions" / "@jupyterlite" / "xeus-python-kernel" / "package.json"

    with open(package, "r") as f:
        version = json.load(f)["version"]
        return (
            version.replace("-alpha.", "a")
            .replace("-beta.", "b")
            .replace("-rc.", "rc")
        )


__version__ = _fetchVersion()
