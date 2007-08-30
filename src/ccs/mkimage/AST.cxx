


#include <stdlib.h>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <string>
#include "AST.hxx"

using namespace sherpa;


#include "UocInfo.hxx"

using namespace sherpa ;
unsigned long long AST::astCount = 0;

AST::~AST()
{
}

AST::AST(const AstType at)
{
  astType = at;
  children = new CVector<sherpa::GCPtr<AST> >;

  ID = ++(AST::astCount);
  printVariant = 0;		
}

AST::AST(const AstType at, const LToken& tok)
{
  astType = at;
  children = new CVector<sherpa::GCPtr<AST> >;
  loc = tok.loc;
  s = tok.str;

  ID = ++(AST::astCount);
  printVariant = 0;		
}

AST::AST(const AstType at, const LexLoc& _loc)
{
  astType = at;
  children = new CVector<sherpa::GCPtr<AST> >;
  loc = _loc;

  ID = ++(AST::astCount);
  printVariant = 0;		
}

AST::AST(const AstType at, const LexLoc& _loc,
         sherpa::GCPtr<AST> child1)
{
  astType = at;
  children = new CVector<sherpa::GCPtr<AST> >;
  loc = _loc;
  addChild(child1);

  ID = ++(AST::astCount);
  printVariant = 0;		
}

AST::AST(const AstType at, const LexLoc& _loc,
         sherpa::GCPtr<AST> child1,
         sherpa::GCPtr<AST> child2)
{
  astType = at;
  children = new CVector<sherpa::GCPtr<AST> >;
  loc = _loc;
  addChild(child1);
  addChild(child2);

  ID = ++(AST::astCount);
  printVariant = 0;		
}

AST::AST(const AstType at, const LexLoc& _loc,
         sherpa::GCPtr<AST> child1,
         sherpa::GCPtr<AST> child2,
         sherpa::GCPtr<AST> child3)
{
  astType = at;
  children = new CVector<sherpa::GCPtr<AST> >;
  loc = _loc;
  addChild(child1);
  addChild(child2);
  addChild(child3);

  ID = ++(AST::astCount);
  printVariant = 0;		
}

AST::AST(const AstType at, const LexLoc& _loc,
         sherpa::GCPtr<AST> child1,
         sherpa::GCPtr<AST> child2,
         sherpa::GCPtr<AST> child3,
         sherpa::GCPtr<AST> child4)
{
  astType = at;
  children = new CVector<sherpa::GCPtr<AST> >;
  loc = _loc;
  addChild(child1);
  addChild(child2);
  addChild(child3);
  addChild(child4);

  ID = ++(AST::astCount);
  printVariant = 0;		
}

AST::AST(const AstType at, const LexLoc& _loc,
         sherpa::GCPtr<AST> child1,
         sherpa::GCPtr<AST> child2,
         sherpa::GCPtr<AST> child3,
         sherpa::GCPtr<AST> child4,
         sherpa::GCPtr<AST> child5)
{
  astType = at;
  children = new CVector<sherpa::GCPtr<AST> >;
  loc = _loc;
  addChild(child1);
  addChild(child2);
  addChild(child3);
  addChild(child4);
  addChild(child5);

  ID = ++(AST::astCount);
  printVariant = 0;		
}

std::string
AST::getTokenString()
{
  return s;
}

void
AST::addChild(sherpa::GCPtr<AST> child)
{
  children->append(child);
}

