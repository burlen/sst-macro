/**
Copyright 2009-2018 National Technology and Engineering Solutions of Sandia, 
LLC (NTESS).  Under the terms of Contract DE-NA-0003525, the U.S.  Government 
retains certain rights in this software.

Sandia National Laboratories is a multimission laboratory managed and operated
by National Technology and Engineering Solutions of Sandia, LLC., a wholly 
owned subsidiary of Honeywell International, Inc., for the U.S. Department of 
Energy's National Nuclear Security Administration under contract DE-NA0003525.

Copyright (c) 2009-2018, NTESS

All rights reserved.

Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

    * Neither the name of the copyright holder nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Questions? Contact sst-macro-help@sandia.gov
*/

#include <sumi-mpi/mpi_protocol/mpi_protocol.h>
#include <sumi-mpi/mpi_queue/mpi_queue.h>
#include <sumi-mpi/mpi_api.h>
#include <sumi-mpi/mpi_queue/mpi_queue_recv_request.h>
#include <sprockit/sim_parameters.h>

namespace sumi {

MpiProtocol::MpiProtocol(SST::Params& params, MpiQueue *queue) :
  queue_(queue), mpi_(queue_->api())
{
}

void*
MpiProtocol::fillSendBuffer(int count, void* buffer, MpiType* typeobj)
{
  uint64_t length = count * typeobj->packed_size();
  void* eager_buf = new char[length];
  if (typeobj->contiguous()){
    ::memcpy(eager_buf, buffer, length);
  } else {
    typeobj->packSend(buffer, eager_buf, count);
  }
  return eager_buf;
}

void
MpiProtocol::logRecvDelay(int stage, MpiMessage* msg, MpiQueueRecvRequest* req)
{
  sstmac::TimeDelta active_delay;
  if (req->req()->activeWait()){
    active_delay = mpi_->activeDelay(req->req()->waitStart());
    req->req()->setWaitStart(mpi_->now());
  }
  sstmac::TimeDelta sync_delay;
  if (msg->timeStarted() > req->start()){
    sync_delay = msg->timeStarted() - req->start();
  }
  mpi_->logMessageDelay(msg, msg->payloadSize(), stage,
                        sync_delay, active_delay);
}

DirectPut::~DirectPut()
{
}

void
DirectPut::start(void* buffer, int src_rank, int dst_rank, sstmac::sw::TaskId tid, int count, MpiType* type,
                 int tag, MPI_Comm comm, int seq_id, MpiRequest* req)
{
  auto* msg = mpi_->rdmaPut<MpiMessage>(tid, count*type->packed_size(), nullptr, nullptr,
                queue_->pt2ptCqId(), queue_->pt2ptCqId(), sumi::Message::pt2pt,
                src_rank, dst_rank, type->id, tag, comm, seq_id, count, type->packed_size(),
                buffer, DIRECT_PUT);

  send_flows_.emplace(std::piecewise_construct,
                      std::forward_as_tuple(msg->flowId()),
                      std::forward_as_tuple(req));
}

void
DirectPut::incomingAck(MpiMessage *msg)
{
  mpi_queue_protocol_debug("RDMA get incoming ack %s", msg->toString().c_str());
  auto iter = send_flows_.find(msg->flowId());
  if (iter == send_flows_.end()){
    spkt_abort_printf("could not find matching ack for %s", msg->toString().c_str());
  }

  MpiRequest* req = iter->second;
  req->complete();
  send_flows_.erase(iter);
  delete msg;
}

void
DirectPut::incoming(MpiMessage* msg)
{
  mpi_queue_protocol_debug("RDMA put incoming %s", msg->toString().c_str());
  switch(msg->sstmac::hw::NetworkMessage::type()){
  case sstmac::hw::NetworkMessage::rdma_put_sent_ack: {
    incomingAck(msg);
    break;
  }
  case sstmac::hw::NetworkMessage::rdma_put_payload: {
    incomingPayload(msg);
    break;
  }
  default:
    spkt_abort_printf("Invalid message type %s to RDMA put protocol",
                      sstmac::hw::NetworkMessage::tostr(msg->sstmac::hw::NetworkMessage::type()));
  }
}

void
DirectPut::incoming(MpiMessage *msg, MpiQueueRecvRequest *req)
{
  queue_->finalizeRecv(msg, req);
  delete msg;
}

void
DirectPut::incomingPayload(MpiMessage* msg)
{
  MpiQueueRecvRequest* req = queue_->findMatchingRecv(msg);
  if (req) incoming(msg, req);

}


}
