#ifndef SYMTAB_HXX
#define SYMTAB_HXX

/*
 * Copyright (C) 2007, The EROS Group, LLC.
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

#include <libsherpa/CVector.hxx>
#include <libsherpa/BigNum.hxx>
#include <libsherpa/GCPtr.hxx>
#include "LTokString.hxx"
#include "DocComment.hxx"
#include "ArchInfo.hxx"

using namespace sherpa;

/*
 * Token type structure. Using a structure for this is a quite
 * amazingly bad idea, but using a union makes the C++ constructor
 * logic unhappy....
 */

enum LitType {			/* literal type */
  lt_void,
  lt_integer,
  lt_unsigned,
  lt_float,
  lt_char,
  lt_bool,
  lt_string,
};

struct LitValue {
  sherpa::BigNum  bn;		/* large precision integers */
  double          d;		/* doubles, floats */
  LitType	  lty;
  /* no special field for lt_string, as the name is the literal */
};

/* There is a design issue hiding here: how symbolic should the output
 * of the IDL compiler be? I think the correct answer is "very", in
 * which case we may need to do some tail chasing for constants. Thus,
 * I consider a computed constant value to be a symbol.
 */

enum SymClass {
#define SYMCLASS(pn, x,n) sc_##x,
#include "symclass.def"
#undef  SYMCLASS
};

#define SYMDEBUG

#define SF_NOSTUB     0x1u
#define SF_NO_OPCODE  0x2u

struct Symbol;

typedef sherpa::CVector<GCPtr<Symbol> > SymVec;

struct Symbol : public sherpa::Countable {
private:
  static const char *sc_names[];
  static const char *sc_printnames[];
  static unsigned sc_isScope[];

public:
  static GCPtr<Symbol> CurScope;
  static GCPtr<Symbol> VoidType;
  static GCPtr<Symbol> UniversalScope;
  static GCPtr<Symbol> KeywordScope;


  sherpa::LexLoc loc;
  std::string    name;
#ifdef SYMDEBUG
  std::string    qualifiedName;
#endif
  DocComment    *docComment;

  bool           mark;		/* used for circularity detection */
  bool           isActiveUOC;	/* created in an active unit of compilation */

  GCPtr<Symbol>  nameSpace;	/* containing namespace */
  SymVec         children;	/* members of the scope */
  SymVec         raises;	/* exceptions raised by this method/interface */
  SymClass       cls;

  GCPtr<Symbol>  type;		/* type of an identifier */
  GCPtr<Symbol>  baseType;	/* base type, if extension */
  GCPtr<Symbol>  value;		/* value of a constant */
				  
  bool           complete;	/* used for top symbols only. */
  unsigned       ifDepth;	/* depth of inheritance tree */

  unsigned       flags;		/* flags that govern code generation */

  LitValue       v;		/* only for sc_value and sc_builtin */

  /* Using a left-recursive grammar is always really messy, because
   * you have to do the scope push/pop by hand, which is at best
   * messy. Bother.
   */

private:
  Symbol();

  static GCPtr<Symbol> Construct(const sherpa::LToken& tok, bool isActiveUOC, SymClass);

  inline void
  AddChild(GCPtr<Symbol> sym)
  {
    children.append(sym);
  }


public:
  /* Creates a new symbol of the specified type in the currently
   * active scope. This is not a constructor because it must be able
   * to return failure in the event of a symbol name collision. */
  static GCPtr<Symbol> 
  Create_inScope(const sherpa::LToken& nm, 
		 bool isActiveUOC, SymClass, 
		 GCPtr<Symbol> inScope);

  static GCPtr<Symbol> 
  Create(const sherpa::LToken& nm, bool isActiveUOC, SymClass);

  static GCPtr<Symbol> 
  CreatePackage(const sherpa::LToken& nm, GCPtr<Symbol> inPkg);

  static GCPtr<Symbol> 
  CreateRef(const sherpa::LToken& nm, bool isActiveUOC);

  static GCPtr<Symbol> 
  CreateRef_inScope(const sherpa::LToken& nm, 
		    bool isActiveUOC, 
		    GCPtr<Symbol>  inScope);

  static GCPtr<Symbol> 
  CreateRaisesRef(const sherpa::LToken& nm, bool isActiveUOC);

  static GCPtr<Symbol> 
  CreateRaisesRef_inScope(const sherpa::LToken& nm, 
			  bool isActiveUOC, 
			  GCPtr<Symbol> inScope);

  static GCPtr<Symbol> 
  GenSym(SymClass, bool isActiveUOC);

  static GCPtr<Symbol> 
  GenSym_inScope(SymClass, bool isActiveUOC, GCPtr<Symbol> inScope);
  
