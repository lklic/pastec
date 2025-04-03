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
#include <cassert>
#include <math.h>

#include <algorithm>

#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/calib3d/calib3d.hpp>

#include <imagereranker.h>
#include <orb/orbindex.h>


/**
 * @brief Rerank images using a vector of sorted results.
 * @param imagesReqHits the hits of the request image.
 * @param indexHits the hits of the index.
 * @param sortedResults the sorted vector of results (weight, imageId).
 * @param i_nbResults the number of results to rerank.
 * @return A vector of reranked search results.
 */
vector<SearchResult> ImageReranker::rerank(unordered_map<u_int32_t, list<Hit> > &imagesReqHits,
                                         unordered_map<u_int32_t, const vector<Hit>* > &indexHits,
                                         const vector<pair<float, u_int32_t>> &sortedResults,
                                         unsigned i_nbResults)
{
    unordered_set<u_int32_t> firstImageIds;
    // Extract the first i_nbResults ranked images from the vector.
    getFirstImageIds(sortedResults, i_nbResults, firstImageIds);         
    // Continue with the common reranking logic
    return rerankCommon(imagesReqHits, indexHits, firstImageIds);
}

/**
 * @brief Return the first ids of ranked images from a sorted vector.
 * @param sortedResults the sorted vector of results (weight, imageId).
 * @param i_nbResults the number of images to return.
 * @param firstImageIds a set to return the image ids.
 */
void ImageReranker::getFirstImageIds(const vector<pair<float, u_int32_t>> &sortedResults,
                                    unsigned i_nbResults, unordered_set<u_int32_t> &firstImageIds)
{
    unsigned i_res = 0;
    for (const auto& result : sortedResults)
    {
        if (i_res >= i_nbResults)
            break;
        
        firstImageIds.insert(result.second); // Insert the image ID
        i_res++;
    }
}

/**
 * @brief Common reranking implementation.
 * @param imagesReqHits the hits of the request image.
 * @param indexHits the hits of the index.
 * @param firstImageIds the set of image IDs to rerank.
 * @return A vector of reranked search results.
 */
