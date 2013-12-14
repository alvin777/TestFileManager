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
    
    char buffer[5];
    memset(buffer, 0, sizeof(buffer));
    int bytesRead = ResourcesManager::sharedManager()->readData("test.txt", &buffer, sizeof(buffer));
    STAssertEquals(bytesRead, 4, @"");
    STAssertEqualObjects(@(buffer), @"test", @"");
}

- (void)testReadCompressedFileInZip
{
    ResourcesManager::sharedManager()->addArchive([[[NSBundle mainBundle] pathForResource:@"archive1" ofType:@"zip"] UTF8String]);
    
    char buffer[5] = {0};
    int bytesRead = ResourcesManager::sharedManager()->readData("test_compressed.txt", &buffer, sizeof(buffer));
    STAssertEquals(bytesRead, 4, @"");
    STAssertEqualObjects(@(buffer), @"test", @"");
}

- (void)testReadFileInFolder
{
    ResourcesManager::sharedManager()->addRootFolder([[[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"res"] UTF8String]);
    
    char buffer[100] = {0};
    ResourcesManager::sharedManager()->readData("res/file_in_folder.txt", &buffer, sizeof(buffer));
    STAssertEqualObjects(@(buffer), @"file_in_folder", @"");

    int size = ResourcesManager::sharedManager()->readData("file_in_folder.txt", &buffer, sizeof(buffer));
    STAssertTrue(size > 0, @"");
}

- (void)testReadFileInZipFolder
{
    ResourcesManager::sharedManager()->addArchive([[[NSBundle mainBundle] pathForResource:@"archive1" ofType:@"zip"] UTF8String]);
    
    char buffer[100] = {0};
    ResourcesManager::sharedManager()->readData("res/compressed_file_in_folder.txt", &buffer, sizeof(buffer));
    STAssertEqualObjects(@(buffer), @"compressed_file_in_folder", @"");

    int size = ResourcesManager::sharedManager()->readData("compressed_file_in_folder.txt", &buffer, sizeof(buffer));
    STAssertTrue(size > 0, @"");
}

- (void)testReadFileTobuffer
{
    ResourcesManager::sharedManager()->addRootFolder([[[NSBundle mainBundle] resourcePath] UTF8String]);
    
    size_t bytesRead = 0;
    auto buffer = ResourcesManager::sharedManager()->readData("test.txt", &bytesRead);
    STAssertEquals(bytesRead, (size_t)4, @"");
    NSString *string = [[NSString alloc] initWithBytes:buffer.get() length:bytesRead encoding:NSUTF8StringEncoding];
    STAssertEqualObjects(string, @"test", @"");
}

- (void)testReadLocalizedFile
{
    ResourcesManager::sharedManager()->addLanguageFolder("ru", "localized/ru");
    ResourcesManager::sharedManager()->addLanguageFolder("es", "localized/es");
    ResourcesManager::sharedManager()->addRootFolder([[[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"lang_res"] UTF8String]);
    
    size_t bytesRead = 0;

    ResourcesManager::sharedManager()->setCurrentLanguage("ru");
    auto buffer = ResourcesManager::sharedManager()->readData("file_in_folder.txt", &bytesRead);
    STAssertTrue(bytesRead > 0, @"");
    NSString *string = [[NSString alloc] initWithBytes:buffer.get() length:bytesRead encoding:NSUTF8StringEncoding];
    STAssertEqualObjects(string, @"файл в папке", @"");

    ResourcesManager::sharedManager()->setCurrentLanguage("es");
    buffer = ResourcesManager::sharedManager()->readData("file_in_folder.txt", &bytesRead);
    STAssertTrue(bytesRead > 0, @"");
    string = [[NSString alloc] initWithBytes:buffer.get() length:bytesRead encoding:NSUTF8StringEncoding];
    STAssertEqualObjects(string, @"un \"file\" es en papel", @"");
}

//- (void)testReadStoredFileInZip
//{
//    ResourcesManager::sharedManager()->addArchive([[[NSBundle mainBundle] pathForResource:@"archive1" ofType:@"zip"] UTF8String])
//    
//    char buffer[4];
//    int bytesRead = ResourcesManager::sharedManager()->readData("test_stored.txt", &buffer, sizeof(buffer));
//    STAssertEquals(bytesRead, 4, @"");
//    STAssertEquals(buffer, "test", @"");
//}
//
//- (void)testReadStoredStreamInZip
//{
//    ResourcesManager::sharedManager()->addArchive([[[NSBundle mainBundle] pathForResource:@"archive1" ofType:@"zip"] UTF8String])
//    
//    Stream stream = ResourcesManager::sharedManager()->getStream("test_stored.txt");
//    STAssertEquals(stream.getSize(), 4, @"");
//    char buffer[2];
//    int bytesRead = stream.readData(&buffer, sizeof(buffer));
//    STAssertEquals(bytesRead, 2, @"");
//    STAssertEquals(buffer, "te", @"");
//    
//    bytesRead = stream.readData(&buffer, sizeof(buffer));
//    STAssertEquals(bytesRead, 2, @"");
//    STAssertEquals(buffer, "st", @"");
//}
//
//- (void)testReadSmallScreenFileFromFolder
//{
//    ResourcesManager::sharedManager()->addRootFolder([[[NSBundle mainBundle] resourcePath] UTF8String]);
//    
//    ResourcesManager::sharedManager()->setTagFolder("screen-small", "small");
//    ResourcesManager::sharedManager()->activateTag("small");
//    
//    char buffer[100];
//    int bytesRead = ResourcesManager::sharedManager()->readData("test.txt", &buffer, sizeof(buffer));
//    STAssertEquals(bytesRead, strlen("small-screen test"), @"");
//    STAssertEquals(buffer, "small-screen test", @"");
//}
//
//- (void)testReadSmallScreenFileWithSuffix
//{
//    ResourcesManager::sharedManager()->addRootFolder([[[NSBundle mainBundle] resourcePath] UTF8String]);
//    
//    ResourcesManager::sharedManager()->setTagSuffix("small", "small");
//    ResourcesManager::sharedManager()->activateTag("small");
//    
//    char buffer[100];
//    int bytesRead = ResourcesManager::sharedManager()->readData("test.txt", &buffer, sizeof(buffer));
//    STAssertEquals(bytesRead, strlen("small-screen test"), @"");
//    STAssertEquals(buffer, "small-screen test", @"");
//}

@end