  static GCPtr<Symbol> 
  MakeKeyword(const std::string& nm, SymClass sc, LitType lt, unsigned value);

  static GCPtr<Symbol> MakeIntLitFromBigNum(const sherpa::LexLoc& loc, 
				      const sherpa::BigNum&);
  static GCPtr<Symbol> MakeIntLit(const sherpa::LToken&);
  static GCPtr<Symbol> MakeStringLit(const sherpa::LToken&);
  static GCPtr<Symbol> MakeCharLit(const sherpa::LToken&);
  static GCPtr<Symbol> MakeFloatLit(const sherpa::LToken&);
  static GCPtr<Symbol> MakeExprNode(const sherpa::LToken& op, 
			      GCPtr<Symbol> left,
			      GCPtr<Symbol> right);

  /* Following, when applied to scalar types (currently only integer
   * types) will produce min and max values. */
  static GCPtr<Symbol> MakeMaxLit(const sherpa::LexLoc& loc, GCPtr<Symbol> );
  static GCPtr<Symbol> MakeMinLit(const sherpa::LexLoc& loc, GCPtr<Symbol> );

  static GCPtr<Symbol> LookupIntType(unsigned bitsz, bool uIntType);
  static void PushScope(GCPtr<Symbol> newScope);
  static void PopScope();

  static void InitSymtab();
  static GCPtr<Symbol> FindPackageScope();

  void ClearAllMarks();

  unsigned CountChildren(bool (Symbol::*predicate)());
  GCPtr<Symbol> LookupChild(const std::string& name, GCPtr<Symbol> bound);
  GCPtr<Symbol> LexicalLookup(const std::string& name, GCPtr<Symbol> bound);
  GCPtr<Symbol> LookupChild(const sherpa::LToken& tok, GCPtr<Symbol> bound)
  {
    return LookupChild(tok.str, bound);
  }
  GCPtr<Symbol> LexicalLookup(const sherpa::LToken& tok, GCPtr<Symbol> bound)
  {
    return LexicalLookup(tok.str, bound);
  }

  bool ResolveSymbolReference();
  bool ResolveInheritedReference();

#ifdef SYMDEBUG
  void QualifyNames();
#else
  inline void QualifyNames()
{
}
#endif
  bool ResolveReferences();
  void ResolveIfDepth();
  bool TypeCheck();

  bool IsLinearizable();
  bool IsFixedSerializable();
  bool IsDirectSerializable();

  void DoComputeDependencies(SymVec&);
  void ComputeDependencies(SymVec&);
#if 0
  void ComputeTransDependencies(SymVec&);
#endif

  unsigned alignof(const ArchInfo&);
  unsigned directSize(const ArchInfo&);
  unsigned indirectSize(const ArchInfo&);

  GCPtr<Symbol> UnitOfCompilation();

  /* Return TRUE iff the passed symbol is itself (directly) a type
     symbol. Note that if you update this list you should also update it
     in the switch in symbol_ResolveType() */
  inline bool IsBasicType()
  {
    if (this == NULL)
      return 0;
  
    return (cls == sc_primtype ||
	    cls == sc_enum ||
	    cls == sc_struct ||
	    cls == sc_union ||
	    cls == sc_interface ||
	    cls == sc_absinterface ||
	    cls == sc_seqType ||
	    cls == sc_bufType ||
	    cls == sc_arrayType);
  }

  /* Chase down symrefs to find the actual defining reference of a symbol */
  inline GCPtr<Symbol> ResolveRef()
  {
    GCPtr<Symbol> sym = this;

    while (sym->cls == sc_symRef)
      sym = sym->value;

    return sym;
  }

  GCPtr<Symbol> ResolvePackage();

  /* Assuming that the passed symbol is a type symbol, chase through
     symrefs and typedefs to find the actual type. */
  GCPtr<Symbol> ResolveType();

  /* Return TRUE iff the symbol is a type symbol */
  inline bool IsTypeSymbol()
  {
    GCPtr<Symbol> sym = ResolveRef();
  
    return sym->IsBasicType() || (sym->cls == sc_typedef);
  }

  /* Return TRUE iff the symbol is of the provided type */
  inline bool IsType(GCPtr<Symbol> sym, SymClass sc)
  {
    GCPtr<Symbol> typeSym = ResolveType();
    if (!typeSym)
      return false;

    return (typeSym->cls == sc);
  }

  inline bool IsConstantSymbol()
  {
    return (cls == sc_const);
  }

  inline bool IsEnumSymbol()
  {
    return (cls == sc_enum);
  }

