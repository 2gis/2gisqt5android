//
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "config.h"

#include "core/inspector/InspectorTracingAgent.h"

#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectorClient.h"
#include "core/inspector/InspectorState.h"
#include "core/inspector/InspectorWorkerAgent.h"
#include "platform/TraceEvent.h"

namespace blink {

namespace TracingAgentState {
const char sessionId[] = "sessionId";
}

namespace {
const char devtoolsMetadataEventCategory[] = TRACE_DISABLED_BY_DEFAULT("devtools.timeline");
}

InspectorTracingAgent::InspectorTracingAgent(InspectorClient* client, InspectorWorkerAgent* workerAgent)
    : InspectorBaseAgent<InspectorTracingAgent>("Tracing")
    , m_layerTreeId(0)
    , m_client(client)
    , m_frontend(0)
    , m_workerAgent(workerAgent)
{
}

void InspectorTracingAgent::restore()
{
    emitMetadataEvents();
}

void InspectorTracingAgent::start(ErrorString*, const String* categoryFilter, const String*, const double*, PassRefPtrWillBeRawPtr<StartCallback> callback)
{
    ASSERT(m_state->getString(TracingAgentState::sessionId).isEmpty());
    m_state->setString(TracingAgentState::sessionId, IdentifiersFactory::createIdentifier());
    m_client->enableTracing(categoryFilter ? *categoryFilter : String());
    emitMetadataEvents();
    callback->sendSuccess();
}

void InspectorTracingAgent::end(ErrorString* errorString, PassRefPtrWillBeRawPtr<EndCallback> callback)
{
    m_client->disableTracing();
    resetSessionId();
    callback->sendSuccess();
}

String InspectorTracingAgent::sessionId()
{
    return m_state->getString(TracingAgentState::sessionId);
}

void InspectorTracingAgent::emitMetadataEvents()
{
    TRACE_EVENT_INSTANT1(devtoolsMetadataEventCategory, "TracingStartedInPage", "sessionId", sessionId().utf8());
    if (m_layerTreeId)
        setLayerTreeId(m_layerTreeId);
    m_workerAgent->setTracingSessionId(sessionId());
}

void InspectorTracingAgent::setLayerTreeId(int layerTreeId)
{
    m_layerTreeId = layerTreeId;
    TRACE_EVENT_INSTANT2(devtoolsMetadataEventCategory, "SetLayerTreeId", "sessionId", sessionId().utf8(), "layerTreeId", m_layerTreeId);
}

void InspectorTracingAgent::setFrontend(InspectorFrontend* frontend)
{
    m_frontend = frontend->tracing();
}

void InspectorTracingAgent::clearFrontend()
{
    resetSessionId();
}

void InspectorTracingAgent::resetSessionId()
{
    m_state->remove(TracingAgentState::sessionId);
    m_workerAgent->setTracingSessionId(sessionId());
}

}
