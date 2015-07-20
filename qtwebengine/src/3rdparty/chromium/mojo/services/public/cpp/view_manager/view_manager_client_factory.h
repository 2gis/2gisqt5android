// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_CLIENT_FACTORY_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_CLIENT_FACTORY_H_

#include "base/memory/scoped_ptr.h"
#include "mojo/public/cpp/application/interface_factory.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"

namespace mojo {

class ViewManagerDelegate;
class Shell;

// Add an instance of this class to an incoming connection to allow it to
// instantiate ViewManagerClient implementations in response to
// ViewManagerClient requests.
class ViewManagerClientFactory : public InterfaceFactory<ViewManagerClient> {
 public:
  ViewManagerClientFactory(Shell* shell, ViewManagerDelegate* delegate);
  ~ViewManagerClientFactory() override;

  // Creates a ViewManagerClient from the supplied arguments.
  static scoped_ptr<ViewManagerClient> WeakBindViewManagerToPipe(
      ScopedMessagePipeHandle handle,
      Shell* shell,
      ViewManagerDelegate* delegate);

  // InterfaceFactory<ViewManagerClient> implementation.
  void Create(ApplicationConnection* connection,
              InterfaceRequest<ViewManagerClient> request) override;

 private:
  Shell* shell_;
  ViewManagerDelegate* delegate_;
};

}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_CLIENT_FACTORY_H_
