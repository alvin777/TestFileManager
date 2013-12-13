//
//  ResourcesManager.cpp
//  TestFileManager
//
//  Created by Stanislav on 13.12.13.
//  Copyright (c) 2013 Redsteep. All rights reserved.
//

#include "ResourcesManager.h"

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include <vector>
#include <map>

#include "unzip.h"

enum FileType {
    RegularFile, CompressedFile, StoredFile
};

struct FileRecord {
    std::string filename;
    FileType fileType;
    size_t size;
    
    // regular file
    std::string filePath;
    
    // zip
    std::string zipFilePath;
    unz_file_pos zipFilePos;
};

class ResourcesManagerImpl {
private:
    friend class ResourcesManager;
    
    std::vector<std::string> rootFoldersList;
    std::map<std::string, FileRecord> filenameToRecordMap;
    
    std::string getFilePath(const std::string& filename);
    void addFolderRecursive(const std::string& folder);
    size_t readDataFromRegularFile(const std::string& filePath, void* buffer, int size);
    size_t readDataFromCompressedFile(const FileRecord& fileRecord, void* buffer, int size);
    
    std::string getFilenameId(const std::string& filename);
};

std::string basename(const std::string& path) {
    std::string basename;
    
    size_t pos = path.find_last_of("/");
    if(pos != std::string::npos)
        basename.assign(path.begin() + pos + 1, path.end());
    else
        basename = path;
    
    return basename;
}

std::string removeExtension(const std::string& filename) {
    size_t lastdot = filename.find_last_of(".");
    if (lastdot == std::string::npos) return filename;
    return filename.substr(0, lastdot);
}

