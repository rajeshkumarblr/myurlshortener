## Multi-stage build for Drogon-based URL shortener (self-contained)

# ---- Builder ----
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libjsoncpp-dev \
    uuid-dev \
    libssl-dev \
    zlib1g-dev \
    libc-ares-dev \
    libbrotli-dev \
    libpq-dev \
    git \
    cmake \
    && rm -rf /var/lib/apt/lists/*

  # Build hiredis from source (latest stable)
  RUN git clone --depth 1 --branch v1.1.0 https://github.com/redis/hiredis.git /tmp/hiredis \
    && cd /tmp/hiredis \
    && make -j"$(nproc)" \
    && make install \
    && ldconfig \
    && rm -rf /tmp/hiredis

WORKDIR /src

# Use local source (copied from build context)
COPY . /src

# Ensure previous host-side build caches don't interfere
RUN rm -rf /src/build /src/drogon/build || true

# If Drogon source is not present in the repo, fetch a pinned upstream tag.
# Note: when the vendored `drogon/` directory exists in this repo, it is used as-is.
# To update Drogon, either update the vendored source or set a new tag here.
ARG DROGON_REF=v1.9.6
RUN if [ -d /src/drogon ]; then \
      echo "Using vendored Drogon source in repo (override pin ${DROGON_REF})"; \
    else \
      echo "Fetching Drogon ${DROGON_REF} from upstream"; \
      git clone --depth 1 --branch "${DROGON_REF}" https://github.com/drogonframework/drogon.git /src/drogon; \
      git -C /src/drogon submodule update --init --recursive; \
    fi

# Build and install Drogon from the vendored source
RUN cmake -S /src/drogon -B /src/drogon/build -DCMAKE_BUILD_TYPE=Release \
 && cmake --build /src/drogon/build -j"$(nproc)" \
 && cmake --install /src/drogon/build

# Build the application
RUN cmake -S /src -B /src/build -DCMAKE_BUILD_TYPE=Release \
 && cmake --build /src/build -j"$(nproc)"

# ---- Runtime ----
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies only
RUN apt-get update && apt-get install -y \
    libjsoncpp25 \
    libuuid1 \
    libssl3 \
    zlib1g \
    libc-ares2 \
    libbrotli1 \
    libpq5 \
    git \
    make \
    gcc \
    curl \
    && rm -rf /var/lib/apt/lists/*

  # Build and install hiredis from source (runtime)
  RUN git clone --depth 1 --branch v1.1.0 https://github.com/redis/hiredis.git /tmp/hiredis \
    && cd /tmp/hiredis \
    && make -j"$(nproc)" \
    && make install \
    && ldconfig \
    && rm -rf /tmp/hiredis

WORKDIR /app

# Non-root user
RUN groupadd -r appuser && useradd -r -g appuser appuser

# Copy compiled binary, config, and static assets
COPY --from=builder /src/build/url_shortener /app/
COPY --from=builder /src/config.json /app/
COPY --from=builder /src/public /app/public

# Copy Drogon/Trantor shared libraries
COPY --from=builder /usr/local/lib/libdrogon* /usr/local/lib/
COPY --from=builder /usr/local/lib/libtrantor* /usr/local/lib/

RUN ldconfig && chown -R appuser:appuser /app

USER appuser

EXPOSE 9090

# Healthcheck uses service endpoint
HEALTHCHECK --interval=15s --timeout=3s --retries=3 CMD curl -fsS http://localhost:9090/api/v1/health || exit 1

CMD ["/app/url_shortener"]
