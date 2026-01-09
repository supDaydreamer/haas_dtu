#include <stdio.h>
#include <stdlib.h>
#include "bfmsg.h"
#include "liblfds711.h"

BFMSG_BOX_HANDLE bfmsg_box_init(size_t box_size)
{
	struct lfds711_queue_bmm_state *qbmms = malloc(sizeof(struct lfds711_queue_bmm_state));
	struct lfds711_queue_bmm_element *qbmme = malloc(sizeof(struct lfds711_queue_bmm_element) * box_size);
	lfds711_queue_bmm_init_valid_on_current_logical_core(qbmms, qbmme, box_size, NULL);

	return qbmms;
}

bool bfmsg_box_read(BFMSG_BOX_HANDLE box_handle , void **msg_type, void **msg_data)
{
	if (box_handle == NULL) return false;

	return lfds711_queue_bmm_dequeue(box_handle, msg_type, msg_data);
}

bool bfmsg_box_write(BFMSG_BOX_HANDLE box_handle, void *msg_type, void *msg_data, void **overwrite_msg_type, void **overwrite_msg_data)
{
	if (box_handle == NULL) return true;

	bool is_success = lfds711_queue_bmm_enqueue(box_handle, msg_type, msg_data);
	if (!is_success) {
		fprintf(stderr, "[%s] overwrite occurred, box_handle: %p\n, %p => %p", __FUNCTION__, box_handle, *overwrite_msg_type, *overwrite_msg_data);;
		lfds711_queue_bmm_dequeue(box_handle, overwrite_msg_type, overwrite_msg_data);
		lfds711_queue_bmm_enqueue(box_handle, msg_type, msg_data);
	}
	return is_success;
}

