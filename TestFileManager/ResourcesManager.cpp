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
#include <iostream>

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
    int randomValue;
    
    // regular file
    FILE* file;
    
    // zip
    unzFile zipFile;
    
    bool operator < (const StreamRecord& other) const {
        return randomValue < other.randomValue;
    }
};

class ResourcesManagerImpl {
private:
    friend class ResourcesManager;
    
    typedef std::vector<FileRecord> FileRecordList;
    
    bool enableTrace;
    
    std::vector<std::string> rootFoldersList;
    
    FileRecordList fileRecordList;
    std::map<std::string, FileRecord*> fileRecordIndex;
    
    bool shouldRebuildIndex;
    std::string languageId;
    std::map<std::string, std::string> relativeFolderToLanguageIdMap;
    std::map<std::string, std::string> relativeFolderToCategoryMap;
    std::set<std::string> enabledCategories;
    
    std::map<int, StreamRecord> openStreams;
    bool searchByRelativePaths;
    std::vector<std::string> searchRootsList;
    
    // methods    
    void addFolderRecursive(const std::string& folder, const std::string& relativeFolder);
    
    size_t readData(const FileRecord& fileRecord, void* buffer, int size);
    size_t readDataFromRegularFile(const std::string& filePath, void* buffer, int size);
    size_t readDataFromCompressedFile(const FileRecord& fileRecord, void* buffer, int size);
    
    std::string makeKey(const std::string& filename);
    
    void rebuildIndex();
    FileRecord* findFileRecord(const std::string& filename);
    StreamRecord* getStreamRecord(int handle);
    
