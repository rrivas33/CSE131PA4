/* File: ast_decl.cc
 * -----------------
 * Implementation of Decl node classes.
 */
#include "ast_decl.h"
#include "ast_type.h"
#include "ast_stmt.h"
#include "symtable.h"        
         
Decl::Decl(Identifier *n) : Node(*n->GetLocation()) {
    Assert(n != NULL);
    (id=n)->SetParent(this); 
}

llvm::Value* Decl::Emit() 
{ 
	return llvm::UndefValue::get(irgen->GetVoidType());
}

VarDecl::VarDecl(Identifier *n, Type *t, Expr *e) : Decl(n) {
    Assert(n != NULL && t != NULL);
    (type=t)->SetParent(this);
    if (e) (assignTo=e)->SetParent(this);
    typeq = NULL;
}

VarDecl::VarDecl(Identifier *n, TypeQualifier *tq, Expr *e) : Decl(n) {
    Assert(n != NULL && tq != NULL);
    (typeq=tq)->SetParent(this);
    if (e) (assignTo=e)->SetParent(this);
    type = NULL;
}

VarDecl::VarDecl(Identifier *n, Type *t, TypeQualifier *tq, Expr *e) : Decl(n) {
    Assert(n != NULL && t != NULL && tq != NULL);
    (type=t)->SetParent(this);
    (typeq=tq)->SetParent(this);
    if (e) (assignTo=e)->SetParent(this);
}
  
void VarDecl::PrintChildren(int indentLevel) { 
   if (typeq) typeq->Print(indentLevel+1);
   if (type) type->Print(indentLevel+1);
   if (id) id->Print(indentLevel+1);
   if (assignTo) assignTo->Print(indentLevel+1, "(initializer) ");
}

//EMIT
llvm::Value* VarDecl::Emit()
{
	std::cerr << "VarDecl\n";

	//current function we are in, if any
	llvm::Function *func = irgen->GetFunction();
	llvm::Module *mod = irgen->GetOrCreateModule("Module");
	llvm::Type *llvmType;

	ArrayType *arrayType = dynamic_cast<ArrayType *>(type);
	if(arrayType != NULL)
	{
		//llvm array type
		llvm::Type *elmtType = arrayType->GetElemType()->typeToLlvmType();
		llvmType =  llvm::ArrayType::get(elmtType, arrayType->GetElemCount());
	}
	else if(type->IsEquivalentTo(Type::intType))
	{
		llvmType = irgen->GetIntType();
	}		
	else if(type->IsEquivalentTo(Type::floatType))
	{	
		llvmType = irgen->GetFloatType();
	}		
	else if(type->IsEquivalentTo(Type::boolType))
	{
		llvmType = irgen->GetBoolType();
	}	
	else if(type->IsEquivalentTo(Type::vec2Type))
	{
		llvmType = irgen->GetVec2Type();
	}		
	else if(type->IsEquivalentTo(Type::vec3Type))
	{
		llvmType = irgen->GetVec3Type();
	}
	else if(type->IsEquivalentTo(Type::vec4Type))
	{		
		llvmType = irgen->GetVec4Type();
		std::cerr << llvm::cast<llvm::VectorType>(llvmType)->getNumElements() << std::endl;
	}



	//Global variable if outside of function
	if(func == NULL)
	{	
		//TODO check if constant or not
		//create global var and add it to current global scope table

		llvm::GlobalVariable *globalVar = llvm::cast<llvm::GlobalVariable>(mod->getOrInsertGlobal(id->GetName(), llvmType));
		globalVar->setConstant(false);

		Symbol sym(id->GetName(), this, E_VarDecl, globalVar);
		symbolTable->insert(sym);

		return globalVar;
	}
	else
	{
		//crete local var and add it to first Basic Block in func
		llvm::Function::iterator b = func->begin();
		llvm::BasicBlock *firstBB = b;

		llvm::AllocaInst *var = new llvm::AllocaInst(llvmType, id->GetName(),  firstBB);

		Symbol sym(id->GetName(), this, E_VarDecl, var);
		symbolTable->insert(sym);

		return var;
	}

}

FnDecl::FnDecl(Identifier *n, Type *r, List<VarDecl*> *d) : Decl(n) {
    Assert(n != NULL && r!= NULL && d != NULL);
    (returnType=r)->SetParent(this);
    (formals=d)->SetParentAll(this);
    body = NULL;
    returnTypeq = NULL;
}

