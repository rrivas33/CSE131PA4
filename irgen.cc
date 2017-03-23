/* irgen.cc -  LLVM IR generator
 *
 * You can implement any LLVM related functions here.
 */

#include "irgen.h"
#include <iostream>


IRGenerator::IRGenerator() :
    context(NULL),
    module(NULL),
    currentFunc(NULL),
    currentBB(NULL)
{
}

IRGenerator::~IRGenerator() {
}

llvm::Module *IRGenerator::GetOrCreateModule(const char *moduleID)
{
   if ( module == NULL ) {
     context = new llvm::LLVMContext();
     module  = new llvm::Module(moduleID, *context);
     module->setTargetTriple(TargetTriple);
     module->setDataLayout(TargetLayout);
   }
   return module;
}

void IRGenerator::SetFunction(llvm::Function *func) {
   currentFunc = func;
}

llvm::Function *IRGenerator::GetFunction() const {
   return currentFunc;
}

void IRGenerator::SetBasicBlock(llvm::BasicBlock *bb) {
   currentBB = bb;
}

llvm::BasicBlock *IRGenerator::GetBasicBlock() const {
   return currentBB;
}

llvm::Type *IRGenerator::GetIntType()  {
   llvm::Type *ty = llvm::Type::getInt32Ty(*context);
   return ty;
}

llvm::Type *IRGenerator::GetBoolType() {
   llvm::Type *ty = llvm::Type::getInt1Ty(*context);
   return ty;
}

llvm::Type *IRGenerator::GetFloatType() {
   llvm::Type *ty = llvm::Type::getFloatTy(*context);
   return ty;
}

llvm::Type *IRGenerator::GetVoidType() {
	llvm::Type *ty = llvm::Type::getVoidTy(*context);
	return ty;
}

llvm::Type *IRGenerator::GetVec2Type() {
	llvm::Type *ty = llvm::VectorType::get(llvm::Type::getFloatTy(*context), 2);
	return ty;
}

llvm::Type *IRGenerator::GetVec3Type() {
	llvm::Type *ty = llvm::VectorType::get(llvm::Type::getFloatTy(*context), 3);
	return ty;
}

llvm::Type *IRGenerator::GetVec4Type() {
	llvm::Type *ty = llvm::VectorType::get(llvm::Type::getFloatTy(*context), 4);
	return ty;
}

llvm::Type *IRGenerator::GetType(llvm::Value *value)
{

	if(llvm::VectorType::classof(llvm::cast<llvm::Type>(value)))
		return llvm::cast<llvm::Type>(value);
	else if(llvm::dyn_cast<llvm::ConstantInt>(value))
		return GetIntType();
	else if(llvm::dyn_cast<llvm::ConstantFP>(value))
		return GetFloatType();
	else if(llvm::dyn_cast<llvm::AllocaInst>(value))
	{
		llvm::AllocaInst *alloc = llvm::cast<llvm::AllocaInst>(value);
		llvm::Type *type = alloc->getAllocatedType();


		if(llvm::ArrayType::classof(type))
		{
			if(llvm::ArrayType::isValidElementType(GetIntType()))
				return GetIntType();
			else
				return GetFloatType();
		}

		return type;
	}
	else
	{
		return NULL;
	}
}

bool IRGenerator::IsFloatType(llvm::Value *value)
{

	 if(llvm::ConstantFP::classof(value))
		return true;
	else if(llvm::GetElementPtrInst::classof(value))
		return true;
	else if(llvm::ExtractElementInst::classof(value))
		return true;

	return false;
}


const char *IRGenerator::TargetLayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128";

const char *IRGenerator::TargetTriple = "x86_64-redhat-linux-gnu";