    void traceFileRecord(const std::string& key, const FileRecord& fileRecord);
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

static void lowercase(std::string& string) {
    std::transform(string.begin(), string.end(), string.begin(), ::tolower);
}

static void replaceAll( std::string &s, const std::string &search, const std::string &replace ) {
    for( size_t pos = 0; ; pos += replace.length() ) {
        // Locate the substring to replace
        pos = s.find( search, pos );
        if( pos == std::string::npos ) break;
        // Replace by erasing and inserting
        s.erase( pos, search.length() );
        s.insert( pos, replace );
    }
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
    reset();
}

//
// configuration methods
//

void ResourcesManager::reset() {
    pImpl->enableTrace = false;
    pImpl->shouldRebuildIndex = false;
    pImpl->rootFoldersList.clear();
    pImpl->fileRecordList.clear();
    pImpl->fileRecordIndex.clear();
    pImpl->languageId.clear();
    pImpl->relativeFolderToLanguageIdMap.clear();
    pImpl->relativeFolderToCategoryMap.clear();
    pImpl->enabledCategories.clear();
    pImpl->searchByRelativePaths = false;
    pImpl->searchRootsList = {""};
}

void ResourcesManager::enableTrace(bool enableTrace) {
    pImpl->enableTrace = enableTrace;
}

void ResourcesManager::addRootFolder(const std::string& rootFolder) {
    pImpl->rootFoldersList.push_back(rootFolder);
    pImpl->addFolderRecursive(rootFolder, "");
}

void ResourcesManager::addLanguageFolder(const std::string& languageId, const std::string& languageFolder) {
    pImpl->relativeFolderToLanguageIdMap[languageFolder] = languageId;
    
    pImpl->shouldRebuildIndex = true;
}

void ResourcesManager::setCurrentLanguage(const std::string& languageId) {
    pImpl->languageId = languageId;
    
    pImpl->shouldRebuildIndex = true;
}

void ResourcesManager::addCategoryFolder(const std::string& category, const std::string& categoryFolder) {
    pImpl->relativeFolderToCategoryMap[categoryFolder] = category;
    
    pImpl->shouldRebuildIndex = true;
}
void ResourcesManager::enableCategory(const std::string& category){
    pImpl->enabledCategories.insert(category);

    pImpl->shouldRebuildIndex = true;
}
void ResourcesManager::disableCategory(const std::string& category) {
    pImpl->enabledCategories.erase(category);

    pImpl->shouldRebuildIndex = true;
}

void ResourcesManager::setSearchByRelativePaths(bool searchByRelativePaths) {
    if (searchByRelativePaths != pImpl->searchByRelativePaths) {
        pImpl->searchByRelativePaths = searchByRelativePaths;

        pImpl->shouldRebuildIndex = true;
    }
}

void ResourcesManager::addSearchRoot(const std::string& searchRoot) {
    std::string canonicalSearchRoot = searchRoot;
    replaceAll(canonicalSearchRoot, "\\\\", "/");
    pImpl->searchRootsList.push_back(canonicalSearchRoot);

    pImpl->shouldRebuildIndex = true;
}


//
// filesystem methods
//

void ResourcesManagerImpl::addFolderRecursive(const std::string& rootFolder, const std::string& relativeFolder) {
    
    DIR *dp = opendir(combine({rootFolder, relativeFolder}).c_str());
    if (!dp) return;
    
    struct dirent *ep;
    while ((ep = readdir(dp))) {
        if (ep->d_name[0] == '.') continue;
        
        if (ep->d_type == DT_DIR) {
            std::string newRelativeFolder = combine({relativeFolder, ep->d_name});
            addFolderRecursive(rootFolder, newRelativeFolder);
        } else {
            FileRecord fileRecord;
            fileRecord.filename    = ep->d_name;
            fileRecord.fileType    = RegularFile;
            fileRecord.relativePath= combine({relativeFolder, ep->d_name});
            fileRecord.filePath    = combine({rootFolder, fileRecord.relativePath});
            fileRecord.size        = getFileSize(fileRecord.filePath);
            
            fileRecordList.push_back(fileRecord);

            shouldRebuildIndex = true;
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
            
//            if (pImpl->filenameToRecordMap.count(filePath) != 0) {
//                throw std::exception();
//            }
            
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
                pImpl->fileRecordList.push_back(fileRecord);

                pImpl->shouldRebuildIndex = true;
                
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

void ResourcesManagerImpl::traceFileRecord(const std::string& key, const FileRecord& fileRecord) {
    
    std::cout << key << ": ";
    
    if (!fileRecord.zipFilePath.empty())
        std::cout << "zip: " << basename(fileRecord.zipFilePath) << ", ";
    
    std::cout << "relative path: " << fileRecord.relativePath << ", ";
        
    if (!fileRecord.category.empty())
        std::cout << "category: " << fileRecord.category << ", ";
    
    std::cout << "size: " << fileRecord.size << std::endl;
}

std::string ResourcesManagerImpl::makeKey(const std::string& filename) {
    std::string key = searchByRelativePaths ? filename :  basename(filename);
    lowercase(key);
    replaceAll(key, "\\\\", "/");
    std::replace(key.begin(), key.end(), '\\', '/');
//    filenameId = removeExtension(filenameId);
    
    return key;
}

void ResourcesManagerImpl::rebuildIndex() {
    fileRecordIndex.clear();
    
    for (auto& fileRecord : fileRecordList) {
        bool skipRecord = false;
        std::string relativePathInMap = fileRecord.relativePath;
        lowercase(relativePathInMap);

        for (auto& folderLanguageIdPair :  relativeFolderToLanguageIdMap) {
            std::string pathComponentToSearch = folderLanguageIdPair.first + "/";
            if (relativePathInMap.find(pathComponentToSearch) != std::string::npos)
            {
                if (languageId != folderLanguageIdPair.second) {
                    skipRecord = true;
                    break;
                }
                
                fileRecord.languageId = folderLanguageIdPair.second;
                replaceAll(relativePathInMap, pathComponentToSearch, "");
            }
        }

        if (skipRecord) continue;


        for (auto& folderCategoryPair :  relativeFolderToCategoryMap) {
            std::string pathComponentToSearch = folderCategoryPair.first + "/";
            lowercase(pathComponentToSearch);

            if (relativePathInMap.find(pathComponentToSearch) != std::string::npos)
            {
                if (enabledCategories.count(folderCategoryPair.second) == 0) {
                    skipRecord = true;
                    break;
                }
                
                fileRecord.category = folderCategoryPair.second;
                replaceAll(relativePathInMap, pathComponentToSearch, "");
            }
        }
        
        if (skipRecord) continue;


        std::string key = makeKey(relativePathInMap);
        fileRecordIndex[key] = &fileRecord;

        if (enableTrace)
            traceFileRecord(key, fileRecord);

        
        for (auto& searchRoot : searchRootsList) {
            if (searchRoot.empty()) continue;
            
            std::string pathComponentToSearch = searchRoot + "/";
            lowercase(pathComponentToSearch);
            replaceAll(pathComponentToSearch, "\\\\", "/");
            
            if (relativePathInMap.compare(0, pathComponentToSearch.size(), pathComponentToSearch) == 0) {
                
                std::string searchRootRelativePath = relativePathInMap.substr(pathComponentToSearch.size());
                
                std::string key = makeKey(searchRootRelativePath);
                fileRecordIndex[key] = &fileRecord;
                
                if (enableTrace)
                    traceFileRecord(key, fileRecord);
            }
        }
    }
    
    shouldRebuildIndex = false;
}

void ResourcesManager::rebuildIndex() {
    pImpl->rebuildIndex();
}

FileRecord* ResourcesManagerImpl::findFileRecord(const std::string& filename) {
    
    if (shouldRebuildIndex) {
        rebuildIndex();
    }
    
    std::string key = makeKey(filename);
    
    auto it = fileRecordIndex.find(key);
    if (it == fileRecordIndex.end()) {
        return nullptr;
    }
    
    return it->second;
}

bool ResourcesManager::exists(const std::string& filename) {
    return (pImpl->findFileRecord(filename) != nullptr);
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

std::unique_ptr<char[]> ResourcesManager::readData(const std::string& filename, size_t* pBytesRead) {
    
    FileRecord* fileRecord = pImpl->findFileRecord(filename);
    if (!fileRecord) {
        if (pBytesRead)
            *pBytesRead = 0;
        return nullptr;
    }
    
    std::unique_ptr<char[]> buffer(new char[fileRecord->size]);
    size_t bytesRead = pImpl->readData(*fileRecord, buffer.get(), fileRecord->size);
    if (bytesRead != fileRecord->size) throw std::exception();

    if (pBytesRead)
        *pBytesRead = bytesRead;
    
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
    streamRecord.randomValue = arc4random();
    
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
    
    auto insertResult = pImpl->openStreams.insert(std::make_pair(streamRecord.randomValue, streamRecord));
    
    if (!insertResult.second) {
        throw std::exception();
    }
    
    return std::unique_ptr<Stream>(new Stream(reinterpret_cast<int>(streamRecord.randomValue)));
}

StreamRecord* ResourcesManagerImpl::getStreamRecord(int handle) {
    auto it = openStreams.find(handle);
    if (it == openStreams.end()) return nullptr;
    
    return &it->second;
}

size_t ResourcesManager::readData(int handle, void* buffer, int size) {
    StreamRecord* streamRecord = pImpl->getStreamRecord(handle);
    if (!streamRecord) return 0;
    
    int ret = 0;
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
    StreamRecord* streamRecord = pImpl->getStreamRecord(handle);
    if (!streamRecord) return 0;
    
    int ret = 0;
    
    switch (streamRecord->fileRecord->fileType) {
        case RegularFile:
            if (!streamRecord->file) {
                break;
            }

            ret = fclose(streamRecord->file);
            streamRecord->file = NULL;
            break;
            
        case CompressedFile:
        case StoredFile: {
            if (!streamRecord->zipFile) {
                break;
            }
                
            unzCloseCurrentFile(streamRecord->zipFile);
            unzClose(streamRecord->zipFile);
            streamRecord->zipFile = NULL;
            break;
        }
    }
    
    pImpl->openStreams.erase(streamRecord->randomValue);
    
    return ret;
}

int ResourcesManager::seek (int handle, long int offset, int whence) {
    StreamRecord* streamRecord = pImpl->getStreamRecord(handle);
    if (!streamRecord) return 0;

    int ret = 0;
    
    switch (streamRecord->fileRecord->fileType) {
        case RegularFile:
            ret = fseek(streamRecord->file, offset, whence);
            break;
            
        case CompressedFile:
        case StoredFile: {
            throw std::exception();
        }
    }
    
    return ret;
}

long int ResourcesManager::tell(int handle) {
    StreamRecord* streamRecord = pImpl->getStreamRecord(handle);
    if (!streamRecord) return 0;
    
    int ret = 0;
    
    switch (streamRecord->fileRecord->fileType) {
        case RegularFile:
            ret = ftell(streamRecord->file);
            break;
            
        case CompressedFile:
        case StoredFile: {
            throw std::exception();
        }
    }
    
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

int Stream::seek (long int offset, int whence) {
    return ResourcesManager::sharedManager()->seek(pImpl->handle, offset, whence);
}

long int Stream::tell() {
    return ResourcesManager::sharedManager()->tell(pImpl->handle);
}

std::unique_ptr<char[]> Stream::readData(size_t* bytesRead) {
    return nullptr;
}
