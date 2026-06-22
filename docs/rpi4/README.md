# Raspberry Pi 4 Minos Bring-up Branch

## Purpose

This branch is used for Raspberry Pi 4 target-board bring-up of the H-A-SEE prototype.

The `main` branch preserves the QEMU-based Minos VM1/VM2 H-A-SEE demo.
This `rpi4-bringup` branch is used for:

- Raspberry Pi 4 Minos boot
- Raspberry Pi 4 VM0/VM1 setup
- VM0 Guardian / VM1 AI-domain mapping
- vmbox/veth communication check
- H-A-SEE replay demo on Raspberry Pi 4
- Later DETECT-CUT-ACT metric instrumentation

## Initial Target Mapping

QEMU prototype:

- VM1: Guardian Domain
- VM2: AI Domain

Raspberry Pi 4 Minos target:

- VM0: Guardian / Safety Domain
- VM1: AI Domain
- VM2: Optional Zephyr / unused in first H-A-SEE replay demo

## First Milestone

Boot Minos on Raspberry Pi 4 and confirm serial console logs.

## Not in Scope Yet

- ncnn inference
- metric trace instrumentation
- hypercall enforcement extension
- DATE-quality evaluation graphs
