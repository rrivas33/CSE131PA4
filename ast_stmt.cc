/* File: ast_stmt.cc
 * -----------------
 * Implementation of statement node classes.
 */
#include "ast_stmt.h"
#include "ast_type.h"
#include "ast_decl.h"
#include "ast_expr.h"
#include "symtable.h"
#include <string>

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
		//std::cerr << stmt->GetPrintNameForNode() << std::endl;

		//push new scope if block stmt is found
		StmtBlock *stmtBlock = dynamic_cast<StmtBlock *>(stmt);
		if(stmtBlock != NULL)
		{
			symbolTable->push();
	
			stmt->Emit();
	
			symbolTable->pop();
		}
		else	
			stmt->Emit();
	}


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

	inLoop = true;

	//footer
	llvm::BasicBlock *footerBB = llvm::BasicBlock::Create(*(irgen->GetContext()), "footerBB", irgen->GetFunction(), !bbStack.empty() ? bbStack.back() : NULL);
	bbStack.push_back(footerBB);
	bbLoopExitStack.push_back(footerBB);

	//step
	llvm::BasicBlock *stepBB = llvm::BasicBlock::Create(*(irgen->GetContext()), "stepBB", irgen->GetFunction(), footerBB);
	bbStack.push_back(stepBB);
	bbContinueStack.push_back(stepBB);

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
	if(llvm::AllocaInst::classof(cond) || llvm::GlobalVariable::classof(cond) || llvm::GetElementPtrInst::classof(cond))
		cond = new llvm::LoadInst(cond, "", irgen->GetBasicBlock());
	llvm::BranchInst::Create(bodyBB, footerBB, cond, irgen->GetBasicBlock());


	//populate bodyBB
	irgen->SetBasicBlock(bodyBB);
	this->body->Emit();

	//no return stmt in body
	if(irgen->GetBasicBlock()->getTerminator() == NULL)
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

	bbStack.pop_back();
	bbContinueStack.pop_back();
	bbLoopExitStack.pop_back();
	inLoop = false;
	return llvm::UndefValue::get(irgen->GetVoidType());
}

void WhileStmt::PrintChildren(int indentLevel) {
    test->Print(indentLevel+1, "(test) ");
    body->Print(indentLevel+1, "(body) ");
}

llvm::Value* WhileStmt::Emit()
{
	inLoop = true;

	//footer bb
	llvm::BasicBlock *footerBB = llvm::BasicBlock::Create(*(irgen->GetContext()), "footerBB", irgen->GetFunction(), !bbStack.empty() ? bbStack.back() : NULL);
	bbStack.push_back(footerBB);
	bbLoopExitStack.push_back(footerBB);

	//body bb
	llvm::BasicBlock *bodyBB = llvm::BasicBlock::Create(*(irgen->GetContext()), "bodyBB", irgen->GetFunction(), footerBB);

	//test bb
	llvm::BasicBlock *testBB = llvm::BasicBlock::Create(*(irgen->GetContext()), "headerBB", irgen->GetFunction(), bodyBB);
	bbContinueStack.push_back(testBB);

	//branch to while testBB
	llvm::BranchInst::Create(testBB, irgen->GetBasicBlock());

	//Emit test
	irgen->SetBasicBlock(testBB);
	llvm::Value *cond = test->Emit();
	if(llvm::AllocaInst::classof(cond) || llvm::GlobalVariable::classof(cond) || llvm::GetElementPtrInst::classof(cond))
		cond = new llvm::LoadInst(cond, "", irgen->GetBasicBlock());
	llvm::BranchInst::Create(bodyBB, footerBB, cond, irgen->GetBasicBlock());

	//Emit body
	irgen->SetBasicBlock(bodyBB);
	body->Emit();

	//no return stmt in body
	if(irgen->GetBasicBlock()->getTerminator() == NULL)
		llvm::BranchInst::Create(testBB, irgen->GetBasicBlock());
	else
		retStmtIncluded = false;

	//set footer bb
	irgen->SetBasicBlock(bbStack.back());
	bbStack.pop_back();
	

	inLoop = false;
	bbLoopExitStack.pop_back();
	bbContinueStack.pop_back();

	return llvm::UndefValue::get(irgen->GetVoidType());
	
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
	if(llvm::AllocaInst::classof(testCond) || llvm::GlobalVariable::classof(testCond) || llvm::GetElementPtrInst::classof(testCond))
		testCond = new llvm::LoadInst(testCond, "", irgen->GetBasicBlock());

	llvm::BasicBlock *footerBB = llvm::BasicBlock::Create(*(irgen->GetContext()), "footerBB", irgen->GetFunction());
	bbStack.push_back(footerBB);

	//push else body bb
	llvm::BasicBlock *elseBB = NULL;
	if(elseBody != NULL)
	{	
		//elseBB = llvm::BasicBlock::Create(*(irgen->GetContext()), "ElseBB", irgen->GetFunction(), footerBB);
		elseBB = llvm::BasicBlock::Create(*(irgen->GetContext()), "ElseBB", irgen->GetFunction());
	}


	//llvm::BasicBlock *thenBB = llvm::BasicBlock::Create(*(irgen->GetContext()), "ThenBB", irgen->GetFunction(), elseBB ? elseBB : footerBB);
	llvm::BasicBlock *thenBB = llvm::BasicBlock::Create(*(irgen->GetContext()), "ThenBB", irgen->GetFunction());

	//branch if
	if(elseBody != NULL)
		llvm::BranchInst::Create(thenBB, elseBB, testCond, irgen->GetBasicBlock());
	else
		llvm::BranchInst::Create(thenBB, footerBB, testCond, irgen->GetBasicBlock());

	//populate thenBB

	irgen->SetBasicBlock(thenBB);
	body->Emit();
	if(elseBB != NULL)
		elseBB->moveAfter(thenBB);


	//branch only when no return stmt in body
	if(irgen->GetBasicBlock()->getTerminator() == NULL)
	{
		//cerr << "NO TERMINATOR\n";
		llvm::BranchInst::Create(footerBB, irgen->GetBasicBlock());
	}
	retStmtIncluded = false;



	//populate else bb
	if(elseBody != NULL)
	{
		irgen->SetBasicBlock(elseBB);
		//bbStack.pop_back();
		elseBody->Emit();
	}

	footerBB->moveAfter(elseBB ? elseBB : thenBB);
	
	if(elseBody != NULL && elseBB->getTerminator() == NULL)
		llvm::BranchInst::Create(footerBB, elseBB);

	//set footerBB
	irgen->SetBasicBlock(footerBB);
	if(!bbStack.empty())
		bbStack.pop_back();

	return llvm::UndefValue::get(irgen->GetVoidType());

}

