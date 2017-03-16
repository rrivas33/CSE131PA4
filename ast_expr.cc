/* File: ast_expr.cc
 * -----------------
 * Implementation of expression node classes.
 */

#include <string.h>
#include "ast_expr.h"
#include "ast_type.h"
#include "ast_decl.h"
#include "symtable.h"

IntConstant::IntConstant(yyltype loc, int val) : Expr(loc) {
    value = val;
}
void IntConstant::PrintChildren(int indentLevel) { 
    printf("%d", value);
}

llvm::Value* IntConstant::Emit()
{
	return llvm::ConstantInt::get(irgen->GetIntType(), value);
}

FloatConstant::FloatConstant(yyltype loc, double val) : Expr(loc) {
    value = val;
}
void FloatConstant::PrintChildren(int indentLevel) { 
    printf("%g", value);
}

llvm::Value* FloatConstant::Emit()
{
	return llvm::ConstantFP::get(irgen->GetFloatType(), value);
}

BoolConstant::BoolConstant(yyltype loc, bool val) : Expr(loc) {
    value = val;
}
void BoolConstant::PrintChildren(int indentLevel) { 
    printf("%s", value ? "true" : "false");
}

llvm::Value* BoolConstant::Emit()
{
	return llvm::ConstantInt::get(irgen->GetBoolType(), value ? 1 : 0);
}

VarExpr::VarExpr(yyltype loc, Identifier *ident) : Expr(loc) {
    Assert(ident != NULL);
    this->id = ident;
}

void VarExpr::PrintChildren(int indentLevel) {
    id->Print(indentLevel+1);
}

llvm::Value* VarExpr::Emit()
{

	//TODO figure out how to get type allocated in variable
	//allocated type for AllocInst?
	Symbol *sym = symbolTable->find(id->GetName());
	
	llvm::Type *type = sym->value->getType();
	if(type->isFloatTy())
		std::cerr << "is float type" << std::endl;
	else if(type->isIntegerTy())
		std::cerr << "is integer type" << std::endl;
	else
		std::cerr << "type not recognized" << std::endl;

	return sym->value;
	
}

Operator::Operator(yyltype loc, const char *tok) : Node(loc) {
    Assert(tok != NULL);
    strncpy(tokenString, tok, sizeof(tokenString));
}

void Operator::PrintChildren(int indentLevel) {
    printf("%s",tokenString);
}

bool Operator::IsOp(const char *op) const {
    return strcmp(tokenString, op) == 0;
}

CompoundExpr::CompoundExpr(Expr *l, Operator *o, Expr *r) 
  : Expr(Join(l->GetLocation(), r->GetLocation())) {
    Assert(l != NULL && o != NULL && r != NULL);
    (op=o)->SetParent(this);
    (left=l)->SetParent(this); 
    (right=r)->SetParent(this);
}

CompoundExpr::CompoundExpr(Operator *o, Expr *r) 
  : Expr(Join(o->GetLocation(), r->GetLocation())) {
    Assert(o != NULL && r != NULL);
    left = NULL; 
    (op=o)->SetParent(this);
    (right=r)->SetParent(this);
}

CompoundExpr::CompoundExpr(Expr *l, Operator *o) 
  : Expr(Join(l->GetLocation(), o->GetLocation())) {
    Assert(l != NULL && o != NULL);
    (left=l)->SetParent(this);
    (op=o)->SetParent(this);
}

void CompoundExpr::PrintChildren(int indentLevel) {
   if (left) left->Print(indentLevel+1);
   op->Print(indentLevel+1);
   if (right) right->Print(indentLevel+1);
}

