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

#ifndef PASTEC_ORBWORDINDEX_H
#define PASTEC_ORBWORDINDEX_H

#include <vector>

#include <opencv2/core/core.hpp>
#include <opencv2/flann.hpp>

// Include SimSIMD
#include <simsimd/simsimd.h>

using namespace cv;
using namespace std;

// Custom Hamming distance functor using SimSIMD
class SimSIMDHamming {
public:
    typedef unsigned char ElementType;
    typedef int ResultType;

    template <typename Iterator1, typename Iterator2>
    ResultType operator()(Iterator1 a, Iterator2 b, size_t size) const {
        simsimd_distance_t distance;
        simsimd_hamming_b8((simsimd_b8_t*)a, (simsimd_b8_t*)b, size, &distance);
        return (ResultType)distance;
    }
};


class ORBWordIndex
{
public:
    // Original constructor
    ORBWordIndex(string visualWordsPath);
    
    // Constructor that accepts an existing words matrix (shares the matrix)
    ORBWordIndex(const Mat* sharedWords);
    
    // Constructor that creates a deep copy of an existing words matrix
    ORBWordIndex(const Mat& wordsToCopy);
    
    ~ORBWordIndex();
    
    void knnSearch(const Mat &query, vector<int>& indices,
                   vector<int> &dists, int knn);
    
    // Getter for the words matrix
    Mat* getWords() const { return words; }

private:
    bool readVisualWords(string fileName);

    Mat *words;  // The matrix that stores the visual words.
    bool ownsWords; // Flag to indicate if this instance owns the words matrix
    cvflann::HierarchicalClusteringIndex<SimSIMDHamming> *kdIndex; // The kd-tree index with SimSIMD Hamming.
};

#endif // PASTEC_ORBWORDINDEX_H
