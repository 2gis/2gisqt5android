// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_service_win.h"

#include <cstdlib>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "device/hid/hid_connection_win.h"
#include "device/hid/hid_device_info.h"
#include "net/base/io_buffer.h"

#if defined(OS_WIN)

#define INITGUID

#include <setupapi.h>
#include <winioctl.h>
#include "base/win/scoped_handle.h"

#endif  // defined(OS_WIN)

// Setup API is required to enumerate HID devices.
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

namespace device {
namespace {

const char kHIDClass[] = "HIDClass";

}  // namespace

HidServiceWin::HidServiceWin() {
  base::ThreadRestrictions::AssertIOAllowed();
  task_runner_ = base::ThreadTaskRunnerHandle::Get();
  DCHECK(task_runner_.get());
  Enumerate();
}

HidServiceWin::~HidServiceWin() {}

void HidServiceWin::Enumerate() {
  BOOL res;
  HDEVINFO device_info_set;
  SP_DEVINFO_DATA devinfo_data;
  SP_DEVICE_INTERFACE_DATA device_interface_data;

  memset(&devinfo_data, 0, sizeof(SP_DEVINFO_DATA));
  devinfo_data.cbSize = sizeof(SP_DEVINFO_DATA);
  device_interface_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

  device_info_set = SetupDiGetClassDevs(
      &GUID_DEVINTERFACE_HID,
      NULL,
      NULL,
      DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

  std::set<std::string> connected_devices;

  if (device_info_set != INVALID_HANDLE_VALUE) {
    for (int device_index = 0;
         SetupDiEnumDeviceInterfaces(device_info_set,
                                     NULL,
                                     &GUID_DEVINTERFACE_HID,
                                     device_index,
                                     &device_interface_data);
         ++device_index) {
      DWORD required_size = 0;

      // Determime the required size of detail struct.
      SetupDiGetDeviceInterfaceDetailA(device_info_set,
                                       &device_interface_data,
                                       NULL,
                                       0,
                                       &required_size,
                                       NULL);

      scoped_ptr<SP_DEVICE_INTERFACE_DETAIL_DATA_A, base::FreeDeleter>
          device_interface_detail_data(
              static_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_A*>(
                  malloc(required_size)));
      device_interface_detail_data->cbSize =
          sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

      // Get the detailed data for this device.
      res = SetupDiGetDeviceInterfaceDetailA(device_info_set,
                                             &device_interface_data,
                                             device_interface_detail_data.get(),
                                             required_size,
                                             NULL,
                                             NULL);
      if (!res)
        continue;

      // Enumerate device info. Looking for Setup Class "HIDClass".
      for (DWORD i = 0;
          SetupDiEnumDeviceInfo(device_info_set, i, &devinfo_data);
          i++) {
        char class_name[256] = {0};
        res = SetupDiGetDeviceRegistryPropertyA(device_info_set,
                                                &devinfo_data,
                                                SPDRP_CLASS,
                                                NULL,
                                                (PBYTE) class_name,
                                                sizeof(class_name) - 1,
                                                NULL);
        if (!res)
          break;
        if (memcmp(class_name, kHIDClass, sizeof(kHIDClass)) == 0) {
          char driver_name[256] = {0};
          // Get bounded driver.
          res = SetupDiGetDeviceRegistryPropertyA(device_info_set,
                                                  &devinfo_data,
                                                  SPDRP_DRIVER,
                                                  NULL,
                                                  (PBYTE) driver_name,
                                                  sizeof(driver_name) - 1,
                                                  NULL);
          if (res) {
            // Found the driver.
            break;
          }
        }
      }

      if (!res)
        continue;

      PlatformAddDevice(device_interface_detail_data->DevicePath);
      connected_devices.insert(device_interface_detail_data->DevicePath);
    }
  }

  // Find disconnected devices.
  std::vector<std::string> disconnected_devices;
  for (DeviceMap::const_iterator it = devices().begin(); it != devices().end();
       ++it) {
    if (!ContainsKey(connected_devices, it->first)) {
      disconnected_devices.push_back(it->first);
    }
  }

  // Remove disconnected devices.
  for (size_t i = 0; i < disconnected_devices.size(); ++i) {
    PlatformRemoveDevice(disconnected_devices[i]);
  }
}

void HidServiceWin::CollectInfoFromButtonCaps(
    PHIDP_PREPARSED_DATA preparsed_data,
    HIDP_REPORT_TYPE report_type,
    USHORT button_caps_length,
    HidCollectionInfo* collection_info) {
  if (button_caps_length > 0) {
    scoped_ptr<HIDP_BUTTON_CAPS[]> button_caps(
        new HIDP_BUTTON_CAPS[button_caps_length]);
    if (HidP_GetButtonCaps(report_type,
                           &button_caps[0],
                           &button_caps_length,
                           preparsed_data) == HIDP_STATUS_SUCCESS) {
      for (size_t i = 0; i < button_caps_length; i++) {
        int report_id = button_caps[i].ReportID;
        if (report_id != 0) {
          collection_info->report_ids.insert(report_id);
        }
      }
    }
  }
}

void HidServiceWin::CollectInfoFromValueCaps(
    PHIDP_PREPARSED_DATA preparsed_data,
    HIDP_REPORT_TYPE report_type,
    USHORT value_caps_length,
    HidCollectionInfo* collection_info) {
  if (value_caps_length > 0) {
    scoped_ptr<HIDP_VALUE_CAPS[]> value_caps(
        new HIDP_VALUE_CAPS[value_caps_length]);
    if (HidP_GetValueCaps(
            report_type, &value_caps[0], &value_caps_length, preparsed_data) ==
        HIDP_STATUS_SUCCESS) {
      for (size_t i = 0; i < value_caps_length; i++) {
        int report_id = value_caps[i].ReportID;
        if (report_id != 0) {
          collection_info->report_ids.insert(report_id);
        }
      }
    }
  }
}

void HidServiceWin::PlatformAddDevice(const std::string& device_path) {
  HidDeviceInfo device_info;
  device_info.device_id = device_path;

  // Try to open the device.
  base::win::ScopedHandle device_handle(
      CreateFileA(device_path.c_str(),
                  GENERIC_WRITE | GENERIC_READ,
                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                  NULL,
                  OPEN_EXISTING,
                  FILE_FLAG_OVERLAPPED,
                  0));

  if (!device_handle.IsValid() &&
      GetLastError() == base::File::FILE_ERROR_ACCESS_DENIED) {
    base::win::ScopedHandle device_handle(
      CreateFileA(device_path.c_str(),
      GENERIC_READ,
      FILE_SHARE_READ,
      NULL,
      OPEN_EXISTING,
      FILE_FLAG_OVERLAPPED,
      0));

    if (!device_handle.IsValid())
      return;
  }

  // Get VID/PID pair.
  HIDD_ATTRIBUTES attrib = {0};
  attrib.Size = sizeof(HIDD_ATTRIBUTES);
  if (!HidD_GetAttributes(device_handle.Get(), &attrib))
    return;

  device_info.vendor_id = attrib.VendorID;
  device_info.product_id = attrib.ProductID;

  for (ULONG i = 32;
      HidD_SetNumInputBuffers(device_handle.Get(), i);
      i <<= 1);

  // Get usage and usage page (optional).
  PHIDP_PREPARSED_DATA preparsed_data;
  if (HidD_GetPreparsedData(device_handle.Get(), &preparsed_data) &&
      preparsed_data) {
    HIDP_CAPS capabilities = {0};
    if (HidP_GetCaps(preparsed_data, &capabilities) == HIDP_STATUS_SUCCESS) {
      device_info.max_input_report_size = capabilities.InputReportByteLength;
      device_info.max_output_report_size = capabilities.OutputReportByteLength;
      device_info.max_feature_report_size =
          capabilities.FeatureReportByteLength;
      HidCollectionInfo collection_info;
      collection_info.usage = HidUsageAndPage(
          capabilities.Usage,
          static_cast<HidUsageAndPage::Page>(capabilities.UsagePage));
      CollectInfoFromButtonCaps(preparsed_data,
                                HidP_Input,
                                capabilities.NumberInputButtonCaps,
                                &collection_info);
      CollectInfoFromButtonCaps(preparsed_data,
                                HidP_Output,
                                capabilities.NumberOutputButtonCaps,
                                &collection_info);
      CollectInfoFromButtonCaps(preparsed_data,
                                HidP_Feature,
                                capabilities.NumberFeatureButtonCaps,
                                &collection_info);
      CollectInfoFromValueCaps(preparsed_data,
                               HidP_Input,
                               capabilities.NumberInputValueCaps,
                               &collection_info);
      CollectInfoFromValueCaps(preparsed_data,
                               HidP_Output,
                               capabilities.NumberOutputValueCaps,
                               &collection_info);
      CollectInfoFromValueCaps(preparsed_data,
                               HidP_Feature,
                               capabilities.NumberFeatureValueCaps,
                               &collection_info);
      if (!collection_info.report_ids.empty()) {
        device_info.has_report_id = true;
      }
      device_info.collections.push_back(collection_info);
    }
    // Whether or not the device includes report IDs in its reports the size
    // of the report ID is included in the value provided by Windows. This
    // appears contrary to the MSDN documentation.
    if (device_info.max_input_report_size > 0) {
      device_info.max_input_report_size--;
    }
    if (device_info.max_output_report_size > 0) {
      device_info.max_output_report_size--;
    }
    if (device_info.max_feature_report_size > 0) {
      device_info.max_feature_report_size--;
    }
    HidD_FreePreparsedData(preparsed_data);
  }

  AddDevice(device_info);
}

void HidServiceWin::PlatformRemoveDevice(const std::string& device_path) {
  RemoveDevice(device_path);
}

void HidServiceWin::GetDevices(std::vector<HidDeviceInfo>* devices) {
  Enumerate();
  HidService::GetDevices(devices);
}

void HidServiceWin::Connect(const HidDeviceId& device_id,
                            const ConnectCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const auto& map_entry = devices().find(device_id);
  if (map_entry == devices().end()) {
    task_runner_->PostTask(FROM_HERE, base::Bind(callback, nullptr));
    return;
  }
  const HidDeviceInfo& device_info = map_entry->second;

  scoped_refptr<HidConnectionWin> connection(new HidConnectionWin(device_info));
  if (!connection->available()) {
    PLOG(ERROR) << "Failed to open device";
    connection = nullptr;
  }
  task_runner_->PostTask(FROM_HERE, base::Bind(callback, connection));
}

}  // namespace device
