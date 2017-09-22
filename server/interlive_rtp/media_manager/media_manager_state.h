#pragma once

namespace media_manager
{
    enum StatusCode
    {
        STATUS_SUCCESS = 0,
        STATUS_PARTIAL_CONTENT = -1,

        STATUS_REQ_DATA = -100,

        STATUS_NO_DATA = -300,
        STATUS_DATA_NOT_COMPLETE = -301,
        STATUS_TOO_SMALL = -305,
        STATUS_TOO_LARGE = -306,

        STATUS_STREAM_STOPPED = -401,
        STATUS_NO_THIS_STREAM = -403,
        STATUS_STREAM_ID_INVALID = -404,
        STATUS_REINIT_STREAM = -405,

        STATUS_PARAMETER_INVALID = -601,
        STATUS_RAW_DATA_INVALID = -602,
        STATUS_RAW_DATA_OVER_SIZE = -603,

        RTP_CACHE_UNKNOWN_ERROR = -701,
        RTP_CACHE_NO_THIS_STREAM = -710,
        RTP_CACHE_NO_THIS_TRACK = -720,
        RTP_CACHE_NO_THIS_SEQ = -730,
        RTP_CACHE_CIRCULAR_CACHE_EMPTY = -731,
        RTP_CACHE_MEDIA_CACHE_EMPTY = -732,
        RTP_CACHE_SEQ_TOO_LARGE = -736,
        RTP_CACHE_SEQ_TOO_SMALL = -737,
        RTP_CACHE_SET_RTP_FAILED = -741,
        RTP_CACHE_RTP_INVALID = -742,

        STATUS_CACHE_REQ_OK = 200, // OK, can close this session.
        STATUS_CACHE_REQ_ACCEPT = 202, // Accept, will be notified after OK.
        STATUS_CACHE_REQ_PARTIAL_CONTENT = 206, // Partial content, you should request remains.
        STATUS_CACHE_REQ_BAD_REQUEST = 400, // Bad request.
        STATUS_CACHE_REQ_NOT_FOUND = 404, // Not found this resource.
        STATUS_CACHE_REQ_INTERNAL_SERVER_ERROR = 500 // Internal Server Error.
    };

}
