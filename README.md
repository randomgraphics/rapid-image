# random-image
A light weight header only (well, not really, you'll see) image library specializing on handling images used for GPU texturing, like 3D texture, cube map, mipmap chain and such.

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
#define RAPID_VULKAN_IMPLEMENTATION
#include <rapid-image/rapid-image.h>
```

# Support to PNG/JPG/BMP file formats
By default rapid-vulkan library supports loading & saving image from/to **.RIL** and **.DDS** file.

**RIL** is the built-in file format for rapid-image library.

**DDS** is Microsoft's texture file format. This library is using the file spec described on this page: https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dx-graphics-dds-pguide

To support other commonly seen image formats, like .PNG, .JPG and .BMP. You'll need the [stb_image.h](https://github.com/nothings/stb/blob/master/stb_image.h) and [stb_image_write.h](https://github.com/nothings/stb/blob/master/stb_image_write.h). Just include these 2 headers in the same file as where you include rapid image implmentation:

```c
// enable loading from PNG/JPG/BMP file
#define STB_IMAGE_IMPLEMENTATION
#include "../3rd-party/stb/stb_image.h"

// enale saving to PNG/JPG/BMP file
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../3rd-party/stb/stb_image_write.h"

#define RAPID_VULKAN_IMPLEMENTATION
#include <rapid-image/rapid-image.h>
```

# License
The library is released under MIT license. See [LICENSE](LICENSE) file for details.
