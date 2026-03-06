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
- json serialization and deserialization using 3rd party (Dave Gamble & Dmitry Pankratov - MIT license)
- gzip compression / decompression C++ wrapper using 3rd party (Paul Sokolovsky, Joergen Ibsen & Simon Tatham - Zlib license)
- some logging macros
- finite state machine based on std::variant, std::visit and overload pattern with transitions and states callbacks (based on
  Rainer Grimm and Bartlomiej Filipek C++ publications)
- C++20 calendar day test
- simple timer helper using 3rdparty (Michael Egli - MIT license) for standard implementation and using FreeRTOS timer for FreeRTOS platform
- C++20 only: custom pool allocator for global new/new[]/delete/delete[]

[GitHub repository](https://github.com/type-one/PublishSubscribeESP32)

Third Parties used in the examples:

- [bytepack](https://github.com/farukeryilmaz/bytepack) (v0.1.0)
- [cjsonpp](https://github.com/ancwrd1/cjsonpp) (master 71d876f)
- [cJSON](https://github.com/DaveGamble/cJSON) (v1.7.18)
- [CException](https://github.com/ThrowTheSwitch/CException) (v1.3.4)
- [uzlib](https://github.com/pfalcon/uzlib) (v2.9.5)
- [tinf](https://github.com/jibsen/tinf) (from uzlib v2.9.5 bundle)
- [cpptime](https://github.com/eglimi/cpptime) (master 08abf1d)

Publications:

- [Finite State Machines in Variant](https://www.cppstories.com/2023/finite-state-machines-variant-cpp/)
- [Visiting a std::variant with the overload pattern](https://www.modernescpp.com/index.php/visiting-a-std-variant-with-the-overload-pattern/)

## What

Small test program written in C++17/C++20 to implement a simple Publish/Subscribe pattern. 
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

Assuming the ESP32 device is connected to an USB port on your PC

If you have an ESP32-S3, you can then use the following settings

```bash
source ~/esp-idf/export.sh

idf.py set-target esp32s3

idf.py build

idf.py flash monitor
```

For an ESP32 classic, just type `idf.py set-target esp32` above instead.

If you develop under Windows/WSL2, this tool is useful to bind the serial port:
[usbipd-win](https://github.com/dorssel/usbipd-win)

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

## Author

Laurent Lardinois / Type One (TFL-TDV)

[LinkedIn profile](https://be.linkedin.com/in/laurentlardinois)

[Demozoo profile](https://demozoo.org/sceners/19691/)