llvm::Value* BreakStmt::Emit()
{
	retStmtIncluded =  true;

	return llvm::BranchInst::Create(inLoop ? bbLoopExitStack.back() : bbStack.back(), irgen->GetBasicBlock());
}

llvm::Value* ContinueStmt::Emit()
{
	//cerr << "continue called\n";
	
	return llvm::BranchInst::Create(bbContinueStack.back(), irgen->GetBasicBlock());
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
		if(llvm::AllocaInst::classof(value) || llvm::GlobalVariable::classof(value) || llvm::GetElementPtrInst::classof(value))
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

llvm::Value* Case::Emit()
{
	stmt->Emit();

	return label->Emit();
}

llvm::Value* Default::Emit()
{
	return stmt->Emit();
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

llvm::Value* SwitchStmt::Emit()
{
	llvm::BasicBlock *initBB = irgen->GetBasicBlock();
	std::vector<llvm::BasicBlock*> bbs;

	//switch exit bb
	llvm::BasicBlock *exitBB = llvm::BasicBlock::Create(*(irgen->GetContext()), "switchExit", irgen->GetFunction(), !bbStack.empty() ? bbStack.back() : NULL);
	bbStack.push_back(exitBB);
	

	//default case
	llvm::BasicBlock *defaultBB = NULL;
	
	//create bb for each case
	for(int i = 0; i < cases->NumElements(); i++)
	{
		if(dynamic_cast<Default*>(cases->Nth(i)))
		{
			defaultBB = llvm::BasicBlock::Create(*(irgen->GetContext()), "switchDef", irgen->GetFunction(), exitBB);
			bbs.push_back(defaultBB);		
		}
		else
		{
			llvm::BasicBlock *bb = llvm::BasicBlock::Create(*(irgen->GetContext()), "switchCase", irgen->GetFunction(), exitBB);
			bbs.push_back(bb);
		}
	}

	//exitBB at bottom of case statck in absence of default case
	if(defaultBB == NULL)
		bbs.push_back(exitBB);

	//emit expr
	llvm::Value *switchValue = expr->Emit();
	
	//load swithValue if necessary
	if(llvm::UnaryInstruction::classof(switchValue) || llvm::GlobalVariable::classof(switchValue) || llvm::GetElementPtrInst::classof(switchValue))
			switchValue = new llvm::LoadInst(switchValue, "ld1", irgen->GetBasicBlock());

	//create swtich inst
	llvm::SwitchInst *switchInst = llvm::SwitchInst::Create(switchValue, defaultBB ? defaultBB : exitBB, bbs.size(), irgen->GetBasicBlock());

	//emit case stmts
	int i;
	for(i = 0; i < bbs.size()-1; i++)
	{
		irgen->SetBasicBlock(bbs[i]);
		llvm::ConstantInt *label = llvm::cast<llvm::ConstantInt>(cases->Nth(i)->Emit());

		//no terminator in bb
		if(!irgen->GetBasicBlock()->getTerminator())
		{
			llvm::BranchInst::Create(bbs[i+1], irgen->GetBasicBlock());
		}

		//add case to switch
		switchInst->addCase(label, bbs[i]); 
	}

	//emit default case
	if(defaultBB != NULL)
	{
		irgen->SetBasicBlock(defaultBB);
		cases->Nth(i)->Emit();

		if(!irgen->GetBasicBlock()->getTerminator())
			llvm::BranchInst::Create(exitBB, irgen->GetBasicBlock());
	}


	irgen->SetBasicBlock(bbStack.back());
	bbStack.pop_back();
	
	
		

	return llvm::UndefValue::get(irgen->GetVoidType());
}

