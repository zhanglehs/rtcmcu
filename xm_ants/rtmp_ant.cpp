/*
 *
 * author: hechao@youku.com
 * create: 2014/3/4
 *
 */
#include "rtmp_ant.h"
#include <librtmp/rtmp.h>
#include <iostream>
#include <string>
#include "global_var.h"
#include "admin_server.h"
#include <net/uri.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <librtmp/log.h>

#define DEF_ON_METADATA     "onMetaData"
 
using namespace std;

int RTMPAnt::set_rtmp_config(std::string rtmp_svr_ip,
        uint16_t rtmp_svr_port,
        std::string rtmp_app_name,
        std::string rtmp_stream_name)
{
    _rtmp_svr_ip = rtmp_svr_ip;
    _rtmp_svr_port = rtmp_svr_port;
    _rtmp_app_name = rtmp_app_name;
    _rtmp_stream_name = rtmp_stream_name;

    return 0;
}

int RTMPAnt::set_rtmp_uri(string rtmp_uri)
{
    _rtmp_uri = rtmp_uri;

    URI uri;
    uri.parse(_rtmp_uri);

    string rtmp_app = uri.get_path();
    if (rtmp_app.length() >= 1
            && rtmp_app[0] == '/')
    {   
        rtmp_app.erase(0,1);
    }   
    if (rtmp_app.length() >= 1
            && rtmp_app[rtmp_app.length()-1] == '/')
    {   
        rtmp_app.erase(rtmp_app.length()-1);
    } 

    set_rtmp_config(uri.get_host().c_str(),
            uri.get_port(),
            rtmp_app,
            uri.get_file());
    return 0;
}

int RTMPAnt::_reconnect_server(RTMP* rtmp, int bufferTime, int dSeek,uint32_t timeout)
{
    uint32_t start = RTMP_GetTime();
    while (!RTMP_ctrlC && (RTMP_GetTime() - start) >= timeout)
    {
        RTMP_Close(rtmp);
        RTMP_Log(RTMP_LOGDEBUG, "Setting buffer time to: %dms", bufferTime);
        RTMP_SetBufferMS(rtmp, bufferTime);

        RTMP_LogPrintf("Connecting ...\n");

        if (!RTMP_Connect(rtmp, NULL))
        {
            usleep(100000);
            continue;
        }

        RTMP_Log(RTMP_LOGINFO, "Connected...");

        RTMP_LogPrintf("RTMP_ConnectStream ...\n");

        if (!RTMP_ConnectStream(rtmp,dSeek))
        {
            usleep(100000);
            continue;
        }
        RTMP_LogPrintf("RTMP_ConnectStreamed\n");
        return 0;
    }
    return 1;
}

int RTMPAnt::_is_exist_str(char* buffer, int bufferlen, const char* substr, int substrlen)
{
    assert(buffer);
    assert(substr);
    assert(buffer != substr);
    assert(bufferlen > 0);
    assert(substrlen > 0);
    assert(bufferlen >= substrlen);
    if (bufferlen < substrlen)
        return -1;
    if (bufferlen < 0 || substrlen < 0)
        return -1;
    if (buffer == substr)
        return 0;

    int i = 0,j = 0;
    int eqcount = 0;
    for (i = 0; i < bufferlen; i++)
    {
        for (j = 0; j < substrlen; j++)
        {
            if (buffer[i + j] == substr[j])
            {
                eqcount++;
                if (eqcount == substrlen)
                    return 0;
                continue;
            }
            eqcount = 0;
            break;
        }
    }
    return -1;
}

