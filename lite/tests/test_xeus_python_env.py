"""Test creating Python envs for jupyterlite-xeus-python."""

import os
from tempfile import TemporaryDirectory
from pathlib import Path

from jupyterlite.app import LiteStatusApp

from jupyterlite_xeus_python.env_build_addon import XeusPythonEnv


def test_python_env():
    app = LiteStatusApp(log_level="DEBUG")
    app.initialize()
    manager = app.lite_manager

    addon = XeusPythonEnv(manager)
    addon.packages = ["numpy", "ipyleaflet"]

    for step in addon.post_build(manager):
        pass

    # Check env
    assert os.path.isdir("/tmp/xeus-python-kernel/envs/xeus-python-kernel")

    assert os.path.isfile("/tmp/xeus-python-kernel/envs/xeus-python-kernel/bin/xpython_wasm.js")
    assert os.path.isfile("/tmp/xeus-python-kernel/envs/xeus-python-kernel/bin/xpython_wasm.wasm")

    # Check empack output
    assert os.path.isfile(Path(addon.cwd.name) / "python_data.js")
    assert os.path.isfile(Path(addon.cwd.name) / "python_data.data")

    os.remove(Path(addon.cwd.name) / "python_data.js")
    os.remove(Path(addon.cwd.name) / "python_data.data")


def test_python_env_from_file_1():
    app = LiteStatusApp(log_level="DEBUG")
    app.initialize()
    manager = app.lite_manager

    addon = XeusPythonEnv(manager)
    addon.environment_file = "environment-1.yml"

    for step in addon.post_build(manager):
        pass

    # Check env
    assert os.path.isdir("/tmp/xeus-python-kernel/envs/xeus-python-kernel-1")

    assert os.path.isfile("/tmp/xeus-python-kernel/envs/xeus-python-kernel-1/bin/xpython_wasm.js")
    assert os.path.isfile("/tmp/xeus-python-kernel/envs/xeus-python-kernel-1/bin/xpython_wasm.wasm")

    # Check empack output
    assert os.path.isfile(Path(addon.cwd.name) / "python_data.js")
    assert os.path.isfile(Path(addon.cwd.name) / "python_data.data")

    os.remove(Path(addon.cwd.name) / "python_data.js")
    os.remove(Path(addon.cwd.name) / "python_data.data")
