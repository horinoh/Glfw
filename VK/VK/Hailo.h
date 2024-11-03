#pragma once

#ifdef _WIN64
#pragma warning(push)
#pragma warning(disable : 4201)
#else
#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored ""
#endif
#include <hailo/hailort.h>
#ifdef _WIN64
#pragma warning(pop)
#else
#pragma GCC diagnostic pop
#endif
