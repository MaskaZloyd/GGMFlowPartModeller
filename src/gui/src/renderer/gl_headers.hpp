#pragma once

// Provide OpenGL 3.3+ function declarations.
// On Linux/Mesa, GL_GLEXT_PROTOTYPES makes all GL functions directly available.
// For Windows support, replace with glad or similar loader.
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