vector<SearchResult> ImageReranker::rerankCommon(unordered_map<u_int32_t, list<Hit> > &imagesReqHits,
                                               unordered_map<u_int32_t, const vector<Hit>* > &indexHits,
                                               unordered_set<u_int32_t> &firstImageIds)
{
    // Use PointPairs instead of RANSACTask
    unordered_map<u_int32_t, PointPairs> imgPointPairs;

    // Compute the histograms.
    unordered_map<u_int32_t, Histogram> histograms; // key: the image id, value: the corresponding histogram.
    
    unsigned totalMatches = 0;
    unsigned totalHistogramEntries = 0;
    unsigned totalPointPairs = 0;

    for (unordered_map<u_int32_t, list<Hit> >::const_iterator it = imagesReqHits.begin();
         it != imagesReqHits.end(); ++it)
    {
        // Try to match all the visual words of the request image.
        const unsigned i_wordId = it->first;
        const list<Hit> &hits = it->second;

        assert(hits.size() == 1);

        // If there is several hits for the same word in the image...
        const u_int16_t i_angle1 = hits.front().i_angle;
        const Point2f point1(hits.front().x, hits.front().y);
        const vector<Hit> *hitIndex = indexHits[i_wordId];
        
        if (!hitIndex) {
            continue;
        }
        
        unsigned matchesForThisWord = 0;

        for (unsigned i = 0; i < hitIndex->size(); ++i)
        {
            const u_int32_t i_imageId = (*hitIndex)[i].i_imageId;
            // Test if the image belongs to the image to rerank.
            if (firstImageIds.find(i_imageId) != firstImageIds.end())
            {
                matchesForThisWord++;
                totalMatches++;
                
                const u_int16_t i_angle2 = (*hitIndex)[i].i_angle;
                float f_diff = angleDiff(i_angle1, i_angle2);
                unsigned bin = (f_diff - DIFF_MIN) / 360 * HISTOGRAM_NB_BINS;
                assert(bin < HISTOGRAM_NB_BINS);

                Histogram &histogram = histograms[i_imageId];
                histogram.bins[bin]++;
                histogram.i_total++;
                totalHistogramEntries++;

                const Point2f point2((*hitIndex)[i].x, (*hitIndex)[i].y);
                PointPairs &pointPairs = imgPointPairs[i_imageId];

                pointPairs.points1.push_back(point1);
                pointPairs.points2.push_back(point2);
                totalPointPairs++;
            }
        }
        
        // Debug output removed to improve performance
    }

    // Create a vector to store the results
    vector<SearchResult> rankedResults;
    rankedResults.reserve(histograms.size()); // Reserve space for efficiency

    // Process all images in a single thread
    unsigned ransacAttempts = 0;
    unsigned successfulRansacs = 0;
    unsigned skippedDueToLowValue = 0;
    unsigned skippedDueToFewPoints = 0;
    unsigned skippedDueToZeroH = 0;
    
    // Rank the images according to their histogram.
    for (const auto& histogramPair : histograms)
    {
        const unsigned i_imageId = histogramPair.first;
        const Histogram& histogram = histogramPair.second;
        
        // Find the maximum bin value
        unsigned i_binMax = max_element(histogram.bins, histogram.bins + HISTOGRAM_NB_BINS) - histogram.bins;
        float i_maxVal = histogram.bins[i_binMax];
        
        if (i_maxVal > 10)
        {
            const PointPairs& pointPairs = imgPointPairs[i_imageId];
            assert(pointPairs.points1.size() == pointPairs.points2.size());

            if (pointPairs.points1.size() >= RANSAC_MIN_INLINERS)
            {
                ransacAttempts++;                
                Mat H = RANSACHelper::pastecEstimateRigidTransform(pointPairs.points2, pointPairs.points1, true);

                if (countNonZero(H) == 0) {
                    skippedDueToZeroH++;
                    continue;
                }

                Rect bRect1 = boundingRect(pointPairs.points1);
                rankedResults.push_back(SearchResult(i_maxVal, i_imageId, bRect1));
                
                successfulRansacs++;
            }
            else {
                skippedDueToFewPoints++;
            }
        }
        else {
            skippedDueToLowValue++;
        }
    }

    // Sort the results by weight in descending order
    sort(rankedResults.begin(), rankedResults.end(), 
         [](const SearchResult& a, const SearchResult& b) {
             return a.f_weight > b.f_weight;
         });
    
    return rankedResults;
}


class Pos {
public:
    Pos(int x, int y) : x(x), y(y) {}

    inline bool operator< (const Pos &rhs) const {
        if (x != rhs.x)
            return x < rhs.x;
        else
            return y < rhs.y;
    }

private:
    int x, y;
};


/**
 * @brief Rerank images using the forward index for better performance.
 * @param imagesReqHits the hits of the request image.
 * @param index the ORB index with forward index.
 * @param firstImageIds the set of image IDs to rerank.
 * @return A vector of reranked search results.
 */
