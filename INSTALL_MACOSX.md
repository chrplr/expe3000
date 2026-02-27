# Building expe3000 on macOS

This guide provides detailed instructions for setting up your development environment and compiling SDL3 along with the `expe3000` project on macOS.

## 1. Prerequisites: Development Tools

You need to have the Xcode Command Line Tools and Homebrew installed.

1.  **Install Xcode Command Line Tools:**
    Open your terminal and run:
    ```bash
    xcode-select --install
    ```
2.  **Install Homebrew (if not already installed):**
    ```bash
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    ```
3.  **Install Build Tools:**
    ```bash
    brew install cmake pkg-config git
    ```

---

## 2. Option A: Fast Installation (Homebrew)

Since SDL3 is now available in Homebrew, this is the simplest method:

```bash
brew install sdl3 sdl3_image sdl3_ttf
```

---

## 3. Option B: Compiling SDL3 from Source

If you need the latest features or a specific version, follow these steps to build from source.

### SDL3 Core
1.  **Clone and Build:**
    ```bash
    git clone https://github.com/libsdl-org/SDL.git
    cd SDL
    mkdir build && cd build
    cmake -DCMAKE_BUILD_TYPE=Release ..
    make -j$(sysctl -n hw.ncpu)
    sudo make install
    cd ../..
    ```

### SDL3_image
1.  **Install dependencies:**
    ```bash
    brew install libpng jpeg-turbo libwebp
    ```
2.  **Clone and Build:**
    ```bash
    git clone https://github.com/libsdl-org/SDL_image.git
    cd SDL_image
    mkdir build && cd build
    cmake -DCMAKE_BUILD_TYPE=Release ..
    make -j$(sysctl -n hw.ncpu)
    sudo make install
    cd ../..
    ```

### SDL3_ttf
1.  **Install dependencies:**
    ```bash
    brew install freetype harfbuzz
    ```
2.  **Clone and Build:**
    ```bash
    git clone https://github.com/libsdl-org/SDL_ttf.git
    cd SDL_ttf
    mkdir build && cd build
    cmake -DCMAKE_BUILD_TYPE=Release ..
    make -j$(sysctl -n hw.ncpu)
    sudo make install
    cd ../..
    ```

---

## 4. Compiling expe3000

1.  **Navigate to the project directory.**
2.  **Configure and Build:**
    We use an "out-of-source" build to keep the root directory clean. If you installed `ninja`, you can use it for faster builds.

    ```bash
    # Configure
    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

    # Build
    cmake --build build
    ```

## 5. Running the Program

The executable `expe3000` will be created in the `build` directory.

To run it:
```bash
./expe3000 ../experiment.csv
```

**Note on Permissions:** macOS may ask for permission to access certain folders or record the screen/input. Ensure you grant these if prompted for the experiment to function correctly.
