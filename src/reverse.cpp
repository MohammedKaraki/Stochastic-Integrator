///////////////////////////////////////////////////////////////////////////////
/////////////////////////       reverse.cpp      //////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// The purpose of this file is to convert an expression from reverse polish
// notation to infix notation, which is more human-readable. As an intermediate
// step, it converts the input string into an abstract syntax tree and performs
// few simplifications on it, before it finally converts this AST into
// infix notation.
//
///////////////////////////////////////////////////////////////////////////////


#include <utility>
#include <stack>
#include <cassert>
#include <iostream>
#include <string>
#include <cmath>
#include <fmt/format.h>


class AST {
public:
  virtual std::string to_string() = 0;
  virtual ~AST() { }
};


struct Operator {
  const char symbol;
  const int precedence;

  Operator(char symbol) : symbol{symbol}, precedence{eval_precedence(symbol)}
  { }


private:
  static int eval_precedence(char symbol)
  {
    switch (symbol) {
      case '+':
      case '-':
        return 50;
      case '*':
      case '/':
        return 60;
      case '^':
        return 70;
    }

    assert(false && "Undefined operator symbol");
  }
};


class VariableAST : public AST {
  std::string name;

public:
  VariableAST(std::string name) : name{std::move(name)}
  { }

  std::string to_string() override
  {
    return name;
  }
};

class IntAST : public AST {
  int value;

public:
  IntAST(int value) : value{value}
  { }

  std::string to_string() override
  {
    return std::to_string(value);
  }

  auto get_value() { return value; }
};


class FuncAST : public AST {
  std::string func_name;
  std::unique_ptr<AST> argument;

public:
  FuncAST(char func_symbol, std::unique_ptr<AST> argument)
    : func_name{eval_func_name(func_symbol)},
    argument{std::move(argument)}
  { }

  std::string to_string() override
  {
    return fmt::format("{}({})", func_name, argument->to_string());
  }

  auto& get_argument() { return argument; }

private:
  static std::string eval_func_name(char symbol)
  {
    switch (symbol) {
      case 'S':
        return "sin";
      case 'C':
        return "cos";
      case 'T':
        return "tan";
      case 'R':
        return "sqrt";
      case 'L':
        return "log";
    }
    assert(false && "Undefined function symbol");
  }
};


class BinaryOperatorAST : public AST {
  Operator op;
  std::unique_ptr<AST> lhs, rhs;

public:
  BinaryOperatorAST(Operator op,
                    std::unique_ptr<AST> lhs,
                    std::unique_ptr<AST> rhs)
    : op{op}, lhs{std::move(lhs)}, rhs{std::move(rhs)}
  { }

  std::string to_string() override
  {
    auto lhs_str = lhs->to_string();
    auto rhs_str = rhs->to_string();

    if (auto p = dynamic_cast<BinaryOperatorAST*>(lhs.get())) {
      if (p->op.precedence < op.precedence) {
        lhs_str = fmt::format("({})", std::move(lhs_str));
      }
    }

    if (auto p = dynamic_cast<BinaryOperatorAST*>(rhs.get())) {
      if (p->op.precedence < op.precedence) {
        rhs_str = fmt::format("({})", std::move(rhs_str));
      }
    }

    // Division and subtraction aren't associative.
    if (op.symbol == '/' || op.symbol == '-') {
      if (auto p = dynamic_cast<BinaryOperatorAST*>(rhs.get())) {
        if (p->op.symbol == op.symbol) {
          rhs_str = fmt::format("({})", std::move(rhs_str));
        }
      }
    }

    return fmt::format("{} {} {}", lhs_str, op.symbol, rhs_str);
  }

  auto& get_lhs() { return lhs; }
  auto& get_rhs() { return rhs; }
  auto get_operator_symbol() { return op.symbol; }
};


class NegativeOperatorAST : public AST {
  std::unique_ptr<AST> rhs;

public:
  NegativeOperatorAST(std::unique_ptr<AST> rhs)
    : rhs{std::move(rhs)}
  { }

  std::string to_string() override
  {
    if (dynamic_cast<BinaryOperatorAST*>(rhs.get())) {
      return fmt::format("-({})", rhs->to_string());
    }
    return fmt::format("-{}", rhs->to_string());
  }

  auto& get_rhs() { return rhs; }
};


