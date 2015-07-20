// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_log_logger.h"

#include <stdio.h>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "net/base/net_log_util.h"

namespace net {

NetLogLogger::NetLogLogger(FILE* file, const base::Value& constants)
    : file_(file),
      log_level_(NetLog::LOG_STRIP_PRIVATE_DATA),
      added_events_(false) {
  DCHECK(file);

  // Write constants to the output file.  This allows loading files that have
  // different source and event types, as they may be added and removed
  // between Chrome versions.
  std::string json;
  base::JSONWriter::Write(&constants, &json);
  fprintf(file_.get(), "{\"constants\": %s,\n", json.c_str());
  fprintf(file_.get(), "\"events\": [\n");
}

NetLogLogger::~NetLogLogger() {
  if (file_.get())
    fprintf(file_.get(), "]}");
}

void NetLogLogger::set_log_level(net::NetLog::LogLevel log_level) {
  DCHECK(!net_log());
  log_level_ = log_level;
}

void NetLogLogger::StartObserving(net::NetLog* net_log) {
  net_log->AddThreadSafeObserver(this, log_level_);
}

void NetLogLogger::StopObserving() {
  net_log()->RemoveThreadSafeObserver(this);
}

void NetLogLogger::OnAddEntry(const net::NetLog::Entry& entry) {
  // Add a comma and newline for every event but the first.  Newlines are needed
  // so can load partial log files by just ignoring the last line.  For this to
  // work, lines cannot be pretty printed.
  scoped_ptr<base::Value> value(entry.ToValue());
  std::string json;
  base::JSONWriter::Write(value.get(), &json);
  fprintf(file_.get(), "%s%s",
          (added_events_ ? ",\n" : ""),
          json.c_str());
  added_events_ = true;
}

// static
base::DictionaryValue* NetLogLogger::GetConstants() {
  return GetNetConstants().release();
}

}  // namespace net
