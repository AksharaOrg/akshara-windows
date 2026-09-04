# Security policy

Report vulnerabilities privately through GitHub Security Advisories for `AksharaOrg/akshara-windows`. Do not include real typed content, credentials, or sensitive documents in a report.

Supported release lines receive security fixes in the latest patch release. The TSF DLL deliberately has a small dependency surface: native C++ and Windows TSF/COM only, no dynamic plug-ins, scripts, arbitrary file parsing, network client, updater, or logging. Builds enable `/sdl`, Control Flow Guard, ASLR, DEP, warnings-as-errors, CodeQL, and sanitizer coverage for the portable core.

Release PE files and installers must be Authenticode-signed and timestamped. The release workflow fails if signature validation or installer cleanup fails.

