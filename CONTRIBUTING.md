# Contributing to Xeus-python

Xeus and xeus-python are subprojects of Project Jupyter and subject to the [Jupyter governance](https://github.com/jupyter/governance) and [Code of conduct](https://github.com/jupyter/governance/blob/master/conduct/code_of_conduct.md).

## General Guidelines

For general documentation about contributing to Jupyter projects, see the [Project Jupyter Contributor Documentation](https://jupyter.readthedocs.io/en/latest/contributor/content-contributor.html).

## Community

The Xeus team organizes public video meetings. The schedule for future meetings and minutes of past meetings can be found on our [team compass](https://jupyter-xeus.github.io/).

## Setting up a development environment

First, you need to fork the project. Then setup your environment:

```bash
# create a new conda environment
conda create -n xeus-python -c conda-forge -c defaults xtl nlohmann_json cppzmq xeus pybind11 pybind11_json jedi pygments
conda activate xeus-python

# download xeus-python from your GitHub fork
git clone https://github.com/<your-github-username>/xeus-python.git
```

You may also want to install a C++ compiler, and cmake from conda if they are not available on your system.

