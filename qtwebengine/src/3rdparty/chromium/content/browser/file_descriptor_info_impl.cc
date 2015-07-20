// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_descriptor_info_impl.h"

namespace content {

// static
scoped_ptr<FileDescriptorInfo> FileDescriptorInfoImpl::Create() {
  return scoped_ptr<FileDescriptorInfo>(new FileDescriptorInfoImpl());
}

FileDescriptorInfoImpl::FileDescriptorInfoImpl() {
}

FileDescriptorInfoImpl::~FileDescriptorInfoImpl() {
}

void FileDescriptorInfoImpl::Share(int id, base::PlatformFile fd) {
  AddToMapping(id, fd);
}

void FileDescriptorInfoImpl::Transfer(int id, base::ScopedFD fd) {
  AddToMapping(id, fd.get());
  owned_descriptors_.push_back(new base::ScopedFD(fd.Pass()));
}

base::PlatformFile FileDescriptorInfoImpl::GetFDAt(size_t i) const {
  return mapping_[i].first;
}

int FileDescriptorInfoImpl::GetIDAt(size_t i) const {
  return mapping_[i].second;
}

size_t FileDescriptorInfoImpl::GetMappingSize() const {
  return mapping_.size();
}

bool FileDescriptorInfoImpl::HasID(int id) const {
  for (unsigned i = 0; i < mapping_.size(); ++i) {
    if (mapping_[i].second == id)
      return true;
  }

  return false;
}

void FileDescriptorInfoImpl::AddToMapping(int id, base::PlatformFile fd) {
  DCHECK(!HasID(id));
  mapping_.push_back(std::make_pair(fd, id));
}

const base::FileHandleMappingVector& FileDescriptorInfoImpl::GetMapping()
    const {
  return mapping_;
}

base::FileHandleMappingVector
FileDescriptorInfoImpl::GetMappingWithIDAdjustment(int delta) const {
  base::FileHandleMappingVector result = mapping_;
  // Adding delta to each ID.
  for (unsigned i = 0; i < mapping_.size(); ++i)
    result[i].second += delta;
  return result;
}

}  // namespace content
