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
#include <stdlib.h>
#include <memory>
#include <sstream>
#include <sys/time.h>

#include <json/json.h>

#include <requesthandler.h>
#include <messages.h>
#include <featureextractor.h>
#include <searcher.h>
#include <index.h>

#include <imageloader.h>
#include <opencv2/highgui/highgui.hpp>


RequestHandler::RequestHandler(FeatureExtractor *featureExtractor,
               Searcher *imageSearcher, Index *index,
               ImageDownloader *imgDownloader, string authKey)
    : featureExtractor(featureExtractor), imageSearcher(imageSearcher),
      index(index), imgDownloader(imgDownloader), authKey(authKey)
{
    // Initialize the batch processor
    batchProcessor = new BatchProcessor(imgDownloader, featureExtractor, 
                                       dynamic_cast<ORBIndex*>(index));
}

RequestHandler::~RequestHandler()
{
    delete batchProcessor;
}


/**
 * @brief Parse an URI.
 * @param uri the uri string.
 * @return the vector containing all the URI element between slashes.
 */
vector<string> RequestHandler::parseURI(string uri)
{
    vector<string> ret;

    if (uri == "/" || uri[0] != '/')
        return ret;

    size_t pos1 = 1;
    size_t pos2;

    while ((pos2 = uri.find('/', pos1)) != string::npos)
    {
        ret.push_back(uri.substr(pos1, pos2 - pos1));
        pos1 = pos2 + 1;
    }

    ret.push_back(uri.substr(pos1, uri.length() - pos1));

    return ret;
}


/**
 * @brief Test that a given parsed URI corresponds to a given request pattern.
 * @param parsedURI the parsed URI.
 * @param p_pattern the request pattern.
 * @return true if there is a correspondance, else false.
 */
bool RequestHandler::testURIWithPattern(vector<string> parsedURI, string p_pattern[])
{
    unsigned i = 0;
    for (;; ++i)
    {
        if (p_pattern[i] == "")
            break;
        if (i >= parsedURI.size())
            return false;
        if (p_pattern[i] == "IDENTIFIER")
        {
            // Test we have a number here.
            if (parsedURI[i].length() == 0)
                return false;
            char* p;
            long n = strtol(parsedURI[i].c_str(), &p, 10);
            if (*p != 0)
                return false;
            if (n < 0)
                return false;
        }
        else if (p_pattern[i] != parsedURI[i])
            return false;
    }

    if (i != parsedURI.size())
        return false;

    return true;
}


/**
 * @brief RequestHandler::handlePost
 * @param uri
 * @param p_data
 * @return
 */
