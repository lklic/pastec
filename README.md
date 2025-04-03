# Pastec

## Introduction

### Presentation

[Pastec](http://www.pastec.io) is an open source index and search engine for image recognition based on [OpenCV](http://www.opencv.org/). It can recognize flat objects such as covers, packaged goods or artworks. It has, however, not been designed to recognize faces, 3D objects, barcodes, or QR codes.

Pastec can be, for example, used to recognize DVD covers in a mobile app or detect near duplicate images in a big database.

Pastec does not store the pixels of the images in its database. It stores a signature of each image thanks to the technique of [visual words](http://en.wikipedia.org/wiki/Visual_Word).

Pastec offers a HTTP API using JSON to add, remove, and search for images in the index.

### Intellectual property

Pastec is developed by [Visualink](http://www.visualink.io) and licenced under the [GNU LGPL v3.0](http://www.gnu.org/licenses/lgpl.html).
It is based on the free packages of [OpenCV](http://www.opencv.org/) that are available for commercial purposes; you should therefore be free to use Pastec without paying for any patent license.

More precisely, Pastec uses the [patent-free ORB descriptor](https://www.willowgarage.com/sites/default/files/orb_final.pdf) and not the well-known SIFT and SURF descriptors that are patented.

## Setup

### Using Docker

The easiest way to run Pastec is using Docker. A Dockerfile and docker-compose configuration are provided.

1. Clone the repository:
```bash
git clone https://github.com/lklic/pastec.git
cd pastec
```

2. Start with Docker Compose:
```bash
docker compose up -d
```

This will start Pastec on port 4212.

#### Optimized Docker Build

Pastec now uses an optimized Docker build process that leverages layer caching for faster rebuilds:

1. Dependencies are built in a separate layer using `deps-flake.nix`
2. The Pastec application is built in another layer using the main `flake.nix`
3. When only Pastec code changes, Docker reuses the cached dependencies layer

For more details on the build system, see [BUILD.md](BUILD.md).

To build and run with the optimized setup:

```bash
# Enable BuildKit for better caching
export DOCKER_BUILDKIT=1

# Build and run
docker-compose build
docker-compose up -d
```

Or use the provided script:

```bash
./build-and-run.sh
```

### Using Nix

Pastec now supports Nix for reproducible builds and development environments. This approach provides consistent environments across development, CI, and production.

#### Prerequisites

- Install Nix: `curl -L https://nixos.org/nix/install | sh`
- Enable flakes: Add `experimental-features = nix-command flakes` to your Nix configuration (in `~/.config/nix/nix.conf` or `/etc/nix/nix.conf`)
- This project uses the stable NixOS 24.11 channel

#### Development Environment

To enter a development shell with all dependencies:

```bash
nix develop
```

#### Building with Nix

To build Pastec using Nix:

```bash
nix build
```

This will create a `result` symlink pointing to the built package.

#### Running with Nix

After building, you can run Pastec:

```bash
./result/bin/pastec -p 4212 ./visualWordsORB.dat
```

#### Docker with Nix

The default Dockerfile now uses Nix to build Pastec, resulting in a minimal and reproducible Docker image with optimized layer caching:

```bash
# Enable BuildKit for better caching
export DOCKER_BUILDKIT=1

# Build the image
docker build -t pastec .

# Run the container
docker run -p 4212:4212 pastec
```

For backward compatibility, the Ubuntu-based Dockerfile is still available as `Dockerfile.ubuntu`.

To test the caching benefits, you can use the provided script:

```bash
./test-caching.sh
```

This script demonstrates how changes to Pastec code only rebuild the necessary layers, significantly reducing build time.

### Manual Compilation

#### Dependencies
To be compiled, Pastec requires [OpenCV 3.X](http://www.opencv.org/) and [libmicrohttpd](http://www.gnu.org/software/libmicrohttpd/) and libcurl. On **Ubuntu 18.04**, these packages can be installed using:

```bash
sudo apt-get install libopencv-dev libmicrohttpd-dev libcurl4-openssl-dev
```

#### Building
Pastec uses cmake as build system. You also need Git to get the source code:

```bash
sudo apt-get install cmake git
```

To compile Pastec:

```bash
git clone https://github.com/Visu4link/pastec.git
cd pastec
mkdir build
cd build
cmake ../
make
```

### Using Pastec as a Library

Pastec can now be used as a library in your C++ projects. This allows you to integrate Pastec's image recognition capabilities directly into your applications without using the HTTP API.

#### Building the Library

To build Pastec as a library:

```bash
git clone https://github.com/Visu4link/pastec.git
cd pastec
mkdir build
cd build
cmake ../
make
sudo make install
```

The library is always built by default. You can control other build options with these CMake flags:
- `-DBUILD_PASTEC_EXE=ON|OFF`: Build Pastec executable with HTTP server (default: ON)
- `-DBUILD_SHARED_LIBS=ON|OFF`: Build shared libraries instead of static (default: OFF)
- `-DBUILD_EXAMPLES=ON|OFF`: Build example applications (default: ON)

#### Using in CMake Projects

After installing the library, you can use it in your CMake projects:

```cmake
find_package(Pastec REQUIRED)
add_executable(myapp main.cpp)
target_link_libraries(myapp Pastec::pastec)
```

#### Basic Usage Example

```cpp
#include <pastec/pastec.h>
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <visual_words_path> <image_path>" << std::endl;
        return 1;
    }
    
    // Initialize index and searcher
    pastec::ORBIndex index;
    pastec::ORBWordIndex wordIndex(argv[1]);
    pastec::ORBSearcher searcher(&index, &wordIndex);
    pastec::ImageLoader imageLoader;
    
    // Load image
    unsigned char* imageData;
    unsigned long imageLength;
    
    if (!imageLoader.loadFile(argv[2], &imageData, &imageLength)) {
        std::cerr << "Failed to load image" << std::endl;
        return 1;
    }
    
    // Add image to index
    unsigned imageId = 1;
    unsigned nbFeatures = 0;
    pastec::ORBFeatureExtractor featureExtractor(&index, &wordIndex);
    featureExtractor.processNewImage(imageId, imageLength, (char*)imageData, nbFeatures);
    
    // Search for the same image
    pastec::SearchRequest request;
    request.imageId = 0; // 0 means don't store the image
    request.imageData.assign(imageData, imageData + imageLength);
    
    searcher.searchImage(request);
    
    // Print results
    for (size_t i = 0; i < request.results.size(); i++) {
        std::cout << "Found image ID: " << request.results[i]
                  << " with score: " << request.scores[i] << std::endl;
    }
    
    delete[] imageData;
    return 0;
}
```

For more examples, see the `examples` directory.

### Running

To start Pastec, run the **pastec** executable. It takes as mandatory argument the path to a file containing a list of ORB visual words:

```bash
./pastec visualWordsORB.dat
```

Optional arguments:
- `-p <port>`: Set the HTTP port (default: 4212)
- `-i <index_file>`: Load an existing index file
- `--https`: Enable HTTPS
- `--auth-key <key>`: Set authentication key

## API Documentation

Pastec can be controlled using a simple HTTP API. By default, it listens to port 4212.

All uploaded images must have their **dimensions** above **150 pixels**. If one of the image dimensions exceeds 1000 pixels, the image is resized so that the maximum dimension is set to 1000 pixels and the original aspect ratio is kept.

### Adding an image to the index

Add the signature of an image to make it available for searching.

* **Path:** /index/images/<image_id>
* **HTTP method:** POST
* **Data:** Image binary data (JPEG) or JSON with image URL
* **Response:**
```json
{
   "type": "IMAGE_ADDED",
   "image_id": 23,
   "nb_features_extracted": 542
}
```

Example using binary data:
```bash
curl -X POST --data-binary @/path/to/image.jpg http://localhost:4212/index/images/23
```

Example using URL:
```bash
curl -X POST -d '{"url":"http://example.com/image.jpg"}' http://localhost:4212/index/images/23
```

Example using local file URL:
```bash
curl -X POST -d '{"url":"file:///path/to/local/image.jpg"}' http://localhost:4212/index/images/23
```

> **Note:** When using the `file://` URL scheme with Docker, ensure that the directory containing your images is mounted as a volume in the container. For example: `docker run -v /path/on/host:/path/in/container pastec ...`

### Batch processing images

Process multiple images in a single request for improved performance. Optionally add tags during indexing.

* **Path:** /index/images/batch
* **HTTP method:** POST
* **Data:** JSON array of objects with image_id, url, and optional tag
* **Response:**
```json
{
   "type": "BATCH_PROCESSED",
   "results": [
      {
         "image_id": 23,
         "url": "http://example.com/image1.jpg",
         "type": "IMAGE_ADDED",
         "nb_features_extracted": 542,
         "tag": "example_tag",
         "tag_status": "IMAGE_TAG_ADDED"
      },
      {
         "image_id": 24,
         "url": "http://example.com/image2.jpg",
         "type": "IMAGE_ADDED",
         "nb_features_extracted": 328
      },
      {
         "image_id": 25,
         "url": "http://invalid-url.com/image.jpg",
         "type": "IMAGE_DOWNLOADER_HTTP_ERROR",
         "image_downloader_http_response_code": 404
      }
   ]
}
```

Example:
```bash
curl -X POST -d '[
  {"image_id": 23, "url": "http://example.com/image1.jpg", "tag": "example_tag"},
  {"image_id": 24, "url": "http://example.com/image2.jpg"},
  {"image_id": 25, "url": "file:///path/to/local/image.jpg", "tag": "local_image"}
]' http://localhost:4212/index/images/batch
```

The batch processing endpoint:
- Processes images in parallel using multiple threads for better performance
- Allows adding tags during initial indexing
- Automatically writes indices to disk after batch operations
- Returns detailed status for each image in the batch

### Removing an image from the index

* **Path:** /index/images/<image_id>
* **HTTP method:** DELETE
* **Response:**
```json
{
   "type": "IMAGE_REMOVED",
   "image_id": 23
}
```

Example:
```bash
curl -X DELETE http://localhost:4212/index/images/23
```

### Adding a tag to an image

* **Path:** /index/images/<image_id>/tag
* **HTTP method:** POST
* **Data:** Tag string
* **Response:**
```json
{
   "type": "IMAGE_TAG_ADDED"
}
```

Example:
```bash
curl -X POST --data "example_tag" http://localhost:4212/index/images/23/tag
```

### Removing a tag from an image

* **Path:** /index/images/<image_id>/tag
* **HTTP method:** DELETE
* **Response:**
```json
{
   "type": "IMAGE_TAG_REMOVED"
}
```

Example:
```bash
curl -X DELETE http://localhost:4212/index/images/23/tag
```

### Search for an image

Search for matches using an image.

* **Path:** /index/searcher
* **HTTP method:** POST
* **Data:** Image binary data (JPEG) or JSON with image URL
* **Response:**
```json
{
    "type": "SEARCH_RESULTS",
    "results": [
        {
            "image_id": 2,
            "score": 0.85,
            "tag": "example_tag",
            "bounding_rect": {
                "x": 100,
                "y": 200,
                "width": 300,
                "height": 400
            }
        },
        {
            "image_id": 5,
            "score": 0.75,
            "tag": "another_tag",
            "bounding_rect": {
                "x": 150,
                "y": 250,
                "width": 350,
                "height": 450
            }
        }
    ]
}
```

Each result object contains:
- `image_id`: ID of the matched image
- `score`: Confidence score (higher is better)
- `tag`: Associated tag (if any)
- `bounding_rect`: Match location in image

Example using binary data:
```bash
curl -X POST --data-binary @/path/to/query.jpg http://localhost:4212/index/searcher
```

Example using URL:
```bash
curl -X POST -d '{"url":"http://example.com/query.jpg"}' http://localhost:4212/index/searcher
```

Example using local file URL:
```bash
curl -X POST -d '{"url":"file:///path/to/local/query.jpg"}' http://localhost:4212/index/searcher
```

> **Note:** When using the `file://` URL scheme with Docker, ensure that the directory containing your images is mounted as a volume in the container.


### List all indexed image IDs

* **Path:** /index/imageIds
* **HTTP method:** GET
* **Response:**
```json
{
    "type": "INDEX_IMAGE_IDS",
    "image_ids": [1, 2, 3, 23, 45]
}
```

Example:
```bash
curl -X GET http://localhost:4212/index/imageIds
```

### Index Management

#### Save Index
```bash
curl -X POST -d '{"type":"WRITE", "index_path":"index.dat"}' http://localhost:4212/index/io
```

#### Load Index
```bash
curl -X POST -d '{"type":"LOAD", "index_path":"index.dat"}' http://localhost:4212/index/io
```

#### Clear Index
```bash
curl -X POST -d '{"type":"CLEAR"}' http://localhost:4212/index/io
```

#### Save Tags
```bash
curl -X POST -d '{"type":"WRITE_TAGS", "index_tags_path":"tags.dat"}' http://localhost:4212/index/io
```

#### Load Tags
```bash
curl -X POST -d '{"type":"LOAD_TAGS", "index_tags_path":"tags.dat"}' http://localhost:4212/index/io
```

### Ping Pastec

Simple health check:

```bash
curl -X POST -d '{"type":"PING"}' http://localhost:4212/
```

Response:
```json
{
    "type": "PONG"
}
```

### Error Handling

All API responses include a `type` field indicating success or error:

```json
{
    "type": "IMAGE_NOT_DECODED"
}
```

Common error types:
- `IMAGE_NOT_DECODED`: Image could not be decoded
- `IMAGE_SIZE_TOO_BIG`: Image dimensions exceed limits
- `IMAGE_SIZE_TOO_SMALL`: Image dimensions below minimum
- `IMAGE_NOT_FOUND`: Referenced image ID not found
- `IMAGE_TAG_NOT_FOUND`: No tag found for image
- `AUTHENTIFICATION_ERROR`: Invalid authentication key
- `IMAGE_DOWNLOADER_HTTP_ERROR`: Error downloading image from URL

## Python Client

A Python client library is provided in the `python` directory. Example usage:

```python
from PastecLib import PastecConnection

pastec = PastecConnection("localhost", 4212)

# Add image from file
pastec.indexImageFile(1, "image.jpg")

# Add image from URL
# This requires handling yourself, the Python lib doesn't have direct URL support

# Add tag
pastec.addTag(1, "example_tag")

# Search with image file
results = pastec.imageQueryFile("query.jpg")
for image_id, tag in results:
    print(f"Match: Image ID {image_id}, Tag: {tag}")

# Save and load index
pastec.writeIndex("index.dat")
pastec.loadIndex("index.dat")

# Save and load tags
pastec.writeIndexTags("tags.dat")
pastec.loadIndexTags("tags.dat")

# Clear index
pastec.clearIndex()
```
