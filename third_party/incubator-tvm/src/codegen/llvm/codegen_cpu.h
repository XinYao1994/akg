/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*!
 * \file codegen_llvm_cpu.h
 * \brief Common base class for generating into LLVM IR on CPU host.
 */

/*
 * 2021.11.01
 *   Adapt LLVM 12 interface support
 * 2022.3.1
 *   Add a extern data argument for kernel
 * 2023.08.12
 *   Adapt LLVM 15 interface support
 */

#ifndef TVM_CODEGEN_LLVM_CODEGEN_CPU_H_
#define TVM_CODEGEN_LLVM_CODEGEN_CPU_H_

#include <utility>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include "codegen_llvm.h"

namespace air {
namespace codegen {

// CPU host code generation
class CodeGenCPU : public CodeGenLLVM {
 public:
  void Init(const std::string& module_name,
            llvm::TargetMachine* tm,
            llvm::LLVMContext* ctx,
            bool system_lib,
            bool dynamic_lookup) override;
  void AddFunction(const LoweredFunc& f) override;
  void AddMainFunction(const std::string& entry_func_name) override;
  std::unique_ptr<llvm::Module> Finish() override;
  void VisitStmt_(const AssertStmt* op) override;
  void VisitStmt_(const AttrStmt* op) override;
  void VisitStmt_(const For* op) override;
  llvm::Value* CreateIntrinsic(const Call* op) override;
  llvm::Value* CreateCallExtern(const Call* op) override;

 protected:
  void AddStartupFunction() final;
  // meta data
  llvm::MDNode* md_tbaa_ctx_ptr_{nullptr};
  // TVM related data types
  llvm::Type* t_tvm_shape_index_{nullptr};
  llvm::Type* t_tvm_func_handle_{nullptr};
  llvm::StructType* t_tvm_context_{nullptr};
  llvm::StructType* t_tvm_type_{nullptr};
  llvm::StructType* t_tvm_array_{nullptr};
  llvm::StructType* t_tvm_value_{nullptr};
  llvm::StructType* t_tvm_parallel_group_env_{nullptr};
  llvm::FunctionType* ftype_tvm_parallel_lambda_{nullptr};
  llvm::FunctionType* ftype_tvm_func_call_{nullptr};
  llvm::FunctionType* ftype_tvm_get_func_from_env_{nullptr};
  llvm::FunctionType* ftype_tvm_api_set_last_error_{nullptr};
  llvm::FunctionType* ftype_tvm_parallel_launch_{nullptr};
  llvm::FunctionType* ftype_tvm_parallel_barrier_{nullptr};
  llvm::FunctionType* ftype_tvm_register_system_symbol_{nullptr};
  // Lazy entry for function call.
  llvm::FunctionType* ftype_tvm_static_init_callback_{nullptr};
  llvm::FunctionType* ftype_tvm_static_init_{nullptr};
  llvm::FunctionType* ftype_alloc_launch_{nullptr};
  llvm::FunctionType* ftype_free_launch_{nullptr};

