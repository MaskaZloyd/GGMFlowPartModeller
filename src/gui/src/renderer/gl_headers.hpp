#pragma once

// GLEW provides OpenGL 3.3+ declarations and runtime function loading on
// Windows, Linux, and macOS. Include it before GLFW; the gui target defines
// GLFW_INCLUDE_NONE so GLFW will not pull platform GL headers first.
#include <GL/glew.h>