void RTMPAnt::run()
{
    // uint8_t byFirst = 0x01;
    // initialize rtmp object
    RTMP *rtmp = RTMP_Alloc();
    if (rtmp == NULL)
    {
        LOG_WARN(g_logger, "failed RTMP_Alloc");
        _stop(true);
        return;
    }
    RTMP_Init(rtmp);

    int protocol = RTMP_PROTOCOL_RTMP;

    char svr_ip[128] = {0};
    strncpy(svr_ip, _rtmp_svr_ip.c_str(), _rtmp_svr_ip.length());
    AVal hostname = {0,0};
    hostname.av_val = svr_ip;
    hostname.av_len = _rtmp_svr_ip.length();

    int port = _rtmp_svr_port;
    AVal sockshost = {0, 0};

    char stream_name[128] = {0};
    strncpy(stream_name, _rtmp_stream_name.c_str(), _rtmp_stream_name.length());
    AVal playpath = {0,0};
    playpath.av_val = stream_name;
    playpath.av_len = _rtmp_stream_name.length();

    AVal tcUrl = {0, 0};
    AVal swfUrl = {0, 0};
    AVal pageUrl = {0, 0};

    char app_name[32] = {0};
    strncpy(app_name, _rtmp_app_name.c_str(), _rtmp_app_name.length());
    AVal app = {0,0};
    app.av_val = app_name;
    app.av_len = _rtmp_app_name.length();

    AVal auth = {0, 0};
    AVal swfHash = {0, 0};
    uint32_t swfSize = 0;
    AVal flashVer = {0, 0};
    AVal subscribepath = {0, 0};
    int dSeek = 0;
    uint32_t dStopOffset = 0;
    int bLiveStream = TRUE;
    int timeout = 12;

    int total_size = 0;
    int buf_cursor = 0;
    int buf_remain = 0;

    //RTMP_debuglevel = RTMP_LOGINFO;
    RTMP_debuglevel = RTMP_LOGDEBUG;

    if (tcUrl.av_len == 0)
    {    
        char str[512] = { 0 }; 

        tcUrl.av_len = snprintf(str, sizeof(str), "%s://%.*s:%d/%.*s",
                RTMPProtocolStringsLower[protocol],
                hostname.av_len,
                hostname.av_val, port, app.av_len, app.av_val);
        tcUrl.av_val = (char *) malloc(tcUrl.av_len + 1);
        strcpy(tcUrl.av_val, str);
    }
    
    char str2[512] = {0};
    snprintf(str2, sizeof(str2), "host: %s port: %d appname: %s streamid: %s",
                (char*)hostname.av_val,
                port,
                (char*)app.av_val,
                _rtmp_stream_name.c_str());
    LOG_INFO(g_logger, "starting ant: "<< str2);

    int read = 0;
    //uint32_t size = 0;
    int32_t bufsize = 64 * 1024;
    char *buf = (char *)malloc(bufsize);

    RTMP_SetupStream(rtmp, protocol, &hostname,
            port, &sockshost, &playpath,
            &tcUrl, &swfUrl, &pageUrl,
            &app, &auth, &swfHash, swfSize,
            &flashVer, &subscribepath, dSeek,
            dStopOffset, bLiveStream, timeout);

    if (!RTMP_Connect(rtmp, NULL))
    {
        LOG_WARN(g_logger, "failed to establish RTMP connection. host: " << hostname.av_val
                << ". port: " << port << ". app: " << app.av_val << ". stream_name: " << playpath.av_val);
        goto end;
    }
    if (!RTMP_ConnectStream(rtmp, 0))
    {
        LOG_WARN(g_logger, "failed to establish RTMP session. host: " << hostname.av_val
                << ". port: " << port << ". app: " << app.av_val << ". stream_name: " << playpath.av_val);
        goto end;
    }


    do
    {
        if (buf_remain > 0)
        {
            int push_num = this->_push_data(buf + buf_cursor, (size_t)buf_remain, 0);
            //RTMP Ant only support 1 stream, so the 3rd parameter of _push_data() should always using '0'
            if (push_num <= 0)
            {
                // no data pushed, push again later;
                usleep(100000);
            }
            else
            {
                buf_cursor += push_num;
                buf_remain -= push_num;
            }
            continue;
        }

        buf_cursor = 0;
        buf_remain = 0;

        // read using Librtmp
        int read_num = RTMP_Read(rtmp, buf, (int)bufsize);

        if(read_num < 0)
        {
            if(errno == EAGAIN)  // read 0 bytes.
            {
                usleep(100000);
            }
            else
            {
                this->stop();
            }
            continue;
        }
        else if(read_num == 0)
        {
            usleep(100000);
            LOG_DEBUG(g_logger, "RTMP_Read 0 bytes.");
            continue;
        }

        buf_remain = read_num;
        total_size += read_num;

        LOG_INFO(g_logger, "total bytes read=" << total_size
            << ", chunk size=" << read_num << ", time=" <<  (double)rtmp->m_read.timestamp/1000.0);

    } while(!_trigger_off && !RTMP_ctrlC && read > -1 && RTMP_IsConnected(rtmp));

    // finalize
end:
    free(buf);
    if (rtmp)
    {
        RTMP_Close(rtmp);
        RTMP_Free(rtmp);
        rtmp = NULL;
    }

    _stop(true);
}

