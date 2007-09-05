%{
/*
 * Copyright (C) 2006, The EROS Group, LLC.
 *
 * This file is part of the Coyotos Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <sys/fcntl.h>
#include <sys/stat.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <dirent.h>
#include <iostream>

#include <string>

#include "AST.hxx"
#include "ParseType.hxx"
  // #include "UocInfo.hxx"
#include "Options.hxx"
  
using namespace sherpa;
using namespace std;  

#define YYSTYPE ParseType
#define YYLEX_PARAM (Lexer *)lexer
#undef yyerror
#define yyerror(lexer, topast, s) lexer->ReportParseError(s)

#include "Lexer.hxx"

#define SHOWPARSE(s) \
  do { \
    if (Options::showParse) \
      lexer->errStream << (s) << std::endl;		\
  } while(false);
#define SHOWPARSE1(s,x) \
  do { \
    if (Options::showParse) \
      lexer->errStream << (s) << " " << (x) << std::endl;	\
  } while(false);


int num_errors = 0;  /* hold the number of syntax errors encountered. */

inline int mkilex(YYSTYPE *lvalp, Lexer *lexer)
{
  return lexer->lex(lvalp);
}

%}

%pure-parser
%parse-param {Lexer *lexer}
%parse-param {GCPtr<AST> & topAst}

%token <tok> tk_Reserved	/* reserved words */

/* Categorical terminals: */
%token <tok> tk_Ident
%token <tok> tk_Int
%token <tok> tk_String

/* Primary types and associated hand-recognized literals: */
%token <tok> ','

 /* Assignment, statements */
%token <tok> '=' ';'

%token <tok> tk_MODULE

%token <tok> tk_RETURN

%token <tok> tk_ENUM
%token <tok> tk_IF
%token <tok> tk_ELSE

%token <tok> tk_TRUE
%token <tok> tk_FALSE
%token <tok> tk_PRINT

%token <tok> tk_DEF
%token <tok> tk_EXPORT
%token <tok> tk_IMPORT

 /* Customary expression operators here. Order of occurrence is
    significant, because it defines precedence. Here is the operator
    precedence in ANSI C, from highest to lowest:

     L   function() [] -> .
     R   ! ~ ++ -- unary+ unary- unary* & (type) sizeof
     L   * / %
     L   + -
     L   << >>
     L   < <= > >=
     L   == !=
     L   &
     L   ^
     L   |
     L   &&
     L   ||
     R   ?:
     R   = += -= *= /= %/ &= ^= |= <<= >>=
     L   ,
 */
 /* Lowest precedence */
%left <tok> tk_CMP_EQL tk_CMP_NOTEQL
%left <tok> '<' '>' tk_CMP_LEQL tk_CMP_GEQL
%left <tok> '-' '+'
%left <tok> '*' '/' '%'
%right <tok> '!'
%left <tok> '.'
%left <tok> '(' '['		/* function call, vector index */

%token <tok> ')'
%token <tok> ']'
 /* Highest precedence */

%type <ast> start
%type <ast> block block_stmt block_stmt_seq statement if_statement
%type <ast> enum_stmt fndef_stmt def_stmt
%type <tok> modname
%type <ast> mod_stmt_seq mod_stmt
%type <ast> fnparms idlist
%type <ast> expr exprlist fnargs
%type <ast> enum_decl enum_decls

%%

start: tk_MODULE modname '{' mod_stmt_seq '}' {
  SHOWPARSE("start -> MODULE modname stmt_seq");
  topAst = new AST(at_uoc, $1.loc, new AST(at_ident, $2));
  topAst->addChildrenFrom($4);
}

modname: {
    lexer->setIfIdentMode(true);
  } tk_Ident {
  lexer->setIfIdentMode(false);

  SHOWPARSE("modname -> Ident");
  $$ = $2;
}

modname: modname {
    lexer->setIfIdentMode(true);
  } '.' tk_Ident {
  lexer->setIfIdentMode(false);

  SHOWPARSE("modname -> modname '.' Ident");
  $$ = LToken($1.loc, $1.str + "." + $4.str);
}

mod_stmt_seq: mod_stmt {
  SHOWPARSE("mod_stmt_seq -> mod_stmt");
  $$ = new AST(at_Null, $1->loc, $1);
};

mod_stmt_seq: mod_stmt_seq mod_stmt {
  SHOWPARSE("mod_stmt_seq -> mod_stmt_seq mod_stmt");
  $$ = $1;
  $1->addChild($2);
};

mod_stmt : statement {
  SHOWPARSE("mod_stmt -> statement");
  $$ = $1;
}

fnparms: '(' ')' {
  SHOWPARSE("fnparms -> '(' ')'");
  $$ = new AST(at_idlist, $1.loc);
}

fnparms: '(' idlist ')' {
  SHOWPARSE("fnparms -> '(' idlist ')'");
  $$ = $2;
};

/* These definitions may only appear at module scope: */
statement: fndef_stmt {
  SHOWPARSE("mod_stmt -> fndef_stmt");
  $$ = $1;
};

