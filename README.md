# UsAR Mirror

## Development

**1. Before Anything:** 
- Make sure to have CMake, Ninja installed.
- Please install `glfw3`, `glad` via this [link](https://www.glfw.org).
- This project relies on `vcpkg` for dependency management. Please install by following this [link](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started?pivots=shell-bash), and set `VCPKG_ROOT` in `CMakeUserPresets.json` after cloning.

**2. Clone Repository**

```bash
git clone <this repo link> --recurse-submodules #--recurse-submodules important!!!
```

**3. CMake**

```bash
cmake --preset=default # CMakePresets.json, CMakeUserPresets.json
cmake --build build # Generates build in ${PROJECT_ROOT}/build
```
