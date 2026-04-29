# PublishSubscribe Linux, Windows and FreeRTOS/ESP32

Agnostic, Lightweight and Portable Publish-Subscribe Helper adapted for FreeRTOS/ESP32

- written in C++17/C++20 using templated classes
- synchronous/asynchronous observer
- topic subscription
- buildable on FreeRTOS/ESP32 and Linux/Windows

Goodies:

- simple thread-safe dictionary helper on top of std::map
- simple thread-safe queue on top of std::queue
- simple waitable object on top of FreeRTOS event_group or C++ std::mutex and std::condition_variable
- simple non_copyable abstract class
- simple periodic task helper on top of FreeRTOS task or C++ thread
- simple worker task helper on top of FreeRTOS task or C++ thread
- simple data processing task helper on top of FreeRTOS task or C++ thread
- simple thread-safe ring buffer and iterable ring buffer on top of std::array
- simple thread-safe resizeable ring buffer and iterable resizeable ring buffer on top of std::vector
- simple single producer/single consumer lock-free ring buffer on top of std::array and scalar types/pointers
- simple single producer/single consumer memory pipe on top of FreeRTOS memory buffer and lock-free ring buffer
- queuable commands
- bytepack serialization in C++20 using header-only 3rd party (Faruk Eryilmaz - MIT license)
- json serialization and deserialization using 3rd party with result-based API in this project (Dave Gamble & Dmitry Pankratov - MIT license)
- gzip compression / decompression C++ wrapper using 3rd party (Paul Sokolovsky, Joergen Ibsen & Simon Tatham - Zlib license)
- some logging macros
- finite state machine based on std::variant, std::visit and overload pattern with transitions and states callbacks (based on
  Rainer Grimm and Bartlomiej Filipek C++ publications)
- C++20 calendar day test
- simple timer helper using 3rdparty (Michael Egli - MIT license) for standard implementation and using FreeRTOS timer for FreeRTOS platform
- custom pool allocator for global new/new[]/delete/delete[]
- `tools::expected`: lightweight result type for exception-free APIs, carrying either a value or a structured error
- fixed-point arithmetic using header-only 3rd party `fpm` library (Mike Lankamp - MIT license)

