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

#ifndef PASTEC_H
#define PASTEC_H

// Core functionality
#include "pastec/core/index.h"
#include "pastec/core/searcher.h"
#include "pastec/core/featureextractor.h"
#include "pastec/core/imageloader.h"
#include "pastec/core/hit.h"
#include "pastec/core/searchResult.h"
#include "pastec/core/imagedownloader.h"

// ORB-specific implementations
#include "pastec/core/orb/orbindex.h"
#include "pastec/core/orb/orbsearcher.h"
#include "pastec/core/orb/orbfeatureextractor.h"
#include "pastec/core/orb/orbwordindex.h"

#endif // PASTEC_H