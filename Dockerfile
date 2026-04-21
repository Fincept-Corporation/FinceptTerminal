# ── Stage 1: Build ────────────────────────────────────────────────────────────
# Debian 12 base: ships a recent glibc (2.36) and g++-12 meeting our GCC 12.3+ pin.
# Qt is NOT installed from apt (Debian ships 6.4.x); we fetch Qt 6.7.2 via
# aqtinstall to match the version pin in fincept-qt/CMakeLists.txt.
FROM debian:12-slim AS builder

ENV DEBIAN_FRONTEND=noninteractive
ARG QT_VERSION=6.8.3
ARG QT_ARCH=gcc_64

# Build toolchain + Qt runtime system deps (for Qt to link against)
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl git \
        cmake ninja-build g++-12 \
        python3 python3-pip python3-venv \
        pkg-config \
        libgl1-mesa-dev libglu1-mesa-dev \
        libxkbcommon-dev libxkbcommon-x11-dev \
        libxcb1-dev libxcb-cursor-dev libxcb-icccm4-dev libxcb-image0-dev \
        libxcb-keysyms1-dev libxcb-randr0-dev libxcb-render-util0-dev \
        libxcb-shape0-dev libxcb-sync-dev libxcb-xfixes0-dev \
        libxcb-xinerama0-dev libxcb-xkb-dev libxcb-util-dev \
        libfontconfig1-dev libdbus-1-dev libssl-dev \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 60 \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 60 \
    && rm -rf /var/lib/apt/lists/*

# ── Install pinned Qt ${QT_VERSION} via aqtinstall ────────────────────────────
# aqtinstall pulls exact Qt binaries from the official Qt mirror. Strict pin —
# matches find_package(Qt6 6.7.2 EXACT ...) in CMakeLists.txt.
ENV QT_ROOT=/opt/Qt
RUN pip3 install --break-system-packages --no-cache-dir aqtinstall \
    && python3 -m aqt install-qt linux desktop ${QT_VERSION} ${QT_ARCH} \
        --outputdir ${QT_ROOT} \
        --modules qtcharts qtwebsockets qtmultimedia qtmultimediawidgets qtspeech

ENV CMAKE_PREFIX_PATH="${QT_ROOT}/${QT_VERSION}/${QT_ARCH}"
ENV PATH="${CMAKE_PREFIX_PATH}/bin:${PATH}"

# ── Build Fincept Terminal ────────────────────────────────────────────────────
WORKDIR /src
COPY fincept-qt/ .

RUN rm -rf build \
    && cmake --preset linux-release -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH}" \
    && cmake --build --preset linux-release \
    && strip build/linux-release/FinceptTerminal


# ── Stage 2: Runtime ──────────────────────────────────────────────────────────
# Minimal runtime: bundles Qt 6.7.2 libs copied from the builder stage so the
# runtime image doesn't depend on Debian's (older) Qt packages.
FROM debian:12-slim AS runtime

ENV DEBIAN_FRONTEND=noninteractive
ARG QT_VERSION=6.8.3
ARG QT_ARCH=gcc_64

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates \
        python3 python3-pip \
        libgl1 libglu1-mesa libglib2.0-0 libdbus-1-3 libfontconfig1 \
        libxcb1 libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 \
        libxcb-randr0 libxcb-render-util0 libxcb-shape0 libxcb-sync1 \
        libxcb-xfixes0 libxcb-xinerama0 libxcb-xkb1 libxcb-util1 \
        libxkbcommon0 libxkbcommon-x11-0 \
    && rm -rf /var/lib/apt/lists/*

ENV QT_ROOT=/opt/Qt
ENV QT_PREFIX="${QT_ROOT}/${QT_VERSION}/${QT_ARCH}"

# Bundle Qt 6.7.2 libs + plugins from builder stage
COPY --from=builder ${QT_PREFIX}/lib       ${QT_PREFIX}/lib
COPY --from=builder ${QT_PREFIX}/plugins   ${QT_PREFIX}/plugins

ENV LD_LIBRARY_PATH="${QT_PREFIX}/lib:${LD_LIBRARY_PATH}"
ENV QT_PLUGIN_PATH="${QT_PREFIX}/plugins"
ENV QT_QPA_PLATFORM_PLUGIN_PATH="${QT_PREFIX}/plugins/platforms"
ENV QT_QPA_PLATFORM=xcb

WORKDIR /app
COPY --from=builder /src/build/linux-release/FinceptTerminal ./FinceptTerminal
COPY --from=builder /src/scripts ./scripts

RUN chmod +x ./FinceptTerminal

ENTRYPOINT ["./FinceptTerminal"]