static std::unique_ptr<AST> parse_reverse_polish(std::string_view src)
{
  auto stack = std::stack<std::unique_ptr<AST>>{};

  for (; !src.empty(); src = src.substr(1)) {
    switch (src.front()) {
      case '0':
        stack.push(std::make_unique<IntAST>(0));
        break;
      case '1':
        stack.push(std::make_unique<IntAST>(1));
        break;
      case 'x':
      case 'y':
      case 'z':
      case 'a':
      case 'b':
      case 'c':
        stack.push(std::make_unique<VariableAST>(std::string(1, src.front())));
        break;

      case 'S':
      case 'C':
      case 'T':
      case 'R':
      case 'L': {
        auto arg = std::unique_ptr(std::move(stack.top()));
        stack.pop();

        stack.push(std::make_unique<FuncAST>(std::move(src.front()),
                                             std::move(arg)));
        break;
      }

      case '+':
      case '-':
      case '*':
      case '/': {
        auto rhs = std::unique_ptr(std::move(stack.top()));
        stack.pop();
        auto lhs = std::unique_ptr(std::move(stack.top()));
        stack.pop();
        auto op = Operator(src.front());
        stack.push(std::make_unique<BinaryOperatorAST>(op,
                                                       std::move(lhs),
                                                       std::move(rhs)));
        break;
      }
      case '\\': {
        auto lhs = std::make_unique<IntAST>(1);
        auto rhs = std::unique_ptr(std::move(stack.top()));
        stack.pop();
        stack.push(std::make_unique<BinaryOperatorAST>('/',
                                                       std::move(lhs),
                                                       std::move(rhs)));
        break;
      }
      case 'H': {
        auto lhs = std::unique_ptr(std::move(stack.top()));
        stack.pop();
        auto rhs = std::make_unique<IntAST>(2);
        stack.push(std::make_unique<BinaryOperatorAST>('/',
                                                       std::move(lhs),
                                                       std::move(rhs)));
        break;
      }
      case '<': {
        auto lhs = std::unique_ptr(std::move(stack.top()));
        stack.pop();
        auto rhs = std::make_unique<IntAST>(1);
        stack.push(std::make_unique<BinaryOperatorAST>('-',
                                                       std::move(lhs),
                                                       std::move(rhs)));
        break;
      }
      case '>': {
        auto lhs = std::unique_ptr(std::move(stack.top()));
        stack.pop();
        auto rhs = std::make_unique<IntAST>(1);
        stack.push(std::make_unique<BinaryOperatorAST>('+',
                                                       std::move(lhs),
                                                       std::move(rhs)));
        break;
      }
      case '2': {
        auto lhs = std::unique_ptr(std::move(stack.top()));
        stack.pop();
        auto rhs = std::make_unique<IntAST>(2);
        stack.push(
          std::make_unique<BinaryOperatorAST>('^',
                                              std::move(lhs),
                                              std::move(rhs)));
        break;
      }
      case '~': {
        auto rhs = std::unique_ptr(std::move(stack.top()));
        stack.pop();
        stack.push(std::make_unique<NegativeOperatorAST>(
            std::move(rhs)));
        break;
      }
      default:
        assert(false);
        break;
    }
  }

  assert(stack.size() == 1);
  return std::move(stack.top());
}

static bool visit(std::unique_ptr<AST>& node)
{
  // Minus Int = another Int
  // Minus Minus Something = Something
  if (auto p = dynamic_cast<NegativeOperatorAST*>(node.get())) {
    if (auto r = dynamic_cast<IntAST*>(p->get_rhs().get())) {
      auto new_value = -r->get_value();
      node = std::make_unique<IntAST>(new_value);
      return true;
    }
    if (auto r = dynamic_cast<NegativeOperatorAST*>(p->get_rhs().get())) {
      node = std::move(r->get_rhs());
      return true;
    }
  }

  // Evaluate operators acting on integers (only if result is an integer)
  if (auto p = dynamic_cast<BinaryOperatorAST*>(node.get())) {
    if (auto l = dynamic_cast<IntAST*>(p->get_lhs().get())) {
      if (auto r = dynamic_cast<IntAST*>(p->get_rhs().get())){
        switch (p->get_operator_symbol()) {
          case '+': {
            auto new_value = l->get_value() + r->get_value();
            node = std::make_unique<IntAST>(new_value);
            return true;
          }
          case '-': {
            auto new_value = l->get_value() - r->get_value();
            node = std::make_unique<IntAST>(new_value);
            return true;
          }
          case '*': {
            auto new_value = l->get_value() * r->get_value();
            node = std::make_unique<IntAST>(new_value);
            return true;
          }
          case '/': {
            auto a = l->get_value();
            auto b = r->get_value();
            if (a % b == 0) {
              auto new_value = a / b;
              node = std::make_unique<IntAST>(new_value);
              return true;
            }
          }
          case '^': {
            auto new_value = std::pow(l->get_value(), r->get_value());
            node = std::make_unique<IntAST>(new_value);
            return true;
          }
        }
      }
    }
  }


  // The integer 1 is the multiplication identity.
  if (auto p = dynamic_cast<BinaryOperatorAST*>(node.get())) {
    if (auto lhs = dynamic_cast<IntAST*>(p->get_lhs().get())) {
      if (lhs->get_value() == 1) {
        node = std::move(p->get_rhs());
        return true;
      }
    }
    if (auto rhs = dynamic_cast<IntAST*>(p->get_rhs().get())) {
      if (rhs->get_value() == 1) {
        node = std::move(p->get_lhs());
        return true;
      }
    }
  }

  return false; // Node has not been modified
}


static void simplify_pass_helper(std::unique_ptr<AST>& root, bool& modified)
{
  if (root) {
    if (auto p = dynamic_cast<FuncAST*>(root.get())) {
      simplify_pass_helper(p->get_argument(), modified);
    }
    if (auto p = dynamic_cast<BinaryOperatorAST*>(root.get())) {
      simplify_pass_helper(p->get_lhs(), modified);
      simplify_pass_helper(p->get_rhs(), modified);
    }
    if (visit(root)) {
      modified = true;
    }
  }
}


static bool simplify_pass(std::unique_ptr<AST>& root)
{
  auto modified = false;
  simplify_pass_helper(root, modified);
  return modified;
}


static void simplify(std::unique_ptr<AST>& root)
{
  do {} while (simplify_pass(root));
}


std::string infix_from_reverse_polish(std::string_view src)
{
  auto ast = parse_reverse_polish(src);
  simplify(ast);
  return ast->to_string();
}
