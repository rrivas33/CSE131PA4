/* irgen.cc -  LLVM IR generator
 *
 * You can implement any LLVM related functions here.
 */

#include "irgen.h"


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

llvm::Type *IRGenerator::GetIntType() const {
   llvm::Type *ty = llvm::Type::getInt32Ty(*context);
   return ty;
}

llvm::Type *IRGenerator::GetBoolType() const {
   llvm::Type *ty = llvm::Type::getInt1Ty(*context);
   return ty;
}

llvm::Type *IRGenerator::GetFloatType() const {
   llvm::Type *ty = llvm::Type::getFloatTy(*context);
   return ty;
}

llvm::Type *IRGenerator::GetVoidType() const {
	llvm::Type *ty = llvm::Type::getVoidTy(*context);
	return ty;
}

llvm::Type *IRGenerator::GetVec2Type() const {
	llvm::Type *ty = llvm::VectorType::get(llvm::Type::getFloatTy(*context), 2);
	return ty;
}

llvm::Type *IRGenerator::GetVec3Type() const {
	llvm::Type *ty = llvm::VectorType::get(llvm::Type::getFloatTy(*context), 3);
	return ty;
}

llvm::Type *IRGenerator::GetVec4Type() const {
	llvm::Type *ty = llvm::VectorType::get(llvm::Type::getFloatTy(*context), 4);
	return ty;
}
/*
llvm::Type *IRGenerator::GetType(Type *type)
{
	llvm::Type *llvmType;
	if(type->IsEquivalentTo(Type::intType))
	{
		llvmType = GetIntType();
	}		
	else if(type->IsEquivalentTo(Type::floatType))
	{	
		llvmType = GetFloatType();
	}		
	else if(type->IsEquivalentTo(Type::boolType))
	{
		llvmType = GetBoolType();
	}		
	else if(type->IsEquivalentTo(Type::vec2Type))
	{
		llvmType = GetVec2Type();
	
	}		
	else if(type->IsEquivalentTo(Type::vec3Type))
	{
		llvmType = GetVec3Type();
	}
	else if(type->IsEquivalentTo(Type::vec4Type))
	{		
		llvmType = GetVec4Type();
	}

	return llvmType;
}
*/

/*
llvm::Type* IRGenerator::GetLlvmType(llvm::Value *value)
{
	//variable
	if(llvm::AllocaInst::classof(value) || llvm::GlobalVariable::classof(value))
	{
		Symbol *sym = symbolTable->find(value->getName());	
		return sym->llvmType;
	}

	else if(llvm::ConstantFP::classof(value))
		return irgen->GetFloatType(->GetIntType(););
	else if(llvm::ConstantInt::classof(value))
		return irgen->GetIntType();
	else
		return NULL:
	
}

*/

const char *IRGenerator::TargetLayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128";

const char *IRGenerator::TargetTriple = "x86_64-redhat-linux-gnu";

