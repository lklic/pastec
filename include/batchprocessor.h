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

#ifndef PASTEC_BATCHPROCESSOR_H
#define PASTEC_BATCHPROCESSOR_H

#include <vector>
#include <unordered_map>
#include <list>
#include <string>
#include <pthread.h>

#include <json/json.h>
#include <thread.h>
#include <hit.h>
#include <imagedownloader.h>
#include <featureextractor.h>
#include <orb/orbindex.h>

using namespace std;

// Structure to hold the result of processing a single image in a batch
struct BatchImageResult {
    u_int32_t imageId;
    string url;
    string tag;  // Added tag field
    u_int32_t status;  // Using the same status codes as single image processing
    unsigned nbFeaturesExtracted;
    long httpResponseCode;  // For URL-based images
};

// Structure to hold the task information for batch processing
struct BatchProcessingTask {
    u_int32_t imageId;
    string url;
    string tag;  // Added tag field
};

// Worker thread for batch processing
class BatchWorkerThread : public Thread {
public:
    BatchWorkerThread(ImageDownloader* imgDownloader, 
                     FeatureExtractor* featureExtractor,
                     const vector<BatchProcessingTask>& tasks,
                     unordered_map<u_int32_t, list<HitForward>>& imageHits,
                     vector<BatchImageResult>& results,
                     pthread_mutex_t& resultsMutex);
    
private:
    virtual void* run();
    
    ImageDownloader* imgDownloader;
    FeatureExtractor* featureExtractor;
    const vector<BatchProcessingTask> tasks;  // This thread's dedicated tasks
    unordered_map<u_int32_t, list<HitForward>>& imageHits;  // Shared hits collection
    vector<BatchImageResult>& results;  // Shared results collection
    pthread_mutex_t& resultsMutex;  // Mutex for thread-safe updates to shared collections
};

// Main batch processor class
class BatchProcessor {
public:
    BatchProcessor(ImageDownloader* imgDownloader, 
                  FeatureExtractor* featureExtractor,
                  ORBIndex* index);
    
    vector<BatchImageResult> processBatch(const vector<Json::Value>& batchData);
    
private:
    static const int NUM_THREADS = 20;  // Hardcoded number of threads
    
    ImageDownloader* imgDownloader;
    FeatureExtractor* featureExtractor;
    ORBIndex* index;
};

#endif // PASTEC_BATCHPROCESSOR_H
