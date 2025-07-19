
#include "cpu/pred/yags.h"

#include "base/bitfield.hh"
#include "base/intmath.hh"

namespace gem5 {

namespace branch_prediction {

Addr YagsBP::extTageAddr(Addr addr) { return addr & cacheTagMask; }
YagsBP::YagsBP(const YagsBPParams &params)
    : BPredUnit(params), globalHistoryReg(params.numThreads, 0),
      globalHistoryBits(ceilLog2(params.globalPredictorSize)),
      choicePredictorSize(params.choicePredictorSize),
      choiceCtrBits(params.choiceCtrBits),
      globalPredictorSize(params.globalPredictorSize),
      globalCtrBits(params.globalCtrBits),
      choiceCounters(choicePredictorSize, SatCounter8(choiceCtrBits)),

      tagCacheBits(params.tagCacheBits), tagCacheSize(params.tagCacheSize),
      cacheTagBits(params.cacheTagBits),

      takenCache(params.tagCacheSize,
                 CacheEntry(std::bind(&YagsBP::extTageAddr, this,
                                      std::placeholders::_1))),
      notTakenCache(params.tagCacheSize,
                    CacheEntry(std::bind(&YagsBP::extTageAddr, this,
                                         std::placeholders::_1))),
      takenCacheCounters(params.tagCacheSize, SatCounter8(tagCacheBits)),
      notTakenCacheCounters(params.tagCacheSize, SatCounter8(tagCacheBits)) {
  if (!isPowerOf2(choicePredictorSize))
    fatal("Invalid choice predictor size.\n");
  if (!isPowerOf2(globalPredictorSize))
    fatal("Invalid global history predictor size.\n");

  historyRegisterMask = mask(globalHistoryBits);
  choiceHistoryMask = choicePredictorSize - 1;
  globalHistoryMask = globalPredictorSize - 1;
  cacheTagMask = mask(params.cacheTagBits);

  choiceThreshold = (1ULL << (choiceCtrBits - 1)) - 1;
  takenThreshold = (1ULL << (globalCtrBits - 1)) - 1;
  notTakenThreshold = (1ULL << (globalCtrBits - 1)) - 1;
}

/*
 * For an unconditional branch we set its history such that
 * everything is set to taken. I.e., its choice predictor
 * chooses the taken array and the taken array predicts taken.
 */
void YagsBP::uncondBranch(ThreadID tid, Addr pc, void *&bp_history) {
  BPHistory *history = new BPHistory;
  history->globalHistoryReg = globalHistoryReg[tid];
  history->choicePred = true;
  history->takenUsed = false;
  history->notTakenUsed = true;
  history->notTakenPred = true;
  history->takenPred = true;
  history->finalPred = true;
  bp_history = static_cast<void *>(history);
}

void YagsBP::updateHistories(ThreadID tid, Addr pc, bool uncond, bool taken,
                             Addr target, const StaticInstPtr &inst,
                             void *&bp_history) {
  assert(uncond || bp_history);
  if (uncond) {
    uncondBranch(tid, pc, bp_history);
  }
  updateGlobalHistReg(tid, taken);
}

void YagsBP::squash(ThreadID tid, void *&bp_history) {
  BPHistory *history = static_cast<BPHistory *>(bp_history);
  globalHistoryReg[tid] = history->globalHistoryReg;

  delete history;
  bp_history = nullptr;
}

bool YagsBP::lookupTageCache(std::vector<CacheEntry> &cache,
                             std::vector<SatCounter8> &data, unsigned addr,
                             Addr tag, bool &hit) {
  hit = false;
  CacheEntry &ce = cache[addr];
  if (ce.match(addr)) {
    hit = true;
    return data[addr] > takenThreshold;
  }
  return false;
}
/*
 * Here we lookup the actual branch prediction. We use the PC to
 * identify the bias of a particular branch, which is based on the
 * prediction in the choice array. A hash of the global history
 * register and a branch's PC is used to index into both the taken
 * and not-taken predictors, which both present a prediction. The
 * choice array's prediction is used to select between the two
 * direction predictors for the final branch prediction.
 */
bool YagsBP::lookup(ThreadID tid, Addr branchAddr, void *&bp_history) {
  unsigned choiceHistoryIdx =
      ((branchAddr >> instShiftAmt) & choiceHistoryMask);
  unsigned globalHistoryIdx =
      (((branchAddr >> instShiftAmt) ^ globalHistoryReg[tid]) &
       globalHistoryMask);

  assert(choiceHistoryIdx < choicePredictorSize);
  assert(globalHistoryIdx < globalPredictorSize);

  bool choicePrediction = choiceCounters[choiceHistoryIdx] > choiceThreshold;
  bool takenCacheHit = false;
  bool notTakenCacheHit = false;
  bool takenCachePreiction = false;
  bool notTakenCachePreiction = false;
  bool takenCacheUsed = false;
  bool notTakenCacheUsed = false;

  notTakenCachePreiction =
      lookupTageCache(notTakenCache, notTakenCacheCounters, globalHistoryIdx,
                      branchAddr, notTakenCacheHit);
  takenCachePreiction =
      lookupTageCache(takenCache, takenCacheCounters, globalHistoryIdx,
                      branchAddr, takenCacheHit);

  bool finalPrediction = choicePrediction;
  notTakenCacheUsed = choicePrediction && notTakenCacheHit;
  takenCacheUsed = !choicePrediction && takenCacheHit;
  if (notTakenCacheUsed) {
    finalPrediction = notTakenCachePreiction;
  }

  if (takenCacheUsed) {
    finalPrediction = takenCachePreiction;
  }

  BPHistory *history = new BPHistory;
  history->globalHistoryReg = globalHistoryReg[tid];
  history->choicePred = choicePrediction;
  history->takenUsed = takenCacheUsed;
  history->notTakenUsed = notTakenCacheUsed;
  history->takenPred = takenCachePreiction;
  history->notTakenPred = notTakenCachePreiction;
  history->finalPred = finalPrediction;
  bp_history = static_cast<void *>(history);

  return finalPrediction;
}

/* Only the selected direction predictor will be updated with the final
 * outcome; the status of the unselected one will not be altered. The choice
 * predictor is always updated with the branch outcome, except when the
 * choice is opposite to the branch outcome but the selected counter of
 * the direction predictors makes a correct final prediction.
 */
void YagsBP::update(ThreadID tid, Addr branchAddr, bool taken,
                    void *&bp_history, bool squashed,
                    const StaticInstPtr &inst,
                    Addr target) {
  assert(bp_history);

  BPHistory *history = static_cast<BPHistory *>(bp_history);

  // We do not update the counters speculatively on a squash.
  // We just restore the global history register.
  if (squashed) {
    globalHistoryReg[tid] = (history->globalHistoryReg << 1) | taken;
    return;
  }

  unsigned choiceHistoryIdx =
      ((branchAddr >> instShiftAmt) & choiceHistoryMask);
  unsigned globalHistoryIdx =
      (((branchAddr >> instShiftAmt) ^ history->globalHistoryReg) &
       globalHistoryMask);

  assert(choiceHistoryIdx < choicePredictorSize);
  assert(globalHistoryIdx < globalPredictorSize);

  if (!history->choicePred && taken) {
    takenCacheCounters[globalHistoryIdx]++;
  } else {
    if (history->takenUsed) {
      // if the taken array's prediction was used, update it
      if (taken) {
        takenCacheCounters[globalHistoryIdx]++;
      } else {
        takenCacheCounters[globalHistoryIdx]--;
      }
    }

    if (history->choicePred && !taken) {
      notTakenCacheCounters[globalHistoryIdx]--;
    } else if (history->notTakenUsed) {
      // if the not-taken array's prediction was used, update it
      if (taken) {
        notTakenCacheCounters[globalHistoryIdx]++;
      } else {
        notTakenCacheCounters[globalHistoryIdx]--;
      }
    }
  }

  /* handle cache insertion */
  bool insertTakenCache = !history->takenUsed && !history->choicePred &&
                          (history->finalPred != taken);
  bool insertNotTakenCache = !history->notTakenUsed && history->choicePred &&
                             (history->finalPred != taken);

  if (insertTakenCache) {
    notTakenCache[globalHistoryIdx].invalidate();
    notTakenCache[globalHistoryIdx].insert(branchAddr);
    notTakenCacheCounters[globalHistoryIdx] =
        SatCounter8(tagCacheBits, choiceThreshold + 1);
  }

  if (insertNotTakenCache) {
    takenCache[globalHistoryIdx].invalidate();
    takenCache[globalHistoryIdx].insert(branchAddr);
    notTakenCacheCounters[globalHistoryIdx] =
        SatCounter8(tagCacheBits, choiceThreshold);
  }

  if (history->finalPred == taken) {
    /* If the final prediction matches the actual branch's
     * outcome and the choice predictor matches the final
     * outcome, we update the choice predictor, otherwise it
     * is not updated. While the designers of the bi-mode
     * predictor don't explicity say why this is done, one
     * can infer that it is to preserve the choice predictor's
     * bias with respect to the branch being predicted; afterall,
     * the whole point of the bi-mode predictor is to identify the
     * atypical case when a branch deviates from its bias.
     */
    if (history->finalPred == history->choicePred) {
      if (taken) {
        choiceCounters[choiceHistoryIdx]++;
      } else {
        choiceCounters[choiceHistoryIdx]--;
      }
    }
  } else {
    // always update the choice predictor on an incorrect prediction
    if (taken) {
      choiceCounters[choiceHistoryIdx]++;
    } else {
      choiceCounters[choiceHistoryIdx]--;
    }
  }

  delete history;
  bp_history = nullptr;
}

void YagsBP::updateGlobalHistReg(ThreadID tid, bool taken) {
  globalHistoryReg[tid] =
      taken ? (globalHistoryReg[tid] << 1) | 1 : (globalHistoryReg[tid] << 1);
  globalHistoryReg[tid] &= historyRegisterMask;
}

} // namespace branch_prediction
} // namespace gem5
