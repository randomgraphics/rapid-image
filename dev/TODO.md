# P0 (required to make the repository public)

- integrate with stb_image_write.h to save to .jpg/.png/.bmp
- save .dds
- setup CI

# P1 (can postpone after the repo is public)
- see if we can move all compile time macros from rapid-image.h into rapid-image.cpp
  - this is to ensure that we RIL is compiled into a library, its behavior is not affected by the definition of the macros.