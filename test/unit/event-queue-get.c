/**
 *    Copyright (C) 2015 splone UG
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "sb-common.h"
#include "rpc/sb-rpc.h"
#include "helper-unix.h"

equeue *equeue_root;
equeue *equeue_child_one;
static api_event events;

void unit_event_queue_get(UNUSED(void **state))
{
  equeue_root = equeue_new(NULL);
  assert_non_null(equeue_root);
  equeue_child_one = equeue_new(equeue_root);
  assert_non_null(equeue_child_one);

  assert_int_not_equal(0, equeue_put(equeue_child_one, events));
  assert_false(TAILQ_EMPTY(&equeue_root->head));
  equeue_get(equeue_child_one);
  assert_true(TAILQ_EMPTY(&equeue_root->head));

  equeue_free(equeue_root);
  equeue_free(equeue_child_one);
}