[GitHub repository](https://github.com/type-one/PublishSubscribeESP32)

AI/Agent guidance files:

- [CLAUDE.md](CLAUDE.md)
- [.github/copilot-instructions.md](.github/copilot-instructions.md)

Third Parties used in the examples:

- [bytepack](https://github.com/farukeryilmaz/bytepack) (v0.1.0)
- [fpm](https://github.com/MikeLankamp/fpm) (master b46537f, header-only fixed-point math library, MIT license)
- [cjsonpp](https://github.com/ancwrd1/cjsonpp) (master 71d876f)
- [cJSON](https://github.com/DaveGamble/cJSON) (v1.7.18)
- [CException](https://github.com/ThrowTheSwitch/CException) (v1.3.4)
- [uzlib](https://github.com/pfalcon/uzlib) (v2.9.5)
- [tinf](https://github.com/jibsen/tinf) (from uzlib v2.9.5 bundle)
- [cpptime](https://github.com/eglimi/cpptime) (master 08abf1d)

JSON integration note:

- The project now uses the non-throwing cjsonpp API based on result values (`parse_result`, `get`, `set`, `add`, `remove`).
- See `main/cjsonpp/README.md` for current usage examples.

Publications:

- [Finite State Machines in Variant](https://www.cppstories.com/2023/finite-state-machines-variant-cpp/)
- [Visiting a std::variant with the overload pattern](https://www.modernescpp.com/index.php/visiting-a-std-variant-with-the-overload-pattern/)

## What

Test program written in C++17/C++20 to implement a simple Publish/Subscribe pattern.

The code is portable and lightweight.

C++20 is only required for the bytepack test in main.cpp

## Why

An attempt to write a flexible little framework that can be used on desktop PCs and embedded systems
(micro-computers and micro-controllers) that are able to compile and run modern C++ code.

## How

Can be compiled on FreeRTOS/ESP32, Linux and Windows, and should be easily
adapted for other platforms (micro-computers, micro-controllers)

### Build on FreeRTOS/ESP32

On FreeRTOS/ESP32 and [Expressif-IDF SDK](https://github.com/espressif/esp-idf) and [Expressif Doc](https://docs.espressif.com).

You can install the Expressif SDK easily under Linux or WSL2

```bash
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
git checkout v5.5.4
git submodule update --init --recursive
./install.sh
cd ..
```
Change the version of the SDK accordingly in the `git checkout` command.

Assuming the ESP32 device is connected to an USB port on your PC

If you have an ESP32-S3, you can then use the following settings

```bash
source ~/esp-idf/export.sh

idf.py set-target esp32s3

idf.py build

idf.py flash monitor
```
Note: `Ctrl-T Ctrl-X will exit the monitor.` 

For an ESP32 classic, just type `idf.py set-target esp32` above instead.

If you have an ESP32-C5, type `idf.py set-target esp32c5` and use a SDK that supports the Risc-V toolchain (ex. 5.5.1 or above).

If you develop under Windows/WSL2, this tool is useful to bind the serial port:
[usbipd-win](https://github.com/dorssel/usbipd-win).

Use then Powershell as admin and type

```bash
usbipd list
```
to get the list of USB devices.

Then bind and attach the USB device by typing with the proper `BUSID`

```bash
usbipd bind --busid=<BUSID>
usbipd attach --wsl --busid=<BUSID>
```

Note: Under WSL2 if you got `usbipd: error: Loading vhci_hcd failed.` when launching `usbipd attach --wsl` try `sudo modprobe vhci_hcd` in you linux shell.

Be sure to `chmod 666 your /dev/ttyUSB0 or /dev/ttyACM0` if you got a flashing error.

### Build on Linux and Windows

You need to go in the main sub-folder to find the proper cmake file for your system

```bash
cd main

rename CMakeLists.txt to CMakeLists.copy 

rename CMakeLists_PC.txt to CMakeLists.txt
```

Then

On Linux, just use `cmake . -B build` and then go to build and run make (or ninja)

On Windows, just use `cmake-gui` to generate a Visual Studio solution

### Linux desktop build modes (speed vs analysis)

The desktop CMake files provide two build modes:

- fast local build (default): `clang-tidy` disabled
- analysis build: `clang-tidy` enabled explicitly

Fast local build (recommended for day-to-day dev):

```bash
cd main
cmake -S . -B build -G Ninja
cmake --build build -j
```

Analysis build (slower, static analysis enabled):

```bash
cd main
cmake -S . -B build_tidy -G Ninja -DENABLE_CLANG_TIDY=ON
cmake --build build_tidy -j
```

`ccache` is used as compiler launcher when available on Linux.
To verify cache activity:

```bash
ccache -s
```

Useful maintenance commands:

```bash
ccache -z   # reset stats
ccache -s   # inspect hit/miss ratio after a build
```


## Memory usage

If you build on Linux you can use valgrind to profile the memory usage (with or without custom allocator enabled):

```bash
valgrind --tool=massif ./publish_subscribe

massif-visualizer ./massif.out.xxxxxx
```

The purpose of the custom allocator is to pre-allocate tiny blocks and reuse them over the time to prevent memory fragmentation and to minimize the need to allocate new blocks from the heap.

Indeed heavy and high frequency usage of dynamic heap allocation for events/messages can cause memory fragmentation, in particular on platforms with a limited amount of memory available such as the ESP32. 

More info on:

[Valgrind and Massiv](https://gist.github.com/felipeek/f9e4392cfe9a9e65dc52048e91ac58ea)
[Valgrind manual](https://valgrind.org/docs/manual/ms-manual.html)
[About custom allocators](https://www.rastergrid.com/blog/sw-eng/2021/03/custom-memory-allocators/)

## Components In main/

The `main/` folder combines first-party framework code and bundled third-party dependencies used by the project.

- `bytepack/`: third-party binary serialization library used for compact payload encoding/decoding in examples and tests.
- `CException/`: third-party lightweight C-style exception support used by some vendor-side integrations.
- `cJSON/`: third-party C JSON parser/printer backend used as the low-level JSON engine.
- `cjsonpp/`: C++ wrapper over cJSON adapted in this repository to use result-based (exception-free) error handling.
- `cpptime/`: portable timer component (`CppTime::Timer`) used for one-shot and periodic timeout scheduling on desktop builds.
- `fpm/`: third-party header-only fixed-point arithmetic library used where deterministic fixed-point math is preferred.
- `portable_concurrency/`: future/promise/executor-style asynchronous framework integrated with this project (result-oriented adaptation).
- `tools/`: core first-party abstraction layer (tasks, synchronization primitives, observer/pub-sub helpers, containers, logging, utilities) with FreeRTOS and desktop backends.
- `uzlib/`: third-party compression/decompression backend used by the gzip wrapper.
- `examples/`: runnable sample scenarios that demonstrate framework usage patterns and integrations.
- `tests/`: unit tests and validation coverage for framework modules and adapters.

## Author

Laurent Lardinois / Type One (TFL-TDV)

[LinkedIn profile](https://be.linkedin.com/in/laurentlardinois)

[Demozoo profile](https://demozoo.org/sceners/19691/)
