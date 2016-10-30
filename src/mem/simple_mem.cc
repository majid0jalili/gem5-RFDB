/*
 * Copyright (c) 2010-2013, 2015 ARM Limited
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2001-2005 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Ron Dreslinski
 *          Ali Saidi
 *          Andreas Hansson
 */

#include "base/random.hh"
#include "mem/simple_mem.hh"
#include "debug/Drain.hh"


#include "base/callback.hh"
#include "base/statistics.hh"

using namespace std;

SimpleMemory::SimpleMemory(const SimpleMemoryParams* p) :
    AbstractMemory(p),
    port(name() + ".port", *this), latency(p->latency),
    latency_var(p->latency_var), bandwidth(p->bandwidth), isBusy(false),
    retryReq(false), retryResp(false),
    releaseEvent(this), dequeueEvent(this)
{
	///Majid/// Registering the callback function
	Callback* cb = new MakeCallback<SimpleMemory,
        &SimpleMemory::onExit>(this);
	registerExitCallback(cb);
		
	Callback* cb2 = new MakeCallback<SimpleMemory,
        &SimpleMemory::onDump>(this);
	Stats::registerDumpCallback(cb2);	
		
	
	
}



void
SimpleMemory::init()
{
    AbstractMemory::init();

    // allow unconnected memories as this is used in several ruby
    // systems at the moment
    if (port.isConnected()) {
        port.sendRangeChange();
    }
}

Tick
SimpleMemory::recvAtomic(PacketPtr pkt)
{
    panic_if(pkt->cacheResponding(), "Should not see packets where cache "
             "is responding");

    access(pkt);
	
	///Majid
	uint8_t hostdata[64] = {}; //our block size is 64B
	uint8_t hostdata_EVEN[64] = {}; //our block size is 64B
	uint8_t pktdata[64] = {}; //our block size is 64B
	
	uint8_t *hostAddr = pmemAddr + pkt->getAddr() - range.start();
	std::memcpy(hostdata, hostAddr, 64);
	std::memcpy(hostdata_EVEN, hostAddr+64, 64);
	std::memcpy(pktdata, pkt->getConstPtr<uint8_t>(), 64);
	
	if(pkt->isWrite() && pkt->hasData() && (pkt->getSize() == 64) )
		writeChecker.dataAnalyser(pktdata, hostdata_EVEN, true);
	
	if(pkt->isRead())//For read packets,there is no payload in packet and we need to reterive the data from PHY mem
		readChecker.dataAnalyser(hostdata, hostdata_EVEN, true);
	
	///!Majid
	
    return getLatency();
}

void
SimpleMemory::onExit()
{
	cout<<"onExit SimpleMemory "<<curTick()<<endl;
	writeChecker.printACC("write");
	readChecker.printACC("read");

}

void
SimpleMemory::onDump() 
{
	cout<<"*******onDump SimpleMemory "<<curTick()<<endl;
	cout<<"====WRITE PRINT Dump"<<endl;
	writeChecker.printEPOCH("write");
	writeChecker.resetVar();
	cout<<"====WRITE PRINT ACC"<<endl;
	writeChecker.printACC("write");
	
	cout<<"====READ PRINT Dump"<<endl;
	readChecker.printEPOCH("read");
	readChecker.resetVar();
	cout<<"====READ PRINT ACC"<<endl;
	readChecker.printACC("read");
	
	for(long int i = 100 ; i < ( size()/64) ; i++){ //16GB/64B=268,435,456
		uint8_t hostdata[64] = {};
		uint8_t dumy[64] = {};
		// cout<<"range.start() "<<range.start()<<endl;
		// cout<<"size() "<< size()<<endl;
		// cout<<"pmemAddr "<< pmemAddr<<endl;
		// getchar();
		// cout<<"getAddrRange()  "<<  getAddrRange() <<endl;
		uint8_t *hostAddr = pmemAddr + (i*64) - range.start();
		std::memcpy(hostdata, hostAddr, 64);
		wholeChecker.dataAnalyser(hostdata, dumy, false);
	}
	cout<<"====whole PRINT Dump"<<endl;
	wholeChecker.printEPOCH("whole");
	wholeChecker.resetVar();
	cout<<"====whole PRINT ACC"<<endl;
	wholeChecker.printACC("whole");
	
}