vector<SearchResult> ImageReranker::rerankUsingForwardIndex(unordered_map<u_int32_t, list<Hit> > &imagesReqHits,
                                                          ORBIndex* index,
                                                          unordered_set<u_int32_t> &firstImageIds)
{    
    // Create a map of query words for fast lookup
    unordered_map<u_int32_t, Hit> queryWords;
    for (const auto& pair : imagesReqHits) {
        queryWords[pair.first] = pair.second.front();
    }
    
    // Use PointPairs instead of RANSACTask
    unordered_map<u_int32_t, PointPairs> imgPointPairs;
    
    // Compute the histograms
    unordered_map<u_int32_t, Histogram> histograms;
    
    unsigned totalMatches = 0;
    unsigned totalHistogramEntries = 0;
    unsigned totalPointPairs = 0;
    
    // Process each image in the reranking set
    for (const u_int32_t i_imageId : firstImageIds) {
        // Get all words for this image from the forward index
        const vector<unsigned>& imageWords = index->getForwardIndexWords(i_imageId);
        
        // For each word in this image
        for (const unsigned i_wordId : imageWords) {
            // Check if this word exists in the query image
            auto queryIt = queryWords.find(i_wordId);
            if (queryIt == queryWords.end()) {
                continue;  // Word not in query, skip
            }
            
            // Get the hit from the index for this word and image
            const Hit* indexHit = index->getHitForWordAndImage(i_wordId, i_imageId);
            if (!indexHit) {
                continue;  // No hit found, skip
            }
            
            totalMatches++;
            
            // Calculate angle difference
            const u_int16_t i_angle1 = queryIt->second.i_angle;
            const u_int16_t i_angle2 = indexHit->i_angle;
            float f_diff = angleDiff(i_angle1, i_angle2);
            unsigned bin = (f_diff - DIFF_MIN) / 360 * HISTOGRAM_NB_BINS;
            assert(bin < HISTOGRAM_NB_BINS);
            
            // Update histogram
            Histogram &histogram = histograms[i_imageId];
            histogram.bins[bin]++;
            histogram.i_total++;
            totalHistogramEntries++;
            
            // Store point pairs for RANSAC
            const Point2f point1(queryIt->second.x, queryIt->second.y);
            const Point2f point2(indexHit->x, indexHit->y);
            PointPairs &pointPairs = imgPointPairs[i_imageId];
            
            pointPairs.points1.push_back(point1);
            pointPairs.points2.push_back(point2);
            totalPointPairs++;
        }
    }
    
    // Create a vector to store the results
    vector<SearchResult> rankedResults;
    rankedResults.reserve(histograms.size()); // Reserve space for efficiency

    // Process all images in a single thread
    unsigned ransacAttempts = 0;
    unsigned successfulRansacs = 0;
    unsigned skippedDueToLowValue = 0;
    unsigned skippedDueToFewPoints = 0;
    unsigned skippedDueToZeroH = 0;
    
    // Rank the images according to their histogram.
    for (const auto& histogramPair : histograms)
    {
        const unsigned i_imageId = histogramPair.first;
        const Histogram& histogram = histogramPair.second;
        
        // Find the maximum bin value
        unsigned i_binMax = max_element(histogram.bins, histogram.bins + HISTOGRAM_NB_BINS) - histogram.bins;
        float i_maxVal = histogram.bins[i_binMax];
        
        if (i_maxVal > 10)
        {
            const PointPairs& pointPairs = imgPointPairs[i_imageId];
            assert(pointPairs.points1.size() == pointPairs.points2.size());

            if (pointPairs.points1.size() >= RANSAC_MIN_INLINERS)
            {
                ransacAttempts++;                
                Mat H = RANSACHelper::pastecEstimateRigidTransform(pointPairs.points2, pointPairs.points1, true);                
                if (countNonZero(H) == 0) {
                    skippedDueToZeroH++;
                    continue;
                }

                Rect bRect1 = boundingRect(pointPairs.points1);
                rankedResults.push_back(SearchResult(i_maxVal, i_imageId, bRect1));
                
                successfulRansacs++;                
            }
            else {
                skippedDueToFewPoints++;
            }
        }
        else {
            skippedDueToLowValue++;
        }
    }
    
    // Sort the results by weight in descending order
    sort(rankedResults.begin(), rankedResults.end(), 
         [](const SearchResult& a, const SearchResult& b) {
             return a.f_weight > b.f_weight;
         });
    
    return rankedResults;
}

float ImageReranker::angleDiff(unsigned i_angle1, unsigned i_angle2)
{
    // Convert the angle in the [-180, 180] range.
    float i1 = (float)i_angle1 * 360 / (1 << 16);
    float i2 = (float)i_angle2 * 360 / (1 << 16);

    i1 = i1 <= 180 ? i1 : i1 - 360;
    i2 = i2 <= 180 ? i2 : i2 - 360;

    // Compute the difference between the two angles.
    float diff = i1 - i2;
    if (diff < DIFF_MIN)
        diff += 360;
    else if (diff >= 360 + DIFF_MIN)
        diff -= 360;

    assert(diff >= DIFF_MIN);
    assert(diff < 360 + DIFF_MIN);

    return diff;
}
