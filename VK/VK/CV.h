#pragma once

#ifdef _WIN64
#pragma warning(push)
#pragma warning(disable : 4127)
#pragma warning(disable : 4819)
#else
#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored ""
#endif
#include <opencv2/opencv.hpp>
#ifdef _WIN64
#pragma warning(pop)
#else
#pragma GCC diagnostic pop
#endif
