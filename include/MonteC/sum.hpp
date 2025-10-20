#ifndef MONTEC_SUM_HPP
#define MONTEC_SUM_HPP

namespace mc {

template<class T>
class Sum {
public:
  Sum(void) = default;
  Sum(const T& val) : m_sum(val) {}

  T val(void) const noexcept {  return m_sum; }

  void add(const T& val) {
    // Kahan
    T y = val - m_compensation;
    T t = m_sum + y;
    m_compensation = (t - m_sum) - y;
    m_sum = t;
  };

  void reset(void) noexcept {
    m_sum.reset();
  }

private:
  T m_sum;
  T m_compensation;
};

}

#endif /* MONTEC_SUM_HPP */
