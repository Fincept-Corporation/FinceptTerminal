# ── Stage 1: Build ────────────────────────────────────────────────────────────
FROM debian:12-slim AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake ninja-build g++ python3 python3-pip pkg-config \
    qt6-base-dev qt6-tools-dev \
    libqt6charts6-dev libqt6opengl6-dev libqt6sql6-sqlite \
    libqt6websockets6-dev libqt6concurrent6 libqt6printsupport6 \
    libgl1-mesa-dev libglu1-mesa-dev \
    libxkbcommon-dev libxkbcommon-x11-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY fincept-qt/ .

RUN rm -rf build \
    && cmake -B build -DCMAKE_BUILD_TYPE=Release -GNinja \
    && cmake --build build --parallel \
    && strip build/FinceptTerminal

# ── Stage 2: Runtime ──────────────────────────────────────────────────────────
FROM debian:12-slim AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    python3 python3-pip \
    libqt6widgets6 libqt6charts6 libqt6network6 \
    libqt6sql6 libqt6sql6-sqlite libqt6websockets6 \
    libqt6concurrent6 libqt6printsupport6 \
    libgl1-mesa-glx libglib2.0-0 libdbus-1-3 \
    libxcb1 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 \
    libxcb-randr0 libxcb-render-util0 libxcb-shape0 \
    libxcb-xinerama0 libxcb-xkb1 libxcb-cursor0 \
    libxkbcommon-x11-0 libxkbcommon0 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /src/build/FinceptTerminal ./FinceptTerminal
COPY --from=builder /src/scripts ./scripts

RUN chmod +x ./FinceptTerminal

ENV QT_QPA_PLATFORM=xcb
ENV DISPLAY=:0

ENTRYPOINT ["./FinceptTerminal"]
