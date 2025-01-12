// crill - the Cross-platform Real-time, I/O, and Low-Latency Library
// Copyright (c) 2022 - Timur Doumler and Fabian Renn-Giles
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef CRILL_CONTRACTS_H
#define CRILL_CONTRACTS_H

#include <cassert>

// This will eventually be a proper macro-based Contracts facility
// but at the moment is just an alias for C assert.
#define CRILL_PRE(x) assert(x)

#endif //CRILL_CONTRACTS_H