 private:
  // the parallel group information
  struct ParallelEnv {
    VarExpr task_id;
    VarExpr num_task;
    bool stride_pattern{false};
    bool in_parallel_loop{false};
    int parallel_loop_count{0};
    llvm::Value* penv{nullptr};
  };
  // Get runtime functions
  void InitGlobalContext(bool dynamic_lookup);
  llvm::GlobalVariable* InitContextPtr(llvm::Type* type, std::string name);
  llvm::Value* GetContextPtr(llvm::GlobalVariable* gv);
  llvm::Value* RuntimeTVMFuncCall();
  llvm::Value* RuntimeTVMGetFuncFromEnv();
  llvm::Value* RuntimeTVMAPISetLastError();
  llvm::Value* RuntimeTVMParallelLaunch();
  llvm::Value* RuntimeTVMParallelBarrier();
  llvm::Value* CreateStaticHandle();
  llvm::Value* GetPackedFuncHandle(const std::string& str);
  llvm::Value* PackClosureData(const Array<Var>& fields, uint64_t *num_bytes);
  CodeGenLLVM::TypedPointer CreateStructRefPtr(Type t, llvm::Value* buffer, llvm::Value* index, int kind);
  void UnpackClosureData(llvm::Value*cdata,
                         const Array<Var>& fields,
                         std::unordered_map<const Variable*, llvm::Value*>* vmap);
  // Make packed call.
  llvm::BasicBlock *MakeCallPacked(const Array<Expr> &args,
                                   llvm::Value **rvalue,
                                   llvm::Value **ret_tcode, const Type &r_type,
                                   const int64_t begin, const int64_t end);
  // create call into tvm packed function.
  llvm::Value* CreateCallPacked(const Call* op);
  // Create trace call into tvm packed function.
  llvm::Value* CreateCallTracePacked(const Call *op);
  // Create static initialization
  void CreateStaticInit(const std::string& init_fname, const Stmt& body);
  // Create parallel launch
  void CreateParallelLaunch(const Stmt& body, int num_task);
  // Create a new compute scope.
  void CreateComputeScope(const AttrStmt* op);
  // Check if the call to packed function is successful
  // if not directly finalize function and pass on return code.
  // return the end block after the check
  llvm::BasicBlock* CheckCallSuccess(llvm::Value* retcode);
  // Context for injection lookup
  llvm::GlobalVariable* gv_mod_ctx_{nullptr};
  llvm::GlobalVariable* gv_tvm_func_call_{nullptr};
  llvm::GlobalVariable* gv_tvm_get_func_from_env_{nullptr};
  llvm::GlobalVariable* gv_tvm_api_set_last_error_{nullptr};
  llvm::GlobalVariable* gv_tvm_parallel_launch_{nullptr};
  llvm::GlobalVariable* gv_tvm_parallel_barrier_{nullptr};
  std::unordered_map<std::string, llvm::GlobalVariable*> gv_func_map_;
  // context for direct dynamic lookup
  llvm::Function* f_tvm_func_call_{nullptr};
  llvm::Function* f_tvm_get_func_from_env_{nullptr};
  llvm::Function* f_tvm_api_set_last_error_{nullptr};
  llvm::Function* f_tvm_parallel_launch_{nullptr};
  llvm::Function* f_tvm_parallel_barrier_{nullptr};
  llvm::Function* f_tvm_register_system_symbol_{nullptr};
  llvm::Function* f_tvm_alloc_launch_{nullptr};
  llvm::Function* f_tvm_free_launch_{nullptr};
  // Current parallel environment scope.
  ParallelEnv parallel_env_;
  // global to packed function handle
  std::unordered_map<std::string, llvm::GlobalVariable*> func_handle_map_;
  // List of symbols to be exported to TVM system lib.
  std::vector<std::pair<std::string, llvm::Value*> > export_system_symbols_;
  // internal debug information, to be populated by
  std::unique_ptr<DebugInfo> dbg_info_;

  // Get the DWARF type corresponding to the LLVM type |ty|. The current API in practice only
  // generates |int32|, and |int8*|.
  static llvm::DIType* getDebugType(IRBuilder* builder, llvm::DIBuilder* di_builder,
                                    llvm::Type* ty);
  // Adds the DWARF debug information for |function| to |dbg_info_|.
  void AddDebugInformation(llvm::Function* function);

  Array<Var> args_real_;
  Var extern_arg_{"extern_arg"};
  Var extern_links_{"extern_links"};
  Var parallel_launch_{"AKGBackendParallelLaunch"};
  Var alloc_launch_{"TVMBackendAllocWorkspace"};
  Var free_launch_{"TVMBackendFreeWorkspace"};
  std::unordered_map<std::string, std::tuple<llvm::Value*, llvm::Type*,
    llvm::FunctionType*>> extern_func_map_;
};

}  // namespace codegen
}  // namespace air
#endif  // TVM_CODEGEN_LLVM_CODEGEN_CPU_H_
