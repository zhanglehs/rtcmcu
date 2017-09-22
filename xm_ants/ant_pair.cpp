#include "flv_sender.h"
#include "admin_server.h"

AntPair::AntPair() :
shutdown_flag(0), ant(NULL), sender(NULL),buffer(NULL), status(ANT_PAIR_STOPED),
continuous_failure_count(0),stream_n_number(0),stream_id_remove_flag(NULL)
{
    reschedule_flag = true;
    plan_end_flag = false;
    ant_start_flag = false;
    gettimeofday(&first_retry_time, 0); 
    gettimeofday(&last_start_time, 0);
    gettimeofday(&plan_end_time, 0);
    restart_wait_time_us = 100000;
}

void AntPair::stop(bool reschedule)
{
    if (this->ant != NULL)
    {
        ant->stop();
    }

    if (this->sender != NULL)
    {
        for(int i = 0; i < this->stream_n_number; i++)
        {
            sender[i].stop();
        }
    }

    reschedule_flag = reschedule;
    ant_start_flag = false;
}

AntPair::~AntPair()
{
    if (this->ant != NULL)
    {
        delete this->ant;
        this->ant = NULL;
    }
    
    if (this->sender != NULL)
    {
        delete[] this->sender;
        this->sender = NULL;
    }

    if (this->buffer != NULL)
    {
        delete[] this->buffer;
        this->buffer = NULL;
    }

    if (this->stream_id_remove_flag != NULL)
    {
        delete[] this->stream_id_remove_flag;
        this->stream_id_remove_flag = NULL;
    }
}

struct timeval AntPair::tolerate_failure_time;
int AntPair::tolerate_failure_count = 0;

int AntPair::set_config(int tolerate_failure_time, int tolerate_failure_count)
{
    AntPair::tolerate_failure_time.tv_sec = tolerate_failure_time;
    AntPair::tolerate_failure_time.tv_usec = 0;
    AntPair::tolerate_failure_count = tolerate_failure_count;
}

void AntPair::heart_beat()
{

}

/**
 * we defined a restart strategy.
 * 1. failure: a failure means after a task starts, no data sent to receiver in 10 seconds.
 * 2. if a failure happens, it will restart in restart_wait_time. (default initial value is 100 ms)
 * 3. if it fails again, restart_wait_time*= 2. (max value is 10 seconds).
 * 4. if there are no success in 60 seconds, it will be stop permanently.
 */
int AntPair::check_restart()
{
    if (!reschedule_flag)
    {
        return RESTART_STRATEGY_STOP;
    }

    timeval now_time;

    gettimeofday(&now_time, 0);

    // success
    if (sender != NULL)
    {
        if (sender->get_total_sent_bytes() > 128 * 1024)//for n sender in one AntPair, check the first sender.
        {
            restart_wait_time_us = 100 * 1000;
            continuous_failure_count = 0;
            return RESTART_STRATEGY_SUCCESS;
        }
    }

    // waiting for next check
    if (now_time.tv_sec - last_start_time.tv_sec <= 15)
    {
        return RESTART_STRATEGY_WAIT;
    }

    // failure
    continuous_failure_count++;

    if (continuous_failure_count == 1)
    {
        first_retry_time = last_start_time;
    }

    return RESTART_STRATEGY_RESTART;
}

int AntPair::check_reschedule()
{
    timeval now_time;

    gettimeofday(&now_time, 0);

    if ((now_time.tv_sec - last_start_time.tv_sec) * 1000000
        + (now_time.tv_usec - last_start_time.tv_usec)
        < restart_wait_time_us)
    {
        return RESTART_STRATEGY_WAIT;
    }

    restart_wait_time_us *= 2;
    if (restart_wait_time_us > 10 * 1000 * 1000)
    {
        restart_wait_time_us = 10 * 1000 * 1000;
    }

    if ((now_time.tv_sec - first_retry_time.tv_sec > 60) && (continuous_failure_count != 0))
    {
        return RESTART_STRATEGY_STOP;
    }
    else
    {
        return RESTART_STRATEGY_RESTART;
    }
}
