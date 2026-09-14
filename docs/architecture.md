# Architecture

Haystack Netrunners is split into four implementation layers. Each layer has a
small, explicit responsibility and communicates through headers in `include/`.

```text
CLI (`src/cli`)                  HTTP server (`src/server`)
       |                                   |
       +---------------+-------------------+
                       |
               ImgFS storage API
                (`src/storage`)
                       |
              append-oriented .imgfs file

HTTP server -> HTTP/TCP helpers (`src/network`) -> client connection
```

## Storage layer

An ImgFS file contains a header, a fixed-capacity metadata table, and appended
image bytes:

```text
+----------------+----------------------+-----------------------------+
| ImgFS header   | Metadata records     | Original/resized JPEG data  |
+----------------+----------------------+-----------------------------+
```

Metadata records contain the image ID, SHA-256 digest, dimensions, validity,
and offsets and sizes for the thumbnail, small, and original representations.
Deletion invalidates a metadata record; it does not rewrite or compact the raw
data region.

The storage source files are organized by operation:

- `imgfs_create.c`, `imgfs_insert.c`, `imgfs_read.c`, `imgfs_list.c`, and
  `imgfs_delete.c` implement the public operations.
- `imgfs_tools.c` handles opening, closing, and displaying stores.
- `image_dedup.c` reuses matching content based on SHA-256.
- `image_content.c` reads dimensions and creates resized images lazily with
  libvips.

## Network and service layers

`socket_layer.c` wraps POSIX TCP primitives. `http_prot.c` parses the project's
minimal HTTP representation, while `http_net.c` owns connections and response
serialization. The service layer in `src/server/imgfs_server_service.c` maps
HTTP routes onto storage operations.

The server currently exposes these routes:

| Route | Purpose |
| --- | --- |
| `/imgfs/list` | Return image IDs as JSON |
| `/imgfs/insert?name=<id>` | Store the raw request body as an image |
| `/imgfs/read?img_id=<id>&res=<resolution>` | Return JPEG bytes |
| `/imgfs/delete?img_id=<id>` | Invalidate an image record |

This is an educational implementation rather than a production service. It
does not provide TLS, authentication, durable concurrency controls, or storage
compaction.
