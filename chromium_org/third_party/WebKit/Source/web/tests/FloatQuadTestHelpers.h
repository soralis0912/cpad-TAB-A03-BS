/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef FloatQuadTestHelpers_h
#define FloatQuadTestHelpers_h

#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatQuad.h"

#include <gtest/gtest.h>

// Gtest will automatically pick up this function if included to print out
// FloatQuads, rather than just spitting out their memory.
namespace blink {
inline void PrintTo(const FloatQuad& quad, ::std::ostream* os)
{
    *os << "FloatQuad("
        << std::fixed
        << std::setprecision(4) << "(" << quad.p1().x() << "f, " << quad.p1().y() << "f),"
        << std::setprecision(4) << "(" << quad.p2().x() << "f, " << quad.p2().y() << "f),"
        << std::setprecision(4) << "(" << quad.p3().x() << "f, " << quad.p3().y() << "f),"
        << std::setprecision(4) << "(" << quad.p4().x() << "f, " << quad.p4().y() << "f))";
    os->unsetf(std::ios::floatfield);
}
}

#endif // FloatQuadTestHelpers_h
