// port_scanner test.h
#include "../port_scanner.h"

#include <iostream>

#ifdef WIN32
#include <Windows.h>
class CHCP {
public:
    CHCP() {
        SetConsoleOutputCP(65001);
    }
} chcp;
#endif // WIN32

int main() {
    xiunneg::WSADATARAII wsa_data_raii;

    xiunneg::PortScanner::Config config{
        .begin = 5600,
        .end = 5620,
        .scan_interval = 5, // 5s
    };

    xiunneg::PortScanner port_scanner(config);

    port_scanner.run();

    std::this_thread::sleep_for(std::chrono::seconds(60));

    port_scanner.stop();

    return 0;
}