llvm::Value* ArithmeticExpr::Emit()
{
	llvm::BasicBlock *bb = irgen->GetBasicBlock();

	//TODO cheking the type of val does not work properly

	//involves 2 expressions
	if(left != NULL)
	{
		//get llvm values
		llvm::Value *valLeft = left->Emit();
		llvm::Value *valRight = right->Emit();

		//load value if necessary
		if(llvm::UnaryInstruction::classof(valLeft) || llvm::GlobalVariable::classof(valLeft) || llvm::GetElementPtrInst::classof(valLeft))
		{
			valLeft = new llvm::LoadInst(valLeft, "ld1", irgen->GetBasicBlock());
		}

		if(llvm::UnaryInstruction::classof(valRight) || llvm::GlobalVariable::classof(valRight) || llvm::GetElementPtrInst::classof(valRight))
		{
			valRight = new llvm::LoadInst(valRight, "ld2", irgen->GetBasicBlock());
		}

		//type of values
		llvm::Type *valType = valLeft->getType();
		llvm::BinaryOperator *binInst;

		if(op->IsOp("*"))
		{
			//integers
			if(valType->isIntegerTy())
			{	
				binInst = llvm::BinaryOperator::CreateMul(valLeft, valRight, "imul");
			}
			//float and vector
			else
			{
				binInst = llvm::BinaryOperator::CreateFMul(valLeft, valRight, "fmul");
			}
		}
		else if(op->IsOp("/"))
		{

			//integers
			if(valType->isIntegerTy())
			{	
				binInst = llvm::BinaryOperator::CreateSDiv(valLeft, valRight, "idiv");
			}
			//float and vector
			else
			{
				binInst = llvm::BinaryOperator::CreateFDiv(valLeft, valRight, "fdiv");
			}

		}
		else if(op->IsOp("+"))
		{

			//integers
			if(valType->isIntegerTy())
			{	
				binInst = llvm::BinaryOperator::CreateAdd(valLeft, valRight, "iadd");
			}
			//float and vector
			else
			{
				binInst = llvm::BinaryOperator::CreateFAdd(valLeft, valRight, "fadd");
			}
		}
		else if(op->IsOp("-"))
		{

			//integers
			if(valType->isIntegerTy())
			{	
				binInst = llvm::BinaryOperator::CreateSub(valLeft, valRight, "isub");

			}
			//float and vector
			else
			{
				binInst = llvm::BinaryOperator::CreateFSub(valLeft, valRight, "fsub");
			}
		}

		//add inst to current basic block
		bb->getInstList().push_back(binInst);
		return binInst;

	}
	//unary expresssion
	else
	{
		
		//get llvm value of expr
		llvm::Value *varStore = right->Emit();
		llvm::Value *valueRight = varStore;

		//load value if necessary
		if(llvm::UnaryInstruction::classof(valueRight) || llvm::GlobalVariable::classof(valueRight) || llvm::GetElementPtrInst::classof(valueRight))
		{
			valueRight = new llvm::LoadInst(valueRight, "ld1", irgen->GetBasicBlock());
		}

		llvm::Type *valType = valueRight->getType();
		llvm::Constant *valLeft;
		llvm::BinaryOperator *binInst;
		
		if(op->IsOp("++"))
		{
			//integer
			if(valType->isIntegerTy())
			{	
				valLeft = llvm::ConstantInt::get(irgen->GetIntType(), 1, true);
				binInst = llvm::BinaryOperator::CreateAdd(valLeft, valueRight, "iadd");

			}
			//float and vector
			else
			{
				valLeft = llvm::ConstantFP::get(irgen->GetFloatType(), 1.0);
				binInst = llvm::BinaryOperator::CreateFAdd(valLeft, valueRight, "fadd");
			}

		}
		else if(op->IsOp("--"))
		{

			//integers
			if(valType->isIntegerTy())
			{	
				valLeft = llvm::ConstantInt::get(valType, 1, true);
				binInst = llvm::BinaryOperator::CreateSub(valueRight, valLeft, "isub");
		
			}
			//float and vector
			else
			{
				valLeft = llvm::ConstantInt::get(valType, 1, true);
				binInst = llvm::BinaryOperator::CreateFSub(valueRight, valLeft, "fsub");
			}
		}
		else if(op->IsOp("-"))
		{
			//integers
			if(valType->isIntegerTy())
			{	
				valLeft = llvm::ConstantInt::get(valType, 0, true);
				binInst = llvm::BinaryOperator::CreateSub(valLeft, valueRight, "isub");
				
			}
			//float and vector
			else
			{
				valLeft = llvm::ConstantInt::get(valType, 0, true);
				binInst = llvm::BinaryOperator::CreateFSub(valLeft, valueRight, "fsub");
			}
		}
		// "+"
		else
		{
			//simply return the llvm value, since "+(variable)" == "variable"
			return right->Emit();
		}
		
		//store result and add instructions to current basic block
		bb->getInstList().push_back(binInst);
		new llvm::StoreInst(binInst, varStore, true, irgen->GetBasicBlock());

		return binInst;

	}//end of unary else
}

