/*****************************************************************************
 * Copyright (C) 2025 Pharos
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
#include <algorithm>

#include <batchprocessor.h>
#include <messages.h>
#include <orb/orbfeatureextractor.h>

BatchWorkerThread::BatchWorkerThread(ImageDownloader* imgDownloader, 
                                   FeatureExtractor* featureExtractor,
                                   const vector<BatchProcessingTask>& tasks,
                                   unordered_map<u_int32_t, list<HitForward>>& imageHits,
                                   vector<BatchImageResult>& results,
                                   pthread_mutex_t& resultsMutex)
    : imgDownloader(imgDownloader), featureExtractor(featureExtractor),
      tasks(tasks), imageHits(imageHits), results(results), resultsMutex(resultsMutex)
{ }

void* BatchWorkerThread::run() {
    // Local collections to minimize synchronization
    unordered_map<u_int32_t, list<HitForward>> localHits;
    vector<BatchImageResult> localResults;
    
    // Process all assigned tasks without synchronization
    for (const auto& task : tasks) {
        BatchImageResult result;
        result.imageId = task.imageId;
        result.url = task.url;
        result.tag = task.tag;  // Store the tag in the result
        
        // Download image if URL is provided
        vector<char> imageData;
        if (!task.url.empty()) {
            if (imgDownloader->canDownloadImage(task.url)) {
                long httpResponseCode;
                u_int32_t downloadStatus = imgDownloader->getImageData(
                    task.url, imageData, httpResponseCode);
                
                result.httpResponseCode = httpResponseCode;
                
                if (downloadStatus != OK) {
                    result.status = downloadStatus;
                    localResults.push_back(result);
                    continue;  // Skip to next image
                }
            } else {
                result.status = IMAGE_NOT_DECODED;
                localResults.push_back(result);
                continue;  // Skip to next image
            }
        } else {
            result.status = IMAGE_NOT_DECODED;
            localResults.push_back(result);
            continue;  // Skip to next image
        }
        
        // Extract features without adding to index
        list<HitForward> hits;
        unsigned nbFeaturesExtracted = 0;
        
        // We need to cast the feature extractor to ORBFeatureExtractor to use extractFeatures
        ORBFeatureExtractor* orbExtractor = dynamic_cast<ORBFeatureExtractor*>(featureExtractor);
        if (!orbExtractor) {
            result.status = ERROR_GENERIC;
            localResults.push_back(result);
            continue;
        }
        
        u_int32_t status = orbExtractor->extractFeatures(
            task.imageId, imageData.size(), imageData.data(),
            hits, nbFeaturesExtracted);
        
        result.status = status;
        result.nbFeaturesExtracted = nbFeaturesExtracted;
        
        // Store results locally
        if (status == IMAGE_ADDED) {
            localHits[task.imageId] = std::move(hits);
        }
        
        localResults.push_back(result);
    }
    
    // Now synchronize once to update shared collections
    pthread_mutex_lock(&resultsMutex);
    
    // Add local hits to shared hits collection
    for (auto& pair : localHits) {
        imageHits[pair.first] = std::move(pair.second);
    }
    
    // Add local results to shared results collection
    results.insert(results.end(), localResults.begin(), localResults.end());
    
    pthread_mutex_unlock(&resultsMutex);
    
    return NULL;
}

BatchProcessor::BatchProcessor(ImageDownloader* imgDownloader, 
                             FeatureExtractor* featureExtractor,
                             ORBIndex* index)
    : imgDownloader(imgDownloader), featureExtractor(featureExtractor), index(index)
{ }

vector<BatchImageResult> BatchProcessor::processBatch(const vector<Json::Value>& batchData) {
    // Prepare tasks from batch data
    vector<BatchProcessingTask> allTasks;
    allTasks.reserve(batchData.size());
    
    for (const auto& item : batchData) {
        BatchProcessingTask task;
        task.imageId = item["image_id"].asUInt();
        task.url = item["url"].asString();
        // Extract tag if present
        task.tag = item.isMember("tag") ? item["tag"].asString() : "";
        allTasks.push_back(task);
    }
    
    // Determine number of threads to use (hardcoded to NUM_THREADS)
    int actualThreads = std::min(NUM_THREADS, static_cast<int>(allTasks.size()));
    
    // Split tasks among threads
    vector<vector<BatchProcessingTask>> threadTasks(actualThreads);
    
    // Distribute tasks evenly
    for (size_t i = 0; i < allTasks.size(); i++) {
        threadTasks[i % actualThreads].push_back(allTasks[i]);
    }
    
    // Shared collections for results and hits
    vector<BatchImageResult> results;
    unordered_map<u_int32_t, list<HitForward>> imageHits;
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);
    
    // Create and start worker threads
    vector<BatchWorkerThread*> threads;
    for (int i = 0; i < actualThreads; i++) {
        BatchWorkerThread* thread = new BatchWorkerThread(
            imgDownloader, featureExtractor, threadTasks[i], 
            imageHits, results, mutex);
        threads.push_back(thread);
        thread->start();
    }
    
    // Wait for all threads to complete
    for (auto thread : threads) {
        thread->join();
        delete thread;
    }
    
    pthread_mutex_destroy(&mutex);
    
    // Add all hits to the index in one transaction
    if (!imageHits.empty()) {
        index->addBatchImages(imageHits);
    }
    
    // Collect tags for successfully processed images
    unordered_map<u_int32_t, string> imageTags;
    for (const auto& result : results) {
        // Only add tags for successfully processed images
        if (result.status == IMAGE_ADDED && !result.tag.empty()) {
            imageTags[result.imageId] = result.tag;
        }
    }
    
    // Log how many tags were collected for addition
    cout << "DEBUG: Collected " << imageTags.size() << " tags for batch addition" << endl;
    
    // Add all tags in one transaction
    if (!imageTags.empty()) {
        index->addBatchTags(imageTags);
    }
    
    // Write both indices to disk after batch processing
    cout << "DEBUG: Writing indices to disk after batch processing" << endl;
    index->write("");  // Pass empty string to use stored paths
    index->writeTags("");  // Pass empty string to use stored paths
    cout << "DEBUG: Indices written successfully" << endl;
    
    return results;
}
