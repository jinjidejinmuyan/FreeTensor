#ifndef FREE_TENSOR_RAND_CTX_H
#define FREE_TENSOR_RAND_CTX_H

#include <map>
#include <mutex>
#include <regex>
#include <string>
#include <unordered_map>

#include <func_utils.h>
#include <probability/rand_cond.h>
#include <probability/rand_var.h>

namespace freetensor {

struct ProgramPositionHelper {};
typedef const ProgramPositionHelper *ProgramPosition;

/**
 * Uniquely marking a program position
 */
#define PROGRAM_POSITION                                                       \
    ([]() -> ProgramPosition {                                                 \
        static ProgramPositionHelper p;                                        \
        return &p;                                                             \
    })()

typedef std::vector<DiscreteObservation> RandTrace;

/**
 * Non-template base class for `RandCtx`
 */
class RandCtxImpl {
  protected:
    std::unordered_map<ProgramPosition,
                       std::unordered_map<RandCondStack, Ref<DiscreteRandVar>>>
        randVars_;

    std::multimap<Ref<RandTrace>, std::pair<double, double>,
                  PtrInvocable<std::less<RandTrace>>>
        traces_; // Ordered map

    bool isInfer_ = true;
    std::regex toLearn_{".*"};

    std::mutex lock_;

  public:
    /**
     * Learn from a trace
     *
     * This trace is compared with existing traces. For each two trace being
     * compared, the first different decision will be found, whose random
     * variable will be upadated to perfer the decision from the trace of the
     * lower value
     *
     * This function must be called from the master thread
     */
    void observeTrace(const Ref<RandTrace> &trace, double value, double stddev);

    /**
     * Set to learn some of the random variables only
     *
     * A pattern can be set, and then only the variables matching this pattern
     * is set to learning. This is useful when testing some of the random
     * decisions with few learning trials
     *
     * This function must be called from the master thread
     */
    void setLearnFilter(const std::regex &toLearn) { toLearn_ = toLearn; }

    /**
     * When `decide` later, randomly sample a decision
     *
     * This function must be called from the master thread
     */
    void setLearn() { isInfer_ = false; }

    /**
     * When `decide` later, pick a most likely decision
     *
     * This function must be called from the master thread
     */
    void setInfer() { isInfer_ = true; }
};

/**
 * Context to do random decisions
 *
 * If `setLearn`ed, sample a decision and use the decision traces to learn a
 * Bayesian model. Each trace is labeled with a performance value, and traces
 * are compared pairwisely (lower is better), to learn P(this decision leads to
 * better performance).
 */
template <std::uniform_random_bit_generator RNG>
class RandCtx : public RandCtxImpl {
    RNG &rng_;

  public:
    RandCtx(RNG &rng) : rng_(rng) {}

    /**
     * Get decision from a random variable uniquely defined by conditions and
     * the program position
     *
     * If learning, sample a random variable and record it as a trace. If
     * infering, pick a most likely decision
     *
     * This function is thread-safe
     */
    int decide(ProgramPosition pos, const std::string &name,
               const RandCondStack &condStack, const std::vector<int> &initObs,
               const Ref<RandTrace> &trace, const std::string &message = "") {
        std::lock_guard<std::mutex> guard(lock_);
        if (!randVars_.count(pos) || !randVars_.at(pos).count(condStack)) {
            randVars_[pos][condStack] =
                Ref<DiscreteRandVar>::make(name, condStack, initObs);
        }
        auto &&var = randVars_.at(pos).at(condStack);
        auto value = isInfer_ || !std::regex_match(var->name(), toLearn_)
                         ? var->mostLikely()
                         : var->sample(rng_);
        if (trace.isValid()) {
            trace->emplace_back(var, value, message);
        }
        return value;
    }
};

} // namespace freetensor

#endif // FREE_TENSOR_RAND_CTX_H
