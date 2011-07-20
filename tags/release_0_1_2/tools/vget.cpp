/*
 * Copyright 2008 Google Inc.
 * Copyright 2006, 2007 Nintendo Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <es.h>
#include <es/clsid.h>
#include <es/exception.h>
#include <es/handle.h>
#include <es/base/IClassStore.h>
#include <es/base/IStream.h>
#include <es/base/IFile.h>
#include <es/device/IFileSystem.h>
#include <es/naming/IContext.h>
#include <es/handle.h>
#include "vdisk.h"

u8 sec[512];

void get(Handle<IContext> root, char* filename)
{
    Handle<IFile> file(root->lookup(filename));
    if (!file)
    {
        esReport("Could not open %s\n", filename);
        exit(EXIT_FAILURE);
    }

    const char* p = filename;
    p += strlen(p);
    while (filename < --p)
    {
        if (strchr(":/\\", *p))
        {
            ++p;
            break;
        }
    }
    FILE* f = fopen(p, "wb");
    if (f == 0)
    {
        esReport("Could not create %s\n", p);
        exit(EXIT_FAILURE);
    }

    Handle<IStream> stream(file->getStream());
    long size = stream->getSize();
    while (0 < size)
    {
        long len = stream->read(sec, 512);
        if (len <= 0)
        {
            exit(EXIT_FAILURE);
        }
        fwrite(sec, len, 1, f);
        size -= len;
    }
    fclose(f);
}

int main(int argc, char* argv[])
{
    IInterface* ns = 0;
    esInit(&ns);
    Handle<IContext> nameSpace(ns);

    Handle<IClassStore> classStore(nameSpace->lookup("class"));
    esRegisterFatFileSystemClass(classStore);

    if (argc < 3)
    {
        esReport("usage: %s disk_image file\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    Handle<IStream> disk = new VDisk(static_cast<char*>(argv[1]));
    Handle<IFileSystem> fatFileSystem;
    fatFileSystem = reinterpret_cast<IFileSystem*>(
        esCreateInstance(CLSID_FatFileSystem, IFileSystem::iid()));
    fatFileSystem->mount(disk);

    long long freeSpace;
    long long totalSpace;
    freeSpace = fatFileSystem->getFreeSpace();
    totalSpace = fatFileSystem->getTotalSpace();
    esReport("Free space %lld, Total space %lld\n", freeSpace, totalSpace);

    {
        Handle<IContext> root;

        root = fatFileSystem->getRoot();
        get(root, argv[2]);
    }
    fatFileSystem->dismount();
    fatFileSystem = 0;
}