StatTracker::StatTracker()
{
	//Manual initialiation; some platforms are wicked
	bitPatterns[0]=bitPatterns[1]=bitPatterns[2]=bitPatterns[3]=0;
	RFDBCounter[0]=RFDBCounter[1]=0;
	allZeroOne[0]=allZeroOne[1]=0;
	EPOCH_SPCMFri[0]=EPOCH_SPCMFri[1]=0;
	SPCMFri[0]=SPCMFri[1]=0;
	for(int i = 0 ; i < 257 ; i++){
		ditributionRFDB[i]  = 0;
	}
	
}


	
void
StatTracker::printEPOCH(string str)
{
	double tot = 0;
	for(int i = 0 ; i < 4 ; i++){
		tot += EPOCH_bitPatterns[i];
	}
	cout<<str+"_DUMP_Frequency of bit patterns ";
	for(int i = 0 ; i < 4 ; i++){
		cout<<EPOCH_bitPatterns[i]/tot<<" ";
	}cout<<endl;
	
	cout<<str+"_DUMP_RFDB Ratio "<<EPOCH_RFDBCounter[0]/(EPOCH_RFDBCounter[0]+EPOCH_RFDBCounter[1])<<endl;
	
	cout<<str+"_DUMP_Distibution of 00 or 11 ";
	for(int i = 0 ; i < 257 ; i++){
		cout<<EPOCH_ditributionRFDB[i]<<" ";
	}cout<<endl;
	
	cout<<str+"_DUMP_AllZO ratio "<<EPOCH_allZeroOne[0]/(EPOCH_allZeroOne[0]+EPOCH_allZeroOne[1])<<endl;
	cout<<str+"_DUMP_SPCMFri ratio "<<EPOCH_SPCMFri[0]/(EPOCH_SPCMFri[0]+EPOCH_SPCMFri[1])<<endl;

}

void
StatTracker::printACC(string str)
{
	double tot = 0;
	for(int i = 0 ; i < 4 ; i++){
		tot += bitPatterns[i];
	}
	cout<<str+"_ACC_Frequency of bit patterns ";
	for(int i = 0 ; i < 4 ; i++){
		cout<<bitPatterns[i]/tot<<" ";
	}cout<<endl;
	
	cout<<str+"_ACC_RFDB Ratio "<<RFDBCounter[0]/(RFDBCounter[0]+RFDBCounter[1])<<endl;
	cout<<str+"_ACC_Distibution of 00 or 11 "<<endl;
	for(int i = 0 ; i < 257 ; i++){
		cout<<ditributionRFDB[i]<<" ";
	}cout<<endl;
	cout<<str+"_ACC_AllZO ratio "<<allZeroOne[0]/(allZeroOne[0]+allZeroOne[1])<<endl;
	cout<<str+"_ACC_SPCMFri ratio "<<SPCMFri[0]/(SPCMFri[0]+SPCMFri[1])<<endl;
}

