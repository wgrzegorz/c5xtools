#!/bin/sh
#         ___|       |               |
#     __| __ \ \  / __|  _ \   _ \  |  __|
#    (      ) |`  <  |   (   | (   | |\__ \ 
#   \___|____/ _/\_\\__|\___/ \___/ _|____/
# ======================================dSc==
#   Brought to you by + Cult of the Modem +
#
# TMS320C5x assembler/linker/disassembler/hex
#
# SPDX-License-Identifier: MIT
# Author:  Grzegorz Worona <grzegorz@worona.pl>
# Version: 1.0.1
#
# out2img.sh - full 16-bit program image from a linked COFF .out (the reference linker or
# c5xlnk). Emits the loadable section words (little-endian) in address order,
# keeping BOTH bytes of every word so a relocation or short/long-form difference
# is visible. The .out timestamp lives only in the file header and is not read.
#
# Pure POSIX shell + od + awk (no python). Usage: out2img.sh in.out out.img

set -eu
in=$1; out=$2
# awk parses the COFF and prints the bytes as octal escape text (\NNN); the shell
# printf then renders them to real bytes. awk is used only for arithmetic/text,
# never for binary output, so this is identical on mawk, gawk and BSD awk.
esc=$(od -A n -v -t u1 "$in" | awk '
{ for (i = 1; i <= NF; i++) b[n++] = $i }        # flatten all bytes into b[0..n-1]
function u16(o) { return b[o] + b[o+1]*256 }
function u32(o) { return b[o] + b[o+1]*256 + b[o+2]*65536 + b[o+3]*16777216 }
function bit7(x) { return int(x/128) % 2 }         # test flag bit 0x80 (.bss)
END {
    nscn = u16(2); opt = u16(16); base = 22 + opt; SCN = 48
    m = 0
    for (i = 0; i < nscn; i++) {
        o = base + SCN*i
        paddr = u32(o+8); size = u32(o+16); scnptr = u32(o+20); flags = u32(o+40)
        if (size > 0 && scnptr > 0 && bit7(flags) == 0) {
            pa[m] = paddr; sp[m] = scnptr; sz[m] = size; m++
        }
    }
    # address-order sort (few sections: simple insertion sort)
    for (i = 1; i < m; i++) {
        kp = pa[i]; ks = sp[i]; kz = sz[i]; j = i - 1
        while (j >= 0 && pa[j] > kp) { pa[j+1]=pa[j]; sp[j+1]=sp[j]; sz[j+1]=sz[j]; j-- }
        pa[j+1]=kp; sp[j+1]=ks; sz[j+1]=kz
    }
    for (i = 0; i < m; i++)
        for (w = 0; w < sz[i]; w++)
            printf "\\%03o\\%03o", b[sp[i] + 2*w], b[sp[i] + 2*w + 1]
}')
printf "$esc" > "$out"
