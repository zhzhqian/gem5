/*
 * Copyright (c) 2025 Technical University of Munich
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
 */

#ifndef __CPU_O3_TDA_HH__
#define __CPU_O3_TDA_HH__

#include "base/statistics.hh"

#include "cpu/o3/commit.hh"
#include "cpu/o3/decode.hh"
#include "cpu/o3/fetch.hh"
#include "cpu/o3/iew.hh"
#include "cpu/o3/rename.hh"

namespace gem5
{

namespace o3
{

class CPU;

struct TopDownStats : statistics::Group
{
    TopDownStats(CPU *cpu, Fetch *fetch, Rename *rename, Decode *decode,
                 IEW *iew, Commit *commit);

    /** Level 1: four-way slot classification */
    struct TopDownL1 : statistics::Group
    {
        TopDownL1(CPU *cpu, Fetch *fetch, Rename *rename, Decode *decode,
                  IEW *iew, Commit *commit);
        /** Fraction of slots lost because frontend undersupplied backend */
        statistics::Formula frontendBound;
        /** Fraction of slots wasted due to misprediction / machine clears */
        statistics::Formula badSpeculation;
        /** Fraction of slots lost due to backend resource constraints */
        statistics::Formula backendBound;
        /** Fraction of slots that successfully retired */
        statistics::Formula retiring;
        /** Intermediate: renamed but not committed slots (squashed) */
        statistics::Formula squashedSlots;
        /** Intermediate: recovery cycles from mispred/mem-order violations */
        statistics::Formula badSpecCycles;
        /** Intermediate: bubble slots during pipeline refill after squash */
        statistics::Formula refillSlots;
        /** Intermediate: total frontend bubble slots */
        statistics::Formula FESlots;
        /** Intermediate: total backend bound slots */
        statistics::Formula BESlots;
        /** Intermediate: slots lost to serializing instructions */
        statistics::Formula serializeSlots;
    } topDownL1;

    /** Level 2: frontend bound breakdown */
    struct TopDownFrontendBoundL2 : statistics::Group
    {
        TopDownFrontendBoundL2(CPU *cpu, Fetch *fetch);
        /** Fraction due to full fetch stalls (icache/iTLB miss) */
        statistics::Formula fetchLatency;
        /** Fraction due to partial delivery (fetch < rename width) */
        statistics::Formula fetchBandwidth;
    } topDownFbL2;

    /** Level 2: bad speculation breakdown */
    struct TopDownBadSpeculationL2 : statistics::Group
    {
        TopDownBadSpeculationL2(CPU *cpu, Decode *decode, IEW *iew);
        /** Fraction attributed to branch mispredictions */
        statistics::Formula branchMissPredicts;
        /** Fraction attributed to memory order violations */
        statistics::Formula machineClears;
    } topDownBsL2;

    /** Level 2: backend bound breakdown */
    struct TopDownBackendBoundL2 : statistics::Group
    {
        TopDownBackendBoundL2(CPU *cpu, Rename *rename, IEW *iew);
        /** Raw execution stall cycles (low-execution + idle cycles) */
        statistics::Formula executionStalls;
        /** Fraction due to memory subsystem stalls */
        statistics::Formula memoryBound;
        /** Fraction due to non-memory execution unit stalls */
        statistics::Formula coreBound;
        /** Fraction due to serializing instruction stalls */
        statistics::Formula serializeBound;
        /** Raw cycles rename stalled for serializing instructions */
        statistics::Formula serializeStalls;
    } topDownBbL2;

    /** Level 3: memory bound breakdown by cache level */
    struct TopDownBackendBoundL3 : statistics::Group
    {
        TopDownBackendBoundL3(CPU *cpu, Rename *rename, IEW *iew);
        /** Fraction of memory bound attributed to L1 cache hits */
        statistics::Formula l1Bound;
        /** Fraction attributed to L1 misses served by L2 */
        statistics::Formula l2Bound;
        /** Fraction attributed to L2 misses served by L3 */
        statistics::Formula l3Bound;
        /** Fraction attributed to L3 misses (off-chip memory) */
        statistics::Formula extMemBound;
        /** Fraction attributed to store buffer stalls */
        statistics::Formula storeBound;
    } topDownBbMem;
};

} // namespace o3
} // namespace gem5

#endif // __CPU_O3_TDA_HH__
