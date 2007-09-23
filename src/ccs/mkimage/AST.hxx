#ifndef AST_HXX
#define AST_HXX




#include <string>
#include <libsherpa/LToken.hxx>
#include <libsherpa/CVector.hxx>
#include <libsherpa/GCPtr.hxx>


#include <stdint.h>
#include <libsherpa/BigNum.hxx>
#include <libcoyimage/CoyImage.hxx>
#include "Value.hxx"
#include "FQName.hxx"
#include "Environment.hxx"
#include "INOstream.hxx"
#include "Interp.hxx"

 
struct UocInfo;

enum AstType {
    at_Null,
    at_AnyGroup,
    at_ident,
    at_string,
    at_ifident,
    at_usesel,
    at_boolLiteral,
    at_intLiteral,
    at_charLiteral,
    at_floatLiteral,
    at_stringLiteral,
    at_uoc,
    at_block,
    at_s_print,
    at_s_printstar,
    at_s_assign,
    at_s_return,
    at_s_def,
    at_s_export_def,
    at_s_fndef,
    at_s_export_fndef,
    at_idlist,
    at_s_export,
    at_s_import,
    at_s_enum,
    at_s_export_enum,
    at_s_capreg,
    at_s_export_capreg,
    at_enum_bind,
    at_s_bank,
    at_s_while,
    at_s_do,
    at_fncall,
    at_ifelse,
    at_dot,
    at_vecref,
    at_docString,
    agt_var,
    agt_literal,
    agt_stmt,
    agt__AnonGroup0,
    agt__AnonGroup1,
    agt__AnonGroup2,
    agt__AnonGroup3,
    agt_expr,
    agt__AnonGroup4,
};

enum { at_NUM_ASTTYPE = agt__AnonGroup4};
class AST : public Countable { 
  bool isOneOf(AstType);
public:
  AstType        astType;
  std::string    s;
  sherpa::LexLoc loc;
  sherpa::GCPtr< sherpa::CVector<sherpa::GCPtr<AST> > > children;


 private:  
  static unsigned long long astCount;
  
 public:
  unsigned long long ID; 

  GCPtr<Value> litValue;
  
  unsigned printVariant;	

  GCPtr<UocInfo> uocInfo;  

  
  
  
  
  
  
  
  
  FQName fqn;                 

  static GCPtr<AST> makeBoolLit(const sherpa::LToken &tok);
  static GCPtr<AST> makeCharLit(const sherpa::LToken &tok);
  static GCPtr<AST> makeIntLit(const sherpa::LToken &tok);
  static GCPtr<AST> makeFloatLit(const sherpa::LToken &tok);
  static GCPtr<AST> makeStringLit(const sherpa::LToken &tok);
  
  
  
  
  
  static GCPtr<AST> genSym(const char *pfx = "tmp");

  std::string asString() const;

  bool isPureAST();

  void PrettyPrint(INOstream& out) const;
  void PrettyPrint(std::ostream& out) const;

  
  void PrettyPrint() const;

  sherpa::GCPtr<Value> interp(InterpState& is);

  AST(const AstType at = at_Null);
  // for literals:
  AST(const AstType at, const sherpa::LToken& tok);
  AST(const AstType at, const sherpa::LexLoc &loc);
  AST(const AstType at, const sherpa::LexLoc &loc,
      sherpa::GCPtr<AST> child1);
  AST(const AstType at, const sherpa::LexLoc &loc,
      sherpa::GCPtr<AST> child1,
      sherpa::GCPtr<AST> child2);
  AST(const AstType at, const sherpa::LexLoc &loc,
      sherpa::GCPtr<AST> child1,
      sherpa::GCPtr<AST> child2,
      sherpa::GCPtr<AST> child3);
  AST(const AstType at, const sherpa::LexLoc &loc,
      sherpa::GCPtr<AST> child1,
      sherpa::GCPtr<AST> child2,
      sherpa::GCPtr<AST> child3,
      sherpa::GCPtr<AST> child4);
  AST(const AstType at, const sherpa::LexLoc &loc,
      sherpa::GCPtr<AST> child1,
      sherpa::GCPtr<AST> child2,
      sherpa::GCPtr<AST> child3,
      sherpa::GCPtr<AST> child4,
      sherpa::GCPtr<AST> child5);
  ~AST();

  sherpa::GCPtr<AST> & child(size_t i) const
  {
    return children->elem(i);
  }

  void addChild(sherpa::GCPtr<AST> child);
  std::string getTokenString();

  void
  addChildrenFrom(sherpa::GCPtr<AST> other)
  {
    for(size_t i = 0; i < other->children->size(); i++)
      addChild(other->child(i));
  }

  static const char *tagName(const AstType at);
  const char *astTypeName() const;
  const char *astName() const;

  bool isMemberOfType(AstType) const;
  bool isValid() const;
};


#endif /* AST_HXX */
