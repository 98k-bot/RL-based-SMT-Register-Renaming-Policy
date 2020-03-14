/* expr.c - expression evaluator routines */

/* SimpleScalar(TM) Tool Suite
 * Copyright (C) 1994-2003 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.
 * All Rights Reserved. 
 * 
 * THIS IS A LEGAL DOCUMENT, BY USING SIMPLESCALAR,
 * YOU ARE AGREEING TO THESE TERMS AND CONDITIONS.
 * 
 * No portion of this work may be used by any commercial entity, or for any
 * commercial purpose, without the prior, written permission of SimpleScalar,
 * LLC (info@simplescalar.com). Nonprofit and noncommercial use is permitted
 * as described below.
 * 
 * 1. SimpleScalar is provided AS IS, with no warranty of any kind, express
 * or implied. The user of the program accepts full responsibility for the
 * application of the program and the use of any results.
 * 
 * 2. Nonprofit and noncommercial use is encouraged. SimpleScalar may be
 * downloaded, compiled, executed, copied, and modified solely for nonprofit,
 * educational, noncommercial research, and noncommercial scholarship
 * purposes provided that this notice in its entirety accompanies all copies.
 * Copies of the modified software can be delivered to persons who use it
 * solely for nonprofit, educational, noncommercial research, and
 * noncommercial scholarship purposes provided that this notice in its
 * entirety accompanies all copies.
 * 
 * 3. ALL COMMERCIAL USE, AND ALL USE BY FOR PROFIT ENTITIES, IS EXPRESSLY
 * PROHIBITED WITHOUT A LICENSE FROM SIMPLESCALAR, LLC (info@simplescalar.com).
 * 
 * 4. No nonprofit user may place any restrictions on the use of this software,
 * including as modified by the user, by any other authorized user.
 * 
 * 5. Noncommercial and nonprofit users may distribute copies of SimpleScalar
 * in compiled or executable form as set forth in Section 2, provided that
 * either: (A) it is accompanied by the corresponding machine-readable source
 * code, or (B) it is accompanied by a written offer, with no time limit, to
 * give anyone a machine-readable copy of the corresponding source code in
 * return for reimbursement of the cost of distribution. This written offer
 * must permit verbatim duplication by anyone, or (C) it is distributed by
 * someone who received only the executable form, and is accompanied by a
 * copy of the written offer of source code.
 * 
 * 6. SimpleScalar was developed by Todd M. Austin, Ph.D. The tool suite is
 * currently maintained by SimpleScalar LLC (info@simplescalar.com). US Mail:
 * 2395 Timbercrest Court, Ann Arbor, MI 48105.
 * 
 * Copyright (C) 1994-2003 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.
 */


#include<cstdio>
#include<cstdlib>
#include<ctype.h>
#include<errno.h>

#include "host.h"
#include "misc.h"
#include "eval.h"

#if defined(sparc) && !defined(__svr4__)
#define strtoul strtol
#endif /* sparc */

/* expression evaluation error, this must be a global */
eval_err_t eval_error = ERR_NOERR;

/* eval_err_t -> error description string map */
const char *eval_err_str[ERR_NUM] = {
	/* ERR_NOERR */		"!! no error!!",
	/* ERR_UPAREN */	"unmatched parenthesis",
	/* ERR_NOTERM */	"expression term is missing",
	/* ERR_DIV0 */		"divide by zero",
	/* ERR_BADCONST */	"badly formed constant",
	/* ERR_BADEXPR */	"badly formed expression",
	/* ERR_UNDEFVAR */	"variable is undefined",
	/* ERR_EXTRA */		"extra characters at end of expression"
};

/* *first* token character -> eval_token_t map */
eval_token_t tok_map[256];
int tok_map_initialized = FALSE;