void 
StatTracker::dataAnalyser(uint8_t data[64], uint8_t evenData[64], bool CheckSPCM)
{
	
	
	//Now, we just need a loop
	bool isRFDB= true;
	int num00or11 = 0 ;
	int allZO = 0; //check for all zero or one
	for(int i = 0 ; i < 64 ; i++){
		if(data[i] == 0 || data[i] == 255 )
			allZO++;
		for(int j = 0 ; j < 4 ; j++){//Counts to 4, since we consider 2-bit MLC PCM
			uint8_t chPacket = (( data[i]>>(2*j))&0x3);
			assert(chPacket < 4);
			bitPatterns[chPacket]++;// the corresponding entry for each bit pattern is increased!
			EPOCH_bitPatterns[chPacket]++;// the corresponding entry for each bit pattern is increased!
			if((chPacket==1) || (chPacket==2) ){
				isRFDB = false;
			}
			if((chPacket==0) || (chPacket==3) ){
				num00or11++;
			}
			
		}
	}
	if(isRFDB){
		RFDBCounter[0]++;
		EPOCH_RFDBCounter[0]++;
	}else{
		RFDBCounter[1]++;
		EPOCH_RFDBCounter[1]++;
	}
	// assert(num00or11 < 256);
	ditributionRFDB[num00or11]++;
	EPOCH_ditributionRFDB[num00or11]++;
	if(allZO == 64){
		allZeroOne[0]++;
		EPOCH_allZeroOne[0]++;
	}else{
		allZeroOne[1]++;
		EPOCH_allZeroOne[1]++;
	}
	bool goodSPCM = true;
	if(CheckSPCM){
		for(int i = 0 ; i < 64 ; i++){
			for(int j = 0 ; j < 8 ; j++){
				int ch1  = (( data[i]>>(j))&0x1);
				int ch2  = (( evenData[i]>>(j))&0x1);
				if(ch1 != ch2){
					goodSPCM = false;
					break;
				}
			}
		}
	}
	if(goodSPCM){
		SPCMFri[0]++;
		EPOCH_SPCMFri[0]++;
	}else{
		SPCMFri[1]++;
		EPOCH_SPCMFri[1]++;
	}
	
}

void
StatTracker::resetVar()
{
	EPOCH_bitPatterns[0]=EPOCH_bitPatterns[1]=EPOCH_bitPatterns[2]=EPOCH_bitPatterns[3]=0;
	EPOCH_RFDBCounter[0]=EPOCH_RFDBCounter[1]=0;
	EPOCH_allZeroOne[0]=EPOCH_allZeroOne[1]=0;
	for(int i = 0 ; i < 257 ; i++){
		EPOCH_ditributionRFDB[i] = 0;
	}
	
}

void
SimpleMemory::recvFunctional(PacketPtr pkt)
{
    pkt->pushLabel(name());

    functionalAccess(pkt);

    bool done = false;
    auto p = packetQueue.begin();
    // potentially update the packets in our packet queue as well
    while (!done && p != packetQueue.end()) {
        done = pkt->checkFunctional(p->pkt);
        ++p;
    }

    pkt->popLabel();
}

bool
SimpleMemory::recvTimingReq(PacketPtr pkt)
{
    panic_if(pkt->cacheResponding(), "Should not see packets where cache "
             "is responding");

    panic_if(!(pkt->isRead() || pkt->isWrite()),
             "Should only see read and writes at memory controller, "
             "saw %s to %#llx\n", pkt->cmdString(), pkt->getAddr());

    // we should not get a new request after committing to retry the
    // current one, but unfortunately the CPU violates this rule, so
    // simply ignore it for now
    if (retryReq)
        return false;

    // if we are busy with a read or write, remember that we have to
    // retry
    if (isBusy) {
        retryReq = true;
        return false;
    }

    // technically the packet only reaches us after the header delay,
    // and since this is a memory controller we also need to
    // deserialise the payload before performing any write operation
    Tick receive_delay = pkt->headerDelay + pkt->payloadDelay;
    pkt->headerDelay = pkt->payloadDelay = 0;

    // update the release time according to the bandwidth limit, and
    // do so with respect to the time it takes to finish this request
    // rather than long term as it is the short term data rate that is
    // limited for any real memory

    // calculate an appropriate tick to release to not exceed
    // the bandwidth limit
    Tick duration = pkt->getSize() * bandwidth;

    // only consider ourselves busy if there is any need to wait
    // to avoid extra events being scheduled for (infinitely) fast
    // memories
    if (duration != 0) {
        schedule(releaseEvent, curTick() + duration);
        isBusy = true;
    }

    // go ahead and deal with the packet and put the response in the
    // queue if there is one
    bool needsResponse = pkt->needsResponse();
    recvAtomic(pkt);
    // turn packet around to go back to requester if response expected
    if (needsResponse) {
        // recvAtomic() should already have turned packet into
        // atomic response
        assert(pkt->isResponse());

        Tick when_to_send = curTick() + receive_delay + getLatency();

        // typically this should be added at the end, so start the
        // insertion sort with the last element, also make sure not to
        // re-order in front of some existing packet with the same
        // address, the latter is important as this memory effectively
        // hands out exclusive copies (shared is not asserted)
        auto i = packetQueue.end();
        --i;
        while (i != packetQueue.begin() && when_to_send < i->tick &&
               i->pkt->getAddr() != pkt->getAddr())
            --i;

        // emplace inserts the element before the position pointed to by
        // the iterator, so advance it one step
        packetQueue.emplace(++i, pkt, when_to_send);

        if (!retryResp && !dequeueEvent.scheduled())
            schedule(dequeueEvent, packetQueue.back().tick);
    } else {
        pendingDelete.reset(pkt);
    }

    return true;
}

