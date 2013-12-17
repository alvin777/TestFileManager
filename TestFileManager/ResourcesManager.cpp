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
#include <set>
#include <map>
#include <sstream>

#include "unzip.h"

enum FileType {
    RegularFile, CompressedFile, StoredFile
};

struct FileRecord {
    std::string filename;     // Demo.png (case as on disk)
    FileType fileType;
    size_t size;
    std::string languageId;
    std::string category;
    
    // regular file
    std::string filePath;     // /Users/user/../<AppId>/res/Textures/Demo.png
    std::string relativePath; // res/textures/Demo.png
    
    // zip
    std::string zipFilePath;
    unz_file_pos zipFilePos;
};

struct StreamRecord {
    FileRecord* fileRecord;
    
    // regular file
    FILE* file;
    
    // zip
    unzFile zipFile;
    
    bool operator < (const StreamRecord& other) const {
        return fileRecord < other.fileRecord;
    }
};

class ResourcesManagerImpl {
private:
    friend class ResourcesManager;
    
    typedef std::vector<FileRecord> FileRecordList;
    
    std::vector<std::string> rootFoldersList;
    std::map<std::string, FileRecordList> filenameToRecordMap;
    std::string languageId;
    std::map<std::string, std::string> relativeFolderToLanguageIdMap;
    std::map<std::string, std::string> relativeFolderToCategoryMap;
    std::set<std::string> enabledCategories;
    
    std::set<StreamRecord> openStreams;
    bool searchByRelativePaths = false;
    std::vector<std::string> searchRootsList = {""};
    
    void addFolderRecursive(const std::string& folder, const std::string& relativeFolder, const std::string& languageId, const std::string& category);
    
    size_t readData(const FileRecord& fileRecord, void* buffer, int size);
    size_t readDataFromRegularFile(const std::string& filePath, void* buffer, int size);
    size_t readDataFromCompressedFile(const FileRecord& fileRecord, void* buffer, int size);
    
    std::string makeKey(const std::string& filename);
    
    FileRecord* findFileRecord(const std::string& filename);
};

//
// utility functions
//

static std::string basename(const std::string& path) {
    std::string basename;
    
    size_t pos = path.find_last_of("/\\");
    if(pos != std::string::npos)
        basename.assign(path.begin() + pos + 1, path.end());
    else
        basename = path;
    
    return basename;
}

static std::string combine(const std::initializer_list<std::string>& pathComponentsList) {
    std::stringstream out;
    for (auto& pathComponent : pathComponentsList) {
        if (!pathComponent.empty())
            out << pathComponent << "/";
    }
    
    std::string outString = out.str();
    
    if (outString.size() == 0) return outString;
    
    return outString.substr(0, outString.size() - 1);
}

std::string removeExtension(const std::string& filename) {
    size_t lastdot = filename.find_last_of(".");
    if (lastdot == std::string::npos) return filename;
    return filename.substr(0, lastdot);
}

static std::string getRootFolder(const std::string& filePath) {
    size_t firstSlash = filePath.find_first_of("/\\");
    if (firstSlash == std::string::npos) return filePath;
    
    // TODO: support paths starting with slash
    
    return filePath.substr(0, firstSlash);
}