llvm::Value* RelationalExpr::Emit()
{
	//TODO checking of type not working!!!!

	llvm::Value *val1 = left->Emit();
	llvm::Value *val2 = right->Emit();
	llvm::Type *type = val1->getType();
	llvm::CmpInst *cmp;
	llvm::BasicBlock *bb = irgen->GetBasicBlock();

	//load variable if necessary
	if(llvm::UnaryInstruction::classof(val1) || llvm::GlobalVariable::classof(val1) || llvm::GetElementPtrInst::classof(val1))
	{
		val1 = new llvm::LoadInst(val1, "ld1", irgen->GetBasicBlock());
	}

	if(llvm::UnaryInstruction::classof(val2) || llvm::GlobalVariable::classof(val2) || llvm::GetElementPtrInst::classof(val2))
	{
		val2 = new llvm::LoadInst(val2, "ld2", irgen->GetBasicBlock());
	}

	//generate compare instruction
	if(op->IsOp("<"))
	{
		if(type->isFloatTy())
			cmp = new llvm::FCmpInst(*(irgen->GetBasicBlock()), llvm::CmpInst::FCMP_OLT, val1, val2, "fcmp");
		else
			cmp = new llvm::ICmpInst(*(irgen->GetBasicBlock()), llvm::CmpInst::ICMP_ULT, val1, val2, "icmp");
	}
	else if(op->IsOp(">"))
	{
		
		if(type->isFloatTy())
			cmp = new llvm::FCmpInst(*(irgen->GetBasicBlock()), llvm::CmpInst::FCMP_OGT, val1, val2, "fcmp");
		else
			cmp = new llvm::ICmpInst(*(irgen->GetBasicBlock()), llvm::CmpInst::ICMP_UGT, val1, val2, "icmp");
		
	}
	else if(op->IsOp("<="))
	{
		if(type->isFloatTy())
			cmp = new llvm::FCmpInst(*(irgen->GetBasicBlock()), llvm::CmpInst::FCMP_OLE, val1, val2, "fcmp");
		else
			cmp = new llvm::ICmpInst(*(irgen->GetBasicBlock()), llvm::CmpInst::ICMP_ULE, val1, val2, "icmp");	
	}
	else if(op->IsOp(">="))
	{
		if(type->isFloatTy())
			cmp = new llvm::FCmpInst(*(irgen->GetBasicBlock()), llvm::CmpInst::FCMP_OGE, val1, val2, "fcmp");
		else
			cmp = new llvm::ICmpInst(*(irgen->GetBasicBlock()), llvm::CmpInst::ICMP_UGE, val1, val2, "icmp");
	}

	//add inst to current basic block
	//bb->getInstList().push_back(cmp);
	return cmp;
}

llvm::Value* EqualityExpr::Emit()
{
	//TODO checking of type not working

	llvm::Value *val1 = left->Emit();
	llvm::Value *val2 = right->Emit();
	llvm::Type *type = val1->getType();

	//load variable if necessary
	if(llvm::UnaryInstruction::classof(val1) || llvm::GlobalVariable::classof(val1) || llvm::GetElementPtrInst::classof(val1))
	{
		val1 = new llvm::LoadInst(val1, "ld1", irgen->GetBasicBlock());
	}

	if(llvm::UnaryInstruction::classof(val2) || llvm::GlobalVariable::classof(val2) || llvm::GetElementPtrInst::classof(val2))
	{
		val2 = new llvm::LoadInst(val2, "ld2", irgen->GetBasicBlock());
	}

	//generate compare inst
	llvm::CmpInst *cmp;
	if(op->IsOp("=="))
	{
		if(type->isFloatTy())
			cmp = new llvm::FCmpInst(*(irgen->GetBasicBlock()), llvm::CmpInst::FCMP_OEQ, val1, val2, "fcmp");
		else
			cmp = new llvm::ICmpInst(*(irgen->GetBasicBlock()), llvm::CmpInst::ICMP_EQ, val1, val2, "icmp");
	}
	else if(op->IsOp("!="))
	{
		if(type->isFloatTy())
			cmp = new llvm::FCmpInst(*(irgen->GetBasicBlock()), llvm::CmpInst::FCMP_ONE, val1, val2, "fcmp");
		else
			cmp = new llvm::ICmpInst(*(irgen->GetBasicBlock()), llvm::CmpInst::ICMP_NE, val1, val2, "icmp");
	}

	return cmp;
}

