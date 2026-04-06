# Pixel Tris

Single-file Tetris-style game in C++17 using [SFML](https://www.sfml-dev.org/) (graphics, window, audio). Marathon, Sprint, VS CPU, hold piece, garbage battle, and pixel-art style UI.

## Dependencies

| Requirement | Notes |
|-------------|--------|
| **C++17** | GCC, Clang, or MSVC with C++17 enabled |
| **SFML 2.5+** | Modules used: `Graphics`, `Window`, `System`, `Audio` |
| **Compiler** | `g++` or `clang++` on Unix; MSVC or MinGW on Windows |

No CMake or extra libraries are required; one `main.cpp` links directly against SFML.

---

## Linux

### 1. Install a compiler and SFML

**Debian / Ubuntu / Linux Mint**

```bash
sudo apt update
sudo apt install build-essential libsfml-dev
```

**Fedora**

```bash
sudo dnf install gcc-c++ SFML-devel
```

**Arch Linux**

```bash
sudo pacman -S --needed base-devel sfml
```

**openSUSE**

```bash
sudo zypper install gcc-c++ sfml-devel
```

### 2. Build

```bash
cd /path/to/tetriscpptest
g++ -std=c++17 -O2 -o tetris main.cpp -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio
```

### 3. Run

```bash
./tetris
```

If the dynamic linker cannot find SFML, install the same `sfml` / `libsfml-dev` package family your distro uses for **runtime** (usually the `-dev` package pulls in or matches the runtime libs).

---

## Windows

Pick one approach.

### Option A: MSYS2 (MinGW) — good match for the same `g++` command

1. Install [MSYS2](https://www.msys2.org/) and open **MSYS2 UCRT64** (or MINGW64).
2. Update and install tools + SFML:

   ```bash
   pacman -Syu
   pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-SFML
   ```

   (If you use the **MINGW64** environment instead, use the `mingw-w64-x86_64-*` package names.)

3. Build (from the folder that contains `main.cpp`):

   ```bash
   cd /c/path/to/tetriscpptest
   g++ -std=c++17 -O2 -o tetris.exe main.cpp -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio
   ```

4. Run `tetris.exe` from the same MSYS2 shell, or copy needed SFML DLLs next to the exe if you run it from Explorer (DLLs are under the MSYS2 `bin` directory).

### Option B: Visual Studio + vcpkg

1. Install [Visual Studio](https://visualstudio.microsoft.com/) with **Desktop development with C++**.
2. Clone [vcpkg](https://github.com/microsoft/vcpkg), run `bootstrap-vcpkg.bat`, then install SFML (64-bit):

   ```powershell
   .\vcpkg install sfml:x64-windows
   .\vcpkg integrate install
   ```

3. Create an **Empty project** (C++), set **C++ language standard** to **ISO C++17** (Project → Properties → C/C++ → Language).
4. Add `main.cpp` to the project.
5. Build **x64**. With `vcpkg integrate install`, MSVC should pick up vcpkg’s include and library paths automatically.
6. In **Project → Properties → Linker → Input → Additional Dependencies**, add:

   `sfml-graphics.lib;sfml-window.lib;sfml-system.lib;sfml-audio.lib`

   (If the linker reports missing symbols, check the [SFML 2 Visual Studio tutorial](https://www.sfml-dev.org/tutorials/2.6/start-vc.php) for any extra system libraries your SFML build expects.)
7. Copy SFML (and dependency) **DLLs** from vcpkg’s `installed\x64-windows\bin` next to your built `tetris.exe`, or run the exe from a shell where that folder is on `PATH`.

### Option C: Prebuilt SFML + MinGW (manual)

1. Download the **GCC** build of SFML 2.x for your MinGW flavor from [sfml-dev.org/download](https://www.sfml-dev.org/download.php).
2. Add SFML’s `include` and `lib` to your compile/link flags and ship SFML DLLs with the executable (see SFML’s official “GCC” tutorial for Windows).

---

## macOS

### 1. Install Xcode command-line tools (compiler)

```bash
xcode-select --install
```

### 2. Install SFML (Homebrew)

```bash
brew install sfml
```

### 3. Build

From the directory that contains `main.cpp`:

```bash
cd /path/to/tetriscpptest
clang++ -std=c++17 -O2 -o tetris main.cpp -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio
```

If the linker cannot find SFML, add the Homebrew prefix (Apple Silicon vs Intel differs):

```bash
# Apple Silicon (typical)
clang++ -std=c++17 -O2 -o tetris main.cpp -I/opt/homebrew/include -L/opt/homebrew/lib \
  -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio

# Intel Mac (typical)
clang++ -std=c++17 -O2 -o tetris main.cpp -I/usr/local/include -L/usr/local/lib \
  -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio
```

### 4. Run

```bash
./tetris
```

---

## Controls (in-game)

| Key | Action |
|-----|--------|
| Arrows | Move / soft drop |
| Up / Z / X | Rotate |
| Space | Hard drop |
| Tab | Hold |
| P | Pause |
| F | Fullscreen |
| M | Mute |
| Esc | Pause menu / back |

---

## License

No license file is included in this repository; add one if you want to specify terms for others.