void RTMPAnt::run1()
{
    /*
    char *argv[] = {
        "./xmants",
        //"-r",
        //"rtmp://10.105.15.43:1935/simpleLive/6f9d90b",
        //"-v",
        //"-o",
        //"2.flv"
    };
    _main(1, argv);
    */

    _main(0, 0);
    _stop(true);
}

void RTMPAnt::stop()
{
    _trigger_off = true;
}



//==================================================================================================================


#define SET_BINMODE(f)

#define RD_SUCCESS      0
#define RD_FAILED       1
#define RD_INCOMPLETE       2

#define DEF_TIMEOUT 30  /* seconds */
#define DEF_BUFTIME (10 * 60 * 60 * 1000)   /* 10 hours default */
#define DEF_SKIPFRM 0

#define STR2AVAL(av,str)    av.av_val = str; av.av_len = strlen(av.av_val)

#ifdef _DEBUG
uint32_t debugTS = 0;
int pnum = 0;
#endif

uint32_t nIgnoredFlvFrameCounter = 0;
uint32_t nIgnoredFrameCounter = 0;
#define MAX_IGNORED_FRAMES  50

FILE *file = 0;

/*
void
sigIntHandler(int sig)
{
  RTMP_ctrlC = TRUE;
  RTMP_LogPrintf("Caught signal: %d, cleaning up, just a second...\n", sig);
  // ignore all these signals now and let the connection close
  signal(SIGINT, SIG_IGN);
  signal(SIGTERM, SIG_IGN);
#ifndef WIN32
  signal(SIGHUP, SIG_IGN);
  signal(SIGPIPE, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);
#endif
}
*/

#define HEX2BIN(a)      (((a)&0x40)?((a)&0xf)+9:((a)&0xf))
int hex2bin(char *str, char **hex)
{
  char *ptr;
  int i, l = strlen(str);

  if (l & 1)
    return 0;

  *hex = (char*)malloc(l/2);
  ptr = *hex;
  if (!ptr)
    return 0;

  for (i=0; i<l; i+=2)
    *ptr++ = (HEX2BIN(str[i]) << 4) | HEX2BIN(str[i+1]);
  return l/2;
}

static const AVal av_onMetaData = AVC("onMetaData");
static const AVal av_duration = AVC("duration");
static const AVal av_conn = AVC("conn");
static const AVal av_token = AVC("token");
static const AVal av_playlist = AVC("playlist");
static const AVal av_true = AVC("true");

