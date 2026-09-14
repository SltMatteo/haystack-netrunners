# Haystack Netrunners

**Haystack Netrunners** is a compact image-storage server written in C. It combines a custom binary image filesystem (ImgFS), a TCP socket layer, and a minimal HTTP server to store, list, retrieve, and delete JPEG images.

This project was completed in the context of **Computer Systems (CS-202)** at **EPFL**.

The project is inspired by the systems ideas behind Meta's Haystack image-storage architecture: keep image bytes in an append-oriented data file, maintain a compact metadata index, and serve several resolutions efficiently.

> This is an educational systems-programming project, not a production-ready image service. It deliberately exposes the lower-level work that a framework or database would normally hide: request parsing, byte buffers, metadata offsets, hashing, file I/O, and image resizing.

## What it does

- Creates an **ImgFS** file: a binary store containing a header, a fixed-size metadata index, and raw image bytes.
- Inserts JPEG images with a caller-provided image ID.
- Lists stored image IDs as JSON.
- Reads the original image, a thumbnail, or a small derivative.
- Generates smaller resolutions lazily, only when they are requested.
- Deduplicates identical image content with SHA-256 while preserving separate image IDs.
- Deletes an image by invalidating its metadata record.
- Exposes the storage operations through a minimal HTTP API over TCP sockets.

## Architecture

```text
Browser / API client
        |
        | HTTP request
        v
TCP + HTTP layer
  - accepts a connection
  - reads and parses a request
  - routes the URI
  - sends an HTTP response
        |
        v
ImgFS service layer
  - list / insert / read / delete
  - validates parameters
  - maps requests to ImgFS operations
        |
        v
ImgFS binary file
  [header][metadata index][raw JPEG bytes]
```

The server and storage responsibilities are intentionally separated:

- `socket_layer.*` wraps the low-level TCP socket operations.
- `http_prot.*` parses the minimal HTTP protocol representation.
- `http_net.*` accepts connections, buffers requests, and builds responses.
- `imgfs_server_service.*` maps HTTP endpoints to ImgFS operations.
- `imgfs_*.c` files implement the filesystem operations.
- `image_content.*` handles image dimensions and lazy resizing through libvips.

See [the architecture notes](docs/architecture.md) for a source-level component
map and a description of the on-disk format.

## ImgFS storage model

An ImgFS file contains three conceptual regions:

```text
+------------------+-------------------------+--------------------------+
| ImgFS header     | Fixed-size metadata     | Appended image content   |
|                  | records                 | (originals/derivatives)  |
+------------------+-------------------------+--------------------------+
```

Each metadata record stores:

- an image ID;
- a SHA-256 hash of the original content;
- original image dimensions;
- a validity flag;
- byte offsets and sizes for thumbnail, small, and original versions.

Image content is appended to the end of the file. Metadata points to it by offset, which keeps lookup independent from the raw bytes themselves.

### Deduplication

When an image is inserted, the server calculates its SHA-256 digest and compares it with existing valid records.

- A duplicate **image ID** is rejected.
- Identical image **content** can reuse the existing content offsets and sizes.
- Different IDs can therefore refer to the same stored original bytes without writing them again.

### Lazy resizing

The original image is stored first. When a caller asks for `thumb` or `small` and that version does not yet exist, ImgFS creates the requested derivative, records its new offset and size, and serves it. Later reads reuse the generated version.

## HTTP API

The server listens on port `8000` by default. A custom port can be passed as the second argument when starting the server.

| Operation | Endpoint | Parameters | Response |
| --- | --- | --- | --- |
| List images | `/imgfs/list` | None | JSON object containing image IDs |
| Insert image | `/imgfs/insert` | `name=<image-id>` and raw image body | Redirect after successful insert |
| Read image | `/imgfs/read` | `img_id=<image-id>`, `res=orig\|thumb\|small` | `image/jpeg` bytes |
| Delete image | `/imgfs/delete` | `img_id=<image-id>` | Redirect after successful deletion |

### Example requests

Create an ImgFS file first, then start the server as described below.