long getFileSize(const std::string& filePath)
{
    struct stat stat_buf;
    int rc = stat(filePath.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

ResourcesManager* ResourcesManager::sharedManager() {
    static ResourcesManager* manager = nullptr;
    
    if (!manager) {
        manager = new ResourcesManager();
    }
    
    return manager;
}

ResourcesManager::ResourcesManager()
{
    pImpl = std::unique_ptr<ResourcesManagerImpl>(new ResourcesManagerImpl());
}

void ResourcesManager::reset() {
    pImpl->rootFoldersList.clear();
    pImpl->filenameToRecordMap.clear();
}

void ResourcesManager::addRootFolder(const std::string& rootFolder) {
    pImpl->rootFoldersList.push_back(rootFolder);
    pImpl->addFolderRecursive(rootFolder);
}

void ResourcesManagerImpl::addFolderRecursive(const std::string& folder) {
    DIR *dp;
    struct dirent *ep;
    dp = opendir(folder.c_str());
    
    if (dp == NULL) return;
    while ((ep = readdir(dp))) {
        if (ep->d_name[0] == '.') continue;
        
        if (ep->d_type == DT_DIR) {
            addFolderRecursive(folder + "/" + ep->d_name);
        } else {
            FileRecord fileRecord;
            fileRecord.filename    = ep->d_name;
            fileRecord.fileType    = RegularFile;
            fileRecord.filePath    = folder + "/" + ep->d_name;
            fileRecord.size        = getFileSize(fileRecord.filePath);
            filenameToRecordMap[getFilenameId(fileRecord.filename)] = fileRecord;
        }
    }
    
    closedir(dp);
}

void ResourcesManager::addArchive(const std::string& archivePath) {
    unzFile zipFile = NULL;
    
    try {
        unzFile zipFile = unzOpen(archivePath.c_str());
        if (zipFile == NULL) throw std::exception();

        char filename[256] = {0};
        unz_file_info64 fileInfo;
        int ret = unzGoToFirstFile2(zipFile, &fileInfo, filename, sizeof(filename), NULL, 0, NULL, 0);
        if (ret != UNZ_OK) throw std::exception();
        
        do {
            unz_file_pos zipFilePos;
            ret = unzGetFilePos(zipFile, &zipFilePos);
            if (ret != UNZ_OK) throw std::exception();
            
            if (pImpl->filenameToRecordMap.count(filename) != 0) {
                throw std::exception();
            }
            
            FileRecord fileRecord;
            fileRecord.filename    = filename;
            fileRecord.fileType    = CompressedFile;
            fileRecord.size        = fileInfo.uncompressed_size;
            fileRecord.zipFilePath = archivePath;
            fileRecord.zipFilePos  = zipFilePos;
            pImpl->filenameToRecordMap[pImpl->getFilenameId(filename)] = fileRecord;
            
            ret = unzGoToNextFile2(zipFile, &fileInfo, filename, sizeof(filename), NULL, 0, NULL, 0);
            if (ret == UNZ_END_OF_LIST_OF_FILE) break;
            if (ret != UNZ_OK) throw std::exception();

        } while (ret != UNZ_END_OF_LIST_OF_FILE);
        
    } catch (std::exception& e) {
        if (zipFile)
            unzClose(zipFile);
        throw;
    }
}

//// should be rewritten to memory cache
//std::string ResourcesManagerImpl::getFilePath(const std::string& filename) {
//    for (auto& rootFolder : rootFoldersList) {
//        std::string filePath = rootFolder + "/" + filename;
//        
//        if (access(filePath.c_str(), F_OK) != -1 ) {
//            return filePath;
//        }
//    }
//    
//    return filename;
//}

std::string ResourcesManagerImpl::getFilenameId(const std::string& filename) {
    std::string filenameId = basename(filename);
    std::transform(filenameId.begin(), filenameId.end(), filenameId.begin(), ::tolower);
    filenameId = removeExtension(filenameId);
    
    return filenameId;
}

size_t ResourcesManager::readData(const std::string& filename, void* buffer, int size) {
    
    std::string filenameId = pImpl->getFilenameId(filename);
    
    auto it = pImpl->filenameToRecordMap.find(filenameId);
    if (it == pImpl->filenameToRecordMap.end()) return 0;
    
    if (it->second.fileType == RegularFile) {
        return pImpl->readDataFromRegularFile(it->second.filePath, buffer, size);
    }
    else if (it->second.fileType == CompressedFile) {
        return pImpl->readDataFromCompressedFile(it->second, buffer, size);
    }
    
    return 0;
}

size_t ResourcesManagerImpl::readDataFromRegularFile(const std::string& filePath, void* buffer, int size) {
    FILE* file = fopen(filePath.c_str(), "rb");
    if (!file) return 0;
    
    size_t bytesRead = fread(buffer, 1, size, file);
    
    fclose(file);
    
    return bytesRead;
}

size_t ResourcesManagerImpl::readDataFromCompressedFile(const FileRecord& fileRecord, void* buffer, int size) {
    
    unzFile zipFile = NULL;
    try {
        unzFile zipFile = unzOpen(fileRecord.zipFilePath.c_str());
        if (zipFile == NULL) throw std::exception();
        
        unz_file_pos file_pos = fileRecord.zipFilePos;
        int ret = unzGoToFilePos(zipFile, &file_pos);
        if (ret != UNZ_OK) throw std::exception();

        ret = unzOpenCurrentFile(zipFile);
        if (ret != UNZ_OK) throw std::exception();
        
        ret = unzReadCurrentFile(zipFile, buffer, size);
        if (ret < 0) throw std::exception();
        
        unzCloseCurrentFile(zipFile);
        
        return ret;
    } catch (std::exception& e) {
        
        if (zipFile) {
            unzCloseCurrentFile(zipFile);
            unzClose(zipFile);
        }
        throw;
    }

    return 0;
}

size_t ResourcesManager::getSize(const std::string& filename) {
    std::string filenameId = pImpl->getFilenameId(filename);
    
    auto it = pImpl->filenameToRecordMap.find(filenameId);
    if (it == pImpl->filenameToRecordMap.end()) return -1;
    
    return it->second.size;
}

std::unique_ptr<char[]> ResourcesManager::readData(const std::string& filename, size_t* bytesRead) {
    
    size_t size = getSize(filename);
    
    std::unique_ptr<char[]> buffer(new char[size]);
    *bytesRead = readData(filename, buffer.get(), size);
    if (*bytesRead != size) throw std::exception();
    
    return buffer;
}
