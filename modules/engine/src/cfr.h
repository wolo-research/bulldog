//
// Created by Isaac Zhang on 2/26/20.
//

#ifndef BULLDOG_MODULES_ENGINE_SRC_CFR_H_
#define BULLDOG_MODULES_ENGINE_SRC_CFR_H_

#include <utility>

#include "cfr_worker.h"
#include "cfr_command.hpp"

const int CFR_SOLVING_TERMINATED_ASYNC = 3;
const int CFR_SOLVING_TERMINATED_ALL_COMMANDS = 1;
const int CFR_SOLVING_TERMINATED_EARLY = 2;
const int CFR_SOLVING_TERMINATED_ERROR = 4;
const int CFR_SOLVING_TERMINATED_ASYNC_CHECKPOINT = 10;

static std::map<int, std::string> CFR_RESULT_MAP = {
    {CFR_SOLVING_TERMINATED_ALL_COMMANDS, "CFR_SOLVING_TERMINATED_ALL_COMMANDS"},
    {CFR_SOLVING_TERMINATED_EARLY, "CFR_SOLVING_TERMINATED_EARLY"},
    {CFR_SOLVING_TERMINATED_ASYNC, "CFR_SOLVING_TERMINATED_ASYNC"},
    {CFR_SOLVING_TERMINATED_ERROR, "CFR_SOLVING_TERMINATED_ERROR"}
};

std::string PrintCfrResultCode(int code);

struct sThreadOutput {
  sThreadOutput() = default;

  double avg_util_;
  double std_dev_;
  double max_util_ = -99999999999;
  double min_util_ = 99999999999;
  double *array_ = nullptr;
  int size_;
  int cursor_ = 0;

  void AddIterResult(double local_util) {
    array_[cursor_++] = local_util;
    if (local_util > max_util_) max_util_ = local_util;
    if (local_util < min_util_) min_util_ = local_util;
  }

  void Prepare(int size) {
    size_ = size;
    array_ = new double[size_];
    for (int i = 0; i < size_; i++)
      array_[i] = -1;
  }

  void Process() {
    double sum = 0;
    for (auto i = 0; i < size_; i++)
      if (array_[i] != -1)
        sum += array_[i];
    avg_util_ = sum / size_;

    double variance = 0.0;
    for (auto i = 0; i < size_; i++)
      if (array_[i] != -1)
        variance += (array_[i] - avg_util_) * (array_[i] - avg_util_);
    std_dev_ = std::sqrt(variance / size_);
    return;
  }

  virtual ~sThreadOutput() {
    delete[] array_;
  }
};

struct sTotalThreadOutput {
  double avg_ = 0.0;
  double max_ = -99999999999;
  double min_ = 99999999999;
  double std_dev_ = 0.0;

  void MergeThreadOutputs(sThreadOutput *outputs, int num_thread) {
    int effective_thread = num_thread;
    //compute mean and max and min
    for (int i = 0; i < num_thread; ++i) {
      if (outputs->array_ == nullptr) {
        effective_thread--;
        continue;
      }
      avg_ += outputs[i].avg_util_;
      std_dev_ += outputs[i].std_dev_;
      if (outputs[i].max_util_ > max_) max_ = outputs[i].max_util_;
      if (outputs[i].min_util_ < min_) min_ = outputs[i].min_util_;
    }
    avg_ /= effective_thread;
    std_dev_ /= effective_thread;
  }
};

/**
 * thread arguments variables
 */
struct sThreadInput {
  sThreadInput(Strategy *blueprint,
               Strategy *strategy,
               std::vector<Board_t> *pub_bucket_flop_boards,
               int thread_idx,
               sCfrParam cfr_param,
               std::vector<int> thread_board,
               sThreadOutput *output,
               int iterations,
               const std::atomic_bool &cancelled_token,
               unsigned long long rnd_seed)
      : blueprint_(blueprint),
        strategy_(strategy),
        pub_bucket_flop_boards_(pub_bucket_flop_boards),
        thread_idx_(thread_idx),
        cfr_param_(std::move(cfr_param)),
        thread_board_(std::move(thread_board)),
        output_(output),
        iterations_(iterations),
        cancelled_token_(cancelled_token),
        seed_(rnd_seed) {
  }
  Strategy *blueprint_;
  Strategy *strategy_;
  std::vector<Board_t> *pub_bucket_flop_boards_;
  int thread_idx_;
  sCfrParam cfr_param_;
  //
  std::vector<int> thread_board_;
  sThreadOutput *output_;
  int iterations_;
  CFR_COMMAND cfr_mode_;
  const std::atomic_bool &cancelled_token_;
  unsigned long long seed_;
};

/**
 * all CFR implementation included.
 */
class CFR {
 public:
  explicit CFR(const char *config_file);

  sCfrParam cfr_param_;
  sProfilingWriter profiling_writer_;
  pthread_t *thread_pool_;

  std::deque<CfrCommandWrapper> commands_{};

  ~CFR() {
    delete[] thread_pool_;
    logger::debug("graceful shutdown | cfr");
  }

  int Solve(Strategy *blueprint,
            Strategy *strategy,
            const sCFRState &convergence,
            const std::atomic_bool &cancelled,
            int starting_checkpoint = 0);

  static void *CfrSolve(void *thread_args);

  static void AllocateFlops(std::vector<Board_t> *pub_flop_boards,
                            std::vector<int> *thread_board,
                            int num_thread,
                            int num_flop_cards);

  void BuildCMDPipeline();
  static int AsyncCfrSolving(CFR *cfr,
                             Strategy *new_strategy,
                             Strategy *blueprint,
                             sCFRState *convergence_state,
                             const std::atomic_bool &cancelled,
                             int cfr_checkpoint = 0);
 private:
  static void ThreadedCfrSolve(Strategy *blueprint,
                               Strategy *strategy,
                               sCfrParam &cfr_param,
                               sCFRState &current_state,
                               int steps,
                               sTotalThreadOutput &total_result,
                               std::vector<int> *thread_board,
                               std::vector<Board_t> *pub_bucket_flop_boards,
                               pthread_t *thread_pool,
                               const std::atomic_bool &cancelled);

  void Config(web::json::value data);
};

#endif //BULLDOG_MODULES_ENGINE_SRC_CFR_H_