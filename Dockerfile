# syntax=docker/dockerfile:1.6
# ─────────────────────────────────────────────────────────────────────────────
# Fincept Terminal — multi-stage, multi-arch Docker build
#
# One Dockerfile, all hosts. Works for Docker Desktop on Windows/macOS (they
# run Linux containers) and native Linux hosts. Windows/macOS native installers
# (.exe / .dmg) are produced by .github/workflows/release.yml on platform
# runners — Docker cannot build those, by design.
#
# Architecture auto-detection
# ───────────────────────────
# BuildKit sets TARGETARCH automatically (`amd64` or `arm64`). The build picks
# the right Qt kit, Kitware CMake tarball, and apt architecture for us:
#
#   docker build -t fincept/terminal:4.0.2 .
#       → auto-detects host arch via BuildKit
#
#   docker buildx build --platform linux/amd64,linux/arm64 \
#       -t fincept/terminal:4.0.2 --push .
#       → cross-build both archs in one go
#
# Pins (must match fincept-qt/CMakeLists.txt + release.yml):
#   • Qt 6.8.3 EXACT                — aqtinstall (linux_gcc_64 / linux_gcc_arm64)
#   • GCC ≥ 12.3                    — Debian trixie g++-13
#   • CMake 3.27.7                  — Kitware prebuilt (x86_64 / aarch64)
#   • QT_MODULES = qtcharts qtwebsockets qtmultimedia
#
# Run (X11 on Linux host):
#   docker run --rm -it --net=host \
#     -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix \
#     fincept/terminal:4.0.2
# ─────────────────────────────────────────────────────────────────────────────

# ── Stage 1: Build ───────────────────────────────────────────────────────────
# `--platform=$BUILDPLATFORM` pins the builder stage to the native host arch —
# this avoids emulation overhead for toolchain work. Qt + CMake tarballs are
# picked for $TARGETARCH so the produced binary matches the requested target.
# When building for the native arch (no --platform override), BUILDPLATFORM
# and TARGETPLATFORM are equal and this is a plain native build.
FROM --platform=$BUILDPLATFORM debian:trixie-slim AS builder

# BuildKit-provided args. Valid values we care about: TARGETARCH ∈ {amd64, arm64}.
ARG TARGETARCH
ARG TARGETPLATFORM
ARG BUILDPLATFORM

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8 \
    LC_ALL=C.UTF-8

# Pins — overridable via `--build-arg` if you need to bump without editing.
ARG QT_VERSION=6.8.3
ARG CMAKE_VERSION=3.27.7

# Architecture-dependent values. Qt 6.8 introduced `linux_gcc_arm64` as the
# aqtinstall arch for Linux on ARM; the on-disk install path is still
# `gcc_arm64`. x86_64 maps to `linux_gcc_64` / `gcc_64` (the 6.8 rename).
#
# We resolve these once in a short shell block and persist them as /etc/build.env
# so later RUN steps can re-source them. Doing the resolution in-image (rather
# than as additional ARGs) keeps the `docker build` CLI simple.
RUN set -eux; \
    case "${TARGETARCH}" in \
      amd64) \
        QT_ARCH_AQT=linux_gcc_64; \
        QT_ARCH_PATH=gcc_64; \
        CMAKE_ASSET="cmake-${CMAKE_VERSION}-linux-x86_64.sh"; \
        ;; \
      arm64) \
        QT_ARCH_AQT=linux_gcc_arm64; \
        QT_ARCH_PATH=gcc_arm64; \
        CMAKE_ASSET="cmake-${CMAKE_VERSION}-linux-aarch64.sh"; \
        ;; \
      *) echo "Unsupported TARGETARCH: ${TARGETARCH}" >&2; exit 1 ;; \
    esac; \
    { \
      echo "QT_ARCH_AQT=${QT_ARCH_AQT}"; \
      echo "QT_ARCH_PATH=${QT_ARCH_PATH}"; \
      echo "CMAKE_ASSET=${CMAKE_ASSET}"; \
      echo "QT_VERSION=${QT_VERSION}"; \
      echo "CMAKE_VERSION=${CMAKE_VERSION}"; \
    } > /etc/build.env; \
    cat /etc/build.env