/* builds the first token map */
void init_tok_map()
{
	for(int i=0; i<256; i++)
		tok_map[i] = tok_invalid;

	/* identifier characters */
	for(int i='a'; i<='z'; i++)
		tok_map[i] = tok_ident;
	for(int i='A'; i<='Z'; i++)
		tok_map[i] = tok_ident;
	tok_map[(int)'_'] = tok_ident;
	tok_map[(int)'$'] = tok_ident;

	/* numeric characters */
	for(int i='0'; i<='9'; i++)
		tok_map[i] = tok_const;
	tok_map[(int)'.'] = tok_const;

	/* operator characters */
	tok_map[(int)'+'] = tok_plus;
	tok_map[(int)'-'] = tok_minus;
	tok_map[(int)'*'] = tok_mult;
	tok_map[(int)'/'] = tok_div;
	tok_map[(int)'('] = tok_oparen;
	tok_map[(int)')'] = tok_cparen;

	/* whitespace characers */
	tok_map[(int)' '] = tok_whitespace;
	tok_map[(int)'\t'] = tok_whitespace;
}

/* get next token from the expression string */
eval_token_t			/* token parsed */
get_next_token(eval_state_t *es)	/* expression evaluator */
{
	int allow_hex;
	eval_token_t tok;
	char last_char;

	/* initialize the token map, if needed */
	if(!tok_map_initialized)
	{
		init_tok_map();
		tok_map_initialized = TRUE;
	}

	/* use the peek'ed token, if available, tok_buf should still be valid */
	if(es->peek_tok != tok_invalid)
	{
		tok = es->peek_tok;
		es->peek_tok = tok_invalid;
		return tok;
	}

	/* set up the token string space */
	char *ptok_buf = es->tok_buf;
	*ptok_buf = '\0';

	/* skip whitespace */
	while(*es->p && tok_map[(int)*es->p] == tok_whitespace)
		es->p++;

	/* end of token stream? */
	if(*es->p == '\0')
		return tok_eof;

	*ptok_buf++ = *es->p;
	tok = tok_map[(int)*es->p++];
	switch(tok)
	{
	case tok_ident:
		/* parse off next identifier */
		while(*es->p && (tok_map[(int)*es->p] == tok_ident || tok_map[(int)*es->p] == tok_const))
		{
			*ptok_buf++ = *es->p++;
		}
		 break;
	case tok_const:
		/* parse off next numeric literal */
		last_char = '\0';
		allow_hex = FALSE;
		while(*es->p &&
			(tok_map[(int)*es->p] == tok_const
			|| (*es->p == '-' && last_char == 'e')
			|| (*es->p == '+' && last_char == 'e')
			|| tolower(*es->p) == 'e'
			|| tolower(*es->p) == 'x'
			|| (tolower(*es->p) == 'a' && allow_hex)
			|| (tolower(*es->p) == 'b' && allow_hex)
			|| (tolower(*es->p) == 'c' && allow_hex)
			|| (tolower(*es->p) == 'd' && allow_hex)
			|| (tolower(*es->p) == 'e' && allow_hex)
			|| (tolower(*es->p) == 'f' && allow_hex)))
			{
				last_char = tolower(*es->p);
				if(*es->p == 'x' || *es->p == 'X')
					allow_hex = TRUE;
				*ptok_buf++ = *es->p++;
			}
		break;
	case tok_plus:
	case tok_minus:
	case tok_mult:
	case tok_div:
	case tok_oparen:
	case tok_cparen:
		/* just pass on the token */
		break;
	default:
		tok = tok_invalid;
		break;
	}

	/* terminate the token string buffer */
	*ptok_buf = '\0';

	return tok;
}

/* peek ahead at the next token from the expression stream, currently
   only the next token can be peek'ed at */
eval_token_t		 	/* next token in expression */
peek_next_token(eval_state_t *es)	/* expression evalutor */
{
	/* if there is no peek ahead token, get one */
	if(es->peek_tok == tok_invalid)
	{
		es->lastp = es->p;
		es->peek_tok = get_next_token(es);
	}

	/* return peek ahead token */
	return es->peek_tok;
}