int RTMPAnt::_download(RTMP * rtmp,     // connected RTMP object
     uint32_t dSeek, uint32_t dStopOffset,
     double duration, int bResume, char *metaHeader,
     uint32_t nMetaHeaderSize, char *initialFrame,
     int initialFrameType, uint32_t nInitialFrameSize,
     int nSkipKeyFrames, int bStdoutMode, int bLiveStream,
     int bHashes, int bOverrideBufferTime, uint32_t bufferTime, double *percent)    // percentage downloaded [out]
{
    int32_t now, lastUpdate;
    int bufferSize = 64 * 1024;
    char *buffer = (char *) malloc(bufferSize);
    int nRead = 0;

    off_t size = 0;
    unsigned long lastPercent = 0;

    rtmp->m_read.timestamp = dSeek;

    *percent = 0.0;

    if (rtmp->m_read.timestamp)
    {
        RTMP_Log(RTMP_LOGDEBUG, "Continuing at TS: %d ms\n", rtmp->m_read.timestamp);
    }

    if (bLiveStream)
    {
        RTMP_LogPrintf("Starting Live Stream\n");
    }
    /*
    else
    {
        // print initial status
        // Workaround to exit with 0 if the file is fully (> 99.9%) downloaded
        if (duration > 0)
        {
            if ((double) rtmp->m_read.timestamp >= (double) duration * 999.0)
            {
                RTMP_LogPrintf("Already Completed at: %.3f sec Duration=%.3f sec\n",
                        (double) rtmp->m_read.timestamp / 1000.0,
                        (double) duration / 1000.0);
                return RD_SUCCESS;
            }
            else
            {
                *percent = ((double) rtmp->m_read.timestamp) / (duration * 1000.0) * 100.0;
                *percent = ((double) (int) (*percent * 10.0)) / 10.0;
                RTMP_LogPrintf("%s download at: %.3f kB / %.3f sec (%.1f%%)\n",
                        bResume ? "Resuming" : "Starting",
                        (double) size / 1024.0, (double) rtmp->m_read.timestamp / 1000.0,
                        *percent);
            }
        }
        else
        {
            RTMP_LogPrintf("%s download at: %.3f kB\n",
                    bResume ? "Resuming" : "Starting",
                    (double) size / 1024.0);
        }
    }
    */

    if (dStopOffset > 0)
        RTMP_LogPrintf("For duration: %.3f sec\n", (double) (dStopOffset - dSeek) / 1000.0);

    if (bResume && nInitialFrameSize > 0)
        rtmp->m_read.flags |= RTMP_READ_RESUME;

    rtmp->m_read.initialFrameType = initialFrameType;
    rtmp->m_read.nResumeTS = dSeek;
    rtmp->m_read.metaHeader = metaHeader;
    rtmp->m_read.initialFrame = initialFrame;
    rtmp->m_read.nMetaHeaderSize = nMetaHeaderSize;
    rtmp->m_read.nInitialFrameSize = nInitialFrameSize;

    now = RTMP_GetTime();
    lastUpdate = now - 1000;
    do
    {
        nRead = RTMP_Read(rtmp, buffer, bufferSize);
        if (nRead > 0)
        {
            /*
            if (fwrite(buffer, sizeof(unsigned char), nRead, file) !=
                    (size_t) nRead)
            {
                RTMP_Log(RTMP_LOGERROR, "%s: Failed writing, exiting!", __FUNCTION__);
                free(buffer);
                return RD_FAILED;
            }
            */
            if (_push_data(buffer, (size_t)nRead, 0) < 0)
            {
                break;
            }
            size += nRead;

            if (duration <= 0)  // if duration unknown try to get it from the stream (onMetaData)
                duration = RTMP_GetDuration(rtmp);

            if (duration > 0)
            {
                // make sure we claim to have enough buffer time!
                if (!bOverrideBufferTime && bufferTime < (duration * 1000.0))
                {
                    bufferTime = (uint32_t) (duration * 1000.0) + 5000; // extra 5sec to make sure we've got enough

                    RTMP_Log(RTMP_LOGDEBUG,
                            "Detected that buffer time is less than duration, resetting to: %dms",
                            bufferTime);
                    RTMP_SetBufferMS(rtmp, bufferTime);
                    RTMP_UpdateBufferMS(rtmp);
                }
                *percent = ((double) rtmp->m_read.timestamp) / (duration * 1000.0) * 100.0;
                *percent = ((double) (int) (*percent * 10.0)) / 10.0;
                if (bHashes)
                {
                    if (lastPercent + 1 <= *percent)
                    {
                        RTMP_LogStatus("#");
                        lastPercent = (unsigned long) *percent;
                    }
                }
                else
                {
                    now = RTMP_GetTime();
                    if (abs(now - lastUpdate) > 200)
                    {
                        RTMP_LogStatus("\r%.3f kB / %.2f sec (%.1f%%)",
                                (double) size / 1024.0,
                                (double) (rtmp->m_read.timestamp) / 1000.0, *percent);
                        lastUpdate = now;
                    }
                }
            }
            else
            {
                now = RTMP_GetTime();
                if (abs(now - lastUpdate) > 200)
                {
                    if (bHashes)
                        RTMP_LogStatus("#");
                    else
                        RTMP_LogStatus("\r%.3f kB / %.2f sec", (double) size / 1024.0,
                                (double) (rtmp->m_read.timestamp) / 1000.0);
                    lastUpdate = now;
                }
            }
        }
#ifdef _DEBUG
        else
        {
            RTMP_Log(RTMP_LOGDEBUG, "zero read!");
        }
#endif

    } while (!_trigger_off && !RTMP_ctrlC && nRead > -1 && RTMP_IsConnected(rtmp));

    free(buffer);
    if (nRead < 0)
        nRead = rtmp->m_read.status;

    /* Final status update */
    if (!bHashes)
    {
        if (duration > 0)
        {
            *percent = ((double) rtmp->m_read.timestamp) / (duration * 1000.0) * 100.0;
            *percent = ((double) (int) (*percent * 10.0)) / 10.0;
            RTMP_LogStatus("\r%.3f kB / %.2f sec (%.1f%%)",
                    (double) size / 1024.0,
                    (double) (rtmp->m_read.timestamp) / 1000.0, *percent);
        }
        else
        {
            RTMP_LogStatus("\r%.3f kB / %.2f sec", (double) size / 1024.0,
                    (double) (rtmp->m_read.timestamp) / 1000.0);
        }
    }

    RTMP_Log(RTMP_LOGDEBUG, "RTMP_Read returned: %d", nRead);

    if (bResume && nRead == -2)
    {
        RTMP_LogPrintf("Couldn't resume FLV file, try --skip %d\n\n",
                nSkipKeyFrames + 1);
        return RD_FAILED;
    }

    if (nRead == -3)
        return RD_SUCCESS;

    if ((duration > 0 && *percent < 99.9) || RTMP_ctrlC || nRead < 0
            || RTMP_IsTimedout(rtmp))
    {
        return RD_INCOMPLETE;
    }

    return RD_SUCCESS;
}


