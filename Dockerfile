FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV PEBBLE_SDK_VERSION=4.6-rc2
ENV PEBBLE_HOME=/opt/pebble-sdk

# System dependencies
RUN apt-get update && apt-get install -y \
    curl \
    python3 \
    python3-pip \
    python3-venv \
    npm \
    libfreetype6-dev \
    libsdl1.2-dev \
    libfdt-dev \
    libpixman-1-dev \
    xvfb \
    && rm -rf /var/lib/apt/lists/*

# Install Pebble SDK (Rebble community archive)
RUN curl -L "https://rebble-sdk.s3.amazonaws.com/pebble-sdk-${PEBBLE_SDK_VERSION}-linux64.tar.bz2" \
    -o /tmp/pebble-sdk.tar.bz2 \
    && mkdir -p ${PEBBLE_HOME} \
    && tar xjf /tmp/pebble-sdk.tar.bz2 -C ${PEBBLE_HOME} --strip-components=1 \
    && rm /tmp/pebble-sdk.tar.bz2

# Set up the SDK virtualenv and install Python deps
RUN cd ${PEBBLE_HOME} && \
    python3 -m venv .env && \
    .env/bin/pip install --upgrade pip setuptools && \
    .env/bin/pip install -r requirements.txt

ENV PATH="${PEBBLE_HOME}/bin:${PATH}"

# Accept Pebble SDK license and install SDK core
RUN pebble sdk set-channel release && \
    echo "YES" | pebble sdk install latest

WORKDIR /app
COPY . /app

# Install npm dependencies (Clay, etc.)
RUN npm install

# Build for all platforms
RUN pebble build

# Screenshot script is the default command
CMD ["/app/scripts/take-screenshots.sh"]
