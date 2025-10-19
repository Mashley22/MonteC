#ifndef MONTEC_MONTEC_HPP
#define MONTEC_MONTEC_HPP

#include <atomic>
#include <array>
#include <thread>

#include "sum.hpp"

#define MONTEC_BLOCK_COUNT 1024

#define MONTEC_DIVIDE_ROUND_UP(num, div) (num/div + 1)
#define MONTEC_ITER_TO_BLOCK(iter) MONTEC_DIVIDE_ROUND_UP(iter, MONTEC_BLOCK_COUNT)

namespace mc {

namespace detail {

template<class Func, class Cond, class input_t, class out_t, int thread_num>
class MonteThread;

}

template<class Func, class Cond, class input_t, class out_t, int thread_num>
class Monte {
private:
  static std::size_t m_totalBlockNum;
  static std::atomic<std::size_t> m_handledBlockCount;

  static detail::MonteThread<Func, Cond, input_t, out_t, thread_num> m_mainThread;
  static std::array<detail::MonteThread<Func, Cond, input_t, out_t, thread_num>, thread_num - 1> m_threads;

  out_t proccessResult(void) {

  }

public:
  Monte() = delete;

  Monte(std::size_t iteration_count) noexcept
  {
    m_totalBlockNum = iteration_count ;
    m_handledBlockCount = 0;
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

    return result / m_totalBlockNum;
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

template<class Func, class Cond, class input_t, class out_t, int thread_num>
class MonteThread {
private:
  using Monty = Monte<Func, Cond, input_t, out_t, thread_num>;
  std::size_t m_blockCount{0};
  Sum<out_t> m_sum;
  std::array<input_t, MONTEC_BLOCK_COUNT> m_inputs;
  std::jthread m_thread;
  std::atomic_bool m_active{false};

public:

  out_t weightedSum(void) const noexcept {
    return m_sum.val() * m_blockCount;
  }

  void genInputBlock(void) noexcept {
    for (auto& input : m_inputs) {
      input.gen();
    }
  }

  void proccessBlock(void) noexcept {
    for (const auto& input : m_inputs) {
      m_sum.add(Func(input));
    }
  }

private:
  void mainLoop(void) noexcept {
    while (Monty::get_nextBlock() == true) {
      m_blockCount++;
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