# Build toolchain + Qt build/link-time system deps. Mirrors the
# "Install system dependencies" step in release.yml (build-linux job).
# The -dev packages pull in their non-dev runtime counterparts; the extra
# `libglib2.0-0t64` / `libicu76` / `libdouble-conversion3` / `libpcre2-16-0`
# lines below are for Qt's own build-time tools (rcc, moc, uic) — they are
# binaries shipped inside the aqtinstall Qt kit and must be able to dlopen
# these at build time, not just link-time.
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl wget git \
        ninja-build \
        g++-13 gcc-13 \
        python3 python3-pip python3-venv \
        pkg-config file \
        libssl-dev \
        libgl1-mesa-dev libglu1-mesa-dev \
        libxkbcommon-dev libxkbcommon-x11-dev \
        libxcb1-dev libxcb-cursor-dev libxcb-icccm4-dev libxcb-image0-dev \
        libxcb-keysyms1-dev libxcb-randr0-dev libxcb-render-util0-dev \
        libxcb-shape0-dev libxcb-sync-dev libxcb-xfixes0-dev \
        libxcb-xinerama0-dev libxcb-xkb-dev libxcb-util-dev \
        libfontconfig1-dev libfreetype6-dev libdbus-1-dev \
        libglib2.0-0t64 libicu-dev libdouble-conversion3 \
        libpcre2-16-0 libpcre2-8-0 zlib1g \
        # Qt Multimedia linkage — prebuilt libQt6Multimedia.so references
        # PulseAudio + GStreamer symbols that must resolve at link time.
        libpulse-dev \
        libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
        libasound2-dev \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-13 60 \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 60 \
    && rm -rf /var/lib/apt/lists/*

# CMake pinned to 3.27.7 — Kitware ships prebuilt installers for both x86_64
# and aarch64. Debian's apt cmake drifts behind the project's EXACT pin.
RUN . /etc/build.env \
    && wget -q "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/${CMAKE_ASSET}" -O /tmp/cmake.sh \
    && chmod +x /tmp/cmake.sh \
    && /tmp/cmake.sh --skip-license --prefix=/usr/local \
    && rm /tmp/cmake.sh \
    && cmake --version

# Qt 6.8.3 via aqtinstall. Modules match QT_MODULES in release.yml. Retry
# loop rides through transient drops from the Qt mirror. --break-system-packages
# is required on trixie's PEP 668 pip.
ENV QT_ROOT=/opt/Qt
RUN . /etc/build.env \
    && pip3 install --break-system-packages --no-cache-dir aqtinstall \
    && for attempt in 1 2 3 4 5; do \
         python3 -m aqt install-qt linux desktop "${QT_VERSION}" "${QT_ARCH_AQT}" \
           --outputdir "${QT_ROOT}" \
           --modules qtcharts qtwebsockets qtmultimedia \
         && break \
         || { echo "aqtinstall attempt ${attempt} failed, retrying in 5s..."; sleep 5; }; \
       done

# ── Build Fincept Terminal ───────────────────────────────────────────────────
WORKDIR /src
COPY fincept-qt/ ./fincept-qt/

WORKDIR /src/fincept-qt
RUN . /etc/build.env \
    && export CMAKE_PREFIX_PATH="${QT_ROOT}/${QT_VERSION}/${QT_ARCH_PATH}" \
    && export PATH="${CMAKE_PREFIX_PATH}/bin:${PATH}" \
    && rm -rf build \
    && cmake -B build -G Ninja \
         -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH}" \
         -DOPENSSL_ROOT_DIR=/usr \
    && cmake --build build --parallel 4 \
    && strip build/FinceptTerminal

# ── Stage 2: Runtime ─────────────────────────────────────────────────────────
# Pinned to $TARGETPLATFORM so the final image actually matches the arch the
# user requested (even when buildx cross-builds from a different BUILDPLATFORM).
# Minimal runtime: we bundle Qt 6.8.3 from the builder stage so the image does
# not depend on Debian's (older) Qt packages. Qt pulls in EGL, GL, XCB, audio,
# Kerberos, and the glib-networking TLS backend — missing any of these causes
# HTTPS to silently fail or the app to crash on launch.
FROM --platform=$TARGETPLATFORM debian:trixie-slim AS runtime

ARG TARGETARCH
ARG QT_VERSION=6.8.3

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8 \
    LC_ALL=C.UTF-8

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates \
        python3 python3-pip python3-venv \
        # GL / EGL — Qt platform plugins need EGL even with xcb
        libegl1 libgl1 libglx-mesa0 libglu1-mesa \
        # XCB platform plugin stack
        libxcb1 libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 \
        libxcb-randr0 libxcb-render-util0 libxcb-shape0 libxcb-sync1 \
        libxcb-xfixes0 libxcb-xinerama0 libxcb-xkb1 libxcb-util1 libxcb-glx0 \
        libxkbcommon0 libxkbcommon-x11-0 \
        # Core Qt runtime dependencies
        libglib2.0-0t64 libdbus-1-3 libfontconfig1 libfreetype6 \
        libx11-6 libx11-xcb1 \
        # TLS backend for QNetworkAccessManager (without it HTTPS silently fails)
        glib-networking \
        # Network auth — Kerberos
        libgssapi-krb5-2 \
        # Qt Multimedia audio backends (optional module, but linked when present)
        libpulse0 libasound2t64 libpipewire-0.3-0t64 \
        # Wayland client libs — WSLg / modern Linux desktops. Without
        # libwayland-cursor, Qt's wayland plugin fails to dlopen.
        libwayland-client0 libwayland-cursor0 libwayland-egl1 \
        libdecor-0-0 libxkbcommon0 \
        # Used by embedded Python analytics
        libopenblas0 \
    && rm -rf /var/lib/apt/lists/*

# Resolve the on-disk Qt arch path for this target. Same mapping as builder.
RUN set -eux; \
    case "${TARGETARCH}" in \
      amd64) echo "gcc_64"   > /etc/qt_arch ;; \
      arm64) echo "gcc_arm64" > /etc/qt_arch ;; \
      *) echo "Unsupported TARGETARCH: ${TARGETARCH}" >&2; exit 1 ;; \
    esac

ENV QT_ROOT=/opt/Qt

# Bundle Qt 6.8.3 libs + plugins from builder stage. The builder writes under
# /opt/Qt/${QT_VERSION}/${arch-dependent}/ — the whole /opt/Qt tree copies
# cleanly for either arch, since only one kit is installed per build.
COPY --from=builder /opt/Qt /opt/Qt

# LD_LIBRARY_PATH / QT_PLUGIN_PATH / QT_QPA_PLATFORM_PLUGIN_PATH are resolved
# at container start by the ENTRYPOINT wrapper so they pick up the correct
# per-arch subdirectory (gcc_64 vs gcc_arm64) without needing a shell.
ENV QT_QPA_PLATFORM=xcb

WORKDIR /app
COPY --from=builder /src/fincept-qt/build/FinceptTerminal ./FinceptTerminal
COPY --from=builder /src/fincept-qt/scripts              ./scripts
COPY --from=builder /src/fincept-qt/resources            ./resources

# QGeoView is a FetchContent dependency built as a shared library next to the
# binary on Linux. Copy it into /usr/local/lib so the map widget can dlopen it.
COPY --from=builder /src/fincept-qt/build/_deps/qgeoview-build/lib/ /usr/local/lib/
RUN ldconfig \
    && chmod +x ./FinceptTerminal

# Tiny wrapper resolves QT_PREFIX at runtime (cannot expand $(cat ...) in ENV).
RUN { \
      echo '#!/bin/sh'; \
      echo 'set -e'; \
      echo 'QT_ARCH="$(cat /etc/qt_arch)"'; \
      echo 'QT_PREFIX="${QT_ROOT}/'"${QT_VERSION}"'/${QT_ARCH}"'; \
      echo 'export LD_LIBRARY_PATH="${QT_PREFIX}/lib:/usr/local/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"'; \
      echo 'export QT_PLUGIN_PATH="${QT_PREFIX}/plugins"'; \
      echo 'export QT_QPA_PLATFORM_PLUGIN_PATH="${QT_PREFIX}/plugins/platforms"'; \
      echo 'exec /app/FinceptTerminal "$@"'; \
    } > /usr/local/bin/fincept-entrypoint.sh \
    && chmod +x /usr/local/bin/fincept-entrypoint.sh

ENTRYPOINT ["/usr/local/bin/fincept-entrypoint.sh"]
