#ifndef EXPRFACTORY_H
#define EXPRFACTORY_H
#include "expr.h"
#include <map>
#include <set>
#include "shared/DocStructure.h"


// Note: do not declare ExprFactory as global.
// It may get messed-up values due to wrong inisitalization order between this class and Values names.
class ExprFactory
{
private:
   // Private constructor. Construct with getFactory.
   ExprFactory();

public:

   void processDocStructure(DocStructure& doc_conf);

   Expr* create(string id, ExprKind kind, const ExprVec& args);

   bool isKind(string id, ExprKind kind);

   int precedence(string id);

   static ExprFactory* getFactory();

   virtual ~ExprFactory();

private:
    void _init();
    void add(Expr* e);
    void add(Expr* e, int precedence);

    std::map<ExprKind, std::map<string, ExprP> > _symtab;
    std::map<string, std::set<ExprKind> > _kindtab;
    std::map<string, int> _precedence;
};

#endif // EXPRFACTORY_H