/* forward declaration */
eval_value_t expr(eval_state_t *es);

/* default expression error value, eval_err is also set */
eval_value_t err_value = { et_int, { 0 } };

/* expression type strings */
const char *eval_type_str[et_NUM] = {
	/* et_int */		"int",
	/* et_uint */		"unsigned int",
	/* et_addr */		"md_addr_t",
	/* et_qword */	"qword_t",
	/* et_sqword */	"sqword_t",
	/* et_float */	"float",
	/* et_double */	"double",
	/* et_symbol */	"symbol"
};

/* determine necessary arithmetic conversion on T1 <op> T2 */
eval_type_t			/* type of expression result */
result_type(eval_type_t t1,	/* left operand type */
	eval_type_t t2)		/* right operand type */
{
  /* sanity check, symbols should not show up in arithmetic exprs */
  if (t1 == et_symbol || t2 == et_symbol)
    panic("symbol used in expression");

  /* using C rules, i.e., A6.5 */
  if (t1 == et_double || t2 == et_double)
    return et_double;
  else if (t1 == et_float || t2 == et_float)
    return et_float;
  else if (t1 == et_qword || t2 == et_qword)
    return et_qword;
  else if (t1 == et_sqword || t2 == et_sqword)
    return et_sqword;
  else if (t1 == et_addr || t2 == et_addr)
    return et_addr;
  else if (t1 == et_uint || t2 == et_uint)
    return et_uint;
  else
    return et_int;
}

/*
 * arithmetic intrinsics operations, used during expression evaluation
 */

/* compute <val1> + <val2> */
eval_value_t f_add(eval_value_t val1, eval_value_t val2)
{
  eval_value_t val;

  /* symbols are not allowed in arithmetic expressions */
  if (val1.type == et_symbol || val2.type == et_symbol)
    {
      eval_error = ERR_BADEXPR;
      return err_value;
    }

  /* get result type, and perform operation in that type */
  eval_type_t et = result_type(val1.type, val2.type);
  switch (et)
    {
    case et_double:
      val.type = et_double;
      val.value.as_double = eval_as<double>(val1) + eval_as<double>(val2);
      break;
    case et_float:
      val.type = et_float;
      val.value.as_float = eval_as<float>(val1) + eval_as<float>(val2);
      break;
    case et_qword:
      val.type = et_qword;
      val.value.as_qword = eval_as<qword_t>(val1) + eval_as<qword_t>(val2);
      break;
    case et_sqword:
      val.type = et_sqword;
      val.value.as_sqword = eval_as<sqword_t>(val1) + eval_as<sqword_t>(val2);
      break;
    case et_addr:
      val.type = et_addr;
      val.value.as_addr = eval_as<md_addr_t>(val1) + eval_as<md_addr_t>(val2);
      break;
    case et_uint:
      val.type = et_uint;
      val.value.as_uint = eval_as<unsigned int>(val1) + eval_as<unsigned int>(val2);
      break;
    case et_int:
      val.type = et_int;
      val.value.as_int = eval_as<int>(val1) + eval_as<int>(val2);
      break;
    default:
      panic("bogus expression type");
    }

  return val;
}

/* compute <val1> - <val2> */
eval_value_t f_sub(eval_value_t val1, eval_value_t val2)
{
  eval_value_t val;

  /* symbols are not allowed in arithmetic expressions */
  if (val1.type == et_symbol || val2.type == et_symbol)
    {
      eval_error = ERR_BADEXPR;
      return err_value;
    }

  /* get result type, and perform operation in that type */
  eval_type_t et = result_type(val1.type, val2.type);
  switch (et)
    {
    case et_double:
      val.type = et_double;
      val.value.as_double = eval_as<double>(val1) - eval_as<double>(val2);
      break;
    case et_float:
      val.type = et_float;
      val.value.as_float = eval_as<float>(val1) - eval_as<float>(val2);
      break;
    case et_qword:
      val.type = et_qword;
      val.value.as_qword = eval_as<qword_t>(val1) - eval_as<qword_t>(val2);
      break;
    case et_sqword:
      val.type = et_sqword;
      val.value.as_sqword = eval_as<sqword_t>(val1) - eval_as<sqword_t>(val2);
      break;
    case et_addr:
      val.type = et_addr;
      val.value.as_addr = eval_as<md_addr_t>(val1) - eval_as<md_addr_t>(val2);
      break;
    case et_uint:
      val.type = et_uint;
      val.value.as_uint = eval_as<unsigned int>(val1) - eval_as<unsigned int>(val2);
      break;
    case et_int:
      val.type = et_int;
      val.value.as_int = eval_as<int>(val1) - eval_as<int>(val2);
      break;
    default:
      panic("bogus expression type");
    }

  return val;
}