llvm::Value* LogicalExpr::Emit()
{
	llvm::Value *val1 = left->Emit();
	llvm::Value *val2 = right->Emit();
	llvm::Type *type = val1->getType();

	//load variable if necessary
	if(llvm::UnaryInstruction::classof(val1) || llvm::GlobalVariable::classof(val1) || llvm::GetElementPtrInst::classof(val1))
	{
		val1 = new llvm::LoadInst(val1, "ld1", irgen->GetBasicBlock());
	}

	if(llvm::UnaryInstruction::classof(val2) || llvm::GlobalVariable::classof(val2) || llvm::GetElementPtrInst::classof(val2))
	{
		val2 = new llvm::LoadInst(val2, "ld2", irgen->GetBasicBlock());
	}

	//generator inst
	if(op->IsOp("&&"))
	{
		llvm::BinaryOperator *binAnd = llvm::BinaryOperator::CreateAnd(val1, val2, "and");
		return binAnd;
	}
	else if(op->IsOp("||"))
	{
		llvm::BinaryOperator *binOr = llvm::BinaryOperator::CreateOr(val1, val2, "or");
		return binOr;
	}
}

llvm::Value* AssignExpr::Emit()
{
	//current basic block
	llvm::BasicBlock *bb = irgen->GetBasicBlock();

	//get llvm value of expressions
	llvm::Value *valueRight = right->Emit();
	llvm::Value *varLeft = left->Emit();
	llvm::Value *valueLeft = varLeft;
	llvm::StoreInst *storeInst;


	//load right value if necessary
	if(llvm::UnaryInstruction::classof(valueRight) || llvm::GlobalVariable::classof(valueRight) || llvm::GetElementPtrInst::classof(valueRight))
	{
		valueRight = new llvm::LoadInst(valueRight, "ld1", irgen->GetBasicBlock());
	}

	//simple assigment
	if(op->IsOp("="))
	{
		storeInst = new llvm::StoreInst(valueRight, varLeft, true, irgen->GetBasicBlock());
		return storeInst;
	}


	//load left value
	if(llvm::UnaryInstruction::classof(valueLeft) || llvm::GlobalVariable::classof(valueLeft) || llvm::GetElementPtrInst::classof(valueLeft))
	{
		valueLeft = new llvm::LoadInst(valueLeft, "ld1", irgen->GetBasicBlock());
	}


	//generate binary operation and store inst
	llvm::Type *valType = valueLeft->getType();
	llvm::BinaryOperator *binInst;

	if(op->IsOp("*="))
	{
		//integers
		if(valType->isIntegerTy())
		{	
			binInst = llvm::BinaryOperator::CreateMul(valueLeft, valueRight, "imul");
			bb->getInstList().push_back(binInst);			

			storeInst = new llvm::StoreInst(binInst, varLeft, true, irgen->GetBasicBlock());
		}
		//float and vector
		else
		{
			binInst = llvm::BinaryOperator::CreateFMul(valueLeft, valueRight, "fmul");
			bb->getInstList().push_back(binInst);

			storeInst = new llvm::StoreInst(binInst, varLeft, true, irgen->GetBasicBlock());
		}
	
	}
	else if(op->IsOp("/="))
	{
		//integers
		if(valType->isIntegerTy())
		{	
			binInst = llvm::BinaryOperator::CreateSDiv(valueLeft, valueRight, "idiv");
			bb->getInstList().push_back(binInst);			

			storeInst = new llvm::StoreInst(binInst, varLeft, true, irgen->GetBasicBlock());
		}
		//float and vector
		else
		{
			binInst = llvm::BinaryOperator::CreateFDiv(valueLeft, valueRight, "fdiv");
			bb->getInstList().push_back(binInst);

			storeInst = new llvm::StoreInst(binInst, varLeft, true, irgen->GetBasicBlock());
		}
	}
	else if(op->IsOp("-="))
	{
		//integers
		if(valType->isIntegerTy())
		{	
			binInst = llvm::BinaryOperator::CreateSub(valueLeft, valueRight, "isub");
			bb->getInstList().push_back(binInst);			

			storeInst = new llvm::StoreInst(binInst, varLeft, true, irgen->GetBasicBlock());
		}
		//float and vector
		else
		{
			binInst = llvm::BinaryOperator::CreateFSub(valueLeft, valueRight, "fsub");
			bb->getInstList().push_back(binInst);

			storeInst = new llvm::StoreInst(binInst, varLeft, true, irgen->GetBasicBlock());
		}
	}
	else if(op->IsOp("+="))
	{
		//integers
		if(valType->isIntegerTy())
		{	
			binInst = llvm::BinaryOperator::CreateAdd(valueLeft, valueRight, "iadd");
			bb->getInstList().push_back(binInst);			

			storeInst = new llvm::StoreInst(binInst, varLeft, true, irgen->GetBasicBlock());
		}
		//float and vector
		else
		{
			binInst = llvm::BinaryOperator::CreateFAdd(valueLeft, valueRight, "fadd");
			bb->getInstList().push_back(binInst);

			storeInst = new llvm::StoreInst(binInst, varLeft, true, irgen->GetBasicBlock());
		}
	}
	
	return storeInst;
}

