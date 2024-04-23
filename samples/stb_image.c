#include "ut/ut.h" // assert(...)
#pragma warning(disable: 4459) // declaration of '...' hides global declaration
#define STBI_ASSERT(x) assert(x)
#include "stb/stb_image.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