const char *
AST::tagName(const AstType at)
{
  switch(at) {
  case at_Null:
    return "at_Null";
  case at_AnyGroup:
    return "at_AnyGroup";
  case at_ident:
    return "at_ident";
  case at_string:
    return "at_string";
  case at_ifident:
    return "at_ifident";
  case at_usesel:
    return "at_usesel";
  case at_boolLiteral:
    return "at_boolLiteral";
  case at_intLiteral:
    return "at_intLiteral";
  case at_charLiteral:
    return "at_charLiteral";
  case at_floatLiteral:
    return "at_floatLiteral";
  case at_stringLiteral:
    return "at_stringLiteral";
  case at_uoc:
    return "at_uoc";
  case at_block:
    return "at_block";
  case at_s_print:
    return "at_s_print";
  case at_s_printstar:
    return "at_s_printstar";
  case at_s_assign:
    return "at_s_assign";
  case at_s_return:
    return "at_s_return";
  case at_s_def:
    return "at_s_def";
  case at_s_export_def:
    return "at_s_export_def";
  case at_s_fndef:
    return "at_s_fndef";
  case at_s_export_fndef:
    return "at_s_export_fndef";
  case at_idlist:
    return "at_idlist";
  case at_s_export:
    return "at_s_export";
  case at_s_import:
    return "at_s_import";
  case at_s_enum:
    return "at_s_enum";
  case at_s_export_enum:
    return "at_s_export_enum";
  case at_enum_bind:
    return "at_enum_bind";
  case at_s_bank:
    return "at_s_bank";
  case at_fncall:
    return "at_fncall";
  case at_ifelse:
    return "at_ifelse";
  case at_dot:
    return "at_dot";
  case at_vecref:
    return "at_vecref";
  case at_docString:
    return "at_docString";
  case agt_var:
    return "agt_var";
  case agt_literal:
    return "agt_literal";
  case agt_stmt:
    return "agt_stmt";
  case agt__AnonGroup0:
    return "agt__AnonGroup0";
  case agt__AnonGroup1:
    return "agt__AnonGroup1";
  case agt_expr:
    return "agt_expr";
  case agt__AnonGroup2:
    return "agt__AnonGroup2";
  default:
    return "<unknown>";
  }
}

const char *
AST::astTypeName() const
{
  return tagName(astType);
}

const char *
AST::astName() const
{
  switch(astType) {
  case at_Null:
    return "Null";
  case at_AnyGroup:
    return "AnyGroup";
  case at_ident:
    return "ident";
  case at_string:
    return "string";
  case at_ifident:
    return "ifident";
  case at_usesel:
    return "usesel";
  case at_boolLiteral:
    return "boolLiteral";
  case at_intLiteral:
    return "intLiteral";
  case at_charLiteral:
    return "charLiteral";
  case at_floatLiteral:
    return "floatLiteral";
  case at_stringLiteral:
    return "stringLiteral";
  case at_uoc:
    return "uoc";
  case at_block:
    return "block";
  case at_s_print:
    return "s_print";
  case at_s_printstar:
    return "s_printstar";
  case at_s_assign:
    return "s_assign";
  case at_s_return:
    return "s_return";
  case at_s_def:
    return "s_def";
  case at_s_export_def:
    return "s_export_def";
  case at_s_fndef:
    return "s_fndef";
  case at_s_export_fndef:
    return "s_export_fndef";
  case at_idlist:
    return "idlist";
  case at_s_export:
    return "s_export";
  case at_s_import:
    return "s_import";
  case at_s_enum:
    return "s_enum";
  case at_s_export_enum:
    return "s_export_enum";
  case at_enum_bind:
    return "enum_bind";
  case at_s_bank:
    return "s_bank";
  case at_fncall:
    return "fncall";
  case at_ifelse:
    return "ifelse";
  case at_dot:
    return "dot";
  case at_vecref:
    return "vecref";
  case at_docString:
    return "docString";
  case agt_var:
    return "{var}";
  case agt_literal:
    return "{literal}";
  case agt_stmt:
    return "{stmt}";
  case agt__AnonGroup0:
    return "{_AnonGroup0}";
  case agt__AnonGroup1:
    return "{_AnonGroup1}";
  case agt_expr:
    return "{expr}";
  case agt__AnonGroup2:
    return "{_AnonGroup2}";
  default:
    return "<unknown>";
  }
}

#define ISSET(v,b) ((v)[((b)/8)] & (1u << ((b)%8)))

void
astChTypeError(const AST &myAst, const AstType exp_at,
               const AstType act_at, size_t child)
{
  std::cerr << myAst.loc.asString() << ": " << myAst.astTypeName();
  std::cerr << " has incompatible Child# " << child;
  std::cerr << ". Expected " << AST::tagName(exp_at) << ", "; 
  std::cerr << "Obtained " << AST::tagName(act_at) << "." << std::endl;
}

void
astChNumError(const AST &myAst, const size_t exp_ch,
               const size_t act_ch)
{
  std::cerr << myAst.loc.asString() << ": " << myAst.astTypeName();
  std::cerr << " has wrong number of children. ";
  std::cerr << "Expected " << exp_ch << ", ";
  std::cerr << "Obtained " << act_ch << "." << std::endl;
}