/* compute <val1> * <val2> */
eval_value_t f_mult(eval_value_t val1, eval_value_t val2)
{
  eval_value_t val;

  /* symbols are not allowed in arithmetic expressions */
  if (val1.type == et_symbol || val2.type == et_symbol)
    {
      eval_error = ERR_BADEXPR;
      return err_value;
    }

  /* get result type, and perform operation in that type */
  eval_type_t et = result_type(val1.type, val2.type);
  switch (et)
    {
    case et_double:
      val.type = et_double;
      val.value.as_double = eval_as<double>(val1) * eval_as<double>(val2);
      break;
    case et_float:
      val.type = et_float;
      val.value.as_float = eval_as<float>(val1) * eval_as<float>(val2);
      break;
    case et_qword:
      val.type = et_qword;
      val.value.as_qword = eval_as<qword_t>(val1) * eval_as<qword_t>(val2);
      break;
    case et_sqword:
      val.type = et_sqword;
      val.value.as_sqword = eval_as<sqword_t>(val1) * eval_as<sqword_t>(val2);
      break;
    case et_addr:
      val.type = et_addr;
      val.value.as_addr = eval_as<md_addr_t>(val1) * eval_as<md_addr_t>(val2);
      break;
    case et_uint:
      val.type = et_uint;
      val.value.as_uint = eval_as<unsigned int>(val1) * eval_as<unsigned int>(val2);
      break;
    case et_int:
      val.type = et_int;
      val.value.as_int = eval_as<int>(val1) * eval_as<int>(val2);
      break;
    default:
      panic("bogus expression type");
    }

  return val;
}

/* compute <val1> / <val2> */
eval_value_t f_div(eval_value_t val1, eval_value_t val2)
{
  eval_value_t val;

  /* symbols are not allowed in arithmetic expressions */
  if (val1.type == et_symbol || val2.type == et_symbol)
    {
      eval_error = ERR_BADEXPR;
      return err_value;
    }

  /* get result type, and perform operation in that type */
  eval_type_t et = result_type(val1.type, val2.type);
  switch (et)
    {
    case et_double:
      val.type = et_double;
      val.value.as_double = eval_as<double>(val1) / eval_as<double>(val2);
      break;
    case et_float:
      val.type = et_float;
      val.value.as_float = eval_as<float>(val1) / eval_as<float>(val2);
      break;
    case et_qword:
      val.type = et_qword;
      val.value.as_qword = eval_as<qword_t>(val1) / eval_as<qword_t>(val2);
      break;
    case et_sqword:
      val.type = et_sqword;
      val.value.as_sqword = eval_as<sqword_t>(val1) / eval_as<sqword_t>(val2);
      break;
    case et_addr:
      val.type = et_addr;
      val.value.as_addr = eval_as<md_addr_t>(val1) / eval_as<md_addr_t>(val2);
      break;
    case et_uint:
      val.type = et_uint;
      val.value.as_uint = eval_as<unsigned int>(val1) / eval_as<unsigned int>(val2);
      break;
    case et_int:
      val.type = et_int;
      val.value.as_int = eval_as<int>(val1) / eval_as<int>(val2);
      break;
    default:
      panic("bogus expression type");
    }

  return val;
}

