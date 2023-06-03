# P0 (required to make the repository public)

- integrate with stb_image.h to import .jpg/.png/.bmp

# P1 (can postpone after the repo is public)
- save from .dds
- see if we can move all compile time macros from rapid-image.h into rapid-image.cpp
  - this is to ensure that we RIL is compiled into a library, its behavior is not affected by the definition of the macros.