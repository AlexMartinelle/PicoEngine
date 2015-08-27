# PicoEngine
The Cogitare Computing Pico Engine. A minimal bare bones OpenGL ES 2.0 rendering engine for Win32, Rasperry Pi and Android

Pico::Engine

A very small, very simple, base of an OpenGL ES 2.0 game rendering engine, that can load Wavefront OBJ files with PNG textures.
Inspired by picoPNG http://lodev.org/lodepng/ .
Uses picoPNG http://lodev.org/lodepng/ (obviously) & tiny_obj_loader https://github.com/syoyo/tinyobjloader

Warning: Only use this engine for rapid development of prototypes. 

The Pico Engine itself is only the picoengine.cpp/h files.
The engine obviously needs picoPNG and tiny_obj_loader to work.
To use the engine examples, get a copy of tiny_obj_loader and picoPNG and apply the patch to picoPNG.
The rest of the setup will be stuff such as installing ANGLE, OpenGL ES or EGL development support on your machine.
But that's it ;-)


The following macros switch between different implementation details for different platforms
PICO_WINDOWS        - A windows version that requires ANGLE https://code.google.com/p/angleproject/
PICO_ANDROID        - An android version using native glue, very basic support
PICO_PI             - A Raspberry Pi version