# Better On

**Gaming, without the fuss.**

Better On is a planned gaming-focused Linux operating system built on the stable KDE desktop image from [Bazzite](https://github.com/ublue-os/bazzite) and the [Universal Blue image-template](https://github.com/ublue-os/image-template) bootc workflow. It keeps the upstream atomic update and rollback model, KDE Plasma desktop, and Bazzite's gaming and hardware support as its foundation.

## Project status

This repository has the **Phase 1 image foundation in progress**, early **Phase 2 desktop assets**, and initial **Phase 3 Welcome**, **Phase 4 Hub**, and **Phase 5 Gaming** implementations. It contains the Bazzite-based image recipe, original branding, KDE defaults, and native Qt apps. CI is configured to build desktop AMD/Intel and NVIDIA images. Before building each OS image, CI resolves the selected Bazzite tag to an immutable digest and verifies it with Bazzite's published Cosign key. Manual artifacts include bootable QCOW2 disks for VM testing, the Anaconda WebUI installer ISO, and a KDE live desktop ISO built from pinned Bazzite/Titanoboa installer sources. Before artifact builds, CI verifies the exact signed Better On image digest it will consume, including the shared desktop live base and selected install payload. The live ISO overlays Better On branding and retains Steam/Lutris in its live session. None of these workflows has run in this project, and the Qt apps have not been compiled or exercised in Plasma. Published images require a matching Cosign key pair: the public key at `build_files/cosign.pub` and private key in the GitHub `SIGNING_SECRET`. CI verifies the pair, signs each published image digest, verifies the signature against the public key, and installs a GHCR-repository-scoped signature rule into the image. No VM boot or installation, hardware, update, or rollback check has passed. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for status and limitations.

## Release priorities

| Priority | Task | Why |
| --- | --- | --- |
| P0 | Upload root `Containerfile` | Essential to verify how the actual OS image is assembled |
| P0 | Get a successful CI build | Proves the image can actually be created |
| P0 | Compile all 3 Qt apps | Catches C++ errors before users encounter them |
| P1 | Build both ISOs | Confirms installers are generated |
| P1 | Test installation in a VM | Checks that users can actually install Better On |
| P1 | Test AMD, Intel, NVIDIA graphics | Confirms driver and gaming support |
| P1 | Test Steam + Proton | Confirms real game compatibility |
| P2 | Test app hub install/update/remove | Confirms software management works end-to-end |
| P2 | Test first boot | Make sure Welcome opens, works, and doesn't keep reopening |
| P2 | Test update and rollback | Protects the OS from bad updates |
| P2 | Finish documentation | Installation, troubleshooting, recovery, and release instructions |

## Build prerequisites

The upstream image-template workflow expects a Linux host running a bootc system, such as Bazzite, Fedora Atomic, Aurora, or Bluefin, with Podman and Just installed. CI uses GitHub Actions and GitHub Container Registry. Local image and ISO work requires a supported Linux build host; Windows alone is not sufficient.

### Build the container image locally

On a supported Linux bootc host with Podman:

```sh
podman build --pull=newer --tag better-on:latest --file Containerfile .
```

This builds the standard AMD/Intel bootc image only. For the proprietary NVIDIA variant, pass `--build-arg BASE_IMAGE=ghcr.io/ublue-os/bazzite-nvidia:stable`. Neither command makes an installable ISO. In GitHub Actions, the image workflow must first publish its default-branch image before the artifact workflows can use its matching commit-tagged payload. `Build Better On VM disk image` creates a bootable QCOW2 disk for QEMU/virt-manager; its artifact includes the verified source image digest and checksums for the disk and build metadata. `Build installer ISO` creates an Anaconda WebUI ISO; `Build Better On Live ISO` uses the pinned Bazzite live installer scripts and Titanoboa to create a KDE live desktop ISO. Each workflow is configured for desktop and NVIDIA variants and creates SHA-256 checksums. Before artifact construction, CI resolves the selected image tag to an immutable registry digest and verifies it with Cosign. Before publishing, generate a passwordless Cosign key pair, commit its public key as `build_files/cosign.pub`, and add its private key as the GitHub `SIGNING_SECRET`. The image build installs a scoped policy for that public key, and the live installer uses that policy when switching to the Better On registry image. These workflows have not run here, and the images, disks, ISOs, storage flow, and boot/install paths still need a Linux build and VM acceptance test.

## Design and safety

- Better On adds no custom kernel and does not replace Bazzite's update mechanism.
- The image adds no persistent services. It compiles Better On Welcome, Better On Hub, and Better On Gaming in a separate build stage using Fedora's CMake, GCC, and Qt 6 development packages; the final apps link against Qt already used by Plasma. `pciutils` is included so Gaming can show actual GPU model names. `jq` is included for the image-build step that merges the GHCR-scoped signature rule with Bazzite's existing container policy. Hub uses fixed catalog IDs and argument-vector process launches, manages apps in the current user's Flatpak scope, checks the configured Flathub URL and signature setting before install/update, and leaves user data intact when uninstalling. Gaming reads system values without changing the host. These sources need a Linux build and runtime review.
- Do not install this development image on a daily-use computer. There is no validated installer or recovery guide yet.
- The initial SVG is original project artwork. Additional third-party assets must have recorded licenses.

The current desktop assets are source files only. The Plasma style references its matching wallpaper and falls back to Breeze for unmodified desktop components. The image build seeds its style and color preference for new accounts without replacing existing skeleton choices; the behavior has a shell regression test in CI. The panel preset should appear in **Add Panel** as **Better On Bottom Panel**; adding it creates an additional panel with a branded launcher button and does not replace the existing one. Better On Welcome is installed as a KDE autostart and application-menu entry. Its five-step first-run flow introduces the desktop and gaming setup, opens KDE Look and Feel settings, Better On Gaming, Better On Hub, and Discover, and remembers a user's finish/skip choice. Hub is also available in the application menu; its catalog labels unverified community packages and known sandbox caveats. These runtime behaviors still need verification in a built image and Plasma session.

## Contributing

Read [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), then implement the next phase without claiming untested features. Record the exact build environment and test results in project documentation.