/* compute - <val1> */
eval_value_t f_neg(eval_value_t val1)
{
	eval_value_t val;

  /* symbols are not allowed in arithmetic expressions */
  if (val1.type == et_symbol)
    {
      eval_error = ERR_BADEXPR;
      return err_value;
    }

  /* result type is the same as the operand type */
  switch (val1.type)
    {
    case et_double:
      val.type = et_double;
      val.value.as_double = - val1.value.as_double;
      break;
    case et_float:
      val.type = et_float;
      val.value.as_float = - val1.value.as_float;
      break;
    case et_qword:
      val.type = et_sqword;
      val.value.as_qword = - (sqword_t)val1.value.as_qword;
      break;
    case et_sqword:
      val.type = et_sqword;
      val.value.as_sqword = - val1.value.as_sqword;
      break;
    case et_addr:
      val.type = et_addr;
      val.value.as_addr = - val1.value.as_addr;
      break;
    case et_uint:
      if ((unsigned int)val1.value.as_uint > 2147483648U)
	{
	  /* promote type */
	  val.type = et_sqword;
	  val.value.as_sqword = - ((sqword_t)val1.value.as_uint);
	}
      else
	{
	  /* don't promote type */
	  val.type = et_int;
	  val.value.as_int = - ((int)val1.value.as_uint);
	}
      break;
    case et_int:
      if ((unsigned int)val1.value.as_int == 0x80000000U)
	{
	  /* promote type */
	  val.type = et_uint;
	  val.value.as_uint = 2147483648U;
	}
      else
	{
	  /* don't promote type */
	  val.type = et_int;
	  val.value.as_int = - val1.value.as_int;
	}
      break;
    default:
      panic("bogus expression type");
    }

  return val;
}

/* compute val1 == 0 */
int f_eq_zero(eval_value_t val1)
{
  int val;

  /* symbols are not allowed in arithmetic expressions */
  if (val1.type == et_symbol)
    {
      eval_error = ERR_BADEXPR;
      return FALSE;
    }

  switch (val1.type)
    {
    case et_double:
      val = val1.value.as_double == 0.0;
      break;
    case et_float:
      val = val1.value.as_float == 0.0;
      break;
    case et_qword:
      val = val1.value.as_qword == 0;
      break;
    case et_sqword:
      val = val1.value.as_sqword == 0;
      break;
    case et_addr:
      val = val1.value.as_addr == 0;
      break;
    case et_uint:
      val = val1.value.as_uint == 0;
      break;
    case et_int:
      val = val1.value.as_int == 0;
      break;
    default:
      panic("bogus expression type");
    }

  return val;
}

/* evaluate the value of the numeric literal constant in ES->TOK_BUF,
   eval_err is set to a value other than ERR_NOERR if the constant cannot
   be parsed and converted to an expression value */
eval_value_t		/* value of the literal constant */
constant(eval_state_t *es)	/* expression evaluator */
{
  eval_value_t val;
  char *endp;

  /*
   * attempt multiple conversions, from least to most precise, using
   * the value returned when the conversion is successful
   */

  /* attempt integer conversion */
  errno = 0;
  int int_val = strtol(es->tok_buf, &endp, /* parse base */0);
  if (!errno && !*endp)
    {
      /* good conversion */
      val.type = et_int;
      val.value.as_int = int_val;
      return val;
    }

  /* else, not an integer, attempt unsigned int conversion */
  errno = 0;
  unsigned int uint_val = strtoul(es->tok_buf, &endp, /* parse base */0);
  if (!errno && !*endp)
    {
      /* good conversion */
      val.type = et_uint;
      val.value.as_uint = uint_val;
      return val;
    }

  /* else, not an int/uint, attempt sqword_t conversion */
  errno = 0;
  sqword_t sqword_val = myatosq(es->tok_buf, &endp, /* parse base */0);
  if (!errno && !*endp)
    {
      /* good conversion */
      val.type = et_sqword;
      val.value.as_sqword = sqword_val;
      return val;
    }

  /* else, not an sqword_t, attempt qword_t conversion */
  errno = 0;
  qword_t qword_val = myatoq(es->tok_buf, &endp, /* parse base */0);
  if (!errno && !*endp)
    {
      /* good conversion */
      val.type = et_qword;
      val.value.as_qword = qword_val;
      return val;
    }

  /* else, not any type of integer, attempt double conversion (NOTE: no
     reliable float conversion is available on all machines) */
  errno = 0;
  double double_val = strtod(es->tok_buf, &endp);
  if (!errno && !*endp)
    {
      /* good conversion */
      val.type = et_double;
      val.value.as_double = double_val;
      return val;
    }

  /* else, not a double value, therefore, could not convert constant,
     declare an error */
  eval_error = ERR_BADCONST;
  return err_value;
}

