# Repository instructions

Before creating or modifying C/C++ code, read and follow
[CODING_STANDARD.md](CODING_STANDARD.md). That file is the single source of
truth for this repository's C/C++ coding rules; do not duplicate its rules
here.

If a relevant rule is still marked `TBD`, do not invent a repository-wide
standard. Follow the surrounding project's established conventions and call
out the unresolved decision when it affects the change.

Never open, read, search, or print the contents of
`source/app/ESP32_Wifi/include/secure_info.h`; it contains private configuration
values. Do not expose its contents in logs, summaries, or generated files.
