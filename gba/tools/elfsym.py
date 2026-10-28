"""Symbol addresses from the linked ELF: python elfsym.py STARLIFT.elf name ..."""
import struct, sys


def symbols(path):
    d = open(path, "rb").read()
    shoff, = struct.unpack_from("<I", d, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", d, 0x2E)
    secs = [struct.unpack_from("<IIIIIIIIII", d, shoff + i * shentsize) for i in range(shnum)]
    out = {}
    for s in secs:
        if s[1] == 2:  # SHT_SYMTAB
            strs = secs[s[6]]
            for i in range(s[5] // 16):
                name, val, size, info, other, shndx = struct.unpack_from("<IIIBBH", d, s[4] + i * 16)
                n = d[strs[4] + name:d.index(bytes([0]), strs[4] + name)].decode(errors="replace")
                if n:
                    out[n] = (val & ~1, size)
    return out


if __name__ == "__main__":
    sy = symbols(sys.argv[1])
    for n in sys.argv[2:]:
        print(n, hex(sy[n][0]) if n in sy else None, sy.get(n, (0, 0))[1])