llvm::Value* PostfixExpr::Emit()
{
	//get llvm value of expr
	llvm::Value *varRight = left->Emit();
	llvm::Value *valueRight = varRight;

	//load value if necessary
	if(llvm::UnaryInstruction::classof(valueRight) || llvm::GlobalVariable::classof(valueRight) || llvm::GetElementPtrInst::classof(valueRight))
	{
		valueRight = new llvm::LoadInst(valueRight, "ld1", irgen->GetBasicBlock());
	}

	llvm::Type *valType = valueRight->getType();
	llvm::Constant *valLeft;
	llvm::BinaryOperator *binInst;
		

	//TODO checking of type not working!
	if(op->IsOp("++"))
	{
		//integer
		if(valType->isIntegerTy())
		{	
			valLeft = llvm::ConstantInt::get(irgen->GetIntType(), 1, true);
			binInst = llvm::BinaryOperator::CreateAdd(valLeft, valueRight, "iadd");

		}
		//float and vector
		else
		{
			valLeft = llvm::ConstantFP::get(irgen->GetFloatType(), 1.0);
			binInst = llvm::BinaryOperator::CreateFAdd(valLeft, valueRight, "fadd");
		}

	}
	else if(op->IsOp("--"))
	{

		//integers
		if(valType->isIntegerTy())
		{	
			valLeft = llvm::ConstantInt::get(valType, 1, true);
			binInst = llvm::BinaryOperator::CreateSub(valueRight, valLeft, "isub");
		}
		//float and vector
		else
		{
			valLeft = llvm::ConstantInt::get(valType, 1, true);
			binInst = llvm::BinaryOperator::CreateFSub(valueRight, valLeft, "fsub");
		}
	}
	
	irgen->GetBasicBlock()->getInstList().push_back(binInst);
	new llvm::StoreInst(binInst, varRight, true, irgen->GetBasicBlock());
	return binInst;
}

ConditionalExpr::ConditionalExpr(Expr *c, Expr *t, Expr *f)
  : Expr(Join(c->GetLocation(), f->GetLocation())) {
    Assert(c != NULL && t != NULL && f != NULL);
    (cond=c)->SetParent(this);
    (trueExpr=t)->SetParent(this);
    (falseExpr=f)->SetParent(this);
}

llvm::Value* ConditionalExpr::Emit()
{
	std::cerr << "CondtionalExpr" << std::endl;

	//ternary operator
	llvm::Value *condValue = cond->Emit();
	llvm::Value *s1Value = trueExpr->Emit();
	llvm::Value *s2Value = falseExpr->Emit();

	//Perform load if necessary
	if(llvm::UnaryInstruction::classof(condValue) || llvm::GlobalVariable::classof(condValue) || llvm::GetElementPtrInst::classof(condValue))
		condValue = new llvm::LoadInst(condValue, "ld1", irgen->GetBasicBlock());
		
	if(llvm::UnaryInstruction::classof(s1Value) || llvm::GlobalVariable::classof(s1Value) || llvm::GetElementPtrInst::classof(s1Value))
		s1Value = new llvm::LoadInst(s1Value, "ld2", irgen->GetBasicBlock());

	if(llvm::UnaryInstruction::classof(s2Value) || llvm::GlobalVariable::classof(s2Value) || llvm::GetElementPtrInst::classof(s2Value))
		s2Value = new llvm::LoadInst(s2Value, "ld3", irgen->GetBasicBlock());

	
	llvm::SelectInst  *ternInst = llvm::SelectInst::Create(condValue, s1Value, s2Value, "ternInst", irgen->GetBasicBlock());

	return ternInst;
}