/* evaluate an expression factor, eval_err will indicate it any
   expression evaluation occurs */
eval_value_t		/* value of factor */
factor(eval_state_t *es)		/* expression evaluator */
{
  eval_value_t val;

  eval_token_t tok = peek_next_token(es);
  switch (tok)
    {
    case tok_oparen:
      (void)get_next_token(es);
      val = expr(es);
      if (eval_error)
	return err_value;

      tok = peek_next_token(es);
      if (tok != tok_cparen)
	{
	  eval_error = ERR_UPAREN;
	  return err_value;
	}
      (void)get_next_token(es);
      break;

    case tok_minus:
      /* negation operator */
      (void)get_next_token(es);
      val = factor(es);
      if (eval_error)
	return err_value;
      val = f_neg(val);
      break;

    case tok_ident:
      (void)get_next_token(es);
      /* evaluate the identifier in TOK_BUF */
      val = es->f_eval_ident(es);
      if (eval_error)
	return err_value;
      break;

    case tok_const:
      (void)get_next_token(es);
      val = constant(es);
      if (eval_error)
	return err_value;
      break;

    default:
      eval_error = ERR_NOTERM;
      return err_value;
    }

  return val;
}

/* evaluate an expression term, eval_err will indicate it any
   expression evaluation occurs */
eval_value_t		/* value to expression term */
term(eval_state_t *es)		/* expression evaluator */
{
  eval_value_t val, val1;

  val = factor(es);
  if (eval_error)
    return err_value;

  eval_token_t tok = peek_next_token(es);
  switch (tok)
    {
    case tok_mult:
      (void)get_next_token(es);
      val = f_mult(val, term(es));
      if (eval_error)
	return err_value;
      break;

    case tok_div:
      (void)get_next_token(es);
      val1 = term(es);
      if (eval_error)
	return err_value;
      if (f_eq_zero(val1))
	{
	  eval_error = ERR_DIV0;
	  return err_value;
	}
      val = f_div(val, val1);
      break;

    default:;
    }

  return val;
}

/* evaluate an expression, eval_err will indicate it any expression
   evaluation occurs */
eval_value_t		/* value of the expression */
expr(eval_state_t *es)		/* expression evaluator */
{
  eval_value_t val = term(es);
  if (eval_error)
    return err_value;

  eval_token_t tok = peek_next_token(es);
  switch (tok)
    {
    case tok_plus:
      (void)get_next_token(es);
      val = f_add(val, expr(es));
      if (eval_error)
	return err_value;
      break;

    case tok_minus:
      (void)get_next_token(es);
      val = f_sub(val, expr(es));
      if (eval_error)
	return err_value;
      break;

    default:;
    }

  return val;
}

eval_state_t::eval_state_t(eval_ident_t f_eval_ident,	/* user ident evaluator */
	void *user_ptr,					/* user ptr passed to ident fn */
	mem_t* mem)
: f_eval_ident(f_eval_ident), user_ptr(user_ptr), mem(mem)
{}

