# UsAR Mirror

***
## Setup

I have only built this on Debian 12 on x86. If you're on Windows, use WSL2 if you want a smoother experience (otherwise feel free to compile using something like Visual Studio -- it might be easier as OpenPose has prebuilt binaries for _only_ Windows). If you're on Mac OSX, good luck.

Ultimately the target is a Jetson Orin Nano which runs Ubuntu 20 using ARM architecture. Please refrain from using anything too fancy/hardware specific and we should be fine... 

**1. Clone Repository**

```bash
git clone <this repo link> --recurse-submodules 
# --recurse-submodules important !!!
```

**2. Prebuild**
- Make sure to have CMake and Ninja installed.
- Install `glfw2`, `glad` via this [link](https://www.glfw.org).
- Install `glm` via this [link](https://github.com/g-truc/glm).
- Install `vcpkg` by via this [link](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started?pivots=shell-bash), and set `VCPKG_ROOT` in `CMakeUserPresets.json` after cloning.

**3. Install OpenPose***

I haven't figured out how to generate OpenPose binaries on the fly in this project's CMake config yet, so you will have to build/install dynamic lib (.so) files manually.
 1. When cloning this repository, you should have also pulled `openpose` as a submodule under `/deps` in the project root. `cd` to that directory. 
 2. Follow this [link](https://github.com/CMU-Perceptual-Computing-Lab/openpose/blob/master/doc/installation/0_index.md#compiling-and-running-openpose-from-source) to build OpenPose from source. Make sure to run the demo to make sure it works.
 3. We will now install the C++ API. Navigate to this [link](https://cmu-perceptual-computing-lab.github.io/openpose/web/html/doc/md_doc_advanced_deployment.html) some documentation. What will happen is you (1) `cd /deps/openpose/build` and (2) `sudo make install`. This should install the respective OpenPose headers and libraries into your system path (something like `/usr/local`).

_*Note: I still haven't built OpenPose on the Jetson..._

**4. Configure CMakeUserPresets.json**

In CMakeUserPresets.json: 
- Set `VCPKG_ROOT` to your `vcpkg` installation root.

**5. Generate build files + Build**

```bash
cmake --preset=default # CMakePresets.json, CMakeUserPresets.json
cmake --build build # Generates build in ${PROJECT_ROOT}/build
```

**6. Run**
```bash
cd <your project>/build
./UsARMirror # You may have to set LD_LIBRARY_PATH environment variable to load .so files.
```

***
## Development
We have `.clang-format`. Please use it.
