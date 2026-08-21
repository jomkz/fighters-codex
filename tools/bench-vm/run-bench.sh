#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# run-bench.sh — host-side entry point for the FA bench VM (#56).
#
#   FX_FA_SRC="/run/media/john/Windows Disk/JANES/Fighters Anthology" tools/bench-vm/run-bench.sh up
#   tools/bench-vm/run-bench.sh console     # open the game's screen (virt-viewer over SPICE)
#   tools/bench-vm/run-bench.sh snapshot pre-mission   # QEMU snapshot for a clean save-diff base
#   tools/bench-vm/run-bench.sh restore  pre-mission
#   tools/bench-vm/run-bench.sh fetch-log   # pull C:\bench\watch.log back to the host
#   tools/bench-vm/run-bench.sh halt | destroy | status
#
# This is an interactive bench: the actual flying is done by a human at the console. The script
# just manages the VM and the observation plumbing around it.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$HERE"

need() { command -v "$1" >/dev/null 2>&1 || { echo "missing: $1 ($2)" >&2; exit 1; }; }
domain() { virsh list --all --name 2>/dev/null | grep -m1 'bench' || echo "bench-vm_default"; }

cmd="${1:-help}"; shift || true
case "$cmd" in
  up)
    need vagrant "the VM manager"
    if [[ -z "${FX_FA_SRC:-}" ]]; then
      echo "note: FX_FA_SRC is unset — the guest will come up WITHOUT an FA install."
      echo "      set it to your licensed install dir and re-run, or 'vagrant provision' later."
    elif [[ ! -f "${FX_FA_SRC}/FA.EXE" ]]; then
      echo "warning: no FA.EXE under FX_FA_SRC='${FX_FA_SRC}' — check the path." >&2
    fi
    echo "=== bringing the bench VM up (first run downloads the Windows box + provisions) ==="
    exec vagrant up --provider=libvirt
    ;;
  console)
    need virt-viewer "the SPICE console"
    # --attach connects to the display through libvirt directly. vagrant-libvirt gives the guest a
    # SPICE display with <listen type='none'> and won't reliably set a reachable listen address
    # (libvirt drops it), so --attach is required. It carries all SPICE channels including audio.
    # GDK_BACKEND=x11 runs virt-viewer under XWayland. On a fractional-scaled Wayland desktop (e.g.
    # a 4K monitor at 125%), spice-gtk's native-Wayland cursor path mis-scales the guest cursor
    # ("cursor image size ... not an integer multiple of scale" -> a giant pointer); XWayland lets
    # the compositor scale the whole window uniformly and the cursor renders at normal size.
    GDK_BACKEND=x11 virt-viewer --attach --connect qemu:///system "$(domain)" &
    echo "opened console for $(domain) (pid $!)"
    ;;
  snapshot)
    need virsh "libvirt"
    [[ $# -ge 1 ]] || { echo "usage: run-bench.sh snapshot <name>" >&2; exit 1; }
    virsh -c qemu:///system snapshot-create-as "$(domain)" "$1" --atomic
    echo "snapshot '$1' taken."
    ;;
  restore)
    need virsh "libvirt"
    [[ $# -ge 1 ]] || { echo "usage: run-bench.sh restore <name>" >&2; exit 1; }
    virsh -c qemu:///system snapshot-revert "$(domain)" "$1"
    echo "reverted to snapshot '$1'."
    ;;
  fetch-log)
    need vagrant "the VM manager"
    # winrm file pull via vagrant; falls back to a documented manual path.
    vagrant winrm -c 'Get-Content C:\bench\watch.log' 2>/dev/null > watch.log \
      && echo "wrote $(pwd)/watch.log" \
      || echo "could not pull the log over WinRM — copy C:\\bench\\watch.log from the console."
    ;;
  halt)    exec vagrant halt ;;
  destroy) exec vagrant destroy -f ;;
  status)  exec vagrant status ;;
  *)
    sed -n '3,20p' "$0" | sed 's/^# \{0,1\}//'
    ;;
esac
