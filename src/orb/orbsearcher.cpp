/*****************************************************************************
 * Copyright (C) 2014 Visualink
 *
 * Authors: Adrien Maglo <adrien@visualink.io>
 *
 * This file is part of Pastec.
 *
 * Pastec is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Pastec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Pastec.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#include <iostream>
#include <fstream>
#include <sys/time.h>

#include <set>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <algorithm>  // For std::partial_sort

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/features2d/features2d.hpp>

#include <orbsearcher.h>
#include <messages.h>
#include <imageloader.h>

// TODO
// Maybe when we have more images we should consider to evaluate algorithm proposed in
// https://dash.harvard.edu/server/api/core/bitstreams/030cf124-530c-4df5-a228-0fd180899d00/content
// Real-Time Tf-Idf Clustering Using Simhash, Approximate Nearest Neighbors, and DBSCAN
// It proposes near real time SIMD accelerated clustering, 
// very very similar to what we are doing here

ORBSearcher::ORBSearcher(ORBIndex *index, ORBWordIndex *wordIndex)
    : index(index), wordIndex(wordIndex), orb(ORB::create(2000, 1.02, 100)),
      threadPool(NUM_THREADS)  // Initialize thread pool once
{
    // Pre-compute word counts are already stored in the index
    
    // Initialize thread-specific word indices with deep copies of words
    for (int i = 0; i < NUM_THREADS; i++) {
        // Use the constructor that creates a deep copy of the words matrix
        threadWordIndices.push_back(std::make_unique<ORBWordIndex>(*wordIndex->getWords()));
    }
}


/**
 * @brief Process a batch of words for TF-IDF computation
 * @param batch The batch of words to process
 * @param wordCounts Vector of word counts per image
 * @param i_nbTotalIndexedImages Total number of indexed images
 * @param maxImageId Maximum image ID
 * @return Vector of weights for each image
 */
vector<float> ORBSearcher::processTFIDFBatch(
    const vector<pair<u_int32_t, const vector<Hit>*>>& batch,
    const vector<unsigned>& wordCounts,
    unsigned i_nbTotalIndexedImages,
    unsigned maxImageId)
{
    // Create a local weights vector for this batch
    vector<float> batchWeights(maxImageId + 1, 0.0f);
    
    // Process each word in the batch
    for (const auto& wordPair : batch) {
        const u_int32_t wordId = wordPair.first;
        const vector<Hit>* hits = wordPair.second;
        
        // Calculate IDF weight for this word
        const float f_weight = log((float)i_nbTotalIndexedImages / hits->size());
        
        // Update weights for all images containing this word
        for (const Hit& hit : *hits) {
            // TF-IDF calculation
            unsigned i_totalNbWords = wordCounts[hit.i_imageId];
            batchWeights[hit.i_imageId] += f_weight / i_totalNbWords;
        }
    }
    
    return batchWeights;
}


ORBSearcher::~ORBSearcher()
{
    threadPool.join();  // Ensure all tasks complete before destruction
}


// Process a batch of keypoints
std::vector<std::pair<u_int32_t, SearchHit>> ORBSearcher::processKeyPointBatch(
    const Mat& descriptors,
    const vector<KeyPoint>& keypoints,
    size_t startIdx,
    size_t endIdx,
    ORBWordIndex* localWordIndex)
{
    const unsigned i_nbTotalIndexedImages = index->getTotalNbIndexedImages();
    const unsigned i_maxNbOccurences = i_nbTotalIndexedImages > 10000 ?
                                      0.15 * i_nbTotalIndexedImages
                                      : i_nbTotalIndexedImages;

    // Create a vector to store all matches
    std::vector<std::pair<u_int32_t, SearchHit>> allMatches;
    allMatches.reserve(endIdx - startIdx); // Reserve space for efficiency

    for (unsigned i = startIdx; i < endIdx; ++i)
    {
        #define NB_NEIGHBORS 1

        vector<int> indices(NB_NEIGHBORS);
        vector<int> dists(NB_NEIGHBORS);
        
        localWordIndex->knnSearch(descriptors.row(i), indices, dists, NB_NEIGHBORS);

        for (unsigned j = 0; j < indices.size(); ++j)
        {
            const unsigned i_wordId = indices[j];
            float distance = dists[j];  // Get the KNN distance

            if (index->getWordNbOccurences(i_wordId) > i_maxNbOccurences)
                continue;
            
            // Convert the angle to a 16 bit integer.
            Hit hit;
            hit.i_imageId = 0;
            hit.i_angle = keypoints[i].angle / 360 * (1 << 16);
            hit.x = keypoints[i].pt.x;
            hit.y = keypoints[i].pt.y;
            
            // Add this match to our results without filtering
            SearchHit searchHit;
            searchHit.hit = hit;
            searchHit.distance = distance;
            allMatches.push_back({i_wordId, searchHit});
        }
    }
    
    // Return all matches without filtering
    return allMatches;
}


