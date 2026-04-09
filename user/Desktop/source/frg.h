#pragma once

#include <new>
#include <frg/vector.hpp>
#include <frg/std_compat.hpp>

template<typename T>
using frg_vector = frg::vector<T, frg::stl_allocator>;