/* evaluate an expression, if an error occurs during evaluation, the
   global variable eval_error will be set to a value other than ERR_NOERR */
eval_value_t			/* value of the expression */
eval_expr(eval_state_t *es,	/* expression evaluator */
	  char *p,			/* ptr to expression string */
	  char **endp)			/* returns ptr to 1st unused char */
{
	//initialize the evaluator state
	eval_error = ERR_NOERR;
	es->p = p;
	*es->tok_buf = '\0';
	es->peek_tok = tok_invalid;

	//evaluate the expression
	eval_value_t val = expr(es);

	//return a pointer to the first character not used in the expression
	if(endp)
	{
		if(es->peek_tok != tok_invalid)
		{
			//did not consume peek'ed token, so return last p
			*endp = es->lastp;
		}
		else
		{
			*endp = es->p;
		}
	}
	return val;
}

/* print an expression value */
void eval_print(FILE *stream,		/* output stream */
	eval_value_t val)	/* expression value to print */
{
  switch (val.type)
    {
    case et_double:
      fprintf(stream, "%f [double]", val.value.as_double);
      break;
    case et_float:
      fprintf(stream, "%f [float]", (double)val.value.as_float);
      break;
    case et_qword:
      myfprintf(stream, "%lu [qword_t]", val.value.as_qword);
      break;
    case et_sqword:
      myfprintf(stream, "%ld [sqword_t]", val.value.as_sqword);
      break;
    case et_addr:
      myfprintf(stream, "0x%p [md_addr_t]", val.value.as_addr);
      break;
    case et_uint:
      fprintf(stream, "%u [uint]", val.value.as_uint);
      break;
    case et_int:
      fprintf(stream, "%d [int]", val.value.as_int);
      break;
    case et_symbol:
      fprintf(stream, "\"%s\" [symbol]", val.value.as_symbol);
      break;
    default:
      panic("bogus expression type");
    }
}


#ifdef TEST

eval_value_t an_int;
eval_value_t a_uint;
eval_value_t a_float;
eval_value_t a_double;
eval_value_t a_symbol;

class sym_map_t {
public:
  char *symbol;
  eval_value_t *value;
};

sym_map_t sym_map[] = {
  { "an_int", &an_int },
  { "a_uint", &a_uint },
  { "a_float", &a_float },
  { "a_double", &a_double },
  { "a_symbol", &a_symbol },
  { NULL, NULL },
};

eval_value_t
my_eval_ident(eval_state_t *es)
{
  sym_map_t *sym;

  for (sym=sym_map; sym->symbol != NULL; sym++)
    {
      if (!strcmp(sym->symbol, es->tok_buf))
	return *sym->value;
    }

  eval_error = ERR_UNDEFVAR;
  return err_value;
}

void
main(void)
{
  eval_state_t *es;

  /* set up test variables */
  an_int.type = et_int; an_int.value.as_int = 1;
  a_uint.type = et_uint; a_uint.value.as_uint = 2;
  a_float.type = et_float; a_float.value.as_float = 3.0f;
  a_double.type = et_double; a_double.value.as_double = 4.0;
  a_symbol.type = et_symbol; a_symbol.value.as_symbol = "testing 1 2 3...";

  /* instantiate an evaluator */
  es = new eval_state_t(my_eval_ident, NULL);

  while (1)
    {
      eval_value_t val;
      char expr_buf[1024];

      fgets(expr_buf, 1024, stdin);

      /* chop */
      if (expr_buf[strlen(expr_buf)-1] == '\n')
	expr_buf[strlen(expr_buf)-1] = '\0';

      if (expr_buf[0] == '\0')
	exit(0);

      val = eval_expr(es, expr_buf, NULL);
      if (eval_error)
	fprintf(stdout, "eval error: %s\n", eval_err_str[eval_error]);
      else
	{
	  fprintf(stdout, "%s == ", expr_buf);
	  eval_print(stdout, val);
	  fprintf(stdout, "\n");
	}
    }
}

#endif /* TEST */
