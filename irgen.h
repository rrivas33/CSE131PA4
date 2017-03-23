/**
 * File: irgen.h
 * -----------
 *  This file defines a class for LLVM IR Generation.
 *
 *  All LLVM instruction related functions or utilities can be implemented
 *  here. You'll need to customize this class heavily to provide any helpers
 *  or untility as you need.
 */

#ifndef _H_IRGen
#define _H_IRGen

// LLVM headers
#include "llvm/IR/Module.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"


class IRGenerator {
  public:
    IRGenerator();
    ~IRGenerator();

    llvm::Module   *GetOrCreateModule(const char *moduleID);
    llvm::LLVMContext *GetContext() const { return context; }

    // Add your helper functions here
    llvm::Function *GetFunction() const;
    void      SetFunction(llvm::Function *func);

    llvm::BasicBlock *GetBasicBlock() const;
    void        SetBasicBlock(llvm::BasicBlock *bb);

    llvm::Type *GetIntType();
    llvm::Type *GetBoolType();
    llvm::Type *GetFloatType();
	llvm::Type *GetVoidType();
	llvm::Type *GetVec2Type();
	llvm::Type *GetVec3Type();
	llvm::Type *GetVec4Type();
	llvm::Type *GetType(llvm::Value *value);
	bool IsFloatType(llvm::Value *value);

	//static llvm::Type* GetLlvmType(llvm::Value *value);

  private:
    llvm::LLVMContext *context;
    llvm::Module      *module;

    // track which function or basic block is active
    llvm::Function    *currentFunc;
    llvm::BasicBlock  *currentBB;

    static const char *TargetTriple;
    static const char *TargetLayout;
};

#endif

