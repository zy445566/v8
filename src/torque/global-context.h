// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_GLOBAL_CONTEXT_H_
#define V8_TORQUE_GLOBAL_CONTEXT_H_

#include "src/torque/TorqueLexer.h"
#include "src/torque/TorqueParser.h"
#include "src/torque/declarable.h"
#include "src/torque/declarations.h"
#include "src/torque/scope.h"
#include "src/torque/type-oracle.h"

namespace v8 {
namespace internal {
namespace torque {

class GlobalContext;
class Scope;
class TypeOracle;

class Module {
 public:
  explicit Module(const std::string& name) : name_(name) {}
  const std::string& name() const { return name_; }
  std::ostream& source_stream() { return source_stream_; }
  std::ostream& header_stream() { return header_stream_; }
  std::string source() { return source_stream_.str(); }
  std::string header() { return header_stream_.str(); }

 private:
  std::string name_;
  std::stringstream header_stream_;
  std::stringstream source_stream_;
};

class OperationHandler {
 public:
  std::string macro_name;
  ParameterTypes parameter_types;
  Type result_type;
};

struct SourceFileContext {
  std::string name;
  std::unique_ptr<antlr4::ANTLRFileStream> stream;
  std::unique_ptr<TorqueLexer> lexer;
  std::unique_ptr<antlr4::CommonTokenStream> tokens;
  std::unique_ptr<TorqueParser> parser;
  TorqueParser::FileContext* file;

  std::string sourceFileAndLineNumber(antlr4::ParserRuleContext* context) {
    antlr4::misc::Interval i = context->getSourceInterval();
    auto token = tokens->get(i.a);
    size_t line = token->getLine();
    size_t pos = token->getCharPositionInLine();
    return name + ":" + std::to_string(line) + ":" + std::to_string(pos);
  }
};

class GlobalContext {
 public:
  explicit GlobalContext(Ast ast)
      : verbose_(false),
        next_label_number_(0),
        declarations_(ast.source_file_map()),
        type_oracle_(&declarations_),
        default_module_(GetModule("base")),
        ast_(std::move(ast)) {}
  Module* GetDefaultModule() { return default_module_; }
  Module* GetModule(const std::string& name) {
    auto i = modules_.find(name);
    if (i != modules_.end()) {
      return i->second.get();
    }
    Module* module = new Module(name);
    modules_[name] = std::unique_ptr<Module>(module);
    return module;
  }

  int GetNextLabelNumber() { return next_label_number_++; }

  const std::map<std::string, std::unique_ptr<Module>>& GetModules() const {
    return modules_;
  }

  void SetVerbose() { verbose_ = true; }
  bool verbose() const { return verbose_; }

  void AddControlSplitChangedVariables(const AstNode* node,
                                       const std::set<const Variable*>& vars) {
    control_split_changed_variables_[node] = vars;
  }

  const std::set<const Variable*>& GetControlSplitChangedVariables(
      const AstNode* node) {
    assert(control_split_changed_variables_.find(node) !=
           control_split_changed_variables_.end());
    return control_split_changed_variables_.find(node)->second;
  }

  void MarkVariableChanged(const AstNode* node, Variable* var) {
    control_split_changed_variables_[node].insert(var);
  }

  friend class CurrentCallableActivator;
  friend class BreakContinueActivator;

  TypeOracle& GetTypeOracle() { return type_oracle_; }

  Callable* GetCurrentCallable() const { return current_callable_; }
  Label* GetCurrentBreak() const { return break_continue_stack_.back().first; }
  Label* GetCurrentContinue() const {
    return break_continue_stack_.back().second;
  }

  std::map<std::string, std::vector<OperationHandler>> op_handlers_;

  std::string PositionAsString(SourcePosition pos) {
    return declarations()->PositionAsString(pos);
  }

  Declarations* declarations() { return &declarations_; }
  Ast* ast() { return &ast_; }

 private:
  bool verbose_;
  int next_label_number_;
  Declarations declarations_;
  TypeOracle type_oracle_;
  std::map<std::string, std::unique_ptr<Module>> modules_;
  Module* default_module_;
  std::vector<std::pair<Label*, Label*>> break_continue_stack_;
  Callable* current_callable_;
  std::map<const AstNode*, std::set<const Variable*>>
      control_split_changed_variables_;
  Ast ast_;
};

class CurrentCallableActivator {
 public:
  CurrentCallableActivator(GlobalContext& context, Callable* callable,
                           Declaration* decl)
      : context_(context), scope_activator_(context.declarations(), decl) {
    context_.current_callable_ = callable;
  }
  ~CurrentCallableActivator() { context_.current_callable_ = nullptr; }

 private:
  GlobalContext& context_;
  Declarations::NodeScopeActivator scope_activator_;
};

class BreakContinueActivator {
 public:
  BreakContinueActivator(GlobalContext& context, Label* break_label,
                         Label* continue_label)
      : context_(context) {
    context_.break_continue_stack_.push_back({break_label, continue_label});
  }
  ~BreakContinueActivator() { context_.break_continue_stack_.pop_back(); }

 private:
  GlobalContext& context_;
};

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_GLOBAL_CONTEXT_H_
