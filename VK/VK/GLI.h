#pragma once

#ifdef _WIN64
#pragma warning(push)
#pragma warning(disable:4100)
#pragma warning(disable:4189)
#pragma warning(disable:4244)
#pragma warning(disable:4458)
#pragma warning(disable:5054)
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic ignored "-Wtype-limits"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#endif
#include <gli/gli.hpp>
#include <gli/target.hpp>
#ifdef _WIN64
#pragma warning(pop)
#else
#pragma GCC diagnostic pop
#endif
