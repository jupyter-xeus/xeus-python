import csv
import os
import shutil
import sys
from pathlib import Path
from subprocess import run
from tempfile import TemporaryDirectory
from typing import List, Optional
from urllib.parse import urlparse

import requests
import typer
import yaml
from empack.file_patterns import PkgFileFilter, pkg_file_filter_from_yaml
from empack.pack import DEFAULT_CONFIG_PATH, pack_env

try:
    from mamba.api import create as mamba_create

    MAMBA_PYTHON_AVAILABLE = True
except ImportError:
    MAMBA_PYTHON_AVAILABLE = False

MAMBA_COMMAND = shutil.which("mamba")
MICROMAMBA_COMMAND = shutil.which("micromamba")
CONDA_COMMAND = shutil.which("conda")

PYTHON_MAJOR = 3
PYTHON_MINOR = 11
PYTHON_VERSION = f"{PYTHON_MAJOR}.{PYTHON_MINOR}"

XEUS_PYTHON_VERSION = "0.15.10"

CHANNELS = [
    "https://repo.mamba.pm/emscripten-forge",
    "conda-forge",
]

PLATFORM = "emscripten-wasm32"
DEFAULT_REQUEST_TIMEOUT = 1  # in minutes


def create_env(
    env_name,
    root_prefix,
    specs,
    channels,
):
    """Create the emscripten environment with the given specs."""
    prefix_path = Path(root_prefix) / "envs" / env_name

    if MAMBA_PYTHON_AVAILABLE:
        mamba_create(
            env_name=env_name,
            base_prefix=root_prefix,
            specs=specs,
            channels=channels,
            target_platform=PLATFORM,
        )
        return

    channels_args = []
    for channel in channels:
        channels_args.extend(["-c", channel])

    if MAMBA_COMMAND:
        # Mamba needs the directory to exist already
        prefix_path.mkdir(parents=True, exist_ok=True)
        return _create_env_with_config(MAMBA_COMMAND, prefix_path, specs, channels_args)

    if MICROMAMBA_COMMAND:
        run(
            [
                MICROMAMBA_COMMAND,
                "create",
                "--yes",
                "--no-pyc",
                "--root-prefix",
                root_prefix,
                "--name",
                env_name,
                f"--platform={PLATFORM}",
                *channels_args,
                *specs,
            ],
            check=True,
        )
        return

    if CONDA_COMMAND:
        return _create_env_with_config(CONDA_COMMAND, prefix_path, specs, channels_args)

    raise RuntimeError(
        """Failed to create the virtual environment for xeus-python,
        please make sure at least mamba, micromamba or conda is installed.
        """
    )


def _create_env_with_config(conda, prefix_path, specs, channels_args):
    run(
        [conda, "create", "--yes", "--prefix", prefix_path, *channels_args],
        check=True,
    )
    _create_config(prefix_path)
    run(
        [
            conda,
            "install",
            "--yes",
            "--prefix",
            prefix_path,
            *channels_args,
            *specs,
        ],
        check=True,
    )


def _create_config(prefix_path):
    with open(prefix_path / ".condarc", "w") as fobj:
        fobj.write(f"subdir: {PLATFORM}")
    os.environ["CONDARC"] = str(prefix_path / ".condarc")


