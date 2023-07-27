/* Automatically generated nanopb header */
/* Generated by nanopb-0.4.7-dev */

#ifndef PB_LANE_PB_H_INCLUDED
#define PB_LANE_PB_H_INCLUDED
#include <pb.h>

#if PB_PROTO_HEADER_VERSION != 40
#error Regenerate this file with the current version of nanopb generator.
#endif

/* Enum definitions */
typedef enum _LaneStatus {
    LaneStatus_FORWARD = 0,
    LaneStatus_BACKWARD = 1,
    LaneStatus_STOP = 2
} LaneStatus;

/* Struct definitions */
typedef struct _LaneColorConfig {
    uint32_t rgb;
} LaneColorConfig;

typedef struct _LaneLengthConfig {
    uint32_t total_length_cm;
    uint32_t line_length_cm;
    uint32_t active_length_cm;
    uint32_t line_leds_num; /* we could calculate the distance between each led with the above two */
} LaneLengthConfig;

typedef struct _LaneSetSpeed {
    /* in m/s */
    double speed;
} LaneSetSpeed;

typedef struct _LaneSetStatus {
    LaneStatus status;
} LaneSetStatus;

/* go through Control Characteristic via Notify/Read */
typedef struct _LaneState {
    LaneStatus status;
    /* / the distance between the start/end of the lane and the head of trail
/ if the status is FORWARD, the head is the distance from the start
/ if the status is BACKWARD, the head is the distance from the end */
    uint32_t head_cm;
} LaneState;

/* use to config the lane
 go through Config Characteristic */
typedef struct _LaneConfig {
    pb_size_t which_msg;
    union {
        LaneLengthConfig length_cfg;
        LaneColorConfig color_cfg;
    } msg;
} LaneConfig;

/* use to control the lane
 go through Control Characteristic vid Write. The lane would clear the characteristic after processing and
 replace the buffer with `LaneState` */
typedef struct _LaneControl {
    pb_size_t which_msg;
    union {
        LaneSetStatus set_status;
        LaneSetSpeed set_speed;
    } msg;
} LaneControl;


/* Helper constants for enums */
#define _LaneStatus_MIN LaneStatus_FORWARD
#define _LaneStatus_MAX LaneStatus_STOP
#define _LaneStatus_ARRAYSIZE ((LaneStatus)(LaneStatus_STOP+1))


