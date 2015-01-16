// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The common functionality when building with or without snapshots.

#include "src/v8.h"

#include "src/api.h"
#include "src/serialize.h"
#include "src/snapshot.h"
#include "src/platform.h"

namespace v8 {
namespace internal {


static void ReserveSpaceForSnapshot(Deserializer* deserializer,
                                    const char* file_name) {
  int file_name_length = StrLength(file_name) + 10;
  Vector<char> name = Vector<char>::New(file_name_length + 1);
  SNPrintF(name, "%s.size", file_name);
  FILE* fp = OS::FOpen(name.start(), "r");
  CHECK_NE(NULL, fp);
  int new_size, pointer_size, data_size, code_size, map_size, cell_size,
      property_cell_size;
#ifdef _MSC_VER
  // Avoid warning about unsafe fscanf from MSVC.
  // Please note that this is only fine if %c and %s are not being used.
#define fscanf fscanf_s
#endif
  CHECK_EQ(1, fscanf(fp, "new %d\n", &new_size));
  CHECK_EQ(1, fscanf(fp, "pointer %d\n", &pointer_size));
  CHECK_EQ(1, fscanf(fp, "data %d\n", &data_size));
  CHECK_EQ(1, fscanf(fp, "code %d\n", &code_size));
  CHECK_EQ(1, fscanf(fp, "map %d\n", &map_size));
  CHECK_EQ(1, fscanf(fp, "cell %d\n", &cell_size));
  CHECK_EQ(1, fscanf(fp, "property cell %d\n", &property_cell_size));
#ifdef _MSC_VER
#undef fscanf
#endif
  fclose(fp);
  deserializer->set_reservation(NEW_SPACE, new_size);
  deserializer->set_reservation(OLD_POINTER_SPACE, pointer_size);
  deserializer->set_reservation(OLD_DATA_SPACE, data_size);
  deserializer->set_reservation(CODE_SPACE, code_size);
  deserializer->set_reservation(MAP_SPACE, map_size);
  deserializer->set_reservation(CELL_SPACE, cell_size);
  deserializer->set_reservation(PROPERTY_CELL_SPACE,
                                property_cell_size);
  name.Dispose();
}


void Snapshot::ReserveSpaceForLinkedInSnapshot(Deserializer* deserializer) {
  deserializer->set_reservation(NEW_SPACE, new_space_used_);
  deserializer->set_reservation(OLD_POINTER_SPACE, pointer_space_used_);
  deserializer->set_reservation(OLD_DATA_SPACE, data_space_used_);
  deserializer->set_reservation(CODE_SPACE, code_space_used_);
  deserializer->set_reservation(MAP_SPACE, map_space_used_);
  deserializer->set_reservation(CELL_SPACE, cell_space_used_);
  deserializer->set_reservation(PROPERTY_CELL_SPACE,
                                property_cell_space_used_);
}


bool Snapshot::Initialize(const char* snapshot_file) {
  if (snapshot_file) {
    int len;
    byte* str = ReadBytes(snapshot_file, &len);
    if (!str) return false;
    bool success;
    {
      SnapshotByteSource source(str, len);
      Deserializer deserializer(&source);
      ReserveSpaceForSnapshot(&deserializer, snapshot_file);
      success = V8::Initialize(&deserializer);
    }
    DeleteArray(str);
    return success;
  } else if (size_ > 0) {
    ElapsedTimer timer;
    if (FLAG_profile_deserialization) {
      timer.Start();
    }
    SnapshotByteSource source(raw_data_, raw_size_);
    Deserializer deserializer(&source);
    ReserveSpaceForLinkedInSnapshot(&deserializer);
    bool success = V8::Initialize(&deserializer);
    if (FLAG_profile_deserialization) {
      double ms = timer.Elapsed().InMillisecondsF();
      PrintF("[Snapshot loading and deserialization took %0.3f ms]\n", ms);
    }
    return success;
  }
  return false;
}


bool Snapshot::HaveASnapshotToStartFrom() {
  return size_ != 0;
}


Handle<Context> Snapshot::NewContextFromSnapshot(Isolate* isolate) {
  if (context_size_ == 0) {
    return Handle<Context>();
  }
  SnapshotByteSource source(context_raw_data_,
                            context_raw_size_);
  Deserializer deserializer(&source);
  Object* root;
  deserializer.set_reservation(NEW_SPACE, context_new_space_used_);
  deserializer.set_reservation(OLD_POINTER_SPACE, context_pointer_space_used_);
  deserializer.set_reservation(OLD_DATA_SPACE, context_data_space_used_);
  deserializer.set_reservation(CODE_SPACE, context_code_space_used_);
  deserializer.set_reservation(MAP_SPACE, context_map_space_used_);
  deserializer.set_reservation(CELL_SPACE, context_cell_space_used_);
  deserializer.set_reservation(PROPERTY_CELL_SPACE,
                               context_property_cell_space_used_);
  deserializer.DeserializePartial(isolate, &root);
  CHECK(root->IsContext());
  return Handle<Context>(Context::cast(root));
}

} }  // namespace v8::internal