static const unsigned char *astMembers[] = {
  (unsigned char *)"\x01\x00\x00\x00\x00", // at_Null
  (unsigned char *)"\x02\x00\x00\x00\x00", // at_AnyGroup
  (unsigned char *)"\x04\x00\x00\x00\x00", // at_ident
  (unsigned char *)"\x08\x00\x00\x00\x00", // at_string
  (unsigned char *)"\x10\x00\x00\x00\x00", // at_ifident
  (unsigned char *)"\x20\x00\x00\x00\x00", // at_usesel
  (unsigned char *)"\x40\x00\x00\x00\x00", // at_boolLiteral
  (unsigned char *)"\x80\x00\x00\x00\x00", // at_intLiteral
  (unsigned char *)"\x00\x01\x00\x00\x00", // at_charLiteral
  (unsigned char *)"\x00\x02\x00\x00\x00", // at_floatLiteral
  (unsigned char *)"\x00\x04\x00\x00\x00", // at_stringLiteral
  (unsigned char *)"\x00\x08\x00\x00\x00", // at_uoc
  (unsigned char *)"\x00\x10\x00\x00\x00", // at_block
  (unsigned char *)"\x00\x20\x00\x00\x00", // at_s_print
  (unsigned char *)"\x00\x40\x00\x00\x00", // at_s_printstar
  (unsigned char *)"\x00\x80\x00\x00\x00", // at_s_assign
  (unsigned char *)"\x00\x00\x01\x00\x00", // at_s_return
  (unsigned char *)"\x00\x00\x02\x00\x00", // at_s_def
  (unsigned char *)"\x00\x00\x04\x00\x00", // at_s_export_def
  (unsigned char *)"\x00\x00\x08\x00\x00", // at_s_fndef
  (unsigned char *)"\x00\x00\x10\x00\x00", // at_s_export_fndef
  (unsigned char *)"\x00\x00\x20\x00\x00", // at_idlist
  (unsigned char *)"\x00\x00\x40\x00\x00", // at_s_export
  (unsigned char *)"\x00\x00\x80\x00\x00", // at_s_import
  (unsigned char *)"\x00\x00\x00\x01\x00", // at_s_enum
  (unsigned char *)"\x00\x00\x00\x02\x00", // at_s_export_enum
  (unsigned char *)"\x00\x00\x00\x04\x00", // at_enum_bind
  (unsigned char *)"\x00\x00\x00\x08\x00", // at_s_bank
  (unsigned char *)"\x00\x00\x00\x10\x00", // at_fncall
  (unsigned char *)"\x00\x00\x00\x20\x00", // at_ifelse
  (unsigned char *)"\x00\x00\x00\x40\x00", // at_dot
  (unsigned char *)"\x00\x00\x00\x80\x00", // at_vecref
  (unsigned char *)"\x00\x00\x00\x00\x01", // at_docString
  (unsigned char *)"\x24\x00\x00\x00\x02", // agt_var
  (unsigned char *)"\x80\x06\x00\x00\x04", // agt_literal
  (unsigned char *)"\x84\xf4\xdf\xf3\x48", // agt_stmt
  (unsigned char *)"\x05\x00\x00\x00\x10", // agt__AnonGroup0
  (unsigned char *)"\x05\x00\x00\x00\x20", // agt__AnonGroup1
  (unsigned char *)"\x84\x14\x00\xf0\x40", // agt_expr
  (unsigned char *)"\x00\x10\x00\x20\x80"  // agt__AnonGroup2
};

bool
AST::isMemberOfType(AstType ty) const
{
  return ISSET(astMembers[ty], astType) ? true : false;}