#ifdef __cplusplus
extern "C" {
#endif

/* Initializer values for message structs */
#define LaneLengthConfig_init_default            {0, 0, 0, 0}
#define LaneColorConfig_init_default             {0}
#define LaneSetStatus_init_default               {_LaneStatus_MIN}
#define LaneSetSpeed_init_default                {0}
#define LaneState_init_default                   {_LaneStatus_MIN, 0}
#define LaneConfig_init_default                  {0, {LaneLengthConfig_init_default}}
#define LaneControl_init_default                 {0, {LaneSetStatus_init_default}}
#define LaneLengthConfig_init_zero               {0, 0, 0, 0}
#define LaneColorConfig_init_zero                {0}
#define LaneSetStatus_init_zero                  {_LaneStatus_MIN}
#define LaneSetSpeed_init_zero                   {0}
#define LaneState_init_zero                      {_LaneStatus_MIN, 0}
#define LaneConfig_init_zero                     {0, {LaneLengthConfig_init_zero}}
#define LaneControl_init_zero                    {0, {LaneSetStatus_init_zero}}

/* Field tags (for use in manual encoding/decoding) */
#define LaneColorConfig_rgb_tag                  1
#define LaneLengthConfig_total_length_cm_tag     1
#define LaneLengthConfig_line_length_cm_tag      2
#define LaneLengthConfig_active_length_cm_tag    3
#define LaneLengthConfig_line_leds_num_tag       4
#define LaneSetSpeed_speed_tag                   1
#define LaneSetStatus_status_tag                 1
#define LaneState_status_tag                     1
#define LaneState_head_cm_tag                    2
#define LaneConfig_length_cfg_tag                1
#define LaneConfig_color_cfg_tag                 2
#define LaneControl_set_status_tag               1
#define LaneControl_set_speed_tag                2

/* Struct field encoding specification for nanopb */
#define LaneLengthConfig_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, UINT32,   total_length_cm,   1) \
X(a, STATIC,   SINGULAR, UINT32,   line_length_cm,    2) \
X(a, STATIC,   SINGULAR, UINT32,   active_length_cm,   3) \
X(a, STATIC,   SINGULAR, UINT32,   line_leds_num,     4)
#define LaneLengthConfig_CALLBACK NULL
#define LaneLengthConfig_DEFAULT NULL

#define LaneColorConfig_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, UINT32,   rgb,               1)
#define LaneColorConfig_CALLBACK NULL
#define LaneColorConfig_DEFAULT NULL

#define LaneSetStatus_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, UENUM,    status,            1)
#define LaneSetStatus_CALLBACK NULL
#define LaneSetStatus_DEFAULT NULL

#define LaneSetSpeed_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, DOUBLE,   speed,             1)
#define LaneSetSpeed_CALLBACK NULL
#define LaneSetSpeed_DEFAULT NULL

#define LaneState_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, UENUM,    status,            1) \
X(a, STATIC,   SINGULAR, UINT32,   head_cm,           2)
#define LaneState_CALLBACK NULL
#define LaneState_DEFAULT NULL

#define LaneConfig_FIELDLIST(X, a) \
X(a, STATIC,   ONEOF,    MESSAGE,  (msg,length_cfg,msg.length_cfg),   1) \
X(a, STATIC,   ONEOF,    MESSAGE,  (msg,color_cfg,msg.color_cfg),   2)
#define LaneConfig_CALLBACK NULL
#define LaneConfig_DEFAULT NULL
#define LaneConfig_msg_length_cfg_MSGTYPE LaneLengthConfig
#define LaneConfig_msg_color_cfg_MSGTYPE LaneColorConfig

#define LaneControl_FIELDLIST(X, a) \
X(a, STATIC,   ONEOF,    MESSAGE,  (msg,set_status,msg.set_status),   1) \
X(a, STATIC,   ONEOF,    MESSAGE,  (msg,set_speed,msg.set_speed),   2)
#define LaneControl_CALLBACK NULL
#define LaneControl_DEFAULT NULL
#define LaneControl_msg_set_status_MSGTYPE LaneSetStatus
#define LaneControl_msg_set_speed_MSGTYPE LaneSetSpeed

extern const pb_msgdesc_t LaneLengthConfig_msg;
extern const pb_msgdesc_t LaneColorConfig_msg;
extern const pb_msgdesc_t LaneSetStatus_msg;
extern const pb_msgdesc_t LaneSetSpeed_msg;
extern const pb_msgdesc_t LaneState_msg;
extern const pb_msgdesc_t LaneConfig_msg;
extern const pb_msgdesc_t LaneControl_msg;

/* Defines for backwards compatibility with code written before nanopb-0.4.0 */
#define LaneLengthConfig_fields &LaneLengthConfig_msg
#define LaneColorConfig_fields &LaneColorConfig_msg
#define LaneSetStatus_fields &LaneSetStatus_msg
#define LaneSetSpeed_fields &LaneSetSpeed_msg
#define LaneState_fields &LaneState_msg
#define LaneConfig_fields &LaneConfig_msg
#define LaneControl_fields &LaneControl_msg

/* Maximum encoded size of messages (where known) */
#define LaneColorConfig_size                     6
#define LaneConfig_size                          26
#define LaneControl_size                         11
#define LaneLengthConfig_size                    24
#define LaneSetSpeed_size                        9
#define LaneSetStatus_size                       2
#define LaneState_size                           8

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
