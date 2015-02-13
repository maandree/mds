/**
 * mds — A micro-display server
 * Copyright © 2014, 2015  Mattias Andrée (maandree@member.fsf.org)
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
#include "globals.h"


volatile sig_atomic_t running = 1;

size_t running_slaves = 0;
pthread_mutex_t slave_mutex;
pthread_cond_t slave_cond;
fd_table_t client_map;
linked_list_t client_list;
uint64_t next_client_id = 1;
uint64_t next_modify_id = 1;
pthread_mutex_t modify_mutex;
pthread_cond_t modify_cond;
hash_table_t modify_map;

