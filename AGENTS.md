# Project conventions

- Integration branch: `metachicken`; GitHub remote: `github`.
- Use Ninja targets; preserve build directories.
- Do not impose process timeouts on tools or programs.
- Use KB/MB/GB with 1024-byte units. Prefer Ruby to Python.
- Target the original Epiphan VGA2USB (`5555:1110`) first.
- Document recovered protocol facts with the source object hash, symbol/address,
  and the distinction between disassembly evidence and hardware verification.
- Never load the legacy proprietary kernel object into a modern kernel.
- Keep vendor binaries and generated firmware under ignored directories; preserve
  provenance and do not apply the project's source license to vendor firmware.
- Do not send guessed commands or write EEPROM while investigating capture.
