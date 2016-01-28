# PicoEngine
The Cogitare Computing Pico Engine.
A minimal bare bones OpenGL ES 2.0 + OpenAL engine for Win32, Rasperry Pi, Android and Windows Universal

Pico::Engine

A very small, very simple, base of an OpenGL ES 2.0 game engine, that can load Wavefront OBJ files with PNG textures
and play sound effects and OGG music through OpenAL.

Inspired by picoPNG http://lodev.org/lodepng/ .

Uses:
picoPNG http://lodev.org/lodepng/ (obviously) 
tiny_obj_loader https://github.com/syoyo/tinyobjloader
stb_vorbis https://github.com/nothings/stb 

Warning: Only use this engine for rapid development of prototypes 
as it is in no way an optimized engine implementation nor is
its design suitably scaleable for anything other than very small projects. 

The Pico Engine itself is only the picoengine.cpp/h files.
The engine uses tiny_obj_loader to parse the 3d obj files,
PicoPng to load png files,
and stb_vorbis to load OGG files.

Depending on your system you will need to install an OpenGL ES library and an OpenAL library.
For instance the Win32 example is setup to use ANGLE as OpenGL ES library and creatives OpenAL for sound,
while windows phone and android expect an OpenAL-soft implementation, and so on...

But that's it ;-)


The following macros switch between different implementation details for different platforms
PICO_WINDOWS        - A windows version that requires ANGLE https://code.google.com/p/angleproject/
PICO_ANDROID        - An android version using native glue, very basic support
PICO_PI             - A Raspberry Pi version
PICO_UNIVERSAL		- A windows universal version that uses the ANGLE package available with the OpenGL ES samples in Visual Studio 2015.

USE_ARRAY_BUFFERS   - Use array buffer objects instead of client side memory for vertex data. 
SOUND_SUPPORT       - Basic sound effect and wave-file loading support using OpenAL
OGG_PLAYBACK        - Ogg Vorbis playback using stb_vorbis decoder http://www.nothings.org/stb_vorbis/ . Requires SOUND_SUPPORT

Some examples of things that have been made with Pico::Engine.

Atelerix: http://www.cogitarecomputing.com/Projects/atelerix

Ro-Langs: http://ludumdare.com/compo/ludum-dare-33/?action=preview&uid=16225

Core Invaders: http://ludumdare.com/compo/ludum-dare-34/?action=preview&uid=16225