def _install_pip_dependencies(prefix_path, dependencies, log=None):
    # Why is this so damn complicated?
    # Isn't it easier to download the .whl ourselves? pip is hell

    if log is not None:
        log.warning(
            """
            Installing pip dependencies. This is very much experimental so use
            this feature at your own risks.
            Note that you can only install pure-python packages.
            pip is being run with the --no-deps option to not pull undesired
            system-specific dependencies, so please install your package dependencies
            from emscripten-forge or conda-forge.
            """
        )

    # Installing with pip in another prefix that has a different Python version IS NOT POSSIBLE
    # So we need to do this whole mess "manually"
    pkg_dir = TemporaryDirectory()

    run(
        [
            sys.executable,
            "-m",
            "pip",
            "install",
            *dependencies,
            # Install in a tmp directory while we process it
            "--target",
            pkg_dir.name,
            # Specify the right Python version
            "--python-version",
            PYTHON_VERSION,
            # No dependency installed
            "--no-deps",
            "--no-input",
            "--verbose",
        ],
        check=True,
    )

    # We need to read the RECORD and try to be smart about what goes
    # under site-packages and what goes where
    packages_dist_info = Path(pkg_dir.name).glob("*.dist-info")

    for package_dist_info in packages_dist_info:
        with open(package_dist_info / "RECORD") as record:
            record_content = record.read()
            record_csv = csv.reader(record_content.splitlines())
            all_files = [_file[0] for _file in record_csv]

            # List of tuples: (path: str, inside_site_packages: bool)
            files = [(_file, not _file.startswith("../../")) for _file in all_files]

            # Why?
            fixed_record_data = record_content.replace("../../", "../../../")

        # OVERWRITE RECORD file
        with open(package_dist_info / "RECORD", "w") as record:
            record.write(fixed_record_data)

        non_supported_files = [".so", ".a", ".dylib", ".lib", ".exe.dll"]

        # COPY files under `prefix_path`
        for _file, inside_site_packages in files:
            path = Path(_file)

            # FAIL if .so / .a / .dylib / .lib / .exe / .dll
            if path.suffix in non_supported_files:
                raise RuntimeError(
                    "Cannot install binary PyPI package, only pure Python packages are supported"
                )

            file_path = _file[6:] if not inside_site_packages else _file
            install_path = (
                prefix_path
                if not inside_site_packages
                else prefix_path / "lib" / f"python{PYTHON_VERSION}" / "site-packages"
            )

            src_path = Path(pkg_dir.name) / file_path
            dest_path = install_path / file_path

            os.makedirs(dest_path.parent, exist_ok=True)

            shutil.copy(src_path, dest_path)


