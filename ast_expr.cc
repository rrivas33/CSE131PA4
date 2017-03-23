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
/*
	if(sym == NULL)
		cerr << "sym = null\n";
	llvm::Type *type = symbolTable->GetType(sym->value);
	
	std::cerr << sym->value->getName().str();
	if(type->isFloatTy())
		std::cerr << "is float type" << std::endl;
	else if(type->isIntegerTy())
		std::cerr << "is integer type" << std::endl;
	else if(llvm::VectorType::classof(type))
		std::cerr << "is vector type" << std::endl;
	else
		std::cerr << "type not recognized" << std::endl;
	cerr << "return happened\n";
*/	
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
		llvm::Value *varRight = right->Emit();
		llvm::Value *valRight = varRight;
		
		llvm::Value *varLeft = left->Emit();
		llvm::Value *valLeft = varLeft;



		//check if either side is a float
		bool valRightFloat = symbolTable->GetType(varRight)->isFloatTy();
		bool valLeftFloat = symbolTable->GetType(varRight)->isFloatTy();


		//load value if necessary
		if(llvm::AllocaInst::classof(valLeft) || llvm::GlobalVariable::classof(valLeft) || llvm::GetElementPtrInst::classof(valLeft))
		{
			valLeft = new llvm::LoadInst(valLeft, "", irgen->GetBasicBlock());
		}

		if(llvm::AllocaInst::classof(valRight) || llvm::GlobalVariable::classof(valRight) || llvm::GetElementPtrInst::classof(valRight))
		{
			valRight = new llvm::LoadInst(valRight, "", irgen->GetBasicBlock());
		}

		//type of values
		llvm::Type *valType = valLeft->getType();
		llvm::BinaryOperator *binInst;

		if(op->IsOp("*"))
		{

			//integers
			if(valType->isIntegerTy())
			{	
				binInst = llvm::BinaryOperator::CreateMul(valLeft, valRight, "");
			}
			//shuffle and float
			else if(llvm::ShuffleVectorInst::classof(valRight) && valLeftFloat)
			{
				//get vector
				llvm::ShuffleVectorInst *shuffle = llvm::dyn_cast<llvm::ShuffleVectorInst>(valRight);
				char *vecName = dynamic_cast<FieldAccess *>(right)->GetBase();
				llvm::Value *var = symbolTable->find(vecName)->value;

	
				llvm::LoadInst *vector = new llvm::LoadInst(var, "", irgen->GetBasicBlock());
				
				llvm::SmallVector<int, 16> mask = shuffle->getShuffleMask();
			
	
				llvm::InsertElementInst *insert = NULL;
				llvm::SmallVector<int, 16>::iterator it = mask.begin();
				int maskSize = 0;

				//calculate mask size
				for(it = mask.begin(); it != mask.end(); it++)
					maskSize++;

				int i = 0;
				for(it = mask.begin(); it != mask.end(); it++, i++)
				{

					if(i == 0)
					{
						if(maskSize == 2)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec2Type()), valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else if(maskSize == 3)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec3Type()), valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec4Type()), valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

					}
					else
						insert = llvm::InsertElementInst::Create(insert, valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
					
				}
				
				binInst = llvm::BinaryOperator::CreateFMul(insert, valRight, "");
					
			}
			//float and shufflevector
			else if(llvm::ShuffleVectorInst::classof(valLeft) && valRightFloat)
			{

				//get vector
				llvm::ShuffleVectorInst *shuffle = llvm::dyn_cast<llvm::ShuffleVectorInst>(valLeft);
				char *vecName = dynamic_cast<FieldAccess *>(left)->GetBase();
				llvm::Value *var = symbolTable->find(vecName)->value;

	
				llvm::LoadInst *vector = new llvm::LoadInst(var, "", irgen->GetBasicBlock());
				
				llvm::SmallVector<int, 16> mask = shuffle->getShuffleMask();
			
	
				llvm::InsertElementInst *insert = NULL;
				llvm::SmallVector<int, 16>::iterator it = mask.begin();
				int maskSize = 0;

				//calculate mask size
				for(it = mask.begin(); it != mask.end(); it++)
					maskSize++;

				int i = 0;
				for(it = mask.begin(); it != mask.end(); it++, i++)
				{

					if(i == 0)
					{
						if(maskSize == 2)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec2Type()), valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else if(maskSize == 3)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec3Type()), valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec4Type()), valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

					}
					else
						insert = llvm::InsertElementInst::Create(insert, valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
					
				}
				
				binInst = llvm::BinaryOperator::CreateFMul(insert, valLeft, "");
			}
			//float and vector
			else if(llvm::VectorType::classof(symbolTable->GetType(varRight)) && valLeftFloat)
			{
				//llvm::LoadInst *vector = new llvm::LoadInst(valRight, "", irgen->GetBasicBlock());
				llvm::VectorType *vecType = llvm::dyn_cast<llvm::VectorType>(symbolTable->GetType(varRight));
				int vecSize = vecType->getNumElements();

				llvm::InsertElementInst *insert = NULL;
				for(int i = 0; i < vecSize; i++)
				{

					if(i == 0)
					{
						if(vecSize == 2)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec2Type()), valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else if(vecSize == 3)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec3Type()), valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec4Type()), valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

					}
					else
						insert = llvm::InsertElementInst::Create(insert, valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
				
				}
				
				binInst = llvm::BinaryOperator::CreateFMul(insert, valRight, "");
			}
			//vector and float
			else if(llvm::VectorType::classof(symbolTable->GetType(varLeft)) && valRightFloat)
			{

				//llvm::LoadInst *vector = new llvm::LoadInst(varLeft, "", irgen->GetBasicBlock());
				llvm::VectorType *vecType = llvm::dyn_cast<llvm::VectorType>(symbolTable->GetType(varLeft));
				int vecSize = vecType->getNumElements();

				llvm::InsertElementInst *insert = NULL;
				for(int i = 0; i < vecSize; i++)
				{

					if(i == 0)
					{
						if(vecSize ==  2)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec2Type()), valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else if(vecSize == 3)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec3Type()), valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec4Type()), valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

					}
					else
						insert = llvm::InsertElementInst::Create(insert, valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
				
				}
				
				binInst = llvm::BinaryOperator::CreateFMul(insert, valLeft, "");
			}
			//floats
			else
			{

				binInst = llvm::BinaryOperator::CreateFMul(valLeft, valRight, "");
			}
		}
		else if(op->IsOp("/"))
		{

			//integers
			if(valType->isIntegerTy())
			{	
				binInst = llvm::BinaryOperator::CreateSDiv(valLeft, valRight, "");
			}
			//shuffle and float
			else if(llvm::ShuffleVectorInst::classof(valRight) && valLeftFloat)
			{
				//get vector
				llvm::ShuffleVectorInst *shuffle = llvm::dyn_cast<llvm::ShuffleVectorInst>(valRight);
				char *vecName = dynamic_cast<FieldAccess *>(right)->GetBase();
				llvm::Value *var = symbolTable->find(vecName)->value;

	
				llvm::LoadInst *vector = new llvm::LoadInst(var, "", irgen->GetBasicBlock());
				
				llvm::SmallVector<int, 16> mask = shuffle->getShuffleMask();
			
	
				llvm::InsertElementInst *insert = NULL;
				llvm::SmallVector<int, 16>::iterator it = mask.begin();
				int maskSize = 0;

				//calculate mask size
				for(it = mask.begin(); it != mask.end(); it++)
					maskSize++;

				int i = 0;
				for(it = mask.begin(); it != mask.end(); it++, i++)
				{

					if(i == 0)
					{
						if(maskSize == 2)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec2Type()), valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else if(maskSize == 3)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec3Type()), valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec4Type()), valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

					}
					else
						insert = llvm::InsertElementInst::Create(insert, valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
					
				}
				
				binInst = llvm::BinaryOperator::CreateFDiv(insert, vector, "");
					
			}
			//float and shufflevector
			else if(llvm::ShuffleVectorInst::classof(valLeft) && valRightFloat)
			{

				//get vector
				llvm::ShuffleVectorInst *shuffle = llvm::dyn_cast<llvm::ShuffleVectorInst>(valLeft);
				char *vecName = dynamic_cast<FieldAccess *>(left)->GetBase();
				llvm::Value *var = symbolTable->find(vecName)->value;

	
				llvm::LoadInst *vector = new llvm::LoadInst(var, "", irgen->GetBasicBlock());
				
				llvm::SmallVector<int, 16> mask = shuffle->getShuffleMask();
			
	
				llvm::InsertElementInst *insert = NULL;
				llvm::SmallVector<int, 16>::iterator it = mask.begin();
				int maskSize = 0;

				//calculate mask size
				for(it = mask.begin(); it != mask.end(); it++)
					maskSize++;

				int i = 0;
				for(it = mask.begin(); it != mask.end(); it++, i++)
				{

					if(i == 0)
					{
						if(maskSize == 2)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec2Type()), valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else if(maskSize == 3)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec3Type()), valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec4Type()), valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

					}
					else
						insert = llvm::InsertElementInst::Create(insert, valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
					
				}
				
				binInst = llvm::BinaryOperator::CreateFDiv(vector, insert, "");
			}
			//float and vector
			else if(llvm::VectorType::classof(symbolTable->GetType(varRight)) && valLeftFloat)
			{
				//llvm::LoadInst *vector = new llvm::LoadInst(valRight, "", irgen->GetBasicBlock());
				llvm::VectorType *vecType = llvm::dyn_cast<llvm::VectorType>(symbolTable->GetType(varRight));
				int vecSize = vecType->getNumElements();

				llvm::InsertElementInst *insert = NULL;
				for(int i = 0; i < vecSize; i++)
				{

					if(i == 0)
					{
						if(vecSize == 2)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec2Type()), valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else if(vecSize == 3)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec3Type()), valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec4Type()), valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

					}
					else
						insert = llvm::InsertElementInst::Create(insert, valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
				
				}
				
				binInst = llvm::BinaryOperator::CreateFDiv(insert, valRight, "");
			}
			//vector and float
			else if(llvm::VectorType::classof(symbolTable->GetType(varLeft)) && valRightFloat)
			{
				//llvm::LoadInst *vector = new llvm::LoadInst(valLeft, "", irgen->GetBasicBlock());
				llvm::VectorType *vecType = llvm::dyn_cast<llvm::VectorType>(valLeft->getType());
				int vecSize = vecType->getNumElements();

				llvm::InsertElementInst *insert = NULL;
				for(int i = 0; i < vecSize; i++)
				{

					if(i == 0)
					{
						if(vecSize ==  2)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec2Type()), valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else if(vecSize == 3)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec3Type()), valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec4Type()), valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

					}
					else
						insert = llvm::InsertElementInst::Create(insert, valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
				
				}
				
				binInst = llvm::BinaryOperator::CreateFDiv(valLeft, insert, "");
			}
			//floats
			else
			{
				binInst = llvm::BinaryOperator::CreateFDiv(valLeft, valRight, "");
			}

		}
		else if(op->IsOp("+"))
		{

			//integers
			if(valType->isIntegerTy())
			{	
				binInst = llvm::BinaryOperator::CreateAdd(valLeft, valRight, "");
			}
			//shuffle and float
			else if(llvm::ShuffleVectorInst::classof(valRight) && valLeftFloat)
			{
				//get vector
				llvm::ShuffleVectorInst *shuffle = llvm::dyn_cast<llvm::ShuffleVectorInst>(valRight);
				char *vecName = dynamic_cast<FieldAccess *>(right)->GetBase();
				llvm::Value *var = symbolTable->find(vecName)->value;

	
				llvm::LoadInst *vector = new llvm::LoadInst(var, "", irgen->GetBasicBlock());
				
				llvm::SmallVector<int, 16> mask = shuffle->getShuffleMask();
			
	
				llvm::InsertElementInst *insert = NULL;
				llvm::SmallVector<int, 16>::iterator it = mask.begin();
				int maskSize = 0;

				//calculate mask size
				for(it = mask.begin(); it != mask.end(); it++)
					maskSize++;

				int i = 0;
				for(it = mask.begin(); it != mask.end(); it++, i++)
				{

					if(i == 0)
					{
						if(maskSize == 2)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec2Type()), valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else if(maskSize == 3)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec3Type()), valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec4Type()), valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

					}
					else
						insert = llvm::InsertElementInst::Create(insert, valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
					
				}
				
				binInst = llvm::BinaryOperator::CreateFAdd(insert, vector, "");
					
			}
			//float and shufflevector
			else if(llvm::ShuffleVectorInst::classof(valLeft) && valRightFloat)
			{

				//get vector
				llvm::ShuffleVectorInst *shuffle = llvm::dyn_cast<llvm::ShuffleVectorInst>(valLeft);
				char *vecName = dynamic_cast<FieldAccess *>(left)->GetBase();
				llvm::Value *var = symbolTable->find(vecName)->value;

	
				llvm::LoadInst *vector = new llvm::LoadInst(var, "", irgen->GetBasicBlock());
				
				llvm::SmallVector<int, 16> mask = shuffle->getShuffleMask();
			
	
				llvm::InsertElementInst *insert = NULL;
				llvm::SmallVector<int, 16>::iterator it = mask.begin();
				int maskSize = 0;

				//calculate mask size
				for(it = mask.begin(); it != mask.end(); it++)
					maskSize++;

				int i = 0;
				for(it = mask.begin(); it != mask.end(); it++, i++)
				{

					if(i == 0)
					{
						if(maskSize == 2)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec2Type()), valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else if(maskSize == 3)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec3Type()), valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec4Type()), valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

					}
					else
						insert = llvm::InsertElementInst::Create(insert, valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
					
				}
				
				binInst = llvm::BinaryOperator::CreateFAdd(vector, insert, "");
			}
			//float and vector
			else if(llvm::VectorType::classof(symbolTable->GetType(varRight)) && valLeftFloat)
			{
				//llvm::LoadInst *vector = new llvm::LoadInst(valRight, "", irgen->GetBasicBlock());
				llvm::VectorType *vecType = llvm::dyn_cast<llvm::VectorType>(symbolTable->GetType(varRight));
				int vecSize = vecType->getNumElements();

				llvm::InsertElementInst *insert = NULL;
				for(int i = 0; i < vecSize; i++)
				{

					if(i == 0)
					{
						if(vecSize == 2)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec2Type()), valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else if(vecSize == 3)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec3Type()), valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec4Type()), valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

					}
					else
						insert = llvm::InsertElementInst::Create(insert, valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
				
				}
				
				binInst = llvm::BinaryOperator::CreateFAdd(insert, valRight, "");
			}
			//vector and float
			else if(llvm::VectorType::classof(symbolTable->GetType(varLeft)) && valRightFloat)
			{
				llvm::LoadInst *vector = new llvm::LoadInst(valLeft, "", irgen->GetBasicBlock());
				llvm::VectorType *vecType = llvm::dyn_cast<llvm::VectorType>(symbolTable->GetType(varLeft));
				int vecSize = vecType->getNumElements();

				llvm::InsertElementInst *insert = NULL;
				for(int i = 0; i < vecSize; i++)
				{

					if(i == 0)
					{
						if(vecSize ==  2)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec2Type()), valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else if(vecSize == 3)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec3Type()), valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec4Type()), valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

					}
					else
						insert = llvm::InsertElementInst::Create(insert, valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
				
				}
				
				binInst = llvm::BinaryOperator::CreateFAdd(valLeft, insert, "");
			}
			//floats
			else
			{
				binInst = llvm::BinaryOperator::CreateFAdd(valLeft, valRight, "");
			}
		}
		else if(op->IsOp("-"))
		{

			//integers
			if(valType->isIntegerTy())
			{	
				binInst = llvm::BinaryOperator::CreateSub(valLeft, valRight, "");

			}
			//shuffle and float
			else if(llvm::ShuffleVectorInst::classof(valRight) && valLeftFloat)
			{
				//get vector
				llvm::ShuffleVectorInst *shuffle = llvm::dyn_cast<llvm::ShuffleVectorInst>(valRight);
				char *vecName = dynamic_cast<FieldAccess *>(right)->GetBase();
				llvm::Value *var = symbolTable->find(vecName)->value;

	
				llvm::LoadInst *vector = new llvm::LoadInst(var, "", irgen->GetBasicBlock());
				
				llvm::SmallVector<int, 16> mask = shuffle->getShuffleMask();
			
	
				llvm::InsertElementInst *insert = NULL;
				llvm::SmallVector<int, 16>::iterator it = mask.begin();
				int maskSize = 0;

				//calculate mask size
				for(it = mask.begin(); it != mask.end(); it++)
					maskSize++;

				int i = 0;
				for(it = mask.begin(); it != mask.end(); it++, i++)
				{

					if(i == 0)
					{
						if(maskSize == 2)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec2Type()), valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else if(maskSize == 3)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec3Type()), valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec4Type()), valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

					}
					else
						insert = llvm::InsertElementInst::Create(insert, valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
					
				}
				
				binInst = llvm::BinaryOperator::CreateFSub(insert, vector, "");
					
			}
			//float and shufflevector
			else if(llvm::ShuffleVectorInst::classof(valLeft) && valRightFloat)
			{

				//get vector
				llvm::ShuffleVectorInst *shuffle = llvm::dyn_cast<llvm::ShuffleVectorInst>(valLeft);
				char *vecName = dynamic_cast<FieldAccess *>(left)->GetBase();
				llvm::Value *var = symbolTable->find(vecName)->value;

	
				llvm::LoadInst *vector = new llvm::LoadInst(var, "", irgen->GetBasicBlock());
				
				llvm::SmallVector<int, 16> mask = shuffle->getShuffleMask();
			
	
				llvm::InsertElementInst *insert = NULL;
				llvm::SmallVector<int, 16>::iterator it = mask.begin();
				int maskSize = 0;

				//calculate mask size
				for(it = mask.begin(); it != mask.end(); it++)
					maskSize++;

				int i = 0;
				for(it = mask.begin(); it != mask.end(); it++, i++)
				{

					if(i == 0)
					{
						if(maskSize == 2)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec2Type()), valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else if(maskSize == 3)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec3Type()), valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec4Type()), valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

					}
					else
						insert = llvm::InsertElementInst::Create(insert, valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
					
				}
				
				binInst = llvm::BinaryOperator::CreateFSub(vector, insert, "");
			}
			//float and vector
			else if(llvm::VectorType::classof(symbolTable->GetType(varRight)) && valLeftFloat)
			{
				//llvm::LoadInst *vector = new llvm::LoadInst(valRight, "", irgen->GetBasicBlock());
				llvm::VectorType *vecType = llvm::dyn_cast<llvm::VectorType>(symbolTable->GetType(varRight));
				int vecSize = vecType->getNumElements();

				llvm::InsertElementInst *insert = NULL;
				for(int i = 0; i < vecSize; i++)
				{

					if(i == 0)
					{
						if(vecSize == 2)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec2Type()), valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else if(vecSize == 3)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec3Type()), valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec4Type()), valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

					}
					else
						insert = llvm::InsertElementInst::Create(insert, valLeft, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
				
				}
				
				binInst = llvm::BinaryOperator::CreateFSub(insert, valRight, "");
			}
			//vector and float
			else if(llvm::VectorType::classof(symbolTable->GetType(varLeft)) && valRightFloat)
			{
				//llvm::LoadInst *vector = new llvm::LoadInst(valLeft, "", irgen->GetBasicBlock());
				llvm::VectorType *vecType = llvm::dyn_cast<llvm::VectorType>(symbolTable->GetType(varLeft));
				int vecSize = vecType->getNumElements();

				llvm::InsertElementInst *insert = NULL;
				for(int i = 0; i < vecSize; i++)
				{

					if(i == 0)
					{
						if(vecSize ==  2)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec2Type()), valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else if(vecSize == 3)
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec3Type()), valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
						else
							insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec4Type()), valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

					}
					else
						insert = llvm::InsertElementInst::Create(insert, valRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
				
				}
				
				binInst = llvm::BinaryOperator::CreateFSub(valLeft, insert, "");
			}
			//float and vector
			else
			{
				binInst = llvm::BinaryOperator::CreateFSub(valLeft, valRight, "");
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
		if(llvm::AllocaInst::classof(valueRight) || llvm::GlobalVariable::classof(valueRight) || llvm::GetElementPtrInst::classof(valueRight))
		{
			valueRight = new llvm::LoadInst(valueRight, "", irgen->GetBasicBlock());
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
				binInst = llvm::BinaryOperator::CreateAdd(valLeft, valueRight, "");

			}
			//float and vector
			else
			{
				valLeft = llvm::ConstantFP::get(irgen->GetFloatType(), 1.0);
				binInst = llvm::BinaryOperator::CreateFAdd(valLeft, valueRight, "");
			}

		}
		else if(op->IsOp("--"))
		{

			//integers
			if(valType->isIntegerTy())
			{	
				valLeft = llvm::ConstantInt::get(valType, 1, true);
				binInst = llvm::BinaryOperator::CreateSub(valueRight, valLeft, "");
		
			}
			//float and vector
			else
			{
				valLeft = llvm::ConstantFP::get(valType, 1.0);
				binInst = llvm::BinaryOperator::CreateFSub(valueRight, valLeft, "");
			}
		}
		else if(op->IsOp("-"))
		{
			//integers
			if(valType->isIntegerTy())
			{	
				valLeft = llvm::ConstantInt::get(valType, 0, true);
				binInst = llvm::BinaryOperator::CreateSub(valLeft, valueRight, "");
				bb->getInstList().push_back(binInst);
				return binInst;
				
			}
			//float and vector
			else
			{
				valLeft = llvm::ConstantFP::get(valType, 0.0);
				binInst = llvm::BinaryOperator::CreateFSub(valLeft, valueRight, "");
				bb->getInstList().push_back(binInst);
				return binInst;
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
	llvm::Type *type = symbolTable->GetType(val1);
	llvm::CmpInst *cmp;
	llvm::BasicBlock *bb = irgen->GetBasicBlock();

	//load variable if necessary
	if(llvm::AllocaInst::classof(val1) || llvm::GlobalVariable::classof(val1) || llvm::GetElementPtrInst::classof(val1))
	{
		val1 = new llvm::LoadInst(val1, "", irgen->GetBasicBlock());
	}

	if(llvm::AllocaInst::classof(val2) || llvm::GlobalVariable::classof(val2) || llvm::GetElementPtrInst::classof(val2))
	{
		val2 = new llvm::LoadInst(val2, "", irgen->GetBasicBlock());
	}

	//generate compare instruction
	if(op->IsOp("<"))
	{
		if(type->isFloatTy())
			cmp = new llvm::FCmpInst(*(irgen->GetBasicBlock()), llvm::CmpInst::FCMP_OLT, val1, val2, "");
		else
			cmp = new llvm::ICmpInst(*(irgen->GetBasicBlock()), llvm::CmpInst::ICMP_ULT, val1, val2, "");
	}
	else if(op->IsOp(">"))
	{
		
		if(type->isFloatTy())
			cmp = new llvm::FCmpInst(*(irgen->GetBasicBlock()), llvm::CmpInst::FCMP_OGT, val1, val2, "");
		else
			cmp = new llvm::ICmpInst(*(irgen->GetBasicBlock()), llvm::CmpInst::ICMP_UGT, val1, val2, "");
		
	}
	else if(op->IsOp("<="))
	{
		if(type->isFloatTy())
			cmp = new llvm::FCmpInst(*(irgen->GetBasicBlock()), llvm::CmpInst::FCMP_OLE, val1, val2, "");
		else
			cmp = new llvm::ICmpInst(*(irgen->GetBasicBlock()), llvm::CmpInst::ICMP_ULE, val1, val2, "");	
	}
	else if(op->IsOp(">="))
	{
		if(type->isFloatTy())
			cmp = new llvm::FCmpInst(*(irgen->GetBasicBlock()), llvm::CmpInst::FCMP_OGE, val1, val2, "");
		else
			cmp = new llvm::ICmpInst(*(irgen->GetBasicBlock()), llvm::CmpInst::ICMP_UGE, val1, val2, "");
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
	llvm::Type *type = symbolTable->GetType(val1);

	//load variable if necessary
	if(llvm::AllocaInst::classof(val1) || llvm::GlobalVariable::classof(val1) || llvm::GetElementPtrInst::classof(val1))
	{
		val1 = new llvm::LoadInst(val1, "", irgen->GetBasicBlock());
	}

	if(llvm::AllocaInst::classof(val2) || llvm::GlobalVariable::classof(val2) || llvm::GetElementPtrInst::classof(val2))
	{
		val2 = new llvm::LoadInst(val2, "", irgen->GetBasicBlock());
	}

	//generate compare inst
	llvm::CmpInst *cmp;
	if(op->IsOp("=="))
	{
		if(type->isFloatTy())
		{
			cmp = new llvm::FCmpInst(*(irgen->GetBasicBlock()), llvm::CmpInst::FCMP_OEQ, val1, val2, "");
		}
		else
			cmp = new llvm::ICmpInst(*(irgen->GetBasicBlock()), llvm::CmpInst::ICMP_EQ, val1, val2, "");
	}
	else if(op->IsOp("!="))
	{
		if(type->isFloatTy())
			cmp = new llvm::FCmpInst(*(irgen->GetBasicBlock()), llvm::CmpInst::FCMP_ONE, val1, val2, "");
		else
			cmp = new llvm::ICmpInst(*(irgen->GetBasicBlock()), llvm::CmpInst::ICMP_NE, val1, val2, "");
	}
	
	return cmp;
}

llvm::Value* LogicalExpr::Emit()
{
	llvm::Value *val2 = right->Emit();
	llvm::Value *val1 = left->Emit();

	llvm::Type *type = val1->getType();

	//load variable if necessary
	if(llvm::AllocaInst::classof(val1) || llvm::GlobalVariable::classof(val1) || llvm::GetElementPtrInst::classof(val1))
	{
		val1 = new llvm::LoadInst(val1, "", irgen->GetBasicBlock());
	}

	if(llvm::AllocaInst::classof(val2) || llvm::GlobalVariable::classof(val2) || llvm::GetElementPtrInst::classof(val2))
	{
		val2 = new llvm::LoadInst(val2, "", irgen->GetBasicBlock());
	}

	//generator inst
	if(op->IsOp("&&"))
	{

		llvm::BinaryOperator *binAnd = llvm::BinaryOperator::CreateAnd(val1, val2, "");
		irgen->GetBasicBlock()->getInstList().push_back(binAnd);
		return binAnd;
	}
	else if(op->IsOp("||"))
	{
		llvm::BinaryOperator *binOr = llvm::BinaryOperator::CreateOr(val1, val2, "");
		irgen->GetBasicBlock()->getInstList().push_back(binOr);
		return binOr;
	}
}

llvm::Value* AssignExpr::Emit()
{	

	//current basic block
	llvm::BasicBlock *bb = irgen->GetBasicBlock();

	//get llvm value of expressions
	llvm::Value *varRight = right->Emit();
	llvm::Value *valueRight = varRight;
	llvm::Value *varLeft = left->Emit();
	llvm::Value *valueLeft = varLeft;
	llvm::StoreInst *storeInst;

	//load right value if necessary
	if(llvm::AllocaInst::classof(valueRight) || llvm::GlobalVariable::classof(valueRight) || llvm::GetElementPtrInst::classof(valueRight))
	{
		valueRight = new llvm::LoadInst(valueRight, "", irgen->GetBasicBlock());
	}

	//simple assigment
	if(op->IsOp("="))
	{

		//setting one vector component
		if(llvm::ExtractElementInst::classof(varLeft))
		{

			//get vector
			llvm::ExtractElementInst *elmt = llvm::dyn_cast<llvm::ExtractElementInst>(varLeft);
			char *vecName = dynamic_cast<FieldAccess *>(left)->GetBase();
			llvm::Value *var = symbolTable->find(vecName)->value;
			
			llvm::LoadInst *vector = new llvm::LoadInst(var, "", irgen->GetBasicBlock());

			//insert new elemtn
			llvm::InsertElementInst *insert = llvm::InsertElementInst::Create(vector, valueRight, elmt->getIndexOperand(), "insert", 
																				irgen->GetBasicBlock());
			//store insertion
			new llvm::StoreInst(insert, var, true, irgen->GetBasicBlock());	
			
			return valueRight;
		}
		//setting vector components
		else if(llvm::ShuffleVectorInst::classof(varLeft))
		{
			//get vector
			llvm::ShuffleVectorInst *shuffle = llvm::dyn_cast<llvm::ShuffleVectorInst>(varLeft);
			char *vecName = dynamic_cast<FieldAccess *>(left)->GetBase();
			llvm::Value *var = symbolTable->find(vecName)->value;

			llvm::LoadInst *vector = new llvm::LoadInst(var, "", irgen->GetBasicBlock());

			llvm::SmallVector<int, 16> mask = shuffle->getShuffleMask();
			
	
			llvm::InsertElementInst *insert = NULL;
			int i = 0;
			for(llvm::SmallVector<int, 16>::iterator it = mask.begin(); it != mask.end(); it++, i++)
			{
				llvm::ExtractElementInst *ext = llvm::ExtractElementInst::Create(valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

				if(i == 0)
					insert = llvm::InsertElementInst::Create(vector, ext, llvm::ConstantInt::get(irgen->GetIntType(), *it), "", irgen->GetBasicBlock());
				else
					insert = llvm::InsertElementInst::Create(insert, ext, llvm::ConstantInt::get(irgen->GetIntType(), *it), "", irgen->GetBasicBlock());
					
			}
				
			//store vector
			new llvm::StoreInst(insert, var, true, irgen->GetBasicBlock());
	
			return valueRight;
		

		}
		else
		{

			storeInst = new llvm::StoreInst(valueRight, varLeft, true, irgen->GetBasicBlock());
			return valueRight;
		}
	}


	//check if either side is of float type
	bool valRightFloat = irgen->IsFloatType(valueRight);
	bool valLeftFloat = irgen->IsFloatType(valueLeft);



	//load left value
	if(llvm::AllocaInst::classof(valueLeft) || llvm::GlobalVariable::classof(valueLeft) || llvm::GetElementPtrInst::classof(valueLeft))
	{
		valueLeft = new llvm::LoadInst(valueLeft, "", irgen->GetBasicBlock());
	}


	//generate binary operation and store inst
	llvm::Type *valType = symbolTable->GetType(varLeft);
	llvm::BinaryOperator *binInst;



	if(op->IsOp("*="))
	{

		//integers
		if(valType->isIntegerTy())
		{	
			binInst = llvm::BinaryOperator::CreateMul(valueLeft, valueRight, "");
			bb->getInstList().push_back(binInst);			

			new llvm::StoreInst(binInst, varLeft, true, irgen->GetBasicBlock());
			return binInst;		
		}
		//shuffle * float
		else if(llvm::ShuffleVectorInst::classof(varLeft) && valRightFloat)
		{
			//get vector
			llvm::ShuffleVectorInst *shuffle = llvm::dyn_cast<llvm::ShuffleVectorInst>(varLeft);
			char *vecName = dynamic_cast<FieldAccess *>(left)->GetBase();
			llvm::Value *var = symbolTable->find(vecName)->value;

	
			llvm::LoadInst *vector = new llvm::LoadInst(var, "", irgen->GetBasicBlock());
			
			llvm::SmallVector<int, 16> mask = shuffle->getShuffleMask();
			
	
			llvm::InsertElementInst *insert = NULL;
			llvm::SmallVector<int, 16>::iterator it = mask.begin();
			int maskSize = 0;

			//calculate mask size
			for(it = mask.begin(); it != mask.end(); it++)
				maskSize++;

			int i = 0;
			for(it = mask.begin(); it != mask.end(); it++, i++)
			{

				if(i == 0)
				{
					if(maskSize == 2)
						insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec2Type()), valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
					else if(maskSize == 3)
						insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec3Type()), valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
					else
						insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec4Type()), valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

					}
				else
					insert = llvm::InsertElementInst::Create(insert, valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
					
			}//end for
				

			//calculate muliplication
			binInst = llvm::BinaryOperator::CreateFMul(insert, valueLeft, "");
			bb->getInstList().push_back(binInst);			

			//add result to vector
			i = 0;
			for(it = mask.begin(); it != mask.end(); it++, i++)
			{
				llvm::ExtractElementInst *ext = llvm::ExtractElementInst::Create(binInst, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

				if(i == 0)
					insert = llvm::InsertElementInst::Create(vector, ext, llvm::ConstantInt::get(irgen->GetIntType(), *it), "", irgen->GetBasicBlock());
				else
					insert = llvm::InsertElementInst::Create(insert, ext, llvm::ConstantInt::get(irgen->GetIntType(), *it), "", irgen->GetBasicBlock());
					
			}

			new llvm::StoreInst(insert, var, true, irgen->GetBasicBlock());
			return binInst;

		}
		// shuffle * (shuffle or vector)
		else if(llvm::ShuffleVectorInst::classof(varLeft) && (llvm::ShuffleVectorInst::classof(varRight) || llvm::VectorType::classof(symbolTable->GetType(varRight)) ))
		{
			//get shuffle and vector variable
			llvm::ShuffleVectorInst *shuffle = llvm::dyn_cast<llvm::ShuffleVectorInst>(valueLeft);
			char *vecName = dynamic_cast<FieldAccess *>(left)->GetBase();
			llvm::Value *var = symbolTable->find(vecName)->value;

			//load vector
			llvm::LoadInst *vector = new llvm::LoadInst(var, "", irgen->GetBasicBlock());

			//calcualte mult
			binInst = llvm::BinaryOperator::CreateFMul(valueRight, valueLeft, "");
			bb->getInstList().push_back(binInst);

			//get valueLeft's mask from shuffle
			llvm::SmallVector<int, 16> mask = shuffle->getShuffleMask();
			llvm::InsertElementInst *insert = NULL;
				

			//insert into vector
			int i = 0;
			for(llvm::SmallVector<int, 16>::iterator it = mask.begin(); it != mask.end(); it++, i++)
			{
				llvm::ExtractElementInst *ext = llvm::ExtractElementInst::Create(binInst, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

				if(i == 0)
					insert = llvm::InsertElementInst::Create(vector, ext, llvm::ConstantInt::get(irgen->GetIntType(), *it), "", irgen->GetBasicBlock());
				else
					insert = llvm::InsertElementInst::Create(insert, ext, llvm::ConstantInt::get(irgen->GetIntType(), *it), "", irgen->GetBasicBlock());
			}

			new llvm::StoreInst(insert, var, true, irgen->GetBasicBlock());
			return binInst;
				
		}
		//vector * float
		else if(llvm::VectorType::classof(symbolTable->GetType(varLeft)) && valRightFloat)
		{

			llvm::LoadInst *vector = new llvm::LoadInst(varLeft, "", irgen->GetBasicBlock());
			llvm::VectorType *vecType = llvm::dyn_cast<llvm::VectorType>(symbolTable->GetType(varLeft));
			int vecSize = vecType->getNumElements();



			//build new vector with right float value
			llvm::InsertElementInst *insert = NULL;
			int i;
			for(i = 0; i < vecSize; i++)
			{

				if(i == 0)
				{
					if(vecSize == 2)
						insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec2Type()), valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
					else if(vecSize == 3)
						insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec3Type()), valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
					else
						insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec4Type()), valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

				}
				else
					insert = llvm::InsertElementInst::Create(insert, valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
				
			}
	
			//calculate mult				
			binInst = llvm::BinaryOperator::CreateFMul(insert, vector, "");
			bb->getInstList().push_back(binInst);			

			new llvm::StoreInst(binInst, varLeft, true, irgen->GetBasicBlock());
			return binInst;
			
		}
		//vector component
		else if(llvm::ExtractElementInst::classof(varLeft))
		{
			llvm::ExtractElementInst *ext = llvm::dyn_cast<llvm::ExtractElementInst>(varLeft);

			//calculate div
			binInst = llvm::BinaryOperator::CreateFMul(valueLeft, valueRight, "");
			bb->getInstList().push_back(binInst);

			//load vector
			char *vecName = dynamic_cast<FieldAccess *>(left)->GetBase();
			llvm::Value *var = symbolTable->find(vecName)->value;
			llvm::LoadInst *vector = new llvm::LoadInst(var, "", irgen->GetBasicBlock());

			//isert value into vector

			llvm::InsertElementInst *insert = llvm::InsertElementInst::Create(vector, binInst, ext->getIndexOperand(), "", irgen->GetBasicBlock());

			new llvm::StoreInst(insert, var, true, irgen->GetBasicBlock());
			return binInst;
		}
		//floats
		else
		{
			binInst = llvm::BinaryOperator::CreateFMul(valueLeft, valueRight, "");
			bb->getInstList().push_back(binInst);

			new llvm::StoreInst(binInst, varLeft, true, irgen->GetBasicBlock());
			return binInst;
		}
	
	}
	else if(op->IsOp("/="))
	{

		//integers
		if(valType->isIntegerTy())
		{	
			binInst = llvm::BinaryOperator::CreateSDiv(valueLeft, valueRight, "");
			bb->getInstList().push_back(binInst);			

			new llvm::StoreInst(binInst, varLeft, true, irgen->GetBasicBlock());
			return binInst;
		}
		//shuffle * float
		else if(llvm::ShuffleVectorInst::classof(varLeft) && valRightFloat)
		{
			//get vector
			llvm::ShuffleVectorInst *shuffle = llvm::dyn_cast<llvm::ShuffleVectorInst>(varLeft);
			char *vecName = dynamic_cast<FieldAccess *>(left)->GetBase();
			llvm::Value *var = symbolTable->find(vecName)->value;

	
			llvm::LoadInst *vector = new llvm::LoadInst(var, "", irgen->GetBasicBlock());
			
			llvm::SmallVector<int, 16> mask = shuffle->getShuffleMask();
			
	
			llvm::InsertElementInst *insert = NULL;
			llvm::SmallVector<int, 16>::iterator it = mask.begin();
			int maskSize = 0;

			//calculate mask size
			for(it = mask.begin(); it != mask.end(); it++)
				maskSize++;

			int i = 0;
			for(it = mask.begin(); it != mask.end(); it++, i++)
			{

				if(i == 0)
				{
					if(maskSize == 2)
						insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec2Type()), valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
					else if(maskSize == 3)
						insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec3Type()), valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
					else
						insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec4Type()), valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

					}
				else
					insert = llvm::InsertElementInst::Create(insert, valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
					
			}//end for
				

			//calculate muliplication
			binInst = llvm::BinaryOperator::CreateFDiv(valueLeft, insert, "");
			bb->getInstList().push_back(binInst);			

			//add result to vector
			i = 0;
			for(it = mask.begin(); it != mask.end(); it++, i++)
			{
				llvm::ExtractElementInst *ext = llvm::ExtractElementInst::Create(binInst, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

				if(i == 0)
					insert = llvm::InsertElementInst::Create(vector, ext, llvm::ConstantInt::get(irgen->GetIntType(), *it), "", irgen->GetBasicBlock());
				else
					insert = llvm::InsertElementInst::Create(insert, ext, llvm::ConstantInt::get(irgen->GetIntType(), *it), "", irgen->GetBasicBlock());
					
			}

			new llvm::StoreInst(insert, var, true, irgen->GetBasicBlock());
			return binInst;

		}
		// shuffle * (shuffle or vector)
		else if(llvm::ShuffleVectorInst::classof(varLeft) && (llvm::ShuffleVectorInst::classof(varRight) || llvm::VectorType::classof(symbolTable->GetType(varRight)) ))
		{
			//get shuffle and vector variable
			llvm::ShuffleVectorInst *shuffle = llvm::dyn_cast<llvm::ShuffleVectorInst>(valueLeft);
			char *vecName = dynamic_cast<FieldAccess *>(left)->GetBase();
			llvm::Value *var = symbolTable->find(vecName)->value;

			//load vector
			llvm::LoadInst *vector = new llvm::LoadInst(var, "", irgen->GetBasicBlock());

			//calcualte mult
			binInst = llvm::BinaryOperator::CreateFDiv(valueLeft, valueRight, "");
			bb->getInstList().push_back(binInst);

			//get valueLeft's mask from shuffle
			llvm::SmallVector<int, 16> mask = shuffle->getShuffleMask();
			llvm::InsertElementInst *insert = NULL;
				

			//insert into vector
			int i = 0;
			for(llvm::SmallVector<int, 16>::iterator it = mask.begin(); it != mask.end(); it++, i++)
			{
				llvm::ExtractElementInst *ext = llvm::ExtractElementInst::Create(binInst, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

				if(i == 0)
					insert = llvm::InsertElementInst::Create(vector, ext, llvm::ConstantInt::get(irgen->GetIntType(), *it), "", irgen->GetBasicBlock());
				else
					insert = llvm::InsertElementInst::Create(insert, ext, llvm::ConstantInt::get(irgen->GetIntType(), *it), "", irgen->GetBasicBlock());
			}

			new llvm::StoreInst(insert, var, true, irgen->GetBasicBlock());
			return binInst;
				
		}
		//vector * float
		else if(llvm::VectorType::classof(symbolTable->GetType(varLeft)) && valRightFloat)
		{

			llvm::LoadInst *vector = new llvm::LoadInst(varLeft, "", irgen->GetBasicBlock());
			llvm::VectorType *vecType = llvm::dyn_cast<llvm::VectorType>(symbolTable->GetType(varLeft));
			int vecSize = vecType->getNumElements();



			//build new vector with right float value
			llvm::InsertElementInst *insert = NULL;
			int i;
			for(i = 0; i < vecSize; i++)
			{

				if(i == 0)
				{
					if(vecSize == 2)
						insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec2Type()), valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
					else if(vecSize == 3)
						insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec3Type()), valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
					else
						insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec4Type()), valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

				}
				else
					insert = llvm::InsertElementInst::Create(insert, valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
				
			}
	
			//calculate mult				
			binInst = llvm::BinaryOperator::CreateFDiv(vector, insert, "");
			bb->getInstList().push_back(binInst);			
	
			new llvm::StoreInst(binInst, varLeft, true, irgen->GetBasicBlock());
			return binInst;
			
		}
		//vector component
		else if(llvm::ExtractElementInst::classof(varLeft))
		{
			llvm::ExtractElementInst *ext = llvm::dyn_cast<llvm::ExtractElementInst>(varLeft);

			//calculate div
			binInst = llvm::BinaryOperator::CreateFDiv(valueLeft, valueRight, "");
			bb->getInstList().push_back(binInst);

			//load vector
			char *vecName = dynamic_cast<FieldAccess *>(left)->GetBase();
			llvm::Value *var = symbolTable->find(vecName)->value;
			llvm::LoadInst *vector = new llvm::LoadInst(var, "", irgen->GetBasicBlock());

			//isert value into vector

			llvm::InsertElementInst *insert = llvm::InsertElementInst::Create(vector, binInst, ext->getIndexOperand(), "", irgen->GetBasicBlock());

			new llvm::StoreInst(insert, var, true, irgen->GetBasicBlock());
			return binInst;
		}
		//floats
		else
		{
			binInst = llvm::BinaryOperator::CreateFDiv(valueLeft, valueRight, "");
			bb->getInstList().push_back(binInst);

			new llvm::StoreInst(binInst, varLeft, true, irgen->GetBasicBlock());
			return binInst;		
		}
	}
	else if(op->IsOp("-="))
	{
		//integers
		if(valType->isIntegerTy())
		{	
			binInst = llvm::BinaryOperator::CreateSub(valueLeft, valueRight, "");
			bb->getInstList().push_back(binInst);			

			new llvm::StoreInst(binInst, varLeft, true, irgen->GetBasicBlock());
			return binInst;		
		}
		//shuffle * float
		else if(llvm::ShuffleVectorInst::classof(varLeft) && valRightFloat)
		{
			//get vector
			llvm::ShuffleVectorInst *shuffle = llvm::dyn_cast<llvm::ShuffleVectorInst>(varLeft);
			char *vecName = dynamic_cast<FieldAccess *>(left)->GetBase();
			llvm::Value *var = symbolTable->find(vecName)->value;

	
			llvm::LoadInst *vector = new llvm::LoadInst(var, "", irgen->GetBasicBlock());
			
			llvm::SmallVector<int, 16> mask = shuffle->getShuffleMask();
			
	
			llvm::InsertElementInst *insert = NULL;
			llvm::SmallVector<int, 16>::iterator it = mask.begin();
			int maskSize = 0;

			//calculate mask size
			for(it = mask.begin(); it != mask.end(); it++)
				maskSize++;

			int i = 0;
			for(it = mask.begin(); it != mask.end(); it++, i++)
			{

				if(i == 0)
				{
					if(maskSize == 2)
						insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec2Type()), valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
					else if(maskSize == 3)
						insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec3Type()), valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
					else
						insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec4Type()), valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

					}
				else
					insert = llvm::InsertElementInst::Create(insert, valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
					
			}//end for
				

			//calculate muliplication
			binInst = llvm::BinaryOperator::CreateFSub(valueLeft, insert, "");
			bb->getInstList().push_back(binInst);			

			//add result to vector
			i = 0;
			for(it = mask.begin(); it != mask.end(); it++, i++)
			{
				llvm::ExtractElementInst *ext = llvm::ExtractElementInst::Create(binInst, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

				if(i == 0)
					insert = llvm::InsertElementInst::Create(vector, ext, llvm::ConstantInt::get(irgen->GetIntType(), *it), "", irgen->GetBasicBlock());
				else
					insert = llvm::InsertElementInst::Create(insert, ext, llvm::ConstantInt::get(irgen->GetIntType(), *it), "", irgen->GetBasicBlock());
					
			}

			new llvm::StoreInst(insert, var, true, irgen->GetBasicBlock());
			return binInst;

		}
		// shuffle * (shuffle or vector)
		else if(llvm::ShuffleVectorInst::classof(varLeft) && (llvm::ShuffleVectorInst::classof(varRight) || llvm::VectorType::classof(symbolTable->GetType(varRight)) ))
		{
			//get shuffle and vector variable
			llvm::ShuffleVectorInst *shuffle = llvm::dyn_cast<llvm::ShuffleVectorInst>(valueLeft);
			char *vecName = dynamic_cast<FieldAccess *>(left)->GetBase();
			llvm::Value *var = symbolTable->find(vecName)->value;

			//load vector
			llvm::LoadInst *vector = new llvm::LoadInst(var, "", irgen->GetBasicBlock());

			//calcualte mult
			binInst = llvm::BinaryOperator::CreateFSub(valueLeft, valueRight, "");
			bb->getInstList().push_back(binInst);

			//get valueLeft's mask from shuffle
			llvm::SmallVector<int, 16> mask = shuffle->getShuffleMask();
			llvm::InsertElementInst *insert = NULL;
				

			//insert into vector
			int i = 0;
			for(llvm::SmallVector<int, 16>::iterator it = mask.begin(); it != mask.end(); it++, i++)
			{
				llvm::ExtractElementInst *ext = llvm::ExtractElementInst::Create(binInst, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

				if(i == 0)
					insert = llvm::InsertElementInst::Create(vector, ext, llvm::ConstantInt::get(irgen->GetIntType(), *it), "", irgen->GetBasicBlock());
				else
					insert = llvm::InsertElementInst::Create(insert, ext, llvm::ConstantInt::get(irgen->GetIntType(), *it), "", irgen->GetBasicBlock());
			}

			new llvm::StoreInst(insert, var, true, irgen->GetBasicBlock());
			return binInst;
				
		}
		//vector * float
		else if(llvm::VectorType::classof(symbolTable->GetType(varLeft)) && valRightFloat)
		{

			llvm::LoadInst *vector = new llvm::LoadInst(varLeft, "", irgen->GetBasicBlock());
			llvm::VectorType *vecType = llvm::dyn_cast<llvm::VectorType>(symbolTable->GetType(varLeft));
			int vecSize = vecType->getNumElements();



			//build new vector with right float value
			llvm::InsertElementInst *insert = NULL;
			int i;
			for(i = 0; i < vecSize; i++)
			{

				if(i == 0)
				{
					if(vecSize == 2)
						insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec2Type()), valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
					else if(vecSize == 3)
						insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec3Type()), valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
					else
						insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec4Type()), valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

				}
				else
					insert = llvm::InsertElementInst::Create(insert, valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
				
			}
	
			//calculate subtraction				
			binInst = llvm::BinaryOperator::CreateFSub(vector, insert, "");
			bb->getInstList().push_back(binInst);			

			new llvm::StoreInst(binInst, varLeft, true, irgen->GetBasicBlock());
			return binInst;
			
		}
		//vector component
		else if(llvm::ExtractElementInst::classof(varLeft))
		{
			llvm::ExtractElementInst *ext = llvm::dyn_cast<llvm::ExtractElementInst>(varLeft);

			//calculate div
			binInst = llvm::BinaryOperator::CreateFSub(valueLeft, valueRight, "");
			bb->getInstList().push_back(binInst);

			//load vector
			char *vecName = dynamic_cast<FieldAccess *>(left)->GetBase();
			llvm::Value *var = symbolTable->find(vecName)->value;
			llvm::LoadInst *vector = new llvm::LoadInst(var, "", irgen->GetBasicBlock());

			//isert value into vector

			llvm::InsertElementInst *insert = llvm::InsertElementInst::Create(vector, binInst, ext->getIndexOperand(), "", irgen->GetBasicBlock());

			new llvm::StoreInst(insert, var, true, irgen->GetBasicBlock());
			return binInst;
		}
		//floats
		else
		{
			binInst = llvm::BinaryOperator::CreateFSub(valueLeft, valueRight, "");
			bb->getInstList().push_back(binInst);

			new llvm::StoreInst(binInst, varLeft, true, irgen->GetBasicBlock());
			return binInst;
		}
	}
	else if(op->IsOp("+="))
	{
		//integers
		if(valType->isIntegerTy())
		{	
			binInst = llvm::BinaryOperator::CreateAdd(valueLeft, valueRight, "");
			bb->getInstList().push_back(binInst);			

			new llvm::StoreInst(binInst, varLeft, true, irgen->GetBasicBlock());
			return binInst;
		}
		//shuffle * float
		else if(llvm::ShuffleVectorInst::classof(varLeft) && valRightFloat)
		{
			//get vector
			llvm::ShuffleVectorInst *shuffle = llvm::dyn_cast<llvm::ShuffleVectorInst>(varLeft);
			char *vecName = dynamic_cast<FieldAccess *>(left)->GetBase();
			llvm::Value *var = symbolTable->find(vecName)->value;

	
			llvm::LoadInst *vector = new llvm::LoadInst(var, "", irgen->GetBasicBlock());
			
			llvm::SmallVector<int, 16> mask = shuffle->getShuffleMask();
			
	
			llvm::InsertElementInst *insert = NULL;
			llvm::SmallVector<int, 16>::iterator it = mask.begin();
			int maskSize = 0;

			//calculate mask size
			for(it = mask.begin(); it != mask.end(); it++)
				maskSize++;

			int i = 0;
			for(it = mask.begin(); it != mask.end(); it++, i++)
			{

				if(i == 0)
				{
					if(maskSize == 2)
						insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec2Type()), valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
					else if(maskSize == 3)
						insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec3Type()), valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
					else
						insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec4Type()), valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

					}
				else
					insert = llvm::InsertElementInst::Create(insert, valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
					
			}//end for
				

			//calculate muliplication
			binInst = llvm::BinaryOperator::CreateFAdd(valueLeft, insert, "");
			bb->getInstList().push_back(binInst);			

			//add result to vector
			i = 0;
			for(it = mask.begin(); it != mask.end(); it++, i++)
			{
				llvm::ExtractElementInst *ext = llvm::ExtractElementInst::Create(binInst, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

				if(i == 0)
					insert = llvm::InsertElementInst::Create(vector, ext, llvm::ConstantInt::get(irgen->GetIntType(), *it), "", irgen->GetBasicBlock());
				else
					insert = llvm::InsertElementInst::Create(insert, ext, llvm::ConstantInt::get(irgen->GetIntType(), *it), "", irgen->GetBasicBlock());
					
			}

			new llvm::StoreInst(insert, var, true, irgen->GetBasicBlock());
			return binInst;

		}
		// shuffle * (shuffle or vector)
		else if(llvm::ShuffleVectorInst::classof(varLeft) && (llvm::ShuffleVectorInst::classof(varRight) || llvm::VectorType::classof(symbolTable->GetType(varRight)) ))
		{
			//get shuffle and vector variable
			llvm::ShuffleVectorInst *shuffle = llvm::dyn_cast<llvm::ShuffleVectorInst>(valueLeft);
			char *vecName = dynamic_cast<FieldAccess *>(left)->GetBase();
			llvm::Value *var = symbolTable->find(vecName)->value;

			//load vector
			llvm::LoadInst *vector = new llvm::LoadInst(var, "", irgen->GetBasicBlock());

			//calcualte mult
			binInst = llvm::BinaryOperator::CreateFAdd(valueLeft, valueRight, "");
			bb->getInstList().push_back(binInst);

			//get valueLeft's mask from shuffle
			llvm::SmallVector<int, 16> mask = shuffle->getShuffleMask();
			llvm::InsertElementInst *insert = NULL;
				

			//insert into vector
			int i = 0;
			for(llvm::SmallVector<int, 16>::iterator it = mask.begin(); it != mask.end(); it++, i++)
			{
				llvm::ExtractElementInst *ext = llvm::ExtractElementInst::Create(binInst, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

				if(i == 0)
					insert = llvm::InsertElementInst::Create(vector, ext, llvm::ConstantInt::get(irgen->GetIntType(), *it), "", irgen->GetBasicBlock());
				else
					insert = llvm::InsertElementInst::Create(insert, ext, llvm::ConstantInt::get(irgen->GetIntType(), *it), "", irgen->GetBasicBlock());
			}

			new llvm::StoreInst(insert, var, true, irgen->GetBasicBlock());
			return binInst;
				
		}
		//vector * float
		else if(llvm::VectorType::classof(symbolTable->GetType(varLeft)) && valRightFloat)
		{

			llvm::LoadInst *vector = new llvm::LoadInst(varLeft, "", irgen->GetBasicBlock());
			llvm::VectorType *vecType = llvm::dyn_cast<llvm::VectorType>(symbolTable->GetType(varLeft));
			int vecSize = vecType->getNumElements();



			//build new vector with right float value
			llvm::InsertElementInst *insert = NULL;
			int i;
			for(i = 0; i < vecSize; i++)
			{

				if(i == 0)
				{
					if(vecSize == 2)
						insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec2Type()), valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
					else if(vecSize == 3)
						insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec3Type()), valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
					else
						insert = llvm::InsertElementInst::Create(llvm::UndefValue::get(irgen->GetVec4Type()), valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());

				}
				else
					insert = llvm::InsertElementInst::Create(insert, valueRight, llvm::ConstantInt::get(irgen->GetIntType(), i), "", irgen->GetBasicBlock());
				
			}
	
			//calculate mult				
			binInst = llvm::BinaryOperator::CreateFAdd(vector, insert, "");
			bb->getInstList().push_back(binInst);			

			new llvm::StoreInst(binInst, varLeft, true, irgen->GetBasicBlock());
			return binInst;
			
		}
		//vector component
		else if(llvm::ExtractElementInst::classof(varLeft))
		{
			llvm::ExtractElementInst *ext = llvm::dyn_cast<llvm::ExtractElementInst>(varLeft);

			//calculate div
			binInst = llvm::BinaryOperator::CreateFAdd(valueLeft, valueRight, "");
			bb->getInstList().push_back(binInst);

			//load vector
			char *vecName = dynamic_cast<FieldAccess *>(left)->GetBase();
			llvm::Value *var = symbolTable->find(vecName)->value;
			llvm::LoadInst *vector = new llvm::LoadInst(var, "", irgen->GetBasicBlock());

			//isert value into vector

			llvm::InsertElementInst *insert = llvm::InsertElementInst::Create(vector, binInst, ext->getIndexOperand(), "", irgen->GetBasicBlock());

			new llvm::StoreInst(insert, var, true, irgen->GetBasicBlock());
			return binInst;
		}
		//float and vector
		else
		{
			binInst = llvm::BinaryOperator::CreateFAdd(valueLeft, valueRight, "");
			bb->getInstList().push_back(binInst);

			new llvm::StoreInst(binInst, varLeft, true, irgen->GetBasicBlock());
			return binInst;
		}
	}
	
}

