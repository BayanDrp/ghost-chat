#include "ghostchat/transport/connection.hpp"

#include <cassert>
#include <iostream>

using namespace ghostchat::transport;

int main() {
    Connection c(0x02);
    assert(c.peer() == 0x02);

    Connection c2(0xFFFFFFFFFFFFFFFFULL);  // broadcast address also storable
    assert(c2.peer() == 0xFFFFFFFFFFFFFFFFULL);

    std::cout << "connection tests passed\n";
    return 0;
}
