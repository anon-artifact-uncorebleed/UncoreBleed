# UncoreBleed
## 1. Overview

This repository contains the artifact accompanying the paper:

**UncoreBleed: AEX-free, High-Resolution, and Low-Noise Side-Channel Attacks on SGX Enclaved Execution**

The artifact provides:

- Source code of our microbenchmarks, reverse engineering tools, and attack implementations.
- Scripts to configure uncore PMON boxes and program the **PKT_MATCH** filter registers.
- Case studies:
  - **Libjpeg decoding leakage** (recovering image contours).
  - **RSA key extraction** against TLBlur-protected enclaves.

------

## 2. Artifact Contents

```
├── /microbenchmarks/     # Core vs. Uncore PMC validation (enclave vs. non-enclave)
├── /tools/               # Tools for PMON configuration, PKT_MATCH setup, etc
├── /reverse_engineer/    # Scripts for PKT_MATCH reverse engineering and M2M mapping
├── /attacks/libjpeg/     # Libjpeg case study (image recovery)
├── /attacks/rsa/         # RSA case study (key extraction under TLBlur)
└── README.md             # This file
```

------

## 3. Environment Setup

### Hardware

- Intel Xeon Scalable CPU with **SGX support** (Ice Lake-SP or newer).
- SGX must be enabled in BIOS and running in **production mode**.
- Tested on:
  - Xeon Silver 4314 (Ice Lake-SP)
  - Xeon Bronze 3408U (Sapphire Rapids-SP)
  - Xeon Silver 4514Y (Emerald Rapids-SP)

### Software

- Linux kernel **5.4+** with SGX enabled (≥5.15 recommended).
- Intel SGX SDK **2.17+** installed and configured.
- GCC ≥ 9.0, CMake ≥ 3.16.
- Root privileges (required for PCIe config, CR0.CD modification, and module loading).

### Recommended Configuration

- Disable **HyperThreading**.
- Shut down background daemons (irqbalance, watchdogs, etc.).

------

## 4. Preparation

Before running experiments, check the environment:

```
./check_env
```

This script checks:

- SGX availability and version (SGX1/2).
- Whether enclaves can be launched in production mode.
- Kernel support for perf events.
- Presence of required tools (perf, msr-tools, pciutils).

**Example output:**

```
[+] SGX available: Yes (SGX1 + SGX2)
[+] SGX production mode: Supported
[+] perf installed: Yes
[+] Kernel version: 6.2.0 (OK)
[+] PCIe access: /dev/mem available
```

------

## 5. Experiments

### 5.1 Core vs. Uncore Microbenchmarks

- Location: `/microbenchmarks/`
- Two sets of programs:
  - `Non-Enclave/` — run outside enclaves.
  - `Enclave/` — run inside SGX enclaves.

Run:

```
./validate_uncore.sh
```

This will:

1. Stress core PMCs (e.g., `INSTRUCTIONS`, `BR_INST_RETIRED.ALL_BRANCHES`).
2. Stress uncore PMCs (e.g., `TOR_INSERT`, `TAG_HIT`, `CAS_COUNT`).
3. Collect counts inside and outside enclaves.
4. Automatically plot a comparison figure: `CoreUncoreCompare.png`.

**Expected:** Core PMCs are suppressed inside enclaves, while uncore PMCs remain active.

------

### 5.2 Reverse Engineering PKT_MATCH

- Location: `/reverse_engineer/`

Steps:

1. Identify ADDRMATCH / ADDRMASK registers (`uncover_PKT_MATCH/`).
2. Configure filters for test addresses (`uncover_PKT_MATCH/`).
3. Reverse engineer address-to-M2M mapping (`uncover_M2M_map/`).

**Expected:**

- Confirm **64B granularity**.
- Recover XOR-based mapping (Figure 6 in the paper).
- Verify event code transition (0x4C → 0x36 on Sapphire/Emerald Rapids).

------

### 5.3 Libjpeg Attack

- Location: `/attacks/libjpeg/`
- Run:

```
./run_libjpeg_attack.sh input.jpg
```

The script:

- Launches enclave-based JPEG decoding.
- Configures PKT_MATCH to monitor the critical loop.
- Collects traces and processes them via Python scripts.

**Output:** Reconstructed image contours (`results/libjpeg_output.png`).

------

### 5.4 RSA Key Extraction under TLBlur

- Location: `/attacks/rsa/`
- Requirement: Install and compile TLBlur (follow its instructions).

Run:

```
./run_rsa_attack.sh
```

The script configures two PKT_MATCH events to monitor `square()` and `multiply()`.

**Output:** Sequence of RSA private key bits (`results/rsa_recovered_bits.txt`).

------

## 6. Additional Tools

- `/uncore_tools/pci_tool` — configure M2M PCICFG registers.
- `/uncore_tools/ptw` — helper for virtual-to-physical translation.
- `/uncore_tools/cpu_set` — lock CPU frequency to minimum.
- `/kernel_module/uncache_module` — set CR0.CD=1 to enforce uncached mode.
- `/kernel_module/perf_tool` — high-frequency polling sampler (~650 ns).

------

## 7. Notes and Limitations

- Best results require **isolated cores** and minimal interrupts.
- Sampling precision depends on hardware (we achieved ~650 ns).
- Multi-socket behavior: different sockets share the same M2M units but have independent counters; interference may occur.
- Some experiments may not reproduce identically on unsupported platforms.

------

## 8. Ethical Considerations

This artifact is provided **for academic evaluation only**.

- Do not use outside controlled research environments.
- We responsibly disclosed the findings to Intel.
