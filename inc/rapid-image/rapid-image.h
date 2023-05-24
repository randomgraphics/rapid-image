/*
MIT License

Copyright (c) 2023 randomgraphics

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef RAPID_IMAGE_H_
#define RAPID_IMAGE_H_

/// A monotonically increasing number that uniquely identify the revision of the header.
#define RAPID_IMAGE_HEADER_REVISION 1

/// \def RAPID_IMAGE_NAMESPACE
/// Define the namespace of rapid-image library.
#ifndef RAPID_IMAGE_NAMESPACE
#define RAPID_IMAGE_NAMESPACE rapid_image
#endif

/// \def RAPID_IMAGE_ENABLE_DEBUG_BUILD
/// Set to non-zero value to enable debug build. Disabled by default.
#ifndef RAPID_IMAGE_ENABLE_DEBUG_BUILD
#define RAPID_IMAGE_ENABLE_DEBUG_BUILD 0
#endif

namespace RAPID_IMAGE_NAMESPACE {

} // namespace RAPID_IMAGE_NAMESPACE

#endif // RAPID_IMAGE_H_

#ifdef RAPID_IMAGE_IMPLEMENTATION
#include "rapid-image.cpp"
#endif
