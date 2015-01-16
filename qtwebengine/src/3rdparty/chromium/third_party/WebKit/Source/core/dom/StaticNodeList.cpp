/*
 * Copyright (C) 2007, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/dom/StaticNodeList.h"

#include "core/dom/Element.h"
#include <v8.h>

namespace WebCore {

PassRefPtrWillBeRawPtr<StaticNodeList> StaticNodeList::adopt(WillBeHeapVector<RefPtrWillBeMember<Node> >& nodes)
{
    RefPtrWillBeRawPtr<StaticNodeList> nodeList = adoptRefWillBeNoop(new StaticNodeList);
    nodeList->m_nodes.swap(nodes);
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(nodeList->AllocationSize());
    return nodeList.release();
}

StaticNodeList::~StaticNodeList()
{
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(-AllocationSize());
}

unsigned StaticNodeList::length() const
{
    return m_nodes.size();
}

Node* StaticNodeList::item(unsigned index) const
{
    if (index < m_nodes.size())
        return m_nodes[index].get();
    return 0;
}

void StaticNodeList::trace(Visitor* visitor)
{
    visitor->trace(m_nodes);
    NodeList::trace(visitor);
}

} // namespace WebCore
