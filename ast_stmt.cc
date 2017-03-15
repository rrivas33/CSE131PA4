/* File: ast_stmt.cc
 * -----------------
 * Implementation of statement node classes.
 */
#include "ast_stmt.h"
#include "ast_type.h"
#include "ast_decl.h"
#include "ast_expr.h"
#include "symtable.h"

#include "irgen.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Support/raw_ostream.h"



Program::Program(List<Decl*> *d) {
    Assert(d != NULL);
    (decls=d)->SetParentAll(this);
}

void Program::PrintChildren(int indentLevel) {
    decls->PrintAll(indentLevel+1);
    printf("\n");
}

llvm::Value* Program::Emit() {
    // TODO:
    // This is just a reference for you to get started
    //
    // You can use this as a template and create Emit() function
    // for individual node to fill in the module structure and instructions.
    //
    //IRGenerator irgen;
    llvm::Module *mod = irgen->GetOrCreateModule("Name_the_Module.bc");

	//Generate code for all declarations
	for(int i = 0; i < decls->NumElements(); i++)
	{
		Decl *decl = decls->Nth(i);

		decl->Emit();
	}

/*------------- EXAMPLES----------------------

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
    // write the BC into standard output
    llvm::WriteBitcodeToFile(mod, llvm::outs());

    //uncomment the next line to generate the human readable/assembly file
    //mod->dump();

	return llvm::UndefValue::get(irgen->GetVoidType());
}

StmtBlock::StmtBlock(List<VarDecl*> *d, List<Stmt*> *s) {
    Assert(d != NULL && s != NULL);
    (decls=d)->SetParentAll(this);
    (stmts=s)->SetParentAll(this);
}

void StmtBlock::PrintChildren(int indentLevel) {
    decls->PrintAll(indentLevel+1);
    stmts->PrintAll(indentLevel+1);
}

llvm::Value* StmtBlock::Emit()
{
	
	symbolTable->push();

	//var decls
	for(int i = 0; i < decls->NumElements(); i++)
	{
		VarDecl *decl = decls->Nth(i);
		decl->Emit();
	}

	//statements
	for(int i = 0; i < stmts->NumElements(); i++)
	{
		Stmt *stmt = stmts->Nth(i);
		std::cerr << stmt->GetPrintNameForNode() << std::endl;
		stmt->Emit();
	}

	symbolTable->pop();

	return llvm::UndefValue::get(irgen->GetVoidType());
}

DeclStmt::DeclStmt(Decl *d) {
    Assert(d != NULL);
    (decl=d)->SetParent(this);
}

void DeclStmt::PrintChildren(int indentLevel) {
    decl->Print(indentLevel+1);
}

llvm::Value* DeclStmt::Emit()
{
	return decl->Emit();
}

ConditionalStmt::ConditionalStmt(Expr *t, Stmt *b) { 
    Assert(t != NULL && b != NULL);
    (test=t)->SetParent(this); 
    (body=b)->SetParent(this);
}


ForStmt::ForStmt(Expr *i, Expr *t, Expr *s, Stmt *b): LoopStmt(t, b) { 
    Assert(i != NULL && t != NULL && b != NULL);
    (init=i)->SetParent(this);
    step = s;
    if ( s )
      (step=s)->SetParent(this);
}

void ForStmt::PrintChildren(int indentLevel) {
    init->Print(indentLevel+1, "(init) ");
    test->Print(indentLevel+1, "(test) ");
    if ( step )
      step->Print(indentLevel+1, "(step) ");
    body->Print(indentLevel+1, "(body) ");
}

llvm::Value* ForStmt::Emit()
{

	//footer
	llvm::BasicBlock *footerBB = llvm::BasicBlock::Create(*(irgen->GetContext()), "footerBB", irgen->GetFunction(), !bbStack.empty() ? bbStack.back() : NULL);

	//step
	llvm::BasicBlock *stepBB = llvm::BasicBlock::Create(*(irgen->GetContext()), "stepBB", irgen->GetFunction(), footerBB);
	bbStack.push_back(stepBB);

	//body
	llvm::BasicBlock *bodyBB = llvm::BasicBlock::Create(*(irgen->GetContext()), "bodyBB", irgen->GetFunction(), stepBB);
		
	//header
	llvm::BasicBlock *headerBB = llvm::BasicBlock::Create(*(irgen->GetContext()), "headerBB", irgen->GetFunction(), bodyBB);


	//branch to loop
	init->Emit();
	llvm::BranchInst::Create(headerBB,irgen->GetBasicBlock());

	//populate headerBB
	irgen->SetBasicBlock(headerBB);
	llvm::Value *cond = test->Emit();
	llvm::BranchInst::Create(bodyBB, footerBB, cond, irgen->GetBasicBlock());


	//populate bodyBB
	irgen->SetBasicBlock(bodyBB);
	this->body->Emit();

	//no return stmt in body
	if(!retStmtIncluded)
		llvm::BranchInst::Create(stepBB, irgen->GetBasicBlock());
	else
		retStmtIncluded = false;


	//populate stepBB
	irgen->SetBasicBlock(bbStack.back());
	bbStack.pop_back();
	step->Emit();
	llvm::BranchInst::Create(headerBB, irgen->GetBasicBlock());
	
	//end of loop
	irgen->SetBasicBlock(footerBB);


	return llvm::UndefValue::get(irgen->GetVoidType());
}

void WhileStmt::PrintChildren(int indentLevel) {
    test->Print(indentLevel+1, "(test) ");
    body->Print(indentLevel+1, "(body) ");
}

IfStmt::IfStmt(Expr *t, Stmt *tb, Stmt *eb): ConditionalStmt(t, tb) { 
    Assert(t != NULL && tb != NULL); // else can be NULL
    elseBody = eb;
    if (elseBody) elseBody->SetParent(this);
}

void IfStmt::PrintChildren(int indentLevel) {
    if (test) test->Print(indentLevel+1, "(test) ");
    if (body) body->Print(indentLevel+1, "(then) ");
    if (elseBody) elseBody->Print(indentLevel+1, "(else) ");
}

llvm::Value* IfStmt::Emit()
{
	llvm::Value *testCond = test->Emit();
	llvm::BasicBlock *initBB = irgen->GetBasicBlock();;

	//TODO need to insert footer bb after initBB. Use the stack of bb?
	//push footer bb
	llvm::BasicBlock *footerBB = llvm::BasicBlock::Create(*(irgen->GetContext()), "footerBB", irgen->GetFunction(), !bbStack.empty() ? bbStack.back() : NULL);
	bbStack.push_back(footerBB);

	//push else body bb
	llvm::BasicBlock *elseBB = NULL;
	if(elseBody != NULL)
	{	
		elseBB = llvm::BasicBlock::Create(*(irgen->GetContext()), "ElseBB", irgen->GetFunction(), footerBB);
		bbStack.push_back(elseBB);
	
		/*
		irgen->SetBasicBlock(elseBB);
		elseBody->Emit();
		
		
		if(!retStmtIncluded)
			llvm::BranchInst::Create(footerBB, elseBB);
		retStmtIncluded = false;
		*/
	}

	//then bb
	llvm::BasicBlock *thenBB = llvm::BasicBlock::Create(*(irgen->GetContext()), "thenBB", irgen->GetFunction(), elseBB ? elseBB : footerBB);
	
	/*	
	irgen->SetBasicBlock(thenBB);
	body->Emit();
	
	//branch only when no return stmt in body
	if(!retStmtIncluded)
		llvm::BranchInst::Create(footerBB, thenBB);
	retStmtIncluded = false;
	*/

	//branch if
	if(elseBody != NULL)
		llvm::BranchInst::Create(thenBB, elseBB, testCond, initBB);
	else
		llvm::BranchInst::Create(thenBB, footerBB, testCond, initBB);

	//populate thenBB
	irgen->SetBasicBlock(thenBB);
	body->Emit();


	//branch only when no return stmt in body
	if(!retStmtIncluded)
		llvm::BranchInst::Create(footerBB, thenBB);
	retStmtIncluded = false;


	//populate else bb
	if(elseBody != NULL)
	{
		irgen->SetBasicBlock(bbStack.back());
		bbStack.pop_back();
		elseBody->Emit();
		
		if(!retStmtIncluded)
			llvm::BranchInst::Create(footerBB, elseBB);

		retStmtIncluded = false;
		
	}
	

	//set footerBB
	irgen->SetBasicBlock(bbStack.back());
	bbStack.pop_back();

	return llvm::UndefValue::get(irgen->GetVoidType());

}

