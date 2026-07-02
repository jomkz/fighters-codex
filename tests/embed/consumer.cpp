// Minimal fa-content-shaped consumer: a shared library that links fx::lib.
#include <fx/ealib.h>

extern "C" int consumer_entry_count(const unsigned char* data, size_t size) {
    return (int)fx::ealib_read_dir(data, size).size();
}
