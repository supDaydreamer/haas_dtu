#include <stdbool.h>

#ifndef __BFMSG_H__
#define __BFMSG_H__

typedef struct lfds711_queue_bmm_state *BFMSG_BOX_HANDLE;

BFMSG_BOX_HANDLE bfmsg_box_init(size_t box_size);
bool bfmsg_box_read(BFMSG_BOX_HANDLE box_handle, void **msg_type, void **msg_data);
bool bfmsg_box_write(BFMSG_BOX_HANDLE box_handle, void *msg_type, void *msg_data, void **overwrite_msg_type, void **overwrite_msg_data);

#endif