int RTMPAnt::_main(int argc, char **argv)
{
  //extern char *optarg;

  int nStatus = RD_SUCCESS;
  double percent = 0;
  double duration = 0.0;

  int nSkipKeyFrames = DEF_SKIPFRM; // skip this number of keyframes when resuming

  int bOverrideBufferTime = FALSE;  // if the user specifies a buffer time override this is true
  int bStdoutMode = TRUE;   // if true print the stream directly to stdout, messages go to stderr
  int bResume = FALSE;      // true in resume mode
  uint32_t dSeek = 0;       // seek position in resume mode, 0 otherwise
  uint32_t bufferTime = DEF_BUFTIME;

  // meta header and initial frame for the resume mode (they are read from the file and compared with
  // the stream we are trying to continue
  char *metaHeader = 0;
  uint32_t nMetaHeaderSize = 0;

  // video keyframe for matching
  char *initialFrame = 0;
  uint32_t nInitialFrameSize = 0;
  int initialFrameType = 0; // tye: audio or video

  AVal hostname = { 0, 0 };
  AVal playpath = { 0, 0 };
  AVal subscribepath = { 0, 0 };
  int port = -1;
  int protocol = RTMP_PROTOCOL_UNDEFINED;
  int retries = 0;
  int bLiveStream = TRUE;   // is it a live stream? then we can't seek/resume
  int bHashes = FALSE;      // display byte counters not hashes by default

  long int timeout = DEF_TIMEOUT;   // timeout connection after 120 seconds
  uint32_t dStartOffset = 0;    // seek position in non-live mode
  uint32_t dStopOffset = 0;
  RTMP rtmp = { 0 };

  AVal swfUrl = { 0, 0 };
  AVal tcUrl = { 0, 0 };
  AVal pageUrl = { 0, 0 };
  AVal app = { 0, 0 };
  AVal auth = { 0, 0 };
  AVal swfHash = { 0, 0 };
  uint32_t swfSize = 0;
  AVal flashVer = { 0, 0 };
  AVal sockshost = { 0, 0 };

#ifdef CRYPTO
  int swfAge = 30;  /* 30 days for SWF cache by default */
  int swfVfy = 0;
  unsigned char hash[RTMP_SWF_HASHLEN];
#endif

  char *flvFile = 0;

  RTMP_debuglevel = RTMP_LOGINFO;

  /* sleep(30); */

  RTMP_Init(&rtmp);

  //=======================================

        AVal parsedHost, parsedApp, parsedPlaypath;
        unsigned int parsedPort = 0;
        int parsedProtocol = RTMP_PROTOCOL_UNDEFINED;

        if (!RTMP_ParseURL
        (_rtmp_uri.c_str(), &parsedProtocol, &parsedHost, &parsedPort,
         &parsedPlaypath, &parsedApp))
          {
        RTMP_Log(RTMP_LOGWARNING, "Couldn't parse the specified url (%s)!",
            //optarg);
            _rtmp_uri.c_str());
          }
        else
          {
        if (!hostname.av_len)
          hostname = parsedHost;
        if (port == -1)
          port = parsedPort;
        if (playpath.av_len == 0 && parsedPlaypath.av_len)
          {
            playpath = parsedPlaypath;
          }
        if (protocol == RTMP_PROTOCOL_UNDEFINED)
          protocol = parsedProtocol;
        if (app.av_len == 0 && parsedApp.av_len)
          {
            app = parsedApp;
          }
          }




  if (!hostname.av_len)
    {
      RTMP_Log(RTMP_LOGERROR,
      "You must specify a hostname (--host) or url (-r \"rtmp://host[:port]/playpath\") containing a hostname");
      return RD_FAILED;
    }
  if (playpath.av_len == 0)
    {
      RTMP_Log(RTMP_LOGERROR,
      "You must specify a playpath (--playpath) or url (-r \"rtmp://host[:port]/playpath\") containing a playpath");
      return RD_FAILED;
    }

  if (protocol == RTMP_PROTOCOL_UNDEFINED)
    {
      RTMP_Log(RTMP_LOGWARNING,
      "You haven't specified a protocol (--protocol) or rtmp url (-r), using default protocol RTMP");
      protocol = RTMP_PROTOCOL_RTMP;
    }
  if (port == -1)
    {
      RTMP_Log(RTMP_LOGWARNING,
      "You haven't specified a port (--port) or rtmp url (-r), using default port 1935");
      port = 0;
    }
  if (port == 0)
    {
      if (protocol & RTMP_FEATURE_SSL)
    port = 443;
      else if (protocol & RTMP_FEATURE_HTTP)
    port = 80;
      else
    port = 1935;
    }

  if (flvFile == 0)
    {
      RTMP_Log(RTMP_LOGWARNING,
      "You haven't specified an output file (-o filename), using stdout");
      bStdoutMode = TRUE;
    }

  if (bStdoutMode && bResume)
    {
      RTMP_Log(RTMP_LOGWARNING,
      "Can't resume in stdout mode, ignoring --resume option");
      bResume = FALSE;
    }

  if (bLiveStream && bResume)
    {
      RTMP_Log(RTMP_LOGWARNING, "Can't resume live stream, ignoring --resume option");
      bResume = FALSE;
    }

#ifdef CRYPTO
  if (swfVfy)
    {
      if (RTMP_HashSWF(swfUrl.av_val, &swfSize, hash, swfAge) == 0)
        {
          swfHash.av_val = (char *)hash;
          swfHash.av_len = RTMP_SWF_HASHLEN;
        }
    }

  if (swfHash.av_len == 0 && swfSize > 0)
    {
      RTMP_Log(RTMP_LOGWARNING,
      "Ignoring SWF size, supply also the hash with --swfhash");
      swfSize = 0;
    }

  if (swfHash.av_len != 0 && swfSize == 0)
    {
      RTMP_Log(RTMP_LOGWARNING,
      "Ignoring SWF hash, supply also the swf size  with --swfsize");
      swfHash.av_len = 0;
      swfHash.av_val = NULL;
    }
#endif

  if (tcUrl.av_len == 0)
    {
      char str[512] = { 0 };

      tcUrl.av_len = snprintf(str, 511, "%s://%.*s:%d/%.*s",
           RTMPProtocolStringsLower[protocol], hostname.av_len,
           hostname.av_val, port, app.av_len, app.av_val);
      tcUrl.av_val = (char *) malloc(tcUrl.av_len + 1);
      strcpy(tcUrl.av_val, str);
    }

  int first = 1;

  // User defined seek offset
  if (dStartOffset > 0)
    {
      // Live stream
      if (bLiveStream)
    {
      RTMP_Log(RTMP_LOGWARNING,
          "Can't seek in a live stream, ignoring --start option");
      dStartOffset = 0;
    }
    }

  RTMP_SetupStream(&rtmp, protocol, &hostname, port, &sockshost, &playpath,
           &tcUrl, &swfUrl, &pageUrl, &app, &auth, &swfHash, swfSize,
           &flashVer, &subscribepath, dSeek, dStopOffset, bLiveStream, timeout);

  /* Try to keep the stream moving if it pauses on us */
  if (!bLiveStream && !(protocol & RTMP_FEATURE_HTTP))
    rtmp.Link.lFlags |= RTMP_LF_BUFX;

  //off_t size = 0;

  while (!RTMP_ctrlC)
    {
      RTMP_Log(RTMP_LOGDEBUG, "Setting buffer time to: %dms", bufferTime);
      RTMP_SetBufferMS(&rtmp, bufferTime);

      if (first)
    {
      first = 0;
      RTMP_LogPrintf("Connecting ...\n");

      if (!RTMP_Connect(&rtmp, NULL))
        {
          nStatus = RD_FAILED;
          break;
        }

      RTMP_Log(RTMP_LOGINFO, "Connected...");

      // User defined seek offset
      if (dStartOffset > 0)
        {
          // Don't need the start offset if resuming an existing file
          if (bResume)
        {
          RTMP_Log(RTMP_LOGWARNING,
              "Can't seek a resumed stream, ignoring --start option");
          dStartOffset = 0;
        }
          else
        {
          dSeek = dStartOffset;
        }
        }

      // Calculate the length of the stream to still play
      if (dStopOffset > 0)
        {
          // Quit if start seek is past required stop offset
          if (dStopOffset <= dSeek)
        {
          RTMP_LogPrintf("Already Completed\n");
          nStatus = RD_SUCCESS;
          break;
        }
        }

      if (!RTMP_ConnectStream(&rtmp, dSeek))
        {
          nStatus = RD_FAILED;
          break;
        }
    }
      else
    {
      nInitialFrameSize = 0;

          if (retries)
            {
          RTMP_Log(RTMP_LOGERROR, "Failed to resume the stream\n\n");
          if (!RTMP_IsTimedout(&rtmp))
            nStatus = RD_FAILED;
          else
            nStatus = RD_INCOMPLETE;
          break;
            }
      RTMP_Log(RTMP_LOGINFO, "Connection timed out, trying to resume.\n\n");
          /* Did we already try pausing, and it still didn't work? */
          if (rtmp.m_pausing == 3)
            {
              /* Only one try at reconnecting... */
              retries = 1;
              dSeek = rtmp.m_pauseStamp;
              if (dStopOffset > 0)
                {
                  if (dStopOffset <= dSeek)
                    {
                      RTMP_LogPrintf("Already Completed\n");
              nStatus = RD_SUCCESS;
              break;
                    }
                }
              if (!RTMP_ReconnectStream(&rtmp, dSeek))
                {
              RTMP_Log(RTMP_LOGERROR, "Failed to resume the stream\n\n");
              if (!RTMP_IsTimedout(&rtmp))
            nStatus = RD_FAILED;
              else
            nStatus = RD_INCOMPLETE;
              break;
                }
            }
      else if (!RTMP_ToggleStream(&rtmp))
        {
          RTMP_Log(RTMP_LOGERROR, "Failed to resume the stream\n\n");
          if (!RTMP_IsTimedout(&rtmp))
        nStatus = RD_FAILED;
          else
        nStatus = RD_INCOMPLETE;
          break;
        }
      bResume = TRUE;
    }

      nStatus = _download(&rtmp, dSeek, dStopOffset, duration, bResume,
             metaHeader, nMetaHeaderSize, initialFrame,
             initialFrameType, nInitialFrameSize,
             nSkipKeyFrames, bStdoutMode, bLiveStream, bHashes,
             bOverrideBufferTime, bufferTime, &percent);
      free(initialFrame);
      initialFrame = NULL;

      /* If we succeeded, we're done.
       */
      if (nStatus != RD_INCOMPLETE || !RTMP_IsTimedout(&rtmp) || bLiveStream)
    break;
    }

  if (nStatus == RD_SUCCESS)
    {
      RTMP_LogPrintf("Download complete\n");
    }
  else if (nStatus == RD_INCOMPLETE)
    {
      RTMP_LogPrintf
    ("Download may be incomplete (downloaded about %.2f%%), try resuming\n",
     percent);
    }

//clean:
  RTMP_Log(RTMP_LOGDEBUG, "Closing connection.\n");
  RTMP_Close(&rtmp);

  return nStatus;
}

