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

#ifndef SnapprNIC_h
#define SnapprNIC_h

#include <sstmac/hardware/nic/nic.h>
#include <sstmac/hardware/interconnect/interconnect_fwd.h>
#include <sstmac/hardware/snappr/snappr_switch.h>
#include <sstmac/hardware/common/recv_cq.h>


namespace sstmac {
namespace hw {

/**
 @class SnapprNIC
 Network interface compatible with snappr network model
 */
class SnapprNIC :
  public NIC
{
 public:
#if SSTMAC_INTEGRATED_SST_CORE
  SST_ELI_REGISTER_SUBCOMPONENT(
    SnapprNIC,
    "macro",
    "snappr_nic",
    SST_ELI_ELEMENT_VERSION(1,0,0),
    "A NIC implementing the snappr model",
    "nic")
#else
  SST_ELI_REGISTER_DERIVED(
    NIC,
    SnapprNIC,
    "macro",
    "snappr",
    SST_ELI_ELEMENT_VERSION(1,0,0),
    "A NIC implementing the snappr model")
#endif

  SnapprNIC(SST::Component* parent, SST::Params& params);

  std::string toString() const override {
    return sprockit::printf("snappr nic(%d)", int(addr()));
  }

  void init(unsigned int phase) override;

  void setup() override;

  virtual ~SnapprNIC() throw ();

  void handlePayload(Event* ev);

  void handleCredit(Event* ev);

  void connectOutput(int src_outport, int dst_inport, EventLink::ptr&& link) override;

  void connectInput(int src_outport, int dst_inport, EventLink::ptr&& link) override;

  LinkHandler* creditHandler(int Port) override;

  LinkHandler* payloadHandler(int Port) override;

 private:
  void doSend(NetworkMessage* payload) override;

  void cqHandle(SnapprPacket* pkt);

  void eject(SnapprPacket* pkt);

  /**
   * @brief inject
   * @param payload
   * @param byte_offset
   * @return The byte_offset that was able to be injected (offset = length when injection is complete)
   */
  uint64_t inject(NetworkMessage* payload, uint64_t byte_offset);

 private:
  Timestamp inj_next_free_;
  EventLink::ptr inj_link_;
  EventLink::ptr credit_link_;

  TimeDelta inj_byte_delay_;

  uint32_t packet_size_;

  int switch_inport_;
  int switch_outport_;

  bool send_credits_;
  std::vector<uint32_t> credits_;
  std::vector<std::queue<std::pair<NetworkMessage*,uint64_t>>> inject_queues_;

  Timestamp ej_next_free_;
  RecvCQ cq_;

  enum state_t {
    STALLED = 0,
    IDLE = 1
  };
  int state_;
  Timestamp state_start_;

  int ftq_idle_state_;
  int ftq_active_state_;
  int ftq_stalled_state_;
  sstmac::FTQCalendar* state_ftq_;
  SST::Statistics::Statistic<uint64_t>* xmit_stall_;
  SST::Statistics::Statistic<uint64_t>* xmit_active_;
  SST::Statistics::Statistic<uint64_t>* xmit_idle_;
  SST::Statistics::Statistic<uint64_t>* bytes_sent_;

};

}
} // end of namespace sstmac


#endif // PiscesNIC_H