void
SimpleMemory::release()
{
    assert(isBusy);
    isBusy = false;
    if (retryReq) {
        retryReq = false;
        port.sendRetryReq();
    }
}

void
SimpleMemory::dequeue()
{
    assert(!packetQueue.empty());
    DeferredPacket deferred_pkt = packetQueue.front();

    retryResp = !port.sendTimingResp(deferred_pkt.pkt);

    if (!retryResp) {
        packetQueue.pop_front();

        // if the queue is not empty, schedule the next dequeue event,
        // otherwise signal that we are drained if we were asked to do so
        if (!packetQueue.empty()) {
            // if there were packets that got in-between then we
            // already have an event scheduled, so use re-schedule
            reschedule(dequeueEvent,
                       std::max(packetQueue.front().tick, curTick()), true);
        } else if (drainState() == DrainState::Draining) {
            DPRINTF(Drain, "Draining of SimpleMemory complete\n");
            signalDrainDone();
        }
    }
}

Tick
SimpleMemory::getLatency() const
{
    return latency +
        (latency_var ? random_mt.random<Tick>(0, latency_var) : 0);
}

void
SimpleMemory::recvRespRetry()
{
    assert(retryResp);

    dequeue();
}

BaseSlavePort &
SimpleMemory::getSlavePort(const std::string &if_name, PortID idx)
{
    if (if_name != "port") {
        return MemObject::getSlavePort(if_name, idx);
    } else {
        return port;
    }
}

DrainState
SimpleMemory::drain()
{
    if (!packetQueue.empty()) {
        DPRINTF(Drain, "SimpleMemory Queue has requests, waiting to drain\n");
        return DrainState::Draining;
    } else {
        return DrainState::Drained;
    }
}

SimpleMemory::MemoryPort::MemoryPort(const std::string& _name,
                                     SimpleMemory& _memory)
    : SlavePort(_name, &_memory), memory(_memory)
{ }

AddrRangeList
SimpleMemory::MemoryPort::getAddrRanges() const
{
    AddrRangeList ranges;
    ranges.push_back(memory.getAddrRange());
    return ranges;
}

Tick
SimpleMemory::MemoryPort::recvAtomic(PacketPtr pkt)
{
    return memory.recvAtomic(pkt);
}

void
SimpleMemory::MemoryPort::recvFunctional(PacketPtr pkt)
{
    memory.recvFunctional(pkt);
}

bool
SimpleMemory::MemoryPort::recvTimingReq(PacketPtr pkt)
{
    return memory.recvTimingReq(pkt);
}

void
SimpleMemory::MemoryPort::recvRespRetry()
{
    memory.recvRespRetry();
}

SimpleMemory*
SimpleMemoryParams::create()
{
    return new SimpleMemory(this);
}
