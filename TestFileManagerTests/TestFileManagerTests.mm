//
//  TestFileManagerTests.m
//  TestFileManagerTests
//
//  Created by Stanislav on 12.12.13.
//  Copyright (c) 2013 Redsteep. All rights reserved.
//

#import "TestFileManagerTests.h"

#include "ResourcesManager.h"

@implementation TestFileManagerTests

- (void)setUp
{
    [super setUp];
    
    // Set-up code here.
}

- (void)tearDown
{
    // Tear-down code here.
    ResourcesManager::sharedManager()->reset();
    
    [super tearDown];
}

//- (void)testFileExists
//{
//    ResourcesManager::sharedManager()->addRootFolder([[[NSBundle mainBundle] resourcePath] UTF8String]);
//    
//    
//    STAssertTrue(ResourcesManager::sharedManager()->exists("test.txt"), @"");
//    STAssertFalse(ResourcesManager::sharedManager()->exists("non-exising-filename"), @"");
//}

- (void)testReadFile
{
    ResourcesManager::sharedManager()->addRootFolder([[[NSBundle mainBundle] resourcePath] UTF8String]);
    
    char Buffer[5];
    memset(Buffer, 0, sizeof(Buffer));
    int bytesRead = ResourcesManager::sharedManager()->readData("test.txt", &Buffer, sizeof(Buffer));
    STAssertEquals(bytesRead, 4, @"");
    STAssertEqualObjects(@(Buffer), @"test", @"");
}

- (void)testReadCompressedFileInZip
{
    ResourcesManager::sharedManager()->addArchive([[[NSBundle mainBundle] pathForResource:@"archive1" ofType:@"zip"] UTF8String]);
    
    char Buffer[5] = {0};
    int bytesRead = ResourcesManager::sharedManager()->readData("test_compressed.txt", &Buffer, sizeof(Buffer));
    STAssertEquals(bytesRead, 4, @"");
    STAssertEqualObjects(@(Buffer), @"test", @"");
}

- (void)testReadFileInFolder
{
    ResourcesManager::sharedManager()->addRootFolder([[[NSBundle mainBundle] resourcePath] UTF8String]);
    
    char Buffer[100] = {0};
    ResourcesManager::sharedManager()->readData("res/file_in_folder.txt", &Buffer, sizeof(Buffer));
    STAssertEqualObjects(@(Buffer), @"file_in_folder", @"");

    int size = ResourcesManager::sharedManager()->readData("file_in_folder.txt", &Buffer, sizeof(Buffer));
    STAssertTrue(size > 0, @"");
}

- (void)testReadFileInZipFolder
{
    ResourcesManager::sharedManager()->addArchive([[[NSBundle mainBundle] pathForResource:@"archive1" ofType:@"zip"] UTF8String]);
    
    char Buffer[100] = {0};
    ResourcesManager::sharedManager()->readData("res/compressed_file_in_folder.txt", &Buffer, sizeof(Buffer));
    STAssertEqualObjects(@(Buffer), @"compressed_file_in_folder", @"");

    int size = ResourcesManager::sharedManager()->readData("compressed_file_in_folder.txt", &Buffer, sizeof(Buffer));
    STAssertTrue(size > 0, @"");
}

- (void)testReadFileToBuffer
{
    ResourcesManager::sharedManager()->addRootFolder([[[NSBundle mainBundle] resourcePath] UTF8String]);
    
    size_t bytesRead = 0;
    auto Buffer = ResourcesManager::sharedManager()->readData("test.txt", &bytesRead);
    STAssertEquals(bytesRead, (size_t)4, @"");
    NSString *string = [[NSString alloc] initWithBytes:Buffer.get() length:4 encoding:NSUTF8StringEncoding];
    STAssertEqualObjects(string, @"test", @"");
}

//- (void)testReadStoredFileInZip
//{
//    ResourcesManager::sharedManager()->addArchive([[[NSBundle mainBundle] pathForResource:@"archive1" ofType:@"zip"] UTF8String])
//    
//    char Buffer[4];
//    int bytesRead = ResourcesManager::sharedManager()->readData("test_stored.txt", &Buffer, sizeof(Buffer));
//    STAssertEquals(bytesRead, 4, @"");
//    STAssertEquals(Buffer, "test", @"");
//}
//
//- (void)testReadStoredStreamInZip
//{
//    ResourcesManager::sharedManager()->addArchive([[[NSBundle mainBundle] pathForResource:@"archive1" ofType:@"zip"] UTF8String])
//    
//    Stream stream = ResourcesManager::sharedManager()->getStream("test_stored.txt");
//    STAssertEquals(stream.getSize(), 4, @"");
//    char Buffer[2];
//    int bytesRead = stream.readData(&Buffer, sizeof(Buffer));
//    STAssertEquals(bytesRead, 2, @"");
//    STAssertEquals(Buffer, "te", @"");
//    
//    bytesRead = stream.readData(&Buffer, sizeof(Buffer));
//    STAssertEquals(bytesRead, 2, @"");
//    STAssertEquals(Buffer, "st", @"");
//}
//
//- (void)testReadSmallScreenFileFromFolder
//{
//    ResourcesManager::sharedManager()->addRootFolder([[[NSBundle mainBundle] resourcePath] UTF8String]);
//    
//    ResourcesManager::sharedManager()->setTagFolder("screen-small", "small");
//    ResourcesManager::sharedManager()->activateTag("small");
//    
//    char Buffer[100];
//    int bytesRead = ResourcesManager::sharedManager()->readData("test.txt", &Buffer, sizeof(Buffer));
//    STAssertEquals(bytesRead, strlen("small-screen test"), @"");
//    STAssertEquals(Buffer, "small-screen test", @"");
//}
//
//- (void)testReadSmallScreenFileWithSuffix
//{
//    ResourcesManager::sharedManager()->addRootFolder([[[NSBundle mainBundle] resourcePath] UTF8String]);
//    
//    ResourcesManager::sharedManager()->setTagSuffix("small", "small");
//    ResourcesManager::sharedManager()->activateTag("small");
//    
//    char Buffer[100];
//    int bytesRead = ResourcesManager::sharedManager()->readData("test.txt", &Buffer, sizeof(Buffer));
//    STAssertEquals(bytesRead, strlen("small-screen test"), @"");
//    STAssertEquals(Buffer, "small-screen test", @"");
//}

@end
