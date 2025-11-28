# URL Shortener Service

A high-performance URL shortener built with C++ and the Drogon web framework.

## Features

- **Fast URL Shortening**: Generate short codes using optimized Base62 encoding
- **TTL Support**: URLs can expire automatically after a specified time
- **High Concurrency**: Sharded mutex design for optimal multi-threaded performance
- **RESTful API**: Clean HTTP endpoints for all operations
- **Simple Web UI**: Built-in HTML page for manual shortening
- **Collision Handling**: Automatic retry mechanism for hash collisions

## API Endpoints

### Create Short URL
```bash
POST /api/shorten
Content-Type: application/json

{
  "url": "https://example.com",
  "ttl": 3600  // optional, seconds
}
```

### Get URL Info
```bash
GET /api/info/{code}
```

### Redirect to Original URL
```bash
GET /{code}
```

### Health Check
```bash
GET /api/health
```

## Quick Start

### Prerequisites
- C++17 compatible compiler
- CMake 3.10+
- Drogon web framework

### Build and Run
```bash
# Build the project
make build

# Start the server
make run

# Run tests
make test

# Stop the server
make stop
```

The service will be available at `http://localhost:9090`

Open `http://localhost:9090/` in a browser to use the built-in UI form for shortening links manually.

## Architecture

### Components

- **UrlShortenerService**: Core business logic with thread-safe operations
- **Base62 Encoder**: Optimized encoding for generating short codes
- **Sharded Mutex**: 16-way mutex sharding for improved concurrency

### Performance Characteristics

- **Encoding Speed**: ~10ns per Base62 operation
- **Concurrency**: 16x better than global mutex through sharding
- **Memory Efficient**: Simple in-memory storage with TTL cleanup

### File Structure

```
src/
├── main.cpp                    # Application entry point
├── UrlShortenerService.h       # Service interface
├── UrlShortenerService.cpp     # Service implementation
├── utils.h                     # Base62 encoding utilities
└── utils.cpp                   # Utility implementations
```

## Configuration

Edit `config.json` to modify server settings:

```json
{
  "listeners": [
    {
      "address": "0.0.0.0",
      "port": 9090,
      "https": false
    }
  ],
  "threads_num": 4,
  "log": {
    "log_path": "./",
    "logfile_base_name": "server",
    "log_size_limit": 100000000
  }
}
```

## Development

### Building
The project uses CMake for compilation and includes VS Code configuration for development.

### Testing
Comprehensive API tests are included covering:
- URL shortening and retrieval
- TTL functionality
- Error handling
- Edge cases

### Performance Notes
- Base62 encoding benchmarked against multiple algorithms
- Sharded mutex provides optimal concurrency without complexity
- Simple division-based encoding outperforms complex lookup tables

## Production Deployment

The service is designed for production use with:
- Proper error handling and HTTP status codes
- Concurrent request handling
- Resource cleanup and TTL management
- Daemon mode operation

## Kubernetes (Kind) Quickstart

See `k8s/README.md` for full details. The short version:

```bash
# Build and deploy to Kind (creates DB secret automatically)
bash k8s/deploy-kind.sh

# If NodePort isn't reachable, port-forward the service
kubectl port-forward svc/url-shortener 9090:9090

# Health and shorten
curl -s http://localhost:9090/api/v1/health
curl -s -X POST -H 'Content-Type: application/json' \
  -d '{"url":"https://example.com","ttl":120}' \
  http://localhost:9090/api/v1/shorten
```

Notes:
- The app uses an external Postgres via `DATABASE_URL` set in secret `external-postgres`.
- Liveness probe is TCP; readiness checks `/api/v1/health` and includes a DB ping.
- For cloud DBs, prefer `sslmode=require`.

## License

This project is licensed under the MIT License - see the LICENSE file for details.
