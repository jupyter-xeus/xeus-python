import json
from pathlib import Path

__all__ = ["__version__"]


def _fetchVersion():
    HERE = Path(__file__).parent.resolve()

    try:
        with open(HERE / "package.json", "r") as f:
            version = json.load(f)["version"]
            return (
                version.replace("-alpha.", "a")
                .replace("-beta.", "b")
                .replace("-rc.", "rc")
            )
    except FileNotFoundError:
        pass


__version__ = _fetchVersion()