// Helper function for sift-down operation in min-heap
static void siftDown(std::pair<float, u_int32_t>* heap, size_t size, size_t idx) {
    size_t smallest = idx;
    size_t left = 2 * idx + 1;
    size_t right = 2 * idx + 2;
    
    if (left < size && heap[left].first < heap[smallest].first)
        smallest = left;
        
    if (right < size && heap[right].first < heap[smallest].first)
        smallest = right;
        
    if (smallest != idx) {
        std::swap(heap[idx], heap[smallest]);
        siftDown(heap, size, smallest);
    }
}


/**
 * @brief Processed a search request.
 * @param request the request to proceed.
 */
u_int32_t ORBSearcher::searchImage(SearchRequest &request)
{    
    Mat img;
    u_int32_t i_ret = ImageLoader::loadImage(request.imageData.size(),
                                             request.imageData.data(), img);
    if (i_ret != OK)
        return i_ret;
    
    vector<KeyPoint> keypoints;
    Mat descriptors;

    orb->detectAndCompute(img, noArray(), keypoints, descriptors);

    const unsigned i_nbTotalIndexedImages = index->getTotalNbIndexedImages();
    const unsigned i_maxNbOccurences = i_nbTotalIndexedImages > 10000 ?
                                       0.15 * i_nbTotalIndexedImages
                                       : i_nbTotalIndexedImages;
    
    std::unordered_map<u_int32_t, list<Hit> > imageReqHits; // key: visual word, value: the found angles
    
    // Use the persistent thread pool
    // boost::asio::thread_pool pool(NUM_THREADS);
    
    // Calculate batch size based on FEATURE_BATCH_COUNT
    size_t totalKeypoints = keypoints.size();
    size_t batchSize = (totalKeypoints + FEATURE_BATCH_COUNT - 1) / FEATURE_BATCH_COUNT; // Ceiling division
    
    // Create a vector to hold futures for each task
    std::vector<std::future<std::vector<std::pair<u_int32_t, SearchHit>>>> futures;
    
    // Submit tasks to the thread pool
    for (int b = 0; b < FEATURE_BATCH_COUNT; b++) {
        size_t startIdx = b * batchSize;
        size_t endIdx = std::min(startIdx + batchSize, totalKeypoints);
        
        // Skip empty batches
        if (startIdx >= totalKeypoints) {
            continue;
        }
                
        // Create a packaged task that returns a vector of all matches
        auto task = std::make_shared<std::packaged_task<std::vector<std::pair<u_int32_t, SearchHit>>()>>(
            [this, &descriptors, &keypoints, startIdx, endIdx, b]() {
                return this->processKeyPointBatch(descriptors, keypoints, startIdx, endIdx, threadWordIndices[b % NUM_THREADS].get());
            }
        );
        
        // Get the future from the task
        futures.push_back(task->get_future());
        
        // Submit the task to the thread pool
        boost::asio::post(threadPool, [task]() { (*task)(); });
    }
    
    // Collect all matches from all batches
    std::vector<std::pair<u_int32_t, SearchHit>> allMatches;
    
    // Wait for all tasks to complete and collect their results
    size_t batchIndex = 0;
    
    for (auto& future : futures) {
        auto batchMatches = future.get();        
        // Add to all matches
        allMatches.insert(allMatches.end(), batchMatches.begin(), batchMatches.end());
        batchIndex++;
    }
    
    // Now apply consistent filtering in a single pass
    std::unordered_map<u_int32_t, float> bestDistances;
    
    // Group by word ID and keep ALL matches (not just the best one)
    for (const auto& [wordId, searchHit] : allMatches) {
        // Add this hit to the list for this word ID
        imageReqHits[wordId].push_back(searchHit.hit);
        
        // Still track best distances for debugging
        auto distIt = bestDistances.find(wordId);
        if (distIt == bestDistances.end() || searchHit.distance < distIt->second) {
            bestDistances[wordId] = searchHit.distance;
        }
    }    
    return processSimilar(request, imageReqHits);
}


