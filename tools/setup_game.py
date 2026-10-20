"""One-step game folder setup for the Crackdown 2 release package.

Usage: python -I setup_game.py <disc .iso or extracted disc folder> <title update package> [<DLC package> ...]

Writes, next to this script (the package folder, where crackdown2.exe finds them with no arguments):
  game/            the disc (extracted from the ISO, or copied from the folder) + default.xexp + TU/
  tu/              the title update tree (update:\\)
DLC packages are unpacked into the save folder (Documents\\crackdown2, or userdata\\ with portable.txt).
Checks every disc file against game-manifest.txt (size + SHA256) and every update/DLC block SHA-1.
"""
import hashlib, os, shutil, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import xiso_extract, stfs_extract, install_dlc  # noqa: E402

TU_XEXP_SHA256 = 'a5e49fc1ff7f6f65e6f95a7b5c4b9392bb9e38674d1237ac0e2f0149d05929d7'


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(8 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def verify_disc(game):
    manifest = os.path.join(HERE, 'game-manifest.txt')
    entries = [l.rstrip('\n').split('\t') for l in open(manifest, encoding='utf-8') if l.strip() and not l.startswith('#')]
    bad = []
    for i, (rel, size, digest) in enumerate(entries, 1):
        path = os.path.join(game, *rel.split('/'))
        print(f'\rchecking disc files {i}/{len(entries)}', end='', flush=True)
        if not os.path.isfile(path) or os.path.getsize(path) != int(size) or sha256(path) != digest:
            bad.append(rel)
    print()
    if bad:
        raise SystemExit('These disc files are missing or differ from the supported World disc:\n  ' + '\n  '.join(bad))


def documents_folder():
    # The game's save root is the shell Documents folder (it can be redirected, e.g. to OneDrive).
    try:
        import ctypes, ctypes.wintypes, uuid
        guid = uuid.UUID('{FDD39AD0-238F-46AF-ADB4-6C85480369C7}')  # FOLDERID_Documents
        buf = (ctypes.c_byte * 16).from_buffer_copy(guid.bytes_le)
        out = ctypes.c_wchar_p()
        if ctypes.windll.shell32.SHGetKnownFolderPath(buf, 0, None, ctypes.byref(out)) == 0:
            path = out.value
            ctypes.windll.ole32.CoTaskMemFree(out)
            return path
    except (AttributeError, OSError):
        pass
    return os.path.join(os.path.expanduser('~'), 'Documents')


def main(argv):
    if len(argv) < 2:
        raise SystemExit(__doc__)
    disc, update, dlcs = argv[0], argv[1], argv[2:]
    game, tu = os.path.join(HERE, 'game'), os.path.join(HERE, 'tu')
    if os.path.exists(game) or os.path.exists(tu):
        raise SystemExit(f'{game} or {tu} already exists; move or delete it first')
    staging = os.path.join(HERE, 'game.installing')
    shutil.rmtree(staging, ignore_errors=True)
    if os.path.isdir(disc):
        print(f'copying disc files from {disc}')
        shutil.copytree(disc, staging, ignore=shutil.ignore_patterns('default.xexp', 'TU', '.verified'))
    else:
        print(f'extracting {disc}')
        xiso_extract.main(disc, staging)
    verify_disc(staging)

    tu_staging = os.path.join(HERE, 'tu.installing')
    shutil.rmtree(tu_staging, ignore_errors=True)
    print(f'extracting title update {update}')
    if stfs_extract.main(update, tu_staging):
        raise SystemExit('the title update package has damaged blocks (SHA-1 mismatch); dump it again')
    xexp = os.path.join(tu_staging, 'default.xexp')
    if not os.path.isfile(xexp) or sha256(xexp) != TU_XEXP_SHA256:
        raise SystemExit('this is not the supported Crackdown 2 title update (0.0.5.1)')
    shutil.copy2(xexp, os.path.join(staging, 'default.xexp'))
    shutil.copytree(tu_staging, os.path.join(staging, 'TU'))
    os.replace(staging, game)
    os.replace(tu_staging, tu)
    print(f'game folder ready: {game}')

    if dlcs:
        portable = os.path.isfile(os.path.join(HERE, 'portable.txt'))
        root = os.path.join(HERE, 'userdata') if portable else os.path.join(documents_folder(), 'crackdown2')
        for package in dlcs:
            install_dlc.install(root, package)
    print('Done. Run crackdown2.exe.')


if __name__ == '__main__':
    main(sys.argv[1:])
