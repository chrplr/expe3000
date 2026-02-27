# Building expe3000 on Windows with MinGW-w64

This guide provides step-by-step instructions for setting up a MinGW-w64 environment and compiling SDL3 (along with its satellite libraries) from source to build the `expe3000` project.

## 1. Prerequisites: MSYS2 Setup

[MSYS2](https://www.msys2.org/) is the recommended way to get a working MinGW-w64 environment on Windows.

1.  **Download and Install MSYS2:**
    Download the installer from [msys2.org](https://www.msys2.org/) and follow the installation instructions.
2.  **Open the "MSYS2 MinGW 64-bit" terminal:**
    Find it in your Start menu. **Do not** use the standard MSYS terminal for compilation; ensure you use the **MinGW 64-bit** one.
3.  **Update the package database and core packages:**
    ```bash
    pacman -Syu
    ```
4.  **Install the Toolchain and Build Tools:**
    ```bash
    pacman -S --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-pkgconf git
    ```
5.  **Install SDL3 and Dependencies:**
    SDL3 and its satellite libraries are now available directly in the MSYS2 repositories:
    ```bash
    pacman -S --needed mingw-w64-x86_64-sdl3 mingw-w64-x86_64-sdl3-image mingw-w64-x86_64-sdl3-ttf
    ```

---

## 2. Compiling expe3000

With the dependencies installed, you can build the main project using CMake and Ninja.

1.  **Navigate to the project directory.**
2.  **Configure and Build:**
    ```bash
    # Configure
    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

    # Build
    cmake --build build
    ```

---

## 3. Compiling SDL3 from Source (Optional)

If you need a version newer than what is in `pacman`, you can still build from source:

### Build SDL3 Core
```bash
git clone https://github.com/libsdl-org/SDL.git
cmake -B SDL/build -S SDL -G Ninja -DCMAKE_INSTALL_PREFIX=/mingw64 -DCMAKE_BUILD_TYPE=Release
cmake --build SDL/build
cmake --install SDL/build
```

*(Follow similar steps for SDL3_image and SDL3_ttf if needed.)*

---

## 4. Running the Program

After a successful build, the executable `expe3000.exe` will be in the `build` folder.

**Note on DLLs:** Since you linked against shared libraries, the SDL3 DLLs must be in your PATH or in the same folder as the executable. If you are running from the MSYS2 MinGW 64-bit terminal, they are already in your PATH (`/mingw64/bin`).

### Distributing your build
To run the `.exe` outside of the MSYS2 terminal, you need to copy the required DLLs to the same folder as the executable. You can find them in `/mingw64/bin/`. At a minimum, you will usually need:
- `SDL3.dll`
- `SDL3_image.dll`
- `SDL3_ttf.dll`
- `libwinpthread-1.dll`
- `libgcc_s_seh-1.dll` (or similar depending on your toolchain)

To run it:
```bash
./expe3000.exe ../experiment.csv --font ../assets/arial.ttf
```
