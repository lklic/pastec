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

#ifndef PASTEC_IMAGESEARCHER_H
#define PASTEC_IMAGESEARCHER_H

#include <queue>
#include <memory>
#include <future>
#include <vector>

#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>

#include <opencv2/core/core.hpp>
#include <opencv2/flann/flann.hpp>

#include <searcher.h>
#include <orbindex.h>
#include <orbwordindex.h>
#include <searchResult.h>
#include <imagereranker.h>

using namespace cv;
using namespace std;

class ClientConnection;

// Structure to hold a hit with its distance during search
struct SearchHit {
    Hit hit;           // The original hit data
    float distance;    // KNN distance (smaller is better)
};


class ORBSearcher : public Searcher
{
public:
    ORBSearcher(ORBIndex *index, ORBWordIndex *wordIndex);
    virtual ~ORBSearcher();
    u_int32_t searchImage(SearchRequest &request);
    u_int32_t searchSimilar(SearchRequest &request);

private:
    // Process a batch of keypoints
    std::vector<std::pair<u_int32_t, SearchHit>> processKeyPointBatch(
        const Mat& descriptors,
        const vector<KeyPoint>& keypoints,
        size_t startIdx,
        size_t endIdx,
        ORBWordIndex* localWordIndex);

    // Helper method to process a batch of words for TF-IDF computation
    vector<float> processTFIDFBatch(
        const vector<pair<u_int32_t, const vector<Hit>*>>& batch,
        const vector<unsigned>& wordCounts,
        unsigned i_nbTotalIndexedImages,
        unsigned maxImageId);

    void returnResults(vector<SearchResult> &rankedResults,
                       SearchRequest &req, unsigned i_maxNbResults);
    unsigned long getTimeDiff(const timeval t1, const timeval t2) const;
    u_int32_t processSimilar(SearchRequest &request,
                             std::unordered_map<u_int32_t, list<Hit> > imageReqHits);

    // Constants
    static const int NUM_THREADS = 20;          // Increased from 3
    static const int FEATURE_BATCH_COUNT = 8;   // Number of batches for feature extraction
    static const int WEIGHT_BATCH_COUNT = 4;    // Number of batches for TF-IDF computation

    // Original members
    ORBIndex *index;
    ORBWordIndex *wordIndex;
    ImageReranker reranker;
    Ptr<ORB> orb;
    
    // Thread pool members
    std::vector<std::unique_ptr<ORBWordIndex>> threadWordIndices;
    boost::asio::thread_pool threadPool;
};

#endif // PASTEC_IMAGESEARCHER_H