void RequestHandler::handleRequest(ConnectionInfo &conInfo)
{
    vector<string> parsedURI = parseURI(conInfo.url);

    string p_image[] = {"index", "images", "IDENTIFIER", ""};
    string p_imageBatch[] = {"index", "images", "batch", ""};
    string p_tag[] = {"index", "images", "IDENTIFIER", "tag", ""};
    string p_searchImage[] = {"index", "searcher", ""};
    string p_ioIndex[] = {"index", "io", ""};
    string p_imageIds[] = {"index", "imageIds", ""};
    string p_root[] = {""};

    Json::Value ret;
    conInfo.answerCode = MHD_HTTP_OK;

    if (authKey != "" && conInfo.authKey != authKey) {
        conInfo.answerCode = MHD_HTTP_FORBIDDEN;
        ret["type"] = Converter::codeToString(AUTHENTIFICATION_ERROR);
    }
    else if (testURIWithPattern(parsedURI, p_image)
        && conInfo.connectionType == POST)
    {
        u_int32_t i_imageId = atoi(parsedURI[2].c_str());
        unsigned i_nbFeaturesExtracted;
        u_int32_t i_ret;

        // Check if the content type is JSON
        if (conInfo.contentType.find("application/json") != string::npos)
        {
            // Process as JSON with URL            
            string dataStr(conInfo.uploadedData.begin(), conInfo.uploadedData.end());
            Json::Value data = StringToJson(dataStr);
            string imgURL = data["url"].asString();
                        
            if (imgDownloader->canDownloadImage(imgURL))
            {
                std::vector<char> imgData;
                long HTTPResponseCode;
                i_ret = imgDownloader->getImageData(imgURL, imgData, HTTPResponseCode);
                if (i_ret == OK)
                {
                    i_ret = featureExtractor->processNewImage(
                        i_imageId, imgData.size(), imgData.data(),
                        i_nbFeaturesExtracted);
                }
                else
                {
                    ret["image_downloader_http_response_code"] = (Json::Int64)HTTPResponseCode;
                }
            }
            else
            {
                i_ret = MISFORMATTED_REQUEST;
            }
        }
        else
        {
            // Process as direct image upload
            i_ret = featureExtractor->processNewImage(
                i_imageId, conInfo.uploadedData.size(), conInfo.uploadedData.data(),
                i_nbFeaturesExtracted);
        }

        ret["type"] = Converter::codeToString(i_ret);
        ret["image_id"] = Json::Value(i_imageId);
        if (i_ret == IMAGE_ADDED)
            ret["nb_features_extracted"] = Json::Value(i_nbFeaturesExtracted);
    }
    else if (testURIWithPattern(parsedURI, p_image)
             && conInfo.connectionType == DELETE)
    {
        u_int32_t i_imageId = atoi(parsedURI[2].c_str());

        u_int32_t i_ret = index->removeImage(i_imageId);
        ret["type"] = Converter::codeToString(i_ret);
        ret["image_id"] = Json::Value(i_imageId);
    }
    else if (testURIWithPattern(parsedURI, p_imageBatch)
             && conInfo.connectionType == POST)
    {
        string dataStr(conInfo.uploadedData.begin(),
                      conInfo.uploadedData.end());
        
        Json::Value data = StringToJson(dataStr);
        
        // Validate the request format
        if (!data.isArray()) {
            ret["type"] = Converter::codeToString(MISFORMATTED_REQUEST);
            conInfo.answerString = JsonToString(ret);
            return;
        }
        
        // Convert JSON array to vector
        vector<Json::Value> batchData;
        for (unsigned i = 0; i < data.size(); i++) {
            batchData.push_back(data[i]);
        }
        
        // Process the batch
        vector<BatchImageResult> results = batchProcessor->processBatch(batchData);
        
        // Create response
        ret["type"] = Converter::codeToString(BATCH_PROCESSED);
        
        Json::Value resultsArray(Json::arrayValue);
        for (const auto& result : results) {
            Json::Value resultObj;
            resultObj["image_id"] = result.imageId;
            resultObj["url"] = result.url;
            resultObj["type"] = Converter::codeToString(result.status);
            
            if (result.status == IMAGE_ADDED) {
                resultObj["nb_features_extracted"] = result.nbFeaturesExtracted;
                
                // Include tag status if a tag was provided
                if (!result.tag.empty()) {
                    resultObj["tag"] = result.tag;
                    resultObj["tag_status"] = Converter::codeToString(IMAGE_TAG_ADDED);
                }
            }
            
            if (!result.url.empty() && result.status != IMAGE_ADDED) {
                resultObj["image_downloader_http_response_code"] = (Json::Int64)result.httpResponseCode;
            }
            
            resultsArray.append(resultObj);
        }
        
        ret["results"] = resultsArray;
    }
    else if (testURIWithPattern(parsedURI, p_tag)
             && conInfo.connectionType == POST)
    {
        u_int32_t i_imageId = atoi(parsedURI[2].c_str());

        string dataStr(conInfo.uploadedData.begin(),
                       conInfo.uploadedData.end());

        u_int32_t i_ret = index->addTag(i_imageId, dataStr);

        ret["type"] = Converter::codeToString(i_ret);
    }
    else if (testURIWithPattern(parsedURI, p_tag)
             && conInfo.connectionType == DELETE)
    {
        u_int32_t i_imageId = atoi(parsedURI[2].c_str());

        u_int32_t i_ret = index->removeTag(i_imageId);

        ret["type"] = Converter::codeToString(i_ret);
    }
    else if (testURIWithPattern(parsedURI, p_searchImage)
            && conInfo.connectionType == POST)
    {
        timeval t_start, t_end;
        gettimeofday(&t_start, NULL);
        
        SearchRequest req;
        req.client = NULL;
        u_int32_t i_ret;

        // Check if the content type is JSON
        if (conInfo.contentType.find("application/json") != string::npos)
        {            
            string dataStr(conInfo.uploadedData.begin(), conInfo.uploadedData.end());
            Json::Value data = StringToJson(dataStr);
            string imgURL = data["url"].asString();            
            
            if (imgDownloader->canDownloadImage(imgURL))
            {
                std::vector<char> imgData;
                long HTTPResponseCode;
                
                // Add timing for the image download
                timeval t_download_start, t_download_end;
                gettimeofday(&t_download_start, NULL);
                
                i_ret = imgDownloader->getImageData(imgURL, imgData, HTTPResponseCode);
                
                gettimeofday(&t_download_end, NULL);
                cout << "Image download time: " << getTimeDiff(t_download_start, t_download_end) << " ms." << endl;
                
                if (i_ret == OK)
                {
                    req.imageData = imgData;
                    
                    // Add timing for the search call
                    timeval t_search_start, t_search_end;
                    gettimeofday(&t_search_start, NULL);
                    
                    i_ret = imageSearcher->searchImage(req);
                    
                    gettimeofday(&t_search_end, NULL);
                    cout << "Search function call time: " << getTimeDiff(t_search_start, t_search_end) << " ms." << endl;
                }
                else {
                    ret["type"] = Converter::codeToString(i_ret);
                    ret["image_downloader_http_response_code"] = (Json::Int64)HTTPResponseCode;
                    conInfo.answerString = JsonToString(ret);
                    return;
                }
            }
            else
            {
                i_ret = MISFORMATTED_REQUEST;
            }
        }
        else
        {
            // Process as direct image upload
            req.imageData = conInfo.uploadedData;
            
            // Add timing for the search call
            timeval t_search_start, t_search_end;
            gettimeofday(&t_search_start, NULL);
            
            i_ret = imageSearcher->searchImage(req);
            
            gettimeofday(&t_search_end, NULL);
            cout << "Search function call time: " << getTimeDiff(t_search_start, t_search_end) << " ms." << endl;
        }

        ret["type"] = Converter::codeToString(i_ret);
        if (i_ret == SEARCH_RESULTS)
        {
            // Create array of result objects
            Json::Value results(Json::arrayValue);
            
            // Safety check that we have at least one result
            if (!req.results.empty()) {
                for (unsigned i = 0; i < req.results.size(); ++i)
                {
                    Json::Value resultObj;
                    resultObj["image_id"] = req.results[i];
                    
                    // Add score if available
                    if (i < req.scores.size()) {
                        resultObj["score"] = req.scores[i];
                    }
                    
                    // Add tag if available
                    if (i < req.tags.size()) {
                        resultObj["tag"] = req.tags[i];
                    }
                    
                    // Add bounding rectangle if available
                    if (i < req.boundingRects.size()) {
                        Json::Value rect;
                        rect["x"] = req.boundingRects[i].x;
                        rect["y"] = req.boundingRects[i].y; 
                        rect["width"] = req.boundingRects[i].width;
                        rect["height"] = req.boundingRects[i].height;
                        resultObj["bounding_rect"] = rect;
                    }

                    results.append(resultObj);
                }
            }
            ret["results"] = results;
        }
        
        gettimeofday(&t_end, NULL);
        cout << "Total search request processing time: " << getTimeDiff(t_start, t_end) << " ms." << endl;
    }

    // And this is the updated similar search handler
    else if (testURIWithPattern(parsedURI, p_image)
            && conInfo.connectionType == GET)
    {
        timeval t_start, t_end;
        gettimeofday(&t_start, NULL);
        
        SearchRequest req;
        req.imageId = atoi(parsedURI[2].c_str());
        req.client = NULL;
        u_int32_t i_ret = imageSearcher->searchSimilar(req);

        ret["type"] = Converter::codeToString(i_ret);

        if (i_ret == SEARCH_RESULTS)
        {
            // Create array of result objects
            Json::Value results(Json::arrayValue);
            
            // Safety check that we have at least one result
            if (!req.results.empty()) {
                for (unsigned i = 0; i < req.results.size(); ++i)
                {
                    Json::Value resultObj;
                    resultObj["image_id"] = req.results[i];
                    
                    // Add score if available  
                    if (i < req.scores.size()) {
                        resultObj["score"] = req.scores[i];
                    }
                    
                    // Add tag if available
                    if (i < req.tags.size()) {
                        resultObj["tag"] = req.tags[i];
                    }
                    
                    // Add bounding rectangle if available
                    if (i < req.boundingRects.size()) {
                        Json::Value rect;
                        rect["x"] = req.boundingRects[i].x;
                        rect["y"] = req.boundingRects[i].y;
                        rect["width"] = req.boundingRects[i].width;
                        rect["height"] = req.boundingRects[i].height;
                        resultObj["bounding_rect"] = rect;
                    }

                    results.append(resultObj);
                }
            }
            ret["results"] = results;
        }
        
        gettimeofday(&t_end, NULL);
        cout << "Total similar search request processing time: " << getTimeDiff(t_start, t_end) << " ms." << endl;
    }
    else if (testURIWithPattern(parsedURI, p_ioIndex)
             && conInfo.connectionType == POST)
    {
        string dataStr(conInfo.uploadedData.begin(),
                       conInfo.uploadedData.end());

        Json::Value data = StringToJson(dataStr);
        u_int32_t i_ret;
        if (data["type"] == "LOAD")
            i_ret = index->load(data["index_path"].asString());
        else if (data["type"] == "WRITE")
            i_ret = index->write(data["index_path"].asString());
        else if (data["type"] == "LOAD_TAGS")
            i_ret = index->loadTags(data["index_tags_path"].asString());
        else if (data["type"] == "WRITE_TAGS")
            i_ret = index->writeTags(data["index_tags_path"].asString());
        else if (data["type"] == "CLEAR")
            i_ret = index->clear();
        else
            i_ret = MISFORMATTED_REQUEST;

        ret["type"] = Converter::codeToString(i_ret);
    }
    else if (testURIWithPattern(parsedURI, p_imageIds)
             && conInfo.connectionType == GET)
    {
        vector<u_int32_t> imageIds;
        u_int32_t i_ret = index->getImageIds(imageIds);

        ret["type"] = Converter::codeToString(i_ret);

        // Return the image ids
        Json::Value imageIdsVal(Json::arrayValue);
        for (unsigned i = 0; i < imageIds.size(); ++i)
            imageIdsVal.append(imageIds[i]);
        ret["image_ids"] = imageIdsVal;
    }
    else if (testURIWithPattern(parsedURI, p_root)
             && conInfo.connectionType == POST)
    {
        string dataStr(conInfo.uploadedData.begin(),
                       conInfo.uploadedData.end());

        Json::Value data = StringToJson(dataStr);
        u_int32_t i_ret;
        if (data["type"] == "PING")
        {
            cout << "Ping received." << endl;
            i_ret = PONG;
        }
        else
            i_ret = MISFORMATTED_REQUEST;

        ret["type"] = Converter::codeToString(i_ret);
    }
    else
    {
        conInfo.answerCode = MHD_HTTP_INTERNAL_SERVER_ERROR;
        ret["type"] = Converter::codeToString(MISFORMATTED_REQUEST);
    }

    conInfo.answerString = JsonToString(ret);
}


/**
 * @brief Conver to JSON value to a string.
 * @param data the JSON value.
 * @return the converted string.
 */
string RequestHandler::JsonToString(Json::Value data)
{
    Json::StreamWriterBuilder builder;
    builder["commentStyle"] = "None";
    builder["indentation"] = "";
    return  Json::writeString(builder, data);
}


/**
 * @brief Convert a string to a JSON value.
 * @param str the string
 * @return the converted JSON value.
 */
Json::Value RequestHandler::StringToJson(string inputStr)
{
    Json::CharReaderBuilder builder;
    Json::Value data;
    std::string errs;
    std::stringstream ss;
    ss.str(inputStr);
    Json::parseFromStream(builder, ss, &data, &errs);
    return data;
}


/**
 * @brief Get the time difference in ms between two instants.
 * @param t1 the start time
 * @param t2 the end time
 * @return the time difference in milliseconds
 */
unsigned long RequestHandler::getTimeDiff(const timeval t1, const timeval t2) const
{
    return ((t2.tv_sec - t1.tv_sec) * 1000000
            + (t2.tv_usec - t1.tv_usec)) / 1000;
}
