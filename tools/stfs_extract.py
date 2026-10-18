# Usage: python -I tools/re/stfs_extract.py <package> <out dir> [--list]. Checks every block SHA-1.
import sys, struct, hashlib, os
BPH = [170, 170*170, 170**3]
def main(path, out, list_only=False):
    f = open(path, 'rb'); d = f.read()
    assert d[:4] in (b'LIVE', b'PIRS', b'CON ')
    header_size = struct.unpack('>I', d[0x340:0x344])[0]
    vd = d[0x379:0x379+0x24]
    flags = vd[2]
    ro = flags & 1; root_active = (flags >> 1) & 1
    ft_count = struct.unpack('<H', vd[3:5])[0]
    ft_block = int.from_bytes(vd[5:8], 'little')
    total = struct.unpack('>I', vd[0x1C:0x20])[0]
    vol_type = struct.unpack('>I', d[0x3A9:0x3AD])[0]
    print(f'header_size=0x{header_size:X} flags=0x{flags:X} ro={ro} vol_type={vol_type} ft={ft_count}@{ft_block} total_blocks={total}')
    assert vol_type == 0, 'SVOD not handled'
    bpht = 1 if ro else 2
    base_off = (header_size + 0xFFF) & ~0xFFF
    step0 = BPH[0] + bpht
    step1 = BPH[1] + (BPH[0] + 1) * bpht
    def b2o(bi):
        base = BPH[0]; blk = bi
        for i in range(3):
            blk += ((bi + base) // base) * bpht
            if bi < base: break
            base *= BPH[0]
        return base_off + (blk << 12)
    def hashblk(bi, lvl):
        if lvl == 0:
            if bi < BPH[0]: return 0
            blk = (bi // BPH[0]) * step0 + ((bi // BPH[1]) + 1) * bpht
            return blk if bi < BPH[1] else blk + bpht
        if lvl == 1:
            if bi < BPH[1]: return step0
            return (bi // BPH[1]) * step1 + bpht
        return step1
    def hoff(bi, lvl):
        return base_off + (hashblk(bi, lvl) << 12)
    def active_offset(bi):
        # pick primary/secondary table for non-read-only packages
        if ro: return 0
        sec = 0x1000 if root_active else 0
        if total > BPH[0]:
            # level-1 entry for this level-0 table carries its active flag
            o1 = hoff(bi, 1) + sec
            e = d[o1 + ((bi // BPH[0]) % BPH[0]) * 0x18: ][:0x18]
            sec = 0x1000 if (e[0x14] & 0x40) else 0
        return sec
    def hentry(bi):
        o = hoff(bi, 0) + active_offset(bi) + (bi % BPH[0]) * 0x18
        e = d[o:o+0x18]
        return e[:0x14], e[0x14], int.from_bytes(e[0x15:0x18], 'big')
    entries = []
    bi = ft_block; bad = 0
    for n in range(ft_count):
        blk = d[b2o(bi):b2o(bi)+0x1000]
        for m in range(0x40):
            e = blk[m*0x40:(m+1)*0x40]
            if e[0] == 0: break
            nl = e[0x28] & 0x3F
            name = e[:nl].decode('latin1')
            is_dir = bool(e[0x28] & 0x80)
            start = int.from_bytes(e[0x2F:0x32], 'little')
            alloc = int.from_bytes(e[0x2C:0x2F], 'little')
            parent = struct.unpack('>H', e[0x32:0x34])[0]
            length = struct.unpack('>I', e[0x34:0x38])[0]
            entries.append((name, is_dir, start, alloc, parent, length))
        bi = hentry(bi)[2]
        if bi == 0xFFFFFF: break
    paths = []
    for name, is_dir, start, alloc, parent, length in entries:
        p = name if parent == 0xFFFF else paths[parent] + '/' + name
        paths.append(p)
    nfiles = 0; nbytes = 0
    for (name, is_dir, start, alloc, parent, length), p in zip(entries, paths):
        if is_dir:
            if not list_only: os.makedirs(os.path.join(out, p), exist_ok=True)
            continue
        data = bytearray(); bi = start; rem = length; nb = 0
        while rem and bi != 0xFFFFFF:
            o = b2o(bi); chunk = d[o:o+0x1000]
            h, st, nxt = hentry(bi)
            if hashlib.sha1(chunk).digest() != h: bad += 1
            data += chunk[:min(0x1000, rem)]; rem -= min(0x1000, rem); bi = nxt; nb += 1
        assert rem == 0, f'{p}: short by {rem}'
        nfiles += 1; nbytes += length
        print(f'{length:10d}  {p}')
        if not list_only:
            fp = os.path.join(out, p); os.makedirs(os.path.dirname(fp) or out, exist_ok=True)
            open(fp, 'wb').write(data)
    print(f'{nfiles} files, {nbytes} bytes, {bad} blocks failed SHA-1')
    return bad
if __name__ == '__main__':
    lo = '--list' in sys.argv; a = [x for x in sys.argv[1:] if x != '--list']
    sys.exit(1 if main(a[0], a[1] if len(a) > 1 else '.', lo) else 0)
