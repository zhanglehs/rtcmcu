#include "perf.h"
#include "util/log.h"
#include <string.h>

#define ONE_MB (1024 * 1024)
#define SMLBUFSIZ 256

Perf * Perf::_instance = NULL;

Perf * Perf::get_instance()
{
	if (NULL == _instance)
		_instance = new Perf();
	return _instance;
}

void Perf::destory()
{
	delete _instance;
}

Perf::Perf()
{
	_cpu = (cpu_t *)malloc(sizeof(cpu_t));
    memset(_cpu, 0, sizeof(cpu_t));
	_sys_info.online_processors = sysconf(_SC_NPROCESSORS_ONLN);
    _last_update = 0;
    _last_total = 0;
    _last_process = 0;
}

Perf::~Perf()
{
	free(_cpu);
}

cpu_t *Perf::cpus_refresh()
{
	static FILE *fp = NULL;
	int num;
	char buf[SMLBUFSIZ];

	if (!fp) {
		if (!(fp = fopen("/proc/stat", "r")))
			ERR("failed /proc/stat open: %s", strerror(errno));
	}
	rewind(fp);
	fflush(fp);

	// first value the last slot with the cpu summary line
	if (!fgets(buf, sizeof(buf), fp)) ERR("failed /proc/stat read.");

	_cpu->x = 0;
	_cpu->y = 0;
	_cpu->z = 0;
	num = sscanf(buf, "cpu %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu",
		&_cpu->u,
		&_cpu->n,
		&_cpu->s,
		&_cpu->i,
		&_cpu->w,
		&_cpu->x,
		&_cpu->y,
		&_cpu->z
		);
	if (num < 4)
		ERR("failed /proc/stat read. parameters less than 4");

	return _cpu;
}

void Perf::update_cpu_rate()
{
#define TRIMz(x)  ((tz = (long long)(x)) < 0 ? 0 : tz)
	long long u_frme, s_frme, n_frme, i_frme, w_frme, x_frme, y_frme, z_frme, tot_frme, tz;
	float scale;

	u_frme = _cpu->u - _cpu->u_sav;
	s_frme = _cpu->s - _cpu->s_sav;
	n_frme = _cpu->n - _cpu->n_sav;
	i_frme = TRIMz(_cpu->i - _cpu->i_sav);
	w_frme = _cpu->w - _cpu->w_sav;
	x_frme = _cpu->x - _cpu->x_sav;
	y_frme = _cpu->y - _cpu->y_sav;
	z_frme = _cpu->z - _cpu->z_sav;
	tot_frme = u_frme + s_frme + n_frme + i_frme + w_frme + x_frme + y_frme + z_frme;
	if (tot_frme < 1) tot_frme = 1;
	scale = 100.0 / (float)tot_frme;

	_cpu->u_sav = _cpu->u;
	_cpu->s_sav = _cpu->s;
	_cpu->n_sav = _cpu->n;
	_cpu->i_sav = _cpu->i;
	_cpu->w_sav = _cpu->w;
	_cpu->x_sav = _cpu->x;
	_cpu->y_sav = _cpu->y;
	_cpu->z_sav = _cpu->z;

	_sys_info.cpu_rate = (tot_frme - i_frme) * scale;

#undef TRIMz
}

void Perf::update_mem_state()
{
	long page_size = sysconf(_SC_PAGESIZE);
	long num_pages = sysconf(_SC_PHYS_PAGES);
	long free_pages = sysconf(_SC_AVPHYS_PAGES);
	ul_t mem = (ul_t)((ul_t)num_pages * (ul_t)page_size);
	_sys_info.mem_ocy = (mem /= ONE_MB);
	ul_t free_mem = (ul_t)((ul_t)free_pages * (ul_t)page_size);
	_sys_info.free_mem_ocy = (free_mem /= ONE_MB);
}

void Perf::keep_update()
{
	update_mem_state();
    update_cur_process_cpu_rate();
	cpus_refresh();
	update_cpu_rate();
}

sys_info_t Perf::get_sys_info()
{
	return _sys_info;
}


char* get_items(char* buffer, int ie)
{
    char* p = buffer;
    int len = strlen(buffer);
    int count = 0;
    if (1 == ie || ie < 1)
    {
        return p;
    }
    int i;

    for (i = 0; i < len; i++)
    {
        if (' ' == *p)
        {
            count++;
            if (count == ie - 1)
            {
                p++;
                break;
            }
        }
        p++;
    }

    return p;
}

unsigned int get_cpu_process_occupy(const pid_t p)
{
    char file[64] = { 0 };
    pid_t pid;
    unsigned int utime;
    unsigned int stime;
    unsigned int cutime;
    unsigned int cstime;

    FILE *fd;
    char line_buff[1024] = { 0 };
    sprintf(file, "/proc/%d/stat", p);
    fd = fopen(file, "r");
    fgets(line_buff, sizeof(line_buff), fd);

    sscanf(line_buff, "%u", &pid);
    char* q = get_items(line_buff, 14);
    sscanf(q, "%u %u %u %u", &utime, &stime, &cutime, &cstime);
    fclose(fd);
    return (utime + stime + cutime + cstime);
}


unsigned int get_cpu_total_occupy()
{
    FILE *fd;
    char buff[1024] = { 0 };
    unsigned int user;
    unsigned int nice;
    unsigned int system;
    unsigned int idle;

    fd = fopen("/proc/stat", "r");
    fgets(buff, sizeof(buff), fd);
    char name[16];
    sscanf(buff, "%s %u %u %u %u", name, &user, &nice, &system, &idle);

    fclose(fd);
    return (user + nice + system + idle);
}


void Perf::update_cur_process_cpu_rate() {
    unsigned int totalcputime = get_cpu_total_occupy();
    unsigned int procputime = get_cpu_process_occupy(getpid());
     
    if (_last_update  > 0)
    {
        if (time(NULL) - _last_update > 2)
        {
            _sys_info.cur_process_cpu_rate 
                = (ul_t)(100.0*(procputime - _last_process) / (totalcputime - _last_total)*_sys_info.online_processors);
            _last_total = totalcputime;
            _last_process = procputime;
            _last_update = time(NULL);
        }
    }
    else {
        _last_total = totalcputime;
        _last_process = procputime;
        _last_update = time(NULL);
    }
}

void Perf::set_cpu_rate_threshold(int32_t threshold)
{
    _cpu_rate_threshold = threshold;
}

bool Perf::is_sys_busy() {
    return _cpu_rate_threshold != 0 && (int)_sys_info.cur_process_cpu_rate >= _cpu_rate_threshold;
}

bool Perf::is_sys_full_load() {
    return _cpu_rate_threshold != 0 && _sys_info.cur_process_cpu_rate >= 85;
}
