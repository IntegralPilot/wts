"""Check the ELF files and embed them in the runner."""

import json
from pathlib import Path
import struct
import sys


names = sys.argv[1:]
tests = [name for name in names if name != "launch" and not name.endswith(".so")]

asm = ['.section .rodata.wts_payload,"a",@progbits']
c = [
    "struct payload {",
    "    const unsigned char *start;",
    "    const unsigned char *end;",
    "};",
    "",
]

for index, name in enumerate(names):
    path = Path("build", name).resolve()
    data = path.read_bytes()

    shared = name.endswith(".so")
    executable = name.startswith("dynamic-")
    dynamic = shared or executable
    expected_kind = 3 if dynamic or name == "elf-pie" else 2
    page = 65536 if name == "launch" else 4096

    kind, machine = struct.unpack_from("<HH", data, 16)
    phoff = struct.unpack_from("<Q", data, 32)[0]
    size, count = struct.unpack_from("<HH", data, 54)

    if data[:6] != b"\x7fELF\x02\x01" or machine != 183 or kind != expected_kind:
        raise SystemExit(f"{name}: unexpected ELF architecture or type")

    loads = 0
    interpreter = False
    dependencies = False

    for i in range(count):
        header = struct.unpack_from("<IIQQQQQQ", data, phoff + i * size)
        segment, flags, offset, address, _, filesz, memsz, alignment = header

        if segment == 1:  # PT_LOAD
            loads += 1

            if (
                alignment != page
                or (address - offset) % page
                or flags & 3 == 3
                or memsz < filesz
                or offset + filesz > len(data)
            ):
                raise SystemExit(f"{name}: invalid {page}-byte LOAD segment")

        elif segment == 2:  # PT_DYNAMIC
            for at in range(offset, offset + filesz, 16):
                tag, value = struct.unpack_from("<qQ", data, at)

                if tag == 0:
                    break

                if tag == 1:
                    dependencies = True

                if shared and (
                    tag == 24
                    or (tag == 30 and value & 8)
                    or (tag == 0x6FFFFFFB and value & 1)
                ):
                    raise SystemExit(f"{name}: BIND_NOW would bypass the lazy-binding test")

        elif segment == 3:  # PT_INTERP
            interpreter = True

        elif segment == 0x6474E551 and flags & 1:  # PT_GNU_STACK
            raise SystemExit(f"{name}: executable stack")

    if loads < 2 or interpreter != executable or dependencies != dynamic:
        raise SystemExit(f"{name}: unexpected static/shared linking")

    asm += [
        ".balign 16",
        f".global blob_{index}_start, blob_{index}_end",
        f"blob_{index}_start:",
        ".incbin " + json.dumps(str(path)),
        f"blob_{index}_end:",
        "",
    ]

    c += [
        f"extern const unsigned char blob_{index}_start[];",
        f"extern const unsigned char blob_{index}_end[];",
        "",
    ]

asm += ['.section .note.GNU-stack,"",@progbits']

c += [
    f"const unsigned int wts_payload_count = {len(names)};",
    "",
    "const struct payload wts_payloads[] = {",
]

for index in range(len(names)):
    c += [f"    {{blob_{index}_start, blob_{index}_end}},"]

c += ["};"]

haskell = ["module Payload (payloads, tests) where", ""]

for label, items in [("payloads", names), ("tests", tests)]:
    haskell += [
        f"{label} :: [String]",
        f"{label} =",
        "  [ " + "\n  , ".join(json.dumps(item) for item in items),
        "  ]",
        "",
    ]

Path("build/payload.S").write_text("\n".join(asm) + "\n")
Path("build/payload.c").write_text("\n".join(c) + "\n")
Path("build/Payload.hs").write_text("\n".join(haskell) + "\n")