```sh
# List the image IDs in the store
curl http://localhost:8000/imgfs/list

# Upload a JPEG image
curl -X POST \
  --data-binary @photo.jpg \
  "http://localhost:8000/imgfs/insert?name=lausannehorizon"

# Fetch the original image
curl -o original.jpg \
  "http://localhost:8000/imgfs/read?img_id=lausannehorizon&res=orig"

# Fetch a lazily generated thumbnail
curl -o thumbnail.jpg \
  "http://localhost:8000/imgfs/read?img_id=lausannehorizon&res=thumb"

# Delete an image record
curl -X DELETE \
  "http://localhost:8000/imgfs/delete?img_id=lausannehorizon"
```

## Command-line tool

`imgfscmd` is the command-line interface for creating and managing an ImgFS file directly.

```text
imgfscmd help
imgfscmd create <imgFS_filename> [options]
imgfscmd list <imgFS_filename>
imgfscmd insert <imgFS_filename> <imgID> <filename>
imgfscmd read <imgFS_filename> <imgID> [original|orig|thumbnail|thumb|small]
imgfscmd delete <imgFS_filename> <imgID>
```

### Create a store

```sh
# Create a store with the defaults:
# 128 image records, 64×64 thumbnails, and 256×256 small images.
./build/bin/imgfscmd create images.imgfs

# Or choose capacity and derivative resolutions explicitly.
./build/bin/imgfscmd create images.imgfs \
  -max_files 256 \
  -thumb_res 96 96 \
  -small_res 512 512
```

### Work with images locally

```sh
./build/bin/imgfscmd insert images.imgfs lausannehorizon photo.jpg
./build/bin/imgfscmd list images.imgfs
./build/bin/imgfscmd read images.imgfs lausannehorizon thumb
./build/bin/imgfscmd delete images.imgfs lausannehorizon
```

`read` writes the requested image to the current directory using an automatically generated filename such as `lausannehorizon_thumb.jpg`.

## Running the server

The server takes the ImgFS path as its first argument and an optional listening port as its second.

```sh
./build/bin/imgfs_server images.imgfs

# Use a custom port
./build/bin/imgfs_server images.imgfs 8080
```

The server handles `SIGINT` and `SIGTERM` to close the listening socket and ImgFS file cleanly.

## Building

The source uses the following native libraries:

- **libvips** for reading image dimensions and creating resized images;
- **OpenSSL** for SHA-256;
- **json-c** for JSON output from the list operation;
- POSIX sockets and pthread support.

On Debian/Ubuntu, install the development packages with:

```sh
sudo apt install build-essential pkg-config libvips-dev libssl-dev libjson-c-dev
```

On macOS with Homebrew:

```sh
brew install pkg-config vips openssl json-c
```

Then build the CLI and HTTP server:

```sh
make
```

The executables are written to `build/bin/`. The repository also includes three
small, manually operated networking programs; build them with `make test-tools`,
or compile everything with `make check`.

## Repository layout

```text
.
├── README.md
├── Makefile
├── include/                   # Shared interfaces and data structures
├── src/
│   ├── cli/                   # imgfscmd entry point and commands
│   ├── common/                # Error handling and general utilities
│   ├── network/               # TCP and HTTP protocol helpers
│   ├── server/                # HTTP server entry point and routing
│   └── storage/               # ImgFS storage operations
├── tests/                     # Standalone manual network test programs
│   └── fixtures/              # Small test inputs
└── docs/                      # Design and architecture notes
```

## Known limitations

This repository is best viewed as a systems-learning project. Important limitations include:

- No authentication, authorization, TLS, upload-size policy, or production-grade input validation.
- The server is currently synchronous; the threaded connection-handling code is present as commented work rather than active behavior.
- Deletion invalidates metadata but does not reclaim raw image bytes. A garbage-collection API is declared but is not implemented in this snapshot.
- The root route and post-operation redirects expect an `index.html`; no frontend is included in this repository snapshot.
- Error responses are currently returned as `500 Internal Server Error`, including cases that would benefit from more specific HTTP status codes.
- The deletion route is documented as a `DELETE` operation, but the current route handler does not enforce the HTTP method. Treat the API as experimental until method checks and request validation are tightened.

## Authors

Matteo Pinto — [GitHub](https://github.com/SltMatteo)

Romeo Maignal — Co-author

## License

This project is available under the permissive [MIT License](LICENSE).
