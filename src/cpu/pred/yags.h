
#ifndef __CPU_PRED_YAGS_HH__
#define __CPU_PRED_YAGS_HH__

#include "base/sat_counter.hh"
#include "cpu/pred/bpred_unit.hh"
#include "params/YagsBP.hh"
#include "base/cache/cache_entry.hh"

namespace gem5
{

namespace branch_prediction
{

/**
 * Implements a bi-mode branch predictor. The bi-mode predictor is a two-level
 * branch predictor that has three seprate history arrays: a taken array, a
 * not-taken array, and a choice array. The taken/not-taken arrays are indexed
 * by a hash of the PC and the global history. The choice array is indexed by
 * the PC only. Because the taken/not-taken arrays use the same index, they must
 * be the same size.
 *
 * The bi-mode branch predictor aims to eliminate the destructive aliasing that
 * occurs when two branches of opposite biases share the same global history
 * pattern. By separating the predictors into taken/not-taken arrays, and using
 * the branch's PC to choose between the two, destructive aliasing is reduced.
 */

class YagsBP : public BPredUnit
{
  public:
    YagsBP(const YagsBPParams &params);
    bool lookup(ThreadID tid, Addr pc, void * &bp_history) override;
    void updateHistories(ThreadID tid, Addr pc, bool uncond, bool taken,
                         Addr target, const StaticInstPtr &inst,
                         void * &bp_history) override;
    void squash(ThreadID tid, void * &bp_history) override;
    void update(ThreadID tid, Addr pc, bool taken,
                void * &bp_history, bool squashed,
                const StaticInstPtr & inst, Addr target) override;

  private:
    void updateGlobalHistReg(ThreadID tid, bool taken);
    void uncondBranch(ThreadID tid, Addr pc, void * &bp_history);

        // bool lookupTageCache(std::vector<CacheEntry>& cache,
        //                      std::vector<SatCounter8>& data,
        //               Addr addr, unsigned tag, bool &hit);
    bool lookupTageCache(std::vector<CacheEntry> &cache,
                         std::vector<SatCounter8> &data, unsigned addr,
                         Addr tag, bool &hit);
    Addr extTageAddr(Addr addr);

    struct BPHistory {
      unsigned globalHistoryReg;
      unsigned choicePred;
      // was the taken array's prediction used?
      // true: takenPred used
      // false: notPred used
      bool takenUsed;
      bool notTakenUsed;
      // prediction of the taken array
      // true: predict taken
      // false: predict not-taken
      bool takenPred;
      // prediction of the not-taken array
      // true: predict taken
      // false: predict not-taken
      bool notTakenPred;
      // the final taken/not-taken prediction
      // true: predict taken
      // false: predict not-taken
      bool finalPred;
    };

    std::vector<unsigned> globalHistoryReg;
    unsigned globalHistoryBits;
    unsigned historyRegisterMask;
    unsigned cacheTagMask;

    unsigned choicePredictorSize;
    unsigned choiceCtrBits;
    unsigned choiceHistoryMask;
    unsigned globalPredictorSize;
    unsigned globalCtrBits;
    unsigned globalHistoryMask;
    // how many cache line
    unsigned tagCacheSize;
    // counter bits in cache
    unsigned tagCacheBits;
    // cache tag bits
    unsigned cacheTagBits;

    // choice predictors
    std::vector<SatCounter8> choiceCounters;
    // taken direction predictors
    std::vector<CacheEntry> takenCache;
    std::vector<SatCounter8> takenCacheCounters;
    // not-taken direction predictors
    std::vector<CacheEntry> notTakenCache;
    std::vector<SatCounter8> notTakenCacheCounters;

    unsigned choiceThreshold;
    unsigned takenThreshold;
    unsigned notTakenThreshold;
};

} // namespace branch_prediction
} // namespace gem5

#endif // __CPU_PRED_BI_MODE_PRED_HH__
