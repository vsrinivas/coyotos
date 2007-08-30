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


#include <stdint.h>
#include <stdlib.h>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <assert.h>

#include "Value.hxx"
#include "AST.hxx"
#include <libsherpa/utf8.hxx>

Value TheUnitValue(Value::vk_unit);

Value::~Value()
{
}

uint32_t
StringValue::DecodeStringCharacter(const char *s, const char **next)
{
  unsigned long codePoint;

  if (*s != '\\') {
    const char *snext;
    codePoint = sherpa::utf8_decode(s, &snext);
    s = snext;
  }
  else {
    // Initial character was '\\', so this is either \<char> or it is
    // encoded somehow.  Next thing is *probably* a left curly
    // brace, but might be a normal char.
    s++;
    switch(*s++) {
    case '{':
      {
	// Okay, could be either a natural number or one of the special
	// character expansions.
	if (isdigit(*s)) {
	  char *snext;
	  unsigned long radix = strtoul(s, &snext, 10);

	  // Okay, this is pretty disgusting. If there actually WAS a
	  // radix prefix, then *snext is now 'r'. If not, then either
	  // /radix/ holds the actual numerical code point or something
	  // went horribly wrong.

	  if (*snext == 'r') {
	    s = snext+1;
	    codePoint = strtoul(s, &snext, radix);
	  }
	  else {
	    codePoint = radix;
	    s = snext;
	  }
	}
	else if (s[0] == 'U' && s[1] == '+') {
	  char *snext;
	  s += 2;
	  codePoint = strtoul(s, &snext, 16);
	  s = snext;
	}
	else if (strncmp(s, "space", 5) == 0) {
	  codePoint = ' ';
	  s+= 5;
	}
	else if (strncmp(s, "tab", 3) == 0) {
	  codePoint = '\t';
	  s+= 3;
	}
	else if (strncmp(s, "linefeed", 8) == 0) {
	  codePoint = '\n';
	  s+= 8;
	}
	else if (strncmp(s, "return", 6) == 0) {
	  codePoint = '\r';
	  s+= 6;
	}
	else if (strncmp(s, "backspace", 9) == 0) {
	  codePoint = '\b';
	  s+= 9;
	}
	else if (strncmp(s, "lbrace", 6) == 0) {
	  codePoint = '{';
	  s+= 6;
	}
	else if (strncmp(s, "rbrace", 6) == 0) {
	  codePoint = '}';
	  s+= 6;
	}
	else
	  assert(false);

	assert (*s == '}');

	s++;

	break;
      }
    case 'n':
      {
	codePoint = '\n';	// newline
	break;
      }
    case 'r':
      {
	codePoint = '\r';	// return
	break;
      }
    case 't':
      {
	codePoint = '\t';	// tab
	break;
      }
    case 'b':
      {
	codePoint = '\010';	// backspace
	break;
      }
    case 's':
      {
	codePoint = ' ';	// space
	break;
      }
    case 'f':
      {
	codePoint = '\f';	// formfeed
	break;
      }
    case '"':
      {
	codePoint = '"';	// double quote
	break;
      }
    case '\\':
      {
	codePoint = '\\';	// backslash
	break;
      }
    default:
      {
	assert(false);
	break;
      }
    }
  }

  if (next) *next = s;
  return codePoint;
}

