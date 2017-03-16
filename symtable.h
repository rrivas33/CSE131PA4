/**
 * File: symtable.h
 * ----------- 
 *  This file defines a class for symbol table and scoped table table.
 *
 *  Scoped table is to hold all declarations in a nested scope. It simply
 *  uses the standard C++ map.
 *
 *  Symbol table is implemented as a vector, where each vector entry holds
 *  a pointer to the scoped table.
 */

#ifndef _H_symtable
#define _H_symtable

#include <map>
#include <vector>
#include <iostream>
#include <string.h>
#include "errors.h"

namespace llvm {
	class Value;
}

using namespace std;

class Decl;
class Stmt;

enum EntryKind {
  E_FunctionDecl,
  E_VarDecl,
};

struct Symbol {
  char *name;
  Decl *decl;
  EntryKind kind;
  int someInfo;
  llvm::Value *value;
  llvm::Type *llvmType;

  Symbol() : name(NULL), decl(NULL), kind(E_VarDecl), value(NULL), llvmType(NULL) {}
  Symbol(char *n, Decl *d, EntryKind k, llvm::Value *v = NULL, llvm::Type *t = NULL) :
        name(n),
        decl(d),
        kind(k),
        value(v),
		llvmType(t) {}
};

struct lessStr {
  bool operator()(const char* s1, const char* s2) const
  { return strcmp(s1, s2) < 0; }
};
 
typedef map<const char *, Symbol, lessStr>::iterator SymbolIterator;

class ScopedTable {
  map<const char *, Symbol, lessStr> symbols;

  public:
    ScopedTable();
    ~ScopedTable();

    void insert(Symbol &sym);
    void remove(Symbol &sym);
    Symbol* find(const char *name);
};
   
class SymbolTable {
  std::vector<ScopedTable *> tables;
  ScopedTable *currentScopedTable;
  FnDecl *currentFuncDecl;

  public:
    SymbolTable();
    ~SymbolTable();

	//Push or Pop ScopedTable from vector
    void push();
    void pop();
	
	//Insert or remove symbol from current ScopedTable
    void insert(Symbol &sym);
    void remove(Symbol &sym);

	//Search for symbol in all tables
    Symbol *find(const char *name);
	
	//Seach for symbol in current scoped table
	Symbol *findInCurrentTable(const char *name);

	Type* getCurrentFuncType();

	bool isGlobalScope() const { return (tables.size() == 1); }

};    

class MyStack {
    vector<Stmt *> stmtStack;
	unsigned int loops;
	unsigned int switches;

  public:
	MyStack(){loops = switches = 0;}

    void push(Stmt *s);
    void pop();
    bool insideLoop()  {return loops > 0;}
    bool insideSwitch(){return switches > 0;}
};

#endif