llvm::Value* PostfixExpr::Emit()
{
	//get llvm value of expr
	llvm::Value *varRight = left->Emit();
	llvm::Value *valueRight = varRight;

	llvm::Type *valType = symbolTable->GetType(varRight);
	llvm::Constant *valLeft;
	llvm::BinaryOperator *binInst;



	//load value if necessary
	if(llvm::AllocaInst::classof(valueRight) || llvm::GlobalVariable::classof(valueRight) || llvm::GetElementPtrInst::classof(valueRight))
	{
		valueRight = new llvm::LoadInst(valueRight, "", irgen->GetBasicBlock());
	}


		

	//TODO checking of type not working!
	if(op->IsOp("++"))
	{
		//integer
		if(valType->isIntegerTy())
		{	
			valLeft = llvm::ConstantInt::get(irgen->GetIntType(), 1, true);
			binInst = llvm::BinaryOperator::CreateAdd(valLeft, valueRight, "");

		}
		//float
		else
		{
			valLeft = llvm::ConstantFP::get(irgen->GetFloatType(), 1.0);
			binInst = llvm::BinaryOperator::CreateFAdd(valLeft, valueRight, "");
		}

	}
	else if(op->IsOp("--"))
	{

		//integers
		if(valType->isIntegerTy())
		{	

			valLeft = llvm::ConstantInt::get(irgen->GetIntType(), 1, true);
			binInst = llvm::BinaryOperator::CreateSub(valueRight, valLeft, "");
		}
		//float
		else
		{
			valLeft = llvm::ConstantFP::get(irgen->GetFloatType(), 1.0);
			binInst = llvm::BinaryOperator::CreateFSub(valueRight, valLeft, "");
		}
	}
	
	irgen->GetBasicBlock()->getInstList().push_back(binInst);
	new llvm::StoreInst(binInst, varRight, true, irgen->GetBasicBlock());
	return valueRight;
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


	//ternary operator
	llvm::Value *condValue = cond->Emit();
	llvm::Value *s1Value = trueExpr->Emit();
	llvm::Value *s2Value = falseExpr->Emit();

	//Perform load if necessary
	if(llvm::AllocaInst::classof(condValue) || llvm::GlobalVariable::classof(condValue) || llvm::GetElementPtrInst::classof(condValue))
		condValue = new llvm::LoadInst(condValue, "", irgen->GetBasicBlock());
		
	if(llvm::AllocaInst::classof(s1Value) || llvm::GlobalVariable::classof(s1Value) || llvm::GetElementPtrInst::classof(s1Value))
		s1Value = new llvm::LoadInst(s1Value, "", irgen->GetBasicBlock());

	if(llvm::AllocaInst::classof(s2Value) || llvm::GlobalVariable::classof(s2Value) || llvm::GetElementPtrInst::classof(s2Value))
		s2Value = new llvm::LoadInst(s2Value, "", irgen->GetBasicBlock());

	
	llvm::SelectInst  *ternInst = llvm::SelectInst::Create(condValue, s1Value, s2Value, "", irgen->GetBasicBlock());

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
	if(llvm::AllocaInst::classof(index) || llvm::GlobalVariable::classof(index) || llvm::GetElementPtrInst::classof(index))
		index = new llvm::LoadInst(index, "", irgen->GetBasicBlock());

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

	//load variable
	llvm::Value *vector = base->Emit();

	if(llvm::AllocaInst::classof(vector) || llvm::GlobalVariable::classof(vector))
		vector = new llvm::LoadInst(vector, "", irgen->GetBasicBlock());

	//get field length
	char *fieldStr = field->GetName();
	int fieldLen = 0;
	for(int i = 0; fieldStr[i] != '\0'; i++)
		fieldLen++;

	if(fieldLen == 1)
	{
		//find index
		llvm::Constant *index;
		if(fieldStr[0] == 'x')
			index = llvm::ConstantInt::get(irgen->GetIntType(), 0);
		else if(fieldStr[0] == 'y')
			index = llvm::ConstantInt::get(irgen->GetIntType(), 1);
		else if(fieldStr[0] == 'z')
			index = llvm::ConstantInt::get(irgen->GetIntType(), 2);
		else
			index = llvm::ConstantInt::get(irgen->GetIntType(), 3);

		//extract element
		llvm::ExtractElementInst *ext = llvm::ExtractElementInst::Create(vector, index, "", irgen->GetBasicBlock());
		return ext;
	}
	else
	{
		//build mask
		std::vector<llvm::Constant*> idxVec;
		for(int i = 0; i < fieldLen; i++)
		{
			if(fieldStr[i] == 'x')
				idxVec.push_back(llvm::ConstantInt::get(irgen->GetIntType(), 0));
			else if(fieldStr[i] == 'y')
				idxVec.push_back(llvm::ConstantInt::get(irgen->GetIntType(), 1));
			else if(fieldStr[i] == 'z')
				idxVec.push_back(llvm::ConstantInt::get(irgen->GetIntType(), 2));
			else
				idxVec.push_back(llvm::ConstantInt::get(irgen->GetIntType(), 3));

		}


		llvm::ArrayRef<llvm::Constant*> idxArray(idxVec);
		llvm::Constant *mask = llvm::ConstantVector::get(idxArray);

		return new llvm::ShuffleVectorInst(vector, vector, mask, "", irgen->GetBasicBlock());
	}

	
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
		if(llvm::AllocaInst::classof(value) || llvm::GlobalVariable::classof(value) || llvm::GetElementPtrInst::classof(value))
			value = new llvm::LoadInst(value, "", irgen->GetBasicBlock());

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

