#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

typedef uint32_t u32;
typedef int32_t i32;

struct process
{
  u32 pid;
  u32 arrival_time;
  u32 burst_time;

  u32 remaining_time;
  u32 start_exec_time;
  u32 waiting_time;
  u32 response_time;
  bool first_run;

  TAILQ_ENTRY(process) pointers;
};

TAILQ_HEAD(process_list, process);

u32 next_int(const char **data, const char *data_end)
{
  u32 current = 0;
  bool started = false;
  while (*data != data_end)
  {
    char c = **data;

    if (c < 0x30 || c > 0x39)
    {
      if (started)
      {
        return current;
      }
    }
    else
    {
      if (!started)
      {
        current = (c - 0x30);
        started = true;
      }
      else
      {
        current *= 10;
        current += (c - 0x30);
      }
    }

    ++(*data);
  }

  printf("Reached end of file while looking for another integer\n");
  exit(EINVAL);
}

u32 next_int_from_c_str(const char *data)
{
  char c;
  u32 i = 0;
  u32 current = 0;
  bool started = false;
  while ((c = data[i++]))
  {
    if (c < 0x30 || c > 0x39)
    {
      exit(EINVAL);
    }
    if (!started)
    {
      current = (c - 0x30);
      started = true;
    }
    else
    {
      current *= 10;
      current += (c - 0x30);
    }
  }
  return current;
}

void init_processes(const char *path,
                    struct process **process_data,
                    u32 *process_size)
{
  int fd = open(path, O_RDONLY);
  if (fd == -1)
  {
    int err = errno;
    perror("open");
    exit(err);
  }

  struct stat st;
  if (fstat(fd, &st) == -1)
  {
    int err = errno;
    perror("stat");
    exit(err);
  }

  u32 size = st.st_size;
  const char *data_start = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (data_start == MAP_FAILED)
  {
    int err = errno;
    perror("mmap");
    exit(err);
  }

  const char *data_end = data_start + size;
  const char *data = data_start;

  *process_size = next_int(&data, data_end);

  *process_data = calloc(sizeof(struct process), *process_size);
  if (*process_data == NULL)
  {
    int err = errno;
    perror("calloc");
    exit(err);
  }

  for (u32 i = 0; i < *process_size; ++i)
  {
    (*process_data)[i].pid = next_int(&data, data_end);
    (*process_data)[i].arrival_time = next_int(&data, data_end);
    (*process_data)[i].burst_time = next_int(&data, data_end);
  }

  munmap((void *)data, size);
  close(fd);
}

int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    return EINVAL;
  }
  struct process *data;
  u32 size;
  init_processes(argv[1], &data, &size);

  u32 quantum_length = next_int_from_c_str(argv[2]);

  struct process_list list; // queue
  TAILQ_INIT(&list);

  u32 total_waiting_time = 0;
  u32 total_response_time = 0;

  // find the earliest-to-arrive process 
  int current_time = data[0].arrival_time;
  int first = 0;
  for (int i = 0; i < size; i++) {
    if (data[i].arrival_time == 0) {
      current_time = 0;
      first = i;
      break;
    }
    else if (data[i].arrival_time < current_time) {
      current_time = data[i].arrival_time;
      first = i;
    }
  }
  // add earliest-to-arrive process to start of queue
  struct process *p = &data[first];
  TAILQ_INSERT_TAIL(&list, p, pointers);

  int start_time;
  while (!TAILQ_EMPTY(&list)) {
    start_time = current_time;
    struct process *p = TAILQ_FIRST(&list);
    //printf("pid running: %d\n", p->pid);
    struct process *q;

    // first time process is on CPU
    if (p->first_run == false) {
      p->first_run = true;
      p->remaining_time = p->burst_time;
      p->start_exec_time = current_time;
      p->response_time = p->start_exec_time - p->arrival_time;
    }
    // process uses all CPU time
    if (p->remaining_time >= quantum_length) {
      current_time += quantum_length;
      p->remaining_time -= quantum_length;
    }
    // process finishes early
    else if (p->remaining_time < quantum_length) {
      current_time += p->remaining_time; 
      p->remaining_time = 0; // process finished
    }
    // add any processes to queue that arrived during time slice
    struct process_list tmp;
    TAILQ_INIT(&tmp);
    for (int i = 0; i < size; i++) {
      if (data[i].pid != p->pid && data[i].arrival_time <= current_time && data[i].arrival_time > start_time) { // process found
        if (TAILQ_EMPTY(&tmp)) {
          TAILQ_INSERT_TAIL(&tmp, &data[i], pointers);
        }
        else {
          struct process *x = TAILQ_FIRST(&tmp);
          struct process *y = &data[i];
          bool match = false;
          TAILQ_FOREACH(q, &tmp, pointers) {
            if (q->arrival_time >= x->arrival_time && q->arrival_time < data[i].arrival_time) {
              x = q;
              match = true;
            }
          }
          if (match) TAILQ_INSERT_AFTER(&tmp, x, y, pointers);
          else TAILQ_INSERT_HEAD(&tmp, y, pointers);
        }
      }
    }
    while (!TAILQ_EMPTY(&tmp)) {
      q = TAILQ_FIRST(&tmp);
      //printf("pid insert: %d\n", q->pid);
      TAILQ_REMOVE(&tmp, q, pointers);
      TAILQ_INSERT_TAIL(&list, q, pointers);
    }
    // add old process to end of queue if remaining time > 0
    if (p->remaining_time > 0) {
      //printf("pid insert: %d\n", p->pid);
      TAILQ_REMOVE(&list, p, pointers);
      TAILQ_INSERT_TAIL(&list, p, pointers);
    }
    else {
      //printf("pid: %d, %d, %d, %d\n", p->pid, current_time, p->start_exec_time, p->burst_time);
      p->waiting_time = current_time - (p->arrival_time) - (p->burst_time);
      TAILQ_REMOVE(&list, p, pointers);
    }
  }
  // calculate totals
  for (int i = 0; i < size; i++) {
    total_waiting_time += data[i].waiting_time;
    total_response_time += data[i].response_time;
    //printf("pid: %d, waiting: %d, response: %d\n", data[i].pid, data[i].waiting_time, data[i].response_time);
  }

  printf("Average waiting time: %.2f\n", (float)total_waiting_time / (float)size);
  printf("Average response time: %.2f\n", (float)total_response_time / (float)size);

  free(data);
  return 0;
}
