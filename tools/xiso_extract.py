# Usage: python -I tools/xiso_extract.py <image.iso> <out dir> [--list]
# Extracts the XDVDFS game partition of an Xbox 360 disc image (full ISO or trimmed XISO).
import os, struct, sys

SECTOR = 2048
MAGIC = b'MICROSOFT*XBOX*MEDIA'
# game partition offsets: trimmed XISO, XGD2, XGD3, XGD1
PARTITIONS = (0, 0xFD90000, 0x2080000, 0x18300000)


def find_partition(f, length):
    for base in PARTITIONS:
        if base + 0x10000 + SECTOR > length:
            continue
        f.seek(base + 0x10000)
        d = f.read(SECTOR)
        if d.startswith(MAGIC) and d[-20:] == MAGIC:
            root_sector, root_size = struct.unpack('<II', d[20:28])
            return base, root_sector, root_size
    raise SystemExit('no XDVDFS game partition found')


def walk(f, base, sector, size, prefix, out):
    if not size:
        return
    f.seek(base + sector * SECTOR)
    data = f.read(size)
    stack, seen = [0], set()
    while stack:
        off = stack.pop()
        if off in seen or off + 14 > len(data):
            continue
        seen.add(off)
        left, right, start, length, attr, n = struct.unpack('<HHIIBB', data[off:off + 14])
        if left == 0xFFFF:  # padding
            continue
        name = data[off + 14:off + 14 + n].decode('ascii')
        if name in ('', '.', '..') or '/' in name or '\\' in name:
            raise SystemExit(f'unsafe name in image: {name!r}')
        rel = prefix + name
        if attr & 0x10:
            walk(f, base, start, length, rel + '/', out)
        else:
            out.append((rel, base + start * SECTOR, length))
        for child in (left, right):
            if child:
                stack.append(child * 4)


def main(iso, dest, list_only=False):
    length = os.path.getsize(iso)
    with open(iso, 'rb') as f:
        base, root_sector, root_size = find_partition(f, length)
        print(f'partition at 0x{base:X}')
        files = []
        walk(f, base, root_sector, root_size, '', files)
        files.sort(key=lambda e: e[0].lower())
        total = sum(e[2] for e in files)
        print(f'{len(files)} files, {total / 2**30:.2f} GiB')
        if list_only:
            for rel, _, size in files:
                print(f'{size:12d}  {rel}')
            return
        for rel, off, size in files:
            path = os.path.join(dest, *rel.split('/'))
            os.makedirs(os.path.dirname(path), exist_ok=True)
            f.seek(off)
            with open(path, 'wb') as o:
                left = size
                while left:
                    chunk = f.read(min(left, 8 << 20))
                    if not chunk:
                        raise SystemExit(f'image truncated in {rel}')
                    o.write(chunk)
                    left -= len(chunk)
        print('done')


if __name__ == '__main__':
    if len(sys.argv) < 3:
        raise SystemExit(__doc__ or 'usage: xiso_extract.py <iso> <out> [--list]')
    main(sys.argv[1], sys.argv[2], '--list' in sys.argv[3:])