// FIX: the current implementation assumes that the input stream
// consists of ASCII characters, which is most definitely a bug.
uint32_t
CharValue::DecodeRawCharacter(const char *s, const char **next)
{
  unsigned long codePoint;

  if (*s != '\\') {
    codePoint = *s++;
  }
  else {
    // Initial character was '\\', so this is either \<char> or it is
    // encoded somehow.  Next thing is *probably* a left curly
    // brace, but might be a normal char.
    s++;
    if (*s == '{') {
      s++;

      // Okay, could be either a natural number or one of the special
      // character expansions.
      if (isdigit(*s)) {
	char *snext;
	unsigned long radix = strtoul(s, &snext, 10);

	// Okay, this is pretty disgusting. If there actually WAS a
	// radix prefix, then *snext is now 'r'. If not, then either
	// /radix/ holds the actual numerical code point or something
	// went horribly wrong.

	if (*snext == 'r') {
	  s = snext+1;
	  codePoint = strtoul(s, &snext, radix);
	}
	else {
	  codePoint = radix;
	  s = snext;
	}
      }
      else if (s[0] == 'U' && s[1] == '+') {
	char *snext;
	s += 2;
	codePoint = strtoul(s, &snext, 16);
	s = snext;
      }
      else if (strncmp(s, "space", 5) == 0) {
	codePoint = ' ';
	s+= 5;
      }
      else if (strncmp(s, "tab", 3) == 0) {
	codePoint = '\t';
	s+= 3;
      }
      else if (strncmp(s, "linefeed", 8) == 0) {
	codePoint = '\n';
	s+= 8;
      }
      else if (strncmp(s, "return", 6) == 0) {
	codePoint = '\r';
	s+= 6;
      }
      else if (strncmp(s, "backspace", 9) == 0) {
	codePoint = '\b';
	s+= 9;
      }
      else if (strncmp(s, "lbrace", 6) == 0) {
	codePoint = '{';
	s+= 6;
      }
      else if (strncmp(s, "rbrace", 6) == 0) {
	codePoint = '}';
	s+= 6;
      }
      else
	assert(false);

      assert (*s == '}');

      s++;
    }
    else if (strcmp(s, "space") == 0) {
      codePoint = ' ';
      s+= 5;
    }
    else if (strcmp(s, "tab") == 0) {
      codePoint = '\t';
      s += 3;
    }
    else if (strcmp(s, "linefeed") == 0) {
      codePoint = '\n';
      s += 8;
    }
    else if (strcmp(s, "return") == 0) {
      codePoint = '\r';
      s += 6;
    }
    else if (strcmp(s, "lbrace") == 0) {
      codePoint = '{';
      s += 6;
    }
    else if (strcmp(s, "rbrace") == 0) {
      codePoint = '}';
      s += 6;
    }
    else
      codePoint = *s++;
  }

  if (next) *next = s;
  return codePoint;
}

uint32_t
CharValue::DecodeCharacter(const std::string& s)
{
  const char *str = s.c_str();

  assert(*str == '#');

  return DecodeRawCharacter(str + 1, 0);
}

IntValue::IntValue(const std::string& _value)
  : Value(vk_int)
{
  std::string litString = _value;
  base = 10;  			// until otherwise proven

  if (litString[0] == '0') {
    base = 8;
    if (litString[1] == 'x' || litString[1] == 'X')
      base = 16;
  }

  bn = sherpa::BigNum(litString, base);
}

FloatValue::FloatValue(const std::string& _value)
  :Value(vk_float)
{
  base = 10;  
  d = strtod(_value.c_str(), 0);

#if 0
  std::string litString = _value;
  std::string expString;
  std::string mantissaString;
 
  std::string::size_type epos = litString.find ('^');

  if(epos != std::string::npos) {
    expString = litString.substr(epos+1, litString.size());
    mantissaString = litString.substr(0, epos);
  }
  else {
    expString = "";
    mantissaString = litString;
  }
   
  /* Handle the mantissa */
  
  std::string::size_type pos = mantissaString.find ('r');
  if(pos != std::string::npos) {
    std::string rad;
    if(mantissaString[0] == '-') {
      rad = mantissaString.substr(1, pos); 
      mantissaString =
        "-" + mantissaString.substr(pos+1, mantissaString.size());
    }
    else {
      rad = mantissaString.substr(0, pos);
      mantissaString = mantissaString.substr(pos+1, mantissaString.size());
    }
    
    char *end;
    base = strtoul(rad.c_str(), &end, 10);
  }
   
  /* Handle the exponent part */
  std::string exponent = "";//ss.str();
  if(epos != std::string::npos) {
    size_t expBase = 10;
    
    std::string::size_type pos = expString.find ('r');
    if(pos != std::string::npos) {
      std::string rad;
      if(expString[0] == '-') {
	rad = expString.substr(1, pos); 
	expString = "-" + expString.substr(pos+1, expString.size());
      }
      else {
	rad = expString.substr(0, pos);
	expString = expString.substr(pos+1, expString.size());
      }
      
      char *end;
      expBase = strtoul(rad.c_str(), &end, 10);
    }
    mpz_t expmpz;
    mpz_init_set_str(expmpz, expString.c_str(), expBase);
    exponent = "@" + std::string(mpz_get_str (NULL, base, expmpz));
  }
  else 
    exponent = "@0";
  
  //std::cout << " Mantissa = " << mantissaString << " Exponent = " 
  //	    << exponent 
  //	    << " Base = " << ast->litBase;
  
  /* Finish off */
  mpf_init_set_str(d, (mantissaString + exponent).c_str(), base);
  //gmp_printf(" %Ff\n", ast->litValue.d);
#endif
}

