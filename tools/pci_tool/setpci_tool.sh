#!/usr/bin/env bash
# -----------------------------------------------------------------------------
# setpci_write.sh – Write a physical address to four PCI endpoints.
#
# Usage: sudo ./setpci_write.sh <phys_addr>
#        phys_addr should be in hex, e.g. 0x1234abcd
# -----------------------------------------------------------------------------
set -euo pipefail

if [[ "$EUID" -ne 0 ]]; then
  echo "This script must be run as root (required by setpci)." >&2
  exit 1
fi

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <physical_address_in_hex>" >&2
  exit 1
fi

PHYS_ADDR=$1

# Validate the address: must start with 0x followed by 1–16 hex digits
if [[ ! $PHYS_ADDR =~ ^0x[0-9a-fA-F]{1,16}$ ]]; then
  echo "Error: '$PHYS_ADDR' is not a valid hexadecimal address." >&2
  exit 1
fi

# Directory containing the setpci binary – adjust if different
SETPCI_BIN="./setpci/setpci"

# List of target devices
declare -a DEVICES=(
  "fe:0c.0"
  "fe:0d.0"
  "fe:0e.0"
  "fe:0f.0"
)

echo "Writing address $PHYS_ADDR to PCI endpoints: ${DEVICES[*]}"

for dev in "${DEVICES[@]}"; do
  echo "  → $dev"
  "$SETPCI_BIN" "$dev" "$PHYS_ADDR"
  # Uncomment below if you need to write to a specific BAR or offset, e.g.:
  # "$SETPCI_BIN" -s "$dev" 0x10.L=$PHYS_ADDR
  # Adjust 0x10 to the desired config-space offset.

done

echo "Done."

