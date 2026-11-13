# SPDX-License-Identifier: AGPL-3.0-only
"""Fetch Melodee's pinned typeface, Rubik (SIL OFL 1.1), into ignored build storage.

The build embeds build/deps/rubik/Rubik-VF.ttf in the binary
(src/melodee/ui/rubik_data.cpp); every weight comes from its wght axis.
"""
from pathlib import Path
import hashlib
import urllib.request

REVISION = "5bb28733cec313b63fbd415b574b5ae35c8aa8b8"  # google/fonts
BASE = f"https://raw.githubusercontent.com/google/fonts/{REVISION}/ofl/rubik/"
FILES = {
    "Rubik-VF.ttf": ("Rubik%5Bwght%5D.ttf", "1b3a7437ba2af80e465e773ed60c5036d1ba6ace492d89046dbcf18fb31e4e88"),
    "OFL.txt": ("OFL.txt", "472cbe7c25441df63e9c7864b43eb3c0f4b3df950c66a76224e6cfe1eae843fb"),
}


# INDIGO X DENIM: the typefaces beside Rubik (src/melodee/ui/fonts.h), each a variable font under the SIL OFL 1.1,
# into build/deps/fonts with their licence texts.
FONT_REVISION = "29b4ec6df2522a8aee279414b78a07910b065874"  # google/fonts
FONT_BASE = f"https://raw.githubusercontent.com/google/fonts/{FONT_REVISION}/ofl/"
FONTS = {
    "Inter.ttf": ("inter/Inter%5Bopsz,wght%5D.ttf", "29160a80ff49ddcab2c97711247e08b1fab27a484a329ce8b813d820dc559031"),
    "Inter-OFL.txt": ("inter/OFL.txt", "5b9321a4298cfeb6b34354164a1c3afc3db114569984c502b9b35d988fd58c57"),
    "SpaceGrotesk.ttf": ("spacegrotesk/SpaceGrotesk%5Bwght%5D.ttf", "acad6de1fc93436f5c0f1f4137751ef04f1aea3063e7036535970ffcfbd79f72"),
    "SpaceGrotesk-OFL.txt": ("spacegrotesk/OFL.txt", "564ce565c371c5e5bbf286006565a7c9aa55a9f56e7ca58d56e05d649dd61a72"),
    "Outfit.ttf": ("outfit/Outfit%5Bwght%5D.ttf", "fc7287273e66929776e2ba54f144fe699080bec29f61bf649d70d871468aeade"),
    "Outfit-OFL.txt": ("outfit/OFL.txt", "c676351bf8576b9aba743cd5eaa8c0e7ee0d51f805d720447b4df4ddb6a2e416"),
    "Sora.ttf": ("sora/Sora%5Bwght%5D.ttf", "84ff7096ae3ec6c8be47d906d1a0ba4de7f2ce78c615275c77301964a316e16c"),
    "Sora-OFL.txt": ("sora/OFL.txt", "ba0b9729c9428ba79a0459ab8ec575791b51509dbec213e383d0316d37fec299"),
    "Manrope.ttf": ("manrope/Manrope%5Bwght%5D.ttf", "3ae11c49db0455a3cc33e37d380f20fdb8c7f8b41dc07625c177e3d87a9d6ae6"),
    "Manrope-OFL.txt": ("manrope/OFL.txt", "58172e0c0fac2cda8a37b348164bb55e44b0e69051e557e92b1d3f6910141f7b"),
    "Lexend.ttf": ("lexend/Lexend%5Bwght%5D.ttf", "3add53e641fbc81da64da4bb254285e2831b52b029527bc0714e2b9610832ee6"),
    "Lexend-OFL.txt": ("lexend/OFL.txt", "5da8505887d0fa7fe963445fd58852707fda34adfeb65af25c99d152bab285bd"),
    "JetBrainsMono.ttf": ("jetbrainsmono/JetBrainsMono%5Bwght%5D.ttf", "48715a42ec242c21e9f02692891e147d022299a52e48d5e413e1a942193ffeda"),
    "JetBrainsMono-OFL.txt": ("jetbrainsmono/OFL.txt", "b2fe5e8987594e9ffd1d2ca52a2f5d73eb8335243893c5d6254b5ad69269591d"),
}


def fetch(folder, base, files):
    folder.mkdir(parents=True, exist_ok=True)
    for name, (remote, digest) in files.items():
        path = folder / name
        if path.exists() and hashlib.sha256(path.read_bytes()).hexdigest() == digest:
            continue
        with urllib.request.urlopen(base + remote, timeout=60) as response:
            data = response.read()
        if hashlib.sha256(data).hexdigest() != digest:
            raise RuntimeError("checksum mismatch for " + name)
        path.write_bytes(data)


def prepare(root):
    folder = Path(root) / "build/deps/rubik"
    folder.mkdir(parents=True, exist_ok=True)
    for name, (remote, digest) in FILES.items():
        path = folder / name
        if not path.exists() or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            with urllib.request.urlopen(BASE + remote, timeout=60) as response:
                data = response.read()
            if hashlib.sha256(data).hexdigest() != digest:
                raise RuntimeError("Rubik checksum mismatch for " + name)
            path.write_bytes(data)
    fetch(Path(root) / "build/deps/fonts", FONT_BASE, FONTS)
