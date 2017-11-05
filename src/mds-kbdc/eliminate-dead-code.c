/**
 * mds — A micro-display server
 * Copyright © 2014, 2015, 2016, 2017  Mattias Andrée (maandree@kth.se)
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "eliminate-dead-code.h"

#include "include-stack.h"

#include <stdlib.h>
#include <errno.h>



/**
 * Tree type constant shortener
 */
#define C(TYPE) MDS_KBDC_TREE_TYPE_##TYPE

/**
 * Add an error with “included from here”-notes to the error list
 * 
 * @param  NODE:const mds_kbdc_tree_t*    The node the triggered the error
 * @param  PTR:size_t                     The number of “included from here”-notes
 * @param  SEVERITY:identifier            * in `MDS_KBDC_PARSE_ERROR_*` to indicate severity
 * @param  ...:const char*, ...           Error description format string and arguments
 * @scope  error:mds_kbdc_parse_error_t*  Variable where the new error will be stored
 */
#define NEW_ERROR(NODE, PTR, SEVERITY, ...)\
	NEW_ERROR_WITH_INCLUDES(NODE, PTR, SEVERITY, __VA_ARGS__)



/**
 * Variable whether the latest created error is stored
 */
static mds_kbdc_parse_error_t *error;

/**
 * The parameter of `eliminate_dead_code`
 */
static mds_kbdc_parsed_t *restrict result;

/**
 * 2: Eliminating because of a return-statement
 * 1: Eliminating because of a break- or continue-statement
 * 0: Not eliminating
 */
static int elimination_level = 0;



/**
 * Eliminate dead code in a subtree
 * 
 * @param   tree  The tree to reduce
 * @return        Zero on success, -1 on error
 */
static int eliminate_subtree(mds_kbdc_tree_t *restrict tree);



/**
 * Eliminate dead code in an include-statement
 * 
 * @param   tree  The tree to reduce
 * @return        Zero on success, -1 on error
 */
static int
eliminate_include(mds_kbdc_tree_include_t *restrict tree)
{
	void *data;
	int r;
	fail_if (mds_kbdc_include_stack_push(tree, &data));
	r = eliminate_subtree(tree->inner);
	mds_kbdc_include_stack_pop(data);
	fail_if (r);
	return 0;
fail:
	return -1;
}


/**
 * Eliminate dead code in an if-statement
 * 
 * @param   tree  The tree to reduce
 * @return        Zero on success, -1 on error
 */
static int
eliminate_if(mds_kbdc_tree_if_t *restrict tree)
{
	int elimination;
	fail_if (eliminate_subtree(tree->inner));
	elimination = elimination_level, elimination_level = 0;
	fail_if (eliminate_subtree(tree->otherwise));
	if (elimination > elimination_level)
		elimination = elimination_level;
	return 0;
fail:
	return -1;
}


/**
 * Eliminate dead code in a subtree
 * 
 * @param   tree  The tree to reduce
 * @return        Zero on success, -1 on error
 */
static int
eliminate_subtree(mds_kbdc_tree_t *restrict tree)
{
#define e(type) fail_if (eliminate_##type(&tree->type))
#define E(type) fail_if (eliminate_##type(&tree->type##_))
again:
	if (!tree)
		return 0;
  
	switch (tree->type) {
	case C(INCLUDE):
		e(include);
		break;
	case C(IF):
		E(if);
		break;
	case C(INFORMATION):
	case C(FUNCTION):
	case C(MACRO):
	case C(ASSUMPTION):
	case C(FOR):
		fail_if (eliminate_subtree(tree->information.inner));
		if (tree->type == C(FUNCTION) || tree->type == C(MACRO))
			elimination_level = 0;
		else if (tree->type == C(FOR) && elimination_level == 1)
			elimination_level = 0;
		break;
	case C(RETURN):
	case C(BREAK):
	case C(CONTINUE):
		elimination_level = tree->type == C(RETURN) ? 2 : 1;
		break;
	default:
		break;
	}

	if (tree->next && elimination_level) {
		NEW_ERROR(tree->next, includes_ptr, WARNING, "statement is unreachable");
		mds_kbdc_tree_free(tree->next), tree->next = NULL;
	}

	tree = tree->next;
	goto again;
fail:
	return -1;
#undef E
#undef e
}


/**
 * Eliminate and warn about dead code
 * 
 * @param   result_  `result` from `validate_tree`, will be updated
 * @return            -1 if an error occursed that cannot be stored in `result`, zero otherwise
 */
int
eliminate_dead_code(mds_kbdc_parsed_t *restrict result_)
{
	int r;
	mds_kbdc_include_stack_begin(result = result_);
	r = eliminate_subtree(result_->tree);
	mds_kbdc_include_stack_end();
	fail_if (r);
	return 0;
fail:
	return -1;
}



#undef NEW_ERROR
#undef C
