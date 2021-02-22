// Forward declaration of the circular buffer and its adaptor.

// Copyright (c) 2003-2008 Jan Gaspar
// Copyright (c) 2016 Jonathon Racz		// C++11 standalone conversion.

// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//  See www.boost.org/libs/circular_buffer for documentation.

#pragma once

#include <memory>

namespace cb {

template <class T, class Alloc = std::allocator<T> >
class circular_buffer;

template <class T, class Alloc = std::allocator<T> >
class circular_buffer_space_optimized;

} // namespace boost