def build_and_pack_emscripten_env(  # noqa: C901, PLR0912, PLR0915
    python_version: str = PYTHON_VERSION,
    xeus_python_version: str = XEUS_PYTHON_VERSION,
    packages: Optional[List[str]] = None,
    environment_file: str = "",
    root_prefix: str = "/tmp/xeus-python-kernel",
    env_name: str = "xeus-python-kernel",
    empack_config: str = "",
    output_path: str = ".",
    build_worker: bool = False,
    force: bool = False,
    log=None,
):
    """Build a conda environment for the emscripten platform and pack it with empack."""
    if packages is None:
        packages = []
    channels = CHANNELS
    specs = [
        f"python={python_version}",
        "xeus-lite",
        "xeus-python" if not xeus_python_version else f"xeus-python={xeus_python_version}",
        *packages,
    ]
    bail_early = True

    if packages or xeus_python_version or environment_file:
        bail_early = False

    pip_dependencies = []

    # Process environment.yml file
    if environment_file and Path(environment_file).exists():
        env_file = Path(environment_file)

        bail_early = False

        with open(env_file) as f:
            env_data = yaml.safe_load(f)

        if env_data.get("name") is not None:
            env_name = env_data["name"]

        channels = env_data.get("channels", CHANNELS)

        if env_data.get("dependencies") is not None:
            dependencies = env_data["dependencies"]

            for dependency in dependencies:
                if isinstance(dependency, str) and dependency not in specs:
                    specs.append(dependency)
                elif isinstance(dependency, dict) and dependency.get("pip") is not None:
                    # If it's a local Python package, make its path relative to the environment file
                    pip_dependencies = [
                        (
                            (env_file.parent / pip_dep).resolve()
                            if os.path.isdir(env_file.parent / pip_dep)
                            else pip_dep
                        )
                        for pip_dep in dependency["pip"]
                    ]

    # Bail early if there is nothing to do
    if bail_early and not force:
        return ""

    orig_config = os.environ.get("CONDARC")

    # Cleanup tmp dir in case it's not empty
    shutil.rmtree(Path(root_prefix) / "envs", ignore_errors=True)
    Path(root_prefix).mkdir(parents=True, exist_ok=True)

    output_path = Path(output_path).resolve()
    output_path.mkdir(parents=True, exist_ok=True)

    prefix_path = Path(root_prefix) / "envs" / env_name

    try:
        # Create emscripten env with the given packages
        create_env(env_name, root_prefix, specs, channels)

        # Install pip dependencies
        if pip_dependencies:
            _install_pip_dependencies(prefix_path, pip_dependencies, log=log)

        pack_kwargs = {}

        # Download env filter config
        if empack_config:
            empack_config_is_url = urlparse(empack_config).scheme in ("http", "https")
            if empack_config_is_url:
                empack_config_content = requests.get(
                    empack_config, timeout=DEFAULT_REQUEST_TIMEOUT
                ).content
                pack_kwargs["file_filters"] = PkgFileFilter.parse_obj(
                    yaml.safe_load(empack_config_content)
                )
            else:
                pack_kwargs["file_filters"] = pkg_file_filter_from_yaml(empack_config)
        else:
            pack_kwargs["file_filters"] = pkg_file_filter_from_yaml(DEFAULT_CONFIG_PATH)

        # Pack the environment
        pack_env(
            env_prefix=prefix_path,
            relocate_prefix="/",
            outdir=output_path,
            use_cache=False,
            **pack_kwargs,
        )

        # Copy xeus-python output
        for file in ["xpython_wasm.js", "xpython_wasm.wasm"]:
            shutil.copyfile(prefix_path / "bin" / file, Path(output_path) / file)

        # Copy worker code and process it
        if build_worker:
            shutil.copytree(
                prefix_path / "share" / "xeus-lite",
                Path(output_path),
                dirs_exist_ok=True,
            )

            with open(Path(output_path) / "worker.ts") as fobj:
                worker = fobj.read()

            worker = worker.replace("XEUS_KERNEL_FILE", "'xpython_wasm.js'")
            worker = worker.replace("LANGUAGE_DATA_FILE", "'python_data.js'")
            worker = worker.replace(
                "importScripts(DATA_FILE);",
                """
                await globalThis.Module.bootstrap_from_empack_packed_environment(
                `./empack_env_meta.json`, /* packages_json_url */
                ".",               /* package_tarballs_root_url */
                false              /* verbose */
            );
            """,
            )
            with open(Path(output_path) / "worker.ts", "w") as fobj:
                fobj.write(worker)

    except Exception as e:
        raise e
    finally:
        if orig_config is not None:
            os.environ["CONDARC"] = orig_config
        elif "CONDARC" in os.environ:
            del os.environ["CONDARC"]

    return prefix_path


def main(
    python_version: str = PYTHON_VERSION,
    xeus_python_version: str = XEUS_PYTHON_VERSION,
    packages: List[str] = typer.Option([], help="The list of packages you want to install"),
    environment_file: str = typer.Option(
        "", help="The path to the environment.yml file you want to use"
    ),
    root_prefix: str = "/tmp/xeus-python-kernel",
    env_name: str = "xeus-python-kernel",
    empack_config: str = typer.Option(
        "",
        help="The empack config file to use. If not provided, the default empack config will be used",
    ),
    output_path: str = typer.Option(
        ".",
        help="The directory where to output the packed environment",
    ),
    build_worker: bool = typer.Option(
        False,
        help="Whether or not to build the TypeScript worker code for using xeus-python in JupyterLite",
    ),
):
    """Build and pack an emscripten environment."""
    return build_and_pack_emscripten_env(
        python_version,
        xeus_python_version,
        packages,
        environment_file,
        root_prefix,
        env_name,
        empack_config,
        output_path,
        build_worker,
        force=True,
    )


def start():
    typer.run(main)


if __name__ == "__main__":
    start()
