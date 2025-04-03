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

#ifndef PASTEC_BACKWARDINDEXREADERACCESS_H
#define PASTEC_BACKWARDINDEXREADERACCESS_H

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstring>

#include <iostream>
#include <string>
#include <fstream>


using namespace std;


class BackwardIndexReaderAccess
{
public:
    virtual ~BackwardIndexReaderAccess() {}
    virtual bool open(string indexPath) = 0;
    virtual void moveAt(u_int64_t pos) = 0;
    virtual void read(char *p_data, unsigned i_nbBytes) = 0;
    virtual bool endOfIndex() = 0;
    virtual void reset() = 0;
    virtual void close() = 0;
};


class BackwardIndexReaderFileAccess : public BackwardIndexReaderAccess
{
public:
    virtual bool open(string indexPath)
    {
        ifs.open(indexPath.c_str(), ios_base::binary);
        if (!ifs.good())
            return false;
        return true;
    }

    virtual void moveAt(u_int64_t pos)
    {
        ifs.seekg(pos);
    }

    virtual void read(char *p_data, unsigned i_nbBytes)
    {
        ifs.read(p_data, i_nbBytes);
    }

    virtual bool endOfIndex()
    {
        return ifs.eof();
    }

    virtual void reset()
    {
        ifs.clear();
    }

    virtual void close()
    {
        ifs.close();
    }

private:
    ifstream ifs;
};


class BackwardIndexReaderMemAccess : public BackwardIndexReaderAccess
{
public:
    virtual bool open(string indexPath)
    {
        ifstream ifs;
        ifs.open(indexPath.c_str(), ios_base::binary);
        if (!ifs.good())
        {
            ifs.close();
            return false;
        }

        ifs.seekg(0, std::ifstream::end);
        i_fileSize = ifs.tellg();

        // Try to allocate the needed size for the entire file.
        p_indexData = new char[i_fileSize];
        if (p_indexData == NULL)
        {
            cout << "Couldn't allocate the space to store the backward index file in memory." << endl;
            ifs.close();
            return false;
        }
        ifs.clear();
        ifs.seekg(0, ios_base::beg);

        // Copy the file in memory.
        u_int64_t i = 0;
        while(!ifs.eof())
        {
            ifs.read(p_indexData + i, 1);
            i++;
        }

        i_curPos = 0;

        ifs.close();
        return true;
    }

    virtual void moveAt(u_int64_t pos)
    {
        i_curPos = pos;
    }

    virtual void read(char *p_data, unsigned i_nbBytes)
    {
        memcpy(p_data, p_indexData + i_curPos, i_nbBytes);
        i_curPos += i_nbBytes;
    }

    virtual bool endOfIndex()
    {
        return i_curPos == i_fileSize;
    }

    virtual void reset()
    {
        i_curPos = 0;
    }

    virtual void close()
    {
        delete p_indexData;
    }

private:
    char *p_indexData;
    u_int64_t i_fileSize;
    u_int64_t i_curPos;
};

/**
 * Memory-mapped file access for the backward index.
 * This provides zero-copy access to the file data through the OS's virtual memory system.
 */
class BackwardIndexReaderMMapAccess : public BackwardIndexReaderAccess
{
public:
    BackwardIndexReaderMMapAccess() : fd(-1), mappedData(nullptr), i_fileSize(0), i_curPos(0) {}
    
    virtual bool open(string indexPath)
    {
        // Open the file
        fd = ::open(indexPath.c_str(), O_RDONLY);
        if (fd == -1)
        {
            cout << "Could not open the backward index file." << endl;
            return false;
        }

        // Get file size
        struct stat sb;
        if (fstat(fd, &sb) == -1)
        {
            cout << "Could not get file size." << endl;
            ::close(fd);
            fd = -1;
            return false;
        }
        i_fileSize = sb.st_size;

        // Map the file into memory
        mappedData = mmap(NULL, i_fileSize, PROT_READ, MAP_PRIVATE, fd, 0);
        if (mappedData == MAP_FAILED)
        {
            cout << "Could not memory map the index file." << endl;
            ::close(fd);
            fd = -1;
            mappedData = nullptr;
            return false;
        }

        // Advise the kernel that we'll access the data sequentially
        madvise(mappedData, i_fileSize, MADV_SEQUENTIAL);
        
        i_curPos = 0;
        return true;
    }

    virtual void moveAt(u_int64_t pos)
    {
        i_curPos = pos;
    }

    virtual void read(char *p_data, unsigned i_nbBytes)
    {
        if (i_curPos + i_nbBytes <= i_fileSize)
        {
            memcpy(p_data, static_cast<char*>(mappedData) + i_curPos, i_nbBytes);
            i_curPos += i_nbBytes;
        }
    }

    virtual bool endOfIndex()
    {
        return i_curPos >= i_fileSize;
    }

    virtual void reset()
    {
        i_curPos = 0;
    }

    virtual void close()
    {
        if (mappedData != nullptr && mappedData != MAP_FAILED)
        {
            munmap(mappedData, i_fileSize);
            mappedData = nullptr;
        }
        
        if (fd != -1)
        {
            ::close(fd);
            fd = -1;
        }
        
        i_fileSize = 0;
        i_curPos = 0;
    }

    // Get direct pointer to the mapped data at current position
    char* getCurrentDataPtr() const
    {
        return static_cast<char*>(mappedData) + i_curPos;
    }
    
    // Get direct pointer to the mapped data at specified offset
    char* getDataPtr(u_int64_t offset) const
    {
        return static_cast<char*>(mappedData) + offset;
    }
    
    // Get file size
    u_int64_t getFileSize() const
    {
        return i_fileSize;
    }

private:
    int fd;                 // File descriptor
    void* mappedData;       // Pointer to memory-mapped data
    u_int64_t i_fileSize;   // Size of the file
    u_int64_t i_curPos;     // Current position in the file
};

#endif // PASTEC_BACKWARDINDEXREADERACCESS_H
