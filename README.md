# random-image
A light weight header only (well, not really, you'll see) image library specializing on handling images used for GPU rendering and texturing, like 3D texture, cube map, mipmap chain and such.

# Why?
You may ask why another image library, given there are already good ones like [std_image.h](https://github.com/nothings/stb/blob/master/stb_image.h).

Those libraries are fantastic when dealing with commonly seen images like .bmp/.jpg and etc. If that's what you need, then go for it.

But when it comes to image/texture specific to GPU rendering, like 3d textures, cubemap, mipmap chain, I still couldn't find a good open source library that handles them nicely, let along of dealing with those strange GPU specific pixel formats like D24_UNORM_S8_UINT, R10G11B10_UNORM and etc.

# License
The library is released under MIT license. See [LICENSE](LICENSE) file for details.