mod_stmt: tk_EXPORT fndef_stmt {
  SHOWPARSE("mod_stmt -> EXPORT fndef_stmt");
  $$ = $2;
  $$->astType = at_s_export_fndef;
};

fndef_stmt: tk_DEF tk_Ident fnparms block {
  SHOWPARSE("fndef_stmt -> FUNCTION Ident fnparms block");
  $$ = new AST(at_s_fndef, $1.loc, new AST(at_ident, $2), $3, $4);
};

mod_stmt: tk_EXPORT enum_stmt {
  SHOWPARSE("mod_stmt -> EXPORT enum_stmt");
  $$ = $2;
  $$->astType = at_s_export_enum;
};

mod_stmt: tk_EXPORT def_stmt {
  SHOWPARSE("mod_stmt -> EXPORT def_stmt");
  $$ = $2;
  $$->astType = at_s_export_def;
};

mod_stmt: tk_IMPORT tk_Ident '=' modname ';' {
  SHOWPARSE("mod_stmt -> IMPORT Ident = mod_name");
  $$ = new AST(at_s_import, $1.loc, new AST(at_ident, $2), 
	       new AST(at_ident, $4));
}
 
idlist: tk_Ident {
  SHOWPARSE("idlist -> Ident");
  $$ = new AST(at_idlist, $1.loc, new AST(at_ident, $1));
}
idlist: idlist ',' tk_Ident {
  SHOWPARSE("idlist -> idlist ',' Ident");
  $$ = $1;
  $$->addChild(new AST(at_ident, $3));
}

block_stmt:  tk_RETURN expr ';' {
  SHOWPARSE("block_stmt -> RETURN expr ';'");
  $$ = new AST (at_s_return, $1.loc, $2);
};

block_stmt:  statement {
  SHOWPARSE("block_stmt -> statement");
  $$ = $1;
};

block_stmt_seq: block_stmt {
  SHOWPARSE("block_stmt_seq -> block_stmt");
  $$ = new AST(at_block, $1->loc, $1);
};

block_stmt_seq: block_stmt_seq block_stmt {
  SHOWPARSE("block_stmt_seq -> block_stmt_seq block_stmt");
  $$ = $1;
  $1->addChild($2);
};

block: '{' block_stmt_seq '}' {
  SHOWPARSE("block -> '{' block_stmt_seq '}'");
  $$ = $2;
};

statement: expr '=' expr ';' {
  SHOWPARSE("statement -> Ident '=' expr ';'");
  $$ = new AST(at_s_assign, $2.loc, $1, $3);
};

statement:  expr ';' {
  SHOWPARSE("statement -> expr ';'");
  $$ = $1;
  };

statement: tk_PRINT expr ';' {
  SHOWPARSE("statement -> PRINT expr ';'");
  $$ = new AST(at_s_print, $1.loc, $2);
};

statement: tk_PRINT '*' expr ';' {
  SHOWPARSE("statement -> PRINT '*' expr ';'");
  $$ = new AST(at_s_printstar, $1.loc, $3);
};

statement: def_stmt {
  SHOWPARSE("statement -> def_stmt");
  $$ = $1;
};

def_stmt: tk_DEF tk_Ident '=' expr ';' {
  SHOWPARSE("def_stmt -> DEF Ident '=' expr ';'");
  $$ = new AST(at_s_def, $1.loc, new AST(at_ident, $2), $4);
};

statement: enum_stmt {
  SHOWPARSE("statement -> enum_stmt");
  $$ = $1;
};

enum_stmt: tk_ENUM tk_Ident '{' enum_decls '}' ';' {
  SHOWPARSE("enum_stmt -> ENUM Ident '{' enumerations '}' ';'");
  $$ = new AST(at_s_enum, $1.loc, new AST(at_ident, $2));
  $$->addChildrenFrom($4);
};

enum_stmt: tk_ENUM '{' enum_decls '}' ';' {
  SHOWPARSE("enum_stmt -> ENUM '{' enumerations '}' ';'");
  $$ = new AST(at_s_enum, $1.loc, new AST(at_Null, $1.loc));
  $$->addChildrenFrom($3);
};

enum_decls: enum_decl {
  SHOWPARSE("enum_decls -> enum_decl");
  $$ = new AST(at_Null, $1->loc, $1);
}

enum_decls: enum_decls ',' enum_decl {
  SHOWPARSE("enum_decls -> enum_decls ',' enum_decl");
  $$ = $1;
  $1->addChild($3);
}

enum_decl: tk_Ident {
  SHOWPARSE("enum_decl -> Ident");
  $$ = new AST(at_enum_bind, $1.loc, new AST(at_ident, $1));
}

enum_decl: tk_Ident '=' expr {
  SHOWPARSE("enum_decl -> Ident '=' expr");
  $$ = new AST(at_enum_bind, $1.loc, new AST(at_ident, $1),
	       $3);
}