#if 0
bool 
PrimFnValue::validateArgs(VecValue *pVec)
{
  size_t ndx = 0;
  const char *args = argDescrip;

  while (*args) {
    switch (*args) {
    case 'b':
      if ((*pVec)[ndx]->kind != vk_bool)
	return false;
      ndx++;
      args++;
      break;

    case 'c':
      if ((*pVec)[ndx]->kind != vk_char)
	return false;
      ndx++;
      args++;
      break;

    case 'i':
      if ((*pVec)[ndx]->kind != vk_int)
	return false;
      ndx++;
      args++;
      break;

    case 's':
      if ((*pVec)[ndx]->kind != vk_string)
	return false;
      ndx++;
      args++;
      break;

    case 'C':
      if ((*pVec)[ndx]->kind != vk_cap)
	return false;
      ndx++;
      args++;
      break;

    case 'N':			// numeric (int or float)
      if ((*pVec)[ndx]->kind != vk_int &&
	  (*pVec)[ndx]->kind != vk_float)
	return false;
      ndx++;
      args++;
      break;

    case '?':			// any one argument
      ndx++;
      args++;
      break;

    case '=':			// same type as some previous arg
      {
	char *nxtArg;
	args++;
	size_t pos = strtoul(args, &nxtArg, 10);
	if (nxtArg == args)	// no following number
	  return false;
	if ((*pVec)[ndx]->kind != (*pVec)[pos]->kind)
	  return false;
	args = nxtArg;
	ndx++;
	break;
      }
    default:
      std::cerr << "Unknown argument type " << *args << " at arg " 
		<< ndx << " of builtin function " << nm;
      exit(1);
    }
  }

  return true;
}
#endif

FnValue::FnValue(std::string id, 
		 const sherpa::GCPtr<Environment<Value> > _closedEnv,
		 const sherpa::GCPtr<AST>& _args, 
		 const sherpa::GCPtr<AST>& _body)
  : Value(vk_function)
{
  nm = id;			// purely for diagnostics
  args = _args;
  body = _body;
  closedEnv = _closedEnv;
}


sherpa::GCPtr<OpenValue> 
PrimMethodValue::dup()
{
  return new PrimMethodValue(*this);
}

sherpa::GCPtr<OpenValue> 
PrimFieldValue::dup()
{
  return new PrimFieldValue(*this);
}

void 
BoundValue::set(sherpa::GCPtr<Value> v)
{
  if (binding->flags & BF_CONSTANT)
    THROW(excpt::NoAccess, "LHS is constant");

  binding->val = v;
}

sherpa::GCPtr<Environment<Value> > 
getWrapperEnvironment();

sherpa::GCPtr<Environment<Value> > 
CapValue::getEnvironment()
{
  switch(cap.type) {
  case ct_Endpoint:
    return getEndpointEnvironment();
  case ct_GPT:
    return getGptEnvironment();
  case ct_Process:
    return getProcessEnvironment();
  default:
    return 0;
  }
}
