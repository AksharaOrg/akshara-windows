# Code signing

The preferred release path is SignPath Foundation for qualifying open-source projects. Akshara must apply to SignPath Foundation and configure a project, artifact configurations for PE/MSI/EXE files, an approved signing policy, and the SignPath GitHub App before the release workflow can succeed. The certificate publisher will be SignPath Foundation.

Repository configuration:

- secret: `SIGNPATH_API_TOKEN`
- variables: `SIGNPATH_ORGANIZATION_ID`, `SIGNPATH_PROJECT_SLUG`, `SIGNPATH_SIGNING_POLICY_SLUG`
- artifact variables: `SIGNPATH_PE_ARTIFACT_CONFIGURATION_SLUG`, `SIGNPATH_MSI_ARTIFACT_CONFIGURATION_SLUG`, `SIGNPATH_EXE_ARTIFACT_CONFIGURATION_SLUG`

The workflow uses `signpath/github-action-submit-signing-request@v2` and GitHub-hosted runners. Signing order is x86/x64 PE payloads, MSI, then offline bundle EXE. Each stage is immutable after signing and is checked with `signtool verify /pa /all /v`, including timestamp presence.

If SignPath Foundation does not accept the project or a maintainer-owned publisher name is required, use an OV certificate backed by a CA cloud/HSM service whose chain is in Microsoft's Trusted Root Program. Do not store an exportable production PFX as a plain GitHub secret.

