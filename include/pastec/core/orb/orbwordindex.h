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

#include <string>
#include <sys/types.h>

#include <opencv2/core/core.hpp>
#include <opencv2/flann/flann.hpp>

namespace pastec {

class ORBWordIndex
{
public:
    ORBWordIndex(std::string visualWordPath);
    ~ORBWordIndex();
    u_int32_t getWordIndex(cv::Mat features, unsigned i_nbFeatures,
                           unsigned *indexes);

private:
    cv::flann::Index *index;
    cv::Mat words;
};

} // namespace pastec

#endif // PASTEC_ORBWORDINDEX_H