bool
AST::isValid() const
{
  size_t c;
  size_t specNdx;
  bool errorsPresent = false;

  for (c = 0; c < children->size(); c++) {
    if (!child(c)->isValid())
      errorsPresent = true;
  }

  c = 0;
  specNdx = 0;

  switch(astType) {
  case at_Null: // leaf AST:
    if(children->size() != 0) {
      astChNumError(*this, 0, children->size());
      errorsPresent = true;
    }
    break;

  case at_AnyGroup: // leaf AST:
    if(children->size() != 0) {
      astChNumError(*this, 0, children->size());
      errorsPresent = true;
    }
    break;

  case at_ident: // leaf AST:
    if(children->size() != 0) {
      astChNumError(*this, 0, children->size());
      errorsPresent = true;
    }
    break;

  case at_string: // leaf AST:
    if(children->size() != 0) {
      astChNumError(*this, 0, children->size());
      errorsPresent = true;
    }
    break;

  case at_ifident: // leaf AST:
    if(children->size() != 0) {
      astChNumError(*this, 0, children->size());
      errorsPresent = true;
    }
    break;

  case at_usesel: // normal AST:
    // match at_ident
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[at_ident], child(c)->astType)) {
      astChTypeError(*this, at_ident, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    // match at_ident
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[at_ident], child(c)->astType)) {
      astChTypeError(*this, at_ident, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    if(c != children->size()) {
      astChNumError(*this, c, children->size());
      errorsPresent = true;
    }
    break;

  case at_boolLiteral: // leaf AST:
    if(children->size() != 0) {
      astChNumError(*this, 0, children->size());
      errorsPresent = true;
    }
    break;

  case at_intLiteral: // leaf AST:
    if(children->size() != 0) {
      astChNumError(*this, 0, children->size());
      errorsPresent = true;
    }
    break;

  case at_charLiteral: // leaf AST:
    if(children->size() != 0) {
      astChNumError(*this, 0, children->size());
      errorsPresent = true;
    }
    break;

  case at_floatLiteral: // leaf AST:
    if(children->size() != 0) {
      astChNumError(*this, 0, children->size());
      errorsPresent = true;
    }
    break;

  case at_stringLiteral: // leaf AST:
    if(children->size() != 0) {
      astChNumError(*this, 0, children->size());
      errorsPresent = true;
    }
    break;

  case at_uoc: // normal AST:
    // match at_ident
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[at_ident], child(c)->astType)) {
      astChTypeError(*this, at_ident, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    // match agt_stmt+
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[agt_stmt], child(c)->astType)) {
      astChTypeError(*this, agt_stmt, child(c)->astType, 1);
      errorsPresent = true;
    }
    while (c < children->size()) {
      if (!ISSET(astMembers[agt_stmt], child(c)->astType))
        astChTypeError(*this, agt_stmt, child(c)->astType, c);
      c++;
    }

    if(c != children->size()) {
      astChNumError(*this, c, children->size());
      errorsPresent = true;
    }
    break;

  case at_block: // normal AST:
    // match agt_stmt+
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[agt_stmt], child(c)->astType)) {
      astChTypeError(*this, agt_stmt, child(c)->astType, 0);
      errorsPresent = true;
    }
    while (c < children->size()) {
      if (!ISSET(astMembers[agt_stmt], child(c)->astType))
        astChTypeError(*this, agt_stmt, child(c)->astType, c);
      c++;
    }

    if(c != children->size()) {
      astChNumError(*this, c, children->size());
      errorsPresent = true;
    }
    break;

  case at_s_print: // normal AST:
    // match agt_expr
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[agt_expr], child(c)->astType)) {
      astChTypeError(*this, agt_expr, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    if(c != children->size()) {
      astChNumError(*this, c, children->size());
      errorsPresent = true;
    }
    break;

  case at_s_printstar: // normal AST:
    // match agt_expr
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[agt_expr], child(c)->astType)) {
      astChTypeError(*this, agt_expr, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    if(c != children->size()) {
      astChNumError(*this, c, children->size());
      errorsPresent = true;
    }
    break;

  case at_s_assign: // normal AST:
    // match agt_expr
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[agt_expr], child(c)->astType)) {
      astChTypeError(*this, agt_expr, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    // match agt_expr
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[agt_expr], child(c)->astType)) {
      astChTypeError(*this, agt_expr, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    if(c != children->size()) {
      astChNumError(*this, c, children->size());
      errorsPresent = true;
    }
    break;

  case at_s_return: // normal AST:
    // match agt_expr
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[agt_expr], child(c)->astType)) {
      astChTypeError(*this, agt_expr, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    if(c != children->size()) {
      astChNumError(*this, c, children->size());
      errorsPresent = true;
    }
    break;

  case at_s_def: // normal AST:
    // match at_ident
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[at_ident], child(c)->astType)) {
      astChTypeError(*this, at_ident, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    // match agt_expr
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[agt_expr], child(c)->astType)) {
      astChTypeError(*this, agt_expr, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    if(c != children->size()) {
      astChNumError(*this, c, children->size());
      errorsPresent = true;
    }
    break;

  case at_s_export_def: // normal AST:
    // match at_ident
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[at_ident], child(c)->astType)) {
      astChTypeError(*this, at_ident, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    // match agt_expr
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[agt_expr], child(c)->astType)) {
      astChTypeError(*this, agt_expr, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    if(c != children->size()) {
      astChNumError(*this, c, children->size());
      errorsPresent = true;
    }
    break;

  case at_s_fndef: // normal AST:
    // match at_ident
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[at_ident], child(c)->astType)) {
      astChTypeError(*this, at_ident, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    // match at_idlist
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[at_idlist], child(c)->astType)) {
      astChTypeError(*this, at_idlist, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    // match at_block
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[at_block], child(c)->astType)) {
      astChTypeError(*this, at_block, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    if(c != children->size()) {
      astChNumError(*this, c, children->size());
      errorsPresent = true;
    }
    break;

  case at_s_export_fndef: // normal AST:
    // match at_ident
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[at_ident], child(c)->astType)) {
      astChTypeError(*this, at_ident, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    // match at_idlist
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[at_idlist], child(c)->astType)) {
      astChTypeError(*this, at_idlist, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    // match at_block
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[at_block], child(c)->astType)) {
      astChTypeError(*this, at_block, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    if(c != children->size()) {
      astChNumError(*this, c, children->size());
      errorsPresent = true;
    }
    break;

  case at_idlist: // normal AST:
    // match at_ident*
    while (c < children->size()) {
      if (!ISSET(astMembers[at_ident], child(c)->astType))
        astChTypeError(*this, at_ident, child(c)->astType, c);
      c++;
    }

    if(c != children->size()) {
      astChNumError(*this, c, children->size());
      errorsPresent = true;
    }
    break;

  case at_s_export: // normal AST:
    // match at_ident*
    while (c < children->size()) {
      if (!ISSET(astMembers[at_ident], child(c)->astType))
        astChTypeError(*this, at_ident, child(c)->astType, c);
      c++;
    }

    if(c != children->size()) {
      astChNumError(*this, c, children->size());
      errorsPresent = true;
    }
    break;

  case at_s_import: // normal AST:
    // match at_ident
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[at_ident], child(c)->astType)) {
      astChTypeError(*this, at_ident, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    // match at_ident
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[at_ident], child(c)->astType)) {
      astChTypeError(*this, at_ident, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    if(c != children->size()) {
      astChNumError(*this, c, children->size());
      errorsPresent = true;
    }
    break;

  case at_s_enum: // normal AST:
    // match agt__AnonGroup0
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[agt__AnonGroup0], child(c)->astType)) {
      astChTypeError(*this, agt__AnonGroup0, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    // match at_enum_bind*
    while (c < children->size()) {
      if (!ISSET(astMembers[at_enum_bind], child(c)->astType))
        astChTypeError(*this, at_enum_bind, child(c)->astType, c);
      c++;
    }

    if(c != children->size()) {
      astChNumError(*this, c, children->size());
      errorsPresent = true;
    }
    break;

  case at_s_export_enum: // normal AST:
    // match agt__AnonGroup1
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[agt__AnonGroup1], child(c)->astType)) {
      astChTypeError(*this, agt__AnonGroup1, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    // match at_enum_bind*
    while (c < children->size()) {
      if (!ISSET(astMembers[at_enum_bind], child(c)->astType))
        astChTypeError(*this, at_enum_bind, child(c)->astType, c);
      c++;
    }

    if(c != children->size()) {
      astChNumError(*this, c, children->size());
      errorsPresent = true;
    }
    break;

  case at_enum_bind: // normal AST:
    // match at_ident
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[at_ident], child(c)->astType)) {
      astChTypeError(*this, at_ident, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    // match agt_expr?
    if ((c < children->size()) && ISSET(astMembers[agt_expr], child(c)->astType))
      c++;

    if(c != children->size()) {
      astChNumError(*this, c, children->size());
      errorsPresent = true;
    }
    break;

  case at_s_bank: // normal AST:
    // match agt_expr
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[agt_expr], child(c)->astType)) {
      astChTypeError(*this, agt_expr, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    // match at_block
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[at_block], child(c)->astType)) {
      astChTypeError(*this, at_block, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    if(c != children->size()) {
      astChNumError(*this, c, children->size());
      errorsPresent = true;
    }
    break;

  case at_fncall: // normal AST:
    // match agt_expr
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[agt_expr], child(c)->astType)) {
      astChTypeError(*this, agt_expr, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    // match agt_expr*
    while (c < children->size()) {
      if (!ISSET(astMembers[agt_expr], child(c)->astType))
        astChTypeError(*this, agt_expr, child(c)->astType, c);
      c++;
    }

    if(c != children->size()) {
      astChNumError(*this, c, children->size());
      errorsPresent = true;
    }
    break;

  case at_ifelse: // normal AST:
    // match agt_expr
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[agt_expr], child(c)->astType)) {
      astChTypeError(*this, agt_expr, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    // match at_block
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[at_block], child(c)->astType)) {
      astChTypeError(*this, at_block, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    // match agt__AnonGroup2?
    if ((c < children->size()) && ISSET(astMembers[agt__AnonGroup2], child(c)->astType))
      c++;

    if(c != children->size()) {
      astChNumError(*this, c, children->size());
      errorsPresent = true;
    }
    break;

  case at_dot: // normal AST:
    // match agt_expr
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[agt_expr], child(c)->astType)) {
      astChTypeError(*this, agt_expr, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    // match at_ident
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[at_ident], child(c)->astType)) {
      astChTypeError(*this, at_ident, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    if(c != children->size()) {
      astChNumError(*this, c, children->size());
      errorsPresent = true;
    }
    break;

  case at_vecref: // normal AST:
    // match agt_expr
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[agt_expr], child(c)->astType)) {
      astChTypeError(*this, agt_expr, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    // match agt_expr
    if(c >= children->size()) {
      astChNumError(*this, c+1, children->size());
      errorsPresent = true;
      break;
    }
    if (!ISSET(astMembers[agt_expr], child(c)->astType)) {
      astChTypeError(*this, agt_expr, child(c)->astType, c);
      errorsPresent = true;
    }
    c++;

    if(c != children->size()) {
      astChNumError(*this, c, children->size());
      errorsPresent = true;
    }
    break;

  case at_docString: // normal AST:
    // match at_stringLiteral?
    if ((c < children->size()) && ISSET(astMembers[at_stringLiteral], child(c)->astType))
      c++;

    if(c != children->size()) {
      astChNumError(*this, c, children->size());
      errorsPresent = true;
    }
    break;

  // group ASTagt_var gets default
    break;

  // group ASTagt_literal gets default
    break;

  // group ASTagt_stmt gets default
    break;

  // group ASTagt__AnonGroup0 gets default
    break;

  // group ASTagt__AnonGroup1 gets default
    break;

  // group ASTagt_expr gets default
    break;

  // group ASTagt__AnonGroup2 gets default
    break;

  default:
    errorsPresent = true;
  }

  return !errorsPresent;
}


GCPtr<AST> 
AST::makeBoolLit(const sherpa::LToken &tok)
{
  GCPtr<AST> ast = new AST(at_boolLiteral, tok);
  ast->litValue = new BoolValue(tok.str);
  return ast;
}

GCPtr<AST> 
AST::makeCharLit(const sherpa::LToken &tok)
{
  
  

  GCPtr<AST> ast = new AST(at_charLiteral, tok);
  ast->litValue = new CharValue(tok.str);
  return ast;
}

GCPtr<AST>  
AST::makeIntLit(const sherpa::LToken &tok) 
{
  GCPtr<AST> ast = new AST(at_intLiteral, tok);
  ast->litValue = new IntValue(tok.str);
  return ast;
}

GCPtr<AST> 
AST::makeFloatLit(const sherpa::LToken &tok) 
{ 
  GCPtr<AST> ast = new AST(at_floatLiteral, tok);
  ast->litValue = new FloatValue(tok.str);
  return ast;
}

GCPtr<AST> 
AST::makeStringLit(const sherpa::LToken &tok)
{
  GCPtr<AST> ast = new AST(at_stringLiteral, tok);
  ast->litValue = new StringValue(tok.str);
  
  return ast;
}

