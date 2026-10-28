// The card, as the cartridge: STARLIFT.DAT is appended to the ROM image (build.py) at _dat_start, and fat::open/read
// serve its 512-byte blocks from there. Only what Card.cpp calls is provided.
#include <Fat.h>
#include <string.h>

extern "C" const uint8_t _dat_start[];
extern "C" const uint32_t gba_datBlocks;      // written into the ROM by build.py (see rom_info.cpp)

namespace fat {

uint8_t open(const char *name, Run *out, uint8_t maxRuns, uint8_t *) {
    if (maxRuns < 1 || memcmp(name, "STARLIFTDAT", 11) != 0 || gba_datBlocks == 0) return 0;
    out[0].lba = 0;
    out[0].blocks = gba_datBlocks;
    return 1;
}

bool read(const Run *run, uint32_t nRuns, uint32_t k, uint8_t *dst) {
    if (!nRuns || k >= run[0].blocks) return false;
    memcpy(dst, _dat_start + k * 512u, 512);
    return true;
}

}  // namespace fat
