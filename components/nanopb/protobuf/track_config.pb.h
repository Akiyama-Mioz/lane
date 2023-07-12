/* Automatically generated nanopb header */
/* Generated by nanopb-0.4.7-dev */

#ifndef PB_TRACK_CONFIG_PB_H_INCLUDED
#define PB_TRACK_CONFIG_PB_H_INCLUDED
#include <pb.h>

#if PB_PROTO_HEADER_VERSION != 40
#error Regenerate this file with the current version of nanopb generator.
#endif

/* Enum definitions */
typedef enum _Command {
    Command_ADD = 0, /* push to the vector */
    Command_RESET = 1 /* empty the vector then push */
} Command;

typedef enum _TrackStatus {
    TrackStatus_STOP = 0,
    TrackStatus_READY = 1,
    TrackStatus_RUN = 2
} TrackStatus;

/* Struct definitions */
typedef struct _TrackStates {
    pb_callback_t states;
} TrackStates;

/* should be from 0 to 255 */
typedef struct _Color {
    int32_t red;
    int32_t green;
    int32_t blue;
} Color;

typedef struct _TrackState {
    int32_t id;
    float shift;
    float speed;
} TrackState;

typedef struct _TrackStatusMsg {
    TrackStatus status;
} TrackStatusMsg;

typedef struct _TupleIntFloat {
    int32_t dist;
    float speed;
} TupleIntFloat;

typedef struct _TrackConfig {
    Command command;
    int32_t id;
    pb_callback_t lst;
    bool has_color;
    Color color;
} TrackConfig;


/* Helper constants for enums */
#define _Command_MIN Command_ADD
#define _Command_MAX Command_RESET
#define _Command_ARRAYSIZE ((Command)(Command_RESET+1))

#define _TrackStatus_MIN TrackStatus_STOP
#define _TrackStatus_MAX TrackStatus_RUN
#define _TrackStatus_ARRAYSIZE ((TrackStatus)(TrackStatus_RUN+1))


#ifdef __cplusplus
extern "C" {
#endif

/* Initializer values for message structs */
#define Color_init_default                       {0, 0, 0}
#define TupleIntFloat_init_default               {0, 0}
#define TrackConfig_init_default                 {_Command_MIN, 0, {{NULL}, NULL}, false, Color_init_default}
#define TrackState_init_default                  {0, 0, 0}
#define TrackStates_init_default                 {{{NULL}, NULL}}
#define TrackStatusMsg_init_default              {_TrackStatus_MIN}
#define Color_init_zero                          {0, 0, 0}
#define TupleIntFloat_init_zero                  {0, 0}
#define TrackConfig_init_zero                    {_Command_MIN, 0, {{NULL}, NULL}, false, Color_init_zero}
#define TrackState_init_zero                     {0, 0, 0}
#define TrackStates_init_zero                    {{{NULL}, NULL}}
#define TrackStatusMsg_init_zero                 {_TrackStatus_MIN}

/* Field tags (for use in manual encoding/decoding) */
#define TrackStates_states_tag                   1
#define Color_red_tag                            1
#define Color_green_tag                          2
#define Color_blue_tag                           3
#define TrackState_id_tag                        1
#define TrackState_shift_tag                     2
#define TrackState_speed_tag                     3
#define TrackStatusMsg_status_tag                1
#define TupleIntFloat_dist_tag                   1
#define TupleIntFloat_speed_tag                  2
#define TrackConfig_command_tag                  1
#define TrackConfig_id_tag                       2
#define TrackConfig_lst_tag                      3
#define TrackConfig_color_tag                    4

/* Struct field encoding specification for nanopb */
#define Color_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, INT32,    red,               1) \
X(a, STATIC,   SINGULAR, INT32,    green,             2) \
X(a, STATIC,   SINGULAR, INT32,    blue,              3)
#define Color_CALLBACK NULL
#define Color_DEFAULT NULL

#define TupleIntFloat_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, INT32,    dist,              1) \
X(a, STATIC,   SINGULAR, FLOAT,    speed,             2)
#define TupleIntFloat_CALLBACK NULL
#define TupleIntFloat_DEFAULT NULL

#define TrackConfig_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, UENUM,    command,           1) \
X(a, STATIC,   SINGULAR, INT32,    id,                2) \
X(a, CALLBACK, REPEATED, MESSAGE,  lst,               3) \
X(a, STATIC,   OPTIONAL, MESSAGE,  color,             4)
#define TrackConfig_CALLBACK pb_default_field_callback
#define TrackConfig_DEFAULT NULL
#define TrackConfig_lst_MSGTYPE TupleIntFloat
#define TrackConfig_color_MSGTYPE Color

#define TrackState_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, INT32,    id,                1) \
X(a, STATIC,   SINGULAR, FLOAT,    shift,             2) \
X(a, STATIC,   SINGULAR, FLOAT,    speed,             3)
#define TrackState_CALLBACK NULL
#define TrackState_DEFAULT NULL

#define TrackStates_FIELDLIST(X, a) \
X(a, CALLBACK, REPEATED, MESSAGE,  states,            1)
#define TrackStates_CALLBACK pb_default_field_callback
#define TrackStates_DEFAULT NULL
#define TrackStates_states_MSGTYPE TrackState

#define TrackStatusMsg_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, UENUM,    status,            1)
#define TrackStatusMsg_CALLBACK NULL
#define TrackStatusMsg_DEFAULT NULL

extern const pb_msgdesc_t Color_msg;
extern const pb_msgdesc_t TupleIntFloat_msg;
extern const pb_msgdesc_t TrackConfig_msg;
extern const pb_msgdesc_t TrackState_msg;
extern const pb_msgdesc_t TrackStates_msg;
extern const pb_msgdesc_t TrackStatusMsg_msg;

/* Defines for backwards compatibility with code written before nanopb-0.4.0 */
#define Color_fields &Color_msg
#define TupleIntFloat_fields &TupleIntFloat_msg
#define TrackConfig_fields &TrackConfig_msg
#define TrackState_fields &TrackState_msg
#define TrackStates_fields &TrackStates_msg
#define TrackStatusMsg_fields &TrackStatusMsg_msg

/* Maximum encoded size of messages (where known) */
/* TrackConfig_size depends on runtime parameters */
/* TrackStates_size depends on runtime parameters */
#define Color_size                               33
#define TrackState_size                          21
#define TrackStatusMsg_size                      2
#define TupleIntFloat_size                       16

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif