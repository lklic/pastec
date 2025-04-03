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

#ifndef PASTEC_IMAGERERANKER_H
#define PASTEC_IMAGERERANKER_H

#include <sys/types.h>
#include <sys/time.h>

#include <queue>
#include <list>
#include <unordered_map>
#include <unordered_set>

#include <opencv2/core/core.hpp>

#include <searchResult.h>
#include <hit.h>

using namespace std;
using namespace cv;


class ImageReranker
{
public:
    ImageReranker() {}
    
    // Main reranking method that works with vectors
    vector<SearchResult> rerank(std::unordered_map<u_int32_t, list<Hit> > &imagesReqHits,
                              std::unordered_map<u_int32_t, const vector<Hit>* > &indexHits,
                              const vector<pair<float, u_int32_t>> &sortedResults,
                              unsigned i_nbResults);
                              
    // Reranking method that uses the forward index for better performance
    vector<SearchResult> rerankUsingForwardIndex(std::unordered_map<u_int32_t, list<Hit> > &imagesReqHits,
                                               class ORBIndex* index,
                                               std::unordered_set<u_int32_t> &firstImageIds);

private:
    float angleDiff(unsigned i_angle1, unsigned i_angle2);
    void getFirstImageIds(const vector<pair<float, u_int32_t>> &sortedResults,
                         unsigned i_nbResults, unordered_set<u_int32_t> &firstImageIds);
    
    // Common reranking implementation
    vector<SearchResult> rerankCommon(std::unordered_map<u_int32_t, list<Hit> > &imagesReqHits,
                                    std::unordered_map<u_int32_t, const vector<Hit>* > &indexHits,
                                    unordered_set<u_int32_t> &firstImageIds);
};


// Point pairs for RANSAC
struct PointPairs
{
    vector<Point2f> points1;
    vector<Point2f> points2;
};

#define HISTOGRAM_NB_BINS 32
#define DIFF_MIN -360.0f / (2.0f * HISTOGRAM_NB_BINS)

struct Histogram
{
    Histogram() : i_total(0)
    {
        for (unsigned i = 0; i < HISTOGRAM_NB_BINS; ++i)
            bins[i] = 0;
    }
    unsigned bins[HISTOGRAM_NB_BINS];
    unsigned i_total;
};

#define RANSAC_MIN_INLINERS 12

// Helper functions for RANSAC
class RANSACHelper
{
public:
    static void getRTMatrix(const Point2f* a, const Point2f* b,
                     int count, Mat& M, bool fullAffine);
    static cv::Mat pastecEstimateRigidTransform(InputArray src1, InputArray src2,
                                         bool fullAffine);
    
    // Helper method to calculate time difference in milliseconds
    static unsigned long getTimeDiff(const timeval t1, const timeval t2)
    {
        return ((t2.tv_sec - t1.tv_sec) * 1000000
                + (t2.tv_usec - t1.tv_usec)) / 1000;
    }
};


#endif // PASTEC_IMAGERERANKER_H
