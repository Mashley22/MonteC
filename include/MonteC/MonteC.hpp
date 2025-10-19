#ifndef MONTEC_MONTEC_HPP
#define MONTEC_MONTEC_HPP

#include <atomic>
#include <array>
#include <thread>
#include <vector>
#include <optional>

#include "sum.hpp"

#define MONTEC_BLOCK_COUNT 1024

#define MONTEC_DIVIDE_ROUND_UP(num, div) (num/div + 1)
#define MONTEC_ITER_TO_BLOCK(iter) MONTEC_DIVIDE_ROUND_UP(iter, MONTEC_BLOCK_COUNT)

namespace mc {

namespace detail {

template<class Func, class input_t, class out_t>
class MonteThread;

}

template<class Func, class input_t, class out_t>
class Monte {
private:
  static std::size_t m_totalBlockNum;
  static std::atomic<std::size_t> m_handledBlockCount;

  static detail::MonteThread<Func, input_t, out_t> m_mainThread;
  static std::vector<detail::MonteThread<Func, input_t, out_t>> m_threads;

public:
  Monte() = default;

  static std::size_t iterations(void) {
    return m_totalBlockNum * MONTEC_BLOCK_COUNT;
  }

  static void set_params(std::size_t iteration_count, std::size_t thread_num) noexcept {
    m_totalBlockNum = MONTEC_ITER_TO_BLOCK(iteration_count);
    m_handledBlockCount = 0;
    // minus 1 since main thread is handled by iteself
    m_threads.resize(thread_num - 1);
  }

  Monte(std::size_t iteration_count, std::size_t thread_num) noexcept {
    set_params(iteration_count, thread_num);
  }


  static out_t run(void) noexcept {
    for (auto& thread : m_threads) {
      thread.start();
    }
    m_mainThread.start();
    
    {
    bool done = false;
    while (done == false) {
      done = true;
      for (const auto& thread : m_threads) {
        if (thread.active() == true) {
          done = false;
        }
      }
    }
    }
    
    out_t result = m_mainThread.weightedSum();
    for (const auto& thread : m_threads) {
      result += thread.weightedSum();
    }

    return result / (m_totalBlockNum * MONTEC_BLOCK_COUNT);
  }
  
  /**@brief returns whether the thread should proccess another block
   */
  static std::size_t get_nextBlock(void) noexcept {
    int batch_idx = m_handledBlockCount++;
    if (batch_idx >= m_handledBlockCount) {
      return false;
    }
    else {
      return true;
    }
  }
  
};

namespace detail {

template<class Func, class input_t, class out_t>
class MonteThread {
private:
  using Monty = Monte<Func, input_t, out_t>;
  // only counts successful iterations based on the Cond functor
  std::size_t m_iterCount{0};
  Sum<out_t> m_sum;
  std::array<input_t, MONTEC_BLOCK_COUNT> m_inputs;
  std::jthread m_thread;
  std::atomic_bool m_active{false};

public:

  out_t weightedSum(void) const noexcept {
    return m_sum.val() * m_iterCount;
  }

  void genInputBlock(void) noexcept {
    for (auto& input : m_inputs) {
      input.gen();
    }
  }

  void proccessBlock(void) noexcept {
    for (const auto& input : m_inputs) {
      std::optional<out_t> res = Func(input);
      if (res.has_value()) {
        m_iterCount++;
        m_sum.add(res.value());
      }
    }
  }

private:
  void mainLoop(void) noexcept {
    while (Monty::get_nextBlock() == true) {
      genInputBlock();
      proccessBlock();
    }
  }

public:
  void start(void) noexcept {
    m_active.store(true, std::memory_order_seq_cst);
    m_thread = std::jthread(this->mainLoop);
    m_active.store(false, std::memory_order_seq_cst);
  }

  bool active(void) const noexcept {
    return m_active.load(std::memory_order_seq_cst);
  }

};

}

}

#endif /* MONTEC_MONTEC_HPP */