/**
 * @brief Processed a similarity request.
 * @param request the request to proceed.
 */
u_int32_t ORBSearcher::searchSimilar(SearchRequest &request)
{
    // key: visual word, value: the found angles
    std::unordered_map<u_int32_t, list<Hit> > imageReqHits;
    u_int32_t i_ret = index->getImageWords(request.imageId, imageReqHits);

    if (i_ret != OK)
        return i_ret;
    return processSimilar(request, imageReqHits);
}


u_int32_t ORBSearcher::processSimilar(SearchRequest &request,
        std::unordered_map<u_int32_t, list<Hit> > imageReqHits)
{
    const unsigned i_nbTotalIndexedImages = index->getTotalNbIndexedImages();

    std::unordered_map<u_int32_t, const vector<Hit>* > indexHits; // key: visual word id, values: index hits.
    indexHits.rehash(imageReqHits.size());
    index->getImagesWithVisualWords(imageReqHits, indexHits);

    // Count total hits across all visual words
    unsigned totalHits = 0;
    unsigned maxHitsPerWord = 0;
    unsigned wordsWithNoHits = 0;
    
    for (auto it = indexHits.begin(); it != indexHits.end(); ++it) {
        unsigned wordHits = it->second->size();
        totalHits += wordHits;
        
        if (wordHits > maxHitsPerWord)
            maxHitsPerWord = wordHits;
            
        if (wordHits == 0)
            wordsWithNoHits++;
    }
    
    // Get the maximum image ID and word counts
    const unsigned maxImageId = index->getWordCountVector().size() - 1;
    const vector<unsigned>& wordCounts = index->getWordCountVector();
    
    // Create a pre-allocated array for direct indexing of weights
    vector<float> weights(maxImageId + 1, 0.0f);
    
    // Process all visual words in parallel
    unsigned totalHitsProcessed = 0;

    // Convert the map to a vector for easier batch division
    vector<pair<u_int32_t, const vector<Hit>*>> wordPairs;
    wordPairs.reserve(indexHits.size());
    
    for (const auto& pair : indexHits) {
        wordPairs.push_back({pair.first, pair.second});
        totalHitsProcessed += pair.second->size();
    }
    
    // Calculate batch size based on WEIGHT_BATCH_COUNT
    size_t totalWords = wordPairs.size();
    size_t batchSize = (totalWords + WEIGHT_BATCH_COUNT - 1) / WEIGHT_BATCH_COUNT; // Ceiling division
    
    // Create a vector to hold futures for each task
    std::vector<std::future<vector<float>>> futures;
    
    // Submit tasks to the thread pool
    for (int b = 0; b < WEIGHT_BATCH_COUNT; b++) {
        size_t startIdx = b * batchSize;
        size_t endIdx = std::min(startIdx + batchSize, totalWords);
        
        // Skip empty batches
        if (startIdx >= totalWords) {
            continue;
        }
        
        // Create the batch
        vector<pair<u_int32_t, const vector<Hit>*>> batch(
            wordPairs.begin() + startIdx,
            wordPairs.begin() + endIdx
        );
        
        // Create a packaged task that returns a weights vector
        auto task = std::make_shared<std::packaged_task<vector<float>()>>(
            [this, batch, &wordCounts, i_nbTotalIndexedImages, maxImageId]() {
                return this->processTFIDFBatch(batch, wordCounts, i_nbTotalIndexedImages, maxImageId);
            }
        );
        
        // Get the future from the task
        futures.push_back(task->get_future());
        
        // Submit the task to the thread pool
        boost::asio::post(threadPool, [task]() { (*task)(); });
    }
    
    // Wait for all tasks to complete and merge their results
    for (auto& future : futures) {
        auto batchWeights = future.get();
        
        // Merge batch weights into the final weights vector
        for (u_int32_t id = 0; id <= maxImageId; ++id) {
            weights[id] += batchWeights[id];
        }
    }
    
    // Find top 2000 results using a bounded min-heap (keeps largest elements by replacing smallest)
    const unsigned TOP_N = 2000;
    std::pair<float, u_int32_t> topResults[TOP_N];
    size_t heapSize = 0;
    
    // Process all images in a single pass
    for (u_int32_t id = 0; id <= maxImageId; ++id) {
        if (weights[id] > 0) {
            if (heapSize < TOP_N) {
                // Heap not full yet, just add the element
                topResults[heapSize++] = {weights[id], id};
                
                // If we just filled the heap, heapify it once
                if (heapSize == TOP_N) {
                    // Build min-heap (smallest element at root)
                    for (int i = heapSize / 2 - 1; i >= 0; i--) {
                        siftDown(topResults, heapSize, i);
                    }
                }
            } 
            else if (weights[id] > topResults[0].first) {
                // Heap is full and we found a larger weight
                // Replace the smallest element (root) and sift down
                topResults[0] = {weights[id], id};
                siftDown(topResults, heapSize, 0);
            }
        }
    }
    
    // Convert heap to sorted vector (descending order by weight)
    vector<pair<float, u_int32_t>> sortedResults(topResults, topResults + heapSize);
    std::sort(sortedResults.begin(), sortedResults.end(), 
              [](const std::pair<float, u_int32_t>& a, const std::pair<float, u_int32_t>& b) { 
                  return a.first > b.first; 
              });

    // Check if forward index is available and use the optimized reranking method
    vector<SearchResult> rerankedResults;
    ORBIndex* orbIndex = static_cast<ORBIndex*>(index);
    
    if (orbIndex->hasForwardIndex()) {
        // Get the set of image IDs to rerank
        unordered_set<u_int32_t> firstImageIds;
        for (unsigned i = 0; i < min(TOP_N, (unsigned)sortedResults.size()); i++) {
            firstImageIds.insert(sortedResults[i].second);
        }
        
        // Use the forward index reranking
        rerankedResults = reranker.rerankUsingForwardIndex(imageReqHits, orbIndex, firstImageIds);
    } else {
        // Fall back to the original reranking
        unordered_set<u_int32_t> firstImageIds;
        for (unsigned i = 0; i < min(TOP_N, (unsigned)sortedResults.size()); i++) {
            firstImageIds.insert(sortedResults[i].second);
        }
        
        rerankedResults = reranker.rerank(imageReqHits, indexHits, sortedResults, TOP_N);
    }
    
    returnResults(rerankedResults, request, 100);
    return SEARCH_RESULTS;
}


/**
 * @brief Return to the client the found results.
 * @param rankedResults the ranked list of results.
 * @param req the received search request.
 * @param i_maxNbResults the maximum number of results returned.
 */
void ORBSearcher::returnResults(vector<SearchResult> &rankedResults,
                              SearchRequest &req, unsigned i_maxNbResults)
{
    list<u_int32_t> imageIds;    
    unsigned i_res = 0;
    for (const auto& res : rankedResults)
    {
        if (i_res >= i_maxNbResults)
            break;
            
        imageIds.push_back(res.i_imageId);
        i_res++;
        req.results.push_back(res.i_imageId);
        req.boundingRects.push_back(res.boundingRect);
        req.scores.push_back(res.f_weight);

        string tag;
        if (index->getTag(res.i_imageId, tag) == OK)
            req.tags.push_back(tag);
        else
            req.tags.push_back("");
    }    
}
