# Building expe3000 on Linux

This guide covers installing dependencies and building the project on various Linux distributions.

## 1. Prerequisites: Build Tools

Regardless of your distribution, you will need `cmake`, `pkg-config`, and a C compiler (`gcc` or `clang`).

### Ubuntu / Debian / Mint / Pop!_OS
```bash
sudo apt update
sudo apt install build-essential cmake pkg-config git
```

### Fedora / RHEL / CentOS
```bash
sudo dnf groupinstall "Development Tools"
sudo dnf install cmake pkg-config git
```

### Arch Linux / Manjaro
```bash
sudo pacman -Syu
sudo pacman -S base-devel cmake pkg-config git
```

---

## 2. Option A: Installing via Package Manager

As SDL3 is relatively new, it may only be available in the "edge" or "testing" repositories of some distributions.

### Ubuntu 24.10+ / Debian Trixie+
```bash
sudo apt install libsdl3-dev libsdl3-image-dev libsdl3-ttf-dev
```

### Arch Linux (AUR)
If not in the official repos, you can use an AUR helper like `yay`:
```bash
yay -S sdl3-git sdl3_image-git sdl3_ttf-git
```

### Fedora 41+
```bash
sudo dnf install sdl3-devel sdl3_image-devel sdl3_ttf-devel
```

---

## 3. Option B: Compiling SDL3 from Source (Universal)

If your distribution does not yet provide SDL3 packages, you must compile it from source.

### Install Library Dependencies first:
- **Ubuntu/Debian:** `sudo apt install libasound2-dev libpulse-dev libdbus-1-dev libwayland-dev libx11-dev libxcursor-dev libxext-dev libxi-dev libxinerama-dev libxrandr-dev libxss-dev libxtst-dev libjpeg-dev libpng-dev libwebp-dev libfreetype6-dev libharfbuzz-dev`
- **Fedora:** `sudo dnf install alsa-lib-devel pulseaudio-libs-devel libX11-devel libXcursor-devel libXext-devel libXi-devel libXinerama-devel libXrandr-devel libXScrnSaver-devel libXtst-devel wayland-devel libjpeg-turbo-devel libpng-devel libwebp-devel freetype-devel harfbuzz-devel`

### Build SDL3 Core
```bash
git clone https://github.com/libsdl-org/SDL.git
cd SDL
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
sudo make install
cd ../..
```

### Build SDL3_image
```bash
git clone https://github.com/libsdl-org/SDL_image.git
cd SDL_image
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
sudo make install
cd ../..
```

### Build SDL3_ttf
```bash
git clone https://github.com/libsdl-org/SDL_ttf.git
cd SDL_ttf
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
sudo make install
cd ../..
```

---

## 4. Compiling expe3000

Once the libraries are installed:

1.  **Navigate to the project directory.**
2.  **Configure and Build:**
    It is highly recommended to perform an **out-of-source build**. Using `Ninja` is recommended for faster builds if you installed it.

    ```bash
    # Configure
    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

    # Build
    cmake --build build
    ```

    The binary `expe3000` will be created in the `build/` directory.

## 5. Troubleshooting

- **Library not found:** If you installed SDL3 to `/usr/local/lib` (the default for source builds), you may need to update the linker cache:
  ```bash
  sudo ldconfig
  ```
- **Permission Denied:** Ensure your user is part of the `audio` or `video` group if you encounter issues accessing hardware, though this is rarely needed on modern distros using PipeWire/Wayland.