  /* Return TRUE iff the symbol, after resolving name references, is a
     typedef. Certain parameter types are legal, but only if they appear
     in typedefed form. */
  inline bool IsTypedef()
  {
    GCPtr<Symbol> sym = ResolveRef();
    if (!sym)
      return false;
    return (sym->cls == sc_typedef);
  }

#if 0
  /* Return TRUE iff the type of this symbol is some sort of aggregate
     type. */
  inline bool IsAggregateType()
  {
    GCPtr<Symbol> sym = ResolveType();
    if (!sym) return false;

    return (sym->cls == sc_struct ||
	    sym->cls == sc_union ||
	    sym->cls == sc_bufType ||
	    sym->cls == sc_seqType ||
	    sym->cls == sc_arrayType);
  }
#endif

  inline bool IsValidParamType()
  {
    GCPtr<Symbol> typeSym = ResolveType();
    
    if (!typeSym) return false;

    /* Sequences, Buffers, are invalid, but typedefs of these are okay. */
    if (typeSym->cls == sc_seqType || typeSym->cls == sc_bufType)
      return IsTypedef();

    return typeSym->IsBasicType();
  }

  inline bool IsValidMemberType()
  {
    GCPtr<Symbol> sym = ResolveType();

    if (!sym)
      return false;

    /* Buffers and typdefs of buffers are disallowed. */
    if (sym->cls == sc_bufType)
      return false;

    return sym->IsBasicType();
  }

  inline bool IsValidSequenceBaseType()
  {
    GCPtr<Symbol> sym = ResolveType();

    if (!sym) return false;

    /* Variable number of client preallocated buffers makes no sense. */
    if (sym->cls == sc_bufType)
      return false;

    /* Temporary restriction: we do not allow sequences of
       sequences. This is an implementation restriction, not a
       fundamental problem. */
    if (sym->cls == sc_arrayType || sym->cls == sc_seqType)
      return false;
  
    return sym->IsBasicType();
  }

  inline bool IsValidBufferBaseType()
  {
    GCPtr<Symbol> sym = ResolveType();

    if (!sym) return false;

    /* Temporary restriction: we do not allow sequences of
       sequences. This is an implementation restriction, not a
       fundamental problem. */
    if (sym->cls == sc_arrayType || sym->cls == sc_seqType ||
	sym->cls == sc_bufType)
      return false;

    return sym->IsBasicType();
  }

  inline bool IsSequenceType()
  {
    GCPtr<Symbol> sym = ResolveType();
  
    if (!sym) return false;

    return (sym->cls == sc_seqType);
  }

  inline bool IsBufferType()
  {
    GCPtr<Symbol> sym = ResolveType();
  
    if (!sym) return false;

    return (sym->cls == sc_bufType);
  }

  inline bool IsFixSequenceType()
  {
    GCPtr<Symbol> sym = ResolveType();
    if (!sym) return false;
  
    return sym->cls == sc_arrayType;
  }

  inline bool IsConstantValue()
  {
    GCPtr<Symbol> sym = ResolveRef();
    if (!sym) return false;
  
    return (sym->cls == sc_const ||
	    sym->cls == sc_value ||
	    sym->cls == sc_arithop);
  }

  inline bool IsInterface()
  {
    GCPtr<Symbol> sym = ResolveType();
    if (!sym) return false;

    return (sym->cls == sc_interface ||
	    sym->cls == sc_absinterface);
  }

  inline bool IsOperation()
  {
    GCPtr<Symbol> sym = ResolveType();
    if (!sym) return false;

    return (sym->cls == sc_operation);
  }

  inline bool IsException()
  {
    GCPtr<Symbol> sym = ResolveType();
    if (!sym) return false;

    return (sym->cls == sc_exception);
  }

  /* Return TRUE if the symbol is a type that is passed by reference 
     rather than by copy. */
  inline bool IsReferenceType()
  {
    return IsInterface();
  }

  inline bool IsVoidType()
  {
    GCPtr<Symbol> sym = ResolveType();
    if (!sym) return false;
     
    if (sym->cls != sc_primtype)
      return false;

    return (sym->v.lty == lt_void);
  }

  inline bool IsAnonymous()
  {
    return (name[0] == '#');
  }

  std::string QualifiedName(char sep);

  unsigned long long CodedName();

  inline std::string ClassName()
  {
    return sc_names[cls];
  }
  inline std::string ClassPrintName()
  {
    return sc_printnames[cls];
  }
  inline bool IsScope() const
  {
    return sc_isScope[cls] ? true : false;
  }

  /* sorting helpers */
  static int
  CompareByName(GCPtr<Symbol>  const*s1, GCPtr<Symbol>  const*s2);

  static int
  CompareByQualifiedName(GCPtr<Symbol>  const*s1, GCPtr<Symbol>  const*s2);
};

std::string symname_join(const std::string& n1, const std::string& n2, char sep);

#endif /* SYMTAB_HXX */
