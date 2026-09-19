# Security and safety reporting

## Supported version

The current `main` branch is the only version receiving documentation and safety-related updates. Historical binaries, archived experiments and unsupported voltage targets must not be flashed.

## Report a vulnerability privately

For software vulnerabilities, unsafe default behavior, protection bypasses or issues that could unexpectedly energize the power stage, use the repository's [private vulnerability reporting](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02/security/advisories/new) when available. Do not publish exploit details or instructions that could cause hardware damage before the issue is reviewed.

Include:

- affected commit and board revision;
- exact reproduction conditions;
- whether a motor or power supply was connected;
- expected and observed gate/current behavior;
- relevant logs or oscilloscope captures with sensitive data removed;
- the safest known mitigation or rollback.

## Hardware-safety boundary

This project has no independent hardware over-current comparator, gate-driver fault feedback, MOSFET temperature sensing or physical VSUPPLY ADC in the documented revision. Software controls reduce risk but do not make the board safety-rated. Always use current limiting, a fuse, an emergency power disconnect and appropriate isolated measurement equipment.
