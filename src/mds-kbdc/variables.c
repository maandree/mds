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
#include "variables.h"

#include <libmdsserver/macros.h>

#include <stdlib.h>
#include <string.h>



/**
 * The state of a variable
 */
typedef struct variable {
	/**
	 * The current value of the variable
	 */
	mds_kbdc_tree_t *value;

	/**
	 * The previous version of variable,
	 * before it was shadowed
	 */
	struct variable *restrict previous;

	/**
	 * The original scope in which the current
	 * shadow of the variable was created
	 */
	size_t scope;

	/**
	 * The latest scope the in which the
	 * variable has been used in a for-loop,
	 * `~0' if never, or below `scope`
	 */
	size_t used_in_for;
} variable_t;



/**
 * Map (by index) of defined variables
 */
static variable_t **restrict variables = NULL;

/**
 * The size of `variables`
 */
static size_t variable_count = 0;

/**
 * The current scope, the number of
 * times the variable-stakc has been
 * pushed without being popped
 */
static size_t current_scope = 0;



/**
 * Destroy the variable storage
 */
void
variables_terminate(void)
{
	size_t i;
	variable_t *old;
	for (i = 0; i < variable_count; i++) {
		while (variables[i]) {
			old = variables[i];
			variables[i] = variables[i]->previous;
			mds_kbdc_tree_free(old->value);
			free(old);
		}
	}
	free(variables), variables = NULL;
	variable_count = current_scope = 0;
}


/**
 * Push the variable-stack, making it
 * possible to shadow all variables
 */
void
variables_stack_push(void)
{
	current_scope++;
}


/**
 * Undo the actions of `variables_stack_push`
 * and all additions to the variable storage
 * since it was last called (without a
 * corresponding call to this function)
 */
void
variables_stack_pop(void)
{
	size_t i;
	variable_t *old;
	for (i = 0; i < variable_count; i++) {
		if (variables[i] && variables[i]->scope == current_scope) {
			old = variables[i];
			variables[i] = variables[i]->previous;
			mds_kbdc_tree_free(old->value);
			free(old);
		}
	}
	current_scope--;
}


/**
 * Check whether a let will override a variable
 * rather the define or shadow it
 * 
 * @param   variable  The variable index
 * @return            Whether a let will override the variable
 */
int
variables_let_will_override(size_t variable)
{
	if (variable >= variable_count) return 0;
	if (!variables[variable])       return 0;
	return variables[variable]->scope == current_scope;
}


/**
 * Assign a value to a variable, and define or shadow it in the process
 * 
 * @param   variable  The variable index
 * @param   value     The variable's new value
 * @return            Zero on success, -1 on error
 */
int
variables_let(size_t variable, mds_kbdc_tree_t *restrict value)
{
	variable_t **new;
	variable_t *previous;

	/* Grow the table if necessary to fit the variable. */
	if (variable >= variable_count) {
		new = variables;
		fail_if (xrealloc(new, variable + 1, variable_t*));
		variables = new;
		memset(variables + variable_count, 0, (variable + 1 - variable_count) * sizeof(variable_t*));
		variable_count = variable + 1;
	}

	if (variables_let_will_override(variable)) {
		/* Override. */
		mds_kbdc_tree_free(variables[variable]->value);
		variables[variable]->value = value;
	} else {
		/* Shadow or define. */
		previous = variables[variable];
		if (xmalloc(variables[variable], 1, variable_t))
			fail_if (variables[variable] = previous, 1);
		variables[variable]->value = value;
		variables[variable]->previous = previous;
		variables[variable]->scope = current_scope;
		variables[variable]->used_in_for = (size_t)(~0LL);
	}
  
	return 0;
fail:
	return -1;
}


/**
 * Get the value currently assigned to a variable
 * 
 * The function cannot fail, `NULL` is however returned
 * if the variable is not defined
 * 
 * @param   variable  The variable index
 * @return            The variable's value, `NULL` if not defined
 */
mds_kbdc_tree_t *
variables_get(size_t variable)
{
	if (variable >= variable_count) return NULL;
	if (!variables[variable])       return NULL;
	return variables[variable]->value;
}


/**
 * Mark a variable as having been unsed in a for-loop in the current scope
 * 
 * @param   variable  The variable index, must already be defined
 * @return            Zero on success, -1 on error
 */
int
variables_was_used_in_for(size_t variable)
{
	variable_t *previous;

	/* Already marked. */
	if (variables[variable]->used_in_for == current_scope)
		return 0;

	/* Not marked. */
	if (variables[variable]->used_in_for == (size_t)(~0LL))
		return variables[variable]->used_in_for = current_scope, 0;

	/* Marked for another scope. */
	previous = variables[variable];
	if (xmalloc(variables[variable], 1, variable_t))
		fail_if (variables[variable] = previous, 1);
	variables[variable]->value = mds_kbdc_tree_dup(previous->value);
	fail_if (variables[variable]->value == NULL);
	variables[variable]->previous = previous;
	variables[variable]->scope = current_scope;
	variables[variable]->used_in_for = current_scope;

	return 0;
fail:
	return -1;
}


/**
 * Check whether a variable has been used in a for-loop in the current scope
 * 
 * @param   variable  The variable index, must already be defined
 * @return            Whether `variables_was_used_in_for` has been unused on the variable
 */
int
variables_has_been_used_in_for(size_t variable)
{
	return variables[variable]->used_in_for == current_scope;
}
