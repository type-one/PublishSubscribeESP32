# PublishSubscribe

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
- queuable commands
- bytepack serialization in C++20 using header-only 3rd party (Faruk Eryilmaz - MIT license)
- json serialization and deserialization using 3rd party (Dave Gamble & Dmitry Pankratov - MIT license)
- gzip compression / decompression C++ wrapper using 3rd party (Paul Sokolovsky, Joergen Ibsen & Simon Tatham - Zlib license)
- some logging macros
- finite state machine based on std::variant, std::visit and overload pattern with transitions and states callbacks (based on
  Rainer Grimm and Bartlomiej Filipek C++ publications)

https://github.com/type-one/PublishSubscribeESP32

Third Parties used in the examples:
https://github.com/farukeryilmaz/bytepack
https://github.com/ancwrd1/cjsonpp
https://github.com/DaveGamble/cJSON
https://github.com/ThrowTheSwitch/CException
https://github.com/pfalcon/uzlib
https://github.com/jibsen/tinf

Publications:
https://www.cppstories.com/2023/finite-state-machines-variant-cpp/
https://www.modernescpp.com/index.php/visiting-a-std-variant-with-the-overload-pattern/

# What

Small test program written in C++17/C++20 to implement a simple Publish/Subscribe pattern. 
The code is portable and lightweight.

C++20 is only required for the bytepack test in main.cpp

# Why

An attempt to write a flexible little framework that can be used on desktop PCs and embedded systems
(micro-computers and micro-controllers) that are able to compile and run modern C++ code.

# How

Can be compiled on FreeRTOS/ESP32, Linux and Windows, and should be easily
adapted for other platforms (micro-computers, micro-controllers)

## Build on FreeRTOS/ESP32

On FreeRTOS/ESP32 and Expressif-IDF SDK (https://docs.espressif.com and https://github.com/espressif/esp-idf)

Assuming the ESP32 device is connected to an USB port on your PC

If you have an ESP32-S3, you can then use the following settings

source ~/esp-idf/export.sh

idf.py set-target esp32s3

idf.py build

idf.py flash monitor

For an ESP32 classic, just type idf.py set-target esp32 above instead 

If you develop under Windows/WSL2, this tool is useful to bind the serial port:
https://github.com/dorssel/usbipd-win

Be sure to chmod 666 your /dev/ttyUSB0 or /dev/ttyACM0 if you got a flashing error 

## Build on Linux and Windows

You need to go in the main sub-folder to find the proper cmake file for your system

cd main

rename CMakeLists.txt to CMakeLists.copy 

rename CMakeLists_PC.txt to CMakeLists.txt

Then 

On Linux, just use cmake . -B build  and then go to build and run make (or ninja)

On Windows, just use cmake-gui to generate a Visual Studio solution

# Author

Laurent Lardinois / Type One (TFL-TDV)

https://be.linkedin.com/in/laurentlardinois

https://demozoo.org/sceners/19691/
