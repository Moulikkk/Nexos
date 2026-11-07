#include <doctest/doctest.h>
#include <nexos/nexos.hpp>

TEST_CASE("Nexos version is correct")
{
    CHECK(nexos::version() == "0.1.0");
}
