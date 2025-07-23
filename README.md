# MIA: Multiscale Image Analysis
MIA (Multiscale Image Analysis) is a visualization software for spectral imaging data. Please [contact me](mailto:hennes.rave@uni-muenster.de) for access to the software and cite it like this:

> Hennes Rave, **MIA: Multiscale Image Analysis**, https://github.com/hennesrave/multiscale-image-analysis
## Examples
![Teaser Image](./images/teaser.png)
##### Data Courtesy by Katharina Kronenberg, University of Graz. [[Paper]](https://chemrxiv.org/engage/chemrxiv/article-details/650d598eed7d0eccc301cd03)

## Build instructions
- Install [Qt](https://www.qt.io/) version 6.9.1 and [Qt Visual Studio Tools](https://marketplace.visualstudio.com/items?itemName=TheQtCompany.QtVisualStudioTools2022) 
- Install [Python 3.13.5](https://www.python.org/downloads/release/python-3135/) and copy `Python313/include/` and `Python313/libs/` to `external/Python313/`
- Download [pybind11 v3.0.0](https://github.com/pybind/pybind11/releases/tag/v3.0.0) and copy `pybind11-3.0.0/include/pybind11/` to `external/` 
- Download [Python 3.13.5 Windows embeddable package (64-bit)](https://www.python.org/ftp/python/3.13.5/python-3.13.5-embed-amd64.zip) and extract to `build/python/`
    - Add `"Lib/site-packages"` to `build/python/python313._pth`
- Download [get-pip.py](https://bootstrap.pypa.io/get-pip.py) and save it to `build/python/`
- Download [RenderDoc](https://renderdoc.org/) and copy `RenderDoc/renderdoc_app.h` to `external/`
- Download [json.hpp](https://github.com/nlohmann/json/blob/develop/single_include/nlohmann/json.hpp) and save it to `external/`