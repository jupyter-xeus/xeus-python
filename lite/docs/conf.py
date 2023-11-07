extensions = [
    "jupyterlite_sphinx",
    "myst_parser",
]

myst_enable_extensions = [
    "linkify",
]

master_doc = "index"
source_suffix = ".rst"

project = "jupyterlite-xeus-python"
copyright = "JupyterLite Team"
author = "JupyterLite Team"

exclude_patterns = []

html_theme = "pydata_sphinx_theme"

jupyterlite_dir = "."

html_theme_options = {
    "logo": {
        "image_light": "xeus-python.svg",
        "image_dark": "xeus-python.svg",
    }
}
