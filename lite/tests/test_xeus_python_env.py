"""Test creating Python envs for jupyterlite-xeus-python."""

import os
from pathlib import Path

import pytest
from jupyterlite_core.app import LiteStatusApp

from jupyterlite_xeus_python.env_build_addon import XeusPythonEnv


def test_python_env():
    app = LiteStatusApp(log_level="DEBUG")
    app.initialize()
    manager = app.lite_manager

    addon = XeusPythonEnv(manager)
    addon.packages = ["numpy", "ipyleaflet"]

    for _step in addon.post_build(manager):
        pass

    # Check env
    assert os.path.isdir("/tmp/xeus-python-kernel/envs/xeus-python-kernel")

    assert os.path.isfile("/tmp/xeus-python-kernel/envs/xeus-python-kernel/bin/xpython_wasm.js")
    assert os.path.isfile("/tmp/xeus-python-kernel/envs/xeus-python-kernel/bin/xpython_wasm.wasm")

    # Check empack output
    assert os.path.isfile(Path(addon.cwd.name) / "empack_env_meta.json")

    os.remove(Path(addon.cwd.name) / "empack_env_meta.json")


def test_python_env_from_file_1():
    app = LiteStatusApp(log_level="DEBUG")
    app.initialize()
    manager = app.lite_manager

    addon = XeusPythonEnv(manager)
    addon.environment_file = "environment-1.yml"

    for _step in addon.post_build(manager):
        pass

    # Check env
    assert os.path.isdir("/tmp/xeus-python-kernel/envs/xeus-python-kernel-1")

    assert os.path.isfile("/tmp/xeus-python-kernel/envs/xeus-python-kernel-1/bin/xpython_wasm.js")
    assert os.path.isfile("/tmp/xeus-python-kernel/envs/xeus-python-kernel-1/bin/xpython_wasm.wasm")

    # Checking pip packages
    assert os.path.isdir("/tmp/xeus-python-kernel/envs/xeus-python-kernel-1/lib/python3.11")
    assert os.path.isdir(
        "/tmp/xeus-python-kernel/envs/xeus-python-kernel-1/lib/python3.11/site-packages"
    )
    assert os.path.isdir(
        "/tmp/xeus-python-kernel/envs/xeus-python-kernel-1/lib/python3.11/site-packages/ipywidgets"
    )
    assert os.path.isdir(
        "/tmp/xeus-python-kernel/envs/xeus-python-kernel-1/lib/python3.11/site-packages/ipycanvas"
    )
    assert os.path.isdir(
        "/tmp/xeus-python-kernel/envs/xeus-python-kernel-1/lib/python3.11/site-packages/py2vega"
    )

    # Checking labextensions
    assert os.path.isdir(
        "/tmp/xeus-python-kernel/envs/xeus-python-kernel-1/share/jupyter/labextensions/@jupyter-widgets/jupyterlab-manager"
    )
    assert os.path.isdir(
        "/tmp/xeus-python-kernel/envs/xeus-python-kernel-1/share/jupyter/labextensions/ipycanvas"
    )

    # Check empack output
    assert os.path.isfile(Path(addon.cwd.name) / "empack_env_meta.json")

    os.remove(Path(addon.cwd.name) / "empack_env_meta.json")


def test_python_env_from_file_3():
    app = LiteStatusApp(log_level="DEBUG")
    app.initialize()
    manager = app.lite_manager

    addon = XeusPythonEnv(manager)
    addon.environment_file = "test_package/environment-3.yml"

    for _step in addon.post_build(manager):
        pass

    # Test
    assert os.path.isdir(
        "/tmp/xeus-python-kernel/envs/xeus-python-kernel-3/lib/python3.11/site-packages/test_package"
    )
    assert os.path.isfile(
        "/tmp/xeus-python-kernel/envs/xeus-python-kernel-3/lib/python3.11/site-packages/test_package/hey.py"
    )

    os.remove(Path(addon.cwd.name) / "empack_env_meta.json")


def test_python_env_from_file_2():
    app = LiteStatusApp(log_level="DEBUG")
    app.initialize()
    manager = app.lite_manager

    addon = XeusPythonEnv(manager)
    addon.environment_file = "environment-2.yml"

    with pytest.raises(RuntimeError, match="Cannot install binary PyPI package"):
        for _step in addon.post_build(manager):
            pass
