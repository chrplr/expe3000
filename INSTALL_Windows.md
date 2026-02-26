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
    *(If the terminal closes, restart it and run the command again until everything is up to date.)*
4.  **Install the Toolchain and Build Tools:**
    ```bash
    pacman -S --needed base-devel mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake git
    ```
5.  **Install Dependencies for SDL3 Satellites:**
    SDL3_image and SDL3_ttf require additional libraries (like libpng, freetype, etc.):
    ```bash
    pacman -S --needed mingw-w64-x86_64-libpng mingw-w64-x86_64-libjpeg-turbo mingw-w64-x86_64-libwebp mingw-w64-x86_64-freetype mingw-w64-x86_64-harfbuzz
    ```

---

## 2. Compiling SDL3 from Source

Since SDL3 is in active development, compiling from the latest source is often necessary.

1.  **Clone the SDL3 repository:**
    ```bash
    git clone https://github.com/libsdl-org/SDL.git
    cd SDL
    ```
2.  **Configure and Build:**
    ```bash
    mkdir build && cd build
    cmake -G "MinGW Makefiles" -DCMAKE_INSTALL_PREFIX=/mingw64 ..
    cmake --build . -j$(nproc)
    cmake --install .
    cd ../..
    ```

---

## 3. Compiling SDL3 Satellite Libraries

### SDL3_image
1.  **Clone and Build:**
    ```bash
    git clone https://github.com/libsdl-org/SDL_image.git
    cd SDL_image
    mkdir build && cd build
    cmake -G "MinGW Makefiles" -DCMAKE_INSTALL_PREFIX=/mingw64 ..
    cmake --build . -j$(nproc)
    cmake --install .
    cd ../..
    ```

### SDL3_ttf
1.  **Clone and Build:**
    ```bash
    git clone https://github.com/libsdl-org/SDL_ttf.git
    cd SDL_ttf
    mkdir build && cd build
    cmake -G "MinGW Makefiles" -DCMAKE_INSTALL_PREFIX=/mingw64 ..
    cmake --build . -j$(nproc)
    cmake --install .
    cd ../..
    ```

---

## 4. Compiling expe3000

Now that the dependencies are installed in your `/mingw64` prefix, you can build the main project.

1.  **Navigate to the project directory:**
    ```bash
    cd /path/to/expe3000
    ```
2.  **Configure and Build:**
    ```bash
    mkdir build && cd build
    cmake -G "MinGW Makefiles" ..
    cmake --build .
    ```

## 5. Running the Program

After a successful build, the executable `expe3000.exe` will be in the `build` folder.

**Note on DLLs:** Since you linked against shared libraries, the SDL3 DLLs must be in your PATH or in the same folder as the executable. If you are running from the MSYS2 MinGW 64-bit terminal, they are already in your PATH (`/mingw64/bin`).

To run it:
```bash
./expe3000.exe ../experiment.csv --font ../assets/arial.ttf
```
