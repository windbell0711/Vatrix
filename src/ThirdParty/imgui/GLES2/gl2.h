// vx: shim so imgui_impl_opengl3 (compiled with IMGUI_IMPL_OPENGL_ES2) resolves
// <GLES2/gl2.h> against the game's bundled glad GLES2 loader
#pragma once
#include <glad/gles2.h>

// vx: the real <GLES2/gl2.h> defines GL_ES so shaders can pick the ES precision dialect
#ifndef GL_ES
#define GL_ES 1
#endif
