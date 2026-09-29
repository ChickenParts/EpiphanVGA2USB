# Firmware provenance

Reference package, downloaded from the manufacturer:

<https://ssl.epiphan.com/downloads/linux/Ubuntu/20.04/USB/vga2usb-3.33.0.17-ubuntu-5.4.0-91-generic-x86_64-54546-1829.deb>

Member: `usr/src/vga2usb-3.33.0.17/vga2usb_bins.o_shipped`.

SHA256: `536b9be25b310d57a77d2888fe83cd2e499417dbb963129edc90357cf9736dbb`.

The original model's descriptor is `usb_board_vga2usb` at `.rodata+0x11b240`.
Its firmware pointer is an ELF relocation at descriptor+0x28, targeting
`.rodata+0x11b2c0`. The firmware contains 65 segments and 7736 decoded bytes,
using uncompressed, repeated-byte and zlib data. Preserve segment order.

The zlib data does not contain complete stream trailers. The vendor decoder stops
when the expected output is produced; standard one-shot zlib decompression rejects
these inputs. The extraction tool must check exact decoded lengths.

Firmware upload dispatch: the original USB function table at `.rodata+0x1a2840`
has startup `.text+0x3550`, segment transfer `.text+0x35e0`, and finish
`.text+0x3530`. These issue vendor OUT request `0xa0`, CPUCS address `0xe600`,
then RAM segments in 64-byte chunks, then release CPUCS. This is disassembly
evidence; actual-device startup verification remains separate.

The firmware and proprietary object belong to Epiphan. The project's source license
does not cover them. The repository provides extraction instructions and provenance;
downloaded objects and generated binary firmware are kept in ignored directories.

## Reproduce the extracted image

From the repository root, download the identified package and extract its data
member. These commands use curl, ar and tar; no kernel module is built or loaded.

```sh
mkdir -p vendor/unpacked
curl -fL --output vendor/vga2usb.deb https://ssl.epiphan.com/downloads/linux/Ubuntu/20.04/USB/vga2usb-3.33.0.17-ubuntu-5.4.0-91-generic-x86_64-54546-1829.deb
ar p vendor/vga2usb.deb data.tar.xz | tar -xJf - -C vendor/unpacked
cp vendor/unpacked/usr/src/vga2usb-3.33.0.17/vga2usb_bins.o_shipped vendor/vga2usb_bins.o_shipped
ninja firmware-audit
```

The generated `build/firmware/original-vga2usb.v2uf` SHA256 must be
`eb61be5636ab3cc73f1042f432dd9cbc7e47f6cbc561ea3a9abd1106abcebdec`.
[The committed manifest](original-vga2usb.json) records every segment's address,
length, compression and decoded SHA256. The executable embeds bytes regenerated
from this exact pinned object and checks the same complete image hash at build time.

`V2UF` format: four magic bytes; LE16 vendor, product, firmware version and segment
count; then, in table order, LE32 address and length followed by the segment bytes.
It is a project artifact, not a vendor USB wire format.