FnDecl::FnDecl(Identifier *n, Type *r, TypeQualifier *rq, List<VarDecl*> *d) : Decl(n) {
    Assert(n != NULL && r != NULL && rq != NULL&& d != NULL);
    (returnType=r)->SetParent(this);
    (returnTypeq=rq)->SetParent(this);
    (formals=d)->SetParentAll(this);
    body = NULL;
}

void FnDecl::SetFunctionBody(Stmt *b) { 
    (body=b)->SetParent(this);
}

void FnDecl::PrintChildren(int indentLevel) {
    if (returnType) returnType->Print(indentLevel+1, "(return type) ");
    if (id) id->Print(indentLevel+1);
    if (formals) formals->PrintAll(indentLevel+1, "(formals) ");
    if (body) body->Print(indentLevel+1, "(body) ");
}

llvm::Value* FnDecl::Emit()
{

	//create llvm function signature
	llvm::Type *llvmRetType = returnType->typeToLlvmType();

	std::vector<llvm::Type *> argTypes;	
	for(int i = 0; i < formals->NumElements(); i++)
	{
		VarDecl *decl = formals->Nth(i);
		argTypes.push_back(decl->GetLlvmType());
	}

	llvm::ArrayRef<llvm::Type *> argArray(argTypes);
	llvm::FunctionType *funcType = llvm::FunctionType::get(llvmRetType, argArray, false);

	//create function
	llvm::Module *mod = irgen->GetOrCreateModule("Module");
	llvm::Function *func = llvm::cast<llvm::Function>(mod->getOrInsertFunction(id->GetName(), funcType));
	irgen->SetFunction(func);


	//set func params name
	llvm::Function::arg_iterator arg = func->arg_begin();
	for(int i = 0; i < formals->NumElements(); i++, arg++)
	{
		Decl *decl = formals->Nth(i);
		arg->setName(decl->GetIdentifier()->GetName());
	}

	
	//insert entry block
	llvm::BasicBlock *entryBB = llvm::BasicBlock::Create(*(irgen->GetContext()), "entry", func);
	irgen->SetBasicBlock(entryBB);

	//allocate params
	for(int i = 0; i < formals->NumElements(); i++)
	{
		VarDecl *decl = formals->Nth(i);
		decl->Emit();		
	}

	//create new basic block
	llvm::BasicBlock *nextBB = llvm::BasicBlock::Create(*(irgen->GetContext()), "next", func);
	irgen->SetBasicBlock(nextBB);

	
	//store params
	llvm::BasicBlock::iterator inst = entryBB->begin();
	arg = func->arg_begin();
	for(int i = 0; i < formals->NumElements(); i++, inst++, arg++)
	{
		//TODO Not sure about storing the argument into the allocation
		llvm::StoreInst *store = new llvm::StoreInst(arg, inst, nextBB);
	}
	
	

	body->Emit();
	
	//branch entry bb to next bb
	llvm::BranchInst *branch = llvm::BranchInst::Create(nextBB, entryBB);


	
/*
	// create a function signature
    std::vector<llvm::Type *> argTypes;
    llvm::Type *intTy = irgen.GetIntType();
    argTypes.push_back(intTy);
    llvm::ArrayRef<llvm::Type *> argArray(argTypes);
    llvm::FunctionType *funcTy = llvm::FunctionType::get(intTy, argArray, false);

    // llvm::Function *f = llvm::cast<llvm::Function>(mod->getOrInsertFunction("foo", intTy, intTy, (Type *)0));
    llvm::Function *f = llvm::cast<llvm::Function>(mod->getOrInsertFunction("foo", funcTy));
    llvm::Argument *arg = f->arg_begin();
    arg->setName("x");

    // insert a block into the function
    llvm::LLVMContext *context = irgen.GetContext();
    llvm::BasicBlock *bb = llvm::BasicBlock::Create(*context, "entry", f);

    // create a return instruction
    llvm::Value *val = llvm::ConstantInt::get(intTy, 1);
    llvm::Value *sum = llvm::BinaryOperator::CreateAdd(arg, val, "", bb);
    llvm::ReturnInst::Create(*context, sum, bb);
*/

	return llvm::UndefValue::get(irgen->GetVoidType());
}

