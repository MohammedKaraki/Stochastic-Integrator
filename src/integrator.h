#include <array>
#include <vector>
#include <utility>
#include <string>
#include <cmath>
#include <random>
#include <numbers>
#include <limits>
#include <boost/circular_buffer.hpp>


namespace integrator {

  //////////////////////////////////////////////////////////////////////////////
  // Pools for random expression generation:
  // ///////////////////////////////////////////////////////////////////////////

  // Nullary operators, i.e. symbols:
  static constexpr auto nullary_pool = std::array{'1', 'x'};

  // Unary operators, i.e. single-variable functions:
  static constexpr auto unary_pool = std::array{
    '\\', '~', '>', '<', 'C', 'S', '2', 'R', 'L', 'H'};

  // Binary operators:
  static constexpr auto binary_pool = std::array{'+', '-', '/', '*'};


  // Capacity of the circular buffer storing the composed function
  static constexpr auto expr_max_size = 64u;


  // Random number generation lies at the heart of a very hot loop.
  // This class implements the "xor" algorithm from Page 4 of
  // Marsaglia, "Xorshift RNGs."
  class CustomGenerator {
  public:
    CustomGenerator(uint32_t seed) : state(seed) { assert(seed != 0); }

    using result_type = int;
    static constexpr result_type max() {
      return std::numeric_limits<result_type>::max();
    }
    static constexpr result_type min() {
      return 0;
    }
    result_type operator()() {
      return xorshift32();
    }

  private:
    uint32_t state;

    uint32_t xorshift32()
    {
      uint32_t x = state;
      x ^= x << 13;
      x ^= x >> 17;
      x ^= x << 5;
      return state = x;
    }
  };


  //////////////////////////////////////////////////////////////////////////////
  // This class is responsible for composing and evaluating expressions.
  //////////////////////////////////////////////////////////////////////////////

  class Composer;
  using MemberFuncPtr = void(Composer::*)();

  class Composer {
  public:
    Composer(unsigned int rng_seed)
      : stack(expr_max_size),
      rng{rng_seed}
    { }


    std::pair<std::string, std::vector<MemberFuncPtr>>
    compose(int tentative_len)
    {
      int len = (rng() % tentative_len) + 2;
      auto raw_expr = gen_random_expr(len);
      return {raw_expr, compile(raw_expr)};
    }


    double eval(const std::vector<MemberFuncPtr>& compiled_expr, double x)
    {
      x_value = x;
      return eval(compiled_expr);
    }


    std::string gen_random_expr(int len)
    {
      std::string result;


      int stack_size = 0; // Used to ensure that the generated expression,
      // after getting fully executed, leaves a stack
      // with size 1, i.e., a stack containing only
      // the return value.

      for (int i = 0; i < len; ++i) {
        int roof = stack_size>=2 ? 3 : stack_size+1;
        int choice = rng() % roof;

        if (i == len-1)
        {
          if (stack_size == 1)
            choice = 1;
          else
            choice = 2;
        }

        switch (choice) {
          case 0:
            result += draw_random_symbol(nullary_pool);
            stack_size++;
            break;
          case 1:
            result += draw_random_symbol(unary_pool);
            break;
          case 2:
            result += draw_random_symbol(binary_pool);
            stack_size--;
            break;
        }
      }

      while (stack_size > 1) {
        result += draw_random_symbol(binary_pool);
        stack_size--;
      }

      return result;
    }


    // Compile means to transform a sequence of chars into a sequence of
    // function pointers.
    std::vector<MemberFuncPtr> compile(const std::string& expr)
    {
      std::vector<MemberFuncPtr> ret;

      for (char c : expr) {
        ret.push_back(operator_dict(c));
      }

      return ret;
    }


  private:
    double x_value;
    boost::circular_buffer<double> stack; // Stack of operands
    // std::ranlux24_base rng;
    CustomGenerator rng;


    static MemberFuncPtr operator_dict(char c)
    {
      switch (c) {
        case 'x': return &Composer::x;
        case '0': return &Composer::zero;
        case '1': return &Composer::one;
        case'\\': return &Composer::invert;
        case '~': return &Composer::invert_sign;
        case '>': return &Composer::increment;
        case '<': return &Composer::decrement;
        case 'S': return &Composer::sin;
        case 'C': return &Composer::cos;
        case 'T': return &Composer::tan;
        case '2': return &Composer::square;
        case 'R': return &Composer::root;
        case 'L': return &Composer::log;
        case 'H': return &Composer::halve;
        case '+': return &Composer::add;
        case '-': return &Composer::subtract;
        case '*': return &Composer::multiply;
        case '/': return &Composer::divide;
      }
      assert(false);
    }


  private:
    template<typename F>
    void apply_unary(const F& f)
    {
      f(stack.back());
    }

    template<typename F>
    void apply_binary(const F& f)
    {
      double back = stack.back();
      stack.pop_back();
      f(stack.back(), back);
    }

    void x() { stack.push_back(x_value); }   // push var to stack
    void zero() { stack.push_back(0.0); }    // push the number 0
    void one() { stack.push_back(1.0); }     // push the number 1

    void invert() { apply_unary([](double& v) { v = 1.0 / v; }); }
    void invert_sign() { apply_unary([](double& v) { v *= -1.0; }); }
    void increment() { apply_unary([](double& v) { v += 1.0; }); }
    void decrement() { apply_unary([](double& v) { v -= 1.0; }); }
    void sin() { apply_unary([](double& v) { v = std::sin(v); }); }
    void cos() { apply_unary([](double& v) { v = std::cos(v); }); }
    void tan() { apply_unary([](double& v) { v = std::tan(v); }); }
    void square() { apply_unary([](double& v) { v *= v; }); }
    void root() { apply_unary([](double& v) { v = std::sqrt(v); }); }
    void log() { apply_unary([](double& v) { v = std::log(v); }); }
    void halve() { apply_unary([](double& v) { v /= 2.0; }); }

    void add() { apply_binary([](double& a, const double& b) { a += b; }); }
    void subtract() { apply_binary([](double& a, const double& b) { a -= b; }); }
    void multiply() { apply_binary([](double& a, const double& b) { a *= b; }); }
    void divide() { apply_binary([](double& a, const double& b) { a /= b; }); }

    // returns a random member of an array
    template<typename T>
    char draw_random_symbol(const T& arr)
    {
      return arr[rng() % arr.size()];
    }


    double eval(const std::vector<MemberFuncPtr>& compiled_expr)
    {
      for (auto func : compiled_expr) {
        (this->*func)();
      }

      double ret = stack.back();
      stack.pop_back();
      return ret;
    }
  };


} // namespace integrator
