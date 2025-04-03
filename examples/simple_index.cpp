/**
 * Simple example of using Pastec as a library
 * 
 * This example shows how to:
 * 1. Initialize the index and searcher
 * 2. Add an image to the index
 * 3. Search for an image
 */

#include <pastec/pastec.h>
#include <iostream>
#include <vector>
#include <string>

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
    
    std::cout << "Adding image to index..." << std::endl;
    
    // Add image to index
    unsigned imageId = 1;
    unsigned nbFeatures = 0;
    pastec::ORBFeatureExtractor featureExtractor(&index, &wordIndex);
    featureExtractor.processNewImage(imageId, imageLength, (char*)imageData, nbFeatures);
    
    std::cout << "Extracted " << nbFeatures << " features" << std::endl;
    
    // Search for the same image
    std::cout << "Searching for the same image..." << std::endl;
    
    pastec::SearchRequest request;
    request.imageId = 0; // 0 means don't store the image
    request.imageData.assign(imageData, imageData + imageLength);
    
    searcher.searchImage(request);
    
    // Print results
    std::cout << "Search results:" << std::endl;
    for (size_t i = 0; i < request.results.size(); i++) {
        std::cout << "Found image ID: " << request.results[i] 
                  << " with score: " << request.scores[i] << std::endl;
    }
    
    delete[] imageData;
    return 0;
}