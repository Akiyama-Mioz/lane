/* Automatically generated nanopb header */
/* Generated by nanopb-0.4.7-dev */

#ifndef PB_TRACK_OPTION_PB_H_INCLUDED
#define PB_TRACK_OPTION_PB_H_INCLUDED
#include <pb.h>

#if PB_PROTO_HEADER_VERSION != 40
#error Regenerate this file with the current version of nanopb generator.
#endif

/* Struct definitions */
typedef struct _CircleLength {
    float meter;
} CircleLength;

typedef struct _CountLed {
    uint32_t num;
} CountLed;

typedef struct _MaxLed {
    uint32_t num;
} MaxLed;

typedef struct _TrackOptions {
    pb_size_t which_options;
    union {
        MaxLed max_led;
        CountLed count_led;
        CircleLength circle;
    } options;
} TrackOptions;


#ifdef __cplusplus
extern "C" {
#endif

/* Initializer values for message structs */
#define MaxLed_init_default                      {0}
#define CountLed_init_default                    {0}
#define CircleLength_init_default                {0}
#define TrackOptions_init_default                {0, {MaxLed_init_default}}
#define MaxLed_init_zero                         {0}
#define CountLed_init_zero                       {0}
#define CircleLength_init_zero                   {0}
#define TrackOptions_init_zero                   {0, {MaxLed_init_zero}}

/* Field tags (for use in manual encoding/decoding) */
#define CircleLength_meter_tag                   1
#define CountLed_num_tag                         1
#define MaxLed_num_tag                           1
#define TrackOptions_max_led_tag                 1
#define TrackOptions_count_led_tag               2
#define TrackOptions_circle_tag                  3

/* Struct field encoding specification for nanopb */
#define MaxLed_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, UINT32,   num,               1)
#define MaxLed_CALLBACK NULL
#define MaxLed_DEFAULT NULL

#define CountLed_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, UINT32,   num,               1)
#define CountLed_CALLBACK NULL
#define CountLed_DEFAULT NULL

#define CircleLength_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, FLOAT,    meter,             1)
#define CircleLength_CALLBACK NULL
#define CircleLength_DEFAULT NULL

#define TrackOptions_FIELDLIST(X, a) \
X(a, STATIC,   ONEOF,    MESSAGE,  (options,max_led,options.max_led),   1) \
X(a, STATIC,   ONEOF,    MESSAGE,  (options,count_led,options.count_led),   2) \
X(a, STATIC,   ONEOF,    MESSAGE,  (options,circle,options.circle),   3)
#define TrackOptions_CALLBACK NULL
#define TrackOptions_DEFAULT NULL
#define TrackOptions_options_max_led_MSGTYPE MaxLed
#define TrackOptions_options_count_led_MSGTYPE CountLed
#define TrackOptions_options_circle_MSGTYPE CircleLength

extern const pb_msgdesc_t MaxLed_msg;
extern const pb_msgdesc_t CountLed_msg;
extern const pb_msgdesc_t CircleLength_msg;
extern const pb_msgdesc_t TrackOptions_msg;

/* Defines for backwards compatibility with code written before nanopb-0.4.0 */
#define MaxLed_fields &MaxLed_msg
#define CountLed_fields &CountLed_msg
#define CircleLength_fields &CircleLength_msg
#define TrackOptions_fields &TrackOptions_msg

/* Maximum encoded size of messages (where known) */
#define CircleLength_size                        5
#define CountLed_size                            6
#define MaxLed_size                              6
#define TrackOptions_size                        8

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
