# rapid-image library
A light weight header only image library specializing on handling images used for GPU texturing, like 3D texture, cube map, mipmap chain and such.

# Why?
You may ask why another image library, given there are already good ones like [std_image.h](https://github.com/nothings/stb/blob/master/stb_image.h).

Those libraries are fantastic when dealing with commonly seen images like .bmp/.jpg and etc. If that's what you need, then go for it.

But when it comes to image/texture specific to GPU rendering, like 3d textures, cubemap, mipmap chain, I still couldn't find a good open source library that handles them nicely, let along of dealing with those strange GPU specific pixel formats like D24_UNORM_S8_UINT, R10G11B10_UNORM and etc.

# Basic Usage
The library is compiled against C++17 standard. Everything you need is included in [inc](inc) folder.

Here's how you integrate it with your project:

1. Copy everything in [inc](inc) folder into your project's source folder.
2. Include [rapid-image.h](inc/rapid-image/rapid-image.h) anywhere you like.
3. In **one and only one** of your source files, include [rapid-image.h](inc/rapid-image/rapid-image.h) with RAPID_IMAGE_IMPLEMENTATION macro defined in front of it. Then you are good to go:

```c
// in your header:
#include <rapid-image/rapid-image.h>

// in one of your source files:
#define RAPID_IMAGE_IMPLEMENTATION
#include <rapid-image/rapid-image.h>
```

# Pixel Format

One of the unique parts of this library is how pixel format is defined. Instead of using enum, pixel format is represented by a 32 bits structure:

```c
struct PixelFormat {
  unsigned int layout    : 7;
  unsigned int reserved0 : 1;
  unsigned int sign0     : 4;
  unsigned int sign12    : 4;
  unsigned int sign3     : 4;
  unsigned int swizzle0  : 3;
  unsigned int swizzle1  : 3;
  unsigned int swizzle2  : 3;
  unsigned int swizzle3  : 3;
};

0        1          2               3         <- Byte number
+------+-+----+-----+-----+-----+-----+-----+-----+     
|Layout|-|SI0 |SI12 |SI3  |SW0  |SW1  |SW2  |SW3  |
+------+-+----+-----+-----+-----+-----+-----+-----+ 
| 0-6  |7|8-11|12-15|16-18|19-21|22-24|25-27|28-31|
```

- **Layout** field is a enum that defines the number of channels and how many bits are there in each channel. Such as 8_8_8_8, 10_10_10_2 and etc.
- **Sign** fields defines the number format of each channel, such as UNORM, SNORM, FLOAT and etc. Note that we only have enough space for 3 sign values. So sign for channel 1 and 2 are combined together.
- **Swizzle** fields defines how channel are swizzled. It has 6 values: X, Y, Z, W, 0, 1.

With this structure we can define pixel format that maps to format enums from all existing modern graphics APIs in a very consistent way. It is also very intuitive to retrieve each color channel's properties, like number of bits, formats and etc, from this format definition.

You are also able to define unconventional pixel formats if that's what you need. For example, you can have a pixel format that stores unsigned integer in R channel, and floating pointer value in G channel.

# Support to PNG/JPG/BMP file formats
By default rapid-vulkan library supports loading & saving image from/to **.RIL** and **.DDS** file.

**RIL** is the built-in file format for rapid-image library.

**DDS** is Microsoft's texture file format. This library is using the file spec described on this page: https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dx-graphics-dds-pguide

To support other commonly seen image formats, like .PNG, .JPG and .BMP. You'll need the [stb_image.h](https://github.com/nothings/stb/blob/master/stb_image.h) and [stb_image_write.h](https://github.com/nothings/stb/blob/master/stb_image_write.h). Just include these 2 headers in the same file as where you include rapid image implmentation:

```c
// enable loading from PNG/JPG/BMP file
#define STB_IMAGE_IMPLEMENTATION
#include "../3rd-party/stb/stb_image.h"

// enable saving to PNG/JPG/BMP file
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../3rd-party/stb/stb_image_write.h"

#define RAPID_IMAGE_IMPLEMENTATION
#include <rapid-image/rapid-image.h>
```

# Build
The library itself is header-only. There's nothing to build. The folllowing instructions are for building samples and tests.

## Build on Ubuntun 22.04

Run the following command to install dependencies:
```sh
sudo apt update
sudo apt install cmake python3-pip clang-14 clang-format-14
python3 -m pip install termcolor
```

From the root of the repository, run the following commmand to initialize build environment:
```sh
./env.sh
```

Then run the following command to build:

```sh
b d # build debug variant
b p # build profile variant
b r # build release variant
b --help # show build help screen.
```

After build is done. Run the following command for check-in-tests:
```sh
cit.py
```

# License
The library is released under MIT license. See [LICENSE](LICENSE) file for details.