void ConditionalExpr::PrintChildren(int indentLevel) {
    cond->Print(indentLevel+1, "(cond) ");
    trueExpr->Print(indentLevel+1, "(true) ");
    falseExpr->Print(indentLevel+1, "(false) ");
}
ArrayAccess::ArrayAccess(yyltype loc, Expr *b, Expr *s) : LValue(loc) {
    (base=b)->SetParent(this); 
    (subscript=s)->SetParent(this);
}

llvm::Value* ArrayAccess::Emit()
{
	llvm::Value *llvmBase = base->Emit();
	llvm::Value *index = subscript->Emit();

	//load index if variable
	if(llvm::UnaryInstruction::classof(index) || llvm::GlobalVariable::classof(index) || llvm::GetElementPtrInst::classof(index))
		index = new llvm::LoadInst(index, "ld1", irgen->GetBasicBlock());

	//construct array access param
	std::vector<llvm::Value*> v;
	v.push_back(llvm::ConstantInt::get(irgen->GetIntType(), 0));
	v.push_back(index);
	
	llvm::ArrayRef<llvm::Value*> indxArray(v);
	llvm::GetElementPtrInst *gep = llvm::GetElementPtrInst::CreateInBounds(llvmBase, indxArray, llvmBase->getName(), irgen->GetBasicBlock());

	return gep;
}

void ArrayAccess::PrintChildren(int indentLevel) {
    base->Print(indentLevel+1);
    subscript->Print(indentLevel+1, "(subscript) ");
}
     
FieldAccess::FieldAccess(Expr *b, Identifier *f) 
  : LValue(b? Join(b->GetLocation(), f->GetLocation()) : *f->GetLocation()) {
    Assert(f != NULL); // b can be be NULL (just means no explicit base)
    base = b; 
    if (base) base->SetParent(this); 
    (field=f)->SetParent(this);
}


void FieldAccess::PrintChildren(int indentLevel) {
    if (base) base->Print(indentLevel+1);
    field->Print(indentLevel+1);
}

llvm::Value* FieldAccess::Emit()
{
	//TODO How to get type of global variable. Finish implementation


	llvm::AllocaInst *value = llvm::dyn_cast<llvm::AllocaInst>(base->Emit());
	//llvm::Value *value = base->Emit();
	//llvm::GlobalValue *value = llvm::dyn_cast<llvm::GlobalValue>(base->Emit());
	
	
	llvm::VectorType *vector = llvm::dyn_cast<llvm::VectorType>(value->getAllocatedType());
	



	//vec2
	unsigned int num = 0;
	num = vector->getNumElements();
	std::cerr << "num: " << num << std::endl;
	if(vector->getNumElements() == 2)
		std::cerr << "vec2" << std::endl;
	else if(vector->getNumElements() == 3)
		std::cerr << "vec3" << std::endl;
	else if(vector->getNumElements() == 4)
		std::cerr << "vec4" << std::endl;

	
}

Call::Call(yyltype loc, Expr *b, Identifier *f, List<Expr*> *a) : Expr(loc)  {
    Assert(f != NULL && a != NULL); // b can be be NULL (just means no explicit base)
    base = b;
    if (base) base->SetParent(this);
    (field=f)->SetParent(this);
    (actuals=a)->SetParentAll(this);
}

llvm::Value* Call::Emit()
{
	//get function to call
	llvm::Module *module = irgen->GetOrCreateModule("Module");
	llvm::Function *func = module->getFunction(field->GetName());

	//Store parameters in vector
	std::vector<llvm::Value *> vecArgs;
	for(int i = 0; i < actuals->NumElements(); i++)
	{
		llvm::Value *value = actuals->Nth(i)->Emit();
		if(llvm::UnaryInstruction::classof(value) || llvm::GlobalVariable::classof(value) || llvm::GetElementPtrInst::classof(value))
			value = new llvm::LoadInst(value, "ld1", irgen->GetBasicBlock());

		vecArgs.push_back(value);
	}
	llvm::ArrayRef<llvm::Value*> argsArray(vecArgs);

	//create call
	llvm::CallInst *call = llvm::CallInst::Create(func, argsArray, field->GetName(), irgen->GetBasicBlock());

	return call;
}

void Call::PrintChildren(int indentLevel) {
   if (base) base->Print(indentLevel+1);
   if (field) field->Print(indentLevel+1);
   if (actuals) actuals->PrintAll(indentLevel+1, "(actuals) ");
}

