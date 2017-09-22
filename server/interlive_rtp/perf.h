#ifndef _PERF_H_
#define _PERF_H_

#include <stdio.h>
#include <malloc.h>
#include <errno.h>
#include <unistd.h>
#include "stdint.h"
#include "time.h"

typedef unsigned long long ul_t;

typedef struct cpu_t {
	ul_t u, n, s, i, w, x, y, z; // as represented in /proc/stat
	ul_t u_sav, s_sav, n_sav, i_sav, w_sav, x_sav, y_sav, z_sav; // in the order of our display
} cpu_t;

typedef struct sys_info_t {
	ul_t mem_ocy;
	ul_t free_mem_ocy;
	ul_t cpu_rate;
	ul_t online_processors;
    ul_t cur_process_cpu_rate;
} sys_info_t;

class Perf {
public:
	static Perf * get_instance();
	static void destory();
	void keep_update();
	sys_info_t get_sys_info();
    void set_cpu_rate_threshold(int32_t threshold);
    bool is_sys_busy();
    bool is_sys_full_load();
private:
	Perf();
	~Perf();
	cpu_t *cpus_refresh();
    void update_cur_process_cpu_rate();
	void update_cpu_rate();
	void update_mem_state();
private:
	static Perf *_instance;
	cpu_t * _cpu;
	sys_info_t _sys_info;
    int32_t  _cpu_rate_threshold;

    uint32_t _last_total;
    uint32_t _last_process;
    time_t _last_update;
};

#endif
