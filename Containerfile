# Keep build inputs out of the final image until the customization step needs them.
ARG BASE_IMAGE=ghcr.io/ublue-os/bazzite:stable
ARG IMAGE_REPOSITORY=

FROM scratch AS ctx
COPY build_files /
COPY system_files /system_files

# Build the native first-login app against the same Qt runtime as the target image.
FROM ${BASE_IMAGE} AS welcome-builder
RUN --mount=type=cache,dst=/var/cache \
    dnf5 install -y cmake gcc-c++ qt6-qtbase-devel && \
    dnf5 clean all
COPY apps/better-on-welcome /src/better-on-welcome
COPY apps/better-on-hub /src/better-on-hub
COPY apps/better-on-gaming /src/better-on-gaming
RUN cmake -S /src/better-on-welcome -B /build/better-on-welcome \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr && \
    cmake --build /build/better-on-welcome --parallel && \
    cmake --install /build/better-on-welcome && \
    cmake -S /src/better-on-hub -B /build/better-on-hub \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr && \
    cmake --build /build/better-on-hub --parallel && \
    cmake --install /build/better-on-hub && \
    cmake -S /src/better-on-gaming -B /build/better-on-gaming \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr && \
    cmake --build /build/better-on-gaming --parallel && \
    cmake --install /build/better-on-gaming

# Better On starts from the stable KDE desktop image maintained by Universal Blue.
# Keep upstream gaming support, updates, rollback, and hardware integration intact.
FROM ${BASE_IMAGE}
ARG IMAGE_REPOSITORY

ARG IMAGE_SOURCE=""
LABEL org.opencontainers.image.source="${IMAGE_SOURCE}"

COPY --from=welcome-builder /usr/bin/better-on-welcome /usr/bin/better-on-welcome
COPY --from=welcome-builder /usr/bin/better-on-hub /usr/bin/better-on-hub
COPY --from=welcome-builder /usr/bin/better-on-gaming /usr/bin/better-on-gaming

# pciutils lets Better On Gaming show hardware model names when available.
# jq is required to merge Better On's signature rule into the base policy.
RUN dnf5 install -y pciutils jq && dnf5 clean all

RUN --mount=type=bind,from=ctx,source=/,target=/ctx \
    --mount=type=cache,dst=/var/cache \
    --mount=type=tmpfs,dst=/tmp \
    env IMAGE_REPOSITORY="$IMAGE_REPOSITORY" /usr/bin/bash /ctx/build.sh

# Catch common bootc image configuration errors during every build.
RUN bootc container lint
