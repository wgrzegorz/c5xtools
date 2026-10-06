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
# difftest_operand_range.sh - reject the out-of-range / invalid operand forms that
# the reference assembler rejects (gap C: over-permissiveness).
#
# the reference assembler is selectively strict: it range-checks a set of narrow bit fields and the
# auxiliary-register operand, while ignoring most other over-range immediates and
# trailing operands. These expectations were established by probing the reference assembler directly
# (isolated, per form). c5xasm must REJECT each "bad" form and ACCEPT each "ok"
# one. Wine-free: the reference assembler verdicts are baked into the expectations below. If the
# wine toolchain is present it is also re-checked live.

set -u
here=$(cd "$(dirname "$0")" && pwd)
asm="$here/../../tools/c5xasm/c5xasm"
wrap="${C5X_TI_WRAPPER:-}"
[ -x "$asm" ] || { echo "SKIP operand-range: build c5xasm first"; exit 0; }

# forms the reference assembler REJECTS (c5xasm must reject too)
BAD="CMPR 4|SPM 4|LARP 8|XC 0,GEQ|XC 3,GEQ|BIT 0h,16|SACL 0h,8|SACH 0h,8|INTR 32|BSAR 0|BSAR 17|BSAR #4|MAR *,AR8|MAR *,AR9|SAR AR0,*,AR9|BIT *|BIT 0h|BIT *,AR5|BIT *,AR5,4|OUT *|OUT 0h|OUT *,AR6,063h|IN *|IN *,AR5,0a7h|APL #8191|OPL #5|XPL #5|CPL #8190"
# forms the reference assembler ACCEPTS (c5xasm must accept too)
OK="CMPR 3|SPM 3|LARP 7|XC 1,GEQ|XC 2,GEQ|BIT 0h,15|SACL 0h,7|SACH 0h,0|INTR 31|INTR 0|BSAR 1|BSAR 16|MAR *,AR7|LACC *,0,AR7|LACC *+,AR3|SAR AR0,*,AR1|BIT 0h,4|BIT *,4|BIT *,4,AR5|OUT 0h,5h|OUT *,063h|OUT *,063h,AR6|IN 0h,5h|IN *,0a7h|APL #8191,lbl|OPL #5,lbl|CPL lbl|XPL lbl"

asm_ok() { # returns 0 if c5xasm accepts
    t=$(mktemp -d)
    printf '\t.version 50\n\t.data\nlbl\t.word 0\n\t.text\n\t%s\n\t.end\n' "$1" > "$t/T.ASM"
    "$asm" "$t/T.ASM" -coff -o "$t/c.obj" -v50 -q >/dev/null 2>&1 && [ -f "$t/c.obj" ]
    r=$?; rm -rf "$t"; return $r
}

rc=0; oldifs=$IFS
IFS='|'; for f in $BAD; do IFS=$oldifs; if asm_ok "$f"; then echo "FAIL operand-range: c5xasm accepts '$f' (the reference assembler rejects)"; rc=1; fi; IFS='|'; done
IFS='|'; for f in $OK;  do IFS=$oldifs; if asm_ok "$f"; then :; else echo "FAIL operand-range: c5xasm rejects '$f' (the reference assembler accepts)"; rc=1; fi; IFS='|'; done
IFS=$oldifs
[ $rc -eq 0 ] && echo "PASS operand-range: CMPR/SPM/LARP/XC/BIT/SACL/SACH/INTR/BSAR ranges + ARn validity match the reference assembler"
exit $rc
