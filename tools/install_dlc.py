# Usage: python -I tools/install_dlc.py <user data root or --dlc_root folder> <package> [<package> ...]
# Unpacks Crackdown 2 Marketplace packages (STFS, content type 00000002) where the SDK's content manager looks
# for them: <root>/0000000000000000/4D5308BC/00000002/<package name>/ plus Headers/00000002/<name>.header
# (XCONTENT_AGGREGATE_DATA big-endian + the package's license bits, as ContentManager::InstallContent writes it).
import importlib.util, os, shutil, struct, sys

TITLE = 0x4D5308BC
MARKETPLACE = 2
HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('stfs_extract', os.path.join(HERE, 'stfs_extract.py'))
stfs = importlib.util.module_from_spec(spec); spec.loader.exec_module(stfs)


def install(root, package):
    d = open(package, 'rb').read(0x1000)
    assert d[:4] in (b'LIVE', b'PIRS', b'CON '), f'{package}: not an STFS package'
    title, ctype = struct.unpack('>I', d[0x360:0x364])[0], struct.unpack('>I', d[0x344:0x348])[0]
    assert title == TITLE, f'{package}: title {title:08X} is not Crackdown 2'
    assert ctype == MARKETPLACE, f'{package}: content type {ctype:08X} is not DLC'
    name = os.path.basename(package)
    assert len(name) <= 42, f'{name}: keep the original package file name'
    display = d[0x411:0x411 + 256].decode('utf-16-be').split('\0')[0]
    license_mask = 0
    for i in range(16):
        _, bits, flags = struct.unpack('>QII', d[0x22C + 16 * i:0x22C + 16 * i + 16])
        if flags: license_mask |= bits

    base = os.path.join(root, '0000000000000000', f'{TITLE:08X}')
    dest = os.path.join(base, '00000002', name)
    staging = dest + '.installing'
    shutil.rmtree(staging, ignore_errors=True)
    bad = stfs.main(package, staging)
    shutil.rmtree(dest, ignore_errors=True)
    os.replace(staging, dest)

    header = bytearray(0x148)
    struct.pack_into('>II', header, 0, 1, MARKETPLACE)  # device HDD, content type
    header[8:8 + 2 * len(display[:127])] = display[:127].encode('utf-16-be')
    header[0x108:0x108 + len(name)] = name.encode('ascii')
    struct.pack_into('>I', header, 0x140, TITLE)
    header += struct.pack('<I', license_mask)  # read back raw by ContentPackage::LoadPackageLicenseMask
    hdir = os.path.join(base, 'Headers', '00000002'); os.makedirs(hdir, exist_ok=True)
    open(os.path.join(hdir, name + '.header'), 'wb').write(header)
    print(f'installed "{display}" -> {dest} (license 0x{license_mask:08X}, {bad} blocks failed SHA-1)')


if __name__ == '__main__':
    if len(sys.argv) < 3: sys.exit(__doc__ or 'usage: install_dlc.py <root> <package>...')
    for p in sys.argv[2:]: install(sys.argv[1], p)