llvm::Value* BreakStmt::Emit()
{
	//TODO
	return llvm::UndefValue::get(irgen->GetVoidType());
}

ReturnStmt::ReturnStmt(yyltype loc, Expr *e) : Stmt(loc) { 
    expr = e;
    if (e != NULL) expr->SetParent(this);
}

void ReturnStmt::PrintChildren(int indentLevel) {
    if ( expr ) 
      expr->Print(indentLevel+1);
}

llvm::Value* ReturnStmt::Emit()
{
	retStmtIncluded = true;

	if(expr != NULL)
	{
		llvm::Value *value = expr->Emit();

		//load variable
		if(llvm::UnaryInstruction::classof(value) || llvm::GlobalVariable::classof(value) || llvm::GetElementPtrInst::classof(value))
			value = new llvm::LoadInst(value, "ld1", irgen->GetBasicBlock());

		llvm::ReturnInst *ret = llvm::ReturnInst::Create(*(irgen->GetContext()), value, irgen->GetBasicBlock());
		return ret;
	}
	else
		return llvm::ReturnInst::Create(*(irgen->GetContext()), irgen->GetBasicBlock());
}

SwitchLabel::SwitchLabel(Expr *l, Stmt *s) {
    Assert(l != NULL && s != NULL);
    (label=l)->SetParent(this);
    (stmt=s)->SetParent(this);
}

SwitchLabel::SwitchLabel(Stmt *s) {
    Assert(s != NULL);
    label = NULL;
    (stmt=s)->SetParent(this);
}

void SwitchLabel::PrintChildren(int indentLevel) {
    if (label) label->Print(indentLevel+1);
    if (stmt)  stmt->Print(indentLevel+1);
}

SwitchStmt::SwitchStmt(Expr *e, List<Stmt *> *c, Default *d) {
    Assert(e != NULL && c != NULL && c->NumElements() != 0 );
    (expr=e)->SetParent(this);
    (cases=c)->SetParentAll(this);
    def = d;
    if (def) def->SetParent(this);
}

void SwitchStmt::PrintChildren(int indentLevel) {
    if (expr) expr->Print(indentLevel+1);
    if (cases) cases->PrintAll(indentLevel+1);
    if (def) def->Print(indentLevel+1);
}