static long getFileSize(const std::string& filePath)
{
    struct stat stat_buf;
    int rc = stat(filePath.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

//
// ResourcesManager
//

ResourcesManager* ResourcesManager::sharedManager() {
    static ResourcesManager* manager = nullptr;
    
    if (!manager) {
        manager = new ResourcesManager();
    }
    
    return manager;
}

ResourcesManager::ResourcesManager() :
    pImpl(new ResourcesManagerImpl())
{
}

//
// configuration methods
//

void ResourcesManager::reset() {
    pImpl->rootFoldersList.clear();
    pImpl->filenameToRecordMap.clear();
    pImpl->languageId.clear();
    pImpl->relativeFolderToLanguageIdMap.clear();
    pImpl->relativeFolderToCategoryMap.clear();
    pImpl->enabledCategories.clear();
    pImpl->searchByRelativePaths = false;
    pImpl->searchRootsList = {""};
}

void ResourcesManager::addRootFolder(const std::string& rootFolder) {
    pImpl->rootFoldersList.push_back(rootFolder);
    pImpl->addFolderRecursive(rootFolder, "", "", "");
}

void ResourcesManager::addLanguageFolder(const std::string& languageId, const std::string& languageFolder) {
    pImpl->relativeFolderToLanguageIdMap[languageFolder] = languageId;
}

void ResourcesManager::setCurrentLanguage(const std::string& languageId) {
    pImpl->languageId = languageId;
}

void ResourcesManager::addCategoryFolder(const std::string& category, const std::string& categoryFolder) {
    pImpl->relativeFolderToCategoryMap[categoryFolder] = category;
    
}
void ResourcesManager::enableCategory(const std::string& category){
    pImpl->enabledCategories.insert(category);
}
void ResourcesManager::disableCategory(const std::string& category) {
    pImpl->enabledCategories.erase(category);
}

void ResourcesManager::setSearchByRelativePaths(bool searchByRelativePaths) {
    pImpl->searchByRelativePaths = searchByRelativePaths;
}

void ResourcesManager::addSearchRoot(const std::string& searchRoot) {
    pImpl->searchRootsList.push_back(searchRoot);
}


//
// filesystem methods
//

void ResourcesManagerImpl::addFolderRecursive(const std::string& rootFolder, const std::string& relativeFolder, const std::string& languageId, const std::string& category) {
    
    DIR *dp = opendir(combine({rootFolder, relativeFolder}).c_str());
    if (!dp) return;
    
    struct dirent *ep;
    while ((ep = readdir(dp))) {
        if (ep->d_name[0] == '.') continue;
        
        if (ep->d_type == DT_DIR) {
            std::string newRelativeFolder = combine({relativeFolder, ep->d_name});
            
            std::string newLanguageId;
            auto it = relativeFolderToLanguageIdMap.find(newRelativeFolder);
            if (it != relativeFolderToLanguageIdMap.end()) {
                newLanguageId = it->second;
            } else {
                newLanguageId = languageId;
            }

            std::string newCategory;
            it = relativeFolderToCategoryMap.find(newRelativeFolder);
            if (it != relativeFolderToCategoryMap.end()) {
                newCategory = it->second;
            } else {
                newCategory = category;
            }

            addFolderRecursive(rootFolder, newRelativeFolder, newLanguageId, newCategory);
        } else {
            FileRecord fileRecord;
            fileRecord.filename    = ep->d_name;
            fileRecord.fileType    = RegularFile;
            fileRecord.relativePath= combine({relativeFolder, ep->d_name});
            fileRecord.filePath    = combine({rootFolder, fileRecord.relativePath});
            fileRecord.size        = getFileSize(fileRecord.filePath);
            fileRecord.languageId  = languageId;
            fileRecord.category    = category;
            
            std::string key = searchByRelativePaths ? fileRecord.relativePath : fileRecord.filename;
            
            filenameToRecordMap[makeKey(key)].push_back(fileRecord);
        }
    }
    
    closedir(dp);
}

size_t ResourcesManagerImpl::readDataFromRegularFile(const std::string& filePath, void* buffer, int size) {
    FILE* file = fopen(filePath.c_str(), "rb");
    if (!file) return 0;
    
    size_t bytesRead = fread(buffer, 1, size, file);
    
    fclose(file);
    
    return bytesRead;
}

//
// zip archive methods
//

void ResourcesManager::addArchive(const std::string& archivePath, const std::string& rootFolder /* = "" */) {
    unzFile zipFile = nullptr;
    
    try {
        unzFile zipFile = unzOpen(archivePath.c_str());
        if (!zipFile) throw std::exception();

        char filePath[1024] = {0};
        unz_file_info64 fileInfo;
        int ret = unzGoToFirstFile2(zipFile, &fileInfo, filePath, sizeof(filePath), NULL, 0, NULL, 0);
        if (ret != UNZ_OK) throw std::exception();
        
        do {
            unz_file_pos zipFilePos;
            ret = unzGetFilePos(zipFile, &zipFilePos);
            if (ret != UNZ_OK) throw std::exception();
            
            if (pImpl->filenameToRecordMap.count(filePath) != 0) {
                throw std::exception();
            }
            
            // skip folders and files outside specified folder
            bool shouldAddRecord = true;
            std::string filePathString = filePath;
            std::string slashEndedRootFolder = rootFolder + '/';
            if (filePathString[filePathString.size()-1] == '/' ||
                (!rootFolder.empty() &&
                 filePathString.compare(0, slashEndedRootFolder.size(), slashEndedRootFolder) != 0)) {
                shouldAddRecord = false; 
            }
            
            if (shouldAddRecord) {
                
                std::string rootFolderRelativePath = filePathString;
                if (!rootFolder.empty()) {
                    rootFolderRelativePath = rootFolderRelativePath.substr(slashEndedRootFolder.size(), rootFolderRelativePath.size() - slashEndedRootFolder.size());
                }
                
                FileRecord fileRecord;
                fileRecord.filename    = filePathString;
                fileRecord.relativePath= rootFolderRelativePath;
                fileRecord.fileType    = CompressedFile;
                fileRecord.size        = fileInfo.uncompressed_size;
                fileRecord.zipFilePath = archivePath;
                fileRecord.zipFilePos  = zipFilePos;
                
                // TODO: improve check by extracting required path components from file path
                for (auto& folderLanguagePair : pImpl->relativeFolderToLanguageIdMap) {
                    std::string pathPrefix = combine({rootFolder, folderLanguagePair.first});
                    if (filePathString.compare(0, pathPrefix.size(), pathPrefix) == 0) {
                        fileRecord.languageId = folderLanguagePair.second;
                    }
                }

                for (auto& folderCategoryPair : pImpl->relativeFolderToCategoryMap) {
                    std::string pathPrefix = combine({rootFolder, folderCategoryPair.first});
                    if (filePathString.compare(0, pathPrefix.size(), pathPrefix) == 0) {
                        fileRecord.category = folderCategoryPair.second;
                    }
                }
                
                std::string key = pImpl->searchByRelativePaths ? fileRecord.relativePath : filePath;

                pImpl->filenameToRecordMap[pImpl->makeKey(key)].push_back(fileRecord);
            }
            
            ret = unzGoToNextFile2(zipFile, &fileInfo, filePath, sizeof(filePath), NULL, 0, NULL, 0);
            if (ret == UNZ_END_OF_LIST_OF_FILE) break;
            if (ret != UNZ_OK) throw std::exception();

        } while (ret != UNZ_END_OF_LIST_OF_FILE);
        
    } catch (std::exception& e) {
        if (zipFile)
            unzClose(zipFile);
        throw;
    }
}

size_t ResourcesManagerImpl::readDataFromCompressedFile(const FileRecord& fileRecord, void* buffer, int size) {
    
    unzFile zipFile = nullptr;
    try {
        unzFile zipFile = unzOpen(fileRecord.zipFilePath.c_str());
        if (!zipFile) throw std::exception();
        
        unz_file_pos file_pos = fileRecord.zipFilePos;
        int ret = unzGoToFilePos(zipFile, &file_pos);
        if (ret != UNZ_OK) throw std::exception();
        
        ret = unzOpenCurrentFile(zipFile);
        if (ret != UNZ_OK) throw std::exception();
        
        ret = unzReadCurrentFile(zipFile, buffer, size);
        if (ret < 0) throw std::exception();
        
        unzCloseCurrentFile(zipFile);
        unzClose(zipFile);

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

//
// common methods
//

std::string ResourcesManagerImpl::makeKey(const std::string& filename) {
    std::string key = searchByRelativePaths ? filename :  basename(filename);
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
//    filenameId = removeExtension(filenameId);
    
    return key;
}

FileRecord* ResourcesManagerImpl::findFileRecord(const std::string& filename) {
    std::string key = makeKey(filename);
    
    std::set<FileRecord*, std::function<bool(FileRecord*, FileRecord*)>>
        candidatesList([] (FileRecord* rec1, FileRecord* rec2) {
            return rec1->category   > rec2->category ||
                   rec1->languageId > rec2->languageId;
    });

    for (auto& searchRoot : searchRootsList) {
        auto it = filenameToRecordMap.find(combine({searchRoot, key}));
        if (it == filenameToRecordMap.end()) continue;

        // empty array (?)
        if (it->second.size() == 0) continue;

        // if language is not set - return non-specific file record
        // if language is set return language-specific record if exists and non-specific otherwise
        
        for (auto& fileRecord : it->second) {
            if ((languageId.empty() || fileRecord.languageId == languageId) &&
                (fileRecord.category.empty() || enabledCategories.count(fileRecord.category) > 0))
            {
                candidatesList.insert(&fileRecord);
            }
        }
    }
    
    if (candidatesList.empty()) return nullptr;
    
    return *candidatesList.begin();
}

size_t ResourcesManagerImpl::readData(const FileRecord& fileRecord, void* buffer, int size) {
    if (fileRecord.fileType == RegularFile) {
        return readDataFromRegularFile(fileRecord.filePath, buffer, size);
    }
    else if (fileRecord.fileType == CompressedFile) {
        return readDataFromCompressedFile(fileRecord, buffer, size);
    }

    return 0;
}

size_t ResourcesManager::readData(const std::string& filename, void* buffer, int size) {
    
    FileRecord* fileRecord = pImpl->findFileRecord(filename);
    if (!fileRecord) return 0;
    
    return pImpl->readData(*fileRecord, buffer, size);
}

std::unique_ptr<char[]> ResourcesManager::readData(const std::string& filename, size_t* bytesRead) {
    
    FileRecord* fileRecord = pImpl->findFileRecord(filename);
    if (!fileRecord) {
        *bytesRead = 0;
        return nullptr;
    }
    
    std::unique_ptr<char[]> buffer(new char[fileRecord->size]);
    *bytesRead = pImpl->readData(*fileRecord, buffer.get(), fileRecord->size);
    if (*bytesRead != fileRecord->size) throw std::exception();
    
    return buffer;
}

size_t ResourcesManager::getSize(const std::string& filename) {
    FileRecord* fileRecord = pImpl->findFileRecord(filename);
    if (!fileRecord) return 0;

    return fileRecord->size;
}

std::unique_ptr<Stream> ResourcesManager::getStream(const std::string& filename) {
    
    FileRecord* fileRecord = pImpl->findFileRecord(filename);
    if (!fileRecord) return nullptr;
    
    StreamRecord streamRecord;
    streamRecord.fileRecord = fileRecord;
    
    switch (fileRecord->fileType) {
        case RegularFile:
            streamRecord.file = fopen(fileRecord->filePath.c_str(), "rb");
            if (!streamRecord.file) return nullptr;
            break;
            
        case CompressedFile:
        case StoredFile:
        {
            streamRecord.zipFile = unzOpen(fileRecord->zipFilePath.c_str());
            if (!streamRecord.zipFile) throw std::exception();
            
            int ret = unzGoToFilePos(streamRecord.zipFile, &fileRecord->zipFilePos);
            if (ret != UNZ_OK) throw std::exception();

            ret = unzOpenCurrentFile(streamRecord.zipFile);
            if (ret != UNZ_OK) throw std::exception();
            break;
        }
    }
    
    auto returnPair = pImpl->openStreams.insert(streamRecord);
    
    return std::unique_ptr<Stream>(new Stream(reinterpret_cast<int>(&*returnPair.first)));
}

size_t ResourcesManager::readData(int handle, void* buffer, int size) {
    
    StreamRecord* streamRecord = reinterpret_cast<StreamRecord*>(handle);
    
    if (pImpl->openStreams.count(*streamRecord) == 0) return 0;
    
    int ret;
    switch (streamRecord->fileRecord->fileType) {
        case RegularFile:
            ret = fread(buffer, 1, size, streamRecord->file);
            break;
            
        case CompressedFile:
        case StoredFile:
            ret = unzReadCurrentFile(streamRecord->zipFile, buffer, size);
            break;
    }
    
    return ret;
}

int ResourcesManager::closeFile(int handle) {
    StreamRecord* streamRecord = reinterpret_cast<StreamRecord*>(handle);
    
    if (pImpl->openStreams.count(*streamRecord) == 0) return 0;
    
    int ret = 0;
    
    switch (streamRecord->fileRecord->fileType) {
        case RegularFile:
            ret = fclose(streamRecord->file);
            break;
            
        case CompressedFile:
        case StoredFile: {
            unzCloseCurrentFile(streamRecord->zipFile);
            unzClose(streamRecord->zipFile);
            break;
        }
    }
    
    pImpl->openStreams.erase(*streamRecord);
    
    return ret;
}


//
// Stream
//

class StreamImpl {
private:
    friend class Stream;
    
    int handle = -1;
};

Stream::Stream(int handle) : pImpl(new StreamImpl()) {
    pImpl->handle = handle;
}

Stream::~Stream() {
    ResourcesManager::sharedManager()->closeFile(pImpl->handle);
}

size_t Stream::readData(void* buffer, int size) {
    return ResourcesManager::sharedManager()->readData(pImpl->handle, buffer, size);
}

std::unique_ptr<char[]> Stream::readData(size_t* bytesRead) {
    return nullptr;
}
