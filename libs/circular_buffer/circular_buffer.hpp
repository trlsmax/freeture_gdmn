// Circular buffer library header file.

// Copyright (c) 2003-2008 Jan Gaspar
// Copyright (c) 2016 Jonathon Racz		// C++11 standalone conversion.

// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//  See www.boost.org/libs/circular_buffer for documentation.

#pragma once

#include "circular_buffer_fwd.hpp"
#include <type_traits>
#include <cassert>
#include <iterator>

#include "circular_buffer/debug.hpp"
#include "circular_buffer/details.hpp"
#include "circular_buffer/base.hpp"
#include "circular_buffer/space_optimized.hpp"
