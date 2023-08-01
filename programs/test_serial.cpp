#include "modules/modbus/modbus_device.h"

#if defined(WITH_MODBUS)
#include <hv/TcpClient.h>
#include <modbus.h>
#include <sys/socket.h>
#endif

#include <codecvt>
#include <locale>

#include <iterator>
#include <regex>

template<class Facet>
struct deletable_facet : Facet {
    template<class... Args>
    explicit deletable_facet(Args &&...args) : Facet(std::forward<Args>(args)...) {}
    ~deletable_facet() override = default;
};

int main() {
#if defined(WITH_MODBUS)
    auto ctx = modbus_new_tcp("10.11.1.82", 502);
    if (modbus_connect(ctx) == -1) { printf("11111 %s\n", modbus_strerror(errno)); }

    uint8_t raw_req_3[]{0x03, 0x1C, 0x00, 0x03, 0x00, 0x00, 0xB1, 0x02, 0x00, 0x02, 0x02, 0x01, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x01, 0xC3, 0xF7, 0xCC, 0xEC, 0xD0, 0xC7, 0xC6, 0xDA, 0xCE, 0xE5};

    while (true) {
        if (modbus_send_raw_request(ctx, raw_req_3, 28 * sizeof(uint8_t)) == -1) {
            printf("==== %s\n", modbus_strerror(errno));
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    modbus_free(ctx);

#endif

    return 0;
}