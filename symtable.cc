/*
 * Symbol table implementation
 *
 */

#include "symtable.h"

ScopedTable::ScopedTable(){}

void ScopedTable::insert(Symbol &sym)
{
	symbols.insert( std::pair<const char *, Symbol>(sym.name, sym) );
}

void ScopedTable::remove(Symbol &sym)
{
	//TODO Implements removing from map
}

Symbol* ScopedTable::find(const char *name)
{
	SymbolIterator it = symbols.find(name);

	if( it != symbols.end() )
		return &(it->second);
	
	return NULL;
}



SymbolTable::SymbolTable(IRGenerator *ir)
{
	//Create global scope table and added to symbol table
	ScopedTable *globalScope = new ScopedTable();
	tables.push_back(globalScope);

	//Set current scope
	currentScopedTable = globalScope;

	currentFuncDecl = NULL;

	irGen = ir;
}

//Push or Pop ScopedTable from vector
void SymbolTable::push()
{
	ScopedTable *table = new ScopedTable();
	tables.push_back(table);
	currentScopedTable = table;

}
void SymbolTable::pop()
{
	tables.pop_back();


	if(tables.size() > 0)
		currentScopedTable = tables.back();
}
	
//Insert or remove symbol from current ScopedTable
void SymbolTable::insert(Symbol &sym)
{
	currentScopedTable->insert(sym);

	//update currentFundDecl if one inserted
	if(sym.kind == E_FunctionDecl)
		currentFuncDecl = dynamic_cast<FnDecl *>(sym.decl);
}
void SymbolTable::remove(Symbol &sym)
{
	currentScopedTable->remove(sym);
}

//Search for symbol in all tables
Symbol* SymbolTable::find(const char *name)
{
	//Traverse all ScopedTables in reverse order and look for 'name'
	std::vector<ScopedTable *>::reverse_iterator it;
	for(it = tables.rbegin(); it != tables.rend(); ++it)
	{
		//look for 'name' in current table
		Symbol *symbol = (*it)->find(name);
		if(symbol != NULL)
			return symbol;
		
	}
	std::cerr << "could not find symbol = " << name << endl;
	return NULL;
}

Symbol* SymbolTable::findInCurrentTable(const char *name)
{
	return currentScopedTable->find(name);
}

Type* SymbolTable::getCurrentFuncType()
{
	return currentFuncDecl->GetType();
}

void MyStack::push(Stmt *s) 
{ 
	stmtStack.push_back(s);
	
	//check if loop or switch
	WhileStmt *whileStmt = dynamic_cast<WhileStmt *>(s);
	ForStmt *forStmt = dynamic_cast<ForStmt *>(s);
	SwitchStmt *switchStmt = dynamic_cast<SwitchStmt *>(s);

	if(whileStmt != NULL || forStmt != NULL)
		loops++;
	else if(switchStmt != NULL)
		switches++;
}

llvm::Type* SymbolTable::GetType(llvm::Value *value)
{
	if(llvm::dyn_cast<llvm::AllocaInst>(value) || llvm::dyn_cast<llvm::GlobalVariable>(value))
	{
		//std::cerr << "symbol allocation\n";
		return find(value->getName().str().c_str())->llvmType;
	}
	else if(llvm::dyn_cast<llvm::ConstantInt>(value))
	{
		//std::cerr << "symbol ConstantInt\n";
		return irGen->GetIntType();
	}
	else if(llvm::dyn_cast<llvm::ConstantFP>(value))
	{
		//std::cerr << "symbol ConstantFP\n";
		return irGen->GetFloatType();
	}
	else if(llvm::dyn_cast<llvm::GetElementPtrInst>(value))
	{
		//std::cerr << "GetElemrntPtr in symbomTable\n";
		llvm::GetElementPtrInst *elmt = llvm::dyn_cast<llvm::GetElementPtrInst>(value);
		return elmt->getType()->getElementType();
	}
	else if(llvm::dyn_cast<llvm::ExtractElementInst>(value))
	{
		//cerr << "ExtractElement in table\n";
		return irGen->GetFloatType();
	}
	else if(llvm::dyn_cast<llvm::ShuffleVectorInst>(value))
	{
		llvm::ShuffleVectorInst *shuffle = llvm::dyn_cast<llvm::ShuffleVectorInst>(value);
		return shuffle->getType();
	}
	else
	{
		
		//cerr << "returned type null\n";
		return value->getType();
		//return NULL;
	}

}

void MyStack::pop()
{
	if(stmtStack.size() > 0)
	{
		Stmt *s = stmtStack.back();

		//check if loop or switch
		WhileStmt *whileStmt = dynamic_cast<WhileStmt *>(s);
		ForStmt *forStmt = dynamic_cast<ForStmt *>(s);
		SwitchStmt *switchStmt = dynamic_cast<SwitchStmt *>(s);

		if(whileStmt != NULL || forStmt != NULL)
			loops--;
		else if(switchStmt != NULL)
			switches--;

		stmtStack.pop_back();
		
	}
}