if_statement : tk_IF '(' expr ')' block {
  SHOWPARSE("if_statement -> IF '(' expr ')' block");
  $$ = new AST (at_ifelse, $1.loc, $3, $5);
};

if_statement : tk_IF '(' expr ')' block tk_ELSE block {
  SHOWPARSE("if_statement -> IF '(' expr ')' block ELSE block");
  $$ = new AST (at_ifelse, $1.loc, $3, $5, $7);
};

if_statement : tk_IF '(' expr ')' block tk_ELSE if_statement {
  SHOWPARSE("if_statement -> IF '(' expr ')' block ELSE if_statement");
  $$ = new AST (at_ifelse, $1.loc, $3, $5, $7);
};

statement: if_statement {
  SHOWPARSE("statement -> if_statement");
  $$ = $1;
}

expr : expr '+' expr {
  SHOWPARSE("expr -> expr '+' expr");
  $$ = new AST (at_fncall, $2.loc, new AST(at_ident, $2), $1, $3);
};

expr : expr '-' expr {
  SHOWPARSE("expr -> expr '-' expr");
  $$ = new AST (at_fncall, $2.loc, new AST(at_ident, $2), $1, $3);
};

expr : expr '*' expr {
  SHOWPARSE("expr -> expr '*' expr");
  $$ = new AST (at_fncall, $2.loc, new AST(at_ident, $2), $1, $3);
};

expr : expr '/' expr {
  SHOWPARSE("expr -> expr '/' expr");
  $$ = new AST (at_fncall, $2.loc, new AST(at_ident, $2), $1, $3);
};

expr : expr '%' expr {
  SHOWPARSE("expr -> expr '%' expr");
  $$ = new AST (at_fncall, $2.loc, new AST(at_ident, $2), $1, $3);
};


expr : '-' expr {
  SHOWPARSE("expr -> '-' expr");
  $$ = new AST (at_fncall, $1.loc, 
		new AST(at_ident, LToken($1.loc, "unary-")), $2);
};

expr : expr '<' expr {
  SHOWPARSE("expr -> expr '<' expr");
  $$ = new AST (at_fncall, $2.loc, new AST(at_ident, $2), $1, $3);
}

expr : expr '>' expr {
  SHOWPARSE("expr -> expr '<' expr");
  $$ = new AST (at_fncall, $2.loc, new AST(at_ident, $2), $1, $3);
}

expr : expr tk_CMP_LEQL expr {
  SHOWPARSE("expr -> expr '<=' expr");
  $$ = new AST (at_fncall, $2.loc, new AST(at_ident, $2), $1, $3);
}

expr : expr tk_CMP_GEQL expr {
  SHOWPARSE("expr -> expr '>=' expr");
  $$ = new AST (at_fncall, $2.loc, new AST(at_ident, $2), $1, $3);
}

expr : expr tk_CMP_EQL expr {
  SHOWPARSE("expr -> expr '==' expr");
  $$ = new AST (at_fncall, $2.loc, new AST(at_ident, $2), $1, $3);
}

expr : expr tk_CMP_NOTEQL expr {
  SHOWPARSE("expr -> expr '!=' expr");
  $$ = new AST (at_fncall, $2.loc, new AST(at_ident, $2), $1, $3);
}

expr : '!' expr {
  SHOWPARSE("expr -> '!' expr");
  $$ = new AST (at_fncall, $1.loc, new AST(at_ident, $1), $2);
};

expr : block {
  SHOWPARSE("expr -> block");
  $$ = $1;
}

fnargs: '(' ')' {
  SHOWPARSE("fnargs -> '(' ')'");
  $$ = new AST(at_Null, $1.loc);
}

fnargs: '(' exprlist ')' {
  SHOWPARSE("fnargs -> '(' exprlist ')'");
  $$ = $2;
}

expr : expr fnargs {
  SHOWPARSE("expr -> expr fnargs");
  $$ = new AST(at_fncall, $1->loc, $1);
  $$->addChildrenFrom($2);
};

expr : expr '.' tk_Ident {
  SHOWPARSE("expr -> expr '.' Ident");
  $$ = new AST(at_dot, $2.loc, $1, new AST(at_ident, $3));
};

expr : expr '[' expr ']' {
  SHOWPARSE("expr -> expr '[' expr ']'");
  $$ = new AST(at_vecref, $2.loc, $1, $3);
};

expr : tk_Int {
  SHOWPARSE("expr -> Int");
  $$ = AST::makeIntLit($1);
};

expr : tk_String {
  SHOWPARSE("expr -> String");
  $$ = AST::makeStringLit($1);
};

expr : tk_Ident {
  SHOWPARSE("expr -> Ident");
  $$ = new AST(at_ident, $1);
};

exprlist: expr {
  SHOWPARSE("exprlist -> expr");
  $$ = new AST(at_Null);
  $$->addChild($1);
};
exprlist: exprlist ',' expr {
  SHOWPARSE("exprlist -> exprlist ',' expr");
  $$ = $1;
  $$->addChild($3);
};

%%

