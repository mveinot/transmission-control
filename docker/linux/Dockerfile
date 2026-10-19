FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
ARG QT_VERSION=6.12.0
ARG AQTINSTALL_VERSION=3.3.0

ENV QT_PREFIX=/opt/Qt/${QT_VERSION}/gcc_64
ENV CMAKE_PREFIX_PATH=${QT_PREFIX}
ENV PATH=${QT_PREFIX}/bin:${PATH}

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        curl \
        file \
        git \
        imagemagick \
        libdbus-1-dev \
        libfontconfig1-dev \
        libfreetype-dev \
        libglib2.0-0t64 \
        libgl1-mesa-dev \
        libxkbcommon-dev \
        libxkbcommon-x11-0 \
        libxcb-cursor0 \
        libxcb-icccm4 \
        libxcb-keysyms1 \
        libxcb-shape0 \
        libxcb-xinerama0 \
        libxcb-xkb1 \
        libwayland-cursor0 \
        ninja-build \
        python3 \
        python3-venv \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m venv /opt/aqt-venv \
    && /opt/aqt-venv/bin/pip install --no-cache-dir "aqtinstall==${AQTINSTALL_VERSION}" \
    && /opt/aqt-venv/bin/aqt install-qt \
        --outputdir /opt/Qt \
        linux desktop "${QT_VERSION}" linux_gcc_64 \
        --archives icu qtbase qtdeclarative qtsvg qttools

WORKDIR /workspace

CMD ["bash"]
